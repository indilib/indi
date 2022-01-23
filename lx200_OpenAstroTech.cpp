/*
    OpenAstroTech
    Copyright (C) 2021 Anjo Krank

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA

*/

#include "lx200_OpenAstroTech.h"

#include "indicom.h"
#include "lx200driver.h"

#include <cmath>
#include <string.h>

#include <libnova/transform.h>
#include <termios.h>
#include <unistd.h>

#define RB_MAX_LEN    64

LX200_OpenAstroTech::LX200_OpenAstroTech(void) : LX200GPS()
{
    setVersion(0,9);
/*
    setLX200Capability(LX200_HAS_PULSE_GUIDING);

    SetTelescopeCapability(TELESCOPE_CAN_SYNC |
                           TELESCOPE_CAN_GOTO |
                           TELESCOPE_CAN_ABORT |
                           TELESCOPE_HAS_TIME |
                           TELESCOPE_CAN_PARK |
                           TELESCOPE_HAS_LOCATION, 4);
*/
}
bool on = false;

bool LX200_OpenAstroTech::initProperties()
{
    LX200GPS::initProperties();
    IUFillText(&MeadeCommandT, "MEADE_COMMAND", "Result / Command", "");
    IUFillTextVector(&MeadeCommandTP, &MeadeCommandT, 1, getDeviceName(), "Meade", "",
                       OPTIONS_TAB, IP_RW, 0, IPS_IDLE);
    return true;
}

bool LX200_OpenAstroTech::updateProperties()
{
    LX200GPS::updateProperties();

    if (isConnected())
    {
        defineProperty(&MeadeCommandTP);
    }
    else
    {
        deleteProperty(MeadeCommandTP.name);
    }

    return true;
}

bool LX200_OpenAstroTech::ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        if (!strcmp(name, MeadeCommandTP.name))
        {
            if(!isSimulation()) {
                // we're using the "Set" field for the command and the actual field for the result
                // we need to:
                // - get the value
                // - check if it's a command
                // - if so, execute it, then set the result to the control
                // the client side needs to
                // - push ":somecmd#"
                // - listen to change on MeadeCommand and log it
                char * cmd = texts[0];
                size_t len = strlen(cmd);
                DEBUGFDEVICE(getDeviceName(), DBG_SCOPE, "Meade Command <%s>", cmd);
                if(len > 2 && cmd[0] == ':' && cmd[len-1] == '#') {
                    IText *tp = IUFindText(&MeadeCommandTP, names[0]);
                    int err = executeMeadeCommand(texts[0]);
                    DEBUGFDEVICE(getDeviceName(), DBG_SCOPE, "Meade Command Result %d <%s>", err, MeadeCommandResult);
                    if(err == 0) {
                        MeadeCommandTP.s = IPS_OK;
                        IUSaveText(tp, MeadeCommandResult); // really?
                        IDSetText(&MeadeCommandTP, MeadeCommandResult);
                        return true;
                    } else {
                        MeadeCommandTP.s = IPS_ALERT;
                        IDSetText(&MeadeCommandTP, nullptr);
                        return true;
                    }
                }
            }
       }
    }

    return INDI::Telescope::ISNewText(dev, name, texts, names, n);
}

bool LX200_OpenAstroTech::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    if (strcmp(dev, getDeviceName()) == 0)
    {
        /* left in for later use
        if (!strcmp(name, SlewAccuracyNP.name))
        {
            if (IUUpdateNumber(&SlewAccuracyNP, values, names, n) < 0)
                return false;

            SlewAccuracyNP.s = IPS_OK;

            if (SlewAccuracyN[0].value < 3 || SlewAccuracyN[1].value < 3)
                IDSetNumber(&SlewAccuracyNP, "Warning: Setting the slew accuracy too low may result in a dead lock");

            IDSetNumber(&SlewAccuracyNP, nullptr);
            return true;
        }*/
    }

    return LX200GPS::ISNewNumber(dev, name, values, names, n);
}

const char *LX200_OpenAstroTech::getDefaultName(void)
{
    return const_cast<const char *>("LX200 OpenAstroTech");
}

int LX200_OpenAstroTech::executeMeadeCommand(char *cmd)
{
    return getCommandString(PortFD, MeadeCommandResult, cmd);
}
