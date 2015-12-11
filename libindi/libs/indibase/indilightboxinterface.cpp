/*
    Dust Cap Interface
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

#include <string.h>

#include "indilightboxinterface.h"
#include "indilogger.h"

INDI::LightBoxInterface::LightBoxInterface()
{
}

INDI::LightBoxInterface::~LightBoxInterface()
{
}

void INDI::LightBoxInterface::initLightBoxProperties(const char *deviceName, const char* groupName)
{
    strncpy(lightBoxName, deviceName, MAXINDIDEVICE);

    // Turn on/off light
    IUFillSwitch(&LightS[0], "FLAT_LIGHT_ON", "On", ISS_OFF);
    IUFillSwitch(&LightS[1], "FLAT_LIGHT_OFF", "Off", ISS_OFF);
    IUFillSwitchVector(&LightSP, LightS, 2, deviceName, "FLAT_LIGHT_CONTROL", "Flat Light", groupName, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    // Light Intensity
    IUFillNumber(&LightIntensityN[0], "FLAT_LIGHT_INTENSITY_VALUE", "Value", "%.f", 0, 255, 10, 0);
    IUFillNumberVector(&LightIntensityNP, LightIntensityN, 1, deviceName, "FLAT_LIGHT_INTENSITY", "Brightness", groupName, IP_RW, 0, IPS_IDLE);
}

bool INDI::LightBoxInterface::processLightBoxSwitch (const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if(strcmp(dev,lightBoxName)==0)
    {
        // Light
        if (!strcmp(LightSP.name, name))
        {
            int prevIndex = IUFindOnSwitchIndex(&LightSP);
            IUUpdateSwitch(&LightSP, states, names, n);
            bool rc = EnableLightBox(LightS[0].s == ISS_ON ? true : false);

            LightSP.s = rc ? IPS_OK : IPS_ALERT;

            if (rc == false)
            {
                IUResetSwitch(&LightSP);
                LightS[prevIndex].s = ISS_ON;
            }

            IDSetSwitch(&LightSP, NULL);

            return true;
        }
    }

    return false;
}

bool INDI::LightBoxInterface::processLightBoxNumber (const char *dev, const char *name, double values[], char *names[], int n)
{
    if(strcmp(dev,lightBoxName)==0)
    {
        // Light Intensity
        if (!strcmp(LightIntensityNP.name, name))
        {
            double prevValue = LightIntensityN[0].value;
            IUUpdateNumber(&LightIntensityNP, values, names, n);

            bool rc = SetLightBoxBrightness(LightIntensityN[0].value);
            if (rc)
                LightIntensityNP.s = IPS_OK;
            else
            {
                LightIntensityN[0].value = prevValue;
                LightIntensityNP.s = IPS_ALERT;
            }

            IDSetNumber(&LightIntensityNP, NULL);

            return true;
        }
    }

    return false;
}

bool INDI::LightBoxInterface::EnableLightBox(bool enable)
{
    INDI_UNUSED(enable);
    // Must be implemented by child class
    return false;
}

bool INDI::LightBoxInterface::SetLightBoxBrightness(uint16_t value)
{
    INDI_UNUSED(value);
    // Must be implemented by child class
    return false;
}
