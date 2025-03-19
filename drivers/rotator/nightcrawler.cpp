/*
    NightCrawler NightCrawler Focuser & Rotator
    Copyright (C) 2017 Jasem Mutlaq (mutlaqja@ikarustech.com)

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

#include "nightcrawler.h"

#include "indicom.h"
#include "connectionplugins/connectionserial.h"

#include <cmath>
#include <cstring>
#include <memory>
#include <termios.h>

#define NIGHTCRAWLER_TIMEOUT 3
#define NIGHTCRAWLER_THRESHOLD 0.1

#define NC_25_STEPS 374920
#define NC_30_STEPS 444080
#define NC_35_STEPS 505960

#define ROTATOR_TAB "Rotator"
#define AUX_TAB "Aux"
#define SETTINGS_TAB "Settings"

// Well, it is time I name something, even if simple, after Tommy, my loyal German Shephard companion.
// By the time of writing this, he is almost 4 years old. Live long and prosper, my good boy!
// 2018-12-12 JM: Updated this driver today. Tommy passed away a couple of months ago. May he rest in peace. I miss you.
static std::unique_ptr<NightCrawler> tommyGoodBoy(new NightCrawler());

NightCrawler::NightCrawler() : RotatorInterface(this)
{
    setVersion(1, 5);

    // Can move in Absolute & Relative motions, can AbortFocuser motion, and has variable speed.
    FI::SetCapability(FOCUSER_CAN_ABS_MOVE | FOCUSER_CAN_REL_MOVE | FOCUSER_CAN_ABORT);
    RI::SetCapability(ROTATOR_CAN_ABORT | ROTATOR_CAN_HOME | ROTATOR_CAN_SYNC | ROTATOR_CAN_REVERSE);
}

bool NightCrawler::initProperties()
{
    INDI::Focuser::initProperties();

    FocusSpeedNP[0].setMin(1);
    FocusSpeedNP[0].setMax(1);
    FocusSpeedNP[0].setValue(1);

    // Focus Sync
    SyncFocusNP[0].fill("FOCUS_SYNC_OFFSET", "Ticks", "%.f", 0, 100000., 0., 0.);
    SyncFocusNP.fill(getDeviceName(), "FOCUS_SYNC", "Sync", MAIN_CONTROL_TAB, IP_RW, 0,
                     IPS_IDLE );

    // Voltage
    VoltageNP[0].fill("VALUE", "Value (v)", "%.2f", 0, 30., 1., 0.);
    VoltageNP.fill(getDeviceName(), "Voltage", "Voltage", MAIN_CONTROL_TAB, IP_RO, 0, IPS_IDLE );

    // Temperature
    TemperatureNP[0].fill("TEMPERATURE", "Value (C)", "%.2f", -100, 100., 1., 0.);
    TemperatureNP.fill(getDeviceName(), "FOCUS_TEMPERATURE", "Temperature", MAIN_CONTROL_TAB,
                       IP_RO, 0, IPS_IDLE );

    // Temperature offset
    IUFillNumber(&TemperatureOffsetN[0], "OFFSET", "Offset", "%.2f", -15, 15., 1., 0.);
    IUFillNumberVector(&TemperatureOffsetNP, TemperatureOffsetN, 1, getDeviceName(), "TEMPERATURE_OFFSET", "Temperature",
                       MAIN_CONTROL_TAB, IP_WO, 0, IPS_IDLE );

    // Motor Step Delay
    FocusStepDelayNP[0].fill("FOCUS_STEP", "Value", "%.f", 7, 100., 1., 7.);
    FocusStepDelayNP.fill(getDeviceName(), "FOCUS_STEP_DELAY", "Step Rate", SETTINGS_TAB,
                          IP_RW, 0, IPS_IDLE );

    // Limit Switch
    LimitSwitchLP[ROTATION_SWITCH].fill("ROTATION_SWITCH", "Rotation Home", IPS_OK);
    LimitSwitchLP[OUT_SWITCH].fill("OUT_SWITCH", "Focus Out Limit", IPS_OK);
    LimitSwitchLP[IN_SWITCH].fill("IN_SWITCH", "Focus In Limit", IPS_OK);
    LimitSwitchLP.fill(getDeviceName(), "LIMIT_SWITCHES", "Limit Switch", SETTINGS_TAB,
                       IPS_IDLE);

    // Home selection
    HomeSelectionSP[MOTOR_FOCUS].fill("FOCUS", "Focuser", ISS_ON);
    HomeSelectionSP[MOTOR_ROTATOR].fill( "ROTATOR", "Rotator", ISS_ON);
    HomeSelectionSP[MOTOR_AUX].fill("AUX", "Aux", ISS_OFF);
    HomeSelectionSP.fill(getDeviceName(), "HOME_SELECTION", "Home Select", SETTINGS_TAB,
                         IP_RW, ISR_NOFMANY, 0, IPS_IDLE);

    // Home Find
    FindHomeSP[0].fill("FIND", "Start", ISS_OFF);
    FindHomeSP.fill(getDeviceName(), "FIND_HOME", "Home Find", SETTINGS_TAB, IP_RW, ISR_1OFMANY,
                    0, IPS_IDLE);

    // Encoders
    EncoderSP[INDI_ENABLED].fill("INDI_ENABLED", "Enabled", ISS_ON);
    EncoderSP[INDI_DISABLED].fill("INDI_DISABLED", "Disabled", ISS_OFF);
    EncoderSP.fill(getDeviceName(), "ENCODERS", "Encoders", SETTINGS_TAB, IP_RW, ISR_1OFMANY, 0,
                   IPS_IDLE);

    // Brightness
    BrightnessNP[BRIGHTNESS_DISPLAY].fill("BRIGHTNESS_DISPLAY", "Display", "%.f", 0, 255., 10., 150.);
    BrightnessNP[BRIGHTNESS_SLEEP].fill("BRIGHTNESS_SLEEP", "Sleep", "%.f", 1, 255., 10., 16.);
    BrightnessNP.fill(getDeviceName(), "BRIGHTNESS", "Brightness", SETTINGS_TAB, IP_RW, 0,
                      IPS_IDLE );

    //////////////////////////////////////////////////////
    // Rotator Properties
    /////////////////////////////////////////////////////

    INDI::RotatorInterface::initProperties(ROTATOR_TAB);

    // Rotator Ticks
    RotatorAbsPosNP[0].fill("ROTATOR_ABSOLUTE_POSITION", "Ticks", "%.f", 0., 100000., 1000., 0.);
    RotatorAbsPosNP.fill(getDeviceName(), "ABS_ROTATOR_POSITION", "Goto", ROTATOR_TAB, IP_RW,
                         0, IPS_IDLE );

    // Rotator Step Delay
    RotatorStepDelayNP[0].fill("ROTATOR_STEP", "Value", "%.f", 7, 100., 1., 7.);
    RotatorStepDelayNP.fill(getDeviceName(), "ROTATOR_STEP_DELAY", "Step Rate",
                            ROTATOR_TAB, IP_RW, 0, IPS_IDLE );

    // For custom focuser, set max steps
    CustomRotatorStepNP[0].fill("STEPS", "Steps", "%.f", 0, 5000000, 0, 0);
    CustomRotatorStepNP.fill(getDeviceName(), "CUSTOM_STEPS", "Custom steps", ROTATOR_TAB, IP_RW, 60, IPS_IDLE);

    //////////////////////////////////////////////////////
    // Aux Properties
    /////////////////////////////////////////////////////

    // Aux GOTO
    GotoAuxNP[0].fill("AUX_ABSOLUTE_POSITION", "Ticks", "%.f", 0, 100000., 0., 0.);
    GotoAuxNP.fill(getDeviceName(), "ABS_AUX_POSITION", "Goto", AUX_TAB, IP_RW, 0, IPS_IDLE );

    // Abort Aux
    AbortAuxSP[0].fill("ABORT", "Abort", ISS_OFF);
    AbortAuxSP.fill(getDeviceName(), "AUX_ABORT_MOTION", "Abort Motion", AUX_TAB, IP_RW,
                    ISR_ATMOST1, 0, IPS_IDLE);

    // Aux Sync
    SyncAuxNP[0].fill("AUX_SYNC_TICK", "Ticks", "%.f", 0, 100000., 0., 0.);
    SyncAuxNP.fill(getDeviceName(), "SYNC_AUX", "Sync", AUX_TAB, IP_RW, 0, IPS_IDLE );

    // Aux Step Delay
    AuxStepDelayNP[0].fill( "AUX_STEP", "Value", "%.f", 7, 100., 1., 7.);
    AuxStepDelayNP.fill(getDeviceName(), "AUX_STEP_DELAY", "Step Rate", AUX_TAB, IP_RW, 0,
                        IPS_IDLE );

    /* Relative and absolte movement */
    FocusRelPosNP[0].setMin(0.);
    FocusRelPosNP[0].setMax(50000.);
    FocusRelPosNP[0].setValue(0);
    FocusRelPosNP[0].setStep(1000);

    FocusAbsPosNP[0].setMin(0.);
    FocusAbsPosNP[0].setMax(100000.);
    FocusAbsPosNP[0].setValue(0);
    FocusAbsPosNP[0].setStep(1000);

    addDebugControl();

    setDefaultPollingPeriod(500);

    setDriverInterface(getDriverInterface() | ROTATOR_INTERFACE);

    serialConnection->setDefaultBaudRate(Connection::Serial::B_57600);

    return true;
}

