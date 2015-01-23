/*******************************************************************************
 Copyright(c) 2010, 2011 Gerry Rozema, Jasem Mutlaq. All rights reserved.

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

#include "indiccd.h"

#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <zlib.h>
#include <errno.h>
#include <dirent.h>

#include <fitsio.h>

const char *IMAGE_SETTINGS_TAB  = "Image Settings";
const char *IMAGE_INFO_TAB      = "Image Info";
const char *GUIDE_HEAD_TAB      = "Guider Head";
const char *GUIDE_CONTROL_TAB   = "Guider Control";
const char *RAPIDGUIDE_TAB      = "Rapid Guide";

CCDChip::CCDChip()
{
    SendCompressed=false;
    Interlaced=false;

    RawFrame= (char *) malloc(sizeof(char)); // Seed for realloc
    RawFrameSize=0;

    BPP = 8;
    BinX = BinY = 1;
    NAxis = 2;

    strncpy(imageExtention, "fits", MAXINDIBLOBFMT);

    FrameType=LIGHT_FRAME;
    lastRapidX = lastRapidY = -1;
}

CCDChip::~CCDChip()
{
    delete RawFrame;
    RawFrameSize=0;
    RawFrame=NULL;

}

void CCDChip::setFrameType(CCD_FRAME type)
{
    FrameType=type;
}

void CCDChip::setResolution(int x, int y)
{
    XRes = x;
    YRes = y;

    ImagePixelSizeN[0].value=x;
    ImagePixelSizeN[1].value=y;

    IDSetNumber(&ImagePixelSizeNP, NULL);

    ImageFrameN[FRAME_X].min = 0;
    ImageFrameN[FRAME_X].max = x-1;
    ImageFrameN[FRAME_Y].min = 0;
    ImageFrameN[FRAME_Y].max = y-1;

    ImageFrameN[FRAME_W].min = 1;
    ImageFrameN[FRAME_W].max = x;
    ImageFrameN[FRAME_H].max = 1;
    ImageFrameN[FRAME_H].max = y;
    IUUpdateMinMax(&ImageFrameNP);
}

void CCDChip::setFrame(int subx, int suby, int subw, int subh)
{
    SubX = subx;
    SubY = suby;
    SubW = subw;
    SubH = subh;

    ImageFrameN[FRAME_X].value = SubX;
    ImageFrameN[FRAME_Y].value = SubY;
    ImageFrameN[FRAME_W].value = SubW;
    ImageFrameN[FRAME_H].value = SubH;

    IDSetNumber(&ImageFrameNP, NULL);
}

void CCDChip::setBin(int hor, int ver)
{
    BinX = hor;
    BinY = ver;

    ImageBinN[BIN_W].value = BinX;
    ImageBinN[BIN_H].value = BinY;

    IDSetNumber(&ImageBinNP, NULL);
}


void CCDChip::setMinMaxStep(const char *property, const char *element, double min, double max, double step, bool sendToClient)
{
    INumberVectorProperty *nvp = NULL;

    if (!strcmp(property, ImageExposureNP.name))
        nvp = &ImageExposureNP;
    else if (!strcmp(property, ImageFrameNP.name))
        nvp = &ImageFrameNP;
    else if (!strcmp(property, ImageBinNP.name))
        nvp = &ImageBinNP;
    else if (!strcmp(property, ImagePixelSizeNP.name))
        nvp = &ImagePixelSizeNP;
    else if (!strcmp(property, RapidGuideDataNP.name))
        nvp = &RapidGuideDataNP;

    INumber *np = IUFindNumber(nvp, element);
    if (np)
    {
        np->min  = min;
        np->max  = max;
        np->step = step;

        if (sendToClient)
            IUUpdateMinMax(nvp);
    }

}

void CCDChip::setPixelSize(float x, float y)
{
    PixelSizex = x;
    PixelSizey = y;

    ImagePixelSizeN[2].value=x;
    ImagePixelSizeN[3].value=x;
    ImagePixelSizeN[4].value=y;

    IDSetNumber(&ImagePixelSizeNP, NULL);

}

void CCDChip::setBPP(int bbp)
{
    BPP = bbp;

    ImagePixelSizeN[5].value = BPP;

    IDSetNumber(&ImagePixelSizeNP, NULL);
}

void CCDChip::setFrameBufferSize(int nbuf, bool allocMem)
{
    if (nbuf == RawFrameSize)
        return;

    RawFrameSize = nbuf;

    if (allocMem == false)
        return;

    RawFrame = (char *) realloc(RawFrame, nbuf * sizeof(char));
}

void CCDChip::setExposureLeft(double duration)
{
    ImageExposureN[0].value = duration;

    IDSetNumber(&ImageExposureNP, NULL);
}

void CCDChip::setExposureDuration(double duration)
{
    exposureDuration = duration;
    gettimeofday(&startExposureTime,NULL);
}

const char *CCDChip::getFrameTypeName(CCD_FRAME fType)
{
    return FrameTypeS[fType].name;
}

const char * CCDChip::getExposureStartTime()
{
    static char ts[32];
    struct tm *tp;
    time_t t = (time_t) startExposureTime.tv_sec;

    //time (&t);
    tp = gmtime (&t);
    strftime (ts, sizeof(ts), "%Y-%m-%dT%H:%M:%S", tp);
    return (ts);
}

void CCDChip::setInterlaced(bool intr)
{
    Interlaced = intr;
}

void CCDChip::setExposureFailed()
{
    ImageExposureNP.s = IPS_ALERT;
    IDSetNumber(&ImageExposureNP, NULL);
}

int CCDChip::getNAxis() const
{
    return NAxis;
}

void CCDChip::setNAxis(int value)
{
    NAxis = value;
}

void CCDChip::setImageExtension(const char *ext)
{
    strncpy(imageExtention, ext, MAXINDINAME);
}

INDI::CCD::CCD()
{
    //ctor
    capability.hasGuideHead=false;
    capability.hasST4Port=false;
    capability.hasShutter=false;
    capability.hasCooler=false;
    capability.canBin=false;
    capability.canSubFrame=false;
    capability.canAbort=false;

    InExposure=false;
    InGuideExposure=false;
    RapidGuideEnabled=false;
    GuiderRapidGuideEnabled=false;

    AutoLoop=false;
    SendImage=false;
    ShowMarker=false;
    GuiderAutoLoop=false;
    GuiderSendImage=false;
    GuiderShowMarker=false;

    ExposureTime = 0.0;
    GuiderExposureTime = 0.0;
    CurrentFilterSlot=-1;

    RA=-1000;
    Dec=-1000;
}

INDI::CCD::~CCD()
{

}

void INDI::CCD::SetCCDCapability(CCDCapability *cap)
{
    capability.canAbort     = cap->canAbort;
    capability.canBin       = cap->canBin;
    capability.canSubFrame  = cap->canSubFrame;
    capability.hasCooler    = cap->hasCooler;
    capability.hasGuideHead = cap->hasGuideHead;
    capability.hasShutter   = cap->hasShutter;
    capability.hasST4Port   = cap->hasST4Port;
}

bool INDI::CCD::initProperties()
{
    DefaultDevice::initProperties();   //  let the base class flesh in what it wants

    // CCD Temperature
    IUFillNumber(&TemperatureN[0], "CCD_TEMPERATURE_VALUE", "Temperature (C)", "%5.2f", -50.0, 50.0, 0., 0.);
    IUFillNumberVector(&TemperatureNP, TemperatureN, 1, getDeviceName(), "CCD_TEMPERATURE", "Temperature", MAIN_CONTROL_TAB, IP_RW, 60, IPS_IDLE);

    // PRIMARY CCD Init

    IUFillNumber(&PrimaryCCD.ImageFrameN[0],"X","Left ","%4.0f",0,1392.0,0,0);
    IUFillNumber(&PrimaryCCD.ImageFrameN[1],"Y","Top","%4.0f",0,1040,0,0);
    IUFillNumber(&PrimaryCCD.ImageFrameN[2],"WIDTH","Width","%4.0f",0,1392.0,0,1392.0);
    IUFillNumber(&PrimaryCCD.ImageFrameN[3],"HEIGHT","Height","%4.0f",0,1392,0,1392.0);
    IUFillNumberVector(&PrimaryCCD.ImageFrameNP,PrimaryCCD.ImageFrameN,4,getDeviceName(),"CCD_FRAME","Frame",IMAGE_SETTINGS_TAB,IP_RW,60,IPS_IDLE);

    IUFillSwitch(&PrimaryCCD.FrameTypeS[0],"FRAME_LIGHT","Light",ISS_ON);
    IUFillSwitch(&PrimaryCCD.FrameTypeS[1],"FRAME_BIAS","Bias",ISS_OFF);
    IUFillSwitch(&PrimaryCCD.FrameTypeS[2],"FRAME_DARK","Dark",ISS_OFF);
    IUFillSwitch(&PrimaryCCD.FrameTypeS[3],"FRAME_FLAT","Flat",ISS_OFF);
    IUFillSwitchVector(&PrimaryCCD.FrameTypeSP,PrimaryCCD.FrameTypeS,4,getDeviceName(),"CCD_FRAME_TYPE","Frame Type",IMAGE_SETTINGS_TAB,IP_RW,ISR_1OFMANY,60,IPS_IDLE);

    IUFillNumber(&PrimaryCCD.ImageExposureN[0],"CCD_EXPOSURE_VALUE","Duration (s)","%5.2f",0.01,3600,1.0,1.0);
    IUFillNumberVector(&PrimaryCCD.ImageExposureNP,PrimaryCCD.ImageExposureN,1,getDeviceName(),"CCD_EXPOSURE","Expose",MAIN_CONTROL_TAB,IP_RW,60,IPS_IDLE);

    IUFillSwitch(&PrimaryCCD.AbortExposureS[0],"ABORT","Abort",ISS_OFF);
    IUFillSwitchVector(&PrimaryCCD.AbortExposureSP,PrimaryCCD.AbortExposureS,1,getDeviceName(),"CCD_ABORT_EXPOSURE","Expose Abort",MAIN_CONTROL_TAB,IP_RW,ISR_ATMOST1,60,IPS_IDLE);

    IUFillNumber(&PrimaryCCD.ImageBinN[0],"HOR_BIN","X","%2.0f",1,4,1,1);
    IUFillNumber(&PrimaryCCD.ImageBinN[1],"VER_BIN","Y","%2.0f",1,4,1,1);
    IUFillNumberVector(&PrimaryCCD.ImageBinNP,PrimaryCCD.ImageBinN,2,getDeviceName(),"CCD_BINNING","Binning",IMAGE_SETTINGS_TAB,IP_RW,60,IPS_IDLE);

    IUFillNumber(&PrimaryCCD.ImagePixelSizeN[0],"CCD_MAX_X","Resolution x","%4.0f",1,16000,0,1392.0);
    IUFillNumber(&PrimaryCCD.ImagePixelSizeN[1],"CCD_MAX_Y","Resolution y","%4.0f",1,16000,0,1392.0);
    IUFillNumber(&PrimaryCCD.ImagePixelSizeN[2],"CCD_PIXEL_SIZE","Pixel size (um)","%5.2f",1,40,0,6.45);
    IUFillNumber(&PrimaryCCD.ImagePixelSizeN[3],"CCD_PIXEL_SIZE_X","Pixel size X","%5.2f",1,40,0,6.45);
    IUFillNumber(&PrimaryCCD.ImagePixelSizeN[4],"CCD_PIXEL_SIZE_Y","Pixel size Y","%5.2f",1,40,0,6.45);
    IUFillNumber(&PrimaryCCD.ImagePixelSizeN[5],"CCD_BITSPERPIXEL","Bits per pixel","%3.0f",8,64,0,8);
    IUFillNumberVector(&PrimaryCCD.ImagePixelSizeNP,PrimaryCCD.ImagePixelSizeN,6,getDeviceName(),"CCD_INFO","CCD Information",IMAGE_INFO_TAB,IP_RO,60,IPS_IDLE);

    IUFillSwitch(&PrimaryCCD.CompressS[0],"CCD_COMPRESS","Compress",ISS_OFF);
    IUFillSwitch(&PrimaryCCD.CompressS[1],"CCD_RAW","Raw",ISS_ON);
    IUFillSwitchVector(&PrimaryCCD.CompressSP,PrimaryCCD.CompressS,2,getDeviceName(),"CCD_COMPRESSION","Image",IMAGE_SETTINGS_TAB,IP_RW,ISR_1OFMANY,60,IPS_IDLE);
    PrimaryCCD.SendCompressed = false;

    IUFillBLOB(&PrimaryCCD.FitsB,"CCD1","Image","");
    IUFillBLOBVector(&PrimaryCCD.FitsBP,&PrimaryCCD.FitsB,1,getDeviceName(),"CCD1","Image Data",IMAGE_INFO_TAB,IP_RO,60,IPS_IDLE);

    IUFillSwitch(&PrimaryCCD.RapidGuideS[0], "ENABLE", "Enable", ISS_OFF);
    IUFillSwitch(&PrimaryCCD.RapidGuideS[1], "DISABLE", "Disable", ISS_ON);
    IUFillSwitchVector(&PrimaryCCD.RapidGuideSP, PrimaryCCD.RapidGuideS, 2, getDeviceName(), "CCD_RAPID_GUIDE", "Rapid Guide", OPTIONS_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    IUFillSwitch(&PrimaryCCD.RapidGuideSetupS[0], "AUTO_LOOP", "Auto loop", ISS_ON);
    IUFillSwitch(&PrimaryCCD.RapidGuideSetupS[1], "SEND_IMAGE", "Send image", ISS_OFF);
    IUFillSwitch(&PrimaryCCD.RapidGuideSetupS[2], "SHOW_MARKER", "Show marker", ISS_OFF);
    IUFillSwitchVector(&PrimaryCCD.RapidGuideSetupSP, PrimaryCCD.RapidGuideSetupS, 3, getDeviceName(), "CCD_RAPID_GUIDE_SETUP", "Rapid Guide Setup", RAPIDGUIDE_TAB, IP_RW, ISR_NOFMANY, 0, IPS_IDLE);

    IUFillNumber(&PrimaryCCD.RapidGuideDataN[0],"GUIDESTAR_X","Guide star position X","%5.2f",0,1024,0,0);
    IUFillNumber(&PrimaryCCD.RapidGuideDataN[1],"GUIDESTAR_Y","Guide star position Y","%5.2f",0,1024,0,0);
    IUFillNumber(&PrimaryCCD.RapidGuideDataN[2],"GUIDESTAR_FIT","Guide star fit","%5.2f",0,1024,0,0);
    IUFillNumberVector(&PrimaryCCD.RapidGuideDataNP,PrimaryCCD.RapidGuideDataN,3,getDeviceName(),"CCD_RAPID_GUIDE_DATA","Rapid Guide Data",RAPIDGUIDE_TAB,IP_RO,60,IPS_IDLE);

    // Reset Frame Settings
    IUFillSwitch(&PrimaryCCD.ResetS[0], "RESET", "Reset", ISS_OFF);
    IUFillSwitchVector(&PrimaryCCD.ResetSP, PrimaryCCD.ResetS, 1, getDeviceName(), "CCD_FRAME_RESET", "Frame Values", IMAGE_SETTINGS_TAB, IP_WO, ISR_1OFMANY, 0, IPS_IDLE);

    // GUIDER CCD Init

    IUFillNumber(&GuideCCD.ImageFrameN[0],"X","Left ","%4.0f",0,1392.0,0,0);
    IUFillNumber(&GuideCCD.ImageFrameN[1],"Y","Top","%4.0f",0,1040,0,0);
    IUFillNumber(&GuideCCD.ImageFrameN[2],"WIDTH","Width","%4.0f",0,1392.0,0,1392.0);
    IUFillNumber(&GuideCCD.ImageFrameN[3],"HEIGHT","Height","%4.0f",0,1040,0,1040);
    IUFillNumberVector(&GuideCCD.ImageFrameNP,GuideCCD.ImageFrameN,4,getDeviceName(),"GUIDER_FRAME","Frame",GUIDE_HEAD_TAB,IP_RW,60,IPS_IDLE);

    IUFillNumber(&GuideCCD.ImageBinN[0],"HOR_BIN","X","%2.0f",1,4,1,1);
    IUFillNumber(&GuideCCD.ImageBinN[1],"VER_BIN","Y","%2.0f",1,4,1,1);
    IUFillNumberVector(&GuideCCD.ImageBinNP,GuideCCD.ImageBinN,2,getDeviceName(),"GUIDER_BINNING","Binning",GUIDE_HEAD_TAB,IP_RW,60,IPS_IDLE);

    IUFillNumber(&GuideCCD.ImagePixelSizeN[0],"CCD_MAX_X","Resolution x","%4.0f",1,16000,0,1392.0);
    IUFillNumber(&GuideCCD.ImagePixelSizeN[1],"CCD_MAX_Y","Resolution y","%4.0f",1,16000,0,1392.0);
    IUFillNumber(&GuideCCD.ImagePixelSizeN[2],"CCD_PIXEL_SIZE","Pixel size (um)","%5.2f",1,40,0,6.45);
    IUFillNumber(&GuideCCD.ImagePixelSizeN[3],"CCD_PIXEL_SIZE_X","Pixel size X","%5.2f",1,40,0,6.45);
    IUFillNumber(&GuideCCD.ImagePixelSizeN[4],"CCD_PIXEL_SIZE_Y","Pixel size Y","%5.2f",1,40,0,6.45);
    IUFillNumber(&GuideCCD.ImagePixelSizeN[5],"CCD_BITSPERPIXEL","Bits per pixel","%3.0f",8,64,0,8);
    IUFillNumberVector(&GuideCCD.ImagePixelSizeNP,GuideCCD.ImagePixelSizeN,6,getDeviceName(),"GUIDER_INFO", "Guide Info",IMAGE_INFO_TAB,IP_RO,60,IPS_IDLE);

    IUFillSwitch(&GuideCCD.FrameTypeS[0],"FRAME_LIGHT","Light",ISS_ON);
    IUFillSwitch(&GuideCCD.FrameTypeS[1],"FRAME_BIAS","Bias",ISS_OFF);
    IUFillSwitch(&GuideCCD.FrameTypeS[2],"FRAME_DARK","Dark",ISS_OFF);
    IUFillSwitch(&GuideCCD.FrameTypeS[3],"FRAME_FLAT","Flat",ISS_OFF);
    IUFillSwitchVector(&GuideCCD.FrameTypeSP,GuideCCD.FrameTypeS,4,getDeviceName(),"GUIDER_FRAME_TYPE","Frame Type",GUIDE_HEAD_TAB,IP_RW,ISR_1OFMANY,60,IPS_IDLE);

    IUFillNumber(&GuideCCD.ImageExposureN[0],"GUIDER_EXPOSURE_VALUE","Duration (s)","%5.2f",0.01,3600,1.0,1.0);
    IUFillNumberVector(&GuideCCD.ImageExposureNP,GuideCCD.ImageExposureN,1,getDeviceName(),"GUIDER_EXPOSURE","Guide Head",MAIN_CONTROL_TAB,IP_RW,60,IPS_IDLE);

    IUFillSwitch(&GuideCCD.AbortExposureS[0],"ABORT","Abort",ISS_OFF);
    IUFillSwitchVector(&GuideCCD.AbortExposureSP,GuideCCD.AbortExposureS,1,getDeviceName(),"GUIDER_ABORT_EXPOSURE","Guide Abort",MAIN_CONTROL_TAB,IP_RW,ISR_ATMOST1,60,IPS_IDLE);

    IUFillSwitch(&GuideCCD.CompressS[0],"GUIDER_COMPRESS","Compress",ISS_OFF);
    IUFillSwitch(&GuideCCD.CompressS[1],"GUIDER_RAW","Raw",ISS_ON);
    IUFillSwitchVector(&GuideCCD.CompressSP,GuideCCD.CompressS,2,getDeviceName(),"GUIDER_COMPRESSION","Image",GUIDE_HEAD_TAB,IP_RW,ISR_1OFMANY,60,IPS_IDLE);
    GuideCCD.SendCompressed = false;

    IUFillBLOB(&GuideCCD.FitsB,"CCD2","Guider Image","");
    IUFillBLOBVector(&GuideCCD.FitsBP,&GuideCCD.FitsB,1,getDeviceName(),"CCD2","Image Data",IMAGE_INFO_TAB,IP_RO,60,IPS_IDLE);

    IUFillSwitch(&GuideCCD.RapidGuideS[0], "ENABLE", "Enable", ISS_OFF);
    IUFillSwitch(&GuideCCD.RapidGuideS[1], "DISABLE", "Disable", ISS_ON);
    IUFillSwitchVector(&GuideCCD.RapidGuideSP, GuideCCD.RapidGuideS, 2, getDeviceName(), "GUIDER_RAPID_GUIDE", "Guider Head Rapid Guide", OPTIONS_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    IUFillSwitch(&GuideCCD.RapidGuideSetupS[0], "AUTO_LOOP", "Auto loop", ISS_ON);
    IUFillSwitch(&GuideCCD.RapidGuideSetupS[1], "SEND_IMAGE", "Send image", ISS_OFF);
    IUFillSwitch(&GuideCCD.RapidGuideSetupS[2], "SHOW_MARKER", "Show marker", ISS_OFF);
    IUFillSwitchVector(&GuideCCD.RapidGuideSetupSP, GuideCCD.RapidGuideSetupS, 3, getDeviceName(), "GUIDER_RAPID_GUIDE_SETUP", "Rapid Guide Setup", RAPIDGUIDE_TAB, IP_RW, ISR_NOFMANY, 0, IPS_IDLE);

    IUFillNumber(&GuideCCD.RapidGuideDataN[0],"GUIDESTAR_X","Guide star position X","%5.2f",0,1024,0,0);
    IUFillNumber(&GuideCCD.RapidGuideDataN[1],"GUIDESTAR_Y","Guide star position Y","%5.2f",0,1024,0,0);
    IUFillNumber(&GuideCCD.RapidGuideDataN[2],"GUIDESTAR_FIT","Guide star fit","%5.2f",0,1024,0,0);
    IUFillNumberVector(&GuideCCD.RapidGuideDataNP,GuideCCD.RapidGuideDataN,3,getDeviceName(),"GUIDER_RAPID_GUIDE_DATA","Rapid Guide Data",RAPIDGUIDE_TAB,IP_RO,60,IPS_IDLE);

    // CCD Class Init

    IUFillSwitch(&UploadS[0], "UPLOAD_CLIENT", "Client", ISS_ON);
    IUFillSwitch(&UploadS[1], "UPLOAD_LOCAL", "Local", ISS_OFF);
    IUFillSwitch(&UploadS[2], "UPLOAD_BOTH", "Both", ISS_OFF);
    IUFillSwitchVector(&UploadSP, UploadS, 3, getDeviceName(), "UPLOAD_MODE", "Upload", OPTIONS_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    IUFillText(&UploadSettingsT[0],"UPLOAD_DIR","Dir","");
    IUFillText(&UploadSettingsT[1],"UPLOAD_PREFIX","Prefix","IMAGE_XX");
    IUFillTextVector(&UploadSettingsTP,UploadSettingsT,2,getDeviceName(),"UPLOAD_SETTINGS","Upload Settings",OPTIONS_TAB,IP_RW,60,IPS_IDLE);

    IUFillText(&ActiveDeviceT[0],"ACTIVE_TELESCOPE","Telescope","Telescope Simulator");
    IUFillText(&ActiveDeviceT[1],"ACTIVE_FOCUSER","Focuser","Focuser Simulator");
    IUFillText(&ActiveDeviceT[2],"ACTIVE_FILTER","Filter","CCD Simulator");
    IUFillTextVector(&ActiveDeviceTP,ActiveDeviceT,3,getDeviceName(),"ACTIVE_DEVICES","Snoop devices",OPTIONS_TAB,IP_RW,60,IPS_IDLE);

    IUFillNumber(&EqN[0],"RA","Ra (hh:mm:ss)","%010.6m",0,24,0,0);
    IUFillNumber(&EqN[1],"DEC","Dec (dd:mm:ss)","%010.6m",-90,90,0,0);
    IUFillNumberVector(&EqNP,EqN,2,ActiveDeviceT[0].text,"EQUATORIAL_EOD_COORD","EQ Coord","Main Control",IP_RW,60,IPS_IDLE);

    IDSnoopDevice(ActiveDeviceT[0].text,"EQUATORIAL_EOD_COORD");
    IDSnoopDevice(ActiveDeviceT[2].text,"FILTER_SLOT");
    IDSnoopDevice(ActiveDeviceT[2].text,"FILTER_NAME");

    // Guider Interface
    initGuiderProperties(getDeviceName(), GUIDE_CONTROL_TAB);

    setInterfaceDescriptor(CCD_INTERFACE|GUIDER_INTERFACE);

    return true;
}

void INDI::CCD::ISGetProperties (const char *dev)
{
    //  First we let our parent populate

    //IDLog("INDI::CCD IsGetProperties with %s\n",dev);

    DefaultDevice::ISGetProperties(dev);

    return;
}

bool INDI::CCD::updateProperties()
{
    //IDLog("INDI::CCD UpdateProperties isConnected returns %d %d\n",isConnected(),Connected);
    if(isConnected())
    {
       defineNumber(&PrimaryCCD.ImageExposureNP);

       if (capability.canAbort)
            defineSwitch(&PrimaryCCD.AbortExposureSP);
       if (capability.canSubFrame == false)
           PrimaryCCD.ImageFrameNP.p = IP_RO;

        defineNumber(&PrimaryCCD.ImageFrameNP);
       if (capability.canBin)
            defineNumber(&PrimaryCCD.ImageBinNP);

        if(capability.hasGuideHead)
        {
            defineNumber(&GuideCCD.ImageExposureNP);
            if (capability.canAbort)
                defineSwitch(&GuideCCD.AbortExposureSP);
            if (capability.canSubFrame == false)
                GuideCCD.ImageFrameNP.p = IP_RO;
            defineNumber(&GuideCCD.ImageFrameNP);
        }

        if (capability.hasCooler)
            defineNumber(&TemperatureNP);

        defineNumber(&PrimaryCCD.ImagePixelSizeNP);
        if(capability.hasGuideHead)
        {
            defineNumber(&GuideCCD.ImagePixelSizeNP);
            if (capability.canBin)
                defineNumber(&GuideCCD.ImageBinNP);
        }
        defineSwitch(&PrimaryCCD.CompressSP);
        defineBLOB(&PrimaryCCD.FitsBP);
        if(capability.hasGuideHead)
        {
            defineSwitch(&GuideCCD.CompressSP);
            defineBLOB(&GuideCCD.FitsBP);
        }
        if(capability.hasST4Port)
        {
            defineNumber(&GuideNSNP);
            defineNumber(&GuideWENP);
        }
        defineSwitch(&PrimaryCCD.FrameTypeSP);

        if (capability.canBin || capability.canSubFrame)
            defineSwitch(&PrimaryCCD.ResetSP);

        if (capability.hasGuideHead)
            defineSwitch(&GuideCCD.FrameTypeSP);

        defineSwitch(&PrimaryCCD.RapidGuideSP);

        if (capability.hasGuideHead)
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
        defineText(&ActiveDeviceTP);
        defineSwitch(&UploadSP);

        if (UploadSettingsT[0].text == NULL)
            IUSaveText(&UploadSettingsT[0], getenv("HOME"));
        defineText(&UploadSettingsTP);
    }
    else
    {
        deleteProperty(PrimaryCCD.ImageFrameNP.name);
        deleteProperty(PrimaryCCD.ImagePixelSizeNP.name);

        if (capability.canBin)
            deleteProperty(PrimaryCCD.ImageBinNP.name);

        deleteProperty(PrimaryCCD.ImageExposureNP.name);
        if (capability.canAbort)
            deleteProperty(PrimaryCCD.AbortExposureSP.name);
        deleteProperty(PrimaryCCD.FitsBP.name);
        deleteProperty(PrimaryCCD.CompressSP.name);
        deleteProperty(PrimaryCCD.RapidGuideSP.name);
        if (RapidGuideEnabled)
        {
          deleteProperty(PrimaryCCD.RapidGuideSetupSP.name);
          deleteProperty(PrimaryCCD.RapidGuideDataNP.name);
        }
        if(capability.hasGuideHead)
        {
            deleteProperty(GuideCCD.ImageExposureNP.name);
            if (capability.canAbort)
                deleteProperty(GuideCCD.AbortExposureSP.name);
                deleteProperty(GuideCCD.ImageFrameNP.name);
                deleteProperty(GuideCCD.ImagePixelSizeNP.name);

            deleteProperty(GuideCCD.FitsBP.name);
            if (capability.canBin)
                deleteProperty(GuideCCD.ImageBinNP.name);
            deleteProperty(GuideCCD.CompressSP.name);
            deleteProperty(GuideCCD.FrameTypeSP.name);
            deleteProperty(GuideCCD.RapidGuideSP.name);
            if (GuiderRapidGuideEnabled)
            {
              deleteProperty(GuideCCD.RapidGuideSetupSP.name);
              deleteProperty(GuideCCD.RapidGuideDataNP.name);
            }
        }
        if (capability.hasCooler)
            deleteProperty(TemperatureNP.name);
        if(capability.hasST4Port)
        {
            deleteProperty(GuideNSNP.name);
            deleteProperty(GuideWENP.name);
        }
        deleteProperty(PrimaryCCD.FrameTypeSP.name);
        if (capability.canBin || capability.canSubFrame)
            deleteProperty(PrimaryCCD.ResetSP.name);
        deleteProperty(ActiveDeviceTP.name);
        deleteProperty(UploadSP.name);
        deleteProperty(UploadSettingsTP.name);
    }
    return true;
}

bool INDI::CCD::ISSnoopDevice (XMLEle *root)
{
    XMLEle *ep=NULL;
    const char *propName = findXMLAttValu(root, "name");

     if(IUSnoopNumber(root,&EqNP)==0)
     {
        float newra,newdec;
        newra=EqN[0].value;
        newdec=EqN[1].value;
        if((newra != RA)||(newdec != Dec))
        {
            //IDLog("RA %4.2f  Dec %4.2f Snooped RA %4.2f  Dec %4.2f\n",RA,Dec,newra,newdec);
            RA=newra;
            Dec=newdec;
        }
     }
     else if (!strcmp(propName, "FILTER_NAME"))
     {
         FilterNames.clear();

         for (ep = nextXMLEle(root, 1) ; ep != NULL ; ep = nextXMLEle(root, 0))
             FilterNames.push_back(pcdataXMLEle(ep));
     }
     else if (!strcmp(propName, "FILTER_SLOT"))
     {
         CurrentFilterSlot=-1;
         for (ep = nextXMLEle(root, 1) ; ep != NULL ; ep = nextXMLEle(root, 0))
             CurrentFilterSlot = atoi(pcdataXMLEle(ep));
     }

     return INDI::DefaultDevice::ISSnoopDevice(root);
 }

bool INDI::CCD::ISNewText (const char *dev, const char *name, char *texts[], char *names[], int n)
{
    //  first check if it's for our device
    if(strcmp(dev,getDeviceName())==0)
    {
        //  This is for our device
        //  Now lets see if it's something we process here
        if(strcmp(name,ActiveDeviceTP.name)==0)
        {
            int rc;
            ActiveDeviceTP.s=IPS_OK;
            rc=IUUpdateText(&ActiveDeviceTP,texts,names,n);
            //  Update client display
            IDSetText(&ActiveDeviceTP,NULL);
            //saveConfig();

            // Update the property name!
            strncpy(EqNP.device, ActiveDeviceT[0].text, MAXINDIDEVICE);
            IDSnoopDevice(ActiveDeviceT[0].text,"EQUATORIAL_EOD_COORD");
            IDSnoopDevice(ActiveDeviceT[2].text,"FILTER_SLOT");
            IDSnoopDevice(ActiveDeviceT[2].text,"FILTER_NAME");

            // Tell children active devices was updated.
            activeDevicesUpdated();

            //  We processed this one, so, tell the world we did it
            return true;
        }

        if (strcmp(name, UploadSettingsTP.name)==0)
        {
            IUUpdateText(&UploadSettingsTP, texts, names, n);
            IDSetText(&UploadSettingsTP, NULL);
            return true;
        }

    }

    return INDI::DefaultDevice::ISNewText(dev,name,texts,names,n);
}

bool INDI::CCD::ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n)
{
    //  first check if it's for our device
    //IDLog("INDI::CCD::ISNewNumber %s\n",name);
    if(strcmp(dev,getDeviceName())==0)
    {
        //  This is for our device
        //  Now lets see if it's something we process here
        if(strcmp(name,"CCD_EXPOSURE")==0)
        {
            PrimaryCCD.ImageExposureN[0].value = ExposureTime = values[0];

            if (PrimaryCCD.ImageExposureNP.s==IPS_BUSY)
                AbortExposure();

            if (StartExposure(ExposureTime))
               PrimaryCCD.ImageExposureNP.s=IPS_BUSY;
            else
               PrimaryCCD.ImageExposureNP.s=IPS_ALERT;
            IDSetNumber(&PrimaryCCD.ImageExposureNP,NULL);
            return true;
        }

        if(strcmp(name,"GUIDER_EXPOSURE")==0)
        {
            GuideCCD.ImageExposureN[0].value = GuiderExposureTime = values[0];
            GuideCCD.ImageExposureNP.s=IPS_BUSY;
            if (StartGuideExposure(GuiderExposureTime))
               GuideCCD.ImageExposureNP.s=IPS_BUSY;
            else
               GuideCCD.ImageExposureNP.s=IPS_ALERT;
            IDSetNumber(&GuideCCD.ImageExposureNP,NULL);
            return true;
        }

        if(strcmp(name,"CCD_BINNING")==0)
        {
            //  We are being asked to set camera binning
            INumber *np = IUFindNumber(&PrimaryCCD.ImageBinNP, names[0]);
            if (np == NULL)
            {
                PrimaryCCD.ImageBinNP.s = IPS_ALERT;
                IDSetNumber (&PrimaryCCD.ImageBinNP, NULL);
                return false;
            }

            int binx,biny;
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
                IUUpdateNumber(&PrimaryCCD.ImageBinNP,values,names,n);
                PrimaryCCD.ImageBinNP.s=IPS_OK;

            }
            else
                PrimaryCCD.ImageBinNP.s = IPS_ALERT;

            IDSetNumber (&PrimaryCCD.ImageBinNP, NULL);

            return true;

        }

        if(strcmp(name,"GUIDER_BINNING")==0)
        {
            //  We are being asked to set camera binning
            INumber *np = IUFindNumber(&GuideCCD.ImageBinNP, names[0]);
            if (np == NULL)
            {
                GuideCCD.ImageBinNP.s = IPS_ALERT;
                IDSetNumber (&GuideCCD.ImageBinNP, NULL);
                return false;
            }

            int binx,biny;
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
                IUUpdateNumber(&GuideCCD.ImageBinNP,values,names,n);
                GuideCCD.ImageBinNP.s=IPS_OK;

            }
            else
                GuideCCD.ImageBinNP.s = IPS_ALERT;

            IDSetNumber (&GuideCCD.ImageBinNP, NULL);

            return true;


        }

        if(strcmp(name,"CCD_FRAME")==0)
        {
            //  We are being asked to set CCD Frame
            if (IUUpdateNumber(&PrimaryCCD.ImageFrameNP,values,names,n) < 0)
                return false;

            PrimaryCCD.ImageFrameNP.s=IPS_OK;

            DEBUGF(Logger::DBG_DEBUG, "Requested CCD Frame is (%3.0f,%3.0f) (%3.0f x %3.0f)", values[0],values[1],values[2],values[3]);

            if (UpdateCCDFrame(PrimaryCCD.ImageFrameN[0].value, PrimaryCCD.ImageFrameN[1].value, PrimaryCCD.ImageFrameN[2].value,
                               PrimaryCCD.ImageFrameN[3].value) == false)
                PrimaryCCD.ImageFrameNP.s = IPS_ALERT;

            IDSetNumber(&PrimaryCCD.ImageFrameNP, NULL);
            return true;
        }

        if(strcmp(name,"GUIDER_FRAME")==0)
        {
            //  We are being asked to set guide frame
            if (IUUpdateNumber(&GuideCCD.ImageFrameNP,values,names,n) < 0)
                return false;

            GuideCCD.ImageFrameNP.s=IPS_OK;

            DEBUGF(Logger::DBG_DEBUG, "Requested Guide Frame is %4.0f,%4.0f %4.0f x %4.0f",
                  values[0],values[1],values[2],values[4]);

            if (UpdateGuiderFrame(GuideCCD.ImageFrameN[0].value, GuideCCD.ImageFrameN[1].value, GuideCCD.ImageFrameN[2].value,
                               GuideCCD.ImageFrameN[3].value) == false)
                GuideCCD.ImageFrameNP.s = IPS_ALERT;

            IDSetNumber(&GuideCCD.ImageFrameNP, NULL);

            return true;
        }

        if(strcmp(name,"CCD_GUIDESTAR")==0)
        {
            PrimaryCCD.RapidGuideDataNP.s=IPS_OK;
            IUUpdateNumber(&PrimaryCCD.RapidGuideDataNP,values,names,n);
            IDSetNumber(&PrimaryCCD.RapidGuideDataNP, NULL);
            return true;
        }

        if(strcmp(name,"GUIDER_GUIDESTAR")==0)
        {
            GuideCCD.RapidGuideDataNP.s=IPS_OK;
            IUUpdateNumber(&GuideCCD.RapidGuideDataNP,values,names,n);
            IDSetNumber(&GuideCCD.RapidGuideDataNP, NULL);
            return true;
        }

        if (!strcmp(name,GuideNSNP.name) || !strcmp(name,GuideWENP.name))
        {
            processGuiderProperties(name, values, names, n);
            return true;
        }

        // CCD TEMPERATURE:
        if(!strcmp(name, TemperatureNP.name))
        {

            if(values[0] < TemperatureN[0].min || values[0] > TemperatureN[0].max)
            {
                TemperatureNP.s = IPS_ALERT;
                DEBUGF(INDI::Logger::DBG_ERROR, "Error: Bad temperature value! Range is [%.1f, %.1f] [C].",
                        TemperatureN[0].min, TemperatureN[0].max);
                IDSetNumber(&TemperatureNP, NULL);
                return false;

            }

            int rc= SetTemperature(values[0]);

            if (rc == 0)
                TemperatureNP.s = IPS_BUSY;
            else if (rc == 1)
                TemperatureNP.s = IPS_OK;
            else
                TemperatureNP.s = IPS_ALERT;

            IDSetNumber(&TemperatureNP, NULL);
            return true;
        }

        // Primary CCD Info
        if (!strcmp(name, PrimaryCCD.ImagePixelSizeNP.name))
        {
            IUUpdateNumber(&PrimaryCCD.ImagePixelSizeNP, values, names, n);
            PrimaryCCD.ImagePixelSizeNP.s = IPS_OK;
            SetCCDParams(PrimaryCCD.ImagePixelSizeNP.np[0].value, PrimaryCCD.ImagePixelSizeNP.np[1].value, PrimaryCCD.getBPP(), PrimaryCCD.ImagePixelSizeNP.np[2].value, PrimaryCCD.ImagePixelSizeNP.np[3].value);
            IDSetNumber(&PrimaryCCD.ImagePixelSizeNP, NULL);
            return true;
        }

        // Guide CCD Info
        if (!strcmp(name, GuideCCD.ImagePixelSizeNP.name))
        {
            IUUpdateNumber(&GuideCCD.ImagePixelSizeNP, values, names, n);
            GuideCCD.ImagePixelSizeNP.s = IPS_OK;
            SetGuiderParams(GuideCCD.ImagePixelSizeNP.np[0].value, GuideCCD.ImagePixelSizeNP.np[1].value, GuideCCD.getBPP(), GuideCCD.ImagePixelSizeNP.np[2].value, GuideCCD.ImagePixelSizeNP.np[3].value);
            IDSetNumber(&GuideCCD.ImagePixelSizeNP, NULL);
            return true;
        }


    }
    //  if we didn't process it, continue up the chain, let somebody else
    //  give it a shot
    return DefaultDevice::ISNewNumber(dev,name,values,names,n);
}

bool INDI::CCD::ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n)
{

    if(strcmp(dev,getDeviceName())==0)
    {
        if (!strcmp(name, UploadSP.name))
        {
            IUUpdateSwitch(&UploadSP, states, names, n);
            UploadSP.s = IPS_OK;
            IDSetSwitch(&UploadSP, NULL);

            if (UploadS[0].s == ISS_ON)
                DEBUG(INDI::Logger::DBG_SESSION, "Upload settings set to client only.");
            else if (UploadS[1].s == ISS_ON)
                DEBUG(INDI::Logger::DBG_SESSION, "Upload settings set to local only.");
            else
                DEBUG(INDI::Logger::DBG_SESSION, "Upload settings set to client and local.");
            return true;
        }

        // Reset
        if(strcmp(name,PrimaryCCD.ResetSP.name)==0)
        {
          IUResetSwitch(&PrimaryCCD.ResetSP);
          PrimaryCCD.ResetSP.s = IPS_OK;
          if (capability.canBin)
              UpdateCCDBin(1,1);
          if (capability.canSubFrame)
              UpdateCCDFrame(0,0, PrimaryCCD.getXRes(), PrimaryCCD.getYRes());

          IDSetSwitch(&PrimaryCCD.ResetSP, NULL);
          return true;
        }

        if(strcmp(name,PrimaryCCD.AbortExposureSP.name)==0)
        {
            IUResetSwitch(&PrimaryCCD.AbortExposureSP);

            if (AbortExposure())
            {
                PrimaryCCD.AbortExposureSP.s = IPS_OK;
                PrimaryCCD.ImageExposureNP.s = IPS_IDLE;
                PrimaryCCD.ImageExposureN[0].value = 0;
            }
            else
            {
                PrimaryCCD.AbortExposureSP.s = IPS_ALERT;
                PrimaryCCD.ImageExposureNP.s = IPS_ALERT;
            }

            IDSetSwitch(&PrimaryCCD.AbortExposureSP, NULL);
            IDSetNumber(&PrimaryCCD.ImageExposureNP, NULL);

            return true;
        }

        if(strcmp(name,GuideCCD.AbortExposureSP.name)==0)
        {
            IUResetSwitch(&GuideCCD.AbortExposureSP);

            if (AbortGuideExposure())
            {
                GuideCCD.AbortExposureSP.s = IPS_OK;
                GuideCCD.ImageExposureNP.s = IPS_IDLE;
                GuideCCD.ImageExposureN[0].value = 0;
            }
            else
            {
                GuideCCD.AbortExposureSP.s = IPS_ALERT;
                GuideCCD.ImageExposureNP.s = IPS_ALERT;
            }

            IDSetSwitch(&GuideCCD.AbortExposureSP, NULL);
            IDSetNumber(&GuideCCD.ImageExposureNP, NULL);

            return true;
        }

        if(strcmp(name,PrimaryCCD.CompressSP.name)==0)
        {

            IUUpdateSwitch(&PrimaryCCD.CompressSP,states,names,n);
            PrimaryCCD.CompressSP.s = IPS_OK;
            IDSetSwitch(&PrimaryCCD.CompressSP,NULL);

            if(PrimaryCCD.CompressS[0].s==ISS_ON    )
            {
                PrimaryCCD.SendCompressed=true;
            } else
            {
                PrimaryCCD.SendCompressed=false;
            }
            return true;
        }

        if(strcmp(name,GuideCCD.CompressSP.name)==0)
        {

            IUUpdateSwitch(&GuideCCD.CompressSP,states,names,n);
            GuideCCD.CompressSP.s = IPS_OK;
            IDSetSwitch(&GuideCCD.CompressSP,NULL);

            if(GuideCCD.CompressS[0].s==ISS_ON    )
            {
                GuideCCD.SendCompressed=true;
            } else
            {
                GuideCCD.SendCompressed=false;
            }
            return true;
        }

        if(strcmp(name,PrimaryCCD.FrameTypeSP.name)==0)
        {
            IUUpdateSwitch(&PrimaryCCD.FrameTypeSP,states,names,n);
            PrimaryCCD.FrameTypeSP.s=IPS_OK;
            if(PrimaryCCD.FrameTypeS[0].s==ISS_ON)
                PrimaryCCD.setFrameType(CCDChip::LIGHT_FRAME);
            else if(PrimaryCCD.FrameTypeS[1].s==ISS_ON)
            {
                PrimaryCCD.setFrameType(CCDChip::BIAS_FRAME);
                if (capability.hasShutter == false)
                    DEBUG(INDI::Logger::DBG_WARNING, "The CCD does not have a shutter. Cover the camera in order to take a bias frame.");
            }
            else if(PrimaryCCD.FrameTypeS[2].s==ISS_ON)
            {
                PrimaryCCD.setFrameType(CCDChip::DARK_FRAME);
                if (capability.hasShutter == false)
                    DEBUG(INDI::Logger::DBG_WARNING, "The CCD does not have a shutter. Cover the camera in order to take a dark frame.");
            }
            else if(PrimaryCCD.FrameTypeS[3].s==ISS_ON)
                PrimaryCCD.setFrameType(CCDChip::FLAT_FRAME);

            if (UpdateCCDFrameType(PrimaryCCD.getFrameType()) == false)
                PrimaryCCD.FrameTypeSP.s = IPS_ALERT;

            IDSetSwitch(&PrimaryCCD.FrameTypeSP,NULL);

            return true;
        }

        if(strcmp(name,GuideCCD.FrameTypeSP.name)==0)
        {
            //  Compression Update
            IUUpdateSwitch(&GuideCCD.FrameTypeSP,states,names,n);
            GuideCCD.FrameTypeSP.s=IPS_OK;
            if(GuideCCD.FrameTypeS[0].s==ISS_ON)
                GuideCCD.setFrameType(CCDChip::LIGHT_FRAME);
            else if(GuideCCD.FrameTypeS[1].s==ISS_ON)
            {
                GuideCCD.setFrameType(CCDChip::BIAS_FRAME);
                if (capability.hasShutter == false)
                    DEBUG(INDI::Logger::DBG_WARNING, "The CCD does not have a shutter. Cover the camera in order to take a bias frame.");
            }
            else if(GuideCCD.FrameTypeS[2].s==ISS_ON)
            {
                GuideCCD.setFrameType(CCDChip::DARK_FRAME);
                if (capability.hasShutter == false)
                    DEBUG(INDI::Logger::DBG_WARNING, "The CCD does not have a shutter. Cover the camera in order to take a dark frame.");
            }
            else if(GuideCCD.FrameTypeS[3].s==ISS_ON)
                GuideCCD.setFrameType(CCDChip::FLAT_FRAME);

            if (UpdateGuiderFrameType(GuideCCD.getFrameType()) == false)
                GuideCCD.FrameTypeSP.s = IPS_ALERT;

            IDSetSwitch(&GuideCCD.FrameTypeSP,NULL);

            return true;
        }


        if (strcmp(name, PrimaryCCD.RapidGuideSP.name)==0)
        {
            IUUpdateSwitch(&PrimaryCCD.RapidGuideSP, states, names, n);
            PrimaryCCD.RapidGuideSP.s=IPS_OK;
            RapidGuideEnabled=(PrimaryCCD.RapidGuideS[0].s==ISS_ON);

            if (RapidGuideEnabled) {
              defineSwitch(&PrimaryCCD.RapidGuideSetupSP);
              defineNumber(&PrimaryCCD.RapidGuideDataNP);
            }
            else {
              deleteProperty(PrimaryCCD.RapidGuideSetupSP.name);
              deleteProperty(PrimaryCCD.RapidGuideDataNP.name);
            }

            IDSetSwitch(&PrimaryCCD.RapidGuideSP,NULL);
            return true;
        }

        if (strcmp(name, GuideCCD.RapidGuideSP.name)==0)
        {
            IUUpdateSwitch(&GuideCCD.RapidGuideSP, states, names, n);
            GuideCCD.RapidGuideSP.s=IPS_OK;
            GuiderRapidGuideEnabled=(GuideCCD.RapidGuideS[0].s==ISS_ON);

            if (GuiderRapidGuideEnabled) {
              defineSwitch(&GuideCCD.RapidGuideSetupSP);
              defineNumber(&GuideCCD.RapidGuideDataNP);
            }
            else {
              deleteProperty(GuideCCD.RapidGuideSetupSP.name);
              deleteProperty(GuideCCD.RapidGuideDataNP.name);
            }

            IDSetSwitch(&GuideCCD.RapidGuideSP,NULL);
            return true;
        }

        if (strcmp(name, PrimaryCCD.RapidGuideSetupSP.name)==0)
        {
            IUUpdateSwitch(&PrimaryCCD.RapidGuideSetupSP, states, names, n);
            PrimaryCCD.RapidGuideSetupSP.s=IPS_OK;

            AutoLoop=(PrimaryCCD.RapidGuideSetupS[0].s==ISS_ON);
            SendImage=(PrimaryCCD.RapidGuideSetupS[1].s==ISS_ON);
            ShowMarker=(PrimaryCCD.RapidGuideSetupS[2].s==ISS_ON);

            IDSetSwitch(&PrimaryCCD.RapidGuideSetupSP,NULL);
            return true;
        }

        if (strcmp(name, GuideCCD.RapidGuideSetupSP.name)==0)
        {
            IUUpdateSwitch(&GuideCCD.RapidGuideSetupSP, states, names, n);
            GuideCCD.RapidGuideSetupSP.s=IPS_OK;

            GuiderAutoLoop=(GuideCCD.RapidGuideSetupS[0].s==ISS_ON);
            GuiderSendImage=(GuideCCD.RapidGuideSetupS[1].s==ISS_ON);
            GuiderShowMarker=(GuideCCD.RapidGuideSetupS[2].s==ISS_ON);

            IDSetSwitch(&GuideCCD.RapidGuideSetupSP,NULL);
            return true;
        }
    }

    // let the default driver have a crack at it
    return DefaultDevice::ISNewSwitch(dev, name, states, names, n);
}

int INDI::CCD::SetTemperature(double temperature)
{
    INDI_UNUSED(temperature);
    DEBUGF(INDI::Logger::DBG_WARNING, "INDI::CCD::SetTemperature %4.2f -  Should never get here", temperature);
    return -1;
}

bool INDI::CCD::StartExposure(float duration)
{
    DEBUGF(INDI::Logger::DBG_WARNING, "INDI::CCD::StartExposure %4.2f -  Should never get here",duration);
    return false;
}

bool INDI::CCD::StartGuideExposure(float duration)
{
    DEBUGF(INDI::Logger::DBG_WARNING, "INDI::CCD::StartGuide Exposure %4.2f -  Should never get here",duration);
    return false;
}

bool INDI::CCD::AbortExposure()
{
    DEBUG(INDI::Logger::DBG_WARNING, "INDI::CCD::AbortExposure -  Should never get here");
    return false;
}

bool INDI::CCD::AbortGuideExposure()
{
    DEBUG(INDI::Logger::DBG_WARNING, "INDI::CCD::AbortGuideExposure -  Should never get here");
    return false;
}

bool INDI::CCD::UpdateCCDFrame(int x, int y, int w, int h)
{
    // Just set value, unless HW layer overrides this and performs its own processing
    PrimaryCCD.setFrame(x, y, w, h);
    return true;
}

bool INDI::CCD::UpdateGuiderFrame(int x, int y, int w, int h)
{
    GuideCCD.setFrame(x,y, w,h);
    return true;
}

bool INDI::CCD::UpdateCCDBin(int hor, int ver)
{
    // Just set value, unless HW layer overrides this and performs its own processing
    PrimaryCCD.setBin(hor,ver);
    return true;
}

bool INDI::CCD::UpdateGuiderBin(int hor, int ver)
{
    // Just set value, unless HW layer overrides this and performs its own processing
    GuideCCD.setBin(hor, ver);
    return true;
}

bool INDI::CCD::UpdateCCDFrameType(CCDChip::CCD_FRAME fType)
{
    INDI_UNUSED(fType);
    // Child classes can override this
    return true;
}

bool INDI::CCD::UpdateGuiderFrameType(CCDChip::CCD_FRAME fType)
{
    INDI_UNUSED(fType);
    // Child classes can override this
    return true;
}

void INDI::CCD::addFITSKeywords(fitsfile *fptr, CCDChip *targetChip)
{
    int status=0;
    char frame_s[32];
    char dev_name[32];
    char exp_start[32];
    double min_val, max_val;
    double exposureDuration;
    double pixSize1,pixSize2;
    unsigned int xbin, ybin;

    if (targetChip->getNAxis() == 2)
        getMinMax(&min_val, &max_val, targetChip);

    xbin = targetChip->getBinX();
    ybin = targetChip->getBinY();

    switch (targetChip->getFrameType())
    {
      case CCDChip::LIGHT_FRAME:
        strcpy(frame_s, "Light");
    break;
      case CCDChip::BIAS_FRAME:
        strcpy(frame_s, "Bias");
    break;
      case CCDChip::FLAT_FRAME:
        strcpy(frame_s, "Flat Field");
    break;
      case CCDChip::DARK_FRAME:
        strcpy(frame_s, "Dark");
    break;
    }

    exposureDuration = targetChip->getExposureDuration();

    pixSize1 = targetChip->getPixelSizeX();
    pixSize2 = targetChip->getPixelSizeY();

    strncpy(dev_name, getDeviceName(), 32);
    strncpy(exp_start, targetChip->getExposureStartTime(), 32);

    fits_update_key_s(fptr, TDOUBLE, "EXPTIME", &(exposureDuration), "Total Exposure Time (s)", &status);

    if(targetChip->getFrameType() == CCDChip::DARK_FRAME)
        fits_update_key_s(fptr, TDOUBLE, "DARKTIME", &(exposureDuration), "Total Exposure Time (s)", &status);

    fits_update_key_s(fptr, TDOUBLE, "PIXSIZE1", &(pixSize1), "Pixel Size 1 (microns)", &status);
    fits_update_key_s(fptr, TDOUBLE, "PIXSIZE2", &(pixSize2), "Pixel Size 2 (microns)", &status);
    fits_update_key_s(fptr, TUINT, "XBINNING", &(xbin) , "Binning factor in width", &status);
    fits_update_key_s(fptr, TUINT, "YBINNING", &(ybin), "Binning factor in height", &status);
    fits_update_key_s(fptr, TSTRING, "FRAME", frame_s, "Frame Type", &status);
    if (CurrentFilterSlot != -1 && CurrentFilterSlot <= FilterNames.size())
    {
        char filter[32];
        strncpy(filter, FilterNames.at(CurrentFilterSlot-1).c_str(), 32);
        fits_update_key_s(fptr, TSTRING, "FILTER", filter, "Filter", &status);
    }

    if (targetChip->getNAxis() == 2)
    {
        fits_update_key_s(fptr, TDOUBLE, "DATAMIN", &min_val, "Minimum value", &status);
        fits_update_key_s(fptr, TDOUBLE, "DATAMAX", &max_val, "Maximum value", &status);
    }

    if (RA != -1000 && Dec != -1000)
    {
        fits_update_key_s(fptr, TDOUBLE, "OBJCTRA", &RA, "Object RA", &status);
        fits_update_key_s(fptr, TDOUBLE, "OBJCTDEC", &Dec, "Object DEC", &status);
    }

    fits_update_key_s(fptr, TSTRING, "INSTRUME", dev_name, "CCD Name", &status);
    fits_update_key_s(fptr, TSTRING, "DATE-OBS", exp_start, "UTC start date of observation", &status);

}

void INDI::CCD::fits_update_key_s(fitsfile* fptr, int type, std::string name, void* p, std::string explanation, int* status)
{
        // this function is for removing warnings about deprecated string conversion to char* (from arg 5)
        fits_update_key(fptr,type,name.c_str(),p, const_cast<char*>(explanation.c_str()), status);
}

bool INDI::CCD::ExposureComplete(CCDChip *targetChip)
{
    bool sendImage = (UploadS[0].s == ISS_ON || UploadS[2].s == ISS_ON);
    bool saveImage = (UploadS[1].s == ISS_ON || UploadS[2].s == ISS_ON);
    bool showMarker = false;
    bool autoLoop = false;
    bool sendData = false;

    if (RapidGuideEnabled && targetChip == &PrimaryCCD)
    {
      autoLoop = AutoLoop;
      sendImage = SendImage;
      showMarker = ShowMarker;
      sendData = true;
      saveImage = false;
    }

    if (GuiderRapidGuideEnabled && targetChip == &GuideCCD)
    {
      autoLoop = GuiderAutoLoop;
      sendImage = GuiderSendImage;
      showMarker = GuiderShowMarker;
      sendData = true;
      saveImage = false;
    }

    if (sendData)
    {
      static double P0 = 0.906, P1 = 0.584, P2 = 0.365, P3 = 0.117, P4 = 0.049, P5 = -0.05, P6 = -0.064, P7 = -0.074, P8 = -0.094;
      targetChip->RapidGuideDataNP.s=IPS_BUSY;
      int width = targetChip->getSubW() / targetChip->getBinX();
      int height = targetChip->getSubH() / targetChip->getBinY();
      unsigned short *src = (unsigned short *) targetChip->getFrameBuffer();
      int i0, i1, i2, i3, i4, i5, i6, i7, i8;
      int ix = 0, iy = 0;
      int xM4;
      unsigned short *p;
      double average, fit, bestFit = 0;
      int minx = 4;
      int maxx = width -4;
      int miny = 4;
      int maxy = height -4;
      if (targetChip->lastRapidX > 0 && targetChip->lastRapidY > 0) {
        minx = std::max(targetChip->lastRapidX - 20, 4);
        maxx = std::min(targetChip->lastRapidX + 20, width - 4);
        miny = std::max(targetChip->lastRapidY - 20, 4);
        maxy = std::min(targetChip->lastRapidY + 20, height -4);
      }
      for (int x = minx; x < maxx; x++)
        for (int y = miny; y < maxy; y++)
        {
          i0 = i1 = i2 = i3 = i4 = i5 = i6 = i7 = i8 = 0;
          xM4 = x - 4;
          p = src + (y - 4) * width + xM4; i8 += *p++; i8 += *p++; i8 += *p++; i8 += *p++; i8 += *p++; i8 += *p++; i8 += *p++; i8 += *p++; i8 += *p++;
          p = src + (y - 3) * width + xM4; i8 += *p++; i8 += *p++; i8 += *p++; i7 += *p++; i6 += *p++; i7 += *p++; i8 += *p++; i8 += *p++; i8 += *p++;
          p = src + (y - 2) * width + xM4; i8 += *p++; i8 += *p++; i5 += *p++; i4 += *p++; i3 += *p++; i4 += *p++; i5 += *p++; i8 += *p++; i8 += *p++;
          p = src + (y - 1) * width + xM4; i8 += *p++; i7 += *p++; i4 += *p++; i2 += *p++; i1 += *p++; i2 += *p++; i4 += *p++; i8 += *p++; i8 += *p++;
          p = src + (y + 0) * width + xM4; i8 += *p++; i6 += *p++; i3 += *p++; i1 += *p++; i0 += *p++; i1 += *p++; i3 += *p++; i6 += *p++; i8 += *p++;
          p = src + (y + 1) * width + xM4; i8 += *p++; i7 += *p++; i4 += *p++; i2 += *p++; i1 += *p++; i2 += *p++; i4 += *p++; i8 += *p++; i8 += *p++;
          p = src + (y + 2) * width + xM4; i8 += *p++; i8 += *p++; i5 += *p++; i4 += *p++; i3 += *p++; i4 += *p++; i5 += *p++; i8 += *p++; i8 += *p++;
          p = src + (y + 3) * width + xM4; i8 += *p++; i8 += *p++; i8 += *p++; i7 += *p++; i6 += *p++; i7 += *p++; i8 += *p++; i8 += *p++; i8 += *p++;
          p = src + (y + 4) * width + xM4; i8 += *p++; i8 += *p++; i8 += *p++; i8 += *p++; i8 += *p++; i8 += *p++; i8 += *p++; i8 += *p++; i8 += *p++;
          average = (i0 + i1 + i2 + i3 + i4 + i5 + i6 + i7 + i8) / 85.0;
          fit = P0 * (i0 - average) + P1 * (i1 - 4 * average) + P2 * (i2 - 4 * average) + P3 * (i3 - 4 * average) + P4 * (i4 - 8 * average) + P5 * (i5 - 4 * average) + P6 * (i6 - 4 * average) + P7 * (i7 - 8 * average) + P8 * (i8 - 48 * average);
          if (bestFit < fit)
          {
            bestFit = fit;
            ix = x;
            iy = y;
          }
        }

      targetChip->RapidGuideDataN[0].value = ix;
      targetChip->RapidGuideDataN[1].value = iy;
      targetChip->RapidGuideDataN[2].value = bestFit;
      targetChip->lastRapidX = ix;
      targetChip->lastRapidY = iy;
      if (bestFit > 50)
      {
        int sumX = 0;
        int sumY = 0;
        int total = 0;
        int max = 0;
        int noiseThreshold = 0;
        for (int y = iy - 4; y <= iy + 4; y++) {
          p = src + y * width + ix - 4;
          for (int x = ix - 4; x <= ix + 4; x++) {
            int w = *p++;
            noiseThreshold += w;
            if (w > max)
              max = w;
          }
        }
        noiseThreshold = (noiseThreshold/81+max)/2; // set threshold between peak and average
        for (int y = iy - 4; y <= iy + 4; y++) {
          p = src + y * width + ix - 4;
          for (int x = ix - 4; x <= ix + 4; x++) {
            int w = *p++;
            if (w < noiseThreshold)
              w = 0;
            sumX += x * w;
            sumY += y * w;
            total += w;
          }
        }
        if (total > 0)
        {
          targetChip->RapidGuideDataN[0].value = ((double)sumX)/total;
          targetChip->RapidGuideDataN[1].value = ((double)sumY)/total;
          targetChip->RapidGuideDataNP.s=IPS_OK;

          DEBUGF(INDI::Logger::DBG_DEBUG, "Guide Star X: %g Y: %g FIT: %g", targetChip->RapidGuideDataN[0].value, targetChip->RapidGuideDataN[1].value,
                  targetChip->RapidGuideDataN[2].value);
        }
        else
        {
          targetChip->RapidGuideDataNP.s=IPS_ALERT;
          targetChip->lastRapidX = targetChip->lastRapidY = -1;
        }
      }
      else
      {
        targetChip->RapidGuideDataNP.s=IPS_ALERT;
        targetChip->lastRapidX = targetChip->lastRapidY = -1;
      }
      IDSetNumber(&targetChip->RapidGuideDataNP,NULL);

      if (showMarker)
      {
        int xmin = std::max(ix - 10, 0);
        int xmax = std::min(ix + 10, width - 1);
        int ymin = std::max(iy - 10, 0);
        int ymax = std::min(iy + 10, height - 1);

        //fprintf(stderr, "%d %d %d %d\n", xmin, xmax, ymin, ymax);

        if (ymin > 0)
        {
          p = src + ymin * width + xmin;
          for (int x = xmin; x <= xmax; x++)
            *p++ = 50000;
        }

        if (xmin > 0)
        {
          for (int y = ymin; y<= ymax; y++)
          {
            *(src + y * width + xmin) = 50000;
          }
        }

        if (xmax < width - 1)
        {
          for (int y = ymin; y<= ymax; y++)
          {
            *(src + y * width + xmax) = 50000;
          }
        }

        if (ymax < height -1)
        {
          p = src + ymax * width + xmin;
          for (int x = xmin; x <= xmax; x++)
            *p++ = 50000;
        }
      }
    }

    if (sendImage || saveImage)
    {
      if (!strcmp(targetChip->getImageExtension(), "fits"))
      {
          void *memptr;
          size_t memsize;
          int img_type=0;
          int byte_type=0;
          int status=0;
          long naxis=targetChip->getNAxis();
          long naxes[naxis];
          int nelements=0;
          std::string bit_depth;

          fitsfile *fptr=NULL;

          naxes[0]=targetChip->getSubW()/targetChip->getBinX();
          naxes[1]=targetChip->getSubH()/targetChip->getBinY();

          switch (targetChip->getBPP())
          {
              case 8:
                  byte_type = TBYTE;
                  img_type  = BYTE_IMG;
                  bit_depth = "8 bits per pixel";
                  break;

              case 16:
                  byte_type = TUSHORT;
                  img_type = USHORT_IMG;
                  bit_depth = "16 bits per pixel";
                  break;

              case 32:
                  byte_type = TULONG;
                  img_type = ULONG_IMG;
                  bit_depth = "32 bits per pixel";
                  break;

               default:
                  DEBUGF(Logger::DBG_WARNING, "Unsupported bits per pixel value %d\n", targetChip->getBPP() );
                  return false;
                  break;
          }

          nelements = naxes[0] * naxes[1];
          if (naxis== 3)
          {
              nelements *= 3;
              naxes[2] = 3;
          }

          /*DEBUGF(Logger::DBG_DEBUG, "Exposure complete. Image Depth: %s. Width: %d Height: %d nelements: %d", bit_depth.c_str(), naxes[0],
                  naxes[1], nelements);*/

          //  Now we have to send fits format data to the client
          memsize=5760;
          memptr=malloc(memsize);
          if(!memptr)
          {
              IDLog("Error: failed to allocate memory: %lu\n",(unsigned long)memsize);
          }

          fits_create_memfile(&fptr,&memptr,&memsize,2880,realloc,&status);

          if(status)
          {
            IDLog("Error: Failed to create FITS image\n");
            fits_report_error(stderr, status);  /* print out any error messages */
            return false;
          }

          fits_create_img(fptr, img_type , naxis, naxes, &status);

          if (status)
          {
            IDLog("Error: Failed to create FITS image\n");
            fits_report_error(stderr, status);  /* print out any error messages */
            return false;
          }

          addFITSKeywords(fptr, targetChip);

          fits_write_img(fptr,byte_type,1,nelements,targetChip->getFrameBuffer(),&status);

          if (status)
          {
            IDLog("Error: Failed to write FITS image\n");
            fits_report_error(stderr, status);  /* print out any error messages */
            return false;
          }

          fits_close_file(fptr,&status);

          uploadFile(targetChip, memptr, memsize, sendImage, saveImage);

          free(memptr);
      }
      else
      {
          uploadFile(targetChip, targetChip->getFrameBuffer(), targetChip->getFrameBufferSize(), sendImage, saveImage);
      }


    }

    targetChip->ImageExposureNP.s=IPS_OK;
    IDSetNumber(&targetChip->ImageExposureNP,NULL);

    if (autoLoop)
    {
      if (targetChip == &PrimaryCCD)
      {
        PrimaryCCD.ImageExposureN[0].value = ExposureTime;
        PrimaryCCD.ImageExposureNP.s=IPS_BUSY;
        if (StartExposure(ExposureTime))
           PrimaryCCD.ImageExposureNP.s=IPS_BUSY;
        else
        {
           DEBUG(INDI::Logger::DBG_DEBUG, "Autoloop: Primary CCD Exposure Error!");
           PrimaryCCD.ImageExposureNP.s=IPS_ALERT;
        }

        IDSetNumber(&PrimaryCCD.ImageExposureNP,NULL);
      }
      else
      {
        GuideCCD.ImageExposureN[0].value = GuiderExposureTime;
        GuideCCD.ImageExposureNP.s=IPS_BUSY;
        if (StartGuideExposure(GuiderExposureTime))
           GuideCCD.ImageExposureNP.s=IPS_BUSY;
        else
        {
           DEBUG(INDI::Logger::DBG_DEBUG, "Autoloop: Guide CCD Exposure Error!");
           GuideCCD.ImageExposureNP.s=IPS_ALERT;
        }

        IDSetNumber(&GuideCCD.ImageExposureNP,NULL);
      }
    }

    return true;
}

