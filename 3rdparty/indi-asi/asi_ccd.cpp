/*
 ASI CCD Driver

 Copyright (C) 2015 Jasem Mutlaq (mutlaqja@ikarustech.com)
 Copyright (C) 2018 Leonard Bottleman (leonard@whiteweasel.net)

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

#include "asi_ccd.h"

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

//#define USE_SIMULATION

static int iAvailableCamerasCount;
static ASI_CAMERA_INFO *pASICameraInfo;
static ASICCD *cameras[MAX_DEVICES];

static bool warn_roi_height = true;
static bool warn_roi_width = true;

static void cleanup()
{
    for (int i = 0; i < iAvailableCamerasCount; i++)
    {
        delete cameras[i];
    }

    if (pASICameraInfo != nullptr)
    {
        delete [] pASICameraInfo;
        pASICameraInfo = nullptr;
    }
}

void ASI_CCD_ISInit()
{
    static bool isInit = false;
    if (!isInit)
    {
        pASICameraInfo       = nullptr;
        iAvailableCamerasCount = 0;

#ifdef USE_SIMULATION
        iConnectedCamerasCount = 2;
        ppASICameraInfo      = (ASI_CAMERA_INFO *)malloc(sizeof(ASI_CAMERA_INFO *) * iConnectedCamerasCount);
        strncpy(ppASICameraInfo[0]->Name, "ASI CCD #1", 64);
        strncpy(ppASICameraInfo[1]->Name, "ASI CCD #2", 64);
        cameras[0] = new ASICCD(ppASICameraInfo[0]);
        cameras[1] = new ASICCD(ppASICameraInfo[1]);
#else
        iAvailableCamerasCount = ASIGetNumOfConnectedCameras();
        if (iAvailableCamerasCount > MAX_DEVICES)
            iAvailableCamerasCount = MAX_DEVICES;
        if (iAvailableCamerasCount <= 0)
            //Try sending IDMessage as well?
            IDLog("No ASI Cameras detected. Power on?");
        else
        {
            size_t size = sizeof(ASI_CAMERA_INFO) * iAvailableCamerasCount;
            pASICameraInfo = new ASI_CAMERA_INFO[size];
            if (pASICameraInfo == nullptr)
            {
                iAvailableCamerasCount = 0;
                IDLog("Failed to allocate memory.");
                return;
            }
            //memset(pASICameraInfo, 0, size);
            ASI_CAMERA_INFO *cameraP = pASICameraInfo;
            std::vector<std::string> cameraNames;
            for (int i = 0; i < iAvailableCamerasCount; i++)
            {
                ASIGetCameraProperty(cameraP, i);
                std::string cameraName;

                if (std::find(cameraNames.begin(), cameraNames.end(), cameraP->Name) == cameraNames.end())
                    cameraName = "ZWO CCD " + std::string(cameraP->Name + 4);
                else
                    cameraName = "ZWO CCD " + std::string(cameraP->Name + 4) + " " +
                                 std::to_string(static_cast<int>(std::count(cameraNames.begin(), cameraNames.end(), cameraP->Name)) + 1);

                cameras[i] = new ASICCD(cameraP, cameraName);
                cameraNames.push_back(cameraP->Name);
                cameraP++;
            }
        }
#endif

        atexit(cleanup);
        isInit = true;
    }
}

void ISGetProperties(const char *dev)
{
    ASI_CCD_ISInit();

    if (iAvailableCamerasCount == 0)
    {
        IDMessage(nullptr, "No ASI cameras detected. Power on?");
        return;
    }

    for (int i = 0; i < iAvailableCamerasCount; i++)
    {
        ASICCD *camera = cameras[i];
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
    ASI_CCD_ISInit();
    for (int i = 0; i < iAvailableCamerasCount; i++)
    {
        ASICCD *camera = cameras[i];
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
    ASI_CCD_ISInit();
    for (int i = 0; i < iAvailableCamerasCount; i++)
    {
        ASICCD *camera = cameras[i];
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
    ASI_CCD_ISInit();
    for (int i = 0; i < iAvailableCamerasCount; i++)
    {
        ASICCD *camera = cameras[i];
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
    ASI_CCD_ISInit();

    for (int i = 0; i < iAvailableCamerasCount; i++)
    {
        ASICCD *camera = cameras[i];
        camera->ISSnoopDevice(root);
    }
}

ASICCD::ASICCD(ASI_CAMERA_INFO *camInfo, std::string cameraName)
{
    setVersion(ASI_VERSION_MAJOR, ASI_VERSION_MINOR);
    ControlN     = nullptr;
    ControlS     = nullptr;
    pControlCaps = nullptr;
    m_camInfo    = camInfo;

    WEPulseRequest = NSPulseRequest = 0;
    genTimerID = WEtimerID = NStimerID = -1;
    NSDir = ASI_GUIDE_NORTH;
    WEDir = ASI_GUIDE_WEST;

    strncpy(this->name, cameraName.c_str(), MAXINDIDEVICE);
    setDeviceName(this->name);
}

const char *ASICCD::getDefaultName()
{
    return "ZWO CCD";
}

bool ASICCD::initProperties()
{
    INDI::CCD::initProperties();

    IUFillSwitch(&CoolerS[0], "COOLER_ON", "ON", ISS_OFF);
    IUFillSwitch(&CoolerS[1], "COOLER_OFF", "OFF", ISS_ON);
    IUFillSwitchVector(&CoolerSP, CoolerS, 2, getDeviceName(), "CCD_COOLER", "Cooler", MAIN_CONTROL_TAB, IP_WO,
                       ISR_1OFMANY, 0, IPS_IDLE);

    IUFillNumber(&CoolerN[0], "CCD_COOLER_VALUE", "Cooling Power (%)", "%+06.2f", 0., 1., .2, 0.0);
    IUFillNumberVector(&CoolerNP, CoolerN, 1, getDeviceName(), "CCD_COOLER_POWER", "Cooling Power", MAIN_CONTROL_TAB,
                       IP_RO, 60, IPS_IDLE);

    IUFillNumberVector(&ControlNP, nullptr, 0, getDeviceName(), "CCD_CONTROLS", "Controls", CONTROL_TAB, IP_RW, 60,
                       IPS_IDLE);

    IUFillSwitchVector(&ControlSP, nullptr, 0, getDeviceName(), "CCD_CONTROLS_MODE", "Set Auto", CONTROL_TAB, IP_RW,
                       ISR_NOFMANY, 60, IPS_IDLE);

    IUFillSwitchVector(&VideoFormatSP, nullptr, 0, getDeviceName(), "CCD_VIDEO_FORMAT", "Format", CONTROL_TAB, IP_RW,
                       ISR_1OFMANY, 60, IPS_IDLE);

    IUSaveText(&BayerT[2], getBayerString());

    IUFillNumber(&ADCDepthN, "BITS", "Bits", "%2.0f", 0, 32, 1, 0);
    ADCDepthN.value = m_camInfo->BitDepth;
    IUFillNumberVector(&ADCDepthNP, &ADCDepthN, 1, getDeviceName(), "ADC_DEPTH", "ADC Depth", IMAGE_INFO_TAB, IP_RO, 60,
                       IPS_IDLE);

    IUFillText(&SDKVersionS[0], "VERSION", "Version", ASIGetSDKVersion());
    IUFillTextVector(&SDKVersionSP, SDKVersionS, 1, getDeviceName(), "SDK", "SDK", INFO_TAB, IP_RO, 60, IPS_IDLE);

    int maxBin = 1;
    for (int i = 0; i < 16; i++)
    {
        if (m_camInfo->SupportedBins[i] != 0)
            maxBin = m_camInfo->SupportedBins[i];
        else
            break;
    }

    PrimaryCCD.setMinMaxStep("CCD_EXPOSURE", "CCD_EXPOSURE_VALUE", 0, 3600, 1, false);
    PrimaryCCD.setMinMaxStep("CCD_BINNING", "HOR_BIN", 1, maxBin, 1, false);
    PrimaryCCD.setMinMaxStep("CCD_BINNING", "VER_BIN", 1, maxBin, 1, false);

    uint32_t cap = 0;

    cap |= CCD_CAN_ABORT;
    if (maxBin > 1)
        cap |= CCD_CAN_BIN;

    cap |= CCD_CAN_SUBFRAME;

    if (m_camInfo->IsCoolerCam)
        cap |= CCD_HAS_COOLER;

    if (m_camInfo->MechanicalShutter)
        cap |= CCD_HAS_SHUTTER;

    if (m_camInfo->ST4Port)
        cap |= CCD_HAS_ST4_PORT;

    if (m_camInfo->IsColorCam)
        cap |= CCD_HAS_BAYER;

    cap |= CCD_HAS_STREAMING;

#ifdef HAVE_WEBSOCKET
    cap |= CCD_HAS_WEB_SOCKET;
#endif

    SetCCDCapability(cap);

    addAuxControls();

    return true;
}

bool ASICCD::updateProperties()
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

        if (ControlNP.nnp > 0)
        {
            defineNumber(&ControlNP);
            loadConfig(true, "CCD_CONTROLS");
        }

        if (ControlSP.nsp > 0)
        {
            defineSwitch(&ControlSP);
            loadConfig(true, "CCD_CONTROLS_MODE");
        }

        if (VideoFormatSP.nsp > 0)
        {
            defineSwitch(&VideoFormatSP);
            loadConfig(true, "CCD_VIDEO_FORMAT");
        }

        defineNumber(&ADCDepthNP);
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

        if (ControlNP.nnp > 0)
            deleteProperty(ControlNP.name);

        if (ControlSP.nsp > 0)
            deleteProperty(ControlSP.name);

        if (VideoFormatSP.nsp > 0)
            deleteProperty(VideoFormatSP.name);

        deleteProperty(SDKVersionSP.name);
        deleteProperty(ADCDepthNP.name);
    }

    return true;
}

bool ASICCD::Connect()
{
    LOGF_DEBUG("Attempting to open %s...", name);

    ASI_ERROR_CODE errCode = ASI_SUCCESS;

    if (isSimulation() == false)
        errCode = ASIOpenCamera(m_camInfo->CameraID);

    if (errCode != ASI_SUCCESS)
    {
        LOGF_ERROR("Error connecting to the CCD (%d)", errCode);
        return false;
    }

    if (isSimulation() == false)
        errCode = ASIInitCamera(m_camInfo->CameraID);

    if (errCode != ASI_SUCCESS)
    {
        LOGF_ERROR("Error Initializing the CCD (%d)", errCode);
        return false;
    }

    genTimerID = SetTimer(TEMP_TIMER_MS);

    /*
     * Create the imaging thread and wait for it to start
     */
    threadRequest = StateIdle;
    threadState = StateNone;
    //int stat = pthread_create(&imagingThread, nullptr, &imagingHelper, this);
    //    if (stat != 0)
    //    {
    //        LOGF_ERROR("Error creating imaging thread (%d)",
    //                   stat);
    //        return false;
    //    }
    imagingThread = std::thread(&ASICCD::imagingThreadEntry, this);
    waitUntil(StateIdle);
    //pthread_mutex_lock(&condMutex);
    //    while (threadState == StateNone)
    //    {
    //        pthread_cond_wait(&cv, &condMutex);
    //    }
    //    pthread_mutex_unlock(&condMutex);

    LOG_INFO("Setting intital bandwidth to AUTO on connection.");
    if ((errCode = ASISetControlValue(m_camInfo->CameraID, ASI_BANDWIDTHOVERLOAD, 40, ASI_FALSE)) != ASI_SUCCESS)
    {
        LOGF_ERROR("Failed to set initial bandwidth: error (%d)", errCode);
    }
    /* Success! */
    LOG_INFO("CCD is online. Retrieving basic data.");

    return true;
}

