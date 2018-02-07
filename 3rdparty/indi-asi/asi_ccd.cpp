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

#include <math.h>
#include <unistd.h>

#define MAX_EXP_RETRIES         3
#define VERBOSE_EXPOSURE        3
#define TEMP_TIMER_MS           1000 /* Temperature polling time (ms) */
#define TEMP_THRESHOLD          .25  /* Differential temperature threshold (C)*/
#define MAX_DEVICES             4    /* Max device cameraCount */

#define CONTROL_TAB "Controls"

//#define USE_SIMULATION

static int iConnectedCamerasCount;
static ASI_CAMERA_INFO *pASICameraInfo;
static ASICCD *cameras[MAX_DEVICES];

//pthread_cond_t cv         = PTHREAD_COND_INITIALIZER;
//pthread_mutex_t condMutex = PTHREAD_MUTEX_INITIALIZER;

static void cleanup()
{
    for (int i = 0; i < iConnectedCamerasCount; i++)
    {
        delete cameras[i];
    }

    if (pASICameraInfo != nullptr)
    {
        free(pASICameraInfo);
        pASICameraInfo = nullptr;
    }
}

void ISInit()
{
    static bool isInit = false;
    if (!isInit)
    {
        pASICameraInfo       = nullptr;
        iConnectedCamerasCount = 0;

#ifdef USE_SIMULATION
        iConnectedCamerasCount = 2;
        ppASICameraInfo      = (ASI_CAMERA_INFO *)malloc(sizeof(ASI_CAMERA_INFO *) * iConnectedCamerasCount);
        strncpy(ppASICameraInfo[0]->Name, "ASI CCD #1", 64);
        strncpy(ppASICameraInfo[1]->Name, "ASI CCD #2", 64);
        cameras[0] = new ASICCD(ppASICameraInfo[0]);
        cameras[1] = new ASICCD(ppASICameraInfo[1]);
#else
        iConnectedCamerasCount = ASIGetNumOfConnectedCameras();
        if (iConnectedCamerasCount > MAX_DEVICES)
            iConnectedCamerasCount = MAX_DEVICES;
        if (iConnectedCamerasCount <= 0)
            //Try sending IDMessage as well?
            IDLog("No ASI Cameras detected. Power on?");
        else
        {
            size_t size = sizeof(ASI_CAMERA_INFO) * iConnectedCamerasCount;
            pASICameraInfo = (ASI_CAMERA_INFO *)malloc(size);
            if (pASICameraInfo == nullptr)
            {
                iConnectedCamerasCount = 0;
                IDLog("malloc failed (init)");
                return;
            }
            (void)memset(pASICameraInfo, 0, size);
            ASI_CAMERA_INFO *cameraP = pASICameraInfo;
            for (int i = 0; i < iConnectedCamerasCount; i++)
            {
                ASIGetCameraProperty(cameraP, i);
                cameras[i] = new ASICCD(cameraP);
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
    ISInit();

    if (iConnectedCamerasCount == 0)
    {
        IDMessage(nullptr, "No ASI cameras detected. Power on?");
        return;
    }

    for (int i = 0; i < iConnectedCamerasCount; i++)
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
    ISInit();
    for (int i = 0; i < iConnectedCamerasCount; i++)
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
    ISInit();
    for (int i = 0; i < iConnectedCamerasCount; i++)
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
    ISInit();
    for (int i = 0; i < iConnectedCamerasCount; i++)
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
    ISInit();

    for (int i = 0; i < iConnectedCamerasCount; i++)
    {
        ASICCD *camera = cameras[i];
        camera->ISSnoopDevice(root);
    }
}

ASICCD::ASICCD(ASI_CAMERA_INFO *camInfo)
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

    snprintf(this->name, MAXINDIDEVICE, "ZWO CCD %s", m_camInfo->Name + 4);
    setDeviceName(this->name);
}

ASICCD::~ASICCD()
{
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
    }

    return true;
}

bool ASICCD::Connect()
{
    DEBUGF(INDI::Logger::DBG_DEBUG, "Attempting to open %s...", name);

    sim = isSimulation();

    ASI_ERROR_CODE errCode = ASI_SUCCESS;

    if (sim == false)
        errCode = ASIOpenCamera(m_camInfo->CameraID);

    if (errCode != ASI_SUCCESS)
    {
        DEBUGF(INDI::Logger::DBG_ERROR, "Error connecting to the CCD (%d)", errCode);
        return false;
    }

    if (sim == false)
        errCode = ASIInitCamera(m_camInfo->CameraID);

    if (errCode != ASI_SUCCESS)
    {
        DEBUGF(INDI::Logger::DBG_ERROR, "Error Initializing the CCD (%d)", errCode);
        return false;
    }

    genTimerID = SetTimer(TEMP_TIMER_MS);

    /*
     * Create the imaging thread and wait for it to start
     */
    threadRequest = StateIdle;
    threadState = StateNone;
    int stat = pthread_create(&imagingThread, nullptr, &imagingHelper, this);
    if (stat != 0)
    {
        DEBUGF(INDI::Logger::DBG_ERROR, "Error creating imaging thread (%d)",
            stat);
        return false;
    }
    pthread_mutex_lock(&condMutex);
    while (threadState == StateNone)
    {
        pthread_cond_wait(&cv, &condMutex);
    }
    pthread_mutex_unlock(&condMutex);

    DEBUG(INDI::Logger::DBG_SESSION, "Setting intital bandwidth to AUTO on connection.");
    if ((errCode = ASISetControlValue(m_camInfo->CameraID, ASI_BANDWIDTHOVERLOAD, 40, ASI_FALSE)) != ASI_SUCCESS)
    {
        DEBUGF(INDI::Logger::DBG_ERROR, "Failed to set initial bandwidth: error (%d)", errCode);
    }
    /* Success! */
    DEBUG(INDI::Logger::DBG_SESSION, "CCD is online. Retrieving basic data.");

    return true;
}

bool ASICCD::Disconnect()
{
    ImageState  tState;
    DEBUGF(INDI::Logger::DBG_DEBUG, "Closing %s...", name);

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
    if (sim == false)
    {
        if (tState == StateStream)
            ASIStopVideoCapture(m_camInfo->CameraID);
        else if (tState == StateExposure)
            ASIStopExposure(m_camInfo->CameraID);
        ASICloseCamera(m_camInfo->CameraID);
    }

    DEBUG(INDI::Logger::DBG_SESSION, "CCD is offline.");

    return true;
}

void ASICCD::setupParams()
{
    ASI_ERROR_CODE errCode = ASI_SUCCESS;
    int piNumberOfControls = 0;

    errCode = ASIGetNumOfControls(m_camInfo->CameraID, &piNumberOfControls);

    if (errCode != ASI_SUCCESS)
        DEBUGF(INDI::Logger::DBG_DEBUG, "ASIGetNumOfControls error (%d)", errCode);

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
            CoolerN[0].min = (double)pCtrlCaps.MinValue;
            CoolerN[0].max = (double)pCtrlCaps.MaxValue;
            CoolerN[0].value = (double)pCtrlCaps.DefaultValue;
        }
        defineNumber(&CoolerNP);
        defineSwitch(&CoolerSP);
    }

