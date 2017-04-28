/*******************************************************************************
  Copyright(c) 2015 Jasem Mutlaq. All rights reserved.

  INDI Davis Vantage Pro/Pro2/Vue Weather Driver

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

#ifndef VANTAGE_H
#define VANTAGE_H

#include "indiweather.h"

class Vantage : public INDI::Weather
{
    public:

    // Forcecast bit mask from Davis Vantage manual
    enum { W_RAINY            = 1 << 0,
           W_CLOUDY           = 1 << 1,
           W_PARTLY_CLOUDY    = 1 << 2,
           W_SUNNY            = 1 << 3,
           W_SNOW             = 1 << 4 };

    Vantage();
    virtual ~Vantage();

    //  Generic indi device entries
    virtual bool Handshake() override;
    const char *getDefaultName();

    virtual bool initProperties();

    protected:

    virtual IPState updateWeather();

private:

    bool ack();
    bool wakeup();
};

#endif
