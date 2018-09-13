/*
 ToupCam CCD Driver

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

static int iConnectedCamerasCount;
static ToupcamInstV2 pToupCameraInfo[TOUPCAM_MAX];
static TOUPCAM *cameras[TOUPCAM_MAX];

//#define USE_SIMULATION
#ifdef USE_SIMULATION
static ToupcamModelV2 model;
#endif

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
#ifdef USE_SIMULATION
        iConnectedCamerasCount=1;
        strncpy(pToupCameraInfo[0].displayname, "Simulation", 64);
        model.flag = TOUPCAM_FLAG_RAW16 | TOUPCAM_FLAG_BINSKIP_SUPPORTED | TOUPCAM_FLAG_ROI_HARDWARE | TOUPCAM_FLAG_TEC_ONOFF | TOUPCAM_FLAG_ST4;
        model.name = pToupCameraInfo[0].displayname;
        model.xpixsz = model.ypixsz  = 5.4;
        model.res[0].width = 1280;
        model.res[0].height = 1024;
        model.res[1].width = 640;
        model.res[1].height = 480;
        pToupCameraInfo[0].model = &model;
        cameras[0] = new TOUPCAM(&pToupCameraInfo[0]);
#else
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
#endif

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

    WEPulseRequest = NSPulseRequest = 0;
    WEtimerID = NStimerID = -1;
    NSDir = TOUPCAM_NORTH;
    WEDir = TOUPCAM_WEST;

    snprintf(this->name, MAXINDIDEVICE, "ToupCam %s", instance->displayname);
    setDeviceName(this->name);
}

const char *TOUPCAM::getDefaultName()
{
    return "ToupCam";
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
    /// Cooler Value
    ///////////////////////////////////////////////////////////////////////////////////
    IUFillNumber(&CoolerN[0], "CCD_COOLER_VALUE", "Cooling Power (%)", "%+06.2f", 0., 1., .2, 0.0);
    IUFillNumberVector(&CoolerNP, CoolerN, 1, getDeviceName(), "CCD_COOLER_POWER", "Cooling Power", MAIN_CONTROL_TAB,
                       IP_RO, 60, IPS_IDLE);

    ///////////////////////////////////////////////////////////////////////////////////
    /// Controls
    ///////////////////////////////////////////////////////////////////////////////////
    IUFillNumber(&ControlN[TC_CONTRAST], "TC_CONTRAST", "Contrast", "%.f", -100.0, 100, 10, 0);
    IUFillNumber(&ControlN[TC_HUE], "TC_HUE", "Hue", "%.f", -180.0, 180, 10, 0);
    IUFillNumber(&ControlN[TC_SATURATION], "TC_SATURATION", "Saturation", "%.f", 0, 255, 10, 128);
    IUFillNumber(&ControlN[TC_BRIGHTNESS], "TC_BRIGHTNESS", "Brightness", "%.f", -64, 64, 8, 0);
    IUFillNumber(&ControlN[TC_GAMMA], "TC_GAMMA", "Gamma", "%.f", 20, 180, 10, 100);
    IUFillNumberVector(&ControlNP, ControlN, 5, getDeviceName(), "CCD_CONTROLS", "Controls", CONTROL_TAB, IP_RW, 60,
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
                       ISR_NOFMANY, 0, IPS_IDLE);


    ///////////////////////////////////////////////////////////////////////////////////
    // White Balance - Temp/Tint
    ///////////////////////////////////////////////////////////////////////////////////
    IUFillNumber(&WBTempTintN[TC_WB_TEMP], "TC_WB_TEMP", "Temp", "%.f", 2000, 15000, 1000, 6503);
    IUFillNumber(&WBTempTintN[TC_WB_TINT], "TC_WB_TINT", "Tint", "%.f", 200, 2500, 100, 1000);
    IUFillNumberVector(&WBTempTintNP, WBTempTintN, 2, getDeviceName(), "TC_WB_TT", "White Balance #1", LEVEL_TAB, IP_RW, 60, IPS_IDLE);

    ///////////////////////////////////////////////////////////////////////////////////
    // White Balance - RGB
    ///////////////////////////////////////////////////////////////////////////////////
    IUFillNumber(&WBRGBN[TC_WB_R], "TC_WB_R", "Red", "%.f", -127, 127, 10, 0);
    IUFillNumber(&WBRGBN[TC_WB_G], "TC_WB_G", "Green", "%.f", -127, 127, 10, 0);
    IUFillNumber(&WBRGBN[TC_WB_B], "TC_WB_B", "Blue", "%.f", -127, 127, 10, 0);
    IUFillNumberVector(&WBRGBNP, WBRGBN, 3, getDeviceName(), "TC_WB_RGB", "White Balance #2", LEVEL_TAB, IP_RW, 60, IPS_IDLE);


    ///////////////////////////////////////////////////////////////////////////////////
    /// White Balance - Auto
    ///////////////////////////////////////////////////////////////////////////////////
    IUFillSwitch(&WBAutoS[TC_AUTO_WB_TT], "TC_AUTO_WB_TT", "Temp/Tint", ISS_OFF);
    IUFillSwitch(&WBAutoS[TC_AUTO_WB_RGB], "TC_AUTO_WB_RGB", "RGB", ISS_ON);
    IUFillSwitchVector(&WBAutoSP, WBAutoS, 2, getDeviceName(), "TC_AUTO_WB", "Auto Balance", LEVEL_TAB, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);

    ///////////////////////////////////////////////////////////////////////////////////
    /// Video Format
    ///////////////////////////////////////////////////////////////////////////////////
    IUFillSwitch(&VideoFormatS[TC_VIDEO_MONO_8], "TC_VIDEO_MONO_8", "Mono 8", ISS_OFF);
    IUFillSwitch(&VideoFormatS[TC_VIDEO_MONO_16], "TC_VIDEO_MONO_16", "Mono 16", ISS_OFF);
    IUFillSwitch(&VideoFormatS[TC_VIDEO_RGB], "TC_VIDEO_RGB", "RGB", ISS_ON);
    IUFillSwitch(&VideoFormatS[TC_VIDEO_RAW], "TC_VIDEO_RAW", "Raw", ISS_OFF);
    IUFillSwitchVector(&VideoFormatSP, VideoFormatS, 4, getDeviceName(), "CCD_VIDEO_FORMAT", "Format", CONTROL_TAB, IP_RW,
                       ISR_1OFMANY, 60, IPS_IDLE);

    IUFillSwitchVector(&ResolutionSP, ResolutionS, 0, getDeviceName(), "CCD_RESOLUTION", "Resolution", CONTROL_TAB, IP_RW,
                       ISR_1OFMANY, 60, IPS_IDLE);

#ifdef USE_SIMULATION
    IUFillText(&SDKVersionS[0], "VERSION", "Version", "Simulation");
#else
    IUFillText(&SDKVersionS[0], "VERSION", "Version", Toupcam_Version());
#endif
    IUFillTextVector(&SDKVersionSP, SDKVersionS, 1, getDeviceName(), "SDK", "SDK", INFO_TAB, IP_RO, 60, IPS_IDLE);

    PrimaryCCD.setMinMaxStep("CCD_BINNING", "HOR_BIN", 1, 2, 1, false);
    PrimaryCCD.setMinMaxStep("CCD_BINNING", "VER_BIN", 1, 2, 1, false);

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
            defineNumber(&CoolerNP);
            loadConfig(true, "CCD_COOLER_POWER");
            defineSwitch(&CoolerSP);
            loadConfig(true, "CCD_COOLER");
        }
        // Even if there is no cooler, we define temperature property as READ ONLY
        else if (m_Instance->model->flag & TOUPCAM_FLAG_GETTEMPERATURE)
        {
            TemperatureNP.p = IP_RO;
            defineNumber(&TemperatureNP);
        }

        defineNumber(&ControlNP);
        defineSwitch(&AutoControlSP);
        defineSwitch(&VideoFormatSP);
        defineSwitch(&ResolutionSP);

        // Levels
        defineNumber(&LevelRangeNP);
        defineNumber(&BlackBalanceNP);

        // Balance
        defineNumber(&WBTempTintNP);
        defineNumber(&WBRGBNP);
        defineSwitch(&WBAutoSP);

        defineText(&SDKVersionSP);
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

        deleteProperty(ControlNP.name);
        deleteProperty(AutoControlSP.name);
        deleteProperty(VideoFormatSP.name);
        deleteProperty(ResolutionSP.name);

        deleteProperty(LevelRangeNP.name);
        deleteProperty(BlackBalanceNP.name);

        deleteProperty(WBTempTintNP.name);
        deleteProperty(WBRGBNP.name);
        deleteProperty(WBAutoSP.name);

        deleteProperty(SDKVersionSP.name);
    }

    return true;
}

bool TOUPCAM::Connect()
{
    LOGF_DEBUG("Attempting to open %s with ID %s", name, m_Instance->id);

    if (isSimulation() == false)
        m_CameraHandle = Toupcam_Open(m_Instance->id);

    if (m_CameraHandle == nullptr)
    {
        LOG_ERROR("Error connecting to the camera");
        return false;
    }

    uint32_t cap = 0;

    cap |= CCD_CAN_ABORT;

    // If raw format is support then we have bayer
    if (m_Instance->model->flag & (TOUPCAM_FLAG_RAW10 | TOUPCAM_FLAG_RAW12 | TOUPCAM_FLAG_RAW14 | TOUPCAM_FLAG_RAW16))
    {
        LOG_DEBUG("RAW format supported. Bayer enabled.");
        cap |= CCD_HAS_BAYER;
    }

    if (m_Instance->model->flag & TOUPCAM_FLAG_BINSKIP_SUPPORTED)
    {
        LOG_DEBUG("Bin-Skip supported.");
        cap |= CCD_CAN_BIN;
    }

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

    // Start callback
    if (Toupcam_StartPullModeWithCallback(m_CameraHandle, &TOUPCAM::eventCB, this) < 0)
    {
        Toupcam_Close(m_CameraHandle);
        return false;
    }

    /*
     * Create the imaging thread and wait for it to start
     * N.B. Do we really need this with ToupCam?
     */
