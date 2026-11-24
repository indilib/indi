/*
    A fake serial device on a pseudo terminal for driver tests: the driver opens the slave side, the fake
    device reads what the driver sends, records every complete command and answers it.

    SPDX-License-Identifier: LGPL-2.1-or-later
*/

#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <fcntl.h>
#include <functional>
#include <mutex>
#if defined(__APPLE__) || defined(__OpenBSD__) || defined(__NetBSD__)
#include <util.h>
#elif defined(__FreeBSD__)
#include <libutil.h>
#else
#include <pty.h>
#endif
#include <string>
#include <sys/select.h>
#include <termios.h>
#include <thread>
#include <unistd.h>
#include <vector>

class FakeSerialDevice
{
    public:
        // Returns the length of the first complete command in the received bytes, or 0 if none is complete yet.
        // Bytes before the command are dropped by setting start.
        using Framer = std::function<size_t(const std::string &received, size_t &start)>;
        // Returns the reply to a complete command.
        using Responder = std::function<std::string(const std::string &command)>;

        // LX200 style commands: from the first ':' to the next '#'. Anything in front (a leading '#', a '\r'
        // left over from the previous command) is dropped.
        static size_t lx200Framer(const std::string &received, size_t &start)
        {
            start = received.find(':');
            if (start == std::string::npos)
            {
                start = received.size();
                return 0;
            }
            size_t end = received.find('#', start);
            return end == std::string::npos ? 0 : end - start + 1;
        }

        explicit FakeSerialDevice(Responder responder, Framer framer = lx200Framer)
            : m_Responder(std::move(responder)), m_Framer(std::move(framer))
        {
            if (openpty(&m_Master, &m_Slave, nullptr, nullptr, nullptr) != 0)
            {
                m_Master = m_Slave = -1;
                return;
            }
            makeRaw(m_Master);
            makeRaw(m_Slave);
            m_Thread = std::thread(&FakeSerialDevice::serve, this);
        }

        ~FakeSerialDevice()
        {
            m_Stop = true;
            if (m_Thread.joinable())
                m_Thread.join();
            if (m_Slave >= 0)
                close(m_Slave);
            if (m_Master >= 0)
                close(m_Master);
        }

        int driverFD() const
        {
            return m_Slave;
        }

        std::vector<std::string> commands()
        {
            std::lock_guard<std::mutex> lock(m_Mutex);
            return m_Commands;
        }

        // Wait until at least count commands were received; the driver may not wait for a reply.
        std::vector<std::string> waitForCommands(size_t count, std::chrono::milliseconds timeout = std::chrono::seconds(2))
        {
            std::unique_lock<std::mutex> lock(m_Mutex);
            m_Received.wait_for(lock, timeout, [&]()
            {
                return m_Commands.size() >= count;
            });
            return m_Commands;
        }

        void clear()
        {
            std::lock_guard<std::mutex> lock(m_Mutex);
            m_Commands.clear();
        }

    private:
        static void makeRaw(int fd)
        {
            struct termios tio;
            tcgetattr(fd, &tio);
            cfmakeraw(&tio);
            tcsetattr(fd, TCSANOW, &tio);
        }

        void serve()
        {
            std::string received;
            while (!m_Stop)
            {
                fd_set readSet;
                FD_ZERO(&readSet);
                FD_SET(m_Master, &readSet);
                struct timeval timeout = {0, 20000};
                if (select(m_Master + 1, &readSet, nullptr, nullptr, &timeout) <= 0)
                    continue;

                char chunk[256];
                ssize_t n = read(m_Master, chunk, sizeof(chunk));
                if (n <= 0)
                    continue;
                received.append(chunk, n);

                while (true)
                {
                    size_t start = 0;
                    size_t length = m_Framer(received, start);
                    if (length == 0)
                    {
                        received.erase(0, start);
                        break;
                    }
                    std::string command = received.substr(start, length);
                    received.erase(0, start + length);
                    {
                        std::lock_guard<std::mutex> lock(m_Mutex);
                        m_Commands.push_back(command);
                    }
                    m_Received.notify_all();
                    std::string reply = m_Responder(command);
                    if (!reply.empty() && write(m_Master, reply.data(), reply.size()) < 0)
                        break;
                }
            }
        }

        Responder m_Responder;
        Framer m_Framer;
        int m_Master {-1};
        int m_Slave {-1};
        std::atomic<bool> m_Stop {false};
        std::thread m_Thread;
        std::mutex m_Mutex;
        std::condition_variable m_Received;
        std::vector<std::string> m_Commands;
};
