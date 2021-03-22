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

#include "indisinglethreadpool.h"
#include "indisinglethreadpool_p.h"

namespace INDI
{

SingleThreadPoolPrivate::SingleThreadPoolPrivate()
{
    thread = std::thread([this]{
        std::unique_lock<std::recursive_mutex> lock(runLock);
        for(;;)
        {
            acquire.wait(lock, [&](){ return functionToRun != nullptr || isThreadAboutToQuit; });
            if (isThreadAboutToQuit)
                break;

            std::function<void(const std::atomic_bool &isAboutToClose)> runningFunction = std::move(functionToRun);
            relased.notify_all();
            runningFunction(isFunctionAboutToQuit);
        }
    });
}

SingleThreadPoolPrivate::~SingleThreadPoolPrivate()
{
    {
        isFunctionAboutToQuit = true;
        isThreadAboutToQuit = true;
        std::unique_lock<std::recursive_mutex> lock(runLock);
        acquire.notify_one();
    }
    if (thread.joinable())
        thread.join();
}

SingleThreadPool::SingleThreadPool()
    : d_ptr(new SingleThreadPoolPrivate)
{ }

SingleThreadPool::~SingleThreadPool()
{ }

void SingleThreadPool::start(const std::function<void(const std::atomic_bool &isAboutToClose)> &functionToRun)
{
    D_PTR(SingleThreadPool);
    d->isFunctionAboutToQuit = true;
    std::unique_lock<std::recursive_mutex> lock(d->runLock);
    d->relased.wait(lock, [&d]{ return d->functionToRun == nullptr; });
    d->functionToRun = functionToRun;
    d->isFunctionAboutToQuit = false;
    d->acquire.notify_one();
}

bool SingleThreadPool::tryStart(const std::function<void(const std::atomic_bool &)> &functionToRun)
{
    D_PTR(SingleThreadPool);
    std::unique_lock<std::recursive_mutex> lock(d->runLock, std::defer_lock);

    if (!lock.try_lock())
        return false;

    d->functionToRun = functionToRun;
    d->isFunctionAboutToQuit = false;
    d->acquire.notify_one();
    return true;
}

void SingleThreadPool::quit()
{
    start(nullptr);
}

}
