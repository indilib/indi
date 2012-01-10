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
}

void CCDChip::setFrame(int subx, int suby, int subw, int subh)
{
    SubX = subx;
    SubY = suby;
    SubW = subw;
    SubH = subh;

    ImageFrameN[0].value = SubX;
    ImageFrameN[1].value = SubY;
    ImageFrameN[2].value = SubW;
    ImageFrameN[3].value = SubH;

    IDSetNumber(ImageFrameNP, NULL);
}

void CCDChip::setBin(int hor, int ver)
{
    BinX = hor;
    BinY = ver;

    ImageBinN[0].value = BinX;
    ImageBinN[1].value = BinY;

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
    TelescopeTP = new ITextVectorProperty;
}

INDI::CCD::~CCD()
{
    delete TelescopeTP;
}

bool INDI::CCD::initProperties()
{
    DefaultDriver::initProperties();   //  let the base class flesh in what it wants

    // PRIMARY CCD Init

    IUFillNumber(&PrimaryCCD.ImageFrameN[0],"X","Left ","%4.0f",0,1392.0,0,0);
    IUFillNumber(&PrimaryCCD.ImageFrameN[1],"Y","Top","%4.0f",0,1040,0,0);
    IUFillNumber(&PrimaryCCD.ImageFrameN[2],"WIDTH","Width","%4.0f",0,1392.0,0,1392.0);
    IUFillNumber(&PrimaryCCD.ImageFrameN[3],"HEIGHT","Height","%4.0f",0,1040,0,1040);
    IUFillNumberVector(PrimaryCCD.ImageFrameNP,PrimaryCCD.ImageFrameN,4,deviceName(),"CCD_FRAME","Frame",IMAGE_SETTINGS_TAB,IP_RW,60,IPS_IDLE);

    IUFillSwitch(&PrimaryCCD.FrameTypeS[0],"FRAME_LIGHT","Light",ISS_ON);
    IUFillSwitch(&PrimaryCCD.FrameTypeS[1],"FRAME_BIAS","Bias",ISS_OFF);
    IUFillSwitch(&PrimaryCCD.FrameTypeS[2],"FRAME_DARK","Dark",ISS_OFF);
    IUFillSwitch(&PrimaryCCD.FrameTypeS[3],"FRAME_FLAT","Flat",ISS_OFF);
    IUFillSwitchVector(PrimaryCCD.FrameTypeSP,PrimaryCCD.FrameTypeS,4,deviceName(),"CCD_FRAME_TYPE","FrameType",IMAGE_SETTINGS_TAB,IP_RW,ISR_1OFMANY,60,IPS_IDLE);

    IUFillNumber(&PrimaryCCD.ImageExposureN[0],"CCD_EXPOSURE_VALUE","Duration (s)","%5.2f",0,36000,0,1.0);
    IUFillNumberVector(PrimaryCCD.ImageExposureNP,PrimaryCCD.ImageExposureN,1,deviceName(),"CCD_EXPOSURE_REQUEST","Expose",MAIN_CONTROL_TAB,IP_RW,60,IPS_IDLE);

    IUFillNumber(&PrimaryCCD.ImageBinN[0],"HOR_BIN","X","%2.0f",1,4,1,1);
    IUFillNumber(&PrimaryCCD.ImageBinN[1],"VER_BIN","Y","%2.0f",1,4,1,1);
    IUFillNumberVector(PrimaryCCD.ImageBinNP,PrimaryCCD.ImageBinN,2,deviceName(),"CCD_BINNING","Binning",IMAGE_SETTINGS_TAB,IP_RW,60,IPS_IDLE);

    IUFillNumber(&PrimaryCCD.ImagePixelSizeN[0],"CCD_MAX_X","Resolution x","%4.0f",1,40,0,6.45);
    IUFillNumber(&PrimaryCCD.ImagePixelSizeN[1],"CCD_MAX_Y","Resolution y","%4.0f",1,40,0,6.45);
    IUFillNumber(&PrimaryCCD.ImagePixelSizeN[2],"CCD_PIXEL_SIZE","Pixel size (um)","%5.2f",1,40,0,6.45);
    IUFillNumber(&PrimaryCCD.ImagePixelSizeN[3],"CCD_PIXEL_SIZE_X","Pixel size X","%5.2f",1,40,0,6.45);
    IUFillNumber(&PrimaryCCD.ImagePixelSizeN[4],"CCD_PIXEL_SIZE_Y","Pixel size Y","%5.2f",1,40,0,6.45);
    IUFillNumber(&PrimaryCCD.ImagePixelSizeN[5],"CCD_BITSPERPIXEL","Bits per pixel","%3.0f",1,40,0,6.45);
    IUFillNumberVector(PrimaryCCD.ImagePixelSizeNP,PrimaryCCD.ImagePixelSizeN,6,deviceName(),"CCD_INFO","Ccd Information",IMAGE_INFO_TAB,IP_RO,60,IPS_IDLE);

    IUFillSwitch(&PrimaryCCD.CompressS[0],"COMPRESS","Compress",ISS_OFF);
    IUFillSwitch(&PrimaryCCD.CompressS[1],"RAW","Raw",ISS_ON);
    IUFillSwitchVector(PrimaryCCD.CompressSP,PrimaryCCD.CompressS,2,deviceName(),"COMPRESSION","Image",IMAGE_SETTINGS_TAB,IP_RW,ISR_1OFMANY,60,IPS_IDLE);

    IUFillBLOB(&PrimaryCCD.FitsB,"CCD1","Image","");
    IUFillBLOBVector(PrimaryCCD.FitsBP,&PrimaryCCD.FitsB,1,deviceName(),"CCD1","Image Data",OPTIONS_TAB,IP_RO,60,IPS_IDLE);

    // GUIDER CCD Init

    IUFillNumber(&GuideCCD.ImageFrameN[0],"X","Left ","%4.0f",0,1392.0,0,0);
    IUFillNumber(&GuideCCD.ImageFrameN[1],"Y","Top","%4.0f",0,1040,0,0);
    IUFillNumber(&GuideCCD.ImageFrameN[2],"WIDTH","Width","%4.0f",0,1392.0,0,1392.0);
    IUFillNumber(&GuideCCD.ImageFrameN[3],"HEIGHT","Height","%4.0f",0,1040,0,1040);
    IUFillNumberVector(GuideCCD.ImageFrameNP,GuideCCD.ImageFrameN,4,deviceName(),"GUIDE_FRAME","Frame",GUIDE_HEAD_TAB,IP_RW,60,IPS_IDLE);

    IUFillNumber(&GuideCCD.ImagePixelSizeN[0],"Image_MAX_X","Resolution x","%4.0f",1,40,0,6.45);
    IUFillNumber(&GuideCCD.ImagePixelSizeN[1],"Image_MAX_Y","Resolution y","%4.0f",1,40,0,6.45);
    IUFillNumber(&GuideCCD.ImagePixelSizeN[2],"Image_PIXEL_SIZE","Pixel size (um)","%5.2f",1,40,0,6.45);
    IUFillNumber(&GuideCCD.ImagePixelSizeN[3],"Image_PIXEL_SIZE_X","Pixel size X","%5.2f",1,40,0,6.45);
    IUFillNumber(&GuideCCD.ImagePixelSizeN[4],"Image_PIXEL_SIZE_Y","Pixel size Y","%5.2f",1,40,0,6.45);
    IUFillNumber(&GuideCCD.ImagePixelSizeN[5],"Image_BITSPERPIXEL","Bits per pixel","%3.0f",1,40,0,6.45);
    IUFillNumberVector(GuideCCD.ImagePixelSizeNP,GuideCCD.ImagePixelSizeN,6,deviceName(),"GUIDE_INFO",GUIDE_HEAD_TAB,GUIDE_HEAD_TAB,IP_RO,60,IPS_IDLE);

    IUFillNumber(&GuideCCD.ImageExposureN[0],"GUIDER_EXPOSURE_VALUE","Duration (s)","%5.2f",0,36000,0,1.0);
    IUFillNumberVector(GuideCCD.ImageExposureNP,GuideCCD.ImageExposureN,1,deviceName(),"GUIDER_EXPOSURE","Image",MAIN_CONTROL_TAB,IP_RW,60,IPS_IDLE);

    IUFillSwitch(&GuideCCD.CompressS[0],"GCOMPRESS","Compress",ISS_OFF);
    IUFillSwitch(&GuideCCD.CompressS[1],"GRAW","Raw",ISS_ON);
    IUFillSwitchVector(GuideCCD.CompressSP,GuideCCD.CompressS,2,deviceName(),"GCOMPRESSION","Image",GUIDE_HEAD_TAB,IP_RW,ISR_1OFMANY,60,IPS_IDLE);

    IUFillBLOB(&GuideCCD.FitsB,"CCD2","Guider Image","");
    IUFillBLOBVector(GuideCCD.FitsBP,&GuideCCD.FitsB,1,deviceName(),"CCD2","Image Data",OPTIONS_TAB,IP_RO,60,IPS_IDLE);

    // CCD Class Init

    IUFillText(&TelescopeT[0],"ACTIVE_TELESCOPE","Telescope","");
    IUFillTextVector(TelescopeTP,TelescopeT,1,deviceName(),"ACTIVE_DEVICES","Snoop Scope",OPTIONS_TAB,IP_RW,60,IPS_IDLE);

    IUFillNumber(&EqN[0],"RA_PEC","Ra (hh:mm:ss)","%010.6m",0,24,0,0);
    IUFillNumber(&EqN[1],"DEC_PEC","Dec (dd:mm:ss)","%010.6m",-90,90,0,0);
    IUFillNumberVector(&EqNP,EqN,2,"","EQUATORIAL_PEC","EQ PEC","Main Control",IP_RW,60,IPS_IDLE);

    // Guider Interface
    initGuiderProperties(deviceName(), GUIDE_CONTROL_TAB);

    return true;
}

