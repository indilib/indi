/*******************************************************************************
  Copyright(c) 2025 Jasem Mutlaq. All rights reserved.

  INDI Safety Monitor Client

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

#include "safetymonitor_client.h"
#include "indilogger.h"
#include "indidevapi.h"

SafetyMonitorClient::SafetyMonitorClient(const std::string &deviceName, std::function<void()> statusCallback)
    : m_DeviceName(deviceName), m_DeviceOnline(false), m_HasSafetyStatus(false), m_SafetyStatusLP(1),
      m_StatusCallback(statusCallback)
{
}

SafetyMonitorClient::~SafetyMonitorClient()
{
}

bool SafetyMonitorClient::connectToServer(const std::string &host, int port)
{
    setServer(host.c_str(), port);

    if (!connectServer())
    {
        LOGF_ERROR("Failed to connect to server %s:%d for device %s",
                   host.c_str(), port, m_DeviceName.c_str());
        return false;
    }

    // Watch for the specific device
    watchDevice(m_DeviceName.c_str());

    LOGF_INFO("Safety Monitor Client: Connecting to %s@%s:%d",
              m_DeviceName.c_str(), host.c_str(), port);

    return true;
}

IPState SafetyMonitorClient::getSafetyStatus() const
{
    if (!m_HasSafetyStatus)
        return IPS_IDLE;

    return m_SafetyStatusLP.getState();
}

void SafetyMonitorClient::newDevice(INDI::BaseDevice dp)
{
    if (m_DeviceName == dp.getDeviceName())
    {
        m_DeviceOnline = true;
        LOGF_INFO("Safety Monitor Client: Device %s is online", m_DeviceName.c_str());
    }
}

void SafetyMonitorClient::removeDevice(INDI::BaseDevice dp)
{
    if (m_DeviceName == dp.getDeviceName())
    {
        m_DeviceOnline = false;
        m_HasSafetyStatus = false;
        LOGF_WARN("Safety Monitor Client: Device %s went offline", m_DeviceName.c_str());
    }
}

void SafetyMonitorClient::newProperty(INDI::Property property)
{
    if (property.getDeviceName() == m_DeviceName && property.isNameMatch("SAFETY_STATUS"))
    {
        auto lp = property.getLight();
        if (lp)
        {
            m_SafetyStatusLP = property;
            m_HasSafetyStatus = true;
            LOGF_INFO("Safety Monitor Client: Received SAFETY_STATUS from %s, state: %s",
                      m_DeviceName.c_str(), pstateStr(m_SafetyStatusLP.getState()));

            // Notify parent driver of status change
            if (m_StatusCallback)
                m_StatusCallback();
        }
    }
}

void SafetyMonitorClient::updateProperty(INDI::Property property)
{
    if (property.getDeviceName() == m_DeviceName && property.isNameMatch("SAFETY_STATUS"))
    {
        m_SafetyStatusLP = property;
        LOGF_INFO("Safety Monitor Client: Updated safety status from %s, state: %s",
                  m_DeviceName.c_str(), pstateStr(m_SafetyStatusLP.getState()));

        // Notify parent driver of status change
        if (m_StatusCallback)
            m_StatusCallback();
    }
}

void SafetyMonitorClient::serverConnected()
{
    LOGF_INFO("Safety Monitor Client: Connected to server for %s", m_DeviceName.c_str());
}

void SafetyMonitorClient::serverDisconnected(int exitCode)
{
    LOGF_WARN("Safety Monitor Client: Disconnected from server for %s (exit code: %d)",
              m_DeviceName.c_str(), exitCode);
    m_DeviceOnline = false;
    m_HasSafetyStatus = false;
}
