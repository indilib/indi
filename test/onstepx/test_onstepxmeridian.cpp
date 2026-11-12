/*
    OnStepX INDI Driver — test_onstepxmeridian.cpp
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
    OnStepX INDI Driver — Meridian flip helper unit tests

    OnStepXMeridian runs against MockFirmware with replies scripted from the
    firmware handlers:
      :MN#      Goto.command.cpp, one unframed digit like :MS# ('0' = started)
      :SX99,1#  Goto.command.cpp, bare '1'
      :GSH#     Site.command.cpp, "HH:MM:SS.ss#"
      :GXEA#    Limits.command.cpp, minutes past the meridian, "60#"
      :GX96#    Goto.command.cpp, "E#" / "W#" / "B#" / "A#"
*/

#include <gtest/gtest.h>

#include "OnStepXComm.h"
#include "OnStepXMeridian.h"
#include "MockFirmware.h"
#include "SavedConfig.h"

#include <algorithm>
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// Pure helpers
// ---------------------------------------------------------------------------
TEST(OnStepXMeridianMath, hour_angle_wraps_to_plus_minus_12)
{
    EXPECT_NEAR(OnStepXMeridian::hourAngle(1.0, 23.0), 2.0, 1e-9);
    EXPECT_NEAR(OnStepXMeridian::hourAngle(23.0, 1.0), -2.0, 1e-9);
    EXPECT_NEAR(OnStepXMeridian::hourAngle(12.0, 0.0), -12.0, 1e-9);
    EXPECT_NEAR(OnStepXMeridian::hourAngle(5.1, 5.0), 0.1, 1e-9);
}

TEST(OnStepXMeridianMath, separation)
{
    EXPECT_NEAR(OnStepXMeridian::separationDeg(5.0, 20.0, 5.0, 20.0), 0.0, 1e-9);
    EXPECT_NEAR(OnStepXMeridian::separationDeg(1.0, 0.0, 2.0, 0.0), 15.0, 1e-9);
    EXPECT_NEAR(OnStepXMeridian::separationDeg(23.9, 0.0, 0.1, 0.0), 3.0, 1e-9);
    // 4 min of RA at Dec 60 is 0.5 deg on the sky
    EXPECT_NEAR(OnStepXMeridian::separationDeg(3.0, 60.0, 3.0 + 4.0 / 60.0, 60.0), 0.5, 1e-3);
}

TEST(OnStepXMeridianMath, notice_best_without_auto_flip)
{
    // 0.1 h past the meridian, west limit 60 min: (1.0 - 0.1) h of hour angle
    // is 54 min of clock time.
    const std::string n = OnStepXMeridian::crossingNotice(0.1, 60, false, 'B');
    EXPECT_NE(n.find("6 min past the meridian"), std::string::npos) << n;
    EXPECT_NE(n.find("(60 min) is about 54 min away"), std::string::npos) << n;
    EXPECT_NE(n.find("stop tracking"), std::string::npos) << n;
    EXPECT_NE(n.find("Best"), std::string::npos) << n;
    EXPECT_NE(n.find("will not change sides"), std::string::npos) << n;
}

TEST(OnStepXMeridianMath, notice_east_with_auto_flip)
{
    const std::string n = OnStepXMeridian::crossingNotice(0.0, 15, true, 'E');
    EXPECT_NE(n.find("flip the mount automatically"), std::string::npos) << n;
    EXPECT_NE(n.find("moves to the east side"), std::string::npos) << n;
    EXPECT_EQ(n.find("will not change sides"), std::string::npos) << n;
}

TEST(OnStepXMeridianMath, notice_auto_and_west)
{
    EXPECT_NE(OnStepXMeridian::crossingNotice(0.0, 60, false, 'A').find("moves to the east side"),
              std::string::npos);
    EXPECT_NE(OnStepXMeridian::crossingNotice(0.0, 60, false, 'W').find("stays west of the pier"),
              std::string::npos);
}

TEST(OnStepXMeridianMath, no_notice_past_the_limit)
{
    EXPECT_EQ(OnStepXMeridian::crossingNotice(1.2, 60, false, 'B'), "");
    EXPECT_EQ(OnStepXMeridian::crossingNotice(0.1, -30, false, 'B'), "");
}

// ---------------------------------------------------------------------------
// Fixture
// ---------------------------------------------------------------------------
class MeridianTest : public ::testing::Test
{
    protected:
        void SetUp() override
        {
            m_comm.setFd(m_fw.start());
            m_meridian.setComm(&m_comm);
            m_meridian.initProperties();
            m_meridian.updateProperties(true, true);

            m_fw.setResponse(":GXEA", "60#");
            m_fw.setResponse(":GX96", "B#");

            m_west.mountType = MountStatus::MountType::GEM;
            m_west.pierSide  = MountStatus::PierSide::WEST;
            m_west.tracking  = true;
        }

        void TearDown() override
        {
            m_fw.stop();
        }

        int count(const std::string &cmd)
        {
            auto c = m_fw.commands();
            return static_cast<int>(std::count(c.begin(), c.end(), cmd));
        }

