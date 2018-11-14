 
#include "onfocus.h"

#include "indicom.h"

#include <cmath>
#include <cstring>
#include <memory>

#include <termios.h>
#include <unistd.h>

static std::unique_ptr<OnFocus> onFocus(new OnFocus());


void ISGetProperties(const char *dev)
{
	onFocus->ISGetProperties(dev);
}

void ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
	onFocus->ISNewSwitch(dev, name, states, names, n);
}

void ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n)
{
	onFocus->ISNewText(dev, name, texts, names, n);
}

void ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
	onFocus->ISNewNumber(dev, name, values, names, n);
}

void ISNewBLOB(const char *dev, const char *name, int sizes[], int blobsizes[], char *blobs[], char *formats[],
	       char *names[], int n)
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
	onFocus->ISSnoopDevice(root);
}


OnFocus::OnFocus() //:FI(this)
{
FI::SetCapability(FOCUSER_CAN_ABS_MOVE | FOCUSER_CAN_REL_MOVE | FOCUSER_CAN_ABORT);
}

bool OnFocus::initProperties()
{
	
	FI::initProperties(FOCUS_TAB);
	
	//FocuserInterface
	//Initial, these will be updated later. 
	FocusRelPosN[0].min   = 0.;
	FocusRelPosN[0].max   = 30000.;
	FocusRelPosN[0].value = 0;
	FocusRelPosN[0].step  = 10;
	FocusAbsPosN[0].min   = 0.;
	FocusAbsPosN[0].max   = 60000.;
	FocusAbsPosN[0].value = 0;
	FocusAbsPosN[0].step  = 10;
	
	// Focuser 1
	
	IUFillSwitch(&OSFocus1InitializeS[0], "Focus1_0", "Zero", ISS_OFF);
	IUFillSwitch(&OSFocus1InitializeS[1], "Focus1_2", "Mid", ISS_OFF);
	//     IUFillSwitch(&OSFocus1InitializeS[2], "Focus1_3", "max", ISS_OFF);
	IUFillSwitchVector(&OSFocus1InitializeSP, OSFocus1InitializeS, 2, getDeviceName(), "Foc1Rate", "Initialize", FOCUS_TAB, IP_RW, ISR_ATMOST1, 0, IPS_IDLE);
	 
	return true;
}
	 
	 
	 
void OnFocus::ISGetProperties(const char *dev)
{
	if (dev != nullptr && strcmp(dev, getDeviceName()) != 0) return;
	//LX200Generic::ISGetProperties(dev);
}

bool OnFocus::updateProperties()
{
	//OnFocus::updateProperties();
	FI::updateProperties();
		
	return true;
}
	
	
bool OnFocus::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
	if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
	{
		if (strstr(name, "FOCUS_"))
			return FI::processNumber(dev, name, values, names, n);
	}
	return true;
	
}


bool OnFocus::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
	int index = 0;
	
	if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
	{
		// Focuser
		// Focuser 1 Rates
		if (!strcmp(name, OSFocus1InitializeSP.name))
		{
			char cmd[RB_MAX_LEN];
			if (IUUpdateSwitch(&OSFocus1InitializeSP, states, names, n) < 0)
				return false;
			index = IUFindOnSwitchIndex(&OSFocus1InitializeSP);
			if (index == 0) {
				snprintf(cmd, 5, ":FZ#");
				sendOnStepCommandBlind(cmd);
				OSFocus1InitializeS[index].s=ISS_OFF;
				OSFocus1InitializeSP.s = IPS_OK;
				IDSetSwitch(&OSFocus1InitializeSP, nullptr);    
			}
			if (index == 1) {
				snprintf(cmd, 5, ":FH#");
				sendOnStepCommandBlind(cmd);
				OSFocus1InitializeS[index].s=ISS_OFF;
				OSFocus1InitializeSP.s = IPS_OK;
				IDSetSwitch(&OSFocus1InitializeSP, nullptr);
			}
		}
// 		if (strstr(name, "FOCUS"))
// 		{
// 			return FI::processSwitch(dev, name, states, names, n);
// 		}
	}
	return INDI::Focuser::ISNewSwitch(dev, name, states, names, n);
	
}

