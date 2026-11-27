/*
    OnStepX INDI Driver — test_onstepxdriverstatus.cpp
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
    OnStepX INDI Driver — Motor driver status helper unit tests

    Replies are transcribed from the firmware's :GXU[n]# handler
    (lib/axis/Axis.command.cpp): eight fields, each its code or empty, a comma
    after each but the last:

        strcat(reply, status.standstill ? "ST," : ",");
        ... "OA,"  "OB,"  "GA,"  "GB,"  "OT,"  "PW," ...
        strcat(reply, status.fault ? "GF" : "");

    and a bare '0' when the axis's driver does not report status.
*/

#include <gtest/gtest.h>

#include "OnStepXComm.h"
#include "OnStepXDriverStatus.h"
#include "MockFirmware.h"

#include <string>
#include <vector>

// The firmware's reply for a set of flags, built as the handler builds it.
static std::string firmwareReply(bool st, bool oa, bool ob, bool ga, bool gb, bool ot, bool pw, bool gf)
{
    std::string r;
    r += st ? "ST," : ",";
    r += oa ? "OA," : ",";
    r += ob ? "OB," : ",";
    r += ga ? "GA," : ",";
    r += gb ? "GB," : ",";
    r += ot ? "OT," : ",";
    r += pw ? "PW," : ",";
    r += gf ? "GF" : "";
    return r;
}

// ---------------------------------------------------------------------------
// parse / describe
// ---------------------------------------------------------------------------
TEST(OnStepXDriverStatusParse, all_clear)
{
    OnStepXDriverStatus::Flags f;
    ASSERT_TRUE(OnStepXDriverStatus::parse(firmwareReply(false, false, false, false, false, false, false, false).c_str(), f));
    EXPECT_FALSE(f.standstill);
    EXPECT_FALSE(f.serious());
    EXPECT_EQ(OnStepXDriverStatus::describe(f), "OK");
}

TEST(OnStepXDriverStatusParse, every_flag)
{
    OnStepXDriverStatus::Flags f;
    ASSERT_TRUE(OnStepXDriverStatus::parse(firmwareReply(true, true, true, true, true, true, true, true).c_str(), f));
    EXPECT_TRUE(f.standstill && f.openA && f.openB && f.shortA && f.shortB && f.overTemp && f.preWarning && f.fault);
    EXPECT_TRUE(f.serious());
}

TEST(OnStepXDriverStatusParse, each_flag_in_its_own_field)
{
    for (int i = 0; i < 8; i++)
    {
        bool b[8] = {};
        b[i] = true;
        OnStepXDriverStatus::Flags f;
        ASSERT_TRUE(OnStepXDriverStatus::parse(firmwareReply(b[0], b[1], b[2], b[3], b[4], b[5], b[6], b[7]).c_str(), f));
        const bool got[8] = { f.standstill, f.openA, f.openB, f.shortA, f.shortB, f.overTemp, f.preWarning, f.fault };
        for (int j = 0; j < 8; j++)
            EXPECT_EQ(got[j], i == j) << "flag " << i << " field " << j;
    }
}

TEST(OnStepXDriverStatusParse, standstill_and_open_load_are_not_problems)
{
    // An idle motor reports standstill; an unplugged one open load.
    OnStepXDriverStatus::Flags f;
    ASSERT_TRUE(OnStepXDriverStatus::parse(firmwareReply(true, true, false, false, false, false, false, false).c_str(), f));
    EXPECT_FALSE(f.serious());
    EXPECT_EQ(OnStepXDriverStatus::problems(f), "");
    EXPECT_EQ(OnStepXDriverStatus::describe(f), "OK, open load A, standstill");
}

TEST(OnStepXDriverStatusParse, problems_text)
{
    OnStepXDriverStatus::Flags f;
    ASSERT_TRUE(OnStepXDriverStatus::parse(firmwareReply(false, false, false, true, false, true, false, false).c_str(), f));
    EXPECT_EQ(OnStepXDriverStatus::problems(f), "short to ground on output A, over temperature");
    EXPECT_EQ(OnStepXDriverStatus::describe(f), "short to ground on output A, over temperature");

    ASSERT_TRUE(OnStepXDriverStatus::parse(firmwareReply(false, false, false, false, false, false, true, false).c_str(), f));
    EXPECT_FALSE(f.serious());
    EXPECT_EQ(OnStepXDriverStatus::problems(f), "temperature pre-warning");
}

