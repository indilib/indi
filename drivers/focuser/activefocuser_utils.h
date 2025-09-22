/*
    ActiveFocuser driver for Takahashi CCA-250 and Mewlon-250/300CRS

    Driver written by Alvin FREY <https://afrey.fr> for Optique Unterlinden and Takahashi Europe

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

#include <map>
#include <string>

#ifdef _USE_SYSTEM_HIDAPILIB
#include <hidapi/hidapi.h>
#else
#include <indi_hidapi.h>
#endif

#include <mutex>
#include <future>
#include <cstring>
#include <chrono>
#include <cmath>
#include <memory>
#include <indifocuser.h>
#include "activefocuser.h"

class ActiveFocuserUtils
{

    public :

        typedef enum
        {
            ZERO,
            RELEASE,
            FREE,
            AUTO,
            MOVE,
            STOP,
            FAN_ON,
            FAN_OFF,
            RESET,
            DUMMY
        } Commands;

        static const std::map<Commands, unsigned char> CommandsMap;

        class Parser
        {
            public :
                static int Get32(const unsigned char *buffer, int position);
                static int Get16(const unsigned char *buffer, int position);
                static double TicksToMillimeters(int ticks);
                static int MillimetersToTicks(double millimeters);
                static void PrintFrame(const unsigned char *buffer);
                static void PrintBasicDeviceData(const unsigned char *buffer);
            private:
                Parser() = delete;
                ~Parser() = delete;
                void operator=(const Parser &) = delete;

        };

        class Poller
        {
            public:
                static Poller *GetInstance(hid_device &hid_handle);
                bool IsRunning;
                bool Start();
                bool Stop();
            protected:
                Poller(hid_device &hid_handle) : hid_handle_(hid_handle) {}
                ~Poller() {}
                hid_device &hid_handle_;
            private:
                Poller(Poller &other) = delete;
                void operator=(const Poller &) = delete;
                static Poller *pinstance_;
                static std::mutex mutex_;
        };

        class SystemState
        {

            public:
                static int GetCurrentPositionStep();
                static void SetCurrentPositionStep(int currentPositionStep);
                static double GetCurrentPosition();
                static void SetCurrentPosition(double currentPosition);
                static void SetIsOrigin(bool isOrigin);
                static bool GetIsMoving();
                static void SetIsMoving(bool isMoving);
                static bool GetIsFanOn();
                static void SetIsFanOn(bool isFanOn);
                static bool GetIsHold();
                static void SetIsHold(bool isHold);
                static char * GetHardwareRevision();
                static void SetHardwareRevision(char * hardwareRevision);
                static int GetImmpp();
                static void SetImmpp(int immpp);
                static int GetSpan();
                static void SetSpan(int span);
                static double GetMmpp();
                static void SetMmpp(double mmpp);
                static double GetAirTemperature();
                static void SetAirTemperature(double airTemperature);
                static double GetTubeTemperature();
                static void SetTubeTemperature(double tubeTemperature);
                static double GetMirrorTemperature();
                static void SetMirrorTemperature(double mirrorTemperature);
            private:
                SystemState() = delete;
                ~SystemState() = delete;
                void operator=(const SystemState &) = delete;

                bool GetIsOrigin();
        };

    private:
        ActiveFocuserUtils() = delete;
        ~ActiveFocuserUtils() = delete;
        void operator=(const ActiveFocuserUtils &) = delete;

};
