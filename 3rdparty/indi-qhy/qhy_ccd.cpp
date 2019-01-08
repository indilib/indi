/*
 QHY INDI Driver

 Copyright (C) 2014 Jasem Mutlaq (mutlaqja@ikarustech.com)
 Copyright (C) 2014 Zhirong Li (lzr@qhyccd.com)
 Copyright (C) 2015 Peter Polakovic (peter.polakovic@cloudmakers.eu)

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 This library is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 Lesser General Public License for more details.

 You should have received a copy of the GNU Lesser General Public
 License along with this library; if not, write to the Free Software
 Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "qhy_ccd.h"
#include "config.h"
#include <stream/streammanager.h>

#include <algorithm>
#include <math.h>

#define TEMP_THRESHOLD       0.2   /* Differential temperature threshold (C)*/
#define MAX_DEVICES          4     /* Max device cameraCount */

//NB Disable for real driver
//#define USE_SIMULATION

static int cameraCount = 0;
static QHYCCD *cameras[MAX_DEVICES];

namespace
{
static void QhyCCDCleanup()
{
    for (int i = 0; i < cameraCount; i++)
    {
        delete cameras[i];
    }

    ReleaseQHYCCDResource();
}

// Scan for the available devices
std::vector<std::string> GetDevicesIDs()
{
    char camid[MAXINDIDEVICE];
    int ret         = QHYCCD_ERROR;
    int deviceCount = 0;
    std::vector<std::string> devices;

#if defined(USE_SIMULATION)
    deviceCount = 2;
#else
    deviceCount = ScanQHYCCD();
#endif

    if (deviceCount > MAX_DEVICES)
    {
        deviceCount = MAX_DEVICES;
        IDLog("Devicescan found %d devices. The driver is compiled to support only up to %d devices.",
               deviceCount, MAX_DEVICES);
    }

    for (int i = 0; i < deviceCount; i++)
    {
        memset(camid, '\0', MAXINDIDEVICE);

#if defined(USE_SIMULATION)
        ret = QHYCCD_SUCCESS;
        snprintf(camid, MAXINDIDEVICE, "Model %d", i + 1);
#else
        ret = GetQHYCCDId(i, camid);
#endif
        if (ret == QHYCCD_SUCCESS)
        {
            devices.push_back(std::string(camid));
        }
        else
        {
            IDLog("#%d GetQHYCCDId error (%d)\n", i, ret);
        }
    }

    return devices;
}
}

void ISInit()
{
    static bool isInit = false;

    if (isInit)
        return;

    for (int i = 0; i < MAX_DEVICES; ++i)
        cameras[i] = nullptr;

#if !defined(USE_SIMULATION)
    int ret = InitQHYCCDResource();

    if (ret != QHYCCD_SUCCESS)
    {
        IDLog("Init QHYCCD SDK failed (%d)\n", ret);
        isInit = true;
        return;
    }
#endif

//On OS X, Prefer embedded App location if it exists
#if defined(__APPLE__)
    char driverSupportPath[128];
        if (getenv("INDIPREFIX") != nullptr)
        sprintf(driverSupportPath, "%s/Contents/Resources", getenv("INDIPREFIX"));
    else
        strncpy(driverSupportPath, "/usr/local/lib/indi", 128);
    strncat(driverSupportPath, "/DriverSupport/qhy", 128);
    IDLog("QHY firmware path: %s\n", driverSupportPath);
    OSXInitQHYCCDFirmware(driverSupportPath);
    // Wait a bit before calling GetDeviceIDs on MacOS
    usleep(2000000);
#endif

    std::vector<std::string> devices = GetDevicesIDs();

    cameraCount = static_cast<int>(devices.size());
    for (int i = 0; i < cameraCount; i++)
    {
        cameras[i] = new QHYCCD(devices[i].c_str());
    }
    if (cameraCount > 0)
    {
        atexit(QhyCCDCleanup);
        isInit = true;
    }
}

void ISGetProperties(const char *dev)
{
    ISInit();

    if (cameraCount == 0)
    {
        IDMessage(nullptr, "No QHY cameras detected. Power on?");
        return;
    }

    for (int i = 0; i < cameraCount; i++)
    {
        QHYCCD *camera = cameras[i];
        if (dev == nullptr || !strcmp(dev, camera->name))
        {
            camera->ISGetProperties(dev);
            if (dev != nullptr)
                break;
        }
    }
}

void ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int num)
{
    ISInit();
    for (int i = 0; i < cameraCount; i++)
    {
        QHYCCD *camera = cameras[i];
        if (dev == nullptr || !strcmp(dev, camera->name))
        {
            camera->ISNewSwitch(dev, name, states, names, num);
            if (dev != nullptr)
                break;
        }
    }
}

void ISNewText(const char *dev, const char *name, char *texts[], char *names[], int num)
{
    ISInit();
    for (int i = 0; i < cameraCount; i++)
    {
        QHYCCD *camera = cameras[i];
        if (dev == nullptr || !strcmp(dev, camera->name))
        {
            camera->ISNewText(dev, name, texts, names, num);
            if (dev != nullptr)
                break;
        }
    }
}

void ISNewNumber(const char *dev, const char *name, double values[], char *names[], int num)
{
    ISInit();
    for (int i = 0; i < cameraCount; i++)
    {
        QHYCCD *camera = cameras[i];
        if (dev == nullptr || !strcmp(dev, camera->name))
        {
            camera->ISNewNumber(dev, name, values, names, num);
            if (dev != nullptr)
                break;
        }
    }
}

void ISNewBLOB(const char *dev, const char *name, int sizes[], int blobsizes[], char *blobs[], char *formats[],
               char *names[], int n)
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

void ISSnoopDevice(XMLEle *root)
{
    ISInit();

    for (int i = 0; i < cameraCount; i++)
    {
        QHYCCD *camera = cameras[i];
        camera->ISSnoopDevice(root);
    }
}

QHYCCD::QHYCCD(const char *name) : FilterInterface(this)
{
    HasUSBTraffic = false;
    HasUSBSpeed   = false;
    HasGain       = false;
    HasOffset     = false;
    HasFilters    = false;
    coolerEnabled = false;

    snprintf(this->name, MAXINDINAME, "QHY CCD %.15s", name);
    snprintf(this->camid, MAXINDINAME, "%s", name);
    setDeviceName(this->name);

    setVersion(INDI_QHY_VERSION_MAJOR, INDI_QHY_VERSION_MINOR);

    m_QHYLogCallback = [this](const std::string &message) { logQHYMessages(message); };

    // We only want to log to the function above
    EnableQHYCCDLogFile(false);
    EnableQHYCCDMessage(false);

    // Set verbose level to Error/Fatal only by default
    SetQHYCCDLogLevel(2);

    sim = false;
}

