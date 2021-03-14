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

#include <stdio.h>
#include <termios.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>
#include <math.h>
#include <memory>

#define TEENASTRO_FOCUSER_TIMEOUT 4

#define POLLMS_OVERRIDE  1500

// Focuser singleton
std::unique_ptr<TeenAstroFocuser> teenAstroFocuser(new TeenAstroFocuser());


// Static method wrappers for singleton
//

void ISGetProperties(const char *dev)
{
         teenAstroFocuser->ISGetProperties(dev);
}

void ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int num)
{
         teenAstroFocuser->ISNewSwitch(dev, name, states, names, num);
}

void ISNewText(	const char *dev, const char *name, char *texts[], char *names[], int num)
{
         teenAstroFocuser->ISNewText(dev, name, texts, names, num);
}

void ISNewNumber(const char *dev, const char *name, double values[], char *names[], int num)
{
         teenAstroFocuser->ISNewNumber(dev, name, values, names, num);
}

void ISNewBLOB (const char *dev, const char *name, int sizes[], int blobsizes[], char *blobs[], char *formats[], char *names[], int n)
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

void ISSnoopDevice (XMLEle *root)
{
     teenAstroFocuser->ISSnoopDevice(root);
}


// Public methods
//

TeenAstroFocuser::TeenAstroFocuser()
{
    FI::SetCapability(FOCUSER_CAN_ABS_MOVE | FOCUSER_CAN_REL_MOVE | FOCUSER_CAN_ABORT);
    lastPos = 0;
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
    sleep(2);

    char resp[128];
    if(!sendAndReceive(":FV#", resp))
        return false;
    if (strncmp(resp, "$ TeenAstro Focuser ",20))
    {
        DEBUGF(INDI::Logger::DBG_ERROR, "Handshake response: %s", resp);
        return false;
    }

    DEBUG(INDI::Logger::DBG_SESSION, "TeenAstroFocuser is online. Getting parameters...");
    return true;
}

