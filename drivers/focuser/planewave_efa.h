/*
    PlaneWave EFA Protocol

    Hendrick Focuser

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

#include "indifocuser.h"

#include <map>

class EFA : public INDI::Focuser
{
    public:
        EFA();

        virtual bool Handshake() override;
        const char *getDefaultName() override;
        virtual bool initProperties() override;
        virtual bool updateProperties() override;

        virtual bool ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n) override;
        virtual bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n) override;

        enum
        {
            MTR_GET_POS = 0x01,
            MTR_GOTO_POS2 = 0x17,
            MTR_OFFSET_CNT = 0x04,
            MTR_GOTO_OVER = 0x13,
            MTR_SLEWLIMITMAX = 0x1B,
            MTR_SLEWLIMITGETMAX = 0x1D,
            MTR_PMSLEW_RATE = 0x24,
            MTR_NMSLEW_RATE = 0x25,
            TEMP_GET = 0x26,
            FANS_SET = 0x27,
            FANS_GET = 0x28,
            MTR_GET_CALIBRATION_STATE = 0x30,
            MTR_SET_CALIBRATION_STATE = 0x31,
            MTR_GET_STOP_DETECT = 0xEE,
            MTR_STOP_DETECT = 0xEF,
            MTR_GET_APPROACH_DIRECTION = 0xFC,
            MTR_APPROACH_DIRECTION = 0xFD,
            GET_VERSION = 0xFE
        };

        enum
        {
            DEVICE_PC = 0x20,
            DEVICE_HC = 0x0D,
            DEVICE_FOC = 0x12,
            DEVICE_FAN = 0x13,
            DEVICE_TEMP = 0x12
        };

    protected:
        virtual IPState MoveAbsFocuser(uint32_t targetTicks) override;
        virtual IPState MoveRelFocuser(FocusDirection dir, unsigned int ticks) override;
        virtual bool SyncFocuser(uint32_t ticks) override;
        virtual bool ReverseFocuser(bool enabled) override;
        virtual bool SetFocuserMaxPosition(uint32_t ticks) override;
        virtual bool AbortFocuser() override;
        virtual void TimerHit() override;
        virtual bool Disconnect() override;

        virtual bool saveConfigItems(FILE *fp) override;

    private:
        ///////////////////////////////////////////////////////////////////////////////////
        /// Query functions
        ///////////////////////////////////////////////////////////////////////////////////
        bool isGOTOComplete();
        bool readPosition();
        bool readTemperature();
        bool readFanState();
        bool readCalibrationState();
        bool readMaxSlewLimit();

        ///////////////////////////////////////////////////////////////////////////////////
        /// Set functions
        ///////////////////////////////////////////////////////////////////////////////////
        bool setFanEnabled(bool enabled);
        bool setCalibrationEnabled(bool enabled);

        ///////////////////////////////////////////////////////////////////////////////
        /// Communication Functions
        ///////////////////////////////////////////////////////////////////////////////
        //bool readResponse(uint8_t * res, uint32_t res_len, int *nbytes_read);
        int readByte(int fd, uint8_t *buf, int timeout, int *nbytes_read);
        int readBytes(int fd, uint8_t *buf, int nbytes, int timeout, int *nbytes_read);
        int writeBytes(int fd, const uint8_t *buf, int nbytes, int *nbytes_written);
        int readPacket(int fd, uint8_t *buf, int nbytes, int timeout, int *nbytes_read);
        bool sendCommand(const uint8_t * cmd, uint8_t * res, uint32_t cmd_len, uint32_t res_len);
        char * efaDump(char * buf, int buflen, const uint8_t * data, uint32_t size);
        std::vector<std::string> split(const std::string &input, const std::string &regex);

        ///////////////////////////////////////////////////////////////////////////////////
        /// Misc
        ///////////////////////////////////////////////////////////////////////////////////
        void getStartupValues();
        double calculateTemperature(uint8_t byte2, uint8_t byte3);
        bool validateLengths(const uint8_t *cmd, uint32_t len);
        uint8_t calculateCheckSum(const uint8_t *cmd, uint32_t len);
        template <typename T> std::string to_string(const T a_value, const int n = 2);

        ///////////////////////////////////////////////////////////////////////////////////
        /// Properties
        ///////////////////////////////////////////////////////////////////////////////////

        // Focuser Information
        INDI::PropertyText InfoTP {1};
        // IText InfoT[1] {};
        enum
        {
            INFO_VERSION
        };


        // FAN State
        INDI::PropertySwitch FanStateSP {2};
        enum
        {
            FAN_ON,
            FAN_OFF
        };

        // Fan Control Mode
        INDI::PropertySwitch FanControlSP {3};
        enum
        {
            FAN_MANUAL,
            FAN_AUTOMATIC_ABSOLUTE,
            FAN_AUTOMATIC_RELATIVE,
        };

        // Fan Control Parameters
        INDI::PropertyNumber FanControlNP {3};
        // INumber FanControlN[3];
        enum
        {
            FAN_MAX_ABSOLUTE,
            FAN_MAX_RELATIVE,
            FAN_DEADZONE,
        };

        // Fan Off on Disconnect
        INDI::PropertySwitch FanDisconnectSP {1};
        enum
        {
            FAN_OFF_ON_DISCONNECT
        };

        // Calibration State
        INDI::PropertySwitch CalibrationStateSP {2};
        enum
        {
            CALIBRATION_ON,
            CALIBRATION_OFF
        };

        // Read Only Temperature Reporting
        INDI::PropertyNumber TemperatureNP {2};
        enum
        {
            TEMPERATURE_PRIMARY,
            TEMPERATURE_AMBIENT
        };

        /////////////////////////////////////////////////////////////////////////////
        /// Private variables
        /////////////////////////////////////////////////////////////////////////////
        double m_LastTemperature[2];
        double m_LastPosition {0};

        bool IN_TIMER = false;

        /////////////////////////////////////////////////////////////////////////////
        /// Static Helper Values
        /////////////////////////////////////////////////////////////////////////////
        // Start of Message
        static const uint8_t DRIVER_SOM { 0x3B };
        // Temperature Reporting threshold
        static constexpr double TEMPERATURE_THRESHOLD { 0.05 };

        static constexpr const uint8_t DRIVER_LEN {9};
        // Wait up to a maximum of 3 seconds for serial input
        static constexpr const uint8_t DRIVER_TIMEOUT {3};
        // Fan Control
        static constexpr const char *FAN_TAB = "Fan";
};
