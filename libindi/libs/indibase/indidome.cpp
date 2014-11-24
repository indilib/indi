/*******************************************************************************
 INDI Dome Base Class
 Copyright(c) 2014 Jasem Mutlaq. All rights reserved.

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

#include "indidome.h"

#include <string.h>

INDI::Dome::Dome()
{
    controller = new INDI::Controller(this);

    controller->setButtonCallback(buttonHelper);
}

INDI::Dome::~Dome()
{
}

bool INDI::Dome::initProperties()
{
    DefaultDevice::initProperties();   //  let the base class flesh in what it wants

    initDomeProperties(getDeviceName(),  MAIN_CONTROL_TAB);

    /* Port */
    IUFillText(&PortT[0], "PORT", "Port", "/dev/ttyUSB0");
    IUFillTextVector(&PortTP, PortT, 1, getDeviceName(), "DEVICE_PORT", "Ports", MAIN_CONTROL_TAB, IP_RW, 0, IPS_IDLE);

    // Presets
    IUFillNumber(&PresetN[0], "Preset 1", "", "%6.2f", 0, 360.0, 1.0, 0);
    IUFillNumber(&PresetN[1], "Preset 2", "", "%6.2f", 0, 360.0, 1.0, 0);
    IUFillNumber(&PresetN[2], "Preset 3", "", "%6.2f", 0, 360.0, 1.0, 0);
    IUFillNumberVector(&PresetNP, PresetN, 3, getDeviceName(), "Presets", "", "Presets", IP_RW, 0, IPS_IDLE);

    //Preset GOTO
    IUFillSwitch(&PresetGotoS[0], "Preset 1", "", ISS_OFF);
    IUFillSwitch(&PresetGotoS[1], "Preset 2", "", ISS_OFF);
    IUFillSwitch(&PresetGotoS[2], "Preset 3", "", ISS_OFF);
    IUFillSwitchVector(&PresetGotoSP, PresetGotoS, 3, getDeviceName(), "Goto", "", "Presets", IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    IUFillText(&ActiveDeviceT[0],"ACTIVE_TELESCOPE","Telescope","Telescope Simulator");
    IUFillTextVector(&ActiveDeviceTP,ActiveDeviceT,1,getDeviceName(),"ACTIVE_DEVICES","Snoop devices",OPTIONS_TAB,IP_RW,60,IPS_IDLE);

    addDebugControl();

    controller->mapController("Dome CW", "Dome CW", INDI::Controller::CONTROLLER_BUTTON, "BUTTON_1");
    controller->mapController("Dome CCW", "Dome CCW", INDI::Controller::CONTROLLER_BUTTON, "BUTTON_2");

    controller->initProperties();

    IDSnoopDevice(ActiveDeviceT[0].text,"EQUATORIAL_EOD_COORD");
    IDSnoopDevice(ActiveDeviceT[0].text,"GEOGRAPHIC_COORD");

    return true;
}

void INDI::Dome::ISGetProperties (const char *dev)
{
    //  First we let our parent populate
    DefaultDevice::ISGetProperties(dev);

    defineText(&PortTP);

    controller->ISGetProperties(dev);
    return;
}

