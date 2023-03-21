/*
    ALTO driver
    Copyright (C) 2023 Jasem Mutlaq

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

#include "alto.h"

#include <memory>
#include <unistd.h>
#include <connectionplugins/connectionserial.h>

static std::unique_ptr<ALTO> sesto(new ALTO());

ALTO::ALTO() : DustCapInterface()
{
    setVersion(1, 0);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////////
bool ALTO::initProperties()
{

    INDI::DefaultDevice::initProperties();

    initDustCapProperties(getDeviceName(), MAIN_CONTROL_TAB);

    setDriverInterface(AUX_INTERFACE | DUSTCAP_INTERFACE);

    addAuxControls();

    serialConnection = new Connection::Serial(this);
    serialConnection->registerHandshake([&]()
    {
        return Handshake();
    });
    registerConnection(serialConnection);

    return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////////
bool ALTO::updateProperties()
{
    INDI::DefaultDevice::updateProperties();
    updateDustCapProperties();

    if (isConnected())
    {
        
    }
    else
    {
        
    }

    return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////////
bool ALTO::Handshake()
{
    PortFD = serialConnection->getPortFD();
    m_ALTO.reset(new PrimalucaLabs::ALTO(getDeviceName(), PortFD));
    json status;
    if (m_ALTO->getStatus(status))
    {
        LOGF_INFO("%s is online. Detected model %s", getDeviceName(), status["MODNAME"]);

        return true;
    }

    LOG_INFO("Error retrieving data from device, please ensure ALTO is powered and the port is correct.");
    return false;
}


////////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////////
const char *ALTO::getDefaultName()
{
    return "ALTO";
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////////
bool ALTO::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{    
    return INDI::DefaultDevice::ISNewNumber(dev, name, values, names, n);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////////
bool ALTO::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if (processDustCapSwitch(dev, name, states, names, n))
        return true;

    return INDI::DefaultDevice::ISNewSwitch(dev, name, states, names, n);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////////
IPState ALTO::ParkCap()
{
    return m_ALTO->Park() ? IPS_BUSY : IPS_ALERT;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////////
IPState ALTO::UnParkCap()
{
    return m_ALTO->UnPark() ? IPS_BUSY : IPS_ALERT;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////////
bool ALTO::saveConfigItems(FILE *fp)
{
    INDI::DefaultDevice::saveConfigItems(fp);    
}
