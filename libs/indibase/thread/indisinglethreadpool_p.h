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

#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <functional>

namespace INDI
{

class SingleThreadPoolPrivate
{
public:
    SingleThreadPoolPrivate();
    virtual ~SingleThreadPoolPrivate();

    std::function<void(const std::atomic_bool &isAboutToClose)> pendingFunction;
    std::function<void(const std::atomic_bool &isAboutToClose)> runningFunction;
    std::atomic_bool isThreadAboutToQuit {false};
    std::atomic_bool isFunctionAboutToQuit {true};

    std::condition_variable_any acquire;
    std::condition_variable_any relased;
    std::mutex runLock;
    std::thread thread;
};

}
