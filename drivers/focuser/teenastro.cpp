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


#define TAF_FOCUSER_TIMEOUT 4
#define TAF_FOCUSER_BUFSIZE 128

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
    if(!sendAndReceive(":FV#", resp, TAF_FOCUSER_BUFSIZE))
        return false;
    if (strncmp(resp, "$ TeenAstro Focuser ",20))
    {
        DEBUGF(INDI::Logger::DBG_ERROR, "Handshake response: %s", resp);
        return false;
    }

    DEBUGF(INDI::Logger::DBG_DEBUG, "TeenAstroFocuser %s found", &resp[20]);
    return true;
}

bool TeenAstroFocuser::initProperties()
{
    INDI::Focuser::initProperties();
 
	strcpy(FocusSyncN[0].label,  "Ticks"); // Rename unit of measure to ticks, to avoid confusion with motor steps,
	strcpy(FocusMaxPosN[0].label,"Ticks"); // motor microsteps and the effect of the resolution setting.
	strcpy(FocusRelPosN[0].label,"Ticks"); // Ticks are resolution units.
	strcpy(FocusAbsPosN[0].label,"Ticks");

	strcpy(FocusReverseSP.group, FOCUS_TAB);

	strcpy(FocusSpeedNP.label, "Go-to speed");
	strcpy(FocusSpeedNP.group, FOCUS_TAB);
	strcpy(FocusSpeedN[0].label,"Steps/s");
	FocusSpeedN[0].min=TAF_speed_min;	
	FocusSpeedN[0].max=TAF_speed_max;
	FocusSpeedN[0].step=TAF_STEP(TAF_speed_min, TAF_speed_max);

    // Main control tab
    //

    IUFillNumber(&CfgParkPosN[0], "VAL", "Ticks", "%.f", 0, 100000, 1000, 0);
    IUFillNumberVector(&CfgParkPosNP, CfgParkPosN, 1, getDeviceName(), "PARK_POS", "Park position", MAIN_CONTROL_TAB, IP_RW, 60, IPS_IDLE );

    IUFillSwitch(&GoToParkS[0], "VAL", "Park", ISS_OFF);
    IUFillSwitchVector(&GoToParkSP, GoToParkS, 1, getDeviceName(), "GOTO_PARK", "Go-to park", MAIN_CONTROL_TAB, IP_RW, ISR_ATMOST1, 60, IPS_IDLE );

    IUFillNumber(&CurSpeedN[0], "VAL", "Steps/s", "%.f", TAF_speed_min, TAF_speed_max, TAF_STEP(TAF_speed_min, TAF_speed_max), TAF_speed_default);
    IUFillNumberVector(&CurSpeedNP, CurSpeedN, 1, getDeviceName(), "CUR_SPEED", "Current Speed", MAIN_CONTROL_TAB, IP_RO, 0, IPS_IDLE );

    IUFillNumber(&TempN[0], "VAL", "Celsius", "%+.1f°", -50., 50., 0, 0);
    IUFillNumberVector(&TempNP, TempN, 1, getDeviceName(), "TEMP", "Temperature", MAIN_CONTROL_TAB, IP_RO, 0, IPS_IDLE );

    // Focuser tab
    //

    IUFillText(&DeviceVersionT[0], "VAL", "Version", "unknown");
    IUFillTextVector(&DeviceVersionTP, DeviceVersionT, 1, getDeviceName(), "DEVICE_VERSION", "Device version", FOCUS_TAB, IP_RO, 60, IPS_IDLE);

    // Focuser tab: Motor configuration
    //

    IUFillNumber(&CfgMotorStepsPerRevolutionN[0], "VAL", "Steps", "%.f", TAF_steprot_min, TAF_steprot_max, TAF_STEP(TAF_steprot_min, TAF_steprot_max), TAF_steprot_default);
    IUFillNumberVector(&CfgMotorStepsPerRevolutionNP, CfgMotorStepsPerRevolutionN, 1, getDeviceName(), "MOT_STEPS_REV", "Steps/rev.", FOCUS_TAB, IP_RW, 60, IPS_IDLE );

    IUFillSwitch(&CfgMotorMicrostepsS[TAF_MICROS_4],   "4",   "4",   ISS_OFF);
    IUFillSwitch(&CfgMotorMicrostepsS[TAF_MICROS_8],   "8",   "8",   ISS_OFF);
    IUFillSwitch(&CfgMotorMicrostepsS[TAF_MICROS_16],  "16",  "16",  ISS_OFF);
    IUFillSwitch(&CfgMotorMicrostepsS[TAF_MICROS_32],  "32",  "32",  ISS_OFF);
    IUFillSwitch(&CfgMotorMicrostepsS[TAF_MICROS_64],  "64",  "64",  ISS_OFF);
    IUFillSwitch(&CfgMotorMicrostepsS[TAF_MICROS_128], "128", "128", ISS_OFF);
    IUFillSwitchVector(&CfgMotorMicrostepsSP, CfgMotorMicrostepsS, TAF_MICROS_N, getDeviceName(), "MOT_USTEPS", "Microsteps", FOCUS_TAB, IP_RW, ISR_ATMOST1, 60, IPS_IDLE );

    IUFillNumber(&CfgMotorResolutionN[0], "VAL", "Microsteps/tick", "%.f", TAF_res_min, TAF_res_max, TAF_STEP(TAF_res_min, TAF_res_max), TAF_res_default);
    IUFillNumberVector(&CfgMotorResolutionNP, CfgMotorResolutionN, 1, getDeviceName(), "MOT_RES", "Resolution", FOCUS_TAB, IP_RW, 60, IPS_IDLE );

    IUFillNumber(&CfgMotorCurrentN[0], "VAL", "mA", "%.f", TAF_curr_min, TAF_curr_max, TAF_STEP(TAF_curr_min, TAF_curr_max), TAF_curr_default);
    IUFillNumberVector(&CfgMotorCurrentNP, CfgMotorCurrentN, 1, getDeviceName(), "MOT_CUR", "Motor current", FOCUS_TAB, IP_RW, 60, IPS_IDLE );

    // Focuser tab: Motion configuration
    //

    IUFillNumber(&CfgGoToAccN[0], "VAL", "Steps/s^2", "%.f", TAF_acc_min, TAF_acc_max, TAF_STEP(TAF_acc_min, TAF_acc_max), TAF_acc_default);
    IUFillNumberVector(&CfgGoToAccNP, CfgGoToAccN, 1, getDeviceName(), "GOTO_ACCEL", "Go-to accel.", FOCUS_TAB, IP_RW, 60, IPS_IDLE );

    IUFillNumber(&CfgManualSpeedN[0], "VAL", "Steps/s", "%.f", TAF_speed_min, TAF_speed_max, TAF_STEP(TAF_speed_min, TAF_speed_max), TAF_speed_default);
    IUFillNumberVector(&CfgManualSpeedNP, CfgManualSpeedN, 1, getDeviceName(), "MAN_SPEED", "Manual speed", FOCUS_TAB, IP_RW, 60, IPS_IDLE );

    IUFillNumber(&CfgManualAccN[0], "VAL", "Steps/s^2", "%.f", TAF_acc_min, TAF_acc_max, TAF_STEP(TAF_acc_min, TAF_acc_max), TAF_acc_default);
    IUFillNumberVector(&CfgManualAccNP, CfgManualAccN, 1, getDeviceName(), "MAN_ACCEL", "Manual accel.", FOCUS_TAB, IP_RW, 60, IPS_IDLE );

    IUFillNumber(&CfgManualDecN[0], "VAL", "Steps/s^2", "%.f", TAF_acc_min, TAF_acc_max, TAF_STEP(TAF_acc_min, TAF_acc_max), TAF_acc_default);
    IUFillNumberVector(&CfgManualDecNP, CfgManualDecN, 1, getDeviceName(), "MAN_DECEL", "Manual decel.", FOCUS_TAB, IP_RW, 60, IPS_IDLE );

    // Focuser tab: Device actions
    //

    IUFillSwitch(&RebootDeviceS[0], "VAL", "Reboot", ISS_OFF);
    IUFillSwitchVector(&RebootDeviceSP, RebootDeviceS, 1, getDeviceName(), "REBOOT_DEVICE", "Reboot device", FOCUS_TAB, IP_RW, ISR_ATMOST1, 60, IPS_IDLE );

    IUFillSwitch(&EraseEEPROMS[0], "VAL", "Erase", ISS_OFF);
    IUFillSwitchVector(&EraseEEPROMSP, EraseEEPROMS, 1, getDeviceName(), "ERASE_EEPROM", "Erase EEPROM", FOCUS_TAB, IP_RW, ISR_ATMOST1, 60, IPS_IDLE );

    addDebugControl();

    return true;
}

