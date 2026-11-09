/*
    Regression tests for the PlaneWave EFA focuser driver: connecting and loading the configuration must not move
    the focuser. MTR_PMSLEW_RATE (0x24) and MTR_NMSLEW_RATE (0x25) are move commands in the EFA protocol (move
    positive/negative at speed 0-9 until the slew limit, 0 = stop), not a speed setting, and the EFA answers every
    packet with an acknowledgement. A driver that sends them to "read" or "set" a speed starts the motor.

    The driver talks to a fake EFA on a pseudo terminal. Its replies are copied from a log of a real EFA
    (firmware 1.6) on a PlaneWave CDK17.

    SPDX-License-Identifier: LGPL-2.1-or-later
*/

#include <gtest/gtest.h>

#include <atomic>
#include <cerrno>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <fstream>
#include <mutex>
#include <pty.h>
#include <regex>
#include <string>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/syscall.h>
#include <termios.h>
#include <thread>
#include <unistd.h>
#include <vector>

#include "indilogger.h"
#include "drivers/focuser/planewave_efa.h"

// The driver waits for CTS to clear and toggles RTS (ioctl TIOCMGET/TIOCMBIS/TIOCMBIC) around every packet.
// A pseudo terminal has no modem lines and rejects these calls, so report an idle bus (CTS clear) for them.
// Every other call goes to the kernel unchanged.
extern "C" int ioctl(int fd, unsigned long request, ...) __THROW
{
    va_list args;
    va_start(args, request);
    void *argument = va_arg(args, void *);
    va_end(args);

    long rc = syscall(SYS_ioctl, fd, request, argument);
    if (rc < 0 && errno == ENOTTY && (request == TIOCMGET || request == TIOCMBIS || request == TIOCMBIC))
    {
        if (request == TIOCMGET && argument != nullptr)
            *static_cast<int *>(argument) = 0;
        errno = 0;
        return 0;
    }
    return static_cast<int>(rc);
}

namespace
{

const char *DEVICE_NAME = "Mock EFA";
const uint32_t START_POSITION = 0x12F91B; // 1243419, as read from the real EFA

struct Packet
{
    uint8_t receiver;
    uint8_t command;
    std::vector<uint8_t> data;
};

bool isMotionCommand(uint8_t command)
{
    switch (command)
    {
        case EFA::MTR_PMSLEW_RATE:
        case EFA::MTR_NMSLEW_RATE:
        case EFA::MTR_GOTO_POS2:
        case 0x02: // GOTO fast
        case 0x06: // track positive
        case 0x07: // track negative
            return true;
        default:
            return false;
    }
}

// Fake EFA: echoes every packet (the bus echo the driver expects) and answers it like the real controller.
class FakeEFA
{
    public:
        FakeEFA()
        {
            if (openpty(&m_Master, &m_Slave, nullptr, nullptr, nullptr) != 0)
                return;
            makeRaw(m_Master);
            makeRaw(m_Slave);
            m_Thread = std::thread(&FakeEFA::serve, this);
        }

        ~FakeEFA()
        {
            m_Stop = true;
            if (m_Thread.joinable())
                m_Thread.join();
            if (m_Slave >= 0)
                close(m_Slave);
            if (m_Master >= 0)
                close(m_Master);
        }

        std::string port() const
        {
            const char *name = (m_Slave >= 0) ? ttyname(m_Slave) : nullptr;
            return name ? name : "";
        }

        std::vector<Packet> packets()
        {
            std::lock_guard<std::mutex> lock(m_Mutex);
            return m_Packets;
        }

        void clearPackets()
        {
            std::lock_guard<std::mutex> lock(m_Mutex);
            m_Packets.clear();
        }

        bool moving()
        {
            std::lock_guard<std::mutex> lock(m_Mutex);
            return m_Moving;
        }

        uint32_t position()
        {
            std::lock_guard<std::mutex> lock(m_Mutex);
            return m_Position;
        }

        std::vector<Packet> motionCommands()
        {
            std::vector<Packet> result;
            for (const auto &packet : packets())
                if (isMotionCommand(packet.command))
                    result.push_back(packet);
            return result;
        }

