/*
    OnStepX INDI Driver — test_onstepxusbplugin.cpp
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
    OnStepX INDI Driver — unit tests: USB Plugin helper

    Tests cover:
      - makePropBase sanitization (label -> INDI property name)
      - discoverAndDefine: only ports in portMask are probed
      - handleSwitch: :SUX[n],V[v]# and refusals
      - pollStatus: :GUX[n]# reply parsing
*/

#include <defaultdevice.h>

#include <gtest/gtest.h>

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#define private public
#include "OnStepXUsbPlugin.h"
#undef private

#include "OnStepXComm.h"
#include "MockFirmware.h"
#include "SavedConfig.h"

// ---------------------------------------------------------------------------
// Expose private helpers via a test subclass
// ---------------------------------------------------------------------------
class TestableUsb : public OnStepXUsbPlugin
{
    public:
        static std::string propBase(const char *label, int idx)
        {
            char buf[64];
            makePropBase(label, idx, buf, sizeof(buf));
            return std::string(buf);
        }
};

// ---------------------------------------------------------------------------
// 1. makePropBase sanitization
// ---------------------------------------------------------------------------
TEST(UsbPlugin, PropBase_Simple)
{
    EXPECT_EQ(TestableUsb::propBase("MainCam", 1), "OSX_MAINCAM");
}

TEST(UsbPlugin, PropBase_AlreadyUpper)
{
    EXPECT_EQ(TestableUsb::propBase("MAINCAM", 2), "OSX_MAINCAM");
}

TEST(UsbPlugin, PropBase_EmptyLabel)
{
    EXPECT_EQ(TestableUsb::propBase("", 3), "OSX_USB3");
}

TEST(UsbPlugin, PropBase_TrailingSpaces)
{
    EXPECT_EQ(TestableUsb::propBase("GUIDE  ", 4), "OSX_GUIDE");
}

TEST(UsbPlugin, PropBase_SpecialChars)
{
    EXPECT_EQ(TestableUsb::propBase("CAM-1", 5), "OSX_CAM_1");
}

// ---------------------------------------------------------------------------
// 2. Discovery, writes and polling
// ---------------------------------------------------------------------------
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

class UsbTest : public ::testing::Test
{
    protected:
        void SetUp() override
        {
            m_comm.setFd(m_fw.start());
            m_usb.setComm(&m_comm);
            m_usb.setDevice(&m_dev);
        }

        void TearDown() override
        {
            m_fw.stop();
        }

        void discoverPorts1And3()
        {
            m_fw.setResponse(":GUY1", "Cam#");
            m_fw.setResponse(":GUY3", "Guide#");
            m_usb.discoverAndDefine(0x05);
        }

        bool press(const char *property, const char *element)
        {
            ISState states[1] = { ISS_ON };
            char *names[1] = { const_cast<char *>(element) };
            return m_usb.handleSwitch(property, states, names, 1);
        }

        MockFirmware     m_fw;
        OnStepXComm      m_comm;
        TestDevice       m_dev;
        OnStepXUsbPlugin m_usb;
};

TEST_F(UsbTest, DiscoveryProbesOnlyPortsInMask)
{
    discoverPorts1And3();

    EXPECT_EQ(m_fw.commands(), (std::vector<std::string> { ":GUY1", ":GUY3" }));
    EXPECT_TRUE(m_usb.m_ports[0].active);
    EXPECT_FALSE(m_usb.m_ports[1].active);
    EXPECT_TRUE(m_usb.m_ports[2].active);
}

TEST_F(UsbTest, SwitchOnAddressesItsPort)
{
    discoverPorts1And3();
    m_fw.setResponse(":SUX3,V1", "1");

    EXPECT_TRUE(press("OSX_GUIDE_SW", "ON"));

    EXPECT_TRUE(m_fw.received(":SUX3,V1"));
    EXPECT_EQ(m_usb.m_ports[2].switchSP.getState(), IPS_OK);
}

TEST_F(UsbTest, RefusedSwitchRaisesAlert)
{
    discoverPorts1And3();

    EXPECT_TRUE(press("OSX_CAM_SW", "OFF"));       // unscripted: bare '0'

    EXPECT_EQ(m_usb.m_ports[0].switchSP.getState(), IPS_ALERT);
}

TEST_F(UsbTest, PollReadsOnlyActivePorts)
{
    discoverPorts1And3();
    m_fw.setResponse(":GUX1", "0#");
    m_fw.setResponse(":GUX3", "1#");

    m_usb.pollStatus();

    EXPECT_EQ(m_usb.m_ports[0].switchSP[0].getState(), ISS_ON);
    EXPECT_EQ(m_usb.m_ports[2].switchSP[1].getState(), ISS_ON);
    EXPECT_FALSE(m_fw.received(":GUX2"));
}

// Plugin builds that forget numericReply=false answer a bare '1' to :GUX.
TEST_F(UsbTest, PollKeepsStateOnBareReply)
{
    discoverPorts1And3();
    m_fw.setResponse(":GUX1", "1#");
    m_fw.setResponse(":GUX3", "1#");
    m_usb.pollStatus();

    m_fw.setResponse(":GUX1", "0");
    m_usb.pollStatus();

    EXPECT_EQ(m_usb.m_ports[0].switchSP[1].getState(), ISS_ON);
}

int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

// The USB plugin has no access to the firmware's NV: its port states are
// kept in the INDI config instead.
TEST_F(UsbTest, PortStatesSaved)
{
    discoverPorts1And3();
    const std::string cfg = savedConfig([&](FILE * fp)
    {
        m_usb.saveConfig(fp);
    });
    EXPECT_TRUE(savesProperty(cfg, "OSX_CAM_SW")) << cfg;
    EXPECT_TRUE(savesProperty(cfg, "OSX_GUIDE_SW")) << cfg;
}