void TeenAstroFocuser::initPositionPropertiesRanges(uint32_t maxPos)
{
    FocusAbsPosN[0].max  = FocusRelPosN[0].max  = FocusSyncN[0].max  = CfgParkPosN[0].max  = maxPos;
    FocusAbsPosN[0].step = FocusRelPosN[0].step = FocusSyncN[0].step = CfgParkPosN[0].step = maxPos / TAF_UI_STEPS;
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
	deleteProperty(FocusSyncNP.name);   // remove superclass controls to improve ordering of UI elements
	deleteProperty(FocusMaxPosNP.name);
	deleteProperty(FocusAbortSP.name);
	deleteProperty(FocusRelPosNP.name);
	deleteProperty(FocusAbsPosNP.name);

	defineProperty(&FocusRelPosNP);
	defineProperty(&FocusAbsPosNP);
	defineProperty(&FocusAbortSP);
    defineProperty(&GoToParkSP);
    defineProperty(&CfgParkPosNP);
	defineProperty(&FocusSyncNP); 
	defineProperty(&FocusMaxPosNP);
    defineProperty(&CurSpeedNP);
    defineProperty(&TempNP);
}

void TeenAstroFocuser::deleteMainControlProperties() 
{
    deleteProperty(GoToParkSP.name);
    deleteProperty(CfgParkPosNP.name);
    deleteProperty(CurSpeedNP.name);
    deleteProperty(TempNP.name);
}

