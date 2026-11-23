/*
    Regression tests for the Pegasus NYX-101 driver (indi_lx200_pegasus_nyx101):
    - An unpark is not undone by a status that still reports the mount parked, an unpark done outside INDI is
      followed, and a failed status query keeps the current state (#2535, #2540).
    - The reply to the unpark command is read and not left for the next status query (#2535).
    - A park that the mount reports as failed ends with an error instead of staying busy (#2535).
    - GUIDE_RATE is the standard number property, read from the mount at connect (#2535).
    - Park Options -> Current stores the park position in the mount; the park position property, which PARK_SIMPLE
      never fills, is not defined (#2535).
    - A GoTo is complete only once the mount has reported slewing, or after a timeout if it never did (#2540).
    - A guide pulse longer than four digits is clamped (LX200 base, #2535).
    - The Report property keeps its own copy of the status text instead of pointing at a buffer on the stack.

    The driver talks to a fake NYX-101 on a pseudo terminal, connected the way a client does it. Where its replies
    come from is noted at each of them: most are copied from debug logs of a real NYX-101 (firmware 1.39.2, USB,
    115200 baud); the others are assumed and marked as such.

    SPDX-License-Identifier: LGPL-2.1-or-later
*/

#include <gtest/gtest.h>

#include <algorithm>
#include <atomic>
#include <cctype>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <fcntl.h>
#include <mutex>
#if defined(__APPLE__) || defined(__OpenBSD__) || defined(__NetBSD__)
#include <util.h>
#elif defined(__FreeBSD__)
#include <libutil.h>
#else
#include <pty.h>
#endif
#include <string>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <termios.h>
#include <thread>
#include <unistd.h>
#include <utility>
#include <vector>

#include "indilogger.h"
#include "drivers/telescope/lx200_pegasus_nyx101.h"

namespace
{

const char *DEVICE_NAME = "Mock NYX-101";

// :GU# replies. The driver reads n = not tracking, N = not slewing, P/p = parked/not parked, I = parking,
// F = park failed, E/A = equatorial/alt-az mount.
// Real NYX-101 logs: parked, unparked and tracking, parking.
const char *STATUS_PARKED   = "nNPezET1:0#";
const char *STATUS_TRACKING = "NpezET1:0#";
const char *STATUS_PARKING  = "nIzET1:0#";
// Assumed: STATUS_TRACKING without 'N', as the real STATUS_PARKING has no 'N' while the mount moves. The logs have
// no status taken during a GoTo.
const char *STATUS_SLEWING  = "pezET1:0#";
// Assumed: 'F' as the driver reads it; the logs have no failed park.
const char *STATUS_PARK_FAILED = "nNpFzET1:0#";

const char *ACK_COMMAND = "<ACK>";

// Fake NYX-101 answering the commands the driver sends while connecting, polling, parking, slewing and guiding.
// Every command it receives is recorded.
class FakeNyx
{
    public:
        FakeNyx()
        {
            char name[128] = {0};
            if (openpty(&m_Master, &m_Slave, name, nullptr, nullptr) != 0)
                return;
            makeRaw(m_Master);
            makeRaw(m_Slave);
            m_PortName = name;
            m_Receiver = std::thread(&FakeNyx::receive, this);
            m_Processor = std::thread(&FakeNyx::process, this);
        }

        ~FakeNyx()
        {
            m_Stop = true;
            m_Received.notify_all();
            if (m_Receiver.joinable())
                m_Receiver.join();
            if (m_Processor.joinable())
                m_Processor.join();
            if (m_Slave >= 0)
                close(m_Slave);
            if (m_Master >= 0)
                close(m_Master);
        }

        const std::string &portName() const
        {
            return m_PortName;
        }

        void setStatus(const std::string &status)
        {
            std::lock_guard<std::mutex> lock(m_Mutex);
            m_Status = status;
        }

        // Reply to :GX90# (guide rate).
        void setGuideRateReply(const std::string &reply)
        {
            std::lock_guard<std::mutex> lock(m_Mutex);
            m_GuideRate = reply;
        }

        // Reply to :hQ# (store the current position as the park position).
        void setParkPositionReply(const std::string &reply)
        {
            std::lock_guard<std::mutex> lock(m_Mutex);
            m_ParkPositionReply = reply;
        }

        // Leave the next n :GU# requests unanswered.
        void dropStatusReplies(int n)
        {
            std::lock_guard<std::mutex> lock(m_Mutex);
            m_DropStatusReplies = n;
        }

