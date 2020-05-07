/*******************************************************************************
  Copyright(c) 2019 Jasem Mutlaq. All rights reserved.

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Library General Public
 License version 2 as published by the Free Software Foundation.

 This library is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 Library General Public License for more details.

 You should have received a copy of the GNU Library General Public License
 along with this library; see the file COPYING.LIB.  If not, write to
 the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 Boston, MA 02110-1301, USA.
*******************************************************************************/

#pragma once

#include "inditelescope.h"
#include "indiguiderinterface.h"

class SynscanDriver : public INDI::Telescope, public INDI::GuiderInterface
{
    public:
        SynscanDriver();

        typedef enum { SYN_N, SYN_S, SYN_E, SYN_W } SynscanDirection;

        virtual bool initProperties() override;
        virtual bool updateProperties() override;
        virtual const char * getDefaultName() override;

        virtual bool ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n) override;
        virtual bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n) override;

        static void guideTimeoutHelperNS(void *context);
        static void guideTimeoutHelperWE(void *context);

    protected:
        virtual bool Handshake() override;

        // Get RA/DE and status
        virtual bool ReadScopeStatus() override;

        // Time & Location
        virtual bool updateLocation(double latitude, double longitude, double elevation) override;
        virtual bool updateTime(ln_date * utc, double utc_offset) override;

        // Parking
        virtual bool Park() override;
        virtual bool UnPark() override;
        virtual bool SetCurrentPark() override;
        virtual bool SetDefaultPark() override;

        // Motion
        virtual bool Sync(double ra, double dec) override;
        virtual bool Goto(double, double) override;
        virtual bool GotoAzAlt(double az, double alt);
        virtual bool Abort() override;
        virtual bool SetSlewRate(int index) override;
        virtual bool MoveNS(INDI_DIR_NS dir, TelescopeMotionCommand command) override;
        virtual bool MoveWE(INDI_DIR_WE dir, TelescopeMotionCommand command) override;

        // Tracking
        virtual bool SetTrackEnabled(bool enabled) override;
        virtual bool SetTrackMode(uint8_t mode) override;

        // Guiding
        virtual IPState GuideNorth(uint32_t ms) override;
        virtual IPState GuideSouth(uint32_t ms) override;
        virtual IPState GuideEast(uint32_t ms) override;
        virtual IPState GuideWest(uint32_t ms) override;

    private:
        /**
         * @brief sendCommand Send a string command to mount.
         * @param cmd Command to be sent
         * @param cmd_len length of command in bytes. If not specified it is treated as null-terminating string
         * @param res If not nullptr, the function will read until it detects the default delimeter ('#') up to SYN_RES length.
         *        if nullptr, no read back is done and the function returns true.
         * @param res_len length of buffer to read. If not specified, the buffer will be read until the delimeter is met.
         *        if specified, only res_len bytes are ready.
         * @return True if successful, false otherwise.
         */
        bool sendCommand(const char * cmd, char * res = nullptr, int cmd_len=-1, int res_len=-1);
        // Check if mount is responding
        virtual bool echo();

        // Initial Params
        void setupParams();
        // Send
        void sendStatus();
        bool sendTime();
        bool sendLocation();
        // Read
        bool readFirmware();
        bool readModel();
        bool readTracking();
        // Goto mode
        bool SetAltAzMode(bool);

        // Slew
        bool slewFixedRate(SynscanDirection direction, uint8_t rate);
        bool isSlewComplete();

        // Guiding
        void guideTimeoutCallbackNS();
        void guideTimeoutCallbackWE();

        /**
         * @brief slewVariableRate Slew using a custom rate
         * @param direction N/S/E/W
         * @param rate rate in arcsecs/s
         * @return
         */
        bool slewVariableRate(SynscanDirection direction, double rate);

        // Simulation
        void mountSim();

        double CurrentRA { 0 }, CurrentDE { 0 };
        double TargetRA {0}, TargetDE {0};
        uint8_t m_MountModel { 0 };
        int m_TargetSlewRate { 5 };
        uint8_t m_TrackingFlag { 0 };
        double m_FirmwareVersion { 0 };
        double m_CustomGuideRA { 0 }, m_CustomGuideDE { 0 };
        int m_GuideNSTID { 0 }, m_GuideWETID { 0 };

        // Utility
        ln_hrz_posn getAltAzPosition(double ra, double dec);
        int hexStrToInteger(const std::string &str);
        void hexDump(char *buf, const char *data, int size);

        // Is mount type Alt-Az?
        bool m_isAltAz { false };
        // Use Alt-Az Goto?
        bool goto_AltAz { false };

        //////////////////////////////////////////////////////////////////
        /// Properties
        //////////////////////////////////////////////////////////////////

        // Mount Status
        IText StatusT[5] = {};
        ITextVectorProperty StatusTP;

        // Custom Slew Rate
        INumber CustomSlewRateN[2];
        INumberVectorProperty CustomSlewRateNP;

        // Guide Rate
        INumber GuideRateN[2];
        INumberVectorProperty GuideRateNP;

        // Horizontal Coords
        INumber HorizontalCoordsN[2];
        INumberVectorProperty HorizontalCoordsNP;

        // Goto mode
        ISwitch GotoModeS[2];
        ISwitchVectorProperty GotoModeSP;

        // Mount Info
        enum MountInfo
        {
            MI_FW_VERSION,
            MI_MOUNT_MODEL,
            MI_GOTO_STATUS,
            MI_POINT_STATUS,
            MI_TRACK_MODE
        };
        std::vector<std::string> m_MountInfo;

        //////////////////////////////////////////////////////////////////
        /// Static Constants
        //////////////////////////////////////////////////////////////////

        // Simulated Slew Rates
        static constexpr uint16_t SIM_SLEW_RATE[] = {1, 8, 16, 32, 64, 128, 400, 600, 800, 900};
        // Maximum buffer for reading from Synscan
        static const uint8_t SYN_RES = 64;
        // Timeout
        static const uint8_t SYN_TIMEOUT = 3;
        // Delimeter
        static const char SYN_DEL = {'#'};
        // Mount Information Tab
        static constexpr const char * MOUNT_TAB = "Mount Information";
};
