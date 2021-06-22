#pragma once

#include "defaultdevice.h"
#include <stdint.h>

namespace Connection
{
class Serial;
}

class PegasusUCH : public INDI::DefaultDevice
{
    public:
        PegasusUCH();
        virtual bool initProperties() override;
        virtual bool updateProperties() override;
        virtual bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n) override;
        virtual bool ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n) override;
        virtual bool ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n) override;

    protected:
        const char *getDefaultName() override;
        virtual bool saveConfigItems(FILE *fp) override;
        // Event loop
        virtual void TimerHit() override;

    private:
        bool Handshake();

        // Device Control
        bool reboot();

        // USB
        bool setPowerLEDEnabled(bool enabled);
        bool setUSBHubEnabled(bool enabled);
        bool setUSBPort(uint8_t port, ISwitch usbPortS[2], ISwitchVectorProperty sp, ISState * states, char * names[], int n);
        bool setUSBPortEnabled(uint8_t port, bool enabled);
        void setFirmwareVersion();
        void updateUSBPower();
        void updateUpTime();

        bool sendCommand(const char *command, char *res);
        void cleanupResponse(char *response);
        std::vector<std::string> split(const std::string &input, const std::string &regex);


        int PortFD { -1 };
        bool initialized { false };

        Connection::Serial *serialConnection { nullptr };

        ////////////////////////////////////////////////////////////////////////////////////
        /// Main Control
        ////////////////////////////////////////////////////////////////////////////////////

        /// Reboot Device
        ISwitch RebootS[1];
        ISwitchVectorProperty RebootSP;


        // Power LED
        ISwitch PowerLEDS[2];
        ISwitchVectorProperty PowerLEDSP;
        enum
        {
            POWER_LED_ON,
            POWER_LED_OFF,
        };


        // USB
        enum { USB_OFF, USB_ON };
        ISwitch USBPort1S[2];
        ISwitchVectorProperty USBPort1SP;

        ISwitch USBPort2S[2];
        ISwitchVectorProperty USBPort2SP;

        ISwitch USBPort3S[2];
        ISwitchVectorProperty USBPort3SP;

        ISwitch USBPort4S[2];
        ISwitchVectorProperty USBPort4SP;

        ISwitch USBPort5S[2];
        ISwitchVectorProperty USBPort5SP;

        ISwitch USBPort6S[2];
        ISwitchVectorProperty USBPort6SP;



        //Firmware
        ITextVectorProperty InfoTP;
        IText InfoT[3] {};


        enum
        {
            INFO_VERSION,
            INFO_UPTIME,
            INFO_USBVOLTAGE

        };


        static constexpr const uint8_t PEGASUS_TIMEOUT {3};
        static constexpr const uint8_t PEGASUS_LEN {32};
        static constexpr const char *USB_TAB {"USB"};
        static constexpr const char *INFO_TAB {"INFO"};
};