        // Commands received since the last call, ACK as "<ACK>". Waits until every byte received so far is handled.
        std::vector<std::string> takeCommands()
        {
            waitUntilIdle();
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

        // Waits (at most 2 s) until nothing is left on the line, in the receive buffer or in progress.
        void waitUntilIdle()
        {
            for (int i = 0; i < 400; i++)
            {
                int pending = 0;
                ioctl(m_Master, FIONREAD, &pending);
                {
                    std::lock_guard<std::mutex> lock(m_RxMutex);
                    if (pending == 0 && m_Rx.empty() && !m_Busy)
                        return;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
            }
        }

        void send(const std::string &reply)
        {
            if (!reply.empty() && write(m_Master, reply.data(), reply.size()) < 0)
                return;
        }

        // Takes every byte off the line as soon as it arrives, like the receive buffer of the mount, so a flush on the
        // driver's side never discards what the mount has already received.
        void receive()
        {
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
                {
                    std::lock_guard<std::mutex> lock(m_RxMutex);
                    m_Rx.append(chunk, n);
                }
                m_Received.notify_all();
            }
        }

        // Handles the received bytes in order and sends the replies.
        void process()
        {
            std::string command;
            while (!m_Stop)
            {
                std::string bytes;
                {
                    std::unique_lock<std::mutex> lock(m_RxMutex);
                    m_Received.wait_for(lock, std::chrono::milliseconds(20), [this]()
                    {
                        return m_Stop || !m_Rx.empty();
                    });
                    bytes.swap(m_Rx);
                    m_Busy = !bytes.empty();
                }

                for (const char c : bytes)
                {
                    // A command runs from ':' to '#'; arguments contain ':' too (":Sr09:33:26#").
                    if (c == 0x06 && command.empty())
                        handle(ACK_COMMAND);
                    else if (c == ':' && command.empty())
                        command = ":";
                    else if (!command.empty())
                    {
                        command += c;
                        if (c == '#')
                        {
                            handle(command);
                            command.clear();
                        }
                    }
                }

                std::lock_guard<std::mutex> lock(m_RxMutex);
                if (m_Rx.empty())
                    m_Busy = false;
            }
        }

        void handle(const std::string &command)
        {
            std::string reply;
            bool delayed = false;
            {
                std::lock_guard<std::mutex> lock(m_Mutex);
                m_Commands.push_back(command);

                // Real NYX-101: ACK answers 'P' (no '#') in equatorial and in alt-az mode.
                if (command == ACK_COMMAND)
                    reply = "P";
                else if (command == ":GU#" && m_DropStatusReplies > 0)
                    m_DropStatusReplies--;
                else if (command == ":GU#")
                    reply = m_Status;
                // Real NYX-101 logs: position, pier side and the replies the driver reads at connect.
                else if (command == ":GR#")
                    reply = "06:25:23#";
                else if (command == ":GD#")
                    reply = "+90*00:00#";
                else if (command == ":Gm#")
                    reply = "E#";
                else if (command == ":GX90#")
                    reply = m_GuideRate;
                else if (command == ":Go#")
                    reply = "90*#";
                else if (command == ":Gh#")
                    reply = "+0*#";
                else if (command == ":GXE9#")
                    reply = "0#";
                else if (command == ":GG#")
                    reply = "-01:00#";
                else if (command == ":GL#")
                    reply = "21:43:35#";
                else if (command == ":GC#")
                    reply = "11/14/26#";
                // Format as in the real logs, the site itself is made up.
                else if (command == ":Gt#")
                    reply = "+50*00#";
                else if (command == ":Gg#")
                    reply = "-008*00#";
                // Real NYX-101: :hR# answers '1'. It is sent 100 ms late, so that the driver before the fix, which did
                // not read it, finds it in front of the next reply ("1NpegzEW1:0#" in a log of that driver).
                else if (command == ":hR#")
                {
                    reply = "1";
                    delayed = true;
                }
                // Real NYX-101: :hQ# answers '1'. '0' (refused) is assumed from the driver.
                else if (command == ":hQ#")
                    reply = m_ParkPositionReply;
                // Real NYX-101: :Te# answers '1'; assumed the same for :Td#.
                else if (command == ":Te#" || command == ":Td#")
                    reply = "1";
                // Assumed from the Meade LX200 protocol, the logs have no GoTo: target accepted ('1'), slew possible ('0').
                else if (command.rfind(":Sr", 0) == 0 || command.rfind(":Sd", 0) == 0)
                    reply = "1";
                else if (command == ":MS#")
                    reply = "0";
                // No reply: :hP# (assumed; the driver doesn't read one and the logs show no stray byte after it),
                // :RAx# :REx# :R0-2# (assumed) and :Mg...# (Meade LX200 protocol: returns nothing).
            }

            if (delayed)
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            send(reply);
        }

        int m_Master {-1};
        int m_Slave {-1};
        std::string m_PortName;
        std::thread m_Receiver;
        std::thread m_Processor;
        std::atomic<bool> m_Stop {false};
        std::mutex m_RxMutex;
        std::condition_variable m_Received;
        std::string m_Rx;
        bool m_Busy {false};
        std::mutex m_Mutex;
        std::string m_Status {STATUS_PARKED};
        // Real NYX-101 log.
        std::string m_GuideRate {"0.50#"};
        std::string m_ParkPositionReply {"1"};
        int m_DropStatusReplies {0};
        std::vector<std::string> m_Commands;
};

class NYXUnderTest : public LX200NYX101
{
    public:
        NYXUnderTest()
        {
            setDeviceName(DEVICE_NAME);
        }

