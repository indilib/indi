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

    //#if defined(__APPLE__)
    //    char driverSupportPath[128];
    //    if (getenv("INDIPREFIX") != nullptr)
    //        sprintf(driverSupportPath, "%s/Contents/Resources", getenv("INDIPREFIX"));
    //    else
    //        strncpy(driverSupportPath, "/usr/local/lib/indi", 128);
    //    strncat(driverSupportPath, "/DriverSupport/qhy/firmware", 128);
    //    IDLog("QHY firmware path: %s\n", driverSupportPath);
    //    OSXInitQHYCCDFirmware(driverSupportPath);
    //#endif

    // JM 2019-03-07: Use OSXInitQHYCCDFirmwareArray as recommended by QHY
#if defined(__APPLE__)
    OSXInitQHYCCDFirmwareArray();
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

    snprintf(this->name, MAXINDINAME, "QHY CCD %.15s", name);
    snprintf(this->camid, MAXINDINAME, "%s", name);
    setDeviceName(this->name);

    setVersion(INDI_QHY_VERSION_MAJOR, INDI_QHY_VERSION_MINOR);

    m_QHYLogCallback = [this](const std::string & message)
    {
        logQHYMessages(message);
    };

    // We only want to log to the function above
    EnableQHYCCDLogFile(false);
    EnableQHYCCDMessage(false);

    // Set verbose level to Error/Fatal only by default
    SetQHYCCDLogLevel(2);
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
    IUFillSwitch(&CoolerS[0], "COOLER_ON", "On", ISS_OFF);
    IUFillSwitch(&CoolerS[1], "COOLER_OFF", "Off", ISS_ON);
    IUFillSwitchVector(&CoolerSP, CoolerS, 2, getDeviceName(), "CCD_COOLER", "Cooler", MAIN_CONTROL_TAB, IP_RW,
                       ISR_1OFMANY, 0, IPS_IDLE);

    // CCD Regulation power
    IUFillNumber(&CoolerN[0], "CCD_COOLER_VALUE", "Cooling Power (%)", "%+06.2f", 0., 100., 5, 0.0);
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
    
    // Read Modes (initial support for QHY42Pro)
    IUFillNumber(&ReadModeN[0], "Read Mode", "Read Mode", "%3.0f", 0, 1, 1, 0);
    IUFillNumberVector(&ReadModeNP, ReadModeN, 1, getDeviceName(), "READ_MODE", "Read Mode", MAIN_CONTROL_TAB, IP_RW, 60,
                       IPS_IDLE);
    

    // USB Traffic
    IUFillNumber(&USBTrafficN[0], "Speed", "Speed", "%3.0f", 0, 0, 1, 0);
    IUFillNumberVector(&USBTrafficNP, USBTrafficN, 1, getDeviceName(), "USB_TRAFFIC", "USB Traffic", MAIN_CONTROL_TAB,
                       IP_RW, 60, IPS_IDLE);

    // Cooler Mode
    IUFillSwitch(&CoolerModeS[COOLER_AUTOMATIC], "COOLER_AUTOMATIC", "Auto", ISS_ON);
    IUFillSwitch(&CoolerModeS[COOLER_MANUAL], "COOLER_MANUAL", "Manual", ISS_OFF);
    IUFillSwitchVector(&CoolerModeSP, CoolerModeS, 2, getDeviceName(), "CCD_COOLER_MODE", "Cooler Mode", MAIN_CONTROL_TAB, IP_RO,
                       ISR_1OFMANY, 0, IPS_IDLE);

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

            if (HasCoolerManualMode)
            {
                defineSwitch(&CoolerModeSP);
            }

            defineNumber(&CoolerNP);
        }

        if (HasUSBSpeed)
            defineNumber(&SpeedNP);
        
        if (HasReadMode)
            defineNumber(&ReadModeNP);

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
            if (HasCoolerManualMode)
            {
                defineSwitch(&CoolerModeSP);
            }

            CoolerNP.p = HasCoolerManualMode ? IP_RW : IP_RO;
            defineNumber(&CoolerNP);

            m_TemperatureTimerID = IEAddTimer(POLLMS, QHYCCD::updateTemperatureHelper, this);
        }

        double min = 0, max = 0, step = 0;
        if (HasUSBSpeed)
        {
            if (isSimulation())
            {
                SpeedN[0].min   = 1;
                SpeedN[0].max   = 5;
                SpeedN[0].step  = 1;
                SpeedN[0].value = 1;
            }
            else
            {
                int ret = GetQHYCCDParamMinMaxStep(m_CameraHandle, CONTROL_SPEED, &min, &max, &step);
                if (ret == QHYCCD_SUCCESS)
                {
                    SpeedN[0].min  = min;
                    SpeedN[0].max  = max;
                    SpeedN[0].step = step;
                }

                SpeedN[0].value = GetQHYCCDParam(m_CameraHandle, CONTROL_SPEED);

                LOGF_INFO("USB Speed Settings: Value: %.f Min: %.f Max: .fg Step %.f", SpeedN[0].value,
                          SpeedN[0].min, SpeedN[0].max, SpeedN[0].step);
            }

            defineNumber(&SpeedNP);
        }
        
        // Read mode support
         if (HasReadMode)
         {
            if (isSimulation())
            {
                ReadModeN[0].min   = 0;
                ReadModeN[0].max   = 2;
                ReadModeN[0].step  = 1;
                ReadModeN[0].value = 1;
            }
            else
            {
               
                ReadModeN[0].min  = 0;
                
                ////////////////////////////////////////////////////////////////////
                /// Read Modes
                ////////////////////////////////////////////////////////////////////
                int ret;
                uint32_t maxNumOfReadModes = 0;
                ret = GetQHYCCDNumberOfReadModes(m_CameraHandle, &maxNumOfReadModes);
                if (ret == QHYCCD_SUCCESS && maxNumOfReadModes > 0)
                {
                    ReadModeN[0].max  = maxNumOfReadModes - 1;
                }
                else
                {
                    ReadModeN[0].max = 0; 
                }
                
                ReadModeN[0].step = 1;
                
                uint32_t currentReadMode = 0;
                
                ret = GetQHYCCDReadMode(m_CameraHandle, &currentReadMode);
                if (ret == QHYCCD_SUCCESS)
                {
                    ReadModeN[0].value = currentReadMode;
                    LOGF_INFO("Current read mode: %zu", currentReadMode);
                }
                else
                {
                    LOGF_INFO("Using default read mode (error reading it): %zu", currentReadMode);
                }

                
            }
         }
        // ---

        if (HasGain)
        {
            if (isSimulation())
            {
                GainN[0].min   = 0;
                GainN[0].max   = 100;
                GainN[0].step  = 10;
                GainN[0].value = 50;
            }
            else
            {
                int ret = GetQHYCCDParamMinMaxStep(m_CameraHandle, CONTROL_GAIN, &min, &max, &step);
                if (ret == QHYCCD_SUCCESS)
                {
                    GainN[0].min  = min;
                    GainN[0].max  = max;
                    GainN[0].step = step;
                }
                GainN[0].value = GetQHYCCDParam(m_CameraHandle, CONTROL_GAIN);

                LOGF_INFO("Gain Settings: Value: %.3f Min: %.3f Max: %.3f Step %.3f", GainN[0].value, GainN[0].min,
                          GainN[0].max, GainN[0].step);
            }

            defineNumber(&GainNP);
        }

        if (HasOffset)
        {
            if (isSimulation())
            {
                OffsetN[0].min   = 1;
                OffsetN[0].max   = 10;
                OffsetN[0].step  = 1;
                OffsetN[0].value = 1;
            }
            else
            {
                int ret = GetQHYCCDParamMinMaxStep(m_CameraHandle, CONTROL_OFFSET, &min, &max, &step);
                if (ret == QHYCCD_SUCCESS)
                {
                    OffsetN[0].min  = min;
                    OffsetN[0].max  = max;
                    OffsetN[0].step = step;
                }
                OffsetN[0].value = GetQHYCCDParam(m_CameraHandle, CONTROL_OFFSET);

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
            if (isSimulation())
            {
                USBTrafficN[0].min   = 1;
                USBTrafficN[0].max   = 100;
                USBTrafficN[0].step  = 5;
                USBTrafficN[0].value = 20;
            }
            else
            {
                int ret = GetQHYCCDParamMinMaxStep(m_CameraHandle, CONTROL_USBTRAFFIC, &min, &max, &step);
                if (ret == QHYCCD_SUCCESS)
                {
                    USBTrafficN[0].min  = min;
                    USBTrafficN[0].max  = max;
                    USBTrafficN[0].step = step;
                }
                USBTrafficN[0].value = GetQHYCCDParam(m_CameraHandle, CONTROL_USBTRAFFIC);

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

            if (HasCoolerManualMode)
                deleteProperty(CoolerModeSP.name);

            deleteProperty(CoolerNP.name);

            RemoveTimer(m_TemperatureTimerID);
        }

        if (HasUSBSpeed)
        {
            deleteProperty(SpeedNP.name);
        }
        
        if (HasReadMode)
        {
            deleteProperty(ReadModeNP.name);
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
    }

    return true;
}

bool QHYCCD::Connect()
{
    unsigned int ret = 0;
    uint32_t cap;
    uint32_t readModes = 0;
    

    if (isSimulation())
    {
        cap = CCD_CAN_SUBFRAME | CCD_CAN_ABORT | CCD_CAN_BIN | CCD_HAS_COOLER | CCD_HAS_ST4_PORT;
        SetCCDCapability(cap);

        HasUSBTraffic = true;
        HasUSBSpeed   = true;
        HasGain       = true;
        HasOffset     = true;
        HasFilters    = true;
        HasReadMode   = true;

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
    m_CameraHandle = OpenQHYCCD(camid);

    if (m_CameraHandle != nullptr)
    {
        LOGF_INFO("Connected to %s.", camid);

        cap = CCD_CAN_ABORT | CCD_CAN_SUBFRAME;

        // Disable the stream mode before connecting
        ret = SetQHYCCDStreamMode(m_CameraHandle, 0);
        if (ret != QHYCCD_SUCCESS)
        {
            LOGF_ERROR("Can not disable stream mode (%d)", ret);
        }
        ret = InitQHYCCD(m_CameraHandle);
        if (ret != QHYCCD_SUCCESS)
        {
            LOGF_ERROR("Init Camera failed (%d)", ret);
            return false;
        }

        ////////////////////////////////////////////////////////////////////
        /// Read Modes
        ////////////////////////////////////////////////////////////////////
        ret = GetQHYCCDNumberOfReadModes(m_CameraHandle, &readModes);
        if (ret == QHYCCD_SUCCESS)
        {
            HasReadMode = true;
            LOGF_INFO("Number of read modes: %zu", readModes);
        }
       
       
        
        ////////////////////////////////////////////////////////////////////
        /// Shutter Support
        ////////////////////////////////////////////////////////////////////
        ret = IsQHYCCDControlAvailable(m_CameraHandle, CAM_MECHANICALSHUTTER);
        if (ret == QHYCCD_SUCCESS)
        {
            cap |= CCD_HAS_SHUTTER;
        }

        LOGF_DEBUG("Shutter Control: %s", cap & CCD_HAS_SHUTTER ? "True" : "False");

        ////////////////////////////////////////////////////////////////////
        /// Streaming Support
        ////////////////////////////////////////////////////////////////////
        ret = IsQHYCCDControlAvailable(m_CameraHandle, CAM_LIVEVIDEOMODE);
        if (ret == QHYCCD_SUCCESS)
        {
            cap |= CCD_HAS_STREAMING;
        }

        LOGF_DEBUG("Has Streaming: %s", cap & CCD_HAS_STREAMING ? "True" : "False");

        ////////////////////////////////////////////////////////////////////
        /// AutoMode Cooler Support
        ////////////////////////////////////////////////////////////////////
        ret = IsQHYCCDControlAvailable(m_CameraHandle, CONTROL_COOLER);
        if (ret == QHYCCD_SUCCESS)
        {
            HasCoolerAutoMode = true;
            cap |= CCD_HAS_COOLER;
        }
        LOGF_DEBUG("Automatic Cooler Control: %s", cap & CCD_HAS_COOLER ? "True" : "False");

        ////////////////////////////////////////////////////////////////////
        /// Manual PWM Support
        ////////////////////////////////////////////////////////////////////
        ret = IsQHYCCDControlAvailable(m_CameraHandle, CONTROL_MANULPWM);
        if (ret == QHYCCD_SUCCESS)
        {
            HasCoolerManualMode = true;
        }
        LOGF_DEBUG("Manual Cooler Control: %s", HasCoolerManualMode ? "True" : "False");

        ////////////////////////////////////////////////////////////////////
        /// ST4 Port Support
        ////////////////////////////////////////////////////////////////////
        ret = IsQHYCCDControlAvailable(m_CameraHandle, CONTROL_ST4PORT);
        if (ret == QHYCCD_SUCCESS)
        {
            cap |= CCD_HAS_ST4_PORT;
        }

        LOGF_DEBUG("Guider Port Control: %s", cap & CCD_HAS_ST4_PORT ? "True" : "False");

        ////////////////////////////////////////////////////////////////////
        /// Camera Speed Support
        ////////////////////////////////////////////////////////////////////
        ret = IsQHYCCDControlAvailable(m_CameraHandle, CONTROL_SPEED);
        if (ret == QHYCCD_SUCCESS)
        {
            HasUSBSpeed = true;

            // Force a certain speed on initialization of QHY5PII-C:
            // CONTROL_SPEED = 2 - Fastest, but the camera gets stuck with long exposure times.
            // CONTROL_SPEED = 1 - This is safe with the current driver.
            // CONTROL_SPEED = 0 - This is safe, but slower than 1.
            if (isQHY5PIIC())
                SetQHYCCDParam(m_CameraHandle, CONTROL_SPEED, 1);
        }

        LOGF_DEBUG("USB Speed Control: %s", HasUSBSpeed ? "True" : "False");

        ////////////////////////////////////////////////////////////////////
        /// Gain Support
        ////////////////////////////////////////////////////////////////////
        ret = IsQHYCCDControlAvailable(m_CameraHandle, CONTROL_GAIN);
        if (ret == QHYCCD_SUCCESS)
        {
            HasGain = true;
        }

        LOGF_DEBUG("Gain Control: %s", HasGain ? "True" : "False");

        ////////////////////////////////////////////////////////////////////
        /// Offset Support
        ////////////////////////////////////////////////////////////////////
        ret = IsQHYCCDControlAvailable(m_CameraHandle, CONTROL_OFFSET);
        if (ret == QHYCCD_SUCCESS)
        {
            HasOffset = true;
        }

        LOGF_DEBUG("Offset Control: %s", HasOffset ? "True" : "False");

        ////////////////////////////////////////////////////////////////////
        /// Filter Wheel Support
        ////////////////////////////////////////////////////////////////////
        ret = IsQHYCCDControlAvailable(m_CameraHandle, CONTROL_CFWPORT);
        if (ret == QHYCCD_SUCCESS)
        {
            HasFilters = true;

            m_MaxFilterCount = GetQHYCCDParam(m_CameraHandle, CONTROL_CFWSLOTSNUM);
            LOGF_DEBUG("Filter Count (CONTROL_CFWSLOTSNUM): %d", m_MaxFilterCount);
            // If we get invalid value, check again in 0.5 sec
            if (m_MaxFilterCount > 16)
            {
                usleep(500000);
                m_MaxFilterCount = GetQHYCCDParam(m_CameraHandle, CONTROL_CFWSLOTSNUM);
                LOGF_DEBUG("Filter Count (CONTROL_CFWSLOTSNUM): %d", m_MaxFilterCount);
            }

            if (m_MaxFilterCount > 16)
            {
                LOG_DEBUG("Camera can support CFW but no filters are present.");
                m_MaxFilterCount = -1;
            }

            if (m_MaxFilterCount > 0)
                updateFilterProperties();
            else
                HasFilters = false;
        }

        LOGF_DEBUG("Has Filters: %s", HasFilters ? "True" : "False");

        ////////////////////////////////////////////////////////////////////
        /// 8bit Support
        ////////////////////////////////////////////////////////////////////
        ret = IsQHYCCDControlAvailable(m_CameraHandle, CONTROL_TRANSFERBIT);
        HasTransferBit = (ret == QHYCCD_SUCCESS);
        LOGF_DEBUG("Has Transfer Bit control? %s", HasTransferBit ? "True" : "False");

        // Using software binning
        cap |= CCD_CAN_BIN;

        ret = IsQHYCCDControlAvailable(m_CameraHandle, CAM_BIN1X1MODE);
        LOGF_DEBUG("Bin 1x1: %s", (ret == QHYCCD_SUCCESS) ? "True" : "False");

        ret &= IsQHYCCDControlAvailable(m_CameraHandle, CAM_BIN2X2MODE);
        ret &= IsQHYCCDControlAvailable(m_CameraHandle, CAM_BIN3X3MODE);
        ret &= IsQHYCCDControlAvailable(m_CameraHandle, CAM_BIN4X4MODE);

        // Only use software binning if NOT supported by hardware
        //useSoftBin = !(ret == QHYCCD_SUCCESS);

        LOGF_DEBUG("Binning Control: %s", cap & CCD_CAN_BIN ? "True" : "False");

        ////////////////////////////////////////////////////////////////////
        /// USB Traffic Control Support
        ////////////////////////////////////////////////////////////////////
        ret = IsQHYCCDControlAvailable(m_CameraHandle, CONTROL_USBTRAFFIC);
        if (ret == QHYCCD_SUCCESS)
        {
            HasUSBTraffic = true;
            // Force the USB traffic value to 30 on initialization of QHY5PII-C otherwise
            // the camera has poor transfer speed.
            if (isQHY5PIIC())
                SetQHYCCDParam(m_CameraHandle, CONTROL_USBTRAFFIC, 30);
        }

        LOGF_DEBUG("USB Traffic Control: %s", HasUSBTraffic ? "True" : "False");

        ////////////////////////////////////////////////////////////////////
        /// Color Support
        ////////////////////////////////////////////////////////////////////
        ret = IsQHYCCDControlAvailable(m_CameraHandle, CAM_COLOR);
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

        double min = 0, max = 0, step = 0;
        // Exposure limits in microseconds
        int ret = GetQHYCCDParamMinMaxStep(m_CameraHandle, CONTROL_EXPOSURE, &min, &max, &step);
        if (ret == QHYCCD_SUCCESS)
            PrimaryCCD.setMinMaxStep("CCD_EXPOSURE", "CCD_EXPOSURE_VALUE", min / 1e6, max / 1e6, step / 1e6, false);
        else
            PrimaryCCD.setMinMaxStep("CCD_EXPOSURE", "CCD_EXPOSURE_VALUE", 0.001, 3600, 1, false);

        LOGF_INFO("Camera exposure limits: Min: %.6fs Max: %.3fs Step %.fs", min / 1e6, max / 1e6, step / 1e6);


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

        SetTimer(POLLMS);

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
            SetQHYCCDStreamMode(m_CameraHandle, 0x0);
            StopQHYCCDLive(m_CameraHandle);
        }
        else if (tState == StateExposure)
            CancelQHYCCDExposingAndReadout(m_CameraHandle);
        CloseQHYCCD(m_CameraHandle);
    }

    LOG_INFO("Camera is offline.");

    return true;
}

bool QHYCCD::setupParams()
{
    uint32_t nbuf, ret, imagew, imageh, bpp;
    double chipw, chiph, pixelw, pixelh;

    if (isSimulation())
    {
        chipw = imagew = 1280;
        chiph = imageh = 1024;
        pixelh = pixelw = 5.4;
        bpp             = 8;
    }
    else
    {
        ret = GetQHYCCDChipInfo(m_CameraHandle, &chipw, &chiph, &imagew, &imageh, &pixelw, &pixelh, &bpp);

        /* JM: We need GetQHYCCDErrorString(ret) to get the string description of the error, please implement this in the SDK */
        if (ret != QHYCCD_SUCCESS)
        {
            LOGF_ERROR("Error: GetQHYCCDChipInfo() (%d)", ret);
            return false;
        }

        LOGF_DEBUG("GetQHYCCDChipInfo: chipW :%g chipH: %g imageW: %d imageH: %d pixelW: %g pixelH: %g bbp %d", chipw,
                   chiph, imagew, imageh, pixelw, pixelh, bpp);
    }

    SetCCDParams(imagew, imageh, bpp, pixelw, pixelh);

    nbuf = PrimaryCCD.getXRes() * PrimaryCCD.getYRes() * PrimaryCCD.getBPP() / 8;
    PrimaryCCD.setFrameBufferSize(nbuf);

    if (HasStreaming())
    {
        Streamer->setPixelFormat(INDI_MONO);
        Streamer->setSize(imagew, imageh);
    }

    return true;
}

int QHYCCD::SetTemperature(double temperature)
{
    // If there difference, for example, is less than 0.1 degrees, let's immediately return OK.
    if (fabs(temperature - TemperatureN[0].value) < TEMP_THRESHOLD)
        return 1;

    LOGF_DEBUG("Requested temperature is %.3f, current temperature is %.3f", temperature, TemperatureN[0].value);

    m_TemperatureRequest = temperature;
    m_PWMRequest = -1;

    SetQHYCCDParam(m_CameraHandle, CONTROL_COOLER, m_TemperatureRequest);

    setCoolerEnabled(m_TemperatureRequest < TemperatureN[0].value);
    setCoolerMode(COOLER_AUTOMATIC);
    return 0;
}

bool QHYCCD::StartExposure(float duration)
{
    unsigned int ret = QHYCCD_ERROR;

    if (HasStreaming() && Streamer->isBusy())
    {
        LOG_ERROR("Cannot take exposure while streaming/recording is active.");
        return false;
    }

    m_ImageFrameType = PrimaryCCD.getFrameType();

    if (GetCCDCapability() & CCD_HAS_SHUTTER)
    {
        if (m_ImageFrameType == INDI::CCDChip::DARK_FRAME || m_ImageFrameType == INDI::CCDChip::BIAS_FRAME)
            ControlQHYCCDShutter(m_CameraHandle, MACHANICALSHUTTER_CLOSE);
        else
            ControlQHYCCDShutter(m_CameraHandle, MACHANICALSHUTTER_FREE);
    }

    long uSecs = duration * 1000 * 1000;
    LOGF_DEBUG("Requested exposure time is %ld us", uSecs);
    m_ExposureRequest = static_cast<double>(duration);
    PrimaryCCD.setExposureDuration(m_ExposureRequest);

    // Setting Exposure time, IF different from last exposure time.
    if (isSimulation())
        ret = QHYCCD_SUCCESS;
    else
    {
        if (m_LastExposureRequestuS != uSecs)
        {
            ret = SetQHYCCDParam(m_CameraHandle, CONTROL_EXPOSURE, uSecs);

            if (ret != QHYCCD_SUCCESS)
            {
                LOGF_ERROR("Set expose time failed (%d).", ret);
                return false;
            }

            m_LastExposureRequestuS = uSecs;
        }
    }

    // Set binning mode
    if (isSimulation())
        ret = QHYCCD_SUCCESS;
    else
        ret = SetQHYCCDBinMode(m_CameraHandle, PrimaryCCD.getBinX(), PrimaryCCD.getBinY());
    if (ret != QHYCCD_SUCCESS)
    {
        LOGF_INFO("Set QHYCCD Bin mode failed (%d)", ret);
        return false;
    }

    LOGF_DEBUG("SetQHYCCDBinMode (%dx%d).", PrimaryCCD.getBinX(), PrimaryCCD.getBinY());

    // Set Region of Interest (ROI)
    if (isSimulation())
        ret = QHYCCD_SUCCESS;
    else
        ret = SetQHYCCDResolution(m_CameraHandle,
                                  PrimaryCCD.getSubX() / PrimaryCCD.getBinX(),
                                  PrimaryCCD.getSubY() / PrimaryCCD.getBinY(),
                                  PrimaryCCD.getSubW() / PrimaryCCD.getBinX(),
                                  PrimaryCCD.getSubH() / PrimaryCCD.getBinY());
    if (ret != QHYCCD_SUCCESS)
    {
        LOGF_INFO("Set QHYCCD ROI resolution (%d,%d) (%d,%d) failed (%d)",
                  PrimaryCCD.getSubX() / PrimaryCCD.getBinX(),
                  PrimaryCCD.getSubY() / PrimaryCCD.getBinY(),
                  PrimaryCCD.getSubW() / PrimaryCCD.getBinX(),
                  PrimaryCCD.getSubH() / PrimaryCCD.getBinY(),
                  ret);
        return false;
    }

    LOGF_DEBUG("SetQHYCCDResolution x: %d y: %d w: %d h: %d",
               PrimaryCCD.getSubX() / PrimaryCCD.getBinX(),
               PrimaryCCD.getSubY() / PrimaryCCD.getBinY(),
               PrimaryCCD.getSubW() / PrimaryCCD.getBinX(),
               PrimaryCCD.getSubH() / PrimaryCCD.getBinY());

    // Start to expose the frame
    if (isSimulation())
        ret = QHYCCD_SUCCESS;
    else
        ret = ExpQHYCCDSingleFrame(m_CameraHandle);
    if (ret == QHYCCD_ERROR)
    {
        LOGF_INFO("Begin QHYCCD expose failed (%d)", ret);
        return false;
    }

    gettimeofday(&ExpStart, nullptr);
    LOGF_DEBUG("Taking a %.5f seconds frame...", m_ExposureRequest);

    InExposure = true;
    pthread_mutex_lock(&condMutex);
    threadRequest = StateExposure;
    pthread_cond_signal(&cv);
    pthread_mutex_unlock(&condMutex);

    return true;
}

bool QHYCCD::AbortExposure()
{
    if (!InExposure || isSimulation())
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
        int rc = CancelQHYCCDExposingAndReadout(m_CameraHandle);
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
    // Set UNBINNED coords
    PrimaryCCD.setFrame(x, y, w, h);
    // Total bytes required for image buffer
    uint32_t nbuf = (PrimaryCCD.getSubW() * PrimaryCCD.getSubH() * PrimaryCCD.getBPP() / 8);
    PrimaryCCD.setFrameBufferSize(nbuf);

    // Streamer is always updated with BINNED size.
    if (HasStreaming())
        Streamer->setSize(PrimaryCCD.getSubW() / PrimaryCCD.getBinX(), PrimaryCCD.getSubH() / PrimaryCCD.getBinY());
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
        ret = IsQHYCCDControlAvailable(m_CameraHandle, CAM_BIN1X1MODE);
    }
    else if (hor == 2 && ver == 2)
    {
        ret = IsQHYCCDControlAvailable(m_CameraHandle, CAM_BIN2X2MODE);
    }
    else if (hor == 3 && ver == 3)
    {
        ret = IsQHYCCDControlAvailable(m_CameraHandle, CAM_BIN3X3MODE);
    }
    else if (hor == 4 && ver == 4)
    {
        ret = IsQHYCCDControlAvailable(m_CameraHandle, CAM_BIN4X4MODE);
    }

    // Binning ALWAYS succeeds
#if 0
    if (ret != QHYCCD_SUCCESS)
    {
        useSoftBin = true;
    }

    // We always use software binning so QHY binning is always at 1x1
    PrimaryCCD.getBinX() = 1;
    PrimaryCCD.getBinY() = 1;
#endif

    if (ret != QHYCCD_SUCCESS)
    {
        LOGF_ERROR("%dx%d binning is not supported.", hor, ver);
        return false;
    }

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

    timeleft = m_ExposureRequest - timesince;
    return timeleft;
}

