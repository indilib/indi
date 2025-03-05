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

#pragma once

#include "baseclient.h"
#include "basedevice.h"

class UniversalRORClient : public INDI::BaseClient
{
    public:
        UniversalRORClient(const std::string &input, const std::string &output);
        ~UniversalRORClient();

        bool isConnected()
        {
            return m_InputReady && m_OutputReady;
        }

        const std::string &inputDevice() const
        {
            return m_Input;
        }
        const std::string &outputDevice() const
        {
            return m_Output;
        }

        bool openRoof();
        bool closeRoof();
        bool stop();

        void setOutputOpenRoof(const std::vector<uint8_t> &value)
        {
            m_OutputOpenRoof = value;
        }

        void setOutputCloseRoof(const std::vector<uint8_t> &value)
        {
            m_OutputCloseRoof = value;
        }

        void setInputFullyOpened(const std::vector<uint8_t> &value)
        {
            m_InputFullyOpened = value;
        }

        void setInputFullyClosed(const std::vector<uint8_t> &value)
        {
            m_InputFullyClosed = value;
        }

        void setFullyOpenedCallback(std::function<void(bool)> callback)
        {
            m_FullyOpenedCallback = callback;
        }

        void setFullyClosedCallback(std::function<void(bool)> callback)
        {
            m_FullyClosedCallback = callback;
        }

        void setConnectionCallback(std::function<void(bool)> callback)
        {
            m_ConnectionCallback = callback;
        }

        bool syncFullyOpenedState();
        bool syncFullyClosedState();

    protected:
        virtual void newDevice(INDI::BaseDevice dp) override;
        virtual void newProperty(INDI::Property property) override;
        virtual void updateProperty(INDI::Property property) override;
        virtual void serverDisconnected(int exitCode) override;

    private:
        std::string m_Input, m_Output;
        bool m_InputReady {false}, m_OutputReady {false};
        std::vector<uint8_t> m_OutputOpenRoof, m_OutputCloseRoof;
        std::vector<uint8_t> m_InputFullyOpened, m_InputFullyClosed;
        std::function<void(bool)> m_FullyOpenedCallback, m_FullyClosedCallback, m_ConnectionCallback;
};
