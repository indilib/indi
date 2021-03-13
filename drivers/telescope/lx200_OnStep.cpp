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
#define ENVIRONMENT_TAB "Weather"

#define ONSTEP_TIMEOUT  3
#define RA_AXIS     0
#define DEC_AXIS    1

LX200_OnStep::LX200_OnStep() : LX200Generic(), WI(this)
{
    currentCatalog    = LX200_STAR_C;
    currentSubCatalog = 0;

    setVersion(1, 9);   // don't forget to update libindi/drivers.xml

    setLX200Capability(LX200_HAS_TRACKING_FREQ | LX200_HAS_SITES | LX200_HAS_ALIGNMENT_TYPE | LX200_HAS_PULSE_GUIDING |
                       LX200_HAS_PRECISE_TRACKING_FREQ);

    SetTelescopeCapability(GetTelescopeCapability() | TELESCOPE_CAN_CONTROL_TRACK | TELESCOPE_HAS_PEC | TELESCOPE_HAS_PIER_SIDE
                           | TELESCOPE_HAS_TRACK_RATE, 10 );

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
    WI::initProperties(ENVIRONMENT_TAB, ENVIRONMENT_TAB);
    SetParkDataType(PARK_RA_DEC);

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
    ReticSP[0].fill("PLUS", "Light", ISS_OFF);
    ReticSP[1].fill("MOINS", "Dark", ISS_OFF);
    ReticSP.fill(getDeviceName(), "RETICULE_BRIGHTNESS", "Reticule +/-", MAIN_CONTROL_TAB, IP_RW,
                       ISR_ATMOST1, 60, IPS_IDLE);

    ElevationLimitNP[0].fill("minAlt", "Elev Min", "%+03f", -90.0, 90.0, 1.0, -30.0);
    ElevationLimitNP[1].fill("maxAlt", "Elev Max", "%+03f", -90.0, 90.0, 1.0, 89.0);
    ElevationLimitNP.fill(getDeviceName(), "Slew elevation Limit", "", MAIN_CONTROL_TAB,
                       IP_RW, 0, IPS_IDLE);

    ObjectInfoTP[0].fill("Info", "", "");
    ObjectInfoTP.fill(getDeviceName(), "Object Info", "", MAIN_CONTROL_TAB, IP_RO, 0, IPS_IDLE);

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
    IUFillSwitchVector(&SlewRateSP, SlewRateS, nSlewRate, getDeviceName(), "TELESCOPE_SLEW_RATE", "Slew Rate", MOTION_TAB,
                       IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    MaxSlewRateNP[0].fill("maxSlew", "Rate", "%f", 0.0, 9.0, 1.0, 5.0);    //2.0, 9.0, 1.0, 9.0
    MaxSlewRateNP.fill(getDeviceName(), "Max slew Rate", "", MOTION_TAB, IP_RW, 0, IPS_IDLE);

    TrackCompSP[0].fill("1", "Full Compensation", ISS_OFF);
    TrackCompSP[1].fill("2", "Refraction", ISS_OFF);
    TrackCompSP[2].fill("3", "Off", ISS_ON);
    TrackCompSP.fill(getDeviceName(), "Compensation", "Compensation Tracking", MOTION_TAB, IP_RW,
                       ISR_1OFMANY, 0, IPS_IDLE);

    TrackAxisSP[0].fill("1", "Single Axis", ISS_OFF);
    TrackAxisSP[1].fill("2", "Dual Axis", ISS_OFF);
    TrackAxisSP.fill(getDeviceName(), "Multi-Axis", "Multi-Axis Tracking", MOTION_TAB, IP_RW,
                       ISR_1OFMANY, 0, IPS_IDLE);

    TrackAxisSP[0].fill("1", "Single Axis", ISS_OFF);
    TrackAxisSP[1].fill("2", "Dual Axis", ISS_OFF);
    TrackAxisSP.fill(getDeviceName(), "Multi-Axis", "Multi-Axis Tracking", MOTION_TAB, IP_RW,
                       ISR_1OFMANY, 0, IPS_IDLE);

    BacklashNP[0].fill("Backlash DEC", "DE", "%g", 0, 999, 1, 15);
    BacklashNP[1].fill("Backlash RA", "RA", "%g", 0, 999, 1, 15);
    BacklashNP.fill(getDeviceName(), "Backlash", "", MOTION_TAB, IP_RW, 0, IPS_IDLE);

    GuideRateNP[RA_AXIS].fill("GUIDE_RATE_WE", "W/E Rate", "%g", 0, 1, 0.25, 0.5);
    GuideRateNP[DEC_AXIS].fill("GUIDE_RATE_NS", "N/S Rate", "%g", 0, 1, 0.25, 0.5);
    GuideRateNP.fill(getDeviceName(), "GUIDE_RATE", "Guiding Rate", MOTION_TAB, IP_RO, 0,
                       IPS_IDLE);

    AutoFlipSP[0].fill("1", "AutoFlip: OFF", ISS_OFF);
    AutoFlipSP[1].fill("2", "AutoFlip: ON", ISS_OFF);
    AutoFlipSP.fill(getDeviceName(), "AutoFlip", "Meridian Auto Flip", MOTION_TAB, IP_RW,
                       ISR_1OFMANY, 0, IPS_IDLE);

    HomePauseSP[0].fill("1", "HomePause: OFF", ISS_OFF);
    HomePauseSP[1].fill("2", "HomePause: ON", ISS_OFF);
    HomePauseSP[2].fill("3", "HomePause: Continue", ISS_OFF);
    HomePauseSP.fill(getDeviceName(), "HomePause", "Pause at Home", MOTION_TAB, IP_RW,
                       ISR_1OFMANY, 0, IPS_IDLE);

    FrequencyAdjustSP[0].fill("1", "Frequency -", ISS_OFF);
    FrequencyAdjustSP[1].fill("2", "Frequency +", ISS_OFF);
    FrequencyAdjustSP[2].fill("3", "Reset Sidereal Frequency", ISS_OFF);
    FrequencyAdjustSP.fill(getDeviceName(), "FrequencyAdjust", "Frequency Adjust",
                       MOTION_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    PreferredPierSideSP[0].fill("1", "West", ISS_OFF);
    PreferredPierSideSP[1].fill("2", "East", ISS_OFF);
    PreferredPierSideSP[2].fill("3", "Best", ISS_OFF);
    PreferredPierSideSP.fill(getDeviceName(), "Preferred Pier Side",
                       "Preferred Pier Side", MOTION_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    minutesPastMeridianNP[0].fill("East", "East", "%g", 0, 180, 1, 30);
    minutesPastMeridianNP[1].fill("West", "West", "%g", 0, 180, 1, 30);
    minutesPastMeridianNP.fill(getDeviceName(), "Minutes Past Meridian",
                       "Minutes Past Meridian", MOTION_TAB, IP_RW, 0, IPS_IDLE);


    // ============== SITE_MANAGEMENT_TAB
    SetHomeSP[0].fill("RETURN_HOME", "Return  Home", ISS_OFF);
    SetHomeSP[1].fill("AT_HOME", "At Home (Reset)", ISS_OFF);
    SetHomeSP.fill(getDeviceName(), "HOME_INIT", "Homing", SITE_TAB, IP_RW, ISR_ATMOST1, 60,
                       IPS_IDLE);

    // ============== GUIDE_TAB

    // ============== FOCUS_TAB
    // Focuser 1

    OSFocus1InitializeSP[0].fill("Focus1_0", "Zero", ISS_OFF);
    OSFocus1InitializeSP[1].fill("Focus1_2", "Mid", ISS_OFF);
    //     OSFocus1InitializeSP[2].fill("Focus1_3", "max", ISS_OFF);
    OSFocus1InitializeSP.fill(getDeviceName(), "Foc1Rate", "Initialize", FOCUS_TAB,
                       IP_RW, ISR_ATMOST1, 0, IPS_IDLE);


    // Focuser 2
    //IUFillSwitch(&OSFocus2SelS[0], "Focus2_Sel1", "Foc 1", ISS_OFF);
    //IUFillSwitch(&OSFocus2SelS[1], "Focus2_Sel2", "Foc 2", ISS_OFF);
    //IUFillSwitchVector(&OSFocus2SelSP, OSFocus2SelS, 2, getDeviceName(), "Foc2Sel", "Foc 2", FOCUS_TAB, IP_RW, ISR_ATMOST1, 0, IPS_IDLE);

    OSFocus2MotionSP[0].fill("Focus2_In", "In", ISS_OFF);
    OSFocus2MotionSP[1].fill("Focus2_Out", "Out", ISS_OFF);
    OSFocus2MotionSP[2].fill("Focus2_Stop", "Stop", ISS_OFF);
    OSFocus2MotionSP.fill(getDeviceName(), "Foc2Mot", "Foc 2 Motion", FOCUS_TAB, IP_RW,
                       ISR_ATMOST1, 0, IPS_IDLE);

    OSFocus2RateSP[0].fill("Focus2_1", "min", ISS_OFF);
    OSFocus2RateSP[1].fill("Focus2_2", "0.01", ISS_OFF);
    OSFocus2RateSP[2].fill("Focus2_3", "0.1", ISS_OFF);
    OSFocus2RateSP[3].fill("Focus2_4", "1", ISS_OFF);
    OSFocus2RateSP.fill(getDeviceName(), "Foc2Rate", "Foc 2 Rates", FOCUS_TAB, IP_RW,
                       ISR_ATMOST1, 0, IPS_IDLE);

    OSFocus2TargNP[0].fill("FocusTarget2", "Abs Pos", "%g", -25000, 25000, 1, 0);
    OSFocus2TargNP.fill(getDeviceName(), "Foc2Targ", "Foc 2 Target", FOCUS_TAB, IP_RW, 0,
                       IPS_IDLE);

    // ============== FIRMWARE_TAB
    IUFillText(&VersionT[0], "Date", "", "");
    IUFillText(&VersionT[1], "Time", "", "");
    IUFillText(&VersionT[2], "Number", "", "");
    IUFillText(&VersionT[3], "Name", "", "");
    IUFillTextVector(&VersionTP, VersionT, 4, getDeviceName(), "Firmware Info", "", FIRMWARE_TAB, IP_RO, 0, IPS_IDLE);

    //PEC Tab
    OSPECStatusSP[0].fill("OFF", "OFF", ISS_OFF);
    OSPECStatusSP[1].fill("Playing", "Playing", ISS_OFF);
    OSPECStatusSP[2].fill("Recording", "Recording", ISS_OFF);
    OSPECStatusSP[3].fill("Will Play", "Will Play", ISS_OFF);
    OSPECStatusSP[4].fill("Will Record", "Will Record", ISS_OFF);
    OSPECStatusSP.fill(getDeviceName(), "PEC Status", "PEC Status", PEC_TAB, IP_RO,
                       ISR_ATMOST1, 0, IPS_IDLE);

    OSPECIndexSP[0].fill("Not Detected", "Not Detected", ISS_ON);
    OSPECIndexSP[1].fill("Detected", "Detected", ISS_OFF);
    OSPECIndexSP.fill(getDeviceName(), "PEC Index Detect", "PEC Index", PEC_TAB, IP_RO,
                       ISR_ATMOST1, 0, IPS_IDLE);

    OSPECRecordSP[0].fill("Clear", "Clear", ISS_OFF);
    OSPECRecordSP[1].fill("Record", "Record", ISS_OFF);
    OSPECRecordSP[2].fill("Write to EEPROM", "Write to EEPROM", ISS_OFF);
    OSPECRecordSP.fill(getDeviceName(), "PEC Operations", "PEC Recording", PEC_TAB, IP_RW,
                       ISR_ATMOST1, 0, IPS_IDLE);

    OSPECReadSP[0].fill("Read", "Read PEC to FILE****", ISS_OFF);
    OSPECReadSP[1].fill("Write", "Write PEC from FILE***", ISS_OFF);
    //    OSPECReadSP[2].fill("Write to EEPROM", "Write to EEPROM", ISS_OFF);
    OSPECReadSP.fill(getDeviceName(), "PEC File", "PEC File", PEC_TAB, IP_RW, ISR_ATMOST1, 0,
                       IPS_IDLE);
    // ============== New ALIGN_TAB
    // Only supports Alpha versions currently (July 2018) Now Beta (Dec 2018)
    OSNAlignStarsSP[0].fill("1", "1 Star", ISS_OFF);
    OSNAlignStarsSP[1].fill("2", "2 Stars", ISS_OFF);
    OSNAlignStarsSP[2].fill("3", "3 Stars", ISS_ON);
    OSNAlignStarsSP[3].fill("4", "4 Stars", ISS_OFF);
    OSNAlignStarsSP[4].fill("5", "5 Stars", ISS_OFF);
    OSNAlignStarsSP[5].fill("6", "6 Stars", ISS_OFF);
    OSNAlignStarsSP[6].fill("7", "7 Stars", ISS_OFF);
    OSNAlignStarsSP[7].fill("8", "8 Stars", ISS_OFF);
    OSNAlignStarsSP[8].fill("9", "9 Stars", ISS_OFF);
    OSNAlignStarsSP.fill(getDeviceName(), "AlignStars", "Align using some stars, Alpha only",
                       ALIGN_TAB, IP_RW, ISR_ATMOST1, 0, IPS_IDLE);

    OSNAlignSP[0].fill("0", "Start Align", ISS_OFF);
    OSNAlignSP[1].fill("1", "Issue Align", ISS_OFF);
    OSNAlignSP[2].fill("3", "Write Align", ISS_OFF);
    OSNAlignSP.fill(getDeviceName(), "NewAlignStar", "Align using up to 6 stars, Alpha only",
                       ALIGN_TAB, IP_RW, ISR_ATMOST1, 0, IPS_IDLE);

    OSNAlignWriteSP[0].fill("0", "Write Align to NVRAM/Flash", ISS_OFF);
    OSNAlignWriteSP.fill(getDeviceName(), "NewAlignStar2", "NVRAM", ALIGN_TAB, IP_RW,
                       ISR_ATMOST1, 0, IPS_IDLE);
    OSNAlignPolarRealignSP[0].fill("0", "Instructions", ISS_OFF);
    OSNAlignPolarRealignSP[1].fill("1", "Refine Polar Align (manually)", ISS_OFF);
    OSNAlignPolarRealignSP.fill(getDeviceName(), "AlignMP",
                       "Polar Correction, See info box", ALIGN_TAB, IP_RW, ISR_ATMOST1, 0, IPS_IDLE);

    OSNAlignTP[0].fill("0", "Align Process Status", "Align not started");
    OSNAlignTP[1].fill("1", "1. Manual Process", "Point towards the NCP");
    OSNAlignTP[2].fill("2", "2. Plate Solver Process", "Point towards the NCP");
    OSNAlignTP[3].fill("3", "Manual Action after 1", "Press 'Start Align'");
    OSNAlignTP[4].fill("4", "Current Status", "Not Updated");
    OSNAlignTP[5].fill("5", "Max Stars", "Not Updated");
    OSNAlignTP[6].fill("6", "Current Star", "Not Updated");
    OSNAlignTP[7].fill("7", "# of Align Stars", "Not Updated");
    OSNAlignTP.fill(getDeviceName(), "NAlign Process", "", ALIGN_TAB, IP_RO, 0, IPS_IDLE);

    OSNAlignErrTP[0].fill("0", "EQ Polar Error Alt", "Available once Aligned");
    OSNAlignErrTP[1].fill("1", "EQ Polar Error Az", "Available once Aligned");
    //     OSNAlignErrTP[2].fill("2", "2. Plate Solver Process", "Point towards the NCP");
    //     OSNAlignErrTP[3].fill("3", "After 1 or 2", "Press 'Start Align'");
    //     OSNAlignErrTP[4].fill("4", "Current Status", "Not Updated");
    //     OSNAlignErrTP[5].fill("5", "Max Stars", "Not Updated");
    //     OSNAlignErrTP[6].fill("6", "Current Star", "Not Updated");
    //     OSNAlignErrTP[7].fill("7", "# of Align Stars", "Not Updated");
    OSNAlignErrTP.fill(getDeviceName(), "ErrAlign Process", "", ALIGN_TAB, IP_RO, 0, IPS_IDLE);

#ifdef ONSTEP_NOTDONE
    // =============== OUTPUT_TAB
    // ===============
    OSOutput1SP[0].fill("0", "OFF", ISS_ON);
    OSOutput1SP[1].fill("1", "ON", ISS_OFF);
    OSOutput1SP.fill(getDeviceName(), "Output 1", "Output 1", OUTPUT_TAB, IP_RW, ISR_ATMOST1, 60,
                       IPS_ALERT);

    OSOutput2SP[0].fill("0", "OFF", ISS_ON);
    OSOutput2SP[1].fill("1", "ON", ISS_OFF);
    OSOutput2SP.fill(getDeviceName(), "Output 2", "Output 2", OUTPUT_TAB, IP_RW, ISR_ATMOST1, 60,
                       IPS_ALERT);
#endif

    for(int i = 0; i < PORTS_COUNT; i++)
    {
        char port_name[30];
        sprintf(port_name, "Output %d", i);
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
    IUFillTextVector(&OnstepStatTP, OnstepStat, 9, getDeviceName(), "OnStep Status", "", STATUS_TAB, IP_RO, 0, IPS_OK);

    // ============== WEATHER TAB
    // Uses OnStep's defaults for this
    OSSetTemperatureNP[0].fill("Set Temperature (C)", "C", "%4.2f", -100, 100, 1, 10);//-274, 999, 1, 10);
    OSSetTemperatureNP.fill(getDeviceName(), "Set Temperature (C)", "", ENVIRONMENT_TAB,
                       IP_RW, 0, IPS_IDLE);
    OSSetHumidityNP[0].fill("Set Relative Humidity (%)", "%", "%5.2f", 0, 100, 1, 70);
    OSSetHumidityNP.fill(getDeviceName(), "Set Relative Humidity (%)", "", ENVIRONMENT_TAB,
                       IP_RW, 0, IPS_IDLE);
    OSSetPressureNP[0].fill("Set Pressure (hPa)", "hPa", "%4f", 500, 1500, 1, 1010);
    OSSetPressureNP.fill(getDeviceName(), "Set Pressure (hPa)", "", ENVIRONMENT_TAB, IP_RW,
                       0, IPS_IDLE);

    //Will eventually pull from the elevation in site settings
    //TODO: Pull from elevation in site settings
    OSSetAltitudeNP[0].fill("Set Altitude (m)", "m", "%4f", 0, 20000, 1, 110);
    OSSetAltitudeNP.fill(getDeviceName(), "Set Altitude (m)", "", ENVIRONMENT_TAB, IP_RW, 0,
                       IPS_IDLE);



    addParameter("WEATHER_TEMPERATURE", "Temperature (C)", -40, 85, 15);
    addParameter("WEATHER_HUMIDITY", "Humidity %", 0, 100, 15);
    addParameter("WEATHER_BAROMETER", "Pressure (hPa)", 0, 1500, 15);
    addParameter("WEATHER_DEWPOINT", "Dew Point (C)", 0, 100, 15); // From OnStep
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
    FI::updateProperties();
    WI::updateProperties();
    if (isConnected())
    {
        // Firstinitialize some variables
        // keep sorted by TABs is easier
        // Main Control
        defineProperty(ReticSP);
        defineProperty(ElevationLimitNP);
        defineProperty(ObjectInfoTP);
        // Connection

        // Options

        // OnStep Status
        defineProperty(&OnstepStatTP);

        // Motion Control
        defineProperty(MaxSlewRateNP);
        defineProperty(TrackCompSP);
        defineProperty(TrackAxisSP);
        defineProperty(BacklashNP);
        defineProperty(GuideRateNP);
        defineProperty(AutoFlipSP);
        defineProperty(HomePauseSP);
        defineProperty(FrequencyAdjustSP);
        defineProperty(PreferredPierSideSP);
        defineProperty(minutesPastMeridianNP);

        // Site Management
        defineProperty(ParkOptionSP);
        defineProperty(SetHomeSP);

        // Guide

        // Focuser

        // Focuser 1

        if (!sendOnStepCommand(":FA#"))  // do we have a Focuser 1
        {
            OSFocuser1 = true;
            defineProperty(OSFocus1InitializeSP);
        }
        // Focuser 2
        if (!sendOnStepCommand(":fA#"))  // Do we have a Focuser 2
        {
            OSFocuser2 = true;
            //defineProperty(&OSFocus2SelSP);
            defineProperty(OSFocus2MotionSP);
            defineProperty(OSFocus2RateSP);
            defineProperty(OSFocus2TargNP);
        }

        // Firmware Data
        defineProperty(&VersionTP);

        //PEC
        defineProperty(OSPECStatusSP);
        defineProperty(OSPECIndexSP);
        defineProperty(OSPECRecordSP);
        defineProperty(OSPECReadSP);
        defineProperty(OSPECCurrentIndexNP);
        defineProperty(OSPECRWValuesNP);

        //New Align
        defineProperty(OSNAlignStarsSP);
        defineProperty(OSNAlignSP);
        defineProperty(OSNAlignWriteSP);
        defineProperty(OSNAlignTP);
        defineProperty(OSNAlignErrTP);
        defineProperty(OSNAlignPolarRealignSP);

#ifdef ONSTEP_NOTDONE
        //Outputs
        defineProperty(OSOutput1SP);
        defineProperty(OSOutput2SP);
#endif

        defineProperty(&OutputPorts_NP);

        //Weather
        defineProperty(OSSetTemperatureNP);
        defineProperty(OSSetPressureNP);
        defineProperty(OSSetHumidityNP);
        defineProperty(OSSetAltitudeNP);


        if (InitPark())
        {
            // If loading parking data is successful, we just set the default parking values.
            SetAxis1ParkDefault(LocationNP[LOCATION_LATITUDE].value >= 0 ? 0 : 180);
            SetAxis2ParkDefault(LocationNP[LOCATION_LATITUDE].value);
        }
        else
        {
            // Otherwise, we set all parking data to default in case no parking data is found.
            SetAxis1Park(LocationNP[LOCATION_LATITUDE].value >= 0 ? 0 : 180);
            SetAxis1ParkDefault(LocationNP[LOCATION_LATITUDE].value);

            SetAxis1ParkDefault(LocationNP[LOCATION_LATITUDE].value >= 0 ? 0 : 180);
            SetAxis2ParkDefault(LocationNP[LOCATION_LATITUDE].value);
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
        deleteProperty(ReticSP.getName());
        deleteProperty(ElevationLimitNP.getName());
        // Connection

        // Options

        // Motion Control
        deleteProperty(MaxSlewRateNP.getName());
        deleteProperty(TrackCompSP.getName());
        deleteProperty(TrackAxisSP.getName());
        deleteProperty(BacklashNP.getName());
        deleteProperty(GuideRateNP.getName());
        deleteProperty(AutoFlipSP.getName());
        deleteProperty(HomePauseSP.getName());
        deleteProperty(FrequencyAdjustSP.getName());
        deleteProperty(PreferredPierSideSP.getName());
        deleteProperty(minutesPastMeridianNP.getName());

        // Site Management
        deleteProperty(ParkOptionSP.getName());
        deleteProperty(SetHomeSP.getName());
        // Guide

        // Focuser
        // Focuser 1
        deleteProperty(OSFocus1InitializeSP.getName());

        // Focuser 2
        //deleteProperty(OSFocus2SelSP.name);
        deleteProperty(OSFocus2MotionSP.getName());
        deleteProperty(OSFocus2RateSP.getName());
        deleteProperty(OSFocus2TargNP.getName());

        // Firmware Data
        deleteProperty(VersionTP.name);


        //PEC
        deleteProperty(OSPECStatusSP.getName());
        deleteProperty(OSPECIndexSP.getName());
        deleteProperty(OSPECRecordSP.getName());
        deleteProperty(OSPECReadSP.getName());
        deleteProperty(OSPECCurrentIndexNP.getName());
        deleteProperty(OSPECRWValuesNP.getName());

        //New Align
        deleteProperty(OSNAlignStarsSP.getName());
        deleteProperty(OSNAlignSP.getName());
        deleteProperty(OSNAlignWriteSP.getName());
        deleteProperty(OSNAlignTP.getName());
        deleteProperty(OSNAlignErrTP.getName());
        deleteProperty(OSNAlignPolarRealignSP.getName());

#ifdef ONSTEP_NOTDONE
        //Outputs
        deleteProperty(OSOutput1SP.getName());
        deleteProperty(OSOutput2SP.getName());
#endif

        deleteProperty(OutputPorts_NP.name);

        // OnStep Status
        deleteProperty(OnstepStatTP.name);


        //Weather
        deleteProperty(OSSetTemperatureNP.getName());
        deleteProperty(OSSetPressureNP.getName());
        deleteProperty(OSSetHumidityNP.getName());
        deleteProperty(OSSetAltitudeNP.getName());

    }
    return true;
}

bool LX200_OnStep::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        if (strstr(name, "FOCUS_"))
            return FI::processNumber(dev, name, values, names, n);
        if (ObjectNoNP.isNameMatch(name))
        {
            char object_name[256];

            if (selectCatalogObject(PortFD, currentCatalog, (int)values[0]) < 0)
            {
                ObjectNoNP.setState(IPS_ALERT);
                ObjectNoNP.apply("Failed to select catalog object.");
                return false;
            }

            getLX200RA(PortFD, &targetRA);
            getLX200DEC(PortFD, &targetDEC);

            ObjectNoNP.setState(IPS_OK);
            ObjectNoNP.apply("Object updated.");

            if (getObjectInfo(PortFD, object_name) < 0)
                IDMessage(getDeviceName(), "Getting object info failed.");
            else
            {
                IUSaveText(&ObjectInfoTP[0], object_name);
                ObjectInfoTP.apply();
            }
            Goto(targetRA, targetDEC);
            return true;
        }

        if (MaxSlewRateNP.isNameMatch(name))
        {
            int ret;
            char cmd[5];
            snprintf(cmd, 5, ":R%d#", (int)values[0]);
            ret = sendOnStepCommandBlind(cmd);

            //if (setMaxSlewRate(PortFD, (int)values[0]) < 0) //(int) MaxSlewRateNP[0].value
            if (ret == -1)
            {
                LOGF_DEBUG("Pas OK Return value =%d", ret);
                LOGF_DEBUG("Setting Max Slew Rate to %f\n", values[0]);
                MaxSlewRateNP.setState(IPS_ALERT);
                MaxSlewRateNP.apply("Setting Max Slew Rate Failed");
                return false;
            }
            LOGF_DEBUG("OK Return value =%d", ret);
            MaxSlewRateNP.setState(IPS_OK);
            MaxSlewRateNP[0].setValue(values[0]);
            MaxSlewRateNP.apply("Slewrate set to %04.1f", values[0]);
            IUResetSwitch(&SlewRateSP);
            SlewRateS[int(values[0])].s = ISS_ON;
            SlewRateSP.s = IPS_OK;
            IDSetSwitch(&SlewRateSP, nullptr);
            return true;
        }

        if (BacklashNP.isNameMatch(name))
        {
            char cmd[9];
            int i, nset;
            double bklshdec = 0, bklshra = 0;

            for (nset = i = 0; i < n; i++)
            {
                INumber *bktp = BacklashNP.findWidgetByName(names[i]);
                if (bktp == &BacklashNP[0])
                {
                    bklshdec = values[i];
                    LOGF_DEBUG("===CMD==> Backlash DEC= %f", bklshdec);
                    nset += bklshdec >= 0 && bklshdec <= 999;  //range 0 to 999
                }
                else if (bktp == &BacklashNP[1])
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
                    BacklashNP.setState(IPS_ALERT);
                    BacklashNP.apply("Error Backlash DEC limit.");
                }
                const struct timespec timeout = {0, 100000000L};
                nanosleep(&timeout, nullptr); // time for OnStep to respond to previous cmd
                snprintf(cmd, 9, ":$BR%d#", (int)bklshra);
                if (sendOnStepCommand(cmd))
                {
                    BacklashNP.setState(IPS_ALERT);
                    BacklashNP.apply("Error Backlash RA limit.");
                }

                BacklashNP[0].setValue(bklshdec);
                BacklashNP[1].setValue(bklshra);
                BacklashNP.setState(IPS_OK);
                BacklashNP.apply();
                return true;
            }
            else
            {
                BacklashNP.setState(IPS_ALERT);
                BacklashNP.apply("Backlash invalid.");
                return false;
            }
        }

        if (ElevationLimitNP.isNameMatch(name))
        {
            // new elevation limits
            double minAlt = 0, maxAlt = 0;
            int i, nset;

            for (nset = i = 0; i < n; i++)
            {
                INumber *altp = ElevationLimitNP.findWidgetByName(names[i]);
                if (altp == &ElevationLimitNP[0])
                {
                    minAlt = values[i];
                    nset += minAlt >= -30.0 && minAlt <= 30.0;  //range -30 to 30
                }
                else if (altp == &ElevationLimitNP[1])
                {
                    maxAlt = values[i];
                    nset += maxAlt >= 60.0 && maxAlt <= 90.0;   //range 60 to 90
                }
            }
            if (nset == 2)
            {
                if (setMinElevationLimit(PortFD, (int)minAlt) < 0)
                {
                    ElevationLimitNP.setState(IPS_ALERT);
                    ElevationLimitNP.apply("Error setting min elevation limit.");
                }

                if (setMaxElevationLimit(PortFD, (int)maxAlt) < 0)
                {
                    ElevationLimitNP.setState(IPS_ALERT);
                    ElevationLimitNP.apply("Error setting max elevation limit.");
                    return false;
                }
                ElevationLimitNP[0].setValue(minAlt);
                ElevationLimitNP[1].setValue(maxAlt);
                ElevationLimitNP.setState(IPS_OK);
                ElevationLimitNP.apply();
                return true;
            }
            else
            {
                ElevationLimitNP.setState(IPS_IDLE);
                ElevationLimitNP.apply("elevation limit missing or invalid.");
                return false;
            }
        }
    }

    if (minutesPastMeridianNP.isNameMatch(name))
    {
        char cmd[20];
        int i, nset;
        double minPMEast = 0, minPMWest = 0;

        for (nset = i = 0; i < n; i++)
        {
            INumber *bktp = minutesPastMeridianNP.findWidgetByName(names[i]);
            if (bktp == &minutesPastMeridianNP[0])
            {
                minPMEast = values[i];
                LOGF_DEBUG("===CMD==> minutesPastMeridianNP[0]/East = %f", minPMEast);
                nset += minPMEast >= 0 && minPMEast <= 180;  //range 0 to 180
            }
            else if (bktp == &minutesPastMeridianNP[1])
            {
                minPMWest = values[i];
                LOGF_DEBUG("===CMD==> minutesPastMeridianNP[1]/West= %f", minPMWest);
                nset += minPMWest >= 0 && minPMWest <= 180;   //range 0 to 180
            }
        }
        if (nset == 2)
        {
            snprintf(cmd, 20, ":SXE9,%d#", (int) minPMEast);
            if (sendOnStepCommand(cmd))
            {
                minutesPastMeridianNP.setState(IPS_ALERT);
                minutesPastMeridianNP.apply("Error Backlash DEC limit.");
            }
            const struct timespec timeout = {0, 100000000L};
            nanosleep(&timeout, nullptr); // time for OnStep to respond to previous cmd
            snprintf(cmd, 20, ":SXEA,%d#", (int) minPMWest);
            if (sendOnStepCommand(cmd))
            {
                minutesPastMeridianNP.setState(IPS_ALERT);
                minutesPastMeridianNP.apply("Error Backlash RA limit.");
            }

            minutesPastMeridianNP[0].setValue(minPMEast);
            minutesPastMeridianNP[1].setValue(minPMWest);
            minutesPastMeridianNP.setState(IPS_OK);
            minutesPastMeridianNP.apply();
            return true;
        }
        else
        {
            minutesPastMeridianNP.setState(IPS_ALERT);
            minutesPastMeridianNP.apply("minutesPastMeridian invalid.");
            return false;
        }
    }
    // Focuser
    // Focuser 1 Now handled by Focusr Interface

    // Focuser 2 Target
    if (OSFocus2TargNP.isNameMatch(name))
    {
        char cmd[32];

        if ((values[0] >= -25000) && (values[0] <= 25000))
        {
            snprintf(cmd, 15, ":fR%d#", (int)values[0]);
            sendOnStepCommandBlind(cmd);
            OSFocus2TargNP.setState(IPS_OK);
            OSFocus2TargNP.apply("Focuser 2 position (relative) moved by %d", (int)values[0]);
            OSUpdateFocuser();
        }
        else
        {
            OSFocus2TargNP.setState(IPS_ALERT);
            OSFocus2TargNP.apply("Setting Max Slew Rate Failed");
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

    if (OSSetTemperatureNP.isNameMatch(name))
    {
        char cmd[32];

        if ((values[0] >= -100) && (values[0] <= 100))
        {
            snprintf(cmd, 15, ":SX9A,%d#", (int)values[0]);
            sendOnStepCommandBlind(cmd);
            OSSetTemperatureNP.setState(IPS_OK);
            OSSetTemperatureNP.apply("Temperature set to %d", (int)values[0]);
        }
        else
        {
            OSSetTemperatureNP.setState(IPS_ALERT);
            OSSetTemperatureNP.apply("Setting Temperature Failed");
        }
        return true;
    }

    if (OSSetHumidityNP.isNameMatch(name))
    {
        char cmd[32];

        if ((values[0] >= 0) && (values[0] <= 100))
        {
            snprintf(cmd, 15, ":SX9C,%d#", (int)values[0]);
            sendOnStepCommandBlind(cmd);
            OSSetHumidityNP.setState(IPS_OK);
            OSSetHumidityNP.apply("Humidity set to %d", (int)values[0]);
        }
        else
        {
            OSSetHumidityNP.setState(IPS_ALERT);
            OSSetHumidityNP.apply("Setting Humidity Failed");
        }
        return true;
    }

    if (OSSetPressureNP.isNameMatch(name))
    {
        char cmd[32];

        if ((values[0] >= 0) && (values[0] <= 100))
        {
            snprintf(cmd, 15, ":SX9B,%d#", (int)values[0]);
            sendOnStepCommandBlind(cmd);
            OSSetPressureNP.setState(IPS_OK);
            OSSetPressureNP.apply("Pressure set to %d", (int)values[0]);
        }
        else
        {
            OSSetPressureNP.setState(IPS_ALERT);
            OSSetPressureNP.apply("Setting Pressure Failed");
        }
        return true;
    }





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
        // Reticlue +/- Buttons
        if (ReticSP.isNameMatch(name))
        {
            long ret = 0;

            ReticSP.update(states, names, n);
            ReticSP.setState(IPS_OK);

            if (ReticSP[0].getState() == ISS_ON)
            {
                ret = ReticPlus(PortFD);
                ReticSP[0].setState(ISS_OFF);
                ReticSP.apply("Bright");
            }
            else
            {
                ret = ReticMoins(PortFD);
                ReticSP[1].setState(ISS_OFF);
                ReticSP.apply("Dark");
            }

            ReticSP.reset();
            ReticSP.apply();
            return true;
        }
        //Move to more standard controls
        if (!strcmp(name, SlewRateSP.name))
        {
            IUUpdateSwitch(&SlewRateSP, states, names, n);
            int ret;
            char cmd[5];
            int index = IUFindOnSwitchIndex(&SlewRateSP) ;//-1; //-1 because index is 1-10, OS Values are 0-9
            snprintf(cmd, 5, ":R%d#", index);
            ret = sendOnStepCommandBlind(cmd);

            //if (setMaxSlewRate(PortFD, (int)values[0]) < 0) //(int) MaxSlewRateNP[0].value
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
            MaxSlewRateNP.setState(IPS_OK);
            MaxSlewRateNP[0].setValue(index);
            MaxSlewRateNP.apply("Slewrate set to %d", index);
            IUResetSwitch(&SlewRateSP);
            SlewRateS[index].s = ISS_ON;
            SlewRateSP.s = IPS_OK;
            IDSetSwitch(&SlewRateSP, nullptr);
            return true;
        }
        // Homing, Cold and Warm Init
        if (SetHomeSP.isNameMatch(name))
        {
            SetHomeSP.update(states, names, n);
            SetHomeSP.setState(IPS_OK);

            if (SetHomeSP[0].getState() == ISS_ON)
            {
                if(!sendOnStepCommandBlind(":hC#"))
                    return false;
                SetHomeSP.apply("Return Home");
                SetHomeSP[0].setState(ISS_OFF);
            }
            else
            {
                if(!sendOnStepCommandBlind(":hF#"))
                    return false;
                SetHomeSP.apply("At Home (Reset)");
                SetHomeSP[1].setState(ISS_OFF);
            }
            ReticSP.reset();
            SetHomeSP.setState(IPS_IDLE);
            SetHomeSP.apply();
            return true;
        }

        // Tracking Compensation selection
        if (TrackCompSP.isNameMatch(name))
        {
            TrackCompSP.update(states, names, n);
            TrackCompSP.setState(IPS_BUSY);

            if (TrackCompSP[0].getState() == ISS_ON)
            {
                if (!sendOnStepCommand(":To#"))
                {
                    TrackCompSP.apply("Full Compensated Tracking On");
                    TrackCompSP.setState(IPS_OK);
                    TrackCompSP.apply();
                    return true;
                }
            }
            if (TrackCompSP[1].getState() == ISS_ON)
            {
                if (!sendOnStepCommand(":Tr#"))
                {
                    TrackCompSP.apply("Refraction Tracking On");
                    TrackCompSP.setState(IPS_OK);
                    TrackCompSP.apply();
                    return true;
                }
            }
            if (TrackCompSP[2].getState() == ISS_ON)
            {
                if (!sendOnStepCommand(":Tn#"))
                {
                    TrackCompSP.apply("Refraction Tracking Disabled");
                    TrackCompSP.setState(IPS_OK);
                    TrackCompSP.apply();
                    return true;
                }
            }
            TrackCompSP.reset();
            TrackCompSP.setState(IPS_IDLE);
            TrackCompSP.apply();
            return true;
        }

        if (TrackAxisSP.isNameMatch(name))
        {
            TrackAxisSP.update(states, names, n);
            TrackAxisSP.setState(IPS_BUSY);

            if (TrackAxisSP[0].getState() == ISS_ON)
            {
                if (!sendOnStepCommand(":T1#"))
                {
                    TrackAxisSP.apply("Single Tracking On");
                    TrackAxisSP.setState(IPS_OK);
                    TrackAxisSP.apply();
                    return true;
                }
            }
            if (TrackAxisSP[1].getState() == ISS_ON)
            {
                if (!sendOnStepCommand(":T2#"))
                {
                    TrackAxisSP.apply("Dual Axis Tracking On");
                    TrackAxisSP.setState(IPS_OK);
                    TrackAxisSP.apply();
                    return true;
                }
            }
            TrackAxisSP.reset();
            TrackAxisSP.setState(IPS_IDLE);
            TrackAxisSP.apply();
            return true;
        }

        if (AutoFlipSP.isNameMatch(name))
        {
            AutoFlipSP.update(states, names, n);
            AutoFlipSP.setState(IPS_BUSY);

            if (AutoFlipSP[0].getState() == ISS_ON)
            {
                if (sendOnStepCommand(":SX95,0#"))
                {
                    AutoFlipSP.setState(IPS_OK);
                    AutoFlipSP.apply("Auto Meridian Flip OFF");
                    return true;
                }
            }
            if (AutoFlipSP[1].getState() == ISS_ON)
            {
                if (sendOnStepCommand(":SX95,1#"))
                {
                    AutoFlipSP.setState(IPS_OK);
                    AutoFlipSP.apply("Auto Meridian Flip ON");
                    return true;
                }
            }
            AutoFlipSP.reset();
            //AutoFlipSP.setState(IPS_IDLE);
            AutoFlipSP.apply();
            return true;
        }

        if (HomePauseSP.isNameMatch(name))
        {
            HomePauseSP.update(states, names, n);
            HomePauseSP.setState(IPS_BUSY);

            if (HomePauseSP[0].getState() == ISS_ON)
            {
                if (sendOnStepCommand(":SX98,0#"))
                {
                    HomePauseSP.setState(IPS_OK);
                    HomePauseSP.apply("Home Pause OFF");
                    return true;
                }
            }
            if (HomePauseSP[1].getState() == ISS_ON)
            {
                if (sendOnStepCommand(":SX98,1#"))
                {
                    HomePauseSP.setState(IPS_OK);
                    HomePauseSP.apply("Home Pause ON");
                    return true;
                }
            }
            if (HomePauseSP[2].getState() == ISS_ON)
            {
                if (sendOnStepCommand(":SX99,1#"))
                {
                    HomePauseSP.reset();
                    HomePauseSP.setState(IPS_OK);
                    HomePauseSP.apply("Home Pause: Continue");
                    return true;
                }
            }
            HomePauseSP.reset();
            HomePauseSP.setState(IPS_IDLE);
            HomePauseSP.apply();
            return true;
        }

        if (FrequencyAdjustSP.isNameMatch(name))      //

            //
        {
            FrequencyAdjustSP.update(states, names, n);
            FrequencyAdjustSP.setState(IPS_OK);

            if (FrequencyAdjustSP[0].getState() == ISS_ON)
            {
                if (!sendOnStepCommandBlind(":T-#"))
                {
                    FrequencyAdjustSP.apply("Frequency decreased");
                    return true;
                }
            }
            if (FrequencyAdjustSP[1].getState() == ISS_ON)
            {
                if (!sendOnStepCommandBlind(":T+#"))
                {
                    FrequencyAdjustSP.apply("Frequency increased");
                    return true;
                }
            }
            if (FrequencyAdjustSP[2].getState() == ISS_ON)
            {
                if (!sendOnStepCommandBlind(":TR#"))
                {
                    FrequencyAdjustSP.apply("Frequency Reset (TO saved EEPROM)");
                    return true;
                }
            }
            FrequencyAdjustSP.reset();
            FrequencyAdjustSP.setState(IPS_IDLE);
            FrequencyAdjustSP.apply();
            return true;
        }

        //Pier Side
        if (PreferredPierSideSP.isNameMatch(name))
        {
            PreferredPierSideSP.update(states, names, n);
            PreferredPierSideSP.setState(IPS_BUSY);

            if (PreferredPierSideSP[0].getState() == ISS_ON) //West
            {
                if (sendOnStepCommand(":SX96,W#"))
                {
                    PreferredPierSideSP.setState(IPS_OK);
                    PreferredPierSideSP.apply("Preferred Pier Side: West");
                    return true;
                }
            }
            if (PreferredPierSideSP[1].getState() == ISS_ON) //East
            {
                if (sendOnStepCommand(":SX96,E#"))
                {
                    PreferredPierSideSP.setState(IPS_OK);
                    PreferredPierSideSP.apply("Preferred Pier Side: East");
                    return true;
                }
            }
            if (PreferredPierSideSP[2].getState() == ISS_ON) //Best
            {
                if (sendOnStepCommand(":SX96,B#"))
                {
                    PreferredPierSideSP.setState(IPS_OK);
                    PreferredPierSideSP.apply("Preferred Pier Side: Best");
                    return true;
                }
            }
            PreferredPierSideSP.reset();
            PreferredPierSideSP.apply();
            return true;
        }


        // Focuser
        // Focuser 1 Rates
        if (OSFocus1InitializeSP.isNameMatch(name))
        {
            char cmd[32];
            if (!OSFocus1InitializeSP.update(states, names, n))
                return false;
            index = OSFocus1InitializeSP.findOnSwitchIndex();
            if (index == 0)
            {
                snprintf(cmd, 5, ":FZ#");
                sendOnStepCommandBlind(cmd);
                OSFocus1InitializeSP[index].setState(ISS_OFF);
                OSFocus1InitializeSP.setState(IPS_OK);
                OSFocus1InitializeSP.apply();
            }
            if (index == 1)
            {
                snprintf(cmd, 5, ":FH#");
                sendOnStepCommandBlind(cmd);
                OSFocus1InitializeSP[index].setState(ISS_OFF);
                OSFocus1InitializeSP.setState(IPS_OK);
                OSFocus1InitializeSP.apply();
            }
        }

        // Focuser 2 Rates
        if (OSFocus2RateSP.isNameMatch(name))
        {
            char cmd[32];

            if (!OSFocus2RateSP.update(states, names, n))
                return false;

            index = OSFocus2RateSP.findOnSwitchIndex();
            snprintf(cmd, 5, ":F%d#", index + 1);
            sendOnStepCommandBlind(cmd);
            OSFocus2RateSP[index].setState(ISS_OFF);
            OSFocus2RateSP.setState(IPS_OK);
            OSFocus2RateSP.apply();
        }
        // Focuser 2 Motion
        if (OSFocus2MotionSP.isNameMatch(name))
        {
            char cmd[32];

            if (!OSFocus2MotionSP.update(states, names, n))
                return false;

            index = OSFocus2MotionSP.findOnSwitchIndex();
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
            OSFocus2MotionSP[index].setState(ISS_OFF);
            OSFocus2MotionSP.setState(IPS_OK);
            OSFocus2MotionSP.apply();
        }

        // PEC
        if (OSPECRecordSP.isNameMatch(name))
        {
            OSPECRecordSP.update(states, names, n);
            OSPECRecordSP.setState(IPS_OK);

            if (OSPECRecordSP[0].getState() == ISS_ON)
            {
                OSPECEnabled = true;
                ClearPECBuffer(0);
                OSPECRecordSP[0].setState(ISS_OFF);
            }
            if (OSPECRecordSP[1].getState() == ISS_ON)
            {
                OSPECEnabled = true;
                StartPECRecord(0);
                OSPECRecordSP[1].setState(ISS_OFF);
            }
            if (OSPECRecordSP[2].getState() == ISS_ON)
            {
                OSPECEnabled = true;
                SavePECBuffer(0);
                OSPECRecordSP[2].setState(ISS_OFF);
            }
            OSPECRecordSP.apply();
        }
        if (OSPECReadSP.isNameMatch(name))
        {
            if (OSPECReadSP[0].getState() == ISS_ON)
            {
                OSPECEnabled = true;
                ReadPECBuffer(0);
                OSPECReadSP[0].setState(ISS_OFF);
            }
            if (OSPECReadSP[1].getState() == ISS_ON)
            {
                OSPECEnabled = true;
                WritePECBuffer(0);
                OSPECReadSP[1].setState(ISS_OFF);
            }
            OSPECReadSP.apply();
        }
        if (PECStateSP.isNameMatch(name))
        {
            index = PECStateSP.findOnSwitchIndex();
            if (index == 0)
            {
                OSPECEnabled = true;
                StopPECPlayback(0); //Status will set OSPECEnabled to false if that's set by the controller
                PECStateSP[0].setState(ISS_ON);
                PECStateSP[1].setState(ISS_OFF);
                PECStateSP.apply();
            }
            else if (index == 1)
            {
                OSPECEnabled = true;
                StartPECPlayback(0);
                PECStateSP[0].setState(ISS_OFF);
                PECStateSP[1].setState(ISS_ON);
                PECStateSP.apply();
            }

        }

        // Align Buttons
        if (OSNAlignStarsSP.isNameMatch(name))
        {
            OSNAlignStarsSP.reset();
            OSNAlignStarsSP.update(states, names, n);
            index = OSNAlignStarsSP.findOnSwitchIndex();

            return true;
        }

        // Alignment
        if (OSNAlignSP.isNameMatch(name))
        {
            if (!OSNAlignSP.update(states, names, n))
                return false;

            index = OSNAlignSP.findOnSwitchIndex();
            //NewGeometricAlignment
            //End NewGeometricAlignment
            OSNAlignSP.setState(IPS_BUSY);
            if (index == 0)
            {

                /* Index is 0-8 and represents index+1*/

                int index_stars = OSNAlignStarsSP.findOnSwitchIndex();
                if ((index_stars <= 8) && (index_stars >= 0))
                {
                    int stars = index_stars + 1;
                    OSNAlignSP[0].setState(ISS_OFF);
                    LOGF_INFO("Align index: %d, stars: %d", index_stars, stars);
                    AlignStartGeometric(stars);
                }
            }
            if (index == 1)
            {
                OSNAlignSP[1].setState(ISS_OFF);
                OSNAlignSP.setState(AlignAddStar());
            }
            //Write to EEPROM moved to new line/variable
            OSNAlignSP.apply();
            UpdateAlignStatus();
        }

        if (OSNAlignWriteSP.isNameMatch(name))
        {
            if (!OSNAlignWriteSP.update(states, names, n))
                return false;

            index = OSNAlignWriteSP.findOnSwitchIndex();

            OSNAlignWriteSP.setState(IPS_BUSY);
            if (index == 0)
            {
                OSNAlignWriteSP[0].setState(ISS_OFF);
                OSNAlignWriteSP.setState(AlignWrite());
            }
            OSNAlignWriteSP.apply();
            UpdateAlignStatus();
        }

        if (OSNAlignPolarRealignSP.isNameMatch(name))
        {
            char cmd[10];
            if (!OSNAlignPolarRealignSP.update(states, names, n))
                return false;


            OSNAlignPolarRealignSP.setState(IPS_BUSY);
            if (OSNAlignPolarRealignSP[0].getState() == ISS_ON) //INFO
            {
                OSNAlignPolarRealignSP[0].setState(ISS_OFF);
                LOG_INFO("Step 1: Goto a bright star between 50 and 80 degrees N/S from the pole. Preferably on the Meridian.");
                LOG_INFO("Step 2: Make sure it is centered.");
                LOG_INFO("Step 3: Press Refine Polar Alignment.");
                LOG_INFO("Step 4: Using the mount's Alt and Az screws manually recenter the star. (Video mode if your camera supports it will be helpful.)");
                LOG_INFO("Optional: Start a new alignment.");
                OSNAlignPolarRealignSP.apply();
                UpdateAlignStatus();
                return true;
            }
            if (OSNAlignPolarRealignSP[1].getState() == ISS_ON) //Command
            {
                OSNAlignPolarRealignSP[1].setState(ISS_OFF);
                // int returncode=sendOnStepCommand("
                snprintf(cmd, 5, ":MP#");
                sendOnStepCommandBlind(cmd);
                if (!sendOnStepCommandBlind(":MP#"))
                {
                    OSNAlignPolarRealignSP.apply("Command for Refine Polar Alignment successful");
                    UpdateAlignStatus();
                    OSNAlignPolarRealignSP.setState(IPS_OK);
                    return true;
                }
                else
                {
                    OSNAlignPolarRealignSP.apply("Command for Refine Polar Alignment FAILED");
                    UpdateAlignStatus();
                    OSNAlignPolarRealignSP.setState(IPS_ALERT);
                    return false;
                }
            }
        }

#ifdef ONSTEP_NOTDONE
        if (OSOutput1SP.isNameMatch(name))      //
        {
            if (OSOutput1SP[0].getState() == ISS_ON)
            {
                OSDisableOutput(1);
                //PECStateSP[0].getState() == ISS_OFF;
            }
            else if (OSOutput1SP[1].getState() == ISS_ON)
            {
                OSEnableOutput(1);
                //PECStateSP[1].getState() == ISS_OFF;
            }
            OSOutput1SP.apply();
        }
        if (OSOutput2SP.isNameMatch(name))      //
        {
            if (OSOutput2SP[0].getState() == ISS_ON)
            {
                OSDisableOutput(2);
                //PECStateSP[0].getState() == ISS_OFF;
            }
            else if (OSOutput2SP[1].getState() == ISS_ON)
            {
                OSEnableOutput(2);
                //PECStateSP[1].getState() == ISS_OFF;
            }
            OSOutput2SP.apply();
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
        if (EqNP.getState() == IPS_BUSY)
        {
            if (!isSimulation() && abortSlew(PortFD) < 0)
            {
                Telescope::AbortSP.setState(IPS_ALERT);
                IDSetSwitch(&(Telescope::AbortSP), "Abort slew failed.");
                return false;
            }
            Telescope::AbortSP.setState(IPS_OK);
            EqNP.setState(IPS_IDLE);
            IDSetSwitch(&(Telescope::AbortSP), "Slew aborted.");
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
            ParkSP.apply("Parking Failed.");
            return false;
        }
    }
    ParkSP.setState(IPS_BUSY);
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
        EqNP.setState(IPS_ALERT);
        EqNP.apply("Error reading RA/DEC.");
        return false;
    }

#ifdef OnStep_Alpha
    OSSupports_bitfield_Gu = try_bitfield_Gu();

    if (!OSSupports_bitfield_Gu)
    {
        //Fall back to :GU parsing
#endif
        getCommandString(PortFD, OSStat, ":GU#"); // :GU# returns a string containg controller status
        if (strcmp(OSStat, OldOSStat) != 0) //if status changed
        {
            // ============= Telescope Status
            strcpy(OldOSStat, OSStat);

            IUSaveText(&OnstepStat[0], OSStat);
            if (strstr(OSStat, "n") && strstr(OSStat, "N"))
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


            // ============= Parkstatus
            if(FirstRead)   // it is the first time I read the status so I need to update
            {
                if (strstr(OSStat, "P"))
                {
                    SetParked(true); //defaults to TrackState=SCOPE_PARKED
                    IUSaveText(&OnstepStat[3], "Parked");
                }
                if (strstr(OSStat, "F"))
                {
                    SetParked(false); // defaults to TrackState=SCOPE_IDLE
                    IUSaveText(&OnstepStat[3], "Parking Failed");
                }
                if (strstr(OSStat, "I"))
                {
                    SetParked(false); //defaults to TrackState=SCOPE_IDLE but we want
                    TrackState = SCOPE_PARKING;
                    IUSaveText(&OnstepStat[3], "Park in Progress");
                }
                if (strstr(OSStat, "p"))
                {
                    SetParked(false); //defaults to TrackState=SCOPE_IDLE but we want
                    if (strstr(OSStat, "nN"))   // azwing need to detect if unparked idle or tracking
                    {
                        IUSaveText(&OnstepStat[1], "Idle");
                        TrackState = SCOPE_IDLE;
                    }
                    else TrackState = SCOPE_TRACKING;
                    IUSaveText(&OnstepStat[3], "UnParked");
                }
                FirstRead = false;
            }
            else
            {
                if (!isParked())
                {
                    if(strstr(OSStat, "P"))
                    {
                        SetParked(true);
                        IUSaveText(&OnstepStat[3], "Parked");
                        //LOG_INFO("OnStep Parking Succeeded");
                    }
                    if (strstr(OSStat, "I"))
                    {
                        SetParked(false); //defaults to TrackState=SCOPE_IDLE but we want
                        TrackState = SCOPE_PARKING;
                        IUSaveText(&OnstepStat[3], "Park in Progress");
                        LOG_INFO("OnStep Parking in Progress...");
                    }
                }
                if (isParked())
                {
                    if (strstr(OSStat, "F"))
                    {
                        // keep Status even if error  TrackState=SCOPE_IDLE;
                        SetParked(false); //defaults to TrackState=SCOPE_IDLE
                        IUSaveText(&OnstepStat[3], "Parking Failed");
                        LOG_ERROR("OnStep Parking failed, need to re Init OnStep at home");
                    }
                    if (strstr(OSStat, "p"))
                    {
                        SetParked(false); //defaults to TrackState=SCOPE_IDLE but we want
                        if (strstr(OSStat, "nN"))   // azwing need to detect if unparked idle or tracking
                        {
                            IUSaveText(&OnstepStat[1], "Idle");
                            TrackState = SCOPE_IDLE;
                        }
                        else TrackState = SCOPE_TRACKING;
                        IUSaveText(&OnstepStat[3], "UnParked");
                        //LOG_INFO("OnStep Unparked...");
                    }
                }
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
                HomePauseSP[1].setState(ISS_ON);
                HomePauseSP.setState(IPS_OK);
                HomePauseSP.apply("Pause at Home Enabled");
            }
            else
            {
                HomePauseSP[0].setState(ISS_ON);
                HomePauseSP.setState(IPS_OK);
                HomePauseSP.apply();
            }

            if (strstr(OSStat, "w"))
            {
                IUSaveText(&OnstepStat[3], "Waiting at Home");
            }

            // ============= Pec Status
            if (!strstr(OSStat, "R") && !strstr(OSStat, "W"))
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
                IUSaveText(&OnstepStat[6], "German Mount");
                OSMountType = 0;
            }
            if (strstr(OSStat, "K"))
            {
                IUSaveText(&OnstepStat[6], "Fork Mount");
                OSMountType = 1;
            }
            if (strstr(OSStat, "k"))
            {
                IUSaveText(&OnstepStat[6], "Fork Alt Mount");
                OSMountType = 2;
            }
            if (strstr(OSStat, "A"))
            {
                IUSaveText(&OnstepStat[6], "AltAZ Mount");
                OSMountType = 3;
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
        }

#ifdef OnStep_Alpha // For the moment, for :Gu
    }
    else
    {
        getCommandString(PortFD, OSStat, ":Gu#"); // :Gu# returns a string containg controller status that's bitpacked
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
            HomePauseSP[1].setState(ISS_ON);
            HomePauseSP.setState(IPS_OK);
            HomePauseSP.apply("Pause at Home Enabled");
        }
        else
        {
            HomePauseSP[0].setState(ISS_ON);
            HomePauseSP.setState(IPS_OK);
            HomePauseSP.apply();
        }
        if (OSStat[2] & 0b10001000 == 0b10001000)
        {
            // Buzzer enabled?
        }
        if (OSStat[2] & 0b10010000 == 0b10010000)
        {
            // Auto meridian flip
            AutoFlipSP[1].setState(ISS_ON);
            AutoFlipSP.setState(IPS_OK);
            AutoFlipSP.apply();
        }
        else
        {
            AutoFlipSP[0].setState(ISS_ON);
            AutoFlipSP.setState(IPS_OK);
            AutoFlipSP.apply();
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
    getCommandString(PortFD, OSPier, ":Gm#");
    if (strcmp(OSPier, OldOSPier) != 0) // any change ?
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
    getCommandString(PortFD, OSbacklashDEC, ":%BD#");
    getCommandString(PortFD, OSbacklashRA, ":%BR#");
    BacklashNP[0].setValue(atof(OSbacklashDEC));
    BacklashNP[1].setValue(atof(OSbacklashRA));
    BacklashNP.apply();

    double pulseguiderate;
    getCommandString(PortFD, GuideValue, ":GX90#");
    //     LOGF_DEBUG("Guide Rate String: %s", GuideValue);
    pulseguiderate = atof(GuideValue);
    LOGF_DEBUG("Guide Rate: %f", pulseguiderate);
    GuideRateNP[0].setValue(pulseguiderate);
    GuideRateNP[1].setValue(pulseguiderate);
    GuideRateNP.apply();


#ifndef OnStep_Alpha
    if (OSMountType == MOUNTTYPE_GEM)
    {
        //AutoFlip
        getCommandString(PortFD, TempValue, ":GX95#");
        if (atoi(TempValue))
        {
            AutoFlipSP[1].setState(ISS_ON);
            AutoFlipSP.setState(IPS_OK);
            AutoFlipSP.apply();
        }
        else
        {
            AutoFlipSP[0].setState(ISS_ON);
            AutoFlipSP.setState(IPS_OK);
            AutoFlipSP.apply();
        }
    }
#endif

    if (OSMountType == MOUNTTYPE_GEM)   //Doesn't apply to non-GEMs
    {
        //PreferredPierSide
        getCommandString(PortFD, TempValue, ":GX96#");
        if (strstr(TempValue, "W"))
        {
            PreferredPierSideSP[0].setState(ISS_ON);
            PreferredPierSideSP.setState(IPS_OK);
            PreferredPierSideSP.apply();
        }
        else if (strstr(TempValue, "E"))
        {
            PreferredPierSideSP[1].setState(ISS_ON);
            PreferredPierSideSP.setState(IPS_OK);
            PreferredPierSideSP.apply();
        }
        else if (strstr(TempValue, "B"))
        {
            PreferredPierSideSP[2].setState(ISS_ON);
            PreferredPierSideSP.setState(IPS_OK);
            PreferredPierSideSP.apply();
        }
        else
        {
            PreferredPierSideSP.reset();
            PreferredPierSideSP.setState(IPS_BUSY);
            PreferredPierSideSP.apply();
        }

        getCommandString(PortFD, TempValue, ":GXE9#"); // E
        getCommandString(PortFD, TempValue2, ":GXEA#"); // W
        minutesPastMeridianNP[0].setValue(atof(TempValue)); // E
        minutesPastMeridianNP[1].setValue(atof(TempValue2)); //W
        minutesPastMeridianNP.apply();

    }

    //TODO: Rotator support


    //Weather update
    getCommandString(PortFD, TempValue, ":GX9A#");
    setParameterValue("WEATHER_TEMPERATURE", std::stod(TempValue));
    getCommandString(PortFD, TempValue, ":GX9C#");
    setParameterValue("WEATHER_HUMIDITY", std::stod(TempValue));
    getCommandString(PortFD, TempValue, ":GX9B#");
    setParameterValue("WEATHER_BAROMETER", std::stod(TempValue));
    getCommandString(PortFD, TempValue, ":GX9E#");
    setParameterValue("WEATHER_DEWPOINT", std::stod(TempValue));

    //Disabled, because this is supplied via Kstars or other location, no sensor to read this
    //getCommandString(PortFD,TempValue, ":GX9D#");
    //setParameterValue("WEATHER_ALTITUDE", std::stod(TempValue));
    WI::updateProperties();

    if (WI::syncCriticalParameters())
        IDSetLight(&critialParametersLP, nullptr);
    ParametersNP.s = IPS_OK;
    IDSetNumber(&ParametersNP, nullptr);



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

IPState LX200_OnStep::MoveAbsFocuser (uint32_t targetTicks)
{
    //  :FSsnnn#  Set focuser target position (in microns)
    //            Returns: Nothing
    if (FocusAbsPosNP[0].max >= int(targetTicks) && FocusAbsPosNP[0].min <= int(targetTicks))
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
        FocusAbsPosNP[0].setValue(atoi(value));
        current = FocusAbsPosNP[0].getValue();
        FocusAbsPosNP.apply();
        LOGF_DEBUG("Current focuser: %d, %d", atoi(value), FocusAbsPosNP[0].getValue());
        //  :FT#  get status
        //         Returns: M# (for moving) or S# (for stopped)
        getCommandString(PortFD, value, ":FT#");
        if (value[0] == 'S')
        {
            FocusRelPosNP.setState(IPS_OK);
            FocusRelPosNP.apply();
            FocusAbsPosNP.setState(IPS_OK);
            FocusAbsPosNP.apply();
        }
        else if (value[0] == 'M')
        {
            FocusRelPosNP.setState(IPS_BUSY);
            FocusRelPosNP.apply();
            FocusAbsPosNP.setState(IPS_BUSY);
            FocusAbsPosNP.apply();
        }
        else
        {
            //INVALID REPLY
            FocusRelPosNP.setState(IPS_ALERT);
            FocusRelPosNP.apply();
            FocusAbsPosNP.setState(IPS_ALERT);
            FocusAbsPosNP.apply();
        }
        //  :FM#  Get max position (in microns)
        //         Returns: n#
        getCommandString(PortFD, value, ":FM#");
        FocusAbsPosNP[0].setMax(atoi(value));
        FocusAbsPosNP.updateMinMax();
        FocusAbsPosNP.apply();
        //  :FI#  Get full in position (in microns)
        //         Returns: n#
        getCommandString(PortFD, value, ":FI#");
        FocusAbsPosNP[0].setMin(atoi(value));
        FocusAbsPosNP.updateMinMax();
        FocusAbsPosNP.apply();
        FI::updateProperties();
        LOGF_DEBUG("After update properties: FocusAbsPosN min: %f max: %f", FocusAbsPosNP[0].min, FocusAbsPosNP[0].max);
    }


    if(OSFocuser2)
    {
        getCommandString(PortFD, value, ":fG#");
        OSFocus2TargNP[0].setValue(atoi(value));
        OSFocus2TargNP.apply();
    }
}

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
    if (OSPECEnabled == true)
    {
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
    if (OSPECEnabled == true)
    {
        if (OSMountType == MOUNTTYPE_ALTAZ)
        {
            OSPECEnabled = false;
            LOG_INFO("Command to give PEC called when Controller does not support PEC due to being Alt-Az Disabled");
            return IPS_ALERT;
        }
        //LOG_INFO("Getting PEC Status");
        //  :$QZ?  Get PEC status
        //         Returns: S#
        // Returns status (pecSense) In the form: Status is one of "IpPrR" (I)gnore, get ready to (p)lay, (P)laying, get ready to (r)ecord, (R)ecording.  Or an optional (.) to indicate an index detect.
        // OSPECStatusSP[0].fill("OFF", "OFF", ISS_ON);
        // OSPECStatusSP[1].fill("Playing", "Playing", ISS_OFF);
        // OSPECStatusSP[2].fill("Recording", "Recording", ISS_OFF);
        // OSPECStatusSP[3].fill("Will Play", "Will Play", ISS_OFF);
        // OSPECStatusSP[4].fill("Will Record", "Will Record", ISS_OFF);
        char value[RB_MAX_LEN] = "  ";
        OSPECStatusSP.setState(IPS_BUSY);
        getCommandString(PortFD, value, ":$QZ?#");
        // LOGF_INFO("Response %s", value);
        // LOGF_INFO("Response %d", value[0]);
        // LOGF_INFO("Response %d", value[1]);
        OSPECStatusSP[0].setState(ISS_OFF );
        OSPECStatusSP[1].setState(ISS_OFF );
        OSPECStatusSP[2].setState(ISS_OFF );
        OSPECStatusSP[3].setState(ISS_OFF );
        OSPECStatusSP[4].setState(ISS_OFF );
        if (value[0] == 'I') //Ignore
        {
            OSPECStatusSP.setState(IPS_OK);
            OSPECStatusSP[0].setState(ISS_ON );
            OSPECRecordSP.setState(IPS_IDLE);
            OSPECEnabled = false;
            LOG_INFO("Controller reports PEC Ignored and not supported");
            LOG_INFO("No Further PEC Commands will be processed, unless status changed");
        }
        else if (value[0] == 'R') //Active Recording
        {
            OSPECStatusSP.setState(IPS_OK);
            OSPECStatusSP[2].setState(ISS_ON );
            OSPECRecordSP.setState(IPS_BUSY);
        }
        else if (value[0] == 'r')  //Waiting for index before recording
        {
            OSPECStatusSP.setState(IPS_OK);
            OSPECStatusSP[4].setState(ISS_ON );
            OSPECRecordSP.setState(IPS_BUSY);
        }
        else if (value[0] == 'P') //Active Playing
        {
            OSPECStatusSP.setState(IPS_BUSY);
            OSPECStatusSP[1].setState(ISS_ON );
            OSPECRecordSP.setState(IPS_IDLE);
        }
        else if (value[0] == 'p') //Waiting for index before playing
        {
            OSPECStatusSP.setState(IPS_BUSY);
            OSPECStatusSP[3].setState(ISS_ON );
            OSPECRecordSP.setState(IPS_IDLE);
        }
        else //INVALID REPLY
        {
            OSPECStatusSP.setState(IPS_ALERT);
            OSPECRecordSP.setState(IPS_ALERT);
        }
        if (value[1] == '.')
        {
            OSPECIndexSP.setState(IPS_OK);
            OSPECIndexSP[0].setState(ISS_OFF);
            OSPECIndexSP[1].setState(ISS_ON);
        }
        else
        {
            OSPECIndexSP[1].setState(ISS_OFF);
            OSPECIndexSP[0].setState(ISS_ON);
        }
        OSPECStatusSP.apply();
        OSPECRecordSP.apply();
        OSPECIndexSP.apply();
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

IPState LX200_OnStep::AlignStartGeometric (int stars)
{
    //See here https://groups.io/g/onstep/message/3624
    char cmd[8];

    LOG_INFO("Sending Command to Start Alignment");
    OSNAlignTP[0].setText("Align STARTED");
    OSNAlignTP[1].setText("GOTO a star, center it");
    OSNAlignTP[2].setText("GOTO a star, Solve and Sync");
    OSNAlignTP[3].setText("Press 'Issue Align' if not solving");
    OSNAlignTP.apply("==>Align Started");
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


IPState LX200_OnStep::AlignAddStar ()
{
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
    OSNAlignTP[5].setText(stars);
    snprintf(stars, sizeof(stars), "%d", current_star);
    OSNAlignTP[6].setText(stars);
    snprintf(stars, sizeof(stars), "%d", align_stars);
    OSNAlignTP[7].setText(stars);
    LOGF_DEBUG("Align: max_stars: %i current star: %u, align_stars %u", max_stars, current_star, align_stars);

    /* if (align_stars > max_stars) {
     * LOGF_ERROR("Failed Sanity check, can't have more stars than max: :A?# gives: %s", read_buffer);
     * return false;
    }*/

    if (current_star <= align_stars)
    {
        snprintf(msg, sizeof(msg), "%s Manual Align: Star %d/%d", read_buffer, current_star, align_stars );
        OSNAlignTP[4].setText(msg);
    }
    if (current_star > align_stars && max_stars > 1)
    {
        LOGF_DEBUG("Align: current star: %u, align_stars %u", int(current_star), int(align_stars));
        snprintf(msg, sizeof(msg), "Manual Align: Completed");
        AlignDone();
        OSNAlignTP[4].setText(msg);
        UpdateAlignErr();
    }
    OSNAlignTP.apply();
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
    //  OSNAlignTP[4].fill("4", "Current Status", "Not Updated");
    //  OSNAlignTP[5].fill("5", "Max Stars", "Not Updated");
    //  OSNAlignTP[6].fill("6", "Current Star", "Not Updated");
    //  OSNAlignTP[7].fill("7", "# of Align Stars", "Not Updated");

    // LOG_INFO("Getting Align Error Status");
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
    OSNAlignErrTP[1].setText(polar_error);
    fs_sexa(sexabuf, (double)altCor / 3600, 4, 3600);
    snprintf(polar_error, sizeof(polar_error), "%ld'' /%s", altCor, sexabuf);
    OSNAlignErrTP[0].setText(polar_error);
    OSNAlignErrTP.apply();


    return true;
}

IPState LX200_OnStep::AlignDone()
{
    //See here https://groups.io/g/onstep/message/3624
    if (OSAlignCompleted == false)
    {
        OSAlignCompleted = true;
        LOG_INFO("Alignment Done - May still be calculating");
        OSNAlignTP[0].setText("Align FINISHED");
        OSNAlignTP[1].setText("------");
        OSNAlignTP[2].setText("Optionally press:");
        OSNAlignTP[3].setText("Write Align to NVRAM/Flash ");
        OSNAlignTP.apply();
        return IPS_OK;
    }
    return IPS_BUSY;
}

IPState LX200_OnStep::AlignWrite()
{
    //See here https://groups.io/g/onstep/message/3624
    char cmd[8];
    LOG_INFO("Sending Command to Finish Alignment and write");
    strcpy(cmd, ":AW#");
    OSNAlignTP[0].setText("Align FINISHED");
    OSNAlignTP[1].setText("------");
    OSNAlignTP[2].setText("And Written to EEPROM");
    OSNAlignTP[3].setText("------");
    OSNAlignTP.apply();
    if (sendOnStepCommandBlind(cmd))
    {
        return IPS_OK;
    }
    OSNAlignTP[0].setText("Align WRITE FAILED");
    OSNAlignTP.apply();
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

bool LX200_OnStep::OSGetOutputState(int output)
{
    //  :GXnn#   Get OnStep value
    //         Returns: value
    // nn= G0-GF (HEX!) - Output status
    //
    char value[RB_MAX_LEN] = "  ";
    char command[64] = ":$GXGm#";
    LOGF_INFO("Output: %s", char(output));
    LOGF_INFO("Command: %s", command);
    command[5] = char(output);
    LOGF_INFO("Command: %s", command);
    getCommandString(PortFD, value, command);
    if (value[0] == 0)
    {
        OSOutput1SP[0].setState(ISS_ON);
        OSOutput1SP[1].setState(ISS_OFF);
    }
    else
    {
        OSOutput1SP[0].setState(ISS_OFF);
        OSOutput1SP[1].setState(ISS_ON);
    }
    OSOutput1SP.apply();
    return true;
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
    EqNP.setState(IPS_ALERT);
    EqNP.apply();
}


//Override LX200 sync function, to allow for error returns
bool LX200_OnStep::Sync(double ra, double dec)
{

    char read_buffer[RB_MAX_LEN] = {0};
    int error_code;

    if (!isSimulation())
    {
        if (setObjectRA(PortFD, ra) < 0 || (setObjectDEC(PortFD, dec)) < 0)
        {
            EqNP.setState(IPS_ALERT);
            EqNP.apply("Error setting RA/DEC. Unable to Sync.");
            return false;
        }
        LOG_DEBUG("CMD <:CM#>");
        getCommandString(PortFD, read_buffer, ":CM#");
        LOGF_DEBUG("RES <%s>", read_buffer);
        if (strcmp(read_buffer, "N/A"))
        {
            error_code = read_buffer[1] - '0';
            LOGF_DEBUG("Sync failed with response: %s, Error code: %i", read_buffer, error_code);
            slewError(error_code);
            EqNP.setState(IPS_ALERT);
            EqNP.apply("Synchronization failed.");
            return false;
        }
    }

    currentRA  = ra;
    currentDEC = dec;

    LOG_INFO("OnStep: Synchronization successful.");

    EqNP.setState(IPS_OK);

    NewRaDec(currentRA, currentDEC);

    return true;
}

bool LX200_OnStep::saveConfigItems(FILE *fp)
{
    LX200Generic::saveConfigItems(fp);
    WI::saveConfigItems(fp);
    return true;
}