void TeenAstroFocuser::defineOtherProperties() 
{
	deleteProperty(FocusReverseSP.name); // Place this on the focuser tab to avoid clutter
    deleteProperty(FocusSpeedNP.name);

    defineProperty(&DeviceVersionTP);

    defineProperty(&FocusReverseSP);
    defineProperty(&CfgMotorStepsPerRevolutionNP);
    defineProperty(&CfgMotorMicrostepsSP);
    defineProperty(&CfgMotorResolutionNP);
    defineProperty(&CfgMotorCurrentNP);

    defineProperty(&FocusSpeedNP);
    defineProperty(&CfgGoToAccNP);
    defineProperty(&CfgManualSpeedNP);
    defineProperty(&CfgManualAccNP);
    defineProperty(&CfgManualDecNP);

    defineProperty(&RebootDeviceSP);
    defineProperty(&EraseEEPROMSP);
}

void TeenAstroFocuser::deleteOtherProperties() 
{
    deleteProperty(DeviceVersionTP.name);

    deleteProperty(CfgMotorStepsPerRevolutionNP.name);
    deleteProperty(CfgMotorMicrostepsSP.name);
    deleteProperty(CfgMotorResolutionNP.name);
    deleteProperty(CfgMotorCurrentNP.name);

    deleteProperty(CfgGoToAccNP.name);
    deleteProperty(CfgManualSpeedNP.name);
    deleteProperty(CfgManualAccNP.name);
    deleteProperty(CfgManualDecNP.name);

    deleteProperty(EraseEEPROMSP.name);
    deleteProperty(EraseEEPROMSP.name);
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
    else if(!strcmp(name, CfgMotorMicrostepsSP.name))
    {
    	// parse selected number of usteps from states and labels. Labels are "Vxxx" where xxx is a number
        uint32_t usteps=0;
        for(int i=0; i<n; i++)
        	if(states[i]==ISS_ON)
        		usteps=atoi(names[i]);

        bool res=setMotorMicrosteps(usteps);
        if(res)
            IUUpdateSwitch(&CfgMotorMicrostepsSP, states, names, n);
		CfgMotorMicrostepsSP.s= res ? IPS_OK : IPS_ALERT;
		IDSetSwitch(&CfgMotorMicrostepsSP, NULL);
		return res;
    }
	else if(!strcmp(name, RebootDeviceSP.name)) 
	{
		RebootDeviceSP.s=IPS_BUSY;
		IDSetSwitch(&RebootDeviceSP, NULL);
		bool res=rebootDevice();
		RebootDeviceS[0].s=ISS_OFF;
		RebootDeviceSP.s= res ? IPS_OK : IPS_ALERT;
		IDSetSwitch(&RebootDeviceSP, NULL);
		return res;
	} 
	else if(!strcmp(name, EraseEEPROMSP.name)) 
	{
		EraseEEPROMSP.s=IPS_BUSY;
		IDSetSwitch(&EraseEEPROMSP, NULL);
		bool res=eraseDeviceEEPROM();
		EraseEEPROMS[0].s=ISS_OFF;
		EraseEEPROMSP.s= res ? IPS_OK : IPS_ALERT;
		IDSetSwitch(&EraseEEPROMSP, NULL);
		return res;
	} else 
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
    	uint32_t val=(uint32_t) rint(values[0]);
        bool res=SetFocuserMaxPosition(val);
        if(res && FocusMaxPosN[0].value!=val) {
    	    initPositionPropertiesRanges(val);
			deleteMainControlProperties();      // Force redraw of UI controls as IUUpdateMinMax
			defineMainControlProperties();    	// does not reliably update the step size.
        }
        return ISNewNumberHelper(&FocusMaxPosNP, values, names, n, res);
    }
    else if(!strcmp(name, CfgManualSpeedNP.name))
    {
        bool res=setManualSpeed((uint32_t) rint(values[0]));
        return ISNewNumberHelper(&CfgManualSpeedNP, values, names, n, res);
    }
    else if(!strcmp(name, FocusSpeedNP.name))
    {
        bool res=SetFocuserSpeed(rint(values[0]));
        return ISNewNumberHelper(&FocusSpeedNP, values, names, n, res);
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
    else if(!strcmp(name, CfgMotorStepsPerRevolutionNP.name))
    {
        bool res=setMotorStepsPerRevolution((uint32_t) rint(values[0]));
        return ISNewNumberHelper(&CfgMotorStepsPerRevolutionNP, values, names, n, res);
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
        return INDI::Focuser::ISNewNumber(dev, name, values, names, n);
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
	if(!isConnected())
		return;

	updateState();
    SetTimer(getCurrentPollingPeriod());
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
        DEBUGF(INDI::Logger::DBG_ERROR, "send(\"%s\"): %s.", msg, errstr);
        return false;
    }
    return true;
}


