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

#include <memory>
#include <libnova.h>
#include <time.h>
#include <curl/curl.h>

#include "weathermeta.h"

// We declare an auto pointer to WeatherMeta.
std::unique_ptr<WeatherMeta> weatherMeta(new WeatherMeta());

static size_t WriteCallback(void *contents, size_t size, size_t nmemb, void *userp)
{
    ((std::string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}

void ISGetProperties(const char *dev)
{
        weatherMeta->ISGetProperties(dev);
}

void ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int num)
{
        weatherMeta->ISNewSwitch(dev, name, states, names, num);
}

void ISNewText(	const char *dev, const char *name, char *texts[], char *names[], int num)
{
        weatherMeta->ISNewText(dev, name, texts, names, num);
}

void ISNewNumber(const char *dev, const char *name, double values[], char *names[], int num)
{
        weatherMeta->ISNewNumber(dev, name, values, names, num);
}

void ISNewBLOB (const char *dev, const char *name, int sizes[], int blobsizes[], char *blobs[], char *formats[], char *names[], int n)
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
void ISSnoopDevice (XMLEle *root)
{
    weatherMeta->ISSnoopDevice(root);
}

WeatherMeta::WeatherMeta()
{
   setVersion(1,0);

}

WeatherMeta::~WeatherMeta()
{

}

const char * WeatherMeta::getDefaultName()
{
    return (char *)"Weather Meta";
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
    IUFillText(&ActiveDeviceT[0],"ACTIVE_WEATHER_1","Station #1",NULL);
    IUFillText(&ActiveDeviceT[1],"ACTIVE_WEATHER_2","Station #2",NULL);
    IUFillText(&ActiveDeviceT[2],"ACTIVE_WEATHER_3","Station #3",NULL);
    IUFillText(&ActiveDeviceT[3],"ACTIVE_WEATHER_4","Station #4",NULL);
    IUFillTextVector(&ActiveDeviceTP,ActiveDeviceT,4,getDeviceName(),"ACTIVE_DEVICES","Stations",OPTIONS_TAB,IP_RW,60,IPS_IDLE);

    // Station Status
    IUFillLight(&StationL[0], "STATION_STATUS_1", "Station #1", IPS_IDLE);
    IUFillLight(&StationL[1], "STATION_STATUS_2", "Station #2", IPS_IDLE);
    IUFillLight(&StationL[2], "STATION_STATUS_3", "Station #3", IPS_IDLE);
    IUFillLight(&StationL[3], "STATION_STATUS_4", "Station #4", IPS_IDLE);
    IUFillLightVector(&StationLP, StationL, 4, getDeviceName(), "WEATHER_STATUS", "Status", MAIN_CONTROL_TAB, IPS_IDLE);

    // Update Period
    IUFillNumber(&UpdatePeriodN[0],"PERIOD","Period (secs)","%4.2f",0,3600,60,60);
    IUFillNumberVector(&UpdatePeriodNP,UpdatePeriodN,1,getDeviceName(),"WEATHER_UPDATE","Update",MAIN_CONTROL_TAB,IP_RW,60,IPS_IDLE);

    addDebugControl();

    setDriverInterface(AUX_INTERFACE);

    return true;

}

void WeatherMeta::ISGetProperties(const char *dev)
{
    INDI::DefaultDevice::ISGetProperties(dev);

    defineText(&ActiveDeviceTP);

    loadConfig(true, "ACTIVE_DEVICES");
}

bool WeatherMeta::updateProperties()
{
    INDI::DefaultDevice::updateProperties();

    if (isConnected())
    {
        // If Active devices are already defined, let's set the active devices as labels
        for (int i=0; i < 4; i++)
        {
            if (ActiveDeviceT[i].text && ActiveDeviceT[i].text[0])
                strncpy(StationL[i].label, ActiveDeviceT[i].text, MAXINDILABEL);
        }

        defineLight(&StationLP);
        defineNumber(&UpdatePeriodNP);
    }
    else
    {
        deleteProperty(StationLP.name);
        deleteProperty(UpdatePeriodNP.name);
    }
}

bool WeatherMeta::ISNewText (const char *dev, const char *name, char *texts[], char *names[], int n)
{
    if(!strcmp(dev,getDeviceName()))
    {
        if(!strcmp(name,ActiveDeviceTP.name))
        {
            ActiveDeviceTP.s=IPS_OK;
            IUUpdateText(&ActiveDeviceTP,texts,names,n);
            //  Update client display
            IDSetText(&ActiveDeviceTP,NULL);

            if (ActiveDeviceT[0].text)
                IDSnoopDevice(ActiveDeviceT[0].text,"WEATHER_STATUS");
            if (ActiveDeviceT[1].text)
                IDSnoopDevice(ActiveDeviceT[1].text,"WEATHER_STATUS");
            if (ActiveDeviceT[2].text)
                IDSnoopDevice(ActiveDeviceT[2].text,"WEATHER_STATUS");
            if (ActiveDeviceT[3].text)
                IDSnoopDevice(ActiveDeviceT[3].text,"WEATHER_STATUS");

            return true;
        }
    }

     return INDI::DefaultDevice::ISNewText(dev,name,texts,names,n);
}

bool WeatherMeta::ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n)
{
    //  first check if it's for our device
    if(strcmp(dev,getDeviceName())==0)
    {
        // Update period
        if(strcmp(name,"WEATHER_UPDATE")==0)
        {
            IUUpdateNumber(&UpdatePeriodNP, values, names, n);
            UpdatePeriodNP.s = IPS_OK;
            IDSetNumber(&UpdatePeriodNP, NULL);

            if (UpdatePeriodN[0].value == 0)
                DEBUG(INDI::Logger::DBG_SESSION,  "Clients are recommended to stop monitoring the overall weather status.");
            else
                DEBUGF(INDI::Logger::DBG_SESSION, "Clients are recommended to query the overall weather status every %d seconds.", (int) UpdatePeriodN[0].value);

            return true;
        }
    }

    return DefaultDevice::ISNewNumber(dev,name,values,names,n);
}

bool WeatherMeta::saveConfigItems(FILE *fp)
{
    INDI::DefaultDevice::saveConfigItems(fp);

    IUSaveConfigText(fp, &ActiveDeviceTP);
    IUSaveConfigNumber(fp, &UpdatePeriodNP);

    return true;
}

bool WeatherMeta::ISSnoopDevice(XMLEle *root)
{
    const char *propName   = findXMLAttValu(root, "name");
    const char *deviceName = findXMLAttValu(root, "device");

    if (isConnected())
    {
        if (!strcmp(propName, "WEATHER_STATUS"))
        {

            for (int i=0; i < 4; i++)
            {
                if (ActiveDeviceT[i].text && !strcmp(ActiveDeviceT[i].text, deviceName))
                {
                    IPState stationState;

                    if (crackIPState(findXMLAttValu(root, "state"), &stationState) < 0)
                        break;

                    StationL[i].s = stationState;

                    updateOverallState();

                    break;
                }
            }

            return true;
        }
    }

    return INDI::DefaultDevice::ISSnoopDevice(root);
}

void WeatherMeta::updateOverallState()
{
    StationLP.s = IPS_IDLE;

    for (int i=0; i < 4; i++)
    {
        if (StationL[i].s > StationLP.s)
            StationLP.s = StationL[i].s;
    }

    IDSetLight(&StationLP, NULL);
}
