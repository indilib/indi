/*******************************************************************************
 Copyright(c) 2010, 2017 Ilia Platone, Jasem Mutlaq. All rights reserved.


 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Library General Public
 License version 2 as published by the Free Software Foundation.

 This library is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 Library General Public License for more details.

 You should have received a copy of the GNU Library General Public License
 along with this library; see the file COPYING.LIB.  If not, write to
 the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 Boston, MA 02110-1301, USA.
*******************************************************************************/

#include "defaultdevice.h"
#include "indisensorinterface.h"
#include "connectionplugins/connectionserial.h"
#include "connectionplugins/connectiontcp.h"

#include "indicom.h"
#include "libastro.h"
#include "stream/streammanager.h"
#include "locale_compat.h"

#include <fitsio.h>

#include <libnova/julian_day.h>
#include <libnova/ln_types.h>
#include <libnova/precession.h>

#include <regex>

#include <dirent.h>
#include <cerrno>
#include <locale.h>
#include <cstdlib>
#include <zlib.h>
#include <sys/stat.h>

// Create dir recursively
static int _det_mkdir(const char *dir, mode_t mode)
{
    char tmp[PATH_MAX];
    char *p = nullptr;
    size_t len;

    snprintf(tmp, sizeof(tmp), "%s", dir);
    len = strlen(tmp);
    if (tmp[len - 1] == '/')
        tmp[len - 1] = 0;
    for (p = tmp + 1; *p; p++)
        if (*p == '/')
        {
            *p = 0;
            if (mkdir(tmp, mode) == -1 && errno != EEXIST)
                return -1;
            *p = '/';
        }
    if (mkdir(tmp, mode) == -1 && errno != EEXIST)
        return -1;

    return 0;
}

namespace INDI
{

SensorInterface::SensorInterface()
{
    //ctor
    capability = 0;

    InIntegration              = false;

    AutoLoop         = false;
    SendIntegration        = false;
    ShowMarker       = false;

    IntegrationTime       = 0.0;

    RA              = -1000;
    Dec             = -1000;
    MPSAS           = -1000;
    Lat             = -1000;
    Lon             = -1000;
    El              = -1000;
    primaryAperture = primaryFocalLength - 1;

    Buffer     = static_cast<uint8_t *>(malloc(sizeof(uint8_t))); // Seed for realloc
    BufferSize = 0;
    NAxis       = 2;

    BPS         = 8;

    strncpy(integrationExtention, "raw", MAXINDIBLOBFMT);
}

SensorInterface::~SensorInterface()
{
    free(Buffer);
    BufferSize = 0;
    Buffer = nullptr;
}


bool SensorInterface::updateProperties()
{
    //IDLog("Sensor UpdateProperties isConnected returns %d %d\n",isConnected(),Connected);
    if (isConnected())
    {
        defineNumber(&FramedIntegrationNP);

        if (CanAbort())
            defineSwitch(&AbortIntegrationSP);

        defineText(&FITSHeaderTP);

        if (HasCooler())
            defineNumber(&TemperatureNP);

        defineBLOB(&FitsBP);

        defineSwitch(&TelescopeTypeSP);

        defineSwitch(&UploadSP);

        if (UploadSettingsT[UPLOAD_DIR].text == nullptr)
            IUSaveText(&UploadSettingsT[UPLOAD_DIR], getenv("HOME"));
        defineText(&UploadSettingsTP);
    }
    else
    {
        deleteProperty(FramedIntegrationNP.name);
        if (CanAbort())
            deleteProperty(AbortIntegrationSP.name);
        deleteProperty(FitsBP.name);

        deleteProperty(FITSHeaderTP.name);

        if (HasCooler())
            deleteProperty(TemperatureNP.name);

        deleteProperty(TelescopeTypeSP.name);

        deleteProperty(UploadSP.name);
        deleteProperty(UploadSettingsTP.name);
    }

    if (HasStreaming())
        Streamer->updateProperties();

    if (HasDSP())
        DSP->updateProperties();
    return true;
}

void SensorInterface::processProperties(const char *dev)
{
    INDI::DefaultDevice::ISGetProperties(dev);

    defineText(&ActiveDeviceTP);
    loadConfig(true, "ACTIVE_DEVICES");

    if (HasStreaming())
        Streamer->ISGetProperties(dev);

    if (HasDSP())
        DSP->ISGetProperties(dev);
}

bool SensorInterface::processSnoopDevice(XMLEle *root)
{
    if (!IUSnoopNumber(root, &EqNP))
    {
        RA = EqNP.np[0].value;
        Dec = EqNP.np[1].value;
        //IDLog("Snooped RA %4.2f  Dec %4.2f\n", RA, Dec);
    }
    if (!IUSnoopNumber(root, &LocationNP))
    {
        Lat = LocationNP.np[0].value;
        Lon = LocationNP.np[1].value;
        El = LocationNP.np[2].value;
        //IDLog("Snooped Lat %4.2f  Lon %4.2f  El %4.2f\n", RA, Dec, El);
    }
    if (!IUSnoopNumber(root, &ScopeParametersNP))
    {
        primaryAperture = ScopeParametersNP.np[0].value;
        primaryFocalLength = ScopeParametersNP.np[1].value;
        //IDLog("Snooped primaryAperture %4.2f  primaryFocalLength %4.2f\n", primaryAperture, primaryFocalLength);
    }

    return INDI::DefaultDevice::ISSnoopDevice(root);
}

bool SensorInterface::processText(const char *dev, const char *name, char *texts[], char *names[], int n)
{
    //  first check if it's for our device
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        //  This is for our device
        //  Now lets see if it's something we process here
        if (!strcmp(name, ActiveDeviceTP.name))
        {
            ActiveDeviceTP.s = IPS_OK;
            IUUpdateText(&ActiveDeviceTP, texts, names, n);
            IDSetText(&ActiveDeviceTP, nullptr);

            // Update the property name!
            strncpy(EqNP.device, ActiveDeviceT[0].text, MAXINDIDEVICE);
            strncpy(LocationNP.device, ActiveDeviceT[0].text, MAXINDIDEVICE);
            strncpy(ScopeParametersNP.device, ActiveDeviceT[0].text, MAXINDIDEVICE);

            IDSnoopDevice(ActiveDeviceT[0].text, "EQUATORIAL_EOD_COORD");
            IDSnoopDevice(ActiveDeviceT[0].text, "TELESCOPE_INFO");
            IDSnoopDevice(ActiveDeviceT[1].text, "GEOGRAPHIC_COORD");

            // Tell children active devices was updated.
            activeDevicesUpdated();

            //  We processed this one, so, tell the world we did it
            return true;
        }

        if (!strcmp(name, FITSHeaderTP.name))
        {
            IUUpdateText(&FITSHeaderTP, texts, names, n);
            FITSHeaderTP.s = IPS_OK;
            IDSetText(&FITSHeaderTP, nullptr);
            return true;
        }

        if (!strcmp(name, UploadSettingsTP.name))
        {
            IUUpdateText(&UploadSettingsTP, texts, names, n);
            UploadSettingsTP.s = IPS_OK;
            IDSetText(&UploadSettingsTP, nullptr);
            return true;
        }
    }

