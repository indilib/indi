/*
    LX200 LX200_OnStep
    Based on LX200 classic, (alain@zwingelstein.org)
    Contributors:
    James Lan https://github.com/james-lan
    Ray Wells https://github.com/blueshawk
    Copyright (C) 2003 Jasem Mutlaq (mutlaqja@ikarustech.com)

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

//#define DEBUG_TRACKSTATE


#include "lx200_OnStep.h"


#include <mutex>

#define LIBRARY_TAB  "Library"
#define FIRMWARE_TAB "Firmware data"
#define STATUS_TAB "ONStep Status"
#define PEC_TAB "PEC"
#define ALIGN_TAB "Align"
#define OUTPUT_TAB "Outputs"
#define ENVIRONMENT_TAB "Weather"
#define ROTATOR_TAB "Rotator"

#define RA_AXIS     0
#define DEC_AXIS    1

extern std::mutex lx200CommsLock;


LX200_OnStep::LX200_OnStep() : LX200Generic(), WI(this), RotatorInterface(this)
{
    currentCatalog    = LX200_STAR_C;
    currentSubCatalog = 0;

    setVersion(1, 23);   // don't forget to update libindi/drivers.xml

    setLX200Capability(LX200_HAS_TRACKING_FREQ | LX200_HAS_SITES | LX200_HAS_ALIGNMENT_TYPE | LX200_HAS_PULSE_GUIDING |
                       LX200_HAS_PRECISE_TRACKING_FREQ);

    SetTelescopeCapability(GetTelescopeCapability() |
                           TELESCOPE_CAN_CONTROL_TRACK |
                           TELESCOPE_HAS_TRACK_RATE |
                           TELESCOPE_CAN_HOME_GO |
                           TELESCOPE_CAN_HOME_SET, 10);

    //CAN_ABORT, CAN_GOTO ,CAN_PARK ,CAN_SYNC ,HAS_LOCATION ,HAS_TIME ,HAS_TRACK_MODE Already inherited from lx200generic,
    // 4 stands for the number of Slewrate Buttons as defined in Inditelescope.cpp
    //setLX200Capability(LX200_HAS_FOCUS | LX200_HAS_TRACKING_FREQ | LX200_HAS_ALIGNMENT_TYPE | LX200_HAS_SITES | LX200_HAS_PULSE_GUIDING);
    //
    // Get generic capabilities but discard the following:
    // LX200_HAS_FOCUS


    FI::SetCapability(FOCUSER_CAN_ABS_MOVE | FOCUSER_CAN_REL_MOVE | FOCUSER_CAN_ABORT);
    // Unused option: FOCUSER_HAS_VARIABLE_SPEED

    RI::SetCapability(ROTATOR_CAN_ABORT | ROTATOR_CAN_HOME | ROTATOR_HAS_BACKLASH);
    //     /*{
    //         ROTATOR_CAN_ABORT          = 1 << 0, /*!< Can the Rotator abort motion once started? */
    //         ROTATOR_CAN_HOME           = 1 << 1, /*!< Can the Rotator go to home position? */
    //         ROTATOR_CAN_SYNC           = 1 << 2, /*!< Can the Rotator sync to specific tick? */ /*Not supported */
    //         ROTATOR_CAN_REVERSE        = 1 << 3, /*!< Can the Rotator reverse direction? */ //It CAN reverse, but there's no way to query the direction
    //         ROTATOR_HAS_BACKLASH       = 1 << 4  /*!< Can the Rotatorer compensate for backlash? */
    //     //}*/

}

const char *LX200_OnStep::getDefaultName()
{
    return "LX200 OnStep";
}

bool LX200_OnStep::initProperties()
{

    LX200Generic::initProperties();
    FI::initProperties(FOCUS_TAB);
    WI::initProperties(ENVIRONMENT_TAB, ENVIRONMENT_TAB);
    RI::initProperties(ROTATOR_TAB);
    SetParkDataType(PARK_RA_DEC);

    initSlewRates();

    //FocuserInterface
    //Initial, these will be updated later.
    FocusRelPosNP[0].setMin(0.);
    FocusRelPosNP[0].setMax(30000.);
    FocusRelPosNP[0].setValue(0);
    FocusRelPosNP[0].setStep(10);
    FocusAbsPosNP[0].setMin(0.);
    FocusAbsPosNP[0].setMax(60000.);
    FocusAbsPosNP[0].setValue(0);
    FocusAbsPosNP[0].setStep(10);


    // ============== MAIN_CONTROL_TAB
    IUFillSwitch(&ReticS[0], "PLUS", "Light", ISS_OFF);
    IUFillSwitch(&ReticS[1], "MOINS", "Dark", ISS_OFF);
    IUFillSwitchVector(&ReticSP, ReticS, 2, getDeviceName(), "RETICULE_BRIGHTNESS", "Reticule +/-", MAIN_CONTROL_TAB, IP_RW,
                       ISR_ATMOST1, 60, IPS_IDLE);

    IUFillNumber(&ElevationLimitN[0], "minAlt", "Elev Min", "%g", -30, 30, 1, -30);
    IUFillNumber(&ElevationLimitN[1], "maxAlt", "Elev Max", "%g", 60, 90, 1, 89);
    IUFillNumberVector(&ElevationLimitNP, ElevationLimitN, 2, getDeviceName(), "Slew elevation Limit", "", MAIN_CONTROL_TAB,
                       IP_RW, 0, IPS_IDLE);

    IUFillText(&ObjectInfoT[0], "Info", "", "");
    IUFillTextVector(&ObjectInfoTP, ObjectInfoT, 1, getDeviceName(), "Object Info", "", MAIN_CONTROL_TAB,
                     IP_RO, 0, IPS_IDLE);

    // ============== COMMUNICATION_TAB

    // ============== CONNECTION_TAB

    // ============== OPTIONS_TAB

    // ============== FILTER_TAB

    // ============== MOTION_TAB
    //Override the standard slew rate command. Also add appropriate description. This also makes it work in Ekos Mount Control correctly
    //Note that SlewRateSP and MaxSlewRateNP BOTH track the rate. I have left them in there because MaxRateNP reports OnStep Values

    IUFillNumber(&MaxSlewRateN[0], "maxSlew", "Rate", "%f", 0.0, 9.0, 1.0, 5.0);    //2.0, 9.0, 1.0, 9.0
    IUFillNumberVector(&MaxSlewRateNP, MaxSlewRateN, 1, getDeviceName(), "Max slew Rate", "", MOTION_TAB,
                       IP_RW, 0, IPS_IDLE);

    IUFillSwitch(&TrackCompS[0], "1", "Full Compensation", ISS_OFF);
    IUFillSwitch(&TrackCompS[1], "2", "Refraction", ISS_OFF);
    IUFillSwitch(&TrackCompS[2], "3", "Off", ISS_ON);
    IUFillSwitchVector(&TrackCompSP, TrackCompS, 3, getDeviceName(), "Compensation", "Compensation Tracking", MOTION_TAB, IP_RW,
                       ISR_1OFMANY, 0, IPS_IDLE);

    IUFillSwitch(&TrackAxisS[0], "1", "Single Axis", ISS_OFF);
    IUFillSwitch(&TrackAxisS[1], "2", "Dual Axis", ISS_OFF);
    IUFillSwitchVector(&TrackAxisSP, TrackAxisS, 2, getDeviceName(), "Multi-Axis", "Multi-Axis Tracking", MOTION_TAB, IP_RW,
                       ISR_1OFMANY, 0, IPS_IDLE);

    IUFillSwitch(&TrackAxisS[0], "1", "Single Axis", ISS_OFF);
    IUFillSwitch(&TrackAxisS[1], "2", "Dual Axis", ISS_OFF);
    IUFillSwitchVector(&TrackAxisSP, TrackAxisS, 2, getDeviceName(), "Multi-Axis", "Multi-Axis Tracking", MOTION_TAB, IP_RW,
                       ISR_1OFMANY, 0, IPS_IDLE);

    IUFillNumber(&BacklashN[0], "Backlash DEC", "DE", "%g", 0, 3600, 1, 15);
    IUFillNumber(&BacklashN[1], "Backlash RA", "RA", "%g", 0, 3600, 1, 15);
    IUFillNumberVector(&BacklashNP, BacklashN, 2, getDeviceName(), "Backlash", "", MOTION_TAB, IP_RW, 0, IPS_IDLE);

    IUFillNumber(&GuideRateN[RA_AXIS], "GUIDE_RATE_WE", "W/E Rate", "%g", 0, 1, 0.25, 0.5);
    IUFillNumber(&GuideRateN[DEC_AXIS], "GUIDE_RATE_NS", "N/S Rate", "%g", 0, 1, 0.25, 0.5);
    IUFillNumberVector(&GuideRateNP, GuideRateN, 2, getDeviceName(), "GUIDE_RATE", "Guiding Rate", MOTION_TAB, IP_RO, 0,
                       IPS_IDLE);

    IUFillSwitch(&AutoFlipS[0], "1", "AutoFlip: OFF", ISS_OFF);
    IUFillSwitch(&AutoFlipS[1], "2", "AutoFlip: ON", ISS_OFF);
    IUFillSwitchVector(&AutoFlipSP, AutoFlipS, 2, getDeviceName(), "AutoFlip", "Meridian Auto Flip", MOTION_TAB, IP_RW,
                       ISR_1OFMANY, 0, IPS_IDLE);

    IUFillSwitch(&HomePauseS[0], "1", "HomePause: OFF", ISS_OFF);
    IUFillSwitch(&HomePauseS[1], "2", "HomePause: ON", ISS_OFF);
    IUFillSwitch(&HomePauseS[2], "3", "HomePause: Continue", ISS_OFF);
    IUFillSwitchVector(&HomePauseSP, HomePauseS, 3, getDeviceName(), "HomePause", "Pause at Home", MOTION_TAB, IP_RW,
                       ISR_1OFMANY, 0, IPS_IDLE);

    IUFillSwitch(&FrequencyAdjustS[0], "1", "Frequency -", ISS_OFF);
    IUFillSwitch(&FrequencyAdjustS[1], "2", "Frequency +", ISS_OFF);
    IUFillSwitch(&FrequencyAdjustS[2], "3", "Reset Sidereal Frequency", ISS_OFF);
    IUFillSwitchVector(&FrequencyAdjustSP, FrequencyAdjustS, 3, getDeviceName(), "FrequencyAdjust", "Frequency Adjust",
                       MOTION_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    IUFillSwitch(&PreferredPierSideS[0], "1", "West", ISS_OFF);
    IUFillSwitch(&PreferredPierSideS[1], "2", "East", ISS_OFF);
    IUFillSwitch(&PreferredPierSideS[2], "3", "Best", ISS_OFF);
    IUFillSwitchVector(&PreferredPierSideSP, PreferredPierSideS, 3, getDeviceName(), "Preferred Pier Side",
                       "Preferred Pier Side", MOTION_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    IUFillNumber(&minutesPastMeridianN[0], "East", "East  ± 180", "%g", -180, 180, 1, 20);
    IUFillNumber(&minutesPastMeridianN[1], "West", "West  ± 180", "%g", -180, 180, 1, -20);
    IUFillNumberVector(&minutesPastMeridianNP, minutesPastMeridianN, 2, getDeviceName(), "Minutes Past Meridian",
                       "Minutes Past Meridian", MOTION_TAB, IP_RW, 0, IPS_IDLE);


    // ============== DATETIME_TAB

    // ============== SITE_TAB
    // IUFillSwitch(&SetHomeS[0], "RETURN_HOME", "Return  Home", ISS_OFF);
    // IUFillSwitch(&SetHomeS[1], "AT_HOME", "At Home (Reset)", ISS_OFF);
    // IUFillSwitchVector(&SetHomeSP, SetHomeS, 2, getDeviceName(), "HOME_INIT", "Homing", SITE_TAB, IP_RW, ISR_ATMOST1, 60,
    //                    IPS_IDLE);

    // ============== GUIDE_TAB

    // ============== FOCUS_TAB
    // Focuser 1

    IUFillSwitch(&OSFocus1InitializeS[0], "Focus1_0", "Zero", ISS_OFF);
    IUFillSwitch(&OSFocus1InitializeS[1], "Focus1_2", "Mid", ISS_OFF);
    //     IUFillSwitch(&OSFocus1InitializeS[2], "Focus1_3", "max", ISS_OFF);
    IUFillSwitchVector(&OSFocus1InitializeSP, OSFocus1InitializeS, 2, getDeviceName(), "Foc1Rate", "Initialize", FOCUS_TAB,
                       IP_RW, ISR_ATMOST1, 0, IPS_IDLE);
    // Focus T° Compensation
    // Property must be FOCUS_TEMPERATURE to be recognized by Ekos
    IUFillNumber(&FocusTemperatureN[0], "FOCUS_TEMPERATURE", "TFC T°", "%+2.2f", 0, 1, 0.25,
                 25);  //default value is meaningless
    IUFillNumber(&FocusTemperatureN[1], "TFC Δ T°", "TFC Δ T°", "%+2.2f", 0, 1, 0.25, 25);  //default value is meaningless
    IUFillNumberVector(&FocusTemperatureNP, FocusTemperatureN, 2, getDeviceName(), "FOCUS_TEMPERATURE", "Focuser T°",
                       FOCUS_TAB, IP_RO, 0,
                       IPS_IDLE);
    IUFillSwitch(&TFCCompensationS[0], "Off", "Compensation: OFF", ISS_OFF);
    IUFillSwitch(&TFCCompensationS[1], "On", "Compensation: ON", ISS_OFF);
    IUFillSwitchVector(&TFCCompensationSP, TFCCompensationS, 2, getDeviceName(), "Compensation T°", "Temperature Compensation",
                       FOCUS_TAB, IP_RW,
                       ISR_1OFMANY, 0, IPS_IDLE);

    IUFillNumber(&TFCCoefficientN[0], "TFC Coefficient", "TFC Coefficient µm/°C", "%+03.5f", -999.99999, 999.99999, 1, 100);
    IUFillNumberVector(&TFCCoefficientNP, TFCCoefficientN, 1, getDeviceName(), "TFC Coefficient", "", FOCUS_TAB, IP_RW, 0,
                       IPS_IDLE);
    IUFillNumber(&TFCDeadbandN[0], "TFC Deadband", "TFC Deadband µm", "%g", 1, 32767, 1, 5);
    IUFillNumberVector(&TFCDeadbandNP, TFCDeadbandN, 1, getDeviceName(), "TFC Deadband", "", FOCUS_TAB, IP_RW, 0, IPS_IDLE);
    // End Focus T° Compensation

    IUFillSwitch(&OSFocusSelectS[0], "Focuser_Primary_1", "Focuser 1", ISS_ON);
    IUFillSwitch(&OSFocusSelectS[1], "Focuser_Primary_2", "Focuser 2/Swap", ISS_OFF);
    // For when OnStepX comes out
    IUFillSwitch(&OSFocusSelectS[2], "Focuser_Primary_3", "3", ISS_OFF);
    IUFillSwitch(&OSFocusSelectS[3], "Focuser_Primary_4", "4", ISS_OFF);
    IUFillSwitch(&OSFocusSelectS[4], "Focuser_Primary_5", "5", ISS_OFF);
    IUFillSwitch(&OSFocusSelectS[5], "Focuser_Primary_6", "6", ISS_OFF);
    IUFillSwitch(&OSFocusSelectS[6], "Focuser_Primary_7", "7", ISS_OFF);
    IUFillSwitch(&OSFocusSelectS[7], "Focuser_Primary_8", "8", ISS_OFF);
    IUFillSwitch(&OSFocusSelectS[8], "Focuser_Primary_9", "9", ISS_OFF);
    IUFillSwitch(&OSFocusSelectS[9], "Focuser_Primary_10", "10", ISS_OFF);

    IUFillSwitchVector(&OSFocusSelectSP, OSFocusSelectS, 1, getDeviceName(), "OSFocusSWAP", "Primary Focuser", FOCUS_TAB,
                       IP_RW, ISR_ATMOST1, 0, IPS_IDLE);


    // Focuser 2
    //IUFillSwitch(&OSFocus2SelS[0], "Focus2_Sel1", "Foc 1", ISS_OFF);
    //IUFillSwitch(&OSFocus2SelS[1], "Focus2_Sel2", "Foc 2", ISS_OFF);
    //IUFillSwitchVector(&OSFocus2SelSP, OSFocus2SelS, 2, getDeviceName(), "Foc2Sel", "Foc 2", FOCUS_TAB, IP_RW, ISR_ATMOST1, 0, IPS_IDLE);

    IUFillSwitch(&OSFocus2MotionS[0], "Focus2_In", "In", ISS_OFF);
    IUFillSwitch(&OSFocus2MotionS[1], "Focus2_Out", "Out", ISS_OFF);
    IUFillSwitch(&OSFocus2MotionS[2], "Focus2_Stop", "Stop", ISS_OFF);
    IUFillSwitchVector(&OSFocus2MotionSP, OSFocus2MotionS, 3, getDeviceName(), "Foc2Mot", "Foc 2 Motion", FOCUS_TAB, IP_RW,
                       ISR_ATMOST1, 0, IPS_IDLE);

    IUFillSwitch(&OSFocus2RateS[0], "Focus2_1", "min", ISS_OFF);
    IUFillSwitch(&OSFocus2RateS[1], "Focus2_2", "0.01", ISS_OFF);
    IUFillSwitch(&OSFocus2RateS[2], "Focus2_3", "0.1", ISS_OFF);
    IUFillSwitch(&OSFocus2RateS[3], "Focus2_4", "1", ISS_OFF);
    IUFillSwitchVector(&OSFocus2RateSP, OSFocus2RateS, 4, getDeviceName(), "Foc2Rate", "Foc 2 Rates", FOCUS_TAB, IP_RW,
                       ISR_ATMOST1, 0, IPS_IDLE);

    IUFillNumber(&OSFocus2TargN[0], "FocusTarget2", "Abs Pos", "%g", -25000, 25000, 1, 0);
    IUFillNumberVector(&OSFocus2TargNP, OSFocus2TargN, 1, getDeviceName(), "Foc2Targ", "Foc 2 Target", FOCUS_TAB, IP_RW, 0,
                       IPS_IDLE);

    // =========== ROTATOR TAB

    IUFillSwitch(&OSRotatorDerotateS[0], "Derotate_OFF", "OFF", ISS_OFF);
    IUFillSwitch(&OSRotatorDerotateS[1], "Derotate_ON", "ON", ISS_OFF);
    IUFillSwitchVector(&OSRotatorDerotateSP, OSRotatorDerotateS, 2, getDeviceName(), "Derotate_Status", "DEROTATE", ROTATOR_TAB,
                       IP_RW,
                       ISR_ATMOST1, 0, IPS_IDLE);

    // ============== FIRMWARE_TAB
    IUFillText(&VersionT[0], "Date", "", "");
    IUFillText(&VersionT[1], "Time", "", "");
    IUFillText(&VersionT[2], "Number", "", "");
    IUFillText(&VersionT[3], "Name", "", "");
    IUFillTextVector(&VersionTP, VersionT, 4, getDeviceName(), "Firmware Info", "", FIRMWARE_TAB, IP_RO, 0, IPS_IDLE);

    //PEC Tab
    IUFillSwitch(&OSPECStatusS[0], "OFF", "OFF", ISS_OFF);
    IUFillSwitch(&OSPECStatusS[1], "Playing", "Playing", ISS_OFF);
    IUFillSwitch(&OSPECStatusS[2], "Recording", "Recording", ISS_OFF);
    IUFillSwitch(&OSPECStatusS[3], "Will Play", "Will Play", ISS_OFF);
    IUFillSwitch(&OSPECStatusS[4], "Will Record", "Will Record", ISS_OFF);
    IUFillSwitchVector(&OSPECStatusSP, OSPECStatusS, 5, getDeviceName(), "PEC Status", "PEC Status", PEC_TAB, IP_RO,
                       ISR_ATMOST1, 0, IPS_IDLE);

    IUFillSwitch(&OSPECIndexS[0], "Not Detected", "Not Detected", ISS_ON);
    IUFillSwitch(&OSPECIndexS[1], "Detected", "Detected", ISS_OFF);
    IUFillSwitchVector(&OSPECIndexSP, OSPECIndexS, 2, getDeviceName(), "PEC Index Detect", "PEC Index", PEC_TAB, IP_RO,
                       ISR_ATMOST1, 0, IPS_IDLE);

    IUFillSwitch(&OSPECRecordS[0], "Clear", "Clear", ISS_OFF);
    IUFillSwitch(&OSPECRecordS[1], "Record", "Record", ISS_OFF);
    IUFillSwitch(&OSPECRecordS[2], "Write to EEPROM", "Write to EEPROM", ISS_OFF);
    IUFillSwitchVector(&OSPECRecordSP, OSPECRecordS, 3, getDeviceName(), "PEC Operations", "PEC Recording", PEC_TAB, IP_RW,
                       ISR_ATMOST1, 0, IPS_IDLE);

    IUFillSwitch(&OSPECReadS[0], "Read", "Read PEC to FILE****", ISS_OFF);
    IUFillSwitch(&OSPECReadS[1], "Write", "Write PEC from FILE***", ISS_OFF);
    //    IUFillSwitch(&OSPECReadS[2], "Write to EEPROM", "Write to EEPROM", ISS_OFF);
    IUFillSwitchVector(&OSPECReadSP, OSPECReadS, 2, getDeviceName(), "PEC File", "PEC File", PEC_TAB, IP_RW, ISR_ATMOST1, 0,
                       IPS_IDLE);
    // ============== ALIGNMENT_TAB
    // Only supports Alpha versions currently (July 2018) Now Beta (Dec 2018)
    IUFillSwitch(&OSNAlignStarsS[0], "1", "1 Star", ISS_OFF);
    IUFillSwitch(&OSNAlignStarsS[1], "2", "2 Stars", ISS_OFF);
    IUFillSwitch(&OSNAlignStarsS[2], "3", "3 Stars", ISS_ON);
    IUFillSwitch(&OSNAlignStarsS[3], "4", "4 Stars", ISS_OFF);
    IUFillSwitch(&OSNAlignStarsS[4], "5", "5 Stars", ISS_OFF);
    IUFillSwitch(&OSNAlignStarsS[5], "6", "6 Stars", ISS_OFF);
    IUFillSwitch(&OSNAlignStarsS[6], "7", "7 Stars", ISS_OFF);
    IUFillSwitch(&OSNAlignStarsS[7], "8", "8 Stars", ISS_OFF);
    IUFillSwitch(&OSNAlignStarsS[8], "9", "9 Stars", ISS_OFF);
    IUFillSwitchVector(&OSNAlignStarsSP, OSNAlignStarsS, 9, getDeviceName(), "AlignStars", "Select # of stars",
                       ALIGN_TAB, IP_RW, ISR_ATMOST1, 0, IPS_IDLE);

    IUFillSwitch(&OSNAlignS[0], "0", "Start Align", ISS_OFF);
    IUFillSwitch(&OSNAlignS[1], "1", "Issue Align", ISS_OFF);
    IUFillSwitchVector(&OSNAlignSP, OSNAlignS, 2, getDeviceName(), "NewAlignStar", "Align using up to 9 stars",
                       ALIGN_TAB, IP_RW, ISR_ATMOST1, 0, IPS_IDLE);

    IUFillSwitch(&OSNAlignWriteS[0], "0", "Write Align to NVRAM/Flash", ISS_OFF);
    IUFillSwitchVector(&OSNAlignWriteSP, OSNAlignWriteS, 1, getDeviceName(), "NewAlignStar2", "NVRAM", ALIGN_TAB, IP_RW,
                       ISR_ATMOST1, 0, IPS_IDLE);
    IUFillSwitch(&OSNAlignPolarRealignS[0], "0", "Instructions", ISS_OFF);
    IUFillSwitch(&OSNAlignPolarRealignS[1], "1", "Refine Polar Align (manually)", ISS_OFF);
    IUFillSwitchVector(&OSNAlignPolarRealignSP, OSNAlignPolarRealignS, 2, getDeviceName(), "AlignMP",
                       "Polar Correction, See info box", ALIGN_TAB, IP_RW, ISR_ATMOST1, 0, IPS_IDLE);

    IUFillText(&OSNAlignT[0], "0", "Align Process Status", "Align not started");
    IUFillText(&OSNAlignT[1], "1", "1. Manual Process", "Point towards the NCP");
    IUFillText(&OSNAlignT[2], "2", "2. Plate Solver Process", "Point towards the NCP");
    IUFillText(&OSNAlignT[3], "3", "Manual Action after 1", "Press 'Start Align'");
    IUFillText(&OSNAlignT[4], "4", "Current Status", "Not Updated");
    IUFillText(&OSNAlignT[5], "5", "Max Stars", "Not Updated");
    IUFillText(&OSNAlignT[6], "6", "Current Star", "Not Updated");
    IUFillText(&OSNAlignT[7], "7", "# of Align Stars", "Not Updated");
    IUFillTextVector(&OSNAlignTP, OSNAlignT, 8, getDeviceName(), "Align Process", "", ALIGN_TAB, IP_RO, 0, IPS_IDLE);

    IUFillText(&OSNAlignErrT[0], "0", "EQ Polar Error Alt", "Available once Aligned");
    IUFillText(&OSNAlignErrT[1], "1", "EQ Polar Error Az", "Available once Aligned");
    //     IUFillText(&OSNAlignErrT[2], "2", "2. Plate Solver Process", "Point towards the NCP");
    //     IUFillText(&OSNAlignErrT[3], "3", "After 1 or 2", "Press 'Start Align'");
    //     IUFillText(&OSNAlignErrT[4], "4", "Current Status", "Not Updated");
    //     IUFillText(&OSNAlignErrT[5], "5", "Max Stars", "Not Updated");
    //     IUFillText(&OSNAlignErrT[6], "6", "Current Star", "Not Updated");
    //     IUFillText(&OSNAlignErrT[7], "7", "# of Align Stars", "Not Updated");
    IUFillTextVector(&OSNAlignErrTP, OSNAlignErrT, 2, getDeviceName(), "Align OnStep results", "", ALIGN_TAB, IP_RO, 0,
                     IPS_IDLE);

    // =============== INFO_TAB

#ifdef ONSTEP_NOTDONE
    // =============== OUTPUT_TAB
    // ===============
    IUFillSwitch(&OSOutput1S[0], "0", "OFF", ISS_ON);
    IUFillSwitch(&OSOutput1S[1], "1", "ON", ISS_OFF);
    IUFillSwitchVector(&OSOutput1SP, OSOutput1S, 2, getDeviceName(), "Output 1", "Output 1", OUTPUT_TAB, IP_RW, ISR_ATMOST1, 60,
                       IPS_ALERT);

    IUFillSwitch(&OSOutput2S[0], "0", "OFF", ISS_ON);
    IUFillSwitch(&OSOutput2S[1], "1", "ON", ISS_OFF);
    IUFillSwitchVector(&OSOutput2SP, OSOutput2S, 2, getDeviceName(), "Output 2", "Output 2", OUTPUT_TAB, IP_RW, ISR_ATMOST1, 60,
                       IPS_ALERT);
#endif

    for(int i = 0; i < PORTS_COUNT; i++)
    {
        char port_name[30];
        snprintf(port_name, sizeof(port_name), "Output %d", i);
        IUFillNumber(&OutputPorts[i], port_name, port_name, "%g", 0, 255, 1, 0);
    }

    IUFillNumberVector(&OutputPorts_NP, OutputPorts, PORTS_COUNT, getDeviceName(), "Outputs", "Outputs",  OUTPUT_TAB, IP_WO, 60,
                       IPS_OK);


    // ============== STATUS_TAB
    IUFillText(&OnstepStat[0], ":GU# return", "", "");
    IUFillText(&OnstepStat[1], "Tracking", "", "");
    IUFillText(&OnstepStat[2], "Refractoring", "", "");
    IUFillText(&OnstepStat[3], "Park", "", "");
    IUFillText(&OnstepStat[4], "Pec", "", "");
    IUFillText(&OnstepStat[5], "TimeSync", "", "");
    IUFillText(&OnstepStat[6], "Mount Type", "", "");
    IUFillText(&OnstepStat[7], "Error", "", "");
    IUFillText(&OnstepStat[8], "Multi-Axis Tracking", "", "");
    IUFillText(&OnstepStat[9], "TMC Axis1", "", "");
    IUFillText(&OnstepStat[10], "TMC Axis2", "", "");
    IUFillTextVector(&OnstepStatTP, OnstepStat, 11, getDeviceName(), "OnStep Status", "", STATUS_TAB, IP_RO, 0, IPS_OK);

    // ============== WEATHER TAB
    // Uses OnStep's defaults for this
    IUFillNumber(&OSSetTemperatureN[0], "Set Temperature (C)", "C", "%4.2f", -100, 100, 1, 10);//-274, 999, 1, 10);
    IUFillNumberVector(&OSSetTemperatureNP, OSSetTemperatureN, 1, getDeviceName(), "Set Temperature (C)", "", ENVIRONMENT_TAB,
                       IP_RW, 0, IPS_IDLE);
    IUFillNumber(&OSSetHumidityN[0], "Set Relative Humidity (%)", "%", "%5.2f", 0, 100, 1, 70);
    IUFillNumberVector(&OSSetHumidityNP, OSSetHumidityN, 1, getDeviceName(), "Set Relative Humidity (%)", "", ENVIRONMENT_TAB,
                       IP_RW, 0, IPS_IDLE);
    IUFillNumber(&OSSetPressureN[0], "Set Pressure (hPa)", "hPa", "%4f", 500, 1500, 1, 1010);
    IUFillNumberVector(&OSSetPressureNP, OSSetPressureN, 1, getDeviceName(), "Set Pressure (hPa)", "", ENVIRONMENT_TAB, IP_RW,
                       0, IPS_IDLE);

    //Will eventually pull from the elevation in site settings
    //TODO: Pull from elevation in site settings
    IUFillNumber(&OSSetAltitudeN[0], "Set Altitude (m)", "m", "%4f", 0, 20000, 1, 110);
    IUFillNumberVector(&OSSetAltitudeNP, OSSetAltitudeN, 1, getDeviceName(), "Set Altitude (m)", "", ENVIRONMENT_TAB, IP_RW, 0,
                       IPS_IDLE);


    addParameter("WEATHER_TEMPERATURE", "Temperature (C)", -40, 85, 15);
    addParameter("WEATHER_HUMIDITY", "Humidity %", 0, 100, 15);
    addParameter("WEATHER_BAROMETER", "Pressure (hPa)", 0, 1500, 15);
    addParameter("WEATHER_DEWPOINT", "Dew Point (C)", 0, 100, 15); // From OnStep
    addParameter("WEATHER_CPU_TEMPERATURE", "OnStep CPU Temperature", -274, 200, -274); // From OnStep, -274 = unread
    setCriticalParameter("WEATHER_TEMPERATURE");

    addAuxControls();


    setDriverInterface(getDriverInterface() | FOCUSER_INTERFACE | WEATHER_INTERFACE);

    return true;
}

void LX200_OnStep::ISGetProperties(const char *dev)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) != 0) return;
    LX200Generic::ISGetProperties(dev);
}

