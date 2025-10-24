/*******************************************************************************
  Copyright(c) 2018-2026 Jasem Mutlaq. All rights reserved.

  Pegasus Ultimate Power Box Driver.

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

#include "pegasus_upb.h"
#include "indicom.h"
#include "connectionplugins/connectionserial.h"

#include <memory>
#include <regex>
#include <termios.h>
#include <cstring>
#include <sys/ioctl.h>
#include <chrono>
#include <math.h>
#include <iomanip>

// We declare an auto pointer to PegasusUPB.
static std::unique_ptr<PegasusUPB> upb(new PegasusUPB());

PegasusUPB::PegasusUPB() : INDI::DefaultDevice(), INDI::FocuserInterface(this), INDI::WeatherInterface(this),
    INDI::PowerInterface(this)
{
    setVersion(1, 6);

    lastSensorData.reserve(21);
    lastPowerData.reserve(4);
    lastStepperData.reserve(4);
    lastDewAggData.reserve(1);
}

bool PegasusUPB::initProperties()
{
    INDI::DefaultDevice::initProperties();

    setDriverInterface(AUX_INTERFACE | FOCUSER_INTERFACE | WEATHER_INTERFACE | POWER_INTERFACE);

    FI::SetCapability(FOCUSER_CAN_ABS_MOVE |
                      FOCUSER_CAN_REL_MOVE |
                      FOCUSER_CAN_REVERSE  |
                      FOCUSER_CAN_SYNC     |
                      FOCUSER_CAN_ABORT    |
                      FOCUSER_HAS_BACKLASH);

    FI::initProperties(FOCUS_TAB);
    WI::initProperties(ENVIRONMENT_TAB, ENVIRONMENT_TAB);


    addAuxControls();

    // Reboot
    RebootSP[0].fill("REBOOT", "Reboot Device", ISS_OFF);
    RebootSP.fill(getDeviceName(), "REBOOT_DEVICE", "Device", MAIN_CONTROL_TAB, IP_RW, ISR_ATMOST1,
                  60, IPS_IDLE);

    // Overall Power Consumption (remains as is, not part of INDI::Power)
    PowerConsumptionNP[CONSUMPTION_AVG_AMPS].fill("CONSUMPTION_AVG_AMPS", "Avg. Amps", "%4.2f", 0, 999, 100, 0);
    PowerConsumptionNP[CONSUMPTION_AMP_HOURS].fill("CONSUMPTION_AMP_HOURS", "Amp Hours", "%4.2f", 0, 999, 100, 0);
    PowerConsumptionNP[CONSUMPTION_WATT_HOURS].fill("CONSUMPTION_WATT_HOURS", "Watt Hours", "%4.2f", 0, 999, 100, 0);
    PowerConsumptionNP.fill(getDeviceName(), "POWER_CONSUMPTION", "Consumption",
                            MAIN_CONTROL_TAB, IP_RO, 60, IPS_IDLE);

    ////////////////////////////////////////////////////////////////////////////
    /// Dew Group (Auto Dew Aggressiveness remains as is)
    ////////////////////////////////////////////////////////////////////////////

    // Automatic Dew Aggressiveness v2
    AutoDewAggNP[AUTO_DEW_AGG].fill("AUTO_DEW_AGG_VALUE", "Auto Dew Agg (50-250)", "%.2f", 50, 250, 20, 0);
    AutoDewAggNP.fill(getDeviceName(), "AUTO_DEW_AGG", "Auto Dew Agg", DEW_TAB, IP_RW, 60,
                      IPS_IDLE);

    // Populate PI::USBPortLabelsTP with custom labels
    if (PI::USBPortLabelsTP.size() >= 6)
    {
        PI::USBPortLabelsTP[0].setLabel("USB3 Port 1");
        PI::USBPortLabelsTP[1].setLabel("USB3 Port 2");
        PI::USBPortLabelsTP[2].setLabel("USB3 Port 3");
        PI::USBPortLabelsTP[3].setLabel("USB3 Port 4");
        PI::USBPortLabelsTP[4].setLabel("USB2 Port 5");
        PI::USBPortLabelsTP[5].setLabel("USB2 Port 6");
    }

    ////////////////////////////////////////////////////////////////////////////
    /// Focuser Group
    ////////////////////////////////////////////////////////////////////////////

    // Settings
    FocuserSettingsNP[SETTING_MAX_SPEED].fill("SETTING_MAX_SPEED", "Max Speed (%)", "%.f", 0, 900, 100, 400);
    FocuserSettingsNP.fill(getDeviceName(), "FOCUSER_SETTINGS", "Settings", FOCUS_TAB,
                           IP_RW, 60, IPS_IDLE);
    ////////////////////////////////////////////////////////////////////////////
    /// Firmware Group
    ////////////////////////////////////////////////////////////////////////////
    FirmwareTP[FIRMWARE_VERSION].fill("VERSION", "Version", "NA");
    FirmwareTP[FIRMWARE_UPTIME].fill("UPTIME", "Uptime (h)", "NA");
    FirmwareTP.fill(getDeviceName(), "FIRMWARE_INFO", "Firmware", FIRMWARE_TAB, IP_RO, 60,
                    IPS_IDLE);
    ////////////////////////////////////////////////////////////////////////////
    /// Environment Group
    ////////////////////////////////////////////////////////////////////////////
    addParameter("WEATHER_TEMPERATURE", "Temperature (C)", -15, 35, 15);
    addParameter("WEATHER_HUMIDITY", "Humidity %", 0, 100, 15);
    addParameter("WEATHER_DEWPOINT", "Dew Point (C)", 0, 100, 15);
    setCriticalParameter("WEATHER_TEMPERATURE");

    ////////////////////////////////////////////////////////////////////////////
    /// Serial Connection
    ////////////////////////////////////////////////////////////////////////////
    serialConnection = new Connection::Serial(this);
    serialConnection->registerHandshake([&]()
    {
        return Handshake();
    });
    registerConnection(serialConnection);

    return true;
}

bool PegasusUPB::updateProperties()
{
    INDI::DefaultDevice::updateProperties();

    if (isConnected())
    {
        // Setup Parameters
        setupParams();

        // Main Control
        defineProperty(PowerConsumptionNP);
        defineProperty(RebootSP);

        // Power
        defineProperty(PowerOnBootSP);
        defineProperty(OverCurrentLP);

        // Dew
        if (version == UPB_V2)
            defineProperty(AutoDewAggNP);

        // Focuser
        FI::updateProperties();
        defineProperty(FocuserSettingsNP);

        // Weather
        WI::updateProperties();

        // Power
        PI::updateProperties();

        // Firmware
        defineProperty(FirmwareTP);

        setupComplete = true;
    }
    else
    {
        // Main Control
        deleteProperty(PowerConsumptionNP);
        deleteProperty(RebootSP);

        // Power
        deleteProperty(PowerOnBootSP);
        deleteProperty(OverCurrentLP);

        // Dew
        if (version == UPB_V2)
            deleteProperty(AutoDewAggNP);

        // Focuser
        FI::updateProperties();
        deleteProperty(FocuserSettingsNP);

        // Weather
        WI::updateProperties();

        // Power
        PI::updateProperties();

        deleteProperty(FirmwareTP);

        setupComplete = false;
    }

    return true;
}

const char * PegasusUPB::getDefaultName()
{
    return "Pegasus UPB";
}

//////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////
bool PegasusUPB::Handshake()
{
    char response[PEGASUS_LEN] = {0};
    int nbytes_read = 0;
    PortFD = serialConnection->getPortFD();

    LOG_DEBUG("CMD <P#>");

    if (isSimulation())
    {
        snprintf(response, PEGASUS_LEN, "UPB2_OK");
        nbytes_read = 8;
    }
    else
    {
        int tty_rc = 0, nbytes_written = 0;
        char command[PEGASUS_LEN] = {0};
        tcflush(PortFD, TCIOFLUSH);
        strncpy(command, "P#\n", PEGASUS_LEN);
        if ( (tty_rc = tty_write_string(PortFD, command, &nbytes_written)) != TTY_OK)
        {
            char errorMessage[MAXRBUF];
            tty_error_msg(tty_rc, errorMessage, MAXRBUF);
            LOGF_ERROR("Serial write error: %s", errorMessage);
            return false;
        }

        // Try first with stopChar as the stop character
        if ( (tty_rc = tty_nread_section(PortFD, response, PEGASUS_LEN, stopChar, 1, &nbytes_read)) != TTY_OK)
        {
            // Try 0xA as the stop character
            if (tty_rc == TTY_OVERFLOW || tty_rc == TTY_TIME_OUT)
            {
                tcflush(PortFD, TCIOFLUSH);
                tty_write_string(PortFD, command, &nbytes_written);
                stopChar = 0xA;
                tty_rc = tty_nread_section(PortFD, response, PEGASUS_LEN, stopChar, 1, &nbytes_read);
            }

            if (tty_rc != TTY_OK)
            {
                char errorMessage[MAXRBUF];
                tty_error_msg(tty_rc, errorMessage, MAXRBUF);
                LOGF_ERROR("Serial read error: %s", errorMessage);
                return false;
            }
        }

        cleanupResponse(response);
        tcflush(PortFD, TCIOFLUSH);
    }


    LOGF_DEBUG("RES <%s>", response);

    setupComplete = false;

    version = strstr(response, "UPB2_OK") ? UPB_V2 : UPB_V1;

    if (version == UPB_V1)
    {
        // V1: No LED Toggle, No Variable Output
        PI::SetCapability(POWER_HAS_DC_OUT | POWER_HAS_DEW_OUT | POWER_HAS_VOLTAGE_SENSOR |
                          POWER_HAS_OVERALL_CURRENT | POWER_HAS_PER_PORT_CURRENT | POWER_HAS_AUTO_DEW |
                          POWER_HAS_POWER_CYCLE | POWER_HAS_USB_TOGGLE | POWER_HAS_LED_TOGGLE);
        // 4 DC ports, 2 DEW ports, 0 Variable port, 1 Auto Dew port (global), 6 USB ports (but can only be all toggled on/off at once)
        PI::initProperties(POWER_TAB, 4, 2, 0, 1, 1);
    }
    else // UPB_V2
    {
        // V2: All capabilities
        PI::SetCapability(POWER_HAS_DC_OUT | POWER_HAS_DEW_OUT | POWER_HAS_VARIABLE_OUT |
                          POWER_HAS_VOLTAGE_SENSOR | POWER_HAS_OVERALL_CURRENT | POWER_HAS_PER_PORT_CURRENT |
                          POWER_HAS_LED_TOGGLE | POWER_HAS_AUTO_DEW | POWER_HAS_POWER_CYCLE | POWER_HAS_USB_TOGGLE);
        // 4 DC ports, 3 DEW ports, 1 Variable port, 3 Auto Dew ports (per-port), 6 USB ports
        PI::initProperties(POWER_TAB, 4, 3, 1, 3, 6);
    }

    return true;
}

//////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////
bool PegasusUPB::ISNewSwitch(const char * dev, const char * name, ISState * states, char * names[], int n)
{
    if (dev && !strcmp(dev, getDeviceName()))
    {
        // Reboot
        if (RebootSP.isNameMatch(name))
        {
            RebootSP.setState(reboot() ? IPS_OK : IPS_ALERT);
            RebootSP.apply();
            LOG_INFO("Rebooting device...");
            return true;
        }

        // Power on boot
        if (PowerOnBootSP.isNameMatch(name))
        {
            PowerOnBootSP.update(states, names, n);
            PowerOnBootSP.setState(setPowerOnBoot() ? IPS_OK : IPS_ALERT);
            PowerOnBootSP.apply();
            saveConfig(PowerOnBootSP);
            return true;
        }


        if (strstr(name, "FOCUS"))
            return FI::processSwitch(dev, name, states, names, n);

        // Process power-related switches via PowerInterface
        if (PI::processSwitch(dev, name, states, names, n))
            return true;
    }

    return DefaultDevice::ISNewSwitch(dev, name, states, names, n);
}

//////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////
bool PegasusUPB::ISNewNumber(const char * dev, const char * name, double values[], char * names[], int n)
{
    if (dev && !strcmp(dev, getDeviceName()))
    {
        // Auto Dew Aggressiveness (device-specific property, not in INDI::Power)
        if (AutoDewAggNP.isNameMatch(name))
        {
            if (setAutoDewAgg(values[0]))
            {
                AutoDewAggNP[0].setValue(values[0]);
                AutoDewAggNP.setState(IPS_OK);
            }
            else
            {
                AutoDewAggNP.setState(IPS_ALERT);
            }

            AutoDewAggNP.apply();
            return true;
        }

        // Focuser Settings
        if (FocuserSettingsNP.isNameMatch(name))
        {
            if (setFocuserMaxSpeed(values[0]))
            {
                FocuserSettingsNP[0].setValue(values[0]);
                FocuserSettingsNP.setState(IPS_OK);
            }
            else
            {
                FocuserSettingsNP.setState(IPS_ALERT);
            }

            FocuserSettingsNP.apply();
            return true;
        }

        if (strstr(name, "FOCUS_"))
            return FI::processNumber(dev, name, values, names, n);

        if (strstr(name, "WEATHER_"))
            return WI::processNumber(dev, name, values, names, n);

        // Process power-related numbers via PowerInterface
        if (PI::processNumber(dev, name, values, names, n))
            return true;
    }
    return INDI::DefaultDevice::ISNewNumber(dev, name, values, names, n);
}

//////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////
bool PegasusUPB::ISNewText(const char * dev, const char * name, char * texts[], char * names[], int n)
{
    if (dev && !strcmp(dev, getDeviceName()))
    {
        // USB Labels are now handled by INDI::PowerInterface

        // Process power-related text via PowerInterface
        if (PI::processText(dev, name, texts, names, n))
            return true;
    }

    return INDI::DefaultDevice::ISNewText(dev, name, texts, names, n);
}

//////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////
bool PegasusUPB::sendCommand(const char * cmd, char * res)
{
    int nbytes_read = 0, nbytes_written = 0, tty_rc = 0;
    LOGF_DEBUG("CMD <%s>", cmd);

    if (isSimulation())
    {
        if (!strcmp(cmd, "PS"))
        {
            strncpy(res, "PS:1111:12", PEGASUS_LEN);
        }
        else if (!strcmp(cmd, "PA"))
        {
            strncpy(res, "UPB2:12.0:0.9:10:24.8:37:9.1:1111:111111:153:153:0:0:0:0:0:70:0:0:0000000:0", PEGASUS_LEN);
        }
        else if (!strcmp(cmd, "PC"))
        {
            strncpy(res, "0.40:0.00:0.03:26969", PEGASUS_LEN);
        }
        else if (!strcmp(cmd, "SA"))
        {
            strncpy(res, "3000:0:0:10", PEGASUS_LEN);
        }
        else if (!strcmp(cmd, "SS"))
        {
            strncpy(res, "999", PEGASUS_LEN);
        }
        else if (!strcmp(cmd, "PD"))
        {
            strncpy(res, "210", PEGASUS_LEN);
        }
        else if (!strcmp(cmd, "PV"))
        {
            strncpy(res, "Sim v1.0", PEGASUS_LEN);
        }
        else if (res)
        {
            strncpy(res, cmd, PEGASUS_LEN);
        }

        return true;
    }

    for (int i = 0; i < 2; i++)
    {
        char command[PEGASUS_LEN] = {0};
        tcflush(PortFD, TCIOFLUSH);
        snprintf(command, PEGASUS_LEN, "%s\n", cmd);
        if ( (tty_rc = tty_write_string(PortFD, command, &nbytes_written)) != TTY_OK)
            continue;

        if (!res)
        {
            tcflush(PortFD, TCIOFLUSH);
            return true;
        }

        if ( (tty_rc = tty_nread_section(PortFD, res, PEGASUS_LEN, stopChar, PEGASUS_TIMEOUT, &nbytes_read)) != TTY_OK
                || nbytes_read == 1)
            continue;

        tcflush(PortFD, TCIOFLUSH);

        cleanupResponse(res);
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

bool PegasusUPB::SetUSBPort(size_t port, bool enabled)
{
    // Port numbers in interface are 0-based, but device expects 1-based
    return setUSBPortEnabled(port, enabled);
}

//////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////
IPState PegasusUPB::MoveAbsFocuser(uint32_t targetTicks)
{
    char cmd[PEGASUS_LEN] = {0}, res[PEGASUS_LEN] = {0};
    snprintf(cmd, PEGASUS_LEN, "SM:%u", targetTicks);
    if (sendCommand(cmd, res))
    {
        return (!strcmp(res, cmd) ? IPS_BUSY : IPS_ALERT);
    }

    return IPS_ALERT;
}

//////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////
IPState PegasusUPB::MoveRelFocuser(FocusDirection dir, uint32_t ticks)
{
    return MoveAbsFocuser(dir == FOCUS_INWARD ? FocusAbsPosNP[0].getValue() - ticks : FocusAbsPosNP[0].getValue() + ticks);
}

//////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////
bool PegasusUPB::AbortFocuser()
{
    char res[PEGASUS_LEN] = {0};
    if (sendCommand("SH", res))
    {
        return (!strcmp(res, "SH"));
    }

    return false;
}

//////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////
bool PegasusUPB::ReverseFocuser(bool enabled)
{
    char cmd[PEGASUS_LEN] = {0}, res[PEGASUS_LEN] = {0};
    snprintf(cmd, PEGASUS_LEN, "SR:%d", enabled ? 1 : 0);
    if (sendCommand(cmd, res))
    {
        return (!strcmp(res, cmd));
    }

    return false;
}

//////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////
bool PegasusUPB::SyncFocuser(uint32_t ticks)
{
    char cmd[PEGASUS_LEN] = {0};
    snprintf(cmd, PEGASUS_LEN, "SC:%u", ticks);
    return sendCommand(cmd, nullptr);
}

//////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////
bool PegasusUPB::SetFocuserBacklash(int32_t steps)
{
    char cmd[PEGASUS_LEN] = {0};
    snprintf(cmd, PEGASUS_LEN, "SB:%d", steps);
    return sendCommand(cmd, nullptr);
}

//////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////
bool PegasusUPB::setFocuserMaxSpeed(uint16_t maxSpeed)
{
    char cmd[PEGASUS_LEN] = {0};
    snprintf(cmd, PEGASUS_LEN, "SS:%d", maxSpeed);
    return sendCommand(cmd, nullptr);
}

//////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////
bool PegasusUPB::SetFocuserBacklashEnabled(bool enabled)
{
    char cmd[PEGASUS_LEN] = {0};
    snprintf(cmd, PEGASUS_LEN, "SB:%d", enabled ? 1 : 0);
    return sendCommand(cmd, nullptr);
}

//////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////
bool PegasusUPB::setPowerEnabled(uint8_t port, bool enabled)
{
    char cmd[PEGASUS_LEN] = {0}, res[PEGASUS_LEN] = {0};
    snprintf(cmd, PEGASUS_LEN, "P%d:%d", port, enabled ? 1 : 0);
    if (sendCommand(cmd, res))
    {
        return (!strcmp(res, cmd));
    }

    return false;
}

//////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////
bool PegasusUPB::setPowerLEDEnabled(bool enabled)
{
    char cmd[PEGASUS_LEN] = {0}, res[PEGASUS_LEN] = {0};
    snprintf(cmd, PEGASUS_LEN, "PL:%d", enabled ? 1 : 0);
    if (sendCommand(cmd, res))
    {
        return (!strcmp(res, cmd));
    }

    return false;
}

//////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////
bool PegasusUPB::setAutoDewEnabled(bool enabled)
{
    char cmd[PEGASUS_LEN] = {0}, res[PEGASUS_LEN] = {0};
    snprintf(cmd, PEGASUS_LEN, "PD:%d", enabled ? 1 : 0);
    if (sendCommand(cmd, res))
    {
        return (!strcmp(res, cmd));
    }

    return false;
}

//////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////
bool PegasusUPB::setAutoDewAgg(uint8_t value)
{
    char cmd[PEGASUS_LEN] = {0}, res[PEGASUS_LEN] = {0}, expected[PEGASUS_LEN] = {0};
    snprintf(cmd, PEGASUS_LEN, "PD:%03d", value);
    snprintf(expected, PEGASUS_LEN, "PD:%d", value);
    if (sendCommand(cmd, res))
    {
        return (!strcmp(res, expected));
    }

    return false;
}
//////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////
bool PegasusUPB::setAdjustableOutput(uint8_t voltage)
{
    char cmd[PEGASUS_LEN] = {0}, res[PEGASUS_LEN] = {0};
    snprintf(cmd, PEGASUS_LEN, "P8:%d", voltage);
    if (sendCommand(cmd, res))
    {
        return (!strcmp(res, cmd));
    }

    return false;
}

//////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////
bool PegasusUPB::setPowerOnBoot()
{
    char cmd[PEGASUS_LEN] = {0}, res[PEGASUS_LEN] = {0};
    snprintf(cmd, PEGASUS_LEN, "PE:%d%d%d%d", PowerOnBootSP[POWER_PORT_1].getState() == ISS_ON ? 1 : 0,
             PowerOnBootSP[POWER_PORT_2].getState() == ISS_ON ? 1 : 0,
             PowerOnBootSP[POWER_PORT_3].getState() == ISS_ON ? 1 : 0,
             PowerOnBootSP[POWER_PORT_4].getState() == ISS_ON ? 1 : 0);
    if (sendCommand(cmd, res))
    {
        return (!strcmp(res, "PE:1"));
    }

    return false;
}

//////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////
bool PegasusUPB::getPowerOnBoot()
{
    char res[PEGASUS_LEN] = {0};
    if (sendCommand("PS", res))
    {
        std::vector<std::string> result = split(res, ":");
        if (result.size() != 3)
        {
            LOGF_WARN("Received wrong number (%i) of power on boot data (%s). Retrying...", result.size(), res);
            return false;
        }

        const char *status = result[1].c_str();
        PowerOnBootSP[POWER_PORT_1].setState((status[0] == '1') ? ISS_ON : ISS_OFF);
        PowerOnBootSP[POWER_PORT_2].setState((status[1] == '1') ? ISS_ON : ISS_OFF);
        PowerOnBootSP[POWER_PORT_3].setState((status[2] == '1') ? ISS_ON : ISS_OFF);
        PowerOnBootSP[POWER_PORT_4].setState((status[3] == '1') ? ISS_ON : ISS_OFF);

        if (PI::VariableChannelVoltsNP.size() > 0)
        {
            PI::VariableChannelVoltsNP[0].setValue(std::stod(result[2]));
            PI::VariableChannelVoltsNP.setState(IPS_OK);
        }

        return true;
    }

    return false;
}

//////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////
bool PegasusUPB::setDewPWM(uint8_t id, uint8_t value)
{
    char cmd[PEGASUS_LEN] = {0}, res[PEGASUS_LEN] = {0}, expected[PEGASUS_LEN] = {0};
    snprintf(cmd, PEGASUS_LEN, "P%d:%03d", id, value);
    snprintf(expected, PEGASUS_LEN, "P%d:%d", id, value);
    if (sendCommand(cmd, res))
    {
        return (!strcmp(res, expected));
    }

    return false;
}

//////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////
bool PegasusUPB::setUSBHubEnabled(bool enabled)
{
    char cmd[PEGASUS_LEN] = {0}, expected[PEGASUS_LEN] = {0}, res[PEGASUS_LEN] = {0};
    snprintf(cmd, PEGASUS_LEN, "PU:%d", enabled ? 1 : 0);
    snprintf(expected, PEGASUS_LEN, "PU:%d", enabled ? 0 : 1);
    if (sendCommand(cmd, res))
    {
        return (!strcmp(res, expected));
    }

    return false;
}

//////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////
bool PegasusUPB::setUSBPortEnabled(uint8_t port, bool enabled)
{
    char cmd[PEGASUS_LEN] = {0}, expected[PEGASUS_LEN] = {0}, res[PEGASUS_LEN] = {0};
    snprintf(cmd, PEGASUS_LEN, "U%d:%d", port + 1, enabled ? 1 : 0);
    snprintf(expected, PEGASUS_LEN, "U%d:%d", port + 1, enabled ? 1 : 0);
    if (sendCommand(cmd, res))
    {
        return (!strcmp(res, expected));
    }

    return false;
}

//////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////
bool PegasusUPB::toggleAutoDewV2(size_t port, bool enabled)
{
    char cmd[PEGASUS_LEN] = {0}, expected[PEGASUS_LEN] = {0}, res[PEGASUS_LEN] = {0};

    uint8_t value = 0;

    // Get current states from PI::AutoDewSP, but override the 'port' being changed
    bool dewA_on = (PI::AutoDewSP.size() > 0 && PI::AutoDewSP[0].getState() == ISS_ON);
    bool dewB_on = (PI::AutoDewSP.size() > 1 && PI::AutoDewSP[1].getState() == ISS_ON);
    bool dewC_on = (PI::AutoDewSP.size() > 2 && PI::AutoDewSP[2].getState() == ISS_ON);

    if (port == 0) dewA_on = enabled;
    else if (port == 1) dewB_on = enabled;
    else if (port == 2) dewC_on = enabled;

    if (!dewA_on && !dewB_on && !dewC_on)
        value = 0;
    else if (dewA_on && dewB_on && dewC_on)
        value = 1;
    else if (dewA_on && dewB_on)
        value = 5;
    else if (dewA_on && dewC_on)
        value = 6;
    else if (dewB_on && dewC_on)
        value = 7;
    else if (dewA_on)
        value = 2;
    else if (dewB_on)
        value = 3;
    else if (dewC_on)
        value = 4;

    snprintf(cmd, PEGASUS_LEN, "PD:%d", value);
    snprintf(expected, PEGASUS_LEN, "PD:%d", value);
    if (sendCommand(cmd, res))
    {
        return (!strcmp(res, expected));
    }

    return false;
}

//////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////
bool PegasusUPB::saveConfigItems(FILE * fp)
{
    // Save CCD Config
    INDI::DefaultDevice::saveConfigItems(fp);
    FI::saveConfigItems(fp);
    WI::saveConfigItems(fp);
    PI::saveConfigItems(fp);

    if (version == UPB_V2)
    {
        AutoDewAggNP.save(fp);
    }
    FocuserSettingsNP.save(fp);
    return true;
}

//////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////
void PegasusUPB::TimerHit()
{
    if (!isConnected() || setupComplete == false)
    {
        SetTimer(getCurrentPollingPeriod());
        return;
    }

    if (getSensorData())
    {
        getPowerData();
        getStepperData();

        if (version == UPB_V2)
            getDewAggData();
    }

    SetTimer(getCurrentPollingPeriod());
}

//////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////
bool PegasusUPB::sendFirmware()
{
    char res[PEGASUS_LEN] = {0};
    if (sendCommand("PV", res))
    {
        LOGF_INFO("Detected firmware %s", res);
        FirmwareTP[FIRMWARE_VERSION].setText(res);
        FirmwareTP.apply();
        return true;
    }

    return false;
}

//////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////
bool PegasusUPB::sensorUpdated(const std::vector<std::string> &result, uint8_t start, uint8_t end)
{
    if (lastSensorData.empty())
        return true;

    for (uint8_t index = start; index <= end; index++)
    {
        if (index >= lastSensorData.size() or result[index] != lastSensorData[index])
            return true;
    }

    return false;
}

//////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////
bool PegasusUPB::stepperUpdated(const std::vector<std::string> &result, uint8_t index)
{
    if (lastStepperData.empty())
        return true;

    return index >= lastStepperData.size() or result[index] != lastSensorData[index];
}

//////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////
bool PegasusUPB::getSensorData()
{
    char res[PEGASUS_LEN] = {0};
    if (sendCommand("PA", res))
    {
        std::vector<std::string> result = split(res, ":");
        if ( (version == UPB_V1 && result.size() != 19) ||
                (version == UPB_V2 && result.size() != 21))
        {
            LOGF_WARN("Received wrong number (%i) of detailed sensor data (%s). Retrying...", result.size(), res);
            return false;
        }

        if (result == lastSensorData)
            return true;

        // Power Sensors
        PowerSensorsNP[SENSOR_VOLTAGE].setValue(std::stod(result[1]));
        PowerSensorsNP[SENSOR_CURRENT].setValue(std::stod(result[2]));
        PowerSensorsNP[SENSOR_POWER].setValue(std::stod(result[3]));
        PowerSensorsNP.setState(IPS_OK);
        //if (lastSensorData[0] != result[0] || lastSensorData[1] != result[1] || lastSensorData[2] != result[2])
        if (sensorUpdated(result, 0, 2))
            PowerSensorsNP.apply();

        // Environment Sensors
        setParameterValue("WEATHER_TEMPERATURE", std::stod(result[4]));
        setParameterValue("WEATHER_HUMIDITY", std::stod(result[5]));
        setParameterValue("WEATHER_DEWPOINT", std::stod(result[6]));
        //if (lastSensorData[4] != result[4] || lastSensorData[5] != result[5] || lastSensorData[6] != result[6])
        if (sensorUpdated(result, 4, 6))
        {
            if (WI::syncCriticalParameters())
                critialParametersLP.apply();
            ParametersNP.setState(IPS_OK);
            ParametersNP.apply();
        }

        // Port Status
        const char * portStatus = result[7].c_str();
        if (PI::PowerChannelsSP.size() >= 1)
            PI::PowerChannelsSP[0].setState((portStatus[0] == '1') ? ISS_ON : ISS_OFF);
        if (PI::PowerChannelsSP.size() >= 2)
            PI::PowerChannelsSP[1].setState((portStatus[1] == '1') ? ISS_ON : ISS_OFF);
        if (PI::PowerChannelsSP.size() >= 3)
            PI::PowerChannelsSP[2].setState((portStatus[2] == '1') ? ISS_ON : ISS_OFF);
        if (PI::PowerChannelsSP.size() >= 4)
            PI::PowerChannelsSP[3].setState((portStatus[3] == '1') ? ISS_ON : ISS_OFF);
        if (sensorUpdated(result, 7, 7))
            PI::PowerChannelsSP.apply();

        // Hub Status
        const char * usb_status = result[8].c_str();
        if (version == UPB_V1)
        {
            // For V1, map global USB hub status to PI::USBPortSP[0]
            if (PI::USBPortSP.size() > 0)
            {
                PI::USBPortSP[0].setState((usb_status[0] == '0') ? ISS_ON : ISS_OFF);
                if (sensorUpdated(result, 8, 8))
                {
                    PI::USBPortSP.apply();
                }
            }
        }
        else
        {
            // V2 has per-port USB control, update PI::USBPortSP directly
            if (sensorUpdated(result, 8, 8))
            {
                for (size_t i = 0; i < PI::USBPortSP.size() && i < 6; ++i) // Assuming 6 USB ports
                {
                    PI::USBPortSP[i].setState((usb_status[i] == '1') ? ISS_ON : ISS_OFF);
                }
                PI::USBPortSP.apply();
            }
        }

        // From here, we get differences between v1 and v2 readings
        int index = 9;
        // Dew DEW
        if (PI::DewChannelDutyCycleNP.size() >= 1)
            PI::DewChannelDutyCycleNP[0].setValue(std::stod(result[index]) / 255.0 * 100.0);
        if (PI::DewChannelDutyCycleNP.size() >= 2)
            PI::DewChannelDutyCycleNP[1].setValue(std::stod(result[index + 1]) / 255.0 * 100.0);
        if (version == UPB_V2 && PI::DewChannelDutyCycleNP.size() >= 3)
            PI::DewChannelDutyCycleNP[2].setValue(std::stod(result[index + 2]) / 255.0 * 100.0);
        if (sensorUpdated(result, index, version == UPB_V1 ? index + 1 : index + 2))
            PI::DewChannelDutyCycleNP.apply();

        // Update Dew Channel switches based on actual power status
        // If Auto Dew is enabled, it may turn channels on/off, so we need to reflect that
        bool dewChannelChanged = false;
        if (PI::DewChannelsSP.size() >= 1)
        {
            auto newState = (std::stoi(result[index]) > 0) ? ISS_ON : ISS_OFF;
            if (PI::DewChannelsSP[0].getState() != newState)
            {
                PI::DewChannelsSP[0].setState(newState);
                dewChannelChanged = true;
            }
        }
        if (PI::DewChannelsSP.size() >= 2)
        {
            auto newState = (std::stoi(result[index + 1]) > 0) ? ISS_ON : ISS_OFF;
            if (PI::DewChannelsSP[1].getState() != newState)
            {
                PI::DewChannelsSP[1].setState(newState);
                dewChannelChanged = true;
            }
        }
        if (version == UPB_V2 && PI::DewChannelsSP.size() >= 3)
        {
            auto newState = (std::stoi(result[index + 2]) > 0) ? ISS_ON : ISS_OFF;
            if (PI::DewChannelsSP[2].getState() != newState)
            {
                PI::DewChannelsSP[2].setState(newState);
                dewChannelChanged = true;
            }
        }
        if (dewChannelChanged)
            PI::DewChannelsSP.apply();

        index = (version == UPB_V1) ? 11 : 12;

        const double ampDivision = (version == UPB_V1) ? 400.0 : 480.0;

        // Current draw
        if (PI::PowerChannelCurrentNP.size() >= 1)
            PI::PowerChannelCurrentNP[0].setValue(std::stod(result[index]) / ampDivision);
        if (PI::PowerChannelCurrentNP.size() >= 2)
            PI::PowerChannelCurrentNP[1].setValue(std::stod(result[index + 1]) / ampDivision);
        if (PI::PowerChannelCurrentNP.size() >= 3)
            PI::PowerChannelCurrentNP[2].setValue(std::stod(result[index + 2]) / ampDivision);
        if (PI::PowerChannelCurrentNP.size() >= 4)
            PI::PowerChannelCurrentNP[3].setValue(std::stod(result[index + 3]) / ampDivision);
        if (sensorUpdated(result, index, index + 3))
            PI::PowerChannelCurrentNP.apply();

        index = (version == UPB_V1) ? 15 : 16;

        if (PI::DewChannelCurrentNP.size() >= 1)
            PI::DewChannelCurrentNP[0].setValue(std::stod(result[index]) / ampDivision);
        if (PI::DewChannelCurrentNP.size() >= 2)
            PI::DewChannelCurrentNP[1].setValue(std::stod(result[index + 1]) / ampDivision);
        if (version == UPB_V2 && PI::DewChannelCurrentNP.size() >= 3)
            PI::DewChannelCurrentNP[2].setValue(std::stod(result[index + 2]) / 700);
        if (sensorUpdated(result, index, version == UPB_V1 ? index + 1 : index + 2))
            PI::DewChannelCurrentNP.apply();

        index = (version == UPB_V1) ? 17 : 19;

        // Over Current
        //if (lastSensorData[index] != result[index])
        if (sensorUpdated(result, index, index))
        {
            const char * over_curent = result[index].c_str();
            OverCurrentLP[POWER_PORT_1].setState((over_curent[0] == '0') ? IPS_OK : IPS_ALERT);
            OverCurrentLP[POWER_PORT_2].setState((over_curent[1] == '0') ? IPS_OK : IPS_ALERT);
            OverCurrentLP[POWER_PORT_3].setState((over_curent[2] == '0') ? IPS_OK : IPS_ALERT);
            OverCurrentLP[POWER_PORT_4].setState((over_curent[3] == '0') ? IPS_OK : IPS_ALERT);
            if (version == UPB_V2)
            {
                OverCurrentLP[DEW_A].setState((over_curent[4] == '0') ? IPS_OK : IPS_ALERT);
                OverCurrentLP[DEW_B].setState((over_curent[5] == '0') ? IPS_OK : IPS_ALERT);
                OverCurrentLP[DEW_C].setState((over_curent[6] == '0') ? IPS_OK : IPS_ALERT);
            }

            OverCurrentLP.apply();
        }

        index = (version == UPB_V1) ? 18 : 20;

        // Auto Dew
        if (version == UPB_V1)
        {
            //if (lastSensorData[index] != result[index])
            if (sensorUpdated(result, index, index))
            {
                PI::AutoDewSP[0].setState((std::stoi(result[index]) == 1) ? ISS_ON : ISS_OFF);
                PI::AutoDewSP.apply();
            }
        }
        else
        {
            // V2 has per-port auto dew control, update PI::AutoDewSP directly
            if (sensorUpdated(result, index, index))
            {
                int value = std::stoi(result[index]);
                // Reset all auto dew switches first
                for (size_t i = 0; i < PI::AutoDewSP.size(); ++i)
                {
                    PI::AutoDewSP[i].setState(ISS_OFF);
                }

                // Set states based on the value
                switch (value)
                {
                    case 1: // All three on
                        if (PI::AutoDewSP.size() >= 3)
                            PI::AutoDewSP[0].setState(ISS_ON);
                        if (PI::AutoDewSP.size() >= 3)
                            PI::AutoDewSP[1].setState(ISS_ON);
                        if (PI::AutoDewSP.size() >= 3)
                            PI::AutoDewSP[2].setState(ISS_ON);
                        break;
                    case 2: // A on
                        if (PI::AutoDewSP.size() >= 1)
                            PI::AutoDewSP[0].setState(ISS_ON);
                        break;
                    case 3: // B on
                        if (PI::AutoDewSP.size() >= 2)
                            PI::AutoDewSP[1].setState(ISS_ON);
                        break;
                    case 4: // C on
                        if (PI::AutoDewSP.size() >= 3)
                            PI::AutoDewSP[2].setState(ISS_ON);
                        break;
                    case 5: // A & B on
                        if (PI::AutoDewSP.size() >= 1)
                            PI::AutoDewSP[0].setState(ISS_ON);
                        if (PI::AutoDewSP.size() >= 2)
                            PI::AutoDewSP[1].setState(ISS_ON);
                        break;
                    case 6: // A & C on
                        if (PI::AutoDewSP.size() >= 1)
                            PI::AutoDewSP[0].setState(ISS_ON);
                        if (PI::AutoDewSP.size() >= 3)
                            PI::AutoDewSP[2].setState(ISS_ON);
                        break;
                    case 7: // B & C on
                        if (PI::AutoDewSP.size() >= 2)
                            PI::AutoDewSP[1].setState(ISS_ON);
                        if (PI::AutoDewSP.size() >= 3)
                            PI::AutoDewSP[2].setState(ISS_ON);
                        break;
                    default:
                        break;
                }
                PI::AutoDewSP.apply();
            }
        }

        lastSensorData = result;
        return true;
    }

    return false;
}

//////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////
bool PegasusUPB::getPowerData()
{
    char res[PEGASUS_LEN] = {0};
    if (sendCommand("PC", res))
    {
        std::vector<std::string> result = split(res, ":");
        // Minimum is 3 values. Uptime is optional.
        if (result.size() < 3)
        {
            LOGF_WARN("Received wrong number (%i) of power sensor data (%s). Retrying...", result.size(), res);
            return false;
        }

        if (result == lastPowerData)
            return true;

        PowerConsumptionNP[CONSUMPTION_AVG_AMPS].setValue(std::stod(result[0]));
        PowerConsumptionNP[CONSUMPTION_AMP_HOURS].setValue(std::stod(result[1]));
        PowerConsumptionNP[CONSUMPTION_WATT_HOURS].setValue(std::stod(result[2]));
        PowerConsumptionNP.setState(IPS_OK);
        PowerConsumptionNP.apply();

        if (result.size() == 4)
        {
            try
            {
                std::chrono::milliseconds uptime(std::stol(result[3]));
                using dhours = std::chrono::duration<double, std::ratio<3600>>;
                std::stringstream ss;
                ss << std::fixed << std::setprecision(3) << dhours(uptime).count();
                FirmwareTP[FIRMWARE_UPTIME].setText(ss.str().c_str());
            }
            catch(...)
            {
                // Uptime not critical, so just put debug statement on failure.
                FirmwareTP[FIRMWARE_UPTIME].setText("NA");
                LOGF_DEBUG("Failed to process uptime: %s", result[3].c_str());
            }
            FirmwareTP.apply();
        }


        lastPowerData = result;
        return true;
    }

    return false;
}

//////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////
bool PegasusUPB::getStepperData()
{
    char res[PEGASUS_LEN] = {0};
    if (sendCommand("SA", res))
    {
        std::vector<std::string> result = split(res, ":");
        if (result.size() != 4)
        {
            LOGF_WARN("Received wrong number (%i) of stepper sensor data (%s). Retrying...", result.size(), res);
            return false;
        }

        if (result == lastStepperData)
            return true;

        FocusAbsPosNP[0].setValue(std::stoi(result[0]));
        focusMotorRunning = (std::stoi(result[1]) == 1);

        if (FocusAbsPosNP.getState() == IPS_BUSY && focusMotorRunning == false)
        {
            FocusAbsPosNP.setState(IPS_OK);
            FocusRelPosNP.setState(IPS_OK);
            FocusAbsPosNP.apply();
            FocusRelPosNP.apply();
        }
        else if (stepperUpdated(result, 0))
            FocusAbsPosNP.apply();

        FocusReverseSP[INDI_ENABLED].setState((std::stoi(result[2]) == 1) ? ISS_ON : ISS_OFF);
        FocusReverseSP[INDI_DISABLED].setState((std::stoi(result[2]) == 1) ? ISS_OFF : ISS_ON);

        if (stepperUpdated(result, 1))
            FocusReverseSP.apply();

        uint16_t backlash = std::stoi(result[3]);
        if (backlash == 0)
        {
            FocusBacklashNP[0].setValue(backlash);
            FocusBacklashSP[INDI_ENABLED].setState(ISS_OFF);
            FocusBacklashSP[INDI_DISABLED].setState(ISS_ON);
            if (stepperUpdated(result, 3))
            {
                FocusBacklashSP.apply();
                FocuserSettingsNP.apply();
            }
        }
        else
        {
            FocusBacklashSP[INDI_ENABLED].setState(ISS_ON);
            FocusBacklashSP[INDI_DISABLED].setState(ISS_OFF);
            FocusBacklashNP[0].setValue(backlash);
            if (stepperUpdated(result, 3))
            {
                FocusBacklashSP.apply();
                FocuserSettingsNP.apply();
            }
        }

        lastStepperData = result;
        return true;
    }

    return false;
}
bool PegasusUPB::getDewAggData()
{
    char res[PEGASUS_LEN] = {0};
    if (sendCommand("DA", res))
    {
        std::vector<std::string> result = split(res, ":");
        if (result.size() != 2)
        {
            LOGF_WARN("Received wrong number (%i) of dew aggresiveness data (%s). Retrying...", result.size(), res);
            return false;
        }

        if (result == lastDewAggData)
            return true;

        AutoDewAggNP[0].setValue(std::stod(result[1]));
        AutoDewAggNP.setState(IPS_OK);
        AutoDewAggNP.apply();

        lastDewAggData = result;
        return true;
    }
    return false;
}
//////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////
bool PegasusUPB::reboot()
{
    return sendCommand("PF", nullptr);
}

//////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////
std::vector<std::string> PegasusUPB::split(const std::string &input, const std::string &regex)
{
    // passing -1 as the submatch index parameter performs splitting
    std::regex re(regex);
    std::sregex_token_iterator
    first{input.begin(), input.end(), re, -1},
          last;
    return {first, last};
}

//////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////
bool PegasusUPB::setupParams()
{
    if (version == UPB_V2)
        getPowerOnBoot();

    sendFirmware();

    // Get Max Focuser Speed
    char res[PEGASUS_LEN] = {0};
    if (sendCommand("SS", res))
    {
        try
        {
            uint32_t value = std::stol(res);
            if (value == UINT16_MAX)
            {
                LOGF_WARN("Invalid maximum speed detected: %u. Please set maximum speed appropriate for your motor focus type (0-900)",
                          value);
                FocuserSettingsNP.setState(IPS_ALERT);
            }
            else
            {
                FocuserSettingsNP[SETTING_MAX_SPEED].setValue(value);
                FocuserSettingsNP.setState(IPS_OK);
            }
        }
        catch(...)
        {
            LOGF_WARN("Failed to process focuser max speed: %s", res);
            FocuserSettingsNP.setState(IPS_ALERT);
        }
    }

    return false;
}

//////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////
void PegasusUPB::cleanupResponse(char *response)
{
    std::string s(response);
    s.erase(std::remove_if(s.begin(), s.end(),
                           [](unsigned char x)
    {
        return std::isspace(x);
    }), s.end());
    strncpy(response, s.c_str(), PEGASUS_LEN);
}

//////////////////////////////////////////////////////////////////////
/// Power Interface Implementations
//////////////////////////////////////////////////////////////////////
bool PegasusUPB::SetPowerPort(size_t port, bool enabled)
{
    // Port numbers in interface are 0-based, but device expects 1-based
    return setPowerEnabled(port + 1, enabled);
}

bool PegasusUPB::SetDewPort(size_t port, bool enabled, double dutyCycle)
{
    // DEW ports are 5, 6, 7 for dew heaters A, B, C (device uses 1-based indexing)
    // Convert duty cycle percentage (0-100) to 0-255 range
    auto pwmValue = static_cast<uint8_t>(dutyCycle / 100.0 * 255.0);
    return setDewPWM(port + 5, enabled ? pwmValue : 0);
}

bool PegasusUPB::SetVariablePort(size_t port, bool enabled, double voltage)
{
    INDI_UNUSED(port);  // UPB only has one adjustable output
    if (!enabled)
    {
        return setAdjustableOutput(0);
    }
    return setAdjustableOutput(static_cast<uint8_t>(voltage));
}

bool PegasusUPB::SetLEDEnabled(bool enabled)
{
    return setPowerLEDEnabled(enabled);
}

bool PegasusUPB::SetAutoDewEnabled(size_t port, bool enabled)
{
    if (version == UPB_V1)
    {
        // V1 only has global auto dew control
        INDI_UNUSED(port);
        return setAutoDewEnabled(enabled);
    }
    else
    {
        // V2 has per-port auto dew control.
        // Call the re-written toggleAutoDewV2 with the specific port and enabled state.
        return toggleAutoDewV2(port, enabled);
    }
}

bool PegasusUPB::CyclePower()
{
    // PZ:1 cycles power off then on to all ports
    char cmd[PEGASUS_LEN] = {0}, res[PEGASUS_LEN] = {0};
    snprintf(cmd, PEGASUS_LEN, "PZ:1");
    if (sendCommand(cmd, res))
    {
        return (!strcmp(cmd, res));
    }
    return false;
}
