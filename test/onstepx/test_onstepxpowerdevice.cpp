/*
    OnStepX INDI Driver — test_onstepxpowerdevice.cpp
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
    OnStepX INDI Driver — Power child device unit tests

    Replies are scripted from the firmware's Features.command.cpp (:GXY[n]#
    "name,purpose#" with MOMENTARY/COVER reported as SWITCH, :GXX[n]# values
    optionally followed by a power-monitor suffix, :SXX[n],V..# bare '1')
    and the USB plugin's Usb.cpp (:GUY[n]# "name#", :GUX[n]# "0#"/"1#",
    :SUX[n],V[v]# bare '1').  Mirrors the hardware test build: features 1-5
    are Switch, Analog, Dew Heat, Interval, Switch; USB ports 1 and 3.
*/

#include <gtest/gtest.h>

#include <cstdio>
#include <memory>
#include <string>

#define private public
#define protected public
#include "OnStepXPowerDevice.h"
#undef protected
#undef private
#include "OnStepXComm.h"
#include "MockFirmware.h"
#include "SavedConfig.h"

namespace
{
constexpr uint8_t FEATURES = 0x1F;   // slots 1-5
constexpr uint8_t USB      = 0x05;   // ports 1 and 3

class PowerDeviceTest : public ::testing::Test
{
    protected:
        void SetUp() override
        {
            m_comm.setFd(m_fw.start());
            m_fw.setResponse(":GXY1", "Main Power,1#");
            m_fw.setResponse(":GXY2", "Flat,2#");
            m_fw.setResponse(":GXY3", "Dew Heat,3#");
            m_fw.setResponse(":GXY4", "Interval,4#");
            m_fw.setResponse(":GXY5", "Camera,1#");
            m_fw.setResponse(":GUY1", "Cam#");
            m_fw.setResponse(":GUY3", "Guide#");
            m_fw.setResponse(":GXX1", "1;12.1,0.5,!!!!!#");   // power-monitor suffix
            m_fw.setResponse(":GXX2", "64#");
            m_fw.setResponse(":GXX3", "1,2.0,5.0,NAN#");
            m_fw.setResponse(":GXX4", "0,30.0,5.0,10#");
            m_fw.setResponse(":GXX5", "0#");
            m_fw.setResponse(":GUX1", "1#");
            m_fw.setResponse(":GUX3", "0#");
        }

        void TearDown() override
        {
            OnStepXPowerDevice::disconnectChild(m_child);
            m_fw.stop();
        }

        // connectChild() defers the first updateProperties() to the INDI
        // event loop, which tests do not run; call it directly.
        void connect()
        {
            OnStepXPowerDevice::connectChild(m_child, FEATURES, USB, &m_comm, "OnStepX");
            ASSERT_TRUE(m_child);
            m_child->updateProperties();
        }

        bool defined(const char *name)
        {
            return m_child->getProperty(name).isValid();
        }

        MockFirmware m_fw;
        OnStepXComm  m_comm;
        std::unique_ptr<OnStepXPowerDevice> m_child;
};
}

TEST_F(PowerDeviceTest, NoChildWithoutOutputs)
{
    OnStepXPowerDevice::connectChild(m_child, 0, 0, &m_comm, "OnStepX");
    EXPECT_FALSE(m_child);
}

TEST_F(PowerDeviceTest, NamedAfterParentAndIsAPowerDevice)
{
    connect();
    EXPECT_STREQ(m_child->getDeviceName(), "OnStepX Power");
    EXPECT_TRUE(m_child->getDriverInterface() & INDI::BaseDevice::POWER_INTERFACE);
    EXPECT_TRUE(m_child->isConnected());
    EXPECT_FALSE(defined("DEVICE_PORT")) << "no connection plugin: the parent's port is shared";
}

// Switch slots become the standard DC outputs, USB ports the standard USB
// ports, each labelled with the firmware's name.
TEST_F(PowerDeviceTest, SwitchesAndUsbPortsAreStandardChannels)
{
    connect();
    auto dc = m_child->getSwitch("POWER_CHANNELS");
    ASSERT_TRUE(dc.isValid());
    ASSERT_EQ(dc.count(), 2u);
    EXPECT_STREQ(dc[0].getLabel(), "Main Power");
    EXPECT_STREQ(dc[1].getLabel(), "Camera");
    EXPECT_STREQ(m_child->getText("POWER_LABELS")[1].getText(), "Camera");

    auto usb = m_child->getSwitch("USB_PORTS");
    ASSERT_TRUE(usb.isValid());
    ASSERT_EQ(usb.count(), 2u);
    EXPECT_STREQ(usb[0].getLabel(), "Cam");
    EXPECT_STREQ(usb[1].getLabel(), "Guide");

    EXPECT_FALSE(defined("OSX_MAIN_POWER_SW")) << "switch slots are not also custom properties";
    EXPECT_FALSE(defined("OSX_CAM_SW"));
}

