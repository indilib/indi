/*
 Toupcam CCD Driver

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

#include "indi_toupcam.h"

#include "config.h"

#include <stream/streammanager.h>

#include <math.h>
#include <unistd.h>

#define MAX_EXP_RETRIES         3
#define VERBOSE_EXPOSURE        3
#define TEMP_TIMER_MS           1000 /* Temperature polling time (ms) */
#define TEMP_THRESHOLD          .25  /* Differential temperature threshold (C)*/
#define MAX_DEVICES             4    /* Max device cameraCount */

#define CONTROL_TAB "Controls"
#define LEVEL_TAB "Levels"

#ifndef MAKEFOURCC
#define MAKEFOURCC(ch0, ch1, ch2, ch3) \
    (static_cast<uint32_t>(static_cast<uint8_t>(ch0)) \
    | (static_cast<uint32_t>(static_cast<uint8_t>(ch1)) << 8) \
    | (static_cast<uint32_t>(static_cast<uint8_t>(ch2)) << 16) \
    | (static_cast<uint32_t>(static_cast<uint8_t>(ch3)) << 24))
#endif /* defined(MAKEFOURCC) */

#define FMT_GBRG    MAKEFOURCC('G', 'B', 'R', 'G')
#define FMT_RGGB    MAKEFOURCC('R', 'G', 'G', 'B')
#define FMT_BGGR    MAKEFOURCC('B', 'G', 'G', 'R')
#define FMT_GRBG    MAKEFOURCC('G', 'R', 'B', 'G')
#define FMT_YYYY    MAKEFOURCC('Y', 'Y', 'Y', 'Y')
#define FMT_YUV411  MAKEFOURCC('Y', '4', '1', '1')
#define FMT_YUV422  MAKEFOURCC('V', 'U', 'Y', 'Y')
#define FMT_YUV444  MAKEFOURCC('Y', '4', '4', '4')
#define FMT_RGB888  MAKEFOURCC('R', 'G', 'B', '8')

static int iConnectedCamerasCount;
static ToupcamInstV2 pToupCameraInfo[TOUPCAM_MAX];
static TOUPCAM *cameras[TOUPCAM_MAX];

/********************************************************************************/
/* HRESULT                                                                      */
/*    |----------------|---------------------------------------|------------|   */
/*    | S_OK           |   Operation successful                | 0x00000000 |   */
/*    | S_FALSE        |   Operation successful                | 0x00000001 |   */
/*    | E_FAIL         |   Unspecified failure                 | 0x80004005 |   */
/*    | E_INVALIDARG   |   One or more arguments are not valid | 0x80070057 |   */
/*    | E_NOTIMPL      |   Not supported or not implemented    | 0x80004001 |   */
/*    | E_NOINTERFACE  |   Interface not supported             | 0x80004002 |   */
/*    | E_POINTER      |   Pointer that is not valid           | 0x80004003 |   */
/*    | E_UNEXPECTED   |   Unexpected failure                  | 0x8000FFFF |   */
/*    | E_OUTOFMEMORY  |   Out of memory                       | 0x8007000E |   */
/*    | E_WRONG_THREAD |   call function in the wrong thread   | 0x8001010E |   */
/*    |----------------|---------------------------------------|------------|   */
/********************************************************************************/
std::map<int, std::string> TOUPCAM::errorCodes =
{
    {0x00000000, "Operation successful"},
    {0x00000001, "Operation failed"},
    {0x80004005, "Unspecified failure"},
    {0x80070057, "One or more arguments are not valid"},
    {0x80004001, "Not supported or not implemented"},
    {0x80004002, "Interface not supported"},
    {0x80004003, "Pointer that is not valid"},
    {0x8000FFFF, "Unexpected failure"},
    {0x8007000E, "Out of memory"},
    {0x8001010E, "call function in the wrong thread"}
};

static void cleanup()
{
    for (int i = 0; i < iConnectedCamerasCount; i++)
    {
        delete cameras[i];
    }
}

void TOUPCAM_ISInit()
{
    static bool isInit = false;
    if (!isInit)
    {
        iConnectedCamerasCount = Toupcam_EnumV2(pToupCameraInfo);
        if (iConnectedCamerasCount <= 0)
            IDLog("No ToupCam detected. Power on?");
        else
        {
            for (int i = 0; i < iConnectedCamerasCount; i++)
            {
                cameras[i] = new TOUPCAM(&pToupCameraInfo[i]);
            }
        }

        atexit(cleanup);
        isInit = true;
    }
}

