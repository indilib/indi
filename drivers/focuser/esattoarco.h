/*
    Primaluca Labs Essato-Arco Focuser+Rotator Driver

    Copyright (C) 2020 Piotr Zyziuk
    Copyright (C) 2020-2022 Jasem Mutlaq

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

    JM 2022.07.16: Major refactor to using json.h and update to Essato Arco
    Document protocol revision 3.3 (8th July 2022).
*/

#pragma once

#include "indifocuser.h"
#include "inditimer.h"
#include "indirotatorinterface.h"
#include "primalucacommandset.h"
#include "indipropertytext.h"
#include "indipropertynumber.h"

class EsattoArco : public INDI::Focuser, public INDI::RotatorInterface
{
    public:
        EsattoArco();
        virtual ~EsattoArco() override = default;

        const char *getDefaultName() override;
        virtual bool initProperties() override;
        virtual bool updateProperties() override;
        virtual bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n) override;
        virtual bool ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n) override;

    protected:
        // Common
        virtual bool Handshake() override;
        virtual void TimerHit() override;
        virtual bool saveConfigItems(FILE *fp) override;

        // Focuser
        virtual IPState MoveRelFocuser(FocusDirection dir, uint32_t ticks) override;
        virtual IPState MoveAbsFocuser(uint32_t targetTicks) override;
        virtual bool AbortFocuser() override;
        virtual bool SetFocuserBacklash(int32_t steps) override;

        // Rotator
        virtual IPState MoveRotator(double angle) override;
        virtual bool SyncRotator(double angle) override;
        virtual bool AbortRotator() override;
        virtual bool ReverseRotator(bool enabled) override;

    private:
        bool Ack();
        bool setMinLimit(uint32_t limit);
        bool setMaxLimit(uint32_t limit);
        bool updateMaxLimit();

        bool updateTemperature();
        bool updatePosition();
        bool updateVoltageIn();

        void setConnectionParams();
        bool initCommandSet();

        bool getStartupValues();
        void hexDump(char * buf, const char * data, int size);
        bool isMotionComplete();

        uint16_t m_TemperatureCounter { 0 };

        INDI::PropertyNumber TemperatureNP {2};
        enum
        {
            TEMPERATURE_EXTERNAL,
            TEMPERATURE_MOTOR,
        };

        INDI::PropertyNumber SpeedNP {1};

        INDI::PropertyText FirmwareTP {4};
        enum
        {
            ESATTO_FIRMWARE_SN,
            ESATTO_FIRMWARE_VERSION,
            ARCO_FIRMWARE_SN,
            ARCO_FIRMWARE_VERSION,
        };

        INDI::PropertyNumber VoltageNP {2};
        enum
        {
            VOLTAGE_12V,
            VOLTAGE_USB
        };


        INDI::PropertySwitch FastMoveSP {3};
        enum
        {
            FASTMOVE_IN,
            FASTMOVE_OUT,
            FASTMOVE_STOP
        };

        INDI::PropertySwitch BacklashMeasurementSP {2};
        enum
        {
            BACKLASH_START,
            BACKLASH_NEXT
        };
        INDI::PropertyText BacklashMessageTP {1};

        // ITextVectorProperty BacklashMessageTP;
        typedef enum { BacklashIdle, BacklashMinimum, BacklashMaximum, BacklashComplete } BacklashStage;
        BacklashStage bStage { BacklashIdle };

        //Rotator
        INDI::PropertyNumber RotatorAbsPosNP {1};
        enum
        {
            ROTATOR_ABSOLUTE_POSITION
        };

        //ROTATOR CALIBRATION
        IText RotCalibrationMessageT[1] {};
        ITextVectorProperty RotCalibrationMessageTP;

        typedef enum { RotCalIdle, RotCalComplete } RotCalibrationStage;
        RotCalibrationStage rcStage { RotCalIdle };

        INDI::PropertySwitch RotatorCalibrationSP {1};
        // ISwitchVectorProperty RotatorCalibrationSP;
        enum
        {
            ARCO_CALIBRATION_START
        };

        std::unique_ptr<PrimalucaLabs::Esatto> m_Esatto;
        std::unique_ptr<PrimalucaLabs::Arco> m_Arco;

        static constexpr uint8_t TEMPERATURE_FREQUENCY {10};
};
