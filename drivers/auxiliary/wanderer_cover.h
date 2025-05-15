/*******************************************************************************
  Copyright(c) 2025 Jérémie Klein. All rights reserved.

  Simple GPS Simulator

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
#include "indilightboxinterface.h"
#include "indidustcapinterface.h"

#include <stdint.h>

namespace Connection
{
class Serial;
}

class WandererCover : public INDI::DefaultDevice, public INDI::LightBoxInterface, public INDI::DustCapInterface
{
    public:
        WandererCover();
        virtual ~WandererCover() = default;

        virtual bool initProperties() override;
        virtual void ISGetProperties(const char *dev) override;
        virtual bool updateProperties() override;

        virtual bool ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n) override;
        virtual bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n) override;
        virtual bool ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n) override;
        virtual bool ISSnoopDevice(XMLEle *root) override;

        // Cover state
        bool isCoverOpen { false };
        bool isCoverCurrentlyOpen() const { return isCoverOpen; }

        // Light box state
        bool isLightOn { false };
        bool isLightCurrentlyOn() const { return isLightOn; }

    protected:
        const char *getDefaultName() override;

        virtual bool saveConfigItems(FILE *fp) override;

        // From Dust Cap
        virtual IPState ParkCap() override;
        virtual IPState UnParkCap() override;

        // From Light Box
        virtual bool SetLightBoxBrightness(uint16_t value) override;
        virtual bool EnableLightBox(bool enable) override;

        virtual void TimerHit() override;

    private:
        enum
        {
            SET_CURRENT_POSITION_OPEN,
            SET_CURRENT_POSITION_CLOSE
        };

        enum
        {
            PLUS_1_DEGREE,
            PLUS_10_DEGREE,
            PLUS_50_DEGREE,
        };

        enum
        {
            MINUS_1_DEGREE,
            MINUS_10_DEGREE,
            MINUS_50_DEGREE,
        };

        bool sendCommand(std::string command, char *response, bool waitForAnswer);
        bool getStartupData();

        bool Handshake();

        // Dust cap
        void updateCoverStatus(char* res);
        void setParkCapStatusAsClosed();
        void setParkCapStatusAsOpen();
        IPState moveDustCap(int degrees);
        bool setCurrentPositionToOpenPosition();
        bool setCurrentPositionToClosedPosition();
        bool processConfigurationButtonSwitch(const char *dev, const char *name, ISState *states, char *names[], int n);

        // Light box
        bool switchOffLightBox();
        void setLightBoxStatusAsSwitchedOff();
        void setLightBoxBrightnesStatusToValue(uint16_t value);

        // Status
        ITextVectorProperty StatusTP;
        IText StatusT[2] {};

        // Firmware version
        ITextVectorProperty FirmwareTP;
        IText FirmwareT[1] {};

        //Number of steps
        void setNumberOfStepsStatusValue(int value);
        int NumberOfStepsBeetweenOpenAndCloseState = 0;
        int NumberOfDegreesSinceLastOpenPositionSet = 0;

        // Configuration
        // Configure open position
        ISwitchVectorProperty ControlPositionNegativeDegreesConfigurationVP;
        ISwitch ControlPositionNegativeDegreesConfigurationV[3] {};
        ISwitchVectorProperty ControlPositionPositiveDegreesConfigurationVP;
        ISwitch ControlPositionPositiveDegreesConfigurationV[3] {};
        ISwitchVectorProperty DefinePositionConfigurationVP;
        ISwitch DefinePositionConfigurationV[2] {};

        // Check for errors in indi driver
        bool hasWandererSentAnError(char* response);
        void displayConfigurationMessage();

        int PortFD{ -1 };

        Connection::Serial *serialConnection{ nullptr };
};