// Set minimum ASI_BANDWIDTHOVERLOAD on ARM
#ifdef LOW_USB_BANDWIDTH
    ASI_CONTROL_CAPS pCtrlCaps;
    for (int j = 0; j < piNumberOfControls; j++)
    {
        ASIGetControlCaps(m_camInfo->CameraID, j, &pCtrlCaps);
        if (pCtrlCaps.ControlType == ASI_BANDWIDTHOVERLOAD)
        {
            DEBUGF(INDI::Logger::DBG_DEBUG, "setupParams->set USB %d", pCtrlCaps.MinValue);
            ASISetControlValue(m_camInfo->CameraID, ASI_BANDWIDTHOVERLOAD, pCtrlCaps.MinValue, ASI_FALSE);
            break;
        }
    }
#endif

    // Get Image Format
    int w = 0, h = 0, bin = 0;
    ASI_IMG_TYPE imgType;
    ASIGetROIFormat(m_camInfo->CameraID, &w, &h, &bin, &imgType);

    DEBUGF(INDI::Logger::DBG_DEBUG, "CCD ID: %d Width: %d Height: %d Binning: %dx%d Image Type: %d",
           m_camInfo->CameraID, w, h, bin, bin, imgType);

    // Get video format and bit depth
    int bit_depth = 8;

    switch (imgType)
    {
        case ASI_IMG_RAW16:
            bit_depth = 16;

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
    VideoFormatS = (ISwitch *)malloc(size);
    if (VideoFormatS == nullptr)
    {
        DEBUGF(INDI::Logger::DBG_ERROR, "CCD ID: %d malloc failed (setup)",
            m_camInfo->CameraID);
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
                DEBUG(INDI::Logger::DBG_DEBUG, "Supported Video Format: ASI_IMG_RAW8");
                break;

            case ASI_IMG_RGB24:
                IUFillSwitch(oneVF, "ASI_IMG_RGB24", "RGB 24", (imgType == ASI_IMG_RGB24) ? ISS_ON : ISS_OFF);
                DEBUG(INDI::Logger::DBG_DEBUG, "Supported Video Format: ASI_IMG_RGB24");
                break;

            case ASI_IMG_RAW16:
                IUFillSwitch(oneVF, "ASI_IMG_RAW16", "Raw 16 bit", (imgType == ASI_IMG_RAW16) ? ISS_ON : ISS_OFF);
                DEBUG(INDI::Logger::DBG_DEBUG, "Supported Video Format: ASI_IMG_RAW16");
                break;

            case ASI_IMG_Y8:
                IUFillSwitch(oneVF, "ASI_IMG_Y8", "Luma", (imgType == ASI_IMG_Y8) ? ISS_ON : ISS_OFF);
                DEBUG(INDI::Logger::DBG_DEBUG, "Supported Video Format: ASI_IMG_Y8");
                break;

            default:
                unknown = true;
                unknownCount++;
                DEBUGF(INDI::Logger::DBG_DEBUG, "Unknown video format (%d)", m_camInfo->SupportedVideoFormat[i]);
                break;
        }

        if (unknown == false)
        {
            oneVF->aux = (void *)&m_camInfo->SupportedVideoFormat[i];
            oneVF++;
        }
        unknown = false;
    }
    nVideoFormats -= unknownCount;

    VideoFormatSP.nsp = nVideoFormats;
    VideoFormatSP.sp  = VideoFormatS;
    rememberVideoFormat = IUFindOnSwitchIndex(&VideoFormatSP);

    float x_pixel_size, y_pixel_size;
    int x_1, y_1, x_2, y_2;

    x_pixel_size = m_camInfo->PixelSize;
    y_pixel_size = m_camInfo->PixelSize;

    x_1 = y_1 = 0;
    x_2       = m_camInfo->MaxWidth;
    y_2       = m_camInfo->MaxHeight;

    SetCCDParams(x_2 - x_1, y_2 - y_1, bit_depth, x_pixel_size, y_pixel_size);

    // Let's calculate required buffer
    int nbuf;
    nbuf = PrimaryCCD.getXRes() * PrimaryCCD.getYRes() * PrimaryCCD.getBPP() / 8; //  this is pixel cameraCount
    //nbuf += 512;    //  leave a little extra at the end
    PrimaryCCD.setFrameBufferSize(nbuf);

    //if (HasCooler())
    //{
    long pValue     = 0;
    ASI_BOOL isAuto = ASI_FALSE;

    if ((errCode = ASIGetControlValue(m_camInfo->CameraID, ASI_TEMPERATURE, &pValue, &isAuto)) != ASI_SUCCESS)
        DEBUGF(INDI::Logger::DBG_DEBUG, "ASIGetControlValue temperature error (%d)", errCode);

    TemperatureN[0].value = (double)pValue / 10.0;

    DEBUGF(INDI::Logger::DBG_SESSION, "The CCD Temperature is %f", TemperatureN[0].value);
    IDSetNumber(&TemperatureNP, nullptr);
    //}

    ASIStopVideoCapture(m_camInfo->CameraID);

    DEBUGF(INDI::Logger::DBG_DEBUG, "setupParams ASISetROIFormat (%dx%d,  bin %d, type %d)", m_camInfo->MaxWidth,
           m_camInfo->MaxHeight, 1, imgType);
    ASISetROIFormat(m_camInfo->CameraID, m_camInfo->MaxWidth, m_camInfo->MaxHeight, 1, imgType);

    updateRecorderFormat();
    Streamer->setSize(w, h);
}

