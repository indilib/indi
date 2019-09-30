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

void ISGetProperties(const char * dev)
{
    pocket_power_box->ISGetProperties(dev);
}

void ISNewSwitch(const char * dev, const char * name, ISState * states, char * names[], int n)
{
    pocket_power_box->ISNewSwitch(dev, name, states, names, n);
}

void ISNewText(const char * dev, const char * name, char * texts[], char * names[], int n)
{
    pocket_power_box->ISNewText(dev, name, texts, names, n);
}

void ISNewNumber(const char * dev, const char * name, double values[], char * names[], int n)
{
    pocket_power_box->ISNewNumber(dev, name, values, names, n);
}

void ISNewBLOB(const char * dev, const char * name, int sizes[], int blobsizes[], char * blobs[], char * formats[],
               char * names[], int n)
{
    INDI_UNUSED(dev);
    INDI_UNUSED(name);
    INDI_UNUSED(sizes);
    INDI_UNUSED(blobsizes);
    INDI_UNUSED(blobs);
    INDI_UNUSED(formats);
    INDI_UNUSED(names);
    INDI_UNUSED(n);
}

void ISSnoopDevice(XMLEle * root)
{
    pocket_power_box->ISSnoopDevice(root);
}

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
    IUFillSwitch(&PowerCycleAllS[POWER_CYCLE_OFF], "POWER_CYCLE_OFF", "All Off", ISS_OFF);
    IUFillSwitch(&PowerCycleAllS[POWER_CYCLE_ON], "POWER_CYCLE_ON", "All On", ISS_OFF);
    IUFillSwitchVector(&PowerCycleAllSP, PowerCycleAllS, 2, getDeviceName(), "POWER_CYCLE", "Cycle Power", MAIN_CONTROL_TAB, IP_RW, ISR_ATMOST1, 60, IPS_IDLE);

    // DSLR on/off
    IUFillSwitch(&DSLRPowerS[DSLR_OFF], "DSLR_OFF", "Off", ISS_OFF);
    IUFillSwitch(&DSLRPowerS[DSLR_ON], "DSLR_ON", "On", ISS_OFF);
    IUFillSwitchVector(&DSLRPowerSP, DSLRPowerS, 2, getDeviceName(), "DSLR_POWER", "DSLR Power", MAIN_CONTROL_TAB, IP_RW, ISR_ATMOST1, 60, IPS_IDLE);

    // Reboot
    IUFillSwitch(&RebootS[0], "REBOOT", "Reboot Device", ISS_OFF);
    IUFillSwitchVector(&RebootSP, RebootS, 1, getDeviceName(), "REBOOT_DEVICE", "Device", MAIN_CONTROL_TAB, IP_RW, ISR_ATMOST1, 60, IPS_IDLE);

    // Power Sensors
    IUFillNumber(&PowerSensorsN[SENSOR_VOLTAGE], "SENSOR_VOLTAGE", "Voltage (V)", "%4.2f", 0, 999, 100, 0);
    IUFillNumber(&PowerSensorsN[SENSOR_CURRENT], "SENSOR_CURRENT", "Current (A)", "%4.2f", 0, 999, 100, 0);
    IUFillNumberVector(&PowerSensorsNP, PowerSensorsN, 2, getDeviceName(), "POWER_SENSORS", "Sensors", MAIN_CONTROL_TAB, IP_RO, 60, IPS_IDLE);

    ////////////////////////////////////////////////////////////////////////////
    /// Power Group
    ////////////////////////////////////////////////////////////////////////////

    // Power on Boot
    IUFillSwitch(&PowerOnBootS[0], "POWER_PORT_1", "Port 1", ISS_ON);
    IUFillSwitch(&PowerOnBootS[1], "POWER_PORT_2", "Port 2", ISS_ON);
    IUFillSwitch(&PowerOnBootS[2], "POWER_PORT_3", "Port 3", ISS_ON);
    IUFillSwitch(&PowerOnBootS[3], "POWER_PORT_4", "Port 4", ISS_ON);
    IUFillSwitchVector(&PowerOnBootSP, PowerOnBootS, 4, getDeviceName(), "POWER_ON_BOOT", "Power On Boot", MAIN_CONTROL_TAB, IP_RW, ISR_NOFMANY, 60, IPS_IDLE);

    ////////////////////////////////////////////////////////////////////////////
    /// Dew Group
    ////////////////////////////////////////////////////////////////////////////

    // Automatic Dew
    IUFillSwitch(&AutoDewS[AUTO_DEW_ENABLED], "AUTO_DEW_ENABLED", "Enabled", ISS_OFF);
    IUFillSwitch(&AutoDewS[AUTO_DEW_DISABLED], "AUTO_DEW_DISABLED", "Disabled", ISS_ON);
    IUFillSwitchVector(&AutoDewSP, AutoDewS, 2, getDeviceName(), "AUTO_DEW", "Auto Dew", DEW_TAB, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);

    // Dew PWM
    IUFillNumber(&DewPWMN[DEW_PWM_A], "DEW_A", "Dew A (%)", "%.2f", 0, 100, 10, 0);
    IUFillNumber(&DewPWMN[DEW_PWM_B], "DEW_B", "Dew B (%)", "%.2f", 0, 100, 10, 0);
    IUFillNumberVector(&DewPWMNP, DewPWMN, 2, getDeviceName(), "DEW_PWM", "Dew PWM", DEW_TAB, IP_RW, 60, IPS_IDLE);

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
        defineSwitch(&PowerCycleAllSP);
        defineSwitch(&DSLRPowerSP);
        defineNumber(&PowerSensorsNP);
        defineSwitch(&PowerOnBootSP);
        defineSwitch(&RebootSP);

        // Dew
        defineSwitch(&AutoDewSP);
        defineNumber(&DewPWMNP);

        WI::updateProperties();

        setupComplete = true;
    }
    else
    {
        // Main Control
        deleteProperty(PowerCycleAllSP.name);
        deleteProperty(DSLRPowerSP.name);
        deleteProperty(PowerSensorsNP.name);
        deleteProperty(PowerOnBootSP.name);
        deleteProperty(RebootSP.name);

        // Dew
        deleteProperty(AutoDewSP.name);
        deleteProperty(DewPWMNP.name);

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

    return (!strcmp(response, "PPB_OK"));
}

