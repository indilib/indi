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
            CMD_FORCE_RESET = 0x80,
            CMD_FORCE_BOOT = 0x81,
            COH_NUMHEATERS = 0xB0,
            COH_ON_MANUAL = 0xB1,
            COH_OFF = 0xB4,
            COH_REPORT = 0xB5,
            COH_RESCAN = 0xBF,
            CMD_GET_VERSION = 0xFE
        };

        enum
        {
            DEVICE_PC = 0x20,
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
        uint8_t calculateCheckSum(const char *cmd, uint32_t len);
        template <typename T> std::string to_string(const T a_value, const int n = 2);

        ///////////////////////////////////////////////////////////////////////////////////
        /// Properties
        ///////////////////////////////////////////////////////////////////////////////////

        // Delta-T Informatin
        ITextVectorProperty InfoTP;
        IText InfoT[1];
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
            HEATER_ON,
            HEATER_OFF
        };

        // PWM Control
        std::vector<std::unique_ptr<INumberVectorProperty>> HeaterParamNP;
        std::vector<std::unique_ptr<INumber[]>> HeaterParamN;
        enum
        {
            PARAM_PERIOD,
            PARAM_DUTY
        };

        /////////////////////////////////////////////////////////////////////////////
        /// Private variables
        /////////////////////////////////////////////////////////////////////////////
        Connection::Serial *serialConnection { nullptr };
        int PortFD { -1 };

        /////////////////////////////////////////////////////////////////////////////
        /// Static Helper Values
        /////////////////////////////////////////////////////////////////////////////
        // Start of Message
        static const char DRIVER_SOM { 0x3B };
        static constexpr const uint8_t DRIVER_LEN {32};
        // Wait up to a maximum of 3 seconds for serial input
        static constexpr const uint8_t DRIVER_TIMEOUT {3};
};
