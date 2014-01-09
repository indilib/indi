/*******************************************************************************
  Copyright(c) 2013 Jasem Mutlaq. All rights reserved.

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Library General Public
 License version 2 as published by the Free Software Foundation.

 This library is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 Library General Public License for more details.

 You should have received a copy of the GNU Library General Public License
 along with this library; see the file COPYING.LIB.  If not, write to
 the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 Boston, MA 02110-1301, USA.
*******************************************************************************/

#include "indifocuser.h"

#include <string.h>

INDI::Focuser::Focuser()
{

}

INDI::Focuser::~Focuser()
{
}

bool INDI::Focuser::initProperties()
{
    DefaultDevice::initProperties();   //  let the base class flesh in what it wants

    initFocuserProperties(getDeviceName(),  MAIN_CONTROL_TAB);

    /* Port */
    IUFillText(&PortT[0], "PORT", "Port", "/dev/ttyUSB0");
    IUFillTextVector(&PortTP, PortT, 1, getDeviceName(), "DEVICE_PORT", "Ports", MAIN_CONTROL_TAB, IP_RW, 0, IPS_IDLE);

    // Presets
    IUFillNumber(&PresetN[0], "Preset 1", "", "%6.2f", 0, 60000, 1000, 0);
    IUFillNumber(&PresetN[1], "Preset 2", "", "%6.2f", 0, 60000, 1000, 0);
    IUFillNumber(&PresetN[2], "Preset 3", "", "%6.2f", 0, 60000, 1000, 0);
    IUFillNumberVector(&PresetNP, PresetN, 3, getDeviceName(), "Presets", "", "Presets", IP_RW, 0, IPS_IDLE);

    //Preset GOTO
    IUFillSwitch(&PresetGotoS[0], "Preset 1", "", ISS_OFF);
    IUFillSwitch(&PresetGotoS[1], "Preset 2", "", ISS_OFF);
    IUFillSwitch(&PresetGotoS[2], "Preset 3", "", ISS_OFF);
    IUFillSwitchVector(&PresetGotoSP, PresetGotoS, 3, getDeviceName(), "Goto", "", "Presets", IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    addDebugControl();

    return true;
}

void INDI::Focuser::ISGetProperties (const char *dev)
{
    //  First we let our parent populate
    DefaultDevice::ISGetProperties(dev);

    defineText(&PortTP);

    return;
}

bool INDI::Focuser::updateProperties()
{
    if(isConnected())
    {
        //  Now we add our focusser specific stuff
        defineSwitch(&FocusMotionSP);

        if (variableSpeed)
        {
            defineNumber(&FocusSpeedNP);
            defineNumber(&FocusTimerNP);
        }
        if (canRelMove)
            defineNumber(&FocusRelPosNP);
        if (canAbsMove)            
            defineNumber(&FocusAbsPosNP);
        if (canAbort)
            defineSwitch(&AbortSP);
        if (canAbsMove)
        {
            defineNumber(&PresetNP);
            defineSwitch(&PresetGotoSP);
        }
    } else
    {
        deleteProperty(FocusMotionSP.name);
        if (variableSpeed)
        {
            deleteProperty(FocusSpeedNP.name);
            deleteProperty(FocusTimerNP.name);
        }
        if (canRelMove)
            deleteProperty(FocusRelPosNP.name);
        if (canAbsMove)
            deleteProperty(FocusAbsPosNP.name);
        if (canAbort)
            deleteProperty(AbortSP.name);
        if (canAbsMove)
        {
            deleteProperty(PresetNP.name);
            deleteProperty(PresetGotoSP.name);
        }
    }
    return true;
}


bool INDI::Focuser::ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n)
{
    //  first check if it's for our device
    if(strcmp(dev,getDeviceName())==0)
    {
        if (!strcmp(name, PresetNP.name))
        {
            IUUpdateNumber(&PresetNP, values, names, n);
            PresetNP.s = IPS_OK;
            IDSetNumber(&PresetNP, NULL);

            saveConfig();

            return true;
        }

        if (strstr(name, "FOCUS_"))
            return processFocuserNumber(dev, name, values, names, n);

    }


    return DefaultDevice::ISNewNumber(dev,name,values,names,n);
}

bool INDI::Focuser::ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n)
{

    if(strcmp(dev,getDeviceName())==0)
    {
        if (!strcmp(PresetGotoSP.name, name))
        {
            IUUpdateSwitch(&PresetGotoSP, states, names, n);
            int index = IUFindOnSwitchIndex(&PresetGotoSP);
            int rc = MoveAbs(PresetN[index].value);
            if (rc >= 0)
            {
                PresetGotoSP.s = IPS_OK;
                DEBUGF(INDI::Logger::DBG_SESSION, "Moving to Preset %d with position %g.", index+1, PresetN[index].value);
                IDSetSwitch(&PresetGotoSP, NULL);
                return true;
            }

            PresetGotoSP.s = IPS_ALERT;
            IDSetSwitch(&PresetGotoSP, NULL);
            return false;
        }

        if (strstr(name, "FOCUS_"))
            return processFocuserSwitch(dev, name, states, names, n);

    }

    //  Nobody has claimed this, so, ignore it
    return DefaultDevice::ISNewSwitch(dev,name,states,names,n);
}

bool INDI::Focuser::ISNewText (const char *dev, const char *name, char *texts[], char *names[], int n)
{
    if(strcmp(dev,getDeviceName())==0)
    {
        if (!strcmp(name, PortTP.name))
        {
            IUUpdateText(&PortTP, texts, names, n);
            PortTP.s = IPS_OK;
            IDSetText(&PortTP, NULL);
            return true;
        }
    }

    return DefaultDevice::ISNewText(dev, name, texts, names, n);
}

bool INDI::Focuser::ISSnoopDevice (XMLEle *root)
{
    return INDI::DefaultDevice::ISSnoopDevice(root);
}

bool INDI::Focuser::saveConfigItems(FILE *fp)
{
    IUSaveConfigText(fp, &PortTP);
    IUSaveConfigNumber(fp, &PresetNP);

    return true;
}