TEST_F(PowerDeviceTest, PollShowsFirmwareState)
{
    connect();
    auto dc = m_child->getSwitch("POWER_CHANNELS");
    EXPECT_EQ(dc[0].getState(), ISS_ON) << "slot 1 is on (power-monitor suffix ignored)";
    EXPECT_EQ(dc[1].getState(), ISS_OFF);
    auto usb = m_child->getSwitch("USB_PORTS");
    EXPECT_EQ(usb[0].getState(), ISS_ON);
    EXPECT_EQ(usb[1].getState(), ISS_OFF);

    // Changed from the hand controller or web UI.
    m_fw.setResponse(":GXX5", "1#");
    m_fw.setResponse(":GUX1", "0#");
    m_child->pollStatus();
    EXPECT_EQ(m_child->getSwitch("POWER_CHANNELS")[1].getState(), ISS_ON);
    EXPECT_EQ(m_child->getSwitch("USB_PORTS")[0].getState(), ISS_OFF);
}

TEST_F(PowerDeviceTest, ChannelsWriteTheirSlotAndPort)
{
    connect();
    m_fw.setResponse(":SXX5,V1.000", "1");
    EXPECT_TRUE(m_child->SetPowerPort(1, true));
    EXPECT_TRUE(m_fw.received(":SXX5,V1.000")) << "DC channel 2 is feature slot 5";

    m_fw.setResponse(":SUX3,V1", "1");
    EXPECT_TRUE(m_child->SetUSBPort(1, true));
    EXPECT_TRUE(m_fw.received(":SUX3,V1")) << "USB channel 2 is plugin port 3";

    EXPECT_FALSE(m_child->SetPowerPort(2, true)) << "no third DC channel";
    EXPECT_FALSE(m_child->SetPowerPort(0, true)) << "slot 1 not scripted: bare '0' is a refusal";
}

TEST_F(PowerDeviceTest, ChannelSwitchFromClient)
{
    connect();
    m_fw.setResponse(":SXX5,V1.000", "1");
    ISState states[2] = { ISS_ON, ISS_ON };
    char *names[2] = { const_cast<char *>("POWER_CHANNEL_1"), const_cast<char *>("POWER_CHANNEL_2") };
    EXPECT_TRUE(m_child->ISNewSwitch("OnStepX Power", "POWER_CHANNELS", states, names, 2));
    EXPECT_TRUE(m_fw.received(":SXX5,V1.000"));
    EXPECT_FALSE(m_fw.received(":SXX1,V1.000")) << "channel 1 was already on";
}

// Analog, dew heater and intervalometer slots have no PowerInterface
// equivalent and stay custom properties on this device.
TEST_F(PowerDeviceTest, OtherOutputsAreCustomProperties)
{
    connect();
    EXPECT_TRUE(defined("OSX_FLAT_AN"));
    EXPECT_STREQ(m_child->getProperty("OSX_FLAT_AN").getGroupName(), "Power");
    EXPECT_TRUE(defined("OSX_DEW_HEAT_EN"));
    EXPECT_TRUE(defined("OSX_INTERVAL_NP"));
    EXPECT_FALSE(defined("DEW_CHANNELS")) << "OnStepX dew heaters have no manual duty cycle";
}

TEST_F(PowerDeviceTest, DisconnectRemovesAndReconnectReuses)
{
    connect();
    OnStepXPowerDevice *first = m_child.get();
    OnStepXPowerDevice::disconnectChild(m_child);
    EXPECT_FALSE(defined("POWER_CHANNELS"));
    EXPECT_FALSE(defined("OSX_FLAT_AN"));
    const size_t before = m_fw.commands().size();
    m_child->pollStatus();
    EXPECT_EQ(m_fw.commands().size(), before) << "no polling while disconnected";

    connect();
    EXPECT_EQ(m_child.get(), first);
    EXPECT_TRUE(defined("POWER_CHANNELS"));
    EXPECT_TRUE(defined("OSX_FLAT_AN"));
}

// Output values and settings are in the firmware's NV; the USB ports (plugin,
// no NV) and the client-side labels are what the config keeps.
TEST_F(PowerDeviceTest, ConfigKeepsUsbStatesAndLabelsOnly)
{
    connect();
    const std::string cfg = savedConfig([&](FILE * fp)
    {
        m_child->saveConfigItems(fp);
    });
    EXPECT_TRUE(savesProperty(cfg, "USB_PORTS")) << cfg;
    EXPECT_TRUE(savesProperty(cfg, "USB_LABELS")) << cfg;
    EXPECT_TRUE(savesProperty(cfg, "POWER_LABELS")) << cfg;
    for (const char *nv : { "POWER_CHANNELS", "OSX_FLAT_AN", "OSX_DEW_HEAT_EN", "OSX_DEW_HEAT_NP",
                "OSX_INTERVAL_EN", "OSX_INTERVAL_NP"
            })
        EXPECT_FALSE(savesProperty(cfg, nv)) << nv << " is NV-stored:\n" << cfg;
}

// indidriver provides a driver main(); define the test runner explicitly.
int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
