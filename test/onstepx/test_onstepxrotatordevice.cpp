/*
    OnStepX INDI Driver — test_onstepxrotatordevice.cpp
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
    OnStepX INDI Driver — Rotator child device unit tests

    The rotator is its own INDI device ("<parent> Rotator") so Ekos can pick
    it for an optical train by name.  Replies are scripted from the
    firmware's Rotator.command.cpp (:rG# "sDDD*MM#", :rT# "S[D][R]<rate>#",
    :rb# steps, :rC#/:rQ#/:rZ#/:rF# reply nothing).
*/

#include <gtest/gtest.h>

#include <cstdio>
#include <memory>
#include <string>

#define private public
#include "OnStepXRotatorDevice.h"
#undef private
#include "OnStepXComm.h"
#include "MockFirmware.h"

namespace
{
class RotatorDeviceTest : public ::testing::Test
{
    protected:
        void SetUp() override
        {
            m_comm.setFd(m_fw.start());
            m_fw.setResponse(":rI", "-180#");
            m_fw.setResponse(":rM", "180#");
            m_fw.setResponse(":rG", "+045*30#");
            m_fw.setResponse(":rT", "S3#");
            m_fw.setResponse(":rb", "25#");
        }

        void TearDown() override
        {
            OnStepXRotatorDevice::disconnectChild(m_child);
            m_fw.stop();
        }

        // connectChild() defers the first updateProperties() to the INDI
        // event loop, which tests do not run; call it directly.
        void connect(bool hasDerotator)
        {
            OnStepXRotatorDevice::connectChild(m_child, true, hasDerotator, &m_comm, "OnStepX");
            ASSERT_TRUE(m_child);
            m_child->updateProperties();
        }

        bool defined(const char *name)
        {
            return m_child->getProperty(name).isValid();
        }

        INDI::PropertyNumber number(const char *name)
        {
            return m_child->getNumber(name);
        }

        MockFirmware m_fw;
        OnStepXComm  m_comm;
        std::unique_ptr<OnStepXRotatorDevice> m_child;
};
}

TEST_F(RotatorDeviceTest, NoChildWithoutRotator)
{
    OnStepXRotatorDevice::connectChild(m_child, false, false, &m_comm, "OnStepX");
    EXPECT_FALSE(m_child);
}

TEST_F(RotatorDeviceTest, NamedAfterParentAndIsARotator)
{
    connect(false);
    EXPECT_STREQ(m_child->getDeviceName(), "OnStepX Rotator");
    EXPECT_TRUE(m_child->getDriverInterface() & INDI::BaseDevice::ROTATOR_INTERFACE);
    EXPECT_TRUE(m_child->isConnected());

    OnStepXRotatorDevice aux("OnStepX Controller");
    EXPECT_STREQ(aux.getDeviceName(), "OnStepX Controller Rotator");
}

// Without RI::SetCapability these are never defined.
TEST_F(RotatorDeviceTest, StandardRotatorPropertiesDefined)
{
    connect(false);
    for (const char *p : { "ABS_ROTATOR_ANGLE", "ROTATOR_ABORT_MOTION", "ROTATOR_HOME", "SYNC_ROTATOR_ANGLE",
                "ROTATOR_BACKLASH_STEPS", "ROTATOR_BACKLASH_TOGGLE", "ROTATOR_LIMITS"
            })
        EXPECT_TRUE(defined(p)) << p;
    EXPECT_FALSE(defined("DEVICE_PORT")) << "no connection plugin: the parent's port is shared";
}

TEST_F(RotatorDeviceTest, InitialAngleAndBacklashRead)
{
    connect(false);
    EXPECT_NEAR(number("ABS_ROTATOR_ANGLE")[0].getValue(), 45.5, 0.01);
    EXPECT_DOUBLE_EQ(number("ROTATOR_BACKLASH_STEPS")[0].getValue(), 25.0);
    EXPECT_EQ(m_child->getSwitch("ROTATOR_BACKLASH_TOGGLE").findOnSwitchIndex(),
              INDI::DefaultDevice::INDI_ENABLED);
}

