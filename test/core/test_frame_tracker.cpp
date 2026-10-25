/*
    Copyright (C) 2026 by Jasem Mutlaq <mutlaqja@ikarustech.com>

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include "stream/frametracker.h"

#include <gtest/gtest.h>

#include <cmath>
#include <random>
#include <vector>

using INDI::FrameTracker;

namespace
{

// Synthetic frame with one or more uniformly lit, anti-aliased disks on a dark sky
struct SyntheticFrame
{
    size_t width, height, components, bytesPerComponent;
    std::vector<double> samples;

    SyntheticFrame(size_t w, size_t h, size_t c = 1, size_t bytes = 1)
        : width(w), height(h), components(c), bytesPerComponent(bytes), samples(w * h * c, 0) {}

    double fullScale() const
    {
        return bytesPerComponent == 1 ? 255.0 : 65535.0;
    }

    // Adds a disk; brightness is a fraction of full scale. Edges are 4x4 supersampled.
    void addDisk(double cx, double cy, double radius, double brightness)
    {
        const int x0 = std::max(0, static_cast<int>(cx - radius - 2));
        const int x1 = std::min(static_cast<int>(width) - 1, static_cast<int>(cx + radius + 2));
        const int y0 = std::max(0, static_cast<int>(cy - radius - 2));
        const int y1 = std::min(static_cast<int>(height) - 1, static_cast<int>(cy + radius + 2));
        for (int y = y0; y <= y1; y++)
        {
            for (int x = x0; x <= x1; x++)
            {
                int inside = 0;
                for (int sy = 0; sy < 4; sy++)
                    for (int sx = 0; sx < 4; sx++)
                    {
                        const double dx = x + (sx + 0.5) / 4 - 0.5 - cx;
                        const double dy = y + (sy + 0.5) / 4 - 0.5 - cy;
                        inside += (dx * dx + dy * dy <= radius * radius);
                    }
                for (size_t c = 0; c < components; c++)
                    samples[(y * width + x) * components + c] += brightness * fullScale() * inside / 16.0;
            }
        }
    }

    void addNoise(double sky, double sigma, unsigned seed = 42)
    {
        std::mt19937 rng(seed);
        std::normal_distribution<double> noise(sky * fullScale(), sigma * fullScale());
        for (double &sample : samples)
            sample += noise(rng);
    }

    // Mosaic the frame in place: keep one RGGB channel per pixel, scaled as a typical CFA response
    std::vector<double> bayer() const
    {
        std::vector<double> mosaic(width * height);
        for (size_t y = 0; y < height; y++)
            for (size_t x = 0; x < width; x++)
            {
                const double response = (y % 2 == 0 && x % 2 == 0) ? 0.6 : (y % 2 == 1 && x % 2 == 1) ? 0.4 : 1.0;
                mosaic[y * width + x] = samples[(y * width + x) * components] * response;
            }
        return mosaic;
    }

    std::vector<uint8_t> encode(const std::vector<double> &values) const
    {
        std::vector<uint8_t> buffer(values.size() * bytesPerComponent);
        for (size_t i = 0; i < values.size(); i++)
        {
            const double value = std::min(fullScale(), std::max(0.0, std::round(values[i])));
            if (bytesPerComponent == 1)
                buffer[i] = static_cast<uint8_t>(value);
            else
                reinterpret_cast<uint16_t *>(buffer.data())[i] = static_cast<uint16_t>(value);
        }
        return buffer;
    }

    FrameTracker::Measurement measure(double referenceArea = 0) const
    {
        const std::vector<uint8_t> buffer = encode(samples);
        FrameTracker::Image image;
        image.data = buffer.data();
        image.width = width;
        image.height = height;
        image.components = components;
        image.bytesPerComponent = bytesPerComponent;
        return FrameTracker::measure(image, referenceArea);
    }
};

FrameTracker::Measurement validAt(double x, double y, double area = 1000)
{
    FrameTracker::Measurement measurement;
    measurement.valid = true;
    measurement.x = x;
    measurement.y = y;
    measurement.area = area;
    measurement.quality = 1;
    return measurement;
}

}

TEST(FrameTracker, MonoDiskSubPixelPosition)
{
    for (double cx : {200.0, 321.25, 400.6})
    {
        for (double cy : {150.0, 240.5, 301.8})
        {
            SyntheticFrame frame(640, 480);
            frame.addDisk(cx, cy, 60, 0.8);
            const auto measurement = frame.measure();
            ASSERT_TRUE(measurement.valid);
            EXPECT_NEAR(measurement.x, cx, 0.1) << "cy=" << cy;
            EXPECT_NEAR(measurement.y, cy, 0.1) << "cx=" << cx;
            EXPECT_NEAR(measurement.area, M_PI * 60 * 60, M_PI * 60 * 60 * 0.15);
            EXPECT_GT(measurement.quality, 0.95);
        }
    }
}

TEST(FrameTracker, NoisyRgb16Disk)
{
    SyntheticFrame frame(1280, 1024, 3, 2);
    frame.addDisk(700.3, 480.7, 120, 0.5);
    frame.addNoise(0.02, 0.005);
    const auto measurement = frame.measure();
    ASSERT_TRUE(measurement.valid);
    EXPECT_NEAR(measurement.x, 700.3, 0.5);
    EXPECT_NEAR(measurement.y, 480.7, 0.5);
    EXPECT_GT(measurement.quality, 0.8);
}

TEST(FrameTracker, IgnoresSmallSatellites)
{
    SyntheticFrame frame(640, 480);
    frame.addDisk(320, 240, 50, 0.8);
    // Bright moons on one side only would pull a plain centroid towards them
    frame.addDisk(420, 240, 3, 0.9);
    frame.addDisk(460, 238, 2, 0.9);
    const auto measurement = frame.measure();
    ASSERT_TRUE(measurement.valid);
    EXPECT_NEAR(measurement.x, 320, 0.2);
    EXPECT_NEAR(measurement.y, 240, 0.2);
}

TEST(FrameTracker, BayerMosaic)
{
    SyntheticFrame frame(1280, 1024);
    frame.addDisk(611.4, 500.2, 90, 0.7);
    const std::vector<uint8_t> buffer = frame.encode(frame.bayer());

    FrameTracker::Image image;
    image.data = buffer.data();
    image.width = frame.width;
    image.height = frame.height;
    image.bayer = true;
    const auto measurement = FrameTracker::measure(image);
    ASSERT_TRUE(measurement.valid);
    EXPECT_NEAR(measurement.x, 611.4, 0.5);
    EXPECT_NEAR(measurement.y, 500.2, 0.5);
}

TEST(FrameTracker, EmptyFrameIsInvalid)
{
    SyntheticFrame frame(640, 480);
    EXPECT_FALSE(frame.measure().valid);
}

TEST(FrameTracker, PureNoiseHasNoQuality)
{
    SyntheticFrame frame(640, 480, 1, 2);
    frame.addNoise(0.05, 0.01);
    const auto measurement = frame.measure();
    EXPECT_LT(measurement.quality, 0.1);
}

TEST(FrameTracker, FaintTargetHasLowQuality)
{
    SyntheticFrame frame(640, 480, 1, 2);
    frame.addDisk(320, 240, 50, 0.002);
    const auto measurement = frame.measure();
    EXPECT_LT(measurement.quality, 0.2);
}

TEST(FrameTracker, EdgeContactLowersQuality)
{
    SyntheticFrame frame(640, 480);
    frame.addDisk(20, 240, 60, 0.8);
    const auto measurement = frame.measure();
    ASSERT_TRUE(measurement.valid);
    EXPECT_LE(measurement.quality, 0.5);
}

TEST(FrameTracker, AreaChangeLowersQuality)
{
    SyntheticFrame frame(640, 480);
    frame.addDisk(320, 240, 50, 0.8);
    const double area = frame.measure().area;

    EXPECT_GT(frame.measure(area).quality, 0.95);
    // Same target, but the reference was recorded on something four times larger
    EXPECT_LT(frame.measure(area * 4).quality, 0.05);
}

TEST(FrameTracker, FirstValidMeasurementAcquiresLock)
{
    FrameTracker tracker;
    FrameTracker::Report report;

    EXPECT_FALSE(tracker.addMeasurement(FrameTracker::Measurement(), 0.0, 1.0, report));
    EXPECT_FALSE(tracker.hasLock());

    EXPECT_FALSE(tracker.addMeasurement(validAt(100, 50, 2000), 0.5, 1.0, report));
    ASSERT_TRUE(tracker.hasLock());
    EXPECT_DOUBLE_EQ(tracker.lockX(), 100);
    EXPECT_DOUBLE_EQ(tracker.lockY(), 50);
    EXPECT_DOUBLE_EQ(tracker.lockArea(), 2000);
}

TEST(FrameTracker, AveragesOverWindow)
{
    FrameTracker tracker;
    tracker.setLock(100, 100);
    FrameTracker::Report report;

    EXPECT_FALSE(tracker.addMeasurement(validAt(101, 99), 10.0, 1.0, report));
    EXPECT_FALSE(tracker.addMeasurement(validAt(103, 97), 10.4, 1.0, report));
    ASSERT_TRUE(tracker.addMeasurement(validAt(105, 95), 11.0, 1.0, report));

    EXPECT_TRUE(report.valid);
    EXPECT_EQ(report.samples, 3u);
    EXPECT_DOUBLE_EQ(report.windowStart, 10.0);
    EXPECT_DOUBLE_EQ(report.x, 103);
    EXPECT_DOUBLE_EQ(report.y, 97);
    EXPECT_DOUBLE_EQ(report.offsetX, 3);
    EXPECT_DOUBLE_EQ(report.offsetY, -3);
    EXPECT_DOUBLE_EQ(report.quality, 1);

    // The next window starts with the next sample
    EXPECT_FALSE(tracker.addMeasurement(validAt(100, 100), 11.2, 1.0, report));
    ASSERT_TRUE(tracker.addMeasurement(validAt(100, 100), 12.2, 1.0, report));
    EXPECT_DOUBLE_EQ(report.windowStart, 11.2);
    EXPECT_DOUBLE_EQ(report.offsetX, 0);
}

TEST(FrameTracker, InvalidSamplesScaleQuality)
{
    FrameTracker tracker;
    tracker.setLock(0, 0);
    FrameTracker::Report report;

    tracker.addMeasurement(validAt(2, 2), 0.0, 1.0, report);
    ASSERT_TRUE(tracker.addMeasurement(FrameTracker::Measurement(), 1.0, 1.0, report));
    EXPECT_TRUE(report.valid);
    EXPECT_DOUBLE_EQ(report.offsetX, 2);
    EXPECT_DOUBLE_EQ(report.quality, 0.5);
}

TEST(FrameTracker, LostTargetReportsInvalid)
{
    FrameTracker tracker;
    tracker.setLock(0, 0);
    FrameTracker::Report report;

    tracker.addMeasurement(FrameTracker::Measurement(), 0.0, 1.0, report);
    ASSERT_TRUE(tracker.addMeasurement(FrameTracker::Measurement(), 1.0, 1.0, report));
    EXPECT_FALSE(report.valid);
    EXPECT_DOUBLE_EQ(report.quality, 0);
    // The lock survives so tracking resumes when the target comes back
    EXPECT_TRUE(tracker.hasLock());
}

TEST(FrameTracker, SetLockRestartsWindow)
{
    FrameTracker tracker;
    tracker.setLock(0, 0);
    FrameTracker::Report report;

    tracker.addMeasurement(validAt(50, 50), 0.0, 1.0, report);
    tracker.setLock(10, 10);
    EXPECT_FALSE(tracker.addMeasurement(validAt(12, 12), 1.0, 1.0, report));
    ASSERT_TRUE(tracker.addMeasurement(validAt(12, 12), 2.0, 1.0, report));
    EXPECT_DOUBLE_EQ(report.offsetX, 2);
    EXPECT_DOUBLE_EQ(report.offsetY, 2);
}
