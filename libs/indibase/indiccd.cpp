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
#include "locale_compat.h"
#include "indiutility.h"

#ifdef HAVE_XISF
#include <libxisf.h>
#endif

#include <fitsio.h>

#include <libnova/julian_day.h>
#include <libnova/precession.h>
#include <libnova/airmass.h>
#include <libnova/transform.h>
#include <libnova/ln_types.h>
#include <libastro.h>

#include <iomanip>
#include <cmath>
#include <regex>
#include <iterator>
#include <variant>

#include <dirent.h>
#include <cerrno>
#include <cstdlib>
#include <zlib.h>
#include <sys/stat.h>

const char * IMAGE_SETTINGS_TAB = "Image Settings";
const char * IMAGE_INFO_TAB     = "Image Info";
const char * GUIDE_HEAD_TAB     = "Guider Head";
//const char * RAPIDGUIDE_TAB     = "Rapid Guide";

#ifdef HAVE_WEBSOCKET
uint16_t INDIWSServer::m_global_port = 11623;
#endif

std::string join(std::vector<std::string> const &strings, std::string delim)
{
    std::stringstream ss;
    std::copy(strings.begin(), strings.end(),
              std::ostream_iterator<std::string>(ss, delim.c_str()));
    return ss.str();
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
    pierSide        = -1;
    J2000RA         = std::numeric_limits<double>::quiet_NaN();
    J2000DE         = std::numeric_limits<double>::quiet_NaN();
    J2000Valid      = false;
    MPSAS           = std::numeric_limits<double>::quiet_NaN();
    RotatorAngle    = std::numeric_limits<double>::quiet_NaN();
    // JJ ed 2019-12-10
    FocuserPos      = -1;
    FocuserTemp     = std::numeric_limits<double>::quiet_NaN();

    Airmass         = std::numeric_limits<double>::quiet_NaN();
    Latitude        = std::numeric_limits<double>::quiet_NaN();
    Longitude       = std::numeric_limits<double>::quiet_NaN();
    Azimuth         = std::numeric_limits<double>::quiet_NaN();
    Altitude        = std::numeric_limits<double>::quiet_NaN();
    snoopedAperture        = std::numeric_limits<double>::quiet_NaN();
    snoopedFocalLength     = std::numeric_limits<double>::quiet_NaN();

    // Check temperature every 5 seconds.
    m_TemperatureCheckTimer.setInterval(5000);
    m_TemperatureCheckTimer.callOnTimeout(std::bind(&CCD::checkTemperatureTarget, this));

    exposureStartTime[0] = 0;
    exposureDuration = 0.0;
}

CCD::~CCD()
{
    // Only update if index is different.
    if (m_ConfigFastExposureIndex != IUFindOnSwitchIndex(&FastExposureToggleSP))
        saveConfig(true, FastExposureToggleSP.name);
}

void CCD::SetCCDCapability(uint32_t cap)
{
    capability = cap;

    if (HasST4Port())
        setDriverInterface(getDriverInterface() | GUIDER_INTERFACE);
    else
        setDriverInterface(getDriverInterface() & ~GUIDER_INTERFACE);

    syncDriverInfo();
    HasStreaming();
    HasDSP();
}

