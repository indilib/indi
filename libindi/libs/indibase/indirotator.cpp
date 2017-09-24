/*******************************************************************************
  Copyright(c) 2017 Jasem Mutlaq. All rights reserved.

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

#include "indirotator.h"

#include "connectionplugins/connectionserial.h"
#include "connectionplugins/connectiontcp.h"

#include <cstring>

INDI::Rotator::Rotator()
{
}

INDI::Rotator::~Rotator()
{
}

bool INDI::Rotator::initProperties()
{
    DefaultDevice::initProperties();

    RotatorInterface::initProperties(this, MAIN_CONTROL_TAB);

    // Presets
    IUFillNumber(&PresetN[0], "PRESET_1", "Preset 1", "%.f", 0, 360, 10, 0);
    IUFillNumber(&PresetN[1], "PRESET_2", "Preset 2", "%.f", 0, 360, 10, 0);
    IUFillNumber(&PresetN[2], "PRESET_3", "Preset 3", "%.f", 0, 360, 10, 0);
    IUFillNumberVector(&PresetNP, PresetN, 3, getDeviceName(), "Presets", "", "Presets", IP_RW, 0, IPS_IDLE);

    //Preset GOTO
    IUFillSwitch(&PresetGotoS[0], "Preset 1", "", ISS_OFF);
    IUFillSwitch(&PresetGotoS[1], "Preset 2", "", ISS_OFF);
    IUFillSwitch(&PresetGotoS[2], "Preset 3", "", ISS_OFF);
    IUFillSwitchVector(&PresetGotoSP, PresetGotoS, 3, getDeviceName(), "Goto", "", "Presets", IP_RW, ISR_1OFMANY, 0,
                       IPS_IDLE);

    addDebugControl();

    setDriverInterface(ROTATOR_INTERFACE);

    if (rotatorConnection & CONNECTION_SERIAL)
    {
        serialConnection = new Connection::Serial(this);
        serialConnection->registerHandshake([&]() { return callHandshake(); });
        registerConnection(serialConnection);
    }

    if (rotatorConnection & CONNECTION_TCP)
    {
        tcpConnection = new Connection::TCP(this);
        tcpConnection->registerHandshake([&]() { return callHandshake(); });
        registerConnection(tcpConnection);
    }

    return true;
}

void INDI::Rotator::ISGetProperties(const char *dev)
{    
    DefaultDevice::ISGetProperties(dev);

    // If connected, let's define properties.
    if (isConnected())
        INDI::RotatorInterface::updateProperties(this);

    return;
}

bool INDI::Rotator::updateProperties()
{
    // #1 Update base device properties.
    INDI::DefaultDevice::updateProperties();

    // #2 Update rotator interface properties
    INDI::RotatorInterface::updateProperties(this);

    if (isConnected())
    {        
        defineNumber(&PresetNP);
        defineSwitch(&PresetGotoSP);
    }
    else
    {
        deleteProperty(PresetNP.name);
        deleteProperty(PresetGotoSP.name);
    }

    return true;
}

bool INDI::Rotator::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        if (!strcmp(name, PresetNP.name))
        {
            IUUpdateNumber(&PresetNP, values, names, n);
            PresetNP.s = IPS_OK;
            IDSetNumber(&PresetNP, nullptr);

            return true;
        }

        if (strstr(name, "ROTATOR"))
        {
          if (INDI::RotatorInterface::processNumber(dev, name, values, names, n))
          return true;
        }
    }

    return DefaultDevice::ISNewNumber(dev, name, values, names, n);
}

bool INDI::Rotator::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        if (!strcmp(PresetGotoSP.name, name))
        {
            IUUpdateSwitch(&PresetGotoSP, states, names, n);
            int index = IUFindOnSwitchIndex(&PresetGotoSP);

            if (MoveRotator(PresetN[index].value) != IPS_ALERT)
            {
                PresetGotoSP.s = IPS_OK;
                DEBUGF(INDI::Logger::DBG_SESSION, "Moving to Preset %d with angle %g degrees.", index + 1, PresetN[index].value);
                IDSetSwitch(&PresetGotoSP, nullptr);
                return true;
            }

            PresetGotoSP.s = IPS_ALERT;
            IDSetSwitch(&PresetGotoSP, nullptr);
            return false;
        }

        if (strstr(name, "ROTATOR"))
        {
            if (INDI::RotatorInterface::processSwitch(dev, name, states, names, n))
            return true;
        }
    }

    return DefaultDevice::ISNewSwitch(dev, name, states, names, n);
}

bool INDI::Rotator::Handshake()
{
    return false;
}

bool INDI::Rotator::saveConfigItems(FILE *fp)
{
    DefaultDevice::saveConfigItems(fp);

    IUSaveConfigNumber(fp, &PresetNP);
    IUSaveConfigSwitch(fp, &ReverseRotatorSP);

    return true;
}

bool INDI::Rotator::callHandshake()
{
    if (rotatorConnection > 0)
    {
        if (getActiveConnection() == serialConnection)
            PortFD = serialConnection->getPortFD();
        else if (getActiveConnection() == tcpConnection)
            PortFD = tcpConnection->getPortFD();
    }

    return Handshake();
}

uint8_t INDI::Rotator::getRotatorConnection() const
{
    return rotatorConnection;
}

void INDI::Rotator::setRotatorConnection(const uint8_t &value)
{
    uint8_t mask = CONNECTION_SERIAL | CONNECTION_TCP | CONNECTION_NONE;

    if (value == 0 || (mask & value) == 0)
    {
        DEBUGF(INDI::Logger::DBG_ERROR, "Invalid connection mode %d", value);
        return;
    }

    rotatorConnection = value;
}
