#ifndef LX_H
#define LX_H

//#include <config.h>

#include <indiapi.h>
#include <defaultdevice.h>

//For serial control
#include <termios.h>

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
ISwitch LxSerialSpeedS[9];
ISwitchVectorProperty LxSerialSpeedSP;
ISwitch LxSerialSizeS[4];
ISwitchVectorProperty LxSerialSizeSP;
ISwitch LxSerialParityS[3];
ISwitchVectorProperty LxSerialParitySP;
ISwitch LxSerialStopS[2];
ISwitchVectorProperty LxSerialStopSP;
ISwitch LxSerialAddeolS[4];
ISwitchVectorProperty LxSerialAddeolSP;

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

// Serial 
int fd;
struct termios oldterminfo;
void closeserial(int fd);
int openserial(char *devicename);
int setRTS(int fd, int level);
int setDTR(int fd, int level);
int startLxSerial();
int stopLxSerial();
void getSerialOptions(unsigned int *speed, unsigned int *wordsize, unsigned int *parity, unsigned int *stops);
const char * getSerialEOL();

// PWC Cameras
struct pwc_probe probe;  
bool checkPWC();
void pwcsetLed(int on, int off);
int startLxPWC();
int stopLxPWC();
};
#endif /* LX_H */
