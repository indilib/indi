/*
    Tests for stable serial port names and the auto search fallback of the serial connection plugin (Linux).

    /dev/ttyUSB<n> numbers change between boots, so a driver that saved /dev/ttyUSB2 can open another device the
    next night. The plugin now lists and saves USB serial ports under names that stay the same: the by-id link for
    an adapter with its own serial number, the by-path link (USB socket) for identical adapters without one (they
    share a single by-id link). Auto search stays enabled after it found a device, so a moved device is found again.

    The naming tests use a fake /dev + /sys tree that mirrors a StellarMate with a CP2102N (mount), an FTDI
    (Pegasus PPBA) and two CH340 adapters without serial numbers (RBF Excalibur, Wanderer ETA). The auto search
    test uses fake devices on pseudo terminals.

    SPDX-License-Identifier: LGPL-2.1-or-later
*/

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <pty.h>
#include <string>
#include <sys/select.h>
#include <sys/stat.h>
#include <termios.h>
#include <thread>
#include <unistd.h>
#include <vector>

#include "defaultdevice.h"
#include "indicom.h"
#include "indilogger.h"
#include "connectionplugins/connectionserial.h"
#include "connectionplugins/serialportnames.h"

namespace
{

namespace fs = std::filesystem;
namespace SerialPorts = Connection::SerialPorts;

//////////////////////////////////////////////////////////////////////////////////////////////////
// Fake /dev and /sys tree
//////////////////////////////////////////////////////////////////////////////////////////////////
class FakeSystem
{
    public:
        FakeSystem()
        {
            char pattern[] = "/tmp/test_serial_port_names_XXXXXX";
            const char *dir = mkdtemp(pattern);
            m_Root = dir ? dir : "";
            fs::create_directories(m_Root + "/dev/serial/by-id");
            fs::create_directories(m_Root + "/dev/serial/by-path");
            fs::create_directories(m_Root + "/sys/class/tty");
        }

        ~FakeSystem()
        {
            std::error_code ignored;
            fs::remove_all(m_Root, ignored);
        }

        const std::string &root() const
        {
            return m_Root;
        }

        std::string dev(const std::string &name) const
        {
            return m_Root + "/dev/" + name;
        }

        // A USB serial adapter at USB socket (e.g. "3-3"), as udev and the kernel present it.
        // serial empty = the adapter reports no serial number. byIdName empty = no by-id link points to it.
        void addAdapter(const std::string &tty, const std::string &socket, const std::string &vid, const std::string &pid,
                        const std::string &serial, const std::string &product, const std::string &byIdName)
        {
            const std::string usb = m_Root + "/sys/devices/pci0000:00/0000:00:14.0/usb3/" + socket;
            const std::string interface = usb + "/" + socket + ":1.0";
            fs::create_directories(interface + "/" + tty);
            writeFile(usb + "/idVendor", vid);
            writeFile(usb + "/idProduct", pid);
            writeFile(usb + "/product", product);
            if (!serial.empty())
                writeFile(usb + "/serial", serial);
            writeFile(interface + "/bInterfaceNumber", "00");

            fs::create_directories(m_Root + "/sys/class/tty/" + tty);
            link(interface + "/" + tty, m_Root + "/sys/class/tty/" + tty + "/device");

            writeFile(dev(tty), "");
            if (!byIdName.empty())
                link("../../" + tty, byId(byIdName));
            link("../../" + tty, byPath(socket));
            link("../../" + tty, m_Root + "/dev/serial/by-path/pci-0000:00:14.0-usbv2-0:" + usbPortPath(socket) +
                 ":1.0-port0");
        }

        std::string byPath(const std::string &socket) const
        {
            return m_Root + "/dev/serial/by-path/pci-0000:00:14.0-usb-0:" + usbPortPath(socket) + ":1.0-port0";
        }

        std::string byId(const std::string &name) const
        {
            return m_Root + "/dev/serial/by-id/" + name;
        }

