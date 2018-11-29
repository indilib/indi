/*******************************************************************************
  Copyright(c) 2018 Jasem Mutlaq. All rights reserved.

  Pegasus Ultimate Power Box

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
#include "indifocuserinterface.h"
#include "indiweatherinterface.h"
#include <stdint.h>

namespace Connection
{
class Serial;
}

class PegasusUPB : public INDI::DefaultDevice, public INDI::FocuserInterface, public INDI::WeatherInterface
{
  public:
    PegasusUPB();

    virtual bool initProperties() override;
    virtual bool updateProperties() override;

    virtual bool ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n) override;
    virtual bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n) override;

  protected:
    const char *getDefaultName() override;
    virtual bool Disconnect() override;

    // Focuser Overrides
    virtual IPState MoveAbsFocuser(uint32_t targetTicks) override;
    virtual IPState MoveRelFocuser(FocusDirection dir, uint32_t ticks) override;
    virtual bool AbortFocuser() override;
    virtual bool ReverseFocuser(bool enabled) override;
    virtual bool SyncFocuser(uint32_t ticks) override;


  private:    
    bool Handshake();
    bool setPowerEnabled(uint8_t port, bool enabled);

    /**
     * @brief sendCommand Send command to unit.
     * @param cmd Command
     * @param res if nullptr, respones is ignored, otherwise read response and store it in the buffer.
     * @return
     */
    bool sendCommand(const char *cmd, char *res);

    int PortFD { -1 };

    Connection::Serial *serialConnection { nullptr };

    ////////////////////////////////////////////////////////////////////////////////////
    /// Power Group
    ////////////////////////////////////////////////////////////////////////////////////

    // Cycle all power on/off
    ISwitch PowerCycleAllS[2];
    ISwitchVectorProperty PowerCycleAllSP;
    enum
    {
        POWER_CYCLE_OFF,
        POWER_CYCLE_ON,
    };

    // Turn on/off power
    ISwitch PowerControlS[4];
    ISwitchVectorProperty PowerControlSP;

    // Rename the power controls above
    IText PowerControlsLabelsT[4] = {};
    ITextVectorProperty PowerControlsLabelsTP;

    // Current Draw
    INumber PowerCurrentN[4];
    INumberVectorProperty PowerCurrentNP;

    // Select which power is ON on bootup
    ISwitch PowerOnBootS[4];
    ISwitchVectorProperty PowerOnBootSP;

    // Overcurrent status
    ILight OverCurrentL[4];
    ILightVectorProperty OverCurrentLP;

    ////////////////////////////////////////////////////////////////////////////////////
    /// Dew Group
    ////////////////////////////////////////////////////////////////////////////////////

    // Auto Dew
    ISwitch AutoDewS[2];
    ISwitchVectorProperty AutoDewSP;
    enum
    {
        AUTO_DEW_ENABLED,
        AUTO_DEW_DISABLED,
    };

    // Dew PWM
    INumber DewPWMN[2];
    INumberVectorProperty DewPWMNP;
    enum
    {
        DEW_PWM_A,
        DEW_PWM_B,
    };

    ////////////////////////////////////////////////////////////////////////////////////
    /// Status
    ////////////////////////////////////////////////////////////////////////////////////

    // Overall power values
    INumber PowerReadingsN[3];
    INumberVectorProperty PowerReadingsNP;
    enum
    {
        READING_VOLTAGE,
        READING_CURRENT,
        READING_POWER,
    };

    ////////////////////////////////////////////////////////////////////////////////////
    /// USB
    ////////////////////////////////////////////////////////////////////////////////////

    // Turn on/off usb ports 1-5
    ISwitch USBControlS[5];
    ISwitchVectorProperty USBControlSP;

    // USB Port Status (1-6)
    ILight USBStatusL[6];
    ILightVectorProperty USBStatusLP;

    ////////////////////////////////////////////////////////////////////////////////////
    /// USB
    ////////////////////////////////////////////////////////////////////////////////////

    // Focuser backlash
    INumber BacklashN[1];
    INumberVectorProperty BacklashNP;

    // Focuser invert
    ISwitch InvertMotorS[2];
    ISwitchVectorProperty InvertMotorSP;
    enum
    {
        INVERT_MOTOR_ENABLED,
        INVERT_MOTOR_DISABLED,
    };



    static constexpr const uint8_t PEGASUS_TIMEOUT {3};
    static constexpr const char *DEW_TAB {"Dew"};
    static constexpr const char *USB_TAB {"USB"};
    static constexpr const char *ENVIRONMENT_TAB {"Environment"};
    static constexpr const char *POWER_TAB {"Power"};
    static constexpr const char *FOCUSER_TAB {"Focuser"};
};