const char *QHYCCD::getDefaultName()
{
    return static_cast<const char *>("QHY CCD");
}

bool QHYCCD::initProperties()
{
    INDI::CCD::initProperties();
    INDI::FilterInterface::initProperties(FILTER_TAB);

    FilterSlotN[0].min = 1;
    FilterSlotN[0].max = 9;

    // CCD Cooler Switch
    IUFillSwitch(&CoolerS[0], "COOLER_ON", "On", ISS_ON);
    IUFillSwitch(&CoolerS[1], "COOLER_OFF", "Off", ISS_OFF);
    IUFillSwitchVector(&CoolerSP, CoolerS, 2, getDeviceName(), "CCD_COOLER", "Cooler", MAIN_CONTROL_TAB, IP_RW,
                       ISR_1OFMANY, 0, IPS_IDLE);

    // CCD Regulation power
    IUFillNumber(&CoolerN[0], "CCD_COOLER_VALUE", "Cooling Power (%)", "%+06.2f", 0., 1., .2, 0.0);
    IUFillNumberVector(&CoolerNP, CoolerN, 1, getDeviceName(), "CCD_COOLER_POWER", "Cooling Power", MAIN_CONTROL_TAB,
                       IP_RO, 60, IPS_IDLE);

    // CCD Gain
    IUFillNumber(&GainN[0], "GAIN", "Gain", "%3.0f", 0, 100, 1, 11);
    IUFillNumberVector(&GainNP, GainN, 1, getDeviceName(), "CCD_GAIN", "Gain", MAIN_CONTROL_TAB, IP_RW, 60, IPS_IDLE);

    // CCD Offset
    IUFillNumber(&OffsetN[0], "Offset", "Offset", "%3.0f", 0, 0, 1, 0);
    IUFillNumberVector(&OffsetNP, OffsetN, 1, getDeviceName(), "CCD_OFFSET", "Offset", MAIN_CONTROL_TAB, IP_RW, 60,
                       IPS_IDLE);

    // USB Speed
    IUFillNumber(&SpeedN[0], "Speed", "Speed", "%3.0f", 0, 0, 1, 0);
    IUFillNumberVector(&SpeedNP, SpeedN, 1, getDeviceName(), "USB_SPEED", "USB Speed", MAIN_CONTROL_TAB, IP_RW, 60,
                       IPS_IDLE);

    // USB Traffic
    IUFillNumber(&USBTrafficN[0], "Speed", "Speed", "%3.0f", 0, 0, 1, 0);
    IUFillNumberVector(&USBTrafficNP, USBTrafficN, 1, getDeviceName(), "USB_TRAFFIC", "USB Traffic", MAIN_CONTROL_TAB,
                       IP_RW, 60, IPS_IDLE);

    addAuxControls();

    setDriverInterface(getDriverInterface() | FILTER_INTERFACE);

    return true;
}

void QHYCCD::ISGetProperties(const char *dev)
{
    INDI::CCD::ISGetProperties(dev);

    if (isConnected())
    {
        if (HasCooler())
        {
            defineSwitch(&CoolerSP);
            defineNumber(&CoolerNP);
        }

        if (HasUSBSpeed)
            defineNumber(&SpeedNP);

        if (HasGain)
            defineNumber(&GainNP);

        if (HasOffset)
            defineNumber(&OffsetNP);

        if (HasFilters)
        {
            //Define the Filter Slot and name properties
            defineNumber(&FilterSlotNP);
            if (FilterNameT != nullptr)
                defineText(FilterNameTP);
        }

        if (HasUSBTraffic)
            defineNumber(&USBTrafficNP);
    }
}

bool QHYCCD::updateProperties()
{
    // Define parent class properties
    INDI::CCD::updateProperties();

    if (isConnected())
    {
        if (HasCooler())
        {
            defineSwitch(&CoolerSP);
            defineNumber(&CoolerNP);

            temperatureID = IEAddTimer(POLLMS, QHYCCD::updateTemperatureHelper, this);
        }

        double min=0,max=0,step=0;
        if (HasUSBSpeed)
        {
            if (sim)
            {
                SpeedN[0].min   = 1;
                SpeedN[0].max   = 5;
                SpeedN[0].step  = 1;
                SpeedN[0].value = 1;
            }
            else
            {
                int ret = GetQHYCCDParamMinMaxStep(camhandle, CONTROL_SPEED, &min, &max, &step);
                if (ret == QHYCCD_SUCCESS)
                {
                    SpeedN[0].min  = min;
                    SpeedN[0].max  = max;
                    SpeedN[0].step = step;
                }

                SpeedN[0].value = GetQHYCCDParam(camhandle, CONTROL_SPEED);

                LOGF_INFO("USB Speed Settings: Value: %.f Min: %.f Max: .fg Step %.f", SpeedN[0].value,
                       SpeedN[0].min, SpeedN[0].max, SpeedN[0].step);
            }

            defineNumber(&SpeedNP);
        }

        if (HasGain)
        {
            if (sim)
            {
                GainN[0].min   = 0;
                GainN[0].max   = 100;
                GainN[0].step  = 10;
                GainN[0].value = 50;
            }
            else
            {
                int ret = GetQHYCCDParamMinMaxStep(camhandle, CONTROL_GAIN, &min, &max, &step);
                if (ret == QHYCCD_SUCCESS)
                {
                    GainN[0].min  = min;
                    GainN[0].max  = max;
                    GainN[0].step = step;
                }
                GainN[0].value = GetQHYCCDParam(camhandle, CONTROL_GAIN);

                LOGF_INFO("Gain Settings: Value: %.3f Min: %.3f Max: %.3f Step %.3f", GainN[0].value, GainN[0].min,
                       GainN[0].max, GainN[0].step);
            }

            defineNumber(&GainNP);
        }

        if (HasOffset)
        {
            if (sim)
            {
                OffsetN[0].min   = 1;
                OffsetN[0].max   = 10;
                OffsetN[0].step  = 1;
                OffsetN[0].value = 1;
            }
            else
            {
                int ret = GetQHYCCDParamMinMaxStep(camhandle, CONTROL_OFFSET, &min, &max, &step);
                if (ret == QHYCCD_SUCCESS)
                {
                    OffsetN[0].min  = min;
                    OffsetN[0].max  = max;
                    OffsetN[0].step = step;
                }
                OffsetN[0].value = GetQHYCCDParam(camhandle, CONTROL_OFFSET);

                LOGF_INFO("Offset Settings: Value: %.3f Min: %.3f Max: %.3f Step %.3f", OffsetN[0].value,
                       OffsetN[0].min, OffsetN[0].max, OffsetN[0].step);
            }

            //Define the Offset
            defineNumber(&OffsetNP);
        }

        if (HasFilters)
        {
            INDI::FilterInterface::updateProperties();
        }

        if (HasUSBTraffic)
        {
            if (sim)
            {
                USBTrafficN[0].min   = 1;
                USBTrafficN[0].max   = 100;
                USBTrafficN[0].step  = 5;
                USBTrafficN[0].value = 20;
            }
            else
            {
                int ret = GetQHYCCDParamMinMaxStep(camhandle, CONTROL_USBTRAFFIC, &min, &max, &step);
                if (ret == QHYCCD_SUCCESS)
                {
                    USBTrafficN[0].min  = min;
                    USBTrafficN[0].max  = max;
                    USBTrafficN[0].step = step;
                }
                USBTrafficN[0].value = GetQHYCCDParam(camhandle, CONTROL_USBTRAFFIC);

                LOGF_INFO("USB Traffic Settings: Value: %.3f Min: %.3f Max: %.3f Step %.3f", USBTrafficN[0].value,
                       USBTrafficN[0].min, USBTrafficN[0].max, USBTrafficN[0].step);
            }
            defineNumber(&USBTrafficNP);
        }

        // Let's get parameters now from CCD
        setupParams();
    }
    else
    {
        if (HasCooler())
        {
            deleteProperty(CoolerSP.name);
            deleteProperty(CoolerNP.name);
        }

        if (HasUSBSpeed)
        {
            deleteProperty(SpeedNP.name);
        }

        if (HasGain)
        {
            deleteProperty(GainNP.name);
        }

        if (HasOffset)
        {
            deleteProperty(OffsetNP.name);
        }

        if (HasFilters)
        {
            INDI::FilterInterface::updateProperties();
        }

        if (HasUSBTraffic)
            deleteProperty(USBTrafficNP.name);

        RemoveTimer(timerID);
    }

    return true;
}

