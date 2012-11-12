#ifndef DOME_H
#define DOME_H

/*
   INDI Developers Manual
   Tutorial #5 - Snooping

   Dome

   Refer to README, which contains instruction on how to build this driver, and use it
   with an INDI-compatible client.

*/

#include "indibase/defaultdevice.h"

class Dome : public INDI::DefaultDevice
{
public:
    Dome();
    bool ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n);
    bool ISSnoopDevice (XMLEle *root);

protected:
    // General device functions
    bool Connect();
    bool Disconnect();
    const char *getDefaultName();
    bool initProperties();
    bool updateProperties();

private:
    void closeShutter();

    ISwitch ShutterS[2];
    ISwitchVectorProperty ShutterSP;

    ILight RainL[1];
    ILightVectorProperty RainLP;

};

#endif // DOME_H
