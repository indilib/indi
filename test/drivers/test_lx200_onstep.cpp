/*
    Regression tests for the LX200 OnStep driver: every goto must be answered with an EQUATORIAL_EOD_COORD
    update, also a goto that OnStep completes before the next status poll without changing the reported
    position (e.g. a goto to the position the mount is already at). Clients mark the property busy when they
    send a goto and wait for the driver's answer, so a goto without an answer leaves them waiting forever.

    The driver talks to a fake OnStep controller on a pseudo terminal. Its replies are copied from a log
    of a real OnStep 4.24 controller.

    SPDX-License-Identifier: LGPL-2.1-or-later
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
#include <termios.h>
#include <thread>
#include <unistd.h>
#include <vector>

#include "indilogger.h"
#include "drivers/telescope/lx200_OnStep.h"

namespace
{

// OnStep :GU# status strings. 'N' = not slewing, 'n' = not tracking, 'p' = not parked.
const char *STATUS_TRACKING = "Npa/ET160";
const char *STATUS_SLEWING  = "pa/ET160";

// Fake OnStep controller answering the commands the driver sends during a goto and a status poll.
class FakeOnStep
{
    public:
        FakeOnStep()
        {
            if (openpty(&m_Master, &m_Slave, nullptr, nullptr, nullptr) != 0)
                return;
            makeRaw(m_Master);
            makeRaw(m_Slave);
            m_Thread = std::thread(&FakeOnStep::serve, this);
        }

        ~FakeOnStep()
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

        void setStatus(const std::string &status, const std::string &ra, const std::string &dec)
        {
            std::lock_guard<std::mutex> lock(m_Mutex);
            m_Status = status;
            m_RA     = ra;
            m_DEC    = dec;
        }

        int gotos()
        {
            std::lock_guard<std::mutex> lock(m_Mutex);
            return m_Gotos;
        }

        // Leave the next n :GU# requests unanswered, as a real controller sometimes does right after :MS#.
        void dropStatusReplies(int n)
        {
            std::lock_guard<std::mutex> lock(m_Mutex);
            m_DropStatusReplies = n;
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
                while ((end = buffer.find('#')) != std::string::npos)
                {
                    // Commands start at the first ':' (RA/DEC arguments contain ':' too)
                    size_t start = buffer.find(':');
                    std::string command = (start != std::string::npos && start < end) ? buffer.substr(start, end - start + 1) : "";
                    buffer.erase(0, end + 1);
                    if (!command.empty())
                        reply(command);
                }
            }
        }

        void reply(const std::string &command)
        {
            std::string answer;
            {
                std::lock_guard<std::mutex> lock(m_Mutex);
                if (command == ":GU#" && m_DropStatusReplies > 0)
                    m_DropStatusReplies--;
                else if (command == ":GU#")
                    answer = m_Status + "#";
                else if (command == ":GR#")
                    answer = m_RA + "#";
                else if (command == ":GD#")
                    answer = m_DEC + "#";
                else if (command.rfind(":Sr", 0) == 0 || command.rfind(":Sd", 0) == 0)
                    answer = "1";
                else if (command == ":MS#")
                {
                    // Goto accepted
                    answer = "0";
                    m_Gotos++;
                }
                else if (command == ":Q#")
                    answer = "";
                else if (command == ":GX90#")
                    answer = "0.50#";
                else if (command == ":GX96#")
                    answer = "E#";
                else if (command == ":Go#")
                    answer = "80*#";
                else if (command == ":Gh#")
                    answer = "+5*#";
                else if (command == ":A?#")
                    answer = "900#";
                else
                    answer = "0#";
            }
            if (!answer.empty() && write(m_Master, answer.data(), answer.size()) < 0)
                return;
        }

        int m_Master {-1};
        int m_Slave {-1};
        std::thread m_Thread;
        std::atomic<bool> m_Stop {false};
        std::mutex m_Mutex;
        std::string m_Status {STATUS_TRACKING};
        std::string m_RA {"17:57:13"};
        std::string m_DEC {"+51*29:19"};
        int m_Gotos {0};
        int m_DropStatusReplies {0};
};

class OnStepUnderTest : public LX200_OnStep
{
    public:
        explicit OnStepUnderTest(int fd)
        {
            setDeviceName("Mock OnStep");
            initProperties();
            PortFD = fd;
        }

        bool poll()
        {
            return ReadScopeStatus();
        }

        // Send a goto the way a client does it.
        bool requestGoto(double ra, double dec)
        {
            double values[2] = {ra, dec};
            char ra_name[] = "RA", dec_name[] = "DEC";
            char *names[2] = {ra_name, dec_name};
            return ISNewNumber(getDeviceName(), "EQUATORIAL_EOD_COORD", values, names, 2);
        }
};

// Captures what the driver publishes on stdout and returns the states of the EQUATORIAL_EOD_COORD updates.
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

        std::vector<std::string> eqStates()
        {
            restore();
            std::string text;
            rewind(m_File);
            char chunk[4096];
            size_t n;
            while ((n = fread(chunk, 1, sizeof(chunk), m_File)) > 0)
                text.append(chunk, n);

            std::vector<std::string> states;
            std::regex update("<setNumberVector[^>]*name=['\"]EQUATORIAL_EOD_COORD['\"][^>]*state=['\"](\\w+)['\"]");
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

const double ELTANIN_RA  = 17.0 + 57.0 / 60 + 13.0 / 3600;
const double ELTANIN_DEC = 51.0 + 29.0 / 60 + 19.0 / 3600;

}

// The mount already sits on the target and keeps tracking: OnStep never reports a slew and the position doesn't
// change. The driver must still answer the goto, first busy, then the tracking state after the next poll.
TEST(LX200OnStepTest, gotoToCurrentPositionIsAnswered)
{
    FakeOnStep mount;
    ASSERT_GE(mount.driverFD(), 0);
    OnStepUnderTest driver(mount.driverFD());

    ASSERT_TRUE(driver.poll());

    PublishedStates published;
    ASSERT_TRUE(driver.requestGoto(ELTANIN_RA, ELTANIN_DEC));
    ASSERT_TRUE(driver.poll());
    ASSERT_TRUE(driver.poll());
    std::vector<std::string> states = published.eqStates();

    EXPECT_EQ(mount.gotos(), 1);
    ASSERT_FALSE(states.empty()) << "The goto was never answered";
    EXPECT_EQ(states.front(), "Busy");
    EXPECT_EQ(states.back(), "Ok");
}

// Same, with the reported position 1" off the target before and after the goto (OnStep doesn't move for it).
TEST(LX200OnStepTest, gotoWithoutPositionChangeIsAnswered)
{
    FakeOnStep mount;
    ASSERT_GE(mount.driverFD(), 0);
    mount.setStatus(STATUS_TRACKING, "17:57:13", "+51*29:18");
    OnStepUnderTest driver(mount.driverFD());

    ASSERT_TRUE(driver.poll());

    PublishedStates published;
    ASSERT_TRUE(driver.requestGoto(ELTANIN_RA, ELTANIN_DEC));
    ASSERT_TRUE(driver.poll());
    std::vector<std::string> states = published.eqStates();

    ASSERT_FALSE(states.empty()) << "The goto was never answered";
    EXPECT_EQ(states.front(), "Busy");
    EXPECT_EQ(states.back(), "Ok");
}

// A real slew: busy while OnStep reports slewing, tracking only once it reports the slew done.
TEST(LX200OnStepTest, realSlewStaysBusyUntilOnStepReportsTracking)
{
    FakeOnStep mount;
    ASSERT_GE(mount.driverFD(), 0);
    mount.setStatus(STATUS_TRACKING, "17:31:01", "+52*17:02");
    OnStepUnderTest driver(mount.driverFD());

    ASSERT_TRUE(driver.poll());

    PublishedStates published;
    ASSERT_TRUE(driver.requestGoto(ELTANIN_RA, ELTANIN_DEC));
    mount.setStatus(STATUS_SLEWING, "17:44:07", "+51*53:10");
    ASSERT_TRUE(driver.poll());
    mount.setStatus(STATUS_SLEWING, "17:57:10", "+51*29:40");
    ASSERT_TRUE(driver.poll());
    std::vector<std::string> whileSlewing = published.eqStates();

    ASSERT_FALSE(whileSlewing.empty());
    for (const auto &state : whileSlewing)
        EXPECT_EQ(state, "Busy");

    PublishedStates afterSlew;
    mount.setStatus(STATUS_TRACKING, "17:57:13", "+51*29:19");
    ASSERT_TRUE(driver.poll());
    std::vector<std::string> states = afterSlew.eqStates();

    ASSERT_FALSE(states.empty());
    EXPECT_EQ(states.back(), "Ok");
}

// The controller doesn't answer the first status poll after the goto (seen on a WD-20EP 50 ms after :MS#). The
// driver must skip that poll instead of failing it: a failed poll puts EQUATORIAL_EOD_COORD into Alert, clients
// then abort and resend the goto, which OnStep refuses with "Goto in progress".
TEST(LX200OnStepTest, missedStatusReplyDuringGotoIsNotAnError)
{
    FakeOnStep mount;
    ASSERT_GE(mount.driverFD(), 0);
    mount.setStatus(STATUS_TRACKING, "17:31:01", "+52*17:02");
    OnStepUnderTest driver(mount.driverFD());

    ASSERT_TRUE(driver.poll());

    PublishedStates published;
    ASSERT_TRUE(driver.requestGoto(ELTANIN_RA, ELTANIN_DEC));
    mount.setStatus(STATUS_SLEWING, "17:44:07", "+51*53:10");
    mount.dropStatusReplies(1);
    EXPECT_TRUE(driver.poll()) << "A missed :GU# reply failed the status poll";
    ASSERT_TRUE(driver.poll());
    mount.setStatus(STATUS_TRACKING, "17:57:13", "+51*29:19");
    ASSERT_TRUE(driver.poll());
    std::vector<std::string> states = published.eqStates();

    ASSERT_FALSE(states.empty());
    for (const auto &state : states)
        EXPECT_NE(state, "Alert");
    EXPECT_EQ(states.front(), "Busy");
    EXPECT_EQ(states.back(), "Ok");
    EXPECT_EQ(mount.gotos(), 1);
}

int main(int argc, char **argv)
{
    // Never read or write the user's real ~/.indi driver configuration.
    const std::string configFile = "/tmp/test_lx200_onstep_" + std::to_string(getpid()) + "_config.xml";
    setenv("INDICONFIG", configFile.c_str(), 1);
    INDI::Logger::getInstance().configure("", INDI::Logger::file_off,
                                          INDI::Logger::DBG_ERROR, INDI::Logger::DBG_ERROR);
    ::testing::InitGoogleTest(&argc, argv);
    int rc = RUN_ALL_TESTS();
    std::remove(configFile.c_str());
    std::remove((configFile + ".default").c_str());
    return rc;
}
