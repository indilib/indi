/*
    OnStepX INDI Driver — test_onstepxrotator.cpp
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
    OnStepX INDI Driver — Rotator helper unit tests

    Drive OnStepXRotator against MockFirmware.  Expected behaviour follows
    OnStepX Rotator.command.cpp:
      :rS[s]DDD:MM:SS#  goto, bare '1'/'0' (3-digit degrees required)
      :rQ# :rC#         stop / home, no reply
      :rb[n]#           backlash, bare '1'/'0'
      :rG#              angle "sDDD*MM" (text)
      :rT#              "M<rate>" moving, "S[D][R]<rate>" stopped (text)
      :rI# :rM#         travel limits, degrees (text)

    INDI angles are 0..360; they are mapped into the firmware's travel range
    (e.g. -180..180) by adding or subtracting 360.
*/

#include <gtest/gtest.h>

#include "OnStepXRotator.h"
#include "OnStepXComm.h"
#include "MockFirmware.h"

#include <cstring>

#include <defaultdevice.h>

namespace
{

class RotatorTest : public ::testing::Test
{
    protected:
        void SetUp() override
        {
            comm.setFd(fw.start());
            rot.setComm(&comm);
        }
        void TearDown() override
        {
            fw.stop();
        }

        // Read the travel limits as on connect.
        void connectWithLimits(const char *minDeg, const char *maxDeg)
        {
            fw.setResponse(":rI", std::string(minDeg) + "#");
            fw.setResponse(":rM", std::string(maxDeg) + "#");
            fw.setResponse(":rG", "+000*00#");
            fw.setResponse(":rb", "0#");
            rot.readInitial();
        }

        MockFirmware   fw;
        OnStepXComm    comm;
        OnStepXRotator rot;
};

}

// ---------------------------------------------------------------------------
// Pure helpers
// ---------------------------------------------------------------------------
TEST(RotatorFormat, GotoHasSignAndThreeDigitDegrees)
{
    char buf[OnStepXComm::CMD_MAX_LEN];
    ASSERT_TRUE(OnStepXRotator::formatGoto(90.0, buf, sizeof(buf)));
    EXPECT_STREQ(buf, ":rS+090:00:00#");
    ASSERT_TRUE(OnStepXRotator::formatGoto(-90.5, buf, sizeof(buf)));
    EXPECT_STREQ(buf, ":rS-090:30:00#");
    ASSERT_TRUE(OnStepXRotator::formatGoto(5.0 + 59.0 / 60.0 + 59.6 / 3600.0, buf, sizeof(buf)));
    EXPECT_STREQ(buf, ":rS+006:00:00#");   // rounding carries
}

TEST(RotatorFormat, MapToRange)
{
    double out = 0;
    ASSERT_TRUE(OnStepXRotator::mapToRange(270.0, -180.0, 180.0, out));
    EXPECT_DOUBLE_EQ(out, -90.0);
    ASSERT_TRUE(OnStepXRotator::mapToRange(90.0, -180.0, 180.0, out));
    EXPECT_DOUBLE_EQ(out, 90.0);
    ASSERT_TRUE(OnStepXRotator::mapToRange(270.0, 0.0, 360.0, out));
    EXPECT_DOUBLE_EQ(out, 270.0);
    EXPECT_FALSE(OnStepXRotator::mapToRange(180.0, -90.0, 90.0, out));
}

// ---------------------------------------------------------------------------
// moveToAngle
// ---------------------------------------------------------------------------
TEST_F(RotatorTest, MoveAccepted)
{
    connectWithLimits("-180", "180");
    fw.setResponse(":rS+090:00:00", "1");
    EXPECT_EQ(rot.moveToAngle(90.0), IPS_BUSY);
    EXPECT_TRUE(fw.received(":rS+090:00:00"));
}

TEST_F(RotatorTest, MoveRejected)
{
    connectWithLimits("-180", "180");
    fw.setResponse(":rS+045:00:00", "0");
    EXPECT_EQ(rot.moveToAngle(45.0), IPS_ALERT);
}

// Regression: 270 used to be sent as 270, outside a -180..180 rotator.
TEST_F(RotatorTest, MoveMapsIntoSymmetricRange)
{
    connectWithLimits("-180", "180");
    fw.setResponse(":rS-090:00:00", "1");
    EXPECT_EQ(rot.moveToAngle(270.0), IPS_BUSY);
    EXPECT_TRUE(fw.received(":rS-090:00:00"));
}

