/*
    Tests of the coordinates the LX200 drivers send to the mount: signs of values between -1 and 0 degrees,
    which getSexComponents() stores as 0 degrees, and the rounding of the seconds.

    The drivers talk to a fake mount on a pseudo terminal that records every command and accepts it ("1").
    The expected commands follow the formats given in the comments of lx200driver.cpp, taken from the Meade
    Telescope Serial Command Protocol Revision 2010.10 and the 10Micron Mount Command Protocol 2.14.11.

    SPDX-License-Identifier: LGPL-2.1-or-later
*/

#include <gtest/gtest.h>

#include <cstdio>
#include <cstdlib>
#include <functional>
#include <string>
#include <unistd.h>

#include "fake_serial_device.h"
#include "indilogger.h"
#include "drivers/telescope/lx200driver.h"

extern int eq_format;
extern int geo_format;

namespace
{

std::string accept(const std::string &)
{
    return "1";
}

// Calls of the lx200driver.cpp functions, with the formats restored after each test.
class LX200DriverCommands : public ::testing::Test
{
    protected:
        void SetUp() override
        {
            m_EqFormat  = eq_format;
            m_GeoFormat = geo_format;
        }

        void TearDown() override
        {
            eq_format  = m_EqFormat;
            geo_format = m_GeoFormat;
        }

        // Runs one command function and returns what reached the mount.
        std::string sent(const std::function<int(int)> &command)
        {
            m_Device.clear();
            EXPECT_EQ(command(m_Device.driverFD()), 0);
            auto commands = m_Device.commands();
            EXPECT_EQ(commands.size(), 1u);
            return commands.empty() ? std::string() : commands.back();
        }

        std::string latitude(int format, double value, bool addSpace = false)
        {
            geo_format = format;
            return sent([&](int fd)
            {
                return setSiteLatitude(fd, value, addSpace);
            });
        }

        std::string altitude(double value)
        {
            return sent([&](int fd)
            {
                return setObjAlt(fd, value);
            });
        }

        FakeSerialDevice m_Device {accept};

    private:
        int m_EqFormat {0};
        int m_GeoFormat {0};
};

TEST_F(LX200DriverCommands, SiteLatitudeKeepsTheSignBetweenMinusOneAndZero)
{
    // A site 0.5 degrees south of the equator, and Quito at 0 deg 10' 48" south
    EXPECT_EQ(latitude(LX200_GEO_SHORT_FORMAT, -0.5), ":St-00*30#");
    EXPECT_EQ(latitude(LX200_GEO_SHORT_FORMAT, -0.5, true), ":St -00*30#");
    EXPECT_EQ(latitude(LX200_GEO_LONG_FORMAT, -0.18), ":St-00*10:48#");
    EXPECT_EQ(latitude(LX200_GEO_LONG_FORMAT, -0.18, true), ":St -00*10:48#");
    EXPECT_EQ(latitude(LX200_GEO_LONGER_FORMAT, -0.18), ":St-00*10:48.0#");
    EXPECT_EQ(latitude(LX200_GEO_LONGER_FORMAT, -0.18, true), ":St -00*10:48.0#");
}

TEST_F(LX200DriverCommands, SiteLatitudeIsUnchangedOutsideMinusOneToZero)
{
    EXPECT_EQ(latitude(LX200_GEO_SHORT_FORMAT, 45.5), ":St+45*30#");
    EXPECT_EQ(latitude(LX200_GEO_SHORT_FORMAT, -10.5), ":St-10*30#");
    EXPECT_EQ(latitude(LX200_GEO_SHORT_FORMAT, 0.5), ":St+00*30#");
    EXPECT_EQ(latitude(LX200_GEO_SHORT_FORMAT, 0.0), ":St+00*00#");
    EXPECT_EQ(latitude(LX200_GEO_SHORT_FORMAT, -5.25), ":St-05*15#");
    EXPECT_EQ(latitude(LX200_GEO_LONG_FORMAT, 51.4775), ":St+51*28:39#");
    EXPECT_EQ(latitude(LX200_GEO_LONG_FORMAT, -33.865), ":St-33*51:54#");
    EXPECT_EQ(latitude(LX200_GEO_LONGER_FORMAT, -33.865), ":St-33*51:54.0#");
    EXPECT_EQ(latitude(LX200_GEO_LONGER_FORMAT, 89.5, true), ":St +89*30:00.0#");
}

TEST_F(LX200DriverCommands, TargetAltitudeKeepsTheSignBetweenMinusOneAndZero)
{
    EXPECT_EQ(altitude(-0.5), ":Sa-0*30#");
    EXPECT_EQ(altitude(-0.99), ":Sa-0*59#");
}

TEST_F(LX200DriverCommands, TargetAltitudeIsUnchangedOutsideMinusOneToZero)
{
    EXPECT_EQ(altitude(45.5), ":Sa+45*30#");
    EXPECT_EQ(altitude(5.5), ":Sa+5*30#");
    EXPECT_EQ(altitude(0.5), ":Sa+0*30#");
    EXPECT_EQ(altitude(-10.25), ":Sa-10*15#");
    EXPECT_EQ(altitude(-5.5), ":Sa-5*30#");
}

}

int main(int argc, char **argv)
{
    // Never touch the user's real ~/.indi (park data is written to $HOME/.indi).
    char homeTemplate[] = "/tmp/test_lx200_coordinates_home_XXXXXX";
    const char *home = mkdtemp(homeTemplate);
    if (home)
        setenv("HOME", home, 1);
    // Never read or write the user's real ~/.indi driver configuration.
    const std::string configFile = "/tmp/test_lx200_coordinates_" + std::to_string(getpid()) + "_config.xml";
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
