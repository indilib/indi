/*******************************************************************************
  Copyright(c) 2015 Jasem Mutlaq. All rights reserved.

  TerransPowerBoxGoV2

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

#include "config.h"
#include "Terrans_PowerBoxGo_V2.h"

#include "indicom.h"
#include "connectionplugins/connectionserial.h"

#include <cerrno>
#include <cstring>
#include <memory>
#include <termios.h>
#include <unistd.h>
#include <inttypes.h>
#include <sys/ioctl.h>

// We declare an auto pointer to TerransPowerBoxGoV2.
static std::unique_ptr<TerransPowerBoxGoV2> terranspowerboxgov2(new TerransPowerBoxGoV2());

#define CMD_LEN 8
#define TIMEOUT 500
#define TAB_INFO "Status"
#define TAB_RENAME "Rename"

double ch1_shuntv = 0;
double ch1_current = 0;
double ch1_bus = 0;
double humidity = 0;
double temperature = 0;
double dewPoint = 0;
double mcu_temp = 0;


TerransPowerBoxGoV2::TerransPowerBoxGoV2() : INDI::PowerInterface(this)
{
    setVersion(1, 0);
}

bool TerransPowerBoxGoV2::initProperties()
{
    INDI::DefaultDevice::initProperties();

    setDriverInterface(AUX_INTERFACE | POWER_INTERFACE); // Add POWER_INTERFACE

    addAuxControls();

    // Populate PI::PowerChannelLabelsTP and PI::USBPortLabelsTP
    if (PI::PowerChannelLabelsTP.size() >= 5)
    {
        PI::PowerChannelLabelsTP[0].setLabel("DC OUT A");
        PI::PowerChannelLabelsTP[1].setLabel("DC OUT B");
        PI::PowerChannelLabelsTP[2].setLabel("DC OUT C");
        PI::PowerChannelLabelsTP[3].setLabel("DC OUT D");
        PI::PowerChannelLabelsTP[4].setLabel("DC OUT E");
    }
    if (PI::USBPortLabelsTP.size() >= 4)
    {
        PI::USBPortLabelsTP[0].setLabel("USB3.0 A");
        PI::USBPortLabelsTP[1].setLabel("USB3.0 B");
        PI::USBPortLabelsTP[2].setLabel("USB2.0 E");
        PI::USBPortLabelsTP[3].setLabel("USB2.0 F");
    }

    // Removed ConfigRenameDCA to ConfigRenameUSBF as they are no longer needed

    // Removed manual Power Switch declarations (DCASP, DCBSP, etc.) as they are handled by PI::PowerChannelsSP and PI::USBPortSP
    // Removed StateSaveSP as it's not part of INDI::PowerInterface directly, but can be re-added as a custom property if needed.

    ////////////////////////////////////////////////////////////////////////////
    /// Sensor Data
    ////////////////////////////////////////////////////////////////////////////
    // InputVotageNP, InputCurrentNP, PowerNP are now handled by PI::PowerSensorsNP
    MCUTempNP[0].fill("MCU_Temp", "MCU Temperature (C)", "%.2f", 0, 200, 0.01, 0);
    MCUTempNP.fill(getDeviceName(), "MCU_Temp", "MCU", MAIN_CONTROL_TAB, IP_RO, 60, IPS_IDLE);

    StateSaveSP[0].fill("SAVE_STATE", "Save State", ISS_ON);
    StateSaveSP[1].fill("DISABLE_SAVE", "Disable Save", ISS_OFF);
    StateSaveSP.fill(getDeviceName(), "STATE_SAVE", "State Save", MAIN_CONTROL_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    ////////////////////////////////////////////////////////////////////////////
    /// Serial Connection
    ////////////////////////////////////////////////////////////////////////////
    serialConnection = new Connection::Serial(this);
    serialConnection->registerHandshake([&]()
    {
        return Handshake();
    });
    serialConnection->setDefaultBaudRate(Connection::Serial::B_9600);
    registerConnection(serialConnection);

    return true;
}

bool TerransPowerBoxGoV2::updateProperties()
{
    INDI::DefaultDevice::updateProperties();

    if (isConnected())
    {
        // PowerInterface properties
        PI::updateProperties();

        // Main Control
        defineProperty(MCUTempNP);
        defineProperty(StateSaveSP);

        setupComplete = true;
    }
    else
    {
        // PowerInterface properties
        PI::updateProperties();

        // Main Control
        deleteProperty(MCUTempNP);
        deleteProperty(StateSaveSP);

        setupComplete = false;
    }

    return true;
}

bool TerransPowerBoxGoV2::saveConfigItems(FILE *fp)
{
    INDI::DefaultDevice::saveConfigItems(fp);
    PI::saveConfigItems(fp);
    StateSaveSP.save(fp);
    return true;
}

bool TerransPowerBoxGoV2::Handshake()
{
    char res[CMD_LEN] = {0};
    if (isSimulation())
    {
        LOGF_INFO("Connected successfuly to simulated %s.", getDeviceName());
        // For simulation, assume full capabilities
        PI::SetCapability(POWER_HAS_DC_OUT | POWER_HAS_USB_TOGGLE | POWER_HAS_VOLTAGE_SENSOR |
                          POWER_HAS_OVERALL_CURRENT);
        // 5 DC ports, 0 DEW ports, 0 Variable port, 0 Auto Dew ports, 4 USB ports
        PI::initProperties(POWER_TAB, 5, 0, 0, 0, 4);
        return true;
    }

    for(int HS = 0 ; HS < 3 ; HS++)
    {
        if (sendCommand(">VR#", res))
        {
            if(!strcmp(res, "*TPGNNN"))
            {
                if (sendCommand(">VN#", res))
                {
                    if(!strcmp(res, "*V001"))
                    {
                        LOG_INFO("Handshake successfully!");
                        // Set capabilities based on device knowledge
                        PI::SetCapability(POWER_HAS_DC_OUT | POWER_HAS_USB_TOGGLE | POWER_HAS_VOLTAGE_SENSOR |
                                          POWER_HAS_OVERALL_CURRENT);
                        // 5 DC ports, 0 DEW ports, 0 Variable port, 0 Auto Dew ports, 4 USB ports
                        PI::initProperties(POWER_TAB, 5, 0, 0, 0, 4);
                        return true;
                    }
                    else
                    {
                        LOG_INFO("The firmware version does not match the driver. Please use the latest firmware and driver!");
                        return false;
                    }
                }
            }
            else
            {
                LOG_INFO("Handshake failed!");
                LOG_INFO("Retry...");
            }
        }
    }
    LOG_INFO("Handshake failed!");
    return false;
}

bool TerransPowerBoxGoV2::SetPowerPort(size_t port, bool enabled)
{
    char cmd[CMD_LEN] = {0};
    char res[CMD_LEN] = {0};
    // Ports are 0-indexed in INDI::PowerInterface, but device expects 1-indexed.
    // Also, the device uses specific commands for each port (SDA, SDB, etc.)
    // Assuming port 0 -> SDA, 1 -> SDB, 2 -> SDC, 3 -> SDD, 4 -> SDE
    const char *portCmds[] = {">SDA", ">SDB", ">SDC", ">SDD", ">SDE"};
    if (port < sizeof(portCmds) / sizeof(portCmds[0]))
    {
        snprintf(cmd, CMD_LEN, "%s%d#", portCmds[port], enabled ? 1 : 0);
        if (sendCommand(cmd, res))
        {
            // Check response based on command sent
            char expectedRes[CMD_LEN] = {0};
            snprintf(expectedRes, CMD_LEN, "*D%c%dNNN", static_cast<int>(('A' + port)), enabled ? 1 : 0);
            return !strcmp(res, expectedRes);
        }
    }
    return false;
}

bool TerransPowerBoxGoV2::SetDewPort(size_t port, bool enabled, double dutyCycle)
{
    INDI_UNUSED(port);
    INDI_UNUSED(enabled);
    INDI_UNUSED(dutyCycle);
    // TerransPowerBoxGoV2 does not appear to have dew heater ports based on the original code.
    // If it did, this would be implemented here.
    LOG_WARN("SetDewPort not implemented for TerransPowerBoxGoV2.");
    return false;
}

bool TerransPowerBoxGoV2::SetVariablePort(size_t port, bool enabled, double voltage)
{
    INDI_UNUSED(port);
    INDI_UNUSED(enabled);
    INDI_UNUSED(voltage);
    // TerransPowerBoxGoV2 does not appear to have variable voltage ports.
    // If it did, this would be implemented here.
    LOG_WARN("SetVariablePort not implemented for TerransPowerBoxGoV2.");
    return false;
}

bool TerransPowerBoxGoV2::SetLEDEnabled(bool enabled)
{
    INDI_UNUSED(enabled);
    // TerransPowerBoxGoV2 does not appear to have LED control.
    // If it did, this would be implemented here.
    LOG_WARN("SetLEDEnabled not implemented for TerransPowerBoxGoV2.");
    return false;
}

bool TerransPowerBoxGoV2::SetAutoDewEnabled(size_t port, bool enabled)
{
    INDI_UNUSED(port);
    INDI_UNUSED(enabled);
    // TerransPowerBoxGoV2 does not appear to have auto dew control.
    // If it did, this would be implemented here.
    LOG_WARN("SetAutoDewEnabled not implemented for TerransPowerBoxGoV2.");
    return false;
}

bool TerransPowerBoxGoV2::CyclePower()
{
    // TerransPowerBoxGoV2 does not appear to have a direct power cycle command.
    // If it did, this would be implemented here.
    LOG_WARN("CyclePower not implemented for TerransPowerBoxGoV2.");
    return false;
}

bool TerransPowerBoxGoV2::SetUSBPort(size_t port, bool enabled)
{
    char cmd[CMD_LEN] = {0};
    char res[CMD_LEN] = {0};
    // Ports are 0-indexed in INDI::PowerInterface, but device expects 1-indexed.
    // Also, the device uses specific commands for each port (SUA, SUB, SUE, SUF)
    // Assuming port 0 -> SUA, 1 -> SUB, 2 -> SUE, 3 -> SUF
    const char *usbCmds[] = {">SUA", ">SUB", ">SUE", ">SUF"};
    if (port < sizeof(usbCmds) / sizeof(usbCmds[0]))
    {
        // The original code uses SUA1A#, SUA0A#, SUB1A#, SUB0A#, SUE1A#, SUE0A#, SUF1A#, SUF0A#
        // This implies a fixed 'A' suffix.
        snprintf(cmd, CMD_LEN, "%s%d%c#", usbCmds[port], enabled ? 1 : 0, 'A');
        if (sendCommand(cmd, res))
        {
            // Check response based on command sent
            char expectedRes[CMD_LEN] = {0};
            if (port == 0 || port == 1) // USB3.0 A, B
                snprintf(expectedRes, CMD_LEN, "*U%c%d11N", static_cast<int>('A' + port), enabled ? 1 : 0);
            else // USB2.0 E, F
                snprintf(expectedRes, CMD_LEN, "*U%c%d1NN", static_cast<int>('E' + (port - 2)), enabled ? 1 : 0);
            return !strcmp(res, expectedRes);
        }
    }
    return false;
}


const char *TerransPowerBoxGoV2::getDefaultName()
{
    LOG_INFO("GET Name");
    return "TerransPowerBoxGoV2";
}

bool TerransPowerBoxGoV2::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        // Process power-related switches via PowerInterface
        if (PI::processSwitch(dev, name, states, names, n))
            return true;

        // Handle custom switches not part of PowerInterface
        if (StateSaveSP.isNameMatch(name))
        {
            StateSaveSP.update(states, names, n);
            if (StateSaveSP[0].getState() == ISS_ON)
            {
                if (sendCommand(">SS1#", nullptr)) // Assuming SS1# saves state
                {
                    StateSaveSP.setState(IPS_OK);
                    LOG_INFO("Save Switch State Enable");
                }
                else
                {
                    StateSaveSP.setState(IPS_ALERT);
                    LOG_INFO("Save Switch State Set Fail");
                }
            }
            else if (StateSaveSP[1].getState() == ISS_ON)
            {
                if (sendCommand(">SS0#", nullptr)) // Assuming SS0# disables state save
                {
                    StateSaveSP.setState(IPS_OK);
                    LOG_INFO("Save Switch State Disable");
                }
                else
                {
                    StateSaveSP.setState(IPS_ALERT);
                    LOG_INFO("Save Switch State Set Fail");
                }
            }
            StateSaveSP.apply();
            return true;
        }
    }

    return INDI::DefaultDevice::ISNewSwitch(dev, name, states, names, n);
}

bool TerransPowerBoxGoV2::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    // Process power-related numbers via PowerInterface
    if (PI::processNumber(dev, name, values, names, n))
        return true;

    return INDI::DefaultDevice::ISNewNumber(dev, name, values, names, n);
}

bool TerransPowerBoxGoV2::ISNewText(const char * dev, const char * name, char * texts[], char * names[], int n)
{
    // Process power-related text via PowerInterface
    if (PI::processText(dev, name, texts, names, n))
        return true;


    return INDI::DefaultDevice::ISNewText(dev, name, texts, names, n);
}

bool TerransPowerBoxGoV2::sendCommand(const char * cmd, char * res)
{

    int nbytes_read = 0, nbytes_written = 0, tty_rc = 0;
    char command[CMD_LEN] = {0};
    PortFD = serialConnection->getPortFD();
    LOGF_DEBUG("CMD <%s>", cmd);

    for (int i = 0; i < 2; i++)
    {
        tcflush(PortFD, TCIOFLUSH);
        snprintf(command, CMD_LEN, "%s", cmd);
        if ( (tty_rc = tty_write_string(PortFD, command, &nbytes_written)) != TTY_OK)
            continue;

        if (!res)
        {
            tcflush(PortFD, TCIOFLUSH);
            return true;
        }

        if ( (tty_rc = tty_nread_section(PortFD, res, CMD_LEN, '#', TIMEOUT, &nbytes_read)) != TTY_OK || nbytes_read == 1)
            continue;

        tcflush(PortFD, TCIOFLUSH);
        res[nbytes_read - 1] = '\0';
        LOGF_DEBUG("RES <%s>", res);
        return true;
    }

    if (tty_rc != TTY_OK)
    {
        char errorMessage[MAXRBUF];
        tty_error_msg(tty_rc, errorMessage, MAXRBUF);
        LOGF_ERROR("Serial error: %s", errorMessage);
    }

    return false;
}

void TerransPowerBoxGoV2::TimerHit()
{
    if (!isConnected() || setupComplete == false)
    {
        SetTimer(100);
        return;
    }

    // Update PowerInterface properties
    char res[CMD_LEN] = {0};

    // Get Power Channel States
    for (size_t i = 0; i < PI::PowerChannelsSP.size(); ++i)
    {
        const char *portCmds[] = {">GDA#", ">GDB#", ">GDC#", ">GDD#", ">GDE#"};
        if (i < sizeof(portCmds) / sizeof(portCmds[0]))
        {
            if (sendCommand(portCmds[i], res))
            {
                if (!strcmp(res, "*D1NNN")) // Assuming D1NNN for ON, D0NNN for OFF
                {
                    PI::PowerChannelsSP[i].setState(ISS_ON);
                }
                else if (!strcmp(res, "*D0NNN"))
                {
                    PI::PowerChannelsSP[i].setState(ISS_OFF);
                }
                else
                {
                    PI::PowerChannelsSP[i].setState(ISS_OFF); // Default to OFF on unknown response
                }
            }
        }
    }
    PI::PowerChannelsSP.apply();

    // Get USB Port States
    for (size_t i = 0; i < PI::USBPortSP.size(); ++i)
    {
        const char *usbCmds[] = {">GUA#", ">GUB#", ">GUE#", ">GUF#"};
        if (i < sizeof(usbCmds) / sizeof(usbCmds[0]))
        {
            if (sendCommand(usbCmds[i], res))
            {
                // Assuming *UA111N for ON, *UA000N for OFF for USB3.0 A/B
                // Assuming *UE11NN for ON, *UE00NN for OFF for USB2.0 E/F
                if ((i == 0 || i == 1) && !strcmp(res, "*U111N"))
                {
                    PI::USBPortSP[i].setState(ISS_ON);
                }
                else if ((i == 0 || i == 1) && !strcmp(res, "*U000N"))
                {
                    PI::USBPortSP[i].setState(ISS_OFF);
                }
                else if ((i == 2 || i == 3) && !strcmp(res, "*U11NN"))
                {
                    PI::USBPortSP[i].setState(ISS_ON);
                }
                else if ((i == 2 || i == 3) && !strcmp(res, "*U00NN"))
                {
                    PI::USBPortSP[i].setState(ISS_OFF);
                }
                else
                {
                    PI::USBPortSP[i].setState(ISS_OFF); // Default to OFF on unknown response
                }
            }
        }
    }
    PI::USBPortSP.apply();

    // Get State Save Switch
    if (sendCommand(">GS#", res))
    {
        if (!strcmp(res, "*SS1NNN"))
        {
            StateSaveSP[0].setState(ISS_ON);
            StateSaveSP[1].setState(ISS_OFF);
        }
        else if (!strcmp(res, "*SS0NNN"))
        {
            StateSaveSP[0].setState(ISS_OFF);
            StateSaveSP[1].setState(ISS_ON);
        }
        else
        {
            StateSaveSP[0].setState(ISS_OFF);
            StateSaveSP[1].setState(ISS_OFF);
        }
    }
    StateSaveSP.apply();

    // Get Input Voltage, Current, Power
    if (sendCommand(">GPA#", res))
    {
        ch1_bus = (res[6] - 0x30) + ((res[5] - 0x30) * 10) + ((res[4] - 0x30) * 100) + ((res[3] - 0x30) * 1000);
        ch1_bus = ch1_bus * 4 / 1000;
        PI::PowerSensorsNP[PI::SENSOR_VOLTAGE].setValue(ch1_bus);
    }

    if (sendCommand(">GPB#", res))
    {
        ch1_shuntv = (res[6] - 0x30) + ((res[5] - 0x30) * 10) + ((res[4] - 0x30) * 100) + ((res[3] - 0x30) * 1000);
        ch1_current = ch1_shuntv * 10 / 1000000 / 0.002;
        PI::PowerSensorsNP[PI::SENSOR_CURRENT].setValue(ch1_current);
        PI::PowerSensorsNP[PI::SENSOR_POWER].setValue(ch1_current * ch1_bus); // Calculate power
    }
    PI::PowerSensorsNP.setState(IPS_OK);
    PI::PowerSensorsNP.apply();

    // Get MCU Temperature
    if (sendCommand(">GC#", res))
    {
        mcu_temp = (res[6] - 0x30) + ((res[5] - 0x30) * 10) + ((res[4] - 0x30) * 100) + ((res[3] - 0x30) * 1000);
        if (mcu_temp == 0)
        {
            MCUTempNP[0].setValue(0);
        }
        else
        {
            if (res[2] == 'A')
            {
                mcu_temp = mcu_temp / 100;
                MCUTempNP[0].setValue(mcu_temp);
            }
            else if (res[2] == 'B')
            {
                mcu_temp = mcu_temp / 100;
                mcu_temp = mcu_temp * (-1);
                MCUTempNP[0].setValue(mcu_temp);
            }
        }
        MCUTempNP.setState(IPS_OK);
        MCUTempNP.apply();
    }

    SetTimer(100);
}

// Removed the original Get_State() function as its functionality is now integrated into TimerHit()