bool LX200_OnStep::updateProperties()
{
    LX200Generic::updateProperties();
    //TODO: Properly setup Weather
    WI::updateProperties();

    if (isConnected())
    {
        Connection::Interface *activeConnection = getActiveConnection();
        if (!activeConnection->name().compare("CONNECTION_TCP"))
        {
            LOG_INFO("Network based connection, detection timeouts set to 2 seconds");
            OSTimeoutMicroSeconds = 0;
            OSTimeoutSeconds = 2;
        }
        else
        {
            LOG_INFO("Non-Network based connection, detection timeouts set to 0.1 seconds");
            OSTimeoutMicroSeconds = 100000;
            OSTimeoutSeconds = 0;
        }

        // Firstinitialize some variables
        // keep sorted by TABs is easier
        // Main Control
        defineProperty(&ReticSP);
        defineProperty(&ElevationLimitNP);
        defineProperty(&ObjectInfoTP);
        // Connection

        // Options

        // OnStep Status
        defineProperty(&OnstepStatTP);

        // Motion Control
        defineProperty(SlewRateSP);    // was missing
        defineProperty(&MaxSlewRateNP);
        defineProperty(&TrackCompSP);
        defineProperty(&TrackAxisSP);
        defineProperty(&BacklashNP);
        defineProperty(&GuideRateNP);
        defineProperty(&AutoFlipSP);
        defineProperty(&HomePauseSP);
        defineProperty(&FrequencyAdjustSP);
        defineProperty(&PreferredPierSideSP);
        defineProperty(&minutesPastMeridianNP);

        // Site Management
        defineProperty(ParkOptionSP);
        //defineProperty(&SetHomeSP);

        // Guide

        // Focuser

        // Focuser 1
        OSNumFocusers = 0; //Reset before detection
        //if (!sendOnStepCommand(":FA#"))  // do we have a Focuser 1
        char response[RB_MAX_LEN] = {0};
        int error_or_fail = getCommandSingleCharResponse(PortFD, response, ":FA#"); //0 = failure, 1 = success, no # on reply
        if (error_or_fail > 0 && response[0] == '1')
        {
            LOG_INFO("Focuser 1 found");
            OSFocuser1 = true;
            defineProperty(&OSFocus1InitializeSP);
            // Focus T° Compensation
            defineProperty(&FocusTemperatureNP);
            defineProperty(&TFCCompensationSP);
            defineProperty(&TFCCoefficientNP);
            defineProperty(&TFCDeadbandNP);
            // End Focus T° Compensation
            OSNumFocusers = 1;
        }
        else
        {
            OSFocuser1 = false;
            LOG_INFO("Focuser 1 NOT found");
            LOGF_DEBUG("error_or_fail = %u, response = %c", error_or_fail, response[0]);
        }
        // Focuser 2
        if (!sendOnStepCommand(":fA#"))  // Do we have a Focuser 2 (:fA# will only work for OnStep, not OnStepX)
        {
            LOG_INFO("Focuser 2 found");
            OSFocuser2 = true;
            OSNumFocusers = 2;
            //defineProperty(&OSFocus2SelSP);
            defineProperty(&OSFocus2MotionSP);
            defineProperty(&OSFocus2RateSP);
            defineProperty(&OSFocus2TargNP);
            IUFillSwitchVector(&OSFocusSelectSP, OSFocusSelectS, OSNumFocusers, getDeviceName(), "OSFocusSWAP", "Primary Focuser",
                               FOCUS_TAB,
                               IP_RW, ISR_ATMOST1, 0, IPS_IDLE);
            defineProperty(&OSFocusSelectSP); //Swap focusers (only matters if two focusers)
        }
        else     //For OnStepX, up to 6 focusers
        {
            LOG_INFO("Focuser 2 NOT found");
            OSFocuser2 = false;
            if (OnStepMountVersion == OSV_UNKNOWN || OnStepMountVersion == OSV_OnStepX)
            {
                LOG_INFO("Version unknown or OnStepX (Checking for OnStepX Focusers)");
                for (int i = 0; i < 9; i++)
                {
                    char cmd[CMD_MAX_LEN] = {0};
                    char read_buffer[RB_MAX_LEN] = {0};
                    snprintf(cmd, sizeof(cmd), ":F%dA#", i + 1);
                    int fail_or_error = getCommandSingleCharResponse(PortFD, read_buffer,
                                        cmd); //0 = failure, 1 = success, 0 on all prior to OnStepX no # on reply
                    if (!fail_or_error && read_buffer[0] == '1')  // Do we have a Focuser X
                    {
                        LOGF_INFO("Focuser %i Found", i);
                        OSNumFocusers = i + 1;
                    }
                    else
                    {
                        if(fail_or_error < 0)
                        {
                            //Non detection = 0, Read errors < 0, stop
                            LOGF_INFO("Function call failed in a way that says OnStep doesn't have this setup, stopping Focuser probing, return: %i",
                                      fail_or_error);
                            break;
                        }
                    }
                }
            }
            if (OSNumFocusers > 1)
            {
                IUFillSwitchVector(&OSFocusSelectSP, OSFocusSelectS, OSNumFocusers, getDeviceName(), "OSFocusSWAP", "Primary Focuser",
                                   FOCUS_TAB,
                                   IP_RW, ISR_ATMOST1, 0, IPS_IDLE);
                defineProperty(&OSFocusSelectSP);
            }
        }
        if (OSNumFocusers == 0)
        {
            LOG_INFO("No Focusers found");
        }
        else
        {
            LOG_INFO("At least one focuser found, showing interface");
            FI::updateProperties();
        }

        LOG_DEBUG("Focusers checked Variables:");
        LOGF_DEBUG("OSFocuser1: %d, OSFocuser2: %d, OSNumFocusers: %i", OSFocuser1, OSFocuser2, OSNumFocusers);

        //Rotation Information
        char rotator_response[RB_MAX_LEN] = {0};
        error_or_fail = getCommandSingleCharResponse(PortFD, rotator_response, ":GX98#");
        if (error_or_fail > 0)
        {
            if (rotator_response[0] == 'D' || rotator_response[0] == 'R')
            {
                LOG_INFO("Rotator found.");
                OSRotator1 = true;
                setDriverInterface(getDriverInterface() | ROTATOR_INTERFACE);
                syncDriverInfo();
                RI::updateProperties();
            }
            if (rotator_response[0] == 'D')
            {
                defineProperty(&OSRotatorDerotateSP);
            }
            if (rotator_response[0] == '0')
            {
                OSRotator1 = false;
            }
        }
        else
        {
            LOGF_WARN("Error: %i", error_or_fail);
            LOG_WARN("Error on response to rotator check (:GX98#) CHECK CONNECTION");
        }
        //=================

        if (OSRotator1 == false)
        {
            LOG_INFO("No Rotator found.");
            OSRotator1 = false;
        }

        // Firmware Data
        defineProperty(&VersionTP);

        //PEC
        //TODO: Define later when it might be supported
        defineProperty(&OSPECStatusSP);
        defineProperty(&OSPECIndexSP);
        defineProperty(&OSPECRecordSP);
        defineProperty(&OSPECReadSP);
        //defineProperty(&OSPECCurrentIndexNP);
        //defineProperty(&OSPECRWValuesNP);

        //New Align
        defineProperty(&OSNAlignStarsSP);
        defineProperty(&OSNAlignSP);
        defineProperty(&OSNAlignWriteSP);
        defineProperty(&OSNAlignTP);
        defineProperty(&OSNAlignErrTP);
        defineProperty(&OSNAlignPolarRealignSP);

#ifdef ONSTEP_NOTDONE
        //Outputs
        defineProperty(&OSOutput1SP);
        defineProperty(&OSOutput2SP);
#endif
        // OSHasOutputs = true; //Set to true on new connection, Init_Outputs will set to false if appropriate
        Init_Outputs();

        //Weather
        defineProperty(&OSSetTemperatureNP);
        defineProperty(&OSSetPressureNP);
        defineProperty(&OSSetHumidityNP);
        defineProperty(&OSSetAltitudeNP);



        if (InitPark())
        {
            // If loading parking data is successful, we just set the default parking values.
            SetAxis1ParkDefault(LocationNP[LOCATION_LATITUDE].getValue() >= 0 ? 0 : 180);
            SetAxis2ParkDefault(LocationNP[LOCATION_LATITUDE].getValue());
        }
        else
        {
            // Otherwise, we set all parking data to default in case no parking data is found.
            SetAxis1Park(LocationNP[LOCATION_LATITUDE].getValue() >= 0 ? 0 : 180);
            SetAxis1ParkDefault(LocationNP[LOCATION_LATITUDE].getValue());

            SetAxis1ParkDefault(LocationNP[LOCATION_LATITUDE].getValue() >= 0 ? 0 : 180);
            SetAxis2ParkDefault(LocationNP[LOCATION_LATITUDE].getValue());
        }

        double longitude = -1000, latitude = -1000;
        // Get value from config file if it exists.
        IUGetConfigNumber(getDeviceName(), "GEOGRAPHIC_COORD", "LONG", &longitude);
        IUGetConfigNumber(getDeviceName(), "GEOGRAPHIC_COORD", "LAT", &latitude);
        //         if (longitude != -1000 && latitude != -1000)
        //         {
        //             updateLocation(latitude, longitude, 0);
        //         }
        //NOTE: if updateProperties is called it clobbers this, so added here


    }
    else
    {
        // keep sorted by TABs is easier
        // Main Control
        deleteProperty(ReticSP.name);
        deleteProperty(ElevationLimitNP.name);
        // Connection

        // Options

        // Motion Control
        deleteProperty(SlewRateSP);    // was missing
        deleteProperty(MaxSlewRateNP.name);
        deleteProperty(TrackCompSP.name);
        deleteProperty(TrackAxisSP.name);
        deleteProperty(BacklashNP.name);
        deleteProperty(GuideRateNP.name);
        deleteProperty(AutoFlipSP.name);
        deleteProperty(HomePauseSP.name);
        deleteProperty(FrequencyAdjustSP.name);
        deleteProperty(PreferredPierSideSP.name);
        deleteProperty(minutesPastMeridianNP.name);

        // Site Management
        deleteProperty(ParkOptionSP);
        //deleteProperty(SetHomeSP.name);
        // Guide

        // Focuser
        // Focuser 1
        deleteProperty(FocusTemperatureNP.name);
        deleteProperty(OSFocus1InitializeSP.name);
        deleteProperty(TFCCoefficientNP.name);
        deleteProperty(TFCDeadbandNP.name);
        // Focus T° Compensation
        deleteProperty(TFCCompensationSP.name);
        // End Focus T° Compensation

        // Focuser 2
        //deleteProperty(OSFocus2SelSP.name);
        deleteProperty(OSFocus2MotionSP.name);
        deleteProperty(OSFocus2RateSP.name);
        deleteProperty(OSFocus2TargNP.name);
        deleteProperty(OSFocusSelectSP.name);

        // Rotator
        deleteProperty(OSRotatorDerotateSP.name);

        // Firmware Data
        deleteProperty(VersionTP.name);


        //PEC
        deleteProperty(OSPECStatusSP.name);
        deleteProperty(OSPECIndexSP.name);
        deleteProperty(OSPECRecordSP.name);
        deleteProperty(OSPECReadSP.name);
        //deleteProperty(OSPECCurrentIndexNP.name);
        //deleteProperty(OSPECRWValuesNP.name);

        //New Align
        deleteProperty(OSNAlignStarsSP.name);
        deleteProperty(OSNAlignSP.name);
        deleteProperty(OSNAlignWriteSP.name);
        deleteProperty(OSNAlignTP.name);
        deleteProperty(OSNAlignErrTP.name);
        deleteProperty(OSNAlignPolarRealignSP.name);

#ifdef ONSTEP_NOTDONE
        //Outputs
        deleteProperty(OSOutput1SP.name);
        deleteProperty(OSOutput2SP.name);
#endif

        deleteProperty(OutputPorts_NP.name);

        // OnStep Status
        deleteProperty(OnstepStatTP.name);
        //Weather
        deleteProperty(OSSetTemperatureNP.name);
        deleteProperty(OSSetPressureNP.name);
        deleteProperty(OSSetHumidityNP.name);
        deleteProperty(OSSetAltitudeNP.name);
        FI::updateProperties();
        RI::updateProperties();
        OSHasOutputs = true; //Set once per connection, either at startup or on disconnection for next connection;
    }
    LOG_INFO("Initialization Complete");
    return true;
}

bool LX200_OnStep::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        if (strstr(name, "FOCUS_"))
            return FI::processNumber(dev, name, values, names, n);
        if (strstr(name, "ROTATOR_"))
            return RI::processNumber(dev, name, values, names, n);

        if (EqNP.isNameMatch(name))
            //Replace this from inditelescope so it doesn't change state
            //Most of this needs to be handled by our updates, or it breaks things
        {
            //  this is for us, and it is a goto
            bool rc    = false;
            double ra  = -1;
            double dec = -100;

            for (int x = 0; x < n; x++)
            {
                if (EqNP[AXIS_RA].isNameMatch(names[x]))
                    ra = values[x];
                else if (EqNP[AXIS_DE].isNameMatch(names[x]))
                    dec = values[x];
            }

            if ((ra >= 0) && (ra <= 24) && (dec >= -90) && (dec <= 90))
            {
                // Check if it is already parked.
                if (CanPark())
                {
                    if (isParked())
                    {
                        LOG_DEBUG("Please unpark the mount before issuing any motion/sync commands.");
                        //                         EqNP.s = lastEqState = IPS_IDLE;
                        //                         IDSetNumber(&EqNP, nullptr);
                        return false;
                    }
                }

                // Check if it can sync
                if (Telescope::CanSync())
                {
                    auto oneSwitch = CoordSP.findWidgetByName("SYNC");
                    if (oneSwitch && oneSwitch->getState() == ISS_ON)
                    {
                        return Sync(ra, dec);
                    }
                }

                // Issue GOTO
                rc = Goto(ra, dec);
                if (rc)
                {
                    //  Now fill in target co-ords, so domes can start turning
                    TargetNP[AXIS_RA].setValue(ra);
                    TargetNP[AXIS_DE].setValue(dec);
                    TargetNP.apply();
                }
            }
            return rc;
        }

        if (!strcmp(name, ObjectNoNP.name))
        {
            char object_name[256];

            if (selectCatalogObject(PortFD, currentCatalog, (int)values[0]) < 0)
            {
                ObjectNoNP.s = IPS_ALERT;
                IDSetNumber(&ObjectNoNP, "Failed to select catalog object.");
                return false;
            }

            getLX200RA(PortFD, &targetRA);
            getLX200DEC(PortFD, &targetDEC);

            ObjectNoNP.s = IPS_OK;
            IDSetNumber(&ObjectNoNP, "Object updated.");

            if (getObjectInfo(PortFD, object_name) < 0)
                IDMessage(getDeviceName(), "Getting object info failed.");
            else
            {
                IUSaveText(&ObjectInfoTP.tp[0], object_name);
                IDSetText(&ObjectInfoTP, nullptr);
            }
            Goto(targetRA, targetDEC);
            return true;
        }

        if (!strcmp(name, MaxSlewRateNP.name))
        {
            int ret;
            char cmd[CMD_MAX_LEN] = {0};
            snprintf(cmd, 5, ":R%d#", (int)values[0]);
            ret = sendOnStepCommandBlind(cmd);

            //if (setMaxSlewRate(PortFD, (int)values[0]) < 0) //(int) MaxSlewRateN[0].value
            if (ret == -1)
            {
                LOGF_DEBUG("Pas OK Return value =%d", ret);
                LOGF_DEBUG("Setting Max Slew Rate to %f\n", values[0]);
                MaxSlewRateNP.s = IPS_ALERT;
                IDSetNumber(&MaxSlewRateNP, "Setting Max Slew Rate Failed");
                return false;
            }
            LOGF_DEBUG("OK Return value =%d", ret);
            MaxSlewRateNP.s           = IPS_OK;
            MaxSlewRateNP.np[0].value = values[0];
            IDSetNumber(&MaxSlewRateNP, "Slewrate set to %04.1f", values[0]);
            SlewRateSP.reset();
            SlewRateSP[int(values[0])].setState(ISS_ON);
            SlewRateSP.setState(IPS_OK);
            SlewRateSP.apply();
            return true;
        }

        if (!strcmp(name, BacklashNP.name))
        {
            //char cmd[CMD_MAX_LEN] = {0};
            int i, nset;
            double bklshdec = 0, bklshra = 0;

            for (nset = i = 0; i < n; i++)
            {
                INumber *bktp = IUFindNumber(&BacklashNP, names[i]);
                if (bktp == &BacklashN[0])
                {
                    bklshdec = values[i];
                    LOGF_DEBUG("===CMD==> Backlash DEC= %f", bklshdec);
                    nset += bklshdec >= 0 && bklshdec <= 999;  //range 0 to 999
                }
                else if (bktp == &BacklashN[1])
                {
                    bklshra = values[i];
                    LOGF_DEBUG("===CMD==> Backlash RA= %f", bklshra);
                    nset += bklshra >= 0 && bklshra <= 999;   //range 0 to 999
                }
            }
            if (nset == 2)
            {
                char cmd[CMD_MAX_LEN] = {0};
                snprintf(cmd, 9, ":$BD%d#", (int)bklshdec);
                if (sendOnStepCommand(cmd))
                {
                    BacklashNP.s = IPS_ALERT;
                    IDSetNumber(&BacklashNP, "Error Backlash DEC limit.");
                }
                const struct timespec timeout = {0, 100000000L};
                nanosleep(&timeout, nullptr); // time for OnStep to respond to previous cmd
                snprintf(cmd, 9, ":$BR%d#", (int)bklshra);
                if (sendOnStepCommand(cmd))
                {
                    BacklashNP.s = IPS_ALERT;
                    IDSetNumber(&BacklashNP, "Error Backlash RA limit.");
                }

                BacklashNP.np[0].value = bklshdec;
                BacklashNP.np[1].value = bklshra;
                BacklashNP.s           = IPS_OK;
                IDSetNumber(&BacklashNP, nullptr);
                return true;
            }
            else
            {
                BacklashNP.s = IPS_ALERT;
                IDSetNumber(&BacklashNP, "Backlash invalid.");
                return false;
            }
        }

        if (!strcmp(name, ElevationLimitNP.name))
        {
            // new elevation limits
            double minAlt = 0, maxAlt = 0;
            int i, nset;

            for (nset = i = 0; i < n; i++)
            {
                INumber *altp = IUFindNumber(&ElevationLimitNP, names[i]);
                if (altp == &ElevationLimitN[0])
                {
                    minAlt = values[i];
                    nset += minAlt >= -30.0 && minAlt <= 30.0;  //range -30 to 30
                }
                else if (altp == &ElevationLimitN[1])
                {
                    maxAlt = values[i];
                    nset += maxAlt >= 60.0 && maxAlt <= 90.0;   //range 60 to 90
                }
            }
            if (nset == 2)
            {
                if (setMinElevationLimit(PortFD, (int)maxAlt) < 0)
                {
                    ElevationLimitNP.s = IPS_ALERT;
                    IDSetNumber(&ElevationLimitNP, "Error setting min elevation limit.");
                }

                if (setMaxElevationLimit(PortFD, (int)minAlt) < 0)
                {
                    ElevationLimitNP.s = IPS_ALERT;
                    IDSetNumber(&ElevationLimitNP, "Error setting max elevation limit.");
                    return false;
                }
                ElevationLimitNP.np[0].value = minAlt;
                ElevationLimitNP.np[1].value = maxAlt;
                ElevationLimitNP.s           = IPS_OK;
                IDSetNumber(&ElevationLimitNP, nullptr);
                return true;
            }
            else
            {
                ElevationLimitNP.s = IPS_IDLE;
                IDSetNumber(&ElevationLimitNP, "elevation limit missing or invalid.");
                return false;
            }
        }
    }

    if (!strcmp(name, minutesPastMeridianNP.name))
    {
        //char cmd[CMD_MAX_LEN] ={0};
        int i, nset;
        double minPMEast = 0, minPMWest = 0;

        for (nset = i = 0; i < n; i++)
        {
            INumber *bktp = IUFindNumber(&minutesPastMeridianNP, names[i]);
            if (bktp == &minutesPastMeridianN[0])
            {
                minPMEast = values[i];
                LOGF_DEBUG("===CMD==> minutesPastMeridianN[0]/East = %f", minPMEast);
                nset += minPMEast >= -180 && minPMEast <= 180;  //range -180 to 180
            }
            else if (bktp == &minutesPastMeridianN[1])
            {
                minPMWest = values[i];
                LOGF_DEBUG("===CMD==> minutesPastMeridianN[1]/West= %f", minPMWest);
                nset += minPMWest >= -180 && minPMWest <= 180;   //range -180 to 180
            }
        }
        if (nset == 2)
        {
            char cmd[CMD_MAX_LEN] = {0};
            snprintf(cmd, 20, ":SXE9,%d#", (int) minPMEast);
            if (sendOnStepCommand(cmd))
            {
                minutesPastMeridianNP.s = IPS_ALERT;
                IDSetNumber(&minutesPastMeridianNP, "Error minutesPastMeridian East.");
            }
            const struct timespec timeout = {0, 100000000L};
            nanosleep(&timeout, nullptr); // time for OnStep to respond to previous cmd
            snprintf(cmd, 20, ":SXEA,%d#", (int) minPMWest);
            if (sendOnStepCommand(cmd))
            {
                minutesPastMeridianNP.s = IPS_ALERT;
                IDSetNumber(&minutesPastMeridianNP, "Error minutesPastMeridian West.");
            }

            minutesPastMeridianNP.np[0].value = minPMEast;
            minutesPastMeridianNP.np[1].value = minPMWest;
            minutesPastMeridianNP.s           = IPS_OK;
            IDSetNumber(&minutesPastMeridianNP, nullptr);
            return true;
        }
        else
        {
            minutesPastMeridianNP.s = IPS_ALERT;
            IDSetNumber(&minutesPastMeridianNP, "minutesPastMeridian invalid.");
            return false;
        }
    }
    // Focuser
    // Focuser 1 Now handled by Focusr Interface

    // Focuser 2 Target
    if (!strcmp(name, OSFocus2TargNP.name))
    {
        char cmd[CMD_MAX_LEN] = {0};

        if ((values[0] >= -25000) && (values[0] <= 25000))
        {
            snprintf(cmd, 15, ":fR%d#", (int)values[0]);
            sendOnStepCommandBlind(cmd);
            OSFocus2TargNP.s           = IPS_OK;
            IDSetNumber(&OSFocus2TargNP, "Focuser 2 position (relative) moved by %d", (int)values[0]);
            OSUpdateFocuser();
        }
        else
        {
            OSFocus2TargNP.s = IPS_ALERT;
            IDSetNumber(&OSFocus2TargNP, "Setting Max Slew Rate Failed");
        }
        return true;
    }

    if (!strcmp(name, OutputPorts_NP.name))
    {
        //go through all output values and see if any value needs to be changed
        for(int i = 0; i < n; i++)
        {
            int value = (int)values[i];
            if(OutputPorts_NP.np[i].value != value)
            {
                int ret;
                char cmd[CMD_MAX_LEN] = {0};
                int port = STARTING_PORT + i;

                //This is for newer version of OnStep:
                snprintf(cmd, sizeof(cmd), ":SXX%d,V%d#", port, value);
                //This is for older version of OnStep:
                //snprintf(cmd, sizeof(cmd), ":SXG%d,%d#", port, value);
                ret = sendOnStepCommandBlind(cmd);

                if (ret == -1)
                {
                    LOGF_ERROR("Set port %d to value =%d failed", port, value);
                    OutputPorts_NP.s = IPS_ALERT;
                    return false;
                }

                OutputPorts_NP.s           = IPS_OK;
                OutputPorts_NP.np[i].value = value;
                IDSetNumber(&OutputPorts_NP, "Set port %d to value =%d", port, value);
            }
        }
        return true;
    }
    //Weather not handled by Weather Interface

    if (!strcmp(name, OSSetTemperatureNP.name))
    {
        //         char cmd[CMD_MAX_LEN] = {0};

        if ((values[0] >= -100) && (values[0] <= 100))
        {
            char cmd[CMD_MAX_LEN] = {0};
            snprintf(cmd, 15, ":SX9A,%d#", (int)values[0]);
            sendOnStepCommandBlind(cmd);
            OSSetTemperatureNP.s           = IPS_OK;
            OSSetTemperatureN[0].value = values[0];
            IDSetNumber(&OSSetTemperatureNP, "Temperature set to %d", (int)values[0]);
        }
        else
        {
            OSSetTemperatureNP.s = IPS_ALERT;
            IDSetNumber(&OSSetTemperatureNP, "Setting Temperature Failed");
        }
        return true;
    }

    if (!strcmp(name, OSSetHumidityNP.name))
    {
        char cmd[CMD_MAX_LEN] = {0};

        if ((values[0] >= 0) && (values[0] <= 100))
        {
            snprintf(cmd, 15, ":SX9C,%d#", (int)values[0]);
            sendOnStepCommandBlind(cmd);
            OSSetHumidityNP.s           = IPS_OK;
            OSSetHumidityN[0].value = values[0];
            IDSetNumber(&OSSetHumidityNP, "Humidity set to %d", (int)values[0]);
        }
        else
        {
            OSSetHumidityNP.s = IPS_ALERT;
            IDSetNumber(&OSSetHumidityNP, "Setting Humidity Failed");
        }
        return true;
    }

    if (!strcmp(name, OSSetPressureNP.name))
    {
        char cmd[CMD_MAX_LEN] = {0};

        if ((values[0] >= 500) && (values[0] <= 1100)) // typo
        {
            snprintf(cmd, 15, ":SX9B,%d#", (int)values[0]);
            sendOnStepCommandBlind(cmd);
            OSSetPressureNP.s           = IPS_OK;
            OSSetPressureN[0].value = values[0];
            IDSetNumber(&OSSetPressureNP, "Pressure set to %d", (int)values[0]);
        }
        else
        {
            OSSetPressureNP.s = IPS_ALERT;
            IDSetNumber(&OSSetPressureNP, "Setting Pressure Failed");
        }
        return true;
    }

    // Focus T° Compensation
    if (!strcmp(name, TFCCoefficientNP.name))
    {
        // :FC[sn.n]# Set focuser temperature compensation coefficient in µ/°C
        char cmd[CMD_MAX_LEN] = {0};

        if (abs(values[0]) < 1000)    //Range is -999.999 .. + 999.999
        {
            snprintf(cmd, 15, ":FC%+3.5f#", values[0]);
            sendOnStepCommandBlind(cmd);
            TFCCoefficientNP.s           = IPS_OK;
            IDSetNumber(&TFCCoefficientNP, "TFC Coefficient set to %+3.5f", values[0]);
        }
        else
        {
            TFCCoefficientNP.s = IPS_ALERT;
            IDSetNumber(&TFCCoefficientNP, "Setting TFC Coefficient Failed");
        }
        return true;
    }

    if (!strcmp(name, TFCDeadbandNP.name))
    {
        // :FD[n]#    Set focuser temperature compensation deadband amount (in steps or microns)
        char cmd[CMD_MAX_LEN] = {0};

        if ((values[0] >= 1) && (values[0] <= 32768))   //Range is 1 .. 32767
        {
            snprintf(cmd, 15, ":FD%d#", (int)values[0]);
            sendOnStepCommandBlind(cmd);
            TFCDeadbandNP.s = IPS_OK;
            IDSetNumber(&TFCDeadbandNP, "TFC Deadbandset to %d", (int)values[0]);
        }
        else
        {
            TFCDeadbandNP.s = IPS_ALERT;
            IDSetNumber(&TFCDeadbandNP, "Setting TFC Deadband Failed");
        }
        return true;
    }



    // end Focus T° Compensation

    if (strstr(name, "WEATHER_"))
    {
        return WI::processNumber(dev, name, values, names, n);
    }

    return LX200Generic::ISNewNumber(dev, name, values, names, n);
}

