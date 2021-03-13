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
    LocationNP[LOCATION_LATITUDE].fill("LAT", "Lat (dd:mm:ss)", "%010.6m", -90, 90, 0, 0.0);
    LocationNP[LOCATION_LONGITUDE].fill("LONG", "Lon (dd:mm:ss)", "%010.6m", 0, 360, 0, 0.0);
    LocationNP[LOCATION_ELEVATION].fill("ELEV", "Elevation (m)", "%g", -200, 10000, 0, 0);
    LocationNP.fill(getDeviceName(), "GEOGRAPHIC_COORD", "Location", SITE_TAB, IP_RW, 60,
                       IPS_OK);

    // Active Devices
    ActiveDeviceTP[0].fill("ACTIVE_GPS", "GPS", "GPS Simulator");
    ActiveDeviceTP.fill(getDeviceName(), "ACTIVE_DEVICES", "Snoop devices", OPTIONS_TAB,
                     IP_RW, 60, IPS_IDLE);

    // Update Period
    UpdatePeriodNP[0].fill("PERIOD", "Period (secs)", "%4.2f", 0, 3600, 60, 60);
    UpdatePeriodNP.fill(getDeviceName(), "WEATHER_UPDATE", "Update", MAIN_CONTROL_TAB,
                       IP_RW, 60, IPS_IDLE);

    // Refresh
    RefreshSP[0].fill("REFRESH", "Refresh", ISS_OFF);
    RefreshSP.fill(getDeviceName(), "WEATHER_REFRESH", "Weather", MAIN_CONTROL_TAB, IP_RW,
                       ISR_ATMOST1, 0, IPS_IDLE);

    // Override
    OverrideSP[0].fill("OVERRIDE", "Override Status", ISS_OFF);
    OverrideSP.fill(getDeviceName(), "WEATHER_OVERRIDE", "Safety", MAIN_CONTROL_TAB, IP_RW,
                       ISR_NOFMANY, 0, IPS_IDLE);


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

bool Weather::updateProperties()
{
    DefaultDevice::updateProperties();

    if (isConnected())
    {
        WI::updateProperties();

        updateTimerID = -1;
        defineProperty(RefreshSP);
        defineProperty(UpdatePeriodNP);
        defineProperty(OverrideSP);
        defineProperty(LocationNP);
        defineProperty(ActiveDeviceTP);

        DEBUG(Logger::DBG_SESSION, "Weather update is in progress...");
        TimerHit();
    }
    else
    {
        WI::updateProperties();

        deleteProperty(RefreshSP.getName());
        deleteProperty(UpdatePeriodNP.getName());
        deleteProperty(OverrideSP.getName());
        deleteProperty(LocationNP.getName());
        deleteProperty(ActiveDeviceTP.getName());
    }

    return true;
}

bool Weather::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        // Refresh
        if (RefreshSP.isNameMatch(name))
        {
            RefreshSP[0].setState(ISS_OFF);
            RefreshSP.setState(IPS_OK);
            RefreshSP.apply();

            TimerHit();
        }

        // Override
        if (OverrideSP.isNameMatch(name))
        {
            OverrideSP.update(states, names, n);
            if (OverrideSP[0].getState() == ISS_ON)
            {
                LOG_WARN("Weather override is enabled. Observatory is not safe. Turn off override as soon as possible.");
                OverrideSP.setState(IPS_BUSY);

                critialParametersLP.s = IPS_OK;
                IDSetLight(&critialParametersLP, nullptr);
            }
            else
            {
                LOG_INFO("Weather override is disabled");
                OverrideSP.setState(IPS_IDLE);

                syncCriticalParameters();
                IDSetLight(&critialParametersLP, nullptr);
            }

            OverrideSP.apply();
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
                LocationNP.setState(IPS_ALERT);
                LocationNP.apply("Location data missing or corrupted.");
            }

            double targetLat  = values[latindex];
            double targetLong = values[longindex];
            double targetElev = values[elevationindex];

            return processLocationInfo(targetLat, targetLong, targetElev);
        }

        // Update period
        if (strcmp(name, "WEATHER_UPDATE") == 0)
        {
            UpdatePeriodNP.update(values, names, n);

            UpdatePeriodNP.setState(IPS_OK);
            UpdatePeriodNP.apply();

            if (UpdatePeriodNP[0].value == 0)
                DEBUG(Logger::DBG_SESSION, "Periodic updates are disabled.");
            else
            {
                if (updateTimerID > 0)
                    RemoveTimer(updateTimerID);

                updateTimerID = SetTimer(UpdatePeriodNP[0].value * 1000);
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

void Weather::TimerHit()
{
    if (!isConnected())
        return;

    if (updateTimerID > 0)
        RemoveTimer(updateTimerID);

    IPState state = updateWeather();

    switch (state)
    {
        // Ok
        case IPS_OK:

            if (syncCriticalParameters())
            {
                // Override weather state if required
                if (OverrideSP[0].getState() == ISS_ON)
                    critialParametersLP.s = IPS_OK;

                IDSetLight(&critialParametersLP, nullptr);
            }

            ParametersNP.s = state;
            IDSetNumber(&ParametersNP, nullptr);

            // If update period is set, then set up the timer
            if (UpdatePeriodNP[0].value > 0)
                updateTimerID = SetTimer(static_cast<int>(UpdatePeriodNP[0].value * 1000));

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
    if (latitude == LocationNP[LOCATION_LATITUDE].value && longitude == LocationNP[LOCATION_LONGITUDE].value &&
            elevation == LocationNP[LOCATION_ELEVATION].getValue())
    {
        LocationNP.setState(IPS_OK);
        LocationNP.apply();
    }

    if (updateLocation(latitude, longitude, elevation))
    {
        LocationNP.setState(IPS_OK);
        LocationNP[LOCATION_LATITUDE].setValue(latitude);
        LocationNP[LOCATION_LONGITUDE].setValue(longitude);
        LocationNP[LOCATION_ELEVATION].setValue(elevation);
        //  Update client display
        LocationNP.apply();

        return true;
    }
    else
    {
        LocationNP.setState(IPS_ALERT);
        //  Update client display
        LocationNP.apply();
        return false;
    }
}


bool Weather::saveConfigItems(FILE *fp)
{
    DefaultDevice::saveConfigItems(fp);
    WI::saveConfigItems(fp);
    ActiveDeviceTP.save(fp);
    LocationNP.save(fp);
    UpdatePeriodNP.save(fp);
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
