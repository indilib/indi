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

#ifndef GPS_DRIVER_H
#define GPS_DRIVER_H

#include "indigps.h"
#include <libgpsmm.h>

class GPSD : public INDI::GPS
{
    public:
        GPSD();
        virtual ~GPSD();

        IText GPSstatusT[1];
        ITextVectorProperty GPSstatusTP;

        INumber PolarisN[1];
        INumberVectorProperty PolarisNP;

        ISwitch RefreshS[1];
        ISwitchVectorProperty RefreshSP;

    protected:
        gpsmm *gps;
        //  Generic indi device entries
        bool Connect();
        bool Disconnect();
        const char *getDefaultName();
        bool initProperties();
        bool updateProperties();
        IPState updateGPS();

};

#endif // GPS_DRIVER_H
