/*******************************************************************************
  Copyright(c) 2020 Jasem Mutlaq. All rights reserved.
  Copyright(c) 2020 Justin Husted.

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Library General Public
 License version 2 as published by the Free Software Foundation.

 This library is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 Library General Public License for more details.

 You should have received a copy of the GNU Library General Public License
 along with this library; see the file COPYING.LIB.  If not, write to
 the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 Boston, MA 02110-1301, USA.
*******************************************************************************/

#pragma once

#include "indibase/indifilterwheel.h"

class XAGYLWheel : public INDI::FilterWheel
{
    public:
        typedef enum
        {
            INFO_PRODUCT_NAME,
            INFO_FIRMWARE_VERSION,
            INFO_FILTER_POSITION,
            INFO_SERIAL_NUMBER,
            INFO_MAX_SPEED,
            INFO_JITTER,
            INFO_OFFSET,
            INFO_THRESHOLD,
            INFO_MAX_SLOTS,
            INFO_PULSE_WIDTH
        } GET_COMMAND;
        typedef enum
        {
            SET_SPEED, SET_JITTER, SET_THRESHOLD, SET_PULSE_WITDH
        } SET_COMMAND;

        XAGYLWheel();
        virtual ~XAGYLWheel() override;

        virtual bool initProperties() override;
        virtual bool updateProperties() override;

        virtual bool ISNewSwitch(const char *dev, const char *name,
                                 ISState *states, char *names[], int n) override;
        virtual bool ISNewNumber(const char *dev, const char *name,
                                 double values[], char *names[], int n) override;

    protected:
        const char *getDefaultName() override;

        bool Handshake() override;

        bool SelectFilter(int) override;
        bool saveConfigItems(FILE *fp) override;

    private:
        bool setMaximumSpeed(int value);
        bool setRelativeCommand(SET_COMMAND command, int value);

        void initOffset();

        bool getStartupData();
        bool getFirmwareInfo();
        bool getSettingInfo();

        bool getFilterPosition();
        bool getMaximumSpeed();
        bool getJitter();
        bool getThreshold();
        bool getMaxFilterSlots();
        bool getPulseWidth();

        // Calibration offset
        bool getOffset(int filter);
        bool setOffset(int filter, int shift);

        // Reset
        bool reset(int value);

        //////////////////////////////////////////////////////////////////////
        /// Communication Functions
        //////////////////////////////////////////////////////////////////////
        bool receiveResponse(char * res, bool optional = false);
        bool sendCommand(const char * cmd, char * res);
        void hexDump(char * buf, const char * data, int size);

        //////////////////////////////////////////////////////////////////////
        /// Properties
        //////////////////////////////////////////////////////////////////////

        // Firmware info
        ITextVectorProperty FirmwareInfoTP;
        IText FirmwareInfoT[3] {};
        enum
        {
            FIRMWARE_PRODUCT,
            FIRMWARE_VERSION,
            FIRMWARE_SERIAL,
        };

        // Settings
        INumberVectorProperty SettingsNP;
        INumber SettingsN[4];
        enum
        {
            SETTING_SPEED,
            SETTING_JITTER,
            SETTING_THRESHOLD,
            SETTING_PW,
        };

        // Filter Offset
        INumberVectorProperty OffsetNP;
        INumber *OffsetN { nullptr };

        // Reset
        ISwitchVectorProperty ResetSP;
        ISwitch ResetS[4];
        enum
        {
            COMMAND_REBOOT,
            COMMAND_INIT,
            COMMAND_CLEAR_CALIBRATION,
            COMMAND_PERFORM_CALIBRAITON,
        };

        uint8_t m_FirmwareVersion { 0 };

        //////////////////////////////////////////////////////////////////////
        /// Static Helper Values
        //////////////////////////////////////////////////////////////////////
        static constexpr const char * SETTINGS_TAB = "Settings";

        // 0xA is the stop char
        static const char DRIVER_STOP_CHAR { 0xA };

        // Wait up to a maximum of 15 seconds for normal serial input
        static constexpr const int DRIVER_TIMEOUT {15};

        // Some commands optionally return an extra string.
        static constexpr const int OPTIONAL_TIMEOUT {1};

        // Maximum buffer for sending/receving.
        static constexpr const int DRIVER_LEN {64};
};