bool QHYCCD::Connect()
{
    unsigned int ret = 0;
    uint32_t cap;

    sim = isSimulation();

    if (sim)
    {
        cap = CCD_CAN_SUBFRAME | CCD_CAN_ABORT | CCD_CAN_BIN | CCD_HAS_COOLER | CCD_HAS_ST4_PORT;
        SetCCDCapability(cap);

        HasUSBTraffic = true;
        HasUSBSpeed   = true;
        HasGain       = true;
        HasOffset     = true;
        HasFilters    = true;

        return true;
    }

    // Query the current CCD cameras. This method makes the driver more robust and
    // it fixes a crash with the new QHC SDK when the INDI driver reopens the same camera
    // with OpenQHYCCD() without a ScanQHYCCD() call before.
    // 2017-12-15 JM: No this method ReleaseQHYCCDResource which ends up clearing handles for multiple
    // cameras
    /*std::vector<std::string> devices = GetDevicesIDs();

    // The CCD device is not available anymore
    if (std::find(devices.begin(), devices.end(), std::string(camid)) == devices.end())
    {
        LOGF_ERROR("Error: Camera %s is not connected", camid);
        return false;
    }*/
    camhandle = OpenQHYCCD(camid);

    if (camhandle != nullptr)
    {
        LOGF_INFO("Connected to %s.", camid);

        cap = CCD_CAN_ABORT | CCD_CAN_SUBFRAME | CCD_HAS_STREAMING;

        // Disable the stream mode before connecting
        ret = SetQHYCCDStreamMode(camhandle, 0);
        if (ret != QHYCCD_SUCCESS)
        {
            LOGF_ERROR("Can not disable stream mode (%d)", ret);
        }
        ret = InitQHYCCD(camhandle);
        if (ret != QHYCCD_SUCCESS)
        {
            LOGF_ERROR("Init Camera failed (%d)", ret);
            return false;
        }

        ret = IsQHYCCDControlAvailable(camhandle, CAM_MECHANICALSHUTTER);
        if (ret == QHYCCD_SUCCESS)
        {
            cap |= CCD_HAS_SHUTTER;
        }

        LOGF_DEBUG("Shutter Control: %s", cap & CCD_HAS_SHUTTER ? "True" : "False");

        ret = IsQHYCCDControlAvailable(camhandle, CONTROL_COOLER);
        if (ret == QHYCCD_SUCCESS)
        {
            cap |= CCD_HAS_COOLER;
        }

        LOGF_DEBUG("Cooler Control: %s", cap & CCD_HAS_COOLER ? "True" : "False");

        ret = IsQHYCCDControlAvailable(camhandle, CONTROL_ST4PORT);
        if (ret == QHYCCD_SUCCESS)
        {
            cap |= CCD_HAS_ST4_PORT;
        }

        LOGF_DEBUG("Guider Port Control: %s", cap & CCD_HAS_ST4_PORT ? "True" : "False");

        ret = IsQHYCCDControlAvailable(camhandle, CONTROL_SPEED);
        if (ret == QHYCCD_SUCCESS)
        {
            HasUSBSpeed = true;

            // Force a certain speed on initialization of QHY5PII-C:
            // CONTROL_SPEED = 2 - Fastest, but the camera gets stuck with long exposure times.
            // CONTROL_SPEED = 1 - This is safe with the current driver.
            // CONTROL_SPEED = 0 - This is safe, but slower than 1.
            if (isQHY5PIIC())
                SetQHYCCDParam(camhandle, CONTROL_SPEED, 1);
        }

        LOGF_DEBUG("USB Speed Control: %s", HasUSBSpeed ? "True" : "False");

        ret = IsQHYCCDControlAvailable(camhandle, CONTROL_GAIN);
        if (ret == QHYCCD_SUCCESS)
        {
            HasGain = true;
        }

        LOGF_DEBUG("Gain Control: %s", HasGain ? "True" : "False");

        ret = IsQHYCCDControlAvailable(camhandle, CONTROL_OFFSET);
        if (ret == QHYCCD_SUCCESS)
        {
            HasOffset = true;
        }

        LOGF_DEBUG("Offset Control: %s", HasOffset ? "True" : "False");

        ret = IsQHYCCDControlAvailable(camhandle, CONTROL_CFWPORT);
        if (ret == QHYCCD_SUCCESS)
        {
            HasFilters = true;
        }

        LOGF_DEBUG("Has Filters: %s", HasFilters ? "True" : "False");

        // Using software binning
        cap |= CCD_CAN_BIN;
        camxbin = 1;
        camybin = 1;

        // Always use INDI software binning
        //useSoftBin = true;


        ret = IsQHYCCDControlAvailable(camhandle,CAM_BIN1X1MODE);
        if(ret == QHYCCD_SUCCESS)
        {
            camxbin = 1;
            camybin = 1;
        }

        LOGF_DEBUG("Bin 1x1: %s", (ret == QHYCCD_SUCCESS) ? "True" : "False");

        ret &= IsQHYCCDControlAvailable(camhandle,CAM_BIN2X2MODE);
        ret &= IsQHYCCDControlAvailable(camhandle,CAM_BIN3X3MODE);
        ret &= IsQHYCCDControlAvailable(camhandle,CAM_BIN4X4MODE);

        // Only use software binning if NOT supported by hardware
        //useSoftBin = !(ret == QHYCCD_SUCCESS);

        LOGF_DEBUG("Binning Control: %s", cap & CCD_CAN_BIN ? "True" : "False");

        ret = IsQHYCCDControlAvailable(camhandle, CONTROL_USBTRAFFIC);
        if (ret == QHYCCD_SUCCESS)
        {
            HasUSBTraffic = true;
            // Force the USB traffic value to 30 on initialization of QHY5PII-C otherwise
            // the camera has poor transfer speed.
            if (isQHY5PIIC())
                SetQHYCCDParam(camhandle, CONTROL_USBTRAFFIC, 30);
        }

        LOGF_DEBUG("USB Traffic Control: %s", HasUSBTraffic ? "True" : "False");

        ret = IsQHYCCDControlAvailable(camhandle, CAM_COLOR);
        //if(ret != QHYCCD_ERROR && ret != QHYCCD_ERROR_NOTSUPPORT)
        if (ret != QHYCCD_ERROR)
        {
            if (ret == BAYER_GB)
                IUSaveText(&BayerT[2], "GBRG");
            else if (ret == BAYER_GR)
                IUSaveText(&BayerT[2], "GRBG");
            else if (ret == BAYER_BG)
                IUSaveText(&BayerT[2], "BGGR");
            else
                IUSaveText(&BayerT[2], "RGGB");

            LOGF_DEBUG("Color camera: %s", BayerT[2].text);

            cap |= CCD_HAS_BAYER;
        }

        double min=0, max=0, step=0;
        // Exposure limits in microseconds
        int ret = GetQHYCCDParamMinMaxStep(camhandle, CONTROL_EXPOSURE, &min, &max, &step);
        if (ret == QHYCCD_SUCCESS)
            PrimaryCCD.setMinMaxStep("CCD_EXPOSURE", "CCD_EXPOSURE_VALUE", min/1e6, max/1e6, step/1e6, false);
        else
            PrimaryCCD.setMinMaxStep("CCD_EXPOSURE", "CCD_EXPOSURE_VALUE", 0.001, 3600, 1, false);

        LOGF_INFO("Camera exposure limits: Min: %.6fs Max: %.3fs Step %.fs", min/1e6, max/1e6, step/1e6);


        SetCCDCapability(cap);

        /*
         * Create the imaging thread and wait for it to start
         */
        threadRequest = StateIdle;
        threadState = StateNone;
        int stat = pthread_create(&imagingThread, nullptr, &imagingHelper, this);
        if (stat != 0)
        {
            LOGF_ERROR("Error creating imaging thread (%d)",
                stat);
            return false;
        }
        pthread_mutex_lock(&condMutex);
        while (threadState == StateNone)
        {
            pthread_cond_wait(&cv, &condMutex);
        }
        pthread_mutex_unlock(&condMutex);

        return true;
    }

    LOGF_ERROR("Connecting to camera failed (%s).", camid);

    return false;
}

