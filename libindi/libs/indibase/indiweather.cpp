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

#define POLLMS  5000
#define PARAMETERS_TAB  "Parameters"

INDI::Weather::Weather()
{
    ParametersN=NULL;
    critialParametersL=NULL;
    updateTimerID=-1;

    ParametersRangeNP = NULL;
    nRanges=0;
}

INDI::Weather::~Weather()
{
    for (int i=0; i < ParametersNP.nnp; i++)
    {
        free(ParametersN[i].aux0);
        free(ParametersN[i].aux1);
        free(ParametersRangeNP[i].np);
    }

    free(ParametersN);
    free(ParametersRangeNP);
    free(critialParametersL);
}

bool INDI::Weather::initProperties()
{
    INDI::DefaultDevice::initProperties();

    // Parameters
    IUFillNumberVector(&ParametersNP, NULL, 0, getDeviceName(), "WEATHER_PARAMETERS", "Parameters", PARAMETERS_TAB, IP_RO, 60, IPS_OK);

    // Refresh
    IUFillSwitch(&RefreshS[0], "REFRESH", "Refresh", ISS_OFF);
    IUFillSwitchVector(&RefreshSP, RefreshS, 1, getDeviceName(), "WEATHER_REFRESH", "Weather", MAIN_CONTROL_TAB, IP_RW, ISR_ATMOST1, 0, IPS_IDLE);

    // Weather Status
    IUFillLightVector(&critialParametersLP, NULL, 0, getDeviceName(), "WEATHER_STATUS", "Status", MAIN_CONTROL_TAB, IPS_IDLE);

    // Location
    IUFillNumber(&LocationN[LOCATION_LATITUDE],"LAT","Lat (dd:mm:ss)","%010.6m",-90,90,0,0.0);
    IUFillNumber(&LocationN[LOCATION_LONGITUDE],"LONG","Lon (dd:mm:ss)","%010.6m",0,360,0,0.0 );
    IUFillNumber(&LocationN[LOCATION_ELEVATION],"ELEV","Elevation (m)","%g",-200,10000,0,0 );
    IUFillNumberVector(&LocationNP,LocationN,3,getDeviceName(),"GEOGRAPHIC_COORD","Location", SITE_TAB,IP_RW,60,IPS_OK);

    // Update Period
    IUFillNumber(&UpdatePeriodN[0],"PERIOD","Period (secs)","%4.2f",0,3600,60,60);
    IUFillNumberVector(&UpdatePeriodNP,UpdatePeriodN,1,getDeviceName(),"WEATHER_UPDATE","Update",MAIN_CONTROL_TAB,IP_RW,60,IPS_IDLE);

    // Active Devices
    IUFillText(&ActiveDeviceT[0],"ACTIVE_GPS","GPS","GPS Simulator");
    IUFillTextVector(&ActiveDeviceTP,ActiveDeviceT,1,getDeviceName(),"ACTIVE_DEVICES","Snoop devices",OPTIONS_TAB,IP_RW,60,IPS_IDLE);

    IDSnoopDevice(ActiveDeviceT[0].text,"GEOGRAPHIC_COORD");

    return true;
}

bool INDI::Weather::updateProperties()
{
    INDI::DefaultDevice::updateProperties();

    if (isConnected())
    {
        updateTimerID = -1;        

        if (critialParametersL)
            defineLight(&critialParametersLP);

        defineNumber(&UpdatePeriodNP);

        defineSwitch(&RefreshSP);

        if (ParametersN)
            defineNumber(&ParametersNP);

        if (ParametersRangeNP)
        {
            for (int i=0; i < nRanges; i++)
                defineNumber(&ParametersRangeNP[i]);
        }


        defineNumber(&LocationNP);
        defineText(&ActiveDeviceTP);

        DEBUG(INDI::Logger::DBG_SESSION, "Weather update is in progress...");
        TimerHit();

    }
    else
    {
        if (critialParametersL)
            deleteProperty(critialParametersLP.name);

        deleteProperty(UpdatePeriodNP.name);

        deleteProperty(RefreshSP.name);

        if (ParametersN)
            deleteProperty(ParametersNP.name);

        if (ParametersRangeNP)
        {
            for (int i=0; i < nRanges; i++)
                deleteProperty(ParametersRangeNP[i].name);
        }

        deleteProperty(LocationNP.name);

        deleteProperty(ActiveDeviceTP.name);
    }

    return true;
}

bool INDI::Weather::ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if(strcmp(dev,getDeviceName())==0)
    {
        if (!strcmp(name, RefreshSP.name))
        {
            RefreshS[0].s = ISS_OFF;
            RefreshSP.s = IPS_OK;
            IDSetSwitch(&RefreshSP, NULL);

            TimerHit();
        }
    }

    return INDI::DefaultDevice::ISNewSwitch(dev, name, states, names, n);
}