bool ASICCD::Disconnect()
{
    //ImageState  tState;
    LOGF_DEBUG("Closing %s...", name);

    stopTimerNS();
    stopTimerWE();
    RemoveTimer(genTimerID);
    genTimerID = -1;

    //pthread_mutex_lock(&condMutex);
    //    {
    //        std::unique_lock<std::mutex> lock(condMutex);
    //        //tState = threadState;
    //        threadRequest = StateTerminate;
    //    }
    //    cv.notify_one();

    setThreadRequest(StateTerminate);
    //    pthread_cond_signal(&cv);
    //    pthread_mutex_unlock(&condMutex);
    //    pthread_join(imagingThread, nullptr);

    imagingThread.join();
    if (isSimulation() == false)
    {
        ASIStopVideoCapture(m_camInfo->CameraID);
        ASIStopExposure(m_camInfo->CameraID);
        ASICloseCamera(m_camInfo->CameraID);
    }

    LOG_INFO("Camera is offline.");

    return true;
}

void ASICCD::setupParams()
{
    int piNumberOfControls = 0;
    ASI_ERROR_CODE errCode = ASIGetNumOfControls(m_camInfo->CameraID, &piNumberOfControls);

    if (errCode != ASI_SUCCESS)
        LOGF_DEBUG("ASIGetNumOfControls error (%d)", errCode);

    if (ControlNP.nnp > 0)
    {
        free(ControlN);
        ControlNP.nnp = 0;
    }

    if (ControlSP.nsp > 0)
    {
        free(ControlS);
        ControlSP.nsp = 0;
    }

    if (piNumberOfControls > 0)
    {
        createControls(piNumberOfControls);
    }

    if (HasCooler())
    {
        ASI_CONTROL_CAPS pCtrlCaps;
        errCode = ASIGetControlCaps(m_camInfo->CameraID, ASI_TARGET_TEMP,
                                    &pCtrlCaps);
        if (errCode == ASI_SUCCESS)
        {
            CoolerN[0].min = pCtrlCaps.MinValue;
            CoolerN[0].max = pCtrlCaps.MaxValue;
            CoolerN[0].value = pCtrlCaps.DefaultValue;
        }
        //defineNumber(&CoolerNP);
        //defineSwitch(&CoolerSP);
    }

    // Set minimum ASI_BANDWIDTHOVERLOAD on ARM
#ifdef LOW_USB_BANDWIDTH
    ASI_CONTROL_CAPS pCtrlCaps;
    for (int j = 0; j < piNumberOfControls; j++)
    {
        ASIGetControlCaps(m_camInfo->CameraID, j, &pCtrlCaps);
        if (pCtrlCaps.ControlType == ASI_BANDWIDTHOVERLOAD)
        {
            LOGF_DEBUG("setupParams->set USB %d", pCtrlCaps.MinValue);
            ASISetControlValue(m_camInfo->CameraID, ASI_BANDWIDTHOVERLOAD, pCtrlCaps.MinValue, ASI_FALSE);
            break;
        }
    }
#endif

    // Get Image Format
    int w = 0, h = 0, bin = 0;
    ASI_IMG_TYPE imgType;
    ASIGetROIFormat(m_camInfo->CameraID, &w, &h, &bin, &imgType);

    LOGF_DEBUG("CCD ID: %d Width: %d Height: %d Binning: %dx%d Image Type: %d",
               m_camInfo->CameraID, w, h, bin, bin, imgType);

    // Get video format and bit depth
    int bit_depth = 8;

    switch (imgType)
    {
        case ASI_IMG_RAW16:
            bit_depth = 16;
            break;

        default:
            break;
    }

    if (VideoFormatSP.nsp > 0)
    {
        free(VideoFormatS);
        VideoFormatSP.nsp = 0;
    }

    VideoFormatS      = nullptr;
    int nVideoFormats = 0;

    for (int i = 0; i < 8; i++)
    {
        if (m_camInfo->SupportedVideoFormat[i] == ASI_IMG_END)
            break;
        nVideoFormats++;
    }
    size_t size = sizeof(ISwitch) * nVideoFormats;
    VideoFormatS = static_cast<ISwitch *>(malloc(size));
    if (VideoFormatS == nullptr)
    {
        LOGF_ERROR("Camera ID: %d malloc failed (setup)",  m_camInfo->CameraID);
        VideoFormatSP.nsp = 0;
        return;
    }
    (void)memset(VideoFormatS, 0, size);
    ISwitch *oneVF = VideoFormatS;
    int unknownCount = 0;
    bool unknown = false;
    for (int i = 0; i < nVideoFormats; i++)
    {
        switch (m_camInfo->SupportedVideoFormat[i])
        {
            case ASI_IMG_RAW8:
                IUFillSwitch(oneVF, "ASI_IMG_RAW8", "Raw 8 bit", (imgType == ASI_IMG_RAW8) ? ISS_ON : ISS_OFF);
                LOG_DEBUG("Supported Video Format: ASI_IMG_RAW8");
                break;

            case ASI_IMG_RGB24:
                IUFillSwitch(oneVF, "ASI_IMG_RGB24", "RGB 24", (imgType == ASI_IMG_RGB24) ? ISS_ON : ISS_OFF);
                LOG_DEBUG("Supported Video Format: ASI_IMG_RGB24");
                break;

            case ASI_IMG_RAW16:
                IUFillSwitch(oneVF, "ASI_IMG_RAW16", "Raw 16 bit", (imgType == ASI_IMG_RAW16) ? ISS_ON : ISS_OFF);
                LOG_DEBUG("Supported Video Format: ASI_IMG_RAW16");
                break;

            case ASI_IMG_Y8:
                IUFillSwitch(oneVF, "ASI_IMG_Y8", "Luma", (imgType == ASI_IMG_Y8) ? ISS_ON : ISS_OFF);
                LOG_DEBUG("Supported Video Format: ASI_IMG_Y8");
                break;

            default:
                unknown = true;
                unknownCount++;
                LOGF_DEBUG("Unknown video format (%d)", m_camInfo->SupportedVideoFormat[i]);
                break;
        }

        if (unknown == false)
        {
            oneVF->aux = &m_camInfo->SupportedVideoFormat[i];
            oneVF++;
        }
        unknown = false;
    }
    nVideoFormats -= unknownCount;

    VideoFormatSP.nsp = nVideoFormats;
    VideoFormatSP.sp  = VideoFormatS;
    rememberVideoFormat = IUFindOnSwitchIndex(&VideoFormatSP);

    float x_pixel_size, y_pixel_size;

    x_pixel_size = m_camInfo->PixelSize;
    y_pixel_size = m_camInfo->PixelSize;

    uint32_t maxWidth = m_camInfo->MaxWidth;
    uint32_t maxHeight = m_camInfo->MaxHeight;

#if 0
    // JM 2019-04-22
    // We need to restrict width to width % 8 = 0
    // and height to height % 2 = 0;
    // Check this thread for discussion:
    // https://www.indilib.org/forum/ccds-dslrs/4956-indi-asi-driver-bug-causes-false-binned-images.html
    int maxBin = 1;
    for (int i = 0; i < 16; i++)
    {
        if (m_camInfo->SupportedBins[i] != 0)
            maxBin = m_camInfo->SupportedBins[i];
        else
            break;
    }

    maxWidth  = ((maxWidth / maxBin) - ((maxWidth / maxBin) % 8)) * maxBin;
    maxHeight = ((maxHeight / maxBin) - ((maxHeight / maxBin) % 2)) * maxBin;
#endif

    SetCCDParams(maxWidth, maxHeight, bit_depth, x_pixel_size, y_pixel_size);

    // Let's calculate required buffer
    int nbuf = PrimaryCCD.getXRes() * PrimaryCCD.getYRes() * PrimaryCCD.getBPP() / 8;
    PrimaryCCD.setFrameBufferSize(nbuf);

    //if (HasCooler())
    //{
    long pValue     = 0;
    ASI_BOOL isAuto = ASI_FALSE;

    if ((errCode = ASIGetControlValue(m_camInfo->CameraID, ASI_TEMPERATURE, &pValue, &isAuto)) != ASI_SUCCESS)
        LOGF_DEBUG("ASIGetControlValue temperature error (%d)", errCode);

    TemperatureN[0].value = pValue / 10.0;

    LOGF_INFO("The CCD Temperature is %.3f", TemperatureN[0].value);
    IDSetNumber(&TemperatureNP, nullptr);
    //}

    ASIStopVideoCapture(m_camInfo->CameraID);

    LOGF_DEBUG("setupParams ASISetROIFormat (%dx%d,  bin %d, type %d)", maxWidth,
               maxHeight, 1, imgType);
    ASISetROIFormat(m_camInfo->CameraID, maxWidth, maxHeight, 1, imgType);

    updateRecorderFormat();
    Streamer->setSize(maxWidth, maxHeight);
}

