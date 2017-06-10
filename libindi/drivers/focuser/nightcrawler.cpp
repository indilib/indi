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

#include <stdio.h>
#include <termios.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>
#include <math.h>
#include <memory>

#define NIGHTCRAWLER_TIMEOUT 3
#define POLLMS 500
#define ROTATOR_TAB "Rotator"
#define AUX_TAB "Aux"
#define SETTINGS_TAB "Settings"

// Well, it is time I name something, even if simple, after Tommy, my loyal German Shephard companion.
// By the time of writing this, he is almost 4 years old. Live long and prosper, my good boy!
std::unique_ptr<NightCrawler> tommyGoodBoy(new NightCrawler());

void ISGetProperties(const char * dev)
{
    tommyGoodBoy->ISGetProperties(dev);
}

void ISNewSwitch(const char * dev, const char * name, ISState * states, char * names[], int num)
{
    tommyGoodBoy->ISNewSwitch(dev, name, states, names, num);
}

void ISNewText(	const char * dev, const char * name, char * texts[], char * names[], int num)
{
    tommyGoodBoy->ISNewText(dev, name, texts, names, num);
}

void ISNewNumber(const char * dev, const char * name, double values[], char * names[], int num)
{
    tommyGoodBoy->ISNewNumber(dev, name, values, names, num);
}

void ISNewBLOB (const char * dev, const char * name, int sizes[], int blobsizes[], char * blobs[], char * formats[], char * names[], int n)
{
    INDI_UNUSED(dev);
    INDI_UNUSED(name);
    INDI_UNUSED(sizes);
    INDI_UNUSED(blobsizes);
    INDI_UNUSED(blobs);
    INDI_UNUSED(formats);
    INDI_UNUSED(names);
    INDI_UNUSED(n);
}

void ISSnoopDevice (XMLEle * root)
{
    tommyGoodBoy->ISSnoopDevice(root);
}

NightCrawler::NightCrawler()
{

    // Can move in Absolute & Relative motions, can AbortFocuser motion, and has variable speed.
    SetFocuserCapability(FOCUSER_CAN_ABS_MOVE | FOCUSER_CAN_REL_MOVE | FOCUSER_CAN_ABORT);

    lastPosition = 0;
    lastTemperature = 0;
}

NightCrawler::~NightCrawler()
{

}

