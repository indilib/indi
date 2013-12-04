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

#include "v4l2driver.h"

V4L2_Driver::V4L2_Driver()
{
  allocateBuffers();

  camNameT[0].text  = NULL; 
  PortT[0].text     = NULL;
  IUSaveText(&PortT[0], "/dev/video0");

  divider = 128.;
  strncpy(device_name, "V4L2 CCD", MAXINDIDEVICE);
  
  // No guide head, no ST4 port, no cooling, no shutter
  SetCCDFeatures(false, false, false, false);

  Options=NULL;
  v4loptions=0;

  stackMode=0;

  lx=new Lx();
}

V4L2_Driver::~V4L2_Driver()
{
  releaseBuffers();
}


bool V4L2_Driver::initProperties()
{
  
   INDI::CCD::initProperties();
 
  ConnectSP=getSwitch("CONNECTION");
  ConnectS=ConnectSP->sp;
 
 /* Port */
  IUFillText(&PortT[0], "PORT", "Port", "/dev/video0");
  IUFillTextVector(&PortTP, PortT, NARRAY(PortT), getDeviceName(), "DEVICE_PORT", "Ports", OPTIONS_TAB, IP_RW, 0, IPS_IDLE);

 /* Video Stream */
  IUFillSwitch(&StreamS[0], "ON", "Stream On", ISS_OFF);
  IUFillSwitch(&StreamS[1], "OFF", "Stream Off", ISS_ON);
  IUFillSwitchVector(&StreamSP, StreamS, NARRAY(StreamS), getDeviceName(), "VIDEO_STREAM", "Video Stream", COMM_GROUP, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

  /* Image type */
  IUFillSwitch(&ImageTypeS[0], "Grey", "", ISS_ON);
  IUFillSwitch(&ImageTypeS[1], "Color", "", ISS_OFF);
  IUFillSwitchVector(&ImageTypeSP, ImageTypeS, NARRAY(ImageTypeS), getDeviceName(), "Image Type", "", IMAGE_SETTINGS_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

  /* Camera Name */
  IUFillText(&camNameT[0], "Model", "", "");
  IUFillTextVector(&camNameTP, camNameT, NARRAY(camNameT), getDeviceName(), "Camera Model", "", IMAGE_INFO_TAB, IP_RO, 0, IPS_IDLE);

  /* Stacking Mode */
  IUFillSwitch(&StackModeS[0], "None", "", ISS_ON);
  IUFillSwitch(&StackModeS[1], "Additive", "", ISS_OFF);
  IUFillSwitchVector(&StackModeSP, StackModeS, NARRAY(StackModeS), getDeviceName(), "Stacking Mode", "", COMM_GROUP, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);
  stackMode=0;

  /* Inputs */
  IUFillSwitchVector(&InputsSP, NULL, 0, getDeviceName(), "V4L2_INPUT", "Inputs", CAPTURE_FORMAT, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);
  /* Capture Formats */
  IUFillSwitchVector(&CaptureFormatsSP, NULL, 0, getDeviceName(), "V4L2_FORMAT", "Capture Format", CAPTURE_FORMAT, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);
  /* Capture Sizes */
  IUFillSwitchVector(&CaptureSizesSP, NULL, 0, getDeviceName(), "V4L2_SIZE_DISCRETE", "Capture Size", CAPTURE_FORMAT, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);
  IUFillNumberVector(&CaptureSizesNP, NULL, 0, getDeviceName(), "V4L2_SIZE_STEP", "Capture Size", CAPTURE_FORMAT, IP_RW, 0, IPS_IDLE);
  /* Frame Rate */
  IUFillSwitchVector(&FrameRatesSP, NULL, 0, getDeviceName(), "V4L2_FRAMEINT_DISCRETE", "Frame Interval", CAPTURE_FORMAT, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);
  IUFillNumberVector(&FrameRateNP, NULL, 0, getDeviceName(), "V4L2_FRAMEINT_STEP", "Frame Interval", CAPTURE_FORMAT, IP_RW, 60, IPS_IDLE);

  /* V4L2 Settings */  
  IUFillNumberVector(&ImageAdjustNP, NULL, 0, getDeviceName(), "Image Adjustments", "", IMAGE_GROUP, IP_RW, 60, IPS_IDLE);

  if (!lx->initProperties(this))
    IDMessage(device_name, "Can not init Long Exposure");
  return true;
}

void V4L2_Driver::initCamBase()
{
    v4l_base = new V4L2_Base();
}

void V4L2_Driver::ISGetProperties (const char *dev)
{ 

  //IDLog("V4L2 ISGetProperties %s\n", (dev?dev:"No device"));

  if (dev && strcmp (device_name, dev))
    return;

  INDI::CCD::ISGetProperties(dev);

  /* COMM_GROUP */
  //IDDefSwitch(&PowerSP, NULL);
  
  defineText(&PortTP);
  //aIDDefText(&PortTP, NULL);

  //IDDefNumber(&ExposeTimeNP, NULL);
  //IDDefBLOB(&imageBP, NULL);
  
  /* Image properties */
  //IDDefSwitch(&CompressSP, NULL);


  
//IDDefNumber(&FrameNP, NULL);
  
}

bool V4L2_Driver::updateProperties ()
{ 
  bool ok;
  //IDLog("V4L2 updateProperties\n");
  if (isConnected()) {
    ok = INDI::CCD::updateProperties();

    ExposeTimeNP=getNumber("CCD_EXPOSURE");
    //if (ExposeTimeNP==NULL) IDLog("updateProperties: CCD_EXPOSURE not found");
    //else IDLog("updateProperties: CCD_EXPOSURE found");
    ExposeTimeN=ExposeTimeNP->np;
    //IDLog("V4L2 Expose Time updateProperties Ok\n");
    
    imageBP=getBLOB("CCD1");
    //if (imageBP==NULL) IDLog("updateProperties: CCD1 not found");
    //else IDLog("updateProperties: CCD1 found");
    imageB=imageBP->bp;
    //IDLog("V4L2 CCD1 Image updateProperties Ok\n");
    
    CompressSP=getSwitch("CCD_COMPRESSION");
    //if (CompressSP==NULL) IDLog("updateProperties: COMPRESSION not found");
    //else IDLog("updateProperties: COMPRESSION found ");
    CompressS=CompressSP->sp;
    //IDLog("V4L2 Compression updateProperties Ok\n");
    
    FrameNP=getNumber("CCD_FRAME");
    //if (FrameNP==NULL) IDLog("updateProperties: CCD_FRAME not found");
    //else IDLog("updateProperties: CCD_FRAME found");
    FrameN=FrameNP->np;
    //IDLog("V4L2 CCD Frame updateProperties Ok\n");
    
    IDDefText(&camNameTP, NULL);
    getBasicData();
    //SetCCDParams(537,597,8,9.8,6.3);
    //SetCCDParams(720,576,8,7.3,6.52);
    SetCCDParams(640,480,8,5.6,5.6);

    IDDefSwitch(&StreamSP, NULL);
    IDDefSwitch(&StackModeSP, NULL);
    IDDefSwitch(&ImageTypeSP, NULL);
    IDDefSwitch(&InputsSP, NULL);
    IDDefSwitch(&CaptureFormatsSP, NULL);
    if (CaptureSizesSP.sp != NULL) IDDefSwitch(&CaptureSizesSP, NULL);
    else if  (CaptureSizesNP.np != NULL) IDDefNumber(&CaptureSizesNP, NULL);
   if (FrameRatesSP.sp != NULL) IDDefSwitch(&FrameRatesSP, NULL);
    else if  (FrameRateNP.np != NULL) IDDefNumber(&FrameRateNP, NULL);
   INDI::CCD::UpdateCCDFrame(0, 0, V4LFrame->width, V4LFrame->height);
   PrimaryCCD.setResolutoin(V4LFrame->width, V4LFrame->height);
   if (ImageTypeS[0].s == ISS_ON) PrimaryCCD.setBPP(8); else PrimaryCCD.setBPP(32);

   lx->updateProperties();

  } else {
    unsigned int i;
   
    lx->updateProperties();
    
    deleteProperty(camNameTP.name);
    deleteProperty(StreamSP.name);
    deleteProperty(StackModeSP.name);
    deleteProperty(ImageTypeSP.name);
    deleteProperty(InputsSP.name);
    deleteProperty(CaptureFormatsSP.name);
    if (CaptureSizesSP.sp != NULL) deleteProperty(CaptureSizesSP.name);
    else if  (CaptureSizesNP.np != NULL) deleteProperty(CaptureSizesNP.name);
   if (FrameRatesSP.sp != NULL) deleteProperty(FrameRatesSP.name);
    else if  (FrameRateNP.np != NULL) deleteProperty(FrameRateNP.name);
    deleteProperty(ImageAdjustNP.name);

    for (i=0; i<v4loptions; i++)
      deleteProperty(Options[i].name);
    if (Options) free(Options);
    Options=NULL;
    v4loptions=0;
    
    INDI::CCD::updateProperties();
  }
}

bool V4L2_Driver::ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n)
{
     char errmsg[ERRMSGSIZ];
     unsigned int iopt;

     /* ignore if not ours */
     if (dev && strcmp (device_name, dev))
       return true;
	    
     /* Input */
	if ((!strcmp(name, InputsSP.name)))
     {
      if ((StreamSP.s == IPS_BUSY) ||  (ExposeTimeNP->s == IPS_BUSY)) {
	 IDMessage(device_name, "Can not set input while capturing.");
	 InputsSP.s = IPS_BUSY;
	 IDSetSwitch(&InputsSP, NULL);
	 return false;
       } else {
	unsigned int inputindex, oldindex;
	oldindex=IUFindOnSwitchIndex(&InputsSP);
	IUResetSwitch(&InputsSP);
	IUUpdateSwitch(&InputsSP, states, names, n);
	inputindex=IUFindOnSwitchIndex(&InputsSP);
	if (v4l_base->setinput(inputindex, errmsg) == -1) {
          IDMessage(device_name, "ERROR (setinput): %s", errmsg);
	  IUResetSwitch(&InputsSP);
	  InputsSP.sp[oldindex].s=ISS_ON;
	  InputsSP.s = IPS_ALERT;
	  IDSetSwitch(&InputsSP, NULL);
	  return false;
	}
	deleteProperty(CaptureFormatsSP.name);
	v4l_base->getcaptureformats(&CaptureFormatsSP);
	IDDefSwitch(&CaptureFormatsSP, NULL);
	if (CaptureSizesSP.sp != NULL) deleteProperty(CaptureSizesSP.name);
	else if  (CaptureSizesNP.np != NULL) deleteProperty(CaptureSizesNP.name);
	v4l_base->getcapturesizes(&CaptureSizesSP, &CaptureSizesNP);
	if (CaptureSizesSP.sp != NULL) IDDefSwitch(&CaptureSizesSP, NULL);
	else if  (CaptureSizesNP.np != NULL) IDDefNumber(&CaptureSizesNP, NULL);
	InputsSP.s = IPS_OK;
	IDSetSwitch(&InputsSP, "Capture input: %d. %s", inputindex, InputsSP.sp[inputindex].name);
	return true;
      }
     }    
	
     /* Capture Format */
	if ((!strcmp(name, CaptureFormatsSP.name)))
     {
       if ((StreamSP.s == IPS_BUSY) ||  (ExposeTimeNP->s == IPS_BUSY)) {
	 IDMessage(device_name, "Can not set format while capturing.");
	 CaptureFormatsSP.s = IPS_BUSY;
	 IDSetSwitch(&CaptureFormatsSP, NULL);
	 return false;
       } else {
	 unsigned int index, oldindex;
	 oldindex=IUFindOnSwitchIndex(&CaptureFormatsSP);
	 IUResetSwitch(&CaptureFormatsSP);
	 IUUpdateSwitch(&CaptureFormatsSP, states, names, n);
	 index=IUFindOnSwitchIndex(&CaptureFormatsSP);
     if (v4l_base->setcaptureformat(*((unsigned int *)CaptureFormatsSP.sp[index].aux), errmsg) == -1) {
          IDMessage(device_name, "ERROR (setformat): %s", errmsg);
	  IUResetSwitch(&CaptureFormatsSP);
	  CaptureFormatsSP.sp[oldindex].s=ISS_ON;
	  CaptureFormatsSP.s = IPS_ALERT;
	  IDSetSwitch(&CaptureFormatsSP, NULL);
	  return false;
	 }

	 if (CaptureSizesSP.sp != NULL) deleteProperty(CaptureSizesSP.name);
	 else if  (CaptureSizesNP.np != NULL) deleteProperty(CaptureSizesNP.name);
	 v4l_base->getcapturesizes(&CaptureSizesSP, &CaptureSizesNP);
	 if (CaptureSizesSP.sp != NULL) IDDefSwitch(&CaptureSizesSP, NULL);
	 else if  (CaptureSizesNP.np != NULL) IDDefNumber(&CaptureSizesNP, NULL);
	 CaptureFormatsSP.s = IPS_OK;
	 IDSetSwitch(&CaptureFormatsSP, "Capture format: %d. %s", index, CaptureFormatsSP.sp[index].name);
	 return true;
       }
     }    

     /* Capture Size (Discrete) */
	if ((!strcmp(name, CaptureSizesSP.name)))
     {
       if ((StreamSP.s == IPS_BUSY) ||  (ExposeTimeNP->s == IPS_BUSY)) {
	 IDMessage(device_name, "Can not set capture size while capturing.");
	 CaptureSizesSP.s = IPS_BUSY;
	 IDSetSwitch(&CaptureSizesSP, NULL);
	 return false;
       } else {
	 unsigned int index, w, h;
	 IUUpdateSwitch(&CaptureSizesSP, states, names, n);
	 index=IUFindOnSwitchIndex(&CaptureSizesSP);
	 sscanf(CaptureSizesSP.sp[index].name, "%dx%d", &w, &h);
	 if (v4l_base->setcapturesize(w, h, errmsg) == -1) {
	   IDMessage(device_name, "ERROR (setsize): %s", errmsg);
	   CaptureSizesSP.s = IPS_ALERT;
	   IDSetSwitch(&CaptureSizesSP, NULL);
	   return false;
	 }
     INDI::CCD::UpdateCCDFrame(0, 0, w, h);
	 V4LFrame->width = w;
	 V4LFrame->height= h;
	 PrimaryCCD.setResolutoin(w, h);
	 CaptureSizesSP.s = IPS_OK;
	 IDSetSwitch(&CaptureSizesSP, "Capture size (discrete): %d. %s", index, CaptureSizesSP.sp[index].name);
	 return true;
       }
     }    
     /* Frame Rate (Discrete) */
	if ((!strcmp(name, FrameRatesSP.name)))
     {
       //if ((StreamSP.s == IPS_BUSY) ||  (ExposeTimeNP->s == IPS_BUSY)) {
       // IDMessage(device_name, "Can not set capture size while capturing.");
       // CaptureSizesSP.s = IPS_BUSY;
       //IDSetSwitch(&CaptureSizesSP, NULL);
       //return false;
       //} else {
	 unsigned int index;
	 struct v4l2_fract frate;
	 IUUpdateSwitch(&FrameRatesSP, states, names, n);
	 index=IUFindOnSwitchIndex(&FrameRatesSP);
	 sscanf(FrameRatesSP.sp[index].name, "%d/%d", &frate.numerator, &frate.denominator);
	 if ((v4l_base->*(v4l_base->setframerate))(frate, errmsg) == -1) {
	   IDMessage(device_name, "ERROR (setframerate): %s", errmsg);
	   FrameRatesSP.s = IPS_ALERT;
	   IDSetSwitch(&FrameRatesSP, NULL);
	   return false;
	 }
	 FrameRatesSP.s = IPS_OK;
	 IDSetSwitch(&FrameRatesSP, "Frame Period (discrete): %d. %s", index, FrameRatesSP.sp[index].name);
	 return true;
	 //}
     }    
     /* Compression */
	if (CompressSP && (!strcmp(name, CompressSP->name)))
     {
       IUResetSwitch(CompressSP);
       IUUpdateSwitch(CompressSP, states, names, n);
       CompressSP->s = IPS_OK;
       
       IDSetSwitch(CompressSP, NULL);
       return true;
     }    

     /* Image Type */
     if (!strcmp(name, ImageTypeSP.name))
     {
       IUResetSwitch(&ImageTypeSP);
       IUUpdateSwitch(&ImageTypeSP, states, names, n);
       ImageTypeSP.s = IPS_OK;
       if (ImageTypeS[0].s == ISS_ON) PrimaryCCD.setBPP(8); else PrimaryCCD.setBPP(32);

       IDSetSwitch(&ImageTypeSP, NULL);
       return true;
     }
     
     /* Stacking Mode */
     if (!strcmp(name, StackModeSP.name))
     {
       IUResetSwitch(&StackModeSP);
       IUUpdateSwitch(&StackModeSP, states, names, n);
       StackModeSP.s = IPS_OK;
       stackMode=IUFindOnSwitchIndex(&StackModeSP);

       IDSetSwitch(&StackModeSP, "Setting Stacking Mode: %s", StackModeS[stackMode].name);
       return true;
     }

     /* Video Stream */
     if (!strcmp(name, StreamSP.name))
     {
     
      if (checkPowerS(&StreamSP))
         return false;
       
       IUResetSwitch(&StreamSP);
       IUUpdateSwitch(&StreamSP, states, names, n);
       StreamSP.s = IPS_IDLE;
       
          
       //v4l_base->stop_capturing(errmsg);

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
       return true;
     }

     /* V4L2 Options/Menus */
     for (iopt=0; iopt<v4loptions; iopt++) 
       if (!strcmp (Options[iopt].name, name))
	 break;
     if (iopt < v4loptions)
       {
	 unsigned int ctrl_id, optindex;
	 IDLog("Toggle switch %s=%s\n", Options[iopt].name, Options[iopt].label);
	 if (checkPowerS(&Options[iopt]))
	   return false;
       
	 Options[iopt].s = IPS_IDLE;
	 IUResetSwitch(&Options[iopt]);
	 if (IUUpdateSwitch(&Options[iopt], states, names, n) < 0)
	   return false;
	
	 optindex=IUFindOnSwitchIndex(&Options[iopt]);
         ctrl_id = (*((unsigned int*) Options[iopt].aux));
	 IDLog("  On switch is (%d) %s=\"%s\", ctrl_id = %d\n", optindex, Options[iopt].sp[optindex].name, Options[iopt].sp[optindex].label, ctrl_id);
         if (v4l_base->setOPTControl( ctrl_id , optindex, ((iopt >= v4lstartextoptions)?true:false), errmsg) < 0)
         {
            Options[iopt].s = IPS_ALERT;
            IDSetSwitch(&Options[iopt], "Unable to adjust setting. %s", errmsg);
            return false;
         }
	 Options[iopt].s = IPS_OK;
	 IDSetSwitch(&Options[iopt], NULL);
	 return true;
       }

     lx->ISNewSwitch (dev, name, states, names, n);
     return INDI::CCD::ISNewSwitch (dev, name, states, names, n);
     
}

bool V4L2_Driver::ISNewText (const char *dev, const char *name, char *texts[], char *names[], int n)
{
	IText *tp;
	//IDLog("ISNewtext for device %s (%s)\n", dev, device_name);

       /* ignore if not ours */ 
       if (dev && strcmp (device_name, dev))
         return true;

       //IDLog("ISNewtext for property %s (%s)\n", name, PortTP.name);       
	if (!strcmp(name, PortTP.name) )
	{
	  PortTP.s = IPS_OK;
	  tp = IUFindText( &PortTP, names[0] );
	  //IDLog("  ISNewtext for text %s \n", names[0]);       
	  if (!tp)
	   return false;
	  //IDLog("  ISNewtext for text %s found\n", names[0]);       
          IUSaveText(tp, texts[0]);
	  IDSetText (&PortTP, NULL);
	  return true;
	}
       
	lx->ISNewText (dev, name, texts, names, n);
       return INDI::CCD::ISNewText (dev, name, texts, names, n);
}

bool V4L2_Driver::ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n)
{
  char errmsg[ERRMSGSIZ];

  /* ignore if not ours */
  if (dev && strcmp (device_name, dev))
    return true;

       /* Capture Size (Step/Continuous) */
  if ((!strcmp(name, CaptureSizesNP.name)))
     {
       if ((StreamSP.s == IPS_BUSY) ||  (ExposeTimeNP->s == IPS_BUSY)) {
	 IDMessage(device_name, "Can not set capture size while capturing.");
	 CaptureSizesNP.s = IPS_BUSY;
	 IDSetNumber(&CaptureSizesNP, NULL);
	 return false;
       } else {
	 unsigned int index, sizes[2], w, h;
	 double rsizes[2];
	 double fsizes[4];
	 const char *fnames[]={"X", "Y", "WIDTH", "HEIGHT"};
	 if (!strcmp(names[0], "Width")) {
	   sizes[0] = values[0];
	   sizes[1] = values[1];
	 } else {
	   sizes[0] = values[1];
	   sizes[1] = values[0];
	 }
	 if (v4l_base->setcapturesize(sizes[0], sizes[1], errmsg) == -1) {
	   IDMessage(device_name, "ERROR (setsize): %s", errmsg);
	   CaptureSizesNP.s = IPS_ALERT;
	   IDSetNumber(&CaptureSizesNP, NULL);
	   return false;
	 }
	 if (!strcmp(names[0], "Width")) {
	   w=v4l_base->getWidth(); rsizes[0]=(double)w;
	   h=v4l_base->getHeight();rsizes[1]=(double)h;
	 } else {
	   w=v4l_base->getWidth();rsizes[1]=(double)w;
	   h=v4l_base->getHeight();rsizes[0]=(double)h;
	 }
	 //fsizes[0]=0.0; fsizes[1]=0.0; 	 fsizes[2]=w; 	 fsizes[3]=h; 
	 //IUUpdateNumber(FrameNP, fsizes, (char **)fnames, 4);
	 //IDSetNumber(FrameNP, "Resetting crop area");
     INDI::CCD::UpdateCCDFrame(0, 0, w, h);
	 IUUpdateNumber(&CaptureSizesNP, rsizes, names, n);
	 V4LFrame->width = w;
	 V4LFrame->height= h;
	 PrimaryCCD.setResolutoin(w, h);
	 CaptureSizesNP.s = IPS_OK;
	 IDSetNumber(&CaptureSizesNP, "Capture size (step/cont): %dx%d", w, h);
	 return true;
       }
     }    
  
   if (!strcmp (ImageAdjustNP.name, name))
   {
     if (checkPowerN(&ImageAdjustNP))
       return false;
       
     ImageAdjustNP.s = IPS_IDLE;
     
     if (IUUpdateNumber(&ImageAdjustNP, values, names, n) < 0)
       return false;
     
     unsigned int ctrl_id;
     for (int i=0; i < ImageAdjustNP.nnp; i++)
     {
         ctrl_id = *((unsigned int *) ImageAdjustNP.np[i].aux0);
         if (v4l_base->setINTControl( ctrl_id , ImageAdjustNP.np[i].value, ((i >= v4lstartextadjustments)?true:false), errmsg) < 0)
         {
            ImageAdjustNP.s = IPS_ALERT;
            IDSetNumber(&ImageAdjustNP, "Unable to adjust setting. %s", errmsg);
            return false;
         }
     }
     
     ImageAdjustNP.s = IPS_OK;
     IDSetNumber(&ImageAdjustNP, NULL);
     return true;
   }
   
   
    /* Exposure */
    if (!strcmp (ExposeTimeNP->name, name))
    {
      int width  = v4l_base->getWidth();
      int height = v4l_base->getHeight();
       if (checkPowerN(ExposeTimeNP))
         return false;
    
        if (StreamS[0].s == ISS_ON)
	  v4l_base->stop_capturing(errmsg);

	StreamS[0].s  = ISS_OFF;
	StreamS[1].s  = ISS_ON;
	StreamSP.s    = IPS_IDLE;
	IDSetSwitch(&StreamSP, NULL);
        
	V4LFrame->expose = values[0];

        ExposeTimeNP->s   = IPS_BUSY;
	if (IUUpdateNumber(ExposeTimeNP, values, names, n) < 0)
	  return false;
	frameBytes  = ImageTypeS[0].s == ISS_ON ? width * height : width * height * 4;
	PrimaryCCD.setFrameBufferSize(frameBytes);
	/*  if (ImageTypeS[0].s == ISS_ON) {
	    V4LFrame->stackedFrame =(unsigned char *) realloc (V4LFrame->stackedFrame, sizeof(unsigned char) * frameBytes );
	    V4LFrame->Y=V4LFrame->stackedFrame;
	  } else {
	    V4LFrame->stackedFrame = (unsigned char *) realloc (V4LFrame->stackedFrame, sizeof(unsigned char) * frameBytes);
	    V4LFrame->colorBuffer=V4LFrame->stackedFrame;
	  }
  	}
	*/
	timerclear(&exposure_duration);
	exposure_duration.tv_sec = (long) values[0] ;
	exposure_duration.tv_usec = (long) ((values[0] - (double) exposure_duration.tv_sec)
                                 * 1000000.0) ;
	frameCount=0;
	gettimeofday(&capture_start, NULL);
	if (lx->isenabled())
	  startlongexposure(ExposeTimeN[0].value);
	else
	  v4l_base->start_capturing(errmsg);
	
        return true;
    } 
      
    return INDI::CCD::ISNewNumber(dev, name, values, names, n);
  	
}

