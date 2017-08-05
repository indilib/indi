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

#include "indidetector.h"

#include "indicom.h"

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


const char *CAPTURE_SETTINGS_TAB = "Capture Settings";
const char *CAPTURE_INFO_TAB     = "Capture Info";

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

DetectorDevice::DetectorDevice()
{
    RawFrame     = (uint8_t *)malloc(sizeof(uint8_t)); // Seed for realloc
    RawFrameSize = 0;

    BPS         = 8;
    NAxis       = 2;


    strncpy(captureExtention, "fits", MAXINDIBLOBFMT);
}

DetectorDevice::~DetectorDevice()
{
    free(RawFrame);
    RawFrameSize = 0;
    RawFrame     = nullptr;
}

void DetectorDevice::setMinMaxStep(const char *property, const char *element, double min, double max, double step,
                            bool sendToClient)
{
    INumberVectorProperty *nvp = nullptr;

    if (!strcmp(property, FramedCaptureNP.name))
        nvp = &FramedCaptureNP;

    INumber *np = IUFindNumber(nvp, element);
    if (np)
    {
        np->min  = min;
        np->max  = max;
        np->step = step;

        if (sendToClient)
            IUUpdateMinMax(nvp);
    }
}

void DetectorDevice::setBandwidth(float bw)
{
    bandwidth = bw;

    DetectorInfoN[DetectorDevice::DETECTOR_BANDWIDTH].value = bw;

    IDSetNumber(&DetectorInfoNP, nullptr);
}

void DetectorDevice::setCaptureFreq(float capfreq)
{
    captureFreq = capfreq;

    DetectorInfoN[DetectorDevice::DETECTOR_CAPTUREFREQUENCY].value = capfreq;

    IDSetNumber(&DetectorInfoNP, nullptr);
}

void DetectorDevice::setSamplingFreq(float samfreq)
{
    samplingFreq = samfreq;

    DetectorInfoN[DetectorDevice::DETECTOR_SAMPLINGFREQUENCY].value = samfreq;

    IDSetNumber(&DetectorInfoNP, nullptr);
}

void DetectorDevice::setBPS(int bbs)
{
    BPS = bbs;

    DetectorInfoN[DetectorDevice::DETECTOR_BITSPERSAMPLE].value = BPS;

    IDSetNumber(&DetectorInfoNP, nullptr);
}

void DetectorDevice::setFrameBufferSize(int nbuf, bool allocMem)
{
    if (nbuf == RawFrameSize)
        return;

    RawFrameSize = nbuf;

    if (allocMem == false)
        return;

    RawFrame = (uint8_t *)realloc(RawFrame, nbuf * sizeof(uint8_t));
}

void DetectorDevice::setCaptureLeft(double duration)
{
    FramedCaptureN[0].value = duration;

    IDSetNumber(&FramedCaptureNP, nullptr);
}

void DetectorDevice::setCaptureDuration(double duration)
{
    captureDuration = duration;
    gettimeofday(&startCaptureTime, nullptr);
}

const char *DetectorDevice::getCaptureStartTime()
{
    static char ts[32];

    char iso8601[32];
    struct tm *tp;
    time_t t = (time_t)startCaptureTime.tv_sec;
    int u    = startCaptureTime.tv_usec / 1000.0;

    tp = gmtime(&t);
    strftime(iso8601, sizeof(iso8601), "%Y-%m-%dT%H:%M:%S", tp);
    snprintf(ts, 32, "%s.%03d", iso8601, u);
    return (ts);
}

void DetectorDevice::setCaptureFailed()
{
    FramedCaptureNP.s = IPS_ALERT;
    IDSetNumber(&FramedCaptureNP, nullptr);
}

int DetectorDevice::getNAxis() const
{
    return NAxis;
}

void DetectorDevice::setNAxis(int value)
{
    NAxis = value;
}

void DetectorDevice::setCaptureExtension(const char *ext)
{
    strncpy(captureExtention, ext, MAXINDIBLOBFMT);
}

INDI::Detector::Detector()
{
    //ctor
    capability = 0;

    InCapture              = false;

    AutoLoop         = false;
    SendCapture        = false;
    ShowMarker       = false;

    CaptureTime       = 0.0;
    CurrentFilterSlot  = -1;

    RA              = -1000;
    Dec             = -1000;
    MPSAS           = -1000;
    primaryAperture = primaryFocalLength - 1;
}

INDI::Detector::~Detector()
{
}

