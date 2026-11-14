/*
    OnStepX INDI Driver — MockFirmware.h
    Copyright (C) 2026 Ed Lee

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

/*
    OnStepX INDI Driver — shared test helper: firmware-faithful mock

    Answers like OnStepX ProcessCmds.cpp: configured commands get their
    configured raw reply (include '#' for text replies; omit it for bare
    '1'/'0'; use "" for commands that reply nothing), and any other command
    gets a bare '0' — the firmware's reply to an unknown or failed command.
    Commands that carry a value can be scripted by prefix.  An exact
    setResponse() match wins, then the longest setPrefixResponse() match.
    setResponseSequence() scripts successive replies to one command (the
    last one repeats), and takes precedence over both.
    Every command is recorded so tests can assert exactly what was sent.

    Keys are the command WITHOUT the trailing '#', e.g. ":F2S1500".
*/

#pragma once

#include <gtest/gtest.h>

#include <chrono>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <sys/socket.h>
#include <unistd.h>

class MockFirmware
{
    public:
        ~MockFirmware()
        {
            if (m_thread.joinable())
                stop();
        }

        void setResponse(const std::string &key, const std::string &resp)
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_responses[key] = resp;
        }

        // Successive replies to key: the first call gets replies[0], and so
        // on; the last reply repeats once the others are used up.
        void setResponseSequence(const std::string &key, const std::vector<std::string> &replies)
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_sequences[key] = replies;
        }

        // Reply for any command starting with prefix, e.g. (":St", "1").
        void setPrefixResponse(const std::string &prefix, const std::string &resp)
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_prefixResponses[prefix] = resp;
        }

        // Returns the driver-end fd for OnStepXComm::setFd().
        int start()
        {
            EXPECT_EQ(socketpair(AF_UNIX, SOCK_STREAM, 0, m_fds), 0);
            m_thread = std::thread([this]()
            {
                serve();
            });
            return m_fds[0];
        }

        void stop()
        {
            if (!m_thread.joinable())
                return;
            shutdown(m_fds[0], SHUT_RDWR);
            close(m_fds[0]);
            if (m_thread.joinable())
                m_thread.join();
            close(m_fds[1]);
        }

        std::vector<std::string> commands()
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            return m_commands;
        }

        bool received(const std::string &cmd)
        {
            for (const auto &c : commands())
                if (c == cmd)
                    return true;
            return false;
        }

        // Wait until cmd has been received (for commands with no reply,
        // where the driver returns before the mock has read them).
        bool waitFor(const std::string &cmd, int timeoutMs = 1000)
        {
            for (int waited = 0; waited < timeoutMs; waited += 5)
            {
                if (received(cmd))
                    return true;
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
            }
            return received(cmd);
        }

    private:
        std::string readCmd()
        {
            std::string cmd;
            char c;
            ssize_t n;
            while ((n = read(m_fds[1], &c, 1)) == 1 && c != '#')
                cmd += c;
            if (n <= 0)
                cmd.clear();
            return cmd;
        }

        // Caller holds m_mutex.
        std::string lookup(const std::string &cmd)
        {
            auto seq = m_sequences.find(cmd);
            if (seq != m_sequences.end() && !seq->second.empty())
            {
                const std::string resp = seq->second.front();
                if (seq->second.size() > 1)
                    seq->second.erase(seq->second.begin());
                return resp;
            }

            auto it = m_responses.find(cmd);
            if (it != m_responses.end())
                return it->second;

            const std::string *best = nullptr;
            std::size_t bestLen = 0;
            for (const auto &rule : m_prefixResponses)
                if (cmd.rfind(rule.first, 0) == 0 && rule.first.size() >= bestLen)
                {
                    best = &rule.second;
                    bestLen = rule.first.size();
                }
            return best ? *best : "0";
        }

        void serve()
        {
            while (true)
            {
                const std::string cmd = readCmd();
                if (cmd.empty())
                    break;
                std::string resp;
                {
                    std::lock_guard<std::mutex> lock(m_mutex);
                    m_commands.push_back(cmd);
                    resp = lookup(cmd);
                }
                if (!resp.empty() && write(m_fds[1], resp.c_str(), resp.size()) < 0)
                    break;
            }
        }

        int                                m_fds[2] { -1, -1 };
        std::thread                        m_thread;
        std::map<std::string, std::string> m_responses;
        std::map<std::string, std::string> m_prefixResponses;
        std::map<std::string, std::vector<std::string>> m_sequences;
        std::mutex                         m_mutex;
        std::vector<std::string>           m_commands;
};