/* Downloads the image from the CCD. */
int QHYCCD::grabImage()
{
    std::unique_lock<std::mutex> guard(ccdBufferLock);
    if (isSimulation())
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
        ret = GetQHYCCDSingleFrame(m_CameraHandle, &w, &h, &bpp, &channels, PrimaryCCD.getFrameBuffer());
        LOG_DEBUG("GetQHYCCDSingleFrame Blocking read call complete.");

        if (ret != QHYCCD_SUCCESS)
        {
            LOGF_ERROR("GetQHYCCDSingleFrame error (%d)", ret);
            PrimaryCCD.setExposureFailed();
            return -1;
        }
    }
    guard.unlock();

    // Perform software binning if necessary
    //if (useSoftBin)
    //    PrimaryCCD.binFrame();

    if (m_ExposureRequest > POLLMS * 5)
        LOG_INFO("Download complete.");
    else
        LOG_DEBUG("Download complete.");

    ExposureComplete(&PrimaryCCD);

    return 0;
}

void QHYCCD::TimerHit()
{
    if (isConnected() == false)
        return;

    //    if (HasFilters && m_MaxFilterCount == -1)
    //    {
    //        m_MaxFilterCount = GetQHYCCDParam(m_CameraHandle, CONTROL_CFWSLOTSNUM);
    //        LOGF_DEBUG("Filter Count (CONTROL_CFWSLOTSNUM): %d", m_MaxFilterCount);
    //        if (m_MaxFilterCount > 16)
    //            m_MaxFilterCount = -1;
    //        if (m_MaxFilterCount > 0)
    //        {
    //            if (updateFilterProperties())
    //            {
    //                deleteProperty("FILTER_NAME");
    //                IUUpdateMinMax(&FilterSlotNP);
    //                defineText(FilterNameTP);
    //            }
    //        }
    //    }

    if (FilterSlotNP.s == IPS_BUSY)
    {
        char currentPos[MAXINDINAME] = {0};
        int rc = GetQHYCCDCFWStatus(m_CameraHandle, currentPos);
        if (rc == QHYCCD_SUCCESS)
        {
            // QHY filter wheel positions are from '0' to 'F'
            // 0 to 15
            // INDI Filter Wheel 1 to 16
            CurrentFilter = strtol(currentPos, nullptr, 16) + 1;
            LOGF_DEBUG("Filter current position: %d", CurrentFilter);

            if (TargetFilter == CurrentFilter)
            {
                m_FilterCheckCounter = 0;
                SelectFilterDone(TargetFilter);
                LOGF_DEBUG("%s: Filter changed to %d", camid, TargetFilter);
            }
        }
        else if (++m_FilterCheckCounter > 30)
        {
            FilterSlotNP.s = IPS_ALERT;
            LOG_ERROR("Filter change timed out.");
            IDSetNumber(&FilterSlotNP, nullptr);
        }
    }

    SetTimer(POLLMS);
}