const char *OnFocus::getDefaultName()
{
	return "OnFocus";
}



/***** FOCUSER INTERFACE ******
 * 
 * NOT USED: 
 * virtual bool 	SetFocuserSpeed (int speed)
 * SetFocuserSpeed Set Focuser speed. More...
 * 
 * USED:
 * virtual IPState 	MoveFocuser (FocusDirection dir, int speed, uint16_t duration)
 * MoveFocuser the focuser in a particular direction with a specific speed for a finite duration. More...
 * 
 * USED:
 * virtual IPState 	MoveAbsFocuser (uint32_t targetTicks)
 * MoveFocuser the focuser to an absolute position. More...
 * 
 * USED:
 * virtual IPState 	MoveRelFocuser (FocusDirection dir, uint32_t ticks)
 * MoveFocuser the focuser to an relative position. More...
 * 
 * USED:
 * virtual bool 	AbortFocuser ()
 * AbortFocuser all focus motion. More...
 * 
 */


IPState OnFocus::MoveFocuser(FocusDirection dir, int speed, uint16_t duration)
{
	INDI_UNUSED(speed);
	//  :MIn#  Move In by n steps
	//         Returns: no reply
	//  :MOn#  Move Out by n steps
	//         Returns: no reply
	double output;
	char read_buffer[RB_MAX_LEN];
	output = duration;
	if (dir == FOCUS_INWARD) { 
		snprintf(read_buffer, sizeof(read_buffer), ":MI%5f#", output);
	} else {
		snprintf(read_buffer, sizeof(read_buffer), ":MO%5f#", output);
	}
	sendOnStepCommandBlind(read_buffer);
	return IPS_BUSY; // Normal case, should be set to normal by update. 
}

IPState OnFocus::MoveAbsFocuser (uint32_t targetTicks) {
	//  :MAn#  Move to Absolute position n
	//         Returns: no reply
	if (FocusAbsPosN[0].max >= targetTicks && FocusAbsPosN[0].min <= targetTicks) {
		char read_buffer[RB_MAX_LEN];
		snprintf(read_buffer, sizeof(read_buffer), ":MA%06d#", targetTicks);
		sendOnStepCommandBlind(read_buffer);
		return IPS_BUSY; // Normal case, should be set to normal by update. 
	} else {
		//Out of bounds as reported by OnStep
		LOG_ERROR("Focuser value outside of limit");
		LOGF_ERROR("Requested: %d min: %d max: %d", targetTicks, FocusAbsPosN[0].min, FocusAbsPosN[0].max);
		return IPS_ALERT; 
	}
}
IPState OnFocus::MoveRelFocuser (FocusDirection dir, uint32_t ticks) {
	//  :MIn#  Move In by n steps
	//         Returns: no reply
	//  :MOn#  Move Out by n steps
	//         Returns: no reply
	int output;
	char read_buffer[RB_MAX_LEN];
	output = ticks;
	if (dir == FOCUS_INWARD) { 
		snprintf(read_buffer, sizeof(read_buffer), ":MI%5f#", output);
	} else {
		snprintf(read_buffer, sizeof(read_buffer), ":MO%5f#", output);
	}
	sendOnStepCommandBlind(read_buffer);
	return IPS_BUSY; // Normal case, should be set to normal by update. 
}

bool OnFocus::AbortFocuser () {   
	//  :MH#  Move Halt
	//         Returns: no reply
	char cmd[8];
	strcpy(cmd, ":MH#");
	return sendOnStepCommandBlind(cmd);
}

