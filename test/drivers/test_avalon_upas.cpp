/*
    Regression tests for the Avalon UPAS driver (GRBL 1.1 over USB):
    - GRBL replies are read up to their final "ok", so a command right after connect gets its own reply and not a
      leftover line of the "$$" settings listing.
    - The status query '?' is a real-time command and is sent without a newline; GRBL answers a newline as an empty
      line with an extra "ok".
    - A feed hold ends a move with an alert instead of keeping it busy, and Abort then sends a soft reset (GRBL
      ignores the jog cancel during a hold).
    - An alarm is reported and can be cleared with PAC_UNLOCK ($X).

    The driver talks to a fake GRBL on a pseudo terminal. Like GRBL, it takes '?', 0x18 and 0x85 as real-time bytes
    from anywhere in the stream, answers line commands in order, answers every line, also an empty one, and sends its
    output at the pace of 115200 baud. The status, "$I" and "$$" replies are copied from a debug log of a real UPAS
    (GRBL 1.1h.20190825); the banner, the hold and alarm states and the "$X" reply follow the GRBL 1.1 documentation,
    as the device was not seen in those states.

    SPDX-License-Identifier: LGPL-2.1-or-later
*/

#include <gtest/gtest.h>

#include <atomic>
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
#include <regex>
#include <string>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <termios.h>
#include <thread>
#include <unistd.h>
#include <vector>

#include "indilogger.h"
#include "drivers/auxiliary/avalon_upas.h"

namespace
{

const char *DEVICE_NAME = "Mock Avalon UPAS";

// "$$" of a real UPAS (GRBL 1.1h.20190825).
const char *SETTINGS[] =
{
    "$0=10", "$1=25", "$2=0", "$3=0", "$4=0", "$5=0", "$6=0", "$10=1", "$11=0.010", "$12=0.002", "$13=0", "$20=0",
    "$21=0", "$22=0", "$23=0", "$24=25.000", "$25=600.000", "$26=250", "$27=5.000", "$30=1000", "$31=0", "$32=0",
    "$100=200.000", "$101=200.000", "$102=200.000", "$110=2500.000", "$111=2500.000", "$112=2500.000",
    "$120=50.000", "$121=50.000", "$122=90.000", "$130=9000.000", "$131=8000.000", "$132=200.000"
};

class FakeGrbl
{
    public:
        FakeGrbl()
        {
            char name[128] = {0};
            if (openpty(&m_Master, &m_Slave, name, nullptr, nullptr) != 0)
                return;
            makeRaw(m_Master);
            makeRaw(m_Slave);
            m_PortName = name;
            m_Receiver = std::thread(&FakeGrbl::receive, this);
            m_Processor = std::thread(&FakeGrbl::process, this);
        }

        ~FakeGrbl()
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

        // State field of the status report, e.g. "Idle", "Jog", "Hold:0" or "Alarm".
        void setState(const std::string &state)
        {
            std::lock_guard<std::mutex> lock(m_Mutex);
            m_State = state;
        }

        std::string state()
        {
            std::lock_guard<std::mutex> lock(m_Mutex);
            return m_State;
        }

        // Line commands received since the last call, with "" for an empty line. Waits until every byte received so
        // far is handled, so commands still queued behind a long reply are included.
        std::vector<std::string> takeLines()
        {
            waitUntilIdle();
            std::lock_guard<std::mutex> lock(m_Mutex);
            std::vector<std::string> lines;
            lines.swap(m_Lines);
            return lines;
        }

        // Real-time bytes other than '?' received since the last call.
        std::vector<int> takeRealtime()
        {
            waitUntilIdle();
            std::lock_guard<std::mutex> lock(m_Mutex);
            std::vector<int> bytes;
            bytes.swap(m_Realtime);
            return bytes;
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

        // Output at 115200 baud: 10 bits per byte.
        void send(const std::string &line)
        {
            std::string text = line + "\r\n";
            if (write(m_Master, text.data(), text.size()) < 0)
                return;
            std::this_thread::sleep_for(std::chrono::microseconds(text.size() * 87));
        }

        std::string status()
        {
            std::lock_guard<std::mutex> lock(m_Mutex);
            return "<" + m_State + "|MPos:0.000,0.000,0.000|FS:0,0>";
        }

        // Takes every byte off the line as soon as it arrives, like GRBL's serial receive interrupt, so a flush on the
        // driver's side never discards what the device has already received.
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
            std::string line;
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

                for (const char byte : bytes)
                {
                    const unsigned char c = byte;
                    if (c == '?')
                        send(status());
                    else if (c == 0x18 || c == 0x85)
                        realtime(c);
                    else if (c == '\n')
                    {
                        if (!line.empty() && line.back() == '\r')
                            line.pop_back();
                        command(line);
                        line.clear();
                    }
                    else
                        line += static_cast<char>(c);
                }

                std::lock_guard<std::mutex> lock(m_RxMutex);
                if (m_Rx.empty())
                    m_Busy = false;
            }
        }