bool INDI::Weather::ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n)
{
    //  first check if it's for our device
    if(strcmp(dev,getDeviceName())==0)
    {
        if(strcmp(name,"GEOGRAPHIC_COORD")==0)
        {
            int latindex = IUFindIndex("LAT", names, n);
            int longindex= IUFindIndex("LONG", names, n);
            int elevationindex = IUFindIndex("ELEV", names, n);

            if (latindex == -1 || longindex==-1 || elevationindex == -1)
            {
                LocationNP.s=IPS_ALERT;
                IDSetNumber(&LocationNP, "Location data missing or corrupted.");
            }

            double targetLat  = values[latindex];
            double targetLong = values[longindex];
            double targetElev = values[elevationindex];

            return processLocationInfo(targetLat, targetLong, targetElev);

        }

        // Update period
        if(strcmp(name,"WEATHER_UPDATE")==0)
        {
            IUUpdateNumber(&UpdatePeriodNP, values, names, n);

            UpdatePeriodNP.s = IPS_OK;
            IDSetNumber(&UpdatePeriodNP, NULL);

            if (UpdatePeriodN[0].value == 0)
                DEBUG(INDI::Logger::DBG_SESSION, "Periodic updates are disabled.");
            else
            {
                if (updateTimerID > 0)                
                    RemoveTimer(updateTimerID);

                updateTimerID = SetTimer(UpdatePeriodN[0].value*1000);
            }

            return true;
        }

        for (int i=0; i < nRanges; i++)
        {
            if (!strcmp(name, ParametersRangeNP[i].name))
            {
                IUUpdateNumber(&ParametersRangeNP[i], values, names, n);

                ParametersN[i].min = ParametersRangeNP[i].np[0].value;
                ParametersN[i].max = ParametersRangeNP[i].np[1].value;
                *( (double *) ParametersN[i].aux0) = ParametersRangeNP[i].np[2].value;
                *( (double *) ParametersN[i].aux1) = ParametersRangeNP[i].np[3].value;

                updateWeatherState();

                ParametersRangeNP[i].s = IPS_OK;
                IDSetNumber(&ParametersRangeNP[i], NULL);                

                return true;
            }
        }
    }

    return DefaultDevice::ISNewNumber(dev,name,values,names,n);
}


