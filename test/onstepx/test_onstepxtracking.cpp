/*
    OnStepX INDI Driver — test_onstepxtracking.cpp
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
    OnStepX INDI Driver — Stage 8 unit tests: Tracking Control

    Tests verify the command strings sent for each tracking operation.
    OnStepXTracking is a plain-C++ helper so it can be exercised directly
    against MockFirmware without a full INDI device context.

    Properties are initialised with a null device pointer which means
    property labels are empty — that's fine for command-level tests.
*/

#include <gtest/gtest.h>

#include <defaultdevice.h>

#include "OnStepXComm.h"
#include "OnStepXTracking.h"
#include "MockFirmware.h"
#include "SavedConfig.h"

#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// Fixture.  The commands exercised here reply a bare '1'/'0'.
// ---------------------------------------------------------------------------
class TrackingTest : public ::testing::Test
{
    protected:
        void SetUp() override
        {
            m_comm.setFd(m_fw.start());
            m_tracking.setComm(&m_comm);
            m_tracking.initProperties();
        }

        void TearDown() override
        {
            m_fw.stop();
        }

        // Press a switch and require exactly one command to reach the firmware.
        void expectSingleCommand(const char *property, ISState *states, const char **names, int n, const char *command)
        {
            m_fw.setResponse(command, "1");
            EXPECT_TRUE(m_tracking.handleSwitch(property, states, const_cast<char **>(names), n));
            EXPECT_EQ(m_fw.commands(), std::vector<std::string> { command });
        }

        MockFirmware    m_fw;
        OnStepXComm     m_comm;
        OnStepXTracking m_tracking;
};

// ---------------------------------------------------------------------------
// Track compensation commands
// ---------------------------------------------------------------------------
TEST_F(TrackingTest, TrackCompFull_SendsTo)
{
    ISState states[3] = { ISS_ON, ISS_OFF, ISS_OFF };
    const char *names[3] = { "TRACK_COMP_FULL", "TRACK_COMP_REFRACTION", "TRACK_COMP_OFF" };

    expectSingleCommand("OSX_TRACK_COMP", states, names, 3, ":To");
}

TEST_F(TrackingTest, TrackCompRefraction_SendsTr)
{
    ISState states[3] = { ISS_OFF, ISS_ON, ISS_OFF };
    const char *names[3] = { "TRACK_COMP_FULL", "TRACK_COMP_REFRACTION", "TRACK_COMP_OFF" };

    expectSingleCommand("OSX_TRACK_COMP", states, names, 3, ":Tr");
}

TEST_F(TrackingTest, TrackCompOff_SendsTn)
{
    ISState states[3] = { ISS_OFF, ISS_OFF, ISS_ON };
    const char *names[3] = { "TRACK_COMP_FULL", "TRACK_COMP_REFRACTION", "TRACK_COMP_OFF" };

    expectSingleCommand("OSX_TRACK_COMP", states, names, 3, ":Tn");
}

// ---------------------------------------------------------------------------
// Track axis commands
// ---------------------------------------------------------------------------
TEST_F(TrackingTest, TrackAxisSingle_SendsT1)
{
    ISState states[2] = { ISS_ON, ISS_OFF };
    const char *names[2] = { "TRACK_AXIS_SINGLE", "TRACK_AXIS_DUAL" };

    expectSingleCommand("OSX_TRACK_COMP_AXES", states, names, 2, ":T1");
}

TEST_F(TrackingTest, TrackAxisDual_SendsT2)
{
    ISState states[2] = { ISS_OFF, ISS_ON };
    const char *names[2] = { "TRACK_AXIS_SINGLE", "TRACK_AXIS_DUAL" };

    expectSingleCommand("OSX_TRACK_COMP_AXES", states, names, 2, ":T2");
}

