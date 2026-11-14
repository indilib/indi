/*
    OnStepX INDI Driver — test_onstepxfocuser.cpp
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
    OnStepX INDI Driver — Focuser unit tests

    Drive a real OnStepXFocuser against MockFirmware and check the exact
    commands sent.  Expected commands follow OnStepX Focuser.command.cpp with
    direct slot addressing (":F<slot><cmd>#"):
      S<n>   absolute goto, bare '1'/'0'
      R<±n>  relative goto, no reply
      Q      stop, no reply
      B<n>   backlash, bare '1'/'0'
      Z      set position to zero, no reply
      G/I/M/T/t  text queries
*/

#include <gtest/gtest.h>

#include "OnStepXComm.h"
#include "OnStepXFocuser.h"
#include "MockFirmware.h"

#include <cstring>
#include <utility>
#include <vector>

namespace
{

class FocuserTest : public ::testing::Test
{
    protected:
        void SetUp() override
        {
            comm.setFd(fw.start());
        }
        void TearDown() override
        {
            fw.stop();
        }

        MockFirmware fw;
        OnStepXComm  comm;
};

// The focuser device under test, with comm wired in.
struct Focuser : public OnStepXFocuser
{
    Focuser(int slot, OnStepXComm *comm) : OnStepXFocuser(slot)
    {
        setComm(comm);
        initProperties();
    }
    using OnStepXFocuser::MoveAbsFocuser;
    using OnStepXFocuser::MoveRelFocuser;
    using OnStepXFocuser::AbortFocuser;
    using OnStepXFocuser::SyncFocuser;
    using OnStepXFocuser::SetFocuserBacklash;
    using OnStepXFocuser::FocusAbsPosNP;
};

}

TEST(FocuserFormat, DirectSlotAddressing)
{
    char buf[OnStepXComm::CMD_MAX_LEN];
    ASSERT_TRUE(OnStepXFocuser::formatCmd(2, "G", buf, sizeof(buf)));
    EXPECT_STREQ(buf, ":F2G#");
    ASSERT_TRUE(OnStepXFocuser::formatCmd(6, "S1500", buf, sizeof(buf)));
    EXPECT_STREQ(buf, ":F6S1500#");
    EXPECT_FALSE(OnStepXFocuser::formatCmd(0, "G", buf, sizeof(buf)));
    EXPECT_FALSE(OnStepXFocuser::formatCmd(7, "G", buf, sizeof(buf)));
}

// Regression: absolute moves used the non-existent :FN command.
TEST_F(FocuserTest, MoveAbsSendsGotoAndAcceptsOne)
{
    fw.setResponse(":F2S1500", "1");
    Focuser f(2, &comm);
    EXPECT_EQ(f.MoveAbsFocuser(1500), IPS_BUSY);
    EXPECT_TRUE(fw.received(":F2S1500"));
}

TEST_F(FocuserTest, MoveAbsRejected)
{
    fw.setResponse(":F2S999999", "0");
    Focuser f(2, &comm);
    EXPECT_EQ(f.MoveAbsFocuser(999999), IPS_ALERT);
}

// Regression: the position stayed at the start of a move until the firmware
// reported it stopped.  Ekos started autofocus from that stale value (0, not
// 23000), and the INDI base built absolute targets from it.
TEST_F(FocuserTest, PositionFollowsAMoveThenSettles)
{
    fw.setResponse(":F1S1500", "1");
    fw.setResponseSequence(":F1T", { "M3#", "M3#", "S3#" });
    fw.setResponseSequence(":F1G", { "400#", "1100#", "1500#" });
    Focuser f(1, &comm);
    ASSERT_EQ(f.MoveAbsFocuser(1500), IPS_BUSY);
    f.FocusAbsPosNP.setState(IPS_BUSY);           // as FocuserInterface sets it

    f.pollMove();
    EXPECT_DOUBLE_EQ(f.FocusAbsPosNP[0].getValue(), 400);
    EXPECT_EQ(f.FocusAbsPosNP.getState(), IPS_BUSY);
    f.pollMove();
    EXPECT_DOUBLE_EQ(f.FocusAbsPosNP[0].getValue(), 1100);
    EXPECT_EQ(f.FocusAbsPosNP.getState(), IPS_BUSY);
    f.pollMove();
    EXPECT_DOUBLE_EQ(f.FocusAbsPosNP[0].getValue(), 1500);
    EXPECT_EQ(f.FocusAbsPosNP.getState(), IPS_OK);
}

