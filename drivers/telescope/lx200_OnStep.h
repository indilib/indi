/*
    LX200 OnStep
    based on LX200 Classic azwing (alain@zwingelstein.org)
    Contributors:
    James Lancaster https://github.com/james-lan
    Ray Wells https://github.com/blueshawk
    Jamie Flinn https://github.com/jamiecflinn

    Copyright (C) 2003 Jasem Mutlaq (mutlaqja@ikarustech.com)-2021 (Contributors, above)

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

    ===========================================

    Version 1.24: During manual slew, only send back RA & DE.
    Version not yet updated/No INDI release:
    Version 1.22
    - fixed #:AW#" and ":MP#" commands by using getCommandSingleCharResponse instead of sendOnStepCommandBlind
    Version 1.21
    - fixed Onstep returning '9:9' when 9 star alignment is achieved thanks to Howard Dutton
    Version 1.20
    - fixed wrong messages due to different return with OnStepX
    - fixed Focuser Temperature not shown on Ekos
    - fixed Weather settings (P/T/Hr) when no sensor present
    - minor typos
    Version 1.19
    - fixed typo on debug information saying error instead of nbchar causing confusion
    - fixed Autoflip Off update
    - fixed Elevation Limits update (was not read from OnStep) and format set to integer and gage for setup
    - fixed minutes passed meridian not showing actual values
    - fixed missing slewrates defineProperty and deleteProperty causing redefinitions of overrides
    - todo focuser stops working after some time ??? could not yet reproduce
    - fixed poll and update slew rates
    - todo poll and update maximum slew speed SmartWebServer=>Settings
    Version 1.18
    - implemented Focuser T° compensation in FOCUSER TAB
    - Minor fixes
    Version 1.17
    - fixed setMaxElevationLimit / setMinElevationLimit
    Version 1.16
    - fixed uninitialized UTC structure thanks to Norikyu
    Version 1.15
    - Fixed setUTCOffset after change in lx200driver to comply with OnStep format :SG[sHH]#
    Version 1.14
    - Modified range for Minutes Pas Meridian East and West to -180 .. +180
    - Modified debug messages Minutes Pas Meridian (Was B"acklash ...)

    Version 1.13
    - Timeouts and misc errors due to new behavior of SWS (SmartWebServer)
    - - Timeouts still at 100ms for USB connections, if on a TCP/network connection timeout reverts to 2 sec.
    - Improvements to Focuser and Rotator polling
    - Focuser doesn't show up if not detected (Regression fixed)

    Past Versions:

    Version 1.12: (INDI 1.9.3)
    - New timeout functions in INDI which significantly reduce startup times waiting for detection to fail. (Min time before was 1 second, current timeout for those is now set to 100 ms (100000 us which works well even with an Arduino Mega (Ramps) setup)
    - Cleanup and completely control TrackState. (Should eliminate various issues.)
    - Behind the scenes: More consistent command declarations (Should eliminate a type of error that's happened in the past when changing commands.)
    - Don't report capability for PierSide and PEC unless supported (This will cause a call to updateProperties so a bunch of messages will be repeated.)
    - From the last, move where the SlewRate values are defined to updateProperties, vs initProperties so that the extra calls to updateProperties don't mangle it.
    - TMC driver reports are now human readable.
    - Detects OnStep or OnStepX version (doesn't do much with it.)


    Version 1.11: (INDI 1.9.2)
    - Fixed one issue with tracking (Jamie Flinn/jamiecflinn)
    Version 1.10: (finalized: INDI 1.9.1)
    - Weather support for setting temperature/humidity/pressure, values will be overridden in OnStep by any sensor values.
    - Ability to swap primary focuser.
    - High precision on location, and not overriding GPS even when marked for Mount > KStars.
    - Added Rotator & De-Rotator Support
    - TMC_SPI status reported (RAW) on the Status Tab. (ST = Standstill, Ox = open load A/B, Gx = grounded A/B, OT = Overtemp Shutdown, PW = Overtemp Prewarning)
    - Manage OnStep Auxiliary Feature Names in Output Tab

    Version 1.9:
    - Weather support for Reading temperature/humidity/pressure (Values are Read-Only)
    - Bugfix: Slew speed

    Version 1.8:
    - Bugfixes for FORK mounted scopes

    Version 1.7:
    - Added support for Reporting Guide rate (to PHD2 among others)
    - Updated Error codes to match up with Android/SHC (Unknown reserved for unknown, so Unspecified = Unknown on other platforms)
    - Added descriptions to SlewRate to match, slider kept which matches OnStep values
    - Support for up to 9 stars for alignment
    - Changed align so the last step isn't the (Optional) Write to EEPROM
    - Added support for polar adjustments, without having to redo the entire model. (:MP# command)
    - Support for Full Compensation/Refraction only, and 1/2 Axis tracking
    - Cleanups

    Version 1.6: Additional Functions
    - James Lan fixed Meredian Flip and Home Pause buttons
    - Cleaned Comments from previon versions
    - Updated lastError Codes
    - azwing typo minutes ' > second ''for Alignment Error

    Version 1.5: Cleaning and Align Code Tuning
    - James Lan Align Code
    - Cleaning out debug messages
    - Fix some TrackState inconsistencies with Ekos
    - removing old Align Code
    - fixing reversed Alt/Azm coorection values

    Version 1.4: Tuning
    - James Lan implementation of High Precision Tracking
    - James lan Focuser Code
    - James lan PEC
    - James Lan Alignment
    - Azwing set all com variable length to RB_MAX_LEN otherwise crash due to overflow
    - Azwing set local variable size to RB_MAX_LEN otherwise erased by overflow preventing Align and other stuf to work
    - James Lan Align Tab implementation
    - Azwing Removed Alignment in main tab
    - Azwing minor typo fixes
    - Azwing reworked TrackState especially for parking/unparking

    Version 1.3: Complete rework of interface and functionalities
    - Telescope Status using :GU#
    - Parking Management
    - Star Alignment
    - Tracking Frequency
    - Focuser rework

    Version 1.2: Initial issue

*/