        // As at a client's first getProperties (this also runs initProperties()), then connect the way a client does
        // it: port, then CONNECT. Auto search is switched off so a failed connection doesn't scan the real serial
        // ports of the machine running the test.
        bool connectTo(const std::string &port)
        {
            ISGetProperties(nullptr);

            ISState autoSearch[2] = {ISS_OFF, ISS_ON};
            char enabled[] = "INDI_ENABLED", disabled[] = "INDI_DISABLED";
            char *autoSearchNames[2] = {enabled, disabled};
            ISNewSwitch(getDeviceName(), "DEVICE_AUTO_SEARCH", autoSearch, autoSearchNames, 2);

            std::string portText = port;
            char portName[] = "PORT";
            char *texts[1] = {&portText[0]};
            char *textNames[1] = {portName};
            ISNewText(getDeviceName(), "DEVICE_PORT", texts, textNames, 1);

            return pressSwitch("CONNECTION", "CONNECT", "DISCONNECT") && isConnected();
        }

        // One status poll, as the polling timer runs it.
        bool poll()
        {
            return ReadScopeStatus();
        }

        bool requestPark()
        {
            return pressSwitch("TELESCOPE_PARK", "PARK", "UNPARK");
        }

        bool requestUnpark()
        {
            return pressSwitch("TELESCOPE_PARK", "UNPARK", "PARK");
        }

        // PARK_CURRENT or PARK_DEFAULT.
        bool requestParkOption(const char *option)
        {
            return pressSwitch("TELESCOPE_PARK_OPTION", option);
        }

        bool requestGoto(double ra, double dec)
        {
            return setNumbers("EQUATORIAL_EOD_COORD", "RA", ra, "DEC", dec);
        }

        bool requestGuideRate(double we, double ns)
        {
            return setNumbers("GUIDE_RATE", "GUIDE_RATE_WE", we, "GUIDE_RATE_NS", ns);
        }

        bool requestGuidePulseNorth(double ms)
        {
            return setNumbers("TELESCOPE_TIMED_GUIDE_NS", "TIMED_GUIDE_N", ms, "TIMED_GUIDE_S", 0);
        }

        bool parked()
        {
            return isParked();
        }

        // The text of the Report property as the driver holds it, e.g. for a client's getProperties.
        std::string reportText()
        {
            auto report = getText("Report");
            if (!report.isValid() || report[0].getText() == nullptr)
                return "(none)";
            std::string text = report[0].getText();
            return text.size() > 32 ? text.substr(0, 32) + "..." : text;
        }

        bool trackStateIs(TelescopeStatus state) const
        {
            return TrackState == state;
        }

    private:
        bool pressSwitch(const char *property, const char *on, const char *off = nullptr)
        {
            std::string onName = on, offName = off ? off : "";
            ISState states[2] = {ISS_ON, ISS_OFF};
            char *names[2] = {&onName[0], &offName[0]};
            return ISNewSwitch(getDeviceName(), property, states, names, off ? 2 : 1);
        }

        bool setNumbers(const char *property, const char *first, double firstValue, const char *second,
                        double secondValue)
        {
            std::string firstName = first, secondName = second;
            double values[2] = {firstValue, secondValue};
            char *names[2] = {&firstName[0], &secondName[0]};
            return ISNewNumber(getDeviceName(), property, values, names, 2);
        }
};

// Captures what the driver publishes on stdout.
class Published
{
    public:
        Published()
        {
            fflush(stdout);
            m_File = tmpfile();
            m_SavedStdout = dup(STDOUT_FILENO);
            dup2(fileno(m_File), STDOUT_FILENO);
        }

        ~Published()
        {
            restore();
            if (m_File)
                fclose(m_File);
        }

        // Stops capturing; what was captured can still be read.
        void stop()
        {
            restore();
        }

        // Every <tag ...>...</tag> element of the property, e.g. ("setNumberVector", "GUIDE_RATE").
        std::vector<std::string> vectors(const std::string &tag, const std::string &property)
        {
            std::vector<std::string> result;
            for (const auto &vector : elements(read(), tag, property))
                result.push_back(vector.first + ">" + vector.second);
            return result;
        }

        // States of the updates of a property.
        std::vector<std::string> states(const std::string &tag, const std::string &property)
        {
            std::vector<std::string> result;
            for (const auto &vector : elements(read(), tag, property))
                result.push_back(attribute(vector.first, "state"));
            return result;
        }

        // Values of an element in each update of a property, e.g. ("setTextVector", "Report", "oneText", "Report").
        std::vector<std::string> values(const std::string &tag, const std::string &property, const std::string &elementTag,
                                        const std::string &element)
        {
            std::vector<std::string> result;
            for (const auto &vector : elements(read(), tag, property))
                for (const auto &value : elements(vector.second, elementTag, element))
                    result.push_back(trim(value.second));
            return result;
        }

