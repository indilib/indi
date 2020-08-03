/*
    LX200_TeenAstro 

    Based on LX200_OnStep and others
    François Desvallées https://github.com/fdesvallees

    Copyright (C) 2005 Jasem Mutlaq (mutlaqja@ikarustech.com)

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

#pragma once

#include "indiguiderinterface.h"
#include "inditelescope.h"
#include "lx200driver.h"
#include "indicom.h"

#define RB_MAX_LEN 64

class LX200_TeenAstro : public INDI::Telescope, public INDI::GuiderInterface
{
    public:
        LX200_TeenAstro();
        ~LX200_TeenAstro() override = default;

        virtual const char *getDefaultName() override;
        virtual const char *getDriverName() override;
        virtual bool Handshake() override;
        virtual bool ReadScopeStatus() override;
        virtual void ISGetProperties(const char *dev) override;
        virtual bool initProperties() override;
        virtual bool updateProperties() override;

        virtual bool ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n) override;
        virtual bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n) override;
        virtual bool ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n) override;
 
    protected:
        virtual bool MoveNS(INDI_DIR_NS dir, TelescopeMotionCommand command) override;
        virtual bool MoveWE(INDI_DIR_WE dir, TelescopeMotionCommand command) override;
        virtual bool Abort() override;
        virtual bool Goto(double, double) override;
        virtual bool Sync(double ra, double dec) override;
        virtual void getBasicData();   // Initial function to get data after connection is successful
        virtual bool Park() override;
        virtual bool UnPark() override;
        virtual bool SetCurrentPark() override;
        virtual bool SetTrackEnabled(bool enabled) override;
        virtual bool updateLocation(double latitude, double longitude, double elevation) override;
        virtual void debugTriggered(bool enable) override;
        virtual bool SetTrackMode(uint8_t mode) override;
        //GUIDE: guiding functions
        virtual IPState GuideNorth(uint32_t ms) override;
        virtual IPState GuideSouth(uint32_t ms) override;
        virtual IPState GuideEast(uint32_t ms) override;
        virtual IPState GuideWest(uint32_t ms) override;
        virtual bool saveConfigItems(FILE *fp) override;

        
    private:
        bool Move(TDirection dir, TelescopeMotionCommand command);
        bool selectSlewRate(int index);
        bool isSlewComplete();
        void slewError(int slewCode);
        void mountSim();
        bool SetGuideRate(int);

        bool getLocalDate(char *dateString);
        bool setLocalDate(uint8_t days, uint8_t months, uint16_t years);

        bool getSiteIndex(int *ndxP);
        bool getSlewRate(int *srP);
        bool setSite(int ndx);       // used instead of selectSite from lx200 driver
        bool getSiteElevation(int *elevationP);
        bool setSiteElevation(double elevation);
        bool getLocation(void);     // read sites from TeenAstro

        // Get Local time in 24 hour format from mount. Expected format is HH:MM:SS
        bool getLocalTime(char *timeString);
        bool setLocalTime24(uint8_t hour, uint8_t minute, uint8_t second);

        // Return UTC Offset from mount in hours.
        bool setUTCOffset(double offset);
        bool getUTFOffset(double * offset);
        
        void handleStatusChange(void);
        void SendPulseCmd(int8_t direction, uint32_t duration_msec);
        void sendCommand(const char *cmd);
        void updateMountStatus(char);
   
        // Send Mount time and location settings to client
        bool sendScopeTime();
        bool sendScopeLocation();

        // User interface

        INumber SlewAccuracyN[2];
        INumberVectorProperty SlewAccuracyNP;

        ISwitchVectorProperty HomePauseSP;
        ISwitch HomePauseS[3];    
        
        ISwitchVectorProperty SetHomeSP;
        ISwitch SetHomeS[2];

        ITextVectorProperty VersionTP;
        IText VersionT[5] {};

        ISwitch SlewRateS[5];
        ISwitchVectorProperty SlewRateSP;

        ISwitch GuideRateS[3];
        ISwitchVectorProperty GuideRateSP;

        ISwitch TATrackModeS[3];
        ISwitchVectorProperty TATrackModeSP;

        // Site Management 
        ISwitchVectorProperty SiteSP;
        ISwitch SiteS[4];
        int currentSiteNum {0}; // on TeenAstro, sites are numbered 0 to 3, not 1 to 4 like on the Meade standard

        // Site Name
        ITextVectorProperty SiteNameTP;
        IText SiteNameT[1] {};

        // Error Status
        ITextVectorProperty ErrorStatusTP;
        IText ErrorStatusT[1] {};

        double targetRA = 0, targetDEC = 0;
        double currentRA = 0, currentDEC = 0;
        uint32_t DBG_SCOPE = 0;
        char OSStat[RB_MAX_LEN];
        char OldOSStat[RB_MAX_LEN];
        const char *statusCommand;           // :GU# for version 1.1, :GXI# for 1.2 and later
        const char *guideSpeedCommand;       // :SX90# or SXR0

};
