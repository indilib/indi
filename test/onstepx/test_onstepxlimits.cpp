/*
    OnStepX INDI Driver — test_onstepxlimits.cpp
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
    OnStepX INDI Driver — Limits/Home unit tests (Stage 6)

    OnStepXLimits protocol logic is tested against MockFirmware.
    No INDI device instance needed.
*/

#include <chrono>
#include <string>

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <defaultdevice.h>

#include "OnStepXComm.h"
#include "OnStepXLimits.h"
#include "MockFirmware.h"

// ---------------------------------------------------------------------------
// Fixture.  Replies are scripted from the firmware's Limits.command.cpp,
// Home.command.cpp and Mount.command.cpp.
// ---------------------------------------------------------------------------
class LimitsTest : public ::testing::Test
{
    protected:
        void SetUp() override
        {
            m_comm.setFd(m_fw.start());
            m_limits.setComm(&m_comm);
            // setDevice(nullptr) is fine for unit tests — logging suppressed
        }

        void TearDown() override
        {
            m_fw.stop();
        }

        // :Gh# is sDD*, :Go# is DD*, :GXE9#/:GXEA# are minutes, :%BR#/:%BD# arcsec.
        void scriptLimits()
        {
            m_fw.setResponse(":Gh",   "-10*#");
            m_fw.setResponse(":Go",   "89*#");
            m_fw.setResponse(":GXE9", "15#");
            m_fw.setResponse(":GXEA", "30#");
            m_fw.setResponse(":%BR",  "120#");
            m_fw.setResponse(":%BD",  "45#");
        }

        MockFirmware  m_fw;
        OnStepXComm   m_comm;
        OnStepXLimits m_limits;
};

// ---------------------------------------------------------------------------
// Home: Find
// ---------------------------------------------------------------------------
// :hC# and :hF# reply nothing in the firmware (even on failure), so the
// driver must send them without waiting for a reply.
TEST(LimitsHome, HomeFind_SendsHCWithoutWaiting)
{
    MockFirmware fw;
    fw.setResponse(":hC", "");
    OnStepXComm comm;
    comm.setFd(fw.start());
    OnStepXLimits limits;
    limits.setComm(&comm);

    const auto start = std::chrono::steady_clock::now();
    EXPECT_TRUE(limits.homeFind());
    EXPECT_LT(std::chrono::steady_clock::now() - start, std::chrono::milliseconds(100));
    EXPECT_TRUE(fw.waitFor(":hC"));
    fw.stop();
}

TEST(LimitsHome, HomeSet_SendsHFWithoutWaiting)
{
    MockFirmware fw;
    fw.setResponse(":hF", "");
    OnStepXComm comm;
    comm.setFd(fw.start());
    OnStepXLimits limits;
    limits.setComm(&comm);

    const auto start = std::chrono::steady_clock::now();
    EXPECT_TRUE(limits.homeSet());
    EXPECT_LT(std::chrono::steady_clock::now() - start, std::chrono::milliseconds(100));
    EXPECT_TRUE(fw.waitFor(":hF"));
    fw.stop();
}

// ---------------------------------------------------------------------------
// Auto-home on boot.  :hA[n]# replies nothing.
// ---------------------------------------------------------------------------
TEST_F(LimitsTest, SetAutoHome_EnableSendsHA1)
{
    m_fw.setResponse(":hA1", "");
    EXPECT_TRUE(m_limits.setAutoHome(true));
    EXPECT_TRUE(m_fw.waitFor(":hA1"));
}

TEST_F(LimitsTest, SetAutoHome_DisableSendsHA0)
{
    m_fw.setResponse(":hA0", "");
    EXPECT_TRUE(m_limits.setAutoHome(false));
    EXPECT_TRUE(m_fw.waitFor(":hA0"));
}

