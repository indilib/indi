/*
    FLI CCD
    INDI Interface for Finger Lakes Instrument CCDs
    Copyright (C) 2003-2016 Jasem Mutlaq (mutlaqja@ikarustech.com)

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

    2016.05.16: Added CCD Cooler Power (JM)

*/

#include <memory>
#include <time.h>
#include <math.h>
#include <unistd.h>
#include <sys/time.h>

#include "config.h"

#include "indidevapi.h"
#include "eventloop.h"

#include "fli_ccd.h"

#define MAX_CCD_TEMP   45   /* Max CCD temperature */
#define MIN_CCD_TEMP   -55  /* Min CCD temperature */
#define MAX_X_BIN      16   /* Max Horizontal binning */
#define MAX_Y_BIN      16   /* Max Vertical binning */
#define TEMP_THRESHOLD .25  /* Differential temperature threshold (C)*/

std::unique_ptr<FLICCD> fliCCD(new FLICCD());

const flidomain_t Domains[] = { FLIDOMAIN_USB, FLIDOMAIN_SERIAL, FLIDOMAIN_PARALLEL_PORT, FLIDOMAIN_INET };

void ISGetProperties(const char *dev)
{
    fliCCD->ISGetProperties(dev);
}

void ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int num)
{
    fliCCD->ISNewSwitch(dev, name, states, names, num);
}

void ISNewText(const char *dev, const char *name, char *texts[], char *names[], int num)
{
    fliCCD->ISNewText(dev, name, texts, names, num);
}

void ISNewNumber(const char *dev, const char *name, double values[], char *names[], int num)
{
    fliCCD->ISNewNumber(dev, name, values, names, num);
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
    fliCCD->ISSnoopDevice(root);
}

FLICCD::FLICCD()
{
    setVersion(FLI_CCD_VERSION_MAJOR, FLI_CCD_VERSION_MINOR);
}

FLICCD::~FLICCD()
{
    delete [] CameraModeS;
}

const char *FLICCD::getDefaultName()
{
    return (char *)"FLI CCD";
}