void ISGetProperties(const char *dev)
{
    TOUPCAM_ISInit();

    if (iConnectedCamerasCount == 0)
    {
        IDMessage(nullptr, "No ToupCam detected. Power on?");
        return;
    }

    for (int i = 0; i < iConnectedCamerasCount; i++)
    {
        TOUPCAM *camera = cameras[i];
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
    TOUPCAM_ISInit();
    for (int i = 0; i < iConnectedCamerasCount; i++)
    {
        TOUPCAM *camera = cameras[i];
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
    TOUPCAM_ISInit();
    for (int i = 0; i < iConnectedCamerasCount; i++)
    {
        TOUPCAM *camera = cameras[i];
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
    TOUPCAM_ISInit();
    for (int i = 0; i < iConnectedCamerasCount; i++)
    {
        TOUPCAM *camera = cameras[i];
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
    TOUPCAM_ISInit();

    for (int i = 0; i < iConnectedCamerasCount; i++)
    {
        TOUPCAM *camera = cameras[i];
        camera->ISSnoopDevice(root);
    }
}

TOUPCAM::TOUPCAM(const ToupcamInstV2 *instance) : m_Instance(instance)
{
    setVersion(TOUPCAM_VERSION_MAJOR, TOUPCAM_VERSION_MINOR);

    WEtimerID = NStimerID = -1;
    NSDir = TOUPCAM_NORTH;
    WEDir = TOUPCAM_WEST;

    snprintf(this->name, MAXINDIDEVICE, "Toupcam %s", instance->displayname);
    setDeviceName(this->name);
}

const char *TOUPCAM::getDefaultName()
{
    return "Toupcam";
}

bool TOUPCAM::initProperties()
{
    INDI::CCD::initProperties();

    ///////////////////////////////////////////////////////////////////////////////////
    /// Cooler Control
    ///////////////////////////////////////////////////////////////////////////////////
    IUFillSwitch(&CoolerS[0], "COOLER_ON", "ON", ISS_OFF);
    IUFillSwitch(&CoolerS[1], "COOLER_OFF", "OFF", ISS_ON);
    IUFillSwitchVector(&CoolerSP, CoolerS, 2, getDeviceName(), "CCD_COOLER", "Cooler", MAIN_CONTROL_TAB, IP_WO,
                       ISR_1OFMANY, 0, IPS_IDLE);

    ///////////////////////////////////////////////////////////////////////////////////
    /// Controls
    ///////////////////////////////////////////////////////////////////////////////////
    IUFillNumber(&ControlN[TC_GAIN], "Gain", "Gain", "%.f", 0, 400, 10, 0);
    IUFillNumber(&ControlN[TC_CONTRAST], "Contrast", "Contrast", "%.f", -100.0, 100, 10, 0);
    IUFillNumber(&ControlN[TC_HUE], "Hue", "Hue", "%.f", -180.0, 180, 10, 0);
    IUFillNumber(&ControlN[TC_SATURATION], "Saturation", "Saturation", "%.f", 0, 255, 10, 128);
    IUFillNumber(&ControlN[TC_BRIGHTNESS], "Brightness", "Brightness", "%.f", -64, 64, 8, 0);
    IUFillNumber(&ControlN[TC_GAMMA], "Gamma", "Gamma", "%.f", 20, 180, 10, 100);
    IUFillNumber(&ControlN[TC_SPEED], "Speed", "Speed", "%.f", 0, 10, 1, 0);
    IUFillNumberVector(&ControlNP, ControlN, 7, getDeviceName(), "CCD_CONTROLS", "Controls", CONTROL_TAB, IP_RW, 60,
                       IPS_IDLE);


    ///////////////////////////////////////////////////////////////////////////////////
    // Black Level
    ///////////////////////////////////////////////////////////////////////////////////
    IUFillNumber(&BlackBalanceN[TC_BLACK_R], "TC_BLACK_R", "Red", "%.f", 0, 255, 10, 0);
    IUFillNumber(&BlackBalanceN[TC_BLACK_G], "TC_BLACK_G", "Green", "%.f", 0, 255, 10, 0);
    IUFillNumber(&BlackBalanceN[TC_BLACK_B], "TC_BLACK_B", "Blue", "%.f", 0, 255, 10, 0);
    IUFillNumberVector(&BlackBalanceNP, BlackBalanceN, 3, getDeviceName(), "CCD_BLACK_LEVEL", "Black Level", LEVEL_TAB, IP_RW, 60, IPS_IDLE);

    ///////////////////////////////////////////////////////////////////////////////////
    // R/G/B/Y levels
    ///////////////////////////////////////////////////////////////////////////////////
    IUFillNumber(&LevelRangeN[TC_LO_R], "TC_LO_R", "Low Red", "%.f", 0, 255, 10, 0);
    IUFillNumber(&LevelRangeN[TC_HI_R], "TC_HI_R", "High Red", "%.f", 0, 255, 10, 0);
    IUFillNumber(&LevelRangeN[TC_LO_G], "TC_LO_G", "Low Green", "%.f", 0, 255, 10, 0);
    IUFillNumber(&LevelRangeN[TC_HI_G], "TC_HI_G", "High Green", "%.f", 0, 255, 10, 0);
    IUFillNumber(&LevelRangeN[TC_LO_B], "TC_LO_B", "Low Blue", "%.f", 0, 255, 10, 0);
    IUFillNumber(&LevelRangeN[TC_HI_B], "TC_HI_B", "High Blue", "%.f", 0, 255, 10, 0);
    IUFillNumber(&LevelRangeN[TC_LO_Y], "TC_LO_Y", "Low Gray", "%.f", 0, 255, 10, 0);
    IUFillNumber(&LevelRangeN[TC_HI_Y], "TC_HI_Y", "High Gray", "%.f", 0, 255, 10, 0);
    IUFillNumberVector(&LevelRangeNP, LevelRangeN, 8, getDeviceName(), "CCD_LEVEL_RANGE", "Level Range", LEVEL_TAB, IP_RW, 60, IPS_IDLE);

    ///////////////////////////////////////////////////////////////////////////////////
    // Auto Controls
    ///////////////////////////////////////////////////////////////////////////////////
    IUFillSwitch(&AutoControlS[TC_AUTO_EXPOSURE], "TC_AUTO_EXPOSURE", "Exposure", ISS_OFF);
    IUFillSwitch(&AutoControlS[TC_AUTO_TINT], "TC_AUTO_TINT", "White Balance Tint", ISS_OFF);
    IUFillSwitch(&AutoControlS[TC_AUTO_WB], "TC_AUTO_WB", "White Balance RGB", ISS_OFF);
    IUFillSwitch(&AutoControlS[TC_AUTO_BB], "TC_AUTO_BB", "Black Balance", ISS_OFF);
    IUFillSwitchVector(&AutoControlSP, AutoControlS, 4, getDeviceName(), "CCD_AUTO_CONTROL", "Auto", CONTROL_TAB, IP_RW,
                       ISR_ATMOST1, 0, IPS_IDLE);


    ///////////////////////////////////////////////////////////////////////////////////
    // White Balance - Temp/Tint
    ///////////////////////////////////////////////////////////////////////////////////
    IUFillNumber(&WBTempTintN[TC_WB_TEMP], "TC_WB_TEMP", "Temp", "%.f", 2000, 15000, 1000, 6503);
    IUFillNumber(&WBTempTintN[TC_WB_TINT], "TC_WB_TINT", "Tint", "%.f", 200, 2500, 100, 1000);
    IUFillNumberVector(&WBTempTintNP, WBTempTintN, 2, getDeviceName(), "TC_WB_TT", "WB #1", LEVEL_TAB, IP_RW, 60, IPS_IDLE);

    ///////////////////////////////////////////////////////////////////////////////////
    // White Balance - RGB
    ///////////////////////////////////////////////////////////////////////////////////
    IUFillNumber(&WBRGBN[TC_WB_R], "TC_WB_R", "Red", "%.f", -127, 127, 10, 0);
    IUFillNumber(&WBRGBN[TC_WB_G], "TC_WB_G", "Green", "%.f", -127, 127, 10, 0);
    IUFillNumber(&WBRGBN[TC_WB_B], "TC_WB_B", "Blue", "%.f", -127, 127, 10, 0);
    IUFillNumberVector(&WBRGBNP, WBRGBN, 3, getDeviceName(), "TC_WB_RGB", "WB #2", LEVEL_TAB, IP_RW, 60, IPS_IDLE);


    ///////////////////////////////////////////////////////////////////////////////////
    /// White Balance - Auto
    ///////////////////////////////////////////////////////////////////////////////////
    IUFillSwitch(&WBAutoS[TC_AUTO_WB_TT], "TC_AUTO_WB_TT", "Temp/Tint", ISS_ON);
    IUFillSwitch(&WBAutoS[TC_AUTO_WB_RGB], "TC_AUTO_WB_RGB", "RGB", ISS_OFF);
    IUFillSwitchVector(&WBAutoSP, WBAutoS, 2, getDeviceName(), "TC_AUTO_WB", "Default WB Mode", MAIN_CONTROL_TAB, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);

    ///////////////////////////////////////////////////////////////////////////////////
    /// Fan Control
    ///////////////////////////////////////////////////////////////////////////////////
    IUFillSwitch(&FanControlS[TC_FAN_ON], "TC_FAN_ON", "On", ISS_ON);
    IUFillSwitch(&FanControlS[TC_FAN_OFF], "TC_FAN_OFF", "Off", ISS_OFF);
    IUFillSwitchVector(&FanControlSP, FanControlS, 2, getDeviceName(), "TC_FAN_CONTROL", "Fan", MAIN_CONTROL_TAB, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);

    ///////////////////////////////////////////////////////////////////////////////////
    /// Video Format
    ///////////////////////////////////////////////////////////////////////////////////
    /// RGB Mode but 8 bits grayscale
    //IUFillSwitch(&VideoFormatS[TC_VIDEO_MONO_8], "TC_VIDEO_MONO_8", "Mono 8", ISS_OFF);
    /// RGB Mode but 16 bits grayscale
    //IUFillSwitch(&VideoFormatS[TC_VIDEO_MONO_16], "TC_VIDEO_MONO_16", "Mono 16", ISS_OFF);
    /// RGB Mode with RGB24 color
    IUFillSwitch(&VideoFormatS[TC_VIDEO_COLOR_RGB], "TC_VIDEO_COLOR_RGB", "RGB", ISS_OFF);
    /// Raw mode (8 to 16 bit)
    IUFillSwitch(&VideoFormatS[TC_VIDEO_COLOR_RAW], "TC_VIDEO_COLOR_RAW", "Raw", ISS_OFF);
    IUFillSwitchVector(&VideoFormatSP, VideoFormatS, 2, getDeviceName(), "CCD_VIDEO_FORMAT", "Format", CONTROL_TAB, IP_RW,
                       ISR_1OFMANY, 60, IPS_IDLE);

    ///////////////////////////////////////////////////////////////////////////////////
    /// Resolution
    ///////////////////////////////////////////////////////////////////////////////////
    IUFillSwitchVector(&ResolutionSP, ResolutionS, 0, getDeviceName(), "CCD_RESOLUTION", "Resolution", CONTROL_TAB, IP_RW,
                       ISR_1OFMANY, 60, IPS_IDLE);

    ///////////////////////////////////////////////////////////////////////////////////
    /// Firmware
    ///////////////////////////////////////////////////////////////////////////////////
    IUFillText(&FirmwareT[TC_FIRMWARE_SERIAL], "Serial", "Serial", nullptr);
    IUFillText(&FirmwareT[TC_FIRMWARE_SW_VERSION], "Software", "Software", nullptr);
    IUFillText(&FirmwareT[TC_FIRMWARE_HW_VERSION], "Hardware", "Hardware", nullptr);
    IUFillText(&FirmwareT[TC_FIRMWARE_DATE], "Date", "Date", nullptr);
    IUFillText(&FirmwareT[TC_FIRMWARE_REV], "Revision", "Revision", nullptr);
    IUFillTextVector(&FirmwareTP, FirmwareT, 5, getDeviceName(), "Firmware", "Firmware", "Firmware", IP_RO, 0, IPS_IDLE);

    PrimaryCCD.setMinMaxStep("CCD_BINNING", "HOR_BIN", 1, 4, 1, false);
    PrimaryCCD.setMinMaxStep("CCD_BINNING", "VER_BIN", 1, 4, 1, false);

    addAuxControls();

    return true;
}

bool TOUPCAM::updateProperties()
{
    INDI::CCD::updateProperties();

    if (isConnected())
    {
        // Let's get parameters now from CCD
        setupParams();

        if (HasCooler())
        {
            defineSwitch(&CoolerSP);
            loadConfig(true, "CCD_COOLER");
        }
        // Even if there is no cooler, we define temperature property as READ ONLY
        else if (m_Instance->model->flag & TOUPCAM_FLAG_GETTEMPERATURE)
        {
            TemperatureNP.p = IP_RO;
            defineNumber(&TemperatureNP);
        }

        if (m_Instance->model->flag & TOUPCAM_FLAG_FAN)
            defineSwitch(&FanControlSP);

        if (m_MonoCamera == false)
            defineSwitch(&WBAutoSP);

        defineNumber(&ControlNP);
        defineSwitch(&AutoControlSP);
        defineSwitch(&VideoFormatSP);
        defineSwitch(&ResolutionSP);

        // Levels
        defineNumber(&LevelRangeNP);
        defineNumber(&BlackBalanceNP);

        // Balance
        if (m_MonoCamera == false)
        {
            defineNumber(&WBTempTintNP);
            defineNumber(&WBRGBNP);
        }

        // Firmware
        defineText(&FirmwareTP);
    }
    else
    {
        if (HasCooler())
            deleteProperty(CoolerSP.name);
        else
            deleteProperty(TemperatureNP.name);

        if (m_Instance->model->flag & TOUPCAM_FLAG_FAN)
            deleteProperty(FanControlSP.name);

        if (m_MonoCamera == false)
            deleteProperty(WBAutoSP.name);

        deleteProperty(ControlNP.name);
        deleteProperty(AutoControlSP.name);
        deleteProperty(VideoFormatSP.name);
        deleteProperty(ResolutionSP.name);

        deleteProperty(LevelRangeNP.name);
        deleteProperty(BlackBalanceNP.name);

        if (m_MonoCamera == false)
        {
            deleteProperty(WBTempTintNP.name);
            deleteProperty(WBRGBNP.name);
        }


        deleteProperty(FirmwareTP.name);
    }

    return true;
}

bool TOUPCAM::Connect()
{
    LOGF_DEBUG("Attempting to open %s with ID %s", name, m_Instance->id);

    if (isSimulation() == false)
    {
        std::string fullID = m_Instance->id;
        // For RGB White Balance Mode, we need to add @ at the beginning as per docs.
        if (m_MonoCamera == false && WBAutoS[TC_AUTO_WB_RGB].s == ISS_ON)
            fullID = "@" + fullID;

        m_CameraHandle = Toupcam_Open(fullID.c_str());
    }

    if (m_CameraHandle == nullptr)
    {
        LOG_ERROR("Error connecting to the camera.");
        return false;
    }

    uint32_t cap = 0;

    cap |= CCD_CAN_ABORT;

    m_MonoCamera = false;
    // If raw format is support then we have bayer
    if (m_Instance->model->flag & (TOUPCAM_FLAG_MONO))
    {
        m_MonoCamera = true;
        m_RAWFormatSupport = false;
    }
    else if (m_Instance->model->flag & (TOUPCAM_FLAG_RAW8 | TOUPCAM_FLAG_RAW10 | TOUPCAM_FLAG_RAW12 | TOUPCAM_FLAG_RAW14 | TOUPCAM_FLAG_RAW16))
    {
        LOG_DEBUG("RAW format supported. Bayer enabled.");
        cap |= CCD_HAS_BAYER;
        m_RAWFormatSupport = true;
    }

    if (m_Instance->model->flag & TOUPCAM_FLAG_BINSKIP_SUPPORTED)
        LOG_DEBUG("Bin-Skip supported.");

    cap |= CCD_CAN_BIN;

    // Hardware ROI really needed? Check later
    if (m_Instance->model->flag & TOUPCAM_FLAG_ROI_HARDWARE)
    {
        LOG_DEBUG("Hardware ROI supported.");
        cap |= CCD_CAN_SUBFRAME;
    }

    if (m_Instance->model->flag & TOUPCAM_FLAG_TEC_ONOFF)
    {
        LOG_DEBUG("TEC control enabled.");
        cap |= CCD_HAS_COOLER;
    }

    if (m_Instance->model->flag & TOUPCAM_FLAG_ST4)
    {
        LOG_DEBUG("ST4 guiding enabled.");
        cap |= CCD_HAS_ST4_PORT;
    }

    cap |= CCD_HAS_STREAMING;

    SetCCDCapability(cap);

    LOGF_DEBUG("maxSpeed: %d preview: %d still: %d maxFanSpeed %d", m_Instance->model->maxspeed, m_Instance->model->preview,
               m_Instance->model->still, m_Instance->model->maxfanspeed);

    // Get min/max exposures
    uint32_t min=0,max=0,current=0;
    Toupcam_get_ExpTimeRange(m_CameraHandle, &min, &max, &current);
    LOGF_DEBUG("Exposure Time Range (us): Min %u Max %u Default %u", min, max, current);
    PrimaryCCD.setMinMaxStep("CCD_EXPOSURE", "CCD_EXPOSURE_VALUE", min/1000000.0, max/1000000.0, 0, false);

    // Success!
    LOGF_INFO("%s is online. Retrieving basic data.", getDeviceName());

    return true;
}

bool TOUPCAM::Disconnect()
{
    stopTimerNS();
    stopTimerWE();

    Toupcam_Close(m_CameraHandle);

    return true;
}

void TOUPCAM::setupParams()
{
    HRESULT rc = 0;

    Toupcam_put_Option(m_CameraHandle, TOUPCAM_OPTION_NOFRAME_TIMEOUT, 1);

    // Get Firmware Info
    char firmwareBuffer[32] = {0};
    uint16_t pRevision=0;
    Toupcam_get_SerialNumber(m_CameraHandle, firmwareBuffer);
    IUSaveText(&FirmwareT[TC_FIRMWARE_SERIAL], firmwareBuffer);
    Toupcam_get_FwVersion(m_CameraHandle, firmwareBuffer);
    IUSaveText(&FirmwareT[TC_FIRMWARE_SW_VERSION], firmwareBuffer);
    Toupcam_get_HwVersion(m_CameraHandle, firmwareBuffer);
    IUSaveText(&FirmwareT[TC_FIRMWARE_HW_VERSION], firmwareBuffer);
    Toupcam_get_ProductionDate(m_CameraHandle, firmwareBuffer);
    IUSaveText(&FirmwareT[TC_FIRMWARE_DATE], firmwareBuffer);
    Toupcam_get_Revision(m_CameraHandle, &pRevision);
    snprintf(firmwareBuffer, 32, "%d", pRevision);
    IUSaveText(&FirmwareT[TC_FIRMWARE_REV], firmwareBuffer);

    // Max supported bit depth
    m_MaxBitDepth = Toupcam_get_MaxBitDepth(m_CameraHandle);
    LOGF_DEBUG("Max bit depth: %d", m_MaxBitDepth);

    m_BitsPerPixel = 8;
    int nVal=0;

    // Check if mono only camera
    if (m_MonoCamera)
    {
        IUFillSwitch(&VideoFormatS[TC_VIDEO_MONO_8], "TC_VIDEO_MONO_8", "Mono 8", ISS_OFF);
        /// RGB Mode but 16 bits grayscale
        IUFillSwitch(&VideoFormatS[TC_VIDEO_MONO_16], "TC_VIDEO_MONO_16", "Mono 16", ISS_OFF);
        LOG_DEBUG("Mono camera detected.");

        rc = Toupcam_put_Option(m_CameraHandle, TOUPCAM_OPTION_RAW, 1);
        LOGF_DEBUG("TOUPCAM_OPTION_RAW 1. rc: %s", errorCodes[rc].c_str());
//        rc = Toupcam_put_Option(m_CameraHandle, TOUPCAM_OPTION_BITDEPTH, 1);
//        LOGF_DEBUG("TOUPCAM_OPTION_BITDEPTH 1. rc: %s", errorCodes[rc].c_str());

        int rgbMode = 0;
        rc = Toupcam_get_Option(m_CameraHandle, TOUPCAM_OPTION_RGB, &rgbMode);
        LOGF_DEBUG("TOUPCAM_OPTION_RGB. rc: %s Value: %d", errorCodes[rc].c_str(), rgbMode);

        // 8 bit
        if (rgbMode <= 3)
        {
            VideoFormatS[TC_VIDEO_MONO_8].s = ISS_ON;
            m_Channels = 1;
            m_CameraPixelFormat = INDI_MONO;
            m_BitsPerPixel = 8;
            LOG_INFO("Video Mode 8-bit mono detected.");
        }
        // 16 bits gray
        else if (rgbMode == 4)
        {
            VideoFormatS[TC_VIDEO_MONO_16].s = ISS_ON;
            m_Channels = 1;
            m_CameraPixelFormat = INDI_MONO;
            m_BitsPerPixel = 16;
            LOG_INFO("Video Mode 16-bit mono detected.");
        }

        LOGF_DEBUG("Bits Per Pixel: %d Video Mode: %s", m_BitsPerPixel, VideoFormatS[TC_VIDEO_MONO_8].s == ISS_ON ? "Mono 8-bit" : "Mono 16-bit");
    }
    // Color Camera
    else
    {
        if (m_Instance->model->flag & (TOUPCAM_FLAG_RAW10 | TOUPCAM_FLAG_RAW12 | TOUPCAM_FLAG_RAW14 | TOUPCAM_FLAG_RAW16))
        {
            // enable bitdepth
            Toupcam_put_Option(m_CameraHandle, TOUPCAM_OPTION_BITDEPTH, 1);
            m_BitsPerPixel = 16;
            m_RAWHighDepthSupport = true;
            LOG_DEBUG("RAW Bit Depth: 16");
        }

        // Get RAW/RGB Mode
        int cameraDataMode=0;
        IUResetSwitch(&VideoFormatSP);
        rc = Toupcam_get_Option(m_CameraHandle, TOUPCAM_OPTION_RAW, &cameraDataMode);
        LOGF_DEBUG("TOUPCAM_OPTION_RAW. rc: %s Value: %d", errorCodes[rc].c_str(), cameraDataMode);

        // Color RAW
        if (cameraDataMode == TC_VIDEO_COLOR_RAW)
        {
            VideoFormatS[TC_VIDEO_COLOR_RAW].s = ISS_ON;
            m_Channels = 1;
            LOG_INFO("Video Mode RAW detected.");

            // Get RAW Format
            IUSaveText(&BayerT[2], getBayerString());
        }
        // Color RGB
        else
        {
            int rgbMode = 0;
            rc = Toupcam_get_Option(m_CameraHandle, TOUPCAM_OPTION_RGB, &rgbMode);
            LOGF_DEBUG("TOUPCAM_OPTION_RGB. rc: %s Value: %d", errorCodes[rc].c_str(), rgbMode);

            // 0 = RGB24, 1 = RGB48, 2 = RGB32
            // We only support RGB24 in the driver
            if (rgbMode != 0)
            {
                LOGF_DEBUG("RGB Mode %s is not supported. Setting mode to RGB24", rgbMode == 1 ? "RGB48" : "RGB32");
                Toupcam_put_Option(m_CameraHandle, TOUPCAM_OPTION_RGB, 0);
            }

            LOG_INFO("Video Mode RGB detected.");
            VideoFormatS[TC_VIDEO_COLOR_RGB].s = ISS_ON;
            m_Channels = 3;
            m_CameraPixelFormat = INDI_RGB;
            m_BitsPerPixel = 8;

            // Disable Bayer until we switch to raw mode
            if (m_RAWFormatSupport)
                SetCCDCapability(GetCCDCapability() & ~CCD_HAS_BAYER);
        }

        LOGF_DEBUG("Bits Per Pixel: %d Video Mode: %s", m_BitsPerPixel, VideoFormatS[TC_VIDEO_COLOR_RGB].s == ISS_ON ? "RGB" : "RAW");
    }

    PrimaryCCD.setNAxis(m_Channels == 1 ? 2 : 3);

    // Get how many resolutions available for the camera
    ResolutionSP.nsp = Toupcam_get_ResolutionNumber(m_CameraHandle);

    int w[TOUPCAM_MAX]={0},h[TOUPCAM_MAX]={0};
    // Get each resolution width x height
    for (uint8_t i=0; i < ResolutionSP.nsp; i++)
    {
        rc = Toupcam_get_Resolution(m_CameraHandle, i, &w[i], &h[i]);
        char label[MAXINDILABEL] = {0};
        snprintf(label, MAXINDILABEL, "%d x %d", w[i], h[i]);
        LOGF_DEBUG("Resolution #%d: %s", i+1, label);
        IUFillSwitch(&ResolutionS[i], label, label, ISS_OFF);
    }

    // Fan Control
    if (m_Instance->model->flag & TOUPCAM_FLAG_FAN)
    {
        int fan = 0;
        Toupcam_get_Option(m_CameraHandle, TOUPCAM_OPTION_FAN, &fan);
        LOGF_DEBUG("Fan is %s", fan == 0 ? "Off" : "On");
        IUResetSwitch(&FanControlSP);
        FanControlS[TC_FAN_ON].s = fan == 0 ? ISS_OFF : ISS_ON;
        FanControlS[TC_FAN_OFF].s = fan == 0 ? ISS_ON : ISS_OFF;
        FanControlSP.s = (fan == 0) ? IPS_IDLE : IPS_BUSY;
    }

    // Get active resolution index
    uint32_t currentResolutionIndex = 0;
    rc = Toupcam_get_eSize(m_CameraHandle, &currentResolutionIndex);
    ResolutionS[currentResolutionIndex].s = ISS_ON;

    SetCCDParams(w[currentResolutionIndex], h[currentResolutionIndex], m_BitsPerPixel, m_Instance->model->xpixsz, m_Instance->model->ypixsz);

    m_CanSnap = m_Instance->model->still > 0;
    LOGF_DEBUG("Camera snap support: %s", m_CanSnap ? "True" : "False");

    // Trigger Mode
    rc = Toupcam_get_Option(m_CameraHandle, TOUPCAM_OPTION_TRIGGER, &nVal);
    LOGF_DEBUG("Trigger mode: %d", nVal);
    m_CurrentTriggerMode = static_cast<eTriggerMode>(nVal);

    // Set trigger mode to software
    if (m_CurrentTriggerMode != TRIGGER_SOFTWARE)
    {
        LOG_DEBUG("Setting trigger mode to software...");
        if ( (rc = Toupcam_put_Option(m_CameraHandle, TOUPCAM_OPTION_TRIGGER, 1)) < 0)
        {
            LOGF_ERROR("Failed to set software trigger mode. %s", errorCodes[rc].c_str());
        }
        else
            m_CurrentTriggerMode = TRIGGER_SOFTWARE;
    }

    // Get CCD Controls values
    uint16_t nMin=0, nMax=0, nDef=0;

    // Gain
    rc = Toupcam_get_ExpoAGainRange(m_CameraHandle, &nMin, &nMax, &nDef);
    LOGF_DEBUG("Exposure Auto Gain Control. Min: %u Max: %u Default: %u", nMin, nMax, nDef);
    ControlN[TC_GAIN].min = nMin;
    ControlN[TC_GAIN].max = nMax;
    ControlN[TC_GAIN].step = (nMax-nMin)/20.0;
    ControlN[TC_GAIN].value = nDef;

    // Contrast
    Toupcam_get_Contrast(m_CameraHandle, &nVal);
    LOGF_DEBUG("Contrast Control. Min: %u Max: %u Default: %u", nMin, nMax, nDef);
    ControlN[TC_CONTRAST].value = nVal;

    // Hue
    rc = Toupcam_get_Hue(m_CameraHandle, &nVal);
    LOGF_DEBUG("Hue Control: %d", nVal);
    ControlN[TC_HUE].value = nVal;

    // Saturation
    rc = Toupcam_get_Saturation(m_CameraHandle, &nVal);
    LOGF_DEBUG("Saturation Control: %d", nVal);
    ControlN[TC_SATURATION].value = nVal;

    // Brightness
    rc = Toupcam_get_Brightness(m_CameraHandle, &nVal);
    LOGF_DEBUG("Brightness Control: %d", nVal);
    ControlN[TC_BRIGHTNESS].value = nVal;

    // Gamma
    rc = Toupcam_get_Gamma(m_CameraHandle, &nVal);
    LOGF_DEBUG("Gamma Control: %d", nVal);
    ControlN[TC_GAMMA].value = nVal;

    // Speed
    rc = Toupcam_get_Speed(m_CameraHandle, &nDef);
    LOGF_DEBUG("Speed Control: %d", nDef);
    ControlN[TC_SPEED].value = nDef;
    ControlN[TC_SPEED].max = m_Instance->model->maxspeed;

    // Set Bin more for better quality over skip
    if (m_Instance->model->flag & TOUPCAM_FLAG_BINSKIP_SUPPORTED)
    {
        LOG_DEBUG("Selecting BIN mode over SKIP...");
        rc = Toupcam_put_Mode(m_CameraHandle, 0);
    }

    // Get White Balance RGB Gain
    int aGain[3] = {0};
    rc = Toupcam_get_WhiteBalanceGain(m_CameraHandle, aGain);
    if (rc >= 0)
    {
        WBRGBN[TC_WB_R].value = aGain[TC_WB_R];
        WBRGBN[TC_WB_G].value = aGain[TC_WB_G];
        WBRGBN[TC_WB_B].value = aGain[TC_WB_B];
        LOGF_DEBUG("White Balance Gain. R: %d G: %d B: %d", aGain[TC_WB_R], aGain[TC_WB_G], aGain[TC_WB_B]);
    }

    // Get Level Ranges
    uint16_t aLow[4]={0}, aHigh[4]={0};
    rc = Toupcam_get_LevelRange(m_CameraHandle, aLow, aHigh);
    if (rc >= 0)
    {
        LevelRangeN[TC_LO_R].value = aLow[0];
        LevelRangeN[TC_LO_G].value = aLow[1];
        LevelRangeN[TC_LO_B].value = aLow[2];
        LevelRangeN[TC_LO_Y].value = aLow[3];

        LevelRangeN[TC_HI_R].value = aHigh[0];
        LevelRangeN[TC_HI_G].value = aHigh[1];
        LevelRangeN[TC_HI_B].value = aHigh[2];
        LevelRangeN[TC_HI_Y].value = aHigh[3];
    }

    // Get Black Balance
    uint16_t aSub[3]={0};
    rc = Toupcam_get_BlackBalance(m_CameraHandle, aSub);
    if (rc >= 0)
    {
        BlackBalanceN[TC_BLACK_R].value = aSub[0];
        BlackBalanceN[TC_BLACK_G].value = aSub[1];
        BlackBalanceN[TC_BLACK_B].value = aSub[2];
    }

    // Allocate memory
    allocateFrameBuffer();

    SetTimer(POLLMS);

    // Start callback
    if ( (rc = Toupcam_StartPullModeWithCallback(m_CameraHandle, &TOUPCAM::eventCB, this)) < 0)
    {
        LOGF_ERROR("Failed to start camera pull mode. %s", errorCodes[rc].c_str());
        Disconnect();
        updateProperties();
        return;
    }

    LOG_DEBUG("Starting event callback in pull mode.");
}

void TOUPCAM::allocateFrameBuffer()
{
    LOG_DEBUG("Allocating Frame Buffer...");

    // Allocate memory
    if (m_MonoCamera)
    {
        switch (m_CurrentVideoFormat)
        {
        case TC_VIDEO_MONO_8:
            PrimaryCCD.setFrameBufferSize(PrimaryCCD.getXRes() * PrimaryCCD.getYRes());
            PrimaryCCD.setBPP(8);
            PrimaryCCD.setNAxis(2);
            Streamer->setPixelFormat(INDI_MONO, 8);
            break;

        case TC_VIDEO_MONO_16:
            PrimaryCCD.setFrameBufferSize(PrimaryCCD.getXRes() * PrimaryCCD.getYRes() * 2);
            PrimaryCCD.setBPP(16);
            PrimaryCCD.setNAxis(2);
            Streamer->setPixelFormat(INDI_MONO, 16);
            break;
        }
    }
    else
    {
        switch (m_CurrentVideoFormat)
        {
        case TC_VIDEO_COLOR_RGB:
            // RGB24 or RGB888
            PrimaryCCD.setFrameBufferSize(PrimaryCCD.getXRes() * PrimaryCCD.getYRes() * 3);
            PrimaryCCD.setBPP(8);
            PrimaryCCD.setNAxis(3);
            Streamer->setPixelFormat(INDI_RGB, 8);
            break;

        case TC_VIDEO_COLOR_RAW:
            PrimaryCCD.setFrameBufferSize(PrimaryCCD.getXRes() * PrimaryCCD.getYRes() * m_BitsPerPixel/8);
            PrimaryCCD.setBPP(m_BitsPerPixel);
            PrimaryCCD.setNAxis(2);
            Streamer->setPixelFormat(m_CameraPixelFormat, m_BitsPerPixel);
            break;

        }
    }

    Streamer->setSize(PrimaryCCD.getXRes(), PrimaryCCD.getYRes());
}

bool TOUPCAM::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    if (dev != nullptr && !strcmp(dev, getDeviceName()))
    {
        //////////////////////////////////////////////////////////////////////
        /// Controls (Contrast, Brightness, Hue...etc)
        //////////////////////////////////////////////////////////////////////
        if (!strcmp(name, ControlNP.name))
        {
            double oldValues[7]={0};
            for (int i = 0; i < ControlNP.nnp; i++)
                oldValues[i] = ControlN[i].value;

            if (IUUpdateNumber(&ControlNP, values, names, n) < 0)
            {
                ControlNP.s = IPS_ALERT;
                IDSetNumber(&ControlNP, nullptr);
                return true;
            }

            for (uint8_t i = 0; i < ControlNP.nnp; i++)
            {
                if (fabs(ControlN[i].value - oldValues[i]) < 0.0001)
                    continue;

                int value = static_cast<int>(ControlN[i].value);
                switch (i)
                {
                case TC_GAIN:
                    Toupcam_put_ExpoAGain(m_CameraHandle, value);
                    break;

                case TC_CONTRAST:
                    Toupcam_put_Contrast(m_CameraHandle, value);
                    break;

                case TC_HUE:
                    Toupcam_put_Hue(m_CameraHandle, value);
                    break;

                case TC_SATURATION:
                    Toupcam_put_Saturation(m_CameraHandle, value);
                    break;

                case TC_BRIGHTNESS:
                    Toupcam_put_Brightness(m_CameraHandle, value);
                    break;

                case TC_GAMMA:
                    Toupcam_put_Gamma(m_CameraHandle, value);
                    break;

                case TC_SPEED:
                    Toupcam_put_Speed(m_CameraHandle, value);
                    break;
                default:
                    break;
                }
            }

            ControlNP.s = IPS_OK;
            IDSetNumber(&ControlNP, nullptr);
            return true;
        }

        //////////////////////////////////////////////////////////////////////
        /// Level Ranges
        //////////////////////////////////////////////////////////////////////
        if (!strcmp(name, LevelRangeNP.name))
        {
            IUUpdateNumber(&LevelRangeNP, values, names, n);
            uint16_t lo[4] =
            {
                static_cast<uint16_t>(LevelRangeN[TC_LO_R].value),
                static_cast<uint16_t>(LevelRangeN[TC_LO_G].value),
                static_cast<uint16_t>(LevelRangeN[TC_LO_B].value),
                static_cast<uint16_t>(LevelRangeN[TC_LO_Y].value),
            };

            uint16_t hi[4] =
            {
                static_cast<uint16_t>(LevelRangeN[TC_HI_R].value),
                static_cast<uint16_t>(LevelRangeN[TC_HI_G].value),
                static_cast<uint16_t>(LevelRangeN[TC_HI_B].value),
                static_cast<uint16_t>(LevelRangeN[TC_HI_Y].value),
            };

            HRESULT rc = 0;

            if ( (rc = Toupcam_put_LevelRange(m_CameraHandle, lo, hi)) < 0)
            {
                LevelRangeNP.s = IPS_ALERT;
                LOGF_ERROR("Failed to set level range. %s", errorCodes[rc].c_str());

            }
            else
                LevelRangeNP.s = IPS_OK;

            IDSetNumber(&LevelRangeNP, nullptr);
            return true;

        }

        //////////////////////////////////////////////////////////////////////
        /// Black Balance
        //////////////////////////////////////////////////////////////////////
        if (!strcmp(name, BlackBalanceNP.name))
        {
            IUUpdateNumber(&BlackBalanceNP, values, names, n);
            uint16_t aSub[3] =
            {
                static_cast<uint16_t>(BlackBalanceN[TC_BLACK_R].value),
                static_cast<uint16_t>(BlackBalanceN[TC_BLACK_G].value),
                static_cast<uint16_t>(BlackBalanceN[TC_BLACK_B].value),
            };

            HRESULT rc = 0;

            if ( (rc = Toupcam_put_BlackBalance(m_CameraHandle, aSub)) < 0)
            {
                BlackBalanceNP.s = IPS_ALERT;
                LOGF_ERROR("Failed to set Black Balance. %s", errorCodes[rc].c_str());

            }
            else
                BlackBalanceNP.s = IPS_OK;

            IDSetNumber(&BlackBalanceNP, nullptr);
            return true;
        }

        //////////////////////////////////////////////////////////////////////
        /// Temp/Tint White Balance
        //////////////////////////////////////////////////////////////////////
        if (!strcmp(name, WBTempTintNP.name))
        {
            IUUpdateNumber(&WBTempTintNP, values, names, n);
            HRESULT rc = 0;

            if ( (rc = Toupcam_put_TempTint(m_CameraHandle, static_cast<int>(WBTempTintN[TC_WB_TEMP].value),
                                              static_cast<int>(WBTempTintN[TC_WB_TINT].value))) < 0)
            {
                WBTempTintNP.s = IPS_ALERT;
                LOGF_ERROR("Failed to set White Balance Tempeture & Tint. %s", errorCodes[rc].c_str());

            }
            else
                WBTempTintNP.s = IPS_OK;

            IDSetNumber(&WBTempTintNP, nullptr);
            return true;
        }

        //////////////////////////////////////////////////////////////////////
        /// RGB White Balance
        //////////////////////////////////////////////////////////////////////
        if (!strcmp(name, WBRGBNP.name))
        {
            IUUpdateNumber(&WBRGBNP, values, names, n);
            HRESULT rc = 0;

            int aSub[3] =
            {
                static_cast<int>(WBRGBN[TC_WB_R].value),
                static_cast<int>(WBRGBN[TC_WB_G].value),
                static_cast<int>(WBRGBN[TC_WB_B].value),
            };

            if ( (rc = Toupcam_put_WhiteBalanceGain(m_CameraHandle, aSub)) < 0)
            {
                WBRGBNP.s = IPS_ALERT;
                LOGF_ERROR("Failed to set White Balance gain. %s", errorCodes[rc].c_str());

            }
            else
                WBRGBNP.s = IPS_OK;

            IDSetNumber(&WBRGBNP, nullptr);
            return true;
        }

    }

    return INDI::CCD::ISNewNumber(dev, name, values, names, n);
}

bool TOUPCAM::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if (dev != nullptr && !strcmp(dev, getDeviceName()))
    {
        //////////////////////////////////////////////////////////////////////
        /// Cooler Control
        //////////////////////////////////////////////////////////////////////
        if (!strcmp(name, CoolerSP.name))
        {
            if (IUUpdateSwitch(&CoolerSP, states, names, n) < 0)
            {
                CoolerSP.s = IPS_ALERT;
                IDSetSwitch(&CoolerSP, nullptr);
                return true;
            }

            if (CoolerS[TC_COOLER_ON].s == ISS_ON)
                activateCooler(true);
            else
                activateCooler(false);

            return true;
        }

        //////////////////////////////////////////////////////////////////////
        /// Fan Control
        //////////////////////////////////////////////////////////////////////
        if (!strcmp(name, FanControlSP.name))
        {
            int prevIndex = IUFindOnSwitchIndex(&FanControlSP);
            IUUpdateSwitch(&FanControlSP, states, names, n);
            HRESULT rc = Toupcam_put_Option(m_CameraHandle, TOUPCAM_OPTION_FAN, FanControlS[0].s == ISS_ON ? 1 : 0 );
            if (rc < 0)
            {
                LOGF_ERROR("Failed to turn the fan %s. Error (%s)", FanControlS[0].s == ISS_ON ? "on" : "off", errorCodes[rc].c_str());
                FanControlSP.s = IPS_ALERT;
                IUResetSwitch(&FanControlSP);
                FanControlS[prevIndex].s = ISS_ON;
            }
            else
            {
                FanControlSP.s = (FanControlS[0].s == ISS_ON) ? IPS_BUSY : IPS_IDLE;
            }

            IDSetSwitch(&FanControlSP, nullptr);
            return true;
        }

        //////////////////////////////////////////////////////////////////////
        /// Video Format
        //////////////////////////////////////////////////////////////////////
        if (!strcmp(name, VideoFormatSP.name))
        {
            int rc = 0;

            if (Streamer->isBusy())
            {
                VideoFormatSP.s = IPS_ALERT;
                LOG_ERROR("Cannot change format while streaming/recording.");
                IDSetSwitch(&VideoFormatSP, nullptr);
                return true;
            }

            int prevIndex = IUFindOnSwitchIndex(&VideoFormatSP);
            IUUpdateSwitch(&VideoFormatSP, states, names, n);
            int currentIndex = IUFindOnSwitchIndex(&VideoFormatSP);

            m_Channels = 1;
            m_BitsPerPixel = 8;

            // Mono
            if (m_MonoCamera)
            {
                if (m_MaxBitDepth == 8 && currentIndex == TC_VIDEO_MONO_16)
                {
                    VideoFormatSP.s = IPS_ALERT;
                    LOG_ERROR("Only 8-bit format is supported.");
                    IUResetSwitch(&VideoFormatSP);
                    VideoFormatS[prevIndex].s = ISS_ON;
                    IDSetSwitch(&VideoFormatSP, nullptr);
                    return true;
                }

                // We need to stop camera first
                LOG_DEBUG("Stopping camera to change video mode.");
                Toupcam_Stop(m_CameraHandle);

//                int rc = Toupcam_put_Option(m_CameraHandle, TOUPCAM_OPTION_RGB, currentIndex+3);
//                if (rc < 0)
//                {
//                    LOGF_ERROR("Failed to set RGB mode %d: %s", currentIndex+3, errorCodes[rc].c_str());
//                    VideoFormatSP.s = IPS_ALERT;
//                    IUResetSwitch(&VideoFormatSP);
//                    VideoFormatS[prevIndex].s = ISS_ON;
//                    IDSetSwitch(&VideoFormatSP, nullptr);

//                    // Restart Capture
//                    Toupcam_StartPullModeWithCallback(m_CameraHandle, &TOUPCAM::eventCB, this);
//                    LOG_DEBUG("Restarting event callback after video mode change failed.");

//                    return true;
//                }
//                else
//                    LOGF_DEBUG("Set TOUPCAM_OPTION_RGB --> %d", currentIndex+3);

                rc = Toupcam_put_Option(m_CameraHandle, TOUPCAM_OPTION_BITDEPTH, currentIndex);
                if (rc != 0)
                {
                    LOGF_ERROR("Failed to set high bit depth mode %s", errorCodes[rc].c_str());
                    VideoFormatSP.s = IPS_ALERT;
                    IUResetSwitch(&VideoFormatSP);
                    VideoFormatS[prevIndex].s = ISS_ON;
                    IDSetSwitch(&VideoFormatSP, nullptr);

                    // Restart Capture
                    Toupcam_StartPullModeWithCallback(m_CameraHandle, &TOUPCAM::eventCB, this);
                    LOG_DEBUG("Restarting event callback after video mode change failed.");

                    return true;
                }
                else
                    LOGF_DEBUG("Set TOUPCAM_OPTION_BITDEPTH --> %d", currentIndex);

                m_BitsPerPixel = (currentIndex == TC_VIDEO_MONO_8) ? 8 : 16;
            }
            // Color
            else
            {
                // Check if raw format is supported.
                if (currentIndex == TC_VIDEO_COLOR_RAW && m_RAWFormatSupport == false)
                {
                    VideoFormatSP.s = IPS_ALERT;
                    IUResetSwitch(&VideoFormatSP);
                    VideoFormatS[prevIndex].s = ISS_ON;
                    LOG_ERROR("RAW format is not supported.");
                    IDSetSwitch(&VideoFormatSP, nullptr);
                    return true;
                }

                // We need to stop camera first
                LOG_DEBUG("Stopping camera to change video mode.");
                Toupcam_Stop(m_CameraHandle);

                rc = Toupcam_put_Option(m_CameraHandle, TOUPCAM_OPTION_RAW, currentIndex);
                if (rc < 0)
                {
                    LOGF_ERROR("Failed to set video mode: %s", errorCodes[rc].c_str());
                    VideoFormatSP.s = IPS_ALERT;
                    IUResetSwitch(&VideoFormatSP);
                    VideoFormatS[prevIndex].s = ISS_ON;
                    IDSetSwitch(&VideoFormatSP, nullptr);

                    // Restart Capture
                    Toupcam_StartPullModeWithCallback(m_CameraHandle, &TOUPCAM::eventCB, this);
                    LOG_DEBUG("Restarting event callback after changing video mode failed.");

                    return true;
                }
                else
                    LOGF_DEBUG("Set TOUPCAM_OPTION_RAW --> %d", currentIndex);

                if (currentIndex == TC_VIDEO_COLOR_RGB)
                {
                    m_Channels = 3;
                    m_BitsPerPixel = 8;
                    // Disable Bayer if supported.
                    if (m_RAWFormatSupport)
                        SetCCDCapability(GetCCDCapability() & ~CCD_HAS_BAYER);
                }
                else
                {
                    SetCCDCapability(GetCCDCapability() | CCD_HAS_BAYER);
                    IUSaveText(&BayerT[2], getBayerString());
                    IDSetText(&BayerTP, nullptr);
                    m_BitsPerPixel = m_RawBitsPerPixel;
                }

                //                if (currentIndex == TC_VIDEO_COLOR_RGB)
                //                {
                //                    int rc = Toupcam_put_Option(m_CameraHandle, TOUPCAM_OPTION_RGB, 0);
                //                    if (rc < 0)
                //                    {
                //                        LOGF_ERROR("Failed to set RGB mode %d: %s", currentIndex+3, errorCodes[rc].c_str());
                //                        VideoFormatSP.s = IPS_ALERT;
                //                        IUResetSwitch(&VideoFormatSP);
                //                        VideoFormatS[prevIndex].s = ISS_ON;
                //                        IDSetSwitch(&VideoFormatSP, nullptr);

                //                        // Restart Capture
                //                        Toupcam_StartPullModeWithCallback(m_CameraHandle, &TOUPCAM::eventCB, this);
                //                        LOG_DEBUG("Restarting event callback after video mode change failed.");

                //                        return true;
                //                    }
                //                }

            }

            m_CurrentVideoFormat = currentIndex;
            m_BitsPerPixel = (m_BitsPerPixel > 8) ? 16 : 8;

            LOGF_DEBUG("Video Format: %d m_BitsPerPixel: %d", currentIndex, m_BitsPerPixel);

            // Allocate memory
            allocateFrameBuffer();

            VideoFormatSP.s = IPS_OK;
            IDSetSwitch(&VideoFormatSP, nullptr);

            // Restart Capture
            Toupcam_StartPullModeWithCallback(m_CameraHandle, &TOUPCAM::eventCB, this);
            LOG_DEBUG("Restarting event callback after video mode change.");

            return true;
        }

        //////////////////////////////////////////////////////////////////////
        /// Auto Controls
        //////////////////////////////////////////////////////////////////////
        if (!strcmp(name, AutoControlSP.name))
        {
            int previousSwitch = IUFindOnSwitchIndex(&AutoControlSP);

            if (IUUpdateSwitch(&AutoControlSP, states, names, n) < 0)
            {
                AutoControlSP.s = IPS_ALERT;
                IDSetSwitch(&AutoControlSP, nullptr);
                return true;
            }

            HRESULT rc = 0;
            std::string autoOperation;
            switch (IUFindOnSwitchIndex(&AutoControlSP))
            {
            case TC_AUTO_EXPOSURE:
                rc = Toupcam_put_AutoExpoEnable(m_CameraHandle, (AutoControlS[TC_AUTO_EXPOSURE].s == ISS_ON) ? TRUE : FALSE);
                autoOperation = "Auto Exposure";
                break;
            case TC_AUTO_TINT:
                rc = Toupcam_AwbOnePush(m_CameraHandle, &TOUPCAM::TempTintCB, this);
                autoOperation = "Auto White Balance Tint/Temp";
                break;
            case TC_AUTO_WB:
                rc = Toupcam_AwbInit(m_CameraHandle, &TOUPCAM::WhiteBalanceCB, this);
                autoOperation = "Auto White Balance RGB";
                break;
            case TC_AUTO_BB:
                rc = Toupcam_AbbOnePush(m_CameraHandle, &TOUPCAM::BlackBalanceCB, this);
                autoOperation = "Auto Black Balance";
                break;
            default:
                rc = -1;
            }

            IUResetSwitch(&AutoControlSP);

            if (rc < 0)
            {
                AutoControlS[previousSwitch].s = ISS_ON;
                AutoControlSP.s = IPS_ALERT;
                LOGF_ERROR("%s failed (%d).", autoOperation.c_str(), rc);
            }
            else
            {
                AutoControlSP.s = IPS_OK;
                LOGF_INFO("%s complete.", autoOperation.c_str());
            }

            IDSetSwitch(&AutoControlSP, nullptr);
            return true;

        }

        //////////////////////////////////////////////////////////////////////
        /// Resolution
        //////////////////////////////////////////////////////////////////////
        if (!strcmp(name, ResolutionSP.name))
        {
            if (Streamer->isBusy())
            {
                ResolutionSP.s = IPS_ALERT;
                LOG_ERROR("Cannot change resolution while streaming/recording.");
                IDSetSwitch(&ResolutionSP, nullptr);
                return true;
            }

            int preIndex = IUFindOnSwitchIndex(&ResolutionSP);
            IUUpdateSwitch(&ResolutionSP, states, names, n);

            // Stop capture
            LOG_DEBUG("Stopping camera to change resolution.");
            Toupcam_Stop(m_CameraHandle);

            int targetIndex = IUFindOnSwitchIndex(&ResolutionSP);

            HRESULT rc = Toupcam_put_eSize(m_CameraHandle, targetIndex);
            if (rc < 0)
            {
                ResolutionSP.s = IPS_ALERT;
                IUResetSwitch(&ResolutionSP);
                ResolutionS[preIndex].s = ISS_ON;
                LOGF_ERROR("Failed to change resolution. %s", errorCodes[rc].c_str());
            }
            else
            {
                ResolutionSP.s = IPS_OK;
                PrimaryCCD.setResolution(m_Instance->model->res[targetIndex].width, m_Instance->model->res[targetIndex].height);
                LOGF_INFO("Resolution changed to %s", ResolutionS[targetIndex].label);
                allocateFrameBuffer();
            }

            IDSetSwitch(&ResolutionSP, nullptr);

            // Restart capture
            Toupcam_StartPullModeWithCallback(m_CameraHandle, &TOUPCAM::eventCB, this);
            LOG_DEBUG("Restarting event callback after changing resolution.");
            return true;
        }

        //////////////////////////////////////////////////////////////////////
        /// Auto White Balance
        //////////////////////////////////////////////////////////////////////
        if (!strcmp(name, WBAutoSP.name))
        {
            IUUpdateSwitch(&WBAutoSP, states, names, n);
            HRESULT rc = 0;
            if (IUFindOnSwitchIndex(&WBAutoSP) == TC_AUTO_WB_TT)
                rc = Toupcam_AwbOnePush(m_CameraHandle, &TOUPCAM::TempTintCB, this);
            else
                rc = Toupcam_AwbInit(m_CameraHandle, &TOUPCAM::WhiteBalanceCB, this);

            IUResetSwitch(&WBAutoSP);
            if (rc >= 0)
            {
                LOG_INFO("Executing auto white balance...");
                WBAutoSP.s = IPS_OK;
            }
            else
            {
                LOGF_ERROR("Executing auto white balance failed %s.", errorCodes[rc].c_str());
                WBAutoSP.s = IPS_ALERT;
            }

            IDSetSwitch(&WBAutoSP, nullptr);
            return true;
        }
    }

    return INDI::CCD::ISNewSwitch(dev, name, states, names, n);
}

bool TOUPCAM::StartStreaming()
{
    int rc=0;

    //    if ( (rc = Toupcam_put_RealTime(m_CameraHandle, true)) < 0)
    //    {
    //        LOGF_ERROR("Failed to set real time mode. Error: %s", errorCodes[rc].c_str());
    //        return false;
    //    }

    if (ExposureRequest != (1.0 / Streamer->getTargetFPS()))
    {
        ExposureRequest = 1.0 / Streamer->getTargetFPS();

        uint32_t uSecs = static_cast<uint32_t>(ExposureRequest * 1000000.0f);
        if ( (rc = Toupcam_put_ExpoTime(m_CameraHandle, uSecs)) < 0)
        {
            LOGF_ERROR("Failed to set video exposure time. Error: %s", errorCodes[rc].c_str());
            return false;
        }
    }

    if ( (rc = Toupcam_put_Option(m_CameraHandle, TOUPCAM_OPTION_TRIGGER, 0)) < 0)
    {
        LOGF_ERROR("Failed to set video trigger mode. %s", errorCodes[rc].c_str());
        return false;
    }
    m_CurrentTriggerMode = TRIGGER_VIDEO;

    return true;
}

bool TOUPCAM::StopStreaming()
{
    int rc=0;

    //    if ( (rc = Toupcam_put_RealTime(m_CameraHandle, false)) < 0)
    //    {
    //        LOGF_ERROR("Failed to disable real time mode. Error: %s", errorCodes[rc].c_str());
    //        return false;
    //    }

    if ( (rc = Toupcam_put_Option(m_CameraHandle, TOUPCAM_OPTION_TRIGGER, 1)) < 0)
    {
        LOGF_ERROR("Failed to set video trigger mode. %s", errorCodes[rc].c_str());
        return false;
    }
    m_CurrentTriggerMode = TRIGGER_SOFTWARE;

    return true;
}

int TOUPCAM::SetTemperature(double temperature)
{
    // If there difference, for example, is less than 0.1 degrees, let's immediately return OK.
    if (fabs(temperature - TemperatureN[0].value) < TEMP_THRESHOLD)
        return 1;

    if (activateCooler(true) == false)
    {
        LOG_ERROR("Failed to activate cooler!");
        return -1;
    }

    int16_t nTemperature = static_cast<int16_t>(temperature * 10.0);

    HRESULT rc = Toupcam_put_Temperature(m_CameraHandle, nTemperature);
    if (rc < 0)
    {
        LOGF_ERROR("Failed to set temperature. %s", errorCodes[rc].c_str());
        return -1;
    }

    // Otherwise, we set the temperature request and we update the status in TimerHit() function.
    TemperatureRequest = temperature;
    LOGF_INFO("Setting CCD temperature to %+06.2f C", temperature);
    return 0;
}

bool TOUPCAM::activateCooler(bool enable)
{
    HRESULT rc = Toupcam_put_Option(m_CameraHandle, TOUPCAM_OPTION_TEC, enable ? 1 : 0);
    IUResetSwitch(&CoolerSP);
    if (rc < 0)
    {
        CoolerS[enable ? TC_COOLER_OFF : TC_COOLER_ON].s = ISS_ON;
        CoolerSP.s = IPS_ALERT;
        LOGF_ERROR("Failed to turn cooler %s (%s)", enable ? "on" : "off", errorCodes[rc].c_str());
        IDSetSwitch(&CoolerSP, nullptr);
        return false;
    }
    else
    {
        CoolerS[enable ? TC_COOLER_ON : TC_COOLER_OFF].s = ISS_ON;
        CoolerSP.s = IPS_OK;
        IDSetSwitch(&CoolerSP, nullptr);
        return true;
    }
}

bool TOUPCAM::StartExposure(float duration)
{
    HRESULT rc = 0;
    PrimaryCCD.setExposureDuration(static_cast<double>(duration));

    uint32_t uSecs = static_cast<uint32_t>(duration * 1000000.0f);

    LOGF_DEBUG("Starting exposure: %d us @ %s", uSecs, IUFindOnSwitch(&ResolutionSP)->label);

    // Only update exposure when necessary
    if (ExposureRequest != duration)
    {
        ExposureRequest = duration;

        if ( (rc = Toupcam_put_ExpoTime(m_CameraHandle, uSecs)) < 0)
        {
            LOGF_ERROR("Failed to set exposure time. Error: %s", errorCodes[rc].c_str());
            return false;
        }
    }

    // If we have still support?
    /*
    if (m_Instance->model->still > 0)
    {
        if ( (rc = Toupcam_Snap(m_CameraHandle, IUFindOnSwitchIndex(&ResolutionSP))) < 0)
        {
            LOGF_ERROR("Failed to take a snap. Error: %d", rc);
            return false;
        }
    }
    */

    struct timeval exposure_time, current_time;
    gettimeofday(&current_time, nullptr);
    exposure_time.tv_sec = uSecs / 1000000;
    exposure_time.tv_usec= uSecs % 1000000;
    timeradd(&current_time, &exposure_time, &ExposureEnd);

    if (ExposureRequest > VERBOSE_EXPOSURE)
        LOGF_INFO("Taking a %g seconds frame...", static_cast<double>(ExposureRequest));

    InExposure = true;

    if (m_CurrentTriggerMode != TRIGGER_SOFTWARE)
    {
        if ( (rc = Toupcam_put_Option(m_CameraHandle, TOUPCAM_OPTION_TRIGGER, 1)) < 0)
        {
            LOGF_ERROR("Failed to set software trigger mode. %s", errorCodes[rc].c_str());
        }
        m_CurrentTriggerMode = TRIGGER_SOFTWARE;
    }

    int timeMS = uSecs/1000 - 50;
    if (timeMS <= 0)
        sendImageCallBack();
    else if (static_cast<uint32_t>(timeMS) < POLLMS)
        IEAddTimer(timeMS, &TOUPCAM::sendImageCB, this);

    // Trigger an exposure
    if ( (rc = Toupcam_Trigger(m_CameraHandle, 1) < 0) )
    {
        LOGF_ERROR("Failed to trigger exposure. Error: %s", errorCodes[rc].c_str());
        return false;
    }

    return true;
}

bool TOUPCAM::AbortExposure()
{
    Toupcam_Trigger(m_CameraHandle, 0);
    InExposure = false;
    m_TimeoutRetries = 0;
    return true;
}

bool TOUPCAM::UpdateCCDFrame(int x, int y, int w, int h)
{
    // Make sure all are even
    x -= (x % 2);
    y -= (y % 2);
    w -= (w % 2);
    h -= (h % 2);

    if (w > PrimaryCCD.getXRes())
    {
        LOGF_INFO("Error: invalid width requested %d", w);
        return false;
    }
    if (h > PrimaryCCD.getYRes())
    {
        LOGF_INFO("Error: invalid height request %d", h);
        return false;
    }

    LOGF_DEBUG("Toupcam ROI. X: %d Y: %d W: %d H: %d. Binning %dx%d ", x, y, w, h, PrimaryCCD.getBinX(), PrimaryCCD.getBinY());

    HRESULT rc = Toupcam_put_Roi(m_CameraHandle, x, y, w, h);
    if (rc < 0)
    {
        LOGF_ERROR("Error setting camera ROI: %d", rc);
        return false;
    }

    // Set UNBINNED coords
    PrimaryCCD.setFrame(x, y, w, h);


    // Total bytes required for image buffer
    uint32_t nbuf = (w * h * PrimaryCCD.getBPP() / 8) * m_Channels;
    LOGF_DEBUG("Updating frame buffer size to %d bytes.", nbuf);
    PrimaryCCD.setFrameBufferSize(nbuf);

    // Always set BINNED size
    Streamer->setSize(w / PrimaryCCD.getBinX(), h / PrimaryCCD.getBinY());
    return true;
}

bool TOUPCAM::UpdateCCDBin(int binx, int biny)
{
    //    if (binx > 4)
    //    {
    //        LOG_ERROR("Only 1x1, 2x2, 3x3, and 4x4 modes are supported.");
    //        return false;
    //    }

    // TODO add option to select between additive vs. average binning
    HRESULT rc = Toupcam_put_Option(m_CameraHandle, TOUPCAM_OPTION_BINNING, binx);
    if (rc < 0)
    {
        LOGF_ERROR("Binning %dx%d is not support. %s", binx, biny, errorCodes[rc].c_str());
        return false;
    }
    PrimaryCCD.setBin(binx, binx);

    return UpdateCCDFrame(PrimaryCCD.getSubX(), PrimaryCCD.getSubY(), PrimaryCCD.getSubW(), PrimaryCCD.getSubH());
}

// The generic timer call back is used for temperature monitoring
void TOUPCAM::TimerHit()
{
    if (isConnected() == false)
        return;

    if (InExposure)
    {
        struct timeval curtime, diff;
        gettimeofday(&curtime, nullptr);
        timersub(&ExposureEnd, &curtime, &diff);
        double timeleft = diff.tv_sec + diff.tv_usec / 1e6;
        uint32_t msecs = 0;
        if (timeleft <= 0)
            msecs = 0;
        else
            msecs = timeleft * 1000.0;
        // If within 50ms, let's set it done
        if (msecs <= 50)
            sendImageCallBack();
        // If time left is less than our polling then let's send image before next poll event
        else
        {
            if (msecs < POLLMS)
                IEAddTimer(msecs-50, &TOUPCAM::sendImageCB, this);

            PrimaryCCD.setExposureLeft(timeleft);
        }
    }

    if (m_Instance->model->flag & TOUPCAM_FLAG_GETTEMPERATURE)
    {
        double currentTemperature = TemperatureN[0].value;
        int16_t nTemperature=0;
        HRESULT rc = Toupcam_get_Temperature(m_CameraHandle, &nTemperature);
        if (rc < 0)
        {
            LOGF_ERROR("Toupcam_get_Temperature error. %s", errorCodes[rc].c_str());
            TemperatureNP.s = IPS_ALERT;
        }
        else
        {
            TemperatureN[0].value = static_cast<double>(nTemperature / 10.0);
        }

        switch (TemperatureNP.s)
        {
        case IPS_IDLE:
        case IPS_OK:
            if (fabs(currentTemperature - TemperatureN[0].value) > TEMP_THRESHOLD / 10.0)
            {
                IDSetNumber(&TemperatureNP, nullptr);
            }
            break;

        case IPS_ALERT:
            break;

        case IPS_BUSY:
            // If we're within threshold, let's make it BUSY ---> OK
            if (fabs(TemperatureRequest - TemperatureN[0].value) <= TEMP_THRESHOLD)
            {
                TemperatureNP.s = IPS_OK;
            }
            IDSetNumber(&TemperatureNP, nullptr);
            break;
        }
    }

    SetTimer(POLLMS);

}

/* Helper function for NS timer call back */
void TOUPCAM::TimerHelperNS(void *context)
{
    static_cast<TOUPCAM *>(context)->TimerNS();
}

/* The timer call back for NS guiding */
void TOUPCAM::TimerNS()
{
    LOG_DEBUG("Guide NS pulse complete");
    NStimerID = -1;
    GuideComplete(AXIS_DE);
}

/* Stop the timer for NS guiding */
void TOUPCAM::stopTimerNS()
{
    if (NStimerID != -1)
    {
        LOG_DEBUG("Guide NS pulse complete");
        GuideComplete(AXIS_DE);
        IERmTimer(NStimerID);
        NStimerID = -1;
    }
}

IPState TOUPCAM::guidePulseNS(uint32_t ms, eGUIDEDIRECTION dir, const char *dirName)
{
    stopTimerNS();
    NSDir = dir;
    NSDirName = dirName;

    LOGF_DEBUG("Starting %s guide for %d ms", NSDirName, ms);

    // If pulse < 50ms, we wait. Otherwise, we schedule it.
    int uSecs = ms * 1000;
    HRESULT rc = Toupcam_ST4PlusGuide(m_CameraHandle, dir, ms);
    if (rc < 0)
    {
        LOGF_ERROR("%s pulse guiding failed: %s", dirName, errorCodes[rc].c_str());
        return IPS_ALERT;
    }

    if (ms < 50)
    {
        usleep(uSecs);
        return IPS_OK;
    }

    //    struct timeval duration, current_time;
    //    gettimeofday(&current_time, nullptr);
    //    duration.tv_sec = uSecs / 1000000;
    //    duration.tv_usec= uSecs % 1000000;
    //    timeradd(&current_time, &duration, &NSPulseEnd);

    NStimerID = IEAddTimer(ms, TOUPCAM::TimerHelperNS, this);
    return IPS_BUSY;
}

IPState TOUPCAM::GuideNorth(uint32_t ms)
{
    return guidePulseNS(ms, TOUPCAM_NORTH, "North");
}

IPState TOUPCAM::GuideSouth(uint32_t ms)
{
    return guidePulseNS(ms, TOUPCAM_SOUTH, "South");
}

/* Helper function for WE timer call back */
void TOUPCAM::TimerHelperWE(void *context)
{
    static_cast<TOUPCAM *>(context)->TimerWE();
}

/* The timer call back for WE guiding */
void TOUPCAM::TimerWE()
{
    LOG_DEBUG("Guide WE pulse complete");
    WEtimerID = -1;
    GuideComplete(AXIS_RA);
}

void TOUPCAM::stopTimerWE()
{
    if (WEtimerID != -1)
    {
        LOG_DEBUG("Guide WE pulse complete");
        GuideComplete(AXIS_RA);
        IERmTimer(WEtimerID);
        WEtimerID = -1;
    }
}

IPState TOUPCAM::guidePulseWE(uint32_t ms, eGUIDEDIRECTION dir, const char *dirName)
{
    stopTimerWE();
    WEDir = dir;
    WEDirName = dirName;

    LOGF_DEBUG("Starting %s guide for %d ms", WEDirName, ms);

    // If pulse < 50ms, we wait. Otherwise, we schedule it.
    int uSecs = ms * 1000;
    HRESULT rc = Toupcam_ST4PlusGuide(m_CameraHandle, dir, ms);
    if (rc < 0)
    {
        LOGF_ERROR("%s pulse guiding failed: %s", dirName, errorCodes[rc].c_str());
        return IPS_ALERT;
    }

    if (ms < 50)
    {
        usleep(uSecs);
        return IPS_OK;
    }

    //    struct timeval duration, current_time;
    //    gettimeofday(&current_time, nullptr);
    //    duration.tv_sec = uSecs / 1000000;
    //    duration.tv_usec= uSecs % 1000000;
    //    timeradd(&current_time, &duration, &WEPulseEnd);

    WEtimerID = IEAddTimer(ms, TOUPCAM::TimerHelperWE, this);
    return IPS_BUSY;
}

IPState TOUPCAM::GuideEast(uint32_t ms)
{
    return guidePulseWE(ms, TOUPCAM_EAST, "East");
}

IPState TOUPCAM::GuideWest(uint32_t ms)
{
    return guidePulseWE(ms, TOUPCAM_WEST, "West");
}

const char *TOUPCAM::getBayerString()
{
    uint32_t nFourCC = 0, nBitDepth=0;
    Toupcam_get_RawFormat(m_CameraHandle, &nFourCC, &nBitDepth);

    LOGF_DEBUG("Raw format FourCC %#8X bitDepth %d", nFourCC, nBitDepth);

    // 8, 10, 12, 14, or 16
    m_RawBitsPerPixel = nBitDepth;

    switch (nFourCC)
    {
    case FMT_GBRG:
        m_CameraPixelFormat = INDI_BAYER_GBRG;
        return "GBRG";
    case FMT_RGGB:
        m_CameraPixelFormat = INDI_BAYER_RGGB;
        return "RGGB";
    case FMT_BGGR:
        m_CameraPixelFormat = INDI_BAYER_BGGR;
        return "BGGR";
    case FMT_GRBG:
        m_CameraPixelFormat = INDI_BAYER_GRBG;
        return "GRBG";
    default:
        m_CameraPixelFormat = INDI_BAYER_RGGB;
        return "RGGB";
    }
}

void TOUPCAM::refreshControls()
{
    IDSetNumber(&ControlNP, nullptr);
}

#if 0
void *TOUPCAM::imagingHelper(void *context)
{
    return static_cast<TOUPCAM *>(context)->imagingThreadEntry();
}

/*
 * A dedicated thread is used for handling streaming video and image
 * exposures because the operations take too much time to be done
 * as part of a timer call-back: there is one timer for the entire
 * process, which must handle events for all ASI cameras
 */
void *TOUPCAM::imagingThreadEntry()
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
            getSnapImage();
        }
        else if (threadRequest == StateStream)
        {
            getVideoImage();
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

void TOUPCAM::getVideoImage()
{
#if 0
    int ret;
    int frames = 0;

    while (threadRequest == StateStream)
    {
        pthread_mutex_unlock(&condMutex);

        uint8_t *targetFrame = PrimaryCCD.getFrameBuffer();
        uint32_t totalBytes  = PrimaryCCD.getFrameBufferSize();
        int waitMS           = (int)(ExposureRequest * 2000.0) + 500;

        ret = ASIGetVideoData(m_camInfo->CameraID, targetFrame, totalBytes,
                              waitMS);
        if (ret != 0)
        {
            if (ret != ASI_ERROR_TIMEOUT)
            {
                Streamer->setStream(false);
                pthread_mutex_lock(&condMutex);
                if (threadRequest == StateStream)
                {
                    LOGF_ERROR(
                                "Error reading video data (%d)", ret);
                    threadRequest = StateIdle;
                }
                break;
            }
            else
            {
                frames = 0;
                usleep(100);
            }
        }
        else
        {
            if (currentVideoFormat == ASI_IMG_RGB24)
            {
                for (uint32_t i = 0; i < totalBytes; i += 3)
                {
                    uint8_t r = targetFrame[i];
                    targetFrame[i] = targetFrame[i + 2];
                    targetFrame[i + 2] = r;
                }
            }

            Streamer->newFrame(targetFrame, totalBytes);

            /*
             * Release the CPU every 30 frames
             */
            frames++;
            if (frames == 30)
            {
                frames = 0;
                usleep(10);
            }
        }

        pthread_mutex_lock(&condMutex);
    }
#endif
}

/* Caller must hold the mutex */
void TOUPCAM::exposureSetRequest(ImageState request)
{
    if (threadRequest == StateExposure)
    {
        threadRequest = request;
    }
}
#endif

void TOUPCAM::addFITSKeywords(fitsfile *fptr, INDI::CCDChip *targetChip)
{
    INDI::CCD::addFITSKeywords(fptr, targetChip);

    INumber *gainNP = IUFindNumber(&ControlNP, ControlN[TC_GAIN].name);

    if (gainNP)
    {
        int status = 0;
        fits_update_key_s(fptr, TDOUBLE, "Gain", &(gainNP->value), "Gain", &status);
    }
}

bool TOUPCAM::saveConfigItems(FILE *fp)
{
    INDI::CCD::saveConfigItems(fp);

    if (HasCooler())
        IUSaveConfigSwitch(fp, &CoolerSP);
    IUSaveConfigNumber(fp, &ControlNP);

    if (m_MonoCamera == false)
        IUSaveConfigSwitch(fp, &WBAutoSP);

    return true;
}

void TOUPCAM::TempTintCB(const int nTemp, const int nTint, void* pCtx)
{
    static_cast<TOUPCAM*>(pCtx)->TempTintChanged(nTemp, nTint);
}

void TOUPCAM::TempTintChanged(const int nTemp, const int nTint)
{
    WBTempTintN[TC_WB_TEMP].value = nTemp;
    WBTempTintN[TC_WB_TINT].value = nTint;
    WBTempTintNP.s = IPS_OK;
    IDSetNumber(&WBTempTintNP, nullptr);
}

void TOUPCAM::WhiteBalanceCB(const int aGain[3], void* pCtx)
{
    static_cast<TOUPCAM*>(pCtx)->WhiteBalanceChanged(aGain);
}
void TOUPCAM::WhiteBalanceChanged(const int aGain[3])
{
    WBRGBN[TC_WB_R].value = aGain[TC_WB_R];
    WBRGBN[TC_WB_G].value = aGain[TC_WB_G];
    WBRGBN[TC_WB_B].value = aGain[TC_WB_B];
    WBRGBNP.s = IPS_OK;
    IDSetNumber(&WBRGBNP, nullptr);
}

void TOUPCAM::BlackBalanceCB(const unsigned short aSub[3], void* pCtx)
{
    static_cast<TOUPCAM*>(pCtx)->BlackBalanceChanged(aSub);
}
void TOUPCAM::BlackBalanceChanged(const unsigned short aSub[3])
{
    BlackBalanceN[TC_BLACK_R].value = aSub[TC_BLACK_R];
    BlackBalanceN[TC_BLACK_G].value = aSub[TC_BLACK_G];
    BlackBalanceN[TC_BLACK_B].value = aSub[TC_BLACK_B];
    BlackBalanceNP.s = IPS_OK;
    IDSetNumber(&BlackBalanceNP, nullptr);
}

void TOUPCAM::AutoExposureCB(void* pCtx)
{
    static_cast<TOUPCAM*>(pCtx)->AutoExposureChanged();
}
void TOUPCAM::AutoExposureChanged()
{
    // TODO
}

void TOUPCAM::sendImageCB(void* pCtx)
{
    static_cast<TOUPCAM*>(pCtx)->sendImageCallBack();
}

void TOUPCAM::eventCB(unsigned event, void* pCtx)
{
    static_cast<TOUPCAM*>(pCtx)->eventPullCallBack(event);
}

void TOUPCAM::sendImageCallBack()
{
    PrimaryCCD.setExposureLeft(0);
    InExposure = false;
    m_lastEventID = -1;

    RemoveTimer(m_TimeoutTimerID);
    m_TimeoutTimerID = IEAddTimer(POLLMS+50, &TOUPCAM::checkTimeoutHelper, this);
}

void TOUPCAM::checkTimeoutHelper(void *context)
{
    static_cast<TOUPCAM*>(context)->checkCameraCallback();
}

void TOUPCAM::checkCameraCallback()
{
    if (m_lastEventID != 4)
    {
        LOG_DEBUG("Exposure timeout, restarting...");
        if (m_TimeoutRetries++ >= MAX_RETRIES)
        {
            PrimaryCCD.setExposureFailed();
            m_TimeoutRetries=0;
            LOG_ERROR("Exposure timeout.");
        }
        else
            StartExposure(PrimaryCCD.getExposureDuration());
    }
    else {
        m_TimeoutRetries = 0;
    }
}

void TOUPCAM::eventPullCallBack(unsigned event)
{
    LOGF_DEBUG("Event %#04X", event);

    m_lastEventID = event;

    switch (event)
    {
    case TOUPCAM_EVENT_EXPOSURE:
        break;
    case TOUPCAM_EVENT_TEMPTINT:
        break;
    case TOUPCAM_EVENT_IMAGE:
    {
        m_TimeoutRetries=0;
        ToupcamFrameInfoV2 info;
        memset(&info, 0, sizeof(ToupcamFrameInfoV2));

        //PrimaryCCD.setFrameBufferSize(subFrameSize, false);
        //PrimaryCCD.setResolution(w, h);
        //PrimaryCCD.setNAxis(m_Channels == 1 ? 2 : 3);
        //PrimaryCCD.setBPP(m_BitsPerPixel);

        int captureBits = m_BitsPerPixel == 8 ? 8 : m_MaxBitDepth;

        if (Streamer->isStreaming())
        {
            HRESULT rc = Toupcam_PullImageV2(m_CameraHandle, PrimaryCCD.getFrameBuffer(), captureBits * m_Channels, &info);
            if (rc >= 0)
                Streamer->newFrame(PrimaryCCD.getFrameBuffer(), PrimaryCCD.getFrameBufferSize());
        }
        else
        {
            uint8_t *buffer = PrimaryCCD.getFrameBuffer();

            if (m_MonoCamera == false && m_CurrentVideoFormat == TC_VIDEO_COLOR_RGB)
                buffer = static_cast<uint8_t*>(malloc(PrimaryCCD.getXRes()*PrimaryCCD.getYRes()*3));

            HRESULT rc = Toupcam_PullImageV2(m_CameraHandle, buffer, captureBits * m_Channels, &info);
            if (rc < 0)
            {
                LOGF_ERROR("Failed to pull image. %s", errorCodes[rc].c_str());
                PrimaryCCD.setExposureFailed();
                if (m_MonoCamera == false && m_CurrentVideoFormat == TC_VIDEO_COLOR_RGB)
                    free(buffer);
            }
            else
            {
                if (m_MonoCamera == false && m_CurrentVideoFormat == TC_VIDEO_COLOR_RGB)
                {
                    uint8_t *image  = PrimaryCCD.getFrameBuffer();
                    uint32_t width  = PrimaryCCD.getSubW() / PrimaryCCD.getBinX() * (PrimaryCCD.getBPP() / 8);
                    uint32_t height = PrimaryCCD.getSubH() / PrimaryCCD.getBinY() * (PrimaryCCD.getBPP() / 8);

                    uint8_t *subR = image;
                    uint8_t *subG = image + width * height;
                    uint8_t *subB = image + width * height * 2;
                    int size      = width * height * 3 - 3;

                    // RGB to three sepearate R-frame, G-frame, and B-frame for color FITS
                    for (int i = 0; i <= size; i += 3)
                    {
                        *subR++ = buffer[i];
                        *subG++ = buffer[i + 1];
                        *subB++ = buffer[i + 2];
                    }

                    free(buffer);
                }

                LOGF_DEBUG("Image received. Width: %d Height: %d flag: %d timestamp: %ld", info.width, info.height, info.flag, info.timestamp);
                ExposureComplete(&PrimaryCCD);
            }
        }
    }
        break;
    case TOUPCAM_EVENT_STILLIMAGE:
    {
        ToupcamFrameInfoV2 info;
        memset(&info, 0, sizeof(ToupcamFrameInfoV2));
        HRESULT rc = Toupcam_PullStillImageV2(m_CameraHandle, PrimaryCCD.getFrameBuffer(), 24, &info);
        if (rc < 0)
        {
            LOGF_ERROR("Failed to pull image. %s", errorCodes[rc].c_str());
            PrimaryCCD.setExposureFailed();
        }
        else
        {
            PrimaryCCD.setExposureLeft(0);
            InExposure  = false;
            ExposureComplete(&PrimaryCCD);
            LOGF_DEBUG("Image captured. Width: %d Height: %d flag: %d timestamp: %ld", info.width, info.height, info.flag, info.timestamp);
        }
    }
        break;
    case TOUPCAM_EVENT_WBGAIN:
        LOG_DEBUG("White Balance Gain changed.");
        break;
    case TOUPCAM_EVENT_TRIGGERFAIL:
        break;
    case TOUPCAM_EVENT_BLACK:
        LOG_DEBUG("Black Balance Gain changed.");
        break;
    case TOUPCAM_EVENT_FFC:
        break;
    case TOUPCAM_EVENT_DFC:
        break;
    case TOUPCAM_EVENT_ERROR:
        break;
    case TOUPCAM_EVENT_DISCONNECTED:
        LOG_DEBUG("Camera disconnected.");
        break;
    case TOUPCAM_EVENT_TIMEOUT:
        LOG_DEBUG("Camera timed out.");
        PrimaryCCD.setExposureFailed();
        break;
    case TOUPCAM_EVENT_FACTORY:
        break;
    default:
        break;
    }
}
