/*******************************************************************************
  Copyright(c) 2026 Jasem Mutlaq. All rights reserved.  

  Celestron Smart Dew & Power Controller.

  This program is free software; you can redistribute it and/or modify it
  under the terms of the GNU General Public License as published by the Free
  Software Foundation; either version 2 of the License, or (at your option)
  any later version.

  This program is distributed in the hope that it will be useful, but WITHOUT
  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
  more details.

  You should have received a copy of the GNU Library General Public License
  along with this library; see the file COPYING.LIB.  If not, write to
  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
  Boston, MA 02110-1301, USA.

  The full GNU General Public License is included in this distribution in the
  file called LICENSE.
*******************************************************************************/

#pragma once

#include "indibase.h"
#include "defaultdevice.h"
#include "indipowerinterface.h"
#include "indiweatherinterface.h"
#include "connectionplugins/connectionserial.h"

#include "celestron_dewpower_auxproto.h"

#include <vector>
#include <thread>
#include <chrono>
#include <termios.h>
#include <sys/ioctl.h>

class CelestronDewPower : public INDI::DefaultDevice, public INDI::PowerInterface, public INDI::WeatherInterface
{
public:
    CelestronDewPower();

    virtual const char *getDefaultName() override;
    virtual bool initProperties() override;
    virtual bool updateProperties() override;
    virtual bool Handshake();
    virtual void TimerHit() override;

    // INDI::WeatherInterface overrides
    virtual IPState updateWeather() override;
    
    // INDI::PowerInterface overrides
    virtual bool SetPowerPort(size_t port, bool enabled) override;
    virtual bool SetDewPort(size_t port, bool enabled, double dutyCycle) override;
    virtual bool SetVariablePort(size_t port, bool enabled, double voltage) override;
    virtual bool SetLEDEnabled(bool enabled) override;
    virtual bool SetAutoDewEnabled(size_t port, bool enabled) override;

protected:
    virtual bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n) override;
    virtual bool ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n) override;
    virtual bool ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n) override;
    virtual bool saveConfigItems(FILE *fp) override;

private:
    Connection::Serial *serialConnection;
    int PortFD;
    bool setupComplete {false};

    // AUX Command communication
    bool sendAUXCommand(AUXCommand &command);
    bool readAUXResponse(AUXCommand c);
    bool serialReadResponse(AUXCommand c);
    int sendBuffer(AUXBuffer buf);
    bool processResponse(AUXCommand &m); // Added processResponse

    // Serial port specific functions
    void setRTS(bool rts);
    bool waitCTS(float timeout);
    bool detectRTSCTS();
    bool tty_set_speed(speed_t speed);
    void hex_dump(char *buf, AUXBuffer data, size_t size);
    int aux_tty_read(char *buf, int bufsiz, int timeout, int *n);
    int aux_tty_write(char *buf, int bufsiz, float timeout, int *n);

    // Device specific commands
    bool getDewPowerControllerVersion();
    bool getNumberOfPorts();
    bool getInputPower();
    bool getPortInfo(uint8_t portNumber);
    bool getDewHeaterPortInfo(uint8_t portNumber);
    bool setPortEnabled(uint8_t portNumber, bool enabled);
    bool setPortVoltage(uint8_t portNumber, uint16_t voltage_mV);
    bool setDewHeaterAuto(uint8_t portNumber, uint8_t mode, uint8_t temp_C);
    bool setDewHeaterManual(uint8_t portNumber, uint8_t powerLevel);
    bool setLEDBrightness(uint8_t brightness);

    // Internal properties
    INDI::PropertyNumber PowerConsumptionNP {3}; // Avg. Amps, Amp Hours, Watt Hours
    INDI::PropertySwitch RebootSP {1};
    INDI::PropertyLight PowerStatusLP {3};
    INDI::PropertySwitch AutoDewModeSP {2};
    INDI::PropertyNumber AutoDewTempNP {1};

    enum
    {
        POWER_STATUS_OVERCURRENT,
        POWER_STATUS_UNDERVOLTAGE,
        POWER_STATUS_OVERVOLTAGE
    };

    // Communication state variables from CelestronAUX
    bool m_IsRTSCTS {false};
    int m_ModemControl {0};

    // Store last received data to avoid unnecessary updates
    AUXBuffer lastInputPowerData;
    std::vector<AUXBuffer> lastPortInfoData;
    std::vector<AUXBuffer> lastDewHeaterPortInfoData;
    AUXBuffer lastEnvironmentData;

    std::vector<uint8_t> m_PortTypes;
    uint8_t m_NumPorts {0};
    uint8_t m_NumDCPorts {0};
    uint8_t m_NumDewPorts {0};
    uint8_t m_NumVariablePorts {0};
    uint8_t m_NumUSBPorts {0};

    // Debug
    uint32_t DBG_CAUX;
    uint32_t DBG_SERIAL;

    // Constants
    // seconds
    static constexpr uint8_t READ_TIMEOUT {1};
    // ms
    static constexpr uint8_t CTS_TIMEOUT {100};
};
