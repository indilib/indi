/*
    Regression tests for the Wanderer ETA M54 handshake.

    The handshake must read a valid "WandererTilterM54..." status line on every connection attempt. The driver only
    did that on the first attempt after it started; later attempts (a reconnect, or an auto search trying other
    ports) took the polling path, which reports success on a read timeout, so the driver came "online" on a port of
    another device (seen with an RBF Excalibur on an identical CH340 adapter). A reconnect also loaded the saved
    positions as move requests.

    The driver talks to fake devices on pseudo terminals.

    SPDX-License-Identifier: LGPL-2.1-or-later
*/

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <fstream>
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
#include "drivers/auxiliary/wanderer_eta.h"

namespace
{

const char *DEVICE_NAME = "Mock Wanderer ETA";

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

// Status line streamed by the device about every 2 s (faster here).
const char *WANDERER_STATUS = "WandererTilterM54A20260701A0.500A0.500A0.500A\r\n";

class WandererUnderTest : public WandererETA
{
    public:
        WandererUnderTest()
        {
            setDeviceName(DEVICE_NAME);
            ISGetProperties(nullptr);
        }

        // One poll, as the driver's timer does while connected.
        using WandererETA::TimerHit;

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

            return setConnection("CONNECT");
        }

        bool disconnectNow()
        {
            return !setConnection("DISCONNECT");
        }

        void loadConfiguration()
        {
            ISState states[1] = {ISS_ON};
            char load[] = "CONFIG_LOAD";
            char *names[1] = {load};
            ISNewSwitch(getDeviceName(), "CONFIG_PROCESS", states, names, 1);
        }

    private:
        bool setConnection(const char *element)
        {
            ISState state[1] = {ISS_ON};
            std::vector<char> name(element, element + strlen(element) + 1);
            char *names[1] = {name.data()};
            ISNewSwitch(getDeviceName(), "CONNECTION", state, names, 1);
            return isConnected();
        }
};

std::string configFile()
{
    return "/tmp/test_wanderer_eta_" + std::to_string(getpid()) + "_config.xml";
}

// Saved positions that differ from the ones the device reports.
void writeConfiguration()
{
    std::ofstream config(configFile());
    config << "<INDIDriver>\n";
    for (int point = 1; point <= 3; point++)
        config << "<newNumberVector device='" << DEVICE_NAME << "' name='POINT_" << point << "_TARGET'>\n"
               << "  <oneNumber name='POINT_" << point << "'>0.800</oneNumber>\n"
               << "</newNumberVector>\n";
    config << "</INDIDriver>\n";
}

}

TEST(WandererETATest, connectsToWanderer)
{
    FakeSerialDevice wanderer(nullptr, WANDERER_STATUS);
    ASSERT_FALSE(wanderer.port().empty());

    bool connected;
    {
        QuietStdout quiet;
        WandererUnderTest driver;
        connected = driver.connectTo(wanderer.port());
    }

    EXPECT_TRUE(connected);
}

TEST(WandererETATest, secondConnectOnSilentPortFails)
{
    FakeSerialDevice wanderer(nullptr, WANDERER_STATUS);
    FakeSerialDevice silent(nullptr);
    ASSERT_FALSE(wanderer.port().empty());
    ASSERT_FALSE(silent.port().empty());

    bool first, disconnected, second;
    {
        QuietStdout quiet;
        WandererUnderTest driver;
        first = driver.connectTo(wanderer.port());
        driver.TimerHit();
        disconnected = driver.disconnectNow();
        second = driver.connectTo(silent.port());
    }

    ASSERT_TRUE(first);
    ASSERT_TRUE(disconnected);
    EXPECT_FALSE(second) << "The driver came online on a port that never sent a Wanderer status line";
}

TEST(WandererETATest, reconnectConfigLoadDoesNotMove)
{
    writeConfiguration();
    // A pseudo terminal keeps the exclusive mode of the first connection, so the reconnect uses a second fake
    // device (as after a USB re-enumeration).
    FakeSerialDevice wanderer(nullptr, WANDERER_STATUS);
    FakeSerialDevice again(nullptr, WANDERER_STATUS);
    ASSERT_FALSE(wanderer.port().empty());
    ASSERT_FALSE(again.port().empty());

    bool first, second;
    std::string sent;
    {
        QuietStdout quiet;
        WandererUnderTest driver;
        first = driver.connectTo(wanderer.port());
        driver.loadConfiguration();
        driver.TimerHit();
        driver.disconnectNow();
        second = driver.connectTo(again.port());
        driver.loadConfiguration();
        sent = again.received();
    }

    ASSERT_TRUE(first);
    ASSERT_TRUE(second);
    EXPECT_TRUE(sent.empty()) << "Commands sent on reconnect + config load: " << sent;
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
