/*
    OnStepX INDI Driver — test_onstepxinfo.cpp
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
    OnStepX INDI Driver — Info helper unit tests (reticle control)

    Replies are scripted from the firmware: :B+# and :B-# reply nothing
    (Reticle.command.cpp sets numericReply false with an empty reply).
*/

#include <gtest/gtest.h>

#include <defaultdevice.h>

#include <string>
#include <vector>

#define private public
#include "OnStepXInfo.h"
#undef private
#include "OnStepXComm.h"
#include "MockFirmware.h"

namespace
{
class InfoTest : public ::testing::Test
{
    protected:
        void SetUp() override
        {
            m_comm.setFd(m_fw.start());
            m_info.setComm(&m_comm);
            m_info.initProperties();
            m_fw.setResponse(":B+", "");
            m_fw.setResponse(":B-", "");
        }

        void TearDown() override
        {
            m_fw.stop();
        }

        bool press(const char *element)
        {
            ISState states[1] = { ISS_ON };
            char *names[1] = { const_cast<char *>(element) };
            return m_info.handleSwitch("OSX_RETICLE", states, names, 1);
        }

        MockFirmware m_fw;
        OnStepXComm  m_comm;
        OnStepXInfo  m_info;
};
}

// ISR_NOFMANY shows as tick boxes in KStars; one-shot actions are buttons.
TEST_F(InfoTest, ReticleIsAPairOfButtons)
{
    EXPECT_EQ(m_info.m_reticleSP.getRule(), ISR_ATMOST1);
    EXPECT_EQ(m_info.m_reticleSP.count(), 2u);
}

TEST_F(InfoTest, BrighterSendsBPlus)
{
    EXPECT_TRUE(press("RETICLE_PLUS"));
    EXPECT_TRUE(m_fw.waitFor(":B+"));
    EXPECT_EQ(m_info.m_reticleSP.findOnSwitchIndex(), -1) << "momentary";
    EXPECT_EQ(m_info.m_reticleSP.getState(), IPS_OK);
}

TEST_F(InfoTest, DimmerSendsBMinus)
{
    EXPECT_TRUE(press("RETICLE_MINUS"));
    EXPECT_TRUE(m_fw.waitFor(":B-"));
    EXPECT_EQ(m_info.m_reticleSP.findOnSwitchIndex(), -1) << "momentary";
}

TEST_F(InfoTest, OtherPropertyNotHandled)
{
    ISState states[1] = { ISS_ON };
    char *names[1] = { const_cast<char *>("RETICLE_PLUS") };
    EXPECT_FALSE(m_info.handleSwitch("SOME_OTHER_PROP", states, names, 1));
    m_fw.stop();
    EXPECT_TRUE(m_fw.commands().empty());
}

// indidriver provides a driver main(); define the test runner explicitly.
int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