        bool received(uint8_t command)
        {
            for (const auto &packet : packets())
                if (packet.command == command)
                    return true;
            return false;
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
            std::vector<uint8_t> buffer;
            while (!m_Stop)
            {
                fd_set readSet;
                FD_ZERO(&readSet);
                FD_SET(m_Master, &readSet);
                struct timeval timeout = {0, 20000};
                if (select(m_Master + 1, &readSet, nullptr, nullptr, &timeout) <= 0)
                    continue;

                uint8_t chunk[256];
                ssize_t n = read(m_Master, chunk, sizeof(chunk));
                if (n <= 0)
                    continue;
                buffer.insert(buffer.end(), chunk, chunk + n);

                // Packet: SOM, LEN, SRC, RCV, CMD, DATA (LEN - 3 bytes), CHECKSUM
                while (true)
                {
                    while (!buffer.empty() && buffer[0] != 0x3B)
                        buffer.erase(buffer.begin());
                    if (buffer.size() < 2 || buffer.size() < static_cast<size_t>(buffer[1]) + 3)
                        break;
                    std::vector<uint8_t> packet(buffer.begin(), buffer.begin() + buffer[1] + 3);
                    buffer.erase(buffer.begin(), buffer.begin() + packet.size());
                    answer(packet);
                }
            }
        }

        std::vector<uint8_t> replyData(const Packet &packet)
        {
            std::lock_guard<std::mutex> lock(m_Mutex);
            m_Packets.push_back(packet);

            switch (packet.command)
            {
                case EFA::GET_VERSION:
                    return {0x01, 0x06};
                case EFA::MTR_GET_POS:
                    if (m_Moving)
                        m_Position += 500;
                    return {static_cast<uint8_t>(m_Position >> 16), static_cast<uint8_t>(m_Position >> 8),
                            static_cast<uint8_t>(m_Position)};
                case EFA::MTR_GET_CALIBRATION_STATE:
                    return {0x01};
                case EFA::FANS_GET:
                    return {0x03};
                case EFA::TEMP_GET:
                    return (!packet.data.empty() && packet.data[0] == 0) ? std::vector<uint8_t> {0xEC, 0x00} :
                           std::vector<uint8_t> {0xF4, 0x00};
                case EFA::MTR_GET_APPROACH_DIRECTION:
                case EFA::MTR_GET_STOP_DETECT:
                    return {0x01};
                case EFA::MTR_SLEWLIMITGETMAX:
                    return {0x96, 0xC1, 0x6C};
                case EFA::MTR_GOTO_OVER:
                    return {0xFF};
                case EFA::MTR_PMSLEW_RATE:
                case EFA::MTR_NMSLEW_RATE:
                    // The real EFA started moving on a packet without a speed byte too.
                    m_Moving = packet.data.empty() || packet.data[0] != 0;
                    return {0x01};
                case EFA::MTR_GOTO_POS2:
                    if (packet.data.size() >= 3)
                        m_Position = packet.data[0] << 16 | packet.data[1] << 8 | packet.data[2];
                    m_Moving = false;
                    return {0x01};
                default:
                    // The EFA acknowledges every packet, also unknown ones.
                    return {0x01};
            }
        }

        void answer(const std::vector<uint8_t> &request)
        {
            Packet packet;
            packet.receiver = request[3];
            packet.command  = request[4];
            packet.data.assign(request.begin() + 5, request.end() - 1);

            std::vector<uint8_t> data = replyData(packet);

            std::vector<uint8_t> reply = {0x3B, static_cast<uint8_t>(3 + data.size()), packet.receiver, EFA::DEVICE_PC,
                                          packet.command
                                         };
            reply.insert(reply.end(), data.begin(), data.end());
            int sum = 0;
            for (size_t i = 1; i < reply.size(); i++)
                sum += reply[i];
            reply.push_back(static_cast<uint8_t>((-sum) & 0xFF));

            // Bus echo first, then the answer
            std::vector<uint8_t> out(request);
            out.insert(out.end(), reply.begin(), reply.end());
            if (write(m_Master, out.data(), out.size()) < 0)
                return;
        }

        int m_Master {-1};
        int m_Slave {-1};
        std::thread m_Thread;
        std::atomic<bool> m_Stop {false};
        std::mutex m_Mutex;
        std::vector<Packet> m_Packets;
        uint32_t m_Position {START_POSITION};
        bool m_Moving {false};
};

// Captures what the driver publishes on stdout. gtest reports on stdout too, so call finish() before checking.
class PublishedXML
{
    public:
        PublishedXML()
        {
            fflush(stdout);
            m_File = tmpfile();
            m_SavedStdout = dup(STDOUT_FILENO);
            dup2(fileno(m_File), STDOUT_FILENO);
        }

