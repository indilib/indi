#if 0
    V4L INDI Driver
    INDI Interface for V4L devices
    Copyright (C) 2003-2013 Jasem Mutlaq (mutlaqja@ikarustech.com)

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

  divider = 128.;
  
  // No guide head, no ST4 port, no cooling, no shutter

  CCDCapability cap;

  cap.canAbort = false;
  cap.canBin = true;
  cap.canSubFrame = true;
  cap.hasGuideHead = false;
  cap.hasCooler = false;
  cap.hasST4Port = false;
  cap.hasShutter = false;
  cap.hasBayer = false;

  SetCCDCapability(&cap);

  Options=NULL;
  v4loptions=0; 
  AbsExposureN=NULL;
  ManualExposureSP=NULL;
  stackMode=0;

  lx=new Lx();

  v4l2_record=new V4L2_Record();
  recorder=v4l2_record->getDefaultRecorder();
  recorder->init();
  direct_record=false;
  //const std::vector<unsigned int> &vsuppformats=decoder->getsupportedformats();
  IDLog("Using default recorder '%s'\n", recorder->getName());
}

V4L2_Driver::~V4L2_Driver()
{
  releaseBuffers();
}


bool V4L2_Driver::initProperties()
{
  
   INDI::CCD::initProperties();
   addDebugControl();

 /* Port */
  IUFillText(&PortT[0], "PORT", "Port", "/dev/video0");
  IUFillTextVector(&PortTP, PortT, NARRAY(PortT), getDeviceName(), "DEVICE_PORT", "Ports", OPTIONS_TAB, IP_RW, 0, IPS_IDLE);

 /* Video Stream */
  IUFillSwitch(&StreamS[0], "ON", "Stream On", ISS_OFF);
  IUFillSwitch(&StreamS[1], "OFF", "Stream Off", ISS_ON);
  IUFillSwitchVector(&StreamSP, StreamS, NARRAY(StreamS), getDeviceName(), "VIDEO_STREAM", "Video Stream", MAIN_CONTROL_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

  /* Image type */
  IUFillSwitch(&ImageTypeS[0], "Grey", "", ISS_ON);
  IUFillSwitch(&ImageTypeS[1], "Color", "", ISS_OFF);
  IUFillSwitchVector(&ImageTypeSP, ImageTypeS, NARRAY(ImageTypeS), getDeviceName(), "Image Type", "", IMAGE_SETTINGS_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

  /* Camera Name */
  IUFillText(&camNameT[0], "Model", "", NULL);
  IUFillTextVector(&camNameTP, camNameT, NARRAY(camNameT), getDeviceName(), "Camera Model", "", IMAGE_INFO_TAB, IP_RO, 0, IPS_IDLE);

  /* Stacking Mode */
  IUFillSwitch(&StackModeS[0], "None", "", ISS_ON);
  IUFillSwitch(&StackModeS[1], "Additive", "", ISS_OFF);
  IUFillSwitchVector(&StackModeSP, StackModeS, NARRAY(StackModeS), getDeviceName(), "Stack", "", MAIN_CONTROL_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

  /* Drop Frames */
  IUFillSwitch(&DropFrameS[0], "On", "", ISS_OFF);
  IUFillSwitch(&DropFrameS[1], "Off", "", ISS_ON);
  IUFillSwitchVector(&DropFrameSP, DropFrameS, NARRAY(DropFrameS), getDeviceName(), "Drop Frames", "", IMAGE_SETTINGS_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

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

  /* Record Frames */
  /* File */
  IUFillText(&RecordFileT[0], "RECORD_FILE_NAME", "File name", "/tmp/indimovie.ser");
  IUFillTextVector(&RecordFileTP, RecordFileT, NARRAY(RecordFileT), getDeviceName(), "RECORD_FILE", "Record File", MAIN_CONTROL_TAB, IP_RW, 0, IPS_IDLE);

 /* Video Record Switch */
  IUFillSwitch(&RecordS[0], "ON", "Record On", ISS_OFF);
  IUFillSwitch(&RecordS[1], "OFF", "Record Off", ISS_ON);
  IUFillSwitchVector(&RecordSP, RecordS, NARRAY(RecordS), getDeviceName(), "VIDEO_RECORD", "Video Record", MAIN_CONTROL_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);


  PrimaryCCD.getCCDInfo()->p = IP_RW;

  PrimaryCCD.setMinMaxStep("CCD_EXPOSURE", "CCD_EXPOSURE_VALUE", 0.001, 3600, 1, false);

  if (!lx->initProperties(this))
    DEBUG(INDI::Logger::DBG_WARNING, "Can not init Long Exposure");
  return true;
}

void V4L2_Driver::initCamBase()
{
    v4l_base = new V4L2_Base();
    v4l_base->setRecorder(recorder);
}

void V4L2_Driver::ISGetProperties (const char *dev)
{ 

  if (dev && strcmp (getDeviceName(), dev))
    return;

  INDI::CCD::ISGetProperties(dev);

  defineText(&PortTP);

  if (isConnected())
  {
    defineText(&camNameTP);
    defineSwitch(&StreamSP);
    defineSwitch(&StackModeSP);
    defineSwitch(&ImageTypeSP);
    defineSwitch(&InputsSP);
    defineSwitch(&CaptureFormatsSP);
    defineSwitch(&DropFrameSP);

    if (CaptureSizesSP.sp != NULL)
        defineSwitch(&CaptureSizesSP);
    else if  (CaptureSizesNP.np != NULL)
        defineNumber(&CaptureSizesNP);
   if (FrameRatesSP.sp != NULL)
       defineSwitch(&FrameRatesSP);
    else if  (FrameRateNP.np != NULL)
       defineNumber(&FrameRateNP);
  }

}

bool V4L2_Driver::updateProperties ()
{ 
  INDI::CCD::updateProperties();

  if (isConnected())
  {
    ExposeTimeNP=getNumber("CCD_EXPOSURE");
    ExposeTimeN=ExposeTimeNP->np;

    imageBP=getBLOB("CCD1");
    imageB=imageBP->bp;
    
    CompressSP=getSwitch("CCD_COMPRESSION");
    CompressS=CompressSP->sp;
    
    FrameNP=getNumber("CCD_FRAME");
    FrameN=FrameNP->np;
    
    defineText(&camNameTP);
    getBasicData();

    defineSwitch(&StreamSP);
    defineSwitch(&StackModeSP);
    defineSwitch(&ImageTypeSP);
    defineSwitch(&InputsSP);
    defineSwitch(&CaptureFormatsSP);
    defineSwitch(&DropFrameSP);

    if (CaptureSizesSP.sp != NULL)
        defineSwitch(&CaptureSizesSP);
    else if  (CaptureSizesNP.np != NULL)
        defineNumber(&CaptureSizesNP);
   if (FrameRatesSP.sp != NULL)
       defineSwitch(&FrameRatesSP);
    else if  (FrameRateNP.np != NULL)
       defineNumber(&FrameRateNP);

   defineSwitch(&RecordSP);  
   defineText(&RecordFileTP);

   SetCCDParams(V4LFrame->width, V4LFrame->height, 8, 5.6, 5.6);
   

   PrimaryCCD.setImageExtension("fits");


   if (v4l_base->isLXmodCapable()) lx->updateProperties();

  } else
  {
    unsigned int i;
    
    if (v4l_base->isLXmodCapable()) lx->updateProperties();
    
    
    deleteProperty(camNameTP.name);
    deleteProperty(StreamSP.name);
    deleteProperty(StackModeSP.name);
    deleteProperty(ImageTypeSP.name);
    deleteProperty(InputsSP.name);
    deleteProperty(CaptureFormatsSP.name);
    deleteProperty(DropFrameSP.name);

    if (CaptureSizesSP.sp != NULL)
        deleteProperty(CaptureSizesSP.name);
    else if  (CaptureSizesNP.np != NULL)
        deleteProperty(CaptureSizesNP.name);
   if (FrameRatesSP.sp != NULL)
       deleteProperty(FrameRatesSP.name);
    else if  (FrameRateNP.np != NULL)
       deleteProperty(FrameRateNP.name);

    deleteProperty(ImageAdjustNP.name);

    for (i=0; i<v4loptions; i++)
      deleteProperty(Options[i].name);
    if (Options) free(Options);
    Options=NULL;
    v4loptions=0;

    deleteProperty(RecordFileTP.name);
    deleteProperty(RecordSP.name);
  }
}

bool V4L2_Driver::ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n)
{
  char errmsg[ERRMSGSIZ];
  unsigned int iopt;
  
  /* ignore if not ours */
  if (dev && strcmp (getDeviceName(), dev))
    return true;
  
  /* Drop Frame */
  if (!strcmp(name, DropFrameSP.name))
  {
      IUUpdateSwitch(&DropFrameSP, states, names, n);
      v4l_base->setDropFrame(DropFrameS[0].s == ISS_ON);
      DropFrameSP.s = IPS_OK;
      IDSetSwitch(&DropFrameSP, NULL);
      return true;
  }

  /* Input */
  if ((!strcmp(name, InputsSP.name)))
    {
      if ((StreamSP.s == IPS_BUSY) ||  (ExposeTimeNP->s == IPS_BUSY) || (RecordSP.s == IPS_BUSY)) {
	DEBUG(INDI::Logger::DBG_ERROR, "Can not set input while capturing.");
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
          DEBUGF(INDI::Logger::DBG_SESSION, "ERROR (setinput): %s", errmsg);
	  IUResetSwitch(&InputsSP);
	  InputsSP.sp[oldindex].s=ISS_ON;
	  InputsSP.s = IPS_ALERT;
	  IDSetSwitch(&InputsSP, NULL);
	  return false;
	}
	deleteProperty(CaptureFormatsSP.name);
	v4l_base->getcaptureformats(&CaptureFormatsSP);
	defineSwitch(&CaptureFormatsSP);
	if (CaptureSizesSP.sp != NULL)
	  deleteProperty(CaptureSizesSP.name);
	else if  (CaptureSizesNP.np != NULL)
	  deleteProperty(CaptureSizesNP.name);
	
	v4l_base->getcapturesizes(&CaptureSizesSP, &CaptureSizesNP);
	
	if (CaptureSizesSP.sp != NULL)
	  defineSwitch(&CaptureSizesSP);
	else if  (CaptureSizesNP.np != NULL)
	  defineNumber(&CaptureSizesNP);
	InputsSP.s = IPS_OK;
	IDSetSwitch(&InputsSP, NULL);
	DEBUGF(INDI::Logger::DBG_SESSION, "Capture input: %d. %s", inputindex, InputsSP.sp[inputindex].name);
	return true;
      }
    }    
	
  /* Capture Format */
  if ((!strcmp(name, CaptureFormatsSP.name))) {
    if ((StreamSP.s == IPS_BUSY) ||  (ExposeTimeNP->s == IPS_BUSY) || (RecordSP.s == IPS_BUSY)) {
      DEBUG(INDI::Logger::DBG_ERROR, "Can not set format while capturing.");
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
	DEBUGF(INDI::Logger::DBG_SESSION, "ERROR (setformat): %s", errmsg);
	IUResetSwitch(&CaptureFormatsSP);
	CaptureFormatsSP.sp[oldindex].s=ISS_ON;
	CaptureFormatsSP.s = IPS_ALERT;
	IDSetSwitch(&CaptureFormatsSP, NULL);
	return false;
      }
      
      if (CaptureSizesSP.sp != NULL)
	deleteProperty(CaptureSizesSP.name);
      else if  (CaptureSizesNP.np != NULL)
	deleteProperty(CaptureSizesNP.name);
      v4l_base->getcapturesizes(&CaptureSizesSP, &CaptureSizesNP);
      
      if (CaptureSizesSP.sp != NULL)
	defineSwitch(&CaptureSizesSP);
      else if  (CaptureSizesNP.np != NULL)
	defineNumber(&CaptureSizesNP);
      CaptureFormatsSP.s = IPS_OK;
      
      direct_record=recorder->setpixelformat(v4l_base->fmt.fmt.pix.pixelformat);
      
      IDSetSwitch(&CaptureFormatsSP, "Capture format: %d. %s", index, CaptureFormatsSP.sp[index].name);
      return true;
    }
  }
  
  /* Capture Size (Discrete) */
  if ((!strcmp(name, CaptureSizesSP.name))) {
    if ((StreamSP.s == IPS_BUSY) ||  (ExposeTimeNP->s == IPS_BUSY) || (RecordSP.s == IPS_BUSY)) {
      DEBUG(INDI::Logger::DBG_ERROR, "Can not set capture size while capturing.");
      CaptureSizesSP.s = IPS_BUSY;
      IDSetSwitch(&CaptureSizesSP, NULL);
      return false;
    } else {
      unsigned int index, w, h;
      IUUpdateSwitch(&CaptureSizesSP, states, names, n);
      index=IUFindOnSwitchIndex(&CaptureSizesSP);
      sscanf(CaptureSizesSP.sp[index].name, "%dx%d", &w, &h);
      if (v4l_base->setcapturesize(w, h, errmsg) == -1) {
	DEBUGF(INDI::Logger::DBG_SESSION, "ERROR (setsize): %s", errmsg);
	CaptureSizesSP.s = IPS_ALERT;
	IDSetSwitch(&CaptureSizesSP, NULL);
	return false;
      }
      
      if (FrameRatesSP.sp != NULL)
	deleteProperty(FrameRatesSP.name);
      else if  (FrameRateNP.np != NULL)
	deleteProperty(FrameRateNP.name);
      v4l_base->getframerates(&FrameRatesSP, &FrameRateNP);
      if (FrameRatesSP.sp != NULL)
	defineSwitch(&FrameRatesSP);
	 else if  (FrameRateNP.np != NULL)
	   defineNumber(&FrameRateNP);
      
      PrimaryCCD.setFrame(0, 0, w, h);
      V4LFrame->width = w;
      V4LFrame->height= h;
      PrimaryCCD.setResolution(w, h);
      
      recorder->setsize(w, h);
      
      CaptureSizesSP.s = IPS_OK;
      IDSetSwitch(&CaptureSizesSP, "Capture size (discrete): %d. %s", index, CaptureSizesSP.sp[index].name);
      return true;
    }
  }

  /* Frame Rate (Discrete) */
  if ((!strcmp(name, FrameRatesSP.name)))  {
    unsigned int index;
    struct v4l2_fract frate;
    IUUpdateSwitch(&FrameRatesSP, states, names, n);
    index=IUFindOnSwitchIndex(&FrameRatesSP);
    sscanf(FrameRatesSP.sp[index].name, "%d/%d", &frate.numerator, &frate.denominator);
    if ((v4l_base->*(v4l_base->setframerate))(frate, errmsg) == -1)
      {
	DEBUGF(INDI::Logger::DBG_SESSION, "ERROR (setframerate): %s", errmsg);
	FrameRatesSP.s = IPS_ALERT;
	IDSetSwitch(&FrameRatesSP, NULL);
	return false;
      }
    
    FrameRatesSP.s = IPS_OK;
    IDSetSwitch(&FrameRatesSP, "Frame Period (discrete): %d. %s", index, FrameRatesSP.sp[index].name);
    return true;
  }
  
  /* Image Type */
  if (!strcmp(name, ImageTypeSP.name)) {
    IUResetSwitch(&ImageTypeSP);
    IUUpdateSwitch(&ImageTypeSP, states, names, n);
    ImageTypeSP.s = IPS_OK;
    if (ImageTypeS[0].s == ISS_ON) {
      PrimaryCCD.setBPP(8);
      PrimaryCCD.setNAxis(2);
    } else {
      //PrimaryCCD.setBPP(32);
      PrimaryCCD.setBPP(8);
      PrimaryCCD.setNAxis(3);
    }
    
    frameBytes  = (ImageTypeS[0].s == ISS_ON) ? (PrimaryCCD.getSubW() * PrimaryCCD.getSubH()):
      (PrimaryCCD.getSubW() * PrimaryCCD.getSubH() * 4);
    PrimaryCCD.setFrameBufferSize(frameBytes);
    
    IDSetSwitch(&ImageTypeSP, NULL);
    return true;
  }
  
  /* Stacking Mode */
  if (!strcmp(name, StackModeSP.name)) {
    IUResetSwitch(&StackModeSP);
    IUUpdateSwitch(&StackModeSP, states, names, n);
    StackModeSP.s = IPS_OK;
    stackMode=IUFindOnSwitchIndex(&StackModeSP);
    
    IDSetSwitch(&StackModeSP, "Setting Stacking Mode: %s", StackModeS[stackMode].name);
    return true;
  }
  
  /* Video Stream */
  if (!strcmp(name, StreamSP.name)) {
    bool is_streaming = (StreamSP.s == IPS_BUSY);
    bool is_recording = (RecordSP.s == IPS_BUSY);
    IUResetSwitch(&StreamSP);
    IUUpdateSwitch(&StreamSP, states, names, n);
    
    if (StreamS[0].s == ISS_ON) {
      if ((!is_streaming) && (!is_recording)) { 
	StreamSP.s  = IPS_BUSY; 
	frameCount = 0;
	DEBUG(INDI::Logger::DBG_DEBUG, "Starting the video stream.\n");
	v4l_base->start_capturing(errmsg);
      } else {
	if (!is_streaming) StreamSP.s=IPS_IDLE;
      }
    } else {
      StreamSP.s = IPS_IDLE;       
      if (is_streaming) {
	DEBUGF(INDI::Logger::DBG_DEBUG, "The video stream has been disabled. Frame count %d\n", frameCount);
	v4l_base->stop_capturing(errmsg);
      }
    }
    
    IDSetSwitch(&StreamSP, NULL);
    return true;
  }
  
  /* Record Stream */
  if (!strcmp(name, RecordSP.name)) {
    bool is_streaming = (StreamSP.s == IPS_BUSY);
    bool is_recording = (RecordSP.s == IPS_BUSY);
    IUResetSwitch(&RecordSP);
    IUUpdateSwitch(&RecordSP, states, names, n);
    
    if (RecordS[0].s == ISS_ON) {
      if ((!is_streaming) && (!is_recording)) { 
	frameCount = 0;
	DEBUG(INDI::Logger::DBG_SESSION, "Recording the video stream (no binning).\n");
	RecordSP.s  = IPS_BUSY; 
	if (!recorder->open(RecordFileT[0].text, errmsg)) {
	  RecordSP.s = IPS_ALERT;
	  IDSetSwitch(&RecordSP, NULL);
	  DEBUGF(INDI::Logger::DBG_ERROR, "%s", errmsg);
	  return false;
	}
	if (direct_record) {
	  DEBUG(INDI::Logger::DBG_SESSION, "Using direct recording (no soft crop, no frame count).\n");
	  v4l_base->doDecode(false);
	  v4l_base->doRecord(true);
	} else {
	  if (ImageTypeS[0].s == ISS_ON) 
	    recorder->setDefaultMono();
	  else
	    recorder->setDefaultColor();
	}
	v4l_base->start_capturing(errmsg);
      } else {
	if (!is_recording) RecordSP.s=IPS_IDLE;
      }
    } else {
      RecordSP.s = IPS_IDLE;   
      if (is_recording) {
	v4l_base->stop_capturing(errmsg);
	if (direct_record) {
	  v4l_base->doDecode(true);
	  v4l_base->doRecord(false);
	}
	recorder->close();
	DEBUGF(INDI::Logger::DBG_SESSION, "Recording stream has been disabled. Frame count %d\n", frameCount);
      }
    }
    
    IDSetSwitch(&RecordSP, NULL);
    return true;
  }

  /* V4L2 Options/Menus */
  for (iopt=0; iopt<v4loptions; iopt++) 
    if (!strcmp (Options[iopt].name, name))
      break;
  if (iopt < v4loptions) {
    unsigned int ctrl_id, optindex, ctrlindex;
    
    DEBUGF(INDI::Logger::DBG_DEBUG, "Toggle switch %s=%s\n", Options[iopt].name, Options[iopt].label);
    
    Options[iopt].s = IPS_IDLE;
    IUResetSwitch(&Options[iopt]);
    if (IUUpdateSwitch(&Options[iopt], states, names, n) < 0)
      return false;
    
    optindex=IUFindOnSwitchIndex(&Options[iopt]);
    if (Options[iopt].sp[optindex].aux != NULL) 
      ctrlindex= *(unsigned int *)(Options[iopt].sp[optindex].aux);
    else 
      ctrlindex=optindex;
    ctrl_id = (*((unsigned int*) Options[iopt].aux));
    DEBUGF(INDI::Logger::DBG_DEBUG, "  On switch is (%d) %s=\"%s\", ctrl_id = 0x%X ctrl_index=%d\n", optindex, 
	   Options[iopt].sp[optindex].name, Options[iopt].sp[optindex].label, ctrl_id, ctrlindex);
    if (v4l_base->setOPTControl( ctrl_id , ctrlindex,  errmsg) < 0) {
      Options[iopt].s = IPS_ALERT;
      IDSetSwitch(&Options[iopt], NULL);
      DEBUGF(INDI::Logger::DBG_ERROR, "Unable to adjust setting. %s", errmsg);
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

       /* ignore if not ours */ 
       if (dev && strcmp (getDeviceName(), dev))
         return true;

	if (!strcmp(name, PortTP.name) )
	{
	  PortTP.s = IPS_OK;
	  tp = IUFindText( &PortTP, names[0] );	  
	  if (!tp)
	   return false;
	  IUSaveText(tp, texts[0]);
	  IDSetText (&PortTP, NULL);
	  return true;
	}

	if (!strcmp(name, RecordFileTP.name) )
	{
	  RecordFileTP.s = IPS_OK;
	  tp = IUFindText( &RecordFileTP, names[0] );	  
	  if (!tp)
	   return false;
	  IUSaveText(tp, texts[0]);
	  IDSetText (&RecordFileTP, NULL);
	  return true;
	}
       
	lx->ISNewText (dev, name, texts, names, n);
    return INDI::CCD::ISNewText (dev, name, texts, names, n);
}

bool V4L2_Driver::ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n)
{
  char errmsg[ERRMSGSIZ];

  /* ignore if not ours */
  if (dev && strcmp (getDeviceName(), dev))
    return true;

       /* Capture Size (Step/Continuous) */
  if ((!strcmp(name, CaptureSizesNP.name)))
     {
       if ((StreamSP.s == IPS_BUSY) ||  (ExposeTimeNP->s == IPS_BUSY) || (RecordSP.s == IPS_BUSY)) {
     DEBUG(INDI::Logger::DBG_ERROR, "Can not set capture size while capturing.");
	 CaptureSizesNP.s = IPS_BUSY;
     IDSetNumber(&CaptureSizesNP, NULL);
	 return false;
       } else
     {
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
     if (v4l_base->setcapturesize(sizes[0], sizes[1], errmsg) == -1)
     {
       DEBUGF(INDI::Logger::DBG_SESSION, "ERROR (setsize): %s", errmsg);
	   CaptureSizesNP.s = IPS_ALERT;
       IDSetNumber(&CaptureSizesNP, NULL);
	   return false;
	 }
     if (!strcmp(names[0], "Width"))
     {
	   w=v4l_base->getWidth(); rsizes[0]=(double)w;
	   h=v4l_base->getHeight();rsizes[1]=(double)h;
	 } else {
	   w=v4l_base->getWidth();rsizes[1]=(double)w;
	   h=v4l_base->getHeight();rsizes[0]=(double)h;
	 }

     PrimaryCCD.setFrame(0, 0, w, h);
     IUUpdateNumber(&CaptureSizesNP, rsizes, names, n);
     V4LFrame->width = w;
     V4LFrame->height= h;
     PrimaryCCD.setResolution(w, h);
     CaptureSizesNP.s = IPS_OK;
     frameBytes  = (ImageTypeS[0].s == ISS_ON) ? (PrimaryCCD.getSubW() * PrimaryCCD.getSubH()):
                                                 (PrimaryCCD.getSubW() * PrimaryCCD.getSubH() * 4);
     PrimaryCCD.setFrameBufferSize(frameBytes);
     
     recorder->setsize(w, h);

     IDSetNumber(&CaptureSizesNP, "Capture size (step/cont): %dx%d", w, h);
     return true;
     }
    }
  
  if (!strcmp (ImageAdjustNP.name, name))
    {      
      ImageAdjustNP.s = IPS_IDLE;
      
      if (IUUpdateNumber(&ImageAdjustNP, values, names, n) < 0)
	return false;
      
      unsigned int ctrl_id;
      for (int i=0; i < ImageAdjustNP.nnp; i++)
	{
	  ctrl_id = *((unsigned int *) ImageAdjustNP.np[i].aux0);
	  
	  DEBUGF(INDI::Logger::DBG_DEBUG, "  Setting %s (%s) to %d, ctrl_id = 0x%X \n", ImageAdjustNP.np[i].name, ImageAdjustNP.np[i].label, (int)ImageAdjustNP.np[i].value, ctrl_id);
	  
	  if (v4l_base->setINTControl( ctrl_id , ImageAdjustNP.np[i].value, errmsg) < 0)
	    {
	      /* Some controls may become read-only depending on selected options
	      ImageAdjustNP.s = IPS_ALERT;
	      IDSetNumber(&ImageAdjustNP, "Unable to adjust setting. %s", errmsg);
	      return false;
	      */
	      DEBUGF(INDI::Logger::DBG_WARNING,"Unable to adjust %s (ctrl_id =  0x%X)",  ImageAdjustNP.np[i].label, ctrl_id);
	      v4l_base->getControl(ctrl_id, &(ImageAdjustNP.np[i].value), errmsg);
	    }
	}
      
      ImageAdjustNP.s = IPS_OK;
      IDSetNumber(&ImageAdjustNP, NULL);
      return true;
    }
   
   /* Exposure */
  if (!strcmp (ExposeTimeNP->name, name))
    {
      bool rc;
      int width  = v4l_base->getWidth();
      int height = v4l_base->getHeight();
      
      if (StreamS[0].s == ISS_ON)
	v4l_base->stop_capturing(errmsg);
      
      StreamS[0].s  = ISS_OFF;
      StreamS[1].s  = ISS_ON;
      StreamSP.s    = IPS_IDLE;
      IDSetSwitch(&StreamSP, NULL);
      
      V4LFrame->expose = values[0];
 
      if (AbsExposureN && ManualExposureSP && (AbsExposureN->max >= (V4LFrame->expose * 10000)))
	{
	  DEBUGF(INDI::Logger::DBG_SESSION, "Using device manual exposure (max %f, required %f).", AbsExposureN->max, (V4LFrame->expose * 10000));
	  rc = setManualExposure(V4LFrame->expose);
	  if (rc == false)
	    DEBUG(INDI::Logger::DBG_WARNING, "Unable to set manual exposure, falling back to auto exposure.");
	}
      
      timerclear(&exposure_duration);
      exposure_duration.tv_sec = (long) values[0] ;
      exposure_duration.tv_usec = (long) ((values[0] - (double) exposure_duration.tv_sec)
					  * 1000000.0) ;
      frameCount=0;
      gettimeofday(&capture_start, NULL);
      if (lx->isenabled()) {
		rc=startlongexposure(V4LFrame->expose);
        if (rc == false)
			DEBUG(INDI::Logger::DBG_WARNING, "Unable to start long exposure, falling back to auto exposure.");
		else {
			if( lx->getLxmode() == LXSERIAL ) {
				v4l_base->start_capturing( errmsg ); 
			}
		}
	  }
	  else
		v4l_base->start_capturing(errmsg);
      
      ExposeTimeNP->s   = IPS_BUSY;
      if (IUUpdateNumber(ExposeTimeNP, values, names, n) < 0)
	return false;
      
      return true;
    } 
  
  return INDI::CCD::ISNewNumber(dev, name, values, names, n);
  	
}

bool V4L2_Driver::setManualExposure(double duration) {

  if (AbsExposureN == NULL || ManualExposureSP == NULL)
    return false;
  
  char errmsg[MAXRBUF];
  unsigned int ctrl_id, ctrlindex;
  
  // Manual mode should be set before changing Exposure (Auto)
  if (ManualExposureSP->sp[0].s == ISS_OFF) {
    ManualExposureSP->sp[0].s = ISS_ON;
    ManualExposureSP->sp[1].s = ISS_OFF;
    ManualExposureSP->s = IPS_IDLE;
    
    if (ManualExposureSP->sp[0].aux != NULL)
      ctrlindex= *(unsigned int *)(ManualExposureSP->sp[0].aux);
    else
      ctrlindex=0;
    
    ctrl_id = (*((unsigned int*) ManualExposureSP->aux));
    if (v4l_base->setOPTControl( ctrl_id , ctrlindex,  errmsg) < 0) {
      ManualExposureSP->sp[0].s = ISS_OFF;
      ManualExposureSP->sp[1].s = ISS_ON;
      ManualExposureSP->s = IPS_ALERT;
      IDSetSwitch(ManualExposureSP, NULL);
      DEBUGF(INDI::Logger::DBG_ERROR, "Unable to adjust setting. %s", errmsg);
      return false;
    }
    
    ManualExposureSP->s = IPS_OK;
    IDSetSwitch(ManualExposureSP, NULL);
  }
  
  /* N.B. Check how this differs from one camera to another. This is just a proof of concept for now */
  if (duration * 10000 != AbsExposureN->value) {
    double curVal = AbsExposureN->value;
    AbsExposureN->value = duration * 10000;
    ctrl_id = *((unsigned int *)  AbsExposureN->aux0);
    if (v4l_base->setINTControl( ctrl_id , AbsExposureN->value, errmsg) < 0) {
      ImageAdjustNP.s = IPS_ALERT;
      AbsExposureN->value = curVal;
      IDSetNumber(&ImageAdjustNP, "Unable to adjust AbsExposure. %s", errmsg);
      return false;
    }
    
    /*
      for (int i=0; i < ImageAdjustNP.nnp; i++) {
      ctrl_id = *((unsigned int *) ImageAdjustNP.np[i].aux0);
      
      if (v4l_base->setINTControl( ctrl_id , ImageAdjustNP.np[i].value, errmsg) < 0) {
      ImageAdjustNP.s = IPS_ALERT;
      AbsExposureN->value = curVal;
      IDSetNumber(&ImageAdjustNP, "Unable to adjust setting. %s", errmsg);
      return false;
      }
      }
    */
    ImageAdjustNP.s = IPS_OK;
    IDSetNumber(&ImageAdjustNP, NULL);
  }
  
  return true;
}

bool V4L2_Driver::startlongexposure(double timeinsec)
{
  lxtimer=IEAddTimer((int)(timeinsec*1000.0), (IE_TCF *)lxtimerCallback, this);
  v4l_base->setlxstate( LX_ACCUMULATING );
  return (lx->startLx());
}

void V4L2_Driver::lxtimerCallback(void *userpointer)
{
  char errmsg[ERRMSGSIZ];
  V4L2_Driver *p = (V4L2_Driver *)userpointer;
  p->lx->stopLx();
  p->v4l_base->setlxstate( LX_TRIGGERED );
  IERmTimer(p->lxtimer);
  if( !p->v4l_base->isstreamactive() )
	p->v4l_base->start_capturing(errmsg); // jump to new/updateFrame
}

bool V4L2_Driver::UpdateCCDBin(int hor, int ver)
{
    if (hor != ver)
    {
        DEBUGF(INDI::Logger::DBG_WARNING, "Cannot accept asymmetrical binning %dx%d.", hor, ver);
        return false;
    }

    if (hor != 1 && hor != 2 && hor !=4)
    {
        DEBUG(INDI::Logger::DBG_WARNING, "Can only accept 1x1, 2x2, and 4x4 binning.");
        return false;
    }

    PrimaryCCD.setBin(hor, ver);

  return true;
}

void V4L2_Driver::binFrame()
{
    int bin;
    if ( (bin = PrimaryCCD.getBinX()) == 1)
        return;

    int w = PrimaryCCD.getSubW();
    int h = PrimaryCCD.getSubH();

    int bin_w = w / bin;
    int bin_h = h / bin;

    unsigned char *bin_buffer = (unsigned char *) malloc(bin_w * bin_h * sizeof(char));
    unsigned char *buffer = (unsigned char *) PrimaryCCD.getFrameBuffer();
    //unsigned char *bin_buffer = buffer;

    memset(bin_buffer, 0, bin_w * bin_h * sizeof(char));

    unsigned char *bin_buf = bin_buffer;

    unsigned char val;
    for (int i=0; i < h; i+= bin)
        for (int j=0; j < w; j+= bin)
        {
            for (int k=0; k < bin; k++)
            {
                for (int l=0; l < bin; l++)
                {
                    val = *(buffer + j + (i+k) * w + l);
                    if (val + *bin_buf > 255)
                        *bin_buffer = 255;
                    else
                        *bin_buf  += val;
                }
            }

            bin_buf++;
        }

    PrimaryCCD.setFrameBuffer((char *) bin_buffer);
    PrimaryCCD.setFrameBufferSize(bin_w * bin_h * sizeof(char), false);
    free (buffer);
}

bool V4L2_Driver::UpdateCCDFrame(int x, int y, int w, int h)
{
  char errmsg[ERRMSGSIZ];
       
  //DEBUGF(INDI::Logger::DBG_SESSION, "calling updateCCDFrame: %d %d %d %d", x, y, w, h);
  //IDLog("calling updateCCDFrame: %d %d %d %d\n", x, y, w, h);
  if (v4l_base->setcroprect(x, y, w, h, errmsg) != -1) {
    struct v4l2_rect crect;
    crect=v4l_base->getcroprect();
    
    V4LFrame->width = crect.width;
    V4LFrame->height= crect.height;
    PrimaryCCD.setFrame(x, y, w, h);
    frameBytes  = (ImageTypeS[0].s == ISS_ON) ? (PrimaryCCD.getSubW() * PrimaryCCD.getSubH()):
      (PrimaryCCD.getSubW() * PrimaryCCD.getSubH() * 4);
    PrimaryCCD.setFrameBufferSize(frameBytes);
    recorder->setsize(w, h);
    //DEBUGF(INDI::Logger::DBG_SESSION, "updateCCDFrame ok: %d %d %d %d", x, y, w, h);
    //IDLog("updateCCDFrame ok: %d %d %d %d\n", x, y, w, h);
    return true;
  } else {
    DEBUGF(INDI::Logger::DBG_SESSION, "ERROR (setcroprect): %s", errmsg);
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

  if (StreamSP.s == IPS_BUSY)
  {
	frameCount++;
	updateStream();
  }
  else if (RecordSP.s == IPS_BUSY)
  {
	frameCount++;
	recordStream();
  }
  else if (ExposeTimeNP->s == IPS_BUSY)
  {
    PrimaryCCD.setExposureDuration(ExposeTimeN[0].value);
    struct timeval current_exposure;
    unsigned int i;
    unsigned char *src, *dest;
    
    gettimeofday(&capture_end,NULL);
    timersub(&capture_end, &capture_start, &current_exposure);

    if (ImageTypeS[0].s == ISS_ON) {
      src = v4l_base->getY();
      dest = (unsigned char *)PrimaryCCD.getFrameBuffer();
      if (frameCount==0)
        for (i=0; i< frameBytes; i++)
	  *(dest++) = *(src++);
      else
        for (i=0; i< frameBytes; i++)
	  *(dest++) += *(src++);
      binFrame();
    } else {
      // Binning not supported in color images for now
      src = v4l_base->getColorBuffer();
      dest = (unsigned char *)PrimaryCCD.getFrameBuffer();
      unsigned char *red = dest;
      unsigned char *green = dest + v4l_base->getWidth() * v4l_base->getHeight();
      unsigned char *blue = dest + v4l_base->getWidth() * v4l_base->getHeight() * 2;
      
      for (int i=0; i <  frameBytes; i+=4)
      {
          *(blue++) = *(src+i);
          *(green++) = *(src+i+1);
          *(red++) = *(src+i+2);
      }
    }

    frameCount+=1;

    if (lx->isenabled())
    {
      //if (!stackMode)
      //{
	v4l_base->stop_capturing(errmsg);
	DEBUGF(INDI::Logger::DBG_SESSION, "Capture of LX frame took %ld.%06ld seconds.\n", current_exposure.tv_sec, current_exposure.tv_usec);
	ExposureComplete(&PrimaryCCD);
	PrimaryCCD.setFrameBufferSize(frameBytes);
	//}
    } else {
      if (!stackMode || timercmp(&current_exposure, &exposure_duration, >))
	{
	  v4l_base->stop_capturing(errmsg);
	  DEBUGF(INDI::Logger::DBG_SESSION, "Capture of ONE frame (%d stacked frames) took %ld.%06ld seconds.\n", frameCount, current_exposure.tv_sec, current_exposure.tv_usec);
	  ExposureComplete(&PrimaryCCD);
	  PrimaryCCD.setFrameBufferSize(frameBytes);
	}
    }
  }

}

void V4L2_Driver::recordStream()
{
  if (RecordS[0].s == ISS_OFF) return;
  if (ImageTypeS[0].s == ISS_ON)
    recorder->writeFrameMono(v4l_base->getY());
  else
    recorder->writeFrameColor(v4l_base->getRGBBuffer());
}

void V4L2_Driver::updateStream()
{
   int width  = v4l_base->getWidth();
   int height = v4l_base->getHeight();
   uLongf compressedBytes = 0;
   uLong totalBytes;
   unsigned char *targetFrame;
   int r;
   
   if (StreamS[0].s == ISS_OFF) return;
   
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


bool V4L2_Driver::AbortExposure()
{
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
  if (!isConnected())
  {
    if (v4l_base->connectCam(PortT[0].text, errmsg) < 0)
    {
        DEBUGF(INDI::Logger::DBG_ERROR, "Error: unable to open device. %s", errmsg);
        return false;
    }
    
    /* Sucess! */
    DEBUG(INDI::Logger::DBG_SESSION, "V4L2 CCD Device is online. Initializing properties.");
    
    v4l_base->registerCallback(newFrame, this);
    
    lx->setCamerafd(v4l_base->fd);



    if (!(strcmp((const char *)v4l_base->cap.driver, "pwc")))
      DEBUG(INDI::Logger::DBG_SESSION,"To use SPCLED Long exposure mode, load pwc module with \"modprobe pwc leds=0,255\"");

  }
  
  return true;
}

bool V4L2_Driver::Disconnect()
{
  if (isConnected()) {
    v4l_base->disconnectCam((StreamSP.s == IPS_BUSY) ||  (ExposeTimeNP->s == IPS_BUSY) || (RecordSP.s == IPS_BUSY)); 
    if ((StreamSP.s == IPS_BUSY) ||  (ExposeTimeNP->s == IPS_BUSY) || (RecordSP.s == IPS_BUSY))
      recorder->close();
  }
  return true;
}

const char *V4L2_Driver::getDefaultName()
{
  return (char *) "V4L2 CCD";
}


/* Retrieves basic data from the device upon connection.*/
void V4L2_Driver::getBasicData()
{

  //int xmax, ymax, xmin, ymin;
  unsigned int w, h;
  int inputindex=-1, formatindex=-1;
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
  if (inputindex >= 0  && formatindex >= 0)
    DEBUGF(INDI::Logger::DBG_SESSION, "Found intial Input \"%s\", Format \"%s\", Size %dx%d, Frame interval %d/%ds",
        InputsSP.sp[inputindex].name, CaptureFormatsSP.sp[formatindex].name, w, h, frate.numerator, frate.denominator);
  else
      DEBUGF(INDI::Logger::DBG_SESSION, "Found intial size %dx%d, frame interval %d/%ds",
           w, h, frate.numerator, frate.denominator);
	    
  IUSaveText(&camNameT[0], v4l_base->getDeviceName());
  IDSetText(&camNameTP, NULL);

  if (Options) free(Options);
  Options=NULL;
  v4loptions=0;
  updateV4L2Controls();


  PrimaryCCD.setResolution(w, h);
  PrimaryCCD.setFrame(0,0, w,h);
  frameBytes  = (ImageTypeS[0].s == ISS_ON) ? (PrimaryCCD.getSubW() * PrimaryCCD.getSubH()):
                                              (PrimaryCCD.getSubW() * PrimaryCCD.getSubH() * 4);
  PrimaryCCD.setFrameBufferSize(frameBytes);

  direct_record=recorder->setpixelformat(v4l_base->fmt.fmt.pix.pixelformat);
  recorder->setsize(w, h);
     
}

void V4L2_Driver::updateV4L2Controls()
{
  unsigned int i;
  // #1 Query for INTEGER controls, and fill up the structure
  free(ImageAdjustNP.np);
  ImageAdjustNP.nnp = 0;
  
  //if (v4l_base->queryINTControls(&ImageAdjustNP) > 0)
  //defineNumber(&ImageAdjustNP);
  v4l_base->enumerate_ext_ctrl();
  useExtCtrl=false;
  if  (v4l_base->queryExtControls(&ImageAdjustNP, &v4ladjustments, &Options, &v4loptions, getDeviceName(), IMAGE_BOOLEAN))
    useExtCtrl=true;
  else
    v4l_base->queryControls(&ImageAdjustNP, &v4ladjustments, &Options, &v4loptions, getDeviceName(), IMAGE_BOOLEAN) ;
  if (v4ladjustments > 0)
  {
      defineNumber(&ImageAdjustNP);

      for (int i=0; i < ImageAdjustNP.nnp; i++)
      {
          if (!strcmp(ImageAdjustNP.np[i].label,  "Exposure (Absolute)"))
          {
              AbsExposureN = ImageAdjustNP.np+i;
              break;
          }
      }
  }
  for (i=0; i < v4loptions; i++)
  {
      //IDLog("Def switch %d %s\n", i, Options[i].label);
      defineSwitch(&Options[i]);

      if (!strcmp(Options[i].label, "Exposure, Auto"))
          ManualExposureSP = Options+i;
  }
      
  //v4l_base->enumerate_ctrl();

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