bool ASICCD::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    ASI_ERROR_CODE errCode = ASI_SUCCESS;

    if (dev != nullptr && !strcmp(dev, getDeviceName()))
    {
        if (!strcmp(name, ControlNP.name))
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
                ASI_BOOL nAuto         = *(static_cast<ASI_BOOL *>(ControlN[i].aux1));
                ASI_CONTROL_TYPE nType = *(static_cast<ASI_CONTROL_TYPE *>(ControlN[i].aux0));

                if (fabs(ControlN[i].value - oldValues[i]) < 0.01)
                    continue;

                LOGF_DEBUG("Setting %s --> %.2f", ControlN[i].label, ControlN[i].value);
                if ((errCode = ASISetControlValue(m_camInfo->CameraID, nType, static_cast<long>(ControlN[i].value), ASI_FALSE)) !=
                        ASI_SUCCESS)
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
                        ASI_CONTROL_TYPE swType = *(static_cast<ASI_CONTROL_TYPE *>(ControlS[j].aux));

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

    return INDI::CCD::ISNewNumber(dev, name, values, names, n);
}

bool ASICCD::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    ASI_ERROR_CODE errCode = ASI_SUCCESS;

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
                ASI_CONTROL_TYPE swType = *(static_cast<ASI_CONTROL_TYPE *>(ControlS[i].aux));
                ASI_BOOL swAuto         = (ControlS[i].s == ISS_ON) ? ASI_TRUE : ASI_FALSE;

                for (int j = 0; j < ControlNP.nnp; j++)
                {
                    ASI_CONTROL_TYPE nType = *(static_cast<ASI_CONTROL_TYPE *>(ControlN[j].aux0));

                    if (swType == nType)
                    {
                        LOGF_DEBUG("Setting %s --> %.2f", ControlN[j].label, ControlN[j].value);
                        if ((errCode = ASISetControlValue(m_camInfo->CameraID, nType, ControlN[j].value, swAuto)) !=
                                ASI_SUCCESS)
                        {
                            LOGF_ERROR("ASISetControlValue (%s=%g) error (%d)", ControlN[j].name,
                                       ControlN[j].value, errCode);
                            ControlNP.s = IPS_ALERT;
                            ControlSP.s = IPS_ALERT;
                            IDSetNumber(&ControlNP, nullptr);
                            IDSetSwitch(&ControlSP, nullptr);
                            return false;
                        }

                        *(static_cast<ASI_BOOL *>(ControlN[j].aux1)) = swAuto;
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
            int targetIndex = -1;
            for (int i = 0; i < VideoFormatSP.nsp; i++)
            {
                if (!strcmp(targetFormat, VideoFormatS[i].name))
                {
                    targetIndex = i;
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

    return INDI::CCD::ISNewSwitch(dev, name, states, names, n);
}

bool ASICCD::setVideoFormat(uint8_t index)
{
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

    // When changing video format, reset frame
    UpdateCCDFrame(0, 0, PrimaryCCD.getXRes(), PrimaryCCD.getYRes());

    updateRecorderFormat();

    VideoFormatSP.s = IPS_OK;
    IDSetSwitch(&VideoFormatSP, nullptr);

    return true;
}

bool ASICCD::StartStreaming()
{
#if 0
    ASI_IMG_TYPE type = getImageType();

    rememberVideoFormat = IUFindOnSwitchIndex(&VideoFormatSP);

    if (type != ASI_IMG_Y8 && type != ASI_IMG_RGB24)
    {
        IUResetSwitch(&VideoFormatSP);
        ISwitch *vf = IUFindSwitch(&VideoFormatSP, "ASI_IMG_Y8");
        if (vf == nullptr)
            vf = IUFindSwitch(&VideoFormatSP, "ASI_IMG_RAW8");

        if (vf)
        {
            vf->s = ISS_ON;
            LOGF_DEBUG("Switching to %s video format.", vf->label);
            PrimaryCCD.setBPP(8);
            UpdateCCDFrame(PrimaryCCD.getSubX(), PrimaryCCD.getSubY(), PrimaryCCD.getSubW(), PrimaryCCD.getSubH());
            IDSetSwitch(&VideoFormatSP, nullptr);
        }
        else
        {
            LOG_ERROR("No 8 bit video format found, cannot start stream.");
            return false;
        }
    }
#endif

    ExposureRequest = 1.0 / Streamer->getTargetFPS();
    long uSecs = static_cast<long>(ExposureRequest * 950000.0);
    ASISetControlValue(m_camInfo->CameraID, ASI_EXPOSURE, uSecs, ASI_FALSE);
    ASIStartVideoCapture(m_camInfo->CameraID);

    //    {
    //        std::lock_guard<std::mutex> lock(condMutex);
    //        threadRequest = StateStream;
    //    }
    //    cv.notify_one();

    setThreadRequest(StateStream);

    //    pthread_cond_signal(&cv);
    //    pthread_mutex_unlock(&condMutex);

    return true;
}

bool ASICCD::StopStreaming()
{
    // Wait until ccd lock is acquired

    //    {
    //        std::lock_guard<std::mutex> lock(condMutex);
    //        threadRequest = StateAbort;
    //    }
    //    cv.notify_one();

    setThreadRequest(StateAbort);

    //    pthread_mutex_lock(&condMutex);
    //    threadRequest = StateAbort;
    //    pthread_cond_signal(&cv);

    //    while (threadState == StateStream)
    //    {
    //        pthread_cond_wait(&cv, &condMutex);
    //    }
    //    pthread_mutex_unlock(&condMutex);
    ASIStopVideoCapture(m_camInfo->CameraID);

    //if (IUFindOnSwitchIndex(&VideoFormatSP) != rememberVideoFormat)
    //setVideoFormat(rememberVideoFormat);

    return true;
}

int ASICCD::SetTemperature(double temperature)
{
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
        tVal = static_cast<long>(temperature + 0.49);
    else if (temperature  < 0.5)
        tVal = static_cast<long>(temperature - 0.49);
    else
        tVal = 0;
    if (ASISetControlValue(m_camInfo->CameraID, ASI_TARGET_TEMP, tVal, ASI_TRUE) != ASI_SUCCESS)
    {
        LOG_ERROR("Failed to set temperature!");
        return -1;
    }

    // Otherwise, we set the temperature request and we update the status in TimerHit() function.
    TemperatureRequest = temperature;
    LOGF_INFO("Setting CCD temperature to %+06.2f C", temperature);
    return 0;
}

bool ASICCD::activateCooler(bool enable)
{
    bool rc = (ASISetControlValue(m_camInfo->CameraID, ASI_COOLER_ON, enable ? ASI_TRUE : ASI_FALSE, ASI_FALSE) ==
               ASI_SUCCESS);
    if (rc == false)
        CoolerSP.s = IPS_ALERT;
    else
    {
        CoolerS[0].s = enable ? ISS_ON : ISS_OFF;
        CoolerS[1].s = enable ? ISS_OFF : ISS_ON;
        CoolerSP.s   = enable ? IPS_BUSY : IPS_IDLE;
    }
    IDSetSwitch(&CoolerSP, nullptr);

    return rc;
}

bool ASICCD::StartExposure(float duration)
{
    ASI_ERROR_CODE errCode = ASI_SUCCESS;

    PrimaryCCD.setExposureDuration(duration);
    ExposureRequest = duration;

    LOGF_DEBUG("StartExposure->setexp : %.3fs", duration);
    long uSecs = duration * 1000000.0;
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
                ASI_SUCCESS)
        {
            LOGF_ERROR("ASIStartExposure error (%d)", errCode);
            // Wait 100ms before trying again
            usleep(100000);
            continue;
        }
        break;
    }

    if (errCode != ASI_SUCCESS)
    {
        LOG_WARN("ASI firmware might require an update to *compatible mode. Check http://www.indilib.org/devices/ccds/zwo-optics-asi-cameras.html for details.");
        return false;
    }

    gettimeofday(&ExpStart, nullptr);
    if (ExposureRequest > VERBOSE_EXPOSURE)
        LOGF_INFO("Taking a %g seconds frame...", ExposureRequest);

    InExposure = true;

    //    pthread_mutex_lock(&condMutex);
    //    threadRequest = StateExposure;
    //    pthread_cond_signal(&cv);
    //    pthread_mutex_unlock(&condMutex);

    setThreadRequest(StateExposure);

    //updateControls();

    return true;
}

bool ASICCD::AbortExposure()
{
    LOG_DEBUG("Aborting camera exposure...");
    //    pthread_mutex_lock(&condMutex);
    //    threadRequest = StateAbort;
    //    pthread_cond_signal(&cv);
    //    while (threadState == StateExposure)
    //    {
    //        pthread_cond_wait(&cv, &condMutex);
    //    }
    //    pthread_mutex_unlock(&condMutex);

    setThreadRequest(StateAbort);
    waitUntil(StateIdle);

    ASIStopExposure(m_camInfo->CameraID);
    InExposure = false;
    return true;
}

bool ASICCD::UpdateCCDFrame(int x, int y, int w, int h)
{

    uint32_t binX = PrimaryCCD.getBinX();
    uint32_t binY = PrimaryCCD.getBinY();
    uint32_t subX = x / binX;
    uint32_t subY = y / binY;
    uint32_t subW = w / binX;
    uint32_t subH = h / binY;

    if (subW > static_cast<uint32_t>(PrimaryCCD.getXRes() / binX))
    {
        LOGF_INFO("Error: invalid width requested %d", w);
        return false;
    }
    if (subH > static_cast<uint32_t>(PrimaryCCD.getYRes() / binY))
    {
        LOGF_INFO("Error: invalid height request %d", h);
        return false;
    }

    m_SubX = subX;
    m_SubY = subY;
    m_SubW = subW;
    m_SubH = subH;

    // ZWO rules are this: width%8 = 0, height%2 = 0
    // if this condition is not met, we set it internally to slightly smaller values

    if (warn_roi_width && subW % 8 > 0)
    {
        LOGF_INFO ("Incompatible frame width %dpx. Reducing by %dpx.", subW, subW % 8);
        warn_roi_width = false;
    }
    if (warn_roi_height && subH % 2 > 0)
    {
        LOGF_INFO ("Incompatible frame height %dpx. Reducing by %dpx.", subH, subH % 2);
        warn_roi_height = false;
    }

    subW -= subW % 8;
    subH -= subH % 2;

    LOGF_DEBUG("CCD Frame ROI x:%d y:%d w:%d h:%d", subX, subY, subW, subH);

    int rc = ASI_SUCCESS;

    if ((rc = ASISetROIFormat(m_camInfo->CameraID, subW, subH, binX, getImageType())) !=
            ASI_SUCCESS)
    {
        LOGF_ERROR("ASISetROIFormat error (%d)", rc);
        return false;
    }

    if ((rc = ASISetStartPos(m_camInfo->CameraID, subX, subY)) != ASI_SUCCESS)
    {
        LOGF_ERROR("ASISetStartPos error (%d)", rc);
        return false;
    }

    // Set UNBINNED coords
    //PrimaryCCD.setFrame(x, y, w, h);
    PrimaryCCD.setFrame(subX * binX, subY * binY, subW * binX, subH * binY);

    // Total bytes required for image buffer
    uint32_t nbuf = (subW * subH * static_cast<uint32_t>(PrimaryCCD.getBPP()) / 8) * ((getImageType() == ASI_IMG_RGB24) ? 3 : 1);

    LOGF_DEBUG("Setting frame buffer size to %d bytes.", nbuf);
    PrimaryCCD.setFrameBufferSize(nbuf);

    // Always set BINNED size
    Streamer->setSize(subW, subH);

    return true;
}

bool ASICCD::UpdateCCDBin(int binx, int biny)
{
    INDI_UNUSED(biny);

    PrimaryCCD.setBin(binx, binx);

    return UpdateCCDFrame(PrimaryCCD.getSubX(), PrimaryCCD.getSubY(), PrimaryCCD.getSubW(), PrimaryCCD.getSubH());
}

float ASICCD::calcTimeLeft(float duration, timeval *start_time)
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
    return timeleft;
}

