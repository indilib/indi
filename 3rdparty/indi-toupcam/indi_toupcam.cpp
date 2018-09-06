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

static int iConnectedCamerasCount;
static ToupcamInstV2 pToupCameraInfo[TOUPCAM_MAX];
static TOUPCAM *cameras[TOUPCAM_MAX];

//#define USE_SIMULATION

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

    IUFillSwitch(&CoolerS[0], "COOLER_ON", "ON", ISS_OFF);
    IUFillSwitch(&CoolerS[1], "COOLER_OFF", "OFF", ISS_ON);
    IUFillSwitchVector(&CoolerSP, CoolerS, 2, getDeviceName(), "CCD_COOLER", "Cooler", MAIN_CONTROL_TAB, IP_WO,
                       ISR_1OFMANY, 0, IPS_IDLE);

    IUFillNumber(&CoolerN[0], "CCD_COOLER_VALUE", "Cooling Power (%)", "%+06.2f", 0., 1., .2, 0.0);
    IUFillNumberVector(&CoolerNP, CoolerN, 1, getDeviceName(), "CCD_COOLER_POWER", "Cooling Power", MAIN_CONTROL_TAB,
                       IP_RO, 60, IPS_IDLE);

    IUFillNumber(&ControlN[TC_CONTRAST], "TC_CONTRAST", "Contrast", "%.f", -100.0, 100, 10, 0);
    IUFillNumber(&ControlN[TC_HUE], "TC_HUE", "Hue", "%.f", -180.0, 180, 10, 0);
    IUFillNumber(&ControlN[TC_SATURATION], "TC_SATURATION", "Saturation", "%.f", 0, 255, 10, 128);
    IUFillNumber(&ControlN[TC_BRIGHTNESS], "TC_BRIGHTNESS", "Brightness", "%.f", -64, 64, 8, 0);
    IUFillNumber(&ControlN[TC_GAMMA], "TC_GAMMA", "Gamma", "%.f", 20, 180, 10, 100);
    IUFillNumber(&ControlN[TC_LEVEL_RANGE], "TC_LEVEL_RANGE", "Level Range", "%.f", 0, 255, 10, 0);
    IUFillNumber(&ControlN[TC_BLACK_LEVEL], "TC_BLACK_LEVEL", "Black Level", "%.f", 0, 65536, 0, 0);
    IUFillNumberVector(&ControlNP, nullptr, 6, getDeviceName(), "CCD_CONTROLS", "Controls", CONTROL_TAB, IP_RW, 60,
                       IPS_IDLE);


    IUFillSwitch(&AutoControlS[TC_AUTO_EXPOSURE], "TC_AUTO_EXPOSURE", "Exposure", ISS_OFF);
    IUFillSwitch(&AutoControlS[TC_AUTO_LEVEL], "TC_AUTO_LEVEL", "Level", ISS_OFF);
    IUFillSwitch(&AutoControlS[TC_AUTO_TINT], "TC_AUTO_TINT", "WB Tint", ISS_OFF);
    IUFillSwitch(&AutoControlS[TC_AUTO_WB], "TC_AUTO_WB", "WB RGB", ISS_OFF);
    IUFillSwitchVector(&AutoControlSP, AutoControlS, 4, getDeviceName(), "CCD_AUTO_CONTROL", "Auto", CONTROL_TAB, IP_RW,
                       ISR_NOFMANY, 0, IPS_IDLE);


    IUFillSwitch(&VideoFormatS[TC_VIDEO_MONO], "TC_VIDEO_MONO", "Mono", ISS_OFF);
    IUFillSwitch(&VideoFormatS[TC_VIDEO_COLOR], "TC_VIDEO_COLOR", "Color", ISS_ON);
    IUFillSwitch(&VideoFormatS[TC_VIDEO_RAW], "TC_VIDEO_RAW", "Raw", ISS_OFF);
    IUFillSwitchVector(&VideoFormatSP, nullptr, 3, getDeviceName(), "CCD_VIDEO_FORMAT", "Format", CONTROL_TAB, IP_RW,
                       ISR_1OFMANY, 60, IPS_IDLE);

    IUFillSwitchVector(&ResolutionSP, nullptr, 0, getDeviceName(), "CCD_RESOLUTION", "Resolution", CONTROL_TAB, IP_RW,
                       ISR_1OFMANY, 60, IPS_IDLE);

    IUSaveText(&BayerT[2], getBayerString());

