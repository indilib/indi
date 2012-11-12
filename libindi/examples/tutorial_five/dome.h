#ifndef DOME_H
#define DOME_H

/*
   INDI Developers Manual
   Tutorial #5 - Snooping

   Dome

   Refer to README, which contains instruction on how to build this driver, and use it
   with an INDI-compatible client.

*/

/** \file dome.h
    \brief Construct a dome device that the user may operate to open or close the dome shutter door. This driver is \e snooping on the Rain Detector rain property status.
    If rain property state is alert, we close the dome shutter door if it is open, and we prevent the user from opening it until the rain threat passes.
    \author Jasem Mutlaq
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
