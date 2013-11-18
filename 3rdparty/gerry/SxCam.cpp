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
#include "SxCam.h"


IndiDevice * _create_device()
{
    IndiDevice *cam;
    IDLog("Create an sx camera device\n");
    cam=new SxCam();
    return cam;
}

SxCam::SxCam()
{
    //ctor
    SubX=0;
    SubY=0;
    SubW=1392;
    SubH=1040;
    BinX=1;
    BinY=1;
}

SxCam::~SxCam()
{
    //dtor
    if(RawFrame != NULL) delete RawFrame;
}

char * SxCam::getDefaultName()
{
    return (char *)"SxCamera";
}

bool SxCam::Connect()
{
    IDLog("Checking for SXV-H9\n");
    dev=FindDevice(0x1278,0x0119,0);
    if(dev==NULL) {
        IDLog("No SXV-H9 found");
        return false;
    }
    IDLog("Found an SXV-H9\n");
    usb_handle=usb_open(dev);
    if(usb_handle != NULL) {
        int rc;

        rc=FindEndpoints();

        usb_detach_kernel_driver_np(usb_handle,0);
        IDLog("Detach Kernel returns %d\n",rc);
        rc=usb_set_configuration(usb_handle,1);
        IDLog("Set Configuration returns %d\n",rc);

        rc=usb_claim_interface(usb_handle,1);
        IDLog("claim interface returns %d\n",rc);
        if(rc==0) {
            //  ok, we have the camera now
            //  Lets see what it really is
            rc=ResetCamera();
            rc=GetCameraModel();
            rc=GetFirmwareVersion();

            struct t_sxccd_params parms;
            rc=GetCameraParams(0,&parms);

            IDLog("Camera is %d x %d with %d bpp  size %4.2f x %4.2f Matrix %x\n",
                  parms.width,parms.height,parms.bits_per_pixel,parms.pix_width,parms.pix_height,parms.color_matrix);

            IDLog("Camera capabilities %x\n",parms.extra_caps);
            IDLog("Camera has %d serial ports\n",parms.num_serial_ports);

            SetCCDParams(parms.width,parms.height,parms.bits_per_pixel,parms.pix_width,parms.pix_height);

            //  Fill in parent ccd values
            //  Initialize for doing full frames

            RawFrameSize=XRes*YRes;                 //  this is pixel count
            RawFrameSize=RawFrameSize*2;            //  Each pixel is 2 bytes
            RawFrameSize+=512;                      //  leave a little extra at the end
            RawFrame=new char[RawFrameSize];


            if((parms.extra_caps & SXCCD_CAPS_GUIDER)==SXCCD_CAPS_GUIDER) {
                IDLog("Camera has a guide head attached\n");
                struct t_sxccd_params gparms;
                rc=GetCameraParams(1,&gparms);

                //HasGuideHead=true;

                IDLog("Guider is %d x %d with %d bpp  size %4.2f x %4.2f Matrix %x\n",
                      gparms.width,gparms.height,gparms.bits_per_pixel,gparms.pix_width,gparms.pix_height,gparms.color_matrix);
                IDLog("Guider capabilities %x\n",gparms.extra_caps);

                SetGuidHeadParams(gparms.width,gparms.height,gparms.bits_per_pixel,gparms.pix_width,gparms.pix_height);

                RawGuideSize=GXRes*GYRes;
                RawGuideSize=RawGuideSize*2;
                RawGuiderFrame=new char[RawGuideSize];

            }
            return true;
        }
    }
    return false;
}

