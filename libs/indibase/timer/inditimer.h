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
#include <memory>
#include <functional>

namespace INDI
{

class TimerPrivate;
/**
 * @class Timer 
 * @brief The Timer class provides repetitive and single-shot timers.
 * 
 * The Timer class provides a high-level programming interface for timers.
 * To use it, create a Timer, set your function with callOnTimeout, and call start().
 * From then on, it will call your function at constant intervals.
 * 
 * You can set a timer to time out only once by calling setSingleShot(true).
 * You can also use the static Timer::singleShot() function to call a function after a specified interval.
 */
class Timer
{
    DECLARE_PRIVATE(Timer)

public:
    Timer();
    virtual ~Timer();

public:
    /** @brief Starts or restarts the timer with the timeout specified in interval. */
    void start();

    /** @brief Starts or restarts the timer with a timeout interval of msec milliseconds. */
    void start(int msec);

    /** @brief Stops the timer. */
    void stop();

public:
    void callOnTimeout(const std::function<void()> &callback);

public:
    /** @brief Set the timeout interval in milliseconds. */
    void setInterval(int msec);

    /** @brief Set whether the timer is a single-shot timer. */
    void setSingleShot(bool singleShot);

public:
    /** @brief Returns true if the timer is running (pending); otherwise returns false. */
    bool isActive() const;

    /** @brief Returns whether the timer is a single-shot timer. */
    bool isSingleShot() const;

    /** @brief Returns the timer's remaining value in milliseconds left until the timeout.
     * If the timer not exists, the returned value will be -1.
     */
    int remainingTime() const;

    /** @brief Returns the timeout interval in milliseconds. */
    int interval() const;

public:
    /** @brief This static function calls a the given function after a given time interval. */
    static void singleShot(int msec, const std::function<void()> &callback);

public:
    /** @brief This function is called when the timer times out. */
    virtual void timeout();

protected:
    std::unique_ptr<TimerPrivate> d_ptr;
    Timer(TimerPrivate &dd);
};

}
