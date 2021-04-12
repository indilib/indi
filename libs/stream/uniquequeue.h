/*
    Copyright (C) 2020 by Pawel Soja <kernel32.pl@gmail.com>

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

#include <queue>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <cstdint>

/**
 * \class UniqueQueue template
 * \brief The UniqueQueue class is a thread-safe FIFO container adapter.
 * 
 * Data is move to the queue, which ensures high efficiency when collecting data, e.g. for processing.
 * This class ensures that threads are wake up waiting for data.
 * It also provides a convenient "abort" method to wake up those waiting if there is no more data to process.
 * Don't use it for large class/arrays (sizeof T). Provide an interface that allows data to be swapped/moved as pointers,
 * like std::vector or simple pointers.
 */
template <typename T>
class UniqueQueue
{
public:
    /**
     * @brief Move data to queue
     * @param data the data will be moved using std::move
     */
    void push(T && data);
    
    /**
     * @brief Pop data from queue
     * @param dest the data will be swapped and destroyed
     * @return returns false if the abort function was called while waiting for data
     */
    bool pop(T & dest);

    /**
     * @brief Pop data from queue
     * @param dest the data will be swapped and destroyed
     * @param msecs timeout in milliseconds
     * @return returns false if timeout or the abort function was called while waiting for data
     */
    bool pop(T & dest, uint32_t msecs);
    
    /**
     * @brief Wait for an empty queue
     */
    void waitForEmpty() const;

    /**
     * @brief Wait for an empty queue
     * @param msecs timeout in milliseconds
     * @return returns false if timeout
     */
    bool waitForEmpty(uint32_t msecs) const;
    
    /**
     * @brief Clear queue
     */
    void clear();

    /**
     * @brief Clear queue and exit pop methods with false return
     */
    void abort();

    /**
     * @brief Return the number of items in the queue
     * @return count of elements
     */
    size_t size() const;

protected:
    std::queue<T>      queue;
    mutable std::mutex mutex;

    mutable std::condition_variable decrease;
    mutable std::condition_variable increase;  
};

// implementation
template <typename T>
inline void UniqueQueue<T>::push(T && data)
{
    std::lock_guard<std::mutex> lock(mutex);
    queue.push(std::move(data));
    increase.notify_all();
}

template <typename T>
inline bool UniqueQueue<T>::pop(T & dest)
{
    std::unique_lock<std::mutex> lock(mutex);
    if (queue.empty())
        increase.wait(lock);

    if (queue.empty())
        return false; // abort

    std::swap(dest, queue.front());
    queue.pop();
    decrease.notify_all();
    return true;
}

template <typename T>
inline bool UniqueQueue<T>::pop(T & dest, uint32_t msecs)
{
    std::unique_lock<std::mutex> lock(mutex);

    if (queue.empty() && increase.wait_for(lock, std::chrono::milliseconds(msecs)) == std::cv_status::timeout)
        return false; // timeout

    if (queue.empty())
        return false; // abort

    std::swap(dest, queue.front());
    queue.pop();
    decrease.notify_all();
    return true;
}

template <typename T>
inline size_t UniqueQueue<T>::size() const
{
    std::unique_lock<std::mutex> lock(mutex);
    return queue.size();
}

template <typename T>
inline void UniqueQueue<T>::clear()
{
    std::queue<T> empty;
    std::unique_lock<std::mutex> lock(mutex);
    std::swap(queue, empty);
    decrease.notify_all();
}

template <typename T>
inline void UniqueQueue<T>::waitForEmpty() const
{
    std::unique_lock<std::mutex> lock(mutex);
    decrease.wait(lock, [this](){ return queue.empty(); });
}

template <typename T>
inline bool UniqueQueue<T>::waitForEmpty(uint32_t msecs) const
{
    std::unique_lock<std::mutex> lock(mutex);
    return decrease.wait_for(lock, std::chrono::milliseconds(msecs), [this](){ return queue.empty(); });
}

template <typename T>
inline void UniqueQueue<T>::abort()
{
    std::queue<T> empty;
    std::unique_lock<std::mutex> lock(mutex);
    std::swap(queue, empty);
    increase.notify_all();
    decrease.notify_all();
}