#if 0
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
#endif

    // Success!
    LOGF_INFO("%s is online. Retrieving basic data.", getDeviceName());

    return true;
}

bool TOUPCAM::Disconnect()
{
    ImageState  tState;
    LOGF_DEBUG("Closing %s...", getDeviceName());

    stopTimerNS();
    stopTimerWE();

    //RemoveTimer(genTimerID);
    //genTimerID = -1;

    pthread_mutex_lock(&condMutex);
    tState = threadState;
    threadRequest = StateTerminate;
    pthread_cond_signal(&cv);
    pthread_mutex_unlock(&condMutex);
    pthread_join(imagingThread, nullptr);
    tState = StateNone;

    Toupcam_Close(m_CameraHandle);

    LOGF_INFO("% is offline.", getDeviceName());

    return true;
}

void TOUPCAM::setupParams()
{
    uint32_t min=0,max=0,current=0;
    Toupcam_get_ExpTimeRange(m_CameraHandle, &min, &max, &current);
    LOGF_DEBUG("Exposure Time Range (us): Min %d Max %d Default %d", min, max, current);

    PrimaryCCD.setMinMaxStep("CCD_EXPOSURE", "CCD_EXPOSURE_VALUE", min/1000000.0, max/1000000.0, 0, false);

    uint8_t bitsPerPixel = 8;
    // bitdepth supported
    if (m_Instance->model->flag & (TOUPCAM_FLAG_RAW10 | TOUPCAM_FLAG_RAW12 | TOUPCAM_FLAG_RAW14 | TOUPCAM_FLAG_RAW16))
    {
        // enable bitdepth
        Toupcam_put_Option(m_CameraHandle, TOUPCAM_OPTION_BITDEPTH, 1);
        bitsPerPixel = 16;
    }

    LOGF_DEBUG("Bits Per Pixel: %d", bitsPerPixel);

    // Get how many resolutions available for the camera
    ResolutionSP.nsp = Toupcam_get_ResolutionNumber(m_CameraHandle);

    int w[TOUPCAM_MAX]={0},h[TOUPCAM_MAX]={0};
    // Get each resolution width x height
    for (uint8_t i=0; i < ResolutionSP.nsp; i++)
    {
        Toupcam_get_Resolution(m_CameraHandle, i, &w[i], &h[i]);
        char label[MAXINDILABEL] = {0};
        snprintf(label, MAXINDILABEL, "%d x %d", w[i], h[i]);
        LOGF_DEBUG("Resolution #%d: %s", i+1, label);
        IUFillSwitch(&ResolutionS[i], label, label, ISS_OFF);
    }

    // Get active resolution index
    uint32_t currentResolutionIndex = 0;
    Toupcam_get_eSize(m_CameraHandle, &currentResolutionIndex);
    ResolutionS[currentResolutionIndex].s = ISS_ON;

    SetCCDParams(w[currentResolutionIndex], h[currentResolutionIndex], bitsPerPixel, m_Instance->model->xpixsz, m_Instance->model->ypixsz);

    // Allocate memory
    // 3 RGB channels
    PrimaryCCD.setFrameBufferSize(PrimaryCCD.getXRes() * PrimaryCCD.getYRes() * 3);
    Streamer->setPixelFormat(INDI_RGB, bitsPerPixel);
    Streamer->setSize(PrimaryCCD.getXRes(), PrimaryCCD.getYRes());

    IUSaveText(&BayerT[2], getBayerString());
}