        // Poll once per check interval, so every call reads the hour angle.
        void poll(const MountStatus &s, double ra)
        {
            for (int i = 0; i < OnStepXMeridian::CHECK_EVERY; i++)
                m_meridian.update(s, ra);
        }

        MockFirmware    m_fw;
        OnStepXComm     m_comm;
        OnStepXMeridian m_meridian;
        MountStatus     m_west;
};

// ---------------------------------------------------------------------------
// Meridian notice
// ---------------------------------------------------------------------------
TEST_F(MeridianTest, notice_once_after_the_meridian)
{
    m_fw.setResponse(":GSH", "05:06:00.00#");    // RA 5h: 6 min past the meridian
    poll(m_west, 5.0);

    EXPECT_NE(m_meridian.lastNotice().find("about 54 min away"), std::string::npos)
            << m_meridian.lastNotice();
    EXPECT_NE(m_meridian.lastNotice().find("Best"), std::string::npos);
    EXPECT_EQ(m_fw.commands(), (std::vector<std::string> { ":GSH", ":GXEA", ":GX96" }));

    // Still past the meridian: the hour angle is read, but no second notice.
    poll(m_west, 5.0);
    poll(m_west, 5.0);
    EXPECT_EQ(count(":GSH"), 3);
    EXPECT_EQ(count(":GXEA"), 1);
}

TEST_F(MeridianTest, nothing_before_the_meridian)
{
    m_fw.setResponse(":GSH", "04:30:00.00#");    // RA 5h: 30 min before
    poll(m_west, 5.0);
    EXPECT_EQ(m_fw.commands(), std::vector<std::string> { ":GSH" });
    EXPECT_TRUE(m_meridian.lastNotice().empty());
}

TEST_F(MeridianTest, rearms_after_a_pier_change)
{
    m_fw.setResponse(":GSH", "05:06:00.00#");
    poll(m_west, 5.0);
    ASSERT_EQ(count(":GXEA"), 1);

    MountStatus east = m_west;
    east.pierSide = MountStatus::PierSide::EAST;
    poll(east, 5.0);
    EXPECT_EQ(count(":GSH"), 1);    // nothing read east of the pier

    poll(m_west, 5.0);
    EXPECT_EQ(count(":GXEA"), 2);
}

TEST_F(MeridianTest, rearms_after_a_goto_east_of_the_meridian)
{
    m_fw.setResponse(":GSH", "05:06:00.00#");
    poll(m_west, 5.0);
    poll(m_west, 6.0);              // new target 54 min east of the meridian
    poll(m_west, 5.0);
    EXPECT_EQ(count(":GXEA"), 2);
}

// A goto that ends past the meridian on the same side (an Ekos flip retry
// with pier preference Best) gets its own notice.
TEST_F(MeridianTest, rearms_after_any_goto)
{
    m_fw.setResponse(":GSH", "05:06:00.00#");
    poll(m_west, 5.0);
    MountStatus slewing = m_west;
    slewing.gotoActive = true;
    m_meridian.update(slewing, 5.0);
    poll(m_west, 5.0);
    EXPECT_EQ(count(":GXEA"), 2);
}

TEST_F(MeridianTest, quiet_while_slewing_or_not_tracking)
{
    m_fw.setResponse(":GSH", "05:06:00.00#");
    MountStatus s = m_west;
    s.gotoActive = true;
    poll(s, 5.0);
    s.gotoActive = false;
    s.tracking   = false;
    poll(s, 5.0);
    EXPECT_TRUE(m_fw.commands().empty());
}

TEST_F(MeridianTest, gem_only)
{
    m_fw.setResponse(":GSH", "05:06:00.00#");
    MountStatus s = m_west;
    s.mountType = MountStatus::MountType::FORK;
    poll(s, 5.0);
    EXPECT_TRUE(m_fw.commands().empty());
}

TEST_F(MeridianTest, limit_read_failure_keeps_the_notice_armed)
{
    m_fw.setResponse(":GSH", "05:06:00.00#");
    m_fw.setResponse(":GXEA", "0");              // bare '0': failed
    poll(m_west, 5.0);
    EXPECT_TRUE(m_meridian.lastNotice().empty());

    m_fw.setResponse(":GXEA", "60#");
    poll(m_west, 5.0);
    EXPECT_FALSE(m_meridian.lastNotice().empty());
}

TEST_F(MeridianTest, nothing_on_a_mount_that_does_not_flip)
{
    m_meridian.updateProperties(false, false);
    m_meridian.updateProperties(true, false);
    m_fw.setResponse(":GSH", "05:06:00.00#");
    poll(m_west, 5.0);
    EXPECT_TRUE(m_fw.commands().empty());
}

// ---------------------------------------------------------------------------
// Flip
// ---------------------------------------------------------------------------
TEST_F(MeridianTest, flip_sends_mn)
{
    m_fw.setResponse(":MN", "0");
    EXPECT_TRUE(m_meridian.flip(5.0, 20.0, 5.001, 20.01));
    EXPECT_EQ(m_fw.commands(), std::vector<std::string> { ":MN" });
}