    if (HasStreaming())
        Streamer->ISNewText(dev, name, texts, names, n);

    if (HasDSP())
        DSP->ISNewText(dev, name, texts, names, n);

    return INDI::DefaultDevice::ISNewText(dev, name, texts, names, n);
}

bool SensorInterface::processNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    //  first check if it's for our device
    //IDLog("SensorInterface::processNumber %s\n",name);
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        if (!strcmp(name, "SENSOR_INTEGRATION"))
        {
            if ((values[0] < FramedIntegrationN[0].min || values[0] > FramedIntegrationN[0].max))
            {
                DEBUGF(Logger::DBG_ERROR, "Requested integration value (%g) seconds out of bounds [%g,%g].",
                       values[0], FramedIntegrationN[0].min, FramedIntegrationN[0].max);
                FramedIntegrationNP.s = IPS_ALERT;
                IDSetNumber(&FramedIntegrationNP, nullptr);
                return false;
            }

            FramedIntegrationN[0].value = IntegrationTime = values[0];

            if (FramedIntegrationNP.s == IPS_BUSY)
            {
                if (CanAbort() && AbortIntegration() == false)
                    DEBUG(Logger::DBG_WARNING, "Warning: Aborting integration failed.");
            }

            if (StartIntegration(IntegrationTime))
                FramedIntegrationNP.s = IPS_BUSY;
            else
                FramedIntegrationNP.s = IPS_ALERT;
            IDSetNumber(&FramedIntegrationNP, nullptr);
            return true;
        }

        // Sensor TEMPERATURE:
        if (!strcmp(name, TemperatureNP.name))
        {
            if (values[0] < TemperatureN[0].min || values[0] > TemperatureN[0].max)
            {
                TemperatureNP.s = IPS_ALERT;
                DEBUGF(Logger::DBG_ERROR, "Error: Bad temperature value! Range is [%.1f, %.1f] [C].",
                       TemperatureN[0].min, TemperatureN[0].max);
                IDSetNumber(&TemperatureNP, nullptr);
                return false;
            }

            int rc = SetTemperature(values[0]);

            if (rc == 0)
                TemperatureNP.s = IPS_BUSY;
            else if (rc == 1)
                TemperatureNP.s = IPS_OK;
            else
                TemperatureNP.s = IPS_ALERT;

            IDSetNumber(&TemperatureNP, nullptr);
            return true;
        }
    }

    if (HasStreaming())
        Streamer->ISNewNumber(dev, name, values, names, n);

    if (HasDSP())
        DSP->ISNewNumber(dev, name, values, names, n);

    return INDI::DefaultDevice::ISNewNumber(dev, name, values, names, n);
}

bool SensorInterface::processSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        if (!strcmp(name, UploadSP.name))
        {
            int prevMode = IUFindOnSwitchIndex(&UploadSP);
            IUUpdateSwitch(&UploadSP, states, names, n);
            UploadSP.s = IPS_OK;
            IDSetSwitch(&UploadSP, nullptr);

            if (UploadS[0].s == ISS_ON)
            {
                DEBUG(Logger::DBG_SESSION, "Upload settings set to client only.");
                if (prevMode != 0)
                    deleteProperty(FileNameTP.name);
            }
            else if (UploadS[1].s == ISS_ON)
            {
                DEBUG(Logger::DBG_SESSION, "Upload settings set to local only.");
                defineText(&FileNameTP);
            }
            else
            {
                DEBUG(Logger::DBG_SESSION, "Upload settings set to client and local.");
                defineText(&FileNameTP);
            }
            return true;
        }

        if (!strcmp(name, TelescopeTypeSP.name))
        {
            IUUpdateSwitch(&TelescopeTypeSP, states, names, n);
            TelescopeTypeSP.s = IPS_OK;
            IDSetSwitch(&TelescopeTypeSP, nullptr);
            return true;
        }

        // Primary Device Abort Expsoure
        if (strcmp(name, AbortIntegrationSP.name) == 0)
        {
            IUResetSwitch(&AbortIntegrationSP);

            if (AbortIntegration())
            {
                AbortIntegrationSP.s       = IPS_OK;
                FramedIntegrationNP.s       = IPS_IDLE;
                FramedIntegrationN[0].value = 0;
            }
            else
            {
                AbortIntegrationSP.s = IPS_ALERT;
                FramedIntegrationNP.s = IPS_ALERT;
            }

            IDSetSwitch(&AbortIntegrationSP, nullptr);
            IDSetNumber(&FramedIntegrationNP, nullptr);

            return true;
        }
    }

    if (HasStreaming())
        Streamer->ISNewSwitch(dev, name, states, names, n);

    if (HasDSP())
        DSP->ISNewSwitch(dev, name, states, names, n);

    return INDI::DefaultDevice::ISNewSwitch(dev, name, states, names, n);
}