bool LX200_OnStep::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    int index = 0;

    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        //Intercept Before inditelescope base can set TrackState
        //Next one modification of inditelescope.cpp function
        if (TrackStateSP.isNameMatch(name))
        {
            //             int previousState = IUFindOnSwitchIndex(&TrackStateSP);
            TrackStateSP.update(states, names, n);
            int targetState = TrackStateSP.findOnSwitchIndex();
            //             LOG_DEBUG("OnStep driver TrackStateSP override called");
            //             if (previousState == targetState)
            //             {
            //                 IDSetSwitch(&TrackStateSP, nullptr);
            //                 return true;
            //             }

            if (TrackState == SCOPE_PARKED)
            {
                LOG_WARN("Telescope is Parked, Unpark before tracking.");
                return false;
            }

            bool rc = SetTrackEnabled((targetState == TRACK_ON) ? true : false);

            if (rc)
            {
                return true;
                //TrackStateSP moved to Update
            }
            else
            {
                //This is the case for an error on sending the command, so change TrackStateSP
                TrackStateSP.setState(IPS_ALERT);
                TrackStateSP.reset();
                return false;
            }

            LOG_DEBUG("TrackStateSP intercept, OnStep driver, should never get here");
            return false;
        }



        // Reticlue +/- Buttons
        if (!strcmp(name, ReticSP.name))
        {
            int ret = 0;
            IUUpdateSwitch(&ReticSP, states, names, n);
            ReticSP.s = IPS_OK;

            if (ReticS[0].s == ISS_ON)
            {
                ret = increaseReticleBrightness(PortFD);    //in lx200driver
                ReticS[0].s = ISS_OFF;
                IDSetSwitch(&ReticSP, "Bright");
            }
            else
            {
                ret = decreaseReticleBrightness(PortFD);    //in lx200driver
                ReticS[1].s = ISS_OFF;
                IDSetSwitch(&ReticSP, "Dark");
            }

            INDI_UNUSED(ret);
            IUResetSwitch(&ReticSP);
            IDSetSwitch(&ReticSP, nullptr);
            return true;
        }
        //Move to more standard controls
        if (SlewRateSP.isNameMatch(name))
        {
            SlewRateSP.update(states, names, n);
            int ret;
            char cmd[CMD_MAX_LEN] = {0};
            int index = SlewRateSP.findOnSwitchIndex() ;//-1; //-1 because index is 1-10, OS Values are 0-9
            snprintf(cmd, sizeof(cmd), ":R%d#", index);
            ret = sendOnStepCommandBlind(cmd);

            //if (setMaxSlewRate(PortFD, (int)values[0]) < 0) //(int) MaxSlewRateN[0].value
            if (ret == -1)
            {
                LOGF_DEBUG("Pas OK Return value =%d", ret);
                LOGF_DEBUG("Setting Max Slew Rate to %u\n", index);
                SlewRateSP.setState(IPS_ALERT);
                LOG_ERROR("Setting Max Slew Rate Failed");
                SlewRateSP.apply();
                return false;
            }
            LOGF_INFO("Setting Max Slew Rate to %u (%s) \n", index, SlewRateSP[index].getLabel());
            LOGF_DEBUG("OK Return value =%d", ret);
            MaxSlewRateNP.s           = IPS_OK;
            MaxSlewRateNP.np[0].value = index;
            IDSetNumber(&MaxSlewRateNP, "Slewrate set to %d", index);
            SlewRateSP.reset();
            SlewRateSP[index].setState(ISS_ON);
            SlewRateSP.setState(IPS_OK);
            SlewRateSP.apply();
            return true;
        }

        // Tracking Compensation selection
        if (!strcmp(name, TrackCompSP.name))
        {
            IUUpdateSwitch(&TrackCompSP, states, names, n);
            TrackCompSP.s = IPS_BUSY;

            if (TrackCompS[0].s == ISS_ON)
            {
                if (!sendOnStepCommand(":To#"))
                {
                    IDSetSwitch(&TrackCompSP, "Full Compensated Tracking On");
                    TrackCompSP.s = IPS_OK;
                    IDSetSwitch(&TrackCompSP, nullptr);
                    return true;
                }
            }
            if (TrackCompS[1].s == ISS_ON)
            {
                if (!sendOnStepCommand(":Tr#"))
                {
                    IDSetSwitch(&TrackCompSP, "Refraction Tracking On");
                    TrackCompSP.s = IPS_OK;
                    IDSetSwitch(&TrackCompSP, nullptr);
                    return true;
                }
            }
            if (TrackCompS[2].s == ISS_ON)
            {
                if (!sendOnStepCommand(":Tn#"))
                {
                    IDSetSwitch(&TrackCompSP, "Refraction Tracking Disabled");
                    TrackCompSP.s = IPS_OK;
                    IDSetSwitch(&TrackCompSP, nullptr);
                    return true;
                }
            }
            IUResetSwitch(&TrackCompSP);
            TrackCompSP.s = IPS_IDLE;
            IDSetSwitch(&TrackCompSP, nullptr);
            return true;
        }

        if (!strcmp(name, TrackAxisSP.name))
        {
            IUUpdateSwitch(&TrackAxisSP, states, names, n);
            TrackAxisSP.s = IPS_BUSY;

            if (TrackAxisS[0].s == ISS_ON)
            {
                if (!sendOnStepCommand(":T1#"))
                {
                    IDSetSwitch(&TrackAxisSP, "Single Tracking On");
                    TrackAxisSP.s = IPS_OK;
                    IDSetSwitch(&TrackAxisSP, nullptr);
                    return true;
                }
            }
            if (TrackAxisS[1].s == ISS_ON)
            {
                if (!sendOnStepCommand(":T2#"))
                {
                    IDSetSwitch(&TrackAxisSP, "Dual Axis Tracking On");
                    TrackAxisSP.s = IPS_OK;
                    IDSetSwitch(&TrackAxisSP, nullptr);
                    return true;
                }
            }
            IUResetSwitch(&TrackAxisSP);
            TrackAxisSP.s = IPS_IDLE;
            IDSetSwitch(&TrackAxisSP, nullptr);
            return true;
        }

        if (!strcmp(name, AutoFlipSP.name))
        {
            IUUpdateSwitch(&AutoFlipSP, states, names, n);
            AutoFlipSP.s = IPS_BUSY;

            if (AutoFlipS[0].s == ISS_ON)
            {
                if (sendOnStepCommand(":SX95,0#"))
                {
                    AutoFlipSP.s = IPS_OK;
                    IDSetSwitch(&AutoFlipSP, "Auto Meridian Flip OFF");
                    return true;
                }
            }
            if (AutoFlipS[1].s == ISS_ON)
            {
                if (sendOnStepCommand(":SX95,1#"))
                {
                    AutoFlipSP.s = IPS_OK;
                    IDSetSwitch(&AutoFlipSP, "Auto Meridian Flip ON");
                    return true;
                }
            }
            IUResetSwitch(&AutoFlipSP);
            //AutoFlipSP.s = IPS_IDLE;
            IDSetSwitch(&AutoFlipSP, nullptr);
            return true;
        }

        if (!strcmp(name, HomePauseSP.name))
        {
            IUUpdateSwitch(&HomePauseSP, states, names, n);
            HomePauseSP.s = IPS_BUSY;

            if (HomePauseS[0].s == ISS_ON)
            {
                if (sendOnStepCommand(":SX98,0#"))
                {
                    HomePauseSP.s = IPS_OK;
                    IDSetSwitch(&HomePauseSP, "Home Pause OFF");
                    return true;
                }
            }
            if (HomePauseS[1].s == ISS_ON)
            {
                if (sendOnStepCommand(":SX98,1#"))
                {
                    HomePauseSP.s = IPS_OK;
                    IDSetSwitch(&HomePauseSP, "Home Pause ON");
                    return true;
                }
            }
            if (HomePauseS[2].s == ISS_ON)
            {
                if (sendOnStepCommand(":SX99,1#"))
                {
                    IUResetSwitch(&HomePauseSP);
                    HomePauseSP.s = IPS_OK;
                    IDSetSwitch(&HomePauseSP, "Home Pause: Continue");
                    return true;
                }
            }
            IUResetSwitch(&HomePauseSP);
            HomePauseSP.s = IPS_IDLE;
            IDSetSwitch(&HomePauseSP, nullptr);
            return true;
        }

        if (!strcmp(name, FrequencyAdjustSP.name))      //

            //
        {
            IUUpdateSwitch(&FrequencyAdjustSP, states, names, n);
            FrequencyAdjustSP.s = IPS_OK;

            if (FrequencyAdjustS[0].s == ISS_ON)
            {
                if (!sendOnStepCommandBlind(":T-#"))
                {
                    IDSetSwitch(&FrequencyAdjustSP, "Frequency decreased");
                    return true;
                }
            }
            if (FrequencyAdjustS[1].s == ISS_ON)
            {
                if (!sendOnStepCommandBlind(":T+#"))
                {
                    IDSetSwitch(&FrequencyAdjustSP, "Frequency increased");
                    return true;
                }
            }
            if (FrequencyAdjustS[2].s == ISS_ON)
            {
                if (!sendOnStepCommandBlind(":TR#"))
                {
                    IDSetSwitch(&FrequencyAdjustSP, "Frequency Reset (TO saved EEPROM)");
                    return true;
                }
            }
            IUResetSwitch(&FrequencyAdjustSP);
            FrequencyAdjustSP.s = IPS_IDLE;
            IDSetSwitch(&FrequencyAdjustSP, nullptr);
            return true;
        }

        //Pier Side
        if (!strcmp(name, PreferredPierSideSP.name))
        {
            IUUpdateSwitch(&PreferredPierSideSP, states, names, n);
            PreferredPierSideSP.s = IPS_BUSY;

            if (PreferredPierSideS[0].s == ISS_ON) //West
            {
                if (sendOnStepCommand(":SX96,W#"))
                {
                    PreferredPierSideSP.s = IPS_OK;
                    IDSetSwitch(&PreferredPierSideSP, "Preferred Pier Side: West");
                    return true;
                }
            }
            if (PreferredPierSideS[1].s == ISS_ON) //East
            {
                if (sendOnStepCommand(":SX96,E#"))
                {
                    PreferredPierSideSP.s = IPS_OK;
                    IDSetSwitch(&PreferredPierSideSP, "Preferred Pier Side: East");
                    return true;
                }
            }
            if (PreferredPierSideS[2].s == ISS_ON) //Best
            {
                if (sendOnStepCommand(":SX96,B#"))
                {
                    PreferredPierSideSP.s = IPS_OK;
                    IDSetSwitch(&PreferredPierSideSP, "Preferred Pier Side: Best");
                    return true;
                }
            }
            IUResetSwitch(&PreferredPierSideSP);
            IDSetSwitch(&PreferredPierSideSP, nullptr);
            return true;
        }


        // Focuser
        // Focuser 1 Rates
        if (!strcmp(name, OSFocus1InitializeSP.name))
        {
            char cmd[CMD_MAX_LEN] = {0};
            if (IUUpdateSwitch(&OSFocus1InitializeSP, states, names, n) < 0)
                return false;
            index = IUFindOnSwitchIndex(&OSFocus1InitializeSP);
            if (index == 0)
            {
                snprintf(cmd, 5, ":FZ#");
                sendOnStepCommandBlind(cmd);
                OSFocus1InitializeS[index].s = ISS_OFF;
                OSFocus1InitializeSP.s = IPS_OK;
                IDSetSwitch(&OSFocus1InitializeSP, nullptr);
            }
            if (index == 1)
            {
                snprintf(cmd, 5, ":FH#");
                sendOnStepCommandBlind(cmd);
                OSFocus1InitializeS[index].s = ISS_OFF;
                OSFocus1InitializeSP.s = IPS_OK;
                IDSetSwitch(&OSFocus1InitializeSP, nullptr);
            }
        }


        //Focuser Swap/Select
        if (!strcmp(name, OSFocusSelectSP.name))
        {
            char cmd[CMD_MAX_LEN] = {0};
            int i;
            if (IUUpdateSwitch(&OSFocusSelectSP, states, names, n) < 0)
                return false;
            index = IUFindOnSwitchIndex(&OSFocusSelectSP);
            LOGF_INFO("Primary focuser set: Focuser 1 in INDI/Controllable Focuser = OnStep Focuser %d", index + 1);
            if (index == 0 && OSNumFocusers <= 2)
            {
                LOG_INFO("If using OnStep: Focuser 2 in INDI = OnStep Focuser 2");
            }
            if (index == 1 && OSNumFocusers <= 2)
            {
                LOG_INFO("If using OnStep: Focuser 2 in INDI = OnStep Focuser 1");
            }
            if (OSNumFocusers > 2)
            {
                LOGF_INFO("If using OnStepX, There is no swap, and current max number: %d", OSNumFocusers);
            }
            snprintf(cmd, 7, ":FA%d#", index + 1 );
            for (i = 0; i < 9; i++)
            {
                OSFocusSelectS[i].s = ISS_OFF;
            }
            OSFocusSelectS[index].s = ISS_ON;
            if (!sendOnStepCommand(cmd))
            {
                OSFocusSelectSP.s = IPS_BUSY;
            }
            else
            {
                OSFocusSelectSP.s = IPS_ALERT;
            }
            IDSetSwitch(&OSFocusSelectSP, nullptr);
        }


        // Focuser 2 Rates
        if (!strcmp(name, OSFocus2RateSP.name))
        {
            char cmd[CMD_MAX_LEN] = {0};

            if (IUUpdateSwitch(&OSFocus2RateSP, states, names, n) < 0)
                return false;

            index = IUFindOnSwitchIndex(&OSFocus2RateSP);
            snprintf(cmd, 5, ":F%d#", index + 1);
            sendOnStepCommandBlind(cmd);
            OSFocus2RateS[index].s = ISS_OFF;
            OSFocus2RateSP.s = IPS_OK;
            IDSetSwitch(&OSFocus2RateSP, nullptr);
        }
        // Focuser 2 Motion
        if (!strcmp(name, OSFocus2MotionSP.name))
        {
            char cmd[CMD_MAX_LEN] = {0};

            if (IUUpdateSwitch(&OSFocus2MotionSP, states, names, n) < 0)
                return false;

            index = IUFindOnSwitchIndex(&OSFocus2MotionSP);
            if (index == 0)
            {
                strncpy(cmd, ":f+#", sizeof(cmd));
            }
            if (index == 1)
            {
                strncpy(cmd, ":f-#", sizeof(cmd));
            }
            if (index == 2)
            {
                strncpy(cmd, ":fQ#", sizeof(cmd));
            }
            sendOnStepCommandBlind(cmd);
            const struct timespec timeout = {0, 100000000L};
            nanosleep(&timeout, nullptr); // Pulse 0,1 s
            if(index != 2)
            {
                sendOnStepCommandBlind(":fQ#");
            }
            OSFocus2MotionS[index].s = ISS_OFF;
            OSFocus2MotionSP.s = IPS_OK;
            IDSetSwitch(&OSFocus2MotionSP, nullptr);
        }

        //Rotator De-rotation
        //         OSRotatorDerotateS
        if (!strcmp(name, OSRotatorDerotateSP.name))
        {
            char cmd[CMD_MAX_LEN] = {0};

            if (IUUpdateSwitch(&OSRotatorDerotateSP, states, names, n) < 0)
                return false;

            index = IUFindOnSwitchIndex(&OSRotatorDerotateSP);
            if (index == 0) //Derotate_OFF
            {
                strncpy(cmd, ":r-#", sizeof(cmd));
            }
            if (index == 1) //Derotate_ON
            {
                strncpy(cmd, ":r+#", sizeof(cmd));
            }
            sendOnStepCommandBlind(cmd);
            OSRotatorDerotateS[index].s = ISS_OFF;
            OSRotatorDerotateSP.s = IPS_IDLE;
            IDSetSwitch(&OSRotatorDerotateSP, nullptr);
        }

        // PEC
        if (!strcmp(name, OSPECRecordSP.name))
        {
            IUUpdateSwitch(&OSPECRecordSP, states, names, n);
            OSPECRecordSP.s = IPS_OK;

            if (OSPECRecordS[0].s == ISS_ON)
            {
                OSPECEnabled = true;
                ClearPECBuffer(0);
                OSPECRecordS[0].s = ISS_OFF;
            }
            if (OSPECRecordS[1].s == ISS_ON)
            {
                OSPECEnabled = true;
                StartPECRecord(0);
                OSPECRecordS[1].s = ISS_OFF;
            }
            if (OSPECRecordS[2].s == ISS_ON)
            {
                OSPECEnabled = true;
                SavePECBuffer(0);
                OSPECRecordS[2].s = ISS_OFF;
            }
            IDSetSwitch(&OSPECRecordSP, nullptr);
        }
        if (!strcmp(name, OSPECReadSP.name))
        {
            if (OSPECReadS[0].s == ISS_ON)
            {
                OSPECEnabled = true;
                ReadPECBuffer(0);
                OSPECReadS[0].s = ISS_OFF;
            }
            if (OSPECReadS[1].s == ISS_ON)
            {
                OSPECEnabled = true;
                WritePECBuffer(0);
                OSPECReadS[1].s = ISS_OFF;
            }
            IDSetSwitch(&OSPECReadSP, nullptr);
        }
        if (PECStateSP.isNameMatch(name))
        {
            index = PECStateSP.findOnSwitchIndex();
            if (index == 0)
            {
                OSPECEnabled = true;
                StopPECPlayback(0); //Status will set OSPECEnabled to false if that's set by the controller
                PECStateSP[PEC_OFF].setState(ISS_ON);
                PECStateSP[PEC_ON].setState(ISS_OFF);
                PECStateSP.apply();
            }
            else if (index == 1)
            {
                OSPECEnabled = true;
                StartPECPlayback(0);
                PECStateSP[PEC_OFF].setState(ISS_OFF);
                PECStateSP[PEC_ON].setState(ISS_ON);
                PECStateSP.apply();
            }

        }

        // Align Buttons
        if (!strcmp(name, OSNAlignStarsSP.name))
        {
            IUResetSwitch(&OSNAlignStarsSP);
            IUUpdateSwitch(&OSNAlignStarsSP, states, names, n);
            index = IUFindOnSwitchIndex(&OSNAlignStarsSP);

            return true;
        }

        // Alignment
        if (!strcmp(name, OSNAlignSP.name))
        {
            if (IUUpdateSwitch(&OSNAlignSP, states, names, n) < 0)
                return false;

            index = IUFindOnSwitchIndex(&OSNAlignSP);
            //NewGeometricAlignment
            //End NewGeometricAlignment
            OSNAlignSP.s = IPS_BUSY;
            if (index == 0)
            {

                /* Index is 0-8 and represents index+1*/

                int index_stars = IUFindOnSwitchIndex(&OSNAlignStarsSP);
                if ((index_stars <= 8) && (index_stars >= 0))
                {
                    int stars = index_stars + 1;
                    OSNAlignS[0].s = ISS_OFF;
                    LOGF_INFO("Align index: %d, stars: %d", index_stars, stars);
                    AlignStartGeometric(stars);
                }
            }
            if (index == 1)
            {
                OSNAlignS[1].s = ISS_OFF;
                OSNAlignSP.s = AlignAddStar();
            }
            //Write to EEPROM moved to new line/variable
            IDSetSwitch(&OSNAlignSP, nullptr);
            UpdateAlignStatus();
        }

        if (!strcmp(name, OSNAlignWriteSP.name))
        {
            if (IUUpdateSwitch(&OSNAlignWriteSP, states, names, n) < 0)
                return false;

            index = IUFindOnSwitchIndex(&OSNAlignWriteSP);

            OSNAlignWriteSP.s = IPS_BUSY;
            if (index == 0)
            {
                OSNAlignWriteS[0].s = ISS_OFF;
                OSNAlignWriteSP.s = AlignWrite();
            }
            IDSetSwitch(&OSNAlignWriteSP, nullptr);
            UpdateAlignStatus();
        }

        if (!strcmp(name, OSNAlignPolarRealignSP.name))
        {
            char cmd[CMD_MAX_LEN] = {0};
            char response[RB_MAX_LEN];
            if (IUUpdateSwitch(&OSNAlignPolarRealignSP, states, names, n) < 0)
                return false;


            OSNAlignPolarRealignSP.s = IPS_BUSY;
            if (OSNAlignPolarRealignS[0].s == ISS_ON) //INFO
            {
                OSNAlignPolarRealignS[0].s = ISS_OFF;
                LOG_INFO("Step 1: Goto a bright star between 50 and 80 degrees N/S from the pole. Preferably on the Meridian.");
                LOG_INFO("Step 2: Make sure it is centered.");
                LOG_INFO("Step 3: Press Refine Polar Alignment.");
                LOG_INFO("Step 4: Using the mount's Alt and Az screws manually recenter the star. (Video mode if your camera supports it will be helpful.)");
                LOG_INFO("Optional: Start a new alignment.");
                IDSetSwitch(&OSNAlignPolarRealignSP, nullptr);
                UpdateAlignStatus();
                return true;
            }
            if (OSNAlignPolarRealignS[1].s == ISS_ON)
            {
                OSNAlignPolarRealignS[1].s = ISS_OFF;
                // int returncode=sendOnStepCommand("
                snprintf(cmd, 5, ":MP#");
                //  Returns:
                //  0=goto is possible
                //  1=below the horizon limit
                //  2=above overhead limit
                //  3=controller in standby
                //  4=mount is parked
                //  5=goto in progress
                //  6=outside limits
                //  7=hardware fault
                //  8=already in motion
                //  9=unspecified error

                int res = getCommandSingleCharResponse(PortFD, response, cmd); //0 = 0 Success 1..9 failure, no # on reply
                if(res > 0 && response[0] == '0')
                {
                    LOG_INFO("Command for Refine Polar Alignment Successful");
                    UpdateAlignStatus();
                    OSNAlignPolarRealignSP.s = IPS_OK;
                    IDSetSwitch(&OSNAlignPolarRealignSP, nullptr);
                    return true;
                }
                else
                {
                    LOGF_ERROR("Command for Refine Polar Alignment Failed, error=%s", response[0]);
                    UpdateAlignStatus();
                    OSNAlignPolarRealignSP.s = IPS_ALERT;
                    IDSetSwitch(&OSNAlignPolarRealignSP, nullptr);
                    return false;
                }
            }
        }

        // Focus T° Compensation
        if (!strcmp(name, TFCCompensationSP.name))
        {
            // :Fc[n]#    Enable/disable focuser temperature compensation where [n] = 0 or 1
            //            Return: 0 on failure
            //                    1 on success
            char cmd[CMD_MAX_LEN] = {0};
            int ret = 0;
            IUUpdateSwitch(&TFCCompensationSP, states, names, n);
            TFCCompensationSP.s = IPS_OK;

            if (TFCCompensationS[0].s == ISS_ON)
            {
                snprintf(cmd, sizeof(cmd), ":Fc0#");
                ret = sendOnStepCommandBlind(cmd);
                //TFCCompensationS[0].s = ISS_OFF;
                IDSetSwitch(&TFCCompensationSP, "Idle");
            }
            else
            {
                snprintf(cmd, sizeof(cmd), ":Fc1#");
                ret = sendOnStepCommandBlind(cmd);
                //TFCCompensationS[1].s = ISS_OFF;
                IDSetSwitch(&TFCCompensationSP, "Idle");
            }

            INDI_UNUSED(ret);
            IUResetSwitch(&TFCCompensationSP);
            IDSetSwitch(&TFCCompensationSP, nullptr);
            return true;
        }
        //End  Focus T° Compensation

