/*
    Phlips webcam INDI driver
    Copyright (C) 2003-2005 by Jasem Mutlaq

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

    2005.04.29  JM: There is no need for this file for Video 4 Linux 2. It is kept for V4L 1 compatibility.

*/

#include "v4lphilips.h"
#include "webcam/pwc-ioctl.h"

V4L_Philips::V4L_Philips() : V4L_Driver()
{

}

V4L_Philips::~V4L_Philips()
{

}

void V4L_Philips::initCamBase()
{
   #ifdef HAVE_LINUX_VIDEODEV2_H
    v4l_base = new V4L2_Base();
   #else
    v4l_pwc = new V4L1_PWC();
    v4l_base = (V4L1_Base *) v4l_pwc;
   #endif
}

void V4L_Philips::initProperties(const char *dev)
{

  // Call parent
  V4L_Driver::initProperties(dev);

  IUFillSwitch(&BackLightS[0], "ON", "", ISS_OFF);
  IUFillSwitch(&BackLightS[1], "OFF", "", ISS_ON);
  IUFillSwitchVector(&BackLightSP, BackLightS, NARRAY(BackLightS), dev, "Back Light", "", IMAGE_CONTROL, IP_RW, ISR_1OFMANY, 0 , IPS_IDLE);

  IUFillSwitch(&AntiFlickerS[0], "ON", "", ISS_OFF);
  IUFillSwitch(&AntiFlickerS[1], "OFF", "", ISS_ON);
  IUFillSwitchVector(&AntiFlickerSP, AntiFlickerS, NARRAY(AntiFlickerS), dev, "Anti Flicker", "", IMAGE_CONTROL, IP_RW, ISR_1OFMANY, 0 , IPS_IDLE);

  IUFillSwitch(&NoiseReductionS[0], "None", "", ISS_ON);
  IUFillSwitch(&NoiseReductionS[1], "Low", "", ISS_OFF);
  IUFillSwitch(&NoiseReductionS[2], "Medium", "", ISS_OFF);
  IUFillSwitch(&NoiseReductionS[3], "High", "", ISS_OFF);
  IUFillSwitchVector(&NoiseReductionSP, NoiseReductionS, NARRAY(NoiseReductionS), dev, "Noise Reduction", "", IMAGE_CONTROL, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

  IUFillSwitch(&CamSettingS[0], "Save", "", ISS_OFF);
  IUFillSwitch(&CamSettingS[1], "Restore", "", ISS_OFF);
  IUFillSwitch(&CamSettingS[2], "Factory", "", ISS_OFF);
  IUFillSwitchVector(&CamSettingSP, CamSettingS, NARRAY(CamSettingS), dev, "Settings", "", IMAGE_CONTROL, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

  IUFillSwitch(&WhiteBalanceModeS[0], "Auto" , "", ISS_ON);
  IUFillSwitch(&WhiteBalanceModeS[1], "Manual" , "", ISS_OFF);
  IUFillSwitch(&WhiteBalanceModeS[2], "Indoor" , "", ISS_OFF);
  IUFillSwitch(&WhiteBalanceModeS[3], "Outdoor" , "", ISS_OFF);
  IUFillSwitch(&WhiteBalanceModeS[4], "Fluorescent" , "", ISS_OFF);
  
  IUFillSwitchVector(&WhiteBalanceModeSP, WhiteBalanceModeS, NARRAY(WhiteBalanceModeS), dev, "White Balance Mode", "", IMAGE_CONTROL, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

  IUFillNumber(&WhiteBalanceN[0], "Manual Red", "", "%0.f", 0., 256., 1., 0.);
  IUFillNumber(&WhiteBalanceN[1], "Manual Blue", "", "%0.f", 0., 256., 1., 0.);
  IUFillNumberVector(&WhiteBalanceNP, WhiteBalanceN, NARRAY(WhiteBalanceN), dev, "White Balance", "", IMAGE_CONTROL, IP_RW, 60, IPS_IDLE);

  IUFillNumber(&ShutterSpeedN[0], "Speed", "", "%0.f", 0., 65535., 100., 0.);
  IUFillNumberVector(&ShutterSpeedNP, ShutterSpeedN, NARRAY(ShutterSpeedN), dev, "Shutter Speed", "", COMM_GROUP, IP_RW, 60, IPS_IDLE);

}

void V4L_Philips::ISGetProperties (const char *dev)
{

  if (dev && strcmp (device_name, dev))
    return;
    
   #ifdef HAVE_LINUX_VIDEODEV2_H
    V4L_Driver::ISGetProperties(dev);
    return;
   #endif

  /* COMM_GROUP */
  IDDefSwitch(&PowerSP, NULL);
  IDDefText(&PortTP, NULL);
  IDDefText(&camNameTP, NULL);
  IDDefSwitch(&StreamSP, NULL);
  IDDefNumber(&FrameRateNP, NULL);
  IDDefNumber(&ExposeTimeNP, NULL);
  IDDefNumber(&ShutterSpeedNP, NULL);
  IDDefBLOB(&imageBP, NULL);
  
  /* Image Groups */
  IDDefSwitch(&CompressSP, NULL);
  IDDefSwitch(&ImageTypeSP, NULL);
  IDDefNumber(&FrameNP, NULL);
  IDDefNumber(&ImageAdjustNP, NULL);
  
  /* Image Control */
  IDDefSwitch(&WhiteBalanceModeSP, NULL);
  IDDefNumber(&WhiteBalanceNP, NULL);
  IDDefSwitch(&BackLightSP, NULL);
  IDDefSwitch(&AntiFlickerSP, NULL);
  IDDefSwitch(&NoiseReductionSP, NULL);
  IDDefSwitch(&CamSettingSP, NULL);

}

void V4L_Philips::ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n)
{
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
    

     #ifndef HAVE_LINUX_VIDEODEV2_H
    /* Anti Flicker control */
    if (!strcmp (AntiFlickerSP.name, name))
    {
       if (checkPowerS(&AntiFlickerSP))
         return;
	 
       AntiFlickerSP.s = IPS_IDLE;
       
       IUResetSwitch(&AntiFlickerSP);
       IUUpdateSwitch(&AntiFlickerSP, states, names, n);
       
       if (AntiFlickerS[0].s == ISS_ON)
       {
         if (v4l_pwc->setFlicker(true, errmsg) < 0)
	 {
	   AntiFlickerS[0].s = ISS_OFF;
	   AntiFlickerS[1].s = ISS_ON;
	   IDSetSwitch(&AntiFlickerSP, "%s", errmsg);
	   return;
	 }
	 
	 AntiFlickerSP.s = IPS_OK;
	 IDSetSwitch(&AntiFlickerSP, NULL);
       }
       else
       {
         if (v4l_pwc->setFlicker(false, errmsg) < 0)
	 {
	   AntiFlickerS[0].s = ISS_ON;
	   AntiFlickerS[1].s = ISS_OFF;
	   IDSetSwitch(&AntiFlickerSP, "%s", errmsg);
	   return;
	 }
	 
	 IDSetSwitch(&AntiFlickerSP, NULL);
       }
       
       return;
    }
    
    /* Back light compensation */
    if (!strcmp (BackLightSP.name, name))
    {
       if (checkPowerS(&BackLightSP))
         return;
	 
       BackLightSP.s = IPS_IDLE;
       
       IUResetSwitch(&BackLightSP);
       IUUpdateSwitch(&BackLightSP, states, names, n);
       
       if (BackLightS[0].s == ISS_ON)
       {
         if (v4l_pwc->setBackLight(true, errmsg) < 0)
	 {
	   BackLightS[0].s = ISS_OFF;
	   BackLightS[1].s = ISS_ON;
	   IDSetSwitch(&BackLightSP, "%s", errmsg);
	   return;
	 }
	 
	 BackLightSP.s = IPS_OK;
	 IDSetSwitch(&BackLightSP, NULL);
       }
       else
       {
         if (v4l_pwc->setBackLight(false, errmsg) < 0)
	 {
	   BackLightS[0].s = ISS_ON;
	   BackLightS[1].s = ISS_OFF;
	   IDSetSwitch(&BackLightSP, "%s", errmsg);
	   return;
	 }
	 
	 IDSetSwitch(&BackLightSP, NULL);
       }
       
       return;
    }
	 
    /* Noise reduction control */
    if (!strcmp (NoiseReductionSP.name, name))
    {
       if (checkPowerS(&NoiseReductionSP))
         return;
	 
       NoiseReductionSP.s = IPS_IDLE;
       
       IUResetSwitch(&NoiseReductionSP);
       IUUpdateSwitch(&NoiseReductionSP, states, names, n);
       
       for (int i=0; i < 4; i++)
        if (NoiseReductionS[i].s == ISS_ON)
	{
	   index = i;
	   break;
	}
	
       if (v4l_pwc->setNoiseRemoval(index, errmsg) < 0)
       {
         IUResetSwitch(&NoiseReductionSP);
	 NoiseReductionS[0].s = ISS_ON;
	 IDSetSwitch(&NoiseReductionSP, "%s", errmsg);
	 return;
       }
       
       NoiseReductionSP.s = IPS_OK;
       
       IDSetSwitch(&NoiseReductionSP, NULL);
       return;
    }
    
    /* White balace mode */
    if (!strcmp (WhiteBalanceModeSP.name, name))
    {
       if (checkPowerS(&WhiteBalanceModeSP))
         return;
	 
       WhiteBalanceModeSP.s = IPS_IDLE;
       
       IUResetSwitch(&WhiteBalanceModeSP);
       IUUpdateSwitch(&WhiteBalanceModeSP, states, names, n);
       
       for (int i=0; i < 5; i++)
        if (WhiteBalanceModeS[i].s == ISS_ON)
	{
	   index = i;
	   break;
	}
	
	switch (index)
	{
	  // Auto
	  case 0:
	   if (v4l_pwc->setWhiteBalanceMode(PWC_WB_AUTO, errmsg) < 0)
	   {
	     IUResetSwitch(&WhiteBalanceModeSP),
	     WhiteBalanceModeS[0].s = ISS_ON;
	     IDSetSwitch(&WhiteBalanceModeSP, "%s", errmsg);
	     return;
	   }
	   break;
	   
	 // Manual
	 case 1:
	  if (v4l_pwc->setWhiteBalanceMode(PWC_WB_MANUAL, errmsg) < 0)
	   {
	     IUResetSwitch(&WhiteBalanceModeSP),
	     WhiteBalanceModeS[0].s = ISS_ON;
	     IDSetSwitch(&WhiteBalanceModeSP, "%s", errmsg);
	     return;
	   }
	   break;
	   
	 // Indoor
	 case 2:
	  if (v4l_pwc->setWhiteBalanceMode(PWC_WB_INDOOR, errmsg) < 0)
	   {
	     IUResetSwitch(&WhiteBalanceModeSP),
	     WhiteBalanceModeS[0].s = ISS_ON;
	     IDSetSwitch(&WhiteBalanceModeSP, "%s", errmsg);
	     return;
	   }
	   break;
	   
	 // Outdoor
	 case 3:
	  if (v4l_pwc->setWhiteBalanceMode(PWC_WB_OUTDOOR, errmsg) < 0)
	   {
	     IUResetSwitch(&WhiteBalanceModeSP),
	     WhiteBalanceModeS[0].s = ISS_ON;
	     IDSetSwitch(&WhiteBalanceModeSP, "%s", errmsg);
	     return;
	   }
	   break;
	   
	 // Flurescent
	 case 4:
	  if (v4l_pwc->setWhiteBalanceMode(PWC_WB_FL, errmsg) < 0)
	   {
	     IUResetSwitch(&WhiteBalanceModeSP),
	     WhiteBalanceModeS[0].s = ISS_ON;
	     IDSetSwitch(&WhiteBalanceModeSP, "%s", errmsg);
	     return;
	   }
	   break;
	   
	}
	     
	WhiteBalanceModeSP.s = IPS_OK;
	IDSetSwitch(&WhiteBalanceModeSP, NULL);
	return;
	
     }
	
    /* Camera setttings */
    if (!strcmp (CamSettingSP.name, name))
    {
       
       if (checkPowerS(&CamSettingSP))
         return;
    
	CamSettingSP.s = IPS_IDLE;
	
	IUResetSwitch(&CamSettingSP);
	IUUpdateSwitch(&CamSettingSP, states, names, n);
	
	if (CamSettingS[0].s == ISS_ON)
	{
	  if (v4l_pwc->saveSettings(errmsg) < 0)
	  {
	    IUResetSwitch(&CamSettingSP);
	    IDSetSwitch(&CamSettingSP, "%s", errmsg);
	    return;
	  }
	  
	  CamSettingSP.s = IPS_OK;
	  IDSetSwitch(&CamSettingSP, "Settings saved.");
	  return;
	}
	
	if (CamSettingS[1].s == ISS_ON)
	{
	   v4l_pwc->restoreSettings();
	   IUResetSwitch(&CamSettingSP);
	   CamSettingSP.s = IPS_OK;
	   IDSetSwitch(&CamSettingSP, "Settings restored.");
           updateV4L1Controls();
	   return;
	}
	
	if (CamSettingS[2].s == ISS_ON)
	{
	  v4l_pwc->restoreFactorySettings();
	  IUResetSwitch(&CamSettingSP);
	  CamSettingSP.s = IPS_OK;
	  IDSetSwitch(&CamSettingSP, "Factory settings restored.");
          updateV4L1Controls();
	  return;
	}
     }
     #endif

     // Call parent
     V4L_Driver::ISNewSwitch(dev, name, states, names, n);
        
     

}

void V4L_Philips::ISNewText (const char *dev, const char *name, char *texts[], char *names[], int n)
{
   
     V4L_Driver::ISNewText(dev, name, texts, names, n);

}

void V4L_Philips::ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n)
{

    // Nothing for V4L 2 to do here
    #ifndef HAVE_LINUX_VIDEODEV2_H
    char errmsg[ERRMSGSIZ];

    /* Frame rate */
   if (!strcmp (FrameRateNP.name, name))
   {
     if (checkPowerN(&FrameRateNP))
      return;
      
     FrameRateNP.s = IPS_IDLE;
     
     int oldFP = (int) FrameRateN[0].value; 
     
     if (IUUpdateNumber(&FrameRateNP, values, names, n) < 0)
       return;
       
     if (v4l_pwc->setFrameRate( (int) FrameRateN[0].value, errmsg) < 0)
     {
       FrameRateN[0].value = oldFP;
       IDSetNumber(&FrameRateNP, "%s", errmsg);
       return;
     }
       
     FrameRateNP.s = IPS_OK;
     IDSetNumber(&FrameRateNP, NULL);
     return;
   }
  
   if (!strcmp (ShutterSpeedNP.name, name))
   {
     if (checkPowerN(&ShutterSpeedNP))
       return;
       
     ShutterSpeedNP.s = IPS_IDLE;
     
     if (v4l_pwc->setExposure( (int) values[0], errmsg) < 0)
     {
       IDSetNumber(&ShutterSpeedNP, "%s", errmsg);
       return;
     }
     
     ShutterSpeedN[0].value = values[0];
     ShutterSpeedNP.s = IPS_OK;
     IDSetNumber(&ShutterSpeedNP, NULL);
     return;
  }
  
   /* White balance */
   if (!strcmp (WhiteBalanceNP.name, name))
   {
     if (checkPowerN(&WhiteBalanceNP))
       return;
       
     WhiteBalanceNP.s = IPS_IDLE;
     
     int oldBalance[2];
     oldBalance[0] = (int) WhiteBalanceN[0].value;
     oldBalance[1] = (int) WhiteBalanceN[1].value;
     
     if (IUUpdateNumber(&WhiteBalanceNP, values, names, n) < 0)
       return;
     
     if (v4l_pwc->setWhiteBalanceRed( (int) WhiteBalanceN[0].value * 256, errmsg))
     {
       WhiteBalanceN[0].value = oldBalance[0];
       WhiteBalanceN[1].value = oldBalance[1];
       IDSetNumber(&WhiteBalanceNP, "%s", errmsg);
       return;
     }
     if (v4l_pwc->setWhiteBalanceBlue( (int) WhiteBalanceN[1].value * 256, errmsg))
     {
       WhiteBalanceN[0].value = oldBalance[0];
       WhiteBalanceN[1].value = oldBalance[1];
       IDSetNumber(&WhiteBalanceNP, "%s", errmsg);
       return;
     }
     
     IUResetSwitch(&WhiteBalanceModeSP);
     WhiteBalanceModeS[1].s = ISS_ON;
     WhiteBalanceModeSP.s   = IPS_OK;
     WhiteBalanceNP.s = IPS_OK;
     IDSetSwitch(&WhiteBalanceModeSP, NULL);
     IDSetNumber(&WhiteBalanceNP, NULL);
     return;
   }

   #endif

   // Call parent 
   V4L_Driver::ISNewNumber(dev, name, values, names, n);

}

void V4L_Philips::connectCamera()
{
  char errmsg[ERRMSGSIZ];
  
    
  switch (PowerS[0].s)
  {
     case ISS_ON:
      #ifdef HAVE_LINUX_VIDEODEV2_H
      if (v4l_base->connectCam(PortT[0].text, errmsg, V4L2_PIX_FMT_YUV420) < 0)
      #else
      if (v4l_base->connectCam(PortT[0].text, errmsg) < 0)
      #endif
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
      IDSetSwitch(&PowerSP, "Philips Webcam is online. Retrieving basic data.");

      v4l_base->registerCallback(newFrame, this);
      
      IDLog("Philips Webcam is online. Retrieving basic data.\n");
      getBasicData();
      
      break;
      
    case ISS_OFF:
      PowerS[0].s = ISS_OFF;
      PowerS[1].s = ISS_ON;
      PowerSP.s = IPS_IDLE;
      
      v4l_base->disconnectCam();
      
      IDSetSwitch(&PowerSP, "Philips Webcam is offline.");
      
      break;
     }
}

#ifndef HAVE_LINUX_VIDEODEV2_H
/* Retrieves basic data from the device upon connection.*/
void V4L_Philips::getBasicData()
{

  char errmsg[ERRMSGSIZ];
  bool result;
  int xmax, ymax, xmin, ymin, index;
  
  v4l_pwc->getMaxMinSize(xmax, ymax, xmin, ymin);
  
  IDLog("X (%d,%d), Y (%d,%d)\n", xmin, xmax, ymin, ymax);

  /* Width */
  FrameN[2].value = v4l_pwc->getWidth();
  FrameN[2].min = xmin;
  FrameN[2].max = xmax;
  
  /* Height */
  FrameN[3].value = v4l_pwc->getHeight();
  FrameN[3].min = ymin;
  FrameN[3].max = ymax;

  IDSetNumber(&FrameNP, NULL);
  IUUpdateMinMax(&FrameNP);
  
  IUSaveText(&camNameT[0], v4l_pwc->getgetDeviceName());
  IDSetText(&camNameTP, NULL);
  
  IDLog("Raw values\n Contrast: %d \n Brightness %d \n Color %d \n Sharpness %d \n Gain %d \n Gamma %d \n", v4l_pwc->getContrast(), v4l_pwc->getBrightness(), v4l_pwc->getColor(), v4l_pwc->getSharpness(), v4l_pwc->getGain(), v4l_pwc->getGama());
  
  updateV4L1Controls();
  
  if (v4l_pwc->setFrameRate( (int) FrameRateN[0].value, errmsg) < 0)
  {
    FrameRateNP.s = IPS_ALERT;
    IDSetNumber(&FrameRateNP, "%s", errmsg);
  }
  else
  {
    FrameRateNP.s = IPS_OK;
    IDSetNumber(&FrameRateNP, NULL);
  }
  
  result = v4l_pwc->getBackLight();
  if (result)
  {
   BackLightS[0].s = ISS_ON;
   BackLightS[1].s = ISS_OFF;
  }
  else
  {
   BackLightS[0].s = ISS_OFF;
   BackLightS[1].s = ISS_ON;
  }
  IDSetSwitch(&BackLightSP, NULL);
  
  result = v4l_pwc->getFlicker();
  if (result)
  {
    AntiFlickerS[0].s = ISS_ON;
    AntiFlickerS[1].s = ISS_OFF;
  }
  else
  {
    AntiFlickerS[0].s = ISS_OFF;
    AntiFlickerS[1].s = ISS_ON;
  }
  IDSetSwitch(&AntiFlickerSP, NULL);
  
  index = v4l_pwc->getNoiseRemoval();
  IUResetSwitch(&NoiseReductionSP);
  NoiseReductionS[index].s = ISS_ON;
  IDSetSwitch(&NoiseReductionSP, NULL);
  
  index = v4l_pwc->getWhiteBalance();
  IUResetSwitch(&WhiteBalanceModeSP);
  switch (index)
  {
    case PWC_WB_AUTO:
     WhiteBalanceModeS[0].s = ISS_ON;
     break;
    case PWC_WB_MANUAL:
     WhiteBalanceModeS[1].s = ISS_ON;
     break;
    case PWC_WB_INDOOR:
     WhiteBalanceModeS[2].s = ISS_ON;
     break;
    case PWC_WB_OUTDOOR:
     WhiteBalanceModeS[3].s = ISS_ON;
     break;
    case PWC_WB_FL:
     WhiteBalanceModeS[3].s = ISS_ON;
     break;
  }
  IDSetSwitch(&WhiteBalanceModeSP, NULL);    
  
}
#endif

#ifndef HAVE_LINUX_VIDEODEV2_H
void V4L_Philips::updateV4L1Controls()
{
  int index =0;

  ImageAdjustN[0].value = v4l_pwc->getContrast() / 256.;
  ImageAdjustN[1].value = v4l_pwc->getBrightness() / 256.;
  ImageAdjustN[2].value = v4l_pwc->getColor() / 256.;
  index = v4l_pwc->getSharpness();
  if (index < 0)
  	ImageAdjustN[3].value = -1;
  else
    ImageAdjustN[3].value = v4l_pwc->getSharpness() / 256.;
    
  ImageAdjustN[4].value = v4l_pwc->getGain() / 256.;
  ImageAdjustN[5].value = v4l_pwc->getGama() / 256.;
       
  ImageAdjustNP.s = IPS_OK;
  IDSetNumber(&ImageAdjustNP, NULL);


}
#endif

