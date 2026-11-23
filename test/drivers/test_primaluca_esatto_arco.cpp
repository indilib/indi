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
    - Assumed: the error reply to a refused command ({"ERROR": ...}), temperatures and voltages as strings, EXT_T
      "-127.0" without the external probe (the value the driver checks for), the backlash 120, CAL_MAXPOS 406929, the
      Arco limits of +-648000 steps (one arc second per step), the serial numbers, STATUS with only BUSY and MST, and
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
                        {"SN", "ESATTO2LP0027"}, {"SWVERS", {{"SWAPP", "3.05.28"}}}, {"EXT_T", "-127.0"},
                        {"VIN_12V", "12.1"}, {"VIN_USB", "5.0"}, {"ARCO", 1}, {"ARCO_SN", "ARCO20001"}
                    }
                },
                {
                    "MOT1", {
                        {"ABS_POS_STEP", 10000}, {"CAL_MAXPOS", 406929}, {"BKLASH", 120}, {"NTC_T", "21.5"},
                        {"SPEED", 0}, {"HSENDET", 0}, {"CAL_STATUS", "stop"}, {"STATUS", stopped()}
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

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Esatto Arco
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// The backlash stored in the focuser is read on connect, and the Arco firmware element has its proper name.
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
    EXPECT_TRUE(contains(firmware, "ARCO_FIRMWARE_VERSION")) << "FOCUS_FIRMWARE has no element ARCO_FIRMWARE_VERSION";
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