#ifdef ONSTEP_NOTDONE
        if (!strcmp(name, OSOutput1SP.name))      //
        {
            if (OSOutput1S[0].s == ISS_ON)
            {
                OSDisableOutput(1);
                //PECStateS[0].s == ISS_OFF;
            }
            else if (OSOutput1S[1].s == ISS_ON)
            {
                OSEnableOutput(1);
                //PECStateS[1].s == ISS_OFF;
            }
            IDSetSwitch(&OSOutput1SP, nullptr);
        }
        if (!strcmp(name, OSOutput2SP.name))      //
        {
            if (OSOutput2S[0].s == ISS_ON)
            {
                OSDisableOutput(2);
                //PECStateS[0].s == ISS_OFF;
            }
            else if (OSOutput2S[1].s == ISS_ON)
            {
                OSEnableOutput(2);
                //PECStateS[1].s == ISS_OFF;
            }
            IDSetSwitch(&OSOutput2SP, nullptr);
        }
#endif

        // Focuser
        if (strstr(name, "FOCUS"))
        {
            return FI::processSwitch(dev, name, states, names, n);
        }
        // Focuser
        if (strstr(name, "ROTATOR"))
        {
            return RI::processSwitch(dev, name, states, names, n);
        }
    }

    return LX200Generic::ISNewSwitch(dev, name, states, names, n);
}

void LX200_OnStep::getBasicData()
{
    // process parent
    LX200Generic::getBasicData();

    if (!isSimulation())
    {
        char buffer[128] = {0};
        getVersionDate(PortFD, buffer);
        IUSaveText(&VersionT[0], buffer);
        getVersionTime(PortFD, buffer);
        IUSaveText(&VersionT[1], buffer);
        getVersionNumber(PortFD, buffer);
        IUSaveText(&VersionT[2], buffer);
        getProductName(PortFD, buffer);
        IUSaveText(&VersionT[3], buffer);

        IDSetText(&VersionTP, nullptr);
        if ((VersionT[2].text[0] == '1' || VersionT[2].text[0] == '2') && (VersionT[2].text[1] == '.' )
                && (strcmp(VersionT[3].text, "OnStep") || strcmp(VersionT[3].text, "On-Step")))
        {
            LOG_INFO("Old OnStep (V1/V2 depreciated) detected, setting some defaults");
            LOG_INFO("Note: Everything should work, but it may have timeouts in places, as it's not tested against.");
            OSHighPrecision = false;
            OnStepMountVersion = OSV_OnStepV1or2;
        }
        else if (VersionT[2].text[0] == '3' && (strcmp(VersionT[3].text, "OnStep") || strcmp(VersionT[3].text, "On-Step")))
        {
            LOG_INFO("V3 OnStep detected, setting some defaults");
            OSHighPrecision = false;
            OnStepMountVersion = OSV_OnStepV3;
        }
        else if (VersionT[2].text[0] == '4' && (strcmp(VersionT[3].text, "OnStep") || strcmp(VersionT[3].text, "On-Step")))
        {
            LOG_INFO("V4 OnStep detected, setting some defaults");
            OSHighPrecision = true;
            OnStepMountVersion = OSV_OnStepV4;
        }
        else if (VersionT[2].text[0] == '5' && (strcmp(VersionT[3].text, "OnStep") || strcmp(VersionT[3].text, "On-Step")))
        {
            LOG_INFO("V5 OnStep detected, setting some defaults");
            OSHighPrecision = true;
            OnStepMountVersion = OSV_OnStepV5;
        }
        else if (VersionT[2].text[0] == '1' && VersionT[2].text[1] == '0' && VersionT[2].text[2] == '.'
                 && (strcmp(VersionT[3].text, "OnStepX") || strcmp(VersionT[3].text, "On-Step")))
        {
            LOG_INFO("OnStepX detected, setting some defaults");
            OSHighPrecision = true;
            OnStepMountVersion = OSV_OnStepX;
        }
        else
        {
            LOG_INFO("OnStep/OnStepX version could not be detected");
            OSHighPrecision = false;
            OnStepMountVersion = OSV_UNKNOWN;
        }

        if (InitPark())
        {
            // If loading parking data is successful, we just set the default parking values.
            LOG_INFO("=============== Parkdata loaded");
        }
        else
        {
            // Otherwise, we set all parking data to default in case no parking data is found.
            LOG_INFO("=============== Parkdata Load Failed");
        }
    }
}

//======================== Parking =======================
bool LX200_OnStep::SetCurrentPark()
{
    char response[RB_MAX_LEN];
    // 0 = failure, 1 = success
    int error_or_fail = getCommandSingleCharResponse(PortFD, response, ":hQ#");
    if(error_or_fail != 1 || response[0] != '1')
    {
        LOGF_WARN("===CMD==> Set Park Pos %s", response);
        return false;
    }
    SetAxis1Park(currentRA);
    SetAxis2Park(currentDEC);
    LOG_WARN("Park Value set to current position");
    return true;
}

bool LX200_OnStep::SetDefaultPark()
{
    IDMessage(getDeviceName(), "Setting Park Data to Default.");
    SetAxis1Park(20);
    SetAxis2Park(80);
    LOG_WARN("Park Position set to Default value, 20/80");
    return true;
}

bool LX200_OnStep::UnPark()
{
    char response[RB_MAX_LEN];


    if (!isSimulation())
    {
        int failure_or_error = getCommandSingleCharResponse(PortFD, response, ":hR#"); //0 = failure, 1 = success, no # on reply
        if ((response[0] != '1') || (failure_or_error < 0))
        {
            return false;
        }
    }
    return true;
}

bool LX200_OnStep::Park()
{
    if (!isSimulation())
    {
        // If scope is moving, let's stop it first.
        if (EqNP.getState() == IPS_BUSY)
        {
            if (!isSimulation() && abortSlew(PortFD) < 0)
            {
                Telescope::AbortSP.setState(IPS_ALERT);
                LOG_ERROR("Abort slew failed.");
                Telescope::AbortSP.apply();
                return false;
            }
            Telescope::AbortSP.setState(IPS_OK);
            EqNP.setState(IPS_IDLE);
            LOG_ERROR("Slew aborted.");
            Telescope::AbortSP.apply();
            EqNP.apply();

            if (MovementNSSP.getState() == IPS_BUSY || MovementWESP.getState() == IPS_BUSY)
            {
                MovementNSSP.setState(IPS_IDLE);
                MovementWESP.setState(IPS_IDLE);
                EqNP.setState(IPS_IDLE);
                MovementNSSP.reset();
                MovementWESP.reset();

                MovementNSSP.apply();
                MovementWESP.apply();
            }
        }
        if (!isSimulation() && slewToPark(PortFD) < 0)
        {
            ParkSP.setState(IPS_ALERT);
            LOG_ERROR("Parking Failed.");
            ParkSP.apply();
            return false;
        }
    }
    ParkSP.setState(IPS_BUSY);
    return true;
}

