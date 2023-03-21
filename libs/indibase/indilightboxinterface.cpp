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

LightBoxInterface::LightBoxInterface(DefaultDevice *device, bool isDimmable) : AbstractInterface(device),
    m_isDimmable(isDimmable)
{
}

LightBoxInterface::~LightBoxInterface()
{
}

void LightBoxInterface::initProperties(const char *group)
{
    // Turn on/off light
    LightSP[ParentDevice::INDI_ENABLED].fill("INDI_ENABLED", "On", ISS_OFF);
    LightSP[ParentDevice::INDI_DISABLED].fill("INDI_DISABLED", "Off", ISS_ON);
    LightSP.fill(m_DefaultDevice->getDeviceName(), "FLAT_LIGHT_CONTROL", "Flat Light", group, IP_RW,  ISR_1OFMANY, 0, IPS_IDLE);

    // Light Intensity
    LightIntensityNP[0].fill("FLAT_LIGHT_INTENSITY_VALUE", "Value", "%.f", 0, 255, 10, 0);
    LightIntensityNP.fill(m_DefaultDevice->getDeviceName(), "FLAT_LIGHT_INTENSITY", "Brightness", group, IP_RW, 0, IPS_IDLE);

    // Active Devices
    strncpy(m_ConfigFilter, "Filter Simulator", MAXINDIDEVICE);
    IUGetConfigText(m_DefaultDevice->getDeviceName(), "ACTIVE_DEVICES", "ACTIVE_FILTER", m_ConfigFilter, MAXINDIDEVICE);
    ActiveDeviceTP[0].fill("ACTIVE_FILTER", "Filter", m_ConfigFilter);
    ActiveDeviceTP.fill(m_DefaultDevice->getDeviceName(), "ACTIVE_DEVICES", "Snoop devices", OPTIONS_TAB, IP_RW, 60, IPS_IDLE);

    // Filter duration
    FilterIntensityNP.fill(m_DefaultDevice->getDeviceName(), "FLAT_LIGHT_FILTER_INTENSITY", "Filter Intensity",
                           "Preset", IP_RW, 60, IPS_OK);

    IDSnoopDevice(m_ConfigFilter, "FILTER_SLOT");
    IDSnoopDevice(m_ConfigFilter, "FILTER_NAME");
}

void LightBoxInterface::ISGetProperties(const char *deviceName)
{
    INDI_UNUSED(deviceName);
    // FIXME. Interfaces should not define ActiveDevice property
    // as this may interface with parent class. Need to work on a method to send the devices
    // and properties we need to snoop to the parent class.
    m_DefaultDevice->defineProperty(ActiveDeviceTP);
}

bool LightBoxInterface::updateProperties()
{
    if (m_DefaultDevice->isConnected())
    {
        m_DefaultDevice->defineProperty(LightSP);
        m_DefaultDevice->defineProperty(LightIntensityNP);

        if (FilterIntensityNP.count() > 0)
            m_DefaultDevice->defineProperty(FilterIntensityNP);
    }
    else
    {
        m_DefaultDevice->deleteProperty(LightSP);
        m_DefaultDevice->deleteProperty(LightIntensityNP);
        if (FilterIntensityNP.count() > 0)
            m_DefaultDevice->deleteProperty(FilterIntensityNP);
    }

    return true;
}

bool LightBoxInterface::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    INDI_UNUSED(dev);
    // Light
    if (LightSP.isNameMatch(name))
    {
        auto prevIndex = LightSP.findOnSwitchIndex();
        LightSP.update(states, names, n);
        auto rc = EnableLightBox(LightSP[ParentDevice::INDI_ENABLED].s == ISS_ON);
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

bool LightBoxInterface::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    INDI_UNUSED(dev);

    // Light Intensity
    if (LightIntensityNP.isNameMatch(name))
    {
        if (SetLightBoxBrightness(values[0]))
        {
            LightIntensityNP.update(values, names, n);
            LightIntensityNP.setState(IPS_OK);
        }
        else
        {
            LightIntensityNP.setState(IPS_ALERT);
        }

        LightIntensityNP.apply();
        return true;
    }

    // Filter Intensity
    if (FilterIntensityNP.isNameMatch(name))
    {
        if (FilterIntensityNP.count() == 0)
        {
            for (auto i = 0; i < n; i++)
                addFilterDuration(names[i], values[i]);

            m_DefaultDevice->defineProperty(FilterIntensityNP);
            return true;
        }

        FilterIntensityNP.update(values, names, n);
        FilterIntensityNP.setState(IPS_OK);
        FilterIntensityNP.apply(nullptr);
        m_DefaultDevice->saveConfig(FilterIntensityNP);
        return true;
    }

    return false;
}

