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
#include "indiutility.h"

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
        defineProperty(FramedIntegrationNP);

        if (CanAbort())
            defineProperty(AbortIntegrationSP);

        defineProperty(FITSHeaderTP);

        if (HasCooler())
            defineProperty(TemperatureNP);

        defineProperty(&FitsBP);

        defineProperty(TelescopeTypeSP);

        defineProperty(UploadSP);

        if (UploadSettingsTP[UPLOAD_DIR].text == nullptr)
            UploadSettingsTP[UPLOAD_DIR].setText(getenv("HOME"));
        defineProperty(UploadSettingsTP);
    }
    else
    {
        deleteProperty(FramedIntegrationNP.getName());
        if (CanAbort())
            deleteProperty(AbortIntegrationSP.getName());
        deleteProperty(FitsBP.name);

        deleteProperty(FITSHeaderTP.getName());

        if (HasCooler())
            deleteProperty(TemperatureNP.getName());

        deleteProperty(TelescopeTypeSP.getName());

        deleteProperty(UploadSP.getName());
        deleteProperty(UploadSettingsTP.getName());
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

    defineProperty(ActiveDeviceTP);
    loadConfig(true, "ACTIVE_DEVICES");

    if (HasStreaming())
        Streamer->ISGetProperties(dev);

    if (HasDSP())
        DSP->ISGetProperties(dev);
}