        void link(const std::string &target, const std::string &path) const
        {
            fs::create_symlink(target, path);
        }

    private:
        // "3-1.2.1" -> "1.2.1"
        static std::string usbPortPath(const std::string &socket)
        {
            return socket.substr(socket.find('-') + 1);
        }

        static void writeFile(const std::string &path, const std::string &content)
        {
            std::ofstream out(path);
            out << content << (content.empty() ? "" : "\n");
        }

        std::string m_Root;
};

const char *MOUNT_BY_ID =
    "usb-Silicon_Labs_CP2102N_USB_to_UART_Bridge_Controller_0059b092d833f1118be3945f30d20014-if00-port0";
const char *PPBA_BY_ID = "usb-Pegasus_Astro_PPBADV_Gen2C_PPBA8VSOTK-if00-port0";
const char *CH340_BY_ID = "usb-1a86_USB_Serial-if00-port0";

// The StellarMate of the bug report: two CH340 without serial numbers share one by-id link (pointing to the one
// enumerated last, ttyUSB3).
void addStellarMate(FakeSystem &fake)
{
    fake.addAdapter("ttyUSB0", "3-1.4", "10c4", "ea60", "0059b092d833f1118be3945f30d20014",
                    "CP2102N USB to UART Bridge Controller", MOUNT_BY_ID);
    fake.addAdapter("ttyUSB1", "3-1.1", "0403", "6015", "PPBA8VSOTK", "PPBADV Gen2C", PPBA_BY_ID);
    fake.addAdapter("ttyUSB2", "3-1.2.1", "1a86", "7523", "", "USB Serial", "");
    fake.addAdapter("ttyUSB3", "3-3", "1a86", "7523", "", "USB Serial", CH340_BY_ID);
}

}

TEST(SerialPortNamesTest, uniqueSerialNumberUsesById)
{
    FakeSystem fake;
    addStellarMate(fake);

    EXPECT_EQ(SerialPorts::stableName(fake.dev("ttyUSB0"), fake.root()), fake.byId(MOUNT_BY_ID));
    EXPECT_EQ(SerialPorts::stableName(fake.dev("ttyUSB1"), fake.root()), fake.byId(PPBA_BY_ID));
}

TEST(SerialPortNamesTest, identicalAdaptersWithoutSerialUseUsbSocket)
{
    FakeSystem fake;
    addStellarMate(fake);

    EXPECT_EQ(SerialPorts::stableName(fake.dev("ttyUSB2"), fake.root()), fake.byPath("3-1.2.1"));
    EXPECT_EQ(SerialPorts::stableName(fake.dev("ttyUSB3"), fake.root()), fake.byPath("3-3"));
}

TEST(SerialPortNamesTest, sharedSerialNumberUsesUsbSocket)
{
    // Some cheap adapters all report the same serial number.
    FakeSystem fake;
    fake.addAdapter("ttyUSB0", "3-1", "0403", "6001", "A50285BI", "FT232R USB UART", "");
    fake.addAdapter("ttyUSB1", "3-2", "0403", "6001", "A50285BI", "FT232R USB UART",
                    "usb-FTDI_FT232R_USB_UART_A50285BI-if00-port0");

    EXPECT_EQ(SerialPorts::stableName(fake.dev("ttyUSB0"), fake.root()), fake.byPath("3-1"));
    EXPECT_EQ(SerialPorts::stableName(fake.dev("ttyUSB1"), fake.root()), fake.byPath("3-2"));
}

TEST(SerialPortNamesTest, singleAdapterWithoutSerialUsesUsbSocket)
{
    FakeSystem fake;
    fake.addAdapter("ttyUSB0", "3-3", "1a86", "7523", "", "USB Serial", CH340_BY_ID);

    EXPECT_EQ(SerialPorts::stableName(fake.dev("ttyUSB0"), fake.root()), fake.byPath("3-3"));
}