bool CCD::initProperties()
{
    DefaultDevice::initProperties();

    // CCD Temperature
    IUFillNumber(&TemperatureN[0], "CCD_TEMPERATURE_VALUE", "Temperature (C)", "%5.2f", -50.0, 50.0, 0., 0.);
    IUFillNumberVector(&TemperatureNP, TemperatureN, 1, getDeviceName(), "CCD_TEMPERATURE", "Temperature",
                       MAIN_CONTROL_TAB, IP_RW, 60, IPS_IDLE);

    // Camera temperature ramp
    TemperatureRampNP[RAMP_SLOPE].fill("RAMP_SLOPE", "Max. dT (C/min)", "%.f", 0, 30, 1, 0);
    TemperatureRampNP[RAMP_THRESHOLD].fill("RAMP_THRESHOLD", "Threshold (C)", "%.1f", 0.1, 2, 0.1, 0.2);
    TemperatureRampNP.fill(getDeviceName(), "CCD_TEMP_RAMP", "Temp. Ramp", MAIN_CONTROL_TAB, IP_RW, 60, IPS_IDLE);

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
                       "Type", IMAGE_SETTINGS_TAB, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);

    // Primary CCD Exposure
    IUFillNumber(&PrimaryCCD.ImageExposureN[0], "CCD_EXPOSURE_VALUE", "Duration (s)", "%5.2f", 0.01, 3600, 1.0, 1.0);
    IUFillNumberVector(&PrimaryCCD.ImageExposureNP, PrimaryCCD.ImageExposureN, 1, getDeviceName(), "CCD_EXPOSURE",
                       "Expose", MAIN_CONTROL_TAB, IP_RW, 60, IPS_IDLE);

    // Primary CCD Abort
    IUFillSwitch(&PrimaryCCD.AbortExposureS[0], "ABORT", "Abort", ISS_OFF);
    IUFillSwitchVector(&PrimaryCCD.AbortExposureSP, PrimaryCCD.AbortExposureS, 1, getDeviceName(), "CCD_ABORT_EXPOSURE",
                       "Abort", MAIN_CONTROL_TAB, IP_RW, ISR_ATMOST1, 60, IPS_IDLE);

    // Primary CCD Binning
    IUFillNumber(&PrimaryCCD.ImageBinN[0], "HOR_BIN", "X", "%2.0f", 1, 4, 1, 1);
    IUFillNumber(&PrimaryCCD.ImageBinN[1], "VER_BIN", "Y", "%2.0f", 1, 4, 1, 1);
    IUFillNumberVector(&PrimaryCCD.ImageBinNP, PrimaryCCD.ImageBinN, 2, getDeviceName(), "CCD_BINNING", "Binning",
                       IMAGE_SETTINGS_TAB, IP_RW, 60, IPS_IDLE);

    // Primary CCD Info
    IUFillNumber(&PrimaryCCD.ImagePixelSizeN[CCDChip::CCD_MAX_X], "CCD_MAX_X", "Max. Width", "%.f", 1, 16000, 0, 0);
    IUFillNumber(&PrimaryCCD.ImagePixelSizeN[CCDChip::CCD_MAX_Y], "CCD_MAX_Y", "Max. Height", "%.f", 1, 16000, 0, 0);
    IUFillNumber(&PrimaryCCD.ImagePixelSizeN[CCDChip::CCD_PIXEL_SIZE], "CCD_PIXEL_SIZE", "Pixel size (um)", "%.2f", 1,
                 40, 0, 0);
    IUFillNumber(&PrimaryCCD.ImagePixelSizeN[CCDChip::CCD_PIXEL_SIZE_X], "CCD_PIXEL_SIZE_X", "Pixel size X", "%.2f", 1,
                 40, 0, 0);
    IUFillNumber(&PrimaryCCD.ImagePixelSizeN[CCDChip::CCD_PIXEL_SIZE_Y], "CCD_PIXEL_SIZE_Y", "Pixel size Y", "%.2f", 1,
                 40, 0, 0);
    IUFillNumber(&PrimaryCCD.ImagePixelSizeN[CCDChip::CCD_BITSPERPIXEL], "CCD_BITSPERPIXEL", "Bits per pixel", "%.f",
                 8, 64, 0, 0);
    IUFillNumberVector(&PrimaryCCD.ImagePixelSizeNP, PrimaryCCD.ImagePixelSizeN, 6, getDeviceName(), "CCD_INFO",
                       "CCD Information", IMAGE_INFO_TAB, IP_RO, 60, IPS_IDLE);

    // Primary CCD Compression Options
    IUFillSwitch(&PrimaryCCD.CompressS[INDI_ENABLED], "INDI_ENABLED", "Enabled", ISS_OFF);
    IUFillSwitch(&PrimaryCCD.CompressS[INDI_DISABLED], "INDI_DISABLED", "Disabled", ISS_ON);
    IUFillSwitchVector(&PrimaryCCD.CompressSP, PrimaryCCD.CompressS, 2, getDeviceName(), "CCD_COMPRESSION", "Compression",
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
                       "Info", IMAGE_INFO_TAB, IP_RO, 60, IPS_IDLE);

    IUFillSwitch(&GuideCCD.FrameTypeS[0], "FRAME_LIGHT", "Light", ISS_ON);
    IUFillSwitch(&GuideCCD.FrameTypeS[1], "FRAME_BIAS", "Bias", ISS_OFF);
    IUFillSwitch(&GuideCCD.FrameTypeS[2], "FRAME_DARK", "Dark", ISS_OFF);
    IUFillSwitch(&GuideCCD.FrameTypeS[3], "FRAME_FLAT", "Flat", ISS_OFF);
    IUFillSwitchVector(&GuideCCD.FrameTypeSP, GuideCCD.FrameTypeS, 4, getDeviceName(), "GUIDER_FRAME_TYPE",
                       "Type", GUIDE_HEAD_TAB, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);

    IUFillNumber(&GuideCCD.ImageExposureN[0], "GUIDER_EXPOSURE_VALUE", "Duration (s)", "%5.2f", 0.01, 3600, 1.0, 1.0);
    IUFillNumberVector(&GuideCCD.ImageExposureNP, GuideCCD.ImageExposureN, 1, getDeviceName(), "GUIDER_EXPOSURE",
                       "Guide Head", MAIN_CONTROL_TAB, IP_RW, 60, IPS_IDLE);

    IUFillSwitch(&GuideCCD.AbortExposureS[0], "ABORT", "Abort", ISS_OFF);
    IUFillSwitchVector(&GuideCCD.AbortExposureSP, GuideCCD.AbortExposureS, 1, getDeviceName(), "GUIDER_ABORT_EXPOSURE",
                       "Abort", MAIN_CONTROL_TAB, IP_RW, ISR_ATMOST1, 60, IPS_IDLE);

    IUFillSwitch(&GuideCCD.CompressS[INDI_ENABLED], "INDI_ENABLED", "Enabled", ISS_OFF);
    IUFillSwitch(&GuideCCD.CompressS[INDI_DISABLED], "INDI_DISABLED", "Disabled", ISS_ON);
    IUFillSwitchVector(&GuideCCD.CompressSP, GuideCCD.CompressS, 2, getDeviceName(), "GUIDER_COMPRESSION", "Compression",
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

    ScopeInfoNP[FocalLength].fill("FOCAL_LENGTH", "Focal Length (mm)", "%.2f", 10, 10000, 100, 0);
    ScopeInfoNP[Aperture].fill("APERTURE", "Aperture (mm)", "%.2f", 10, 3000, 100, 0);
    ScopeInfoNP.fill(getDeviceName(), "SCOPE_INFO", "Scope", OPTIONS_TAB, IP_RW, 60, IPS_IDLE);

    /**********************************************/
    /************** Capture Format ***************/
    /**********************************************/
    char configName[64] = {0};
    if (IUGetConfigOnSwitchName(getDeviceName(), "CCD_CAPTURE_FORMAT", configName, MAXINDINAME) == 0)
        m_ConfigCaptureFormatName = configName;
    CaptureFormatSP.fill(getDeviceName(), "CCD_CAPTURE_FORMAT", "Format", IMAGE_SETTINGS_TAB, IP_RW, ISR_1OFMANY, 60,
                         IPS_IDLE);

    m_ConfigEncodeFormatIndex = FORMAT_FITS;
    IUGetConfigOnSwitchIndex(getDeviceName(), "CCD_TRANSFER_FORMAT", &m_ConfigEncodeFormatIndex);
    EncodeFormatSP[FORMAT_FITS].fill("FORMAT_FITS", "FITS",
                                     m_ConfigEncodeFormatIndex == FORMAT_FITS ? ISS_ON : ISS_OFF);
    EncodeFormatSP[FORMAT_NATIVE].fill("FORMAT_NATIVE", "Native",
                                       m_ConfigEncodeFormatIndex == FORMAT_NATIVE ? ISS_ON : ISS_OFF);
#ifdef HAVE_XISF
    EncodeFormatSP[FORMAT_XISF].fill("FORMAT_XISF", "XISF",
                                     m_ConfigEncodeFormatIndex == FORMAT_XISF ? ISS_ON : ISS_OFF);
#endif
    EncodeFormatSP.fill(getDeviceName(), "CCD_TRANSFER_FORMAT", "Encode", IMAGE_SETTINGS_TAB, IP_RW, ISR_1OFMANY, 60,
                        IPS_IDLE);

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

    FITSHeaderTP[KEYWORD_NAME].fill("KEYWORD_NAME", "Name", nullptr);
    FITSHeaderTP[KEYWORD_VALUE].fill("KEYWORD_VALUE", "Value", nullptr);
    FITSHeaderTP[KEYWORD_COMMENT].fill("KEYWORD_COMMENT", "Comment", nullptr);
    FITSHeaderTP.fill(getDeviceName(), "FITS_HEADER", "FITS Header", INFO_TAB, IP_WO, 60, IPS_IDLE);

    /**********************************************/
    /****************** Exposure Looping **********/
    /***************** Primary CCD Only ***********/
    IUGetConfigOnSwitchIndex(getDeviceName(), FastExposureToggleSP.name, &m_ConfigFastExposureIndex);
    IUFillSwitch(&FastExposureToggleS[INDI_ENABLED], "INDI_ENABLED", "Enabled",
                 m_ConfigFastExposureIndex == INDI_ENABLED ? ISS_ON : ISS_OFF);
    IUFillSwitch(&FastExposureToggleS[INDI_DISABLED], "INDI_DISABLED", "Disabled",
                 m_ConfigFastExposureIndex == INDI_DISABLED ? ISS_ON : ISS_OFF);
    IUFillSwitchVector(&FastExposureToggleSP, FastExposureToggleS, 2, getDeviceName(), "CCD_FAST_TOGGLE", "Fast Exposure",
                       OPTIONS_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    // CCD Should loop until the number of frames specified in this property is completed
    IUFillNumber(&FastExposureCountN[0], "FRAMES", "Frames", "%.f", 0, 100000, 1, 1);
    IUFillNumberVector(&FastExposureCountNP, FastExposureCountN, 1, getDeviceName(), "CCD_FAST_COUNT", "Fast Count",
                       OPTIONS_TAB, IP_RW, 0, IPS_IDLE);

    /**********************************************/
    /**************** Web Socket ******************/
    /**********************************************/
    IUFillSwitch(&WebSocketS[WEBSOCKET_ENABLED], "WEBSOCKET_ENABLED", "Enabled", ISS_OFF);
    IUFillSwitch(&WebSocketS[WEBSOCKET_DISABLED], "WEBSOCKET_DISABLED", "Disabled", ISS_ON);
    IUFillSwitchVector(&WebSocketSP, WebSocketS, 2, getDeviceName(), "CCD_WEBSOCKET", "Websocket", OPTIONS_TAB,
                       IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    IUFillNumber(&WebSocketSettingsN[WS_SETTINGS_PORT], "WS_SETTINGS_PORT", "Port", "%.f", 0, 50000, 0, 0);
    IUFillNumberVector(&WebSocketSettingsNP, WebSocketSettingsN, 1, getDeviceName(), "CCD_WEBSOCKET_SETTINGS", "WS Settings",
                       OPTIONS_TAB, IP_RW,
                       60, IPS_IDLE);

    /**********************************************/
    /**************** Snooping ********************/
    /**********************************************/

    // Snooped Devices

    // Load from config
    char telescope[MAXINDIDEVICE] = {"Telescope Simulator"};
    IUGetConfigText(getDeviceName(), "ACTIVE_DEVICES", "ACTIVE_TELESCOPE", telescope, MAXINDIDEVICE);
    char rotator[MAXINDIDEVICE] = {"Rotator Simulator"};
    IUGetConfigText(getDeviceName(), "ACTIVE_DEVICES", "ACTIVE_ROTATOR", rotator, MAXINDIDEVICE);
    char focuser[MAXINDIDEVICE] = {"Focuser Simulator"};
    IUGetConfigText(getDeviceName(), "ACTIVE_DEVICES", "ACTIVE_FOCUSER", focuser, MAXINDIDEVICE);
    char filter[MAXINDIDEVICE] = {"CCD Simulator"};
    IUGetConfigText(getDeviceName(), "ACTIVE_DEVICES", "ACTIVE_FILTER", filter, MAXINDIDEVICE);
    char skyquality[MAXINDIDEVICE] = {"SQM"};
    IUGetConfigText(getDeviceName(), "ACTIVE_DEVICES", "ACTIVE_SKYQUALITY", skyquality, MAXINDIDEVICE);

    IUFillText(&ActiveDeviceT[ACTIVE_TELESCOPE], "ACTIVE_TELESCOPE", "Telescope", telescope);
    IUFillText(&ActiveDeviceT[ACTIVE_ROTATOR], "ACTIVE_ROTATOR", "Rotator", rotator);
    IUFillText(&ActiveDeviceT[ACTIVE_FOCUSER], "ACTIVE_FOCUSER", "Focuser", focuser);
    IUFillText(&ActiveDeviceT[ACTIVE_FILTER], "ACTIVE_FILTER", "Filter", filter);
    IUFillText(&ActiveDeviceT[ACTIVE_SKYQUALITY], "ACTIVE_SKYQUALITY", "Sky Quality", skyquality);
    IUFillTextVector(&ActiveDeviceTP, ActiveDeviceT, 5, getDeviceName(), "ACTIVE_DEVICES", "Snoop devices", OPTIONS_TAB,
                     IP_RW, 60, IPS_IDLE);

    auto mount = ActiveDeviceT[ACTIVE_TELESCOPE].text ? ActiveDeviceT[ACTIVE_TELESCOPE].text : "";
    // Snooped RA/DEC Property
    IUFillNumber(&EqN[0], "RA", "Ra (hh:mm:ss)", "%010.6m", 0, 24, 0, 0);
    IUFillNumber(&EqN[1], "DEC", "Dec (dd:mm:ss)", "%010.6m", -90, 90, 0, 0);
    IUFillNumberVector(&EqNP, EqN, 2, mount, "EQUATORIAL_EOD_COORD", "EQ Coord", "Main Control", IP_RW, 60, IPS_IDLE);

    // Snooped J2000 RA/DEC Property
    IUFillNumber(&J2000EqN[0], "RA", "Ra (hh:mm:ss)", "%010.6m", 0, 24, 0, 0);
    IUFillNumber(&J2000EqN[1], "DEC", "Dec (dd:mm:ss)", "%010.6m", -90, 90, 0, 0);
    IUFillNumberVector(&J2000EqNP, J2000EqN, 2, mount, "EQUATORIAL_COORD", "J2000 EQ Coord",
                       "Main Control", IP_RW,
                       60, IPS_IDLE);

    // Snoop properties of interest

    // Snoop mount
    IDSnoopDevice(mount, "EQUATORIAL_EOD_COORD");
    IDSnoopDevice(mount, "EQUATORIAL_COORD");
    IDSnoopDevice(mount, "TELESCOPE_INFO");
    IDSnoopDevice(mount, "GEOGRAPHIC_COORD");
    IDSnoopDevice(mount, "TELESCOPE_PIER_SIDE");

    // Snoop Rotator
    IDSnoopDevice(ActiveDeviceT[ACTIVE_ROTATOR].text, "ABS_ROTATOR_ANGLE");

    // JJ ed 2019-12-10
    // Snoop Focuser
    IDSnoopDevice(ActiveDeviceT[ACTIVE_FOCUSER].text, "ABS_FOCUS_POSITION");
    IDSnoopDevice(ActiveDeviceT[ACTIVE_FOCUSER].text, "FOCUS_TEMPERATURE");
    //

    // Snoop Filter Wheel
    IDSnoopDevice(ActiveDeviceT[ACTIVE_FILTER].text, "FILTER_SLOT");
    IDSnoopDevice(ActiveDeviceT[ACTIVE_FILTER].text, "FILTER_NAME");

    // Snoop Sky Quality Meter
    IDSnoopDevice(ActiveDeviceT[ACTIVE_SKYQUALITY].text, "SKY_QUALITY");

    // Guider Interface
    initGuiderProperties(getDeviceName(), GUIDE_CONTROL_TAB);

    addPollPeriodControl();

    setDriverInterface(CCD_INTERFACE);

    return true;
}

void CCD::ISGetProperties(const char * dev)
{
    DefaultDevice::ISGetProperties(dev);
    defineProperty(&ActiveDeviceTP);

    if (HasStreaming())
        Streamer->ISGetProperties(dev);

    if (HasDSP())
        DSP->ISGetProperties(dev);
}

bool CCD::updateProperties()
{
    //IDLog("CCD UpdateProperties isConnected returns %d %d\n",isConnected(),Connected);
    if (isConnected())
    {
        defineProperty(&PrimaryCCD.ImageExposureNP);

        if (CanAbort())
            defineProperty(&PrimaryCCD.AbortExposureSP);
        if (CanSubFrame() == false)
            PrimaryCCD.ImageFrameNP.p = IP_RO;

        defineProperty(&PrimaryCCD.ImageFrameNP);
        if (CanBin() || CanSubFrame())
            defineProperty(&PrimaryCCD.ResetSP);

        if (CanBin())
            defineProperty(&PrimaryCCD.ImageBinNP);

        defineProperty(FITSHeaderTP);

        if (HasGuideHead())
        {
            defineProperty(&GuideCCD.ImageExposureNP);
            if (CanAbort())
                defineProperty(&GuideCCD.AbortExposureSP);
            if (CanSubFrame() == false)
                GuideCCD.ImageFrameNP.p = IP_RO;
            defineProperty(&GuideCCD.ImageFrameNP);
        }

        if (HasCooler())
        {
            defineProperty(&TemperatureNP);
            defineProperty(TemperatureRampNP);
        }

        defineProperty(CaptureFormatSP);
        defineProperty(EncodeFormatSP);

        defineProperty(&PrimaryCCD.ImagePixelSizeNP);
        if (HasGuideHead())
        {
            defineProperty(&GuideCCD.ImagePixelSizeNP);
            if (CanBin())
                defineProperty(&GuideCCD.ImageBinNP);
        }
        defineProperty(&PrimaryCCD.CompressSP);
        defineProperty(&PrimaryCCD.FitsBP);
        if (HasGuideHead())
        {
            defineProperty(&GuideCCD.CompressSP);
            defineProperty(&GuideCCD.FitsBP);
        }
        if (HasST4Port())
        {
            defineProperty(&GuideNSNP);
            defineProperty(&GuideWENP);
        }
        defineProperty(&PrimaryCCD.FrameTypeSP);

        if (HasGuideHead())
            defineProperty(&GuideCCD.FrameTypeSP);

        if (HasBayer())
            defineProperty(&BayerTP);

#if 0
        defineProperty(&PrimaryCCD.RapidGuideSP);

        if (HasGuideHead())
            defineProperty(&GuideCCD.RapidGuideSP);

        if (RapidGuideEnabled)
        {
            defineProperty(&PrimaryCCD.RapidGuideSetupSP);
            defineProperty(&PrimaryCCD.RapidGuideDataNP);
        }
        if (GuiderRapidGuideEnabled)
        {
            defineProperty(&GuideCCD.RapidGuideSetupSP);
            defineProperty(&GuideCCD.RapidGuideDataNP);
        }
#endif
        defineProperty(ScopeInfoNP);

        defineProperty(&WorldCoordSP);
        defineProperty(&UploadSP);

        if (UploadSettingsT[UPLOAD_DIR].text == nullptr)
            IUSaveText(&UploadSettingsT[UPLOAD_DIR], getenv("HOME"));
        defineProperty(&UploadSettingsTP);

#ifdef HAVE_WEBSOCKET
        if (HasWebSocket())
            defineProperty(&WebSocketSP);
#endif

        defineProperty(&FastExposureToggleSP);
        defineProperty(&FastExposureCountNP);
    }
    else
    {
        deleteProperty(PrimaryCCD.ImageFrameNP.name);
        if (CanBin() || CanSubFrame())
            deleteProperty(PrimaryCCD.ResetSP.name);

        deleteProperty(PrimaryCCD.ImagePixelSizeNP.name);

        deleteProperty(CaptureFormatSP.getName());
        deleteProperty(EncodeFormatSP.getName());

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

        deleteProperty(FITSHeaderTP);

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
        {
            deleteProperty(TemperatureNP.name);
            deleteProperty(TemperatureRampNP.getName());
        }
        if (HasST4Port())
        {
            deleteProperty(GuideNSNP.name);
            deleteProperty(GuideWENP.name);
        }
        deleteProperty(PrimaryCCD.FrameTypeSP.name);
        if (HasBayer())
            deleteProperty(BayerTP.name);
        deleteProperty(ScopeInfoNP);

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
        deleteProperty(FastExposureToggleSP.name);
        deleteProperty(FastExposureCountNP.name);
    }

    // Streamer
    if (HasStreaming())
        Streamer->updateProperties();

    // DSP
    if (HasDSP())
        DSP->updateProperties();

    return true;
}

bool CCD::ISSnoopDevice(XMLEle * root)
{
    XMLEle * ep           = nullptr;
    const char * propName = findXMLAttValu(root, "name");

    if (IUSnoopNumber(root, &EqNP) == 0)
    {
        double newra, newdec;
        newra  = EqN[0].value;
        newdec = EqN[1].value;
        if ((newra != RA) || (newdec != Dec))
        {
            //IDLog("RA %4.2f  Dec %4.2f Snooped RA %4.2f  Dec %4.2f\n",RA,Dec,newra,newdec);
            RA  = newra;
            Dec = newdec;
        }
    }
    else if (IUSnoopNumber(root, &J2000EqNP) == 0)
    {
        float newra, newdec;
        newra  = J2000EqN[0].value;
        newdec = J2000EqN[1].value;
        if ((newra != J2000RA) || (newdec != J2000DE))
        {
            //    	    IDLog("J2000 RA %4.2f  Dec %4.2f Snooped RA %4.2f  Dec %4.2f\n",J2000RA,J2000DE,newra,newdec);
            J2000RA = newra;
            J2000DE = newdec;
        }
        J2000Valid = true;
    }
    else if (!strcmp("TELESCOPE_PIER_SIDE", propName))
    {
        // set default to say we have no valid information from mount
        pierSide = -1;
        //  crack the message
        for (ep = nextXMLEle(root, 1); ep != nullptr; ep = nextXMLEle(root, 0))
        {
            const char * elemName = findXMLAttValu(ep, "name");

            if (!strcmp(elemName, "PIER_EAST") && !strcmp(pcdataXMLEle(ep), "On"))
                pierSide = 1;
            else if (!strcmp(elemName, "PIER_WEST") && !strcmp(pcdataXMLEle(ep), "On"))
                pierSide = 0;
        }
    }
    else if (!strcmp(propName, "TELESCOPE_INFO"))
    {
        for (ep = nextXMLEle(root, 1); ep != nullptr; ep = nextXMLEle(root, 0))
        {
            const char * name = findXMLAttValu(ep, "name");

            if (!strcmp(name, "TELESCOPE_APERTURE"))
            {
                snoopedAperture = atof(pcdataXMLEle(ep));
            }
            else if (!strcmp(name, "TELESCOPE_FOCAL_LENGTH"))
            {
                snoopedFocalLength = atof(pcdataXMLEle(ep));
            }
        }
    }
    else if (!strcmp(propName, "FILTER_NAME"))
    {
        LOG_DEBUG("SNOOP: FILTER_NAME update...");
        FilterNames.clear();
        for (ep = nextXMLEle(root, 1); ep != nullptr; ep = nextXMLEle(root, 0))
            FilterNames.push_back(pcdataXMLEle(ep));
        LOGF_DEBUG("SNOOP: FILTER_NAME -> %s", join(FilterNames, ", ").c_str());

    }
    else if (!strcmp(propName, "FILTER_SLOT"))
    {
        LOG_DEBUG("SNOOP: FILTER_SLOT update...");
        CurrentFilterSlot = -1;
        for (ep = nextXMLEle(root, 1); ep != nullptr; ep = nextXMLEle(root, 0))
            CurrentFilterSlot = atoi(pcdataXMLEle(ep));
        LOGF_DEBUG("SNOOP: FILTER_SLOT is %d", CurrentFilterSlot);
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

    // JJ ed 2019-12-10
    else if (!strcmp(propName, "ABS_FOCUS_POSITION"))
    {
        for (ep = nextXMLEle(root, 1); ep != nullptr; ep = nextXMLEle(root, 0))
        {
            const char * name = findXMLAttValu(ep, "name");

            if (!strcmp(name, "FOCUS_ABSOLUTE_POSITION"))
            {
                FocuserPos = atol(pcdataXMLEle(ep));
                break;
            }
        }
    }
    else if (!strcmp(propName, "FOCUS_TEMPERATURE"))
    {
        for (ep = nextXMLEle(root, 1); ep != nullptr; ep = nextXMLEle(root, 0))
        {
            const char * name = findXMLAttValu(ep, "name");

            if (!strcmp(name, "TEMPERATURE"))
            {
                FocuserTemp = atof(pcdataXMLEle(ep));
                break;
            }
        }
    }
    //

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
            strncpy(EqNP.device, ActiveDeviceT[ACTIVE_TELESCOPE].text, MAXINDIDEVICE);
            strncpy(J2000EqNP.device, ActiveDeviceT[ACTIVE_TELESCOPE].text, MAXINDIDEVICE);
            if (strlen(ActiveDeviceT[ACTIVE_TELESCOPE].text) > 0)
            {
                LOGF_DEBUG("Snopping on Mount %s", ActiveDeviceT[ACTIVE_TELESCOPE].text);
                IDSnoopDevice(ActiveDeviceT[ACTIVE_TELESCOPE].text, "EQUATORIAL_EOD_COORD");
                IDSnoopDevice(ActiveDeviceT[ACTIVE_TELESCOPE].text, "EQUATORIAL_COORD");
                IDSnoopDevice(ActiveDeviceT[ACTIVE_TELESCOPE].text, "TELESCOPE_INFO");
                IDSnoopDevice(ActiveDeviceT[ACTIVE_TELESCOPE].text, "GEOGRAPHIC_COORD");
            }
            else
            {
                LOG_DEBUG("No mount is set. Clearing all mount watchers.");
                RA = std::numeric_limits<double>::quiet_NaN();
                Dec = std::numeric_limits<double>::quiet_NaN();
                J2000RA = std::numeric_limits<double>::quiet_NaN();
                J2000DE = std::numeric_limits<double>::quiet_NaN();
                Latitude = std::numeric_limits<double>::quiet_NaN();
                Longitude = std::numeric_limits<double>::quiet_NaN();
                Airmass = std::numeric_limits<double>::quiet_NaN();
                Azimuth = std::numeric_limits<double>::quiet_NaN();
                Altitude = std::numeric_limits<double>::quiet_NaN();
            }

            if (strlen(ActiveDeviceT[ACTIVE_ROTATOR].text) > 0)
            {
                LOGF_DEBUG("Snopping on Rotator %s", ActiveDeviceT[ACTIVE_ROTATOR].text);
                IDSnoopDevice(ActiveDeviceT[ACTIVE_ROTATOR].text, "ABS_ROTATOR_ANGLE");
            }
            else
            {
                LOG_DEBUG("No rotator is set. Clearing all rotator watchers.");
                MPSAS = std::numeric_limits<double>::quiet_NaN();
            }

            // JJ ed 2019-12-10
            if (strlen(ActiveDeviceT[ACTIVE_FOCUSER].text) > 0)
            {
                LOGF_DEBUG("Snopping on Focuser %s", ActiveDeviceT[ACTIVE_FOCUSER].text);
                IDSnoopDevice(ActiveDeviceT[ACTIVE_FOCUSER].text, "ABS_FOCUS_POSITION");
                IDSnoopDevice(ActiveDeviceT[ACTIVE_FOCUSER].text, "FOCUS_TEMPERATURE");
            }
            else
            {
                LOG_DEBUG("No focuser is set. Clearing all focuser watchers.");
                FocuserPos = -1;
                FocuserTemp = std::numeric_limits<double>::quiet_NaN();
            }


            if (strlen(ActiveDeviceT[ACTIVE_FILTER].text) > 0)
            {
                LOGF_DEBUG("Snopping on Filter Wheel %s", ActiveDeviceT[ACTIVE_FILTER].text);
                IDSnoopDevice(ActiveDeviceT[ACTIVE_FILTER].text, "FILTER_SLOT");
                IDSnoopDevice(ActiveDeviceT[ACTIVE_FILTER].text, "FILTER_NAME");
            }
            else
            {
                LOG_DEBUG("No filter wheel is set. Clearing All filter wheel watchers.");
                CurrentFilterSlot = -1;
            }

            IDSnoopDevice(ActiveDeviceT[ACTIVE_SKYQUALITY].text, "SKY_QUALITY");

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

        // FITS Header
        if (FITSHeaderTP.isNameMatch(name))
        {
            FITSHeaderTP.update(texts, names, n);

            std::string name = FITSHeaderTP[KEYWORD_NAME].getText();
            std::string value = FITSHeaderTP[KEYWORD_VALUE].getText();
            std::string comment = FITSHeaderTP[KEYWORD_COMMENT].getText();

            if (name.empty() && value.empty() && comment.empty())
            {
                LOG_ERROR("Cannot add an empty FITS record.");
                FITSHeaderTP.setState(IPS_ALERT);
            }
            else
            {
                FITSHeaderTP.setState(IPS_OK);
                // Specical keyword
                if (name == "INDI_CLEAR")
                {
                    m_CustomFITSKeywords.clear();
                    LOG_INFO("Custom FITS headers cleared.");
                }
                else if (name.empty() == false && value.empty() == false)
                {
                    // Double regex
                    std::regex checkDouble("^[-+]?([0-9]*?[.,][0-9]+|[0-9]+)$");
                    // Integer regex
                    std::regex checkInteger("^[-+]?([0-9]*)$");

                    try
                    {
                        // Try long
                        if (std::regex_match(value, checkInteger))
                        {
                            auto lValue = std::stol(value);
                            FITSRecord record(name.c_str(), lValue, comment.c_str());
                            m_CustomFITSKeywords[name.c_str()] = record;
                        }
                        // Try double
                        else if (std::regex_match(value, checkDouble))
                        {
                            auto dValue = std::stod(value);
                            FITSRecord record(name.c_str(), dValue, 6, comment.c_str());
                            m_CustomFITSKeywords[name.c_str()] = record;
                        }
                        // Store as text
                        else
                        {
                            // String
                            FITSRecord record(name.c_str(), value.c_str(), comment.c_str());
                            m_CustomFITSKeywords[name.c_str()] = record;
                        }
                    }
                    // In case conversion fails
                    catch (std::exception &e)
                    {
                        // String
                        FITSRecord record(name.c_str(), value.c_str(), comment.c_str());
                        m_CustomFITSKeywords[name.c_str()] = record;
                    }
                }
                else if (comment.empty() == false)
                {
                    FITSRecord record(comment.c_str());
                    m_CustomFITSKeywords[comment.c_str()] = record;
                }
            }

            FITSHeaderTP.apply();
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

    // DSP
    if (HasDSP())
        DSP->ISNewText(dev, name, texts, names, n);

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
            //if (PrimaryCCD.ImageExposureNP.s == IPS_BUSY && FastExposureToggleS[INDI_DISABLED].s == ISS_ON)
            if (PrimaryCCD.ImageExposureNP.s == IPS_BUSY)
            {
                if (CanAbort() && AbortExposure() == false)
                    DEBUG(Logger::DBG_WARNING, "Warning: Aborting exposure failed.");
            }

            if (StartExposure(ExposureTime))
            {
                PrimaryCCD.ImageExposureNP.s = IPS_BUSY;
                if (ExposureTime * 1000 < getCurrentPollingPeriod())
                    setCurrentPollingPeriod(ExposureTime * 950);
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
            else if (values[0] == 0 || values[1] == 0)
            {
                PrimaryCCD.ImageBinNP.s = IPS_ALERT;
                IDSetNumber(&PrimaryCCD.ImageBinNP, nullptr);
                LOGF_ERROR("%.fx%.f binning is invalid.", values[0], values[1]);
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
            else if (values[0] == 0 || values[1] == 0)
            {
                PrimaryCCD.ImageBinNP.s = IPS_ALERT;
                IDSetNumber(&PrimaryCCD.ImageBinNP, nullptr);
                LOGF_ERROR("%.fx%.f binning is invalid.", values[0], values[1]);
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

        // Scope Information
        if (ScopeInfoNP.isNameMatch(name))
        {
            ScopeInfoNP.update(values, names, n);
            ScopeInfoNP.setState(IPS_OK);
            ScopeInfoNP.apply();
            saveConfig(true, ScopeInfoNP.getName());
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

        // Fast Exposure Count
        if (!strcmp(name, FastExposureCountNP.name))
        {
            IUUpdateNumber(&FastExposureCountNP, values, names, n);
            FastExposureCountNP.s = IPS_OK;
            IDSetNumber(&FastExposureCountNP, nullptr);
            return true;
        }

        // CCD TEMPERATURE
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

            double nextTemperature = values[0];
            // If temperature ramp is enabled, find
            if (TemperatureRampNP[RAMP_SLOPE].getValue() != 0)
            {
                if (values[0] < TemperatureN[0].value)
                {
                    nextTemperature = std::max(values[0], TemperatureN[0].value - TemperatureRampNP[RAMP_SLOPE].getValue());
                }
                // Going up
                else
                {
                    nextTemperature = std::min(values[0], TemperatureN[0].value + TemperatureRampNP[RAMP_SLOPE].getValue());
                }
            }

            int rc = SetTemperature(nextTemperature);

            if (rc == 0)
            {
                if (TemperatureRampNP[RAMP_SLOPE].getValue() != 0)
                    m_TemperatureElapsedTimer.start();

                m_TargetTemperature = values[0];
                m_TemperatureCheckTimer.start();
                TemperatureNP.s = IPS_BUSY;
            }
            else if (rc == 1)
                TemperatureNP.s = IPS_OK;
            else
                TemperatureNP.s = IPS_ALERT;

            IDSetNumber(&TemperatureNP, nullptr);
            return true;
        }

        // Camera Temperature Ramp
        if (!strcmp(name, TemperatureRampNP.getName()))
        {
            double previousSlope     = TemperatureRampNP[RAMP_SLOPE].getValue();
            double previousThreshold = TemperatureRampNP[RAMP_THRESHOLD].getValue();
            TemperatureRampNP.update(values, names, n);
            TemperatureRampNP.setState(IPS_OK);
            TemperatureRampNP.apply();
            if (TemperatureRampNP[0].getValue() == 0)
                LOG_INFO("Temperature ramp is disabled.");
            else
                LOGF_INFO("Temperature ramp is enabled. Gradual cooling and warming is regulated at %.f Celsius per minute.",
                          TemperatureRampNP[0].getValue());

            // Save config if there is a change
            if (std::abs(previousSlope - TemperatureRampNP[RAMP_SLOPE].getValue()) > 0 ||
                    std::abs(previousThreshold - TemperatureRampNP[RAMP_THRESHOLD].getValue()) > 0.01)
                saveConfig(true, TemperatureRampNP.getName());
            return true;
        }

        // Primary CCD Info
        if (!strcmp(name, PrimaryCCD.ImagePixelSizeNP.name))
        {
            if (IUUpdateNumber(&PrimaryCCD.ImagePixelSizeNP, values, names, n) == 0)
            {
                PrimaryCCD.ImagePixelSizeNP.s = IPS_OK;
                SetCCDParams(PrimaryCCD.ImagePixelSizeNP.np[CCDChip::CCD_MAX_X].value,
                             PrimaryCCD.ImagePixelSizeNP.np[CCDChip::CCD_MAX_Y].value,
                             PrimaryCCD.getBPP(),
                             PrimaryCCD.ImagePixelSizeNP.np[CCDChip::CCD_PIXEL_SIZE_X].value,
                             PrimaryCCD.ImagePixelSizeNP.np[CCDChip::CCD_PIXEL_SIZE_Y].value);
                saveConfig(true, PrimaryCCD.ImagePixelSizeNP.name);
            }
            else
                PrimaryCCD.ImagePixelSizeNP.s = IPS_ALERT;

            IDSetNumber(&PrimaryCCD.ImagePixelSizeNP, nullptr);
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

    // DSP
    if (HasDSP())
        DSP->ISNewNumber(dev, name, values, names, n);

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
                    defineProperty(&FileNameTP);
                }
                else
                {
                    DEBUG(Logger::DBG_SESSION, "Upload settings set to client and local.");
                    defineProperty(&FileNameTP);
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

        // Fast Exposure Toggle
        if (!strcmp(name, FastExposureToggleSP.name))
        {
            IUUpdateSwitch(&FastExposureToggleSP, states, names, n);

            // Only display warning for the first time this is enabled.
            if (FastExposureToggleSP.s == IPS_IDLE && FastExposureToggleS[INDI_ENABLED].s == ISS_ON)
                LOG_WARN("Experimental Feature: After a frame is downloaded, the next frame capture immediately starts to avoid any delays.");

            if (FastExposureToggleS[INDI_DISABLED].s == ISS_ON)
            {
                FastExposureCountNP.s = IPS_IDLE;
                IDSetNumber(&FastExposureCountNP, nullptr);
                m_UploadTime = 0;
                if (PrimaryCCD.isExposing())
                    AbortExposure();
            }

            FastExposureToggleSP.s = IPS_OK;
            IDSetSwitch(&FastExposureToggleSP, nullptr);
            return true;
        }


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
                defineProperty(&WebSocketSettingsNP);
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
                defineProperty(&CCDRotationNP);
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

            setCurrentPollingPeriod(getPollingPeriod());

            // Fast Exposure Count
            if (FastExposureCountNP.s == IPS_BUSY)
            {
                m_UploadTime = 0;
                FastExposureCountNP.s = IPS_IDLE;
                FastExposureCountN[0].value = 1;
                IDSetNumber(&FastExposureCountNP, nullptr);
            }

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
            PrimaryCCD.SendCompressed = PrimaryCCD.CompressS[INDI_ENABLED].s == ISS_ON;
            return true;
        }

        // Guide Chip Compression
        if (strcmp(name, GuideCCD.CompressSP.name) == 0)
        {
            IUUpdateSwitch(&GuideCCD.CompressSP, states, names, n);
            GuideCCD.CompressSP.s = IPS_OK;
            IDSetSwitch(&GuideCCD.CompressSP, nullptr);
            GuideCCD.SendCompressed = GuideCCD.CompressS[INDI_ENABLED].s == ISS_ON;
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

        // Capture Format
        if (CaptureFormatSP.isNameMatch(name))
        {
            int previousIndex = CaptureFormatSP.findOnSwitchIndex();
            CaptureFormatSP.update(states, names, n);

            if (SetCaptureFormat(CaptureFormatSP.findOnSwitchIndex()))
                CaptureFormatSP.setState(IPS_OK);
            else
            {
                if (previousIndex >= 0)
                {
                    CaptureFormatSP.reset();
                    CaptureFormatSP[previousIndex].setState(ISS_ON);
                }
                CaptureFormatSP.setState(IPS_ALERT);
            }
            CaptureFormatSP.apply();

            if (m_ConfigCaptureFormatName != CaptureFormatSP.findOnSwitch()->getName())
            {
                m_ConfigCaptureFormatName = CaptureFormatSP.findOnSwitch()->getName();
                saveConfig(true, CaptureFormatSP.getName());
            }

            return true;
        }

        // Encode Format
        if (EncodeFormatSP.isNameMatch(name))
        {
            EncodeFormatSP.update(states, names, n);
            EncodeFormatSP.setState(IPS_OK);
            EncodeFormatSP.apply();

            if (m_ConfigEncodeFormatIndex != EncodeFormatSP.findOnSwitchIndex())
            {
                m_ConfigEncodeFormatIndex = EncodeFormatSP.findOnSwitchIndex();
                saveConfig(true, EncodeFormatSP.getName());
            }

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
                defineProperty(&PrimaryCCD.RapidGuideSetupSP);
                defineProperty(&PrimaryCCD.RapidGuideDataNP);
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
                defineProperty(&GuideCCD.RapidGuideSetupSP);
                defineProperty(&GuideCCD.RapidGuideDataNP);
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

    // DSP
    if (HasDSP())
        DSP->ISNewSwitch(dev, name, states, names, n);

    return DefaultDevice::ISNewSwitch(dev, name, states, names, n);
}

bool CCD::ISNewBLOB(const char *dev, const char *name, int sizes[], int blobsizes[], char *blobs[],
                    char *formats[], char *names[], int n)
{
    // DSP
    if (HasDSP())
        DSP->ISNewBLOB(dev, name, sizes, blobsizes, blobs, formats, names, n);

    return DefaultDevice::ISNewBLOB(dev, name, sizes, blobsizes, blobs, formats, names, n);
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

    // DSP
    if (HasDSP())
        DSP->setSizes(2, new int[2] { PrimaryCCD.getSubW() / hor, PrimaryCCD.getSubH() / ver });

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

void CCD::addFITSKeywords(CCDChip * targetChip, std::vector<FITSRecord> &fitsKeywords)
{
    char dev_name[MAXINDINAME] = {0};
    double effectiveFocalLength = std::numeric_limits<double>::quiet_NaN();
    double effectiveAperture = std::numeric_limits<double>::quiet_NaN();


    AutoCNumeric locale;
    fitsKeywords.push_back({"ROWORDER", "TOP-DOWN", "Row Order"});
    fitsKeywords.push_back({"INSTRUME", getDeviceName(), "CCD Name"});

    // Telescope
    if (strlen(ActiveDeviceT[ACTIVE_TELESCOPE].text) > 0)
    {
        fitsKeywords.push_back({"TELESCOP", ActiveDeviceT[0].text, "Telescope name"});
    }

    // Which scope is in effect
    // Prefer Scope Info over snooped property which should be deprecated.
    effectiveFocalLength = ScopeInfoNP[FocalLength].getValue() > 0 ?  ScopeInfoNP[FocalLength].getValue() : snoopedFocalLength;
    effectiveAperture = ScopeInfoNP[Aperture].getValue() > 0 ?  ScopeInfoNP[Aperture].getValue() : snoopedAperture;

    if (std::isnan(effectiveFocalLength))
        LOG_WARN("Telescope focal length is missing.");
    if (std::isnan(effectiveAperture))
        LOG_WARN("Telescope aperture is missing.");

    double subPixSize1 = static_cast<double>(targetChip->getPixelSizeX());
    double subPixSize2 = static_cast<double>(targetChip->getPixelSizeY());
    uint32_t subW = targetChip->getSubW();
    uint32_t subH = targetChip->getSubH();
    uint32_t subBinX = targetChip->getBinX();
    uint32_t subBinY = targetChip->getBinY();

    strncpy(dev_name, getDeviceName(), MAXINDINAME);

    fitsKeywords.push_back({"EXPTIME", exposureDuration, 6, "Total Exposure Time (s)"});

    if (targetChip->getFrameType() == CCDChip::DARK_FRAME)
        fitsKeywords.push_back({"DARKTIME", exposureDuration, 6, "Total Dark Exposure Time (s)"});

    // If the camera has a cooler OR if the temperature permission was explicitly set to Read-Only, then record the temperature
    if (HasCooler() || TemperatureNP.p == IP_RO)
        fitsKeywords.push_back({"CCD-TEMP", TemperatureN[0].value, 3, "CCD Temperature (Celsius)"});

    fitsKeywords.push_back({"PIXSIZE1", subPixSize1, 6, "Pixel Size 1 (microns)"});
    fitsKeywords.push_back({"PIXSIZE2", subPixSize2, 6, "Pixel Size 2 (microns)"});
    fitsKeywords.push_back({"XBINNING", targetChip->getBinX(), "Binning factor in width"});
    fitsKeywords.push_back({"YBINNING", targetChip->getBinY(), "Binning factor in height"});
    // XPIXSZ and YPIXSZ are logical sizes including the binning factor
    double xpixsz = subPixSize1 * subBinX;
    double ypixsz = subPixSize2 * subBinY;
    fitsKeywords.push_back({"XPIXSZ", xpixsz, 6, "X binned pixel size in microns"});
    fitsKeywords.push_back({"YPIXSZ", ypixsz, 6, "Y binned pixel size in microns"});

    switch (targetChip->getFrameType())
    {
        case CCDChip::LIGHT_FRAME:
            fitsKeywords.push_back({"FRAME", "Light", "Frame Type"});
            fitsKeywords.push_back({"IMAGETYP", "Light Frame", "Frame Type"});
            break;
        case CCDChip::BIAS_FRAME:
            fitsKeywords.push_back({"FRAME", "Bias", "Frame Type"});
            fitsKeywords.push_back({"IMAGETYP", "Bias Frame", "Frame Type"});
            break;
        case CCDChip::FLAT_FRAME:
            fitsKeywords.push_back({"FRAME", "Flat", "Frame Type"});
            fitsKeywords.push_back({"IMAGETYP", "Flat Frame", "Frame Type"});
            break;
        case CCDChip::DARK_FRAME:
            fitsKeywords.push_back({"FRAME", "Dark", "Frame Type"});
            fitsKeywords.push_back({"IMAGETYP", "Dark Frame", "Frame Type"});
            break;
    }

    if (CurrentFilterSlot != -1 && CurrentFilterSlot <= static_cast<int>(FilterNames.size()))
    {
        fitsKeywords.push_back({"FILTER", FilterNames.at(CurrentFilterSlot - 1).c_str(), "Filter"});
    }

#ifdef WITH_MINMAX
    if (targetChip->getNAxis() == 2)
    {
        double min_val, max_val;
        getMinMax(&min_val, &max_val, targetChip);

        fitsKeywords.push_back({"DATAMIN", min_val, 6, "Minimum value"});
        fitsKeywords.push_back({"DATAMAX", max_val, 6, "Maximum value"});
    }
#endif

    if (HasBayer() && targetChip->getNAxis() == 2)
    {
        fitsKeywords.push_back({"XBAYROFF", atoi(BayerT[0].text), "X offset of Bayer array"});
        fitsKeywords.push_back({"YBAYROFF", atoi(BayerT[1].text), "Y offset of Bayer array"});
        fitsKeywords.push_back({"BAYERPAT", BayerT[2].text, "Bayer color pattern"});
    }

    if (!std::isnan(effectiveFocalLength))
        fitsKeywords.push_back({"FOCALLEN", effectiveFocalLength, 3, "Focal Length (mm)"});

    if (!std::isnan(effectiveAperture))
        fitsKeywords.push_back({"APTDIA", effectiveAperture, 3, "Telescope diameter (mm)"});

    if (!std::isnan(MPSAS))
    {
        fitsKeywords.push_back({"MPSAS", MPSAS, 6, "Sky Quality (mag per arcsec^2)"});
    }

    if (!std::isnan(RotatorAngle))
    {
        fitsKeywords.push_back({"ROTATANG", RotatorAngle, 3, "Rotator angle in degrees"});
    }

    // JJ ed 2020-03-28
    // If the focus position or temperature is set, add the information to the FITS header
    if (FocuserPos != -1)
    {
        fitsKeywords.push_back({"FOCUSPOS", FocuserPos, "Focus position in steps"});
    }
    if (!std::isnan(FocuserTemp))
    {
        fitsKeywords.push_back({"FOCUSTEM", FocuserTemp, 3, "Focuser temperature in degrees C"});
    }

    // SCALE assuming square-pixels
    if (!std::isnan(effectiveFocalLength))
    {
        double pixScale = subPixSize1 / effectiveFocalLength * 206.3 * subBinX;
        fitsKeywords.push_back({"SCALE", pixScale, 6, "arcsecs per pixel"});
    }


    if ( targetChip->getFrameType() == CCDChip::LIGHT_FRAME && !std::isnan(RA) && !std::isnan(Dec) && (std::isnan(J2000RA)
            || std::isnan(J2000DE) || !J2000Valid) )
    {
        INDI::IEquatorialCoordinates epochPos { 0, 0 }, J2000Pos { 0, 0 };
        epochPos.rightascension  = RA;
        epochPos.declination = Dec;

        // Convert from JNow to J2000
        INDI::ObservedToJ2000(&epochPos, ln_get_julian_from_sys(), &J2000Pos);

        J2000RA = J2000Pos.rightascension;
        J2000DE = J2000Pos.declination;
    }
    J2000Valid = false;  // enforce usage of EOD position if we receive no new epoch position

    if ( targetChip->getFrameType() == CCDChip::LIGHT_FRAME && !std::isnan(J2000RA) && !std::isnan(J2000DE) )
    {
        if (!std::isnan(Latitude) && !std::isnan(Longitude))
        {
            INDI::IEquatorialCoordinates epochPos { 0, 0 }, J2000Pos { 0, 0 };

            J2000Pos.rightascension = J2000RA;
            J2000Pos.declination = J2000DE;

            // Convert from JNow to J2000
            INDI::J2000toObserved(&J2000Pos, ln_get_julian_from_sys(), &epochPos);

            // Horizontal Coords
            INDI::IHorizontalCoordinates horizontalPos;
            IGeographicCoordinates observer;
            observer.latitude = Latitude;
            observer.longitude = Longitude;

            EquatorialToHorizontal(&epochPos, &observer, ln_get_julian_from_sys(), &horizontalPos);
            Azimuth = horizontalPos.azimuth;
            Altitude = horizontalPos.altitude;
            Airmass = ln_get_airmass(Altitude, 750);
        }

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
            fitsKeywords.push_back({"SITELAT", Latitude, 6, "Latitude of the imaging site in degrees"});
            fitsKeywords.push_back({"SITELONG", Longitude, 6, "Longitude of the imaging site in degrees"});
        }
        if (!std::isnan(Airmass))
        {
            //fits_update_key_s(fptr, TDOUBLE, "AIRMASS", &Airmass, "Airmass", &status);
            fitsKeywords.push_back({"AIRMASS", Airmass, 6, "Airmass"});
            fitsKeywords.push_back({"OBJCTAZ", Azimuth, 6, "Azimuth of center of image in Degrees"});
            fitsKeywords.push_back({"OBJCTALT", Altitude, 6, "Altitude of center of image in Degrees"});
        }
        fitsKeywords.push_back({"OBJCTRA", ra_str, "Object J2000 RA in Hours"});
        fitsKeywords.push_back({"OBJCTDEC", de_str, "Object J2000 DEC in Degrees"});

        fitsKeywords.push_back({"RA", J2000RA * 15, 6, "Object J2000 RA in Degrees"});
        fitsKeywords.push_back({"DEC", J2000DE, 6, "Object J2000 DEC in Degrees"});

        // pier side
        switch (pierSide)
        {
            case 0:
                fitsKeywords.push_back({"PIERSIDE", "WEST", "West, looking East"});
                break;
            case 1:
                fitsKeywords.push_back({"PIERSIDE", "EAST", "East, looking West"});
                break;
        }

        //fits_update_key_s(fptr, TINT, "EPOCH", &epoch, "Epoch", &status);
        fitsKeywords.push_back({"EQUINOX", 2000, "Equinox"});

        // Add WCS Info
        if (WorldCoordS[0].s == ISS_ON && m_ValidCCDRotation && !std::isnan(effectiveFocalLength))
        {
            double J2000RAHours = J2000RA * 15;
            fitsKeywords.push_back({"CRVAL1", J2000RAHours, 10, "CRVAL1"});
            fitsKeywords.push_back({"CRVAL2", J2000DE, 10, "CRVAL1"});

            char radecsys[8] = "FK5";
            char ctype1[16]  = "RA---TAN";
            char ctype2[16]  = "DEC--TAN";

            fitsKeywords.push_back({"RADECSYS", radecsys, "RADECSYS"});
            fitsKeywords.push_back({"CTYPE1", ctype1, "CTYPE1"});
            fitsKeywords.push_back({"CTYPE2", ctype2, "CTYPE2"});

            double crpix1 = subW / subBinX / 2.0;
            double crpix2 = subH / subBinY / 2.0;

            fitsKeywords.push_back({"CRPIX1", crpix1, 10, "CRPIX1"});
            fitsKeywords.push_back({"CRPIX2", crpix2, 10, "CRPIX2"});

            double secpix1 = subPixSize1 / effectiveFocalLength * 206.3 * subBinX;
            double secpix2 = subPixSize2 / effectiveFocalLength * 206.3 * subBinY;

            fitsKeywords.push_back({"SECPIX1", secpix1, 10, "SECPIX1"});
            fitsKeywords.push_back({"SECPIX2", secpix2, 10, "SECPIX2"});

            double degpix1 = secpix1 / 3600.0;
            double degpix2 = secpix2 / 3600.0;

            fitsKeywords.push_back({"CDELT1", degpix1, 10, "CDELT1"});
            fitsKeywords.push_back({"CDELT2", degpix2, 10, "CDELT2"});

            // Rotation is CW, we need to convert it to CCW per CROTA1 definition
            double rotation = 360 - CCDRotationN[0].value;
            if (rotation > 360)
                rotation -= 360;

            fitsKeywords.push_back({"CROTA1", rotation, 10, "CROTA1"});
            fitsKeywords.push_back({"CROTA2", rotation, 10, "CROTA2"});

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

    fitsKeywords.push_back({"DATE-OBS", exposureStartTime, "UTC start date of observation"});
    fitsKeywords.push_back(FITSRecord("Generated by INDI"));
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
    setCurrentPollingPeriod(getPollingPeriod());

    // Run async
    std::thread(&CCD::ExposureCompletePrivate, this, targetChip).detach();

    return true;
}

bool CCD::ExposureCompletePrivate(CCDChip * targetChip)
{
    LOG_DEBUG("Exposure complete");

    // save information used for the fits header
    exposureDuration = targetChip->getExposureDuration();
    strncpy(exposureStartTime, targetChip->getExposureStartTime(), MAXINDINAME);

    if(HasDSP())
    {
        uint8_t* buf = static_cast<uint8_t*>(malloc(targetChip->getFrameBufferSize()));
        memcpy(buf, targetChip->getFrameBuffer(), targetChip->getFrameBufferSize());
        DSP->processBLOB(buf, 2, new int[2] { targetChip->getXRes() / targetChip->getBinX(), targetChip->getYRes() / targetChip->getBinY() },
                         targetChip->getBPP());
        free(buf);
    }

    if (processFastExposure(targetChip) == false)
        return false;

    bool sendImage = (UploadS[UPLOAD_CLIENT].s == ISS_ON || UploadS[UPLOAD_BOTH].s == ISS_ON);
    bool saveImage = (UploadS[UPLOAD_LOCAL].s == ISS_ON || UploadS[UPLOAD_BOTH].s == ISS_ON);

    // Do not send or save an empty image.
    if (targetChip->getFrameBufferSize() == 0)
        sendImage = saveImage = false;

    if (sendImage || saveImage)
    {
        if (EncodeFormatSP[FORMAT_FITS].getState() == ISS_ON)
        {
            targetChip->setImageExtension("fits");

            int img_type  = 0;
            int byte_type = 0;
            int status    = 0;
            long naxis    = targetChip->getNAxis();
            long naxes[3];
            int nelements = 0;
            char error_status[MAXRBUF];

            naxes[0] = targetChip->getSubW() / targetChip->getBinX();
            naxes[1] = targetChip->getSubH() / targetChip->getBinY();

            switch (targetChip->getBPP())
            {
                case 8:
                    byte_type = TBYTE;
                    img_type  = BYTE_IMG;
                    break;

                case 16:
                    byte_type = TUSHORT;
                    img_type  = USHORT_IMG;
                    break;

                case 32:
                    byte_type = TULONG;
                    img_type  = ULONG_IMG;
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

            // 8640 = 2880 * 3 which is sufficient for most cases.
            uint32_t size = 8640 + nelements * (targetChip->getBPP() / 8);
            //  Initialize FITS file.
            if (targetChip->openFITSFile(size, status) == false)
            {
                fits_report_error(stderr, status); /* print out any error messages */
                fits_get_errstatus(status, error_status);
                LOGF_ERROR("FITS Error: %s", error_status);
                return false;
            }

            auto fptr = *targetChip->fitsFilePointer();

            fits_create_img(fptr, img_type, naxis, naxes, &status);

            if (status)
            {
                fits_report_error(stderr, status); /* print out any error messages */
                fits_get_errstatus(status, error_status);
                LOGF_ERROR("FITS Error: %s", error_status);
                targetChip->closeFITSFile();
                return false;
            }

            std::vector<FITSRecord> fitsKeywords;

            addFITSKeywords(targetChip, fitsKeywords);

            // Add all custom keywords next
            for (auto &record : m_CustomFITSKeywords)
                fitsKeywords.push_back(record.second);

            for (auto &keyword : fitsKeywords)
            {
                int key_status = 0;
                switch(keyword.type())
                {
                    case INDI::FITSRecord::VOID:
                        break;
                    case INDI::FITSRecord::COMMENT:
                        fits_write_comment(fptr, keyword.comment().c_str(), &key_status);
                        break;
                    case INDI::FITSRecord::STRING:
                        fits_update_key_str(fptr, keyword.key().c_str(), keyword.valueString().c_str(), keyword.comment().c_str(), &key_status);
                        break;
                    case INDI::FITSRecord::LONGLONG:
                        fits_update_key_lng(fptr, keyword.key().c_str(), keyword.valueInt(), keyword.comment().c_str(), &key_status);
                        break;
                    case INDI::FITSRecord::DOUBLE:
                        fits_update_key_dbl(fptr, keyword.key().c_str(), keyword.valueDouble(), keyword.decimal(), keyword.comment().c_str(),
                                            &key_status);
                        break;
                }
                if (key_status)
                {
                    fits_get_errstatus(key_status, error_status);
                    LOGF_ERROR("FITS key %s Error: %s", keyword.key().c_str(), error_status);
                }
            }

            fits_write_img(fptr, byte_type, 1, nelements, targetChip->getFrameBuffer(), &status);
            targetChip->finishFITSFile(status);
            if (status)
            {
                fits_report_error(stderr, status); /* print out any error messages */
                fits_get_errstatus(status, error_status);
                LOGF_ERROR("FITS Error: %s", error_status);
                targetChip->closeFITSFile();
                return false;
            }


            bool rc = uploadFile(targetChip, *(targetChip->fitsMemoryBlockPointer()), *(targetChip->fitsMemorySizePointer()), sendImage,
                                 saveImage);

            targetChip->closeFITSFile();

            guard.unlock();

            if (rc == false)
            {
                targetChip->setExposureFailed();
                return false;
            }
        }
#ifdef HAVE_XISF
        else if (EncodeFormatSP[FORMAT_XISF].getState() == ISS_ON)
        {
            std::vector<FITSRecord> fitsKeywords;
            addFITSKeywords(targetChip, fitsKeywords);
            targetChip->setImageExtension("xisf");

            try
            {
                AutoCNumeric locale;
                LibXISF::Image image;
                LibXISF::XISFWriter xisfWriter;

                for (auto &keyword : fitsKeywords)
                {
                    image.addFITSKeyword({keyword.key().c_str(), keyword.valueString().c_str(), keyword.comment().c_str()});
                    image.addFITSKeywordAsProperty(keyword.key().c_str(), keyword.valueString());
                }

                image.setGeometry(targetChip->getSubW() / targetChip->getBinX(),
                                  targetChip->getSubH() / targetChip->getBinY(),
                                  targetChip->getNAxis() == 2 ? 1 : 3);
                switch(targetChip->getBPP())
                {
                    case 8:
                        image.setSampleFormat(LibXISF::Image::UInt8);
                        break;
                    case 16:
                        image.setSampleFormat(LibXISF::Image::UInt16);
                        break;
                    case 32:
                        image.setSampleFormat(LibXISF::Image::UInt32);
                        break;
                    default:
                        LOGF_ERROR("Unsupported bits per pixel value %d", targetChip->getBPP());
                        return false;
                }

                switch(targetChip->getFrameType())
                {
                    case CCDChip::LIGHT_FRAME:
                        image.setImageType(LibXISF::Image::Light);
                        break;
                    case CCDChip::BIAS_FRAME:
                        image.setImageType(LibXISF::Image::Bias);
                        break;
                    case CCDChip::DARK_FRAME:
                        image.setImageType(LibXISF::Image::Dark);
                        break;
                    case CCDChip::FLAT_FRAME:
                        image.setImageType(LibXISF::Image::Flat);
                        break;
                }

                if (targetChip->SendCompressed)
                {
                    image.setCompression(LibXISF::DataBlock::LZ4);
                    image.setByteshuffling(targetChip->getBPP() / 8);
                }

                if (HasBayer())
                    image.setColorFilterArray({2, 2, BayerT[2].text});

                if (targetChip->getNAxis() == 3)
                {
                    image.setColorSpace(LibXISF::Image::RGB);
                }

                std::unique_lock<std::mutex> guard(ccdBufferLock);
                std::memcpy(image.imageData(), targetChip->getFrameBuffer(), image.imageDataSize());
                xisfWriter.writeImage(image);

                LibXISF::ByteArray xisfFile;
                xisfWriter.save(xisfFile);
                bool rc = uploadFile(targetChip, xisfFile.data(), xisfFile.size(), sendImage, saveImage);
                if (rc == false)
                {
                    targetChip->setExposureFailed();
                    return false;
                }
            }
            catch (LibXISF::Error &error)
            {
                LOGF_ERROR("XISF Error: %s", error.what());
                return false;
            }
        }
#endif
        else
        {
            // If image extension was set to fits (default), change if bin if not already set to another format by the driver.
            if (!strcmp(targetChip->getImageExtension(), "fits"))
                targetChip->setImageExtension("bin");
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

    if (FastExposureToggleS[INDI_ENABLED].s != ISS_ON)
        targetChip->setExposureComplete();

    UploadComplete(targetChip);
    return true;
}

bool CCD::uploadFile(CCDChip * targetChip, const void * fitsData, size_t totalBytes, bool sendImage,
                     bool saveImage)
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
            auto now = std::chrono::system_clock::now();
            std::time_t time = std::chrono::system_clock::to_time_t(now);
            std::tm* now_tm = std::localtime(&time);
            long long timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();

            std::stringstream stream;
            stream    << std::setfill('0')
                      << std::put_time(now_tm, "%FT%H:%M:")
                      << std::setw(2) << (timestamp / 1000) % 60 << '.'
                      << std::setw(3) << timestamp % 1000;

            prefix = std::regex_replace(prefix, std::regex("ISO8601"), stream.str());

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
        for (int nr = 0; nr < targetChip->FitsB.bloblen; nr += n)
            n = fwrite((static_cast<char *>(targetChip->FitsB.blob) + nr), 1, targetChip->FitsB.bloblen - nr, fp);

        fclose(fp);

        // Save image file path
        IUSaveText(&FileNameT[0], imageFileName);

        DEBUGF(Logger::DBG_SESSION, "Image saved to %s", imageFileName);
        FileNameTP.s = IPS_OK;
        IDSetText(&FileNameTP, nullptr);
    }

    if (targetChip->SendCompressed && EncodeFormatSP[FORMAT_XISF].getState() != ISS_ON)
    {
        if (EncodeFormatSP[FORMAT_FITS].getState() == ISS_ON && !strcmp(targetChip->getImageExtension(), "fits"))
        {
            fpstate	fpvar;
            fp_init (&fpvar);
            size_t compressedBytes = 0;
            int islossless = 0;
            if (fp_pack_data_to_data(reinterpret_cast<const char *>(fitsData), totalBytes, &compressedData, &compressedBytes, fpvar,
                                     &islossless) < 0)
            {
                free(compressedData);
                LOG_ERROR("Error: Ran out of memory compressing image");
                return false;
            }

            targetChip->FitsB.blob    = compressedData;
            targetChip->FitsB.bloblen = compressedBytes;
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

bool CCD::processFastExposure(CCDChip * targetChip)
{
    // If fast exposure is on, let's immediately take another capture
    if (FastExposureToggleS[INDI_ENABLED].s == ISS_ON)
    {
        targetChip->setExposureComplete();
        double duration = targetChip->getExposureDuration();

        // Check fast exposure count
        if (FastExposureCountN[0].value > 1)
        {
            if (UploadS[UPLOAD_LOCAL].s != ISS_ON)
            {
                if (FastExposureCountNP.s != IPS_BUSY)
                {
                    FastExposureToggleStartup = std::chrono::system_clock::now();
                }
                else
                {
                    auto end = std::chrono::system_clock::now();

                    m_UploadTime = (std::chrono::duration_cast<std::chrono::milliseconds>(end - FastExposureToggleStartup)).count() / 1000.0 -
                                   duration;
                    LOGF_DEBUG("Image download and upload/save took %.3f seconds.", m_UploadTime);

                    FastExposureToggleStartup = end;
                }
            }

            FastExposureCountNP.s = IPS_BUSY;
            FastExposureCountN[0].value--;
            IDSetNumber(&FastExposureCountNP, nullptr);

            if (UploadS[UPLOAD_LOCAL].s == ISS_ON || m_UploadTime < duration)
            {
                if (StartExposure(duration))
                    PrimaryCCD.ImageExposureNP.s = IPS_BUSY;
                else
                    PrimaryCCD.ImageExposureNP.s = IPS_ALERT;
                if (duration * 1000 < getCurrentPollingPeriod())
                    setCurrentPollingPeriod(duration * 950);
            }
            else
            {
                LOGF_ERROR("Rapid exposure not possible since upload time is %.2f seconds while exposure time is %.2f seconds.",
                           m_UploadTime,
                           duration);
                PrimaryCCD.ImageExposureNP.s = IPS_ALERT;
                IDSetNumber(&PrimaryCCD.ImageExposureNP, nullptr);
                FastExposureCountN[0].value = 1;
                FastExposureCountNP.s = IPS_IDLE;
                IDSetNumber(&FastExposureCountNP, nullptr);
                m_UploadTime = 0;
                return false;
            }
        }
        else
        {
            m_UploadTime = 0;
            FastExposureCountNP.s = IPS_IDLE;
            IDSetNumber(&FastExposureCountNP, nullptr);
        }
    }

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
    IUSaveConfigSwitch(fp, &FastExposureToggleSP);

    IUSaveConfigSwitch(fp, &PrimaryCCD.CompressSP);

    if (PrimaryCCD.getCCDInfo()->p != IP_RO)
        IUSaveConfigNumber(fp, PrimaryCCD.getCCDInfo());

    CaptureFormatSP.save(fp);
    EncodeFormatSP.save(fp);

    if (HasCooler())
        TemperatureRampNP.save(fp);

    if (HasGuideHead())
    {
        IUSaveConfigSwitch(fp, &GuideCCD.CompressSP);
        IUSaveConfigNumber(fp, &GuideCCD.ImageBinNP);
    }

    if (CanSubFrame() && PrimaryCCD.ImageFrameN[2].value > 0)
        IUSaveConfigNumber(fp, &PrimaryCCD.ImageFrameNP);

    if (CanBin())
        IUSaveConfigNumber(fp, &PrimaryCCD.ImageBinNP);

    if (HasBayer())
        IUSaveConfigText(fp, &BayerTP);

    if (HasStreaming())
        Streamer->saveConfigItems(fp);

    if (HasDSP())
        DSP->saveConfigItems(fp);

    ScopeInfoNP.save(fp);

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
            if (INDI::mkpath(dir, 0755) == -1)
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

    for (uint32_t i = 0; i < files.size(); i++)
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

/////////////////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////////////////
void CCD::checkTemperatureTarget()
{
    if (TemperatureNP.s == IPS_BUSY)
    {
        if (std::abs(m_TargetTemperature - TemperatureN[0].value) <= TemperatureRampNP[RAMP_THRESHOLD].getValue())
        {
            TemperatureNP.s = IPS_OK;
            m_TemperatureCheckTimer.stop();
            IDSetNumber(&TemperatureNP, nullptr);
        }
        // If we are beyond a minute, check for next step
        else if (TemperatureRampNP[RAMP_SLOPE].getValue() > 0 && m_TemperatureElapsedTimer.elapsed() >= 60000)
        {
            double nextTemperature = 0;
            // Going down
            if (m_TargetTemperature < TemperatureN[0].value)
            {
                nextTemperature = std::max(m_TargetTemperature, TemperatureN[0].value - TemperatureRampNP[RAMP_SLOPE].getValue());
            }
            // Going up
            else
            {
                nextTemperature = std::min(m_TargetTemperature, TemperatureN[0].value + TemperatureRampNP[RAMP_SLOPE].getValue());
            }

            m_TemperatureElapsedTimer.restart();
            SetTemperature(nextTemperature);
        }
    }
}

void CCD::addCaptureFormat(const CaptureFormat &format)
{
    // Avoid duplicates.
    auto pos = std::find_if(m_CaptureFormats.begin(), m_CaptureFormats.end(), [format](auto & oneFormat)
    {
        return format.name == oneFormat.name;
    });
    if (pos != m_CaptureFormats.end())
        return;

    // Add NEW format.
    auto count = CaptureFormatSP.size();
    CaptureFormatSP.resize(count + 1);
    // Format is ON if the label matches the configuration label OR if there is no configuration saved and isDefault is true.
    const bool isOn = (format.name == m_ConfigCaptureFormatName) || (m_ConfigCaptureFormatName.empty() && format.isDefault);
    CaptureFormatSP[count].fill(format.name.c_str(), format.label.c_str(), isOn ? ISS_ON : ISS_OFF);
    m_CaptureFormats.push_back(format);
}

bool CCD::SetCaptureFormat(uint8_t index)
{
    INDI_UNUSED(index);
    return true;
}

}