bool SensorInterface::processBLOB(const char *dev, const char *name, int sizes[], int blobsizes[], char *blobs[],
                                  char *formats[], char *names[], int n)
{
    if (HasDSP())
        DSP->ISNewBLOB(dev, name, sizes, blobsizes, blobs, formats, names, n);

    return INDI::DefaultDevice::ISNewBLOB(dev, name, sizes, blobsizes, blobs, formats, names, n);
}

bool SensorInterface::initProperties()
{
    INDI::DefaultDevice::initProperties(); //  let the base class flesh in what it wants

    // Sensor Temperature
    IUFillNumber(&TemperatureN[0], "SENSOR_TEMPERATURE_VALUE", "Temperature (C)", "%5.2f", -50.0, 50.0, 0., 0.);
    IUFillNumberVector(&TemperatureNP, TemperatureN, 1, getDeviceName(), "SENSOR_TEMPERATURE", "Temperature",
                       MAIN_CONTROL_TAB, IP_RW, 60, IPS_IDLE);

    /**********************************************/
    /**************** Primary Device ****************/
    /**********************************************/

    // Sensor Integration
    IUFillNumber(&FramedIntegrationN[0], "SENSOR_INTEGRATION_VALUE", "Time (s)", "%5.2f", 0.01, 3600, 1.0, 1.0);
    IUFillNumberVector(&FramedIntegrationNP, FramedIntegrationN, 1, getDeviceName(), "SENSOR_INTEGRATION",
                       "Integration", MAIN_CONTROL_TAB, IP_RW, 60, IPS_IDLE);

    // Sensor Abort
    if(CanAbort())
    {
        IUFillSwitch(&AbortIntegrationS[0], "ABORT", "Abort", ISS_OFF);
        IUFillSwitchVector(&AbortIntegrationSP, AbortIntegrationS, 1, getDeviceName(), "SENSOR_ABORT_INTEGRATION",
                           "Integration Abort", MAIN_CONTROL_TAB, IP_RW, ISR_ATMOST1, 60, IPS_IDLE);
    }


    /**********************************************/
    /************** Upload Settings ***************/
    /**********************************************/
    // Upload Data
    IUFillBLOB(&FitsB, "DATA", "Sensor Data Blob", "");

    IUFillBLOBVector(&FitsBP, &FitsB, 1, getDeviceName(), "SENSOR", "Integration Data", MAIN_CONTROL_TAB,
                     IP_RO, 60, IPS_IDLE);

    // Upload Mode
    IUFillSwitch(&UploadS[0], "UPLOAD_CLIENT", "Client", ISS_ON);
    IUFillSwitch(&UploadS[1], "UPLOAD_LOCAL", "Local", ISS_OFF);
    IUFillSwitch(&UploadS[2], "UPLOAD_BOTH", "Both", ISS_OFF);
    IUFillSwitchVector(&UploadSP, UploadS, 3, getDeviceName(), "UPLOAD_MODE", "Upload", OPTIONS_TAB, IP_RW, ISR_1OFMANY,
                       0, IPS_IDLE);

    // Upload Settings
    IUFillText(&UploadSettingsT[UPLOAD_DIR], "UPLOAD_DIR", "Dir", "");
    IUFillText(&UploadSettingsT[UPLOAD_PREFIX], "UPLOAD_PREFIX", "Prefix", "INTEGRATION_XXX");
    IUFillTextVector(&UploadSettingsTP, UploadSettingsT, 2, getDeviceName(), "UPLOAD_SETTINGS", "Upload Settings",
                     OPTIONS_TAB, IP_RW, 60, IPS_IDLE);

    // Upload File Path
    IUFillText(&FileNameT[0], "FILE_PATH", "Path", "");
    IUFillTextVector(&FileNameTP, FileNameT, 1, getDeviceName(), "SENSOR_FILE_PATH", "Filename", OPTIONS_TAB, IP_RO, 60,
                     IPS_IDLE);

    /**********************************************/
    /****************** FITS Header****************/
    /**********************************************/

    IUFillText(&FITSHeaderT[FITS_OBSERVER], "FITS_OBSERVER", "Observer", "Unknown");
    IUFillText(&FITSHeaderT[FITS_OBJECT], "FITS_OBJECT", "Object", "Unknown");
    IUFillTextVector(&FITSHeaderTP, FITSHeaderT, 2, getDeviceName(), "FITS_HEADER", "FITS Header", INFO_TAB, IP_RW, 60,
                     IPS_IDLE);

    /**********************************************/
    /**************** Snooping ********************/
    /**********************************************/

    // Snooped Devices
    IUFillText(&ActiveDeviceT[0], "ACTIVE_TELESCOPE", "Telescope", "Telescope Simulator");
    IUFillText(&ActiveDeviceT[1], "ACTIVE_GPS", "GPS", "GPS Simulator");
    IUFillTextVector(&ActiveDeviceTP, ActiveDeviceT, 2, getDeviceName(), "ACTIVE_DEVICES", "Snoop devices", OPTIONS_TAB,
                     IP_RW, 60, IPS_IDLE);

    // Snoop properties of interest
    IUFillNumber(&EqN[0], "RA", "RA (hh:mm:ss)", "%010.6m", 0, 24, 0, 0);
    IUFillNumber(&EqN[1], "DEC", "DEC (dd:mm:ss)", "%010.6m", -90, 90, 0, 0);
    IUFillNumberVector(&EqNP, EqN, 2, getDeviceName(), "EQUATORIAL_EOD_COORD", "Eq. Coordinates", MAIN_CONTROL_TAB,
                       IP_RW, 60, IPS_IDLE);

    IUFillNumber(&LocationN[0], "LAT", "Lat (dd:mm:ss)", "%010.6m", -90, 90, 0, 0.0);
    IUFillNumber(&LocationN[1], "LONG", "Lon (dd:mm:ss)", "%010.6m", 0, 360, 0, 0.0);
    IUFillNumber(&LocationN[2], "ELEV", "Elevation (m)", "%g", -200, 10000, 0, 0);
    IUFillNumberVector(&LocationNP, LocationN, 3, getDeviceName(), "GEOGRAPHIC_COORD", "Location", MAIN_CONTROL_TAB,
                       IP_RO, 60, IPS_IDLE);

    IUFillNumber(&ScopeParametersN[0], "TELESCOPE_APERTURE", "Aperture (mm)", "%g", 10, 5000, 0, 0.0);
    IUFillNumber(&ScopeParametersN[1], "TELESCOPE_FOCAL_LENGTH", "Focal Length (mm)", "%g", 10, 10000, 0, 0.0);
    IUFillNumber(&ScopeParametersN[2], "GUIDER_APERTURE", "Guider Aperture (mm)", "%g", 10, 5000, 0, 0.0);
    IUFillNumber(&ScopeParametersN[3], "GUIDER_FOCAL_LENGTH", "Guider Focal Length (mm)", "%g", 10, 10000, 0, 0.0);
    IUFillNumberVector(&ScopeParametersNP, ScopeParametersN, 4, getDeviceName(), "TELESCOPE_INFO", "Scope Properties",
                       OPTIONS_TAB, IP_RW, 60, IPS_OK);

    IDSnoopDevice(ActiveDeviceT[0].text, "EQUATORIAL_EOD_COORD");
    IDSnoopDevice(ActiveDeviceT[0].text, "TELESCOPE_INFO");
    IDSnoopDevice(ActiveDeviceT[1].text, "GEOGRAPHIC_COORD");

    if (sensorConnection & CONNECTION_SERIAL)
    {
        serialConnection = new Connection::Serial(this);
        serialConnection->registerHandshake([&]()
        {
            return callHandshake();
        });
        registerConnection(serialConnection);
    }

    if (sensorConnection & CONNECTION_TCP)
    {
        tcpConnection = new Connection::TCP(this);
        tcpConnection->registerHandshake([&]()
        {
            return callHandshake();
        });

        registerConnection(tcpConnection);
    }

    return true;
}

