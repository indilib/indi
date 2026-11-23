/*
    Regression tests for the LX200 connection check (check_lx200_connection, used by the handshake of the generic
    LX200 drivers, Autostar, Classic, SS2000PC, OnStep, ...).

    The check sends ACK (0x06) and accepted any single byte as the mount's answer. A stale byte, a device that sends
    data on its own (e.g. a Wanderer ETA M54, which streams a status line every 2 s) or any device answering with one
    byte therefore passed as a telescope when an auto search probed its port. A mount answers ACK with its alignment
    mode (Meade: A, D, L, P; OnStepX: P or A; G for German equatorial; some firmwares add '#'), so the check now
    accepts only these.

    The check talks to fake devices on pseudo terminals.

    SPDX-License-Identifier: LGPL-2.1-or-later
*/

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <functional>
#include <mutex>
#include <pty.h>
#include <string>
#include <sys/select.h>
#include <termios.h>
#include <thread>
#include <unistd.h>
#include <vector>

#include "indicom.h"
#include "indilogger.h"
#include "drivers/telescope/lx200driver.h"

namespace
{

// A device on a pseudo terminal. It either streams a line periodically or answers what it receives.
class FakeSerialDevice
{
    public:
        using Responder = std::function<std::string(std::string &received)>;

        FakeSerialDevice(Responder responder, std::string streamLine = "") : m_Responder(std::move(responder)),
            m_StreamLine(std::move(streamLine))
        {
            if (openpty(&m_Master, &m_Slave, nullptr, nullptr, nullptr) != 0)
                return;
            makeRaw(m_Master);
            makeRaw(m_Slave);
            // Keep only the name: the driver sets exclusive mode on the port, which an open slave
            // descriptor here would keep alive across the driver's disconnect and reconnect.
            const char *name = ttyname(m_Slave);
            m_Port = name ? name : "";
            close(m_Slave);
            m_Slave = -1;
            m_Thread = std::thread(&FakeSerialDevice::serve, this);
        }

        ~FakeSerialDevice()
        {
            m_Stop = true;
            if (m_Thread.joinable())
                m_Thread.join();
            if (m_Master >= 0)
                close(m_Master);
        }

        std::string received()
        {
            std::lock_guard<std::mutex> lock(m_Mutex);
            return m_All;
        }

        void clearReceived()
        {
            std::lock_guard<std::mutex> lock(m_Mutex);
            m_All.clear();
        }

        // Sends data now, without being asked.
        void send(const std::string &data)
        {
            if (write(m_Master, data.data(), data.size()) < 0)
                perror("fake device write");
        }

        std::string port() const
        {
            return m_Port;
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
            auto lastStream = std::chrono::steady_clock::now();
            while (!m_Stop)
            {
                if (!m_StreamLine.empty() && std::chrono::steady_clock::now() - lastStream > std::chrono::milliseconds(100))
                {
                    if (write(m_Master, m_StreamLine.data(), m_StreamLine.size()) < 0)
                        break;
                    lastStream = std::chrono::steady_clock::now();
                }

                fd_set readSet;
                FD_ZERO(&readSet);
                FD_SET(m_Master, &readSet);
                struct timeval timeout = {0, 20000};
                if (select(m_Master + 1, &readSet, nullptr, nullptr, &timeout) <= 0)
                    continue;

                char chunk[256];
                ssize_t n = read(m_Master, chunk, sizeof(chunk));
                if (n <= 0)
                {
                    // No driver has the port open (EIO on the master side).
                    std::this_thread::sleep_for(std::chrono::milliseconds(20));
                    continue;
                }
                received.append(chunk, n);
                {
                    std::lock_guard<std::mutex> lock(m_Mutex);
                    m_All.append(chunk, n);
                }
                if (m_Responder)
                {
                    std::string reply = m_Responder(received);
                    if (!reply.empty() && write(m_Master, reply.data(), reply.size()) < 0)
                        break;
                }
            }
        }

