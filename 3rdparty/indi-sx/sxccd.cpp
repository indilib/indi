
#include "sxccd.h"


const int SX_PIDS[MODEL_COUNT] = { 0x0105, 0x0305, 0x0107, 0x0307, 0x0308, 0x0109, 0x0325, 0x0326, 0x0128, 0x0126, 0x0135, 0x0136, 0x0119, 0x0319, 0x0507, 0x0517 };


SxCCD::SxCCD()
{
    //ctor
    //Interlaced=false;
    //RawFrame = NULL;
    //RawGuiderFrame=NULL;
    //CamBits=8;
    //HasGuideHead=0;
    sx_hasguide=false;
    sx_hasST4=false;
    guide_cmd = 0;
}

bool SxCCD::Connect(int pid)
{
   if (pid == -1)
   {
    for (int i=0; i < MODEL_COUNT; i++)
    {
        dev=FindDevice(0x1278,SX_PIDS[i],0);
        if(dev != NULL)
            break;
    }
   }
   else
      dev=FindDevice(0x1278,SX_PIDS[pid],0);

   if(dev==NULL)
    {
        IDLog("Error: No SX Cameras found\n");
        return false;
    }
    usb_handle=usb_open(dev);
    if(usb_handle != NULL)
    {
        int rc;

        rc=FindEndpoints();

        usb_detach_kernel_driver_np(usb_handle,0);
        IDLog("Detach Kernel returns %d\n",rc);
        //rc=usb_set_configuration(usb_handle,1);
        IDLog("Set Configuration returns %d\n",rc);

#ifdef __APPLE__
        rc=usb_claim_interface(usb_handle,0);
#else
        rc=usb_claim_interface(usb_handle,1);
#endif
        fprintf(stderr,"claim interface returns %d\n",rc);

        getCapabilites();

        if (rc== 0)
            return true;
    }

   return false;
}

void SxCCD::getCapabilites()
{
    int rc=0;

        //  ok, we have the camera now
        //  Lets see what it really is
        rc=ResetCamera();
        rc=GetCameraModel();
        rc=GetFirmwareVersion();
        rc=GetCameraParams(0,&parms);


          IDLog("Camera is %d x %d with %d bpp  size %4.2f x %4.2f Matrix %x\n",
              parms.width,parms.height,parms.bits_per_pixel,parms.pix_width,parms.pix_height,parms.color_matrix);

          IDLog("Camera capabilities %x\n",parms.extra_caps);
          IDLog("Camera has %d serial ports\n",parms.num_serial_ports);


        pixwidth=parms.pix_width;
        pixheight=parms.pix_height;
        bits_per_pixel=parms.bits_per_pixel;
        xres=parms.width;
        yres=parms.height;

        if((parms.extra_caps & SXCCD_CAPS_GUIDER)==SXCCD_CAPS_GUIDER)
        {
            IDLog("Camera has a guide head attached\n");
            rc=GetCameraParams(1,&gparms);

            sx_hasguide=true;


            IDLog("Guider is %d x %d with %d bpp  size %4.2f x %4.2f Matrix %x\n",
                  gparms.width,gparms.height,gparms.bits_per_pixel,gparms.pix_width,gparms.pix_height,gparms.color_matrix);

            //IDMessage(deviceName(), "Guider is %d x %d with %d bpp  size %4.2f x %4.2f Matrix %x",
            //          gparms.width,gparms.height,gparms.bits_per_pixel,gparms.pix_width,gparms.pix_height,gparms.color_matrix);


            IDLog("Guider capabilities %x\n",gparms.extra_caps);

            gbits_per_pixel=gparms.bits_per_pixel;
            gxres=gparms.width;
            gyres=gparms.height;
            gpixwidth=gparms.pix_width;
            gpixheight=gparms.pix_height;

        }

        if ((parms.extra_caps & SXCCD_CAPS_STAR2K)==SXCCD_CAPS_STAR2K)
            sx_hasST4 = true;

}

void SxCCD::getDefaultParam()
{
     SetParams(xres,yres,bits_per_pixel,pixwidth,pixheight);

     if (sx_hasguide)
        SetGuideParams(gparms.width,gparms.height,gparms.bits_per_pixel,gparms.pix_width,gparms.pix_height);

}

bool SxCCD::Disconnect()
{
    usb_release_interface(usb_handle, 1);
    usb_close(usb_handle);
    return true;
}

int SxCCD::ResetCamera()
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
    fprintf(stderr,"ResetCamera WriteBulk returns %d\n",rc);
    if(rc==8) return 0;
    return -1;
}

