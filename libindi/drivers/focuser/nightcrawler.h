/*
    NightCrawler NightCrawler Focuser & Rotator
    Copyright (C) 2017 Jasem Mutlaq (mutlaqja@ikarustech.com)

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

#ifndef NIGHTCRAWLER_H
#define NIGHTCRAWLER_H

#include "indibase/indifocuser.h"

class NightCrawler : public INDI::Focuser
{
    public:

        typedef enum { MOTOR_FOCUS, MOTOR_ROTATOR, MOTOR_AUX } MotorType;

        NightCrawler();
        ~NightCrawler();

        virtual bool Handshake();
        const char * getDefaultName();
        virtual bool initProperties();
        virtual bool updateProperties();
        virtual bool ISNewNumber (const char * dev, const char * name, double values[], char * names[], int n);
        virtual bool ISNewSwitch (const char * dev, const char * name, ISState * states, char * names[], int n);

    protected:

        virtual IPState MoveAbsFocuser(uint32_t ticks);
        virtual IPState MoveRelFocuser(FocusDirection dir, uint32_t ticks);

        virtual bool AbortFocuser();
        virtual void TimerHit();

    private:                

        // Get Firmware
        bool getFirmware();
        // Check if connection is OK
        bool Ack();

        // Goto Position
        bool gotoMotor(MotorType type, int32_t position);

        // Get Position
        bool getPosition(MotorType type);
        INumber GotoRotatorN[1];
        INumberVectorProperty GotoRotatorNP;
        INumber GotoAuxN[1];
        INumberVectorProperty GotoAuxNP;

        // Sync to Position
        bool syncMotor(MotorType type, int32_t position);
        INumber SyncFocusN[1];
        INumberVectorProperty SyncFocusNP;
        INumber SyncRotatorN[1];
        INumberVectorProperty SyncRotatorNP;
        INumber SyncAuxN[1];
        INumberVectorProperty SyncAuxNP;

        // Start/Stop Motors
        bool startMotor(MotorType type);
        bool stopMotor(MotorType type);
        bool isMotorMoving(MotorType type);

        // Sensors (Temperature + Voltage)
        bool getSensors();
        INumber SensorN[2];
        INumberVectorProperty SensorNP;
        enum { SENSOR_TEMPERATURE, SENSOR_VOLTAGE };

        // Temperature offset
        bool setTemperatureOffset(double offset);
        INumber TemperatureSettingN[1];
        INumberVectorProperty TemperatureSettingNP;

        // Motor step rate in 100 microsecond intervals
        bool getStepDelay(MotorType type);
        bool setStepDelay(MotorType type, uint32_t delay);
        INumber FocusStepDelayN[1];
        INumberVectorProperty FocusStepDelayNP;
        INumber RotatorStepDelayN[1];
        INumberVectorProperty RotatorStepDelayNP;
        INumber AuxStepDelayN[1];
        INumberVectorProperty AuxStepDelayNP;

        // Limit Switch
        bool getLimitSwitchStatus();
        ILight LimitSwitchL[3];
        ILightVectorProperty LimitSwitchLP;
        enum { ROTATION_SWITCH, OUT_SWITCH, IN_SWITCH };

        // Home
        bool findHome(MotorType type);
        ISwitch FindFocusHomeS[1];
        ISwitchVectorProperty FindFocusHomeSP;
        ISwitch FindRotatorHomeS[1];
        ISwitchVectorProperty FindRotatorHomeSP;
        ISwitch FindAuxHomeS[1];
        ISwitchVectorProperty FindAuxHomeSP;

        // Encoders
        bool setEncodersEnabled(bool enable);
        ISwitch EncoderS[1];
        ISwitchVectorProperty EncoderSP;

        // Brightness
        bool getBrightness();
        bool setBrightness(uint8_t display, uint8_t sleep);
        INumber BrightnessN[2];
        INumberVectorProperty BrightnessNP;
        enum { BRIGHTNESS_DISPLAY, BRIGHTNESS_SLEEP };

        double lastTemperature, lastVoltage;
        uint32_t lastPosition, targetPosition;

};

#endif // NightCrawler_H