// Periodically Polls OnStep Parameter from controller
bool LX200_OnStep::ReadScopeStatus()
{
    char OSbacklashDEC[RB_MAX_LEN] = {0};
    char OSbacklashRA[RB_MAX_LEN] = {0};
    char GuideValue[RB_MAX_LEN] = {0};
    //    int i;
    bool pier_not_set = true; // Avoid a call to :Gm if :GU it
    Errors Lasterror = ERR_NONE;


    if (isSimulation()) //if Simulation is selected
    {
        mountSim();
        return true;
    }

    tcflush(PortFD, TCIOFLUSH);
    flushIO(PortFD);


#ifdef OnStep_Alpha
    OSSupports_bitfield_Gu = try_bitfield_Gu();

    if (!OSSupports_bitfield_Gu)
    {
        //Fall back to :GU parsing
#endif

        int error_or_fail = getCommandSingleCharErrorOrLongResponse(PortFD, OSStat,
                            ":GU#"); // :GU# returns a string containing controller status
        if (error_or_fail > 1) // check if successful read (strcmp(OSStat, OldOSStat) != 0) //if status changed
        {
            //If this fails, simply return;
            char check_GU_valid1[RB_MAX_LEN] = {0};
            char check_GU_valid2[RB_MAX_LEN] = {0};
            char check_GU_valid3[RB_MAX_LEN] = {0};
            //:GU should always have one of pIPF and 3 numbers
            if (sscanf(OSStat, "%s%[pIPF]%s%[0-9]%[0-9]%[0-9]", check_GU_valid1, &check_GU_valid2[0], check_GU_valid3,
                       &check_GU_valid2[1], &check_GU_valid2[2], &check_GU_valid2[3]) != 1)
            {
                LOG_WARN(":GU# returned something that can not be right, this update aborted, will try again...");
                LOGF_DEBUG("Parameters matched: %u from %s", sscanf(OSStat, "%s%[pIPF]%s%[0-9]%[0-9]%[0-9]", check_GU_valid1,
                           &check_GU_valid2[0], check_GU_valid3, &check_GU_valid2[1], &check_GU_valid2[2], &check_GU_valid2[3]), OSStat);
                flushIO(PortFD);
                return true; //COMMUNICATION ERROR, BUT DON'T PUT TELESCOPE IN ERROR STATE
            }
            if (getLX200RA(PortFD, &currentRA) < 0 || getLX200DEC(PortFD, &currentDEC) < 0) // Update actual position
            {
                EqNP.setState(IPS_ALERT);
                LOG_ERROR("Error reading RA/DEC.");
                EqNP.apply();
                LOG_INFO("RA/DEC could not be read, possible solution if using (wireless) ethernet: Use port 9998");
                LOG_WARN("This update aborted, will try again...");
                return true; //COMMUNICATION ERROR, BUT DON'T PUT TELESCOPE IN ERROR STATE
            }
            strncpy(OldOSStat, OSStat, sizeof(OldOSStat));

            IUSaveText(&OnstepStat[0], OSStat);

            // ============= Parkstatus

#ifdef DEBUG_TRACKSTATE
            LOG_DEBUG("Prior TrackState:");
            PrintTrackState();
            LOG_DEBUG("^ Prior");
#endif
            //TelescopeStatus PriorTrackState = TrackState;
            // not [p]arked, parking [I]n-progress, [P]arked, Park [F]ailed
            // "P" (Parked moved to Telescope Status, since it would override any other TrackState
            // Other than parked, none of these affect TrackState
            if (strstr(OSStat, "F"))
            {
                IUSaveText(&OnstepStat[3], "Parking Failed");
            }
            if (strstr(OSStat, "I"))
            {
                IUSaveText(&OnstepStat[3], "Park in Progress");
            }
            if (strstr(OSStat, "p"))
            {
                IUSaveText(&OnstepStat[3], "UnParked");
            }
            // ============= End Parkstatus




            // ============= Telescope Status

            if (strstr(OSStat, "P"))
            {
                TrackState = SCOPE_PARKED;
                IUSaveText(&OnstepStat[3], "Parked");
                IUSaveText(&OnstepStat[1], "Parked");
                if (!isParked())   //Don't call this every time OSStat changes
                {
                    SetParked(true);
                }
                PrintTrackState();
            }
            else
            {
                if (strstr(OSStat, "n") && strstr(OSStat, "N"))
                {
                    IUSaveText(&OnstepStat[1], "Idle");
                    TrackState = SCOPE_IDLE;
                }
                if (strstr(OSStat, "n") && !strstr(OSStat, "N"))
                {
                    if (strstr(OSStat, "I"))
                    {
                        IUSaveText(&OnstepStat[1], "Parking/Slewing");
                        TrackState = SCOPE_PARKING;
                    }
                    else
                    {
                        IUSaveText(&OnstepStat[1], "Slewing");
                        TrackState = SCOPE_SLEWING;
                    }
                }
                if (strstr(OSStat, "N") && !strstr(OSStat, "n"))
                {
                    IUSaveText(&OnstepStat[1], "Tracking");
                    TrackState = SCOPE_TRACKING;
                }
                if (!strstr(OSStat, "N") && !strstr(OSStat, "n"))
                {
                    IUSaveText(&OnstepStat[1], "Slewing");
                    TrackState = SCOPE_SLEWING;
                }
                PrintTrackState();
                if (isParked())   //IMPORTANT: SET AFTER setting TrackState!
                {
                    SetParked(false);
                }
                PrintTrackState();
            }
            // Set TrackStateSP based on above, but only change if needed.
            // NOTE: Technically during a slew it can have tracking on, but elsewhere there is the assumption:
            //      Slewing = Not tracking
#ifdef DEBUG_TRACKSTATE
            LOG_DEBUG("BEFORE UPDATE");
            if (EqNP.s == IPS_BUSY)
            {
                LOG_DEBUG("EqNP is IPS_BUSY (Goto/slew or Parking)");
            }
            if (EqNP.s == IPS_OK)
            {
                LOG_DEBUG("EqNP is IPS_OK (Tracking)");
            }
            if (EqNP.s == IPS_IDLE)
            {
                LOG_DEBUG("EqNP is IPS_IDLE (Not Tracking or Parked)");
            }
            if (EqNP.s == IPS_ALERT)
            {
                LOG_DEBUG("EqNP is IPS_ALERT (Something wrong)");
            }
            LOG_DEBUG("/BEFORE UPDATE");
#endif
            // Fewer updates might help with KStars handling.
            bool trackStateUpdateNeded = false;
            if (TrackState == SCOPE_TRACKING)
            {
                if (TrackStateSP.getState() != IPS_BUSY)
                {
                    TrackStateSP.setState(IPS_BUSY);
                    trackStateUpdateNeded = true;
                }
                if (TrackStateSP[TRACK_ON].getState() != ISS_ON || TrackStateSP[TRACK_OFF].getState() != ISS_OFF)
                {
                    TrackStateSP[TRACK_ON].setState(ISS_ON);
                    TrackStateSP[TRACK_OFF].setState(ISS_OFF);
                    trackStateUpdateNeded = true;
                }
            }
            else
            {
                if (TrackStateSP.getState() != IPS_IDLE)
                {
                    TrackStateSP.setState(IPS_IDLE);
                    trackStateUpdateNeded = true;
                }
                if (TrackStateSP[TRACK_ON].getState() != ISS_OFF || TrackStateSP[TRACK_OFF].getState() != ISS_ON)
                {
                    TrackStateSP[TRACK_ON].setState(ISS_OFF);
                    TrackStateSP[TRACK_OFF].setState(ISS_ON);
                    trackStateUpdateNeded = true;
                }
            }
            if (trackStateUpdateNeded)
            {
#ifdef DEBUG_TRACKSTATE
                LOG_DEBUG("TRACKSTATE CHANGED");
#endif
                TrackStateSP.apply();
            }
            else
            {
#ifdef DEBUG_TRACKSTATE
                LOG_DEBUG("TRACKSTATE UNCHANGED");
#endif
            }
            //TrackState should be set correctly, only update EqNP if actually needed.
            bool update_needed = false;
            switch (TrackState)
            {
                case SCOPE_PARKED:
                case SCOPE_IDLE:
                    if (EqNP.getState() != IPS_IDLE)
                    {
                        EqNP.setState(IPS_IDLE);
                        update_needed = true;
#ifdef DEBUG_TRACKSTATE
                        LOG_DEBUG("EqNP set to IPS_IDLE");
#endif
                    }
                    break;

                case SCOPE_SLEWING:
                case SCOPE_PARKING:
                    if (EqNP.getState() != IPS_BUSY)
                    {
                        EqNP.setState(IPS_BUSY);
                        update_needed = true;
#ifdef DEBUG_TRACKSTATE
                        LOG_DEBUG("EqNP set to IPS_BUSY");
#endif
                    }
                    break;

                case SCOPE_TRACKING:
                    if (EqNP.getState() != IPS_OK)
                    {
                        EqNP.setState(IPS_OK);
                        update_needed = true;
#ifdef DEBUG_TRACKSTATE
                        LOG_DEBUG("EqNP set to IPS_OK");
#endif
                    }
                    break;
            }
            if (EqNP[AXIS_RA].getValue() != currentRA || EqNP[AXIS_DE].getValue() != currentDEC)
            {
#ifdef DEBUG_TRACKSTATE
                LOG_DEBUG("EqNP coordinates updated");
#endif
                update_needed = true;
            }
            if (update_needed)
            {
#ifdef DEBUG_TRACKSTATE
                LOG_DEBUG("EqNP changed state");
#endif
                EqNP[AXIS_RA].setValue(currentRA);
                EqNP[AXIS_DE].setValue(currentDEC);
                EqNP.apply();
#ifdef DEBUG_TRACKSTATE
                if (EqNP.s == IPS_BUSY)
                {
                    LOG_DEBUG("EqNP is IPS_BUSY (Goto/slew or Parking)");
                }
                if (EqNP.s == IPS_OK)
                {
                    LOG_DEBUG("EqNP is IPS_OK (Tracking)");
                }
                if (EqNP.s == IPS_IDLE)
                {
                    LOG_DEBUG("EqNP is IPS_IDLE (Not Tracking or Parked)");
                }
                if (EqNP.s == IPS_ALERT)
                {
                    LOG_DEBUG("EqNP is IPS_ALERT (Something wrong)");
                }
#endif
            }
            else
            {
#ifdef DEBUG_TRACKSTATE
                LOG_DEBUG("EqNP UNCHANGED");
#endif
            }
            PrintTrackState();

            // ============= End Telescope Status

            // ============= Refractoring
            if ((strstr(OSStat, "r") || strstr(OSStat, "t"))) //On, either refractory only (r) or full (t)
            {
                if (strstr(OSStat, "t"))
                {
                    IUSaveText(&OnstepStat[2], "Full Comp");
                }
                if (strstr(OSStat, "r"))
                {
                    IUSaveText(&OnstepStat[2], "Refractory Comp");
                }
                if (strstr(OSStat, "s"))
                {
                    IUSaveText(&OnstepStat[8], "Single Axis");
                }
                else
                {
                    IUSaveText(&OnstepStat[8], "2-Axis");
                }
            }
            else
            {
                IUSaveText(&OnstepStat[2], "Refractoring Off");
                IUSaveText(&OnstepStat[8], "N/A");
            }

            //if (strstr(OSStat,"H")) { IUSaveText(&OnstepStat[3],"At Home"); }
            if (strstr(OSStat, "H") && strstr(OSStat, "P"))
            {
                IUSaveText(&OnstepStat[3], "At Home and Parked");
            }
            if (strstr(OSStat, "H") && strstr(OSStat, "p"))
            {
                IUSaveText(&OnstepStat[3], "At Home and UnParked");
            }
            //AutoPauseAtHome
            if (strstr(OSStat, "u"))  //  pa[u]se at home enabled?
            {
                HomePauseS[1].s = ISS_ON;
                HomePauseSP.s = IPS_OK;
                IDSetSwitch(&HomePauseSP, "Pause at Home Enabled");
            }
            else
            {
                HomePauseS[0].s = ISS_ON;
                HomePauseSP.s = IPS_OK;
                IDSetSwitch(&HomePauseSP, nullptr);
            }

            if (strstr(OSStat, "w"))
            {
                IUSaveText(&OnstepStat[3], "Waiting at Home");
            }

            // ============= Pec Status
            if ((!strstr(OSStat, "R") && !strstr(OSStat, "W")))
            {
                IUSaveText(&OnstepStat[4], "N/A");
            }
            if (strstr(OSStat, "R"))
            {
                IUSaveText(&OnstepStat[4], "Recorded");
            }
            if (strstr(OSStat, "W"))
            {
                IUSaveText(&OnstepStat[4], "Autorecord");
            }

            //Handles pec with :GU, also disables the (old) :$QZ?# command
            if (strstr(OSStat, "/"))
            {
                IUSaveText(&OnstepStat[4], "Ignored");
                OSPECviaGU = true;
                OSPECStatusSP.s = IPS_OK;
                OSPECStatusS[0].s = ISS_ON;
                OSPECRecordSP.s = IPS_IDLE;
            }
            if (strstr(OSStat, ";"))
            {

                IUSaveText(&OnstepStat[4], "AutoRecord (waiting on index)");
                OSPECviaGU = true;
                OSPECStatusSP.s = IPS_OK;
                OSPECStatusS[4].s = ISS_ON ;
                OSPECRecordSP.s = IPS_BUSY;
            }
            if (strstr(OSStat, ","))
            {
                IUSaveText(&OnstepStat[4], "AutoPlaying  (waiting on index)");
                OSPECviaGU = true;
                OSPECStatusSP.s = IPS_BUSY;
                OSPECStatusS[3].s = ISS_ON ;
                OSPECRecordSP.s = IPS_IDLE;
            }
            if (strstr(OSStat, "~"))
            {
                IUSaveText(&OnstepStat[4], "Playing");
                OSPECviaGU = true;
                OSPECStatusSP.s = IPS_BUSY;
                OSPECStatusS[1].s = ISS_ON ;
                OSPECRecordSP.s = IPS_IDLE;
            }
            if (strstr(OSStat, "^"))
            {
                IUSaveText(&OnstepStat[4], "Recording");
                OSPECviaGU = true;
                OSPECStatusSP.s = IPS_OK;
                OSPECStatusS[2].s = ISS_ON ;
                OSPECRecordSP.s = IPS_BUSY;
            }
            if (OSPECviaGU)
            {
                if (OSMountType != MOUNTTYPE_ALTAZ && OSMountType != MOUNTTYPE_FORK_ALT )
                {
                    //We have PEC reported via :GU already, enable if any are detected, as they are not reported with ALTAZ/FORK_ALT)
                    //NOTE: Might want to drop the && !strstr(OSStat, "/") as it will startup that way.
                    uint32_t capabilities = GetTelescopeCapability();
                    if ((capabilities | TELESCOPE_HAS_PEC) != capabilities)
                    {
                        LOG_INFO("Telescope detected having PEC, setting that capability");
                        LOGF_DEBUG("capabilities = %x", capabilities);
                        capabilities |= TELESCOPE_HAS_PEC;
                        SetTelescopeCapability(capabilities, 10 );
                        initSlewRates();
                        LX200_OnStep::updateProperties();
                    }
                }
                IDSetSwitch(&OSPECStatusSP, nullptr);
                IDSetSwitch(&OSPECRecordSP, nullptr);
                IDSetSwitch(&OSPECIndexSP, nullptr);
            }



            // ============= Time Sync Status
            if (!strstr(OSStat, "S"))
            {
                IUSaveText(&OnstepStat[5], "N/A");
            }
            if (strstr(OSStat, "S"))
            {
                IUSaveText(&OnstepStat[5], "PPS / GPS Sync Ok");
            }

            // ============= Mount Types
            if (strstr(OSStat, "E"))
            {
                IUSaveText(&OnstepStat[6], "German Equatorial Mount");
                OSMountType = MOUNTTYPE_GEM;
            }
            if (strstr(OSStat, "K"))
            {
                IUSaveText(&OnstepStat[6], "Fork Mount");
                OSMountType = MOUNTTYPE_FORK;
            }
            if (strstr(OSStat, "k"))
            {
                //NOTE: This seems to have been removed from OnStep, so the chances of encountering it are small, I can't even find, but I think it was Alt-Az mounting of a FORK, now folded into ALTAZ
                IUSaveText(&OnstepStat[6], "Fork Alt Mount");
                OSMountType = MOUNTTYPE_FORK_ALT;
            }
            if (strstr(OSStat, "A"))
            {
                IUSaveText(&OnstepStat[6], "AltAZ Mount");
                OSMountType = MOUNTTYPE_ALTAZ;
            }


            //Pier side:
            // o - nOne
            // T - easT
            // W - West
            if (OSMountType != MOUNTTYPE_ALTAZ && OSMountType != MOUNTTYPE_FORK_ALT)
            {
                uint32_t capabilities = GetTelescopeCapability();
                if ((capabilities | TELESCOPE_HAS_PIER_SIDE) != capabilities)
                {
                    LOG_INFO("Telescope detected having Pier Side, adding that capability (many messages duplicated)");
                    LOGF_DEBUG("capabilities = %x", capabilities);
                    capabilities |= TELESCOPE_HAS_PIER_SIDE;
                    SetTelescopeCapability(capabilities, 10 );
                    initSlewRates();
                    LX200_OnStep::updateProperties();
                }
                if (strstr(OSStat, "o"))
                {
                    setPierSide(
                        PIER_UNKNOWN); //Closest match to None, For forks may trigger an extra goto, during imaging if it would do a meridian flip
                    pier_not_set = false;
                }
                if (strstr(OSStat, "T"))
                {
                    setPierSide(PIER_EAST);
                    pier_not_set = false;
                }
                if (strstr(OSStat, "W"))
                {
                    setPierSide(PIER_WEST);
                    pier_not_set = false;
                }
            }

            // ============= Error Code
            //From OnStep: ERR_NONE, ERR_MOTOR_FAULT, ERR_ALT_MIN, ERR_LIMIT_SENSE, ERR_DEC, ERR_AZM, ERR_UNDER_POLE, ERR_MERIDIAN, ERR_SYNC, ERR_PARK, ERR_GOTO_SYNC, ERR_UNSPECIFIED, ERR_ALT_MAX, ERR_GOTO_ERR_NONE, ERR_GOTO_ERR_BELOW_HORIZON, ERR_GOTO_ERR_ABOVE_OVERHEAD, ERR_GOTO_ERR_STANDBY, ERR_GOTO_ERR_PARK, ERR_GOTO_ERR_GOTO, ERR_GOTO_ERR_OUTSIDE_LIMITS, ERR_GOTO_ERR_HARDWARE_FAULT, ERR_GOTO_ERR_IN_MOTION, ERR_GOTO_ERR_UNSPECIFIED

            //For redoing this, quick python, if needed (will only give the error name):
            //all_errors=' (Insert)
            //split_errors=all_errors.split(', ')
            //for specific in split_errors:
            //     print('case ' +specific+':')
            //     print('\tIUSaveText(&OnstepStat[7],"'+specific+'");')
            //     print('\tbreak;')

            Lasterror = (Errors)(OSStat[strlen(OSStat) - 1] - '0');

            // Refresh current Slew Rate
            int idx = OSStat[strlen(OSStat) - 2] - '0';
            if (SlewRateSP.findOnSwitchIndex() != idx)
            {
                SlewRateSP.reset();
                SlewRateSP[idx].setState(ISS_ON);
                SlewRateSP.setState(IPS_OK);
                SlewRateSP.apply();
                LOGF_DEBUG("Slew Rate Index: %d", idx);
            }
            // End Refresh current Slew Rate
        }
        else
        {
            return false;
        }

#ifdef OnStep_Alpha // For the moment, for :Gu
    }
    else
    {
        //TODO: Check and recode :Gu# paths
        int error_or_fail = getCommandSingleCharErrorOrLongResponse(PortFD, OSStat,
                            ":Gu#"); // :Gu# returns a string containing controller status that's bitpacked
        if (strcmp(OSStat, OldOSStat) != 0) //if status changed
        {
            //Ignored for now.
        }
        //Byte 0: Current Status
        if (OSStat[0] & 0b10000001 == 0b10000001)
        {
            //Not tracking
        }
        if (OSStat[0] & 0b10000010 == 0b10000010)
        {
            //No goto
        }
        if (OSStat[0] & 0b10000100 == 0b10000100)
        {
            // PPS sync
            IUSaveText(&OnstepStat[5], "PPS / GPS Sync Ok");
        }
        else
        {
            IUSaveText(&OnstepStat[5], "N/A");
        }
        if (OSStat[0] & 0b10001000 == 0b10001000)
        {
            // Guide active
        }
        //Refraction and Number of axis handled differently for now, might combine to one variable.
        // reply[0]|=0b11010000;                       // Refr enabled Single axis
        // reply[0]|=0b10010000;                       // Refr enabled
        // reply[0]|=0b11100000;                       // OnTrack enabled Single axis
        // reply[0]|=0b10100000;                       // OnTrack enabled
        if ((OSStat[0] & 0b10010000 == 0b10010000
                || OSStat[0] & 0b10100000 == 0b10100000)) //On, either refractory only (r) or full (t)
        {
            if (OSStat[0] & 0b10100000 == 0b10100000)
            {
                IUSaveText(&OnstepStat[2], "Full Comp");
            }
            if (OSStat[0] & 0b10010000 == 0b10010000)
            {
                IUSaveText(&OnstepStat[2], "Refractory Comp");
            }
            if (OSStat[0] & 0b11000000 == 0b11000000)
            {
                IUSaveText(&OnstepStat[8], "Single Axis");
            }
            else
            {
                IUSaveText(&OnstepStat[8], "2-Axis");
            }
        }
        else
        {
            IUSaveText(&OnstepStat[2], "Refractoring Off");
            IUSaveText(&OnstepStat[8], "N/A");
        }
        //Byte 1: Standard tracking rates
        if (OSStat[1] & 0b10000001 == 0b10000001)
        {
            // Lunar rate selected
        }
        if (OSStat[1] & 0b10000010 == 0b10000010)
        {
            // Solar rate selected
        }
        if (OSStat[1] & 0b10000011 == 0b10000011)
        {
            // King rate selected
        }
        //Byte 2: Flags
        if (OSStat[2] & 0b10000001 == 0b10000001)
        {
            // At home
        }
        if (OSStat[2] & 0b10000010 == 0b10000010)
        {
            // Waiting at home
            IUSaveText(&OnstepStat[3], "Waiting at Home");
        }
        if (OSStat[2] & 0b10000100 == 0b10000100)
        {
            // Pause at home enabled?
            //AutoPauseAtHome
            HomePauseS[1].s = ISS_ON;
            HomePauseSP.s = IPS_OK;
            IDSetSwitch(&HomePauseSP, "Pause at Home Enabled");
        }
        else
        {
            HomePauseS[0].s = ISS_ON;
            HomePauseSP.s = IPS_OK;
            IDSetSwitch(&HomePauseSP, nullptr);
        }
        if (OSStat[2] & 0b10001000 == 0b10001000)
        {
            // Buzzer enabled?
        }
        if (OSStat[2] & 0b10010000 == 0b10010000)
        {
            // Auto meridian flip
            AutoFlipS[0].s = ISS_OFF;
            AutoFlipS[1].s = ISS_ON;
            AutoFlipSP.s = IPS_OK;
            IDSetSwitch(&AutoFlipSP, nullptr);
        }
        else
        {
            AutoFlipS[1].s = ISS_OFF;
            AutoFlipS[0].s = ISS_ON;
            AutoFlipSP.s = IPS_OK;
            IDSetSwitch(&AutoFlipSP, nullptr);
        }
        if (OSStat[2] & 0b10100000 == 0b10100000)
        {
            // PEC data has been recorded
        }




        //Byte 3: Mount type and info
        if (OSStat[3] & 0b10000001 == 0b10000001)
        {
            // GEM
            IUSaveText(&OnstepStat[6], "German Mount");
            OSMountType = MOUNTTYPE_GEM;
        }
        if (OSStat[3] & 0b10000010 == 0b10000010)
        {
            // FORK
            IUSaveText(&OnstepStat[6], "Fork Mount");
            OSMountType = MOUNTTYPE_FORK;
        }
        if (OSStat[3] & 0b10000100 == 0b10000100)   //Seems depreciated/subsumed into  FORK
        {
            // Fork Alt
            IUSaveText(&OnstepStat[6], "Fork Alt Mount");
            OSMountType = MOUNTTYPE_FORK_ALT;
        }
        if (OSStat[3] & 0b10001000 == 0b10001000)
        {
            // ALTAZM
            IUSaveText(&OnstepStat[6], "AltAZ Mount");
            OSMountType = MOUNTTYPE_ALTAZ;
        }


        setPierSide(PIER_UNKNOWN);
        if (OSStat[3] & 0b10010000 == 0b10010000)
        {
            // Pier side none
            setPierSide(PIER_UNKNOWN);
            // INDI doesn't account for 'None'
        }
        if (OSStat[3] & 0b10100000 == 0b10100000)
        {
            // Pier side east
            setPierSide(PIER_EAST);
        }
        if (OSStat[3] & 0b11000000 == 0b11000000)
        {
            // Pier side west
            setPierSide(PIER_WEST);
        }
        //     Byte 4: PEC
        PECStatusGU = OSStat[4] & 0b01111111;
        if (OSStat[4] == 0)
        {
            // AltAZM, no PEC possible
            PECStatusGU = 0;
        }
        else
        {
            //    PEC status: 0 ignore, 1 get ready to play, 2 playing, 3 get ready to record, 4 recording


        }
        ParkStatusGU = OSStat[5] & 0b01111111;
        PulseGuideGU = OSStat[6] & 0b01111111;
        GuideRateGU = OSStat[7] & 0b01111111;
        LastError = OSStat[8] & 0b01111111;
    }
#endif


    switch (Lasterror)
    {
        case ERR_NONE:
            IUSaveText(&OnstepStat[7], "None");
            break;
        case ERR_MOTOR_FAULT:
            IUSaveText(&OnstepStat[7], "Motor/Driver Fault");
            break;
        case ERR_ALT_MIN:
            IUSaveText(&OnstepStat[7], "Below Horizon Limit");
            break;
        case ERR_LIMIT_SENSE:
            IUSaveText(&OnstepStat[7], "Limit Sense");
            break;
        case ERR_DEC:
            IUSaveText(&OnstepStat[7], "Dec Limit Exceeded");
            break;
        case ERR_AZM:
            IUSaveText(&OnstepStat[7], "Azm Limit Exceeded");
            break;
        case ERR_UNDER_POLE:
            IUSaveText(&OnstepStat[7], "Under Pole Limit Exceeded");
            break;
        case ERR_MERIDIAN:
            IUSaveText(&OnstepStat[7], "Meridian Limit (W) Exceeded");
            break;
        case ERR_SYNC:
            IUSaveText(&OnstepStat[7], "Sync Safety Limit Exceeded");
            break;
        case ERR_PARK:
            IUSaveText(&OnstepStat[7], "Park Failed");
            break;
        case ERR_GOTO_SYNC:
            IUSaveText(&OnstepStat[7], "Goto Sync Failed");
            break;
        case ERR_UNSPECIFIED:
            IUSaveText(&OnstepStat[7], "Unspecified Error");
            break;
        case ERR_ALT_MAX:
            IUSaveText(&OnstepStat[7], "Above Overhead Limit");
            break;
        case ERR_GOTO_ERR_NONE:
            IUSaveText(&OnstepStat[7], "Goto No Error");
            break;
        case ERR_GOTO_ERR_BELOW_HORIZON:
            IUSaveText(&OnstepStat[7], "Goto Below Horizon");
            break;
        case ERR_GOTO_ERR_ABOVE_OVERHEAD:
            IUSaveText(&OnstepStat[7], "Goto Abv Overhead");
            break;
        case ERR_GOTO_ERR_STANDBY:
            IUSaveText(&OnstepStat[7], "Goto Err Standby");
            break;
        case ERR_GOTO_ERR_PARK:
            IUSaveText(&OnstepStat[7], "Goto Err Park");
            break;
        case ERR_GOTO_ERR_GOTO:
            IUSaveText(&OnstepStat[7], "Goto Err Goto");
            break;
        case ERR_GOTO_ERR_OUTSIDE_LIMITS:
            IUSaveText(&OnstepStat[7], "Goto Outside Limits");
            break;
        case ERR_GOTO_ERR_HARDWARE_FAULT:
            IUSaveText(&OnstepStat[7], "Goto H/W Fault");
            break;
        case ERR_GOTO_ERR_IN_MOTION:
            IUSaveText(&OnstepStat[7], "Goto Err Motion");
            break;
        case ERR_GOTO_ERR_UNSPECIFIED:
            IUSaveText(&OnstepStat[7], "Goto Unspecified Error");
            break;
        default:
            IUSaveText(&OnstepStat[7], "Unknown Error");
            break;
    }

#ifndef OnStep_Alpha
    // Get actual Pier Side
    if (pier_not_set)
    {
        if (OSMountType == MOUNTTYPE_ALTAZ || OSMountType == MOUNTTYPE_FORK_ALT)
        {
            setPierSide(PIER_UNKNOWN);
        }
        else
        {
            int error_or_fail = getCommandSingleCharErrorOrLongResponse(PortFD, OSPier, ":Gm#");
            if (error_or_fail > 1)
            {
                if (strcmp(OSPier, OldOSPier) != 0) // any change ?
                {
                    strncpy(OldOSPier, OSPier, sizeof(OldOSPier));
                    switch(OSPier[0])
                    {
                        case 'E':
                            setPierSide(PIER_EAST);
                            break;
                        case 'W':
                            setPierSide(PIER_WEST);
                            break;
                        case 'N':
                            setPierSide(PIER_UNKNOWN);
                            break;
                        case '?':
                            setPierSide(PIER_UNKNOWN);
                            break;
                    }
                }
            }
            else
            {
                LOG_WARN("Communication error on Pier Side (:Gm#), this update aborted, will try again...");
                return true; //COMMUNICATION ERROR, BUT DON'T PUT TELESCOPE IN ERROR STATE
            }
        }
    }
#endif


    //========== Jasem Mutlaq 2025.01.10: If we are in manual motion, immediately return as rapid updates for RA/DE are far more critical
    // then the rest of the measurements below.
    if (MovementNSSP.getState() == IPS_BUSY || MovementWESP.getState() == IPS_BUSY)
        return true;

    //========== Get actual Backlash values
    double backlash_DEC, backlash_RA;
    int BD_error = getCommandDoubleResponse(PortFD, &backlash_DEC, OSbacklashDEC, ":%BD#");
    int BR_error = getCommandDoubleResponse(PortFD, &backlash_RA, OSbacklashRA, ":%BR#");
    if (BD_error > 1 && BR_error > 1)
    {
        BacklashNP.np[0].value = backlash_DEC;
        BacklashNP.np[1].value = backlash_RA;
        IDSetNumber(&BacklashNP, nullptr);
    }
    else
    {
        LOG_WARN("Communication error on backlash (:%BD#/:%BR#), this update aborted, will try again...");
        return true; //COMMUNICATION ERROR, BUT DON'T PUT TELESCOPE IN ERROR STATE
    }

    double pulseguiderate = 0.0;
    if (getCommandDoubleResponse(PortFD, &pulseguiderate, GuideValue, ":GX90#") > 1)
    {
        LOGF_DEBUG("Guide Rate String: %s", GuideValue);
        pulseguiderate = atof(GuideValue);
        LOGF_DEBUG("Guide Rate: %f", pulseguiderate);
        GuideRateNP.np[0].value = pulseguiderate;
        GuideRateNP.np[1].value = pulseguiderate;
        IDSetNumber(&GuideRateNP, nullptr);
    }
    else
    {
        LOGF_DEBUG("Guide Rate String: %s", GuideValue);
        LOG_DEBUG("Guide rate error response, Not setting guide rate from :GX90# response, falling back to :GU#, which may not be accurate, if custom settings are used");
        int pulseguiderateint = (Errors)(OSStat[strlen(OSStat) - 3] - '0');
        switch(pulseguiderateint)
        {
            case 0:
                pulseguiderate = (double)0.25;
                break;
            case 1:
                pulseguiderate = (double)0.5;
                break;
            case 2:
                pulseguiderate = (double)1.0;
                break;
            default:
                pulseguiderate = 0.0;
                LOG_DEBUG("Could not get guide rate from :GU# response, not setting");
                LOG_WARN("Communication error on Guide Rate (:GX90#/:GU#), this update aborted, will try again...");
                return true; //COMMUNICATION ERROR, BUT DON'T PUT TELESCOPE IN ERROR STATE
        }
        if (pulseguiderate != 0.0)
        {
            LOGF_DEBUG("Guide Rate: %f", pulseguiderate);
            GuideRateNP.np[0].value = pulseguiderate;
            GuideRateNP.np[1].value = pulseguiderate;
            IDSetNumber(&GuideRateNP, nullptr);
        }
    }

#ifndef OnStep_Alpha
    if (OSMountType == MOUNTTYPE_GEM)
    {
        //AutoFlip
        char merdidianflipauto_response[RB_MAX_LEN] = {0};
        int gx95_error  = getCommandSingleCharErrorOrLongResponse(PortFD, merdidianflipauto_response, ":GX95#");
        if (gx95_error > 1)
        {
            if (merdidianflipauto_response[0] == '1') // && merdidianflipauto_response[1] == 0) //Only set on 1#
            {
                AutoFlipS[0].s = ISS_OFF;
                AutoFlipS[1].s = ISS_ON;
                AutoFlipSP.s = IPS_OK;
                IDSetSwitch(&AutoFlipSP, nullptr);
            }
            else if (merdidianflipauto_response[0] == '0') // && merdidianflipauto_response[1] == 0) //Only set on 1#
            {
                AutoFlipS[1].s = ISS_OFF;
                AutoFlipS[0].s = ISS_ON;
                AutoFlipSP.s = IPS_OK;
                IDSetSwitch(&AutoFlipSP, nullptr);
            }
        }
        else
        {
            LOG_WARN("Communication error on meridianAutoFlip (:GX95#), this update aborted, will try again...");
            return true; //COMMUNICATION ERROR, BUT DON'T PUT TELESCOPE IN ERROR STATE
        }
    }
#endif

    if (OSMountType == MOUNTTYPE_GEM)   //Doesn't apply to non-GEMs
    {
        //PreferredPierSide
        char preferredpierside_response[RB_MAX_LEN] = {0};
        int gx96_error  = getCommandSingleCharErrorOrLongResponse(PortFD, preferredpierside_response, ":GX96#");
        if (gx96_error > 1)
        {
            if (strstr(preferredpierside_response, "W"))
            {
                PreferredPierSideS[0].s = ISS_ON;
                PreferredPierSideSP.s = IPS_OK;
                IDSetSwitch(&PreferredPierSideSP, nullptr);
            }
            else if (strstr(preferredpierside_response, "E"))
            {
                PreferredPierSideS[1].s = ISS_ON;
                PreferredPierSideSP.s = IPS_OK;
                IDSetSwitch(&PreferredPierSideSP, nullptr);
            }
            else if (strstr(preferredpierside_response, "B"))
            {
                PreferredPierSideS[2].s = ISS_ON;
                PreferredPierSideSP.s = IPS_OK;
                IDSetSwitch(&PreferredPierSideSP, nullptr);
            }
            /* remove this dead code
            else if (strstr(preferredpierside_response, "%"))
            {
                //NOTE: This bug is only present in very early OnStepX, and should be fixed shortly after 10.03k
                LOG_DEBUG(":GX96 returned \% indicating early OnStepX bug");
                IUResetSwitch(&PreferredPierSideSP);
                PreferredPierSideSP.s = IPS_ALERT;
                IDSetSwitch(&PreferredPierSideSP, nullptr);
            }
            */
            else
            {
                IUResetSwitch(&PreferredPierSideSP);
                PreferredPierSideSP.s = IPS_BUSY;
                IDSetSwitch(&PreferredPierSideSP, nullptr);
            }
        }
        else
        {
            LOG_WARN("Communication error on Preferred Pier Side (:GX96#), this update aborted, will try again...");
            return true; //COMMUNICATION ERROR, BUT DON'T PUT TELESCOPE IN ERROR STATE
        }

        if (OSMountType == MOUNTTYPE_GEM)
        {
            // Minutes past Meridian, Onstep uses angulat values in degree we use minutes: 1° = 4 minutes
            char limit1_response[RB_MAX_LEN] = {0};
            int gxea_error, gxe9_error;
            double minutes_past_Meridian_East, minutes_past_Meridian_West;
            gxe9_error  = getCommandDoubleResponse(PortFD, &minutes_past_Meridian_East, limit1_response, ":GXE9#");
            if (gxe9_error > 1) //NOTE: Possible failure not checked.
            {
                char limit2_response[RB_MAX_LEN] = {0};
                gxea_error  = getCommandDoubleResponse(PortFD, &minutes_past_Meridian_West, limit2_response, ":GXEA#");
                if (gxea_error > 1)   //NOTE: Possible failure not checked.
                {
                    minutesPastMeridianNP.np[0].value = minutes_past_Meridian_East; // E
                    minutesPastMeridianNP.np[1].value = minutes_past_Meridian_West; //W
                    IDSetNumber(&minutesPastMeridianNP, nullptr);
                }
                else
                {
                    LOG_WARN("Communication error on Degrees past Meridian West (:GXEA#), this update aborted, will try again...");
                    return true; //COMMUNICATION ERROR, BUT DON'T PUT TELESCOPE IN ERROR STATE
                }
            }
            else
            {
                LOG_WARN("Communication error on Degrees past Meridian East (:GXE9#), this update aborted, will try again...");
                return true; //COMMUNICATION ERROR, BUT DON'T PUT TELESCOPE IN ERROR STATE
            }
        }
    }
    // Get Overhead Limits
    // :Go#       Get Overhead Limit
    //            Returns: DD*#
    //            The highest elevation above the horizon that the telescope will goto
    char Go[RB_MAX_LEN] = {0};
    int Go_int ;
    int Go_error = getCommandIntResponse(PortFD, &Go_int, Go, ":Go#");
    if (Go_error > 0)
    {
        ElevationLimitN[1].value =  atoi(Go);
        IDSetNumber(&ElevationLimitNP, nullptr);
        LOGF_DEBUG("Elevation Limit Min: %s, %i Go_nbcar: %i", Go, Go_int, Go_error);    //typo
    }
    else
    {
        LOG_WARN("Communication :Go# error, check connection.");
        flushIO(PortFD); //Unlikely to do anything, but just in case.
    }

    // :Gh#       Get Horizon Limit, the minimum elevation of the mount relative to the horizon
    //            Returns: sDD*#
    char Gh[RB_MAX_LEN] = {0};
    int Gh_int ;
    int Gh_error = getCommandIntResponse(PortFD, &Gh_int, Gh, ":Gh#");
    if (Gh_error > 0)
    {
        ElevationLimitN[0].value =  atoi(Gh);
        IDSetNumber(&ElevationLimitNP, nullptr);
        LOGF_DEBUG("Elevation Limit Min: %s, %i Gh_nbcar: %i", Gh, Gh_int, Gh_error);    //typo
    }
    else
    {
        LOG_WARN("Communication :Gh# error, check connection.");
        flushIO(PortFD); //Unlikely to do anything, but just in case.
    }
    // End Get Overhead Limits

    //TODO: Improve Rotator support
    if (OSUpdateRotator() != 0)
    {
        LOG_WARN("Communication error on Rotator Update, this update aborted, will try again...");
        return true; //COMMUNICATION ERROR, BUT DON'T PUT TELESCOPE IN ERROR STATE
    }

    //Weather update
    char temperature_response[RB_MAX_LEN] = {0};
    double temperature_value;
    int gx9a_error  = getCommandDoubleResponse(PortFD, &temperature_value, temperature_response, ":GX9A#");
    if (gx9a_error > 1) //> 1 as an OnStep error would be 1 char in response
    {
        setParameterValue("WEATHER_TEMPERATURE", temperature_value);
    }
    else
    {
        LOG_WARN("Communication error on Temperature (:GX9A#), this update aborted, will try again...");
        return true; //COMMUNICATION ERROR, BUT DON'T PUT TELESCOPE IN ERROR STATE
    }

    char humidity_response[RB_MAX_LEN] = {0};
    double humidity_value;
    int gx9c_error  = getCommandDoubleResponse(PortFD, &humidity_value, humidity_response, ":GX9C#");
    if (gx9c_error > 1) //> 1 as an OnStep error would be 1 char in response
    {
        setParameterValue("WEATHER_HUMIDITY", humidity_value);
    }
    else
    {
        LOG_WARN("Communication error on Humidity (:GX9C#), this update aborted, will try again...");
        return true; //COMMUNICATION ERROR, BUT DON'T PUT TELESCOPE IN ERROR STATE
    }


    char barometer_response[RB_MAX_LEN] = {0};
    double barometer_value;
    int gx9b_error  = getCommandDoubleResponse(PortFD, &barometer_value, barometer_response, ":GX9B#");
    if (gx9b_error > 1)
    {
        setParameterValue("WEATHER_BAROMETER", barometer_value);
    }
    else
    {
        LOG_WARN("Communication error on Barometer (:GX9B#), this update aborted, will try again...");
        return true; //COMMUNICATION ERROR, BUT DON'T PUT TELESCOPE IN ERROR STATE
    }

    char dewpoint_reponse[RB_MAX_LEN] = {0};
    double dewpoint_value;
    int gx9e_error  = getCommandDoubleResponse(PortFD, &dewpoint_value, dewpoint_reponse, ":GX9E#");
    if (gx9e_error > 1)
    {
        setParameterValue("WEATHER_DEWPOINT", dewpoint_value);
    }
    else
    {
        LOG_WARN("Communication error on Dewpoint (:GX9E#), this update aborted, will try again...");
        return true; //COMMUNICATION ERROR, BUT DON'T PUT TELESCOPE IN ERROR STATE
    }

    if (OSCpuTemp_good)
    {
        char cputemp_reponse[RB_MAX_LEN] = {0};
        double cputemp_value;
        int error_return = getCommandDoubleResponse(PortFD, &cputemp_value, cputemp_reponse, ":GX9F#");
        if ( error_return >= 0) // && !strcmp(cputemp_reponse, "0") )
        {
            setParameterValue("WEATHER_CPU_TEMPERATURE", cputemp_value);
        }
        else
        {
            LOGF_DEBUG("CPU Temp not responded to, disabling further checks, return values: error_return: %i, cputemp_reponse: %s",
                       error_return, cputemp_reponse);
            OSCpuTemp_good = false;
        }
    }
    //
    //Disabled, because this is supplied via Kstars or other location, no sensor to read this
    //double altitude_value;
    //int error_or_fail = getCommandDoubleResponse(PortFD, &altitude_value, TempValue, ":GX9D#");
    //setParameterValue("WEATHER_ALTITUDE", altitude_value);
    WI::updateProperties();

    if (WI::syncCriticalParameters())
        critialParametersLP.apply();
    ParametersNP.setState(IPS_OK);
    ParametersNP.apply();

    if (TMCDrivers)
    {
        for (int driver_number = 1; driver_number < 3; driver_number++)
        {
            char TMCDriverTempValue[RB_MAX_LEN] = {0};
            char TMCDriverCMD[CMD_MAX_LEN] = {0};
            snprintf(TMCDriverCMD, sizeof(TMCDriverCMD), ":GXU%i#", driver_number);
            if (TMCDrivers)   //Prevent check on :GXU2# if :GXU1# failed
            {
                int i = getCommandSingleCharErrorOrLongResponse(PortFD, TMCDriverTempValue, TMCDriverCMD);
                if (i == -4  && TMCDriverTempValue[0] == '0' )
                {
                    char ResponseText[RB_MAX_LEN] = {0};
                    snprintf(ResponseText, sizeof(ResponseText),  "TMC Reporting not detected, Axis %i", driver_number);
                    IUSaveText(&OnstepStat[8 + driver_number], ResponseText);
                    LOG_DEBUG("TMC Drivers responding as if not there, disabling further checks");
                    TMCDrivers = false;
                }
                else
                {
                    if (i > 0 )
                    {
                        if (TMCDriverTempValue[0] == 0)
                        {
                            IUSaveText(&OnstepStat[8 + driver_number], "No Condition");
                            TMCDrivers = false;
                        }
                        else
                        {
                            char StepperState[1024] = {0};
                            bool unknown_value = false;
                            int current_position = 0;
                            while (TMCDriverTempValue[current_position] != 0 && unknown_value == false)
                            {
                                if (TMCDriverTempValue[current_position] == ',')
                                {
                                    current_position++;
                                }
                                else
                                {
                                    if (TMCDriverTempValue[current_position] == 'S' && TMCDriverTempValue[current_position + 1] == 'T')
                                    {
                                        strcat(StepperState, "Standstill,");
                                    }
                                    else if (TMCDriverTempValue[current_position] == 'O' && TMCDriverTempValue[current_position + 1] == 'A')
                                    {
                                        strcat(StepperState, "Open Load A Pair,");
                                    }
                                    else if (TMCDriverTempValue[current_position] == 'O' && TMCDriverTempValue[current_position + 1] == 'B')
                                    {
                                        strcat(StepperState, "Open Load B Pair,");
                                    }
                                    else if (TMCDriverTempValue[current_position] == 'G' && TMCDriverTempValue[current_position + 1] == 'A')
                                    {
                                        strcat(StepperState, "Short to Ground A Pair,");
                                    }
                                    else if (TMCDriverTempValue[current_position] == 'G' && TMCDriverTempValue[current_position + 1] == 'B')
                                    {
                                        strcat(StepperState, "Short to Ground B Pair,");
                                    }
                                    else if (TMCDriverTempValue[current_position] == 'O' && TMCDriverTempValue[current_position + 1] == 'T')
                                    {
                                        strcat(StepperState, "Over Temp (>150C),");
                                    }
                                    else if (TMCDriverTempValue[current_position] == 'P' && TMCDriverTempValue[current_position + 1] == 'W')
                                    {
                                        strcat(StepperState, "Pre-Warning: Over Temp (>120C),");
                                    }
                                    else if (TMCDriverTempValue[current_position] == 'G' && TMCDriverTempValue[current_position + 1] == 'F')
                                    {
                                        strcat(StepperState, "General Fault,");
                                    }
                                    else
                                    {
                                        unknown_value = true;
                                        break;
                                    }
                                    current_position = current_position + 3;
                                }
                            }
                            if (unknown_value)
                            {
                                IUSaveText(&OnstepStat[8 + driver_number], TMCDriverTempValue);
                            }
                            else
                            {
                                IUSaveText(&OnstepStat[8 + driver_number], StepperState);
                            }
                        }
                    }
                    else
                    {
                        IUSaveText(&OnstepStat[8 + driver_number], "Unknown read error");
                    }
                }
            }
        }
    }

    // Update OnStep Status TAB
    IDSetText(&OnstepStatTP, nullptr);
    //Align tab, so it doesn't conflict
    //May want to reduce frequency of updates
    if (!UpdateAlignStatus())
    {
        LOG_WARN("Fail Align Command");
        LOG_WARN("Communication error on Align Status Update, this update aborted, will try again...");
        return true; //COMMUNICATION ERROR, BUT DON'T PUT TELESCOPE IN ERROR STATE
    }
    UpdateAlignErr();


    if (OSUpdateFocuser() != 0)  // Update Focuser Position
    {
        LOG_WARN("Communication error on Focuser Update, this update aborted, will try again...");
        return true; //COMMUNICATION ERROR, BUT DON'T PUT TELESCOPE IN ERROR STATE
    }

#ifndef OnStep_Alpha
    if (!OSPECviaGU)
    {
        PECStatus(0);
    }
    //#Gu# has this built in
#endif


    return true;
}