    private:
        // Header (up to '>') and content of every <tag ...>...</tag> element with the given name. The XML writer puts
        // a space or a line break after the tag and between the attributes.
        static std::vector<std::pair<std::string, std::string>> elements(const std::string &text, const std::string &tag,
                const std::string &name)
        {
            std::vector<std::pair<std::string, std::string>> result;
            const std::string open = "<" + tag, close = "</" + tag + ">";
            for (size_t start = text.find(open); start != std::string::npos; start = text.find(open, start + 1))
            {
                if (start + open.size() >= text.size() || !isspace(static_cast<unsigned char>(text[start + open.size()])))
                    continue;
                size_t headerEnd = text.find('>', start);
                size_t end = text.find(close, start);
                if (headerEnd == std::string::npos || end == std::string::npos)
                    break;
                std::string header = text.substr(start, headerEnd - start);
                if (attribute(header, "name") == name)
                    result.emplace_back(header, text.substr(headerEnd + 1, end - headerEnd - 1));
            }
            return result;
        }

        static std::string attribute(const std::string &header, const std::string &name)
        {
            static const char quotes[] = {'"', '\''};
            for (const char quote : quotes)
            {
                const std::string key = name + "=" + quote;
                for (size_t start = header.find(key); start != std::string::npos; start = header.find(key, start + 1))
                {
                    if (start == 0 || !isspace(static_cast<unsigned char>(header[start - 1])))
                        continue;
                    start += key.size();
                    return header.substr(start, header.find(quote, start) - start);
                }
            }
            return "";
        }

        static std::string trim(const std::string &text)
        {
            size_t start = text.find_first_not_of(" \t\r\n");
            size_t end = text.find_last_not_of(" \t\r\n");
            return start == std::string::npos ? "" : text.substr(start, end - start + 1);
        }

