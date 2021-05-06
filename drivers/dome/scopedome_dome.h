/*******************************************************************************
 ScopeDome Dome INDI Driver

 Copyright(c) 2017-2021 Jarno Paananen. All rights reserved.

 based on:

 ScopeDome Windows ASCOM driver version 5.1.30

 and

 Copyright(c) 2014 Jasem Mutlaq. All rights reserved.

 Baader Planetarium Dome INDI Driver

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Library General Public
 License version 2 as published by the Free Software Foundation.
 .
 This library is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 Library General Public License for more details.
 .
 You should have received a copy of the GNU Library General Public License
 along with this library; see the file COPYING.LIB.  If not, write to
 the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 Boston, MA 02110-1301, USA.
*******************************************************************************/

#pragma once

#include "indidome.h"
#include "indipropertyswitch.h"
#include "indipropertynumber.h"
#include "indipropertytext.h"

#include <string>
#include <vector>

/**
 * Interface to either a real ScopeDome card or simulator
 */

class ScopeDomeCard
{
    public:

        enum ScopeDomeStatusBits
        {
            STATUS_RESET            = 1,
            STATUS_MOVING           = 2,
            STATUS_HOMING           = 4,
            STATUS_OPEN1            = 8,
            STATUS_OPEN2            = 0x10,
            STATUS_AUTO_CLOSE1      = 0x20,
            STATUS_AUTO_CLOSE2      = 0x40,
            STATUS_CALIBRATING      = 0x80,
            STATUS_FINDING_POWER    = 0x100,
            STATUS_CALIBRATION_STOP = 0x200
        };

        enum AbstractInput
        {
            HOME,
            OPEN1,
            CLOSED1,
            OPEN2,
            CLOSED2,
            ROTARY_LINK
        };

        enum AbstractOutput
        {
            RESET,
            CCW,
            CW,
        };

        enum ShutterOperation
        {
            OPEN_SHUTTER = 0,
            CLOSE_SHUTTER = 1,
            STOP_SHUTTER = 2
        };

        struct SensorInfo
        {
            std::string propName;
            std::string label;
            std::string format;
            float minValue;
            float maxValue;
        };

        struct RelayInfo
        {
            std::string propName;
            std::string label;
        };

        struct InputInfo
        {
            std::string propName;
            std::string label;
        };

        enum HomeSensorPolarity
        {
            ACTIVE_HIGH = 0,
            ACTIVE_LOW = 1
        };

        /** Destructor. */
        virtual ~ScopeDomeCard() = default;

        virtual bool detect() = 0;
        const char *getDeviceName()
        {
            return (const char *)"ScopeDome Dome";
        };
        virtual void setPortFD(int fd) = 0;

        // State polling
        virtual int updateState()                                             = 0;
        virtual uint32_t getStatus()                                          = 0;
        virtual int getRotationCounter()                                      = 0;
        virtual int getRotationCounterExt()                                   = 0;

        // Information
        virtual void getFirmwareVersions(double &main, double &rotary)        = 0;
        virtual uint32_t getStepsPerRevolution()                              = 0;
        virtual bool isCalibrationNeeded()                                    = 0;

        // Commands
        virtual void abort()                                                  = 0;
        virtual void calibrate()                                              = 0;
        virtual void findHome()                                               = 0;
        virtual void controlShutter(ShutterOperation operation)   = 0;

        virtual void resetCounter()                                           = 0;

        // negative means CCW, positive CW steps
        virtual void move(int steps)                                          = 0;

        // Input/Output management
        virtual size_t getNumberOfSensors()                                   = 0;
        virtual SensorInfo getSensorInfo(size_t index)                        = 0;
        virtual double getSensorValue(size_t index)                           = 0;

        virtual size_t getNumberOfRelays()                                    = 0;
        virtual RelayInfo getRelayInfo(size_t index)                          = 0;
        virtual ISState getRelayState(size_t index)                           = 0;
        virtual void setRelayState(size_t index, ISState state)               = 0;

