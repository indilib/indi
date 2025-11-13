/*******************************************************************************
  Copyright(c) 2019-2026 Jasem Mutlaq. All rights reserved.

  Pegasus Pocket Power Box Driver.

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

#include "pegasus_ppb.h"
#include "indicom.h"
#include "connectionplugins/connectionserial.h"

#include <memory>
#include <regex>
#include <termios.h>
#include <cstring>
#include <sys/ioctl.h>

// We declare an auto pointer to PegasusPPB.
static std::unique_ptr<PegasusPPB> pocket_power_box(new PegasusPPB());

PegasusPPB::PegasusPPB() : INDI::DefaultDevice(), INDI::WeatherInterface(this), INDI::PowerInterface(this)
{
    setVersion(1, 1);
    lastSensorData.reserve(PA_N);
}

bool PegasusPPB::initProperties()
{
    INDI::DefaultDevice::initProperties();

    setDriverInterface(AUX_INTERFACE | WEATHER_INTERFACE | POWER_INTERFACE);

    WI::initProperties(ENVIRONMENT_TAB, ENVIRONMENT_TAB);

    addAuxControls();

    ////////////////////////////////////////////////////////////////////////////
    /// Main Control Panel
    ////////////////////////////////////////////////////////////////////////////
    // DSLR on/off
    DSLRPowerSP[INDI_ENABLED].fill("INDI_ENABLED", "On", ISS_OFF);
    DSLRPowerSP[INDI_DISABLED].fill("INDI_DISABLED", "Off", ISS_ON);
    DSLRPowerSP.fill(getDeviceName(), "DSLR_POWER", "DSLR Power", MAIN_CONTROL_TAB, IP_RW,
                     ISR_1OFMANY, 60, IPS_IDLE);

    // Reboot
    RebootSP[0].fill("REBOOT", "Reboot Device", ISS_OFF);
    RebootSP.fill(getDeviceName(), "REBOOT_DEVICE", "Device", MAIN_CONTROL_TAB, IP_RW, ISR_ATMOST1,
                  60, IPS_IDLE);

    ////////////////////////////////////////////////////////////////////////////
    /// Power Group
    ////////////////////////////////////////////////////////////////////////////

    // Power on Boot
    PowerOnBootSP[POWER_PORT_1].fill("POWER_PORT_1", "Port 1", ISS_ON);
    PowerOnBootSP[POWER_PORT_2].fill("POWER_PORT_2", "Port 2", ISS_ON);
    PowerOnBootSP[POWER_PORT_3].fill("POWER_PORT_3", "Port 3", ISS_ON);
    PowerOnBootSP[POWER_PORT_4].fill("POWER_PORT_4", "Port 4", ISS_ON);
    PowerOnBootSP.fill(getDeviceName(), "POWER_ON_BOOT", "Power On Boot", MAIN_CONTROL_TAB,
                       IP_RW, ISR_NOFMANY, 60, IPS_IDLE);

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

bool PegasusPPB::updateProperties()
{
    INDI::DefaultDevice::updateProperties();

    if (isConnected())
    {
        // Main Control
        defineProperty(DSLRPowerSP);
        defineProperty(PowerOnBootSP);
        defineProperty(RebootSP);

        WI::updateProperties();
        PI::updateProperties();

        setupComplete = true;
    }
    else
    {
        // Main Control
        deleteProperty(DSLRPowerSP);
        deleteProperty(PowerOnBootSP);
        deleteProperty(RebootSP);

        WI::updateProperties();
        PI::updateProperties();

        setupComplete = false;
    }

    return true;
}

const char * PegasusPPB::getDefaultName()
{
    return "Pegasus PPB";
}

bool PegasusPPB::Handshake()
{
    int tty_rc = 0, nbytes_written = 0, nbytes_read = 0;
    char command[PEGASUS_LEN] = {0}, response[PEGASUS_LEN] = {0};

    PortFD = serialConnection->getPortFD();

    LOG_DEBUG("CMD <P#>");

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

    tcflush(PortFD, TCIOFLUSH);
    response[nbytes_read - 1] = '\0';
    LOGF_DEBUG("RES <%s>", response);

    setupComplete = false;

    PI::SetCapability(POWER_HAS_DC_OUT | POWER_HAS_DEW_OUT | POWER_HAS_VOLTAGE_SENSOR |
                      POWER_HAS_OVERALL_CURRENT | POWER_HAS_AUTO_DEW | POWER_HAS_POWER_CYCLE | POWER_HAS_LED_TOGGLE);
    // 1 DC port group (controls all 4 outputs together), 2 DEW ports, 0 Variable port, 1 Auto Dew port (global), 0 USB ports
    PI::initProperties(POWER_TAB, 1, 2, 0, 1, 0);

    return !strcmp(response, "PPB_OK");
}

bool PegasusPPB::ISNewSwitch(const char * dev, const char * name, ISState * states, char * names[], int n)
{
    if (dev && !strcmp(dev, getDeviceName()))
    {
        // DSLR
        if (DSLRPowerSP.isNameMatch(name))
        {
            DSLRPowerSP.update(states, names, n);
            char cmd[PEGASUS_LEN] = {0}, res[PEGASUS_LEN] = {0};
            snprintf(cmd, PEGASUS_LEN, "P2:%d", (DSLRPowerSP[INDI_ENABLED].getState() == ISS_ON) ? 1 : 0);
            DSLRPowerSP.setState(sendCommand(cmd, res) ? IPS_OK : IPS_ALERT);
            DSLRPowerSP.apply();
            return true;
        }

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
            saveConfig(true, PowerOnBootSP.getName());
            return true;
        }

        // Process power-related switches via PowerInterface
        if (PI::processSwitch(dev, name, states, names, n))
            return true;
    }

    return DefaultDevice::ISNewSwitch(dev, name, states, names, n);
}

bool PegasusPPB::ISNewNumber(const char * dev, const char * name, double values[], char * names[], int n)
{
    if (dev && !strcmp(dev, getDeviceName()))
    {
        if (strstr(name, "WEATHER_"))
            return WI::processNumber(dev, name, values, names, n);

        // Process power-related numbers via PowerInterface
        if (PI::processNumber(dev, name, values, names, n))
            return true;
    }
    return INDI::DefaultDevice::ISNewNumber(dev, name, values, names, n);
}

bool PegasusPPB::ISNewText(const char * dev, const char * name, char * texts[], char * names[], int n)
{
    if (dev && !strcmp(dev, getDeviceName()))
    {
        // Process power-related text via PowerInterface
        if (PI::processText(dev, name, texts, names, n))
            return true;
    }

    return INDI::DefaultDevice::ISNewText(dev, name, texts, names, n);
}

bool PegasusPPB::sendCommand(const char * cmd, char * res)
{
    int nbytes_read = 0, nbytes_written = 0, tty_rc = 0;
    char command[PEGASUS_LEN] = {0};
    LOGF_DEBUG("CMD <%s>", cmd);

    for (int i = 0; i < 2; i++)
    {
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

bool PegasusPPB::setAutoDewEnabled(bool enabled)
{
    char cmd[PEGASUS_LEN] = {0}, res[PEGASUS_LEN] = {0};
    snprintf(cmd, PEGASUS_LEN, "PD:%d", enabled ? 1 : 0);
    if (sendCommand(cmd, res))
    {
        return (!strcmp(res, cmd));
    }

    return false;
}

bool PegasusPPB::setPowerEnabled(uint8_t port, bool enabled)
{
    char cmd[PEGASUS_LEN] = {0}, res[PEGASUS_LEN] = {0};
    snprintf(cmd, PEGASUS_LEN, "P%d:%d", port, enabled ? 1 : 0);
    if (sendCommand(cmd, res))
    {
        return (!strcmp(res, cmd));
    }

    return false;
}

bool PegasusPPB::setPowerOnBoot()
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

bool PegasusPPB::setDewPWM(uint8_t id, uint8_t value)
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

bool PegasusPPB::saveConfigItems(FILE * fp)
{
    INDI::DefaultDevice::saveConfigItems(fp);
    WI::saveConfigItems(fp);
    PI::saveConfigItems(fp);

    return true;
}

void PegasusPPB::TimerHit()
{
    if (!isConnected() || setupComplete == false)
    {
        SetTimer(getCurrentPollingPeriod());
        return;
    }

    getSensorData();
    SetTimer(getCurrentPollingPeriod());
}

bool PegasusPPB::sendFirmware()
{
    char res[PEGASUS_LEN] = {0};
    if (sendCommand("PV", res))
    {
        LOGF_INFO("Detected firmware %s", res);
        return true;
    }

    return false;
}

bool PegasusPPB::getSensorData()
{
    char res[PEGASUS_LEN] = {0};
    if (sendCommand("PA", res))
    {
        std::vector<std::string> result = split(res, ":");
        if (result.size() < PA_N)
        {
            LOGF_WARN("Received wrong number of detailed sensor data. Expected at least %d, got %zu. Retrying...", PA_N, result.size());
            return false;
        }

        if (result == lastSensorData)
            return true;

        try
        {
            // Power Sensors
            if (result.size() > PA_VOLTAGE && result.size() > PA_CURRENT)
            {
                PI::PowerSensorsNP[PI::SENSOR_VOLTAGE].setValue(std::stod(result[PA_VOLTAGE]));
                PI::PowerSensorsNP[PI::SENSOR_CURRENT].setValue(std::stod(result[PA_CURRENT]) / 65.0);
                PI::PowerSensorsNP.setState(IPS_OK);
                if (lastSensorData.size() > PA_CURRENT &&
                    (lastSensorData[PA_VOLTAGE] != result[PA_VOLTAGE] || lastSensorData[PA_CURRENT] != result[PA_CURRENT]))
                    PI::PowerSensorsNP.apply();
            }

            // Environment Sensors
            if (result.size() > PA_TEMPERATURE && result.size() > PA_HUMIDITY && result.size() > PA_DEW_POINT)
            {
                setParameterValue("WEATHER_TEMPERATURE", std::stod(result[PA_TEMPERATURE]));
                setParameterValue("WEATHER_HUMIDITY", std::stod(result[PA_HUMIDITY]));
                setParameterValue("WEATHER_DEWPOINT", std::stod(result[PA_DEW_POINT]));
                if (lastSensorData.size() > PA_DEW_POINT &&
                    (lastSensorData[PA_TEMPERATURE] != result[PA_TEMPERATURE] ||
                     lastSensorData[PA_HUMIDITY] != result[PA_HUMIDITY] ||
                     lastSensorData[PA_DEW_POINT] != result[PA_DEW_POINT]))
                {
                    if (WI::syncCriticalParameters())
                        critialParametersLP.apply();
                    ParametersNP.setState(IPS_OK);
                    ParametersNP.apply();
                }
            }

            // Power Channel (single port group controlling all 4 physical outputs together)
            if (result.size() > PA_PORT_STATUS && !result[PA_PORT_STATUS].empty())
            {
                const std::string& portStatus = result[PA_PORT_STATUS];
                if (PI::PowerChannelsSP.size() >= 1 && portStatus.length() > 0)
                {
                    PI::PowerChannelsSP[0].setState((portStatus[0] == '1') ? ISS_ON : ISS_OFF);
                    if (lastSensorData.size() > PA_PORT_STATUS && lastSensorData[PA_PORT_STATUS] != result[PA_PORT_STATUS])
                        PI::PowerChannelsSP.apply();
                }
            }

            // DSLR Power Status (This is a specific power output, not a generic USB port)
            if (result.size() > PA_DSLR_STATUS && !result[PA_DSLR_STATUS].empty())
            {
                int dslrStatus = std::stoi(result[PA_DSLR_STATUS]);
                DSLRPowerSP[INDI_ENABLED].setState((dslrStatus == 1) ? ISS_ON : ISS_OFF);
                DSLRPowerSP[INDI_DISABLED].setState((dslrStatus == 0) ? ISS_ON : ISS_OFF);
                DSLRPowerSP.setState((dslrStatus == 1) ? IPS_OK : IPS_IDLE);
                if (lastSensorData.size() > PA_DSLR_STATUS && lastSensorData[PA_DSLR_STATUS] != result[PA_DSLR_STATUS])
                    DSLRPowerSP.apply();
            }

            // Dew PWM (2 ports)
            if (result.size() > PA_DEW_1 && result.size() > PA_DEW_2)
            {
                if (PI::DewChannelDutyCycleNP.size() >= 1)
                    PI::DewChannelDutyCycleNP[0].setValue(std::stod(result[PA_DEW_1]) / 255.0 * 100.0);
                if (PI::DewChannelDutyCycleNP.size() >= 2)
                    PI::DewChannelDutyCycleNP[1].setValue(std::stod(result[PA_DEW_2]) / 255.0 * 100.0);
                if (lastSensorData.size() > PA_DEW_2 &&
                    (lastSensorData[PA_DEW_1] != result[PA_DEW_1] || lastSensorData[PA_DEW_2] != result[PA_DEW_2]))
                    PI::DewChannelDutyCycleNP.apply();
            }

            // Update Dew Channel switches based on actual power status
            // If Auto Dew is enabled, it may turn channels on/off, so we need to reflect that
            if (result.size() > PA_DEW_1 && result.size() > PA_DEW_2)
            {
                bool dewChannelChanged = false;
                if (PI::DewChannelsSP.size() >= 1)
                {
                    auto newState = (std::stoi(result[PA_DEW_1]) > 0) ? ISS_ON : ISS_OFF;
                    if (PI::DewChannelsSP[0].getState() != newState)
                    {
                        PI::DewChannelsSP[0].setState(newState);
                        dewChannelChanged = true;
                    }
                }
                if (PI::DewChannelsSP.size() >= 2)
                {
                    auto newState = (std::stoi(result[PA_DEW_2]) > 0) ? ISS_ON : ISS_OFF;
                    if (PI::DewChannelsSP[1].getState() != newState)
                    {
                        PI::DewChannelsSP[1].setState(newState);
                        dewChannelChanged = true;
                    }
                }
                if (dewChannelChanged)
                    PI::DewChannelsSP.apply();
            }

            // Auto Dew (global)
            if (result.size() > PA_AUTO_DEW && !result[PA_AUTO_DEW].empty())
            {
                if (PI::AutoDewSP.size() >= 1)
                    PI::AutoDewSP[0].setState((std::stoi(result[PA_AUTO_DEW]) == 1) ? ISS_ON : ISS_OFF);
                if (lastSensorData.size() > PA_AUTO_DEW && lastSensorData[PA_AUTO_DEW] != result[PA_AUTO_DEW])
                    PI::AutoDewSP.apply();
            }

            lastSensorData = result;
            return true;
        }
        catch (const std::exception& e)
        {
            LOGF_ERROR("Error parsing sensor data: %s. Response was: %s", e.what(), res);
            return false;
        }
    }

    return false;
}

// Device Control
bool PegasusPPB::reboot()
{
    return sendCommand("PF", nullptr);
}

std::vector<std::string> PegasusPPB::split(const std::string &input, const std::string &regex)
{
    // passing -1 as the submatch index parameter performs splitting
    std::regex re(regex);
    std::sregex_token_iterator
    first{input.begin(), input.end(), re, -1},
          last;
    return {first, last};
}

//////////////////////////////////////////////////////////////////////
/// Power Interface Implementations
//////////////////////////////////////////////////////////////////////
bool PegasusPPB::SetPowerPort(size_t port, bool enabled)
{
    // Port numbers in interface are 0-based, but device expects 1-based
    return setPowerEnabled(port + 1, enabled);
}

bool PegasusPPB::SetDewPort(size_t port, bool enabled, double dutyCycle)
{
    // DEW ports are 3, 4 for dew heaters A, B (device uses 1-based indexing)
    // Convert duty cycle percentage (0-100) to 0-255 range
    auto pwmValue = static_cast<uint8_t>(dutyCycle / 100.0 * 255.0);
    return setDewPWM(port + 3, enabled ? pwmValue : 0);
}

bool PegasusPPB::SetVariablePort(size_t port, bool enabled, double voltage)
{
    INDI_UNUSED(port);
    INDI_UNUSED(enabled);
    INDI_UNUSED(voltage);
    return false;
}

bool PegasusPPB::SetLEDEnabled(bool enabled)
{
    char cmd[PEGASUS_LEN] = {0}, res[PEGASUS_LEN] = {0};
    snprintf(cmd, PEGASUS_LEN, "PL:%d", enabled ? 1 : 0);
    if (sendCommand(cmd, res))
    {
        return (!strcmp(res, cmd));
    }
    return false;
}

bool PegasusPPB::SetAutoDewEnabled(size_t port, bool enabled)
{
    INDI_UNUSED(port); // PPB only has global auto dew control
    return setAutoDewEnabled(enabled);
}

bool PegasusPPB::CyclePower()
{
    return reboot();
}

bool PegasusPPB::SetUSBPort(size_t port, bool enabled)
{
    INDI_UNUSED(port);
    INDI_UNUSED(enabled);
    return false;
}
