/*
    Regression test for the telescope simulator: alignment sync points recorded for one mount
    geometry must not be reused after switching between ALTAZ and an equatorial mount type.

    SPDX-License-Identifier: LGPL-2.1-or-later
*/

#include <gtest/gtest.h>

#include <cstdio>
#include <cstdlib>
#include <string>
#include <unistd.h>

#include "indilogger.h"
#include "lilxml.h"
#include "drivers/telescope/telescope_simulator.h"

char _me[] = "MockScopeSimDriver";
char *me = _me;

using INDI::AlignmentSubsystem::AlignmentDatabaseEntry;

class MockScopeSimDriver : public ScopeSim
{
    public:
        MockScopeSimDriver() : ScopeSim()
        {
            initProperties();
            ISGetProperties(getDeviceName());
        }

        // Select a mount type through the regular client path (TELESCOPE_MOUNT_TYPE switch).
        void selectMountType(const char *type)
        {
            const char *types[] = { "ALTAZ", "EQ_FORK", "EQ_GEM" };
            ISState states[3];
            char *names[3];
            for (int i = 0; i < 3; i++)
            {
                states[i] = strcmp(types[i], type) == 0 ? ISS_ON : ISS_OFF;
                names[i]  = const_cast<char *>(types[i]);
            }
            ASSERT_TRUE(ISNewSwitch(getDeviceName(), "TELESCOPE_MOUNT_TYPE", states, names, 3));
        }

        bool syncAt(double ra, double dec)
        {
            return Sync(ra, dec);
        }

        void setLocation(double latitude, double longitude)
        {
            updateLocation(latitude, longitude, 200);
        }

        const AlignmentDatabaseEntry &lastSyncPoint()
        {
            return GetAlignmentDatabase().back();
        }

        void addSyncPoint()
        {
            AlignmentDatabaseEntry entry;
            entry.ObservationJulianDate = 2461300.5;
            entry.RightAscension        = 9.5;
            entry.Declination           = 45.0;
            entry.PrivateDataSize       = 0;
            GetAlignmentDatabase().push_back(entry);
        }

        size_t syncPoints()
        {
            return GetAlignmentDatabase().size();
        }

        // Deliver a property update from another driver, as indiserver does for snooped properties.
        void snoop(const std::string &xml)
        {
            LilXML *lp = newLilXML();
            char errmsg[MAXRBUF] = {0};
            std::string buffer = xml;
            XMLEle **nodes = parseXMLChunk(lp, buffer.data(), static_cast<int>(buffer.size()), errmsg);
            ASSERT_NE(nodes, nullptr) << errmsg;
            for (int i = 0; nodes[i] != nullptr; i++)
            {
                ISSnoopDevice(nodes[i]);
                delXMLEle(nodes[i]);
            }
            free(nodes);
            delLilXML(lp);
        }

        void setActivePAC(const char *device)
        {
            char *texts[] = { const_cast<char *>(device) };
            char *names[] = { const_cast<char *>("PAC_DEVICE") };
            ASSERT_TRUE(ISNewText(getDeviceName(), "ACTIVE_PAC", texts, names, 1));
        }

        void setPolarError(double ma, double me)
        {
            double values[] = { ma, me };
            char *names[] = { const_cast<char *>("MM_MA"), const_cast<char *>("MM_ME") };
            ASSERT_TRUE(ISNewNumber(getDeviceName(), "MOUNT_MODEL", values, names, 2));
        }

        double mountModel(const char *element)
        {
            return getNumber("MOUNT_MODEL").findWidgetByName(element)->getValue();
        }

        void saveAllConfig()
        {
            ASSERT_TRUE(saveConfig());
        }

        void setDomePolicy(const char *policy)
        {
            ISState states[] = { strcmp(policy, "DOME_IGNORED") == 0 ? ISS_ON : ISS_OFF,
                                 strcmp(policy, "DOME_LOCKS") == 0 ? ISS_ON : ISS_OFF
                               };
            char *names[] = { const_cast<char *>("DOME_IGNORED"), const_cast<char *>("DOME_LOCKS") };
            ASSERT_TRUE(ISNewSwitch(getDeviceName(), "DOME_POLICY", states, names, 2));
        }

        std::string domePolicy()
        {
            auto on = getSwitch("DOME_POLICY").findOnSwitch();
            return on ? on->getName() : "";
        }
};

