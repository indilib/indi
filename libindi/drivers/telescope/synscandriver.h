/*******************************************************************************
  Copyright(c) 2010 Gerry Rozema. All rights reserved.
  Copyright(c) 2018 Jasem Mutlaq. All rights reserved.

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

class SynscanDriver : public INDI::Telescope
{
    public:
        SynscanDriver();

        virtual bool initProperties() override;
        virtual bool updateProperties() override;
        virtual const char * getDefaultName() override;

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
        virtual bool Abort() override;
        virtual bool SetSlewRate(int index) override;
        virtual bool MoveNS(INDI_DIR_NS dir, TelescopeMotionCommand command) override;
        virtual bool MoveWE(INDI_DIR_WE dir, TelescopeMotionCommand command) override;

    protected:
        // Check if command is responding
        virtual bool echo();

        double SlewTargetAlt { -1 };
        double SlewTargetAz { -1 };
        double CurrentRA { 0 };
        double CurrentDEC { 0 };
        double TargetRA {0};
        double TargetDEC {0};
        int m_MountModel { 0 };
        int SlewRate { 5 };
        bool NewFirmware { false };
        double FirmwareVersion { 0 };



    private:
        bool passThruCommand(int cmd, int target, int msgsize, int data, int numReturn);
        bool sendCommand(const char * cmd, char * res = nullptr);
        void sendMountStatus();
        bool startTrackMode();
        bool sendTime();
        bool sendLocation();
        bool readFirmware();
        bool readModel();

        void mountSim();

        // Utility
        ln_hrz_posn getAltAzPosition(double ra, double dec);
        int hexStrToInteger(const std::string &str);

        char LastParkRead[64];
        int NumPark { 0 };
        int StopCount { 0 };
        int CustomNSSlewRate { -1 };
        int CustomWESlewRate { -1 };
        int RecoverTrials { 0 };

        IText BasicMountInfoT[6] = {};
        ITextVectorProperty BasicMountInfoTP;
        enum MountInfo
        {
            MI_FW_VERSION,
            MI_MOUNT_MODEL,
            MI_ALIGN_STATUS,
            MI_GOTO_STATUS,
            MI_POINT_STATUS,
            MI_TRACK_MODE
        };
        std::vector<std::string> m_MountInfo;

        // Supported Slew Rates
        static constexpr uint16_t SLEW_RATE[] = {1, 2, 8, 16, 64, 128, 256, 512, 1024};
        // Maximum buffer for reading from Synscan
        static const uint8_t SYN_RES = 64;
        // Timeout
        static const uint8_t SYN_TIMEOUT = 3;
        // Delimeter
        static const char SYN_DEL = {'#'};
        // Mount Information Tab
        static constexpr const char * MOUNT_TAB = "Mount Information";
};