void V4L2_Driver::startlongexposure(double timeinsec) {
  lxtimer=IEAddTimer((int)(timeinsec*1000.0), (IE_TCF *)lxtimerCallback, this);
  lx->startLx();
}

void V4L2_Driver::lxtimerCallback(void *userpointer) {
  char errmsg[ERRMSGSIZ];
  V4L2_Driver *p = (V4L2_Driver *)userpointer;
  p->lx->stopLx();
  IERmTimer(p->lxtimer);
  p->v4l_base->start_capturing(errmsg); // jump to new/updateFrame
}

bool V4L2_Driver::updateCCDFrame(int x, int y, int w, int h)
  {
  char errmsg[ERRMSGSIZ];
    if (checkPowerN(FrameNP))
      return false;
    
    int oldW = (int) FrameN[2].value;
    int oldH = (int) FrameN[3].value;
    
    //FrameNP->s = IPS_OK;
    
    //if (IUUpdateNumber(FrameNP, values, names, n) < 0)
    //return false;
    
    if (v4l_base->setcroprect(x, y, w, h, errmsg) != -1)
      {
	struct v4l2_rect crect;
	crect=v4l_base->getcroprect();
	
	V4LFrame->width = crect.width;
	V4LFrame->height= crect.height;
	//IDSetNumber(FrameNP, NULL);
    INDI::CCD::UpdateCCDFrame(x, y, w, h);
	return true;
      }
    else
      {
	IDMessage(device_name, "ERROR (setcroprect): %s", errmsg);
	//FrameN[2].value = oldW;
	//FrameN[3].value = oldH;
	//FrameNP->s = IPS_ALERT;
	//IDSetNumber(FrameNP, "Failed to set a new image size.");
      }
    
    return false;
  }


