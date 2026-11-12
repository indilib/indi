/*
    OnStepX INDI Driver — test_onstepxguide.cpp
    Copyright (C) 2026 Ed Lee

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/

/*
    OnStepX INDI Driver — Guide and weather helper unit tests

    Tests cover:
      - Guide rate mapping onto the firmware's pulse rates (:R0/:R1/:R2)
      - Pulse commands (:Mg<dir><ms>#, no reply) and the int16 ms limit
      - Guide rate read (:GX90#, text)
      - SensorData struct (WeatherInterface refactor sanity check)

    Firmware reference: OnStepX Guide.command.cpp.  GuideRateSelect index
    0 = 0.25x, 1 = 0.5x, 2 = 1x, ... 5 = 8x.
*/

#include <gtest/gtest.h>

#include "OnStepXComm.h"
#define private public
#include "OnStepXGuide.h"
#undef private
#include "OnStepXWeather.h"
#include "MockFirmware.h"
#include "SavedConfig.h"

// ---------------------------------------------------------------------------
// 1. Rate mapping — regression: 0.5x used to map to index 5 (8x)
// ---------------------------------------------------------------------------
TEST(GuideRate, MapsToFirmwarePulseRates)
{
    EXPECT_EQ(OnStepXGuide::rateToIndex(0.25), 0);
    EXPECT_EQ(OnStepXGuide::rateToIndex(0.5), 1);
    EXPECT_EQ(OnStepXGuide::rateToIndex(1.0), 2);

    // nearest
    EXPECT_EQ(OnStepXGuide::rateToIndex(0.3), 0);
    EXPECT_EQ(OnStepXGuide::rateToIndex(0.4), 1);
    EXPECT_EQ(OnStepXGuide::rateToIndex(0.7), 1);
    EXPECT_EQ(OnStepXGuide::rateToIndex(0.8), 2);

    EXPECT_DOUBLE_EQ(OnStepXGuide::indexToRate(0), 0.25);
    EXPECT_DOUBLE_EQ(OnStepXGuide::indexToRate(1), 0.5);
    EXPECT_DOUBLE_EQ(OnStepXGuide::indexToRate(2), 1.0);
}

TEST(GuideRate, NeverSelectsAboveOneX)
{
    for (double r = 0.0; r <= 1.0; r += 0.01)
        EXPECT_LE(OnStepXGuide::rateToIndex(r), 2) << r;
}

// ---------------------------------------------------------------------------
// 2. Pulses — :Mg (no reply), not :MG (bare '1' that leaked into the next reply)
// ---------------------------------------------------------------------------
class GuidePulseTest : public ::testing::Test
{
    protected:
        void SetUp() override
        {
            comm.setFd(fw.start());
            guide.setComm(&comm);
        }
        void TearDown() override
        {
            fw.stop();
        }

        MockFirmware fw;
        OnStepXComm  comm;
        OnStepXGuide guide;
};

TEST_F(GuidePulseTest, DirectionsAndFormat)
{
    EXPECT_EQ(guide.guideNorth(500), IPS_BUSY);
    EXPECT_EQ(guide.guideSouth(250), IPS_BUSY);
    EXPECT_EQ(guide.guideEast(1000), IPS_BUSY);
    EXPECT_EQ(guide.guideWest(0), IPS_BUSY);

    EXPECT_TRUE(fw.waitFor(":Mgw0"));
    const auto cmds = fw.commands();
    ASSERT_EQ(cmds.size(), 4u);
    EXPECT_EQ(cmds[0], ":Mgn500");
    EXPECT_EQ(cmds[1], ":Mgs250");
    EXPECT_EQ(cmds[2], ":Mge1000");
    EXPECT_EQ(cmds[3], ":Mgw0");
}

TEST_F(GuidePulseTest, LongPulseClampedToFirmwareLimit)
{
    EXPECT_EQ(guide.guideNorth(60000), IPS_BUSY);
    EXPECT_TRUE(fw.waitFor(":Mgn32767"));
}

TEST_F(GuidePulseTest, NotConnectedIsAlert)
{
    comm.setFd(-1);
    EXPECT_EQ(guide.guideNorth(500), IPS_ALERT);
}

// Regression (hardware, 2026-11-11): after a West pulse, a client that sent
// only TIMED_GUIDE_E got another West pulse, because GuiderInterface acts
// on the first non-zero element and the West length was still set.
namespace
{
struct GuiderDevice : public INDI::DefaultDevice, public INDI::GuiderInterface
{
    GuiderDevice() : INDI::GuiderInterface(this)
    {
        setDeviceName("OnStepX");
    }
    const char *getDefaultName() override
    {
        return "OnStepX";
    }
    bool initProperties() override
    {
        INDI::DefaultDevice::initProperties();
        INDI::GuiderInterface::initProperties("Motion Control");
        return true;
    }
    IPState GuideNorth(uint32_t ms) override
    {
        return guide->guideNorth(ms);
    }
    IPState GuideSouth(uint32_t ms) override
    {
        return guide->guideSouth(ms);
    }
    IPState GuideEast(uint32_t ms) override
    {
        return guide->guideEast(ms);
    }
    IPState GuideWest(uint32_t ms) override
    {
        return guide->guideWest(ms);
    }
    using INDI::GuiderInterface::GuideWENP;
    using INDI::GuiderInterface::processNumber;
    OnStepXGuide *guide { nullptr };
};
}

