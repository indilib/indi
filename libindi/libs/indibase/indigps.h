/*******************************************************************************
  Copyright(c) 2015 Jasem Mutlaq. All rights reserved.

  INDI GPS Device Class

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

#ifndef INDIGPS_H
#define INDIGPS_H

#include <defaultdevice.h>

class INDI::GPS : public INDI::DefaultDevice
{
    public:

    enum GPSLocation { LOCATION_LATITUDE, LOCATION_LONGITUDE, LOCATION_ELEVATION };

    GPS();
    virtual ~GPS();

    virtual bool initProperties();
    virtual bool updateProperties();
    virtual bool ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n);

    protected:

    /**
     * @brief updateGPS Retrieve Location & Time from GPS. Update LocationNP & TimeTP properties withouit sending them to the client (i.e. IDSetXXX).
     * @return true if successful, false otherwise.
     */
    virtual bool updateGPS();

    //  A number vector that stores lattitude and longitude
    INumberVectorProperty LocationNP;
    INumber LocationN[3];

    // UTC and UTC Offset
    IText TimeT[2];
    ITextVectorProperty TimeTP;

    // Refresh data
    ISwitch RefreshS[1];
    ISwitchVectorProperty RefreshSP;

};

#endif // INDIGPS_H
