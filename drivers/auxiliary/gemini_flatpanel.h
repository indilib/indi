#pragma once

#include "defaultdevice.h"
#include "connectionplugins/connectionserial.h"
#include "indilightboxinterface.h"
#include "indidustcapinterface.h"
#include "gemini_flatpanel_adapters.h"
#include <memory>

#define GEMINI_DEVICE_ID 99

namespace Connection
{
class Serial;
}

// Forward declaration for the adapter
class GeminiFlatpanelAdapter;

class GeminiFlatpanel : public INDI::DefaultDevice, public INDI::LightBoxInterface, public INDI::DustCapInterface
{
    public:
        GeminiFlatpanel();
        virtual ~GeminiFlatpanel() = default;
        virtual const char *getDefaultName() override;

        void ISGetProperties(const char *deviceName) override;
        virtual bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n) override;
        virtual bool ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n) override;
        virtual bool ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n) override;
        virtual bool ISSnoopDevice(XMLEle *root) override;

    protected:
        bool initProperties() override;
        bool updateProperties() override;
        bool saveConfigItems(FILE *fp) override;
        void initStatusProperties();
        void initLimitsProperties();

        void TimerHit() override;

        // From LightBoxInterface
        bool SetLightBoxBrightness(uint16_t value) override;
        bool EnableLightBox(bool enable) override;
        void FilterNamesUpdated(const std::vector<std::string> &filterNames) override;
        void FilterSlotChanged(int index) override;

        // From DustCapInterface
        virtual IPState ParkCap() override;
        virtual IPState UnParkCap() override;
        virtual IPState AbortCap() override;

        // UI interactions
        void startConfiguration();
        void endConfiguration();
        bool validateOperation();
        bool validateCalibrationOperation(int direction);
        void onMove(INDI::PropertySwitch &positionSwitch, int direction);
        void onSetPosition(int direction);
        void cleanupSwitch(INDI::PropertySwitch &currentSwitch, int switchIndex);
        void onBeepChange();
        void onBrightnessModeChange();
        void applyFilterBrightnessMode(int index);

    private:
        // Serial connection
        bool Handshake();
        int PortFD{-1};

        Connection::Serial *serialConnection{nullptr};

        // Adapter for firmware-specific functionality
        std::unique_ptr<GeminiFlatpanelAdapter> adapter;

        // Note: Simulation state now managed by GeminiFlatpanelSimulationAdapter

        // Device revision
        int deviceRevision{-1};
        char commandTerminator{'\n'};

        // State variables
        int prevCoverStatus{-1};
        int prevLightStatus{-1};
        int prevMotorStatus{-1};
        int prevBrightness{-1};
        int configStatus{GEMINI_CONFIG_NOTREADY};

        // Index of the currently active filter slot (per FilterSlotChanged()), used to
        // apply a per-filter brightness mode preset immediately when it is toggled
        // while that filter is already selected. -1 while unknown.
        int currentFilterIndex{-1};

        // State update methods
        bool updateCoverStatus(char coverStatus);
        bool updateLightStatus(char lightStatus);
        bool updateMotorStatus(char motorStatus);
        bool updateBrightness(int brightness);
        void updateConfigStatus();
        void refreshStatus();
        void markMoving();

        // Commands
        bool sendCommand(const char *command, char *response, int timeout = SERIAL_TIMEOUT_SEC);
        bool pingRevision1();
        bool pingRevision2();
        bool getFirmwareVersion(int *version);
        bool getConfigStatus();
        bool getBrightness(int *const brightness);
        bool setBrightness(int value);
        bool lightOn();
        bool lightOff();
        bool openCover();
        bool closeCover();
        bool getStatus(int *const coverStatus, int *const lightStatus, int *const motorStatus);
        bool move(uint16_t value, int direction);
        bool setClosePosition();
        bool setOpenPosition();
        bool setBeep(bool enable);
        bool setBrightnessMode(int mode);

        // Helper functions
        bool extractIntValue(const char *response, int startPos, int *value);
        bool formatCommand(char commandLetter, char *commandString, int value = NO_VALUE);

        // Status properties
        enum
        {
            STATUS_COVER,
            STATUS_LIGHT,
            STATUS_MOTOR,
            STATUS_N
        };
        INDI::PropertyText StatusTP{STATUS_N};
        INDI::PropertyText ConfigurationTP{1};
        INDI::PropertySwitch BeepSP{2};
        INDI::PropertySwitch BrightnessModeSP{2};

        // Per-filter brightness mode presets (Low/High), one independent switch per filter name
        INDI::PropertySwitch FilterBrightnessModeSP{0};

        // Limit properties
        // Split into "coarse" (270/180/90) and "fine" (45/10/1) groups because
        // KStars' generic INDI panel renders any switch property with more than 4
        // elements as a dropdown instead of buttons -- keeping each group small
        // keeps them clickable buttons.
        enum
        {
            MOVEMENT_COARSE_270,
            MOVEMENT_COARSE_180,
            MOVEMENT_COARSE_90,
            MOVEMENT_COARSE_N
        };
        enum
        {
            MOVEMENT_FINE_45,
            MOVEMENT_FINE_10,
            MOVEMENT_FINE_01,
            MOVEMENT_FINE_N
        };
        INDI::PropertySwitch ClosedPositionCoarseSP{MOVEMENT_COARSE_N};
        INDI::PropertySwitch ClosedPositionSP{MOVEMENT_FINE_N};
        INDI::PropertySwitch SetClosedSP{1};
        INDI::PropertySwitch OpenPositionCoarseSP{MOVEMENT_COARSE_N};
        INDI::PropertySwitch OpenPositionSP{MOVEMENT_FINE_N};
        INDI::PropertySwitch SetOpenSP{1};
        INDI::PropertySwitch ConfigureSP{1};

        // Device selection property
        enum
        {
            DEVICE_AUTO,
            DEVICE_REV1,
            DEVICE_REV2,
            DEVICE_LITE,
            DEVICE_PRO,
            DEVICE_N
        };
        INDI::PropertySwitch DeviceTypeSP{DEVICE_N};
};