        void realtime(unsigned char c)
        {
            {
                std::lock_guard<std::mutex> lock(m_Mutex);
                m_Realtime.push_back(c);
                // Jog cancel ends a running jog; a soft reset ends any move.
                if (m_State == "Jog" || (c == 0x18 && m_State.rfind("Hold", 0) == 0))
                    m_State = "Idle";
            }
            if (c == 0x18)
            {
                send("");
                send("Grbl 1.1h ['$' for help]");
            }
        }

        void command(const std::string &cmd)
        {
            {
                std::lock_guard<std::mutex> lock(m_Mutex);
                m_Lines.push_back(cmd);
            }

            if (cmd == "$I")
            {
                send("[VER:1.1h.20190825:]");
                send("[OPT:V,15,128]");
            }
            else if (cmd == "$$")
            {
                for (const char *setting : SETTINGS)
                    send(setting);
            }
            else if (cmd == "$X")
            {
                setState("Idle");
                send("[MSG:Caution: Unlocked]");
            }
            else if (cmd.rfind("$J=", 0) == 0)
                setState("Jog");
            send("ok");
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
        std::string m_State {"Idle"};
        std::vector<std::string> m_Lines;
        std::vector<int> m_Realtime;
};

class UPASUnderTest : public AvalonUPAS
{
    public:
        UPASUnderTest()
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

            return pressSwitch("CONNECTION", "CONNECT", "DISCONNECT") && isConnected();
        }

        // Move both axes by the given degrees, as a client does it.
        bool requestMove(double azDegrees, double altDegrees)
        {
            double values[2] = {azDegrees, altDegrees};
            char az[] = "MANUAL_AZ_STEP", alt[] = "MANUAL_ALT_STEP";
            char *names[2] = {az, alt};
            return ISNewNumber(getDeviceName(), "PAC_MANUAL_ADJUSTMENT", values, names, 2);
        }

        bool requestAbort()
        {
            return pressSwitch("PAC_ABORT_MOTION", "ABORT");
        }

        bool requestUnlock()
        {
            return pressSwitch("PAC_UNLOCK", "UNLOCK");
        }

        // One poll, as the polling timer runs it.
        void poll()
        {
            TimerHit();
        }