TEST_F(LimitsTest, SyncAutoHome_MirrorsFirmwareFlag)
{
    m_limits.initProperties();
    ASSERT_EQ(ISS_ON, m_limits.autoBootSP()[1].getState());

    m_limits.syncAutoHome(true);
    EXPECT_EQ(ISS_ON,  m_limits.autoBootSP()[0].getState());
    EXPECT_EQ(ISS_OFF, m_limits.autoBootSP()[1].getState());
    EXPECT_EQ(IPS_OK,  m_limits.autoBootSP().getState());

    m_limits.syncAutoHome(false);
    EXPECT_EQ(ISS_OFF, m_limits.autoBootSP()[0].getState());
    EXPECT_EQ(ISS_ON,  m_limits.autoBootSP()[1].getState());
}

TEST_F(LimitsTest, SyncAutoHome_DoesNotClearAlertWhenUnchanged)
{
    m_limits.initProperties();
    m_limits.autoBootSP().setState(IPS_ALERT);

    m_limits.syncAutoHome(false);

    EXPECT_EQ(IPS_ALERT, m_limits.autoBootSP().getState());
    EXPECT_TRUE(m_fw.commands().empty()) << "the flag comes from the status poll";
}

// ---------------------------------------------------------------------------
// Home offsets.  :hC1,n# and :hC2,n# reply nothing.
// ---------------------------------------------------------------------------
TEST_F(LimitsTest, WriteHomeOffsets_SendsBothAxes)
{
    m_fw.setResponse(":hC1,120", "");
    m_fw.setResponse(":hC2,-45", "");

    EXPECT_TRUE(m_limits.writeHomeOffsets(120.0, -45.0));

    EXPECT_TRUE(m_fw.waitFor(":hC1,120"));
    EXPECT_TRUE(m_fw.waitFor(":hC2,-45"));
}

// ---------------------------------------------------------------------------
// readLimits
// ---------------------------------------------------------------------------
TEST_F(LimitsTest, ReadLimits_ParsesFirmwareFormats)
{
    scriptLimits();
    m_limits.initProperties();

    EXPECT_TRUE(m_limits.readLimits());

    EXPECT_NEAR(-10.0, m_limits.horizonLimitNP()[0].getValue(),  0.01);
    EXPECT_NEAR( 89.0, m_limits.horizonLimitNP()[1].getValue(),  0.01);
    EXPECT_NEAR( 15.0, m_limits.meridianLimitNP()[0].getValue(), 0.01);
    EXPECT_NEAR( 30.0, m_limits.meridianLimitNP()[1].getValue(), 0.01);
    EXPECT_NEAR(120.0, m_limits.backlashNP()[0].getValue(), 0.1);
    EXPECT_NEAR( 45.0, m_limits.backlashNP()[1].getValue(), 0.1);
}

TEST_F(LimitsTest, ReadLimits_FailureRaisesAlert)
{
    m_limits.initProperties();

    EXPECT_FALSE(m_limits.readLimits());      // unscripted: bare '0' to everything

    EXPECT_EQ(IPS_ALERT, m_limits.horizonLimitNP().getState());
    EXPECT_EQ(IPS_ALERT, m_limits.meridianLimitNP().getState());
    EXPECT_EQ(IPS_ALERT, m_limits.backlashNP().getState());
}

// ---------------------------------------------------------------------------
// handleNumber — the set commands reply a bare '1'/'0'
// ---------------------------------------------------------------------------
TEST_F(LimitsTest, HandleNumber_HorizonLimitsSendsShSo)
{
    m_limits.initProperties();
    m_fw.setResponse(":Sh-5", "1");
    m_fw.setResponse(":So85", "1");

    double values[] = { -5.0, 85.0 };
    const char *names[] = { "HORIZON_MIN", "HORIZON_MAX" };
    EXPECT_TRUE(m_limits.handleNumber("HORIZON_LIMITS", values, const_cast<char **>(names), 2));

    EXPECT_TRUE(m_fw.received(":Sh-5"));
    EXPECT_TRUE(m_fw.received(":So85"));
    EXPECT_EQ(IPS_OK, m_limits.horizonLimitNP().getState());
}