// ---------------------------------------------------------------------------
// AutoFlip commands
// ---------------------------------------------------------------------------
TEST_F(TrackingTest, AutoFlipOn_SendsSX95_1)
{
    ISState states[2] = { ISS_OFF, ISS_ON };
    const char *names[2] = { "AUTO_FLIP_OFF", "AUTO_FLIP_ON" };

    expectSingleCommand("OSX_AUTO_FLIP", states, names, 2, ":SX95,1");
}

TEST_F(TrackingTest, AutoFlipOff_SendsSX95_0)
{
    ISState states[2] = { ISS_ON, ISS_OFF };
    const char *names[2] = { "AUTO_FLIP_OFF", "AUTO_FLIP_ON" };

    expectSingleCommand("OSX_AUTO_FLIP", states, names, 2, ":SX95,0");
}

// ---------------------------------------------------------------------------
// A mount that does not flip (Alt-Az: :GX94# ends " N") gets neither auto
// flip nor the pier preference, and a stale config entry sends nothing.
// ---------------------------------------------------------------------------
namespace
{
class TestDevice : public INDI::DefaultDevice
{
    public:
        TestDevice()
        {
            setDeviceName("Test");
        }
    protected:
        const char *getDefaultName() override
        {
            return "Test";
        }
};
}

TEST_F(TrackingTest, NoFlipPropertiesOnMountThatDoesNotFlip)
{
    TestDevice dev;
    m_tracking.setDevice(&dev);
    m_tracking.initProperties();
    m_tracking.updateProperties(true, false);

    EXPECT_FALSE(dev.getProperty("OSX_AUTO_FLIP").isValid());
    EXPECT_FALSE(dev.getProperty("OSX_PREFERRED_PIER").isValid());
    EXPECT_TRUE(dev.getProperty("OSX_TRACK_COMP").isValid());
    for (const auto &c : m_fw.commands())
        EXPECT_TRUE(c != ":GX95" && c != ":GX96") << "read " << c << " on a mount that does not flip";

    const size_t before = m_fw.commands().size();
    ISState states[2] = { ISS_OFF, ISS_ON };
    const char *names[2] = { "AUTO_FLIP_OFF", "AUTO_FLIP_ON" };
    EXPECT_TRUE(m_tracking.handleSwitch("OSX_AUTO_FLIP", states, const_cast<char **>(names), 2));
    const char *pier[1] = { "PIER_EAST" };
    ISState on[1] = { ISS_ON };
    EXPECT_TRUE(m_tracking.handleSwitch("OSX_PREFERRED_PIER", on, const_cast<char **>(pier), 1));
    EXPECT_EQ(m_fw.commands().size(), before) << "nothing sent to a mount that does not flip";
}

TEST_F(TrackingTest, FlipPropertiesOnFlippingMount)
{
    TestDevice dev;
    m_tracking.setDevice(&dev);
    m_tracking.initProperties();
    m_tracking.updateProperties(true, true);

    EXPECT_TRUE(dev.getProperty("OSX_AUTO_FLIP").isValid());
    EXPECT_TRUE(dev.getProperty("OSX_PREFERRED_PIER").isValid());
}

// Compensation, auto flip, pier preference and max slew rate are in the
// firmware's NV (MOUNT_SETTINGS, GOTO_SETTINGS): never in the INDI config.
TEST_F(TrackingTest, SavesNothingToTheConfig)
{
    EXPECT_EQ(savedConfig([&](FILE * fp)
    {
        m_tracking.saveConfig(fp);
    }), "");
}

// ---------------------------------------------------------------------------
// Preferred pier side commands
// ---------------------------------------------------------------------------
TEST_F(TrackingTest, PreferredPierWest_SendsSX96_W)
{
    ISState states[3] = { ISS_ON, ISS_OFF, ISS_OFF };
    const char *names[3] = { "PIER_WEST", "PIER_EAST", "PIER_BEST" };

    expectSingleCommand("OSX_PREFERRED_PIER", states, names, 3, ":SX96,W");
}

