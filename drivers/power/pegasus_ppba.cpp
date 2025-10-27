/*******************************************************************************
  Copyright(c) 2019 Jasem Mutlaq. All rights reserved.

  Pegasus Pocket Power Box Advance Driver.

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

#include "pegasus_ppba.h"
#include "indicom.h"
#include "connectionplugins/connectionserial.h"

#include <regex>
#include <termios.h>
#include <chrono>
#include <iomanip>

// We declare an auto pointer to PegasusPPBA.
static std::unique_ptr<PegasusPPBA> ppba(new PegasusPPBA());

PegasusPPBA::PegasusPPBA() : INDI::DefaultDevice(), FI(this), WI(this), INDI::PowerInterface(this)
{
    setVersion(1, 3);
    lastSensorData.reserve(PA_N);
    lastConsumptionData.reserve(PS_N);
    lastMetricsData.reserve(PC_N);
}

bool PegasusPPBA::initProperties()
{
    INDI::DefaultDevice::initProperties();

    setDriverInterface(AUX_INTERFACE | WEATHER_INTERFACE | POWER_INTERFACE);

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

    PI::SetCapability(POWER_HAS_DC_OUT | POWER_HAS_DEW_OUT | POWER_HAS_VARIABLE_OUT |
                      POWER_HAS_VOLTAGE_SENSOR | POWER_HAS_OVERALL_CURRENT | POWER_HAS_PER_PORT_CURRENT |
                      POWER_HAS_LED_TOGGLE | POWER_HAS_AUTO_DEW);
    // Power Interface properties
    // 1 DC output, 2 DEW outputs, 1 Variable output, 1 Auto Dew ports (Global), 0 USB ports
    PI::initProperties(POWER_TAB, 1, 2, 1, 1, 0);

    // Power on Boot
    PowerOnBootSP[POWER_PORT_1].fill("POWER_PORT_1", "Quad Out", ISS_ON);
    PowerOnBootSP[POWER_PORT_2].fill("POWER_PORT_2", "Adj Out", ISS_ON);
    PowerOnBootSP[POWER_PORT_3].fill("POWER_PORT_3", "Dew A", ISS_ON);
    PowerOnBootSP[POWER_PORT_4].fill("POWER_PORT_4", "Dew B", ISS_ON);
    PowerOnBootSP.fill(getDeviceName(), "POWER_ON_BOOT", "Power On Boot", MAIN_CONTROL_TAB,
                       IP_RW, ISR_NOFMANY, 60, IPS_IDLE);

    // Automatic Dew Settings
    AutoDewSettingsNP[AUTO_DEW_AGGRESSION].fill("AGGRESSION", "Aggresiveness (%)", "%.2f", 0, 100, 10, 0);
    AutoDewSettingsNP.fill(getDeviceName(), "AUTO_DEW_SETTINGS", "Auto Dew Settings", DEW_TAB, IP_RW, 60, IPS_IDLE);

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
    /// Focuser Group
    ////////////////////////////////////////////////////////////////////////////

    // Max Speed
    FocuserSettingsNP[SETTING_MAX_SPEED].fill("SETTING_MAX_SPEED", "Max Speed (%)", "%.f", 0, 900, 100, 400);
    FocuserSettingsNP.fill(getDeviceName(), "FOCUSER_SETTINGS", "Settings", FOCUS_TAB,
                           IP_RW, 60, IPS_IDLE);

    // Stepping
    FocuserDriveSP[STEP_FULL].fill("STEP_FULL", "Full", ISS_OFF);
    FocuserDriveSP[STEP_HALF].fill("STEP_HALF", "Half", ISS_ON);
    FocuserDriveSP[STEP_FORTH].fill("STEP_FORTH", "1/4", ISS_OFF);
    FocuserDriveSP[STEP_EIGHTH].fill("STEP_EIGHTH", "1/8", ISS_OFF);
    FocuserDriveSP.fill(getDeviceName(), "FOCUSER_DRIVE", "Microstepping", FOCUS_TAB,
                        IP_RW, ISR_1OFMANY, 60, IPS_IDLE);

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

bool PegasusPPBA::updateProperties()
{
    INDI::DefaultDevice::updateProperties();

    if (isConnected())
    {
        m_HasExternalMotor = findExternalMotorController();

        if (m_HasExternalMotor)
        {
            getXMCStartupData();
            setDriverInterface(getDriverInterface() | FOCUSER_INTERFACE);
            syncDriverInfo();
        }

        // Main Control
        defineProperty(RebootSP);
        defineProperty(PowerWarnLP); // This is a custom property, not part of INDI::PowerInterface
        defineProperty(PowerOnBootSP); // Re-add PowerOnBootSP

        defineProperty(AutoDewSettingsNP);
        getAutoDewAggression();

        // Power Interface properties
        PI::updateProperties();

        // Focuser
        if (m_HasExternalMotor)
        {
            FI::updateProperties();
            defineProperty(FocuserSettingsNP);
            defineProperty(FocuserDriveSP);
        }

        WI::updateProperties();

        // Firmware
        defineProperty(FirmwareTP);
        sendFirmware();

        setupComplete = true;
    }
    else
    {
        // Main Control
        deleteProperty(RebootSP);
        deleteProperty(PowerWarnLP);
        deleteProperty(PowerOnBootSP);

        deleteProperty(AutoDewSettingsNP);

        // Power Interface properties
        PI::updateProperties();

        if (m_HasExternalMotor)
        {
            FI::updateProperties();
            deleteProperty(FocuserSettingsNP);
            deleteProperty(FocuserDriveSP);
        }

        WI::updateProperties();

        deleteProperty(FirmwareTP);

        setupComplete = false;
    }

    return true;
}

const char * PegasusPPBA::getDefaultName()
{
    return "Pegasus PPBA";
}

bool PegasusPPBA::Handshake()
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
            tty_rc = tty_nread_section(PortFD, response, PEGASUS_LEN, 0xA, 1, &nbytes_read);
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

    return (!strcmp(response, "PPBA_OK") || !strcmp(response, "PPBM_OK"));
}

bool PegasusPPBA::ISNewSwitch(const char * dev, const char * name, ISState * states, char * names[], int n)
{
    if (dev && !strcmp(dev, getDeviceName()))
    {
        // Process power-related switches via PowerInterface
        if (PI::processSwitch(dev, name, states, names, n))
            return true;

        // Reboot
        if (RebootSP.isNameMatch(name))
        {
            RebootSP.setState(reboot() ? IPS_OK : IPS_ALERT);
            RebootSP.apply();
            LOG_INFO("Rebooting device...");
            return true;
        }

        // Microstepping
        if (FocuserDriveSP.isNameMatch(name))
        {
            int prevIndex = FocuserDriveSP.findOnSwitchIndex();
            FocuserDriveSP.update(states, names, n);
            if (setFocuserMicrosteps(FocuserDriveSP.findOnSwitchIndex() + 1))
            {
                FocuserDriveSP.setState(IPS_OK);
            }
            else
            {
                FocuserDriveSP.reset();
                FocuserDriveSP[prevIndex].setState(ISS_ON);
                FocuserDriveSP.setState(IPS_ALERT);
            }

            FocuserDriveSP.apply();
            return true;
        }

        if (strstr(name, "FOCUS"))
            return FI::processSwitch(dev, name, states, names, n);
    }

    return DefaultDevice::ISNewSwitch(dev, name, states, names, n);
}

bool PegasusPPBA::ISNewNumber(const char * dev, const char * name, double values[], char * names[], int n)
{
    if (dev && !strcmp(dev, getDeviceName()))
    {
        // Process power-related numbers via PowerInterface
        if (PI::processNumber(dev, name, values, names, n))
            return true;

        // Auto Dew Aggressiveness
        if (AutoDewSettingsNP.isNameMatch(name))
        {
            // Convert percentage (0-100) to device range (0-255)
            uint8_t aggression = static_cast<uint8_t>(values[0] / 100.0 * 255.0);
            if (setAutoDewAggression(aggression))
            {
                AutoDewSettingsNP.update(values, names, n);
                AutoDewSettingsNP.setState(IPS_OK);
            }
            else
            {
                AutoDewSettingsNP.setState(IPS_ALERT);
            }
            AutoDewSettingsNP.apply();
            return true;
        }

        // Focuser Settings
        if (FocuserSettingsNP.isNameMatch(name))
        {
            if (setFocuserMaxSpeed(values[0]))
            {
                FocuserSettingsNP[SETTING_MAX_SPEED].setValue(values[0]);
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
    }
    return INDI::DefaultDevice::ISNewNumber(dev, name, values, names, n);
}

bool PegasusPPBA::ISNewText(const char * dev, const char * name, char * texts[], char * names[], int n)
{
    if (dev && !strcmp(dev, getDeviceName()))
    {
        // Process power-related text via PowerInterface
        if (PI::processText(dev, name, texts, names, n))
            return true;
    }

    return INDI::DefaultDevice::ISNewText(dev, name, texts, names, n);
}

bool PegasusPPBA::sendCommand(const char * cmd, char * res)
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

bool PegasusPPBA::findExternalMotorController()
{
    char res[PEGASUS_LEN] = {0};
    if (!sendCommand("XS", res))
        return false;

    // 200 XMC present
    return strstr(res, "200");
}

bool PegasusPPBA::setAutoDewEnabled(bool enabled)
{
    char cmd[PEGASUS_LEN] = {0}, res[PEGASUS_LEN] = {0};
    snprintf(cmd, PEGASUS_LEN, "PD:%d", enabled ? 1 : 0);
    if (sendCommand(cmd, res))
    {
        return (!strcmp(res, cmd));
    }

    return false;
}

bool PegasusPPBA::setAutoDewAggression(uint8_t value)
{
    char cmd[PEGASUS_LEN] = {0}, res[PEGASUS_LEN] = {0};
    snprintf(cmd, PEGASUS_LEN, "PD:%d", value);
    if (sendCommand(cmd, res))
    {
        return (!strcmp(res, cmd));
    }

    return false;
}

bool PegasusPPBA::setPowerEnabled(uint8_t port, bool enabled)
{
    char cmd[PEGASUS_LEN] = {0}, res[PEGASUS_LEN] = {0};
    snprintf(cmd, PEGASUS_LEN, "P%d:%d", port, enabled ? 1 : 0);
    if (sendCommand(cmd, res))
    {
        return (!strcmp(res, cmd));
    }

    return false;
}

bool PegasusPPBA::setAdjustableOutput(uint8_t voltage)
{
    char cmd[PEGASUS_LEN] = {0}, res[PEGASUS_LEN] = {0};
    snprintf(cmd, PEGASUS_LEN, "P2:%d", voltage);
    if (sendCommand(cmd, res))
    {
        return (!strcmp(res, cmd));
    }

    return false;
}

bool PegasusPPBA::setLedIndicator(bool enabled)
{
    char cmd[PEGASUS_LEN] = {0}, res[PEGASUS_LEN] = {0};
    snprintf(cmd, PEGASUS_LEN, "PL:%d", enabled ? 1 : 0);
    if (sendCommand(cmd, res))
    {
        return (!strcmp(res, cmd));
    }

    return false;
}

bool PegasusPPBA::setPowerOnBoot()
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

bool PegasusPPBA::setDewPWM(uint8_t id, uint8_t value)
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

bool PegasusPPBA::saveConfigItems(FILE * fp)
{
    INDI::DefaultDevice::saveConfigItems(fp);
    if (m_HasExternalMotor)
    {
        FI::saveConfigItems(fp);
        FocuserSettingsNP.save(fp);
        FocuserDriveSP.save(fp);
    }
    WI::saveConfigItems(fp);
    PI::saveConfigItems(fp); // Save power properties
    AutoDewSettingsNP.save(fp); // This is a custom property, not part of INDI::PowerInterface
    return true;
}

void PegasusPPBA::TimerHit()
{
    if (!isConnected() || setupComplete == false)
    {
        SetTimer(getCurrentPollingPeriod());
        return;
    }

    getSensorData();
    getConsumptionData();
    getMetricsData();

    if (m_HasExternalMotor)
        queryXMC();

    SetTimer(getCurrentPollingPeriod());
}

bool PegasusPPBA::sendFirmware()
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

bool PegasusPPBA::getSensorData()
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
        PI::PowerSensorsNP[PI::SENSOR_VOLTAGE].setValue(std::stod(result[PA_VOLTAGE]));
        PI::PowerSensorsNP[PI::SENSOR_CURRENT].setValue(std::stod(result[PA_CURRENT]) / 65.0);
        // PPBA does not report total power directly, so we calculate it
        PI::PowerSensorsNP[PI::SENSOR_POWER].setValue(PI::PowerSensorsNP[PI::SENSOR_VOLTAGE].getValue() *
                PI::PowerSensorsNP[PI::SENSOR_CURRENT].getValue());
        PI::PowerSensorsNP.setState(IPS_OK);
        if (lastSensorData.size() < PA_N ||
                lastSensorData[PA_VOLTAGE] != result[PA_VOLTAGE] || lastSensorData[PA_CURRENT] != result[PA_CURRENT])
            PI::PowerSensorsNP.apply();

        // Environment Sensors
        setParameterValue("WEATHER_TEMPERATURE", std::stod(result[PA_TEMPERATURE]));
        setParameterValue("WEATHER_HUMIDITY", std::stod(result[PA_HUMIDITY]));
        setParameterValue("WEATHER_DEWPOINT", std::stod(result[PA_DEW_POINT]));
        if (lastSensorData.size() < PA_N ||
                lastSensorData[PA_TEMPERATURE] != result[PA_TEMPERATURE] ||
                lastSensorData[PA_HUMIDITY] != result[PA_HUMIDITY] ||
                lastSensorData[PA_DEW_POINT] != result[PA_DEW_POINT])
        {
            if (WI::syncCriticalParameters())
                critialParametersLP.apply();
            ParametersNP.setState(IPS_OK);
            ParametersNP.apply();
        }

        // Power Status (DC Output)
        if (PI::PowerChannelsSP.size() > 0)
        {
            PI::PowerChannelsSP[0].setState((std::stoi(result[PA_PORT_STATUS]) == 1) ? ISS_ON : ISS_OFF);
            if (lastSensorData.size() < PA_N || lastSensorData[PA_PORT_STATUS] != result[PA_PORT_STATUS])
                PI::PowerChannelsSP.apply();
        }

        // Adjustable Power Status
        //        AdjOutS[INDI_ENABLED].s = (std::stoi(result[PA_ADJ_STATUS]) == 1) ? ISS_ON : ISS_OFF;
        //        AdjOutS[INDI_DISABLED].s = (std::stoi(result[PA_ADJ_STATUS]) == 1) ? ISS_OFF : ISS_ON;
        //        AdjOutSP.s = (std::stoi(result[PA_ADJ_STATUS]) == 1) ? IPS_OK : IPS_IDLE;
        //        if (lastSensorData[PA_ADJ_STATUS] != result[PA_ADJ_STATUS])
        //            IDSetSwitch(&AdjOutSP, nullptr);

        // Adjustable Power Status (Variable Output)
        if (PI::VariableChannelsSP.size() > 0)
        {
            PI::VariableChannelsSP[0].setState((std::stoi(result[PA_ADJ_STATUS]) == 1) ? ISS_ON : ISS_OFF);
            if (PI::VariableChannelVoltsNP.size() > 0)
                PI::VariableChannelVoltsNP[0].setValue(std::stod(result[PA_PWRADJ]));
            if (lastSensorData.size() < PA_N ||
                    lastSensorData[PA_PWRADJ] != result[PA_PWRADJ] || lastSensorData[PA_ADJ_STATUS] != result[PA_ADJ_STATUS])
            {
                PI::VariableChannelsSP.apply();
                PI::VariableChannelVoltsNP.apply();
            }
        }

        // Power Warn (This is a custom property, not part of INDI::PowerInterface)
        PowerWarnLP[0].setState((std::stoi(result[PA_PWR_WARN]) == 1) ? IPS_ALERT : IPS_OK);
        PowerWarnLP.setState((std::stoi(result[PA_PWR_WARN]) == 1) ? IPS_ALERT : IPS_OK);
        if (lastSensorData.size() < PA_N ||
                lastSensorData[PA_PWR_WARN] != result[PA_PWR_WARN])
            PowerWarnLP.apply();

        // Dew PWM (Dew Outputs)
        if (PI::DewChannelDutyCycleNP.size() > 0)
            PI::DewChannelDutyCycleNP[0].setValue(std::stod(result[PA_DEW_1]) / 255.0 * 100.0);
        if (PI::DewChannelDutyCycleNP.size() > 1)
            PI::DewChannelDutyCycleNP[1].setValue(std::stod(result[PA_DEW_2]) / 255.0 * 100.0);
        if (lastSensorData.size() < PA_N ||
                lastSensorData[PA_DEW_1] != result[PA_DEW_1] || lastSensorData[PA_DEW_2] != result[PA_DEW_2])
            PI::DewChannelDutyCycleNP.apply();

        // Update Dew Channel switches based on actual power status
        // If Auto Dew is enabled, it may turn channels on/off, so we need to reflect that
        bool dewChannelChanged = false;
        if (PI::DewChannelsSP.size() > 0)
        {
            auto newState = (std::stoi(result[PA_DEW_1]) > 0) ? ISS_ON : ISS_OFF;
            if (PI::DewChannelsSP[0].getState() != newState)
            {
                PI::DewChannelsSP[0].setState(newState);
                dewChannelChanged = true;
            }
        }
        if (PI::DewChannelsSP.size() > 1)
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

        // Auto Dew
        if (PI::AutoDewSP.size() > 0)
        {
            PI::AutoDewSP[0].setState((std::stoi(result[PA_AUTO_DEW]) == 1) ? ISS_ON : ISS_OFF);
            if (lastSensorData.size() < PA_N || lastSensorData[PA_AUTO_DEW] != result[PA_AUTO_DEW])
                PI::AutoDewSP.apply();
        }

        lastSensorData = result;

        return true;
    }

    return false;
}

bool PegasusPPBA::getConsumptionData()
{
    char res[PEGASUS_LEN] = {0};
    if (sendCommand("PS", res))
    {
        std::vector<std::string> result = split(res, ":");
        if (result.size() < PS_N)
        {
            LOG_WARN("Received wrong number of detailed consumption data. Retrying...");
            return false;
        }

        if (result == lastConsumptionData)
            return true;

        // Power Consumption Statistics are not directly mapped to INDI::PowerInterface properties.
        // The overall power (PI::SENSOR_POWER) is already calculated in getSensorData().
        // We will just update the lastConsumptionData for change detection.
        INDI_UNUSED(result);
        INDI_UNUSED(lastConsumptionData);

        lastConsumptionData = result;

        return true;
    }

    return false;
}

bool PegasusPPBA::getAutoDewAggression()
{
    char res[PEGASUS_LEN] = {0};
    if (sendCommand("DA", res))
    {

        uint32_t value = 0;
        sscanf(res, "%*[^:]:%d", &value);
        AutoDewSettingsNP[AUTO_DEW_AGGRESSION].setValue(100 * value / 255);
    }
    else
        AutoDewSettingsNP.setState(IPS_ALERT);

    AutoDewSettingsNP.apply();
    return AutoDewSettingsNP.getState() != IPS_ALERT;
}

bool PegasusPPBA::getMetricsData()
{
    char res[PEGASUS_LEN] = {0};
    if (sendCommand("PC", res))
    {
        std::vector<std::string> result = split(res, ":");
        if (result.size() < PC_N)
        {
            LOG_WARN("Received wrong number of detailed metrics data. Retrying...");
            return false;
        }

        if (result == lastMetricsData)
            return true;

        // Power Sensors (Per-port current monitoring)
        if (PI::PowerChannelCurrentNP.size() > 0)
            PI::PowerChannelCurrentNP[0].setValue(std::stod(result[PC_12V_CURRENT]));
        if (PI::DewChannelCurrentNP.size() > 0)
            PI::DewChannelCurrentNP[0].setValue(std::stod(result[PC_DEWA_CURRENT]));
        if (PI::DewChannelCurrentNP.size() > 1)
            PI::DewChannelCurrentNP[1].setValue(std::stod(result[PC_DEWB_CURRENT]));
        if (lastMetricsData.size() < PC_N ||
                lastMetricsData[PC_TOTAL_CURRENT] != result[PC_TOTAL_CURRENT]
                || // Total current is not directly mapped to PI properties, but we keep it for change detection
                lastMetricsData[PC_12V_CURRENT] != result[PC_12V_CURRENT] ||
                lastMetricsData[PC_DEWA_CURRENT] != result[PC_DEWA_CURRENT] ||
                lastMetricsData[PC_DEWB_CURRENT] != result[PC_DEWB_CURRENT])
        {
            PI::PowerChannelCurrentNP.apply();
            PI::DewChannelCurrentNP.apply();
        }

        std::chrono::milliseconds uptime(std::stol(result[PC_UPTIME]));
        using dhours = std::chrono::duration<double, std::ratio<3600 >>;
        std::stringstream ss;
        ss << std::fixed << std::setprecision(3) << dhours(uptime).count();
        FirmwareTP[FIRMWARE_UPTIME].setText(ss.str().c_str());
        FirmwareTP.apply();

        lastMetricsData = result;

        return true;
    }

    return false;
}
// Device Control
bool PegasusPPBA::reboot()
{
    return sendCommand("PF", nullptr);
}

//////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////
IPState PegasusPPBA::MoveAbsFocuser(uint32_t targetTicks)
{
    char cmd[PEGASUS_LEN] = {0}, res[PEGASUS_LEN] = {0};
    snprintf(cmd, PEGASUS_LEN, "XS:3#%u", targetTicks);
    return (sendCommand(cmd, res) ? IPS_BUSY : IPS_ALERT);
}

//////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////
IPState PegasusPPBA::MoveRelFocuser(FocusDirection dir, uint32_t ticks)
{
    return MoveAbsFocuser(dir == FOCUS_INWARD ? FocusAbsPosNP[0].getValue() - ticks : FocusAbsPosNP[0].getValue() + ticks);
}

//////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////
bool PegasusPPBA::AbortFocuser()
{
    return sendCommand("XS:6", nullptr);
}

//////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////
bool PegasusPPBA::ReverseFocuser(bool enabled)
{
    char cmd[PEGASUS_LEN] = {0};
    snprintf(cmd, PEGASUS_LEN, "XS:8#%d", enabled ? 1 : 0);
    return sendCommand(cmd, nullptr);
}

//////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////
bool PegasusPPBA::SyncFocuser(uint32_t ticks)
{
    char cmd[PEGASUS_LEN] = {0};
    snprintf(cmd, PEGASUS_LEN, "XS:5#%u", ticks);
    return sendCommand(cmd, nullptr);
}

//////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////
bool PegasusPPBA::SetFocuserBacklash(int32_t steps)
{
    char cmd[PEGASUS_LEN] = {0};
    snprintf(cmd, PEGASUS_LEN, "XS:10#%d", steps);
    return sendCommand(cmd, nullptr);
}

//////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////
bool PegasusPPBA::setFocuserMaxSpeed(uint16_t maxSpeed)
{
    char cmd[PEGASUS_LEN] = {0};
    snprintf(cmd, PEGASUS_LEN, "XS:7#%d", maxSpeed);
    return sendCommand(cmd, nullptr);
}

bool PegasusPPBA::setFocuserMicrosteps(int value)
{
    char cmd[PEGASUS_LEN] = {0};
    snprintf(cmd, PEGASUS_LEN, "XS:9#%d", value);
    return sendCommand(cmd, nullptr);
}

//////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////
bool PegasusPPBA::SetFocuserBacklashEnabled(bool enabled)
{
    char cmd[PEGASUS_LEN] = {0};
    snprintf(cmd, PEGASUS_LEN, "XS:8#%d", enabled ? 1 : 0);
    return sendCommand(cmd, nullptr);
}

//////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////
bool PegasusPPBA::getXMCStartupData()
{
    char res[PEGASUS_LEN] = {0};

    // Position
    if (sendCommand("XS:2", res))
    {
        uint32_t position = 0;
        sscanf(res, "%*[^#]#%d", &position);
        FocusAbsPosNP[0].setValue(position);
    }

    // Max speed
    if (sendCommand("XS:7", res))
    {
        uint32_t speed = 0;
        sscanf(res, "%*[^#]#%d", &speed);
        FocuserSettingsNP[SETTING_MAX_SPEED].setValue(speed);
    }

    return true;
}

//////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////
void PegasusPPBA::queryXMC()
{
    char res[PEGASUS_LEN] = {0};
    uint32_t position = 0;
    uint32_t motorRunning = 0;

    // Get Motor Status
    if (sendCommand("XS:1", res))
        sscanf(res, "%*[^#]#%d", &motorRunning);
    // Get Position
    if (sendCommand("XS:2", res))
        sscanf(res, "%*[^#]#%d", &position);

    uint32_t lastPosition = FocusAbsPosNP[0].getValue();
    FocusAbsPosNP[0].setValue(position);

    if (FocusAbsPosNP.getState() == IPS_BUSY && motorRunning == 0)
    {
        FocusAbsPosNP.setState(IPS_OK);
        FocusRelPosNP.setState(IPS_OK);
        FocusAbsPosNP.apply();
        FocusRelPosNP.apply();
    }
    else if (lastPosition != position)
        FocusAbsPosNP.apply();

}
//////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////
std::vector<std::string> PegasusPPBA::split(const std::string &input, const std::string &regex)
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
bool PegasusPPBA::SetPowerPort(size_t port, bool enabled)
{
    // Port numbers in interface are 0-based, but device expects 1-based
    return setPowerEnabled(port + 1, enabled);
}

bool PegasusPPBA::SetDewPort(size_t port, bool enabled, double dutyCycle)
{
    // DEW ports are 3, 4 for Dew A, Dew B (device uses 1-based indexing)
    // Convert duty cycle percentage (0-100) to 0-255 range
    auto pwmValue = static_cast<uint8_t>(dutyCycle / 100.0 * 255.0);
    return setDewPWM(port + 3, enabled ? pwmValue : 0);
}

bool PegasusPPBA::SetVariablePort(size_t port, bool enabled, double voltage)
{
    INDI_UNUSED(port); // PPBA only has one adjustable output

    // Set voltage first
    setAdjustableOutput(static_cast<uint8_t>(voltage));

    // Then enable/disable
    return setAdjustableOutput(enabled ? 1 : 0);

}

bool PegasusPPBA::SetLEDEnabled(bool enabled)
{
    return setLedIndicator(enabled);
}

bool PegasusPPBA::SetAutoDewEnabled(size_t port, bool enabled)
{
    INDI_UNUSED(port); // PPBA only has global auto dew control
    return setAutoDewEnabled(enabled);
}

bool PegasusPPBA::CyclePower()
{
    // PPBA does not have a specific power cycle command, reboot is the closest.
    // Or we can implement a software cycle by turning off and on all ports.
    // For now, let's just reboot.
    return reboot();
}

bool PegasusPPBA::SetUSBPort(size_t port, bool enabled)
{
    INDI_UNUSED(port);
    INDI_UNUSED(enabled);
    // PPBA does not have USB power control
    return false;
}
