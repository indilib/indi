/*******************************************************************************
  Copyright(c) 2019 Jasem Mutlaq. All rights reserved.

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

PegasusPPB::PegasusPPB() : WI(this)
{
    setVersion(1, 1);
    lastSensorData.reserve(PA_N);
}

bool PegasusPPB::initProperties()
{
    INDI::DefaultDevice::initProperties();

    setDriverInterface(AUX_INTERFACE | WEATHER_INTERFACE);

    WI::initProperties(ENVIRONMENT_TAB, ENVIRONMENT_TAB);

    addAuxControls();

    ////////////////////////////////////////////////////////////////////////////
    /// Main Control Panel
    ////////////////////////////////////////////////////////////////////////////
    // Cycle all power on/off
    PowerCycleAllSP[POWER_CYCLE_OFF].fill("POWER_CYCLE_OFF", "All Off", ISS_OFF);
    PowerCycleAllSP[POWER_CYCLE_ON].fill("POWER_CYCLE_ON", "All On", ISS_OFF);
    PowerCycleAllSP.fill(getDeviceName(), "POWER_CYCLE", "Cycle Power", MAIN_CONTROL_TAB,
                       IP_RW, ISR_ATMOST1, 60, IPS_IDLE);

    // DSLR on/off
    DSLRPowerSP[INDI_ENABLED].fill("INDI_ENABLED", "On", ISS_OFF);
    DSLRPowerSP[INDI_DISABLED].fill("INDI_DISABLED", "Off", ISS_ON);
    DSLRPowerSP.fill(getDeviceName(), "DSLR_POWER", "DSLR Power", MAIN_CONTROL_TAB, IP_RW,
                       ISR_1OFMANY, 60, IPS_IDLE);

    // Reboot
    RebootSP[0].fill("REBOOT", "Reboot Device", ISS_OFF);
    RebootSP.fill(getDeviceName(), "REBOOT_DEVICE", "Device", MAIN_CONTROL_TAB, IP_RW, ISR_ATMOST1,
                       60, IPS_IDLE);

    // Power Sensors
    PowerSensorsNP[SENSOR_VOLTAGE].fill("SENSOR_VOLTAGE", "Voltage (V)", "%4.2f", 0, 999, 100, 0);
    PowerSensorsNP[SENSOR_CURRENT].fill("SENSOR_CURRENT", "Current (A)", "%4.2f", 0, 999, 100, 0);
    PowerSensorsNP.fill(getDeviceName(), "POWER_SENSORS", "Sensors", MAIN_CONTROL_TAB, IP_RO,
                       60, IPS_IDLE);

    ////////////////////////////////////////////////////////////////////////////
    /// Power Group
    ////////////////////////////////////////////////////////////////////////////

    // Power on Boot
    PowerOnBootSP[POWER_PORT_1].fill("POWER_PORT_1", "Port 1", ISS_ON);
    PowerOnBootSP[POWER_PORT_2].fill("POWER_PORT_2", "Port 2", ISS_ON);
    PowerOnBootSP[POWER_PORT_3].fill("POWER_PORT_3", "Port 3", ISS_ON);
    PowerOnBootSP[POWER_PORT_4].fill("POWER_PORT_4", "Port 4", ISS_ON);
    PowerOnBootSP.fill(getDeviceName(),"POWER_ON_BOOT", "Power On Boot", MAIN_CONTROL_TAB,
                       IP_RW, ISR_NOFMANY, 60, IPS_IDLE);

    ////////////////////////////////////////////////////////////////////////////
    /// Dew Group
    ////////////////////////////////////////////////////////////////////////////

    // Automatic Dew
    AutoDewSP[INDI_ENABLED].fill("INDI_ENABLED", "Enabled", ISS_OFF);
    AutoDewSP[INDI_DISABLED].fill("INDI_DISABLED", "Disabled", ISS_ON);
    AutoDewSP.fill(getDeviceName(), "AUTO_DEW", "Auto Dew", DEW_TAB, IP_RW, ISR_1OFMANY, 60,
                       IPS_IDLE);

    // Dew PWM
    DewPWMNP[DEW_PWM_A].fill("DEW_A", "Dew A (%)", "%.2f", 0, 100, 10, 0);
    DewPWMNP[DEW_PWM_B].fill("DEW_B", "Dew B (%)", "%.2f", 0, 100, 10, 0);
    DewPWMNP.fill(getDeviceName(), "DEW_PWM", "Dew PWM", DEW_TAB, IP_RW, 60, IPS_IDLE);

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
        defineProperty(PowerCycleAllSP);
        defineProperty(DSLRPowerSP);
        defineProperty(PowerSensorsNP);
        defineProperty(PowerOnBootSP);
        defineProperty(RebootSP);

        // Dew
        defineProperty(AutoDewSP);
        defineProperty(DewPWMNP);

        WI::updateProperties();

        setupComplete = true;
    }
    else
    {
        // Main Control
        deleteProperty(PowerCycleAllSP);
        deleteProperty(DSLRPowerSP);
        deleteProperty(PowerSensorsNP);
        deleteProperty(PowerOnBootSP);
        deleteProperty(RebootSP);

        // Dew
        deleteProperty(AutoDewSP);
        deleteProperty(DewPWMNP);

        WI::updateProperties();

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

    return !strcmp(response, "PPB_OK");
}

bool PegasusPPB::ISNewSwitch(const char * dev, const char * name, ISState * states, char * names[], int n)
{
    if (dev && !strcmp(dev, getDeviceName()))
    {
        // Cycle all power on or off
        if (PowerCycleAllSP.isNameMatch(name))
        {
            PowerCycleAllSP.update(states, names, n);

            PowerCycleAllSP.setState(IPS_ALERT);
            char cmd[PEGASUS_LEN] = {0}, res[PEGASUS_LEN] = {0};
            snprintf(cmd, PEGASUS_LEN, "P1:%d", PowerCycleAllSP.findOnSwitchIndex());
            if (sendCommand(cmd, res))
            {
                PowerCycleAllSP.setState(!strcmp(cmd, res) ? IPS_OK : IPS_ALERT);
            }

            PowerCycleAllSP.reset();
            PowerCycleAllSP.apply();
            return true;
        }

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

        // Auto Dew
        if (AutoDewSP.isNameMatch(name))
        {
            int prevIndex = AutoDewSP.findOnSwitchIndex();
            AutoDewSP.update(states, names, n);
            if (setAutoDewEnabled(AutoDewSP[INDI_ENABLED].getState() == ISS_ON))
            {
                AutoDewSP.setState(IPS_OK);
            }
            else
            {
                AutoDewSP.reset();
                AutoDewSP[prevIndex].setState(ISS_ON);
                AutoDewSP.setState(IPS_ALERT);
            }

            AutoDewSP.apply();
            return true;
        }
    }

    return DefaultDevice::ISNewSwitch(dev, name, states, names, n);
}

bool PegasusPPB::ISNewNumber(const char * dev, const char * name, double values[], char * names[], int n)
{
    if (dev && !strcmp(dev, getDeviceName()))
    {
        // Dew PWM
        if (DewPWMNP.isNameMatch(name))
        {
            bool rc1 = false, rc2 = false;
            for (int i = 0; i < n; i++)
            {
                if (!strcmp(names[i], DewPWMNP[DEW_PWM_A].getName()))
                    rc1 = setDewPWM(3, static_cast<uint8_t>(values[i] / 100.0 * 255.0));
                else if (!strcmp(names[i], DewPWMNP[DEW_PWM_B].getName()))
                    rc2 = setDewPWM(4, static_cast<uint8_t>(values[i] / 100.0 * 255.0));
            }

            DewPWMNP.setState((rc1 && rc2) ? IPS_OK : IPS_ALERT);
            if (DewPWMNP.getState() == IPS_OK)
                DewPWMNP.update(values, names, n);
            DewPWMNP.apply();
            return true;
        }

        if (strstr(name, "WEATHER_"))
            return WI::processNumber(dev, name, values, names, n);
    }
    return INDI::DefaultDevice::ISNewNumber(dev, name, values, names, n);
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

bool PegasusPPB::setPowerOnBoot()
{
    char cmd[PEGASUS_LEN] = {0}, res[PEGASUS_LEN] = {0};
    snprintf(cmd, PEGASUS_LEN, "PE:%d%d%d%d", PowerOnBootSP[POWER_PORT_1].getState() == ISS_ON ? 1 : 0,
             PowerOnBootSP[POWER_PORT_2].s == ISS_ON ? 1 : 0,
             PowerOnBootSP[POWER_PORT_3].s == ISS_ON ? 1 : 0,
             PowerOnBootSP[POWER_PORT_4].s == ISS_ON ? 1 : 0);
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
    AutoDewSP.save(fp);

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
            LOG_WARN("Received wrong number of detailed sensor data. Retrying...");
            return false;
        }

        if (result == lastSensorData)
            return true;

        // Power Sensors
        PowerSensorsNP[SENSOR_VOLTAGE].setValue(std::stod(result[PA_VOLTAGE]));
        PowerSensorsNP[SENSOR_CURRENT].setValue(std::stod(result[PA_CURRENT]) / 65.0);
        PowerSensorsNP.setState(IPS_OK);
        if (lastSensorData[PA_VOLTAGE] != result[PA_VOLTAGE] || lastSensorData[PA_CURRENT] != result[PA_CURRENT])
            PowerSensorsNP.apply();

        // Environment Sensors
        setParameterValue("WEATHER_TEMPERATURE", std::stod(result[PA_TEMPERATURE]));
        setParameterValue("WEATHER_HUMIDITY", std::stod(result[PA_HUMIDITY]));
        setParameterValue("WEATHER_DEWPOINT", std::stod(result[PA_DEW_POINT]));
        if (lastSensorData[PA_TEMPERATURE] != result[PA_TEMPERATURE] ||
                lastSensorData[PA_HUMIDITY] != result[PA_HUMIDITY] ||
                lastSensorData[PA_DEW_POINT] != result[PA_DEW_POINT])
        {
            if (WI::syncCriticalParameters())
                critialParametersLP.apply();
            ParametersNP.setState(IPS_OK);
            ParametersNP.apply();
        }

        // Power Status
        PowerCycleAllSP[POWER_CYCLE_ON].setState((std::stoi(result[PA_PORT_STATUS]) == 1) ? ISS_ON : ISS_OFF);
        PowerCycleAllSP[POWER_CYCLE_OFF].setState((std::stoi(result[PA_PORT_STATUS]) == 0) ? ISS_ON : ISS_OFF);
        PowerCycleAllSP.setState((std::stoi(result[6]) == 1) ? IPS_OK : IPS_IDLE);
        if (lastSensorData[PA_PORT_STATUS] != result[PA_PORT_STATUS])
            PowerCycleAllSP.apply();

        // DSLR Power Status
        DSLRPowerSP[INDI_ENABLED].setState((std::stoi(result[PA_DSLR_STATUS]) == 1) ? ISS_ON : ISS_OFF);
        DSLRPowerSP[INDI_DISABLED].setState((std::stoi(result[PA_DSLR_STATUS]) == 0) ? ISS_ON : ISS_OFF);
        DSLRPowerSP.setState((std::stoi(result[PA_DSLR_STATUS]) == 1) ? IPS_OK : IPS_IDLE);
        if (lastSensorData[PA_DSLR_STATUS] != result[PA_DSLR_STATUS])
            DSLRPowerSP.apply();

        // Dew PWM
        DewPWMNP[DEW_PWM_A].setValue(std::stod(result[PA_DEW_1]) / 255.0 * 100.0);
        DewPWMNP[DEW_PWM_B].setValue(std::stod(result[PA_DEW_2]) / 255.0 * 100.0);
        if (lastSensorData[PA_DEW_1] != result[PA_DEW_1] || lastSensorData[PA_DEW_2] != result[PA_DEW_2])
            DewPWMNP.apply();

        // Auto Dew
        AutoDewSP[INDI_ENABLED].setState((std::stoi(result[PA_AUTO_DEW]) == 1) ? ISS_ON : ISS_OFF);
        AutoDewSP[INDI_DISABLED].s = (std::stoi(result[PA_AUTO_DEW]) == 1) ? ISS_OFF : ISS_ON;
        if (lastSensorData[PA_AUTO_DEW] != result[PA_AUTO_DEW])
            AutoDewSP.apply();

        lastSensorData = result;

        return true;
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

