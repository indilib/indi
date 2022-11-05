
#pragma once

#include "indilightboxinterface.h"
#include "defaultdevice.h"
#include "indidustcapinterface.h"

class Excalibur : public INDI::DefaultDevice, public INDI::LightBoxInterface, public INDI::DustCapInterface
{
    public:
        Excalibur();
        virtual ~Excalibur() override = default;

        const char *getDefaultName() override;
        virtual bool initProperties() override;
        virtual bool updateProperties() override;

        virtual bool ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n) override;
        virtual bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n) override;
        virtual bool ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n) override;
        virtual bool ISSnoopDevice(XMLEle *root) override;
        virtual void TimerHit() override;

    protected:
        virtual bool SetLightBoxBrightness(uint16_t value) override;
        virtual bool EnableLightBox(bool enable) override;
        virtual bool saveConfigItems(FILE *fp) override;
        virtual IPState ParkCap() override;
        virtual IPState UnParkCap() override;


    private:
        bool Ack();
        int PortFD{ -1 };
       bool sendCommand(const char * cmd, char * res = nullptr);
        void updateFirmwareVersion();
        void deviceStatus();
        bool getStartupData();
        // Firmware version

        ITextVectorProperty StatusTP;
        IText StatusT[2] {};
        static const uint32_t DRIVER_RES { 32 };

        static const char DRIVER_DEL { '#' };
        static const char DRIVER_DEL2 { ' ' };

        static const uint8_t DRIVER_TIMEOUT { 10 };
        Connection::Serial *serialConnection{ nullptr };
};
