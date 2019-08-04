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


#include "lx200_OnStep.h"

#define LIBRARY_TAB  "Library"
#define FIRMWARE_TAB "Firmware data"
#define STATUS_TAB "ONStep Status"
#define PEC_TAB "PEC"
#define ALIGN_TAB "Align"
#define OUTPUT_TAB "Outputs"

#define ONSTEP_TIMEOUT  3
#define RA_AXIS     0
#define DEC_AXIS    1

LX200_OnStep::LX200_OnStep() : LX200Generic()
{
    currentCatalog    = LX200_STAR_C;
    currentSubCatalog = 0;

    setVersion(1, 7);   // don't forget to update libindi/drivers.xml

    setLX200Capability(LX200_HAS_TRACKING_FREQ | LX200_HAS_SITES | LX200_HAS_ALIGNMENT_TYPE | LX200_HAS_PULSE_GUIDING | LX200_HAS_PRECISE_TRACKING_FREQ);
    
    SetTelescopeCapability(GetTelescopeCapability() | TELESCOPE_CAN_CONTROL_TRACK | TELESCOPE_HAS_PEC | TELESCOPE_HAS_PIER_SIDE | TELESCOPE_HAS_TRACK_RATE, 10 );
    
    //CAN_ABORT, CAN_GOTO ,CAN_PARK ,CAN_SYNC ,HAS_LOCATION ,HAS_TIME ,HAS_TRACK_MODE Already inherited from lx200generic,
    // 4 stands for the number of Slewrate Buttons as defined in Inditelescope.cpp
    //setLX200Capability(LX200_HAS_FOCUS | LX200_HAS_TRACKING_FREQ | LX200_HAS_ALIGNMENT_TYPE | LX200_HAS_SITES | LX200_HAS_PULSE_GUIDING);
    //
    // Get generic capabilities but discard the followng:
    // LX200_HAS_FOCUS


    FI::SetCapability(FOCUSER_CAN_ABS_MOVE | FOCUSER_CAN_REL_MOVE | FOCUSER_CAN_ABORT);
    // Unused option: FOCUSER_HAS_VARIABLE_SPEED
}

const char *LX200_OnStep::getDefaultName()
{
    return "LX200 OnStep";
}