void V4L2_Driver::newFrame(void *p)
{
  ((V4L2_Driver *) (p))->updateFrame();
}

void V4L2_Driver::updateFrame()
{
  char errmsg[ERRMSGSIZ];
  static const unsigned int FRAME_DROP = 0;
  static int dropLarge = FRAME_DROP;

  if (StreamSP.s == IPS_BUSY)
  {
	// Ad hoc way of dropping frames
	frameCount++;
        //dropLarge--;
        //if (dropLarge == 0)
        //{
        //  dropLarge = (int) (((ImageTypeS[0].s == ISS_ON) ? FRAME_DROP : FRAME_DROP*3) * (FrameN[2].value / 160.0));
	//  updateStream();
        //  return;
        //}
	updateStream();
  }
  else if (ExposeTimeNP->s == IPS_BUSY)
  {
    struct timeval current_exposure;
    unsigned int i;
    unsigned char *src, *dest;
    
    gettimeofday(&capture_end,NULL);
    timersub(&capture_end, &capture_start, &current_exposure);
    if (ImageTypeS[0].s == ISS_ON) {
      src = v4l_base->getY();
    } else	{
      src = v4l_base->getColorBuffer(); 
    }  
    dest = (unsigned char *)PrimaryCCD.getFrameBuffer();;
    if (frameCount==0) 
      for (i=0; i< frameBytes; i++)
	*(dest++) = *(src++);
    else
      for (i=0; i< frameBytes; i++)
	*(dest++) += *(src++);

    frameCount+=1;
    if (lx->isenabled()) {
      if (!stackMode) {
	v4l_base->stop_capturing(errmsg);
	IDMessage(device_name, "Capture of LX frame took %ld.%06ld seconds.\n", current_exposure.tv_sec, current_exposure.tv_usec);
	INDI::CCD::ExposureComplete(&PrimaryCCD);
      }
    } else {
      if (!stackMode || timercmp(&current_exposure, &exposure_duration, >)) {
	v4l_base->stop_capturing(errmsg);
	IDMessage(device_name, "Capture of ONE frame (%d stacked frames) took %ld.%06ld seconds.\n", frameCount, current_exposure.tv_sec, current_exposure.tv_usec);
	INDI::CCD::ExposureComplete(&PrimaryCCD);
      }
    }
  }

}

