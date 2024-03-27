/*******************************************************************************
  Copyright(c) 2015 Jasem Mutlaq. All rights reserved.

  INDI Weather Underground (TM) Weather Driver

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

#include "weathermeta.h"

#include <cstring>
#include <memory>

// We declare an auto pointer to WeatherMeta.
std::unique_ptr<WeatherMeta> weatherMeta(new WeatherMeta());

WeatherMeta::WeatherMeta()
{
    setVersion(1, 0);

    updatePeriods[0] = updatePeriods[1] = updatePeriods[2] = updatePeriods[3] = -1;
}

const char *WeatherMeta::getDefaultName()
{
    return (const char *)"Weather Meta";
}

bool WeatherMeta::Connect()
{
    return true;
}

bool WeatherMeta::Disconnect()
{
    return true;
}

bool WeatherMeta::initProperties()
{
    INDI::DefaultDevice::initProperties();

    // Active Devices
    ActiveDeviceTP[ACTIVE_WEATHER_1].fill("ACTIVE_WEATHER_1", "Station #1", nullptr);
    ActiveDeviceTP[ACTIVE_WEATHER_2].fill( "ACTIVE_WEATHER_2", "Station #2", nullptr);
    ActiveDeviceTP[ACTIVE_WEATHER_3].fill( "ACTIVE_WEATHER_3", "Station #3", nullptr);
    ActiveDeviceTP[ACTIVE_WEATHER_4].fill("ACTIVE_WEATHER_4", "Station #4", nullptr);
    ActiveDeviceTP.fill(getDeviceName(), "ACTIVE_DEVICES", "Stations", OPTIONS_TAB,
                        IP_RW, 60, IPS_IDLE);

    // Station Status
    StationLP[STATION_STATUS_1].fill("STATION_STATUS_1", "Station #1", IPS_IDLE);
    StationLP[STATION_STATUS_2].fill("STATION_STATUS_2", "Station #2", IPS_IDLE);
    StationLP[STATION_STATUS_3].fill("STATION_STATUS_3", "Station #3", IPS_IDLE);
    StationLP[STATION_STATUS_4].fill("STATION_STATUS_4", "Station #4", IPS_IDLE);
    StationLP.fill(getDeviceName(), "WEATHER_STATUS", "Status", MAIN_CONTROL_TAB, IPS_IDLE);

    // Update Period
    UpdatePeriodNP[0].fill("PERIOD", "Period (secs)", "%4.2f", 0, 3600, 60, 60);
    UpdatePeriodNP.fill(getDeviceName(), "WEATHER_UPDATE", "Update", MAIN_CONTROL_TAB,
                        IP_RO, 60, IPS_IDLE);

    addDebugControl();

    setDriverInterface(AUX_INTERFACE);

    return true;
}

void WeatherMeta::ISGetProperties(const char *dev)
{
    INDI::DefaultDevice::ISGetProperties(dev);

    defineProperty(ActiveDeviceTP);

    loadConfig(true, "ACTIVE_DEVICES");
}

bool WeatherMeta::updateProperties()
{
    INDI::DefaultDevice::updateProperties();

    if (isConnected())
    {
        // If Active devices are already defined, let's set the active devices as labels
        for (int i = 0; i < 4; i++)
        {
            if (ActiveDeviceTP[i].getText() != nullptr && ActiveDeviceTP[i].getText()[0] != 0)
                StationLP[i].setLabel(ActiveDeviceTP[i].getText());
        }

        defineProperty(StationLP);
        defineProperty(UpdatePeriodNP);
    }
    else
    {
        deleteProperty(StationLP.getName());
        deleteProperty(UpdatePeriodNP.getName());
    }
    return true;
}

bool WeatherMeta::ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        if (ActiveDeviceTP.isNameMatch(name))
        {
            ActiveDeviceTP.setState(IPS_OK);
            ActiveDeviceTP.update(texts, names, n);
            //  Update client display
            ActiveDeviceTP.apply();

            if (ActiveDeviceTP[ACTIVE_WEATHER_1].getText() != nullptr)
            {
                IDSnoopDevice(ActiveDeviceTP[ACTIVE_WEATHER_1].getText(), "WEATHER_STATUS");
                IDSnoopDevice(ActiveDeviceTP[ACTIVE_WEATHER_1].getText(), "WEATHER_UPDATE");
            }
            if (ActiveDeviceTP[ACTIVE_WEATHER_1].text != nullptr)
            {
                IDSnoopDevice(ActiveDeviceTP[ACTIVE_WEATHER_1].getText(), "WEATHER_STATUS");
                IDSnoopDevice(ActiveDeviceTP[ACTIVE_WEATHER_1].getText(), "WEATHER_UPDATE");
            }
            if (ActiveDeviceTP[ACTIVE_WEATHER_3].getText() != nullptr)
            {
                IDSnoopDevice(ActiveDeviceTP[ACTIVE_WEATHER_3].getText(), "WEATHER_STATUS");
                IDSnoopDevice(ActiveDeviceTP[ACTIVE_WEATHER_3].getText(), "WEATHER_UPDATE");
            }
            if (ActiveDeviceTP[ACTIVE_WEATHER_4].getText() != nullptr)
            {
                IDSnoopDevice(ActiveDeviceTP[ACTIVE_WEATHER_4].getText(), "WEATHER_STATUS");
                IDSnoopDevice(ActiveDeviceTP[ACTIVE_WEATHER_4].getText(), "WEATHER_UPDATE");
            }

            saveConfig(ActiveDeviceTP);

            return true;
        }
    }

    return INDI::DefaultDevice::ISNewText(dev, name, texts, names, n);
}

bool WeatherMeta::saveConfigItems(FILE *fp)
{
    INDI::DefaultDevice::saveConfigItems(fp);

    ActiveDeviceTP.save(fp);
    UpdatePeriodNP.save(fp);

    return true;
}

bool WeatherMeta::ISSnoopDevice(XMLEle *root)
{
    const char *propName   = findXMLAttValu(root, "name");
    const char *deviceName = findXMLAttValu(root, "device");

    if (isConnected())
    {
        if (strcmp(propName, "WEATHER_STATUS") == 0)
        {
            for (int i = 0; i < 4; i++)
            {
                if (ActiveDeviceTP[i].getText() != nullptr && strcmp(ActiveDeviceTP[i].getText(), deviceName) == 0)
                {
                    IPState stationState;

                    if (crackIPState(findXMLAttValu(root, "state"), &stationState) < 0)
                        break;

                    StationLP[i].setState(stationState);

                    updateOverallState();

                    break;
                }
            }

            return true;
        }

        if (strcmp(propName, "WEATHER_UPDATE") == 0)
        {
            XMLEle *ep = nextXMLEle(root, 1);

            for (int i = 0; i < 4; i++)
            {
                if (ActiveDeviceTP[i].getText() != nullptr && strcmp(ActiveDeviceTP[i].getText(), deviceName) == 0)
                {
                    updatePeriods[i] = atof(pcdataXMLEle(ep));

                    updateUpdatePeriod();
                    break;
                }
            }
        }
    }

    return INDI::DefaultDevice::ISSnoopDevice(root);
}

void WeatherMeta::updateOverallState()
{
    StationLP.setState(IPS_IDLE);

    for (int i = 0; i < 4; i++)
    {
        if (StationLP[i].getState() > StationLP.getState())
            StationLP.setState(StationLP[i].getState());
    }

    StationLP.apply();
}

void WeatherMeta::updateUpdatePeriod()
{
    double minPeriod = UpdatePeriodNP[0].getMin();

    for (int i = 0; i < 4; i++)
    {
        if (updatePeriods[i] > 0 && updatePeriods[i] < minPeriod)
            minPeriod = updatePeriods[i];
    }

    if (minPeriod != UpdatePeriodNP[0].getMax())
    {
        UpdatePeriodNP[0].setValue(minPeriod);
        UpdatePeriodNP.apply();
    }
}
