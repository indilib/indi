/*******************************************************************************
  Copyright(c) 2010, 2011 Gerry Rozema, Jasem Mutlaq. All rights reserved.

  This program is free software; you can redistribute it and/or modify it
  under the terms of the GNU General Public License as published by the Free
  Software Foundation; either version 2 of the License, or (at your option)
  any later version.

  This program is distributed in the hope that it will be useful, but WITHOUT
  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
  more details.

  You should have received a copy of the GNU General Public License along with
  this program; if not, write to the Free Software Foundation, Inc., 59
  Temple Place - Suite 330, Boston, MA  02111-1307, USA.

  The full GNU General Public License is included in this distribution in the
  file called LICENSE.
*******************************************************************************/

#include "indiccd.h"

#include <string.h>

#include <zlib.h>

const char *IMAGE_SETTINGS_TAB = "Image Settings";
const char *IMAGE_INFO_TAB     = "Image Info";
const char *GUIDE_HEAD_TAB     = "Guide Head";
const char *GUIDE_CONTROL_TAB     = "Guider Control";

INDI::CCD::CCD()
{
    //ctor
    SendCompressed=false;
    GuiderCompressed=false;
    HasGuideHead=false;
    HasSt4Port=false;
    Interlaced=false;

    RawFrame=NULL;
    RawFrameSize=0;

    RawGuideSize=0;
    RawGuiderFrame=NULL;

    FrameType=LIGHT_FRAME;

    ImageFrameNP = new INumberVectorProperty;
    FrameTypeSP = new ISwitchVectorProperty;
    ImageExposureNP = new INumberVectorProperty;
    ImageBinNP = new INumberVectorProperty;
    ImagePixelSizeNP = new INumberVectorProperty;
    GuiderFrameNP = new INumberVectorProperty;
    GuiderPixelSizeNP = new INumberVectorProperty;
    GuiderExposureNP = new INumberVectorProperty;
    GuiderVideoSP = new ISwitchVectorProperty;
    CompressSP = new ISwitchVectorProperty;
    GuiderCompressSP = new ISwitchVectorProperty;
    FitsBP = new IBLOBVectorProperty;
    GuiderBP = new IBLOBVectorProperty;
    GuideNSP = new INumberVectorProperty;
    GuideEWP = new INumberVectorProperty;
    TelescopeTP = new ITextVectorProperty;

}

INDI::CCD::~CCD()
{
    //dtor
}

