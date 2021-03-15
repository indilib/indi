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

#define TAF_UI_STEPS    20.0
#define TAF_STEP(min,max)  (((max)-(min))/(TAF_UI_STEPS))


#define TEENASTRO_FOCUSER_TIMEOUT 4
#define TEENASTRO_FOCUSER_BUFSIZE 128

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

void ISNewBLOB(const char *dev, const char *name, int sizes[], int blobsizes[], char *blobs[], char *formats[], char *names[], int n)
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

void ISSnoopDevice(XMLEle *root)
{
     teenAstroFocuser->ISSnoopDevice(root);
}


// Public methods
//

TeenAstroFocuser::TeenAstroFocuser()
{
    FI::SetCapability(FOCUSER_CAN_ABS_MOVE | FOCUSER_CAN_REL_MOVE | FOCUSER_CAN_ABORT | FOCUSER_CAN_SYNC);
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

    char resp[TEENASTRO_FOCUSER_BUFSIZE];
    if(!sendAndReceive(":FV#", resp, TEENASTRO_FOCUSER_BUFSIZE))
        return false;
    if (strncmp(resp, "$ TeenAstro Focuser ",20))
    {
        DEBUGF(INDI::Logger::DBG_ERROR, "Handshake response: %s", resp);
        return false;
    }

    DEBUG(INDI::Logger::DBG_SESSION, "TeenAstroFocuser found, updating parameters...");
    return true;
}

