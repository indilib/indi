/*
    OnStepX INDI Driver — test_onstepxprobe.cpp
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
    OnStepX INDI Driver — Capability probing unit tests

    Uses MockFirmware.h, which answers like the real firmware (unknown
    commands get a bare '0') and records every command so tests can check
    that probing is read-only.
*/

#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "OnStepXCore.h"
#include "MockFirmware.h"

static int countOf(const std::vector<std::string> &cmds, const std::string &name)
{
    int n = 0;
    for (const auto &c : cmds)
        n += c == ":" + name;
    return n;
}

// Phase-1 replies for a controller with a rotator, focuser 1, aux slot 1,
// weather and an MCU temperature sensor.
static void addPhase1Responses(MockFirmware &fw)
{
    fw.setResponse(":GVP", "On-Step#");
    fw.setResponse(":GVN", "10.28q#");
    fw.setResponse(":GVD", "Nov 07 2026#");
    fw.setResponse(":GVT", "12:00:00#");
    fw.setResponse(":GVC", "Config#");
    fw.setResponse(":GX98", "R#");
    fw.setResponse(":F1a", "1");
    fw.setResponse(":GXY0", "10000000#");
    fw.setResponse(":GX9A", "15.0#");
    fw.setResponse(":Gv", "2000#");
    fw.setResponse(":GX9F", "25#");
}

// Phase-2 replies for a parked GEM with PEC compiled in.
static void addPhase2Responses(MockFirmware &fw, const char *gw = "GNP#")
{
    fw.setResponse(":GW", gw);
    std::string gu(9, static_cast<char>(0x80));
    gu += '#';
    fw.setResponse(":Gu", gu);
    fw.setResponse(":GU", "nNPE/To000#");
    fw.setResponse(":h?", "1,0,0#");
    fw.setResponse(":Gm", "E#");
    fw.setResponse(":GX94", "1#");      // east of the pier; flips (no " N")
}

static bool anySetCommand(const std::vector<std::string> &cmds)
{
    for (const auto &c : cmds)
        if (c.size() > 1 && c[1] == 'S')
            return true;
    return false;
}

TEST(OnStepXProbeTest, full_mount)
{
    MockFirmware fw;
    addPhase1Responses(fw);
    addPhase2Responses(fw);
    OnStepXCore core;
    core.setFd(fw.start());

    EXPECT_TRUE(core.probeController());
    EXPECT_TRUE(core.probeMount());

    const Capabilities &cap = core.caps();
    EXPECT_TRUE(cap.isOnStepX);
    EXPECT_STREQ(cap.firmwareVersion, "10.28q");
    EXPECT_TRUE(cap.hasRotator);
    EXPECT_FALSE(cap.hasDerotator);
    EXPECT_EQ(cap.focuserMask, 0x01);
    EXPECT_EQ(cap.featureMask, 0x01);
    EXPECT_TRUE(cap.hasWeatherRead);
    EXPECT_TRUE(cap.hasElevation);
    EXPECT_TRUE(cap.hasMcuTemp);

    EXPECT_TRUE(cap.hasMount);
    EXPECT_EQ(cap.mountType, MountType::GEM);
    EXPECT_TRUE(cap.hasBinaryStatus);
    EXPECT_TRUE(cap.hasPec);
    EXPECT_TRUE(cap.hasHomeSense);
    EXPECT_TRUE(cap.hasPierSide);
    EXPECT_TRUE(cap.hasMeridianFlips);

    fw.stop();
}

// Regression: probing used to write :SX9A,15.0# and :SU0.0#, overwriting the
// user's refraction temperature and DUT1 on every connect.
TEST(OnStepXProbeTest, probing_is_read_only)
{
    MockFirmware fw;
    addPhase1Responses(fw);
    addPhase2Responses(fw);
    OnStepXCore core;
    core.setFd(fw.start());

    EXPECT_TRUE(core.probeController());
    EXPECT_TRUE(core.probeMount());
    fw.stop();

    EXPECT_FALSE(anySetCommand(fw.commands()));
    for (const auto &c : fw.commands())
        EXPECT_NE(c.rfind(":FA", 0), 0u) << "probe must not change the active focuser: " << c;
}