TEST_F(RotatorDeviceTest, DerotatorOnlyWhenCapable)
{
    connect(false);
    EXPECT_FALSE(defined("OSX_ROT_DEROTATE"));
    OnStepXRotatorDevice::disconnectChild(m_child);

    connect(true);
    EXPECT_TRUE(defined("OSX_ROT_DEROTATE"));
    EXPECT_TRUE(defined("OSX_ROT_PARALLACTIC"));
    EXPECT_TRUE(defined("OSX_ROT_DEROTATE_REVERSE"));
    EXPECT_STREQ(m_child->getProperty("OSX_ROT_DEROTATE").getGroupName(), "Main Control");
}

TEST_F(RotatorDeviceTest, PollUpdatesAngleAndMotion)
{
    connect(false);
    m_fw.setResponse(":rG", "-090*00#");
    m_fw.setResponse(":rT", "M3#");
    m_child->pollStatus();
    EXPECT_NEAR(number("ABS_ROTATOR_ANGLE")[0].getValue(), 270.0, 0.01);
    EXPECT_EQ(number("ABS_ROTATOR_ANGLE").getState(), IPS_BUSY);
}

// Regression (hardware, 2026-11-11): ROTATOR_HOME stayed Busy after the
// rotator reached half travel.
TEST_F(RotatorDeviceTest, HomeEndsWhenTheRotatorStops)
{
    connect(false);
    m_fw.setResponse(":rC", "");          // no reply
    m_fw.setResponse(":rT", "M3#");
    ISState on[] = { ISS_ON };
    char *names[] = { const_cast<char *>("HOME") };
    ASSERT_TRUE(m_child->ISNewSwitch(m_child->getDeviceName(), "ROTATOR_HOME", on, names, 1));
    EXPECT_TRUE(m_fw.waitFor(":rC"));
    m_child->pollStatus();
    EXPECT_EQ(m_child->getSwitch("ROTATOR_HOME").getState(), IPS_BUSY);
    m_fw.setResponse(":rT", "S3#");
    m_child->pollStatus();
    EXPECT_EQ(m_child->getSwitch("ROTATOR_HOME").getState(), IPS_OK);
    EXPECT_EQ(m_child->getSwitch("ROTATOR_HOME")[0].getState(), ISS_OFF);
}

TEST_F(RotatorDeviceTest, NoPollWhileDisconnected)
{
    connect(false);
    OnStepXRotatorDevice::disconnectChild(m_child);
    EXPECT_FALSE(m_child->isConnected());
    const size_t before = m_fw.commands().size();
    m_child->pollStatus();
    EXPECT_EQ(m_fw.commands().size(), before);
}

TEST_F(RotatorDeviceTest, ReconnectReusesChild)
{
    connect(false);
    OnStepXRotatorDevice *first = m_child.get();
    OnStepXRotatorDevice::disconnectChild(m_child);
    connect(false);
    EXPECT_EQ(m_child.get(), first);
    EXPECT_TRUE(m_child->isConnected());
}

// The firmware keeps the backlash in NV; a config reload would overwrite it.
TEST_F(RotatorDeviceTest, ConfigLeavesOutBacklash)
{
    connect(false);
    char *buf = nullptr;
    size_t len = 0;
    FILE *fp = open_memstream(&buf, &len);
    ASSERT_NE(fp, nullptr);
    m_child->saveConfigItems(fp);
    fclose(fp);
    const std::string cfg(buf, len);
    free(buf);
    EXPECT_EQ(cfg.find("ROTATOR_BACKLASH"), std::string::npos) << cfg;
    EXPECT_NE(cfg.find("ROTATOR_LIMITS"), std::string::npos) << cfg;
}

TEST_F(RotatorDeviceTest, GotoSendsAngle)
{
    connect(false);
    m_fw.setResponse(":rS+090:00:00", "1");
    EXPECT_EQ(m_child->MoveRotator(90.0), IPS_BUSY);
    EXPECT_TRUE(m_fw.received(":rS+090:00:00"));
}

// indidriver provides a driver main(); define the test runner explicitly.
int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