void INDI::CCD::ISGetProperties (const char *dev)
{
    //  First we let our parent populate

    //IDLog("INDI::CCD IsGetProperties with %s\n",dev);

    DefaultDriver::ISGetProperties(dev);

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
            IDLog("Sending Guider Stuff\n");
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
            defineNumber(GuideNSP);
            defineNumber(GuideEWP);
        }
        defineSwitch(PrimaryCCD.FrameTypeSP);
        defineText(TelescopeTP);
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
            deleteProperty(GuideNSP->name);
            deleteProperty(GuideEWP->name);

        }
        deleteProperty(PrimaryCCD.FrameTypeSP->name);
        deleteProperty(TelescopeTP->name);
    }
    return true;
}

void INDI::CCD::ISSnoopDevice (XMLEle *root)
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
 }

bool INDI::CCD::ISNewText (const char *dev, const char *name, char *texts[], char *names[], int n)
{
    //  Ok, lets see if this is a property wer process
    //IDLog("IndiTelescope got %d new text items name %s\n",n,name);
    //  first check if it's for our device
    if(strcmp(dev,deviceName())==0)
    {
        //  This is for our device
        //  Now lets see if it's something we process here
        if(strcmp(name,TelescopeTP->name)==0)
        {
            int rc;
            //IDLog("calling update text\n");
            TelescopeTP->s=IPS_OK;
            rc=IUUpdateText(TelescopeTP,texts,names,n);
            //IDLog("update text returns %d\n",rc);
            //  Update client display
            IDSetText(TelescopeTP,NULL);
            saveConfig();
            IUFillNumberVector(&EqNP,EqN,2,TelescopeT[0].text,"EQUATORIAL_PEC","EQ PEC",MAIN_CONTROL_TAB,IP_RW,60,IPS_IDLE);
            IDSnoopDevice(TelescopeT[0].text,"EQUATORIAL_PEC");
            //  We processed this one, so, tell the world we did it
            return true;
        }

    }

    return INDI::DefaultDriver::ISNewText(dev,name,texts,names,n);
}