/* Downloads the image from the CCD.
 N.B. No processing is done on the image */
int ASICCD::grabImage()
{
    ASI_ERROR_CODE errCode = ASI_SUCCESS;

    ASI_IMG_TYPE type = getImageType();

    std::unique_lock<std::mutex> guard(ccdBufferLock);
    uint8_t *image = PrimaryCCD.getFrameBuffer();
    uint8_t *buffer = image;

    uint16_t subW = PrimaryCCD.getSubW() / PrimaryCCD.getBinX();
    uint16_t subH = PrimaryCCD.getSubH() / PrimaryCCD.getBinY();
    int nChannels = (type == ASI_IMG_RGB24) ? 3 : 1;
    size_t nTotalBytes = subW * subH * nChannels * (PrimaryCCD.getBPP() / 8);

    if (type == ASI_IMG_RGB24)
    {
        buffer = static_cast<uint8_t *>(malloc(nTotalBytes));
        if (buffer == nullptr)
        {
            LOGF_ERROR("%s: %d malloc failed (RGB 24)", getDeviceName());
            return -1;
        }
    }

    if ((errCode = ASIGetDataAfterExp(m_camInfo->CameraID, buffer, nTotalBytes)) != ASI_SUCCESS)
    {
        LOGF_ERROR("ASIGetDataAfterExp (%dx%d #%d channels) error (%d)", subW, subH, nChannels,
                   errCode);
        if (type == ASI_IMG_RGB24)
            free(buffer);
        return -1;
    }

    if (type == ASI_IMG_RGB24)
    {
        uint8_t *subR = image;
        uint8_t *subG = image + subW * subH;
        uint8_t *subB = image + subW * subH * 2;
        uint32_t nPixels = subW * subH * 3 - 3;

        for (uint32_t i = 0; i <= nPixels; i += 3)
        {
            *subB++ = buffer[i];
            *subG++ = buffer[i + 1];
            *subR++ = buffer[i + 2];
        }

        free(buffer);
    }
    guard.unlock();

    if (type == ASI_IMG_RGB24)
        PrimaryCCD.setNAxis(3);
    else
        PrimaryCCD.setNAxis(2);

    // If mono camera or we're sending Luma or RGB, turn off bayering
    if (m_camInfo->IsColorCam == false || type == ASI_IMG_Y8 || type == ASI_IMG_RGB24)
        SetCCDCapability(GetCCDCapability() & ~CCD_HAS_BAYER);
    else
        SetCCDCapability(GetCCDCapability() | CCD_HAS_BAYER);

    if (ExposureRequest > VERBOSE_EXPOSURE)
        LOG_INFO("Download complete.");

    ExposureComplete(&PrimaryCCD);
    return 0;
}