bool ASICCD::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    ASI_ERROR_CODE errCode = ASI_SUCCESS;

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

                DEBUGF(INDI::Logger::DBG_DEBUG, "ISNewNumber->set ctrl %d: %.2f", nType, ControlN[i].value);
                if ((errCode = ASISetControlValue(m_camInfo->CameraID, nType, (long)ControlN[i].value, ASI_FALSE)) !=
                    ASI_SUCCESS)
                {
                    DEBUGF(INDI::Logger::DBG_ERROR, "ASISetControlValue (%s=%g) error (%d)", ControlN[i].name,
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
                ASI_CONTROL_TYPE swType = *((ASI_CONTROL_TYPE *)ControlS[i].aux);
                ASI_BOOL swAuto         = (ControlS[i].s == ISS_ON) ? ASI_TRUE : ASI_FALSE;

                for (int j = 0; j < ControlNP.nnp; j++)
                {
                    ASI_CONTROL_TYPE nType = *((ASI_CONTROL_TYPE *)ControlN[j].aux0);

                    if (swType == nType)
                    {
                        DEBUGF(INDI::Logger::DBG_DEBUG, "ISNewSwitch->SetControlValue %d %.2f", nType,
                               ControlN[j].value);
                        if ((errCode = ASISetControlValue(m_camInfo->CameraID, nType, ControlN[j].value, swAuto)) !=
                            ASI_SUCCESS)
                        {
                            DEBUGF(INDI::Logger::DBG_ERROR, "ASISetControlValue (%s=%g) error (%d)", ControlN[j].name,
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
                DEBUG(INDI::Logger::DBG_ERROR, "Cannot change format while streaming/recording.");
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
                DEBUGF(INDI::Logger::DBG_ERROR, "Unable to locate format %s.", targetFormat);
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
            DEBUG(INDI::Logger::DBG_WARNING, "Warning: 16bit RAW is not supported on all hardware platforms.");
            break;

        default:
            PrimaryCCD.setBPP(8);
            break;
    }

    UpdateCCDFrame(PrimaryCCD.getSubX(), PrimaryCCD.getSubY(), PrimaryCCD.getSubW(), PrimaryCCD.getSubH());

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
            DEBUGF(INDI::Logger::DBG_DEBUG, "Switching to %s video format.", vf->label);
            PrimaryCCD.setBPP(8);
            UpdateCCDFrame(PrimaryCCD.getSubX(), PrimaryCCD.getSubY(), PrimaryCCD.getSubW(), PrimaryCCD.getSubH());
            IDSetSwitch(&VideoFormatSP, nullptr);
        }
        else
        {
            DEBUG(INDI::Logger::DBG_ERROR, "No 8 bit video format found, cannot start stream.");
            return false;
        }
    }
#endif

    ExposureRequest = 1.0 / Streamer->getTargetFPS();
    ASIStartVideoCapture(m_camInfo->CameraID);
    pthread_mutex_lock(&condMutex);
    threadRequest = StateStream;
    pthread_cond_signal(&cv);
    pthread_mutex_unlock(&condMutex);

    return true;
}

bool ASICCD::StopStreaming()
{
    pthread_mutex_lock(&condMutex);
    threadRequest = StateAbort;
    pthread_cond_signal(&cv);
    while (threadState == StateStream)
    {
        pthread_cond_wait(&cv, &condMutex);
    }
    pthread_mutex_unlock(&condMutex);
    ASIStopVideoCapture(m_camInfo->CameraID);

    if (IUFindOnSwitchIndex(&VideoFormatSP) != rememberVideoFormat)
        setVideoFormat(rememberVideoFormat);

    return true;
}

int ASICCD::SetTemperature(double temperature)
{
    // If there difference, for example, is less than 0.1 degrees, let's immediately return OK.
    if (fabs(temperature - TemperatureN[0].value) < TEMP_THRESHOLD)
        return 1;

    if (activateCooler(true) == false)
    {
        DEBUG(INDI::Logger::DBG_ERROR, "Failed to activate cooler!");
        return -1;
    }

    long tVal;
    if (temperature > 0.5)
        tVal = (long)(temperature + 0.49);
    else if (temperature  < 0.5)
        tVal = (long)(temperature - 0.49);
    else
        tVal = 0;
    if (ASISetControlValue(m_camInfo->CameraID, ASI_TARGET_TEMP, tVal, ASI_TRUE) != ASI_SUCCESS)
    {
        DEBUG(INDI::Logger::DBG_ERROR, "Failed to set temperature!");
        return -1;
    }

    // Otherwise, we set the temperature request and we update the status in TimerHit() function.
    TemperatureRequest = temperature;
    DEBUGF(INDI::Logger::DBG_SESSION, "Setting CCD temperature to %+06.2f C", temperature);
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
        CoolerSP.s   = enable ? IPS_BUSY: IPS_IDLE;
    }
    IDSetSwitch(&CoolerSP, nullptr);

    return rc;
}

bool ASICCD::StartExposure(float duration)
{
    ASI_ERROR_CODE errCode = ASI_SUCCESS;

    PrimaryCCD.setExposureDuration(duration);
    ExposureRequest = duration;

    DEBUGF(INDI::Logger::DBG_DEBUG, "StartExposure->setexp : %.3fs", duration);
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
            ASI_SUCCESS)
        {
            DEBUGF(INDI::Logger::DBG_ERROR, "ASIStartExposure error (%d)", errCode);
            // Wait 100ms before trying again
            usleep(100000);
            continue;
        }
        break;
    }

    if (errCode != ASI_SUCCESS)
    {
        DEBUG(INDI::Logger::DBG_WARNING, "ASI firmware might require an update to *compatible mode. Check http://www.indilib.org/devices/ccds/zwo-optics-asi-cameras.html for details.");
        return false;
    }

    gettimeofday(&ExpStart, nullptr);
    if (ExposureRequest > VERBOSE_EXPOSURE)
        DEBUGF(INDI::Logger::DBG_SESSION, "Taking a %g seconds frame...", ExposureRequest);

    InExposure = true;
    pthread_mutex_lock(&condMutex);
    threadRequest = StateExposure;
    pthread_cond_signal(&cv);
    pthread_mutex_unlock(&condMutex);

    //updateControls();

    return true;
}