bool TOUPCAM::allocateFrameBuffer()
{
    // Allocate memory
    // 3 RGB channels
    switch (currentVideoFormat)
    {
    case TC_VIDEO_MONO_8:
        PrimaryCCD.setFrameBufferSize(PrimaryCCD.getXRes() * PrimaryCCD.getYRes());
        Streamer->setPixelFormat(INDI_MONO, 8);
        break;

    case TC_VIDEO_MONO_16:
        PrimaryCCD.setFrameBufferSize(PrimaryCCD.getXRes() * PrimaryCCD.getYRes() * 2);
        Streamer->setPixelFormat(INDI_MONO, 16);
        break;

    case TC_VIDEO_RGB:
        PrimaryCCD.setFrameBufferSize(PrimaryCCD.getXRes() * PrimaryCCD.getYRes() * 3);
        Streamer->setPixelFormat(INDI_RGB, 8);
        break;

    case TC_VIDEO_RAW:
        PrimaryCCD.setFrameBufferSize(PrimaryCCD.getXRes() * PrimaryCCD.getYRes() * 2);
        // FIXME get actual pixel format
        Streamer->setPixelFormat(INDI_BAYER_BGGR, 8);
        break;

    }

    Streamer->setSize(PrimaryCCD.getXRes(), PrimaryCCD.getYRes());
    return true;
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
            double oldValues[5];
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
                if (ControlN[i].value == oldValues[i])
                    continue;

                int value = static_cast<int>(ControlN[i].value);
                switch (i)
                {
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
                LOGF_ERROR("Failed to set level range. Error %d", rc);

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
                LOGF_ERROR("Failed to set Black Balance. Error %d", rc);

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
                LOGF_ERROR("Failed to set White Balance Tempeture & Tint. Error %d", rc);

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
                LOGF_ERROR("Failed to set White Balance gain. Error %d", rc);

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

            if (CoolerS[0].s == ISS_ON)
                activateCooler(true);
            else
                activateCooler(false);

            return true;
        }

        //////////////////////////////////////////////////////////////////////
        /// Video Format
        //////////////////////////////////////////////////////////////////////
        if (!strcmp(name, VideoFormatSP.name))
        {
            if (Streamer->isBusy())
            {
                VideoFormatSP.s = IPS_ALERT;
                LOG_ERROR("Cannot change format while streaming/recording.");
                IDSetSwitch(&VideoFormatSP, nullptr);
                return true;
            }

            const char *targetFormat = IUFindOnSwitchName(states, names, n);
            int targetIndex=-1;
            for (int i=0; i < VideoFormatSP.nsp; i++)
            {
                if (!strcmp(targetFormat, VideoFormatS[i].name))
                {
                    targetIndex=i;
                    break;
                }
            }

            if (targetIndex == -1)
            {
                VideoFormatSP.s = IPS_ALERT;
                LOGF_ERROR("Unable to locate format %s.", targetFormat);
                IDSetSwitch(&VideoFormatSP, nullptr);
                return true;
            }

            return setVideoFormat(targetIndex);
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
            switch (IUFindOnSwitchIndex(&AutoControlSP))
            {
            case TC_AUTO_EXPOSURE:
                rc = Toupcam_put_AutoExpoEnable(m_CameraHandle, (AutoControlS[TC_AUTO_EXPOSURE].s == ISS_ON) ? TRUE : FALSE);
                break;
            case TC_AUTO_TINT:
                rc = Toupcam_AwbOnePush(m_CameraHandle, &TOUPCAM::TempTintCB, this);
                break;
            case TC_AUTO_WB:
                rc = Toupcam_AwbInit(m_CameraHandle, &TOUPCAM::WhiteBalanceCB, this);
                break;
            case TC_AUTO_BB:
                rc = Toupcam_AbbOnePush(m_CameraHandle, &TOUPCAM::BlackBalanceCB, this);
                break;
            default:
                rc = -1;
            }

            if (rc < 0)
            {
                IUResetSwitch(&AutoControlSP);
                AutoControlS[previousSwitch].s = ISS_ON;
                AutoControlSP.s = IPS_ALERT;
            }
            else
                AutoControlSP.s = IPS_OK;

            IDSetSwitch(&AutoControlSP, nullptr);
            return true;

        }        
    }

    return INDI::CCD::ISNewSwitch(dev, name, states, names, n);
}

