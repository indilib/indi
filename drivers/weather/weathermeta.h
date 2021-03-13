/*******************************************************************************
  Copyright(c) 2015 Jasem Mutlaq. All rights reserved.

  INDI Weather Meta Driver. It watches up to 4 weather drivers and report worst case
  of each in a single property.

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

/* Smart Widget-Property */
#include "indipropertytext.h"
#include "indipropertynumber.h"
#include "indipropertylight.h"

class WeatherMeta : public INDI::DefaultDevice
{
  public:
    WeatherMeta();
    virtual ~WeatherMeta() = default;

    //  Generic indi device entries
    bool Connect();
    bool Disconnect();
    const char *getDefaultName();

    virtual bool ISSnoopDevice(XMLEle *root) override;

    virtual bool initProperties() override;
    virtual bool updateProperties() override;
    virtual void ISGetProperties(const char *dev) override;
    virtual bool ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n) override;

  protected:
    virtual bool saveConfigItems(FILE *fp);

  private:
    void updateOverallState();
    void updateUpdatePeriod();

    // Active stations
    INDI::PropertyText ActiveDeviceTP {4};

    // Stations status
    INDI::PropertyLight StationLP {4};

    // Update Period
    INDI::PropertyNumber UpdatePeriodNP {1};

    double updatePeriods[4];
};