bool TeenAstroFocuser::sendAndReceive(const char *const msg, char *resp, int bufsize) {
    if(!send(msg))
        return false;

    int nbytes_read=0;
    int rc = tty_nread_section(PortFD, resp, bufsize, '#', TAF_FOCUSER_TIMEOUT, &nbytes_read);
    resp[nbytes_read]='\0';
	if(rc!=TTY_OK || nbytes_read==0 || resp[nbytes_read-1]!='#')
    {
        char errstr[MAXRBUF];
        tty_error_msg(rc, errstr, MAXRBUF);
        DEBUGF(INDI::Logger::DBG_ERROR, "sendAndReceive(\"%s\") received \"%s\": %s.", msg, resp, errstr);
        return false;
    }
    DEBUGF(INDI::Logger::DBG_DEBUG, "sendAndReceive(\"%s\") received \"%s\".", msg, resp);
    return true;
}

bool TeenAstroFocuser::sendAndReceiveBool(const char *const msg) {
    if(!send(msg))
        return false;

    char resp[2];
    int nbytes_read=0;
    int rc = tty_read(PortFD, resp, 1, TAF_FOCUSER_TIMEOUT, &nbytes_read);
    resp[nbytes_read]='\0';
    if(rc!=TTY_OK || resp[0]!='1')
    {
        char errstr[MAXRBUF];
        tty_error_msg(rc, errstr, MAXRBUF);
        DEBUGF(INDI::Logger::DBG_ERROR, "sendAndReceiveBool(\"%s\") received \"%s\": %s.", msg, resp, errstr);
        return false;
    }
    DEBUGF(INDI::Logger::DBG_DEBUG, "sendAndReceiveBool(\"%s\") received \"%s\".", msg, resp);
    return resp[0]=='1';
}


