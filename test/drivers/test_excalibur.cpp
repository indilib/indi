/*
    Regression tests for the RBF Excalibur handshake.

    During an auto search the driver probes ports that belong to other devices. One of them, a Wanderer ETA M54
    (CH340 adapter like the Excalibur's), streams long status lines without the Excalibur's '#' delimiter. The driver
    read the reply into a 16 byte buffer without a size limit and aborted with a stack overflow. The reply check also
    had its arguments swapped, so a bare '#' counted as an Excalibur.

    The driver talks to fake devices on pseudo terminals.

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

#include "indilogger.h"
#include "drivers/auxiliary/Excalibur.h"

namespace
{

const char *DEVICE_NAME = "Mock Excalibur";

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
};

// Answers like the Excalibur: "#" -> "FLAT.FLAP!#", "O#" / "P#" -> "0#", "L<n>##" -> "0#".
std::string excaliburReplies(std::string &received)
{
    std::string reply;
    size_t pos;
    while ((pos = received.find('#')) != std::string::npos)
    {
        std::string command = received.substr(0, pos);
        received.erase(0, pos + 1);
        if (command.empty())
            reply += "FLAT.FLAP!#";
        else if (command == "O" || command == "P" || command[0] == 'L')
            reply += "0#";
    }
    return reply;
}

// Answers every command with a bare delimiter.
std::string bareDelimiterReplies(std::string &received)
{
    std::string reply;
    size_t pos;
    while ((pos = received.find('#')) != std::string::npos)
    {
        received.erase(0, pos + 1);
        reply += "#";
    }
    return reply;
}

// Status line streamed by a Wanderer ETA M54 (no '#').
const char *WANDERER_STATUS = "WandererTilterM54A20260701A0.500A0.500A0.500A\r\n";

class ExcaliburUnderTest : public Excalibur
{
    public:
        ExcaliburUnderTest()
        {
            setDeviceName(DEVICE_NAME);
            INDI::DefaultDevice::ISGetProperties(nullptr);
        }

        bool connectTo(const std::string &port)
        {
            std::vector<char> portText(port.begin(), port.end());
            portText.push_back('\0');
            char portName[] = "PORT";
            char *texts[1] = {portText.data()};
            char *textNames[1] = {portName};
            ISNewText(getDeviceName(), "DEVICE_PORT", texts, textNames, 1);

            // Never scan the system's real serial ports if the connection fails.
            ISState searchStates[2] = {ISS_OFF, ISS_ON};
            char enabled[] = "INDI_ENABLED", disabled[] = "INDI_DISABLED";
            char *searchNames[2] = {enabled, disabled};
            ISNewSwitch(getDeviceName(), "DEVICE_AUTO_SEARCH", searchStates, searchNames, 2);

            ISState connectState[1] = {ISS_ON};
            char connect[] = "CONNECT";
            char *connectNames[1] = {connect};
            ISNewSwitch(getDeviceName(), "CONNECTION", connectState, connectNames, 1);
            return isConnected();
        }
};

// Keeps the driver's XML output out of the test log.
class QuietStdout
{
    public:
        QuietStdout()
        {
            fflush(stdout);
            m_Saved = dup(STDOUT_FILENO);
            FILE *null = fopen("/dev/null", "w");
            if (null)
            {
                dup2(fileno(null), STDOUT_FILENO);
                fclose(null);
            }
        }
        ~QuietStdout()
        {
            fflush(stdout);
            if (m_Saved >= 0)
            {
                dup2(m_Saved, STDOUT_FILENO);
                close(m_Saved);
            }
        }
    private:
        int m_Saved {-1};
};

std::string configFile()
{
    return "/tmp/test_excalibur_" + std::to_string(getpid()) + "_config.xml";
}

}

TEST(ExcaliburTest, handshakeOnStreamingDeviceFailsWithoutCrash)
{
    FakeSerialDevice wanderer(nullptr, WANDERER_STATUS);
    ASSERT_FALSE(wanderer.port().empty());

    bool connected;
    {
        QuietStdout quiet;
        ExcaliburUnderTest driver;
        connected = driver.connectTo(wanderer.port());
    }

    EXPECT_FALSE(connected) << "The Excalibur driver accepted a Wanderer ETA as an Excalibur";
}

TEST(ExcaliburTest, handshakeRejectsBareDelimiter)
{
    FakeSerialDevice other(bareDelimiterReplies);
    ASSERT_FALSE(other.port().empty());

    bool connected;
    {
        QuietStdout quiet;
        ExcaliburUnderTest driver;
        connected = driver.connectTo(other.port());
    }

    EXPECT_FALSE(connected) << "An empty reply was accepted as the Excalibur's FLAT.FLAP!";
}

TEST(ExcaliburTest, handshakeAcceptsExcalibur)
{
    FakeSerialDevice excalibur(excaliburReplies);
    ASSERT_FALSE(excalibur.port().empty());

    bool connected;
    {
        QuietStdout quiet;
        ExcaliburUnderTest driver;
        connected = driver.connectTo(excalibur.port());
    }

    EXPECT_TRUE(connected);
}

int main(int argc, char **argv)
{
    setenv("INDICONFIG", configFile().c_str(), 1);
    INDI::Logger::getInstance().configure("", INDI::Logger::file_off,
                                          INDI::Logger::DBG_ERROR, INDI::Logger::DBG_ERROR);
    ::testing::InitGoogleTest(&argc, argv);
    int rc = RUN_ALL_TESTS();
    std::remove(configFile().c_str());
    std::remove((configFile() + ".default").c_str());
    return rc;
}