TEST_F(GuidePulseTest, EastAfterWestSendsEast)
{
    GuiderDevice dev;
    dev.guide = &guide;
    dev.initProperties();
    dev.defineProperty(dev.GuideWENP);
    guide.setDevice(&dev);
    guide.setGuiderInterface(&dev);

    double w[] = { 500 };
    char *wn[] = { const_cast<char *>("TIMED_GUIDE_W") };
    ASSERT_TRUE(dev.processNumber("OnStepX", "TELESCOPE_TIMED_GUIDE_WE", w, wn, 1));
    guide.pulseDone(INDI_EQ_AXIS::AXIS_RA);     // what the pulse timer calls
    EXPECT_DOUBLE_EQ(dev.GuideWENP[0].getValue(), 0);
    EXPECT_DOUBLE_EQ(dev.GuideWENP[1].getValue(), 0);

    double e[] = { 700 };
    char *en[] = { const_cast<char *>("TIMED_GUIDE_E") };
    ASSERT_TRUE(dev.processNumber("OnStepX", "TELESCOPE_TIMED_GUIDE_WE", e, en, 1));
    EXPECT_TRUE(fw.waitFor(":Mge700"));
    const auto cmds = fw.commands();
    ASSERT_EQ(cmds.size(), 2u);
    EXPECT_EQ(cmds[0], ":Mgw500");
    EXPECT_EQ(cmds[1], ":Mge700");
}

// ---------------------------------------------------------------------------
// 3. SensorData struct — sanity check for the WeatherInterface refactor
// ---------------------------------------------------------------------------
TEST(SensorData, AnyOkFalseWhenEmpty)
{
    SensorData d;
    EXPECT_FALSE(d.anyOk());
    EXPECT_FALSE(d.temp.ok);
    EXPECT_FALSE(d.pressure.ok);
    EXPECT_FALSE(d.humidity.ok);
    EXPECT_FALSE(d.dewpoint.ok);
}

TEST(SensorData, AnyOkTrueWhenTempOnly)
{
    SensorData d;
    d.temp = {true, 20.5};
    EXPECT_TRUE(d.anyOk());
    EXPECT_DOUBLE_EQ(d.temp.value, 20.5);
    EXPECT_FALSE(d.pressure.ok);
}

// readSensors uses :GX9x# text replies; "nan" (failed read) is not a reading.
// MCU temperature is not weather: readSensors() must not ask for it.
TEST(SensorData, ReadSensorsRejectsBareZero)
{
    MockFirmware fw;
    fw.setResponse(":GX9A", "12.5#");
    fw.setResponse(":GX9B", "1010.0#");
    fw.setResponse(":GX9E", "8.5#");
    fw.setResponse(":GX9C", "nan#");          // failed humidity read
    OnStepXComm comm;
    comm.setFd(fw.start());
    OnStepXWeather weather;
    weather.setComm(&comm);

    SensorData d = weather.readSensors();
    EXPECT_TRUE(d.temp.ok);
    EXPECT_DOUBLE_EQ(d.temp.value, 12.5);
    EXPECT_TRUE(d.pressure.ok);
    EXPECT_FALSE(d.humidity.ok);
    fw.stop();
    EXPECT_FALSE(fw.received(":GX9F"));
}

// :GX9F# answers a bare '0' when the MCU has no temperature sensor.
TEST(McuTemp, BareZeroIsNotAReading)
{
    MockFirmware fw;
    fw.setResponse(":GX9F", "0");
    OnStepXComm comm;
    comm.setFd(fw.start());
    OnStepXWeather weather;
    weather.setComm(&comm);
    EXPECT_FALSE(weather.readMcuTemp().ok);
    fw.stop();
}

TEST(McuTemp, Reading)
{
    MockFirmware fw;
    fw.setResponse(":GX9F", "53#");
    OnStepXComm comm;
    comm.setFd(fw.start());
    OnStepXWeather weather;
    weather.setComm(&comm);
    const WeatherReading r = weather.readMcuTemp();
    EXPECT_TRUE(r.ok);
    EXPECT_DOUBLE_EQ(r.value, 53.0);
    fw.stop();
}

// :R<n># stores the pulse-guide rate in the firmware's NV: not in the config.
TEST(GuideConfig, GuideRateNotSaved)
{
    OnStepXGuide guide;
    guide.initProperties();
    EXPECT_EQ(savedConfig([&](FILE * fp)
    {
        guide.saveConfig(fp);
    }), "");
}

// Manual weather values and DUT1 are RAM-only in the firmware: the config
// restores them after the controller reboots.
TEST(WeatherConfig, ManualWeatherAndDut1Saved)
{
    OnStepXWeather weather;
    weather.initProperties();
    const std::string cfg = savedConfig([&](FILE * fp)
    {
        weather.saveConfig(fp);
    });
    EXPECT_TRUE(savesProperty(cfg, "OSX_WEATHER_SET")) << cfg;
    EXPECT_TRUE(savesProperty(cfg, "OSX_DUT1")) << cfg;
    EXPECT_FALSE(savesProperty(cfg, "OSX_MCU_TEMP")) << cfg;
}

int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