TEST(OnStepXProbeTest, not_onstepx)
{
    MockFirmware fw;
    fw.setResponse(":GVP", "LX200#");
    OnStepXCore core;
    core.setFd(fw.start());

    EXPECT_FALSE(core.probeController());
    EXPECT_FALSE(core.caps().isOnStepX);
    fw.stop();
    EXPECT_EQ(countOf(fw.commands(), "GVP"), OnStepXCore::GVP_ATTEMPTS);
    EXPECT_FALSE(fw.received(":GVN")) << "must stop at an unrecognised product";
}

// A controller that is still booting, or a reply that lost a byte on the
// USB link, must not fail the handshake.
TEST(OnStepXProbeTest, gvp_retried_after_bad_replies)
{
    MockFirmware fw;
    addPhase1Responses(fw);
    fw.setResponseSequence(":GVP", {"", "n-Step#", "On-Step#"});
    OnStepXCore core;
    core.setFd(fw.start());

    EXPECT_TRUE(core.probeController());
    EXPECT_TRUE(core.caps().isOnStepX);
    fw.stop();
    EXPECT_EQ(countOf(fw.commands(), "GVP"), 3);
}

TEST(OnStepXProbeTest, gvp_no_response_gives_up)
{
    MockFirmware fw;
    fw.setResponse(":GVP", "");
    OnStepXCore core;
    core.setFd(fw.start());

    EXPECT_FALSE(core.probeController());
    fw.stop();
    EXPECT_EQ(countOf(fw.commands(), "GVP"), OnStepXCore::GVP_ATTEMPTS);
}

TEST(OnStepXProbeTest, classic_onstep)
{
    MockFirmware fw;
    fw.setResponse(":GVP", "On-Step#");
    fw.setResponse(":GVN", "4.12f#");
    OnStepXCore core;
    core.setFd(fw.start());

    EXPECT_FALSE(core.probeController());
    EXPECT_FALSE(core.caps().isOnStepX);
    fw.stop();
}

// Regression: without a mount :GW# is an unknown command and the firmware
// replies a bare '0', which used to be accepted as "mount present".
TEST(OnStepXProbeTest, no_mount)
{
    MockFirmware fw;
    addPhase1Responses(fw);   // no :GW response -> bare '0'
    OnStepXCore core;
    core.setFd(fw.start());

    EXPECT_TRUE(core.probeController());
    EXPECT_FALSE(core.probeMount());
    EXPECT_FALSE(core.caps().hasMount);
    fw.stop();
}

// Regression (hardware, 2026-11-12): the USB link lost the 'G' of "GT1", and
// the reconnect failed with "Unrecognised mount type in :GW# reply 'T1'".
TEST(OnStepXProbeTest, gw_retried_after_lost_byte)
{
    MockFirmware fw;
    addPhase1Responses(fw);
    addPhase2Responses(fw);
    fw.setResponseSequence(":GW", {"T1#", "GT1#"});
    OnStepXCore core;
    core.setFd(fw.start());

    ASSERT_TRUE(core.probeController());
    EXPECT_TRUE(core.probeMount());
    EXPECT_TRUE(core.caps().hasMount);
    EXPECT_EQ(core.caps().mountType, MountType::GEM);
    fw.stop();
    EXPECT_EQ(countOf(fw.commands(), "GW"), 2);
}

TEST(OnStepXProbeTest, gw_malformed_every_time_is_no_mount)
{
    MockFirmware fw;
    addPhase1Responses(fw);
    addPhase2Responses(fw, "XN0#");
    OnStepXCore core;
    core.setFd(fw.start());

    ASSERT_TRUE(core.probeController());
    EXPECT_FALSE(core.probeMount());
    EXPECT_FALSE(core.caps().hasMount);
    fw.stop();
    EXPECT_EQ(countOf(fw.commands(), "GW"), OnStepXCore::GVP_ATTEMPTS);
}

TEST(OnStepXProbeTest, gw_reply_format)
{
    const char *valid[] = { "GNP", "GT1", "PNH", "AT0", "LN1" };
    const char *invalid[] = { "", "T1", "NP", "GT", "GT1x", "XN0", "GX0", "GNZ" };
    for (const char *ok : valid)
        EXPECT_TRUE(OnStepXCore::isValidGwReply(ok)) << ok;
    for (const char *bad : invalid)
        EXPECT_FALSE(OnStepXCore::isValidGwReply(bad)) << bad;
}

