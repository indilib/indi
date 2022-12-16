/*
    Losmandy Gemini INDI driver

    Copyright (C) 2017 Jasem Mutlaq
    Copyright (C) 2018 Eric Vickery

    Difference from LX200 Generic:

    1. Added Side of Pier
    2. Reimplemented isSlewComplete to use :Gv# since it is more reliable
    3. Support networked connections.
    4. Side of pier
    5. Variable GOTO/SLEW/MOVE speeds.

    v1.4:

    + Added MOUNT_STATE_UPDATE_FREQ to reduce number of calls to updateMountState to reduce traffic
    + All TCIFLUSH --> TCIOFLUSH to make sure both pipes are flushed since we received logs with mismatched traffic.

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

#include "lx200gemini.h"

#include "indicom.h"
#include "lx200driver.h"
#include "connectionplugins/connectioninterface.h"
#include "connectionplugins/connectiontcp.h"

#include <cstring>
#include <termios.h>

#define MANUAL_SLEWING_SPEED_ID     120
#define GOTO_SLEWING_SPEED_ID       140
#define MOVE_SPEED_ID               145 // L5
#define GUIDING_SPEED_ID            150
#define GUIDING_SPEED_RA_ID         151 // L5
#define GUIDING_SPEED_DEC_ID        152 // L5
#define CENTERING_SPEED_ID          170
#define SERVO_POINTING_PRECISION_ID 401 // L6
#define PEC_MAX_STEPS_ID            27
#define PEC_COUNTER_ID              501
#define PEC_STATUS_ID               509
#define PEC_START_TRAINING_ID       530 // L5
#define PEC_ABORT_TRAINING_ID       535 // L5
#define PEC_REPLAY_ON_ID            531 // L5
#define PEC_REPLAY_OFF_ID           532 // L5
#define PEC_ENABLE_AT_BOOT_ID       508 // L5.2
#define PEC_GUIDING_SPEED_ID        502
#define SERVO_FIRMWARE              400 // L6 <ra>;<dec> (L6)
#define FLIP_POINT_EAST_ID          227 // L6
#define FLIP_POINT_WEST_ID          228 // L6
#define FLIP_POINTS_ENABLED_ID      229 // L6
#define PEC_TAB "PEC"
#define FIRMWARE_TAB "Firmware data"


LX200Gemini::LX200Gemini()
{
    setVersion(1, 6);

    setLX200Capability(LX200_HAS_SITES | LX200_HAS_PULSE_GUIDING);

    SetTelescopeCapability(TELESCOPE_CAN_PARK | TELESCOPE_CAN_SYNC | TELESCOPE_CAN_GOTO | TELESCOPE_CAN_ABORT |
                           TELESCOPE_HAS_TIME | TELESCOPE_HAS_LOCATION | TELESCOPE_HAS_PIER_SIDE | TELESCOPE_HAS_TRACK_MODE |
                           TELESCOPE_CAN_CONTROL_TRACK | TELESCOPE_HAS_PEC,
			   4);
}

const char *LX200Gemini::getDefaultName()
{
    return "Losmandy Gemini";
}

bool LX200Gemini::Connect()
{
    Connection::Interface *activeConnection = getActiveConnection();

    if (!activeConnection->name().compare("CONNECTION_TCP"))
    {
        tty_set_gemini_udp_format(1);
    }

    return LX200Generic::Connect();
}

void LX200Gemini::ISGetProperties(const char *dev)
{
    LX200Generic::ISGetProperties(dev);

    // Read config from file
    int index = 0;
    if (IUGetConfigOnSwitch(&StartupModeSP, &index) == 0)
    {
        IUResetSwitch(&StartupModeSP);
        StartupModeSP.sp[index].s = ISS_ON;
        defineProperty(&StartupModeSP);
    }
}

bool LX200Gemini::initProperties()
{
    LX200Generic::initProperties();

    // Show firmware
    IUFillText(&VersionT[FIRMWARE_DATE], "Date", "", "");
    IUFillText(&VersionT[FIRMWARE_TIME], "Time", "", "");
    IUFillText(&VersionT[FIRMWARE_LEVEL], "Level", "", "");
    IUFillText(&VersionT[FIRMWARE_NAME], "Name", "", "");
    IUFillTextVector(&VersionTP, VersionT, 5, getDeviceName(), "Firmware Info", "", FIRMWARE_TAB, IP_RO, 0, IPS_IDLE);

    // Park Option
    IUFillSwitch(&ParkSettingsS[PARK_HOME], "HOME", "Home", ISS_ON);
    IUFillSwitch(&ParkSettingsS[PARK_STARTUP], "STARTUP", "Startup", ISS_OFF);
    IUFillSwitch(&ParkSettingsS[PARK_ZENITH], "ZENITH", "Zenith", ISS_OFF);
    IUFillSwitchVector(&ParkSettingsSP, ParkSettingsS, 3, getDeviceName(), "PARK_SETTINGS", "Park Settings",
                       MAIN_CONTROL_TAB, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);

    IUFillSwitch(&StartupModeS[COLD_START], "COLD_START", "Cold", ISS_ON);
    IUFillSwitch(&StartupModeS[PARK_STARTUP], "WARM_START", "Warm", ISS_OFF);
    IUFillSwitch(&StartupModeS[PARK_ZENITH], "WARM_RESTART", "Restart", ISS_OFF);
    IUFillSwitchVector(&StartupModeSP, StartupModeS, 3, getDeviceName(), "STARTUP_MODE", "Startup Mode",
                       MAIN_CONTROL_TAB, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);

    IUFillNumber(&ManualSlewingSpeedN[0], "MANUAL_SLEWING_SPEED", "Manual Slewing Speed", "%g", 20, 2000., 10., 800);
    IUFillNumberVector(&ManualSlewingSpeedNP, ManualSlewingSpeedN, 1, getDeviceName(), "MANUAL_SLEWING_SPEED",
                       "Slew Speed", MOTION_TAB, IP_RW,  0, IPS_IDLE);

    IUFillNumber(&GotoSlewingSpeedN[0], "GOTO_SLEWING_SPEED", "Goto Slewing Speed", "%g", 20, 2000., 10., 800);
    IUFillNumberVector(&GotoSlewingSpeedNP, GotoSlewingSpeedN, 1, getDeviceName(), "GOTO_SLEWING_SPEED",
		       "Goto Speed", MOTION_TAB, IP_RW, 0, IPS_IDLE);

    IUFillNumber(&MoveSpeedN[0], "MOVE_SPEED", "Move Speed", "%g", 20, 2000., 10., 10);
    IUFillNumberVector(&MoveSpeedNP, MoveSpeedN, 1, getDeviceName(), "MOVE_SLEWING_SPEED",
		       "Move Speed", MOTION_TAB, IP_RW, 0, IPS_IDLE);

    IUFillNumber(&GuidingSpeedBothN[GUIDING_BOTH], "GUIDING_SPEED", "Guide Speed RA/DEC", "%g", 0.2, 0.8, 0.1, 0.5);
    IUFillNumberVector(&GuidingSpeedBothNP, GuidingSpeedBothN, 1, getDeviceName(), "GUIDING_SLEWING_SPEED_BOTH",
		       "Guide Speed",
                       GUIDE_TAB, IP_RW, 0, IPS_IDLE);

    IUFillNumber(&GuidingSpeedN[GUIDING_WE], "GUIDE_RATE_WE", "W/E Rate", "%g", 0.2, 0.8, 0.1, 0.5);
    IUFillNumber(&GuidingSpeedN[GUIDING_NS], "GUIDE_RATE_NS", "N/S Rate", "%g", 0.2, 0.8, 0.1, 0.5);
    IUFillNumberVector(&GuidingSpeedNP, GuidingSpeedN, 2, getDeviceName(), "GUIDE_RATE",
                       "Guide Speed", GUIDE_TAB, IP_RW, 0, IPS_IDLE);

    IUFillNumber(&CenteringSpeedN[0], "CENTERING_SPEED", "Centering Speed", "%g", 20, 2000., 10., 10);
    IUFillNumberVector(&CenteringSpeedNP, CenteringSpeedN, 1, getDeviceName(), "CENTERING_SLEWING_SPEED",
                       "Center Speed", MOTION_TAB, IP_RW, 0, IPS_IDLE);

    IUFillSwitch(&TrackModeS[GEMINI_TRACK_SIDEREAL], "TRACK_SIDEREAL", "Sidereal", ISS_ON);
    IUFillSwitch(&TrackModeS[GEMINI_TRACK_KING], "TRACK_CUSTOM", "King", ISS_OFF);
    IUFillSwitch(&TrackModeS[GEMINI_TRACK_LUNAR], "TRACK_LUNAR", "Lunar", ISS_OFF);
    IUFillSwitch(&TrackModeS[GEMINI_TRACK_SOLAR], "TRACK_SOLAR", "Solar", ISS_OFF);

    //PEC 
    IUFillSwitch(&PECControlS[PEC_START_TRAINING], "PEC_START_TRAINING", "Start Training", ISS_OFF);
    IUFillSwitch(&PECControlS[PEC_ABORT_TRAINING], "PEC_ABORT_TRAINING", "Abort Training", ISS_OFF);
    IUFillSwitchVector(&PECControlSP, PECControlS, 2, getDeviceName(), "PEC_COMMANDS",
                       "PEC Cmds", MOTION_TAB, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);

    IUFillText(&PECStateT[PEC_STATUS_ACTIVE], "PEC_STATUS_ACTIVE", "PEC active", "");
    IUFillText(&PECStateT[PEC_STATUS_FRESH_TRAINED], "PEC_STATUS_FRESH_TRAINED", "PEC freshly trained", "");
    IUFillText(&PECStateT[PEC_STATUS_TRAINING_IN_PROGRESS], "PEC_STATUS_TRAINING_IN_PROGRESS", "PEC training in progress", "");
    IUFillText(&PECStateT[PEC_STATUS_TRAINING_COMPLETED], "PEC_STATUS_TRAINING_COMPLETED", "PEC training just completed", "");
    IUFillText(&PECStateT[PEC_STATUS_WILL_TRAIN], "PEC_STATUS_WILL_TRAIN", "PEC will train soon", "");

    IUFillText(&PECStateT[PEC_STATUS_DATA_AVAILABLE], "PEC_STATUS_DATA_AVAILABLE", "PEC Data available", "");    
    IUFillTextVector(&PECStateTP, PECStateT, 6, getDeviceName(), "PEC_STATE",
                     "PEC State", MOTION_TAB, IP_RO, 0, IPS_OK);

    IUFillText(&PECCounterT[0], "PEC_COUNTER", "Counter", "");
    IUFillTextVector(&PECCounterTP, PECCounterT, 1, getDeviceName(), "PEC_COUNTER",
                     "PEC Counter", MOTION_TAB, IP_RO, 0, IPS_IDLE);

    IUFillNumber(&PECMaxStepsN[0], "PEC_MAX_STEPS", "PEC MaxSteps", "%f", 0, 4294967296, 1, 0);
    IUFillNumberVector(&PECMaxStepsNP, PECMaxStepsN, 1, getDeviceName(), "PEC_MAX_STEPS",
                       "PEC Steps", MOTION_TAB, IP_RO, 0, IPS_IDLE);

    IUFillNumber(&ServoPrecisionN[SERVO_RA], "SERVO_RA", "4x RA Precision", "%f", 0, 1, 1, 0);
    IUFillNumber(&ServoPrecisionN[SERVO_DEC], "SERVO_DEC", "4x DEC Precision", "%f", 0, 1, 1, 0);
    IUFillNumberVector(&ServoPrecisionNP, ServoPrecisionN, 2, getDeviceName(), "SERVO",
                       "Servo", MOTION_TAB, IP_RW, 0, IPS_IDLE);
    
    IUFillNumber(&PECGuidingSpeedN[0], "PEC_GUIDING_SPEED", "PEC GuidingSpeed", "%f", 0.2, 0.8, 0.1, 0);
    IUFillNumberVector(&PECGuidingSpeedNP, PECGuidingSpeedN, 1, getDeviceName(), "PEC_GUIDING_SPEED",
                       "PEC Speed", MOTION_TAB, IP_RO, 0, IPS_IDLE);

    IUFillNumber(&PECEnableAtBootN[0], "ENABLE_PEC_AT_BOOT", "Enable PEC at boot", "%f", 0, 1, 1, 0);
    IUFillNumberVector(&PECEnableAtBootNP, PECEnableAtBootN, 1, getDeviceName(), "ENABLE_PEC_AT_BOOT",
                       "PEC Setting", MOTION_TAB, IP_RW, 0, IPS_IDLE);

    gemini_software_level_ = 0.0;
    
    return true;
}

void LX200Gemini::syncPec(){
        const int MAX_VALUE_LENGTH = 32;
        char value[MAX_VALUE_LENGTH] = {0};


	// Gemini Firmware > 4
	if(gemini_software_level_ >= 6.0)
	{
	    if (getGeminiProperty(SERVO_POINTING_PRECISION_ID, value))
	    {
	        uint8_t servo_value;
		sscanf(value, "%c", &servo_value);
		ServoPrecisionN[SERVO_RA].value = servo_value & 1;
		ServoPrecisionN[SERVO_DEC].value = (servo_value & 2)>>1;
		IDSetNumber(&ServoPrecisionNP, nullptr);
	    } else {
	        ServoPrecisionNP.s = IPS_ALERT;
		IDSetNumber(&ServoPrecisionNP, nullptr);
	    }	  
	} else if (gemini_software_level_ >= 5.2)
	{
	    if (getGeminiProperty(PEC_ENABLE_AT_BOOT_ID, value))
	    {
	        uint32_t pec_at_boot_value;
		sscanf(value, "%i", &pec_at_boot_value);
		PECEnableAtBootN[0].value = pec_at_boot_value;
		IDSetNumber(&PECEnableAtBootNP, nullptr);
	    } else {
	        PECEnableAtBootNP.s = IPS_ALERT;
		IDSetNumber(&PECEnableAtBootNP, nullptr);
	    }
	} else if (gemini_software_level_ >= 5.0)
	{
	    if (getGeminiProperty(GUIDING_SPEED_RA_ID, value))
	    {
	        float guiding_value;
		sscanf(value, "%f", &guiding_value);
		GuidingSpeedN[GUIDING_WE].value = guiding_value;
		IDSetNumber(&GuidingSpeedNP, nullptr);
	    } else {
	        GuidingSpeedNP.s = IPS_ALERT;
		IDSetNumber(&GuidingSpeedNP, nullptr);
	    }	  
	    if (getGeminiProperty(GUIDING_SPEED_DEC_ID, value))
	    {
	        float guiding_value;
		sscanf(value, "%f", &guiding_value);
		GuidingSpeedN[GUIDING_NS].value = guiding_value;
		IDSetNumber(&GuidingSpeedNP, nullptr);
	    } else {
	        GuidingSpeedNP.s = IPS_ALERT;
		IDSetNumber(&GuidingSpeedNP, nullptr);
	    }	  
	}
	// Gemini Firmware > 4

	
        if (getGeminiProperty(PEC_MAX_STEPS_ID, value))
        {
	    float max_value;
            sscanf(value, "%f", &max_value);
            PECMaxStepsN[0].value = max_value;
	    IDSetNumber(&PECMaxStepsNP, nullptr);
        } else {
	    PECMaxStepsNP.s = IPS_ALERT;
	    IDSetNumber(&PECMaxStepsNP, nullptr);
	}	  
        if (getGeminiProperty(PEC_COUNTER_ID, value))
        {
	    char valueString[32] = {0};
	    uint32_t pec_counter = 0;
	    sscanf(value, "%u", &pec_counter);
	    snprintf(valueString, 32, "%i", pec_counter);

	    IUSaveText(&PECCounterT[0], valueString);
	    IDSetText(&PECCounterTP, nullptr);
        } else {
	    PECCounterTP.s = IPS_ALERT;
	    IDSetText(&PECCounterTP, nullptr);
	}	  
        if (getGeminiProperty(PEC_GUIDING_SPEED_ID, value))
        {
	    float guiding_value;
            sscanf(value, "%f", &guiding_value);
            PECGuidingSpeedN[0].value = guiding_value;
	    IDSetNumber(&PECGuidingSpeedNP, nullptr);
        } else {
	    PECGuidingSpeedNP.s = IPS_ALERT;
	    IDSetNumber(&PECGuidingSpeedNP, nullptr);
	}	  
        if (getGeminiProperty(GUIDING_SPEED_ID, value))
        {
	    float guiding_value;
            sscanf(value, "%f", &guiding_value);
            GuidingSpeedBothN[GUIDING_BOTH].value = guiding_value;
	    IDSetNumber(&GuidingSpeedBothNP, nullptr);
        } else {
	    GuidingSpeedBothNP.s = IPS_ALERT;
	    IDSetNumber(&GuidingSpeedBothNP, nullptr);
	}	  
        if (getGeminiProperty(PEC_STATUS_ID, value))
        {
	    uint32_t pec_status = 0;
	    sscanf(value, "%u", &pec_status);
	    if(pec_status & 1){ // PEC_ACTIVE
	      IUSaveText(&PECStateT[PEC_STATUS_ACTIVE], "Yes");
	    } else {
	      IUSaveText(&PECStateT[PEC_STATUS_ACTIVE], "No");
	    }
    	    if(pec_status & 2){ // Freshly_Trained
	      IUSaveText(&PECStateT[PEC_STATUS_FRESH_TRAINED], "Yes");
	    } else {
	      IUSaveText(&PECStateT[PEC_STATUS_FRESH_TRAINED], "No");
	    }	    
    	    if(pec_status & 4){ // Training_In_Progress
	      IUSaveText(&PECStateT[PEC_STATUS_TRAINING_IN_PROGRESS], "Yes");
	    } else {
	      IUSaveText(&PECStateT[PEC_STATUS_TRAINING_IN_PROGRESS], "No");
	    }
   	    if(pec_status & 8){ // Training_just_completed
	      IUSaveText(&PECStateT[PEC_STATUS_TRAINING_COMPLETED], "Yes");
	    } else {
	      IUSaveText(&PECStateT[PEC_STATUS_TRAINING_COMPLETED], "No");
	    }
    	    if(pec_status & 16){ // Training will start soon
	      IUSaveText(&PECStateT[PEC_STATUS_WILL_TRAIN], "Yes");
	    } else {
	      IUSaveText(&PECStateT[PEC_STATUS_WILL_TRAIN], "No");
	    }
    	    if(pec_status & 32){ // PEC Data Available
	      IUSaveText(&PECStateT[PEC_STATUS_DATA_AVAILABLE], "Yes");
	    } else {
	      IUSaveText(&PECStateT[PEC_STATUS_DATA_AVAILABLE], "No");
	    }
	    IDSetText(&PECStateTP, nullptr);
        } else {
	    PECStateTP.s = IPS_ALERT;
	    IDSetText(&PECStateTP, nullptr);
	}	  
}


bool LX200Gemini::updateProperties()
{
    const int MAX_VALUE_LENGTH = 32;
    LX200Generic::updateProperties();

    if (isConnected())
    {
        uint32_t speed = 0;
        char value[MAX_VALUE_LENGTH] = {0};
        char value2[MAX_VALUE_LENGTH] = {0};
	if (!isSimulation())
	{
	    VersionTP.tp[FIRMWARE_DATE].text = new char[64];
	    getVersionDate(PortFD, VersionTP.tp[FIRMWARE_DATE].text);
	    VersionTP.tp[FIRMWARE_TIME].text = new char[64];
	    getVersionTime(PortFD, VersionTP.tp[FIRMWARE_TIME].text);
	    VersionTP.tp[FIRMWARE_LEVEL].text = new char[64];
	    getVersionNumber(PortFD, VersionTP.tp[FIRMWARE_LEVEL].text);
	    VersionTP.tp[FIRMWARE_NAME].text = new char[128];
	    getProductName(PortFD, VersionTP.tp[FIRMWARE_NAME].text);
	    sscanf(VersionTP.tp[FIRMWARE_LEVEL].text, "%f", &gemini_software_level_);
	    IDSetText(&VersionTP, nullptr);
	}
        defineProperty(&VersionTP);
        defineProperty(&ParkSettingsSP);

	if (gemini_software_level_ < 5.0)
	{
	    deleteProperty(PECStateSP.name);
	}
	if (gemini_software_level_ >= 5.2 && getGeminiProperty(PEC_ENABLE_AT_BOOT_ID, value))
        {
	    uint32_t pec_at_boot_value;
            sscanf(value, "%i", &pec_at_boot_value);
            PECEnableAtBootN[0].value = pec_at_boot_value;
	    IDSetNumber(&PECEnableAtBootNP, nullptr);
            defineProperty(&PECEnableAtBootNP);
        }
        if (gemini_software_level_ >= 6.0 && getGeminiProperty(SERVO_POINTING_PRECISION_ID, value))
        {
	    uint8_t servo_value;
            sscanf(value, "%c", &servo_value);
            ServoPrecisionN[SERVO_RA].value = servo_value & 1;
            ServoPrecisionN[SERVO_DEC].value = (servo_value & 2)>>1;
            defineProperty(&ServoPrecisionNP);
        }
        if (getGeminiProperty(PEC_GUIDING_SPEED_ID, value))
        {
  	    float guiding_speed_value;
            sscanf(value, "%f", &guiding_speed_value);
            PECGuidingSpeedN[0].value = guiding_speed_value;
            defineProperty(&PECGuidingSpeedNP);
        }
        if (gemini_software_level_ >= 5.0 && getGeminiProperty(PEC_COUNTER_ID, value))
        {
	    defineProperty(&PECControlSP);
	    char valueString[32] = {0};
	    uint32_t pec_counter = 0;
	    sscanf(value, "%u", &pec_counter);
	    snprintf(valueString, 32, "%i", pec_counter);

	    PECControlSP.s = IPS_OK;
	    IUSaveText(&PECCounterT[0], valueString);
	    defineProperty(&PECCounterTP);
	}
        if (gemini_software_level_ >= 6.0 && getGeminiProperty(FLIP_POINTS_ENABLED_ID, value))
        {
	    char valueString[32] = {0};
	    uint32_t flip_value = 0;
	    sscanf(value, "%u", &flip_value);
	    snprintf(valueString, 32, "%i", flip_value);

	    if(flip_value){
	        if(flip_value & FLIP_EAST){
		    IUFillSwitch(&FlipControlS[FLIP_EAST_CONTROL], "FLIP_EAST_CONTROL", "East", ISS_ON);
		} else {
		    IUFillSwitch(&FlipControlS[FLIP_EAST_CONTROL], "FLIP_EAST_CONTROL", "East", ISS_OFF);
		}
		if(flip_value & FLIP_WEST){
		    IUFillSwitch(&FlipControlS[FLIP_WEST_CONTROL], "FLIP_WEST_CONTROL", "West", ISS_ON);
		} else {
		    IUFillSwitch(&FlipControlS[FLIP_WEST_CONTROL], "FLIP_WEST_CONTROL", "West", ISS_OFF);
		}
	    } else {
	        IUFillSwitch(&FlipControlS[FLIP_EAST_CONTROL], "FLIP_EAST_CONTROL", "East", ISS_OFF);
	        IUFillSwitch(&FlipControlS[FLIP_WEST_CONTROL], "FLIP_WEST_CONTROL", "West", ISS_OFF);
	    }
	    IUFillSwitchVector(&FlipControlSP, FlipControlS, 2, getDeviceName(), "FLIP_COMMANDS",
			       "Flip Point", MOTION_TAB, IP_RW, ISR_NOFMANY, 60, IPS_IDLE);
	    defineProperty(&FlipControlSP);
	}

        if (gemini_software_level_ >= 6.0 &&
	    getGeminiProperty(FLIP_POINT_EAST_ID, value) &&
	    getGeminiProperty(FLIP_POINT_WEST_ID, value2))
        {
	    uint32_t eastDegree = 0;
	    uint32_t eastMin = 0;

	    uint32_t westDegree = 0;
	    uint32_t westMin = 0;
	    
	    sscanf(value, " %ud%u ", &eastDegree, &eastMin);
	    IUFillNumber(&FlipPositionN[FLIP_EAST_DEGREE_VALUE], "FLIP_EAST_DEGREE_VALUE", "East Degrees", "%g", 0, 360, 1, eastDegree);
	    IUFillNumber(&FlipPositionN[FLIP_EAST_MIN_VALUE], "FLIP_EAST_MIN_VALUE", "East Minutes", "%g", 0, 360, 1, eastMin);
	    
	    sscanf(value2, " %ud%u ", &westDegree, &westMin);
	    IUFillNumber(&FlipPositionN[FLIP_WEST_DEGREE_VALUE], "FLIP_WEST_DEGREE_VALUE", "West Degrees", "%g", 0, 360, 1, westDegree);
	    IUFillNumber(&FlipPositionN[FLIP_WEST_MIN_VALUE], "FLIP_WEST_MIN_VALUE", "West Minutes", "%g", 0, 360, 1, westMin);

	    IUFillNumberVector(&FlipPositionNP, FlipPositionN, 4, getDeviceName(), "FLIP_POSITION",
	     		       "Flip Position", MOTION_TAB, IP_RW, 0, IPS_IDLE);
	    
	    LOGF_INFO("FlipValueEastD: <%i>", eastDegree);
	    LOGF_INFO("FlipValueEastM: <%i>", eastMin);
	    LOGF_INFO("FlipValueWestD: <%i>", westDegree);
	    LOGF_INFO("FlipValueWestM: <%i>", westMin);

	    defineProperty(&FlipPositionNP);
	}
        if (getGeminiProperty(PEC_MAX_STEPS_ID, value))
        {
  	    float max_steps_value;
            sscanf(value, "%f", &max_steps_value);
            PECMaxStepsN[0].value = max_steps_value;
            defineProperty(&PECMaxStepsNP);
        }
        if (getGeminiProperty(PEC_STATUS_ID, value))
        {
	    uint32_t pec_status = 0;
	    sscanf(value, "%u", &pec_status);
	    if(pec_status & 1){ // PEC_ACTIVE
	        IUSaveText(&PECStateT[PEC_STATUS_ACTIVE], "Yes");
		setPECState(PEC_ON);
	    } else {
	        IUSaveText(&PECStateT[PEC_STATUS_ACTIVE], "No");
		setPECState(PEC_OFF);
	    }
	    
    	    if(pec_status & 2){ // Freshly_Trained
	        IUSaveText(&PECStateT[PEC_STATUS_FRESH_TRAINED], "Yes");
	    } else {
	        IUSaveText(&PECStateT[PEC_STATUS_FRESH_TRAINED], "No");
	    }
	    
    	    if(pec_status & 4){ // Training_In_Progress
	        IUSaveText(&PECStateT[PEC_STATUS_TRAINING_IN_PROGRESS], "Yes");
	    } else {
	        IUSaveText(&PECStateT[PEC_STATUS_TRAINING_IN_PROGRESS], "No");
	    }
   	    if(pec_status & 6){ // Training_just_completed
	        IUSaveText(&PECStateT[PEC_STATUS_TRAINING_COMPLETED], "Yes");
	    } else {
	        IUSaveText(&PECStateT[PEC_STATUS_TRAINING_COMPLETED], "No");
	    }
    	    if(pec_status & 16){ // Training will start soon
	        IUSaveText(&PECStateT[PEC_STATUS_WILL_TRAIN], "Yes");
	    } else {
	        IUSaveText(&PECStateT[PEC_STATUS_WILL_TRAIN], "No");
	    }
    	    if(pec_status & 32){ // PEC Data Available
	        IUSaveText(&PECStateT[PEC_STATUS_DATA_AVAILABLE], "Yes");
	    } else {
	        IUSaveText(&PECStateT[PEC_STATUS_DATA_AVAILABLE], "Yes");
	    }

	    defineProperty(&PECStateTP);
        }
        if (getGeminiProperty(MANUAL_SLEWING_SPEED_ID, value))
        {
            sscanf(value, "%u", &speed);
            ManualSlewingSpeedN[0].value = speed;
            defineProperty(&ManualSlewingSpeedNP);
        }
        if (getGeminiProperty(GOTO_SLEWING_SPEED_ID, value))
        {
            sscanf(value, "%u", &speed);
            GotoSlewingSpeedN[0].value = speed;
            defineProperty(&GotoSlewingSpeedNP);
        }
        if (gemini_software_level_ >= 5.0 && getGeminiProperty(MOVE_SPEED_ID, value))
        {
            sscanf(value, "%u", &speed);
            MoveSpeedN[0].value = speed;
            defineProperty(&MoveSpeedNP);
        }
        if (getGeminiProperty(GUIDING_SPEED_ID, value))
        {
            float guidingSpeed = 0.0;
            sscanf(value, "%f", &guidingSpeed);
            GuidingSpeedBothN[GUIDING_BOTH].value = guidingSpeed;
            defineProperty(&GuidingSpeedBothNP);
        }
        if (gemini_software_level_ >= 5.0 && getGeminiProperty(GUIDING_SPEED_RA_ID, value))
        {
            float guidingSpeed = 0.0;
            sscanf(value, "%f", &guidingSpeed);
            GuidingSpeedN[GUIDING_WE].value = guidingSpeed;
            //defineProperty(&GuidingSpeedNP);
        }
        if (gemini_software_level_ >= 5.0 && getGeminiProperty(GUIDING_SPEED_DEC_ID, value))
        {
            float guidingSpeed = 0.0;
            sscanf(value, "%f", &guidingSpeed);
            GuidingSpeedN[GUIDING_NS].value = guidingSpeed;
            defineProperty(&GuidingSpeedNP);
        }
        if (getGeminiProperty(CENTERING_SPEED_ID, value))
        {
            sscanf(value, "%u", &speed);
            CenteringSpeedN[0].value = speed;
            defineProperty(&CenteringSpeedNP);
        }

        updateParkingState();
        updateMovementState();
    }
    else
    {
        deleteProperty(ParkSettingsSP.name);
        deleteProperty(ManualSlewingSpeedNP.name);
        deleteProperty(GotoSlewingSpeedNP.name);
        deleteProperty(MoveSpeedNP.name);
        deleteProperty(GuidingSpeedNP.name);
        deleteProperty(GuidingSpeedBothNP.name);
        deleteProperty(CenteringSpeedNP.name);
	deleteProperty(PECControlSP.name);
	deleteProperty(PECStateTP.name);
	deleteProperty(PECCounterTP.name);
        deleteProperty(PECMaxStepsNP.name);
        deleteProperty(PECGuidingSpeedNP.name);
        deleteProperty(ServoPrecisionNP.name);
        deleteProperty(PECEnableAtBootNP.name);
        deleteProperty(PECGuidingSpeedNP.name);
        deleteProperty(VersionTP.name);
	deleteProperty(FlipPositionNP.name);
    }

    return true;
}
bool LX200Gemini::ISNewText(const char *dev, const char *name, char **texts, char **names, int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        if (!strcmp(name, PECStateTP.name))
        {
            IUUpdateText(&PECStateTP, texts, names, n);
            IDSetText(&PECStateTP, nullptr);
        }
	if (!strcmp(name, PECCounterTP.name))
	{
	    IUUpdateText(&PECCounterTP, texts, names, n);
            IDSetText(&PECCounterTP, nullptr);
	}
    }

    return LX200Generic::ISNewText(dev, name, texts, names, n);
}

bool LX200Gemini::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        if (!strcmp(name, StartupModeSP.name))
        {
            IUUpdateSwitch(&StartupModeSP, states, names, n);
            StartupModeSP.s = IPS_OK;
            if (isConnected())
                LOG_INFO("Startup mode will take effect on future connections.");
            IDSetSwitch(&StartupModeSP, nullptr);
            return true;
        }

        if (!strcmp(name, ParkSettingsSP.name))
        {
            IUUpdateSwitch(&ParkSettingsSP, states, names, n);
            ParkSettingsSP.s = IPS_OK;
            IDSetSwitch(&ParkSettingsSP, nullptr);
            return true;
        }

        if (gemini_software_level_ >= 5.0 && !strcmp(name, PECStateSP.name))
        {
	  IUUpdateSwitch(&PECStateSP, states, names, n);
	  IDSetSwitch(&PECStateSP, nullptr);
	    for(int i = 0; i<n; ++i)
	    {
	        if (!strcmp(names[i], PECStateS[PEC_ON].name))
	        {
		    if(PECStateS[PEC_ON].s == ISS_ON)
		    {
		        char valueString[16] = {0};
			setGeminiProperty(PEC_REPLAY_ON_ID, valueString);
		    }
	        }
	        if (!strcmp(names[i], PECStateS[PEC_OFF].name))
	        {
		    if(PECStateS[PEC_OFF].s == ISS_ON)
		    {
		        char valueString[16] = {0};
			setGeminiProperty(PEC_REPLAY_OFF_ID, valueString);
		    }
	        }
	    }
	}

        if (gemini_software_level_ >= 6.0 && !strcmp(name, FlipControlSP.name))
        {
	    IUUpdateSwitch(&FlipControlSP, states, names, n);
	    FlipControlSP.s = IPS_OK;
	    IDSetSwitch(&FlipControlSP, nullptr);
	    int32_t flipEnabled = 0;
	    for(int i = 0; i<n; ++i){
	        if (!strcmp(names[i], FlipControlS[FLIP_EAST_CONTROL].name)){
		    if(FlipControlS[FLIP_EAST_CONTROL].s == ISS_ON){
		        flipEnabled |= 1;
		        LOGF_INFO("FlipControl: EAST ON  <%i>", flipEnabled);
		    } else {
		        flipEnabled &= 0xfffffffe;
		        LOGF_INFO("FlipControl: EAST OFF  <%i>", flipEnabled);
		    }
	      }
	    
	    if (!strcmp(names[i], FlipControlS[FLIP_EAST_CONTROL].name)){
	        if(FlipControlS[FLIP_WEST_CONTROL].s == ISS_ON){		
		    flipEnabled |= 2;
		    LOGF_INFO("FlipControl: WEST ON  <%i>", flipEnabled);
		} else {
		    flipEnabled &= 0xfffffffd;
		    LOGF_INFO("FlipControl: WEST OFF  <%i>", flipEnabled);
		}
	    }
	  }
	  char valueString[32] = {0};
	  snprintf(valueString, 32, "%i", flipEnabled);
	  LOGF_INFO("FlipControl: <%s>", valueString);
	  setGeminiProperty(FLIP_POINTS_ENABLED_ID, valueString);
	}
	
        if (gemini_software_level_ >= 5.0 && !strcmp(name, PECControlSP.name))
        {
	    for(int i = 0; i<n; ++i){
	        if (!strcmp(names[i], PECControlS[PEC_START_TRAINING].name))
	        {
		    char valueString[16] = {0};
		    setGeminiProperty(PEC_START_TRAINING_ID, valueString);
	        } else if (!strcmp(names[i], PECControlS[PEC_ABORT_TRAINING].name))
	        {
		    char valueString[16] = {0};
		    setGeminiProperty(PEC_ABORT_TRAINING_ID, valueString);
	        }

	    }
	    IUUpdateSwitch(&PECControlSP, states, names, n);
	    PECControlSP.s = IPS_OK;
	    IDSetSwitch(&PECControlSP, nullptr);
            return true;
        }


    }

    return LX200Generic::ISNewSwitch(dev, name, states, names, n);
}

bool LX200Gemini::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        char valueString[16] = {0};
        snprintf(valueString, 16, "%2.0f", values[0]);

        if (!strcmp(name, ManualSlewingSpeedNP.name))
        {
            LOGF_DEBUG("Trying to set manual slewing speed of: %f", values[0]);

            if (!isSimulation() && !setGeminiProperty(MANUAL_SLEWING_SPEED_ID, valueString))
            {
                ManualSlewingSpeedNP.s = IPS_ALERT;
                IDSetNumber(&ManualSlewingSpeedNP, "Error setting manual slewing speed");
                return false;
            }

            ManualSlewingSpeedNP.s    = IPS_OK;
            ManualSlewingSpeedN[0].value = values[0];
            IDSetNumber(&ManualSlewingSpeedNP, "Manual slewing speed set to %f", values[0]);

            return true;
        }
        if (!strcmp(name, GotoSlewingSpeedNP.name))
        {
            LOGF_DEBUG("Trying to set goto slewing speed of: %f", values[0]);

            if (!isSimulation() && !setGeminiProperty(GOTO_SLEWING_SPEED_ID, valueString))
            {
                GotoSlewingSpeedNP.s = IPS_ALERT;
                IDSetNumber(&GotoSlewingSpeedNP, "Error setting goto slewing speed");
                return false;
            }

            GotoSlewingSpeedNP.s       = IPS_OK;
            GotoSlewingSpeedN[0].value = values[0];
            IDSetNumber(&GotoSlewingSpeedNP, "Goto slewing speed set to %f", values[0]);

            return true;
        }
        if (gemini_software_level_ >= 5.0 && !strcmp(name, MoveSpeedNP.name))
        {
            LOGF_DEBUG("Trying to set move speed of: %f", values[0]);

            if (!isSimulation() && !setGeminiProperty(MOVE_SPEED_ID, valueString))
            {
                MoveSpeedNP.s = IPS_ALERT;
                IDSetNumber(&MoveSpeedNP, "Error setting move speed");
                return false;
            }

            MoveSpeedNP.s       = IPS_OK;
            MoveSpeedN[0].value = values[0];
            IDSetNumber(&MoveSpeedNP, "Move speed set to %f", values[0]);

            return true;
        }
        if (gemini_software_level_ >= 5.0 && !strcmp(name, GuidingSpeedBothNP.name))
        {
            LOGF_DEBUG("Trying to set guiding speed of: %f", values[0]);

	    for(int i = 0; i<n; ++i){
	        if (!strcmp(names[i], GuidingSpeedBothN[GUIDING_BOTH].name))
		{
		    // Special formatting for guiding speed
		    snprintf(valueString, 16, "%1.1f", values[0]);
		    
		    if (!isSimulation() && !setGeminiProperty(GUIDING_SPEED_ID, valueString))
		    {
  		        GuidingSpeedBothNP.s = IPS_ALERT;
			IDSetNumber(&GuidingSpeedBothNP, "Error setting guiding speed");
			return false;
		    }
		    
		}
	    }
	    
	    GuidingSpeedBothN[GUIDING_BOTH].value = values[0];
            GuidingSpeedBothNP.s                  = IPS_OK;
            IDSetNumber(&GuidingSpeedBothNP, "Guiding speed set to %f", values[0]);

            return true;
        }
        if (!strcmp(name, GuidingSpeedNP.name))
        {

	    for(int i = 0; i<n; ++i){
		if (!strcmp(names[i], GuidingSpeedN[GUIDING_WE].name))
		{
		    // Special formatting for guiding speed
		    snprintf(valueString, 16, "%1.1f", values[i]);
		    if (!isSimulation() && !setGeminiProperty(GUIDING_SPEED_RA_ID, valueString)){
		        GuidingSpeedN[GUIDING_WE].value = values[i];
		        GuidingSpeedNP.s = IPS_ALERT;
			IDSetNumber(&GuidingSpeedNP, "Error Setting Guiding WE");
			return false;
		    }
			
		}
		if (!strcmp(names[i], GuidingSpeedN[GUIDING_NS].name))
		{
		    // Special formatting for guiding speed
		    snprintf(valueString, 16, "%1.1f", values[i]);
		    GuidingSpeedNP.s = IPS_ALERT;
		    if (!isSimulation() && !setGeminiProperty(GUIDING_SPEED_DEC_ID, valueString))
		    {
		        GuidingSpeedN[GUIDING_NS].value = values[i];
		        GuidingSpeedNP.s = IPS_ALERT;
			IDSetNumber(&GuidingSpeedNP, "Error Setting Guiding WE");
			return false;
		    }
		}
	    }
	    
            GuidingSpeedNP.s       = IPS_OK;
            IDSetNumber(&GuidingSpeedNP, "Guiding speed set to RA:%f DEC:%f",  GuidingSpeedN[GUIDING_WE].value, GuidingSpeedN[GUIDING_NS].value);

            return true;
        }
        if (gemini_software_level_ >= 6.0 && !strcmp(name, FlipPositionNP.name))
        {
	  int eastD = 0;
	  int eastM = 0;
	  int westD = 0;
	  int westM = 0;
	  
	  
	    for(int i = 0; i<n; ++i){
		if (!strcmp(names[i], FlipPositionN[FLIP_EAST_DEGREE_VALUE].name)){
		    eastD = values[i];
		}
		if (!strcmp(names[i], FlipPositionN[FLIP_EAST_MIN_VALUE].name)){
		    eastM = values[i];
		}
		if (!strcmp(names[i], FlipPositionN[FLIP_WEST_DEGREE_VALUE].name)){
		    westD = values[i];
		}
		if (!strcmp(names[i], FlipPositionN[FLIP_WEST_MIN_VALUE].name)){
		    westM = values[i];
		}
	    }
	    char east[16] = {0};
	    snprintf(east, 16, "%id%i", eastD, eastM);
	    FlipPositionN[FLIP_EAST_DEGREE_VALUE].value = eastD;
	    FlipPositionN[FLIP_EAST_MIN_VALUE].value = eastM;

	    char west[16] = {0};
	    snprintf(west, 16, "%id%i", westD, westM);
	    FlipPositionN[FLIP_WEST_DEGREE_VALUE].value = westD;
	    FlipPositionN[FLIP_WEST_MIN_VALUE].value = westM;

	    if (!isSimulation() &&
		!setGeminiProperty(FLIP_POINT_EAST_ID, east) &&
		!setGeminiProperty(FLIP_POINT_WEST_ID, west)){
	        FlipPositionNP.s = IPS_ALERT;
	        IDSetNumber(&FlipPositionNP, "Error Setting Guiding WE");
	    }
	    
	    IDSetNumber(&FlipPositionNP, "OK");
            FlipPositionNP.s = IPS_OK;
            return true;
        }
        if (!strcmp(name, CenteringSpeedNP.name))
        {
            LOGF_DEBUG("Trying to set centering speed of: %f", values[0]);

            if (!isSimulation() && !setGeminiProperty(CENTERING_SPEED_ID, valueString))
            {
                CenteringSpeedNP.s = IPS_ALERT;
                IDSetNumber(&CenteringSpeedNP, "Error setting centering speed");
                return false;
            }

            CenteringSpeedNP.s       = IPS_OK;
            CenteringSpeedN[0].value = values[0];
            IDSetNumber(&CenteringSpeedNP, "Centering speed set to %f", values[0]);

            return true;
        }
        if (!strcmp(name, PECMaxStepsNP.name))
        {
            PECMaxStepsNP.s       = IPS_OK;
            PECMaxStepsN[0].value = values[0];
            IDSetNumber(&PECMaxStepsNP, "Max steps set to %f", values[0]);
            return true;
        }
        if (gemini_software_level_ >= 5.0 && !strcmp(name, PECGuidingSpeedNP.name))
        {
            PECGuidingSpeedNP.s       = IPS_OK;
            PECGuidingSpeedN[0].value = values[0];
            IDSetNumber(&PECGuidingSpeedNP, "Guiding Speed set to %f", values[0]);
            return true;
        }
        if (gemini_software_level_ >= 6.0 && !strcmp(name, ServoPrecisionNP.name))
        {

	    for(int i = 0; i<n; ++i){
	        if (!strcmp(names[i], ServoPrecisionN[SERVO_RA].name))
		{
		    uint8_t servo_value = static_cast<uint8_t>(values[i]);
		    ServoPrecisionN[SERVO_RA].value = (float)(servo_value & 1);
		}
	        if (!strcmp(names[i], ServoPrecisionN[SERVO_DEC].name))
		{
		    uint8_t servo_value = static_cast<uint8_t>(values[i]);
		    ServoPrecisionN[SERVO_DEC].value = (float)((servo_value));
		}
	    }

	    uint8_t pointingValue;
	    pointingValue  = static_cast<uint8_t>(ServoPrecisionN[SERVO_RA].value);
	    pointingValue |= static_cast<uint8_t>(ServoPrecisionN[SERVO_DEC].value) << 1;

            snprintf(valueString, 16, "%u", pointingValue);

	    if (!isSimulation() && !setGeminiProperty(SERVO_POINTING_PRECISION_ID, valueString))
            {
                ServoPrecisionNP.s = IPS_ALERT;
                IDSetNumber(&ServoPrecisionNP, "Error setting servo speed");
                return false;
            }
            ServoPrecisionNP.s       = IPS_OK;
            ServoPrecisionN[0].value = values[0];
            IDSetNumber(&ServoPrecisionNP, "Servo Precision %f", values[0]);
            return true;
        }
        if (gemini_software_level_ >= 5.0 && !strcmp(name, PECEnableAtBootNP.name))
        {

	    uint32_t enable_pec_value = static_cast<uint32_t>(values[0]);
	    PECEnableAtBootN[0].value = (float)(enable_pec_value);
            snprintf(valueString, 16, "%u", enable_pec_value);

	    if (!isSimulation() && !setGeminiProperty(PEC_ENABLE_AT_BOOT_ID, valueString))
            {
                PECEnableAtBootNP.s = IPS_ALERT;
                IDSetNumber(&PECEnableAtBootNP, "Error setting pec at boot");
                return false;
            }
            PECEnableAtBootNP.s       = IPS_OK;
            PECEnableAtBootN[0].value = values[0];
            IDSetNumber(&PECEnableAtBootNP, "PEC at boot %f", values[0]);
            return true;
        }
    }

    //  If we didn't process it, continue up the chain, let somebody else give it a shot
    return LX200Generic::ISNewNumber(dev, name, values, names, n);
}

bool LX200Gemini::checkConnection()
{
    if (isSimulation())
        return true;

    // Response
    char response[8] = { 0 };
    int rc = 0, nbytes_read = 0, nbytes_written = 0;

    LOGF_DEBUG("CMD: <%#02X>", 0x06);

    tcflush(PortFD, TCIFLUSH);

    char ack[1] = { 0x06 };

    if ((rc = tty_write(PortFD, ack, 1, &nbytes_written)) != TTY_OK)
    {
        char errmsg[256];
        tty_error_msg(rc, errmsg, 256);
        LOGF_ERROR("Error writing to device %s (%d)", errmsg, rc);
        return false;
    }

    // Read response
    if ((rc = tty_read_section(PortFD, response, '#', GEMINI_TIMEOUT, &nbytes_read)) != TTY_OK)
    {
        char errmsg[256];
        tty_error_msg(rc, errmsg, 256);
        LOGF_ERROR("Error reading from device %s (%d)", errmsg, rc);
        return false;
    }

    //response[1] = '\0';

    tcflush(PortFD, TCIFLUSH);

    LOGF_DEBUG("RES: <%s>", response);

    // If waiting for selection of startup mode, let us select it
    if (response[0] == 'b')
    {
        LOG_DEBUG("Mount is waiting for selection of the startup mode.");
        char cmd[4]     = "bC#";
        int startupMode = IUFindOnSwitchIndex(&StartupModeSP);
        if (startupMode == WARM_START)
            strncpy(cmd, "bW#", 4);
        else if (startupMode == WARM_RESTART)
            strncpy(cmd, "bR#", 4);

        LOGF_DEBUG("CMD: <%s>", cmd);

        if ((rc = tty_write(PortFD, cmd, 4, &nbytes_written)) != TTY_OK)
        {
            char errmsg[256];
            tty_error_msg(rc, errmsg, 256);
            LOGF_ERROR("Error writing to device %s (%d)", errmsg, rc);
            return false;
        }

        tcflush(PortFD, TCIFLUSH);

        // Send ack again and check response
        return checkConnection();
    }
    else if (response[0] == 'B')
    {
        LOG_DEBUG("Initial startup message is being displayed.");
    }
    else if (response[0] == 'S')
    {
        LOG_DEBUG("Cold start in progress.");
    }
    else if (response[0] == 'G')
    {
        updateParkingState();
        updateMovementState();
        LOG_DEBUG("Startup complete with equatorial mount selected.");
    }
    else if (response[0] == 'A')
    {
        LOG_DEBUG("Startup complete with Alt-Az mount selected.");
    }

    return true;
}

bool LX200Gemini::isSlewComplete()
{
    LX200Gemini::MovementState movementState = getMovementState();

    if (movementState == TRACKING || movementState == GUIDING || movementState == NO_MOVEMENT)
        return true;
    else
        return false;
}

bool LX200Gemini::ReadScopeStatus()
{
    LOGF_DEBUG("ReadScopeStatus: TrackState is <%d>", TrackState);

    if (!isConnected())
        return false;

    if (isSimulation())
        return LX200Generic::ReadScopeStatus();

    if(m_isSleeping)
    {
        return true;
    }

    if (TrackState == SCOPE_SLEWING)
    {
        updateMovementState();

        EqNP.s = IPS_BUSY;
        IDSetNumber(&EqNP, NULL);

        // Check if LX200 is done slewing
        if (isSlewComplete())
        {
            // Set slew mode to "Centering"
            IUResetSwitch(&SlewRateSP);
            SlewRateS[SLEW_CENTERING].s = ISS_ON;
            IDSetSwitch(&SlewRateSP, nullptr);

            EqNP.s = IPS_OK;
            IDSetNumber(&EqNP, NULL);

            LOG_INFO("Slew is complete. Tracking...");
        }
    }
    else if (TrackState == SCOPE_PARKING)
    {
        updateParkingState();

        if (isSlewComplete())
        {
            LOG_DEBUG("Park is complete ...");
            SetParked(true);
            sleepMount();

            EqNP.s = IPS_IDLE;
            IDSetNumber(&EqNP, NULL);

            return true;
        }
    }

    if (getLX200RA(PortFD, &currentRA) < 0 || getLX200DEC(PortFD, &currentDEC) < 0)
    {
        LOG_ERROR("Error reading RA/DEC.");
        return false;
    }

    NewRaDec(currentRA, currentDEC);

    syncSideOfPier();
    syncPec();
    return true;
}

void LX200Gemini::syncSideOfPier()
{
    // Send ':Gm#'
    const char *cmd = ":Gm#";
    // Response
    char response[8] = { 0 };
    int rc = 0, nbytes_read = 0, nbytes_written = 0;

    LOGF_DEBUG("CMD: <%s>", cmd);

    tcflush(PortFD, TCIOFLUSH);

    if ((rc = tty_write(PortFD, cmd, 5, &nbytes_written)) != TTY_OK)
    {
        char errmsg[256];
        tty_error_msg(rc, errmsg, 256);
        LOGF_ERROR("Error writing to device %s (%d)", errmsg, rc);
        return;
    }

    if ((rc = tty_read_section(PortFD, response, '#', GEMINI_TIMEOUT, &nbytes_read)) != TTY_OK)
    {
        char errmsg[256];
        tty_error_msg(rc, errmsg, 256);
        LOGF_ERROR("Error reading from device %s (%d)", errmsg, rc);
        return;
    }

    response[nbytes_read - 1] = '\0';

    tcflush(PortFD, TCIOFLUSH);

    //LOGF_DEBUG("RES: <%s>", response);

    // fix to pier side read from the mount using the hour angle as a guide
    // see https://www.indilib.org/forum/general/6785-side-of-pier-problem-bug.html?start=12#52492
    // for a description of the problem and the proposed fix
    //
    auto lst = get_local_sidereal_time(this->LocationN[LOCATION_LONGITUDE].value);
    auto ha = rangeHA(lst - currentRA);
    auto pointingState = PIER_UNKNOWN;

    if (ha >= -5.0 && ha <= 5.0)
    {
        // mount pier side is used unchanged
        pointingState = response[0] == 'E' ? PIER_EAST : PIER_WEST;
    }
    else if (ha <= -7.0 || ha >= 7.0)
    {
        // mount pier side is reversed
        pointingState = response[0] == 'W' ? PIER_EAST : PIER_WEST;
    }
    else
    {
        // use hour angle because the pier side changes spontaneously near +-6h
        pointingState = ha > 0 ? PIER_EAST : PIER_WEST;
    }

    LOGF_DEBUG("RES: <%s>, lst %f, ha %f, pierSide %d", response, lst, ha, pointingState);

    setPierSide(pointingState);
}


bool LX200Gemini::Park()
{
    char cmd[6] = ":hP#";

    int parkSetting = IUFindOnSwitchIndex(&ParkSettingsSP);

    if (parkSetting == PARK_STARTUP)
        strncpy(cmd, ":hC#", 5);
    else if (parkSetting == PARK_ZENITH)
        strncpy(cmd, ":hZ#", 5);

    // Response
    int rc = 0, nbytes_written = 0;

    LOGF_DEBUG("CMD: <%s>", cmd);

    tcflush(PortFD, TCIOFLUSH);

    if ((rc = tty_write(PortFD, cmd, 5, &nbytes_written)) != TTY_OK)
    {
        char errmsg[256];
        tty_error_msg(rc, errmsg, 256);
        LOGF_ERROR("Error writing to device %s (%d)", errmsg, rc);
        return false;
    }

    tcflush(PortFD, TCIOFLUSH);

    ParkSP.s   = IPS_BUSY;
    TrackState = SCOPE_PARKING;

    updateParkingState();
    return true;
}

bool LX200Gemini::UnPark()
{
    wakeupMount();

    SetParked(false);
    TrackState = SCOPE_TRACKING;

    updateParkingState();
    return true;
}

bool LX200Gemini::sleepMount()
{
    const char *cmd = ":hN#";

    // Response
    int rc = 0, nbytes_written = 0;

    LOGF_DEBUG("CMD: <%s>", cmd);

    tcflush(PortFD, TCIOFLUSH);

    if ((rc = tty_write(PortFD, cmd, 5, &nbytes_written)) != TTY_OK)
    {
        char errmsg[256];
        tty_error_msg(rc, errmsg, 256);
        LOGF_ERROR("Error writing to device %s (%d)", errmsg, rc);
        return false;
    }

    tcflush(PortFD, TCIOFLUSH);

    m_isSleeping = true;
    LOG_INFO("Mount is sleeping...");
    return true;
}

bool LX200Gemini::wakeupMount()
{
    const char *cmd = ":hW#";

    // Response
    int rc = 0, nbytes_written = 0;

    LOGF_DEBUG("CMD: <%s>", cmd);

    tcflush(PortFD, TCIOFLUSH);

    if ((rc = tty_write(PortFD, cmd, 5, &nbytes_written)) != TTY_OK)
    {
        char errmsg[256];
        tty_error_msg(rc, errmsg, 256);
        LOGF_ERROR("Error writing to device %s (%d)", errmsg, rc);
        return false;
    }

    tcflush(PortFD, TCIOFLUSH);

    m_isSleeping = false;
    LOG_INFO("Mount is awake...");
    return true;
}

void LX200Gemini::setTrackState(INDI::Telescope::TelescopeStatus state)
{
    if (TrackState != state)
        TrackState = state;
}

void LX200Gemini::updateMovementState()
{
    LX200Gemini::MovementState movementState = getMovementState();

    switch (movementState)
    {
        case NO_MOVEMENT:
            if (priorParkingState == PARKED)
                setTrackState(SCOPE_PARKED);
            else
                setTrackState(SCOPE_IDLE);
            break;

        case TRACKING:
        case GUIDING:
            setTrackState(SCOPE_TRACKING);
            break;

        case CENTERING:
        case SLEWING:
            setTrackState(SCOPE_SLEWING);
            break;

        case STALLED:
            setTrackState(SCOPE_IDLE);
            break;
    }
}

void LX200Gemini::updateParkingState()
{
    LX200Gemini::ParkingState parkingState = getParkingState();

    if (parkingState != priorParkingState)
    {
        if (parkingState == PARKED)
            SetParked(true);
        else if (parkingState == NOT_PARKED)
            SetParked(false);
    }
    priorParkingState = parkingState;
}

LX200Gemini::MovementState LX200Gemini::getMovementState()
{
    const char *cmd = ":Gv#";
    char response[2] = { 0 };
    int rc = 0, nbytes_read = 0, nbytes_written = 0;

    LOGF_DEBUG("CMD: <%s>", cmd);

    tcflush(PortFD, TCIOFLUSH);

    if ((rc = tty_write(PortFD, cmd, 5, &nbytes_written)) != TTY_OK)
    {
        char errmsg[256];
        tty_error_msg(rc, errmsg, 256);
        LOGF_ERROR("Error writing to device %s (%d)", errmsg, rc);
        return LX200Gemini::MovementState::NO_MOVEMENT;
    }

    if ((rc = tty_read(PortFD, response, 1, GEMINI_TIMEOUT, &nbytes_read)) != TTY_OK)
    {
        char errmsg[256];
        tty_error_msg(rc, errmsg, 256);
        LOGF_ERROR("Error reading from device %s (%d)", errmsg, rc);
        return LX200Gemini::MovementState::NO_MOVEMENT;
    }

    response[1] = '\0';

    tcflush(PortFD, TCIOFLUSH);

    LOGF_DEBUG("RES: <%s>", response);

    switch (response[0])
    {
        case 'N':
            return LX200Gemini::MovementState::NO_MOVEMENT;

        case 'T':
            return LX200Gemini::MovementState::TRACKING;

        case 'G':
            return LX200Gemini::MovementState::GUIDING;

        case 'C':
            return LX200Gemini::MovementState::CENTERING;

        case 'S':
            return LX200Gemini::MovementState::SLEWING;

        case '!':
            return LX200Gemini::MovementState::STALLED;

        default:
            return LX200Gemini::MovementState::NO_MOVEMENT;
    }
}

LX200Gemini::ParkingState LX200Gemini::getParkingState()
{
    const char *cmd = ":h?#";
    char response[2] = { 0 };
    int rc = 0, nbytes_read = 0, nbytes_written = 0;

    LOGF_DEBUG("CMD: <%s>", cmd);

    tcflush(PortFD, TCIOFLUSH);

    if ((rc = tty_write(PortFD, cmd, 5, &nbytes_written)) != TTY_OK)
    {
        char errmsg[256];
        tty_error_msg(rc, errmsg, 256);
        LOGF_ERROR("Error writing to device %s (%d)", errmsg, rc);
        return LX200Gemini::ParkingState::NOT_PARKED;
    }

    if ((rc = tty_read(PortFD, response, 1, GEMINI_TIMEOUT, &nbytes_read)) != TTY_OK)
    {
        char errmsg[256];
        tty_error_msg(rc, errmsg, 256);
        LOGF_ERROR("Error reading from device %s (%d)", errmsg, rc);
        return LX200Gemini::ParkingState::NOT_PARKED;
    }

    response[1] = '\0';

    tcflush(PortFD, TCIOFLUSH);

    LOGF_DEBUG("RES: <%s>", response);

    switch (response[0])
    {
        case '0':
            return LX200Gemini::ParkingState::NOT_PARKED;

        case '1':
            return LX200Gemini::ParkingState::PARKED;

        case '2':
            return LX200Gemini::ParkingState::PARK_IN_PROGRESS;

        default:
            return LX200Gemini::ParkingState::NOT_PARKED;
    }
}

bool LX200Gemini::saveConfigItems(FILE *fp)
{
    LX200Generic::saveConfigItems(fp);

    IUSaveConfigSwitch(fp, &StartupModeSP);
    IUSaveConfigSwitch(fp, &ParkSettingsSP);

    return true;
}

bool LX200Gemini::getGeminiProperty(uint32_t propertyNumber, char* value)
{
    int rc = TTY_OK;
    int nbytes = 0;
    char prefix[16] = {0};
    char cmd[16] = {0};

    switch(propertyNumber){
    case MOVE_SPEED_ID:
    case GUIDING_SPEED_RA_ID:
    case GUIDING_SPEED_DEC_ID:
    case PEC_START_TRAINING_ID:
    case PEC_ABORT_TRAINING_ID:
    case PEC_REPLAY_ON_ID:
    case PEC_REPLAY_OFF_ID:
      if(gemini_software_level_ < 5.0)
      {
        LOGF_ERROR("Error Gemini Firmware Level %f does not support command %i ", gemini_software_level_, propertyNumber);
	return false;
      }
      break;
    case PEC_ENABLE_AT_BOOT_ID:
      if(gemini_software_level_ < 5.2)
      {
        LOGF_ERROR("Error Gemini Firmware Level %f does not support command %i ", gemini_software_level_, propertyNumber);
	return false;
      }
      break;
    case FLIP_POINT_EAST_ID:
    case FLIP_POINT_WEST_ID:
    case FLIP_POINTS_ENABLED_ID:
    case SERVO_POINTING_PRECISION_ID:
    case SERVO_FIRMWARE:
      if(gemini_software_level_ < 6)
      {
        LOGF_ERROR("Error Gemini Firmware Level %f does not support command %i ", gemini_software_level_, propertyNumber);
	return false;
      }
      break;      
    default:;
    }
    
    snprintf(prefix, 16, "<%d:", propertyNumber);

    uint8_t checksum = calculateChecksum(prefix);

    snprintf(cmd, 16, "%s%c#", prefix, checksum);

    LOGF_DEBUG("CMD: <%s>", cmd);

    if ((rc = tty_write_string(PortFD, cmd, &nbytes)) != TTY_OK)
    {
        char errmsg[256];
        tty_error_msg(rc, errmsg, 256);
        LOGF_ERROR("Error writing to device %s (%d)", errmsg, rc);
        return false;
    }

    if ((rc = tty_read_section(PortFD, value, '#', GEMINI_TIMEOUT, &nbytes)) != TTY_OK)
    {
        char errmsg[256];
        tty_error_msg(rc, errmsg, 256);
        LOGF_ERROR("Error reading from device %s (%d)", errmsg, rc);
        return false;
    }

    value[nbytes - 1] = '\0';

    tcflush(PortFD, TCIFLUSH);

    LOGF_DEBUG("RES: <%s>", value);
    return true;
}

int LX200Gemini::SendPulseCmd(int8_t direction, uint32_t duration_msec)
{
    return ::SendPulseCmd(PortFD, direction, duration_msec, true, 1000);
}

bool LX200Gemini::setGeminiProperty(uint32_t propertyNumber, char* value)
{
    int rc = TTY_OK;
    int nbytes_written = 0;
    char prefix[16] = {0};
    char cmd[16] = {0};

    switch(propertyNumber){
    case MOVE_SPEED_ID:
    case GUIDING_SPEED_RA_ID:
    case GUIDING_SPEED_DEC_ID:
    case PEC_START_TRAINING_ID:
    case PEC_ABORT_TRAINING_ID:
    case PEC_REPLAY_ON_ID:
    case PEC_REPLAY_OFF_ID:
      if(gemini_software_level_ < 5.0)
      {
        LOGF_ERROR("Error Gemini Firmware Level %f does not support command %i ", gemini_software_level_, propertyNumber);
	return false;
      }
      break;
    case PEC_ENABLE_AT_BOOT_ID:
      if(gemini_software_level_ < 5.2)
      {
        LOGF_ERROR("Error Gemini Firmware Level %f does not support command %i ", gemini_software_level_, propertyNumber);
	return false;
      }
      break;      
    case SERVO_POINTING_PRECISION_ID:
    case SERVO_FIRMWARE:
      if(gemini_software_level_ < 6)
      {
        LOGF_ERROR("Error Gemini Firmware Level %f does not support command %i ", gemini_software_level_, propertyNumber);
	return false;
      }
      break;      
    default:;
    }

    snprintf(prefix, 16, ">%d:%s", propertyNumber, value);

    uint8_t checksum = calculateChecksum(prefix);

    snprintf(cmd, 16, "%s%c#", prefix, checksum);

    LOGF_DEBUG("CMD: <%s>", cmd);
    
    if ((rc = tty_write_string(PortFD, cmd, &nbytes_written)) != TTY_OK)
    {
        char errmsg[256];
        tty_error_msg(rc, errmsg, 256);
        LOGF_ERROR("Error writing to device %s (%d)", errmsg, rc);
        return false;
    }

    tcflush(PortFD, TCIFLUSH);

    return true;
}

bool LX200Gemini::SetTrackMode(uint8_t mode)
{
    int rc = TTY_OK, nbytes_written = 0;
    char prefix[16] = {0};
    char cmd[16] = {0};

    snprintf(prefix, 16, ">130:%d", mode + 131);

    uint8_t checksum = calculateChecksum(prefix);

    snprintf(cmd, 16, "%s%c#", prefix, checksum);

    LOGF_DEBUG("CMD: <%s>", cmd);

    if ((rc = tty_write_string(PortFD, cmd, &nbytes_written)) != TTY_OK)
    {
        char errmsg[256];
        tty_error_msg(rc, errmsg, 256);
        LOGF_ERROR("Error writing to device %s (%d)", errmsg, rc);
        return false;
    }

    tcflush(PortFD, TCIFLUSH);

    return true;
}

bool LX200Gemini::SetTrackEnabled(bool enabled)
{
    if (enabled)
    {
        return wakeupMount();
    }
    else
    {
        return sleepMount();
    }
}

uint8_t LX200Gemini::calculateChecksum(char *cmd)
{
    uint8_t result = cmd[0];

    for (size_t i = 1; i < strlen(cmd); i++)
        result = result ^ cmd[i];

    result = result % 128;
    result += 64;

    return result;
}
