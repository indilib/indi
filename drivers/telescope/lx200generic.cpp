#if 0
LX200 Generic
Copyright (C) 2003 - 2017 Jasem Mutlaq (mutlaqja@ikarustech.com)

This library is free software;
you can redistribute it and / or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation;
either
version 2.1 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
     but WITHOUT ANY WARRANTY;
without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library;
if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110 - 1301  USA

2013 - 10 - 27:
Updated driver to use INDI::Telescope (JM)
2015 - 11 - 25:
    Use variable POLLMS instead of static POLLMS

#endif

#include "lx200generic.h"

#include "lx200_10micron.h"
#include "lx200_16.h"
#include "lx200_OnStep.h"
#include "lx200ap_experimental.h"
#include "lx200ap_gtocp2.h"
#include "lx200ap.h"
#include "lx200classic.h"
#include "lx200fs2.h"
#include "lx200gemini.h"
#include "lx200pulsar2.h"
#include "lx200ss2000pc.h"
#include "lx200zeq25.h"
#include "lx200gotonova.h"
#include "ioptronHC8406.h"
#include <cmath>
#include <memory>
#include <cstring>
#include <unistd.h>
#include "eq500x.h"

    // We declare an auto pointer to LX200Generic.
    static std::unique_ptr<LX200Generic> telescope;

/* There is _one_ binary for all LX200 drivers, but each binary is renamed
** to its device name (i.e. lx200gps, lx200_16..etc). The main function will
** fetch from std args the binary name and ISInit will create the apporpiate
** device afterwards. If the binary name does not match any known devices,
** we simply create a generic device.
*/
extern char *me;

#define LX200_TRACK 0
#define LX200_SYNC  1

/* send client definitions of all properties */
void ISInit()
{
    static int isInit = 0;

    if (isInit)
        return;

    isInit = 1;

    if (strstr(me, "indi_lx200classic"))
    {
        IDLog("initializing from LX200 classic device...\n");

        if (telescope.get() == nullptr)
            telescope.reset(new LX200Classic());
    }
    if (strstr(me, "indi_lx200_OnStep"))
    {
        IDLog("initializing from LX200 OnStep device...\n");

        if (telescope.get() == nullptr)
            telescope.reset(new LX200_OnStep());
    }
    else if (strstr(me, "indi_lx200gps"))
    {
        IDLog("initializing from LX200 GPS device...\n");

        if (telescope.get() == nullptr)
            telescope.reset(new LX200GPS());
    }
    else if (strstr(me, "indi_lx200_16"))
    {
        IDLog("Initializing from LX200 16 device...\n");

        if (telescope.get() == nullptr)
            telescope.reset(new LX200_16());
    }
    else if (strstr(me, "indi_lx200autostar"))
    {
        IDLog("initializing from Autostar device...\n");

        if (telescope.get() == nullptr)
            telescope.reset(new LX200Autostar());
    }
    else if (strstr(me, "indi_lx200ap_experimental"))
    {
        IDLog("initializing from Astrophysics Experiemtal device...\n");

        if (telescope.get() == nullptr)
            telescope.reset(new LX200AstroPhysicsExperimental());
    }
    else if (strstr(me, "indi_lx200ap_gtocp2"))
    {
        IDLog("initializing from Astrophysics GTOCP2 device...\n");

        if (telescope.get() == nullptr)
            telescope.reset(new LX200AstroPhysicsGTOCP2());
    }
    else if (strstr(me, "indi_lx200ap"))
    {
        IDLog("initializing from Astrophysics device...\n");

        if (telescope.get() == nullptr)
            telescope.reset(new LX200AstroPhysics());
    }
    else if (strstr(me, "indi_lx200gemini"))
    {
        IDLog("initializing from Losmandy Gemini device...\n");

        if (telescope.get() == nullptr)
            telescope.reset(new LX200Gemini());
    }
    else if (strstr(me, "indi_lx200zeq25"))
    {
        IDLog("initializing from ZEQ25 device...\n");

        if (telescope.get() == nullptr)
            telescope.reset(new LX200ZEQ25());
    }
    else if (strstr(me, "indi_lx200gotonova"))
    {
        IDLog("initializing from GotoNova device...\n");

        if (telescope.get() == nullptr)
            telescope.reset(new LX200GotoNova());
    }
    else if (strstr(me, "indi_ioptronHC8406"))
    {
        IDLog("initializing from ioptron telescope Hand Controller HC8406 device...\n");

        if (telescope.get() == nullptr)
            telescope.reset(new ioptronHC8406());
    }
    else if (strstr(me, "indi_lx200pulsar2"))
    {
        IDLog("initializing from pulsar2 device...\n");

        if (telescope.get() == nullptr)
            telescope.reset(new LX200Pulsar2());
    }
    else if (strstr(me, "indi_lx200ss2000pc"))
    {
        IDLog("initializing from skysensor2000pc device...\n");

        if (telescope.get() == nullptr)
            telescope.reset(new LX200SS2000PC());
    }
    else if (strstr(me, "indi_lx200fs2"))
    {
        IDLog("initializing from Astro-Electronic FS-2...\n");

        if (telescope.get() == nullptr)
            telescope.reset(new LX200FS2());
    }
    else if (strstr(me, "indi_lx200_10micron"))
    {
        IDLog("initializing for 10Micron mount...\n");

        if (telescope.get() == nullptr)
            telescope.reset(new LX200_10MICRON());
    }
    else if (strstr(me, "indi_eq500x"))
    {
        IDLog("initializing for EQ500X mount...\n");

        if (telescope.get() == nullptr)
            telescope.reset(new EQ500X());
    }
    // be nice and give them a generic device
    else if (telescope.get() == nullptr)
        telescope.reset(new LX200Generic());
}

void ISGetProperties(const char *dev)
{
    ISInit();
    telescope->ISGetProperties(dev);
}

void ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    ISInit();
    telescope->ISNewSwitch(dev, name, states, names, n);
}

void ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n)
{
    ISInit();
    telescope->ISNewText(dev, name, texts, names, n);
}

void ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    ISInit();
    telescope->ISNewNumber(dev, name, values, names, n);
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
    ISInit();
    telescope->ISSnoopDevice(root);
}

/**************************************************
*** LX200 Generic Implementation
***************************************************/

LX200Generic::LX200Generic()
{
    setVersion(2, 1);

    currentSiteNum = 1;
    trackingMode   = LX200_TRACK_SIDEREAL;
    GuideNSTID     = 0;
    GuideWETID     = 0;

    DBG_SCOPE = INDI::Logger::getInstance().addDebugLevel("Scope Verbose", "SCOPE");

    setLX200Capability(LX200_HAS_FOCUS | LX200_HAS_TRACKING_FREQ | LX200_HAS_ALIGNMENT_TYPE | LX200_HAS_SITES |
                       LX200_HAS_PULSE_GUIDING);

    SetTelescopeCapability(TELESCOPE_CAN_PARK | TELESCOPE_CAN_SYNC | TELESCOPE_CAN_GOTO | TELESCOPE_CAN_ABORT |
                           TELESCOPE_HAS_TIME | TELESCOPE_HAS_LOCATION | TELESCOPE_HAS_TRACK_MODE,
                           4);

    LOG_DEBUG("Initializing from Generic LX200 device...");
}