        std::string read()
        {
            restore();
            std::string text;
            rewind(m_File);
            char chunk[4096];
            size_t n;
            while ((n = fread(chunk, 1, sizeof(chunk), m_File)) > 0)
                text.append(chunk, n);
            return text;
        }

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

// Connects the driver with its output captured, so the property definitions don't fill the test log.
bool connectQuietly(NYXUnderTest &driver, FakeNyx &mount)
{
    Published quiet;
    bool connected = driver.connectTo(mount.portName());
    mount.takeCommands();
    return connected;
}

// One poll with its output captured.
bool pollQuietly(NYXUnderTest &driver, FakeNyx &mount)
{
    Published quiet;
    bool rc = driver.poll();
    mount.takeCommands();
    return rc;
}

// Overwrites the stack below its caller, where the frames of the functions the caller called before were.
__attribute__((noinline)) void overwriteStack()
{
    volatile char buffer[16384];
    for (size_t i = 0; i < sizeof(buffer) - 1; ++i)
        buffer[i] = 'X';
    buffer[sizeof(buffer) - 1] = '\0';
}

std::string lastSwitch(Published &published, const char *property, const char *element)
{
    std::vector<std::string> values = published.values("setSwitchVector", property, "oneSwitch", element);
    return values.empty() ? "" : values.back();
}

const double TARGET_RA  = 9.0 + 33.0 / 60 + 26.0 / 3600;
const double TARGET_DEC = 85.0;

}

// After :hR# the mount may still report 'P' for a moment. That status must not park the driver again: it would
// refuse every motion command with "Please unpark the mount", e.g. the rotation of the Ekos polar alignment. The real
// mount reported 'p' at the first poll in the logged cycles; here it keeps reporting 'P' for that poll.
TEST(PegasusNYX101Test, unparkIsNotUndoneByStaleParkedStatus)
{
    FakeNyx mount;
    ASSERT_FALSE(mount.portName().empty());
    NYXUnderTest driver;
    ASSERT_TRUE(connectQuietly(driver, mount));
    ASSERT_TRUE(driver.parked());

    Published published;
    bool unparked = driver.requestUnpark();
    bool polled = driver.poll();
    bool parkedAfterStaleStatus = driver.parked();
    bool gotoAccepted = driver.requestGoto(TARGET_RA, TARGET_DEC);
    std::vector<std::string> commands = mount.takeCommands();
    std::string parkSwitch = lastSwitch(published, "TELESCOPE_PARK", "PARK");

    EXPECT_TRUE(unparked);
    EXPECT_TRUE(polled);
    EXPECT_FALSE(parkedAfterStaleStatus) << "A status still showing 'P' right after the unpark parked the driver again";
    EXPECT_EQ(parkSwitch, "Off");
    EXPECT_TRUE(gotoAccepted) << "A GoTo after the unpark was refused";
    EXPECT_NE(std::find(commands.begin(), commands.end(), ":MS#"), commands.end()) << "No GoTo reached the mount";
}

// An unpark done outside INDI (hand controller, Pegasus app) must be followed, or the driver refuses motion commands.
TEST(PegasusNYX101Test, unparkOutsideTheDriverIsFollowed)
{
    FakeNyx mount;
    ASSERT_FALSE(mount.portName().empty());
    NYXUnderTest driver;
    ASSERT_TRUE(connectQuietly(driver, mount));
    ASSERT_TRUE(pollQuietly(driver, mount));
    ASSERT_TRUE(driver.parked());

    Published published;
    mount.setStatus(STATUS_TRACKING);
    bool polled = driver.poll();
    bool parked = driver.parked();
    std::string unparkSwitch = lastSwitch(published, "TELESCOPE_PARK", "UNPARK");

    EXPECT_TRUE(polled);
    EXPECT_FALSE(parked) << "The driver still reports parked although the mount reports unparked";
    EXPECT_EQ(unparkSwitch, "On");
}

// When :GU# gets no answer, the status flags are only defaults: the driver must keep its park and tracking state and
// must not publish refraction and tracking mode from them. Takes 3 s, the driver's read timeout.
TEST(PegasusNYX101Test, failedStatusQueryKeepsState)
{
    FakeNyx mount;
    ASSERT_FALSE(mount.portName().empty());
    NYXUnderTest driver;
    ASSERT_TRUE(connectQuietly(driver, mount));
    ASSERT_TRUE(pollQuietly(driver, mount));
    ASSERT_TRUE(driver.trackStateIs(INDI::Telescope::SCOPE_PARKED));

    Published published;
    mount.dropStatusReplies(1);
    driver.poll();
    bool stillParked = driver.trackStateIs(INDI::Telescope::SCOPE_PARKED) && driver.parked();
    std::vector<std::string> eqStates = published.states("setNumberVector", "EQUATORIAL_EOD_COORD");
    std::vector<std::string> refraction = published.states("setSwitchVector", "REFRACTION");
    std::vector<std::string> trackMode = published.states("setSwitchVector", "TELESCOPE_TRACK_MODE");
    std::vector<std::string> commands = mount.takeCommands();

    ASSERT_FALSE(commands.empty());
    EXPECT_EQ(commands.front(), ":GU#");
    EXPECT_TRUE(stillParked) << "A failed status query changed the state of a parked mount";
    for (const auto &state : eqStates)
        EXPECT_EQ(state, "Idle") << "A failed status query reported the parked mount as tracking or slewing";
    EXPECT_TRUE(refraction.empty()) << "Refraction was published from default flags";
    EXPECT_TRUE(trackMode.empty()) << "The tracking mode was published from default flags";
}

// :hR# answers '1'. The driver must read it, or it stays in front of the next reply ("1NpegzEW1:0#").
TEST(PegasusNYX101Test, unparkReplyIsRead)
{
    FakeNyx mount;
    ASSERT_FALSE(mount.portName().empty());
    NYXUnderTest driver;
    ASSERT_TRUE(connectQuietly(driver, mount));
    ASSERT_TRUE(driver.parked());

    Published published;
    mount.setStatus(STATUS_TRACKING);
    bool unparked = driver.requestUnpark();
    bool polled = driver.poll();
    std::vector<std::string> reports = published.values("setTextVector", "Report", "oneText", "Report");
    std::vector<std::string> commands = mount.takeCommands();

    EXPECT_TRUE(unparked);
    EXPECT_TRUE(polled);
    ASSERT_FALSE(commands.empty());
    EXPECT_EQ(commands.front(), ":hR#");
    ASSERT_FALSE(reports.empty()) << "The status after the unpark was not published";
    for (const auto &report : reports)
        EXPECT_EQ(report, STATUS_TRACKING) << "The status reply was read with a leftover byte in front of it";
}

// A park that the mount reports as failed ('F') must end with an error, not stay busy forever.
TEST(PegasusNYX101Test, failedParkIsReported)
{
    FakeNyx mount;
    ASSERT_FALSE(mount.portName().empty());
    mount.setStatus(STATUS_TRACKING);
    NYXUnderTest driver;
    ASSERT_TRUE(connectQuietly(driver, mount));
    ASSERT_TRUE(pollQuietly(driver, mount));
    ASSERT_FALSE(driver.parked());

    Published published;
    bool requested = driver.requestPark();
    mount.setStatus(STATUS_PARK_FAILED);
    bool polled = driver.poll();
    std::vector<std::string> parkStates = published.states("setSwitchVector", "TELESCOPE_PARK");
    std::string unparkSwitch = lastSwitch(published, "TELESCOPE_PARK", "UNPARK");
    bool parking = driver.trackStateIs(INDI::Telescope::SCOPE_PARKING);
    bool parked = driver.parked();
    std::vector<std::string> commands = mount.takeCommands();

    EXPECT_TRUE(requested);
    EXPECT_TRUE(polled);
    ASSERT_FALSE(commands.empty());
    EXPECT_EQ(commands.front(), ":hP#");
    ASSERT_FALSE(parkStates.empty());
    EXPECT_EQ(parkStates.front(), "Busy");
    EXPECT_EQ(parkStates.back(), "Alert") << "The failed park was not reported";
    EXPECT_EQ(unparkSwitch, "On");
    EXPECT_FALSE(parking) << "The driver still waits for the failed park";
    EXPECT_FALSE(parked);
}

// Control for the park checks: a running park ('I', no 'N') stays busy until the mount reports parked.
TEST(PegasusNYX101Test, runningParkStaysBusyUntilParked)
{
    FakeNyx mount;
    ASSERT_FALSE(mount.portName().empty());
    mount.setStatus(STATUS_TRACKING);
    NYXUnderTest driver;
    ASSERT_TRUE(connectQuietly(driver, mount));
    ASSERT_TRUE(pollQuietly(driver, mount));

    Published published;
    bool requested = driver.requestPark();
    mount.setStatus(STATUS_PARKING);
    driver.poll();
    driver.poll();
    std::vector<std::string> whileParking = published.states("setSwitchVector", "TELESCOPE_PARK");
    bool parkedEarly = driver.parked();

    Published afterPark;
    mount.setStatus(STATUS_PARKED);
    driver.poll();
    std::vector<std::string> parkStates = afterPark.states("setSwitchVector", "TELESCOPE_PARK");
    bool parked = driver.parked();

    EXPECT_TRUE(requested);
    ASSERT_FALSE(whileParking.empty());
    for (const auto &state : whileParking)
        EXPECT_EQ(state, "Busy") << "The park ended before the mount reported parked";
    EXPECT_FALSE(parkedEarly);
    ASSERT_FALSE(parkStates.empty());
    EXPECT_EQ(parkStates.back(), "Ok");
    EXPECT_TRUE(parked);
}

// PHD2 reads the guide rate from the standard number property GUIDE_RATE (GUIDE_RATE_WE, GUIDE_RATE_NS). The driver
// reads it from the mount at connect, and connecting writes nothing to the mount.
TEST(PegasusNYX101Test, guideRateIsReadIntoStandardNumberProperty)
{
    FakeNyx mount;
    ASSERT_FALSE(mount.portName().empty());
    // Assumed: the real mount answered "0.50#", which is also the driver's default.
    mount.setGuideRateReply("1.00#");
    NYXUnderTest driver;

    Published published;
    bool connected = driver.connectTo(mount.portName());
    std::vector<std::string> numberDefinitions = published.vectors("defNumberVector", "GUIDE_RATE");
    std::vector<std::string> switchDefinitions = published.vectors("defSwitchVector", "GUIDE_RATE");
    std::vector<std::string> we = published.values("defNumberVector", "GUIDE_RATE", "defNumber", "GUIDE_RATE_WE");
    std::vector<std::string> ns = published.values("defNumberVector", "GUIDE_RATE", "defNumber", "GUIDE_RATE_NS");
    std::vector<std::string> commands = mount.takeCommands();

    ASSERT_TRUE(connected);
    EXPECT_TRUE(switchDefinitions.empty()) << "GUIDE_RATE is defined as a switch";
    ASSERT_FALSE(numberDefinitions.empty()) << "GUIDE_RATE is not defined as a number property";
    ASSERT_FALSE(we.empty());
    ASSERT_FALSE(ns.empty());
    EXPECT_DOUBLE_EQ(std::stod(we.back()), 1.0);
    EXPECT_DOUBLE_EQ(std::stod(ns.back()), 1.0);
    EXPECT_NE(std::find(commands.begin(), commands.end(), ":GX90#"), commands.end());
    for (const auto &command : commands)
        EXPECT_TRUE(command == ACK_COMMAND || command.rfind(":G", 0) == 0) << "Connecting sent " << command;
}

// The NYX-101 has one guide rate for both axes, 0.25, 0.50 or 1.00 (:R0# to :R2#): the changed value is rounded to
// the nearest of them and applied to both axes.
TEST(PegasusNYX101Test, guideRateIsRoundedAndAppliedToBothAxes)
{
    FakeNyx mount;
    ASSERT_FALSE(mount.portName().empty());
    NYXUnderTest driver;
    ASSERT_TRUE(connectQuietly(driver, mount));

    Published published;
    bool first = driver.requestGuideRate(1.0, 0.5);
    std::vector<std::string> firstCommands = mount.takeCommands();
    bool second = driver.requestGuideRate(1.0, 0.3);
    std::vector<std::string> secondCommands = mount.takeCommands();
    std::vector<std::string> states = published.states("setNumberVector", "GUIDE_RATE");
    std::vector<std::string> we = published.values("setNumberVector", "GUIDE_RATE", "oneNumber", "GUIDE_RATE_WE");
    std::vector<std::string> ns = published.values("setNumberVector", "GUIDE_RATE", "oneNumber", "GUIDE_RATE_NS");

    EXPECT_TRUE(first);
    EXPECT_TRUE(second);
    EXPECT_EQ(firstCommands, (std::vector<std::string> {":R2#"}));
    EXPECT_EQ(secondCommands, (std::vector<std::string> {":R0#"}));
    ASSERT_EQ(states.size(), 2u) << "GUIDE_RATE was not answered";
    EXPECT_EQ(states, (std::vector<std::string> {"Ok", "Ok"}));
    ASSERT_EQ(we.size(), 2u);
    ASSERT_EQ(ns.size(), 2u);
    EXPECT_DOUBLE_EQ(std::stod(we[0]), 1.0);
    EXPECT_DOUBLE_EQ(std::stod(ns[0]), 1.0);
    EXPECT_DOUBLE_EQ(std::stod(we[1]), 0.25);
    EXPECT_DOUBLE_EQ(std::stod(ns[1]), 0.25);
}

// Park Options -> Current stores the current position as the park position in the mount (:hQ#).
TEST(PegasusNYX101Test, parkOptionCurrentStoresParkPositionInMount)
{
    FakeNyx mount;
    ASSERT_FALSE(mount.portName().empty());
    mount.setStatus(STATUS_TRACKING);
    NYXUnderTest driver;

    Published published;
    bool connected = driver.connectTo(mount.portName());
    driver.poll();
    mount.takeCommands();
    bool requested = driver.requestParkOption("PARK_CURRENT");
    std::vector<std::string> commands = mount.takeCommands();
    std::vector<std::string> definitions = published.vectors("defSwitchVector", "TELESCOPE_PARK_OPTION");
    std::vector<std::string> states = published.states("setSwitchVector", "TELESCOPE_PARK_OPTION");

    ASSERT_TRUE(connected);
    EXPECT_FALSE(definitions.empty()) << "TELESCOPE_PARK_OPTION is not defined";
    EXPECT_TRUE(requested);
    EXPECT_EQ(commands, (std::vector<std::string> {":hQ#"}));
    ASSERT_FALSE(states.empty());
    EXPECT_EQ(states.back(), "Ok");
}

// The mount has no default park position, so Default sends nothing and fails; a position the mount refuses fails too.
TEST(PegasusNYX101Test, parkOptionDefaultAndRefusedPositionFail)
{
    FakeNyx mount;
    ASSERT_FALSE(mount.portName().empty());
    mount.setStatus(STATUS_TRACKING);
    NYXUnderTest driver;
    ASSERT_TRUE(connectQuietly(driver, mount));

    Published published;
    driver.requestParkOption("PARK_DEFAULT");
    std::vector<std::string> defaultCommands = mount.takeCommands();
    std::vector<std::string> defaultStates = published.states("setSwitchVector", "TELESCOPE_PARK_OPTION");

    Published refused;
    mount.setParkPositionReply("0");
    driver.requestParkOption("PARK_CURRENT");
    std::vector<std::string> refusedCommands = mount.takeCommands();
    std::vector<std::string> refusedStates = refused.states("setSwitchVector", "TELESCOPE_PARK_OPTION");

    EXPECT_TRUE(defaultCommands.empty()) << "Default sent " << defaultCommands.front();
    ASSERT_FALSE(defaultStates.empty());
    EXPECT_EQ(defaultStates.back(), "Alert");
    EXPECT_EQ(refusedCommands, (std::vector<std::string> {":hQ#"}));
    ASSERT_FALSE(refusedStates.empty());
    EXPECT_EQ(refusedStates.back(), "Alert") << "A park position refused by the mount was reported as stored";
}

// With PARK_SIMPLE the park position property is never filled. Defining it sent clients a number vector without
// device and name.
TEST(PegasusNYX101Test, parkPositionPropertyIsNotDefined)
{
    FakeNyx mount;
    ASSERT_FALSE(mount.portName().empty());
    NYXUnderTest driver;

    Published published;
    bool connected = driver.connectTo(mount.portName());
    std::vector<std::string> definitions = published.vectors("defNumberVector", "TELESCOPE_PARK_POSITION");
    std::vector<std::string> nameless = published.vectors("defNumberVector", "");
    mount.takeCommands();

    ASSERT_TRUE(connected);
    EXPECT_TRUE(definitions.empty()) << "TELESCOPE_PARK_POSITION is defined";
    EXPECT_TRUE(nameless.empty()) << "A number property without a name is defined";
}

// The first status after :MS# can still report no slew ('N'). The GoTo must stay busy until the mount has reported
// slewing and then stopped: Ekos takes its alignment image when the GoTo is complete.
TEST(PegasusNYX101Test, gotoIsCompleteOnlyAfterTheMountReportedSlewing)
{
    FakeNyx mount;
    ASSERT_FALSE(mount.portName().empty());
    mount.setStatus(STATUS_TRACKING);
    NYXUnderTest driver;
    ASSERT_TRUE(connectQuietly(driver, mount));
    ASSERT_TRUE(pollQuietly(driver, mount));

    Published published;
    bool accepted = driver.requestGoto(TARGET_RA, TARGET_DEC);
    driver.poll();
    driver.poll();
    mount.setStatus(STATUS_SLEWING);
    driver.poll();
    std::vector<std::string> beforeEnd = published.states("setNumberVector", "EQUATORIAL_EOD_COORD");
    bool slewingBeforeEnd = driver.trackStateIs(INDI::Telescope::SCOPE_SLEWING);
    std::vector<std::string> commands = mount.takeCommands();

    Published atEnd;
    mount.setStatus(STATUS_TRACKING);
    driver.poll();
    std::vector<std::string> afterEnd = atEnd.states("setNumberVector", "EQUATORIAL_EOD_COORD");
    bool tracking = driver.trackStateIs(INDI::Telescope::SCOPE_TRACKING);

    EXPECT_TRUE(accepted);
    EXPECT_NE(std::find(commands.begin(), commands.end(), ":MS#"), commands.end());
    ASSERT_FALSE(beforeEnd.empty());
    for (const auto &state : beforeEnd)
        EXPECT_EQ(state, "Busy") << "The GoTo was reported complete before the mount had moved";
    EXPECT_TRUE(slewingBeforeEnd);
    ASSERT_FALSE(afterEnd.empty());
    EXPECT_EQ(afterEnd.back(), "Ok");
    EXPECT_TRUE(tracking);
}

// A short GoTo can end between two polls, so the mount never reports slewing. It must still complete, after
// SLEW_START_TIMEOUT (3 s). Takes about 3 s.
TEST(PegasusNYX101Test, gotoWithoutReportedSlewCompletesAfterTimeout)
{
    FakeNyx mount;
    ASSERT_FALSE(mount.portName().empty());
    mount.setStatus(STATUS_TRACKING);
    NYXUnderTest driver;
    ASSERT_TRUE(connectQuietly(driver, mount));
    ASSERT_TRUE(pollQuietly(driver, mount));

    Published published;
    const auto start = std::chrono::steady_clock::now();
    bool accepted = driver.requestGoto(TARGET_RA, TARGET_DEC);
    double completedAfter = -1;
    while (std::chrono::steady_clock::now() - start < std::chrono::seconds(6))
    {
        driver.poll();
        if (driver.trackStateIs(INDI::Telescope::SCOPE_TRACKING))
        {
            completedAfter = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    std::vector<std::string> states = published.states("setNumberVector", "EQUATORIAL_EOD_COORD");

    EXPECT_TRUE(accepted);
    ASSERT_GE(completedAfter, 0) << "The GoTo never completed";
    EXPECT_GE(completedAfter, 3.0) << "The GoTo was reported complete before the mount could start";
    EXPECT_LT(completedAfter, 4.0);
    ASSERT_FALSE(states.empty());
    EXPECT_EQ(states.back(), "Ok");
}

// The pulse guide command has four digits for the duration; a longer pulse is clamped instead of sending :MgnDDDDD#.
TEST(PegasusNYX101Test, longGuidePulseIsClamped)
{
    FakeNyx mount;
    ASSERT_FALSE(mount.portName().empty());
    mount.setStatus(STATUS_TRACKING);
    NYXUnderTest driver;
    ASSERT_TRUE(connectQuietly(driver, mount));
    ASSERT_TRUE(pollQuietly(driver, mount));

    Published published;
    driver.requestGuidePulseNorth(12000);
    std::vector<std::string> commands = mount.takeCommands();
    published.stop();

    EXPECT_EQ(commands, (std::vector<std::string> {":Mgn9999#"}));
}

// The Report property must keep its own copy of the status text. With the text pointing at the local buffer of the
// poll, a client asking for the property between two polls gets whatever has been written to that stack memory since.
TEST(PegasusNYX101Test, reportTextIsKeptAfterThePoll)
{
    FakeNyx mount;
    ASSERT_FALSE(mount.portName().empty());
    mount.setStatus(STATUS_TRACKING);
    NYXUnderTest driver;
    ASSERT_TRUE(connectQuietly(driver, mount));

    Published published;
    bool polled = driver.poll();
    overwriteStack();
    std::string report = driver.reportText();
    published.stop();

    EXPECT_TRUE(polled);
    EXPECT_EQ(report, STATUS_TRACKING) << "The Report text changed after the poll had returned";
}

// The destructor of the Report property frees its text. That must be the heap copy the property owns, not a stack
// address left by a poll (the C library aborts on it). Runs in a child process so the other tests survive a crash.
TEST(PegasusNYX101DeathTest, reportPropertyIsFreedCleanly)
{
    EXPECT_EXIT(
    {
        {
            FakeNyx mount;
            mount.setStatus(STATUS_TRACKING);
            NYXUnderTest driver;
            if (!connectQuietly(driver, mount) || !pollQuietly(driver, mount))
                std::_Exit(2);
        }
        std::_Exit(0);
    }, ::testing::ExitedWithCode(0), "");
}

int main(int argc, char **argv)
{
    // Never touch the user's real ~/.indi (park data is written to $HOME/.indi).
    char homeTemplate[] = "/tmp/test_lx200_pegasus_nyx101_home_XXXXXX";
    const char *home = mkdtemp(homeTemplate);
    if (home)
        setenv("HOME", home, 1);
    // Never read or write the user's real driver configuration.
    const std::string configFile = "/tmp/test_lx200_pegasus_nyx101_" + std::to_string(getpid()) + "_config.xml";
    setenv("INDICONFIG", configFile.c_str(), 1);
    INDI::Logger::getInstance().configure("", INDI::Logger::file_off,
                                          INDI::Logger::DBG_ERROR, INDI::Logger::DBG_ERROR);
    ::testing::InitGoogleTest(&argc, argv);
    int rc = RUN_ALL_TESTS();
    std::remove(configFile.c_str());
    std::remove((configFile + ".default").c_str());
    if (home)
    {
        const std::string parkData = std::string(home) + "/.indi/ParkData.xml";
        std::remove(parkData.c_str());
        std::remove((std::string(home) + "/.indi").c_str());
        std::remove(home);
    }
    return rc;
}
