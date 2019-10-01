/*******************************************************************************
 Copyright(c) 2010-2018 Jasem Mutlaq. All rights reserved.

 Copyright(c) 2010, 2011 Gerry Rozema. All rights reserved.

 Rapid Guide support added by CloudMakers, s. r. o.
 Copyright(c) 2013 CloudMakers, s. r. o. All rights reserved.

 Star detection algorithm is based on PHD Guiding by Craig Stark
 Copyright (c) 2006-2010 Craig Stark. All rights reserved.

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Library General Public
 License version 2 as published by the Free Software Foundation.

 This library is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 Library General Public License for more details.

 You should have received a copy of the GNU Library General Public License
 along with this library; see the file COPYING.LIB.  If not, write to
 the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 Boston, MA 02110-1301, USA.
*******************************************************************************/

// use 64-bit values when calling stat()
#define _FILE_OFFSET_BITS 64

#include "indiccd.h"

#include "fpack/fpack.h"
#include "indicom.h"
#include "stream/streammanager.h"
#include "locale_compat.h"

#include <fitsio.h>

#include <libnova/julian_day.h>
#include <libnova/precession.h>
#include <libnova/airmass.h>
#include <libnova/transform.h>
#include <libnova/ln_types.h>

#include <cmath>
#include <regex>

#include <dirent.h>
#include <cerrno>
#include <cstdlib>
#include <zlib.h>
#include <sys/stat.h>

const char * IMAGE_SETTINGS_TAB = "Image Settings";
const char * IMAGE_INFO_TAB     = "Image Info";
const char * GUIDE_HEAD_TAB     = "Guider Head";
const char * RAPIDGUIDE_TAB     = "Rapid Guide";

#ifdef HAVE_WEBSOCKET
uint16_t INDIWSServer::m_global_port = 11623;
#endif

// Create dir recursively
static int _ccd_mkdir(const char * dir, mode_t mode)
{
    char tmp[PATH_MAX];
    char * p = nullptr;
    size_t len;

    snprintf(tmp, sizeof(tmp), "%s", dir);
    len = strlen(tmp);
    if (tmp[len - 1] == '/')
        tmp[len - 1] = 0;
    for (p = tmp + 1; *p; p++)
        if (*p == '/')
        {
            *p = 0;
            if (mkdir(tmp, mode) == -1 && errno != EEXIST)
                return -1;
            *p = '/';
        }
    if (mkdir(tmp, mode) == -1 && errno != EEXIST)
        return -1;

    return 0;
}

