/*
    USB_Dewpoint
    Copyright (C) 2017-2023 Jarno Paananen

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

#pragma once

#include <defaultdevice.h>
#include <indipowerinterface.h>

/***************************** USB_Dewpoint Commands **************************/

// All commands are exactly 6 bytes, no start/end markers
#define UDP_CMD_LEN 6
#define UDP_STATUS_CMD "SGETAL"
#define UDP_OUTPUT_CMD "S%1uO%03u"         // channel 1-3, power 0-100
#define UDP_THRESHOLD_CMD "STHR%1u%1u"     // channel 1-2, value 0-9
#define UDP_CALIBRATION_CMD "SCA%1u%1u%1u" // channel 1-2-A, value 0-9
#define UDP_LINK_CMD "SLINK%1u"            // 0 or 1 to link channels 2 and 3
#define UDP_AUTO_CMD "SAUTO%1u"            // 0 or 1 to enable auto mode
#define UDP_AGGRESSIVITY_CMD "SAGGR%1u"    // 1-4 (1, 2, 5, 10)
#define UDP_IDENTIFY_CMD "SWHOIS"
#define UDP_RESET_CMD "SEERAZ"

/**************************** USB_Dewpoint Constants **************************/

// Responses also include "\n\r"
#define UDP_DONE_RESPONSE "DONE"
#define UDP_RES_LEN 80 // With some margin, maximum feasible seems to be around 70

// Status response is like:
// ##22.37/22.62/23.35/50.77/12.55/0/0/0/0/0/0/2/2/0/0/4**

// Fields are in order:
// temperature ch 1
// temperature ch 2
// temperature ambient
// relative humidity
// dew point
// output ch 1
// output ch 2
// output ch 3
// calibration ch 1
// calibration ch 2
// calibration ambient
// threshold ch 1
// threshold ch 2
// auto mode
// outputs ch 2 & 3 linked
// aggressivity

#define UDP_STATUS_RESPONSE "##%f/%f/%f/%f/%f/%u/%u/%u/%u/%u/%u/%u/%u/%u/%u/%u**"
#define UDP_STATUS_START "##"
#define UDP_STATUS_SEPARATOR "/"
#define UDP_STATUS_END "**"

#define UDP_IDENTIFY_RESPONSE "UDP2(%u)" // Firmware version? Mine is "UDP2(1446)"

/******************************************************************************/

namespace Connection
{
class Serial;
};

class USBDewpoint : public INDI::DefaultDevice, public INDI::PowerInterface
{
    public:
        USBDewpoint();
        virtual ~USBDewpoint() = default;

        virtual const char *getDefaultName() override;
        virtual bool initProperties() override;
        virtual bool updateProperties() override;
        virtual bool ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n) override;
        virtual bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n) override;
        virtual bool ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n) override;
        virtual void TimerHit() override;

    protected:
        virtual bool SetPowerPort(size_t port, bool enabled) override;
        virtual bool SetDewPort(size_t port, bool enabled, double dutyCycle) override;
        virtual bool SetVariablePort(size_t port, bool enabled, double voltage) override;
        virtual bool SetLEDEnabled(bool enabled) override;
        virtual bool SetAutoDewEnabled(size_t port, bool enabled) override;
        virtual bool CyclePower() override;
        virtual bool SetUSBPort(size_t port, bool enabled) override;
        virtual bool saveConfigItems(FILE *fp) override;

    private:
        bool sendCommand(const char *cmd, char *response);

        bool Handshake();
        bool Ack();
        bool Resync();

        bool reset();
        bool readSettings();

        bool setOutput(unsigned int channel, unsigned int value);
        bool setCalibrations(unsigned int ch1, unsigned int ch2, unsigned int ambient);
        bool setThresholds(unsigned int ch1, unsigned int ch2);
        bool setAutoMode(bool enable);
        bool setLinkMode(bool enable);
        bool setAggressivity(unsigned int aggressivity);

        Connection::Serial *serialConnection{ nullptr };
        int PortFD{ -1 };

        INDI::PropertyNumber OutputsNP{3};
        INDI::PropertyNumber TemperaturesNP{3};
        INDI::PropertyNumber CalibrationsNP{3};
        INDI::PropertyNumber ThresholdsNP{2};
        INDI::PropertyNumber HumidityNP{1};
        INDI::PropertyNumber DewpointNP{1};
        INDI::PropertyNumber AggressivityNP{1};
        INDI::PropertySwitch AutoModeSP{2};
        INDI::PropertySwitch LinkOut23SP{2};
        INDI::PropertySwitch ResetSP{1};
        INDI::PropertyNumber FWversionNP{1};
};
