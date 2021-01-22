/*
    Copyright (C) 2020 by Pawel Soja <kernel32.pl@gmail.com>
    FPS Meter

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

#include <chrono>
#include <cstdint>

namespace INDI
{
class FPSMeter
{
public:
    FPSMeter(double timeWindow = 1000);

public:
    /**
     * @brief Reset all frame information
     */
    void reset();

    /**
     * @brief When you get a frame, call the function to count
     * @return returns true if the time window has been reached
     */
    bool newFrame();

    /**
     * @brief Time window setup
     * @param timeWindow Time window in milliseconds over which the frames are counted
     */
    void setTimeWindow(double timeWindow);

public:
    /**
     * @brief Number of frames per second counted in the time window
     */
    double framesPerSecond() const;
    /**
     * @brief Time in milliseconds between last frames
     */
    double deltaTime() const;

    /**
     * @brief Total frames
     * @return Number of frames counted
     */
    uint64_t totalFrames() const;

    /**
     * @brief Total time
     * @return Total time of counted frames
     */
    double totalTime() const;

private:
    uint64_t mFramesPerElapsedTime = 0;
    double mElapsedTime = 0;
    double mTimeWindow = 1000;

    std::chrono::steady_clock::time_point mFrameTime1;
    std::chrono::steady_clock::time_point mFrameTime2;
    
    double mFramesPerSecond = 0;

    double mTotalTime = 0;
    uint64_t mTotalFrames = 0;

};
}
