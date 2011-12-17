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
#include "sxcam.h"

// We declare an auto pointer to sxcamera.
std::auto_ptr<SxCam> sxcamera(0);

void ISInit()
{
   static int isInit =0;

   if (isInit == 1)
       return;

    isInit = 1;
    if(sxcamera.get() == 0) sxcamera.reset(new SxCam());
    //IEAddTimer(POLLMS, ISPoll, NULL);

}

void ISGetProperties(const char *dev)
{
        ISInit();
        sxcamera->ISGetProperties(dev);
}

void ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int num)
{
        ISInit();
        sxcamera->ISNewSwitch(dev, name, states, names, num);
}

void ISNewText(	const char *dev, const char *name, char *texts[], char *names[], int num)
{
        ISInit();
        sxcamera->ISNewText(dev, name, texts, names, num);
}

void ISNewNumber(const char *dev, const char *name, double values[], char *names[], int num)
{
        ISInit();
        sxcamera->ISNewNumber(dev, name, values, names, num);
}

void ISNewBLOB (const char *dev, const char *name, int sizes[], int blobsizes[], char *blobs[], char *formats[], char *names[], int n)
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
void ISSnoopDevice (XMLEle *root)
{
    INDI_UNUSED(root);
}

SxCam::SxCam()
{
    //ctor
    HasGuideHead=false;
    evenBuf = NULL;
    oddBuf  = NULL;
}

SxCam::~SxCam()
{
    //dtor
    usb_close(usb_handle);
}


const char * SxCam::getDefaultName()
{
    return (char *)"SX CCD";
}

bool SxCam::Connect()
{
    bool rc;
    IDLog("Calling sx connect\n");
    rc=SxCCD::Connect();
    return rc;
    /*if(rc)
    {
        IDLog("Calling Set CCD %d %d\n",XRes,YRes);
        SetCCDParams(XRes,YRes,CamBits,pixwidth,pixheight);
        if(HasGuideHead) {
            printf("Guide head detected\n");
            SetGuidHeadParams(GXRes,GYRes,GuiderBits,gpixwidth,gpixheight);
        } else {
            printf("no guide head\n");
        }
    }
    return rc;*/
}

bool SxCam::Disconnect()
{
    bool rc;
    rc=SxCCD::Disconnect();
    return rc;
}

float SxCam::CalcTimeLeft()
{
    double timesince;
    double timeleft;
    struct timeval now;
    gettimeofday(&now,NULL);

    timesince=(double)(now.tv_sec * 1000.0 + now.tv_usec/1000) - (double)(ExpStart.tv_sec * 1000.0 + ExpStart.tv_usec/1000);
    timesince=timesince/1000;


    timeleft=ExposureRequest-timesince;
    return timeleft;
}
float SxCam::CalcGuideTimeLeft()
{
    double timesince;
    double timeleft;
    struct timeval now;
    gettimeofday(&now,NULL);

    timesince=(double)(now.tv_sec * 1000.0 + now.tv_usec/1000) - (double)(GuideExpStart.tv_sec * 1000.0 + GuideExpStart.tv_usec/1000);
    timesince=timesince/1000;


    timeleft=GuideExposureRequest-timesince;
    return timeleft;
}

int SxCam::StartExposure(float n)
{
    ExposureRequest=n;
    gettimeofday(&ExpStart,NULL);
    InExposure=true;

    //  Clear the pixels to start a fresh exposure
    //  calling here with no parameters flushes both
    //  the accumulators and the light sensative portions
    DidFlush=0;
    DidLatch=0;

    if (PrimaryCCD.isInterlaced() && PrimaryCCD.getBinY() == 1)
    {
        ClearPixels(SXCCD_EXP_FLAGS_FIELD_EVEN,IMAGE_CCD);

        // TODO: Add Delay here

        ClearPixels(SXCCD_EXP_FLAGS_FIELD_ODD,IMAGE_CCD);

    }
    else
        ClearPixels(SXCCD_EXP_FLAGS_FIELD_BOTH,IMAGE_CCD);

    //  Relatively long exposure
     //  lets do it on our own timers
     int tval;
     tval=n*1000;
     tval=tval-50;
     if(tval < 1)
         tval=1;
     if(tval > 250)
         tval=250;

     IDLog("Cleared all fields, setting timer to %d\n", tval);

     SetTimer(tval);

    return 0;
}