IPState QHYCCD::GuideNorth(uint32_t ms)
{
    ControlQHYCCDGuide(m_CameraHandle, 1, ms);
    return IPS_OK;
}

IPState QHYCCD::GuideSouth(uint32_t ms)
{
    ControlQHYCCDGuide(m_CameraHandle, 2, ms);
    return IPS_OK;
}

IPState QHYCCD::GuideEast(uint32_t ms)
{
    ControlQHYCCDGuide(m_CameraHandle, 0, ms);
    return IPS_OK;
}

IPState QHYCCD::GuideWest(uint32_t ms)
{
    ControlQHYCCDGuide(m_CameraHandle, 3, ms);
    return IPS_OK;
}

bool QHYCCD::SelectFilter(int position)
{
    if (isSimulation())
        return true;

    // QHY Filter position is '0' to 'F'
    // 0 to 15
    // INDI Filters 1 to 16
    char targetPos[8] = {0};
    snprintf(targetPos, 8, "%X", position - 1);
    return (SendOrder2QHYCCDCFW(m_CameraHandle, targetPos, 1) == QHYCCD_SUCCESS);
}

int QHYCCD::QueryFilter()
{
    return CurrentFilter;
}

bool QHYCCD::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        //////////////////////////////////////////////////////////////////////
        /// Cooler On/Off Control
        //////////////////////////////////////////////////////////////////////
        if (!strcmp(name, CoolerSP.name))
        {
            if (IUUpdateSwitch(&CoolerSP, states, names, n) < 0)
            {
                CoolerSP.s = IPS_ALERT;
                IDSetSwitch(&CoolerSP, nullptr);
                return true;
            }

            bool enabled = (CoolerS[COOLER_ON].s == ISS_ON);

            // If explicitly enabled, we always set temperature to 0
            if (enabled)
            {
                if (HasCoolerAutoMode)
                {
                    if (SetTemperature(0) == 0)
                    {
                        TemperatureNP.s = IPS_BUSY;
                        IDSetNumber(&TemperatureNP, nullptr);
                    }
                    return true;
                }
                else
                {
                    IUResetSwitch(&CoolerSP);
                    CoolerS[COOLER_OFF].s = ISS_ON;
                    CoolerSP.s = IPS_ALERT;
                    LOG_ERROR("Cannot turn on cooler in manual mode. Set cooler power to activate it.");
                    IDSetSwitch(&CoolerSP, nullptr);
                    return true;
                }
            }
            else
            {
                if (HasCoolerManualMode)
                {
                    m_PWMRequest = 0;
                    m_TemperatureRequest = 30;
                    SetQHYCCDParam(m_CameraHandle, CONTROL_MANULPWM, 0);

                    CoolerSP.s = IPS_IDLE;
                    IDSetSwitch(&CoolerSP, nullptr);

                    TemperatureNP.s = IPS_IDLE;
                    IDSetNumber(&TemperatureNP, nullptr);

                    setCoolerMode(COOLER_MANUAL);
                    LOG_INFO("Camera is warming up.");
                }
                else
                {
                    // Warm up the camera in auto mode
                    if (SetTemperature(30) == 0)
                    {
                        TemperatureNP.s = IPS_IDLE;
                        IDSetNumber(&TemperatureNP, nullptr);
                    }
                    LOG_INFO("Camera is warming up.");
                    return true;
                }
            }

            return true;
        }

        //////////////////////////////////////////////////////////////////////
        /// Cooler Mode
        //////////////////////////////////////////////////////////////////////
        if (!strcmp(CoolerModeSP.name, name))
        {
            IUUpdateSwitch(&CoolerModeSP, states, names, n);
            if (IUFindOnSwitchIndex(&CoolerModeSP) == COOLER_AUTOMATIC)
            {
                m_PWMRequest = -1;
                LOG_INFO("Camera cooler is now automatically controlled to maintain the desired temperature.");
            }
            else
            {
                m_TemperatureRequest = 30;
                LOG_INFO("Camera cooler is manually controlled. Set the desired cooler power.");
            }

            IDSetSwitch(&CoolerModeSP, nullptr);
        }
    }

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
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        if (!strcmp(name, FilterSlotNP.name))
        {
            return INDI::FilterInterface::processNumber(dev, name, values, names, n);
        }

        //////////////////////////////////////////////////////////////////////
        /// Gain Control
        //////////////////////////////////////////////////////////////////////
        if (!strcmp(name, GainNP.name))
        {
            double currentGain = GainN[0].value;
            IUUpdateNumber(&GainNP, values, names, n);
            GainRequest = GainN[0].value;
            if (fabs(LastGainRequest - GainRequest) > 0.001)
            {
                int rc = SetQHYCCDParam(m_CameraHandle, CONTROL_GAIN, GainN[0].value);
                if (rc == QHYCCD_SUCCESS)
                {
                    LastGainRequest = GainRequest;
                    GainNP.s = IPS_OK;
                    saveConfig(true, GainNP.name);
                    LOGF_INFO("Gain updated to %.f", GainN[0].value);
                }
                else
                {
                    GainN[0].value = currentGain;
                    GainNP.s = IPS_ALERT;
                    LOGF_ERROR("Failed to changed gain: %d.", rc);
                }
            }
            else
                GainNP.s = IPS_OK;

            IDSetNumber(&GainNP, nullptr);
            return true;
        }

        //////////////////////////////////////////////////////////////////////
        /// Offset Control
        //////////////////////////////////////////////////////////////////////
        if (!strcmp(name, OffsetNP.name))
        {
            double currentOffset = OffsetN[0].value;
            IUUpdateNumber(&OffsetNP, values, names, n);
            int rc = SetQHYCCDParam(m_CameraHandle, CONTROL_OFFSET, OffsetN[0].value);

            if (rc == QHYCCD_SUCCESS)
            {
                OffsetNP.s = IPS_OK;
                LOGF_INFO("Offset updated to %.f", OffsetN[0].value);
                saveConfig(true, OffsetNP.name);
            }
            else
            {
                LOGF_ERROR("Failed to update offset: %.f", OffsetN[0].value);
                OffsetN[0].value = currentOffset;
                OffsetNP.s = IPS_ALERT;
            }

            IDSetNumber(&OffsetNP, nullptr);
            return true;
        }

        //////////////////////////////////////////////////////////////////////
        /// Speed Control
        //////////////////////////////////////////////////////////////////////
        if (!strcmp(name, SpeedNP.name))
        {
            double currentSpeed = SpeedN[0].value;
            IUUpdateNumber(&SpeedNP, values, names, n);
            int rc = SetQHYCCDParam(m_CameraHandle, CONTROL_SPEED, SpeedN[0].value);

            if (rc == QHYCCD_SUCCESS)
            {
                LOGF_INFO("Speed updated to %.f", SpeedN[0].value);
                SpeedNP.s = IPS_OK;
                saveConfig(true, SpeedNP.name);
            }
            else
            {
                LOGF_ERROR("Failed to update speed: %d", rc);
                SpeedNP.s = IPS_ALERT;
                SpeedN[0].value = currentSpeed;
            }

            IDSetNumber(&SpeedNP, nullptr);
            return true;
        }

        //////////////////////////////////////////////////////////////////////
        /// USB Traffic Control
        //////////////////////////////////////////////////////////////////////
        if (!strcmp(name, USBTrafficNP.name))
        {
            double currentTraffic = USBTrafficN[0].value;
            IUUpdateNumber(&USBTrafficNP, values, names, n);
            int rc = SetQHYCCDParam(m_CameraHandle, CONTROL_USBTRAFFIC, USBTrafficN[0].value);
            if (rc == QHYCCD_SUCCESS)
            {
                LOGF_INFO("USB Traffic updated to %.f", USBTrafficN[0].value);
                USBTrafficNP.s = IPS_OK;
                saveConfig(true, USBTrafficNP.name);
            }
            else
            {
                USBTrafficNP.s = IPS_ALERT;
                USBTrafficN[0].value = currentTraffic;
                LOGF_ERROR("Failed to update USB Traffic: %d", rc);
            }

            IDSetNumber(&USBTrafficNP, nullptr);
            return true;
        }
        
        
        //////////////////////////////////////////////////////////////////////
        /// Read Modes Control
        //////////////////////////////////////////////////////////////////////
         if (!strcmp(name, ReadModeNP.name))
        {
            uint32_t imageRMw, imageRMh, ret;
            double newReadMode = ReadModeN[0].value;
            uint32_t nbuf, imagew, imageh, bpp;
            double chipw, chiph, pixelw, pixelh;
            IUUpdateNumber(&ReadModeNP, values, names, n);
            int rc = SetQHYCCDReadMode(m_CameraHandle,ReadModeN[0].value);
            if (rc == QHYCCD_SUCCESS)
            {
                LOGF_INFO("Read mode updated to %.f", ReadModeN[0].value);
                // Get resolution
                ret = GetQHYCCDReadModeResolution(m_CameraHandle, ReadModeN[0].value, &imageRMw, &imageRMh);
                LOGF_INFO("GetQHYCCDReadModeResolution in this ReadMode: imageW: %d imageH: %d \n", imageRMw, imageRMh);
                
                ReadModeNP.s = IPS_OK;
                saveConfig(true, ReadModeNP.name);
                    if (isSimulation())
                    {
                        chipw = imagew = 1280;
                        chiph = imageh = 1024;
                        pixelh = pixelw = 5.4;
                        bpp             = 8;
                    }
                    else
                    {
                        ret = GetQHYCCDChipInfo(m_CameraHandle, &chipw, &chiph, &imagew, &imageh, &pixelw, &pixelh, &bpp);
                        
                        /* JM: We need GetQHYCCDErrorString(ret) to get the string description of the error, please implement this in the SDK */
                        if (ret != QHYCCD_SUCCESS)
                        {
                            LOGF_ERROR("Error: GetQHYCCDChipInfo() (%d)", ret);
                            return false;
                        }
                        
                    }

                    SetCCDParams(imageRMw, imageRMh, bpp, pixelw, pixelh);
                    nbuf = imageRMw * imageRMh * PrimaryCCD.getBPP() / 8;
                    PrimaryCCD.setFrameBufferSize(nbuf);

                    if (HasStreaming())
                    {
                        Streamer->setPixelFormat(INDI_MONO);
                        Streamer->setSize(imageRMw, imageRMh);
                    }
                
                
            }
            else
            {
                ReadModeNP.s = IPS_ALERT;
                ReadModeN[0].value = newReadMode;
                LOGF_ERROR("Failed to update read mode: %d", rc);
            }

            IDSetNumber(&ReadModeNP, nullptr);
            return true;
        }
        

        //////////////////////////////////////////////////////////////////////
        /// Cooler PWM Control
        //////////////////////////////////////////////////////////////////////
        if (!strcmp(name, CoolerNP.name))
        {
            if (HasCoolerManualMode == false)
            {
                CoolerNP.s = IPS_ALERT;
                LOG_WARN("Manual cooler control is not available.");
                IDSetNumber(&CoolerNP, nullptr);
            }

            setCoolerEnabled(values[0] > 0);
            setCoolerMode(COOLER_MANUAL);

            m_PWMRequest = values[0] / 100.0 * 255;
            CoolerNP.s = IPS_BUSY;
            LOGF_INFO("Setting cooler power manually to %.2f%%", values[0]);
            IDSetNumber(&CoolerNP, nullptr);
            return true;
        }
    }

    return INDI::CCD::ISNewNumber(dev, name, values, names, n);
}