bool TeenAstroFocuser::initProperties()
{
    INDI::Focuser::initProperties();
     
    // Relative and absolute positions
    /*
    FocusRelPosN[0].min = 0.;
    FocusRelPosN[0].max = 200.;
    FocusRelPosN[0].value = 0.;
    FocusRelPosN[0].step = 10.;

    FocusAbsPosN[0].min = 0.;
    FocusAbsPosN[0].max = 10000000.;
    FocusAbsPosN[0].value = 0.;
    FocusAbsPosN[0].step = 500.;
    */

    // UI Element: SetZero
    IUFillSwitch(&SyncZeroS[0], "VAL", "Sync", ISS_OFF);
    IUFillSwitchVector(&SyncZeroSP, SyncZeroS, 1, getDeviceName(), "SYNC_ZERO", "Sync zero position", MAIN_CONTROL_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    // Configuration
    //

    IUFillText(&CfgDeviceVersionT[0], "VAL", "Version", "unknown");
    IUFillTextVector(&CfgDeviceVersionTP, CfgDeviceVersionT, 1, getDeviceName(), "DEVICE_VERSION", "Device version", FOCUS_TAB, IP_RO, 0, IPS_IDLE);

    IUFillNumber(&CfgParkPosN[0], "VAL", "Ticks", "%5.0f", 0, 100000., 0, 0);
    IUFillNumberVector(&CfgParkPosNP, CfgParkPosN, 1, getDeviceName(), "PARK_POS", "Park position", FOCUS_TAB, IP_RW, 0, IPS_IDLE );

    // Focuser max position is inherited from superclass as FocusMaxPosN, FocusMaxPosNP

    IUFillNumber(&CfgGoToSpeedN[0], "VAL", "tbd/s", "%3.0f", 1., 1000., 10, 0);
    IUFillNumberVector(&CfgGoToSpeedNP, CfgGoToSpeedN, 1, getDeviceName(), "GOTO_SPEED", "Go-to speed", FOCUS_TAB, IP_RW, 0, IPS_IDLE );

    IUFillNumber(&CfgGoToAccN[0], "VAL", "tbd/s^2", "%3.0f", 1., 1000., 10, 0);
    IUFillNumberVector(&CfgGoToAccNP, CfgGoToAccN, 1, getDeviceName(), "GOTO_ACCEL", "Go-to accel.", FOCUS_TAB, IP_RW, 0, IPS_IDLE );

    IUFillNumber(&CfgManualSpeedN[0], "VAL", "tbd/s", "%3.0f", 1., 1000., 10, 0);
    IUFillNumberVector(&CfgManualSpeedNP, CfgManualSpeedN, 1, getDeviceName(), "MAN_SPEED", "Manual speed", FOCUS_TAB, IP_RW, 0, IPS_IDLE );

    IUFillNumber(&CfgManualAccN[0], "VAL", "tbd/s^2", "%3.0f", 1., 1000., 10, 0);
    IUFillNumberVector(&CfgManualAccNP, CfgManualAccN, 1, getDeviceName(), "MAN_ACCEL", "Manual accel.", FOCUS_TAB, IP_RW, 0, IPS_IDLE );

    IUFillNumber(&CfgManualDecN[0], "VAL", "tbd/s^2", "%8.0f", 1., 1000., 10, 0);
    IUFillNumberVector(&CfgManualDecNP, CfgManualDecN, 1, getDeviceName(), "MAN_DECEL", "Manual decel.", FOCUS_TAB, IP_RW, 0, IPS_IDLE );

    // Motor configuration
    //

    IUFillNumber(&CfgMotorInvertN[0], "VAL", "0=norm. 1=inv.", "%8.0f", 0, 1, 1, 0);
    IUFillNumberVector(&CfgMotorInvertNP, CfgMotorInvertN, 1, getDeviceName(), "MOT_INV", "Motor invert", FOCUS_TAB, IP_RW, 0, IPS_IDLE );

    IUFillNumber(&CfgMotorStepsPerRevolutionN[0], "VAL", "Steps", "%3.0f", 1, 800, 10, 0);
    IUFillNumberVector(&CfgMotorStepsPerRevolutionNP, CfgMotorStepsPerRevolutionN, 1, getDeviceName(), "MOT_STEPS_REV", "Motor steps/rev", FOCUS_TAB, IP_RW, 0, IPS_IDLE );

    IUFillNumber(&CfgMotorMicrostepsN[0], "VAL", "Usteps", "%3.0f", 1, 512, 1, 0);
    IUFillNumberVector(&CfgMotorMicrostepsNP, CfgMotorMicrostepsN, 1, getDeviceName(), "MOT_USTEPS", "Motor usteps", FOCUS_TAB, IP_RW, 0, IPS_IDLE );

    IUFillNumber(&CfgMotorResolutionN[0], "VAL", "Usteps/tick", "%3.0f", 1., 1000., 10, 0);
    IUFillNumberVector(&CfgMotorResolutionNP, CfgMotorResolutionN, 1, getDeviceName(), "MOT_RES", "Motor resolution", FOCUS_TAB, IP_RW, 0, IPS_IDLE );

    IUFillNumber(&CfgMotorCurrentN[0], "VAL", "mA", "%4.0f", 1., 10000., 10, 0);
    IUFillNumberVector(&CfgMotorCurrentNP, CfgMotorCurrentN, 1, getDeviceName(), "MOT_CUR", "Motor current", FOCUS_TAB, IP_RW, 0, IPS_IDLE );

    // Status variables
    //
    
    // Focuser current position is inherited from superclass as FocusAbsPosN, FocusAbsPosNP

    // Current speed
    IUFillNumber(&CurSpeedN[0], "VAL", "tbd/s", "%3.0f", 1., 10000000., 0, 0);
    IUFillNumberVector(&CurSpeedNP, CurSpeedN, 1, getDeviceName(), "CUR_SPEED", "Current Speed", MAIN_CONTROL_TAB, IP_RO, 0, IPS_IDLE );

    // Temperature
    IUFillNumber(&TempN[0], "VAL", "°Celsius", "%+2.2f", -50., 50., 0, 0);
    IUFillNumberVector(&TempNP, TempN, 1, getDeviceName(), "TEMP", "Temperature", MAIN_CONTROL_TAB, IP_RO, 0, IPS_IDLE );

    addDebugControl();

    return true;

}

bool TeenAstroFocuser::updateProperties()
{
    INDI::Focuser::updateProperties();
    if (isConnected())
    {
        defineProperty(&SyncZeroSP);

        defineProperty(&CfgDeviceVersionTP);
        updateDeviceVersion();
        defineProperty(&CfgParkPosNP);
        defineProperty(&CfgGoToSpeedNP);
        defineProperty(&CfgGoToAccNP);
        defineProperty(&CfgManualSpeedNP);
        defineProperty(&CfgManualAccNP);
        defineProperty(&CfgManualDecNP);

        defineProperty(&CfgMotorInvertNP);
        defineProperty(&CfgMotorStepsPerRevolutionNP);
        defineProperty(&CfgMotorMicrostepsNP);
        defineProperty(&CfgMotorResolutionNP);
        defineProperty(&CfgMotorCurrentNP);

        defineProperty(&CurSpeedNP);
        defineProperty(&TempNP);

        updateConfig();
        updateMotorConfig();
        updateState();

        loadConfig(true);

        DEBUG(INDI::Logger::DBG_SESSION, "TeenAstroFocuser parameters updated, focuser ready for use.");
    }
    else
    {
        deleteProperty(SyncZeroSP.name);

        deleteProperty(CfgDeviceVersionTP.name);
        deleteProperty(CfgParkPosNP.name);
        deleteProperty(CfgGoToSpeedNP.name);
        deleteProperty(CfgGoToAccNP.name);
        deleteProperty(CfgManualSpeedNP.name);
        deleteProperty(CfgManualAccNP.name);
        deleteProperty(CfgManualDecNP.name);

        deleteProperty(CfgMotorInvertNP.name);
        deleteProperty(CfgMotorStepsPerRevolutionNP.name);
        deleteProperty(CfgMotorMicrostepsNP.name);
        deleteProperty(CfgMotorResolutionNP.name);
        deleteProperty(CfgMotorCurrentNP.name);

        deleteProperty(CurSpeedNP.name);
        deleteProperty(TempNP.name);

    }

    return true;
}


bool TeenAstroFocuser::ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if(dev==NULL || strcmp(dev,getDeviceName()))
        return INDI::Focuser::ISNewSwitch(dev, name, states, names, n);

    if (!strcmp(name, SyncZeroSP.name))
    {
        SyncZeroSP.s = IPS_BUSY;
        IDSetSwitch(&SyncZeroSP, NULL);

        syncZero();

        IUResetSwitch(&SyncZeroSP);
        SyncZeroSP.s = IPS_OK;
        IDSetSwitch(&SyncZeroSP, NULL);
        return true;
    }
    else 
        return INDI::Focuser::ISNewSwitch(dev, name, states, names, n);
}