bool TOUPCAM::setVideoFormat(uint8_t index)
{
#if 0
    IUResetSwitch(&VideoFormatSP);
    VideoFormatS[index].s = ISS_ON;

    ASI_IMG_TYPE type = getImageType();

    switch (type)
    {
    case ASI_IMG_RAW16:
        PrimaryCCD.setBPP(16);
        LOG_WARN("Warning: 16bit RAW is not supported on all hardware platforms.");
        break;

    default:
        PrimaryCCD.setBPP(8);
        break;
    }

    UpdateCCDFrame(PrimaryCCD.getSubX(), PrimaryCCD.getSubY(), PrimaryCCD.getSubW(), PrimaryCCD.getSubH());

    updateRecorderFormat();

    VideoFormatSP.s = IPS_OK;
    IDSetSwitch(&VideoFormatSP, nullptr);
#endif

    return true;
}

bool TOUPCAM::StartStreaming()
{
#if 0
    ExposureRequest = 1.0 / Streamer->getTargetFPS();
    long uSecs = (long)(ExposureRequest * 950000.0);
    ASISetControlValue(m_camInfo->CameraID, ASI_EXPOSURE, uSecs, ASI_FALSE);
    ASIStartVideoCapture(m_camInfo->CameraID);
    pthread_mutex_lock(&condMutex);
    threadRequest = StateStream;
    pthread_cond_signal(&cv);
    pthread_mutex_unlock(&condMutex);
#endif
    return true;
}

bool TOUPCAM::StopStreaming()
{
#if 0
    pthread_mutex_lock(&condMutex);
    threadRequest = StateAbort;
    pthread_cond_signal(&cv);
    while (threadState == StateStream)
    {
        pthread_cond_wait(&cv, &condMutex);
    }
    pthread_mutex_unlock(&condMutex);
    ASIStopVideoCapture(m_camInfo->CameraID);

#endif
    return true;
}

int TOUPCAM::SetTemperature(double temperature)
{
#if 0
    // If there difference, for example, is less than 0.1 degrees, let's immediately return OK.
    if (fabs(temperature - TemperatureN[0].value) < TEMP_THRESHOLD)
        return 1;

    if (activateCooler(true) == false)
    {
        LOG_ERROR("Failed to activate cooler!");
        return -1;
    }

    long tVal;
    if (temperature > 0.5)
        tVal = (long)(temperature + 0.49);
    else if (temperature  < 0.5)
        tVal = (long)(temperature - 0.49);
    else
        tVal = 0;
    if (ASISetControlValue(m_camInfo->CameraID, ASI_TARGET_TEMP, tVal, ASI_TRUE) != 0)
    {
        LOG_ERROR("Failed to set temperature!");
        return -1;
    }

    // Otherwise, we set the temperature request and we update the status in TimerHit() function.
    TemperatureRequest = temperature;
    LOGF_INFO("Setting CCD temperature to %+06.2f C", temperature);
#endif
    return 0;
}