// A status that lost a byte ("3" for "M3") must not end a move in progress.
TEST_F(FocuserTest, MoveNotEndedByAStatusThatLostAByte)
{
    fw.setResponse(":F1S1500", "1");
    fw.setResponseSequence(":F1T", { "3#", "M#", "S3#" });
    fw.setResponseSequence(":F1G", { "1500#" });
    Focuser f(1, &comm);
    ASSERT_EQ(f.MoveAbsFocuser(1500), IPS_BUSY);
    f.FocusAbsPosNP.setState(IPS_BUSY);

    f.pollMove();
    f.pollMove();
    EXPECT_EQ(f.FocusAbsPosNP.getState(), IPS_BUSY);
    f.pollMove();
    EXPECT_EQ(f.FocusAbsPosNP.getState(), IPS_OK);
    EXPECT_DOUBLE_EQ(f.FocusAbsPosNP[0].getValue(), 1500);
}

// Regression: relative moves used the non-existent :Fm command, and
// inward increased the position.
TEST_F(FocuserTest, MoveRelInwardDecreases)
{
    fw.setResponse(":F1R-100", "");
    Focuser f(1, &comm);
    EXPECT_EQ(f.MoveRelFocuser(Focuser::FOCUS_INWARD, 100), IPS_BUSY);
    EXPECT_TRUE(fw.waitFor(":F1R-100"));
}

TEST_F(FocuserTest, MoveRelOutwardIncreases)
{
    fw.setResponse(":F1R+250", "");
    Focuser f(1, &comm);
    EXPECT_EQ(f.MoveRelFocuser(Focuser::FOCUS_OUTWARD, 250), IPS_BUSY);
    EXPECT_TRUE(fw.waitFor(":F1R+250"));
}

TEST_F(FocuserTest, AbortSendsStop)
{
    fw.setResponse(":F3Q", "");
    Focuser f(3, &comm);
    EXPECT_TRUE(f.AbortFocuser());
    EXPECT_TRUE(fw.waitFor(":F3Q"));
}

// The firmware can only zero the position; any other sync must fail
// without sending anything.
TEST_F(FocuserTest, SyncZeroOnly)
{
    fw.setResponse(":F1Z", "");
    Focuser f(1, &comm);
    EXPECT_FALSE(f.SyncFocuser(1234));
    EXPECT_TRUE(f.SyncFocuser(0));
    EXPECT_TRUE(fw.waitFor(":F1Z"));
    EXPECT_EQ(fw.commands().size(), 1u);
}

TEST_F(FocuserTest, BacklashIsBool)
{
    fw.setResponse(":F4B50", "1");
    Focuser f(4, &comm);
    EXPECT_TRUE(f.SetFocuserBacklash(50));
}

// Commands never touch the firmware's active-focuser selection.
TEST_F(FocuserTest, NeverSelectsActiveFocuser)
{
    fw.setResponse(":F2S10", "1");
    fw.setResponse(":F2R+5", "");
    fw.setResponse(":F2Q", "");
    Focuser f(2, &comm);
    f.MoveAbsFocuser(10);
    f.MoveRelFocuser(Focuser::FOCUS_OUTWARD, 5);
    f.AbortFocuser();
    EXPECT_TRUE(fw.waitFor(":F2Q"));
    for (const auto &c : fw.commands())
        EXPECT_NE(c.rfind(":FA", 0), 0u) << c;
}

