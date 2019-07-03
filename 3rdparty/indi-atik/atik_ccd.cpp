/*
 ATIK CCD & Filter Wheel Driver

 Copyright (C) 2018 Jasem Mutlaq (mutlaqja@ikarustech.com)

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

#include "atik_ccd.h"

#include "config.h"

#include <stream/streammanager.h>

#include <algorithm>
#include <math.h>
#include <unistd.h>

#define MAX_EXP_RETRIES         3
#define VERBOSE_EXPOSURE        3
#define TEMP_TIMER_MS           1000 /* Temperature polling time (ms) */
#define TEMP_THRESHOLD          .25  /* Differential temperature threshold (C)*/
#define MAX_DEVICES             4    /* Max device cameraCount */

#define CONTROL_TAB "Controls"

static int iAvailableCamerasCount;
static ATIKCCD *cameras[MAX_DEVICES];

static void cleanup()
{
    for (int i = 0; i < iAvailableCamerasCount; i++)
    {
        delete cameras[i];
    }
}

void ATIK_CCD_ISInit()
{
    static bool isInit = false;
    if (!isInit)
    {
        iAvailableCamerasCount = 0;
        std::vector<std::string> cameraNames;

        iAvailableCamerasCount = ArtemisDeviceCount();
        if (iAvailableCamerasCount > MAX_DEVICES)
            iAvailableCamerasCount = MAX_DEVICES;
        if (iAvailableCamerasCount <= 0)
            IDLog("No Atik Cameras detected. Power on?");
        else
        {
            for (int i = 0; i < iAvailableCamerasCount; i++)
            {
                // We only do cameras in this driver.
                if (ArtemisDeviceIsPresent(i) == false || ArtemisDeviceIsCamera(i) == false)
                    continue;

                char pName[MAXINDILABEL] = {0};
                std::string cameraName;

                if (ArtemisDeviceName(i, pName) == false)
                    continue;

                if (std::find(cameraNames.begin(), cameraNames.end(), pName) == cameraNames.end())
                    cameraName = std::string(pName);
                else
                    cameraName = std::string(pName) + " " +
                                 std::to_string(static_cast<int>(std::count(cameraNames.begin(), cameraNames.end(), pName)) + 1);

                cameras[i] = new ATIKCCD(cameraName, i);
                cameraNames.push_back(pName);
            }
        }

        if (cameraNames.empty())
        {
            iAvailableCamerasCount = 0;
            return;
        }

        atexit(cleanup);
        isInit = true;
    }
}

