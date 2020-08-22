/*******************************************************************************
  Copyright(c) 2015 Jasem Mutlaq. All rights reserved.

  INDI Watchdog driver.

  The driver expects a heartbeat from the client every X minutes. If no heartbeat
  is received, the driver executes the shutdown procedures.

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

#include "defaultdevice.h"

class WatchDogClient;

class WatchDog : public INDI::DefaultDevice
{
    public:
        typedef enum
        {
            WATCHDOG_IDLE,
            WATCHDOG_CLIENT_STARTED,
            WATCHDOG_MOUNT_PARKED,
            WATCHDOG_DOME_PARKED,
            WATCHDOG_COMPLETE,
            WATCHDOG_ERROR
        } ShutdownStages;
        typedef enum { PARK_MOUNT, PARK_DOME, EXECUTE_SCRIPT } ShutdownProcedure;
        typedef enum { MOUNT_IGNORED, MOUNT_LOCKS } MountPolicy;

        WatchDog();
        virtual ~WatchDog();

        virtual void ISGetProperties(const char *dev) override;
        virtual bool ISSnoopDevice(XMLEle * root) override;
        virtual bool ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n) override;
        virtual bool ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n) override;
        virtual bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n) override;

    protected:
        virtual bool initProperties() override;

        virtual void TimerHit() override;

        virtual bool Connect() override;
        virtual bool Disconnect() override;
        virtual const char *getDefaultName() override;

        virtual bool saveConfigItems(FILE *fp) override;

    private:
        void parkDome();
        void parkMount();
        void executeScript();

        // Heart Beat to check if client is alive
        INumberVectorProperty HeartBeatNP;
        INumber HeartBeatN[1];

        // Weather threshold
        INumberVectorProperty WeatherThresholdNP;
        INumber WeatherThresholdN[1];

        // Settings
        ITextVectorProperty SettingsTP;
        IText SettingsT[4] {};
        enum {INDISERVER_HOST, INDISERVER_PORT, SHUTDOWN_SCRIPT};

        // Shutdown steps
        ISwitchVectorProperty ShutdownProcedureSP;
        ISwitch ShutdownProcedureS[3];

        // Mount Policy
        ISwitchVectorProperty MountPolicySP;
        ISwitch MountPolicyS[2];

        // Which source should trigger the shutdown?
        ISwitchVectorProperty ShutdownTriggerSP;
        ISwitch ShutdownTriggerS[2];
        enum { TRIGGER_CLIENT, TRIGGER_WEATHER };

        // Active Devices to Snoop on
        ITextVectorProperty ActiveDeviceTP;
        IText ActiveDeviceT[3] {};
        enum { ACTIVE_TELESCOPE, ACTIVE_DOME, ACTIVE_WEATHER };

        // Pointer to client to issue commands to the respective mount and/or dome drivers.
        WatchDogClient *watchdogClient {nullptr};
        // Watchdog timer to ensure heart beat is there
        int32_t m_WatchDogTimer {-1};
        // Weather timer to trigger shutdown if weather remains ALERT for this many seconds.
        int32_t m_WeatherAlertTimer {-1};
        // INDI Port
        uint32_t m_INDIServerPort { 7624 };
        // Weather State
        IPState m_WeatherState { IPS_IDLE };
        // Mount Parked?
        bool m_IsMountParked { false };
        // Dome Parked?
        bool m_IsDomeParked { false };
        // State machine to store where in the shutdown procedure we currently stand
        ShutdownStages m_ShutdownStage;
};