bool TOUPCAM::activateCooler(bool enable)
{
#if 0
    bool rc = (ASISetControlValue(m_camInfo->CameraID, ASI_COOLER_ON, enable ? ASI_TRUE : ASI_FALSE, ASI_FALSE) ==
               0);
    if (rc == false)
        CoolerSP.s = IPS_ALERT;
    else
    {
        CoolerS[0].s = enable ? ISS_ON : ISS_OFF;
        CoolerS[1].s = enable ? ISS_OFF : ISS_ON;
        CoolerSP.s   = enable ? IPS_BUSY: IPS_IDLE;
    }
    IDSetSwitch(&CoolerSP, nullptr);

    return rc;
#endif
    return false;
}

bool TOUPCAM::StartExposure(float duration)
{    
    PrimaryCCD.setExposureDuration(static_cast<double>(duration));
    ExposureRequest = duration;

    LOGF_DEBUG("Start exposure: %.3fs", static_cast<double>(duration));
    uint32_t uSecs = static_cast<uint32_t>(duration * 1000000.0f);

    Toupcam_put_ExpoTime(m_CameraHandle, uSecs);

    if (Toupcam_Snap(m_CameraHandle, IUFindOnSwitchIndex(&ResolutionSP)) < 0)
        return false;

    gettimeofday(&ExpStart, nullptr);
    if (ExposureRequest > VERBOSE_EXPOSURE)
        LOGF_INFO("Taking a %g seconds frame...", static_cast<double>(ExposureRequest));

    InExposure = true;
    pthread_mutex_lock(&condMutex);
    threadRequest = StateExposure;
    pthread_cond_signal(&cv);
    pthread_mutex_unlock(&condMutex);

    return true;
}

bool TOUPCAM::AbortExposure()
{
#if 0
    LOG_DEBUG("AbortExposure");
    pthread_mutex_lock(&condMutex);
    threadRequest = StateAbort;
    pthread_cond_signal(&cv);
    while (threadState == StateExposure)
    {
        pthread_cond_wait(&cv, &condMutex);
    }
    pthread_mutex_unlock(&condMutex);
    ASIStopExposure(m_camInfo->CameraID);
    InExposure = false;
#endif
    return true;
}

bool TOUPCAM::UpdateCCDFrame(int x, int y, int w, int h)
{
#if 0
    long bin_width  = w / PrimaryCCD.getBinX();
    long bin_height = h / PrimaryCCD.getBinY();

    bin_width  = bin_width - (bin_width % 2);
    bin_height = bin_height - (bin_height % 2);

    w = bin_width * PrimaryCCD.getBinX();
    h = bin_height * PrimaryCCD.getBinY();

    int errCode = 0;

    /* Add the X and Y offsets */
    long x_1 = x / PrimaryCCD.getBinX();
    long y_1 = y / PrimaryCCD.getBinY();

    if (bin_width > PrimaryCCD.getXRes() / PrimaryCCD.getBinX())
    {
        LOGF_INFO("Error: invalid width requested %d", w);
        return false;
    }
    else if (bin_height > PrimaryCCD.getYRes() / PrimaryCCD.getBinY())
    {
        LOGF_INFO("Error: invalid height request %d", h);
        return false;
    }

    LOGF_DEBUG("UpdateCCDFrame ASISetROIFormat (%dx%d,  bin %d, type %d)", bin_width, bin_height,
               PrimaryCCD.getBinX(), getImageType());
    if ((errCode = ASISetROIFormat(m_camInfo->CameraID, bin_width, bin_height, PrimaryCCD.getBinX(), getImageType())) !=
            0)
    {
        LOGF_ERROR("ASISetROIFormat (%dx%d @ %d) error (%d)", bin_width, bin_height,
                   PrimaryCCD.getBinX(), errCode);
        return false;
    }

    if ((errCode = ASISetStartPos(m_camInfo->CameraID, x_1, y_1)) != 0)
    {
        LOGF_ERROR("ASISetStartPos (%d,%d) error (%d)", x_1, y_1, errCode);
        return false;
    }

    // Set UNBINNED coords
    PrimaryCCD.setFrame(x, y, w, h);

    int nChannels = 1;

    if (getImageType() == ASI_IMG_RGB24)
        nChannels = 3;

    // Total bytes required for image buffer
    uint32_t nbuf = (bin_width * bin_height * PrimaryCCD.getBPP() / 8) * nChannels;
    LOGF_DEBUG("Setting frame buffer size to %d bytes.", nbuf);
    PrimaryCCD.setFrameBufferSize(nbuf);

    // Always set BINNED size
    Streamer->setSize(bin_width, bin_height);
#endif
    return true;
}

bool TOUPCAM::UpdateCCDBin(int binx, int biny)
{
    INDI_UNUSED(biny);

    if (binx > 2)
    {
        LOG_ERROR("Only 1x1 and 2x2 modes supported.");
        return false;
    }

    PrimaryCCD.setBin(binx, binx);

    return UpdateCCDFrame(PrimaryCCD.getSubX(), PrimaryCCD.getSubY(), PrimaryCCD.getSubW(), PrimaryCCD.getSubH());
}

