/*******************************************************************************
  Copyright(c) 2010 Gerry Rozema. All rights reserved.

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

#include <fitsio.h>
#include <zlib.h>

INDI::CCD::CCD()
{
    //ctor
    SendCompressed=false;
    GuiderCompressed=false;
    HasGuideHead=false;
    HasSt4Port=false;

    RawFrame=NULL;
    RawFrameSize=0;

    RawGuideSize=0;
    RawGuiderFrame=NULL;

    FrameType=FRAME_TYPE_LIGHT;

    ImageFrameNV = new INumberVectorProperty;
    FrameTypeSV = new ISwitchVectorProperty;
    ImageExposureNV = new INumberVectorProperty;
    ImageBinNV = new INumberVectorProperty;
    ImagePixelSizeNV = new INumberVectorProperty;
    GuiderFrameNV = new INumberVectorProperty;
    GuiderPixelSizeNV = new INumberVectorProperty;
    GuiderExposureNV = new INumberVectorProperty;
    GuiderVideoSV = new ISwitchVectorProperty;
    CompressSV = new ISwitchVectorProperty;
    GuiderCompressSV = new ISwitchVectorProperty;
    FitsBV = new IBLOBVectorProperty;
    GuiderBV = new IBLOBVectorProperty;
    GuideNSV = new INumberVectorProperty;
    GuideEWV = new INumberVectorProperty;

}

INDI::CCD::~CCD()
{
    //dtor
}

bool INDI::CCD::initProperties()
{
    //IDLog("INDI::CCD initProperties '%s'\n",deviceName());

    DefaultDriver::initProperties();   //  let the base class flesh in what it wants

    //IDLog("INDI::CCD::initProperties()\n");

    IUFillNumber(&ImageFrameN[0],"X","Left ","%4.0f",0,1392.0,0,0);
    IUFillNumber(&ImageFrameN[1],"Y","Top","%4.0f",0,1040,0,0);
    IUFillNumber(&ImageFrameN[2],"WIDTH","Width","%4.0f",0,1392.0,0,1392.0);
    IUFillNumber(&ImageFrameN[3],"HEIGHT","Height","%4.0f",0,1040,0,1040);
    IUFillNumberVector(ImageFrameNV,ImageFrameN,4,deviceName(),"CCD_FRAME","Frame","Image Settings",IP_RW,60,IPS_IDLE);

    IUFillSwitch(&FrameTypeS[0],"FRAME_LIGHT","Light",ISS_ON);
    IUFillSwitch(&FrameTypeS[1],"FRAME_BIAS","Bias",ISS_OFF);
    IUFillSwitch(&FrameTypeS[2],"FRAME_DARK","Dark",ISS_OFF);
    IUFillSwitch(&FrameTypeS[3],"FRAME_FLAT","Flat",ISS_OFF);
    IUFillSwitchVector(FrameTypeSV,FrameTypeS,4,deviceName(),"CCD_FRAME_TYPE","FrameType","Image Settings",IP_RW,ISR_1OFMANY,60,IPS_IDLE);

    IUFillNumber(&ImageExposureN[0],"CCD_EXPOSURE_VALUE","Duration (s)","%5.2f",0,36000,0,1.0);
    IUFillNumberVector(ImageExposureNV,ImageExposureN,1,deviceName(),"CCD_EXPOSURE_REQUEST","Expose","Main Control",IP_RW,60,IPS_IDLE);
    //IUFillNumber(&CcdExposureReqN[0],"CCD_EXPOSURE_VALUE","Duration","%5.2f",0,36000,0,1.0);
    //IUFillNumberVector(&CcdExposureReqNV,CcdExposureReqN,1,deviceName(),"CCD_EXPOSURE_REQUEST","Expose","Main Control",IP_WO,60,IPS_IDLE);

    IUFillNumber(&ImageBinN[0],"HOR_BIN","X","%2.0f",1,4,1,1);
    IUFillNumber(&ImageBinN[1],"VER_BIN","Y","%2.0f",1,4,1,1);
    IUFillNumberVector(ImageBinNV,ImageBinN,2,deviceName(),"CCD_BINNING","Binning","Image Settings",IP_RW,60,IPS_IDLE);


    IUFillNumber(&ImagePixelSizeN[0],"CCD_MAX_X","Resolution x","%4.0f",1,40,0,6.45);
    IUFillNumber(&ImagePixelSizeN[1],"CCD_MAX_Y","Resolution y","%4.0f",1,40,0,6.45);
    IUFillNumber(&ImagePixelSizeN[2],"CCD_PIXEL_SIZE","Pixel size (um)","%5.2f",1,40,0,6.45);
    IUFillNumber(&ImagePixelSizeN[3],"CCD_PIXEL_SIZE_X","Pixel size X","%5.2f",1,40,0,6.45);
    IUFillNumber(&ImagePixelSizeN[4],"CCD_PIXEL_SIZE_Y","Pixel size Y","%5.2f",1,40,0,6.45);
    IUFillNumber(&ImagePixelSizeN[5],"CCD_BITSPERPIXEL","Bits per pixel","%3.0f",1,40,0,6.45);
    IUFillNumberVector(ImagePixelSizeNV,ImagePixelSizeN,6,deviceName(),"CCD_INFO","Ccd Information","Image Info",IP_RO,60,IPS_IDLE);

    IUFillNumber(&GuiderFrameN[0],"X","Left ","%4.0f",0,1392.0,0,0);
    IUFillNumber(&GuiderFrameN[1],"Y","Top","%4.0f",0,1040,0,0);
    IUFillNumber(&GuiderFrameN[2],"WIDTH","Width","%4.0f",0,1392.0,0,1392.0);
    IUFillNumber(&GuiderFrameN[3],"HEIGHT","Height","%4.0f",0,1040,0,1040);
    IUFillNumberVector(GuiderFrameNV,GuiderFrameN,4,deviceName(),"GUIDER_FRAME","Frame","Guidehead Settings",IP_RW,60,IPS_IDLE);


    IUFillNumber(&GuiderPixelSizeN[0],"GUIDER_MAX_X","Resolution x","%4.0f",1,40,0,6.45);
    IUFillNumber(&GuiderPixelSizeN[1],"GUIDER_MAX_Y","Resolution y","%4.0f",1,40,0,6.45);
    IUFillNumber(&GuiderPixelSizeN[2],"GUIDER_PIXEL_SIZE","Pixel size (um)","%5.2f",1,40,0,6.45);
    IUFillNumber(&GuiderPixelSizeN[3],"GUIDER_PIXEL_SIZE_X","Pixel size X","%5.2f",1,40,0,6.45);
    IUFillNumber(&GuiderPixelSizeN[4],"GUIDER_PIXEL_SIZE_Y","Pixel size Y","%5.2f",1,40,0,6.45);
    IUFillNumber(&GuiderPixelSizeN[5],"GUIDER_BITSPERPIXEL","Bits per pixel","%3.0f",1,40,0,6.45);
    IUFillNumberVector(GuiderPixelSizeNV,GuiderPixelSizeN,6,deviceName(),"GUIDER_INFO","Guidehead Information","Guidehead Info",IP_RO,60,IPS_IDLE);

    IUFillNumber(&GuiderExposureN[0],"GUIDER_EXPOSURE_VALUE","Duration (s)","%5.2f",0,36000,0,1.0);
    IUFillNumberVector(GuiderExposureNV,GuiderExposureN,1,deviceName(),"GUIDER_EXPOSURE","Guider","Main Control",IP_RW,60,IPS_IDLE);

    IUFillSwitch(&GuiderVideoS[0],"ON","on",ISS_OFF);
    IUFillSwitch(&GuiderVideoS[1],"OFF","off",ISS_OFF);
    IUFillSwitchVector(GuiderVideoSV,GuiderVideoS,2,deviceName(),"VIDEO_STREAM","Guider Stream","Guidehead Settings",IP_RW,ISR_1OFMANY,60,IPS_IDLE);

    IUFillSwitch(&CompressS[0],"COMPRESS","Compress",ISS_OFF);
    IUFillSwitch(&CompressS[1],"RAW","Raw",ISS_ON);
    IUFillSwitchVector(CompressSV,CompressS,2,deviceName(),"COMPRESSION","Image","Data Channel",IP_RW,ISR_1OFMANY,60,IPS_IDLE);

    IUFillSwitch(&GuiderCompressS[0],"GCOMPRESS","Compress",ISS_OFF);
    IUFillSwitch(&GuiderCompressS[1],"GRAW","Raw",ISS_ON);
    IUFillSwitchVector(GuiderCompressSV,GuiderCompressS,2,deviceName(),"GCOMPRESSION","Guider","Data Channel",IP_RW,ISR_1OFMANY,60,IPS_IDLE);

    IUFillBLOB(&FitsB,"CCD1","Image","");
    IUFillBLOBVector(FitsBV,&FitsB,1,deviceName(),"CCD1","Image Data","Data Channel",IP_RO,60,IPS_IDLE);

    IUFillBLOB(&GuiderB,"CCD2","Guider","");
    IUFillBLOBVector(GuiderBV,&GuiderB,1,deviceName(),"CCD2","Guider Data","Data Channel",IP_RO,60,IPS_IDLE);

    IUFillNumber(&GuideNS[0],"TIMED_GUIDE_N","North (sec)","%g",0,10,0.001,0);
    IUFillNumber(&GuideNS[1],"TIMED_GUIDE_S","South (sec)","%g",0,10,0.001,0);
    IUFillNumberVector(GuideNSV,GuideNS,2,deviceName(),"TELESCOPE_TIMED_GUIDE_NS","Guide North/South","GuiderControl",IP_RW,60,IPS_IDLE);

    IUFillNumber(&GuideEW[0],"TIMED_GUIDE_E","East (sec)","%g",0,10,0.001,0);
    IUFillNumber(&GuideEW[1],"TIMED_GUIDE_W","West (sec)","%g",0,10,0.001,0);
    IUFillNumberVector(GuideEWV,GuideEW,2,deviceName(),"TELESCOPE_TIMED_GUIDE_WE","Guide East/West","GuiderControl",IP_RW,60,IPS_IDLE);


    //IDLog("Setting up ccdpreview stuff\n");
    //IUFillNumber(&GuiderN[0],"WIDTH","Width","%4.0f",0.,1392.0,0.,1392.);
    //IUFillNumber(&GuiderN[1],"HEIGHT","Height","%4.0f",0.,1040.,0.,1040.);
    //IUFillNumber(&GuiderN[2],"MAXGOODDATA","max good","%5.0f",0.,65535.0,0.,65535.0);
    //IUFillNumber(&GuiderN[3],"BYTESPERPIXEL","BPP","%1.0f",1.,4.,1.,2.);
    //IUFillNumber(&GuiderN[4],"BYTEORDER","BO","%1.0f",1.,2.,1.,1.);
    //IUFillNumberVector(&GuiderNV,GuiderN,5,deviceName(),"CCDPREVIEW_CTRL","Image Size","Guider Settings",IP_RW,60,IPS_IDLE);

    return 0;
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
    if(isConnected()) {
        defineNumber(ImageExposureNV);
        defineNumber(ImageFrameNV);
        defineNumber(ImageBinNV);


        if(HasGuideHead) {
            IDLog("Sending Guider Stuff\n");
            defineNumber(GuiderExposureNV);
            defineNumber(GuiderFrameNV);
            defineSwitch(GuiderVideoSV);
        }

        defineNumber(ImagePixelSizeNV);
        if(HasGuideHead) {
            defineNumber(GuiderPixelSizeNV);
        }
        defineSwitch(CompressSV);
        defineBLOB(FitsBV);
        if(HasGuideHead)
        {
            defineSwitch(GuiderCompressSV);
            defineBLOB(GuiderBV);
        }
        if(HasSt4Port)
        {
            defineNumber(GuideNSV);
            defineNumber(GuideEWV);
        }
        defineSwitch(FrameTypeSV);
    } else {
        deleteProperty(ImageFrameNV->name);
        deleteProperty(ImageBinNV->name);
        deleteProperty(ImagePixelSizeNV->name);
        deleteProperty(ImageExposureNV->name);
        deleteProperty(FitsBV->name);
        deleteProperty(CompressSV->name);
        if(HasGuideHead) {
            deleteProperty(GuiderVideoSV->name);
            deleteProperty(GuiderExposureNV->name);
            deleteProperty(GuiderFrameNV->name);
            deleteProperty(GuiderPixelSizeNV->name);
            deleteProperty(GuiderBV->name);
            deleteProperty(GuiderCompressSV->name);
        }
        if(HasSt4Port) {
            deleteProperty(GuideNSV->name);
            deleteProperty(GuideEWV->name);

        }
        deleteProperty(FrameTypeSV->name);
    }
    return true;
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
            ImageExposureNV->s=IPS_BUSY;
            //  now we have a new number, this is our requested exposure time
            //  Tell the clients we are busy with this exposure

            //  And now call the physical hardware layer to start the exposure
            rc=StartExposure(n);
            switch(rc) {
                case 0: //  normal case, exposure running on timers, callbacks when it's finished
                    ImageExposureNV->s=IPS_BUSY;
                    break;
                case 1: //  Short exposure, it's already done
                    ImageExposureNV->s=IPS_OK;
                    break;
                case -1:    //  error condition
                    ImageExposureNV->s=IPS_ALERT;
                break;
            }
            IDSetNumber(ImageExposureNV,NULL);
            return true;
        }

        if(strcmp(name,"GUIDER_EXPOSURE")==0) {
            float n;
            int rc;

            n=values[0];

            GuiderExposureN[0].value=n;
            GuiderExposureNV->s=IPS_BUSY;
            //  now we have a new number, this is our requested exposure time
            //  Tell the clients we are busy with this exposure

            //  And now call the physical hardware layer to start the exposure
            //  change of plans, this is just changing exposure time
            //  the the start/stop stream buttons do the rest
            rc=StartGuideExposure(n);
            //rc=1;   //  set it to ok
            switch(rc) {
                case 0: //  normal case, exposure running on timers, callbacks when it's finished
                    GuiderExposureNV->s=IPS_BUSY;
                    break;
                case 1: //  Short exposure, it's already done
                    GuiderExposureNV->s=IPS_OK;
                    break;
                case -1:    //  error condition
                    GuiderExposureNV->s=IPS_ALERT;
                break;
            }
            IDSetNumber(GuiderExposureNV,NULL);
            return true;
        }
        if(strcmp(name,"CCD_BINNING")==0) {
            //  We are being asked to set camera binning
            ImageBinNV->s=IPS_OK;
            IUUpdateNumber(ImageBinNV,values,names,n);
            //  Update client display
            IDSetNumber(ImageBinNV,NULL);

            //IDLog("Binning set to %4.0f x %4.0f\n",CcdBinN[0].value,CcdBinN[1].value);
            BinX=ImageBinN[0].value;
            BinY=ImageBinN[1].value;
        }
        if(strcmp(name,"CCD_FRAME")==0) {
            //  We are being asked to set camera binning
            ImageFrameNV->s=IPS_OK;
            IUUpdateNumber(ImageFrameNV,values,names,n);
            //  Update client display
            IDSetNumber(ImageFrameNV,NULL);

            //IDLog("Frame set to %4.0f,%4.0f %4.0f x %4.0f\n",CcdFrameN[0].value,CcdFrameN[1].value,CcdFrameN[2].value,CcdFrameN[3].value);
            SubX=ImageFrameN[0].value;
            SubY=ImageFrameN[1].value;
            SubW=ImageFrameN[2].value;
            SubH=ImageFrameN[3].value;
            return true;
        }

        if(strcmp(name,"GUIDER_FRAME")==0) {
            //  We are being asked to set camera binning
            GuiderFrameNV->s=IPS_OK;
            IUUpdateNumber(GuiderFrameNV,values,names,n);
            //  Update client display
            IDSetNumber(GuiderFrameNV,NULL);

            IDLog("GuiderFrame set to %4.0f,%4.0f %4.0f x %4.0f\n",
                  GuiderFrameN[0].value,GuiderFrameN[1].value,GuiderFrameN[2].value,GuiderFrameN[3].value);
            GSubX=GuiderFrameN[0].value;
            GSubY=GuiderFrameN[1].value;
            GSubW=GuiderFrameN[2].value;
            GSubH=GuiderFrameN[3].value;
            return true;
        }

        if(strcmp(name,GuideNSV->name)==0) {
            //  We are being asked to send a guide pulse north/south on the st4 port
            GuideNSV->s=IPS_BUSY;
            IUUpdateNumber(GuideNSV,values,names,n);
            //  Update client display
            IDSetNumber(GuideNSV,NULL);

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
            GuideNSV->s=IPS_OK;
            IDSetNumber(GuideNSV,NULL);

            return true;
        }
        if(strcmp(name,GuideEWV->name)==0) {
            //  We are being asked to send a guide pulse north/south on the st4 port
            GuideEWV->s=IPS_BUSY;
            IUUpdateNumber(GuideEWV,values,names,n);
            //  Update client display
            IDSetNumber(GuideEWV,NULL);

            fprintf(stderr,"GuiderEastWest set to %6.3f,%6.3f\n",
                  GuideEW[0].value,GuideEW[1].value);

            if(GuideEW[0].value != 0) {
                GuideEast(GuideEW[0].value);
            } else {
                GuideWest(GuideEW[1].value);
            }

            GuideEW[0].value=0;
            GuideEW[1].value=0;
            GuideEWV->s=IPS_OK;
            IDSetNumber(GuideEWV,NULL);

            return true;
        }

        /*
        if(strcmp(name,"CCDPREVIEW_CTRL")==0) {
            //  We are being asked to set camera binning
            GuiderNV->s=IPS_OK;
            IUUpdateNumber(&GuiderNV,values,names,n);
            //  Update client display
            IDSetNumber(&GuiderNV,NULL);
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

        if(strcmp(name,CompressSV->name)==0) {

            IUUpdateSwitch(CompressSV,states,names,n);
            IDSetSwitch(CompressSV,NULL);

            if(CompressS[0].s==ISS_ON    ) {
                SendCompressed=true;
            } else {
                SendCompressed=false;
            }
            return true;
        }
        if(strcmp(name,GuiderCompressSV->name)==0) {

            IUUpdateSwitch(GuiderCompressSV,states,names,n);
            IDSetSwitch(GuiderCompressSV,NULL);

            if(GuiderCompressS[0].s==ISS_ON    ) {
                SendCompressed=true;
            } else {
                SendCompressed=false;
            }
            return true;
        }

        if(strcmp(name,GuiderVideoSV->name)==0) {
            //  Compression Update
            IUUpdateSwitch(GuiderVideoSV,states,names,n);
            if(GuiderVideoS[0].s==ISS_ON    ) {
                GuiderVideoSV->s=IPS_OK;
                StartGuideExposure(GuiderExposureN[0].value);
            } else {
                AbortGuideExposure();
                GuiderVideoSV->s=IPS_IDLE;
            }

            IDSetSwitch(GuiderVideoSV,NULL);
            return true;
        }

        if(strcmp(name,FrameTypeSV->name)==0) {
            //  Compression Update
            IUUpdateSwitch(FrameTypeSV,states,names,n);
            FrameTypeSV->s=IPS_OK;
            if(FrameTypeS[0].s==ISS_ON) SetFrameType(FRAME_TYPE_LIGHT);
            if(FrameTypeS[1].s==ISS_ON) SetFrameType(FRAME_TYPE_BIAS);
            if(FrameTypeS[2].s==ISS_ON) SetFrameType(FRAME_TYPE_DARK);
            if(FrameTypeS[3].s==ISS_ON) SetFrameType(FRAME_TYPE_FLAT);


            IDSetSwitch(FrameTypeSV,NULL);
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
    IDSetNumber(ImageFrameNV,NULL);


    ImageExposureNV->s=IPS_OK;
    IDSetNumber(ImageExposureNV,NULL);

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
        IDSetNumber(ImageFrameNV,NULL);

        //IDLog("Sending guider stream blob\n");

        GuiderExposureNV->s=IPS_OK;
        IDSetNumber(GuiderExposureNV,NULL);
        GuiderB.blob=RawGuiderFrame;
        GuiderB.bloblen=GSubW*GSubH;
        GuiderB.size=GSubW*GSubH;
        strcpy(GuiderB.format,"->stream");
        GuiderBV->s=IPS_OK;
        IDSetBLOB(GuiderBV,NULL);
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
        GuiderExposureNV->s=IPS_OK;
        IDSetNumber(GuiderExposureNV,NULL);

        IDLog("Sending guider fits file via %s\n",GuiderBV->name);
        GuiderB.blob=memptr;
        GuiderB.bloblen=memsize;
        GuiderB.size=memsize;
        strcpy(GuiderB.format,".fits");
        GuiderBV->s=IPS_OK;
        IDSetBLOB(GuiderBV,NULL);
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
    yw=SubH/BinY;

    //GuiderNV->s=IPS_OK;
    //GuiderN[0].value=xw;
    //GuiderN[1].value=yw;
    //IDLog("Guider Frame %d x %d\n",xw,yw);
    //IDSetNumber(&GuiderNV,"Starting to send new preview frame\n");

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
    FitsBV->s=IPS_OK;
    strcpy(FitsB.format,".ccdpreview");
    IDSetBLOB(FitsBV,NULL);
    //IDLog("Done with SetBlob with %d compressed to %d\n",numbytes,compressedbytes);
    //free(compressed);
    return 0;

}

int INDI::CCD::uploadfile(void *fitsdata,int total)
{
    //  lets try sending a ccd preview


    //IDLog("Enter Uploadfile with %d total sending via %s\n",total,FitsBV->name);
    FitsB.blob=fitsdata;
    FitsB.bloblen=total;
    FitsB.size=total;
    strcpy(FitsB.format,".fits");
    FitsBV->s=IPS_OK;
    IDSetBLOB(FitsBV,NULL);
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
int INDI::CCD::SetFrameType(int t)
{
    //fprintf(stderr,"Set frame type to %d\n",t);
    FrameType=t;
    return 0;
}
