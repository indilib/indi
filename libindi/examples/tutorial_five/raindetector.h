#ifndef RAINDETECTOR_H
#define RAINDETECTOR_H

/*
   INDI Developers Manual
   Tutorial #5 - Snooping

   Rain Detector

   Refer to README, which contains instruction on how to build this driver, and use it
   with an INDI-compatible client.

*/

#include "indibase/defaultdevice.h"

class RainDetector : public INDI::DefaultDevice
{
public:
    RainDetector();
    bool ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n);

protected:
    // General device functions
    bool Connect();
    bool Disconnect();
    const char *getDefaultName();
    bool initProperties();
    bool updateProperties();

private:
    ILight RainL[1];
    ILightVectorProperty RainLP;

    ISwitch RainS[2];
    ISwitchVectorProperty RainSP;
};

#endif // RAINDETECTOR_H