#pragma once

#include "lx200generic.h"
#include "lx200driver.h"
#include "indicom.h"
#include "indifocuserinterface.h"
#include "indiweatherinterface.h"
#include "indirotatorinterface.h"
#include "connectionplugins/connectiontcp.h"


#include <cstring>
#include <unistd.h>
#include <termios.h>
#include <stdlib.h>

#define RB_MAX_LEN 64
#define CMD_MAX_LEN 32

#define PORTS_COUNT 10
#define STARTING_PORT 0

enum ResponseErrors {RES_ERR_FORMAT = -1001};

enum Errors {ERR_NONE, ERR_MOTOR_FAULT, ERR_ALT_MIN, ERR_LIMIT_SENSE, ERR_DEC, ERR_AZM, ERR_UNDER_POLE, ERR_MERIDIAN, ERR_SYNC, ERR_PARK, ERR_GOTO_SYNC, ERR_UNSPECIFIED, ERR_ALT_MAX, ERR_GOTO_ERR_NONE, ERR_GOTO_ERR_BELOW_HORIZON, ERR_GOTO_ERR_ABOVE_OVERHEAD, ERR_GOTO_ERR_STANDBY, ERR_GOTO_ERR_PARK, ERR_GOTO_ERR_GOTO, ERR_GOTO_ERR_OUTSIDE_LIMITS, ERR_GOTO_ERR_HARDWARE_FAULT, ERR_GOTO_ERR_IN_MOTION, ERR_GOTO_ERR_UNSPECIFIED};
enum RateCompensation {RC_NONE, RC_REFR_RA, RC_REFR_BOTH, RC_FULL_RA, RC_FULL_BOTH}; //To allow for using one variable instead of two in the future

enum MountType {MOUNTTYPE_GEM, MOUNTTYPE_FORK, MOUNTTYPE_FORK_ALT, MOUNTTYPE_ALTAZ};

enum OnStepVersion {OSV_UNKNOWN, OSV_OnStepV1or2, OSV_OnStepV3, OSV_OnStepV4, OSV_OnStepV5, OSV_OnStepX};

class LX200_OnStep : public LX200Generic, public INDI::WeatherInterface, public INDI::RotatorInterface
{
    public:
        LX200_OnStep();
        ~LX200_OnStep() {}

