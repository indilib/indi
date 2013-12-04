#include "Lx.h"
#include <indicom.h>

void Lx::setCamerafd(int fd) {
  camerafd=fd;
}

bool Lx::isenabled() {
  return (LxEnableS[1].s==ISS_ON);
}

bool Lx::initProperties(INDI::DefaultDevice *device) {

  //IDLog("Initializing Long Exposure Properties\n");
  dev=device;
  device_name=dev->getDeviceName();
  IUFillSwitch(&LxEnableS[0], "Disable", "", ISS_ON);
  IUFillSwitch(&LxEnableS[1], "Enable", "", ISS_OFF);
  IUFillSwitchVector(&LxEnableSP, LxEnableS, NARRAY(LxEnableS), device_name, "Activate", "", LX_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);
  IUFillSwitch(&LxModeS[0], "Serial", "", ISS_ON);
  IUFillSwitch(&LxModeS[1], "Parallel", "", ISS_OFF);
  IUFillSwitch(&LxModeS[2], "SPC900 LED", "", ISS_OFF);
  IUFillSwitch(&LxModeS[3], "GPIO (Arm/RPI)", "", ISS_OFF);
  //  IUFillSwitch(&LxModeS[4], "IndiDuino Switcher", "", ISS_OFF); // Snooping is not enough
  IUFillSwitchVector(&LxModeSP, LxModeS, NARRAY(LxModeS), device_name, "LX Mode", "", LX_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);
  IUFillText(&LxPortT[0], "Port", "", "/dev/ttyS0");
  IUFillTextVector(&LxPortTP, LxPortT, NARRAY(LxPortT), device_name, "Lx port", "", LX_TAB, IP_RW, 0, IPS_IDLE);
  IUFillSwitch(&LxSerialOptionS[0], "Use RTS (pin 7)", "", ISS_ON);
  IUFillSwitch(&LxSerialOptionS[1], "Use DTR (pin 4)", "", ISS_OFF);
  IUFillSwitch(&LxSerialOptionS[2], "Use Serial command", "", ISS_OFF);
  IUFillSwitchVector(&LxSerialOptionSP, LxSerialOptionS, NARRAY(LxSerialOptionS), device_name, "Serial Options", "", LX_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);
  IUFillSwitch(&LxParallelOptionS[0], "Use Data 0 (pin 2)", "", ISS_OFF);
  IUFillSwitch(&LxParallelOptionS[1], "Use Data 1 (pin 3)", "", ISS_ON); // Steve's Chambers Schematics
  IUFillSwitch(&LxParallelOptionS[2], "Use Data 2 (pin 4)", "", ISS_OFF);
  IUFillSwitch(&LxParallelOptionS[3], "Use Data 3 (pin 5)", "", ISS_OFF);
  IUFillSwitch(&LxParallelOptionS[4], "Use Data 4 (pin 6)", "", ISS_OFF);
  IUFillSwitch(&LxParallelOptionS[5], "Use Data 5 (pin 7)", "", ISS_OFF);
  IUFillSwitch(&LxParallelOptionS[6], "Use Data 6 (pin 8)", "", ISS_OFF);
  IUFillSwitch(&LxParallelOptionS[7], "Use Data 7 (pin 9)", "", ISS_OFF);
  IUFillSwitch(&LxParallelOptionS[8], "Use Parallel command", "", ISS_OFF);
  IUFillSwitchVector(&LxParallelOptionSP, LxParallelOptionS, NARRAY(LxParallelOptionS), device_name, "Parallel Options", "", LX_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);
  IUFillText(&LxStartStopCmdT[0], "Start command", "", ":01");
  IUFillText(&LxStartStopCmdT[1], "Stop command", "", ":01");
  IUFillTextVector(&LxStartStopCmdTP, LxStartStopCmdT, NARRAY(LxStartStopCmdT), device_name, "Start/Stop commands", "", LX_TAB, IP_RW, 0, IPS_IDLE);
  IUFillSwitch(&LxLogicalLevelS[0], "Low to High", "", ISS_ON);
  IUFillSwitch(&LxLogicalLevelS[1], "High to Low", "", ISS_OFF);
  IUFillSwitchVector(&LxLogicalLevelSP, LxLogicalLevelS, NARRAY(LxLogicalLevelS), device_name, "Start Transition", "", LX_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

  return true;
}

bool Lx::updateProperties() {
  if (dev->isConnected()) {
    dev->defineSwitch(&LxEnableSP);
    dev->defineSwitch(&LxModeSP);
    dev->defineText(&LxPortTP);
    dev->defineSwitch(&LxSerialOptionSP);
    dev->defineSwitch(&LxParallelOptionSP);
    dev->defineText(&LxStartStopCmdTP);
    dev->defineSwitch(&LxLogicalLevelSP);
  } else {
    dev->deleteProperty(LxEnableSP.name);
    dev->deleteProperty(LxModeSP.name);
    dev->deleteProperty(LxPortTP.name);
    dev->deleteProperty(LxSerialOptionSP.name);
    dev->deleteProperty(LxParallelOptionSP.name);
    dev->deleteProperty(LxStartStopCmdTP.name);
    dev->deleteProperty(LxLogicalLevelSP.name);
  }
  return true;
}

bool Lx::ISNewSwitch (const char *devname, const char *name, ISState *states, char *names[], int n) {
  
  /* ignore if not ours */
  if (devname && strcmp (device_name, devname))
    return true;

  if (!strcmp(name, LxEnableSP.name))
    {
      IUResetSwitch(&LxEnableSP);
      IUUpdateSwitch(&LxEnableSP, states, names, n);
      LxEnableSP.s = IPS_OK;
      
      IDSetSwitch(&LxEnableSP, "%s long exposure on device %s", (LxEnableS[0].s==ISS_ON?"Disbaling":"Enabling"), device_name);
      return true;
    }    

  if (!strcmp(name, LxModeSP.name))
    {
      unsigned int index, oldindex;
      oldindex=IUFindOnSwitchIndex(&LxModeSP);   
      IUResetSwitch(&LxModeSP);
      IUUpdateSwitch(&LxModeSP, states, names, n);
      LxModeSP.s = IPS_OK;
      index=IUFindOnSwitchIndex(&LxModeSP);   
      if (index == 2) 
	if (!checkPWC()) {
	  IUResetSwitch(&LxModeSP);
	  LxModeSP.s = IPS_ALERT;
	  LxModeS[oldindex].s=ISS_ON;
	  IDSetSwitch(&LxModeSP, "Can not set Lx Mode to %s", LxModeS[index].name);
	  return false;
	}
      IDSetSwitch(&LxModeSP, "Setting Lx Mode to %s", LxModeS[index].name);
      return true;
    }    

  if (!strcmp(name, LxSerialOptionSP.name))
    {
      unsigned int index;
      IUResetSwitch(&LxSerialOptionSP);
      IUUpdateSwitch(&LxSerialOptionSP, states, names, n);
      LxSerialOptionSP.s = IPS_OK;
      index=IUFindOnSwitchIndex(&LxSerialOptionSP);   
      IDSetSwitch(&LxSerialOptionSP, "Setting Lx Serial option: %s", LxSerialOptionS[index].name);
      return true;
    }

  if (!strcmp(name, LxParallelOptionSP.name))
    {
      unsigned int index;
      IUResetSwitch(&LxParallelOptionSP);
      IUUpdateSwitch(&LxParallelOptionSP, states, names, n);
      LxParallelOptionSP.s = IPS_OK;
      index=IUFindOnSwitchIndex(&LxParallelOptionSP);   
      IDSetSwitch(&LxParallelOptionSP, "Setting Lx Parallel option: %s", LxParallelOptionS[index].name);
      return true;
    }

  if (!strcmp(name, LxLogicalLevelSP.name))
    {
      unsigned int index;
      IUResetSwitch(&LxLogicalLevelSP);
      IUUpdateSwitch(&LxLogicalLevelSP, states, names, n);
      LxLogicalLevelSP.s = IPS_OK;
      index=IUFindOnSwitchIndex(&LxLogicalLevelSP);   
      IDSetSwitch(&LxLogicalLevelSP, "Setting Lx logical levels for start transition: %s", LxLogicalLevelS[index].name);
      return true;
    }
  return true; // not ours, don't care
}

bool Lx::ISNewText (const char *devname, const char *name, char *texts[], char *names[], int n) {

  IText *tp;
  /* ignore if not ours */
  if (devname && strcmp (device_name, devname))
    return true;

  if (!strcmp(name, LxPortTP.name) )
    {
      LxPortTP.s = IPS_OK;
      tp = IUFindText( &LxPortTP, names[0] );
      if (!tp)
	return false;

      IUSaveText(tp, texts[0]);
      IDSetText (&LxPortTP, "Setting Lx port to %s", tp->text);
      return true;
    }

   if (!strcmp(name, LxStartStopCmdTP.name) )
    {
      unsigned int i;
      LxStartStopCmdTP.s = IPS_OK;
      for (i=0; i<n; i++) {
	tp = IUFindText( &LxStartStopCmdTP, names[i] );
	if (!tp)
	  return false;
	IUSaveText(tp, texts[i]);
      }
      IDSetText (&LxStartStopCmdTP, "Setting Lx Start/stop commands");
      return true;
    } 


  return true; // not ours, don't care
}

int Lx::startLx() {
  unsigned int index;
  IDMessage(device_name, "Starting Long Exposure");
  index=IUFindOnSwitchIndex(&LxModeSP);   
  switch(index) {
  case 2:
    return startLxPWC();
  default:
    return -1;
  }
  return 0;
}

int Lx::stopLx() {
  unsigned int index;
  IDMessage(device_name, "Stopping Long Exposure");
  index=IUFindOnSwitchIndex(&LxModeSP);   
  switch(index) {
  case 2:
    return stopLxPWC();
  default:
    return -1;
  }

  return 0;
}

// PWC Stuff
bool Lx::checkPWC() {

  if (ioctl(camerafd, VIDIOCPWCPROBE, &probe) != 0) {
    IDMessage(device_name, "ERROR: device does not support PWC ioctl");
    return false;
  }
  if (probe.type < 730) {
    IDMessage(device_name, "ERROR: camera type %d does not support led control", probe.type);
    return false;
  }

  return true;
}

void Lx::pwcsetLed(int on, int off)  
{
      struct pwc_leds leds;
      leds.led_on=on;
      leds.led_off=off;
      if (ioctl(camerafd, VIDIOCPWCSLED, &leds)) {
	IDLog("ioctl: can't set Led.\n");
      }
}

int Lx::startLxPWC() {
  if (LxLogicalLevelS[0].s == ISS_ON)
    pwcsetLed(25500, 0);
  else
    pwcsetLed(0,25500);
  return 0;
}

int Lx::stopLxPWC() {
  if (LxLogicalLevelS[0].s == ISS_ON)
    pwcsetLed(0,25500);
  else
    pwcsetLed(25500, 0);
  return 0;
}
