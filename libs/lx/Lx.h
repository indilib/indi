
#pragma once

#include "defaultdevice.h"

//For SPC900 Led control
#include "webcam/pwc-ioctl.h"

//For serial control
#include <termios.h>

/* Smart Widget-Property */
#include "indipropertytext.h"
#include "indipropertyswitch.h"

#define LX_TAB "Long Exposure"
// LX Modes
#define LXSERIAL   0
#define LXLED      1
#define LXPARALLEL 2
#define LXGPIO     3

#define LXMODENUM 2

class Lx
{
  public:
    INDI::PropertySwitch LxEnableSP {2};
    INDI::PropertySwitch LxModeSP {LXMODENUM};
    INDI::PropertyText LxPortTP {1};
    INDI::PropertySwitch LxSerialOptionSP {3};
    INDI::PropertySwitch LxParallelOptionSP {9};
    INDI::PropertyText LxStartStopCmdTP {2};
    INDI::PropertySwitch LxLogicalLevelSP {2};
    INDI::PropertySwitch LxSerialSpeedSP {9};
    INDI::PropertySwitch LxSerialSizeSP {4};
    INDI::PropertySwitch LxSerialParitySP {3};
    INDI::PropertySwitch LxSerialStopSP {2};
    INDI::PropertySwitch LxSerialAddeolSP {4};

    bool isEnabled();
    void setCamerafd(int fd);
    bool initProperties(INDI::DefaultDevice *device);
    bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n);
    bool ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n);
    bool updateProperties();
    bool startLx();
    int stopLx();
    unsigned int getLxmode();

  private:
    INDI::DefaultDevice *dev;
    const char *device_name;
    int camerafd;

    // Serial
    int serialfd;
    struct termios oldterminfo;
    void closeserial(int fd);
    int openserial(const char *devicename);
    int setRTS(int fd, int level);
    int setDTR(int fd, int level);
    bool startLxSerial();
    int stopLxSerial();
    void getSerialOptions(unsigned int *speed, unsigned int *wordsize, unsigned int *parity, unsigned int *stops);
    const char *getSerialEOL();
    INDI::Property *findbyLabel(INDI::DefaultDevice *dev, char *label);
    // PWC Cameras
    ISwitchVectorProperty *FlashStrobeSP;
    ISwitchVectorProperty *FlashStrobeStopSP;
    enum pwcledmethod
    {
        PWCIOCTL,
        FLASHLED
    };
    char ledmethod;
    struct pwc_probe probe;
    bool checkPWC();
    void pwcsetLed(int on, int off);
    void pwcsetflashon();
    void pwcsetflashoff();
    bool startLxPWC();
    int stopLxPWC();
};
