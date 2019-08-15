/*
    Driver type: GPhoto Camera INDI Driver

    Copyright (C) 2009 Geoffrey Hausheer
    Copyright (C) 2013 Jasem Mutlaq (mutlaqja AT ikarustech DOT com)

    This library is free software; you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published
    by the Free Software Foundation; either version 2.1 of the License, or
    (at your option) any later version.

    This library is distributed in the hope that it will be useful, but
    WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
    or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
    License for more details.

    You should have received a copy of the GNU Lesser General Public License
    along with this library; if not, write to the Free Software Foundation,
    Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA

*/

#include "gphoto_ccd.h"

#include "config.h"
#include "gphoto_driver.h"
#include "gphoto_readimage.h"

#include <algorithm>
#include <stream/streammanager.h>

#include <math.h>
#include <unistd.h>

#define FOCUS_TAB    "Focus"
#define MAX_DEVICES  5 /* Max device cameraCount */
#define FOCUS_TIMER  50
#define MAX_RETRIES  3

extern char * me;

static int cameraCount = 0;
static GPhotoCCD * cameras[MAX_DEVICES];
static GPContext * context = gp_context_new();

typedef struct
{
    const char * exec;
    const char * driver;
    const char * model;
} CamDriverInfo;

static CamDriverInfo camInfos[] = { { "indi_gphoto_ccd", "GPhoto CCD", "GPhoto" },
    { "indi_canon_ccd", "Canon DSLR", "Canon" },
    { "indi_nikon_ccd", "Nikon DSLR", "Nikon" },
    { "indi_pentax_ccd", "Pentax DSLR", "Pentax" },
    { "indi_sony_ccd", "Sony DSLR", "Sony" },
    { "indi_fuji_ccd", "Fuji DSLR", "Fuji" },
    { nullptr, nullptr, nullptr }
};

/**********************************************************
 *
 *  IMPORRANT: List supported camera models in initializer of deviceTypes structure
 *
 **********************************************************/

static void cleanup()
{
    for (int i = 0; i < cameraCount; i++)
    {
        delete cameras[i];
    }
}

void ISInit()
{
    static bool isInit = false;
    if (!isInit)
    {
        // Let's just create one camera for now
        if (!strcmp(me, "indi_gphoto_ccd"))
        {
            isInit      = true;
            cameraCount = 1;
            cameras[0]  = new GPhotoCCD();
            atexit(cleanup);
        }
        else
        {
            CameraList * list;
            /* Detect all the cameras that can be autodetected... */
            int ret = gp_list_new(&list);
            if (ret < GP_OK)
            {
                // Use Legacy Mode
                IDLog("Failed to initilize list in libgphoto2\n");
                return;
            }

            const char * model, *port;
            gp_list_reset(list);
            cameraCount = gp_camera_autodetect(list, context);
            /* Now open all cameras we autodected for usage */
            IDLog("Number of cameras detected: %d.\n", cameraCount);

            if (cameraCount == 0)
            {
                IDLog("Failed to detect any cameras. Check power and make sure camera is not mounted by other programs "
                      "and try again.\n");
                // Use Legacy Mode
#if 0
                IDLog("No cameras detected. Using legacy mode...");
                cameraCount = 1;
                cameras[0] = new GPhotoCCD();
                atexit(cleanup);
                isInit = true;
#endif
                return;
            }

            int availableCameras = cameraCount;
            int cameraIndex = 0;
            cameraCount = 0;

            std::vector<std::string> cameraNames;

            while (availableCameras > 0)
            {
                gp_list_get_name(list, cameraIndex, &model);
                gp_list_get_value(list, cameraIndex, &port);

                IDLog("Detected camera model %s on port %s\n", model, port);

                cameraIndex++;
                availableCameras--;

                // If we're NOT using the Generic INDI GPhoto drievr
                // then let's search for multiple cameras
                if (strcmp(me, "indi_gphoto_ccd"))
                {
                    char prefix[MAXINDINAME];
                    char name[MAXINDINAME];
                    bool modelFound = false;

                    for (int j = 0; camInfos[j].exec != nullptr; j++)
                    {
                        if (strstr(model, camInfos[j].model))
                        {
                            strncpy(prefix, camInfos[j].driver, MAXINDINAME);

                            // If if the model was already registered for a prior camera in case we are using
                            // two identical models
                            if (std::find(cameraNames.begin(), cameraNames.end(), camInfos[j].model) == cameraNames.end())
                                snprintf(name, MAXINDIDEVICE, "%s %s", prefix, model + strlen(camInfos[j].model) + 1);
                            else
                                snprintf(name, MAXINDIDEVICE, "%s %s %d", prefix, model + strlen(camInfos[j].model) + 1,
                                         static_cast<int>(std::count(cameraNames.begin(), cameraNames.end(), camInfos[j].model)) + 1);
                            cameras[cameraCount] = new GPhotoCCD(model, port);
                            cameras[cameraCount]->setDeviceName(name);
                            cameraCount++;
                            modelFound = true;
                            // Store camera model in list to check for duplicates
                            cameraNames.push_back(camInfos[j].model);
                            break;
                        }
                    }

                    if (modelFound == false)
                    {
                        IDLog("Failed to find model %s in supported cameras.\n", model);
                        // If we are no cameras left
                        // Let us use the generic model name
                        // This is a libgphoto2 bug for some cameras which model does not correspond
                        // to the actual make of the camera but rather a generic class designation is given (e.g. PTP USB Camera)
                        if (availableCameras == 0)
                        {
                            IDLog("Falling back to generic name.\n");
                            for (int j = 0; camInfos[j].exec != nullptr; j++)
                            {
                                if (!strcmp(camInfos[j].exec, me))
                                {
                                    snprintf(name, MAXINDIDEVICE, "%s", camInfos[j].model);
                                    cameras[cameraCount] = new GPhotoCCD(model, port);
                                    cameras[cameraCount]->setDeviceName(name);
                                    cameraCount++;
                                }
                            }
                        }
                    }
                }
                else
                {
                    cameras[cameraCount++] = new GPhotoCCD(model, port);
                }
            }
            atexit(cleanup);
            isInit = true;
        }
    }
}

void ISGetProperties(const char * dev)
{
    ISInit();

    if (cameraCount == 0)
    {
        IDMessage(nullptr, "No cameras detected.Check power and make sure camera is not mounted by other programs and try again.");
        return;
    }

    for (int i = 0; i < cameraCount; i++)
    {
        GPhotoCCD * camera = cameras[i];
        if (dev == nullptr || !strcmp(dev, camera->name))
        {
            camera->ISGetProperties(dev);
            if (dev != nullptr)
                break;
        }
    }
}

void ISNewSwitch(const char * dev, const char * name, ISState * states, char * names[], int num)
{
    ISInit();
    for (int i = 0; i < cameraCount; i++)
    {
        GPhotoCCD * camera = cameras[i];
        if (dev == nullptr || !strcmp(dev, camera->name))
        {
            camera->ISNewSwitch(dev, name, states, names, num);
            if (dev != nullptr)
                break;
        }
    }
}

void ISNewText(const char * dev, const char * name, char * texts[], char * names[], int num)
{
    ISInit();
    for (int i = 0; i < cameraCount; i++)
    {
        GPhotoCCD * camera = cameras[i];
        if (dev == nullptr || !strcmp(dev, camera->name))
        {
            camera->ISNewText(dev, name, texts, names, num);
            if (dev != nullptr)
                break;
        }
    }
}

void ISNewNumber(const char * dev, const char * name, double values[], char * names[], int num)
{
    ISInit();
    for (int i = 0; i < cameraCount; i++)
    {
        GPhotoCCD * camera = cameras[i];
        if (dev == nullptr || !strcmp(dev, camera->name))
        {
            camera->ISNewNumber(dev, name, values, names, num);
            if (dev != nullptr)
                break;
        }
    }
}

void ISNewBLOB(const char * dev, const char * name, int sizes[], int blobsizes[], char * blobs[], char * formats[],
               char * names[], int n)
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
void ISSnoopDevice(XMLEle * root)
{
    ISInit();

    for (int i = 0; i < cameraCount; i++)
    {
        GPhotoCCD * camera = cameras[i];
        camera->ISSnoopDevice(root);
    }
}

//==========================================================================
GPhotoCCD::GPhotoCCD() : FI(this)
{
    memset(model, 0, MAXINDINAME);
    memset(port, 0, MAXINDINAME);

    gphotodrv        = nullptr;
    frameInitialized = false;
    on_off[0]        = strdup("On");
    on_off[1]        = strdup("Off");

    setVersion(INDI_GPHOTO_VERSION_MAJOR, INDI_GPHOTO_VERSION_MINOR);
}

GPhotoCCD::GPhotoCCD(const char * model, const char * port) : FI(this)
{
    strncpy(this->port, port, MAXINDINAME);
    strncpy(this->model, model, MAXINDINAME);

    gphotodrv        = nullptr;
    frameInitialized = false;
    on_off[0]        = strdup("On");
    on_off[1]        = strdup("Off");

    setVersion(INDI_GPHOTO_VERSION_MAJOR, INDI_GPHOTO_VERSION_MINOR);
}

