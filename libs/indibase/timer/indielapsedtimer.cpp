/*
    Copyright (C) 2021 by Pawel Soja <kernel32.pl@gmail.com>

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

#include "indielapsedtimer.h"
#include "indielapsedtimer_p.h"

namespace INDI
{

ElapsedTimer::ElapsedTimer()
    : d_ptr(new ElapsedTimerPrivate)
{ start(); }

ElapsedTimer::ElapsedTimer(ElapsedTimerPrivate &dd)
    : d_ptr(&dd)
{ start(); }

ElapsedTimer::~ElapsedTimer()
{ }

void ElapsedTimer::start()
{
    D_PTR(ElapsedTimer);
    d->start = std::chrono::steady_clock::now();
}

int64_t ElapsedTimer::restart()
{
    D_PTR(ElapsedTimer);
    std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
    int64_t result = std::chrono::duration_cast<std::chrono::milliseconds>(now - d->start).count();
    d->start = now;
    return result;
}

int64_t ElapsedTimer::elapsed() const
{
    D_PTR(const ElapsedTimer);
    std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(now - d->start).count();
}

int64_t ElapsedTimer::nsecsElapsed() const
{
    D_PTR(const ElapsedTimer);
    std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
    return std::chrono::duration_cast<std::chrono::nanoseconds>(now - d->start).count();
}

bool ElapsedTimer::hasExpired(int64_t timeout) const
{
    return elapsed() > timeout;
}

void ElapsedTimer::nsecsRewind(int64_t nsecs)
{
    D_PTR(ElapsedTimer);
    d->start += std::chrono::nanoseconds(nsecs);
}

}
