/*
    OnStepX INDI Driver — test_onstepxalignment.cpp
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
    OnStepX INDI Driver — unit tests: Alignment helper

    Replies are scripted from the firmware handlers (Goto.command.cpp).

    Tests cover:
      - Start sends :A[n]# for the selected star count, then refreshes status
      - Accept Star sends :A+#, Write sends :AW#, and refusals raise an alert
      - updateStatus: :A?# "mno" reply parsing, including ':' = 10
      - updatePolarError: :GX02#/:GX03#
      - Refine Polar Align: :MP# (one unframed digit like :MS#), refused on
        non-equatorial mounts, during a model build, and with no polar error
*/

#include <defaultdevice.h>

#include <gtest/gtest.h>

#include <string>
#include <vector>

#define private public
#include "OnStepXAlignment.h"
#undef private

#include "OnStepXComm.h"
#include "MockFirmware.h"

namespace
{

class AlignmentTest : public ::testing::Test
{
    protected:
        void SetUp() override
        {
            m_comm.setFd(m_fw.start());
            m_align.setComm(&m_comm);
            m_align.initProperties();
        }

        void TearDown() override
        {
            m_fw.stop();
        }

        bool press(const char *property, const char *element)
        {
            ISState states[1] = { ISS_ON };
            char *names[1] = { const_cast<char *>(element) };
            return m_align.handleSwitch(property, states, names, 1);
        }

        std::string status(int field)
        {
            return m_align.m_statusTP[field].getText();
        }

        MockFirmware     m_fw;
        OnStepXComm      m_comm;
        OnStepXAlignment m_align;
};

}  // namespace

TEST_F(AlignmentTest, StartUsesSelectedStarCount)
{
    m_fw.setResponse(":A3", "1");
    m_fw.setResponse(":A?", "913#");

    EXPECT_TRUE(press("OSX_ALIGN_STARS", "STARS_3"));
    EXPECT_TRUE(press("OSX_ALIGN_CONTROL", "START"));

    EXPECT_EQ(m_fw.commands(), (std::vector<std::string> { ":A3", ":A?" }));
    EXPECT_EQ(m_align.m_controlSP.getState(), IPS_OK);
    EXPECT_EQ(m_align.m_controlSP.findOnSwitchIndex(), -1) << "control switches are momentary";
    EXPECT_EQ(status(0), "In progress");
}

TEST_F(AlignmentTest, StartRefusedRaisesAlertAndSkipsStatus)
{
    // A star count above ALIGN_MAX_NUM_STARS falls through to a bare '0'.
    EXPECT_TRUE(press("OSX_ALIGN_STARS", "STARS_9"));
    EXPECT_TRUE(press("OSX_ALIGN_CONTROL", "START"));

    EXPECT_EQ(m_fw.commands(), (std::vector<std::string> { ":A9" }));
    EXPECT_EQ(m_align.m_controlSP.getState(), IPS_ALERT);
}

TEST_F(AlignmentTest, AcceptStarSendsPlus)
{
    m_fw.setResponse(":A+", "1");
    m_fw.setResponse(":A?", "923#");

    EXPECT_TRUE(press("OSX_ALIGN_CONTROL", "ACCEPT"));

    EXPECT_EQ(m_fw.commands(), (std::vector<std::string> { ":A+", ":A?" }));
    EXPECT_EQ(m_align.m_controlSP.getState(), IPS_OK);
}

TEST_F(AlignmentTest, AcceptRefusedWhenNotAligningRaisesAlert)
{
    EXPECT_TRUE(press("OSX_ALIGN_CONTROL", "ACCEPT"));   // CE_ALIGN_NOT_ACTIVE -> bare '0'
    EXPECT_EQ(m_align.m_controlSP.getState(), IPS_ALERT);
}

TEST_F(AlignmentTest, WriteSendsAW)
{
    m_fw.setResponse(":AW", "1");
    EXPECT_TRUE(press("OSX_ALIGN_WRITE", "WRITE"));
    EXPECT_TRUE(m_fw.received(":AW"));
    EXPECT_EQ(m_align.m_writeSP.getState(), IPS_OK);
}

TEST_F(AlignmentTest, WriteRefusedRaisesAlert)
{
    EXPECT_TRUE(press("OSX_ALIGN_WRITE", "WRITE"));
    EXPECT_EQ(m_align.m_writeSP.getState(), IPS_ALERT);
}

TEST_F(AlignmentTest, OtherPropertyNotHandled)
{
    EXPECT_FALSE(press("SOME_OTHER_PROP", "START"));
    EXPECT_TRUE(m_fw.commands().empty());
}

