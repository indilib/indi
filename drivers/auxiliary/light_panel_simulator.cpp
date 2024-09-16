/*******************************************************************************
  Light Panel Simulator

  SPDX-FileCopyrightText: 2021 Jasem Mutlaq
  SPDX-License-Identifier: LGPL-2.0-or-later
*******************************************************************************/

#include "light_panel_simulator.h"

static std::unique_ptr<LightPanelSimulator> simulator(new LightPanelSimulator());

LightPanelSimulator::LightPanelSimulator() : LightBoxInterface(this)
{
}

void LightPanelSimulator::ISGetProperties(const char *dev)
{
    INDI::DefaultDevice::ISGetProperties(dev);

    // Get Light box properties
    LI::ISGetProperties(dev);
}

bool LightPanelSimulator::initProperties()
{
    INDI::DefaultDevice::initProperties();
    setDriverInterface(AUX_INTERFACE | LIGHTBOX_INTERFACE);
    LI::initProperties(MAIN_CONTROL_TAB, CAN_DIM);
    addAuxControls();
    return true;
}

bool LightPanelSimulator::updateProperties()
{
    INDI::DefaultDevice::updateProperties();
    LI::updateProperties();
    return true;
}

bool LightPanelSimulator::ISSnoopDevice(XMLEle *root)
{
    LI::snoop(root);

    return INDI::DefaultDevice::ISSnoopDevice(root);
}

bool LightPanelSimulator::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    if (LI::processNumber(dev, name, values, names, n))
        return true;

    return INDI::DefaultDevice::ISNewNumber(dev, name, values, names, n);
}

bool LightPanelSimulator::ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n)
{
    if (LI::processText(dev, name, texts, names, n))
        return true;

    return INDI::DefaultDevice::ISNewText(dev, name, texts, names, n);
}

bool LightPanelSimulator::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if (LI::processSwitch(dev, name, states, names, n))
        return true;

    return INDI::DefaultDevice::ISNewSwitch(dev, name, states, names, n);
}

bool LightPanelSimulator::saveConfigItems(FILE *fp)
{
    INDI::DefaultDevice::saveConfigItems(fp);
    return LI::saveConfigItems(fp);
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
