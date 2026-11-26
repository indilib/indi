/*
    Regression tests for the Pegasus Ultimate Powerbox (UPB v2) driver:
    - #2539: a focuser move to the current position completes, and a move is complete only when the motor stands at
      the target (not at the first poll after SM:, before the motor started); one that stops short ends with Alert.
    - #2537: the duty cycle set for a dew channel is kept while the device reports 0 for it; settings stored in the
      device are neither saved in nor loaded from the configuration; enabling the backlash sends the configured steps;
      the replies to SB:n and SS:n are read; the backlash steps from the device are shown; port states are published
      again after a reconnection; the adjustable output cannot be switched off and its voltage is limited to 3-12 V;
      a garbled reply does not end the driver; the stepper data is compared with the last stepper data; Power On Boot,
      Over Current and the USB labels are published again; switching the LED off warns about the stepper output.

    The driver talks to a fake UPB v2 on a pseudo terminal. Sources of its replies:
    - LOG: debug log of a real UPB v2 (revision B, firmware 2.4) with this driver: P#, PA, PC, SA, DA, PS, PV, SS,
      SB:0, SR:0 and PD:n.
    - PR: observed on the same device in the hardware tests of #2537 and #2539: P8:0 is answered with the stored
      voltage (P8:12); SB:n and SS:n are answered (a late SB:0 was read as the reply to P8:5). P5:n, P8:n, Un:b and
      SM:n passed the driver's reply check there, so they are answered as the driver expects.
    - ASSUMED: only the driver's reply check or parser is known: the exact SS:n reply, PE:bbbb, PL:b, P1-P4, SA while
      the motor runs, the garbled status reply, and the reply delay of 50 ms in backlashAndSpeedRepliesAreRead.

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <gtest/gtest.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <fcntl.h>
#include <fstream>
#include <mutex>
#if defined(__APPLE__) || defined(__OpenBSD__) || defined(__NetBSD__)
#include <util.h>
#elif defined(__FreeBSD__)
#include <libutil.h>
#else
#include <pty.h>
#endif
#include <sstream>
#include <string>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <termios.h>
#include <thread>
#include <unistd.h>
#include <vector>

#include "indilogger.h"
#include "drivers/power/pegasus_upb.h"

namespace
{

const char *DEVICE_NAME = "Mock UPB";
std::string g_ConfigFile;

// Fake UPB v2: takes every byte off the line at once in a receiver thread and handles the commands in a second thread,
// so a tcflush() on the driver's side never discards what the device has already received.
class FakeUPB
{
    public:
        FakeUPB()
        {
            char name[128] = {0};
            if (openpty(&m_Master, &m_Slave, name, nullptr, nullptr) != 0)
                return;
            makeRaw(m_Master);
            makeRaw(m_Slave);
            m_PortName = name;
            m_Receiver = std::thread(&FakeUPB::receive, this);
            m_Processor = std::thread(&FakeUPB::process, this);
        }

        ~FakeUPB()
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

        // tty_connect() puts the port in exclusive mode (TIOCEXCL) and tty_disconnect() does not clear it. A real
        // serial port is closed completely after a disconnection, which clears it; here the fake keeps its own end
        // open, so on Linux a second connection would be refused with EBUSY. Clear it as a closed port would be.
        void releasePort()
        {
            ioctl(m_Slave, TIOCNXCL);
        }

        // Focuser position and motor state as reported by SA. A move (SM:n) only sets the target; the test moves the
        // motor between polls.
        void setStepper(int position, bool running)
        {
            std::lock_guard<std::mutex> lock(m_Mutex);
            m_Position = position;
            m_Running = running;
        }

        void setBacklash(int steps)
        {
            std::lock_guard<std::mutex> lock(m_Mutex);
            m_Backlash = steps;
        }

        void setReverse(bool reverse)
        {
            std::lock_guard<std::mutex> lock(m_Mutex);
            m_Reverse = reverse;
        }

        // Over current flags of PA: ports 1-4, then dew A-C.
        void setOverCurrent(const std::string &flags)
        {
            std::lock_guard<std::mutex> lock(m_Mutex);
            m_OverCurrent = flags;
        }

        // The next PA reply carries a field that is not a number, as after a transmission error.
        void garbleNextStatus()
        {
            std::lock_guard<std::mutex> lock(m_Mutex);
            m_Garble = true;
        }

        // Time the device takes before it answers a command.
        void setReplyDelay(std::chrono::milliseconds delay)
        {
            std::lock_guard<std::mutex> lock(m_Mutex);
            m_ReplyDelay = delay;
        }

        // Commands received since the last call. Waits until every byte received so far is handled.
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

                for (const char c : bytes)
                {
                    if (c == '\n')
                    {
                        if (!line.empty() && line.back() == '\r')
                            line.pop_back();
                        if (!line.empty())
                            command(line);
                        line.clear();
                    }
                    else
                        line += c;
                }

                std::lock_guard<std::mutex> lock(m_RxMutex);
                if (m_Rx.empty())
                    m_Busy = false;
            }
        }

        // PA of the real device (LOG) with the port, dew, over current and auto dew fields of this fake. The current
        // of port 1 and 2 alternates between two logged readings, so consecutive replies differ as on the device.
        std::string status()
        {
            std::ostringstream reply;
            const bool even = (m_StatusCount++ % 2) == 0;
            reply << "UPB2:12.8:1.1:14:" << (m_Garble ? "%4.0" : "24.0") << ":37:8.4:" << m_Ports << ":" << m_USB << ":"
                  << m_Dew[0] << ":" << m_Dew[1] << ":" << m_Dew[2] << ":" << (even ? "134:381" : "129:375")
                  << ":0:0:0:0:0:" << m_OverCurrent << ":" << m_AutoDew;
            m_Garble = false;
            return reply.str();
        }

        static bool hasPrefix(const std::string &text, const std::string &prefix)
        {
            return text.compare(0, prefix.size(), prefix) == 0;
        }

        void command(const std::string &cmd)
        {
            std::string reply;
            std::chrono::milliseconds delay;
            {
                std::lock_guard<std::mutex> lock(m_Mutex);
                m_Commands.push_back(cmd);
                delay = m_ReplyDelay;
                reply = answer(cmd);
            }
            if (reply.empty())
                return;
            if (delay.count() > 0)
                std::this_thread::sleep_for(delay);
            reply += "\r\n";
            if (write(m_Master, reply.data(), reply.size()) < 0)
                return;
        }

        // Called with m_Mutex held.
        std::string answer(const std::string &cmd)
        {
            const std::string value = cmd.size() > 3 ? cmd.substr(3) : "";
            // LOG
            if (cmd == "P#")
                return "UPB2_OK";
            if (cmd == "PA")
                return status();
            if (cmd == "PC")
                return "1.36:4.86:61.92:12837169";
            if (cmd == "SA")
                return std::to_string(m_Position) + ":" + (m_Running ? "1" : "0") + ":" + (m_Reverse ? "1" : "0") + ":" +
                       std::to_string(m_Backlash);
            if (cmd == "DA")
                return "DA:" + std::to_string(m_DewAgg);
            if (cmd == "PS")
                return "PS:" + m_PowerOnBoot + ":" + std::to_string(m_Voltage);
            if (cmd == "PV")
                return "2.4";
            if (cmd == "SS")
                return std::to_string(m_MaxSpeed);
            // LOG (SB:0 -> SB:0); PR: SB:n is answered.
            if (hasPrefix(cmd, "SB:"))
            {
                m_Backlash = std::stoi(value);
                return cmd;
            }
            // LOG
            if (hasPrefix(cmd, "SR:"))
            {
                m_Reverse = value == "1";
                return cmd;
            }
            // PR (moves in #2539 passed the reply check): answered with the command. The test moves the motor.
            if (hasPrefix(cmd, "SM:"))
                return cmd;
            // PR: answered with the new speed. ASSUMED: the speed alone, as for the SS query.
            if (hasPrefix(cmd, "SS:"))
            {
                m_MaxSpeed = std::stoi(value);
                return std::to_string(m_MaxSpeed);
            }
            // LOG (PD:83 -> PD:83). Three digits set the aggressiveness, one digit the auto dew channels.
            if (hasPrefix(cmd, "PD:"))
            {
                if (value.size() == 3)
                    m_DewAgg = std::stoi(value);
                else
                    m_AutoDew = value;
                return "PD:" + std::to_string(std::stoi(value));
            }
            // PR (P5:127 and P5:005 passed the reply check): the duty cycle without leading zeros.
            if (cmd.size() > 3 && cmd[0] == 'P' && cmd[1] >= '5' && cmd[1] <= '7' && cmd[2] == ':')
            {
                m_Dew[cmd[1] - '5'] = std::stoi(value);
                return cmd.substr(0, 3) + std::to_string(m_Dew[cmd[1] - '5']);
            }
            // PR: P8:0 is answered with the stored voltage, P8:n with P8:n.
            if (hasPrefix(cmd, "P8:"))
            {
                if (std::stoi(value) > 0)
                    m_Voltage = std::stoi(value);
                return "P8:" + std::to_string(m_Voltage);
            }
            // ASSUMED: the 12 V ports answer with the command.
            if (cmd.size() == 4 && cmd[0] == 'P' && cmd[1] >= '1' && cmd[1] <= '4' && cmd[2] == ':')
            {
                m_Ports[cmd[1] - '1'] = cmd[3];
                return cmd;
            }
            // PR (USB port 6 off and on passed the reply check): answered with the command.
            if (cmd.size() == 4 && cmd[0] == 'U' && cmd[1] >= '1' && cmd[1] <= '6' && cmd[2] == ':')
            {
                m_USB[cmd[1] - '1'] = cmd[3];
                return cmd;
            }
            // ASSUMED: as the driver checks it.
            if (hasPrefix(cmd, "PE:"))
            {
                m_PowerOnBoot = value;
                return "PE:1";
            }
            // ASSUMED: answered with the command.
            if (hasPrefix(cmd, "PL:"))
                return cmd;
            return "";
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
        std::vector<std::string> m_Commands;
        std::chrono::milliseconds m_ReplyDelay {0};
        // Device state; the initial values are those of the real device in the log.
        int m_Position {40000};
        bool m_Running {false};
        bool m_Reverse {false};
        int m_Backlash {0};
        int m_MaxSpeed {400};
        int m_DewAgg {150};
        int m_Voltage {12};
        int m_Dew[3] {0, 0, 0};
        std::string m_Ports {"1111"};
        std::string m_USB {"111111"};
        std::string m_PowerOnBoot {"1111"};
        std::string m_OverCurrent {"0000000"};
        std::string m_AutoDew {"0"};
        unsigned m_StatusCount {0};
        bool m_Garble {false};
};

class UPBUnderTest : public PegasusUPB
{
    public:
        UPBUnderTest()
        {
            setDeviceName(DEVICE_NAME);
            // As at a client's first getProperties; this also runs initProperties().
            ISGetProperties(nullptr);
        }

        // Connect the way a client does it: port, then CONNECT. Auto search is switched off so a failed connection
        // doesn't scan the real serial ports of the machine running the test.
        bool connectTo(const std::string &port)
        {
            setSwitch("DEVICE_AUTO_SEARCH", {{"INDI_ENABLED", ISS_OFF}, {"INDI_DISABLED", ISS_ON}});
            setText("DEVICE_PORT", "PORT", port);
            setSwitch("CONNECTION", {{"CONNECT", ISS_ON}, {"DISCONNECT", ISS_OFF}});
            return isConnected();
        }

        bool disconnectNow()
        {
            setSwitch("CONNECTION", {{"CONNECT", ISS_OFF}, {"DISCONNECT", ISS_ON}});
            return !isConnected();
        }

        void setSwitch(const char *property, const std::vector<std::pair<std::string, ISState>> &elements)
        {
            std::vector<std::string> names;
            std::vector<ISState> states;
            for (const auto &element : elements)
            {
                names.push_back(element.first);
                states.push_back(element.second);
            }
            std::vector<char *> namePointers;
            for (auto &name : names)
                namePointers.push_back(&name[0]);
            ISNewSwitch(getDeviceName(), property, states.data(), namePointers.data(), static_cast<int>(names.size()));
        }

        void setNumber(const char *property, const char *element, double value)
        {
            std::string name = element;
            char *names[1] = {&name[0]};
            double values[1] = {value};
            ISNewNumber(getDeviceName(), property, values, names, 1);
        }

        void setText(const char *property, const char *element, const std::string &value)
        {
            std::string name = element, text = value;
            char *names[1] = {&name[0]};
            char *texts[1] = {&text[0]};
            ISNewText(getDeviceName(), property, texts, names, 1);
        }

        // One poll, as the polling timer runs it.
        void poll()
        {
            TimerHit();
        }
};

// One property vector as published by the driver (def... or set...).
struct Vector
{
    std::string kind;
    std::string type;
    std::string name;
    std::string device;
    std::string state;
    std::vector<std::pair<std::string, std::string>> labels;
    std::vector<std::pair<std::string, std::string>> values;

    std::string value(const std::string &element) const
    {
        for (const auto &one : values)
            if (one.first == element)
                return one.second;
        return "";
    }

    std::string label(const std::string &element) const
    {
        for (const auto &one : labels)
            if (one.first == element)
                return one.second;
        return "";
    }
};

std::string attribute(const std::string &tag, const std::string &name)
{
    const std::string key = " " + name + "=";
    size_t start = tag.find(key);
    if (start == std::string::npos)
        return "";
    start += key.size();
    const char quote = tag[start];
    const size_t end = tag.find(quote, start + 1);
    return tag.substr(start + 1, end - start - 1);
}

std::string trim(const std::string &text)
{
    const size_t first = text.find_first_not_of(" \t\r\n");
    if (first == std::string::npos)
        return "";
    return text.substr(first, text.find_last_not_of(" \t\r\n") - first + 1);
}

// The def...Vector and set...Vector elements in the published XML, in order.
std::vector<Vector> parseVectors(const std::string &xml)
{
    std::vector<Vector> vectors;
    size_t pos = 0;
    while ((pos = xml.find('<', pos)) != std::string::npos)
    {
        const size_t nameEnd = xml.find_first_of(" \t\r\n/>", pos + 1);
        const std::string tagName = xml.substr(pos + 1, nameEnd - pos - 1);
        const bool isVector = (tagName.rfind("def", 0) == 0 || tagName.rfind("set", 0) == 0) && tagName.size() > 9 &&
                              tagName.compare(tagName.size() - 6, 6, "Vector") == 0;
        if (!isVector)
        {
            pos++;
            continue;
        }
        const size_t tagEnd = xml.find('>', pos);
        const size_t close = xml.find("</" + tagName + ">", tagEnd);
        if (tagEnd == std::string::npos || close == std::string::npos)
            break;

        const std::string tag = xml.substr(pos, tagEnd - pos);
        Vector vector;
        vector.kind = tagName.substr(0, 3);
        vector.type = tagName.substr(3, tagName.size() - 9);
        vector.name = attribute(tag, "name");
        vector.device = attribute(tag, "device");
        vector.state = attribute(tag, "state");

        const std::string body = xml.substr(tagEnd + 1, close - tagEnd - 1);
        size_t element = 0;
        while ((element = body.find('<', element)) != std::string::npos)
        {
            const size_t elementTagEnd = body.find('>', element);
            const size_t valueEnd = body.find("</", elementTagEnd);
            if (elementTagEnd == std::string::npos || valueEnd == std::string::npos)
                break;
            const std::string elementTag = body.substr(element, elementTagEnd - element);
            const std::string name = attribute(elementTag, "name");
            vector.labels.push_back({name, attribute(elementTag, "label")});
            vector.values.push_back({name, trim(body.substr(elementTagEnd + 1, valueEnd - elementTagEnd - 1))});
            element = body.find('>', valueEnd) + 1;
        }
        vectors.push_back(vector);
        pos = close;
    }
    return vectors;
}

// The vectors of one property, in order.
std::vector<Vector> vectorsOf(const std::string &xml, const std::string &name)
{
    std::vector<Vector> result;
    for (const auto &vector : parseVectors(xml))
        if (vector.name == name)
            result.push_back(vector);
    return result;
}

// The last vector published for a property, or an empty one.
Vector lastOf(const std::string &xml, const std::string &name)
{
    std::vector<Vector> vectors = vectorsOf(xml, name);
    return vectors.empty() ? Vector() : vectors.back();
}

// Captures what the driver publishes on stdout. take() returns what was published since the last call; finish() also
// restores stdout, so call it before any EXPECT (gtest prints its messages to stdout).
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
            finish();
            if (m_File)
                fclose(m_File);
        }

        std::string take()
        {
            fflush(stdout);
            std::string text;
            fseek(m_File, m_Offset, SEEK_SET);
            char chunk[4096];
            size_t n;
            while ((n = fread(chunk, 1, sizeof(chunk), m_File)) > 0)
                text.append(chunk, n);
            m_Offset += static_cast<long>(text.size());
            fseek(m_File, 0, SEEK_END);
            return text;
        }

        std::string finish()
        {
            std::string text = take();
            if (m_SavedStdout >= 0)
            {
                fflush(stdout);
                dup2(m_SavedStdout, STDOUT_FILENO);
                close(m_SavedStdout);
                m_SavedStdout = -1;
            }
            return text;
        }

    private:
        FILE *m_File {nullptr};
        int m_SavedStdout {-1};
        long m_Offset {0};
};

std::string readFile(const std::string &path)
{
    std::ifstream file(path);
    std::stringstream text;
    text << file.rdbuf();
    return text.str();
}

// Whether a configuration file contains a vector of that name.
bool savesVector(const std::string &config, const std::string &name)
{
    return config.find("name='" + name + "'") != std::string::npos ||
           config.find("name=\"" + name + "\"") != std::string::npos;
}

bool contains(const std::vector<std::string> &commands, const std::string &command)
{
    return std::find(commands.begin(), commands.end(), command) != commands.end();
}

std::vector<std::string> withPrefix(const std::vector<std::string> &commands, const std::string &prefix)
{
    std::vector<std::string> result;
    for (const auto &command : commands)
        if (command.compare(0, prefix.size(), prefix) == 0)
            result.push_back(command);
    return result;
}

// Removes the configuration of the previous test before the driver reads it.
struct FreshConfig
{
    FreshConfig()
    {
        std::remove(g_ConfigFile.c_str());
        std::remove((g_ConfigFile + ".default").c_str());
    }
};

class PegasusUPBTest : public ::testing::Test
{
    protected:
        // Captures from the start, so the driver's output does not flood the test log.
        FreshConfig fresh;
        Published published;
        FakeUPB device;
        UPBUnderTest driver;
};

}

// #2539: a move to the current position leaves the SA reply unchanged; the move must still complete.
TEST_F(PegasusUPBTest, focusMoveToCurrentPositionCompletes)
{
    const bool connected = driver.connectTo(device.portName());
    driver.poll();
    device.takeCommands();
    published.take();

    driver.setNumber("ABS_FOCUS_POSITION", "FOCUS_ABSOLUTE_POSITION", 40000);
    const std::vector<std::string> commands = device.takeCommands();
    const std::string requested = published.take();
    driver.poll();
    const std::string polled = published.finish();

    ASSERT_TRUE(connected);
    EXPECT_TRUE(contains(commands, "SM:40000"));
    EXPECT_EQ(lastOf(requested, "ABS_FOCUS_POSITION").state, "Busy");
    EXPECT_EQ(lastOf(polled, "ABS_FOCUS_POSITION").state, "Ok") << "The move to the current position stayed busy";
}

// #2539: at the first poll after SM: the motor may not have started yet; the move is complete only when the motor
// stands at the target.
TEST_F(PegasusUPBTest, focusMoveIsNotCompleteBeforeMotorStarts)
{
    const bool connected = driver.connectTo(device.portName());
    driver.poll();
    published.take();

    driver.setNumber("ABS_FOCUS_POSITION", "FOCUS_ABSOLUTE_POSITION", 40200);
    std::string sofar = published.take();
    const std::string afterRequest = lastOf(sofar, "ABS_FOCUS_POSITION").state;

    // Not started yet: position and motor state as before the move.
    driver.poll();
    sofar += published.take();
    const std::string beforeStart = lastOf(sofar, "ABS_FOCUS_POSITION").state;

    device.setStepper(40100, true);
    driver.poll();
    sofar += published.take();
    const std::string running = lastOf(sofar, "ABS_FOCUS_POSITION").state;

    device.setStepper(40200, false);
    driver.poll();
    sofar += published.finish();
    const Vector atTarget = lastOf(sofar, "ABS_FOCUS_POSITION");

    ASSERT_TRUE(connected);
    EXPECT_EQ(afterRequest, "Busy");
    EXPECT_EQ(beforeStart, "Busy") << "The move was reported complete before the motor started";
    EXPECT_EQ(running, "Busy");
    EXPECT_EQ(atTarget.state, "Ok");
    EXPECT_EQ(atTarget.value("FOCUS_ABSOLUTE_POSITION"), "40200");
}

// #2539: a motor that stops away from the target ends the move with Alert after three polls, not with Ok.
TEST_F(PegasusUPBTest, focusMoveStoppedShortEndsWithAlert)
{
    const bool connected = driver.connectTo(device.portName());
    driver.poll();
    published.take();

    driver.setNumber("ABS_FOCUS_POSITION", "FOCUS_ABSOLUTE_POSITION", 40200);
    device.setStepper(40100, true);
    driver.poll();
    device.setStepper(40150, false);
    driver.poll();
    driver.poll();
    std::string sofar = published.take();
    const std::string afterTwoIdlePolls = lastOf(sofar, "ABS_FOCUS_POSITION").state;
    driver.poll();
    sofar += published.finish();

    ASSERT_TRUE(connected);
    EXPECT_EQ(afterTwoIdlePolls, "Busy") << "A motor standing away from the target ended the move at once";
    EXPECT_EQ(lastOf(sofar, "ABS_FOCUS_POSITION").state, "Alert");
    EXPECT_NE(sofar.find("The focuser stopped at 40150 instead of 40200."), std::string::npos);
}

// #2537: while a dew channel is off the device reports 0 for it; the duty cycle set for the channel must survive the
// poll, so switching the channel on sends it (P5:127 for 50 %), not P5:000.
TEST_F(PegasusUPBTest, dewDutyCycleIsKeptWhileChannelIsOff)
{
    const bool connected = driver.connectTo(device.portName());
    driver.poll();
    driver.setNumber("DEW_DUTY_CYCLES", "DEW_CHANNEL_1", 50);
    driver.poll();
    device.takeCommands();
    driver.setSwitch("DEW_CHANNELS", {{"DEW_CHANNEL_1", ISS_ON}});
    const std::vector<std::string> commands = device.takeCommands();
    published.finish();

    ASSERT_TRUE(connected);
    EXPECT_EQ(withPrefix(commands, "P5:"), std::vector<std::string> {"P5:127"});
}

// #2537: auto dew aggressiveness, focuser max speed, Power On Boot and the adjustable output voltage are stored in the
// device. A configuration saved by an older driver still contains them; loading it must not send them back.
TEST_F(PegasusUPBTest, oldConfigurationDoesNotChangeDeviceSettings)
{
    const bool connected = driver.connectTo(device.portName());
    driver.poll();

    // The device settings of a real configuration (driver 1.6, same UPB) with values other than the device's (5 V as
    // in the hardware test of #2537), and POWER_ON_BOOT as the drivers before the PowerInterface migration saved it
    // (1.6 saved it without device and name). FOCUS_MAX shows that the configuration was loaded.
    {
        std::ofstream config(g_ConfigFile);
        config << "<INDIDriver>\n"
                  "<newNumberVector device='Mock UPB' name='FOCUS_MAX'>\n"
                  "  <oneNumber name='FOCUS_MAX_VALUE'>\n90000\n  </oneNumber>\n</newNumberVector>\n"
                  "<newNumberVector device='Mock UPB' name='AUTO_DEW_AGG'>\n"
                  "  <oneNumber name='AUTO_DEW_AGG_VALUE'>\n120\n  </oneNumber>\n</newNumberVector>\n"
                  "<newNumberVector device='Mock UPB' name='FOCUSER_SETTINGS'>\n"
                  "  <oneNumber name='SETTING_MAX_SPEED'>\n300\n  </oneNumber>\n</newNumberVector>\n"
                  "<newSwitchVector device='Mock UPB' name='POWER_ON_BOOT'>\n"
                  "  <oneSwitch name='POWER_PORT_1'>\nOn\n  </oneSwitch>\n"
                  "  <oneSwitch name='POWER_PORT_2'>\nOn\n  </oneSwitch>\n"
                  "  <oneSwitch name='POWER_PORT_3'>\nOn\n  </oneSwitch>\n"
                  "  <oneSwitch name='POWER_PORT_4'>\nOff\n  </oneSwitch>\n</newSwitchVector>\n"
                  "<newSwitchVector device='Mock UPB' name='VARIABLE_CHANNELS'>\n"
                  "  <oneSwitch name='VAR_CHANNEL_1'>\nOn\n  </oneSwitch>\n</newSwitchVector>\n"
                  "<newNumberVector device='Mock UPB' name='VARIABLE_VOLTAGES'>\n"
                  "  <oneNumber name='VAR_CHANNEL_1'>\n      5\n  </oneNumber>\n</newNumberVector>\n"
                  "</INDIDriver>\n";
    }
    device.takeCommands();
    published.take();
    driver.setSwitch("CONFIG_PROCESS", {{"CONFIG_LOAD", ISS_ON}});
    const std::vector<std::string> commands = device.takeCommands();
    const std::string xml = published.finish();

    ASSERT_TRUE(connected);
    ASSERT_EQ(lastOf(xml, "FOCUS_MAX").value("FOCUS_MAX_VALUE"), "90000") << "The configuration was not loaded";
    EXPECT_TRUE(withPrefix(commands, "PD:").empty()) << "Auto dew aggressiveness sent: " << withPrefix(commands, "PD:")[0];
    EXPECT_TRUE(withPrefix(commands, "SS:").empty()) << "Focuser max speed sent: " << withPrefix(commands, "SS:")[0];
    EXPECT_TRUE(withPrefix(commands, "PE:").empty()) << "Power On Boot sent: " << withPrefix(commands, "PE:")[0];
    EXPECT_TRUE(withPrefix(commands, "P8:").empty()) << "Output voltage sent: " << withPrefix(commands, "P8:")[0];
}

// #2537: the settings stored in the device are no longer saved in the configuration.
TEST_F(PegasusUPBTest, configurationDoesNotSaveDeviceSettings)
{
    const bool connected = driver.connectTo(device.portName());
    driver.poll();
    driver.setSwitch("CONFIG_PROCESS", {{"CONFIG_SAVE", ISS_ON}});
    published.finish();
    const std::string config = readFile(g_ConfigFile);

    ASSERT_TRUE(connected);
    ASSERT_TRUE(savesVector(config, "DEW_DUTY_CYCLES")) << "No configuration saved:\n" << config;
    EXPECT_FALSE(savesVector(config, "AUTO_DEW_AGG"));
    EXPECT_FALSE(savesVector(config, "FOCUSER_SETTINGS"));
    EXPECT_FALSE(savesVector(config, "POWER_ON_BOOT"));
}

// #2537: the backlash steps reported by the device (SA, last field) are shown in FOCUS_BACKLASH_STEPS.
TEST_F(PegasusUPBTest, backlashStepsFromDeviceAreShown)
{
    device.setBacklash(25);
    const bool connected = driver.connectTo(device.portName());
    driver.poll();
    const std::string xml = published.finish();

    ASSERT_TRUE(connected);
    EXPECT_EQ(lastOf(xml, "FOCUS_BACKLASH_STEPS").value("FOCUS_BACKLASH_VALUE"), "25");
    EXPECT_EQ(lastOf(xml, "FOCUS_BACKLASH_TOGGLE").value("INDI_ENABLED"), "On");
}

// #2537: SB:0 switches the compensation off and SB:n on with n steps. Enabling it sent SB:1 (one step); it must send
// the steps the device reported before, which are kept while the compensation is off.
TEST_F(PegasusUPBTest, enablingBacklashSendsTheKeptSteps)
{
    device.setBacklash(25);
    const bool connected = driver.connectTo(device.portName());
    driver.poll();
    driver.setSwitch("FOCUS_BACKLASH_TOGGLE", {{"INDI_ENABLED", ISS_OFF}, {"INDI_DISABLED", ISS_ON}});
    driver.poll();
    const std::vector<std::string> disabling = device.takeCommands();
    const std::string afterDisable = published.take();
    driver.setSwitch("FOCUS_BACKLASH_TOGGLE", {{"INDI_ENABLED", ISS_ON}, {"INDI_DISABLED", ISS_OFF}});
    const std::vector<std::string> enabling = device.takeCommands();
    published.finish();

    ASSERT_TRUE(connected);
    EXPECT_TRUE(contains(disabling, "SB:0"));
    EXPECT_EQ(lastOf(afterDisable, "FOCUS_BACKLASH_TOGGLE").value("INDI_DISABLED"), "On");
    EXPECT_EQ(enabling, std::vector<std::string> {"SB:25"});
}

// #2537: SB:n and SS:n are answered. The reply must be read, otherwise it becomes the reply to the next command (seen
// on the device as P8:5 -> SB:0). The fake answers after 50 ms, so the reply is still on its way when the next command
// follows.
TEST_F(PegasusUPBTest, backlashAndSpeedRepliesAreRead)
{
    const bool connected = driver.connectTo(device.portName());
    driver.poll();
    device.takeCommands();
    published.take();
    device.setReplyDelay(std::chrono::milliseconds(50));

    driver.setSwitch("FOCUS_BACKLASH_TOGGLE", {{"INDI_ENABLED", ISS_OFF}, {"INDI_DISABLED", ISS_ON}});
    driver.setNumber("VARIABLE_VOLTAGES", "VAR_CHANNEL_1", 5);
    const std::string afterBacklash = published.take();
    driver.setNumber("FOCUSER_SETTINGS", "SETTING_MAX_SPEED", 500);
    driver.setNumber("VARIABLE_VOLTAGES", "VAR_CHANNEL_1", 6);
    const std::string afterSpeed = published.finish();
    const std::vector<std::string> commands = device.takeCommands();

    ASSERT_TRUE(connected);
    EXPECT_EQ(commands, (std::vector<std::string> {"SB:0", "P8:5", "SS:500", "P8:6"}));
    EXPECT_EQ(lastOf(afterBacklash, "VARIABLE_VOLTAGES").state, "Ok") << "P8:5 got the reply to SB:0";
    EXPECT_EQ(lastOf(afterSpeed, "VARIABLE_VOLTAGES").state, "Ok") << "P8:6 got the reply to SS:500";
}

// #2537: after a reconnection the switches are reset by PI::initProperties(); the next poll must publish the real
// port states even though the device reports the same as before, and switching a port off must send the command.
TEST_F(PegasusUPBTest, reconnectionPublishesRealPortStates)
{
    const bool connected = driver.connectTo(device.portName());
    driver.poll();
    const bool disconnected = driver.disconnectNow();
    device.releasePort();
    const bool reconnected = driver.connectTo(device.portName());
    published.take();
    driver.poll();
    const std::string polled = published.take();
    device.takeCommands();
    driver.setSwitch("USB_PORTS", {{"USB_PORT_6", ISS_OFF}});
    const std::vector<std::string> commands = device.takeCommands();
    published.finish();

    ASSERT_TRUE(connected);
    ASSERT_TRUE(disconnected);
    ASSERT_TRUE(reconnected);
    const Vector usb = lastOf(polled, "USB_PORTS");
    EXPECT_EQ(usb.value("USB_PORT_1"), "On") << "USB ports not published as on after the reconnection";
    EXPECT_EQ(usb.value("USB_PORT_6"), "On");
    EXPECT_EQ(lastOf(polled, "POWER_CHANNELS").value("POWER_CHANNEL_1"), "On");
    EXPECT_EQ(commands, std::vector<std::string> {"U6:0"});
}

// #2537: the adjustable output always supplies its voltage; P8:0 is answered with the stored voltage. It is shown as
// on, and switching it off is refused without sending P8:0.
TEST_F(PegasusUPBTest, adjustableOutputCannotBeSwitchedOff)
{
    const bool connected = driver.connectTo(device.portName());
    const std::string connectXml = published.take();
    device.takeCommands();
    driver.setSwitch("VARIABLE_CHANNELS", {{"VAR_CHANNEL_1", ISS_OFF}});
    const std::vector<std::string> commands = device.takeCommands();
    const std::string xml = published.finish();

    ASSERT_TRUE(connected);
    EXPECT_EQ(lastOf(connectXml, "VARIABLE_CHANNELS").value("VAR_CHANNEL_1"), "On");
    EXPECT_TRUE(withPrefix(commands, "P8:").empty()) << "Sent " << withPrefix(commands, "P8:")[0];
    const Vector channels = lastOf(xml, "VARIABLE_CHANNELS");
    EXPECT_EQ(channels.state, "Alert");
    EXPECT_EQ(channels.value("VAR_CHANNEL_1"), "On");
}

// #2537: P8:n takes whole volts from 3 to 12; the property shows the voltage that was sent.
TEST_F(PegasusUPBTest, adjustableVoltageIsRoundedAndLimited)
{
    const bool connected = driver.connectTo(device.portName());
    device.takeCommands();
    published.take();
    driver.setNumber("VARIABLE_VOLTAGES", "VAR_CHANNEL_1", 5.4);
    const std::string rounded = published.take();
    driver.setNumber("VARIABLE_VOLTAGES", "VAR_CHANNEL_1", 15);
    const std::string limited = published.finish();
    const std::vector<std::string> commands = device.takeCommands();

    ASSERT_TRUE(connected);
    EXPECT_EQ(commands, (std::vector<std::string> {"P8:5", "P8:12"}));
    EXPECT_EQ(lastOf(rounded, "VARIABLE_VOLTAGES").value("VAR_CHANNEL_1"), "5");
    EXPECT_EQ(lastOf(limited, "VARIABLE_VOLTAGES").value("VAR_CHANNEL_1"), "12");
}

// #2537: a status reply with a field that is not a number threw from std::stod out of TimerHit() and ended the driver.
TEST_F(PegasusUPBTest, garbledReplyDoesNotEndDriver)
{
    const bool connected = driver.connectTo(device.portName());
    driver.poll();
    device.garbleNextStatus();
    bool threw = false;
    try
    {
        driver.poll();
    }
    catch (const std::exception &)
    {
        threw = true;
    }
    const bool stillConnected = driver.isConnected();
    published.finish();

    ASSERT_TRUE(connected);
    EXPECT_FALSE(threw) << "The garbled reply threw out of the poll";
    EXPECT_TRUE(stillConnected);
}

// #2537: stepperUpdated() compared the stepper data with the sensor data, and the reverse state with the motor state
// field. A change of the backlash alone must not republish position and reverse; a change of the reverse state must
// publish it.
TEST_F(PegasusUPBTest, stepperChangesArePublishedPerField)
{
    const bool connected = driver.connectTo(device.portName());
    driver.poll();
    published.take();
    device.setBacklash(30);
    driver.poll();
    const std::string backlashChanged = published.take();
    device.setReverse(true);
    driver.poll();
    const std::string reverseChanged = published.finish();

    ASSERT_TRUE(connected);
    ASSERT_EQ(lastOf(backlashChanged, "FOCUS_BACKLASH_STEPS").value("FOCUS_BACKLASH_VALUE"), "30") << "No stepper poll";
    EXPECT_TRUE(vectorsOf(backlashChanged, "ABS_FOCUS_POSITION").empty()) << "Unchanged position published again";
    EXPECT_TRUE(vectorsOf(backlashChanged, "FOCUS_REVERSE_MOTION").empty()) << "Unchanged reverse state published again";
    EXPECT_EQ(lastOf(reverseChanged, "FOCUS_REVERSE_MOTION").value("INDI_ENABLED"), "On")
            << "Changed reverse state not published";
}

// #2537: Power On Boot and Over Current were defined without device and name since the PowerInterface migration.
TEST_F(PegasusUPBTest, powerOnBootAndOverCurrentAreDefined)
{
    const bool connected = driver.connectTo(device.portName());
    const std::string xml = published.finish();

    ASSERT_TRUE(connected);
    const Vector boot = lastOf(xml, "POWER_ON_BOOT");
    EXPECT_EQ(boot.kind, "def");
    EXPECT_EQ(boot.device, DEVICE_NAME);
    EXPECT_EQ(boot.values.size(), 4u);
    EXPECT_EQ(boot.value("POWER_PORT_4"), "On");
    const Vector overCurrent = lastOf(xml, "POWER_OVER_CURRENT");
    EXPECT_EQ(overCurrent.kind, "def");
    EXPECT_EQ(overCurrent.device, DEVICE_NAME);
    EXPECT_EQ(overCurrent.values.size(), 7u);
}

// #2537: the over current lights of the dew ports used the indices 0-2 and overwrote those of ports 1-3.
TEST_F(PegasusUPBTest, dewOverCurrentDoesNotOverwritePortLights)
{
    const bool connected = driver.connectTo(device.portName());
    driver.poll();
    published.take();
    device.setOverCurrent("0000100");
    driver.poll();
    driver.poll();
    const std::string xml = published.finish();

    ASSERT_TRUE(connected);
    const Vector lights = lastOf(xml, "POWER_OVER_CURRENT");
    EXPECT_EQ(lights.value("POWER_PORT_1"), "Ok") << "The light of port 1 was overwritten by dew A";
    EXPECT_EQ(lights.value("DEW_A"), "Alert");
    EXPECT_EQ(lights.state, "Alert");
}

// #2537: the USB labels were set before PI::initProperties() created them, so they were lost.
TEST_F(PegasusUPBTest, usbLabelsAreSet)
{
    const bool connected = driver.connectTo(device.portName());
    const std::string xml = published.finish();

    ASSERT_TRUE(connected);
    const Vector labels = lastOf(xml, "USB_LABELS");
    EXPECT_EQ(labels.label("USB_PORT_1"), "USB3 Port 1");
    EXPECT_EQ(labels.label("USB_PORT_4"), "USB3 Port 4");
    EXPECT_EQ(labels.label("USB_PORT_5"), "USB2 Port 5");
    EXPECT_EQ(labels.label("USB_PORT_6"), "USB2 Port 6");
}

// #2537: on UPB v2 boards from revision C on the stepper output sleeps while the LED is off (Pegasus command table).
TEST_F(PegasusUPBTest, ledOffWarnsAboutStepperOutput)
{
    const bool connected = driver.connectTo(device.portName());
    published.take();
    // Only the element switched on, as indi_setprop sends it: PI::processSwitch() takes the first element name.
    driver.setSwitch("LED_CONTROL", {{"INDI_DISABLED", ISS_ON}});
    const std::vector<std::string> commands = device.takeCommands();
    const std::string xml = published.finish();

    ASSERT_TRUE(connected);
    EXPECT_TRUE(contains(commands, "PL:0"));
    EXPECT_NE(xml.find("the stepper motor output sleeps"), std::string::npos);
}

int main(int argc, char **argv)
{
    // Never touch the user's real ~/.indi.
    char homeTemplate[] = "/tmp/test_pegasus_upb_home_XXXXXX";
    const char *home = mkdtemp(homeTemplate);
    if (home)
        setenv("HOME", home, 1);
    // Never read or write the user's real driver configuration.
    g_ConfigFile = "/tmp/test_pegasus_upb_" + std::to_string(getpid()) + "_config.xml";
    setenv("INDICONFIG", g_ConfigFile.c_str(), 1);
    INDI::Logger::getInstance().configure("", INDI::Logger::file_off, INDI::Logger::DBG_ERROR,
                                          INDI::Logger::DBG_ERROR | INDI::Logger::DBG_WARNING);
    ::testing::InitGoogleTest(&argc, argv);
    int rc = RUN_ALL_TESTS();
    std::remove(g_ConfigFile.c_str());
    std::remove((g_ConfigFile + ".default").c_str());
    if (home)
    {
        std::remove((std::string(home) + "/.indi").c_str());
        std::remove(home);
    }
    return rc;
}