bool QHYCCD::Disconnect()
{
    ImageState  tState;
    LOGF_DEBUG("Closing %s...", name);

    pthread_mutex_lock(&condMutex);
    tState = threadState;
    threadRequest = StateTerminate;
    pthread_cond_signal(&cv);
    pthread_mutex_unlock(&condMutex);
    pthread_join(imagingThread, nullptr);
    tState = StateNone;
    if (isSimulation() == false)
    {
        if (tState == StateStream)
        {
            SetQHYCCDStreamMode(camhandle, 0x0);
            StopQHYCCDLive(camhandle);
        }
        else if (tState == StateExposure)
            CancelQHYCCDExposingAndReadout(camhandle);
        CloseQHYCCD(camhandle);
    }

    LOG_INFO("Camera is offline.");

    return true;
}

bool QHYCCD::setupParams()
{
    uint32_t nbuf, ret, imagew, imageh, bpp;
    double chipw, chiph, pixelw, pixelh;

    if (sim)
    {
        chipw = imagew = 1280;
        chiph = imageh = 1024;
        pixelh = pixelw = 5.4;
        bpp             = 8;
    }
    else
    {
        ret = GetQHYCCDChipInfo(camhandle, &chipw, &chiph, &imagew, &imageh, &pixelw, &pixelh, &bpp);

        /* JM: We need GetQHYCCDErrorString(ret) to get the string description of the error, please implement this in the SDK */
        if (ret != QHYCCD_SUCCESS)
        {
            LOGF_ERROR("Error: GetQHYCCDChipInfo() (%d)", ret);
            return false;
        }

        LOGF_DEBUG("GetQHYCCDChipInfo: chipW :%g chipH: %g imageW: %d imageH: %d pixelW: %g pixelH: %g bbp %d", chipw,
                    chiph, imagew, imageh, pixelw, pixelh, bpp);

        camroix      = 0;
        camroiy      = 0;
        camroiwidth  = imagew;
        camroiheight = imageh;
    }

    SetCCDParams(imagew, imageh, bpp, pixelw, pixelh);

    nbuf = PrimaryCCD.getXRes() * PrimaryCCD.getYRes() * PrimaryCCD.getBPP() / 8;
    PrimaryCCD.setFrameBufferSize(nbuf);

    Streamer->setPixelFormat(INDI_MONO);
    Streamer->setSize(imagew, imageh);
    return true;
}

int QHYCCD::SetTemperature(double temperature)
{
    // If there difference, for example, is less than 0.1 degrees, let's immediately return OK.
    if (fabs(temperature - TemperatureN[0].value) < TEMP_THRESHOLD)
        return 1;

    LOGF_DEBUG("Requested temperature is %.3f, current temperature is %.3f", temperature, TemperatureN[0].value);

    TemperatureRequest = temperature;

    // Enable cooler
    //setCooler(true);

    ControlQHYCCDTemp(camhandle,TemperatureRequest);

    return 0;
}