//    IUFillNumber(&ADCDepthN, "BITS", "Bits", "%2.0f", 0, 32, 1, 0);
//    ADCDepthN.value = m_camInfo->BitDepth;
//    IUFillNumberVector(&ADCDepthNP, &ADCDepthN, 1, getDeviceName(), "ADC_DEPTH", "ADC Depth", IMAGE_INFO_TAB, IP_RO, 60,
//                       IPS_IDLE);

    IUFillText(&SDKVersionS[0], "VERSION", "Version", Toupcam_Version());
    IUFillTextVector(&SDKVersionSP, SDKVersionS, 1, getDeviceName(), "SDK", "SDK", INFO_TAB, IP_RO, 60, IPS_IDLE);    

    PrimaryCCD.setMinMaxStep("CCD_EXPOSURE", "CCD_EXPOSURE_VALUE", 0, 3600, 1, false);
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
        else
        {
            TemperatureNP.p = IP_RO;
            defineNumber(&TemperatureNP);
        }

        defineNumber(&ControlNP);
        defineSwitch(&AutoControlSP);
        defineSwitch(&VideoFormatSP);
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
        deleteProperty(SDKVersionSP.name);        
    }

    return true;
}

bool TOUPCAM::Connect()
{
    LOGF_DEBUG("Attempting to open %s...", name);

    if (isSimulation() == false)
        m_CameraHandle = Toupcam_Open(m_Instance->id);

    if (m_CameraHandle)
    {
        LOG_ERROR("Error connecting to the camera");
        return false;
    }

    uint32_t cap = 0;

    cap |= CCD_CAN_ABORT;

    if (m_Instance->model->flag & TOUPCAM_FLAG_BINSKIP_SUPPORTED)
        cap |= CCD_CAN_BIN;

    // Hardware ROI really needed? Check later
    if (m_Instance->model->flag & TOUPCAM_FLAG_ROI_HARDWARE)
        cap |= CCD_CAN_SUBFRAME;

    if (m_Instance->model->flag & TOUPCAM_FLAG_TEC_ONOFF)
        cap |= CCD_HAS_COOLER;

    if (m_Instance->model->flag & TOUPCAM_FLAG_ST4)
        cap |= CCD_HAS_ST4_PORT;

    cap |= CCD_HAS_STREAMING;

    SetCCDCapability(cap);

    /*
     * Create the imaging thread and wait for it to start
     * N.B. Do we really need this with ToupCam?
     */
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
    uint8_t bitsPerPixel = 8;
    /* bitdepth supported */
    if (m_Instance->model->flag & (TOUPCAM_FLAG_RAW10 | TOUPCAM_FLAG_RAW12 | TOUPCAM_FLAG_RAW14 | TOUPCAM_FLAG_RAW16))
    {
       /* enable bitdepth */
       Toupcam_put_Option(m_CameraHandle, TOUPCAM_OPTION_BITDEPTH, 1);
       bitsPerPixel = 16;
    }

    // Get how many resolutions available for the camera
    ResolutionSP.nsp = Toupcam_get_ResolutionNumber(m_CameraHandle);

    int w[TOUPCAM_MAX]={0},h[TOUPCAM_MAX]={0};
    // Get each resolution width x height
    for (uint8_t i=0; i < ResolutionSP.nsp; i++)
    {
      Toupcam_get_Resolution(m_CameraHandle, i, &w[i], &h[i]);
      char label[MAXINDILABEL] = {0};
      snprintf(label, MAXINDILABEL, "%d x %d", w[i], h[i]);
      IUFillSwitch(&ResolutionS[i], label, label, ISS_OFF);
    }

    // Get active resolution index
    uint32_t currentResolutionIndex = 0;
    Toupcam_get_eSize(m_CameraHandle, &currentResolutionIndex);
    ResolutionS[currentResolutionIndex].s = ISS_ON;

    SetCCDParams(w[currentResolutionIndex], h[currentResolutionIndex], bitsPerPixel, m_Instance->model->xpixsz, m_Instance->model->ypixsz);


    // Get Resolution
    // Get Bit Depth
    // Get Video Mode
    // Get Mono/Color
    // Get Pixel Size
    // Get Temperature
    // Get Cooler

    // Allocate memory
    PrimaryCCD.setFrameBufferSize(PrimaryCCD.getXRes() * PrimaryCCD.getYRes() * PrimaryCCD.getBPP() / 8);

    // Set Streamer pixel format
    Streamer->setPixelFormat(INDI_MONO, bitsPerPixel);
    Streamer->setSize(PrimaryCCD.getXRes(), PrimaryCCD.getYRes());
}