TEST_F(RotatorTest, MoveKeepsAngleInFullRange)
{
    connectWithLimits("0", "360");
    fw.setResponse(":rS+270:00:00", "1");
    EXPECT_EQ(rot.moveToAngle(270.0), IPS_BUSY);
    EXPECT_TRUE(fw.received(":rS+270:00:00"));
}

TEST_F(RotatorTest, MoveOutsideTravelIsRefusedWithoutSending)
{
    connectWithLimits("-90", "90");
    const size_t before = fw.commands().size();
    EXPECT_EQ(rot.moveToAngle(180.0), IPS_ALERT);
    EXPECT_EQ(fw.commands().size(), before);
}

// ---------------------------------------------------------------------------
// Stop / home / backlash
// ---------------------------------------------------------------------------
TEST_F(RotatorTest, AbortSendsStop)
{
    fw.setResponse(":rQ", "");
    EXPECT_TRUE(rot.abortRotator());
    EXPECT_TRUE(fw.waitFor(":rQ"));
}

TEST_F(RotatorTest, HomeReturnsBusy)
{
    fw.setResponse(":rC", "");
    EXPECT_EQ(rot.homeRotator(), IPS_BUSY);
    EXPECT_TRUE(fw.waitFor(":rC"));
}

TEST_F(RotatorTest, BacklashAccepted)
{
    fw.setResponse(":rb50", "1");
    EXPECT_TRUE(rot.setBacklash(50));
}

TEST_F(RotatorTest, BacklashRejected)
{
    fw.setResponse(":rb9999", "0");
    EXPECT_FALSE(rot.setBacklash(9999));
}

// ---------------------------------------------------------------------------
// Polling
// ---------------------------------------------------------------------------
TEST_F(RotatorTest, PollMoving)
{
    fw.setResponse(":rG", "+045*30#");
    fw.setResponse(":rT", "M3#");
    auto r = rot.pollStatus();
    EXPECT_TRUE(r.angleValid);
    EXPECT_NEAR(r.angle, 45.5, 0.01);
    EXPECT_TRUE(r.statusValid);
    EXPECT_TRUE(r.moving);
}

// Regression (hardware, 2026-11-12): any :rT# reply not starting with 'M' was
// read as stopped, so one that lost its 'M' ("3" for "M3") ended a move to
// 30 deg at 5.5 deg, and the next goto was refused as the rotator was busy.
TEST_F(RotatorTest, PollIgnoresAStatusThatLostAByte)
{
    fw.setResponse(":rG", "+005*30#");
    const char *corrupted[] = { "3#", "D3#", "M#", "S#", "Mx#" };
    for (const char *bad : corrupted)
    {
        fw.setResponse(":rT", bad);
        EXPECT_FALSE(rot.pollStatus().statusValid) << bad;
    }
    fw.setResponse(":rT", "SDR3#");
    auto r = rot.pollStatus();
    EXPECT_TRUE(r.statusValid);
    EXPECT_FALSE(r.moving);
}

TEST_F(RotatorTest, PollStoppedNegativeAngleNormalised)
{
    fw.setResponse(":rG", "-090*00#");
    fw.setResponse(":rT", "S3#");
    auto r = rot.pollStatus();
    EXPECT_TRUE(r.angleValid);
    EXPECT_NEAR(r.angle, 270.0, 0.01);
    EXPECT_FALSE(r.moving);
}

TEST_F(RotatorTest, ReadInitialAngleBacklashAndLimits)
{
    fw.setResponse(":rI", "-180#");
    fw.setResponse(":rM", "180#");
    fw.setResponse(":rG", "+090*00#");
    fw.setResponse(":rb", "25#");
    auto init = rot.readInitial();
    EXPECT_TRUE(init.angleValid);
    EXPECT_NEAR(init.angle, 90.0, 0.01);
    EXPECT_TRUE(init.backlashValid);
    EXPECT_EQ(init.backlash, 25);
    EXPECT_TRUE(fw.received(":rI"));
    EXPECT_TRUE(fw.received(":rM"));
}