void QHYCCD::setCoolerMode(uint8_t mode)
{
    int currentMode = IUFindOnSwitchIndex(&CoolerModeSP);
    if (mode == currentMode)
        return;

    IUResetSwitch(&CoolerModeSP);

    CoolerModeS[COOLER_AUTOMATIC].s = (mode == COOLER_AUTOMATIC) ? ISS_ON : ISS_OFF;
    CoolerModeS[COOLER_MANUAL].s = (mode == COOLER_AUTOMATIC) ? ISS_OFF : ISS_ON;
    CoolerSP.s = IPS_OK;
    LOGF_INFO("Switching to %s cooler control.", (mode == COOLER_AUTOMATIC) ? "automatic" : "manual");
    IDSetSwitch(&CoolerModeSP, nullptr);
}

void QHYCCD::setCoolerEnabled(bool enable)
{
    bool isEnabled = IUFindOnSwitchIndex(&CoolerSP) == COOLER_ON;
    if (isEnabled == enable)
        return;

    IUResetSwitch(&CoolerSP);
    CoolerS[COOLER_ON].s = enable ? ISS_ON : ISS_OFF;
    CoolerS[COOLER_OFF].s = enable ? ISS_OFF : ISS_ON;
    CoolerSP.s = enable ? IPS_BUSY : IPS_IDLE;
    IDSetSwitch(&CoolerSP, nullptr);
}