bool SensorInterface::processSnoopDevice(XMLEle *root)
{
    if (!IUSnoopNumber(root, EqNP.getNumber())) // #PS: refactor needed
    {
        RA = EqNP[0].getValue();
        Dec = EqNP[1].getValue();
        //IDLog("Snooped RA %4.2f  Dec %4.2f\n", RA, Dec);
    }
    if (!IUSnoopNumber(root, LocationNP.getNumber())) // #PS: refactor needed
    {
        Lat = LocationNP[0].getValue();
        Lon = LocationNP[1].getValue();
        El = LocationNP[2].getValue();
        //IDLog("Snooped Lat %4.2f  Lon %4.2f  El %4.2f\n", RA, Dec, El);
    }
    if (!IUSnoopNumber(root, ScopeParametersNP.getNumber())) // #PS: refactor needed
    {
        primaryAperture = ScopeParametersNP[0].getValue();
        primaryFocalLength = ScopeParametersNP[1].getValue();
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
        if (ActiveDeviceTP.isNameMatch(name))
        {
            ActiveDeviceTP.setState(IPS_OK);
            ActiveDeviceTP.update(texts, names, n);
            ActiveDeviceTP.apply();

            // Update the property name!
            EqNP.setDeviceName(ActiveDeviceTP[0].getText());
            LocationNP.setDeviceName(ActiveDeviceTP[0].getText());
            ScopeParametersNP.setDeviceName(ActiveDeviceTP[0].getText());

            IDSnoopDevice(ActiveDeviceTP[0].getText(), "EQUATORIAL_EOD_COORD");
            IDSnoopDevice(ActiveDeviceTP[0].getText(), "GEOGRAPHIC_COORD");
            IDSnoopDevice(ActiveDeviceTP[0].getText(), "TELESCOPE_INFO");
            IDSnoopDevice(ActiveDeviceTP[1].getText(), "GEOGRAPHIC_COORD");

            // Tell children active devices was updated.
            activeDevicesUpdated();

            //  We processed this one, so, tell the world we did it
            return true;
        }

        if (FITSHeaderTP.isNameMatch(name))
        {
            FITSHeaderTP.update(texts, names, n);
            FITSHeaderTP.setState(IPS_OK);
            FITSHeaderTP.apply();
            return true;
        }

        if (UploadSettingsTP.isNameMatch(name))
        {
            UploadSettingsTP.update(texts, names, n);
            UploadSettingsTP.setState(IPS_OK);
            UploadSettingsTP.apply();
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
            if ((values[0] < FramedIntegrationNP[0].min || values[0] > FramedIntegrationNP[0].max))
            {
                DEBUGF(Logger::DBG_ERROR, "Requested integration value (%g) seconds out of bounds [%g,%g].",
                       values[0], FramedIntegrationNP[0].min, FramedIntegrationNP[0].max);
                FramedIntegrationNP.setState(IPS_ALERT);
                FramedIntegrationNP.apply();
                return false;
            }

            FramedIntegrationNP[0].setValue(IntegrationTime = values[0]);

            if (FramedIntegrationNP.getState() == IPS_BUSY)
            {
                if (CanAbort() && AbortIntegration() == false)
                    DEBUG(Logger::DBG_WARNING, "Warning: Aborting integration failed.");
            }

            if (StartIntegration(IntegrationTime))
                FramedIntegrationNP.setState(IPS_BUSY);
            else
                FramedIntegrationNP.setState(IPS_ALERT);
            FramedIntegrationNP.apply();
            return true;
        }

        // Sensor TEMPERATURE:
        if (TemperatureNP.isNameMatch(name))
        {
            if (values[0] < TemperatureNP[0].min || values[0] > TemperatureNP[0].max)
            {
                TemperatureNP.setState(IPS_ALERT);
                DEBUGF(Logger::DBG_ERROR, "Error: Bad temperature value! Range is [%.1f, %.1f] [C].",
                       TemperatureNP[0].min, TemperatureNP[0].max);
                TemperatureNP.apply();
                return false;
            }

            int rc = SetTemperature(values[0]);

            if (rc == 0)
                TemperatureNP.setState(IPS_BUSY);
            else if (rc == 1)
                TemperatureNP.setState(IPS_OK);
            else
                TemperatureNP.setState(IPS_ALERT);

            TemperatureNP.apply();
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
        if (UploadSP.isNameMatch(name))
        {
            int prevMode = UploadSP.findOnSwitchIndex();
            UploadSP.update(states, names, n);
            UploadSP.setState(IPS_OK);
            UploadSP.apply();

            if (UploadSP[0].getState() == ISS_ON)
            {
                DEBUG(Logger::DBG_SESSION, "Upload settings set to client only.");
                if (prevMode != 0)
                    deleteProperty(FileNameTP.getName());
            }
            else if (UploadSP[1].getState() == ISS_ON)
            {
                DEBUG(Logger::DBG_SESSION, "Upload settings set to local only.");
                defineProperty(FileNameTP);
            }
            else
            {
                DEBUG(Logger::DBG_SESSION, "Upload settings set to client and local.");
                defineProperty(FileNameTP);
            }
            return true;
        }

        if (TelescopeTypeSP.isNameMatch(name))
        {
            TelescopeTypeSP.update(states, names, n);
            TelescopeTypeSP.setState(IPS_OK);
            TelescopeTypeSP.apply();
            return true;
        }

        // Primary Device Abort Expsoure
        if (AbortIntegrationSP.isNameMatch(name))
        {
            AbortIntegrationSP.reset();

            if (AbortIntegration())
            {
                AbortIntegrationSP.setState(IPS_OK);
                FramedIntegrationNP.setState(IPS_IDLE);
                FramedIntegrationNP[0].setValue(0);
            }
            else
            {
                AbortIntegrationSP.setState(IPS_ALERT);
                FramedIntegrationNP.setState(IPS_ALERT);
            }

            AbortIntegrationSP.apply();
            FramedIntegrationNP.apply();

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
    TemperatureNP[0].fill("SENSOR_TEMPERATURE_VALUE", "Temperature (C)", "%5.2f", -50.0, 50.0, 0., 0.);
    TemperatureNP.fill(getDeviceName(), "SENSOR_TEMPERATURE", "Temperature",
                       MAIN_CONTROL_TAB, IP_RW, 60, IPS_IDLE);

    /**********************************************/
    /**************** Primary Device ****************/
    /**********************************************/

    // Sensor Integration
    FramedIntegrationNP[0].fill("SENSOR_INTEGRATION_VALUE", "Time (s)", "%5.2f", 0.01, 3600, 1.0, 1.0);
    FramedIntegrationNP.fill(getDeviceName(), "SENSOR_INTEGRATION",
                       "Integration", MAIN_CONTROL_TAB, IP_RW, 60, IPS_IDLE);

    // Sensor Abort
    if(CanAbort())
    {
        AbortIntegrationSP[0].fill("ABORT", "Abort", ISS_OFF);
        AbortIntegrationSP.fill(getDeviceName(), "SENSOR_ABORT_INTEGRATION",
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
    UploadSP[0].fill("UPLOAD_CLIENT", "Client", ISS_ON);
    UploadSP[1].fill("UPLOAD_LOCAL", "Local", ISS_OFF);
    UploadSP[2].fill("UPLOAD_BOTH", "Both", ISS_OFF);
    UploadSP.fill(getDeviceName(), "UPLOAD_MODE", "Upload", OPTIONS_TAB, IP_RW, ISR_1OFMANY,
                       0, IPS_IDLE);

    // Upload Settings
    UploadSettingsTP[UPLOAD_DIR].fill("UPLOAD_DIR", "Dir", "");
    UploadSettingsTP[UPLOAD_PREFIX].fill("UPLOAD_PREFIX", "Prefix", "INTEGRATION_XXX");
    UploadSettingsTP.fill(getDeviceName(), "UPLOAD_SETTINGS", "Upload Settings",
                     OPTIONS_TAB, IP_RW, 60, IPS_IDLE);

    // Upload File Path
    FileNameTP[0].fill("FILE_PATH", "Path", "");
    FileNameTP.fill(getDeviceName(), "SENSOR_FILE_PATH", "Filename", OPTIONS_TAB, IP_RO, 60,
                     IPS_IDLE);

    /**********************************************/
    /****************** FITS Header****************/
    /**********************************************/

    FITSHeaderTP[FITS_OBSERVER].fill("FITS_OBSERVER", "Observer", "Unknown");
    FITSHeaderTP[FITS_OBJECT].fill("FITS_OBJECT", "Object", "Unknown");
    FITSHeaderTP.fill(getDeviceName(), "FITS_HEADER", "FITS Header", INFO_TAB, IP_RW, 60,
                     IPS_IDLE);

    /**********************************************/
    /**************** Snooping ********************/
    /**********************************************/

    // Snooped Devices
    ActiveDeviceTP[0].fill("ACTIVE_TELESCOPE", "Telescope", "Telescope Simulator");
    ActiveDeviceTP[1].fill("ACTIVE_GPS", "GPS", "GPS Simulator");
    ActiveDeviceTP.fill(getDeviceName(), "ACTIVE_DEVICES", "Snoop devices", OPTIONS_TAB,
                     IP_RW, 60, IPS_IDLE);

    // Snoop properties of interest
    EqNP[0].fill("RA", "RA (hh:mm:ss)", "%010.6m", 0, 24, 0, 0);
    EqNP[1].fill("DEC", "DEC (dd:mm:ss)", "%010.6m", -90, 90, 0, 0);
    EqNP.fill(getDeviceName(), "EQUATORIAL_EOD_COORD", "Eq. Coordinates", MAIN_CONTROL_TAB,
                       IP_RW, 60, IPS_IDLE);

    LocationNP[0].fill("LAT", "Lat (dd:mm:ss)", "%010.6m", -90, 90, 0, 0.0);
    LocationNP[1].fill("LONG", "Lon (dd:mm:ss)", "%010.6m", 0, 360, 0, 0.0);
    LocationNP[2].fill("ELEV", "Elevation (m)", "%g", -200, 10000, 0, 0);
    LocationNP.fill(getDeviceName(), "GEOGRAPHIC_COORD", "Location", MAIN_CONTROL_TAB,
                       IP_RO, 60, IPS_IDLE);

    ScopeParametersNP[0].fill("TELESCOPE_APERTURE", "Aperture (mm)", "%g", 10, 5000, 0, 0.0);
    ScopeParametersNP[1].fill("TELESCOPE_FOCAL_LENGTH", "Focal Length (mm)", "%g", 10, 10000, 0, 0.0);
    ScopeParametersNP[2].fill("GUIDER_APERTURE", "Guider Aperture (mm)", "%g", 10, 5000, 0, 0.0);
    ScopeParametersNP[3].fill("GUIDER_FOCAL_LENGTH", "Guider Focal Length (mm)", "%g", 10, 10000, 0, 0.0);
    ScopeParametersNP.fill(getDeviceName(), "TELESCOPE_INFO", "Scope Properties",
                       OPTIONS_TAB, IP_RW, 60, IPS_OK);

    IDSnoopDevice(ActiveDeviceTP[0].getText(), "EQUATORIAL_EOD_COORD");
    IDSnoopDevice(ActiveDeviceTP[0].getText(), "GEOGRAPHIC_COORD");
    IDSnoopDevice(ActiveDeviceTP[0].getText(), "TELESCOPE_INFO");
    IDSnoopDevice(ActiveDeviceTP[1].getText(), "GEOGRAPHIC_COORD");

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

    if (FramedIntegrationNP.isNameMatch(property))
    {
        vp = FramedIntegrationNP.getNumber(); // #PS: refactor needed

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
    FramedIntegrationNP[0].setValue(duration);

    FramedIntegrationNP.apply();
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
    FramedIntegrationNP.setState(IPS_ALERT);
    FramedIntegrationNP.apply();
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
#ifndef WITH_MINMAX
    INDI_UNUSED(buf);
    INDI_UNUSED(len);
#endif
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
    strncpy(fitsString, ActiveDeviceTP[0].getText(), MAXINDIDEVICE);
    fits_update_key_s(fptr, TSTRING, "TELESCOP", fitsString, "Telescope name", &status);

    // Observer
    strncpy(fitsString, FITSHeaderTP[FITS_OBSERVER].getText(), MAXINDIDEVICE);
    fits_update_key_s(fptr, TSTRING, "OBSERVER", fitsString, "Observer name", &status);

    // Object
    strncpy(fitsString, FITSHeaderTP[FITS_OBJECT].getText(), MAXINDIDEVICE);
    fits_update_key_s(fptr, TSTRING, "OBJECT", fitsString, "Object name", &status);

    integrationTime = getIntegrationTime();

    strncpy(dev_name, getDeviceName(), 32);
    strncpy(exp_start, getIntegrationStartTime(), 32);
    snprintf(timestamp, 32, "%lf", startIntegrationTime);

    fits_update_key_s(fptr, TDOUBLE, "EXPTIME", &(integrationTime), "Total Integration Time (s)", &status);

    if (HasCooler())
        fits_update_key_s(fptr, TDOUBLE, "SENSOR-TEMP", &(TemperatureNP[0].value), "PrimarySensorInterface Temperature (Celsius)",
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
    bool sendIntegration = (UploadSP[0].getState() == ISS_ON || UploadSP[2].getState() == ISS_ON);
    bool saveIntegration = (UploadSP[1].getState() == ISS_ON || UploadSP[2].getState() == ISS_ON);
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
    setCurrentPollingPeriod(getPollingPeriod());

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
    bool sendIntegration = (UploadSP[0].getState() == ISS_ON || UploadSP[2].getState() == ISS_ON);
    bool saveIntegration = (UploadSP[1].getState() == ISS_ON || UploadSP[2].getState() == ISS_ON);
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

    FramedIntegrationNP.setState(IPS_OK);
    FramedIntegrationNP.apply();

    if (autoLoop)
    {
        FramedIntegrationNP[0].setValue(IntegrationTime);
        FramedIntegrationNP.setState(IPS_BUSY);

        if (StartIntegration(IntegrationTime))
            FramedIntegrationNP.setState(IPS_BUSY);
        else
        {
            DEBUG(Logger::DBG_DEBUG, "Autoloop: Sensor Integration Error!");
            FramedIntegrationNP.setState(IPS_ALERT);
        }

        FramedIntegrationNP.apply();
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

        std::string prefix = UploadSettingsTP[UPLOAD_PREFIX].getText();
        int maxIndex       = getFileIndex(UploadSettingsTP[UPLOAD_DIR].getText(), UploadSettingsTP[UPLOAD_PREFIX].getText(),
                                          FitsB.format);

        if (maxIndex < 0)
        {
            DEBUGF(Logger::DBG_ERROR, "Error iterating directory %s. %s", UploadSettingsTP[0].getText(),
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

        snprintf(integrationFileName, MAXRBUF, "%s/%s%s", UploadSettingsTP[0].getText(), prefix.c_str(), FitsB.format);

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
        FileNameTP[0].setText(integrationFileName);

        DEBUGF(Logger::DBG_SESSION, "Image saved to %s", integrationFileName);
        FileNameTP.setState(IPS_OK);
        FileNameTP.apply();
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

    ActiveDeviceTP.save(fp);
    UploadSP.save(fp);
    UploadSettingsTP.save(fp);
    TelescopeTypeSP.save(fp);

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
        LOGF_DEBUG("Creating directory %s...", dir);
        if (INDI::mkpath(dir, 0755) == -1)
            LOGF_ERROR("Error creating directory %s (%s)", dir, strerror(errno));
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