// ---------------------------------------------------------------------------
// Home and temperature compensation (Focuser.command.cpp):
//   h / H      go to home / set current position as home, no reply
//   c          compensation on?  bare '1'/'0'
//   c0 / c1    off / on, bare '1' (the firmware stays off without a
//              temperature reading, but still replies '1')
//   C / C<v>   coefficient um/C, "%7.5f#" / bare '1'
//   d / d<n>   deadband in steps, "%ld#" / bare '1'
//   e          temperature change since compensation started, "%3.1f#"
// ---------------------------------------------------------------------------
namespace
{
struct ConnectedFocuser : public Focuser
{
    ConnectedFocuser(int slot, OnStepXComm *comm) : Focuser(slot, comm)
    {
        setConnected(true);
        updateProperties();
    }
    bool press(const char *prop, const char *element)
    {
        ISState states[1] = { ISS_ON };
        char *names[1] = { const_cast<char *>(element) };
        return ISNewSwitch(getDeviceName(), prop, states, names, 1);
    }
    bool setNumbers(const char *prop, std::vector<std::pair<const char *, double>> v)
    {
        std::vector<double> values;
        std::vector<char *> names;
        for (auto &p : v)
        {
            names.push_back(const_cast<char *>(p.first));
            values.push_back(p.second);
        }
        return ISNewNumber(getDeviceName(), prop, values.data(), names.data(), static_cast<int>(v.size()));
    }
};

void scriptFocuser(MockFirmware &fw, const char *temperature)
{
    fw.setResponse(":F2I", "0#");
    fw.setResponse(":F2M", "50000#");
    fw.setResponse(":F2G", "25000#");
    fw.setResponse(":F2t", temperature);
    fw.setResponse(":F2c", "0");
    fw.setResponse(":F2C", "-1.50000#");
    fw.setResponse(":F2d", "20#");
    fw.setResponse(":F2e", "0.0#");
    fw.setResponse(":F2h", "");                      // h and H reply nothing
    fw.setResponse(":F2H", "");
}
}

TEST_F(FocuserTest, GoToHomeSendsLowerH)
{
    scriptFocuser(fw, "12.5#");
    ConnectedFocuser f(2, &comm);
    EXPECT_TRUE(f.press("OSX_FOCUS_HOME", "FOCUS_GOTO_HOME"));
    EXPECT_TRUE(fw.waitFor(":F2h"));
    EXPECT_FALSE(fw.received(":F2H"));
    EXPECT_EQ(f.getSwitch("OSX_FOCUS_HOME").getState(), IPS_OK);
}

TEST_F(FocuserTest, SetHomeSendsUpperHAndReadsThePosition)
{
    scriptFocuser(fw, "12.5#");
    ConnectedFocuser f(2, &comm);
    fw.setResponse(":F2G", "30000#");                // the home position
    EXPECT_TRUE(f.press("OSX_FOCUS_HOME", "FOCUS_SET_HOME"));
    EXPECT_TRUE(fw.received(":F2H"));
    EXPECT_FALSE(fw.received(":F2h"));
    EXPECT_EQ(f.getNumber("ABS_FOCUS_POSITION")[0].getValue(), 30000);
}

TEST_F(FocuserTest, TemperatureCompensationReadOnConnect)
{
    scriptFocuser(fw, "12.5#");
    ConnectedFocuser f(2, &comm);
    auto np = f.getNumber("OSX_FOCUS_TCF_SETTINGS");
    ASSERT_TRUE(np.isValid());
    EXPECT_DOUBLE_EQ(np[0].getValue(), -1.5);
    EXPECT_DOUBLE_EQ(np[1].getValue(), 20);
    EXPECT_EQ(f.getSwitch("OSX_FOCUS_TCF")[0].getState(), ISS_ON);    // off
    auto temp = f.getNumber("FOCUS_TEMPERATURE_NP");
    ASSERT_TRUE(temp.isValid());
    EXPECT_DOUBLE_EQ(temp[0].getValue(), 12.5);
}

// The travel limits are firmware settings; a client must not widen them.
TEST_F(FocuserTest, MaxPositionFromFirmwareIsReadOnly)
{
    scriptFocuser(fw, "nan#");
    ConnectedFocuser f(2, &comm);
    auto np = f.getNumber("FOCUS_MAX");
    ASSERT_TRUE(np.isValid());
    EXPECT_EQ(np.getPermission(), IP_RO);
}

TEST_F(FocuserTest, NoTemperatureCompensationWithoutATemperature)
{
    scriptFocuser(fw, "nan#");
    ConnectedFocuser f(2, &comm);
    EXPECT_FALSE(f.getSwitch("OSX_FOCUS_TCF").isValid());
    // Regression: the temperature was shown as 0 C with no sensor.
    EXPECT_FALSE(f.getNumber("FOCUS_TEMPERATURE_NP").isValid());
    EXPECT_FALSE(fw.received(":F2C"));
    f.press("OSX_FOCUS_TCF", "TCF_ON");
    EXPECT_FALSE(fw.received(":F2c1"));
}