TEST(TelescopeSimulatorTest, switchingBetweenAltAzAndEquatorialClearsSyncPoints)
{
    MockScopeSimDriver sim;

    sim.selectMountType("EQ_GEM");
    sim.addSyncPoint();
    sim.addSyncPoint();
    ASSERT_EQ(sim.syncPoints(), 2u);

    // Equatorial -> ALTAZ: RA/Dec-encoded points are meaningless for an Az/Alt mount.
    sim.selectMountType("ALTAZ");
    EXPECT_EQ(sim.syncPoints(), 0u);

    // ALTAZ -> equatorial: Az/Alt-encoded points are meaningless for an equatorial mount.
    sim.addSyncPoint();
    sim.selectMountType("EQ_FORK");
    EXPECT_EQ(sim.syncPoints(), 0u);
}

TEST(TelescopeSimulatorTest, switchingBetweenEquatorialTypesKeepsSyncPoints)
{
    MockScopeSimDriver sim;

    sim.selectMountType("EQ_GEM");
    sim.addSyncPoint();
    ASSERT_EQ(sim.syncPoints(), 1u);

    // EQ_GEM and EQ_FORK use the same encoder RA/Dec encoding.
    sim.selectMountType("EQ_FORK");
    EXPECT_EQ(sim.syncPoints(), 1u);
}

// Re-syncing at (practically) the same place must update the pointing model with the new plate-solved
// position. Previously the new sync point was silently dropped as a duplicate while Sync() still reported
// success, so the old position stayed in the model.
static void checkResyncReplacesOlderPoint(const char *mountType)
{
    MockScopeSimDriver sim;
    sim.setLocation(48.2, 16.4);
    sim.selectMountType(mountType);

    ASSERT_TRUE(sim.syncAt(9.50, 45.00));
    ASSERT_EQ(sim.syncPoints(), 1u);

    // Same mount position, slightly different solved sky position.
    ASSERT_TRUE(sim.syncAt(9.51, 45.05));
    ASSERT_EQ(sim.syncPoints(), 1u);
    EXPECT_NEAR(sim.lastSyncPoint().RightAscension, 9.51, 1e-9);
    EXPECT_NEAR(sim.lastSyncPoint().Declination, 45.05, 1e-9);
}

TEST(TelescopeSimulatorTest, resyncReplacesOlderSyncPointEquatorial)
{
    checkResyncReplacesOlderPoint("EQ_GEM");
}

// No ALTAZ variant here: a freshly constructed simulator points at the zenith, and the math plugin's
// SanitizePolarEntries() deliberately rewrites sync points above 88 deg altitude, so a zenith sync point
// cannot be compared directly. The ALTAZ path shares the replacement logic in ScopeSim::Sync().

// ---------------------------------------------------------------------------------------------------------
// Polar alignment corrector (PAC): the simulator snoops PAC_MANUAL_ADJUSTMENT and moves its polar axis.
// ---------------------------------------------------------------------------------------------------------

namespace
{
const char *PAC = "Alignment Correction Simulator";

std::string manualAdjustment(const char *state, double azStep, double altStep)
{
    return std::string("<setNumberVector device='") + PAC + "' name='PAC_MANUAL_ADJUSTMENT' state='" + state + "'>"
           "<oneNumber name='MANUAL_AZ_STEP'>" + std::to_string(azStep) + "</oneNumber>"
           "<oneNumber name='MANUAL_ALT_STEP'>" + std::to_string(altStep) + "</oneNumber>"
           "</setNumberVector>";
}

std::string correctionGain(double percent)
{
    return std::string("<setNumberVector device='") + PAC + "' name='CORRECTION_GAIN' state='Ok'>"
           "<oneNumber name='GAIN'>" + std::to_string(percent) + "</oneNumber></setNumberVector>";
}

constexpr double LATITUDE = 51.4683;
}

// The PAA reports the azimuth error in degrees of azimuth. Near the pole that is 1/cos(latitude) of the
// angle on the sky that MM_MA measures, so a full correction of the reported error removes MM_MA exactly.
TEST(TelescopeSimulatorTest, pacMoveCorrectsThePolarAxis)
{
    MockScopeSimDriver sim;
    sim.setLocation(LATITUDE, 0);
    sim.setActivePAC(PAC);
    sim.setPolarError(30, -20);

    // MM_MA = 30' (axis west) reads as an azimuth error of -30'/cos(lat), MM_ME = -20' (axis high) as an
    // altitude error of +20'. The client sends the correction, the negative of the error: move East and down.
    // The XML carries 6 decimals, about 4e-6 arcmin.
    const double azStep = 30.0 / std::cos(LATITUDE * M_PI / 180) / 60.0;
    sim.snoop(manualAdjustment("Busy", azStep, -20.0 / 60));
    EXPECT_NEAR(sim.mountModel("MM_MA"), 0, 1e-4);
    EXPECT_NEAR(sim.mountModel("MM_ME"), 0, 1e-4);
}

