/*
    Rainbow Mount Driver
    Copyright (C) 2020 Jasem Mutlaq (mutlaqja@ikarustech.com)

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

#include "inditelescope.h"
#include "indiguiderinterface.h"

#define ALIGNMENT_TAB "Alignment"
#define GENERAL_INFO_TAB "General Info"


class Rainbow : public INDI::Telescope, public INDI::GuiderInterface
{
    public:
        Rainbow();

        const char *getDefaultName() override;
        virtual bool ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n) override;
        virtual bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n) override;

        typedef enum { Equatorial, Horizontal } GotoType;
        typedef enum { North, South, West, East } Direction;

    protected:
        virtual bool initProperties() override;
        virtual bool updateProperties() override;
        virtual bool Handshake() override;

        ///////////////////////////////////////////////////////////////////////////////
        /// Motion Functions
        ///////////////////////////////////////////////////////////////////////////////
        // Is slew over?
        virtual bool isSlewComplete();
        // Goto
        virtual bool Goto(double ra, double dec) override;
        // Read mount state
        virtual bool ReadScopeStatus() override;
        // Abort
        virtual bool Abort() override;
        // N/S manual motion
        virtual bool MoveNS(INDI_DIR_NS dir, TelescopeMotionCommand command) override;
        // W/E manual motion
        virtual bool MoveWE(INDI_DIR_WE dir, TelescopeMotionCommand command) override;
        // Slew Rate
        virtual bool SetSlewRate(int index) override;
        // Syncing
        virtual bool Sync(double ra, double dec) override;

        // RA/DE
        bool getRA();
        bool getDE();
        bool setRA(double ra);
        bool setDE(double de);
        // Slew to RA/DE
        bool slewToEquatorialCoords(double ra, double de);


        ///////////////////////////////////////////////////////////////////////////////
        /// Tracking Functions
        ///////////////////////////////////////////////////////////////////////////////
        // Toggle Tracking
        virtual bool SetTrackEnabled(bool enabled) override;
        virtual bool SetTrackMode(uint8_t mode) override;
        bool getTrackingState();

        ///////////////////////////////////////////////////////////////////////////////
        /// Query Functions
        ///////////////////////////////////////////////////////////////////////////////
        void getStartupStatus();
        bool getFirmwareVersion();
        TelescopePierSide getSideOfPier();

        ///////////////////////////////////////////////////////////////////////////////
        /// Location & Time
        ///////////////////////////////////////////////////////////////////////////////
        virtual bool updateLocation(double latitude, double longitude, double elevation) override;
        //virtual bool updateTime(ln_date *utc, double utc_offset) override;

        ///////////////////////////////////////////////////////////////////////////////
        /// Guiding
        ///////////////////////////////////////////////////////////////////////////////
        virtual IPState GuideNorth(uint32_t ms) override;
        virtual IPState GuideSouth(uint32_t ms) override;
        virtual IPState GuideEast(uint32_t ms) override;
        virtual IPState GuideWest(uint32_t ms) override;

        static void guideTimeoutHelperN(void *p);
        static void guideTimeoutHelperS(void *p);
        static void guideTimeoutHelperW(void *p);
        static void guideTimeoutHelperE(void *p);

        IPState guide(Direction direction, uint32_t ms);
        void guideTimeout(Direction direction);
        void addGuideTimer(Direction direction, uint32_t ms);
        bool setGuideRate(double rate);
        bool getGuideRate();

        ///////////////////////////////////////////////////////////////////////////////
        /// Parking & Homing
        ///////////////////////////////////////////////////////////////////////////////
        // Parking
        virtual bool Park() override;
        virtual bool UnPark() override;
        virtual bool SetCurrentPark() override;
        virtual bool SetDefaultPark() override;
        // Homing
        bool findHome();
        bool checkHoming();

        ///////////////////////////////////////////////////////////////////////////////
        /// Location & Time
        ///////////////////////////////////////////////////////////////////////////////
        bool sendScopeTime();
        bool sendScopeLocation();
        bool getUTFOffset(double *offset);
        bool getLocalDate(char *dateString);
        bool getLocalTime(char *timeString);

        ///////////////////////////////////////////////////////////////////////////////
        /// Communication Functions
        ///////////////////////////////////////////////////////////////////////////////
        bool sendCommand(const char * cmd, char * res = nullptr, int cmd_len = -1, int res_len = -1);
        void hexDump(char * buf, const char * data, int size);
        std::vector<std::string> split(const std::string &input, const std::string &regex);

    private:

        // Horizontal Coordinates functions.
        bool getAZ();
        bool getAL();
        bool setAZ(double azimuth);
        bool setAL(double altitude);
        bool slewToHorizontalCoords(double azimuth, double altitude);

        // Set and get slew speeds
        bool setSlewSpeedVal(int speedtype, double rate);
        bool getSlewSpeedVal(int speedtype);

        ///////////////////////////////////////////////////////////////////////////////////
        /// Properties
        ///////////////////////////////////////////////////////////////////////////////////
        ISwitchVectorProperty HomeSP;
        ISwitch HomeS[1];

        ISwitchVectorProperty SaveAlignBeforeSyncSP;
        ISwitch SaveAlignBeforeSyncS[2];
        enum { STAR_ALIGNMENT_DISABLED, STAR_ALIGNMENT_ENABLED};

        IText RSTVersionsT[2];
        ITextVectorProperty RSTVersionsTP;
        enum { FIRMWARE, SERIALNUMBER };

        ISwitchVectorProperty PullVoltTempSP;
        ISwitch PullVoltTempS[2];
        enum { PULL_VOLTTEMP_DISABLED, PULL_VOLTTEMP_ENABLED};

        INumber RSTVoltTempN[4];
        INumberVectorProperty RSTVoltTempNP;
        enum { VOLTAGE, BOARD_TEMPERATURE, RA_M_TEMPERATURE, DE_M_TEMPERATURE };

        INumber RSTMotorPowN[2];
        INumberVectorProperty RSTMotorPowNP;
        enum { RA_M_POWER, DE_M_POWER };

        INumberVectorProperty HorizontalCoordsNP;
        INumber HorizontalCoordsN[2];

        INumber GuideRateN[1];
        INumberVectorProperty GuideRateNP;

        INumberVectorProperty SlewSpeedsNP;
        INumber SlewSpeedsN[3];
        enum { SLEW_SPEED_MAX, SLEW_SPEED_FIND, SLEW_SPEED_CENTERING };

        const std::string getSlewErrorString(uint8_t code);
        uint8_t m_SlewErrorCode {0};

        GotoType m_GotoType { Equatorial };
        double m_CurrentAZ {0}, m_CurrentAL {0};
        double m_CurrentRA {0}, m_CurrentDE {0};
        std::string m_Version;
        int m_GuideNSTID {0};
        int m_GuideWETID {0};

        /////////////////////////////////////////////////////////////////////////////
        /// Static Helper Values
        /////////////////////////////////////////////////////////////////////////////
        static constexpr const char * INFO_TAB = "Info";
        // '#' is the stop char
        static const char DRIVER_STOP_CHAR { 0x23 };
        // Wait up to a maximum of 3 seconds for serial input
        static constexpr const uint8_t DRIVER_TIMEOUT {3};
        // Maximum buffer for sending/receiving.
        static constexpr const uint8_t DRIVER_LEN {64};
};
