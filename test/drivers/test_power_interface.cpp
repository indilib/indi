/*
    Tests for the LED switch (LED_CONTROL) handled by INDI::PowerInterface.

    LED_CONTROL is a one-of-many switch with INDI_ENABLED ("On") and INDI_DISABLED ("Off"). Clients such as Ekos send
    the whole vector, e.g. {INDI_ENABLED: Off, INDI_DISABLED: On}, while indi_setprop usually sends only the element that
    is On. The interface must switch the LEDs by the element that is On, not by the first element name it receives.

    The mock power box below plays the driver: it records every SetLEDEnabled() request the interface makes.

    SPDX-License-Identifier: LGPL-2.1-or-later
*/

#include <gtest/gtest.h>

#include <cstdlib>
#include <string>
#include <unistd.h>
#include <vector>

#include "defaultdevice.h"
#include "indipowerinterface.h"
#include "indilogger.h"

namespace
{

const char *DEVICE_NAME = "Mock Power Box";

class MockPowerBox : public INDI::DefaultDevice, public INDI::PowerInterface
{
    public:
        MockPowerBox() : INDI::PowerInterface(this)
        {
            setDeviceName(DEVICE_NAME);
        }

        const char *getDefaultName() override
        {
            return DEVICE_NAME;
        }

        bool initProperties() override
        {
            INDI::DefaultDevice::initProperties();
            SetCapability(POWER_HAS_LED_TOGGLE);
            INDI::PowerInterface::initProperties("Power");
            return true;
        }

        bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n) override
        {
            if (INDI::PowerInterface::processSwitch(dev, name, states, names, n))
                return true;
            return INDI::DefaultDevice::ISNewSwitch(dev, name, states, names, n);
        }

        // What the "hardware" answers.
        bool ledResult {true};

        // SetLEDEnabled() requests, in order.
        std::vector<bool> ledCalls;

        // Put the LED property into a known state without going through the client path.
        void setLEDProperty(bool enabled)
        {
            LEDControlSP.reset();
            LEDControlSP[enabled ? INDI_ENABLED : INDI_DISABLED].setState(ISS_ON);
            LEDControlSP.setState(IPS_IDLE);
        }

        ISState ledEnabledState() const
        {
            return LEDControlSP[INDI_ENABLED].getState();
        }

        ISState ledDisabledState() const
        {
            return LEDControlSP[INDI_DISABLED].getState();
        }

        IPState ledPropertyState() const
        {
            return LEDControlSP.getState();
        }

        // Client request with the given elements, in the given order.
        void sendLED(std::vector<std::pair<const char *, ISState>> elements)
        {
            std::vector<ISState> states;
            std::vector<char *> names;
            for (const auto &element : elements)
            {
                names.push_back(const_cast<char *>(element.first));
                states.push_back(element.second);
            }
            ISNewSwitch(DEVICE_NAME, "LED_CONTROL", states.data(), names.data(), static_cast<int>(elements.size()));
        }

    protected:
        bool SetLEDEnabled(bool enabled) override
        {
            ledCalls.push_back(enabled);
            return ledResult;
        }
};

class PowerInterfaceLEDTest : public ::testing::Test
{
    protected:
        void SetUp() override
        {
            box.initProperties();
        }

        void expectLEDProperty(bool enabled, IPState state)
        {
            EXPECT_EQ(box.ledEnabledState(), enabled ? ISS_ON : ISS_OFF);
            EXPECT_EQ(box.ledDisabledState(), enabled ? ISS_OFF : ISS_ON);
            EXPECT_EQ(box.ledPropertyState(), state);
        }

        MockPowerBox box;
};

}

// Ekos and other clients send the whole vector, INDI_ENABLED first.
TEST_F(PowerInterfaceLEDTest, fullVectorOffSwitchesLEDOff)
{
    box.setLEDProperty(true);
    box.sendLED({{"INDI_ENABLED", ISS_OFF}, {"INDI_DISABLED", ISS_ON}});
    EXPECT_EQ(box.ledCalls, (std::vector<bool> {false}));
    expectLEDProperty(false, IPS_OK);
}