bool NightCrawler::initProperties()
{
    INDI::Focuser::initProperties();

    FocusSpeedN[0].min = 1;
    FocusSpeedN[0].max = 1;
    FocusSpeedN[0].value = 1;

    // Focus Sync
    IUFillNumber(&SyncFocusN[0], "FOCUS_SYNC_OFFSET", "Ticks", "%.f", 0, 100000., 0., 0.);
    IUFillNumberVector(&SyncFocusNP, SyncFocusN, 1, getDeviceName(), "FOCUS_SYNC", "Sync", MAIN_CONTROL_TAB, IP_RW, 0, IPS_IDLE );

    // Temperature + Voltage Sensors
    IUFillNumber(&SensorN[SENSOR_TEMPERATURE], "TEMPERATURE", "Temperature (C)", "%.2f", -100, 100., 1., 0.);
    IUFillNumber(&SensorN[SENSOR_VOLTAGE], "VOLTAGE", "Voltage (V)", "%.2f", 0, 20., 1., 0.);
    IUFillNumberVector(&SensorNP, SensorN, 2, getDeviceName(), "SENSORS", "Sensors", MAIN_CONTROL_TAB, IP_RO, 0, IPS_IDLE );

    // Temperature offset
    IUFillNumber(&SensorN[SENSOR_TEMPERATURE], "OFFSET", "OFFSET", "%.2f", -15, 15., 1., 0.);
    IUFillNumberVector(&SensorNP, SensorN, 1, getDeviceName(), "TEMPERATURE_OFFSET", "Temperature", MAIN_CONTROL_TAB, IP_WO, 0, IPS_IDLE );

    // Motor Step Delay
    IUFillNumber(&FocusStepDelayN[0], "FOCUS_STEP", "Value", "%.f", 7, 100., 1., 10.);
    IUFillNumberVector(&FocusStepDelayNP, FocusStepDelayN, 1, getDeviceName(), "FOCUS_STEP_DELAY", "Step Rate", SETTINGS_TAB, IP_RW, 0, IPS_IDLE );

    // Limit Switch
    IUFillLight(&LimitSwitchL[ROTATION_SWITCH], "ROTATION_SWITCH", "Rotation Home", IPS_OK);
    IUFillLight(&LimitSwitchL[OUT_SWITCH], "OUT_SWITCH", "Out Limit", IPS_OK);
    IUFillLight(&LimitSwitchL[IN_SWITCH], "IN_SWITCH", "In Limit", IPS_OK);
    IUFillLightVector(&LimitSwitchLP, LimitSwitchL, 3, getDeviceName(), "LIMIT_SWITCHES", "Limit Switch", SETTINGS_TAB, IPS_IDLE);

    // Home
    IUFillSwitch(&FindFocusHomeS[0], "FOCUS_HOME", "Home", ISS_OFF);
    IUFillSwitchVector(&FindFocusHomeSP, FindFocusHomeS, 1, getDeviceName(), "FIND_FOCUS_HOME", "Focuser", SETTINGS_TAB, IP_RW, ISR_ATMOST1, 0, IPS_IDLE);

    // Encoders
    IUFillSwitch(&EncoderS[0], "ENABLED", "Enabled", ISS_ON);
    IUFillSwitch(&EncoderS[1], "DISABLED", "Disabled", ISS_OFF);
    IUFillSwitchVector(&EncoderSP, EncoderS, 2, getDeviceName(), "ENCODERS", "Encoders", SETTINGS_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    // Brightness
    IUFillNumber(&BrightnessN[BRIGHTNESS_DISPLAY], "BRIGHTNESS_DISPLAY", "Display", "%.f", 0, 255., 10., 0.);
    IUFillNumber(&BrightnessN[BRIGHTNESS_SLEEP], "BRIGHTNESS_SLEEP", "Sleep", "%.f", 1, 255., 10., 0.);
    IUFillNumberVector(&BrightnessNP, BrightnessN, 2, getDeviceName(), "BRIGHTNESS", "Brightness", SETTINGS_TAB, IP_RW, 0, IPS_IDLE );

    //////////////////////////////////////////////////////
    // Rotator Properties
    /////////////////////////////////////////////////////

    // Rotator GOTO
    IUFillNumber(&GotoRotatorN[0], "ROTATOR_GOTO_TICK", "Ticks", "%.f", 0, 100000., 0., 0.);
    IUFillNumberVector(&GotoRotatorNP, GotoRotatorN, 1, getDeviceName(), "GOTO_ROTATOR", "Goto", ROTATOR_TAB, IP_RW, 0, IPS_IDLE );

    // Rotator Sync
    IUFillNumber(&SyncRotatorN[0], "ROTATOR_SYNC_TICK", "Ticks", "%.f", 0, 100000., 0., 0.);
    IUFillNumberVector(&SyncRotatorNP, SyncRotatorN, 1, getDeviceName(), "SYNC_ROTATOR", "Sync", ROTATOR_TAB, IP_RW, 0, IPS_IDLE );

    // Find Rotator Home
    IUFillSwitch(&FindRotatorHomeS[0], "ROTATOR_HOME", "Find", ISS_OFF);
    IUFillSwitchVector(&FindRotatorHomeSP, FindRotatorHomeS, 1, getDeviceName(), "FIND_HOME_ROTATOR", "Home", ROTATOR_TAB, IP_RW, ISR_ATMOST1, 0, IPS_IDLE);

    // Rotator Step Delay
    IUFillNumber(&RotatorStepDelayN[0], "ROTATOR_STEP", "Value", "%.f", 1, 100., 1., 10.);
    IUFillNumberVector(&RotatorStepDelayNP, RotatorStepDelayN, 1, getDeviceName(), "ROTATOR_STEP_DELAY", "Step Rate", ROTATOR_TAB, IP_RW, 0, IPS_IDLE );

    //////////////////////////////////////////////////////
    // Aux Properties
    /////////////////////////////////////////////////////

    // Aux GOTO
    IUFillNumber(&GotoAuxN[0], "AUX_GOTO_TICK", "Ticks", "%.f", 0, 100000., 0., 0.);
    IUFillNumberVector(&GotoAuxNP, GotoAuxN, 1, getDeviceName(), "GOTO_AUX", "Goto", AUX_TAB, IP_RW, 0, IPS_IDLE );

    // Aux Sync
    IUFillNumber(&SyncAuxN[0], "AUX_SYNC_TICK", "Ticks", "%.f", 0, 100000., 0., 0.);
    IUFillNumberVector(&SyncAuxNP, SyncAuxN, 1, getDeviceName(), "SYNC_AUX", "Sync", AUX_TAB, IP_RW, 0, IPS_IDLE );

    // Find Aux Home
    IUFillSwitch(&FindAuxHomeS[0], "AUX_HOME", "Find", ISS_OFF);
    IUFillSwitchVector(&FindAuxHomeSP, FindAuxHomeS, 1, getDeviceName(), "FIND_HOME_AUX", "Home", AUX_TAB, IP_RW, ISR_ATMOST1, 0, IPS_IDLE);

    // Aux Step Delay
    IUFillNumber(&AuxStepDelayN[0], "AUX_STEP", "Value", "%.f", 1, 100., 1., 10.);
    IUFillNumberVector(&AuxStepDelayNP, AuxStepDelayN, 1, getDeviceName(), "AUX_STEP_DELAY", "Step Rate", AUX_TAB, IP_RW, 0, IPS_IDLE );

    /* Relative and absolute movement */
    FocusRelPosN[0].min = 0.;
    FocusRelPosN[0].max = 50000.;
    FocusRelPosN[0].value = 0;
    FocusRelPosN[0].step = 1000;

    FocusAbsPosN[0].min = -0.;
    FocusAbsPosN[0].max = 100000.;
    FocusAbsPosN[0].value = 0;
    FocusAbsPosN[0].step = 1000;

    addDebugControl();

    updatePeriodMS = POLLMS;

    return true;
}

bool NightCrawler::updateProperties()
{
    INDI::Focuser::updateProperties();

    if (isConnected())
    {
        // Focus
        defineNumber(&SyncFocusNP);
        defineNumber(&SensorNP);
        defineNumber(&TemperatureSettingNP);
        defineNumber(&FocusStepDelayNP);
        defineLight(&LimitSwitchLP);
        defineSwitch(&EncoderSP);
        defineNumber(&BrightnessNP);
        defineSwitch(&FindFocusHomeSP);

        // Rotator
        defineNumber(&GotoRotatorNP);
        defineNumber(&SyncRotatorNP);
        defineNumber(&RotatorStepDelayNP);
        defineSwitch(&FindRotatorHomeSP);

        // Aux
        defineNumber(&GotoAuxNP);
        defineNumber(&SyncAuxNP);
        defineNumber(&AuxStepDelayNP);
        defineSwitch(&FindAuxHomeSP);
    }
    else
    {
        // Focus
        deleteProperty(SyncFocusNP.name);
        deleteProperty(SensorNP.name);
        deleteProperty(TemperatureSettingNP.name);
        deleteProperty(FocusStepDelayNP.name);
        deleteProperty(LimitSwitchLP.name);
        deleteProperty(FindFocusHomeSP.name);
        deleteProperty(EncoderSP.name);
        deleteProperty(BrightnessNP.name);

        // Rotator
        deleteProperty(GotoRotatorNP.name);
        deleteProperty(SyncRotatorNP.name);
        deleteProperty(RotatorStepDelayNP.name);
        deleteProperty(FindRotatorHomeSP.name);

        // Aux
        deleteProperty(GotoAuxNP.name);
        deleteProperty(SyncAuxNP.name);
        deleteProperty(AuxStepDelayNP.name);
        deleteProperty(FindAuxHomeSP.name);
    }

    return true;
}

bool NightCrawler::Handshake()
{
    if (Ack())
    {
        DEBUG(INDI::Logger::DBG_SESSION, "NightCrawler is online.");
        return true;
    }

    DEBUG(INDI::Logger::DBG_SESSION, "Error retreiving data from NightCrawler, please ensure NightCrawler controller is powered and the port is correct.");
    return false;
}

const char * NightCrawler::getDefaultName()
{
    return "NightCrawler";
}

bool NightCrawler::Ack()
{
    return getFirmware();
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
        DEBUGF(INDI::Logger::DBG_ERROR, "getFirmware error: %s.", errstr);
        return false;
    }

    if ( (rc = tty_read_section(PortFD, resp, '#', NIGHTCRAWLER_TIMEOUT, &nbytes_read)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        DEBUGF(INDI::Logger::DBG_ERROR, "getFirmware error: %s.", errstr);
        return false;
    }

    tcflush(PortFD, TCIOFLUSH);

    resp[nbytes_read-1] = '\0';

    DEBUGF(INDI::Logger::DBG_SESSION, "Firmware %s", resp);

    return true;
}

bool NightCrawler::gotoMotor(MotorType type, int32_t position)
{
    char cmd[16] = {0};
    char res[16] = {0};

    int nbytes_written = 0, nbytes_read = 0, rc = -1;
    char errstr[MAXRBUF];

    snprintf(cmd, 16, "%dSN%d#", type+1, position);

    DEBUGF(INDI::Logger::DBG_DEBUG, "CMD <%s>", cmd);

    tcflush(PortFD, TCIOFLUSH);

    if ( (rc = tty_write(PortFD, cmd, strlen(cmd), &nbytes_written)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        DEBUGF(INDI::Logger::DBG_ERROR, "gotoMotor: %s.", errstr);
        return false;
    }

    if ( (rc = tty_read(PortFD, res, 1, NIGHTCRAWLER_TIMEOUT, &nbytes_read)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        DEBUGF(INDI::Logger::DBG_ERROR, "gotoMotor error: %s.", errstr);
        return false;
    }

    DEBUGF(INDI::Logger::DBG_DEBUG, "RES <%s>", res);

    return startMotor(type);
}

bool NightCrawler::getPosition(MotorType type)
{
    char cmd[16] = {0};
    char res[16] = {0};
    int position = -1e6;

    int nbytes_written = 0, nbytes_read = 0, rc = -1;
    char errstr[MAXRBUF];

    snprintf(cmd, 16, "%dGP#", type+1);

    DEBUGF(INDI::Logger::DBG_DEBUG, "CMD <%s>", cmd);

    tcflush(PortFD, TCIOFLUSH);

    if ( (rc = tty_write(PortFD, cmd, strlen(cmd), &nbytes_written)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        DEBUGF(INDI::Logger::DBG_ERROR, "getPosition: %s.", errstr);
        return false;
    }

    if ( (rc = tty_read(PortFD, res, 8, NIGHTCRAWLER_TIMEOUT, &nbytes_read)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        DEBUGF(INDI::Logger::DBG_ERROR, "getPosition error: %s.", errstr);
        return false;
    }

    res[nbytes_read] = '\0';

    DEBUGF(INDI::Logger::DBG_DEBUG, "RES <%s>", res);

    position = atoi(res);

    if (position != -1e6)
    {
        if (type == MOTOR_FOCUS)
            FocusAbsPosN[0].value = position;
        else if (type == MOTOR_ROTATOR)
            GotoRotatorN[0].value = position;
        else
            GotoAuxN[0].value = position;

        return true;
    }

    DEBUGF(INDI::Logger::DBG_DEBUG, "Invalid Position! %d", position);
    return false;
}

bool NightCrawler::ISNewSwitch (const char * dev, const char * name, ISState * states, char * names[], int n)
{
    if(strcmp(dev, getDeviceName()) == 0)
    {

    }

    return INDI::Focuser::ISNewSwitch(dev, name, states, names, n);
}

bool NightCrawler::ISNewNumber (const char * dev, const char * name, double values[], char * names[], int n)
{
    if(strcmp(dev, getDeviceName()) == 0)
    {

    }

    return INDI::Focuser::ISNewNumber(dev, name, values, names, n);

}

IPState NightCrawler::MoveAbsFocuser(uint32_t targetTicks)
{
    targetPosition = targetTicks;

    bool rc = false;

    rc = gotoMotor(MOTOR_FOCUS, targetPosition);

    if (rc == false)
        return IPS_ALERT;

    FocusAbsPosNP.s = IPS_BUSY;

    return IPS_BUSY;
}

IPState NightCrawler::MoveRelFocuser(FocusDirection dir, uint32_t ticks)
{
    double newPosition = 0;
    bool rc = false;

    if (dir == FOCUS_INWARD)
        newPosition = FocusAbsPosN[0].value - ticks;
    else
        newPosition = FocusAbsPosN[0].value + ticks;

    rc = gotoMotor(MOTOR_FOCUS, newPosition);

    if (rc == false)
        return IPS_ALERT;

    FocusRelPosN[0].value = ticks;
    FocusRelPosNP.s = IPS_BUSY;

    return IPS_BUSY;
}

void NightCrawler::TimerHit()
{
    if (isConnected() == false)
    {
        SetTimer(POLLMS);
        return;
    }

    SetTimer(POLLMS);
}

bool NightCrawler::AbortFocuser()
{
    return false;
}

bool NightCrawler::syncMotor(MotorType type, int32_t position)
{
    return false;
}

bool NightCrawler::startMotor(MotorType type)
{
    return false;
}

bool NightCrawler::stopMotor(MotorType type)
{
    return false;
}

bool NightCrawler::isMotorMoving(MotorType type)
{
    return false;
}

bool NightCrawler::getSensors()
{
   return false;
}

bool NightCrawler::setTemperatureOffset(double offset)
{
    return false;
}

bool NightCrawler::getStepDelay(MotorType type)
{
    return false;
}

bool NightCrawler::setStepDelay(MotorType type, uint32_t delay)
{
    return false;
}

bool NightCrawler::getLimitSwitchStatus()
{
    return false;
}

bool NightCrawler::findHome(MotorType type)
{
    return false;
}

bool NightCrawler::setEncodersEnabled(bool enable)
{
    return false;
}

bool NightCrawler::getBrightness()
{
    return false;
}

bool NightCrawler::setBrightness(uint8_t display, uint8_t sleep)
{
    return false;
}