int SxCCD::GetCameraModel()
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
    fprintf(stderr,"GetCameraModel returns %d\n",CameraModel);

    snprintf(Name, MAXINDILABEL, "SXV-%c%d%c", CameraModel & 0x40 ? 'M' : 'H', CameraModel & 0x1F, CameraModel & 0x80 ? 'C' : '\0');

    SubType = CameraModel & 0x1F;

    if (CameraModel & 0x80)  // color
            ColorSensor = true;
    else
            ColorSensor = false;


    if (CameraModel & 0x40)
            SetInterlaced(true);
    else
            SetInterlaced(false);

    if (SubType == 25)
            SetInterlaced(false);

    return CameraModel;
}

int SxCCD::GetFirmwareVersion()
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
    fprintf(stderr,"GetFirmwareVersion returns %x\n",ver);
    return ver;
}

int SxCCD::GetCameraParams(int index,PCCDPARMS params)
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
        fprintf(stderr,"Bad camera parameters readout\n");
        return -1;
    }

    //for (int i=0; i < 17; i++)
    //{
    //    printf("OUTPUT[%d]= %#X -- %d\n", i, output_data[i], (int) output_data[i]);
    //}
    params->hfront_porch = output_data[0];
    params->hback_porch = output_data[1];

    //printf("width raw numbers %d %d\n",output_data[2],output_data[3]);

    params->width = output_data[2] | (output_data[3] << 8);
    params->vfront_porch = output_data[4];
    params->vback_porch = output_data[5];

    fprintf(stderr,"height raw numbers %d %d\n",output_data[6],output_data[7]);

    params->height = output_data[6] | (output_data[7] << 8);
    params->pix_width = (output_data[8] | (output_data[9] << 8)) / 256.0;
    params->pix_height = (output_data[10] | (output_data[11] << 8)) / 256.0;

    params->color_matrix = output_data[12] | (output_data[13] << 8);
    params->bits_per_pixel = output_data[14];
    params->num_serial_ports = output_data[15];
    params->extra_caps = output_data[16];
    return (0);
}

int SxCCD::ClearPixels(int flags,int camIndex)
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

int SxCCD::LatchPixels(int flags,int camIndex,int xoffset,int yoffset,int width,int height,int xbin,int ybin)
{
    char setup_data[18];
    int rc;

    //if (Interlaced)
        //ybin--;

    fprintf(stderr,"Latch pixels: xoffset: %d - yoffset: %d - Width: %d - Height: %d - xbin: %d - ybin: %d\n", xoffset, yoffset, width, height, xbin, ybin);

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

int SxCCD::ExposePixels(int flags,int camIndex,int xoffset,int yoffset,int width,int height,int xbin,int ybin,int msec)
{
    char setup_data[22];
    int rc;

    //if (Interlaced)
        //ybin--;

    fprintf(stderr,"Expose pixels: xoffset: %d - yoffset: %d - Width: %d - Height: %d - xbin: %d - ybin: %d - delay: %d\n", xoffset, yoffset, width, height, xbin, ybin, msec);
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

int SxCCD::ReadPixels(char *pixels,int count)
{
    int rc=0, bread=0;
    int tries=0;
    while ((bread < count) && (tries < 5)&&(rc >=0))
    {
        tries++;
        rc = ReadBulk(pixels+bread,count-bread,10000);
        fprintf(stderr,"Read bulk returns %d\n",rc);
        //if (rc < 0 );
        //    break;
        //bread += rc;
        if(rc > 0) bread+=rc;

        fprintf(stderr,"On Read Attempt %d got %d bytes while requested is %d\n", tries, bread, count);
        usleep(50);

    }

    fprintf(stderr,"Read Pixels request %d got %d after %d tries.\n",count,bread, tries);

	//FILE *h = fopen("rawcam.dat", "w+");
	//fwrite(pixels, count, 1, h);
	//fclose(h);
    return bread;
}

int SxCCD::SetParams(int XRes,int YRes,int CamBits,float pixwidth,float pixheight)
{
    fprintf(stderr,"SxCCD::SetParams\n");
    return 0;
}

int SxCCD::SetGuideParams(int XRes,int YRes,int CamBits,float pixwidth,float pixheight)
{

    return 0;
}

int SxCCD::SetInterlaced(bool)
{
    return 0;
}

int SxCCD::pulseGuide()
{
    char setup_data[8];
    int rc;

    setup_data[USB_REQ_TYPE    ] = USB_REQ_VENDOR | USB_REQ_DATAOUT;
    setup_data[USB_REQ         ] = SXUSB_SET_STAR2K;
    setup_data[USB_REQ_VALUE_L ] = guide_cmd;
    setup_data[USB_REQ_VALUE_H ] = 0;
    setup_data[USB_REQ_INDEX_L ] = 0;
    setup_data[USB_REQ_INDEX_H ] = 0;
    setup_data[USB_REQ_LENGTH_L] = 0;
    setup_data[USB_REQ_LENGTH_H] = 0;
    rc=WriteBulk(setup_data,8,1000);
    return 0;
}

