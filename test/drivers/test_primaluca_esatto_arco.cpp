/*
    Regression tests for the PrimaLuceLab Esatto (indi_esatto_focus) and Esatto Arco (indi_esattoarco_focus)
    drivers and their shared command set (primalucacommandset.cpp):
    - Rotator angles keep their fraction in GoTo and Sync.
    - A relative move the device refuses is reported as such, and a focuser move leaves the rotator idle.
    - A move ends also when the last poll before the stop saw no position change.
    - Unexpected input does not crash the driver: a firmware reply without SWAPP, both backlash switches off and
      a reply longer than the read buffer.
    - Values at connect: the Esatto position after the limits, a missing external probe, a reply without a number,
      the Arco backlash and the name of the Arco firmware element.
    - Esatto Arco: backlash measurement towards larger positions, motor temperature without the external probe,
      motor speed during a move, warnings at connect.
    - A motor still braking (BUSY 0, MST "dec") is busy.
    - Esatto Arco: the final position of a move, a rotator stop short of the target, the end of the Arco calibration.
    - The focuser calibration only starts at the sensor, blocks moves and a second start, and ends with CAL_STATUS.
      It is complete only if CAL_NUMOF rose; an abort ends it in Alert, and moves wait until the firmware reports
      CAL_STATUS "stop", also for a calibration started elsewhere. After an aborted or incomplete calibration moves
      wait until a calibration has completed (CAL_NUMOF rose), and the messages name the web interface as the way out.
      While moves are locked, a model with a sensor at each end (no "LP" in MODNAME) is not calibrated by the driver.
      A move to the current position sends nothing, but one to the last polled position while the focuser moves or
      brakes, also before the motor reports that it moves, or after it stopped elsewhere, is sent.

    The drivers talk to a fake device on a pseudo terminal. Requests and replies are JSON objects ({"req": ...} and
    {"res": ...}, replies end with CR LF). The replies have the shape the driver's parser expects, as in an earlier
    simulator written for these fixes (not part of the repository). The protocol document named in the driver (USB
    Control Specification revision 3.3) was not available. Where the values come from:
    - Seen on an Esatto 2" LP (firmware 3.05.28) and an Arco 2, as recorded in the commit messages of the fixes
      ("PrimaLuceLab: a motor still braking is busy", "Esatto Arco: report the final position, finish the
      calibration", "Esatto, Esatto Arco: calibrate the focuser like the web interface"): STATUS with BUSY 0 and
      MST "dec" while braking, MST "CstSpeed", the calibration request of the web interface and CAL_STATUS
      "ManageCalibInMinPosWithSensorAct" during an Esatto calibration, "exec" during an Arco calibration, "stop" when
      idle, Arco position 0 during its calibration, an Arco stop 0.2 degrees short of the target, HSENDET 1 at the
      inner end, position 816 after a calibration of the LP, the firmware version 3.05.28.
    - Seen on 24.11.2026 on the same devices: an Arco 2 move to 10000 steps ends at 1.75781 degrees (about 5689
      steps per degree); MOT_STOP during a focuser calibration stops the motor, but CAL_STATUS keeps the phase name
      for about 60 s and CAL_NUMOF does not change; a completed calibration raises CAL_NUMOF by one.
    - Seen on a standard ESATTO 2" (ESATTO20582, firmware 3.05.28): HSENDET 1 at both ends (24.11.2026), MODNAME
      "ESATTO2" (26.11.2026).
    - Assumed: MODNAME "ESATTO2LP" for the LP (its serial number is ESATTO2LP0027), the error reply to a refused command ({"ERROR": ...}), temperatures and voltages as strings, EXT_T
      "-127.0" without the external probe (the value the driver checks for), the backlash 120, CAL_MAXPOS 406929, the
      Arco limits of +-648000 steps, the serial numbers, STATUS with only BUSY and MST, and
      a pause with MST "stop" between two calibration phases. Each test sets when the positions change.

    SPDX-License-Identifier: LGPL-2.1-or-later
*/

#include <gtest/gtest.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <fcntl.h>
#include <filesystem>
#include <memory>
#include <mutex>
#include <new>
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
#include <utility>
#include <vector>

#include "indilogger.h"
#include "drivers/focuser/esatto.h"
#include "drivers/focuser/esattoarco.h"

namespace
{

const char *ESATTO_NAME = "Mock Esatto";
const char *ARCO_NAME   = "Mock Esatto Arco";

// Esatto calibration phase as the LP reports it in CAL_STATUS.
const char *CALIBRATION_PHASE = "ManageCalibInMinPosWithSensorAct";

// Fake Esatto with an Arco. Nodes: "" for the generic values, "MOT1" for the focuser and "MOT2" for the Arco.
class FakePrimaLuce
{
    public:
        FakePrimaLuce()
        {
            m_State =
            {
                {
                    "", {
                        {"SN", "ESATTO2LP0027"}, {"MODNAME", "ESATTO2LP"}, {"SWVERS", {{"SWAPP", "3.05.28"}}}, {"EXT_T", "-127.0"},
                        {"VIN_12V", "12.1"}, {"VIN_USB", "5.0"}, {"ARCO", 1}, {"ARCO_SN", "ARCO20001"}
                    }
                },
                {
                    "MOT1", {
                        {"ABS_POS_STEP", 10000}, {"CAL_MAXPOS", 406929}, {"BKLASH", 120}, {"NTC_T", "21.5"},
                        {"SPEED", 0}, {"HSENDET", 0}, {"CAL_STATUS", "stop"}, {"CAL_NUMOF", 17}, {"STATUS", stopped()}
                    }
                },
                {
                    "MOT2", {
                        {"POSITION_STEP", 0}, {"POSITION_DEG", 0.0}, {"CAL_STATUS", "stop"}, {"STATUS", stopped()},
                        {"CAL_MAXPOS", 648000}, {"CAL_MINPOS", -648000}
                    }
                }
            };

            char name[128] = {0};
            if (openpty(&m_Master, &m_Slave, name, nullptr, nullptr) != 0)
                return;
            makeRaw(m_Master);
            makeRaw(m_Slave);
            m_PortName = name;
            m_Receiver = std::thread(&FakePrimaLuce::receive, this);
            m_Processor = std::thread(&FakePrimaLuce::process, this);
        }

        ~FakePrimaLuce()
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

        static json stopped()
        {
            return {{"BUSY", 0}, {"MST", "stop"}};
        }

        static json moving()
        {
            return {{"BUSY", 1}, {"MST", "CstSpeed"}};
        }

        // Value the device reports from now on.
        void set(const std::string &node, const std::string &key, const json &value)
        {
            waitUntilIdle();
            std::lock_guard<std::mutex> lock(m_Mutex);
            m_State[node][key] = value;
        }

        // Changes a value once the next read of readNode/readKey has been answered.
        void setAfterRead(const std::string &readNode, const std::string &readKey, const std::string &node,
                          const std::string &key, const json &value)
        {
            std::lock_guard<std::mutex> lock(m_Mutex);
            m_AfterRead.push_back({readNode, readKey, node, key, value});
        }

        void rejectCommands(bool reject)
        {
            std::lock_guard<std::mutex> lock(m_Mutex);
            m_Reject = reject;
        }

        // Sends this instead of the reply to the next request.
        void replyRaw(const std::string &reply)
        {
            std::lock_guard<std::mutex> lock(m_Mutex);
            m_RawReply = reply;
        }

        // "set" and "cmd" requests received since the last call, e.g. {"cmd":{"MOT1":{"MOVE_ABS":{"STEP":20000}}}}.
        std::vector<json> takeWrites()
        {
            waitUntilIdle();
            std::lock_guard<std::mutex> lock(m_Mutex);
            std::vector<json> writes;
            writes.swap(m_Writes);
            return writes;
        }

    private:
        struct Change
        {
            std::string readNode, readKey, node, key;
            json value;
        };

        static void makeRaw(int fd)
        {
            struct termios tio;
            tcgetattr(fd, &tio);
            cfmakeraw(&tio);
            tcsetattr(fd, TCSANOW, &tio);
        }

        // Waits (at most 2 s) until nothing is left on the line, in the receive buffer or in progress. Checks the line
        // with select(): FIONREAD on a pty master reports the slave's input queue on macOS.
        // The line is checked under the same lock as receive() reads and stores, so bytes taken off the line but not
        // yet stored are never missed.
        void waitUntilIdle()
        {
            for (int i = 0; i < 400; i++)
            {
                {
                    std::lock_guard<std::mutex> lock(m_RxMutex);
                    fd_set readSet;
                    FD_ZERO(&readSet);
                    FD_SET(m_Master, &readSet);
                    struct timeval timeout = {0, 0};
                    const bool pending = select(m_Master + 1, &readSet, nullptr, nullptr, &timeout) > 0;
                    if (!pending && m_Rx.empty() && !m_Busy)
                        return;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
            }
        }

        // Takes every byte off the line as soon as it arrives, like a UART interrupt, so the tcflush() before each
        // request never discards what the device has already received.
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

                {
                    // Read and store under one lock, see waitUntilIdle().
                    std::lock_guard<std::mutex> lock(m_RxMutex);
                    char chunk[256];
                    ssize_t n = read(m_Master, chunk, sizeof(chunk));
                    if (n <= 0)
                        continue;
                    m_Rx.append(chunk, n);
                }
                m_Received.notify_all();
            }
        }

        // Splits the received bytes into JSON objects and answers each one.
        void process()
        {
            std::string request;
            int depth = 0;
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
                    if (depth == 0 && c != '{')
                        continue;
                    request += c;
                    depth += (c == '{') - (c == '}');
                    if (depth == 0)
                    {
                        const std::string answer = reply(request);
                        request.clear();
                        if (write(m_Master, answer.data(), answer.size()) < 0)
                            break;
                    }
                }