bool QHYCCD::isQHY5PIIC()
{
    return std::string(camid, 9) == "QHY5PII-C";
}

void QHYCCD::updateTemperatureHelper(void *p)
{
    static_cast<QHYCCD *>(p)->updateTemperature();
}

void QHYCCD::updateTemperature()
{
    double ccdtemp = 0, coolpower = 0;

    if (isSimulation())
    {
        ccdtemp = TemperatureN[0].value;
        if (TemperatureN[0].value < m_TemperatureRequest)
            ccdtemp += TEMP_THRESHOLD;
        else if (TemperatureN[0].value > m_TemperatureRequest)
            ccdtemp -= TEMP_THRESHOLD;

        coolpower = 128;
    }
    else
    {
        // Sleep for 1 second before setting temperature/pwm again.
        //usleep(1000000);

        // Call this function as long as we are busy
        if (TemperatureNP.s == IPS_BUSY)
        {
            SetQHYCCDParam(m_CameraHandle, CONTROL_COOLER, m_TemperatureRequest);
        }
        else if (m_PWMRequest >= 0)
        {
            SetQHYCCDParam(m_CameraHandle, CONTROL_MANULPWM, m_PWMRequest);
        }

        ccdtemp   = GetQHYCCDParam(m_CameraHandle, CONTROL_CURTEMP);
        coolpower = GetQHYCCDParam(m_CameraHandle, CONTROL_CURPWM);
    }

    // No need to spam to log
    if (fabs(ccdtemp - TemperatureN[0].value) > 0.001 || fabs(CoolerN[0].value - (coolpower / 255.0 * 100)) > 0.001)
    {
        LOGF_DEBUG("CCD T.: %.3f (C) Power: %.3f (%%.2f)", ccdtemp, coolpower, coolpower / 255.0 * 100);
    }

    TemperatureN[0].value = ccdtemp;

    CoolerN[0].value      = coolpower / 255.0 * 100;
    CoolerNP.s = CoolerN[0].value > 0 ? IPS_BUSY : IPS_IDLE;

    IPState coolerSwitchState = CoolerN[0].value > 0 ? IPS_BUSY : IPS_OK;
    if (coolerSwitchState != CoolerSP.s)
    {
        CoolerSP.s = coolerSwitchState;
        IDSetSwitch(&CoolerSP, nullptr);
    }

    if (TemperatureNP.s == IPS_BUSY && fabs(TemperatureN[0].value - m_TemperatureRequest) <= TEMP_THRESHOLD)
    {
        TemperatureN[0].value = m_TemperatureRequest;
        TemperatureNP.s       = IPS_OK;
    }

    IDSetNumber(&TemperatureNP, nullptr);
    IDSetNumber(&CoolerNP, nullptr);

    m_TemperatureTimerID = IEAddTimer(POLLMS, QHYCCD::updateTemperatureHelper, this);
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
    
    if (HasReadMode)
        IUSaveConfigNumber(fp, &ReadModeNP);

    if (HasUSBTraffic)
        IUSaveConfigNumber(fp, &USBTrafficNP);

    return true;
}

