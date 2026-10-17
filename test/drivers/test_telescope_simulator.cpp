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
