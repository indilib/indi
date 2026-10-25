/*
    Copyright (C) 2026 by Jasem Mutlaq <mutlaqja@ikarustech.com>

    Frame Tracker

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

#include "frametracker.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace INDI
{

namespace
{
// Long side of the binned working image
constexpr size_t MAX_WORKING_SIZE = 256;
// Smallest blob, in binned pixels, accepted as a target
constexpr size_t MIN_BLOB_PIXELS = 4;
// Target to sky contrast, in sky noise sigmas, below which quality drops to 0, and above which it is 1
constexpr double MIN_SNR = 3;
constexpr double GOOD_SNR = 10;
// Target to sky brightness, as a fraction of full scale, that gives full quality
constexpr double GOOD_SIGNAL = 0.02;
// Quality factor when the blob touches the frame edge (centroid is biased by clipping)
constexpr double EDGE_FACTOR = 0.5;

double clamp01(double value)
{
    return std::min(1.0, std::max(0.0, value));
}

// Otsu's method over a 256-bin histogram of the working image
double otsuThreshold(const std::vector<double> &pixels, double minValue, double maxValue)
{
    std::array<uint32_t, 256> histogram {};
    const double range = maxValue - minValue;
    for (double value : pixels)
    {
        int bin = static_cast<int>((value - minValue) / range * 256.0);
        histogram[std::min(255, std::max(0, bin))]++;
    }

    const double total = static_cast<double>(pixels.size());
    double sumAll = 0;
    for (size_t i = 0; i < histogram.size(); i++)
        sumAll += i * static_cast<double>(histogram[i]);

    double sumBackground = 0, weightBackground = 0, bestVariance = -1;
    size_t bestBin = 0;
    for (size_t i = 0; i < histogram.size(); i++)
    {
        weightBackground += histogram[i];
        if (weightBackground == 0)
            continue;
        const double weightForeground = total - weightBackground;
        if (weightForeground == 0)
            break;

        sumBackground += i * static_cast<double>(histogram[i]);
        const double meanBackground = sumBackground / weightBackground;
        const double meanForeground = (sumAll - sumBackground) / weightForeground;
        const double variance = weightBackground * weightForeground * (meanBackground - meanForeground) * (meanBackground - meanForeground);
        if (variance > bestVariance)
        {
            bestVariance = variance;
            bestBin = i;
        }
    }

    // Pixels strictly above the upper edge of the best bin are foreground
    return minValue + (bestBin + 1) * range / 256.0;
}
}

FrameTracker::Measurement FrameTracker::measure(const Image &image, double referenceArea)
{
    Measurement result;

    if (image.data == nullptr || image.width == 0 || image.height == 0 || image.components == 0
            || (image.bytesPerComponent != 1 && image.bytesPerComponent != 2))
        return result;

    // Bin the frame into the working image, summing all color components
    size_t bin = (std::max(image.width, image.height) + MAX_WORKING_SIZE - 1) / MAX_WORKING_SIZE;
    if (image.bayer && bin % 2)
        bin++;
    bin = std::max<size_t>(bin, 1);

    const size_t workWidth  = image.width / bin;
    const size_t workHeight = image.height / bin;
    if (workWidth == 0 || workHeight == 0)
        return result;

    const size_t rowSamples = image.width * image.components;
    const size_t binSamples = bin * bin * image.components;
    std::vector<double> pixels(workWidth * workHeight);

    for (size_t wy = 0; wy < workHeight; wy++)
    {
        for (size_t wx = 0; wx < workWidth; wx++)
        {
            uint64_t sum = 0;
            for (size_t y = wy * bin; y < (wy + 1) * bin; y++)
            {
                const size_t start = y * rowSamples + wx * bin * image.components;
                const size_t end   = start + bin * image.components;
                if (image.bytesPerComponent == 1)
                {
                    for (size_t i = start; i < end; i++)
                        sum += image.data[i];
                }
                else
                {
                    const uint16_t *data16 = reinterpret_cast<const uint16_t *>(image.data);
                    for (size_t i = start; i < end; i++)
                        sum += data16[i];
                }
            }
            // Mean sample value, so thresholds are independent of the bin size
            pixels[wy * workWidth + wx] = static_cast<double>(sum) / binSamples;
        }
    }

    const auto minMax = std::minmax_element(pixels.begin(), pixels.end());
    const double minValue = *minMax.first, maxValue = *minMax.second;
    if (maxValue - minValue <= 0)
        return result;

    const double threshold = otsuThreshold(pixels, minValue, maxValue);

    // Keep the largest connected blob above the threshold, so small satellites (Jupiter's moons)
    // and hot pixels do not pull the centroid.
    std::vector<int32_t> labels(pixels.size(), 0);
    std::vector<size_t> stack;
    int32_t bestLabel = 0, nextLabel = 0;
    size_t bestCount = 0;

    for (size_t start = 0; start < pixels.size(); start++)
    {
        if (labels[start] != 0 || pixels[start] <= threshold)
            continue;

        const int32_t label = ++nextLabel;
        size_t count = 0;
        labels[start] = label;
        stack.push_back(start);
        while (!stack.empty())
        {
            const size_t index = stack.back();
            stack.pop_back();
            count++;

            const size_t x = index % workWidth, y = index / workWidth;
            const size_t neighbors[4] =
            {
                x > 0 ? index - 1 : index,
                x + 1 < workWidth ? index + 1 : index,
                y > 0 ? index - workWidth : index,
                y + 1 < workHeight ? index + workWidth : index
            };
            for (size_t neighbor : neighbors)
            {
                if (labels[neighbor] == 0 && pixels[neighbor] > threshold)
                {
                    labels[neighbor] = label;
                    stack.push_back(neighbor);
                }
            }
        }

        if (count > bestCount)
        {
            bestCount = count;
            bestLabel = label;
        }
    }

    if (bestCount < MIN_BLOB_PIXELS)
        return result;

    // Centroid weighted by brightness above threshold, plus statistics for the quality value
    double sumWeight = 0, sumX = 0, sumY = 0, sumBlob = 0;
    double sumSky = 0, sumSky2 = 0;
    size_t skyCount = 0;
    bool touchesEdge = false;

    for (size_t y = 0; y < workHeight; y++)
    {
        for (size_t x = 0; x < workWidth; x++)
        {
            const size_t index = y * workWidth + x;
            const double value = pixels[index];
            if (labels[index] == bestLabel)
            {
                const double weight = value - threshold;
                sumWeight += weight;
                sumX += weight * x;
                sumY += weight * y;
                sumBlob += value;
                if (x == 0 || y == 0 || x + 1 == workWidth || y + 1 == workHeight)
                    touchesEdge = true;
            }
            else if (labels[index] == 0)
            {
                sumSky += value;
                sumSky2 += value * value;
                skyCount++;
            }
        }
    }

    if (sumWeight <= 0)
        return result;

    // Each working pixel covers bin x bin frame pixels; map its center back to frame coordinates
    result.x = sumX / sumWeight * bin + (bin - 1) / 2.0;
    result.y = sumY / sumWeight * bin + (bin - 1) / 2.0;
    result.area = static_cast<double>(bestCount * bin * bin);

    // Quality: target must stand out from the sky noise and be bright enough in absolute terms
    const double blobMean = sumBlob / bestCount;
    const double skyMean = skyCount > 0 ? sumSky / skyCount : minValue;
    const double skyStd = skyCount > 1 ? std::sqrt(std::max(0.0, sumSky2 / skyCount - skyMean * skyMean)) : 0;
    const double signal = blobMean - skyMean;
    const double fullScale = image.bytesPerComponent == 1 ? 255.0 : 65535.0;

    double quality = skyStd > 0 ? clamp01((signal / skyStd - MIN_SNR) / (GOOD_SNR - MIN_SNR)) : 1.0;
    quality *= clamp01(signal / (GOOD_SIGNAL * fullScale));
    if (touchesEdge)
        quality *= EDGE_FACTOR;
    if (referenceArea > 0)
        quality *= clamp01(1.0 - std::fabs(std::log2(result.area / referenceArea)));

    result.quality = quality;
    result.valid = quality > 0;
    return result;
}

void FrameTracker::reset()
{
    m_HasLock = false;
    m_LockX = m_LockY = m_LockArea = 0;
    resetWindow();
}

void FrameTracker::setLock(double x, double y)
{
    m_HasLock = true;
    m_LockX = x;
    m_LockY = y;
    resetWindow();
}

void FrameTracker::resetWindow()
{
    m_WindowOpen = false;
    m_WindowStart = 0;
    m_Samples = m_ValidSamples = 0;
    m_SumX = m_SumY = m_SumQuality = 0;
}

bool FrameTracker::addMeasurement(const Measurement &measurement, double time, double window, Report &report)
{
    if (!m_HasLock)
    {
        if (!measurement.valid)
            return false;

        m_HasLock = true;
        m_LockX = measurement.x;
        m_LockY = measurement.y;
        m_LockArea = measurement.area;
        resetWindow();
    }
    // An explicit lock has no area yet; take it from the first valid sample
    else if (m_LockArea <= 0 && measurement.valid)
        m_LockArea = measurement.area;

    if (!m_WindowOpen)
    {
        m_WindowOpen = true;
        m_WindowStart = time;
    }

    m_Samples++;
    if (measurement.valid)
    {
        m_ValidSamples++;
        m_SumX += measurement.x;
        m_SumY += measurement.y;
        m_SumQuality += measurement.quality;
    }

    if (time - m_WindowStart < window)
        return false;

    report = Report();
    report.windowStart = m_WindowStart;
    report.samples = m_Samples;
    report.valid = m_ValidSamples > 0;
    if (report.valid)
    {
        report.x = m_SumX / m_ValidSamples;
        report.y = m_SumY / m_ValidSamples;
        report.offsetX = report.x - m_LockX;
        report.offsetY = report.y - m_LockY;
        // Mean quality of valid samples times the valid fraction
        report.quality = m_SumQuality / m_Samples;
    }

    resetWindow();
    return true;
}

}