bool TeenAstroFocuser::initProperties()
{
    INDI::Focuser::initProperties();
     
    IUFillText(&DeviceVersionT[0], "VAL", "Version", "unknown");
    IUFillTextVector(&DeviceVersionTP, DeviceVersionT, 1, getDeviceName(), "DEVICE_VERSION", "Device version", FOCUS_TAB, IP_RO, 60, IPS_IDLE);

    // Configuration
    //

    IUFillNumber(&CfgParkPosN[0], "VAL", "Ticks", "%5.0f", 0, 100000, 1000, 0);
    IUFillNumberVector(&CfgParkPosNP, CfgParkPosN, 1, getDeviceName(), "PARK_POS", "Park position", MAIN_CONTROL_TAB, IP_RW, 60, IPS_IDLE );

    IUFillSwitch(&GoToParkS[0], "VAL", "Park", ISS_OFF);
    IUFillSwitchVector(&GoToParkSP, GoToParkS, 1, getDeviceName(), "GOTO_PARK", "Go-to park", MAIN_CONTROL_TAB, IP_RW, ISR_ATMOST1, 60, IPS_IDLE );

    IUFillNumber(&CfgGoToSpeedN[0], "VAL", "1/s", "%3.0f", TAF_speed_min, TAF_speed_max, TAF_STEP(TAF_speed_min, TAF_speed_max), TAF_speed_default);
    IUFillNumberVector(&CfgGoToSpeedNP, CfgGoToSpeedN, 1, getDeviceName(), "GOTO_SPEED", "Go-to speed", FOCUS_TAB, IP_RW, 60, IPS_IDLE );

    IUFillNumber(&CfgGoToAccN[0], "VAL", "1/s^2", "%3.0f", TAF_acc_min, TAF_acc_max, TAF_STEP(TAF_acc_min, TAF_acc_max), TAF_acc_default);
    IUFillNumberVector(&CfgGoToAccNP, CfgGoToAccN, 1, getDeviceName(), "GOTO_ACCEL", "Go-to accel.", FOCUS_TAB, IP_RW, 60, IPS_IDLE );

    IUFillNumber(&CfgManualSpeedN[0], "VAL", "1/s", "%3.0f", TAF_speed_min, TAF_speed_max, TAF_STEP(TAF_speed_min, TAF_speed_max), TAF_speed_default);
    IUFillNumberVector(&CfgManualSpeedNP, CfgManualSpeedN, 1, getDeviceName(), "MAN_SPEED", "Manual speed", FOCUS_TAB, IP_RW, 60, IPS_IDLE );

    IUFillNumber(&CfgManualAccN[0], "VAL", "1/s^2", "%3.0f", TAF_acc_min, TAF_acc_max, TAF_STEP(TAF_acc_min, TAF_acc_max), TAF_acc_default);
    IUFillNumberVector(&CfgManualAccNP, CfgManualAccN, 1, getDeviceName(), "MAN_ACCEL", "Manual accel.", FOCUS_TAB, IP_RW, 60, IPS_IDLE );

    IUFillNumber(&CfgManualDecN[0], "VAL", "1/s^2", "%8.0f", TAF_acc_min, TAF_acc_max, TAF_STEP(TAF_acc_min, TAF_acc_max), TAF_acc_default);
    IUFillNumberVector(&CfgManualDecNP, CfgManualDecN, 1, getDeviceName(), "MAN_DECEL", "Manual decel.", FOCUS_TAB, IP_RW, 60, IPS_IDLE );

    // Motor configuration
    //

    IUFillNumber(&CfgMotorInvertN[0], "VAL", "0=norm. 1=inv.", "%8.0f", 0, 1, 1, 0);
    IUFillNumberVector(&CfgMotorInvertNP, CfgMotorInvertN, 1, getDeviceName(), "MOT_INV", "Motor invert", FOCUS_TAB, IP_RW, 60, IPS_IDLE );

    IUFillNumber(&CfgMotorStepsPerRevolutionN[0], "VAL", "Steps", "%3.0f", TAF_steprot_min, TAF_steprot_max, TAF_STEP(TAF_steprot_min, TAF_steprot_max), TAF_steprot_default);
    IUFillNumberVector(&CfgMotorStepsPerRevolutionNP, CfgMotorStepsPerRevolutionN, 1, getDeviceName(), "MOT_STEPS_REV", "Motor steps/rev", FOCUS_TAB, IP_RW, 60, IPS_IDLE );

    IUFillNumber(&CfgMotorMicrostepsN[0], "VAL", "Usteps", "%3.0f", TAF_micro_min, TAF_micro_max, TAF_STEP(TAF_micro_min, TAF_micro_max), TAF_micro_default);
    IUFillNumberVector(&CfgMotorMicrostepsNP, CfgMotorMicrostepsN, 1, getDeviceName(), "MOT_USTEPS", "Motor usteps", FOCUS_TAB, IP_RW, 60, IPS_IDLE );

    IUFillNumber(&CfgMotorResolutionN[0], "VAL", "Usteps/tick", "%3.0f", TAF_res_min, TAF_res_max, TAF_STEP(TAF_res_min, TAF_res_max), TAF_res_default);
    IUFillNumberVector(&CfgMotorResolutionNP, CfgMotorResolutionN, 1, getDeviceName(), "MOT_RES", "Motor resolution", FOCUS_TAB, IP_RW, 60, IPS_IDLE );

    IUFillNumber(&CfgMotorCurrentN[0], "VAL", "mA", "%4.0f", TAF_curr_min, TAF_curr_max, TAF_STEP(TAF_curr_min, TAF_curr_max), TAF_curr_default);
    IUFillNumberVector(&CfgMotorCurrentNP, CfgMotorCurrentN, 1, getDeviceName(), "MOT_CUR", "Motor current", FOCUS_TAB, IP_RW, 60, IPS_IDLE );

    // Status variables
    //
    
    // Current speed
    IUFillNumber(&CurSpeedN[0], "VAL", "tbd/s", "%3.0f", TAF_speed_min, TAF_speed_max, TAF_STEP(TAF_speed_min, TAF_speed_max), TAF_speed_default);
    IUFillNumberVector(&CurSpeedNP, CurSpeedN, 1, getDeviceName(), "CUR_SPEED", "Current Speed", MAIN_CONTROL_TAB, IP_RO, 0, IPS_IDLE );

    // Temperature
    IUFillNumber(&TempN[0], "VAL", "°Celsius", "%+2.1f", -50., 50., 0, 0);
    IUFillNumberVector(&TempNP, TempN, 1, getDeviceName(), "TEMP", "Temperature", MAIN_CONTROL_TAB, IP_RO, 0, IPS_IDLE );

    addDebugControl();

    return true;

}