TEST(SerialPortNamesTest, nameToSaveReplacesRenumberingNames)
{
    FakeSystem fake;
    addStellarMate(fake);

    // Kernel device nodes
    EXPECT_EQ(SerialPorts::nameToSave(fake.dev("ttyUSB0"), fake.root()), fake.byId(MOUNT_BY_ID));
    EXPECT_EQ(SerialPorts::nameToSave(fake.dev("ttyUSB2"), fake.root()), fake.byPath("3-1.2.1"));
    // The by-id link shared by the two CH340 points to whichever was enumerated last.
    EXPECT_EQ(SerialPorts::nameToSave(fake.byId(CH340_BY_ID), fake.root()), fake.byPath("3-3"));
}

TEST(SerialPortNamesTest, nameToSaveKeepsStableAndCustomNames)
{
    FakeSystem fake;
    addStellarMate(fake);
    fake.link("ttyUSB3", fake.dev("ttyUSB_excalibur"));
    fake.link("ttyUSB0", fake.dev("mount"));

    for (const std::string &port :
            {
                fake.byId(MOUNT_BY_ID), fake.byPath("3-3"), fake.dev("ttyUSB_excalibur"), fake.dev("mount"),
                fake.dev("rfcomm0"), fake.dev("ttyUSB9")
            })
        EXPECT_EQ(SerialPorts::nameToSave(port, fake.root()), port);
}

TEST(SerialPortNamesTest, listShowsStableNames)
{
    FakeSystem fake;
    addStellarMate(fake);
    // Custom udev names are not listed twice.
    fake.link("ttyUSB3", fake.dev("ttyUSB_excalibur"));

    const std::vector<std::string> expected =
    {
        fake.byId(MOUNT_BY_ID), fake.byId(PPBA_BY_ID), fake.byPath("3-1.2.1"), fake.byPath("3-3")
    };
    EXPECT_EQ(SerialPorts::list(fake.root()), expected);
}

TEST(SerialPortNamesTest, adapterTypeReadsUsbIds)
{
    FakeSystem fake;
    addStellarMate(fake);
    fake.link("ttyUSB3", fake.dev("ttyUSB_excalibur"));

    EXPECT_EQ(SerialPorts::adapterType(fake.byId(MOUNT_BY_ID), fake.root()), "10c4:ea60");
    EXPECT_EQ(SerialPorts::adapterType(fake.byPath("3-1.2.1"), fake.root()), "1a86:7523");
    EXPECT_EQ(SerialPorts::adapterType(fake.dev("ttyUSB_excalibur"), fake.root()), "1a86:7523");
    EXPECT_EQ(SerialPorts::adapterType(fake.dev("ttyUSB9"), fake.root()), "");
}

TEST(SerialPortNamesTest, labelShowsProductAndSocket)
{
    FakeSystem fake;
    addStellarMate(fake);

    EXPECT_EQ(SerialPorts::label(fake.byPath("3-1.2.1"), fake.root()), "USB Serial @ USB 3-1.2.1");
    EXPECT_EQ(SerialPorts::label(fake.byId(MOUNT_BY_ID), fake.root()), "");
}

TEST(SerialPortNamesTest, searchOrderPrefersLikelyPorts)
{
    FakeSystem fake;
    addStellarMate(fake);
    const std::vector<std::string> ports =
    {
        fake.byId(MOUNT_BY_ID), fake.byPath("3-3"), fake.byId(PPBA_BY_ID), fake.byPath("3-1.2.1")
    };
    const std::vector<std::string> byPathFirst =
    {
        fake.byPath("3-3"), fake.byPath("3-1.2.1"), fake.byId(MOUNT_BY_ID), fake.byId(PPBA_BY_ID)
    };
    // Gone by-path port (the device was moved to another USB socket), type unknown: the other by-path ports first.
    const std::string emptySocket = fake.byPath("3-4");
    EXPECT_EQ(SerialPorts::searchOrder(ports, emptySocket, "", fake.root()), byPathFirst);
    // Type known from this session: same type first, even if the saved port is a by-id link.
    EXPECT_EQ(SerialPorts::searchOrder(ports, fake.byId(MOUNT_BY_ID), "1a86:7523", fake.root()), byPathFirst);
    // Type of the adapter now at the configured port.
    const std::vector<std::string> cp2102First =
    {
        fake.byId(MOUNT_BY_ID), fake.byPath("3-3"), fake.byId(PPBA_BY_ID), fake.byPath("3-1.2.1")
    };
    EXPECT_EQ(SerialPorts::searchOrder(ports, fake.dev("ttyUSB0"), "", fake.root()), cp2102First);
    // Nothing known (e.g. a custom name that is gone): unchanged.
    EXPECT_EQ(SerialPorts::searchOrder(ports, fake.dev("ttyUSB_gone"), "", fake.root()), ports);
}