bool NightCrawler::updateProperties()
{
    INDI::Focuser::updateProperties();

    if (isConnected())
    {
        // Focus
        defineProperty(SyncFocusNP);
        defineProperty(VoltageNP);
        defineProperty(TemperatureNP);
        defineProperty(&TemperatureOffsetNP);
        defineProperty(FocusStepDelayNP);
        defineProperty(LimitSwitchLP);
        defineProperty(EncoderSP);
        defineProperty(BrightnessNP);
        defineProperty(HomeSelectionSP);
        defineProperty(FindHomeSP);

        // Rotator
        INDI::RotatorInterface::updateProperties();
        defineProperty(RotatorAbsPosNP);
        defineProperty(RotatorStepDelayNP);
        defineProperty(CustomRotatorStepNP);

        // Aux
        defineProperty(GotoAuxNP);
        defineProperty(AbortAuxSP);
        defineProperty(SyncAuxNP);
        defineProperty(AuxStepDelayNP);
    }
    else
    {
        // Focus
        deleteProperty(SyncFocusNP);
        deleteProperty(VoltageNP);
        deleteProperty(TemperatureNP);
        deleteProperty(TemperatureOffsetNP.name);
        deleteProperty(FocusStepDelayNP);
        deleteProperty(LimitSwitchLP);
        deleteProperty(EncoderSP);
        deleteProperty(BrightnessNP);
        deleteProperty(FindHomeSP);
        deleteProperty(HomeSelectionSP);

        // Rotator
        INDI::RotatorInterface::updateProperties();
        deleteProperty(RotatorAbsPosNP);
        deleteProperty(RotatorStepDelayNP);
        deleteProperty(CustomRotatorStepNP);

        // Aux
        deleteProperty(GotoAuxNP);
        deleteProperty(AbortAuxSP);
        deleteProperty(SyncAuxNP);
        deleteProperty(AuxStepDelayNP);
    }

    return true;
}

bool NightCrawler::Handshake()
{
    if (Ack())
        return true;

    LOG_INFO("Error retrieving data from NightCrawler, please ensure NightCrawler controller is powered and the port is correct.");
    return false;
}

const char * NightCrawler::getDefaultName()
{
    return "NightCrawler";
}

bool NightCrawler::Ack()
{
    bool rcFirmware = getFirmware();
    bool rcType = getFocuserType();

    return (rcFirmware && rcType);
}

