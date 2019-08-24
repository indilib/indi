#ifndef LX200TELESCOPE_H
#define LX200TELESCOPE_H

#pragma once

/*
 * Standard LX200 implementation.

   Copyright (C) 2003 - 2018 Jasem Mutlaq (mutlaqja@ikarustech.com)

   This library is free software;
   you can redistribute it and / or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation;
   either
   version 2.1 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY;
   without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this library;
   if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110 - 1301  USA
 */

#include "indiguiderinterface.h"
#include "indifocuserinterface.h"
#include "inditelescope.h"

class LX200Telescope : public INDI::Telescope, public INDI::GuiderInterface, public INDI::FocuserInterface
{
    public:
        LX200Telescope();
        /**
         * \struct LX200Capability
         * \brief Holds properties of LX200 Generic that might be used by child classes
         */
        enum
        {
            LX200_HAS_FOCUS                 = 1 << 0, /** Define focus properties */
            LX200_HAS_TRACKING_FREQ         = 1 << 1, /** Define Tracking Frequency */
            LX200_HAS_ALIGNMENT_TYPE        = 1 << 2, /** Define Alignment Type */
            LX200_HAS_SITES                 = 1 << 3, /** Define Sites */
            LX200_HAS_PULSE_GUIDING         = 1 << 4, /** Define Pulse Guiding */
            LX200_HAS_PRECISE_TRACKING_FREQ = 1 << 5, /** Use more precise tracking frequency, if supported by hardware. */
        } LX200Capability;

        uint32_t getLX200Capability() const
        {
            return genericCapability;
        }
        void setLX200Capability(uint32_t cap)
        {
            genericCapability = cap;
        }

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

        void updateFocusTimer();

    protected:
        // Slew Rate
        virtual bool SetSlewRate(int index) override;
        // Track Mode (Sidereal, Solar..etc)
        virtual bool SetTrackMode(uint8_t mode) override;

        // NSWE Motion Commands
        virtual bool MoveNS(INDI_DIR_NS dir, TelescopeMotionCommand command) override;
        virtual bool MoveWE(INDI_DIR_WE dir, TelescopeMotionCommand command) override;

        // Abort ALL motion
        virtual bool Abort() override;

        // Time and Location
        virtual bool updateTime(ln_date *utc, double utc_offset) override;
        virtual bool updateLocation(double latitude, double longitude, double elevation) override;

        // Guide Commands
        virtual IPState GuideNorth(uint32_t ms) override;
        virtual IPState GuideSouth(uint32_t ms) override;
        virtual IPState GuideEast(uint32_t ms) override;
        virtual IPState GuideWest(uint32_t ms) override;

        // Guide Pulse Commands
        virtual int SendPulseCmd(int8_t direction, uint32_t duration_msec);

        // Goto
        virtual bool Goto(double ra, double dec) override;

        // Is slew over?
        virtual bool isSlewComplete();

        // Park Mount
        virtual bool Park() override;

        // Sync coordinates
        virtual bool Sync(double ra, double dec) override;

        // Check if mount is responsive
        virtual bool checkConnection();

        // Save properties in config file
        virtual bool saveConfigItems(FILE *fp) override;

        // Action to perform when Debug is turned on or off
        virtual void debugTriggered(bool enable) override;

        // Initial function to get data after connection is successful
        virtual void getBasicData();

        // Get local calender date (NOT UTC) from mount. Expected format is YYYY-MM-DD
        virtual bool getLocalDate(char *dateString);
        virtual bool setLocalDate(uint8_t days, uint8_t months, uint16_t years);

        // Get Local time in 24 hour format from mount. Expected format is HH:MM:SS
        virtual bool getLocalTime(char *timeString);
        virtual bool setLocalTime24(uint8_t hour, uint8_t minute, uint8_t second);

        // Return UTC Offset from mount in hours.
        virtual bool setUTCOffset(double offset);
        virtual bool getUTFOffset(double * offset);

        // Send slew error message to client
        virtual void slewError(int slewCode);

        // Get mount alignment type (AltAz..etc)
        void getAlignment();

        // Send Mount time and location settings to client
        bool sendScopeTime();
        bool sendScopeLocation();

        // Update slew rate if different than current
        bool updateSlewRate(int index);

        // Simulate Mount in simulation mode
        void mountSim();

        // Focus functions
        static void updateFocusHelper(void *p);
        virtual bool AbortFocuser () override;
        virtual bool ReverseFocuser(bool enabled) override;
        virtual IPState MoveFocuser(FocusDirection dir, int speed, uint16_t duration) override;
        virtual bool SetFocuserSpeed(int speed) override;

        static void guideTimeoutHelperNS(void *p);
        static void guideTimeoutHelperWE(void *p);
        void guideTimeoutNS();
        void guideTimeoutWE();

        int GuideNSTID { -1 };
        int GuideWETID { -1 };
        int8_t guide_direction_ns { -1 };
        int8_t guide_direction_we { -1 };

        int timeFormat = -1;
        int currentSiteNum {0};
        int trackingMode {0};

        bool sendTimeOnStartup = true, sendLocationOnStartup = true;
        uint8_t DBG_SCOPE {0};

        double JD {0};
        double targetRA {0}, targetDEC {0};
        double currentRA {0}, currentDEC {0};
        int MaxReticleFlashRate {0};

        /* Telescope Alignment Mode */
        ISwitchVectorProperty AlignmentSP;
        ISwitch AlignmentS[3];

        /* Tracking Frequency */
        INumberVectorProperty TrackingFreqNP;
        INumber TrackFreqN[1];

        /* Use pulse-guide commands */
        ISwitchVectorProperty UsePulseCmdSP;
        ISwitch UsePulseCmdS[2];
        bool usePulseCommand { false };

        /* Site Management */
        ISwitchVectorProperty SiteSP;
        ISwitch SiteS[4];

        /* Site Name */
        ITextVectorProperty SiteNameTP;
        IText SiteNameT[1] {};

        /* Focus Mode */
        ISwitchVectorProperty FocusModeSP;
        ISwitch FocusModeS[3];

        uint32_t genericCapability {0};
};

#endif // LX200TELESCOPE_H
