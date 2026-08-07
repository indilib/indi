/*
    INDI driver for IKunFocuser
    Copyright (C) 2026 IKunFocuser contributors
    SPDX-License-Identifier: LGPL-2.1-or-later

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
    Lesser General Public License for more details.
*/

#pragma once

#include "ikunfocuser_protocol.h"
#include "indifocuser.h"

#include <cstddef>
#include <cstdint>

class IKunFocuser : public INDI::Focuser
{
    public:
        IKunFocuser();
        ~IKunFocuser() override = default;

        const char *getDefaultName() override;
        bool initProperties() override;
        bool updateProperties() override;
        bool ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n) override;
        bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n) override;

    protected:
        bool Handshake() override;
        IPState MoveAbsFocuser(uint32_t targetTicks) override;
        IPState MoveRelFocuser(FocusDirection dir, uint32_t ticks) override;
        bool AbortFocuser() override;
        bool SyncFocuser(uint32_t ticks) override;
        bool ReverseFocuser(bool enabled) override;
        bool SetFocuserMaxPosition(uint32_t ticks) override;
        bool SetFocuserSpeed(int speed) override;
        void TimerHit() override;

    private:
        enum
        {
            HOLD_ON,
            HOLD_OFF
        };

        enum
        {
            FIRMWARE_VERSION,
            CONTROLLER_MODEL
        };

        bool sendCommand(const char *command, char *response, std::size_t responseSize, bool silent = false);
        bool readMotionStatus(IKunFocuserProtocol::MotionStatus &status, bool silent = false);
        bool readFullState();
        bool setAcceleration(uint32_t acceleration);
        bool setHold(bool enabled);
        void applyMaximumPosition(uint32_t maximum);
        void applyMotionStatus(const IKunFocuserProtocol::MotionStatus &status);
        void markMotionAlert();

        INDI::PropertyNumber AccelerationNP { 1 };
        INDI::PropertySwitch HoldSP { 2 };
        INDI::PropertyNumber TemperatureNP { 1 };
        INDI::PropertyText FirmwareTP { 2 };

        uint32_t m_LastPosition { 0 };
        uint32_t m_FixedSpeed { 300 };
        uint8_t m_CommunicationFailures { 0 };
        uint8_t m_TemperaturePollCounter { 0 };
        int m_FirmwareVersion { 0 };

        static constexpr std::size_t RESPONSE_SIZE = 1024;
        static constexpr uint8_t IO_TIMEOUT_SECONDS = 3;
        static constexpr uint8_t MAX_COMMUNICATION_FAILURES = 3;
};