bool ASICCD::AbortExposure()
{
    DEBUG(INDI::Logger::DBG_DEBUG, "AbortExposure");
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
    return true;
}

bool ASICCD::UpdateCCDFrame(int x, int y, int w, int h)
{
    long bin_width  = w / PrimaryCCD.getBinX();
    long bin_height = h / PrimaryCCD.getBinY();

    bin_width  = bin_width - (bin_width % 2);
    bin_height = bin_height - (bin_height % 2);

    w = bin_width * PrimaryCCD.getBinX();
    h = bin_height * PrimaryCCD.getBinY();

    ASI_ERROR_CODE errCode = ASI_SUCCESS;

    /* Add the X and Y offsets */
    long x_1 = x / PrimaryCCD.getBinX();
    long y_1 = y / PrimaryCCD.getBinY();

    if (bin_width > PrimaryCCD.getXRes() / PrimaryCCD.getBinX())
    {
        DEBUGF(INDI::Logger::DBG_SESSION, "Error: invalid width requested %d", w);
        return false;
    }
    else if (bin_height > PrimaryCCD.getYRes() / PrimaryCCD.getBinY())
    {
        DEBUGF(INDI::Logger::DBG_SESSION, "Error: invalid height request %d", h);
        return false;
    }

    DEBUGF(INDI::Logger::DBG_DEBUG, "UpdateCCDFrame ASISetROIFormat (%dx%d,  bin %d, type %d)", bin_width, bin_height,
           PrimaryCCD.getBinX(), getImageType());
    if ((errCode = ASISetROIFormat(m_camInfo->CameraID, bin_width, bin_height, PrimaryCCD.getBinX(), getImageType())) !=
        ASI_SUCCESS)
    {
        DEBUGF(INDI::Logger::DBG_ERROR, "ASISetROIFormat (%dx%d @ %d) error (%d)", bin_width, bin_height,
               PrimaryCCD.getBinX(), errCode);
        return false;
    }

    if ((errCode = ASISetStartPos(m_camInfo->CameraID, x_1, y_1)) != ASI_SUCCESS)
    {
        DEBUGF(INDI::Logger::DBG_ERROR, "ASISetStartPos (%d,%d) error (%d)", x_1, y_1, errCode);
        return false;
    }    

    // Set UNBINNED coords
    PrimaryCCD.setFrame(x, y, w, h);

    int nChannels = 1;

    if (getImageType() == ASI_IMG_RGB24)
        nChannels = 3;

    // Total bytes required for image buffer
    uint32_t nbuf = (bin_width * bin_height * PrimaryCCD.getBPP() / 8) * nChannels;
    DEBUGF(INDI::Logger::DBG_DEBUG, "Setting frame buffer size to %d bytes.", nbuf);
    PrimaryCCD.setFrameBufferSize(nbuf);

    // Always set BINNED size
    Streamer->setSize(bin_width, bin_height);

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
    return (float)timeleft;
}

