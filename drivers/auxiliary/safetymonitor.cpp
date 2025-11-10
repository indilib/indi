/*******************************************************************************
  Copyright(c) 2025 Jasem Mutlaq. All rights reserved.

  INDI Safety Monitor Driver
  Monitors multiple INDI devices' SAFETY_STATUS properties and reports the worst case

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

#include "safetymonitor.h"

#include <memory>
#include <regex>
#include <sstream>

// We declare an auto pointer to SafetyMonitor
std::unique_ptr<SafetyMonitor> safetyMonitor(new SafetyMonitor());

SafetyMonitor::SafetyMonitor()
{
    setVersion(1, 0);
}

const char *SafetyMonitor::getDefaultName()
{
    return "Safety Monitor";
}

bool SafetyMonitor::initProperties()
{
    INDI::DefaultDevice::initProperties();

    // Connection strings
    ConnectionStringsTP[0].fill("DEVICES", "Devices", "");
    ConnectionStringsTP.fill(getDeviceName(), "CONNECTION_STRINGS", "Safety Devices", 
                             OPTIONS_TAB, IP_RW, 60, IPS_IDLE);
    ConnectionStringsTP.load();

    // Start with empty safety status property
    SafetyStatusLP.fill(getDeviceName(), "SAFETY_STATUS", "Status", 
                        MAIN_CONTROL_TAB, IPS_IDLE);

    // Override switch
    OverrideSP[0].fill("OVERRIDE", "Override Status", ISS_OFF);
    OverrideSP.fill(getDeviceName(), "SAFETY_OVERRIDE", "Safety", 
                    MAIN_CONTROL_TAB, IP_RW, ISR_NOFMANY, 0, IPS_IDLE);

    addDebugControl();
    setDriverInterface(AUX_INTERFACE);

    return true;
}

void SafetyMonitor::ISGetProperties(const char *dev)
{
    INDI::DefaultDevice::ISGetProperties(dev);

    defineProperty(ConnectionStringsTP);
}

bool SafetyMonitor::updateProperties()
{
    INDI::DefaultDevice::updateProperties();

    if (isConnected())
    {
        if (SafetyStatusLP.count() > 0)
            defineProperty(SafetyStatusLP);
        defineProperty(OverrideSP);
    }
    else
    {
        if (SafetyStatusLP.count() > 0)
            deleteProperty(SafetyStatusLP);
        deleteProperty(OverrideSP);
    }

    return true;
}

bool SafetyMonitor::Connect()
{
    // Parse connection strings and build property
    rebuildSafetyStatusProperty();
    return true;
}

bool SafetyMonitor::Disconnect()
{
    // Disconnect all clients
    for (auto &client : m_Clients)
    {
        client->disconnectServer();
    }
    return true;
}

bool SafetyMonitor::ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        if (ConnectionStringsTP.isNameMatch(name))
        {
            ConnectionStringsTP.update(texts, names, n);
            ConnectionStringsTP.setState(IPS_OK);
            ConnectionStringsTP.apply();

            // Rebuild everything when connection strings change
            rebuildSafetyStatusProperty();
            saveConfig(ConnectionStringsTP);
            return true;
        }
    }

    return INDI::DefaultDevice::ISNewText(dev, name, texts, names, n);
}

bool SafetyMonitor::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        if (OverrideSP.isNameMatch(name))
        {
            OverrideSP.update(states, names, n);
            
            if (OverrideSP[0].getState() == ISS_ON)
            {
                LOG_WARN("Safety override is enabled. Observatory safety is overridden. Turn off as soon as possible.");
                OverrideSP.setState(IPS_BUSY);
            }
            else
            {
                LOG_INFO("Safety override is disabled");
                OverrideSP.setState(IPS_IDLE);
            }
            
            OverrideSP.apply();
            
            // Update overall status considering the override
            updateOverallStatus();
            
            return true;
        }
    }

    return INDI::DefaultDevice::ISNewSwitch(dev, name, states, names, n);
}

bool SafetyMonitor::saveConfigItems(FILE *fp)
{
    INDI::DefaultDevice::saveConfigItems(fp);
    
    ConnectionStringsTP.save(fp);
    
    return true;
}

void SafetyMonitor::parseConnectionStrings()
{
    std::string connStr = ConnectionStringsTP[0].getText();
    
    if (connStr.empty())
    {
        LOG_WARN("No connection strings configured");
        return;
    }

    // Split by comma to get individual device entries
    // Format: DeviceName@host:port or just DeviceName (use localhost:7624)
    // Device names can contain spaces, @ separates name from connection
    // Multiple devices separated by commas
    // Examples: 
    //   "Open Weather Map, UPS@localhost:7624"
    //   "Weather Station@192.168.1.5, Power Supply@192.168.1.10:7624"
    
    std::istringstream iss(connStr);
    std::string deviceEntry;
    
    while (std::getline(iss, deviceEntry, ','))
    {
        // Trim leading/trailing whitespace
        size_t start = deviceEntry.find_first_not_of(" \t\n\r");
        size_t end = deviceEntry.find_last_not_of(" \t\n\r");
        
        if (start == std::string::npos)
            continue; // Empty entry
            
        deviceEntry = deviceEntry.substr(start, end - start + 1);
        
        if (deviceEntry.empty())
            continue;
        
        // Parse: DeviceName@host:port
        // The @ symbol separates device name from connection info
        std::string deviceName, host;
        int port = 7624; // Default INDI port
        
        size_t atPos = deviceEntry.find('@');
        if (atPos != std::string::npos)
        {
            // Has explicit host
            deviceName = deviceEntry.substr(0, atPos);
            std::string hostPort = deviceEntry.substr(atPos + 1);
            
            // Trim trailing space from device name
            size_t nameEnd = deviceName.find_last_not_of(" \t");
            if (nameEnd != std::string::npos)
                deviceName = deviceName.substr(0, nameEnd + 1);
            
            // Parse host:port
            size_t colonPos = hostPort.find(':');
            if (colonPos != std::string::npos)
            {
                // Has explicit port
                host = hostPort.substr(0, colonPos);
                try {
                    port = std::stoi(hostPort.substr(colonPos + 1));
                } catch (...) {
                    LOGF_ERROR("Invalid port in '%s', using default 7624", deviceEntry.c_str());
                }
            }
            else
            {
                // Only host, no port
                host = hostPort;
            }
            
            // Trim host
            size_t hostStart = host.find_first_not_of(" \t");
            size_t hostEnd = host.find_last_not_of(" \t");
            if (hostStart != std::string::npos)
                host = host.substr(hostStart, hostEnd - hostStart + 1);
        }
        else
        {
            // No @ symbol, just device name (use localhost)
            deviceName = deviceEntry;
            host = "localhost";
        }
        
        if (deviceName.empty())
        {
            LOGF_WARN("Empty device name in entry: %s", deviceEntry.c_str());
            continue;
        }
        
        LOGF_INFO("Configuring connection to '%s'@%s:%d", 
                  deviceName.c_str(), host.c_str(), port);

        // Create client with callback to update overall status
        auto client = std::make_unique<SafetyMonitorClient>(
            deviceName,
            [this]() { this->updateOverallStatus(); }
        );
        
        if (client->connectToServer(host, port))
        {
            m_Clients.push_back(std::move(client));
        }
        else
        {
            LOGF_ERROR("Failed to create client for %s", deviceName.c_str());
        }
    }

    if (m_Clients.empty())
    {
        LOG_WARN("No valid device connections configured");
    }
}

void SafetyMonitor::rebuildSafetyStatusProperty()
{
    // Delete existing property if defined
    if (SafetyStatusLP.count() > 0)
    {
        deleteProperty(SafetyStatusLP);
    }

    // Disconnect and clear all existing clients
    for (auto &client : m_Clients)
    {
        client->disconnectServer();
    }
    m_Clients.clear();

    // Parse connection strings and create new clients
    parseConnectionStrings();

    SafetyStatusLP.resize(0);

    // Build SafetyStatusLP with one element per device
    for (auto &client : m_Clients)
    {
        INDI::WidgetLight oneLight;
        oneLight.fill(client->getMonitoredDeviceName().c_str(), 
                     client->getMonitoredDeviceName().c_str(), 
                     IPS_IDLE);
        SafetyStatusLP.push(std::move(oneLight));
    }

    // Define the new property if connected and has elements
    if (isConnected() && SafetyStatusLP.count() > 0)
    {
        defineProperty(SafetyStatusLP);
        
        // Initial status update
        updateOverallStatus();
    }
}

void SafetyMonitor::updateOverallStatus()
{
    if (SafetyStatusLP.count() == 0)
        return;

    // Start with IDLE as the base state
    IPState worstState = IPS_IDLE;

    // Update each device's status and find worst case
    for (size_t i = 0; i < m_Clients.size() && i < SafetyStatusLP.count(); i++)
    {
        auto &client = m_Clients[i];
        auto &light = SafetyStatusLP[i];

        if (client->isDeviceOnline() && client->hasSafetyStatus())
        {
            IPState deviceState = client->getSafetyStatus();
            light.setState(deviceState);

            // Track worst state (higher enum value = worse)
            if (deviceState > worstState)
                worstState = deviceState;
        }
        else
        {
            // Device offline or no status yet
            light.setState(IPS_IDLE);
        }
    }

    // Apply override if enabled
    if (OverrideSP[0].getState() == ISS_ON)
    {
        worstState = IPS_OK;
        LOGF_DEBUG("Safety override active, forcing status to OK (actual worst: %d)", worstState);
    }

    // Set overall property state to worst case
    SafetyStatusLP.setState(worstState);
    SafetyStatusLP.apply();

    LOGF_DEBUG("Overall safety status updated: %d", worstState);
}
