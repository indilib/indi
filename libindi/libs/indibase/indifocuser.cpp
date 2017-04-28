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
#include "connectionplugins/connectionserial.h"
#include "connectionplugins/connectiontcp.h"

#include <string.h>

INDI::Focuser::Focuser()
{
    controller = new INDI::Controller(this);

    controller->setButtonCallback(buttonHelper);
}

INDI::Focuser::~Focuser()
{
}

bool INDI::Focuser::initProperties()
{
    DefaultDevice::initProperties();   //  let the base class flesh in what it wants

    initFocuserProperties(getDeviceName(),  MAIN_CONTROL_TAB);

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

    controller->mapController("Focus In", "Focus In", INDI::Controller::CONTROLLER_BUTTON, "BUTTON_1");
    controller->mapController("Focus Out", "Focus Out", INDI::Controller::CONTROLLER_BUTTON, "BUTTON_2");
    controller->mapController("Abort Focus", "Abort Focus", INDI::Controller::CONTROLLER_BUTTON, "BUTTON_3");

    controller->initProperties();

    setDriverInterface(FOCUSER_INTERFACE);

    if (focuserConnection & CONNECTION_SERIAL)
    {
        serialConnection = new Connection::Serial(this);
        serialConnection->registerHandshake([&]()
        {
            return callHandshake();
        });
        registerConnection(serialConnection);
    }

    if (focuserConnection & CONNECTION_TCP)
    {
        tcpConnection = new Connection::TCP(this);
        tcpConnection->registerHandshake([&]()
        {
            return callHandshake();
        });
        registerConnection(tcpConnection);
    }

    return true;
}

void INDI::Focuser::ISGetProperties (const char * dev)
{
    //  First we let our parent populate
    DefaultDevice::ISGetProperties(dev);

    controller->ISGetProperties(dev);
    return;
}

bool INDI::Focuser::updateProperties()
{
    if(isConnected())
    {
        //  Now we add our focusser specific stuff
        defineSwitch(&FocusMotionSP);

        if (HasVariableSpeed())
        {
            defineNumber(&FocusSpeedNP);
            defineNumber(&FocusTimerNP);
        }
        if (CanRelMove())
            defineNumber(&FocusRelPosNP);
        if (CanAbsMove())
            defineNumber(&FocusAbsPosNP);
        if (CanAbort())
            defineSwitch(&AbortSP);
        if (CanAbsMove())
        {
            defineNumber(&PresetNP);
            defineSwitch(&PresetGotoSP);
        }
    }
    else
    {
        deleteProperty(FocusMotionSP.name);
        if (HasVariableSpeed())
        {
            deleteProperty(FocusSpeedNP.name);
            deleteProperty(FocusTimerNP.name);
        }
        if (CanRelMove())
            deleteProperty(FocusRelPosNP.name);
        if (CanAbsMove())
            deleteProperty(FocusAbsPosNP.name);
        if (CanAbort())
            deleteProperty(AbortSP.name);
        if (CanAbsMove())
        {
            deleteProperty(PresetNP.name);
            deleteProperty(PresetGotoSP.name);
        }
    }

    controller->updateProperties();
    return true;
}


bool INDI::Focuser::ISNewNumber (const char * dev, const char * name, double values[], char * names[], int n)
{
    //  first check if it's for our device
    if(strcmp(dev,getDeviceName())==0)
    {
        if (!strcmp(name, PresetNP.name))
        {
            IUUpdateNumber(&PresetNP, values, names, n);
            PresetNP.s = IPS_OK;
            IDSetNumber(&PresetNP, NULL);

            //saveConfig();

            return true;
        }

        if (strstr(name, "FOCUS_"))
            return processFocuserNumber(dev, name, values, names, n);

    }

    return DefaultDevice::ISNewNumber(dev,name,values,names,n);
}