bool INDI::CCD::uploadFile(CCDChip * targetChip, const void *fitsData, size_t totalBytes, bool sendImage, bool saveImage)
{
    unsigned char *compressedData = NULL;
    uLongf compressedBytes=0;

    if (saveImage)
    {
        targetChip->FitsB.blob=(unsigned char *)fitsData;
        targetChip->FitsB.bloblen=totalBytes;
        snprintf(targetChip->FitsB.format, MAXINDIBLOBFMT, ".%s", targetChip->getImageExtension());

        FILE *fp = NULL;
        char imageFileName[MAXRBUF];
        std::string prefix = UploadSettingsT[1].text;
        int maxIndex = getFileIndex(UploadSettingsT[0].text, UploadSettingsT[1].text, targetChip->FitsB.format);

        if (maxIndex < 0)
        {
            DEBUGF(INDI::Logger::DBG_ERROR, "Error iterating directory %s. %s", UploadSettingsT[0].text, strerror(errno));
            return false;
        }

        if (maxIndex > 0)
        {
            char indexString[3];
            snprintf(indexString, 3, "%02d", maxIndex);
            std::string prefixIndex = indexString;
            prefix.replace(prefix.find("XX"), 2, prefixIndex);
        }

        snprintf(imageFileName, MAXRBUF, "%s/%s%s", UploadSettingsT[0].text, prefix.c_str(), targetChip->FitsB.format);
        fp = fopen(imageFileName, "w");
        if (fp == NULL)
        {
            DEBUGF(INDI::Logger::DBG_ERROR, "Unable to save image file (%s). %s", imageFileName, strerror(errno));
            return false;
        }

        int n=0;
        for (int nr=0; nr < (int) targetChip->FitsB.bloblen; nr += n)
            n = fwrite( (static_cast<char *>(targetChip->FitsB.blob) + nr), 1, targetChip->FitsB.bloblen - nr, fp);

        DEBUGF(INDI::Logger::DBG_SESSION, "Image saved to %s", imageFileName);
        fclose(fp);
    }

    if (targetChip->SendCompressed)
    {
        compressedBytes = sizeof(char) * totalBytes + totalBytes / 64 + 16 + 3;
        compressedData = (unsigned char *) malloc (compressedBytes);

        if (fitsData == NULL || compressedData == NULL)
        {
            if (compressedData)
                free(compressedData);
            DEBUG(INDI::Logger::DBG_ERROR, "Error: Ran out of memory compressing image");
            return false;
        }

        int r = compress2(compressedData, &compressedBytes, (const Bytef*)fitsData, totalBytes, 9);
        if (r != Z_OK)
        {
            /* this should NEVER happen */
            DEBUG(INDI::Logger::DBG_ERROR, "Error: Failed to compress image");
            return false;
        }

        targetChip->FitsB.blob=compressedData;
        targetChip->FitsB.bloblen=compressedBytes;
        snprintf(targetChip->FitsB.format, MAXINDIBLOBFMT, ".%s.z", targetChip->getImageExtension());
    } else
    {
        targetChip->FitsB.blob=(unsigned char *)fitsData;
        targetChip->FitsB.bloblen=totalBytes;
        snprintf(targetChip->FitsB.format, MAXINDIBLOBFMT, ".%s", targetChip->getImageExtension());
    }

    targetChip->FitsB.size = totalBytes;
    targetChip->FitsBP.s=IPS_OK;

    if (sendImage)
        IDSetBLOB(&targetChip->FitsBP,NULL);

    if (compressedData)
        free (compressedData);

    return true;
}