bool INDI::Weather::ISSnoopDevice(XMLEle *root)
{

    XMLEle *ep=NULL;
    const char *propName = findXMLAttValu(root, "name");

    if (isConnected())
    {
        if (!strcmp(propName, "GEOGRAPHIC_COORD"))
        {
            // Only accept IPS_OK state
            if (strcmp(findXMLAttValu(root, "state"), "Ok"))
                return false;

            double longitude=-1, latitude=-1, elevation=-1;

            for (ep = nextXMLEle(root, 1) ; ep != NULL ; ep = nextXMLEle(root, 0))
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

    return INDI::DefaultDevice::ISSnoopDevice(root);
}

void INDI::Weather::TimerHit()
{
    if (isConnected() == false)
        return;

    if (updateTimerID > 0)
        RemoveTimer(updateTimerID);

    IPState state = updateWeather();

    switch (state)
    {
        // Ok
        case IPS_OK:

        updateWeatherState();
        ParametersNP.s = state;
        IDSetNumber(&ParametersNP, NULL);

        // If update period is set, then set up the timer
        if (UpdatePeriodN[0].value > 0)
            updateTimerID = SetTimer( (int) (UpdatePeriodN[0].value * 1000));

        return;

        // Alert
        case IPS_ALERT:
        ParametersNP.s = state;
        IDSetNumber(&ParametersNP, NULL);
        return;

        // Weather update is in progress
        default:
            break;
    }

    updateTimerID = SetTimer(POLLMS);
}

IPState INDI::Weather::updateWeather()
{
    DEBUG(INDI::Logger::DBG_ERROR, "updateWeather() must be implemented in Weather device child class to update GEOGRAPHIC_COORD properties.");
    return IPS_ALERT;
}

bool INDI::Weather::updateLocation(double latitude, double longitude, double elevation)
{
    INDI_UNUSED(latitude);
    INDI_UNUSED(longitude);
    INDI_UNUSED(elevation);

    return true;
}

bool INDI::Weather::processLocationInfo(double latitude, double longitude, double elevation)
{
    if (updateLocation(latitude, longitude, elevation))
    {
        LocationNP.s=IPS_OK;
        LocationN[LOCATION_LATITUDE].value  = latitude;
        LocationN[LOCATION_LONGITUDE].value = longitude;
        LocationN[LOCATION_ELEVATION].value = elevation;
        //  Update client display
        IDSetNumber(&LocationNP,NULL);

        return true;
    }
    else
    {
        LocationNP.s=IPS_ALERT;
        //  Update client display
        IDSetNumber(&LocationNP,NULL);
        return false;
    }
}

INumber * INDI::Weather::addParameter(std::string name, double minimumOK, double maximumOK, double minimumWarning, double maximumWarning)
{
    DEBUGF(INDI::Logger::DBG_DEBUG, "Parameter %s is added. Ok (%g,%g) Warn (%g,%g)", name.c_str(), minimumOK, maximumOK, minimumWarning, maximumWarning);

    ParametersN = (ParametersN == NULL) ? (INumber *) malloc(sizeof(INumber)) : (INumber *) realloc(ParametersN, (ParametersNP.nnp+1) * sizeof(INumber));

    double *minWarn = (double *) malloc(sizeof(double));
    double *maxWarn = (double *) malloc(sizeof(double));

    *minWarn = minimumWarning;
    *maxWarn = maximumWarning;

    IUFillNumber(&ParametersN[ParametersNP.nnp], name.c_str(), name.c_str(), "%4.2f", minimumOK, maximumOK, 0, 0);

    ParametersN[ParametersNP.nnp].aux0 = minWarn;
    ParametersN[ParametersNP.nnp].aux1 = maxWarn;

    ParametersNP.np = ParametersN;

    //createParameterRange(name);

    return &ParametersN[ParametersNP.nnp++];
}

ILight  * INDI::Weather::setCriticalParameter(std::string param)
{
    for (int i=0; i < ParametersNP.nnp; i++)
    {
        if (!strcmp(ParametersN[i].name, param.c_str()))
        {
            critialParametersL = (critialParametersL == NULL) ? (ILight*) malloc(sizeof(ILight)) : (ILight *) realloc(critialParametersL, (critialParametersLP.nlp+1) * sizeof(ILight));

            IUFillLight(&critialParametersL[critialParametersLP.nlp], param.c_str(), param.c_str(), IPS_IDLE);

            critialParametersLP.lp = critialParametersL;

            return &critialParametersL[critialParametersLP.nlp++];
        }
    }

    DEBUGF(INDI::Logger::DBG_WARNING, "Unable to find parameter %s in list of existing parameters!", param.c_str());
    return NULL;

}

void INDI::Weather::updateWeatherState()
{
    if (critialParametersL == NULL)
        return;

    critialParametersLP.s = IPS_IDLE;

    for (int i=0; i < critialParametersLP.nlp; i++)
    {
        for (int j=0; j < ParametersNP.nnp; j++)
        {
            if (!strcmp(critialParametersL[i].name, ParametersN[j].name))
            {
                double minWarn = *(static_cast<double *>(ParametersN[j].aux0));
                double maxWarn = *(static_cast<double *>(ParametersN[j].aux1));

                if ( (ParametersN[j].value >= ParametersN[j].min) && (ParametersN[i].value <= ParametersN[j].max) )
                    critialParametersL[i].s = IPS_OK;
                else if ( (ParametersN[j].value >= minWarn) && (ParametersN[j].value <= maxWarn) )
                {
                    critialParametersL[i].s = IPS_BUSY;
                    DEBUGF(INDI::Logger::DBG_WARNING, "Warning: Parameter %s value (%g) is in the warning zone!", ParametersN[j].name, ParametersN[j].value);
                }
                else
                {
                    critialParametersL[i].s = IPS_ALERT;
                    DEBUGF(INDI::Logger::DBG_WARNING, "Caution: Parameter %s value (%g) is in the danger zone!", ParametersN[j].name, ParametersN[j].value);
                }
                break;
            }
        }

        // The overall state is the worst individual state.
        if (critialParametersL[i].s > critialParametersLP.s)
            critialParametersLP.s = critialParametersL[i].s;                
    }

    IDSetLight(&critialParametersLP, NULL);
}

void INDI::Weather::generateParameterRanges()
{
    for (int i=0; i < ParametersNP.nnp; i++)
        createParameterRange(ParametersN[i].name);
}

void INDI::Weather::createParameterRange(std::string param)
{
    ParametersRangeNP = (ParametersRangeNP == NULL) ? (INumberVectorProperty *) malloc(sizeof(INumberVectorProperty)) : (INumberVectorProperty *) realloc(ParametersRangeNP, (nRanges+1) * sizeof(INumberVectorProperty));

    INumber *rangesN = (INumber *) malloc(sizeof(INumber)*4);

    IUFillNumber(&rangesN[0], "MIN_OK", "Min OK", "%4.2f", -1e6, 1e6, 0, ParametersN[nRanges].min);
    IUFillNumber(&rangesN[1], "MAX_OK", "Max OK", "%4.2f", -1e6, 1e6, 0, ParametersN[nRanges].max);
    IUFillNumber(&rangesN[2], "MIN_WARN", "Min Warn", "%4.2f", -1e6, 1e6, 0, *( (double *) ParametersN[nRanges].aux0));
    IUFillNumber(&rangesN[3], "MAX_WARN", "Max Warn", "%4.2f", -1e6, 1e6, 0, *( (double *) ParametersN[nRanges].aux1));

    char propName[MAXINDINAME];
    snprintf(propName, MAXINDINAME, "%s Range", param.c_str());

    IUFillNumberVector(&ParametersRangeNP[nRanges], rangesN, 4, getDeviceName(), propName, propName, PARAMETERS_TAB, IP_RW, 60, IPS_IDLE);

    nRanges++;

}

bool INDI::Weather::saveConfigItems(FILE *fp)
{
    INDI::DefaultDevice::saveConfigItems(fp);

    for (int i=0; i < nRanges; i++)
        IUSaveConfigNumber(fp, &ParametersRangeNP[i]);

    return true;
}
