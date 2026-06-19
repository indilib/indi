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

#include "indilightboxinterface.h"

#include "indilogger.h"

#include <cstring>

namespace INDI
{

////////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////////
LightBoxInterface::LightBoxInterface(DefaultDevice *device) : m_DefaultDevice(device)
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////////
LightBoxInterface::~LightBoxInterface()
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////////
void LightBoxInterface::initProperties(const char *group, uint32_t capabilities)
{
    m_Capabilities = capabilities;
    // Turn on/off light
    // @INDI_STANDARD_PROPERTY@
    LightSP[FLAT_LIGHT_ON].fill("FLAT_LIGHT_ON", "On", ISS_OFF);
    LightSP[FLAT_LIGHT_OFF].fill("FLAT_LIGHT_OFF", "Off", ISS_ON);
    LightSP.fill(m_DefaultDevice->getDeviceName(), "FLAT_LIGHT_CONTROL", "Flat Light", group, IP_RW,  ISR_1OFMANY, 0, IPS_IDLE);

    // Light Intensity
    // @INDI_STANDARD_PROPERTY@
    LightIntensityNP[0].fill("FLAT_LIGHT_INTENSITY_VALUE", "Value", "%.f", 0, 255, 10, 0);
    LightIntensityNP.fill(m_DefaultDevice->getDeviceName(), "FLAT_LIGHT_INTENSITY", "Brightness", group, IP_RW, 0, IPS_IDLE);

    // Active Devices
    ActiveDeviceTP[0].fill("ACTIVE_FILTER", "Filter", "Filter Simulator");
    ActiveDeviceTP.fill(m_DefaultDevice->getDeviceName(), "ACTIVE_DEVICES", "Snoop devices", OPTIONS_TAB, IP_RW, 60, IPS_IDLE);
    ActiveDeviceTP.load();

    // Filter Intensities
    // @INDI_STANDARD_PROPERTY@
    FilterIntensityNP.fill(m_DefaultDevice->getDeviceName(), "FLAT_LIGHT_FILTER_INTENSITY", "Filter Intensity", "Preset", IP_RW,
                           60, IPS_IDLE);

    IDSnoopDevice(ActiveDeviceTP[0].getText(), "FILTER_SLOT");
    IDSnoopDevice(ActiveDeviceTP[0].getText(), "FILTER_NAME");
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////////
void LightBoxInterface::ISGetProperties(const char *deviceName)
{
    INDI_UNUSED(deviceName);
    m_DefaultDevice->defineProperty(ActiveDeviceTP);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////////
bool LightBoxInterface::updateProperties()
{
    if (m_DefaultDevice->isConnected())
    {
        m_DefaultDevice->defineProperty(LightSP);
        if (m_Capabilities & CAN_DIM)
            m_DefaultDevice->defineProperty(LightIntensityNP);
        if (!FilterIntensityNP.isEmpty())
            m_DefaultDevice->defineProperty(FilterIntensityNP);
    }
    else
    {
        m_DefaultDevice->deleteProperty(LightSP);
        if (m_Capabilities & CAN_DIM)
            m_DefaultDevice->deleteProperty(LightIntensityNP);

        if (!FilterIntensityNP.isEmpty())
            m_DefaultDevice->deleteProperty(FilterIntensityNP);
    }

    return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////////
bool LightBoxInterface::processSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if (strcmp(dev, m_DefaultDevice->getDeviceName()))
        return false;

    // Light
    if (LightSP.isNameMatch(name))
    {
        auto prevIndex = LightSP.findOnSwitchIndex();
        LightSP.update(states, names, n);
        auto rc = EnableLightBox(LightSP[FLAT_LIGHT_ON].getState() == ISS_ON ? true : false);
        LightSP.setState(rc ? IPS_OK : IPS_ALERT);

        if (!rc)
        {
            LightSP.reset();
            LightSP[prevIndex].setState(ISS_ON);
        }

        LightSP.apply();
        return true;
    }

    return false;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////////
bool LightBoxInterface::processNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    if (strcmp(dev, m_DefaultDevice->getDeviceName()))
        return false;

    // Light Intensity
    if (LightIntensityNP.isNameMatch(name))
    {
        auto prevValue = LightIntensityNP[0].getValue();
        LightIntensityNP.update(values, names, n);

        bool rc = SetLightBoxBrightness(LightIntensityNP[0].getValue());
        if (rc)
            LightIntensityNP.setState(IPS_OK);
        else
        {
            LightIntensityNP[0].setValue(prevValue);
            LightIntensityNP.setState(IPS_ALERT);
        }

        LightIntensityNP.apply();
        // If we have no filters, then save config
        if (FilterIntensityNP.isEmpty())
            m_DefaultDevice->saveConfig(LightIntensityNP);
        return true;
    }

    if (FilterIntensityNP.isNameMatch(name))
    {
        // If this is first time, we add the filters
        if (FilterIntensityNP.isEmpty())
        {
            for (int i = 0; i < n; i++)
                addFilterDuration(names[i], values[i]);

            m_DefaultDevice->defineProperty(FilterIntensityNP);
            return true;
        }

        FilterIntensityNP.update(values, names, n);
        FilterIntensityNP.setState(IPS_OK);
        FilterIntensityNP.apply();
        m_DefaultDevice->saveConfig(FilterIntensityNP);
        return true;
    }

    return false;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////////
bool LightBoxInterface::processText(const char *dev, const char *name, char *texts[], char *names[], int n)
{
    if (strcmp(dev, m_DefaultDevice->getDeviceName()))
        return false;

    if (ActiveDeviceTP.isNameMatch(name))
    {
        ActiveDeviceTP.setState(IPS_OK);
        ActiveDeviceTP.update(texts, names, n);
        //  Update client display
        ActiveDeviceTP.apply();
        m_DefaultDevice->saveConfig(ActiveDeviceTP);

        if (!ActiveDeviceTP[0].isEmpty())
        {
            IDSnoopDevice(ActiveDeviceTP[0].getText(), "FILTER_SLOT");
            IDSnoopDevice(ActiveDeviceTP[0].getText(), "FILTER_NAME");
        }
        // If filter removed, remove presets
        else
        {
            m_DefaultDevice->deleteProperty(FilterIntensityNP);
        }
        return true;
    }


    return false;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////////
bool LightBoxInterface::EnableLightBox(bool enable)
{
    INDI_UNUSED(enable);
    // Must be implemented by child class
    return false;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////////
bool LightBoxInterface::SetLightBoxBrightness(uint16_t value)
{
    INDI_UNUSED(value);
    // Must be implemented by child class
    return false;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////////
bool LightBoxInterface::snoop(XMLEle *root)
{
    auto deviceName = findXMLAttValu(root, "device");

    // If dimming not supported or not our device, we return
    if (!(m_Capabilities & CAN_DIM) || strcmp(ActiveDeviceTP[0].getText(), deviceName))
        return false;

    XMLEle *ep = nullptr;
    auto propTag  = tagXMLEle(root);
    auto propName = findXMLAttValu(root, "name");

    if (!strcmp(propTag, "delProperty"))
        return false;

    if (!strcmp(propName, "FILTER_NAME"))
    {
        if (!FilterIntensityNP.isEmpty())
        {
            uint32_t snoopCounter = 0;
            bool isDifferent = false;
            for (ep = nextXMLEle(root, 1); ep != nullptr; ep = nextXMLEle(root, 0))
            {
                // If either count or labels are different, then mark it as such
                if (snoopCounter >= FilterIntensityNP.count() || !FilterIntensityNP[snoopCounter].isLabelMatch(pcdataXMLEle(ep)))
                {
                    isDifferent = true;
                    break;
                }

                snoopCounter++;
            }

            if (isDifferent == false && snoopCounter != FilterIntensityNP.count())
                isDifferent = true;

            // Check if we have different FILTER_NAME
            // If identical, no need to recreate it.
            if (isDifferent)
            {
                m_DefaultDevice->deleteProperty(FilterIntensityNP);
                FilterIntensityNP.resize(0);
            }
            else
                return false;
        }

        for (ep = nextXMLEle(root, 1); ep != nullptr; ep = nextXMLEle(root, 0))
            addFilterDuration(pcdataXMLEle(ep), 0);

        FilterIntensityNP.load();
        m_DefaultDevice->defineProperty(FilterIntensityNP);

        if (m_DefaultDevice->isConnected())
        {
            if (currentFilterSlot < FilterIntensityNP.count())
            {
                auto duration = FilterIntensityNP[currentFilterSlot].getValue();
                if (duration > 0)
                    SetLightBoxBrightness(duration);
            }
        }
    }
    else if (!strcmp(propName, "FILTER_SLOT"))
    {
        // Only accept IPS_OK/IPS_IDLE state
        if (strcmp(findXMLAttValu(root, "state"), "Ok") && strcmp(findXMLAttValu(root, "state"), "Idle"))
            return false;

        for (ep = nextXMLEle(root, 1); ep != nullptr; ep = nextXMLEle(root, 0))
        {
            const char *elemName = findXMLAttValu(ep, "name");

            if (!strcmp(elemName, "FILTER_SLOT_VALUE"))
            {
                currentFilterSlot = atoi(pcdataXMLEle(ep)) - 1;
                break;
            }
        }

        if (!FilterIntensityNP.isEmpty() && m_DefaultDevice->isConnected())
        {
            if (currentFilterSlot < FilterIntensityNP.count())
            {
                auto value = FilterIntensityNP[currentFilterSlot].getValue();
                if (value > 0)
                {
                    if (SetLightBoxBrightness(value))
                    {
                        LightIntensityNP[0].setValue(value);
                        LightIntensityNP.setState(IPS_OK);
                        LightIntensityNP.apply();
                    }
                }
            }
        }
    }

    return false;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////////
void LightBoxInterface::addFilterDuration(const char *filterName, uint16_t filterDuration)
{
    // Ensure no duplicates
    for (uint32_t i = 0; i < FilterIntensityNP.count(); i++)
    {
        if (FilterIntensityNP[i].isNameMatch(filterName))
            return;
    }

    INDI::WidgetNumber element;
    element.fill(filterName, filterName, "%0.f", 0, LightIntensityNP[0].getMax(), LightIntensityNP[0].getStep(),
                 filterDuration);
    FilterIntensityNP.push(std::move(element));
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////////
bool LightBoxInterface::saveConfigItems(FILE *fp)
{
    ActiveDeviceTP.save(fp);
    // N.B. In case we do not have any filters defined, the light intensity is saved directly.
    // Otherwise, we rely on filter settings to set the appropiate intensity for each filter.
    if (FilterIntensityNP.isEmpty())
        LightIntensityNP.save(fp);
    else
        FilterIntensityNP.save(fp);

    return true;
}
}