void ISGetProperties(const char *dev)
{
    ATIK_CCD_ISInit();

    if (iAvailableCamerasCount == 0)
    {
        IDMessage(nullptr, "No Atik cameras detected. Power on?");
        return;
    }

    for (int i = 0; i < iAvailableCamerasCount; i++)
    {
        ATIKCCD *camera = cameras[i];
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
    ATIK_CCD_ISInit();
    for (int i = 0; i < iAvailableCamerasCount; i++)
    {
        ATIKCCD *camera = cameras[i];
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
    ATIK_CCD_ISInit();
    for (int i = 0; i < iAvailableCamerasCount; i++)
    {
        ATIKCCD *camera = cameras[i];
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
    ATIK_CCD_ISInit();
    for (int i = 0; i < iAvailableCamerasCount; i++)
    {
        ATIKCCD *camera = cameras[i];
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
    ATIK_CCD_ISInit();

    for (int i = 0; i < iAvailableCamerasCount; i++)
    {
        ATIKCCD *camera = cameras[i];
        camera->ISSnoopDevice(root);
    }
}

ATIKCCD::ATIKCCD(std::string filterName, int id) : FilterInterface(this), m_iDevice(id)
{
    setVersion(ATIK_VERSION_MAJOR, ATIK_VERSION_MINOR);

    genTimerID = WEtimerID = NStimerID = -1;
    NSDir = NORTH;
    WEDir = WEST;

    strncpy(this->name, filterName.c_str(), MAXINDIDEVICE);
    setDeviceName(this->name);
}

const char *ATIKCCD::getDefaultName()
{
    return "Atik";
}

bool ATIKCCD::initProperties()
{
    INDI::CCD::initProperties();

    // Cooler control
    IUFillSwitch(&CoolerS[COOLER_ON], "COOLER_ON", "ON", ISS_OFF);
    IUFillSwitch(&CoolerS[COOLER_OFF], "COOLER_OFF", "OFF", ISS_ON);
    IUFillSwitchVector(&CoolerSP, CoolerS, 2, getDeviceName(), "CCD_COOLER", "Cooler", MAIN_CONTROL_TAB, IP_WO,
                       ISR_1OFMANY, 2, IPS_IDLE);

    // Temperature value
    IUFillNumber(&CoolerN[0], "CCD_COOLER_VALUE", "Cooling Power (%)", "%+06.2f", 0., 1., .2, 0.0);
    IUFillNumberVector(&CoolerNP, CoolerN, 1, getDeviceName(), "CCD_COOLER_POWER", "Cooling Power", MAIN_CONTROL_TAB,
                       IP_RO, 60, IPS_IDLE);

    // Version information
    IUFillText(&VersionInfoS[VERSION_API], "VERSION_API", "API", std::to_string(ArtemisAPIVersion()).c_str());
    IUFillText(&VersionInfoS[VERSION_FIRMWARE], "VERSION_FIRMWARE", "Firmware", "Unknown");
    IUFillTextVector(&VersionInfoSP, VersionInfoS, 2, getDeviceName(), "VERSION", "Version", INFO_TAB, IP_RO, 60, IPS_IDLE);

    // Gain/Offset Presets
    IUFillSwitch(&ControlPresetsS[PRESET_CUSTOM], "PRESET_CUSTOM", "Custom", ISS_ON);
    IUFillSwitch(&ControlPresetsS[PRESET_LOW], "PRESET_LOW", "Low", ISS_OFF);
    IUFillSwitch(&ControlPresetsS[PRESET_MEDIUM], "PRESET_MEDIUM", "Medium", ISS_OFF);
    IUFillSwitch(&ControlPresetsS[PRESET_HIGH], "PRESET_HIGH", "High", ISS_OFF);
    IUFillSwitchVector(&ControlPresetsSP, ControlPresetsS, 2, getDeviceName(), "CCD_CONTROL_PRESETS", "GO Presets", CONTROLS_TAB, IP_RW,
                       ISR_1OFMANY, 4, IPS_IDLE);

    // Gain/Offset Controls
    IUFillNumber(&ControlN[CONTROL_GAIN], "CONTROL_GAIN", "Gain", "%.f", 0, 60, 5, 30);
    IUFillNumber(&ControlN[CONTROL_OFFSET], "CONTROL_OFFSET", "Offset", "%.f", 0, 511, 10, 0);
    IUFillNumberVector(&ControlNP, ControlN, 2, getDeviceName(), "CCD_CONTROLS", "GO Controls", CONTROLS_TAB,
                       IP_RW, 60, IPS_IDLE);

    IUSaveText(&BayerT[2], "RGGB");

    INDI::FilterInterface::initProperties(FILTER_TAB);

    addAuxControls();

    return true;
}

bool ATIKCCD::updateProperties()
{
    INDI::CCD::updateProperties();

    if (isConnected())
    {
        if (HasCooler())
        {
            defineNumber(&CoolerNP);
            loadConfig(true, "CCD_COOLER_POWER");
            defineSwitch(&CoolerSP);
            loadConfig(true, "CCD_COOLER");
        }
        // Even if there is no cooler, we define temperature property as READ ONLY
        else
        {
            TemperatureNP.p = IP_RO;
            defineNumber(&TemperatureNP);
        }

        if (m_isHorizon)
        {
            defineSwitch(&ControlPresetsSP);
            defineNumber(&ControlNP);
        }

        if (m_CameraFlags & ARTEMIS_PROPERTIES_CAMERAFLAGS_HAS_FILTERWHEEL)
        {
            INDI::FilterInterface::updateProperties();
        }

        defineText(&VersionInfoSP);
    }
    else
    {
        if (HasCooler())
        {
            deleteProperty(CoolerNP.name);
            deleteProperty(CoolerSP.name);
        }
        else
            deleteProperty(TemperatureNP.name);

        if (m_isHorizon)
        {
            deleteProperty(ControlPresetsSP.name);
            deleteProperty(ControlNP.name);
        }

        if (m_CameraFlags & ARTEMIS_PROPERTIES_CAMERAFLAGS_HAS_FILTERWHEEL)
        {
            INDI::FilterInterface::updateProperties();
        }

        deleteProperty(VersionInfoSP.name);
    }

    return true;
}

bool ATIKCCD::Connect()
{
    LOGF_DEBUG("Attempting to open %s...", name);

    hCam = ArtemisConnect(m_iDevice);

    if (hCam == nullptr)
    {
        LOGF_ERROR("Failed to connected to %s", name);
        return false;
    }

    return setupParams();
}

bool ATIKCCD::setupParams()
{
    ARTEMISPROPERTIES pProp;

    int rc = ArtemisProperties(hCam, &pProp);

    if (rc != ARTEMIS_OK)
    {
        LOGF_ERROR("Failed to inquire camera properties (%d)", rc);
        return false;
    }

    // Camera & Pixel properties
    // FIXME is it always 16bit depth?
    SetCCDParams(pProp.nPixelsX, pProp.nPixelsY, 16, pProp.PixelMicronsX, pProp.PixelMicronsY);
    // Set frame buffer size
    PrimaryCCD.setFrameBufferSize(PrimaryCCD.getXRes() * PrimaryCCD.getYRes() * PrimaryCCD.getBPP() / 8, false);

    m_CameraFlags = pProp.cameraflags;
    LOGF_DEBUG("Camera flags: %d", m_CameraFlags);

    int binX = 1, binY = 1;

    rc = ArtemisGetMaxBin(hCam, &binX, &binY);

    if (rc != ARTEMIS_OK)
    {
        LOGF_ERROR("Failed to inquire camera max binning (%d)", rc);
    }

    PrimaryCCD.setMinMaxStep("CCD_EXPOSURE", "CCD_EXPOSURE_VALUE", 0.001, 3600, 1, false);
    PrimaryCCD.setMinMaxStep("CCD_BINNING", "HOR_BIN", 1, binX, 1, false);
    PrimaryCCD.setMinMaxStep("CCD_BINNING", "VER_BIN", 1, binY, 1, false);

    char firmware[8] = {0};
    snprintf(firmware, sizeof(firmware), "%d.%d", pProp.Protocol >> 8, pProp.Protocol & 0xff);
    IUSaveText(&VersionInfoS[VERSION_FIRMWARE], firmware);
    LOGF_INFO("Detected camera %s %s with firmware %s", pProp.Manufacturer, pProp.Description, firmware);

    uint32_t cap = 0;

    // All Atik cameras can abort and subframe
    cap = CCD_CAN_ABORT | CCD_CAN_SUBFRAME;

    // Can we bin?
    if (binX > 1)
    {
        cap |= CCD_CAN_BIN;
        LOG_DEBUG("Camera can bin.");
    }

    // Do we have color or mono camera?
    ARTEMISCOLOURTYPE colorType;
    rc = ArtemisColourProperties(hCam, &colorType, &normalOffsetX, &normalOffsetY, &previewOffsetX, &previewOffsetY);

    if (rc != ARTEMIS_OK)
    {
        LOGF_ERROR("Failed to inquire camera color (%d). Assuming Mono.", rc);
    }
    if (colorType == ARTEMIS_COLOUR_RGGB)
    {
        cap |= CCD_HAS_BAYER;
        IUSaveText(&BayerT[0], std::to_string(normalOffsetX).c_str());
        IUSaveText(&BayerT[1], std::to_string(normalOffsetY).c_str());
    }

    LOGF_DEBUG("Camera is %s.", colorType == ARTEMIS_COLOUR_RGGB ? "Color" : "Mono");

    // Do we have temperature?
    rc = ArtemisTemperatureSensorInfo(hCam, 0, &m_TemperatureSensorsCount);
    LOGF_DEBUG("Camera has %d temperature sensor(s).", m_TemperatureSensorsCount);
    if (m_TemperatureSensorsCount > 0)
    {
        // Do we have cooler control?
        int flags, level, minlvl, maxlvl, setpoint;
        rc = ArtemisCoolingInfo(hCam, &flags, &level, &minlvl, &maxlvl, &setpoint);
        if (flags & 0x1)
        {
            LOG_DEBUG("Camera supports cooling control.");
            cap |= CCD_HAS_COOLER;
        }

        genTimerID = SetTimer(TEMP_TIMER_MS);
    }

    // Do we have mechanical shutter?
    if (m_CameraFlags & ARTEMIS_PROPERTIES_CAMERAFLAGS_HAS_SHUTTER)
    {
        LOG_DEBUG("Camera has mechanical shutter.");
        cap |= CCD_HAS_SHUTTER;
    }

    // Do we have guide port?
    if (m_CameraFlags & ARTEMIS_PROPERTIES_CAMERAFLAGS_HAS_GUIDE_PORT)
    {
        LOG_DEBUG("Camera has guide port.");
        cap |= CCD_HAS_ST4_PORT;
    }

    // Done with the capabilities!
    SetCCDCapability(cap);

    // Check if camra has internal filter wheel
    if (m_CameraFlags & ARTEMIS_PROPERTIES_CAMERAFLAGS_HAS_FILTERWHEEL)
    {
        int numFilters, moving, currentPos, targetPos;
        rc = ArtemisFilterWheelInfo(hCam, &numFilters, &moving, &currentPos, &targetPos);
        if (rc != ARTEMIS_OK)
        {
            LOGF_ERROR("Failed to inquire internal filter wheel info (%d). Filter wheel functions are disabled.", rc);
        }
        else
        {
            setDriverInterface(getDriverInterface() | FILTER_INTERFACE);

            FilterSlotN[0].min = 1;
            FilterSlotN[0].max = numFilters;

            LOGF_INFO("Detected %d-position internal filter wheel.", numFilters);
        }
    }

    // Check if we have Horizon camera
    m_isHorizon = ArtemisHasCameraSpecificOption(hCam, 1);
    if (m_isHorizon)
    {
        uint8_t data[2] = {0};
        int len = 0, index = 0;
        ArtemisCameraSpecificOptionGetData(hCam, ID_AtikHorizonGOPresetMode, data, 2, &len);
        index = *(reinterpret_cast<uint16_t*>(&data));
        LOGF_DEBUG("Horizon current GO mode: data[0] %d data[1] %d index %d", data[0], data[1], index);
        IUResetSwitch(&ControlPresetsSP);
        ControlPresetsS[index].s = ISS_ON;

        // Get Gain & Offset valuse
        ArtemisCameraSpecificOptionGetData(hCam, ID_AtikHorizonGOCustomGain, data, 2, &len);
        index = *(reinterpret_cast<uint16_t*>(&data));
        LOGF_DEBUG("Horizon current gain: data[0] %d data[1] %d value %d", data[0], data[1], index);

        ArtemisCameraSpecificOptionGetData(hCam, ID_AtikHorizonGOCustomOffset, data, 2, &len);
        index = *(reinterpret_cast<uint16_t*>(&data));
        LOGF_DEBUG("Horizon current offset: data[0] %d data[1] %d value %d", data[0], data[1], index);
    }

    // Create imaging thread
    threadRequest = StateIdle;
    threadState = StateNone;
    int stat = pthread_create(&imagingThread, nullptr, &imagingHelper, this);
    if (stat != 0)
    {
        LOGF_ERROR("Error creating imaging thread (%d)", stat);
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

bool ATIKCCD::Disconnect()
{
    ImageState  tState;
    LOGF_DEBUG("Closing %s...", name);

    stopTimerNS();
    stopTimerWE();
    RemoveTimer(genTimerID);
    genTimerID = -1;

    pthread_mutex_lock(&condMutex);
    tState = threadState;
    threadRequest = StateTerminate;
    pthread_cond_signal(&cv);
    pthread_mutex_unlock(&condMutex);
    pthread_join(imagingThread, nullptr);
    tState = StateNone;
    if (isSimulation() == false)
    {
        if (tState == StateExposure)
            ArtemisStopExposure(hCam);
        ArtemisDisconnect(hCam);
    }

    LOG_INFO("Camera is offline.");

    return true;
}

bool ATIKCCD::ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n)
{
    if (dev != nullptr && !strcmp(dev, getDeviceName()))
    {
        if (strcmp(name, FilterNameTP->name) == 0)
        {
            INDI::FilterInterface::processText(dev, name, texts, names, n);
            return true;
        }
    }

    return INDI::CCD::ISNewText(dev, name, texts, names, n);
}

bool ATIKCCD::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    if (dev != nullptr && !strcmp(dev, getDeviceName()))
    {
        if (!strcmp(name, FilterSlotNP.name))
        {
            INDI::FilterInterface::processNumber(dev, name, values, names, n);
            return true;
        }
        else if (!strcmp(name, ControlNP.name))
        {
            std::vector<double> oldValues;
            for (int i = 0; i < ControlNP.nnp; i++)
                oldValues.push_back(ControlN[i].value);

            if (IUUpdateNumber(&ControlNP, values, names, n) < 0)
            {
                ControlNP.s = IPS_ALERT;
                IDSetNumber(&ControlNP, nullptr);
                return true;
            }

            for (int i = 0; i < ControlNP.nnp; i++)
            {
                if (ControlN[i].value == oldValues[i])
                    continue;

                // Gain 0, Offset 1. We add 5 to them to get them to Atik horizon IDs
                uint16_t value = static_cast<uint16_t>(ControlN[i].value);
                ArtemisCameraSpecificOptionSetData(hCam, i + 5, reinterpret_cast<uint8_t*>(&value), 2);
            }

            ControlNP.s = IPS_OK;
            IDSetNumber(&ControlNP, nullptr);
            return true;
        }
    }

    return INDI::CCD::ISNewNumber(dev, name, values, names, n);
}

bool ATIKCCD::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if (dev != nullptr && !strcmp(dev, getDeviceName()))
    {
        // Gain/Offset Presets
        if (!strcmp(name, ControlPresetsSP.name))
        {
            int prevIndex = IUFindOnSwitchIndex(&ControlPresetsSP);
            IUUpdateSwitch(&ControlPresetsSP, states, names, n);
            int targetIndex = IUFindOnSwitchIndex(&ControlPresetsSP);
            uint16_t value = static_cast<uint16_t>(targetIndex + 2);
            uint8_t *data = reinterpret_cast<uint8_t*>(&value);
            int rc = ArtemisCameraSpecificOptionSetData(hCam, ID_AtikHorizonGOPresetMode, data, 2);
            if (rc != ARTEMIS_OK)
            {
                ControlPresetsSP.s = IPS_ALERT;
                IUResetSwitch(&ControlPresetsSP);
                ControlPresetsS[prevIndex].s = ISS_ON;
            }
            else
                ControlPresetsSP.s = IPS_OK;

            IDSetSwitch(&ControlPresetsSP, nullptr);
            return true;
        }

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

    return INDI::CCD::ISNewSwitch(dev, name, states, names, n);
}

int ATIKCCD::SetTemperature(double temperature)
{
    // If there difference, for example, is less than 0.1 degrees, let's immediately return OK.
    if (fabs(temperature - TemperatureN[0].value) < TEMP_THRESHOLD)
        return 1;

    // setpoint is int 1/100 of a degree C.
    int setpoint = static_cast<int>(temperature * 100);

    int rc = ArtemisSetCooling(hCam, setpoint);
    if (rc != ARTEMIS_OK)
    {
        LOGF_ERROR("Failed to set temperature (%d).", rc);
        return -1;
    }

    // Otherwise, we set the temperature request and we update the status in TimerHit() function.
    TemperatureRequest = temperature;
    LOGF_INFO("Setting CCD temperature to %+06.2f C", temperature);

    activateCooler(true);

    return 0;
}

bool ATIKCCD::activateCooler(bool enable)
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
        int rc = ArtemisCoolerWarmUp(hCam);
        if (rc != ARTEMIS_OK)
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

bool ATIKCCD::StartExposure(float duration)
{
    PrimaryCCD.setExposureDuration(duration);
    ExposureRequest = duration;

    // Camera needs to be in idle state to start exposure after previous abort
    int maxWaitCount = 1000; // 1000 * 0.1s = 100s
    while (ArtemisCameraState(hCam) != CAMERA_IDLE && --maxWaitCount > 0)
    {
        LOG_DEBUG("Waiting camera to be idle...");
        usleep(100000);
    }
    if (maxWaitCount == 0)
    {
        LOG_ERROR("Camera not in idle state, can't start exposure");
        return false;
    }

    LOGF_DEBUG("Start Exposure : %.3fs", duration);

    //    if (m_CameraFlags & ARTEMIS_PROPERTIES_CAMERAFLAGS_HAS_SHUTTER)
    //    {
    //        if (PrimaryCCD.getFrameType() == INDI::CCDChip::DARK_FRAME ||
    //            PrimaryCCD.getFrameType() == INDI::CCDChip::BIAS_FRAME)
    //        {
    //            ArtemisCloseShutter(hCam);
    //        }
    //        else
    //        {
    //            ArtemisOpenShutter(hCam);
    //        }
    //    }

    ArtemisSetDarkMode(hCam, PrimaryCCD.getFrameType() == INDI::CCDChip::DARK_FRAME ||
                       PrimaryCCD.getFrameType() == INDI::CCDChip::BIAS_FRAME);

    int rc = ArtemisStartExposure(hCam, duration);

    if (rc != ARTEMIS_OK)
    {
        LOGF_ERROR("Failed to start exposure (%d).", rc);
        return false;
    }

    gettimeofday(&ExpStart, nullptr);
    if (ExposureRequest > VERBOSE_EXPOSURE)
        LOGF_INFO("Taking a %g seconds frame...", ExposureRequest);

    InExposure = true;
    pthread_mutex_lock(&condMutex);
    threadRequest = StateExposure;
    pthread_cond_signal(&cv);
    pthread_mutex_unlock(&condMutex);

    return true;
}

/////////////////////////////////////////////////////////
/// \brief ATIKCCD::AbortExposure Abort camera exposure
/// \return true if successful, false otherwise.
////////////////////////////////////////////////////////////
bool ATIKCCD::AbortExposure()
{
    LOG_DEBUG("Aborting camera exposure...");
    pthread_mutex_lock(&condMutex);
    threadRequest = StateAbort;
    pthread_cond_signal(&cv);
    while (threadState == StateExposure)
    {
        pthread_cond_wait(&cv, &condMutex);
    }
    pthread_mutex_unlock(&condMutex);
    ArtemisStopExposure(hCam);
    InExposure = false;
    return true;
}

/////////////////////////////////////////////////////////
/// Updates CCD sub frame
/////////////////////////////////////////////////////////
bool ATIKCCD::UpdateCCDFrame(int x, int y, int w, int h)
{
    int rc = ArtemisSubframe(hCam, x, y, w, h);
    if (rc != ARTEMIS_OK)
    {
        LOGF_ERROR("Error settings subframe: (%d,%d,%d,%d) with binning (%d,%d).", x, y, w, h, PrimaryCCD.getBinX(), PrimaryCCD.getBinY());
        return false;
    }

    // Set UNBINNED coords
    PrimaryCCD.setFrame(x, y, w, h);

    // Total bytes required for image buffer
    PrimaryCCD.setFrameBufferSize(w / PrimaryCCD.getBinX() * h / PrimaryCCD.getBinY() * PrimaryCCD.getBPP() / 8, false);
    return true;
}

/////////////////////////////////////////////////////////
/// Update CCD bin mode
/////////////////////////////////////////////////////////
bool ATIKCCD::UpdateCCDBin(int binx, int biny)
{
    int rc = ArtemisBin(hCam, binx, biny);

    if (rc != ARTEMIS_OK)
        return false;

    PrimaryCCD.setBin(binx, biny);

    return UpdateCCDFrame(PrimaryCCD.getSubX(), PrimaryCCD.getSubY(), PrimaryCCD.getSubW(), PrimaryCCD.getSubH());
}

/////////////////////////////////////////////////////////
/// Download from CCD
/////////////////////////////////////////////////////////
bool ATIKCCD::grabImage()
{
    //uint8_t *image = PrimaryCCD.getFrameBuffer();
    int x, y, w, h, binx, biny;

    int rc = ArtemisGetImageData(hCam, &x, &y, &w, &h, &binx, &biny);
    if (rc != ARTEMIS_OK)
        return false;

    int bufferSize = w * binx * h * biny * PrimaryCCD.getBPP() / 8;
    if ( bufferSize < PrimaryCCD.getFrameBufferSize())
    {
        LOGF_WARN("Image size is unexpected. Expecting %d bytes but received %d bytes.", PrimaryCCD.getFrameBufferSize(), bufferSize);
        PrimaryCCD.setFrameBufferSize(bufferSize, false);
    }

    std::unique_lock<std::mutex> guard(ccdBufferLock);
    PrimaryCCD.setFrameBuffer(reinterpret_cast<uint8_t*>(ArtemisImageBuffer(hCam)));
    guard.unlock();

    if (ExposureRequest > VERBOSE_EXPOSURE)
        LOG_INFO("Download complete.");

    ExposureComplete(&PrimaryCCD);
    return true;
}

/////////////////////////////////////////////////////////
/// Cooler & Filter Wheel monitoring
/////////////////////////////////////////////////////////
void ATIKCCD::TimerHit()
{
    double currentTemperature = TemperatureN[0].value;

    int flags, level, minlvl, maxlvl, setpoint;

    pthread_mutex_lock(&accessMutex);
    int rc = ArtemisCoolingInfo(hCam, &flags, &level, &minlvl, &maxlvl, &setpoint);
    pthread_mutex_unlock(&accessMutex);

    if (rc != ARTEMIS_OK)
    {
        LOGF_ERROR("Cooling Info inquiry failed (%d)", rc);
        genTimerID = SetTimer(TEMP_TIMER_MS);
        return;
    }

    LOGF_DEBUG("Cooling: flags (%d) level (%d), minlvl (%d), maxlvl (%d), setpoint (%d)", flags, level, minlvl, maxlvl, setpoint);

    int temperature = 0;
    pthread_mutex_lock(&accessMutex);
    rc = ArtemisTemperatureSensorInfo(hCam, 1, &temperature);
    pthread_mutex_unlock(&accessMutex);
    TemperatureN[0].value = temperature / 100.0;

    switch (TemperatureNP.s)
    {
        case IPS_IDLE:
        case IPS_OK:
            if (fabs(currentTemperature - TemperatureN[0].value)  > TEMP_THRESHOLD / 10.0)
            {
                IDSetNumber(&TemperatureNP, nullptr);
            }
            break;

        case IPS_ALERT:
            break;

        case IPS_BUSY:
            // If we're within threshold, let's make it BUSY ---> OK
            if (fabs(TemperatureRequest - TemperatureN[0].value)  <= TEMP_THRESHOLD)
            {
                TemperatureNP.s = IPS_OK;
            }
            IDSetNumber(&TemperatureNP, nullptr);
            break;
    }

    if (HasCooler())
    {
        bool coolerChanged = false;
        double coolerPower = static_cast<double>(level) / maxlvl * 100.0;
        if (fabs(CoolerN[0].value - coolerPower) > 0.01)
        {
            CoolerN[0].value = coolerPower;
            coolerChanged = true;
        }

        // b5 0 = normal control 1=warming up
        // b6 0 = cooling off 1 = cooling on
        if (!(flags & 0x20) && // Normal Control?
                (flags & 0x40))   // Cooling On?
        {
            if (CoolerNP.s != IPS_BUSY)
                coolerChanged = true;
            CoolerNP.s = IPS_BUSY;
        }
        // Otherwise cooler is either warming up or not active
        else
        {
            if (CoolerNP.s != IPS_IDLE)
                coolerChanged = true;
            CoolerNP.s = IPS_IDLE;
        }

        if (coolerChanged)
            IDSetNumber(&CoolerNP, nullptr);
    }

    // If filter wheel is in motion
    if (FilterSlotNP.s == IPS_BUSY)
    {
        int numFilters, moving, currentPos, targetPos;
        pthread_mutex_lock(&accessMutex);
        int rc = ArtemisFilterWheelInfo(hCam, &numFilters, &moving, &currentPos, &targetPos);
        pthread_mutex_unlock(&accessMutex);

        if (rc != ARTEMIS_OK)
        {
            LOGF_ERROR("Querying internal filter wheel failed (%d).", rc);
        }
        else
        {
            if (moving == 0 && currentPos == targetPos)
            {
                SelectFilterDone(currentPos + 1);
            }
        }

    }

    genTimerID = SetTimer(TEMP_TIMER_MS);
}


/////////////////////////////////////////////////////////
/// Timer Helper NS
/////////////////////////////////////////////////////////
void ATIKCCD::TimerHelperNS(void *context)
{
    static_cast<ATIKCCD *>(context)->stopTimerNS();
}

/////////////////////////////////////////////////////////
/// Resets N/S Guide to OK after timeout
/////////////////////////////////////////////////////////
void ATIKCCD::stopTimerNS()
{
    if (NStimerID != -1)
    {
        GuideComplete(AXIS_DE);
        IERmTimer(NStimerID);
        NStimerID = -1;
    }
}

/////////////////////////////////////////////////////////
/// Guide North/South
/////////////////////////////////////////////////////////
IPState ATIKCCD::guidePulseNS(uint32_t ms, AtikGuideDirection dir, const char *dirName)
{
    stopTimerNS();
    NSDir = dir;
    NSDirName = dirName;

    LOGF_DEBUG("Starting %s guide for %f ms",  NSDirName, ms);

    int rc = ArtemisPulseGuide(hCam, dir, ms);
    if (rc != ARTEMIS_OK)
    {
        return IPS_ALERT;
    }

    NStimerID = IEAddTimer(ms, ATIKCCD::TimerHelperNS, this);
    return IPS_BUSY;
}

/////////////////////////////////////////////////////////
/// Guide North
/////////////////////////////////////////////////////////
IPState ATIKCCD::GuideNorth(uint32_t ms)
{
    return guidePulseNS(ms, NORTH, "North");
}

/////////////////////////////////////////////////////////
/// Guide South
/////////////////////////////////////////////////////////
IPState ATIKCCD::GuideSouth(uint32_t ms)
{
    return guidePulseNS(ms, SOUTH, "South");
}

/////////////////////////////////////////////////////////
/// Timer Helper for West/East guide pulses
/////////////////////////////////////////////////////////
void ATIKCCD::TimerHelperWE(void *context)
{
    static_cast<ATIKCCD *>(context)->stopTimerWE();
}

/////////////////////////////////////////////////////////
/// Stop West/East pulses
/////////////////////////////////////////////////////////
void ATIKCCD::stopTimerWE()
{
    if (WEtimerID != -1)
    {
        GuideComplete(AXIS_RA);
        IERmTimer(WEtimerID);
        WEtimerID = -1;
    }
}

/////////////////////////////////////////////////////////
/// Start West/East guide pulses
/////////////////////////////////////////////////////////
IPState ATIKCCD::guidePulseWE(uint32_t ms, AtikGuideDirection dir, const char *dirName)
{
    WEDir = dir;
    WEDirName = dirName;

    LOGF_DEBUG("Starting %s guide for %f ms", WEDirName, ms);

    int rc = ArtemisPulseGuide(hCam, dir, ms);
    if (rc != ARTEMIS_OK)
    {
        return IPS_ALERT;
    }

    WEtimerID = IEAddTimer(ms, ATIKCCD::TimerHelperWE, this);
    return IPS_BUSY;
}

/////////////////////////////////////////////////////////
/// East guide pulse
/////////////////////////////////////////////////////////
IPState ATIKCCD::GuideEast(uint32_t ms)
{
    return guidePulseWE(ms, EAST, "East");
}

/////////////////////////////////////////////////////////
/// West guide pulse
/////////////////////////////////////////////////////////
IPState ATIKCCD::GuideWest(uint32_t ms)
{
    return guidePulseWE(ms, WEST, "West");
}

/////////////////////////////////////////////////////////
/// Helper function
/////////////////////////////////////////////////////////
void *ATIKCCD::imagingHelper(void *context)
{
    return static_cast<ATIKCCD *>(context)->imagingThreadEntry();
}

/////////////////////////////////////////////////////////
/// Dedicated imaging thread
/////////////////////////////////////////////////////////
void *ATIKCCD::imagingThreadEntry()
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
            checkExposureProgress();
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

/////////////////////////////////////////////////////////
/// Dedicated imaging thread
/////////////////////////////////////////////////////////
void ATIKCCD::checkExposureProgress()
{
    int expRetry = 0;
    int uSecs = 1000000;

    while (threadRequest == StateExposure)
    {
        pthread_mutex_unlock(&condMutex);
        pthread_mutex_lock(&accessMutex);
        if (ArtemisImageReady(hCam))
        {
            InExposure = false;
            PrimaryCCD.setExposureLeft(0.0);
            if (ExposureRequest > VERBOSE_EXPOSURE)
                DEBUG(INDI::Logger::DBG_SESSION, "Exposure done, downloading image...");
            pthread_mutex_lock(&condMutex);
            exposureSetRequest(StateIdle);
            pthread_mutex_unlock(&condMutex);
            grabImage();
            pthread_mutex_lock(&condMutex);
            pthread_mutex_unlock(&accessMutex);
            break;
        }

        int state = ArtemisCameraState(hCam);
        pthread_mutex_unlock(&accessMutex);
        if (state == -1)
        {
            if (++expRetry < MAX_EXP_RETRIES)
            {
                if (threadRequest == StateExposure)
                {
                    LOG_DEBUG("ASIGetExpStatus failed. Restarting exposure...");
                }
                InExposure = false;
                pthread_mutex_lock(&accessMutex);
                ArtemisStopExposure(hCam);
                pthread_mutex_unlock(&accessMutex);
                usleep(100000);
                pthread_mutex_lock(&condMutex);
                exposureSetRequest(StateRestartExposure);
                break;
            }
            else
            {
                if (threadRequest == StateExposure)
                {
                    LOGF_ERROR("Exposure failed after %d attempts.", expRetry);
                }
                pthread_mutex_lock(&accessMutex);
                ArtemisStopExposure(hCam);
                pthread_mutex_unlock(&accessMutex);
                PrimaryCCD.setExposureFailed();
                usleep(100000);
                pthread_mutex_lock(&condMutex);
                exposureSetRequest(StateIdle);
                break;
            }
        }

        pthread_mutex_lock(&accessMutex);
        float timeLeft = ArtemisExposureTimeRemaining(hCam);
        pthread_mutex_unlock(&accessMutex);
        if (timeLeft > 1.1)
        {
            float fraction = timeLeft - static_cast<float>(static_cast<int>(timeLeft));
            if (fraction >= 0.005)
            {
                uSecs = (fraction * 1000000.0f);
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

        usleep(uSecs);
        pthread_mutex_lock(&condMutex);
    }
}

/////////////////////////////////////////////////////////
/// Update Exposure Request
/////////////////////////////////////////////////////////
void ATIKCCD::exposureSetRequest(ImageState request)
{
    if (threadRequest == StateExposure)
    {
        threadRequest = request;
    }
}

/////////////////////////////////////////////////////////
/// Add applicable FITS keywords to header
/////////////////////////////////////////////////////////
void ATIKCCD::addFITSKeywords(fitsfile *fptr, INDI::CCDChip *targetChip)
{
    INDI::CCD::addFITSKeywords(fptr, targetChip);

    if (m_isHorizon)
    {
        int status = 0;
        fits_update_key_dbl(fptr, "Gain", ControlN[CONTROL_GAIN].value, 3, "Gain", &status);
        fits_update_key_dbl(fptr, "Offset", ControlN[CONTROL_OFFSET].value, 3, "Offset", &status);
    }
}

/////////////////////////////////////////////////////////
/// Save properties in config file
/////////////////////////////////////////////////////////
bool ATIKCCD::saveConfigItems(FILE *fp)
{
    INDI::CCD::saveConfigItems(fp);

    if (HasCooler())
    {
        IUSaveConfigNumber(fp, &CoolerNP);
        IUSaveConfigSwitch(fp, &CoolerSP);
    }

    if (m_isHorizon && IUFindOnSwitchIndex(&ControlPresetsSP) == PRESET_CUSTOM)
        IUSaveConfigNumber(fp, &ControlNP);

    if (m_CameraFlags & ARTEMIS_PROPERTIES_CAMERAFLAGS_HAS_FILTERWHEEL)
        INDI::FilterInterface::saveConfigItems(fp);

    return true;
}

bool ATIKCCD::SelectFilter(int targetFilter)
{
    LOGF_DEBUG("Selecting filter %d", targetFilter);
    int rc = ArtemisFilterWheelMove(hCam, targetFilter - 1);
    return (rc == ARTEMIS_OK);
}

int ATIKCCD::QueryFilter()
{
    int numFilters = 0, moving = 0, currentPos = 0, targetPos = 0;
    int rc = ArtemisFilterWheelInfo(hCam, &numFilters, &moving, &currentPos, &targetPos);

    if (rc != ARTEMIS_OK)
    {
        LOGF_ERROR("Querying internal filter wheel failed (%d).", rc);
        return -1;
    }
    else
        LOGF_DEBUG("CFW Filters: %d moving: %d current: %d target: %d", numFilters, moving, currentPos, targetPos);

    return currentPos + 1;
}

void ATIKCCD::debugTriggered(bool enable)
{
    if (enable)
        ArtemisSetDebugCallbackContext(this, &ATIKCCD::debugCallbackHelper);
    else
        ArtemisSetDebugCallbackContext(nullptr, nullptr);
}

void ATIKCCD::debugCallbackHelper(void *context, const char *message)
{
    static_cast<ATIKCCD*>(context)->debugCallback(message);
}

void ATIKCCD::debugCallback(const char *message)
{
    LOGF_DEBUG("%s", message);
}