bool TeenAstroFocuser::updateProperties()
{
    INDI::Focuser::updateProperties();
    if (isConnected())
    {
        defineProperty(&DeviceVersionTP);

        defineProperty(&GoToParkSP);
        defineProperty(&CfgParkPosNP);

		deleteProperty(FocusSyncNP.name);
		defineProperty(&FocusSyncNP);

		deleteProperty(FocusMaxPosNP.name);
		defineProperty(&FocusMaxPosNP);

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

        updateDeviceVersion();
        updateConfig();
        updateMotorConfig();
        updateState();

        DEBUG(INDI::Logger::DBG_SESSION, "TeenAstroFocuser ready for use.");
    }
    else
    {
        deleteProperty(DeviceVersionTP.name);

        deleteProperty(GoToParkSP.name);
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

	if(!strcmp(name, GoToParkSP.name)) {
		GoToParkSP.s=IPS_BUSY;
		IDSetSwitch(&GoToParkSP, NULL);
		bool res=goToPark();
		GoToParkS[0].s=ISS_OFF;
		GoToParkSP.s= res ? IPS_OK : IPS_ALERT;
		IDSetSwitch(&GoToParkSP, NULL);
		return res;
	}

    return INDI::Focuser::ISNewSwitch(dev, name, states, names, n);
}

bool TeenAstroFocuser::ISNewNumberHelper(INumberVectorProperty *NP, double values[], char *names[], int n, bool res) 
{
	if(res)
		IUUpdateNumber(NP, values, names, n);               
	NP->s=res ? IPS_OK : IPS_ALERT;
	IDSetNumber(NP, NULL);
	return res;
}

bool TeenAstroFocuser::ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n)
{
	if(dev==NULL || strcmp(dev,getDeviceName()))
		return INDI::Focuser::ISNewNumber(dev, name, values, names, n);

	if(!strcmp(name, FocusSyncNP.name)) 
    {
        bool res=SyncFocuser((uint32_t) rint(values[0]));
        return ISNewNumberHelper(&FocusSyncNP, values, names, n, res);
    }
	else if(!strcmp(name, CfgParkPosNP.name)) 
    {
        bool res=setParkPos((uint32_t) rint(values[0]));
        return ISNewNumberHelper(&CfgParkPosNP, values, names, n, res);
    }
 	else if(!strcmp(name, FocusMaxPosNP.name))
    {
        bool res=SetFocuserMaxPosition((uint32_t) rint(values[0]));
        return ISNewNumberHelper(&FocusMaxPosNP, values, names, n, res);
    }
    else if(!strcmp(name, CfgManualSpeedNP.name))
    {
        bool res=setManualSpeed((uint32_t) rint(values[0]));
        return ISNewNumberHelper(&CfgManualSpeedNP, values, names, n, res);
    }
    else if(!strcmp(name, CfgGoToSpeedNP.name))
    {
        bool res=setGoToSpeed((uint32_t) rint(values[0]));
        return ISNewNumberHelper(&CfgGoToSpeedNP, values, names, n, res);
    }
    else if(!strcmp(name, CfgGoToAccNP.name))
    {
        bool res=setGoToAcc((uint32_t) rint(values[0]));
        return ISNewNumberHelper(&CfgGoToAccNP, values, names, n, res);
    }
    else if(!strcmp(name, CfgManualAccNP.name))
    {
        bool res=setManualAcc((uint32_t) rint(values[0]));
        return ISNewNumberHelper(&CfgManualAccNP, values, names, n, res);
    }
    else if(!strcmp(name, CfgManualDecNP.name))
    {
        bool res=setManualDec((uint32_t) rint(values[0]));
        return ISNewNumberHelper(&CfgManualDecNP, values, names, n, res);
    }
    else if(!strcmp(name, CfgMotorInvertNP.name))
    {
        bool res=setMotorInvert((uint32_t) rint(values[0]));
        return ISNewNumberHelper(&CfgMotorInvertNP, values, names, n, res);
    }
    else if(!strcmp(name, CfgMotorStepsPerRevolutionNP.name))
    {
        bool res=setMotorStepsPerRevolution((uint32_t) rint(values[0]));
        return ISNewNumberHelper(&CfgMotorStepsPerRevolutionNP, values, names, n, res);
    }
    else if(!strcmp(name, CfgMotorMicrostepsNP.name))
    {
        bool res=setMotorMicrosteps((uint32_t) rint(values[0]));
        return ISNewNumberHelper(&CfgMotorMicrostepsNP, values, names, n, res);
    }
    else if(!strcmp(name, CfgMotorResolutionNP.name))
    {
        bool res=setMotorResolution((uint32_t) rint(values[0]));
        return ISNewNumberHelper(&CfgMotorResolutionNP, values, names, n, res);
    }
    else if(!strcmp(name, CfgMotorCurrentNP.name))
    {
        bool res=setMotorCurrent((uint32_t) rint(values[0]));
        return ISNewNumberHelper(&CfgMotorCurrentNP, values, names, n, res);
    }
    else
        return INDI::Focuser::ISNewNumber(dev, name, values, names, n);  // handle maxPos, sync, ...
}


