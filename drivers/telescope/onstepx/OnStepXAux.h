/*
    OnStepX INDI Driver — Auxiliary controller device class (indi_onstepx_aux)
    Copyright (C) 2026 Ed Lee

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

    Top-level device class for OnStepX installations without a mount.
    Composes OnStepXCore (probing), OnStepXDriverStatus and OnStepXWeather;
    focusers (up to 6), the rotator and the aux outputs/USB ports are child
    devices (OnStepXFocuser, OnStepXRotatorDevice, OnStepXPowerDevice).  Mixes in WeatherInterface.  All protocol commands are implemented in the
    individual helper classes.
*/

#pragma once

#include "OnStepXCore.h"
#include "OnStepXDriverStatus.h"
#include "OnStepXFocuser.h"
#include "OnStepXRotatorDevice.h"
#include "OnStepXWeather.h"
#include "OnStepXPowerDevice.h"
#include <defaultdevice.h>
#include <indipropertytext.h>
#include <indiweatherinterface.h>
#include "connectionplugins/connectionserial.h"
#include "connectionplugins/connectiontcp.h"

#include <array>
#include <memory>

class OnStepXAux : public INDI::DefaultDevice,
    public INDI::WeatherInterface
{
    public:
        OnStepXAux();

        virtual const char *getDefaultName() override;
        virtual bool initProperties() override;
        virtual bool updateProperties() override;
        virtual bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n) override;
        virtual bool ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n) override;
        virtual bool ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n) override;
        virtual bool saveConfigItems(FILE *fp) override;

    protected:
        virtual bool Handshake();
        virtual bool Disconnect() override;
        virtual void TimerHit() override;
        virtual IPState updateWeather() override;

    private:
        void createFocusers();
        void disconnectFocusers();
        void pollFocusers();
        void updateRotatorState();
        void pollPower();

        OnStepXCore        m_core;
        OnStepXWeather     m_weather;
        OnStepXDriverStatus m_driverStatus;
        INDI::PropertyText m_firmwareTP { 4 };
        Connection::Serial *m_serialConnection { nullptr };
        Connection::TCP    *m_tcpConnection    { nullptr };

        int  m_pollCount { 0 };

        // WeatherInterface parameters must be added exactly once.
        bool m_weatherParamsAdded { false };

        // Focuser child devices — index slot-1, one per slot in focuserMask.
        OnStepXFocuser::Children m_focusers;
        std::unique_ptr<OnStepXRotatorDevice> m_rotatorDev;   // "<name> Rotator", when present
        std::unique_ptr<OnStepXPowerDevice>   m_powerDev;     // "<name> Power", when present
};