    private:
        bool pressSwitch(const char *property, const char *on, const char *off = nullptr)
        {
            std::string onName = on, offName = off ? off : "";
            ISState states[2] = {ISS_ON, ISS_OFF};
            char *names[2] = {&onName[0], &offName[0]};
            return ISNewSwitch(getDeviceName(), property, states, names, off ? 2 : 1);
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

        // States of the updates of a property, e.g. ("setNumberVector", "PAC_MANUAL_ADJUSTMENT").
        std::vector<std::string> states(const std::string &vector, const std::string &property)
        {
            std::string text = read();
            std::vector<std::string> states;
            std::regex update("<" + vector + "[^>]*name=['\"]" + property + "['\"][^>]*state=['\"](\\w+)['\"]");
            for (auto it = std::sregex_iterator(text.begin(), text.end(), update); it != std::sregex_iterator(); ++it)
                states.push_back((*it)[1]);
            return states;
        }

        // Last published value of a text element.
        std::string text(const std::string &element)
        {
            std::string text = read();
            std::regex value("<(?:def|one)Text[^>]*name=['\"]" + element + "['\"][^>]*>\\s*([^<]*?)\\s*</");
            std::string last;
            for (auto it = std::sregex_iterator(text.begin(), text.end(), value); it != std::sregex_iterator(); ++it)
                last = (*it)[1];
            return last;
        }

    private:
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

// 1 arcmin in degrees; with the default gear ratios a jog of 2 mm (AZ) and 22 mm (ALT).
const double ONE_ARCMIN = 1.0 / 60;

}

// A move sent right after connect, as a client may do (Ekos loads the configuration right after CONNECT), gets the
// "ok" to its jog and not a line of the "$$" listing that was still arriving when the driver stopped reading it.
TEST(AvalonUPASTest, moveRightAfterConnectIsAccepted)
{
    FakeGrbl grbl;
    ASSERT_FALSE(grbl.portName().empty());
    UPASUnderTest driver;

    Published published;
    bool connected = driver.connectTo(grbl.portName());
    bool accepted = driver.requestMove(ONE_ARCMIN, 0);
    std::vector<std::string> moveStates = published.states("setNumberVector", "PAC_MANUAL_ADJUSTMENT");
    std::string firmware = published.text("VERSION");
    std::vector<std::string> lines = grbl.takeLines();

    ASSERT_TRUE(connected);
    EXPECT_TRUE(accepted);
    EXPECT_EQ(firmware, "1.1h.20190825");
    EXPECT_EQ(lines, (std::vector<std::string> {"$I", "$$", "$J=G91G21X2.0000 F1000"}));
    ASSERT_FALSE(moveStates.empty()) << "The move was never answered";
    EXPECT_EQ(moveStates.back(), "Busy") << "The jog right after connect failed";
}

// Status polls send only '?': a newline after it would be an empty line to GRBL, answered with an extra "ok" that the
// next command then reads as its reply.
TEST(AvalonUPASTest, statusQueryIsSentWithoutNewline)
{
    FakeGrbl grbl;
    ASSERT_FALSE(grbl.portName().empty());
    UPASUnderTest driver;
    ASSERT_TRUE(driver.connectTo(grbl.portName()));
    grbl.takeLines();

    for (int i = 0; i < 5; i++)
        driver.poll();
    std::vector<std::string> lines = grbl.takeLines();

    EXPECT_TRUE(lines.empty()) << "The status polls sent " << lines.size() << " lines besides '?'";

    // The next command still gets its own reply.
    Published published;
    driver.requestMove(ONE_ARCMIN, 0);
    std::vector<std::string> moveStates = published.states("setNumberVector", "PAC_MANUAL_ADJUSTMENT");
    ASSERT_FALSE(moveStates.empty());
    EXPECT_EQ(moveStates.back(), "Busy");
}

// A feed hold pauses the jog until a cycle start, which the driver never sends: the move ends with an alert instead
// of staying busy. Abort then sends a soft reset (0x18) instead of the jog cancel (0x85) that GRBL ignores in a hold,
// and the driver reads GRBL's banner so it is not taken as the reply to the next command.
TEST(AvalonUPASTest, heldMoveEndsWithAlertAndAbortSendsSoftReset)
{
    FakeGrbl grbl;
    ASSERT_FALSE(grbl.portName().empty());
    UPASUnderTest driver;
    ASSERT_TRUE(driver.connectTo(grbl.portName()));
    grbl.takeRealtime();

    Published published;
    driver.requestMove(ONE_ARCMIN, 0);
    driver.poll();
    grbl.setState("Hold:0");
    driver.poll();
    std::vector<std::string> moveStates = published.states("setNumberVector", "PAC_MANUAL_ADJUSTMENT");

    bool aborted = driver.requestAbort();
    std::vector<int> realtime = grbl.takeRealtime();
    grbl.takeLines();
    Published afterAbort;
    driver.poll();
    driver.requestMove(ONE_ARCMIN, 0);
    std::vector<std::string> positionStates = afterAbort.states("setNumberVector", "PAC_POSITION");
    std::vector<std::string> nextMove = afterAbort.states("setNumberVector", "PAC_MANUAL_ADJUSTMENT");

    ASSERT_FALSE(moveStates.empty());
    EXPECT_EQ(moveStates.back(), "Alert") << "A held move stayed busy";
    EXPECT_TRUE(aborted);
    EXPECT_EQ(realtime, (std::vector<int> {0x18})) << "Abort during a hold did not send a soft reset";
    ASSERT_FALSE(positionStates.empty());
    EXPECT_EQ(positionStates.front(), "Ok") << "The poll after the abort did not read a status report";
    ASSERT_FALSE(nextMove.empty());
    EXPECT_EQ(nextMove.back(), "Busy") << "The move after the abort failed";
}

// An alarm is reported (position and move in alert) and PAC_UNLOCK clears it with $X, without a power cycle.
TEST(AvalonUPASTest, alarmIsReportedAndCanBeUnlocked)
{
    FakeGrbl grbl;
    ASSERT_FALSE(grbl.portName().empty());
    UPASUnderTest driver;
    ASSERT_TRUE(driver.connectTo(grbl.portName()));
    grbl.takeLines();

    Published published;
    grbl.setState("Alarm");
    driver.poll();
    std::vector<std::string> inAlarm = published.states("setNumberVector", "PAC_POSITION");

    Published afterUnlock;
    bool unlocked = driver.requestUnlock();
    std::vector<std::string> lines = grbl.takeLines();
    std::vector<std::string> unlockStates = afterUnlock.states("setSwitchVector", "PAC_UNLOCK");
    std::vector<std::string> positionStates = afterUnlock.states("setNumberVector", "PAC_POSITION");

    ASSERT_FALSE(inAlarm.empty());
    EXPECT_EQ(inAlarm.back(), "Alert") << "The alarm was not reported";
    EXPECT_TRUE(unlocked) << "No PAC_UNLOCK switch";
    EXPECT_EQ(lines, (std::vector<std::string> {"$X"}));
    EXPECT_EQ(grbl.state(), "Idle");
    ASSERT_FALSE(unlockStates.empty());
    EXPECT_EQ(unlockStates.back(), "Ok");
    ASSERT_FALSE(positionStates.empty()) << "The state after the unlock was not published";
    EXPECT_EQ(positionStates.back(), "Ok");
}

int main(int argc, char **argv)
{
    // Never touch the user's real ~/.indi.
    char homeTemplate[] = "/tmp/test_avalon_upas_home_XXXXXX";
    const char *home = mkdtemp(homeTemplate);
    if (home)
        setenv("HOME", home, 1);
    // Never read or write the user's real driver configuration.
    const std::string configFile = "/tmp/test_avalon_upas_" + std::to_string(getpid()) + "_config.xml";
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
