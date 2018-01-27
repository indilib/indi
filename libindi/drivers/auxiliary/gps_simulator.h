/*******************************************************************************
  Copyright(c) 2015 Jasem Mutlaq. All rights reserved.

  Simple GPS Simulator

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

/**
 * @brief The GPSSimulator class provides a simple simulator that provide GPS Time and Location services.
 *
 * The time is fetched from the system time while the location is hard-coded to Latitude: 29.1 and Longitude: 48.5
 */
class GPSSimulator : public INDI::GPS
{
  public:
    GPSSimulator();
    virtual ~GPSSimulator() = default;

  protected:
    //  Generic indi device entries
    bool Connect();
    bool Disconnect();
    const char *getDefaultName();

    IPState updateGPS();
};
