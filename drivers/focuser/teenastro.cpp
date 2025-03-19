/*
    TeenAstro Focuser
    Copyright (C) 2021 Markus Noga

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

#include "teenastro.h"
#include "indicom.h"
#include "indicontroller.h"

#include <unistd.h> // for sleep()
#include <termios.h> // for tcflush

// Default, minimal and maximal values for focuser configuration properties
// In absolute units (not device units, where e.g. current is /10 and microsteps are log_2)
//

#define TAF_curr_default  500
#define TAF_curr_min 100
#define TAF_curr_max 1600
#define TAF_micro_default  (1<<4)
#define TAF_micro_min (1<<2)
#define TAF_micro_max (1<<7)
#define TAF_steprot_default  200
#define TAF_steprot_min 10
#define TAF_steprot_max 800
#define TAF_pos_default 0
#define TAF_pos_min 0
#define TAF_pos_max 2000000000UL
#define TAF_speed_default 20
#define TAF_speed_min 1
#define TAF_speed_max 999
#define TAF_acc_default 30
#define TAF_acc_min 1
#define TAF_acc_max 99
#define TAF_res_default 16
#define TAF_res_min 1
#define TAF_res_max 512

#define TAF_UI_STEPS    100.0
#define TAF_STEP(min,max)  (((max)-(min))/(TAF_UI_STEPS))


#define TAF_FOCUSER_TIMEOUT 2
#define TAF_FOCUSER_BUFSIZE 128

// Focuser singleton
std::unique_ptr<TeenAstroFocuser> teenAstroFocuser(new TeenAstroFocuser());

// Public methods
//

TeenAstroFocuser::TeenAstroFocuser()
{
    FI::SetCapability(FOCUSER_CAN_ABS_MOVE | FOCUSER_CAN_REL_MOVE | FOCUSER_CAN_ABORT |
                      FOCUSER_CAN_REVERSE  | FOCUSER_CAN_SYNC     | FOCUSER_HAS_VARIABLE_SPEED);
}

TeenAstroFocuser::~TeenAstroFocuser()
{
}

const char * TeenAstroFocuser::getDefaultName()
{
    return "TeenAstroFocuser";
}

bool TeenAstroFocuser::Handshake()
{
    char resp[TAF_FOCUSER_BUFSIZE];

    // Issue handshake challenge for TeenAstro focuser
    if(!sendAndReceive(":FV#", resp, TAF_FOCUSER_BUFSIZE))
        return false;
    if (strncmp(resp, "$ TeenAstro Focuser ", 20))
    {
        DEBUGF(INDI::Logger::DBG_ERROR, "Handshake response: %s", resp);
        return false;
    }

    // TeenAstro mounts internally forward focuser commands via a
    // serial connection. When mount & focuser are both on USB,
    // and scanning for devices, a race condition can occur. The
    // focuser device driver may probe the USB connection of the
    // mount first, and receive a correct response to the focuser
    // handshake challenge. Then the focuser driver would block
    // the USB port of the mount. And the scan performed by
    // mount driver would fail. To avoid this race condition,
    // we issue the handshake challenge for a LX200 mount and
    // abort if it is answered.
    if(!sendAndExpectTimeout(":GR#", resp, TAF_FOCUSER_BUFSIZE))
    {
        DEBUGF(INDI::Logger::DBG_DEBUG, "Device responded to focuser and mount handshake (%s), skipping.", resp);
        return false;
    }

    DEBUGF(INDI::Logger::DBG_DEBUG, "TeenAstroFocuser %s found", &resp[20]);
    return true;
}

bool TeenAstroFocuser::initProperties()
{
    INDI::Focuser::initProperties();

    FocusSyncNP[0].setLabel("Ticks"); // Rename unit of measure to ticks, to avoid confusion with motor steps,
    FocusMaxPosNP[0].setLabel("Ticks"); // motor microsteps and the effect of the resolution setting.
    FocusRelPosNP[0].setLabel("Ticks"); // Ticks are resolution units.
    FocusAbsPosNP[0].setLabel("Ticks");

    FocusReverseSP.setGroupName(FOCUS_TAB);

    FocusSpeedNP[0].setLabel("Go-to speed");
    FocusSpeedNP.setGroupName(FOCUS_TAB);
    FocusSpeedNP[0].setLabel("Steps/s");
    FocusSpeedNP[0].setMin(TAF_speed_min);
    FocusSpeedNP[0].setMax(TAF_speed_max);
    FocusSpeedNP[0].setStep(TAF_STEP(TAF_speed_min, TAF_speed_max));

    // Main control tab
    //

    CfgParkPosNP[0].fill("VAL", "Ticks", "%.f", 0, 100000, 1000, 0);
    CfgParkPosNP.fill(getDeviceName(), "PARK_POS", "Park position", MAIN_CONTROL_TAB, IP_RW, 60,
                       IPS_IDLE );

    IUFillSwitch(&GoToParkS[0], "VAL", "Park", ISS_OFF);
    IUFillSwitchVector(&GoToParkSP, GoToParkS, 1, getDeviceName(), "GOTO_PARK", "Go-to park", MAIN_CONTROL_TAB, IP_RW,
                       ISR_ATMOST1, 60, IPS_IDLE );

    IUFillNumber(&CurSpeedN[0], "VAL", "Steps/s", "%.f", TAF_speed_min, TAF_speed_max, TAF_STEP(TAF_speed_min, TAF_speed_max),
                 TAF_speed_default);
    IUFillNumberVector(&CurSpeedNP, CurSpeedN, 1, getDeviceName(), "CUR_SPEED", "Current Speed", MAIN_CONTROL_TAB, IP_RO, 0,
                       IPS_IDLE );

    IUFillNumber(&TempN[0], "VAL", "Celsius", "%+.1f°", -50., 50., 0, 0);
    IUFillNumberVector(&TempNP, TempN, 1, getDeviceName(), "TEMP", "Temperature", MAIN_CONTROL_TAB, IP_RO, 0, IPS_IDLE );

    // Focuser tab
    //

    IUFillText(&DeviceVersionT[0], "VAL", "Version", "unknown");
    IUFillTextVector(&DeviceVersionTP, DeviceVersionT, 1, getDeviceName(), "DEVICE_VERSION", "Device version", FOCUS_TAB, IP_RO,
                     60, IPS_IDLE);

    // Focuser tab: Motor configuration
    //

    CfgMotorStepsPerRevolutionNP[0].fill("VAL", "Steps", "%.f", TAF_steprot_min, TAF_steprot_max,
                 TAF_STEP(TAF_steprot_min, TAF_steprot_max), TAF_steprot_default);
    CfgMotorStepsPerRevolutionNP.fill(getDeviceName(), "MOT_STEPS_REV",
                       "Steps/rev.", FOCUS_TAB, IP_RW, 60, IPS_IDLE );

    IUFillSwitch(&CfgMotorMicrostepsS[TAF_MICROS_4],   "4",   "4",   ISS_OFF);
    IUFillSwitch(&CfgMotorMicrostepsS[TAF_MICROS_8],   "8",   "8",   ISS_OFF);
    IUFillSwitch(&CfgMotorMicrostepsS[TAF_MICROS_16],  "16",  "16",  ISS_OFF);
    IUFillSwitch(&CfgMotorMicrostepsS[TAF_MICROS_32],  "32",  "32",  ISS_OFF);
    IUFillSwitch(&CfgMotorMicrostepsS[TAF_MICROS_64],  "64",  "64",  ISS_OFF);
    IUFillSwitch(&CfgMotorMicrostepsS[TAF_MICROS_128], "128", "128", ISS_OFF);
    IUFillSwitchVector(&CfgMotorMicrostepsSP, CfgMotorMicrostepsS, TAF_MICROS_N, getDeviceName(), "MOT_USTEPS", "Microsteps",
                       FOCUS_TAB, IP_RW, ISR_ATMOST1, 60, IPS_IDLE );

    CfgMotorResolutionNP[0].fill("VAL", "Microsteps/tick", "%.f", TAF_res_min, TAF_res_max, TAF_STEP(TAF_res_min,
                 TAF_res_max), TAF_res_default);
    CfgMotorResolutionNP.fill(getDeviceName(), "MOT_RES", "Resolution", FOCUS_TAB,
                       IP_RW, 60, IPS_IDLE );

    CfgMotorCurrentNP[0].fill("VAL", "mA", "%.f", TAF_curr_min, TAF_curr_max, TAF_STEP(TAF_curr_min, TAF_curr_max),
                 TAF_curr_default);
    CfgMotorCurrentNP.fill(getDeviceName(), "MOT_CUR", "Motor current", FOCUS_TAB, IP_RW,
                       60, IPS_IDLE );

    // Focuser tab: Motion configuration
    //

    CfgGoToAccNP[0].fill("VAL", "Steps/s^2", "%.f", TAF_acc_min, TAF_acc_max, TAF_STEP(TAF_acc_min, TAF_acc_max),
                 TAF_acc_default);
    CfgGoToAccNP.fill(getDeviceName(), "GOTO_ACCEL", "Go-to accel.", FOCUS_TAB, IP_RW, 60,
                       IPS_IDLE );

    CfgManualSpeedNP[0].fill("VAL", "Steps/s", "%.f", TAF_speed_min, TAF_speed_max, TAF_STEP(TAF_speed_min,
                 TAF_speed_max), TAF_speed_default);
    CfgManualSpeedNP.fill(getDeviceName(), "MAN_SPEED", "Manual speed", FOCUS_TAB, IP_RW,
                       60, IPS_IDLE );

    CfgManualAccNP[0].fill("VAL", "Steps/s^2", "%.f", TAF_acc_min, TAF_acc_max, TAF_STEP(TAF_acc_min, TAF_acc_max),
                 TAF_acc_default);
    CfgManualAccNP.fill(getDeviceName(), "MAN_ACCEL", "Manual accel.", FOCUS_TAB, IP_RW, 60,
                       IPS_IDLE );

    CfgManualDecNP[0].fill("VAL", "Steps/s^2", "%.f", TAF_acc_min, TAF_acc_max, TAF_STEP(TAF_acc_min, TAF_acc_max),
                 TAF_acc_default);
    CfgManualDecNP.fill(getDeviceName(), "MAN_DECEL", "Manual decel.", FOCUS_TAB, IP_RW, 60,
                       IPS_IDLE );

    // Focuser tab: Device actions
    //

    IUFillSwitch(&RebootDeviceS[0], "VAL", "Reboot", ISS_OFF);
    IUFillSwitchVector(&RebootDeviceSP, RebootDeviceS, 1, getDeviceName(), "REBOOT_DEVICE", "Reboot device", FOCUS_TAB, IP_RW,
                       ISR_ATMOST1, 60, IPS_IDLE );

    IUFillSwitch(&EraseEEPROMS[0], "VAL", "Erase", ISS_OFF);
    IUFillSwitchVector(&EraseEEPROMSP, EraseEEPROMS, 1, getDeviceName(), "ERASE_EEPROM", "Erase EEPROM", FOCUS_TAB, IP_RW,
                       ISR_ATMOST1, 60, IPS_IDLE );

    addDebugControl();

    return true;
}

void TeenAstroFocuser::initPositionPropertiesRanges(uint32_t maxPos)
{
    FocusAbsPosNP[0].setMax(maxPos);
    FocusRelPosNP[0].setMax(maxPos);
    FocusSyncNP[0].setMax(maxPos);
    CfgParkPosNP[0].setMax(maxPos);

    auto step = maxPos / TAF_UI_STEPS;
    FocusAbsPosNP[0].setStep(step);
    FocusRelPosNP[0].setStep(step);
    FocusSyncNP[0].setStep(step);
    CfgParkPosNP[0].setStep(step);
}

bool TeenAstroFocuser::updateProperties()
{
    if(!INDI::Focuser::updateProperties())
        return false;

    if (isConnected())
    {
        // Update values from device before defining UI controls.
        // This minimizes flicker as changes to FocuserMaxPosNP
        // must redraw all positional controls to update their range.
        if(!(updateDeviceVersion() && updateMotorConfig() && updateMotionConfig() && updateState()))
            return false;

        defineMainControlProperties();
        defineOtherProperties();

        DEBUG(INDI::Logger::DBG_DEBUG, "TeenAstroFocuser ready for use.");
    }
    else
    {
        deleteMainControlProperties();
        deleteOtherProperties();
    }

    return true;
}

void TeenAstroFocuser::defineMainControlProperties()
{
    deleteProperty(FocusSyncNP);   // remove superclass controls to improve ordering of UI elements
    deleteProperty(FocusMaxPosNP);
    deleteProperty(FocusAbortSP);
    deleteProperty(FocusRelPosNP);
    deleteProperty(FocusAbsPosNP);

    defineProperty(FocusRelPosNP);
    defineProperty(FocusAbsPosNP);
    defineProperty(FocusAbortSP);
    defineProperty(&GoToParkSP);
    defineProperty(CfgParkPosNP);
    defineProperty(FocusSyncNP);
    defineProperty(FocusMaxPosNP);
    defineProperty(&CurSpeedNP);
    defineProperty(&TempNP);
}

void TeenAstroFocuser::deleteMainControlProperties()
{
    deleteProperty(GoToParkSP.name);
    deleteProperty(CfgParkPosNP);
    deleteProperty(CurSpeedNP.name);
    deleteProperty(TempNP.name);
}

void TeenAstroFocuser::defineOtherProperties()
{
    deleteProperty(FocusReverseSP); // Place this on the focuser tab to avoid clutter
    deleteProperty(FocusSpeedNP);

    defineProperty(&DeviceVersionTP);

    defineProperty(FocusReverseSP);
    defineProperty(CfgMotorStepsPerRevolutionNP);
    defineProperty(&CfgMotorMicrostepsSP);
    defineProperty(CfgMotorResolutionNP);
    defineProperty(CfgMotorCurrentNP);

    defineProperty(FocusSpeedNP);
    defineProperty(CfgGoToAccNP);
    defineProperty(CfgManualSpeedNP);
    defineProperty(CfgManualAccNP);
    defineProperty(CfgManualDecNP);

    defineProperty(&RebootDeviceSP);
    defineProperty(&EraseEEPROMSP);
}

void TeenAstroFocuser::deleteOtherProperties()
{
    deleteProperty(DeviceVersionTP.name);

    deleteProperty(CfgMotorStepsPerRevolutionNP);
    deleteProperty(CfgMotorMicrostepsSP.name);
    deleteProperty(CfgMotorResolutionNP);
    deleteProperty(CfgMotorCurrentNP);

    deleteProperty(CfgGoToAccNP);
    deleteProperty(CfgManualSpeedNP);
    deleteProperty(CfgManualAccNP);
    deleteProperty(CfgManualDecNP);

    deleteProperty(EraseEEPROMSP.name);
    deleteProperty(EraseEEPROMSP.name);
}

bool TeenAstroFocuser::ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if(dev == NULL || strcmp(dev, getDeviceName()))
        return INDI::Focuser::ISNewSwitch(dev, name, states, names, n);

    if(!strcmp(name, GoToParkSP.name))
    {
        GoToParkSP.s = IPS_BUSY;
        IDSetSwitch(&GoToParkSP, NULL);
        bool res = goToPark();
        GoToParkS[0].s = ISS_OFF;
        GoToParkSP.s = res ? IPS_OK : IPS_ALERT;
        IDSetSwitch(&GoToParkSP, NULL);
        return res;
    }
    else if(!strcmp(name, CfgMotorMicrostepsSP.name))
    {
        // parse selected number of usteps from states and labels. Labels are "Vxxx" where xxx is a number
        uint32_t usteps = 0;
        for(int i = 0; i < n; i++)
            if(states[i] == ISS_ON)
                usteps = atoi(names[i]);

        bool res = setMotorMicrosteps(usteps);
        if(res)
            IUUpdateSwitch(&CfgMotorMicrostepsSP, states, names, n);
        CfgMotorMicrostepsSP.s = res ? IPS_OK : IPS_ALERT;
        IDSetSwitch(&CfgMotorMicrostepsSP, NULL);
        return res;
    }
    else if(!strcmp(name, RebootDeviceSP.name))
    {
        RebootDeviceSP.s = IPS_BUSY;
        IDSetSwitch(&RebootDeviceSP, NULL);
        bool res = rebootDevice();
        RebootDeviceS[0].s = ISS_OFF;
        RebootDeviceSP.s = res ? IPS_OK : IPS_ALERT;
        IDSetSwitch(&RebootDeviceSP, NULL);
        return res;
    }
    else if(!strcmp(name, EraseEEPROMSP.name))
    {
        EraseEEPROMSP.s = IPS_BUSY;
        IDSetSwitch(&EraseEEPROMSP, NULL);
        bool res = eraseDeviceEEPROM();
        EraseEEPROMS[0].s = ISS_OFF;
        EraseEEPROMSP.s = res ? IPS_OK : IPS_ALERT;
        IDSetSwitch(&EraseEEPROMSP, NULL);
        return res;
    }
    else
        return INDI::Focuser::ISNewSwitch(dev, name, states, names, n);
}

bool TeenAstroFocuser::ISNewNumberHelper(INDI::PropertyNumber &NP, double values[], char *names[], int n, bool res)
{
    if(res)
        NP.update(values, names, n);
    NP.setState(res ? IPS_OK : IPS_ALERT);
    NP.apply();
    return res;
}

bool TeenAstroFocuser::ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n)
{
    if(dev == NULL || strcmp(dev, getDeviceName()))
        return INDI::Focuser::ISNewNumber(dev, name, values, names, n);

    if(FocusSyncNP.isNameMatch(name))
    {
        bool res = SyncFocuser((uint32_t) rint(values[0]));
        return ISNewNumberHelper(FocusSyncNP, values, names, n, res);
    }
    else if(CfgParkPosNP.isNameMatch(name))
    {
        bool res = setParkPos((uint32_t) rint(values[0]));
        return ISNewNumberHelper(CfgParkPosNP, values, names, n, res);
    }
    else if(FocusMaxPosNP.isNameMatch(name))
    {
        uint32_t val = (uint32_t) rint(values[0]);
        bool res = SetFocuserMaxPosition(val);
        if(res && FocusMaxPosNP[0].getValue() != val)
        {
            initPositionPropertiesRanges(val);
            deleteMainControlProperties();      // Force redraw of UI controls as IUUpdateMinMax
            defineMainControlProperties();    	// does not reliably update the step size.
        }
        return ISNewNumberHelper(FocusMaxPosNP, values, names, n, res);
    }
    else if(CfgManualSpeedNP.isNameMatch(name))
    {
        bool res = setManualSpeed((uint32_t) rint(values[0]));
        return ISNewNumberHelper(CfgManualSpeedNP, values, names, n, res);
    }
    else if(FocusSpeedNP.isNameMatch(name))
    {
        bool res = SetFocuserSpeed(rint(values[0]));
        return ISNewNumberHelper(FocusSpeedNP, values, names, n, res);
    }
    else if(CfgGoToAccNP.isNameMatch(name))
    {
        bool res = setGoToAcc((uint32_t) rint(values[0]));
        return ISNewNumberHelper(CfgGoToAccNP, values, names, n, res);
    }
    else if(CfgManualAccNP.isNameMatch(name))
    {
        bool res = setManualAcc((uint32_t) rint(values[0]));
        return ISNewNumberHelper(CfgManualAccNP, values, names, n, res);
    }
    else if(CfgManualDecNP.isNameMatch(name))
    {
        bool res = setManualDec((uint32_t) rint(values[0]));
        return ISNewNumberHelper(CfgManualDecNP, values, names, n, res);
    }
    else if(CfgMotorStepsPerRevolutionNP.isNameMatch(name))
    {
        bool res = setMotorStepsPerRevolution((uint32_t) rint(values[0]));
        return ISNewNumberHelper(CfgMotorStepsPerRevolutionNP, values, names, n, res);
    }
    else if(CfgMotorResolutionNP.isNameMatch(name))
    {
        bool res = setMotorResolution((uint32_t) rint(values[0]));
        return ISNewNumberHelper(CfgMotorResolutionNP, values, names, n, res);
    }
    else if(CfgMotorCurrentNP.isNameMatch(name))
    {
        bool res = setMotorCurrent((uint32_t) rint(values[0]));
        return ISNewNumberHelper(CfgMotorCurrentNP, values, names, n, res);
    }
    else
        return INDI::Focuser::ISNewNumber(dev, name, values, names, n);
}


IPState TeenAstroFocuser::MoveAbsFocuser(uint32_t pos)
{
    FocusAbsPosNP.setState(IPS_BUSY);
    FocusAbsPosNP.apply();
    FocusRelPosNP.setState(IPS_BUSY);
    FocusRelPosNP.apply();

    if(!goTo(pos))
    {
        FocusAbsPosNP.setState(IPS_ALERT);
        FocusAbsPosNP.apply();
        FocusRelPosNP.setState(IPS_ALERT);
        FocusRelPosNP.apply();

        return IPS_ALERT;
    }
    return IPS_BUSY;
}

IPState TeenAstroFocuser::MoveRelFocuser(FocusDirection dir, uint32_t ticks)
{
    FocusRelPosNP[0].setValue(ticks);

    uint32_t pos = uint32_t(FocusAbsPosNP[0].getValue());
    if (dir == FOCUS_INWARD)
        pos -= ticks;
    else
        pos += ticks;

    return MoveAbsFocuser(pos);
}

void TeenAstroFocuser::TimerHit()
{
    if(!isConnected())
        return;

    updateState();
    SetTimer(getCurrentPollingPeriod());
}

bool TeenAstroFocuser::AbortFocuser()
{
    FocusAbortSP.setState(IPS_BUSY);
    FocusAbortSP.apply();

    if(!stop())
    {
        FocusAbortSP.setState(IPS_ALERT);
        FocusAbortSP.apply();
        return false;
    }
    FocusAbortSP.setState(IPS_OK);
    FocusAbortSP.apply();

    return updateState();
}


// Protected methods
//

bool TeenAstroFocuser::send(const char *const msg)
{
    DEBUGF(INDI::Logger::DBG_DEBUG, "send(\"%s\")", msg);

    tcflush(PortFD, TCIOFLUSH);

    int nbytes_written = 0, rc = -1;
    if ( (rc = tty_write(PortFD, msg, strlen(msg), &nbytes_written)) != TTY_OK)
    {
        char errstr[MAXRBUF];
        tty_error_msg(rc, errstr, MAXRBUF);
        DEBUGF(INDI::Logger::DBG_ERROR, "send(\"%s\"): %s.", msg, errstr);
        return false;
    }
    return true;
}


bool TeenAstroFocuser::sendAndReceive(const char *const msg, char *resp, int bufsize)
{
    if(!send(msg))
        return false;

    int nbytes_read = 0;
    int rc = tty_nread_section(PortFD, resp, bufsize, '#', TAF_FOCUSER_TIMEOUT, &nbytes_read);
    resp[nbytes_read] = '\0';
    if(rc != TTY_OK || nbytes_read == 0 || resp[nbytes_read - 1] != '#')
    {
        char errstr[MAXRBUF];
        tty_error_msg(rc, errstr, MAXRBUF);
        DEBUGF(INDI::Logger::DBG_ERROR, "sendAndReceive(\"%s\") received \"%s\": %s.", msg, resp, errstr);
        return false;
    }
    DEBUGF(INDI::Logger::DBG_DEBUG, "sendAndReceive(\"%s\") received \"%s\".", msg, resp);
    return true;
}

bool TeenAstroFocuser::sendAndReceiveBool(const char *const msg)
{
    if(!send(msg))
        return false;

    char resp[2];
    int nbytes_read = 0;
    int rc = tty_read(PortFD, resp, 1, TAF_FOCUSER_TIMEOUT, &nbytes_read);
    resp[nbytes_read] = '\0';
    if(rc != TTY_OK || resp[0] != '1')
    {
        char errstr[MAXRBUF];
        tty_error_msg(rc, errstr, MAXRBUF);
        DEBUGF(INDI::Logger::DBG_ERROR, "sendAndReceiveBool(\"%s\") received \"%s\": %s.", msg, resp, errstr);
        return false;
    }
    DEBUGF(INDI::Logger::DBG_DEBUG, "sendAndReceiveBool(\"%s\") received \"%s\".", msg, resp);
    return resp[0] == '1';
}


bool TeenAstroFocuser::sendAndExpectTimeout(const char *const msg, char *resp, int bufsize)
{
    if(!send(msg))
        return false;

    int nbytes_read = 0;
    int rc = tty_nread_section(PortFD, resp, bufsize, '#', TAF_FOCUSER_TIMEOUT, &nbytes_read);
    resp[nbytes_read] = '\0';
    if(rc != TTY_TIME_OUT || nbytes_read != 0)
    {
        char errstr[MAXRBUF];
        tty_error_msg(rc, errstr, MAXRBUF);
        DEBUGF(INDI::Logger::DBG_ERROR, "sendAndExpectTimeout(\"%s\") received \"%s\".", msg, resp);
        return false;
    }
    DEBUGF(INDI::Logger::DBG_DEBUG, "sendAndExpectTimeout(\"%s\") got timeout.", msg, resp);
    return true;
}


bool TeenAstroFocuser::updateDeviceVersion()
{
    char resp[TAF_FOCUSER_BUFSIZE];
    if(!sendAndReceive(":FV#", resp, TAF_FOCUSER_BUFSIZE))
        return false;
    int len = strlen(resp);
    resp[len - 1] = 0;
    IUSaveText(&DeviceVersionT[0], resp + 2);
    DeviceVersionTP.s = IPS_OK;
    IDSetText(&DeviceVersionTP, NULL);
    return true;
}

bool TeenAstroFocuser::updateState()
{
    char resp[TAF_FOCUSER_BUFSIZE];
    int pos = -1, speed = -1;
    float temp = -1;

    if(!sendAndReceive(":F?#", resp, TAF_FOCUSER_BUFSIZE))
        return false;

    if(sscanf(resp, "?%d %d %f#", &pos, &speed, &temp) <= 0)
    {
        DEBUGF(INDI::Logger::DBG_ERROR, "Invalid format: focuser state (%s)", resp);
        return false;
    }

    if(FocusAbsPosNP.getState() == IPS_BUSY && speed == 0)
        DEBUG(INDI::Logger::DBG_SESSION, "Focuser reached target position.");

    FocusAbsPosNP[0].setValue(pos);
    FocusAbsPosNP.setState(speed > 0 ? IPS_BUSY : IPS_OK);
    FocusAbsPosNP.apply();

    FocusRelPosNP.setState(speed > 0 ? IPS_BUSY : IPS_OK);
    FocusRelPosNP.apply();

    CurSpeedN[0].value = speed;
    CurSpeedNP.s = speed > 0 ? IPS_BUSY : IPS_OK;
    IDSetNumber(&CurSpeedNP, NULL);

    TempN[0].value = temp;
    TempNP.s = (temp == -99) ? IPS_ALERT : IPS_OK;
    IDSetNumber(&TempNP, NULL);

    return true;
}

bool TeenAstroFocuser::isMoving()
{
    if(!updateState())
        return false;
    return CurSpeedN[0].value > 0;
}


bool TeenAstroFocuser::updateMotionConfig()
{
    char resp[TAF_FOCUSER_BUFSIZE];
    int parkPos = -1, maxPos = -1, manualSpeed = -1, goToSpeed = -1, gotoAcc = -1, manAcc = -1, manDec = -1;

    if(!sendAndReceive(":F~#", resp, TAF_FOCUSER_BUFSIZE))
        return false;

    if(sscanf(resp, "~%d %d %d %d %d %d %d#", &parkPos, &maxPos, &manualSpeed, &goToSpeed, &gotoAcc, &manAcc, &manDec) <= 0)
    {
        DEBUGF(INDI::Logger::DBG_ERROR, "Invalid format: focuser motion config (%s)", resp);
        return false;
    }

    if(maxPos != FocusMaxPosNP[0].getValue())
        initPositionPropertiesRanges(maxPos);

    CfgParkPosNP[0].setValue(parkPos);
    CfgParkPosNP.setState(IPS_OK);
    CfgParkPosNP.apply();
    FocusMaxPosNP[0].setValue(maxPos);
    FocusMaxPosNP.setState(IPS_OK);
    FocusMaxPosNP.apply();
    CfgManualSpeedNP[0].setValue(manualSpeed);
    CfgManualSpeedNP.setState(IPS_OK);
    CfgManualSpeedNP.apply();
    FocusSpeedNP[0].setValue(goToSpeed);
    FocusSpeedNP.setState(IPS_OK);
    FocusSpeedNP.apply();
    CfgGoToAccNP[0].setValue(gotoAcc);
    CfgGoToAccNP.setState(IPS_OK);
    CfgGoToAccNP.apply();
    CfgManualAccNP[0].setValue(manAcc);
    CfgManualAccNP.setState(IPS_OK);
    CfgManualAccNP.apply();
    CfgManualDecNP[0].setValue(manDec);
    CfgManualDecNP.setState(IPS_OK);
    CfgManualDecNP.apply();

    return true;
}

bool TeenAstroFocuser::setConfigItem(char item, uint32_t deviceValue)
{
    char cmd[64];
    snprintf(cmd, 64, ":F%c,%d#", item, deviceValue);
    return sendAndReceiveBool(cmd);
}

bool TeenAstroFocuser::setParkPos(uint32_t value)
{
    return setConfigItem('0', value);
}

bool TeenAstroFocuser::SetFocuserMaxPosition(uint32_t value)
{
    return setConfigItem('1', value);
}

bool TeenAstroFocuser::setManualSpeed(uint32_t value)
{
    return setConfigItem('2', value);
}

bool TeenAstroFocuser::SetFocuserSpeed(int value)
{
    return setConfigItem('3', (uint32_t) value);
}

bool TeenAstroFocuser::setGoToAcc(uint32_t value)
{
    return setConfigItem('4', value);
}

bool TeenAstroFocuser::setManualAcc(uint32_t value)
{
    return setConfigItem('5', value);
}

bool TeenAstroFocuser::setManualDec(uint32_t value)
{
    return setConfigItem('6', value);
}


bool TeenAstroFocuser::updateMotorConfig()
{
    char resp[TAF_FOCUSER_BUFSIZE];
    int reverse = -1, log2_micro = -1, resolution = -1, curr_10ma = -1, steprot = -1;

    if(!sendAndReceive(":FM#", resp, TAF_FOCUSER_BUFSIZE))
        return false;

    if(sscanf(resp, "M%d %d %d %d %d#", &reverse, &log2_micro, &resolution, &curr_10ma, &steprot) <= 0)
    {
        DEBUGF(INDI::Logger::DBG_ERROR, "Invalid format: focuser motor config (%s)", resp);
        return false;
    }

    FocusReverseSP[INDI_ENABLED].setState((reverse == 0) ? ISS_OFF : ISS_ON);
    FocusReverseSP[INDI_DISABLED].setState((reverse == 0) ? ISS_ON  : ISS_OFF);
    FocusReverseSP.setState(IPS_OK);
    FocusReverseSP.apply();

    uint32_t micro = (1 << log2_micro); // TeenAstro device returns and expects log_2(microsteps)
    bool microFound = false;
    for(int i = 0; i < TAF_MICROS_N; i++)
    {
        uint32_t thisMicro = (uint32_t) atoi(CfgMotorMicrostepsS[i].name);
        CfgMotorMicrostepsS[i].s = (micro == thisMicro) ? ISS_ON : ISS_OFF;
        if(micro == thisMicro)
            microFound = true;
    }
    CfgMotorMicrostepsSP.s = microFound ? IPS_OK : IPS_ALERT;
    IDSetSwitch(&CfgMotorMicrostepsSP, NULL);

    CfgMotorResolutionNP[0].setValue(resolution);
    CfgMotorResolutionNP.setState(IPS_OK);
    CfgMotorResolutionNP.apply();
    CfgMotorCurrentNP[0].setValue(curr_10ma * 10);    // TeenAstro device returns and expects units of 10 mA
    CfgMotorCurrentNP.setState(IPS_OK);
    CfgMotorCurrentNP.apply();
    CfgMotorStepsPerRevolutionNP[0].setValue(steprot);
    CfgMotorStepsPerRevolutionNP.setState(IPS_OK);
    CfgMotorStepsPerRevolutionNP.apply();

    return true;
}

bool TeenAstroFocuser::ReverseFocuser(bool enable)
{
    return setConfigItem('7', enable ? 1 : 0);
}

bool TeenAstroFocuser::setMotorMicrosteps(uint32_t value)
{
    uint32_t bitPos = 0;
    value >>= 1;
    for(; value != 0; bitPos++)
        value >>= 1;
    return setConfigItem('m', bitPos);      // TeenAstro device returns and expects log_2(microsteps)
}

bool TeenAstroFocuser::setMotorResolution(uint32_t value)
{
    return setConfigItem('8', value);
}

bool TeenAstroFocuser::setMotorCurrent(uint32_t value)
{
    return setConfigItem('c', value / 10);  // TeenAstro device returns and expects units of 10 mA
}

bool TeenAstroFocuser::setMotorStepsPerRevolution(uint32_t value)
{
    return setConfigItem('r', value);
}

bool TeenAstroFocuser::SyncFocuser(uint32_t value)
{
    char cmd[64];
    snprintf(cmd, 64, ":FS,%d#", value);
    return send(cmd); // no confirmation via "0" or "1"
}


bool TeenAstroFocuser::goTo(uint32_t position)
{
    char cmd[64];
    snprintf(cmd, 64, ":FG,%d#", position);
    return send(cmd);
}

bool TeenAstroFocuser::goToPark()
{
    return send(":FP#");
}

bool TeenAstroFocuser::stop()
{
    return send(":FQ#");
}

bool TeenAstroFocuser::rebootDevice()
{
    if(!send(":F!#"))
        return false;
    sleep(3);
    return updateDeviceVersion() && updateMotorConfig() && updateMotionConfig() && updateState();
}

bool TeenAstroFocuser::eraseDeviceEEPROM()
{
    if(!send(":F$#"))
        return false;
    sleep(1);
    return updateDeviceVersion() && updateMotorConfig() && updateMotionConfig() && updateState();
}

bool TeenAstroFocuser::saveConfigItems(FILE *fp)
{
    // This is a verbatim copy of Focuser::saveConfigItems except one override
    //

    DefaultDevice::saveConfigItems(fp);

    // Do not save focuser configuration items on INDI host, as they are stored on the focuser device.
    // FI::saveConfigItems(fp);

    PresetNP.save(fp);
    this->controller->saveConfigItems(fp);

    return true;
}