bool FLICCD::initProperties()
{
    // Init parent properties first
    INDI::CCD::initProperties();

    IUFillSwitch(&PortS[0], "USB", "USB", ISS_ON);
    IUFillSwitch(&PortS[1], "SERIAL", "Serial", ISS_OFF);
    IUFillSwitch(&PortS[2], "PARALLEL", "Parallel", ISS_OFF);
    IUFillSwitch(&PortS[3], "INET", "INet", ISS_OFF);
    IUFillSwitchVector(&PortSP, PortS, 4, getDeviceName(), "PORTS", "Port", MAIN_CONTROL_TAB, IP_WO, ISR_1OFMANY, 0,
                       IPS_IDLE);

    IUFillText(&CamInfoT[0], "Model", "", "");
    IUFillText(&CamInfoT[1], "HW Rev", "", "");
    IUFillText(&CamInfoT[2], "FW Rev", "", "");
    IUFillTextVector(&CamInfoTP, CamInfoT, 3, getDeviceName(), "Model", "", IMAGE_INFO_TAB, IP_RO, 60, IPS_IDLE);

    IUFillNumber(&CoolerN[0], "CCD_COOLER_VALUE", "Cooling Power (%)", "%+06.2f", 0., 100., .2, 0.0);
    IUFillNumberVector(&CoolerNP, CoolerN, 1, getDeviceName(), "CCD_COOLER_POWER", "Cooling Power", MAIN_CONTROL_TAB,
                       IP_RO, 60, IPS_IDLE);

    // Number of flush pre-exposure
    IUFillNumber(&FlushN[0], "FLUSH_COUNT", "Count", "%.f", 0., 16., 1, 0);
    IUFillNumberVector(&FlushNP, FlushN, 1, getDeviceName(), "CCD_FLUSH_COUNT", "N Flush", OPTIONS_TAB,
                       IP_RW, 60, IPS_IDLE);

    // Background Flushing
    IUFillSwitch(&BackgroundFlushS[0], "ENABLED", "Enabled", ISS_OFF);
    IUFillSwitch(&BackgroundFlushS[1], "DISABLED", "Disabled", ISS_ON);
    IUFillSwitchVector(&BackgroundFlushSP, BackgroundFlushS, 2, getDeviceName(), "CCD_BACKGROUND_FLUSH", "BKG. Flush", OPTIONS_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    SetCCDCapability(CCD_CAN_ABORT | CCD_CAN_BIN | CCD_CAN_SUBFRAME | CCD_HAS_COOLER | CCD_HAS_SHUTTER);

    PrimaryCCD.setMinMaxStep("CCD_EXPOSURE", "CCD_EXPOSURE_VALUE", 0.04, 3600, 1, false);

    addAuxControls();

    return true;
}

void FLICCD::ISGetProperties(const char *dev)
{
    INDI::CCD::ISGetProperties(dev);

    defineSwitch(&PortSP);
}

bool FLICCD::updateProperties()
{
    INDI::CCD::updateProperties();

    if (isConnected())
    {
        defineText(&CamInfoTP);
        defineNumber(&CoolerNP);
        defineNumber(&FlushNP);
        defineSwitch(&BackgroundFlushSP);

        setupParams();

        if (CameraModeS != nullptr)
            defineSwitch(&CameraModeSP);

        timerID = SetTimer(POLLMS);
    }
    else
    {
        deleteProperty(CamInfoTP.name);
        deleteProperty(CoolerNP.name);
        deleteProperty(FlushNP.name);
        deleteProperty(BackgroundFlushSP.name);

        if (CameraModeS != nullptr)
            deleteProperty(CameraModeSP.name);

        rmTimer(timerID);
    }

    return true;
}

bool FLICCD::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        if (!strcmp(name, FlushNP.name))
        {
            int err=0;
            long nflushes = values[0];

            if ((err = FLISetNFlushes(fli_dev, nflushes)))
            {
                LOGF_DEBUG("Error: FLISetNFlushes() failed. %s.", strerror((int)-err));
                FlushNP.s = IPS_ALERT;
                IDSetNumber(&FlushNP, nullptr);
                return true;
            }

            IUUpdateNumber(&FlushNP, values, names, n);
            FlushNP.s = IPS_OK;
            IDSetNumber(&FlushNP, nullptr);
            return true;
        }
    }

    return INDI::CCD::ISNewNumber(dev, name, values, names, n);
}

bool FLICCD::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if (strcmp(dev, getDeviceName()) == 0)
    {
        // Ports
        if (!strcmp(name, PortSP.name))
        {
            if (IUUpdateSwitch(&PortSP, states, names, n) < 0)
                return false;

            PortSP.s = IPS_OK;
            IDSetSwitch(&PortSP, nullptr);
            return true;
        }

        // Background Flushing
        if (!strcmp(name, BackgroundFlushSP.name))
        {
            int err=0;
            bool enabled = !strcmp(IUFindOnSwitchName(states, names, n), "ENABLED");
            if ((err = FLIControlBackgroundFlush(fli_dev, enabled ? FLI_BGFLUSH_START : FLI_BGFLUSH_STOP)))
            {
                LOGF_ERROR("Error: FLIControlBackgroundFlush() %s failed. %s.", (enabled ? "starting" : "stopping"), strerror((int)-err));
                BackgroundFlushSP.s = IPS_ALERT;
                IDSetSwitch(&BackgroundFlushSP, nullptr);
                return true;
            }

            IUUpdateSwitch(&BackgroundFlushSP, states, names, n);
            BackgroundFlushSP.s = IPS_OK;
            IDSetSwitch(&BackgroundFlushSP, nullptr);
            return true;
        }

        // Camera Modes
        if (!strcmp(name, CameraModeSP.name) && CameraModeS != nullptr)
        {
            int currentIndex = IUFindOnSwitchIndex(&CameraModeSP);
            LIBFLIAPI errCode = 0;
            IUUpdateSwitch(&CameraModeSP, states, names, n);
            flimode_t cameraModelIndex = static_cast<flimode_t>(IUFindOnSwitchIndex(&CameraModeSP));
            if ( (errCode = FLISetCameraMode(fli_dev, cameraModelIndex)))
            {
                LOGF_ERROR("Error: FLISetCameraMode(%ld) failed. %s.", cameraModelIndex, strerror((int)-errCode));
                IUResetSwitch(&CameraModeSP);
                CameraModeS[currentIndex].s = ISS_ON;
                CameraModeSP.s = IPS_ALERT;
            }
            else
            {
                LOG_WARN(("Camera mode is updated. Please capture a bias frame now before proceeding further to synchronize the change."));
                CameraModeSP.s = IPS_OK;
            }

            IDSetSwitch(&CameraModeSP, nullptr);
            return true;
        }
    }

    //  Nobody has claimed this, so, ignore it
    return INDI::CCD::ISNewSwitch(dev, name, states, names, n);
}