TEST(OnStepXDriverStatusParse, rejects_other_replies)
{
    OnStepXDriverStatus::Flags f;
    EXPECT_FALSE(OnStepXDriverStatus::parse("0", f));
    EXPECT_FALSE(OnStepXDriverStatus::parse("", f));
    EXPECT_FALSE(OnStepXDriverStatus::parse("ST,XX,,,,,,", f));
    EXPECT_FALSE(OnStepXDriverStatus::parse(nullptr, f));
}

TEST(OnStepXDriverStatusParse, axis_names)
{
    EXPECT_STREQ(OnStepXDriverStatus::axisName(1), "Axis 1 (RA/Azm)");
    EXPECT_STREQ(OnStepXDriverStatus::axisName(3), "Axis 3 (Rotator)");
    EXPECT_STREQ(OnStepXDriverStatus::axisName(9), "Axis 9 (Focuser 6)");
}

// ---------------------------------------------------------------------------
// Polling against MockFirmware
// ---------------------------------------------------------------------------
class DriverStatusTest : public ::testing::Test
{
    protected:
        void SetUp() override
        {
            m_comm.setFd(m_fw.start());
            m_status.setComm(&m_comm);
        }
        void TearDown() override
        {
            m_fw.stop();
        }

        void reply(const char *key, bool ga, bool st = false)
        {
            m_fw.setResponse(key, firmwareReply(st, false, false, ga, false, false, false, false) + "#");
        }

        MockFirmware        m_fw;
        OnStepXComm         m_comm;
        OnStepXDriverStatus m_status;
};

TEST_F(DriverStatusTest, polls_only_the_reporting_axes)
{
    reply(":GXU1", false, true);
    reply(":GXU4", false);
    m_status.discoverAndDefine(0x009);           // axes 1 and 4

    EXPECT_EQ(m_fw.commands(), (std::vector<std::string> { ":GXU1", ":GXU4" }));
    ASSERT_EQ(m_status.statusTP().count(), 2u);
    EXPECT_STREQ(m_status.statusTP()[0].getName(), "AXIS1");
    EXPECT_STREQ(m_status.statusTP()[1].getName(), "AXIS4");
    EXPECT_STREQ(m_status.statusTP()[0].getText(), "OK, standstill");
    EXPECT_STREQ(m_status.statusTP()[1].getText(), "OK");
    EXPECT_EQ(m_status.statusTP().getState(), IPS_OK);
}

TEST_F(DriverStatusTest, fault_raises_alert_and_clears)
{
    reply(":GXU2", true);
    m_status.discoverAndDefine(0x002);
    EXPECT_STREQ(m_status.statusTP()[0].getText(), "short to ground on output A");
    EXPECT_EQ(m_status.statusTP().getState(), IPS_ALERT);

    reply(":GXU2", false);
    m_status.poll();
    EXPECT_STREQ(m_status.statusTP()[0].getText(), "OK");
    EXPECT_EQ(m_status.statusTP().getState(), IPS_OK);
}

TEST_F(DriverStatusTest, missed_read_keeps_the_last_state)
{
    reply(":GXU1", true);
    m_status.discoverAndDefine(0x001);
    m_fw.setResponse(":GXU1", "0");              // e.g. a stall or a bare reply
    m_status.poll();
    EXPECT_STREQ(m_status.statusTP()[0].getText(), "short to ground on output A");
    EXPECT_EQ(m_status.statusTP().getState(), IPS_ALERT);
}

TEST_F(DriverStatusTest, nothing_without_reporting_axes)
{
    m_status.discoverAndDefine(0);
    m_status.poll();
    EXPECT_TRUE(m_fw.commands().empty());
}

TEST_F(DriverStatusTest, reconnect_rebuilds_the_axis_list)
{
    reply(":GXU1", false);
    reply(":GXU3", false);
    m_status.discoverAndDefine(0x005);
    m_status.deleteAll();
    m_status.discoverAndDefine(0x004);
    ASSERT_EQ(m_status.statusTP().count(), 1u);
    EXPECT_STREQ(m_status.statusTP()[0].getName(), "AXIS3");
}

int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
