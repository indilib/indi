/*******************************************************************************
  Copyright(c) 2018 Jasem Mutlaq. All rights reserved.

  Pegasus Ultimate Power Box

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
#include "indifocuserinterface.h"
#include "indiweatherinterface.h"
#include <stdint.h>

namespace Connection
{
class Serial;
}

class PegasusUPB : public INDI::DefaultDevice, public INDI::FocuserInterface, public INDI::WeatherInterface
{
  public:
    PegasusUPB();

    virtual bool initProperties() override;
    virtual bool updateProperties() override;

    virtual bool ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n) override;
    virtual bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n) override;

  protected:
    const char *getDefaultName() override;
    virtual bool Disconnect() override;

  private:    
    bool Handshake();

    /**
     * @brief sendCommand Send command to unit.
     * @param cmd Command
     * @param res if nullptr, respones is ignored, otherwise read response and store it in the buffer.
     * @return
     */
    bool sendCommand(const char *cmd, char *res);

    int PortFD { -1 };

    Connection::Serial *serialConnection { nullptr };

    static constexpr const uint8_t PEGASUS_TIMEOUT {3};
    static constexpr const char *DEW_GROUP {"Dew"};
    static constexpr const char *ENVIRONMENT_GROUP {"Environment"};
    static constexpr const char *STATUS_GROUP {"Status"};
    static constexpr const char *POWER_GROUP {"Power"};
    static constexpr const char *FOCUSER_GROUP {"Focuser"};
};