bool PegasusPPB::ISNewSwitch(const char * dev, const char * name, ISState * states, char * names[], int n)
{
    if (dev && !strcmp(dev, getDeviceName()))
    {
        // Cycle all power on or off
        if (!strcmp(name, PowerCycleAllSP.name))
        {
            IUUpdateSwitch(&PowerCycleAllSP, states, names, n);

            PowerCycleAllSP.s = IPS_ALERT;
            char cmd[PEGASUS_LEN] = {0}, res[PEGASUS_LEN] = {0};
            snprintf(cmd, PEGASUS_LEN, "P1:%d", IUFindOnSwitchIndex(&PowerCycleAllSP));
            if (sendCommand(cmd, res))
            {
                PowerCycleAllSP.s = !strcmp(cmd, res) ? IPS_OK : IPS_ALERT;
            }

            IUResetSwitch(&PowerCycleAllSP);
            IDSetSwitch(&PowerCycleAllSP, nullptr);
            return true;
        }

        // DSLR
        if (!strcmp(name, DSLRPowerSP.name))
        {
            IUUpdateSwitch(&DSLRPowerSP, states, names, n);

            DSLRPowerSP.s = IPS_ALERT;
            char cmd[PEGASUS_LEN] = {0}, res[PEGASUS_LEN] = {0};
            snprintf(cmd, PEGASUS_LEN, "P2:%d", IUFindOnSwitchIndex(&DSLRPowerSP));
            if (sendCommand(cmd, res))
            {
                DSLRPowerSP.s = !strcmp(cmd, res) ? IPS_OK : IPS_ALERT;
            }

            IUResetSwitch(&DSLRPowerSP);
            IDSetSwitch(&DSLRPowerSP, nullptr);
            return true;
        }

        // Reboot
        if (!strcmp(name, RebootSP.name))
        {
            RebootSP.s = reboot() ? IPS_OK : IPS_ALERT;
            IDSetSwitch(&RebootSP, nullptr);
            LOG_INFO("Rebooting device...");
            return true;
        }

        // Power on boot
        if (!strcmp(name, PowerOnBootSP.name))
        {
            IUUpdateSwitch(&PowerOnBootSP, states, names, n);
            PowerOnBootSP.s = setPowerOnBoot() ? IPS_OK : IPS_ALERT;
            IDSetSwitch(&PowerOnBootSP, nullptr);
            saveConfig(true, PowerOnBootSP.name);
            return true;
        }

        // Auto Dew
        if (!strcmp(name, AutoDewSP.name))
        {
            int prevIndex = IUFindOnSwitchIndex(&AutoDewSP);
            IUUpdateSwitch(&AutoDewSP, states, names, n);
            if (setAutoDewEnabled(AutoDewS[AUTO_DEW_ENABLED].s == ISS_ON))
            {
                AutoDewSP.s = IPS_OK;
            }
            else
            {
                IUResetSwitch(&AutoDewSP);
                AutoDewS[prevIndex].s = ISS_ON;
                AutoDewSP.s = IPS_ALERT;
            }

            IDSetSwitch(&AutoDewSP, nullptr);
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
        if (!strcmp(name, DewPWMNP.name))
        {
            bool rc1 = false, rc2 = false;
            for (int i = 0; i < n; i++)
            {
                if (!strcmp(names[i], DewPWMN[DEW_PWM_A].name))
                    rc1 = setDewPWM(3, static_cast<uint8_t>(values[i] / 100.0 * 255.0));
                else if (!strcmp(names[i], DewPWMN[DEW_PWM_B].name))
                    rc2 = setDewPWM(4, static_cast<uint8_t>(values[i] / 100.0 * 255.0));
            }

            DewPWMNP.s = (rc1 && rc2) ? IPS_OK : IPS_ALERT;
            if (DewPWMNP.s == IPS_OK)
                IUUpdateNumber(&DewPWMNP, values, names, n);
            IDSetNumber(&DewPWMNP, nullptr);
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

        if ( (tty_rc = tty_nread_section(PortFD, res, PEGASUS_LEN, stopChar, PEGASUS_TIMEOUT, &nbytes_read)) != TTY_OK || nbytes_read == 1)
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
    snprintf(cmd, PEGASUS_LEN, "PE:%d%d%d%d", PowerOnBootS[0].s == ISS_ON ? 1 : 0,
             PowerOnBootS[1].s == ISS_ON ? 1 : 0,
             PowerOnBootS[2].s == ISS_ON ? 1 : 0,
             PowerOnBootS[3].s == ISS_ON ? 1 : 0);
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
    // Save CCD Config
    INDI::DefaultDevice::saveConfigItems(fp);
    IUSaveConfigSwitch(fp, &AutoDewSP);

    return true;
}

void PegasusPPB::TimerHit()
{
    if (!isConnected() || setupComplete == false)
    {
        SetTimer(POLLMS);
        return;
    }

    getSensorData();
    SetTimer(POLLMS);
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
        if (result.size() != PA_N)
        {
            LOG_WARN("Received wrong number of detailed sensor data. Retrying...");
            return false;
        }

        if (result == lastSensorData)
            return true;

        // Power Sensors
        PowerSensorsN[SENSOR_VOLTAGE].value = std::stod(result[PA_VOLTAGE]);
        PowerSensorsN[SENSOR_CURRENT].value = std::stod(result[PA_CURRENT]) / 65.0;
        PowerSensorsNP.s = IPS_OK;
        if (lastSensorData[PA_VOLTAGE] != result[PA_VOLTAGE] || lastSensorData[PA_CURRENT] != result[PA_CURRENT])
            IDSetNumber(&PowerSensorsNP, nullptr);

        // Environment Sensors
        setParameterValue("WEATHER_TEMPERATURE", std::stod(result[PA_TEMPERATURE]));
        setParameterValue("WEATHER_HUMIDITY", std::stod(result[PA_HUMIDITY]));
        setParameterValue("WEATHER_DEWPOINT", std::stod(result[PA_DEW_POINT]));
        if (lastSensorData[PA_TEMPERATURE] != result[PA_TEMPERATURE] ||
                lastSensorData[PA_HUMIDITY] != result[PA_HUMIDITY] ||
                lastSensorData[PA_DEW_POINT] != result[PA_DEW_POINT])
        {
            WI::syncCriticalParameters();
            ParametersNP.s = IPS_OK;
            IDSetNumber(&ParametersNP, nullptr);
        }

        // Power Status
        PowerCycleAllS[POWER_CYCLE_ON].s = (std::stoi(result[PA_PORT_STATUS]) == 1) ? ISS_ON : ISS_OFF;
        PowerCycleAllS[POWER_CYCLE_ON].s = (std::stoi(result[PA_PORT_STATUS]) == 0) ? ISS_ON : ISS_OFF;
        PowerCycleAllSP.s = (std::stoi(result[6]) == 1) ? IPS_OK : IPS_IDLE;
        if (lastSensorData[PA_PORT_STATUS] != result[PA_PORT_STATUS])
            IDSetSwitch(&PowerCycleAllSP, nullptr);

        // DSLR Power Status
        DSLRPowerS[POWER_CYCLE_ON].s = (std::stoi(result[PA_DSLR_STATUS]) == 1) ? ISS_ON : ISS_OFF;
        DSLRPowerS[POWER_CYCLE_ON].s = (std::stoi(result[PA_DSLR_STATUS]) == 0) ? ISS_ON : ISS_OFF;
        DSLRPowerSP.s = (std::stoi(result[PA_DSLR_STATUS]) == 1) ? IPS_OK : IPS_IDLE;
        if (lastSensorData[PA_DSLR_STATUS] != result[PA_DSLR_STATUS])
            IDSetSwitch(&DSLRPowerSP, nullptr);

        // Dew PWM
        DewPWMN[0].value = std::stod(result[PA_DEW_1]) / 255.0 * 100.0;
        DewPWMN[1].value = std::stod(result[PA_DEW_2]) / 255.0 * 100.0;
        if (lastSensorData[PA_DEW_1] != result[PA_DEW_1] || lastSensorData[PA_DEW_2] != result[PA_DEW_2])
            IDSetNumber(&DewPWMNP, nullptr);

        // Auto Dew
        AutoDewS[AUTO_DEW_ENABLED].s  = (std::stoi(result[PA_AUTO_DEW]) == 1) ? ISS_ON : ISS_OFF;
        AutoDewS[AUTO_DEW_DISABLED].s = (std::stoi(result[PA_AUTO_DEW]) == 1) ? ISS_OFF : ISS_ON;
        if (lastSensorData[PA_AUTO_DEW] != result[PA_AUTO_DEW])
            IDSetSwitch(&AutoDewSP, nullptr);

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