        virtual size_t getNumberOfInputs()                                    = 0;
        virtual InputInfo getInputInfo(size_t index)                          = 0;
        virtual ISState getInputValue(size_t index)                           = 0;

        virtual ISState getInputState(AbstractInput input)                    = 0;
        virtual int setOutputState(AbstractOutput output, ISState state)      = 0;

        virtual void setHomeSensorPolarity(HomeSensorPolarity polarity)       = 0;

    protected:
        /** Default constructor. */
        ScopeDomeCard() = default;

    private:
        /** Prevent copy construction.virtual  */
        ScopeDomeCard(const ScopeDomeCard &rhs) = delete;
        /** Prevent assignment. */
        ScopeDomeCard &operator=(const ScopeDomeCard &rhs) = delete;
};

class ScopeDome : public INDI::Dome
{
    public:
        typedef enum
        {
            DOME_UNKNOWN,
            DOME_CALIBRATING,
            DOME_READY,
            DOME_HOMING,
            DOME_DEROTATING
        } DomeStatus;

        ScopeDome();
        virtual ~ScopeDome() = default;

        virtual const char *getDefaultName() override;
        virtual bool initProperties() override;
        virtual bool updateProperties() override;
        virtual bool saveConfigItems(FILE *fp) override;

        virtual bool Handshake() override;

        virtual void TimerHit() override;

        virtual bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n) override;
        virtual bool ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n) override;
        virtual bool ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n) override;

        virtual IPState Move(DomeDirection dir, DomeMotionCommand operation) override;
        virtual IPState MoveRel(double azDiff) override;
        virtual IPState MoveAbs(double az) override;
        virtual IPState ControlShutter(ShutterOperation operation) override;
        virtual bool Abort() override;

        // Parking
        virtual IPState Park() override;
        virtual IPState UnPark() override;
        virtual bool SetCurrentPark() override;
        virtual bool SetDefaultPark() override;

    protected:
        // Commands
        bool UpdatePosition();
        bool UpdateShutterStatus();
        bool UpdateSensorStatus();
        bool UpdateRelayStatus();

        // Misc
        bool SetupParms();

        DomeStatus status{ DOME_UNKNOWN };
        double targetAz{ 0 };
        bool refineMove{ false };
        ShutterOperation targetShutter{ SHUTTER_OPEN };
        bool sim{ false };
        double simShutterTimer{ 0 };
        ShutterState simShutterStatus{ SHUTTER_OPENED };

        enum CardType
        {
            SCOPEDOME_USB21 = 0,
            SCOPEDOME_ARDUINO = 1
        };
        INDI::PropertySwitch CardTypeSP{2};

        INDI::PropertyNumber DomeHomePositionNP{1};

        INDI::PropertySwitch HomeSensorPolaritySP{2};

        INDI::PropertySwitch FindHomeSP{1};

        INDI::PropertySwitch DerotateSP{1};

        INDI::PropertyNumber StepsPerRevolutionNP{1};

        INDI::PropertySwitch CalibrationNeededSP{1};

        INDI::PropertySwitch StartCalibrationSP{1};

        INDI::PropertyNumber FirmwareVersionsNP{2};

        INDI::PropertyText CredentialsTP{2};

        // Dynamic properies initialized based on card type
        INDI::PropertySwitch RelaysSP{0};
        INDI::PropertyNumber SensorsNP{0};
        INDI::PropertySwitch InputsSP{0};

    private:
        uint16_t currentStatus;
        int32_t currentRotation;
        int32_t rotationCounter;

        uint32_t stepsPerRevolution;

        std::unique_ptr<ScopeDomeCard> interface;

        IPState sendMove(double azDiff);

        // Dome inertia compensation
        std::vector<uint16_t> inertiaTable;
        int compensateInertia(int steps);

        // Needed to access reconnect function
        friend class ScopeDomeUSB21;
        friend class ScopeDomeArduino;

        void reconnect();
        bool reconnecting{false};

        const INDI::PropertyText &getCredentials() const
        {
            return CredentialsTP;
        };

        // Dew point calculation
        float getDewPoint(float RH, float T);

        ISState rotaryLinkEstablished {ISS_OFF};
};