                std::lock_guard<std::mutex> lock(m_RxMutex);
                if (m_Rx.empty())
                    m_Busy = false;
            }
        }

        // Last key of a request body, which the reply acknowledges: "STEP" for {"MOVE_ABS":{"STEP":20000}}.
        static std::string lastKey(const json &body)
        {
            const std::string path = body.flatten().items().begin().key();
            return path.substr(path.rfind('/') + 1);
        }

        std::string reply(const std::string &text)
        {
            std::lock_guard<std::mutex> lock(m_Mutex);
            if (!m_RawReply.empty())
            {
                std::string raw;
                raw.swap(m_RawReply);
                return raw;
            }

            const json request = json::parse(text, nullptr, false);
            if (!request.is_object() || !request.contains("req") || !request["req"].is_object() || request["req"].empty())
                return "{\"res\":{}}\r\n";

            const std::string kind = request["req"].begin().key();
            const json &body = request["req"].begin().value();
            std::string node;
            json params = body;
            if (body.is_object() && body.size() == 1 && (body.contains("MOT1") || body.contains("MOT2")))
            {
                node = body.begin().key();
                params = body.begin().value();
            }

            json answer = json::object();
            if (kind == "get" && params.is_string())
            {
                // All values of a motor, the Arco motor info
                answer = m_State[node];
            }
            else if (kind == "get" && params.is_object())
            {
                std::vector<Change> applied;
                for (const auto &item : params.items())
                {
                    if (m_State[node].contains(item.key()))
                        answer[item.key()] = m_State[node][item.key()];
                    else
                        answer["ERROR"] = "unknown " + item.key();

                    for (auto it = m_AfterRead.begin(); it != m_AfterRead.end();)
                    {
                        if (it->readNode == node && it->readKey == item.key())
                        {
                            applied.push_back(*it);
                            it = m_AfterRead.erase(it);
                        }
                        else
                            ++it;
                    }
                }
                for (const auto &change : applied)
                    m_State[change.node][change.key] = change.value;
            }
            else if (kind == "set" && params.is_object())
            {
                m_Writes.push_back(request["req"]);
                for (const auto &item : params.items())
                {
                    m_State[node][item.key()] = item.value();
                    answer[item.key()] = "done";
                }
            }
            else if (kind == "cmd")
            {
                m_Writes.push_back(request["req"]);
                if (m_Reject)
                    answer["ERROR"] = "rejected";
                else
                    answer[lastKey(params)] = "done";
            }

            json result;
            if (node.empty())
                result[kind] = answer;
            else
                result[kind][node] = answer;
            return json{{"res", result}}.dump() + "\r\n";
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
        json m_State;
        std::vector<Change> m_AfterRead;
        std::vector<json> m_Writes;
        bool m_Reject {false};
        std::string m_RawReply;
};

// Drives Esatto or EsattoArco as a client does.
template <class Driver>
class UnderTest : public Driver
{
    public:
        explicit UnderTest(const char *name)
        {
            this->setDeviceName(name);
            // As at a client's first getProperties; this also runs initProperties().
            this->ISGetProperties(nullptr);
        }

        // Port, then CONNECT. Auto search is off so a failed connection doesn't scan the real serial ports.
        bool connectTo(const std::string &port)
        {
            sendSwitch("DEVICE_AUTO_SEARCH", {{"INDI_ENABLED", ISS_OFF}, {"INDI_DISABLED", ISS_ON}});

            std::string portText = port;
            char portName[] = "PORT";
            char *texts[1] = {&portText[0]};
            char *textNames[1] = {portName};
            this->ISNewText(this->getDeviceName(), "DEVICE_PORT", texts, textNames, 1);

            return sendSwitch("CONNECTION", {{"CONNECT", ISS_ON}, {"DISCONNECT", ISS_OFF}}) && this->isConnected();
        }

        bool sendSwitch(const char *property, const std::vector<std::pair<std::string, ISState>> &switches)
        {
            std::vector<std::string> names;
            std::vector<ISState> states;
            for (const auto &one : switches)
            {
                names.push_back(one.first);
                states.push_back(one.second);
            }
            std::vector<char *> namePointers;
            for (auto &name : names)
                namePointers.push_back(&name[0]);
            return this->ISNewSwitch(this->getDeviceName(), property, states.data(), namePointers.data(),
                                     static_cast<int>(states.size()));
        }

        bool sendNumber(const char *property, const char *element, double value)
        {
            std::string elementName = element;
            char *names[1] = {&elementName[0]};
            return this->ISNewNumber(this->getDeviceName(), property, &value, names, 1);
        }

        bool moveTo(double position)
        {
            return sendNumber("ABS_FOCUS_POSITION", "FOCUS_ABSOLUTE_POSITION", position);
        }

        bool moveIn(double steps)
        {
            sendSwitch("FOCUS_MOTION", {{"FOCUS_INWARD", ISS_ON}, {"FOCUS_OUTWARD", ISS_OFF}});
            return sendNumber("REL_FOCUS_POSITION", "FOCUS_RELATIVE_POSITION", steps);
        }

        bool startCalibration()
        {
            return sendSwitch("FOCUS_CALIBRATION", {{"CALIBRATION_START", ISS_ON}});
        }

        // State of a property as the driver holds it.
        IPState numberState(const char *property)
        {
            return this->getNumber(property).getState();
        }

        IPState switchState(const char *property)
        {
            return this->getSwitch(property).getState();
        }

        // One poll, as the polling timer runs it.
        void poll()
        {
            this->TimerHit();
        }

        bool abort()
        {
            return sendSwitch("FOCUS_ABORT_MOTION", {{"ABORT", ISS_ON}});
        }
};

using EsattoUnderTest = UnderTest<Esatto>;
using ArcoUnderTest = UnderTest<EsattoArco>;

// EsattoArco defines RotCalibrationMessageTP without ever filling it (also on upstream master), and an uninitialized
// one crashes defineProperty(). The driver's only instance is allocated at program start, where fresh heap memory is
// usually zero, so the tests create theirs in zeroed memory.
template <class Driver>
std::unique_ptr<Driver, void (*)(Driver *)> makeDriver(const char *name)
{
    void *memory = calloc(1, sizeof(Driver));
    return std::unique_ptr<Driver, void (*)(Driver *)>(new (memory) Driver(name), [](Driver * driver)
    {
        driver->~Driver();
        free(driver);
    });
}

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

        // States of the definitions and updates of a property.
        std::vector<std::string> states(const std::string &property)
        {
            std::vector<std::string> result;
            for (const auto &update : updates())
                if (update.name == property)
                    result.push_back(update.state);
            return result;
        }

        // Published values of an element of a number property, in order.
        std::vector<double> values(const std::string &property, const std::string &element)
        {
            std::vector<double> result;
            for (const auto &text : texts(property, element))
                result.push_back(std::stod(text));
            return result;
        }

        // Published values of an element, as text.
        std::vector<std::string> texts(const std::string &property, const std::string &element)
        {
            std::vector<std::string> result;
            const std::regex value("<(?:def|one)(?:Number|Text|Switch)\\b[^>]*\\bname=['\"]" + element +
                                   "['\"][^>]*>\\s*([^<]*?)\\s*</");
            for (const auto &update : updates())
            {
                std::smatch match;
                if (update.name == property && std::regex_search(update.body, match, value))
                    result.push_back(match[1]);
            }
            return result;
        }

        // Element names of the first definition of a property.
        std::vector<std::string> elements(const std::string &property)
        {
            std::vector<std::string> result;
            const std::regex element("<def(?:Number|Text|Switch)\\b[^>]*\\bname=['\"]([^'\"]*)['\"]");
            for (const auto &update : updates())
            {
                if (update.name != property || update.vector.rfind("def", 0) != 0)
                    continue;
                for (auto it = std::sregex_iterator(update.body.begin(), update.body.end(), element);
                        it != std::sregex_iterator(); ++it)
                    result.push_back((*it)[1]);
                break;
            }
            return result;
        }

        // Log messages sent to the client.
        std::vector<std::string> messages()
        {
            std::string text = read();
            std::vector<std::string> result;
            const std::regex message("<message\\b[^>]*\\bmessage=['\"]([^'\"]*)['\"]");
            for (auto it = std::sregex_iterator(text.begin(), text.end(), message); it != std::sregex_iterator(); ++it)
                result.push_back((*it)[1]);
            return result;
        }

    private:
        struct Update
        {
            std::string vector, name, state, body;
        };

        static std::string attribute(const std::string &attributes, const std::string &name)
        {
            std::smatch match;
            const std::regex value("\\b" + name + "=['\"]([^'\"]*)['\"]");
            return std::regex_search(attributes, match, value) ? std::string(match[1]) : std::string();
        }

        std::vector<Update> updates()
        {
            const std::string text = read();
            std::vector<Update> result;
            const std::regex header("<((?:def|set)(?:Number|Switch|Text|Light|BLOB)Vector)\\b([^>]*)>");
            size_t offset = 0;
            std::smatch match;
            while (offset < text.size() && std::regex_search(text.begin() + offset, text.end(), match, header))
            {
                Update update;
                update.vector = match[1];
                update.name = attribute(match[2], "name");
                update.state = attribute(match[2], "state");
                const size_t bodyStart = offset + match.position(0) + match.length(0);
                const std::string close = "</" + update.vector + ">";
                const size_t bodyEnd = std::min(text.find(close, bodyStart), text.size());
                update.body = text.substr(bodyStart, bodyEnd - bodyStart);
                result.push_back(update);
                offset = bodyEnd + close.size();
            }
            return result;
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

// Writes of one kind to one key, e.g. ("cmd", "MOT2", "MOVE_ABS") gives {"DEG":123.45} for a rotator GoTo.
std::vector<json> writesTo(const std::vector<json> &writes, const std::string &kind, const std::string &node,
                           const std::string &key)
{
    std::vector<json> result;
    for (const auto &write : writes)
    {
        if (!write.contains(kind))
            continue;
        const json &body = node.empty() ? write[kind] : (write[kind].contains(node) ? write[kind][node] : json());
        if (body.is_object() && body.contains(key))
            result.push_back(body[key]);
    }
    return result;
}

bool contains(const std::vector<std::string> &values, const std::string &value)
{
    return std::find(values.begin(), values.end(), value) != values.end();
}

bool anyContains(const std::vector<std::string> &texts, const std::string &part)
{
    for (const auto &text : texts)
        if (text.find(part) != std::string::npos)
            return true;
    return false;
}

void pause(int milliseconds)
{
    std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds));
}