bool FLICCD::Connect()
{
    int err = 0;

    LOG_DEBUG("Attempting to find FLI CCD...");

    sim = isSimulation();

    if (sim)
    {
        LOG_DEBUG("Simulator used.");
        return true;
    }

    int portSwitchIndex = IUFindOnSwitchIndex(&PortSP);

    if (findFLICCD(Domains[portSwitchIndex]) == false)
    {
        LOG_ERROR("Error: no cameras were detected.");
        return false;
    }

    if ((err = FLIOpen(&fli_dev, FLICam.name, FLIDEVICE_CAMERA | FLICam.domain)))
    {
        LOGF_ERROR("Error: FLIOpen() failed. %s.", strerror((int)-err));
        return false;
    }

    /* Success! */
    LOGF_DEBUG("CCD %s is online.", FLICam.name);
    return true;
}

bool FLICCD::Disconnect()
{
    int err;

    if (sim)
        return true;

    if ((err = FLIClose(fli_dev)))
    {
        LOGF_DEBUG("Error: FLIClose() failed. %s.", strerror((int)-err));
        return false;
    }

    LOG_INFO("CCD is offline.");
    return true;
}

bool FLICCD::setupParams()
{
    int err = 0;

    LOG_DEBUG("Retieving camera parameters...");

    char hw_rev[16]={0}, fw_rev[16]={0};

    //////////////////////
    // 1. Get Camera Model
    //////////////////////

    if (sim)
        IUSaveText(&CamInfoT[0], getDeviceName());
    else if ((err = FLIGetModel(fli_dev, FLICam.model, 32)))
    {
        LOGF_ERROR("FLIGetModel() failed. %s.", strerror((int)-err));
        return false;
    }
    else
    {
        LOGF_DEBUG("FLIGetModel() succeed -> %s", FLICam.model);
        IUSaveText(&CamInfoT[0], FLICam.model);
    }

    ///////////////////////////
    // 2. Get Hardware revision
    ///////////////////////////
    if (sim)
        FLICam.HWRevision = 1;
    else if ((err = FLIGetHWRevision(fli_dev, &FLICam.HWRevision)))
    {
        LOGF_ERROR("FLIGetHWRevision() failed. %s.", strerror((int)-err));
        return false;
    }
    else
    {
        snprintf(hw_rev, 16, "%ld", FLICam.HWRevision);
        IUSaveText(&CamInfoT[1], hw_rev);
        LOGF_DEBUG("FLIGetHWRevision() succeed -> %s", hw_rev);
    }

    ///////////////////////////
    // 3. Get Firmware revision
    ///////////////////////////
    if (sim)
        FLICam.FWRevision = 1;
    else if ((err = FLIGetFWRevision(fli_dev, &FLICam.FWRevision)))
    {
        LOGF_ERROR("FLIGetFWRevision() failed. %s.", strerror((int)-err));
        return false;
    }
    else
    {
        snprintf(fw_rev, 16, "%ld", FLICam.FWRevision);
        IUSaveText(&CamInfoT[2], fw_rev);
        LOGF_DEBUG("FLIGetFWRevision() succeed -> %s", fw_rev);
    }

    IDSetText(&CamInfoTP, nullptr);
    ///////////////////////////
    // 4. Get Pixel size
    ///////////////////////////
    if (sim)
    {
        FLICam.x_pixel_size = 5.4 / 1e6;
        FLICam.y_pixel_size = 5.4 / 1e6;
    }
    else if ((err = FLIGetPixelSize(fli_dev, &FLICam.x_pixel_size, &FLICam.y_pixel_size)))
    {
        LOGF_ERROR("FLIGetPixelSize() failed. %s.", strerror((int)-err));
        return false;
    }
    else
    {
        FLICam.x_pixel_size *= 1e6;
        FLICam.y_pixel_size *= 1e6;
        LOGF_DEBUG("FLIGetPixelSize() succeed -> %g x %g", FLICam.x_pixel_size,
               FLICam.y_pixel_size);
    }

    ///////////////////////////
    // 5. Get array area
    ///////////////////////////
    if (sim)
    {
        FLICam.Array_Area[0] = FLICam.Array_Area[1] = 0;
        FLICam.Array_Area[2]                        = 1280;
        FLICam.Array_Area[3]                        = 1024;
    }
    else if ((err = FLIGetArrayArea(fli_dev, &FLICam.Array_Area[0], &FLICam.Array_Area[1], &FLICam.Array_Area[2],
                                    &FLICam.Array_Area[3])))
    {
        LOGF_ERROR("FLIGetArrayArea() failed. %s.", strerror((int)-err));
        return false;
    }
    else
    {
        LOGF_DEBUG("FLIGetArrayArea() succeed -> %ld x %ld + %ld x %ld", FLICam.Array_Area[0],
               FLICam.Array_Area[1], FLICam.Array_Area[2], FLICam.Array_Area[3]);
    }

    ///////////////////////////
    // 6. Get visible area
    ///////////////////////////
    if (sim)
    {
        FLICam.Visible_Area[0] = FLICam.Visible_Area[1] = 0;
        FLICam.Visible_Area[2]                          = 1280;
        FLICam.Visible_Area[3]                          = 1024;
    }
    else if ((err = FLIGetVisibleArea(fli_dev, &FLICam.Visible_Area[0], &FLICam.Visible_Area[1],
                                      &FLICam.Visible_Area[2], &FLICam.Visible_Area[3])))
    {
        LOGF_ERROR("FLIGetVisibleArea() failed. %s.", strerror((int)-err));
        return false;
    }
    else
    {
        LOGF_DEBUG("FLIGetVisibleArea() succeed -> %ld x %ld + %ld x %ld", FLICam.Visible_Area[0],
               FLICam.Visible_Area[1], FLICam.Visible_Area[2], FLICam.Visible_Area[3]);
    }

    ///////////////////////////
    // 7. Get temperature
    ///////////////////////////
    if (sim)
        FLICam.temperature = 25.0;
    else if ((err = FLIGetTemperature(fli_dev, &FLICam.temperature)))
    {
        LOGF_ERROR("FLIGetTemperature() failed. %s.", strerror((int)-err));
        return false;
    }
    else
    {
        TemperatureN[0].value = FLICam.temperature; /* CCD chip temperatre (degrees C) */
        TemperatureN[0].min   = MIN_CCD_TEMP;
        TemperatureN[0].max   = MAX_CCD_TEMP;
        IUUpdateMinMax(&TemperatureNP);
        IDSetNumber(&TemperatureNP, nullptr);
        LOGF_DEBUG("FLIGetTemperature() succeed -> %f", FLICam.temperature);
    }

    SetCCDParams(FLICam.Visible_Area[2] - FLICam.Visible_Area[0], FLICam.Visible_Area[3] - FLICam.Visible_Area[1], 16,
                 FLICam.x_pixel_size, FLICam.y_pixel_size);

    if (!sim)
    {
        /* Default frame type is NORMAL */
        if ((err = FLISetFrameType(fli_dev, FLI_FRAME_TYPE_NORMAL)))
        {
            LOGF_DEBUG("FLISetFrameType() failed. %s.", strerror((int)-err));
            return false;
        }

        /* X horizontal binning */
        if ((err = FLISetHBin(fli_dev, PrimaryCCD.getBinX())))
        {
            LOGF_ERROR("FLISetBin() failed. %s.", strerror((int)-err));
            return false;
        }

        /* Y vertical binning */
        if ((err = FLISetVBin(fli_dev, PrimaryCCD.getBinY())))
        {
            LOGF_ERROR("FLISetVBin() failed. %s.", strerror((int)-err));
            return false;
        }
    }

    int nbuf = PrimaryCCD.getXRes() * PrimaryCCD.getYRes() * PrimaryCCD.getBPP() / 8;
    PrimaryCCD.setFrameBufferSize(nbuf);

    ///////////////////////////
    // 8. Get Modes
    ///////////////////////////
    std::vector<std::string> cameraModeValues;
    flimode_t index = 0;
    char cameraModeValue[MAXINDILABEL];
    while ( FLIGetCameraModeString(fli_dev, index, cameraModeValue, MAXINDILABEL ) == 0)
    {
        cameraModeValues.push_back(cameraModeValue);
        index++;
    }

    if (index > 0)
    {
        if (CameraModeS)
            delete [] CameraModeS;
        CameraModeS = new ISwitch[index];
        for (int i=0; i < index; i++)
            IUFillSwitch(CameraModeS+i, cameraModeValues.at(i).c_str(), cameraModeValues.at(i).c_str(), ISS_OFF);

        IUFillSwitchVector(&CameraModeSP, CameraModeS, index, getDeviceName(), "CAMERA_MODES", "Modes", OPTIONS_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);
        FLIGetCameraMode(fli_dev, &index);
        CameraModeS[index].s = ISS_ON;
    }

    return true;
}