void SensorInterface::SetCapability(uint32_t cap)
{
    capability = cap;

    setDriverInterface(getDriverInterface());

    HasStreaming();
    HasDSP();
}

void SensorInterface::setMinMaxStep(const char *property, const char *element, double min, double max, double step,
                                    bool sendToClient)
{
    INumberVectorProperty *vp = nullptr;

    if (!strcmp(property, FramedIntegrationNP.name))
    {
        vp = &FramedIntegrationNP;

        INumber *np = IUFindNumber(vp, element);
        if (np)
        {
            np->min  = min;
            np->max  = max;
            np->step = step;

            if (sendToClient)
                IUUpdateMinMax(vp);
        }
    }
}

void SensorInterface::setBufferSize(int nbuf, bool allocMem)
{
    if (nbuf == BufferSize)
        return;

    BufferSize = nbuf;

    // Reset size
    if (HasStreaming())
        Streamer->setSize(BufferSize * 8 / getBPS());

    // DSP
    if (HasDSP())
        DSP->setSizes(1, new int[1] { BufferSize * 8 / getBPS() });

    if (allocMem == false)
        return;

    Buffer = static_cast<uint8_t *>(realloc(Buffer, nbuf * sizeof(uint8_t)));
}

bool SensorInterface::StartIntegration(double duration)
{
    INDI_UNUSED(duration);
    DEBUGF(Logger::DBG_WARNING, "SensorInterface::StartIntegration %4.2f -  Should never get here", duration);
    return false;
}

void SensorInterface::setIntegrationLeft(double duration)
{
    FramedIntegrationN[0].value = duration;

    IDSetNumber(&FramedIntegrationNP, nullptr);
}

void SensorInterface::setIntegrationTime(double duration)
{
    integrationTime = duration;
    // JM 2020-04-28: FIXME
    // This does not compile on MacOS, so commenting now.
    // IP 2020-04-29: trying with some adaptation
    startIntegrationTime = time_ns();
}

const char *SensorInterface::getIntegrationStartTime()
{
    static char ts[32];

    char iso8601[32];
    struct tm *tp;
    time_t t = (time_t)startIntegrationTime;

    tp = gmtime(&t);
    strftime(iso8601, sizeof(iso8601), "%Y-%m-%dT%H:%M:%S", tp);
    return (ts);
}

void SensorInterface::setIntegrationFailed()
{
    FramedIntegrationNP.s = IPS_ALERT;
    IDSetNumber(&FramedIntegrationNP, nullptr);
}

int SensorInterface::getNAxis() const
{
    return NAxis;
}

