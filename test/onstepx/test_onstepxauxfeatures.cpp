/*
    OnStepX INDI Driver — test_onstepxauxfeatures.cpp
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
    OnStepX INDI Driver — unit tests: Auxiliary Features helper

    Replies are scripted from the firmware's Features.command.cpp.

    Tests cover:
      - makePropBase sanitization (label -> INDI property name)
      - parseType: all type characters mapped correctly
      - discoverAndDefine: only slots in featureMask are probed
      - handleSwitch / handleNumber: the write commands sent, and refusals
      - pollStatus: :GXX[n]# reply parsing for each feature type
*/

#include <defaultdevice.h>

#include <gtest/gtest.h>

#include <algorithm>

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#define private public
#include "OnStepXAuxFeatures.h"
#undef private

#include "OnStepXComm.h"
#include "MockFirmware.h"
#include "SavedConfig.h"

// ---------------------------------------------------------------------------
// Expose private helpers via a test subclass
// ---------------------------------------------------------------------------
class TestableAux : public OnStepXAuxFeatures
{
    public:
        static std::string propBase(const char *label, int idx)
        {
            char buf[64];
            makePropBase(label, idx, buf, sizeof(buf));
            return std::string(buf);
        }

        // Expose parseType via a public wrapper
        static int typeOf(char t)
        {
            return static_cast<int>(parseType(t));
        }
};

// ---------------------------------------------------------------------------
// 1. makePropBase sanitization
// ---------------------------------------------------------------------------
TEST(AuxFeatures, PropBase_Simple)
{
    EXPECT_EQ(TestableAux::propBase("Dew A", 1), "OSX_DEW_A");
}

TEST(AuxFeatures, PropBase_AlreadyUpper)
{
    EXPECT_EQ(TestableAux::propBase("SWITCH", 2), "OSX_SWITCH");
}

TEST(AuxFeatures, PropBase_EmptyLabel)
{
    EXPECT_EQ(TestableAux::propBase("", 3), "OSX_AUX3");
}

TEST(AuxFeatures, PropBase_TrailingSpaces)
{
    EXPECT_EQ(TestableAux::propBase("DEW  ", 4), "OSX_DEW");
}

TEST(AuxFeatures, PropBase_SpecialChars)
{
    EXPECT_EQ(TestableAux::propBase("Dew-1", 5), "OSX_DEW_1");
}

// ---------------------------------------------------------------------------
// 2. parseType
// ---------------------------------------------------------------------------
TEST(AuxFeatures, ParseType_Switch)
{
    // '1' and 'S'/'s' -> SWITCH (1)
    EXPECT_EQ(TestableAux::typeOf('1'), 0);
    EXPECT_EQ(TestableAux::typeOf('S'), 0);
    EXPECT_EQ(TestableAux::typeOf('s'), 0);
}

TEST(AuxFeatures, ParseType_Analog)
{
    EXPECT_EQ(TestableAux::typeOf('2'), 1);
    EXPECT_EQ(TestableAux::typeOf('A'), 1);
    EXPECT_EQ(TestableAux::typeOf('a'), 1);
}

TEST(AuxFeatures, ParseType_DewHeater)
{
    EXPECT_EQ(TestableAux::typeOf('3'), 2);
    EXPECT_EQ(TestableAux::typeOf('D'), 2);
    EXPECT_EQ(TestableAux::typeOf('d'), 2);
}

TEST(AuxFeatures, ParseType_Intervalometer)
{
    EXPECT_EQ(TestableAux::typeOf('4'), 3);
    EXPECT_EQ(TestableAux::typeOf('I'), 3);
    EXPECT_EQ(TestableAux::typeOf('i'), 3);
}

TEST(AuxFeatures, ParseType_Unknown)
{
    EXPECT_EQ(TestableAux::typeOf('X'), 4);   // UNKNOWN
}

// ---------------------------------------------------------------------------
// 3. Discovery, writes and polling against the firmware's replies
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

class AuxTest : public ::testing::Test
{
    protected:
        void SetUp() override
        {
            m_comm.setFd(m_fw.start());
            m_aux.setComm(&m_comm);
            m_aux.setDevice(&m_dev);
        }

