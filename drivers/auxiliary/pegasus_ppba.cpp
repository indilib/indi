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
    IUFillSwitch(&QuadOutS[INDI_ENABLED], "QUADOUT_ON", "Enable", ISS_OFF);
    IUFillSwitch(&QuadOutS[INDI_DISABLED], "QUADOUT_OFF", "Disable", ISS_OFF);
    IUFillSwitchVector(&QuadOutSP, QuadOutS, 2, getDeviceName(), "QUADOUT_POWER", "Quad Output", MAIN_CONTROL_TAB,
                       IP_RW, ISR_ATMOST1, 60, IPS_IDLE);

    // Adjustable Power
    //    IUFillSwitch(&AdjOutS[INDI_ENABLED], "ADJOUT_ON", "Enable", ISS_OFF);
    //    IUFillSwitch(&AdjOutS[INDI_DISABLED], "ADJOUT_OFF", "Disable", ISS_OFF);
    //    IUFillSwitchVector(&AdjOutSP, AdjOutS, 2, getDeviceName(), "ADJOUT_POWER", "Adj Output", MAIN_CONTROL_TAB, IP_RW,
    //                       ISR_1OFMANY, 60, IPS_IDLE);

    // Adjustable Voltage
    IUFillSwitch(&AdjOutVoltS[ADJOUT_OFF], "ADJOUT_OFF", "Off", ISS_ON);
    IUFillSwitch(&AdjOutVoltS[ADJOUT_3V], "ADJOUT_3V", "3V", ISS_OFF);
    IUFillSwitch(&AdjOutVoltS[ADJOUT_5V], "ADJOUT_5V", "5V", ISS_OFF);
    IUFillSwitch(&AdjOutVoltS[ADJOUT_8V], "ADJOUT_8V", "8V", ISS_OFF);
    IUFillSwitch(&AdjOutVoltS[ADJOUT_9V], "ADJOUT_9V", "9V", ISS_OFF);
    IUFillSwitch(&AdjOutVoltS[ADJOUT_12V], "ADJOUT_12V", "12V", ISS_OFF);
    IUFillSwitchVector(&AdjOutVoltSP, AdjOutVoltS, 6, getDeviceName(), "ADJOUT_VOLTAGE", "Adj voltage", MAIN_CONTROL_TAB, IP_RW,
                       ISR_1OFMANY, 60, IPS_IDLE);

    // Reboot
    IUFillSwitch(&RebootS[0], "REBOOT", "Reboot Device", ISS_OFF);
    IUFillSwitchVector(&RebootSP, RebootS, 1, getDeviceName(), "REBOOT_DEVICE", "Device", MAIN_CONTROL_TAB, IP_RW, ISR_ATMOST1,
                       60, IPS_IDLE);

    // Power Sensors
    IUFillNumber(&PowerSensorsN[SENSOR_VOLTAGE], "SENSOR_VOLTAGE", "Voltage (V)", "%4.2f", 0, 999, 100, 0);
    IUFillNumber(&PowerSensorsN[SENSOR_CURRENT], "SENSOR_CURRENT", "Current (A)", "%4.2f", 0, 999, 100, 0);
    IUFillNumber(&PowerSensorsN[SENSOR_AVG_AMPS], "SENSOR_AVG_AMPS", "Average Current (A)", "%4.2f", 0, 999, 100, 0);
    IUFillNumber(&PowerSensorsN[SENSOR_AMP_HOURS], "SENSOR_AMP_HOURS", "Amp hours (Ah)", "%4.2f", 0, 999, 100, 0);
    IUFillNumber(&PowerSensorsN[SENSOR_WATT_HOURS], "SENSOR_WATT_HOURS", "Watt hours (Wh)", "%4.2f", 0, 999, 100, 0);
    IUFillNumber(&PowerSensorsN[SENSOR_TOTAL_CURRENT], "SENSOR_TOTAL_CURRENT", "Total current (A)", "%4.2f", 0, 999, 100, 0);
    IUFillNumber(&PowerSensorsN[SENSOR_12V_CURRENT], "SENSOR_12V_CURRENT", "12V current (A)", "%4.2f", 0, 999, 100, 0);
    IUFillNumber(&PowerSensorsN[SENSOR_DEWA_CURRENT], "SENSOR_DEWA_CURRENT", "DewA current (A)", "%4.2f", 0, 999, 100, 0);
    IUFillNumber(&PowerSensorsN[SENSOR_DEWB_CURRENT], "SENSOR_DEWB_CURRENT", "DewB current (A)", "%4.2f", 0, 999, 100, 0);
    IUFillNumberVector(&PowerSensorsNP, PowerSensorsN, 9, getDeviceName(), "POWER_SENSORS", "Sensors", MAIN_CONTROL_TAB, IP_RO,
                       60, IPS_IDLE);

    IUFillLight(&PowerWarnL[0], "POWER_WARN_ON", "Current Overload", IPS_IDLE);
    IUFillLightVector(&PowerWarnLP, PowerWarnL, 1, getDeviceName(), "POWER_WARM", "Power Warn", MAIN_CONTROL_TAB, IPS_IDLE);

    // LED Indicator
    IUFillSwitch(&LedIndicatorS[INDI_ENABLED], "LED_ON", "Enable", ISS_ON);
    IUFillSwitch(&LedIndicatorS[INDI_DISABLED], "LED_OFF", "Disable", ISS_OFF);
    IUFillSwitchVector(&LedIndicatorSP, LedIndicatorS, 2, getDeviceName(), "LED_INDICATOR", "LED Indicator", MAIN_CONTROL_TAB,
                       IP_RW,
                       ISR_1OFMANY, 60, IPS_IDLE);

    ////////////////////////////////////////////////////////////////////////////
    /// Power Group
    ////////////////////////////////////////////////////////////////////////////

    // Power on Boot
    IUFillSwitch(&PowerOnBootS[0], "POWER_PORT_1", "Quad Out", ISS_ON);
    IUFillSwitch(&PowerOnBootS[1], "POWER_PORT_2", "Adj Out", ISS_ON);
    IUFillSwitch(&PowerOnBootS[2], "POWER_PORT_3", "Dew A", ISS_ON);
    IUFillSwitch(&PowerOnBootS[3], "POWER_PORT_4", "Dew B", ISS_ON);
    IUFillSwitchVector(&PowerOnBootSP, PowerOnBootS, 4, getDeviceName(), "POWER_ON_BOOT", "Power On Boot", MAIN_CONTROL_TAB,
                       IP_RW, ISR_NOFMANY, 60, IPS_IDLE);

    ////////////////////////////////////////////////////////////////////////////
    /// Dew Group
    ////////////////////////////////////////////////////////////////////////////

    // Automatic Dew
    IUFillSwitch(&AutoDewS[INDI_ENABLED], "INDI_ENABLED", "Enabled", ISS_OFF);
    IUFillSwitch(&AutoDewS[INDI_DISABLED], "INDI_DISABLED", "Disabled", ISS_OFF);
    IUFillSwitchVector(&AutoDewSP, AutoDewS, 2, getDeviceName(), "AUTO_DEW", "Auto Dew", DEW_TAB, IP_RW, ISR_1OFMANY, 60,
                       IPS_IDLE);

    // Dew PWM
    IUFillNumber(&DewPWMN[DEW_PWM_A], "DEW_A", "Dew A (%)", "%.2f", 0, 100, 10, 0);
    IUFillNumber(&DewPWMN[DEW_PWM_B], "DEW_B", "Dew B (%)", "%.2f", 0, 100, 10, 0);
    IUFillNumberVector(&DewPWMNP, DewPWMN, 2, getDeviceName(), "DEW_PWM", "Dew PWM", DEW_TAB, IP_RW, 60, IPS_IDLE);

    ////////////////////////////////////////////////////////////////////////////
    /// Firmware Group
    ////////////////////////////////////////////////////////////////////////////
    IUFillText(&FirmwareT[FIRMWARE_VERSION], "VERSION", "Version", "NA");
    IUFillText(&FirmwareT[FIRMWARE_UPTIME], "UPTIME", "Uptime (h)", "NA");
    IUFillTextVector(&FirmwareTP, FirmwareT, 2, getDeviceName(), "FIRMWARE_INFO", "Firmware", FIRMWARE_TAB, IP_RO, 60,
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
    IUFillNumber(&FocuserSettingsN[SETTING_MAX_SPEED], "SETTING_MAX_SPEED", "Max Speed (%)", "%.f", 0, 900, 100, 400);
    IUFillNumberVector(&FocuserSettingsNP, FocuserSettingsN, 1, getDeviceName(), "FOCUSER_SETTINGS", "Settings", FOCUS_TAB,
                       IP_RW, 60, IPS_IDLE);

    // Stepping
    IUFillSwitch(&FocuserDriveS[STEP_FULL], "STEP_FULL", "Full", ISS_OFF);
    IUFillSwitch(&FocuserDriveS[STEP_HALF], "STEP_HALF", "Half", ISS_ON);
    IUFillSwitch(&FocuserDriveS[STEP_FORTH], "STEP_FORTH", "1/4", ISS_OFF);
    IUFillSwitch(&FocuserDriveS[STEP_EIGHTH], "STEP_EIGHTH", "1/8", ISS_OFF);
    IUFillSwitchVector(&FocuserDriveSP, FocuserDriveS, 4, getDeviceName(), "FOCUSER_DRIVE", "Microstepping", FOCUS_TAB,
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
        defineProperty(&QuadOutSP);
        //defineProperty(&AdjOutSP);
        defineProperty(&AdjOutVoltSP);
        defineProperty(&PowerSensorsNP);
        defineProperty(&PowerOnBootSP);
        defineProperty(&RebootSP);
        defineProperty(&PowerWarnLP);
        defineProperty(&LedIndicatorSP);

        // Dew
        defineProperty(&AutoDewSP);
        defineProperty(&DewPWMNP);

        // Focuser
        if (m_HasExternalMotor)
        {
            FI::updateProperties();
            defineProperty(&FocuserSettingsNP);
            defineProperty(&FocuserDriveSP);
        }

        WI::updateProperties();

        // Firmware
        defineProperty(&FirmwareTP);

        setupComplete = true;
    }
    else
    {
        // Main Control
        deleteProperty(QuadOutSP.name);
        //deleteProperty(AdjOutSP.name);
        deleteProperty(AdjOutVoltSP.name);
        deleteProperty(PowerSensorsNP.name);
        deleteProperty(PowerOnBootSP.name);
        deleteProperty(RebootSP.name);
        deleteProperty(PowerWarnLP.name);
        deleteProperty(LedIndicatorSP.name);

        // Dew
        deleteProperty(AutoDewSP.name);
        deleteProperty(DewPWMNP.name);

        if (m_HasExternalMotor)
        {
            FI::updateProperties();
            deleteProperty(FocuserSettingsNP.name);
            deleteProperty(FocuserDriveSP.name);
        }

        WI::updateProperties();

        deleteProperty(FirmwareTP.name);

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

    return (!strcmp(response, "PPBA_OK") || !strcmp(response, "PPBM_OK"));
}

bool PegasusPPBA::ISNewSwitch(const char * dev, const char * name, ISState * states, char * names[], int n)
{
    if (dev && !strcmp(dev, getDeviceName()))
    {
        // Quad 12V Power
        if (!strcmp(name, QuadOutSP.name))
        {
            IUUpdateSwitch(&QuadOutSP, states, names, n);

            QuadOutSP.s = IPS_ALERT;
            char cmd[PEGASUS_LEN] = {0}, res[PEGASUS_LEN] = {0};
            snprintf(cmd, PEGASUS_LEN, "P1:%d", QuadOutS[INDI_ENABLED].s == ISS_ON);
            if (sendCommand(cmd, res))
            {
                QuadOutSP.s = !strcmp(cmd, res) ? IPS_OK : IPS_ALERT;
            }

            IUResetSwitch(&QuadOutSP);
            IDSetSwitch(&QuadOutSP, nullptr);
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
        if (!strcmp(name, AdjOutVoltSP.name))
        {
            int previous_index = IUFindOnSwitchIndex(&AdjOutVoltSP);
            IUUpdateSwitch(&AdjOutVoltSP, states, names, n);
            int target_index = IUFindOnSwitchIndex(&AdjOutVoltSP);
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

            AdjOutVoltSP.s = IPS_ALERT;
            char cmd[PEGASUS_LEN] = {0}, res[PEGASUS_LEN] = {0};
            snprintf(cmd, PEGASUS_LEN, "P2:%d", adjv);
            if (sendCommand(cmd, res))
                AdjOutVoltSP.s = IPS_OK;
            else
            {
                IUResetSwitch(&AdjOutVoltSP);
                AdjOutVoltS[previous_index].s = ISS_ON;
                AdjOutVoltSP.s = IPS_ALERT;
            }

            IDSetSwitch(&AdjOutVoltSP, nullptr);
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

        // LED Indicator
        if (!strcmp(name, LedIndicatorSP.name))
        {
            IUUpdateSwitch(&LedIndicatorSP, states, names, n);
            char cmd[PEGASUS_LEN] = {0}, res[PEGASUS_LEN] = {0};
            snprintf(cmd, PEGASUS_LEN, "PL:%d", LedIndicatorS[INDI_ENABLED].s == ISS_ON);
            if (sendCommand(cmd, res))
            {
                LedIndicatorSP.s = !strcmp(cmd, res) ? IPS_OK : IPS_ALERT;
            }
            IDSetSwitch(&LedIndicatorSP, nullptr);
            saveConfig(true, LedIndicatorSP.name);
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
            if (setAutoDewEnabled(AutoDewS[INDI_ENABLED].s == ISS_ON))
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

        // Microstepping
        if (!strcmp(name, FocuserDriveSP.name))
        {
            int prevIndex = IUFindOnSwitchIndex(&FocuserDriveSP);
            IUUpdateSwitch(&FocuserDriveSP, states, names, n);
            if (setFocuserMicrosteps(IUFindOnSwitchIndex(&FocuserDriveSP) + 1))
            {
                FocuserDriveSP.s = IPS_OK;
            }
            else
            {
                IUResetSwitch(&FocuserDriveSP);
                FocuserDriveS[prevIndex].s = ISS_ON;
                FocuserDriveSP.s = IPS_ALERT;
            }

            IDSetSwitch(&FocuserDriveSP, nullptr);
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

        // Focuser Settings
        if (!strcmp(name, FocuserSettingsNP.name))
        {
            if (setFocuserMaxSpeed(values[0]))
            {
                FocuserSettingsN[0].value = values[0];
                FocuserSettingsNP.s = IPS_OK;
            }
            else
            {
                FocuserSettingsNP.s = IPS_ALERT;
            }

            IDSetNumber(&FocuserSettingsNP, nullptr);
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

bool PegasusPPBA::setPowerOnBoot()
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
        IUSaveConfigNumber(fp, &FocuserSettingsNP);
        IUSaveConfigSwitch(fp, &FocuserDriveSP);
    }
    WI::saveConfigItems(fp);
    IUSaveConfigSwitch(fp, &AutoDewSP);
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
        IUSaveText(&FirmwareT[FIRMWARE_VERSION], res);
        IDSetText(&FirmwareTP, nullptr);
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
            if (WI::syncCriticalParameters())
                IDSetLight(&critialParametersLP, nullptr);
            ParametersNP.s = IPS_OK;
            IDSetNumber(&ParametersNP, nullptr);
        }

        // Power Status
        QuadOutS[INDI_ENABLED].s = (std::stoi(result[PA_PORT_STATUS]) == 1) ? ISS_ON : ISS_OFF;
        QuadOutS[INDI_DISABLED].s = (std::stoi(result[PA_PORT_STATUS]) == 1) ? ISS_OFF : ISS_ON;
        QuadOutSP.s = (std::stoi(result[6]) == 1) ? IPS_OK : IPS_IDLE;
        if (lastSensorData[PA_PORT_STATUS] != result[PA_PORT_STATUS])
            IDSetSwitch(&QuadOutSP, nullptr);

        // Adjustable Power Status
        //        AdjOutS[INDI_ENABLED].s = (std::stoi(result[PA_ADJ_STATUS]) == 1) ? ISS_ON : ISS_OFF;
        //        AdjOutS[INDI_DISABLED].s = (std::stoi(result[PA_ADJ_STATUS]) == 1) ? ISS_OFF : ISS_ON;
        //        AdjOutSP.s = (std::stoi(result[PA_ADJ_STATUS]) == 1) ? IPS_OK : IPS_IDLE;
        //        if (lastSensorData[PA_ADJ_STATUS] != result[PA_ADJ_STATUS])
        //            IDSetSwitch(&AdjOutSP, nullptr);

        // Adjustable Power Status
        IUResetSwitch(&AdjOutVoltSP);
        if (std::stoi(result[PA_ADJ_STATUS]) == 0)
            AdjOutVoltS[ADJOUT_OFF].s = ISS_ON;
        else
        {
            AdjOutVoltS[ADJOUT_3V].s = (std::stoi(result[PA_PWRADJ]) == 3) ? ISS_ON : ISS_OFF;
            AdjOutVoltS[ADJOUT_5V].s = (std::stoi(result[PA_PWRADJ]) == 5) ? ISS_ON : ISS_OFF;
            AdjOutVoltS[ADJOUT_8V].s = (std::stoi(result[PA_PWRADJ]) == 8) ? ISS_ON : ISS_OFF;
            AdjOutVoltS[ADJOUT_9V].s = (std::stoi(result[PA_PWRADJ]) == 9) ? ISS_ON : ISS_OFF;
            AdjOutVoltS[ADJOUT_12V].s = (std::stoi(result[PA_PWRADJ]) == 12) ? ISS_ON : ISS_OFF;
        }
        if (lastSensorData[PA_PWRADJ] != result[PA_PWRADJ] || lastSensorData[PA_ADJ_STATUS] != result[PA_ADJ_STATUS])
            IDSetSwitch(&AdjOutVoltSP, nullptr);

        // Power Warn
        PowerWarnL[0].s = (std::stoi(result[PA_PWR_WARN]) == 1) ? IPS_ALERT : IPS_OK;
        PowerWarnLP.s = (std::stoi(result[PA_PWR_WARN]) == 1) ? IPS_ALERT : IPS_OK;
        if (lastSensorData[PA_PWR_WARN] != result[PA_PWR_WARN])
            IDSetLight(&PowerWarnLP, nullptr);

        // Dew PWM
        DewPWMN[0].value = std::stod(result[PA_DEW_1]) / 255.0 * 100.0;
        DewPWMN[1].value = std::stod(result[PA_DEW_2]) / 255.0 * 100.0;
        if (lastSensorData[PA_DEW_1] != result[PA_DEW_1] || lastSensorData[PA_DEW_2] != result[PA_DEW_2])
            IDSetNumber(&DewPWMNP, nullptr);

        // Auto Dew
        AutoDewS[INDI_DISABLED].s = (std::stoi(result[PA_AUTO_DEW]) == 1) ? ISS_OFF : ISS_ON;
        AutoDewS[INDI_ENABLED].s  = (std::stoi(result[PA_AUTO_DEW]) == 1) ? ISS_ON : ISS_OFF;
        AutoDewSP.s = (std::stoi(result[PA_AUTO_DEW]) == 1) ? IPS_OK : IPS_IDLE;
        if (lastSensorData[PA_AUTO_DEW] != result[PA_AUTO_DEW])
            IDSetSwitch(&AutoDewSP, nullptr);

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
        PowerSensorsN[SENSOR_AVG_AMPS].value = std::stod(result[PS_AVG_AMPS]);
        PowerSensorsN[SENSOR_AMP_HOURS].value = std::stod(result[PS_AMP_HOURS]);
        PowerSensorsN[SENSOR_WATT_HOURS].value = std::stod(result[PS_WATT_HOURS]);
        PowerSensorsNP.s = IPS_OK;
        if (lastConsumptionData[PS_AVG_AMPS] != result[PS_AVG_AMPS] || lastConsumptionData[PS_AMP_HOURS] != result[PS_AMP_HOURS]
                || lastConsumptionData[PS_WATT_HOURS] != result[PS_WATT_HOURS])
            IDSetNumber(&PowerSensorsNP, nullptr);

        lastConsumptionData = result;

        return true;
    }

    return false;
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
        PowerSensorsN[SENSOR_TOTAL_CURRENT].value = std::stod(result[PC_TOTAL_CURRENT]);
        PowerSensorsN[SENSOR_12V_CURRENT].value = std::stod(result[PC_12V_CURRENT]);
        PowerSensorsN[SENSOR_DEWA_CURRENT].value = std::stod(result[PC_DEWA_CURRENT]);
        PowerSensorsN[SENSOR_DEWB_CURRENT].value = std::stod(result[PC_DEWB_CURRENT]);
        PowerSensorsNP.s = IPS_OK;
        if (lastMetricsData[PC_TOTAL_CURRENT] != result[PC_TOTAL_CURRENT] ||
                lastMetricsData[PC_12V_CURRENT] != result[PC_12V_CURRENT] ||
                lastMetricsData[PC_DEWA_CURRENT] != result[PC_DEWA_CURRENT] ||
                lastMetricsData[PC_DEWB_CURRENT] != result[PC_DEWB_CURRENT])
            IDSetNumber(&PowerSensorsNP, nullptr);

        std::chrono::milliseconds uptime(std::stol(result[PC_UPTIME]));
        using dhours = std::chrono::duration<double, std::ratio<3600>>;
        std::stringstream ss;
        ss << std::fixed << std::setprecision(3) << dhours(uptime).count();
        IUSaveText(&FirmwareT[FIRMWARE_UPTIME], ss.str().c_str());
        IDSetText(&FirmwareTP, nullptr);

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
    return MoveAbsFocuser(dir == FOCUS_INWARD ? FocusAbsPosN[0].value - ticks : FocusAbsPosN[0].value + ticks);
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
        FocusAbsPosN[0].value = position;
    }

    // Max speed
    if (sendCommand("XS:7", res))
    {
        uint32_t speed = 0;
        sscanf(res, "%*[^#]#%d", &speed);
        FocuserSettingsN[0].value = speed;
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

    uint32_t lastPosition = FocusAbsPosN[0].value;
    FocusAbsPosN[0].value = position;

    if (FocusAbsPosNP.s == IPS_BUSY && motorRunning == 0)
    {
        FocusAbsPosNP.s = IPS_OK;
        FocusRelPosNP.s = IPS_OK;
        IDSetNumber(&FocusAbsPosNP, nullptr);
        IDSetNumber(&FocusRelPosNP, nullptr);
    }
    else if (lastPosition != position)
        IDSetNumber(&FocusAbsPosNP, nullptr);

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