bool TOUPCAM::allocateFrameBuffer()
{

    return true;
}

bool TOUPCAM::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
#if 0
    int errCode = 0;

    if (dev != nullptr && !strcmp(dev, getDeviceName()))
    {
        if (!strcmp(name, ControlNP.name))
        {
            double oldValues[ControlNP.nnp];
            for (int i = 0; i < ControlNP.nnp; i++)
                oldValues[i] = ControlN[i].value;

            if (IUUpdateNumber(&ControlNP, values, names, n) < 0)
            {
                ControlNP.s = IPS_ALERT;
                IDSetNumber(&ControlNP, nullptr);
                return true;
            }

            for (int i = 0; i < ControlNP.nnp; i++)
            {
                ASI_BOOL nAuto         = *((ASI_BOOL *)ControlN[i].aux1);
                ASI_CONTROL_TYPE nType = *((ASI_CONTROL_TYPE *)ControlN[i].aux0);

                if (ControlN[i].value == oldValues[i])
                    continue;

                LOGF_DEBUG("ISNewNumber->set ctrl %d: %.2f", nType, ControlN[i].value);
                if ((errCode = ASISetControlValue(m_camInfo->CameraID, nType, (long)ControlN[i].value, ASI_FALSE)) !=
                    0)
                {
                    LOGF_ERROR("ASISetControlValue (%s=%g) error (%d)", ControlN[i].name,
                           ControlN[i].value, errCode);
                    ControlNP.s = IPS_ALERT;
                    for (int i = 0; i < ControlNP.nnp; i++)
                        ControlN[i].value = oldValues[i];
                    IDSetNumber(&ControlNP, nullptr);
                    return false;
                }

                // If it was set to nAuto value to turn it off
                if (nAuto)
                {
                    for (int j = 0; j < ControlSP.nsp; j++)
                    {
                        ASI_CONTROL_TYPE swType = *((ASI_CONTROL_TYPE *)ControlS[j].aux);

                        if (swType == nType)
                        {
                            ControlS[j].s = ISS_OFF;
                            break;
                        }
                    }

                    IDSetSwitch(&ControlSP, nullptr);
                }
            }

            ControlNP.s = IPS_OK;
            IDSetNumber(&ControlNP, nullptr);
            return true;
        }
    }
#endif
    return INDI::CCD::ISNewNumber(dev, name, values, names, n);
}

bool TOUPCAM::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
#if 0
    int errCode = 0;

    if (dev != nullptr && !strcmp(dev, getDeviceName()))
    {
        if (!strcmp(name, ControlSP.name))
        {
            if (IUUpdateSwitch(&ControlSP, states, names, n) < 0)
            {
                ControlSP.s = IPS_ALERT;
                IDSetSwitch(&ControlSP, nullptr);
                return true;
            }

            for (int i = 0; i < ControlSP.nsp; i++)
            {
                ASI_CONTROL_TYPE swType = *((ASI_CONTROL_TYPE *)ControlS[i].aux);
                ASI_BOOL swAuto         = (ControlS[i].s == ISS_ON) ? ASI_TRUE : ASI_FALSE;

                for (int j = 0; j < ControlNP.nnp; j++)
                {
                    ASI_CONTROL_TYPE nType = *((ASI_CONTROL_TYPE *)ControlN[j].aux0);

                    if (swType == nType)
                    {
                        LOGF_DEBUG("ISNewSwitch->SetControlValue %d %.2f", nType,
                               ControlN[j].value);
                        if ((errCode = ASISetControlValue(m_camInfo->CameraID, nType, ControlN[j].value, swAuto)) !=
                            0)
                        {
                            LOGF_ERROR("ASISetControlValue (%s=%g) error (%d)", ControlN[j].name,
                                   ControlN[j].value, errCode);
                            ControlNP.s = IPS_ALERT;
                            ControlSP.s = IPS_ALERT;
                            IDSetNumber(&ControlNP, nullptr);
                            IDSetSwitch(&ControlSP, nullptr);
                            return false;
                        }

                        *((ASI_BOOL *)ControlN[j].aux1) = swAuto;
                        break;
                    }
                }
            }

            ControlSP.s = IPS_OK;
            IDSetSwitch(&ControlSP, nullptr);
            return true;
        }

        /* Cooler */
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
    }
