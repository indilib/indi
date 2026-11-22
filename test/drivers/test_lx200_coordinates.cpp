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
#include "drivers/telescope/lx200_OnStep.h"

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

        // value is a cartographic longitude (East positive); the mount gets East as negative.
        std::string longitude(int format, double value, bool addSpace = false)
        {
            geo_format = format;
            return sent([&](int fd)
            {
                return setSiteLongitude(fd, value, addSpace);
            });
        }

        std::string altitude(double value)
        {
            return sent([&](int fd)
            {
                return setObjAlt(fd, value);
            });
        }

        std::string ra(int format, double value)
        {
            eq_format = format;
            return sent([&](int fd)
            {
                return setObjectRA(fd, value);
            });
        }

        std::string dec(int format, double value)
        {
            eq_format = format;
            return sent([&](int fd)
            {
                return setObjectDEC(fd, value);
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

TEST_F(LX200DriverCommands, SiteLongitudeKeepsTheSignBetweenZeroAndOneDegreeEast)
{
    // 0.5 degrees East, and 0 deg 7' 12" East (Cambridge, UK)
    EXPECT_EQ(longitude(LX200_GEO_SHORT_FORMAT, 0.5), ":Sg-00*30#");
    EXPECT_EQ(longitude(LX200_GEO_SHORT_FORMAT, 0.5, true), ":Sg -00*30#");
    EXPECT_EQ(longitude(LX200_GEO_LONG_FORMAT, 0.12), ":Sg-00*07:12#");
    EXPECT_EQ(longitude(LX200_GEO_LONG_FORMAT, 0.12, true), ":Sg -00*07:12#");
    EXPECT_EQ(longitude(LX200_GEO_LONGER_FORMAT, 0.12), ":Sg-00*07:12.0#");
    EXPECT_EQ(longitude(LX200_GEO_LONGER_FORMAT, 0.12, true), ":Sg -00*07:12.0#");
}

TEST_F(LX200DriverCommands, SiteLongitudeIsUnchangedOutsideZeroToOneDegreeEast)
{
    // West longitudes have three digits, East ones a minus sign and at least two digits
    EXPECT_EQ(longitude(LX200_GEO_SHORT_FORMAT, -0.5), ":Sg000*30#");
    EXPECT_EQ(longitude(LX200_GEO_SHORT_FORMAT, 0.0), ":Sg000*00#");
    EXPECT_EQ(longitude(LX200_GEO_SHORT_FORMAT, -71.5), ":Sg071*30#");
    EXPECT_EQ(longitude(LX200_GEO_SHORT_FORMAT, -120.25, true), ":Sg 120*15#");
    EXPECT_EQ(longitude(LX200_GEO_SHORT_FORMAT, 5.5), ":Sg-05*30#");
    EXPECT_EQ(longitude(LX200_GEO_SHORT_FORMAT, 151.2), ":Sg-151*12#");
    EXPECT_EQ(longitude(LX200_GEO_LONG_FORMAT, 2.35), ":Sg-02*21:00#");
    EXPECT_EQ(longitude(LX200_GEO_LONG_FORMAT, -71.5), ":Sg071*30:00#");
    EXPECT_EQ(longitude(LX200_GEO_LONGER_FORMAT, 151.25), ":Sg-151*15:00.0#");
    EXPECT_EQ(longitude(LX200_GEO_LONGER_FORMAT, -0.5, true), ":Sg 000*30:00.0#");
}

TEST_F(LX200DriverCommands, LongerFormatsNeverSendSixtySeconds)
{
    // 0 59' 59.964" is 1 00' 00.0" with tenths of arcseconds, not 0 59' 60.0"
    EXPECT_EQ(latitude(LX200_GEO_LONGER_FORMAT, -0.99999), ":St-01*00:00.0#");
    EXPECT_EQ(latitude(LX200_GEO_LONGER_FORMAT, 45.99999), ":St+46*00:00.0#");
    EXPECT_EQ(longitude(LX200_GEO_LONGER_FORMAT, 151.2), ":Sg-151*12:00.0#");
    EXPECT_EQ(longitude(LX200_GEO_LONGER_FORMAT, -71.99999), ":Sg072*00:00.0#");
    EXPECT_EQ(dec(LX200_EQ_LONGER_FORMAT, -0.99999), ":Sd-01*00:00.0#");
    EXPECT_EQ(dec(LX200_EQ_LONGER_FORMAT, 45.99999), ":Sd+46*00:00.0#");
    // 5h 59m 59.9964s is 6h 00m 00.00s with hundredths of seconds
    EXPECT_EQ(ra(LX200_EQ_LONGER_FORMAT, 5.999999), ":Sr06:00:00.00#");
}

TEST_F(LX200DriverCommands, LongerFormatsAreUnchangedAwayFromSixtySeconds)
{
    EXPECT_EQ(latitude(LX200_GEO_LONGER_FORMAT, 12.3456789), ":St+12*20:44.4#");
    EXPECT_EQ(longitude(LX200_GEO_LONGER_FORMAT, -12.3456789), ":Sg012*20:44.4#");
    EXPECT_EQ(dec(LX200_EQ_LONGER_FORMAT, -12.3456789), ":Sd-12*20:44.4#");
    EXPECT_EQ(dec(LX200_EQ_LONGER_FORMAT, -0.5), ":Sd-00*30:00.0#");
    EXPECT_EQ(dec(LX200_EQ_LONGER_FORMAT, 89.5), ":Sd+89*30:00.0#");
    EXPECT_EQ(ra(LX200_EQ_LONGER_FORMAT, 1.2345678), ":Sr01:14:04.44#");
    EXPECT_EQ(ra(LX200_EQ_LONGER_FORMAT, 12.5), ":Sr12:30:00.00#");
}

TEST_F(LX200DriverCommands, RightAscensionRoundedUpToTwentyFourHoursIsZero)
{
    EXPECT_EQ(ra(LX200_EQ_LONGER_FORMAT, 23.9999999), ":Sr00:00:00.00#");
    EXPECT_EQ(ra(LX200_EQ_LONG_FORMAT, 23.99999), ":Sr00:00:00#");
    EXPECT_EQ(ra(LX200_EQ_LONG_FORMAT, 23.5), ":Sr23:30:00#");
    EXPECT_EQ(ra(LX200_EQ_SHORT_FORMAT, 23.99999), ":Sr00:00.0#");
    EXPECT_EQ(ra(LX200_EQ_SHORT_FORMAT, 23.5), ":Sr23:30.0#");
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

// LX200_OnStep with the functions under test made reachable.
class TestOnStep : public LX200_OnStep
{
    public:
        using LX200_OnStep::setSiteLatitude;
        using LX200_OnStep::setSiteLongitude;

        void setHighPrecision(bool enabled)
        {
            OSHighPrecision = enabled;
        }
};

class OnStepCommands : public ::testing::Test
{
    protected:
        std::string latitude(bool highPrecision, double value)
        {
            m_OnStep.setHighPrecision(highPrecision);
            return sent([&](int fd)
            {
                return m_OnStep.setSiteLatitude(fd, value);
            });
        }

        // value is an OnStep longitude, 0 to 360 degrees West
        std::string longitude(bool highPrecision, double value)
        {
            m_OnStep.setHighPrecision(highPrecision);
            return sent([&](int fd)
            {
                return m_OnStep.setSiteLongitude(fd, value);
            });
        }

        std::string sent(const std::function<int(int)> &command)
        {
            m_Device.clear();
            EXPECT_EQ(command(m_Device.driverFD()), 0);
            auto commands = m_Device.commands();
            EXPECT_EQ(commands.size(), 1u);
            return commands.empty() ? std::string() : commands.back();
        }

        TestOnStep m_OnStep;
        FakeSerialDevice m_Device {accept};
};

TEST_F(OnStepCommands, HighPrecisionNeverSendsSixtySeconds)
{
    // 45 59' 59.9996" is 46 00' 00.00" with hundredths of arcseconds, not 45 59' 60.00"
    EXPECT_EQ(latitude(true, 45.9999999), ":St+46:00:0.00#");
    EXPECT_EQ(longitude(true, 100.9999999), ":Sg101:00:0.00#");
}

TEST_F(OnStepCommands, HighPrecisionIsUnchangedAwayFromSixtySeconds)
{
    EXPECT_EQ(latitude(true, 12.3456789), ":St+12:20:44.44#");
    EXPECT_EQ(latitude(true, -33.865), ":St-33:51:54.00#");
    EXPECT_EQ(longitude(true, 12.3456789), ":Sg012:20:44.44#");
    EXPECT_EQ(longitude(true, 288.5), ":Sg288:30:0.00#");
    EXPECT_EQ(latitude(false, -33.865), ":St-33:51#");
    EXPECT_EQ(longitude(false, 288.5), ":Sg288:30#");
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
