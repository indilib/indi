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

#pragma once

#include <cstddef>
#include <cstdint>

namespace INDI
{

/**
 * @brief The FrameTracker class measures where a bright extended target (planet, Moon or Sun disk)
 * sits in a video frame, and reports its averaged offset from a lock point.
 *
 * The measurement bins the frame down to at most 256 pixels on its long side, separates the target
 * from the sky with Otsu's threshold, keeps the largest connected blob and returns its
 * brightness-weighted centroid. Each measurement carries a quality value between 0 and 1.
 *
 * Measurements are averaged over a time window to filter out seeing. The first valid measurement after
 * reset() becomes the lock point unless one is set explicitly with setLock().
 *
 * The class does no I/O and holds no locks; StreamManager feeds it from its frame thread.
 */
class FrameTracker
{
    public:
        struct Image
        {
            const uint8_t *data {nullptr};
            size_t width {0};
            size_t height {0};
            /// 1 for mono or Bayer, 3 for interleaved RGB
            size_t components {1};
            /// 1 for 8-bit, 2 for 16-bit (native endian) samples
            size_t bytesPerComponent {1};
            /// Raw Bayer mosaic: binning is kept even so each bin averages whole CFA cells
            bool bayer {false};
        };

        struct Measurement
        {
            bool valid {false};
            /// Centroid in frame pixels
            double x {0};
            double y {0};
            /// Blob area in frame pixels
            double area {0};
            /// 0 (unusable) to 1 (clean, well separated disk)
            double quality {0};
        };

        struct Report
        {
            /// False if no sample in the window was valid (target lost)
            bool valid {false};
            /// Averaged centroid in frame pixels
            double x {0};
            double y {0};
            /// Averaged centroid minus lock point
            double offsetX {0};
            double offsetY {0};
            /// Mean quality of the valid samples, scaled by the fraction of samples that were valid
            double quality {0};
            /// Time of the first sample in the window
            double windowStart {0};
            /// Number of samples in the window
            uint32_t samples {0};
        };

        /**
         * @brief measure Find the target in a single frame.
         * @param image frame to measure.
         * @param referenceArea blob area at lock time, used to lower the quality when the blob size
         * changes a lot (clouds, another object). Pass 0 to skip this check.
         */
        static Measurement measure(const Image &image, double referenceArea = 0);

        /// Drop the lock point and the current averaging window.
        void reset();

        /// Lock to an explicit position. Restarts the averaging window.
        void setLock(double x, double y);

        bool hasLock() const
        {
            return m_HasLock;
        }
        double lockX() const
        {
            return m_LockX;
        }
        double lockY() const
        {
            return m_LockY;
        }
        double lockArea() const
        {
            return m_LockArea;
        }

        /**
         * @brief addMeasurement Add a measurement to the averaging window.
         * @param measurement result of measure(), in the same coordinates as the lock point.
         * @param time time of the frame, in seconds.
         * @param window length of the averaging window, in seconds.
         * @param report filled when the window completes.
         * @return true when a window completed and report was filled.
         * @note Until a lock point exists, valid measurements acquire one and invalid ones are dropped.
         */
        bool addMeasurement(const Measurement &measurement, double time, double window, Report &report);

    private:
        void resetWindow();

        bool m_HasLock {false};
        double m_LockX {0};
        double m_LockY {0};
        double m_LockArea {0};

        bool m_WindowOpen {false};
        double m_WindowStart {0};
        uint32_t m_Samples {0};
        uint32_t m_ValidSamples {0};
        double m_SumX {0};
        double m_SumY {0};
        double m_SumQuality {0};
};

}
