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

#include <string.h>

// We declare an auto pointer to GPSSimulator.
std::unique_ptr<GPSSimulator> gpsSimulator(new GPSSimulator());

void ISGetProperties(const char *dev)
{
    gpsSimulator->ISGetProperties(dev);
}

void ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    gpsSimulator->ISNewSwitch(dev, name, states, names, n);
}

void ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n)
{
    gpsSimulator->ISNewText(dev, name, texts, names, n);
}

void ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    gpsSimulator->ISNewNumber(dev, name, values, names, n);
}

void ISNewBLOB(const char *dev, const char *name, int sizes[], int blobsizes[], char *blobs[], char *formats[],
               char *names[], int n)
{
    INDI_UNUSED(dev);
    INDI_UNUSED(name);
    INDI_UNUSED(sizes);
    INDI_UNUSED(blobsizes);
    INDI_UNUSED(blobs);
    INDI_UNUSED(formats);
    INDI_UNUSED(names);
    INDI_UNUSED(n);
}
void ISSnoopDevice(XMLEle *root)
{
    INDI_UNUSED(root);
}

GPSSimulator::GPSSimulator()
{
    setVersion(1, 0);
    setDriverInterface(GPS_INTERFACE);
}

bool GPSSimulator::initProperties()
{
    GPS::initProperties();

    IUFillNumber(&LocationN[LOCATION_LATITUDE], "LAT", "Lat (dd:mm:ss)", "%010.6m", -90, 90, 0, 29.1);
    IUFillNumber(&LocationN[LOCATION_LONGITUDE], "LONG", "Lon (dd:mm:ss)", "%010.6m", 0, 360, 0, 48.5);
    IUFillNumber(&LocationN[LOCATION_ELEVATION], "ELEV", "Elevation (m)", "%g", -200, 10000, 0, 12);
    IUFillNumberVector(&LocationNP, LocationN, 3, getDeviceName(), "GEOGRAPHIC_COORD", "Location", MAIN_CONTROL_TAB,
                       IP_RW, 60, IPS_IDLE);
}

bool GPSSimulator::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        ///////////////////////////////////
        // Geographic Coords
        ///////////////////////////////////
        if (strcmp(name, "GEOGRAPHIC_COORD") == 0)
        {
            int latindex       = IUFindIndex("LAT",  names, n);
            int longindex      = IUFindIndex("LONG", names, n);
            int elevationindex = IUFindIndex("ELEV", names, n);

            if (latindex == -1 || longindex == -1 || elevationindex == -1)
            {
                LocationNP.s = IPS_ALERT;
                IDSetNumber(&LocationNP, "Location data missing or corrupted.");
            }

            double latitude  = values[latindex];
            double longitude = values[longindex];
            double elevation = values[elevationindex];

            /*
            if (latitude == 0 && longitude == 0)
            {
                LOG_DEBUG("Silently ignoring invalid latitude and longitude.");
                LocationNP.s = IPS_IDLE;
                IDSetNumber(&LocationNP, nullptr);
                return false;
            }

            // Do not update if not necessary
            if (latitude  == LocationN[LOCATION_LATITUDE].value &&
                longitude == LocationN[LOCATION_LONGITUDE].value &&
                elevation == LocationN[LOCATION_ELEVATION].value)
            {
                LocationNP.s = IPS_OK;
                IDSetNumber(&LocationNP, nullptr);
            }*/

            LocationNP.s                        = IPS_OK;
            LocationN[LOCATION_LATITUDE].value  = latitude;
            LocationN[LOCATION_LONGITUDE].value = longitude;
            LocationN[LOCATION_ELEVATION].value = elevation;

            //IUUpdateNumber(&LocationNP, values, names, n);

            //  Update client display
            IDSetNumber(&LocationNP, nullptr);

            // Manual trigger
            TimerHit();

            // Always save geographic coord config immediately.
            // saveConfig(true, "GEOGRAPHIC_COORD");
        }
    }

    return GPS::ISNewNumber(dev, name, values, names, n);
}

bool GPSSimulator::saveConfigItems(FILE *fp)
{
    IUSaveConfigNumber(fp, &LocationNP);

    return GPS::saveConfigItems(fp);
}

const char *GPSSimulator::getDefaultName()
{
    return (const char *)"GPS Simulator";
}

bool GPSSimulator::Connect()
{
    // Request Data from Stellarium at Connect
    return true;
}

bool GPSSimulator::Disconnect()
{
    return true;
}

IPState GPSSimulator::updateGPS()
{
    static char ts[32]={0};
    struct tm *utc, *local;

    time_t raw_time;
    time(&raw_time);

    utc = gmtime(&raw_time);
    strftime(ts, sizeof(ts), "%Y-%m-%dT%H:%M:%S", utc);
    IUSaveText(&TimeT[0], ts);

    local = localtime(&raw_time);
    snprintf(ts, sizeof(ts), "%4.2f", (local->tm_gmtoff / 3600.0));
    IUSaveText(&TimeT[1], ts);

    TimeTP.s = IPS_OK;
    LocationNP.s = IPS_OK;

    /*
    if (LocationNP.s==IPS_IDLE)
    {
        LocationN[LOCATION_LATITUDE].value  = 1;//50.841110;
        LocationN[LOCATION_LONGITUDE].value = 2;//6.367500;
        LocationN[LOCATION_ELEVATION].value = 3;//108;
    }*/

    return IPS_OK;
}
