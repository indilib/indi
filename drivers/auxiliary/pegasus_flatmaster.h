#pragma once

#include "indilightboxinterface.h"
#include "defaultdevice.h"


class PegasusFlatMaster : public INDI::DefaultDevice, public INDI::LightBoxInterface
{
public:
        PegasusFlatMaster();
        virtual ~PegasusFlatMaster() = default;

        const char *getDefaultName() override;
        virtual bool initProperties() override;
        virtual bool updateProperties() override;   
        virtual bool ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n) override;
        virtual bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n) override;
protected:    
        virtual bool SetLightBoxBrightness(uint16_t value) override;
        virtual bool EnableLightBox(bool enable) override;
        

private:
        bool Ack();
        int PortFD{ -1 };
        bool sendCommand(const char* cmd,char* response);
        void updateFirmwareVersion();

         // Firmware version
        ITextVectorProperty FirmwareTP;
        IText FirmwareT[1] {};
                
        Connection::Serial *serialConnection{ nullptr };
};
