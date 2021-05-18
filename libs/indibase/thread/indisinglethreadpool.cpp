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
        std::unique_lock<std::mutex> lock(runLock);
        for(;;)
        {
            acquire.wait(lock, [&](){ return pendingFunction != nullptr || isThreadAboutToQuit; });
            if (isThreadAboutToQuit)
                break;

            isFunctionAboutToQuit = false;
            std::swap(runningFunction, pendingFunction);
            relased.notify_all();

            lock.unlock();
            runningFunction(isFunctionAboutToQuit);
            lock.lock();

            runningFunction = nullptr;
        }
    });
}

SingleThreadPoolPrivate::~SingleThreadPoolPrivate()
{
    {
        isFunctionAboutToQuit = true;
        isThreadAboutToQuit = true;
        std::unique_lock<std::mutex> lock(runLock);
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
    std::unique_lock<std::mutex> lock(d->runLock);
    d->pendingFunction = functionToRun;
    d->isFunctionAboutToQuit = true;
    d->acquire.notify_one();

    // wait for run
    if (std::this_thread::get_id() != d->thread.get_id())
        d->relased.wait(lock, [&d]{ return d->pendingFunction == nullptr; });
}

bool SingleThreadPool::tryStart(const std::function<void(const std::atomic_bool &)> &functionToRun)
{
    D_PTR(SingleThreadPool);
    std::unique_lock<std::mutex> lock(d->runLock);
    if (d->runningFunction != nullptr)
        return false;

    d->isFunctionAboutToQuit = true;
    d->pendingFunction = functionToRun;
    d->acquire.notify_one();

    // wait for run
    if (std::this_thread::get_id() != d->thread.get_id())
        d->relased.wait(lock, [&d]{ return d->pendingFunction == nullptr; });

    return true;
}

void SingleThreadPool::quit()
{
    start(nullptr);
}

}