//////////////////////////////////////////////////////////////////////////////////////////////////
// Auto search with fake devices on pseudo terminals
//////////////////////////////////////////////////////////////////////////////////////////////////
namespace
{

const char *DEVICE_NAME = "Mock Serial Search";

// Answers "?\n" with "OK\n" when talkative, else never answers.
class FakeSerialDevice
{
    public:
        explicit FakeSerialDevice(bool talkative) : m_Talkative(talkative)
        {
            int slave = -1;
            if (openpty(&m_Master, &slave, nullptr, nullptr, nullptr) != 0)
                return;
            struct termios tio;
            const int fds[] = {m_Master, slave};
            for (int fd : fds)
            {
                tcgetattr(fd, &tio);
                cfmakeraw(&tio);
                tcsetattr(fd, TCSANOW, &tio);
            }
            const char *name = ttyname(slave);
            m_Port = name ? name : "";
            close(slave);
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

        const std::string &port() const
        {
            return m_Port;
        }

    private:
        void serve()
        {
            while (!m_Stop)
            {
                fd_set readSet;
                FD_ZERO(&readSet);
                FD_SET(m_Master, &readSet);
                struct timeval timeout = {0, 20000};
                if (select(m_Master + 1, &readSet, nullptr, nullptr, &timeout) <= 0)
                    continue;
                char chunk[64];
                ssize_t n = read(m_Master, chunk, sizeof(chunk));
                if (n <= 0)
                {
                    std::this_thread::sleep_for(std::chrono::milliseconds(20));
                    continue;
                }
                if (m_Talkative && memchr(chunk, '?', n) != nullptr && write(m_Master, "OK\n", 3) < 0)
                    break;
            }
        }

        bool m_Talkative;
        int m_Master {-1};
        std::string m_Port;
        std::thread m_Thread;
        std::atomic<bool> m_Stop {false};
};

// Serial plugin whose system ports are given by the test instead of the real /dev.
class TestSerial : public Connection::Serial
{
    public:
        explicit TestSerial(INDI::DefaultDevice *device) : Connection::Serial(device) {}

        void useSystemPorts(const std::vector<std::string> &ports)
        {
            m_SystemPorts = ports;
            SystemPortSP.resize(0);
            for (const auto &port : ports)
            {
                INDI::WidgetSwitch node;
                node.fill(port, port, ISS_OFF);
                SystemPortSP.push(std::move(node));
            }
        }

        bool autoSearchEnabled() const
        {
            return AutoSearchSP[INDI::DefaultDevice::INDI_ENABLED].getState() == ISS_ON;
        }
};

class SearchDevice : public INDI::DefaultDevice
{
    public:
        SearchDevice()
        {
            setDeviceName(DEVICE_NAME);
        }

        const char *getDefaultName() override
        {
            return DEVICE_NAME;
        }

        bool initProperties() override
        {
            INDI::DefaultDevice::initProperties();
            serial = new TestSerial(this);
            serial->registerHandshake([this]()
            {
                int fd = serial->getPortFD(), written = 0, read = 0;
                char reply[16] = {0};
                tcflush(fd, TCIOFLUSH);
                if (tty_write_string(fd, "?\n", &written) != TTY_OK)
                    return false;
                return tty_nread_section(fd, reply, sizeof(reply), '\n', 1, &read) == TTY_OK &&
                       strncmp(reply, "OK", 2) == 0;
            });
            registerConnection(serial);
            return true;
        }