bool LX200_OnStep::initProperties()
{

    LX200Generic::initProperties();
    FI::initProperties(FOCUS_TAB);
    SetParkDataType(PARK_RA_DEC);

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


    // ============== MAIN_CONTROL_TAB
    IUFillSwitch(&ReticS[0], "PLUS", "Light", ISS_OFF);
    IUFillSwitch(&ReticS[1], "MOINS", "Dark", ISS_OFF);
    IUFillSwitchVector(&ReticSP, ReticS, 2, getDeviceName(), "RETICULE_BRIGHTNESS", "Reticule +/-", MAIN_CONTROL_TAB, IP_RW, ISR_ATMOST1, 60, IPS_IDLE);

    IUFillNumber(&ElevationLimitN[0], "minAlt", "Elev Min", "%+03f", -90.0, 90.0, 1.0, -30.0);
    IUFillNumber(&ElevationLimitN[1], "maxAlt", "Elev Max", "%+03f", -90.0, 90.0, 1.0, 89.0);
    IUFillNumberVector(&ElevationLimitNP, ElevationLimitN, 2, getDeviceName(), "Slew elevation Limit", "", MAIN_CONTROL_TAB, IP_RW, 0, IPS_IDLE);

    IUFillText(&ObjectInfoT[0], "Info", "", "");
    IUFillTextVector(&ObjectInfoTP, ObjectInfoT, 1, getDeviceName(), "Object Info", "", MAIN_CONTROL_TAB, IP_RO, 0, IPS_IDLE);

    // ============== CONNECTION_TAB

    // ============== OPTION_TAB

    // ============== MOTION_CONTROL_TAB
    //Override the standard slew rate command. Also add appropriate description. This also makes it work in Ekos Mount Control correctly
    //Note that SlewRateSP and MaxSlewRateNP BOTH track the rate. I have left them in there because MaxRateNP reports OnStep Values
    uint8_t nSlewRate = 10;
    free(SlewRateS);
    SlewRateS = (ISwitch *)malloc(sizeof(ISwitch) * nSlewRate);
    // 0=.25X 1=.5x 2=1x 3=2x 4=4x 5=8x 6=24x 7=48x 8=half-MaxRate 9=MaxRate
    IUFillSwitch(&SlewRateS[0], "0", "0.25x", ISS_OFF);
    IUFillSwitch(&SlewRateS[1], "1", "0.5x", ISS_OFF);
    IUFillSwitch(&SlewRateS[2], "2", "1x", ISS_OFF);
    IUFillSwitch(&SlewRateS[3], "3", "2x", ISS_OFF);
    IUFillSwitch(&SlewRateS[4], "4", "4x", ISS_OFF);
    IUFillSwitch(&SlewRateS[5], "5", "8x", ISS_ON);
    IUFillSwitch(&SlewRateS[6], "6", "24x", ISS_OFF);
    IUFillSwitch(&SlewRateS[7], "7", "48x", ISS_OFF);
    IUFillSwitch(&SlewRateS[8], "8", "Half-Max", ISS_OFF);
    IUFillSwitch(&SlewRateS[9], "9", "Max", ISS_OFF);
    IUFillSwitchVector(&SlewRateSP, SlewRateS, nSlewRate, getDeviceName(), "TELESCOPE_SLEW_RATE", "Slew Rate", MOTION_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    IUFillNumber(&MaxSlewRateN[0], "maxSlew", "Rate", "%f", 0.0, 9.0, 1.0, 5.0);    //2.0, 9.0, 1.0, 9.0
    IUFillNumberVector(&MaxSlewRateNP, MaxSlewRateN, 1, getDeviceName(), "Max slew Rate", "", MOTION_TAB, IP_RW, 0,IPS_IDLE);

    IUFillSwitch(&TrackCompS[0], "1", "Full Compensation", ISS_OFF);
    IUFillSwitch(&TrackCompS[1], "2", "Refraction", ISS_OFF);
    IUFillSwitch(&TrackCompS[2], "3", "Off", ISS_ON);
    IUFillSwitchVector(&TrackCompSP, TrackCompS, 3, getDeviceName(), "Compensation", "Compensation Tracking", MOTION_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);
    
    IUFillSwitch(&TrackAxisS[0], "1", "Single Axis", ISS_OFF);
    IUFillSwitch(&TrackAxisS[1], "2", "Dual Axis", ISS_OFF);
    IUFillSwitchVector(&TrackAxisSP, TrackAxisS, 2, getDeviceName(), "Multi-Axis", "Multi-Axis Tracking", MOTION_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    IUFillSwitch(&TrackAxisS[0], "1", "Single Axis", ISS_OFF);
    IUFillSwitch(&TrackAxisS[1], "2", "Dual Axis", ISS_OFF);
    IUFillSwitchVector(&TrackAxisSP, TrackAxisS, 2, getDeviceName(), "Multi-Axis", "Multi-Axis Tracking", MOTION_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);
    
    IUFillNumber(&BacklashN[0], "Backlash DEC", "DE", "%g", 0, 999, 1, 15);
    IUFillNumber(&BacklashN[1], "Backlash RA", "RA", "%g", 0, 999, 1, 15);
    IUFillNumberVector(&BacklashNP, BacklashN, 2, getDeviceName(), "Backlash", "", MOTION_TAB, IP_RW, 0,IPS_IDLE);
    
    IUFillNumber(&GuideRateN[RA_AXIS], "GUIDE_RATE_WE", "W/E Rate", "%g", 0, 1, 0.25, 0.5);
    IUFillNumber(&GuideRateN[DEC_AXIS], "GUIDE_RATE_NS", "N/S Rate", "%g", 0, 1, 0.25, 0.5);
    IUFillNumberVector(&GuideRateNP, GuideRateN, 2, getDeviceName(), "GUIDE_RATE", "Guiding Rate", MOTION_TAB, IP_RO, 0, IPS_IDLE);
    
    IUFillSwitch(&AutoFlipS[0], "1", "AutoFlip: OFF", ISS_OFF);
    IUFillSwitch(&AutoFlipS[1], "2", "AutoFlip: ON", ISS_OFF);
    IUFillSwitchVector(&AutoFlipSP, AutoFlipS, 2, getDeviceName(), "AutoFlip", "Meridian Auto Flip", MOTION_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    IUFillSwitch(&HomePauseS[0], "1", "HomePause: OFF", ISS_OFF);
    IUFillSwitch(&HomePauseS[1], "2", "HomePause: ON", ISS_OFF);
    IUFillSwitch(&HomePauseS[2], "3", "HomePause: Continue", ISS_OFF);
    IUFillSwitchVector(&HomePauseSP, HomePauseS, 3, getDeviceName(), "HomePause", "Pause at Home", MOTION_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    IUFillSwitch(&FrequencyAdjustS[0], "1", "Frequency -", ISS_OFF);
    IUFillSwitch(&FrequencyAdjustS[1], "2", "Frequency +", ISS_OFF);
    IUFillSwitch(&FrequencyAdjustS[2], "3", "Reset Sidereal Frequency", ISS_OFF);
    IUFillSwitchVector(&FrequencyAdjustSP, FrequencyAdjustS, 3, getDeviceName(), "FrequencyAdjust", "Frequency Adjust", MOTION_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    IUFillSwitch(&PreferredPierSideS[0], "1", "West", ISS_OFF);
    IUFillSwitch(&PreferredPierSideS[1], "2", "East", ISS_OFF);
    IUFillSwitch(&PreferredPierSideS[2], "3", "Best", ISS_OFF);
    IUFillSwitchVector(&PreferredPierSideSP, PreferredPierSideS, 3, getDeviceName(), "Preferred Pier Side", "Preferred Pier Side", MOTION_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);
    
    IUFillNumber(&minutesPastMeridianN[0], "East", "East", "%g", 0, 180, 1, 30);
    IUFillNumber(&minutesPastMeridianN[1], "West", "West", "%g", 0, 180, 1, 30);
    IUFillNumberVector(&minutesPastMeridianNP, minutesPastMeridianN, 2, getDeviceName(), "Minutes Past Meridian", "Minutes Past Meridian", MOTION_TAB, IP_RW, 0,IPS_IDLE);
    

    // ============== SITE_MANAGEMENT_TAB
    IUFillSwitch(&SetHomeS[0], "RETURN_HOME", "Return  Home", ISS_OFF);
    IUFillSwitch(&SetHomeS[1], "AT_HOME", "At Home (Reset)", ISS_OFF);
    IUFillSwitchVector(&SetHomeSP, SetHomeS, 2, getDeviceName(), "HOME_INIT", "Homing", SITE_TAB, IP_RW, ISR_ATMOST1, 60, IPS_IDLE);

    // ============== GUIDE_TAB

    // ============== FOCUSER_TAB
    // Focuser 1

    IUFillSwitch(&OSFocus1InitializeS[0], "Focus1_0", "Zero", ISS_OFF);
    IUFillSwitch(&OSFocus1InitializeS[1], "Focus1_2", "Mid", ISS_OFF);
    //     IUFillSwitch(&OSFocus1InitializeS[2], "Focus1_3", "max", ISS_OFF);
    IUFillSwitchVector(&OSFocus1InitializeSP, OSFocus1InitializeS, 2, getDeviceName(), "Foc1Rate", "Initialize", FOCUS_TAB, IP_RW, ISR_ATMOST1, 0, IPS_IDLE);


    // Focuser 2
    //IUFillSwitch(&OSFocus2SelS[0], "Focus2_Sel1", "Foc 1", ISS_OFF);
    //IUFillSwitch(&OSFocus2SelS[1], "Focus2_Sel2", "Foc 2", ISS_OFF);
    //IUFillSwitchVector(&OSFocus2SelSP, OSFocus2SelS, 2, getDeviceName(), "Foc2Sel", "Foc 2", FOCUS_TAB, IP_RW, ISR_ATMOST1, 0, IPS_IDLE);

    IUFillSwitch(&OSFocus2MotionS[0], "Focus2_In", "In", ISS_OFF);
    IUFillSwitch(&OSFocus2MotionS[1], "Focus2_Out", "Out", ISS_OFF);
    IUFillSwitch(&OSFocus2MotionS[2], "Focus2_Stop", "Stop", ISS_OFF);
    IUFillSwitchVector(&OSFocus2MotionSP, OSFocus2MotionS, 3, getDeviceName(), "Foc2Mot", "Foc 2 Motion", FOCUS_TAB, IP_RW, ISR_ATMOST1, 0, IPS_IDLE);

    IUFillSwitch(&OSFocus2RateS[0], "Focus2_1", "min", ISS_OFF);
    IUFillSwitch(&OSFocus2RateS[1], "Focus2_2", "0.01", ISS_OFF);
    IUFillSwitch(&OSFocus2RateS[2], "Focus2_3", "0.1", ISS_OFF);
    IUFillSwitch(&OSFocus2RateS[3], "Focus2_4", "1", ISS_OFF);
    IUFillSwitchVector(&OSFocus2RateSP, OSFocus2RateS, 4, getDeviceName(), "Foc2Rate", "Foc 2 Rates", FOCUS_TAB, IP_RW, ISR_ATMOST1, 0, IPS_IDLE);

    IUFillNumber(&OSFocus2TargN[0], "FocusTarget2", "Abs Pos", "%g", -25000, 25000, 1, 0);
    IUFillNumberVector(&OSFocus2TargNP, OSFocus2TargN, 1, getDeviceName(), "Foc2Targ", "Foc 2 Target", FOCUS_TAB, IP_RW, 0, IPS_IDLE);

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
    IUFillSwitchVector(&OSPECStatusSP, OSPECStatusS, 5, getDeviceName(), "PEC Status", "PEC Status", PEC_TAB, IP_RO, ISR_ATMOST1, 0, IPS_IDLE);

    IUFillSwitch(&OSPECIndexS[0], "Not Detected", "Not Detected", ISS_ON);
    IUFillSwitch(&OSPECIndexS[1], "Detected", "Detected", ISS_OFF);
    IUFillSwitchVector(&OSPECIndexSP, OSPECIndexS, 2, getDeviceName(), "PEC Index Detect", "PEC Index", PEC_TAB, IP_RO, ISR_ATMOST1, 0, IPS_IDLE);

    IUFillSwitch(&OSPECRecordS[0], "Clear", "Clear", ISS_OFF);
    IUFillSwitch(&OSPECRecordS[1], "Record", "Record", ISS_OFF);
    IUFillSwitch(&OSPECRecordS[2], "Write to EEPROM", "Write to EEPROM", ISS_OFF);
    IUFillSwitchVector(&OSPECRecordSP, OSPECRecordS, 3, getDeviceName(), "PEC Operations", "PEC Recording", PEC_TAB, IP_RW, ISR_ATMOST1, 0, IPS_IDLE);

    IUFillSwitch(&OSPECReadS[0], "Read", "Read PEC to FILE****", ISS_OFF);
    IUFillSwitch(&OSPECReadS[1], "Write", "Write PEC from FILE***", ISS_OFF);
    //    IUFillSwitch(&OSPECReadS[2], "Write to EEPROM", "Write to EEPROM", ISS_OFF);
    IUFillSwitchVector(&OSPECReadSP, OSPECReadS, 2, getDeviceName(), "PEC File", "PEC File", PEC_TAB, IP_RW, ISR_ATMOST1, 0, IPS_IDLE);
    // ============== New ALIGN_TAB
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
    IUFillSwitchVector(&OSNAlignStarsSP, OSNAlignStarsS, 9, getDeviceName(), "AlignStars", "Align using some stars, Alpha only", ALIGN_TAB, IP_RW, ISR_ATMOST1, 0, IPS_IDLE);
    
    IUFillSwitch(&OSNAlignS[0], "0", "Start Align", ISS_OFF);
    IUFillSwitch(&OSNAlignS[1], "1", "Issue Align", ISS_OFF);
    IUFillSwitch(&OSNAlignS[2], "3", "Write Align", ISS_OFF);
    IUFillSwitchVector(&OSNAlignSP, OSNAlignS, 2, getDeviceName(), "NewAlignStar", "Align using up to 6 stars, Alpha only", ALIGN_TAB, IP_RW, ISR_ATMOST1, 0, IPS_IDLE);
    
    IUFillSwitch(&OSNAlignWriteS[0], "0", "Write Align to NVRAM/Flash", ISS_OFF);
    IUFillSwitchVector(&OSNAlignWriteSP, OSNAlignWriteS, 1, getDeviceName(), "NewAlignStar2", "NVRAM", ALIGN_TAB, IP_RW, ISR_ATMOST1, 0, IPS_IDLE);
    IUFillSwitch(&OSNAlignPolarRealignS[0], "0", "Instructions", ISS_OFF);
    IUFillSwitch(&OSNAlignPolarRealignS[1], "1", "Refine Polar Align (manually)", ISS_OFF);
    IUFillSwitchVector(&OSNAlignPolarRealignSP, OSNAlignPolarRealignS, 2, getDeviceName(), "AlignMP", "Polar Correction, See info box", ALIGN_TAB, IP_RW, ISR_ATMOST1, 0, IPS_IDLE);
    
    IUFillText(&OSNAlignT[0], "0", "Align Process Status", "Align not started");
    IUFillText(&OSNAlignT[1], "1", "1. Manual Process", "Point towards the NCP");
    IUFillText(&OSNAlignT[2], "2", "2. Plate Solver Process", "Point towards the NCP");
    IUFillText(&OSNAlignT[3], "3", "Manual Action after 1", "Press 'Start Align'");
    IUFillText(&OSNAlignT[4], "4", "Current Status", "Not Updated");
    IUFillText(&OSNAlignT[5], "5", "Max Stars", "Not Updated");
    IUFillText(&OSNAlignT[6], "6", "Current Star", "Not Updated");
    IUFillText(&OSNAlignT[7], "7", "# of Align Stars", "Not Updated");
    IUFillTextVector(&OSNAlignTP, OSNAlignT, 8, getDeviceName(), "NAlign Process", "", ALIGN_TAB, IP_RO, 0, IPS_IDLE);

    IUFillText(&OSNAlignErrT[0], "0", "EQ Polar Error Alt", "Available once Aligned");
    IUFillText(&OSNAlignErrT[1], "1", "EQ Polar Error Az", "Available once Aligned");
    //     IUFillText(&OSNAlignErrT[2], "2", "2. Plate Solver Process", "Point towards the NCP");
    //     IUFillText(&OSNAlignErrT[3], "3", "After 1 or 2", "Press 'Start Align'");
    //     IUFillText(&OSNAlignErrT[4], "4", "Current Status", "Not Updated");
    //     IUFillText(&OSNAlignErrT[5], "5", "Max Stars", "Not Updated");
    //     IUFillText(&OSNAlignErrT[6], "6", "Current Star", "Not Updated");
    //     IUFillText(&OSNAlignErrT[7], "7", "# of Align Stars", "Not Updated");
    IUFillTextVector(&OSNAlignErrTP, OSNAlignErrT, 2, getDeviceName(), "ErrAlign Process", "", ALIGN_TAB, IP_RO, 0, IPS_IDLE);

#ifdef ONSTEP_NOTDONE
    // =============== OUTPUT_TAB
    // ===============
    IUFillSwitch(&OSOutput1S[0], "0", "OFF", ISS_ON);
    IUFillSwitch(&OSOutput1S[1], "1", "ON", ISS_OFF);
    IUFillSwitchVector(&OSOutput1SP, OSOutput1S, 2, getDeviceName(), "Output 1", "Output 1", OUTPUT_TAB, IP_RW, ISR_ATMOST1, 60, IPS_ALERT);

    IUFillSwitch(&OSOutput2S[0], "0", "OFF", ISS_ON);
    IUFillSwitch(&OSOutput2S[1], "1", "ON", ISS_OFF);
    IUFillSwitchVector(&OSOutput2SP, OSOutput2S, 2, getDeviceName(), "Output 2", "Output 2", OUTPUT_TAB, IP_RW, ISR_ATMOST1, 60, IPS_ALERT);
#endif

    for(int i = 0; i < PORTS_COUNT; i++)
    {
        char port_name[30];
        sprintf(port_name, "Output %d", i);
        IUFillNumber(&OutputPorts[i], port_name, port_name, "%g", 0, 255, 1, 0);
    }

    IUFillNumberVector(&OutputPorts_NP, OutputPorts, PORTS_COUNT, getDeviceName(), "Outputs", "Outputs",  OUTPUT_TAB, IP_WO, 60, IPS_OK);


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
    IUFillTextVector(&OnstepStatTP, OnstepStat, 9, getDeviceName(), "OnStep Status", "", STATUS_TAB, IP_RO, 0, IPS_OK);


    
    setDriverInterface(getDriverInterface() | FOCUSER_INTERFACE);

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
    FI::updateProperties();

    if (isConnected())
    {
        // Firstinitialize some variables
        // keep sorted by TABs is easier
        // Main Control
        defineSwitch(&ReticSP);
        defineNumber(&ElevationLimitNP);
        defineText(&ObjectInfoTP);
        // Connection

        // Options

        // Motion Control
        defineNumber(&MaxSlewRateNP);
        defineSwitch(&TrackCompSP);
        defineSwitch(&TrackAxisSP);
        defineNumber(&BacklashNP);
        defineNumber(&GuideRateNP);
        defineSwitch(&AutoFlipSP);
        defineSwitch(&HomePauseSP);
        defineSwitch(&FrequencyAdjustSP);
        defineSwitch(&PreferredPierSideSP);
        defineNumber(&minutesPastMeridianNP);

        // Site Management
        defineSwitch(&ParkOptionSP);
        defineSwitch(&SetHomeSP);

        // Guide

        // Focuser

        // Focuser 1

        if (!sendOnStepCommand(":FA#"))  // do we have a Focuser 1
        {
            OSFocuser1 = true;
            defineSwitch(&OSFocus1InitializeSP);
        }
        // Focuser 2
        if (!sendOnStepCommand(":fA#"))  // Do we have a Focuser 2
        {
            OSFocuser2 = true;
            //defineSwitch(&OSFocus2SelSP);
            defineSwitch(&OSFocus2MotionSP);
            defineSwitch(&OSFocus2RateSP);
            defineNumber(&OSFocus2TargNP);
        }

        // Firmware Data
        defineText(&VersionTP);

        //PEC
        defineSwitch(&OSPECStatusSP);
        defineSwitch(&OSPECIndexSP);
        defineSwitch(&OSPECRecordSP);
        defineSwitch(&OSPECReadSP);

        //New Align
        defineSwitch(&OSNAlignStarsSP);
        defineSwitch(&OSNAlignSP);
        defineSwitch(&OSNAlignWriteSP);
        defineText(&OSNAlignTP);
        defineText(&OSNAlignErrTP);
        defineSwitch(&OSNAlignPolarRealignSP);

#ifdef ONSTEP_NOTDONE
        //Outputs
        defineSwitch(&OSOutput1SP);
        defineSwitch(&OSOutput2SP);
#endif

        defineNumber(&OutputPorts_NP);

        // OnStep Status
        defineText(&OnstepStatTP);

        if (InitPark())
        {
            // If loading parking data is successful, we just set the default parking values.
            SetAxis1ParkDefault(LocationN[LOCATION_LATITUDE].value >= 0 ? 0 : 180);
            SetAxis2ParkDefault(LocationN[LOCATION_LATITUDE].value);
        }
        else
        {
            // Otherwise, we set all parking data to default in case no parking data is found.
            SetAxis1Park(LocationN[LOCATION_LATITUDE].value >= 0 ? 0 : 180);
            SetAxis1ParkDefault(LocationN[LOCATION_LATITUDE].value);

            SetAxis1ParkDefault(LocationN[LOCATION_LATITUDE].value >= 0 ? 0 : 180);
            SetAxis2ParkDefault(LocationN[LOCATION_LATITUDE].value);
        }

        double longitude = -1000, latitude = -1000;
        // Get value from config file if it exists.
        IUGetConfigNumber(getDeviceName(), "GEOGRAPHIC_COORD", "LONG", &longitude);
        IUGetConfigNumber(getDeviceName(), "GEOGRAPHIC_COORD", "LAT", &latitude);
        if (longitude != -1000 && latitude != -1000)
        {
            updateLocation(latitude, longitude, 0);
        }
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
        deleteProperty(ParkOptionSP.name);
        deleteProperty(SetHomeSP.name);
        // Guide

        // Focuser
        // Focuser 1
        deleteProperty(OSFocus1InitializeSP.name);

        // Focuser 2
        //deleteProperty(OSFocus2SelSP.name);
        deleteProperty(OSFocus2MotionSP.name);
        deleteProperty(OSFocus2RateSP.name);
        deleteProperty(OSFocus2TargNP.name);

        // Firmware Data
        deleteProperty(VersionTP.name);


        //PEC
        deleteProperty(OSPECStatusSP.name);
        deleteProperty(OSPECIndexSP.name);
        deleteProperty(OSPECRecordSP.name);
        deleteProperty(OSPECReadSP.name);

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
    }
    return true;
}

bool LX200_OnStep::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        if (strstr(name, "FOCUS_"))
            return FI::processNumber(dev, name, values, names, n);
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
            char cmd[4];
            snprintf(cmd, 4, ":R%d#", (int)values[0]);
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
            IUResetSwitch(&SlewRateSP);
            SlewRateS[int(values[0])].s = ISS_ON;
            SlewRateSP.s = IPS_OK;
            IDSetSwitch(&SlewRateSP, nullptr);
            return true;
        }

        if (!strcmp(name, BacklashNP.name))
        {
            char cmd[9];
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
                if (setMinElevationLimit(PortFD, (int)minAlt) < 0)
                {
                    ElevationLimitNP.s = IPS_ALERT;
                    IDSetNumber(&ElevationLimitNP, "Error setting min elevation limit.");
                }

                if (setMaxElevationLimit(PortFD, (int)maxAlt) < 0)
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
        char cmd[20];
        int i, nset;
        double minPMEast = 0, minPMWest = 0;

        for (nset = i = 0; i < n; i++)
        {
            INumber *bktp = IUFindNumber(&minutesPastMeridianNP, names[i]);
            if (bktp == &minutesPastMeridianN[0])
            {
                minPMEast = values[i];
                LOGF_DEBUG("===CMD==> minutesPastMeridianN[0]/East = %f", minPMEast);
                nset += minPMEast >= 0 && minPMEast <= 180;  //range 0 to 180
            }
            else if (bktp == &minutesPastMeridianN[1])
            {
                minPMWest = values[i];
                LOGF_DEBUG("===CMD==> minutesPastMeridianN[1]/West= %f", minPMWest);
                nset += minPMWest >= 0 && minPMWest <= 180;   //range 0 to 180
            }
        }
        if (nset == 2)
        {
            snprintf(cmd, 20, ":SXE9,%d#", (int) minPMEast);
            if (sendOnStepCommand(cmd))
            {
                minutesPastMeridianNP.s = IPS_ALERT;
                IDSetNumber(&minutesPastMeridianNP, "Error Backlash DEC limit.");
            }
            const struct timespec timeout = {0, 100000000L};
            nanosleep(&timeout, nullptr); // time for OnStep to respond to previous cmd
            snprintf(cmd, 20, ":SXEA,%d#", (int) minPMWest);
            if (sendOnStepCommand(cmd))
            {
                minutesPastMeridianNP.s = IPS_ALERT;
                IDSetNumber(&minutesPastMeridianNP, "Error Backlash RA limit.");
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
        char cmd[32];

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
                char cmd[20];
                int port = STARTING_PORT + i;

                snprintf(cmd, sizeof(cmd), ":SXG%d,%d#", port, value);
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

    return LX200Generic::ISNewNumber(dev, name, values, names, n);
}

bool LX200_OnStep::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    int index = 0;

    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        // Reticlue +/- Buttons
        if (!strcmp(name, ReticSP.name))
        {
            long ret = 0;

            IUUpdateSwitch(&ReticSP, states, names, n);
            ReticSP.s = IPS_OK;

            if (ReticS[0].s == ISS_ON)
            {
                ret = ReticPlus(PortFD);
                ReticS[0].s = ISS_OFF;
                IDSetSwitch(&ReticSP, "Bright");
            }
            else
            {
                ret = ReticMoins(PortFD);
                ReticS[1].s = ISS_OFF;
                IDSetSwitch(&ReticSP, "Dark");
            }

            IUResetSwitch(&ReticSP);
            IDSetSwitch(&ReticSP, nullptr);
            return true;
        }
        //Move to more standard controls
        if (!strcmp(name, SlewRateSP.name))
        {
                IUUpdateSwitch(&SlewRateSP, states, names, n);
                int ret;
                char cmd[4];
                int index = IUFindOnSwitchIndex(&SlewRateSP) ;//-1; //-1 because index is 1-10, OS Values are 0-9
                snprintf(cmd, 4, ":R%d#", index);
                ret = sendOnStepCommandBlind(cmd);
                
                //if (setMaxSlewRate(PortFD, (int)values[0]) < 0) //(int) MaxSlewRateN[0].value
                if (ret == -1)
                {
                        LOGF_DEBUG("Pas OK Return value =%d", ret);
                        LOGF_DEBUG("Setting Max Slew Rate to %u\n", index);
                        SlewRateSP.s = IPS_ALERT;
                        IDSetSwitch(&SlewRateSP, "Setting Max Slew Rate Failed");
                        return false;
                }
                LOGF_INFO("Setting Max Slew Rate to %u\n", index);
                LOGF_DEBUG("OK Return value =%d", ret);
                MaxSlewRateNP.s           = IPS_OK;
                MaxSlewRateNP.np[0].value = index;
                IDSetNumber(&MaxSlewRateNP, "Slewrate set to %d", index);
                IUResetSwitch(&SlewRateSP);
                SlewRateS[index].s = ISS_ON;
                SlewRateSP.s = IPS_OK;
                IDSetSwitch(&SlewRateSP, nullptr);
                return true;
        }
        // Homing, Cold and Warm Init
        if (!strcmp(name, SetHomeSP.name))
        {
            IUUpdateSwitch(&SetHomeSP, states, names, n);
            SetHomeSP.s = IPS_OK;

            if (SetHomeS[0].s == ISS_ON)
            {
                if(!sendOnStepCommandBlind(":hC#"))
                    return false;
                IDSetSwitch(&SetHomeSP, "Return Home");
                SetHomeS[0].s = ISS_OFF;
            }
            else
            {
                if(!sendOnStepCommandBlind(":hF#"))
                    return false;
                IDSetSwitch(&SetHomeSP, "At Home (Reset)");
                SetHomeS[1].s = ISS_OFF;
            }
            IUResetSwitch(&ReticSP);
            SetHomeSP.s = IPS_IDLE;
            IDSetSwitch(&SetHomeSP, nullptr);
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
                    IDSetSwitch(&AutoFlipSP, "Auto Meridan Flip OFF");
                    return true;
                } 
            }
            if (AutoFlipS[1].s == ISS_ON)
            {
                if (sendOnStepCommand(":SX95,1#"))
                {
                    AutoFlipSP.s = IPS_OK;
                    IDSetSwitch(&AutoFlipSP, "Auto Meridan Flip ON");
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
            char cmd[32];
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

        // Focuser 2 Rates
        if (!strcmp(name, OSFocus2RateSP.name))
        {
            char cmd[32];

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
            char cmd[32];

            if (IUUpdateSwitch(&OSFocus2MotionSP, states, names, n) < 0)
                return false;

            index = IUFindOnSwitchIndex(&OSFocus2MotionSP);
            if (index == 0)
            {
                strcpy(cmd, ":f+#");
            }
            if (index == 1)
            {
                strcpy(cmd, ":f-#");
            }
            if (index == 2)
            {
                strcpy(cmd, ":fQ#");
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
    if (!strcmp(name, PECStateSP.name))
    {
        index = IUFindOnSwitchIndex(&PECStateSP);
        if (index == 0)
        {
            OSPECEnabled = true;
            StopPECPlayback(0); //Status will set OSPECEnabled to false if that's set by the controller
            PECStateS[0].s = ISS_ON;
            PECStateS[1].s = ISS_OFF;
            IDSetSwitch(&PECStateSP, nullptr);
        }
        else if (index == 1)
        {
            OSPECEnabled = true;
            StartPECPlayback(0);
            PECStateS[0].s = ISS_OFF;
            PECStateS[1].s = ISS_ON;
            IDSetSwitch(&PECStateSP, nullptr);
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
            if ((index_stars <= 8) && (index_stars >= 0)) {
                int stars = index_stars+1;
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
        char cmd[10];
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
        if (OSNAlignPolarRealignS[1].s == ISS_ON) //Command
        {
            OSNAlignPolarRealignS[1].s = ISS_OFF;
            // int returncode=sendOnStepCommand("
            snprintf(cmd, 5, ":MP#");
            sendOnStepCommandBlind(cmd);
            if (!sendOnStepCommandBlind(":MP#"))
            {
                IDSetSwitch(&OSNAlignPolarRealignSP, "Command for Refine Polar Alignment successful");
                UpdateAlignStatus();
                OSNAlignPolarRealignSP.s = IPS_OK;
                return true;
            } else {
                IDSetSwitch(&OSNAlignPolarRealignSP, "Command for Refine Polar Alignment FAILED");
                UpdateAlignStatus();
                OSNAlignPolarRealignSP.s = IPS_ALERT;
                return false;
            }
        }
    }

#ifdef ONSTEP_NOTDONE
    if (!strcmp(name, OSOutput1SP.name))      // 
    {
        if (OSOutput1S[0].s == ISS_ON)
        {
            OSDisableOutput(1);
            //PECStateS[0].s == ISS_OFF;
        } else if (OSOutput1S[1].s == ISS_ON)
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
        } else if (OSOutput2S[1].s == ISS_ON)
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

    }

    return LX200Generic::ISNewSwitch(dev, name, states, names, n);
}

void LX200_OnStep::getBasicData()
{
    // process parent
    LX200Generic::getBasicData();

    if (!isSimulation())
    {
        char buffer[128];
        getVersionDate(PortFD, buffer);
        IUSaveText(&VersionT[0], buffer);
        getVersionTime(PortFD, buffer);
        IUSaveText(&VersionT[1], buffer);
        getVersionNumber(PortFD, buffer);
        IUSaveText(&VersionT[2], buffer);
        getProductName(PortFD, buffer);
        IUSaveText(&VersionT[3], buffer);

        IDSetText(&VersionTP, nullptr);

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

    if(!getCommandString(PortFD, response, ":hQ#"))
    {
        LOGF_WARN("===CMD==> Set Park Pos %s", response);
        return false;
    }
    SetAxis1Park(currentRA);
    SetAxis2Park(currentDEC);
    LOG_WARN("Park Value set to current postion");
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
        if(!getCommandString(PortFD, response, ":hR#"))
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
        if (EqNP.s == IPS_BUSY)
        {
            if (!isSimulation() && abortSlew(PortFD) < 0)
            {
                Telescope::AbortSP.s = IPS_ALERT;
                IDSetSwitch(&(Telescope::AbortSP), "Abort slew failed.");
                return false;
            }
            Telescope::AbortSP.s = IPS_OK;
            EqNP.s    = IPS_IDLE;
            IDSetSwitch(&(Telescope::AbortSP), "Slew aborted.");
            IDSetNumber(&EqNP, nullptr);

            if (MovementNSSP.s == IPS_BUSY || MovementWESP.s == IPS_BUSY)
            {
                MovementNSSP.s = MovementWESP.s = IPS_IDLE;
                EqNP.s                          = IPS_IDLE;
                IUResetSwitch(&MovementNSSP);
                IUResetSwitch(&MovementWESP);

                IDSetSwitch(&MovementNSSP, nullptr);
                IDSetSwitch(&MovementWESP, nullptr);
            }
        }
        if (!isSimulation() && slewToPark(PortFD) < 0)
        {
            ParkSP.s = IPS_ALERT;
            IDSetSwitch(&ParkSP, "Parking Failed.");
            return false;
        }
    }
    ParkSP.s   = IPS_BUSY;
    return true;
}

// Periodically Polls OnStep Parameter from controller
bool LX200_OnStep::ReadScopeStatus()
{
    char OSbacklashDEC[RB_MAX_LEN];
    char OSbacklashRA[RB_MAX_LEN];
    char GuideValue[RB_MAX_LEN];
    char TempValue[RB_MAX_LEN];
    char TempValue2[RB_MAX_LEN];
    Errors Lasterror = ERR_NONE;
    
    if (isSimulation()) //if Simulation is selected
    {
        mountSim();
        return true;
    }
    
    if (getLX200RA(PortFD, &currentRA) < 0 || getLX200DEC(PortFD, &currentDEC) < 0) // Update actual position
    {
        EqNP.s = IPS_ALERT;
        IDSetNumber(&EqNP, "Error reading RA/DEC.");
        return false;
    }
    
    #ifdef OnStep_Alpha    
    OSSupports_bitfield_Gu = try_bitfield_Gu();
    
    if (!OSSupports_bitfield_Gu) {
        //Fall back to :GU parsing
        #endif    
        getCommandString(PortFD,OSStat,":GU#"); // :GU# returns a string containg controller status
        if (strcmp(OSStat,OldOSStat) != 0)  //if status changed
        {
            // ============= Telescope Status
            strcpy(OldOSStat ,OSStat);
            
            IUSaveText(&OnstepStat[0],OSStat);
            if (strstr(OSStat,"n") && strstr(OSStat,"N"))
            {
                IUSaveText(&OnstepStat[1], "Idle");
                TrackState = SCOPE_IDLE;
            }
            if (strstr(OSStat, "n") && !strstr(OSStat, "N"))
            {
                IUSaveText(&OnstepStat[1], "Slewing");
                TrackState = SCOPE_SLEWING;
            }
            if (strstr(OSStat, "N") && !strstr(OSStat, "n"))
            {
                IUSaveText(&OnstepStat[1], "Tracking");
                TrackState = SCOPE_TRACKING;
            }
            
            // ============= Refractoring
            if ((strstr(OSStat,"r") || strstr(OSStat,"t"))) //On, either refractory only (r) or full (t)
            {
                if (strstr(OSStat,"t")) {IUSaveText(&OnstepStat[2],"Full Comp"); }
                if (strstr(OSStat,"r")) { IUSaveText(&OnstepStat[2],"Refractory Comp"); }
                if (strstr(OSStat,"s")) {
                    IUSaveText(&OnstepStat[8],"Single Axis"); 
                } else { 
                    IUSaveText(&OnstepStat[8],"2-Axis"); 
                }
            } else {
                
                IUSaveText(&OnstepStat[2],"Refractoring Off");
                IUSaveText(&OnstepStat[8],"N/A");
            }
            
            
            // ============= Parkstatus
            if(FirstRead)   // it is the first time I read the status so I need to update
            {
                if (strstr(OSStat,"P"))
                {
                    SetParked(true); //defaults to TrackState=SCOPE_PARKED
                    IUSaveText(&OnstepStat[3],"Parked");
                }
                if (strstr(OSStat,"F"))
                {
                    SetParked(false); // defaults to TrackState=SCOPE_IDLE
                    IUSaveText(&OnstepStat[3],"Parking Failed");
                }
                if (strstr(OSStat,"I"))
                {
                    SetParked(false); //defaults to TrackState=SCOPE_IDLE but we want
                    TrackState=SCOPE_PARKING;
                    IUSaveText(&OnstepStat[3],"Park in Progress");
                }
                if (strstr(OSStat,"p"))
                {
                    SetParked(false); //defaults to TrackState=SCOPE_IDLE but we want
                    if (strstr(OSStat,"nN"))    // azwing need to detect if unparked idle or tracking
                    {
                        IUSaveText(&OnstepStat[1],"Idle");
                        TrackState=SCOPE_IDLE;
                    }
                    else TrackState=SCOPE_TRACKING;
                    IUSaveText(&OnstepStat[3],"UnParked");
                }
                FirstRead=false;
            }
            else
            {
                if (!isParked())
                {
                    if(strstr(OSStat,"P"))
                    {
                        SetParked(true);
                        IUSaveText(&OnstepStat[3],"Parked");
                        //LOG_INFO("OnStep Parking Succeded");
                    }
                    if (strstr(OSStat,"I"))
                    {
                        SetParked(false); //defaults to TrackState=SCOPE_IDLE but we want
                        TrackState=SCOPE_PARKING;
                        IUSaveText(&OnstepStat[3],"Park in Progress");
                        LOG_INFO("OnStep Parking in Progress...");
                    }
                }
                if (isParked())
                {
                    if (strstr(OSStat,"F"))
                    {
                        // keep Status even if error  TrackState=SCOPE_IDLE;
                        SetParked(false); //defaults to TrackState=SCOPE_IDLE
                        IUSaveText(&OnstepStat[3],"Parking Failed");
                        LOG_ERROR("OnStep Parking failed, need to re Init OnStep at home");
                    }
                    if (strstr(OSStat,"p"))
                    {
                        SetParked(false); //defaults to TrackState=SCOPE_IDLE but we want
                        if (strstr(OSStat,"nN"))    // azwing need to detect if unparked idle or tracking
                        {
                            IUSaveText(&OnstepStat[1],"Idle");
                            TrackState=SCOPE_IDLE;
                        }
                        else TrackState=SCOPE_TRACKING;
                        IUSaveText(&OnstepStat[3],"UnParked");
                        //LOG_INFO("OnStep Unparked...");
                    }
                }
            }
            
            
            //if (strstr(OSStat,"H")) { IUSaveText(&OnstepStat[3],"At Home"); }
            if (strstr(OSStat,"H") && strstr(OSStat,"P"))
            {
                IUSaveText(&OnstepStat[3],"At Home and Parked");
            }
            if (strstr(OSStat,"H") && strstr(OSStat,"p"))
            {
                IUSaveText(&OnstepStat[3],"At Home and UnParked");
            }
            //AutoPauseAtHome
            if (strstr(OSStat, "u")){ //  pa[u]se at home enabled?
                HomePauseS[1].s = ISS_ON;
                HomePauseSP.s = IPS_OK;
                IDSetSwitch(&HomePauseSP, "Pause at Home Enabled");
            } else {
                HomePauseS[0].s=ISS_ON;
                HomePauseSP.s = IPS_OK;
                IDSetSwitch(&HomePauseSP, nullptr);
            }    
            
            if (strstr(OSStat,"w")) { IUSaveText(&OnstepStat[3],"Waiting at Home"); }
            
            // ============= Pec Status
            if (!strstr(OSStat,"R") && !strstr(OSStat,"W")) { IUSaveText(&OnstepStat[4],"N/A"); }
            if (strstr(OSStat,"R")) { IUSaveText(&OnstepStat[4],"Recorded"); }
            if (strstr(OSStat,"W")) { IUSaveText(&OnstepStat[4],"Autorecord"); }
            
            // ============= Time Sync Status
            if (!strstr(OSStat,"S")) { IUSaveText(&OnstepStat[5],"N/A"); }
            if (strstr(OSStat,"S")) { IUSaveText(&OnstepStat[5],"PPS / GPS Sync Ok"); }
            
            // ============= Mount Types
            if (strstr(OSStat,"E")) { IUSaveText(&OnstepStat[6],"German Mount"); OSMountType = 0;}
            if (strstr(OSStat,"K")) { IUSaveText(&OnstepStat[6],"Fork Mount"); OSMountType = 1;}
            if (strstr(OSStat,"k")) { IUSaveText(&OnstepStat[6],"Fork Alt Mount"); OSMountType = 2;}
            if (strstr(OSStat,"A")) { IUSaveText(&OnstepStat[6],"AltAZ Mount"); OSMountType = 3; }
            
            // ============= Error Code 
            //From OnStep: ERR_NONE, ERR_MOTOR_FAULT, ERR_ALT_MIN, ERR_LIMIT_SENSE, ERR_DEC, ERR_AZM, ERR_UNDER_POLE, ERR_MERIDIAN, ERR_SYNC, ERR_PARK, ERR_GOTO_SYNC, ERR_UNSPECIFIED, ERR_ALT_MAX, ERR_GOTO_ERR_NONE, ERR_GOTO_ERR_BELOW_HORIZON, ERR_GOTO_ERR_ABOVE_OVERHEAD, ERR_GOTO_ERR_STANDBY, ERR_GOTO_ERR_PARK, ERR_GOTO_ERR_GOTO, ERR_GOTO_ERR_OUTSIDE_LIMITS, ERR_GOTO_ERR_HARDWARE_FAULT, ERR_GOTO_ERR_IN_MOTION, ERR_GOTO_ERR_UNSPECIFIED
            
            //For redoing this, quick python, if needed (will only give the error name):
            //all_errors=' (Insert)
            //split_errors=all_errors.split(', ')
            //for specific in split_errors:
            //     print('case ' +specific+':')                          
            //     print('\tIUSaveText(&OnstepStat[7],"'+specific+'");')
            //     print('\tbreak;')  
            
            
            Lasterror=(Errors)(OSStat[strlen(OSStat)-1]-'0');
        }
        
        #ifdef OnStep_Alpha // For the moment, for :Gu
    } else {
        getCommandString(PortFD,OSStat,":Gu#"); // :Gu# returns a string containg controller status that's bitpacked
        if (strcmp(OSStat,OldOSStat) != 0)  //if status changed
        { 
            //Ignored for now.     
        }
        //Byte 0: Current Status
        if (OSStat[0] & 0b10000001 == 0b10000001) {
            //Not tracking
        }
        if (OSStat[0] & 0b10000010 == 0b10000010) {
            //No goto
        }
        if (OSStat[0] & 0b10000100 == 0b10000100) {
            // PPS sync
            IUSaveText(&OnstepStat[5],"PPS / GPS Sync Ok");
        } else {
            IUSaveText(&OnstepStat[5],"N/A");
        }
        if (OSStat[0] & 0b10001000 == 0b10001000) {
            // Guide active
        }
        //Refraction and Number of axis handled differently for now, might combine to one variable. 
        // reply[0]|=0b11010000;                       // Refr enabled Single axis
        // reply[0]|=0b10010000;                       // Refr enabled
        // reply[0]|=0b11100000;                       // OnTrack enabled Single axis
        // reply[0]|=0b10100000;                       // OnTrack enabled
        if ((OSStat[0] & 0b10010000 == 0b10010000 || OSStat[0] & 0b10100000 == 0b10100000)) //On, either refractory only (r) or full (t)
        {
            if (OSStat[0] & 0b10100000 == 0b10100000) { IUSaveText(&OnstepStat[2],"Full Comp"); }
            if (OSStat[0] & 0b10010000 == 0b10010000) { IUSaveText(&OnstepStat[2],"Refractory Comp"); }
            if (OSStat[0] & 0b11000000 == 0b11000000) {
                IUSaveText(&OnstepStat[8],"Single Axis"); 
            } else { 
                IUSaveText(&OnstepStat[8],"2-Axis"); 
            }
        } else {
            IUSaveText(&OnstepStat[2],"Refractoring Off");
            IUSaveText(&OnstepStat[8],"N/A");
        }
        //Byte 1: Standard tracking rates
        if (OSStat[1] & 0b10000001 == 0b10000001) {
            // Lunar rate selected
        }
        if (OSStat[1] & 0b10000010 == 0b10000010) {
            // Solar rate selected
        }
        if (OSStat[1] & 0b10000011 == 0b10000011) {
            // King rate selected
        }
        //Byte 2: Flags
        if (OSStat[2] & 0b10000001 == 0b10000001) {
            // At home
        }
        if (OSStat[2] & 0b10000010 == 0b10000010) {
            // Waiting at home
            IUSaveText(&OnstepStat[3],"Waiting at Home");
        }
        if (OSStat[2] & 0b10000100 == 0b10000100) {
            // Pause at home enabled?
            //AutoPauseAtHome
            HomePauseS[1].s = ISS_ON;
            HomePauseSP.s = IPS_OK;
            IDSetSwitch(&HomePauseSP, "Pause at Home Enabled");
        } else {
            HomePauseS[0].s=ISS_ON;
            HomePauseSP.s = IPS_OK;
            IDSetSwitch(&HomePauseSP, nullptr);
        }
        if (OSStat[2] & 0b10001000 == 0b10001000) {
            // Buzzer enabled?
        }
        if (OSStat[2] & 0b10010000 == 0b10010000) {
            // Auto meridian flip
            AutoFlipS[1].s = ISS_ON;
            AutoFlipSP.s = IPS_OK;
            IDSetSwitch(&AutoFlipSP, nullptr);
        } else {
            AutoFlipS[0].s=ISS_ON;
            AutoFlipSP.s = IPS_OK;
            IDSetSwitch(&AutoFlipSP, nullptr);
        }
        if (OSStat[2] & 0b10100000 == 0b10100000) {
            // PEC data has been recorded
        }
        
        
        
        
        //Byte 3: Mount type and info
        if (OSStat[3] & 0b10000001 == 0b10000001) {
            // GEM
            IUSaveText(&OnstepStat[6],"German Mount"); 
            OSMountType = 0;
        }
        if (OSStat[3] & 0b10000010 == 0b10000010) {
            // FORK
            IUSaveText(&OnstepStat[6],"Fork Mount"); 
            OSMountType = 1;
        }
        if (OSStat[3] & 0b10000100 == 0b10000100) {
            // Fork Alt
            IUSaveText(&OnstepStat[6],"Fork Alt Mount"); 
            OSMountType = 2;
        }
        if (OSStat[3] & 0b10001000 == 0b10001000) {
            // ALTAZM
            IUSaveText(&OnstepStat[6],"AltAZ Mount"); 
            OSMountType = 3;
        }
        
        
        setPierSide(PIER_UNKNOWN);
        if (OSStat[3] & 0b10010000 == 0b10010000) {
            // Pier side none
            setPierSide(PIER_UNKNOWN);
            // INDI doesn't account for 'None'
        }
        if (OSStat[3] & 0b10100000 == 0b10100000) {
            // Pier side east
            setPierSide(PIER_EAST);
        }
        if (OSStat[3] & 0b11000000 == 0b11000000) {
            // Pier side west
            setPierSide(PIER_WEST);
        }
        //     Byte 4: PEC
        PECStatusGU = OSStat[4] & 0b01111111;
        if (OSStat[4] == 0) {
            // AltAZM, no PEC possible 
            PECStatusGU=0;
        } else {
            //    PEC status: 0 ignore, 1 get ready to play, 2 playing, 3 get ready to record, 4 recording
            
            
        }
        ParkStatusGU = OSStat[5] & 0b01111111;
        PulseGuideGU = OSStat[6] & 0b01111111;
        GuideRateGU = OSStat[7] & 0b01111111;
        LastError = OSStat[8] & 0b01111111;
    }
    #endif
    
    
    switch (Lasterror) {
        case ERR_NONE:
            IUSaveText(&OnstepStat[7],"None"); 
            break;
        case ERR_MOTOR_FAULT:
            IUSaveText(&OnstepStat[7],"Motor/Driver Fault"); 
            break;
        case ERR_ALT_MIN:
            IUSaveText(&OnstepStat[7],"Below Horizon Limit"); 
            break;
        case ERR_LIMIT_SENSE:
            IUSaveText(&OnstepStat[7],"Limit Sense"); 
            break;
        case ERR_DEC:
            IUSaveText(&OnstepStat[7],"Dec Limit Exceeded"); 
            break;
        case ERR_AZM:
            IUSaveText(&OnstepStat[7],"Azm Limit Exceeded"); 
            break;
        case ERR_UNDER_POLE:
            IUSaveText(&OnstepStat[7],"Under Pole Limit Exceeded"); 
            break;
        case ERR_MERIDIAN:
            IUSaveText(&OnstepStat[7],"Meridian Limit (W) Exceeded"); 
            break;
        case ERR_SYNC:
            IUSaveText(&OnstepStat[7],"Sync Safety Limit Exceeded"); 
            break;
        case ERR_PARK:
            IUSaveText(&OnstepStat[7],"Park Failed"); 
            break;
        case ERR_GOTO_SYNC:
            IUSaveText(&OnstepStat[7],"Goto Sync Failed"); 
            break;
        case ERR_UNSPECIFIED:
            IUSaveText(&OnstepStat[7],"Unspecified Error"); 
            break;
        case ERR_ALT_MAX:
            IUSaveText(&OnstepStat[7],"Above Overhead Limit"); 
            break;
        case ERR_GOTO_ERR_NONE:
            IUSaveText(&OnstepStat[7],"Goto No Error"); 
            break;
        case ERR_GOTO_ERR_BELOW_HORIZON:
            IUSaveText(&OnstepStat[7],"Goto Below Horizon"); 
            break;
        case ERR_GOTO_ERR_ABOVE_OVERHEAD:
            IUSaveText(&OnstepStat[7],"Goto Abv Overhead"); 
            break;
        case ERR_GOTO_ERR_STANDBY:
            IUSaveText(&OnstepStat[7],"Goto Err Standby"); 
            break;
        case ERR_GOTO_ERR_PARK:
            IUSaveText(&OnstepStat[7],"Goto Err Park"); 
            break;
        case ERR_GOTO_ERR_GOTO:
            IUSaveText(&OnstepStat[7],"Goto Err Goto"); 
            break;
        case ERR_GOTO_ERR_OUTSIDE_LIMITS:
            IUSaveText(&OnstepStat[7],"Goto Outside Limits"); 
            break;
        case ERR_GOTO_ERR_HARDWARE_FAULT:
            IUSaveText(&OnstepStat[7],"Goto H/W Fault"); 
            break;
        case ERR_GOTO_ERR_IN_MOTION:
            IUSaveText(&OnstepStat[7],"Goto Err Motion"); 
            break;
        case ERR_GOTO_ERR_UNSPECIFIED:
            IUSaveText(&OnstepStat[7],"Goto Unspecified Error"); 
            break;
        default:
            IUSaveText(&OnstepStat[7],"Unknown Error"); 
            break;
    }
    
    #ifndef OnStep_Alpha
    // Get actual Pier Side
    getCommandString(PortFD,OSPier,":Gm#");
    if (strcmp(OSPier, OldOSPier) !=0)  // any change ?
    {
        strcpy(OldOSPier, OSPier);
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
    #endif
    
    //========== Get actual Backlash values
    getCommandString(PortFD,OSbacklashDEC, ":%BD#");
    getCommandString(PortFD,OSbacklashRA, ":%BR#");
    BacklashNP.np[0].value = atof(OSbacklashDEC);
    BacklashNP.np[1].value = atof(OSbacklashRA);
    IDSetNumber(&BacklashNP, nullptr);
    
    double pulseguiderate;
    getCommandString(PortFD, GuideValue, ":GX90#");
    //     LOGF_DEBUG("Guide Rate String: %s", GuideValue);
    pulseguiderate=atof(GuideValue);
    LOGF_DEBUG("Guide Rate: %f", pulseguiderate);
    GuideRateNP.np[0].value = pulseguiderate;
    GuideRateNP.np[1].value = pulseguiderate;
    IDSetNumber(&GuideRateNP, nullptr);
    
    
    #ifndef OnStep_Alpha
    //AutoFlip
    getCommandString(PortFD,TempValue,":GX95#");
    if (atoi(TempValue)) {
        AutoFlipS[1].s = ISS_ON;
        AutoFlipSP.s = IPS_OK;
        IDSetSwitch(&AutoFlipSP, nullptr);
    } else {
        AutoFlipS[0].s=ISS_ON;
        AutoFlipSP.s = IPS_OK;
        IDSetSwitch(&AutoFlipSP, nullptr);
    }
    #endif
    
    //PreferredPierSide
    getCommandString(PortFD,TempValue,":GX96#");
    if (strstr(TempValue,"W")) {
        PreferredPierSideS[0].s = ISS_ON;
        PreferredPierSideSP.s = IPS_OK;
        IDSetSwitch(&PreferredPierSideSP, nullptr);
    } else if (strstr(TempValue,"E")) {
        PreferredPierSideS[1].s=ISS_ON;
        PreferredPierSideSP.s = IPS_OK;
        IDSetSwitch(&PreferredPierSideSP, nullptr);
    } else if (strstr(TempValue,"B")) {
        PreferredPierSideS[2].s=ISS_ON;
        PreferredPierSideSP.s = IPS_OK;
        IDSetSwitch(&PreferredPierSideSP, nullptr);
    } else {
        IUResetSwitch(&PreferredPierSideSP);
        PreferredPierSideSP.s = IPS_BUSY;
        IDSetSwitch(&PreferredPierSideSP, nullptr);
    }
    
    
    getCommandString(PortFD,TempValue, ":GXE9#"); // E
    getCommandString(PortFD,TempValue2, ":GXEA#"); // W 
    minutesPastMeridianNP.np[0].value = atof(TempValue); // E
    minutesPastMeridianNP.np[1].value = atof(TempValue2); //W
    IDSetNumber(&minutesPastMeridianNP, nullptr);
    
    // Update OnStep Status TAB
    IDSetText(&OnstepStatTP, nullptr);
    //Align tab, so it doesn't conflict
    //May want to reduce frequency of updates 
    if (!UpdateAlignStatus()) LOG_WARN("Fail Align Command");
    UpdateAlignErr();
    
    
    OSUpdateFocuser();  // Update Focuser Position
    #ifndef OnStep_Alpha
    PECStatus(0);
    //#Gu# has this built in
    #endif
    
    
    NewRaDec(currentRA, currentDEC);
    return true;
}


bool LX200_OnStep::SetTrackEnabled(bool enabled) //track On/Off events handled by inditelescope       Tested
{
    char response[RB_MAX_LEN];

    if (enabled)
    {
        if(!getCommandString(PortFD, response, ":Te#"))
        {
            LOGF_ERROR("===CMD==> Track On %s", response);
            return false;
        }
    }
    else
    {
        if(!getCommandString(PortFD, response, ":Td#"))
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
    char cmd[32];

    snprintf(cmd, 32, ":SC%02d/%02d/%02d#", months, days, years);

    if (!sendOnStepCommand(cmd)) return true;
    return false;
}

bool LX200_OnStep::sendOnStepCommandBlind(const char *cmd)
{
    int error_type;
    int nbytes_write = 0;

    DEBUGF(DBG_SCOPE, "CMD <%s>", cmd);

    tcflush(PortFD, TCIFLUSH);

    if ((error_type = tty_write_string(PortFD, cmd, &nbytes_write)) != TTY_OK)
        return error_type;

    return 1;
}

bool LX200_OnStep::sendOnStepCommand(const char *cmd)
{
    char response[1];
    int error_type;
    int nbytes_write = 0, nbytes_read = 0;

    DEBUGF(DBG_SCOPE, "CMD <%s>", cmd);

    tcflush(PortFD, TCIFLUSH);

    if ((error_type = tty_write_string(PortFD, cmd, &nbytes_write)) != TTY_OK)
        return error_type;

    error_type = tty_read(PortFD, response, 1, ONSTEP_TIMEOUT, &nbytes_read);

    tcflush(PortFD, TCIFLUSH);

    if (nbytes_read < 1)
    {
        LOG_ERROR("Unable to parse response.");
        return error_type;
    }

    return (response[0] == '0');
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
    fs_sexa(l, latitude, 3, 3600);
    fs_sexa(L, longitude, 4, 3600);

    LOGF_INFO("Site location updated to Lat %.32s - Long %.32s", l, L);

    return true;
}

int LX200_OnStep::setMaxElevationLimit(int fd, int max)   // According to standard command is :SoDD*#       Tested
{
    LOGF_INFO("<%s>", __FUNCTION__);

    char read_buffer[RB_MAX_LEN] = {0};

    snprintf(read_buffer, sizeof(read_buffer), ":So%02d#", max);

    return (setStandardProcedure(fd, read_buffer));
}

int LX200_OnStep::setSiteLongitude(int fd, double Long)
{
    //DEBUGFDEVICE(lx200Name, DBG_SCOPE, "<%s>", __FUNCTION__);
    int d, m, s;
    char read_buffer[32];

    getSexComponents(Long, &d, &m, &s);

    snprintf(read_buffer, sizeof(read_buffer), ":Sg%.03d:%02d#", d, m);

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

IPState LX200_OnStep::MoveAbsFocuser (uint32_t targetTicks) {
    //  :FSsnnn#  Set focuser target position (in microns)
    //            Returns: Nothing
    if (FocusAbsPosN[0].max >= int(targetTicks) && FocusAbsPosN[0].min <= int(targetTicks)) {
        char read_buffer[32];
        snprintf(read_buffer, sizeof(read_buffer), ":FS%06d#", int(targetTicks));
        sendOnStepCommandBlind(read_buffer);
        return IPS_BUSY; // Normal case, should be set to normal by update.
    } else {
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
    char cmd[8];
    strcpy(cmd, ":FQ#");
    return sendOnStepCommandBlind(cmd);
}

void LX200_OnStep::OSUpdateFocuser()
{
    char value[RB_MAX_LEN];
    double current = 0;
    if (OSFocuser1) 
    {
        // Alternate option:
        //if (!sendOnStepCommand(":FA#")) {
        getCommandString(PortFD, value, ":FG#");
        FocusAbsPosN[0].value =  atoi(value);
        current = FocusAbsPosN[0].value;
        IDSetNumber(&FocusAbsPosNP, nullptr);
        LOGF_DEBUG("Current focuser: %d, %d", atoi(value), FocusAbsPosN[0].value);
        //  :FT#  get status
        //         Returns: M# (for moving) or S# (for stopped)
        getCommandString(PortFD, value, ":FT#");
        if (value[0] == 'S') 
        {
            FocusRelPosNP.s = IPS_OK;
            IDSetNumber(&FocusRelPosNP, nullptr);
            FocusAbsPosNP.s = IPS_OK;
            IDSetNumber(&FocusAbsPosNP, nullptr);
        } 
        else if (value[0] == 'M') 
        {
            FocusRelPosNP.s = IPS_BUSY;
            IDSetNumber(&FocusRelPosNP, nullptr);
            FocusAbsPosNP.s = IPS_BUSY;
            IDSetNumber(&FocusAbsPosNP, nullptr);
        }
        else 
        { //INVALID REPLY
            FocusRelPosNP.s = IPS_ALERT;
            IDSetNumber(&FocusRelPosNP, nullptr);
            FocusAbsPosNP.s = IPS_ALERT;
            IDSetNumber(&FocusAbsPosNP, nullptr);
        }
        //  :FM#  Get max position (in microns)
        //         Returns: n#
        getCommandString(PortFD, value, ":FM#");
        FocusAbsPosN[0].max   = atoi(value);
        IUUpdateMinMax(&FocusAbsPosNP);
        IDSetNumber(&FocusAbsPosNP, nullptr);
        //  :FI#  Get full in position (in microns)
        //         Returns: n#
        getCommandString(PortFD, value, ":FI#");
        FocusAbsPosN[0].min =  atoi(value);
        IUUpdateMinMax(&FocusAbsPosNP);
        IDSetNumber(&FocusAbsPosNP, nullptr);
        FI::updateProperties();
        LOGF_DEBUG("After update proerties: FocusAbsPosN min: %f max: %f", FocusAbsPosN[0].min, FocusAbsPosN[0].max);
    } 
    
    
    if(OSFocuser2)
    {
        getCommandString(PortFD, value, ":fG#");
        OSFocus2TargNP.np[0].value = atoi(value);
        IDSetNumber(&OSFocus2TargNP, nullptr);
    }
}

//PEC Support
//Should probably be added to inditelescope or another interface, because the PEC that's there... is very limited.

IPState LX200_OnStep::StartPECPlayback (int axis) 
{
    //  :$QZ+  Enable RA PEC compensation 
    //         Returns: nothing
    INDI_UNUSED(axis); //We only have RA on OnStep
    if (OSPECEnabled == true) 
    { 
        char cmd[8];
        LOG_INFO("Sending Command to Start PEC Playback");
        strcpy(cmd, ":$QZ+#");
        sendOnStepCommandBlind(cmd);
        return IPS_BUSY;
    }
    else
    {
        LOG_DEBUG("Command to Playback PEC called when Controller does not support PEC");
    }
    return IPS_ALERT;
}

IPState LX200_OnStep::StopPECPlayback (int axis)
{
    //  :$QZ-  Disable RA PEC Compensation
    //         Returns: nothing
    INDI_UNUSED(axis); //We only have RA on OnStep
    if (OSPECEnabled == true) 
    {
        char cmd[8];
        LOG_INFO("Sending Command to Stop PEC Playback");
        strcpy(cmd, ":$QZ-#");
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
        char cmd[8];
        LOG_INFO("Sending Command to Start PEC record");
        strcpy(cmd, ":$QZ/#");
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
        char cmd[8];
        LOG_INFO("Sending Command to Clear PEC record");
        strcpy(cmd, ":$QZZ#");
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
    if (OSPECEnabled == true) {
        char cmd[8];
        LOG_INFO("Sending Command to Save PEC to EEPROM");
        strcpy(cmd, ":$QZ!#");
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
    INDI_UNUSED(axis); //We only have RA on OnStep
    if (OSPECEnabled == true) {
        //LOG_INFO("Getting PEC Status");
        //  :$QZ?  Get PEC status
        //         Returns: S#
        // Returns status (pecSense) In the form: Status is one of "IpPrR" (I)gnore, get ready to (p)lay, (P)laying, get ready to (r)ecord, (R)ecording.  Or an optional (.) to indicate an index detect. 
        // IUFillSwitch(&OSPECStatusS[0], "OFF", "OFF", ISS_ON);
        // IUFillSwitch(&OSPECStatusS[1], "Playing", "Playing", ISS_OFF);
        // IUFillSwitch(&OSPECStatusS[2], "Recording", "Recording", ISS_OFF);
        // IUFillSwitch(&OSPECStatusS[3], "Will Play", "Will Play", ISS_OFF);
        // IUFillSwitch(&OSPECStatusS[4], "Will Record", "Will Record", ISS_OFF);
        char value[RB_MAX_LEN] ="  ";
        OSPECStatusSP.s = IPS_BUSY;
        getCommandString(PortFD, value, ":$QZ?#");
        // LOGF_INFO("Response %s", value);
        // LOGF_INFO("Response %d", value[0]);
        // LOGF_INFO("Response %d", value[1]);
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
            OSPECEnabled = false;
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
        // LOG_DEBUG("PEC status called when Controller does not support PEC");
    }
    return IPS_ALERT;
}


IPState LX200_OnStep::ReadPECBuffer (int axis)
{
    INDI_UNUSED(axis); //We only have RA on OnStep
    if (OSPECEnabled == true)
    {
        LOG_ERROR("PEC Reading NOT Implemented");
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
        LOG_ERROR("PEC Writing NOT Implemented");
        return IPS_OK;
    }
    else
    {
        LOG_DEBUG("Command to Read PEC called when Controller does not support PEC");
    }
    return IPS_ALERT;
}

// New, Multistar alignment goes here: 

IPState LX200_OnStep::AlignStartGeometric (int stars){
    //See here https://groups.io/g/onstep/message/3624
    char cmd[8];
    
    LOG_INFO("Sending Command to Start Alignment");
    IUSaveText(&OSNAlignT[0], "Align STARTED");
    IUSaveText(&OSNAlignT[1], "GOTO a star, center it");
    IUSaveText(&OSNAlignT[2], "GOTO a star, Solve and Sync");
    IUSaveText(&OSNAlignT[3], "Press 'Issue Align' if not solving");
    IDSetText(&OSNAlignTP, "==>Align Started");
    // Check for max number of stars and gracefully fall back to max, if more are requested.
    char read_buffer[RB_MAX_LEN];
    if(getCommandString(PortFD, read_buffer, ":A?#"))
    {
        LOGF_INFO("Getting Max Star: response Error, response = %s>", read_buffer);
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
    LOGF_INFO("Started Align with %s, max possible: %d", cmd, max_stars);
    if(sendOnStepCommand(cmd))
    {
        LOG_INFO("Starting Align failed");
        return IPS_BUSY;
    }
    return IPS_ALERT;
}


IPState LX200_OnStep::AlignAddStar (){
    //See here https://groups.io/g/onstep/message/3624
    char cmd[8];
    LOG_INFO("Sending Command to Record Star");
    strcpy(cmd, ":A+#");
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
    //               n is the current alignment star (0 otherwise)
    //               o is the last required alignment star when an alignment is in progress (0 otherwise)
    
    char read_buffer[RB_MAX_LEN];
    char msg[40];
    char stars[5];
    
    int max_stars, current_star, align_stars;
    // LOG_INFO("Getting Align Status");
    if(getCommandString(PortFD, read_buffer, ":A?#"))
    {
        LOGF_INFO("Align Status response Error, response = %s>", read_buffer);
        return false;
    }
    //  LOGF_INFO("Getting Align Status: %s", read_buffer);
    max_stars = read_buffer[0] - '0';
    current_star = read_buffer[1] - '0';
    align_stars = read_buffer[2] - '0';
    snprintf(stars, sizeof(stars), "%d", max_stars);
    IUSaveText(&OSNAlignT[5], stars);
    snprintf(stars, sizeof(stars), "%d", current_star);
    IUSaveText(&OSNAlignT[6], stars);
    snprintf(stars, sizeof(stars), "%d", align_stars);
    IUSaveText(&OSNAlignT[7], stars);
    LOGF_DEBUG("Align: max_stars: %i current star: %u, align_stars %u", max_stars, current_star, align_stars);
    
    /* if (align_stars > max_stars) {
     * LOGF_ERROR("Failed Sanity check, can't have more stars than max: :A?# gives: %s", read_buffer);
     * return false;
}*/
    
    if (current_star <= align_stars)
    {
        snprintf(msg, sizeof(msg), "%s Manual Align: Star %d/%d", read_buffer, current_star, align_stars );
        IUSaveText(&OSNAlignT[4],msg);
    }
    if (current_star > align_stars && max_stars > 1)
    {
        LOGF_DEBUG("Align: current star: %u, align_stars %u", int(current_star), int(align_stars));
        snprintf(msg, sizeof(msg), "Manual Align: Completed");
        AlignDone();
        IUSaveText(&OSNAlignT[4], msg);
        UpdateAlignErr();
    }
    IDSetText(&OSNAlignTP, nullptr);
    return true;
}

bool LX200_OnStep::UpdateAlignErr()
{
    //  :GXnn#   Get OnStep value
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



    char read_buffer[RB_MAX_LEN];
    char polar_error[40], sexabuf[20];
    //  IUFillText(&OSNAlignT[4], "4", "Current Status", "Not Updated");
    //  IUFillText(&OSNAlignT[5], "5", "Max Stars", "Not Updated");
    //  IUFillText(&OSNAlignT[6], "6", "Current Star", "Not Updated");
    //  IUFillText(&OSNAlignT[7], "7", "# of Align Stars", "Not Updated");

    // LOG_INFO("Gettng Align Error Status");
    if(getCommandString(PortFD, read_buffer, ":GX02#"))
    {
        LOGF_INFO("Polar Align Error Status response Error, response = %s>", read_buffer);
        return false;
    }
    //  LOGF_INFO("Getting Align Error Status: %s", read_buffer);

    long altCor = strtold(read_buffer, nullptr);
    if(getCommandString(PortFD, read_buffer, ":GX03#"))
    {
        LOGF_INFO("Polar Align Error Status response Error, response = %s>", read_buffer);
        return false;
    }
    //  LOGF_INFO("Getting Align Error Status: %s", read_buffer);

    long azmCor = strtold(read_buffer, nullptr);
    fs_sexa(sexabuf, (double)azmCor / 3600, 4, 3600);
    snprintf(polar_error, sizeof(polar_error), "%ld'' /%s", azmCor, sexabuf);
    IUSaveText(&OSNAlignErrT[1], polar_error);
    fs_sexa(sexabuf, (double)altCor / 3600, 4, 3600);
    snprintf(polar_error, sizeof(polar_error), "%ld'' /%s", altCor, sexabuf);
    IUSaveText(&OSNAlignErrT[0], polar_error);
    IDSetText(&OSNAlignErrTP, nullptr);


    return true;
}

IPState LX200_OnStep::AlignDone(){
    //See here https://groups.io/g/onstep/message/3624
    if (OSAlignCompleted == false) {
        OSAlignCompleted =true;
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

IPState LX200_OnStep::AlignWrite(){
    //See here https://groups.io/g/onstep/message/3624
    char cmd[8];
    LOG_INFO("Sending Command to Finish Alignment and write");
    strcpy(cmd, ":AW#");
    IUSaveText(&OSNAlignT[0], "Align FINISHED");
    IUSaveText(&OSNAlignT[1], "------");
    IUSaveText(&OSNAlignT[2], "And Written to EEPROM");
    IUSaveText(&OSNAlignT[3], "------");
    IDSetText(&OSNAlignTP, nullptr);
    if (sendOnStepCommandBlind(cmd))
    {
        return IPS_OK;
    }
    IUSaveText(&OSNAlignT[0],"Align WRITE FAILED");
    IDSetText(&OSNAlignTP, nullptr);
    return IPS_ALERT;
    
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
bool LX200_OnStep::OSGetValue(char selection[2]) {
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
    char value[64] ="  ";
    char command[64]=":$GXmm#";
    int error_type;
    command[4]=selection[0];
    command[5]=selection[1];
    //Should change to LOGF_DEBUG once tested
    LOGF_INFO("Command: %s", command);
    LOGF_INFO("Response: %s", command);
    if(getCommandString(PortFD, value, command) != TTY_OK) {
        return false;

}
*/

bool LX200_OnStep::OSGetOutputState(int output) {
    //  :GXnn#   Get OnStep value
    //         Returns: value
    // nn= G0-GF (HEX!) - Output status
    //
    char value[RB_MAX_LEN] ="  ";
    char command[64] = ":$GXGm#";
    LOGF_INFO("Output: %s", char(output));
    LOGF_INFO("Command: %s", command);
    command[5] = char(output);
    LOGF_INFO("Command: %s", command);
    getCommandString(PortFD, value, command);
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

bool LX200_OnStep::SetTrackRate(double raRate, double deRate)
{
    char read_buffer[RB_MAX_LEN];
    snprintf(read_buffer, sizeof(read_buffer), ":RA%04f#", raRate);
    LOGF_INFO("Setting: RA Rate to %04f", raRate);
    if (!sendOnStepCommand(read_buffer))
    {
        return false;
    }
    snprintf(read_buffer, sizeof(read_buffer), ":RE%04f#", deRate);
    LOGF_INFO("Setting: DE Rate to %04f", deRate);
    if (!sendOnStepCommand(read_buffer))
    {
        return false;
    }
    LOG_INFO("RA and DE Rates succesfully set");
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
            LOG_ERROR("OnStep slew/syncError: Controller in standby");
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
    EqNP.s = IPS_ALERT;
    IDSetNumber(&EqNP, nullptr);
}


//Override LX200 sync function, to allow for error returns
bool LX200_OnStep::Sync(double ra, double dec)
{
    
    char read_buffer[RB_MAX_LEN]={0};
    int error_code;
    
    if (!isSimulation()) {
        if (setObjectRA(PortFD, ra) < 0 || (setObjectDEC(PortFD, dec)) < 0)
        {
            EqNP.s = IPS_ALERT;
            IDSetNumber(&EqNP, "Error setting RA/DEC. Unable to Sync.");
            return false;
        }
        LOG_DEBUG("CMD <:CM#>");
        getCommandString(PortFD, read_buffer, ":CM#");
        LOGF_DEBUG("RES <%s>", read_buffer);
        if (strcmp(read_buffer,"N/A"))
        {
            error_code = read_buffer[1] - '0';
            LOGF_DEBUG("Sync failed with response: %s, Error code: %i", read_buffer, error_code);
            slewError(error_code);
            EqNP.s = IPS_ALERT;
            IDSetNumber(&EqNP, "Synchronization failed.");
            return false;
        }
    }
    
    currentRA  = ra;
    currentDEC = dec;
    
    LOG_INFO("OnStep: Synchronization successful.");
    
    EqNP.s     = IPS_OK;
    
    NewRaDec(currentRA, currentDEC);
    
    return true;
}