void V4L2_Driver::updateStream()
{
 
   int width  = v4l_base->getWidth();
   int height = v4l_base->getHeight();
   uLongf compressedBytes = 0;
   uLong totalBytes;
   unsigned char *targetFrame;
   int r;
   
   if (ConnectS[0].s == ISS_OFF || StreamS[0].s == ISS_OFF) return;
   
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
   	imageB->blob = V4LFrame->compressedFrame;
   	imageB->bloblen = compressedBytes;
   	imageB->size = totalBytes;
   	strcpy(imageB->format, ".stream.z");
     }
     else
     {
       /* #3.B Send it uncompressed */
        imageB->blob = targetFrame;
   	imageB->bloblen = totalBytes;
   	imageB->size = totalBytes;
   	strcpy(imageB->format, ".stream");
     }
        
   imageBP->s = IPS_OK;
   IDSetBLOB (imageBP, NULL);
   
}


bool V4L2_Driver::AbortExposure() {
  char errmsg[ERRMSGSIZ];
  if (lx->isenabled())
    lx->stopLx();
  else
    v4l_base->stop_capturing(errmsg);
  return true;
}



bool V4L2_Driver::Connect()
{
  char errmsg[ERRMSGSIZ];
  if (!isConnected()) {
    if (v4l_base->connectCam(PortT[0].text, errmsg) < 0)
      {
	ConnectSP->s = IPS_IDLE;
	ConnectS[0].s = ISS_OFF;
	ConnectS[1].s = ISS_ON;
	IDSetSwitch(ConnectSP, "Error: unable to open device");
	IDLog("Error: %s\n", errmsg);
	return false;
      }
    
    /* Sucess! */
    ConnectS[0].s = ISS_ON;
    ConnectS[1].s = ISS_OFF;
    ConnectSP->s = IPS_OK;
    IDSetSwitch(ConnectSP, "V4L2 CCD Device is online. Initializing properties.");
    
    v4l_base->registerCallback(newFrame, this);
    
    lx->setCamerafd(v4l_base->fd);
    if (!(strcmp((const char *)v4l_base->cap.driver, "pwc")))
      IDMessage(device_name,"To use SPCLED Long exposure mode, load pwc module with \"modprobe pwc leds=0,255\"");

    IDLog("V4L2 CCD Device is online. Initializing properties.\n");
  }
  
  return true;
}