int TOUPCAM::grabImage()
{
#if 0
    int errCode = 0;

    ASI_IMG_TYPE type = getImageType();

    uint8_t *image = PrimaryCCD.getFrameBuffer();

    uint8_t *buffer = image;

    int width     = PrimaryCCD.getSubW() / PrimaryCCD.getBinX() * (PrimaryCCD.getBPP() / 8);
    int height    = PrimaryCCD.getSubH() / PrimaryCCD.getBinY() * (PrimaryCCD.getBPP() / 8);
    int nChannels = (type == ASI_IMG_RGB24) ? 3 : 1;
    size_t size = width * height * nChannels;

    if (type == ASI_IMG_RGB24)
    {
        buffer = (unsigned char *)malloc(size);
        if (buffer == nullptr)
        {
            LOGF_ERROR("CCD ID: %d malloc failed (RGB 24)",
                       m_camInfo->CameraID);
            return -1;
        }
    }

    if ((errCode = ASIGetDataAfterExp(m_camInfo->CameraID, buffer, size)) != 0)
    {
        LOGF_ERROR("ASIGetDataAfterExp (%dx%d #%d channels) error (%d)", width, height, nChannels,
                   errCode);
        if (type == ASI_IMG_RGB24)
            free(buffer);
        return -1;
    }

    if (type == ASI_IMG_RGB24)
    {
        uint8_t *subR = image;
        uint8_t *subG = image + width * height;
        uint8_t *subB = image + width * height * 2;
        int size      = width * height * 3 - 3;

        for (int i = 0; i <= size; i += 3)
        {
            *subB++ = buffer[i];
            *subG++ = buffer[i + 1];
            *subR++ = buffer[i + 2];
        }

        free(buffer);
    }

    if (type == ASI_IMG_RGB24)
        PrimaryCCD.setNAxis(3);
    else
        PrimaryCCD.setNAxis(2);

    bool restoreBayer = false;

    // If we're sending Luma, turn off bayering
    if (type == ASI_IMG_Y8 && HasBayer())
    {
        restoreBayer = true;
        SetCCDCapability(GetCCDCapability() & ~CCD_HAS_BAYER);
    }

    if (ExposureRequest > VERBOSE_EXPOSURE)
        LOG_INFO("Download complete.");

    ExposureComplete(&PrimaryCCD);

    // Restore bayer cap
    if (restoreBayer)
        SetCCDCapability(GetCCDCapability() | CCD_HAS_BAYER);

#endif
    return 0;
}

// The generic timer call back is used for temperature monitoring
void TOUPCAM::TimerHit()
{
#if 0
    long ASIControlValue = 0;
    ASI_BOOL ASIControlAuto = ASI_FALSE;
    double currentTemperature = TemperatureN[0].value;

    int errCode = ASIGetControlValue(m_camInfo->CameraID,
                                     ASI_TEMPERATURE, &ASIControlValue, &ASIControlAuto);
    if (errCode != 0)
    {
        LOGF_ERROR(
                    "ASIGetControlValue ASI_TEMPERATURE error (%d)", errCode);
        TemperatureNP.s = IPS_ALERT;
    }
    else
    {
        TemperatureN[0].value = (double)ASIControlValue / 10.0;
    }

    switch (TemperatureNP.s)
    {
    case IPS_IDLE:
    case IPS_OK:
        if (fabs(currentTemperature - TemperatureN[0].value)
                > TEMP_THRESHOLD / 10.0)
        {
            IDSetNumber(&TemperatureNP, nullptr);
        }
        break;

    case IPS_ALERT:
        break;

    case IPS_BUSY:
        // If we're within threshold, let's make it BUSY ---> OK
        if (fabs(TemperatureRequest - TemperatureN[0].value)
                <= TEMP_THRESHOLD)
        {
            TemperatureNP.s = IPS_OK;
        }
        IDSetNumber(&TemperatureNP, nullptr);
        break;
    }

    if (HasCooler())
    {
        errCode = ASIGetControlValue(m_camInfo->CameraID,
                                     ASI_COOLER_POWER_PERC, &ASIControlValue, &ASIControlAuto);
        if (errCode != 0)
        {
            LOGF_ERROR(
                        "ASIGetControlValue ASI_COOLER_POWER_PERC error (%d)", errCode);
            CoolerNP.s = IPS_ALERT;
        }
        else
        {
            CoolerN[0].value = ASIControlValue;
            if (ASIControlValue > 0)
                CoolerNP.s = IPS_BUSY;
            else
                CoolerNP.s = IPS_IDLE;
        }
        IDSetNumber(&CoolerNP, nullptr);
    }
    genTimerID = SetTimer(TEMP_TIMER_MS);
#endif
}

/* Helper function for NS timer call back */
void TOUPCAM::TimerHelperNS(void *context)
{
    static_cast<TOUPCAM *>(context)->TimerNS();
}

/* The timer call back for NS guiding */
void TOUPCAM::TimerNS()
{
#if 0
    NStimerID = -1;
    float timeleft = calcTimeLeft(NSPulseRequest, &NSPulseStart);
    if (timeleft >= 0.000001)
    {
        if (timeleft < 0.001)
        {
            int uSecs = (int)(timeleft * 1000000.0);
            usleep(uSecs);
        }
        else
        {
            int mSecs = (int)(timeleft * 1000.0);
            NStimerID = IEAddTimer(mSecs, TOUPCAM::TimerHelperNS, this);
            return;
        }
    }
    ASIPulseGuideOff(m_camInfo->CameraID, NSDir);
    LOGF_DEBUG("Stopping %s guide.", NSDirName);
    GuideComplete(AXIS_DE);
#endif
}