TEST_F(TrackingTest, PreferredPierEast_SendsSX96_E)
{
    ISState states[3] = { ISS_OFF, ISS_ON, ISS_OFF };
    const char *names[3] = { "PIER_WEST", "PIER_EAST", "PIER_BEST" };

    expectSingleCommand("OSX_PREFERRED_PIER", states, names, 3, ":SX96,E");
}

TEST_F(TrackingTest, PreferredPierBest_SendsSX96_B)
{
    ISState states[3] = { ISS_OFF, ISS_OFF, ISS_ON };
    const char *names[3] = { "PIER_WEST", "PIER_EAST", "PIER_BEST" };

    expectSingleCommand("OSX_PREFERRED_PIER", states, names, 3, ":SX96,B");
}

// ---------------------------------------------------------------------------
// Stage 15: OSX_SLEW_RATE_MAX tests
// ---------------------------------------------------------------------------
// OSX_SLEW_RATE_MAX: read the current rate (:GX97#, deg/s) and step period
// (:GX92#, us/step), then set the scaled step period with :SX92,n#.
// Regression: the driver used to send the non-existent :Rs and :GX4C.
TEST(SlewRateMax, ScalesStepPeriod)
{
    MockFirmware fw;
    fw.setResponse(":GX97", "2.0#");
    fw.setResponse(":GX92", "100.000#");
    fw.setResponse(":SX92,50.000", "1");
    OnStepXComm comm;
    comm.setFd(fw.start());
    OnStepXTracking tracking;
    tracking.setComm(&comm);
    tracking.initProperties();

    double values[] = { 4.0 };                 // twice as fast -> half the us/step
    const char *names[] = { "SLEW_RATE_MAX" };
    EXPECT_TRUE(tracking.handleNumber("OSX_SLEW_RATE_MAX", values, const_cast<char**>(names), 1));
    fw.stop();

    EXPECT_TRUE(fw.received(":SX92,50.000"));
    EXPECT_EQ(IPS_OK, tracking.slewRateMaxNP().getState());
    // Display shows what the firmware reports after the change
    EXPECT_DOUBLE_EQ(2.0, tracking.slewRateMaxNP()[0].getValue());
}

TEST(SlewRateMax, RefusedWhileSlewingSetsAlert)
{
    MockFirmware fw;
    fw.setResponse(":GX97", "2.0#");
    fw.setResponse(":GX92", "100.000#");
    fw.setResponse(":SX92,40.000", "0");       // firmware refuses while slewing
    OnStepXComm comm;
    comm.setFd(fw.start());
    OnStepXTracking tracking;
    tracking.setComm(&comm);
    tracking.initProperties();

    double values[] = { 5.0 };
    const char *names[] = { "SLEW_RATE_MAX" };
    tracking.handleNumber("OSX_SLEW_RATE_MAX", values, const_cast<char**>(names), 1);
    fw.stop();

    EXPECT_EQ(IPS_ALERT, tracking.slewRateMaxNP().getState());
}

// ---------------------------------------------------------------------------
// A refused command raises an alert (unscripted commands get a bare '0').
// ---------------------------------------------------------------------------
TEST_F(TrackingTest, TrackCompRefusedSetsAlert)
{
    ISState states[3] = { ISS_ON, ISS_OFF, ISS_OFF };
    const char *names[3] = { "TRACK_COMP_FULL", "TRACK_COMP_REFRACTION", "TRACK_COMP_OFF" };

    m_tracking.handleSwitch("OSX_TRACK_COMP", states, const_cast<char **>(names), 3);

    EXPECT_TRUE(m_fw.received(":To"));
    EXPECT_EQ(IPS_ALERT, m_tracking.trackCompSP().getState());
}

// ---------------------------------------------------------------------------
// OSX_TRACK_FREQ: :GT# is n.nnnnn Hz, or "0" while not tracking.
// ---------------------------------------------------------------------------
TEST_F(TrackingTest, ReadSettingsQueriesGT)
{
    m_fw.setResponse(":GX95", "0#");
    m_fw.setResponse(":GX96", "B#");
    m_fw.setResponse(":GX97", "2.0#");
    m_fw.setResponse(":GT",   "60.16427#");

    m_tracking.readSettings();

    EXPECT_TRUE(m_fw.received(":GT"));
    EXPECT_NEAR(60.16427, m_tracking.trackFreqNP()[0].getValue(), 0.00001);
}