int FLICCD::SetTemperature(double temperature)
{
    int err = 0;

    if (!sim && (err = FLISetTemperature(fli_dev, temperature)))
    {
        LOGF_ERROR("FLISetTemperature() failed. %s.", strerror((int)-err));
        return -1;
    }

    FLICam.temperature = temperature;

    LOGF_INFO("Setting CCD temperature to %.2f C", temperature);
    return 0;
}

bool FLICCD::StartExposure(float duration)
{
    int err = 0;

    if (PrimaryCCD.getFrameType() == INDI::CCDChip::BIAS_FRAME)
    {
        // TODO check if this work with the SDK
        duration = 0;
    }

    if (!sim)
    {
        if ((err = FLISetExposureTime(fli_dev, (long)(duration * 1000))))
        {
            LOGF_ERROR("FLISetExposureTime() failed. %s.", strerror((int)-err));
            return false;
        }
        if ((err = FLIExposeFrame(fli_dev)))
        {
            LOGF_ERROR("FLIExposeFrame() failed. %s.", strerror((int)-err));
            return false;
        }
    }

    PrimaryCCD.setExposureDuration(duration);
    ExposureRequest = duration;

    gettimeofday(&ExpStart, nullptr);
    LOGF_DEBUG("Taking a %g seconds frame...", ExposureRequest);

    InExposure = true;
    return true;
}