TEST_F(LimitsTest, HandleNumber_MeridianLimitsSendsSXE9SXEA)
{
    m_limits.initProperties();
    m_fw.setResponse(":SXE9,20", "1");
    m_fw.setResponse(":SXEA,40", "1");

    double values[] = { 20.0, 40.0 };
    const char *names[] = { "MERIDIAN_EAST", "MERIDIAN_WEST" };
    EXPECT_TRUE(m_limits.handleNumber("MERIDIAN_LIMITS", values, const_cast<char **>(names), 2));

    EXPECT_TRUE(m_fw.received(":SXE9,20"));
    EXPECT_TRUE(m_fw.received(":SXEA,40"));
    EXPECT_EQ(IPS_OK, m_limits.meridianLimitNP().getState());
}

// The firmware applies meridian limits to a GEM only: an Alt-Az or fork mount
// gets no MERIDIAN_LIMITS, reads nothing and sends nothing for them.
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
}

TEST_F(LimitsTest, MeridianLimits_NotOfferedOnNonGem)
{
    TestDevice dev;
    m_limits.setDevice(&dev);
    m_limits.initProperties();
    scriptLimits();
    m_limits.updateProperties(true, false, false);

    EXPECT_FALSE(dev.getProperty("MERIDIAN_LIMITS").isValid());
    EXPECT_TRUE(dev.getProperty("HORIZON_LIMITS").isValid());
    EXPECT_FALSE(m_fw.received(":GXE9"));
    EXPECT_FALSE(m_fw.received(":GXEA"));

    double values[] = { 20.0, 40.0 };
    const char *names[] = { "MERIDIAN_EAST", "MERIDIAN_WEST" };
    EXPECT_TRUE(m_limits.handleNumber("MERIDIAN_LIMITS", values, const_cast<char **>(names), 2));
    EXPECT_FALSE(m_fw.received(":SXE9,20"));
    m_limits.updateProperties(false, false);
}

TEST_F(LimitsTest, MeridianLimits_OfferedOnGem)
{
    TestDevice dev;
    m_limits.setDevice(&dev);
    m_limits.initProperties();
    scriptLimits();
    m_limits.updateProperties(true, false, true);

    EXPECT_TRUE(dev.getProperty("MERIDIAN_LIMITS").isValid());
    EXPECT_TRUE(m_fw.received(":GXE9"));
    m_limits.updateProperties(false, false);
}

TEST_F(LimitsTest, Backlash_WriteSendsBothAxes)
{
    m_limits.initProperties();
    m_fw.setResponse(":$BR200", "1");
    m_fw.setResponse(":$BD80",  "1");

    double values[] = { 200.0, 80.0 };
    const char *names[] = { "MOUNT_BACKLASH_AXIS1", "MOUNT_BACKLASH_AXIS2" };
    EXPECT_TRUE(m_limits.handleNumber("OSX_MOUNT_BACKLASH", values, const_cast<char **>(names), 2));

    EXPECT_TRUE(m_fw.received(":$BR200"));
    EXPECT_TRUE(m_fw.received(":$BD80"));
    EXPECT_EQ(IPS_OK, m_limits.backlashNP().getState());
}

TEST_F(LimitsTest, Backlash_NackSetsAlertState)
{
    m_limits.initProperties();

    double values[] = { 200.0, 80.0 };
    const char *names[] = { "MOUNT_BACKLASH_AXIS1", "MOUNT_BACKLASH_AXIS2" };
    m_limits.handleNumber("OSX_MOUNT_BACKLASH", values, const_cast<char **>(names), 2);   // bare '0'

    EXPECT_EQ(IPS_ALERT, m_limits.backlashNP().getState());
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------
int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