void SensorInterface::setNAxis(int value)
{
    NAxis = value;
}

void SensorInterface::setIntegrationFileExtension(const char *ext)
{
    strncpy(integrationExtention, ext, MAXINDIBLOBFMT);
}


bool SensorInterface::AbortIntegration()
{
    DEBUG(Logger::DBG_WARNING, "SensorInterface::AbortIntegration -  Should never get here");
    return false;
}

void SensorInterface::addFITSKeywords(fitsfile *fptr, uint8_t* buf, int len)
{
    INDI_UNUSED(buf);
    INDI_UNUSED(len);
    int status = 0;
    char dev_name[32];
    char exp_start[32];
    char timestamp[32];
    double integrationTime;

    char *orig = setlocale(LC_NUMERIC, "C");

    char fitsString[MAXINDIDEVICE];

    // SENSOR
    strncpy(fitsString, getDeviceName(), MAXINDIDEVICE);
    fits_update_key_s(fptr, TSTRING, "INSTRUME", fitsString, "Sensor Name", &status);

    // Telescope
    strncpy(fitsString, ActiveDeviceT[0].text, MAXINDIDEVICE);
    fits_update_key_s(fptr, TSTRING, "TELESCOP", fitsString, "Telescope name", &status);

    // Observer
    strncpy(fitsString, FITSHeaderT[FITS_OBSERVER].text, MAXINDIDEVICE);
    fits_update_key_s(fptr, TSTRING, "OBSERVER", fitsString, "Observer name", &status);

    // Object
    strncpy(fitsString, FITSHeaderT[FITS_OBJECT].text, MAXINDIDEVICE);
    fits_update_key_s(fptr, TSTRING, "OBJECT", fitsString, "Object name", &status);

    integrationTime = getIntegrationTime();

    strncpy(dev_name, getDeviceName(), 32);
    strncpy(exp_start, getIntegrationStartTime(), 32);
    snprintf(timestamp, 32, "%lf", startIntegrationTime);

    fits_update_key_s(fptr, TDOUBLE, "EXPTIME", &(integrationTime), "Total Integration Time (s)", &status);

    if (HasCooler())
        fits_update_key_s(fptr, TDOUBLE, "SENSOR-TEMP", &(TemperatureN[0].value), "PrimarySensorInterface Temperature (Celsius)",
                          &status);

#ifdef WITH_MINMAX
    if (getNAxis() == 2)
    {
        double min_val, max_val;
        getMinMax(&min_val, &max_val, buf, len, getBPS());

        fits_update_key_s(fptr, TDOUBLE, "DATAMIN", &min_val, "Minimum value", &status);
        fits_update_key_s(fptr, TDOUBLE, "DATAMAX", &max_val, "Maximum value", &status);
    }
#endif

    if (primaryFocalLength != -1)
        fits_update_key_s(fptr, TDOUBLE, "FOCALLEN", &primaryFocalLength, "Focal Length (mm)", &status);

    if (MPSAS != -1000)
    {
        fits_update_key_s(fptr, TDOUBLE, "MPSAS", &MPSAS, "Sky Quality (mag per arcsec^2)", &status);
    }

    if (Lat != -1000 && Lon != -1000 && El != -1000)
    {
        char lat_str[MAXINDIFORMAT];
        char lon_str[MAXINDIFORMAT];
        char el_str[MAXINDIFORMAT];
        fs_sexa(lat_str, Lat, 2, 360000);
        fs_sexa(lat_str, Lon, 2, 360000);
        snprintf(el_str, MAXINDIFORMAT, "%lf", El);
        fits_update_key_s(fptr, TSTRING, "LATITUDE", lat_str, "Location Latitude", &status);
        fits_update_key_s(fptr, TSTRING, "LONGITUDE", lon_str, "Location Longitude", &status);
        fits_update_key_s(fptr, TSTRING, "ELEVATION", el_str, "Location Elevation", &status);
    }
    if (RA != -1000 && Dec != -1000)
    {
        ln_equ_posn epochPos { 0, 0 }, J2000Pos { 0, 0 };
        epochPos.ra  = RA * 15.0;
        epochPos.dec = Dec;

        // Convert from JNow to J2000
        //TODO use exp_start instead of julian from system
        //ln_get_equ_prec2(&epochPos, ln_get_julian_from_sys(), JD2000, &J2000Pos);
        LibAstro::ObservedToJ2000(&epochPos, ln_get_julian_from_sys(), &J2000Pos);

        double raJ2000  = J2000Pos.ra / 15.0;
        double decJ2000 = J2000Pos.dec;
        char ra_str[32], de_str[32];

        fs_sexa(ra_str, raJ2000, 2, 360000);
        fs_sexa(de_str, decJ2000, 2, 360000);

        char *raPtr = ra_str, *dePtr = de_str;
        while (*raPtr != '\0')
        {
            if (*raPtr == ':')
                *raPtr = ' ';
            raPtr++;
        }
        while (*dePtr != '\0')
        {
            if (*dePtr == ':')
                *dePtr = ' ';
            dePtr++;
        }

        fits_update_key_s(fptr, TSTRING, "OBJCTRA", ra_str, "Object RA", &status);
        fits_update_key_s(fptr, TSTRING, "OBJCTDEC", de_str, "Object DEC", &status);

        int epoch = 2000;

        //fits_update_key_s(fptr, TINT, "EPOCH", &epoch, "Epoch", &status);
        fits_update_key_s(fptr, TINT, "EQUINOX", &epoch, "Equinox", &status);
    }

    fits_update_key_s(fptr, TSTRING, "TIMESTAMP", timestamp, "Timestamp of start of integration", &status);

    fits_update_key_s(fptr, TSTRING, "DATE-OBS", exp_start, "UTC start date of observation", &status);

    fits_write_comment(fptr, "Generated by INDI", &status);

    setlocale(LC_NUMERIC, orig);
}