bool QHYCCD::StartExposure(float duration)
{
    unsigned int ret = QHYCCD_ERROR;

    if (Streamer->isBusy())
    {
        LOG_ERROR("Cannot take exposure while streaming/recording is active.");
        return false;
    }

    imageFrameType = PrimaryCCD.getFrameType();

    if (GetCCDCapability() & CCD_HAS_SHUTTER)
    {
        if (imageFrameType == INDI::CCDChip::DARK_FRAME || imageFrameType == INDI::CCDChip::BIAS_FRAME)
            ControlQHYCCDShutter(camhandle, MACHANICALSHUTTER_CLOSE);
        else
            ControlQHYCCDShutter(camhandle, MACHANICALSHUTTER_FREE);
    }

    long uSecs = duration * 1000 * 1000;
    LOGF_DEBUG("Requested exposure time is %ld us", uSecs);
    ExposureRequest = static_cast<double>(duration);
    PrimaryCCD.setExposureDuration(ExposureRequest);

    // Setting Exposure time, IF different from last exposure time.
    if (sim)
        ret = QHYCCD_SUCCESS;
    else
    {
        if (LastExposureRequestuS != uSecs)
        {
            ret = SetQHYCCDParam(camhandle, CONTROL_EXPOSURE, uSecs);

            if (ret != QHYCCD_SUCCESS)
            {
                LOGF_ERROR("Set expose time failed (%d).", ret);
                return false;
            }

            LastExposureRequestuS = uSecs;
        }
    }

    // Set binning mode
    if (sim)
        ret = QHYCCD_SUCCESS;
    else
        ret = SetQHYCCDBinMode(camhandle, camxbin, camybin);
    if (ret != QHYCCD_SUCCESS)
    {
        LOGF_INFO("Set QHYCCD Bin mode failed (%d)", ret);
        return false;
    }

    LOGF_DEBUG("SetQHYCCDBinMode (%dx%d).", camxbin, camybin);

    // Set Region of Interest (ROI)
    if (sim)
        ret = QHYCCD_SUCCESS;
    else
        ret = SetQHYCCDResolution(camhandle, camroix, camroiy, camroiwidth, camroiheight);
    if (ret != QHYCCD_SUCCESS)
    {
        LOGF_INFO("Set QHYCCD ROI resolution (%d,%d) (%d,%d) failed (%d)", camroix, camroiy,
               camroiwidth, camroiheight, ret);
        return false;
    }

    LOGF_DEBUG("SetQHYCCDResolution camroix %d camroiy %d camroiwidth %d camroiheight %d", camroix,
           camroiy, camroiwidth, camroiheight);

    // Start to expose the frame
    if (sim)
        ret = QHYCCD_SUCCESS;
    else
        ret = ExpQHYCCDSingleFrame(camhandle);
    if (ret == QHYCCD_ERROR)
    {
        LOGF_INFO("Begin QHYCCD expose failed (%d)", ret);
        return false;
    }

    gettimeofday(&ExpStart, nullptr);
    LOGF_DEBUG("Taking a %.5f seconds frame...", ExposureRequest);

    InExposure = true;
    pthread_mutex_lock(&condMutex);
    threadRequest = StateExposure;
    pthread_cond_signal(&cv);
    pthread_mutex_unlock(&condMutex);

    return true;
}

bool QHYCCD::AbortExposure()
{
    if (!InExposure || sim)
    {
        InExposure = false;
        return true;
    }

    LOG_DEBUG("Aborting camera exposure...");

    pthread_mutex_lock(&condMutex);
    threadRequest = StateAbort;
    pthread_cond_signal(&cv);
    while (threadState == StateExposure)
    {
        pthread_cond_wait(&cv, &condMutex);
    }
    pthread_mutex_unlock(&condMutex);

    if (std::string(camid) != "QHY5-M-")
    {
        int rc = CancelQHYCCDExposingAndReadout(camhandle);
        if (rc == QHYCCD_SUCCESS)
        {
            InExposure = false;
            LOG_INFO("Exposure aborted.");
            return true;
        }
        else
            LOGF_ERROR("Abort exposure failed (%d)", rc);

        return false;
    }
    else
    {
        InExposure = false;
        LOG_INFO("Exposure aborted.");
        return true;
    }
}

bool QHYCCD::UpdateCCDFrame(int x, int y, int w, int h)
{
    camroix      = x / PrimaryCCD.getBinX();
    camroiy      = y / PrimaryCCD.getBinY();
    camroiwidth  = w / PrimaryCCD.getBinX();
    camroiheight = h / PrimaryCCD.getBinY();

    LOGF_DEBUG("Final binned (%dx%d) image area is (%d, %d), (%d, %d)", PrimaryCCD.getBinX(), PrimaryCCD.getBinY(),
           camroix, camroiy, camroiwidth, camroiheight);

    // Set UNBINNED coords
    PrimaryCCD.setFrame(x, y, w, h);
    // Total bytes required for image buffer
    uint32_t nbuf = (camroiwidth * camroiheight * PrimaryCCD.getBPP() / 8);
    PrimaryCCD.setFrameBufferSize(nbuf);

    // Streamer is always updated with BINNED size.
    Streamer->setSize(camroiwidth, camroiheight);
    return true;
}

bool QHYCCD::UpdateCCDBin(int hor, int ver)
{
    int ret = QHYCCD_ERROR;

    if (hor != ver)
    {
        LOG_ERROR("Invalid binning mode. Asymmetrical binning not supported.");
        return false;
    }

    if (hor == 3)
    {
        LOG_ERROR("Invalid binning mode. Only 1x1, 2x2, and 4x4 binning modes supported.");
        return false;
    }

    if (hor == 1 && ver == 1)
    {
        ret = IsQHYCCDControlAvailable(camhandle, CAM_BIN1X1MODE);
    }
    else if (hor == 2 && ver == 2)
    {
        ret = IsQHYCCDControlAvailable(camhandle, CAM_BIN2X2MODE);
    }
    else if (hor == 3 && ver == 3)
    {
        ret = IsQHYCCDControlAvailable(camhandle, CAM_BIN3X3MODE);
    }
    else if (hor == 4 && ver == 4)
    {
        ret = IsQHYCCDControlAvailable(camhandle, CAM_BIN4X4MODE);
    }

    // Binning ALWAYS succeeds
#if 0
    if (ret != QHYCCD_SUCCESS)
    {
        useSoftBin = true;
    }

    // We always use software binning so QHY binning is always at 1x1
    camxbin = 1;
    camybin = 1;
#endif

    if (ret != QHYCCD_SUCCESS)
    {
        LOGF_ERROR("%dx%d binning is not supported.", hor, ver);
        return false;
    }

    camxbin = hor;
    camybin = ver;

    PrimaryCCD.setBin(hor, ver);

    return UpdateCCDFrame(PrimaryCCD.getSubX(), PrimaryCCD.getSubY(), PrimaryCCD.getSubW(), PrimaryCCD.getSubH());
}

double QHYCCD::calcTimeLeft()
{
    double timesince;
    double timeleft;
    struct timeval now;
    gettimeofday(&now, nullptr);

    timesince = static_cast<double>(now.tv_sec * 1000.0 + now.tv_usec / 1000) -
                static_cast<double>(ExpStart.tv_sec * 1000.0 + ExpStart.tv_usec / 1000);
    timesince = timesince / 1000;

    timeleft = ExposureRequest - timesince;
    return timeleft;
}

