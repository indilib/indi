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
  //sigevent sevp;
  //struct itimerspec fpssettings;  

  allocateBuffers();

  divider = 128.;
  
  // No guide head, no ST4 port, no cooling, no shutter

  uint32_t cap = 0;

  cap = CCD_CAN_BIN | CCD_CAN_SUBFRAME | CCD_HAS_STREAMING;

  SetCCDCapability(cap);
  
  is_capturing = false;
  is_exposing = false;

  Options=NULL;
  v4loptions=0; 
  AbsExposureN=NULL;
  ManualExposureSP=NULL;

  stackMode=STACK_NONE;

  lx=new Lx();

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

  /* Color space */
  IUFillSwitch(&ImageColorS[0], "CCD_COLOR_GRAY", "Gray", ISS_ON);
  IUFillSwitch(&ImageColorS[1], "CCD_COLOR_RGB", "Color", ISS_OFF);
  IUFillSwitchVector(&ImageColorSP, ImageColorS, NARRAY(ImageColorS), getDeviceName(), "CCD_COLOR_SPACE", "Image Type", IMAGE_SETTINGS_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

  /* Image depth */
  IUFillSwitch(&ImageDepthS[0], "8 bits", "", ISS_ON);
  IUFillSwitch(&ImageDepthS[1], "16 bits", "", ISS_OFF);
  IUFillSwitchVector(&ImageDepthSP, ImageDepthS, NARRAY(ImageDepthS), getDeviceName(), "Image Depth", "", IMAGE_SETTINGS_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

  /* Camera Name */
  IUFillText(&camNameT[0], "Model", "", NULL);
  IUFillTextVector(&camNameTP, camNameT, NARRAY(camNameT), getDeviceName(), "Camera", "", IMAGE_INFO_TAB, IP_RO, 0, IPS_IDLE);

  /* Stacking Mode */
  IUFillSwitch(&StackModeS[STACK_NONE], "None", "", ISS_ON);
  IUFillSwitch(&StackModeS[STACK_MEAN], "Mean", "", ISS_OFF);
  IUFillSwitch(&StackModeS[STACK_ADDITIVE], "Additive", "", ISS_OFF);
  IUFillSwitch(&StackModeS[STACK_TAKE_DARK], "Take Dark", "", ISS_OFF);
  IUFillSwitch(&StackModeS[STACK_RESET_DARK], "Reset Dark", "", ISS_OFF);
  IUFillSwitchVector(&StackModeSP, StackModeS, NARRAY(StackModeS), getDeviceName(), "Stack", "", MAIN_CONTROL_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

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
  /* Capture Colorspace */
  IUFillText(&CaptureColorSpaceT[0], "Name", "", NULL);
  IUFillText(&CaptureColorSpaceT[1], "YCbCr Encoding", "", NULL);
  IUFillText(&CaptureColorSpaceT[2], "Quantization", "", NULL);
  IUFillTextVector(&CaptureColorSpaceTP, CaptureColorSpaceT, NARRAY(CaptureColorSpaceT), getDeviceName(), "V4L2_COLORSPACE", "ColorSpace", IMAGE_INFO_TAB, IP_RO, 0, IPS_IDLE);
  
  /* Color Processing */
  IUFillSwitch(&ColorProcessingS[0], "Quantization", "", ISS_ON);
  IUFillSwitch(&ColorProcessingS[1], "Color Conversion", "", ISS_OFF);
  IUFillSwitch(&ColorProcessingS[2], "Linearization", "", ISS_OFF);
  IUFillSwitchVector(&ColorProcessingSP, ColorProcessingS, NARRAY(ColorProcessingS), getDeviceName(), "V4L2_COLOR_PROCESSING", "Color Process", CAPTURE_FORMAT, IP_RW, ISR_NOFMANY, 0, IPS_IDLE);


  /* V4L2 Settings */  
  IUFillNumberVector(&ImageAdjustNP, NULL, 0, getDeviceName(), "Image Adjustments", "", IMAGE_GROUP, IP_RW, 60, IPS_IDLE);  

  PrimaryCCD.getCCDInfo()->p = IP_RW;

  PrimaryCCD.setMinMaxStep("CCD_EXPOSURE", "CCD_EXPOSURE_VALUE", 0.001, 3600, 1, false);

  if (!lx->initProperties(this))
    DEBUG(INDI::Logger::DBG_WARNING, "Can not init Long Exposure");
  return true;
}

void V4L2_Driver::initCamBase()
{
    v4l_base = new V4L2_Base();
    v4l_base->setRecorder(streamer->getRecorder());
}

void V4L2_Driver::ISGetProperties (const char *dev)
{ 

  if (dev && strcmp (getDeviceName(), dev))
    return;

  INDI::CCD::ISGetProperties(dev);

  defineText(&PortTP);
  loadConfig(true, "DEVICE_PORT");

  if (isConnected())
  {
    defineText(&camNameTP);

    defineSwitch(&ImageColorSP);
    defineSwitch(&InputsSP);
    defineSwitch(&CaptureFormatsSP);    
   
    if (CaptureSizesSP.sp != NULL)
        defineSwitch(&CaptureSizesSP);
    else if  (CaptureSizesNP.np != NULL)
        defineNumber(&CaptureSizesNP);
    if (FrameRatesSP.sp != NULL)
       defineSwitch(&FrameRatesSP);
    else if  (FrameRateNP.np != NULL)
       defineNumber(&FrameRateNP);
    
#ifdef WITH_V4L2_EXPERIMENTS
    defineSwitch(&ImageDepthSP);
    defineSwitch(&StackModeSP);
    defineSwitch(&ColorProcessingSP);
    defineText(&CaptureColorSpaceTP);
#endif
  }
}

bool V4L2_Driver::updateProperties ()
{ 
  INDI::CCD::updateProperties();

  if (isConnected())
  {
    //ExposeTimeNP=getNumber("CCD_EXPOSURE");
    //ExposeTimeN=ExposeTimeNP->np;
    
    CompressSP=getSwitch("CCD_COMPRESSION");
    CompressS=CompressSP->sp;
    
    FrameNP=getNumber("CCD_FRAME");
    FrameN=FrameNP->np;
    
    defineText(&camNameTP);
    getBasicData();

    defineSwitch(&ImageColorSP);
    defineSwitch(&InputsSP);
    defineSwitch(&CaptureFormatsSP);

    if (CaptureSizesSP.sp != NULL)
        defineSwitch(&CaptureSizesSP);
    else if  (CaptureSizesNP.np != NULL)
        defineNumber(&CaptureSizesNP);
    if (FrameRatesSP.sp != NULL)
       defineSwitch(&FrameRatesSP);
    else if  (FrameRateNP.np != NULL)
       defineNumber(&FrameRateNP);

#ifdef WITH_V4L2_EXPERIMENTS
   defineSwitch(&ImageDepthSP);
    defineSwitch(&StackModeSP);
    defineSwitch(&ColorProcessingSP);
    defineText(&CaptureColorSpaceTP);
#endif

    SetCCDParams(V4LFrame->width, V4LFrame->height, V4LFrame->bpp, 5.6, 5.6);
    PrimaryCCD.setImageExtension("fits");

    if (v4l_base->isLXmodCapable()) lx->updateProperties();
    return true;

  } else
  {
    unsigned int i;
    
    if (v4l_base->isLXmodCapable()) lx->updateProperties();
    
    
    deleteProperty(camNameTP.name);

    deleteProperty(ImageColorSP.name);
    deleteProperty(InputsSP.name);
    deleteProperty(CaptureFormatsSP.name);

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

#ifdef WITH_V4L2_EXPERIMENTS
    deleteProperty(ImageDepthSP.name);
    deleteProperty(StackModeSP.name);
    deleteProperty(ColorProcessingSP.name);
    deleteProperty(CaptureColorSpaceTP.name);
#endif
    
    return true;
  }
}

bool V4L2_Driver::ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n)
{
  char errmsg[ERRMSGSIZ];
  unsigned int iopt;
  
  /* ignore if not ours */
  if (dev && strcmp (getDeviceName(), dev))
    return true;
   
  /* Input */
  if ((!strcmp(name, InputsSP.name)))
  {
      //if ((StreamSP.s == IPS_BUSY) ||  (ExposeTimeNP->s == IPS_BUSY) || (RecordStreamSP.s == IPS_BUSY)) {
    if (PrimaryCCD.isExposing() || streamer->isBusy())
    {
        DEBUG(INDI::Logger::DBG_ERROR, "Can not set input while capturing.");
        InputsSP.s = IPS_ALERT;
        IDSetSwitch(&InputsSP, NULL);
        return false;
    }
    else
    {
        unsigned int inputindex, oldindex;
        oldindex=IUFindOnSwitchIndex(&InputsSP);
        IUResetSwitch(&InputsSP);
        IUUpdateSwitch(&InputsSP, states, names, n);
        inputindex=IUFindOnSwitchIndex(&InputsSP);

        if (v4l_base->setinput(inputindex, errmsg) == -1)
        {
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
  if ((!strcmp(name, CaptureFormatsSP.name)))
  {
    //if ((StreamSP.s == IPS_BUSY) ||  (ExposeTimeNP->s == IPS_BUSY) || (RecordStreamSP.s == IPS_BUSY)) {
      if (PrimaryCCD.isExposing() || streamer->isBusy())
      {
          DEBUG(INDI::Logger::DBG_ERROR, "Can not set format while capturing.");
          CaptureFormatsSP.s = IPS_ALERT;
          IDSetSwitch(&CaptureFormatsSP, NULL);
          return false;
      }
      else
      {
          unsigned int index, oldindex;
          oldindex=IUFindOnSwitchIndex(&CaptureFormatsSP);
          IUResetSwitch(&CaptureFormatsSP);
          IUUpdateSwitch(&CaptureFormatsSP, states, names, n);
          index=IUFindOnSwitchIndex(&CaptureFormatsSP);

         if (v4l_base->setcaptureformat(*((unsigned int *)CaptureFormatsSP.sp[index].aux), errmsg) == -1)
         {
            DEBUGF(INDI::Logger::DBG_SESSION, "ERROR (setformat): %s", errmsg);
            IUResetSwitch(&CaptureFormatsSP);
            CaptureFormatsSP.sp[oldindex].s=ISS_ON;
            CaptureFormatsSP.s = IPS_ALERT;
            IDSetSwitch(&CaptureFormatsSP, NULL);
            return false;
         }

        V4LFrame->bpp = v4l_base->getBpp();
        PrimaryCCD.setBPP(V4LFrame->bpp);

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

#ifdef WITH_V4L2_EXPERIMENTS
      IUSaveText(&CaptureColorSpaceT[0], getColorSpaceName(&v4l_base->fmt));
      IUSaveText(&CaptureColorSpaceT[1], getYCbCrEncodingName(&v4l_base->fmt));
      IUSaveText(&CaptureColorSpaceT[2], getQuantizationName(&v4l_base->fmt));
      IDSetText(&CaptureColorSpaceTP, NULL);     
#endif
      //direct_record=recorder->setpixelformat(v4l_base->fmt.fmt.pix.pixelformat);
      streamer->setPixelFormat(v4l_base->fmt.fmt.pix.pixelformat);
      
      IDSetSwitch(&CaptureFormatsSP, "Capture format: %d. %s", index, CaptureFormatsSP.sp[index].name);
      return true;
    }
  }
  
  /* Capture Size (Discrete) */
  if ((!strcmp(name, CaptureSizesSP.name)))
  {
    //if ((StreamSP.s == IPS_BUSY) ||  (ExposeTimeNP->s == IPS_BUSY) || (RecordStreamSP.s == IPS_BUSY)) {
      if (PrimaryCCD.isExposing() || streamer->isBusy())
      {
          DEBUG(INDI::Logger::DBG_ERROR, "Can not set capture size while capturing.");
          CaptureSizesSP.s = IPS_ALERT;
          IDSetSwitch(&CaptureSizesSP, NULL);
          return false;
      }
     else
      {
        unsigned int index, w, h;
        IUUpdateSwitch(&CaptureSizesSP, states, names, n);
        index=IUFindOnSwitchIndex(&CaptureSizesSP);
        sscanf(CaptureSizesSP.sp[index].name, "%dx%d", &w, &h);
        if (v4l_base->setcapturesize(w, h, errmsg) == -1)
        {
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
      if (ImageColorS[0].s == ISS_ON)
        PrimaryCCD.setFrameBufferSize(w*h*PrimaryCCD.getBPP()/8);
      else
          PrimaryCCD.setFrameBufferSize(w*h*PrimaryCCD.getBPP()/8 * 4);
      
      //recorder->setsize(w, h);
      streamer->setRecorderSize(w,h);
      
      CaptureSizesSP.s = IPS_OK;
      IDSetSwitch(&CaptureSizesSP, "Capture size (discrete): %d. %s", index, CaptureSizesSP.sp[index].name);
      return true;
    }
  }

  /* Frame Rate (Discrete) */
  if ((!strcmp(name, FrameRatesSP.name)))
  {
   if (PrimaryCCD.isExposing() || streamer->isBusy())
   {
        DEBUG(INDI::Logger::DBG_ERROR, "Can not change frame rate while capturing.");
        FrameRatesSP.s = IPS_ALERT;
        IDSetSwitch(&FrameRatesSP, NULL);
        return false;
   }
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
  if (!strcmp(name, ImageColorSP.name))
  {
    if (streamer->isRecording())
    {
      DEBUG(INDI::Logger::DBG_WARNING, "Can not set Image type (GRAY/COLOR) while recording.");
      return false;
    }

    IUResetSwitch(&ImageColorSP);
    IUUpdateSwitch(&ImageColorSP, states, names, n);
    ImageColorSP.s = IPS_OK;
    if (ImageColorS[0].s == ISS_ON) {
      //PrimaryCCD.setBPP(8);
      PrimaryCCD.setNAxis(2);
    } else {
      //PrimaryCCD.setBPP(32);
      //PrimaryCCD.setBPP(8);
      PrimaryCCD.setNAxis(3);
    }
    
    frameBytes  = (ImageColorS[0].s == ISS_ON) ? (PrimaryCCD.getSubW() * PrimaryCCD.getSubH() * (PrimaryCCD.getBPP() / 8)):
      (PrimaryCCD.getSubW() * PrimaryCCD.getSubH() * (PrimaryCCD.getBPP() / 8) * 4);
    PrimaryCCD.setFrameBufferSize(frameBytes);
    
    IDSetSwitch(&ImageColorSP, NULL);
    return true;
  }

  /* Image Depth */
  if (!strcmp(name, ImageDepthSP.name)) {
    if (streamer->isRecording())
    {
      DEBUG(INDI::Logger::DBG_WARNING, "Can not set Image depth (8/16bits) while recording.");
      return false;
    }

    IUResetSwitch(&ImageDepthSP);
    IUUpdateSwitch(&ImageDepthSP, states, names, n);
    ImageDepthSP.s = IPS_OK;
    if (ImageDepthS[0].s == ISS_ON) {
      PrimaryCCD.setBPP(8);
    } else {
      PrimaryCCD.setBPP(16);
    }
    IDSetSwitch(&ImageDepthSP, NULL);
    return true;
  }
  
  /* Stacking Mode */
  if (!strcmp(name, StackModeSP.name)) {
    IUResetSwitch(&StackModeSP);
    IUUpdateSwitch(&StackModeSP, states, names, n);
    StackModeSP.s = IPS_OK;
    stackMode=IUFindOnSwitchIndex(&StackModeSP);
    if (stackMode==STACK_RESET_DARK) {
      if (V4LFrame->darkFrame != NULL) {
	free(V4LFrame->darkFrame);
	V4LFrame->darkFrame = NULL;
      }
    }
    
    IDSetSwitch(&StackModeSP, "Setting Stacking Mode: %s", StackModeS[stackMode].name);
    return true;
  }

  /* V4L2 Options/Menus */
  for (iopt=0; iopt<v4loptions; iopt++) 
    if (!strcmp (Options[iopt].name, name))
      break;
  if (iopt < v4loptions) {
    unsigned int ctrl_id, optindex, ctrlindex;
    
    DEBUGF(INDI::Logger::DBG_DEBUG, "Toggle switch %s=%s", Options[iopt].name, Options[iopt].label);
    
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
    DEBUGF(INDI::Logger::DBG_DEBUG, "  On switch is (%d) %s=\"%s\", ctrl_id = 0x%X ctrl_index=%d", optindex,
	   Options[iopt].sp[optindex].name, Options[iopt].sp[optindex].label, ctrl_id, ctrlindex);
    if (v4l_base->setOPTControl( ctrl_id , ctrlindex,  errmsg) < 0) {
      if (Options[iopt].nsp == 1) { // button
	Options[iopt].sp[optindex].s = ISS_OFF;
      }
      Options[iopt].s = IPS_ALERT;
      IDSetSwitch(&Options[iopt], NULL);
      DEBUGF(INDI::Logger::DBG_ERROR, "Unable to adjust setting. %s", errmsg);
      return false;
    }
    if (Options[iopt].nsp == 1) { // button
      Options[iopt].sp[optindex].s = ISS_OFF;
    }
    Options[iopt].s = IPS_OK;
    IDSetSwitch(&Options[iopt], NULL);
    return true;
  }

  /* ColorProcessing */
  if (!strcmp(name, ColorProcessingSP.name))
  {
    if (ImageColorS[0].s == ISS_ON) {
      IUUpdateSwitch(&ColorProcessingSP, states, names, n);
      v4l_base->setColorProcessing(ColorProcessingS[0].s == ISS_ON, ColorProcessingS[1].s == ISS_ON, ColorProcessingS[2].s == ISS_ON);
      ColorProcessingSP.s = IPS_OK;
      IDSetSwitch(&ColorProcessingSP, NULL);
      V4LFrame->bpp = v4l_base->getBpp();
      PrimaryCCD.setBPP(V4LFrame->bpp);
      PrimaryCCD.setBPP(V4LFrame->bpp);
      frameBytes  = (ImageColorS[0].s == ISS_ON) ? (PrimaryCCD.getSubW() * PrimaryCCD.getSubH() * (PrimaryCCD.getBPP() / 8)):
	(PrimaryCCD.getSubW() * PrimaryCCD.getSubH() * (PrimaryCCD.getBPP() / 8) * 4);
      PrimaryCCD.setFrameBufferSize(frameBytes);
      return true;
    } else {
      DEBUG(INDI::Logger::DBG_WARNING, "No color processing in color mode ");
      return false; 
    }
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
       if (PrimaryCCD.isExposing() || streamer->isBusy())
       {
           DEBUG(INDI::Logger::DBG_ERROR, "Can not set capture size while capturing.");
           CaptureSizesNP.s = IPS_BUSY;
           IDSetNumber(&CaptureSizesNP, NULL);
           return false;
     }
     else
	 {
	   unsigned int index, sizes[2], w, h;
	   double rsizes[2];
	   double fsizes[4];
	   const char *fnames[]={"X", "Y", "WIDTH", "HEIGHT"};
       if (!strcmp(names[0], "Width"))
       {
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
	     } else
	     {
	       w=v4l_base->getWidth();rsizes[1]=(double)w;
	       h=v4l_base->getHeight();rsizes[0]=(double)h;
	     }

	   PrimaryCCD.setFrame(0, 0, w, h);
	   IUUpdateNumber(&CaptureSizesNP, rsizes, names, n);
	   V4LFrame->width = w;
	   V4LFrame->height= h;
	   PrimaryCCD.setResolution(w, h);
	   CaptureSizesNP.s = IPS_OK;
	   frameBytes  = (ImageColorS[0].s == ISS_ON) ? (PrimaryCCD.getSubW() * PrimaryCCD.getSubH() * (PrimaryCCD.getBPP() / 8)):
	     (PrimaryCCD.getSubW() * PrimaryCCD.getSubH() * (PrimaryCCD.getBPP() / 8) * 4);
	   PrimaryCCD.setFrameBufferSize(frameBytes);
     
       streamer->setRecorderSize(w,h);
	   
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
	  
      DEBUGF(INDI::Logger::DBG_DEBUG, "  Setting %s (%s) to %d, ctrl_id = 0x%X", ImageAdjustNP.np[i].name, ImageAdjustNP.np[i].label, (int)ImageAdjustNP.np[i].value, ctrl_id);
	  
	  if (v4l_base->setINTControl( ctrl_id , ImageAdjustNP.np[i].value, errmsg) < 0)
	    {
	      /* Some controls may become read-only depending on selected options */
	      DEBUGF(INDI::Logger::DBG_WARNING,"Unable to adjust %s (ctrl_id =  0x%X)",  ImageAdjustNP.np[i].label, ctrl_id);

	    }
	  /* Some controls may have been ajusted by the driver */
	  /* a read is mandatory as VIDIOC_S_CTRL is write only and does not return the actual new value */ 
	  v4l_base->getControl(ctrl_id, &(ImageAdjustNP.np[i].value), errmsg);
	}      
      ImageAdjustNP.s = IPS_OK;
      IDSetNumber(&ImageAdjustNP, NULL);
      return true;
    }
      
  

  return INDI::CCD::ISNewNumber(dev, name, values, names, n);
  	
}

bool V4L2_Driver::StartExposure(float duration)
{
    if (is_exposing)
    {
        DEBUG(INDI::Logger::DBG_ERROR, "Can not start new exposure while exposing.");
        return false;
    }

    V4LFrame->expose = duration;
    setShutter(V4LFrame->expose);

    PrimaryCCD.setExposureDuration(duration);

     if (!(lx->isenabled()) || (lx->getLxmode() == LXSERIAL ))
        start_capturing();

     is_exposing=true;

     return true;
}

bool V4L2_Driver::setShutter(double duration)
{
  bool rc;
  gettimeofday(&capture_start, NULL);
  if (lx->isenabled()) 
    {
      DEBUGF(INDI::Logger::DBG_SESSION, "Using long exposure mode for %g sec frame.", duration);
      rc=startlongexposure(duration);
      if (rc == false)
	DEBUG(INDI::Logger::DBG_WARNING, "Unable to start long exposure, falling back to auto exposure.");
    }
  else if (AbsExposureN && ManualExposureSP && (AbsExposureN->max >= (duration * 10000)))
    {
      DEBUGF(INDI::Logger::DBG_SESSION, "Using device manual exposure (max %f, required %f).", AbsExposureN->max, (duration * 10000));
      rc = setManualExposure(duration);
      if (rc == false)
	DEBUG(INDI::Logger::DBG_WARNING, "Unable to set manual exposure, falling back to auto exposure.");
    }
  timerclear(&exposure_duration);
  exposure_duration.tv_sec = (long) duration ;
  exposure_duration.tv_usec = (long) ((duration - (double) exposure_duration.tv_sec) * 1000000.0) ;
  frameCount=0;
  subframeCount=0;
}

bool V4L2_Driver::setManualExposure(double duration)
{

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

    ImageAdjustNP.s = IPS_OK;
    IDSetNumber(&ImageAdjustNP, NULL);
  }
  
  return true;
}

void V4L2_Driver::start_capturing() {
  char errmsg[ERRMSGSIZ];
  if (is_capturing) return;
  v4l_base->start_capturing(errmsg);
  is_capturing = true;
  //timer_gettime(fpstimer, &tframe1);  
}

void V4L2_Driver::stop_capturing() {
  char errmsg[ERRMSGSIZ];
  if (!is_capturing) return;
  v4l_base->stop_capturing(errmsg);
  is_capturing = false;
}



bool V4L2_Driver::startlongexposure(double timeinsec)
{
  lxtimer=IEAddTimer((int)(timeinsec*1000.0), (IE_TCF *)lxtimerCallback, this);
  v4l_base->setlxstate( LX_ACCUMULATING );
  return (lx->startLx());
}

void V4L2_Driver::lxtimerCallback(void *userpointer)
{
  struct timespec waittime;
  char errmsg[ERRMSGSIZ];
  V4L2_Driver *p = (V4L2_Driver *)userpointer;
  p->lx->stopLx();
  if (p->lx->getLxmode() == LXSERIAL ) {
    p->v4l_base->setlxstate( LX_TRIGGERED );
  } else {
    p->v4l_base->setlxstate( LX_ACTIVE );
  }
  IERmTimer(p->lxtimer);
  if( !p->v4l_base->isstreamactive() )
	p->start_capturing(); // jump to new/updateFrame
        //p->v4l_base->start_capturing(errmsg); // jump to new/updateFrame
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
    frameBytes  = (ImageColorS[0].s == ISS_ON) ? (PrimaryCCD.getSubW() * PrimaryCCD.getSubH() * (PrimaryCCD.getBPP() / 8)):
      (PrimaryCCD.getSubW() * PrimaryCCD.getSubH() * (PrimaryCCD.getBPP() / 8) * 4);
    PrimaryCCD.setFrameBufferSize(frameBytes);
    //recorder->setsize(w, h);
    streamer->setRecorderSize(w,h);
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
  ((V4L2_Driver *) (p))->newFrame();
}

void V4L2_Driver::stackFrame()
{
  struct timeval current_exposure;

  if (!V4LFrame->stackedFrame) {
    float *src, *dest;
    unsigned int i;
    
    V4LFrame->stackedFrame = (float *)malloc(sizeof(float) * v4l_base->getWidth() * v4l_base->getHeight());
    src=v4l_base->getLinearY();
    dest=V4LFrame->stackedFrame;
    for (i=0; i < v4l_base->getWidth() * v4l_base->getHeight(); i++)
      *dest++ = *src++;
    subframeCount=1;
  } else {
    float *src, *dest;
    unsigned int i;
    
    src=v4l_base->getLinearY();
    dest=V4LFrame->stackedFrame;
    for (i=0; i < v4l_base->getWidth() * v4l_base->getHeight(); i++)
      *dest++ += *src++;
    subframeCount+=1;    
  }

}

void V4L2_Driver::newFrame()
{
    if (streamer->isBusy())
    {
        int width  = v4l_base->getWidth();
        int height = v4l_base->getHeight();
        int bpp = v4l_base->getBpp();
        int dbpp=8;

        if (ImageColorS[0].s == ISS_ON)
           V4LFrame->Y      		= v4l_base->getY();
        else
           V4LFrame->colorBuffer 	= v4l_base->getColorBuffer();

        int totalBytes  = ImageColorS[0].s == ISS_ON ? width * height * (dbpp / 8) : width * height * (dbpp / 8) * 4;
        unsigned char *buffer = ImageColorS[0].s == ISS_ON ? V4LFrame->Y : V4LFrame->colorBuffer;

        // downscale Y10 Y12 Y16
        if (bpp > dbpp)
        {
          unsigned int i;
          unsigned short *src=(unsigned short *)buffer;
          unsigned char *dest=buffer;
          unsigned char shift=0;
          if (bpp < 16)
          {
            switch (bpp)
            {
            case 10: shift=2; break;
            case 12: shift=4; break;
            }
            for (i = 0; i < totalBytes; i++)
            {
                 *dest++ = *(src++) >> shift;
            }
          }
          else
          {
            unsigned char *src=(unsigned char *)buffer + 1; // Y16 is little endian
            for (i = 0; i < totalBytes; i++)
            {
                 *dest++ = *src; src+=2;
            }
          }
        }

        if (ImageColorS[0].s == ISS_ON)
            streamer->newFrame(buffer, StreamRecorder::IMAGE_MONO_8);
        else
            streamer->newFrame(buffer, StreamRecorder::IMAGE_RGB_32);
    }

  if (PrimaryCCD.isExposing())
  {
    unsigned int i;
    struct timeval current_exposure;
    // Stack Mono frames
    if ((stackMode) && !(lx->isenabled()) && !(ImageColorS[1].s == ISS_ON))
    {
      stackFrame();
    }

    gettimeofday(&capture_end,NULL);
    timersub(&capture_end, &capture_start, &current_exposure);    

    if ((stackMode) && !(lx->isenabled()) && !(ImageColorS[1].s == ISS_ON) && (timercmp(&current_exposure, &exposure_duration, <)))
      return; // go on stacking

    //IDLog("Copying frame.\n");
     if (ImageColorS[0].s == ISS_ON)
     {
       if (!stackMode)
       {
            unsigned char *src, *dest;
            src = v4l_base->getY();
            dest = (unsigned char *)PrimaryCCD.getFrameBuffer();
            for (i=0; i< frameBytes; i++)
                *(dest++) = *(src++);

            binFrame();
       }
       else
       {
            float *src=V4LFrame->stackedFrame;
            if ((stackMode != STACK_TAKE_DARK) && (V4LFrame->darkFrame != NULL))
            {
                float *dark=V4LFrame->darkFrame;
                for (i=0; i < v4l_base->getWidth() * v4l_base->getHeight(); i++)
                {
                    if (*src > *dark) *src -= *dark;
                    else *src = 0.0;
                    src++; dark++;
                }

                src=V4LFrame->stackedFrame;
            }
	 //IDLog("Copying stack frame from %p to %p.\n", src, dest);
            if (stackMode == STACK_MEAN)
            {
                if (ImageDepthS[0].s == ISS_ON)
                { // depth 8 bits
                    unsigned char *dest=(unsigned char *)PrimaryCCD.getFrameBuffer();
                    for (i=0; i < v4l_base->getWidth() * v4l_base->getHeight(); i++)
                        *dest++ = (unsigned char) ((*src++ * 255) / subframeCount );
                }
                else
                {                          // depth 16 bits
                    unsigned short *dest=(unsigned short *)PrimaryCCD.getFrameBuffer();
                    for (i=0; i < v4l_base->getWidth() * v4l_base->getHeight(); i++)
                        *dest++ = (unsigned short) ((*src++ * 65535) / subframeCount );
                }

                free(V4LFrame->stackedFrame);
                V4LFrame->stackedFrame=NULL;
            }
            else if (stackMode == STACK_ADDITIVE)
            {
                if (ImageDepthS[0].s == ISS_ON)
                { // depth 8 bits
                    unsigned char *dest=(unsigned char *)PrimaryCCD.getFrameBuffer();
                    for (i=0; i < v4l_base->getWidth() * v4l_base->getHeight(); i++)
                        *dest++ = (unsigned char) ((*src++ * 255));
                }
                else
                {                          // depth 16 bits
                    unsigned short *dest=(unsigned short *)PrimaryCCD.getFrameBuffer();
                    for (i=0; i < v4l_base->getWidth() * v4l_base->getHeight(); i++)
                        *dest++ = (unsigned short) ((*src++ * 65535));
                }

                free(V4LFrame->stackedFrame);
                V4LFrame->stackedFrame=NULL;
            }
            else if (stackMode == STACK_TAKE_DARK)
            {
                if (V4LFrame->darkFrame != NULL)
                    free(V4LFrame->darkFrame);
                V4LFrame->darkFrame = V4LFrame->stackedFrame;
                V4LFrame->stackedFrame=NULL;
                src=V4LFrame->darkFrame;
                if (ImageDepthS[0].s == ISS_ON)
                { // depth 8 bits
                    unsigned char *dest=(unsigned char *)PrimaryCCD.getFrameBuffer();
                    for (i=0; i < v4l_base->getWidth() * v4l_base->getHeight(); i++)
                    *dest++ = (unsigned char) ((*src++ * 255));
                }
                else
                {                          // depth 16 bits
                    unsigned short *dest=(unsigned short *)PrimaryCCD.getFrameBuffer();
                    for (i=0; i < v4l_base->getWidth() * v4l_base->getHeight(); i++)
                    *dest++ = (unsigned short) ((*src++ * 65535));
                }
            }
       }
     }
     else
     {
       unsigned char *src, *dest;
      // Binning not supported in color images for now
      src = v4l_base->getColorBuffer();
      dest = (unsigned char *)PrimaryCCD.getFrameBuffer();
      unsigned char *red = dest;
      unsigned char *green = dest + v4l_base->getWidth() * v4l_base->getHeight() * (v4l_base->getBpp() / 8);
      unsigned char *blue = dest + v4l_base->getWidth() * v4l_base->getHeight() * (v4l_base->getBpp() / 8) * 2;
      
      for (int i=0; i <  frameBytes; i+=4)
      {
          *(blue++) = *(src+i);
          *(green++) = *(src+i+1);
          *(red++) = *(src+i+2);
      }
    }
     //IDLog("Copy frame finished.\n");
    frameCount+=1;
    
    if (lx->isenabled())
    {
      //if (!is_streaming && !is_recording)
        if (streamer->isBusy() == false)
          stop_capturing();

        DEBUGF(INDI::Logger::DBG_SESSION, "Capture of LX frame took %ld.%06ld seconds.", current_exposure.tv_sec, current_exposure.tv_usec);
        ExposureComplete(&PrimaryCCD);
        //PrimaryCCD.setFrameBufferSize(frameBytes);
	//}
    }
    else
    {
      //if (!is_streaming && !is_recording) stop_capturing();
      if (streamer->isBusy() == false)
          stop_capturing();

      DEBUGF(INDI::Logger::DBG_SESSION, "Capture of one frame (%d stacked frames) took %ld.%06ld seconds.", subframeCount, current_exposure.tv_sec, current_exposure.tv_usec);
      ExposureComplete(&PrimaryCCD);
      //PrimaryCCD.setFrameBufferSize(frameBytes);
    }
    is_exposing=false;
  }
}

bool V4L2_Driver::AbortExposure()
{
  char errmsg[ERRMSGSIZ];
  if (lx->isenabled())
    lx->stopLx();
  else
    //if (!is_streaming && !is_recording)
    if (streamer->isBusy() == false)
        stop_capturing();
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
      DEBUG(INDI::Logger::DBG_SESSION,"To use LED Long exposure mode with recent kernels, see https://code.google.com/p/pwc-lxled/");

  }
  
  return true;
}

bool V4L2_Driver::Disconnect()
{
  if (isConnected()) {
    v4l_base->disconnectCam(PrimaryCCD.isExposing() || streamer->isBusy());
    if (PrimaryCCD.isExposing() || streamer->isBusy())
      streamer->close();
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
  V4LFrame->bpp=v4l_base->getBpp();

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
#ifdef WITH_V4L2_EXPERIMENTS
  IUSaveText(&CaptureColorSpaceT[0], getColorSpaceName(&v4l_base->fmt));
  IUSaveText(&CaptureColorSpaceT[1], getYCbCrEncodingName(&v4l_base->fmt));
  IUSaveText(&CaptureColorSpaceT[2], getQuantizationName(&v4l_base->fmt));
  IDSetText(&CaptureColorSpaceTP, NULL);
#endif
  if (Options) free(Options);
  Options=NULL;
  v4loptions=0;
  updateV4L2Controls();

  PrimaryCCD.setResolution(w, h);
  PrimaryCCD.setFrame(0,0, w,h);
  PrimaryCCD.setBPP(V4LFrame->bpp);
  frameBytes  = (ImageColorS[0].s == ISS_ON) ? (PrimaryCCD.getSubW() * PrimaryCCD.getSubH() * (PrimaryCCD.getBPP() / 8)):
                                              (PrimaryCCD.getSubW() * PrimaryCCD.getSubH() * (PrimaryCCD.getBPP() / 8) * 4);
  PrimaryCCD.setFrameBufferSize(frameBytes);

  //direct_record=recorder->setpixelformat(v4l_base->fmt.fmt.pix.pixelformat);
  //recorder->setsize(w, h);
  streamer->setPixelFormat(v4l_base->fmt.fmt.pix.pixelformat);
  streamer->setRecorderSize(w,h);
     
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
     //V4LFrame->compressedFrame  = (unsigned char *) malloc (sizeof(unsigned char) * 1);
     V4LFrame->stackedFrame  = NULL;
     V4LFrame->darkFrame  = NULL;
}

void V4L2_Driver::releaseBuffers()
{
  free(fitsData);
  free(V4LFrame->Y);
  free(V4LFrame->U);
  free(V4LFrame->V);
  free(V4LFrame->colorBuffer);
  //free(V4LFrame->compressedFrame);
  free (V4LFrame);
}

bool V4L2_Driver::StartStreaming()
{
    if (!is_capturing)
    {
        start_capturing();
        //v4l_base->setDropFrameCount(streamer->getFramesToDrop());
        v4l_base->doRecord(streamer->isDirectRecording());
        return true;
    }

    return false;
}

bool V4L2_Driver::StopStreaming()
{
    if (is_exposing)
        return false;

    if (streamer->isDirectRecording())
        v4l_base->doRecord(false);

    stop_capturing();
    return true;
}