bool INDI::Dome::updateProperties()
{
    if(isConnected())
    {
        defineText(&ActiveDeviceTP);

        if (capability.hasShutter)
            defineSwitch(&DomeShutterSP);

        //  Now we add our Dome specific stuff
        defineSwitch(&DomeMotionSP);

        if (capability.variableSpeed)
        {
            defineNumber(&DomeSpeedNP);
            defineNumber(&DomeTimerNP);
        }
        if (capability.canRelMove)
            defineNumber(&DomeRelPosNP);
        if (capability.canAbsMove)
            defineNumber(&DomeAbsPosNP);
        if (capability.canAbort)
            defineSwitch(&AbortSP);
        if (capability.canAbsMove)
        {
            defineNumber(&PresetNP);
            defineSwitch(&PresetGotoSP);
        }
        if (capability.canPark)
        {
            defineNumber(&DomeParamNP);
            defineSwitch(&DomeGotoSP);
        }
    } else
    {
        deleteProperty(ActiveDeviceTP.name);

        if (capability.hasShutter)
            deleteProperty(DomeShutterSP.name);

        deleteProperty(DomeMotionSP.name);
        if (capability.variableSpeed)
        {
            deleteProperty(DomeSpeedNP.name);
            deleteProperty(DomeTimerNP.name);
        }
        if (capability.canRelMove)
            deleteProperty(DomeRelPosNP.name);
        if (capability.canAbsMove)
            deleteProperty(DomeAbsPosNP.name);
        if (capability.canAbort)
            deleteProperty(AbortSP.name);
        if (capability.canAbsMove)
        {
            deleteProperty(PresetNP.name);
            deleteProperty(PresetGotoSP.name);
        }
        if (capability.canPark)
        {
            deleteProperty(DomeParamNP.name);
            deleteProperty(DomeGotoSP.name);
        }

    }

    controller->updateProperties();
    return true;
}


bool INDI::Dome::ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n)
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

        if (strstr(name, "DOME_"))
            return processDomeNumber(dev, name, values, names, n);

    }

    return DefaultDevice::ISNewNumber(dev,name,values,names,n);
}

bool INDI::Dome::ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if(strcmp(dev,getDeviceName())==0)
    {
        if (!strcmp(PresetGotoSP.name, name))
        {
            IUUpdateSwitch(&PresetGotoSP, states, names, n);
            int index = IUFindOnSwitchIndex(&PresetGotoSP);
            int rc = MoveAbsDome(PresetN[index].value);
            if (rc >= 0)
            {
                PresetGotoSP.s = IPS_OK;
                DEBUGF(INDI::Logger::DBG_SESSION, "Moving to Preset %d (%g degrees).", index+1, PresetN[index].value);
                IDSetSwitch(&PresetGotoSP, NULL);

                if (rc == 1)
                {
                    DomeAbsPosNP.s = IPS_BUSY;
                    IDSetNumber(&DomeAbsPosNP, NULL);
                }
                return true;
            }

            PresetGotoSP.s = IPS_ALERT;
            IDSetSwitch(&PresetGotoSP, NULL);
            return false;
        }

        if (strstr(name, "DOME_"))
            return processDomeSwitch(dev, name, states, names, n);

    }

    controller->ISNewSwitch(dev, name, states, names, n);

    //  Nobody has claimed this, so, ignore it
    return DefaultDevice::ISNewSwitch(dev,name,states,names,n);
}

bool INDI::Dome::ISNewText (const char *dev, const char *name, char *texts[], char *names[], int n)
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

        if(strcmp(name,ActiveDeviceTP.name)==0)
        {
            int rc;
            ActiveDeviceTP.s=IPS_OK;
            rc=IUUpdateText(&ActiveDeviceTP,texts,names,n);
            //  Update client display
            IDSetText(&ActiveDeviceTP,NULL);
            //saveConfig();

            // Update the property name!
            //strncpy(EqNP.device, ActiveDeviceT[0].text, MAXINDIDEVICE);
            IDSnoopDevice(ActiveDeviceT[0].text,"EQUATORIAL_EOD_COORD");
            IDSnoopDevice(ActiveDeviceT[0].text,"GEOGRAPHIC_COORD");
            //  We processed this one, so, tell the world we did it
            return true;
        }
    }

    controller->ISNewText(dev, name, texts, names, n);

    return DefaultDevice::ISNewText(dev, name, texts, names, n);
}

