/*
    Celestron Aux Command

    Copyright (C) 2020 Paweł T. Jochym
    Copyright (C) 2020 Fabrizio Pollastri
    Copyright (C) 2021 Jasem Mutlaq

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

#include <vector>
#include <stddef.h>
#include <stdint.h>

typedef std::vector<uint8_t> AUXBuffer;

enum AUXCommands
{
    // Port Controller Aux Commands
    PORTCTRL_GET_INPUT_POWER        = 0x00,
    PORTCTRL_SET_POWER_ENABLED      = 0x01, // (should not be used unless connected via USB port)
    PORTCTRL_GET_POWER_ENABLED      = 0x02,
    PORTCTRL_SET_EXT_CURRENT_LIMIT  = 0x03,
    PORTCTRL_GET_EXT_CURRENT_LIMIT  = 0x04,

    PORTCTRL_GET_NUMBER_OF_PORTS    = 0x10,
    PORTCTRL_GET_PORT_INFO          = 0x11,
    PORTCTRL_GET_DH_PORT_INFO       = 0x12,
    PORTCTRL_RESET_FUSE             = 0x13,
    PORTCTRL_SET_PORT_ENABLED       = 0x14,
    PORTCTRL_SET_PORT_VOLTAGE       = 0x15,
    PORTCTRL_DH_ENABLE_AUTO         = 0x16,
    PORTCTRL_DH_ENABLE_MANUAL       = 0x17,
    PORTCTRL_GET_ENVIRONMENT        = 0x18,
    PORTCTRL_ENABLE_SELF_HEATER     = 0x19,
    PORTCTRL_SELF_HEATER_STATUS     = 0x1A,
    PORTCTRL_GET_HEATER_LEVEL       = 0x1B,
    PORTCTRL_FACTORY_RESET          = 0xAA,
    PORTCTRL_GET_VERSION            = 0xFE,

    PORTCTRL_SET_LED_BRIGHTNESS     = 0x20,
    PORTCTRL_GET_LED_BRIGHTNESS     = 0x21
};

enum AUXTargets
{
    HC    = 0x04,
    APP   = 0x20,
    DEW_POWER_CTRL = 0xc0 // New target for Celestron Dew Heater & Power Controller
};

#define CAUX_DEFAULT_IP   "1.2.3.4"
#define CAUX_DEFAULT_PORT 2000

void logBytes(unsigned char *buf, int n, const char *deviceName, uint32_t debugLevel);

class AUXCommand
{
    public:
        AUXCommand();
        AUXCommand(const AUXBuffer &buf);
        AUXCommand(AUXCommands command, AUXTargets source, AUXTargets destination, const AUXBuffer &data);
        AUXCommand(AUXCommands command, AUXTargets source, AUXTargets destination);

        ///////////////////////////////////////////////////////////////////////////////
        /// Buffer Management
        ///////////////////////////////////////////////////////////////////////////////
        void fillBuf(AUXBuffer &buf);
        void parseBuf(AUXBuffer buf);
        void parseBuf(AUXBuffer buf, bool do_checksum);

        ///////////////////////////////////////////////////////////////////////////////
        /// Getters
        ///////////////////////////////////////////////////////////////////////////////
        const AUXTargets &source() const
        {
            return m_Source;
        }
        const AUXTargets &destination() const
        {
            return m_Destination;
        }
        const AUXBuffer &data() const
        {
            return m_Data;
        }
        AUXCommands command() const
        {
            return m_Command;
        }
        size_t dataSize() const
        {
            return m_Data.size();
        }
        const char * commandName() const
        {
            return commandName(m_Command);
        }

        ///////////////////////////////////////////////////////////////////////////////
        /// Set and Get data
        ///////////////////////////////////////////////////////////////////////////////
        /**
         * @brief getData Parses data packet and convert it to a 32bit unsigned integer
         * @param bytes How many bytes to interpret the data.
         * @return
         */
        uint32_t getData();
        void setData(uint32_t value, uint8_t bytes = 3);
        void setDataBuffer(const AUXBuffer &data) { m_Data = data; len = 3 + m_Data.size(); }
        const AUXBuffer &getDataBuffer() const { return m_Data; }
        bool isValid() const { return valid; }


        ///////////////////////////////////////////////////////////////////////////////
        /// Check sum
        ///////////////////////////////////////////////////////////////////////////////
        uint8_t checksum(AUXBuffer buf);

        ///////////////////////////////////////////////////////////////////////////////
        /// Logging
        ///////////////////////////////////////////////////////////////////////////////
        const char * commandName(AUXCommands command) const;
        const char * moduleName(AUXTargets n);
        int responseDataSize();
        void logResponse();
        void logCommand();
        static void setDebugInfo(const char *deviceName, uint8_t debugLevel);

        static uint8_t DEBUG_LEVEL;
        static char DEVICE_NAME[64];

    private:
        uint8_t len {0};
        bool valid {false};

        AUXCommands m_Command;
        AUXTargets m_Source, m_Destination;
        AUXBuffer m_Data;
};