/* Downloads the image from the CCD.
 N.B. No processing is done on the image */
int ASICCD::grabImage()
{
    ASI_ERROR_CODE errCode = ASI_SUCCESS;

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
            DEBUGF(INDI::Logger::DBG_ERROR, "CCD ID: %d malloc failed (RGB 24)",
                m_camInfo->CameraID);
            return -1;
        }
    }

    if ((errCode = ASIGetDataAfterExp(m_camInfo->CameraID, buffer, size)) != ASI_SUCCESS)
    {
        DEBUGF(INDI::Logger::DBG_ERROR, "ASIGetDataAfterExp (%dx%d #%d channels) error (%d)", width, height, nChannels,
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
        DEBUG(INDI::Logger::DBG_SESSION, "Download complete.");

    ExposureComplete(&PrimaryCCD);

    // Restore bayer cap
    if (restoreBayer)
        SetCCDCapability(GetCCDCapability() | CCD_HAS_BAYER);

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
        DEBUGF(INDI::Logger::DBG_ERROR,
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
        if (errCode != ASI_SUCCESS)
        {
            DEBUGF(INDI::Logger::DBG_ERROR,
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
    ((ASICCD *)context)->TimerNS();
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
    DEBUGF(INDI::Logger::DBG_DEBUG, "Stopping %s guide.", NSDirName);
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

    DEBUGF(INDI::Logger::DBG_DEBUG, "Starting %s guide for %f ms",
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
        DEBUGF(INDI::Logger::DBG_DEBUG, "Stopped %s guide.", dirName);
        return IPS_OK;
    }
    else
    {
        NStimerID = IEAddTimer(mSecs, ASICCD::TimerHelperNS, this);
        return IPS_BUSY;
    }
}

IPState ASICCD::GuideNorth(float ms)
{
    return guidePulseNS(ms, ASI_GUIDE_NORTH, "North");
}

IPState ASICCD::GuideSouth(float ms)
{
    return guidePulseNS(ms, ASI_GUIDE_SOUTH, "South");
}

/* Helper function for WE timer call back */
void ASICCD::TimerHelperWE(void *context)
{
    ((ASICCD *)context)->TimerWE();
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
    DEBUGF(INDI::Logger::DBG_DEBUG, "Stopping %s guide.", WEDirName);
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

    DEBUGF(INDI::Logger::DBG_DEBUG, "Starting %s guide for %f ms",
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

    ASIPulseGuideOn(m_camInfo->CameraID, NSDir);
    if (uSecs != 0)
    {
        usleep(uSecs);
        ASIPulseGuideOff(m_camInfo->CameraID, WEDir);
        DEBUGF(INDI::Logger::DBG_DEBUG, "Stopped %s guide.", dirName);
        return IPS_OK;
    }
    else
    {
        WEtimerID = IEAddTimer(mSecs, ASICCD::TimerHelperWE, this);
        return IPS_BUSY;
    }
}

IPState ASICCD::GuideEast(float ms)
{
    return guidePulseWE(ms, ASI_GUIDE_EAST, "East");
}

IPState ASICCD::GuideWest(float ms)
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
        DEBUGF(INDI::Logger::DBG_ERROR, "CCD ID: %d malloc failed (controls)",
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
        DEBUGF(INDI::Logger::DBG_ERROR,
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
        DEBUGF(INDI::Logger::DBG_ERROR,
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
            DEBUGF(INDI::Logger::DBG_ERROR, "ASIGetControlCaps error (%d)", errCode);
            return;
        }

        DEBUGF(INDI::Logger::DBG_DEBUG,
               "Control #%d: name (%s), Descp (%s), Min (%ld), Max (%ld), Default Value (%ld), IsAutoSupported (%s), "
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
            DEBUGF(INDI::Logger::DBG_DEBUG, "createControls->set USB %d", oneControlCap->MinValue);
            ASISetControlValue(m_camInfo->CameraID, oneControlCap->ControlType, oneControlCap->MinValue, ASI_FALSE);
        }
#else
        if (oneControlCap->ControlType == ASI_BANDWIDTHOVERLOAD)
        {
            if (m_camInfo->IsUSB3Camera && !m_camInfo->IsUSB3Host)
            {
                DEBUGF(INDI::Logger::DBG_DEBUG, "createControls->set USB %d", 0.8 * oneControlCap->MaxValue);
                ASISetControlValue(m_camInfo->CameraID, oneControlCap->ControlType, 0.8 * oneControlCap->MaxValue,
                                   ASI_FALSE);
            }
            else
            {
                DEBUGF(INDI::Logger::DBG_DEBUG, "createControls->set USB %d", oneControlCap->MinValue);
                ASISetControlValue(m_camInfo->CameraID, oneControlCap->ControlType, oneControlCap->MinValue, ASI_FALSE);
            }
        }
#endif

        ASIGetControlValue(m_camInfo->CameraID, oneControlCap->ControlType, &pValue, &isAuto);

        if (oneControlCap->IsWritable)
        {
            nWritableControls++;

            DEBUGF(INDI::Logger::DBG_DEBUG,
                "Adding above control as writable control number %d",
                nWritableControls);

            IUFillNumber(control_np, oneControlCap->Name, oneControlCap->Name,
                "%g", oneControlCap->MinValue, oneControlCap->MaxValue,
                (oneControlCap->MaxValue - oneControlCap->MinValue) / 10.0,
                pValue);
            control_np->aux0 = (void *)&oneControlCap->ControlType;
            control_np->aux1 = (void *)&oneControlCap->IsAutoSupported;
            control_np++;
        }

        if (oneControlCap->IsAutoSupported)
        {
            nAutoSwitches++;

            DEBUGF(INDI::Logger::DBG_DEBUG,
                "Adding above control as auto control number %d",
                nAutoSwitches);

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
        case ASI_BAYER_RG:
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
            type = *((ASI_IMG_TYPE *)sp->aux);
    }

    return type;
}

void ASICCD::updateControls()
{
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
    return ((ASICCD *)context)->imagingThreadEntry();
}

/*
 * A dedicated thread is used for handling streaming video and image
 * exposures because the operations take too much time to be done
 * as part of a timer call-back: there is one timer for the entire
 * process, which must handle events for all ASI cameras
 */
void *ASICCD::imagingThreadEntry()
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

void ASICCD::streamVideo()
{
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
        if (ret != ASI_SUCCESS)
        {
            if (ret != ASI_ERROR_TIMEOUT)
            {
                Streamer->setStream(false);
                pthread_mutex_lock(&condMutex);
                if (threadRequest == StateStream)
                {
                    DEBUGF(INDI::Logger::DBG_ERROR,
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
}

void ASICCD::getExposure()
{
    float timeLeft;
    int expRetry = 0;
    int statRetry = 0;
    int uSecs = 1000000;
    ASI_EXPOSURE_STATUS status = ASI_EXP_IDLE;
    ASI_ERROR_CODE errCode;

    pthread_mutex_unlock(&condMutex);
    usleep(10000);
    pthread_mutex_lock(&condMutex);

    while (threadRequest == StateExposure)
    {
        pthread_mutex_unlock(&condMutex);
        errCode = ASIGetExpStatus(m_camInfo->CameraID, &status);
        if (errCode == ASI_SUCCESS)
        {
            if (status == ASI_EXP_SUCCESS)
            {
                InExposure = false;
                PrimaryCCD.setExposureLeft(0.0);
                DEBUG(INDI::Logger::DBG_SESSION,
                    "Exposure done, downloading image...");
                grabImage();
                pthread_mutex_lock(&condMutex);
                exposureSetRequest(StateIdle);
                break;
            }
            else if (status == ASI_EXP_FAILED)
            {
                if (++expRetry < MAX_EXP_RETRIES)
                {
                    if (threadRequest == StateExposure)
                    {
                        DEBUG(INDI::Logger::DBG_DEBUG,
                            "ASIGetExpStatus failed. Restarting exposure...");
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
                        DEBUGF(INDI::Logger::DBG_ERROR,
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
            DEBUGF(INDI::Logger::DBG_DEBUG, "ASIGetExpStatus error (%d)",
                errCode);
            if (++statRetry >= 10)
            {
                if (threadRequest == StateExposure)
                {
                    DEBUGF(INDI::Logger::DBG_ERROR,
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
    {
        IUSaveConfigNumber(fp, &CoolerNP);
        IUSaveConfigSwitch(fp, &CoolerSP);
    }

    if (ControlNP.nnp > 0)
        IUSaveConfigNumber(fp, &ControlNP);

    if (ControlSP.nsp > 0)
        IUSaveConfigSwitch(fp, &ControlSP);

    IUSaveConfigSwitch(fp, &VideoFormatSP);

    return true;
}
