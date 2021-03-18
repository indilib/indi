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

#pragma once

#include "indimacros.h"
#include <cstdint>
#include <memory>

namespace INDI
{

class ElapsedTimerPrivate;
/**
 * @class ElapsedTimer
 * @brief The ElapsedTimer class provides a fast way to calculate elapsed times.
 *
 * The ElapsedTimer class is usually used to quickly calculate how much time has elapsed between two events.
 */
class ElapsedTimer
{
    DECLARE_PRIVATE(ElapsedTimer)
public:
    ElapsedTimer();
    virtual ~ElapsedTimer();

public:
    /** @brief Starts this timer. Once started, a timer value can be checked with elapsed(). */
    void start();

    /** @brief Restarts the timer and returns the number of milliseconds elapsed since the previous start. */
    int64_t restart();

public:
    /** @brief Returns the number of milliseconds since this ElapsedTimer was last started. */
    int64_t elapsed() const;

    /** @brief Returns the number of nanoseconds since this ElapsedTimer was last started. */
    int64_t nsecsElapsed() const;

    /** @brief Returns true if this ElapsedTimer has already expired by timeout milliseconds. */
    bool hasExpired(int64_t timeout) const;

public:
    /** @brief Rewind elapsed time of nsec nanoseconds */
    void nsecsRewind(int64_t nsecs);

protected:
    std::unique_ptr<ElapsedTimerPrivate> d_ptr;
    ElapsedTimer(ElapsedTimerPrivate &dd);
};

}