bool QHYCCD::StartStreaming()
{
    int ret = 0;
    m_ExposureRequest = 1.0 / Streamer->getTargetFPS();

    // N.B. There is no corresponding value for GBGR. It is odd that QHY selects this as the default as no one seems to process it
    const std::map<const char *, INDI_PIXEL_FORMAT> formats = { { "GBGR", INDI_MONO },
        { "GRGB", INDI_BAYER_GRBG },
        { "BGGR", INDI_BAYER_BGGR },
        { "RGGB", INDI_BAYER_RGGB }
    };

    // Set binning mode
    if (isSimulation())
        ret = QHYCCD_SUCCESS;
    else
        ret = SetQHYCCDBinMode(m_CameraHandle, PrimaryCCD.getBinX(), PrimaryCCD.getBinY());
    if (ret != QHYCCD_SUCCESS)
    {
        LOGF_INFO("Set QHYCCD Bin mode failed (%d)", ret);
        return false;
    }

    LOGF_DEBUG("SetQHYCCDBinMode (%dx%d).", PrimaryCCD.getBinX(), PrimaryCCD.getBinY());

    // Set Region of Interest (ROI)
    if (isSimulation())
        ret = QHYCCD_SUCCESS;
    else
        ret = SetQHYCCDResolution(m_CameraHandle,
                                  PrimaryCCD.getSubX() / PrimaryCCD.getBinX(),
                                  PrimaryCCD.getSubY() / PrimaryCCD.getBinY(),
                                  PrimaryCCD.getSubW() / PrimaryCCD.getBinX(),
                                  PrimaryCCD.getSubH() / PrimaryCCD.getBinY());
    if (ret != QHYCCD_SUCCESS)
    {
        LOGF_INFO("Set QHYCCD ROI resolution (%d,%d) (%d,%d) failed (%d)",
                  PrimaryCCD.getSubX() / PrimaryCCD.getBinX(),
                  PrimaryCCD.getSubY() / PrimaryCCD.getBinY(),
                  PrimaryCCD.getSubW() / PrimaryCCD.getBinX(),
                  PrimaryCCD.getSubH() / PrimaryCCD.getBinY(),
                  ret);
        return false;
    }

    LOGF_DEBUG("SetQHYCCDResolution x: %d y: %d w: %d h: %d",
               PrimaryCCD.getSubX() / PrimaryCCD.getBinX(),
               PrimaryCCD.getSubY() / PrimaryCCD.getBinY(),
               PrimaryCCD.getSubW() / PrimaryCCD.getBinX(),
               PrimaryCCD.getSubH() / PrimaryCCD.getBinY());

    INDI_PIXEL_FORMAT qhyFormat = INDI_MONO;
    if (BayerT[2].text && formats.count(BayerT[2].text) != 0)
        qhyFormat = formats.at(BayerT[2].text);

    double uSecs = static_cast<long>(m_ExposureRequest * 950000.0);

    LOGF_INFO("Starting video streaming with exposure %.3f seconds (%.f FPS)", m_ExposureRequest, Streamer->getTargetFPS());

    SetQHYCCDParam(m_CameraHandle, CONTROL_EXPOSURE, uSecs);

    // Set Stream Mode
    SetQHYCCDStreamMode(m_CameraHandle, 1);

    if (HasUSBSpeed)
    {
        ret = SetQHYCCDParam(m_CameraHandle, CONTROL_SPEED, 2.0);
        if (ret != QHYCCD_SUCCESS)
            LOG_WARN("SetQHYCCDParam CONTROL_SPEED 2.0 failed.");
    }
    if (HasUSBTraffic)
    {
        ret = SetQHYCCDParam(m_CameraHandle, CONTROL_USBTRAFFIC, 20.0);
        if (ret != QHYCCD_SUCCESS)
            LOG_WARN("SetQHYCCDParam CONTROL_USBTRAFFIC 20.0 failed.");
    }

    ret = SetQHYCCDBitsMode(m_CameraHandle, 8);
    if (ret == QHYCCD_SUCCESS)
        Streamer->setPixelFormat(qhyFormat, 8);
    else
    {
        LOG_WARN("SetQHYCCDBitsMode 8bit failed.");
        Streamer->setPixelFormat(qhyFormat, PrimaryCCD.getBPP());
    }

    BeginQHYCCDLive(m_CameraHandle);

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

    if (HasUSBSpeed)
        SetQHYCCDParam(m_CameraHandle, CONTROL_SPEED, SpeedN[0].value);
    if (HasUSBTraffic)
        SetQHYCCDParam(m_CameraHandle, CONTROL_USBTRAFFIC, USBTrafficN[0].value);
    SetQHYCCDStreamMode(m_CameraHandle, 0);
    StopQHYCCDLive(m_CameraHandle);

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
            StartExposure(m_ExposureRequest);
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
        uint32_t retries = 0;

        std::unique_lock<std::mutex> guard(ccdBufferLock);
        while (retries++ < 10)
        {
            ret = GetQHYCCDLiveFrame(m_CameraHandle, &w, &h, &bpp, &channels, PrimaryCCD.getFrameBuffer());
            if (ret == QHYCCD_ERROR)
                usleep(1000);
            else
                break;
        }
        guard.unlock();
        if (ret == QHYCCD_SUCCESS)
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
        uint32_t uSecs = 0;
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
            if (m_ExposureRequest * 1000 > 5 * POLLMS)
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
    // For some reason QHYSDK does not define this for MacOS! Needs to be fixed
#ifdef __linux__
    SetQHYCCDLogFunction(m_QHYLogCallback);
#endif
    if (enable)
        SetQHYCCDLogLevel(5);
    else
        SetQHYCCDLogLevel(2);
}