bool V4L2_Driver::Disconnect()
{
  if (isConnected()) {
    ConnectS[0].s = ISS_OFF;
    ConnectS[1].s = ISS_ON;
    ConnectSP->s = IPS_IDLE;
    
    v4l_base->disconnectCam((StreamSP.s == IPS_BUSY) ||  (ExposeTimeNP->s == IPS_BUSY));
    
    IDSetSwitch(ConnectSP, "Video4Linux Generic Device is offline.");
  }
  return true;
}

const char *V4L2_Driver::getDefaultName()
{
  return device_name;
}


/* Retrieves basic data from the device upon connection.*/
void V4L2_Driver::getBasicData()
{

  //int xmax, ymax, xmin, ymin;
  unsigned int w, h;
  unsigned int inputindex, formatindex;
  struct v4l2_fract frate;

  v4l_base->getinputs(&InputsSP);  
  v4l_base->getcaptureformats(&CaptureFormatsSP);
  v4l_base->getcapturesizes(&CaptureSizesSP, &CaptureSizesNP);
  v4l_base->getframerates(&FrameRatesSP, &FrameRateNP);
  
  w = v4l_base->getWidth();
  h = v4l_base->getHeight();
  V4LFrame->width = w;
  V4LFrame->height= h;

  inputindex=IUFindOnSwitchIndex(&InputsSP);
  formatindex=IUFindOnSwitchIndex(&CaptureFormatsSP);
  frate=(v4l_base->*(v4l_base->getframerate))();
  IDMessage(device_name, "Found intial Input \"%s\", Format \"%s\", Size %dx%d, Frame interval %d/%ds",
	    InputsSP.sp[inputindex].name, CaptureFormatsSP.sp[formatindex].name, w, h, frate.numerator, frate.denominator);  
	    
  IUSaveText(&camNameT[0], v4l_base->getDeviceName());
  IDSetText(&camNameTP, NULL);

  if (Options) free(Options);
  Options=NULL;
  v4loptions=0;
  updateV4L2Controls();
     
}