bool FLICCD::AbortExposure()
{
    int err = 0;

    if (!sim && (err = FLICancelExposure(fli_dev)))
    {
        LOGF_ERROR("FLICancelExposure() failed. %s.", strerror((int)-err));
        return false;
    }

    InExposure = false;
    return true;
}

bool FLICCD::UpdateCCDFrameType(INDI::CCDChip::CCD_FRAME fType)
{    
    if (sim)
        return true;

    int err = 0;
    switch (fType)
    {
        case INDI::CCDChip::BIAS_FRAME:
        case INDI::CCDChip::DARK_FRAME:
            if ((err = FLISetFrameType(fli_dev, FLI_FRAME_TYPE_DARK)))
            {
                LOGF_ERROR("FLISetFrameType() failed. %s.", strerror((int)-err));
                return false;
            }
            break;

        case INDI::CCDChip::LIGHT_FRAME:
        case INDI::CCDChip::FLAT_FRAME:
            if ((err = FLISetFrameType(fli_dev, FLI_FRAME_TYPE_NORMAL)))
            {
                LOGF_ERROR("FLISetFrameType() failed. %s.", strerror((int)-err));
                return false;
            }
            break;
    }

    return true;
}

bool FLICCD::UpdateCCDFrame(int x, int y, int w, int h)
{
    int err = 0;

    long bin_right  = x + (w / PrimaryCCD.getBinX());
    long bin_bottom = y + (h / PrimaryCCD.getBinY());

    if ( (x+w) > PrimaryCCD.getXRes())
    {
        LOGF_ERROR("Error: invalid frame requested (%d,%d) size(%d,%d)", x, y, w, h);
        return false;
    }
    else if ( (y+h) > PrimaryCCD.getYRes())
    {
        LOGF_ERROR("Error: invalid frame requested (%d,%d) size(%d,%d)", x, y, w, h);
        return false;
    }

    LOGF_DEBUG("Binning (%dx%d). Final FLI image area is (%d, %d), (%ld, %ld). Size (%dx%d)", PrimaryCCD.getBinX(), PrimaryCCD.getBinY(),
           x, y, bin_right, bin_bottom, w / PrimaryCCD.getBinX(), h / PrimaryCCD.getBinY());

    if (!sim && (err = FLISetImageArea(fli_dev, x, y, bin_right, bin_bottom)))
    {
        LOGF_ERROR("FLISetImageArea() failed. %s.", strerror((int)-err));
        return false;
    }

    // Set UNBINNED coords
    PrimaryCCD.setFrame(x, y, w, h);

    int nbuf = (w / PrimaryCCD.getBinX()) * (h / PrimaryCCD.getBinY()) * (PrimaryCCD.getBPP() / 8);
    PrimaryCCD.setFrameBufferSize(nbuf);

    return true;
}

