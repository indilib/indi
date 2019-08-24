/*******************************************************************************
  Copyright(c) 2015 Jasem Mutlaq. All rights reserved.
  Copyright(c) 2015 Paweł T. Jochym  <jochym AT gmail DOT com>
..Copyright(c) 2014 Radek Kaczorek  <rkaczorek AT gmail DOT com>
  Based on Simple GPS Simulator by Jasem Mutlaq

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

#include "indigps.h"

class gpsmm;

class GPSD : public INDI::GPS
{
    public:
        GPSD();

        virtual const char *getDefaultName() override;
        virtual bool initProperties() override;
        virtual bool updateProperties() override;

        virtual bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n) override;

    protected:
        virtual bool Connect() override;
        virtual bool Disconnect() override;

        virtual bool saveConfigItems(FILE *fp) override;

        virtual IPState updateGPS() override;

    private:
        gpsmm *gps = nullptr;

        ITextVectorProperty GPSstatusTP;
        IText GPSstatusT[1] {};

        INumberVectorProperty PolarisNP;
        INumber PolarisN[1];

        ISwitchVectorProperty TimeSourceSP;
        ISwitch TimeSourceS[2];
        enum
        {
            TS_GPS,
            TS_SYSTEM
        };

};
