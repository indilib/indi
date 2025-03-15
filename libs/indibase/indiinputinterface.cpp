/*
    Input Interface
    Copyright (C) 2024 Jasem Mutlaq (mutlaqja@ikarustech.com)

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

#include "indiinputinterface.h"
#include <cstring>
#include "indilogger.h"

namespace INDI
{

/////////////////////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////////////////////
InputInterface::InputInterface(DefaultDevice *defaultDevice) : m_defaultDevice(defaultDevice)
{
}

/////////////////////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////////////////////
InputInterface::~InputInterface()
{
}

/////////////////////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////////////////////
void InputInterface::initProperties(const char *groupName, uint8_t digital, uint8_t analog,
                                    const std::string &digitalPrefix, const std::string &analogPrefix)
{
    m_DigitalInputLabelsConfig = false;
    m_AnalogInputLabelsConfig = false;

    // @INDI_STANDARD_PROPERTY@
    DigitalInputLabelsTP.resize(0);
    // Digital labels
    for (size_t i = 0; i < digital; i++)
    {
        auto name = "DIGITAL_INPUT_" + std::to_string(i + 1);
        auto label = digitalPrefix + " #" + std::to_string(i + 1);

        INDI::WidgetText oneLabel;
        oneLabel.fill(name, label, label);
        DigitalInputLabelsTP.push(std::move(oneLabel));
    }

    if (digital > 0)
    {
        // @INDI_STANDARD_PROPERTY@
        DigitalInputLabelsTP.fill(m_defaultDevice->getDeviceName(), "DIGITAL_INPUT_LABELS", "Digital Labels", groupName, IP_RW, 60,
                                  IPS_IDLE);
        DigitalInputLabelsTP.shrink_to_fit();
        m_DigitalInputLabelsConfig = DigitalInputLabelsTP.load();
    }

    // Analog labels
    // @INDI_STANDARD_PROPERTY@
    AnalogInputLabelsTP.resize(0);
    for (size_t i = 0; i < analog; i++)
    {
        auto name = "ANALOG_INPUT_" + std::to_string(i + 1);
        auto label = analogPrefix + " #" + std::to_string(i + 1);

        INDI::WidgetText oneLabel;
        oneLabel.fill(name, label, label);
        AnalogInputLabelsTP.push(std::move(oneLabel));
    }

    if (analog > 0)
    {
        AnalogInputLabelsTP.fill(m_defaultDevice->getDeviceName(), "ANALOG_INPUT_LABELS", "Analog Labels", groupName, IP_RW, 60,
                                 IPS_IDLE);
        AnalogInputLabelsTP.shrink_to_fit();
        m_AnalogInputLabelsConfig = AnalogInputLabelsTP.load();
    }

    // Analog inputs
    // @INDI_STANDARD_PROPERTY@
    AnalogInputsNP.reserve(analog);
    for (size_t i = 0; i < analog; i++)
    {
        auto name = "ANALOG_INPUT_" + std::to_string(i + 1);
        auto label = analogPrefix + " #" + std::to_string(i + 1);

        if (i < AnalogInputLabelsTP.count())
            label = AnalogInputLabelsTP[i].getText();

        INDI::PropertyNumber oneNumber {1};
        oneNumber[0].fill(name, label, "%.2f", 0, 1e6, 1, 1);
        oneNumber.fill(m_defaultDevice->getDeviceName(), name.c_str(), label.c_str(), groupName, IP_RO, 60, IPS_IDLE);
        AnalogInputsNP.push_back(std::move(oneNumber));
    }

    // @INDI_STANDARD_PROPERTY@
    DigitalInputsSP.reserve(digital);
    // Initialize switches, use labels if loaded.
    for (size_t i = 0; i < digital; i++)
    {
        auto name = "DIGITAL_INPUT_" + std::to_string(i + 1);
        auto label = digitalPrefix + " #" + std::to_string(i + 1);

        INDI::PropertySwitch oneInput {2};
        oneInput[Off].fill("OFF", "Off", ISS_OFF);
        oneInput[On].fill("ON", "On", ISS_OFF);

        if (i < DigitalInputLabelsTP.count())
            label = DigitalInputLabelsTP[i].getText();
        oneInput.fill(m_defaultDevice->getDeviceName(), name.c_str(), label.c_str(), groupName, IP_RO, ISR_1OFMANY, 60, IPS_IDLE);
        DigitalInputsSP.push_back(std::move(oneInput));
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////////////////////
bool InputInterface::updateProperties()
{
    if (m_defaultDevice->isConnected())
    {
        if (!DigitalInputsSP.empty())
        {
            for (auto &oneInput : DigitalInputsSP)
                m_defaultDevice->defineProperty(oneInput);
            m_defaultDevice->defineProperty(DigitalInputLabelsTP);
        }

        if (!AnalogInputsNP.empty())
        {
            for (auto &oneInput : AnalogInputsNP)
                m_defaultDevice->defineProperty(oneInput);
            m_defaultDevice->defineProperty(AnalogInputLabelsTP);
        }

    }
    else
    {
        if (!DigitalInputsSP.empty())
        {
            for (auto &oneInput : DigitalInputsSP)
                m_defaultDevice->deleteProperty(oneInput);
            m_defaultDevice->deleteProperty(DigitalInputLabelsTP);
        }

        if (!AnalogInputsNP.empty())
        {
            for (auto &oneInput : AnalogInputsNP)
                m_defaultDevice->deleteProperty(oneInput);
            m_defaultDevice->deleteProperty(AnalogInputLabelsTP);
        }
    }

    return true;
}

/////////////////////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////////////////////
bool InputInterface::processText(const char *dev, const char *name, char *texts[], char *names[], int n)
{
    if (dev && !strcmp(dev, m_defaultDevice->getDeviceName()))
    {
        // If this call due to config loading, let's delete existing dummy property and define the full one
        if (DigitalInputLabelsTP.isNameMatch(name))
        {
            DigitalInputLabelsTP.update(texts, names, n);
            DigitalInputLabelsTP.setState(IPS_OK);
            DigitalInputLabelsTP.apply();
            m_defaultDevice->saveConfig(DigitalInputLabelsTP);
            return true;
        }

        if (AnalogInputLabelsTP.isNameMatch(name))
        {
            AnalogInputLabelsTP.update(texts, names, n);
            AnalogInputLabelsTP.setState(IPS_OK);
            AnalogInputLabelsTP.apply();
            m_defaultDevice->saveConfig(AnalogInputLabelsTP);
            return true;
        }
    }

    return false;
}

/////////////////////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////////////////////
bool InputInterface::saveConfigItems(FILE *fp)
{
    DigitalInputLabelsTP.save(fp);
    AnalogInputLabelsTP.save(fp);
    return true;
}

}