void SensorInterface::fits_update_key_s(fitsfile *fptr, int type, std::string name, void *p, std::string explanation,
                                        int *status)
{
    // this function is for removing warnings about deprecated string conversion to char* (from arg 5)
    fits_update_key(fptr, type, name.c_str(), p, const_cast<char *>(explanation.c_str()), status);
}

void* SensorInterface::sendFITS(uint8_t *buf, int len)
{
    bool sendIntegration = (UploadS[0].s == ISS_ON || UploadS[2].s == ISS_ON);
    bool saveIntegration = (UploadS[1].s == ISS_ON || UploadS[2].s == ISS_ON);
    fitsfile *fptr = nullptr;
    void *memptr;
    size_t memsize;
    int img_type  = 0;
    int byte_type = 0;
    int status    = 0;
    long naxis    = 2;
    long naxes[2] = {0};
    int nelements = 0;
    std::string bit_depth;
    char error_status[MAXRBUF];
    switch (getBPS())
    {
        case 8:
            byte_type = TBYTE;
            img_type  = BYTE_IMG;
            bit_depth = "8 bits per sample";
            break;

        case 16:
            byte_type = TUSHORT;
            img_type  = USHORT_IMG;
            bit_depth = "16 bits per sample";
            break;

        case 32:
            byte_type = TLONG;
            img_type  = LONG_IMG;
            bit_depth = "32 bits per sample";
            break;

        case 64:
            byte_type = TLONGLONG;
            img_type  = LONGLONG_IMG;
            bit_depth = "64 bits double per sample";
            break;

        case -32:
            byte_type = TFLOAT;
            img_type  = FLOAT_IMG;
            bit_depth = "32 bits double per sample";
            break;

        case -64:
            byte_type = TDOUBLE;
            img_type  = DOUBLE_IMG;
            bit_depth = "64 bits double per sample";
            break;

        default:
            DEBUGF(Logger::DBG_ERROR, "Unsupported bits per sample value %d", getBPS());
            return nullptr;
    }
    naxes[0] = len;
    naxes[0] = naxes[0] < 1 ? 1 : naxes[0];
    naxes[1] = 1;
    nelements = naxes[0];

    /*DEBUGF(Logger::DBG_DEBUG, "Exposure complete. Image Depth: %s. Width: %d Height: %d nelements: %d", bit_depth.c_str(), naxes[0],
            naxes[1], nelements);*/

    //  Now we have to send fits format data to the client
    memsize = 5760;
    memptr  = malloc(memsize);
    if (!memptr)
    {
        DEBUGF(Logger::DBG_ERROR, "Error: failed to allocate memory: %lu", static_cast<unsigned long>(memsize));
    }

    fits_create_memfile(&fptr, &memptr, &memsize, 2880, realloc, &status);

    if (status)
    {
        fits_report_error(stderr, status); /* print out any error messages */
        fits_get_errstatus(status, error_status);
        DEBUGF(Logger::DBG_ERROR, "FITS Error: %s", error_status);
        if(memptr != nullptr)
            free(memptr);
        return nullptr;
    }

    fits_create_img(fptr, img_type, naxis, naxes, &status);

    if (status)
    {
        fits_report_error(stderr, status); /* print out any error messages */
        fits_get_errstatus(status, error_status);
        DEBUGF(Logger::DBG_ERROR, "FITS Error: %s", error_status);
        if(memptr != nullptr)
            free(memptr);
        return nullptr;
    }

    addFITSKeywords(fptr, buf, len);

    fits_write_img(fptr, byte_type, 1, nelements, buf, &status);

    if (status)
    {
        fits_report_error(stderr, status); /* print out any error messages */
        fits_get_errstatus(status, error_status);
        DEBUGF(Logger::DBG_ERROR, "FITS Error: %s", error_status);
        if(memptr != nullptr)
            free(memptr);
        return nullptr;
    }

    fits_close_file(fptr, &status);

    uploadFile(memptr, memsize, sendIntegration, saveIntegration);

    return memptr;
}

bool SensorInterface::IntegrationComplete()
{
    // Reset POLLMS to default value
    POLLMS = getPollingPeriod();

    if(HasDSP())
    {
        uint8_t* buf = (uint8_t*)malloc(getBufferSize());
        memcpy(buf, getBuffer(), getBufferSize());
        DSP->processBLOB(buf, 1, new int[1] { getBufferSize() * 8 / getBPS() }, getBPS());
        free(buf);
    }
    // Run async
    std::thread(&SensorInterface::IntegrationCompletePrivate, this).detach();

    return true;
}

bool SensorInterface::IntegrationCompletePrivate()
{
    bool sendIntegration = (UploadS[0].s == ISS_ON || UploadS[2].s == ISS_ON);
    bool saveIntegration = (UploadS[1].s == ISS_ON || UploadS[2].s == ISS_ON);
    bool autoLoop   = false;

    if (sendIntegration || saveIntegration)
    {
        void* blob = nullptr;
        if (!strcmp(getIntegrationFileExtension(), "fits"))
        {
            blob = sendFITS(getBuffer(), getBufferSize() * 8 / abs(getBPS()));
        }
        else
        {
            uploadFile(getBuffer(), getBufferSize(), sendIntegration,
                       saveIntegration);
        }

        if (sendIntegration)
            IDSetBLOB(&FitsBP, nullptr);
        if(blob != nullptr)
            free(blob);

        DEBUG(Logger::DBG_DEBUG, "Upload complete");
    }

    FramedIntegrationNP.s = IPS_OK;
    IDSetNumber(&FramedIntegrationNP, nullptr);

    if (autoLoop)
    {
        FramedIntegrationN[0].value = IntegrationTime;
        FramedIntegrationNP.s       = IPS_BUSY;

        if (StartIntegration(IntegrationTime))
            FramedIntegrationNP.s = IPS_BUSY;
        else
        {
            DEBUG(Logger::DBG_DEBUG, "Autoloop: Sensor Integration Error!");
            FramedIntegrationNP.s = IPS_ALERT;
        }

        IDSetNumber(&FramedIntegrationNP, nullptr);
    }

    return true;
}

