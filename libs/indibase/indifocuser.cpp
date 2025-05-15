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

#include "indicontroller.h"
#include "connectionplugins/connectionserial.h"
#include "connectionplugins/connectiontcp.h"

#include <cstring>

namespace INDI
{

Focuser::Focuser() : FI(this)
{
    controller = new Controller(this);

    controller->setButtonCallback(buttonHelper);
}

Focuser::~Focuser()
{
    delete (controller);
}

bool Focuser::initProperties()
{
    DefaultDevice::initProperties(); //  let the base class flesh in what it wants

    FI::initProperties(MAIN_CONTROL_TAB);

    // Presets
    PresetNP[0].fill("PRESET_1", "Preset 1", "%.f", 0, 100000, 1000, 0);
    PresetNP[1].fill("PRESET_2", "Preset 2", "%.f", 0, 100000, 1000, 0);
    PresetNP[2].fill("PRESET_3", "Preset 3", "%.f", 0, 100000, 1000, 0);
    PresetNP.fill(getDeviceName(), "Presets", "", "Presets", IP_RW, 0, IPS_IDLE);

    //Preset GOTO
    PresetGotoSP[0].fill("Preset 1", "", ISS_OFF);
    PresetGotoSP[1].fill("Preset 2", "", ISS_OFF);
    PresetGotoSP[2].fill("Preset 3", "", ISS_OFF);
    PresetGotoSP.fill(getDeviceName(), "Goto", "", "Presets", IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    addDebugControl();
    addPollPeriodControl();

    controller->mapController("Focus In", "Focus In", Controller::CONTROLLER_BUTTON, "BUTTON_1");
    controller->mapController("Focus Out", "Focus Out", Controller::CONTROLLER_BUTTON, "BUTTON_2");
    controller->mapController("Abort Focus", "Abort Focus", Controller::CONTROLLER_BUTTON, "BUTTON_3");

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

void Focuser::ISGetProperties(const char *dev)
{
    //  First we let our parent populate
    DefaultDevice::ISGetProperties(dev);

    controller->ISGetProperties(dev);
    return;
}

bool Focuser::updateProperties()
{
    FI::updateProperties();

    if (isConnected())
    {
        if (CanAbsMove())
        {
            defineProperty(PresetNP);
            defineProperty(PresetGotoSP);
        }
    }
    else
    {
        if (CanAbsMove())
        {
            deleteProperty(PresetNP);
            deleteProperty(PresetGotoSP);
        }
    }

    controller->updateProperties();
    return true;
}

bool Focuser::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    if (FI::processNumber(dev, name, values, names, n))
        return true;

    //  first check if it's for our device
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        if (PresetNP.isNameMatch(name))
        {
            PresetNP.update(values, names, n);
            PresetNP.setState(IPS_OK);
            PresetNP.apply();

            saveConfig(PresetNP);
            return true;
        }
    }

    return DefaultDevice::ISNewNumber(dev, name, values, names, n);
}

bool Focuser::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if (FI::processSwitch(dev, name, states, names, n))
        return true;

    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        if (PresetGotoSP.isNameMatch(name))
        {
            PresetGotoSP.update(states, names, n);
            int index = PresetGotoSP.findOnSwitchIndex();

            if (PresetNP[index].getValue() < FocusAbsPosNP[0].getMin())
            {
                PresetGotoSP.setState(IPS_ALERT);
                PresetGotoSP.apply();
                DEBUGFDEVICE(dev, Logger::DBG_ERROR,
                             "Requested position out of bound. Focus minimum position is %g", FocusAbsPosNP[0].getMin());
                return true;
            }
            else if (PresetNP[index].getValue() > FocusAbsPosNP[0].getMax())
            {
                PresetGotoSP.setState(IPS_ALERT);
                PresetGotoSP.apply();
                DEBUGFDEVICE(dev, Logger::DBG_ERROR,
                             "Requested position out of bound. Focus maximum position is %g", FocusAbsPosNP[0].getMax());
                return true;
            }

            IPState rc = MoveAbsFocuser(PresetNP[index].getValue());
            if (rc != IPS_ALERT)
            {
                PresetGotoSP.setState(IPS_OK);
                DEBUGF(Logger::DBG_SESSION, "Moving to Preset %d with position %g.", index + 1,
                       PresetNP[index].getValue());
                PresetGotoSP.apply();

                FocusAbsPosNP.setState(IPS_BUSY);
                FocusAbsPosNP.apply();
                return true;
            }

            PresetGotoSP.setState(IPS_ALERT);
            PresetGotoSP.apply();
            return true;
        }
    }

    controller->ISNewSwitch(dev, name, states, names, n);
    return DefaultDevice::ISNewSwitch(dev, name, states, names, n);
}

bool Focuser::ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n)
{
    controller->ISNewText(dev, name, texts, names, n);

    return DefaultDevice::ISNewText(dev, name, texts, names, n);
}

bool Focuser::ISSnoopDevice(XMLEle *root)
{
    controller->ISSnoopDevice(root);

    return DefaultDevice::ISSnoopDevice(root);
}

bool Focuser::Handshake()
{
    return false;
}

