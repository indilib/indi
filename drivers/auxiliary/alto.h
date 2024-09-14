/*
    ALTO driver
    Copyright (C) 2023 Jasem Mutlaq
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

#pragma once

#include "indidustcapinterface.h"
#include "defaultdevice.h"
#include "../focuser/primalucacommandset.h"

class ALTO : public INDI::DefaultDevice, public INDI::DustCapInterface
{
    public:
        ALTO();
        virtual ~ALTO() override = default;

        const char *getDefaultName() override;
        virtual bool initProperties() override;
        virtual bool updateProperties() override;

        virtual bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n) override;
        virtual bool ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n) override;

    protected:
        bool Handshake();


        // From Dust Cap
        virtual IPState ParkCap() override;
        virtual IPState UnParkCap() override;
        virtual IPState AbortCap() override;

        virtual void TimerHit() override;
        virtual bool saveConfigItems(FILE *fp) override;

    private:
        INDI::PropertySwitch CalibrateToggleSP {2};

        // Position
        INDI::PropertyNumber PositionNP {1};

        INDI::PropertySwitch MotionSpeedSP {2};
        enum
        {
            Slow,
            Fast
        };

        INDI::PropertySwitch MotionCommandSP {3};
        enum
        {
            Open,
            Close,
            Stop
        };

        typedef enum
        {
            Idle,
            findClosePosition,
            findOpenPosition,
        } CalibrationStatus;

        CalibrationStatus m_CalibrationStatus {Idle};

        Connection::Serial *serialConnection{ nullptr };
        int PortFD{ -1 };
        std::unique_ptr<PrimalucaLabs::ALTO> m_ALTO;
        uint8_t m_TargetPosition {0};
};
