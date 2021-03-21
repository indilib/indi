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
#include "fpsmeter.h"

namespace INDI
{

FPSMeter::FPSMeter(double timeWindow)
    : mTimeWindow(timeWindow)
{
    reset();
}

bool FPSMeter::newFrame()
{
    mFrameTime2 = mFrameTime1;
    mFrameTime1 = std::chrono::steady_clock::now();
    
    ++mTotalFrames;
    ++mFramesPerElapsedTime;

    double dt = deltaTime();

    mElapsedTime += dt;
    mTotalTime   += dt;

    if (mElapsedTime >= mTimeWindow)
    {            
        mFramesPerSecond = mFramesPerElapsedTime / mElapsedTime * 1000;
        mElapsedTime = 0;
        mFramesPerElapsedTime = 0;
        return true;
    }

    return false;
}

void FPSMeter::setTimeWindow(double timeWindow)
{
    mTimeWindow = timeWindow;
}

double FPSMeter::framesPerSecond() const
{
    return mFramesPerSecond;
}

double FPSMeter::deltaTime() const
{
    return std::chrono::duration<double>(mFrameTime1 - mFrameTime2).count() * 1000;
}

uint64_t FPSMeter::totalFrames() const
{
    return mTotalFrames;
}

double FPSMeter::totalTime() const
{
    return mTotalTime;
}

void FPSMeter::reset()
{
    mFramesPerElapsedTime = 0;
    mElapsedTime = 0;
    mFrameTime1 = std::chrono::steady_clock::now();
    mFrameTime2 = mFrameTime1;
    mFramesPerSecond = 0;
    mTotalFrames = 0;
    mTotalTime = 0;
}

}
