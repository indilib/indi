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
#include "indilogger.h"

#include <cmath>
#include <cstring>

///////////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////////
UniversalRORClient::UniversalRORClient(const std::string &input, const std::string &output) : m_Input(input),
    m_Output(output)
{
    DEBUGFDEVICE("Universal ROR", INDI::Logger::DBG_DEBUG, "Client initialized with input driver: %s, output driver: %s",
                 input.c_str(), output.c_str());
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
    if (dp.isDeviceNameMatch(m_Input) && dp.isConnected())
    {
        m_InputReady = true;
        DEBUGFDEVICE("Universal ROR", INDI::Logger::DBG_DEBUG, "Input device '%s' is ready", m_Input.c_str());
    }

    if (dp.isDeviceNameMatch(m_Output) && dp.isConnected())
    {
        m_OutputReady = true;
        DEBUGFDEVICE("Universal ROR", INDI::Logger::DBG_DEBUG, "Output device '%s' is ready", m_Output.c_str());
    }

    // If we just became connected and we have a connection callback, call it
    if (m_InputReady && m_OutputReady && m_ConnectionCallback)
    {
        DEBUGDEVICE("Universal ROR", INDI::Logger::DBG_DEBUG, "Both devices are now connected, triggering connection callback");
        m_ConnectionCallback(true);
    }
}

///////////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////////
void UniversalRORClient::newProperty(INDI::Property property)
{
    updateProperty(property);
}

///////////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////////
void UniversalRORClient::serverDisconnected(int exitCode)
{
    INDI_UNUSED(exitCode);
    m_InputReady = m_OutputReady = false;
    DEBUGFDEVICE("Universal ROR", INDI::Logger::DBG_DEBUG, "Server disconnected with exit code %d", exitCode);
}