TEST_F(PowerInterfaceLEDTest, fullVectorOnSwitchesLEDOn)
{
    box.setLEDProperty(false);
    box.sendLED({{"INDI_ENABLED", ISS_ON}, {"INDI_DISABLED", ISS_OFF}});
    EXPECT_EQ(box.ledCalls, (std::vector<bool> {true}));
    expectLEDProperty(true, IPS_OK);
}

TEST_F(PowerInterfaceLEDTest, fullVectorOffInOtherOrderSwitchesLEDOff)
{
    box.setLEDProperty(true);
    box.sendLED({{"INDI_DISABLED", ISS_ON}, {"INDI_ENABLED", ISS_OFF}});
    EXPECT_EQ(box.ledCalls, (std::vector<bool> {false}));
    expectLEDProperty(false, IPS_OK);
}

// indi_setprop ... LED_CONTROL.INDI_DISABLED=On sends only the element that is On.
TEST_F(PowerInterfaceLEDTest, singleDisabledOnSwitchesLEDOff)
{
    box.setLEDProperty(true);
    box.sendLED({{"INDI_DISABLED", ISS_ON}});
    EXPECT_EQ(box.ledCalls, (std::vector<bool> {false}));
    expectLEDProperty(false, IPS_OK);
}

TEST_F(PowerInterfaceLEDTest, singleEnabledOnSwitchesLEDOn)
{
    box.setLEDProperty(false);
    box.sendLED({{"INDI_ENABLED", ISS_ON}});
    EXPECT_EQ(box.ledCalls, (std::vector<bool> {true}));
    expectLEDProperty(true, IPS_OK);
}

// The LED already has the requested state: nothing is sent to the device.
TEST_F(PowerInterfaceLEDTest, unchangedStateSendsNothing)
{
    box.setLEDProperty(false);
    box.sendLED({{"INDI_ENABLED", ISS_OFF}, {"INDI_DISABLED", ISS_ON}});
    EXPECT_TRUE(box.ledCalls.empty());
    expectLEDProperty(false, IPS_OK);
}

// No element On is not a valid request for a one-of-many switch: nothing is sent, the property keeps its state.
TEST_F(PowerInterfaceLEDTest, noElementOnSendsNothing)
{
    box.setLEDProperty(true);
    box.sendLED({{"INDI_ENABLED", ISS_OFF}});
    EXPECT_TRUE(box.ledCalls.empty());
    expectLEDProperty(true, IPS_ALERT);
}

// The device rejects the request: the property keeps its state and reports the failure.
TEST_F(PowerInterfaceLEDTest, deviceFailureKeepsState)
{
    box.setLEDProperty(true);
    box.ledResult = false;
    box.sendLED({{"INDI_ENABLED", ISS_OFF}, {"INDI_DISABLED", ISS_ON}});
    EXPECT_EQ(box.ledCalls, (std::vector<bool> {false}));
    expectLEDProperty(true, IPS_ALERT);
}

int main(int argc, char **argv)
{
    // Never read or write the user's real ~/.indi configuration.
    const std::string home = "/tmp/test_power_interface_" + std::to_string(getpid());
    const std::string configFile = home + "/config.xml";
    std::string mkdir = "mkdir -p '" + home + "/.indi'";
    if (std::system(mkdir.c_str()) != 0)
        return 1;
    setenv("HOME", home.c_str(), 1);
    setenv("INDICONFIG", configFile.c_str(), 1);

    INDI::Logger::getInstance().configure("", INDI::Logger::file_off,
                                          INDI::Logger::DBG_ERROR, INDI::Logger::DBG_ERROR);

    ::testing::InitGoogleTest(&argc, argv);
    int rc = RUN_ALL_TESTS();

    std::string cleanup = "rm -rf '" + home + "'";
    if (std::system(cleanup.c_str()) != 0)
        rc = rc ? rc : 1;
    return rc;
}
