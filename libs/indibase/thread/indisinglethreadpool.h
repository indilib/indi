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
#include <atomic>

namespace INDI
{

class SingleThreadPoolPrivate;
class SingleThreadPool
{
    DECLARE_PRIVATE(SingleThreadPool)
public:
    SingleThreadPool();
    ~SingleThreadPool();

public:
    /** @brief Reserves a thread and uses it to run functionToRun.
     *  A running function can check the 'isAboutToClose' flag and decide whether to end the work and give the thread. */
    void start(const std::function<void(const std::atomic_bool &isAboutToClose)> &functionToRun);

    /** @brief If thread isn't available at the time of calling, then this function does nothing and returns false.
     *  Otherwise, functionToRun is run immediately using thread and this function returns true. */
    bool tryStart(const std::function<void(const std::atomic_bool &isAboutToClose)> &functionToRun);

public:
    /** @brief Sets the 'isAboutToClose' flag to 'true' and waits for the end of running function. */
    void quit();

protected:
    std::shared_ptr<SingleThreadPoolPrivate> d_ptr;
};

}