// Polls for 2.3 s, past the 2 s the driver allows a move or a calibration to start.
template <class Driver>
void pollPastStartGrace(Driver &driver)
{
    const auto start = std::chrono::steady_clock::now();
    while (std::chrono::steady_clock::now() - start < std::chrono::milliseconds(2300))
    {
        driver.poll();
        pause(100);
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Focuser calibration checks, the same in both drivers. The focuser starts on its sensor (HSENDET 1).
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// A calibration that raises CAL_NUMOF has completed.
template <class Driver>
void checkCompletedCalibrationIsOk(Driver &driver, FakePrimaLuce &device)
{
    device.takeWrites();

    Published published;
    driver.startCalibration();
    driver.poll();
    device.set("MOT1", "CAL_STATUS", "stop");
    device.set("MOT1", "CAL_NUMOF", 18);
    pollPastStartGrace(driver);
    std::vector<std::string> states = published.states("FOCUS_CALIBRATION");
    std::vector<std::string> messages = published.messages();

    ASSERT_FALSE(states.empty());
    EXPECT_EQ(states.back(), "Ok");
    EXPECT_TRUE(anyContains(messages, "Focuser calibration complete."));
}

// CAL_STATUS also returns to "stop" when the firmware gives up (on an LP: 120 s without reaching the sensor, after an
// abort: about 60 s). Without a higher CAL_NUMOF the calibration is reported as failed, not as complete.
template <class Driver>
void checkCalibrationWithoutCountIsAlert(Driver &driver, FakePrimaLuce &device)
{
    device.takeWrites();

    Published published;
    driver.startCalibration();
    driver.poll();
    device.set("MOT1", "CAL_STATUS", "stop");
    pollPastStartGrace(driver);
    std::vector<std::string> states = published.states("FOCUS_CALIBRATION");
    std::vector<std::string> messages = published.messages();

    ASSERT_FALSE(states.empty());
    EXPECT_EQ(states.back(), "Alert") << "A calibration that did not raise CAL_NUMOF was reported as complete";
    EXPECT_FALSE(anyContains(messages, "Focuser calibration complete."));
    EXPECT_TRUE(anyContains(messages, "did not complete"));
}

// An abort sends MOT_STOP, which stops the motor, but the firmware keeps CAL_STATUS on the phase for about a minute.
// The calibration ends in Alert with a warning about the zero position. Moves are refused while CAL_STATUS names the
// phase (a move into a running calibration shifted the zero position) and also after "stop" (on an ESATTO 2" a move
// after an aborted calibration ran 60 s against the inner end), until a calibration has completed.
template <class Driver>
void checkAbortDuringCalibration(Driver &driver, FakePrimaLuce &device)
{
    device.takeWrites();

    Published published;
    driver.startCalibration();
    driver.poll();
    device.takeWrites();
    driver.abort();
    std::vector<json> aborted = device.takeWrites();
    driver.poll();
    driver.moveTo(30000);
    std::vector<json> whileStillRunning = device.takeWrites();
    std::vector<std::string> states = published.states("FOCUS_CALIBRATION");
    std::vector<std::string> messages = published.messages();

    Published afterStopPublished;
    device.set("MOT1", "CAL_STATUS", "stop");
    driver.moveTo(30000);
    std::vector<json> afterStop = device.takeWrites();
    std::vector<std::string> afterStopMessages = afterStopPublished.messages();

    // A completed calibration (CAL_NUMOF rose) makes the position valid again
    driver.startCalibration();
    driver.poll();
    device.set("MOT1", "CAL_STATUS", "stop");
    device.set("MOT1", "CAL_NUMOF", 18);
    pollPastStartGrace(driver);
    device.takeWrites();
    driver.moveTo(30000);
    std::vector<json> afterCalibration = device.takeWrites();

    EXPECT_EQ(writesTo(aborted, "cmd", "MOT1", "MOT_STOP").size(), 1u) << "The motor was not stopped";
    ASSERT_FALSE(states.empty());
    EXPECT_EQ(states.back(), "Alert") << "An aborted calibration did not end in Alert";
    EXPECT_TRUE(anyContains(messages, "zero position is no longer valid")) << "No warning about the zero position";
    EXPECT_TRUE(writesTo(whileStillRunning, "cmd", "MOT1", "MOVE_ABS").empty())
            << "A move was sent while the firmware still reported the calibration";
    EXPECT_TRUE(anyContains(messages, "reports a running calibration"));
    EXPECT_TRUE(writesTo(afterStop, "cmd", "MOT1", "MOVE_ABS").empty())
            << "A move was sent after an aborted calibration, before a new one completed";
    EXPECT_TRUE(anyContains(afterStopMessages, "position is not valid"));
    EXPECT_EQ(writesTo(afterCalibration, "cmd", "MOT1", "MOVE_ABS").size(), 1u)
            << "The move was refused after a completed calibration";
}

// A sensor that is active in the outer half (assumed: a model with its sensor at the outer end) is not calibrated:
// the web interface sends a different request for a focuser fully out, which was never seen on a device.
template <class Driver>
void checkOuterSensorIsRefused(Driver &driver, FakePrimaLuce &device)
{
    device.takeWrites();

    Published published;
    driver.startCalibration();
    std::vector<json> writes = device.takeWrites();
    std::vector<std::string> states = published.states("FOCUS_CALIBRATION");
    std::vector<std::string> messages = published.messages();

    EXPECT_TRUE(writesTo(writes, "set", "MOT1", "CAL_STATUS").empty()) << "A calibration was started fully out";
    ASSERT_FALSE(states.empty());
    EXPECT_EQ(states.back(), "Alert");
    EXPECT_TRUE(anyContains(messages, "not fully in"));
}

// After an aborted calibration a calibration completed elsewhere (web interface, hand pad: CAL_NUMOF rose) also makes
// moves possible again.
template <class Driver>
void checkCalibrationElsewhereEndsTheLock(Driver &driver, FakePrimaLuce &device)
{
    driver.startCalibration();
    driver.poll();
    driver.abort();
    device.set("MOT1", "CAL_STATUS", "stop");
    device.takeWrites();
    driver.moveTo(30000);
    std::vector<json> locked = device.takeWrites();

    device.set("MOT1", "CAL_NUMOF", 18);
    driver.moveTo(30000);
    std::vector<json> unlocked = device.takeWrites();

    EXPECT_TRUE(writesTo(locked, "cmd", "MOT1", "MOVE_ABS").empty()) << "A move was sent after the abort";
    EXPECT_EQ(writesTo(unlocked, "cmd", "MOT1", "MOVE_ABS").size(), 1u)
            << "A calibration completed elsewhere did not end the lock";
}

// After an aborted calibration away from the sensor, moves are locked and the calibration needs the sensor: the
// messages name the way out (the web interface) instead of asking to move the focuser to its sensor.
template <class Driver>
void checkLockedAwayFromSensorNamesWayOut(Driver &driver, FakePrimaLuce &device)
{
    driver.startCalibration();
    driver.poll();
    driver.abort();
    device.set("MOT1", "CAL_STATUS", "stop");
    device.set("MOT1", "HSENDET", 0);
    device.takeWrites();

    Published published;
    driver.moveTo(30000);
    driver.startCalibration();
    std::vector<json> writes = device.takeWrites();
    std::vector<std::string> messages = published.messages();

    EXPECT_TRUE(writesTo(writes, "cmd", "MOT1", "MOVE_ABS").empty()) << "A move was sent after the abort";
    EXPECT_TRUE(writesTo(writes, "set", "MOT1", "CAL_STATUS").empty()) << "A calibration was started off the sensor";
    EXPECT_EQ(driver.switchState("FOCUS_CALIBRATION"), IPS_ALERT);
    EXPECT_TRUE(anyContains(messages, "moves are locked after an incomplete calibration"));
    EXPECT_FALSE(anyContains(messages, "Move the focuser fully in")) << "Asked for a move that is refused";
    EXPECT_TRUE(anyContains(messages, "ESATTO web interface")) << "No way out was named";
}

// A standard ESATTO 2" reports HSENDET 1 at both ends, and while moves are locked only the position, which the lock
// calls invalid, would tell them apart. At the outer end the web interface sends another request. So after an
// aborted calibration the driver refuses to calibrate it, also at the sensor in the inner half, and names only the
// web interface. Unlocked, it is calibrated at its sensor as before.
template <class Driver>
void checkLockedTwoSensorModelNamesOnlyWebInterface(Driver &driver, FakePrimaLuce &device)
{
    device.takeWrites();

    Published published;
    driver.startCalibration();
    std::vector<json> unlocked = device.takeWrites();
    driver.poll();
    driver.abort();
    device.set("MOT1", "CAL_STATUS", "stop");
    device.takeWrites();

    driver.moveTo(30000);
    driver.startCalibration();
    std::vector<json> locked = device.takeWrites();
    std::vector<std::string> messages = published.messages();

    EXPECT_EQ(writesTo(unlocked, "set", "MOT1", "CAL_STATUS").size(), 1u) << "Not calibrated at the sensor";
    EXPECT_TRUE(writesTo(locked, "cmd", "MOT1", "MOVE_ABS").empty()) << "A move was sent after the abort";
    EXPECT_TRUE(writesTo(locked, "set", "MOT1", "CAL_STATUS").empty())
            << "A model with a sensor at each end was calibrated while its position was invalid";
    EXPECT_EQ(driver.switchState("FOCUS_CALIBRATION"), IPS_ALERT);
    EXPECT_TRUE(anyContains(messages, "cannot tell at which end"));
    EXPECT_TRUE(anyContains(messages, "ESATTO web interface")) << "No way out was named";
    EXPECT_FALSE(anyContains(messages, "or here with the focuser")) << "Offered a calibration the driver refuses";
}

// A calibration that returned to "stop" without a higher CAL_NUMOF also leaves the position invalid: moves are refused.
template <class Driver>
void checkIncompleteCalibrationLocksMoves(Driver &driver, FakePrimaLuce &device)
{
    driver.startCalibration();
    driver.poll();
    device.set("MOT1", "CAL_STATUS", "stop");
    pollPastStartGrace(driver);
    device.takeWrites();
    driver.moveTo(30000);
    std::vector<json> writes = device.takeWrites();

    EXPECT_EQ(driver.switchState("FOCUS_CALIBRATION"), IPS_ALERT);
    EXPECT_TRUE(writesTo(writes, "cmd", "MOT1", "MOVE_ABS").empty()) << "A move was sent after an incomplete calibration";
}

// A move to the current position sends nothing to the focuser and ends Ok.
template <class Driver>
void checkMoveToCurrentPositionSendsNothing(Driver &driver, FakePrimaLuce &device)
{
    device.takeWrites();

    Published published;
    driver.moveTo(10000);
    std::vector<json> writes = device.takeWrites();
    std::vector<std::string> states = published.states("ABS_FOCUS_POSITION");

    EXPECT_TRUE(writesTo(writes, "cmd", "MOT1", "MOVE_ABS").empty()) << "A move to the current position was sent";
    ASSERT_FALSE(states.empty());
    EXPECT_EQ(states.back(), "Ok");
}

// A move back to the start while the focuser still moves, or right after an abort while it still brakes, is sent:
// the last polled position is not where the focuser ends. Without it the focuser stayed at the old target, and the
// move was reported Ok.
template <class Driver>
void checkMoveBackWhileMovingIsSent(Driver &driver, FakePrimaLuce &device)
{
    driver.moveTo(30000);
    device.set("MOT1", "STATUS", FakePrimaLuce::moving());
    driver.poll();
    device.takeWrites();

    Published whileMoving;
    driver.moveTo(10000);
    std::vector<json> movingWrites = device.takeWrites();
    std::vector<std::string> movingStates = whileMoving.states("ABS_FOCUS_POSITION");

    // Abort at 20000, the motor brakes on: the position property is no longer Busy, but the motor still moves
    device.set("MOT1", "ABS_POS_STEP", 20000);
    driver.poll();
    driver.abort();
    device.set("MOT1", "STATUS", {{"BUSY", 0}, {"MST", "dec"}});
    device.takeWrites();
    driver.moveTo(20000);
    std::vector<json> brakingWrites = device.takeWrites();

    // Stopped at 20400 after the abort, the last poll saw 20000
    device.set("MOT1", "ABS_POS_STEP", 20400);
    device.set("MOT1", "STATUS", FakePrimaLuce::stopped());
    driver.abort();
    device.takeWrites();
    driver.moveTo(20000);
    std::vector<json> stoppedWrites = device.takeWrites();

    std::vector<json> moves = writesTo(movingWrites, "cmd", "MOT1", "MOVE_ABS");
    ASSERT_EQ(moves.size(), 1u) << "A move back to the start was not sent while the focuser moved";
    EXPECT_EQ(moves[0]["STEP"], 10000);
    ASSERT_FALSE(movingStates.empty());
    EXPECT_EQ(movingStates.back(), "Busy") << "A move that was not sent was reported";
    EXPECT_EQ(writesTo(brakingWrites, "cmd", "MOT1", "MOVE_ABS").size(), 1u)
            << "A move to the last polled position was not sent while the motor braked after an abort";
    EXPECT_EQ(writesTo(stoppedWrites, "cmd", "MOT1", "MOVE_ABS").size(), 1u)
            << "A move to the last polled position was not sent although the focuser stopped elsewhere";
}

// Right after MOVE_ABS the focuser can still report BUSY 0, MST "stop" and the old position (the driver allows
// MOVE_START_GRACE_MS for the start). A move back to the start then is sent too: the motor is about to leave it.
template <class Driver>
void checkMoveBackBeforeMotorStartsIsSent(Driver &driver, FakePrimaLuce &device)
{
    driver.moveTo(30000);
    device.takeWrites();

    Published published;
    driver.moveTo(10000);
    std::vector<json> writes = device.takeWrites();
    std::vector<std::string> states = published.states("ABS_FOCUS_POSITION");

    std::vector<json> moves = writesTo(writes, "cmd", "MOT1", "MOVE_ABS");
    ASSERT_EQ(moves.size(), 1u) << "A move back to the start was not sent before the motor reported moving";
    EXPECT_EQ(moves[0]["STEP"], 10000);
    ASSERT_FALSE(states.empty());
    EXPECT_EQ(states.back(), "Busy") << "A move that was not sent was reported";
}

// A calibration the driver did not start (web interface, hand pad, or still running after an abort before a
// reconnect) also refuses moves and a new start.
template <class Driver>
void checkCalibrationStartedElsewhereBlocks(Driver &driver, FakePrimaLuce &device)
{
    device.takeWrites();

    Published published;
    driver.moveTo(30000);
    driver.startCalibration();
    std::vector<json> writes = device.takeWrites();
    std::vector<std::string> messages = published.messages();

    EXPECT_TRUE(writesTo(writes, "cmd", "MOT1", "MOVE_ABS").empty()) << "A move was sent into a running calibration";
    EXPECT_TRUE(writesTo(writes, "set", "MOT1", "CAL_STATUS").empty()) << "A second calibration was started";
    EXPECT_TRUE(anyContains(messages, "reports a running calibration"));
}

}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Esatto
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Reading the limits sets the position property to 0, so the position has to be read after them. Without the
// optional external probe the Esatto reports -127, which is shown as -273.15 like in the Arco driver.
TEST(EsattoTest, connectReportsPositionAndMissingProbe)
{
    FakePrimaLuce device;
    ASSERT_FALSE(device.portName().empty());
    EsattoUnderTest driver(ESATTO_NAME);

    Published published;
    bool connected = driver.connectTo(device.portName());
    std::vector<double> positions = published.values("ABS_FOCUS_POSITION", "FOCUS_ABSOLUTE_POSITION");
    std::vector<double> external = published.values("FOCUS_TEMPERATURE", "TEMPERATURE_ETX");

    ASSERT_TRUE(connected);
    ASSERT_FALSE(positions.empty());
    EXPECT_EQ(positions.back(), 10000) << "The position at connect is not the one of the device";
    ASSERT_FALSE(external.empty());
    EXPECT_DOUBLE_EQ(external.back(), -273.15) << "A missing external probe is shown as a temperature";
}

// A temperature reply that holds no number is no reading: the external temperature must not show the value of the
// motor temperature read before it.
TEST(EsattoTest, temperatureReplyWithoutNumberIsNoReading)
{
    FakePrimaLuce device;
    ASSERT_FALSE(device.portName().empty());
    device.set("", "EXT_T", "n/a");
    EsattoUnderTest driver(ESATTO_NAME);

    Published published;
    bool connected = driver.connectTo(device.portName());
    std::vector<double> external = published.values("FOCUS_TEMPERATURE", "TEMPERATURE_ETX");

    ASSERT_TRUE(connected);
    ASSERT_FALSE(external.empty());
    EXPECT_DOUBLE_EQ(external.back(), -273.15) << "\"n/a\" was taken as a temperature";
}

// A firmware version reply without SWAPP threw an uncaught JSON exception during the handshake.
TEST(EsattoTest, firmwareReplyWithoutSwappDoesNotThrow)
{
    FakePrimaLuce device;
    ASSERT_FALSE(device.portName().empty());
    device.set("", "SWVERS", {{"OTHER", 1}});
    EsattoUnderTest driver(ESATTO_NAME);

    bool connected = false;
    std::string exception;
    {
        Published published;
        try
        {
            connected = driver.connectTo(device.portName());
        }
        catch (const std::exception &e)
        {
            exception = e.what();
        }
    }

    EXPECT_EQ(exception, "") << "The handshake threw";
    EXPECT_TRUE(connected) << "The firmware version is optional, the connection must still work";
}

// A reply longer than the 4 kB read buffer was read into it without a limit and overwrote the stack. The driver must
// refuse it and stay alive. Runs in a child process, as the unfixed driver crashes.
TEST(EsattoTest, overlongReplyIsRefused)
{
    EXPECT_EXIT(
    {
        FakePrimaLuce device;
        device.replyRaw(std::string(6000, 'x') + "\r\n");
        EsattoUnderTest driver(ESATTO_NAME);
        bool connected = true;
        {
            Published published;
            connected = driver.connectTo(device.portName());
        }
        fflush(stdout);
        _exit(connected ? 1 : 0);
    }, ::testing::ExitedWithCode(0), "");
}

// MoveRelFocuser() turned the IPS_ALERT of MoveAbsFocuser() into a bool, so a refused move was reported busy.
TEST(EsattoTest, refusedRelativeMoveIsReported)
{
    FakePrimaLuce device;
    ASSERT_FALSE(device.portName().empty());
    EsattoUnderTest driver(ESATTO_NAME);
    ASSERT_TRUE(driver.connectTo(device.portName()));
    device.takeWrites();

    device.rejectCommands(true);
    Published published;
    driver.moveIn(100);
    std::vector<std::string> states = published.states("REL_FOCUS_POSITION");
    std::vector<json> moves = writesTo(device.takeWrites(), "cmd", "MOT1", "MOVE_ABS");

    ASSERT_EQ(moves.size(), 1u);
    EXPECT_EQ(moves[0], json({{"STEP", 9900}}));
    ASSERT_FALSE(states.empty());
    EXPECT_EQ(states.back(), "Alert") << "The refused relative move was reported as running";
}

// The motor still reports BUSY in the poll that sees it arrive, the next poll sees it stopped at the same position.
// The end of a move was only checked in a poll where the position changed, so the move stayed busy forever.
TEST(EsattoTest, moveEndsWithoutLastPositionChange)
{
    FakePrimaLuce device;
    ASSERT_FALSE(device.portName().empty());
    EsattoUnderTest driver(ESATTO_NAME);
    ASSERT_TRUE(driver.connectTo(device.portName()));

    Published published;
    driver.moveTo(20000);
    device.set("MOT1", "ABS_POS_STEP", 20000);
    device.set("MOT1", "STATUS", FakePrimaLuce::moving());
    driver.poll();
    device.set("MOT1", "STATUS", FakePrimaLuce::stopped());
    driver.poll();
    driver.poll();
    std::vector<std::string> states = published.states("ABS_FOCUS_POSITION");

    ASSERT_FALSE(states.empty());
    EXPECT_EQ(states.front(), "Busy");
    EXPECT_EQ(states.back(), "Ok") << "The move never ended";
}

// The Esatto reports BUSY 0 while MST is still "dec" and drives on for up to about 800 steps: still moving.
TEST(EsattoTest, brakingMotorIsBusy)
{
    FakePrimaLuce device;
    ASSERT_FALSE(device.portName().empty());
    EsattoUnderTest driver(ESATTO_NAME);
    ASSERT_TRUE(driver.connectTo(device.portName()));

    Published braking;
    driver.moveTo(20000);
    device.set("MOT1", "ABS_POS_STEP", 19184);
    device.set("MOT1", "STATUS", {{"BUSY", 0}, {"MST", "dec"}});
    driver.poll();
    std::vector<std::string> whileBraking = braking.states("ABS_FOCUS_POSITION");

    Published stopped;
    device.set("MOT1", "ABS_POS_STEP", 20000);
    device.set("MOT1", "STATUS", FakePrimaLuce::stopped());
    driver.poll();
    std::vector<std::string> states = stopped.states("ABS_FOCUS_POSITION");
    std::vector<double> positions = stopped.values("ABS_FOCUS_POSITION", "FOCUS_ABSOLUTE_POSITION");

    EXPECT_FALSE(contains(whileBraking, "Ok")) << "The move was reported done while the motor was still braking";
    ASSERT_FALSE(states.empty());
    EXPECT_EQ(states.back(), "Ok");
    ASSERT_FALSE(positions.empty());
    EXPECT_EQ(positions.back(), 20000);
}

// Like the web interface, the calibration is only started with the focuser on its sensor (HSENDET 1).
TEST(EsattoTest, calibrationNeedsTheSensor)
{
    FakePrimaLuce device;
    ASSERT_FALSE(device.portName().empty());
    EsattoUnderTest driver(ESATTO_NAME);
    ASSERT_TRUE(driver.connectTo(device.portName()));
    device.takeWrites();

    Published published;
    driver.startCalibration();
    std::vector<std::string> states = published.states("FOCUS_CALIBRATION");
    std::vector<json> calibrations = writesTo(device.takeWrites(), "set", "MOT1", "CAL_STATUS");

    EXPECT_TRUE(calibrations.empty()) << "The calibration was started away from the sensor";
    EXPECT_FALSE(contains(states, "Ok"));
}

// A calibration request before the first connection has no focuser to talk to: it is refused instead of crashing.
TEST(EsattoTest, calibrationBeforeConnectIsRefused)
{
    EsattoUnderTest driver(ESATTO_NAME);

    Published published;
    const bool handled = driver.startCalibration();
    std::vector<std::string> messages = published.messages();

    EXPECT_TRUE(handled);
    EXPECT_TRUE(anyContains(messages, "Connect the focuser")) << "No error about the missing connection";
}

// At the sensor the calibration starts with CAL_STATUS. While it runs the positions stay put and the Esatto reports
// the name of the phase in CAL_STATUS: a move and a second start are refused (a move shifted the zero position), and
// the calibration only ends once CAL_STATUS is "stop" again, also after the 2 s the driver allows a move to start.
TEST(EsattoTest, calibrationRunsUntilCalStatusStops)
{
    FakePrimaLuce device;
    ASSERT_FALSE(device.portName().empty());
    device.set("MOT1", "HSENDET", 1);
    EsattoUnderTest driver(ESATTO_NAME);
    ASSERT_TRUE(driver.connectTo(device.portName()));
    device.takeWrites();

    Published running;
    driver.startCalibration();
    std::vector<json> started = device.takeWrites();
    driver.poll();
    driver.moveTo(30000);
    driver.startCalibration();
    std::vector<json> refused = device.takeWrites();
    // A pause between two phases: the motor stands still, CAL_STATUS still names the phase.
    const auto start = std::chrono::steady_clock::now();
    while (std::chrono::steady_clock::now() - start < std::chrono::milliseconds(2300))
    {
        driver.poll();
        pause(100);
    }
    std::vector<std::string> whileRunning = running.states("FOCUS_CALIBRATION");

    Published done;
    device.set("MOT1", "CAL_STATUS", "stop");
    device.set("MOT1", "CAL_NUMOF", 18);
    device.set("MOT1", "ABS_POS_STEP", 816);
    driver.poll();
    std::vector<std::string> calibrationStates = done.states("FOCUS_CALIBRATION");
    std::vector<std::string> positionStates = done.states("ABS_FOCUS_POSITION");
    std::vector<double> positions = done.values("ABS_FOCUS_POSITION", "FOCUS_ABSOLUTE_POSITION");

    EXPECT_EQ(writesTo(started, "set", "MOT1", "CAL_STATUS"), std::vector<json> {CALIBRATION_PHASE});
    EXPECT_EQ(started.size(), 1u);
    EXPECT_TRUE(writesTo(refused, "cmd", "MOT1", "MOVE_ABS").empty()) << "A move was sent during the calibration";
    EXPECT_TRUE(writesTo(refused, "set", "MOT1", "CAL_STATUS").empty()) << "The calibration was started twice";
    ASSERT_FALSE(whileRunning.empty());
    EXPECT_EQ(whileRunning.front(), "Busy");
    EXPECT_FALSE(contains(whileRunning, "Ok")) << "The calibration ended while CAL_STATUS was " << CALIBRATION_PHASE;
    ASSERT_FALSE(calibrationStates.empty());
    EXPECT_EQ(calibrationStates.back(), "Ok") << "The calibration did not end with CAL_STATUS \"stop\"";
    ASSERT_FALSE(positionStates.empty());
    EXPECT_EQ(positionStates.back(), "Ok");
    ASSERT_FALSE(positions.empty());
    EXPECT_EQ(positions.back(), 816);
}

TEST(EsattoTest, completedCalibrationIsOk)
{
    FakePrimaLuce device;
    ASSERT_FALSE(device.portName().empty());
    device.set("MOT1", "HSENDET", 1);
    EsattoUnderTest driver(ESATTO_NAME);
    ASSERT_TRUE(driver.connectTo(device.portName()));
    checkCompletedCalibrationIsOk(driver, device);
}

TEST(EsattoTest, calibrationWithoutCountIsAlert)
{
    FakePrimaLuce device;
    ASSERT_FALSE(device.portName().empty());
    device.set("MOT1", "HSENDET", 1);
    EsattoUnderTest driver(ESATTO_NAME);
    ASSERT_TRUE(driver.connectTo(device.portName()));
    checkCalibrationWithoutCountIsAlert(driver, device);
}

TEST(EsattoTest, abortDuringCalibration)
{
    FakePrimaLuce device;
    ASSERT_FALSE(device.portName().empty());
    device.set("MOT1", "HSENDET", 1);
    EsattoUnderTest driver(ESATTO_NAME);
    ASSERT_TRUE(driver.connectTo(device.portName()));
    checkAbortDuringCalibration(driver, device);
}

TEST(EsattoTest, calibrationElsewhereEndsTheLock)
{
    FakePrimaLuce device;
    ASSERT_FALSE(device.portName().empty());
    device.set("MOT1", "HSENDET", 1);
    EsattoUnderTest driver(ESATTO_NAME);
    ASSERT_TRUE(driver.connectTo(device.portName()));
    checkCalibrationElsewhereEndsTheLock(driver, device);
}

TEST(EsattoTest, incompleteCalibrationLocksMoves)
{
    FakePrimaLuce device;
    ASSERT_FALSE(device.portName().empty());
    device.set("MOT1", "HSENDET", 1);
    EsattoUnderTest driver(ESATTO_NAME);
    ASSERT_TRUE(driver.connectTo(device.portName()));
    checkIncompleteCalibrationLocksMoves(driver, device);
}

TEST(EsattoTest, moveToCurrentPositionSendsNothing)
{
    FakePrimaLuce device;
    ASSERT_FALSE(device.portName().empty());
    EsattoUnderTest driver(ESATTO_NAME);
    ASSERT_TRUE(driver.connectTo(device.portName()));
    checkMoveToCurrentPositionSendsNothing(driver, device);
}

TEST(EsattoTest, lockedAwayFromSensorNamesWayOut)
{
    FakePrimaLuce device;
    ASSERT_FALSE(device.portName().empty());
    device.set("MOT1", "HSENDET", 1);
    EsattoUnderTest driver(ESATTO_NAME);
    ASSERT_TRUE(driver.connectTo(device.portName()));
    checkLockedAwayFromSensorNamesWayOut(driver, device);
}

TEST(EsattoTest, lockedTwoSensorModelNamesOnlyWebInterface)
{
    FakePrimaLuce device;
    ASSERT_FALSE(device.portName().empty());
    device.set("", "MODNAME", "ESATTO2");
    device.set("MOT1", "HSENDET", 1);
    EsattoUnderTest driver(ESATTO_NAME);
    ASSERT_TRUE(driver.connectTo(device.portName()));
    checkLockedTwoSensorModelNamesOnlyWebInterface(driver, device);
}

TEST(EsattoTest, moveBackBeforeMotorStartsIsSent)
{
    FakePrimaLuce device;
    ASSERT_FALSE(device.portName().empty());
    EsattoUnderTest driver(ESATTO_NAME);
    ASSERT_TRUE(driver.connectTo(device.portName()));
    checkMoveBackBeforeMotorStartsIsSent(driver, device);
}

TEST(EsattoTest, moveBackWhileMovingIsSent)
{
    FakePrimaLuce device;
    ASSERT_FALSE(device.portName().empty());
    EsattoUnderTest driver(ESATTO_NAME);
    ASSERT_TRUE(driver.connectTo(device.portName()));
    checkMoveBackWhileMovingIsSent(driver, device);
}

TEST(EsattoTest, outerSensorIsRefused)
{
    FakePrimaLuce device;
    ASSERT_FALSE(device.portName().empty());
    device.set("MOT1", "HSENDET", 1);
    device.set("MOT1", "ABS_POS_STEP", 400000);
    EsattoUnderTest driver(ESATTO_NAME);
    ASSERT_TRUE(driver.connectTo(device.portName()));
    checkOuterSensorIsRefused(driver, device);
}

TEST(EsattoTest, calibrationStartedElsewhereBlocks)
{
    FakePrimaLuce device;
    ASSERT_FALSE(device.portName().empty());
    device.set("MOT1", "HSENDET", 1);
    device.set("MOT1", "CAL_STATUS", CALIBRATION_PHASE);
    EsattoUnderTest driver(ESATTO_NAME);
    ASSERT_TRUE(driver.connectTo(device.portName()));
    checkCalibrationStartedElsewhereBlocks(driver, device);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Esatto Arco
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// The backlash stored in the focuser is read on connect. The Arco firmware element keeps its published name: element
// names are public API, so the old typo stays.
TEST(EsattoArcoTest, connectReadsBacklashAndNamesFirmware)
{
    FakePrimaLuce device;
    ASSERT_FALSE(device.portName().empty());
    auto arco = makeDriver<ArcoUnderTest>(ARCO_NAME);
    ArcoUnderTest &driver = *arco;

    Published published;
    bool connected = driver.connectTo(device.portName());
    std::vector<double> backlash = published.values("FOCUS_BACKLASH_STEPS", "FOCUS_BACKLASH_VALUE");
    std::vector<std::string> firmware = published.elements("FOCUS_FIRMWARE");

    ASSERT_TRUE(connected);
    ASSERT_FALSE(backlash.empty());
    EXPECT_EQ(backlash.back(), 120) << "The backlash of the focuser was not read";
    EXPECT_TRUE(contains(firmware, "VERARCO_FIRMWARE_VERSIONSION"))
            << "FOCUS_FIRMWARE no longer has its element VERARCO_FIRMWARE_VERSIONSION";
    EXPECT_FALSE(contains(firmware, "ARCO_FIRMWARE_VERSION")) << "The Arco firmware element was renamed";
}

// Without 12 V the Arco cannot rotate reliably, and an Arco the driver had to enable needs a calibration: both are
// reported at connect.
TEST(EsattoArcoTest, connectWarnsAboutPowerAndDisabledArco)
{
    FakePrimaLuce device;
    ASSERT_FALSE(device.portName().empty());
    device.set("", "VIN_12V", "0.0");
    device.set("", "ARCO", 0);
    auto arco = makeDriver<ArcoUnderTest>(ARCO_NAME);
    ArcoUnderTest &driver = *arco;

    Published published;
    bool connected = driver.connectTo(device.portName());
    std::vector<std::string> messages = published.messages();
    std::vector<json> enabled = writesTo(device.takeWrites(), "set", "", "ARCO");

    ASSERT_TRUE(connected);
    EXPECT_EQ(enabled, std::vector<json> {1});
    EXPECT_TRUE(anyContains(messages, "12v DC power supply")) << "No warning about the missing 12 V";
    EXPECT_TRUE(anyContains(messages, "has to be calibrated")) << "No warning about the Arco calibration";
}

// GoTo and Sync angles were cut to whole degrees although the Arco resolves one arc second.
TEST(EsattoArcoTest, rotatorAnglesKeepTheirFraction)
{
    FakePrimaLuce device;
    ASSERT_FALSE(device.portName().empty());
    auto arco = makeDriver<ArcoUnderTest>(ARCO_NAME);
    ArcoUnderTest &driver = *arco;
    ASSERT_TRUE(driver.connectTo(device.portName()));
    device.takeWrites();

    Published published;
    driver.sendNumber("ABS_ROTATOR_ANGLE", "ANGLE", 123.45);
    driver.sendSwitch("ROTATOR_ABORT_MOTION", {{"ABORT", ISS_ON}});
    driver.sendNumber("SYNC_ROTATOR_ANGLE", "ANGLE", 200.7);
    published.states("ABS_ROTATOR_ANGLE");
    std::vector<json> writes = device.takeWrites();
    std::vector<json> moves = writesTo(writes, "cmd", "MOT2", "MOVE_ABS");
    std::vector<json> syncs = writesTo(writes, "cmd", "MOT2", "SYNC_POS");

    ASSERT_EQ(moves.size(), 1u);
    ASSERT_TRUE(moves[0].contains("DEG"));
    EXPECT_NEAR(moves[0]["DEG"].get<double>(), 123.45, 1e-9) << "GoTo angle was cut";
    ASSERT_EQ(syncs.size(), 1u);
    ASSERT_TRUE(syncs[0].contains("DEG"));
    // Angles above 180 degrees go to the Arco as negative angles
    EXPECT_NEAR(syncs[0]["DEG"].get<double>(), 200.7 - 360, 1e-9) << "Sync angle was cut";
}

// A focuser move set the rotator steps property busy and never cleared it.
TEST(EsattoArcoTest, focuserMoveLeavesRotatorIdle)
{
    FakePrimaLuce device;
    ASSERT_FALSE(device.portName().empty());
    auto arco = makeDriver<ArcoUnderTest>(ARCO_NAME);
    ArcoUnderTest &driver = *arco;
    ASSERT_TRUE(driver.connectTo(device.portName()));

    Published published;
    driver.moveTo(20000);
    device.set("MOT1", "ABS_POS_STEP", 20000);
    driver.poll();
    std::vector<std::string> rotatorStates = published.states("ABS_ROTATOR_POSITION");
    std::vector<std::string> focuserStates = published.states("ABS_FOCUS_POSITION");

    EXPECT_FALSE(contains(rotatorStates, "Busy")) << "A focuser move set the rotator busy";
    EXPECT_NE(driver.numberState("ABS_ROTATOR_POSITION"), IPS_BUSY);
    ASSERT_FALSE(focuserStates.empty());
    EXPECT_EQ(focuserStates.back(), "Ok");
}

// Same as for the Esatto: a refused relative move is reported as refused.
TEST(EsattoArcoTest, refusedRelativeMoveIsReported)
{
    FakePrimaLuce device;
    ASSERT_FALSE(device.portName().empty());
    auto arco = makeDriver<ArcoUnderTest>(ARCO_NAME);
    ArcoUnderTest &driver = *arco;
    ASSERT_TRUE(driver.connectTo(device.portName()));
    device.takeWrites();

    device.rejectCommands(true);
    Published published;
    driver.moveIn(100);
    std::vector<std::string> states = published.states("REL_FOCUS_POSITION");
    std::vector<json> moves = writesTo(device.takeWrites(), "cmd", "MOT1", "MOVE_ABS");

    ASSERT_EQ(moves.size(), 1u);
    ASSERT_FALSE(states.empty());
    EXPECT_EQ(states.back(), "Alert") << "The refused relative move was reported as running";
}

// Same as for the Esatto: the move ends in a poll without a position change.
TEST(EsattoArcoTest, focusMoveEndsWithoutLastPositionChange)
{
    FakePrimaLuce device;
    ASSERT_FALSE(device.portName().empty());
    auto arco = makeDriver<ArcoUnderTest>(ARCO_NAME);
    ArcoUnderTest &driver = *arco;
    ASSERT_TRUE(driver.connectTo(device.portName()));

    Published published;
    driver.moveTo(20000);
    device.set("MOT1", "ABS_POS_STEP", 20000);
    device.set("MOT1", "STATUS", FakePrimaLuce::moving());
    driver.poll();
    device.set("MOT1", "STATUS", FakePrimaLuce::stopped());
    driver.poll();
    driver.poll();
    std::vector<std::string> states = published.states("ABS_FOCUS_POSITION");

    ASSERT_FALSE(states.empty());
    EXPECT_EQ(states.back(), "Ok") << "The move never ended";
}

// The positions were read before the motor status: a move that ended in a poll was reported with the position read
// while the motor was still braking. The driver reads the position again once the motor has stopped.
TEST(EsattoArcoTest, focusMoveReportsFinalPosition)
{
    FakePrimaLuce device;
    ASSERT_FALSE(device.portName().empty());
    auto arco = makeDriver<ArcoUnderTest>(ARCO_NAME);
    ArcoUnderTest &driver = *arco;
    ASSERT_TRUE(driver.connectTo(device.portName()));

    Published published;
    driver.moveTo(20000);
    device.set("MOT1", "ABS_POS_STEP", 19184);
    device.setAfterRead("MOT1", "STATUS", "MOT1", "ABS_POS_STEP", 20000);
    driver.poll();
    std::vector<std::string> states = published.states("ABS_FOCUS_POSITION");
    std::vector<double> positions = published.values("ABS_FOCUS_POSITION", "FOCUS_ABSOLUTE_POSITION");

    ASSERT_FALSE(states.empty());
    EXPECT_EQ(states.back(), "Ok");
    ASSERT_FALSE(positions.empty());
    EXPECT_EQ(positions.back(), 20000) << "The move was reported done at a position from the braking phase";
}

// FOCUS_SPEED always showed 0, it is updated during a move.
TEST(EsattoArcoTest, speedIsReportedDuringMove)
{
    FakePrimaLuce device;
    ASSERT_FALSE(device.portName().empty());
    auto arco = makeDriver<ArcoUnderTest>(ARCO_NAME);
    ArcoUnderTest &driver = *arco;
    ASSERT_TRUE(driver.connectTo(device.portName()));

    Published published;
    driver.moveTo(20000);
    device.set("MOT1", "ABS_POS_STEP", 15000);
    device.set("MOT1", "SPEED", 1200);
    device.set("MOT1", "STATUS", FakePrimaLuce::moving());
    driver.poll();
    std::vector<double> speeds = published.values("FOCUS_SPEED", "SPEED");

    ASSERT_FALSE(speeds.empty());
    EXPECT_EQ(speeds.back(), 1200) << "The motor speed was not updated during the move";
}

// Switching off both backlash switches indexed the property with -1 (an assertion in a debug build) and left the
// measurement busy. Runs in a child process, as the unfixed driver aborts. Exit code 2: the property was not idle.
TEST(EsattoArcoTest, backlashSwitchesOffDoNotStartMeasurement)
{
    EXPECT_EXIT(
    {
        FakePrimaLuce device;
        auto arco = makeDriver<ArcoUnderTest>(ARCO_NAME);
        std::vector<std::string> states;
        bool connected = false;
        {
            Published published;
            connected = arco->connectTo(device.portName());
            arco->sendSwitch("FOCUS_BACKLASH", {{"BACKLASH_START", ISS_OFF}, {"BACKLASH_NEXT", ISS_OFF}});
            states = published.states("FOCUS_BACKLASH");
        }
        fflush(stdout);
        _exit(!connected ? 1 : (states.empty() || states.back() != "Idle") ? 2 : 0);
    }, ::testing::ExitedWithCode(0), "");
}

// A backlash measurement towards larger positions gave a negative value, which went to the focuser as a huge
// unsigned number. The focuser gets the size of the backlash.
TEST(EsattoArcoTest, backlashMeasuredOutwardIsPositive)
{
    FakePrimaLuce device;
    ASSERT_FALSE(device.portName().empty());
    auto arco = makeDriver<ArcoUnderTest>(ARCO_NAME);
    ArcoUnderTest &driver = *arco;
    ASSERT_TRUE(driver.connectTo(device.portName()));
    device.takeWrites();

    Published published;
    driver.sendSwitch("FOCUS_BACKLASH", {{"BACKLASH_START", ISS_ON}, {"BACKLASH_NEXT", ISS_OFF}});
    device.set("MOT1", "ABS_POS_STEP", 20000);
    driver.poll();
    driver.sendSwitch("FOCUS_BACKLASH", {{"BACKLASH_START", ISS_OFF}, {"BACKLASH_NEXT", ISS_ON}});
    device.set("MOT1", "ABS_POS_STEP", 20500);
    driver.poll();
    driver.sendSwitch("FOCUS_BACKLASH", {{"BACKLASH_START", ISS_OFF}, {"BACKLASH_NEXT", ISS_ON}});
    std::vector<std::string> states = published.states("FOCUS_BACKLASH");
    std::vector<json> backlash = writesTo(device.takeWrites(), "set", "MOT1", "BKLASH");

    // Sent twice: once directly, once more by SetFocuserBacklashEnabled(true) of the base class
    ASSERT_FALSE(backlash.empty());
    for (const auto &steps : backlash)
        EXPECT_EQ(steps, 500) << "The focuser did not get the measured 500 steps";
    ASSERT_FALSE(states.empty());
    EXPECT_EQ(states.back(), "Ok");
}

// Without the optional external probe a change of the motor temperature was never sent: the driver compared only
// the external temperature, which stays at -273.15.
TEST(EsattoArcoTest, motorTemperatureChangeIsSentWithoutProbe)
{
    FakePrimaLuce device;
    ASSERT_FALSE(device.portName().empty());
    auto arco = makeDriver<ArcoUnderTest>(ARCO_NAME);
    ArcoUnderTest &driver = *arco;
    ASSERT_TRUE(driver.connectTo(device.portName()));

    Published published;
    device.set("MOT1", "NTC_T", "23.0");
    // The temperature is read every 11th poll
    for (int i = 0; i < 11; i++)
        driver.poll();
    std::vector<double> temperatures = published.values("FOCUS_TEMPERATURE", "TEMPERATURE");

    ASSERT_FALSE(temperatures.empty()) << "The changed motor temperature was not sent";
    EXPECT_DOUBLE_EQ(temperatures.back(), 23.0);
}

// The Arco reports BUSY 0 while MST is still "dec": the rotator is still moving although it is at the target.
TEST(EsattoArcoTest, brakingRotatorIsBusy)
{
    FakePrimaLuce device;
    ASSERT_FALSE(device.portName().empty());
    auto arco = makeDriver<ArcoUnderTest>(ARCO_NAME);
    ArcoUnderTest &driver = *arco;
    ASSERT_TRUE(driver.connectTo(device.portName()));

    Published braking;
    driver.sendNumber("ABS_ROTATOR_ANGLE", "ANGLE", 90);
    device.set("MOT2", "POSITION_STEP", 324000);
    device.set("MOT2", "POSITION_DEG", 90.0);
    device.set("MOT2", "STATUS", {{"BUSY", 0}, {"MST", "dec"}});
    driver.poll();
    std::vector<std::string> whileBraking = braking.states("ABS_ROTATOR_ANGLE");

    Published stopped;
    device.set("MOT2", "STATUS", FakePrimaLuce::stopped());
    driver.poll();
    std::vector<std::string> states = stopped.states("ABS_ROTATOR_ANGLE");

    EXPECT_FALSE(contains(whileBraking, "Ok")) << "The rotator was reported done while still braking";
    ASSERT_FALSE(states.empty());
    EXPECT_EQ(states.back(), "Ok");
}

// The Arco can report a stop 0.2 degrees short of the target and correct the rest a moment later: the move only ends
// at the target (or 3 s after the stop).
TEST(EsattoArcoTest, rotatorStopShortOfTargetStaysBusy)
{
    FakePrimaLuce device;
    ASSERT_FALSE(device.portName().empty());
    auto arco = makeDriver<ArcoUnderTest>(ARCO_NAME);
    ArcoUnderTest &driver = *arco;
    ASSERT_TRUE(driver.connectTo(device.portName()));

    Published shortStop;
    driver.sendNumber("ABS_ROTATOR_ANGLE", "ANGLE", 90);
    device.set("MOT2", "POSITION_STEP", 162000);
    device.set("MOT2", "POSITION_DEG", 45.0);
    device.set("MOT2", "STATUS", FakePrimaLuce::moving());
    driver.poll();
    device.set("MOT2", "POSITION_STEP", 323208);
    device.set("MOT2", "POSITION_DEG", 89.78);
    device.set("MOT2", "STATUS", FakePrimaLuce::stopped());
    driver.poll();
    std::vector<std::string> beforeTarget = shortStop.states("ABS_ROTATOR_ANGLE");

    Published atTarget;
    device.set("MOT2", "POSITION_STEP", 324000);
    device.set("MOT2", "POSITION_DEG", 90.0);
    driver.poll();
    std::vector<std::string> states = atTarget.states("ABS_ROTATOR_ANGLE");
    std::vector<double> angles = atTarget.values("ABS_ROTATOR_ANGLE", "ANGLE");

    EXPECT_FALSE(contains(beforeTarget, "Ok")) << "The rotator move ended 0.22 degrees short of the target";
    ASSERT_FALSE(states.empty());
    EXPECT_EQ(states.back(), "Ok");
    ASSERT_FALSE(angles.empty());
    EXPECT_DOUBLE_EQ(angles.back(), 90.0);
}

// A move in steps (ABS_ROTATOR_POSITION) ends when the Arco stops at the target steps. It used to compare the angle
// with the target of the last angle move, so it only ended after the 3 s settle time, with a warning.
TEST(EsattoArcoTest, rotatorStepMoveEndsAtTarget)
{
    FakePrimaLuce device;
    ASSERT_FALSE(device.portName().empty());
    auto arco = makeDriver<ArcoUnderTest>(ARCO_NAME);
    ArcoUnderTest &driver = *arco;
    ASSERT_TRUE(driver.connectTo(device.portName()));
    device.takeWrites();

    Published published;
    driver.sendNumber("ABS_ROTATOR_POSITION", "ROTATOR_ABSOLUTE_POSITION", 50000);
    std::vector<json> moves = writesTo(device.takeWrites(), "cmd", "MOT2", "MOVE_ABS");
    device.set("MOT2", "POSITION_STEP", 25000);
    device.set("MOT2", "POSITION_DEG", 4.39453);
    device.set("MOT2", "STATUS", FakePrimaLuce::moving());
    driver.poll();
    device.set("MOT2", "POSITION_STEP", 50000);
    device.set("MOT2", "POSITION_DEG", 8.78906);
    device.set("MOT2", "STATUS", FakePrimaLuce::stopped());
    driver.poll();
    std::vector<std::string> states = published.states("ABS_ROTATOR_POSITION");
    std::vector<std::string> messages = published.messages();

    ASSERT_EQ(moves.size(), 1u) << "The move in steps was not sent";
    ASSERT_FALSE(states.empty());
    EXPECT_EQ(states.back(), "Ok") << "The move in steps did not end at its target";
    EXPECT_FALSE(anyContains(messages, "Arco stopped")) << "A move that reached its target was reported as short";
}

// The Arco reports position 0 during its whole calibration, so waiting for a position change kept it busy forever.
// The end comes from CAL_STATUS ("exec" until "stop"), then the driver syncs the Arco to 0.
TEST(EsattoArcoTest, arcoCalibrationEndsWithCalStatus)
{
    FakePrimaLuce device;
    ASSERT_FALSE(device.portName().empty());
    auto arco = makeDriver<ArcoUnderTest>(ARCO_NAME);
    ArcoUnderTest &driver = *arco;
    ASSERT_TRUE(driver.connectTo(device.portName()));
    device.takeWrites();

    Published published;
    driver.sendSwitch("ARCO_CALIBRATION", {{"ARCO_CALIBRATION_START", ISS_ON}});
    std::vector<json> started = writesTo(device.takeWrites(), "set", "MOT2", "CAL_STATUS");
    // A pause between two phases
    for (int i = 0; i < 3; i++)
    {
        driver.poll();
        pause(100);
    }
    const bool busyWhileRunning = driver.numberState("ABS_ROTATOR_ANGLE") == IPS_BUSY;

    device.set("MOT2", "CAL_STATUS", "stop");
    // The driver allows 2 s for a move to start
    const auto start = std::chrono::steady_clock::now();
    while (driver.numberState("ABS_ROTATOR_ANGLE") == IPS_BUSY &&
            std::chrono::steady_clock::now() - start < std::chrono::seconds(5))
    {
        driver.poll();
        pause(100);
    }
    std::vector<std::string> states = published.states("ABS_ROTATOR_ANGLE");
    std::vector<json> syncs = writesTo(device.takeWrites(), "cmd", "MOT2", "SYNC_POS");

    EXPECT_EQ(started, std::vector<json> {"exec"});
    EXPECT_TRUE(busyWhileRunning);
    ASSERT_FALSE(states.empty());
    EXPECT_EQ(states.back(), "Ok") << "The Arco calibration never ended";
    EXPECT_EQ(syncs, std::vector<json> {json({{"STEP", 0}})});
    EXPECT_NE(driver.switchState("ARCO_CALIBRATION"), IPS_BUSY);
}

// The focuser calibration of the Esatto Arco driver also needs the sensor.
TEST(EsattoArcoTest, focusCalibrationNeedsTheSensor)
{
    FakePrimaLuce device;
    ASSERT_FALSE(device.portName().empty());
    auto arco = makeDriver<ArcoUnderTest>(ARCO_NAME);
    ArcoUnderTest &driver = *arco;
    ASSERT_TRUE(driver.connectTo(device.portName()));
    device.takeWrites();

    Published published;
    driver.startCalibration();
    std::vector<std::string> states = published.states("FOCUS_CALIBRATION");
    std::vector<json> calibrations = writesTo(device.takeWrites(), "set", "MOT1", "CAL_STATUS");

    EXPECT_TRUE(calibrations.empty()) << "The calibration was started away from the sensor";
    EXPECT_FALSE(contains(states, "Ok"));
}

// A focuser calibration request before the first connection is refused instead of crashing.
TEST(EsattoArcoTest, focusCalibrationBeforeConnectIsRefused)
{
    auto arco = makeDriver<ArcoUnderTest>(ARCO_NAME);
    ArcoUnderTest &driver = *arco;

    Published published;
    const bool handled = driver.startCalibration();
    std::vector<std::string> messages = published.messages();

    EXPECT_TRUE(handled);
    EXPECT_TRUE(anyContains(messages, "Connect the focuser")) << "No error about the missing connection";
}

// At the sensor it starts with CAL_STATUS, and a move during the calibration is refused.
TEST(EsattoArcoTest, focusCalibrationBlocksMoves)
{
    FakePrimaLuce device;
    ASSERT_FALSE(device.portName().empty());
    device.set("MOT1", "HSENDET", 1);
    auto arco = makeDriver<ArcoUnderTest>(ARCO_NAME);
    ArcoUnderTest &driver = *arco;
    ASSERT_TRUE(driver.connectTo(device.portName()));
    device.takeWrites();

    Published published;
    driver.startCalibration();
    std::vector<json> started = device.takeWrites();
    driver.poll();
    driver.moveTo(30000);
    driver.startCalibration();
    std::vector<json> refused = device.takeWrites();
    std::vector<std::string> states = published.states("FOCUS_CALIBRATION");

    EXPECT_EQ(writesTo(started, "set", "MOT1", "CAL_STATUS"), std::vector<json> {CALIBRATION_PHASE});
    EXPECT_EQ(started.size(), 1u);
    EXPECT_TRUE(writesTo(refused, "cmd", "MOT1", "MOVE_ABS").empty()) << "A move was sent during the calibration";
    EXPECT_TRUE(writesTo(refused, "set", "MOT1", "CAL_STATUS").empty()) << "The calibration was started twice";
    ASSERT_FALSE(states.empty());
    EXPECT_EQ(states.back(), "Busy");
}

TEST(EsattoArcoTest, completedFocusCalibrationIsOk)
{
    FakePrimaLuce device;
    ASSERT_FALSE(device.portName().empty());
    device.set("MOT1", "HSENDET", 1);
    auto arco = makeDriver<ArcoUnderTest>(ARCO_NAME);
    ASSERT_TRUE(arco->connectTo(device.portName()));
    checkCompletedCalibrationIsOk(*arco, device);
}

TEST(EsattoArcoTest, focusCalibrationWithoutCountIsAlert)
{
    FakePrimaLuce device;
    ASSERT_FALSE(device.portName().empty());
    device.set("MOT1", "HSENDET", 1);
    auto arco = makeDriver<ArcoUnderTest>(ARCO_NAME);
    ASSERT_TRUE(arco->connectTo(device.portName()));
    checkCalibrationWithoutCountIsAlert(*arco, device);
}

TEST(EsattoArcoTest, abortDuringFocusCalibration)
{
    FakePrimaLuce device;
    ASSERT_FALSE(device.portName().empty());
    device.set("MOT1", "HSENDET", 1);
    auto arco = makeDriver<ArcoUnderTest>(ARCO_NAME);
    ASSERT_TRUE(arco->connectTo(device.portName()));
    checkAbortDuringCalibration(*arco, device);
}

TEST(EsattoArcoTest, calibrationElsewhereEndsTheLock)
{
    FakePrimaLuce device;
    ASSERT_FALSE(device.portName().empty());
    device.set("MOT1", "HSENDET", 1);
    auto arco = makeDriver<ArcoUnderTest>(ARCO_NAME);
    ASSERT_TRUE(arco->connectTo(device.portName()));
    checkCalibrationElsewhereEndsTheLock(*arco, device);
}

TEST(EsattoArcoTest, incompleteCalibrationLocksMoves)
{
    FakePrimaLuce device;
    ASSERT_FALSE(device.portName().empty());
    device.set("MOT1", "HSENDET", 1);
    auto arco = makeDriver<ArcoUnderTest>(ARCO_NAME);
    ASSERT_TRUE(arco->connectTo(device.portName()));
    checkIncompleteCalibrationLocksMoves(*arco, device);
}

TEST(EsattoArcoTest, moveToCurrentPositionSendsNothing)
{
    FakePrimaLuce device;
    ASSERT_FALSE(device.portName().empty());
    auto arco = makeDriver<ArcoUnderTest>(ARCO_NAME);
    ASSERT_TRUE(arco->connectTo(device.portName()));
    checkMoveToCurrentPositionSendsNothing(*arco, device);
}

TEST(EsattoArcoTest, lockedAwayFromSensorNamesWayOut)
{
    FakePrimaLuce device;
    ASSERT_FALSE(device.portName().empty());
    device.set("MOT1", "HSENDET", 1);
    auto arco = makeDriver<ArcoUnderTest>(ARCO_NAME);
    ASSERT_TRUE(arco->connectTo(device.portName()));
    checkLockedAwayFromSensorNamesWayOut(*arco, device);
}

TEST(EsattoArcoTest, lockedTwoSensorModelNamesOnlyWebInterface)
{
    FakePrimaLuce device;
    ASSERT_FALSE(device.portName().empty());
    device.set("", "MODNAME", "ESATTO2");
    device.set("MOT1", "HSENDET", 1);
    auto arco = makeDriver<ArcoUnderTest>(ARCO_NAME);
    ASSERT_TRUE(arco->connectTo(device.portName()));
    checkLockedTwoSensorModelNamesOnlyWebInterface(*arco, device);
}

TEST(EsattoArcoTest, moveBackBeforeMotorStartsIsSent)
{
    FakePrimaLuce device;
    ASSERT_FALSE(device.portName().empty());
    auto arco = makeDriver<ArcoUnderTest>(ARCO_NAME);
    ASSERT_TRUE(arco->connectTo(device.portName()));
    checkMoveBackBeforeMotorStartsIsSent(*arco, device);
}

TEST(EsattoArcoTest, moveBackWhileMovingIsSent)
{
    FakePrimaLuce device;
    ASSERT_FALSE(device.portName().empty());
    auto arco = makeDriver<ArcoUnderTest>(ARCO_NAME);
    ASSERT_TRUE(arco->connectTo(device.portName()));
    checkMoveBackWhileMovingIsSent(*arco, device);
}

TEST(EsattoArcoTest, outerSensorIsRefused)
{
    FakePrimaLuce device;
    ASSERT_FALSE(device.portName().empty());
    device.set("MOT1", "HSENDET", 1);
    device.set("MOT1", "ABS_POS_STEP", 400000);
    auto arco = makeDriver<ArcoUnderTest>(ARCO_NAME);
    ASSERT_TRUE(arco->connectTo(device.portName()));
    checkOuterSensorIsRefused(*arco, device);
}

TEST(EsattoArcoTest, focusCalibrationStartedElsewhereBlocks)
{
    FakePrimaLuce device;
    ASSERT_FALSE(device.portName().empty());
    device.set("MOT1", "HSENDET", 1);
    device.set("MOT1", "CAL_STATUS", CALIBRATION_PHASE);
    auto arco = makeDriver<ArcoUnderTest>(ARCO_NAME);
    ASSERT_TRUE(arco->connectTo(device.portName()));
    checkCalibrationStartedElsewhereBlocks(*arco, device);
}

int main(int argc, char **argv)
{
    // Never touch the user's real ~/.indi or driver configuration: HOME and INDICONFIG point to a temporary folder.
    // The death tests run in a child process started with the same environment, it uses the folder of its parent.
    const char *ENV_DIR = "TEST_PRIMALUCA_ESATTO_ARCO_DIR";
    std::string dir;
    bool ownsDir = false;
    if (getenv(ENV_DIR) != nullptr)
        dir = getenv(ENV_DIR);
    else
    {
        char dirTemplate[] = "/tmp/test_primaluca_esatto_arco_XXXXXX";
        if (mkdtemp(dirTemplate) == nullptr)
            return 1;
        dir = dirTemplate;
        ownsDir = true;
        setenv(ENV_DIR, dir.c_str(), 1);
    }
    setenv("HOME", dir.c_str(), 1);
    setenv("INDICONFIG", (dir + "/config.xml").c_str(), 1);

    // Warnings reach the client as messages, some tests check them.
    INDI::Logger::getInstance().configure("", INDI::Logger::file_off,
                                          INDI::Logger::DBG_ERROR, INDI::Logger::DBG_ERROR | INDI::Logger::DBG_WARNING);
    // The death tests run in a fresh process, not in a fork of one that already ran fake devices.
    GTEST_FLAG_SET(death_test_style, "threadsafe");
    ::testing::InitGoogleTest(&argc, argv);
    int rc = RUN_ALL_TESTS();
    if (ownsDir)
    {
        std::error_code error;
        std::filesystem::remove_all(dir, error);
    }
    return rc;
}
