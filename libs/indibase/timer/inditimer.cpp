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

#include "inditimer.h"
#include "inditimer_p.h"

#include "eventloop.h"

namespace INDI
{

TimerPrivate::TimerPrivate(Timer *p)
    : p(p)
{ }

TimerPrivate::~TimerPrivate()
{ }

void TimerPrivate::start()
{
    timerId = addTimer(interval, [](void *arg){
        TimerPrivate *d = static_cast<TimerPrivate*>(arg);
        d->p->timeout();
        if (d->singleShot)
            d->timerId = -1;
        else
            d->start();
    }, this);
}

void TimerPrivate::stop()
{
    int id = timerId.exchange(-1);
    if (id != -1)
        rmTimer(id);
}

Timer::Timer()
    : d_ptr(new TimerPrivate(this))
{ }

Timer::~Timer()
{ }

void Timer::callOnTimeout(const std::function<void()> &callback)
{
    D_PTR(Timer);
    d->callback = callback;
}

void Timer::start()
{
    D_PTR(Timer);
    d->stop();
    d->start();
}

void Timer::start(int msec)
{
    D_PTR(Timer);
    d->stop();
    d->interval = msec;
    d->start();
}

void Timer::stop()
{
    D_PTR(Timer);
    d->stop();
}

void Timer::setInterval(int msec)
{
    D_PTR(Timer);
    d->interval = msec;
}

void Timer::setSingleShot(bool singleShot)
{
    D_PTR(Timer);
    d->singleShot = singleShot;
}

bool Timer::isActive() const
{
    D_PTR(const Timer);
    return d->timerId != -1;
}

bool Timer::isSingleShot() const
{
    D_PTR(const Timer);
    return d->singleShot;
}

int Timer::remainingTime() const
{
    //D_PTR(const Timer);
    return 0;
}

int Timer::interval() const
{
    D_PTR(const Timer);
    return d->interval;
}

void Timer::timeout()
{
    D_PTR(Timer);
    if (d->callback != nullptr)
        d->callback();
}

void Timer::singleShot(int msec, const std::function<void()> &callback)
{
    Timer *timer = new Timer();
    timer->setSingleShot(true);
    timer->setInterval(msec);
    timer->callOnTimeout([callback, timer](){
        callback();
        delete timer;
    });
}

}