void INDI::CCD::SetCCDParams(int x,int y,int bpp,float xf,float yf)
{
    PrimaryCCD.setResolution(x, y);
    PrimaryCCD.setFrame(0, 0, x, y);
    if (capability.canBin)
        PrimaryCCD.setBin(1,1);
    PrimaryCCD.setPixelSize(xf, yf);
    PrimaryCCD.setBPP(bpp);

}

void INDI::CCD::SetGuiderParams(int x,int y,int bpp,float xf,float yf)
{
    capability.hasGuideHead=true;

    GuideCCD.setResolution(x, y);
    GuideCCD.setFrame(0, 0, x, y);
    GuideCCD.setPixelSize(xf, yf);
    GuideCCD.setBPP(bpp);

}

bool INDI::CCD::saveConfigItems(FILE *fp)
{
    IUSaveConfigText(fp, &ActiveDeviceTP);
    IUSaveConfigSwitch(fp, &UploadSP);
    IUSaveConfigText(fp, &UploadSettingsTP);

    IUSaveConfigSwitch(fp, &PrimaryCCD.CompressSP);

    if (capability.hasGuideHead)
        IUSaveConfigSwitch(fp, &GuideCCD.CompressSP);

    if (capability.canSubFrame)
        IUSaveConfigNumber(fp, &PrimaryCCD.ImageFrameNP);

    if (capability.canBin)
        IUSaveConfigNumber(fp, &PrimaryCCD.ImageBinNP);

    return true;
}