bool QHYCCD::updateFilterProperties()
{
    if (FilterNameTP->ntp != m_MaxFilterCount)
    {
        LOGF_DEBUG("Max filter count is: %d", m_MaxFilterCount);
        FilterSlotN[0].max = m_MaxFilterCount;
        char filterName[MAXINDINAME];
        char filterLabel[MAXINDILABEL];
        if (FilterNameT != nullptr)
        {
            for (int i = 0; i < FilterNameTP->ntp; i++)
                free(FilterNameT[i].text);
            delete [] FilterNameT;
        }

        FilterNameT = new IText[m_MaxFilterCount];
        memset(FilterNameT, 0, sizeof(IText) * m_MaxFilterCount);
        for (int i = 0; i < m_MaxFilterCount; i++)
        {
            snprintf(filterName, MAXINDINAME, "FILTER_SLOT_NAME_%d", i + 1);
            snprintf(filterLabel, MAXINDILABEL, "Filter#%d", i + 1);
            IUFillText(&FilterNameT[i], filterName, filterLabel, filterLabel);
        }
        IUFillTextVector(FilterNameTP, FilterNameT, m_MaxFilterCount, m_defaultDevice->getDeviceName(), "FILTER_NAME", "Filter", FilterSlotNP.group, IP_RW, 0, IPS_IDLE);

        return true;
    }

    return false;
}

void QHYCCD::addFITSKeywords(fitsfile *fptr, INDI::CCDChip *targetChip)
{
    INDI::CCD::addFITSKeywords(fptr, targetChip);

    if (HasGain)
    {
        int status = 0;
        fits_update_key_dbl(fptr, "Gain", GainN[0].value, 3, "Gain", &status);
    }

}