int SxCam::StartGuideExposure(float n)
{
    GuideExposureRequest=n;

    IDLog("Start guide exposure %4.2f\n",n);

    if(InGuideExposure)
    {
        //  We already have an exposure running
        //  so we just change the exposure time
        //  and return
        return true;
    }

    gettimeofday(&GuideExpStart,NULL);
    InGuideExposure=true;

    //  Clear the pixels to start a fresh exposure
    //  calling here with no parameters flushes both
    //  the accumulators and the light sensative portions
    DidGuideLatch=0;

        //ClearPixels(SXCCD_EXP_FLAGS_FIELD_BOTH,GUIDE_CCD);
        //  Relatively long exposure
        //  lets do it on our own timers
        int tval;
        tval=n*1000;
        tval=tval-50;
        if(tval < 1) tval=1;
        if(tval > 250) tval=250;
        SetTimer(tval);

    return 0;
}

bool SxCam::AbortGuideExposure()
{
    if(InGuideExposure) {
        InGuideExposure=false;
        return true;
    }
    return false;
}


void SxCam::TimerHit()
{
    float timeleft;
    int rc;
    bool IgnoreGuider=false;

    IDLog("SXCam Timer \n");

    //  If this is a relatively long exposure
    //  and its nearing the end, but not quite there yet
    //  We want to flush the accumulators

    if(InExposure)
    {
        timeleft=CalcTimeLeft();

        if((timeleft < 3) && (timeleft > 2) && (DidFlush==0)&&(InExposure))
        {
            //  This will clear accumulators, but, not affect the
            //  light sensative parts currently exposing
            IDLog("Doing Flush\n");
            ClearPixels(SXCCD_EXP_FLAGS_NOWIPE_FRAME,IMAGE_CCD);
            DidFlush=1;
        }

        if(timeleft < 1.0)
        {
            IgnoreGuider=true;
            if(timeleft > 0.25)
            {
                //  a quarter of a second or more
                //  just set a tighter timer
                SetTimer(250);
            } else
            {
                if(timeleft >0.07)
                {
                    //  use an even tighter timer
                    SetTimer(50);
                } else
                {
                    //  it's real close now, so spin on it
                    while(timeleft > 0)
                    {
                        int slv;
                        slv=100000*timeleft;
                        //IDLog("usleep %d\n",slv);
                        usleep(slv);
                        timeleft=CalcTimeLeft();
                    }

                    // If not interlaced, take full frame
                    // If interlaced, but vertical binning is > 1, then take full frame as well.
                    if (PrimaryCCD.isInterlaced() == false)
                        rc=LatchPixels(SXCCD_EXP_FLAGS_FIELD_BOTH,IMAGE_CCD,PrimaryCCD.getSubX(),PrimaryCCD.getSubY(),PrimaryCCD.getSubW(),
                                       PrimaryCCD.getSubH(),PrimaryCCD.getBinX(),PrimaryCCD.getBinY());
                    else if (PrimaryCCD.isInterlaced() && PrimaryCCD.getBinY() > 1)
                        rc=LatchPixels(SXCCD_EXP_FLAGS_FIELD_BOTH,IMAGE_CCD,PrimaryCCD.getSubX(),PrimaryCCD.getSubY(),PrimaryCCD.getSubW(),
                                       PrimaryCCD.getSubH()/2,PrimaryCCD.getBinX(),(PrimaryCCD.getBinY()/2));

                    DidLatch=1;

                }
            }
        } else
        {
            if(!InGuideExposure) SetTimer(250);
        }
    }

    if(!IgnoreGuider)
    {
        if(InGuideExposure)
        {
            timeleft=CalcGuideTimeLeft();
            if(timeleft < 0.25)
            {
                if(timeleft < 0.10)
                {
                    while(timeleft > 0)
                    {
                        int slv;
                        slv=100000*timeleft;
                        //IDLog("usleep %d\n",slv);
                        usleep(slv);
                        timeleft=CalcGuideTimeLeft();
                    }
                    //  first a flush
                    ClearPixels(SXCCD_EXP_FLAGS_NOWIPE_FRAME,GUIDE_CCD);
                    //  now latch the exposure
                    //rc=LatchPixels(SXCCD_EXP_FLAGS_FIELD_EVEN | SXCCD_EXP_FLAGS_NOWIPE_FRAME_FRAME,GUIDE_CCD,GSubX,GSubY,GSubW,GSubH,1,1);
                    rc=LatchPixels(SXCCD_EXP_FLAGS_FIELD_EVEN | SXCCD_EXP_FLAGS_NOCLEAR_FRAME,GUIDE_CCD,GuideCCD.getSubX(),GuideCCD.getSubY(),
                                   GuideCCD.getSubW(),GuideCCD.getSubH(),1,1);
                    //rc=LatchPixels(SXCCD_EXP_FLAGS_FIELD_BOTH ,GUIDE_CCD,GSubX,GSubY,GSubW,GSubH,1,1);
                    DidGuideLatch=1;
                    IDLog("Guide Even Pixels latched\n");

                } else
                {
                    SetTimer(100);
                }
            } else
            {
                SetTimer(250);
            }
        }
    }

    if(DidLatch==1)
    {
        //  Pixels have been latched
        //  now download them
        int rc;
        rc=ReadCameraFrame(IMAGE_CCD,PrimaryCCD.getFrameBuffer());
        IDLog("Read camera frame with rc=%d\n", rc);
        //rc=ProcessRawData(RawFrame, RawData);
        //IDLog("processed raw data with rc=%d\n", rc);
        DidLatch=0;
        InExposure=false;

/*
        if (StreamSP->s == IPS_BUSY)
        {
            sendPreview();
            StartExposure(0.5);
        }
        else
*/
            ExposureComplete();

        //  if we get here, we quite likely ignored a guider hit
        if(InGuideExposure) SetTimer(1);    //  just make it all run again

    }
    if(DidGuideLatch==1)
    {
        int rc;
        rc=ReadCameraFrame(GUIDE_CCD,PrimaryCCD.getFrameBuffer());
        DidGuideLatch=0;
        InGuideExposure=false;
        //  send half a frame
        GuideExposureComplete();

        //rc=LatchPixels(SXCCD_EXP_FLAGS_FIELD_ODD | SXCCD_EXP_FLAGS_NOWIPE_FRAME,GUIDE_CCD,GSubX,GSubY,GSubW,GSubH,1,1);
        //rc=LatchPixels(SXCCD_EXP_FLAGS_FIELD_ODD | SXCCD_EXP_FLAGS_NOWIPE_FRAME,GUIDE_CCD,GSubX,GSubY,GSubW,GSubH,1,1);
        //rc=ReadCameraFrame(GUIDE_CCD,RawGuiderFrame);
        //GuideExposureComplete();
        //ClearPixels(SXCCD_EXP_FLAGS_FIELD_BOTH,GUIDE_CCD);

    }
}