bool FLICCD::UpdateCCDBin(int binx, int biny)
{
    int err = 0;

    /* X horizontal binning */
    if (!sim && (err = FLISetHBin(fli_dev, binx)))
    {
        LOGF_ERROR("FLISetBin() failed. %s.", strerror((int)-err));
        return false;
    }

    /* Y vertical binning */
    if (!sim && (err = FLISetVBin(fli_dev, biny)))
    {
        LOGF_ERROR("FLISetVBin() failed. %s.", strerror((int)-err));
        return false;
    }

    PrimaryCCD.setBin(binx, biny);

    return UpdateCCDFrame(PrimaryCCD.getSubX(), PrimaryCCD.getSubY(), PrimaryCCD.getSubW(), PrimaryCCD.getSubH());
}

// Downloads the image from the CCD.
bool FLICCD::grabImage()
{
    int err        = 0;
    uint8_t *image = PrimaryCCD.getFrameBuffer();
    int row_size   = PrimaryCCD.getSubW() / PrimaryCCD.getBinX() * PrimaryCCD.getBPP() / 8;
    int width      = PrimaryCCD.getSubW() / PrimaryCCD.getBinX();
    int height     = PrimaryCCD.getSubH() / PrimaryCCD.getBinY();

    if (sim)
    {
        for (int i = 0; i < height; i++)
            for (int j = 0; j < row_size; j++)
                image[i * row_size + j] = rand() % 255;
    }
    else
    {
        bool success = true;
        for (int i = 0; i < height; i++)
        {
            if ((err = FLIGrabRow(fli_dev, image + (i * row_size), width)))
            {
                /* print this error once but read to the end to flush the array */
                if (success)
                {
                    LOGF_ERROR("FLIGrabRow() failed at row %d. %s.", i, strerror((int)-err));
                    success = false;
                }
            }
        }

        if (!success)
            return false;
    }

    LOG_INFO("Download complete.");

    ExposureComplete(&PrimaryCCD);

    return true;
}

