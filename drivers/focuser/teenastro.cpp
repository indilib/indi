/*
    TeenAstro Focuser
    Copyright (C) 2021 Markus Noga
    Derived from the below, and hereby made available under the same license.

    TeenAstroFocuser Focuser
    Copyright (C) 2018 Paul de Backer (74.0632@gmail.com)

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


std::unique_ptr<TeenAstroFocuser> teenAstroFocuser(new TeenAstroFocuser());

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
    IUFillSwitch(&SetZeroS[0], "VAL", "Sync current position to 0", ISS_OFF);
    IUFillSwitchVector(&SetZeroSP, SetZeroS, 1, getDeviceName(), "SYNC_ZERO", "Sync current position to 0", MAIN_CONTROL_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    // Configuration
    //

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
        defineProperty(&SetZeroSP);

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
        deleteProperty(SetZeroSP.name);

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


bool TeenAstroFocuser::Send(const char *const msg) {
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


bool TeenAstroFocuser::SendAndReceive(const char *const msg, char *resp) {
    if(!Send(msg))
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

bool TeenAstroFocuser::Handshake()
{
    sleep(2);

    char resp[128];
    if(!SendAndReceive(":FV#", resp))
        return false;

    int l=strlen(resp);
    if (strncmp(resp, "$ TeenAstro Focuser ",20) || resp[l-1]!='#')
    {
        DEBUGF(INDI::Logger::DBG_ERROR, "Handshake response: %s", resp);
        return false;
    }

    DEBUG(INDI::Logger::DBG_SESSION, "TeenAstroFocuser is online. Getting focus parameters...");
    return true;
}


bool TeenAstroFocuser::updateState()
{
    char resp[128];
    int pos=-1, speed=-1;
    float temp=-1;

    if(!SendAndReceive(":F?#", resp))
        return false;

    if(sscanf(resp, "?%d %d %f#", &pos, &speed, &temp)<=0)
    {
        DEBUGF(INDI::Logger::DBG_ERROR, "Invalid format: focuser state (%s)", resp);
        return false;
    }

    FocusAbsPosN[0].value = pos;
    IDSetNumber(&FocusAbsPosNP, NULL);
    CurSpeedN[0].value = speed;
    IDSetNumber(&CurSpeedNP, NULL);
    TempN[0].value = temp;
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
    int parkPos=-1, maxPos=-1, goToSpeed=-1, manualSpeed=-1, gotoAcc=-1, manAcc=-1, manDec=-1;

    if(!SendAndReceive(":F~#", resp))
        return false;

    if(sscanf(resp, "~%d %d %d %d %d %d %d#", &parkPos, &maxPos, &goToSpeed, &manualSpeed, &gotoAcc, &manAcc, &manDec)<=0)
    {
        DEBUGF(INDI::Logger::DBG_ERROR, "Invalid format: focuser state: %s", resp);
        return false;
    }

    CfgParkPosN[0].value = parkPos;
    IDSetNumber(&CfgParkPosNP, NULL);
    FocusMaxPosN[0].value = maxPos;
    IDSetNumber(&FocusMaxPosNP, NULL);
    CfgGoToSpeedN[0].value = goToSpeed;
    IDSetNumber(&CfgGoToSpeedNP, NULL);
    CfgManualSpeedN[0].value = manualSpeed;
    IDSetNumber(&CfgManualSpeedNP, NULL);
    CfgGoToAccN[0].value = gotoAcc;
    IDSetNumber(&CfgGoToAccNP, NULL);
    CfgManualAccN[0].value = manAcc;
    IDSetNumber(&CfgManualAccNP, NULL);
    CfgManualDecN[0].value = manDec;
    IDSetNumber(&CfgManualDecNP, NULL);

    // Update focuser absolute position limits
    FocusAbsPosN[0].max = maxPos;
    IDSetNumber(&FocusAbsPosNP, NULL);

    return true;
}


bool TeenAstroFocuser::updateMotorConfig() 
{
    char resp[128];
    int invert=-1, micro=-1, resolution=-1, curr=-1, steprot=-1;

    if(!SendAndReceive(":FM#", resp))
        return false;

    if(sscanf(resp, "M%d %d %d %d %d#", &invert, &micro, &resolution, &curr, &steprot)<=0)
    {
        DEBUGF(INDI::Logger::DBG_ERROR, "Invalid format: focuser state (%s)", resp);
        return false;
    }

    CfgMotorInvertN[0].value = invert;
    IDSetNumber(&CfgMotorInvertNP, NULL);
    CfgMotorMicrostepsN[0].value = micro;
    IDSetNumber(&CfgMotorMicrostepsNP, NULL);
    CfgMotorResolutionN[0].value = resolution;
    IDSetNumber(&CfgMotorResolutionNP, NULL);
    CfgMotorCurrentN[0].value = curr * 10;   // TeenAstro device returns and expects units of 10 mA
    IDSetNumber(&CfgMotorCurrentNP, NULL);
    CfgMotorStepsPerRevolutionN[0].value = steprot;
    IDSetNumber(&CfgMotorStepsPerRevolutionNP, NULL);

    return true;
}

bool TeenAstroFocuser::goTo(uint32_t position)
{
    char cmd[64];
    snprintf(cmd, 64, ":FG,%d#", position);
    return Send(cmd);
}

void TeenAstroFocuser::setZero()
{
    Send(":FS#"); // ignoring errors, as they are already logged 
}


bool TeenAstroFocuser::setMaxPos(uint32_t maxPos)
{
    char cmd[64];
    snprintf(cmd, 64, ":F1,%d#", maxPos);
    if(!Send(cmd))
        return false;

    updateConfig();
    return true;
}

bool TeenAstroFocuser::ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if(strcmp(dev,getDeviceName()))
        return INDI::Focuser::ISNewSwitch(dev, name, states, names, n);

    if (!strcmp(name, SetZeroSP.name))
    {
        setZero();
        IUResetSwitch(&SetZeroSP);
        SetZeroSP.s = IPS_OK;
        IDSetSwitch(&SetZeroSP, NULL);
        return true;
    }
        
    return INDI::Focuser::ISNewSwitch(dev, name, states, names, n);
}

bool TeenAstroFocuser::ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n)
{
    if(strcmp(dev,getDeviceName())==0)
        return INDI::Focuser::ISNewNumber(dev, name, values, names, n);

    if (!strcmp(name, FocusMaxPosNP.name))
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
    if(!Send(":FQ#"))
        return false;

    FocusAbsPosNP.s = IPS_IDLE;
    FocusRelPosNP.s = IPS_IDLE;
    IDSetNumber(&FocusAbsPosNP, NULL);
    IDSetNumber(&FocusRelPosNP, NULL);
    return true;
}