bool LightBoxInterface::ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n)
{
    INDI_UNUSED(dev);
    if (ActiveDeviceTP.isNameMatch(name))
    {
        ActiveDeviceTP.setState(IPS_OK);
        ActiveDeviceTP.update(texts, names, n);
        //  Update client display
        ActiveDeviceTP.apply();

        if (strlen(ActiveDeviceTP[0].getText()) > 0)
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

bool LightBoxInterface::EnableLightBox(bool enable)
{
    INDI_UNUSED(enable);
    // Must be implemented by child class
    return false;
}

bool LightBoxInterface::SetLightBoxBrightness(uint16_t value)
{
    INDI_UNUSED(value);
    // Must be implemented by child class
    return false;
}

bool LightBoxInterface::ISSnoopDevice(XMLEle *root)
{
    if (m_isDimmable == false)
        return false;

    XMLEle *ep           = nullptr;
    const char *propTag  = tagXMLEle(root);
    const char *propName = findXMLAttValu(root, "name");

    if (!strcmp(propTag, "delProperty"))
        return false;

    if (!strcmp(propName, "FILTER_NAME"))
    {
        if (FilterIntensityNP.count() > 0)
        {
            size_t snoopCounter = 0;
            bool isDifferent = false;
            for (ep = nextXMLEle(root, 1); ep != nullptr; ep = nextXMLEle(root, 0))
            {
                if (snoopCounter >= FilterIntensityNP.count() || (strcmp(FilterIntensityNP[snoopCounter].getLabel(), pcdataXMLEle(ep))))
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
            }
            else
                return false;
        }

        for (ep = nextXMLEle(root, 1); ep != nullptr; ep = nextXMLEle(root, 0))
            addFilterDuration(pcdataXMLEle(ep), 0);

        m_DefaultDevice->defineProperty(FilterIntensityNP);
        char errmsg[MAXRBUF];
        IUReadConfig(nullptr, m_DefaultDevice->getDeviceName(), "FLAT_LIGHT_FILTER_INTENSITY", 1, errmsg);

        if (m_DefaultDevice->isConnected())
        {
            if (m_CurrentFilterSlot < FilterIntensityNP.count())
            {
                auto duration = FilterIntensityNP[m_CurrentFilterSlot].getValue();
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
                m_CurrentFilterSlot = atoi(pcdataXMLEle(ep)) - 1;
                break;
            }
        }

        if (FilterIntensityNP.count() > 0 && m_DefaultDevice->isConnected())
        {
            if (m_CurrentFilterSlot < FilterIntensityNP.count())
            {
                double duration = FilterIntensityNP[m_CurrentFilterSlot].getValue();
                if (duration > 0)
                    SetLightBoxBrightness(duration);
            }
        }
    }

    return false;
}

void LightBoxInterface::addFilterDuration(const char *filterName, uint16_t filterDuration)
{
    if (FilterIntensityN == nullptr)
    {
        FilterIntensityN = (INumber *)malloc(sizeof(INumber));
        DEBUGDEVICE(device->getDeviceName(), Logger::DBG_DEBUG, "Filter intensity preset created.");
    }
    else
    {
        // Ensure no duplicates
        for (int i = 0; i < FilterIntensityNP.nnp; i++)
        {
            if (!strcmp(filterName, FilterIntensityN[i].name))
                return;
        }

        FilterIntensityN = (INumber *)realloc(FilterIntensityN, (FilterIntensityNP.nnp + 1) * sizeof(INumber));
    }

    IUFillNumber(&FilterIntensityN[FilterIntensityNP.nnp], filterName, filterName, "%0.f", 0, LightIntensityN[0].max,
                 LightIntensityN[0].step, filterDuration);

    FilterIntensityNP.nnp++;
    FilterIntensityNP.np = FilterIntensityN;
}

bool LightBoxInterface::saveLightBoxConfigItems(FILE *fp)
{
    IUSaveConfigText(fp, &ActiveDeviceTP);
    if (FilterIntensityN != nullptr)
        IUSaveConfigNumber(fp, &FilterIntensityNP);

    return true;
}
}
