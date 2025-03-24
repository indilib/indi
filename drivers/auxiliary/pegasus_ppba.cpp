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

PegasusPPBA::PegasusPPBA() : FI(this), WI(this)
{
    setVersion(1, 2);
    lastSensorData.reserve(PA_N);
    lastConsumptionData.reserve(PS_N);
    lastMetricsData.reserve(PC_N);
}

bool PegasusPPBA::initProperties()
{
    INDI::DefaultDevice::initProperties();

    setDriverInterface(AUX_INTERFACE | WEATHER_INTERFACE);

    FI::SetCapability(FOCUSER_CAN_ABS_MOVE |
                      FOCUSER_CAN_REL_MOVE |
                      FOCUSER_CAN_REVERSE  |
                      FOCUSER_CAN_SYNC     |
                      FOCUSER_CAN_ABORT    |
                      FOCUSER_HAS_BACKLASH);

    FI::initProperties(FOCUS_TAB);
    WI::initProperties(ENVIRONMENT_TAB, ENVIRONMENT_TAB);

    addAuxControls();

    ////////////////////////////////////////////////////////////////////////////
    /// Main Control Panel
    ////////////////////////////////////////////////////////////////////////////
    // Quad 12v Power
    QuadOutSP[INDI_ENABLED].fill("QUADOUT_ON", "Enable", ISS_OFF);
    QuadOutSP[INDI_DISABLED].fill("QUADOUT_OFF", "Disable", ISS_OFF);
    QuadOutSP.fill(getDeviceName(), "QUADOUT_POWER", "Quad Output", MAIN_CONTROL_TAB,
                   IP_RW, ISR_ATMOST1, 60, IPS_IDLE);

    // Adjustable Power
    //    IUFillSwitch(&AdjOutS[INDI_ENABLED], "ADJOUT_ON", "Enable", ISS_OFF);
    //    IUFillSwitch(&AdjOutS[INDI_DISABLED], "ADJOUT_OFF", "Disable", ISS_OFF);
    //    IUFillSwitchVector(&AdjOutSP, AdjOutS, 2, getDeviceName(), "ADJOUT_POWER", "Adj Output", MAIN_CONTROL_TAB, IP_RW,
    //                       ISR_1OFMANY, 60, IPS_IDLE);

    // Adjustable Voltage
    AdjOutVoltSP[ADJOUT_OFF].fill("ADJOUT_OFF", "Off", ISS_ON);
    AdjOutVoltSP[ADJOUT_3V].fill("ADJOUT_3V", "3V", ISS_OFF);
    AdjOutVoltSP[ADJOUT_5V].fill("ADJOUT_5V", "5V", ISS_OFF);
    AdjOutVoltSP[ADJOUT_8V].fill("ADJOUT_8V", "8V", ISS_OFF);
    AdjOutVoltSP[ADJOUT_9V].fill("ADJOUT_9V", "9V", ISS_OFF);
    AdjOutVoltSP[ADJOUT_12V].fill("ADJOUT_12V", "12V", ISS_OFF);
    AdjOutVoltSP.fill(getDeviceName(), "ADJOUT_VOLTAGE", "Adj voltage", MAIN_CONTROL_TAB, IP_RW,
                      ISR_1OFMANY, 60, IPS_IDLE);

    // Reboot
    RebootSP[0].fill("REBOOT", "Reboot Device", ISS_OFF);
    RebootSP.fill(getDeviceName(), "REBOOT_DEVICE", "Device", MAIN_CONTROL_TAB, IP_RW, ISR_ATMOST1,
                  60, IPS_IDLE);

    // Power Sensors
    PowerSensorsNP[SENSOR_VOLTAGE].fill("SENSOR_VOLTAGE", "Voltage (V)", "%4.2f", 0, 999, 100, 0);
    PowerSensorsNP[SENSOR_CURRENT].fill("SENSOR_CURRENT", "Current (A)", "%4.2f", 0, 999, 100, 0);
    PowerSensorsNP[SENSOR_AVG_AMPS].fill("SENSOR_AVG_AMPS", "Average Current (A)", "%4.2f", 0, 999, 100, 0);
    PowerSensorsNP[SENSOR_AMP_HOURS].fill("SENSOR_AMP_HOURS", "Amp hours (Ah)", "%4.2f", 0, 999, 100, 0);
    PowerSensorsNP[SENSOR_WATT_HOURS].fill("SENSOR_WATT_HOURS", "Watt hours (Wh)", "%4.2f", 0, 999, 100, 0);
    PowerSensorsNP[SENSOR_TOTAL_CURRENT].fill("SENSOR_TOTAL_CURRENT", "Total current (A)", "%4.2f", 0, 999, 100, 0);
    PowerSensorsNP[SENSOR_12V_CURRENT].fill("SENSOR_12V_CURRENT", "12V current (A)", "%4.2f", 0, 999, 100, 0);
    PowerSensorsNP[SENSOR_DEWA_CURRENT].fill("SENSOR_DEWA_CURRENT", "DewA current (A)", "%4.2f", 0, 999, 100, 0);
    PowerSensorsNP[SENSOR_DEWB_CURRENT].fill("SENSOR_DEWB_CURRENT", "DewB current (A)", "%4.2f", 0, 999, 100, 0);
    PowerSensorsNP.fill(getDeviceName(), "POWER_SENSORS", "Sensors", MAIN_CONTROL_TAB, IP_RO,
                        60, IPS_IDLE);

    PowerWarnLP[0].fill("POWER_WARN_ON", "Current Overload", IPS_IDLE);
    PowerWarnLP.fill(getDeviceName(), "POWER_WARM", "Power Warn", MAIN_CONTROL_TAB, IPS_IDLE);

    // LED Indicator
    LedIndicatorSP[INDI_ENABLED].fill("LED_ON", "Enable", ISS_ON);
    LedIndicatorSP[INDI_DISABLED].fill("LED_OFF", "Disable", ISS_OFF);
    LedIndicatorSP.fill(getDeviceName(), "LED_INDICATOR", "LED Indicator", MAIN_CONTROL_TAB,
                        IP_RW,
                        ISR_1OFMANY, 60, IPS_IDLE);

    ////////////////////////////////////////////////////////////////////////////
    /// Power Group
    ////////////////////////////////////////////////////////////////////////////

    // Power on Boot
    PowerOnBootSP[POWER_PORT_1].fill("POWER_PORT_1", "Quad Out", ISS_ON);
    PowerOnBootSP[POWER_PORT_2].fill("POWER_PORT_2", "Adj Out", ISS_ON);
    PowerOnBootSP[POWER_PORT_3].fill("POWER_PORT_3", "Dew A", ISS_ON);
    PowerOnBootSP[POWER_PORT_4].fill("POWER_PORT_4", "Dew B", ISS_ON);
    PowerOnBootSP.fill(getDeviceName(), "POWER_ON_BOOT", "Power On Boot", MAIN_CONTROL_TAB,
                       IP_RW, ISR_NOFMANY, 60, IPS_IDLE);

    ////////////////////////////////////////////////////////////////////////////
    /// Dew Group
    ////////////////////////////////////////////////////////////////////////////

    // Automatic Dew
    AutoDewSP[INDI_ENABLED].fill("INDI_ENABLED", "Enabled", ISS_OFF);
    AutoDewSP[INDI_DISABLED].fill("INDI_DISABLED", "Disabled", ISS_OFF);
    AutoDewSP.fill(getDeviceName(), "AUTO_DEW", "Auto Dew", DEW_TAB, IP_RW, ISR_1OFMANY, 60,
                   IPS_IDLE);

    AutoDewSettingsNP[AUTO_DEW_AGGRESSION].fill("AGGRESSION", "Aggresiveness (%)", "%.2f", 0, 100, 10, 0);
    AutoDewSettingsNP.fill(getDeviceName(), "AUTO_DEW_SETTINGS", "Auto Dew Settings", DEW_TAB, IP_RW, 60, IPS_IDLE);


    // Dew PWM
    DewPWMNP[DEW_PWM_A].fill("DEW_A", "Dew A (%)", "%.2f", 0, 100, 10, 0);
    DewPWMNP[DEW_PWM_B].fill("DEW_B", "Dew B (%)", "%.2f", 0, 100, 10, 0);
    DewPWMNP.fill(getDeviceName(), "DEW_PWM", "Dew PWM", DEW_TAB, IP_RW, 60, IPS_IDLE);

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
        defineProperty(QuadOutSP);
        //defineProperty(&AdjOutSP);
        defineProperty(AdjOutVoltSP);
        defineProperty(PowerSensorsNP);
        defineProperty(PowerOnBootSP);
        defineProperty(RebootSP);
        defineProperty(PowerWarnLP);
        defineProperty(LedIndicatorSP);

        // Dew
        defineProperty(AutoDewSP);
        defineProperty(AutoDewSettingsNP);
        defineProperty(DewPWMNP);

        getAutoDewAggression();

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
        deleteProperty(QuadOutSP);
        //deleteProperty(AdjOutSP.name);
        deleteProperty(AdjOutVoltSP);
        deleteProperty(PowerSensorsNP);
        deleteProperty(PowerOnBootSP);
        deleteProperty(RebootSP);
        deleteProperty(PowerWarnLP);
        deleteProperty(LedIndicatorSP);

        // Dew
        deleteProperty(AutoDewSP);
        deleteProperty(AutoDewSettingsNP);
        deleteProperty(DewPWMNP);

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
        // Quad 12V Power
        if (QuadOutSP.isNameMatch(name))
        {
            QuadOutSP.update(states, names, n);

            QuadOutSP.setState(IPS_ALERT);
            char cmd[PEGASUS_LEN] = {0}, res[PEGASUS_LEN] = {0};
            snprintf(cmd, PEGASUS_LEN, "P1:%d", QuadOutSP[INDI_ENABLED].getState() == ISS_ON);
            if (sendCommand(cmd, res))
            {
                QuadOutSP.setState(!strcmp(cmd, res) ? IPS_OK : IPS_ALERT);
            }

            QuadOutSP.reset();
            QuadOutSP.apply();
            return true;
        }

        //        // Adjustable Power
        //        if (!strcmp(name, AdjOutSP.name))
        //        {
        //            IUUpdateSwitch(&AdjOutSP, states, names, n);

        //            AdjOutSP.s = IPS_ALERT;
        //            char cmd[PEGASUS_LEN] = {0}, res[PEGASUS_LEN] = {0};
        //            snprintf(cmd, PEGASUS_LEN, "P2:%d", AdjOutS[INDI_ENABLED].s == ISS_ON);
        //            if (sendCommand(cmd, res))
        //            {
        //                AdjOutSP.s = !strcmp(cmd, res) ? IPS_OK : IPS_ALERT;
        //            }

        //            IUResetSwitch(&AdjOutSP);
        //            IDSetSwitch(&AdjOutSP, nullptr);
        //            return true;
        //        }

        // Adjustable Voltage
        if (AdjOutVoltSP.isNameMatch(name))
        {
            int previous_index = AdjOutVoltSP.findOnSwitchIndex();
            AdjOutVoltSP.update(states, names, n);
            int target_index = AdjOutVoltSP.findOnSwitchIndex();
            int adjv = 0;
            switch(target_index)
            {
                case ADJOUT_OFF:
                    adjv = 0;
                    break;
                case ADJOUT_3V:
                    adjv = 3;
                    break;
                case ADJOUT_5V:
                    adjv = 5;
                    break;
                case ADJOUT_8V:
                    adjv = 8;
                    break;
                case ADJOUT_9V:
                    adjv = 9;
                    break;
                case ADJOUT_12V:
                    adjv = 12;
                    break;
            }

            AdjOutVoltSP.setState(IPS_ALERT);
            char cmd[PEGASUS_LEN] = {0}, res[PEGASUS_LEN] = {0};
            snprintf(cmd, PEGASUS_LEN, "P2:%d", adjv);
            if (sendCommand(cmd, res))
                AdjOutVoltSP.setState(IPS_OK);
            else
            {
                AdjOutVoltSP.reset();
                AdjOutVoltSP[previous_index].setState(ISS_ON);
                AdjOutVoltSP.setState(IPS_ALERT);
            }

            AdjOutVoltSP.apply();
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

        // LED Indicator
        if (LedIndicatorSP.isNameMatch(name))
        {
            LedIndicatorSP.update(states, names, n);
            char cmd[PEGASUS_LEN] = {0}, res[PEGASUS_LEN] = {0};
            snprintf(cmd, PEGASUS_LEN, "PL:%d", LedIndicatorSP[INDI_ENABLED].getState() == ISS_ON);
            if (sendCommand(cmd, res))
            {
                LedIndicatorSP.setState(!strcmp(cmd, res) ? IPS_OK : IPS_ALERT);
            }
            LedIndicatorSP.apply();
            saveConfig(true, LedIndicatorSP.getName());
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

        // Auto Dew Settings
        if (AutoDewSettingsNP.isNameMatch(name))
        {
            AutoDewSettingsNP.setState(setAutoDewAggression(static_cast<uint8_t>(values[AUTO_DEW_AGGRESSION] / 100.0 * 255.0)) ?
                                       IPS_OK : IPS_ALERT);
            if (AutoDewSettingsNP.getState() == IPS_OK)
                AutoDewSettingsNP.update(values, names, n);
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
    AutoDewSP.save(fp);
    AutoDewSettingsNP.save(fp);
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
        QuadOutSP[INDI_ENABLED].setState((std::stoi(result[PA_PORT_STATUS]) == 1) ? ISS_ON : ISS_OFF);
        QuadOutSP[INDI_DISABLED].setState((std::stoi(result[PA_PORT_STATUS]) == 1) ? ISS_OFF : ISS_ON);
        QuadOutSP.setState((std::stoi(result[6]) == 1) ? IPS_OK : IPS_IDLE);
        if (lastSensorData[PA_PORT_STATUS] != result[PA_PORT_STATUS])
            QuadOutSP.apply();

        // Adjustable Power Status
        //        AdjOutS[INDI_ENABLED].s = (std::stoi(result[PA_ADJ_STATUS]) == 1) ? ISS_ON : ISS_OFF;
        //        AdjOutS[INDI_DISABLED].s = (std::stoi(result[PA_ADJ_STATUS]) == 1) ? ISS_OFF : ISS_ON;
        //        AdjOutSP.s = (std::stoi(result[PA_ADJ_STATUS]) == 1) ? IPS_OK : IPS_IDLE;
        //        if (lastSensorData[PA_ADJ_STATUS] != result[PA_ADJ_STATUS])
        //            IDSetSwitch(&AdjOutSP, nullptr);

        // Adjustable Power Status
        AdjOutVoltSP.reset();
        if (std::stoi(result[PA_ADJ_STATUS]) == 0)
            AdjOutVoltSP[ADJOUT_OFF].setState(ISS_ON);
        else
        {
            AdjOutVoltSP[ADJOUT_3V].setState((std::stoi(result[PA_PWRADJ]) == 3) ? ISS_ON : ISS_OFF);
            AdjOutVoltSP[ADJOUT_5V].setState((std::stoi(result[PA_PWRADJ]) == 5) ? ISS_ON : ISS_OFF);
            AdjOutVoltSP[ADJOUT_8V].setState((std::stoi(result[PA_PWRADJ]) == 8) ? ISS_ON : ISS_OFF);
            AdjOutVoltSP[ADJOUT_9V].setState((std::stoi(result[PA_PWRADJ]) == 9) ? ISS_ON : ISS_OFF);
            AdjOutVoltSP[ADJOUT_12V].setState((std::stoi(result[PA_PWRADJ]) == 12) ? ISS_ON : ISS_OFF);
        }
        if (lastSensorData[PA_PWRADJ] != result[PA_PWRADJ] || lastSensorData[PA_ADJ_STATUS] != result[PA_ADJ_STATUS])
            AdjOutVoltSP.apply();

        // Power Warn
        PowerWarnLP[0].setState((std::stoi(result[PA_PWR_WARN]) == 1) ? IPS_ALERT : IPS_OK);
        PowerWarnLP.setState((std::stoi(result[PA_PWR_WARN]) == 1) ? IPS_ALERT : IPS_OK);
        if (lastSensorData[PA_PWR_WARN] != result[PA_PWR_WARN])
            PowerWarnLP.apply();

        // Dew PWM
        DewPWMNP[DEW_PWM_A].setValue(std::stod(result[PA_DEW_1]) / 255.0 * 100.0);
        DewPWMNP[DEW_PWM_B].setValue(std::stod(result[PA_DEW_2]) / 255.0 * 100.0);
        if (lastSensorData[PA_DEW_1] != result[PA_DEW_1] || lastSensorData[PA_DEW_2] != result[PA_DEW_2])
            DewPWMNP.apply();

        // Auto Dew
        AutoDewSP[INDI_DISABLED].setState((std::stoi(result[PA_AUTO_DEW]) == 1) ? ISS_OFF : ISS_ON);
        AutoDewSP[INDI_ENABLED].setState((std::stoi(result[PA_AUTO_DEW]) == 1) ? ISS_ON : ISS_OFF);
        AutoDewSP.setState((std::stoi(result[PA_AUTO_DEW]) == 1) ? IPS_OK : IPS_IDLE);
        if (lastSensorData[PA_AUTO_DEW] != result[PA_AUTO_DEW])
            AutoDewSP.apply();

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

        // Power Sensors
        PowerSensorsNP[SENSOR_AVG_AMPS].setValue(std::stod(result[PS_AVG_AMPS]));
        PowerSensorsNP[SENSOR_AMP_HOURS].setValue(std::stod(result[PS_AMP_HOURS]));
        PowerSensorsNP[SENSOR_WATT_HOURS].setValue(std::stod(result[PS_WATT_HOURS]));
        PowerSensorsNP.setState(IPS_OK);
        if (lastConsumptionData[PS_AVG_AMPS] != result[PS_AVG_AMPS] || lastConsumptionData[PS_AMP_HOURS] != result[PS_AMP_HOURS]
                || lastConsumptionData[PS_WATT_HOURS] != result[PS_WATT_HOURS])
            PowerSensorsNP.apply();

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

        // Power Sensors
        PowerSensorsNP[SENSOR_TOTAL_CURRENT].setValue(std::stod(result[PC_TOTAL_CURRENT]));
        PowerSensorsNP[SENSOR_12V_CURRENT].setValue(std::stod(result[PC_12V_CURRENT]));
        PowerSensorsNP[SENSOR_DEWA_CURRENT].setValue(std::stod(result[PC_DEWA_CURRENT]));
        PowerSensorsNP[SENSOR_DEWB_CURRENT].setValue(std::stod(result[PC_DEWB_CURRENT]));
        PowerSensorsNP.setState(IPS_OK);
        if (lastMetricsData[PC_TOTAL_CURRENT] != result[PC_TOTAL_CURRENT] ||
                lastMetricsData[PC_12V_CURRENT] != result[PC_12V_CURRENT] ||
                lastMetricsData[PC_DEWA_CURRENT] != result[PC_DEWA_CURRENT] ||
                lastMetricsData[PC_DEWB_CURRENT] != result[PC_DEWB_CURRENT])
            PowerSensorsNP.apply();

        std::chrono::milliseconds uptime(std::stol(result[PC_UPTIME]));
        using dhours = std::chrono::duration<double, std::ratio<3600>>;
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