// The current star is always one more than the number accepted, so the
// alignment is complete once it passes the last star.
TEST_F(AlignmentTest, StatusFromReply)
{
    struct Case
    {
        const char *reply, *text, *max, *current, *target;
    };
    const Case cases[] =
    {
        { "900#", "Not started", "9", "0", "0" },
        { "912#", "In progress", "9", "1", "2" },
        { "933#", "In progress", "9", "3", "3" },
        { "943#", "Complete",    "9", "4", "3" },
    };
    for (const auto &c : cases)
    {
        m_fw.setResponse(":A?", c.reply);
        ASSERT_TRUE(m_align.updateStatus()) << c.reply;
        EXPECT_EQ(status(0), c.text) << c.reply;
        EXPECT_EQ(status(1), c.max) << c.reply;
        EXPECT_EQ(status(2), c.current) << c.reply;
        EXPECT_EQ(status(3), c.target) << c.reply;
    }
}

// Each field is '0' + value, so ':' is 10: after accepting 9 of 9 stars.
TEST_F(AlignmentTest, StatusColonIsTen)
{
    m_fw.setResponse(":A?", "9:9#");
    ASSERT_TRUE(m_align.updateStatus());
    EXPECT_EQ(status(0), "Complete");
    EXPECT_EQ(status(2), "10");
}

TEST_F(AlignmentTest, StatusFailsOnBareReply)
{
    EXPECT_FALSE(m_align.updateStatus());
}

TEST_F(AlignmentTest, StatusFailsOnShortReply)
{
    m_fw.setResponse(":A?", "91#");
    EXPECT_FALSE(m_align.updateStatus());
}

TEST_F(AlignmentTest, PolarErrorShowsBothAxes)
{
    m_fw.setResponse(":GX02", "12#");
    m_fw.setResponse(":GX03", "-5#");

    m_align.updatePolarError();

    EXPECT_STREQ(m_align.m_errorTP[0].getText(), "12");
    EXPECT_STREQ(m_align.m_errorTP[1].getText(), "-5");
}

// ---------------------------------------------------------------------------
// Refine polar alignment (:MP#)
// ---------------------------------------------------------------------------
TEST_F(AlignmentTest, RefineSendsMPAfterCheckingThePolarError)
{
    m_align.m_equatorial = true;
    m_fw.setResponse(":GX02", "120#");
    m_fw.setResponse(":GX03", "-45#");
    m_fw.setResponse(":MP", "0");

    EXPECT_TRUE(press("OSX_POLAR_REFINE", "REFINE"));
    EXPECT_EQ(m_align.m_refineSP.getState(), IPS_OK);
    EXPECT_EQ(m_align.m_refineSP[0].getState(), ISS_OFF);
    const auto c = m_fw.commands();
    ASSERT_GE(c.size(), 3u);
    EXPECT_EQ(c[0], ":GX02");
    EXPECT_EQ(c[1], ":GX03");
    EXPECT_EQ(c[2], ":MP");
}

TEST_F(AlignmentTest, RefineRejectedByFirmwareRaisesAlert)
{
    m_align.m_equatorial = true;
    m_fw.setResponse(":GX02", "120#");
    m_fw.setResponse(":GX03", "0#");
    m_fw.setResponse(":MP", "4");                    // mount is parked

    EXPECT_TRUE(press("OSX_POLAR_REFINE", "REFINE"));
    EXPECT_EQ(m_align.m_refineSP.getState(), IPS_ALERT);
}

TEST_F(AlignmentTest, RefineRefusedWithoutAPolarError)
{
    m_align.m_equatorial = true;
    m_fw.setResponse(":GX02", "0#");
    m_fw.setResponse(":GX03", "0#");

    EXPECT_TRUE(press("OSX_POLAR_REFINE", "REFINE"));
    EXPECT_EQ(m_align.m_refineSP.getState(), IPS_ALERT);
    EXPECT_FALSE(m_fw.received(":MP"));
}

TEST_F(AlignmentTest, RefineRefusedWhenThePolarErrorIsUnreadable)
{
    m_align.m_equatorial = true;                     // :GX02# answers a bare '0'
    EXPECT_TRUE(press("OSX_POLAR_REFINE", "REFINE"));
    EXPECT_FALSE(m_fw.received(":MP"));
}

TEST_F(AlignmentTest, RefineRefusedDuringAModelBuild)
{
    // :MP# zeroes model terms the builder is about to restore.
    m_align.m_equatorial = true;
    m_align.setModelBuildCheck([]()
    {
        return true;
    });
    EXPECT_TRUE(press("OSX_POLAR_REFINE", "REFINE"));
    EXPECT_EQ(m_align.m_refineSP.getState(), IPS_ALERT);
    EXPECT_TRUE(m_fw.commands().empty());
}

TEST_F(AlignmentTest, RefineRefusedOnANonEquatorialMount)
{
    EXPECT_TRUE(press("OSX_POLAR_REFINE", "REFINE"));
    EXPECT_TRUE(m_fw.commands().empty());
}

int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