/* Downloads the image from the CCD. */
int QHYCCD::grabImage()
{
    if (sim)
    {
        uint8_t *image = PrimaryCCD.getFrameBuffer();
        int width      = PrimaryCCD.getSubW() / PrimaryCCD.getBinX() * PrimaryCCD.getBPP() / 8;
        int height     = PrimaryCCD.getSubH() / PrimaryCCD.getBinY();

        for (int i = 0; i < height; i++)
            for (int j = 0; j < width; j++)
                image[i * width + j] = rand() % 255;
    }
    else
    {
        uint32_t ret, w, h, bpp, channels;

        LOG_DEBUG("GetQHYCCDSingleFrame Blocking read call.");
        ret = GetQHYCCDSingleFrame(camhandle, &w, &h, &bpp, &channels, PrimaryCCD.getFrameBuffer());
        LOG_DEBUG("GetQHYCCDSingleFrame Blocking read call complete.");

        if (ret != QHYCCD_SUCCESS)
        {
            LOGF_ERROR("GetQHYCCDSingleFrame error (%d)", ret);
            PrimaryCCD.setExposureFailed();
            return -1;
        }
    }

    // Perform software binning if necessary
    //if (useSoftBin)
    //    PrimaryCCD.binFrame();

    if (ExposureRequest > POLLMS * 5)
        LOG_INFO("Download complete.");
    else
        LOG_DEBUG("Download complete.");

    ExposureComplete(&PrimaryCCD);

    return 0;
}

//void QHYCCD::TimerHit()
//{
//    if (isConnected() == false)
//        return;

//    if (InExposure)
//    {
//        long timeleft = calcTimeLeft();

//        if (timeleft < 1.0)
//        {
//            if (timeleft > 0.25)
//            {
//                //  a quarter of a second or more
//                //  just set a tighter timer
//                SetTimer(250);
//            }
//            else
//            {
//                if (timeleft > 0.07)
//                {
//                    //  use an even tighter timer
//                    SetTimer(50);
//                }
//                else
//                {
//                    /* We're done exposing */
//                    LOG_DEBUG("Exposure done, downloading image...");
//                    // Don't spam the session log unless it is a long exposure > 5 seconds
//                    if (ExposureRequest > POLLMS * 5)
//                        LOG_INFO("Exposure done, downloading image...");

//                    PrimaryCCD.setExposureLeft(0);
//                    InExposure = false;

//                    // grab and save image
//                    grabImage();
//                }
//            }
//        }
//        else
//        {
//            PrimaryCCD.setExposureLeft(timeleft);
//            SetTimer(POLLMS);
//        }
//    }
//}

IPState QHYCCD::GuideNorth(uint32_t ms)
{
    ControlQHYCCDGuide(camhandle, 1, ms);
    return IPS_OK;
}

IPState QHYCCD::GuideSouth(uint32_t ms)
{
    ControlQHYCCDGuide(camhandle, 2, ms);
    return IPS_OK;
}

IPState QHYCCD::GuideEast(uint32_t ms)
{
    ControlQHYCCDGuide(camhandle, 0, ms);
    return IPS_OK;
}

IPState QHYCCD::GuideWest(uint32_t ms)
{
    ControlQHYCCDGuide(camhandle, 3, ms);
    return IPS_OK;
}

bool QHYCCD::SelectFilter(int position)
{
    char targetpos = 0;
    char currentpos[64]={0};
    int checktimes = 0;
    int ret = 0;

    if (sim)
        ret = QHYCCD_SUCCESS;
    else
    {
        // JM: THIS WILL CRASH! I am using another method with same result!
        //sprintf(&targetpos,"%d",position - 1);
        targetpos = '0' + (position - 1);
        ret       = SendOrder2QHYCCDCFW(camhandle, &targetpos, 1);
    }

    if (ret == QHYCCD_SUCCESS)
    {
        while (checktimes < 90)
        {
            ret = GetQHYCCDCFWStatus(camhandle, currentpos);
            if (ret == QHYCCD_SUCCESS)
            {
                if ((targetpos + 1) == currentpos[0])
                {
                    break;
                }
            }
            checktimes++;
        }

        CurrentFilter = position;
        SelectFilterDone(position);
        LOGF_DEBUG("%s: Filter changed to %d", camid, position);
        return true;
    }
    else
        LOGF_ERROR("Changing filter failed (%d)", ret);

    return false;
}

int QHYCCD::QueryFilter()
{
    return CurrentFilter;
}

bool QHYCCD::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        // Cooler controler
        if (!strcmp(name, CoolerSP.name))
        {
            if (IUUpdateSwitch(&CoolerSP, states, names, n) < 0)
            {
                CoolerSP.s = IPS_ALERT;
                IDSetSwitch(&CoolerSP, nullptr);
                return true;
            }

            bool enabled = (CoolerS[COOLER_ON].s == ISS_ON);

            // If user turns on cooler, but the requested temperature is higher than current temperature
            // then we set temperature to zero degrees. If that was still higher than current temperature
            // we return an error
            if (enabled && TemperatureRequest > TemperatureN[0].value)
            {
                TemperatureRequest = 0;
                // If current temperature is still lower than zero, then we shouldn't risk
                // setting temperature to any arbitrary value. Instead, we report an error and ask
                // user to explicitly set the requested temperature.
                if (TemperatureRequest > TemperatureN[0].value)
                {
                    CoolerS[COOLER_ON].s = ISS_OFF;
                    CoolerS[COOLER_OFF].s = ISS_OFF;
                    CoolerSP.s = IPS_ALERT;
                    LOGF_WARN("Cannot manually activate cooler since current temperature is %.2f. To activate cooler, request a lower temperature.", TemperatureN[0].value);
                    IDSetSwitch(&CoolerSP, nullptr);
                    return true;
                }

                SetTemperature(0);
                return true;
            }

            return activateCooler(enabled);
        }
    }

    //  Nobody has claimed this, so, ignore it
    return INDI::CCD::ISNewSwitch(dev, name, states, names, n);
}

bool QHYCCD::ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        //  This is for our device
        //  Now lets see if it's something we process here
        if (strcmp(name, FilterNameTP->name) == 0)
        {
            INDI::FilterInterface::processText(dev, name, texts, names, n);
            return true;
        }
    }

    return INDI::CCD::ISNewText(dev, name, texts, names, n);
}