bool TeenAstroFocuser::ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n)
{
    if(dev==NULL || strcmp(dev,getDeviceName()))
        return INDI::Focuser::ISNewNumber(dev, name, values, names, n);

    if(!strcmp(name, CfgParkPosNP.name)) 
    {
        IUUpdateNumber(&CfgParkPosNP, values, names, n);               
        if(!setParkPos((uint32_t) CfgParkPosN[0].value))
            return false;
        CfgParkPosNP.s = IPS_OK;
        return true;
    }
    else if(!strcmp(name, FocusMaxPosNP.name))
    {
        if (values[0] < FocusAbsPosN[0].value)
        {
            DEBUGF(INDI::Logger::DBG_ERROR, "Can't set max position lower than current absolute position ( %8.0f )", FocusAbsPosN[0].value);
            return false;
        }
        IUUpdateNumber(&FocusMaxPosNP, values, names, n);               
        FocusAbsPosN[0].max = FocusMaxPosN[0].value;
        setMaxPos(FocusMaxPosN[0].value);
        FocusMaxPosNP.s = IPS_OK;
        return true;
    }
    else if(!strcmp(name, CfgManualSpeedNP.name))
    {
        IUUpdateNumber(&CfgManualSpeedNP, values, names, n);               
        if(!setManualSpeed((uint32_t) CfgManualSpeedN[0].value))
            return false;
        CfgManualSpeedNP.s = IPS_OK;
        return true;
    }
    else if(!strcmp(name, CfgGoToSpeedNP.name))
    {
        IUUpdateNumber(&CfgGoToSpeedNP, values, names, n);               
        if(!setGoToSpeed((uint32_t) CfgGoToSpeedN[0].value))
            return false;
        CfgGoToSpeedNP.s = IPS_OK;
        return true;
    }
    else if(!strcmp(name, CfgGoToAccNP.name))
    {
        IUUpdateNumber(&CfgGoToAccNP, values, names, n);               
        if(!setGoToAcc((uint32_t) CfgGoToAccN[0].value))
            return false;
        CfgGoToAccNP.s = IPS_OK;
        return true;
    }
    else if(!strcmp(name, CfgManualAccNP.name))
    {
        IUUpdateNumber(&CfgManualAccNP, values, names, n);               
        if(!setManualAcc((uint32_t) CfgManualAccN[0].value))
            return false;
        CfgManualAccNP.s = IPS_OK;
        return true;
    }
    else if(!strcmp(name, CfgManualDecNP.name))
    {
        IUUpdateNumber(&CfgManualDecNP, values, names, n);               
        if(!setManualDec((uint32_t) CfgManualDecN[0].value))
            return false;
        CfgManualDecNP.s = IPS_OK;
        return true;
    }
    else if(!strcmp(name, CfgMotorInvertNP.name))
    {
        IUUpdateNumber(&CfgMotorInvertNP, values, names, n);               
        if(!setMotorInvert((uint32_t) CfgMotorInvertN[0].value))
            return false;
        CfgMotorInvertNP.s = IPS_OK;
        return true;
    }
    else if(!strcmp(name, CfgMotorStepsPerRevolutionNP.name))
    {
        IUUpdateNumber(&CfgMotorStepsPerRevolutionNP, values, names, n);               
        if(!setMotorStepsPerRevolution((uint32_t) CfgMotorStepsPerRevolutionN[0].value))
            return false;
        CfgMotorStepsPerRevolutionNP.s = IPS_OK;
        return true;
    }
    else if(!strcmp(name, CfgMotorMicrostepsNP.name))
    {
        IUUpdateNumber(&CfgMotorMicrostepsNP, values, names, n);               
        if(!setMotorMicrosteps((uint32_t) CfgMotorMicrostepsN[0].value))
            return false;
        CfgMotorMicrostepsNP.s = IPS_OK;
        return true;
    }
    else if(!strcmp(name, CfgMotorResolutionNP.name))
    {
        IUUpdateNumber(&CfgMotorResolutionNP, values, names, n);               
        if(!setMotorResolution((uint32_t) CfgMotorResolutionN[0].value))
            return false;
        CfgMotorResolutionNP.s = IPS_OK;
        return true;
    }
    else if(!strcmp(name, CfgMotorCurrentNP.name))
    {
        IUUpdateNumber(&CfgMotorCurrentNP, values, names, n);               
        if(!setMotorCurrent((uint32_t) CfgMotorCurrentN[0].value))
            return false;
        CfgMotorCurrentNP.s = IPS_OK;
        return true;
    }
    else
        return INDI::Focuser::ISNewNumber(dev, name, values, names, n);
}