        ~PublishedXML()
        {
            restore();
            if (m_File)
                fclose(m_File);
        }

        // Stops capturing and returns the captured XML.
        std::string finish()
        {
            restore();
            std::string result;
            rewind(m_File);
            char chunk[4096];
            size_t n;
            while ((n = fread(chunk, 1, sizeof(chunk), m_File)) > 0)
                result.append(chunk, n);
            return result;
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

// Drives the driver the way a client (Ekos) does: set the port, connect, then load the configuration.
class EFAUnderTest : public EFA
{
    public:
        EFAUnderTest()
        {
            setDeviceName(DEVICE_NAME);
            ISGetProperties(nullptr);
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

        void loadConfiguration()
        {
            ISState states[1] = {ISS_ON};
            char load[] = "CONFIG_LOAD";
            char *names[1] = {load};
            ISNewSwitch(getDeviceName(), "CONFIG_PROCESS", states, names, 1);
        }

        void requestNumber(const char *property, const char *element, double value)
        {
            double values[1] = {value};
            std::vector<char> elementName(element, element + strlen(element) + 1);
            char *names[1] = {elementName.data()};
            ISNewNumber(getDeviceName(), property, values, names, 1);
        }

        void requestAbort()
        {
            ISState states[1] = {ISS_ON};
            char abort[] = "ABORT";
            char *names[1] = {abort};
            ISNewSwitch(getDeviceName(), "FOCUS_ABORT_MOTION", states, names, 1);
        }
};

std::string configFile()
{
    return "/tmp/test_planewave_efa_" + std::to_string(getpid()) + "_config.xml";
}

// A saved configuration like the one the driver writes itself.
void writeConfiguration()
{
    std::ofstream config(configFile());
    config << "<INDIDriver>\n"
           << "<newNumberVector device='" << DEVICE_NAME << "' name='FOCUS_MAX'>\n"
           << "  <oneNumber name='FOCUS_MAX_VALUE'>9000000</oneNumber>\n"
           << "</newNumberVector>\n"
           << "<newSwitchVector device='" << DEVICE_NAME << "' name='FOCUS_REVERSE_MOTION'>\n"
           << "  <oneSwitch name='INDI_ENABLED'>Off</oneSwitch>\n"
           << "  <oneSwitch name='INDI_DISABLED'>On</oneSwitch>\n"
           << "</newSwitchVector>\n"
           << "<newSwitchVector device='" << DEVICE_NAME << "' name='FOCUS_FAN_CONTROL'>\n"
           << "  <oneSwitch name='FAN_MANUAL'>On</oneSwitch>\n"
           << "  <oneSwitch name='FAN_AUTOMATIC_ABSOLUTE'>Off</oneSwitch>\n"
           << "  <oneSwitch name='FAN_AUTOMATIC_RELATIVE'>Off</oneSwitch>\n"
           << "</newSwitchVector>\n"
           << "<newSwitchVector device='" << DEVICE_NAME << "' name='FOCUS_FAN_DISCONNECT'>\n"
           << "  <oneSwitch name='FAN_OFF_ON_DISCONNECT'>On</oneSwitch>\n"
           << "</newSwitchVector>\n"
           << "</INDIDriver>\n";
}

// The XML uses single quotes and may break lines between attributes.
bool definesNumber(const std::string &xml, const std::string &property)
{
    std::regex definition("<defNumberVector[^>]*name=['\"]" + property + "['\"]");
    return std::regex_search(xml, definition);
}

std::string describe(const std::vector<Packet> &packets)
{
    std::string text;
    char hex[8];
    for (const auto &packet : packets)
    {
        snprintf(hex, sizeof(hex), "%02X", packet.command);
        text += std::string(" <") + hex + ">";
        for (uint8_t byte : packet.data)
        {
            snprintf(hex, sizeof(hex), " %02X", byte);
            text += hex;
        }
    }
    return text;
}

}

// Connecting and loading the configuration only reads and configures; the motor must stay where it is.
TEST(PlaneWaveEFATest, connectAndConfigLoadDoNotMoveFocuser)
{
    writeConfiguration();
    FakeEFA efa;
    ASSERT_FALSE(efa.port().empty());

    PublishedXML published;
    EFAUnderTest driver;
    bool connected = driver.connectTo(efa.port());
    driver.loadConfiguration();
    published.finish();

    ASSERT_TRUE(connected);
    // The fake really served the connection, the startup reads and the configuration
    EXPECT_TRUE(efa.received(EFA::GET_VERSION));
    EXPECT_TRUE(efa.received(EFA::MTR_GET_POS));
    EXPECT_TRUE(efa.received(EFA::MTR_SLEWLIMITGETMAX));
    EXPECT_TRUE(efa.received(EFA::MTR_SLEWLIMITMAX)) << "The saved FOCUS_MAX was not loaded";

    std::vector<Packet> motion = efa.motionCommands();
    EXPECT_TRUE(motion.empty()) << "Motion command(s) sent during connect/config load:" << describe(motion);
    EXPECT_FALSE(efa.moving());
    EXPECT_EQ(efa.position(), START_POSITION);
}

// The EFA has no speed setting, so the driver must not offer one, and a speed request must not move the motor.
TEST(PlaneWaveEFATest, speedRequestDoesNotMoveFocuser)
{
    writeConfiguration();
    FakeEFA efa;
    ASSERT_FALSE(efa.port().empty());

    PublishedXML published;
    EFAUnderTest driver;
    bool connected = driver.connectTo(efa.port());
    efa.clearPackets();
    driver.requestNumber("FOCUS_SPEED", "FOCUS_SPEED_VALUE", 5);
    std::string xml = published.finish();

    ASSERT_TRUE(connected);
    ASSERT_TRUE(definesNumber(xml, "ABS_FOCUS_POSITION")) << "No property definitions captured";
    EXPECT_FALSE(definesNumber(xml, "FOCUS_SPEED")) << "The driver offers a focuser speed the EFA doesn't have";

    std::vector<Packet> motion = efa.motionCommands();
    EXPECT_TRUE(motion.empty()) << "Motion command(s) sent for a speed request:" << describe(motion);
    EXPECT_FALSE(efa.moving());
}

// Control: an absolute move still sends a GOTO to the target, and abort still sends a GOTO to the current position.
TEST(PlaneWaveEFATest, absoluteMoveAndAbortStillWork)
{
    writeConfiguration();
    FakeEFA efa;
    ASSERT_FALSE(efa.port().empty());

    PublishedXML published;
    EFAUnderTest driver;
    bool connected = driver.connectTo(efa.port());
    efa.clearPackets();
    driver.requestNumber("ABS_FOCUS_POSITION", "FOCUS_ABSOLUTE_POSITION", 1250000);
    std::vector<Packet> moves = efa.motionCommands();
    uint32_t positionAfterMove = efa.position();
    efa.clearPackets();
    driver.requestAbort();
    std::vector<Packet> aborts = efa.motionCommands();
    published.finish();

    ASSERT_TRUE(connected);
    ASSERT_EQ(moves.size(), 1u) << describe(moves);
    EXPECT_EQ(moves[0].command, EFA::MTR_GOTO_POS2);
    EXPECT_EQ(moves[0].data, (std::vector<uint8_t> {0x13, 0x12, 0xD0}));
    EXPECT_EQ(positionAfterMove, 1250000u);

    ASSERT_EQ(aborts.size(), 1u) << describe(aborts);
    EXPECT_EQ(aborts[0].command, EFA::MTR_GOTO_POS2);
    EXPECT_EQ(aborts[0].data, (std::vector<uint8_t> {0x13, 0x12, 0xD0}));
}

int main(int argc, char **argv)
{
    // Never read or write the user's real ~/.indi driver configuration.
    setenv("INDICONFIG", configFile().c_str(), 1);
    INDI::Logger::getInstance().configure("", INDI::Logger::file_off,
                                          INDI::Logger::DBG_ERROR, INDI::Logger::DBG_ERROR);
    ::testing::InitGoogleTest(&argc, argv);
    int rc = RUN_ALL_TESTS();
    std::remove(configFile().c_str());
    std::remove((configFile() + ".default").c_str());
    return rc;
}
