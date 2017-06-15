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

namespace Connection
{
class TCP;
}

class SQM : public INDI::DefaultDevice
{
  public:
    SQM();
    virtual ~SQM();

    virtual bool initProperties();
    virtual bool updateProperties();

  protected:
    const char *getDefaultName();
    void TimerHit();

  private:
    bool getReadings();
    bool getDeviceInfo();

    // Readings
    INumberVectorProperty AverageReadingNP;
    INumber AverageReadingN[5];

    // Device Information
    INumberVectorProperty UnitInfoNP;
    INumber UnitInfoN[4];

    Connection::TCP *tcpConnection = NULL;

    int PortFD = -1;
};