bool TeenAstroFocuser::updateDeviceVersion()
{
    char resp[TAF_FOCUSER_BUFSIZE];
    if(!sendAndReceive(":FV#", resp, TAF_FOCUSER_BUFSIZE))
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
    char resp[TAF_FOCUSER_BUFSIZE];
    int pos=-1, speed=-1;
    float temp=-1;

    if(!sendAndReceive(":F?#", resp, TAF_FOCUSER_BUFSIZE))
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


bool TeenAstroFocuser::updateMotionConfig() 
{
    char resp[TAF_FOCUSER_BUFSIZE];
    int parkPos=-1, maxPos=-1, manualSpeed=-1, goToSpeed=-1, gotoAcc=-1, manAcc=-1, manDec=-1;

    if(!sendAndReceive(":F~#", resp, TAF_FOCUSER_BUFSIZE))
        return false;

    if(sscanf(resp, "~%d %d %d %d %d %d %d#", &parkPos, &maxPos, &manualSpeed, &goToSpeed, &gotoAcc, &manAcc, &manDec)<=0)
    {
        DEBUGF(INDI::Logger::DBG_ERROR, "Invalid format: focuser state: %s", resp);
        return false;
    }

    if(maxPos!=FocusMaxPosN[0].value)
	    initPositionPropertiesRanges(maxPos);

    CfgParkPosN[0].value = parkPos;
    CfgParkPosNP.s = IPS_OK;
    IDSetNumber(&CfgParkPosNP, NULL);
    FocusMaxPosN[0].value = maxPos;
    FocusMaxPosNP.s = IPS_OK;
    IDSetNumber(&FocusMaxPosNP, NULL);
    CfgManualSpeedN[0].value = manualSpeed;
    CfgManualSpeedNP.s = IPS_OK;
    IDSetNumber(&CfgManualSpeedNP, NULL);
    FocusSpeedN[0].value = goToSpeed;
    FocusSpeedNP.s = IPS_OK;
    IDSetNumber(&FocusSpeedNP, NULL);
    CfgGoToAccN[0].value = gotoAcc;
    CfgGoToAccNP.s = IPS_OK;
    IDSetNumber(&CfgGoToAccNP, NULL);
    CfgManualAccN[0].value = manAcc;
    CfgManualAccNP.s = IPS_OK;
    IDSetNumber(&CfgManualAccNP, NULL);
    CfgManualDecN[0].value = manDec;
    CfgManualDecNP.s = IPS_OK;
    IDSetNumber(&CfgManualDecNP, NULL);

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
    int reverse=-1, log2_micro=-1, resolution=-1, curr_10ma=-1, steprot=-1;

    if(!sendAndReceive(":FM#", resp, TAF_FOCUSER_BUFSIZE))
        return false;

    if(sscanf(resp, "M%d %d %d %d %d#", &reverse, &log2_micro, &resolution, &curr_10ma, &steprot)<=0)
    {
        DEBUGF(INDI::Logger::DBG_ERROR, "Invalid format: focuser state (%s)", resp);
        return false;
    }

    FocusReverseS[0].s=(reverse==0) ? ISS_OFF : ISS_ON;
    FocusReverseS[1].s=(reverse==0) ? ISS_ON  : ISS_OFF;
    FocusReverseSP.s  = IPS_OK;
    IDSetSwitch(&FocusReverseSP, NULL);

    uint32_t micro=(1<<log2_micro); // TeenAstro device returns and expects log_2(microsteps)
    bool microFound=false;
    for(int i=0; i<TAF_MICROS_N; i++)
    {
    	uint32_t thisMicro=(uint32_t) atoi(CfgMotorMicrostepsS[i].name);
    	CfgMotorMicrostepsS[i].s= (micro==thisMicro) ? ISS_ON : ISS_OFF;
    	if(micro==thisMicro)
    		microFound=true;
    }
    CfgMotorMicrostepsSP.s = microFound ? IPS_OK : IPS_ALERT;
    IDSetSwitch(&CfgMotorMicrostepsSP, NULL);

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

bool TeenAstroFocuser::ReverseFocuser(bool enable)
{
    return setConfigItem('7', enable ? 1 : 0);
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

    IUSaveConfigNumber(fp, &PresetNP);
    this->controller->saveConfigItems(fp);

    return true;
}