/* Stop the timer for NS guiding */
void TOUPCAM::stopTimerNS()
{
#if 0
    if (NStimerID != -1)
    {
        ASIPulseGuideOff(m_camInfo->CameraID, NSDir);
        GuideComplete(AXIS_DE);
        IERmTimer(NStimerID);
        NStimerID = -1;
    }
#endif
}

IPState TOUPCAM::guidePulseNS(float ms, eGUIDEDIRECTION dir, const char *dirName)
{
#if 0
    stopTimerNS();
    NSDir = dir;
    NSDirName = dirName;

    LOGF_DEBUG("Starting %s guide for %f ms",
               NSDirName, ms);

    /*
     * If the pulse is for a ms or longer then schedule a timer callback
     * to turn off the pulse, otherwise wait here to turn it off
     */
    int mSecs = 0;
    int uSecs = 0;
    if (ms >= 1.0)
    {
        mSecs = (int)ms;
        NSPulseRequest = ms / 1000.0;
        gettimeofday(&NSPulseStart, nullptr);
    }
    else
    {
        uSecs = (int)(ms * 1000.0);
    }

    ASIPulseGuideOn(m_camInfo->CameraID, NSDir);
    if (uSecs != 0)
    {
        usleep(uSecs);
        ASIPulseGuideOff(m_camInfo->CameraID, NSDir);
        LOGF_DEBUG("Stopped %s guide.", dirName);
        return IPS_OK;
    }
    else
    {
        NStimerID = IEAddTimer(mSecs, TOUPCAM::TimerHelperNS, this);
        return IPS_BUSY;
    }
#endif
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
#if 0
    WEtimerID = -1;
    float timeleft = calcTimeLeft(WEPulseRequest, &WEPulseStart);
    if (timeleft >= 0.000001)
    {
        if (timeleft < 0.001)
        {
            int uSecs = (int)(timeleft * 1000000.0);
            usleep(uSecs);
        }
        else
        {
            int mSecs = (int)(timeleft * 1000.0);
            WEtimerID = IEAddTimer(mSecs, TOUPCAM::TimerHelperWE, this);
            return;
        }
    }
    ASIPulseGuideOff(m_camInfo->CameraID, WEDir);
    LOGF_DEBUG("Stopping %s guide.", WEDirName);
    GuideComplete(AXIS_RA);
#endif
}

void TOUPCAM::stopTimerWE()
{
#if 0
    if (WEtimerID != -1)
    {
        ASIPulseGuideOff(m_camInfo->CameraID, WEDir);
        GuideComplete(AXIS_RA);
        IERmTimer(WEtimerID);
        WEtimerID = -1;
    }
#endif
}

IPState TOUPCAM::guidePulseWE(float ms, eGUIDEDIRECTION dir, const char *dirName)
{
#if 0
    stopTimerWE();
    WEDir = dir;
    WEDirName = dirName;

    LOGF_DEBUG("Starting %s guide for %f ms",
               WEDirName, ms);

    /*
     * If the pulse is for a ms or longer then schedule a timer callback
     * to turn off the pulse, otherwise wait here to turn it off
     */
    int mSecs = 0;
    int uSecs = 0;
    if (ms >= 1.0)
    {
        mSecs = (int)ms;
        WEPulseRequest = ms / 1000.0;
        gettimeofday(&WEPulseStart, nullptr);
    }
    else
    {
        uSecs = (int)(ms * 1000.0);
    }

    ASIPulseGuideOn(m_camInfo->CameraID, WEDir);
    if (uSecs != 0)
    {
        usleep(uSecs);
        ASIPulseGuideOff(m_camInfo->CameraID, WEDir);
        LOGF_DEBUG("Stopped %s guide.", dirName);
        return IPS_OK;
    }
    else
    {
        WEtimerID = IEAddTimer(mSecs, TOUPCAM::TimerHelperWE, this);
        return IPS_BUSY;
    }
#endif
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
#if 0
    switch (m_camInfo->BayerPattern)
    {
    case ASI_BAYER_BG:
        return "BGGR";
    case ASI_BAYER_GR:
        return "GRBG";
    case ASI_BAYER_GB:
        return "GBRG";
    case ASI_BAYER_RG:
    default:
        return "RGGB";
    }
#endif
    return "RGGB";
}

TOUPCAM::ePIXELFORMAT TOUPCAM::getImageType()
{
#if 0
    ASI_IMG_TYPE type = ASI_IMG_END;

    if (VideoFormatSP.nsp > 0)
    {
        ISwitch *sp = IUFindOnSwitch(&VideoFormatSP);
        if (sp)
            type = *((ASI_IMG_TYPE *)sp->aux);
    }

    return type;
#endif
    return PIXELFORMAT_RAW8;
}

void TOUPCAM::refreshControls()
{   
    IDSetNumber(&ControlNP, nullptr);
}

