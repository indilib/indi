#if 0
    V4L INDI Driver
    INDI Interface for V4L devices
    Copyright (C) 2003-2005 Jasem Mutlaq (mutlaqja@ikarustech.com)

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA

#endif

#include "v4ldriver.h"

V4L_Driver::V4L_Driver()
{
  allocateBuffers();

  camNameT[0].text  = NULL; 
  PortT[0].text     = NULL;
  IUSaveText(&PortT[0], "/dev/video0");

  divider = 128.;
}

V4L_Driver::~V4L_Driver()
{
  releaseBuffers();
}


void V4L_Driver::initProperties(const char *dev)
{
  
  strncpy(device_name, dev, MAXINDIDEVICE);
 
  /* Connection */
  IUFillSwitch(&PowerS[0], "CONNECT", "Connect", ISS_OFF);
  IUFillSwitch(&PowerS[1], "DISCONNECT", "Disconnect", ISS_ON);
  IUFillSwitchVector(&PowerSP, PowerS, NARRAY(PowerS), dev, "CONNECTION", "Connection", COMM_GROUP, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);

 /* Port */
  IUFillText(&PortT[0], "PORT", "Port", "/dev/video0");
  IUFillTextVector(&PortTP, PortT, NARRAY(PortT), dev, "DEVICE_PORT", "Ports", COMM_GROUP, IP_RW, 0, IPS_IDLE);

 /* Video Stream */
  IUFillSwitch(&StreamS[0], "ON", "Stream On", ISS_OFF);
  IUFillSwitch(&StreamS[1], "OFF", "Stream Off", ISS_ON);
  IUFillSwitchVector(&StreamSP, StreamS, NARRAY(StreamS), dev, "VIDEO_STREAM", "Video Stream", COMM_GROUP, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

  /* Compression */
  IUFillSwitch(&CompressS[0], "ON", "", ISS_ON);
  IUFillSwitch(&CompressS[1], "OFF", "", ISS_OFF);
  IUFillSwitchVector(&CompressSP, CompressS, NARRAY(StreamS), dev, "Compression", "", IMAGE_GROUP, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

  /* Image type */
  IUFillSwitch(&ImageTypeS[0], "Grey", "", ISS_ON);
  IUFillSwitch(&ImageTypeS[1], "Color", "", ISS_OFF);
  IUFillSwitchVector(&ImageTypeSP, ImageTypeS, NARRAY(ImageTypeS), dev, "Image Type", "", IMAGE_GROUP, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

  /* Camera Name */
  IUFillText(&camNameT[0], "Model", "", "");
  IUFillTextVector(&camNameTP, camNameT, NARRAY(camNameT), dev, "Camera Model", "", COMM_GROUP, IP_RO, 0, IPS_IDLE);
  
  /* Expose */
  IUFillNumber(&ExposeTimeN[0], "CCD_EXPOSURE_VALUE", "Duration (s)", "%5.2f", 0., 36000., 0.5, 1.);
  IUFillNumberVector(&ExposeTimeNP, ExposeTimeN, NARRAY(ExposeTimeN), dev, "CCD_EXPOSURE", "Expose", COMM_GROUP, IP_RW, 60, IPS_IDLE);

/* Frame Rate */
  IUFillNumber(&FrameRateN[0], "RATE", "Rate", "%0.f", 1., 50., 1., 10.);
  IUFillNumberVector(&FrameRateNP, FrameRateN, NARRAY(FrameRateN), dev, "FRAME_RATE", "Frame Rate", COMM_GROUP, IP_RW, 60, IPS_IDLE);

  /* Frame dimension */
  IUFillNumber(&FrameN[0], "X", "X", "%.0f", 0., 0., 0., 0.);
  IUFillNumber(&FrameN[1], "Y", "Y", "%.0f", 0., 0., 0., 0.);
  IUFillNumber(&FrameN[2], "WIDTH", "Width", "%.0f", 0., 0., 10., 0.);
  IUFillNumber(&FrameN[3], "HEIGHT", "Height", "%.0f", 0., 0., 10., 0.);
  IUFillNumberVector(&FrameNP, FrameN, NARRAY(FrameN), dev, "CCD_FRAME", "Frame", IMAGE_GROUP, IP_RW, 60, IPS_IDLE);

  /*IUFillNumber(&ImageSizeN[0], "WIDTH", "Width", "%0.f", 0., 0., 10., 0.);
  IUFillNumber(&ImageSizeN[1], "HEIGHT", "Height", "%0.f", 0., 0., 10., 0.);
  IUFillNumberVector(&ImageSizeNP, ImageSizeN, NARRAY(ImageSizeN), dev, "IMAGE_SIZE", "Image Size", IMAGE_GROUP, IP_RW, 60, IPS_IDLE);*/
  
  #ifndef HAVE_LINUX_VIDEODEV2_H
  IUFillNumber(&ImageAdjustN[0], "Contrast", "", "%0.f", 0., 256., 1., 0.);
  IUFillNumber(&ImageAdjustN[1], "Brightness", "", "%0.f", 0., 256., 1., 0.);
  IUFillNumber(&ImageAdjustN[2], "Hue", "", "%0.f", 0., 256., 1., 0.);
  IUFillNumber(&ImageAdjustN[3], "Color", "", "%0.f", 0., 256., 1., 0.);
  IUFillNumber(&ImageAdjustN[4], "Whiteness", "", "%0.f", 0., 256., 1., 0.);
  IUFillNumberVector(&ImageAdjustNP, ImageAdjustN, NARRAY(ImageAdjustN), dev, "Image Adjustments", "", IMAGE_GROUP, IP_RW, 60, IPS_IDLE);
  #else
  IUFillNumberVector(&ImageAdjustNP, NULL, 0, dev, "Image Adjustments", "", IMAGE_GROUP, IP_RW, 60, IPS_IDLE);
  #endif 

  // We need to setup the BLOB (Binary Large Object) below. Using this property, we can send FITS to our client
  strcpy(imageB.name, "CCD1");
  strcpy(imageB.label, "Feed");
  strcpy(imageB.format, "");
  imageB.blob    = 0;
  imageB.bloblen = 0;
  imageB.size    = 0;
  imageB.bvp     = 0;
  imageB.aux0    = 0;
  imageB.aux1    = 0;
  imageB.aux2    = 0;
  
  strcpy(imageBP.device, dev);
  strcpy(imageBP.name, "Video");
  strcpy(imageBP.label, "Video");
  strcpy(imageBP.group, COMM_GROUP);
  strcpy(imageBP.timestamp, "");
  imageBP.p       = IP_RO;
  imageBP.timeout = 0;
  imageBP.s       = IPS_IDLE;
  imageBP.bp	  = &imageB;
  imageBP.nbp     = 1;
  imageBP.aux     = 0;

}

void V4L_Driver::initCamBase()
{
    v4l_base = new V4L2_Base();
}

void V4L_Driver::ISGetProperties (const char *dev)
{ 

  if (dev && strcmp (device_name, dev))
    return;

  /* COMM_GROUP */
  IDDefSwitch(&PowerSP, NULL);
  IDDefText(&PortTP, NULL);
  IDDefText(&camNameTP, NULL);
  IDDefSwitch(&StreamSP, NULL);
  #ifndef HAVE_LINUX_VIDEODEV2_H
  IDDefNumber(&FrameRateNP, NULL);
  #endif
  IDDefNumber(&ExposeTimeNP, NULL);
  IDDefBLOB(&imageBP, NULL);
  
  /* Image properties */
  IDDefSwitch(&CompressSP, NULL);
  IDDefSwitch(&ImageTypeSP, NULL);
  IDDefNumber(&FrameNP, NULL);
  
  #ifndef HAVE_LINUX_VIDEODEV2_H
  IDDefNumber(&ImageAdjustNP, NULL);
  #endif


  
}
 
void V4L_Driver::ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n)
{
	char errmsg[ERRMSGSIZ];

	/* ignore if not ours */
	if (dev && strcmp (device_name, dev))
	    return;
	    
     /* Connection */
     if (!strcmp (name, PowerSP.name))
     {
          IUResetSwitch(&PowerSP);
	  IUUpdateSwitch(&PowerSP, states, names, n);
   	  connectCamera();
	  return;
     }

     /* Compression */
     if (!strcmp(name, CompressSP.name))
     {
       IUResetSwitch(&CompressSP);
       IUUpdateSwitch(&CompressSP, states, names, n);
       CompressSP.s = IPS_OK;
       
       IDSetSwitch(&CompressSP, NULL);
       return;
     }    

     /* Image Type */
     if (!strcmp(name, ImageTypeSP.name))
     {
       IUResetSwitch(&ImageTypeSP);
       IUUpdateSwitch(&ImageTypeSP, states, names, n);
       ImageTypeSP.s = IPS_OK;
       
       IDSetSwitch(&ImageTypeSP, NULL);
       return;
     }
     
     /* Video Stream */
     if (!strcmp(name, StreamSP.name))
     {
     
      if (checkPowerS(&StreamSP))
         return;
       
       IUResetSwitch(&StreamSP);
       IUUpdateSwitch(&StreamSP, states, names, n);
       StreamSP.s = IPS_IDLE;
       
          
       v4l_base->stop_capturing(errmsg);

       if (StreamS[0].s == ISS_ON)
       {
         frameCount = 0;
         IDLog("Starting the video stream.\n");
	 StreamSP.s  = IPS_BUSY; 
	 v4l_base->start_capturing(errmsg);
       }
       else
       {
         IDLog("The video stream has been disabled. Frame count %d\n", frameCount);
         v4l_base->stop_capturing(errmsg);
       }
       
       IDSetSwitch(&StreamSP, NULL);
       return;
     }
     
}

void V4L_Driver::ISNewText (const char *dev, const char *name, char *texts[], char *names[], int /*n*/)
{
	IText *tp;


       /* ignore if not ours */ 
       if (dev && strcmp (device_name, dev))
         return;

	if (!strcmp(name, PortTP.name) )
	{
	  PortTP.s = IPS_OK;
	  tp = IUFindText( &PortTP, names[0] );
	  if (!tp)
	   return;

          IUSaveText(tp, texts[0]);
	  IDSetText (&PortTP, NULL);
	  return;
	}
}

void V4L_Driver::ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n)
{
      char errmsg[ERRMSGSIZ];

	/* ignore if not ours */
	if (dev && strcmp (device_name, dev))
	    return;
	    
    
    /* Frame Size */
    if (!strcmp (FrameNP.name, name))
    {
      if (checkPowerN(&FrameNP))
        return;
	
       int oldW = (int) FrameN[2].value;
       int oldH = (int) FrameN[3].value;

      FrameNP.s = IPS_OK;
      
      if (IUUpdateNumber(&FrameNP, values, names, n) < 0)
       return;
      
      if (v4l_base->setSize( (int) FrameN[2].value, (int) FrameN[3].value) != -1)
      {
         FrameN[2].value = v4l_base->getWidth();
	 FrameN[3].value = v4l_base->getHeight();
	 V4LFrame->width = (int) FrameN[2].value;
	 V4LFrame->height= (int) FrameN[3].value;
         IDSetNumber(&FrameNP, NULL);
	 return;
      }
      else
      {
        FrameN[2].value = oldW;
	FrameN[3].value = oldH;
        FrameNP.s = IPS_ALERT;
	IDSetNumber(&FrameNP, "Failed to set a new image size.");
      }
      
      return;
   }

   #ifndef HAVE_LINUX_VIDEODEV2_H
   /* Frame rate */
   if (!strcmp (FrameRateNP.name, name))
   {
     if (checkPowerN(&FrameRateNP))
      return;
      
     FrameRateNP.s = IPS_IDLE;
     
     if (IUUpdateNumber(&FrameRateNP, values, names, n) < 0)
       return;
       
     v4l_base->setFPS( (int) FrameRateN[0].value );
     
     FrameRateNP.s = IPS_OK;
     IDSetNumber(&FrameRateNP, NULL);
     return;
   }
   #endif
   
   
   if (!strcmp (ImageAdjustNP.name, name))
   {
     if (checkPowerN(&ImageAdjustNP))
       return;
       
     ImageAdjustNP.s = IPS_IDLE;
     
     if (IUUpdateNumber(&ImageAdjustNP, values, names, n) < 0)
       return;
     
     #ifndef HAVE_LINUX_VIDEODEV2_H
     v4l_base->setContrast( (int) (ImageAdjustN[0].value * divider));
     v4l_base->setBrightness( (int) (ImageAdjustN[1].value * divider));
     v4l_base->setHue( (int) (ImageAdjustN[2].value * divider));
     v4l_base->setColor( (int) (ImageAdjustN[3].value * divider));
     v4l_base->setWhiteness( (int) (ImageAdjustN[4].value * divider));
     
     ImageAdjustN[0].value = v4l_base->getContrast() / divider;
     ImageAdjustN[1].value = v4l_base->getBrightness() / divider;
     ImageAdjustN[2].value = v4l_base->getHue() / divider;
     ImageAdjustN[3].value = v4l_base->getColor() / divider;
     ImageAdjustN[4].value = v4l_base->getWhiteness() / divider;

     #else
     unsigned int ctrl_id;
     for (int i=0; i < ImageAdjustNP.nnp; i++)
     {
         ctrl_id = *((unsigned int *) ImageAdjustNP.np[i].aux0);
         if (v4l_base->setINTControl( ctrl_id , ImageAdjustNP.np[i].value, errmsg) < 0)
         {
            ImageAdjustNP.s = IPS_ALERT;
            IDSetNumber(&ImageAdjustNP, "Unable to adjust setting. %s", errmsg);
            return;
         }
     }
     #endif
     
     ImageAdjustNP.s = IPS_OK;
     IDSetNumber(&ImageAdjustNP, NULL);
     return;
   }
   
   
    /* Exposure */
    if (!strcmp (ExposeTimeNP.name, name))
    {
       
       if (checkPowerN(&ExposeTimeNP))
         return;
    
        //if (StreamS[0].s == ISS_ON)
    v4l_base->stop_capturing(errmsg);

	StreamS[0].s  = ISS_OFF;
	StreamS[1].s  = ISS_ON;
	StreamSP.s    = IPS_IDLE;
	IDSetSwitch(&StreamSP, NULL);
        
	V4LFrame->expose = 1000;

        ExposeTimeNP.s   = IPS_BUSY;
	IDSetNumber(&ExposeTimeNP, NULL);

	time(&capture_start);
	v4l_base->start_capturing(errmsg);
	
        return;
    } 
      
  
  	
}

void V4L_Driver::newFrame(void *p)
{
  ((V4L_Driver *) (p))->updateFrame();
}

void V4L_Driver::updateFrame()
{
  char errmsg[ERRMSGSIZ];
  static const unsigned int FRAME_DROP = 2;
  static int dropLarge = FRAME_DROP;

  if (StreamSP.s == IPS_BUSY)
  {
	// Ad hoc way of dropping frames
	frameCount++;
        dropLarge--;
        if (dropLarge == 0)
        {
          dropLarge = (int) (((ImageTypeS[0].s == ISS_ON) ? FRAME_DROP : FRAME_DROP*3) * (FrameN[2].value / 160.0));
	  updateStream();
          return;
        }
  }
  else if (ExposeTimeNP.s == IPS_BUSY)
  {
     V4LFrame->Y      = v4l_base->getY();
     v4l_base->stop_capturing(errmsg);
     time(&capture_end);
     IDLog("Capture of ONE frame took %g seconds.\n", difftime(capture_end, capture_start));
     grabImage();
  }

}

void V4L_Driver::updateStream()
{
 
   int width  = v4l_base->getWidth();
   int height = v4l_base->getHeight();
   uLongf compressedBytes = 0;
   uLong totalBytes;
   unsigned char *targetFrame;
   int r;
   
   if (PowerS[0].s == ISS_OFF || StreamS[0].s == ISS_OFF) return;
   
   if (ImageTypeS[0].s == ISS_ON)
      V4LFrame->Y      		= v4l_base->getY();
   else
      V4LFrame->colorBuffer 	= v4l_base->getColorBuffer();
  
   totalBytes  = ImageTypeS[0].s == ISS_ON ? width * height : width * height * 4;
   targetFrame = ImageTypeS[0].s == ISS_ON ? V4LFrame->Y : V4LFrame->colorBuffer;

   /* Do we want to compress ? */
    if (CompressS[0].s == ISS_ON)
    {   
   	/* Compress frame */
   	V4LFrame->compressedFrame = (unsigned char *) realloc (V4LFrame->compressedFrame, sizeof(unsigned char) * totalBytes + totalBytes / 64 + 16 + 3);
   
   	compressedBytes = sizeof(unsigned char) * totalBytes + totalBytes / 64 + 16 + 3;
   
   	r = compress2(V4LFrame->compressedFrame, &compressedBytes, targetFrame, totalBytes, 4);
   	if (r != Z_OK)
   	{
	 	/* this should NEVER happen */
	 	IDLog("internal error - compression failed: %d\n", r);
		return;
   	}
   
   	/* #3.A Send it compressed */
   	imageB.blob = V4LFrame->compressedFrame;
   	imageB.bloblen = compressedBytes;
   	imageB.size = totalBytes;
   	strcpy(imageB.format, ".stream.z");
     }
     else
     {
       /* #3.B Send it uncompressed */
        imageB.blob = targetFrame;
   	imageB.bloblen = totalBytes;
   	imageB.size = totalBytes;
   	strcpy(imageB.format, ".stream");
     }
        
   imageBP.s = IPS_OK;
   IDSetBLOB (&imageBP, NULL);
   
   #ifndef HAVE_LINUX_VIDEODEV2_H
      char errmsg[ERRMSGSIZ];
      v4l_base->start_capturing(errmsg);
   #endif
}

/* Downloads the image from the CCD row by row and store them
   in a raw file.
 N.B. No processing is done on the image */
int V4L_Driver::grabImage()
{
   int err, fd;
   char errmsg[ERRMSG_SIZE];
   char temp_filename[TEMPFILE_LEN] = "/tmp/fitsXXXXXX";
   
   
   if ((fd = mkstemp(temp_filename)) < 0)
   { 
    IDMessage(device_name, "Error making temporary filename.");
    IDLog("Error making temporary filename.\n");
    return -1;
   }
   close(fd);
  
   
   err = writeFITS(temp_filename, errmsg);
   if (err)
   {
       IDMessage(device_name, errmsg, NULL);
       return -1;
   }
   
  return 0;
}

int V4L_Driver::writeFITS(const char * filename, char errmsg[])
{
    fitsfile *fptr;       /* pointer to the FITS file; defined in fitsio.h */
    int status;
    long  fpixel = 1, naxis = 2, nelements;
    long naxes[2];
    char filename_rw[TEMPFILE_LEN+1];

    INDI_UNUSED(errmsg);

    // Append ! to file name to over write it.
    snprintf(filename_rw, TEMPFILE_LEN+1, "!%s", filename);

    naxes[0] = v4l_base->getWidth();
    naxes[1] = v4l_base->getHeight();

    status = 0;         /* initialize status before calling fitsio routines */
    fits_create_file(&fptr, filename_rw, &status);   /* create new file */

    /* Create the primary array image (16-bit short integer pixels */
    fits_create_img(fptr, BYTE_IMG, naxis, naxes, &status);

    addFITSKeywords(fptr);

    nelements = naxes[0] * naxes[1];          /* number of pixels to write */

    /* Write the array of integers to the image */
    fits_write_img(fptr, TBYTE, fpixel, nelements, V4LFrame->Y, &status);

    fits_close_file(fptr, &status);            /* close the file */

    fits_report_error(stderr, status);  /* print out any error messages */

    /* Success */
    ExposeTimeNP.s = IPS_OK;
    IDSetNumber(&ExposeTimeNP, NULL);
    uploadFile(filename);
    unlink(filename);

    return status;
}

void V4L_Driver::addFITSKeywords(fitsfile *fptr)
{
  int status=0; 

  char keyname[32], comment[64];

 strncpy(keyname, "EXPOSURE", 32);
 strncpy(comment, "Total Exposure Time (ms)", 64);
 fits_update_key(fptr, TLONG, keyname , &(V4LFrame->expose), comment, &status);

 strncpy(keyname, "INSTRUME", 32);
 strncpy(comment, "Webcam Name", 64);
 fits_update_key(fptr, TSTRING, keyname, v4l_base->getDeviceName(), comment, &status);

 fits_write_date(fptr, &status);
}

void V4L_Driver::uploadFile(const char * filename)
{
   
   FILE * fitsFile;
   int r=0;
   unsigned int nr = 0;
   uLong  totalBytes;
   uLongf compressedBytes = 0;
   struct stat stat_p; 
 
   if ( -1 ==  stat (filename, &stat_p))
   { 
     IDLog(" Error occurred attempting to stat %s\n", filename); 
     return; 
   }
   
   totalBytes = stat_p.st_size;

   fitsFile = fopen(filename, "r");
   
   if (fitsFile == NULL)
    return;
   
   fitsData = (fitsData == NULL) ? (unsigned char *) malloc(sizeof(unsigned char) * totalBytes) :
				   (unsigned char *) realloc(fitsData, sizeof(unsigned char) * totalBytes);
   /* #1 Read file from disk */ 
   for (unsigned int i=0; i < totalBytes; i+= nr)
   {
      nr = fread(fitsData + i, 1, totalBytes - i, fitsFile);
     
     if (nr <= 0)
     {
        IDLog("Error reading temporary FITS file.\n");
        return;
     }
   }
   fclose(fitsFile);
   
   if (CompressS[0].s == ISS_ON)
   {   
   /* #2 Compress it */
   V4LFrame->compressedFrame = (unsigned char *) realloc (V4LFrame->compressedFrame, sizeof(unsigned char) * totalBytes + totalBytes / 64 + 16 + 3);
   
    compressedBytes = sizeof(unsigned char) * totalBytes + totalBytes / 64 + 16 + 3;
     
     
   r = compress2(V4LFrame->compressedFrame, &compressedBytes, fitsData, totalBytes, 9);
   if (r != Z_OK)
   {
 	/* this should NEVER happen */
 	IDLog("internal error - compression failed: %d\n", r);
	return;
   }

   /* #3.A Send it compressed */
   imageB.blob = V4LFrame->compressedFrame;
   imageB.bloblen = compressedBytes;
   imageB.size = totalBytes;
   strcpy(imageB.format, ".fits.z");
   }
   else
   {
     imageB.blob = fitsData;
     imageB.bloblen = totalBytes;
     imageB.size = totalBytes;
     strcpy(imageB.format, ".fits");
   }
   
   imageBP.s = IPS_OK;
   IDSetBLOB (&imageBP, NULL);
   
} 

void V4L_Driver::connectCamera()
{
  char errmsg[ERRMSGSIZ];
  
    
  switch (PowerS[0].s)
  {
     case ISS_ON:
      if (v4l_base->connectCam(PortT[0].text, errmsg) < 0)
      {
	  PowerSP.s = IPS_IDLE;
	  PowerS[0].s = ISS_OFF;
	  PowerS[1].s = ISS_ON;
	  IDSetSwitch(&PowerSP, "Error: unable to open device");
	  IDLog("Error: %s\n", errmsg);
	  return;
      }
      
      /* Sucess! */
      PowerS[0].s = ISS_ON;
      PowerS[1].s = ISS_OFF;
      PowerSP.s = IPS_OK;
      IDSetSwitch(&PowerSP, "Video4Linux Generic Device is online. Retrieving basic data.");

      v4l_base->registerCallback(newFrame, this);
      
      IDLog("V4L Device is online. Retrieving basic data.\n");
      getBasicData();
      
      break;
      
    case ISS_OFF:
      PowerS[0].s = ISS_OFF;
      PowerS[1].s = ISS_ON;
      PowerSP.s = IPS_IDLE;
      
      v4l_base->disconnectCam();
      
      IDSetSwitch(&PowerSP, "Video4Linux Generic Device is offline.");
      
      break;
     }
}

/* Retrieves basic data from the device upon connection.*/
void V4L_Driver::getBasicData()
{

  int xmax, ymax, xmin, ymin;
  
  v4l_base->getMaxMinSize(xmax, ymax, xmin, ymin);

  /* Width */
  FrameN[2].value = v4l_base->getWidth();
  FrameN[2].min = xmin;
  FrameN[2].max = xmax;
  V4LFrame->width = (int) FrameN[2].value;
  
  /* Height */
  FrameN[3].value = v4l_base->getHeight();
  FrameN[3].min = ymin;
  FrameN[3].max = ymax;
  V4LFrame->height = (int) FrameN[3].value;
  
  IUUpdateMinMax(&FrameNP);
  IDSetNumber(&FrameNP, NULL);
  
  IUSaveText(&camNameT[0], v4l_base->getDeviceName());
  IDSetText(&camNameTP, NULL);

   #ifndef HAVE_LINUX_VIDEODEV2_H
     updateV4L1Controls();
   #else
    updateV4L2Controls();
   #endif
   
}

#ifdef HAVE_LINUX_VIDEODEV2_H
void V4L_Driver::updateV4L2Controls()
{
    // #1 Query for INTEGER controls, and fill up the structure
      free(ImageAdjustNP.np);
      ImageAdjustNP.nnp = 0;
      
   if (v4l_base->queryINTControls(&ImageAdjustNP) > 0)
      IDDefNumber(&ImageAdjustNP, NULL);
}
#else
void V4L_Driver::updateV4L1Controls()
{

  if ( (v4l_base->getContrast() / divider) > ImageAdjustN[0].max)
      divider *=2;

  if ( (v4l_base->getHue() / divider) > ImageAdjustN[2].max)
      divider *=2;

  ImageAdjustN[0].value = v4l_base->getContrast() / divider;
  ImageAdjustN[1].value = v4l_base->getBrightness() / divider;
  ImageAdjustN[2].value = v4l_base->getHue() / divider;
  ImageAdjustN[3].value = v4l_base->getColor() / divider;
  ImageAdjustN[4].value = v4l_base->getWhiteness() / divider;

  ImageAdjustNP.s = IPS_OK;
  IDSetNumber(&ImageAdjustNP, NULL);
  
}
#endif

int V4L_Driver::checkPowerS(ISwitchVectorProperty *sp)
{
  if (PowerSP.s != IPS_OK)
  {
    if (!strcmp(sp->label, ""))
        IDMessage (device_name, "Cannot change property %s while the camera is offline.", sp->name);
    else
        IDMessage (device_name, "Cannot change property %s while the camera is offline.", sp->label);
    
    sp->s = IPS_IDLE;
    IDSetSwitch(sp, NULL);
    return -1;
  }

  return 0;
}

int V4L_Driver::checkPowerN(INumberVectorProperty *np)
{
   
  if (PowerSP.s != IPS_OK)
  {
    if (!strcmp(np->label, ""))
        IDMessage (device_name, "Cannot change property %s while the camera is offline.", np->name);
    else
        IDMessage (device_name, "Cannot change property %s while the camera is offline.", np->label);
	
    np->s = IPS_IDLE;
    IDSetNumber(np, NULL);
    return -1;
  }

  return 0;
}

int V4L_Driver::checkPowerT(ITextVectorProperty *tp)
{

  if (PowerSP.s != IPS_OK)
  {
    if (!strcmp(tp->label, ""))
        IDMessage (device_name, "Cannot change property %s while the camera is offline.", tp->name);
    else
        IDMessage (device_name, "Cannot change property %s while the camera is offline.", tp->label);
	
    tp->s = IPS_IDLE;
    IDSetText(tp, NULL);
    return -1;
  }

  return 0;

}

void V4L_Driver::allocateBuffers()
{
     V4LFrame = (img_t *) malloc (sizeof(img_t));
 
     if (V4LFrame == NULL)
     {
       IDMessage(NULL, "Error: unable to initialize driver. Low memory.");
       IDLog("Error: unable to initialize driver. Low memory.");
       exit(-1);
     }

     fitsData			= (unsigned char *) malloc (sizeof(unsigned char) * 1);
     V4LFrame->Y                = (unsigned char *) malloc (sizeof(unsigned char) * 1);
     V4LFrame->U                = (unsigned char *) malloc (sizeof(unsigned char) * 1);
     V4LFrame->V                = (unsigned char *) malloc (sizeof(unsigned char) * 1);
     V4LFrame->colorBuffer      = (unsigned char *) malloc (sizeof(unsigned char) * 1);
     V4LFrame->compressedFrame  = (unsigned char *) malloc (sizeof(unsigned char) * 1);
}

void V4L_Driver::releaseBuffers()
{
  free(fitsData);
  free(V4LFrame->Y);
  free(V4LFrame->U);
  free(V4LFrame->V);
  free(V4LFrame->colorBuffer);
  free(V4LFrame->compressedFrame);
  free (V4LFrame);
}


