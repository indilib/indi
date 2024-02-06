/*
  Skeleton Focuser Driver

  Modify this driver when developing new absolute position
  based focusers. This driver uses serial communication by default
  but it can be changed to use networked TCP/UDP connection as well.

  Copyright(c) 2019 Jasem Mutlaq. All rights reserved.

  Thanks to Rigel Systems, especially Gene Nolan and Leon Palmer,
  for their support in writing this driver.

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
#include "indifocuser.h"
#include <mutex>
#include <chrono>
#include <ctime>

class AllunaTCS2 : public INDI::Focuser //, public INDI::DustCapInterface
{
    public:
        AllunaTCS2();

        virtual bool Handshake() override;
        const char *getDefaultName() override;

        bool initProperties() override;
        bool updateProperties() override;
        void ISGetProperties(const char *dev) override;

        bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n) override;
        bool ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n) override;

    protected:
        // From INDI::DefaultDevice
        void TimerHit() override;
        bool saveConfigItems(FILE *fp) override;

        // From INDI::Focuser
        IPState MoveRelFocuser(FocusDirection dir, uint32_t ticks) override;
        IPState MoveAbsFocuser(uint32_t targetTicks) override;
        bool AbortFocuser() override;

    private:
        // while a multi response action is running (such as GetTemperatures), we cannot process additional commands
        std::mutex tcs;

        // focuser internal state
        bool isFocuserMoving = false;

        // is getTemperature command in progress
        bool isGetTemperature = false;

        // is setDustCover command in progress
        bool isCoverMoving = false;

        // Climate
        INDI::PropertyNumber TemperatureNP{4}; // note: last value is humidity, not temperature.
        INDI::PropertySwitch ClimateControlSP{2};
        typedef enum { AUTO, MANUAL } ClimateControlMode;
        INDI::PropertySwitch PrimaryDewHeaterSP{2}, SecondaryDewHeaterSP{2};
        typedef enum { ON, OFF } DewHeaterMode;
        INDI::PropertyNumber FanPowerNP{1};

        // Focuser
        INDI::PropertySwitch SteppingModeSP{2};

        typedef enum { MICRO = 1, SPEED = 0 } SteppingMode;
        SteppingMode steppingMode=MICRO;

        // Dust cover
        INDI::PropertySwitch CoverSP{2};
        typedef enum { OPEN, CLOSED } CoverMode;


        ///////////////////////////////////////////////////////////////////////////////
        /// Read Data From Controller
        ///////////////////////////////////////////////////////////////////////////////
        bool getTemperature();
        bool getPosition();
        bool getStepping();
        bool getDustCover();
        bool getClimateControl();
        bool getPrimaryDewHeater();
        bool getSecondaryDewHeater();
        bool getFanPower();

        ///////////////////////////////////////////////////////////////////////////////
        /// Write Data to Controller
        ///////////////////////////////////////////////////////////////////////////////
        bool setStepping(SteppingMode mode);
        bool setDustCover(void); // open/close dust cover
        bool setClimateControl(ClimateControlMode mode); // turn on/off climate control
        bool setPrimaryDewHeater(DewHeaterMode mode); // turn on/off climate control
        bool setSecondaryDewHeater(DewHeaterMode mode); // turn on/off climate control
        bool setFanPower(int); // read fan power (between 121=47% and 255=100%, or 0=off)

        ///////////////////////////////////////////////////////////////////////////////
        /// Utility Functions
        ///////////////////////////////////////////////////////////////////////////////
        /**
         * @brief sendCommand Send a string command to device.
         * @param cmd Command to be sent. Can be either NULL TERMINATED or just byte buffer.
         * @param res If not nullptr, the function will wait for a response from the device. If nullptr, it returns true immediately
         * after the command is successfully sent.
         * @param cmd_len if -1, it is assumed that the @a cmd is a null-terminated string. Otherwise, it would write @a cmd_len bytes from
         * the @a cmd buffer.
         * @param res_len if -1 and if @a res is not nullptr, the function will read until it detects the default delimeter DRIVER_STOP_CHAR
         *  up to DRIVER_LEN length. Otherwise, the function will read @a res_len from the device and store it in @a res.
         * @return True if successful, false otherwise.
         */
        bool sendCommand(const char * cmd, char * res = nullptr, int cmd_len = -1, int res_len = -1);
        bool sendCommandNoLock(const char * cmd, char * res = nullptr, int cmd_len = -1, int res_len = -1);
        bool sendCommandOnly(const char * cmd, int cmd_len = -1);
        bool receiveNext(char * res, int res_len = -1);
        void receiveDone();

        /**
         * @brief hexDump Helper function to print non-string commands to the logger so it is easier to debug
         * @param buf buffer to format the command into hex strings.
         * @param data the command
         * @param size length of the command in bytes.
         * @note This is called internally by sendCommand, no need to call it directly.
         */
        void hexDump(char * buf, const char * data, int size);

        /**
         * @return is the focuser in motion?
         */
        bool isMoving();

        /////////////////////////////////////////////////////////////////////////////
        /// Class Variables
        /////////////////////////////////////////////////////////////////////////////
        int32_t m_TargetDiff { 0 };

        /////////////////////////////////////////////////////////////////////////////
        /// Static Helper Values
        /////////////////////////////////////////////////////////////////////////////
        static constexpr const char * FOCUSER_TAB = "Focus";
        static constexpr const char * ROTATOR_TAB = "Rotate";
        static constexpr const char * CLIMATE_TAB = "Climate";
        static constexpr const char * DUSTCOVER_TAB = "Dust Cover";
        // 'LF' is the stop char
        static const char DRIVER_STOP_CHAR { 0x0A };
        // Update temperature every 10x POLLMS. For 500ms, we would
        // update the temperature one every 5 seconds.
        static constexpr const uint8_t DRIVER_TEMPERATURE_FREQ {10};
        // Wait up to a maximum of 3 seconds for serial input
        static constexpr const uint8_t DRIVER_TIMEOUT {5};
        // Maximum buffer for sending/receving.
        static constexpr const uint8_t DRIVER_LEN {64};
};