///////////////////////////////////////////////////////////////////////////////////////////
/// Check if any of the relevant digital inputs are for the fully opened or closed states
///////////////////////////////////////////////////////////////////////////////////////////
void UniversalRORClient::updateProperty(INDI::Property property)
{
    // If we're not fully connected yet, let's check.
    if (!m_InputReady || !m_OutputReady)
    {
        if (property.isNameMatch("CONNECTION"))
        {
            auto toggled = property.getSwitch()->at(0)->getState() == ISS_ON;
            if (property.isDeviceNameMatch(m_Input))
                m_InputReady = toggled;
            if (property.isDeviceNameMatch(m_Output))
                m_OutputReady = toggled;

            if (m_InputReady && m_OutputReady && m_ConnectionCallback)
            {
                DEBUGDEVICE("Universal ROR", INDI::Logger::DBG_DEBUG, "Both devices are now connected, triggering connection callback");
                m_ConnectionCallback(true);
            }

            return;
        }
    }

    auto fullyOpenedUpdated = false, fullyClosedUpdated = false;
    for (const auto &value : m_InputFullyOpened)
    {
        const auto name = "DIGITAL_INPUT_" + std::to_string(value);
        if (property.isNameMatch(name))
        {
            fullyOpenedUpdated = true;
            DEBUGFDEVICE("Universal ROR", INDI::Logger::DBG_DEBUG, "Fully opened input %s updated", name.c_str());
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
            DEBUGFDEVICE("Universal ROR", INDI::Logger::DBG_DEBUG, "Fully closed input %s updated", name.c_str());
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
    {
        DEBUGFDEVICE("Universal ROR", INDI::Logger::DBG_ERROR, "Cannot open roof - Output device '%s' not connected",
                     m_Output.c_str());
        return false;
    }

    DEBUGFDEVICE("Universal ROR", INDI::Logger::DBG_DEBUG, "Opening roof using output device '%s'", m_Output.c_str());

    for (const auto &value : m_OutputOpenRoof)
    {
        const auto name = "DIGITAL_OUTPUT_" + std::to_string(value);
        auto property = device.getSwitch(name.c_str());
        if (property)
        {
            DEBUGFDEVICE("Universal ROR", INDI::Logger::DBG_DEBUG, "Setting output %s to ON", name.c_str());
            property.reset();
            property[1].setState(ISS_ON);
            sendNewSwitch(property);
        }
        else
        {
            DEBUGFDEVICE("Universal ROR", INDI::Logger::DBG_ERROR, "Failed to get switch property for %s", name.c_str());
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
    {
        DEBUGFDEVICE("Universal ROR", INDI::Logger::DBG_ERROR, "Cannot close roof - Output device '%s' not connected",
                     m_Output.c_str());
        return false;
    }

    DEBUGFDEVICE("Universal ROR", INDI::Logger::DBG_DEBUG, "Closing roof using output device '%s'", m_Output.c_str());

    for (const auto &value : m_OutputCloseRoof)
    {
        const auto name = "DIGITAL_OUTPUT_" + std::to_string(value);
        auto property = device.getSwitch(name.c_str());
        if (property)
        {
            DEBUGFDEVICE("Universal ROR", INDI::Logger::DBG_DEBUG, "Setting output %s to ON", name.c_str());
            property.reset();
            property[1].setState(ISS_ON);
            sendNewSwitch(property);
        }
        else
        {
            DEBUGFDEVICE("Universal ROR", INDI::Logger::DBG_ERROR, "Failed to get switch property for %s", name.c_str());
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
    {
        DEBUGFDEVICE("Universal ROR", INDI::Logger::DBG_ERROR, "Cannot stop roof - Output device '%s' not connected",
                     m_Output.c_str());
        return false;
    }

    DEBUGFDEVICE("Universal ROR", INDI::Logger::DBG_DEBUG, "Stopping roof movement using output device '%s'", m_Output.c_str());

    // Find all close roof digital outputs and set them to OFF
    // Only send OFF if required
    for (const auto &value : m_OutputCloseRoof)
    {
        const auto name = "DIGITAL_OUTPUT_" + std::to_string(value);
        auto property = device.getSwitch(name.c_str());
        if (property && property[0].getState() != ISS_ON)
        {
            DEBUGFDEVICE("Universal ROR", INDI::Logger::DBG_DEBUG, "Setting close output %s to OFF", name.c_str());
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
            DEBUGFDEVICE("Universal ROR", INDI::Logger::DBG_DEBUG, "Setting open output %s to OFF", name.c_str());
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
bool UniversalRORClient::syncFullyOpenedState()
{
    auto device = getDevice(m_Input.c_str());
    if (!device || !device.isConnected())
    {
        DEBUGFDEVICE("Universal ROR", INDI::Logger::DBG_ERROR, "Cannot sync open state - Input device '%s' not connected",
                     m_Input.c_str());
        return false;
    }

    DEBUGFDEVICE("Universal ROR", INDI::Logger::DBG_DEBUG, "Syncing fully opened state from input device '%s'",
                 m_Input.c_str());

    std::vector<bool> fullyOpenedStates;
    for (const auto &value : m_InputFullyOpened)
    {
        const auto name = "DIGITAL_INPUT_" + std::to_string(value);
        auto property = device.getSwitch(name.c_str());
        if (property)
        {
            auto toggled = property[1].getState() == ISS_ON;
            DEBUGFDEVICE("Universal ROR", INDI::Logger::DBG_DEBUG, "Input %s state: %s", name.c_str(), toggled ? "ON" : "OFF");
            fullyOpenedStates.push_back(toggled);
        }
        else
        {
            DEBUGFDEVICE("Universal ROR", INDI::Logger::DBG_ERROR, "Failed to get switch property for %s", name.c_str());
            return false;
        }
    }

    auto on = std::all_of(fullyOpenedStates.begin(), fullyOpenedStates.end(), [](bool value)
    {
        return value;
    });
    DEBUGFDEVICE("Universal ROR", INDI::Logger::DBG_DEBUG, "Fully opened state: %s", on ? "YES" : "NO");
    m_FullyOpenedCallback(on);
    return true;
}

///////////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////////
bool UniversalRORClient::syncFullyClosedState()
{
    auto device = getDevice(m_Input.c_str());
    if (!device || !device.isConnected())
    {
        DEBUGFDEVICE("Universal ROR", INDI::Logger::DBG_ERROR, "Cannot sync closed state - Input device '%s' not connected",
                     m_Input.c_str());
        return false;
    }

    DEBUGFDEVICE("Universal ROR", INDI::Logger::DBG_DEBUG, "Syncing fully closed state from input device '%s'",
                 m_Input.c_str());

    std::vector<bool> fullyClosedStates;
    for (const auto &value : m_InputFullyClosed)
    {
        const auto name = "DIGITAL_INPUT_" + std::to_string(value);
        auto property = device.getSwitch(name.c_str());
        if (property)
        {
            auto toggled = property[1].getState() == ISS_ON;
            DEBUGFDEVICE("Universal ROR", INDI::Logger::DBG_DEBUG, "Input %s state: %s", name.c_str(), toggled ? "ON" : "OFF");
            fullyClosedStates.push_back(toggled);
        }
        else
        {
            DEBUGFDEVICE("Universal ROR", INDI::Logger::DBG_ERROR, "Failed to get switch property for %s", name.c_str());
            return false;
        }
    }

    auto on = std::all_of(fullyClosedStates.begin(), fullyClosedStates.end(), [](bool value)
    {
        return value;
    });
    DEBUGFDEVICE("Universal ROR", INDI::Logger::DBG_DEBUG, "Fully closed state: %s", on ? "YES" : "NO");
    m_FullyClosedCallback(on);
    return true;
}
