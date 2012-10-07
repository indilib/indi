/*******************************************************************************
  Copyright(c) 2010, 2011 Gerry Rozema, Jasem Mutlaq. All rights reserved.

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

#include <zlib.h>

const char *IMAGE_SETTINGS_TAB = "Image Settings";
const char *IMAGE_INFO_TAB     = "Image Info";
const char *GUIDE_HEAD_TAB     = "Guide Head";
const char *GUIDE_CONTROL_TAB     = "Guider Control";

CCDChip::CCDChip()
{
    SendCompressed=false;
    Interlaced=false;

    RawFrame=NULL;
    RawFrameSize=0;

    BinX = BinY = 1;

    FrameType=LIGHT_FRAME;

    ImageFrameNP = new INumberVectorProperty;
    FrameTypeSP = new ISwitchVectorProperty;
    ImageExposureNP = new INumberVectorProperty;
    ImageBinNP = new INumberVectorProperty;
    ImagePixelSizeNP = new INumberVectorProperty;
    CompressSP = new ISwitchVectorProperty;
    FitsBP = new IBLOBVectorProperty;

}

CCDChip::~CCDChip()
{
    delete RawFrame;
    RawFrameSize=0;
    RawFrame=NULL;

    delete ImageFrameNP;
    delete FrameTypeSP;
    delete ImageExposureNP;
    delete ImageBinNP;
    delete ImagePixelSizeNP;
    delete CompressSP;
    delete FitsBP;
}

int CCDChip::setFrameType(CCD_FRAME t)
{
    //fprintf(stderr,"Set frame type to %d\n",t);
    FrameType=t;
    return 0;
}

void CCDChip::setResolutoin(int x, int y)
{
    XRes = x;
    YRes = y;

    ImagePixelSizeN[0].value=x;
    ImagePixelSizeN[1].value=y;

    IDSetNumber(ImagePixelSizeNP, NULL);

    ImageFrameN[FRAME_W].max = x;
    ImageFrameN[FRAME_H].max = y;
    IUUpdateMinMax(ImageFrameNP);
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

    IDSetNumber(ImageFrameNP, NULL);
}

void CCDChip::setBin(int hor, int ver)
{
    BinX = hor;
    BinY = ver;

    ImageBinN[BIN_W].value = BinX;
    ImageBinN[BIN_H].value = BinY;

    IDSetNumber(ImageBinNP, NULL);
}

void CCDChip::setPixelSize(int x, int y)
{
    PixelSizex = x;
    PixelSizey = y;

    ImagePixelSizeN[2].value=x;
    ImagePixelSizeN[3].value=x;
    ImagePixelSizeN[4].value=y;

    IDSetNumber(ImagePixelSizeNP, NULL);

}

void CCDChip::setBPP(int bbp)
{
    BPP = bbp;

    ImagePixelSizeN[5].value = BPP;

    IDSetNumber(ImagePixelSizeNP, NULL);
}

void CCDChip::setFrameBufferSize(int nbuf)
{
    if (nbuf == RawFrameSize)
        return;

    if (RawFrame != NULL)
        delete RawFrame;


    RawFrameSize = nbuf;

    RawFrame = new char[nbuf];
}

void CCDChip::setExposure(double duration)
{
    ImageExposureN[0].value = duration;
    IDSetNumber(ImageExposureNP, NULL);
}

void CCDChip::setInterlaced(bool intr)
{
    Interlaced = intr;
}

void CCDChip::setExposureFailed()
{
    ImageExposureNP->s = IPS_ALERT;
    IDSetNumber(ImageExposureNP, NULL);
}

INDI::CCD::CCD()
{
    //ctor
    HasGuideHead=false;
    HasSt4Port=false;
    InExposure=false;
    ActiveDeviceTP = new ITextVectorProperty;
}

INDI::CCD::~CCD()
{
    delete ActiveDeviceTP;
}

bool INDI::CCD::initProperties()
{
    DefaultDevice::initProperties();   //  let the base class flesh in what it wants

    // PRIMARY CCD Init

    IUFillNumber(&PrimaryCCD.ImageFrameN[0],"X","Left ","%4.0f",0,1392.0,0,0);
    IUFillNumber(&PrimaryCCD.ImageFrameN[1],"Y","Top","%4.0f",0,1040,0,0);
    IUFillNumber(&PrimaryCCD.ImageFrameN[2],"WIDTH","Width","%4.0f",0,1392.0,0,1392.0);
    IUFillNumber(&PrimaryCCD.ImageFrameN[3],"HEIGHT","Height","%4.0f",0,1040,0,1040);
    IUFillNumberVector(PrimaryCCD.ImageFrameNP,PrimaryCCD.ImageFrameN,4,getDeviceName(),"CCD_FRAME","Frame",IMAGE_SETTINGS_TAB,IP_RW,60,IPS_IDLE);

    IUFillSwitch(&PrimaryCCD.FrameTypeS[0],"FRAME_LIGHT","Light",ISS_ON);
    IUFillSwitch(&PrimaryCCD.FrameTypeS[1],"FRAME_BIAS","Bias",ISS_OFF);
    IUFillSwitch(&PrimaryCCD.FrameTypeS[2],"FRAME_DARK","Dark",ISS_OFF);
    IUFillSwitch(&PrimaryCCD.FrameTypeS[3],"FRAME_FLAT","Flat",ISS_OFF);
    IUFillSwitchVector(PrimaryCCD.FrameTypeSP,PrimaryCCD.FrameTypeS,4,getDeviceName(),"CCD_FRAME_TYPE","FrameType",IMAGE_SETTINGS_TAB,IP_RW,ISR_1OFMANY,60,IPS_IDLE);

    IUFillNumber(&PrimaryCCD.ImageExposureN[0],"CCD_EXPOSURE_VALUE","Duration (s)","%5.2f",0,36000,0,1.0);
    IUFillNumberVector(PrimaryCCD.ImageExposureNP,PrimaryCCD.ImageExposureN,1,getDeviceName(),"CCD_EXPOSURE_REQUEST","Expose",MAIN_CONTROL_TAB,IP_RW,60,IPS_IDLE);

    IUFillNumber(&PrimaryCCD.ImageBinN[0],"HOR_BIN","X","%2.0f",1,4,1,1);
    IUFillNumber(&PrimaryCCD.ImageBinN[1],"VER_BIN","Y","%2.0f",1,4,1,1);
    IUFillNumberVector(PrimaryCCD.ImageBinNP,PrimaryCCD.ImageBinN,2,getDeviceName(),"CCD_BINNING","Binning",IMAGE_SETTINGS_TAB,IP_RW,60,IPS_IDLE);

    IUFillNumber(&PrimaryCCD.ImagePixelSizeN[0],"CCD_MAX_X","Resolution x","%4.0f",1,40,0,6.45);
    IUFillNumber(&PrimaryCCD.ImagePixelSizeN[1],"CCD_MAX_Y","Resolution y","%4.0f",1,40,0,6.45);
    IUFillNumber(&PrimaryCCD.ImagePixelSizeN[2],"CCD_PIXEL_SIZE","Pixel size (um)","%5.2f",1,40,0,6.45);
    IUFillNumber(&PrimaryCCD.ImagePixelSizeN[3],"CCD_PIXEL_SIZE_X","Pixel size X","%5.2f",1,40,0,6.45);
    IUFillNumber(&PrimaryCCD.ImagePixelSizeN[4],"CCD_PIXEL_SIZE_Y","Pixel size Y","%5.2f",1,40,0,6.45);
    IUFillNumber(&PrimaryCCD.ImagePixelSizeN[5],"CCD_BITSPERPIXEL","Bits per pixel","%3.0f",1,40,0,6.45);
    IUFillNumberVector(PrimaryCCD.ImagePixelSizeNP,PrimaryCCD.ImagePixelSizeN,6,getDeviceName(),"CCD_INFO","CCD Information",IMAGE_INFO_TAB,IP_RO,60,IPS_IDLE);

    IUFillSwitch(&PrimaryCCD.CompressS[0],"COMPRESS","Compress",ISS_OFF);
    IUFillSwitch(&PrimaryCCD.CompressS[1],"RAW","Raw",ISS_ON);
    IUFillSwitchVector(PrimaryCCD.CompressSP,PrimaryCCD.CompressS,2,getDeviceName(),"COMPRESSION","Image",IMAGE_SETTINGS_TAB,IP_RW,ISR_1OFMANY,60,IPS_IDLE);

    IUFillBLOB(&PrimaryCCD.FitsB,"CCD1","Image","");
    IUFillBLOBVector(PrimaryCCD.FitsBP,&PrimaryCCD.FitsB,1,getDeviceName(),"CCD1","Image Data",OPTIONS_TAB,IP_RO,60,IPS_IDLE);

    // GUIDER CCD Init

    IUFillNumber(&GuideCCD.ImageFrameN[0],"X","Left ","%4.0f",0,1392.0,0,0);
    IUFillNumber(&GuideCCD.ImageFrameN[1],"Y","Top","%4.0f",0,1040,0,0);
    IUFillNumber(&GuideCCD.ImageFrameN[2],"WIDTH","Width","%4.0f",0,1392.0,0,1392.0);
    IUFillNumber(&GuideCCD.ImageFrameN[3],"HEIGHT","Height","%4.0f",0,1040,0,1040);
    IUFillNumberVector(GuideCCD.ImageFrameNP,GuideCCD.ImageFrameN,4,getDeviceName(),"GUIDE_FRAME","Frame",GUIDE_HEAD_TAB,IP_RW,60,IPS_IDLE);

    IUFillNumber(&GuideCCD.ImagePixelSizeN[0],"CCD_MAX_X","Resolution x","%4.0f",1,40,0,6.45);
    IUFillNumber(&GuideCCD.ImagePixelSizeN[1],"CCD_MAX_Y","Resolution y","%4.0f",1,40,0,6.45);
    IUFillNumber(&GuideCCD.ImagePixelSizeN[2],"CCD_PIXEL_SIZE","Pixel size (um)","%5.2f",1,40,0,6.45);
    IUFillNumber(&GuideCCD.ImagePixelSizeN[3],"CCD_PIXEL_SIZE_X","Pixel size X","%5.2f",1,40,0,6.45);
    IUFillNumber(&GuideCCD.ImagePixelSizeN[4],"CCD_PIXEL_SIZE_Y","Pixel size Y","%5.2f",1,40,0,6.45);
    IUFillNumber(&GuideCCD.ImagePixelSizeN[5],"CCD_BITSPERPIXEL","Bits per pixel","%3.0f",1,40,0,6.45);
    IUFillNumberVector(GuideCCD.ImagePixelSizeNP,GuideCCD.ImagePixelSizeN,6,getDeviceName(),"GUIDE_INFO",GUIDE_HEAD_TAB,GUIDE_HEAD_TAB,IP_RO,60,IPS_IDLE);

    IUFillNumber(&GuideCCD.ImageExposureN[0],"GUIDER_EXPOSURE_VALUE","Duration (s)","%5.2f",0,36000,0,1.0);
    IUFillNumberVector(GuideCCD.ImageExposureNP,GuideCCD.ImageExposureN,1,getDeviceName(),"GUIDER_EXPOSURE_REQUEST","Guide",MAIN_CONTROL_TAB,IP_RW,60,IPS_IDLE);

    IUFillSwitch(&GuideCCD.CompressS[0],"GCOMPRESS","Compress",ISS_OFF);
    IUFillSwitch(&GuideCCD.CompressS[1],"GRAW","Raw",ISS_ON);
    IUFillSwitchVector(GuideCCD.CompressSP,GuideCCD.CompressS,2,getDeviceName(),"GCOMPRESSION","Image",GUIDE_HEAD_TAB,IP_RW,ISR_1OFMANY,60,IPS_IDLE);

    IUFillBLOB(&GuideCCD.FitsB,"CCD2","Guider Image","");
    IUFillBLOBVector(GuideCCD.FitsBP,&GuideCCD.FitsB,1,getDeviceName(),"CCD2","Image Data",OPTIONS_TAB,IP_RO,60,IPS_IDLE);

    // CCD Class Init

    IUFillText(&ActiveDeviceT[0],"ACTIVE_TELESCOPE","Telescope","Telescope Simulator");
    IUFillText(&ActiveDeviceT[1],"ACTIVE_FOCUSER","Focuser","Focuser Simulator");
    IUFillTextVector(ActiveDeviceTP,ActiveDeviceT,2,getDeviceName(),"ACTIVE_DEVICES","Snoop devices",OPTIONS_TAB,IP_RW,60,IPS_IDLE);

    IUFillNumber(&EqN[0],"RA","Ra (hh:mm:ss)","%010.6m",0,24,0,0);
    IUFillNumber(&EqN[1],"DEC","Dec (dd:mm:ss)","%010.6m",-90,90,0,0);
    IUFillNumberVector(&EqNP,EqN,2,ActiveDeviceT[0].text,"EQUATORIAL_COORD","EQ Coord","Main Control",IP_RW,60,IPS_IDLE);

    IDSnoopDevice(ActiveDeviceT[0].text,"EQUATORIAL_COORD");
    IDSnoopDevice(ActiveDeviceT[0].text,"TELESCOPE_INFO");
    IDSnoopDevice(ActiveDeviceT[1].text,"FWHM");


    // Guider Interface
    initGuiderProperties(getDeviceName(), GUIDE_CONTROL_TAB);

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
       defineNumber(PrimaryCCD.ImageExposureNP);
        defineNumber(PrimaryCCD.ImageFrameNP);
        defineNumber(PrimaryCCD.ImageBinNP);

        if(HasGuideHead)
        {
           // IDLog("Sending Guider Stuff\n");
            defineNumber(GuideCCD.ImageExposureNP);
            defineNumber(GuideCCD.ImageFrameNP);
        }

        defineNumber(PrimaryCCD.ImagePixelSizeNP);
        if(HasGuideHead)
        {
            defineNumber(GuideCCD.ImagePixelSizeNP);
        }
        defineSwitch(PrimaryCCD.CompressSP);
        defineBLOB(PrimaryCCD.FitsBP);
        if(HasGuideHead)
        {
            defineSwitch(GuideCCD.CompressSP);
            defineBLOB(GuideCCD.FitsBP);
        }
        if(HasSt4Port)
        {
            defineNumber(&GuideNSP);
            defineNumber(&GuideEWP);
        }
        defineSwitch(PrimaryCCD.FrameTypeSP);
        defineText(ActiveDeviceTP);
    }
    else
    {
        deleteProperty(PrimaryCCD.ImageFrameNP->name);
        deleteProperty(PrimaryCCD.ImageBinNP->name);
        deleteProperty(PrimaryCCD.ImagePixelSizeNP->name);
        deleteProperty(PrimaryCCD.ImageExposureNP->name);
        deleteProperty(PrimaryCCD.FitsBP->name);
        deleteProperty(PrimaryCCD.CompressSP->name);
        if(HasGuideHead)
        {
            deleteProperty(GuideCCD.ImageExposureNP->name);
            deleteProperty(GuideCCD.ImageFrameNP->name);
            deleteProperty(GuideCCD.ImagePixelSizeNP->name);
            deleteProperty(GuideCCD.FitsBP->name);
            deleteProperty(GuideCCD.CompressSP->name);
        }
        if(HasSt4Port)
        {
            deleteProperty(GuideNSP.name);
            deleteProperty(GuideEWP.name);

        }
        deleteProperty(PrimaryCCD.FrameTypeSP->name);
        deleteProperty(ActiveDeviceTP->name);
    }
    return true;
}

bool INDI::CCD::ISSnoopDevice (XMLEle *root)
 {
     //fprintf(stderr," ################# CCDSim handling snoop ##############\n");
     if(IUSnoopNumber(root,&EqNP)==0)
     {
        float newra,newdec;
        newra=EqN[0].value;
        newdec=EqN[1].value;
        if((newra != RA)||(newdec != Dec))
        {
            //fprintf(stderr,"RA %4.2f  Dec %4.2f Snooped RA %4.2f  Dec %4.2f\n",RA,Dec,newra,newdec);
            RA=newra;
            Dec=newdec;

        }
     }
     else
     {
        //fprintf(stderr,"Snoop Failed\n");
     }

     return true;
 }

bool INDI::CCD::ISNewText (const char *dev, const char *name, char *texts[], char *names[], int n)
{
    //  Ok, lets see if this is a property wer process
    //IDLog("IndiTelescope got %d new text items name %s\n",n,name);
    //  first check if it's for our device
    if(strcmp(dev,getDeviceName())==0)
    {
        //  This is for our device
        //  Now lets see if it's something we process here
        if(strcmp(name,ActiveDeviceTP->name)==0)
        {
            int rc;
            //IDLog("calling update text\n");
            ActiveDeviceTP->s=IPS_OK;
            rc=IUUpdateText(ActiveDeviceTP,texts,names,n);
            //IDLog("update text returns %d\n",rc);
            //  Update client display
            IDSetText(ActiveDeviceTP,NULL);
            saveConfig();
            IUFillNumberVector(&EqNP,EqN,2,ActiveDeviceT[0].text,"EQUATORIAL_PEC","EQ PEC",MAIN_CONTROL_TAB,IP_RW,60,IPS_IDLE);

            IDSnoopDevice(ActiveDeviceT[0].text,"EQUATORIAL_PEC");
            IDSnoopDevice(ActiveDeviceT[0].text,"TELESCOPE_INFO");
            IDSnoopDevice(ActiveDeviceT[1].text,"FWHM");
            //  We processed this one, so, tell the world we did it
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
        if(strcmp(name,"CCD_EXPOSURE_REQUEST")==0)
        {
            float n;
            int rc;

            n=values[0];

            PrimaryCCD.ImageExposureN[0].value=n;

            if (PrimaryCCD.ImageExposureNP->s==IPS_BUSY)
                AbortExposure();

            PrimaryCCD.ImageExposureNP->s=IPS_BUSY;
            //  now we have a new number, this is our requested exposure time
            //  Tell the clients we are busy with this exposure

            //  And now call the physical hardware layer to start the exposure
            rc=StartExposure(n);
            switch(rc)
            {
                case 0: //  normal case, exposure running on timers, callbacks when it's finished
                    PrimaryCCD.ImageExposureNP->s=IPS_BUSY;
                    break;
                case 1: //  Short exposure, it's already done
                    PrimaryCCD.ImageExposureNP->s=IPS_OK;
                    break;
                case -1:    //  error condition
                    PrimaryCCD.ImageExposureNP->s=IPS_ALERT;
                break;
            }
            IDSetNumber(PrimaryCCD.ImageExposureNP,NULL);
            return true;
        }

        if(strcmp(name,"GUIDER_EXPOSURE_REQUEST")==0)
        {
            float n;
            int rc;

            n=values[0];

            GuideCCD.ImageExposureN[0].value=n;
            GuideCCD.ImageExposureNP->s=IPS_BUSY;
            //  now we have a new number, this is our requested exposure time
            //  Tell the clients we are busy with this exposure

            //  And now call the physical hardware layer to start the exposure
            //  change of plans, this is just changing exposure time
            //  the start/stop stream buttons do the rest
            rc=StartGuideExposure(n);
            //rc=1;   //  set it to ok
            switch(rc) {
                case 0: //  normal case, exposure running on timers, callbacks when it's finished
                    GuideCCD.ImageExposureNP->s=IPS_BUSY;
                    break;
                case 1: //  Short exposure, it's already done
                    GuideCCD.ImageExposureNP->s=IPS_OK;
                    break;
                case -1:    //  error condition
                    GuideCCD.ImageExposureNP->s=IPS_ALERT;
                break;
            }
            IDSetNumber(GuideCCD.ImageExposureNP,NULL);
            return true;
        }

        if(strcmp(name,"CCD_BINNING")==0)
        {
            //  We are being asked to set camera binning
            PrimaryCCD.ImageBinNP->s=IPS_OK;
            IUUpdateNumber(PrimaryCCD.ImageBinNP,values,names,n);


            if (updateCCDBin(PrimaryCCD.ImageBinN[0].value, PrimaryCCD.ImageBinN[1].value) == false)
            {
                PrimaryCCD.ImageBinNP->s = IPS_ALERT;
                IDSetNumber (PrimaryCCD.ImageBinNP, NULL);
            }

            return true;

      }

        if(strcmp(name,"CCD_FRAME")==0)
        {
            //  We are being asked to set camera binning
            PrimaryCCD.ImageFrameNP->s=IPS_OK;
            IUUpdateNumber(PrimaryCCD.ImageFrameNP,values,names,n);

            if (updateCCDFrame(PrimaryCCD.ImageFrameN[0].value, PrimaryCCD.ImageFrameN[1].value, PrimaryCCD.ImageFrameN[2].value,
                               PrimaryCCD.ImageFrameN[3].value) == false)
            {
                PrimaryCCD.ImageFrameNP->s = IPS_ALERT;
                IDSetNumber(PrimaryCCD.ImageFrameNP, NULL);

            }
            return true;
        }

        if(strcmp(name,"GUIDER_FRAME")==0)
        {
            //  We are being asked to set camera binning
            GuideCCD.ImageFrameNP->s=IPS_OK;
            IUUpdateNumber(GuideCCD.ImageFrameNP,values,names,n);
            //  Update client display
            //IDSetNumber(GuiderFrameNP,NULL);

            if (isDebug())
                IDLog("GuiderFrame set to %4.0f,%4.0f %4.0f x %4.0f\n",
                  GuideCCD.ImageFrameN[0].value,GuideCCD.ImageFrameN[1].value,GuideCCD.ImageFrameN[2].value,GuideCCD.ImageFrameN[3].value);
            //GSubX=GuiderFrameN[0].value;
            //GSubY=GuiderFrameN[1].value;
            //GSubW=GuiderFrameN[2].value;
            //GSubH=GuiderFrameN[3].value;
            GuideCCD.setFrame(GuideCCD.ImageFrameN[0].value, GuideCCD.ImageFrameN[1].value,
                              GuideCCD.ImageFrameN[2].value,GuideCCD.ImageFrameN[3].value);

            return true;
        }

        if (!strcmp(name,GuideNSP.name) || !strcmp(name,GuideEWP.name))
        {
            processGuiderProperties(name, values, names, n);
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


        if(strcmp(name,PrimaryCCD.CompressSP->name)==0)
        {

            IUUpdateSwitch(PrimaryCCD.CompressSP,states,names,n);
            IDSetSwitch(PrimaryCCD.CompressSP,NULL);

            if(PrimaryCCD.CompressS[0].s==ISS_ON    )
            {
                PrimaryCCD.SendCompressed=true;
            } else
            {
                PrimaryCCD.SendCompressed=false;
            }
            return true;
        }

        if(strcmp(name,GuideCCD.CompressSP->name)==0)
        {

            IUUpdateSwitch(GuideCCD.CompressSP,states,names,n);
            IDSetSwitch(GuideCCD.CompressSP,NULL);

            if(GuideCCD.CompressS[0].s==ISS_ON    )
            {
                GuideCCD.SendCompressed=true;
            } else
            {
                GuideCCD.SendCompressed=false;
            }
            return true;
        }

        if(strcmp(name,PrimaryCCD.FrameTypeSP->name)==0)
        {
            //  Compression Update
            IUUpdateSwitch(PrimaryCCD.FrameTypeSP,states,names,n);
            PrimaryCCD.FrameTypeSP->s=IPS_OK;
            if(PrimaryCCD.FrameTypeS[0].s==ISS_ON) PrimaryCCD.setFrameType(CCDChip::LIGHT_FRAME);
            if(PrimaryCCD.FrameTypeS[1].s==ISS_ON) PrimaryCCD.setFrameType(CCDChip::BIAS_FRAME);
            if(PrimaryCCD.FrameTypeS[2].s==ISS_ON) PrimaryCCD.setFrameType(CCDChip::DARK_FRAME);
            if(PrimaryCCD.FrameTypeS[3].s==ISS_ON) PrimaryCCD.setFrameType(CCDChip::FLAT_FRAME);

            IDSetSwitch(PrimaryCCD.FrameTypeSP,NULL);
            return true;
        }
    }


    // let the default driver have a crack at it
    return DefaultDevice::ISNewSwitch(dev, name, states, names, n);
}


int INDI::CCD::StartExposure(float duration)
{
    IDLog("INDI::CCD::StartExposure %4.2f -  Should never get here\n",duration);
    return -1;
}

int INDI::CCD::StartGuideExposure(float duration)
{
    IDLog("INDI::CCD::StartGuide Exposure %4.2f -  Should never get here\n",duration);
    return -1;
}

bool INDI::CCD::AbortExposure()
{
    IDLog("INDI::CCD::AbortExposure -  Should never get here\n");
    return false;
}

bool INDI::CCD::updateCCDFrame(int x, int y, int w, int h)
{
    // Just set value, unless HW layer overrides this and performs its own processing
    PrimaryCCD.setFrame(x, y, w, h);
    return true;
}

bool INDI::CCD::updateCCDBin(int hor, int ver)
{
    // Just set value, unless HW layer overrides this and performs its own processing
    PrimaryCCD.setBin(hor, ver);
    return true;
}

void INDI::CCD::addFITSKeywords(fitsfile *fptr)
{
    // Nothing to do here. HW layer should take care of it if needed.
}

bool INDI::CCD::ExposureComplete(CCDChip *targetChip)
{
    void *memptr;
    size_t memsize;
    int img_type=0;
    int byte_type=0;
    int status=0;
    long naxes[2];
    long naxis=2;
    int numbytes=0;

    fitsfile *fptr=NULL;

    //IDLog("Enter Exposure Complete %d %d %d %d\n",SubW,SubH,BinX,BinY);

    naxes[0]=targetChip->getSubW()/targetChip->getBinX();
    naxes[1]=targetChip->getSubH()/targetChip->getBinY();

    switch (targetChip->getBPP())
    {
        case 8:
            byte_type = TBYTE;
            img_type  = BYTE_IMG;
            break;

        case 16:
            byte_type = TUSHORT;
            img_type = USHORT_IMG;
            break;

        case 32:
            byte_type = TULONG;
            img_type = ULONG_IMG;
            break;

         default:
            IDLog("Unsupported bits per pixel value %d\n", targetChip->getBPP() );
            return false;
            break;
    }


    numbytes = naxes[0] * naxes[1];

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

    addFITSKeywords(fptr);

    fits_write_img(fptr,byte_type,1,numbytes,targetChip->getFrameBuffer(),&status);

    if (status)
    {
		IDLog("Error: Failed to write FITS image\n");
		fits_report_error(stderr, status);  /* print out any error messages */
		return false;
    }

    fits_close_file(fptr,&status);

    targetChip->ImageExposureNP->s=IPS_OK;
    IDSetNumber(targetChip->ImageExposureNP,NULL);


    targetChip->FitsB.blob=memptr;
    targetChip->FitsB.bloblen=memsize;
    targetChip->FitsB.size=memsize;
    strcpy(targetChip->FitsB.format,".fits");
    targetChip->FitsBP->s=IPS_OK;
    //IDLog("Enter Uploadfile with %d total sending via %s, and format %s\n",total,targetChip->FitsB.name, targetChip->FitsB.format);
    IDSetBLOB(targetChip->FitsBP,NULL);

    free(memptr);
    return true;
}

void INDI::CCD::SetCCDParams(int x,int y,int bpp,float xf,float yf)
{
    PrimaryCCD.setResolutoin(x, y);
    PrimaryCCD.setFrame(0, 0, x, y);
    PrimaryCCD.setBin(1,1);
    PrimaryCCD.setPixelSize(xf, yf);
    PrimaryCCD.setBPP(bpp);

}

void INDI::CCD::SetGuidHeadParams(int x,int y,int bpp,float xf,float yf)
{
    HasGuideHead=true;

    GuideCCD.setResolutoin(x, y);
    GuideCCD.setFrame(0, 0, x, y);
    GuideCCD.setPixelSize(xf, yf);
    GuideCCD.setBPP(bpp);

}

bool INDI::CCD::AbortGuideExposure()
{
    return false;
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

