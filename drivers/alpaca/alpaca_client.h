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

#pragma once

#include "baseclient.h"
#include "basedevice.h"
#include "indiproperty.h"

class DeviceManager;

class AlpacaClient : public INDI::BaseClient
{
    public:
        AlpacaClient(DeviceManager* deviceManager);
        ~AlpacaClient();

        // Connect to INDI server
        bool connectServer() override;
        bool disconnectServer(int exist_code = 0) override;

    protected:
        // BaseClient overrides
        void newDevice(INDI::BaseDevice dp) override;
        void removeDevice(INDI::BaseDevice dp) override;
        void newProperty(INDI::Property property) override;
        void removeProperty(INDI::Property property) override;
        void updateProperty(INDI::Property property) override;
        void serverConnected() override;
        void serverDisconnected(int exitCode) override;

    private:
        DeviceManager* m_DeviceManager;
};