TEST_F(TrackingTest, SyncStatusPollsGTEvery10Calls)
{
    m_fw.setResponse(":GT", "59.90000#");

    MountStatus s;
    s.trackComp = MountStatus::TrackComp::NONE;
    for (int i = 0; i < 10; i++)
        m_tracking.syncStatus(s);

    int gtCount = 0;
    for (const auto &c : m_fw.commands())
        if (c == ":GT")
            gtCount++;
    EXPECT_EQ(1, gtCount) << "Expected exactly one :GT# query after 10 syncStatus calls";
    EXPECT_NEAR(59.9, m_tracking.trackFreqNP()[0].getValue(), 0.00001);
}

int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

// ---------------------------------------------------------------------------
// Custom rates as firmware offsets.  The driver reads :GXTR#/:GXTD# at
// connect to show Custom when the controller already has offsets.
// ---------------------------------------------------------------------------
TEST(TrackRateOffsets, SiderealIsZeroOffset)
{
    EXPECT_NEAR(OnStepXTracking::raOffsetFromRate(OnStepXTracking::SIDEREAL_ARCSEC_PER_SEC), 0.0, 1e-9);
    EXPECT_NEAR(OnStepXTracking::raRateFromOffset(0.0), OnStepXTracking::SIDEREAL_ARCSEC_PER_SEC, 1e-9);
    EXPECT_NEAR(OnStepXTracking::deRateFromOffset(0.0), 0.0, 1e-12);
}

TEST(TrackRateOffsets, RoundTrip)
{
    const double raRates[] = { 0.0, 14.5, 15.0, 15.5, 30.0 };
    const double deRates[] = { -5.0, -1.0, 0.0, 1.0, 5.0 };
    for (double ra : raRates)
        EXPECT_NEAR(OnStepXTracking::raRateFromOffset(OnStepXTracking::raOffsetFromRate(ra)), ra, 1e-9) << ra;
    for (double de : deRates)
        EXPECT_NEAR(OnStepXTracking::deRateFromOffset(OnStepXTracking::deOffsetFromRate(de)), de, 1e-9) << de;
}

// RA 15.0, Dec 1.0 arcsec/s (what the GEM end-to-end test sets): the firmware
// then reports these offsets, and a Dec offset makes the mount drift north.
TEST(TrackRateOffsets, MatchesFirmwareSign)
{
    EXPECT_GT(OnStepXTracking::raOffsetFromRate(15.0), 0.0) << "slower than sidereal is +ve";
    EXPECT_NEAR(OnStepXTracking::raOffsetFromRate(15.0), 0.0409, 1e-3);
    EXPECT_NEAR(OnStepXTracking::deOffsetFromRate(1.0), 0.99727, 1e-4);
}

// :GT# reports the base rate in Hz while tracking (Mount.command.cpp); the
// status only reports it while compensation is off.
TEST(TrackRateOffsets, BaseRateFromHz)
{
    using B = OnStepXTracking::BaseRate;
    EXPECT_EQ(OnStepXTracking::baseRateFromHz(57.90000), B::LUNAR);
    EXPECT_EQ(OnStepXTracking::baseRateFromHz(60.00000), B::SOLAR);
    EXPECT_EQ(OnStepXTracking::baseRateFromHz(60.13600), B::KING);
    EXPECT_EQ(OnStepXTracking::baseRateFromHz(60.16427), B::SIDEREAL);
    EXPECT_EQ(OnStepXTracking::baseRateFromHz(60.15000), B::UNKNOWN) << "e.g. after :T+# clock trimming";
    EXPECT_EQ(OnStepXTracking::baseRateFromHz(0.0), B::UNKNOWN);
}