void INDI::Detector::SetDetectorCapability(uint32_t cap)
{
    capability = cap;

    setDriverInterface(getDriverInterface());
}

bool INDI::Detector::initProperties()
{
    DefaultDevice::initProperties(); //  let the base class flesh in what it wants

    // PrimaryDetector Temperature
    IUFillNumber(&TemperatureN[0], "DETECTOR_TEMPERATURE_VALUE", "Temperature (C)", "%5.2f", -50.0, 50.0, 0., 0.);
    IUFillNumberVector(&TemperatureNP, TemperatureN, 1, getDeviceName(), "DETECTOR_TEMPERATURE", "Temperature",
                       MAIN_CONTROL_TAB, IP_RW, 60, IPS_IDLE);

    /**********************************************/
    /**************** Primary Device ****************/
    /**********************************************/

    // PrimaryDetector Capture
    IUFillNumber(&PrimaryDetector.FramedCaptureN[0], "DETECTOR_CAPTURE_VALUE", "Duration (s)", "%5.2f", 0.01, 3600, 1.0, 1.0);
    IUFillNumberVector(&PrimaryDetector.FramedCaptureNP, PrimaryDetector.FramedCaptureN, 1, getDeviceName(), "DETECTOR_CAPTURE",
                       "Capture", MAIN_CONTROL_TAB, IP_RW, 60, IPS_IDLE);

    // PrimaryDetector Abort
    IUFillSwitch(&PrimaryDetector.AbortCaptureS[0], "ABORT", "Abort", ISS_OFF);
    IUFillSwitchVector(&PrimaryDetector.AbortCaptureSP, PrimaryDetector.AbortCaptureS, 1, getDeviceName(), "DETECTOR_ABORT_CAPTURE",
                       "Capture Abort", MAIN_CONTROL_TAB, IP_RW, ISR_ATMOST1, 60, IPS_IDLE);

    // PrimaryDetector Info
    IUFillNumber(&PrimaryDetector.DetectorInfoN[DetectorDevice::DETECTOR_BANDWIDTH], "DETECTOR_BANDWIDTH", "Bandwidth (Hz)", "%4.0f", 0.01, 16000, 0.01, 0);
    IUFillNumber(&PrimaryDetector.DetectorInfoN[DetectorDevice::DETECTOR_CAPTUREFREQUENCY], "DETECTOR_CAPTURE_FREQUENCY", "Observed frequency (Hz)", "%18.2f", 0.01, 1.0e+15, 0.01, 0);
    IUFillNumber(&PrimaryDetector.DetectorInfoN[DetectorDevice::DETECTOR_SAMPLINGFREQUENCY], "DETECTOR_SAMPLING_FREQUENCY", "Sampling frequency (Hz)", "%14.2f", 0.01, 1.0e+10, 0.01, 0);
    IUFillNumber(&PrimaryDetector.DetectorInfoN[DetectorDevice::DETECTOR_BITSPERSAMPLE], "DETECTOR_BITSPERSAMPLE", "Bits per sample", "%3.0f", 1, 64, 1, 8);
    IUFillNumberVector(&PrimaryDetector.DetectorInfoNP, PrimaryDetector.DetectorInfoN, 4, getDeviceName(), "DETECTOR_INFO", "Primary Detector Information", CAPTURE_INFO_TAB, IP_RW, 60, IPS_IDLE);

    // PrimaryDetector Device Data Blob
    IUFillBLOB(&PrimaryDetector.FitsB, "DETECTOR1", "Capture", "");
    IUFillBLOBVector(&PrimaryDetector.FitsBP, &PrimaryDetector.FitsB, 1, getDeviceName(), "DETECTOR1", "Capture Data", CAPTURE_INFO_TAB,
                     IP_RO, 60, IPS_IDLE);

    /**********************************************/
    /************** Upload Settings ***************/
    /**********************************************/

    // Upload Mode
    IUFillSwitch(&UploadS[0], "UPLOAD_CLIENT", "Client", ISS_ON);
    IUFillSwitch(&UploadS[1], "UPLOAD_LOCAL", "Local", ISS_OFF);
    IUFillSwitch(&UploadS[2], "UPLOAD_BOTH", "Both", ISS_OFF);
    IUFillSwitchVector(&UploadSP, UploadS, 3, getDeviceName(), "UPLOAD_MODE", "Upload", OPTIONS_TAB, IP_RW, ISR_1OFMANY,
                       0, IPS_IDLE);

    // Upload Settings
    IUFillText(&UploadSettingsT[UPLOAD_DIR], "UPLOAD_DIR", "Dir", "");
    IUFillText(&UploadSettingsT[UPLOAD_PREFIX], "UPLOAD_PREFIX", "Prefix", "CAPTURE_XXX");
    IUFillTextVector(&UploadSettingsTP, UploadSettingsT, 2, getDeviceName(), "UPLOAD_SETTINGS", "Upload Settings",
                     OPTIONS_TAB, IP_RW, 60, IPS_IDLE);

    // Upload File Path
    IUFillText(&FileNameT[0], "FILE_PATH", "Path", "");
    IUFillTextVector(&FileNameTP, FileNameT, 1, getDeviceName(), "DETECTOR_FILE_PATH", "Filename", CAPTURE_INFO_TAB, IP_RO, 60,
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
    IUFillText(&ActiveDeviceT[1], "ACTIVE_FOCUSER", "Focuser", "Focuser Simulator");
    IUFillText(&ActiveDeviceT[2], "ACTIVE_FILTER", "Filter", "PrimaryDetector Simulator");
    IUFillText(&ActiveDeviceT[3], "ACTIVE_SKYQUALITY", "Sky Quality", "SQM");
    IUFillTextVector(&ActiveDeviceTP, ActiveDeviceT, 4, getDeviceName(), "ACTIVE_DEVICES", "Snoop devices", OPTIONS_TAB,
                     IP_RW, 60, IPS_IDLE);

    // Snooped RA/DEC Property
    IUFillNumber(&EqN[0], "RA", "Ra (hh:mm:ss)", "%010.6m", 0, 24, 0, 0);
    IUFillNumber(&EqN[1], "DEC", "Dec (dd:mm:ss)", "%010.6m", -90, 90, 0, 0);
    IUFillNumberVector(&EqNP, EqN, 2, ActiveDeviceT[0].text, "EQUATORIAL_EOD_COORD", "EQ Coord", "Main Control", IP_RW,
                       60, IPS_IDLE);

    // Snoop properties of interest
    IDSnoopDevice(ActiveDeviceT[0].text, "EQUATORIAL_EOD_COORD");
    IDSnoopDevice(ActiveDeviceT[0].text, "TELESCOPE_INFO");
    IDSnoopDevice(ActiveDeviceT[2].text, "FILTER_SLOT");
    IDSnoopDevice(ActiveDeviceT[2].text, "FILTER_NAME");
    IDSnoopDevice(ActiveDeviceT[3].text, "SKY_QUALITY");

    setDriverInterface(DETECTOR_INTERFACE);

    return true;
}

void INDI::Detector::ISGetProperties(const char *dev)
{
    DefaultDevice::ISGetProperties(dev);

    defineText(&ActiveDeviceTP);
    loadConfig(true, "ACTIVE_DEVICES");
}

bool INDI::Detector::updateProperties()
{
    //IDLog("INDI::PrimaryDetector UpdateProperties isConnected returns %d %d\n",isConnected(),Connected);
    if (isConnected())
    {
        defineNumber(&PrimaryDetector.FramedCaptureNP);

        if (CanAbort())
            defineSwitch(&PrimaryDetector.AbortCaptureSP);

        defineText(&FITSHeaderTP);

        if (HasCooler())
            defineNumber(&TemperatureNP);

        defineNumber(&PrimaryDetector.DetectorInfoNP);
        defineBLOB(&PrimaryDetector.FitsBP);

        defineSwitch(&TelescopeTypeSP);

        defineSwitch(&UploadSP);

        if (UploadSettingsT[UPLOAD_DIR].text == nullptr)
            IUSaveText(&UploadSettingsT[UPLOAD_DIR], getenv("HOME"));
        defineText(&UploadSettingsTP);
    }
    else
    {
        deleteProperty(PrimaryDetector.DetectorInfoNP.name);

        deleteProperty(PrimaryDetector.FramedCaptureNP.name);
        if (CanAbort())
            deleteProperty(PrimaryDetector.AbortCaptureSP.name);
        deleteProperty(PrimaryDetector.FitsBP.name);

        deleteProperty(FITSHeaderTP.name);

        if (HasCooler())
            deleteProperty(TemperatureNP.name);

        deleteProperty(TelescopeTypeSP.name);

        deleteProperty(UploadSP.name);
        deleteProperty(UploadSettingsTP.name);
    }

    return true;
}

bool INDI::Detector::ISSnoopDevice(XMLEle *root)
{
    XMLEle *ep           = nullptr;
    const char *propName = findXMLAttValu(root, "name");

    if (IUSnoopNumber(root, &EqNP) == 0)
    {
        float newra, newdec;
        newra  = EqN[0].value;
        newdec = EqN[1].value;
        if ((newra != RA) || (newdec != Dec))
        {
            //IDLog("RA %4.2f  Dec %4.2f Snooped RA %4.2f  Dec %4.2f\n",RA,Dec,newra,newdec);
            RA  = newra;
            Dec = newdec;
        }
    }
    else if (!strcmp(propName, "TELESCOPE_INFO"))
    {
        for (ep = nextXMLEle(root, 1); ep != nullptr; ep = nextXMLEle(root, 0))
        {
            const char *name = findXMLAttValu(ep, "name");

            if (!strcmp(name, "TELESCOPE_APERTURE"))
            {
                primaryAperture = atof(pcdataXMLEle(ep));
            }
            else if (!strcmp(name, "TELESCOPE_FOCAL_LENGTH"))
            {
                primaryFocalLength = atof(pcdataXMLEle(ep));
            }
        }
    }
    else if (!strcmp(propName, "FILTER_NAME"))
    {
        FilterNames.clear();

        for (ep = nextXMLEle(root, 1); ep != nullptr; ep = nextXMLEle(root, 0))
            FilterNames.push_back(pcdataXMLEle(ep));
    }
    else if (!strcmp(propName, "FILTER_SLOT"))
    {
        CurrentFilterSlot = -1;
        for (ep = nextXMLEle(root, 1); ep != nullptr; ep = nextXMLEle(root, 0))
            CurrentFilterSlot = atoi(pcdataXMLEle(ep));
    }
    else if (!strcmp(propName, "SKY_QUALITY"))
    {
        for (ep = nextXMLEle(root, 1); ep != nullptr; ep = nextXMLEle(root, 0))
        {
            const char *name = findXMLAttValu(ep, "name");

            if (!strcmp(name, "SKY_BRIGHTNESS"))
            {
                MPSAS = atof(pcdataXMLEle(ep));
                break;
            }
        }
    }

    return INDI::DefaultDevice::ISSnoopDevice(root);
}

bool INDI::Detector::ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n)
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
            IDSnoopDevice(ActiveDeviceT[0].text, "EQUATORIAL_EOD_COORD");
            IDSnoopDevice(ActiveDeviceT[0].text, "TELESCOPE_INFO");
            IDSnoopDevice(ActiveDeviceT[2].text, "FILTER_SLOT");
            IDSnoopDevice(ActiveDeviceT[2].text, "FILTER_NAME");
            IDSnoopDevice(ActiveDeviceT[3].text, "SKY_QUALITY");

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

    return INDI::DefaultDevice::ISNewText(dev, name, texts, names, n);
}