bool Focuser::saveConfigItems(FILE *fp)
{
    DefaultDevice::saveConfigItems(fp);

    FI::saveConfigItems(fp);

    PresetNP.save(fp);
    controller->saveConfigItems(fp);

    return true;
}

void Focuser::buttonHelper(const char *button_n, ISState state, void *context)
{
    static_cast<Focuser *>(context)->processButton(button_n, state);
}

void Focuser::processButton(const char *button_n, ISState state)
{
    //ignore OFF
    if (state == ISS_OFF)
        return;

    FocusTimerNP[0].setValue(lastTimerValue);

    IPState rc = IPS_IDLE;

    // Abort
    if (!strcmp(button_n, "Abort Focus"))
    {
        if (AbortFocuser())
        {
            FocusAbortSP.setState(IPS_OK);
            DEBUG(Logger::DBG_SESSION, "Focuser aborted.");
            if (CanAbsMove() && FocusAbsPosNP.getState() != IPS_IDLE)
            {
                FocusAbsPosNP.setState(IPS_IDLE);
                FocusAbsPosNP.apply();
            }
            if (CanRelMove() && FocusRelPosNP.getState() != IPS_IDLE)
            {
                FocusRelPosNP.setState(IPS_IDLE);
                FocusRelPosNP.apply();
            }
        }
        else
        {
            FocusAbortSP.setState(IPS_ALERT);
            DEBUG(Logger::DBG_ERROR, "Aborting focuser failed.");
        }

        FocusAbortSP.apply();
    }
    // Focus In
    else if (!strcmp(button_n, "Focus In"))
    {
        if (FocusMotionSP[FOCUS_INWARD].getState() != ISS_ON)
        {
            FocusMotionSP[FOCUS_INWARD].setState(ISS_ON);
            FocusMotionSP[FOCUS_OUTWARD].setState(ISS_OFF);
            FocusMotionSP.apply();
        }

        if (CanRelMove())
        {
            rc = MoveRelFocuser(FOCUS_INWARD, FocusRelPosNP[0].getValue());
            if (rc == IPS_OK)
            {
                FocusRelPosNP.setState(IPS_OK);
                FocusRelPosNP.apply("Focuser moved %d steps inward", (int)FocusRelPosNP[0].getValue());
                FocusAbsPosNP.apply();
            }
            else if (rc == IPS_BUSY)
            {
                FocusRelPosNP.setState(IPS_BUSY);
                FocusAbsPosNP.apply("Focuser is moving %d steps inward...", (int)FocusRelPosNP[0].getValue());
            }
        }
        else if (HasVariableSpeed())
        {
            rc = MoveFocuser(FOCUS_INWARD, FocusSpeedNP[0].getValue(), FocusTimerNP[0].getValue());
            FocusTimerNP.setState(rc);
            FocusTimerNP.apply();
        }

    }
    else if (!strcmp(button_n, "Focus Out"))
    {
        if (FocusMotionSP[FOCUS_OUTWARD].getState() != ISS_ON)
        {
            FocusMotionSP[FOCUS_INWARD].setState(ISS_OFF);
            FocusMotionSP[FOCUS_OUTWARD].setState(ISS_ON);
            FocusMotionSP.apply();
        }

        if (CanRelMove())
        {
            rc = MoveRelFocuser(FOCUS_OUTWARD, FocusRelPosNP[0].getValue());
            if (rc == IPS_OK)
            {
                FocusRelPosNP.setState(IPS_OK);
                FocusRelPosNP.apply("Focuser moved %d steps outward", (int)FocusRelPosNP[0].getValue());
                FocusAbsPosNP.apply();
            }
            else if (rc == IPS_BUSY)
            {
                FocusRelPosNP.setState(IPS_BUSY);
                FocusAbsPosNP.apply("Focuser is moving %d steps outward...", (int)FocusRelPosNP[0].getValue());
            }
        }
        else if (HasVariableSpeed())
        {
            rc = MoveFocuser(FOCUS_OUTWARD, FocusSpeedNP[0].getValue(), FocusTimerNP[0].getValue());
            FocusTimerNP.setState(rc);
            FocusTimerNP.apply();
        }
    }
}

bool Focuser::callHandshake()
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

void Focuser::setSupportedConnections(const uint8_t &value)
{
    uint8_t mask = CONNECTION_SERIAL | CONNECTION_TCP | CONNECTION_NONE;

    if (value == 0 || (mask & value) == 0)
    {
        DEBUGF(Logger::DBG_ERROR, "Invalid connection mode %d", value);
        return;
    }

    focuserConnection = value;
}

bool Focuser::SetFocuserMaxPosition(uint32_t ticks)
{
    SyncPresets(ticks);
    return true;
}

void Focuser::SyncPresets(uint32_t ticks)
{
    PresetNP[0].setMax(ticks);
    PresetNP[0].setStep(PresetNP[0].getMax() / 50.0);
    PresetNP[1].setMax(ticks);
    PresetNP[1].setStep(PresetNP[0].getMax() / 50.0);
    PresetNP[2].setMax(ticks);
    PresetNP[2].setStep(PresetNP[0].getMax() / 50.0);
    PresetNP.updateMinMax();
}
}
