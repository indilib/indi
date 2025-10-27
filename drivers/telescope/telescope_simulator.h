/*******************************************************************************
 Copyright(c) 2015 Jasem Mutlaq. All rights reserved.

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Library General Public
 License version 2 as published by the Free Software Foundation.
 .
 This library is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 Library General Public License for more details.
 .
 You should have received a copy of the GNU Library General Public License
 along with this library; see the file COPYING.LIB.  If not, write to
 the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 Boston, MA 02110-1301, USA.
*******************************************************************************/

#pragma once

#include "indiguiderinterface.h"
#include "inditelescope.h"
#include "scopesim_helper.h"

#define USE_SIM_TAB

/**
 * @brief The ScopeSim class provides a simple mount simulator of an equatorial mount.
 *
 * It supports the following features:
 * + Sidereal and Custom Tracking rates.
 * + Goto & Sync
 * + NWSE Hand controller direction key slew.
 * + Tracking On/Off.
 * + Parking & Unparking with custom parking positions.
 * + Setting Time & Location.
 *
 * On startup and by default the mount shall point to the celestial pole.
 *
 * @author Jasem Mutlaq
 */
class ScopeSim : public INDI::Telescope, public INDI::GuiderInterface
{
    public:
        ScopeSim();
        virtual ~ScopeSim() = default;

        virtual const char *getDefaultName() override;
        virtual bool Connect() override;
        virtual bool Disconnect() override;
        virtual bool ReadScopeStatus() override;
        virtual bool initProperties() override;
        virtual void ISGetProperties(const char *dev) override;
        virtual bool updateProperties() override;

        virtual bool ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n) override;
        virtual bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n) override;

    protected:
        // Slew Rate
        virtual bool MoveNS(INDI_DIR_NS dir, TelescopeMotionCommand command) override;
        virtual bool MoveWE(INDI_DIR_WE dir, TelescopeMotionCommand command) override;
        virtual bool Abort() override;

        virtual IPState GuideNorth(uint32_t ms) override;
        virtual IPState GuideSouth(uint32_t ms) override;
        virtual IPState GuideEast(uint32_t ms) override;
        virtual IPState GuideWest(uint32_t ms) override;

        virtual bool SetTrackMode(uint8_t mode) override;
        virtual bool SetTrackEnabled(bool enabled) override;
        virtual bool SetTrackRate(double raRate, double deRate) override;

        virtual bool Goto(double, double) override;
        virtual bool Park() override;
        virtual bool UnPark() override;
        virtual bool Sync(double ra, double dec) override;

        // Parking
        virtual bool SetCurrentPark() override;
        virtual bool SetDefaultPark() override;
        virtual bool updateLocation(double latitude, double longitude, double elevation) override;

        // Home
        virtual IPState ExecuteHomeAction(TelescopeHomeAction action) override;

        virtual bool saveConfigItems(FILE *fp) override;

    private:
        double m_currentRA { 0 };
        double m_currentDEC { 90 };
        double m_currentAz { 180 };
        double m_currentAlt { 0 };
        double m_targetRA { 0 };
        double m_targetDEC { 0 };
        double m_sinLat, m_cosLat;

        /// used by GoTo and Park
        void StartSlew(double ra, double dec, TelescopeStatus status);

        // bool forceMeridianFlip { false }; // #PS: unused
        unsigned int DBG_SCOPE { 0 };

        int mcRate = 0;

        //    double guiderEWTarget[2];
        //    double guiderNSTarget[2];

        bool guidingNS = false;
        bool guidingEW = false;

        uint32_t applyDecBacklash(double rate, uint32_t ms);
        uint32_t backlashComputation(uint32_t ms);

        // In milliseconds. Doesn't depend on guide rate.
        uint32_t m_DecGuideBacklashMs { 30 };
        // True if last dec guide pulse was north. Actually a little more complicated
        // than that because of pier side...
        bool m_GuideDecLastNorth = false;
        // Will be between 0 and decGuideBacklash. When guide pulses change directions,
        // a pulse may not completely clear the backlash. This is how much is left
        // after that pulse.
        uint32_t m_DecGuideRemainingBacklash { 0 };

        INDI::PropertyNumber GuideRateNP {2};
        enum
        {
            GUIDE_RATE_WE,
            GUIDE_RATE_NS
        };

        double m_Home[2] = {0, 0};

        Axis axisPrimary { "Primary Axis" };    // entails angle of mount for primary axis
        Axis axisSecondary { "Secondary Axis" }; // entails angle of mount for secondary axis ("mechanical DEC")

        int m_PierSide {-1};
        int m_MountType {-1};

        Alignment alignment;
        bool updateMountAndPierSide();

#ifdef USE_SIM_TAB
        // Simulator Tab properties
        // Scope type and alignment
        INDI::PropertySwitch mountTypeSP {3};
        enum
        {
            ALTAZ,
            EQ_FORK,
            EQ_GEM
        };

        INDI::PropertySwitch simPierSideSP {2};
        enum
        {
            PS_OFF,
            PS_ON
        };

        INDI::PropertyNumber mountModelNP {6};
        enum
        {
            MM_IH,
            MM_ID,
            MM_CH,
            MM_NP,
            MM_MA,
            MM_ME
        };

        INDI::PropertyNumber mountAxisNP {2};
        enum
        {
            PRIMARY,
            SECONDARY
        };

        INDI::PropertyNumber flipHourAngleNP {1};
        INDI::PropertyNumber decBacklashNP {1};

#endif

};