namespace INDI
{

CCD::CCD()
{
    //ctor
    capability = 0;

    InExposure              = false;
    InGuideExposure         = false;
    //RapidGuideEnabled       = false;
    //GuiderRapidGuideEnabled = false;
    m_ValidCCDRotation        = false;

    AutoLoop         = false;
    SendImage        = false;
    ShowMarker       = false;
    GuiderAutoLoop   = false;
    GuiderSendImage  = false;
    GuiderShowMarker = false;

    ExposureTime       = 0.0;
    GuiderExposureTime = 0.0;
    CurrentFilterSlot  = -1;

    RA              = std::numeric_limits<double>::quiet_NaN();
    Dec             = std::numeric_limits<double>::quiet_NaN();
    J2000RA         = std::numeric_limits<double>::quiet_NaN();
    J2000DE         = std::numeric_limits<double>::quiet_NaN();
    MPSAS           = std::numeric_limits<double>::quiet_NaN();
    RotatorAngle    = std::numeric_limits<double>::quiet_NaN();
    Airmass         = std::numeric_limits<double>::quiet_NaN();
    Latitude        = std::numeric_limits<double>::quiet_NaN();
    Longitude       = std::numeric_limits<double>::quiet_NaN();
    primaryAperture = primaryFocalLength = guiderAperture = guiderFocalLength - 1;
}

CCD::~CCD()
{
}

void CCD::SetCCDCapability(uint32_t cap)
{
    capability = cap;

    if (HasST4Port())
        setDriverInterface(getDriverInterface() | GUIDER_INTERFACE);
    else
        setDriverInterface(getDriverInterface() & ~GUIDER_INTERFACE);

    if (HasStreaming() && Streamer.get() == nullptr)
    {
        Streamer.reset(new StreamManager(this));
        Streamer->initProperties();
    }
}

bool CCD::initProperties()
{
    DefaultDevice::initProperties(); //  let the base class flesh in what it wants

    // CCD Temperature
    IUFillNumber(&TemperatureN[0], "CCD_TEMPERATURE_VALUE", "Temperature (C)", "%5.2f", -50.0, 50.0, 0., 0.);
    IUFillNumberVector(&TemperatureNP, TemperatureN, 1, getDeviceName(), "CCD_TEMPERATURE", "Temperature",
                       MAIN_CONTROL_TAB, IP_RW, 60, IPS_IDLE);

    /**********************************************/
    /**************** Primary Chip ****************/
    /**********************************************/

    // Primary CCD Region-Of-Interest (ROI)
    IUFillNumber(&PrimaryCCD.ImageFrameN[CCDChip::FRAME_X], "X", "Left ", "%4.0f", 0, 0.0, 0, 0);
    IUFillNumber(&PrimaryCCD.ImageFrameN[CCDChip::FRAME_Y], "Y", "Top", "%4.0f", 0, 0, 0, 0);
    IUFillNumber(&PrimaryCCD.ImageFrameN[CCDChip::FRAME_W], "WIDTH", "Width", "%4.0f", 0, 0.0, 0, 0.0);
    IUFillNumber(&PrimaryCCD.ImageFrameN[CCDChip::FRAME_H], "HEIGHT", "Height", "%4.0f", 0, 0, 0, 0.0);
    IUFillNumberVector(&PrimaryCCD.ImageFrameNP, PrimaryCCD.ImageFrameN, 4, getDeviceName(), "CCD_FRAME", "Frame",
                       IMAGE_SETTINGS_TAB, IP_RW, 60, IPS_IDLE);

    // Primary CCD Frame Type
    IUFillSwitch(&PrimaryCCD.FrameTypeS[CCDChip::LIGHT_FRAME], "FRAME_LIGHT", "Light", ISS_ON);
    IUFillSwitch(&PrimaryCCD.FrameTypeS[CCDChip::BIAS_FRAME], "FRAME_BIAS", "Bias", ISS_OFF);
    IUFillSwitch(&PrimaryCCD.FrameTypeS[CCDChip::DARK_FRAME], "FRAME_DARK", "Dark", ISS_OFF);
    IUFillSwitch(&PrimaryCCD.FrameTypeS[CCDChip::FLAT_FRAME], "FRAME_FLAT", "Flat", ISS_OFF);
    IUFillSwitchVector(&PrimaryCCD.FrameTypeSP, PrimaryCCD.FrameTypeS, 4, getDeviceName(), "CCD_FRAME_TYPE",
                       "Frame Type", IMAGE_SETTINGS_TAB, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);

    // Primary CCD Exposure
    IUFillNumber(&PrimaryCCD.ImageExposureN[0], "CCD_EXPOSURE_VALUE", "Duration (s)", "%5.2f", 0.01, 3600, 1.0, 1.0);
    IUFillNumberVector(&PrimaryCCD.ImageExposureNP, PrimaryCCD.ImageExposureN, 1, getDeviceName(), "CCD_EXPOSURE",
                       "Expose", MAIN_CONTROL_TAB, IP_RW, 60, IPS_IDLE);

    // Primary CCD Abort
    IUFillSwitch(&PrimaryCCD.AbortExposureS[0], "ABORT", "Abort", ISS_OFF);
    IUFillSwitchVector(&PrimaryCCD.AbortExposureSP, PrimaryCCD.AbortExposureS, 1, getDeviceName(), "CCD_ABORT_EXPOSURE",
                       "Expose Abort", MAIN_CONTROL_TAB, IP_RW, ISR_ATMOST1, 60, IPS_IDLE);

    // Primary CCD Binning
    IUFillNumber(&PrimaryCCD.ImageBinN[0], "HOR_BIN", "X", "%2.0f", 1, 4, 1, 1);
    IUFillNumber(&PrimaryCCD.ImageBinN[1], "VER_BIN", "Y", "%2.0f", 1, 4, 1, 1);
    IUFillNumberVector(&PrimaryCCD.ImageBinNP, PrimaryCCD.ImageBinN, 2, getDeviceName(), "CCD_BINNING", "Binning",
                       IMAGE_SETTINGS_TAB, IP_RW, 60, IPS_IDLE);

    // Primary CCD Info
    IUFillNumber(&PrimaryCCD.ImagePixelSizeN[CCDChip::CCD_MAX_X], "CCD_MAX_X", "Max. Width", "%4.0f", 1, 16000, 0, 0);
    IUFillNumber(&PrimaryCCD.ImagePixelSizeN[CCDChip::CCD_MAX_Y], "CCD_MAX_Y", "Max. Height", "%4.0f", 1, 16000, 0, 0);
    IUFillNumber(&PrimaryCCD.ImagePixelSizeN[CCDChip::CCD_PIXEL_SIZE], "CCD_PIXEL_SIZE", "Pixel size (um)", "%5.2f", 1,
                 40, 0, 0);
    IUFillNumber(&PrimaryCCD.ImagePixelSizeN[CCDChip::CCD_PIXEL_SIZE_X], "CCD_PIXEL_SIZE_X", "Pixel size X", "%5.2f", 1,
                 40, 0, 0);
    IUFillNumber(&PrimaryCCD.ImagePixelSizeN[CCDChip::CCD_PIXEL_SIZE_Y], "CCD_PIXEL_SIZE_Y", "Pixel size Y", "%5.2f", 1,
                 40, 0, 0);
    IUFillNumber(&PrimaryCCD.ImagePixelSizeN[CCDChip::CCD_BITSPERPIXEL], "CCD_BITSPERPIXEL", "Bits per pixel", "%3.0f",
                 8, 64, 0, 0);
    IUFillNumberVector(&PrimaryCCD.ImagePixelSizeNP, PrimaryCCD.ImagePixelSizeN, 6, getDeviceName(), "CCD_INFO",
                       "CCD Information", IMAGE_INFO_TAB, IP_RO, 60, IPS_IDLE);

    // Primary CCD Compression Options
    IUFillSwitch(&PrimaryCCD.CompressS[0], "CCD_COMPRESS", "Compress", ISS_OFF);
    IUFillSwitch(&PrimaryCCD.CompressS[1], "CCD_RAW", "Raw", ISS_ON);
    IUFillSwitchVector(&PrimaryCCD.CompressSP, PrimaryCCD.CompressS, 2, getDeviceName(), "CCD_COMPRESSION", "Image",
                       IMAGE_SETTINGS_TAB, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);
    PrimaryCCD.SendCompressed = false;

    // Primary CCD Chip Data Blob
    IUFillBLOB(&PrimaryCCD.FitsB, "CCD1", "Image", "");
    IUFillBLOBVector(&PrimaryCCD.FitsBP, &PrimaryCCD.FitsB, 1, getDeviceName(), "CCD1", "Image Data", IMAGE_INFO_TAB,
                     IP_RO, 60, IPS_IDLE);

    // Bayer
    IUFillText(&BayerT[0], "CFA_OFFSET_X", "X Offset", "0");
    IUFillText(&BayerT[1], "CFA_OFFSET_Y", "Y Offset", "0");
    IUFillText(&BayerT[2], "CFA_TYPE", "Filter", nullptr);
    IUFillTextVector(&BayerTP, BayerT, 3, getDeviceName(), "CCD_CFA", "Bayer Info", IMAGE_INFO_TAB, IP_RW, 60,
                     IPS_IDLE);

    // Reset Frame Settings
    IUFillSwitch(&PrimaryCCD.ResetS[0], "RESET", "Reset", ISS_OFF);
    IUFillSwitchVector(&PrimaryCCD.ResetSP, PrimaryCCD.ResetS, 1, getDeviceName(), "CCD_FRAME_RESET", "Frame Values",
                       IMAGE_SETTINGS_TAB, IP_WO, ISR_1OFMANY, 0, IPS_IDLE);

    /**********************************************/
    /********* Primary Chip Rapid Guide  **********/
    /**********************************************/
#if 0
    IUFillSwitch(&PrimaryCCD.RapidGuideS[0], "ENABLE", "Enable", ISS_OFF);
    IUFillSwitch(&PrimaryCCD.RapidGuideS[1], "DISABLE", "Disable", ISS_ON);
    IUFillSwitchVector(&PrimaryCCD.RapidGuideSP, PrimaryCCD.RapidGuideS, 2, getDeviceName(), "CCD_RAPID_GUIDE",
                       "Rapid Guide", OPTIONS_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    IUFillSwitch(&PrimaryCCD.RapidGuideSetupS[0], "AUTO_LOOP", "Auto loop", ISS_ON);
    IUFillSwitch(&PrimaryCCD.RapidGuideSetupS[1], "SEND_IMAGE", "Send image", ISS_OFF);
    IUFillSwitch(&PrimaryCCD.RapidGuideSetupS[2], "SHOW_MARKER", "Show marker", ISS_OFF);
    IUFillSwitchVector(&PrimaryCCD.RapidGuideSetupSP, PrimaryCCD.RapidGuideSetupS, 3, getDeviceName(),
                       "CCD_RAPID_GUIDE_SETUP", "Rapid Guide Setup", RAPIDGUIDE_TAB, IP_RW, ISR_NOFMANY, 0, IPS_IDLE);

    IUFillNumber(&PrimaryCCD.RapidGuideDataN[0], "GUIDESTAR_X", "Guide star position X", "%5.2f", 0, 1024, 0, 0);
    IUFillNumber(&PrimaryCCD.RapidGuideDataN[1], "GUIDESTAR_Y", "Guide star position Y", "%5.2f", 0, 1024, 0, 0);
    IUFillNumber(&PrimaryCCD.RapidGuideDataN[2], "GUIDESTAR_FIT", "Guide star fit", "%5.2f", 0, 1024, 0, 0);
    IUFillNumberVector(&PrimaryCCD.RapidGuideDataNP, PrimaryCCD.RapidGuideDataN, 3, getDeviceName(),
                       "CCD_RAPID_GUIDE_DATA", "Rapid Guide Data", RAPIDGUIDE_TAB, IP_RO, 60, IPS_IDLE);
#endif

    /**********************************************/
    /***************** Guide Chip *****************/
    /**********************************************/

    IUFillNumber(&GuideCCD.ImageFrameN[CCDChip::FRAME_X], "X", "Left ", "%4.0f", 0, 0, 0, 0);
    IUFillNumber(&GuideCCD.ImageFrameN[CCDChip::FRAME_Y], "Y", "Top", "%4.0f", 0, 0, 0, 0);
    IUFillNumber(&GuideCCD.ImageFrameN[CCDChip::FRAME_W], "WIDTH", "Width", "%4.0f", 0, 0, 0, 0);
    IUFillNumber(&GuideCCD.ImageFrameN[CCDChip::FRAME_H], "HEIGHT", "Height", "%4.0f", 0, 0, 0, 0);
    IUFillNumberVector(&GuideCCD.ImageFrameNP, GuideCCD.ImageFrameN, 4, getDeviceName(), "GUIDER_FRAME", "Frame",
                       GUIDE_HEAD_TAB, IP_RW, 60, IPS_IDLE);

    IUFillNumber(&GuideCCD.ImageBinN[0], "HOR_BIN", "X", "%2.0f", 1, 4, 1, 1);
    IUFillNumber(&GuideCCD.ImageBinN[1], "VER_BIN", "Y", "%2.0f", 1, 4, 1, 1);
    IUFillNumberVector(&GuideCCD.ImageBinNP, GuideCCD.ImageBinN, 2, getDeviceName(), "GUIDER_BINNING", "Binning",
                       GUIDE_HEAD_TAB, IP_RW, 60, IPS_IDLE);

    IUFillNumber(&GuideCCD.ImagePixelSizeN[CCDChip::CCD_MAX_X], "CCD_MAX_X", "Max. Width", "%4.0f", 1, 16000, 0, 0);
    IUFillNumber(&GuideCCD.ImagePixelSizeN[CCDChip::CCD_MAX_Y], "CCD_MAX_Y", "Max. Height", "%4.0f", 1, 16000, 0, 0);
    IUFillNumber(&GuideCCD.ImagePixelSizeN[CCDChip::CCD_PIXEL_SIZE], "CCD_PIXEL_SIZE", "Pixel size (um)", "%5.2f", 1,
                 40, 0, 0);
    IUFillNumber(&GuideCCD.ImagePixelSizeN[CCDChip::CCD_PIXEL_SIZE_X], "CCD_PIXEL_SIZE_X", "Pixel size X", "%5.2f", 1,
                 40, 0, 0);
    IUFillNumber(&GuideCCD.ImagePixelSizeN[CCDChip::CCD_PIXEL_SIZE_Y], "CCD_PIXEL_SIZE_Y", "Pixel size Y", "%5.2f", 1,
                 40, 0, 0);
    IUFillNumber(&GuideCCD.ImagePixelSizeN[CCDChip::CCD_BITSPERPIXEL], "CCD_BITSPERPIXEL", "Bits per pixel", "%3.0f", 8,
                 64, 0, 0);
    IUFillNumberVector(&GuideCCD.ImagePixelSizeNP, GuideCCD.ImagePixelSizeN, 6, getDeviceName(), "GUIDER_INFO",
                       "Guide Info", IMAGE_INFO_TAB, IP_RO, 60, IPS_IDLE);

    IUFillSwitch(&GuideCCD.FrameTypeS[0], "FRAME_LIGHT", "Light", ISS_ON);
    IUFillSwitch(&GuideCCD.FrameTypeS[1], "FRAME_BIAS", "Bias", ISS_OFF);
    IUFillSwitch(&GuideCCD.FrameTypeS[2], "FRAME_DARK", "Dark", ISS_OFF);
    IUFillSwitch(&GuideCCD.FrameTypeS[3], "FRAME_FLAT", "Flat", ISS_OFF);
    IUFillSwitchVector(&GuideCCD.FrameTypeSP, GuideCCD.FrameTypeS, 4, getDeviceName(), "GUIDER_FRAME_TYPE",
                       "Frame Type", GUIDE_HEAD_TAB, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);

    IUFillNumber(&GuideCCD.ImageExposureN[0], "GUIDER_EXPOSURE_VALUE", "Duration (s)", "%5.2f", 0.01, 3600, 1.0, 1.0);
    IUFillNumberVector(&GuideCCD.ImageExposureNP, GuideCCD.ImageExposureN, 1, getDeviceName(), "GUIDER_EXPOSURE",
                       "Guide Head", MAIN_CONTROL_TAB, IP_RW, 60, IPS_IDLE);

    IUFillSwitch(&GuideCCD.AbortExposureS[0], "ABORT", "Abort", ISS_OFF);
    IUFillSwitchVector(&GuideCCD.AbortExposureSP, GuideCCD.AbortExposureS, 1, getDeviceName(), "GUIDER_ABORT_EXPOSURE",
                       "Guide Abort", MAIN_CONTROL_TAB, IP_RW, ISR_ATMOST1, 60, IPS_IDLE);

    IUFillSwitch(&GuideCCD.CompressS[0], "GUIDER_COMPRESS", "Compress", ISS_OFF);
    IUFillSwitch(&GuideCCD.CompressS[1], "GUIDER_RAW", "Raw", ISS_ON);
    IUFillSwitchVector(&GuideCCD.CompressSP, GuideCCD.CompressS, 2, getDeviceName(), "GUIDER_COMPRESSION", "Image",
                       GUIDE_HEAD_TAB, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);
    GuideCCD.SendCompressed = false;

    IUFillBLOB(&GuideCCD.FitsB, "CCD2", "Guider Image", "");
    IUFillBLOBVector(&GuideCCD.FitsBP, &GuideCCD.FitsB, 1, getDeviceName(), "CCD2", "Image Data", IMAGE_INFO_TAB, IP_RO,
                     60, IPS_IDLE);

    /**********************************************/
    /********* Guider Chip Rapid Guide  ***********/
    /**********************************************/

#if 0
    IUFillSwitch(&GuideCCD.RapidGuideS[0], "ENABLE", "Enable", ISS_OFF);
    IUFillSwitch(&GuideCCD.RapidGuideS[1], "DISABLE", "Disable", ISS_ON);
    IUFillSwitchVector(&GuideCCD.RapidGuideSP, GuideCCD.RapidGuideS, 2, getDeviceName(), "GUIDER_RAPID_GUIDE",
                       "Guider Head Rapid Guide", OPTIONS_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    IUFillSwitch(&GuideCCD.RapidGuideSetupS[0], "AUTO_LOOP", "Auto loop", ISS_ON);
    IUFillSwitch(&GuideCCD.RapidGuideSetupS[1], "SEND_IMAGE", "Send image", ISS_OFF);
    IUFillSwitch(&GuideCCD.RapidGuideSetupS[2], "SHOW_MARKER", "Show marker", ISS_OFF);
    IUFillSwitchVector(&GuideCCD.RapidGuideSetupSP, GuideCCD.RapidGuideSetupS, 3, getDeviceName(),
                       "GUIDER_RAPID_GUIDE_SETUP", "Rapid Guide Setup", RAPIDGUIDE_TAB, IP_RW, ISR_NOFMANY, 0,
                       IPS_IDLE);

    IUFillNumber(&GuideCCD.RapidGuideDataN[0], "GUIDESTAR_X", "Guide star position X", "%5.2f", 0, 1024, 0, 0);
    IUFillNumber(&GuideCCD.RapidGuideDataN[1], "GUIDESTAR_Y", "Guide star position Y", "%5.2f", 0, 1024, 0, 0);
    IUFillNumber(&GuideCCD.RapidGuideDataN[2], "GUIDESTAR_FIT", "Guide star fit", "%5.2f", 0, 1024, 0, 0);
    IUFillNumberVector(&GuideCCD.RapidGuideDataNP, GuideCCD.RapidGuideDataN, 3, getDeviceName(),
                       "GUIDER_RAPID_GUIDE_DATA", "Rapid Guide Data", RAPIDGUIDE_TAB, IP_RO, 60, IPS_IDLE);

#endif

    /**********************************************/
    /******************** WCS *********************/
    /**********************************************/

    // WCS Enable/Disable
    IUFillSwitch(&WorldCoordS[0], "WCS_ENABLE", "Enable", ISS_OFF);
    IUFillSwitch(&WorldCoordS[1], "WCS_DISABLE", "Disable", ISS_ON);
    IUFillSwitchVector(&WorldCoordSP, WorldCoordS, 2, getDeviceName(), "WCS_CONTROL", "WCS", WCS_TAB, IP_RW,
                       ISR_1OFMANY, 0, IPS_IDLE);

    IUFillNumber(&CCDRotationN[0], "CCD_ROTATION_VALUE", "Rotation", "%g", -360, 360, 1, 0);
    IUFillNumberVector(&CCDRotationNP, CCDRotationN, 1, getDeviceName(), "CCD_ROTATION", "CCD FOV", WCS_TAB, IP_RW, 60,
                       IPS_IDLE);

    IUFillSwitch(&TelescopeTypeS[TELESCOPE_PRIMARY], "TELESCOPE_PRIMARY", "Primary", ISS_ON);
    IUFillSwitch(&TelescopeTypeS[TELESCOPE_GUIDE], "TELESCOPE_GUIDE", "Guide", ISS_OFF);
    IUFillSwitchVector(&TelescopeTypeSP, TelescopeTypeS, 2, getDeviceName(), "TELESCOPE_TYPE", "Telescope", OPTIONS_TAB,
                       IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    /**********************************************/
    /************** Upload Settings ***************/
    /**********************************************/

    // Upload Mode
    IUFillSwitch(&UploadS[UPLOAD_CLIENT], "UPLOAD_CLIENT", "Client", ISS_ON);
    IUFillSwitch(&UploadS[UPLOAD_LOCAL], "UPLOAD_LOCAL", "Local", ISS_OFF);
    IUFillSwitch(&UploadS[UPLOAD_BOTH], "UPLOAD_BOTH", "Both", ISS_OFF);
    IUFillSwitchVector(&UploadSP, UploadS, 3, getDeviceName(), "UPLOAD_MODE", "Upload", OPTIONS_TAB, IP_RW, ISR_1OFMANY,
                       0, IPS_IDLE);

    // Upload Settings
    IUFillText(&UploadSettingsT[UPLOAD_DIR], "UPLOAD_DIR", "Dir", "");
    IUFillText(&UploadSettingsT[UPLOAD_PREFIX], "UPLOAD_PREFIX", "Prefix", "IMAGE_XXX");
    IUFillTextVector(&UploadSettingsTP, UploadSettingsT, 2, getDeviceName(), "UPLOAD_SETTINGS", "Upload Settings",
                     OPTIONS_TAB, IP_RW, 60, IPS_IDLE);

    // Upload File Path
    IUFillText(&FileNameT[0], "FILE_PATH", "Path", "");
    IUFillTextVector(&FileNameTP, FileNameT, 1, getDeviceName(), "CCD_FILE_PATH", "Filename", IMAGE_INFO_TAB, IP_RO, 60,
                     IPS_IDLE);

    /**********************************************/
    /****************** FITS Header****************/
    /**********************************************/

    IUFillText(&FITSHeaderT[FITS_OBSERVER], "FITS_OBSERVER", "Observer", "Unknown");
    IUFillText(&FITSHeaderT[FITS_OBJECT], "FITS_OBJECT", "Object", "Unknown");
    IUFillTextVector(&FITSHeaderTP, FITSHeaderT, 2, getDeviceName(), "FITS_HEADER", "FITS Header", INFO_TAB, IP_RW, 60,
                     IPS_IDLE);

    /**********************************************/
    /****************** Exposure Looping **********/
    /***************** Primary CCD Only ***********/
#ifdef WITH_EXPOSURE_LOOPING
    IUFillSwitch(&ExposureLoopS[EXPOSURE_LOOP_ON], "LOOP_ON", "Enabled", ISS_OFF);
    IUFillSwitch(&ExposureLoopS[EXPOSURE_LOOP_OFF], "LOOP_OFF", "Disabled", ISS_ON);
    IUFillSwitchVector(&ExposureLoopSP, ExposureLoopS, 2, getDeviceName(), "CCD_EXPOSURE_LOOP", "Rapid Looping", OPTIONS_TAB,
                       IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    // CCD Should loop until the number of frames specified in this property is completed
    IUFillNumber(&ExposureLoopCountN[0], "FRAMES", "Frames", "%.f", 0, 100000, 1, 1);
    IUFillNumberVector(&ExposureLoopCountNP, ExposureLoopCountN, 1, getDeviceName(), "CCD_EXPOSURE_LOOP_COUNT", "Rapid Count", OPTIONS_TAB, IP_RW, 0, IPS_IDLE);
#endif

    /**********************************************/
    /**************** Web Socket ******************/
    /**********************************************/
    IUFillSwitch(&WebSocketS[WEBSOCKET_ENABLED], "WEBSOCKET_ENABLED", "Enabled", ISS_OFF);
    IUFillSwitch(&WebSocketS[WEBSOCKET_DISABLED], "WEBSOCKET_DISABLED", "Disabled", ISS_ON);
    IUFillSwitchVector(&WebSocketSP, WebSocketS, 2, getDeviceName(), "CCD_WEBSOCKET", "Websocket", OPTIONS_TAB,
                       IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    IUFillNumber(&WebSocketSettingsN[WS_SETTINGS_PORT], "WS_SETTINGS_PORT", "Port", "%.f", 0, 50000, 0, 0);
    IUFillNumberVector(&WebSocketSettingsNP, WebSocketSettingsN, 1, getDeviceName(), "CCD_WEBSOCKET_SETTINGS", "WS Settings", OPTIONS_TAB, IP_RW,
                       60, IPS_IDLE);

    /**********************************************/
    /**************** Snooping ********************/
    /**********************************************/

    // Snooped Devices
    IUFillText(&ActiveDeviceT[0], "ACTIVE_TELESCOPE", "Telescope", "Telescope Simulator");
    IUFillText(&ActiveDeviceT[1], "ACTIVE_FOCUSER", "Focuser", "Focuser Simulator");
    IUFillText(&ActiveDeviceT[2], "ACTIVE_FILTER", "Filter", "CCD Simulator");
    IUFillText(&ActiveDeviceT[3], "ACTIVE_SKYQUALITY", "Sky Quality", "SQM");
    IUFillTextVector(&ActiveDeviceTP, ActiveDeviceT, 4, getDeviceName(), "ACTIVE_DEVICES", "Snoop devices", OPTIONS_TAB,
                     IP_RW, 60, IPS_IDLE);

    // Snooped RA/DEC Property
    IUFillNumber(&EqN[0], "RA", "Ra (hh:mm:ss)", "%010.6m", 0, 24, 0, 0);
    IUFillNumber(&EqN[1], "DEC", "Dec (dd:mm:ss)", "%010.6m", -90, 90, 0, 0);
    IUFillNumberVector(&EqNP, EqN, 2, ActiveDeviceT[0].text, "EQUATORIAL_EOD_COORD", "EQ Coord", "Main Control", IP_RW,
                       60, IPS_IDLE);

    // Snoop properties of interest

    // Snoop mount
    IDSnoopDevice(ActiveDeviceT[SNOOP_MOUNT].text, "EQUATORIAL_EOD_COORD");
    IDSnoopDevice(ActiveDeviceT[SNOOP_MOUNT].text, "TELESCOPE_INFO");
    IDSnoopDevice(ActiveDeviceT[SNOOP_MOUNT].text, "GEOGRAPHIC_COORD");

    // Snoop Rotator
    IDSnoopDevice(ActiveDeviceT[SNOOP_ROTATOR].text, "ABS_ROTATOR_ANGLE");

    // Snoop Filter Wheel
    IDSnoopDevice(ActiveDeviceT[SNOOP_FILTER_WHEEL].text, "FILTER_SLOT");
    IDSnoopDevice(ActiveDeviceT[SNOOP_FILTER_WHEEL].text, "FILTER_NAME");

    // Snoop Sky Quality Meter
    IDSnoopDevice(ActiveDeviceT[SNOOP_SQM].text, "SKY_QUALITY");

    // Guider Interface
    initGuiderProperties(getDeviceName(), GUIDE_CONTROL_TAB);

    addPollPeriodControl();

    setDriverInterface(CCD_INTERFACE | GUIDER_INTERFACE);

    return true;
}

void CCD::ISGetProperties(const char * dev)
{
    DefaultDevice::ISGetProperties(dev);

    defineText(&ActiveDeviceTP);
    loadConfig(true, "ACTIVE_DEVICES");

    if (HasStreaming())
        Streamer->ISGetProperties(dev);
}

bool CCD::updateProperties()
{
    //IDLog("CCD UpdateProperties isConnected returns %d %d\n",isConnected(),Connected);
    if (isConnected())
    {
        defineNumber(&PrimaryCCD.ImageExposureNP);

        if (CanAbort())
            defineSwitch(&PrimaryCCD.AbortExposureSP);
        if (CanSubFrame() == false)
            PrimaryCCD.ImageFrameNP.p = IP_RO;

        defineNumber(&PrimaryCCD.ImageFrameNP);
        if (CanBin())
            defineNumber(&PrimaryCCD.ImageBinNP);

        defineText(&FITSHeaderTP);

        if (HasGuideHead())
        {
            defineNumber(&GuideCCD.ImageExposureNP);
            if (CanAbort())
                defineSwitch(&GuideCCD.AbortExposureSP);
            if (CanSubFrame() == false)
                GuideCCD.ImageFrameNP.p = IP_RO;
            defineNumber(&GuideCCD.ImageFrameNP);
        }

        if (HasCooler())
            defineNumber(&TemperatureNP);

        defineNumber(&PrimaryCCD.ImagePixelSizeNP);
        if (HasGuideHead())
        {
            defineNumber(&GuideCCD.ImagePixelSizeNP);
            if (CanBin())
                defineNumber(&GuideCCD.ImageBinNP);
        }
        defineSwitch(&PrimaryCCD.CompressSP);
        defineBLOB(&PrimaryCCD.FitsBP);
        if (HasGuideHead())
        {
            defineSwitch(&GuideCCD.CompressSP);
            defineBLOB(&GuideCCD.FitsBP);
        }
        if (HasST4Port())
        {
            defineNumber(&GuideNSNP);
            defineNumber(&GuideWENP);
        }
        defineSwitch(&PrimaryCCD.FrameTypeSP);

        if (CanBin() || CanSubFrame())
            defineSwitch(&PrimaryCCD.ResetSP);

        if (HasGuideHead())
            defineSwitch(&GuideCCD.FrameTypeSP);

        if (HasBayer())
            defineText(&BayerTP);

#if 0
        defineSwitch(&PrimaryCCD.RapidGuideSP);

        if (HasGuideHead())
            defineSwitch(&GuideCCD.RapidGuideSP);

        if (RapidGuideEnabled)
        {
            defineSwitch(&PrimaryCCD.RapidGuideSetupSP);
            defineNumber(&PrimaryCCD.RapidGuideDataNP);
        }
        if (GuiderRapidGuideEnabled)
        {
            defineSwitch(&GuideCCD.RapidGuideSetupSP);
            defineNumber(&GuideCCD.RapidGuideDataNP);
        }
#endif
        defineSwitch(&TelescopeTypeSP);

        defineSwitch(&WorldCoordSP);
        defineSwitch(&UploadSP);

        if (UploadSettingsT[UPLOAD_DIR].text == nullptr)
            IUSaveText(&UploadSettingsT[UPLOAD_DIR], getenv("HOME"));
        defineText(&UploadSettingsTP);

#ifdef HAVE_WEBSOCKET
        if (HasWebSocket())
            defineSwitch(&WebSocketSP);
#endif

#ifdef WITH_EXPOSURE_LOOPING
        defineSwitch(&ExposureLoopSP);
        defineNumber(&ExposureLoopCountNP);
#endif
    }
    else
    {
        deleteProperty(PrimaryCCD.ImageFrameNP.name);
        deleteProperty(PrimaryCCD.ImagePixelSizeNP.name);

        if (CanBin())
            deleteProperty(PrimaryCCD.ImageBinNP.name);

        deleteProperty(PrimaryCCD.ImageExposureNP.name);
        if (CanAbort())
            deleteProperty(PrimaryCCD.AbortExposureSP.name);
        deleteProperty(PrimaryCCD.FitsBP.name);
        deleteProperty(PrimaryCCD.CompressSP.name);

#if 0
        deleteProperty(PrimaryCCD.RapidGuideSP.name);
        if (RapidGuideEnabled)
        {
            deleteProperty(PrimaryCCD.RapidGuideSetupSP.name);
            deleteProperty(PrimaryCCD.RapidGuideDataNP.name);
        }
#endif

        deleteProperty(FITSHeaderTP.name);

        if (HasGuideHead())
        {
            deleteProperty(GuideCCD.ImageExposureNP.name);
            if (CanAbort())
                deleteProperty(GuideCCD.AbortExposureSP.name);
            deleteProperty(GuideCCD.ImageFrameNP.name);
            deleteProperty(GuideCCD.ImagePixelSizeNP.name);

            deleteProperty(GuideCCD.FitsBP.name);
            if (CanBin())
                deleteProperty(GuideCCD.ImageBinNP.name);
            deleteProperty(GuideCCD.CompressSP.name);
            deleteProperty(GuideCCD.FrameTypeSP.name);

#if 0
            deleteProperty(GuideCCD.RapidGuideSP.name);
            if (GuiderRapidGuideEnabled)
            {
                deleteProperty(GuideCCD.RapidGuideSetupSP.name);
                deleteProperty(GuideCCD.RapidGuideDataNP.name);
            }
#endif
        }
        if (HasCooler())
            deleteProperty(TemperatureNP.name);
        if (HasST4Port())
        {
            deleteProperty(GuideNSNP.name);
            deleteProperty(GuideWENP.name);
        }
        deleteProperty(PrimaryCCD.FrameTypeSP.name);
        if (CanBin() || CanSubFrame())
            deleteProperty(PrimaryCCD.ResetSP.name);
        if (HasBayer())
            deleteProperty(BayerTP.name);
        deleteProperty(TelescopeTypeSP.name);

        if (WorldCoordS[0].s == ISS_ON)
        {
            deleteProperty(CCDRotationNP.name);
        }
        deleteProperty(WorldCoordSP.name);
        deleteProperty(UploadSP.name);
        deleteProperty(UploadSettingsTP.name);

#ifdef HAVE_WEBSOCKET
        if (HasWebSocket())
        {
            deleteProperty(WebSocketSP.name);
            deleteProperty(WebSocketSettingsNP.name);
        }
#endif
#ifdef WITH_EXPOSURE_LOOPING
        deleteProperty(ExposureLoopSP.name);
        deleteProperty(ExposureLoopCountNP.name);
#endif
    }

    // Streamer
    if (HasStreaming())
        Streamer->updateProperties();

    return true;
}

bool CCD::ISSnoopDevice(XMLEle * root)
{
    XMLEle * ep           = nullptr;
    const char * propName = findXMLAttValu(root, "name");

    if (IUSnoopNumber(root, &EqNP) == 0)
    {
        float newra, newdec;
        newra  = EqN[0].value;
        newdec = EqN[1].value;
        if ((newra != RA) || (newdec != Dec))
        {
            //IDLog("RA %4.2f  Dec %4.2f Snooped RA %4.2f  Dec %4.2f\n",RA,Dec,newra,newdec);
            RA  = newra;
            Dec = newdec;
        }
    }
    else if (!strcmp(propName, "TELESCOPE_INFO"))
    {
        for (ep = nextXMLEle(root, 1); ep != nullptr; ep = nextXMLEle(root, 0))
        {
            const char * name = findXMLAttValu(ep, "name");

            if (!strcmp(name, "TELESCOPE_APERTURE"))
            {
                primaryAperture = atof(pcdataXMLEle(ep));
            }
            else if (!strcmp(name, "TELESCOPE_FOCAL_LENGTH"))
            {
                primaryFocalLength = atof(pcdataXMLEle(ep));
            }
            else if (!strcmp(name, "GUIDER_APERTURE"))
            {
                guiderAperture = atof(pcdataXMLEle(ep));
            }
            else if (!strcmp(name, "GUIDER_FOCAL_LENGTH"))
            {
                guiderFocalLength = atof(pcdataXMLEle(ep));
            }
        }
    }
    else if (!strcmp(propName, "FILTER_NAME"))
    {
        FilterNames.clear();

        for (ep = nextXMLEle(root, 1); ep != nullptr; ep = nextXMLEle(root, 0))
            FilterNames.push_back(pcdataXMLEle(ep));
    }
    else if (!strcmp(propName, "FILTER_SLOT"))
    {
        CurrentFilterSlot = -1;
        for (ep = nextXMLEle(root, 1); ep != nullptr; ep = nextXMLEle(root, 0))
            CurrentFilterSlot = atoi(pcdataXMLEle(ep));
    }
    else if (!strcmp(propName, "SKY_QUALITY"))
    {
        for (ep = nextXMLEle(root, 1); ep != nullptr; ep = nextXMLEle(root, 0))
        {
            const char * name = findXMLAttValu(ep, "name");

            if (!strcmp(name, "SKY_BRIGHTNESS"))
            {
                MPSAS = atof(pcdataXMLEle(ep));
                break;
            }
        }
    }
    else if (!strcmp(propName, "ABS_ROTATOR_ANGLE"))
    {
        for (ep = nextXMLEle(root, 1); ep != nullptr; ep = nextXMLEle(root, 0))
        {
            const char * name = findXMLAttValu(ep, "name");

            if (!strcmp(name, "ANGLE"))
            {
                RotatorAngle = atof(pcdataXMLEle(ep));
                break;
            }
        }
    }
    else if (!strcmp(propName, "GEOGRAPHIC_COORD"))
    {
        for (ep = nextXMLEle(root, 1); ep != nullptr; ep = nextXMLEle(root, 0))
        {
            const char * name = findXMLAttValu(ep, "name");

            if (!strcmp(name, "LONG"))
            {
                Longitude = atof(pcdataXMLEle(ep));
                if (Longitude > 180)
                    Longitude -= 360;
            }
            else if (!strcmp(name, "LAT"))
            {
                Latitude = atof(pcdataXMLEle(ep));
            }
        }
    }

    return DefaultDevice::ISSnoopDevice(root);
}

bool CCD::ISNewText(const char * dev, const char * name, char * texts[], char * names[], int n)
{
    //  first check if it's for our device
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        //  This is for our device
        //  Now lets see if it's something we process here
        if (!strcmp(name, ActiveDeviceTP.name))
        {
            ActiveDeviceTP.s = IPS_OK;
            IUUpdateText(&ActiveDeviceTP, texts, names, n);
            IDSetText(&ActiveDeviceTP, nullptr);

            // Update the property name!
            strncpy(EqNP.device, ActiveDeviceT[SNOOP_MOUNT].text, MAXINDIDEVICE);
            if (strlen(ActiveDeviceT[SNOOP_MOUNT].text) > 0)
            {
                IDSnoopDevice(ActiveDeviceT[SNOOP_MOUNT].text, "EQUATORIAL_EOD_COORD");
                IDSnoopDevice(ActiveDeviceT[SNOOP_MOUNT].text, "TELESCOPE_INFO");
                IDSnoopDevice(ActiveDeviceT[SNOOP_MOUNT].text, "GEOGRAPHIC_COORD");
            }
            else
            {
                RA = std::numeric_limits<double>::quiet_NaN();
                Dec = std::numeric_limits<double>::quiet_NaN();
                J2000RA = std::numeric_limits<double>::quiet_NaN();
                J2000DE = std::numeric_limits<double>::quiet_NaN();
                Latitude = std::numeric_limits<double>::quiet_NaN();
                Longitude = std::numeric_limits<double>::quiet_NaN();
                Airmass = std::numeric_limits<double>::quiet_NaN();
            }

            if (strlen(ActiveDeviceT[SNOOP_ROTATOR].text) > 0)
                IDSnoopDevice(ActiveDeviceT[SNOOP_ROTATOR].text, "ABS_ROTATOR_ANGLE");
            else
                MPSAS = std::numeric_limits<double>::quiet_NaN();

            if (strlen(ActiveDeviceT[SNOOP_FILTER_WHEEL].text) > 0)
            {
                IDSnoopDevice(ActiveDeviceT[SNOOP_FILTER_WHEEL].text, "FILTER_SLOT");
                IDSnoopDevice(ActiveDeviceT[SNOOP_FILTER_WHEEL].text, "FILTER_NAME");
            }
            else
            {
                CurrentFilterSlot = -1;
            }

            IDSnoopDevice(ActiveDeviceT[SNOOP_SQM].text, "SKY_QUALITY");

            // Tell children active devices was updated.
            activeDevicesUpdated();

            //  We processed this one, so, tell the world we did it
            return true;
        }

        if (!strcmp(name, BayerTP.name))
        {
            IUUpdateText(&BayerTP, texts, names, n);
            BayerTP.s = IPS_OK;
            IDSetText(&BayerTP, nullptr);
            return true;
        }

        if (!strcmp(name, FITSHeaderTP.name))
        {
            IUUpdateText(&FITSHeaderTP, texts, names, n);
            FITSHeaderTP.s = IPS_OK;
            IDSetText(&FITSHeaderTP, nullptr);
            return true;
        }

        if (!strcmp(name, UploadSettingsTP.name))
        {
            IUUpdateText(&UploadSettingsTP, texts, names, n);
            UploadSettingsTP.s = IPS_OK;
            IDSetText(&UploadSettingsTP, nullptr);
            return true;
        }
    }

    // Streamer
    if (HasStreaming())
        Streamer->ISNewText(dev, name, texts, names, n);

    return DefaultDevice::ISNewText(dev, name, texts, names, n);
}

bool CCD::ISNewNumber(const char * dev, const char * name, double values[], char * names[], int n)
{
    //  first check if it's for our device
    //IDLog("CCD::ISNewNumber %s\n",name);
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        if (!strcmp(name, "CCD_EXPOSURE"))
        {
            if (PrimaryCCD.getFrameType() != CCDChip::BIAS_FRAME &&
                    (values[0] < PrimaryCCD.ImageExposureN[0].min || values[0] > PrimaryCCD.ImageExposureN[0].max))
            {
                LOGF_ERROR("Requested exposure value (%g) seconds out of bounds [%g,%g].",
                           values[0], PrimaryCCD.ImageExposureN[0].min, PrimaryCCD.ImageExposureN[0].max);
                PrimaryCCD.ImageExposureNP.s = IPS_ALERT;
                IDSetNumber(&PrimaryCCD.ImageExposureNP, nullptr);
                return false;
            }

            if (PrimaryCCD.getFrameType() == CCDChip::BIAS_FRAME)
                PrimaryCCD.ImageExposureN[0].value = ExposureTime = PrimaryCCD.ImageExposureN[0].min;
            else
                PrimaryCCD.ImageExposureN[0].value = ExposureTime = values[0];

            // Only abort when busy if we are not already in an exposure loops
            //if (PrimaryCCD.ImageExposureNP.s == IPS_BUSY && ExposureLoopS[EXPOSURE_LOOP_OFF].s == ISS_ON)
            if (PrimaryCCD.ImageExposureNP.s == IPS_BUSY)
            {
                if (CanAbort() && AbortExposure() == false)
                    DEBUG(Logger::DBG_WARNING, "Warning: Aborting exposure failed.");
            }

            if (StartExposure(ExposureTime))
            {
                if (PrimaryCCD.getFrameType() == CCDChip::LIGHT_FRAME && !std::isnan(RA) && !std::isnan(Dec))
                {
                    ln_equ_posn epochPos { 0, 0 }, J2000Pos { 0, 0 };
                    epochPos.ra  = RA * 15.0;
                    epochPos.dec = Dec;

                    // Convert from JNow to J2000
                    ln_get_equ_prec2(&epochPos, ln_get_julian_from_sys(), JD2000, &J2000Pos);

                    J2000RA = J2000Pos.ra / 15.0;
                    J2000DE = J2000Pos.dec;

                    if (!std::isnan(Latitude) && !std::isnan(Longitude))
                    {
                        // Horizontal Coords
                        ln_hrz_posn horizontalPos;
                        ln_lnlat_posn observer;
                        observer.lat = Latitude;
                        observer.lng = Longitude;

                        ln_get_hrz_from_equ(&epochPos, &observer, ln_get_julian_from_sys(), &horizontalPos);
                        Airmass = ln_get_airmass(horizontalPos.alt, 750);
                    }
                }

                PrimaryCCD.ImageExposureNP.s = IPS_BUSY;
                if (ExposureTime * 1000 < POLLMS)
                    POLLMS = ExposureTime * 950;
            }
            else
                PrimaryCCD.ImageExposureNP.s = IPS_ALERT;
            IDSetNumber(&PrimaryCCD.ImageExposureNP, nullptr);
            return true;
        }

        if (!strcmp(name, "GUIDER_EXPOSURE"))
        {
            if (GuideCCD.getFrameType() != CCDChip::BIAS_FRAME &&
                    (values[0] < GuideCCD.ImageExposureN[0].min || values[0] > GuideCCD.ImageExposureN[0].max))
            {
                LOGF_ERROR("Requested guide exposure value (%g) seconds out of bounds [%g,%g].",
                           values[0], GuideCCD.ImageExposureN[0].min, GuideCCD.ImageExposureN[0].max);
                GuideCCD.ImageExposureNP.s = IPS_ALERT;
                IDSetNumber(&GuideCCD.ImageExposureNP, nullptr);
                return false;
            }

            if (GuideCCD.getFrameType() == CCDChip::BIAS_FRAME)
                GuideCCD.ImageExposureN[0].value = GuiderExposureTime = GuideCCD.ImageExposureN[0].min;
            else
                GuideCCD.ImageExposureN[0].value = GuiderExposureTime = values[0];

            GuideCCD.ImageExposureNP.s = IPS_BUSY;
            if (StartGuideExposure(GuiderExposureTime))
                GuideCCD.ImageExposureNP.s = IPS_BUSY;
            else
                GuideCCD.ImageExposureNP.s = IPS_ALERT;
            IDSetNumber(&GuideCCD.ImageExposureNP, nullptr);
            return true;
        }

        if (!strcmp(name, "CCD_BINNING"))
        {
            //  We are being asked to set camera binning
            INumber * np = IUFindNumber(&PrimaryCCD.ImageBinNP, names[0]);
            if (np == nullptr)
            {
                PrimaryCCD.ImageBinNP.s = IPS_ALERT;
                IDSetNumber(&PrimaryCCD.ImageBinNP, nullptr);
                return false;
            }

            int binx, biny;
            if (!strcmp(np->name, "HOR_BIN"))
            {
                binx = values[0];
                biny = values[1];
            }
            else
            {
                binx = values[1];
                biny = values[0];
            }

            if (UpdateCCDBin(binx, biny))
            {
                IUUpdateNumber(&PrimaryCCD.ImageBinNP, values, names, n);
                PrimaryCCD.ImageBinNP.s = IPS_OK;
            }
            else
                PrimaryCCD.ImageBinNP.s = IPS_ALERT;

            IDSetNumber(&PrimaryCCD.ImageBinNP, nullptr);

            return true;
        }

        if (!strcmp(name, "GUIDER_BINNING"))
        {
            //  We are being asked to set camera binning
            INumber * np = IUFindNumber(&GuideCCD.ImageBinNP, names[0]);
            if (np == nullptr)
            {
                GuideCCD.ImageBinNP.s = IPS_ALERT;
                IDSetNumber(&GuideCCD.ImageBinNP, nullptr);
                return false;
            }

            int binx, biny;
            if (!strcmp(np->name, "HOR_BIN"))
            {
                binx = values[0];
                biny = values[1];
            }
            else
            {
                binx = values[1];
                biny = values[0];
            }

            if (UpdateGuiderBin(binx, biny))
            {
                IUUpdateNumber(&GuideCCD.ImageBinNP, values, names, n);
                GuideCCD.ImageBinNP.s = IPS_OK;
            }
            else
                GuideCCD.ImageBinNP.s = IPS_ALERT;

            IDSetNumber(&GuideCCD.ImageBinNP, nullptr);

            return true;
        }

        if (!strcmp(name, "CCD_FRAME"))
        {
            int x = -1, y = -1, w = -1, h = -1;
            for (int i = 0; i < n; i++)
            {
                if (!strcmp(names[i], "X"))
                    x = values[i];
                else if (!strcmp(names[i], "Y"))
                    y = values[i];
                else if (!strcmp(names[i], "WIDTH"))
                    w = values[i];
                else if (!strcmp(names[i], "HEIGHT"))
                    h = values[i];
            }

            DEBUGF(Logger::DBG_DEBUG, "Requested CCD Frame is (%d,%d) (%d x %d)", x, y, w, h);

            if (x < 0 || y < 0 || w <= 0 || h <= 0)
            {
                LOGF_ERROR("Invalid frame requested (%d,%d) (%d x %d)", x, y, w, h);
                PrimaryCCD.ImageFrameNP.s = IPS_ALERT;
                IDSetNumber(&PrimaryCCD.ImageFrameNP, nullptr);
                return true;
            }

            if (UpdateCCDFrame(x, y, w, h))
            {
                PrimaryCCD.ImageFrameNP.s = IPS_OK;
                IUUpdateNumber(&PrimaryCCD.ImageFrameNP, values, names, n);
            }
            else
                PrimaryCCD.ImageFrameNP.s = IPS_ALERT;

            IDSetNumber(&PrimaryCCD.ImageFrameNP, nullptr);
            return true;
        }

        if (!strcmp(name, "GUIDER_FRAME"))
        {
            //  We are being asked to set guide frame
            if (IUUpdateNumber(&GuideCCD.ImageFrameNP, values, names, n) < 0)
                return false;

            GuideCCD.ImageFrameNP.s = IPS_OK;

            DEBUGF(Logger::DBG_DEBUG, "Requested Guide Frame is %4.0f,%4.0f %4.0f x %4.0f", values[0], values[1],
                   values[2], values[4]);

            if (UpdateGuiderFrame(GuideCCD.ImageFrameN[0].value, GuideCCD.ImageFrameN[1].value,
                                  GuideCCD.ImageFrameN[2].value, GuideCCD.ImageFrameN[3].value) == false)
                GuideCCD.ImageFrameNP.s = IPS_ALERT;

            IDSetNumber(&GuideCCD.ImageFrameNP, nullptr);

            return true;
        }

#if 0
        if (!strcmp(name, "CCD_GUIDESTAR"))
        {
            PrimaryCCD.RapidGuideDataNP.s = IPS_OK;
            IUUpdateNumber(&PrimaryCCD.RapidGuideDataNP, values, names, n);
            IDSetNumber(&PrimaryCCD.RapidGuideDataNP, nullptr);
            return true;
        }

        if (!strcmp(name, "GUIDER_GUIDESTAR"))
        {
            GuideCCD.RapidGuideDataNP.s = IPS_OK;
            IUUpdateNumber(&GuideCCD.RapidGuideDataNP, values, names, n);
            IDSetNumber(&GuideCCD.RapidGuideDataNP, nullptr);
            return true;
        }
#endif

        if (!strcmp(name, GuideNSNP.name) || !strcmp(name, GuideWENP.name))
        {
            processGuiderProperties(name, values, names, n);
            return true;
        }

#ifdef WITH_EXPOSURE_LOOPING
        if (!strcmp(name, ExposureLoopCountNP.name))
        {
            IUUpdateNumber(&ExposureLoopCountNP, values, names, n);
            ExposureLoopCountNP.s = IPS_OK;
            IDSetNumber(&ExposureLoopCountNP, nullptr);
            return true;
        }
#endif

        // CCD TEMPERATURE:
        if (!strcmp(name, TemperatureNP.name))
        {
            if (values[0] < TemperatureN[0].min || values[0] > TemperatureN[0].max)
            {
                TemperatureNP.s = IPS_ALERT;
                LOGF_ERROR("Error: Bad temperature value! Range is [%.1f, %.1f] [C].",
                           TemperatureN[0].min, TemperatureN[0].max);
                IDSetNumber(&TemperatureNP, nullptr);
                return false;
            }

            int rc = SetTemperature(values[0]);

            if (rc == 0)
                TemperatureNP.s = IPS_BUSY;
            else if (rc == 1)
                TemperatureNP.s = IPS_OK;
            else
                TemperatureNP.s = IPS_ALERT;

            IDSetNumber(&TemperatureNP, nullptr);
            return true;
        }

        // Primary CCD Info
        if (!strcmp(name, PrimaryCCD.ImagePixelSizeNP.name))
        {
            IUUpdateNumber(&PrimaryCCD.ImagePixelSizeNP, values, names, n);
            PrimaryCCD.ImagePixelSizeNP.s = IPS_OK;
            SetCCDParams(PrimaryCCD.ImagePixelSizeNP.np[CCDChip::CCD_MAX_X].value,
                         PrimaryCCD.ImagePixelSizeNP.np[CCDChip::CCD_MAX_Y].value, PrimaryCCD.getBPP(),
                         PrimaryCCD.ImagePixelSizeNP.np[CCDChip::CCD_PIXEL_SIZE_X].value,
                         PrimaryCCD.ImagePixelSizeNP.np[CCDChip::CCD_PIXEL_SIZE_Y].value);
            IDSetNumber(&PrimaryCCD.ImagePixelSizeNP, nullptr);
            saveConfig(true);
            return true;
        }

        // Guide CCD Info
        if (!strcmp(name, GuideCCD.ImagePixelSizeNP.name))
        {
            IUUpdateNumber(&GuideCCD.ImagePixelSizeNP, values, names, n);
            GuideCCD.ImagePixelSizeNP.s = IPS_OK;
            SetGuiderParams(GuideCCD.ImagePixelSizeNP.np[CCDChip::CCD_MAX_X].value,
                            GuideCCD.ImagePixelSizeNP.np[CCDChip::CCD_MAX_Y].value, GuideCCD.getBPP(),
                            GuideCCD.ImagePixelSizeNP.np[CCDChip::CCD_PIXEL_SIZE_X].value,
                            GuideCCD.ImagePixelSizeNP.np[CCDChip::CCD_PIXEL_SIZE_Y].value);
            IDSetNumber(&GuideCCD.ImagePixelSizeNP, nullptr);
            saveConfig(true);
            return true;
        }

        // CCD Rotation
        if (!strcmp(name, CCDRotationNP.name))
        {
            IUUpdateNumber(&CCDRotationNP, values, names, n);
            CCDRotationNP.s = IPS_OK;
            IDSetNumber(&CCDRotationNP, nullptr);
            m_ValidCCDRotation = true;

            DEBUGF(Logger::DBG_SESSION, "CCD FOV rotation updated to %g degrees.", CCDRotationN[0].value);

            return true;
        }
    }

    // Streamer
    if (HasStreaming())
        Streamer->ISNewNumber(dev, name, values, names, n);

    return DefaultDevice::ISNewNumber(dev, name, values, names, n);
}

bool CCD::ISNewSwitch(const char * dev, const char * name, ISState * states, char * names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        // Upload Mode
        if (!strcmp(name, UploadSP.name))
        {
            int prevMode = IUFindOnSwitchIndex(&UploadSP);
            IUUpdateSwitch(&UploadSP, states, names, n);

            if (UpdateCCDUploadMode(static_cast<CCD_UPLOAD_MODE>(IUFindOnSwitchIndex(&UploadSP))))
            {
                if (UploadS[UPLOAD_CLIENT].s == ISS_ON)
                {
                    DEBUG(Logger::DBG_SESSION, "Upload settings set to client only.");
                    if (prevMode != 0)
                        deleteProperty(FileNameTP.name);
                }
                else if (UploadS[UPLOAD_LOCAL].s == ISS_ON)
                {
                    DEBUG(Logger::DBG_SESSION, "Upload settings set to local only.");
                    defineText(&FileNameTP);
                }
                else
                {
                    DEBUG(Logger::DBG_SESSION, "Upload settings set to client and local.");
                    defineText(&FileNameTP);
                }

                UploadSP.s = IPS_OK;
            }
            else
            {
                IUResetSwitch(&UploadSP);
                UploadS[prevMode].s = ISS_ON;
                UploadSP.s = IPS_ALERT;
            }

            IDSetSwitch(&UploadSP, nullptr);

            return true;
        }

        if (!strcmp(name, TelescopeTypeSP.name))
        {
            IUUpdateSwitch(&TelescopeTypeSP, states, names, n);
            TelescopeTypeSP.s = IPS_OK;
            IDSetSwitch(&TelescopeTypeSP, nullptr);
            return true;
        }

#ifdef WITH_EXPOSURE_LOOPING
        // Exposure Looping
        if (!strcmp(name, ExposureLoopSP.name))
        {
            IUUpdateSwitch(&ExposureLoopSP, states, names, n);
            ExposureLoopSP.s = IPS_OK;
            IDSetSwitch(&ExposureLoopSP, nullptr);
            return true;
        }
#endif

#ifdef HAVE_WEBSOCKET
        // Websocket Enable/Disable
        if (!strcmp(name, WebSocketSP.name))
        {
            IUUpdateSwitch(&WebSocketSP, states, names, n);
            WebSocketSP.s = IPS_OK;

            if (WebSocketS[WEBSOCKET_ENABLED].s == ISS_ON)
            {
                wsThread = std::thread(&wsThreadHelper, this);
                WebSocketSettingsN[WS_SETTINGS_PORT].value = wsServer.generatePort();
                WebSocketSettingsNP.s = IPS_OK;
                defineNumber(&WebSocketSettingsNP);
            }
            else if (wsServer.is_running())
            {
                wsServer.stop();
                wsThread.join();
                deleteProperty(WebSocketSettingsNP.name);
            }

            IDSetSwitch(&WebSocketSP, nullptr);
            return true;
        }
#endif

        // WCS Enable/Disable
        if (!strcmp(name, WorldCoordSP.name))
        {
            IUUpdateSwitch(&WorldCoordSP, states, names, n);
            WorldCoordSP.s = IPS_OK;

            if (WorldCoordS[0].s == ISS_ON)
            {
                LOG_INFO("World Coordinate System is enabled.");
                defineNumber(&CCDRotationNP);
            }
            else
            {
                LOG_INFO("World Coordinate System is disabled.");
                deleteProperty(CCDRotationNP.name);
            }

            m_ValidCCDRotation = false;
            IDSetSwitch(&WorldCoordSP, nullptr);
        }

        // Primary Chip Frame Reset
        if (strcmp(name, PrimaryCCD.ResetSP.name) == 0)
        {
            IUResetSwitch(&PrimaryCCD.ResetSP);
            PrimaryCCD.ResetSP.s = IPS_OK;
            if (CanBin())
                UpdateCCDBin(1, 1);
            if (CanSubFrame())
                UpdateCCDFrame(0, 0, PrimaryCCD.getXRes(), PrimaryCCD.getYRes());

            IDSetSwitch(&PrimaryCCD.ResetSP, nullptr);
            return true;
        }

        // Primary Chip Abort Expsoure
        if (strcmp(name, PrimaryCCD.AbortExposureSP.name) == 0)
        {
            IUResetSwitch(&PrimaryCCD.AbortExposureSP);

            if (AbortExposure())
            {
                PrimaryCCD.AbortExposureSP.s       = IPS_OK;
                PrimaryCCD.ImageExposureNP.s       = IPS_IDLE;
                PrimaryCCD.ImageExposureN[0].value = 0;
            }
            else
            {
                PrimaryCCD.AbortExposureSP.s = IPS_ALERT;
                PrimaryCCD.ImageExposureNP.s = IPS_ALERT;
            }

            POLLMS = getPollingPeriod();

#ifdef WITH_EXPOSURE_LOOPING
            if (ExposureLoopCountNP.s == IPS_BUSY)
            {
                uploadTime = 0;
                ExposureLoopCountNP.s = IPS_IDLE;
                ExposureLoopCountN[0].value = 1;
                IDSetNumber(&ExposureLoopCountNP, nullptr);
            }
#endif
            IDSetSwitch(&PrimaryCCD.AbortExposureSP, nullptr);
            IDSetNumber(&PrimaryCCD.ImageExposureNP, nullptr);

            return true;
        }

        // Guide Chip Abort Exposure
        if (strcmp(name, GuideCCD.AbortExposureSP.name) == 0)
        {
            IUResetSwitch(&GuideCCD.AbortExposureSP);

            if (AbortGuideExposure())
            {
                GuideCCD.AbortExposureSP.s       = IPS_OK;
                GuideCCD.ImageExposureNP.s       = IPS_IDLE;
                GuideCCD.ImageExposureN[0].value = 0;
            }
            else
            {
                GuideCCD.AbortExposureSP.s = IPS_ALERT;
                GuideCCD.ImageExposureNP.s = IPS_ALERT;
            }

            IDSetSwitch(&GuideCCD.AbortExposureSP, nullptr);
            IDSetNumber(&GuideCCD.ImageExposureNP, nullptr);

            return true;
        }

        // Primary Chip Compression
        if (strcmp(name, PrimaryCCD.CompressSP.name) == 0)
        {
            IUUpdateSwitch(&PrimaryCCD.CompressSP, states, names, n);
            PrimaryCCD.CompressSP.s = IPS_OK;
            IDSetSwitch(&PrimaryCCD.CompressSP, nullptr);

            if (PrimaryCCD.CompressS[0].s == ISS_ON)
            {
                PrimaryCCD.SendCompressed = true;
            }
            else
            {
                PrimaryCCD.SendCompressed = false;
            }
            return true;
        }

        // Guide Chip Compression
        if (strcmp(name, GuideCCD.CompressSP.name) == 0)
        {
            IUUpdateSwitch(&GuideCCD.CompressSP, states, names, n);
            GuideCCD.CompressSP.s = IPS_OK;
            IDSetSwitch(&GuideCCD.CompressSP, nullptr);

            if (GuideCCD.CompressS[0].s == ISS_ON)
            {
                GuideCCD.SendCompressed = true;
            }
            else
            {
                GuideCCD.SendCompressed = false;
            }
            return true;
        }

        // Primary Chip Frame Type
        if (strcmp(name, PrimaryCCD.FrameTypeSP.name) == 0)
        {
            IUUpdateSwitch(&PrimaryCCD.FrameTypeSP, states, names, n);
            PrimaryCCD.FrameTypeSP.s = IPS_OK;
            if (PrimaryCCD.FrameTypeS[0].s == ISS_ON)
                PrimaryCCD.setFrameType(CCDChip::LIGHT_FRAME);
            else if (PrimaryCCD.FrameTypeS[1].s == ISS_ON)
            {
                PrimaryCCD.setFrameType(CCDChip::BIAS_FRAME);
                if (HasShutter() == false)
                    DEBUG(Logger::DBG_WARNING,
                          "The CCD does not have a shutter. Cover the camera in order to take a bias frame.");
            }
            else if (PrimaryCCD.FrameTypeS[2].s == ISS_ON)
            {
                PrimaryCCD.setFrameType(CCDChip::DARK_FRAME);
                if (HasShutter() == false)
                    DEBUG(Logger::DBG_WARNING,
                          "The CCD does not have a shutter. Cover the camera in order to take a dark frame.");
            }
            else if (PrimaryCCD.FrameTypeS[3].s == ISS_ON)
                PrimaryCCD.setFrameType(CCDChip::FLAT_FRAME);

            if (UpdateCCDFrameType(PrimaryCCD.getFrameType()) == false)
                PrimaryCCD.FrameTypeSP.s = IPS_ALERT;

            IDSetSwitch(&PrimaryCCD.FrameTypeSP, nullptr);

            return true;
        }

        // Guide Chip Frame Type
        if (strcmp(name, GuideCCD.FrameTypeSP.name) == 0)
        {
            //  Compression Update
            IUUpdateSwitch(&GuideCCD.FrameTypeSP, states, names, n);
            GuideCCD.FrameTypeSP.s = IPS_OK;
            if (GuideCCD.FrameTypeS[0].s == ISS_ON)
                GuideCCD.setFrameType(CCDChip::LIGHT_FRAME);
            else if (GuideCCD.FrameTypeS[1].s == ISS_ON)
            {
                GuideCCD.setFrameType(CCDChip::BIAS_FRAME);
                if (HasShutter() == false)
                    DEBUG(Logger::DBG_WARNING,
                          "The CCD does not have a shutter. Cover the camera in order to take a bias frame.");
            }
            else if (GuideCCD.FrameTypeS[2].s == ISS_ON)
            {
                GuideCCD.setFrameType(CCDChip::DARK_FRAME);
                if (HasShutter() == false)
                    DEBUG(Logger::DBG_WARNING,
                          "The CCD does not have a shutter. Cover the camera in order to take a dark frame.");
            }
            else if (GuideCCD.FrameTypeS[3].s == ISS_ON)
                GuideCCD.setFrameType(CCDChip::FLAT_FRAME);

            if (UpdateGuiderFrameType(GuideCCD.getFrameType()) == false)
                GuideCCD.FrameTypeSP.s = IPS_ALERT;

            IDSetSwitch(&GuideCCD.FrameTypeSP, nullptr);

            return true;
        }

#if 0
        // Primary Chip Rapid Guide Enable/Disable
        if (strcmp(name, PrimaryCCD.RapidGuideSP.name) == 0)
        {
            IUUpdateSwitch(&PrimaryCCD.RapidGuideSP, states, names, n);
            PrimaryCCD.RapidGuideSP.s = IPS_OK;
            RapidGuideEnabled         = (PrimaryCCD.RapidGuideS[0].s == ISS_ON);

            if (RapidGuideEnabled)
            {
                defineSwitch(&PrimaryCCD.RapidGuideSetupSP);
                defineNumber(&PrimaryCCD.RapidGuideDataNP);
            }
            else
            {
                deleteProperty(PrimaryCCD.RapidGuideSetupSP.name);
                deleteProperty(PrimaryCCD.RapidGuideDataNP.name);
            }

            IDSetSwitch(&PrimaryCCD.RapidGuideSP, nullptr);
            return true;
        }

        // Guide Chip Rapid Guide Enable/Disable
        if (strcmp(name, GuideCCD.RapidGuideSP.name) == 0)
        {
            IUUpdateSwitch(&GuideCCD.RapidGuideSP, states, names, n);
            GuideCCD.RapidGuideSP.s = IPS_OK;
            GuiderRapidGuideEnabled = (GuideCCD.RapidGuideS[0].s == ISS_ON);

            if (GuiderRapidGuideEnabled)
            {
                defineSwitch(&GuideCCD.RapidGuideSetupSP);
                defineNumber(&GuideCCD.RapidGuideDataNP);
            }
            else
            {
                deleteProperty(GuideCCD.RapidGuideSetupSP.name);
                deleteProperty(GuideCCD.RapidGuideDataNP.name);
            }

            IDSetSwitch(&GuideCCD.RapidGuideSP, nullptr);
            return true;
        }

        // Primary CCD Rapid Guide Setup
        if (strcmp(name, PrimaryCCD.RapidGuideSetupSP.name) == 0)
        {
            IUUpdateSwitch(&PrimaryCCD.RapidGuideSetupSP, states, names, n);
            PrimaryCCD.RapidGuideSetupSP.s = IPS_OK;

            AutoLoop   = (PrimaryCCD.RapidGuideSetupS[0].s == ISS_ON);
            SendImage  = (PrimaryCCD.RapidGuideSetupS[1].s == ISS_ON);
            ShowMarker = (PrimaryCCD.RapidGuideSetupS[2].s == ISS_ON);

            IDSetSwitch(&PrimaryCCD.RapidGuideSetupSP, nullptr);
            return true;
        }

        // Guide Chip Rapid Guide Setup
        if (strcmp(name, GuideCCD.RapidGuideSetupSP.name) == 0)
        {
            IUUpdateSwitch(&GuideCCD.RapidGuideSetupSP, states, names, n);
            GuideCCD.RapidGuideSetupSP.s = IPS_OK;

            GuiderAutoLoop   = (GuideCCD.RapidGuideSetupS[0].s == ISS_ON);
            GuiderSendImage  = (GuideCCD.RapidGuideSetupS[1].s == ISS_ON);
            GuiderShowMarker = (GuideCCD.RapidGuideSetupS[2].s == ISS_ON);

            IDSetSwitch(&GuideCCD.RapidGuideSetupSP, nullptr);
            return true;
        }
#endif
    }

    if (HasStreaming())
        Streamer->ISNewSwitch(dev, name, states, names, n);

    return DefaultDevice::ISNewSwitch(dev, name, states, names, n);
}

int CCD::SetTemperature(double temperature)
{
    INDI_UNUSED(temperature);
    DEBUGF(Logger::DBG_WARNING, "CCD::SetTemperature %4.2f -  Should never get here", temperature);
    return -1;
}

bool CCD::StartExposure(float duration)
{
    DEBUGF(Logger::DBG_WARNING, "CCD::StartExposure %4.2f -  Should never get here", duration);
    return false;
}

bool CCD::StartGuideExposure(float duration)
{
    DEBUGF(Logger::DBG_WARNING, "CCD::StartGuide Exposure %4.2f -  Should never get here", duration);
    return false;
}

bool CCD::AbortExposure()
{
    DEBUG(Logger::DBG_WARNING, "CCD::AbortExposure -  Should never get here");
    return false;
}

bool CCD::AbortGuideExposure()
{
    DEBUG(Logger::DBG_WARNING, "CCD::AbortGuideExposure -  Should never get here");
    return false;
}

bool CCD::UpdateCCDFrame(int x, int y, int w, int h)
{
    // Just set value, unless HW layer overrides this and performs its own processing
    PrimaryCCD.setFrame(x, y, w, h);
    return true;
}

bool CCD::UpdateGuiderFrame(int x, int y, int w, int h)
{
    GuideCCD.setFrame(x, y, w, h);
    return true;
}

bool CCD::UpdateCCDBin(int hor, int ver)
{
    // Just set value, unless HW layer overrides this and performs its own processing
    PrimaryCCD.setBin(hor, ver);
    // Reset size
    if (HasStreaming())
        Streamer->setSize(PrimaryCCD.getSubW() / hor, PrimaryCCD.getSubH() / ver);
    return true;
}

bool CCD::UpdateGuiderBin(int hor, int ver)
{
    // Just set value, unless HW layer overrides this and performs its own processing
    GuideCCD.setBin(hor, ver);
    return true;
}

bool CCD::UpdateCCDFrameType(CCDChip::CCD_FRAME fType)
{
    INDI_UNUSED(fType);
    // Child classes can override this
    return true;
}

bool CCD::UpdateGuiderFrameType(CCDChip::CCD_FRAME fType)
{
    INDI_UNUSED(fType);
    // Child classes can override this
    return true;
}

void CCD::addFITSKeywords(fitsfile * fptr, CCDChip * targetChip)
{
    int status = 0;
    char dev_name[32];
    char exp_start[32];
    double pixSize1, pixSize2;

    AutoCNumeric locale;
    fits_update_key_str(fptr, "INSTRUME", getDeviceName(), "CCD Name", &status);

    // Telescope
    if (strlen(ActiveDeviceT[0].text) > 0)
    {
        fits_update_key_str(fptr, "TELESCOP", ActiveDeviceT[0].text, "Telescope name", &status);
    }

    // Observer
    fits_update_key_str(fptr, "OBSERVER", FITSHeaderT[FITS_OBSERVER].text, "Observer name", &status);

    // Object
    fits_update_key_str(fptr, "OBJECT", FITSHeaderT[FITS_OBJECT].text, "Object name", &status);

    pixSize1 = static_cast<double>(targetChip->getPixelSizeX());
    pixSize2 = static_cast<double>(targetChip->getPixelSizeY());

    strncpy(dev_name, getDeviceName(), 32);
    strncpy(exp_start, targetChip->getExposureStartTime(), 32);

    fits_update_key_dbl(fptr, "EXPTIME", targetChip->getExposureDuration(), 6, "Total Exposure Time (s)", &status);

    if (targetChip->getFrameType() == CCDChip::DARK_FRAME)
        fits_update_key_dbl(fptr, "DARKTIME", targetChip->getExposureDuration(), 6, "Total Dark Exposure Time (s)", &status);

    // If the camera has a cooler OR if the temperature permission was explicitly set to Read-Only, then record the temperature
    if (HasCooler() || TemperatureNP.p == IP_RO)
        fits_update_key_dbl(fptr, "CCD-TEMP", TemperatureN[0].value, 2, "CCD Temperature (Celsius)", &status);

    fits_update_key_dbl(fptr, "PIXSIZE1", targetChip->getPixelSizeX(), 6, "Pixel Size 1 (microns)", &status);
    fits_update_key_dbl(fptr, "PIXSIZE2", targetChip->getPixelSizeY(), 6, "Pixel Size 2 (microns)", &status);
    fits_update_key_lng(fptr, "XBINNING", targetChip->getBinX(), "Binning factor in width", &status);
    fits_update_key_lng(fptr, "YBINNING", targetChip->getBinY(), "Binning factor in height", &status);
    // XPIXSZ and YPIXSZ are logical sizes including the binning factor
    double xpixsz = pixSize1 * targetChip->getBinX();
    double ypixsz = pixSize2 * targetChip->getBinY();
    fits_update_key_dbl(fptr, "XPIXSZ", xpixsz, 6, "X binned pixel size in microns", &status);
    fits_update_key_dbl(fptr, "YPIXSZ", ypixsz, 6, "Y binned pixel size in microns", &status);

    switch (targetChip->getFrameType())
    {
        case CCDChip::LIGHT_FRAME:
            fits_update_key_str(fptr, "FRAME", "Light", "Frame Type", &status);
            break;
        case CCDChip::BIAS_FRAME:
            fits_update_key_str(fptr, "FRAME", "Bias", "Frame Type", &status);
            break;
        case CCDChip::FLAT_FRAME:
            fits_update_key_str(fptr, "FRAME", "Flat", "Frame Type", &status);
            break;
        case CCDChip::DARK_FRAME:
            fits_update_key_str(fptr, "FRAME", "Dark", "Frame Type", &status);
            break;
    }

    if (CurrentFilterSlot != -1 && CurrentFilterSlot <= static_cast<int>(FilterNames.size()))
    {
        fits_update_key_str(fptr, "FILTER", FilterNames.at(CurrentFilterSlot - 1).c_str(), "Filter", &status);
    }

#ifdef WITH_MINMAX
    if (targetChip->getNAxis() == 2)
    {
        double min_val, max_val;
        getMinMax(&min_val, &max_val, targetChip);

        fits_update_key_dbl(fptr, "DATAMIN", min_val, 6, "Minimum value", &status);
        fits_update_key_dbl(fptr, "DATAMAX", max_val, 6, "Maximum value", &status);
    }
#endif

    if (HasBayer() && targetChip->getNAxis() == 2)
    {
        fits_update_key_lng(fptr, "XBAYROFF", atoi(BayerT[0].text), "X offset of Bayer array", &status);
        fits_update_key_lng(fptr, "YBAYROFF", atoi(BayerT[1].text), "Y offset of Bayer array", &status);
        fits_update_key_str(fptr, "BAYERPAT", BayerT[2].text, "Bayer color pattern", &status);
    }

    if (TelescopeTypeS[TELESCOPE_PRIMARY].s == ISS_ON && primaryFocalLength != -1)
        fits_update_key_dbl(fptr, "FOCALLEN", primaryFocalLength, 2, "Focal Length (mm)", &status);
    else if (TelescopeTypeS[TELESCOPE_GUIDE].s == ISS_ON && guiderFocalLength != -1)
        fits_update_key_dbl(fptr, "FOCALLEN", guiderFocalLength, 2, "Focal Length (mm)", &status);

    if (!std::isnan(MPSAS))
    {
        fits_update_key_dbl(fptr, "MPSAS", MPSAS, 6, "Sky Quality (mag per arcsec^2)", &status);
    }

    if (!std::isnan(RotatorAngle))
    {
        fits_update_key_dbl(fptr, "ROTATANG", RotatorAngle, 3, "Rotator angle in degrees", &status);
    }

    // SCALE assuming square-pixels
    double pixScale = pixSize1 / primaryFocalLength * 206.3 * targetChip->getBinX();
    fits_update_key_dbl(fptr, "SCALE", pixScale, 6, "arcsecs per pixel", &status);

    if (targetChip->getFrameType() == CCDChip::LIGHT_FRAME && !std::isnan(J2000RA) && !std::isnan(J2000DE))
    {
        char ra_str[32] = {0}, de_str[32] = {0};

        fs_sexa(ra_str, J2000RA, 2, 360000);
        fs_sexa(de_str, J2000DE, 2, 360000);

        char * raPtr = ra_str, *dePtr = de_str;
        while (*raPtr != '\0')
        {
            if (*raPtr == ':')
                *raPtr = ' ';
            raPtr++;
        }
        while (*dePtr != '\0')
        {
            if (*dePtr == ':')
                *dePtr = ' ';
            dePtr++;
        }

        if (!std::isnan(Latitude) && !std::isnan(Longitude))
        {
            fits_update_key_dbl(fptr, "SITELAT", Latitude, 6, "Latitude of the imaging site in degrees", &status);
            fits_update_key_dbl(fptr, "SITELONG", Longitude, 6, "Longitude of the imaging site in degrees", &status);
        }
        if (!std::isnan(Airmass))
            //fits_update_key_s(fptr, TDOUBLE, "AIRMASS", &Airmass, "Airmass", &status);
            fits_update_key_dbl(fptr, "AIRMASS", Airmass, 6, "Airmass", &status);

        fits_update_key_str(fptr, "OBJCTRA", ra_str, "Object J2000 RA in Hours", &status);
        fits_update_key_str(fptr, "OBJCTDEC", de_str, "Object J2000 DEC in Degrees", &status);

        fits_update_key_dbl(fptr, "RA", J2000RA * 15, 6, "Object J2000 RA in Degrees", &status);
        fits_update_key_dbl(fptr, "DEC", J2000DE, 6, "Object J2000 DEC in Degrees", &status);

        //fits_update_key_s(fptr, TINT, "EPOCH", &epoch, "Epoch", &status);
        fits_update_key_lng(fptr, "EQUINOX", 2000, "Equinox", &status);

        // Add WCS Info
        if (WorldCoordS[0].s == ISS_ON && m_ValidCCDRotation && primaryFocalLength != -1)
        {
            double J2000RAHours = J2000RA * 15;
            fits_update_key_dbl(fptr, "CRVAL1", J2000RAHours, 10, "CRVAL1", &status);
            fits_update_key_dbl(fptr, "CRVAL2", J2000DE, 10, "CRVAL1", &status);

            char radecsys[8] = "FK5";
            char ctype1[16]  = "RA---TAN";
            char ctype2[16]  = "DEC--TAN";

            fits_update_key_str(fptr, "RADECSYS", radecsys, "RADECSYS", &status);
            fits_update_key_str(fptr, "CTYPE1", ctype1, "CTYPE1", &status);
            fits_update_key_str(fptr, "CTYPE2", ctype2, "CTYPE2", &status);

            double crpix1 = targetChip->getSubW() / targetChip->getBinX() / 2.0;
            double crpix2 = targetChip->getSubH() / targetChip->getBinY() / 2.0;

            fits_update_key_dbl(fptr, "CRPIX1", crpix1, 10, "CRPIX1", &status);
            fits_update_key_dbl(fptr, "CRPIX2", crpix2, 10, "CRPIX2", &status);

            double secpix1 = pixSize1 / primaryFocalLength * 206.3 * targetChip->getBinX();
            double secpix2 = pixSize2 / primaryFocalLength * 206.3 * targetChip->getBinY();

            //double secpix1 = pixSize1 / FocalLength * 206.3;
            //double secpix2 = pixSize2 / FocalLength * 206.3;

            fits_update_key_dbl(fptr, "SECPIX1", secpix1, 10, "SECPIX1", &status);
            fits_update_key_dbl(fptr, "SECPIX2", secpix2, 10, "SECPIX2", &status);

            double degpix1 = secpix1 / 3600.0;
            double degpix2 = secpix2 / 3600.0;

            fits_update_key_dbl(fptr, "CDELT1", degpix1, 10, "CDELT1", &status);
            fits_update_key_dbl(fptr, "CDELT2", degpix2, 10, "CDELT2", &status);

            // Rotation is CW, we need to convert it to CCW per CROTA1 definition
            double rotation = 360 - CCDRotationN[0].value;
            if (rotation > 360)
                rotation -= 360;

            fits_update_key_dbl(fptr, "CROTA1", rotation, 10, "CROTA1", &status);
            fits_update_key_dbl(fptr, "CROTA2", rotation, 10, "CROTA2", &status);

            /*double cd[4];
            cd[0] = degpix1;
            cd[1] = 0;
            cd[2] = 0;
            cd[3] = degpix2;

            fits_update_key_s(fptr, TDOUBLE, "CD1_1", &cd[0], "CD1_1", &status);
            fits_update_key_s(fptr, TDOUBLE, "CD1_2", &cd[1], "CD1_2", &status);
            fits_update_key_s(fptr, TDOUBLE, "CD2_1", &cd[2], "CD2_1", &status);
            fits_update_key_s(fptr, TDOUBLE, "CD2_2", &cd[3], "CD2_2", &status);*/
        }
    }

    fits_update_key_str(fptr, "DATE-OBS", exp_start, "UTC start date of observation", &status);
    fits_write_comment(fptr, "Generated by INDI", &status);
}

void CCD::fits_update_key_s(fitsfile * fptr, int type, std::string name, void * p, std::string explanation,
                            int * status)
{
    // this function is for removing warnings about deprecated string conversion to char* (from arg 5)
    fits_update_key(fptr, type, name.c_str(), p, const_cast<char *>(explanation.c_str()), status);
}

bool CCD::ExposureComplete(CCDChip * targetChip)
{
    // Reset POLLMS to default value
    POLLMS = getPollingPeriod();

    // Run async
    std::thread(&CCD::ExposureCompletePrivate, this, targetChip).detach();

    return true;
}

bool CCD::ExposureCompletePrivate(CCDChip * targetChip)
{
#ifdef WITH_EXPOSURE_LOOPING
    // If looping is on, let's immediately take another capture
    if (ExposureLoopS[EXPOSURE_LOOP_ON].s == ISS_ON)
    {
        double duration = targetChip->getExposureDuration();

        if (ExposureLoopCountN[0].value > 1)
        {
            if (ExposureLoopCountNP.s != IPS_BUSY)
            {
                exposureLoopStartup = std::chrono::system_clock::now();
            }
            else
            {
                auto end = std::chrono::system_clock::now();

                uploadTime = (std::chrono::duration_cast<std::chrono::milliseconds>(end - exposureLoopStartup)).count() / 1000.0 - duration;
                LOGF_DEBUG("Image download and upload/save took %.3f seconds.", uploadTime);

                exposureLoopStartup = end;
            }

            ExposureLoopCountNP.s = IPS_BUSY;
            ExposureLoopCountN[0].value--;
            IDSetNumber(&ExposureLoopCountNP, nullptr);

            if (uploadTime < duration)
            {
                StartExposure(duration);
                PrimaryCCD.ImageExposureNP.s = IPS_BUSY;
                IDSetNumber(&PrimaryCCD.ImageExposureNP, nullptr);
                if (duration * 1000 < POLLMS)
                    POLLMS = duration * 950;
            }
            else
            {
                LOGF_ERROR("Rapid exposure not possible since upload time is %.2f seconds while exposure time is %.2f seconds.", uploadTime, duration);
                PrimaryCCD.ImageExposureNP.s = IPS_ALERT;
                IDSetNumber(&PrimaryCCD.ImageExposureNP, nullptr);
                ExposureLoopCountN[0].value = 1;
                ExposureLoopCountNP.s = IPS_IDLE;
                IDSetNumber(&ExposureLoopCountNP, nullptr);
                uploadTime = 0;
                return false;
            }
        }
        else
        {
            uploadTime = 0;
            ExposureLoopCountNP.s = IPS_IDLE;
            IDSetNumber(&ExposureLoopCountNP, nullptr);
        }
    }
#endif

    bool sendImage = (UploadS[0].s == ISS_ON || UploadS[2].s == ISS_ON);
    bool saveImage = (UploadS[1].s == ISS_ON || UploadS[2].s == ISS_ON);

#if 0
    bool showMarker = false;
    bool autoLoop   = false;
    bool sendData   = false;

    if (RapidGuideEnabled && targetChip == &PrimaryCCD && (PrimaryCCD.getBPP() == 16 || PrimaryCCD.getBPP() == 8))
    {
        autoLoop   = AutoLoop;
        sendImage  = SendImage;
        showMarker = ShowMarker;
        sendData   = true;
        saveImage  = false;
    }

    if (GuiderRapidGuideEnabled && targetChip == &GuideCCD && (GuideCCD.getBPP() == 16 || PrimaryCCD.getBPP() == 8))
    {
        autoLoop   = GuiderAutoLoop;
        sendImage  = GuiderSendImage;
        showMarker = GuiderShowMarker;
        sendData   = true;
        saveImage  = false;
    }

    if (sendData)
    {
        static double P0 = 0.906, P1 = 0.584, P2 = 0.365, P3 = 0.117, P4 = 0.049, P5 = -0.05, P6 = -0.064, P7 = -0.074,
                      P8               = -0.094;
        targetChip->RapidGuideDataNP.s = IPS_BUSY;
        int width                      = targetChip->getSubW() / targetChip->getBinX();
        int height                     = targetChip->getSubH() / targetChip->getBinY();
        void * src                      = (unsigned short *)targetChip->getFrameBuffer();
        int i0, i1, i2, i3, i4, i5, i6, i7, i8;
        int ix = 0, iy = 0;
        int xM4;
        double average, fit, bestFit = 0;
        int minx = 4;
        int maxx = width - 4;
        int miny = 4;
        int maxy = height - 4;
        if (targetChip->lastRapidX > 0 && targetChip->lastRapidY > 0)
        {
            minx = std::max(targetChip->lastRapidX - 20, 4);
            maxx = std::min(targetChip->lastRapidX + 20, width - 4);
            miny = std::max(targetChip->lastRapidY - 20, 4);
            maxy = std::min(targetChip->lastRapidY + 20, height - 4);
        }
        if (targetChip->getBPP() == 16)
        {
            unsigned short * p;
            for (int x = minx; x < maxx; x++)
                for (int y = miny; y < maxy; y++)
                {
                    i0 = i1 = i2 = i3 = i4 = i5 = i6 = i7 = i8 = 0;
                    xM4                                        = x - 4;
                    p                                          = (unsigned short *)src + (y - 4) * width + xM4;
                    i8 += *p++;
                    i8 += *p++;
                    i8 += *p++;
                    i8 += *p++;
                    i8 += *p++;
                    i8 += *p++;
                    i8 += *p++;
                    i8 += *p++;
                    i8 += *p++;
                    p = (unsigned short *)src + (y - 3) * width + xM4;
                    i8 += *p++;
                    i8 += *p++;
                    i8 += *p++;
                    i7 += *p++;
                    i6 += *p++;
                    i7 += *p++;
                    i8 += *p++;
                    i8 += *p++;
                    i8 += *p++;
                    p = (unsigned short *)src + (y - 2) * width + xM4;
                    i8 += *p++;
                    i8 += *p++;
                    i5 += *p++;
                    i4 += *p++;
                    i3 += *p++;
                    i4 += *p++;
                    i5 += *p++;
                    i8 += *p++;
                    i8 += *p++;
                    p = (unsigned short *)src + (y - 1) * width + xM4;
                    i8 += *p++;
                    i7 += *p++;
                    i4 += *p++;
                    i2 += *p++;
                    i1 += *p++;
                    i2 += *p++;
                    i4 += *p++;
                    i8 += *p++;
                    i8 += *p++;
                    p = (unsigned short *)src + (y + 0) * width + xM4;
                    i8 += *p++;
                    i6 += *p++;
                    i3 += *p++;
                    i1 += *p++;
                    i0 += *p++;
                    i1 += *p++;
                    i3 += *p++;
                    i6 += *p++;
                    i8 += *p++;
                    p = (unsigned short *)src + (y + 1) * width + xM4;
                    i8 += *p++;
                    i7 += *p++;
                    i4 += *p++;
                    i2 += *p++;
                    i1 += *p++;
                    i2 += *p++;
                    i4 += *p++;
                    i8 += *p++;
                    i8 += *p++;
                    p = (unsigned short *)src + (y + 2) * width + xM4;
                    i8 += *p++;
                    i8 += *p++;
                    i5 += *p++;
                    i4 += *p++;
                    i3 += *p++;
                    i4 += *p++;
                    i5 += *p++;
                    i8 += *p++;
                    i8 += *p++;
                    p = (unsigned short *)src + (y + 3) * width + xM4;
                    i8 += *p++;
                    i8 += *p++;
                    i8 += *p++;
                    i7 += *p++;
                    i6 += *p++;
                    i7 += *p++;
                    i8 += *p++;
                    i8 += *p++;
                    i8 += *p++;
                    p = (unsigned short *)src + (y + 4) * width + xM4;
                    i8 += *p++;
                    i8 += *p++;
                    i8 += *p++;
                    i8 += *p++;
                    i8 += *p++;
                    i8 += *p++;
                    i8 += *p++;
                    i8 += *p++;
                    i8 += *p++;
                    average = (i0 + i1 + i2 + i3 + i4 + i5 + i6 + i7 + i8) / 85.0;
                    fit     = P0 * (i0 - average) + P1 * (i1 - 4 * average) + P2 * (i2 - 4 * average) +
                              P3 * (i3 - 4 * average) + P4 * (i4 - 8 * average) + P5 * (i5 - 4 * average) +
                              P6 * (i6 - 4 * average) + P7 * (i7 - 8 * average) + P8 * (i8 - 48 * average);
                    if (bestFit < fit)
                    {
                        bestFit = fit;
                        ix      = x;
                        iy      = y;
                    }
                }
        }
        else
        {
            unsigned char * p;
            for (int x = minx; x < maxx; x++)
                for (int y = miny; y < maxy; y++)
                {
                    i0 = i1 = i2 = i3 = i4 = i5 = i6 = i7 = i8 = 0;
                    xM4                                        = x - 4;
                    p                                          = (unsigned char *)src + (y - 4) * width + xM4;
                    i8 += *p++;
                    i8 += *p++;
                    i8 += *p++;
                    i8 += *p++;
                    i8 += *p++;
                    i8 += *p++;
                    i8 += *p++;
                    i8 += *p++;
                    i8 += *p++;
                    p = (unsigned char *)src + (y - 3) * width + xM4;
                    i8 += *p++;
                    i8 += *p++;
                    i8 += *p++;
                    i7 += *p++;
                    i6 += *p++;
                    i7 += *p++;
                    i8 += *p++;
                    i8 += *p++;
                    i8 += *p++;
                    p = (unsigned char *)src + (y - 2) * width + xM4;
                    i8 += *p++;
                    i8 += *p++;
                    i5 += *p++;
                    i4 += *p++;
                    i3 += *p++;
                    i4 += *p++;
                    i5 += *p++;
                    i8 += *p++;
                    i8 += *p++;
                    p = (unsigned char *)src + (y - 1) * width + xM4;
                    i8 += *p++;
                    i7 += *p++;
                    i4 += *p++;
                    i2 += *p++;
                    i1 += *p++;
                    i2 += *p++;
                    i4 += *p++;
                    i8 += *p++;
                    i8 += *p++;
                    p = (unsigned char *)src + (y + 0) * width + xM4;
                    i8 += *p++;
                    i6 += *p++;
                    i3 += *p++;
                    i1 += *p++;
                    i0 += *p++;
                    i1 += *p++;
                    i3 += *p++;
                    i6 += *p++;
                    i8 += *p++;
                    p = (unsigned char *)src + (y + 1) * width + xM4;
                    i8 += *p++;
                    i7 += *p++;
                    i4 += *p++;
                    i2 += *p++;
                    i1 += *p++;
                    i2 += *p++;
                    i4 += *p++;
                    i8 += *p++;
                    i8 += *p++;
                    p = (unsigned char *)src + (y + 2) * width + xM4;
                    i8 += *p++;
                    i8 += *p++;
                    i5 += *p++;
                    i4 += *p++;
                    i3 += *p++;
                    i4 += *p++;
                    i5 += *p++;
                    i8 += *p++;
                    i8 += *p++;
                    p = (unsigned char *)src + (y + 3) * width + xM4;
                    i8 += *p++;
                    i8 += *p++;
                    i8 += *p++;
                    i7 += *p++;
                    i6 += *p++;
                    i7 += *p++;
                    i8 += *p++;
                    i8 += *p++;
                    i8 += *p++;
                    p = (unsigned char *)src + (y + 4) * width + xM4;
                    i8 += *p++;
                    i8 += *p++;
                    i8 += *p++;
                    i8 += *p++;
                    i8 += *p++;
                    i8 += *p++;
                    i8 += *p++;
                    i8 += *p++;
                    i8 += *p++;
                    average = (i0 + i1 + i2 + i3 + i4 + i5 + i6 + i7 + i8) / 85.0;
                    fit     = P0 * (i0 - average) + P1 * (i1 - 4 * average) + P2 * (i2 - 4 * average) +
                              P3 * (i3 - 4 * average) + P4 * (i4 - 8 * average) + P5 * (i5 - 4 * average) +
                              P6 * (i6 - 4 * average) + P7 * (i7 - 8 * average) + P8 * (i8 - 48 * average);
                    if (bestFit < fit)
                    {
                        bestFit = fit;
                        ix      = x;
                        iy      = y;
                    }
                }
        }

        targetChip->RapidGuideDataN[0].value = ix;
        targetChip->RapidGuideDataN[1].value = iy;
        targetChip->RapidGuideDataN[2].value = bestFit;
        targetChip->lastRapidX               = ix;
        targetChip->lastRapidY               = iy;
        if (bestFit > 50)
        {
            int sumX           = 0;
            int sumY           = 0;
            int total          = 0;
            int max            = 0;
            int noiseThreshold = 0;

            if (targetChip->getBPP() == 16)
            {
                unsigned short * p;
                for (int y = iy - 4; y <= iy + 4; y++)
                {
                    p = (unsigned short *)src + y * width + ix - 4;
                    for (int x = ix - 4; x <= ix + 4; x++)
                    {
                        int w = *p++;
                        noiseThreshold += w;
                        if (w > max)
                            max = w;
                    }
                }
                noiseThreshold = (noiseThreshold / 81 + max) / 2; // set threshold between peak and average
                for (int y = iy - 4; y <= iy + 4; y++)
                {
                    p = (unsigned short *)src + y * width + ix - 4;
                    for (int x = ix - 4; x <= ix + 4; x++)
                    {
                        int w = *p++;
                        if (w < noiseThreshold)
                            w = 0;
                        sumX += x * w;
                        sumY += y * w;
                        total += w;
                    }
                }
            }
            else
            {
                unsigned char * p;
                for (int y = iy - 4; y <= iy + 4; y++)
                {
                    p = (unsigned char *)src + y * width + ix - 4;
                    for (int x = ix - 4; x <= ix + 4; x++)
                    {
                        int w = *p++;
                        noiseThreshold += w;
                        if (w > max)
                            max = w;
                    }
                }
                noiseThreshold = (noiseThreshold / 81 + max) / 2; // set threshold between peak and average
                for (int y = iy - 4; y <= iy + 4; y++)
                {
                    p = (unsigned char *)src + y * width + ix - 4;
                    for (int x = ix - 4; x <= ix + 4; x++)
                    {
                        int w = *p++;
                        if (w < noiseThreshold)
                            w = 0;
                        sumX += x * w;
                        sumY += y * w;
                        total += w;
                    }
                }
            }

            if (total > 0)
            {
                targetChip->RapidGuideDataN[0].value = ((double)sumX) / total;
                targetChip->RapidGuideDataN[1].value = ((double)sumY) / total;
                targetChip->RapidGuideDataNP.s       = IPS_OK;

                DEBUGF(Logger::DBG_DEBUG, "Guide Star X: %g Y: %g FIT: %g", targetChip->RapidGuideDataN[0].value,
                       targetChip->RapidGuideDataN[1].value, targetChip->RapidGuideDataN[2].value);
            }
            else
            {
                targetChip->RapidGuideDataNP.s = IPS_ALERT;
                targetChip->lastRapidX = targetChip->lastRapidY = -1;
            }
        }
        else
        {
            targetChip->RapidGuideDataNP.s = IPS_ALERT;
            targetChip->lastRapidX = targetChip->lastRapidY = -1;
        }
        IDSetNumber(&targetChip->RapidGuideDataNP, nullptr);

        if (showMarker)
        {
            int xmin = std::max(ix - 10, 0);
            int xmax = std::min(ix + 10, width - 1);
            int ymin = std::max(iy - 10, 0);
            int ymax = std::min(iy + 10, height - 1);

            //fprintf(stderr, "%d %d %d %d\n", xmin, xmax, ymin, ymax);

            if (targetChip->getBPP() == 16)
            {
                unsigned short * p;
                if (ymin > 0)
                {
                    p = (unsigned short *)src + ymin * width + xmin;
                    for (int x = xmin; x <= xmax; x++)
                        *p++ = 50000;
                }

                if (xmin > 0)
                {
                    for (int y = ymin; y <= ymax; y++)
                    {
                        *((unsigned short *)src + y * width + xmin) = 50000;
                    }
                }

                if (xmax < width - 1)
                {
                    for (int y = ymin; y <= ymax; y++)
                    {
                        *((unsigned short *)src + y * width + xmax) = 50000;
                    }
                }

                if (ymax < height - 1)
                {
                    p = (unsigned short *)src + ymax * width + xmin;
                    for (int x = xmin; x <= xmax; x++)
                        *p++ = 50000;
                }
            }
            else
            {
                unsigned char * p;
                if (ymin > 0)
                {
                    p = (unsigned char *)src + ymin * width + xmin;
                    for (int x = xmin; x <= xmax; x++)
                        *p++ = 255;
                }

                if (xmin > 0)
                {
                    for (int y = ymin; y <= ymax; y++)
                    {
                        *((unsigned char *)src + y * width + xmin) = 255;
                    }
                }

                if (xmax < width - 1)
                {
                    for (int y = ymin; y <= ymax; y++)
                    {
                        *((unsigned char *)src + y * width + xmax) = 255;
                    }
                }

                if (ymax < height - 1)
                {
                    p = (unsigned char *)src + ymax * width + xmin;
                    for (int x = xmin; x <= xmax; x++)
                        *p++ = 255;
                }
            }
        }
    }
#endif

    if (sendImage || saveImage /* || useSolver*/)
    {
        if (!strcmp(targetChip->getImageExtension(), "fits"))
        {
            void * memptr;
            size_t memsize;
            int img_type  = 0;
            int byte_type = 0;
            int status    = 0;
            long naxis    = targetChip->getNAxis();
            long naxes[3];
            int nelements = 0;
            std::string bit_depth;
            char error_status[MAXRBUF];

            fitsfile * fptr = nullptr;

            naxes[0] = targetChip->getSubW() / targetChip->getBinX();
            naxes[1] = targetChip->getSubH() / targetChip->getBinY();

            switch (targetChip->getBPP())
            {
                case 8:
                    byte_type = TBYTE;
                    img_type  = BYTE_IMG;
                    bit_depth = "8 bits per pixel";
                    break;

                case 16:
                    byte_type = TUSHORT;
                    img_type  = USHORT_IMG;
                    bit_depth = "16 bits per pixel";
                    break;

                case 32:
                    byte_type = TULONG;
                    img_type  = ULONG_IMG;
                    bit_depth = "32 bits per pixel";
                    break;

                default:
                    LOGF_ERROR("Unsupported bits per pixel value %d", targetChip->getBPP());
                    return false;
            }

            nelements = naxes[0] * naxes[1];
            if (naxis == 3)
            {
                nelements *= 3;
                naxes[2] = 3;
            }

            /*DEBUGF(Logger::DBG_DEBUG, "Exposure complete. Image Depth: %s. Width: %d Height: %d nelements: %d", bit_depth.c_str(), naxes[0],
                    naxes[1], nelements);*/

            std::unique_lock<std::mutex> guard(ccdBufferLock);

            //  Now we have to send fits format data to the client
            memsize = 5760;
            memptr  = malloc(memsize);
            if (!memptr)
            {
                LOGF_ERROR("Error: failed to allocate memory: %lu", memsize);
                return false;
            }

            fits_create_memfile(&fptr, &memptr, &memsize, 2880, realloc, &status);

            if (status)
            {
                fits_report_error(stderr, status); /* print out any error messages */
                fits_get_errstatus(status, error_status);
                fits_close_file(fptr, &status);
                free(memptr);
                LOGF_ERROR("FITS Error: %s", error_status);
                return false;
            }

            fits_create_img(fptr, img_type, naxis, naxes, &status);

            if (status)
            {
                fits_report_error(stderr, status); /* print out any error messages */
                fits_get_errstatus(status, error_status);
                fits_close_file(fptr, &status);
                free(memptr);
                LOGF_ERROR("FITS Error: %s", error_status);
                return false;
            }

            addFITSKeywords(fptr, targetChip);

            fits_write_img(fptr, byte_type, 1, nelements, targetChip->getFrameBuffer(), &status);

            if (status)
            {
                fits_report_error(stderr, status); /* print out any error messages */
                fits_get_errstatus(status, error_status);
                fits_close_file(fptr, &status);
                free(memptr);
                LOGF_ERROR("FITS Error: %s", error_status);
                return false;
            }

            fits_close_file(fptr, &status);

            bool rc = uploadFile(targetChip, memptr, memsize, sendImage, saveImage /*, useSolver*/);

            free(memptr);

            guard.unlock();

            if (rc == false)
            {
                targetChip->setExposureFailed();
                return false;
            }
        }
        else
        {
            std::unique_lock<std::mutex> guard(ccdBufferLock);
            bool rc = uploadFile(targetChip, targetChip->getFrameBuffer(), targetChip->getFrameBufferSize(), sendImage,
                                 saveImage);
            guard.unlock();

            if (rc == false)
            {
                targetChip->setExposureFailed();
                return false;
            }
        }
    }

    targetChip->ImageExposureNP.s = IPS_OK;
    IDSetNumber(&targetChip->ImageExposureNP, nullptr);

#if 0
    if (autoLoop)
    {
        if (targetChip == &PrimaryCCD)
        {
            PrimaryCCD.ImageExposureN[0].value = ExposureTime;
            if (StartExposure(ExposureTime))
            {
                // Record information required later in creation of FITS header
                if (targetChip->getFrameType() == CCDChip::LIGHT_FRAME && !std::isnan(RA) && !std::isnan(Dec))
                {
                    ln_equ_posn epochPos { 0, 0 }, J2000Pos { 0, 0 };
                    epochPos.ra  = RA * 15.0;
                    epochPos.dec = Dec;

                    // Convert from JNow to J2000
                    ln_get_equ_prec2(&epochPos, ln_get_julian_from_sys(), JD2000, &J2000Pos);

                    J2000RA = J2000Pos.ra / 15.0;
                    J2000DE = J2000Pos.dec;

                    if (!std::isnan(Latitude) && !std::isnan(Longitude))
                    {
                        // Horizontal Coords
                        ln_hrz_posn horizontalPos;
                        ln_lnlat_posn observer;
                        observer.lat = Latitude;
                        observer.lng = Longitude;

                        ln_get_hrz_from_equ(&epochPos, &observer, ln_get_julian_from_sys(), &horizontalPos);
                        Airmass = ln_get_airmass(horizontalPos.alt, 750);
                    }
                }

                PrimaryCCD.ImageExposureNP.s = IPS_BUSY;
            }
            else
            {
                DEBUG(Logger::DBG_DEBUG, "Autoloop: Primary CCD Exposure Error!");
                PrimaryCCD.ImageExposureNP.s = IPS_ALERT;
            }

            IDSetNumber(&PrimaryCCD.ImageExposureNP, nullptr);
        }
        else
        {
            GuideCCD.ImageExposureN[0].value = GuiderExposureTime;
            GuideCCD.ImageExposureNP.s       = IPS_BUSY;
            if (StartGuideExposure(GuiderExposureTime))
                GuideCCD.ImageExposureNP.s = IPS_BUSY;
            else
            {
                DEBUG(Logger::DBG_DEBUG, "Autoloop: Guide CCD Exposure Error!");
                GuideCCD.ImageExposureNP.s = IPS_ALERT;
            }

            IDSetNumber(&GuideCCD.ImageExposureNP, nullptr);
        }
    }
#endif

    return true;
}

bool CCD::uploadFile(CCDChip * targetChip, const void * fitsData, size_t totalBytes, bool sendImage,
                     bool saveImage /*, bool useSolver*/)
{
    uint8_t * compressedData = nullptr;

    DEBUGF(Logger::DBG_DEBUG, "Uploading file. Ext: %s, Size: %d, sendImage? %s, saveImage? %s",
           targetChip->getImageExtension(), totalBytes, sendImage ? "Yes" : "No", saveImage ? "Yes" : "No");

    if (saveImage)
    {
        targetChip->FitsB.blob    = const_cast<void *>(fitsData);
        targetChip->FitsB.bloblen = totalBytes;
        snprintf(targetChip->FitsB.format, MAXINDIBLOBFMT, ".%s", targetChip->getImageExtension());

        FILE * fp = nullptr;
        char imageFileName[MAXRBUF];

        std::string prefix = UploadSettingsT[UPLOAD_PREFIX].text;
        int maxIndex       = getFileIndex(UploadSettingsT[UPLOAD_DIR].text, UploadSettingsT[UPLOAD_PREFIX].text,
                                          targetChip->FitsB.format);

        if (maxIndex < 0)
        {
            LOGF_ERROR("Error iterating directory %s. %s", UploadSettingsT[0].text,
                       strerror(errno));
            return false;
        }

        if (maxIndex > 0)
        {
            char ts[32];
            struct tm * tp;
            time_t t;
            time(&t);
            tp = localtime(&t);
            strftime(ts, sizeof(ts), "%Y-%m-%dT%H-%M-%S", tp);
            std::string filets(ts);
            prefix = std::regex_replace(prefix, std::regex("ISO8601"), filets);

            char indexString[8];
            snprintf(indexString, 8, "%03d", maxIndex);
            std::string prefixIndex = indexString;
            //prefix.replace(prefix.find("XXX"), std::string::npos, prefixIndex);
            prefix = std::regex_replace(prefix, std::regex("XXX"), prefixIndex);
        }

        snprintf(imageFileName, MAXRBUF, "%s/%s%s", UploadSettingsT[0].text, prefix.c_str(), targetChip->FitsB.format);

        fp = fopen(imageFileName, "w");
        if (fp == nullptr)
        {
            LOGF_ERROR("Unable to save image file (%s). %s", imageFileName, strerror(errno));
            return false;
        }

        int n = 0;
        for (int nr = 0; nr < (int)targetChip->FitsB.bloblen; nr += n)
            n = fwrite((static_cast<char *>(targetChip->FitsB.blob) + nr), 1, targetChip->FitsB.bloblen - nr, fp);

        fclose(fp);

        // Save image file path
        IUSaveText(&FileNameT[0], imageFileName);

        DEBUGF(Logger::DBG_SESSION, "Image saved to %s", imageFileName);
        FileNameTP.s = IPS_OK;
        IDSetText(&FileNameTP, nullptr);
    }

    if (targetChip->SendCompressed)
    {
        if (!strcmp(targetChip->getImageExtension(), "fits"))
        {
            int  compressedBytes = 0;
            char filename[MAXRBUF] = {0};
            strncpy(filename, "/tmp/compressedfits.fits", MAXRBUF);

            FILE * fp = fopen(filename, "w");
            if (fp == nullptr)
            {
                LOGF_ERROR("Unable to save temporary image file: %s", strerror(errno));
                return false;
            }

            int n = 0;
            for (int nr = 0; nr < static_cast<int>(totalBytes); nr += n)
                n = fwrite(static_cast<const uint8_t *>(fitsData) + nr, 1, totalBytes - nr, fp);
            fclose(fp);

            fpstate	fpvar;
            std::vector<std::string> arguments = {"fpack", filename};
            std::vector<char *> arglist;
            for (const auto &arg : arguments)
                arglist.push_back((char *)arg.data());
            arglist.push_back(nullptr);

            int argc = arglist.size() - 1;
            char ** argv = arglist.data();

            // TODO: Check for errors
            fp_init (&fpvar);
            fp_get_param (argc, argv, &fpvar);
            fp_preflight (argc, argv, FPACK, &fpvar);
            fp_loop (argc, argv, FPACK, filename, fpvar);

            // Remove temporary file from disk
            remove(filename);

            // Add .fz
            strncat(filename, ".fz", 3);

            struct stat st;
            stat(filename, &st);
            compressedBytes = st.st_size;

            compressedData = new uint8_t[compressedBytes];

            if (compressedData == nullptr)
            {
                LOG_ERROR("Ran out of memory compressing image.");
                return false;
            }

            fp = fopen(filename, "r");
            if (fp == nullptr)
            {
                LOGF_ERROR("Unable to open temporary image file: %s", strerror(errno));
                delete [] compressedData;
                return false;
            }

            n = 0;
            for (int nr = 0; nr < compressedBytes; nr += n)
                n = fread(compressedData + nr, 1, compressedBytes - nr, fp);
            fclose(fp);

            // Remove compressed temporary file from disk
            remove(filename);

            targetChip->FitsB.blob    = compressedData;
            targetChip->FitsB.bloblen = compressedBytes;
            totalBytes = compressedBytes;
            snprintf(targetChip->FitsB.format, MAXINDIBLOBFMT, ".%s.fz", targetChip->getImageExtension());
        }
        else
        {
            uLong compressedBytes = sizeof(char) * totalBytes + totalBytes / 64 + 16 + 3;
            compressedData  = new uint8_t[compressedBytes];

            if (fitsData == nullptr || compressedData == nullptr)
            {
                if (compressedData)
                    delete [] compressedData;
                LOG_ERROR("Error: Ran out of memory compressing image");
                return false;
            }

            int r = compress2(compressedData, &compressedBytes, (const Bytef *)fitsData, totalBytes, 9);
            if (r != Z_OK)
            {
                /* this should NEVER happen */
                LOG_ERROR("Error: Failed to compress image");
                delete [] compressedData;
                return false;
            }

            targetChip->FitsB.blob    = compressedData;
            targetChip->FitsB.bloblen = compressedBytes;
            snprintf(targetChip->FitsB.format, MAXINDIBLOBFMT, ".%s.z", targetChip->getImageExtension());
        }
    }
    else
    {
        targetChip->FitsB.blob    = const_cast<void *>(fitsData);
        targetChip->FitsB.bloblen = totalBytes;
        snprintf(targetChip->FitsB.format, MAXINDIBLOBFMT, ".%s", targetChip->getImageExtension());
    }

    targetChip->FitsB.size = totalBytes;
    targetChip->FitsBP.s   = IPS_OK;

    if (sendImage)
    {
#ifdef HAVE_WEBSOCKET
        if (HasWebSocket() && WebSocketS[WEBSOCKET_ENABLED].s == ISS_ON)
        {
            auto start = std::chrono::high_resolution_clock::now();

            // Send format/size/..etc first later
            wsServer.send_text(std::string(targetChip->FitsB.format));
            wsServer.send_binary(targetChip->FitsB.blob, targetChip->FitsB.bloblen);

            auto end = std::chrono::high_resolution_clock::now();
            std::chrono::duration<double> diff = end - start;
            LOGF_DEBUG("Websocket transfer took %g seconds", diff.count());
        }
        else
#endif
        {
            auto start = std::chrono::high_resolution_clock::now();
            IDSetBLOB(&targetChip->FitsBP, nullptr);
            auto end = std::chrono::high_resolution_clock::now();
            std::chrono::duration<double> diff = end - start;
            LOGF_DEBUG("BLOB transfer took %g seconds", diff.count());
        }
    }

    if (compressedData)
        delete [] compressedData;

    DEBUG(Logger::DBG_DEBUG, "Upload complete");

    return true;
}

void CCD::SetCCDParams(int x, int y, int bpp, float xf, float yf)
{
    PrimaryCCD.setResolution(x, y);
    PrimaryCCD.setFrame(0, 0, x, y);
    if (CanBin())
        PrimaryCCD.setBin(1, 1);
    PrimaryCCD.setPixelSize(xf, yf);
    PrimaryCCD.setBPP(bpp);
}

void CCD::SetGuiderParams(int x, int y, int bpp, float xf, float yf)
{
    capability |= CCD_HAS_GUIDE_HEAD;

    GuideCCD.setResolution(x, y);
    GuideCCD.setFrame(0, 0, x, y);
    GuideCCD.setPixelSize(xf, yf);
    GuideCCD.setBPP(bpp);
}

bool CCD::saveConfigItems(FILE * fp)
{
    DefaultDevice::saveConfigItems(fp);

    IUSaveConfigText(fp, &ActiveDeviceTP);
    IUSaveConfigSwitch(fp, &UploadSP);
    IUSaveConfigText(fp, &UploadSettingsTP);
    IUSaveConfigSwitch(fp, &TelescopeTypeSP);
#ifdef WITH_EXPOSURE_LOOPING
    IUSaveConfigSwitch(fp, &ExposureLoopSP);
#endif

    IUSaveConfigSwitch(fp, &PrimaryCCD.CompressSP);

    if (HasGuideHead())
    {
        IUSaveConfigSwitch(fp, &GuideCCD.CompressSP);
        IUSaveConfigNumber(fp, &GuideCCD.ImageBinNP);
    }

    if (CanSubFrame())
        IUSaveConfigNumber(fp, &PrimaryCCD.ImageFrameNP);

    if (CanBin())
        IUSaveConfigNumber(fp, &PrimaryCCD.ImageBinNP);

    if (HasBayer())
        IUSaveConfigText(fp, &BayerTP);

    if (HasStreaming())
        Streamer->saveConfigItems(fp);

    return true;
}

IPState CCD::GuideNorth(uint32_t ms)
{
    INDI_UNUSED(ms);
    LOG_ERROR("The CCD does not support guiding.");
    return IPS_ALERT;
}

IPState CCD::GuideSouth(uint32_t ms)
{
    INDI_UNUSED(ms);
    LOG_ERROR("The CCD does not support guiding.");
    return IPS_ALERT;
}

IPState CCD::GuideEast(uint32_t ms)
{
    INDI_UNUSED(ms);
    LOG_ERROR("The CCD does not support guiding.");
    return IPS_ALERT;
}

IPState CCD::GuideWest(uint32_t ms)
{
    INDI_UNUSED(ms);
    LOG_ERROR("The CCD does not support guiding.");
    return IPS_ALERT;
}

void CCD::getMinMax(double * min, double * max, CCDChip * targetChip)
{
    int ind         = 0, i, j;
    int imageHeight = targetChip->getSubH() / targetChip->getBinY();
    int imageWidth  = targetChip->getSubW() / targetChip->getBinX();
    double lmin = 0, lmax = 0;

    switch (targetChip->getBPP())
    {
        case 8:
        {
            uint8_t * imageBuffer = targetChip->getFrameBuffer();
            lmin = lmax = imageBuffer[0];

            for (i = 0; i < imageHeight; i++)
                for (j = 0; j < imageWidth; j++)
                {
                    ind = (i * imageWidth) + j;
                    if (imageBuffer[ind] < lmin)
                        lmin = imageBuffer[ind];
                    else if (imageBuffer[ind] > lmax)
                        lmax = imageBuffer[ind];
                }
        }
        break;

        case 16:
        {
            uint16_t * imageBuffer = reinterpret_cast<uint16_t*>(targetChip->getFrameBuffer());
            lmin = lmax = imageBuffer[0];

            for (i = 0; i < imageHeight; i++)
                for (j = 0; j < imageWidth; j++)
                {
                    ind = (i * imageWidth) + j;
                    if (imageBuffer[ind] < lmin)
                        lmin = imageBuffer[ind];
                    else if (imageBuffer[ind] > lmax)
                        lmax = imageBuffer[ind];
                }
        }
        break;

        case 32:
        {
            uint32_t * imageBuffer = reinterpret_cast<uint32_t*>(targetChip->getFrameBuffer());
            lmin = lmax = imageBuffer[0];

            for (i = 0; i < imageHeight; i++)
                for (j = 0; j < imageWidth; j++)
                {
                    ind = (i * imageWidth) + j;
                    if (imageBuffer[ind] < lmin)
                        lmin = imageBuffer[ind];
                    else if (imageBuffer[ind] > lmax)
                        lmax = imageBuffer[ind];
                }
        }
        break;
    }
    *min = lmin;
    *max = lmax;
}

std::string regex_replace_compat(const std::string &input, const std::string &pattern, const std::string &replace)
{
    std::stringstream s;
    std::regex_replace(std::ostreambuf_iterator<char>(s), input.begin(), input.end(), std::regex(pattern), replace);
    return s.str();
}

int CCD::getFileIndex(const char * dir, const char * prefix, const char * ext)
{
    INDI_UNUSED(ext);

    DIR * dpdf = nullptr;
    struct dirent * epdf = nullptr;
    std::vector<std::string> files = std::vector<std::string>();

    std::string prefixIndex = prefix;
    prefixIndex             = regex_replace_compat(prefixIndex, "_ISO8601", "");
    prefixIndex             = regex_replace_compat(prefixIndex, "_XXX", "");

    // Create directory if does not exist
    struct stat st;

    if (stat(dir, &st) == -1)
    {
        if (errno == ENOENT)
        {
            DEBUGF(Logger::DBG_DEBUG, "Creating directory %s...", dir);
            if (_ccd_mkdir(dir, 0755) == -1)
                LOGF_ERROR("Error creating directory %s (%s)", dir, strerror(errno));
        }
        else
        {
            LOGF_ERROR("Couldn't stat directory %s: %s", dir, strerror(errno));
            return -1;
        }
    }

    dpdf = opendir(dir);
    if (dpdf != nullptr)
    {
        while ((epdf = readdir(dpdf)))
        {
            if (strstr(epdf->d_name, prefixIndex.c_str()))
                files.push_back(epdf->d_name);
        }
    }
    else
    {
        closedir(dpdf);
        return -1;
    }
    int maxIndex = 0;

    for (int i = 0; i < (int)files.size(); i++)
    {
        int index = -1;

        std::string file  = files.at(i);
        std::size_t start = file.find_last_of("_");
        std::size_t end   = file.find_last_of(".");
        if (start != std::string::npos)
        {
            index = atoi(file.substr(start + 1, end).c_str());
            if (index > maxIndex)
                maxIndex = index;
        }
    }

    closedir(dpdf);
    return (maxIndex + 1);
}

void CCD::GuideComplete(INDI_EQ_AXIS axis)
{
    GuiderInterface::GuideComplete(axis);
}

bool CCD::StartStreaming()
{
    LOG_ERROR("Streaming is not supported.");
    return false;
}

bool CCD::StopStreaming()
{
    LOG_ERROR("Streaming is not supported.");
    return false;
}

#ifdef HAVE_WEBSOCKET
void CCD::wsThreadHelper(void * context)
{
    static_cast<CCD *>(context)->wsThreadEntry();
}

void CCD::wsThreadEntry()
{
    wsServer.run();
}
#endif

}