        Responder m_Responder;
        std::string m_StreamLine;
        int m_Master {-1};
        int m_Slave {-1};
        std::string m_Port;
        std::thread m_Thread;
        std::atomic<bool> m_Stop {false};
        std::mutex m_Mutex;
        std::string m_All;
};

// Answers every ACK (0x06) with the given reply.
FakeSerialDevice::Responder answerAck(const std::string &reply)
{
    return [reply](std::string & received)
    {
        std::string out;
        size_t pos;
        while ((pos = received.find('\x06')) != std::string::npos)
        {
            received.erase(0, pos + 1);
            out += reply;
        }
        return out;
    };
}

int checkPort(const std::string &port)
{
    int fd = -1;
    if (tty_connect(port.c_str(), 9600, 8, 0, 1, &fd) != TTY_OK)
        return -2;
    int rc = check_lx200_connection(fd);
    tty_disconnect(fd);
    return rc;
}

const char *WANDERER_STATUS = "WandererTilterM54A20260701A0.500A0.500A0.500A\r\n";

}

TEST(LX200ConnectionCheckTest, meadeMountAnswersAck)
{
    const char *replies[] = {"P", "A", "L", "D", "G"};
    for (const char *reply : replies)
    {
        FakeSerialDevice mount(answerAck(reply));
        ASSERT_FALSE(mount.port().empty());
        EXPECT_EQ(checkPort(mount.port()), 0) << "reply " << reply;
    }
}

TEST(LX200ConnectionCheckTest, answerWithTerminatorIsAccepted)
{
    const char *replies[] = {"P#", "P\r\n", "G#\r\n"};
    for (const char *reply : replies)
    {
        FakeSerialDevice mount(answerAck(reply));
        ASSERT_FALSE(mount.port().empty());
        EXPECT_EQ(checkPort(mount.port()), 0) << "reply with " << strlen(reply) << " characters";
    }
}

TEST(LX200ConnectionCheckTest, streamingDeviceIsRejected)
{
    FakeSerialDevice wanderer(nullptr, WANDERER_STATUS);
    ASSERT_FALSE(wanderer.port().empty());
    EXPECT_EQ(checkPort(wanderer.port()), -1) << "A device streaming status lines was taken for an LX200 mount";
}

TEST(LX200ConnectionCheckTest, staleInputIsNotTheAnswer)
{
    // A byte left over from before (power-up noise, another program) must not count as the answer to ACK.
    FakeSerialDevice silent(nullptr);
    ASSERT_FALSE(silent.port().empty());
    int fd = -1;
    ASSERT_EQ(tty_connect(silent.port().c_str(), 9600, 8, 0, 1, &fd), TTY_OK);
    silent.send("X");
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    int rc = check_lx200_connection(fd);
    tty_disconnect(fd);
    EXPECT_EQ(rc, -1) << "A stale byte was taken as the answer to ACK";
}

TEST(LX200ConnectionCheckTest, otherReplyIsRejected)
{
    // Any device that answers a single byte (e.g. an ack '1' or '#') is not an LX200 mount.
    const char *replies[] = {"1", "#", "0", "W"};
    for (const char *reply : replies)
    {
        FakeSerialDevice other(answerAck(reply));
        ASSERT_FALSE(other.port().empty());
        EXPECT_EQ(checkPort(other.port()), -1) << "reply " << reply;
    }
}

TEST(LX200ConnectionCheckTest, nulReplyIsRejected)
{
    // strchr() also finds the terminating NUL of the accepted set, so a 0x00 reply must be rejected explicitly
    // (a Pegasus NYX-101 sends two 0x00 bytes after power-up).
    FakeSerialDevice other(answerAck(std::string(1, char(0))));
    ASSERT_FALSE(other.port().empty());
    EXPECT_EQ(checkPort(other.port()), -1) << "reply 0x00";
}

TEST(LX200ConnectionCheckTest, silentPortIsRejected)
{
    FakeSerialDevice silent(nullptr);
    ASSERT_FALSE(silent.port().empty());
    EXPECT_EQ(checkPort(silent.port()), -1);
}

int main(int argc, char **argv)
{
    INDI::Logger::getInstance().configure("", INDI::Logger::file_off,
                                          INDI::Logger::DBG_ERROR, INDI::Logger::DBG_ERROR);
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