bool INDI::Detector::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    //  first check if it's for our device
    //IDLog("INDI::Detector::ISNewNumber %s\n",name);
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        if (!strcmp(name, "DETECTOR_CAPTURE"))
        {
            if ((values[0] < PrimaryDetector.FramedCaptureN[0].min || values[0] > PrimaryDetector.FramedCaptureN[0].max))
            {
                DEBUGF(INDI::Logger::DBG_ERROR, "Requested capture value (%g) seconds out of bounds [%g,%g].",
                       values[0], PrimaryDetector.FramedCaptureN[0].min, PrimaryDetector.FramedCaptureN[0].max);
                PrimaryDetector.FramedCaptureNP.s = IPS_ALERT;
                IDSetNumber(&PrimaryDetector.FramedCaptureNP, nullptr);
                return false;
            }

            PrimaryDetector.FramedCaptureN[0].value = CaptureTime = values[0];

            if (PrimaryDetector.FramedCaptureNP.s == IPS_BUSY)
            {
                if (CanAbort() && AbortCapture() == false)
                    DEBUG(INDI::Logger::DBG_WARNING, "Warning: Aborting capture failed.");
            }

            if (StartCapture(CaptureTime))
                PrimaryDetector.FramedCaptureNP.s = IPS_BUSY;
            else
                PrimaryDetector.FramedCaptureNP.s = IPS_ALERT;
            IDSetNumber(&PrimaryDetector.FramedCaptureNP, nullptr);
            return true;
        }

        // PrimaryDetector TEMPERATURE:
        if (!strcmp(name, TemperatureNP.name))
        {
            if (values[0] < TemperatureN[0].min || values[0] > TemperatureN[0].max)
            {
                TemperatureNP.s = IPS_ALERT;
                DEBUGF(INDI::Logger::DBG_ERROR, "Error: Bad temperature value! Range is [%.1f, %.1f] [C].",
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

        // PrimaryDetector Info
        if (!strcmp(name, PrimaryDetector.DetectorInfoNP.name))
        {
            IUUpdateNumber(&PrimaryDetector.DetectorInfoNP, values, names, n);
            PrimaryDetector.DetectorInfoNP.s = IPS_OK;
            SetDetectorParams(PrimaryDetector.DetectorInfoNP.np[DetectorDevice::DETECTOR_BANDWIDTH].value,
                         PrimaryDetector.DetectorInfoNP.np[DetectorDevice::DETECTOR_CAPTUREFREQUENCY].value,
                         PrimaryDetector.DetectorInfoNP.np[DetectorDevice::DETECTOR_SAMPLINGFREQUENCY].value,
                         PrimaryDetector.DetectorInfoNP.np[DetectorDevice::DETECTOR_BITSPERSAMPLE].value);
            IDSetNumber(&PrimaryDetector.DetectorInfoNP, nullptr);
            return true;
        }
    }

    return DefaultDevice::ISNewNumber(dev, name, values, names, n);
}

bool INDI::Detector::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
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
                DEBUG(INDI::Logger::DBG_SESSION, "Upload settings set to client only.");
                if (prevMode != 0)
                    deleteProperty(FileNameTP.name);
            }
            else if (UploadS[1].s == ISS_ON)
            {
                DEBUG(INDI::Logger::DBG_SESSION, "Upload settings set to local only.");
                defineText(&FileNameTP);
            }
            else
            {
                DEBUG(INDI::Logger::DBG_SESSION, "Upload settings set to client and local.");
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
        if (strcmp(name, PrimaryDetector.AbortCaptureSP.name) == 0)
        {
            IUResetSwitch(&PrimaryDetector.AbortCaptureSP);

            if (AbortCapture())
            {
                PrimaryDetector.AbortCaptureSP.s       = IPS_OK;
                PrimaryDetector.FramedCaptureNP.s       = IPS_IDLE;
                PrimaryDetector.FramedCaptureN[0].value = 0;
            }
            else
            {
                PrimaryDetector.AbortCaptureSP.s = IPS_ALERT;
                PrimaryDetector.FramedCaptureNP.s = IPS_ALERT;
            }

            IDSetSwitch(&PrimaryDetector.AbortCaptureSP, nullptr);
            IDSetNumber(&PrimaryDetector.FramedCaptureNP, nullptr);

            return true;
        }
    }

    return DefaultDevice::ISNewSwitch(dev, name, states, names, n);
}

