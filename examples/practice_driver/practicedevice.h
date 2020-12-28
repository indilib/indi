

#pragma once

#include "defaultdevice.h"

class PracticeDevice : public INDI::DefaultDevice
{
    public:
        PracticeDevice() = default;
        virtual bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n) override;
        virtual bool ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n) override;

    protected:

        virtual bool initProperties() override;
        virtual bool updateProperties() override;

        virtual bool Connect() override;
        virtual bool Disconnect() override;
        const char *getDefaultName() override;

        virtual void TimerHit() override;

    private:

        bool readDeviceValues();
        void resetStatus();

        // Firmware Property
        ITextVectorProperty FirmwareTP;
        IText FirmwareT[1] = {};
        enum
        {
            FIRMWARE_VERSION,
        };

        // Scale Property
        INumberVectorProperty ScaleNP;
        INumber ScaleN[1];

        // Sync Scale Property
        INumberVectorProperty SyncScaleNP;
        INumber SyncScaleN[1];

        // Status
        ILightVectorProperty StatusLP;
        ILight StatusL[5];
        enum
        {
            STATUS_FULLY_RELEASED,
            STATUS_RELEASING,
            STATUS_HOLDING,
            STATUS_PRESSING,
            STATUS_FULLY_PRESSED,
        };

        // Tension levels
        ISwitchVectorProperty TensionLevelsSP;
        ISwitch TensionLevelsS[2];
        typedef enum
        {
            TENSION_LOW,
            TENSION_HIGH,
        } TensionState;

        // Pedal Actions
        ISwitchVectorProperty PedalActionsSP;
        ISwitch PedalActionsS[3];
        typedef enum
        {
            ACTION_PRESS_FOOT,
            ACTION_HOLD_FOOT,
            ACTION_REMOVE_FOOT,
        } PedalState;

        void updateStatus(ILight &Pstate);

        // Member Variables
        PedalState m_PedalState = ACTION_REMOVE_FOOT;
        TensionState m_TensionState = TENSION_LOW;
};

