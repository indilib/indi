/*
    OnStepX INDI Driver — test_onstepxpec.cpp
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
    OnStepX INDI Driver — unit tests: PEC helper

    Replies are scripted from the firmware handlers (Pec.command.cpp and
    Mount.command.cpp :VW#).

    Tests cover:
      - each control button sends its :$QZ command without waiting for a reply
      - pollStatus: state character -> lights, '.' suffix -> index light
      - readWormSteps: :VW# reply parsing
*/

#include <defaultdevice.h>

#include <gtest/gtest.h>

#include <cstring>

#include <string>
#include <vector>

#define private public
#include "OnStepXPec.h"
#undef private

#include "OnStepXComm.h"
#include "MockFirmware.h"

// Outside the anonymous namespace: it is the parameter type of a TEST_P
// fixture, and GCC's -Wsubobject-linkage (an error under FIX_WARNINGS)
// rejects a parameter type with internal linkage.
struct ControlCase
{
    const char *property;
    const char *element;
    const char *command;
};

namespace
{

class PecTest : public ::testing::Test
{
    protected:
        void SetUp() override
        {
            m_comm.setFd(m_fw.start());
            m_pec.setComm(&m_comm);
            m_pec.initProperties();
        }

        void TearDown() override
        {
            m_fw.stop();
        }

        bool press(const char *property, const char *element)
        {
            ISState states[1] = { ISS_ON };
            char *names[1] = { const_cast<char *>(element) };
            return m_pec.handleSwitch(property, states, names, 1);
        }

        MockFirmware m_fw;
        OnStepXComm  m_comm;
        OnStepXPec   m_pec;
};

class PecControlTest : public PecTest, public ::testing::WithParamInterface<ControlCase> {};

}  // namespace

// :$QZ commands reply nothing, even when refused, so the driver must not wait
// for a reply and reads the result from the status poll that follows.
TEST_P(PecControlTest, SendsCommandThenPolls)
{
    m_fw.setResponse(GetParam().command, "");
    m_fw.setResponse(":$QZ?", "I#");

    EXPECT_TRUE(press(GetParam().property, GetParam().element));

    EXPECT_EQ(m_fw.commands(), (std::vector<std::string> { GetParam().command, ":$QZ?" }));
    INDI::PropertySwitch &sp = strcmp(GetParam().property, "OSX_PEC_DATA") ? m_pec.m_controlSP : m_pec.m_dataSP;
    EXPECT_EQ(sp.getState(), IPS_OK);
    EXPECT_EQ(sp.findOnSwitchIndex(), -1) << "PEC actions are momentary";
}

INSTANTIATE_TEST_SUITE_P(Buttons, PecControlTest, ::testing::Values(
                             ControlCase { "OSX_PEC_CONTROL", "PLAY",         ":$QZ+" },
                             ControlCase { "OSX_PEC_CONTROL", "STOP",         ":$QZ-" },
                             ControlCase { "OSX_PEC_CONTROL", "READY_RECORD", ":$QZ/" },
                             ControlCase { "OSX_PEC_DATA",    "CLEAR",        ":$QZZ" },
                             ControlCase { "OSX_PEC_DATA",    "SAVE",         ":$QZ!" }));

// KStars shows a switch with more than four elements as a drop-down list.
TEST_F(PecTest, ActionsShowAsButtons)
{
    EXPECT_LE(m_pec.m_controlSP.count(), 4u);
    EXPECT_LE(m_pec.m_dataSP.count(), 4u);
}

// An element of the other property is not an action of this one.
TEST_F(PecTest, ElementOfOtherPropertyRefused)
{
    EXPECT_TRUE(press("OSX_PEC_CONTROL", "SAVE"));
    EXPECT_TRUE(m_fw.commands().empty());
    EXPECT_EQ(m_pec.m_controlSP.getState(), IPS_ALERT);
}

