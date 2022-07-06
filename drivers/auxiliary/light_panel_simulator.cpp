/*******************************************************************************
  Light Panel Simulator

  SPDX-FileCopyrightText: 2021 Jasem Mutlaq
  SPDX-License-Identifier: LGPL-2.0-or-later
*******************************************************************************/

#include "light_panel_simulator.h"

static std::unique_ptr<LightPanelSimulator> simulator(new LightPanelSimulator());

LightPanelSimulator::LightPanelSimulator() : LightBoxInterface(this, true)
{
}

void LightPanelSimulator::ISGetProperties(const char *dev)
{
    INDI::DefaultDevice::ISGetProperties(dev);

    // Get Light box properties
    isGetLightBoxProperties(dev);
}

bool LightPanelSimulator::initProperties()
{
    INDI::DefaultDevice::initProperties();
    setDriverInterface(AUX_INTERFACE | LIGHTBOX_INTERFACE);
    initLightBoxProperties(getDeviceName(), MAIN_CONTROL_TAB);
    addAuxControls();
    return true;
}

bool LightPanelSimulator::updateProperties()
{
    INDI::DefaultDevice::updateProperties();

    if (isConnected())
    {
        defineProperty(&LightSP);
        defineProperty(&LightIntensityNP);
    }
    else
    {
        deleteProperty(LightSP.name);
        deleteProperty(LightIntensityNP.name);
    }

    updateLightBoxProperties();
    return true;
}

bool LightPanelSimulator::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    if (processLightBoxNumber(dev, name, values, names, n))
        return true;

    return INDI::DefaultDevice::ISNewNumber(dev, name, values, names, n);
}

bool LightPanelSimulator::ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        if (processLightBoxText(dev, name, texts, names, n))
            return true;
    }

    return INDI::DefaultDevice::ISNewText(dev, name, texts, names, n);
}

bool LightPanelSimulator::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        if (processLightBoxSwitch(dev, name, states, names, n))
            return true;
    }

    return INDI::DefaultDevice::ISNewSwitch(dev, name, states, names, n);
}

bool LightPanelSimulator::SetLightBoxBrightness(uint16_t value)
{
    INDI_UNUSED(value);
    return true;
}

bool LightPanelSimulator::EnableLightBox(bool enable)
{
    INDI_UNUSED(enable);
    return true;
}