bool LX200_OnStep::SetTrackEnabled(bool enabled) //track On/Off events handled by inditelescope       Tested
{
    char response[RB_MAX_LEN];

    if (enabled)
    {
        int res = getCommandSingleCharResponse(PortFD, response, ":Te#"); //0 = failure, 1 = success, no # on reply
        if(res < 0 || response[0] == '0')
        {
            LOGF_ERROR("===CMD==> Track On %s", response);
            return false;
        }
    }
    else
    {
        int res = getCommandSingleCharResponse(PortFD, response, ":Td#"); //0 = failure, 1 = success, no # on reply
        if(res < 0 || response[0] == '0')
        {
            LOGF_ERROR("===CMD==> Track Off %s", response);
            return false;
        }
    }
    return true;
}

bool LX200_OnStep::setLocalDate(uint8_t days, uint8_t months, uint16_t years)
{
    years = years % 100;
    char cmd[CMD_MAX_LEN] = {0};

    snprintf(cmd, CMD_MAX_LEN, ":SC%02d/%02d/%02d#", months, days, years);

    if (!sendOnStepCommand(cmd)) return true;
    return false;
}

bool LX200_OnStep::sendOnStepCommandBlind(const char *cmd)
{
    int error_type;
    int nbytes_write = 0;

    DEBUGF(DBG_SCOPE, "CMD <%s>", cmd);

    flushIO(PortFD);
    /* Add mutex */
    std::unique_lock<std::mutex> guard(lx200CommsLock);
    tcflush(PortFD, TCIFLUSH);


    if ((error_type = tty_write_string(PortFD, cmd, &nbytes_write)) != TTY_OK)
    {
        LOGF_ERROR("CHECK CONNECTION: Error sending command %s", cmd);
        return 0; //Fail if we can't write
        //return error_type;
    }

    return 1;
}

bool LX200_OnStep::sendOnStepCommand(const char *cmd)
{
    char response[1] = {0};
    int error_type;
    int nbytes_write = 0, nbytes_read = 0;

    DEBUGF(DBG_SCOPE, "CMD <%s>", cmd);

    flushIO(PortFD);
    /* Add mutex */
    std::unique_lock<std::mutex> guard(lx200CommsLock);
    tcflush(PortFD, TCIFLUSH);

    if ((error_type = tty_write_string(PortFD, cmd, &nbytes_write)) != TTY_OK)
        return error_type;

    error_type = tty_read_expanded(PortFD, response, 1, OSTimeoutSeconds, OSTimeoutMicroSeconds, &nbytes_read);

    tcflush(PortFD, TCIFLUSH);
    DEBUGF(DBG_SCOPE, "RES <%c>", response[0]);

    if (nbytes_read < 1)
    {
        LOG_WARN("Timeout/Error on response. Check connection.");
        return false;
    }

    return (response[0] == '0'); //OnStep uses 0 for success and non zero for failure, in *most* cases;
}

int LX200_OnStep::getCommandSingleCharResponse(int fd, char *data, const char *cmd)
{
    char *term;
    int error_type;
    int nbytes_write = 0, nbytes_read = 0;

    DEBUGF(DBG_SCOPE, "CMD <%s>", cmd);

    flushIO(fd);
    /* Add mutex */
    std::unique_lock<std::mutex> guard(lx200CommsLock);

    if ((error_type = tty_write_string(fd, cmd, &nbytes_write)) != TTY_OK)
        return error_type;

    error_type = tty_read_expanded(fd, data, 1, OSTimeoutSeconds, OSTimeoutMicroSeconds, &nbytes_read);
    tcflush(fd, TCIFLUSH);

    if (error_type != TTY_OK)
        return error_type;

    term = strchr(data, '#');
    if (term)
        *term = '\0';
    if (nbytes_read < RB_MAX_LEN) //given this function that should always be true, as should nbytes_read always be 1
    {
        data[nbytes_read] = '\0';
    }
    else
    {
        LOG_DEBUG("got RB_MAX_LEN bytes back (which should never happen), last byte set to null and possible overflow");
        data[RB_MAX_LEN - 1] = '\0';
    }

    DEBUGF(DBG_SCOPE, "RES <%s>", data);

    return nbytes_read;
}


int LX200_OnStep::flushIO(int fd)
{
    tcflush(fd, TCIOFLUSH);
    int error_type = 0;
    int nbytes_read;
    std::unique_lock<std::mutex> guard(lx200CommsLock);
    tcflush(fd, TCIOFLUSH);
    do
    {
        char discard_data[RB_MAX_LEN] = {0};
        error_type = tty_nread_section_expanded(fd, discard_data, sizeof(discard_data), '#', 0, 1000, &nbytes_read);
        if (error_type >= 0)
        {
            LOGF_DEBUG("flushIO: Information in buffer: Bytes: %u, string: %s", nbytes_read, discard_data);
        }
        //LOGF_DEBUG("flushIO: error_type = %i", error_type);
    }
    while (error_type > 0);
    return 0;
}

int LX200_OnStep::getCommandDoubleResponse(int fd, double *value, char *data, const char *cmd)
{
    char *term;
    int error_type;
    int nbytes_write = 0, nbytes_read = 0;

    DEBUGF(DBG_SCOPE, "CMD <%s>", cmd);

    flushIO(fd);
    /* Add mutex */
    std::unique_lock<std::mutex> guard(lx200CommsLock);
    tcflush(fd, TCIFLUSH);

    if ((error_type = tty_write_string(fd, cmd, &nbytes_write)) != TTY_OK)
        return error_type;

    error_type = tty_nread_section_expanded(fd, data, RB_MAX_LEN, '#', OSTimeoutSeconds, OSTimeoutMicroSeconds, &nbytes_read);
    tcflush(fd, TCIFLUSH);

    term = strchr(data, '#');
    if (term)
        *term = '\0';
    if (nbytes_read < RB_MAX_LEN) //If within buffer, terminate string with \0 (in case it didn't find the #)
    {
        data[nbytes_read] = '\0'; //Indexed at 0, so this is the byte passed it
    }
    else
    {
        LOG_DEBUG("got RB_MAX_LEN bytes back, last byte set to null and possible overflow");
        data[RB_MAX_LEN - 1] = '\0';
    }

    DEBUGF(DBG_SCOPE, "RES <%s>", data);

    if (error_type != TTY_OK)
    {
        LOGF_DEBUG("Error %d", error_type);
        LOG_DEBUG("Flushing connection");
        tcflush(fd, TCIOFLUSH);
        return error_type;
    }

    if (sscanf(data, "%lf", value) != 1)
    {
        LOG_WARN("Invalid response, check connection");
        LOG_DEBUG("Flushing connection");
        tcflush(fd, TCIOFLUSH);
        return RES_ERR_FORMAT; //-1001, so as not to conflict with TTY_RESPONSE;
    }

    return nbytes_read;

}


int LX200_OnStep::getCommandIntResponse(int fd, int *value, char *data, const char *cmd)
{
    char *term;
    int error_type;
    int nbytes_write = 0, nbytes_read = 0;

    DEBUGF(DBG_SCOPE, "CMD <%s>", cmd);

    flushIO(fd);
    /* Add mutex */
    std::unique_lock<std::mutex> guard(lx200CommsLock);
    tcflush(fd, TCIFLUSH);

    if ((error_type = tty_write_string(fd, cmd, &nbytes_write)) != TTY_OK)
        return error_type;

    error_type = tty_nread_section_expanded(fd, data, RB_MAX_LEN, '#', OSTimeoutSeconds, OSTimeoutMicroSeconds, &nbytes_read);
    tcflush(fd, TCIFLUSH);

    term = strchr(data, '#');
    if (term)
        *term = '\0';
    if (nbytes_read < RB_MAX_LEN) //If within buffer, terminate string with \0 (in case it didn't find the #)
    {
        data[nbytes_read] = '\0'; //Indexed at 0, so this is the byte passed it
    }
    else
    {
        LOG_DEBUG("got RB_MAX_LEN bytes back, last byte set to null and possible overflow");
        data[RB_MAX_LEN - 1] = '\0';
    }
    DEBUGF(DBG_SCOPE, "RES <%s>", data);
    if (error_type != TTY_OK)
    {
        LOGF_DEBUG("Error %d", error_type);
        LOG_DEBUG("Flushing connection");
        tcflush(fd, TCIOFLUSH);
        return error_type;
    }
    if (sscanf(data, "%i", value) != 1)
    {
        LOG_WARN("Invalid response, check connection");
        LOG_DEBUG("Flushing connection");
        tcflush(fd, TCIOFLUSH);
        return RES_ERR_FORMAT; //-1001, so as not to conflict with TTY_RESPONSE;
    }
    return nbytes_read;
}

int LX200_OnStep::getCommandSingleCharErrorOrLongResponse(int fd, char *data, const char *cmd)
{
    char *term;
    int error_type;
    int nbytes_write = 0, nbytes_read = 0;

    DEBUGF(DBG_SCOPE, "CMD <%s>", cmd);

    flushIO(fd);
    /* Add mutex */
    std::unique_lock<std::mutex> guard(lx200CommsLock);
    tcflush(fd, TCIFLUSH);

    if ((error_type = tty_write_string(fd, cmd, &nbytes_write)) != TTY_OK)
        return error_type;

    error_type = tty_nread_section_expanded(fd, data, RB_MAX_LEN, '#', OSTimeoutSeconds, OSTimeoutMicroSeconds, &nbytes_read);
    tcflush(fd, TCIFLUSH);

    term = strchr(data, '#');
    if (term)
        *term = '\0';
    if (nbytes_read < RB_MAX_LEN) //If within buffer, terminate string with \0 (in case it didn't find the #)
    {
        data[nbytes_read] = '\0'; //Indexed at 0, so this is the byte passed it
    }
    else
    {
        LOG_DEBUG("got RB_MAX_LEN bytes back, last byte set to null and possible overflow");
        data[RB_MAX_LEN - 1] = '\0';
    }

    DEBUGF(DBG_SCOPE, "RES <%s>", data);

    if (error_type != TTY_OK)
    {
        LOGF_DEBUG("Error %d", error_type);
        return error_type;
    }
    return nbytes_read;

}



bool LX200_OnStep::updateLocation(double latitude, double longitude, double elevation)
{
    INDI_UNUSED(elevation);

    if (isSimulation())
        return true;

    double onstep_long = 360 - longitude ;
    while (onstep_long < 0)
        onstep_long += 360;
    while (onstep_long > 360)
        onstep_long -= 360;

    if (!isSimulation() && setSiteLongitude(PortFD, onstep_long) < 0)
    {
        LOG_ERROR("Error setting site longitude coordinates");
        return false;
    }

    if (!isSimulation() && setSiteLatitude(PortFD, latitude) < 0)
    {
        LOG_ERROR("Error setting site latitude coordinates");
        return false;
    }

    char l[32] = {0}, L[32] = {0};
    fs_sexa(l, latitude, 3, 360000);
    fs_sexa(L, longitude, 4, 360000);

    LOGF_INFO("Site location updated to Lat %.32s - Long %.32s", l, L);

    return true;
}

int LX200_OnStep::setMinElevationLimit(int fd, int max)   // According to standard command is :SoDD*#       Tested
{
    LOGF_INFO("<%s>", __FUNCTION__);

    char read_buffer[RB_MAX_LEN] = {0};

    snprintf(read_buffer, sizeof(read_buffer), ":So%02d#", max);

    return (setStandardProcedure(fd, read_buffer));
}

int LX200_OnStep::setSiteLongitude(int fd, double Long)
{
    int d, m;
    double s;
    char read_buffer[32];

    getSexComponentsIID(Long, &d, &m, &s);
    if (OSHighPrecision)
    {
        snprintf(read_buffer, sizeof(read_buffer), ":Sg%.03d:%02d:%.02f#", d, m, s);
        int result1 = setStandardProcedure(fd, read_buffer);
        if (result1 == 0)
        {
            return 0;
        }
        else
        {
            snprintf(read_buffer, sizeof(read_buffer), ":Sg%03d:%02d#", d, m);
            return (setStandardProcedure(fd, read_buffer));
        }
    }
    snprintf(read_buffer, sizeof(read_buffer), ":Sg%03d:%02d#", d, m);
    return (setStandardProcedure(fd, read_buffer));
}

int LX200_OnStep::setSiteLatitude(int fd, double Long)
{
    int d, m;
    double s;
    char read_buffer[32];

    getSexComponentsIID(Long, &d, &m, &s);

    if(OSHighPrecision)
    {
        snprintf(read_buffer, sizeof(read_buffer), ":St%+.02d:%02d:%.02f#", d, m, s);
        int result1 = setStandardProcedure(fd, read_buffer);
        if (result1 == 0)
        {
            return 0;
        }
        else
        {
            snprintf(read_buffer, sizeof(read_buffer), ":St%+03d:%02d#", d, m);
            return (setStandardProcedure(fd, read_buffer));
        }
    }
    snprintf(read_buffer, sizeof(read_buffer), ":St%+03d:%02d#", d, m);
    return (setStandardProcedure(fd, read_buffer));
}




/***** FOCUSER INTERFACE ******

NOT USED:
virtual bool    SetFocuserSpeed (int speed)
SetFocuserSpeed Set Focuser speed. More...

USED:
virtual IPState     MoveFocuser (FocusDirection dir, int speed, uint16_t duration)
MoveFocuser the focuser in a particular direction with a specific speed for a finite duration. More...

USED:
virtual IPState     MoveAbsFocuser (uint32_t targetTicks)
MoveFocuser the focuser to an absolute position. More...

USED:
virtual IPState     MoveRelFocuser (FocusDirection dir, uint32_t ticks)
MoveFocuser the focuser to an relative position. More...

USED:
virtual bool    AbortFocuser ()
AbortFocuser all focus motion. More...

*/


IPState LX200_OnStep::MoveFocuser(FocusDirection dir, int speed, uint16_t duration)
{
    INDI_UNUSED(speed);
    //  :FRsnnn#  Set focuser target position relative (in microns)
    //            Returns: Nothing
    double output;
    char read_buffer[32];
    output = duration;
    if (dir == FOCUS_INWARD) output = 0 - output;
    snprintf(read_buffer, sizeof(read_buffer), ":FR%5f#", output);
    sendOnStepCommandBlind(read_buffer);
    return IPS_BUSY; // Normal case, should be set to normal by update.
}

IPState LX200_OnStep::MoveAbsFocuser (uint32_t targetTicks)
{
    //  :FSsnnn#  Set focuser target position (in microns)
    //            Returns: Nothing
    if (FocusAbsPosNP[0].getMax() >= int(targetTicks) && FocusAbsPosNP[0].getMin() <= int(targetTicks))
    {
        char read_buffer[32];
        snprintf(read_buffer, sizeof(read_buffer), ":FS%06d#", int(targetTicks));
        sendOnStepCommandBlind(read_buffer);
        return IPS_BUSY; // Normal case, should be set to normal by update.
    }
    else
    {
        LOG_INFO("Unable to move focuser, out of range");
        return IPS_ALERT;
    }
}

IPState LX200_OnStep::MoveRelFocuser (FocusDirection dir, uint32_t ticks)
{
    //  :FRsnnn#  Set focuser target position relative (in microns)
    //            Returns: Nothing
    int output;
    char read_buffer[32];
    output = ticks;
    if (dir == FOCUS_INWARD) output = 0 - ticks;
    snprintf(read_buffer, sizeof(read_buffer), ":FR%04d#", output);
    sendOnStepCommandBlind(read_buffer);
    return IPS_BUSY; // Normal case, should be set to normal by update.
}

bool LX200_OnStep::AbortFocuser ()
{
    //  :FQ#   Stop the focuser
    //         Returns: Nothing
    char cmd[CMD_MAX_LEN] = {0};
    strncpy(cmd, ":FQ#", sizeof(cmd));
    return sendOnStepCommandBlind(cmd);
}

int LX200_OnStep::OSUpdateFocuser()
{

    //    double current = 0;
    //     int temp_value;
    //     int i;
    if (OSFocuser1)
    {
        // Alternate option:
        //if (!sendOnStepCommand(":FA#")) {
        char value[RB_MAX_LEN] = {0};
        int value_int;
        int error_or_fail = getCommandIntResponse(PortFD, &value_int, value, ":FG#");
        if (error_or_fail > 1)
        {
            FocusAbsPosNP[0].setValue( value_int);
            //         double current = FocusAbsPosNP[0].getValue();
            FocusAbsPosNP.apply();
            LOGF_DEBUG("Current focuser: %d, %f", value_int, FocusAbsPosNP[0].getValue());
        }

        //  :FT#  get status
        //         Returns: M# (for moving) or S# (for stopped)
        char valueStatus[RB_MAX_LEN] = {0};
        error_or_fail = getCommandSingleCharErrorOrLongResponse(PortFD, valueStatus, ":FT#");
        if (error_or_fail > 0 )
        {
            if (valueStatus[0] == 'S')
            {
                FocusRelPosNP.setState(IPS_OK);
                FocusRelPosNP.apply();
                FocusAbsPosNP.setState(IPS_OK);
                FocusAbsPosNP.apply();
            }
            else if (valueStatus[0] == 'M')
            {
                FocusRelPosNP.setState(IPS_BUSY);
                FocusRelPosNP.apply();
                FocusAbsPosNP.setState(IPS_BUSY);
                FocusAbsPosNP.apply();
            }
            else
            {
                LOG_WARN("Communication :FT# error, check connection.");
                //INVALID REPLY
                FocusRelPosNP.setState(IPS_ALERT);
                FocusRelPosNP.apply();
                FocusAbsPosNP.setState(IPS_ALERT);
                FocusAbsPosNP.apply();
            }
        }
        else
        {
            //INVALID REPLY
            LOG_WARN("Communication :FT# error, check connection.");
            FocusRelPosNP.setState(IPS_ALERT);
            FocusRelPosNP.apply();
            FocusAbsPosNP.setState(IPS_ALERT);
            FocusAbsPosNP.apply();
        }

        //  :FM#  Get max position (in microns)
        //         Returns: n#
        char focus_max[RB_MAX_LEN] = {0};
        int focus_max_int;
        int fm_error = getCommandIntResponse(PortFD, &focus_max_int, focus_max, ":FM#");
        if (fm_error > 0)
        {
            FocusAbsPosNP[0].setMax(focus_max_int);
            FocusAbsPosNP.updateMinMax();
            FocusAbsPosNP.apply();
            LOGF_DEBUG("focus_max: %s, %i, fm_nbchar: %i", focus_max, focus_max_int, fm_error);
        }
        else
        {
            LOG_WARN("Communication :FM# error, check connection.");
            LOGF_WARN("focus_max: %s, %u, fm_error: %i", focus_max, focus_max[0], fm_error);
            flushIO(PortFD); //Unlikely to do anything, but just in case.
        }

        //  :FI#  Get full in position (in microns)
        //         Returns: n#
        char focus_min[RB_MAX_LEN] = {0};
        int focus_min_int ;
        int fi_error = getCommandIntResponse(PortFD, &focus_min_int, focus_min, ":FI#");
        if (fi_error > 0)
        {
            FocusAbsPosNP[0].setMin( focus_min_int);
            FocusAbsPosNP.updateMinMax();
            FocusAbsPosNP.apply();
            LOGF_DEBUG("focus_min: %s, %i fi_nbchar: %i", focus_min, focus_min_int, fi_error);
        }
        else
        {
            LOG_WARN("Communication :FI# error, check connection.");
            flushIO(PortFD); //Unlikely to do anything, but just in case.
        }

        //  :Ft#    Get Focuser Temperature
        //          Returns: n#
        char focus_T[RB_MAX_LEN] = {0};
        double focus_T_double ;
        int ft_error = getCommandDoubleResponse(PortFD, &focus_T_double, focus_T, ":Ft#");
        if (ft_error > 0)
        {
            FocusTemperatureN[0].value = atof(focus_T);
            IDSetNumber(&FocusTemperatureNP, nullptr);
            LOGF_DEBUG("focus T°: %s, focus_T_double %i ft_nbcar: %i", focus_T, focus_T_double, ft_error);
        }
        else
        {
            LOG_WARN("Communication :Ft# error, check connection.");
            LOGF_DEBUG("focus T°: %s, focus_T_double %i ft_nbcar: %i", focus_T, focus_T_double, ft_error);
            flushIO(PortFD); //Unlikely to do anything, but just in case.
        }

        //  :Fe#    Get Focus Differential T°
        //          Returns: n#
        char focus_TD[RB_MAX_LEN] = {0};
        int focus_TD_int ;
        int fe_error = getCommandIntResponse(PortFD, &focus_TD_int, focus_TD, ":Fe#");
        if (fe_error > 0)
        {
            FocusTemperatureN[1].value =  atof(focus_TD);
            IDSetNumber(&FocusTemperatureNP, nullptr);
            LOGF_DEBUG("focus Differential T°: %s, %i fi_nbchar: %i", focus_TD, focus_TD_int, fe_error);
        }
        else
        {
            LOG_WARN("Communication :Fe# error, check connection.");
            flushIO(PortFD); //Unlikely to do anything, but just in case.
        }

        // :FC#       Get focuser temperature compensation coefficient in microns per °C)
        //            Return: n.n#
        char focus_Coeficient[RB_MAX_LEN] = {0};
        int focus_Coefficient_int ;
        int fC_error = getCommandIntResponse(PortFD, &focus_Coefficient_int, focus_Coeficient, ":FC#");
        if (fC_error > 0)
        {
            TFCCoefficientN[0].value =  atof(focus_Coeficient);
            IDSetNumber(&TFCCoefficientNP, nullptr);
            LOGF_DEBUG("TFC Coefficient: %s, %i fC_nbchar: %i", focus_Coeficient, focus_Coefficient_int, fC_error);
        }
        else
        {
            LOG_WARN("Communication :FC# error, check connection.");
            flushIO(PortFD); //Unlikely to do anything, but just in case.
        }

        // :FD#       Get focuser temperature compensation deadband amount (in steps or microns)
        //            Return: n#
        char focus_Deadband[RB_MAX_LEN] = {0};
        int focus_Deadband_int ;
        int fD_error = getCommandIntResponse(PortFD, &focus_Deadband_int, focus_Deadband, ":FD#");
        if (fD_error > 0)
        {
            TFCDeadbandN[0].value =  focus_Deadband_int;
            IDSetNumber(&TFCDeadbandNP, nullptr);
            LOGF_DEBUG("TFC Deadband: %s, %i fD_nbchar: %i", focus_Deadband, focus_Deadband_int, fD_error);
        }
        else
        {
            LOG_WARN("Communication :FD# error, check connection.");
            flushIO(PortFD); //Unlikely to do anything, but just in case.
        }

        // :FC#       Get focuser temperature compensation coefficient in microns per °C)
        //            Return: n.n#
        char response[RB_MAX_LEN];
        int res = getCommandSingleCharResponse(PortFD, response, ":Fc#");
        if (res > 0)
        {
            if (strcmp(response, "0"))
            {
                TFCCompensationSP.s = IPS_OK;
                TFCCompensationS[0].s = ISS_OFF;
                TFCCompensationS[1].s = ISS_ON;
            }
            else if (strcmp(response, "1"))
            {
                TFCCompensationSP.s = IPS_OK;
                TFCCompensationS[0].s = ISS_ON;
                TFCCompensationS[1].s = ISS_OFF;
            }
            IDSetSwitch(&TFCCompensationSP, nullptr);
            LOGF_DEBUG("TFC Enable: fc_nbchar:%d Fc_response: %s", res, response);
        }
        else
        {
            //LOGF_DEBUG("TFC Enable1: fc_error:%i Fc_response: %s", res, response);
            LOG_WARN("Communication :Fc# error, check connection.");
            flushIO(PortFD); //Unlikely to do anything, but just in case.
        }

        FI::updateProperties();
        LOGF_DEBUG("After update properties: FocusAbsPosN min: %f max: %f", FocusAbsPosNP[0].getMin(), FocusAbsPosNP[0].getMax());
    }

    if(OSFocuser2)
    {
        char value[RB_MAX_LEN] = {0};
        int error_return;
        //TODO: Check to see if getCommandIntResponse would be better
        error_return = getCommandSingleCharErrorOrLongResponse(PortFD, value, ":fG#");
        if (error_return >= 0)
        {
            if ( strcmp(value, "0") )
            {
                LOG_INFO("Focuser 2 called, but not present, disabling polling");
                LOGF_DEBUG("OSFocuser2: %d, OSNumFocusers: %i", OSFocuser2, OSNumFocusers);
                OSFocuser2 = false;
            }
            else
            {
                OSFocus2TargNP.np[0].value = atoi(value);
                IDSetNumber(&OSFocus2TargNP, nullptr);
            }
        }
        else
        {
            LOGF_INFO("Focuser 2 called, but returned error %i on read, disabling further polling", error_return);
            LOGF_DEBUG("OSFocuser2: %d, OSNumFocusers: %i", OSFocuser2, OSNumFocusers);
            OSFocuser2 = false;
        }
    }

    if(OSNumFocusers > 1)
    {
        char value[RB_MAX_LEN] = {0};
        int error_or_fail = getCommandSingleCharResponse(PortFD, value, ":Fa#"); //0 = failure, 1 = success, no # on reply
        if (error_or_fail > 0 && value[0] > '0' && value[0] < '9')
        {
            int temp_value = (unsigned int)(value[0]) - '0';
            LOGF_DEBUG(":Fa# return: %d", temp_value);
            for (int i = 0; i < 9; i++)
            {
                OSFocusSelectS[i].s = ISS_OFF;
            }
            if (temp_value == 0)
            {
                OSFocusSelectS[1].s = ISS_ON;
            }
            else if (temp_value > 9 || temp_value < 0) //TODO: Check if completely redundant
            {
                //To solve issue mentioned https://www.indilib.org/forum/development/1406-driver-onstep-lx200-like-for-indi.html?start=624#71572
                OSFocusSelectSP.s = IPS_ALERT;
                LOGF_WARN("Active focuser returned out of range: %s, should be 0-9", temp_value);
                IDSetSwitch(&OSFocusSelectSP, nullptr);
                return 1;
            }
            else
            {
                OSFocusSelectS[temp_value - 1].s = ISS_ON;
            }
            OSFocusSelectSP.s = IPS_OK;
            IDSetSwitch(&OSFocusSelectSP, nullptr);
        }
        else
        {
            LOGF_DEBUG(":Fa# returned outside values: %c, %u", value[0], value[0]);
        }
    }
    return 0;
}


