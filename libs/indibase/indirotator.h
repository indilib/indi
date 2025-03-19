/*******************************************************************************
  Copyright(c) 2017 Jasem Mutlaq. All rights reserved.

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

#include "defaultdevice.h"
#include "indirotatorinterface.h"
#include "indipropertynumber.h"
#include "indipropertyswitch.h"

namespace Connection
{
class Serial;
class TCP;
}
/**
 * \class Rotator
   \brief Class to provide general functionality of a rotator device.

   Rotators must be able to move to a specific angle. Other capabilities including abort, syncing, homing are optional.

   The angle is to be interpreted as the raw angle and not necessarily the position angle as this definition should be
   handled by clients after homing and syncing.

   This class is designed for pure rotator devices. To utilize Rotator Interface in another type of device, inherit from RotatorInterface.

\author Jasem Mutlaq
*/
namespace INDI
{

class Rotator : public DefaultDevice, public RotatorInterface
{
    public:
        Rotator();
        virtual ~Rotator();

        /** \struct RotatorConnection
                \brief Holds the connection mode of the Rotator.
            */
        enum
        {
            CONNECTION_NONE   = 1 << 0, /** Do not use any connection plugin */
            CONNECTION_SERIAL = 1 << 1, /** For regular serial and bluetooth connections */
            CONNECTION_TCP    = 1 << 2  /** For Wired and WiFI connections */
        } RotatorConnection;

        virtual bool initProperties() override;
        virtual void ISGetProperties(const char *dev) override;
        virtual bool updateProperties() override;
        virtual bool ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n) override;
        virtual bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n) override;

        /**
             * @brief setRotatorConnection Set Rotator connection mode. Child class should call this in the constructor before Rotator registers
             * any connection interfaces
             * @param value ORed combination of RotatorConnection values.
             */
        void setRotatorConnection(const uint8_t &value);

        /**
             * @return Get current Rotator connection mode
             */
        uint8_t getRotatorConnection() const;

    protected:
        /**
             * @brief saveConfigItems Saves the reverse direction property in the configuration file
             * @param fp pointer to configuration file
             * @return true if successful, false otherwise.
             */
        virtual bool saveConfigItems(FILE *fp) override;

        /** \brief perform handshake with device to check communication */
        virtual bool Handshake();

        INDI::PropertyNumber PresetNP {3};
        INDI::PropertySwitch PresetGotoSP {3};

        Connection::Serial *serialConnection = nullptr;
        Connection::TCP *tcpConnection       = nullptr;

        int PortFD = -1;

    private:
        bool callHandshake();
        uint8_t rotatorConnection = CONNECTION_SERIAL | CONNECTION_TCP;
};
}
