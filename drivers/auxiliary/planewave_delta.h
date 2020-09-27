/*
    PlaneWave Delta Protocol

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
#include "pid/pid.h"

#include <memory>
#include <map>

class DeltaT : public INDI::DefaultDevice
{
    public:
        DeltaT();

        virtual bool Handshake();
        const char *getDefaultName();
        virtual bool initProperties();
        virtual bool updateProperties();

        virtual bool ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n);
        virtual bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n);

        enum
        {
            TEMP_GET = 0x26,
            CMD_FORCE_RESET = 0x80,
            CMD_FORCE_BOOT = 0x81,
            COH_NUMHEATERS = 0xB0,
            COH_ON_MANUAL = 0xB1,
            COH_OFF = 0xB4,
            COH_REPORT = 0xB5,
            COH_RESCAN = 0xBF,
            CMD_GET_VERSION = 0xFE,
        };

        enum
        {
            DEVICE_PC = 0x20,
            DEVICE_HC = 0x0D,
            DEVICE_FOC = 0x12,
            DEVICE_FAN = 0x13,
            DEVICE_TEMP = 0x12,
            DEVICE_DELTA = 0x32
        };

        typedef struct
        {
            uint8_t  StateUB;
            uint8_t  ModeUB;
            uint16_t SetPointUW;
            uint8_t  TempHtrIdUB;
            uint16_t TempHtrUW;
            uint16_t TempAmbUW;
            uint16_t PeriodUW;
            uint8_t  DutyCycleUB;
        } HeaterReport;


    protected:
        virtual void TimerHit();
        virtual bool saveConfigItems(FILE *fp);

    private:
        ///////////////////////////////////////////////////////////////////////////////////
        /// Query functions
        ///////////////////////////////////////////////////////////////////////////////////
        bool readReport(uint8_t index);
        bool readTemperature();
        bool initializeHeaters();

        ///////////////////////////////////////////////////////////////////////////////////
        /// Set functions
        ///////////////////////////////////////////////////////////////////////////////////
        bool setHeaterEnabled(uint8_t index, bool enabled);
        bool setHeaterParam(uint8_t index, double period, double duty);
        bool forceBoot();
        bool forceReset();

        ///////////////////////////////////////////////////////////////////////////////
        /// Communication Functions
        ///////////////////////////////////////////////////////////////////////////////
        bool sendCommand(const char * cmd, char * res, uint32_t cmd_len, uint32_t res_len);
        void hexDump(char * buf, const char * data, uint32_t size);
        std::vector<std::string> split(const std::string &input, const std::string &regex);

        ///////////////////////////////////////////////////////////////////////////////////
        /// Misc
        ///////////////////////////////////////////////////////////////////////////////////
        double calculateTemperature(uint8_t lsb, uint8_t msb);
        uint8_t calculateCheckSum(const char *cmd, uint32_t len);
        const char *getHeaterName(int index);
        template <typename T> std::string to_string(const T a_value, const int n = 2);

        ///////////////////////////////////////////////////////////////////////////////////
        /// Properties
        ///////////////////////////////////////////////////////////////////////////////////

        // Delta-T Informatin
        ITextVectorProperty InfoTP;
        IText InfoT[1] {};
        enum
        {
            INFO_VERSION
        };


        // Force Control
        ISwitchVectorProperty ForceSP;
        ISwitch ForceS[2];
        enum
        {
            FORCE_RESET,
            FORCE_BOOT
        };

        // Heater Control
        std::vector<std::unique_ptr<ISwitchVectorProperty>> HeaterControlSP;
        std::vector<std::unique_ptr<ISwitch[]>> HeaterControlS;
        enum
        {
            HEATER_OFF,
            HEATER_ON,
            HEATER_CONTROL,
            HEATER_THRESHOLD
        };

        // Control Params
        std::vector<std::unique_ptr<INumberVectorProperty>> HeaterParamNP;
        std::vector<std::unique_ptr<INumber[]>> HeaterParamN;
        enum
        {
            PARAM_PERIOD,
            PARAM_DUTY,
            PARAM_CONTROL,
            PARAM_THRESHOLD,
        };

        // Monitor
        std::vector<std::unique_ptr<INumberVectorProperty>> HeaterMonitorNP;
        std::vector<std::unique_ptr<INumber[]>> HeaterMonitorN;
        enum
        {
            MONITOR_PERIOD,
            MONITOR_DUTY
        };

        // Read Only Temperature Reporting
        INumberVectorProperty TemperatureNP;
        INumber TemperatureN[3];
        enum
        {
            // Primary is 0 , not used in this driver.
            // 1
            TEMPERATURE_AMBIENT,
            // 2
            TEMPERATURE_SECONDARY,
            // 3
            TEMPERATURE_BACKPLATE,
        };

        /////////////////////////////////////////////////////////////////////////////
        /// Private variables
        /////////////////////////////////////////////////////////////////////////////
        Connection::Serial *serialConnection { nullptr };
        double m_LastTemperature[3];
        int PortFD { -1 };
        std::vector<std::unique_ptr<PID>> m_Controllers;

        /////////////////////////////////////////////////////////////////////////////
        /// Static Helper Values
        /////////////////////////////////////////////////////////////////////////////
        // Start of Message
        static const char DRIVER_SOM { 0x3B };
        // Temperature Reporting threshold
        static constexpr double TEMPERATURE_REPORT_THRESHOLD { 0.05 };
        // Temperature Control threshold
        static constexpr double TEMPERATURE_CONTROL_THRESHOLD { 0.1 };
        static constexpr const uint8_t DRIVER_LEN {32};
        // Wait up to a maximum of 3 seconds for serial input
        static constexpr const uint8_t DRIVER_TIMEOUT {3};

        // Primary Backplate heater
        static constexpr const char *PRIMARY_TAB = "Primary Backplate Heater";
        // Secondary Mirror heater
        static constexpr const char *SECONDARY_TAB = "Secondary Mirror Heater";
};
