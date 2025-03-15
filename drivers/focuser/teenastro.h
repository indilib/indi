/*
    TeenAstro Focuser
    Copyright (C) 2021 Markus Noga

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

#ifndef TEENASTRO_FOCUSER_H
#define TEENASTRO_FOCUSER_H

#include "indifocuser.h"

class TeenAstroFocuser : public INDI::Focuser
{
    public:
        TeenAstroFocuser();
        ~TeenAstroFocuser();

        const char * getDefaultName();
        virtual bool Handshake();

        virtual bool ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n);
        virtual bool ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n);

    protected:
        // Helper function to update NP with the values if res is true and display status IPS_OK, else display status IPS_ALERT. Returns res for convenience.
        bool ISNewNumberHelper(INDI::PropertyNumber &NP, double values[], char *names[], int n, bool res);

        virtual bool initProperties();

        // Initializes value ranges and steps for position UI controls based on maxPos.
        // Does not force redraw via deleteMainControlProperties() and defineMainControlProperties()
        void initPositionPropertiesRanges(uint32_t maxPos);

        virtual bool updateProperties();
        void defineMainControlProperties();
        void defineOtherProperties();
        void deleteMainControlProperties();
        void deleteOtherProperties();

        virtual IPState MoveAbsFocuser(uint32_t ticks);
        virtual IPState MoveRelFocuser(FocusDirection dir, uint32_t ticks);
        virtual bool AbortFocuser();
        virtual void TimerHit();

        // Syncs device focuser position to given value. Returns true on success else false.
        virtual bool SyncFocuser(uint32_t value);

        bool SetFocuserMaxPosition(uint32_t value);

        bool ReverseFocuser(bool enable);

        bool saveConfigItems(FILE *fp);


        // Sends a command to the focuser. Returns true on success and false on failure.
        bool send(const char *const msg);

        // Sends a command to the focuser, and waits for a response terminated by '#'. Returns true on success and false on failure.
        bool sendAndReceive(const char *const msg, char *resp, int bufsize);

        // Sends a command to the focuser, and waits for a single character response. Returns true if successful and response is '1', otherwise false.
        bool sendAndReceiveBool(const char *const msg);

        // Sends a command to the focuser, and expects a timeout. Returns true if timeout happens, else false
        bool sendAndExpectTimeout(const char *const msg, char *resp, int bufsize);

        // Updates device version string from device
        bool updateDeviceVersion();

        // Updates current state from device, consisting of position, speed, and temperature. Returns true on success and false on failure.
        bool updateState();

        // Returns true if focuser is moving, else false.
        bool isMoving();

        // Updates motion configuration from device. Returns true on success and false on failure.
        bool updateMotionConfig();

        // Sets given configuration item on to device to provided value in device native units. Returns true on success and false on failure.
        bool setConfigItem(char item, uint32_t deviceValue);
        bool setParkPos(uint32_t value);
        bool SetFocuserSpeed(int value); // sets TeenAstro GoTo speed
        bool setManualSpeed(uint32_t value);
        bool setGoToAcc(uint32_t value);
        bool setManualAcc(uint32_t value);
        bool setManualDec(uint32_t value);

        // Updates motor configuration from device. Returns true on success and false on failure.
        bool updateMotorConfig();

        bool setMotorMicrosteps(uint32_t value);
        bool setMotorResolution(uint32_t value);
        bool setMotorCurrent(uint32_t value);
        bool setMotorStepsPerRevolution(uint32_t value);

        // Starts movement of focuser towards the given absolute position. Returns immediately, true on success else false.
        bool goTo(uint32_t position);

        // Starts movement of focuser towards the parking position. Returns immediately, true on success else false.
        bool goToPark();

        // Stop focuser movement. Returns immediately, true on success else false.
        bool stop();

        // Reboots the device microcontroller
        bool rebootDevice();

        // Clears the device EEPROM
        bool eraseDeviceEEPROM();

        // Main control tab
        //

        // Focuser park position
        INDI::PropertyNumber CfgParkPosNP {1};

        // Go to park button
        ISwitch GoToParkS[1];
        ISwitchVectorProperty GoToParkSP;

        // Focuser max position is inherited from superclass as FocusMaxPosN, FocusMaxPosNP
        // Sync current focuser position to given value inherited from superclass as FocusSyncN, FocusSyncNP

        // Current focuser speed
        INumber CurSpeedN[1];
        INumberVectorProperty CurSpeedNP;

        // Focuser temperature
        INumber TempN[1];
        INumberVectorProperty TempNP;

        // Focuser current position is inherited from superclass as FocusAbsPosN, FocusAbsPosNP


        // Focuser configuration tab: motion configuration
        //

        // Device version string
        IText DeviceVersionT[1];
        ITextVectorProperty DeviceVersionTP;

        // Focuser goto speed: FocusSpeedNP and FocusSpeedN[1] from superclass

        // Focuser manual speed
        INDI::PropertyNumber CfgManualSpeedNP {1};

        // Go-to acceleration
        INDI::PropertyNumber CfgGoToAccNP {1};
        // INumberVectorProperty CfgGoToAccNP;

        // Manual acceleration
        INDI::PropertyNumber CfgManualAccNP {1};

        // Manual deceleration
        INDI::PropertyNumber CfgManualDecNP {1};


        // Focuser configuration tab: motor configuration
        //

        // Reverse motor direction: FocusReverseSP and FocusReverseS[2] from superclass. [0] is enable, [1] is disable (i.e. normal)

        // Configuration element: Motor microsteps
        enum
        {
            TAF_MICROS_4, TAF_MICROS_8, TAF_MICROS_16, TAF_MICROS_32, TAF_MICROS_64, TAF_MICROS_128, TAF_MICROS_N,
        };
        ISwitch CfgMotorMicrostepsS[TAF_MICROS_N] {};
        ISwitchVectorProperty CfgMotorMicrostepsSP;

        // Configuration element: Impulse resolution (???)
        INDI::PropertyNumber CfgMotorResolutionNP {1};

        // Configuration element: Motor current
        INDI::PropertyNumber CfgMotorCurrentNP {1};

        // Configuration element: Motor steps per revolution
        INDI::PropertyNumber CfgMotorStepsPerRevolutionNP {1};

        // Reboot device microcontroller button
        ISwitch RebootDeviceS[1];
        ISwitchVectorProperty RebootDeviceSP;

        // Erase device EEPROM button
        ISwitch EraseEEPROMS[1];
        ISwitchVectorProperty EraseEEPROMSP;
};

#endif // TEENASTRROFOCUSER_H