//Rotator stuff
// IPState MoveRotator(double angle) override;
// bool SyncRotator(double angle) override;
//         IPState HomeRotator() override;
// bool ReverseRotator(bool enabled) override;
// bool AbortRotator() override;
//         bool SetRotatorBacklash (int32_t steps) override;
//         bool SetRotatorBacklashEnabled(bool enabled) override;

//OnStep Rotator Commands (For reference, and from 5 1 v 4)
// :r+#       Enable derotator
//            Returns: Nothing
// :r-#       Disable derotator
//            Returns: Nothing
// :rP#       Move rotator to the parallactic angle
//            Returns: Nothing
// :rR#       Reverse derotator direction
//            Returns: Nothing
// :rT#       Get status
//            Returns: M# (for moving) or S# (for stopped)
// :rI#       Get mIn position (in degrees)
//            Returns: n#
// :rM#       Get Max position (in degrees)
//            Returns: n#
// :rD#       Get rotator degrees per step
//            Returns: n.n#
// :rb#       Get rotator backlash amount in steps
//            Return: n#
// :rb[n]#
//            Set rotator backlash amount in steps
//            Returns: 0 on failure
//                     1 on success
// :rF#       Reset rotator at the home position
//            Returns: Nothing
// :rC#       Moves rotator to the home position
//            Returns: Nothing
// :rG#       Get rotator current position in degrees
//            Returns: sDDD*MM#
// :rc#       Set continuous move mode (for next move command)
//            Returns: Nothing
// :r>#       Move clockwise as set by :rn# command, default = 1 deg (or 0.1 deg/s in continuous mode)
//            Returns: Nothing
// :r<#       Move counter clockwise as set by :rn# command
//            Returns: Nothing
// :rQ#       Stops movement (except derotator)
//            Returns: Nothing
// :r[n]#     Move increment where n = 1 for 1 degrees, 2 for 2 degrees, 3 for 5 degrees, 4 for 10 degrees
//            Move rate where n = 1 for .01 deg/s, 2 for 0.1 deg/s, 3 for 1.0 deg/s, 4 for 5.0 deg/s
//            Returns: Nothing
// :rS[sDDD*MM'SS]#
//            Set position in degrees
//            Returns: 0 on failure
//                     1 on success

int LX200_OnStep::OSUpdateRotator()
{
    char value[RB_MAX_LEN];
    double double_value;
    if(OSRotator1)
    {
        int error_or_fail = getCommandSingleCharErrorOrLongResponse(PortFD, value, ":rG#");
        if (error_or_fail == 1 && value[0] == '0') //1 char return, response 0 = no Rotator
        {
            LOG_INFO("Detected Response that Rotator is not present, disabling further checks");
            OSRotator1 = false;
            return 0; //Return 0, as this is not a communication error
        }
        if (error_or_fail < 1)   //This does not necessarily mean
        {
            LOG_WARN("Error talking to rotator, might be timeout (especially on network)");
            return -1;
        }
        if (f_scansexa(value, &double_value))
        {
            // 0 = good, thus this is the bad
            GotoRotatorNP.setState(IPS_ALERT);
            GotoRotatorNP.apply();
            return -1;
        }
        GotoRotatorNP[0].setValue(double_value);
        double min_rotator, max_rotator;
        //NOTE: The following commands are only on V4, V5 & OnStepX, not V3
        //TODO: Psudo-state for V3 Rotator?
        bool changed_minmax = false;
        if (OnStepMountVersion != OSV_OnStepV1or2 && OnStepMountVersion != OSV_OnStepV3)
        {
            memset(value, 0, RB_MAX_LEN);
            error_or_fail = getCommandDoubleResponse(PortFD, &min_rotator, value, ":rI#");
            if (error_or_fail > 1)
            {
                changed_minmax = true;
                GotoRotatorNP[0].setMin(min_rotator);
            }
            memset(value, 0, RB_MAX_LEN);
            error_or_fail = getCommandDoubleResponse(PortFD, &max_rotator, value, ":rM#");
            if (error_or_fail > 1)
            {
                changed_minmax = true;
                GotoRotatorNP[0].setMax(max_rotator);
            }
            if (changed_minmax)
            {
                GotoRotatorNP.updateMinMax();
                GotoRotatorNP.apply();
            }
            //GotoRotatorN
            memset(value, 0, RB_MAX_LEN);
            error_or_fail = getCommandSingleCharErrorOrLongResponse(PortFD, value, ":rT#");
            if (error_or_fail > 1)
            {
                if (value[0] == 'S') /*Stopped normal on EQ mounts */
                {
                    GotoRotatorNP.setState(IPS_OK);
                    GotoRotatorNP.apply();

                }
                else if (value[0] == 'M') /* Moving, including de-rotation */
                {
                    GotoRotatorNP.setState(IPS_BUSY);
                    GotoRotatorNP.apply();
                }
                else
                {
                    //INVALID REPLY
                    GotoRotatorNP.setState(IPS_ALERT);
                    GotoRotatorNP.apply();
                }
            }
            memset(value, 0, RB_MAX_LEN);
            int backlash_value;
            error_or_fail = getCommandIntResponse(PortFD, &backlash_value, value, ":rb#");
            if (error_or_fail > 1)
            {
                RotatorBacklashNP[0].setValue(backlash_value);
                RotatorBacklashNP.setState(IPS_OK);
                RotatorBacklashNP.apply();
            }
        }
    }
    return 0;
}

IPState LX200_OnStep::MoveRotator(double angle)
{
    char cmd[CMD_MAX_LEN] = {0};
    int d, m, s;
    getSexComponents(angle, &d, &m, &s);

    snprintf(cmd, sizeof(cmd), ":rS%.03d:%02d:%02d#", d, m, s);
    LOGF_INFO("Move Rotator: %s", cmd);


    if(setStandardProcedure(PortFD, cmd))
    {
        return IPS_BUSY;
    }
    else
    {
        return IPS_ALERT;
    }


    return IPS_BUSY;
}
/*
bool LX200_OnStep::SyncRotator(double angle) {

}*/
IPState LX200_OnStep::HomeRotator()
{
    //Not entirely sure if this means attempt to use limit switches and home, or goto home
    //Assuming MOVE to Home
    LOG_INFO("Moving Rotator to Home");
    sendOnStepCommandBlind(":rC#");
    return IPS_BUSY;
}
// bool LX200_OnStep::ReverseRotator(bool enabled) {
//     sendOnStepCommandBlind(":rR#");
//     return true;
// } //No way to check which way it's going as Indi expects

bool LX200_OnStep::AbortRotator()
{
    LOG_INFO("Aborting Rotation, de-rotation in same state");
    sendOnStepCommandBlind(":rQ#"); //Does NOT abort de-rotator
    return true;
}

bool LX200_OnStep::SetRotatorBacklash(int32_t steps)
{
    char cmd[CMD_MAX_LEN] = {0};
    //     char response[RB_MAX_LEN];
    snprintf(cmd, sizeof(cmd), ":rb%d#", steps);
    if(sendOnStepCommand(cmd))
    {
        return true;
    }
    return false;
}

bool LX200_OnStep::SetRotatorBacklashEnabled(bool enabled)
{
    //Nothing required here.
    INDI_UNUSED(enabled);
    return true;
    //     As it's always enabled, which would mean setting it like SetRotatorBacklash to 0, and losing any saved values. So for now, leave it as is (always enabled)
}

// bool SyncRotator(double angle) override;
//         IPState HomeRotator(double angle) override;
// bool ReverseRotator(bool enabled) override;
// bool AbortRotator() override;
//         bool SetRotatorBacklash (int32_t steps) override;
//         bool SetRotatorBacklashEnabled(bool enabled) override;

// Now, derotation is NOT explicitly handled.



//End Rotator stuff




//PEC Support
//Should probably be added to inditelescope or another interface, because the PEC that's there... is very limited.

IPState LX200_OnStep::StartPECPlayback (int axis)
{
    //  :$QZ+  Enable RA PEC compensation
    //         Returns: nothing
    INDI_UNUSED(axis); //We only have RA on OnStep
    if (OSMountType != MOUNTTYPE_ALTAZ )
    {
        if (OSPECEnabled == true)
        {
            char cmd[CMD_MAX_LEN] = {0};
            LOG_INFO("Sending Command to Start PEC Playback");
            strncpy(cmd, ":$QZ+#", sizeof(cmd));
            sendOnStepCommandBlind(cmd);
            return IPS_BUSY;
        }
        else
        {
            LOG_DEBUG("Command to Playback PEC called when Controller does not support PEC");
        }
        return IPS_ALERT;
    }
    else
    {
        OSPECEnabled = false;
        LOG_INFO("Command to Start Playback PEC called when Controller does not support PEC due to being Alt-Az, PEC Ignored going forward");
        return IPS_ALERT;
    }

}

IPState LX200_OnStep::StopPECPlayback (int axis)
{
    //  :$QZ-  Disable RA PEC Compensation
    //         Returns: nothing
    INDI_UNUSED(axis); //We only have RA on OnStep
    if (OSPECEnabled == true)
    {
        char cmd[CMD_MAX_LEN] = {0};
        LOG_INFO("Sending Command to Stop PEC Playback");
        strncpy(cmd, ":$QZ-#", sizeof(cmd));
        sendOnStepCommandBlind(cmd);
        return IPS_BUSY;
    }
    else
    {
        LOG_DEBUG("Command to Stop Playing PEC called when Controller does not support PEC");
    }
    return IPS_ALERT;
}

IPState LX200_OnStep::StartPECRecord (int axis)
{
    //  :$QZ/  Ready Record PEC
    //         Returns: nothing
    INDI_UNUSED(axis); //We only have RA on OnStep
    if (OSPECEnabled == true)
    {
        char cmd[CMD_MAX_LEN] = {0};
        LOG_INFO("Sending Command to Start PEC record");
        strncpy(cmd, ":$QZ/#", CMD_MAX_LEN);
        sendOnStepCommandBlind(cmd);
        return IPS_BUSY;
    }
    else
    {
        LOG_DEBUG("Command to Record PEC called when Controller does not support PEC");
    }
    return IPS_ALERT;
}

IPState LX200_OnStep::ClearPECBuffer (int axis)
{
    //  :$QZZ  Clear the PEC data buffer
    //         Return: Nothing
    INDI_UNUSED(axis); //We only have RA on OnStep
    if (OSPECEnabled == true)
    {
        char cmd[CMD_MAX_LEN] = {0};
        LOG_INFO("Sending Command to Clear PEC record");
        strncpy(cmd, ":$QZZ#", CMD_MAX_LEN);
        sendOnStepCommandBlind(cmd);
        return IPS_BUSY;
    }
    else
    {
        LOG_DEBUG("Command to clear PEC called when Controller does not support PEC");
    }
    return IPS_ALERT;

}

IPState LX200_OnStep::SavePECBuffer (int axis)
{
    //  :$QZ!  Write PEC data to EEPROM
    //         Returns: nothing
    INDI_UNUSED(axis); //We only have RA on OnStep
    if (OSPECEnabled == true)
    {
        char cmd[CMD_MAX_LEN] = {0};
        LOG_INFO("Sending Command to Save PEC to EEPROM");
        strncpy(cmd, ":$QZ!#", CMD_MAX_LEN);
        sendOnStepCommandBlind(cmd);
        return IPS_BUSY;
    }
    else
    {
        LOG_DEBUG("Command to save PEC called when Controller does not support PEC");
    }
    return IPS_ALERT;
}


IPState LX200_OnStep::PECStatus (int axis)
{
    //     if (!OSPECviaGU) {
    INDI_UNUSED(axis); //We only have RA on OnStep
    if (OSPECEnabled == true && OSPECviaGU == false) //All current versions report via #GU
    {
        if (OSMountType == MOUNTTYPE_ALTAZ || OSMountType == MOUNTTYPE_FORK_ALT)
        {
            OSPECEnabled = false;
            LOG_INFO("Command to give PEC called when Controller does not support PEC due to being Alt-Az Disabled");
            return IPS_ALERT;
        }
        //LOG_INFO("Getting PEC Status");
        //  :$QZ?  Get PEC status
        //         Returns: S#
        // Returns status (pecSense) In the form: Status is one of "IpPrR" (I)gnore, get ready to (p)lay, (P)laying, get ready to (r)ecord, (R)ecording.  Or an optional (.) to indicate an index detect.
        // IUFillSwitch(&OSPECStatusS[0], "OFF", "OFF", ISS_ON);
        // IUFillSwitch(&OSPECStatusS[1], "Playing", "Playing", ISS_OFF);
        // IUFillSwitch(&OSPECStatusS[2], "Recording", "Recording", ISS_OFF);
        // IUFillSwitch(&OSPECStatusS[3], "Will Play", "Will Play", ISS_OFF);
        // IUFillSwitch(&OSPECStatusS[4], "Will Record", "Will Record", ISS_OFF);
        char value[RB_MAX_LEN] = {0};
        OSPECStatusSP.s = IPS_BUSY;
        int error_or_fail = getCommandSingleCharErrorOrLongResponse(PortFD, value, ":$QZ?#");
        if (error_or_fail > 1)
        {
            OSPECStatusS[0].s = ISS_OFF ;
            OSPECStatusS[1].s = ISS_OFF ;
            OSPECStatusS[2].s = ISS_OFF ;
            OSPECStatusS[3].s = ISS_OFF ;
            OSPECStatusS[4].s = ISS_OFF ;
            if (value[0] == 'I') //Ignore
            {
                OSPECStatusSP.s = IPS_OK;
                OSPECStatusS[0].s = ISS_ON ;
                OSPECRecordSP.s = IPS_IDLE;
                // 		OSPECEnabled = false;
                LOG_INFO("Controller reports PEC Ignored and not supported");
                LOG_INFO("No Further PEC Commands will be processed, unless status changed");
            }
            else if (value[0] == 'R') //Active Recording
            {
                OSPECStatusSP.s = IPS_OK;
                OSPECStatusS[2].s = ISS_ON ;
                OSPECRecordSP.s = IPS_BUSY;
            }
            else if (value[0] == 'r')  //Waiting for index before recording
            {
                OSPECStatusSP.s = IPS_OK;
                OSPECStatusS[4].s = ISS_ON ;
                OSPECRecordSP.s = IPS_BUSY;
            }
            else if (value[0] == 'P') //Active Playing
            {
                OSPECStatusSP.s = IPS_BUSY;
                OSPECStatusS[1].s = ISS_ON ;
                OSPECRecordSP.s = IPS_IDLE;
            }
            else if (value[0] == 'p') //Waiting for index before playing
            {
                OSPECStatusSP.s = IPS_BUSY;
                OSPECStatusS[3].s = ISS_ON ;
                OSPECRecordSP.s = IPS_IDLE;
            }
            else //INVALID REPLY
            {
                OSPECStatusSP.s = IPS_ALERT;
                OSPECRecordSP.s = IPS_ALERT;
            }
            if (value[1] == '.')
            {
                OSPECIndexSP.s = IPS_OK;
                OSPECIndexS[0].s = ISS_OFF;
                OSPECIndexS[1].s = ISS_ON;
            }
            else
            {
                OSPECIndexS[1].s = ISS_OFF;
                OSPECIndexS[0].s = ISS_ON;
            }
            IDSetSwitch(&OSPECStatusSP, nullptr);
            IDSetSwitch(&OSPECRecordSP, nullptr);
            IDSetSwitch(&OSPECIndexSP, nullptr);
            return IPS_OK;
        }
        else
        {
            LOG_DEBUG("Timeout or other error on :$QZ?#");
        }
    }
    else
    {
        // LOG_DEBUG("PEC status called when Controller does not support PEC");
    }
    return IPS_ALERT;
}


IPState LX200_OnStep::ReadPECBuffer (int axis)
{
    INDI_UNUSED(axis); //We only have RA on OnStep
    if (OSPECEnabled == true)
    {
        LOG_WARN("PEC Reading NOT Implemented");
        return IPS_OK;
    }
    else
    {
        LOG_DEBUG("Command to Read PEC called when Controller does not support PEC");
    }
    return IPS_ALERT;
}


IPState LX200_OnStep::WritePECBuffer (int axis)
{
    INDI_UNUSED(axis); //We only have RA on OnStep
    if (OSPECEnabled == true)
    {
        LOG_WARN("PEC Writing NOT Implemented");
        return IPS_OK;
    }
    else
    {
        LOG_DEBUG("Command to Read PEC called when Controller does not support PEC");
    }
    return IPS_ALERT;
}

// New, Multistar alignment goes here:

IPState LX200_OnStep::AlignStartGeometric (int stars)
{
    //See here https://groups.io/g/onstep/message/3624
    char cmd[CMD_MAX_LEN] = {0};

    LOG_INFO("Sending Command to Start Alignment");
    IUSaveText(&OSNAlignT[0], "Align STARTED");
    IUSaveText(&OSNAlignT[1], "GOTO a star, center it");
    IUSaveText(&OSNAlignT[2], "GOTO a star, Solve and Sync");
    IUSaveText(&OSNAlignT[3], "Press 'Issue Align' if not solving");
    IDSetText(&OSNAlignTP, "==>Align Started");
    // Check for max number of stars and gracefully fall back to max, if more are requested.
    char read_buffer[RB_MAX_LEN] = {0};
    int error_or_fail = getCommandSingleCharErrorOrLongResponse(PortFD, read_buffer, ":A?#");
    if(error_or_fail != 4 || read_buffer[0] < '0' || read_buffer[0] > '9' || read_buffer[1] < '0' || read_buffer[1] > ':'
            || read_buffer[2] < '0' || read_buffer[2] > '9')
    {
        LOGF_INFO("Getting Alignment Status: response Error, response = %s>", read_buffer);
        return IPS_ALERT;
    }
    //Check max_stars

    int max_stars = read_buffer[0] - '0';
    if (stars > max_stars)
    {
        LOG_INFO("Tried to start Align with too many stars.");
        LOGF_INFO("Starting Align with %d stars", max_stars);
        stars = max_stars;
    }
    snprintf(cmd, sizeof(cmd), ":A%.1d#", stars);
    LOGF_INFO("Started Align with %s, max possible stars: %d", cmd, max_stars);
    if(sendOnStepCommand(cmd))
    {
        LOG_INFO("Starting Align failed");
        return IPS_BUSY;
    }
    return IPS_ALERT;
}


IPState LX200_OnStep::AlignAddStar ()
{
    //Used if centering a star manually, most will use plate-solving
    //See here https://groups.io/g/onstep/message/3624
    char cmd[CMD_MAX_LEN] = {0};
    LOG_INFO("Sending Command to Record Star");
    strncpy(cmd, ":A+#", sizeof(cmd));
    if(sendOnStepCommand(cmd))
    {
        LOG_INFO("Adding Align failed");
        return IPS_BUSY;
    }
    return IPS_ALERT;
}

bool LX200_OnStep::UpdateAlignStatus ()
{
    //  :A?#  Align status
    //         Returns: mno#
    //         where m is the maximum number of alignment stars
    //               n is the current alignment star (0 otherwise) or ':' in case 9 stars selected
    //               o is the last required alignment star when an alignment is in progress (0 otherwise)

    char msg[40] = {0};
    char stars[5] = {0};
    int max_stars, current_star, align_stars;

    char read_buffer[RB_MAX_LEN] = {0};
    int error_or_fail = getCommandSingleCharErrorOrLongResponse(PortFD, read_buffer, ":A?#");
    if(error_or_fail != 4 || read_buffer[0] < '0' || read_buffer[0] > '9' || read_buffer[1] < '0' || read_buffer[1] > ':'
            || read_buffer[2] < '0' || read_buffer[2] > '9')
    {
        LOGF_INFO("Getting Alignment Status: response Error, response = %s>", read_buffer);
        return false;
    }
    max_stars = read_buffer[0] - '0';
    current_star = read_buffer[1] - '0';
    align_stars = read_buffer[2] - '0';
    snprintf(stars, sizeof(stars), "%d", max_stars);
    IUSaveText(&OSNAlignT[5], stars);
    snprintf(stars, sizeof(stars), "%d", current_star);
    if (read_buffer[1] > '9')
    {
        IUSaveText(&OSNAlignT[6], ":");
    }
    else
    {
        IUSaveText(&OSNAlignT[6], stars);
    }
    snprintf(stars, sizeof(stars), "%d", align_stars);
    IUSaveText(&OSNAlignT[7], stars);
    LOGF_DEBUG("Align: max_stars: %i current star: %u, align_stars %u", max_stars, current_star, align_stars);

    if (current_star <= align_stars)
    {
        snprintf(msg, sizeof(msg), "%s Alignment: Star %d/%d", read_buffer, current_star, align_stars );
        IUSaveText(&OSNAlignT[4], msg);
    }
    if (current_star > align_stars && max_stars > 1)
    {
        LOGF_DEBUG("Align: current star: %u, align_stars %u", int(current_star), int(align_stars));
        snprintf(msg, sizeof(msg), "Align: Completed");
        AlignDone();
        IUSaveText(&OSNAlignT[4], msg);
        UpdateAlignErr();
    }
    IDSetText(&OSNAlignTP, nullptr);
    return true;
}

bool LX200_OnStep::UpdateAlignErr()
{
    //  :GX0n#   Get OnStep value
    //         Returns: value

    // 00 ax1Cor
    // 01 ax2Cor
    // 02 altCor
    // 03 azmCor
    // 04 doCor
    // 05 pdCor
    // 06 ffCor
    // 07 dfCor
    // 08 tfCor
    // 09 Number of stars, reset to first star
    // 0A Star  #n HA
    // 0B Star  #n Dec
    // 0C Mount #n HA
    // 0D Mount #n Dec
    // 0E Mount PierSide (and increment n)



    char read_buffer[RB_MAX_LEN] = {0};
    char polar_error[RB_MAX_LEN] = {0};
    char sexabuf[RB_MAX_LEN] = {0};
    //  IUFillText(&OSNAlignT[4], "4", "Current Status", "Not Updated");
    //  IUFillText(&OSNAlignT[5], "5", "Max Stars", "Not Updated");
    //  IUFillText(&OSNAlignT[6], "6", "Current Star", "Not Updated");
    //  IUFillText(&OSNAlignT[7], "7", "# of Align Stars", "Not Updated");

    // LOG_INFO("Getting Align Error Status");
    int error_or_fail;
    double altCor, azmCor;
    error_or_fail = getCommandDoubleResponse(PortFD, &altCor, read_buffer, ":GX02#");
    if (error_or_fail < 2)
    {
        LOGF_INFO("Polar Align Error Status response Error, response = %s>", read_buffer);
        return false;
    }
    error_or_fail = getCommandDoubleResponse(PortFD, &azmCor, read_buffer, ":GX03#");
    if (error_or_fail < 2)
    {
        LOGF_INFO("Polar Align Error Status response Error, response = %s>", read_buffer);
        return false;
    }
    fs_sexa(sexabuf, (double)azmCor / 3600, 4, 3600);
    snprintf(polar_error, sizeof(polar_error), "%f'' /%s", azmCor, sexabuf);
    IUSaveText(&OSNAlignErrT[1], polar_error);
    fs_sexa(sexabuf, (double)altCor / 3600, 4, 3600);
    snprintf(polar_error, sizeof(polar_error), "%f'' /%s", altCor, sexabuf);
    IUSaveText(&OSNAlignErrT[0], polar_error);
    IDSetText(&OSNAlignErrTP, nullptr);


    return true;
}