/* The generic timer call back is used for temperature monitoring */
void ASICCD::TimerHit()
{
    long ASIControlValue = 0;
    ASI_BOOL ASIControlAuto = ASI_FALSE;
    double currentTemperature = TemperatureN[0].value;

    ASI_ERROR_CODE errCode = ASIGetControlValue(m_camInfo->CameraID,
                             ASI_TEMPERATURE, &ASIControlValue, &ASIControlAuto);
    if (errCode != ASI_SUCCESS)
    {
        LOGF_ERROR(
            "ASIGetControlValue ASI_TEMPERATURE error (%d)", errCode);
        TemperatureNP.s = IPS_ALERT;
    }
    else
    {
        TemperatureN[0].value = ASIControlValue / 10.0;
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
        if (errCode != ASI_SUCCESS)
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
}

/* Helper function for NS timer call back */
void ASICCD::TimerHelperNS(void *context)
{
    static_cast<ASICCD *>(context)->TimerNS();
}

/* The timer call back for NS guiding */
void ASICCD::TimerNS()
{
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
            NStimerID = IEAddTimer(mSecs, ASICCD::TimerHelperNS, this);
            return;
        }
    }
    ASIPulseGuideOff(m_camInfo->CameraID, NSDir);
    LOGF_DEBUG("Stopping %s guide.", NSDirName);
    GuideComplete(AXIS_DE);
}