bool INDI::CCD::ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n)
{
    //  first check if it's for our device
    //IDLog("INDI::CCD::ISNewNumber %s\n",name);
    if(strcmp(dev,deviceName())==0)
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

        if(strcmp(name,"GUIDER_EXPOSURE")==0)
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
            //  the the start/stop stream buttons do the rest
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

            // Let child classes know of CCD BIN changes
            // If child processes and updates bin (true) then no further action is needed
            // if child class does not do any processing, then we simply update the values.
            if (updateCCDBin(PrimaryCCD.ImageBinN[0].value, PrimaryCCD.ImageBinN[1].value) == false)
                PrimaryCCD.setBin(PrimaryCCD.ImageBinN[0].value, PrimaryCCD.ImageBinN[1].value);

            return true;

        }

        if(strcmp(name,"CCD_FRAME")==0)
        {
            //  We are being asked to set camera binning
            PrimaryCCD.ImageFrameNP->s=IPS_OK;
            IUUpdateNumber(PrimaryCCD.ImageFrameNP,values,names,n);
            //  Update client display
            //IDSetNumber(PrimaryCCD.ImageFrameNP,NULL);

            //IDLog("Frame set to %4.0f,%4.0f %4.0f x %4.0f\n",CcdFrameN[0].value,CcdFrameN[1].value,CcdFrameN[2].value,CcdFrameN[3].value);
            //SubX=PrimaryCCD.ImageFrameN[0].value;
            //SubY=PrimaryCCD.ImageFrameN[1].value;
            //SubW=PrimaryCCD.ImageFrameN[2].value;
            //SubH=PrimaryCCD.ImageFrameN[3].value;

            if (updateCCDFrame(PrimaryCCD.ImageFrameN[0].value, PrimaryCCD.ImageFrameN[1].value, PrimaryCCD.ImageFrameN[2].value,
                               PrimaryCCD.ImageFrameN[3].value) == false)
                PrimaryCCD.setFrame(PrimaryCCD.ImageFrameN[0].value, PrimaryCCD.ImageFrameN[1].value, PrimaryCCD.ImageFrameN[2].value,
                                    PrimaryCCD.ImageFrameN[3].value);
            return true;
        }

        if(strcmp(name,"GUIDER_FRAME")==0)
        {
            //  We are being asked to set camera binning
            GuideCCD.ImageFrameNP->s=IPS_OK;
            IUUpdateNumber(GuideCCD.ImageFrameNP,values,names,n);
            //  Update client display
            //IDSetNumber(GuiderFrameNP,NULL);

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

        if(strcmp(name,GuideNSP->name)==0)
        {
            //  We are being asked to send a guide pulse north/south on the st4 port
            GuideNSP->s=IPS_BUSY;
            IUUpdateNumber(GuideNSP,values,names,n);
            //  Update client display
            IDSetNumber(GuideNSP,NULL);

            fprintf(stderr,"GuideNorthSouth set to %7.3f,%7.3f\n", GuideNS[0].value,GuideNS[1].value);

            if(GuideNS[0].value != 0)
            {
                GuideNorth(GuideNS[0].value);
            }
            if(GuideNS[1].value != 0) {
                GuideSouth(GuideNS[1].value);
            }
            GuideNS[0].value=0;
            GuideNS[1].value=0;
            GuideNSP->s=IPS_OK;
            IDSetNumber(GuideNSP,NULL);

            return true;
        }

        if(strcmp(name,GuideEWP->name)==0)
        {
            //  We are being asked to send a guide pulse north/south on the st4 port
            GuideEWP->s=IPS_BUSY;
            IUUpdateNumber(GuideEWP,values,names,n);
            //  Update client display
            IDSetNumber(GuideEWP,NULL);

            fprintf(stderr,"GuiderEastWest set to %6.3f,%6.3f\n",
                  GuideEW[0].value,GuideEW[1].value);

            if(GuideEW[0].value != 0)
            {
                GuideEast(GuideEW[0].value);
            } else
            {
                GuideWest(GuideEW[1].value);
            }

            GuideEW[0].value=0;
            GuideEW[1].value=0;
            GuideEWP->s=IPS_OK;
            IDSetNumber(GuideEWP,NULL);

            return true;
        }

    }
    //  if we didn't process it, continue up the chain, let somebody else
    //  give it a shot
    return DefaultDriver::ISNewNumber(dev,name,values,names,n);
}