bool INDI::Focuser::ISNewSwitch (const char * dev, const char * name, ISState * states, char * names[], int n)
{
    if(strcmp(dev,getDeviceName())==0)
    {
        if (!strcmp(PresetGotoSP.name, name))
        {
            IUUpdateSwitch(&PresetGotoSP, states, names, n);
            int index = IUFindOnSwitchIndex(&PresetGotoSP);

            if (PresetN[index].value < FocusAbsPosN[0].min)
            {
                PresetGotoSP.s = IPS_ALERT;
                IDSetSwitch(&PresetGotoSP, NULL);
                DEBUGFDEVICE(dev, INDI::Logger::DBG_ERROR, "Requested position out of bound. Focus minimum position is %g", FocusAbsPosN[0].min);
                return false;
            }
            else if (PresetN[index].value > FocusAbsPosN[0].max)
            {
                PresetGotoSP.s = IPS_ALERT;
                IDSetSwitch(&PresetGotoSP, NULL);
                DEBUGFDEVICE(dev, INDI::Logger::DBG_ERROR, "Requested position out of bound. Focus maximum position is %g", FocusAbsPosN[0].max);
                return false;
            }

            int rc = MoveAbsFocuser(PresetN[index].value);
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

    controller->ISNewSwitch(dev, name, states, names, n);

    //  Nobody has claimed this, so, ignore it
    return DefaultDevice::ISNewSwitch(dev,name,states,names,n);
}

bool INDI::Focuser::ISNewText (const char * dev, const char * name, char * texts[], char * names[], int n)
{
    controller->ISNewText(dev, name, texts, names, n);

    return DefaultDevice::ISNewText(dev, name, texts, names, n);
}

bool INDI::Focuser::ISSnoopDevice (XMLEle * root)
{
    controller->ISSnoopDevice(root);

    return INDI::DefaultDevice::ISSnoopDevice(root);
}

bool INDI::Focuser::Handshake()
{
    return false;
}

bool INDI::Focuser::saveConfigItems(FILE * fp)
{
    DefaultDevice::saveConfigItems(fp);

    IUSaveConfigNumber(fp, &PresetNP);

    controller->saveConfigItems(fp);

    return true;
}

void INDI::Focuser::buttonHelper(const char * button_n, ISState state, void * context)
{
    static_cast<INDI::Focuser *>(context)->processButton(button_n, state);
}

void INDI::Focuser::processButton(const char * button_n, ISState state)
{
    //ignore OFF
    if (state == ISS_OFF)
        return;

    FocusTimerN[0].value = lastTimerValue;

    IPState rc= IPS_IDLE;

    // Abort
    if (!strcmp(button_n, "Abort Focus"))
    {
        if (AbortFocuser())
        {
            AbortSP.s = IPS_OK;
            DEBUG(INDI::Logger::DBG_SESSION, "Focuser aborted.");
            if (CanAbsMove() && FocusAbsPosNP.s != IPS_IDLE)
            {
                FocusAbsPosNP.s = IPS_IDLE;
                IDSetNumber(&FocusAbsPosNP, NULL);
            }
            if (CanRelMove() && FocusRelPosNP.s != IPS_IDLE)
            {
                FocusRelPosNP.s = IPS_IDLE;
                IDSetNumber(&FocusRelPosNP, NULL);
            }
        }
        else
        {
            AbortSP.s = IPS_ALERT;
            DEBUG(INDI::Logger::DBG_ERROR, "Aborting focuser failed.");
        }

        IDSetSwitch(&AbortSP, NULL);
    }
    // Focus In
    else if (!strcmp(button_n, "Focus In"))
    {
        if (FocusMotionS[FOCUS_INWARD].s != ISS_ON)
        {
            FocusMotionS[FOCUS_INWARD].s = ISS_ON;
            FocusMotionS[FOCUS_OUTWARD].s = ISS_OFF;
            IDSetSwitch(&FocusMotionSP, NULL);
        }

        if (HasVariableSpeed())
        {
            rc = MoveFocuser(FOCUS_INWARD, FocusSpeedN[0].value, FocusTimerN[0].value);
            FocusTimerNP.s = rc;
            IDSetNumber(&FocusTimerNP,NULL);
        }
        else if (CanRelMove())
        {
            rc=MoveRelFocuser(FOCUS_INWARD, FocusRelPosN[0].value);
            if (rc == IPS_OK)
            {
                FocusRelPosNP.s=IPS_OK;
                IDSetNumber(&FocusRelPosNP, "Focuser moved %d steps inward", (int) FocusRelPosN[0].value);
                IDSetNumber(&FocusAbsPosNP, NULL);
            }
            else if (rc == IPS_BUSY)
            {
                FocusRelPosNP.s=IPS_BUSY;
                IDSetNumber(&FocusAbsPosNP, "Focuser is moving %d steps inward...", (int) FocusRelPosN[0].value);
            }
        }
    }
    else if (!strcmp(button_n, "Focus Out"))
    {
        if (FocusMotionS[FOCUS_OUTWARD].s != ISS_ON)
        {
            FocusMotionS[FOCUS_INWARD].s = ISS_OFF;
            FocusMotionS[FOCUS_OUTWARD].s = ISS_ON;
            IDSetSwitch(&FocusMotionSP, NULL);
        }

        if (HasVariableSpeed())
        {
            rc = MoveFocuser(FOCUS_OUTWARD, FocusSpeedN[0].value, FocusTimerN[0].value);
            FocusTimerNP.s = rc;
            IDSetNumber(&FocusTimerNP,NULL);
        }
        else if (CanRelMove())
        {
            rc=MoveRelFocuser(FOCUS_OUTWARD, FocusRelPosN[0].value);
            if (rc == IPS_OK)
            {
                FocusRelPosNP.s=IPS_OK;
                IDSetNumber(&FocusRelPosNP, "Focuser moved %d steps outward", (int) FocusRelPosN[0].value);
                IDSetNumber(&FocusAbsPosNP, NULL);
            }
            else if (rc == IPS_BUSY)
            {
                FocusRelPosNP.s=IPS_BUSY;
                IDSetNumber(&FocusAbsPosNP, "Focuser is moving %d steps outward...", (int) FocusRelPosN[0].value);
            }
        }
    }
}

bool INDI::Focuser::callHandshake()
{
    if (focuserConnection > 0)
    {
        if (getActiveConnection() == serialConnection)
            PortFD = serialConnection->getPortFD();
        else if (getActiveConnection() == tcpConnection)
            PortFD = tcpConnection->getPortFD();
    }

    return Handshake();
}

uint8_t INDI::Focuser::getFocuserConnection() const
{
    return focuserConnection;
}

void INDI::Focuser::setFocuserConnection(const uint8_t &value)
{
    uint8_t mask = CONNECTION_SERIAL | CONNECTION_TCP | CONNECTION_NONE;

    if (value == 0 || (mask & value) == 0)
    {
        DEBUGF(INDI::Logger::DBG_ERROR, "Invalid connection mode %d", value);
        return;
    }

    focuserConnection = value;
}