TEST_F(MeridianTest, flip_rejected_by_firmware)
{
    m_fw.setResponse(":MN", "6");                // outside limits
    EXPECT_FALSE(m_meridian.flip(5.0, 20.0, 5.0, 20.0));
}

TEST_F(MeridianTest, flip_refused_far_from_the_current_position)
{
    EXPECT_FALSE(m_meridian.flip(5.0, 20.0, 6.0, 20.0));
    EXPECT_TRUE(m_fw.commands().empty());
}

TEST_F(MeridianTest, flip_refused_on_a_mount_that_does_not_flip)
{
    // :MN# replies a bare '0' there, which would read as "started".
    m_meridian.updateProperties(false, false);
    m_meridian.updateProperties(true, false);
    EXPECT_FALSE(m_meridian.flip(5.0, 20.0, 5.0, 20.0));
    EXPECT_TRUE(m_fw.commands().empty());
}

// ---------------------------------------------------------------------------
// Paused at home
// ---------------------------------------------------------------------------
TEST_F(MeridianTest, paused_at_home_shows_busy_and_continues)
{
    MountStatus s;
    s.mountType     = MountStatus::MountType::GEM;
    s.gotoActive    = true;
    s.waitingAtHome = true;
    m_meridian.update(s, 5.0);
    EXPECT_EQ(m_meridian.continueSP().getState(), IPS_BUSY);

    m_fw.setResponse(":SX99,1", "1");
    ISState states[1] = { ISS_ON };
    const char *names[1] = { "HOME_CONTINUE" };
    EXPECT_TRUE(m_meridian.handleSwitch("OSX_HOME_CONTINUE", states, const_cast<char **>(names), 1));
    EXPECT_EQ(m_fw.commands(), std::vector<std::string> { ":SX99,1" });
    EXPECT_EQ(m_meridian.continueSP()[0].getState(), ISS_OFF);

    s.waitingAtHome = false;
    m_meridian.update(s, 5.0);
    EXPECT_EQ(m_meridian.continueSP().getState(), IPS_IDLE);
}

TEST_F(MeridianTest, continue_without_a_pause_sends_nothing)
{
    ISState states[1] = { ISS_ON };
    const char *names[1] = { "HOME_CONTINUE" };
    EXPECT_TRUE(m_meridian.handleSwitch("OSX_HOME_CONTINUE", states, const_cast<char **>(names), 1));
    EXPECT_TRUE(m_fw.commands().empty());
    EXPECT_FALSE(m_meridian.handleSwitch("OSX_OTHER", states, const_cast<char **>(names), 1));
}

// ---------------------------------------------------------------------------
// Pause at home (:SX98,n#, bare '1'; the state is the status flag)
// ---------------------------------------------------------------------------
TEST_F(MeridianTest, pause_at_home_on_and_off)
{
    m_fw.setResponse(":SX98,1", "1");
    m_fw.setResponse(":SX98,0", "1");
    ISState states[2] = { ISS_OFF, ISS_ON };
    const char *names[2] = { "HOME_PAUSE_OFF", "HOME_PAUSE_ON" };
    EXPECT_TRUE(m_meridian.handleSwitch("OSX_HOME_PAUSE", states, const_cast<char **>(names), 2));
    EXPECT_TRUE(m_fw.received(":SX98,1"));
    EXPECT_EQ(m_meridian.pauseSP().getState(), IPS_OK);

    states[0] = ISS_ON;
    states[1] = ISS_OFF;
    EXPECT_TRUE(m_meridian.handleSwitch("OSX_HOME_PAUSE", states, const_cast<char **>(names), 2));
    EXPECT_TRUE(m_fw.received(":SX98,0"));
}

TEST_F(MeridianTest, pause_at_home_refused_raises_alert)
{
    ISState states[2] = { ISS_OFF, ISS_ON };
    const char *names[2] = { "HOME_PAUSE_OFF", "HOME_PAUSE_ON" };
    EXPECT_TRUE(m_meridian.handleSwitch("OSX_HOME_PAUSE", states, const_cast<char **>(names), 2));
    EXPECT_EQ(m_meridian.pauseSP().getState(), IPS_ALERT);
}

TEST_F(MeridianTest, pause_at_home_follows_the_firmware)
{
    // Set from the hand controller: the status poll shows it.
    MountStatus s;
    s.mountType          = MountStatus::MountType::GEM;
    s.pauseAtHomeEnabled = true;
    m_meridian.update(s, 5.0);
    EXPECT_EQ(m_meridian.pauseSP()[1].getState(), ISS_ON);
    s.pauseAtHomeEnabled = false;
    m_meridian.update(s, 5.0);
    EXPECT_EQ(m_meridian.pauseSP()[0].getState(), ISS_ON);
}

int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

// Pause at home is GOTO_SETTINGS.meridianFlipPause in the firmware's NV.
TEST_F(MeridianTest, pause_at_home_not_saved)
{
    EXPECT_EQ(savedConfig([&](FILE * fp)
    {
        m_meridian.saveConfig(fp);
    }), "");
}
