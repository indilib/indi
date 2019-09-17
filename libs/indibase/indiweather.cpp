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
    IUFillText(&ActiveDeviceT[0], "ACTIVE_GPS", "GPS", "GPS Simulator");
    IUFillTextVector(&ActiveDeviceTP, ActiveDeviceT, 1, getDeviceName(), "ACTIVE_DEVICES", "Snoop devices", OPTIONS_TAB,
                     IP_RW, 60, IPS_IDLE);

    // Update Period
    IUFillNumber(&UpdatePeriodN[0], "PERIOD", "Period (secs)", "%4.2f", 0, 3600, 60, 60);
    IUFillNumberVector(&UpdatePeriodNP, UpdatePeriodN, 1, getDeviceName(), "WEATHER_UPDATE", "Update", MAIN_CONTROL_TAB,
                       IP_RW, 60, IPS_IDLE);

    // Refresh
    IUFillSwitch(&RefreshS[0], "REFRESH", "Refresh", ISS_OFF);
    IUFillSwitchVector(&RefreshSP, RefreshS, 1, getDeviceName(), "WEATHER_REFRESH", "Weather", MAIN_CONTROL_TAB, IP_RW,
                       ISR_ATMOST1, 0, IPS_IDLE);

    // Override
    IUFillSwitch(&OverrideS[0], "OVERRIDE", "Override Status", ISS_OFF);
    IUFillSwitchVector(&OverrideSP, OverrideS, 1, getDeviceName(), "WEATHER_OVERRIDE", "Safety", MAIN_CONTROL_TAB, IP_RW,
                       ISR_NOFMANY, 0, IPS_IDLE);


    IDSnoopDevice(ActiveDeviceT[0].text, "GEOGRAPHIC_COORD");

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

bool Weather::updateProperties()
{
    DefaultDevice::updateProperties();

    if (isConnected())
    {
        WI::updateProperties();

        updateTimerID = -1;
        defineSwitch(&RefreshSP);
        defineNumber(&UpdatePeriodNP);
        defineSwitch(&OverrideSP);
        defineNumber(&LocationNP);
        defineText(&ActiveDeviceTP);

        DEBUG(Logger::DBG_SESSION, "Weather update is in progress...");
        TimerHit();
    }
    else
    {
        WI::updateProperties();

        deleteProperty(RefreshSP.name);
        deleteProperty(UpdatePeriodNP.name);
        deleteProperty(OverrideSP.name);
        deleteProperty(LocationNP.name);
        deleteProperty(ActiveDeviceTP.name);
    }

    return true;
}

bool Weather::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        // Refresh
        if (!strcmp(name, RefreshSP.name))
        {
            RefreshS[0].s = ISS_OFF;
            RefreshSP.s   = IPS_OK;
            IDSetSwitch(&RefreshSP, nullptr);

            TimerHit();
        }

        // Override
        if (!strcmp(name, OverrideSP.name))
        {
            IUUpdateSwitch(&OverrideSP, states, names, n);
            if (OverrideS[0].s == ISS_ON)
            {
                LOG_WARN("Weather override is enabled. Observatory is not safe. Turn off override as soon as possible.");
                OverrideSP.s = IPS_BUSY;

                critialParametersLP.s = IPS_OK;
                IDSetLight(&critialParametersLP, nullptr);
            }
            else
            {
                LOG_INFO("Weather override is disabled");
                OverrideSP.s = IPS_IDLE;

                syncCriticalParameters();
            }

            IDSetSwitch(&OverrideSP, nullptr);
            return true;
        }
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

        // Update period
        if (strcmp(name, "WEATHER_UPDATE") == 0)
        {
            IUUpdateNumber(&UpdatePeriodNP, values, names, n);

            UpdatePeriodNP.s = IPS_OK;
            IDSetNumber(&UpdatePeriodNP, nullptr);

            if (UpdatePeriodN[0].value == 0)
                DEBUG(Logger::DBG_SESSION, "Periodic updates are disabled.");
            else
            {
                if (updateTimerID > 0)
                    RemoveTimer(updateTimerID);

                updateTimerID = SetTimer(UpdatePeriodN[0].value * 1000);
            }
            return true;
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
        if (!strcmp(name, ActiveDeviceTP.name))
        {
            ActiveDeviceTP.s = IPS_OK;
            IUUpdateText(&ActiveDeviceTP, texts, names, n);
            //  Update client display
            IDSetText(&ActiveDeviceTP, nullptr);

            IDSnoopDevice(ActiveDeviceT[0].text, "GEOGRAPHIC_COORD");
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

void Weather::TimerHit()
{
    if (!isConnected())
        return;

    if (updateTimerID > 0)
        RemoveTimer(updateTimerID);

    IPState state = updateWeather();

    // Override weather state if required
    if (OverrideS[0].s == ISS_ON)
        state = IPS_OK;

    switch (state)
    {
        // Ok
        case IPS_OK:

            syncCriticalParameters();

            if (OverrideS[0].s == ISS_ON)
                critialParametersLP.s = IPS_OK;

            ParametersNP.s = state;
            IDSetNumber(&ParametersNP, nullptr);

            // If update period is set, then set up the timer
            if (UpdatePeriodN[0].value > 0)
                updateTimerID = SetTimer(static_cast<int>(UpdatePeriodN[0].value * 1000));

            return;

        // Alert
        // We retry every 5000 ms until we get OK
        case IPS_ALERT:
            ParametersNP.s = state;
            IDSetNumber(&ParametersNP, nullptr);
            break;

        // Weather update is in progress
        default:
            break;
    }

    updateTimerID = SetTimer(5000);
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

    IUSaveConfigText(fp, &ActiveDeviceTP);
    IUSaveConfigNumber(fp, &LocationNP);
    IUSaveConfigNumber(fp, &UpdatePeriodNP);

    for (int i = 0; i < nRanges; i++)
        IUSaveConfigNumber(fp, &ParametersRangeNP[i]);

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