IPState TeenAstroFocuser::MoveAbsFocuser(uint32_t pos)
{
    FocusAbsPosNP.s = IPS_BUSY;
    IDSetNumber(&FocusAbsPosNP, NULL);
    FocusRelPosNP.s = IPS_BUSY;
    IDSetNumber(&FocusRelPosNP, NULL);

    if(!goTo(pos)) {
	    FocusAbsPosNP.s = IPS_ALERT;
	    IDSetNumber(&FocusAbsPosNP, NULL);
	    FocusRelPosNP.s = IPS_ALERT;
	    IDSetNumber(&FocusRelPosNP, NULL);

        return IPS_ALERT;
    }
    return IPS_BUSY;
}

IPState TeenAstroFocuser::MoveRelFocuser(FocusDirection dir, uint32_t ticks)
{
    FocusRelPosN[0].value = ticks;

    uint32_t pos=uint32_t(FocusAbsPosN[0].value);
    if (dir == FOCUS_INWARD)
        pos -= ticks;
    else
        pos += ticks;

    return MoveAbsFocuser(pos);
}

void TeenAstroFocuser::TimerHit()
{
	if(isConnected())
		updateState();
    SetTimer(POLLMS_OVERRIDE);
}

bool TeenAstroFocuser::AbortFocuser()
{
	FocusAbortSP.s=IPS_BUSY;
	IDSetSwitch(&FocusAbortSP, NULL);

    if(!stop()) {
		FocusAbortSP.s=IPS_ALERT;
		IDSetSwitch(&FocusAbortSP, NULL);
        return false;
    }
	FocusAbortSP.s=IPS_OK;
	IDSetSwitch(&FocusAbortSP, NULL);

    return updateState();
}


// Protected methods
//

bool TeenAstroFocuser::send(const char *const msg) {
    DEBUGF(INDI::Logger::DBG_DEBUG, "send(\"%s\")", msg);

    int nbytes_written=0, rc=-1;
    if ( (rc = tty_write(PortFD, msg, strlen(msg), &nbytes_written)) != TTY_OK)
    {
        char errstr[MAXRBUF];
        tty_error_msg(rc, errstr, MAXRBUF);
        DEBUGF(INDI::Logger::DBG_ERROR, "Send error: %s.", errstr);
        return false;
    }
    return true;
}


bool TeenAstroFocuser::sendAndReceive(const char *const msg, char *resp, int bufsize) {
    if(!send(msg))
        return false;

    int nbytes_read=0;
    int rc = tty_nread_section(PortFD, resp, bufsize, '#', TEENASTRO_FOCUSER_TIMEOUT, &nbytes_read);
    resp[nbytes_read]='\0';
	if(rc!=TTY_OK || nbytes_read==0 || resp[nbytes_read-1]!='#')
    {
        char errstr[MAXRBUF];
        tty_error_msg(rc, errstr, MAXRBUF);
        DEBUGF(INDI::Logger::DBG_ERROR, "sendAndReceive(\"%s\"): got \"%s\": receive error: %s.", msg, resp, errstr);
        return false;
    }
    DEBUGF(INDI::Logger::DBG_DEBUG, "sendAndReceive(\"%s\"): got \"%s\"", msg, resp);
    return true;
}