/* Stop the timer for NS guiding */
void ASICCD::stopTimerNS()
{
    if (NStimerID != -1)
    {
        ASIPulseGuideOff(m_camInfo->CameraID, NSDir);
        GuideComplete(AXIS_DE);
        IERmTimer(NStimerID);
        NStimerID = -1;
    }
}

IPState ASICCD::guidePulseNS(float ms, ASI_GUIDE_DIRECTION dir,
                             const char *dirName)
{
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
        NStimerID = IEAddTimer(mSecs, ASICCD::TimerHelperNS, this);
        return IPS_BUSY;
    }
}

IPState ASICCD::GuideNorth(uint32_t ms)
{
    return guidePulseNS(ms, ASI_GUIDE_NORTH, "North");
}

IPState ASICCD::GuideSouth(uint32_t ms)
{
    return guidePulseNS(ms, ASI_GUIDE_SOUTH, "South");
}

/* Helper function for WE timer call back */
void ASICCD::TimerHelperWE(void *context)
{
    static_cast<ASICCD *>(context)->TimerWE();
}

/* The timer call back for WE guiding */
void ASICCD::TimerWE()
{
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
            WEtimerID = IEAddTimer(mSecs, ASICCD::TimerHelperWE, this);
            return;
        }
    }
    ASIPulseGuideOff(m_camInfo->CameraID, WEDir);
    LOGF_DEBUG("Stopping %s guide.", WEDirName);
    GuideComplete(AXIS_RA);
}