bool SensorInterface::uploadFile(const void *fitsData, size_t totalBytes, bool sendIntegration,
                                 bool saveIntegration)
{

    DEBUGF(Logger::DBG_DEBUG, "Uploading file. Ext: %s, Size: %d, sendIntegration? %s, saveIntegration? %s",
           getIntegrationFileExtension(), totalBytes, sendIntegration ? "Yes" : "No", saveIntegration ? "Yes" : "No");

    FitsB.blob    = const_cast<void *>(fitsData);
    FitsB.bloblen = totalBytes;
    snprintf(FitsB.format, MAXINDIBLOBFMT, ".%s", getIntegrationFileExtension());
    if (saveIntegration)
    {

        FILE *fp = nullptr;
        char integrationFileName[MAXRBUF];

        std::string prefix = UploadSettingsT[UPLOAD_PREFIX].text;
        int maxIndex       = getFileIndex(UploadSettingsT[UPLOAD_DIR].text, UploadSettingsT[UPLOAD_PREFIX].text,
                                          FitsB.format);

        if (maxIndex < 0)
        {
            DEBUGF(Logger::DBG_ERROR, "Error iterating directory %s. %s", UploadSettingsT[0].text,
                   strerror(errno));
            return false;
        }

        if (maxIndex > 0)
        {
            char ts[32];
            struct tm *tp;
            time_t t;
            time(&t);
            tp = localtime(&t);
            strftime(ts, sizeof(ts), "%Y-%m-%dT%H-%M-%S", tp);
            std::string filets(ts);
            prefix = std::regex_replace(prefix, std::regex("ISO8601"), filets);

            char indexString[8];
            snprintf(indexString, 8, "%03d", maxIndex);
            std::string prefixIndex = indexString;
            //prefix.replace(prefix.find("XXX"), std::string::npos, prefixIndex);
            prefix = std::regex_replace(prefix, std::regex("XXX"), prefixIndex);
        }

        snprintf(integrationFileName, MAXRBUF, "%s/%s%s", UploadSettingsT[0].text, prefix.c_str(), FitsB.format);

        fp = fopen(integrationFileName, "w");
        if (fp == nullptr)
        {
            DEBUGF(Logger::DBG_ERROR, "Unable to save image file (%s). %s", integrationFileName, strerror(errno));
            return false;
        }

        int n = 0;
        for (int nr = 0; nr < static_cast<int>(FitsB.bloblen); nr += n)
            n = fwrite((static_cast<char *>(FitsB.blob) + nr), 1, FitsB.bloblen - nr, fp);

        fclose(fp);

        // Save image file path
        IUSaveText(&FileNameT[0], integrationFileName);

        DEBUGF(Logger::DBG_SESSION, "Image saved to %s", integrationFileName);
        FileNameTP.s = IPS_OK;
        IDSetText(&FileNameTP, nullptr);
    }

    FitsB.size = totalBytes;
    FitsBP.s   = IPS_OK;

    DEBUG(Logger::DBG_DEBUG, "Upload complete");

    return true;
}

bool SensorInterface::StartStreaming()
{
    LOG_ERROR("Streaming is not supported.");
    return false;
}

bool SensorInterface::StopStreaming()
{
    LOG_ERROR("Streaming is not supported.");
    return false;
}

int SensorInterface::SetTemperature(double temperature)
{
    INDI_UNUSED(temperature);
    DEBUGF(Logger::DBG_WARNING, "SensorInterface::SetTemperature %4.2f -  Should never get here", temperature);
    return -1;
}

bool SensorInterface::saveConfigItems(FILE *fp)
{
    INDI::DefaultDevice::saveConfigItems(fp);

    IUSaveConfigText(fp, &ActiveDeviceTP);
    IUSaveConfigSwitch(fp, &UploadSP);
    IUSaveConfigText(fp, &UploadSettingsTP);
    IUSaveConfigSwitch(fp, &TelescopeTypeSP);

    if (HasStreaming())
        Streamer->saveConfigItems(fp);

    if (HasDSP())
        DSP->saveConfigItems(fp);

    return true;
}