bool INDI::CCD::ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n)
{

    if(strcmp(dev,deviceName())==0)
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
    return DefaultDriver::ISNewSwitch(dev, name, states, names, n);
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
    // Nothing to do here. HW layer should take care of it if needed.
    return false;
}

bool INDI::CCD::updateCCDBin(int hor, int ver)
{
    // Nothing to do here. HW layer should take care of it if needed.
    return false;
}

void INDI::CCD::addFITSKeywords(fitsfile *fptr)
{
    // Nothing to do here. HW layer should take care of it if needed.
}

bool INDI::CCD::ExposureComplete()
{
    void *memptr;
    size_t memsize;
    int status=0;
    long naxes[2];
    long naxis=2;
    int numbytes=0;

    fitsfile *fptr=NULL;

    //IDLog("Enter Exposure Complete %d %d %d %d\n",SubW,SubH,BinX,BinY);

    naxes[0]=PrimaryCCD.getSubW()/PrimaryCCD.getBinX();
    naxes[1]=PrimaryCCD.getSubH()/PrimaryCCD.getBinY();

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

    fits_create_img(fptr, USHORT_IMG , naxis, naxes, &status);

    if (status)
    {
		IDLog("Error: Failed to create FITS image\n");
		fits_report_error(stderr, status);  /* print out any error messages */
		return false;
    }

    addFITSKeywords(fptr);

    fits_write_img(fptr,TUSHORT,1,numbytes,PrimaryCCD.getFrameBuffer(),&status);

    if (status)
    {
		IDLog("Error: Failed to write FITS image\n");
		fits_report_error(stderr, status);  /* print out any error messages */
		return false;
    }

    fits_close_file(fptr,&status);


    PrimaryCCD.ImageExposureNP->s=IPS_OK;
    IDSetNumber(PrimaryCCD.ImageExposureNP,NULL);

    uploadfile(memptr,memsize);
    free(memptr);
    return true;
}