bool QHYCCD::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    //  first check if it's for our device
    //IDLog("INDI::CCD::ISNewNumber %s\n",name);
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        if (strcmp(name, FilterSlotNP.name) == 0)
        {
            INDI::FilterInterface::processNumber(dev, name, values, names, n);
            return true;
        }

        if (strcmp(name, GainNP.name) == 0)
        {
            IUUpdateNumber(&GainNP, values, names, n);
            GainRequest = GainN[0].value;
            if (fabs(LastGainRequest - GainRequest) > 0.001)
            {
                SetQHYCCDParam(camhandle, CONTROL_GAIN, GainN[0].value);
                LastGainRequest = GainRequest;
            }
            LOGF_INFO("Current %s value %f", GainNP.name, GainN[0].value);
            GainNP.s = IPS_OK;
            IDSetNumber(&GainNP, nullptr);
            return true;
        }

        if (strcmp(name, OffsetNP.name) == 0)
        {
            IUUpdateNumber(&OffsetNP, values, names, n);
            SetQHYCCDParam(camhandle, CONTROL_OFFSET, OffsetN[0].value);
            LOGF_INFO("Current %s value %f", OffsetNP.name, OffsetN[0].value);
            OffsetNP.s = IPS_OK;
            IDSetNumber(&OffsetNP, nullptr);
            saveConfig(true, OffsetNP.name);
            return true;
        }

        if (strcmp(name, SpeedNP.name) == 0)
        {
            IUUpdateNumber(&SpeedNP, values, names, n);
            SetQHYCCDParam(camhandle, CONTROL_SPEED, SpeedN[0].value);
            LOGF_INFO("Current %s value %f", SpeedNP.name, SpeedN[0].value);
            SpeedNP.s = IPS_OK;
            IDSetNumber(&SpeedNP, nullptr);
            saveConfig(true, SpeedNP.name);
            return true;
        }

        if (strcmp(name, USBTrafficNP.name) == 0)
        {
            IUUpdateNumber(&USBTrafficNP, values, names, n);
            SetQHYCCDParam(camhandle, CONTROL_USBTRAFFIC, USBTrafficN[0].value);
            LOGF_INFO("Current %s value %f", USBTrafficNP.name, USBTrafficN[0].value);
            USBTrafficNP.s = IPS_OK;
            IDSetNumber(&USBTrafficNP, nullptr);
            saveConfig(true, USBTrafficNP.name);
            return true;
        }
    }

    return INDI::CCD::ISNewNumber(dev, name, values, names, n);
}

bool QHYCCD::activateCooler(bool enable)
{
    IUResetSwitch(&CoolerSP);
    if (enable)
    {
        if (TemperatureRequest < TemperatureN[0].value)
        {
            if (CoolerSP.s != IPS_BUSY)
                LOG_INFO("Camera cooler is on.");

            CoolerS[COOLER_ON].s = ISS_ON;
            CoolerS[COOLER_OFF].s = ISS_OFF;
            CoolerSP.s = IPS_BUSY;
        }
        else
        {
            CoolerS[COOLER_ON].s = ISS_OFF;
            CoolerS[COOLER_OFF].s = ISS_ON;
            CoolerSP.s = IPS_IDLE;
            LOG_WARN("Cooler cannot be activated manually. Set a lower temperature to activate it.");
            IDSetSwitch(&CoolerSP, nullptr);
            return false;
        }
    }
    else if (enable == false)
    {
        int rc = SetQHYCCDParam(camhandle, CONTROL_MANULPWM, 0);
        if (rc != QHYCCD_SUCCESS)
        {
            CoolerS[COOLER_ON].s = ISS_ON;
            CoolerS[COOLER_OFF].s = ISS_OFF;
            CoolerSP.s = IPS_ALERT;
            LOGF_ERROR("Failed to warm camera (%d).", rc);
            IDSetSwitch(&CoolerSP, nullptr);
            return false;
        }

        CoolerS[COOLER_ON].s = ISS_OFF;
        CoolerS[COOLER_OFF].s = ISS_ON;
        CoolerSP.s = IPS_IDLE;
        LOG_INFO("Camera is warming up...");
    }

    IDSetSwitch(&CoolerSP, nullptr);
    return true;
}

bool QHYCCD::isQHY5PIIC()
{
    return std::string(camid, 9) == "QHY5PII-C";
}

void QHYCCD::updateTemperatureHelper(void *p)
{
    if (static_cast<QHYCCD *>(p)->isConnected())
        static_cast<QHYCCD *>(p)->updateTemperature();
}

void QHYCCD::updateTemperature()
{
    double ccdtemp = 0, coolpower = 0;
    double nextPoll = POLLMS;

    if (sim)
    {
        ccdtemp = TemperatureN[0].value;
        if (TemperatureN[0].value < TemperatureRequest)
            ccdtemp += TEMP_THRESHOLD;
        else if (TemperatureN[0].value > TemperatureRequest)
            ccdtemp -= TEMP_THRESHOLD;

        coolpower = 128;
    }
    else
    {
        ccdtemp   = GetQHYCCDParam(camhandle, CONTROL_CURTEMP);
        coolpower = GetQHYCCDParam(camhandle, CONTROL_CURPWM);

        // In previous SDKs, we have to call this _every_ second to set the temperature
        // but shouldn't be required for SDK v3 and above.
        //ControlQHYCCDTemp(camhandle, TemperatureRequest);
    }

    // No need to spam to log
    if (fabs(ccdtemp - TemperatureN[0].value) > 0.001 || fabs(CoolerN[0].value - (coolpower / 255.0 * 100)) > 0.001)
    {
        LOGF_DEBUG("CCD Temp: %g CCD RAW Cooling Power: %g, CCD Cooling percentage: %g", ccdtemp,
               coolpower, coolpower / 255.0 * 100);
    }

    TemperatureN[0].value = ccdtemp;
    CoolerN[0].value      = coolpower / 255.0 * 100;

//    if (coolpower > 0 && CoolerS[0].s == ISS_OFF)
//    {
//        CoolerNP.s   = IPS_BUSY;
//        CoolerSP.s   = IPS_OK;
//        CoolerS[0].s = ISS_ON;
//        CoolerS[1].s = ISS_OFF;
//        IDSetSwitch(&CoolerSP, nullptr);
//    }
//    else if (coolpower <= 0 && CoolerS[0].s == ISS_ON)
//    {
//        CoolerNP.s   = IPS_IDLE;
//        CoolerSP.s   = IPS_IDLE;
//        CoolerS[0].s = ISS_OFF;
//        CoolerS[1].s = ISS_ON;
//        IDSetSwitch(&CoolerSP, nullptr);
//    }

    if (TemperatureNP.s == IPS_BUSY && fabs(TemperatureN[0].value - TemperatureRequest) <= TEMP_THRESHOLD)
    {
        TemperatureN[0].value = TemperatureRequest;
        TemperatureNP.s       = IPS_OK;
    }

    /*
    //we need call ControlQHYCCDTemp every second to control temperature
    if (TemperatureNP.s == IPS_BUSY)
        nextPoll = TEMPERATURE_BUSY_MS;
*/

    IDSetNumber(&TemperatureNP, nullptr);
    IDSetNumber(&CoolerNP, nullptr);

    temperatureID = IEAddTimer(nextPoll, QHYCCD::updateTemperatureHelper, this);
}

