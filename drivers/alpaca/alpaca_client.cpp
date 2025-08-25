/*******************************************************************************
  Copyright(c) 2025 Jasem Mutlaq. All rights reserved.

  INDI Alpaca Client

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

#include "alpaca_client.h"
#include "device_manager.h"
#include "indilogger.h"

AlpacaClient::AlpacaClient(DeviceManager* deviceManager)
    : m_DeviceManager(deviceManager)
{
    DEBUGDEVICE("INDI Alpaca Server", INDI::Logger::DBG_SESSION, "Alpaca client initialized");
}

AlpacaClient::~AlpacaClient()
{
    DEBUGDEVICE("INDI Alpaca Server", INDI::Logger::DBG_SESSION, "Alpaca client destroyed");
}

bool AlpacaClient::connectServer()
{
    DEBUGDEVICE("INDI Alpaca Server", INDI::Logger::DBG_SESSION, "Connecting to INDI server");
    return INDI::BaseClient::connectServer();
}

bool AlpacaClient::disconnectServer(int exit_code)
{
    DEBUGDEVICE("INDI Alpaca Server", INDI::Logger::DBG_SESSION, "Disconnecting from INDI server");
    return INDI::BaseClient::disconnectServer(exit_code);
}

void AlpacaClient::newDevice(INDI::BaseDevice dp)
{
    // Do not process our own driver at all
    if (!strcmp(dp.getDeviceName(), "INDI Alpaca Server"))
        return;
    DEBUGFDEVICE("INDI Alpaca Server", INDI::Logger::DBG_SESSION, "New device: %s", dp.getDeviceName());
    m_DeviceManager->addDevice(dp);
}

void AlpacaClient::removeDevice(INDI::BaseDevice dp)
{
    DEBUGFDEVICE("INDI Alpaca Server", INDI::Logger::DBG_SESSION, "Remove device: %s", dp.getDeviceName());
    m_DeviceManager->removeDevice(dp);
}

void AlpacaClient::newProperty(INDI::Property property)
{
    DEBUGFDEVICE("INDI Alpaca Server", INDI::Logger::DBG_DEBUG, "New property: %s.%s", property.getDeviceName(),
                 property.getName());
    m_DeviceManager->updateDeviceProperty(property);
}

void AlpacaClient::removeProperty(INDI::Property property)
{
    DEBUGFDEVICE("INDI Alpaca Server", INDI::Logger::DBG_DEBUG, "Remove property: %s.%s", property.getDeviceName(),
                 property.getName());
}

void AlpacaClient::updateProperty(INDI::Property property)
{
    DEBUGFDEVICE("INDI Alpaca Server", INDI::Logger::DBG_DEBUG, "Update property: %s.%s", property.getDeviceName(),
                 property.getName());
    m_DeviceManager->updateDeviceProperty(property);
}

void AlpacaClient::serverConnected()
{
    DEBUGDEVICE("INDI Alpaca Server", INDI::Logger::DBG_SESSION, "Connected to INDI server");
}

void AlpacaClient::serverDisconnected(int exitCode)
{
    DEBUGFDEVICE("INDI Alpaca Server", INDI::Logger::DBG_SESSION, "Disconnected from INDI server (exit code: %d)", exitCode);
}