        virtual const char *getDefaultName() override;
        virtual bool initProperties() override;
        virtual void ISGetProperties(const char *dev) override;
        virtual bool updateProperties() override;
        virtual bool ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n) override;
        virtual bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n) override;
        virtual bool Handshake() override;

    protected:
        virtual void getBasicData() override;
        virtual bool Park() override;
        virtual bool UnPark() override;
        virtual bool SetCurrentPark() override;
        virtual bool SetDefaultPark() override;
        virtual bool SetTrackEnabled(bool enabled) override;
        virtual bool updateLocation(double latitude, double longitude, double elevation) override;
        virtual bool setLocalDate(uint8_t days, uint8_t months, uint16_t years) override;
        virtual bool ReadScopeStatus() override;
        virtual int setSiteLongitude(int fd, double Long);
        virtual int setSiteLatitude(int fd, double Long);
        virtual bool SetTrackRate(double raRate, double deRate) override;
        virtual void slewError(int slewCode) override;
        virtual bool Sync(double ra, double dec) override;

        virtual bool MoveNS(INDI_DIR_NS dir, TelescopeMotionCommand command) override;
        virtual bool MoveWE(INDI_DIR_WE dir, TelescopeMotionCommand command) override;

        virtual bool saveConfigItems(FILE *fp) override;
        virtual void Init_Outputs();

        //Mount information
        MountType OSMountType = MOUNTTYPE_GEM; //default to GEM
        /*  0 = EQ mount  (Presumed default for most things.)
        *  1 = Fork
        *  2 = Fork Alt
        *  3 = Alt Azm
        */

        virtual bool sendScopeTime() override;
        virtual bool sendScopeLocation() override;
        virtual bool setUTCOffset(double offset) override; //azwing fix after change in lx200driver.cpp

        // Goto
        virtual bool Goto(double ra, double dec) override;

        //FocuserInterface

        IPState MoveFocuser(FocusDirection dir, int speed, uint16_t duration) override;
        IPState MoveAbsFocuser (uint32_t targetTicks) override;
        IPState MoveRelFocuser (FocusDirection dir, uint32_t ticks) override;
        bool AbortFocuser () override;

        //End FocuserInterface

        //RotatorInterface

        IPState MoveRotator(double angle) override;
        IPState HomeRotator() override;
        bool AbortRotator() override;
        bool SetRotatorBacklash (int32_t steps) override;
        bool SetRotatorBacklashEnabled(bool enabled) override;

        // Homing
        virtual IPState ExecuteHomeAction(TelescopeHomeAction action) override;

        //End RotatorInterface

        //PECInterface
        //axis 0=RA, 1=DEC, others?
        IPState StopPECPlayback (int axis);
        IPState StartPECPlayback (int axis);
        IPState ClearPECBuffer (int axis);
        IPState StartPECRecord (int axis);
        IPState SavePECBuffer (int axis);
        IPState PECStatus (int axis);
        IPState ReadPECBuffer (int axis);
        IPState WritePECBuffer (int axis);
        bool ISPECRecorded (int axis);
        bool OSPECEnabled = false;
        bool OSPECviaGU = false; //Older versions use :QZ# for PEC status, new can use the standard :GU#/:Gu#
        //End PECInterface


        //NewGeometricAlignment
        IPState AlignStartGeometric(int stars);

        /**
         * @brief AlignStartGeometric starts the OnStep Multistar align process.
         * @brief Max of 9 stars,
         * @param stars Number of stars to be included. If stars is more than the controller supports, it will be reduced.
         * @return IPS_BUSY if no issues, IPS_ALERT if commands don't get the expected response.
         */

        IPState AlignAddStar();
        IPState AlignDone();
        IPState AlignWrite();
        virtual bool UpdateAlignStatus();
        virtual bool UpdateAlignErr();
        //End NewGeometricAlignment
        bool OSAlignCompleted = false;

        //Outputs
        IPState OSEnableOutput(int output);
        IPState OSDisableOutput(int output);
        bool OSGetOutputState(int output);

        // Reset slew rate labels
        void initSlewRates();

        bool sendOnStepCommand(const char *cmd);
        bool sendOnStepCommandBlind(const char *cmd);
        int flushIO(int fd);
        int getCommandSingleCharResponse(int fd, char *data, const char *cmd); //Reimplemented from getCommandString
        int getCommandSingleCharErrorOrLongResponse(int fd, char *data, const char *cmd); //Reimplemented from getCommandString
        int getCommandDoubleResponse(int fd, double *value, char *data,
                                     const char *cmd); //Reimplemented from getCommandString Will return a double, and raw value.
        int getCommandIntResponse(int fd, int *value, char *data, const char *cmd);
        int  setMinElevationLimit(int fd, int min);
        int OSUpdateFocuser(); //Return = 0 good, -1 = Communication error
        int OSUpdateRotator(); //Return = 0 good, -1 = Communication error

        ITextVectorProperty ObjectInfoTP;
        IText ObjectInfoT[1] {};

        ISwitchVectorProperty StarCatalogSP;
        ISwitch StarCatalogS[3];

        ISwitchVectorProperty DeepSkyCatalogSP;
        ISwitch DeepSkyCatalogS[7];

        ISwitchVectorProperty SolarSP;
        ISwitch SolarS[10];

        INumberVectorProperty ObjectNoNP;
        INumber ObjectNoN[1];

        INumberVectorProperty MaxSlewRateNP;
        INumber MaxSlewRateN[1];

        INumberVectorProperty BacklashNP;    //test
        INumber BacklashN[2];    //Test

        INumberVectorProperty ElevationLimitNP;
        INumber ElevationLimitN[2];

        ITextVectorProperty VersionTP;
        IText VersionT[5] {};

        OnStepVersion OnStepMountVersion = OSV_UNKNOWN;

        long int OSTimeoutSeconds = 0;
        long int OSTimeoutMicroSeconds = 100000;

        // OnStep Status controls
        ITextVectorProperty OnstepStatTP;
        IText OnstepStat[11] {};

        bool TMCDrivers = true; //Set to false if it doesn't detect TMC_SPI reporting. (Small delay on connection/first update)
        bool OSHighPrecision = false;

        // Focuser controls
        // Focuser 1
        bool OSFocuser1 = false;
        ISwitchVectorProperty OSFocus1InitializeSP;
        ISwitch OSFocus1InitializeS[4];

        // Focus T° Compensation
        INumberVectorProperty FocusTemperatureNP;
        INumber FocusTemperatureN[2];

        ISwitchVectorProperty TFCCompensationSP;
        ISwitch TFCCompensationS[2];
        INumberVectorProperty TFCCoefficientNP;
        INumber TFCCoefficientN[1];
        INumberVectorProperty TFCDeadbandNP;
        INumber TFCDeadbandN[1];
        // End Focus T° Compensation

        int OSNumFocusers = 0;
        ISwitchVectorProperty OSFocusSelectSP;
        ISwitch OSFocusSelectS[9];

        // Focuser 2
        //ISwitchVectorProperty OSFocus2SelSP;
        //ISwitch OSFocus2SelS[2];
        bool OSFocuser2 = false;
        ISwitchVectorProperty OSFocus2RateSP;
        ISwitch OSFocus2RateS[4];

        ISwitchVectorProperty OSFocus2MotionSP;
        ISwitch OSFocus2MotionS[3];

        INumberVectorProperty OSFocus2TargNP;
        INumber OSFocus2TargN[1];

        //Rotator - Some handled by RotatorInterface, but that's mostly for rotation only, absolute, and... very limited.
        bool OSRotator1 = false; //Change to false after detection code
        ISwitchVectorProperty OSRotatorRateSP;
        ISwitch OSRotatorRateS[4]; //Set rate

        ISwitchVectorProperty OSRotatorDerotateSP;
        ISwitch OSRotatorDerotateS[2]; //On or Off

        int IsTracking = 0;
        uint32_t m_RememberPollingPeriod {1000};

        // Reticle +/- Buttons
        ISwitchVectorProperty ReticSP;
        ISwitch ReticS[2];

        // Align Buttons
        ISwitchVectorProperty TrackCompSP;
        ISwitch TrackCompS[3];

        ISwitchVectorProperty TrackAxisSP;
        ISwitch TrackAxisS[3];

        ISwitchVectorProperty FrequencyAdjustSP;
        ISwitch FrequencyAdjustS[3];

        ISwitchVectorProperty AutoFlipSP;
        ISwitch AutoFlipS[2];

        ISwitchVectorProperty HomePauseSP;
        ISwitch HomePauseS[3];

        // ISwitchVectorProperty SetHomeSP;
        // ISwitch SetHomeS[2];

        ISwitchVectorProperty PreferredPierSideSP;
        ISwitch PreferredPierSideS[3];

        INumberVectorProperty minutesPastMeridianNP;
        INumber minutesPastMeridianN[2];

        ISwitchVectorProperty OSPECStatusSP;
        ISwitch OSPECStatusS[5];
        ISwitchVectorProperty OSPECIndexSP;
        ISwitch OSPECIndexS[2];
        ISwitchVectorProperty OSPECRecordSP;
        ISwitch OSPECRecordS[3];
        ISwitchVectorProperty OSPECReadSP;
        ISwitch OSPECReadS[2];
        INumberVectorProperty OSPECCurrentIndexNP;
        INumber OSPECCurrentIndexN[2];
        INumberVectorProperty OSPECUserIndexNP;
        INumber OSPECUserIndexN[2];
        INumberVectorProperty OSPECRWValuesNP;
        INumber OSPECRWValuesN[2]; //Current Index  and User Index

        ISwitchVectorProperty OSNAlignStarsSP;
        ISwitch OSNAlignStarsS[9];
        ISwitchVectorProperty OSNAlignSP;
        ISwitch OSNAlignS[2];
        ISwitchVectorProperty OSNAlignWriteSP;
        ISwitch OSNAlignWriteS[1];
        ISwitchVectorProperty OSNAlignPolarRealignSP;
        ISwitch OSNAlignPolarRealignS[2];
        IText OSNAlignT[8] {};
        ITextVectorProperty OSNAlignTP;
        IText OSNAlignErrT[4] {};
        ITextVectorProperty OSNAlignErrTP;
        char OSNAlignStat[RB_MAX_LEN];

        ISwitchVectorProperty OSOutput1SP;
        ISwitch OSOutput1S[2];
        ISwitchVectorProperty OSOutput2SP;
        ISwitch OSOutput2S[2];


        INumber OutputPorts[PORTS_COUNT];
        INumberVectorProperty OutputPorts_NP;
        bool OSHasOutputs = true;

        INumber GuideRateN[2];
        INumberVectorProperty GuideRateNP;

        char OSStat[RB_MAX_LEN];
        char OldOSStat[RB_MAX_LEN];


        char OSPier[RB_MAX_LEN];
        char OldOSPier[RB_MAX_LEN];

        bool OSSeparate_Pulse_Guide_Rate = false;
        bool OSSupports_bitfield_Gu = false;
        uint8_t PECStatusGU = 0;
        uint8_t ParkStatusGU = 0;

        // Weather support
        // NOTE: Much is handled by WeatherInterface, these controls are mainly for setting values which are not detected
        // As of right now, if there is a sensor the values will be overwritten on the next update
        bool OSCpuTemp_good =
            true; //This can fail on some processors and take the timeout before an update, so if it fails, don't check again.


        INumberVectorProperty OSSetTemperatureNP;
        INumber OSSetTemperatureN[1];
        INumberVectorProperty OSSetHumidityNP;
        INumber OSSetHumidityN[1];
        INumberVectorProperty OSSetPressureNP;
        INumber OSSetPressureN[1];
        //Not sure why this would be used, but will feed to it from site settings
        INumberVectorProperty OSSetAltitudeNP;
        INumber OSSetAltitudeN[1];


        //This is updated via other commands, as such I'm going to ignore it like some others do.
        virtual IPState updateWeather() override
        {
            return IPS_OK;
        }


        /**
         * @brief SyncParkStatus Update the state and switches for parking
         * @param isparked True if parked, false otherwise.
         */
        virtual void SyncParkStatus(bool isparked) override;

        /**
         * @brief SetParked Change the mount parking status. The data park file (stored in
         * ~/.indi/ParkData.xml) is updated in the process.
         * @param isparked set to true if parked, false otherwise.
         */
        virtual void SetParked(bool isparked) override;

        /**
         * @brief PrintTrackState will print to the debug log the status of TrackState if
         * DEBUG_TRACKSTATE is defined otherwise it will simply return.
         */
        // #define DEBUG_TRACKSTATE
        void PrintTrackState();

    private:
        int currentCatalog;
        int currentSubCatalog;
};
