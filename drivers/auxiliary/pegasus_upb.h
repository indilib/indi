/*******************************************************************************
  Copyright(c) 2018 Jasem Mutlaq. All rights reserved.

  Pegasus Ultimate Power Box (v1 and v2)

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

        enum
        {
            UPB_V1,
            UPB_V2
        };

        virtual bool initProperties() override;
        virtual bool updateProperties() override;

        virtual bool ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n) override;
        virtual bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n) override;
        virtual bool ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n) override;

    protected:
        const char *getDefaultName() override;
        virtual bool saveConfigItems(FILE *fp) override;

        // Event loop
        virtual void TimerHit() override;

        // Focuser Overrides
        virtual IPState MoveAbsFocuser(uint32_t targetTicks) override;
        virtual IPState MoveRelFocuser(FocusDirection dir, uint32_t ticks) override;
        virtual bool AbortFocuser() override;
        virtual bool ReverseFocuser(bool enabled) override;
        virtual bool SyncFocuser(uint32_t ticks) override;

        virtual bool SetFocuserBacklash(int32_t steps) override;
        virtual bool SetFocuserBacklashEnabled(bool enabled) override;

        // Weather Overrides
        virtual IPState updateWeather() override
        {
            return IPS_OK;
        }


    private:
        bool Handshake();

        // Get Data
        bool setupParams();
        bool sendFirmware();
        bool getSensorData();
        bool getPowerData();
        bool getStepperData();
        bool getDewAggData();
        std::vector<std::string> split(const std::string &input, const std::string &regex);

        // Device Control
        bool reboot();

        // Power
        bool setPowerEnabled(uint8_t port, bool enabled);
        bool setPowerLEDEnabled(bool enabled);
        bool setPowerOnBoot();
        bool getPowerOnBoot();
        bool setAdjustableOutput(uint8_t voltage);

        // Dew
        bool setAutoDewEnabled(bool enabled);
        bool toggleAutoDewV2();
        bool setAutoDewAgg(uint8_t value);
        bool setDewPWM(uint8_t id, uint8_t value);

        // USB
        bool setUSBHubEnabled(bool enabled);
        bool setUSBPortEnabled(uint8_t port, bool enabled);

        // Focuser
        bool setFocuserMaxSpeed(uint16_t maxSpeed);
        //    bool setFocuserBacklash(uint16_t value);
        //    bool setFocuserBacklashEnabled(bool enabled);

        /**
         * @brief sendCommand Send command to unit.
         * @param cmd Command
         * @param res if nullptr, respones is ignored, otherwise read response and store it in the buffer.
         * @return
         */
        bool sendCommand(const char *cmd, char *res);

        /**
         * @brief cleanupResponse Removes all spaces
         * @param response buffer
         */
        void cleanupResponse(char *response);

        /**
         * @return Return true if sensor data different from last data
         *
         * If the previous sensor data is empty then this will always
         * return true.
         */
        bool sensorUpdated(const std::vector<std::string> &result, uint8_t start, uint8_t end);

        /**
         * @return Return true if stepper data different from last data.
         *
         * If the previous stepper data is empty then this will always
         * return true.
         */
        bool stepperUpdated(const std::vector<std::string> &result, uint8_t index);

        int PortFD { -1 };
        bool setupComplete { false };

        Connection::Serial *serialConnection { nullptr };

        ////////////////////////////////////////////////////////////////////////////////////
        /// Main Control
        ////////////////////////////////////////////////////////////////////////////////////
        /// Reboot Device
        ISwitch RebootS[1];
        ISwitchVectorProperty RebootSP;

        // Power Sensors
        INumber PowerSensorsN[3];
        INumberVectorProperty PowerSensorsNP;
        enum
        {
            SENSOR_VOLTAGE,
            SENSOR_CURRENT,
            SENSOR_POWER,
        };

        // Power Consumption
        INumber PowerConsumptionN[3];
        INumberVectorProperty PowerConsumptionNP;
        enum
        {
            CONSUMPTION_AVG_AMPS,
            CONSUMPTION_AMP_HOURS,
            CONSUMPTION_WATT_HOURS,
        };

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
        ILight OverCurrentL[7];
        ILightVectorProperty OverCurrentLP;

        // Power LED
        ISwitch PowerLEDS[2];
        ISwitchVectorProperty PowerLEDSP;
        enum
        {
            POWER_LED_ON,
            POWER_LED_OFF,
        };

        // Adjustable Output
        INumber AdjustableOutputN[1];
        INumberVectorProperty AdjustableOutputNP;

        ////////////////////////////////////////////////////////////////////////////////////
        /// Dew Group
        ////////////////////////////////////////////////////////////////////////////////////

        // Auto Dew v1
        ISwitch AutoDewS[2];
        ISwitchVectorProperty AutoDewSP;

        enum
        {
            DEW_PWM_A,
            DEW_PWM_B,
            DEW_PWM_C,
        };

        // Auto Dew v2
        ISwitch AutoDewV2S[3];
        ISwitchVectorProperty AutoDewV2SP;

        // Rename the power controls above
        IText DewControlsLabelsT[3] = {};
        ITextVectorProperty DewControlsLabelsTP;

        // Auto Dew v2 Aggressiveness

        enum
        {
            AUTO_DEW_AGG,
        };

        INumber AutoDewAggN[1];
        INumberVectorProperty AutoDewAggNP;

        // Dew PWM
        INumber DewPWMN[3];
        INumberVectorProperty DewPWMNP;

        // Current Draw
        INumber DewCurrentDrawN[3];
        INumberVectorProperty DewCurrentDrawNP;

        ////////////////////////////////////////////////////////////////////////////////////
        /// USB
        ////////////////////////////////////////////////////////////////////////////////////

        // Turn on/off usb ports 1-5 (v1)
        ISwitch USBControlS[2];
        ISwitchVectorProperty USBControlSP;

        // Turn on/off usb ports 1-6 (v2)
        ISwitch USBControlV2S[6];
        ISwitchVectorProperty USBControlV2SP;

        // USB Port Status (1-6)
        ILight USBStatusL[6];
        ILightVectorProperty USBStatusLP;

        // Rename the USB controls above
        IText USBControlsLabelsT[6] = {};
        ITextVectorProperty USBControlsLabelsTP;

        ////////////////////////////////////////////////////////////////////////////////////
        /// Focuser
        ////////////////////////////////////////////////////////////////////////////////////

        // Focuser speed
        INumber FocuserSettingsN[1];
        INumberVectorProperty FocuserSettingsNP;
        enum
        {
            //SETTING_BACKLASH,
            SETTING_MAX_SPEED,
        };

        ////////////////////////////////////////////////////////////////////////////////////
        /// USB
        ////////////////////////////////////////////////////////////////////////////////////
        ITextVectorProperty FirmwareTP;
        IText FirmwareT[2] {};
        enum
        {
            FIRMWARE_VERSION,
            FIRMWARE_UPTIME
        };


        std::vector<std::string> lastSensorData, lastPowerData, lastStepperData, lastDewAggData;
        bool focusMotorRunning { false };
        char stopChar { 0xD };
        uint8_t version { UPB_V1 };

        static constexpr const uint8_t PEGASUS_TIMEOUT {3};
        static constexpr const uint8_t PEGASUS_LEN {128};
        static constexpr const char *DEW_TAB {"Dew"};
        static constexpr const char *USB_TAB {"USB"};
        static constexpr const char *ENVIRONMENT_TAB {"Environment"};
        static constexpr const char *POWER_TAB {"Power"};
        static constexpr const char *FIRMWARE_TAB {"Firmware"};
};