// The move takes effect when the PAC starts it, once. Clients take the next frame as soon as the PAC reports
// Ok, so applying the move on Ok left that frame at the old position.
TEST(TelescopeSimulatorTest, pacMoveIsAppliedOnceWhenItStarts)
{
    MockScopeSimDriver sim;
    sim.setLocation(LATITUDE, 0);
    sim.setActivePAC(PAC);
    sim.setPolarError(0, 0);

    // Moving the axis up by 30' leaves it 30' high (MM_ME = -30')
    sim.snoop(manualAdjustment("Busy", 0, 0.5));
    EXPECT_NEAR(sim.mountModel("MM_ME"), -30, 1e-4);

    // Further updates of the same move, and its completion, do not move the axis again
    sim.snoop(manualAdjustment("Busy", 0, 0.5));
    sim.snoop(manualAdjustment("Ok", 0, 0.5));
    EXPECT_NEAR(sim.mountModel("MM_ME"), -30, 1e-4);

    // A move the driver completes at once (Ok without Busy, e.g. 0/0) moves nothing
    sim.snoop(manualAdjustment("Ok", 0, 0.5));
    EXPECT_NEAR(sim.mountModel("MM_ME"), -30, 1e-4);

    // The next move is applied again
    sim.snoop(manualAdjustment("Busy", 0, -0.5));
    EXPECT_NEAR(sim.mountModel("MM_ME"), 0, 1e-4);
}

// The PAC simulator's gain simulates a corrector that moves less than requested.
TEST(TelescopeSimulatorTest, pacMoveFollowsTheCorrectionGain)
{
    MockScopeSimDriver sim;
    sim.setLocation(LATITUDE, 0);
    sim.setActivePAC(PAC);
    sim.setPolarError(0, 0);

    sim.snoop(correctionGain(50));
    sim.snoop(manualAdjustment("Busy", 0, 0.5));
    EXPECT_NEAR(sim.mountModel("MM_ME"), -15, 1e-4);
    sim.snoop(manualAdjustment("Ok", 0, 0.5));

    // Selecting another PAC resets the gain to a full move
    sim.setActivePAC("Other PAC");
    sim.setActivePAC(PAC);
    sim.snoop(manualAdjustment("Busy", 0, 0.5));
    EXPECT_NEAR(sim.mountModel("MM_ME"), -45, 1e-4);
}

// Every client that connects sends getProperties. A PAC move is not saved to the config, so reloading
// the mount model for each new client discarded the corrections made during a polar alignment.
TEST(TelescopeSimulatorTest, newClientKeepsThePACCorrection)
{
    MockScopeSimDriver sim;
    sim.setLocation(LATITUDE, 0);
    sim.setActivePAC(PAC);
    // Saved to the config
    sim.setPolarError(10, -10);

    sim.snoop(manualAdjustment("Busy", 0, 0.5));
    sim.snoop(manualAdjustment("Ok", 0, 0.5));
    EXPECT_NEAR(sim.mountModel("MM_ME"), -40, 1e-4);

    // Another client connects
    sim.ISGetProperties(sim.getDeviceName());
    EXPECT_NEAR(sim.mountModel("MM_MA"), 10, 1e-4);
    EXPECT_NEAR(sim.mountModel("MM_ME"), -40, 1e-4);
}

// INDI::Telescope: the dome policy is not saved when it changes. Another client connecting must not revert
// it to the saved value: "Dome ignored" would let the mount unpark while the dome is parked.
TEST(TelescopeSimulatorTest, newClientKeepsTheDomePolicy)
{
    MockScopeSimDriver sim;
    sim.setDomePolicy("DOME_IGNORED");
    sim.saveAllConfig();

    sim.setDomePolicy("DOME_LOCKS");
    ASSERT_EQ(sim.domePolicy(), "DOME_LOCKS");

    // Another client connects
    sim.ISGetProperties(sim.getDeviceName());
    EXPECT_EQ(sim.domePolicy(), "DOME_LOCKS");
}

int main(int argc, char **argv)
{
    // Never read or write the user's real ~/.indi driver configuration.
    // Use a file private to this process so that test runs can happen in parallel.
    const std::string configFile = "/tmp/test_telescope_simulator_" + std::to_string(getpid()) + "_config.xml";
    setenv("INDICONFIG", configFile.c_str(), 1);
    INDI::Logger::getInstance().configure("", INDI::Logger::file_off,
                                          INDI::Logger::DBG_ERROR, INDI::Logger::DBG_ERROR);
    ::testing::InitGoogleTest(&argc, argv);
    int rc = RUN_ALL_TESTS();
    std::remove(configFile.c_str());
    std::remove((configFile + ".default").c_str());
    return rc;
}