bool SxCam::Disconnect()
{
    usb_close(usb_handle);
    return true;
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

    ClearPixels(SXCCD_EXP_FLAGS_FIELD_BOTH,IMAGE_CCD);
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

int SxCam::StartGuideExposure(float n)
{



    GuideExposureRequest=n;

    IDLog("Start guide exposure %4.2f\n",n);

    if(InGuideExposure) {
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

int SxCam::ReadCameraFrame(int index, char *buf)
{
    int rc;
    int numbytes;

    double timesince;
    struct timeval start;
    struct timeval end;

    gettimeofday(&start,NULL);

    if(index==IMAGE_CCD) {
        numbytes=SubW*SubH/BinX/BinY;
        numbytes=numbytes*2;
        IDLog("Download Starting for %d\n",numbytes);
        rc=ReadPixels(buf,numbytes);
    } else {
        numbytes=GSubW*GSubH;
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

void SxCam::TimerHit()
{
    float timeleft;
    int rc;
    bool IgnoreGuider=false;

    IDLog("SXCam Timer \n");

    //  If this is a relatively long exposure
    //  and its nearing the end, but not quite there yet
    //  We want to flush the accumulators

    if(InExposure) {
        timeleft=CalcTimeLeft();

        if((timeleft < 3) && (timeleft > 2) && (DidFlush==0)&&(InExposure)) {
            //  This will clear accumulators, but, not affect the
            //  light sensative parts currently exposing
            IDLog("Doing Flush\n");
            ClearPixels(SXCCD_EXP_FLAGS_NOWIPE_FRAME,IMAGE_CCD);
            DidFlush=1;
        }
        if(timeleft < 1.0) {
            IgnoreGuider=true;
            if(timeleft > 0.25) {
                //  a quarter of a second or more
                //  just set a tighter timer
                SetTimer(250);
            } else {
                if(timeleft >0.07) {
                    //  use an even tighter timer
                    SetTimer(50);
                } else {
                    //  it's real close now, so spin on it
                    while(timeleft > 0) {
                        int slv;
                        slv=100000*timeleft;
                        //IDLog("usleep %d\n",slv);
                        usleep(slv);
                        timeleft=CalcTimeLeft();
                    }
                    //  first a flush
                    rc=LatchPixels(SXCCD_EXP_FLAGS_FIELD_BOTH,IMAGE_CCD,SubX,SubY,SubW,SubH,BinX,BinY);
                    //rc=LatchPixels(0,GUIDE_CCD,GSubX,GSubY,GSubW,GSubH,1,1);
                    DidLatch=1;
                    IDLog("Image Pixels latched\n");
                }
            }
        } else {
            if(!InGuideExposure) SetTimer(250);
        }
    }

    if(!IgnoreGuider) {
        if(InGuideExposure) {
            timeleft=CalcGuideTimeLeft();
            if(timeleft < 0.25) {
                if(timeleft < 0.10) {
                    while(timeleft > 0) {
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
                    rc=LatchPixels(SXCCD_EXP_FLAGS_FIELD_EVEN | SXCCD_EXP_FLAGS_NOCLEAR_FRAME,GUIDE_CCD,GSubX,GSubY,GSubW,GSubH,1,1);
                    //rc=LatchPixels(SXCCD_EXP_FLAGS_FIELD_BOTH ,GUIDE_CCD,GSubX,GSubY,GSubW,GSubH,1,1);
                    DidGuideLatch=1;
                    IDLog("Guide Even Pixels latched\n");

                } else {
                    SetTimer(100);
                }
            } else {
                SetTimer(250);
            }
        }
    }

    if(DidLatch==1) {
        //  Pixels have been latched
        //  now download them
        int rc;
        rc=ReadCameraFrame(IMAGE_CCD,RawFrame);
        DidLatch=0;
        InExposure=false;
        ExposureComplete();
        //  if we get here, we quite likely ignored a guider hit
        if(InGuideExposure) SetTimer(1);    //  just make it all run again

    }
    if(DidGuideLatch==1) {
        int rc;
        rc=ReadCameraFrame(GUIDE_CCD,RawGuiderFrame);
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




int SxCam::ResetCamera()
{
    char setup_data[8];
    int rc;

    setup_data[USB_REQ_TYPE    ] = USB_REQ_VENDOR | USB_REQ_DATAOUT;
    setup_data[USB_REQ         ] = SXUSB_RESET;
    setup_data[USB_REQ_VALUE_L ] = 0;
    setup_data[USB_REQ_VALUE_H ] = 0;
    setup_data[USB_REQ_INDEX_L ] = 0;
    setup_data[USB_REQ_INDEX_H ] = 0;
    setup_data[USB_REQ_LENGTH_L] = 0;
    setup_data[USB_REQ_LENGTH_H] = 0;
    //return (WriteFile(sxHandle, setup_data, sizeof(setup_data), &written, NULL));
    rc=WriteBulk(setup_data,8,1000);
    IDLog("ResetCamera WriteBulk returns %d\n",rc);
    if(rc==8) return 0;
    return -1;
}

int SxCam::GetCameraModel()
{
    char setup_data[8];
    unsigned short int model;
    int rc;

    setup_data[USB_REQ_TYPE    ] = USB_REQ_VENDOR | USB_REQ_DATAIN;
    setup_data[USB_REQ         ] = SXUSB_CAMERA_MODEL;
    setup_data[USB_REQ_VALUE_L ] = 0;
    setup_data[USB_REQ_VALUE_H ] = 0;
    setup_data[USB_REQ_INDEX_L ] = 0;
    setup_data[USB_REQ_INDEX_H ] = 0;
    setup_data[USB_REQ_LENGTH_L] = 2;
    setup_data[USB_REQ_LENGTH_H] = 0;
    rc=WriteBulk(setup_data,8,1000);
    rc=ReadBulk((char *)&model,2,1000);
    IDLog("GetCameraModel returns %d\n",model);
    return model;
}

int SxCam::GetFirmwareVersion()
{
    char setup_data[8];
    unsigned int ver;
    int rc;

    setup_data[USB_REQ_TYPE    ] = USB_REQ_VENDOR | USB_REQ_DATAIN;
    setup_data[USB_REQ         ] = SXUSB_GET_FIRMWARE_VERSION;
    setup_data[USB_REQ_VALUE_L ] = 0;
    setup_data[USB_REQ_VALUE_H ] = 0;
    setup_data[USB_REQ_INDEX_L ] = 0;
    setup_data[USB_REQ_INDEX_H ] = 0;
    setup_data[USB_REQ_LENGTH_L] = 4;
    setup_data[USB_REQ_LENGTH_H] = 0;

    rc=WriteBulk(setup_data,8,1000);
    ver=0;
    rc=ReadBulk((char *)&ver,4,1000);
    IDLog("GetFirmwareVersion returns %x\n",ver);
    return ver;
}

int SxCam::GetCameraParams(int index,PCCDPARMS params)
{
    unsigned char setup_data[17];
    int rc;

    setup_data[USB_REQ_TYPE    ] = USB_REQ_VENDOR | USB_REQ_DATAIN;
    setup_data[USB_REQ         ] = SXUSB_GET_CCD;
    setup_data[USB_REQ_VALUE_L ] = 0;
    setup_data[USB_REQ_VALUE_H ] = 0;
    setup_data[USB_REQ_INDEX_L ] = index;
    setup_data[USB_REQ_INDEX_H ] = 0;
    setup_data[USB_REQ_LENGTH_L] = 17;
    setup_data[USB_REQ_LENGTH_H] = 0;
    rc=WriteBulk((char *)setup_data,17,1000);
    rc=ReadBulk((char *)setup_data,17,1000);
    if(rc != 17) {
        IDLog("Bad camera parameters readout\n");
        return -1;
    }
    params->hfront_porch = setup_data[0];
    params->hback_porch = setup_data[1];

    IDLog("width raw numbers %d %d\n",setup_data[2],setup_data[3]);

    params->width = setup_data[2] | (setup_data[3] << 8);
    params->vfront_porch = setup_data[4];
    params->vback_porch = setup_data[5];

    IDLog("height raw numbers %d %d\n",setup_data[6],setup_data[7]);

    params->height = setup_data[6] | (setup_data[7] << 8);
    params->pix_width = (setup_data[8] | (setup_data[9] << 8)) / 256.0;
    params->pix_height = (setup_data[10] | (setup_data[11] << 8)) / 256.0;
    params->color_matrix = setup_data[12] | (setup_data[13] << 8);
    params->bits_per_pixel = setup_data[14];
    params->num_serial_ports = setup_data[15];
    params->extra_caps = setup_data[16];
    return (0);
}

int SxCam::ClearPixels(int flags,int camIndex)
{
    char setup_data[8];
    int rc;

    setup_data[USB_REQ_TYPE    ] = USB_REQ_VENDOR | USB_REQ_DATAOUT;
    setup_data[USB_REQ         ] = SXUSB_CLEAR_PIXELS;
    setup_data[USB_REQ_VALUE_L ] = flags;
    setup_data[USB_REQ_VALUE_H ] = flags >> 8;
    setup_data[USB_REQ_INDEX_L ] = camIndex;
    setup_data[USB_REQ_INDEX_H ] = 0;
    setup_data[USB_REQ_LENGTH_L] = 0;
    setup_data[USB_REQ_LENGTH_H] = 0;
    rc=WriteBulk(setup_data,8,1000);
    return 0;
}

int SxCam::LatchPixels(int flags,int camIndex,int xoffset,int yoffset,int width,int height,int xbin,int ybin)
{
    char setup_data[18];
    int rc;

    setup_data[USB_REQ_TYPE    ] = USB_REQ_VENDOR | USB_REQ_DATAOUT;
    setup_data[USB_REQ         ] = SXUSB_READ_PIXELS;
    setup_data[USB_REQ_VALUE_L ] = flags;
    setup_data[USB_REQ_VALUE_H ] = flags >> 8;
    setup_data[USB_REQ_INDEX_L ] = camIndex;
    setup_data[USB_REQ_INDEX_H ] = 0;
    setup_data[USB_REQ_LENGTH_L] = 10;
    setup_data[USB_REQ_LENGTH_H] = 0;
    setup_data[USB_REQ_DATA + 0] = xoffset & 0xFF;
    setup_data[USB_REQ_DATA + 1] = xoffset >> 8;
    setup_data[USB_REQ_DATA + 2] = yoffset & 0xFF;
    setup_data[USB_REQ_DATA + 3] = yoffset >> 8;
    setup_data[USB_REQ_DATA + 4] = width & 0xFF;
    setup_data[USB_REQ_DATA + 5] = width >> 8;
    setup_data[USB_REQ_DATA + 6] = height & 0xFF;
    setup_data[USB_REQ_DATA + 7] = height >> 8;
    setup_data[USB_REQ_DATA + 8] = xbin;
    setup_data[USB_REQ_DATA + 9] = ybin;
    rc=WriteBulk(setup_data,18,1000);

    return 0;
}

int SxCam::ExposePixels(int flags,int camIndex,int xoffset,int yoffset,int width,int height,int xbin,int ybin,int msec)
{
    char setup_data[22];
    int rc;

    setup_data[USB_REQ_TYPE    ] = USB_REQ_VENDOR | USB_REQ_DATAOUT;
    setup_data[USB_REQ         ] = SXUSB_READ_PIXELS_DELAYED;
    setup_data[USB_REQ_VALUE_L ] = flags;
    setup_data[USB_REQ_VALUE_H ] = flags >> 8;
    setup_data[USB_REQ_INDEX_L ] = camIndex;
    setup_data[USB_REQ_INDEX_H ] = 0;
    setup_data[USB_REQ_LENGTH_L] = 10;
    setup_data[USB_REQ_LENGTH_H] = 0;
    setup_data[USB_REQ_DATA + 0] = xoffset & 0xFF;
    setup_data[USB_REQ_DATA + 1] = xoffset >> 8;
    setup_data[USB_REQ_DATA + 2] = yoffset & 0xFF;
    setup_data[USB_REQ_DATA + 3] = yoffset >> 8;
    setup_data[USB_REQ_DATA + 4] = width & 0xFF;
    setup_data[USB_REQ_DATA + 5] = width >> 8;
    setup_data[USB_REQ_DATA + 6] = height & 0xFF;
    setup_data[USB_REQ_DATA + 7] = height >> 8;
    setup_data[USB_REQ_DATA + 8] = xbin;
    setup_data[USB_REQ_DATA + 9] = ybin;
    setup_data[USB_REQ_DATA + 10] = msec;
    setup_data[USB_REQ_DATA + 11] = msec >> 8;
    setup_data[USB_REQ_DATA + 12] = msec >> 16;
    setup_data[USB_REQ_DATA + 13] = msec >> 24;

    rc=WriteBulk(setup_data,22,1000);
    return 0;
}

int SxCam::ReadPixels(char *pixels,int count)
{
    int rc;
    int total;

    total=count;
    //  round to divisible by 256
    //total=total/256;
    //total++;
    //total=total*256;

    rc=ReadBulk(pixels,total,35000);
    //total+=rc;
    //if(rc > count) rc=count;
    IDLog("Read Pixels request %d got %d\n",count,rc);

	//FILE *h = fopen("rawcam.dat", "w+");
	//fwrite(pixels, count, 1, h);
	//fclose(h);
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