bool INDI::CCD::initProperties()
{
    DefaultDriver::initProperties();   //  let the base class flesh in what it wants

    IUFillNumber(&ImageFrameN[0],"X","Left ","%4.0f",0,1392.0,0,0);
    IUFillNumber(&ImageFrameN[1],"Y","Top","%4.0f",0,1040,0,0);
    IUFillNumber(&ImageFrameN[2],"WIDTH","Width","%4.0f",0,1392.0,0,1392.0);
    IUFillNumber(&ImageFrameN[3],"HEIGHT","Height","%4.0f",0,1040,0,1040);
    IUFillNumberVector(ImageFrameNP,ImageFrameN,4,deviceName(),"CCD_FRAME","Frame",IMAGE_SETTINGS_TAB,IP_RW,60,IPS_IDLE);

    IUFillSwitch(&FrameTypeS[0],"FRAME_LIGHT","Light",ISS_ON);
    IUFillSwitch(&FrameTypeS[1],"FRAME_BIAS","Bias",ISS_OFF);
    IUFillSwitch(&FrameTypeS[2],"FRAME_DARK","Dark",ISS_OFF);
    IUFillSwitch(&FrameTypeS[3],"FRAME_FLAT","Flat",ISS_OFF);
    IUFillSwitchVector(FrameTypeSP,FrameTypeS,4,deviceName(),"CCD_FRAME_TYPE","FrameType",IMAGE_SETTINGS_TAB,IP_RW,ISR_1OFMANY,60,IPS_IDLE);

    IUFillNumber(&ImageExposureN[0],"CCD_EXPOSURE_VALUE","Duration (s)","%5.2f",0,36000,0,1.0);
    IUFillNumberVector(ImageExposureNP,ImageExposureN,1,deviceName(),"CCD_EXPOSURE_REQUEST","Expose",MAIN_CONTROL_TAB,IP_RW,60,IPS_IDLE);

    IUFillNumber(&ImageBinN[0],"HOR_BIN","X","%2.0f",1,4,1,1);
    IUFillNumber(&ImageBinN[1],"VER_BIN","Y","%2.0f",1,4,1,1);
    IUFillNumberVector(ImageBinNP,ImageBinN,2,deviceName(),"CCD_BINNING","Binning",IMAGE_SETTINGS_TAB,IP_RW,60,IPS_IDLE);

    IUFillNumber(&ImagePixelSizeN[0],"CCD_MAX_X","Resolution x","%4.0f",1,40,0,6.45);
    IUFillNumber(&ImagePixelSizeN[1],"CCD_MAX_Y","Resolution y","%4.0f",1,40,0,6.45);
    IUFillNumber(&ImagePixelSizeN[2],"CCD_PIXEL_SIZE","Pixel size (um)","%5.2f",1,40,0,6.45);
    IUFillNumber(&ImagePixelSizeN[3],"CCD_PIXEL_SIZE_X","Pixel size X","%5.2f",1,40,0,6.45);
    IUFillNumber(&ImagePixelSizeN[4],"CCD_PIXEL_SIZE_Y","Pixel size Y","%5.2f",1,40,0,6.45);
    IUFillNumber(&ImagePixelSizeN[5],"CCD_BITSPERPIXEL","Bits per pixel","%3.0f",1,40,0,6.45);
    IUFillNumberVector(ImagePixelSizeNP,ImagePixelSizeN,6,deviceName(),"CCD_INFO","Ccd Information",IMAGE_INFO_TAB,IP_RO,60,IPS_IDLE);

    IUFillNumber(&GuiderFrameN[0],"X","Left ","%4.0f",0,1392.0,0,0);
    IUFillNumber(&GuiderFrameN[1],"Y","Top","%4.0f",0,1040,0,0);
    IUFillNumber(&GuiderFrameN[2],"WIDTH","Width","%4.0f",0,1392.0,0,1392.0);
    IUFillNumber(&GuiderFrameN[3],"HEIGHT","Height","%4.0f",0,1040,0,1040);
    IUFillNumberVector(GuiderFrameNP,GuiderFrameN,4,deviceName(),"GUIDER_FRAME","Frame",GUIDE_HEAD_TAB,IP_RW,60,IPS_IDLE);

    IUFillNumber(&GuiderPixelSizeN[0],"GUIDER_MAX_X","Resolution x","%4.0f",1,40,0,6.45);
    IUFillNumber(&GuiderPixelSizeN[1],"GUIDER_MAX_Y","Resolution y","%4.0f",1,40,0,6.45);
    IUFillNumber(&GuiderPixelSizeN[2],"GUIDER_PIXEL_SIZE","Pixel size (um)","%5.2f",1,40,0,6.45);
    IUFillNumber(&GuiderPixelSizeN[3],"GUIDER_PIXEL_SIZE_X","Pixel size X","%5.2f",1,40,0,6.45);
    IUFillNumber(&GuiderPixelSizeN[4],"GUIDER_PIXEL_SIZE_Y","Pixel size Y","%5.2f",1,40,0,6.45);
    IUFillNumber(&GuiderPixelSizeN[5],"GUIDER_BITSPERPIXEL","Bits per pixel","%3.0f",1,40,0,6.45);
    IUFillNumberVector(GuiderPixelSizeNP,GuiderPixelSizeN,6,deviceName(),"GUIDER_INFO",GUIDE_HEAD_TAB,GUIDE_HEAD_TAB,IP_RO,60,IPS_IDLE);

    IUFillNumber(&GuiderExposureN[0],"GUIDER_EXPOSURE_VALUE","Duration (s)","%5.2f",0,36000,0,1.0);
    IUFillNumberVector(GuiderExposureNP,GuiderExposureN,1,deviceName(),"GUIDER_EXPOSURE","Guider",MAIN_CONTROL_TAB,IP_RW,60,IPS_IDLE);

    IUFillSwitch(&GuiderVideoS[0],"ON","on",ISS_OFF);
    IUFillSwitch(&GuiderVideoS[1],"OFF","off",ISS_OFF);
    IUFillSwitchVector(GuiderVideoSP,GuiderVideoS,2,deviceName(),"VIDEO_STREAM","Guider Stream",GUIDE_HEAD_TAB,IP_RW,ISR_1OFMANY,60,IPS_IDLE);

    IUFillSwitch(&CompressS[0],"COMPRESS","Compress",ISS_OFF);
    IUFillSwitch(&CompressS[1],"RAW","Raw",ISS_ON);
    IUFillSwitchVector(CompressSP,CompressS,2,deviceName(),"COMPRESSION","Image",IMAGE_SETTINGS_TAB,IP_RW,ISR_1OFMANY,60,IPS_IDLE);

    IUFillSwitch(&GuiderCompressS[0],"GCOMPRESS","Compress",ISS_OFF);
    IUFillSwitch(&GuiderCompressS[1],"GRAW","Raw",ISS_ON);
    IUFillSwitchVector(GuiderCompressSP,GuiderCompressS,2,deviceName(),"GCOMPRESSION","Guider",GUIDE_HEAD_TAB,IP_RW,ISR_1OFMANY,60,IPS_IDLE);

    IUFillBLOB(&FitsB,"CCD1","Image","");
    IUFillBLOBVector(FitsBP,&FitsB,1,deviceName(),"CCD1","Image Data",OPTIONS_TAB,IP_RO,60,IPS_IDLE);

    IUFillBLOB(&GuiderB,"CCD2","Guider","");
    IUFillBLOBVector(GuiderBP,&GuiderB,1,deviceName(),"CCD2","Guider Data",OPTIONS_TAB,IP_RO,60,IPS_IDLE);

    IUFillNumber(&GuideNS[0],"TIMED_GUIDE_N","North (sec)","%g",0,10,0.001,0);
    IUFillNumber(&GuideNS[1],"TIMED_GUIDE_S","South (sec)","%g",0,10,0.001,0);
    IUFillNumberVector(GuideNSP,GuideNS,2,deviceName(),"TELESCOPE_TIMED_GUIDE_NS","Guide North/South",GUIDE_CONTROL_TAB,IP_RW,60,IPS_IDLE);

    IUFillNumber(&GuideEW[0],"TIMED_GUIDE_E","East (sec)","%g",0,10,0.001,0);
    IUFillNumber(&GuideEW[1],"TIMED_GUIDE_W","West (sec)","%g",0,10,0.001,0);
    IUFillNumberVector(GuideEWP,GuideEW,2,deviceName(),"TELESCOPE_TIMED_GUIDE_WE","Guide East/West",GUIDE_CONTROL_TAB,IP_RW,60,IPS_IDLE);

    IUFillText(&TelescopeT[0],"ACTIVE_TELESCOPE","Telescope","");
    IUFillTextVector(TelescopeTP,TelescopeT,1,deviceName(),"ACTIVE_DEVICES","Snoop Scope",OPTIONS_TAB,IP_RW,60,IPS_IDLE);

    IUFillNumber(&EqN[0],"RA_PEC","Ra (hh:mm:ss)","%010.6m",0,24,0,0);
    IUFillNumber(&EqN[1],"DEC_PEC","Dec (dd:mm:ss)","%010.6m",-90,90,0,0);
    IUFillNumberVector(&EqNP,EqN,2,"","EQUATORIAL_PEC","EQ PEC","Main Control",IP_RW,60,IPS_IDLE);


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
        defineNumber(ImageExposureNP);
        defineNumber(ImageFrameNP);
        defineNumber(ImageBinNP);


        if(HasGuideHead)
        {
            IDLog("Sending Guider Stuff\n");
            defineNumber(GuiderExposureNP);
            defineNumber(GuiderFrameNP);
            defineSwitch(GuiderVideoSP);
        }

        defineNumber(ImagePixelSizeNP);
        if(HasGuideHead)
        {
            defineNumber(GuiderPixelSizeNP);
        }
        defineSwitch(CompressSP);
        defineBLOB(FitsBP);
        if(HasGuideHead)
        {
            defineSwitch(GuiderCompressSP);
            defineBLOB(GuiderBP);
        }
        if(HasSt4Port)
        {
            defineNumber(GuideNSP);
            defineNumber(GuideEWP);
        }
        defineSwitch(FrameTypeSP);
        defineText(TelescopeTP);
    }
    else
    {
        deleteProperty(ImageFrameNP->name);
        deleteProperty(ImageBinNP->name);
        deleteProperty(ImagePixelSizeNP->name);
        deleteProperty(ImageExposureNP->name);
        deleteProperty(FitsBP->name);
        deleteProperty(CompressSP->name);
        if(HasGuideHead)
        {
            deleteProperty(GuiderVideoSP->name);
            deleteProperty(GuiderExposureNP->name);
            deleteProperty(GuiderFrameNP->name);
            deleteProperty(GuiderPixelSizeNP->name);
            deleteProperty(GuiderBP->name);
            deleteProperty(GuiderCompressSP->name);
        }
        if(HasSt4Port)
        {
            deleteProperty(GuideNSP->name);
            deleteProperty(GuideEWP->name);

        }
        deleteProperty(FrameTypeSP->name);
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
    if(strcmp(dev,deviceName())==0) {
        //  This is for our device
        //  Now lets see if it's something we process here
        if(strcmp(name,"CCD_EXPOSURE_REQUEST")==0) {
            float n;
            int rc;

            n=values[0];

            ImageExposureN[0].value=n;

            if (ImageExposureNP->s==IPS_BUSY)
                AbortExposure();

            ImageExposureNP->s=IPS_BUSY;
            //  now we have a new number, this is our requested exposure time
            //  Tell the clients we are busy with this exposure

            //  And now call the physical hardware layer to start the exposure
            rc=StartExposure(n);
            switch(rc) {
                case 0: //  normal case, exposure running on timers, callbacks when it's finished
                    ImageExposureNP->s=IPS_BUSY;
                    break;
                case 1: //  Short exposure, it's already done
                    ImageExposureNP->s=IPS_OK;
                    break;
                case -1:    //  error condition
                    ImageExposureNP->s=IPS_ALERT;
                break;
            }
            IDSetNumber(ImageExposureNP,NULL);
            return true;
        }

        if(strcmp(name,"GUIDER_EXPOSURE")==0) {
            float n;
            int rc;

            n=values[0];

            GuiderExposureN[0].value=n;
            GuiderExposureNP->s=IPS_BUSY;
            //  now we have a new number, this is our requested exposure time
            //  Tell the clients we are busy with this exposure

            //  And now call the physical hardware layer to start the exposure
            //  change of plans, this is just changing exposure time
            //  the the start/stop stream buttons do the rest
            rc=StartGuideExposure(n);
            //rc=1;   //  set it to ok
            switch(rc) {
                case 0: //  normal case, exposure running on timers, callbacks when it's finished
                    GuiderExposureNP->s=IPS_BUSY;
                    break;
                case 1: //  Short exposure, it's already done
                    GuiderExposureNP->s=IPS_OK;
                    break;
                case -1:    //  error condition
                    GuiderExposureNP->s=IPS_ALERT;
                break;
            }
            IDSetNumber(GuiderExposureNP,NULL);
            return true;
        }
        if(strcmp(name,"CCD_BINNING")==0) {
            //  We are being asked to set camera binning
            ImageBinNP->s=IPS_OK;
            IUUpdateNumber(ImageBinNP,values,names,n);
            //  Update client display
            IDSetNumber(ImageBinNP,NULL);

            //IDLog("Binning set to %4.0f x %4.0f\n",CcdBinN[0].value,CcdBinN[1].value);
            BinX=ImageBinN[0].value;
            BinY=ImageBinN[1].value;

            updateCCDBin();
        }

        if(strcmp(name,"CCD_FRAME")==0)
        {
            //  We are being asked to set camera binning
            ImageFrameNP->s=IPS_OK;
            IUUpdateNumber(ImageFrameNP,values,names,n);
            //  Update client display
            IDSetNumber(ImageFrameNP,NULL);

            //IDLog("Frame set to %4.0f,%4.0f %4.0f x %4.0f\n",CcdFrameN[0].value,CcdFrameN[1].value,CcdFrameN[2].value,CcdFrameN[3].value);
            SubX=ImageFrameN[0].value;
            SubY=ImageFrameN[1].value;
            SubW=ImageFrameN[2].value;
            SubH=ImageFrameN[3].value;

            updateCCDFrame();
            return true;
        }

        if(strcmp(name,"GUIDER_FRAME")==0)
        {
            //  We are being asked to set camera binning
            GuiderFrameNP->s=IPS_OK;
            IUUpdateNumber(GuiderFrameNP,values,names,n);
            //  Update client display
            IDSetNumber(GuiderFrameNP,NULL);

            IDLog("GuiderFrame set to %4.0f,%4.0f %4.0f x %4.0f\n",
                  GuiderFrameN[0].value,GuiderFrameN[1].value,GuiderFrameN[2].value,GuiderFrameN[3].value);
            GSubX=GuiderFrameN[0].value;
            GSubY=GuiderFrameN[1].value;
            GSubW=GuiderFrameN[2].value;
            GSubH=GuiderFrameN[3].value;
            return true;
        }

        if(strcmp(name,GuideNSP->name)==0) {
            //  We are being asked to send a guide pulse north/south on the st4 port
            GuideNSP->s=IPS_BUSY;
            IUUpdateNumber(GuideNSP,values,names,n);
            //  Update client display
            IDSetNumber(GuideNSP,NULL);

            fprintf(stderr,"GuideNorthSouth set to %7.3f,%7.3f\n",
                  GuideNS[0].value,GuideNS[1].value);

            if(GuideNS[0].value != 0) {
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
        if(strcmp(name,GuideEWP->name)==0) {
            //  We are being asked to send a guide pulse north/south on the st4 port
            GuideEWP->s=IPS_BUSY;
            IUUpdateNumber(GuideEWP,values,names,n);
            //  Update client display
            IDSetNumber(GuideEWP,NULL);

            fprintf(stderr,"GuiderEastWest set to %6.3f,%6.3f\n",
                  GuideEW[0].value,GuideEW[1].value);

            if(GuideEW[0].value != 0) {
                GuideEast(GuideEW[0].value);
            } else {
                GuideWest(GuideEW[1].value);
            }

            GuideEW[0].value=0;
            GuideEW[1].value=0;
            GuideEWP->s=IPS_OK;
            IDSetNumber(GuideEWP,NULL);

            return true;
        }

        /*
        if(strcmp(name,"CCDPREVIEW_CTRL")==0) {
            //  We are being asked to set camera binning
            GuiderNP->s=IPS_OK;
            IUUpdateNumber(&GuiderNP,values,names,n);
            //  Update client display
            IDSetNumber(&GuiderNP,NULL);
            return true;
        }
        */

    }
    //  if we didn't process it, continue up the chain, let somebody else
    //  give it a shot
    return DefaultDriver::ISNewNumber(dev,name,values,names,n);
}

bool INDI::CCD::ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n)
{

    //  Ok, lets Process any switches we actually handle here
    //IDLog("INDI::CCD IsNewSwitch %s %s\n",dev,name);


    if(strcmp(dev,deviceName())==0) {
        //  it's for this device

        //for(int x=0; x<n; x++) {
        //    IDLog("Switch %s\n",names[x]);
        //}

        if(strcmp(name,CompressSP->name)==0) {

            IUUpdateSwitch(CompressSP,states,names,n);
            IDSetSwitch(CompressSP,NULL);

            if(CompressS[0].s==ISS_ON    ) {
                SendCompressed=true;
            } else {
                SendCompressed=false;
            }
            return true;
        }
        if(strcmp(name,GuiderCompressSP->name)==0) {

            IUUpdateSwitch(GuiderCompressSP,states,names,n);
            IDSetSwitch(GuiderCompressSP,NULL);

            if(GuiderCompressS[0].s==ISS_ON    ) {
                SendCompressed=true;
            } else {
                SendCompressed=false;
            }
            return true;
        }

        if(strcmp(name,GuiderVideoSP->name)==0) {
            //  Compression Update
            IUUpdateSwitch(GuiderVideoSP,states,names,n);
            if(GuiderVideoS[0].s==ISS_ON    ) {
                GuiderVideoSP->s=IPS_OK;
                StartGuideExposure(GuiderExposureN[0].value);
            } else {
                AbortGuideExposure();
                GuiderVideoSP->s=IPS_IDLE;
            }

            IDSetSwitch(GuiderVideoSP,NULL);
            return true;
        }

        if(strcmp(name,FrameTypeSP->name)==0)
        {
            //  Compression Update
            IUUpdateSwitch(FrameTypeSP,states,names,n);
            FrameTypeSP->s=IPS_OK;
            if(FrameTypeS[0].s==ISS_ON) SetFrameType(LIGHT_FRAME);
            if(FrameTypeS[1].s==ISS_ON) SetFrameType(BIAS_FRAME);
            if(FrameTypeS[2].s==ISS_ON) SetFrameType(DARK_FRAME);
            if(FrameTypeS[3].s==ISS_ON) SetFrameType(FLAT_FRAME);

            IDSetSwitch(FrameTypeSP,NULL);
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

void INDI::CCD::updateCCDFrame()
{
    // Nothing to do here. HW layer should take care of it if needed.
}

void INDI::CCD::updateCCDBin()
{
    // Nothing to do here. HW layer should take care of it if needed.
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

    fitsfile *fptr=NULL;


    //IDLog("Enter Exposure Complete %d %d %d %d\n",SubW,SubH,BinX,BinY);


    naxes[0]=SubW/BinX;

    if (Interlaced)
        naxes[1]=SubH/(BinY-1);
    else
        naxes[1]=SubH/BinY;
    //  Now we have to send fits format data to the client
    memsize=5760;
    memptr=malloc(memsize);
    if(!memptr) {
        IDLog("Error: failed to allocate memory: %lu\n",(unsigned long)memsize);
    }
    fits_create_memfile(&fptr,&memptr,&memsize,2880,realloc,&status);
    if(status) {
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

    if (Interlaced)
        fits_write_img(fptr,TUSHORT,1,SubW*SubH/BinX/(BinY-1),RawFrame,&status);
    else
        fits_write_img(fptr,TUSHORT,1,SubW*SubH/BinX/BinY,RawFrame,&status);

    if (status)
	{
		IDLog("Error: Failed to write FITS image\n");
		fits_report_error(stderr, status);  /* print out any error messages */
		return false;
	}
	fits_close_file(fptr,&status);
	//IDLog("Built the fits file\n");

    //  ok, undo the kludge we threw in for
    //  guider frames, and set the resolution back

    ImageFrameN[2].value=SubW;
    ImageFrameN[3].value=SubH;
    IDSetNumber(ImageFrameNP,NULL);


    ImageExposureNP->s=IPS_OK;
    IDSetNumber(ImageExposureNP,NULL);

    uploadfile(memptr,memsize);
    free(memptr);
    return true;
}

bool INDI::CCD::GuideExposureComplete()
{

    //IDLog("Enter GuideExposure Complete %d %d\n",GSubW,GSubH);

    if(GuiderVideoS[0].s==ISS_ON) {
        //  if our stream switch for the guider
        //  is on, then send this as a video stream
        //  frame, and, start another frame

        //  Ok, bit of a kludge here
        //  we need to send a new size to kstars
        ImageFrameN[2].value=GSubW;
        ImageFrameN[3].value=GSubH;
        IDSetNumber(ImageFrameNP,NULL);

        //IDLog("Sending guider stream blob\n");

        GuiderExposureNP->s=IPS_OK;
        IDSetNumber(GuiderExposureNP,NULL);
        GuiderB.blob=RawGuiderFrame;
        GuiderB.bloblen=GSubW*GSubH;
        GuiderB.size=GSubW*GSubH;
        strcpy(GuiderB.format,"->stream");
        GuiderBP->s=IPS_OK;
        IDSetBLOB(GuiderBP,NULL);
        StartGuideExposure(GuiderExposureN[0].value);
    } else {
        //  We are not streaming
        //  So, lets send this as an 8 bit fits frame
        void *memptr;
        size_t memsize;
        int status=0;
        long naxes[2];
        long naxis=2;

        fitsfile *fptr=NULL;
        naxes[0]=GSubW;
        naxes[1]=GSubH;
        //  Now we have to send fits format data to the client
        memsize=5760;
        memptr=malloc(memsize);
        if(!memptr) {
            IDLog("Error: failed to allocate memory: %lu\n",(unsigned long)memsize);
        }
        fits_create_memfile(&fptr,&memptr,&memsize,2880,realloc,&status);
        if(status) {
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

        fits_write_img(fptr,TBYTE,1,GSubW*GSubH,RawGuiderFrame,&status);
        if (status)
        {
            IDLog("Error: Failed to write FITS image\n");
            fits_report_error(stderr, status);  /* print out any error messages */
            return false;
        }
        fits_close_file(fptr,&status);
        IDLog("Built the Guider fits file\n");
        GuiderExposureNP->s=IPS_OK;
        IDSetNumber(GuiderExposureNP,NULL);

        IDLog("Sending guider fits file via %s\n",GuiderBP->name);
        GuiderB.blob=memptr;
        GuiderB.bloblen=memsize;
        GuiderB.size=memsize;
        strcpy(GuiderB.format,".fits");
        GuiderBP->s=IPS_OK;
        IDSetBLOB(GuiderBP,NULL);
        free(memptr);
    }


    return true;
}


int INDI::CCD::sendPreview()
{
    int numbytes;
    int xw;
    int yw;
    //unsigned char *compressed;
    //uLongf compressedbytes=0;
    //int ccount;
    //int r;

    return 0;
    xw=SubW/BinX;

    if (Interlaced)
        yw=SubH/(BinY-1);
    else
        yw=SubH/BinY;

    //GuiderNP->s=IPS_OK;
    //GuiderN[0].value=xw;
    //GuiderN[1].value=yw;
    //IDLog("Guider Frame %d x %d\n",xw,yw);
    //IDSetNumber(&GuiderNP,"Starting to send new preview frame\n");

    numbytes=xw*yw;
    numbytes=numbytes*2;
    //compressedbytes=numbytes+numbytes/64+16+3;
    //compressed=(unsigned char *)malloc(numbytes+numbytes/64+16+3);
    //r=compress2(compressed,&compressedbytes,(unsigned char *)RawFrame,numbytes,9);
    //if(r != Z_OK) {
    //    IDLog("Got an error from compress\n");
    //    return -1;
    //}


    //FitsB.blob=compressed;
    //FitsB.bloblen=compressedbytes;
    FitsB.blob=RawFrame;
    FitsB.bloblen=numbytes;
    FitsB.size=numbytes;
    FitsBP->s=IPS_OK;
    strcpy(FitsB.format,".ccdpreview");
    IDSetBLOB(FitsBP,NULL);
    //IDLog("Done with SetBlob with %d compressed to %d\n",numbytes,compressedbytes);
    //free(compressed);
    return 0;

}

int INDI::CCD::uploadfile(void *fitsdata,int total)
{
    //  lets try sending a ccd preview


    //IDLog("Enter Uploadfile with %d total sending via %s\n",total,FitsBP->name);
    FitsB.blob=fitsdata;
    FitsB.bloblen=total;
    FitsB.size=total;
    strcpy(FitsB.format,".fits");
    FitsBP->s=IPS_OK;
    IDSetBLOB(FitsBP,NULL);
    //IDLog("Done with SetBlob\n");

    return 0;
}

int INDI::CCD::SetCCDParams(int x,int y,int bpp,float xf,float yf)
{
    XRes=x;
    YRes=y;
    SubX=0;
    SubY=0;
    SubW=XRes;
    SubH=YRes;
    BinX=1;
    BinY=1;

    PixelSizex=xf;
    PixelSizey=yf;
    ImageFrameN[2].value=x;
    ImageFrameN[3].value=y;
    ImagePixelSizeN[0].value=x;
    ImagePixelSizeN[1].value=y;
    ImagePixelSizeN[2].value=xf;
    ImagePixelSizeN[3].value=xf;
    ImagePixelSizeN[4].value=yf;
    ImagePixelSizeN[5].value=bpp;
    return 0;
}

int INDI::CCD::SetGuidHeadParams(int x,int y,int bpp,float xf,float yf)
{
    HasGuideHead=true;
    GXRes=x;
    GYRes=y;
    GSubX=0;
    GSubY=0;
    GSubW=GXRes;
    GSubH=GYRes;
    GuiderFrameN[2].value=x;
    GuiderFrameN[3].value=y;
    GuiderPixelSizeN[0].value=x;
    GuiderPixelSizeN[1].value=y;
    GuiderPixelSizeN[2].value=xf;
    GuiderPixelSizeN[3].value=xf;
    GuiderPixelSizeN[4].value=yf;
    GuiderPixelSizeN[5].value=bpp;
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
int INDI::CCD::SetFrameType(CCD_FRAME t)
{
    //fprintf(stderr,"Set frame type to %d\n",t);
    FrameType=t;
    return 0;
}
