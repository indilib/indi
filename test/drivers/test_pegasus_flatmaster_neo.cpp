/*
    Regression tests for the Pegasus FlatMaster Neo driver: a device that stops answering must not keep the driver
    connected. Over WiFi the Neo keeps the TCP connection open after it left the network (e.g. after a WiFi factory
    reset) but never answers again; the driver then stayed connected and waited for the timeout of every query of
    every poll.

    The driver talks to a fake FlatMaster Neo on a pseudo terminal, i.e. over the USB connection; the poll logic is
    the same over TCP. Its replies are copied from a debug log of a real FlatMaster Neo, firmware 1.4, over USB,
    except the reply to AL (hotspot SSID and password), which the driver masks in its log.

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <gtest/gtest.h>

#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <fcntl.h>
#include <map>
#include <mutex>
#if defined(__APPLE__) || defined(__OpenBSD__) || defined(__NetBSD__)
#include <util.h>
#elif defined(__FreeBSD__)
#include <libutil.h>
#else
#include <pty.h>
#endif
#include <regex>
#include <string>
#include <sys/select.h>
#include <termios.h>
#include <thread>
#include <unistd.h>
#include <vector>

#include "indilogger.h"
#include "drivers/auxiliary/pegasus_flatmaster_neo.h"

namespace
{

const char *DEVICE_NAME = "Mock FlatMaster Neo";

// Fake FlatMaster Neo answering the commands the driver sends while connecting and polling.
class FakeFlatMasterNeo
{
    public:
        FakeFlatMasterNeo()
        {
            char name[128] = {0};
            if (openpty(&m_Master, &m_Slave, name, nullptr, nullptr) != 0)
                return;
            makeRaw(m_Master);
            makeRaw(m_Slave);
            m_PortName = name;
            m_Thread = std::thread(&FakeFlatMasterNeo::serve, this);
        }

        ~FakeFlatMasterNeo()
        {
            m_Stop = true;
            if (m_Thread.joinable())
                m_Thread.join();
            if (m_Slave >= 0)
                close(m_Slave);
            if (m_Master >= 0)
                close(m_Master);
        }

        const std::string &portName() const
        {
            return m_PortName;
        }

        // A silent device still receives commands but never answers.
        void setSilent(bool silent)
        {
            m_Silent = silent;
        }

        // The commands received since the last call.
        std::vector<std::string> takeCommands()
        {
            std::lock_guard<std::mutex> lock(m_Mutex);
            std::vector<std::string> commands;
            commands.swap(m_Commands);
            return commands;
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
            std::string buffer;
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
                buffer.append(chunk, n);

                size_t end;
                while ((end = buffer.find('\n')) != std::string::npos)
                {
                    std::string command = buffer.substr(0, end);
                    buffer.erase(0, end + 1);
                    if (!command.empty() && command.back() == '\r')
                        command.pop_back();
                    if (!command.empty())
                        reply(command);
                }
            }
        }

        void reply(const std::string &command)
        {
            {
                std::lock_guard<std::mutex> lock(m_Mutex);
                m_Commands.push_back(command);
            }
            if (m_Silent)
                return;

            // Replies of a FlatMaster Neo, firmware 1.4, over USB. AL is masked in that log: SSID and password
            // separated by ':', as the driver parses it.
            static const std::map<std::string, std::string> replies =
            {
                {"F#", "FMNEO_c3d2e0_A"},
                {"FV", "FV:1.4"},
                {"A?", "0"},
                {"AL", "FMNEO_c3d2e0:12345678"},
                {"AC", "11"},
                {"WA", "Astro,192.168.10.84,50,3"},
                {"CR", "CR:0.0:0"},
                {"DT", "DT:4"},
                {"DC:99", "DC:1"},
                {"FA", "FMNEO:0:1:0:270:1:0:1:5:0"},
                {"ES", "ES:23.3:45:10.7"},
                {"FU", "1887"},
            };
            auto it = replies.find(command);
            if (it == replies.end())
                return;
            std::string answer = it->second + "\r\n";
            if (write(m_Master, answer.data(), answer.size()) < 0)
                return;
        }

        int m_Master {-1};
        int m_Slave {-1};
        std::string m_PortName;
        std::thread m_Thread;
        std::atomic<bool> m_Stop {false};
        std::atomic<bool> m_Silent {false};
        std::mutex m_Mutex;
        std::vector<std::string> m_Commands;
};

class FlatMasterNeoUnderTest : public PegasusFlatMasterNeo
{
    public:
        FlatMasterNeoUnderTest()
        {
            setDeviceName(DEVICE_NAME);
            // As at a client's first getProperties; this also runs initProperties().
            ISGetProperties(nullptr);
        }

        // Connect the way a client does it: port, then CONNECT. Auto search is switched off so a failed connection
        // doesn't scan the real serial ports of the machine running the test.
        bool connectTo(const std::string &port)
        {
            ISState autoSearch[2] = {ISS_OFF, ISS_ON};
            char enabled[] = "INDI_ENABLED", disabled[] = "INDI_DISABLED";
            char *autoSearchNames[2] = {enabled, disabled};
            ISNewSwitch(getDeviceName(), "DEVICE_AUTO_SEARCH", autoSearch, autoSearchNames, 2);

            std::string portText = port;
            char portName[] = "PORT";
            char *texts[1] = {&portText[0]};
            char *textNames[1] = {portName};
            ISNewText(getDeviceName(), "DEVICE_PORT", texts, textNames, 1);

            ISState connection[2] = {ISS_ON, ISS_OFF};
            char connect[] = "CONNECT", disconnect[] = "DISCONNECT";
            char *connectionNames[2] = {connect, disconnect};
            ISNewSwitch(getDeviceName(), "CONNECTION", connection, connectionNames, 2);
            return isConnected();
        }

        // One poll, as the polling timer runs it.
        void poll()
        {
            TimerHit();
        }
};

// Captures what the driver publishes on stdout and returns the states of the CONNECTION updates.
class PublishedStates
{
    public:
        PublishedStates()
        {
            fflush(stdout);
            m_File = tmpfile();
            m_SavedStdout = dup(STDOUT_FILENO);
            dup2(fileno(m_File), STDOUT_FILENO);
        }

        ~PublishedStates()
        {
            restore();
            if (m_File)
                fclose(m_File);
        }

        std::vector<std::string> connectionStates()
        {
            restore();
            std::string text;
            rewind(m_File);
            char chunk[4096];
            size_t n;
            while ((n = fread(chunk, 1, sizeof(chunk), m_File)) > 0)
                text.append(chunk, n);

            std::vector<std::string> states;
            std::regex update("<setSwitchVector[^>]*name=['\"]CONNECTION['\"][^>]*state=['\"](\\w+)['\"]");
            for (auto it = std::sregex_iterator(text.begin(), text.end(), update); it != std::sregex_iterator(); ++it)
                states.push_back((*it)[1]);
            return states;
        }

    private:
        void restore()
        {
            if (m_SavedStdout < 0)
                return;
            fflush(stdout);
            dup2(m_SavedStdout, STDOUT_FILENO);
            close(m_SavedStdout);
            m_SavedStdout = -1;
        }

        FILE *m_File {nullptr};
        int m_SavedStdout {-1};
};

}

// A device that stops answering: two polls without a status reply are tolerated, the third disconnects the driver
// and reports the connection as failed.
TEST(PegasusFlatMasterNeoTest, silentDeviceIsDisconnectedAfterThreeMissedPolls)
{
    FakeFlatMasterNeo device;
    ASSERT_FALSE(device.portName().empty());
    FlatMasterNeoUnderTest driver;
    ASSERT_TRUE(driver.connectTo(device.portName()));
    driver.poll();
    ASSERT_TRUE(driver.isConnected());

    PublishedStates published;
    device.setSilent(true);
    driver.poll();
    driver.poll();
    bool connectedAfterTwo = driver.isConnected();
    driver.poll();
    bool connectedAfterThree = driver.isConnected();
    std::vector<std::string> states = published.connectionStates();

    EXPECT_TRUE(connectedAfterTwo) << "The driver disconnected before three polls in a row went unanswered";
    EXPECT_FALSE(connectedAfterThree) << "The driver stayed connected to a device that no longer answers";
    ASSERT_FALSE(states.empty()) << "The lost connection was never published";
    EXPECT_EQ(states.back(), "Alert");
}

// Without a reply to the status query FA, the rest of the poll is skipped: each further query would wait for its
// own timeout and block the driver for seconds per poll.
TEST(PegasusFlatMasterNeoTest, pollWithoutStatusReplySendsNoFurtherQueries)
{
    FakeFlatMasterNeo device;
    ASSERT_FALSE(device.portName().empty());
    FlatMasterNeoUnderTest driver;
    ASSERT_TRUE(driver.connectTo(device.portName()));
    device.takeCommands();
    driver.poll();
    std::vector<std::string> answered = device.takeCommands();

    device.setSilent(true);
    driver.poll();
    std::vector<std::string> unanswered = device.takeCommands();

    // Control: an answered poll sends all its queries.
    EXPECT_EQ(answered, (std::vector<std::string> {"FA", "ES", "FU", "WA"}));
    EXPECT_EQ(unanswered, (std::vector<std::string> {"FA"}));
}

// Only polls in a row count: an answered poll in between starts the count again.
TEST(PegasusFlatMasterNeoTest, answeredPollResetsMissedPollCount)
{
    FakeFlatMasterNeo device;
    ASSERT_FALSE(device.portName().empty());
    FlatMasterNeoUnderTest driver;
    ASSERT_TRUE(driver.connectTo(device.portName()));

    device.setSilent(true);
    driver.poll();
    driver.poll();
    device.setSilent(false);
    driver.poll();
    device.setSilent(true);
    driver.poll();
    driver.poll();

    EXPECT_TRUE(driver.isConnected()) << "Missed polls separated by an answered one disconnected the driver";
}

int main(int argc, char **argv)
{
    // Never touch the user's real ~/.indi.
    char homeTemplate[] = "/tmp/test_pegasus_flatmaster_neo_home_XXXXXX";
    const char *home = mkdtemp(homeTemplate);
    if (home)
        setenv("HOME", home, 1);
    // Never read or write the user's real driver configuration.
    const std::string configFile = "/tmp/test_pegasus_flatmaster_neo_" + std::to_string(getpid()) + "_config.xml";
    setenv("INDICONFIG", configFile.c_str(), 1);
    INDI::Logger::getInstance().configure("", INDI::Logger::file_off,
                                          INDI::Logger::DBG_ERROR, INDI::Logger::DBG_ERROR);
    ::testing::InitGoogleTest(&argc, argv);
    int rc = RUN_ALL_TESTS();
    std::remove(configFile.c_str());
    std::remove((configFile + ".default").c_str());
    if (home)
    {
        std::remove((std::string(home) + "/.indi").c_str());
        std::remove(home);
    }
    return rc;
}
