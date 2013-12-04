#ifndef LX_H
#define LX_H

//#include <config.h>

#include <indiapi.h>
#include <defaultdevice.h>

//For SPC900 Led control
#include "webcam/pwc-ioctl.h"
#include <sys/ioctl.h>

#define LX_TAB "Long Exposure"

class Lx {
 public:

  ISwitch LxEnableS[2];
  ISwitchVectorProperty LxEnableSP;
  ISwitch LxModeS[4];
  ISwitchVectorProperty LxModeSP;
  IText LxPortT[1];
  ITextVectorProperty LxPortTP;
  ISwitch LxSerialOptionS[3];
  ISwitchVectorProperty LxSerialOptionSP;
  ISwitch LxParallelOptionS[9];
  ISwitchVectorProperty LxParallelOptionSP;
  IText LxStartStopCmdT[2];
  ITextVectorProperty LxStartStopCmdTP;
  ISwitch LxLogicalLevelS[2];
  ISwitchVectorProperty LxLogicalLevelSP;

  bool isenabled();
  void setCamerafd(int fd);
  bool initProperties(INDI::DefaultDevice *device);
  bool ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n);
  bool ISNewText (const char *dev, const char *name, char *texts[], char *names[], int n);
  bool updateProperties();
  int startLx();
  int stopLx();

 private:
  INDI::DefaultDevice *dev;
  const char *device_name;
  int camerafd;
  
  // PWC Cameras
  struct pwc_probe probe;  
  bool checkPWC();
  void pwcsetLed(int on, int off);
  int startLxPWC();
  int stopLxPWC();
};
#endif /* LX_H */