bool TeenAstroFocuser::sendAndReceiveBool(const char *const msg) {
    if(!send(msg))
        return false;

    char resp[2];
    int nbytes_read=0;
    int rc = tty_read(PortFD, resp, 1, TEENASTRO_FOCUSER_TIMEOUT, &nbytes_read);
    resp[nbytes_read]='\0';
    if(rc!=TTY_OK || resp[0]!='1')
    {
        char errstr[MAXRBUF];
        tty_error_msg(rc, errstr, MAXRBUF);
        DEBUGF(INDI::Logger::DBG_ERROR, "sendAndReceiveBool(\"%s\"): got \"%s\": receive error: %s.", msg, resp, errstr);
        return false;
    }
    DEBUGF(INDI::Logger::DBG_DEBUG, "sendAndReceiveBool(\"%s\"): got \"%s\"", msg, resp);
    return resp[0]=='1';
}


bool TeenAstroFocuser::updateDeviceVersion()
{
    char resp[TEENASTRO_FOCUSER_BUFSIZE];
    if(!sendAndReceive(":FV#", resp, TEENASTRO_FOCUSER_BUFSIZE))
        return false;
    int len=strlen(resp);
    resp[len-1]=0;
    IUSaveText(&DeviceVersionT[0], resp+2);
    DeviceVersionTP.s = IPS_OK;
    IDSetText(&DeviceVersionTP, NULL);
    return true;
}

bool TeenAstroFocuser::updateState()
{
    char resp[TEENASTRO_FOCUSER_BUFSIZE];
    int pos=-1, speed=-1;
    float temp=-1;

    if(!sendAndReceive(":F?#", resp, TEENASTRO_FOCUSER_BUFSIZE))
        return false;

    if(sscanf(resp, "?%d %d %f#", &pos, &speed, &temp)<=0)
    {
        DEBUGF(INDI::Logger::DBG_ERROR, "Invalid format: focuser state (%s)", resp);
        return false;
    }

    if(FocusAbsPosNP.s==IPS_BUSY && speed==0)
        DEBUG(INDI::Logger::DBG_SESSION, "Focuser reached target position.");

    FocusAbsPosN[0].value = pos;
    FocusAbsPosNP.s = speed>0 ? IPS_BUSY : IPS_OK;
    IDSetNumber(&FocusAbsPosNP, NULL);
    FocusRelPosNP.s = speed>0 ? IPS_BUSY : IPS_OK;
    IDSetNumber(&FocusRelPosNP, NULL);

    CurSpeedN[0].value = speed;
    CurSpeedNP.s = speed>0 ? IPS_BUSY : IPS_OK;
    IDSetNumber(&CurSpeedNP, NULL);
    TempN[0].value = temp;
    TempNP.s = (temp==-99) ? IPS_ALERT : IPS_OK;
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
    char resp[TEENASTRO_FOCUSER_BUFSIZE];
    int parkPos=-1, maxPos=-1, manualSpeed=-1, goToSpeed=-1, gotoAcc=-1, manAcc=-1, manDec=-1;

    if(!sendAndReceive(":F~#", resp, TEENASTRO_FOCUSER_BUFSIZE))
        return false;

    if(sscanf(resp, "~%d %d %d %d %d %d %d#", &parkPos, &maxPos, &manualSpeed, &goToSpeed, &gotoAcc, &manAcc, &manDec)<=0)
    {
        DEBUGF(INDI::Logger::DBG_ERROR, "Invalid format: focuser state: %s", resp);
        return false;
    }

    // Update proper values
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

    // Update UI control maximum values for focuser positions
    FocusAbsPosN[0].max = maxPos;
    FocusAbsPosN[0].step = maxPos / TAF_UI_STEPS;
    IUUpdateMinMax(&FocusAbsPosNP);
    FocusRelPosN[0].max = maxPos;
    FocusRelPosN[0].step = maxPos / TAF_UI_STEPS;
    IUUpdateMinMax(&FocusRelPosNP);
    FocusSyncN[0].max = maxPos;
    FocusSyncN[0].step = maxPos / TAF_UI_STEPS;
    IUUpdateMinMax(&FocusSyncNP);
    CfgParkPosN[0].max = maxPos;
    CfgParkPosN[0].step = maxPos / TAF_UI_STEPS;
    IUUpdateMinMax(&CfgParkPosNP);

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
    char resp[TEENASTRO_FOCUSER_BUFSIZE];
    int invert=-1, log2_micro=-1, resolution=-1, curr_10ma=-1, steprot=-1;

    if(!sendAndReceive(":FM#", resp, TEENASTRO_FOCUSER_BUFSIZE))
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
