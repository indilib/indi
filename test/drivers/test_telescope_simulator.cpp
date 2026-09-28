/*
    Regression test for the telescope simulator: alignment sync points recorded for one mount
    geometry must not be reused after switching between ALTAZ and an equatorial mount type.

    SPDX-License-Identifier: LGPL-2.1-or-later
*/

#include <gtest/gtest.h>

#include <cstdlib>

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

int main(int argc, char **argv)
{
    // Never read or write the user's real ~/.indi driver configuration.
    setenv("INDICONFIG", "/tmp/test_telescope_simulator_config.xml", 1);
    INDI::Logger::getInstance().configure("", INDI::Logger::file_off,
                                          INDI::Logger::DBG_ERROR, INDI::Logger::DBG_ERROR);
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