#endif

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
#if 0
    int errCode = 0;

    PrimaryCCD.setExposureDuration(duration);
    ExposureRequest = duration;

    LOGF_DEBUG("StartExposure->setexp : %.3fs", duration);
    long uSecs = (long)(duration * 1000000.0);
    ASISetControlValue(m_camInfo->CameraID, ASI_EXPOSURE, uSecs, ASI_FALSE);

    // Try exposure for 3 times
    ASI_BOOL isDark = ASI_FALSE;
    if (PrimaryCCD.getFrameType() == INDI::CCDChip::DARK_FRAME)
    {
      isDark = ASI_TRUE;
    }
    for (int i = 0; i < 3; i++)
    {
        if ((errCode = ASIStartExposure(m_camInfo->CameraID, isDark)) !=
            0)
        {
            LOGF_ERROR("ASIStartExposure error (%d)", errCode);
            // Wait 100ms before trying again
            usleep(100000);
            continue;
        }
        break;
    }

    if (errCode != 0)
    {
        LOG_WARN("ASI firmware might require an update to *compatible mode. Check http://www.indilib.org/devices/ccds/zwo-optics-asi-cameras.html for details.");
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

    //updateControls();
#endif
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

    PrimaryCCD.setBin(binx, binx);

    return UpdateCCDFrame(PrimaryCCD.getSubX(), PrimaryCCD.getSubY(), PrimaryCCD.getSubW(), PrimaryCCD.getSubH());
}

/* Downloads the image from the CCD.
 N.B. No processing is done on the image */
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

/* The generic timer call back is used for temperature monitoring */
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
    ((TOUPCAM *)context)->TimerNS();
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

