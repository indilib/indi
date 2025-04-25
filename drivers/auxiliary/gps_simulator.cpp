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

#include "gps_simulator.h"

#include <memory>
#include <ctime>

// We declare an auto pointer to GPSSimulator.
std::unique_ptr<GPSSimulator> gpsSimulator(new GPSSimulator());

GPSSimulator::GPSSimulator()
{
    setVersion(1, 1);
    setDriverInterface(GPS_INTERFACE);
}

const char *GPSSimulator::getDefaultName()
{
    return (const char *)"GPS Simulator";
}

bool GPSSimulator::Connect()
{
    return true;
}

bool GPSSimulator::Disconnect()
{
    return true;
}

bool GPSSimulator::initProperties()
{
    INDI::GPS::initProperties();

    // Location property must be RW.
    LocationNP.fill(m_DefaultDevice->getDeviceName(), "GEOGRAPHIC_COORD", "Location",
                    MAIN_CONTROL_TAB, IP_RW, 60, IPS_IDLE);

    // Original default values.
    LocationNP[LOCATION_LATITUDE].setValue(51.0);
    LocationNP[LOCATION_LONGITUDE].setValue(357.7);
    LocationNP[LOCATION_ELEVATION].setValue(72);

    return true;
}

bool GPSSimulator::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        if (LocationNP.isNameMatch(name))
        {
            LocationNP.update(values, names, n);
            LocationNP.setState(IPS_OK);
            LocationNP.apply();
            LOG_INFO("Values are updated and should be active on the next GPS update.");

            return true;
        }
    }

    return INDI::GPS::ISNewNumber(dev, name, values, names, n);
}


IPState GPSSimulator::updateGPS()
{
    static char ts[32] = {0};
    struct tm *utc, *local;

    time_t raw_time;
    time(&raw_time);

    m_GPSTime = raw_time;

    utc = gmtime(&raw_time);
    strftime(ts, sizeof(ts), "%Y-%m-%dT%H:%M:%S", utc);
    TimeTP[0].setText(ts);

    local = localtime(&raw_time);
    snprintf(ts, sizeof(ts), "%4.2f", (local->tm_gmtoff / 3600.0));
    TimeTP[1].setText(ts);

    TimeTP.setState(IPS_OK);

    LocationNP.setState(IPS_OK);

    return IPS_OK;
}

bool GPSSimulator::saveConfigItems(FILE *fp)
{
    INDI::GPS::saveConfigItems(fp);
    LocationNP.save(fp);

    return true;
}