        void TearDown() override
        {
            m_fw.stop();
        }

        // Slot 1 Switch, 2 Analog, 3 Dew heater, 4 Intervalometer, 5 Switch
        // (5 has the same name as 1).
        void discoverAll()
        {
            m_fw.setResponse(":GXY1", "Switch,1#");
            m_fw.setResponse(":GXY2", "Analog,2#");
            m_fw.setResponse(":GXY3", "Dew Heat,3#");
            m_fw.setResponse(":GXY4", "Interval,4#");
            m_fw.setResponse(":GXY5", "Switch,1#");
            m_aux.discoverAndDefine(0x1F);
        }

        bool press(const char *property, const char *element)
        {
            ISState states[1] = { ISS_ON };
            char *names[1] = { const_cast<char *>(element) };
            return m_aux.handleSwitch(property, states, names, 1);
        }

        MockFirmware       m_fw;
        OnStepXComm        m_comm;
        TestDevice         m_dev;
        OnStepXAuxFeatures m_aux;
};

TEST_F(AuxTest, DiscoveryProbesOnlySlotsInMask)
{
    m_fw.setResponse(":GXY1", "Switch,1#");
    m_fw.setResponse(":GXY3", "Dew Heat,3#");

    m_aux.discoverAndDefine(0x05);

    EXPECT_EQ(m_fw.commands(), (std::vector<std::string> { ":GXY1", ":GXY3" }));
    EXPECT_TRUE(m_aux.m_slots[0].active);
    EXPECT_FALSE(m_aux.m_slots[1].active);
    EXPECT_TRUE(m_aux.m_slots[2].active);
}

// Firmware purpose 6 (HIDDEN_SWITCH) only sets a pin at boot; :GXX6# answers
// a bare '0' (Features.command.cpp), so the slot must be neither shown nor polled.
TEST_F(AuxTest, HiddenSwitchIsSkipped)
{
    m_fw.setResponse(":GXY1", "Switch,1#");
    m_fw.setResponse(":GXY6", "HIDDEN,6#");
    m_aux.discoverAndDefine(0x21);
    EXPECT_TRUE(m_aux.m_slots[0].active);
    EXPECT_FALSE(m_aux.m_slots[5].active);

    m_fw.setResponse(":GXX1", "1#");
    m_aux.pollStatus();
    const auto cmds = m_fw.commands();
    EXPECT_EQ(std::count(cmds.begin(), cmds.end(), std::string(":GXX6")), 0) << "hidden slot polled";
    EXPECT_EQ(std::count(cmds.begin(), cmds.end(), std::string(":GXX1")), 1);
}

TEST_F(AuxTest, SwitchWriteSendsValue)
{
    discoverAll();
    m_fw.setResponse(":SXX1,V1.000", "1");

    EXPECT_TRUE(press("OSX_SWITCH_SW", "ON"));

    EXPECT_TRUE(m_fw.received(":SXX1,V1.000"));
    EXPECT_EQ(m_aux.m_slots[0].switchSP.getState(), IPS_OK);
}

TEST_F(AuxTest, DuplicateNameAddressesItsOwnSlot)
{
    discoverAll();
    m_fw.setResponse(":SXX5,V1.000", "1");

    EXPECT_TRUE(press("OSX_SWITCH_5_SW", "ON"));

    EXPECT_TRUE(m_fw.received(":SXX5,V1.000"));
    EXPECT_FALSE(m_fw.received(":SXX1,V1.000"));
}

TEST_F(AuxTest, RefusedWriteRaisesAlert)
{
    discoverAll();

    EXPECT_TRUE(press("OSX_SWITCH_SW", "ON"));     // unscripted: bare '0'

    EXPECT_EQ(m_aux.m_slots[0].switchSP.getState(), IPS_ALERT);
}