void FLICCD::TimerHit()
{
    int timerID     = -1;
    int err         = 0;
    long timeleft   = 0;
    double ccdTemp  = 0;
    double ccdPower = 0;
    long camera_status = 0;

    if (isConnected() == false)
        return; //  No need to reset timer if we are not connected anymore

    if (InExposure)
    {
        if (sim)
        {
            PrimaryCCD.setExposureLeft(0);
            InExposure = false;
            grabImage();
        }
        else
        {
	    if ((err = FLIGetDeviceStatus(fli_dev, &camera_status))) 
            {
               LOGF_ERROR("FLIGetDeviceStatus() failed. %s.", strerror((int)-err));
               SetTimer(POLLMS);
               return;
            }
            LOGF_DEBUG("FLIGetDeviceStatus() succeed -> %ld", camera_status);
 
            if ((err = FLIGetExposureStatus(fli_dev, &timeleft)))
            {
                LOGF_ERROR("FLIGetExposureStatus() failed. %s.", strerror((int)-err));
                SetTimer(POLLMS);
                return;
            }
            LOGF_DEBUG("FLIGetExposureStatus() succeed -> %ld", timeleft);
          
            if (!sim && (((camera_status == FLI_CAMERA_STATUS_UNKNOWN) && (timeleft == 0)) ||
                               ((camera_status != FLI_CAMERA_STATUS_UNKNOWN) && ((camera_status & FLI_CAMERA_DATA_READY) != 0))))
            {

                /* We're done exposing */
                LOG_INFO("Exposure done, downloading image...");

                PrimaryCCD.setExposureLeft(0);
                InExposure = false;
                /* grab and save image */
                grabImage();
            }
            else
            {
                LOGF_DEBUG("Exposure in progress. Time left: %ld seconds", timeleft / 1000);
                PrimaryCCD.setExposureLeft(timeleft / 1000.0);
            }
        }
    }

    switch (TemperatureNP.s)
    {
        case IPS_IDLE:
        case IPS_OK:
            if (sim == false)
            {
                if ((err = FLIGetTemperature(fli_dev, &ccdTemp)))
                {
                    TemperatureNP.s = IPS_IDLE;
                    IDSetNumber(&TemperatureNP, nullptr);
                    LOGF_ERROR("FLIGetTemperature() failed. %s.", strerror((int)-err));
                    break;
                }
                if ((err = FLIGetCoolerPower(fli_dev, &ccdPower)))
                {
                    CoolerNP.s = IPS_IDLE;
                    IDSetNumber(&TemperatureNP, nullptr);
                    IDSetNumber(&TemperatureNP, "FLIGetCoolerPower() failed. %s.", strerror((int)-err));
                    break;
                }
            }

            if (fabs(TemperatureN[0].value - ccdTemp) >= TEMP_THRESHOLD)
            {
                TemperatureN[0].value = ccdTemp;
                IDSetNumber(&TemperatureNP, nullptr);
            }

            if (fabs(CoolerN[0].value - ccdPower) >= TEMP_THRESHOLD)
            {
                CoolerN[0].value = ccdPower;
                CoolerNP.s       = TemperatureNP.s;
                IDSetNumber(&CoolerNP, nullptr);
            }
            break;

        case IPS_BUSY:
            if (sim)
            {
                ccdTemp               = FLICam.temperature;
                TemperatureN[0].value = ccdTemp;
            }
            else
            {
                if ((err = FLIGetTemperature(fli_dev, &ccdTemp)))
                {
                    TemperatureNP.s = IPS_IDLE;
                    IDSetNumber(&TemperatureNP, nullptr);
                    LOGF_ERROR("FLIGetTemperature() failed. %s.", strerror((int)-err));
                    break;
                }

                if ((err = FLIGetCoolerPower(fli_dev, &ccdPower)))
                {
                    CoolerNP.s = IPS_IDLE;
                    IDSetNumber(&TemperatureNP, "FLIGetCoolerPower() failed. %s.", strerror((int)-err));
                    break;
                }
            }

            if (fabs(FLICam.temperature - ccdTemp) <= TEMP_THRESHOLD)
            {
                TemperatureNP.s = IPS_OK;
                IDSetNumber(&TemperatureNP, nullptr);
            }

            if (fabs(CoolerN[0].value - ccdPower) >= TEMP_THRESHOLD)
            {
                CoolerN[0].value = ccdPower;
                CoolerNP.s       = TemperatureNP.s;
                IDSetNumber(&CoolerNP, nullptr);
            }

            TemperatureN[0].value = ccdTemp;
            IDSetNumber(&TemperatureNP, nullptr);
            break;

        case IPS_ALERT:
            break;
    }

    if (timerID == -1)
        SetTimer(POLLMS);
    return;
}

