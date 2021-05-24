#pragma once

#include "indifocuser.h"

class PegasusFocusCube : public INDI::Focuser
{
    public:
        PegasusFocusCube();
        virtual ~PegasusFocusCube() override = default;

        virtual bool Handshake() override;
        const char *getDefaultName() override;
        virtual bool initProperties() override;
        virtual bool updateProperties() override;
        virtual bool ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n) override;
        virtual bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n) override;

    protected:
        virtual IPState MoveAbsFocuser(uint32_t targetTicks) override;
        virtual IPState MoveRelFocuser(FocusDirection dir, uint32_t ticks) override;
        virtual bool AbortFocuser() override;
        virtual void TimerHit() override;

        virtual bool SyncFocuser(uint32_t ticks) override;
        virtual bool ReverseFocuser(bool enabled) override;
        virtual bool SetFocuserBacklash(int32_t steps) override;
        virtual bool SetFocuserBacklashEnabled(bool enabled) override;
        virtual bool saveConfigItems(FILE *fp) override;

    private:
        bool updateFocusParams();
        bool move(uint32_t newPosition);
        bool setMaxSpeed(uint16_t speed);
        bool setLedEnabled(bool enable);
        bool setEncodersEnabled(bool enable);        
        bool ack();
        void ignoreResponse();

        uint32_t currentPosition { 0 };
        uint32_t targetPosition { 0 };
        bool isMoving = false;

        // Temperature probe
        INumber TemperatureN[1];
        INumberVectorProperty TemperatureNP;


        // Rotator Encoders
        ISwitch EncoderS[2];
        ISwitchVectorProperty EncoderSP;
        enum { ENCODERS_ON, ENCODERS_OFF };

        // LED
        ISwitch LEDS[2];
        ISwitchVectorProperty LEDSP;
        enum { LED_OFF, LED_ON };

        // Maximum Speed
        INumber MaxSpeedN[1];
        INumberVectorProperty MaxSpeedNP;

        // Firmware Version
        IText FirmwareVersionT[1] {};
        ITextVectorProperty FirmwareVersionTP;
};