void ASICCD::stopTimerWE()
{
    if (WEtimerID != -1)
    {
        ASIPulseGuideOff(m_camInfo->CameraID, WEDir);
        GuideComplete(AXIS_RA);
        IERmTimer(WEtimerID);
        WEtimerID = -1;
    }
}

IPState ASICCD::guidePulseWE(float ms, ASI_GUIDE_DIRECTION dir,
                             const char *dirName)
{
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
        WEtimerID = IEAddTimer(mSecs, ASICCD::TimerHelperWE, this);
        return IPS_BUSY;
    }
}

IPState ASICCD::GuideEast(uint32_t ms)
{
    return guidePulseWE(ms, ASI_GUIDE_EAST, "East");
}

IPState ASICCD::GuideWest(uint32_t ms)
{
    return guidePulseWE(ms, ASI_GUIDE_WEST, "West");
}

void ASICCD::createControls(int piNumberOfControls)
{
    ASI_ERROR_CODE errCode = ASI_SUCCESS;

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
        if ((errCode = ASIGetControlCaps(m_camInfo->CameraID, i, oneControlCap)) != ASI_SUCCESS)
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
            double step = 1;
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
}

const char *ASICCD::getBayerString()
{
    switch (m_camInfo->BayerPattern)
    {
        case ASI_BAYER_BG:
            return "BGGR";
        case ASI_BAYER_GR:
            return "GRBG";
        case ASI_BAYER_GB:
            return "GBRG";
        default:
            return "RGGB";
    }
}

ASI_IMG_TYPE ASICCD::getImageType()
{
    ASI_IMG_TYPE type = ASI_IMG_END;

    if (VideoFormatSP.nsp > 0)
    {
        ISwitch *sp = IUFindOnSwitch(&VideoFormatSP);
        if (sp)
            type = *(static_cast<ASI_IMG_TYPE *>(sp->aux));
    }

    return type;
}

void ASICCD::updateControls()
{
    long pValue     = 0;
    ASI_BOOL isAuto = ASI_FALSE;

    for (int i = 0; i < ControlNP.nnp; i++)
    {
        ASI_CONTROL_TYPE nType = *(static_cast<ASI_CONTROL_TYPE *>(ControlN[i].aux0));

        ASIGetControlValue(m_camInfo->CameraID, nType, &pValue, &isAuto);

        ControlN[i].value = pValue;

        for (int j = 0; j < ControlSP.nsp; j++)
        {
            ASI_CONTROL_TYPE sType = *(static_cast<ASI_CONTROL_TYPE *>(ControlS[j].aux));

            if (sType == nType)
            {
                ControlS[j].s = (isAuto == ASI_TRUE) ? ISS_ON : ISS_OFF;
                break;
            }
        }
    }

    IDSetNumber(&ControlNP, nullptr);
    IDSetSwitch(&ControlSP, nullptr);
}

void ASICCD::updateRecorderFormat()
{
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
}

void *ASICCD::imagingHelper(void *context)
{
    return static_cast<ASICCD *>(context)->imagingThreadEntry();
}

/*
 * A dedicated thread is used for handling streaming video and image
 * exposures because the operations take too much time to be done
 * as part of a timer call-back: there is one timer for the entire
 * process, which must handle events for all ASI cameras
 */
