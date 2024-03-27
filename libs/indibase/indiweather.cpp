/*******************************************************************************
  Copyright(c) 2015 Jasem Mutlaq. All rights reserved.

  INDI Weather Device Class

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

#include "indiweather.h"

#include "connectionplugins/connectionserial.h"
#include "connectionplugins/connectiontcp.h"

#include <cstring>

#define PARAMETERS_TAB "Parameters"

namespace INDI
{

Weather::Weather() : WI(this)
{
}

bool Weather::initProperties()
{
    DefaultDevice::initProperties();
    WI::initProperties(MAIN_CONTROL_TAB, PARAMETERS_TAB);

    // Location
    IUFillNumber(&LocationN[LOCATION_LATITUDE], "LAT", "Lat (dd:mm:ss)", "%010.6m", -90, 90, 0, 0.0);
    IUFillNumber(&LocationN[LOCATION_LONGITUDE], "LONG", "Lon (dd:mm:ss)", "%010.6m", 0, 360, 0, 0.0);
    IUFillNumber(&LocationN[LOCATION_ELEVATION], "ELEV", "Elevation (m)", "%g", -200, 10000, 0, 0);
    IUFillNumberVector(&LocationNP, LocationN, 3, getDeviceName(), "GEOGRAPHIC_COORD", "Location", SITE_TAB, IP_RW, 60,
                       IPS_OK);

    // Active Devices
    ActiveDeviceTP[0].fill("ACTIVE_GPS", "GPS", "GPS Simulator");
    ActiveDeviceTP.fill(getDeviceName(), "ACTIVE_DEVICES", "Snoop devices", OPTIONS_TAB, IP_RW, 60, IPS_IDLE);
    ActiveDeviceTP.load();

    IDSnoopDevice(ActiveDeviceTP[0].getText(), "GEOGRAPHIC_COORD");

    if (weatherConnection & CONNECTION_SERIAL)
    {
        serialConnection = new Connection::Serial(this);
        serialConnection->registerHandshake([&]()
        {
            return callHandshake();
        });
        registerConnection(serialConnection);
    }

    if (weatherConnection & CONNECTION_TCP)
    {
        tcpConnection = new Connection::TCP(this);
        tcpConnection->registerHandshake([&]()
        {
            return callHandshake();
        });
        registerConnection(tcpConnection);
    }

    setDriverInterface(WEATHER_INTERFACE);

    return true;
}

void Weather::ISGetProperties(const char *dev)
{
    DefaultDevice::ISGetProperties(dev);
    defineProperty(ActiveDeviceTP);
}

bool Weather::updateProperties()
{
    DefaultDevice::updateProperties();

    if (isConnected())
    {
        WI::updateProperties();

        defineProperty(&LocationNP);

        DEBUG(Logger::DBG_SESSION, "Weather update is in progress...");
    }
    else
    {
        WI::updateProperties();

        deleteProperty(LocationNP.name);
    }

    return true;
}

bool Weather::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        if (processSwitch(dev, name, states, names, n))
            return true;
    }

    return DefaultDevice::ISNewSwitch(dev, name, states, names, n);
}

bool Weather::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    //  first check if it's for our device
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        if (strcmp(name, "GEOGRAPHIC_COORD") == 0)
        {
            int latindex       = IUFindIndex("LAT", names, n);
            int longindex      = IUFindIndex("LONG", names, n);
            int elevationindex = IUFindIndex("ELEV", names, n);

            if (latindex == -1 || longindex == -1 || elevationindex == -1)
            {
                LocationNP.s = IPS_ALERT;
                IDSetNumber(&LocationNP, "Location data missing or corrupted.");
            }

            double targetLat  = values[latindex];
            double targetLong = values[longindex];
            double targetElev = values[elevationindex];

            return processLocationInfo(targetLat, targetLong, targetElev);
        }

        // Pass to weather interface
        if (processNumber(dev, name, values, names, n))
            return true;
    }

    return DefaultDevice::ISNewNumber(dev, name, values, names, n);
}

bool INDI::Weather::ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n)
{
    //  first check if it's for our device
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        if (ActiveDeviceTP.isNameMatch(name))
        {
            ActiveDeviceTP.setState(IPS_OK);
            ActiveDeviceTP.update(texts, names, n);
            //  Update client display
            ActiveDeviceTP.apply();
            IDSnoopDevice(ActiveDeviceTP[0].getText(), "GEOGRAPHIC_COORD");
            return true;
        }
    }
    return DefaultDevice::ISNewText(dev, name, texts, names, n);
}

bool INDI::Weather::ISSnoopDevice(XMLEle *root)
{
    XMLEle *ep           = nullptr;
    const char *propName = findXMLAttValu(root, "name");

    if (isConnected())
    {
        if (!strcmp(propName, "GEOGRAPHIC_COORD"))
        {
            // Only accept IPS_OK state
            if (strcmp(findXMLAttValu(root, "state"), "Ok"))
                return false;

            double longitude = -1, latitude = -1, elevation = -1;

            for (ep = nextXMLEle(root, 1); ep != nullptr; ep = nextXMLEle(root, 0))
            {
                const char *elemName = findXMLAttValu(ep, "name");

                if (!strcmp(elemName, "LAT"))
                    latitude = atof(pcdataXMLEle(ep));
                else if (!strcmp(elemName, "LONG"))
                    longitude = atof(pcdataXMLEle(ep));
                else if (!strcmp(elemName, "ELEV"))
                    elevation = atof(pcdataXMLEle(ep));
            }

            return processLocationInfo(latitude, longitude, elevation);
        }
    }

    return DefaultDevice::ISSnoopDevice(root);
}

bool Weather::updateLocation(double latitude, double longitude, double elevation)
{
    INDI_UNUSED(latitude);
    INDI_UNUSED(longitude);
    INDI_UNUSED(elevation);

    return true;
}

bool Weather::processLocationInfo(double latitude, double longitude, double elevation)
{
    // Do not update if not necessary
    if (latitude == LocationN[LOCATION_LATITUDE].value && longitude == LocationN[LOCATION_LONGITUDE].value &&
            elevation == LocationN[LOCATION_ELEVATION].value)
    {
        LocationNP.s = IPS_OK;
        IDSetNumber(&LocationNP, nullptr);
    }

    if (updateLocation(latitude, longitude, elevation))
    {
        LocationNP.s                        = IPS_OK;
        LocationN[LOCATION_LATITUDE].value  = latitude;
        LocationN[LOCATION_LONGITUDE].value = longitude;
        LocationN[LOCATION_ELEVATION].value = elevation;
        //  Update client display
        IDSetNumber(&LocationNP, nullptr);

        return true;
    }
    else
    {
        LocationNP.s = IPS_ALERT;
        //  Update client display
        IDSetNumber(&LocationNP, nullptr);
        return false;
    }
}


bool Weather::saveConfigItems(FILE *fp)
{
    DefaultDevice::saveConfigItems(fp);
    WI::saveConfigItems(fp);
    ActiveDeviceTP.save(fp);
    IUSaveConfigNumber(fp, &LocationNP);
    return true;
}

bool Weather::Handshake()
{
    return false;
}

bool Weather::callHandshake()
{
    if (weatherConnection > 0)
    {
        if (getActiveConnection() == serialConnection)
            PortFD = serialConnection->getPortFD();
        else if (getActiveConnection() == tcpConnection)
            PortFD = tcpConnection->getPortFD();
    }

    return Handshake();
}

uint8_t Weather::getWeatherConnection() const
{
    return weatherConnection;
}

void Weather::setWeatherConnection(const uint8_t &value)
{
    uint8_t mask = CONNECTION_SERIAL | CONNECTION_TCP | CONNECTION_NONE;

    if (value == 0 || (mask & value) == 0)
    {
        DEBUGF(Logger::DBG_ERROR, "Invalid connection mode %d", value);
        return;
    }

    weatherConnection = value;
}
}
