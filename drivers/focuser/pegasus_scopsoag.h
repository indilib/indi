#pragma once

#include "indifocuser.h"

class PegasusScopsOAG : public INDI::Focuser
{
       public:
        PegasusScopsOAG();
        virtual ~PegasusScopsOAG() override = default;

        virtual bool Handshake() override;
        const char *getDefaultName() override;
        virtual bool initProperties() override;
        virtual bool updateProperties() override;
        virtual bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n) override;

    protected:
        virtual IPState MoveAbsFocuser(uint32_t targetTicks) override;
        virtual IPState MoveRelFocuser(FocusDirection dir, uint32_t ticks) override;
        virtual bool AbortFocuser() override;
        virtual void TimerHit() override;

        virtual bool SyncFocuser(uint32_t ticks) override;
        virtual bool saveConfigItems(FILE *fp) override;

    private:
        bool updateFocusParams();
        bool move(uint32_t newPosition);
        bool setLedEnabled(bool enable);
        bool ack();
        void ignoreResponse();


        uint32_t currentPosition { 0 };
        uint32_t targetPosition { 0 };
        bool isMoving = false;

        // LED
        ISwitch LEDS[2];
        ISwitchVectorProperty LEDSP;
        enum { LED_OFF, LED_ON };       

        // Firmware Version
        IText FirmwareVersionT[1] {};
        ITextVectorProperty FirmwareVersionTP;
};