        void enableAutoSearch()
        {
            ISState states[2] = {ISS_ON, ISS_OFF};
            char enabled[] = "INDI_ENABLED", disabled[] = "INDI_DISABLED";
            char *names[2] = {enabled, disabled};
            ISNewSwitch(getDeviceName(), "DEVICE_AUTO_SEARCH", states, names, 2);
        }

        bool connectNow()
        {
            ISState state[1] = {ISS_ON};
            char connect[] = "CONNECT";
            char *names[1] = {connect};
            ISNewSwitch(getDeviceName(), "CONNECTION", state, names, 1);
            return isConnected();
        }

        TestSerial *serial {nullptr};
};

std::string configFile()
{
    return "/tmp/test_serial_port_names_" + std::to_string(getpid()) + "_config.xml";
}

// Auto search saved as disabled, as older INDI versions did after their first successful search.
void writeConfiguration(const std::string &port)
{
    std::ofstream config(configFile());
    config << "<INDIDriver>\n"
           << "<newSwitchVector device='" << DEVICE_NAME << "' name='CONNECTION_MODE'>\n"
           << "  <oneSwitch name='CONNECTION_SERIAL'>On</oneSwitch>\n"
           << "</newSwitchVector>\n"
           << "<newTextVector device='" << DEVICE_NAME << "' name='DEVICE_PORT'>\n"
           << "  <oneText name='PORT'>" << port << "</oneText>\n"
           << "</newTextVector>\n"
           << "<newSwitchVector device='" << DEVICE_NAME << "' name='DEVICE_BAUD_RATE'>\n";
    // A complete serial configuration, so INDI saves single properties in place instead of the whole file.
    const char *rates[] = {"1200", "2400", "4800", "9600", "19200", "38400", "57600", "115200", "230400"};
    for (const char *rate : rates)
        config << "  <oneSwitch name='" << rate << "'>" << (strcmp(rate, "9600") ? "Off" : "On") << "</oneSwitch>\n";
    config << "</newSwitchVector>\n"
           << "<newSwitchVector device='" << DEVICE_NAME << "' name='DEVICE_AUTO_SEARCH'>\n"
           << "  <oneSwitch name='INDI_ENABLED'>Off</oneSwitch>\n"
           << "  <oneSwitch name='INDI_DISABLED'>On</oneSwitch>\n"
           << "</newSwitchVector>\n"
           << "</INDIDriver>\n";
}

std::string savedPort()
{
    char port[MAXINDINAME] = {0};
    return IUGetConfigText(DEVICE_NAME, "DEVICE_PORT", "PORT", port, MAXINDINAME) == 0 ? port : "";
}

int savedAutoSearchIndex()
{
    int index = -1;
    return IUGetConfigOnSwitchIndex(DEVICE_NAME, "DEVICE_AUTO_SEARCH", &index) == 0 ? index : -1;
}

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

}

TEST(SerialAutoSearchTest, savedPortGoneFindsDeviceAndKeepsAutoSearch)
{
    // The saved port no longer exists (renumbered or renamed) and the user switched auto search on (not saved).
    // Auto search must find the device and stay enabled, also in the saved configuration, so the next renumbering
    // is handled the same way after a restart.
    FakeSerialDevice other(false), device(true);
    ASSERT_FALSE(other.port().empty());
    ASSERT_FALSE(device.port().empty());
    writeConfiguration("/dev/ttyUSB_gone_" + std::to_string(getpid()));

    bool connected, autoSearch;
    std::string port;
    {
        QuietStdout quiet;
        SearchDevice driver;
        driver.ISGetProperties(nullptr);
        driver.serial->useSystemPorts({other.port(), device.port()});
        driver.enableAutoSearch();
        connected = driver.connectNow();
        port = driver.serial->port();
        autoSearch = driver.serial->autoSearchEnabled();
    }

    ASSERT_TRUE(connected);
    EXPECT_EQ(port, device.port());
    EXPECT_EQ(savedPort(), device.port());
    EXPECT_TRUE(autoSearch) << "Auto search was switched off after it found the device";
    EXPECT_EQ(savedAutoSearchIndex(), 0) << "Auto search is not saved as enabled";
}

// Driver messages (INDI XML on stdout) of everything run inside, as "[LEVEL] text" lines.
class CapturedMessages
{
    public:
        CapturedMessages()
        {
            fflush(stdout);
            m_File = tmpfile();
            m_Saved = dup(STDOUT_FILENO);
            if (m_File)
                dup2(fileno(m_File), STDOUT_FILENO);
        }
        ~CapturedMessages()
        {
            restore();
            if (m_File)
                fclose(m_File);
        }
        std::vector<std::string> finish()
        {
            restore();
            std::string xml;
            if (m_File)
            {
                rewind(m_File);
                char chunk[4096];
                size_t n;
                while ((n = fread(chunk, 1, sizeof(chunk), m_File)) > 0)
                    xml.append(chunk, n);
            }
            std::vector<std::string> messages;
            const std::string key = "message='";
            for (size_t pos = xml.find(key); pos != std::string::npos; pos = xml.find(key, pos))
            {
                pos += key.size();
                const size_t end = xml.find('\'', pos);
                messages.push_back(xml.substr(pos, end - pos));
            }
            return messages;
        }
    private:
        void restore()
        {
            if (m_Saved < 0)
                return;
            fflush(stdout);
            dup2(m_Saved, STDOUT_FILENO);
            close(m_Saved);
            m_Saved = -1;
        }
        FILE *m_File {nullptr};
        int m_Saved {-1};
};

int countContaining(const std::vector<std::string> &messages, const std::string &text)
{
    int count = 0;
    for (const auto &message : messages)
        if (message.find(text) != std::string::npos)
            count++;
    return count;
}

TEST(SerialAutoSearchTest, failedSearchLogsOneSummaryOnly)
{
    // The device is absent: one port answers nothing, one is in use by another driver, the saved port is gone.
    // The user gets one warning that sums it up, not a warning or error per port and pass.
    FakeSerialDevice silent(false), taken(false);
    ASSERT_FALSE(silent.port().empty());
    ASSERT_FALSE(taken.port().empty());
    int takenFd = -1;
    ASSERT_EQ(tty_connect(taken.port().c_str(), 9600, 8, 0, 1, &takenFd), TTY_OK);
    writeConfiguration("/dev/ttyUSB_gone_" + std::to_string(getpid()));

    bool connected;
    std::vector<std::string> messages;
    {
        CapturedMessages captured;
        SearchDevice driver;
        driver.ISGetProperties(nullptr);
        driver.serial->useSystemPorts({silent.port(), taken.port()});
        driver.enableAutoSearch();
        connected = driver.connectNow();
        messages = captured.finish();
    }
    tty_disconnect(takenFd);

    std::string all;
    for (const auto &message : messages)
        all += "\n  " + message;

    EXPECT_FALSE(connected);
    EXPECT_EQ(countContaining(messages, "Starting Auto Search"), 1) << all;
    EXPECT_EQ(countContaining(messages, "was not found on 3 port(s)"), 1) << all;
    EXPECT_EQ(countContaining(messages, "found a port in use by another driver"), 1) << all;
    EXPECT_EQ(countContaining(messages, "[INFO] Trying connecting"), 0) << all;
    EXPECT_EQ(countContaining(messages, "[WARNING] Port "), 0) << all;
    // Only the first attempt on the saved port reports its error.
    EXPECT_EQ(countContaining(messages, "[ERROR] Failed to connect"), 1) << all;
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
