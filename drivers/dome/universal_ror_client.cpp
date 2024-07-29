/*******************************************************************************
  Copyright(c) 2024 Jasem Mutlaq. All rights reserved.

  INDI Universal ROR Client. It connects to INDI server running locally at
  localhost:7624. It checks for both input and output drivers.

  Output driver is used to command Open, Close, and Stop ROR.
  Input driver is used to query the fully closed and opened states.

  The client does NOT stop the roof if the limit switches are activated. This
  is the responsiblity of the external hardware.

  This program is free software; you can redistribute it and/or modify it
  under the terms of the GNU General Public License as published by the Free
  Software Foundation; either version 2 of the License, or (at your option)
  any later version.

  This program is distributed in the hope that it will be useful, but WITHOUT
  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
  more details.

  You should have received a copy of the GNU Library General Public License
  along with this library; see the file COPYING.LIB.  If not, write to
  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
  Boston, MA 02110-1301, USA.

  The full GNU General Public License is included in this distribution in the
  file called LICENSE.
*******************************************************************************/

#include "universal_ror_client.h"

#include <cmath>
#include <cstring>

///////////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////////
UniversalRORClient::UniversalRORClient(const std::string &input, const std::string &output) : m_Input(input),
    m_Output(output)
{
}

///////////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////////
UniversalRORClient::~UniversalRORClient()
{
}

///////////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////////
void UniversalRORClient::newDevice(INDI::BaseDevice dp)
{
    if (dp.isDeviceNameMatch(m_Input))
        m_InputReady = true;
    if (dp.isDeviceNameMatch(m_Output))
        m_OutputReady = true;
}

///////////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////////
void UniversalRORClient::newProperty(INDI::Property property)
{
    updateProperty(property);
}

///////////////////////////////////////////////////////////////////////////////////////////
/// Check if any of the relevant digital inputs are for the fully opened or closed states
///////////////////////////////////////////////////////////////////////////////////////////
void UniversalRORClient::updateProperty(INDI::Property property)
{
    auto fullyOpenedUpdated = false, fullyClosedUpdated = false;
    for (const auto &value : m_InputFullyOpened)
    {
        const auto name = "DIGITAL_INPUT_" + std::to_string(value);
        if (property.isNameMatch(name))
        {
            fullyOpenedUpdated = true;
        }
    }

    if (fullyOpenedUpdated)
        syncFullyOpenedState();

    for (const auto &value : m_InputFullyClosed)
    {
        const auto name = "DIGITAL_INPUT_" + std::to_string(value);
        if (property.isNameMatch(name))
        {
            fullyClosedUpdated = true;
        }
    }

    if (fullyClosedUpdated)
        syncFullyClosedState();

}

///////////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////////
bool UniversalRORClient::openRoof()
{
    auto device = getDevice(m_Output.c_str());
    if (!device || !device.isConnected())
        return false;

    for (const auto &value : m_OutputOpenRoof)
    {
        const auto name = "DIGITAL_OUTPUT_" + std::to_string(value);
        auto property = device.getSwitch(name.c_str());
        if (property)
        {
            property.reset();
            property[1].setState(ISS_ON);
            sendNewSwitch(property);
        }
    }
    return true;
}

///////////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////////
bool UniversalRORClient::closeRoof()
{
    auto device = getDevice(m_Output.c_str());
    if (!device || !device.isConnected())
        return false;

    for (const auto &value : m_OutputCloseRoof)
    {
        const auto name = "DIGITAL_OUTPUT_" + std::to_string(value);
        auto property = device.getSwitch(name.c_str());
        if (property)
        {
            property.reset();
            property[1].setState(ISS_ON);
            sendNewSwitch(property);
        }
    }
    return true;
}

///////////////////////////////////////////////////////////////////////////////////////////
/// Sets both Close & Open roof outputs to OFF
///////////////////////////////////////////////////////////////////////////////////////////
bool UniversalRORClient::stop()
{
    auto device = getDevice(m_Output.c_str());
    if (!device || !device.isConnected())
        return false;

    // Find all close roof digital outputs and set them to OFF
    // Only send OFF if required
    for (const auto &value : m_OutputCloseRoof)
    {
        const auto name = "DIGITAL_OUTPUT_" + std::to_string(value);
        auto property = device.getSwitch(name.c_str());
        if (property && property[0].getState() != ISS_ON)
        {
            property.reset();
            property[0].setState(ISS_ON);
            sendNewSwitch(property);
        }
    }

    // Same for Open roof digital outputs
    for (const auto &value : m_OutputOpenRoof)
    {
        const auto name = "DIGITAL_OUTPUT_" + std::to_string(value);
        auto property = device.getSwitch(name.c_str());
        if (property && property[0].getState() != ISS_ON)
        {
            property.reset();
            property[0].setState(ISS_ON);
            sendNewSwitch(property);
        }
    }
    return true;
}

///////////////////////////////////////////////////////////////////////////////////////////
/// Checks fully opened state properties.
///////////////////////////////////////////////////////////////////////////////////////////
void UniversalRORClient::syncFullyOpenedState()
{
    auto device = getDevice(m_Input.c_str());
    if (!device || !device.isConnected())
        return;

    std::vector<bool> fullyOpenedStates;
    for (const auto &value : m_InputFullyOpened)
    {
        const auto name = "DIGITAL_INPUT_" + std::to_string(value);
        auto property = device.getSwitch(name.c_str());
        if (property)
        {
            auto toggled = property[1].getState() == ISS_ON;
            fullyOpenedStates.push_back(toggled);
        }
    }

    auto on = std::all_of(fullyOpenedStates.begin(), fullyOpenedStates.end(), [](bool value)
    {
        return value;
    });
    m_FullyOpenedCallback(on);
}

///////////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////////
void UniversalRORClient::syncFullyClosedState()
{
    auto device = getDevice(m_Input.c_str());
    if (!device || !device.isConnected())
        return;

    std::vector<bool> fullyClosedStates;
    for (const auto &value : m_InputFullyClosed)
    {
        const auto name = "DIGITAL_INPUT_" + std::to_string(value);
        auto property = device.getSwitch(name.c_str());
        if (property)
        {
            auto toggled = property[1].getState() == ISS_ON;
            fullyClosedStates.push_back(toggled);
        }
    }

    auto on = std::all_of(fullyClosedStates.begin(), fullyClosedStates.end(), [](bool value)
    {
        return value;
    });
    m_FullyClosedCallback(on);
}