int SxCam::ReadCameraFrame(int index, char *buf)
{
    int rc;
    int numbytes, xwidth=0;
    //static int expCount=0;

    double timesince;
    struct timeval start;
    struct timeval end;

    gettimeofday(&start,NULL);

    if(index==IMAGE_CCD)
    {
        if (PrimaryCCD.isInterlaced() && PrimaryCCD.getBinY() > 1)
            numbytes= PrimaryCCD.getBPP() * PrimaryCCD.getSubW()*PrimaryCCD.getSubH()/2/PrimaryCCD.getBinX()/(PrimaryCCD.getBinY()/2);
        else
            numbytes= PrimaryCCD.getBPP() * PrimaryCCD.getSubW()*PrimaryCCD.getSubH()/PrimaryCCD.getBinX()/PrimaryCCD.getBinY();

        xwidth = PrimaryCCD.getSubW() * PrimaryCCD.getBPP();


        //IDLog("SubW: %d - SubH: %d - BinX: %d - BinY: %d CamBits %d\n",SubW, SubH, BinX, BinY,CamBits);

        if (PrimaryCCD.isInterlaced() && PrimaryCCD.getBinY() == 1)
        {

            rc=LatchPixels(SXCCD_EXP_FLAGS_FIELD_EVEN | SXCCD_EXP_FLAGS_NOBIN_ACCUM,IMAGE_CCD,PrimaryCCD.getSubX(),PrimaryCCD.getSubY(),
                           PrimaryCCD.getSubW(),PrimaryCCD.getSubH()/2,PrimaryCCD.getBinX(),1);

            // Let's read EVEN fields now
            //IDLog("EVEN FIELD: Read Starting for %d\n",numbytes);

            rc=ReadPixels(evenBuf,numbytes/2);

            //IDLog("EVEN FIELD: Read %d\n",rc);

            rc=LatchPixels(SXCCD_EXP_FLAGS_FIELD_ODD | SXCCD_EXP_FLAGS_NOBIN_ACCUM,IMAGE_CCD,PrimaryCCD.getSubX(),PrimaryCCD.getSubY(),
                           PrimaryCCD.getSubW(),PrimaryCCD.getSubH()/2,PrimaryCCD.getBinX(),1);

            //IDLog("bpp: %d - xwidth: %d - ODD FIELD: Read Starting for %d\n", (CamBits==16) ? 2 : 1, xwidth, numbytes);

            rc=ReadPixels(oddBuf,numbytes/2);

            //IDLog("ODD FIELD: Read %d\n",rc);

            int height = PrimaryCCD.getSubH();
            for (int i=0, j=0; i < height ; i+=2, j++)
            {
                memcpy(buf + i * xwidth, evenBuf + (j * xwidth), xwidth);
                memcpy(buf + ((i+1) * xwidth), oddBuf + (j*xwidth), xwidth);
            }

        }
        else
        {
                IDLog("non interlaced Read Starting for %d\n",numbytes);
                rc=ReadPixels(buf,numbytes);
        }
    } else
    {
        numbytes=GuideCCD.getSubW()*GuideCCD.getSubH();
        //numbytes=numbytes*2;
        IDLog("Download Starting for %d\n",numbytes);
        rc=ReadPixels(buf,numbytes);

    }

    gettimeofday(&end,NULL);

    timesince=(double)(end.tv_sec * 1000.0 + end.tv_usec/1000) - (double)(start.tv_sec * 1000.0 + start.tv_usec/1000);
    timesince=timesince/1000;

    IDLog("Download returns %d in %4.2f seconds\n",rc,timesince);
    return rc;
}


