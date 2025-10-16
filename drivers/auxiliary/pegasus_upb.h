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
#include "indipowerinterface.h" // Include the new power interface
#include <stdint.h>

namespace Connection
{
class Serial;
}

class PegasusUPB : public INDI::DefaultDevice, public INDI::FocuserInterface, public INDI::WeatherInterface,
    public INDI::PowerInterface
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

        // Power Interface Overrides
        virtual bool SetPowerPort(size_t port, bool enabled) override;
        virtual bool SetPWMPort(size_t port, bool enabled, double dutyCycle) override;
        virtual bool SetVariablePort(size_t port, bool enabled, double voltage) override;
        virtual bool SetLEDEnabled(bool enabled) override;
        virtual bool SetAutoDewEnabled(size_t port, bool enabled) override;
        virtual bool CyclePower() override;

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
        INDI::PropertySwitch RebootSP {1};

        // Power Sensors
        INDI::PropertyNumber PowerSensorsNP {3};
        enum
        {
            SENSOR_VOLTAGE,
            SENSOR_CURRENT,
            SENSOR_POWER,
        };

        // Power Consumption
        INDI::PropertyNumber PowerConsumptionNP {3};
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
        INDI::PropertySwitch PowerCycleAllSP {2};
        enum
        {
            POWER_CYCLE_OFF,
            POWER_CYCLE_ON,
        };

        // Turn on/off power
        INDI::PropertySwitch PowerControlSP {4};
        enum
        {
            POWER_CONTROL_1,
            POWER_CONTROL_2,
            POWER_CONTROL_3,
            POWER_CONTROL_4
        };

        // Rename the power controls above
        INDI::PropertyText PowerControlsLabelsTP {4};
        enum
        {
            POWER_LABEL_1,
            POWER_LABEL_2,
            POWER_LABEL_3,
            POWER_LABEL_4
        };

        // Current Draw
        INDI::PropertyNumber PowerCurrentNP {4};
        enum
        {
            POWER_CURRENT_1,
            POWER_CURRENT_2,
            POWER_CURRENT_3,
            POWER_CURRENT_4
        };

        // Select which power is ON on bootup
        INDI::PropertySwitch PowerOnBootSP {4};
        enum
        {
            POWER_PORT_1,
            POWER_PORT_2,
            POWER_PORT_3,
            POWER_PORT_4
        };

        // Overcurrent status
        INDI::PropertyLight OverCurrentLP {7};
        enum
        {
            DEW_A,
            DEW_B,
            DEW_C
        };

        // Power LED
        INDI::PropertySwitch PowerLEDSP {2};
        enum
        {
            POWER_LED_ON,
            POWER_LED_OFF,
        };

        // Adjustable Output
        INDI::PropertyNumber AdjustableOutputNP {1};

        ////////////////////////////////////////////////////////////////////////////////////
        /// Dew Group
        ////////////////////////////////////////////////////////////////////////////////////

        // Auto Dew v1
        INDI::PropertySwitch AutoDewSP {2};

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
        INDI::PropertyText DewControlsLabelsTP {3};
        enum
        {
            DEW_LABEL_1,
            DEW_LABEL_2,
            DEW_LABEL_3
        };

        // Auto Dew v2 Aggressiveness

        enum
        {
            AUTO_DEW_AGG,
        };

        INDI::PropertyNumber AutoDewAggNP {1};

        // Dew PWM
        INDI::PropertyNumber DewPWMNP {3};

        // Current Draw
        INDI::PropertyNumber DewCurrentDrawNP {3};

        ////////////////////////////////////////////////////////////////////////////////////
        /// USB
        ////////////////////////////////////////////////////////////////////////////////////

        // Turn on/off usb ports 1-5 (v1)
        INDI::PropertySwitch USBControlSP{2};

        // Turn on/off usb ports 1-6 (v2)
        INDI::PropertySwitch USBControlV2SP {6};
        enum
        {
            PORT_1,
            PORT_2,
            PORT_3,
            PORT_4,
            PORT_5,
            PORT_6
        };

        // USB Port Status (1-6)
        INDI::PropertyLight USBStatusLP {6};

        // Rename the USB controls above
        INDI::PropertyText USBControlsLabelsTP {6};
        enum
        {
            USB_LABEL_1,
            USB_LABEL_2,
            USB_LABEL_3,
            USB_LABEL_4,
            USB_LABEL_5,
            USB_LABEL_6
        };

        ////////////////////////////////////////////////////////////////////////////////////
        /// Focuser
        ////////////////////////////////////////////////////////////////////////////////////

        // Focuser speed
        INDI::PropertyNumber FocuserSettingsNP {1};
        enum
        {
            //SETTING_BACKLASH,
            SETTING_MAX_SPEED,
        };

        ////////////////////////////////////////////////////////////////////////////////////
        /// USB
        ////////////////////////////////////////////////////////////////////////////////////
        INDI::PropertyText FirmwareTP {2};
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
