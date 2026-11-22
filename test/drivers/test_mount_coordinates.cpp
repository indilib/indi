/*
    Tests of the coordinates mount drivers outside the LX200 family send to the mount: signs of values between
    -1 and 0 degrees, which getSexComponents() stores as 0 degrees, and the rounding of the seconds.

    Each driver talks to a fake mount on a pseudo terminal that records every command and accepts it. The
    expected commands follow the formats each driver already sends for other values.

    SPDX-License-Identifier: LGPL-2.1-or-later
*/

#include <gtest/gtest.h>

#include <cstdio>
#include <cstdlib>
#include <functional>
#include <string>
#include <unistd.h>
#include <vector>

#include "fake_serial_device.h"
#include "indilogger.h"
#include "drivers/telescope/rainbow.h"
#include "drivers/telescope/crux_mount.h"

namespace
{

std::string accept(const std::string &)
{
    return "1";
}

// Rainbow RST with the functions under test made reachable.
class TestRainbow : public Rainbow
{
    public:
        void setPort(int fd)
        {
            PortFD = fd;
        }

        using Rainbow::setRA;
        using Rainbow::setDE;
        using Rainbow::setAZ;
        using Rainbow::setAL;
};

class RainbowCommands : public ::testing::Test
{
    protected:
        void SetUp() override
        {
            m_Rainbow.setPort(m_Device.driverFD());
        }

        // Rainbow::sendCommand() flushes the port before each command, so wait until the fake mount has read
        // one command before sending the next.
        std::string sent(const std::function<bool()> &command)
        {
            m_Device.clear();
            EXPECT_TRUE(command());
            auto commands = m_Device.waitForCommands(1);
            EXPECT_EQ(commands.size(), 1u);
            return commands.empty() ? std::string() : commands.back();
        }

        std::string ra(double value)
        {
            return sent([&]()
            {
                return m_Rainbow.setRA(value);
            });
        }

        std::string dec(double value)
        {
            return sent([&]()
            {
                return m_Rainbow.setDE(value);
            });
        }

        std::string azimuth(double value)
        {
            return sent([&]()
            {
                return m_Rainbow.setAZ(value);
            });
        }

        std::string altitude(double value)
        {
            return sent([&]()
            {
                return m_Rainbow.setAL(value);
            });
        }

        FakeSerialDevice m_Device {accept};
        TestRainbow m_Rainbow;
};

TEST_F(RainbowCommands, NeverSendsSixtySeconds)
{
    EXPECT_EQ(ra(5.99999), ":Sr06:00:00.0#");
    EXPECT_EQ(ra(23.99999), ":Sr00:00:00.0#");
    EXPECT_EQ(dec(-0.99999), ":Sd-01*00:00.0#");
    EXPECT_EQ(dec(45.99999), ":Sd+46*00:00.0#");
    EXPECT_EQ(azimuth(359.99999), ":Sz000*00:00.0#");
    EXPECT_EQ(azimuth(99.99999), ":Sz100*00:00.0#");
    EXPECT_EQ(altitude(45.99999), ":Sa+46*00:00.0#");
}

TEST_F(RainbowCommands, IsUnchangedAwayFromSixtySeconds)
{
    EXPECT_EQ(ra(1.2345678), ":Sr01:14:04.4#");
    EXPECT_EQ(dec(-12.3456789), ":Sd-12*20:44.4#");
    EXPECT_EQ(dec(-0.5), ":Sd-00*30:00.0#");
    EXPECT_EQ(azimuth(180.5), ":Sz180*30:00.0#");
    EXPECT_EQ(azimuth(12.3456789), ":Sz012*20:44.4#");
    EXPECT_EQ(altitude(12.3456789), ":Sa+12*20:44.4#");
    EXPECT_EQ(altitude(-12.3456789), ":Sa-12*20:44.4#");
}

// CRUX Titan TCS with the site update reachable; it sends the latitude, then the longitude.
class TestTitanTCS : public TitanTCS
{
    public:
        void setPort(int fd)
        {
            PortFD = fd;
        }

        using TitanTCS::updateLocation;
};

class TitanTCSCommands : public ::testing::Test
{
    protected:
        void SetUp() override
        {
            m_Titan.setPort(m_Device.driverFD());
        }

        // longitude is an INDI longitude, 0 to 360 degrees East; the mount gets it as negative.
        std::vector<std::string> location(double latitude, double longitude)
        {
            m_Device.clear();
            EXPECT_TRUE(m_Titan.updateLocation(latitude, longitude, 0));
            return m_Device.commands();
        }

        FakeSerialDevice m_Device {accept};
        TestTitanTCS m_Titan;
};

TEST_F(TitanTCSCommands, SiteKeepsTheSignBetweenMinusOneAndZero)
{
    EXPECT_EQ(location(-0.5, 10), (std::vector<std::string> {":St -00:30:00#", ":Sg -10:00:00#"}));
    EXPECT_EQ(location(45.5, 0.5), (std::vector<std::string> {":St 045:30:00#", ":Sg -00:30:00#"}));
}

TEST_F(TitanTCSCommands, OtherSitesAreUnchanged)
{
    EXPECT_EQ(location(-30.5, 350), (std::vector<std::string> {":St -30:30:00#", ":Sg -350:00:00#"}));
    EXPECT_EQ(location(0.5, 10), (std::vector<std::string> {":St 000:30:00#", ":Sg -10:00:00#"}));
    EXPECT_EQ(location(51.4775, 0), (std::vector<std::string> {":St 051:28:39#", ":Sg 000:00:00#"}));
}

}

int main(int argc, char **argv)
{
    // Never touch the user's real ~/.indi (park data is written to $HOME/.indi).
    char homeTemplate[] = "/tmp/test_mount_coordinates_home_XXXXXX";
    const char *home = mkdtemp(homeTemplate);
    if (home)
        setenv("HOME", home, 1);
    // Never read or write the user's real ~/.indi driver configuration.
    const std::string configFile = "/tmp/test_mount_coordinates_" + std::to_string(getpid()) + "_config.xml";
    setenv("INDICONFIG", configFile.c_str(), 1);
    INDI::Logger::getInstance().configure("", INDI::Logger::file_off,
                                          INDI::Logger::DBG_ERROR, INDI::Logger::DBG_ERROR);
    ::testing::InitGoogleTest(&argc, argv);
    int rc = RUN_ALL_TESTS();
    std::remove(configFile.c_str());
    std::remove((configFile + ".default").c_str());
    if (home)
    {
        std::remove((std::string(home) + "/.indi/ParkData.xml").c_str());
        std::remove((std::string(home) + "/.indi").c_str());
        std::remove(home);
    }
    return rc;
}
