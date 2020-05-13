/*******************************************************************************
  Copyright(c) 2016 Jasem Mutlaq. All rights reserved.

  INDI Sky Quality Meter Driver

  This program is free software; you can redistribute it and/or modify it
  under the terms of the GNU General Public License as published by the Free
  Software Foundation; either version 2 of the License, or (at your option)
  any later version.

  This program is distributed in the hope that it will be useful, but WITHOUT
  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
  more details.

  You should have received a copy of the GNU Library General Public License
  along with this library; see the file COPYING.LIB.  If not, write to
  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
  Boston, MA 02110-1301, USA.

  The full GNU General Public License is included in this distribution in the
  file called LICENSE.
*******************************************************************************/

#pragma once

#include "defaultdevice.h"

class SQM : public INDI::DefaultDevice
{
    public:
        SQM();
        virtual ~SQM() = default;

        virtual bool initProperties();
        virtual bool updateProperties();
        virtual bool ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n);

        /**
         * @struct SqmConnection
         * @brief Holds the connection mode of the device.
         */
        typedef enum
        {
            CONNECTION_NONE   = 1 << 0,
            CONNECTION_SERIAL = 1 << 1,
            CONNECTION_TCP    = 1 << 2
        } SqmConnection;

    protected:
        const char *getDefaultName();
        void TimerHit();

    private:
        bool getReadings();
        bool getDeviceInfo();

        // Readings
        INumberVectorProperty AverageReadingNP;
        INumber AverageReadingN[5];
        enum
        {
            SKY_BRIGHTNESS,
            SENSOR_FREQUENCY,
            SENSOR_COUNTS,
            SENSOR_PERIOD,
            SKY_TEMPERATURE
        };

        // Device Information
        INumberVectorProperty UnitInfoNP;
        INumber UnitInfoN[4];
        enum
        {
            UNIT_PROTOCOL,
            UNIT_MODEL,
            UNIT_FEATURE,
            UNIT_SERIAL
        };

        Connection::Serial *serialConnection { nullptr };
        Connection::TCP *tcpConnection { nullptr };

        int PortFD { -1 };
        uint8_t sqmConnection { CONNECTION_SERIAL | CONNECTION_TCP };

        ///////////////////////////////////////////////////////////////////////////////
        /// Communication Functions
        ///////////////////////////////////////////////////////////////////////////////
        bool sendCommand(const char * cmd, char * res = nullptr, int cmd_len = -1, int res_len = -1);
        void hexDump(char * buf, const char * data, int size);
        std::vector<std::string> split(const std::string &input, const std::string &regex);

        ///////////////////////////////////////////////////////////////////////////////////
        /// Properties
        ///////////////////////////////////////////////////////////////////////////////////

        /////////////////////////////////////////////////////////////////////////////
        /// Static Helper Values
        /////////////////////////////////////////////////////////////////////////////
        static constexpr const char * INFO_TAB = "Info";
        // 0xA is the stop char
        static const char DRIVER_STOP_CHAR { 0x0A };
        // Wait up to a maximum of 3 seconds for serial input
        static constexpr const uint8_t DRIVER_TIMEOUT {3};
        // Maximum buffer for sending/receving.
        static constexpr const uint8_t DRIVER_LEN {128};
};