IPState LX200_OnStep::AlignDone()
{
    //See here https://groups.io/g/onstep/message/3624
    if (OSAlignCompleted == false)
    {
        OSAlignCompleted = true;
        LOG_INFO("Alignment Done - May still be calculating");
        IUSaveText(&OSNAlignT[0], "Align FINISHED");
        IUSaveText(&OSNAlignT[1], "------");
        IUSaveText(&OSNAlignT[2], "Optionally press:");
        IUSaveText(&OSNAlignT[3], "Write Align to NVRAM/Flash ");
        IDSetText(&OSNAlignTP, nullptr);
        return IPS_OK;
    }
    return IPS_BUSY;
}

IPState LX200_OnStep::AlignWrite()
{
    //See here https://groups.io/g/onstep/message/3624
    char cmd[CMD_MAX_LEN] = {0};
    char response[RB_MAX_LEN];

    LOG_INFO("Sending Command to Finish Alignment and write");
    strncpy(cmd, ":AW#", sizeof(cmd));
    int res = getCommandSingleCharResponse(PortFD, response, cmd); //1 success , no # on reply
    if(res > 0 && response[0] == '1')
    {
        LOG_INFO("Align Write Successful");
        UpdateAlignStatus();
        IUSaveText(&OSNAlignT[0], "Align FINISHED");
        IUSaveText(&OSNAlignT[1], "------");
        IUSaveText(&OSNAlignT[2], "And Written to EEPROM");
        IUSaveText(&OSNAlignT[3], "------");
        IDSetText(&OSNAlignTP, nullptr);
        return IPS_OK;
    }
    else
    {
        LOGF_ERROR("Align Write Failed: error=%s", response);
        UpdateAlignStatus();
        IUSaveText(&OSNAlignT[0], "Align WRITE FAILED");
        IDSetText(&OSNAlignTP, nullptr);
        return IPS_ALERT;
    }
}

#ifdef ONSTEP_NOTDONE
IPState LX200_OnStep::OSEnableOutput(int output)
{
    //  :SXnn,VVVVVV...#   Set OnStep value
    //          Return: 0 on failure
    //                  1 on success
    // if (parameter[0]=='G') { // Gn: General purpose output
    // :SXGn,value
    // value, 0 = low, other = high
    LOG_INFO("Not implemented yet");
    return IPS_OK;
}
#endif

IPState LX200_OnStep::OSDisableOutput(int output)
{
    LOG_INFO("Not implemented yet");
    OSGetOutputState(output);
    return IPS_OK;
}

/*
Reference:
    //  :GXnn#   Get OnStep value
    //         Returns: value
    //         Error = 123456789
    //
    // Double unless noted: integer:i, special:* and values in {}
    //
    //   00 ax1Cor
    //   01 ax2Cor
    //   02 altCor  //EQ Altitude Correction
    //   03 azmCor  //EQ Azimuth Correction
    //   04 doCor
    //   05 pdCor
    //   06 ffCor
    //   07 dfCor
    //   08 tfCor
    //   09 Number of stars, reset to first star
    //   0A Star  #n HA
    //   0B Star  #n Dec
    //   0C Mount #n HA
    //   0D Mount #n Dec
    //   0E Mount PierSide (and increment n)
    //   80 UTC time
    //   81 UTC date
    //   90 pulse-guide rate
    // i 91 pec analog value
    //   92 MaxRate
    //   93 MaxRate (default) number
    // * 94 pierSide (N if never) {Same as :Gm# (E, W, None)}
    // i 95 autoMeridianFlip AutoFlip setting {0/1+}
    // * 96 preferred pier side {E, W, B}
    //   97 slew speed
    // * 98 rotator {D, R, N}
    //   9A temperature in deg. C
    //   9B pressure in mb
    //   9C relative humidity in %
    //   9D altitude in meters
    //   9E dew point in deg. C
    //   9F internal MCU temperature in deg. C
    // * Un: Get stepper driver statUs
    //   En: Get settings
    //   Fn: Debug
    //   G0-GF (HEX!) = Onstep output status

*/

bool LX200_OnStep::OSGetOutputState(int output)
{
    //  :GXnn#   Get OnStep value
    //         Returns: value
    // nn= G0-GF (HEX!) - Output status
    //
    char value[RB_MAX_LEN] = {0};
    char command[CMD_MAX_LEN] = {0};
    strncpy(command, ":$GXGm#", CMD_MAX_LEN);
    LOGF_INFO("Output: %s", char(output));
    LOGF_INFO("Command: %s", command);
    command[5] = char(output);
    LOGF_INFO("Command: %s", command);

    int error_or_fail = getCommandSingleCharErrorOrLongResponse(PortFD, value, command);
    if (error_or_fail > 0)
    {
        if (value[0] == 0)
        {
            OSOutput1S[0].s = ISS_ON;
            OSOutput1S[1].s = ISS_OFF;
        }
        else
        {
            OSOutput1S[0].s = ISS_OFF;
            OSOutput1S[1].s = ISS_ON;
        }
        IDSetSwitch(&OSOutput1SP, nullptr);
        return true;
    }
    return false;
}

bool LX200_OnStep::SetTrackRate(double raRate, double deRate)
{
    char read_buffer[RB_MAX_LEN];
    snprintf(read_buffer, sizeof(read_buffer), ":RA%04f#", raRate);
    LOGF_INFO("Setting: Custom RA Rate to %04f", raRate);
    if (!sendOnStepCommand(read_buffer))
    {
        return false;
    }
    snprintf(read_buffer, sizeof(read_buffer), ":RE%04f#", deRate);
    LOGF_INFO("Setting: Custom DE Rate to %04f", deRate);
    if (!sendOnStepCommand(read_buffer))
    {
        return false;
    }
    LOG_INFO("Custom RA and DE Rates successfully set");
    return true;
}

void LX200_OnStep::slewError(int slewCode)
{
    //         0=Goto is possible
    //         1=below the horizon limit
    //         2=above overhead limit
    //         3=controller in standby
    //         4=mount is parked
    //         5=Goto in progress
    //         6=outside limits (MaxDec, MinDec, UnderPoleLimit, MeridianLimit)
    //         7=hardware fault
    //         8=already in motion
    //         9=unspecified error
    switch(slewCode)
    {
        case 0:
            LOG_ERROR("OnStep slew/syncError called with value 0-goto possible, this is normal operation");
            return;
        case 1:
            LOG_ERROR("OnStep slew/syncError: Below the horizon limit");
            break;
        case 2:
            LOG_ERROR("OnStep slew/syncError: Above Overhead limit");
            break;
        case 3:
            LOG_ERROR("OnStep slew/syncError: Controller in standby, Usual issue fix: Turn tracking on");
            break;
        case 4:
            LOG_ERROR("OnStep slew/syncError: Mount is Parked");
            break;
        case 5:
            LOG_ERROR("OnStep slew/syncError: Goto in progress");
            break;
        case 6:
            LOG_ERROR("OnStep slew/syncError: Outside limits: Max/Min Dec, Under Pole Limit, Meridian Limit, Sync attempted to wrong pier side");
            break;
        case 7:
            LOG_ERROR("OnStep slew/syncError: Hardware Fault");
            break;
        case 8:
            LOG_ERROR("OnStep slew/syncError: Already in motion");
            break;
        case 9:
            LOG_ERROR("OnStep slew/syncError: Unspecified Error");
            break;
        default:
            LOG_ERROR("OnStep slew/syncError: Not in range of values that should be returned! INVALID, Something went wrong!");
    }
    EqNP.setState(IPS_ALERT);
    EqNP.apply();
}


//Override LX200 sync function, to allow for error returns
bool LX200_OnStep::Sync(double ra, double dec)
{

    char read_buffer[RB_MAX_LEN] = {0};
    //     int error_code;

    if (!isSimulation())
    {
        if (setObjectRA(PortFD, ra) < 0 || (setObjectDEC(PortFD, dec)) < 0)
        {
            EqNP.setState(IPS_ALERT);
            LOG_ERROR("Error setting RA/DEC. Unable to Sync.");
            EqNP.apply();
            return false;
        }
        LOG_DEBUG("CMD <:CM#>");
        int error_or_fail = getCommandSingleCharErrorOrLongResponse(PortFD, read_buffer, ":CM#");
        LOGF_DEBUG("RES <%s>", read_buffer);
        if (error_or_fail > 1)
        {
            if (strcmp(read_buffer, "N/A"))
            {
                if (read_buffer[0] == 'E' && read_buffer[1] >= '0'
                        && read_buffer[1] <= '9') //strcmp will be 0 if they match, so this is the case for failure.
                {
                    int error_code = read_buffer[1] - '0';
                    LOGF_DEBUG("Sync failed with response: %s, Error code: %i", read_buffer, error_code);
                    slewError(error_code);
                    EqNP.setState(IPS_ALERT);
                    LOG_ERROR("Synchronization failed.");
                    EqNP.apply();
                    return false;
                }
                else
                {
                    LOG_ERROR("Unexpected return on sync call!");
                    LOG_ERROR("Check system & Align if doing align to see if it went through!");
                    return false;
                }
            }
        }
        else
        {
            LOG_ERROR("Communication error on sync! Re-issue sync!");
            return false;
        }
    }

    currentRA  = ra;
    currentDEC = dec;

    LOG_INFO("OnStep: Synchronization successful.");
    return true;
}

bool LX200_OnStep::saveConfigItems(FILE *fp)
{
    LX200Generic::saveConfigItems(fp);
    WI::saveConfigItems(fp);
    return true;
}

void LX200_OnStep::Init_Outputs()
{
    if (OSHasOutputs)
    {
        // Features names and type are accessed via :GXYn (where n 1 to 8)
        // we take these names to display in Output tab
        // return value is ssssss,n where ssssss is the name and n is the type
        char port_name[MAXINDINAME] = {0}, getoutp[MAXINDINAME] = {0}, configured[MAXINDINAME] = {0}, p_name[MAXINDINAME] = {0};
        size_t  k {0};
        int error_or_fail = getCommandSingleCharErrorOrLongResponse(PortFD, configured,
                            ":GXY0#"); // returns a string with 1 where Feature is configured
        // ex: 10010010 means Feature 1,4 and 7 are configured

        if (error_or_fail == -4 && configured[0] == '0')
        {
            OSHasOutputs = false;
            LOG_INFO("Outputs not detected, disabling further checks");
        }

        IUFillNumber(&OutputPorts[0], "Unconfigured", "Unconfigured", "%g", 0, 255, 1, 0);
        for(int i = 1; i < PORTS_COUNT; i++)
        {
            if(configured[i - 1] == '1') // is Feature is configured
            {
                snprintf(getoutp, sizeof(getoutp), ":GXY%d#", i);
                int error_or_fail = getCommandSingleCharErrorOrLongResponse(PortFD, port_name, getoutp);
                if (error_or_fail > 0)
                {
                    for(k = 0; k < strlen(port_name); k++) // remove feature type
                    {
                        if(port_name[k] == ',') port_name[k] = '_';
                        p_name[k] = port_name[k];
                        p_name[k + 1] = 0;
                    }
                    IUFillNumber(&OutputPorts[i], p_name, p_name, "%g", 0, 255, 1, 0);
                }
                else
                {
                    LOGF_ERROR("Communication error on %s, ignoring, disconnect and reconnect to clear", getoutp);
                    IUFillNumber(&OutputPorts[i], "Unconfigured", "Unconfigured", "%g", 0, 255, 1, 0);
                }
            }
            else
            {
                IUFillNumber(&OutputPorts[i], "Unconfigured", "Unconfigured", "%g", 0, 255, 1, 0);
            }
        }
        defineProperty(&OutputPorts_NP);
    }
}


bool LX200_OnStep::sendScopeTime()
{
    char cdate[MAXINDINAME] = {0};
    char ctime[MAXINDINAME] = {0};
    struct tm ltm;
    struct tm utm;
    time_t time_epoch;
    memset(&ltm, 0, sizeof(ltm));
    memset(&utm, 0, sizeof(utm));

    double offset = 0;
    if (getUTFOffset(&offset))
    {
        char utcStr[8] = {0};
        snprintf(utcStr, 8, "%.2f", offset);
        TimeTP[OFFSET].setText(utcStr);
    }
    else
    {
        LOG_WARN("Could not obtain UTC offset from mount!");
        return false;
    }

    if (getLocalTime(ctime) == false)
    {
        LOG_WARN("Could not obtain local time from mount!");
        return false;
    }

    if (getLocalDate(cdate) == false)
    {
        LOG_WARN("Could not obtain local date from mount!");
        return false;
    }

    // To ISO 8601 format in LOCAL TIME!
    char datetime[MAXINDINAME] = {0};
    snprintf(datetime, MAXINDINAME, "%sT%s", cdate, ctime);

    // Now that date+time are combined, let's get tm representation of it.
    if (strptime(datetime, "%FT%T", &ltm) == nullptr)
    {
        LOGF_WARN("Could not process mount date and time: %s", datetime);
        return false;
    }

    ltm.tm_isdst = 0;
    // Get local time epoch in UNIX seconds
    time_epoch = mktime(&ltm);

    // LOCAL to UTC by subtracting offset.
    time_epoch -= static_cast<int>(offset * 3600.0);

    // Get UTC (we're using localtime_r, but since we shifted time_epoch above by UTCOffset, we should be getting the real UTC time)
    localtime_r(&time_epoch, &utm);

    // Format it into the final UTC ISO 8601
    strftime(cdate, MAXINDINAME, "%Y-%m-%dT%H:%M:%S", &utm);
    TimeTP[UTC].setText(cdate);

    LOGF_DEBUG("Mount controller UTC Time: %s", TimeTP[UTC].getText());
    LOGF_DEBUG("Mount controller UTC Offset: %s", TimeTP[OFFSET].getText());

    // Let's send everything to the client
    TimeTP.setState(IPS_OK);
    TimeTP.apply();

    return true;
}

bool LX200_OnStep::sendScopeLocation()
{
    int lat_dd = 0, lat_mm = 0, long_dd = 0, long_mm = 0;
    double lat_ssf = 0.0, long_ssf = 0.0;
    char lat_sexagesimal[MAXINDIFORMAT];
    char lng_sexagesimal[MAXINDIFORMAT];

    if (isSimulation())
    {
        LocationNP[LOCATION_LATITUDE].setValue(29.5);
        LocationNP[LOCATION_LONGITUDE].setValue(48.0);
        LocationNP[LOCATION_ELEVATION].setValue(10);
        LocationNP.setState(IPS_OK);
        LocationNP.apply();
        return true;
    }
    if (OSHighPrecision)
    {
        if (getSiteLatitudeAlt(PortFD, &lat_dd, &lat_mm, &lat_ssf, ":GtH#") < 0)
        {
            //NOTE: All OnStep pre-31 Aug 2020 will fail the above:
            //      So Try the normal command, if it fails
            if (getSiteLatitude(PortFD, &lat_dd, &lat_mm, &lat_ssf) < 0)
            {
                LOG_WARN("Failed to get site latitude from device.");
                return false;
            }
            else
            {
                double value = 0;
                OSHighPrecision = false; //Don't check using :GtH again
                snprintf(lat_sexagesimal, MAXINDIFORMAT, "%02d:%02d:%02.1lf", lat_dd, lat_mm, lat_ssf);
                f_scansexa(lat_sexagesimal, &value);
                LocationNP[LOCATION_LATITUDE].setValue(value);
            }
        }
        else
        {
            double value = 0;
            //Got High precision coordinates
            snprintf(lat_sexagesimal, MAXINDIFORMAT, "%02d:%02d:%02.1lf", lat_dd, lat_mm, lat_ssf);
            f_scansexa(lat_sexagesimal, &value);
            LocationNP[LOCATION_LATITUDE].setValue(value);
        }
    }
    if (!OSHighPrecision) //Bypass check
    {
        if (getSiteLatitude(PortFD, &lat_dd, &lat_mm, &lat_ssf) < 0)
        {
            LOG_WARN("Failed to get site latitude from device.");
            return false;
        }
        else
        {
            double value = 0;
            snprintf(lat_sexagesimal, MAXINDIFORMAT, "%02d:%02d:%02.1lf", lat_dd, lat_mm, lat_ssf);
            f_scansexa(lat_sexagesimal, &value);
            LocationNP[LOCATION_LATITUDE].setValue(value);
        }
    }

    if (OSHighPrecision)
    {
        if (getSiteLongitudeAlt(PortFD, &long_dd, &long_mm, &long_ssf, ":GgH#") < 0)
        {
            //NOTE: All OnStep pre-31 Aug 2020 will fail the above:
            //      So Try the normal command, if it fails
            if (getSiteLongitude(PortFD, &long_dd, &long_mm, &long_ssf) < 0)
            {
                LOG_WARN("Failed to get site longitude from device.");
                return false;
            }
            else
            {
                double value = 0;
                OSHighPrecision = false;
                snprintf(lng_sexagesimal, MAXINDIFORMAT, "%02d:%02d:%02.1lf", long_dd, long_mm, long_ssf);
                f_scansexa(lng_sexagesimal, &value);
                LocationNP[LOCATION_LONGITUDE].setValue(value);
            }
        }
        else
        {
            double value = 0;
            //Got High precision coordinates
            snprintf(lng_sexagesimal, MAXINDIFORMAT, "%02d:%02d:%02.1lf", long_dd, long_mm, long_ssf);
            f_scansexa(lng_sexagesimal, &value);
            LocationNP[LOCATION_LONGITUDE].setValue(value);
        }
    }
    if(!OSHighPrecision) //Not using high precision
    {
        if (getSiteLongitude(PortFD, &long_dd, &long_mm, &long_ssf) < 0)
        {
            LOG_WARN("Failed to get site longitude from device.");
            return false;
        }
        else
        {
            double value = 0;
            snprintf(lng_sexagesimal, MAXINDIFORMAT, "%02d:%02d:%02.1lf", long_dd, long_mm, long_ssf);

            f_scansexa(lng_sexagesimal, &value);
            LocationNP[LOCATION_LONGITUDE].setValue(value);
        }
    }

    LOGF_INFO("Mount has Latitude %s (%g) Longitude %s (%g) (Longitude sign in carthography format)",
              lat_sexagesimal,
              LocationNP[LOCATION_LATITUDE].getValue(),
              lng_sexagesimal,
              LocationNP[LOCATION_LONGITUDE].getValue());

    LocationNP.apply();

    saveConfig(true, "GEOGRAPHIC_COORD");

    return true;
}


bool LX200_OnStep::Goto(double ra, double dec)
{
    const struct timespec timeout = {0, 100000000L};

    targetRA  = ra;
    targetDEC = dec;
    char RAStr[64] = {0}, DecStr[64] = {0};
    int fracbase = 0;

    switch (getLX200EquatorialFormat())
    {
        case LX200_EQ_LONGER_FORMAT:
            fracbase = 360000;
            break;
        case LX200_EQ_LONG_FORMAT:
        case LX200_EQ_SHORT_FORMAT:
        default:
            fracbase = 3600;
            break;
    }

    fs_sexa(RAStr, targetRA, 2, fracbase);
    fs_sexa(DecStr, targetDEC, 2, fracbase);

    // If moving, let's stop it first.
    if (EqNP.getState() == IPS_BUSY)
    {
        if (!isSimulation() && abortSlew(PortFD) < 0)
        {
            AbortSP.setState(IPS_ALERT);
            LOG_ERROR("Abort slew failed.");
            Telescope::AbortSP.apply();
            return false;
        }

        AbortSP.setState(IPS_OK);
        EqNP.setState(IPS_IDLE);
        LOG_ERROR("Slew aborted.");
        AbortSP.apply();
        EqNP.apply();

        if (MovementNSSP.getState() == IPS_BUSY || MovementWESP.getState() == IPS_BUSY)
        {
            MovementNSSP.setState(IPS_IDLE);
            MovementWESP.setState(IPS_IDLE);
            EqNP.setState(IPS_IDLE);
            MovementNSSP.reset();
            MovementWESP.reset();
            MovementNSSP.apply();
            MovementWESP.apply();
        }

        // sleep for 100 mseconds
        nanosleep(&timeout, nullptr);
    }

    if (!isSimulation())
    {
        if (setObjectRA(PortFD, targetRA) < 0 || (setObjectDEC(PortFD, targetDEC)) < 0)
        {
            EqNP.setState(IPS_ALERT);
            LOG_ERROR("Error setting RA/DEC.");
            EqNP.apply();
            return false;
        }

        int err = 0;

        /* Slew reads the '0', that is not the end of the slew */
        if ((err = Slew(PortFD)))
        {
            LOGF_ERROR("Error Slewing to JNow RA %s - DEC %s", RAStr, DecStr);
            slewError(err);
            return false;
        }
    }

    //OnStep: DON'T set TrackState, this may resolve issues with the autoalign, this is updated by the status updates.
    //     TrackState = SCOPE_SLEWING;
    //EqNP.s     = IPS_BUSY;

    LOGF_INFO("Slewing to RA: %s - DEC: %s", RAStr, DecStr);

    return true;
}

void LX200_OnStep::SyncParkStatus(bool isparked)
{
    //NOTE: THIS SHOULD ONLY BE CALLED _AFTER_ TrackState is set by the update function.
    //Otherwise it will not be consistent.
    LOG_DEBUG("OnStep SyncParkStatus called");
    PrintTrackState();
    IsParked = isparked;
    ParkSP.reset();
    ParkSP.setState(IPS_OK);

    if (TrackState == SCOPE_PARKED)
    {
        ParkSP[PARK].setState(ISS_ON);
        LOG_INFO("Mount is parked.");
    }
    else
    {
        ParkSP[UNPARK].setState(ISS_ON);
        LOG_INFO("Mount is unparked.");
    }

    ParkSP.apply();
}


void LX200_OnStep::SetParked(bool isparked)
{
    PrintTrackState();
    SyncParkStatus(isparked);
    PrintTrackState();
    if (parkDataType != PARK_NONE)
        WriteParkData();
    PrintTrackState();
}

void LX200_OnStep::PrintTrackState()
{
#ifdef DEBUG_TRACKSTATE
    switch(TrackState)
    {
        case(SCOPE_IDLE):
            LOG_DEBUG("TrackState: SCOPE_IDLE");
            return;
        case(SCOPE_SLEWING):
            LOG_DEBUG("TrackState: SCOPE_SLEWING");
            return;
        case(SCOPE_TRACKING):
            LOG_DEBUG("TrackState: SCOPE_TRACKING");
            return;
        case(SCOPE_PARKING):
            LOG_DEBUG("TrackState: SCOPE_PARKING");
            return;
        case(SCOPE_PARKED):
            LOG_DEBUG("TrackState: SCOPE_PARKED");
            return;
    }
#endif
    return;
}

bool LX200_OnStep::setUTCOffset(double offset)
{
    bool result = true;
    char temp_string[RB_MAX_LEN];
    int utc_hour, utc_min;
    // strange thing offset is rounded up to first decimal so that .75 is .8
    utc_hour = int(offset) * -1;
    utc_min = abs((offset - int(offset)) * 60); // negative offsets require this abs()
    if (utc_min > 30) utc_min = 45;
    snprintf(temp_string, sizeof(temp_string), ":SG%03d:%02d#", utc_hour, utc_min);
    result = (setStandardProcedure(PortFD, temp_string) == 0);
    return result;
}

IPState LX200_OnStep::ExecuteHomeAction(TelescopeHomeAction action)
{
    // Homing, Cold and Warm Init
    switch (action)
    {
        case HOME_GO:
            if(!sendOnStepCommandBlind(":hC#"))
                return IPS_ALERT;
            return IPS_BUSY;

        case HOME_SET:
            if(!sendOnStepCommandBlind(":hF#"))
                return IPS_ALERT;
            return IPS_OK;

        default:
            return IPS_ALERT;
    }

    return IPS_ALERT;
}

bool LX200_OnStep::Handshake()
{
    if (checkConnection())
    {
        return true;
    }

    /* OnStepX has a tendency to start up in an unresponsive state
     * due to grabage in the serial buffer. Try to reset it by sending
     * the :GVP# command repeatedly.
     *
     * First sending should result in a '0' response, the second in
     * 'OnStep' so the 2nd sending should return with a failure.
     */
    if(sendOnStepCommand(":GVP#"))
    {
        if(!sendOnStepCommand(":GVP#"))
        {
            return checkConnection();
        }
    }

    return false;
}

void LX200_OnStep::initSlewRates()
{
    SlewRateSP[0].fill("0", "0.25x", ISS_OFF);
    SlewRateSP[1].fill("1", "0.5x", ISS_OFF);
    SlewRateSP[2].fill("2", "1x", ISS_OFF);
    SlewRateSP[3].fill("3", "2x", ISS_OFF);
    SlewRateSP[4].fill("4", "4x", ISS_OFF);
    SlewRateSP[5].fill("5", "8x", ISS_ON);
    SlewRateSP[6].fill("6", "20x", ISS_OFF);   //last OnStep - OnStepX
    SlewRateSP[7].fill("7", "48x", ISS_OFF);
    SlewRateSP[8].fill("8", "Half-Max", ISS_OFF);
    SlewRateSP[9].fill("9", "Max", ISS_OFF);

    SlewRateSP.fill(getDeviceName(), "TELESCOPE_SLEW_RATE", "Slew Rate", MOTION_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);
}

bool LX200_OnStep::MoveNS(INDI_DIR_NS dir, TelescopeMotionCommand command)
{
    if (command == MOTION_START)
    {
        if (MovementWESP.getState() != IPS_BUSY && m_RememberPollingPeriod == 0)
            m_RememberPollingPeriod = getCurrentPollingPeriod();
        setCurrentPollingPeriod(200);
    }
    else
    {
        // Only restore if WE isn't moving
        if (MovementWESP.getState() != IPS_BUSY)
        {
            setCurrentPollingPeriod(m_RememberPollingPeriod);
            m_RememberPollingPeriod = 0;
        }
    }

    return LX200Telescope::MoveNS(dir, command);
}

bool LX200_OnStep::MoveWE(INDI_DIR_WE dir, TelescopeMotionCommand command)
{
    if (command == MOTION_START)
    {
        if (MovementNSSP.getState() != IPS_BUSY && m_RememberPollingPeriod == 0)
            m_RememberPollingPeriod = getCurrentPollingPeriod();
        setCurrentPollingPeriod(200);
    }
    else
    {
        // Only restore if NS isn't moving
        if (MovementNSSP.getState() != IPS_BUSY)
        {
            setCurrentPollingPeriod(m_RememberPollingPeriod);
            m_RememberPollingPeriod = 0;
        }
    }

    return LX200Telescope::MoveWE(dir, command);
}