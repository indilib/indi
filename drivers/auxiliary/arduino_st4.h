/*******************************************************************************
  Copyright(c) 2018 Jasem Mutlaq. All rights reserved.

  Arduino ST4 Driver.

  For this project: https://github.com/kevinferrare/arduino-st4

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
#include "indiguiderinterface.h"
#include <stdint.h>

namespace Connection
{
class Serial;
}

class ArduinoST4 : public INDI::DefaultDevice, public INDI::GuiderInterface
{
  public:
    ArduinoST4();

    typedef enum { ARD_N, ARD_S, ARD_W, ARD_E } ARDUINO_DIRECTION;

    virtual bool initProperties() override;
    virtual bool updateProperties() override;

    virtual bool ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n) override ;

    void guideTimeout(ARDUINO_DIRECTION direction);

  protected:
    const char *getDefaultName() override;

    virtual bool Disconnect() override;
    virtual IPState GuideNorth(uint32_t ms) override;
    virtual IPState GuideSouth(uint32_t ms) override;
    virtual IPState GuideEast(uint32_t ms) override;
    virtual IPState GuideWest(uint32_t ms) override;

    // Helper functions
    static void guideTimeoutHelperN(void *p);
    static void guideTimeoutHelperS(void *p);
    static void guideTimeoutHelperW(void *p);
    static void guideTimeoutHelperE(void *p);

  private:    
    bool Handshake();
    bool sendCommand(const char *cmd);

    int GuideNSTID { -1 };
    int GuideWETID { -1 };
    ARDUINO_DIRECTION guideDirection;

    int PortFD { -1 };

    Connection::Serial *serialConnection { nullptr };

    const uint8_t ARDUINO_TIMEOUT = 3;
};