bool INDI::CCD::GuideExposureComplete()
{
        void *memptr;
        size_t memsize;
        int status=0;
        long naxes[2];
        long naxis=2;

        fitsfile *fptr=NULL;
        naxes[0]=GuideCCD.getSubW();
        naxes[1]=GuideCCD.getSubH();
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
        fits_create_img(fptr, BYTE_IMG , naxis, naxes, &status);
        if (status)
        {
            IDLog("Error: Failed to create FITS image\n");
            fits_report_error(stderr, status);  /* print out any error messages */
            return false;
        }

        fits_write_img(fptr,TBYTE,1,naxes[0]*naxes[1],GuideCCD.getFrameBuffer(),&status);
        if (status)
        {
            IDLog("Error: Failed to write FITS image\n");
            fits_report_error(stderr, status);  /* print out any error messages */
            return false;
        }
        fits_close_file(fptr,&status);
        IDLog("Built the Guider fits file\n");
        GuideCCD.ImageExposureNP->s=IPS_OK;
        IDSetNumber(GuideCCD.ImageExposureNP,NULL);

        IDLog("Sending guider fits file via %s\n",GuideCCD.FitsBP->name);
        GuideCCD.FitsB.blob=memptr;
        GuideCCD.FitsB.bloblen=memsize;
        GuideCCD.FitsB.size=memsize;
        strcpy(GuideCCD.FitsB.format,".fits");
        GuideCCD.FitsBP->s=IPS_OK;
        IDSetBLOB(GuideCCD.FitsBP,NULL);
        free(memptr);

    return true;
}


int INDI::CCD::uploadfile(void *fitsdata,int total)
{
    //  lets try sending a ccd preview
    //IDLog("Enter Uploadfile with %d total sending via %s\n",total,FitsBP->name);
    PrimaryCCD.FitsB.blob=fitsdata;
    PrimaryCCD.FitsB.bloblen=total;
    PrimaryCCD.FitsB.size=total;
    strcpy(PrimaryCCD.FitsB.format,".fits");
    PrimaryCCD.FitsBP->s=IPS_OK;
    IDSetBLOB(PrimaryCCD.FitsBP,NULL);

    return 0;
}

int INDI::CCD::SetCCDParams(int x,int y,int bpp,float xf,float yf)
{
    PrimaryCCD.setResolutoin(x, y);
    PrimaryCCD.setFrame(0, 0, x, y);
    PrimaryCCD.setBin(1,1);
    PrimaryCCD.setPixelSize(xf, yf);
    PrimaryCCD.setBPP(bpp/8);

    return 0;
}

int INDI::CCD::SetGuidHeadParams(int x,int y,int bpp,float xf,float yf)
{
    HasGuideHead=true;

    GuideCCD.setResolutoin(x, y);
    GuideCCD.setFrame(0, 0, x, y);
    GuideCCD.setBin(1,1);
    GuideCCD.setPixelSize(xf, yf);
    GuideCCD.setBPP(bpp/8);

    return 0;
}

bool INDI::CCD::AbortGuideExposure()
{
    return false;
}



int INDI::CCD::GuideNorth(float)
{
    return -1;
}
int INDI::CCD::GuideSouth(float)
{
    return -1;
}
int INDI::CCD::GuideEast(float)
{
    return -1;
}
int INDI::CCD::GuideWest(float)
{
    return -1;
}

