/*
    Astro-Electronic FS-2 Driver
    Copyright (C) 2015 Jasem Mutlaq (mutlaqja@ikarustech.com)

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

#include <math.h>

#include "lx200fs2.h"

LX200FS2::LX200FS2() : LX200Generic()
{
    setVersion(2, 0);

    TelescopeCapability cap;

    cap.canPark = false;
    cap.canSync = true;
    cap.canAbort = true;
    cap.nSlewRate=4;
    SetTelescopeCapability(&cap);
}

bool LX200FS2::initProperties()
{
    LX200Generic::initProperties();

    IUFillNumber(&SlewAccuracyN[0], "SlewRA",  "RA (arcmin)", "%10.6m",  0., 60., 1., 3.0);
    IUFillNumber(&SlewAccuracyN[1], "SlewDEC", "Dec (arcmin)", "%10.6m", 0., 60., 1., 3.0);
    IUFillNumberVector(&SlewAccuracyNP, SlewAccuracyN, NARRAY(SlewAccuracyN), getDeviceName(), "Slew Accuracy", "", OPTIONS_TAB, IP_RW, 0, IPS_IDLE);

    return true;
}

bool LX200FS2::updateProperties()
{
    INDI::Telescope::updateProperties();

    if (isConnected())
    {
        defineSwitch(&SlewRateSP);
        defineNumber(&SlewAccuracyNP);
    }
    else
    {
        deleteProperty(SlewRateSP.name);
        deleteProperty(SlewAccuracyNP.name);
    }
}

bool LX200FS2::ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n)
{
    if(strcmp(dev,getDeviceName())==0)
    {
        if (!strcmp(name, SlewAccuracyNP.name))
        {
            if (IUUpdateNumber(&SlewAccuracyNP, values, names, n) < 0)
                return false;

            SlewAccuracyNP.s = IPS_OK;

            if (SlewAccuracyN[0].value < 3 || SlewAccuracyN[1].value < 3)
                IDSetNumber(&SlewAccuracyNP, "Warning: Setting the slew accuracy too low may result in a dead lock");

            IDSetNumber(&SlewAccuracyNP, NULL);
            return true;
        }
    }

    return LX200Generic::ISNewNumber(dev, name, values, names, n);
}


const char * LX200FS2::getDefaultName()
{
    return (char *)"Astro-Electronic FS-2";
}


bool LX200FS2::isSlewComplete()
{
    const double dx = targetRA - currentRA;
    const double dy = targetDEC - currentDEC;
    return fabs(dx) <= (SlewAccuracyN[0].value/(900.0)) && fabs(dy) <= (SlewAccuracyN[1].value/60.0);
}

bool LX200FS2::checkConnection()
{
    return true;
}

bool LX200FS2::updateTime(ln_date *utc, double utc_offset)
{
    INDI_UNUSED(utc);
    INDI_UNUSED(utc_offset);

    return true;
}

bool LX200FS2::updateLocation(double latitude, double longitude, double elevation)
{
    INDI_UNUSED(latitude);
    INDI_UNUSED(longitude);
    INDI_UNUSED(elevation);

    return true;
}

bool LX200FS2::saveConfigItems(FILE *fp)
{
    INDI::Telescope::saveConfigItems(fp);

    IUSaveConfigNumber(fp, &SlewAccuracyNP);

    return true;
}