//==========================================================================
GPhotoCCD::~GPhotoCCD()
{
    free(on_off[0]);
    free(on_off[1]);
    expTID = 0;
}

const char * GPhotoCCD::getDefaultName()
{
    return "GPhoto CCD";
}

bool GPhotoCCD::initProperties()
{
    /*if (strcmp(me, "indi_gphoto_ccd"))
    {
        char prefix[MAXINDINAME];
        modelFound = false;

        for (int i = 0; camInfos[i].exec != nullptr; i++)
        {
            if (strstr(model, camInfos[i].model))
            {
                strncpy(prefix, camInfos[i].driver, MAXINDINAME);
                snprintf(this->name, MAXINDIDEVICE, "%s %s", prefix, model + strlen(camInfos[i].model) + 1);
                setDeviceName(this->name);
                modelFound = true;
            }
        }

        if (modelFound == false)
        {
            LOGF_ERROR("Failed to find model %s in %s", model, getDeviceName());
            return false;
        }
    }*/
    //else
    //{
    // For now let's set name to default name. In the future, we need to to support multiple devices per one driver
    if (*getDeviceName() == '\0')
        strncpy(name, getDefaultName(), MAXINDINAME);
    else
        strncpy(name, getDeviceName(), MAXINDINAME);
    setDeviceName(this->name);
    //modelFound = true;
    //}

    // Init parent properties first
    INDI::CCD::initProperties();

    FI::initProperties(FOCUS_TAB);

    IUFillText(&mPortT[0], "PORT", "Port", "");
    IUFillTextVector(&PortTP, mPortT, NARRAY(mPortT), getDeviceName(), "DEVICE_PORT", "Shutter Release",
                     MAIN_CONTROL_TAB, IP_RW, 0, IPS_IDLE);

    IUFillNumber(&mMirrorLockN[0], "MIRROR_LOCK_SECONDS", "Seconds", "%1.0f", 0, 10, 1, 0);
    IUFillNumberVector(&mMirrorLockNP, mMirrorLockN, 1, getDeviceName(), "MIRROR_LOCK", "Mirror Lock", MAIN_CONTROL_TAB,
                       IP_RW, 60, IPS_IDLE);

    //We don't know how many items will be in the switch yet
    IUFillSwitchVector(&mIsoSP, nullptr, 0, getDeviceName(), "CCD_ISO", "ISO", IMAGE_SETTINGS_TAB, IP_RW, ISR_1OFMANY, 60,
                       IPS_IDLE);
    IUFillSwitchVector(&mFormatSP, nullptr, 0, getDeviceName(), "CAPTURE_FORMAT", "Capture Format", IMAGE_SETTINGS_TAB,
                       IP_RW, ISR_1OFMANY, 60, IPS_IDLE);
    IUFillSwitchVector(&mExposurePresetSP, nullptr, 0, getDeviceName(), "CCD_EXPOSURE_PRESETS", "Presets",
                       MAIN_CONTROL_TAB, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);

    IUFillSwitch(&autoFocusS[0], "Set", "", ISS_OFF);
    IUFillSwitchVector(&autoFocusSP, autoFocusS, 1, getDeviceName(), "Auto Focus", "", FOCUS_TAB, IP_RW, ISR_1OFMANY, 0,
                       IPS_IDLE);

    IUFillSwitch(&transferFormatS[0], "FORMAT_FITS", "FITS", ISS_ON);
    IUFillSwitch(&transferFormatS[1], "FORMAT_NATIVE", "Native", ISS_OFF);
    IUFillSwitchVector(&transferFormatSP, transferFormatS, 2, getDeviceName(), "CCD_TRANSFER_FORMAT", "Transfer Format",
                       IMAGE_SETTINGS_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    IUFillSwitch(&livePreviewS[0], "Enable", "", ISS_OFF);
    IUFillSwitch(&livePreviewS[1], "Disable", "", ISS_ON);
    IUFillSwitchVector(&livePreviewSP, livePreviewS, 2, getDeviceName(), "AUX_VIDEO_STREAM", "Preview",
                       MAIN_CONTROL_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    IUFillSwitch(&captureTargetS[CAPTURE_INTERNAL_RAM], "RAM", "", ISS_ON);
    IUFillSwitch(&captureTargetS[CAPTURE_SD_CARD], "SD Card", "", ISS_OFF);
    IUFillSwitchVector(&captureTargetSP, captureTargetS, 2, getDeviceName(), "CCD_CAPTURE_TARGET", "Capture Target",
                       IMAGE_SETTINGS_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    IUFillSwitch(&SDCardImageS[SD_CARD_SAVE_IMAGE], "Save", "", ISS_ON);
    IUFillSwitch(&SDCardImageS[SD_CARD_DELETE_IMAGE], "Delete", "", ISS_OFF);
    IUFillSwitchVector(&SDCardImageSP, SDCardImageS, 2, getDeviceName(), "CCD_SD_CARD_ACTION", "SD Image",
                       IMAGE_SETTINGS_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    IUFillSwitch(&forceBULBS[FORCE_BULB_ON], "On", "On", ISS_ON);
    IUFillSwitch(&forceBULBS[FORCE_BULB_OFF], "Off", "Off", ISS_OFF);
    IUFillSwitchVector(&forceBULBSP, forceBULBS, 2, getDeviceName(), "CCD_FORCE_BLOB", "Force BULB",
                       OPTIONS_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    PrimaryCCD.setMinMaxStep("CCD_EXPOSURE", "CCD_EXPOSURE_VALUE", 0.001, 3600, 1, false);

    // Most cameras have this by default, so let's set it as default.
    IUSaveText(&BayerT[2], "RGGB");

#ifdef HAVE_WEBSOCKET
    SetCCDCapability(CCD_CAN_SUBFRAME | CCD_CAN_ABORT | CCD_HAS_BAYER | CCD_HAS_STREAMING | CCD_HAS_WEB_SOCKET);
#else
    SetCCDCapability(CCD_CAN_SUBFRAME | CCD_CAN_ABORT | CCD_HAS_BAYER | CCD_HAS_STREAMING);
#endif

    Streamer->setStreamingExposureEnabled(false);

#if 0
    FI::SetCapability(FOCUSER_HAS_VARIABLE_SPEED);
    FocusSpeedN[0].min   = 0;
    FocusSpeedN[0].max   = 3;
    FocusSpeedN[0].step  = 1;
    FocusSpeedN[0].value = 3;
#endif

    FI::SetCapability(FOCUSER_CAN_REL_MOVE);

    /* JM 2014-05-20 Make PrimaryCCD.ImagePixelSizeNP writable since we can't know for now the pixel size and bit depth from gphoto */
    PrimaryCCD.getCCDInfo()->p = IP_RW;

    setDriverInterface(getDriverInterface() | FOCUSER_INTERFACE);

    gphoto_set_debug(getDeviceName());
    gphoto_read_set_debug(getDeviceName());

    // Add Debug, Simulator, and Configuration controls
    addAuxControls();

    return true;
}

void GPhotoCCD::ISGetProperties(const char * dev)
{
    INDI::CCD::ISGetProperties(dev);

    defineText(&PortTP);
    loadConfig(true, "DEVICE_PORT");

    if (isConnected())
        return;

    // Read Image Info if we have not connected yet.
    double pixel = 0, pixel_x = 0, pixel_y = 0;
    IUGetConfigNumber(getDeviceName(), "CCD_INFO", "CCD_PIXEL_SIZE", &pixel);
    IUGetConfigNumber(getDeviceName(), "CCD_INFO", "CCD_PIXEL_SIZE_X", &pixel_x);
    IUGetConfigNumber(getDeviceName(), "CCD_INFO", "CCD_PIXEL_SIZE_Y", &pixel_y);

    INumberVectorProperty *nvp = PrimaryCCD.getCCDInfo();

    if (!nvp)
        return;

    // Load the necessary pixel size information
    // The maximum resolution and bits per pixel depend on the capture itself.
    // while the pixel size data remains constant.
    if (pixel > 0)
        nvp->np[INDI::CCDChip::CCD_PIXEL_SIZE].value = pixel;
    if (pixel_x > 0)
        nvp->np[INDI::CCDChip::CCD_PIXEL_SIZE_X].value = pixel_x;
    if (pixel_y > 0)
        nvp->np[INDI::CCDChip::CCD_PIXEL_SIZE_Y].value = pixel_y;

}

bool GPhotoCCD::updateProperties()
{
    INDI::CCD::updateProperties();

    if (isConnected())
    {
        if (mExposurePresetSP.nsp > 0)
            defineSwitch(&mExposurePresetSP);
        if (mIsoSP.nsp > 0)
            defineSwitch(&mIsoSP);
        if (mFormatSP.nsp > 0)
            defineSwitch(&mFormatSP);

        defineSwitch(&livePreviewSP);
        defineSwitch(&transferFormatSP);
        defineSwitch(&autoFocusSP);

        if (m_CanFocus)
            FI::updateProperties();

        if (captureTargetSP.s == IPS_OK)
        {
            defineSwitch(&captureTargetSP);
        }

        defineSwitch(&SDCardImageSP);

        imageBP = getBLOB("CCD1");
        imageB  = imageBP->bp;

        // Dummy values until first capture is done
        //SetCCDParams(1280, 1024, 8, 5.4, 5.4);

        if (isSimulation() == false)
        {
            ShowExtendedOptions();

            if (strstr(gphoto_get_manufacturer(gphotodrv), "Canon"))
                defineNumber(&mMirrorLockNP);
        }

        if (isSimulation())
            isTemperatureSupported = false;
        else
            isTemperatureSupported = gphoto_supports_temperature(gphotodrv);

        if (isTemperatureSupported)
        {
            TemperatureNP.p = IP_RO;
            defineNumber(&TemperatureNP);
        }

        defineSwitch(&forceBULBSP);

        //timerID = SetTimer(POLLMS);
    }
    else
    {
        if (mExposurePresetSP.nsp > 0)
            deleteProperty(mExposurePresetSP.name);
        if (mIsoSP.nsp > 0)
            deleteProperty(mIsoSP.name);
        if (mFormatSP.nsp > 0)
            deleteProperty(mFormatSP.name);

        deleteProperty(mMirrorLockNP.name);
        deleteProperty(livePreviewSP.name);
        deleteProperty(autoFocusSP.name);
        deleteProperty(transferFormatSP.name);

        if (m_CanFocus)
            FI::updateProperties();

        if (captureTargetSP.s != IPS_IDLE)
        {
            deleteProperty(captureTargetSP.name);
        }

        if (isTemperatureSupported)
            deleteProperty(TemperatureNP.name);

        deleteProperty(SDCardImageSP.name);

        deleteProperty(forceBULBSP.name);

        HideExtendedOptions();
    }

    return true;
}

bool GPhotoCCD::ISNewText(const char * dev, const char * name, char * texts[], char * names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        if (strcmp(name, PortTP.name) == 0)
        {
            PortTP.s = IPS_OK;
            IUUpdateText(&PortTP, texts, names, n);
            IDSetText(&PortTP, nullptr);
            return true;
        }

        if (CamOptions.find(name) != CamOptions.end())
        {
            cam_opt * opt = CamOptions[name];
            if (opt->widget->type != GP_WIDGET_TEXT)
            {
                LOGF_ERROR("ERROR: Property '%s'is not a string", name);
                return false;
            }
            if (opt->widget->readonly)
            {
                LOGF_WARN("WARNING: Property %s is read-only", name);
                IDSetText(&opt->prop.text, nullptr);
                return false;
            }

            if (IUUpdateText(&opt->prop.text, texts, names, n) < 0)
                return false;
            gphoto_set_widget_text(gphotodrv, opt->widget, texts[0]);
            opt->prop.num.s = IPS_OK;
            IDSetText(&opt->prop.text, nullptr);
            return true;
        }
    }

    return INDI::CCD::ISNewText(dev, name, texts, names, n);
}

bool GPhotoCCD::ISNewSwitch(const char * dev, const char * name, ISState * states, char * names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        if (!strcmp(name, mIsoSP.name))
        {
            if (IUUpdateSwitch(&mIsoSP, states, names, n) < 0)
                return false;

            for (int i = 0; i < mIsoSP.nsp; i++)
            {
                if (mIsoS[i].s == ISS_ON)
                {
                    if (isSimulation() == false)
                        gphoto_set_iso(gphotodrv, i);
                    mIsoSP.s = IPS_OK;
                    IDSetSwitch(&mIsoSP, nullptr);
                    break;
                }
            }
        }

        ///////////////////////////////////////////////////////////////////////////////////////////////
        // Force BULB
        // This force driver to _always_ capture in bulb mode and never use predefined exposures unless the exposures are less
        // than a second.
        ///////////////////////////////////////////////////////////////////////////////////////////////
        if (!strcmp(name, forceBULBSP.name))
        {
            if (IUUpdateSwitch(&forceBULBSP, states, names, n) < 0)
                return false;

            forceBULBSP.s = IPS_OK;
            if (forceBULBS[FORCE_BULB_ON].s == ISS_ON)
            {
                if (isSimulation() == false)
                    gphoto_force_bulb(gphotodrv, true);
                LOG_INFO("Force BULB is enabled. All expsures shall be captured in BULB mode except for subsecond captures.");
            }
            else
            {
                if (isSimulation() == false)
                    gphoto_force_bulb(gphotodrv, false);
                LOG_INFO("Force BULB is disabled. Exposures shall utilize camera predefined exposures time first before attempting BULB.");
            }

            IDSetSwitch(&forceBULBSP, nullptr);
            return true;
        }

        if (!strcmp(name, mExposurePresetSP.name))
        {
            if (IUUpdateSwitch(&mExposurePresetSP, states, names, n) < 0)
                return false;

            mExposurePresetSP.s = IPS_OK;
            IDSetSwitch(&mExposurePresetSP, nullptr);

            ISwitch * currentSwitch = IUFindOnSwitch(&mExposurePresetSP);
            if (strcmp(currentSwitch->label, "bulb"))
            {
                LOGF_INFO("Preset %s seconds selected.", currentSwitch->label);

                float duration;
                int num, denom;
                if (sscanf(currentSwitch->label, "%d/%d", &num, &denom) == 2)
                {
                    duration = (static_cast<double>(num)) / (static_cast<double>(denom));
                    StartExposure(duration);
                }
                else if (sscanf(currentSwitch->label, "%g", &duration) == 1)
                {
                    StartExposure(duration);
                }
            }

            return true;
        }

        // Formats
        if (!strcmp(name, mFormatSP.name))
        {
            int prevSwitch = IUFindOnSwitchIndex(&mFormatSP);
            if (IUUpdateSwitch(&mFormatSP, states, names, n) < 0)
                return false;

            ISwitch * sp = IUFindOnSwitch(&mFormatSP);
            if (sp)
            {
                if (strstr(sp->label, "+"))
                {
                    LOGF_ERROR("%s format is not supported.", sp->label);
                    IUResetSwitch(&mFormatSP);
                    mFormatSP.s                = IPS_ALERT;
                    mFormatSP.sp[prevSwitch].s = ISS_ON;
                    IDSetSwitch(&mFormatSP, nullptr);
                    return false;
                }
            }
            for (int i = 0; i < mFormatSP.nsp; i++)
            {
                if (mFormatS[i].s == ISS_ON)
                {
                    if (isSimulation() == false)
                        gphoto_set_format(gphotodrv, i);
                    mFormatSP.s = IPS_OK;
                    IDSetSwitch(&mFormatSP, nullptr);
                    // We need to get frame W and H if format changes
                    frameInitialized = false;
                    break;
                }
            }
        }

        // How images are transferred to the client
        if (!strcmp(name, transferFormatSP.name))
        {
            IUUpdateSwitch(&transferFormatSP, states, names, n);
            transferFormatSP.s = IPS_OK;
            IDSetSwitch(&transferFormatSP, nullptr);
            // We need to get frame W and H if transfer format changes
            // 2017-01-17: Do we? transform format change should not affect W and H
            //frameInitialized = false;
            return true;
        }

        // Autofocus
        if (!strcmp(name, autoFocusSP.name))
        {
            IUResetSwitch(&autoFocusSP);
            char errMsg[MAXRBUF];
            if (gphoto_auto_focus(gphotodrv, errMsg) == GP_OK)
                autoFocusSP.s = IPS_OK;
            else
            {
                autoFocusSP.s = IPS_ALERT;
                LOGF_ERROR("%s", errMsg);
            }

            IDSetSwitch(&autoFocusSP, nullptr);
            return true;
        }

        // Live preview
#if 0
        if (!strcmp(name, livePreviewSP.name))
        {
            IUUpdateSwitch(&livePreviewSP, states, names, n);

            if (Streamer->isBusy())
            {
                livePreviewS[0].s = ISS_OFF;
                livePreviewS[1].s = ISS_ON;
                livePreviewSP.s   = IPS_ALERT;
                LOG_WARN("Cannot start live preview while video streaming is active.");
                IDSetSwitch(&livePreviewSP, nullptr);
                return true;
            }

            if (livePreviewS[0].s == ISS_ON)
            {
                livePreviewSP.s = IPS_BUSY;
                SetTimer(STREAMPOLLMS);
            }
            else
            {
                stopLivePreview();
                livePreviewSP.s = IPS_IDLE;
            }
            IDSetSwitch(&livePreviewSP, nullptr);
            return true;
        }
#endif

        // Capture target
        if (!strcmp(captureTargetSP.name, name))
        {
            const char * onSwitch = IUFindOnSwitchName(states, names, n);
            int captureTarget =
                (!strcmp(onSwitch, captureTargetS[CAPTURE_INTERNAL_RAM].name) ? CAPTURE_INTERNAL_RAM : CAPTURE_SD_CARD);
            int ret = gphoto_set_capture_target(gphotodrv, captureTarget);
            if (ret == GP_OK)
            {
                captureTargetSP.s = IPS_OK;
                IUUpdateSwitch(&captureTargetSP, states, names, n);
                LOGF_INFO("Capture target set to %s",
                          (captureTarget == CAPTURE_INTERNAL_RAM) ? "Internal RAM" : "SD Card");
            }
            else
            {
                captureTargetSP.s = IPS_ALERT;
                LOGF_INFO("Failed to set capture target set to %s",
                          (captureTarget == CAPTURE_INTERNAL_RAM) ? "Internal RAM" : "SD Card");
            }

            IDSetSwitch(&captureTargetSP, nullptr);
            return true;
        }

        if (!strcmp(SDCardImageSP.name, name))
        {
            const char * onSwitch = IUFindOnSwitchName(states, names, n);
            bool delete_sdcard_image = (!strcmp(onSwitch, SDCardImageS[SD_CARD_DELETE_IMAGE].name));
            int ret = gphoto_delete_sdcard_image(gphotodrv, delete_sdcard_image);

            if (ret == GP_OK)
            {
                SDCardImageSP.s = IPS_OK;
                IUUpdateSwitch(&SDCardImageSP, states, names, n);
                LOGF_WARN("All images and folders shall be %s the camera SD card after capture if capture target is set to SD Card.", delete_sdcard_image ? "deleted from" : "saved in");
            }
            else
            {
                SDCardImageSP.s = IPS_ALERT;
                LOG_INFO("Failed to set SD card action.");
            }

            IDSetSwitch(&SDCardImageSP, nullptr);
            return true;
        }

        if (strstr(name, "FOCUS"))
        {
            return FI::processSwitch(dev, name, states, names, n);
        }

        if (CamOptions.find(name) != CamOptions.end())
        {
            cam_opt * opt = CamOptions[name];
            if (opt->widget->type != GP_WIDGET_RADIO && opt->widget->type != GP_WIDGET_MENU &&
                    opt->widget->type != GP_WIDGET_TOGGLE)
            {
                LOGF_ERROR("ERROR: Property '%s'is not a switch (%d)", name, opt->widget->type);
                return false;
            }

            if (opt->widget->readonly)
            {
                LOGF_WARN("WARNING: Property %s is read-only", name);
                IDSetSwitch(&opt->prop.sw, nullptr);
                return false;
            }

            if (IUUpdateSwitch(&opt->prop.sw, states, names, n) < 0)
                return false;

            if (opt->widget->type == GP_WIDGET_TOGGLE)
            {
                gphoto_set_widget_num(gphotodrv, opt->widget, opt->item.sw[ON_S].s == ISS_ON);
            }
            else
            {
                for (int i = 0; i < opt->prop.sw.nsp; i++)
                {
                    if (opt->item.sw[i].s == ISS_ON)
                    {
                        gphoto_set_widget_num(gphotodrv, opt->widget, i);
                        break;
                    }
                }
            }

            opt->prop.sw.s = IPS_OK;
            IDSetSwitch(&opt->prop.sw, nullptr);
            return true;
        }
    }

    return INDI::CCD::ISNewSwitch(dev, name, states, names, n);
}

bool GPhotoCCD::ISNewNumber(const char * dev, const char * name, double values[], char * names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        if (strstr(name, "FOCUS_"))
            return FI::processNumber(dev, name, values, names, n);

        if (!strcmp(name, mMirrorLockNP.name))
        {
            IUUpdateNumber(&mMirrorLockNP, values, names, n);
            mMirrorLockNP.s = IPS_OK;
            IDSetNumber(&mMirrorLockNP, nullptr);
            return true;
        }

        if (CamOptions.find(name) != CamOptions.end())
        {
            cam_opt * opt = CamOptions[name];
            if (opt->widget->type != GP_WIDGET_RANGE)
            {
                LOGF_ERROR("ERROR: Property '%s'is not a string", name);
                return false;
            }
            if (opt->widget->readonly)
            {
                LOGF_WARN("WARNING: Property %s is read-only", name);
                return false;
            }
            if (IUUpdateNumber(&opt->prop.num, values, names, n) < 0)
                return false;
            gphoto_set_widget_num(gphotodrv, opt->widget, values[0]);
            opt->prop.num.s = IPS_OK;
            IDSetNumber(&opt->prop.num, nullptr);
            return true;
        }
    }

    return INDI::CCD::ISNewNumber(dev, name, values, names, n);
}

bool GPhotoCCD::Connect()
{
    int setidx;
    char ** options;
    int max_opts;
    const char * shutter_release_port = nullptr;
    LOGF_DEBUG("Mirror lock value: %f", mMirrorLockN[0].value);

    if (PortTP.tp[0].text && strlen(PortTP.tp[0].text))
    {
        shutter_release_port = PortTP.tp[0].text;
    }

    if (isSimulation() == false)
    {
        // Regular detect
        if (port[0] == '\0')
            gphotodrv = gphoto_open(camera, context, nullptr, nullptr, shutter_release_port);
        else
            gphotodrv = gphoto_open(camera, context, model, port, shutter_release_port);
        if (gphotodrv == nullptr)
        {
            LOG_ERROR("Can not open camera: Power OK? If camera is auto-mounted as external disk "
                      "storage, please unmount it and disable auto-mount.");
            return false;
        }
    }

    if (isSimulation())
    {
        PrimaryCCD.setMinMaxStep("CCD_EXPOSURE", "CCD_EXPOSURE_VALUE", 0.001, 3600, 1, true);
    }
    else
    {
        double min_exposure = 0.001, max_exposure = 3600;

        gphoto_get_minmax_exposure(gphotodrv, &min_exposure, &max_exposure);
        PrimaryCCD.setMinMaxStep("CCD_EXPOSURE", "CCD_EXPOSURE_VALUE", min_exposure, max_exposure, 1, true);
    }

    if (mFormatS)
    {
        free(mFormatS);
        mFormatS = nullptr;
    }

    if (isSimulation())
    {
        const char * fmts[] = { "Custom" };
        setidx             = 0;
        max_opts           = 1;
        options            = const_cast<char **>(fmts);
    }
    else
    {
        setidx  = gphoto_get_format_current(gphotodrv);
        options = gphoto_get_formats(gphotodrv, &max_opts);
    }

    if (max_opts > 0)
    {
        mFormatS      = create_switch("FORMAT", options, max_opts, setidx);
        mFormatSP.sp  = mFormatS;
        mFormatSP.nsp = max_opts;

        ISwitch * sp = IUFindOnSwitch(&mFormatSP);
        if (sp && strstr(sp->label, "+"))
        {
            IUResetSwitch(&mFormatSP);
            int i = 0;

            // Prefer RAW format in case selected format is not supported.
            for (i = 0; i < mFormatSP.nsp; i++)
            {
                // Make sure the new selection does not include the problematic label with the +
                // also also contains the string RAW in it.
                if (strcmp(sp->label, mFormatSP.sp[i].label) && strcasestr("RAW", mFormatSP.sp[i].label))
                {
                    mFormatS[i].s = ISS_ON;
                    break;
                }
            }

            if (i == mFormatSP.nsp)
            {
                LOGF_ERROR("%s format is not supported. Please select another format.", sp->label);
                mFormatSP.s = IPS_ALERT;
            }

            IDSetSwitch(&mFormatSP, nullptr);
        }
    }

    if (mIsoS)
        free(mIsoS);

    if (isSimulation())
    {
        const char * isos[] = { "100", "200", "400", "800" };
        setidx   = 0;
        max_opts = 4;
        options  = const_cast<char **>(isos);
    }
    else
    {
        setidx  = gphoto_get_iso_current(gphotodrv);
        options = gphoto_get_iso(gphotodrv, &max_opts);
    }

    mIsoS      = create_switch("ISO", options, max_opts, setidx);
    mIsoSP.sp  = mIsoS;
    mIsoSP.nsp = max_opts;

    if (mExposurePresetS)
    {
        free(mExposurePresetS);
        mExposurePresetS = nullptr;
    }

    if (isSimulation())
    {
        const char * exposureList[] = { "1/8", "1/4", "1/2", "bulb" };
        setidx   = 0;
        max_opts = 4;
        options  = const_cast<char **>(exposureList);
    }
    else
    {
        setidx   = 0;
        max_opts = 0;
        options  = gphoto_get_exposure_presets(gphotodrv, &max_opts);
    }

    if (max_opts > 0)
    {
        mExposurePresetS      = create_switch("EXPOSURE_PRESET", options, max_opts, setidx);
        mExposurePresetSP.sp  = mExposurePresetS;
        mExposurePresetSP.nsp = max_opts;
    }

    // Get Capture target
    int captureTarget = -1;

    if (!isSimulation() && gphoto_get_capture_target(gphotodrv, &captureTarget) == GP_OK)
    {
        IUResetSwitch(&captureTargetSP);
        captureTargetS[CAPTURE_INTERNAL_RAM].s = (captureTarget == 0) ? ISS_ON : ISS_OFF;
        captureTargetS[CAPTURE_SD_CARD].s      = (captureTarget == 1) ? ISS_ON : ISS_OFF;
        captureTargetSP.s                      = IPS_OK;
    }

    if (isSimulation())
        m_CanFocus = false;
    else
        m_CanFocus = gphoto_can_focus(gphotodrv);

    LOGF_INFO("%s is online.", getDeviceName());

    if (!isSimulation() && gphoto_get_manufacturer(gphotodrv) && gphoto_get_model(gphotodrv))
    {
        LOGF_INFO("Detected %s Model %s.", gphoto_get_manufacturer(gphotodrv),
                  gphoto_get_model(gphotodrv));
    }

    frameInitialized = false;

    return true;
}

bool GPhotoCCD::Disconnect()
{
    if (isSimulation())
        return true;
    gphoto_close(gphotodrv);
    gphotodrv        = nullptr;
    frameInitialized = false;
    LOGF_INFO("%s is offline.", getDeviceName());
    return true;
}

bool GPhotoCCD::StartExposure(float duration)
{
    if (PrimaryCCD.getPixelSizeX() == 0.0)
    {
        LOG_INFO("Please update the CCD Information in the Image Info section before "
                 "proceeding. The camera resolution shall be updated after the first exposure "
                 "is complete.");
        return false;
    }

    if (InExposure)
    {
        LOG_ERROR("GPhoto driver is already exposing.");
        return false;
    }

    if (mFormatS != nullptr)
    {
        ISwitch * sp = IUFindOnSwitch(&mFormatSP);
        if (sp == nullptr)
        {
            LOG_ERROR("Please select a format before capturing an image.");
            return false;
        }
    }

    /* start new exposure with last ExpValues settings.
     * ExpGo goes busy. set timer to read when done
     */

    // Microseconds
    uint32_t exp_us = static_cast<uint32_t>(ceil(duration * 1e6));

    PrimaryCCD.setExposureDuration(duration);

    if (mMirrorLockN[0].value > 0)
        LOGF_INFO("Starting %g seconds exposure (+%g seconds mirror lock).", duration, mMirrorLockN[0].value);
    else
        LOGF_INFO("Starting %g seconds exposure.", duration);

    if (isSimulation() == false && gphoto_start_exposure(gphotodrv, exp_us, mMirrorLockN[0].value) < 0)
    {
        LOG_ERROR("Error starting exposure");
        return false;
    }

    ExposureRequest = duration;
    gettimeofday(&ExpStart, nullptr);
    InExposure = true;

    SetTimer(POLLMS);

    return true;
}

bool GPhotoCCD::AbortExposure()
{
    gphoto_abort_exposure(gphotodrv);
    InExposure = false;
    return true;
}

bool GPhotoCCD::UpdateCCDFrame(int x, int y, int w, int h)
{
    if (transferFormatS[0].s != ISS_ON)
    {
        LOG_ERROR("Subframing is only supported in FITS transport mode.");
        return false;
    }

    PrimaryCCD.setFrame(x, y, w, h);
    return true;
}

double GPhotoCCD::CalcTimeLeft()
{
    struct timeval now, diff;
    gettimeofday(&now, nullptr);

    timersub(&now, &ExpStart, &diff);
    double timesince = diff.tv_sec + diff.tv_usec / 1000000.0;
    return (ExposureRequest - timesince);
}

void GPhotoCCD::TimerHit()
{
    if (isConnected() == false)
        return;

    //    if (Streamer->isBusy())
    //    {
    //        bool rc = streamLiveView();

    //        if (rc)
    //            timerID = SetTimer(STREAMPOLLMS);
    //        else
    //        {
    //            LOG_ERROR("Error capturing video stream.");
    //            Streamer->setStream(false);
    //        }
    //    }

    /*
    if (livePreviewSP.s == IPS_BUSY)
    {
        bool rc = startLivePreview();

        if (rc)
            timerID = SetTimer(STREAMPOLLMS);
        else
        {
            livePreviewSP.s = IPS_ALERT;
            LOG_ERROR("Error capturing preview.");
            livePreviewS[0].s = ISS_OFF;
            livePreviewS[1].s = ISS_ON;
            IDSetSwitch(&livePreviewSP, nullptr);
        }
    }
    */

    //    if (FocusTimerNP.s == IPS_BUSY)
    //    {
    //        char errMsg[MAXRBUF];
    //        if (gphoto_manual_focus(gphotodrv, focusSpeed, errMsg) != GP_OK)
    //        {
    //            LOGF_ERROR("Focusing failed: %s", errMsg);
    //            FocusTimerNP.s       = IPS_ALERT;
    //            FocusTimerN[0].value = 0;
    //        }
    //        else
    //        {
    //            FocusTimerN[0].value -= FOCUS_TIMER;
    //            if (FocusTimerN[0].value <= 0)
    //            {
    //                FocusTimerN[0].value = 0;
    //                FocusTimerNP.s       = IPS_OK;
    //                if (Streamer->isBusy() == false)
    //                    gphoto_set_view_finder(gphotodrv, false);
    //            }
    //            else if (timerID == -1)
    //                timerID = SetTimer(FOCUS_TIMER);
    //        }

    //        IDSetNumber(&FocusTimerNP, nullptr);
    //    }

    if (InExposure)
    {
        int timerID   = -1;
        double timeleft = CalcTimeLeft();

        if (timeleft < 0)
            timeleft = 0;

        PrimaryCCD.setExposureLeft(timeleft);

        if (timeleft < 1.0)
        {
            if (timeleft > 0.25 && timerID == -1)
                timerID = SetTimer(timeleft * 900);
            else
            {
                PrimaryCCD.setExposureLeft(0);
                InExposure = false;
                // grab and save image
                bool rc = grabImage();
                if (rc == false)
                {
                    PrimaryCCD.setExposureFailed();
                }

                if (isTemperatureSupported)
                {
                    double cameraTemperature = static_cast<double>(gphoto_get_last_sensor_temperature(gphotodrv));
                    if (fabs(cameraTemperature - TemperatureN[0].value) > 0.01)
                    {
                        // Check if we are getting bogus temperature values and set property to alert
                        // unless it is already set
                        if (cameraTemperature < MINUMUM_CAMERA_TEMPERATURE)
                        {
                            if (TemperatureNP.s != IPS_ALERT)
                            {
                                TemperatureNP.s = IPS_ALERT;
                                IDSetNumber(&TemperatureNP, nullptr);
                            }
                        }
                        else
                        {
                            TemperatureNP.s = IPS_OK;
                            TemperatureN[0].value = cameraTemperature;
                            IDSetNumber(&TemperatureNP, nullptr);
                        }
                    }
                }
            }
        }
        else
        {
            //LOGF_DEBUG("Capture in progress. Time left %.2f seconds", timeleft);
            if (timerID == -1)
                SetTimer(POLLMS);
        }
    }
}

void GPhotoCCD::UpdateExtendedOptions(void * p)
{
    GPhotoCCD * cam = static_cast<GPhotoCCD *>(p);
    cam->UpdateExtendedOptions();
}

void GPhotoCCD::UpdateExtendedOptions(bool force)
{
    if (!expTID)
    {
        for (auto &option : CamOptions)
        {
            cam_opt * opt = option.second;

            if (force || gphoto_widget_changed(opt->widget))
            {
                gphoto_read_widget(opt->widget);
                UpdateWidget(opt);
            }
        }
    }

    optTID = IEAddTimer(1000, GPhotoCCD::UpdateExtendedOptions, this);
}

bool GPhotoCCD::grabImage()
{
    uint8_t * memptr = PrimaryCCD.getFrameBuffer();
    size_t memsize = 0;
    int naxis = 2, w = 0, h = 0, bpp = 8;

    if (isSimulation())
    {
        uint16_t subW = PrimaryCCD.getSubW() / PrimaryCCD.getBinX();
        uint16_t subH = PrimaryCCD.getSubH() / PrimaryCCD.getBinY();

        subW -= subW % 2;
        subH -= subH % 2;

        uint32_t size = subW * subH;

        if (PrimaryCCD.getFrameBufferSize() < static_cast<int>(size))
        {
            PrimaryCCD.setFrameBufferSize(size);
            memptr = PrimaryCCD.getFrameBuffer();
        }

        // TODO
        // Need to simulate bayer
        // Need to simulate subframing
        if (PrimaryCCD.getBPP() == 8)
        {
            for (uint32_t i = 0 ; i < size; i++)
                memptr[i] = rand() % 255;
        }
        else
        {
            uint16_t *buffer = reinterpret_cast<uint16_t*>(memptr);
            for (uint32_t i = 0 ; i < size; i++)
                buffer[i] = rand() % 65535;
        }

        PrimaryCCD.setFrame(PrimaryCCD.getSubX(), PrimaryCCD.getSubY(), subW, subH);
        ExposureComplete(&PrimaryCCD);
        return true;
    }

    // If only save to SD Card, let's not upload back to client
    /*if (UploadS[GP_UPLOAD_SDCARD].s == ISS_ON)
    {
        LOG_INFO("Exposure complete. Image saved to SD Card.");
        ExposureComplete(&PrimaryCCD);
        return true;
    }*/

    if (transferFormatS[0].s == ISS_ON)
    {
        char tmpfile[] = "/tmp/indi_XXXXXX";

        //dcraw can't read from stdin, so we need to write to disk then read it back
        int fd = mkstemp(tmpfile);

        int ret = gphoto_read_exposure_fd(gphotodrv, fd);

        if (ret != GP_OK || fd == -1)
        {
            if (fd == -1)
                LOGF_ERROR("Exposure failed to save image. Cannot create temp file %s", tmpfile);
            else
            {
                LOGF_ERROR("Exposure failed to save image... %s", gp_result_as_string(ret));
                // As suggested on INDI forums, this result could be misleading.
                if (ret == GP_ERROR_DIRECTORY_NOT_FOUND)
                    LOG_INFO("Make sure BULB switch is ON in the camera. Try setting AF switch to OFF.");
            }
            unlink(tmpfile);
            return false;
        }

        if (!strcmp(gphoto_get_file_extension(gphotodrv), "unknown"))
        {
            LOG_ERROR("Exposure failed.");
            return false;
        }

        // We're done exposing
        if (ExposureRequest > 3)
            LOG_INFO("Exposure done, downloading image...");

        if (strcasecmp(gphoto_get_file_extension(gphotodrv), "jpg") == 0 ||
                strcasecmp(gphoto_get_file_extension(gphotodrv), "jpeg") == 0)
        {
            if (read_jpeg(tmpfile, &memptr, &memsize, &naxis, &w, &h))
            {
                LOG_ERROR("Exposure failed to parse jpeg.");
                unlink(tmpfile);
                return false;
            }

            LOGF_DEBUG("read_jpeg: memsize (%d) naxis (%d) w (%d) h (%d) bpp (%d)", memsize, naxis,
                       w, h, bpp);

            SetCCDCapability(GetCCDCapability() & ~CCD_HAS_BAYER);
        }
        else
        {
            char bayer_pattern[8] = {};

            if (read_libraw(tmpfile, &memptr, &memsize, &naxis, &w, &h, &bpp, bayer_pattern))
            {
                LOG_ERROR("Exposure failed to parse raw image.");
                unlink(tmpfile);
                return false;
            }

            LOGF_DEBUG("read_libraw: memsize (%d) naxis (%d) w (%d) h (%d) bpp (%d) bayer pattern (%s)",
                       memsize, naxis, w, h, bpp, bayer_pattern);

            unlink(tmpfile);

            IUSaveText(&BayerT[2], bayer_pattern);
            IDSetText(&BayerTP, nullptr);
            SetCCDCapability(GetCCDCapability() | CCD_HAS_BAYER);
        }

        PrimaryCCD.setImageExtension("fits");

        uint16_t subW = PrimaryCCD.getSubW();
        uint16_t subH = PrimaryCCD.getSubH();

        // If subframing is requested
        // If either axis is less than the image resolution
        // then we subframe, given the OTHER axis is within range as well.
        if ( (subW < w && subH <= h) ||
                (subH < h && subW <= w))
        {
            uint16_t subX = PrimaryCCD.getSubX();
            uint16_t subY = PrimaryCCD.getSubY();

            // Align all boundaries to be even
            // This should fix issues with subframed bayered images.
            //            subX -= subX % 2;
            //            subY -= subY % 2;
            //            subW -= subW % 2;
            //            subH -= subH % 2;

            int subFrameSize     = subW * subH * bpp / 8 * ((naxis == 3) ? 3 : 1);
            int oneFrameSize     = subW * subH * bpp / 8;
            //uint8_t * subframeBuf = new uint8_t[subFrameSize];

            int lineW  = subW * bpp / 8;

            LOGF_DEBUG("Subframing... subFrameSize: %d - oneFrameSize: %d - subX: %d - subY: %d - subW: %d - subH: %d",
                       subFrameSize, oneFrameSize,
                       subX, subY, subW, subH);

            if (naxis == 2)
            {
                for (int i = subY; i < subY + subH; i++)
                    memcpy(memptr + (i - subY) * lineW, memptr + (i * w + subX) * bpp / 8, lineW);
                //memcpy(subframeBuf + (i - subY) * lineW, memptr + (i * w + subX) * bpp / 8, lineW);
            }
            else
            {
                //                uint8_t * subR = subframeBuf;
                //                uint8_t * subG = subframeBuf + oneFrameSize;
                //                uint8_t * subB = subframeBuf + oneFrameSize * 2;
                uint8_t * subR = memptr;
                uint8_t * subG = memptr + oneFrameSize;
                uint8_t * subB = memptr + oneFrameSize * 2;

                uint8_t * startR = memptr;
                uint8_t * startG = memptr + (w * h * bpp / 8);
                uint8_t * startB = memptr + (w * h * bpp / 8 * 2);

                for (int i = subY; i < subY + subH; i++)
                {
                    memcpy(subR + (i - subY) * lineW, startR + (i * w + subX) * bpp / 8, lineW);
                    memcpy(subG + (i - subY) * lineW, startG + (i * w + subX) * bpp / 8, lineW);
                    memcpy(subB + (i - subY) * lineW, startB + (i * w + subX) * bpp / 8, lineW);
                }
            }

            PrimaryCCD.setFrameBuffer(memptr);
            PrimaryCCD.setFrameBufferSize(memsize, false);
            PrimaryCCD.setResolution(w, h);
            PrimaryCCD.setFrame(subX, subY, subW, subH);
            PrimaryCCD.setNAxis(naxis);
            PrimaryCCD.setBPP(bpp);

            ExposureComplete(&PrimaryCCD);

            // Restore old pointer and release memory
            //PrimaryCCD.setFrameBuffer(memptr);
            //PrimaryCCD.setFrameBufferSize(memsize, false);
            //delete [] (subframeBuf);
        }
        else
        {
            if (PrimaryCCD.getSubW() != 0 && (w > PrimaryCCD.getSubW() || h > PrimaryCCD.getSubH()))
                LOGF_WARN("Camera image size (%dx%d) is less than requested size (%d,%d). Purge configuration and update frame size to match camera size.", w, h, PrimaryCCD.getSubW(), PrimaryCCD.getSubH());

            PrimaryCCD.setFrame(0, 0, w, h);
            PrimaryCCD.setFrameBuffer(memptr);
            PrimaryCCD.setFrameBufferSize(memsize, false);
            PrimaryCCD.setResolution(w, h);
            PrimaryCCD.setNAxis(naxis);
            PrimaryCCD.setBPP(bpp);

            ExposureComplete(&PrimaryCCD);
        }
    }
    // Read Native image AS IS
    else
    {
        int rc = gphoto_read_exposure(gphotodrv);

        if (rc != 0)
        {
            LOG_ERROR("Failed to expose.");
            if (strstr(gphoto_get_manufacturer(gphotodrv), "Canon") && mMirrorLockN[0].value == 0.0)
                DEBUG(INDI::Logger::DBG_WARNING,
                      "If your camera mirror lock is enabled, you must set a value for the mirror locking duration.");
            return false;
        }

        // We're done exposing
        if (ExposureRequest > 3)
            LOG_DEBUG("Exposure done, downloading image...");
        uint8_t * newMemptr = nullptr;
        gphoto_get_buffer(gphotodrv, const_cast<const char **>(reinterpret_cast<char **>(&newMemptr)), &memsize);
        // We copy the obtained memory pointer to avoid freeing some gphoto memory
        memptr = static_cast<uint8_t *>(realloc(memptr, memsize));
        memcpy(memptr, newMemptr, memsize);

        gphoto_get_dimensions(gphotodrv, &w, &h);

        PrimaryCCD.setImageExtension(gphoto_get_file_extension(gphotodrv));
        if (w > 0 && h > 0)
            PrimaryCCD.setFrame(0, 0, w, h);
        PrimaryCCD.setFrameBuffer(memptr);
        PrimaryCCD.setFrameBufferSize(memsize, false);
        if (w > 0 && h > 0)
            PrimaryCCD.setResolution(w, h);
        PrimaryCCD.setNAxis(naxis);
        PrimaryCCD.setBPP(bpp);

        ExposureComplete(&PrimaryCCD);
    }

    return true;
}

ISwitch * GPhotoCCD::create_switch(const char * basestr, char ** options, int max_opts, int setidx)
{
    int i;
    ISwitch * sw     = static_cast<ISwitch *>(calloc(sizeof(ISwitch), max_opts));
    ISwitch * one_sw = sw;

    char sw_name[MAXINDINAME];
    char sw_label[MAXINDILABEL];
    ISState sw_state;

    for (i = 0; i < max_opts; i++)
    {
        snprintf(sw_name, MAXINDINAME, "%s%d", basestr, i);
        strncpy(sw_label, options[i], MAXINDILABEL);
        sw_state = (i == setidx) ? ISS_ON : ISS_OFF;

        IUFillSwitch(one_sw++, sw_name, sw_label, sw_state);
    }
    return sw;
}

void GPhotoCCD::UpdateWidget(cam_opt * opt)
{
    switch (opt->widget->type)
    {
        case GP_WIDGET_RADIO:
        case GP_WIDGET_MENU:
            for (int i = 0; i < opt->widget->choice_cnt; i++)
                opt->item.sw[i].s = opt->widget->value.index == i ? ISS_ON : ISS_OFF;
            IDSetSwitch(&opt->prop.sw, nullptr);
            break;
        case GP_WIDGET_TEXT:
            free(opt->item.text.text);
            opt->item.text.text = strdup(opt->widget->value.text);
            IDSetText(&opt->prop.text, nullptr);
            break;
        case GP_WIDGET_TOGGLE:
            if (opt->widget->value.toggle)
            {
                opt->item.sw[0].s = ISS_ON;
                opt->item.sw[1].s = ISS_OFF;
            }
            else
            {
                opt->item.sw[0].s = ISS_OFF;
                opt->item.sw[1].s = ISS_ON;
            }
            IDSetSwitch(&opt->prop.sw, nullptr);
            break;
        case GP_WIDGET_RANGE:
            opt->item.num.value = opt->widget->value.num;
            IDSetNumber(&opt->prop.num, nullptr);
            break;
        case GP_WIDGET_DATE:
            free(opt->item.text.text);
            opt->item.text.text = static_cast<char *>(malloc(MAXINDILABEL));
            strftime(opt->item.text.text, MAXINDILABEL, "%FT%TZ", gmtime(reinterpret_cast<time_t *>(&opt->widget->value.date)));
            IDSetText(&opt->prop.text, nullptr);
            break;
        default:
            delete opt;
            return;
    }
}

void GPhotoCCD::AddWidget(gphoto_widget * widget)
{
    IPerm perm;

    if (!widget)
        return;

    perm = widget->readonly ? IP_RO : IP_RW;

    cam_opt * opt = new cam_opt();
    opt->widget  = widget;

    switch (widget->type)
    {
        case GP_WIDGET_RADIO:
        case GP_WIDGET_MENU:
            opt->item.sw = create_switch(widget->name, widget->choices, widget->choice_cnt, widget->value.index);
            IUFillSwitchVector(&opt->prop.sw, opt->item.sw, widget->choice_cnt, getDeviceName(), widget->name,
                               widget->name, widget->parent, perm, ISR_1OFMANY, 60, IPS_IDLE);
            defineSwitch(&opt->prop.sw);
            break;
        case GP_WIDGET_TEXT:
            IUFillText(&opt->item.text, widget->name, widget->name, widget->value.text);
            IUFillTextVector(&opt->prop.text, &opt->item.text, 1, getDeviceName(), widget->name, widget->name,
                             widget->parent, perm, 60, IPS_IDLE);
            defineText(&opt->prop.text);
            break;
        case GP_WIDGET_TOGGLE:
            opt->item.sw = create_switch(widget->name, static_cast<char **>(on_off), 2, widget->value.toggle ? 0 : 1);
            IUFillSwitchVector(&opt->prop.sw, opt->item.sw, 2, getDeviceName(), widget->name, widget->name,
                               widget->parent, perm, ISR_1OFMANY, 60, IPS_IDLE);
            defineSwitch(&opt->prop.sw);
            break;
        case GP_WIDGET_RANGE:
            IUFillNumber(&opt->item.num, widget->name, widget->name, "%5.2f", widget->min, widget->max, widget->step,
                         widget->value.num);
            IUFillNumberVector(&opt->prop.num, &opt->item.num, 1, getDeviceName(), widget->name, widget->name,
                               widget->parent, perm, 60, IPS_IDLE);
            defineNumber(&opt->prop.num);
            break;
        case GP_WIDGET_DATE:
        {
            //tm = gmtime((time_t *)&widget->value.date);
            //IUFillText(&opt->item.text, widget->name, widget->name, asctime(tm));
            char ts[MAXINDITSTAMP] = {0};
            strftime(ts, MAXINDILABEL, "%FT%TZ", gmtime(reinterpret_cast<time_t *>(&opt->widget->value.date)));
            IUFillText(&opt->item.text, widget->name, widget->name, ts);
            IUFillTextVector(&opt->prop.text, &opt->item.text, 1, getDeviceName(), widget->name, widget->name,
                             widget->parent, perm, 60, IPS_IDLE);
            defineText(&opt->prop.text);
        }
        break;
        default:
            delete opt;
            return;
    }

    CamOptions[widget->name] = opt;
}

void GPhotoCCD::ShowExtendedOptions(void)
{
    gphoto_widget_list * iter;

    iter = gphoto_find_all_widgets(gphotodrv);
    while (iter)
    {
        gphoto_widget * widget = gphoto_get_widget_info(gphotodrv, &iter);
        AddWidget(widget);
    }

    gphoto_show_options(gphotodrv);

    optTID = IEAddTimer(1000, GPhotoCCD::UpdateExtendedOptions, this);
}

void GPhotoCCD::HideExtendedOptions(void)
{
    if (optTID)
    {
        IERmTimer(optTID);
        optTID = 0;
    }

    while (CamOptions.begin() != CamOptions.end())
    {
        cam_opt * opt = (*CamOptions.begin()).second;
        IDDelete(getDeviceName(), (*CamOptions.begin()).first.c_str(), nullptr);

        switch (opt->widget->type)
        {
            case GP_WIDGET_RADIO:
            case GP_WIDGET_MENU:
            case GP_WIDGET_TOGGLE:
                free(opt->item.sw);
                break;
            case GP_WIDGET_TEXT:
            case GP_WIDGET_DATE:
                free(opt->item.text.text);
                break;
            default:
                break;
        }

        delete opt;
        CamOptions.erase(CamOptions.begin());
    }
}

#if 0
IPState GPhotoCCD::MoveFocuser(FocusDirection dir, int speed, uint16_t duration)
{
    if (isSimulation() || speed == 0)
        return IPS_OK;

    // Set speed accordng to duration
    if (duration >= 1000)
        speed = 3;
    else if (duration >= 250)
        speed = 2;
    else
        speed = 1;

    if (dir == FOCUS_INWARD)
        focusSpeed = speed * -1;
    else
        focusSpeed = speed;

    LOGF_DEBUG("Setting focuser speed to %d", focusSpeed);

    FocusTimerNP.s = IPS_BUSY;
    IDSetNumber(&FocusTimerNP, nullptr);

    /*while (duration-->0)
    {
       if ( gphoto_manual_focus(gphotodrv, focusSpeed, errMsg) != GP_OK)
       {
           LOGF_ERROR("Focusing failed: %s", errMsg);
           return IPS_ALERT;
       }
    }*/

    // If we have a view finder, let's turn it on
    if (Streamer->isBusy() == false)
        gphoto_set_view_finder(gphotodrv, true);

    //SetTimer(FOCUS_TIMER);

    char errMsg[MAXRBUF];
    if (gphoto_manual_focus(gphotodrv, focusSpeed, errMsg) != GP_OK)
    {
        LOGF_ERROR("Focusing failed: %s", errMsg);
        return IPS_ALERT;
    }
    else if (Streamer->isBusy() == false)
        gphoto_set_view_finder(gphotodrv, false);

    FocusTimerN[0].value = 0;
    return IPS_OK;
}

bool GPhotoCCD::SetFocuserSpeed(int speed)
{
    if (speed >= FocusSpeedN[0].min && speed <= FocusSpeedN[0].max)
        return true;

    return false;
}
#endif

IPState GPhotoCCD::MoveRelFocuser(FocusDirection dir, uint32_t ticks)
{
    INDI_UNUSED(dir);

    // Reduce by a factor of 10
    double adaptiveTicks = ticks / 10.0;

    double largeStep = adaptiveTicks / (FOCUS_HIGH_MED_RATIO * FOCUS_MED_LOW_RATIO);
    double medStep   = (largeStep - rint(largeStep)) * FOCUS_HIGH_MED_RATIO;
    double lowStep   = (medStep - rint(medStep)) * FOCUS_MED_LOW_RATIO;

    m_TargetLargeStep = rint(fabs(largeStep));
    m_TargetMedStep = rint(fabs(medStep));
    m_TargetLowStep = rint(fabs(lowStep));

    if (m_FocusTimerID > 0)
        RemoveTimer(m_FocusTimerID);

    m_FocusTimerID = IEAddTimer(FOCUS_TIMER, &GPhotoCCD::UpdateFocusMotionHelper, this);

    return IPS_BUSY;
}

void GPhotoCCD::UpdateFocusMotionHelper(void *context)
{
    static_cast<GPhotoCCD*>(context)->UpdateFocusMotionCallback();
}

void GPhotoCCD::UpdateFocusMotionCallback()
{
    char errmsg[MAXRBUF] = {0};
    int focusSpeed = -1;

    if (m_TargetLargeStep > 0)
    {
        m_TargetLargeStep--;
        focusSpeed = IUFindOnSwitchIndex(&FocusMotionSP) == FOCUS_INWARD ? -3 : 3;
    }
    else if (m_TargetMedStep > 0)
    {
        m_TargetMedStep--;
        focusSpeed = IUFindOnSwitchIndex(&FocusMotionSP) == FOCUS_INWARD ? -2 : 2;
    }
    else if (m_TargetLowStep > 0)
    {
        m_TargetLowStep--;
        focusSpeed = IUFindOnSwitchIndex(&FocusMotionSP) == FOCUS_INWARD ? -1 : 1;
    }

    if (gphoto_manual_focus(gphotodrv, focusSpeed, errmsg) != GP_OK)
    {
        LOGF_ERROR("Focusing failed: %s", errmsg);
        FocusRelPosNP.s = IPS_ALERT;
        IDSetNumber(&FocusRelPosNP, nullptr);
        return;
    }

    if (m_TargetLargeStep == 0 && m_TargetMedStep == 0 && m_TargetLowStep == 0)
    {
        FocusRelPosNP.s = IPS_OK;
        IDSetNumber(&FocusRelPosNP, nullptr);
    }
    else
        m_FocusTimerID = IEAddTimer(FOCUS_TIMER, &GPhotoCCD::UpdateFocusMotionHelper, this);
}

bool GPhotoCCD::StartStreaming()
{
    if (livePreviewSP.s == IPS_BUSY)
    {
        LOG_ERROR("Cannot start live video streaming while live preview is on.");
        return false;
    }

    if (gphoto_start_preview(gphotodrv) == GP_OK)
    {
        Streamer->setPixelFormat(INDI_RGB);
        std::unique_lock<std::mutex> guard(liveStreamMutex);
        m_RunLiveStream = true;
        guard.unlock();
        liveViewThread = std::thread(&GPhotoCCD::streamLiveView, this);
        return true;
    }

    return false;

}

bool GPhotoCCD::StopStreaming()
{
    std::unique_lock<std::mutex> guard(liveStreamMutex);
    m_RunLiveStream = false;
    guard.unlock();
    liveViewThread.join();
    return (gphoto_stop_preview(gphotodrv) == GP_OK);
}

void GPhotoCCD::streamLiveView()
{
    //char errMsg[MAXRBUF];
    const char * previewData = nullptr;
    unsigned long int previewSize = 0;
    CameraFile * previewFile = nullptr;

    int rc = gp_file_new(&previewFile);
    if (rc != GP_OK)
    {
        LOGF_ERROR("Error creating gphoto file: %s", gp_result_as_string(rc));
        return;
    }

    char errMsg[MAXRBUF] = {0};
    while (true)
    {
        std::unique_lock<std::mutex> guard(liveStreamMutex);
        if (m_RunLiveStream == false)
            break;
        guard.unlock();

        rc = gphoto_capture_preview(gphotodrv, previewFile, errMsg);
        if (rc != GP_OK)
        {
            //LOGF_DEBUG("%s", errMsg);
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }

        if (rc >= GP_OK)
        {
            rc = gp_file_get_data_and_size(previewFile, &previewData, &previewSize);
            if (rc != GP_OK)
            {
                LOGF_ERROR("Error getting preview image data and size: %s", gp_result_as_string(rc));
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                continue;
            }
        }

        uint8_t * inBuffer = reinterpret_cast<uint8_t *>(const_cast<char *>(previewData));

        //        if (streamSubframeS[1].s == ISS_ON)
        //        {
        //            if (liveVideoWidth <= 0)
        //            {
        //                read_jpeg_size(inBuffer, previewSize, &liveVideoWidth, &liveVideoHeight);
        //                Streamer->setSize(liveVideoWidth, liveVideoHeight);
        //            }

        //            std::unique_lock<std::mutex> ccdguard(ccdBufferLock);
        //            Streamer->newFrame(inBuffer, previewSize);
        //            ccdguard.unlock();
        //            continue;
        //        }

        uint8_t * ccdBuffer      = PrimaryCCD.getFrameBuffer();
        size_t size             = 0;
        int w = 0, h = 0, naxis = 0;

        // Read jpeg from memory
        std::unique_lock<std::mutex> ccdguard(ccdBufferLock);
        rc = read_jpeg_mem(inBuffer, previewSize, &ccdBuffer, &size, &naxis, &w, &h);

        if (rc != 0)
        {
            LOG_ERROR("Error getting live video frame.");
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }

        if (liveVideoWidth <= 0)
        {
            liveVideoWidth = w;
            liveVideoHeight = h;
            Streamer->setSize(liveVideoWidth, liveVideoHeight);
        }

        PrimaryCCD.setFrameBuffer(ccdBuffer);

        // We are done with writing to CCD buffer
        ccdguard.unlock();

        if (naxis != PrimaryCCD.getNAxis())
        {
            if (naxis == 1)
                Streamer->setPixelFormat(INDI_MONO);

            PrimaryCCD.setNAxis(naxis);
        }

        if (PrimaryCCD.getSubW() != w || PrimaryCCD.getSubH() != h)
        {
            Streamer->setSize(w, h);
            PrimaryCCD.setFrame(0, 0, w, h);
        }

        if (PrimaryCCD.getFrameBufferSize() != static_cast<int>(size))
            PrimaryCCD.setFrameBufferSize(size, false);

        Streamer->newFrame(ccdBuffer, size);
    }

    gp_file_unref(previewFile);
}

#if 0
bool GPhotoCCD::startLivePreview()
{
    if (isSimulation())
        return false;

    int rc = GP_OK;
    char errMsg[MAXRBUF];
    const char * previewData = nullptr;
    unsigned long int previewSize = 0;

    CameraFile * previewFile = nullptr;

    rc = gp_file_new(&previewFile);
    if (rc != GP_OK)
    {
        LOGF_ERROR("Error creating gphoto file: %s", gp_result_as_string(rc));
        return false;
    }

    for (int i = 0; i < MAX_RETRIES; i++)
    {
        rc = gphoto_capture_preview(gphotodrv, previewFile, errMsg);
        if (rc == true)
            break;
    }

    if (rc != GP_OK)
    {
        LOGF_ERROR("%s", errMsg);
        return false;
    }

    if (rc >= GP_OK)
    {
        rc = gp_file_get_data_and_size(previewFile, &previewData, &previewSize);
        if (rc != GP_OK)
        {
            LOGF_ERROR("Error getting preview image data and size: %s", gp_result_as_string(rc));
            return false;
        }
    }

    //LOGF_DEBUG("Preview capture size %d bytes.", previewSize);

    char * previewBlob = (char *)previewData;

    imageB->blob    = previewBlob;
    imageB->bloblen = previewSize;
    imageB->size    = previewSize;
    strncpy(imageB->format, "stream_jpeg", MAXINDIBLOBFMT);

    IDSetBLOB(imageBP, nullptr);

    if (previewFile)
    {
        gp_file_unref(previewFile);
        previewFile = nullptr;
    }

    return true;
}
#endif

//bool GPhotoCCD::stopLiveVideo()
//{
//    if (isSimulation())
//        return false;

//    return (gphoto_stop_preview(gphotodrv) == GP_OK);
//}

bool GPhotoCCD::saveConfigItems(FILE * fp)
{
    // First save Device Port
    IUSaveConfigText(fp, &PortTP);

    // Second save the CCD Info property
    IUSaveConfigNumber(fp, PrimaryCCD.getCCDInfo());

    // Save regular CCD properties
    INDI::CCD::saveConfigItems(fp);

    // Mirror Locking
    IUSaveConfigNumber(fp, &mMirrorLockNP);

    // Capture Target
    if (captureTargetSP.s == IPS_OK)
    {
        IUSaveConfigSwitch(fp, &captureTargetSP);
        // SD Card delete?
        IUSaveConfigSwitch(fp, &SDCardImageSP);
    }

    // ISO Settings
    if (mIsoSP.nsp > 0)
        IUSaveConfigSwitch(fp, &mIsoSP);

    // Format Settings
    if (mFormatSP.nsp > 0)
        IUSaveConfigSwitch(fp, &mFormatSP);

    // Transfer Format
    IUSaveConfigSwitch(fp, &transferFormatSP);

    //    // Subframe Stream
    //    IUSaveConfigSwitch(fp, &streamSubframeSP);

    // Force BULB Mode
    IUSaveConfigSwitch(fp, &forceBULBSP);

    return true;
}

void GPhotoCCD::addFITSKeywords(fitsfile * fptr, INDI::CCDChip * targetChip)
{
    INDI::CCD::addFITSKeywords(fptr, targetChip);

    int status = 0;

    if (mIsoSP.nsp > 0)
    {
        ISwitch * onISO = IUFindOnSwitch(&mIsoSP);
        if (onISO)
        {
            int isoSpeed = atoi(onISO->label);
            if (isoSpeed > 0)
                fits_update_key_s(fptr, TUINT, "ISOSPEED", &isoSpeed, "ISO Speed", &status);
        }
    }

    if (isTemperatureSupported)
    {
        fits_update_key_s(fptr, TDOUBLE, "CCD-TEMP", &(TemperatureN[0].value), "CCD Temperature (Celsius)", &status);
    }
}

bool GPhotoCCD::UpdateCCDUploadMode(CCD_UPLOAD_MODE mode)
{
    if (!isSimulation())
        gphoto_set_upload_settings(gphotodrv, mode);
    return true;
}
