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
    SubX=0;
    SubY=0;
    SubW=1392;
    SubH=1040;
    BinX=1;
    BinY=1;
    Interlaced=false;
    RawFrame = NULL;
}

SxCam::~SxCam()
{
    //dtor
    if(RawFrame != NULL) delete RawFrame;
}

const char * SxCam::getDefaultName()
{
    return (char *)"SX CCD";
}

bool SxCam::Connect()
{
    IDLog("Checking for SXV-H9\n");

    //dev=FindDevice(0x1278,0x0119,0);

    dev=FindDevice(0x1278,0x0507,0);
    if(dev==NULL)
    {
        IDLog("No SXV-H9 found\n");
        IDMessage(deviceName(), "Error: No SXV-H9 found.");
        return false;
    }
    IDLog("Found an SXV-H9\n");
    usb_handle=usb_open(dev);
    if(usb_handle != NULL)
    {
        int rc;

        rc=FindEndpoints();

        usb_detach_kernel_driver_np(usb_handle,0);
        IDLog("Detach Kernel returns %d\n",rc);
        //rc=usb_set_configuration(usb_handle,1);
        IDLog("Set Configuration returns %d\n",rc);

        rc=usb_claim_interface(usb_handle,1);
        IDLog("claim interface returns %d\n",rc);
        if(rc==0)
        {
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

            BinX = ImageBinN[0].value;
            BinY = ImageBinN[1].value;
            IDSetNumber(ImageBinNP, NULL);
            //  Fill in parent ccd values
            //  Initialize for doing full frames

            if (RawFrame != NULL)
                delete RawFrame;

            RawFrameSize=XRes*YRes;                 //  this is pixel count
            RawFrameSize=RawFrameSize*2;            //  Each pixel is 2 bytes
            RawFrameSize+=512;                      //  leave a little extra at the end
            RawFrame=new char[RawFrameSize];
            RawData = new char[RawFrameSize];

            if((parms.extra_caps & SXCCD_CAPS_GUIDER)==SXCCD_CAPS_GUIDER)
            {
                IDLog("Camera has a guide head attached\n");
                struct t_sxccd_params gparms;
                rc=GetCameraParams(1,&gparms);

                //HasGuideHead=true;

                IDLog("Guider is %d x %d with %d bpp  size %4.2f x %4.2f Matrix %x\n",
                      gparms.width,gparms.height,gparms.bits_per_pixel,gparms.pix_width,gparms.pix_height,gparms.color_matrix);

                IDMessage(deviceName(), "Guider is %d x %d with %d bpp  size %4.2f x %4.2f Matrix %x",
                          gparms.width,gparms.height,gparms.bits_per_pixel,gparms.pix_width,gparms.pix_height,gparms.color_matrix);

                IDLog("Guider capabilities %x\n",gparms.extra_caps);

                IDMessage(deviceName(), "Guider capabilities %x",gparms.extra_caps);

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
     if(tval < 1)
         tval=1;
     if(tval > 250)
         tval=250;

     IDLog("Cleared both fields, setting timer to %d\n", tval);

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

int SxCam::ReadCameraFrame(int index, char *buf)
{
    int rc;
    int numbytes;
    //static int expCount=0;

    double timesince;
    struct timeval start;
    struct timeval end;

    gettimeofday(&start,NULL);

    if(index==IMAGE_CCD)
    {
        if (Interlaced)
        {
            numbytes=SubW*SubH/BinX/BinY/4;
            //numbytes= 28672 + expCount;

            //expCount += 64;

            IDLog("Total Bytes: %d - SubW: %d - SubH: %d - BinX: %d - BinY: %d\n", numbytes, SubW, SubH, BinX, BinY);

            IDLog("Latching EVEN lines...\n");
            rc=LatchPixels(SXCCD_EXP_FLAGS_FIELD_EVEN,IMAGE_CCD,SubX,SubY,SubW,SubH,BinX,BinY);
            IDLog("Latch result: %d. Reading EVEN lines...\n", rc);
            rc=ReadPixels(buf,numbytes);
            IDLog("Latching ODD lines...\n");
            rc=LatchPixels(SXCCD_EXP_FLAGS_FIELD_ODD,IMAGE_CCD,SubX,SubY,SubW,SubH,BinX,BinY);
            IDLog("Latch result: %d. Reading ODD lines...\n", rc);
            rc=ReadPixels(buf,numbytes);



        }
        else
        {
            numbytes=SubW*SubH/BinX/BinY;
            IDLog("SubW: %d - SubH: %d - BinX: %d - BinY: %d\n",SubW, SubH, BinX, BinY);
        IDLog("Download Starting for %d\n",numbytes);
        rc=ReadPixels(buf,numbytes);
        }
    } else
    {
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

                        if (Interlaced == false)
                        {
                        IDLog("Image Pixels latched with rc=%d\n", rc);
                        rc=LatchPixels(SXCCD_EXP_FLAGS_FIELD_BOTH,IMAGE_CCD,SubX,SubY,SubW,SubH,BinX,BinY);
                        }

                    //rc=LatchPixels(0,GUIDE_CCD,GSubX,GSubY,GSubW,GSubH,1,1);
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
                    rc=LatchPixels(SXCCD_EXP_FLAGS_FIELD_EVEN | SXCCD_EXP_FLAGS_NOCLEAR_FRAME,GUIDE_CCD,GSubX,GSubY,GSubW,GSubH,1,1);
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
        rc=ReadCameraFrame(IMAGE_CCD,RawData);
        IDLog("Read camera frame with rc=%d\n", rc);
        rc=ProcessRawData(RawFrame, RawData);
        IDLog("processed raw data with rc=%d\n", rc);
        DidLatch=0;
        InExposure=false;


        ExposureComplete();
        //  if we get here, we quite likely ignored a guider hit
        if(InGuideExposure) SetTimer(1);    //  just make it all run again

    }
    if(DidGuideLatch==1)
    {
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

int SxCam::ProcessRawData(char *imageData, char *rawData)
{
    int xsize, ysize, npixels;
    char *rawptr, *dataptr;

    // FIXME TEMP ONLY
    imageData = rawData;
    return 0;

    rawptr = rawData;
    dataptr = imageData;

    xsize = SubW;

    if (Interlaced)
        ysize = SubH / 2;
    else
        ysize = SubH;

    npixels = xsize * ysize / BinX / BinY;

    if (CameraModel == 39)
    { // CMOS guider -- crop and clean
            int x, y, val;
            int oddbias, evenbias;

            for (y=0; y<SubH; y++)
            {
                    oddbias = evenbias = 0;
                    for (x=0; x<16; x+=2)
                    { // Figure the offsets for this line
                            oddbias += (int) *rawptr++;
                            evenbias += (int) *rawptr++;
                    }
                    oddbias = oddbias / 8 - 1000;  // Create avg and pre-build in the offset to keep off of the floor
                    evenbias = evenbias / 8 - 1000;
                    for (x=0; x<SubW; x+=2)
                    { // Load value into new image array pulling out right bias
                            val = (int) *rawptr++ - oddbias;
                            if (val < 0.0) val = 0.0;  //Bounds check
                            else if (val > 65535.0) val = 65535.0;
                            *dataptr++ = (unsigned short) val;
                            val = (int) *rawptr++ - evenbias;
                            if (val < 0.0) val = 0.0;  //Bounds check
                            else if (val > 65535.0) val = 65535.0;
                            *dataptr++ = (unsigned short) val;
                    }
            }
    }
    else if (Interlaced)
    {  // recon 1x2 bin into full-frame
            unsigned int x,y;
            for (y=0; y<ysize; y++)
            {  // load into image w/skips
                    for (x=0; x<xsize; x++, rawptr++, dataptr++)
                    {
                            *dataptr = (*rawptr);
                    }
                    dataptr += xsize;
            }
            // interpolate
            dataptr = imageData + xsize;
            for (y=0; y<(ysize - 1); y++)
            {
                    for (x=0; x<xsize; x++, dataptr++)
                            *dataptr = ( *(dataptr - xsize) + *(dataptr + xsize) ) / 2;
                    dataptr += xsize;
            }
            for (x=0; x<xsize; x++, dataptr++)
                    *dataptr =  *(dataptr - xsize);

    }
    else {  // Progressive
            for (int i=0; i<npixels; i++, rawptr++, dataptr++)
            {
                    *dataptr = (*rawptr);
            }
    }



    return 0;
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
    char Name[MAXINDILABEL];
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
    rc=ReadBulk((char *)&CameraModel,2,1000);
    IDLog("GetCameraModel returns %d\n",CameraModel);

    snprintf(Name, MAXINDILABEL, "SXV-%c%d%c", CameraModel & 0x40 ? 'M' : 'H', CameraModel & 0x1F, CameraModel & 0x80 ? 'C' : '\0');

    SubType = CameraModel & 0x1F;

    if (CameraModel & 0x80)  // color
            ColorSensor = true;
    else
            ColorSensor = false;

    if (CameraModel & 0x40)
            Interlaced = true;
    else
            Interlaced = false;

    if (SubType == 25)
            Interlaced = false;

    if (Interlaced)
        IDMessage(deviceName(), "Detected interlaced camera %s", Name);
    else
        IDMessage(deviceName(), "Detected progressive camera %s", Name);

    return CameraModel;
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

    unsigned char output_data[17];

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
    rc=ReadBulk((char *)output_data,17,1000);
    if(rc != 17) {
        IDLog("Bad camera parameters readout\n");
        return -1;
    }

    for (int i=0; i < 17; i++)
    {
        IDLog("OUTPUT[%d]= %#X -- %d\n", i, output_data[i], (int) output_data[i]);
    }
    params->hfront_porch = output_data[0];
    params->hback_porch = output_data[1];

    IDLog("width raw numbers %d %d\n",output_data[2],output_data[3]);

    params->width = output_data[2] | (output_data[3] << 8);
    params->vfront_porch = output_data[4];
    params->vback_porch = output_data[5];

    IDLog("height raw numbers %d %d\n",output_data[6],output_data[7]);

    params->height = output_data[6] | (output_data[7] << 8);
    params->pix_width = (output_data[8] | (output_data[9] << 8)) / 256.0;
    params->pix_height = (output_data[10] | (output_data[11] << 8)) / 256.0;

    // Interlaced cameras only report half the field and double the pixel height
    // So correct for it.
    /*if (Interlaced)
    {
        int pixAspect = floor((params->pix_height / params->pix_width) + 0.5);
        params->pix_height /= pixAspect;
        params->height *= pixAspect;
    }*/

    params->color_matrix = output_data[12] | (output_data[13] << 8);
    params->bits_per_pixel = output_data[14];
    params->num_serial_ports = output_data[15];
    params->extra_caps = output_data[16];
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

    IDLog("Latch pixels: xoffset: %d - yoffset: %d - Width: %d - Height: %d - xbin: %d - ybin: %d\n", xoffset, yoffset, width, height, xbin, ybin);

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

    IDLog("Expose pixels: xoffset: %d - yoffset: %d - Width: %d - Height: %d - xbin: %d - ybin: %d - delay: %d\n", xoffset, yoffset, width, height, xbin, ybin, msec);
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

    // Let's set them all to WHITE
    //for (int i=0; i<count; i++)
      //  pixels[i] = 255;

    total=count;
    //  round to divisible by 256
    //total=total/256;
    //total++;
    //total=total*256;

    rc=ReadBulk(pixels,total,10000);
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