void OnFocus::OSUpdateFocuser()
{
	char value[RB_MAX_LEN];  //azwing RB_MAX_LEN
	int current = 0;
	if (1) {
		// Alternate option:
		//if (!sendOnStepCommand(":FA#")) {
		//  :GP#  Get position
		//         Returns: n#
		getCommandString(PortFD, value, ":GP#");
		FocusAbsPosN[0].value =  atoi(value);
		current = FocusAbsPosN[0].value;
		IDSetNumber(&FocusAbsPosNP, nullptr);
		LOGF_DEBUG("Current focuser: %d, %d", atoi(value), FocusAbsPosN[0].value);
		//  :IS#  get status
		//         Returns: M# (for moving) or S# (for stopped)
		getCommandString(PortFD, value, ":IS#");
		if (value[0] == 'S') {
			FocusRelPosNP.s = IPS_OK;
			IDSetNumber(&FocusRelPosNP, nullptr);
			FocusAbsPosNP.s = IPS_OK;
			IDSetNumber(&FocusAbsPosNP, nullptr);
		} else if (value[0] == 'M') {
			FocusRelPosNP.s = IPS_BUSY;
			IDSetNumber(&FocusRelPosNP, nullptr);
			FocusAbsPosNP.s = IPS_BUSY;
			IDSetNumber(&FocusAbsPosNP, nullptr);
		} else { //INVALID REPLY
			FocusRelPosNP.s = IPS_ALERT;
			IDSetNumber(&FocusRelPosNP, nullptr);
			FocusAbsPosNP.s = IPS_ALERT;
			IDSetNumber(&FocusAbsPosNP, nullptr);
		}
		//  :GM#  Get max position
		//         Returns: n#
		getCommandString(PortFD, value, ":GM#");
		FocusAbsPosN[0].max   = atoi(value);
		IUUpdateMinMax(&FocusAbsPosNP);
		IDSetNumber(&FocusAbsPosNP, nullptr);
		//  :GI#  Get full in position
		//         Returns: 0#
		getCommandString(PortFD, value, ":GI#");
		FocusAbsPosN[0].min =  atoi(value);
		IUUpdateMinMax(&FocusAbsPosNP);
		IDSetNumber(&FocusAbsPosNP, nullptr);
		FI::updateProperties();
	} 
	

}




bool OnFocus::sendOnStepCommandBlind(const char *cmd)
{
	int error_type;
	int nbytes_write = 0;
	
	LOGF_DEBUG("CMD <%s>", cmd);
	
	tcflush(PortFD, TCIFLUSH);
	
	if ((error_type = tty_write_string(PortFD, cmd, &nbytes_write)) != TTY_OK)
		return error_type;
	
	return 1;
}

bool OnFocus::sendOnStepCommand(const char *cmd)      // Tested
{
	char response[1];
	int error_type;
	int nbytes_write = 0, nbytes_read = 0;
	
	LOGF_DEBUG("CMD <%s>", cmd);
	
	tcflush(PortFD, TCIFLUSH);
	
	if ((error_type = tty_write_string(PortFD, cmd, &nbytes_write)) != TTY_OK)
		return error_type;
	
	error_type = tty_read(PortFD, response, 1, ONFOCUS_TIMEOUT, &nbytes_read);
	
	tcflush(PortFD, TCIFLUSH);
	
	if (nbytes_read < 1)
	{
		LOG_ERROR("Unable to parse response.");
		return error_type;
	}
	
	return (response[0] == '0');
}

//Support Functions


int getCommandString(int fd, char *data, const char *cmd)
{
	char *term;
	int error_type;
	int nbytes_write = 0, nbytes_read = 0;
	
//	LOGF_DEBUG("CMD <%s>", cmd);
	//DEBUGFDEVICE(lx200Name, DBG_SCOPE, "CMD <%s>", cmd);
	
	if ((error_type = tty_write_string(fd, cmd, &nbytes_write)) != TTY_OK)
		return error_type;
	
	error_type = tty_nread_section(fd, data, RB_MAX_LEN, '#', ONFOCUS_TIMEOUT, &nbytes_read);
	tcflush(fd, TCIFLUSH);
	
	if (error_type != TTY_OK)
		return error_type;
	
	term = strchr(data, '#');
	if (term)
		*term = '\0';
	
//	LOGF_DEBUG("RES <%s>", data);
// 	DEBUGFDEVICE(lx200Name, DBG_SCOPE, "RES <%s>", data);
	
	return 0;
}