bool INDI::Dome::ISSnoopDevice (XMLEle *root)
{
    XMLEle *ep=NULL;
    const char *propName = findXMLAttValu(root, "name");

    // Check EOD
    if (!strcmp("EQUATORIAL_EOD_COORD", propName))
    {
        for (ep = nextXMLEle(root, 1) ; ep != NULL ; ep = nextXMLEle(root, 0))
        {
            const char *elemName = findXMLAttValu(ep, "name");
            if (!strcmp(elemName, "RA"))
                equ.ra = atof(pcdataXMLEle(ep))*15.0;
            else if (!strcmp(elemName, "DEC"))
                equ.dec = atof(pcdataXMLEle(ep));

            DEBUGF(INDI::Logger::DBG_DEBUG, "Snooped RA: %g - DEC: %g", equ.ra, equ.dec);
        }

        return true;
     }

    // Check Geographic coords
    if (!strcmp("GEOGRAPHIC_COORD", propName))
    {
        for (ep = nextXMLEle(root, 1) ; ep != NULL ; ep = nextXMLEle(root, 0))
        {
            const char *elemName = findXMLAttValu(ep, "name");
            if (!strcmp(elemName, "LONG"))
                observer.lng = atof(pcdataXMLEle(ep));
            else if (!strcmp(elemName, "LAT"))
                observer.lat = atof(pcdataXMLEle(ep));
        }

        DEBUGF(INDI::Logger::DBG_DEBUG, "Snooped LONG: %g - LAT: %g", observer.lng, observer.lat);

        return true;
     }

    controller->ISSnoopDevice(root);

    return INDI::DefaultDevice::ISSnoopDevice(root);
}

bool INDI::Dome::saveConfigItems(FILE *fp)
{
    IUSaveConfigText(fp, &ActiveDeviceTP);
    IUSaveConfigText(fp, &PortTP);
    IUSaveConfigNumber(fp, &PresetNP);
    IUSaveConfigNumber(fp, &DomeParamNP);

    controller->saveConfigItems(fp);

    return true;
}

void INDI::Dome::buttonHelper(const char *button_n, ISState state, void *context)
{
     static_cast<INDI::Dome *>(context)->processButton(button_n, state);
}

void INDI::Dome::processButton(const char * button_n, ISState state)
{
    //ignore OFF
    if (state == ISS_OFF)
        return;

    int rc=0;
    // Dome In
    if (!strcmp(button_n, "Dome CW"))
    {
        if (capability.variableSpeed)
        {
           rc = MoveDome(DOME_CW, DomeSpeedN[0].value, DomeTimerN[0].value);
            if (rc == 0)
                DomeTimerNP.s = IPS_OK;
            else if (rc == 1)
                DomeTimerNP.s = IPS_BUSY;
            else
                DomeTimerNP.s = IPS_ALERT;

            IDSetNumber(&DomeTimerNP,NULL);
        }
        else if (capability.canRelMove)
        {
            rc=MoveRelDome(DOME_CW, DomeRelPosN[0].value);
            if (rc == 0)
            {
               DomeRelPosNP.s=IPS_OK;
               IDSetNumber(&DomeRelPosNP, "Dome moved %g degrees CW", DomeRelPosN[0].value);
               IDSetNumber(&DomeAbsPosNP, NULL);
            }
            else if (rc == 1)
            {
                 DomeRelPosNP.s=IPS_BUSY;
                 IDSetNumber(&DomeAbsPosNP, "Dome is moving %g degrees CW...", DomeRelPosN[0].value);
            }
        }
    }
    else if (!strcmp(button_n, "Dome CCW"))
    {
        if (capability.variableSpeed)
        {
           rc = MoveDome(DOME_CCW, DomeSpeedN[0].value, DomeTimerN[0].value);
            if (rc == 0)
                DomeTimerNP.s = IPS_OK;
            else if (rc == 1)
                DomeTimerNP.s = IPS_BUSY;
            else
                DomeTimerNP.s = IPS_ALERT;

            IDSetNumber(&DomeTimerNP,NULL);
        }
        else if (capability.canRelMove)
        {
            rc=MoveRelDome(DOME_CCW, DomeRelPosN[0].value);
            if (rc == 0)
            {
               DomeRelPosNP.s=IPS_OK;
               IDSetNumber(&DomeRelPosNP, "Dome moved %g degrees CCW", DomeRelPosN[0].value);
               IDSetNumber(&DomeAbsPosNP, NULL);
            }
            else if (rc == 1)
            {
                 DomeRelPosNP.s=IPS_BUSY;
                 IDSetNumber(&DomeAbsPosNP, "Dome is moving %g degrees CCW...", DomeRelPosN[0].value);
            }
        }
    }
}
