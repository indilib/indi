/*
    LX200 GPS
    Copyright (C) 2003 Jasem Mutlaq (mutlaqja@ikarustech.com)

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

#include "lx200autostar.h"

/* Smart Widget-Property */
#include "indipropertynumber.h"
#include "indipropertyswitch.h"

class LX200GPS : public LX200Autostar
{
  public:
    LX200GPS();
    ~LX200GPS() {}

    const char *getDefaultName();
    bool initProperties();
    bool updateProperties();
    void ISGetProperties(const char *dev);
    bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n);
    virtual bool updateTime(ln_date *utc, double utc_offset);

  protected:
    virtual bool UnPark();
    INDI::PropertySwitch GPSPowerSP {2};
    INDI::PropertySwitch GPSStatusSP {3};
    INDI::PropertySwitch GPSUpdateSP {2};
    INDI::PropertySwitch AltDecPecSP {2};
    INDI::PropertySwitch AzRaPecSP {2};
    INDI::PropertySwitch SelenSyncSP {1};
    INDI::PropertySwitch AltDecBacklashSP {1};
    INDI::PropertySwitch AzRaBacklashSP {1};
    INDI::PropertySwitch OTAUpdateSP {1};
    INDI::PropertyNumber OTATempNP {1};
};