bool INDI::CCD::GuideNorth(float ms)
{
    return false;
}

bool INDI::CCD::GuideSouth(float ms)
{
    return false;
}

bool INDI::CCD::GuideEast(float ms)
{
    return false;
}

bool INDI::CCD::GuideWest(float ms)
{
    return false;
}

void INDI::CCD::getMinMax(double *min, double *max, CCDChip *targetChip)
{
    int ind=0, i, j;
    int imageHeight = targetChip->getSubH() / targetChip->getBinY();
    int imageWidth  = targetChip->getSubW() / targetChip->getBinX();
    double lmin, lmax;

    switch (targetChip->getBPP())
    {
        case 8:
        {
            unsigned char *imageBuffer = (unsigned char *) targetChip->getFrameBuffer();
            lmin = lmax = imageBuffer[0];


            for (i= 0; i < imageHeight ; i++)
                for (j= 0; j < imageWidth; j++)
                {
                    ind = (i * imageWidth) + j;
                    if (imageBuffer[ind] < lmin) lmin = imageBuffer[ind];
                    else if (imageBuffer[ind] > lmax) lmax = imageBuffer[ind];
                }

        }
        break;

        case 16:
        {
            unsigned short *imageBuffer = (unsigned short* ) targetChip->getFrameBuffer();
            lmin = lmax = imageBuffer[0];

            for (i= 0; i < imageHeight ; i++)
                for (j= 0; j < imageWidth; j++)
                {
                    ind = (i * imageWidth) + j;
                    if (imageBuffer[ind] < lmin) lmin = imageBuffer[ind];
                     else if (imageBuffer[ind] > lmax) lmax = imageBuffer[ind];
                }

        }
        break;

        case 32:
        {
            unsigned int *imageBuffer = (unsigned int* ) targetChip->getFrameBuffer();
            lmin = lmax = imageBuffer[0];

            for (i= 0; i < imageHeight ; i++)
                for (j= 0; j < imageWidth; j++)
                {
                    ind = (i * imageWidth) + j;
                    if (imageBuffer[ind] < lmin) lmin = imageBuffer[ind];
                    else if (imageBuffer[ind] > lmax) lmax = imageBuffer[ind];

                }

        }
        break;

    }
        *min = lmin;
        *max = lmax;
}

int INDI::CCD::getFileIndex(const char *dir, const char *prefix, const char *ext)
{
    DIR *dpdf;
    struct dirent *epdf;
    std::vector<std::string> files = std::vector<std::string>();

    std::string prefixIndex = prefix;
    if (prefixIndex.find("XX") == std::string::npos)
        return 0;

    std::string prefixSearch = prefix;
    prefixSearch.replace(prefixSearch.find("XX"), 2, "");

    dpdf = opendir(dir);
    if (dpdf != NULL)
    {
       while (epdf = readdir(dpdf))
       {
          if (strstr(epdf->d_name, prefixSearch.c_str()))
              files.push_back(epdf->d_name);
       }
    }
    else
        return -1;

    int maxIndex=0;

    std::string filterIndex = "%d";
    prefixIndex.replace(prefixIndex.find("XX"), 2, filterIndex);
    char filter[MAXRBUF];
    snprintf(filter, MAXRBUF, "%s%s", prefixIndex.c_str(), ext);
    for (int i=0; i < files.size(); i++)
    {
        int index=-1;
        sscanf(files.at(i).c_str(), filter, &index);
        if (index > maxIndex)
            maxIndex=index;
    }

    return (maxIndex+1);

}


