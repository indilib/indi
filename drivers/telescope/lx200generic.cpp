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
#include "lx200_OpenAstroTech.h"
#include "lx200ap_v2.h"
#include "lx200ap_gtocp2.h"
#include "lx200classic.h"
#include "lx200fs2.h"
#include "lx200gemini.h"
#include "lx200pulsar2.h"
#include "lx200ss2000pc.h"
#include "lx200zeq25.h"
#include "lx200gotonova.h"
#include "ioptronHC8406.h"
#include "lx200am5.h"
#include "lx200_pegasus_nyx101.h"
#include <cmath>
#include <memory>
#include <cstring>
#include <unistd.h>
#include "eq500x.h"

    /* There is _one_ binary for all LX200 drivers, but each binary is renamed
    ** to its device name (i.e. lx200gps, lx200_16..etc). The main function will
    ** fetch from std args the binary name and ISInit will create the appropriate
    ** device afterwards. If the binary name does not match any known devices,
    ** we simply create a generic device.
    */
    extern char *__progname;

#define LX200_TRACK 0
#define LX200_SYNC  1

static class Loader
{
        std::unique_ptr<LX200Generic> telescope;
    public:
        Loader()
        {
            // Note: these if statements use strstr() which isn't a full string match, just a substring,
            // so if one driver name is the start of another's name, it needs to be AFTER the longer one!
            if (strstr(__progname, "indi_lx200classic"))
            {
                IDLog("initializing from LX200 classic device...\n");
                telescope.reset(new LX200Classic());
            }
            else if (strstr(__progname, "indi_lx200_OnStep"))
            {
                IDLog("initializing from LX200 OnStep device...\n");
                telescope.reset(new LX200_OnStep());
            }
            else if (strstr(__progname, "indi_lx200gps"))
            {
                IDLog("initializing from LX200 GPS device...\n");
                telescope.reset(new LX200GPS());
            }
            else if (strstr(__progname, "indi_lx200_16"))
            {
                IDLog("Initializing from LX200 16 device...\n");
                telescope.reset(new LX200_16());
            }
            else if (strstr(__progname, "indi_lx200autostar"))
            {
                IDLog("initializing from Autostar device...\n");
                telescope.reset(new LX200Autostar());
            }
            else if (strstr(__progname, "indi_lx200ap_v2"))
            {
                IDLog("initializing from Astrophysics V2 device...\n");
                telescope.reset(new LX200AstroPhysicsV2());
            }
            else if (strstr(__progname, "indi_lx200ap_legacy"))
            {
                IDLog("initializing from Astrophysics GTOCP2 device...\n");
                telescope.reset(new LX200AstroPhysicsGTOCP2());
            }
            else if (strstr(__progname, "indi_lx200gemini"))
            {
                IDLog("initializing from Losmandy Gemini device...\n");
                telescope.reset(new LX200Gemini());
            }
            else if (strstr(__progname, "indi_lx200zeq25"))
            {
                IDLog("initializing from ZEQ25 device...\n");
                telescope.reset(new LX200ZEQ25());
            }
            else if (strstr(__progname, "indi_lx200gotonova"))
            {
                IDLog("initializing from GotoNova device...\n");
                telescope.reset(new LX200GotoNova());
            }
            else if (strstr(__progname, "indi_ioptronHC8406"))
            {
                IDLog("initializing from ioptron telescope Hand Controller HC8406 device...\n");
                telescope.reset(new ioptronHC8406());
            }
            else if (strstr(__progname, "indi_lx200pulsar2"))
            {
                IDLog("initializing from pulsar2 device...\n");
                telescope.reset(new LX200Pulsar2());
            }
            else if (strstr(__progname, "indi_lx200ss2000pc"))
            {
                IDLog("initializing from skysensor2000pc device...\n");
                telescope.reset(new LX200SS2000PC());
            }
            else if (strstr(__progname, "indi_lx200fs2"))
            {
                IDLog("initializing from Astro-Electronic FS-2...\n");
                telescope.reset(new LX200FS2());
            }
            else if (strstr(__progname, "indi_lx200_10micron"))
            {
                IDLog("initializing for 10Micron mount...\n");
                telescope.reset(new LX200_10MICRON());
            }
            else if (strstr(__progname, "indi_eq500x"))
            {
                IDLog("initializing for EQ500X mount...\n");
                telescope.reset(new EQ500X());
            }
            else if (strstr(__progname, "indi_lx200am5"))
            {
                IDLog("initializing for ZWO AM5 mount...\n");
                telescope.reset(new LX200AM5());
            }
            else if (strstr(__progname, "indi_lx200_OpenAstroTech"))
            {
                IDLog("initializing for OpenAstroTech mount...\n");
                telescope.reset(new LX200_OpenAstroTech());
            }
            else if (strstr(__progname, "indi_lx200_pegasus_nyx101"))
            {
                IDLog("initializing for Pegasus NYX-101 mount...\n");
                telescope.reset(new LX200NYX101());
            }
            // be nice and give them a generic device
            else
                telescope.reset(new LX200Generic());
        }
} loader;

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