void SensorInterface::getMinMax(double *min, double *max, uint8_t *buf, int len, int bpp)
{
    int ind         = 0, i, j;
    int integrationHeight = 1;
    int integrationWidth  = len;
    double lmin = 0, lmax = 0;

    switch (bpp)
    {
        case 8:
        {
            uint8_t *integrationBuffer = static_cast<uint8_t *>(buf);
            lmin = lmax = integrationBuffer[0];

            for (i = 0; i < integrationHeight; i++)
                for (j = 0; j < integrationWidth; j++)
                {
                    ind = (i * integrationWidth) + j;
                    if (integrationBuffer[ind] < lmin)
                        lmin = integrationBuffer[ind];
                    else if (integrationBuffer[ind] > lmax)
                        lmax = integrationBuffer[ind];
                }
        }
        break;

        case 16:
        {
            uint16_t *integrationBuffer = reinterpret_cast<uint16_t *>(buf);
            lmin = lmax = integrationBuffer[0];

            for (i = 0; i < integrationHeight; i++)
                for (j = 0; j < integrationWidth; j++)
                {
                    ind = (i * integrationWidth) + j;
                    if (integrationBuffer[ind] < lmin)
                        lmin = integrationBuffer[ind];
                    else if (integrationBuffer[ind] > lmax)
                        lmax = integrationBuffer[ind];
                }
        }
        break;

        case 32:
        {
            uint32_t *integrationBuffer = reinterpret_cast<uint32_t *>(buf);
            lmin = lmax = integrationBuffer[0];

            for (i = 0; i < integrationHeight; i++)
                for (j = 0; j < integrationWidth; j++)
                {
                    ind = (i * integrationWidth) + j;
                    if (integrationBuffer[ind] < lmin)
                        lmin = integrationBuffer[ind];
                    else if (integrationBuffer[ind] > lmax)
                        lmax = integrationBuffer[ind];
                }
        }
        break;

        case 64:
        {
            unsigned long *integrationBuffer = reinterpret_cast<unsigned long *>(buf);
            lmin = lmax = integrationBuffer[0];

            for (i = 0; i < integrationHeight; i++)
                for (j = 0; j < integrationWidth; j++)
                {
                    ind = (i * integrationWidth) + j;
                    if (integrationBuffer[ind] < lmin)
                        lmin = integrationBuffer[ind];
                    else if (integrationBuffer[ind] > lmax)
                        lmax = integrationBuffer[ind];
                }
        }
        break;

        case -32:
        {
            double *integrationBuffer = reinterpret_cast<double *>(buf);
            lmin = lmax = integrationBuffer[0];

            for (i = 0; i < integrationHeight; i++)
                for (j = 0; j < integrationWidth; j++)
                {
                    ind = (i * integrationWidth) + j;
                    if (integrationBuffer[ind] < lmin)
                        lmin = integrationBuffer[ind];
                    else if (integrationBuffer[ind] > lmax)
                        lmax = integrationBuffer[ind];
                }
        }
        break;

        case -64:
        {
            double *integrationBuffer = reinterpret_cast<double *>(buf);
            lmin = lmax = integrationBuffer[0];

            for (i = 0; i < integrationHeight; i++)
                for (j = 0; j < integrationWidth; j++)
                {
                    ind = (i * integrationWidth) + j;
                    if (integrationBuffer[ind] < lmin)
                        lmin = integrationBuffer[ind];
                    else if (integrationBuffer[ind] > lmax)
                        lmax = integrationBuffer[ind];
                }
        }
        break;
    }
    *min = lmin;
    *max = lmax;
}

std::string regex_replace_compat2(const std::string &input, const std::string &pattern, const std::string &replace)
{
    std::stringstream s;
    std::regex_replace(std::ostreambuf_iterator<char>(s), input.begin(), input.end(), std::regex(pattern), replace);
    return s.str();
}

int SensorInterface::getFileIndex(const char *dir, const char *prefix, const char *ext)
{
    INDI_UNUSED(ext);

    DIR *dpdf = nullptr;
    struct dirent *epdf = nullptr;
    std::vector<std::string> files = std::vector<std::string>();

    std::string prefixIndex = prefix;
    prefixIndex             = regex_replace_compat2(prefixIndex, "_ISO8601", "");
    prefixIndex             = regex_replace_compat2(prefixIndex, "_XXX", "");

    // Create directory if does not exist
    struct stat st;

    if (stat(dir, &st) == -1)
    {
        DEBUGF(Logger::DBG_DEBUG, "Creating directory %s...", dir);
        if (_det_mkdir(dir, 0755) == -1)
            DEBUGF(Logger::DBG_ERROR, "Error creating directory %s (%s)", dir, strerror(errno));
    }

    dpdf = opendir(dir);
    if (dpdf != nullptr)
    {
        while ((epdf = readdir(dpdf)))
        {
            if (strstr(epdf->d_name, prefixIndex.c_str()))
                files.push_back(epdf->d_name);
        }
        closedir(dpdf);
    }
    else
        return -1;

    int maxIndex = 0;

    for (int i = 0; i < static_cast<int>(files.size()); i++)
    {
        int index = -1;

        std::string file  = files.at(i);
        std::size_t start = file.find_last_of("_");
        std::size_t end   = file.find_last_of(".");
        if (start != std::string::npos)
        {
            index = atoi(file.substr(start + 1, end).c_str());
            if (index > maxIndex)
                maxIndex = index;
        }
    }

    return (maxIndex + 1);
}

void SensorInterface::setBPS(int bps)
{
    BPS = bps;

    // Reset size
    if (HasStreaming())
        Streamer->setSize(getBufferSize() * 8 / BPS);

    // DSP
    if (HasDSP())
        DSP->setSizes(1, new int[1] { getBufferSize() * 8 / BPS });
}


bool SensorInterface::Handshake()
{
    return false;
}

bool SensorInterface::callHandshake()
{
    if (sensorConnection > 0)
    {
        if (getActiveConnection() == serialConnection)
            PortFD = serialConnection->getPortFD();
        else if (getActiveConnection() == tcpConnection)
            PortFD = tcpConnection->getPortFD();
    }

    return Handshake();
}

void SensorInterface::setSensorConnection(const uint8_t &value)
{
    uint8_t mask = CONNECTION_SERIAL | CONNECTION_TCP | CONNECTION_NONE;

    if (value == 0 || (mask & value) == 0)
    {
        DEBUGF(Logger::DBG_ERROR, "Invalid connection mode %d", value);
        return;
    }

    sensorConnection = value;
}
}