bool NightCrawler::getFirmware()
{
    int nbytes_written = 0, nbytes_read = 0, rc = -1;
    char errstr[MAXRBUF];
    char resp[64];

    tcflush(PortFD, TCIOFLUSH);

    if ( (rc = tty_write(PortFD, "PV#", 3, &nbytes_written)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("getFirmware error: %s.", errstr);
        return false;
    }

    if ( (rc = tty_read_section(PortFD, resp, '#', NIGHTCRAWLER_TIMEOUT, &nbytes_read)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("getFirmware error: %s.", errstr);
        return false;
    }

    tcflush(PortFD, TCIOFLUSH);

    resp[nbytes_read - 1] = '\0';

    LOGF_INFO("Firmware %s", resp);

    return true;
}

bool NightCrawler::getFocuserType()
{
    int nbytes_written = 0, nbytes_read = 0, rc = -1;
    char errstr[MAXRBUF];
    char resp[64];

    tcflush(PortFD, TCIOFLUSH);

    if ( (rc = tty_write(PortFD, "PF#", 3, &nbytes_written)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("getFirmware error: %s.", errstr);
        return false;
    }

    if ( (rc = tty_read_section(PortFD, resp, '#', NIGHTCRAWLER_TIMEOUT, &nbytes_read)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("getFirmware error: %s.", errstr);
        return false;
    }

    tcflush(PortFD, TCIOFLUSH);

    resp[nbytes_read - 1] = '\0';

    LOGF_INFO("Focuser Type %s", resp);

    if (strcmp(resp, "2.5 NC") == 0)
    {
        RotatorAbsPosNP[0].setMin(-NC_25_STEPS / 2.0);
        RotatorAbsPosNP[0].setMax(NC_25_STEPS / 2.0);
        m_RotatorStepsPerRevolution = NC_25_STEPS;
    }
    else if (strcmp(resp, "3.0 NC") == 0)
    {
        RotatorAbsPosNP[0].setMin(-NC_30_STEPS / 2.0);
        RotatorAbsPosNP[0].setMax(NC_30_STEPS / 2.0);
        m_RotatorStepsPerRevolution = NC_30_STEPS;
    }
    else
    {
        RotatorAbsPosNP[0].setMin(-NC_35_STEPS / 2.0);
        RotatorAbsPosNP[0].setMax(NC_35_STEPS / 2.0);
        m_RotatorStepsPerRevolution = NC_35_STEPS;
    }

    m_RotatorTicksPerDegree = m_RotatorStepsPerRevolution / 360.0;

    return true;
}

bool NightCrawler::gotoMotor(MotorType type, int32_t position)
{
    char cmd[16] = {0};
    char res[16] = {0};

    int nbytes_written = 0, nbytes_read = 0, rc = -1;
    char errstr[MAXRBUF];

    snprintf(cmd, 16, "%dSN %d#", type + 1, position);

    LOGF_DEBUG("CMD <%s>", cmd);

    tcflush(PortFD, TCIOFLUSH);

    if ( (rc = tty_write(PortFD, cmd, strlen(cmd), &nbytes_written)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("%s: %s.", __FUNCTION__, errstr);
        return false;
    }

    if ( (rc = tty_read(PortFD, res, 1, NIGHTCRAWLER_TIMEOUT, &nbytes_read)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("%s error: %s.", __FUNCTION__, errstr);
        return false;
    }

    LOGF_DEBUG("RES <%s>", res);

    return startMotor(type);
}

bool NightCrawler::getPosition(MotorType type)
{
    char cmd[16] = {0};
    char res[16] = {0};
    int position = -1e6;

    int nbytes_written = 0, nbytes_read = 0, rc = -1;
    char errstr[MAXRBUF];

    snprintf(cmd, 16, "%dGP#", type + 1);

    LOGF_DEBUG("CMD <%s>", cmd);

    tcflush(PortFD, TCIOFLUSH);

    if ( (rc = tty_write(PortFD, cmd, strlen(cmd), &nbytes_written)) != TTY_OK)
    {
        //        tty_error_msg(rc, errstr, MAXRBUF);
        //        LOGF_ERROR("%s: %s.", __FUNCTION__, errstr);
        //        return false;
        abnormalDisconnect();
        return false;
    }

    if ( (rc = tty_read(PortFD, res, 8, NIGHTCRAWLER_TIMEOUT, &nbytes_read)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("%s error: %s.", __FUNCTION__, errstr);
        return false;
    }

    res[nbytes_read] = '\0';

    LOGF_DEBUG("RES <%s>", res);

    position = atoi(res);

    if (position != -1e6)
    {
        if (type == MOTOR_FOCUS)
            FocusAbsPosNP[0].setValue(position);
        else if (type == MOTOR_ROTATOR)
            RotatorAbsPosNP[0].setValue(position);
        else
            GotoAuxNP[0].setValue(position);

        return true;
    }

    LOGF_DEBUG("Invalid Position! %d", position);
    return false;
}

void NightCrawler::abnormalDisconnectCallback(void *userpointer)
{
    NightCrawler *p = static_cast<NightCrawler *>(userpointer);
    if (p->Connect())
    {
        p->setConnected(true, IPS_OK);
        p->updateProperties();
    }
}

void NightCrawler::abnormalDisconnect()
{
    // Ignore disconnect errors
    Disconnect();

    // Set Disconnected
    setConnected(false, IPS_IDLE);
    // Update properties
    updateProperties();

    // Reconnect in 2 seconds
    IEAddTimer(2000, abnormalDisconnectCallback, this);
}

bool NightCrawler::ISNewSwitch (const char * dev, const char * name, ISState * states, char * names[], int n)
{
    if(strcmp(dev, getDeviceName()) == 0)
    {
        if (HomeSelectionSP.isNameMatch(name))
        {
            bool atLeastOne = false;

            for (int i = 0; i < n; i++)
            {
                if (states[i] == ISS_ON)
                {
                    atLeastOne = true;
                    break;
                }
            }

            if (!atLeastOne)
            {
                HomeSelectionSP.setState(IPS_ALERT);
                LOG_ERROR("At least one selection must be on.");
                HomeSelectionSP.apply();
                return false;
            }

            HomeSelectionSP.update(states, names, n);
            HomeSelectionSP.setState(IPS_OK);
            HomeSelectionSP.apply();
            return true;
        }
        else if (FindHomeSP.isNameMatch(name))
        {
            uint8_t selection = 0;

            if (HomeSelectionSP[MOTOR_FOCUS].getState() == ISS_ON)
                selection |= 0x01;
            if (HomeSelectionSP[MOTOR_ROTATOR].getState() == ISS_ON)
                selection |= 0x02;
            if (HomeSelectionSP[MOTOR_AUX].getState() == ISS_ON)
                selection |= 0x04;

            if (findHome(selection))
            {
                FindHomeSP.setState(IPS_BUSY);
                FindHomeSP[0].setState(ISS_ON);
                LOG_WARN("Homing process can take up to 10 minutes. You cannot control the unit until the process is fully complete.");
            }
            else
            {
                FindHomeSP.setState(IPS_ALERT);
                FindHomeSP[0].setState(ISS_OFF);
                LOG_ERROR("Failed to start homing process.");
            }

            FindHomeSP.apply();
            return true;
        }
        else if (EncoderSP.isNameMatch(name))
        {
            EncoderSP.update(states, names, n);
            EncoderSP.setState(setEncodersEnabled(EncoderSP[0].getState() == ISS_ON) ? IPS_OK : IPS_ALERT);
            if (EncoderSP.getState() == IPS_OK)
                LOGF_INFO("Encoders are %s", (EncoderSP[0].getState() == ISS_ON) ? "ON" : "OFF");
            EncoderSP.apply();
            return true;
        }
        else if (AbortAuxSP.isNameMatch(name))
        {
            AbortAuxSP.setState(stopMotor(MOTOR_AUX) ? IPS_OK : IPS_ALERT);
            AbortAuxSP.apply();
            if (AbortAuxSP.getState() == IPS_OK)
            {
                if (GotoAuxNP.getState() != IPS_OK)
                {
                    GotoAuxNP.setState(IPS_OK);
                    GotoAuxNP.apply();
                }
            }
            return true;
        }
        else if (strstr(name, "ROTATOR"))
        {
            if (INDI::RotatorInterface::processSwitch(dev, name, states, names, n))
                return true;
        }
    }

    return INDI::Focuser::ISNewSwitch(dev, name, states, names, n);
}

bool NightCrawler::ISNewNumber (const char * dev, const char * name, double values[], char * names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        if (SyncFocusNP.isNameMatch(name))
        {
            bool rc = syncMotor(MOTOR_FOCUS, static_cast<uint32_t>(values[0]));
            SyncFocusNP.setState(rc ? IPS_OK : IPS_ALERT);
            if (rc)
                SyncFocusNP[0].setValue(values[0]);

            SyncFocusNP.apply();
            return true;
        }
        else if (SyncAuxNP.isNameMatch(name))
        {
            bool rc = syncMotor(MOTOR_AUX, static_cast<uint32_t>(values[0]));
            SyncAuxNP.setState(rc ? IPS_OK : IPS_ALERT);
            if (rc)
                SyncAuxNP[0].setValue(values[0]);

            SyncAuxNP.apply();
            return true;
        }
        else if (strcmp(name, TemperatureOffsetNP.name) == 0)
        {
            bool rc = setTemperatureOffset(values[0]);
            TemperatureOffsetNP.s = rc ? IPS_OK : IPS_ALERT;
            IDSetNumber(&TemperatureOffsetNP, nullptr);
            return true;
        }
        else if (FocusStepDelayNP.isNameMatch(name))
        {
            bool rc = setStepDelay(MOTOR_FOCUS, static_cast<uint32_t>(values[0]));
            FocusStepDelayNP.setState(rc ? IPS_OK : IPS_ALERT);
            if (rc)
                FocusStepDelayNP[0].setValue(values[0]);
            FocusStepDelayNP.apply();
            return true;
        }
        else if (RotatorStepDelayNP.isNameMatch(name))
        {
            bool rc = setStepDelay(MOTOR_ROTATOR, static_cast<uint32_t>(values[0]));
            RotatorStepDelayNP.setState(rc ? IPS_OK : IPS_ALERT);
            if (rc)
                RotatorStepDelayNP[0].setValue(values[0]);
            RotatorStepDelayNP.apply();
            return true;
        }
        else if (AuxStepDelayNP.isNameMatch(name))
        {
            bool rc = setStepDelay(MOTOR_AUX, static_cast<uint32_t>(values[0]));
            AuxStepDelayNP.setState(rc ? IPS_OK : IPS_ALERT);
            if (rc)
                AuxStepDelayNP[0].setValue(values[0]);
            AuxStepDelayNP.apply();
            return true;
        }
        else if (BrightnessNP.isNameMatch(name))
        {
            BrightnessNP.update(values, names, n);
            bool rcDisplay = setDisplayBrightness(static_cast<uint8_t>(BrightnessNP[BRIGHTNESS_DISPLAY].getValue()));
            bool rcSleep = setSleepBrightness(static_cast<uint8_t>(BrightnessNP[BRIGHTNESS_SLEEP].getValue()));
            if (rcDisplay && rcSleep)
                BrightnessNP.setState(IPS_OK);
            else
                BrightnessNP.setState(IPS_ALERT);

            BrightnessNP.apply();
            return true;
        }
        else if (GotoAuxNP.isNameMatch(name))
        {
            bool rc = gotoMotor(MOTOR_AUX, static_cast<int32_t>(values[0]));
            GotoAuxNP.setState(rc ? IPS_BUSY : IPS_OK);
            GotoAuxNP.apply();
            LOGF_INFO("Aux moving to %.f...", values[0]);
            return true;
        }
        else if (RotatorAbsPosNP.isNameMatch(name))
        {
            RotatorAbsPosNP.setState(gotoMotor(MOTOR_ROTATOR, static_cast<int32_t>(values[0])) ? IPS_BUSY : IPS_ALERT);
            RotatorAbsPosNP.apply();
            if (RotatorAbsPosNP.getState() == IPS_BUSY)
                LOGF_INFO("Rotator moving to %.f ticks...", values[0]);
            return true;
        }
        else if (CustomRotatorStepNP.isNameMatch(name))
        {
            CustomRotatorStepNP.update(values, names, n);
            CustomRotatorStepNP.setState(IPS_OK);
            CustomRotatorStepNP.apply();

            auto customValue = CustomRotatorStepNP[0].getValue();
            if (customValue > 0)
            {
                RotatorAbsPosNP[0].setMin(customValue / 2.0);
                RotatorAbsPosNP[0].setMax(customValue / 2.0);
                m_RotatorStepsPerRevolution = customValue;
                m_RotatorTicksPerDegree = m_RotatorStepsPerRevolution / 360.0;
                RotatorAbsPosNP.updateMinMax();

                LOGF_INFO("Custom steps per revolution updated to %.f. Ticks per degree %.2f", customValue,
                          m_RotatorTicksPerDegree);
            }
            saveConfig(CustomRotatorStepNP);
            return true;
        }
        else if (strstr(name, "ROTATOR"))
        {
            if (INDI::RotatorInterface::processNumber(dev, name, values, names, n))
                return true;
        }
    }

    return INDI::Focuser::ISNewNumber(dev, name, values, names, n);
}

IPState NightCrawler::MoveAbsFocuser(uint32_t targetTicks)
{
    targetPosition = targetTicks;

    bool rc = false;

    rc = gotoMotor(MOTOR_FOCUS, targetPosition);

    if (!rc)
        return IPS_ALERT;

    FocusAbsPosNP.setState(IPS_BUSY);

    return IPS_BUSY;
}

IPState NightCrawler::MoveRelFocuser(FocusDirection dir, uint32_t ticks)
{
    double newPosition = 0;
    bool rc = false;

    if (dir == FOCUS_INWARD)
        newPosition = FocusAbsPosNP[0].getValue() - ticks;
    else
        newPosition = FocusAbsPosNP[0].getValue() + ticks;

    rc = gotoMotor(MOTOR_FOCUS, newPosition);

    if (!rc)
        return IPS_ALERT;

    FocusRelPosNP[0].setValue(ticks);
    FocusRelPosNP.setState(IPS_BUSY);

    return IPS_BUSY;
}

void NightCrawler::TimerHit()
{
    if (!isConnected())
    {
        SetTimer(getCurrentPollingPeriod());
        return;
    }

    bool rc = false;

    // #1 If we're homing, we check if homing is complete as we cannot check for anything else
    if (FindHomeSP.getState() == IPS_BUSY || HomeRotatorSP.getState() == IPS_BUSY)
    {
        if (isHomingComplete())
        {
            HomeRotatorSP[0].setState(ISS_OFF);
            HomeRotatorSP.setState(IPS_OK);
            HomeRotatorSP.apply();

            FindHomeSP[0].setState(ISS_OFF);
            FindHomeSP.setState(IPS_OK);
            FindHomeSP.apply();

            LOG_INFO("Homing is complete.");
        }

        SetTimer(getCurrentPollingPeriod());
        return;
    }

    // #2 Get Temperature
    rc = getTemperature();
    if (rc && std::abs(TemperatureNP[0].getValue() - lastTemperature) > NIGHTCRAWLER_THRESHOLD)
    {
        lastTemperature = TemperatureNP[0].getValue();
        TemperatureNP.apply();
    }

    // #3 Get Voltage
    rc = getVoltage();
    if (rc && std::abs(VoltageNP[0].getValue() - lastVoltage) > NIGHTCRAWLER_THRESHOLD)
    {
        lastVoltage = VoltageNP[0].getValue();
        VoltageNP.apply();
    }

    // #4 Get Limit Switch Status
    rc = getLimitSwitchStatus();
    if (rc && (LimitSwitchLP[ROTATION_SWITCH].getState() != rotationLimit
               || LimitSwitchLP[OUT_SWITCH].getState() != outSwitchLimit
               || LimitSwitchLP[IN_SWITCH].getState() != inSwitchLimit))
    {
        rotationLimit = LimitSwitchLP[ROTATION_SWITCH].getState();
        outSwitchLimit = LimitSwitchLP[OUT_SWITCH].getState();
        inSwitchLimit = LimitSwitchLP[IN_SWITCH].getState();
        LimitSwitchLP.apply();
    }

    // #5 Focus Position & Status
    bool absFocusUpdated = false;

    if (FocusAbsPosNP.getState() == IPS_BUSY)
    {
        // Stopped moving
        if (!isMotorMoving(MOTOR_FOCUS))
        {
            FocusAbsPosNP.setState(IPS_OK);
            if (FocusRelPosNP.getState() != IPS_OK)
            {
                FocusRelPosNP.setState(IPS_OK);
                FocusRelPosNP.apply();
            }
            absFocusUpdated = true;
        }
    }
    rc = getPosition(MOTOR_FOCUS);
    if (rc && std::abs(FocusAbsPosNP[0].getValue() - lastFocuserPosition) > NIGHTCRAWLER_THRESHOLD)
    {
        lastFocuserPosition = FocusAbsPosNP[0].getValue();
        absFocusUpdated = true;
    }
    if (absFocusUpdated)
        FocusAbsPosNP.apply();

    // #6 Rotator Position & Status
    bool absRotatorUpdated = false;

    if (RotatorAbsPosNP.getState() == IPS_BUSY)
    {
        // Stopped moving
        if (!isMotorMoving(MOTOR_ROTATOR))
        {
            RotatorAbsPosNP.setState(IPS_OK);
            GotoRotatorNP.setState(IPS_OK);
            absRotatorUpdated = true;
            LOG_INFO("Rotator motion complete.");
        }
    }
    rc = getPosition(MOTOR_ROTATOR);
    // If absolute position is zero, we must sync to 180 degrees so we can rotate in both directions freely.
    // Also sometimes Rotator motor returns negative result, we must sync it to 180 degrees as well.
    while (RotatorAbsPosNP[0].getValue() < -m_RotatorStepsPerRevolution
            || RotatorAbsPosNP[0].getValue() > m_RotatorStepsPerRevolution)
    {
        // Update value to take care of multiple rotations.
        const auto newOffset = static_cast<int32_t>(RotatorAbsPosNP[0].getValue()) % m_RotatorStepsPerRevolution;
        LOGF_INFO("Out of bounds value detected. Syncing rotator position to %d", newOffset);
        syncMotor(MOTOR_ROTATOR, newOffset);
        rc = getPosition(MOTOR_ROTATOR);
    }
    if (rc && std::abs(RotatorAbsPosNP[0].getValue() - lastRotatorPosition) > NIGHTCRAWLER_THRESHOLD)
    {
        lastRotatorPosition = RotatorAbsPosNP[0].getValue();
        if (ReverseRotatorSP[INDI_ENABLED].getState() == ISS_ON)
            GotoRotatorNP[0].setValue(range360(360 - (RotatorAbsPosNP[0].getValue() / m_RotatorTicksPerDegree)));
        else
            GotoRotatorNP[0].setValue(range360(RotatorAbsPosNP[0].getValue() / m_RotatorTicksPerDegree));
        absRotatorUpdated = true;
    }
    if (absRotatorUpdated)
    {
        RotatorAbsPosNP.apply();
        GotoRotatorNP.apply();
    }

    // #7 Aux Position & Status
    bool absAuxUpdated = false;

    if (GotoAuxNP.getState() == IPS_BUSY)
    {
        // Stopped moving
        if (!isMotorMoving(MOTOR_AUX))
        {
            GotoAuxNP.setState(IPS_OK);
            absAuxUpdated = true;
            LOG_INFO("Aux motion complete.");
        }
    }
    rc = getPosition(MOTOR_AUX);
    if (rc && std::abs(GotoAuxNP[0].getValue() - lastAuxPosition) > NIGHTCRAWLER_THRESHOLD)
    {
        lastAuxPosition = GotoAuxNP[0].getValue();
        absAuxUpdated = true;
    }
    if (absAuxUpdated)
        GotoAuxNP.apply();

    SetTimer(getCurrentPollingPeriod());
}

bool NightCrawler::AbortFocuser()
{
    return stopMotor(MOTOR_FOCUS);
}

bool NightCrawler::syncMotor(MotorType type, uint32_t position)
{
    char cmd[16] = {0};
    char res[16] = {0};

    int nbytes_written = 0, nbytes_read = 0, rc = -1;
    char errstr[MAXRBUF];

    snprintf(cmd, 16, "%dSP %d#", type + 1, position);

    LOGF_DEBUG("CMD <%s>", cmd);

    tcflush(PortFD, TCIOFLUSH);

    if ( (rc = tty_write(PortFD, cmd, strlen(cmd), &nbytes_written)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("%s: %s.", __FUNCTION__, errstr);
        return false;
    }

    if ( (rc = tty_read(PortFD, res, 1, NIGHTCRAWLER_TIMEOUT, &nbytes_read)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("%s error: %s.", __FUNCTION__, errstr);
        return false;
    }

    res[nbytes_read] = '\0';

    LOGF_DEBUG("RES <%s>", res);

    return (res[0] == '#');
}

bool NightCrawler::startMotor(MotorType type)
{
    char cmd[16] = {0};
    char res[16] = {0};

    int nbytes_written = 0, nbytes_read = 0, rc = -1;
    char errstr[MAXRBUF];

    snprintf(cmd, 16, "%dSM#", type + 1);

    LOGF_DEBUG("CMD <%s>", cmd);

    tcflush(PortFD, TCIOFLUSH);

    if ( (rc = tty_write(PortFD, cmd, strlen(cmd), &nbytes_written)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("%s: %s.", __FUNCTION__, errstr);
        return false;
    }

    if ( (rc = tty_read(PortFD, res, 1, NIGHTCRAWLER_TIMEOUT, &nbytes_read)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("%s error: %s.", __FUNCTION__, errstr);
        return false;
    }

    res[nbytes_read] = '\0';

    LOGF_DEBUG("RES <%s>", res);

    return (res[0] == '#');
}

bool NightCrawler::stopMotor(MotorType type)
{
    char cmd[16] = {0};
    char res[16] = {0};

    int nbytes_written = 0, nbytes_read = 0, rc = -1;
    char errstr[MAXRBUF];

    snprintf(cmd, 16, "%dSQ#", type + 1);

    LOGF_DEBUG("CMD <%s>", cmd);

    tcflush(PortFD, TCIOFLUSH);

    if ( (rc = tty_write(PortFD, cmd, strlen(cmd), &nbytes_written)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("%s: %s.", __FUNCTION__, errstr);
        return false;
    }

    if ( (rc = tty_read(PortFD, res, 1, NIGHTCRAWLER_TIMEOUT, &nbytes_read)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("%s error: %s.", __FUNCTION__, errstr);
        return false;
    }

    res[nbytes_read] = '\0';

    LOGF_DEBUG("RES <%s>", res);

    return (res[0] == '#');
}

bool NightCrawler::isMotorMoving(MotorType type)
{
    char cmd[16] = {0};
    char res[16] = {0};

    int nbytes_written = 0, nbytes_read = 0, rc = -1;
    char errstr[MAXRBUF];

    snprintf(cmd, 16, "%dGM#", type + 1);

    LOGF_DEBUG("CMD <%s>", cmd);

    tcflush(PortFD, TCIOFLUSH);

    if ( (rc = tty_write(PortFD, cmd, strlen(cmd), &nbytes_written)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("%s error: %s.", __FUNCTION__, errstr);
        return false;
    }

    if ( (rc = tty_read_section(PortFD, res, '#', NIGHTCRAWLER_TIMEOUT, &nbytes_read)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("%s error: %s.", __FUNCTION__, errstr);
        return false;
    }

    res[nbytes_read - 1] = '\0';

    LOGF_DEBUG("RES <%s>", res);

    return (strcmp("01", res) == 0);
}

bool NightCrawler::getTemperature()
{
    char cmd[16] = "GT#";
    char res[16] = {0};

    int nbytes_written = 0, nbytes_read = 0, rc = -1;
    char errstr[MAXRBUF];

    LOGF_DEBUG("CMD <%s>", cmd);

    tcflush(PortFD, TCIOFLUSH);

    if ( (rc = tty_write(PortFD, cmd, strlen(cmd), &nbytes_written)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("%s error: %s.", __FUNCTION__, errstr);
        return false;
    }

    if ( (rc = tty_read_section(PortFD, res, '#', NIGHTCRAWLER_TIMEOUT, &nbytes_read)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("%s error: %s.", __FUNCTION__, errstr);
        return false;
    }

    res[nbytes_read - 1] = '\0';

    LOGF_DEBUG("RES <%s>", res);

    TemperatureNP[0].setValue(atoi(res) / 10.0);

    return true;
}

bool NightCrawler::getVoltage()
{
    char cmd[16] = "GV#";
    char res[16] = {0};

    int nbytes_written = 0, nbytes_read = 0, rc = -1;
    char errstr[MAXRBUF];

    LOGF_DEBUG("CMD <%s>", cmd);

    tcflush(PortFD, TCIOFLUSH);

    if ( (rc = tty_write(PortFD, cmd, strlen(cmd), &nbytes_written)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("%s error: %s.", __FUNCTION__, errstr);
        return false;
    }

    if ( (rc = tty_read_section(PortFD, res, '#', NIGHTCRAWLER_TIMEOUT, &nbytes_read)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("%s error: %s.", __FUNCTION__, errstr);
        return false;
    }

    res[nbytes_read - 1] = '\0';

    LOGF_DEBUG("RES <%s>", res);

    VoltageNP[0].setValue(atoi(res) / 10.0);

    return true;
}

bool NightCrawler::setTemperatureOffset(double offset)
{
    char cmd[16] = {0};

    int nbytes_written = 0, rc = -1;
    char errstr[MAXRBUF];

    snprintf(cmd, 16, "Pt %03d#", static_cast<int>(offset * 10));

    LOGF_DEBUG("CMD <%s>", cmd);

    tcflush(PortFD, TCIOFLUSH);

    if ( (rc = tty_write(PortFD, cmd, strlen(cmd), &nbytes_written)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("%s error: %s.", __FUNCTION__, errstr);
        return false;
    }

    return true;
}

bool NightCrawler::getStepDelay(MotorType type)
{
    char cmd[16] = {0};
    char res[16] = {0};

    int nbytes_written = 0, nbytes_read = 0, rc = -1;
    char errstr[MAXRBUF];

    snprintf(cmd, 16, "%dSR#", type + 1);

    LOGF_DEBUG("CMD <%s>", cmd);

    tcflush(PortFD, TCIOFLUSH);

    if ( (rc = tty_write(PortFD, cmd, strlen(cmd), &nbytes_written)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("%s: %s.", __FUNCTION__, errstr);
        return false;
    }

    if ( (rc = tty_read_section(PortFD, res, '#', NIGHTCRAWLER_TIMEOUT, &nbytes_read)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("%s error: %s.", __FUNCTION__, errstr);
        return false;
    }

    res[nbytes_read - 1] = '\0';

    LOGF_DEBUG("RES <%s>", res);

    if (type == MOTOR_FOCUS)
        FocusStepDelayNP[0].setValue(atoi(res));
    else if (type == MOTOR_ROTATOR)
        RotatorStepDelayNP[0].setValue(atoi(res));
    else
        AuxStepDelayNP[0].setValue(atoi(res));

    return true;
}

bool NightCrawler::setStepDelay(MotorType type, uint32_t delay)
{
    char cmd[16] = {0};
    char res[16] = {0};

    int nbytes_written = 0, nbytes_read = 0, rc = -1;
    char errstr[MAXRBUF];

    snprintf(cmd, 16, "%dSR %03d#", type + 1, delay);

    LOGF_DEBUG("CMD <%s>", cmd);

    tcflush(PortFD, TCIOFLUSH);

    if ( (rc = tty_write(PortFD, cmd, strlen(cmd), &nbytes_written)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("%s error: %s.", __FUNCTION__, errstr);
        return false;
    }

    if ( (rc = tty_read(PortFD, res, 1, NIGHTCRAWLER_TIMEOUT, &nbytes_read)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("%s error: %s.", __FUNCTION__, errstr);
        return false;
    }

    res[nbytes_read] = '\0';

    LOGF_DEBUG("RES <%s>", res);

    return (res[0] == '#');
}

bool NightCrawler::getLimitSwitchStatus()
{
    char cmd[16] = "GS#";
    char res[16] = {0};

    int nbytes_written = 0, nbytes_read = 0, rc = -1;
    char errstr[MAXRBUF];

    LOGF_DEBUG("CMD <%s>", cmd);

    tcflush(PortFD, TCIOFLUSH);

    if ( (rc = tty_write(PortFD, cmd, strlen(cmd), &nbytes_written)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("%s error: %s.", __FUNCTION__, errstr);
        return false;
    }

    if ( (rc = tty_read_section(PortFD, res, '#', NIGHTCRAWLER_TIMEOUT, &nbytes_read)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("%s error: %s.", __FUNCTION__, errstr);
        return false;
    }

    res[nbytes_read - 1] = '\0';

    LOGF_DEBUG("RES <%s>", res);

    int value = atoi(res);

    LimitSwitchLP[ROTATION_SWITCH].setState((value & 0x01) ? IPS_ALERT : IPS_OK);
    LimitSwitchLP[OUT_SWITCH].setState((value & 0x02) ? IPS_ALERT : IPS_OK);
    LimitSwitchLP[IN_SWITCH].setState((value & 0x04) ? IPS_ALERT : IPS_OK);

    return true;
}

bool NightCrawler::findHome(uint8_t motorTypes)
{
    char cmd[16] = {0};
    char res[16] = {0};

    int nbytes_written = 0, nbytes_read = 0, rc = -1;
    char errstr[MAXRBUF];

    snprintf(cmd, 16, "SH %02d#", motorTypes);

    LOGF_DEBUG("CMD <%s>", cmd);

    tcflush(PortFD, TCIOFLUSH);

    if ( (rc = tty_write(PortFD, cmd, strlen(cmd), &nbytes_written)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("%s error: %s.", __FUNCTION__, errstr);
        return false;
    }

    if ( (rc = tty_read(PortFD, res, 1, NIGHTCRAWLER_TIMEOUT, &nbytes_read)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("%s error: %s.", __FUNCTION__, errstr);
        return false;
    }

    res[nbytes_read] = '\0';

    LOGF_DEBUG("RES <%s>", res);

    return (res[0] == '#');
}

bool NightCrawler::isHomingComplete()
{
    char res[16] = {0};
    int nbytes_read = 0, rc = -1;

    if ( (rc = tty_read_section(PortFD, res, '#', NIGHTCRAWLER_TIMEOUT, &nbytes_read)) != TTY_OK)
    {
        // No error as we are waiting until controller returns "OK#"
        LOG_DEBUG("Waiting for NightCrawler to complete homing...");
        return false;
    }

    res[nbytes_read - 1] = '\0';

    LOGF_DEBUG("RES <%s>", res);

    return (strcmp("OK", res) == 0);
}

bool NightCrawler::setEncodersEnabled(bool enable)
{
    char cmd[16] = {0};
    char res[16] = {0};

    int nbytes_written = 0, nbytes_read = 0, rc = -1;
    char errstr[MAXRBUF];

    snprintf(cmd, 16, "PE %s#", enable ? "01" : "00");

    LOGF_DEBUG("CMD <%s>", cmd);

    tcflush(PortFD, TCIOFLUSH);

    if ( (rc = tty_write(PortFD, cmd, strlen(cmd), &nbytes_written)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("%s error: %s.", __FUNCTION__, errstr);
        return false;
    }

    if ( (rc = tty_read_section(PortFD, res, '#', NIGHTCRAWLER_TIMEOUT, &nbytes_read)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("%s error: %s.", __FUNCTION__, errstr);
        return false;
    }

    res[nbytes_read] = '\0';

    LOGF_DEBUG("RES <%s>", res);

    return true;
}

bool NightCrawler::setDisplayBrightness(uint8_t value)
{
    char cmd[16] = {0};
    char res[16] = {0};

    int nbytes_written = 0, nbytes_read = 0, rc = -1;
    char errstr[MAXRBUF];

    snprintf(cmd, 16, "PD %03d#", value);

    LOGF_DEBUG("CMD <%s>", cmd);

    tcflush(PortFD, TCIOFLUSH);

    if ( (rc = tty_write(PortFD, cmd, strlen(cmd), &nbytes_written)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("%s error: %s.", __FUNCTION__, errstr);
        return false;
    }

    if ( (rc = tty_read(PortFD, res, 1, NIGHTCRAWLER_TIMEOUT, &nbytes_read)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("%s error: %s.", __FUNCTION__, errstr);
        return false;
    }

    res[nbytes_read] = '\0';

    LOGF_DEBUG("RES <%s>", res);

    return (res[0] == '#');
}

bool NightCrawler::setSleepBrightness(uint8_t value)
{
    char cmd[16] = {0};
    char res[16] = {0};

    int nbytes_written = 0, nbytes_read = 0, rc = -1;
    char errstr[MAXRBUF];

    snprintf(cmd, 16, "PL %03d#", value);

    LOGF_DEBUG("CMD <%s>", cmd);

    tcflush(PortFD, TCIOFLUSH);

    if ( (rc = tty_write(PortFD, cmd, strlen(cmd), &nbytes_written)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("%s error: %s.", __FUNCTION__, errstr);
        return false;
    }

    if ( (rc = tty_read(PortFD, res, 1, NIGHTCRAWLER_TIMEOUT, &nbytes_read)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("%s error: %s.", __FUNCTION__, errstr);
        return false;
    }

    res[nbytes_read] = '\0';

    LOGF_DEBUG("RES <%s>", res);

    return (res[0] == '#');
}

bool NightCrawler::saveConfigItems(FILE *fp)
{
    Focuser::saveConfigItems(fp);
    RI::saveConfigItems(fp);

    BrightnessNP.save(fp);
    FocusStepDelayNP.save(fp);
    RotatorStepDelayNP.apply();
    AuxStepDelayNP.save(fp);
    CustomRotatorStepNP.save(fp);

    return true;
}

IPState NightCrawler::HomeRotator()
{
    if (findHome(0x02))
    {
        FindHomeSP.setState(IPS_BUSY);
        FindHomeSP[0].setState(ISS_ON);
        FindHomeSP.apply();
        LOG_WARN("Homing process can take up to 10 minutes. You cannot control the unit until the process is fully complete.");
        return IPS_BUSY;
    }
    else
    {
        FindHomeSP.setState(IPS_ALERT);
        FindHomeSP[0].setState(ISS_OFF);
        FindHomeSP.apply();
        LOG_ERROR("Failed to start homing process.");
        return IPS_ALERT;
    }
}

IPState NightCrawler::MoveRotator(double angle)
{
    // Rotator move 0 to +180 degrees CCW
    // Rotator move 0 to -180 degrees CW
    // This is from looking at rotator from behind.
    const bool isReversed = ReverseRotatorSP[INDI_ENABLED].getState() == ISS_ON;
    auto newAngle = ( angle > 180 ? angle - 360 : angle);
    if (isReversed)
        newAngle *= -1;

    auto newTarget = newAngle * m_RotatorTicksPerDegree;
    if (newTarget < RotatorAbsPosNP[0].getMin())
        newTarget = RotatorAbsPosNP[0].getMin();
    else if (newTarget > RotatorAbsPosNP[0].getMax())
        newTarget = RotatorAbsPosNP[0].getMax();

    bool rc = gotoMotor(MOTOR_ROTATOR, static_cast<int32_t>(newTarget));

    if (rc)
    {
        RotatorAbsPosNP.setState(IPS_BUSY);
        RotatorAbsPosNP.apply();
        return IPS_BUSY;
    }

    return IPS_ALERT;
}

bool NightCrawler::SyncRotator(double angle)
{
    const bool isReversed = ReverseRotatorSP[INDI_ENABLED].getState() == ISS_ON;
    auto newAngle = ( angle > 180 ? angle - 360 : angle);
    if (isReversed)
        newAngle *= -1;

    auto newTarget = newAngle * m_RotatorTicksPerDegree;
    if (newTarget < RotatorAbsPosNP[0].getMin())
        newTarget = RotatorAbsPosNP[0].getMin();
    else if (newTarget > RotatorAbsPosNP[0].getMax())
        newTarget = RotatorAbsPosNP[0].getMax();

    return syncMotor(MOTOR_ROTATOR, static_cast<int32_t>(newTarget));
}

bool NightCrawler::AbortRotator()
{
    bool rc = stopMotor(MOTOR_ROTATOR);
    if (rc && RotatorAbsPosNP.getState() != IPS_OK)
    {
        RotatorAbsPosNP.setState(IPS_OK);
        RotatorAbsPosNP.apply();
    }

    return rc;
}

bool NightCrawler::ReverseRotator(bool enabled)
{
    // Immediately update the angle after reverse is set.
    if (enabled)
        GotoRotatorNP[0].setValue(range360(360 - (RotatorAbsPosNP[0].getValue() / m_RotatorTicksPerDegree)));
    else
        GotoRotatorNP[0].setValue(range360(RotatorAbsPosNP[0].getValue() / m_RotatorTicksPerDegree));
    GotoRotatorNP.apply();
    return true;
}