int INDI::Detector::SetTemperature(double temperature)
{
    INDI_UNUSED(temperature);
    DEBUGF(INDI::Logger::DBG_WARNING, "INDI::Detector::SetTemperature %4.2f -  Should never get here", temperature);
    return -1;
}

bool INDI::Detector::StartCapture(float duration)
{
    DEBUGF(INDI::Logger::DBG_WARNING, "INDI::Detector::StartCapture %4.2f -  Should never get here", duration);
    return false;
}

bool INDI::Detector::CaptureParamsUpdated(float bw, float capfreq, float samfreq, float bps)
{
    DEBUGF(INDI::Logger::DBG_WARNING, "INDI::Detector::CaptureParamsUpdated %15.0f %15.0f %15.0f %15.0f -  Should never get here", bw, capfreq, samfreq, bps);
    return false;
}

bool INDI::Detector::AbortCapture()
{
    DEBUG(INDI::Logger::DBG_WARNING, "INDI::Detector::AbortCapture -  Should never get here");
    return false;
}

void INDI::Detector::addFITSKeywords(fitsfile *fptr, DetectorDevice *targetDevice)
{
    int status = 0;
    char dev_name[32];
    char exp_start[32];
    double captureDuration;

    char *orig = setlocale(LC_NUMERIC, "C");

    char fitsString[MAXINDIDEVICE];

    // DETECTOR
    strncpy(fitsString, getDeviceName(), MAXINDIDEVICE);
    fits_update_key_s(fptr, TSTRING, "INSTRUME", fitsString, "PrimaryDetector Name", &status);

    // Telescope
    strncpy(fitsString, ActiveDeviceT[0].text, MAXINDIDEVICE);
    fits_update_key_s(fptr, TSTRING, "TELESCOP", fitsString, "Telescope name", &status);

    // Observer
    strncpy(fitsString, FITSHeaderT[FITS_OBSERVER].text, MAXINDIDEVICE);
    fits_update_key_s(fptr, TSTRING, "OBSERVER", fitsString, "Observer name", &status);

    // Object
    strncpy(fitsString, FITSHeaderT[FITS_OBJECT].text, MAXINDIDEVICE);
    fits_update_key_s(fptr, TSTRING, "OBJECT", fitsString, "Object name", &status);

    captureDuration = targetDevice->getCaptureDuration();

    strncpy(dev_name, getDeviceName(), 32);
    strncpy(exp_start, targetDevice->getCaptureStartTime(), 32);

    fits_update_key_s(fptr, TDOUBLE, "EXPTIME", &(captureDuration), "Total Capture Time (s)", &status);

    if (HasCooler())
        fits_update_key_s(fptr, TDOUBLE, "DETECTOR-TEMP", &(TemperatureN[0].value), "PrimaryDetector Temperature (Celsius)", &status);

    if (CurrentFilterSlot != -1 && CurrentFilterSlot <= (int)FilterNames.size())
    {
        char filter[32];
        strncpy(filter, FilterNames.at(CurrentFilterSlot - 1).c_str(), 32);
        fits_update_key_s(fptr, TSTRING, "FILTER", filter, "Filter", &status);
    }

#ifdef WITH_MINMAX
    if (targetDevice->getNAxis() == 2)
    {
        double min_val, max_val;
        getMinMax(&min_val, &max_val, targetDevice);

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

    if (RA != -1000 && Dec != -1000)
    {
        ln_equ_posn epochPos { 0, 0 }, J2000Pos { 0, 0 };
        epochPos.ra  = RA * 15.0;
        epochPos.dec = Dec;

        // Convert from JNow to J2000
        //TODO use exp_start instead of julian from system
        ln_get_equ_prec2(&epochPos, ln_get_julian_from_sys(), JD2000, &J2000Pos);

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

    fits_update_key_s(fptr, TSTRING, "DATE-OBS", exp_start, "UTC start date of observation", &status);
    fits_write_comment(fptr, "Generated by INDI", &status);

    setlocale(LC_NUMERIC, orig);
}

void INDI::Detector::fits_update_key_s(fitsfile *fptr, int type, std::string name, void *p, std::string explanation,
                                  int *status)
{
    // this function is for removing warnings about deprecated string conversion to char* (from arg 5)
    fits_update_key(fptr, type, name.c_str(), p, const_cast<char *>(explanation.c_str()), status);
}

bool INDI::Detector::CaptureComplete(DetectorDevice *targetDevice)
{
    bool sendCapture = (UploadS[0].s == ISS_ON || UploadS[2].s == ISS_ON);
    bool saveCapture = (UploadS[1].s == ISS_ON || UploadS[2].s == ISS_ON);
    //bool useSolver = (SolverS[0].s == ISS_ON);
    bool autoLoop   = false;

    if (sendCapture || saveCapture /* || useSolver*/)
    {
        if (!strcmp(targetDevice->getCaptureExtension(), "fits"))
        {
            void *memptr;
            size_t memsize;
            int img_type  = 0;
            int byte_type = 0;
            int status    = 0;
            long naxis    = 2;
            long naxes[naxis];
            int nelements = 0;
            std::string bit_depth;
            char error_status[MAXRBUF];

            fitsfile *fptr = nullptr;

            naxes[0] = targetDevice->getSamplingFrequency() * targetDevice->getCaptureDuration() * targetDevice->getBPS() / 8;
            naxes[1] = 1;

            switch (targetDevice->getBPS())
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
                    byte_type = TULONG;
                    img_type  = ULONG_IMG;
                    bit_depth = "32 bits per sample";
                    break;

                default:
                    DEBUGF(Logger::DBG_ERROR, "Unsupported bits per sample value %d", targetDevice->getBPS());
                    return false;
                    break;
            }

            nelements = naxes[0] * naxes[1];
            if (naxis == 3)
            {
                nelements *= 3;
                naxes[2] = 3;
            }

            /*DEBUGF(Logger::DBG_DEBUG, "Capture complete. Capture Depth: %s. Width: %d Height: %d nelements: %d", bit_depth.c_str(), naxes[0],
                    naxes[1], nelements);*/

            //  Now we have to send fits format data to the client
            memsize = 5760;
            memptr  = malloc(memsize);
            if (!memptr)
            {
                DEBUGF(INDI::Logger::DBG_ERROR, "Error: failed to allocate memory: %lu", (unsigned long)memsize);
            }

            fits_create_memfile(&fptr, &memptr, &memsize, 2880, realloc, &status);

            if (status)
            {
                fits_report_error(stderr, status); /* print out any error messages */
                fits_get_errstatus(status, error_status);
                DEBUGF(INDI::Logger::DBG_ERROR, "FITS Error: %s", error_status);
                return false;
            }

            fits_create_img(fptr, img_type, naxis, naxes, &status);

            if (status)
            {
                fits_report_error(stderr, status); /* print out any error messages */
                fits_get_errstatus(status, error_status);
                DEBUGF(INDI::Logger::DBG_ERROR, "FITS Error: %s", error_status);
                return false;
            }

            addFITSKeywords(fptr, targetDevice);

            fits_write_img(fptr, byte_type, 1, nelements, targetDevice->getFrameBuffer(), &status);

            if (status)
            {
                fits_report_error(stderr, status); /* print out any error messages */
                fits_get_errstatus(status, error_status);
                DEBUGF(INDI::Logger::DBG_ERROR, "FITS Error: %s", error_status);
                return false;
            }

            fits_close_file(fptr, &status);

            uploadFile(targetDevice, memptr, memsize, sendCapture, saveCapture /*, useSolver*/);

            free(memptr);
        }
        else
        {
            uploadFile(targetDevice, targetDevice->getFrameBuffer(), targetDevice->getFrameBufferSize(), sendCapture,
                       saveCapture);
        }
    }

    targetDevice->FramedCaptureNP.s = IPS_OK;
    IDSetNumber(&targetDevice->FramedCaptureNP, nullptr);

    if (autoLoop)
    {
        PrimaryDetector.FramedCaptureN[0].value = CaptureTime;
        PrimaryDetector.FramedCaptureNP.s       = IPS_BUSY;
        if (StartCapture(CaptureTime))
            PrimaryDetector.FramedCaptureNP.s = IPS_BUSY;
        else
        {
            DEBUG(INDI::Logger::DBG_DEBUG, "Autoloop: PrimaryDetector Capture Error!");
            PrimaryDetector.FramedCaptureNP.s = IPS_ALERT;
        }

        IDSetNumber(&PrimaryDetector.FramedCaptureNP, nullptr);
    }

    return true;
}

bool INDI::Detector::uploadFile(DetectorDevice *targetDevice, const void *fitsData, size_t totalBytes, bool sendCapture,
                           bool saveCapture /*, bool useSolver*/)
{

    DEBUGF(INDI::Logger::DBG_DEBUG, "Uploading file. Ext: %s, Size: %d, sendCapture? %s, saveCapture? %s",
           targetDevice->getCaptureExtension(), totalBytes, sendCapture ? "Yes" : "No", saveCapture ? "Yes" : "No");

    if (saveCapture)
    {
        targetDevice->FitsB.blob    = (unsigned char *)fitsData;
        targetDevice->FitsB.bloblen = totalBytes;
        snprintf(targetDevice->FitsB.format, MAXINDIBLOBFMT, ".%s", targetDevice->getCaptureExtension());

        FILE *fp = nullptr;
        char captureFileName[MAXRBUF];

        std::string prefix = UploadSettingsT[UPLOAD_PREFIX].text;
        int maxIndex       = getFileIndex(UploadSettingsT[UPLOAD_DIR].text, UploadSettingsT[UPLOAD_PREFIX].text,
                                    targetDevice->FitsB.format);

        if (maxIndex < 0)
        {
            DEBUGF(INDI::Logger::DBG_ERROR, "Error iterating directory %s. %s", UploadSettingsT[0].text,
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

        snprintf(captureFileName, MAXRBUF, "%s/%s%s", UploadSettingsT[0].text, prefix.c_str(), targetDevice->FitsB.format);

        fp = fopen(captureFileName, "w");
        if (fp == nullptr)
        {
            DEBUGF(INDI::Logger::DBG_ERROR, "Unable to save capture file (%s). %s", captureFileName, strerror(errno));
            return false;
        }

        int n = 0;
        for (int nr = 0; nr < (int)targetDevice->FitsB.bloblen; nr += n)
            n = fwrite((static_cast<char *>(targetDevice->FitsB.blob) + nr), 1, targetDevice->FitsB.bloblen - nr, fp);

        fclose(fp);

        // Save capture file path
        IUSaveText(&FileNameT[0], captureFileName);

        DEBUGF(INDI::Logger::DBG_SESSION, "Capture saved to %s", captureFileName);
        FileNameTP.s = IPS_OK;
        IDSetText(&FileNameTP, nullptr);
    }
    targetDevice->FitsB.blob    = (unsigned char *)fitsData;
    targetDevice->FitsB.bloblen = totalBytes;
    snprintf(targetDevice->FitsB.format, MAXINDIBLOBFMT, ".%s", targetDevice->getCaptureExtension());

    targetDevice->FitsB.size = totalBytes;
    targetDevice->FitsBP.s   = IPS_OK;

    if (sendCapture)
        IDSetBLOB(&targetDevice->FitsBP, nullptr);

    DEBUG(INDI::Logger::DBG_DEBUG, "Upload complete");

    return true;
}

void INDI::Detector::SetDetectorParams(float bw, float capfreq, float samfreq, float bps)
{
    PrimaryDetector.setBandwidth(bw);
    PrimaryDetector.setCaptureFreq(capfreq);
    PrimaryDetector.setSamplingFreq(samfreq);
    PrimaryDetector.setBPS(bps);
    CaptureParamsUpdated(bw, capfreq, samfreq, bps);
}

bool INDI::Detector::saveConfigItems(FILE *fp)
{
    DefaultDevice::saveConfigItems(fp);

    IUSaveConfigText(fp, &ActiveDeviceTP);
    IUSaveConfigSwitch(fp, &UploadSP);
    IUSaveConfigText(fp, &UploadSettingsTP);
    IUSaveConfigSwitch(fp, &TelescopeTypeSP);

    return true;
}

void INDI::Detector::getMinMax(double *min, double *max, DetectorDevice *targetDevice)
{
    int ind         = 0, i, j;
    int captureHeight = 1;
    int captureWidth  = targetDevice->getCaptureDuration() * targetDevice->getSamplingFrequency();
    double lmin = 0, lmax = 0;

    switch (targetDevice->getBPS())
    {
        case 8:
        {
            unsigned char *captureBuffer = (unsigned char *)targetDevice->getFrameBuffer();
            lmin = lmax = captureBuffer[0];

            for (i = 0; i < captureHeight; i++)
                for (j = 0; j < captureWidth; j++)
                {
                    ind = (i * captureWidth) + j;
                    if (captureBuffer[ind] < lmin)
                        lmin = captureBuffer[ind];
                    else if (captureBuffer[ind] > lmax)
                        lmax = captureBuffer[ind];
                }
        }
        break;

        case 16:
        {
            unsigned short *captureBuffer = (unsigned short *)targetDevice->getFrameBuffer();
            lmin = lmax = captureBuffer[0];

            for (i = 0; i < captureHeight; i++)
                for (j = 0; j < captureWidth; j++)
                {
                    ind = (i * captureWidth) + j;
                    if (captureBuffer[ind] < lmin)
                        lmin = captureBuffer[ind];
                    else if (captureBuffer[ind] > lmax)
                        lmax = captureBuffer[ind];
                }
        }
        break;

        case 32:
        {
            unsigned int *captureBuffer = (unsigned int *)targetDevice->getFrameBuffer();
            lmin = lmax = captureBuffer[0];

            for (i = 0; i < captureHeight; i++)
                for (j = 0; j < captureWidth; j++)
                {
                    ind = (i * captureWidth) + j;
                    if (captureBuffer[ind] < lmin)
                        lmin = captureBuffer[ind];
                    else if (captureBuffer[ind] > lmax)
                        lmax = captureBuffer[ind];
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

int INDI::Detector::getFileIndex(const char *dir, const char *prefix, const char *ext)
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
        DEBUGF(INDI::Logger::DBG_DEBUG, "Creating directory %s...", dir);
        if (_det_mkdir(dir, 0755) == -1)
            DEBUGF(INDI::Logger::DBG_ERROR, "Error creating directory %s (%s)", dir, strerror(errno));
    }

    dpdf = opendir(dir);
    if (dpdf != nullptr)
    {
        while ((epdf = readdir(dpdf)))
        {
            if (strstr(epdf->d_name, prefixIndex.c_str()))
                files.push_back(epdf->d_name);
        }
    }
    else
        return -1;

    int maxIndex = 0;

    for (int i = 0; i < (int)files.size(); i++)
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
