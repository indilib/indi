/*
    Giotto driver
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

#include "giotto.h"

#include <memory>
#include <unistd.h>
#include <connectionplugins/connectionserial.h>

static std::unique_ptr<GIOTTO> sesto(new GIOTTO());

GIOTTO::GIOTTO() : LightBoxInterface(this, true)
{
    setVersion(1, 0);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////////
bool GIOTTO::initProperties()
{

    INDI::DefaultDevice::initProperties();

    initLightBoxProperties(getDeviceName(), MAIN_CONTROL_TAB);

    setDriverInterface(AUX_INTERFACE | LIGHTBOX_INTERFACE);

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
bool GIOTTO::updateProperties()
{
    INDI::DefaultDevice::updateProperties();

    if (isConnected())
    {
        defineProperty(&LightSP);
        defineProperty(&LightIntensityNP);
        updateLightBoxProperties();
    }
    else
    {
        deleteProperty(LightSP.name);
        deleteProperty(LightIntensityNP.name);
    }

    return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////////
void GIOTTO::ISGetProperties(const char *dev)
{
    INDI::DefaultDevice::ISGetProperties(dev);

    // Get Light box properties
    isGetLightBoxProperties(dev);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////////
bool GIOTTO::Handshake()
{
    PortFD = serialConnection->getPortFD();
    m_GIOTTO.reset(new PrimalucaLabs::GIOTTO(getDeviceName(), PortFD));
    uint16_t value = 0;
    if (m_GIOTTO->getBrightness(value))
    {
        LOGF_INFO("%s is online.", getDeviceName());

        LightIntensityNP.np[0].value = value;

        auto lightEnabled = m_GIOTTO->isLightEnabled();
        LightS[0].s = lightEnabled ? ISS_ON : ISS_OFF;
        LightS[1].s = lightEnabled ? ISS_OFF : ISS_ON;

        return true;
    }

    LOG_INFO("Error retrieving data from device, please ensure GIOTTO is powered and the port is correct.");
    return false;
}


////////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////////
const char *GIOTTO::getDefaultName()
{
    return "GIOTTO";
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////////
bool GIOTTO::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    if (processLightBoxNumber(dev, name, values, names, n))
        return true;

    return INDI::DefaultDevice::ISNewNumber(dev, name, values, names, n);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////////
bool GIOTTO::ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n)
{
    if (processLightBoxText(dev, name, texts, names, n))
        return true;

    return INDI::DefaultDevice::ISNewText(dev, name, texts, names, n);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////////
bool GIOTTO::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if (processLightBoxSwitch(dev, name, states, names, n))
        return true;

    return INDI::DefaultDevice::ISNewSwitch(dev, name, states, names, n);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////////
bool GIOTTO::ISSnoopDevice(XMLEle *root)
{
    snoopLightBox(root);

    return INDI::DefaultDevice::ISSnoopDevice(root);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////////
bool GIOTTO::SetLightBoxBrightness(uint16_t value)
{
    return m_GIOTTO->setBrightness(value);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////////
bool GIOTTO::EnableLightBox(bool enable)
{
    return m_GIOTTO->setLightEnabled(enable);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////////
bool GIOTTO::saveConfigItems(FILE *fp)
{
    INDI::DefaultDevice::saveConfigItems(fp);

    return saveLightBoxConfigItems(fp);
}