IPState TeenAstroFocuser::MoveAbsFocuser(uint32_t pos)
{
    if(!goTo(pos))
        return IPS_ALERT;

    FocusAbsPosNP.s = IPS_BUSY;
    return IPS_BUSY;
}

IPState TeenAstroFocuser::MoveRelFocuser(FocusDirection dir, uint32_t ticks)
{
    uint32_t pos=uint32_t(FocusAbsPosN[0].value);
    if (dir == FOCUS_INWARD)
        pos -= ticks;
    else
        pos += ticks;

    if(!goTo(pos))
        return IPS_ALERT;

    FocusRelPosN[0].value = ticks;
    FocusRelPosNP.s = IPS_BUSY;
    return IPS_BUSY;
}

void TeenAstroFocuser::TimerHit()
{
    if (isConnected() == false)
    {
        SetTimer(POLLMS_OVERRIDE);
        return;
    }

    if (updateState())
    {
        if (fabs(lastPos - FocusAbsPosN[0].value) > 5)
        {
            IDSetNumber(&FocusAbsPosNP, NULL);
            lastPos = FocusAbsPosN[0].value;
        }
    }

    if (FocusAbsPosNP.s == IPS_BUSY || FocusRelPosNP.s == IPS_BUSY)
    {
        if (isMoving() == false)
        {
            FocusAbsPosNP.s = IPS_OK;
            FocusRelPosNP.s = IPS_OK;
            IDSetNumber(&FocusAbsPosNP, NULL);
            IDSetNumber(&FocusRelPosNP, NULL);
            lastPos = FocusAbsPosN[0].value;
            DEBUG(INDI::Logger::DBG_SESSION, "Focuser reached target position.");
        }
    }
    SetTimer(POLLMS_OVERRIDE);
}

