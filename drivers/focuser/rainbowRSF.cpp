#include "rainbowRSF.h"
//#include "indicom.h"
//#include "connectionplugins/connectionserial.h"

//#include <cmath>
//#include <memory>
//#include <cstring>
//#include <termios.h>
//#include <unistd.h>
//#include <regex>


static std::unique_ptr<RainbowRSF> rainbowRSF(new RainbowRSF());

void ISGetProperties(const char *dev)
{
    rainbowRSF->ISGetProperties(dev);
}
void ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    rainbowRSF->ISNewSwitch(dev, name, states, names, n);
}

void ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n)
{
    rainbowRSF->ISNewText(dev, name, texts, names, n);
}

void ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    rainbowRSF->ISNewNumber(dev, name, values, names, n);
}

void ISNewBLOB(const char *dev, const char *name, int sizes[], int blobsizes[], char *blobs[], char *formats[],
               char *names[], int n)
{
    INDI_UNUSED(dev);
    INDI_UNUSED(name);
    INDI_UNUSED(sizes);
    INDI_UNUSED(blobsizes);
    INDI_UNUSED(blobs);
    INDI_UNUSED(formats);
    INDI_UNUSED(names);
    INDI_UNUSED(n);
}

void ISSnoopDevice(XMLEle *root)
{
    rainbowRSF->ISSnoopDevice(root);
}

RainbowRSF::RainbowRSF()
{
    setVersion(1, 0);

    FI::SetCapability(FOCUSER_CAN_ABS_MOVE | FOCUSER_CAN_REL_MOVE);
}

const char *RainbowRSF::getDefaultName()
{
    return "Rainbow Astro RSF";
}

bool RainbowRSF::Handshake()
{
    return true;
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////

bool RainbowRSF::initProperties()
{
    INDI::Focuser::initProperties();

    // Firmware Information
    IUFillText(&FirmwareT[0], "VERSION", "Version", "");
    IUFillTextVector(&FirmwareTP, FirmwareT, 1, getDeviceName(), "FOCUS_FIRMWARE", "Firmware", MAIN_CONTROL_TAB, IP_RO, 0,
                     IPS_IDLE);

    return true;
}
