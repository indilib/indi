/*
    Celestron GPS
    Copyright (C) 2003-2017 Jasem Mutlaq (mutlaqja@ikarustech.com)

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

/*
    Version with experimental pulse guide support. GC 04.12.2015
*/

#pragma once

#include "celestrondriver.h"

#include "indiguiderinterface.h"
#include "inditelescope.h"
#include "indifocuserinterface.h"

class CelestronGPS : public INDI::Telescope, public INDI::GuiderInterface, public INDI::FocuserInterface
{
    public:
        CelestronGPS();

        virtual const char *getDefaultName() override;
        virtual bool Handshake() override;
        virtual bool ReadScopeStatus() override;
        virtual void ISGetProperties(const char *dev) override;
        virtual bool ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n) override;
        virtual bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n) override;
        virtual bool initProperties() override;
        virtual bool updateProperties() override;

        //GUIDE guideTimeout() funcion
        void guideTimeout(CELESTRON_DIRECTION calldir);

    protected:
        // Goto, Sync, and Motion
        virtual bool Goto(double ra, double dec) override;
        //bool GotoAzAlt(double az, double alt);
        virtual bool Sync(double ra, double dec) override;
        virtual bool MoveNS(INDI_DIR_NS dir, TelescopeMotionCommand command) override;
        virtual bool MoveWE(INDI_DIR_WE dir, TelescopeMotionCommand command) override;
        virtual bool Abort() override;

        // Time and Location
        virtual bool updateLocation(double latitude, double longitude, double elevation) override;
        virtual bool updateTime(ln_date *utc, double utc_offset) override;

        //GUIDE: guiding functions
        virtual IPState GuideNorth(uint32_t ms) override;
        virtual IPState GuideSouth(uint32_t ms) override;
        virtual IPState GuideEast(uint32_t ms) override;
        virtual IPState GuideWest(uint32_t ms) override;

        //GUIDE guideTimeoutHelper() function
        static void guideTimeoutHelperN(void *p);
        static void guideTimeoutHelperS(void *p);
        static void guideTimeoutHelperW(void *p);
        static void guideTimeoutHelperE(void *p);

        // Focus Backlash
        virtual bool SetFocuserBacklash(int32_t steps) override;

        // Tracking
        virtual bool SetTrackMode(uint8_t mode) override;
        virtual bool SetTrackEnabled(bool enabled) override;

        // Parking
        virtual bool Park() override;
        virtual bool UnPark() override;
        virtual bool SetCurrentPark() override;
        virtual bool SetDefaultPark() override;

        virtual bool saveConfigItems(FILE *fp) override;

        virtual void simulationTriggered(bool enable) override;

        void mountSim();

        //GUIDE variables.
        int GuideNSTID;
        int GuideWETID;
        CELESTRON_DIRECTION guide_direction;

        /* Firmware */
        IText FirmwareT[5] {};
        ITextVectorProperty FirmwareTP;

        //INumberVectorProperty HorizontalCoordsNP;
        //INumber HorizontalCoordsN[2];

        //ISwitch TrackS[4];
        //ISwitchVectorProperty TrackSP;

        // Celestron Track Mode (AltAz, EQ N, EQ S)
        ISwitchVectorProperty CelestronTrackModeSP;
        ISwitch CelestronTrackModeS[3];

        //GUIDE Pulse guide switch
        ISwitchVectorProperty UsePulseCmdSP;
        ISwitch UsePulseCmdS[2];

        ISwitchVectorProperty UseHibernateSP;
        ISwitch UseHibernateS[2];

        //FocuserInterface

        IPState MoveAbsFocuser (uint32_t targetTicks) override;
        IPState MoveRelFocuser (FocusDirection dir, uint32_t ticks) override;
        bool AbortFocuser () override;



        //End FocuserInterface

    private:
        bool setCelestronTrackMode(CELESTRON_TRACK_MODE mode);
        bool checkMinVersion(float minVersion, const char *feature, bool debug = false);
        void checkAlignment();

        double currentRA, currentDEC, currentAZ, currentALT;
        double targetRA, targetDEC, targetAZ, targetALT;

        CelestronDriver driver;
        FirmwareInfo fwInfo;
        bool usePreciseCoords {false};
        bool usePulseCommand { false };

        // experimental last align property
        ISwitch LastAlignS[1];
        ISwitchVectorProperty LastAlignSP;

        bool slewToIndex;

        // focuser
        //        INumber FocusBacklashN[1];
        //        INumberVectorProperty FocusBacklashNP;

        INumber FocusMinPosN[1];
        INumberVectorProperty FocusMinPosNP;

        bool focusBacklashMove;      // set if a final move is needed
        uint32_t focusPosition;
        bool focusReadLimits();
        bool focuserIsCalibrated;
};