bool TeenAstroFocuser::AbortFocuser()
{
    if(!stop())
        return false;

    FocusAbsPosNP.s = IPS_IDLE;
    FocusRelPosNP.s = IPS_IDLE;
    IDSetNumber(&FocusAbsPosNP, NULL);
    IDSetNumber(&FocusRelPosNP, NULL);
    return true;
}


// Private methods
//

bool TeenAstroFocuser::send(const char *const msg) {
    int nbytes_written=0, rc=-1;
    if ( (rc = tty_write(PortFD, msg, strlen(msg), &nbytes_written)) != TTY_OK)
    {
        char errstr[MAXRBUF];
        tty_error_msg(rc, errstr, MAXRBUF);
        DEBUGF(INDI::Logger::DBG_ERROR, "Send error: %s.", errstr);
        return false;
    }

    tcflush(PortFD, TCIOFLUSH);
    return true;
}


bool TeenAstroFocuser::sendAndReceive(const char *const msg, char *resp) {
    if(!send(msg))
        return false;

    int nbytes_read=0, rc=-1;
    if ( (rc = tty_read_section(PortFD, resp, '#', TEENASTRO_FOCUSER_TIMEOUT, &nbytes_read)) != TTY_OK)
    {
        char errstr[MAXRBUF];
        tty_error_msg(rc, errstr, MAXRBUF);
        DEBUGF(INDI::Logger::DBG_ERROR, "Receive error: %s.", errstr);
        resp[0]='\0';
        return false;
    }
    resp[nbytes_read]='\0';

    return true;
}

bool TeenAstroFocuser::sendAndReceiveBool(const char *const msg) {
    if(!send(msg))
        return false;

    char resp[2];
    int nbytes_read=0, rc=-1;
    if ( (rc = tty_read(PortFD, resp, 1, TEENASTRO_FOCUSER_TIMEOUT, &nbytes_read)) != TTY_OK)
    {
        char errstr[MAXRBUF];
        tty_error_msg(rc, errstr, MAXRBUF);
        DEBUGF(INDI::Logger::DBG_ERROR, "Receive error: %s.", errstr);
        resp[0]='\0';
        return false;
    }
    resp[nbytes_read]='\0';

    return resp[0]=='1';
}


bool TeenAstroFocuser::updateDeviceVersion()
{
    char resp[128];
    if(!sendAndReceive(":FV#", resp))
        return false;
    int len=strlen(resp);
    resp[len-1]=0;
    IUSaveText(&CfgDeviceVersionT[0], resp+2);
    CfgDeviceVersionTP.s = IPS_OK;
    IDSetText(&CfgDeviceVersionTP, NULL);
    return true;
}

bool TeenAstroFocuser::updateState()
{
    char resp[128];
    int pos=-1, speed=-1;
    float temp=-1;

    if(!sendAndReceive(":F?#", resp))
        return false;

    if(sscanf(resp, "?%d %d %f#", &pos, &speed, &temp)<=0)
    {
        DEBUGF(INDI::Logger::DBG_ERROR, "Invalid format: focuser state (%s)", resp);
        return false;
    }

    FocusAbsPosN[0].value = pos;
    FocusAbsPosNP.s = IPS_OK;
    IDSetNumber(&FocusAbsPosNP, NULL);
    CurSpeedN[0].value = speed;
    CurSpeedNP.s = IPS_OK;
    IDSetNumber(&CurSpeedNP, NULL);
    TempN[0].value = temp;
    TempNP.s = IPS_OK;
    IDSetNumber(&TempNP, NULL);

    return true;
}

bool TeenAstroFocuser::isMoving()
{
    if(!updateState())
        return false;
    return CurSpeedN[0].value > 0;
}