bool FLICCD::findFLICCD(flidomain_t domain)
{
    char **names;
    long err;

    LOGF_DEBUG("In find Camera, the domain is %ld", domain);

    if ((err = FLIList(domain | FLIDEVICE_CAMERA, &names)))
    {
        LOGF_ERROR("FLIList() failed. %s", strerror((int)-err));
        return false;
    }

    if (names != nullptr && names[0] != nullptr)
    {
        LOGF_DEBUG("Devices list: %s", names);

        for (int i = 0; names[i] != nullptr; i++)
        {
            for (int j = 0; names[i][j] != '\0'; j++)
                if (names[i][j] == ';')
                {
                    names[i][j] = '\0';
                    break;
                }
        }

        FLICam.domain = domain;

        switch (domain)
        {
            case FLIDOMAIN_PARALLEL_PORT:
                FLICam.dname = strdup("parallel port");
                break;

            case FLIDOMAIN_USB:
                FLICam.dname = strdup("USB");
                break;

            case FLIDOMAIN_SERIAL:
                FLICam.dname = strdup("serial");
                break;

            case FLIDOMAIN_INET:
                FLICam.dname = strdup("inet");
                break;

            default:
                FLICam.dname = strdup("Unknown domain");
        }

        FLICam.name = strdup(names[0]);

        if ((err = FLIFreeList(names)))
        {
            LOGF_ERROR("FLIFreeList() failed. %s.", strerror((int)-err));
            return false;
        }

    } /* end if */
    else
    {
        LOG_ERROR("FLIList returned empty result!");

        if ((err = FLIFreeList(names)))
        {
            LOGF_ERROR("FLIFreeList() failed. %s.", strerror((int)-err));
            return false;
        }

        return false;
    }

    LOG_DEBUG("FindFLICCD() finished successfully.");

    return true;
}

bool FLICCD::saveConfigItems(FILE *fp)
{
    // Save CCD Config
    INDI::CCD::saveConfigItems(fp);

    IUSaveConfigNumber(fp, &FlushNP);
    IUSaveConfigSwitch(fp, &BackgroundFlushSP);

    if (CameraModeS)
        IUSaveConfigSwitch(fp, &CameraModeSP);

    return true;
}

void FLICCD::debugTriggered(bool enable)
{
    if (enable)
        FLISetDebugLevel(nullptr, FLIDEBUG_INFO);
    else
        FLISetDebugLevel(nullptr, FLIDEBUG_WARN);
}