TEST(OnStepXProbeTest, no_rotator)
{
    MockFirmware fw;
    addPhase1Responses(fw);
    fw.setResponse(":GX98", "N#");
    OnStepXCore core;
    core.setFd(fw.start());

    EXPECT_TRUE(core.probeController());
    EXPECT_FALSE(core.caps().hasRotator);
    EXPECT_FALSE(core.caps().hasDerotator);
    fw.stop();
}

TEST(OnStepXProbeTest, derotator)
{
    MockFirmware fw;
    addPhase1Responses(fw);
    fw.setResponse(":GX98", "D#");
    OnStepXCore core;
    core.setFd(fw.start());

    EXPECT_TRUE(core.probeController());
    EXPECT_TRUE(core.caps().hasRotator);
    EXPECT_TRUE(core.caps().hasDerotator);
    fw.stop();
}

// Regression: probing stopped at the first absent slot, so a controller with
// only focuser 2 (Axis5) reported no focusers.
TEST(OnStepXProbeTest, sparse_focusers)
{
    MockFirmware fw;
    addPhase1Responses(fw);
    fw.setResponse(":F1a", "0");
    fw.setResponse(":F2a", "1");
    fw.setResponse(":F5a", "1");
    OnStepXCore core;
    core.setFd(fw.start());

    EXPECT_TRUE(core.probeController());
    EXPECT_EQ(core.caps().focuserMask, 0b00010010);
    EXPECT_FALSE(core.caps().hasFocuser(1));
    EXPECT_TRUE(core.caps().hasFocuser(2));
    EXPECT_TRUE(core.caps().hasFocuser(5));
    fw.stop();
}

// Regression: a bare '0' (no MCU sensor) used to be accepted as a reading.
TEST(OnStepXProbeTest, no_mcu_temperature)
{
    MockFirmware fw;
    addPhase1Responses(fw);
    fw.setResponse(":GX9F", "0");
    OnStepXCore core;
    core.setFd(fw.start());

    EXPECT_TRUE(core.probeController());
    EXPECT_FALSE(core.caps().hasMcuTemp);
    fw.stop();
}

// Seen on hardware (OnStepX 10.29c, no weather sensor): :GX9A# -> "nan#".
TEST(OnStepXProbeTest, weather_nan_means_unavailable)
{
    MockFirmware fw;
    addPhase1Responses(fw);
    fw.setResponse(":GX9A", "nan#");
    OnStepXCore core;
    core.setFd(fw.start());

    EXPECT_TRUE(core.probeController());
    EXPECT_FALSE(core.caps().hasWeatherRead);
    fw.stop();
}

TEST(OnStepXProbeTest, mcu_temperature_of_zero_is_a_reading)
{
    MockFirmware fw;
    addPhase1Responses(fw);
    fw.setResponse(":GX9F", "0#");
    OnStepXCore core;
    core.setFd(fw.start());

    EXPECT_TRUE(core.probeController());
    EXPECT_TRUE(core.caps().hasMcuTemp);
    fw.stop();
}

// Regression: :h?# replies "0,..." / "1,..."; it used to be read as always present.
TEST(OnStepXProbeTest, no_home_sensor)
{
    MockFirmware fw;
    addPhase1Responses(fw);
    addPhase2Responses(fw);
    fw.setResponse(":h?", "0,0,0#");
    OnStepXCore core;
    core.setFd(fw.start());

    EXPECT_TRUE(core.probeController());
    EXPECT_TRUE(core.probeMount());
    EXPECT_FALSE(core.caps().hasHomeSense);
    fw.stop();
}

// PEC presence comes from the PEC state char in :GU#, present only when
// AXIS1_PEC is compiled in (and the mount is equatorial).
TEST(OnStepXProbeTest, no_pec)
{
    MockFirmware fw;
    addPhase1Responses(fw);
    addPhase2Responses(fw);
    fw.setResponse(":GU", "nNPETo000#");
    OnStepXCore core;
    core.setFd(fw.start());

    EXPECT_TRUE(core.probeController());
    EXPECT_TRUE(core.probeMount());
    EXPECT_FALSE(core.caps().hasPec);
    fw.stop();
}

TEST(OnStepXProbeTest, altaz)
{
    MockFirmware fw;
    addPhase1Responses(fw);
    addPhase2Responses(fw, "AT0#");
    OnStepXCore core;
    core.setFd(fw.start());

    EXPECT_TRUE(core.probeController());
    EXPECT_TRUE(core.probeMount());
    EXPECT_EQ(core.caps().mountType, MountType::ALTAZM);
    fw.stop();
}

