/*******************************************************************************
  Copyright(c) 2015 Jasem Mutlaq. All rights reserved.
  Copyright(c) 2015 Paweł T. Jochym  <jochym AT gmail DOT com>
..Copyright(c) 2014 Radek Kaczorek  <rkaczorek AT gmail DOT com>
  Based on Simple GPS Simulator by Jasem Mutlaq

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

#include "gps_driver.h"

#include <libgpsmm.h>

using namespace std;

// We declare an auto pointer to GPD.
std::unique_ptr<GPSD> gpsd(new GPSD());

void ISGetProperties(const char *dev)
{
    gpsd->ISGetProperties(dev);
}

void ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int num)
{
    gpsd->ISNewSwitch(dev, name, states, names, num);
}

void ISNewText(	const char *dev, const char *name, char *texts[], char *names[], int num)
{
    gpsd->ISNewText(dev, name, texts, names, num);
}

void ISNewNumber(const char *dev, const char *name, double values[], char *names[], int num)
{
    gpsd->ISNewNumber(dev, name, values, names, num);
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
    INDI_UNUSED(root);
}

GPSD::GPSD()
{
   setVersion(0,2);
   gps=NULL;
}

GPSD::~GPSD()
{

}

const char * GPSD::getDefaultName()
{
    return (char *)"GPSD driver";
}

bool GPSD::Connect()
{
    if (gps==NULL) {
        gps=new gpsmm("localhost", DEFAULT_GPSD_PORT);
    } 
    if (gps->stream(WATCH_ENABLE|WATCH_JSON) == NULL) {
        IDMessage(getDeviceName(), "No GPSD running.");
        return false;
    }
    return true;
}

bool GPSD::Disconnect()
{
    delete gps;
    gps=NULL;
    IDMessage(getDeviceName(), "GPS disconnected successfully.");
    return true;
}

bool GPSD::initProperties()
{
    // We init parent properties first
    INDI::GPS::initProperties();

    IUFillText(&GPSstatusT[0],"GPS_FIX","Fix Mode",NULL);
    IUFillTextVector(&GPSstatusTP,GPSstatusT,1,getDeviceName(),
        "GPS_STATUS","GPS Status",MAIN_CONTROL_TAB,IP_RO,60,IPS_IDLE);

    IUFillNumber(&PolarisN[0],"HA","Polaris Hour Angle","%010.6m",0,24,0,0.0);
    IUFillNumberVector(&PolarisNP,PolarisN,1,getDeviceName(),
        "POLARIS","Polaris",MAIN_CONTROL_TAB,IP_RO,60,IPS_IDLE);

    IUFillSwitch(&RefreshS[0], "REFRESH", "Update", ISS_OFF);
    IUFillSwitchVector(&RefreshSP, RefreshS, 1, getDeviceName(), 
        "GPS_REFRESH","GPS",MAIN_CONTROL_TAB,IP_RW,ISR_ATMOST1,0,IPS_IDLE);

    return true;
}

bool GPSD::updateProperties()
{
    // Call parent update properties first
    INDI::GPS::updateProperties();

    if (isConnected()) {
        deleteProperty(RefreshSP.name);
        defineText(&GPSstatusTP);
        defineNumber(&PolarisNP);
        defineSwitch(&RefreshSP);
    } else {
        // We're disconnected
        deleteProperty(GPSstatusTP.name);
        deleteProperty(PolarisNP.name);
    }
    return true;
}

IPState GPSD::updateGPS()
{
    // Indicate gps refresh in progress
    TimeTP.s = IPS_BUSY;
    IDSetText(&TimeTP, NULL);
    LocationNP.s = IPS_BUSY;
    IDSetNumber(&LocationNP, NULL);

    GPSstatusTP.s = IPS_BUSY;
    IDSetText(&GPSstatusTP, NULL);
    PolarisNP.s = IPS_BUSY;
    IDSetNumber(&PolarisNP, NULL);
    RefreshSP.s = IPS_BUSY;
    IDSetSwitch(&RefreshSP, NULL);

    struct gps_data_t* gpsData;

    TimeTP.s = IPS_OK;

    if (!gps->waiting(100000)) {
        IDMessage(getDeviceName(), "Waiting for gps data...");
        return IPS_BUSY;
    }

    if ((gpsData = gps->read()) == NULL) {
        IDMessage(getDeviceName(), "GPSD read error.");
        IDSetText(&GPSstatusTP, NULL);
        return IPS_ALERT;
    }
    
    if (gpsData->status != STATUS_FIX) {
        // cerr << "No fix.\n";
        return IPS_BUSY;
    }

    // detect gps fix
    if (GPSstatusT[0].text == "NO FIX" && gpsData->fix.mode > 2)
        IDMessage(getDeviceName(), "GPS fix obtained.");
    
    // update gps fix status
    if ( gpsData->fix.mode >= 3 ) {
        GPSstatusT[0].text = (char*) "3D FIX";
        GPSstatusTP.s = IPS_OK;
        IDSetText(&GPSstatusTP, NULL);
    } else if ( gpsData->fix.mode == 2 ) {
        GPSstatusT[0].text = (char*) "2D FIX";
        GPSstatusTP.s = IPS_BUSY;
        IDSetText(&GPSstatusTP, NULL);
        return IPS_BUSY;
    } else {
        GPSstatusT[0].text = (char*) "NO FIX";
        GPSstatusTP.s = IPS_BUSY;
        IDSetText(&GPSstatusTP, NULL);
        return IPS_BUSY;
    }

    // Update time. We are using system time
    // assuming the system is synced with the gps 
    // by gpsd using chronyd or ntpd.
    static char ts[32];
    struct tm *utc, *local;

    // To get time from gps fix we can use
    // raw_time = gpsData->fix.time;
    time_t raw_time;
    time(&raw_time);

    utc  = gmtime(&raw_time);    
    strftime (ts, sizeof(ts), "%Y-%m-%dT%H:%M:%S", utc);
    IUSaveText(&TimeT[0], ts);

    local= localtime(&raw_time);
    snprintf(ts, sizeof(ts), "%4.2f", (local->tm_gmtoff/3600.0));
    IUSaveText(&TimeT[1], ts);

    // get utc_offset
    double offset = local->tm_gmtoff / 3600;

    // adjust offset for DST
    if (local->tm_isdst)
        offset -= 1;

    // convert offset to string
    sprintf(ts,"%0.0f", offset);
    IUSaveText(&TimeT[1], ts);

    TimeTP.s = IPS_OK;

    // update gps location
    // fprintf(stderr,"Fix: %d time: %lf\n", data->fix.mode, data->fix.time);
            
    LocationN[LOCATION_LATITUDE].value = gpsData->fix.latitude;
    LocationN[LOCATION_LONGITUDE].value = gpsData->fix.longitude;
    LocationN[LOCATION_ELEVATION].value = gpsData->fix.altitude;
    LocationNP.s = IPS_OK;

    // calculate Polaris HA
    double jd, lst, polarislsrt;

    // polaris location - RA 02h 31m 47s DEC 89° 15' 50'' (J2000)
    jd = ln_get_julian_from_timet(&raw_time);
    lst=ln_get_apparent_sidereal_time(jd);

    // Local Hour Angle = Local Sidereal Time - Polaris Right Ascension
    polarislsrt = lst - 2.529722222 + (gpsData->fix.longitude / 15.0);
    PolarisN[0].value = polarislsrt;

    GPSstatusTP.s = IPS_OK;
    IDSetText(&GPSstatusTP, NULL);
    PolarisNP.s = IPS_OK;
    IDSetNumber(&PolarisNP, NULL);
    RefreshSP.s = IPS_OK;
    IDSetSwitch(&RefreshSP, NULL);

    return IPS_OK;
}