void TOUPCAM::createControls(int piNumberOfControls)
{
#if 0
    int errCode = 0;

    INumber *control_number;
    INumber *control_np;
    int nWritableControls   = 0;

    ISwitch *auto_switch;
    ISwitch *auto_sp;
    int nAutoSwitches    = 0;
    size_t size;

    if (pControlCaps != nullptr)
        free(pControlCaps);

    size = sizeof(ASI_CONTROL_CAPS) * piNumberOfControls;
    pControlCaps = (ASI_CONTROL_CAPS *)malloc(size);
    if (pControlCaps == nullptr)
    {
        LOGF_ERROR("CCD ID: %d malloc failed (controls)",
            m_camInfo->CameraID);
        return;
    }
    (void)memset(pControlCaps, 0, size);
    ASI_CONTROL_CAPS *oneControlCap = pControlCaps;

    if (ControlNP.nnp != 0)
    {
        free(ControlNP.np);
        ControlNP.nnp = 0;
    }

    size = sizeof(INumber) * piNumberOfControls;
    control_number = (INumber *)malloc(size);
    if (control_number == nullptr)
    {
        LOGF_ERROR(
            "CCD ID: %d malloc failed (control number)", m_camInfo->CameraID);
        free(pControlCaps);
        pControlCaps = nullptr;
        return;
    }
    (void)memset(control_number, 0, size);
    control_np = control_number;

    if (ControlSP.nsp != 0)
    {
        free(ControlSP.sp);
        ControlSP.nsp = 0;
    }

    size = sizeof(ISwitch) * piNumberOfControls;
    auto_switch = (ISwitch *)malloc(size);
    if (auto_switch == nullptr)
    {
        LOGF_ERROR(
            "CCD ID: %d malloc failed (control auto)", m_camInfo->CameraID);
        free(control_number);
        free(pControlCaps);
        pControlCaps = nullptr;
        return;
    }
    (void)memset(auto_switch, 0, size);
    auto_sp = auto_switch;

    for (int i = 0; i < piNumberOfControls; i++)
    {
        if ((errCode = ASIGetControlCaps(m_camInfo->CameraID, i, oneControlCap)) != 0)
        {
            LOGF_ERROR("ASIGetControlCaps error (%d)", errCode);
            return;
        }

        LOGF_DEBUG("Control #%d: name (%s), Descp (%s), Min (%ld), Max (%ld), Default Value (%ld), IsAutoSupported (%s), "
               "isWritale (%s) ",
               i, oneControlCap->Name, oneControlCap->Description, oneControlCap->MinValue, oneControlCap->MaxValue,
               oneControlCap->DefaultValue, oneControlCap->IsAutoSupported ? "True" : "False",
               oneControlCap->IsWritable ? "True" : "False");

        if (oneControlCap->IsWritable == ASI_FALSE || oneControlCap->ControlType == ASI_TARGET_TEMP ||
            oneControlCap->ControlType == ASI_COOLER_ON)
            continue;

        // Update Min/Max exposure as supported by the camera
        if (oneControlCap->ControlType == ASI_EXPOSURE)
        {
            double minExp = (double)oneControlCap->MinValue / 1000000.0;
            double maxExp = (double)oneControlCap->MaxValue / 1000000.0;
            PrimaryCCD.setMinMaxStep("CCD_EXPOSURE", "CCD_EXPOSURE_VALUE",
                minExp, maxExp, 1);
            continue;
        }

        long pValue     = 0;
        ASI_BOOL isAuto = ASI_FALSE;

#ifdef LOW_USB_BANDWIDTH
        if (oneControlCap->ControlType == ASI_BANDWIDTHOVERLOAD)
        {
            LOGF_DEBUG("createControls->set USB %d", oneControlCap->MinValue);
            ASISetControlValue(m_camInfo->CameraID, oneControlCap->ControlType, oneControlCap->MinValue, ASI_FALSE);
        }
#else
        if (oneControlCap->ControlType == ASI_BANDWIDTHOVERLOAD)
        {
            if (m_camInfo->IsUSB3Camera && !m_camInfo->IsUSB3Host)
            {
                LOGF_DEBUG("createControls->set USB %d", 0.8 * oneControlCap->MaxValue);
                ASISetControlValue(m_camInfo->CameraID, oneControlCap->ControlType, 0.8 * oneControlCap->MaxValue,
                                   ASI_FALSE);
            }
            else
            {
                LOGF_DEBUG("createControls->set USB %d", oneControlCap->MinValue);
                ASISetControlValue(m_camInfo->CameraID, oneControlCap->ControlType, oneControlCap->MinValue, ASI_FALSE);
            }
        }
#endif

        ASIGetControlValue(m_camInfo->CameraID, oneControlCap->ControlType, &pValue, &isAuto);

        if (oneControlCap->IsWritable)
        {
            nWritableControls++;

            LOGF_DEBUG(
                "Adding above control as writable control number %d",
                nWritableControls);

            // JM 2018-07-04: If Max-Min == 1 then it's boolean value
            // So no need to set a custom step value.
            double step=1;
            if (oneControlCap->MaxValue - oneControlCap->MinValue > 1)
                step = (oneControlCap->MaxValue - oneControlCap->MinValue) / 10.0;
            IUFillNumber(control_np,
                         oneControlCap->Name,
                         oneControlCap->Name,
                        "%g",
                         oneControlCap->MinValue,
                         oneControlCap->MaxValue,
                         step,
                        pValue);
            control_np->aux0 = (void *)&oneControlCap->ControlType;
            control_np->aux1 = (void *)&oneControlCap->IsAutoSupported;
            control_np++;
        }

        if (oneControlCap->IsAutoSupported)
        {
            nAutoSwitches++;

            LOGF_DEBUG("Adding above control as auto control number %d", nAutoSwitches);

            char autoName[MAXINDINAME];
            snprintf(autoName, MAXINDINAME, "AUTO_%s", oneControlCap->Name);
            IUFillSwitch(auto_sp, autoName, oneControlCap->Name,
                isAuto == ASI_TRUE ? ISS_ON : ISS_OFF);
            auto_sp->aux = (void *)&oneControlCap->ControlType;
            auto_sp++;
        }

        oneControlCap++;
    }

    /*
     * Resize the buffers to free up unused space -- there is no need to
     * check for realloc() failures because we are reducing the size
     */
    if (nWritableControls != piNumberOfControls)
    {
        size = sizeof(INumber) * nWritableControls;
        control_number = (INumber *)realloc(control_number, size);
    }
    if (nAutoSwitches != piNumberOfControls)
    {
        size = sizeof(ISwitch) * nAutoSwitches;
        auto_switch = (ISwitch *)realloc(auto_switch, size);
    }

    ControlNP.nnp = nWritableControls;
    ControlNP.np  = control_number;
    ControlN      = control_number;

    ControlSP.nsp = nAutoSwitches;
    ControlSP.sp  = auto_switch;
    ControlS      = auto_switch;
#endif
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

void TOUPCAM::updateControls()
{
#if 0
    long pValue     = 0;
    ASI_BOOL isAuto = ASI_FALSE;

    for (int i = 0; i < ControlNP.nnp; i++)
    {
        ASI_CONTROL_TYPE nType = *((ASI_CONTROL_TYPE *)ControlN[i].aux0);

        ASIGetControlValue(m_camInfo->CameraID, nType, &pValue, &isAuto);

        ControlN[i].value = pValue;

        for (int j = 0; j < ControlSP.nsp; j++)
        {
            ASI_CONTROL_TYPE sType = *((ASI_CONTROL_TYPE *)ControlS[j].aux);

            if (sType == nType)
            {
                ControlS[j].s = (isAuto == ASI_TRUE) ? ISS_ON : ISS_OFF;
                break;
            }
        }
    }

    IDSetNumber(&ControlNP, nullptr);
    IDSetSwitch(&ControlSP, nullptr);
#endif
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

void TOUPCAM::streamVideo()
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

void TOUPCAM::getExposure()
{
#if 0
    float timeLeft;
    int expRetry = 0;
    int statRetry = 0;
    int uSecs = 1000000;
    ASI_EXPOSURE_STATUS status = ASI_EXP_IDLE;
    int errCode;

    pthread_mutex_unlock(&condMutex);
    usleep(10000);
    pthread_mutex_lock(&condMutex);

    while (threadRequest == StateExposure)
    {
        pthread_mutex_unlock(&condMutex);
        errCode = ASIGetExpStatus(m_camInfo->CameraID, &status);
        if (errCode == 0)
        {
            if (status == ASI_EXP_SUCCESS)
            {
                InExposure = false;
                PrimaryCCD.setExposureLeft(0.0);
                DEBUG(INDI::Logger::DBG_SESSION,
                    "Exposure done, downloading image...");
                pthread_mutex_lock(&condMutex);
                exposureSetRequest(StateIdle);
                pthread_mutex_unlock(&condMutex);
                grabImage();
                pthread_mutex_lock(&condMutex);
                break;
            }
            else if (status == ASI_EXP_FAILED)
            {
                if (++expRetry < MAX_EXP_RETRIES)
                {
                    if (threadRequest == StateExposure)
                    {
                        LOG_DEBUG("ASIGetExpStatus failed. Restarting exposure...");
                    }
                    InExposure = false;
                    ASIStopExposure(m_camInfo->CameraID);
                    usleep(100000);
                    pthread_mutex_lock(&condMutex);
                    exposureSetRequest(StateRestartExposure);
                    break;
                }
                else
                {
                    if (threadRequest == StateExposure)
                    {
                        LOGF_ERROR(
                            "Exposure failed after %d attempts.", expRetry);
                    }
                    ASIStopExposure(m_camInfo->CameraID);
                    PrimaryCCD.setExposureFailed();
                    usleep(100000);
                    pthread_mutex_lock(&condMutex);
                    exposureSetRequest(StateIdle);
                    break;
                }
            }
        }
        else
        {
            LOGF_DEBUG("ASIGetExpStatus error (%d)",
                errCode);
            if (++statRetry >= 10)
            {
                if (threadRequest == StateExposure)
                {
                    LOGF_ERROR(
                        "Exposure status timed out (%d)", errCode);
                }
                PrimaryCCD.setExposureFailed();
                InExposure = false;
                pthread_mutex_lock(&condMutex);
                exposureSetRequest(StateIdle);
                break;
            }
        }

        /*
         * Check the status every second until the time left is
         * about one second, after which decrease the poll interval
         */
        timeLeft = calcTimeLeft(ExposureRequest, &ExpStart);
        if (timeLeft > 1.1)
        {
            /*
             * For expsoures with more than a second left try
             * to keep the displayed "exposure left" value at
             * a full second boundary, which keeps the
             * count down neat
             */
            float fraction = timeLeft - (float)((int)timeLeft);
            if (fraction >= 0.005)
            {
                uSecs = (int)(fraction * 1000000.0);
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
