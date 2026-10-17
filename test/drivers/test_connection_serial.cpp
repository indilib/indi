/*******************************************************************************
 Copyright(c) 2026 Jasem Mutlaq. All rights reserved.

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Library General Public
 License version 2 as published by the Free Software Foundation.

 This library is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 Library General Public License for more details.

 You should have received a copy of the GNU Library General Public License
 along with this library; see the file COPYING.LIB.  If not, write to
 the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 Boston, MA 02110-1301, USA.
*******************************************************************************/

// Regression tests for the serial connection plugin configuration handling:
// - Baud rates are loaded by name, so configurations saved before the slower baud rates were
//   added (different switch order) still select the right baud rate.
// - Saving a single property whose saved layout differs from the current one rewrites it with
//   the current layout instead of dropping the new switches.

#include <gtest/gtest.h>

#include <cstdio>
#include <fstream>
#include <memory>
#include <string>
#include <unistd.h>

#include "defaultdevice.h"
#include "indidriver.h"
#include "indilogger.h"
#include "connectionplugins/connectionserial.h"

namespace
{

const char *DEVICE_NAME = "MockSerialDevice";

class SerialDevice : public INDI::DefaultDevice
{
    public:
        explicit SerialDevice(Connection::Serial::BaudRate defaultBaudRate) : m_DefaultBaudRate(defaultBaudRate)
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
            serialConnection = new Connection::Serial(this);
            serialConnection->setDefaultBaudRate(m_DefaultBaudRate);
            registerConnection(serialConnection);
            return true;
        }

        using INDI::DefaultDevice::saveConfig;

        Connection::Serial *serialConnection {nullptr};

    private:
        Connection::Serial::BaudRate m_DefaultBaudRate;
};

class SerialConfigTest : public ::testing::Test
{
    protected:
        void SetUp() override
        {
            // Use a configuration file private to this process, never the user's ~/.indi
            m_ConfigFile = "/tmp/test_connection_serial_" + std::to_string(getpid()) + "_config.xml";
            setenv("INDICONFIG", m_ConfigFile.c_str(), 1);
            removeConfig();
        }

        void TearDown() override
        {
            removeConfig();
        }

        void removeConfig()
        {
            std::remove(m_ConfigFile.c_str());
            // Saving the whole configuration also writes a default configuration copy
            std::remove((m_ConfigFile + ".default").c_str());
        }

        // Configuration as saved before 1200, 2400 and 4800 baud were added.
        void writeOldLayoutConfig(const std::string &onBaudRate)
        {
            std::ofstream out(m_ConfigFile);
            out << "<INDIDriver>\n";
            out << "<newSwitchVector device='" << DEVICE_NAME << "' name='CONNECTION_MODE'>\n";
            out << "  <oneSwitch name='CONNECTION_SERIAL'>On</oneSwitch>\n";
            out << "</newSwitchVector>\n";
            out << "<newTextVector device='" << DEVICE_NAME << "' name='DEVICE_PORT'>\n";
            out << "  <oneText name='PORT'>/dev/null</oneText>\n";
            out << "</newTextVector>\n";
            out << "<newSwitchVector device='" << DEVICE_NAME << "' name='DEVICE_BAUD_RATE'>\n";
            for (const char *rate : {"9600", "19200", "38400", "57600", "115200", "230400"})
                out << "  <oneSwitch name='" << rate << "'>" << (onBaudRate == rate ? "On" : "Off") << "</oneSwitch>\n";
            out << "</newSwitchVector>\n";
            out << "</INDIDriver>\n";
        }

        std::unique_ptr<SerialDevice> startDevice(Connection::Serial::BaudRate defaultBaudRate)
        {
            auto device = std::make_unique<SerialDevice>(defaultBaudRate);
            device->initProperties();
            device->ISGetProperties(nullptr);
            return device;
        }

        std::string savedBaudRate() const
        {
            char name[MAXINDINAME] = {0};
            if (IUGetConfigOnSwitchName(DEVICE_NAME, "DEVICE_BAUD_RATE", name, MAXINDINAME) != 0)
                return "";
            return name;
        }

        std::string m_ConfigFile;
};

TEST_F(SerialConfigTest, DefaultBaudRateWithoutConfig)
{
    auto device = startDevice(Connection::Serial::B_4800);
    EXPECT_EQ(device->serialConnection->baud(), 4800u);
}

TEST_F(SerialConfigTest, OldLayoutConfigLoadsBaudRateByName)
{
    // Index 4 in the old layout is 115200, but 19200 in the new one.
    writeOldLayoutConfig("115200");
    auto device = startDevice(Connection::Serial::B_9600);
    EXPECT_EQ(device->serialConnection->baud(), 115200u);
}

TEST_F(SerialConfigTest, OldLayoutConfigOverridesDriverDefault)
{
    writeOldLayoutConfig("57600");
    auto device = startDevice(Connection::Serial::B_115200);
    EXPECT_EQ(device->serialConnection->baud(), 57600u);
}

TEST_F(SerialConfigTest, SavingNewBaudRateOverOldLayoutConfig)
{
    writeOldLayoutConfig("57600");
    auto device = startDevice(Connection::Serial::B_9600);

    const char *names[] = {"4800"};
    ISState states[] = {ISS_ON};
    ASSERT_TRUE(device->ISNewSwitch(DEVICE_NAME, "DEVICE_BAUD_RATE", states, const_cast<char **>(names), 1));
    ASSERT_EQ(device->serialConnection->baud(), 4800u);

    // Single-property save, as done by the plugin after a successful handshake.
    ASSERT_TRUE(device->saveConfig(true, "DEVICE_BAUD_RATE"));
    EXPECT_EQ(savedBaudRate(), "4800");

    // Other saved properties must survive the rewrite.
    char port[MAXINDINAME] = {0};
    ASSERT_EQ(IUGetConfigText(DEVICE_NAME, "DEVICE_PORT", "PORT", port, MAXINDINAME), 0);
    EXPECT_STREQ(port, "/dev/null");

    // A restarted driver picks up the new baud rate.
    device.reset();
    auto restarted = startDevice(Connection::Serial::B_9600);
    EXPECT_EQ(restarted->serialConnection->baud(), 4800u);
}

TEST_F(SerialConfigTest, SavingOverCurrentLayoutConfig)
{
    auto device = startDevice(Connection::Serial::B_9600);
    ASSERT_TRUE(device->saveConfig(true));
    ASSERT_EQ(savedBaudRate(), "9600");

    const char *names[] = {"2400"};
    ISState states[] = {ISS_ON};
    ASSERT_TRUE(device->ISNewSwitch(DEVICE_NAME, "DEVICE_BAUD_RATE", states, const_cast<char **>(names), 1));
    ASSERT_TRUE(device->saveConfig(true, "DEVICE_BAUD_RATE"));
    EXPECT_EQ(savedBaudRate(), "2400");
}

}

int main(int argc, char **argv)
{
    INDI::Logger::getInstance().configure("", INDI::Logger::file_off,
                                          INDI::Logger::DBG_ERROR, INDI::Logger::DBG_ERROR);
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
