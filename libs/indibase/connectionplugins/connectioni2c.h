/*
    I2C Connection Plugin
    Copyright (C) 2025 Jasem Mutlaq (mutlaqja@ikarustech.com)

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

#include "connectioninterface.h"
#include "indipropertytext.h"
#include <string>
#include <stdint.h>

namespace Connection
{

/**
 * @brief The I2C class manages connection with I2C devices on Linux systems. I2C communication is commonly used
 * for sensors and other embedded devices. The default I2C bus is /dev/i2c-1 and default address is 0x28.
 * After I2C connection is established successfully, a handshake is performed to verify device communication.
 */
class I2C : public Interface
{
    public:
        enum I2CConnectionText
        {
            I2C_BUS = 0,
            I2C_ADDRESS,
        };


        I2C(INDI::DefaultDevice *dev, IPerm permission = IP_RW);
        virtual ~I2C();

        virtual bool Connect() override;
        virtual bool Disconnect() override;

        virtual void Activated() override;
        virtual void Deactivated() override;

        virtual std::string name() override
        {
            return "CONNECTION_I2C";
        }

        virtual std::string label() override
        {
            return "I2C";
        }

        /**
         * @return Currently active I2C bus path
         */
        virtual const char *busPath();

        /**
         * @return Currently active I2C device address
         */
        virtual uint8_t address();

        /**
         * @brief setDefaultBusPath Set default I2C bus path. Call this function in initProperties() of your driver
         * if you want to change default bus path.
         * @param path Name of desired default bus path
         */
        void setDefaultBusPath(const char *path);

        /**
         * @brief setDefaultAddress Set default I2C address. The default address is 0x28 unless
         * otherwise changed by this function. Call this function in initProperties() of your driver.
         * @param addr Desired new address
         */
        void setDefaultAddress(uint8_t addr);

        /**
         * @return Return I2C file descriptor. If connection is successful, I2CFD is a positive
         * integer otherwise it is set to -1
         */
        int getPortFD() const
        {
            return PortFD;
        }

        virtual bool ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n) override;
        virtual bool saveConfigItems(FILE *fp) override;

    protected:
        /**
         * \brief Connect to I2C device.
         * \param busPath I2C bus path to connect to.
         * \param address I2C device address
         * \return True if connection is successful, false otherwise
         * \warning Do not call this function directly, it is called by Connection::I2C Connect() function.
         */
        virtual bool Connect(const char *busPath, uint8_t address);

        virtual bool processHandshake();

        INDI::PropertyText I2CConnectionTP {2};

        int PortFD = -1;

        IPerm m_Permission = IP_RW;

        std::string m_ConfigBusPath;
        int m_ConfigAddress {-1};
};

}