bool TeenAstroFocuser::updateConfig() 
{
    char resp[128];
    int parkPos=-1, maxPos=-1, manualSpeed=-1, goToSpeed=-1, gotoAcc=-1, manAcc=-1, manDec=-1;

    if(!sendAndReceive(":F~#", resp))
        return false;

    if(sscanf(resp, "~%d %d %d %d %d %d %d#", &parkPos, &maxPos, &manualSpeed, &goToSpeed, &gotoAcc, &manAcc, &manDec)<=0)
    {
        DEBUGF(INDI::Logger::DBG_ERROR, "Invalid format: focuser state: %s", resp);
        return false;
    }

    CfgParkPosN[0].value = parkPos;
    CfgParkPosNP.s = IPS_OK;
    IDSetNumber(&CfgParkPosNP, NULL);
    FocusMaxPosN[0].value = maxPos;
    FocusMaxPosNP.s = IPS_OK;
    IDSetNumber(&FocusMaxPosNP, NULL);
    CfgManualSpeedN[0].value = manualSpeed;
    CfgManualSpeedNP.s = IPS_OK;
    IDSetNumber(&CfgManualSpeedNP, NULL);
    CfgGoToSpeedN[0].value = goToSpeed;
    CfgGoToSpeedNP.s = IPS_OK;
    IDSetNumber(&CfgGoToSpeedNP, NULL);
    CfgGoToAccN[0].value = gotoAcc;
    CfgGoToAccNP.s = IPS_OK;
    IDSetNumber(&CfgGoToAccNP, NULL);
    CfgManualAccN[0].value = manAcc;
    CfgManualAccNP.s = IPS_OK;
    IDSetNumber(&CfgManualAccNP, NULL);
    CfgManualDecN[0].value = manDec;
    CfgManualDecNP.s = IPS_OK;
    IDSetNumber(&CfgManualDecNP, NULL);

    // Update focuser absolute position limits
    FocusAbsPosN[0].max = maxPos;
    IDSetNumber(&FocusAbsPosNP, NULL);

    return true;
}

bool TeenAstroFocuser::setConfigItem(char item, uint32_t deviceValue)
{
    char cmd[64];
    snprintf(cmd, 64, ":F%c,%d#", item, deviceValue);
    if(!sendAndReceiveBool(cmd))
        return false;
    updateConfig();
    return true;
}

bool TeenAstroFocuser::setParkPos(uint32_t value)
{
    return setConfigItem('0', value);
}

bool TeenAstroFocuser::setMaxPos(uint32_t value)
{
    return setConfigItem('1', value);
}

bool TeenAstroFocuser::setManualSpeed(uint32_t value)
{
    return setConfigItem('2', value);
}

bool TeenAstroFocuser::setGoToSpeed(uint32_t value)
{
    return setConfigItem('3', value);
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
    char resp[128];
    int invert=-1, log2_micro=-1, resolution=-1, curr_10ma=-1, steprot=-1;

    if(!sendAndReceive(":FM#", resp))
        return false;

    if(sscanf(resp, "M%d %d %d %d %d#", &invert, &log2_micro, &resolution, &curr_10ma, &steprot)<=0)
    {
        DEBUGF(INDI::Logger::DBG_ERROR, "Invalid format: focuser state (%s)", resp);
        return false;
    }

    CfgMotorInvertN[0].value = invert;
    CfgMotorInvertNP.s = IPS_OK;
    IDSetNumber(&CfgMotorInvertNP, NULL);
    CfgMotorMicrostepsN[0].value = 1<<log2_micro;  // TeenAstro device returns and expects log_2(microsteps)
    CfgMotorMicrostepsNP.s = IPS_OK;
    IDSetNumber(&CfgMotorMicrostepsNP, NULL);
    CfgMotorResolutionN[0].value = resolution;
    CfgMotorResolutionNP.s = IPS_OK;
    IDSetNumber(&CfgMotorResolutionNP, NULL);
    CfgMotorCurrentN[0].value = curr_10ma * 10;    // TeenAstro device returns and expects units of 10 mA
    CfgMotorCurrentNP.s = IPS_OK;
    IDSetNumber(&CfgMotorCurrentNP, NULL);
    CfgMotorStepsPerRevolutionN[0].value = steprot;
    CfgMotorStepsPerRevolutionNP.s = IPS_OK;
    IDSetNumber(&CfgMotorStepsPerRevolutionNP, NULL);

    return true;
}

bool TeenAstroFocuser::setMotorInvert(uint32_t value)
{
    return setConfigItem('7', value);
}

bool TeenAstroFocuser::setMotorMicrosteps(uint32_t value)
{
    uint32_t bitPos=0;
    value>>=1;
    for(; value!=0; bitPos++)
        value>>=1;
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

bool TeenAstroFocuser::syncZero()
{
    return sendAndReceiveBool(":FS#");  
}


