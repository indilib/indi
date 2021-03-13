/*
    INDI IOptron v3 Driver for firmware version 20171001 or later.

    Copyright (C) 2018 Jasem Mutlaq

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
    
    Updated for PEC V3
*/

#pragma once

#include <memory>

#include "ioptronv3driver.h"
#include "indiguiderinterface.h"
#include "inditelescope.h"

/* Smart Widget-Property */
#include "indipropertytext.h"
#include "indipropertynumber.h"
#include "indipropertyswitch.h"

class IOptronV3 : public INDI::Telescope, public INDI::GuiderInterface
{
    public:

        IOptronV3();

        virtual bool ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n) override;
        virtual bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n) override;

    protected:
        virtual const char *getDefaultName() override;

        virtual bool Handshake() override;

        virtual bool initProperties() override;
        virtual bool updateProperties() override;

        virtual bool ReadScopeStatus() override;

        virtual bool MoveNS(INDI_DIR_NS dir, TelescopeMotionCommand command) override;
        virtual bool MoveWE(INDI_DIR_WE dir, TelescopeMotionCommand command) override;

        virtual bool saveConfigItems(FILE *fp) override;

        virtual bool Park() override;
        virtual bool UnPark() override;

        virtual bool Sync(double ra, double de) override;
        virtual bool Goto(double ra, double de) override;
        virtual bool Abort() override;

        virtual bool updateTime(ln_date *utc, double utc_offset) override;
        virtual bool updateLocation(double latitude, double longitude, double elevation) override;

        virtual void debugTriggered(bool enable) override;
        virtual void simulationTriggered(bool enable) override;

        // Parking
        virtual bool SetCurrentPark() override;
        virtual bool SetDefaultPark() override;

        // Track Mode
        virtual bool SetTrackMode(uint8_t mode) override;

        // Track Rate
        virtual bool SetTrackRate(double raRate, double deRate) override;

        // Track On/Off
        virtual bool SetTrackEnabled(bool enabled) override;

        // Slew Rate
        virtual bool SetSlewRate(int index) override;

        // Sim
        void mountSim();

        // Guide
        virtual IPState GuideNorth(uint32_t ms) override;
        virtual IPState GuideSouth(uint32_t ms) override;
        virtual IPState GuideEast(uint32_t ms) override;
        virtual IPState GuideWest(uint32_t ms) override;

    private:
        /**
            * @brief getStartupData Get initial mount info on startup.
            */
        void getStartupData();
        
        /** Mod v3.0 PEC Data Status
            * @brief get PEC data from the mount info.
            * @param true  = Update log
            * @param false = Don't update log
            */
        bool GetPECDataStatus(bool enabled);
        
        /* Mod v3.0 Adding PEC Recording Switches  */
        INDI::PropertySwitch PECTrainingSP {2};
        INDI::PropertyText PECInfoTP {2};
        char PECText[128];
        int PECTime = 0;
        bool isTraining;
        // End Mod */

        /* Firmware */
        INDI::PropertyText FirmwareTP {5};

        /* GPS Status */
        INDI::PropertySwitch GPSStatusSP {3};

        /* Time Source */
        INDI::PropertySwitch TimeSourceSP {3};

        /* Hemisphere */
        INDI::PropertySwitch HemisphereSP {2};

        /* Home Control */
        INDI::PropertySwitch HomeSP {3};

        /* Guide Rate */
        INDI::PropertyNumber GuideRateNP {2};

        /* Slew Mode */
        INDI::PropertySwitch SlewModeSP {2};

        /* Counterweight Status */
        INDI::PropertySwitch CWStateSP {2};

        /* Daylight Saving */
        INDI::PropertySwitch DaylightSP {2};

        uint32_t DBG_SCOPE;

        double currentRA, currentDEC;
        double targetRA, targetDEC;

        IOPv3::IOPInfo scopeInfo;
        IOPv3::FirmwareInfo firmwareInfo;

        std::unique_ptr<IOPv3::Driver> driver;
};