int SxCam::SetCamTimer(int msec)
{
    char setup_data[12];
    int rc;

    setup_data[USB_REQ_TYPE    ] = USB_REQ_VENDOR | USB_REQ_DATAOUT;
    setup_data[USB_REQ         ] = SXUSB_SET_TIMER;
    setup_data[USB_REQ_VALUE_L ] = 0;
    setup_data[USB_REQ_VALUE_H ] = 0;
    setup_data[USB_REQ_INDEX_L ] = 0;
    setup_data[USB_REQ_INDEX_H ] = 0;
    setup_data[USB_REQ_LENGTH_L] = 4;
    setup_data[USB_REQ_LENGTH_H] = 0;
    setup_data[USB_REQ_DATA + 0] = msec;
    setup_data[USB_REQ_DATA + 1] = msec >> 8;
    setup_data[USB_REQ_DATA + 2] = msec >> 16;
    setup_data[USB_REQ_DATA + 3] = msec >> 24;

    rc=WriteBulk(setup_data,12,1000);
    return 0;
}

int SxCam::GetCamTimer()
{
    char setup_data[8];
    unsigned int timer;
    int rc;

    setup_data[USB_REQ_TYPE    ] = USB_REQ_VENDOR | USB_REQ_DATAIN;
    setup_data[USB_REQ         ] = SXUSB_GET_TIMER;
    setup_data[USB_REQ_VALUE_L ] = 0;
    setup_data[USB_REQ_VALUE_H ] = 0;
    setup_data[USB_REQ_INDEX_L ] = 0;
    setup_data[USB_REQ_INDEX_H ] = 0;
    setup_data[USB_REQ_LENGTH_L] = 4;
    setup_data[USB_REQ_LENGTH_H] = 0;
    rc=WriteBulk(setup_data,8,1000);
    timer=0;
    rc=ReadBulk((char *)&timer,4,1000);
    return timer;
}

int SxCam::SetParams(int xres,int yres,int Bits,float pixwidth,float pixheight)
{
    IDLog("SxCam::Setparams %d %d\n",xres,yres);
    int nbuf;

    if (PrimaryCCD.isInterlaced())
    {
        pixheight /= 2;
        yres *= 2;
    }

   SetCCDParams(xres,yres,Bits,pixwidth,pixheight);

   nbuf=PrimaryCCD.getXRes()*PrimaryCCD.getYRes();                 //  this is pixel count
   if(Bits==16)
         nbuf *=2;            //  Each pixel is 2 bytes
   nbuf+=512;                      //  leave a little extra at the end

   PrimaryCCD.setFrameBufferSize(nbuf);

   if (evenBuf != NULL)
       delete evenBuf;
   if (oddBuf != NULL)
      delete oddBuf;

   evenBuf = new char[nbuf/2];
   oddBuf = new char[nbuf/2];
}

int SxCam::SetGuideParams(int gXRes,int gYRes,int gBits,float gpixwidth,float gpixheight)
{
    IDLog("SxCam::SetGuideparams %d %d\n",xres,yres);
    int nbuf=0;
    SetGuidHeadParams(gXRes,gYRes,gBits,gpixwidth,gpixheight);

    nbuf = GuideCCD.getXRes() * GuideCCD.getYRes();
    if(gparms.bits_per_pixel ==16)
        nbuf *= 2;

    GuideCCD.setFrameBufferSize(nbuf);
}

int SxCam::SetInterlaced(bool i)
{
    PrimaryCCD.setInterlaced(i);
    return 0;
}

bool SxCam::updateCCDBin(int hor, int ver)
{
    if (hor == 3 || ver ==3)
    {
        PrimaryCCD.setBin(1,1);
        IDMessage(deviceName(), "3x3 binning is not supported on this CCD. Valid modes are 1x1, 2x2, and 4x4.");
        return true;
    }

    return false;

}
