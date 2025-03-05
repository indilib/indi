/*******************************************************************************
 Universal Roll Off Roof driver.
 Copyright(c) 2024 Jasem Mutlaq. All rights reserved.

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Library General Public
 License version 2 as published by the Free Software Foundation.
 .
 This library is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 Library General Public License for more details.
 .
 You should have received a copy of the GNU Library General Public License
 along with this library; see the file COPYING.LIB.  If not, write to
 the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 Boston, MA 02110-1301, USA.
*******************************************************************************/

#pragma once

#include "indidome.h"
#include "inditimer.h"
#include "universal_ror_client.h"

class UniversalROR : public INDI::Dome
{
    public:
        UniversalROR();
        virtual ~UniversalROR() = default;

        virtual bool initProperties() override;
        const char *getDefaultName() override;
        bool updateProperties() override;

        virtual bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n) override;
        virtual bool ISNewText(const char * dev, const char * name, char * texts[], char * names[], int n) override;
        virtual bool saveConfigItems(FILE *fp) override;
        virtual bool ISSnoopDevice(XMLEle *root) override;

    protected:
        bool Connect() override;
        bool Disconnect() override;

        void TimerHit() override;

        virtual IPState Move(DomeDirection dir, DomeMotionCommand operation) override;
        virtual IPState Park() override;
        virtual IPState UnPark() override;
        virtual bool Abort() override;
        virtual void ActiveDevicesUpdated() override;

    private:
        bool setupParms();
        bool syncIndexes();
        std::vector<uint8_t> extract(const std::string &text);
        void checkConnectionStatus();
        bool m_FullOpenLimitSwitch {false}, m_FullClosedLimitSwitch {false};

        INDI::PropertyText InputTP {2};
        INDI::PropertyLight LimitSwitchLP {2};
        enum
        {
            FullyOpened,
            FullyClosed
        };

        INDI::PropertyText OutputTP {2};
        enum
        {
            OpenRoof,
            CloseRoof
        };

        std::vector<uint8_t> m_OutputOpenRoof, m_OutputCloseRoof;
        std::vector<uint8_t> m_InputFullyOpened, m_InputFullyClosed;
        std::unique_ptr<UniversalRORClient> m_Client;

        // Connection tracking
        int m_ConnectionAttempts {0};
        static constexpr int MAX_CONNECTION_ATTEMPTS {5}; // 5 attempts * 5 seconds = 25 seconds max wait time
        INDI::Timer m_ConnectionTimer;
};