void V4L2_Driver::updateV4L2Controls()
{
  unsigned int i;
  // #1 Query for INTEGER controls, and fill up the structure
  free(ImageAdjustNP.np);
  ImageAdjustNP.nnp = 0;
  
  //if (v4l_base->queryINTControls(&ImageAdjustNP) > 0)
  //IDDefNumber(&ImageAdjustNP, NULL);
  v4l_base->queryControls(&ImageAdjustNP, &v4ladjustments, &v4lstartextadjustments, &Options, &v4loptions, &v4lstartextoptions, device_name, IMAGE_BOOLEAN) ;
  if (v4ladjustments > 0) IDDefNumber(&ImageAdjustNP, NULL);
  for (i=0; i < v4loptions; i++) {
      //IDLog("Def switch %d %s\n", i, Options[i].label);
      IDDefSwitch(&Options[i], NULL);
  }
      
  //v4l_base->enumerate_ctrl();

}

int V4L2_Driver::checkPowerS(ISwitchVectorProperty *sp)
{
  if (ConnectSP->s != IPS_OK)
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

int V4L2_Driver::checkPowerN(INumberVectorProperty *np)
{
   
  if (ConnectSP->s != IPS_OK)
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

int V4L2_Driver::checkPowerT(ITextVectorProperty *tp)
{

  if (ConnectSP->s != IPS_OK)
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

void V4L2_Driver::allocateBuffers()
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

void V4L2_Driver::releaseBuffers()
{
  free(fitsData);
  free(V4LFrame->Y);
  free(V4LFrame->U);
  free(V4LFrame->V);
  free(V4LFrame->colorBuffer);
  free(V4LFrame->compressedFrame);
  free (V4LFrame);
}