bool QHYCCD::saveConfigItems(FILE *fp)
{
    INDI::CCD::saveConfigItems(fp);

    if (HasFilters)
    {
        INDI::FilterInterface::saveConfigItems(fp);
    }

    if (HasGain)
        IUSaveConfigNumber(fp, &GainNP);

    if (HasOffset)
        IUSaveConfigNumber(fp, &OffsetNP);

    if (HasUSBSpeed)
        IUSaveConfigNumber(fp, &SpeedNP);

    if (HasUSBTraffic)
        IUSaveConfigNumber(fp, &USBTrafficNP);

    return true;
}

bool QHYCCD::StartStreaming()
{
    ExposureRequest = 1.0 / Streamer->getTargetFPS();

    // N.B. There is no corresponding value for GBGR. It is odd that QHY selects this as the default as no one seems to process it
    const std::map<const char *, INDI_PIXEL_FORMAT> formats = { { "GBGR", INDI_MONO },
                                                                { "GRGB", INDI_BAYER_GRBG },
                                                                { "BGGR", INDI_BAYER_BGGR },
                                                                { "RGGB", INDI_BAYER_RGGB } };

    INDI_PIXEL_FORMAT qhyFormat = INDI_MONO;
    if (BayerT[2].text && formats.count(BayerT[2].text) != 0)
        qhyFormat = formats.at(BayerT[2].text);

    Streamer->setPixelFormat(qhyFormat, PrimaryCCD.getBPP());

    double uSecs = static_cast<long>(ExposureRequest * 950000.0);
    SetQHYCCDParam(camhandle, CONTROL_EXPOSURE, uSecs);
    SetQHYCCDStreamMode(camhandle, 0x01);
    BeginQHYCCDLive(camhandle);

    pthread_mutex_lock(&condMutex);
    threadRequest = StateStream;
    pthread_cond_signal(&cv);
    pthread_mutex_unlock(&condMutex);

    return true;
}

bool QHYCCD::StopStreaming()
{
    pthread_mutex_lock(&condMutex);
    threadRequest = StateAbort;
    pthread_cond_signal(&cv);
    while (threadState == StateStream)
    {
        pthread_cond_wait(&cv, &condMutex);
    }
    pthread_mutex_unlock(&condMutex);
    SetQHYCCDStreamMode(camhandle, 0x0);
    StopQHYCCDLive(camhandle);

    return true;
}

void *QHYCCD::imagingHelper(void *context)
{
    return static_cast<QHYCCD *>(context)->imagingThreadEntry();
}

/*
 * A dedicated thread is used for handling streaming video and image
 * exposures because the operations take too much time to be done
 * as part of a timer call-back: there is one timer for the entire
 * process, which must handle events for all ASI cameras
 */
void *QHYCCD::imagingThreadEntry()
{
    pthread_mutex_lock(&condMutex);
    threadState = StateIdle;
    pthread_cond_signal(&cv);
    while (true)
    {
        while (threadRequest == StateIdle)
        {
            pthread_cond_wait(&cv, &condMutex);
        }
        threadState = threadRequest;
        if (threadRequest == StateExposure)
        {
            getExposure();
        }
        else if (threadRequest == StateStream)
        {
            streamVideo();
        }
        else if (threadRequest == StateRestartExposure)
        {
            threadRequest = StateIdle;
            pthread_mutex_unlock(&condMutex);
            StartExposure(ExposureRequest);
            pthread_mutex_lock(&condMutex);
        }
        else if (threadRequest == StateTerminate)
        {
            break;
        }
        else
        {
            threadRequest = StateIdle;
            pthread_cond_signal(&cv);
        }
        threadState = StateIdle;
    }
    threadState = StateTerminated;
    pthread_cond_signal(&cv);
    pthread_mutex_unlock(&condMutex);

    return nullptr;
}

void QHYCCD::streamVideo()
{
    uint32_t ret = 0, w, h, bpp, channels;

    while (threadRequest == StateStream)
    {
        pthread_mutex_unlock(&condMutex);

        if ((ret = GetQHYCCDLiveFrame(camhandle, &w, &h, &bpp, &channels, PrimaryCCD.getFrameBuffer())) == QHYCCD_SUCCESS)
            Streamer->newFrame(PrimaryCCD.getFrameBuffer(), PrimaryCCD.getFrameBufferSize());

        pthread_mutex_lock(&condMutex);
    }
}

void QHYCCD::getExposure()
{
    pthread_mutex_unlock(&condMutex);
    usleep(10000);
    pthread_mutex_lock(&condMutex);

    while (threadRequest == StateExposure)
    {
        pthread_mutex_unlock(&condMutex);
        /*
         * Check the status every second until the time left is
         * about one second, after which decrease the poll interval
         */
        double timeLeft = calcTimeLeft();
        uint32_t uSecs=0;
        if (timeLeft > 1.1)
        {
            /*
             * For expsoures with more than a second left try
             * to keep the displayed "exposure left" value at
             * a full second boundary, which keeps the
             * count down neat
             */
            double fraction = timeLeft - static_cast<int>(timeLeft);
            if (fraction >= 0.005)
            {
                uSecs = static_cast<uint32_t>(fraction * 1000000.0);
            }
            else
            {
                uSecs = 1000000;
            }
        }
        else
        {
            uSecs = 10000;
        }

        if (timeLeft >= 0.0049)
        {
            PrimaryCCD.setExposureLeft(timeLeft);
        }
        else
        {
            InExposure = false;
            PrimaryCCD.setExposureLeft(0.0);
            if (ExposureRequest*1000 > 5 * POLLMS)
                DEBUG(INDI::Logger::DBG_SESSION, "Exposure done, downloading image...");
            pthread_mutex_lock(&condMutex);
            exposureSetRequest(StateIdle);
            pthread_mutex_unlock(&condMutex);
            grabImage();
            pthread_mutex_lock(&condMutex);
            break;
        }
        usleep(uSecs);

        pthread_mutex_lock(&condMutex);
    }
}

/* Caller must hold the mutex */
void QHYCCD::exposureSetRequest(ImageState request)
{
    if (threadRequest == StateExposure)
    {
        threadRequest = request;
    }
}

void QHYCCD::logQHYMessages(const std::string &message)
{
    LOGF_DEBUG("%s", message.c_str());
}

void QHYCCD::debugTriggered(bool enable)
{
    SetQHYCCDLogFunction(m_QHYLogCallback);
    if (enable)
        SetQHYCCDLogLevel(5);
    else
        SetQHYCCDLogLevel(2);
}