TEST_F(AuxTest, AnalogWriteSendsValue)
{
    discoverAll();
    m_fw.setResponse(":SXX2,V64.000", "1");

    double values[] = { 64.0 };
    const char *names[] = { "VALUE" };
    EXPECT_TRUE(m_aux.handleNumber("OSX_ANALOG_AN", values, const_cast<char **>(names), 1));

    EXPECT_TRUE(m_fw.received(":SXX2,V64.000"));
    EXPECT_EQ(m_aux.m_slots[1].analogNP.getState(), IPS_OK);
}

TEST_F(AuxTest, DewHeaterWriteSendsZeroAndSpan)
{
    discoverAll();
    m_fw.setResponse(":SXX3,Z1.000", "1");
    m_fw.setResponse(":SXX3,S4.000", "1");

    double values[] = { 1.0, 4.0 };
    const char *names[] = { "ZERO", "SPAN" };
    EXPECT_TRUE(m_aux.handleNumber("OSX_DEW_HEAT_NP", values, const_cast<char **>(names), 2));

    EXPECT_TRUE(m_fw.received(":SXX3,Z1.000"));
    EXPECT_TRUE(m_fw.received(":SXX3,S4.000"));
    EXPECT_EQ(m_aux.m_slots[2].dewNP.getState(), IPS_OK);
}

TEST_F(AuxTest, IntervalometerWriteSendsAllFields)
{
    discoverAll();
    m_fw.setResponse(":SXX4,E3.000", "1");
    m_fw.setResponse(":SXX4,D5.000", "1");
    m_fw.setResponse(":SXX4,C7.000", "1");

    double values[] = { 3.0, 5.0, 7.0 };
    const char *names[] = { "EXPOSURE", "DELAY", "COUNT" };
    EXPECT_TRUE(m_aux.handleNumber("OSX_INTERVAL_NP", values, const_cast<char **>(names), 3));

    EXPECT_EQ(m_aux.m_slots[3].ivoNP.getState(), IPS_OK);
}

TEST_F(AuxTest, UnknownPropertyNotHandled)
{
    discoverAll();
    EXPECT_FALSE(press("SOME_OTHER_PROP", "ON"));
}

TEST_F(AuxTest, PollParsesEachFeatureType)
{
    discoverAll();
    m_fw.setResponse(":GXX1", "1#");
    m_fw.setResponse(":GXX2", "64#");
    m_fw.setResponse(":GXX3", "1,1.0,4.0,nan#");
    m_fw.setResponse(":GXX4", "2,3.0,5.0,7#");
    m_fw.setResponse(":GXX5", "0#");

    m_aux.pollStatus();

    EXPECT_EQ(m_aux.m_slots[0].switchSP[1].getState(), ISS_ON);
    EXPECT_DOUBLE_EQ(m_aux.m_slots[1].analogNP[0].getValue(), 64.0);
    EXPECT_EQ(m_aux.m_slots[2].dewEnSP[1].getState(), ISS_ON);
    EXPECT_DOUBLE_EQ(m_aux.m_slots[2].dewNP[1].getValue(), 4.0);
    EXPECT_EQ(m_aux.m_slots[2].dewStatusNP.getState(), IPS_IDLE) << "nan: no temperature sensor";
    EXPECT_DOUBLE_EQ(m_aux.m_slots[3].ivoNP[2].getValue(), 7.0);
    EXPECT_DOUBLE_EQ(m_aux.m_slots[3].ivoStatusNP[0].getValue(), 2.0);
    EXPECT_EQ(m_aux.m_slots[4].switchSP[0].getState(), ISS_ON);
}

TEST_F(AuxTest, PollIgnoresPowerMonitorSuffix)
{
    discoverAll();
    m_fw.setResponse(":GXX2", "128;12.1,0.5,0#");

    m_aux.pollStatus();

    EXPECT_DOUBLE_EQ(m_aux.m_slots[1].analogNP[0].getValue(), 128.0);
}

int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

// Feature values, dew zero/span and intervalometer settings are in the
// firmware's NV: a config load would overwrite hand controller changes.
TEST_F(AuxTest, SavesNothingToTheConfig)
{
    EXPECT_EQ(savedConfig([&](FILE * fp)
    {
        m_aux.saveConfig(fp);
    }), "");
}