void *ASICCD::imagingThreadEntry()
{
    //pthread_mutex_lock(&condMutex);
    {
        std::lock_guard<std::mutex> lock(condMutex);
        threadState = StateIdle;
    }
    cv.notify_one();
    //pthread_cond_signal(&cv);
    while (true)
    {
        std::unique_lock<std::mutex> lock(condMutex);
        cv.wait(lock, [this] {return threadRequest != StateIdle;});
        //        while (threadRequest == StateIdle)
        //        {
        //            pthread_cond_wait(&cv, &condMutex);
        //        }
        threadState = threadRequest;
        if (threadRequest == StateExposure)
        {
            lock.unlock();
            getExposure();
            lock.lock();
        }
        else if (threadRequest == StateStream)
        {
            lock.unlock();
            streamVideo();
            lock.lock();
        }
        else if (threadRequest == StateRestartExposure)
        {
            threadRequest = StateIdle;
            //pthread_mutex_unlock(&condMutex);
            lock.unlock();
            StartExposure(ExposureRequest);
            lock.lock();
            //pthread_mutex_lock(&condMutex);
        }
        else if (threadRequest == StateTerminate)
        {
            break;
        }
        else
        {
            threadRequest = StateIdle;
            cv.notify_one();
            //pthread_cond_signal(&cv);
        }
        threadState = StateIdle;

        //lock.unlock();
    }

    {
        std::lock_guard<std::mutex> lock(condMutex);
        threadState = StateTerminated;
    }
    cv.notify_one();


    //pthread_cond_signal(&cv);
    //pthread_mutex_unlock(&condMutex);

    return nullptr;
}

void ASICCD::setThreadRequest(ImageState request)
{
    std::lock_guard<std::mutex> lock(condMutex);

    threadRequest = request;

    cv.notify_one();
}

void ASICCD::waitUntil(ImageState request)
{
    std::unique_lock<std::mutex> lock(condMutex);
    cv.wait(lock, [this, request] {return threadState == request;});
}

void ASICCD::streamVideo()
{
    //int ret;
    //int frames = 0;

    std::unique_lock<std::mutex> lock(condMutex);

    while (threadRequest == StateStream)
    {
        //pthread_mutex_unlock(&condMutex);
        lock.unlock();

        std::unique_lock<std::mutex> guard(ccdBufferLock);
        uint8_t *targetFrame = PrimaryCCD.getFrameBuffer();
        uint32_t totalBytes  = PrimaryCCD.getFrameBufferSize();
        int waitMS           = static_cast<int>((ExposureRequest * 2000.0) + 500);

        int ret = ASIGetVideoData(m_camInfo->CameraID, targetFrame, totalBytes, waitMS);
        if (ret != ASI_SUCCESS)
        {
            if (ret != ASI_ERROR_TIMEOUT)
            {
                Streamer->setStream(false);
                //pthread_mutex_lock(&condMutex);
                lock.lock();
                if (threadRequest == StateStream)
                {
                    LOGF_ERROR("Error reading video data (%d)", ret);
                    exposureSetRequest(StateIdle);
                }
                break;
            }
            else
            {
                //frames = 0;
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
            guard.unlock();

            Streamer->newFrame(targetFrame, totalBytes);

            //            /*
            //             * Release the CPU every 30 frames
            //             */
            //            frames++;
            //            if (frames == 30)
            //            {
            //                frames = 0;
            //                usleep(10);
            //            }
        }

        //pthread_mutex_lock(&condMutex);
        lock.lock();
    }
}

void ASICCD::getExposure()
{
    int expRetry = 0;
    int statRetry = 0;
    int uSecs = 1000000;
    ASI_EXPOSURE_STATUS status = ASI_EXP_IDLE;
    ASI_ERROR_CODE errCode;

    //    pthread_mutex_unlock(&condMutex);
    //    usleep(10000);
    //    pthread_mutex_lock(&condMutex);

    std::unique_lock<std::mutex> lock(condMutex);

    while (threadRequest == StateExposure)
    {
        //pthread_mutex_unlock(&condMutex);
        lock.unlock();

        errCode = ASIGetExpStatus(m_camInfo->CameraID, &status);
        if (errCode == ASI_SUCCESS)
        {
            if (status == ASI_EXP_SUCCESS)
            {
                InExposure = false;
                PrimaryCCD.setExposureLeft(0.0);
                if (PrimaryCCD.getExposureDuration() > 3)
                    LOG_INFO("Exposure done, downloading image...");
                //pthread_mutex_lock(&condMutex);
                lock.lock();
                exposureSetRequest(StateIdle);
                lock.unlock();
                //pthread_mutex_unlock(&condMutex);
                grabImage();
                //pthread_mutex_lock(&condMutex);
                lock.lock();
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
                    //pthread_mutex_lock(&condMutex);
                    lock.lock();
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
                    //pthread_mutex_lock(&condMutex);
                    lock.lock();
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
                //pthread_mutex_lock(&condMutex);
                lock.lock();
                exposureSetRequest(StateIdle);
                break;
            }
        }

        /*
         * Check the status every second until the time left is
         * about one second, after which decrease the poll interval
         */
        double timeLeft = calcTimeLeft(ExposureRequest, &ExpStart);
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

        //pthread_mutex_lock(&condMutex);
        lock.lock();
    }
}

/* Caller must hold the mutex */
void ASICCD::exposureSetRequest(ImageState request)
{
    if (threadRequest == StateExposure)
    {
        threadRequest = request;
    }
}

void ASICCD::addFITSKeywords(fitsfile *fptr, INDI::CCDChip *targetChip)
{
    INDI::CCD::addFITSKeywords(fptr, targetChip);

    INumber *gainNP = IUFindNumber(&ControlNP, "Gain");

    if (gainNP)
    {
        int status = 0;
        fits_update_key_s(fptr, TDOUBLE, "Gain", &(gainNP->value), "Gain", &status);
    }
}

bool ASICCD::saveConfigItems(FILE *fp)
{
    INDI::CCD::saveConfigItems(fp);

    if (HasCooler())
        IUSaveConfigSwitch(fp, &CoolerSP);

    if (ControlNP.nnp > 0)
        IUSaveConfigNumber(fp, &ControlNP);

    if (ControlSP.nsp > 0)
        IUSaveConfigSwitch(fp, &ControlSP);

    IUSaveConfigSwitch(fp, &VideoFormatSP);

    return true;
}
