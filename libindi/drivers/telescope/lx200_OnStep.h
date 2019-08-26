/*
    LX200 OnStep
    based on LX200 Classic azwing (alain@zwingelstein.org)
    Contributors:
    James Lancaster https://github.com/james-lan
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

    ===========================================
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
    - Azwing set all com variable legth to RB_MAX_LEN otherwise crash due to overflow
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

#include <cstring>
#include <unistd.h>
#include <termios.h>
#include <stdlib.h>

#define RB_MAX_LEN 64

#define setParkOnStep(fd)  write(fd, "#:hQ#", 5)
#define ReticPlus(fd)      write(fd, "#:B+#", 5)
#define ReticMoins(fd)     write(fd, "#:B-#", 5)
#define OnStepalign1(fd)   write(fd, "#:A1#", 5)
#define OnStepalign2(fd)   write(fd, "#:A2#", 5)
#define OnStepalign3(fd)   write(fd, "#:A3#", 5)
#define OnStepalignOK(fd)   write(fd, "#:A+#", 5)
#define OnStep
#define RB_MAX_LEN 64

#define PORTS_COUNT 10
#define STARTING_PORT 0



enum Errors {ERR_NONE, ERR_MOTOR_FAULT, ERR_ALT_MIN, ERR_LIMIT_SENSE, ERR_DEC, ERR_AZM, ERR_UNDER_POLE, ERR_MERIDIAN, ERR_SYNC, ERR_PARK, ERR_GOTO_SYNC, ERR_UNSPECIFIED, ERR_ALT_MAX, ERR_GOTO_ERR_NONE, ERR_GOTO_ERR_BELOW_HORIZON, ERR_GOTO_ERR_ABOVE_OVERHEAD, ERR_GOTO_ERR_STANDBY, ERR_GOTO_ERR_PARK, ERR_GOTO_ERR_GOTO, ERR_GOTO_ERR_OUTSIDE_LIMITS, ERR_GOTO_ERR_HARDWARE_FAULT, ERR_GOTO_ERR_IN_MOTION, ERR_GOTO_ERR_UNSPECIFIED};
enum RateCompensation {RC_NONE, RC_REFR_RA, RC_REFR_BOTH, RC_FULL_RA, RC_FULL_BOTH}; //To allow for using one variable instead of two in the future



class LX200_OnStep : public LX200Generic
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
    virtual bool SetTrackRate(double raRate, double deRate) override;
    virtual void slewError(int slewCode) override; 
    virtual bool Sync(double ra, double dec) override;
    
    //Mount information 
    int OSMountType = 0;
    /*  0 = EQ mount  (Presumed default for most things.) 
     *  1 = Fork 
     *  2 = Fork Alt 
     *  3 = Alt Azm
     */
   
    
    //FocuserInterface
    
    IPState MoveFocuser(FocusDirection dir, int speed, uint16_t duration) override;
    IPState MoveAbsFocuser (uint32_t targetTicks) override;
    IPState MoveRelFocuser (FocusDirection dir, uint32_t ticks) override;
    bool AbortFocuser () override;

    
    //End FocuserInterface
    
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
    //End PECInterface
    
    
    //NewGeometricAlignment    
    IPState AlignStartGeometric(int stars);
    IPState AlignAddStar();
    IPState AlignDone();
    IPState AlignWrite();
    virtual bool UpdateAlignStatus();
    virtual bool UpdateAlignErr();
    //End NewGeometricAlignment 
    bool OSAlignCompleted=false;
    
    //Outputs
    IPState OSEnableOutput(int output);
    IPState OSDisableOutput(int output);
    bool OSGetOutputState(int output);
    

    bool sendOnStepCommand(const char *cmd);
    bool sendOnStepCommandBlind(const char *cmd);
    int  setMaxElevationLimit(int fd, int max);
    void OSUpdateFocuser();

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
    INumber MaxSlewRateN[2];

    INumberVectorProperty BacklashNP;    //test
    INumber BacklashN[2];    //Test

    INumberVectorProperty ElevationLimitNP;
    INumber ElevationLimitN[2];

    ITextVectorProperty VersionTP;
    IText VersionT[5] {};

    // OnStep Status controls
    ITextVectorProperty OnstepStatTP;
    IText OnstepStat[10] {};

    // Focuser controls
    // Focuser 1
    bool OSFocuser1 = false;
    ISwitchVectorProperty OSFocus1InitializeSP;
    ISwitch OSFocus1InitializeS[4];

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


    int IsTracking = 0;

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
    
    ISwitchVectorProperty SetHomeSP;
    ISwitch SetHomeS[2];

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
    
    ISwitchVectorProperty OSNAlignStarsSP;
    ISwitch OSNAlignStarsS[9];
    ISwitchVectorProperty OSNAlignSP;
    ISwitch OSNAlignS[4];
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
    
    
  private:
    int currentCatalog;
    int currentSubCatalog;
    bool FirstRead=true;

};