TEST_F(PecTest, OtherPropertyNotHandled)
{
    ISState states[1] = { ISS_ON };
    char *names[1] = { const_cast<char *>("PLAY") };
    EXPECT_FALSE(m_pec.handleSwitch("SOME_OTHER_PROP", states, names, 1));
    EXPECT_TRUE(m_fw.commands().empty());
}

// Regression: INDI lights default to Ok, so every PEC light showed green from
// connect until the first PEC poll.
TEST_F(PecTest, LightsIdleBeforeFirstPoll)
{
    for (int i = 0; i < 5; i++)
        EXPECT_EQ(m_pec.m_stateSP[i].getState(), IPS_IDLE) << "state light " << i;
    for (int i = 0; i < 2; i++)
        EXPECT_EQ(m_pec.m_indexSP[i].getState(), IPS_IDLE) << "index light " << i;
}

TEST_F(PecTest, PollIgnoredIsIdle)
{
    m_fw.setResponse(":$QZ?", "I#");
    m_pec.pollStatus();
    EXPECT_EQ(m_pec.m_stateSP[0].getState(), IPS_OK);
    EXPECT_EQ(m_pec.m_stateSP.getState(), IPS_IDLE);
}

TEST_F(PecTest, PollActiveStatesAreBusy)
{
    const std::pair<const char *, int> cases[] = { {"p#", 1}, {"P#", 2}, {"r#", 3}, {"R#", 4} };
    for (const auto &c : cases)
    {
        m_fw.setResponse(":$QZ?", c.first);
        m_pec.pollStatus();
        for (int i = 0; i < 5; i++)
            EXPECT_EQ(m_pec.m_stateSP[i].getState(), i == c.second ? IPS_BUSY : IPS_IDLE) << c.first << " light " << i;
        EXPECT_EQ(m_pec.m_stateSP.getState(), IPS_BUSY) << c.first;
    }
}

TEST_F(PecTest, PollDetectsIndexSuffix)
{
    m_fw.setResponse(":$QZ?", "P.#");
    m_pec.pollStatus();
    EXPECT_EQ(m_pec.m_stateSP[2].getState(), IPS_BUSY);
    EXPECT_EQ(m_pec.m_indexSP[1].getState(), IPS_OK);
    EXPECT_EQ(m_pec.m_indexSP.getState(), IPS_OK);

    m_fw.setResponse(":$QZ?", "P#");
    m_pec.pollStatus();
    EXPECT_EQ(m_pec.m_indexSP[1].getState(), IPS_IDLE);
    EXPECT_EQ(m_pec.m_indexSP.getState(), IPS_IDLE);
}

TEST_F(PecTest, PollUnknownStateIsAlert)
{
    m_fw.setResponse(":$QZ?", "X#");
    m_pec.pollStatus();
    EXPECT_EQ(m_pec.m_stateSP.getState(), IPS_ALERT);
}

TEST_F(PecTest, PollKeepsStateWhenCommandFails)
{
    m_fw.setResponse(":$QZ?", "P#");
    m_pec.pollStatus();

    m_fw.setResponse(":$QZ?", "0");            // bare '0': no text reply
    m_pec.pollStatus();
    EXPECT_EQ(m_pec.m_stateSP[2].getState(), IPS_BUSY);
}

TEST_F(PecTest, WormStepsParsesZeroPaddedReply)
{
    m_fw.setResponse(":VW", "004800#");
    m_pec.readWormSteps();
    EXPECT_DOUBLE_EQ(m_pec.m_wormNP[0].getValue(), 4800.0);
    EXPECT_EQ(m_pec.m_wormNP.getState(), IPS_OK);
}

TEST_F(PecTest, WormStepsIgnoresZeroWhenPecIsNotConfigured)
{
    m_fw.setResponse(":VW", "000000#");
    m_pec.readWormSteps();
    EXPECT_EQ(m_pec.m_wormNP.getState(), IPS_IDLE);
}

int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