// ---------------------------------------------------------------------------
// Sync: the firmware can only set 0 deg (:rZ#) or half travel (:rF#); both
// reply nothing, so the angle is read back.
// ---------------------------------------------------------------------------
TEST_F(RotatorTest, SyncToZeroSendsRZ)
{
    connectWithLimits("0", "360");
    fw.setResponse(":rZ", "");                       // replies nothing
    EXPECT_TRUE(rot.syncAngle(0.0));
    EXPECT_TRUE(fw.received(":rZ"));
}

TEST_F(RotatorTest, SyncToHalfTravelSendsRF)
{
    connectWithLimits("0", "360");
    fw.setResponse(":rF", "");
    fw.setResponse(":rG", "+180*00#");
    EXPECT_TRUE(rot.syncAngle(180.0));
    EXPECT_TRUE(fw.received(":rF"));
    EXPECT_FALSE(fw.received(":rZ"));
}

TEST_F(RotatorTest, SyncToOtherAnglesRefusedWithoutSending)
{
    connectWithLimits("0", "360");
    const size_t before = fw.commands().size();
    EXPECT_FALSE(rot.syncAngle(90.0));
    EXPECT_EQ(fw.commands().size(), before);
}

TEST_F(RotatorTest, SyncNotAppliedIsDetected)
{
    // e.g. the rotator is moving, so the firmware ignores :rZ#.
    connectWithLimits("0", "360");
    fw.setResponse(":rZ", "");
    fw.setResponse(":rG", "+045*00#");
    EXPECT_FALSE(rot.syncAngle(0.0));
}

// ---------------------------------------------------------------------------
// De-rotator direction: :rR# toggles; the state is the 'R' in a stopped :rT#
// ---------------------------------------------------------------------------
namespace
{
class TestDevice : public INDI::DefaultDevice
{
    public:
        TestDevice()
        {
            setDeviceName("Test");
        }
    protected:
        const char *getDefaultName() override
        {
            return "Test";
        }
};

class DerotatorTest : public RotatorTest
{
    protected:
        void SetUp() override
        {
            RotatorTest::SetUp();
            rot.setDevice(&dev);
            rot.initProperties(true);
            fw.setResponse(":rT", "S3#");
            fw.setResponse(":rR", "");               // replies nothing
            rot.updateProperties(true, true);
        }
        bool press(const char *element)
        {
            ISState states[2] = { ISS_OFF, ISS_OFF };
            char *names[2] = { const_cast<char *>("DEROTATE_NORMAL"), const_cast<char *>("DEROTATE_REVERSED") };
            states[strcmp(element, "DEROTATE_REVERSED") == 0 ? 1 : 0] = ISS_ON;
            return rot.handleSwitch("OSX_ROT_DEROTATE_REVERSE", states, names, 2);
        }
        TestDevice dev;
};
}

TEST_F(DerotatorTest, ReverseSendsToggleAndChecksTheResult)
{
    // The mock's :rT# does not change, so the toggle is seen not to apply.
    EXPECT_TRUE(press("DEROTATE_REVERSED"));
    EXPECT_TRUE(fw.waitFor(":rR"));
    EXPECT_EQ(dev.getSwitch("OSX_ROT_DEROTATE_REVERSE").getState(), IPS_ALERT);
}

TEST_F(DerotatorTest, AlreadyReversedSendsNoToggle)
{
    fw.setResponse(":rT", "SDR3#");
    EXPECT_TRUE(press("DEROTATE_REVERSED"));
    EXPECT_FALSE(fw.received(":rR"));
    EXPECT_EQ(dev.getSwitch("OSX_ROT_DEROTATE_REVERSE")[1].getState(), ISS_ON);
    EXPECT_EQ(dev.getSwitch("OSX_ROT_DEROTATE_REVERSE").getState(), IPS_OK);
}

TEST_F(DerotatorTest, ReverseRefusedWhileMoving)
{
    fw.setResponse(":rT", "M3#");
    EXPECT_TRUE(press("DEROTATE_REVERSED"));
    EXPECT_FALSE(fw.received(":rR"));
    EXPECT_EQ(dev.getSwitch("OSX_ROT_DEROTATE_REVERSE").getState(), IPS_ALERT);
}

TEST_F(DerotatorTest, PollShowsTheFirmwareDirection)
{
    fw.setResponse(":rG", "+010*00#");
    fw.setResponse(":rT", "SR3#");
    rot.pollStatus();
    EXPECT_EQ(dev.getSwitch("OSX_ROT_DEROTATE_REVERSE")[1].getState(), ISS_ON);
}

int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