TEST_F(FocuserTest, CompensationOnIsReadBack)
{
    scriptFocuser(fw, "12.5#");
    ConnectedFocuser f(2, &comm);
    fw.setResponse(":F2c1", "1");
    fw.setResponse(":F2c", "1");
    EXPECT_TRUE(f.press("OSX_FOCUS_TCF", "TCF_ON"));
    EXPECT_TRUE(fw.received(":F2c1"));
    EXPECT_EQ(f.getSwitch("OSX_FOCUS_TCF")[1].getState(), ISS_ON);
    EXPECT_EQ(f.getSwitch("OSX_FOCUS_TCF").getState(), IPS_OK);
}

TEST_F(FocuserTest, CompensationQuietlyRefusedRaisesAlert)
{
    // Parked, or the temperature reading was lost: '1' back, but still off.
    scriptFocuser(fw, "12.5#");
    ConnectedFocuser f(2, &comm);
    fw.setResponse(":F2c1", "1");
    EXPECT_TRUE(f.press("OSX_FOCUS_TCF", "TCF_ON"));
    EXPECT_EQ(f.getSwitch("OSX_FOCUS_TCF")[0].getState(), ISS_ON);
    EXPECT_EQ(f.getSwitch("OSX_FOCUS_TCF").getState(), IPS_ALERT);
}

TEST_F(FocuserTest, CompensationSettingsSendOnlyWhatChanged)
{
    scriptFocuser(fw, "12.5#");
    ConnectedFocuser f(2, &comm);
    fw.setResponse(":F2C2.250", "1");
    fw.setResponse(":F2C", "2.25000#");
    EXPECT_TRUE(f.setNumbers("OSX_FOCUS_TCF_SETTINGS", { { "TCF_COEFFICIENT", 2.25 }, { "TCF_DEADBAND", 20 } }));
    EXPECT_TRUE(fw.received(":F2C2.250"));
    for (const auto &c : fw.commands())
        EXPECT_NE(c.rfind(":F2d", 0) == 0 && c.size() > 4, true) << "deadband sent: " << c;
    EXPECT_EQ(f.getNumber("OSX_FOCUS_TCF_SETTINGS").getState(), IPS_OK);
}

TEST_F(FocuserTest, CompensationDeadbandRefusedRaisesAlert)
{
    scriptFocuser(fw, "12.5#");
    ConnectedFocuser f(2, &comm);                        // :F2d5# not scripted: bare '0'
    EXPECT_TRUE(f.setNumbers("OSX_FOCUS_TCF_SETTINGS", { { "TCF_DEADBAND", 5 } }));
    EXPECT_TRUE(fw.received(":F2d5"));
    EXPECT_EQ(f.getNumber("OSX_FOCUS_TCF_SETTINGS").getState(), IPS_ALERT);
}

// The backlash lives in the firmware's NV: read it at connect, and keep it out
// of the INDI config so a reload cannot overwrite a change made elsewhere.
TEST_F(FocuserTest, BacklashReadAtConnectAndNotSaved)
{
    scriptFocuser(fw, "12.5#");
    fw.setResponse(":F2B", "40#");
    ConnectedFocuser f(2, &comm);
    EXPECT_EQ(f.getNumber("FOCUS_BACKLASH_STEPS")[0].getValue(), 40);
    EXPECT_EQ(f.getSwitch("FOCUS_BACKLASH_TOGGLE")[0].getState(), ISS_ON);    // INDI_ENABLED

    char *buf = nullptr;
    size_t len = 0;
    FILE *fp = open_memstream(&buf, &len);
    ASSERT_NE(fp, nullptr);
    f.saveConfigItems(fp);
    fclose(fp);
    const std::string config(buf, len);
    free(buf);
    EXPECT_EQ(config.find("FOCUS_BACKLASH"), std::string::npos);
    EXPECT_EQ(config.find("FOCUS_MAX"), std::string::npos);
}

int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