// Axis.command.cpp :GXU[n]#: a flag list when the axis's driver reports
// status, a bare '0' otherwise (and for axes that do not exist).
TEST(OnStepXProbeTest, driver_status_axes)
{
    MockFirmware fw;
    addPhase1Responses(fw);
    fw.setResponse(":GXU1", "ST,,,,,,,#");
    fw.setResponse(":GXU4", ",,,,,,,#");
    OnStepXCore core;
    core.setFd(fw.start());

    EXPECT_TRUE(core.probeController());
    EXPECT_EQ(core.caps().driverStatusMask, 0x009);
    fw.stop();

    // Rotator and focuser 1 only: axes 5-9 cannot exist, so are not asked.
    const char *asked[]    = { ":GXU1", ":GXU2", ":GXU3", ":GXU4" };
    const char *notAsked[] = { ":GXU5", ":GXU6", ":GXU9" };
    for (const char *cmd : asked)
        EXPECT_TRUE(fw.received(cmd)) << cmd;
    for (const char *cmd : notAsked)
        EXPECT_FALSE(fw.received(cmd)) << cmd;
}

// Goto.command.cpp :GX94#: the pier side with " N" appended when the mount
// does not do meridian flips (fork/AltAz without alternate orientation).
TEST(OnStepXProbeTest, no_meridian_flips)
{
    MockFirmware fw;
    addPhase1Responses(fw);
    addPhase2Responses(fw, "AT0#");
    fw.setResponse(":GX94", "1 N#");
    OnStepXCore core;
    core.setFd(fw.start());

    EXPECT_TRUE(core.probeController());
    EXPECT_TRUE(core.probeMount());
    EXPECT_FALSE(core.caps().hasMeridianFlips);
    fw.stop();
}

TEST(OnStepXProbeTest, meridian_flips_unknown_without_reply)
{
    MockFirmware fw;
    addPhase1Responses(fw);
    addPhase2Responses(fw);
    fw.setResponse(":GX94", "0");       // bare '0': not answered
    OnStepXCore core;
    core.setFd(fw.start());

    EXPECT_TRUE(core.probeController());
    EXPECT_TRUE(core.probeMount());
    EXPECT_FALSE(core.caps().hasMeridianFlips);
    fw.stop();
}

// Binary status needs all 9 bytes >= 0x80 followed by '#'.
TEST(OnStepXProbeTest, binary_status_requires_terminator)
{
    MockFirmware fw;
    addPhase1Responses(fw);
    addPhase2Responses(fw);
    fw.setResponse(":Gu", std::string(10, static_cast<char>(0x80)));   // no '#'
    OnStepXCore core;
    core.setFd(fw.start());

    EXPECT_TRUE(core.probeController());
    EXPECT_TRUE(core.probeMount());
    EXPECT_FALSE(core.caps().hasBinaryStatus);
    fw.stop();
}

// Capabilities are reset on every probe so nothing survives a reconnect.
TEST(OnStepXProbeTest, reprobe_resets_capabilities)
{
    OnStepXCore core;
    {
        MockFirmware fw;
        addPhase1Responses(fw);
        fw.setResponse(":F3a", "1");
        core.setFd(fw.start());
        EXPECT_TRUE(core.probeController());
        EXPECT_TRUE(core.caps().hasFocuser(3));
        fw.stop();
    }
    {
        MockFirmware fw;
        addPhase1Responses(fw);
        fw.setResponse(":GX98", "N#");
        core.setFd(fw.start());
        EXPECT_TRUE(core.probeController());
        EXPECT_FALSE(core.caps().hasFocuser(3));
        EXPECT_FALSE(core.caps().hasRotator);
        fw.stop();
    }
}

TEST(OnStepXProbeTest, aux_binary_phase1_only)
{
    MockFirmware fw;
    addPhase1Responses(fw);
    fw.setResponse(":F2a", "1");
    fw.setResponse(":GXY0", "11000000#");
    OnStepXCore core;
    core.setFd(fw.start());

    EXPECT_TRUE(core.probeController());
    EXPECT_EQ(core.caps().focuserMask, 0b00000011);
    EXPECT_EQ(core.caps().featureMask, 0b00000011);
    EXPECT_FALSE(core.caps().hasMount);
    fw.stop();
}

int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