void TOUPCAM::updateRecorderFormat()
{
#if 0
    currentVideoFormat = getImageType();

    switch (currentVideoFormat)
    {
    case ASI_IMG_Y8:
        Streamer->setPixelFormat(INDI_MONO);
        break;

    case ASI_IMG_RAW8:
        if (m_camInfo->BayerPattern == ASI_BAYER_RG)
            Streamer->setPixelFormat(INDI_BAYER_RGGB);
        else if (m_camInfo->BayerPattern == ASI_BAYER_BG)
            Streamer->setPixelFormat(INDI_BAYER_BGGR);
        else if (m_camInfo->BayerPattern == ASI_BAYER_GR)
            Streamer->setPixelFormat(INDI_BAYER_GRBG);
        else if (m_camInfo->BayerPattern == ASI_BAYER_GB)
            Streamer->setPixelFormat(INDI_BAYER_GBRG);
        break;

    case ASI_IMG_RAW16:
        if (m_camInfo->IsColorCam == ASI_FALSE)
            Streamer->setPixelFormat(INDI_MONO, 16);
        else if (m_camInfo->BayerPattern == ASI_BAYER_RG)
            Streamer->setPixelFormat(INDI_BAYER_RGGB, 16);
        else if (m_camInfo->BayerPattern == ASI_BAYER_BG)
            Streamer->setPixelFormat(INDI_BAYER_BGGR, 16);
        else if (m_camInfo->BayerPattern == ASI_BAYER_GR)
            Streamer->setPixelFormat(INDI_BAYER_GRBG, 16);
        else if (m_camInfo->BayerPattern == ASI_BAYER_GB)
            Streamer->setPixelFormat(INDI_BAYER_GBRG, 16);
        break;

    case ASI_IMG_RGB24:
        Streamer->setPixelFormat(INDI_RGB);
        break;

    case ASI_IMG_END:
        break;
    }
#endif
}

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

void TOUPCAM::addFITSKeywords(fitsfile *fptr, INDI::CCDChip *targetChip)
{
    INDI::CCD::addFITSKeywords(fptr, targetChip);

    INumber *gainNP = IUFindNumber(&ControlNP, "Gain");

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
    {
        IUSaveConfigNumber(fp, &CoolerNP);
        IUSaveConfigSwitch(fp, &CoolerSP);
    }

    if (ControlNP.nnp > 0)
        IUSaveConfigNumber(fp, &ControlNP);

    IUSaveConfigSwitch(fp, &VideoFormatSP);

    return true;
}

// TODO use timers from GPhoto better
float TOUPCAM::calcTimeLeft(float duration, timeval *start_time)
{
    double timesince;
    double timeleft;
    struct timeval now;

    gettimeofday(&now, nullptr);
    timesince = ((double)now.tv_sec + (double)now.tv_usec / 1000000.0) -
            ((double)start_time->tv_sec + (double)start_time->tv_usec / 1000000.0);
    if (duration > timesince)
    {
        timeleft = duration - timesince;
    }
    else
    {
        timeleft = 0.0;
    }
    return (float)timeleft;
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

void TOUPCAM::eventCB(unsigned event, void* pCtx)
{
    static_cast<TOUPCAM*>(pCtx)->eventPullCallBack(event);
}

void TOUPCAM::eventPullCallBack(unsigned event)
{
    LOGF_DEBUG("Event %#04X", event);

    switch (event)
    {
    case TOUPCAM_EVENT_EXPOSURE:
        break;
    case TOUPCAM_EVENT_TEMPTINT:
        break;
    case TOUPCAM_EVENT_IMAGE:
    {
        ToupcamFrameInfoV2 info;
        memset(&info, 0, sizeof(ToupcamFrameInfoV2));
        HRESULT rc = Toupcam_PullImageV2(m_CameraHandle, PrimaryCCD.getFrameBuffer(), 24, &info);
        if (rc < 0)
        {
            LOGF_ERROR("Failed to pull image, Error Code = %08x", rc);
            PrimaryCCD.setExposureFailed();
        }
        else
        {
            PrimaryCCD.setExposureLeft(0);
            InExposure  = false;
            ExposureComplete(&PrimaryCCD);
            LOGF_DEBUG("Image captured. Width: %d Height: %d flag: %d", info.width, info.height, info.flag);
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
            LOGF_ERROR("Failed to pull image, Error Code = %08x", rc);
            PrimaryCCD.setExposureFailed();
        }
        else
        {
            PrimaryCCD.setExposureLeft(0);
            InExposure  = false;
            ExposureComplete(&PrimaryCCD);
            LOGF_DEBUG("Image captured. Width: %d Height: %d flag: %d", info.width, info.height, info.flag);
        }
    }
        break;
    case TOUPCAM_EVENT_WBGAIN:
        break;
    case TOUPCAM_EVENT_TRIGGERFAIL:
        break;
    case TOUPCAM_EVENT_BLACK:
        break;
    case TOUPCAM_EVENT_FFC:
        break;
    case TOUPCAM_EVENT_DFC:
        break;
    case TOUPCAM_EVENT_ERROR:
        break;
    case TOUPCAM_EVENT_DISCONNECTED:
        break;
    case TOUPCAM_EVENT_TIMEOUT:
        break;
    case TOUPCAM_EVENT_FACTORY:
        break;
    default:
        break;
    }
}

void TOUPCAM::getSnapImage()
{

}
