/*******************************************************************************
  Copyright(c) 2018 Jasem Mutlaq. All rights reserved.

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

// We declare an auto pointer to PegasusUPB.
static std::unique_ptr<PegasusUPB> upb(new PegasusUPB());

void ISGetProperties(const char * dev)
{
    upb->ISGetProperties(dev);
}

void ISNewSwitch(const char * dev, const char * name, ISState * states, char * names[], int n)
{
    upb->ISNewSwitch(dev, name, states, names, n);
}

void ISNewText(const char * dev, const char * name, char * texts[], char * names[], int n)
{
    upb->ISNewText(dev, name, texts, names, n);
}

void ISNewNumber(const char * dev, const char * name, double values[], char * names[], int n)
{
    upb->ISNewNumber(dev, name, values, names, n);
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
    upb->ISSnoopDevice(root);
}

PegasusUPB::PegasusUPB() : FI(this), WI(this)
{
    setVersion(1, 0);

    lastSensorData.reserve(19);
    lastPowerData.reserve(3);
    lastStepperData.reserve(4);
}

bool PegasusUPB::initProperties()
{
    INDI::DefaultDevice::initProperties();

    setDriverInterface(AUX_INTERFACE | FOCUSER_INTERFACE | WEATHER_INTERFACE);

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
    // Cycle all power on/off
    IUFillSwitch(&PowerCycleAllS[POWER_CYCLE_ON], "POWER_CYCLE_ON", "All On", ISS_OFF);
    IUFillSwitch(&PowerCycleAllS[POWER_CYCLE_OFF], "POWER_CYCLE_OFF", "All Off", ISS_OFF);
    IUFillSwitchVector(&PowerCycleAllSP, PowerCycleAllS, 2, getDeviceName(), "POWER_CYCLE", "Cycle Power", MAIN_CONTROL_TAB, IP_RW, ISR_ATMOST1, 60, IPS_IDLE);

    // Reboot
    IUFillSwitch(&RebootS[0], "REBOOT", "Reboot Device", ISS_OFF);
    IUFillSwitchVector(&RebootSP, RebootS, 1, getDeviceName(), "REBOOT_DEVICE", "Device", MAIN_CONTROL_TAB, IP_RW, ISR_ATMOST1, 60, IPS_IDLE);

    // Power Sensors
    IUFillNumber(&PowerSensorsN[SENSOR_VOLTAGE], "SENSOR_VOLTAGE", "Voltage (V)", "%4.2f", 0, 999, 100, 0);
    IUFillNumber(&PowerSensorsN[SENSOR_CURRENT], "SENSOR_CURRENT", "Current (A)", "%4.2f", 0, 999, 100, 0);
    IUFillNumber(&PowerSensorsN[SENSOR_POWER], "SENSOR_POWER", "Power (W)", "%4.2f", 0, 999, 100, 0);
    IUFillNumberVector(&PowerSensorsNP, PowerSensorsN, 3, getDeviceName(), "POWER_SENSORS", "Sensors", MAIN_CONTROL_TAB, IP_RO, 60, IPS_IDLE);

    // Overall Power Consumption
    IUFillNumber(&PowerConsumptionN[CONSUMPTION_AVG_AMPS], "CONSUMPTION_AVG_AMPS", "Avg. Amps", "%4.2f", 0, 999, 100, 0);
    IUFillNumber(&PowerConsumptionN[CONSUMPTION_AMP_HOURS], "CONSUMPTION_AMP_HOURS", "Amp Hours", "%4.2f", 0, 999, 100, 0);
    IUFillNumber(&PowerConsumptionN[CONSUMPTION_WATT_HOURS], "CONSUMPTION_WATT_HOURS", "Watt Hours", "%4.2f", 0, 999, 100, 0);
    IUFillNumberVector(&PowerConsumptionNP, PowerConsumptionN, 3, getDeviceName(), "POWER_CONSUMPTION", "Consumption", MAIN_CONTROL_TAB, IP_RO, 60, IPS_IDLE);

    ////////////////////////////////////////////////////////////////////////////
    /// Power Group
    ////////////////////////////////////////////////////////////////////////////

    // Power Labels
    IUFillText(&PowerControlsLabelsT[0], "POWER_LABEL_1", "Port 1", "Port 1");
    IUFillText(&PowerControlsLabelsT[1], "POWER_LABEL_2", "Port 2", "Port 2");
    IUFillText(&PowerControlsLabelsT[2], "POWER_LABEL_3", "Port 3", "Port 3");
    IUFillText(&PowerControlsLabelsT[3], "POWER_LABEL_4", "Port 4", "Port 4");
    IUFillTextVector(&PowerControlsLabelsTP, PowerControlsLabelsT, 4, getDeviceName(), "POWER_CONTROL_LABEL", "Power Labels", POWER_TAB, IP_WO, 60, IPS_IDLE);

    char portLabel[MAXINDILABEL];

    // Turn on/off power and power boot up
    memset(portLabel, 0, MAXINDILABEL);
    int portRC = IUGetConfigText(getDeviceName(), PowerControlsLabelsTP.name, PowerControlsLabelsT[0].name, portLabel, MAXINDILABEL);
    IUFillSwitch(&PowerControlS[0], "POWER_CONTROL_1", portRC == -1 ? "Port 1" : portLabel, ISS_OFF);

    memset(portLabel, 0, MAXINDILABEL);
    portRC = IUGetConfigText(getDeviceName(), PowerControlsLabelsTP.name, PowerControlsLabelsT[1].name, portLabel, MAXINDILABEL);
    IUFillSwitch(&PowerControlS[1], "POWER_CONTROL_2", portRC == -1 ? "Port 2" : portLabel, ISS_OFF);

    memset(portLabel, 0, MAXINDILABEL);
    portRC = IUGetConfigText(getDeviceName(), PowerControlsLabelsTP.name, PowerControlsLabelsT[2].name, portLabel, MAXINDILABEL);
    IUFillSwitch(&PowerControlS[2], "POWER_CONTROL_3", portRC == -1 ? "Port 3" : portLabel, ISS_OFF);

    memset(portLabel, 0, MAXINDILABEL);
    portRC = IUGetConfigText(getDeviceName(), PowerControlsLabelsTP.name, PowerControlsLabelsT[3].name, portLabel, MAXINDILABEL);
    IUFillSwitch(&PowerControlS[3], "POWER_CONTROL_4", portRC == -1 ? "Port 4" : portLabel, ISS_OFF);

    IUFillSwitchVector(&PowerControlSP, PowerControlS, 4, getDeviceName(), "POWER_CONTROL", "Power Control", POWER_TAB, IP_RW, ISR_NOFMANY, 60, IPS_IDLE);


    // Current Draw
    IUFillNumber(&PowerCurrentN[0], "POWER_CURRENT_1", "#1 (A)", "%4.2f", 0, 1000, 0, 0);
    IUFillNumber(&PowerCurrentN[1], "POWER_CURRENT_2", "#2 (A)", "%4.2f", 0, 1000, 0, 0);
    IUFillNumber(&PowerCurrentN[2], "POWER_CURRENT_3", "#3 (A)", "%4.2f", 0, 1000, 0, 0);
    IUFillNumber(&PowerCurrentN[3], "POWER_CURRENT_4", "#4 (A)", "%4.2f", 0, 1000, 0, 0);
    IUFillNumberVector(&PowerCurrentNP, PowerCurrentN, 4, getDeviceName(), "POWER_CURRENT", "Current Draw", POWER_TAB, IP_RO, 60, IPS_IDLE);

    // Power on Boot
    IUFillSwitch(&PowerOnBootS[0], "POWER_PORT_1", PowerControlS[0].label, ISS_ON);
    IUFillSwitch(&PowerOnBootS[1], "POWER_PORT_2", PowerControlS[1].label, ISS_ON);
    IUFillSwitch(&PowerOnBootS[2], "POWER_PORT_3", PowerControlS[2].label, ISS_ON);
    IUFillSwitch(&PowerOnBootS[3], "POWER_PORT_4", PowerControlS[3].label, ISS_ON);
    IUFillSwitchVector(&PowerOnBootSP, PowerOnBootS, 4, getDeviceName(), "POWER_ON_BOOT", "Power On Boot", POWER_TAB, IP_RW, ISR_NOFMANY, 60, IPS_IDLE);

    // Over Current
    IUFillLight(&OverCurrentL[0], "POWER_PORT_1", PowerControlS[0].label, IPS_OK);
    IUFillLight(&OverCurrentL[1], "POWER_PORT_1", PowerControlS[1].label, IPS_OK);
    IUFillLight(&OverCurrentL[2], "POWER_PORT_1", PowerControlS[2].label, IPS_OK);
    IUFillLight(&OverCurrentL[3], "POWER_PORT_1", PowerControlS[3].label, IPS_OK);
    IUFillLightVector(&OverCurrentLP, OverCurrentL, 4, getDeviceName(), "POWER_OVER_CURRENT", "Over Current", POWER_TAB, IPS_IDLE);

    // Power LED
    IUFillSwitch(&PowerLEDS[POWER_LED_ON], "POWER_LED_ON", "On", ISS_ON);
    IUFillSwitch(&PowerLEDS[POWER_LED_OFF], "POWER_LED_OFF", "Off", ISS_OFF);
    IUFillSwitchVector(&PowerLEDSP, PowerLEDS, 2, getDeviceName(), "POWER_LED", "LED", POWER_TAB, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);

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

    // Dew current draw
    IUFillNumber(&DewCurrentDrawN[DEW_PWM_A], "DEW_CURRENT_A", "Dew A (A)", "%4.2f", 0, 1000, 10, 0);
    IUFillNumber(&DewCurrentDrawN[DEW_PWM_B], "DEW_CURRENT_B", "Dew B (A)", "%4.2f", 0, 1000, 10, 0);
    IUFillNumberVector(&DewCurrentDrawNP, DewCurrentDrawN, 2, getDeviceName(), "DEW_CURRENT", "Dew Current", DEW_TAB, IP_RO, 60, IPS_IDLE);

    ////////////////////////////////////////////////////////////////////////////
    /// USB Group
    ////////////////////////////////////////////////////////////////////////////

    // USB Hub control
    IUFillSwitch(&USBControlS[0], "ENABLED", "Enabled", ISS_ON);
    IUFillSwitch(&USBControlS[1], "DISABLED", "Disabled", ISS_OFF);
    IUFillSwitchVector(&USBControlSP, USBControlS, 2, getDeviceName(), "USB_PORT_CONTROL", "USB Hub", USB_TAB, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);

    // USB Hub Status
    IUFillLight(&USBStatusL[0], "PORT_1", "Port #1", IPS_OK);
    IUFillLight(&USBStatusL[1], "PORT_2", "Port #2", IPS_OK);
    IUFillLight(&USBStatusL[2], "PORT_3", "Port #3", IPS_OK);
    IUFillLight(&USBStatusL[3], "PORT_4", "Port #4", IPS_OK);
    IUFillLight(&USBStatusL[4], "PORT_5", "Port #5", IPS_OK);
    IUFillLight(&USBStatusL[5], "PORT_6", "Port #6", IPS_OK);
    IUFillLightVector(&USBStatusLP, USBStatusL, 6, getDeviceName(), "USB_PORT_STATUS", "Status", USB_TAB, IPS_IDLE);

    ////////////////////////////////////////////////////////////////////////////
    /// Focuser Group
    ////////////////////////////////////////////////////////////////////////////

    // Settings
    //    IUFillNumber(&FocusBacklashN[0], "SETTING_BACKLASH", "Backlash (steps)", "%.f", 0, 999, 100, 0);
    IUFillNumber(&FocuserSettingsN[SETTING_MAX_SPEED], "SETTING_MAX_SPEED", "Max Speed (%)", "%.2f", 0, 100, 10, 0);
    IUFillNumberVector(&FocuserSettingsNP, FocuserSettingsN, 1, getDeviceName(), "FOCUSER_SETTINGS", "Settings", FOCUS_TAB, IP_RW, 60, IPS_IDLE);

    // Backlash
    //    IUFillSwitch(&FocusBacklashS[BACKLASH_ENABLED], "BACKLASH_ENABLED", "Enabled", ISS_OFF);
    //    IUFillSwitch(&FocusBacklashS[BACKLASH_DISABLED], "BACKLASH_DISABLED", "Disabled", ISS_ON);
    //    IUFillSwitchVector(&FocusBacklashSP, FocusBacklashS, 2, getDeviceName(), "FOCUSER_BACKLASH", "Backlash", FOCUS_TAB, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);

    // Temperature
    IUFillNumber(&FocuserTemperatureN[0], "FOCUS_TEMPERATURE_VALUE", "Value (C)", "%4.2f", -50, 85, 1, 0);
    IUFillNumberVector(&FocuserTemperatureNP, FocuserTemperatureN, 1, getDeviceName(), "FOCUS_TEMPERATURE", "Temperature", FOCUS_TAB, IP_RO, 60, IPS_IDLE);

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
        defineSwitch(&PowerCycleAllSP);
        defineNumber(&PowerSensorsNP);
        defineNumber(&PowerConsumptionNP);
        defineSwitch(&RebootSP);

        // Power
        defineSwitch(&PowerControlSP);
        defineText(&PowerControlsLabelsTP);
        defineNumber(&PowerCurrentNP);
        defineSwitch(&PowerOnBootSP);
        defineLight(&OverCurrentLP);
        defineSwitch(&PowerLEDSP);

        // Dew
        defineSwitch(&AutoDewSP);
        defineNumber(&DewPWMNP);
        defineNumber(&DewCurrentDrawNP);

        // USB
        defineSwitch(&USBControlSP);
        defineLight(&USBStatusLP);

        // Focuser
        FI::updateProperties();
        defineNumber(&FocuserSettingsNP);
        //defineSwitch(&FocusBacklashSP);
        defineNumber(&FocuserTemperatureNP);

        WI::updateProperties();

        setupComplete = true;
    }
    else
    {
        // Main Control
        deleteProperty(PowerCycleAllSP.name);
        deleteProperty(PowerSensorsNP.name);
        deleteProperty(PowerConsumptionNP.name);
        deleteProperty(RebootSP.name);

        // Power
        deleteProperty(PowerControlSP.name);
        deleteProperty(PowerControlsLabelsTP.name);
        deleteProperty(PowerCurrentNP.name);
        deleteProperty(PowerOnBootSP.name);
        deleteProperty(OverCurrentLP.name);
        deleteProperty(PowerLEDSP.name);

        // Dew
        deleteProperty(AutoDewSP.name);
        deleteProperty(DewPWMNP.name);
        deleteProperty(DewCurrentDrawNP.name);

        // USB
        deleteProperty(USBControlSP.name);
        deleteProperty(USBStatusLP.name);

        // Focuser
        FI::updateProperties();
        deleteProperty(FocuserSettingsNP.name);
        //deleteProperty(FocusBacklashSP.name);
        deleteProperty(FocuserTemperatureNP.name);

        WI::updateProperties();

        setupComplete = false;
    }

    return true;
}

const char * PegasusUPB::getDefaultName()
{
    return "Pegasus UBP";
}

bool PegasusUPB::Handshake()
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

    return (!strcmp(response, "UPB_OK"));
}

bool PegasusUPB::ISNewSwitch(const char * dev, const char * name, ISState * states, char * names[], int n)
{
    if (dev && !strcmp(dev, getDeviceName()))
    {
        // Cycle all power on or off
        if (!strcmp(name, PowerCycleAllSP.name))
        {
            IUUpdateSwitch(&PowerCycleAllSP, states, names, n);

            PowerCycleAllSP.s = IPS_ALERT;
            char cmd[PEGASUS_LEN] = {0}, res[PEGASUS_LEN] = {0};
            snprintf(cmd, PEGASUS_LEN, "PZ:%d", IUFindOnSwitchIndex(&PowerCycleAllSP));
            if (sendCommand(cmd, res))
            {
                PowerCycleAllSP.s = !strcmp(cmd, res) ? IPS_OK : IPS_ALERT;
            }

            IUResetSwitch(&PowerCycleAllSP);
            IDSetSwitch(&PowerCycleAllSP, nullptr);
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

        // Control Power per port
        if (!strcmp(name, PowerControlSP.name))
        {
            bool failed = false;
            for (int i = 0; i < n; i++)
            {
                if (!strcmp(names[i], PowerControlS[i].name) && states[i] != PowerControlS[i].s)
                {
                    if (setPowerEnabled(i + 1, states[i] == ISS_ON) == false)
                    {
                        failed = true;
                        break;
                    }
                }
            }

            if (failed)
                PowerControlSP.s = IPS_ALERT;
            else
            {
                PowerControlSP.s = IPS_OK;
                IUUpdateSwitch(&PowerControlSP, states, names, n);
            }

            IDSetSwitch(&PowerControlSP, nullptr);
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

        // USB Hub Control
        if (!strcmp(name, USBControlSP.name))
        {
            int prevIndex = IUFindOnSwitchIndex(&USBControlSP);
            IUUpdateSwitch(&USBControlSP, states, names, n);
            if (setUSBHubEnabled(USBControlS[0].s == ISS_ON))
            {
                USBControlSP.s = IPS_OK;
            }
            else
            {
                IUResetSwitch(&USBControlSP);
                USBControlS[prevIndex].s = ISS_ON;
                USBControlSP.s = IPS_ALERT;
            }

            IDSetSwitch(&USBControlSP, nullptr);
            return true;
        }

        // Focuser backlash
        //        if (!strcmp(name, FocusBacklashSP.name))
        //        {
        //            int prevIndex = IUFindOnSwitchIndex(&FocusBacklashSP);
        //            IUUpdateSwitch(&FocusBacklashSP, states, names, n);
        //            if (setFocuserBacklashEnabled(FocusBacklashS[0].s == ISS_ON))
        //            {
        //                FocusBacklashSP.s = IPS_OK;
        //            }
        //            else
        //            {
        //                IUResetSwitch(&FocusBacklashSP);
        //                FocusBacklashS[prevIndex].s = ISS_ON;
        //                FocusBacklashSP.s = IPS_ALERT;
        //            }

        //            IDSetSwitch(&FocusBacklashSP, nullptr);
        //            return true;
        //        }

        // Power LED
        if (!strcmp(name, PowerLEDSP.name))
        {
            int prevIndex = IUFindOnSwitchIndex(&PowerLEDSP);
            IUUpdateSwitch(&PowerLEDSP, states, names, n);
            if (setPowerLEDEnabled(PowerLEDS[0].s == ISS_ON))
            {
                PowerLEDSP.s = IPS_OK;
            }
            else
            {
                IUResetSwitch(&PowerLEDSP);
                PowerLEDS[prevIndex].s = ISS_ON;
                PowerLEDSP.s = IPS_ALERT;
            }

            IDSetSwitch(&PowerLEDSP, nullptr);
            return true;
        }

        if (strstr(name, "FOCUS"))
            return FI::processSwitch(dev, name, states, names, n);
    }

    return DefaultDevice::ISNewSwitch(dev, name, states, names, n);
}

bool PegasusUPB::ISNewNumber(const char * dev, const char * name, double values[], char * names[], int n)
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
                    rc1 = setDewPWM(5, static_cast<uint8_t>(values[i] / 100.0 * 255.0));
                else if (!strcmp(names[i], DewPWMN[DEW_PWM_B].name))
                    rc2 = setDewPWM(6, static_cast<uint8_t>(values[i] / 100.0 * 255.0));
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
            //bool rc1 = true, rc2 = true;
            bool rc = true;
            for (int i = 0; i < n; i++)
            {
                //                if (!strcmp(names[i], FocusBacklashN[0].name) && values[i] != FocusBacklashN[0].value)
                //                    rc1 = setFocuserBacklash(values[i]);
                if (!strcmp(names[i], FocuserSettingsN[SETTING_MAX_SPEED].name) && values[i] != FocuserSettingsN[SETTING_MAX_SPEED].value)
                    rc = setFocuserMaxSpeed(values[i] / 100.0 * 999.0);
            }

            FocuserSettingsNP.s = (rc) ? IPS_OK : IPS_ALERT;
            if (FocuserSettingsNP.s == IPS_OK)
                IUUpdateNumber(&FocuserSettingsNP, values, names, n);
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

bool PegasusUPB::ISNewText(const char * dev, const char * name, char * texts[], char * names[], int n)
{
    if (dev && !strcmp(dev, getDeviceName()))
    {
        // Power Labels
        if (!strcmp(name, PowerControlsLabelsTP.name))
        {
            IUUpdateText(&PowerControlsLabelsTP, texts, names, n);
            PowerControlsLabelsTP.s = IPS_OK;
            LOG_INFO("Power port labels saved. Driver must be restarted for the labels to take effect.");
            saveConfig();
            IDSetText(&PowerControlsLabelsTP, nullptr);
            return true;
        }
    }

    return INDI::DefaultDevice::ISNewText(dev, name, texts, names, n);
}

bool PegasusUPB::sendCommand(const char * cmd, char * res)
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

IPState PegasusUPB::MoveAbsFocuser(uint32_t targetTicks)
{
    char cmd[PEGASUS_LEN] = {0}, res[PEGASUS_LEN] = {0};
    snprintf(cmd, PEGASUS_LEN, "SM:%d", targetTicks);
    if (sendCommand(cmd, res))
    {
        return (!strcmp(res, cmd) ? IPS_BUSY : IPS_ALERT);
    }

    return IPS_ALERT;
}

IPState PegasusUPB::MoveRelFocuser(FocusDirection dir, uint32_t ticks)
{
    return MoveAbsFocuser(dir == FOCUS_INWARD ? FocusAbsPosN[0].value - ticks : FocusAbsPosN[0].value + ticks);
}

bool PegasusUPB::AbortFocuser()
{
    char res[PEGASUS_LEN] = {0};
    if (sendCommand("SH", res))
    {
        return (!strcmp(res, "SH"));
    }

    return false;
}

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

bool PegasusUPB::SyncFocuser(uint32_t ticks)
{
    char cmd[PEGASUS_LEN] = {0};
    snprintf(cmd, PEGASUS_LEN, "SC:%d", ticks);
    return sendCommand(cmd, nullptr);
}

//bool PegasusUPB::setFocuserBacklash(uint16_t value)
bool PegasusUPB::SetFocuserBacklash(int32_t steps)
{
    char cmd[PEGASUS_LEN] = {0};
    snprintf(cmd, PEGASUS_LEN, "SB:%d", steps);
    return sendCommand(cmd, nullptr);
}

bool PegasusUPB::setFocuserMaxSpeed(uint16_t maxSpeed)
{
    char cmd[PEGASUS_LEN] = {0};
    snprintf(cmd, PEGASUS_LEN, "SS:%d", maxSpeed);
    return sendCommand(cmd, nullptr);
}

//bool PegasusUPB::setFocuserBacklashEnabled(bool enabled)
bool PegasusUPB::SetFocuserBacklashEnabled(bool enabled)
{
    char cmd[PEGASUS_LEN] = {0};
    snprintf(cmd, PEGASUS_LEN, "SB:%d", enabled ? 1 : 0);
    return sendCommand(cmd, nullptr);
}

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

bool PegasusUPB::setPowerOnBoot()
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

bool PegasusUPB::saveConfigItems(FILE * fp)
{
    // Save CCD Config
    INDI::DefaultDevice::saveConfigItems(fp);
    FI::saveConfigItems(fp);

    IUSaveConfigSwitch(fp, &PowerLEDSP);
    IUSaveConfigSwitch(fp, &AutoDewSP);
    IUSaveConfigNumber(fp, &FocuserSettingsNP);
    //IUSaveConfigSwitch(fp, &FocusBacklashSP);
    IUSaveConfigText(fp, &PowerControlsLabelsTP);

    return true;
}

void PegasusUPB::TimerHit()
{
    if (!isConnected() || setupComplete == false)
    {
        SetTimer(POLLMS);
        return;
    }

    if (getSensorData())
    {
        getPowerData();
        getStepperData();
    }

    SetTimer(POLLMS);
}

bool PegasusUPB::sendFirmware()
{
    char res[PEGASUS_LEN] = {0};
    if (sendCommand("PV", res))
    {
        LOGF_INFO("Detected firmware %s", res);
        return true;
    }

    return false;
}

bool PegasusUPB::getSensorData()
{
    char res[PEGASUS_LEN] = {0};
    if (sendCommand("PA", res))
    {
        std::vector<std::string> result = split(res, ":");
        if (result.size() != 19)
        {
            LOG_WARN("Received wrong number of detailed sensor data. Retrying...");
            return false;
        }

        if (result == lastSensorData)
            return true;

        // Power Sensors
        PowerSensorsN[SENSOR_VOLTAGE].value = std::stod(result[1]);
        PowerSensorsN[SENSOR_CURRENT].value = std::stod(result[2]);
        PowerSensorsN[SENSOR_POWER].value = std::stod(result[3]);
        PowerSensorsNP.s = IPS_OK;
        if (lastSensorData[0] != result[0] || lastSensorData[1] != result[1] || lastSensorData[2] != result[2])
            IDSetNumber(&PowerSensorsNP, nullptr);

        // Environment Sensors
        setParameterValue("WEATHER_TEMPERATURE", std::stod(result[4]));
        setParameterValue("WEATHER_HUMIDITY", std::stod(result[5]));
        setParameterValue("WEATHER_DEWPOINT", std::stod(result[6]));
        if (lastSensorData[4] != result[4] || lastSensorData[5] != result[5] || lastSensorData[6] != result[6])
        {
            WI::syncCriticalParameters();
            ParametersNP.s = IPS_OK;
            IDSetNumber(&ParametersNP, nullptr);
        }

        // Port Status
        const char * portStatus = result[7].c_str();
        PowerControlS[0].s = (portStatus[0] == '1') ? ISS_ON : ISS_OFF;
        PowerControlS[1].s = (portStatus[1] == '1') ? ISS_ON : ISS_OFF;
        PowerControlS[2].s = (portStatus[2] == '1') ? ISS_ON : ISS_OFF;
        PowerControlS[3].s = (portStatus[3] == '1') ? ISS_ON : ISS_OFF;
        if (lastSensorData[7] != result[7])
            IDSetSwitch(&PowerControlSP, nullptr);

        // Hub Status
        const char * usb_status = result[8].c_str();
        USBControlS[0].s = (usb_status[0] == '0') ? ISS_ON : ISS_OFF;
        USBControlS[1].s = (usb_status[0] == '0') ? ISS_OFF : ISS_ON;
        USBStatusL[0].s = (USBControlS[0].s == ISS_ON) ? IPS_OK : IPS_IDLE;
        USBStatusL[1].s = (USBControlS[0].s == ISS_ON) ? IPS_OK : IPS_IDLE;
        USBStatusL[2].s = (USBControlS[0].s == ISS_ON) ? IPS_OK : IPS_IDLE;
        USBStatusL[3].s = (USBControlS[0].s == ISS_ON) ? IPS_OK : IPS_IDLE;
        USBStatusL[4].s = (USBControlS[0].s == ISS_ON) ? IPS_OK : IPS_IDLE;
        if (lastSensorData[8] != result[8])
        {
            USBControlSP.s = (IUFindOnSwitchIndex(&USBControlSP) == 0) ? IPS_OK : IPS_IDLE;
            IDSetSwitch(&USBControlSP, nullptr);
            IDSetLight(&USBStatusLP, nullptr);
        }

        // Dew PWM
        DewPWMN[0].value = std::stod(result[9]) / 255.0 * 100.0;
        DewPWMN[1].value = std::stod(result[10]) / 255.0 * 100.0;
        if (lastSensorData[9] != result[9] || lastSensorData[10] != result[10])
            IDSetNumber(&DewPWMNP, nullptr);

        // Current draw
        PowerCurrentN[0].value = std::stod(result[11]) / 400.0;
        PowerCurrentN[1].value = std::stod(result[12]) / 400.0;
        PowerCurrentN[2].value = std::stod(result[13]) / 400.0;
        PowerCurrentN[3].value = std::stod(result[14]) / 400.0;
        if (lastSensorData[11] != result[11] || lastSensorData[12] != result[12] ||
                lastSensorData[13] != result[13] || lastSensorData[14] != result[14])
            IDSetNumber(&PowerCurrentNP, nullptr);

        DewCurrentDrawN[0].value = std::stod(result[15]) / 400.0;
        DewCurrentDrawN[1].value = std::stod(result[16]) / 400.0;
        if (lastSensorData[15] != result[15] || lastSensorData[16] != result[16])
            IDSetNumber(&DewCurrentDrawNP, nullptr);

        // Over Current
        const char * over_curent = result[17].c_str();
        OverCurrentL[0].s = (over_curent[0] == '0') ? IPS_OK : IPS_ALERT;
        OverCurrentL[1].s = (over_curent[1] == '0') ? IPS_OK : IPS_ALERT;
        OverCurrentL[2].s = (over_curent[2] == '0') ? IPS_OK : IPS_ALERT;
        OverCurrentL[3].s = (over_curent[3] == '0') ? IPS_OK : IPS_ALERT;
        if (lastSensorData[17] != result[17])
            IDSetLight(&OverCurrentLP, nullptr);

        // Auto Dew
        AutoDewS[AUTO_DEW_ENABLED].s  = (std::stoi(result[18]) == 1) ? ISS_ON : ISS_OFF;
        AutoDewS[AUTO_DEW_DISABLED].s = (std::stoi(result[18]) == 1) ? ISS_OFF : ISS_ON;
        if (lastSensorData[18] != result[18])
            IDSetSwitch(&AutoDewSP, nullptr);

        lastSensorData = result;

        return true;
    }

    return false;
}

bool PegasusUPB::getPowerData()
{
    char res[PEGASUS_LEN] = {0};
    if (sendCommand("PC", res))
    {
        std::vector<std::string> result = split(res, ":");
        if (result.size() != 3)
        {
            LOG_WARN("Received wrong number of power sensor data. Retrying...");
            return false;
        }

        if (result == lastPowerData)
            return true;

        PowerConsumptionN[CONSUMPTION_AVG_AMPS].value = std::stod(result[0]);
        PowerConsumptionN[CONSUMPTION_AMP_HOURS].value = std::stod(result[1]);
        PowerConsumptionN[CONSUMPTION_WATT_HOURS].value = std::stod(result[2]);
        PowerConsumptionNP.s = IPS_OK;
        IDSetNumber(&PowerConsumptionNP, nullptr);

        lastPowerData = result;
        return true;
    }

    return false;
}

bool PegasusUPB::getStepperData()
{
    char res[PEGASUS_LEN] = {0};
    if (sendCommand("SA", res))
    {
        std::vector<std::string> result = split(res, ":");
        if (result.size() != 4)
        {
            LOG_WARN("Received wrong number of stepper sensor data. Retrying...");
            return false;
        }

        if (result == lastStepperData)
            return true;

        FocusAbsPosN[0].value = std::stoi(result[0]);
        focusMotorRunning = (std::stoi(result[1]) == 1);

        if (FocusAbsPosNP.s == IPS_BUSY && focusMotorRunning == false)
        {
            FocusAbsPosNP.s = FocusRelPosNP.s = IPS_OK;
            IDSetNumber(&FocusAbsPosNP, nullptr);
            IDSetNumber(&FocusRelPosNP, nullptr);
        }
        else if (result[0] != lastStepperData[0])
            IDSetNumber(&FocusAbsPosNP, nullptr);

        FocusReverseS[REVERSED_ENABLED].s = (std::stoi(result[2]) == 1) ? ISS_ON : ISS_OFF;
        FocusReverseS[REVERSED_DISABLED].s = (std::stoi(result[2]) == 1) ? ISS_OFF : ISS_ON;

        if (result[2] != lastStepperData[2])
            IDSetSwitch(&FocusReverseSP, nullptr);

        uint16_t backlash = std::stoi(result[3]);
        if (backlash == 0)
        {
            FocusBacklashN[0].value = backlash;
            FocusBacklashS[BACKLASH_ENABLED].s = ISS_OFF;
            FocusBacklashS[BACKLASH_DISABLED].s = ISS_ON;
            if (result[3] != lastStepperData[3])
            {
                IDSetSwitch(&FocusBacklashSP, nullptr);
                IDSetNumber(&FocuserSettingsNP, nullptr);
            }
        }
        else
        {
            FocusBacklashS[BACKLASH_ENABLED].s = ISS_ON;
            FocusBacklashS[BACKLASH_DISABLED].s = ISS_OFF;
            FocusBacklashN[0].value = backlash;
            if (result[3] != lastStepperData[3])
            {
                IDSetSwitch(&FocusBacklashSP, nullptr);
                IDSetNumber(&FocuserSettingsNP, nullptr);
            }
        }

        lastStepperData = result;
        return true;
    }

    return false;
}

// Device Control
bool PegasusUPB::reboot()
{
    return sendCommand("PF", nullptr);
}

std::vector<std::string> PegasusUPB::split(const std::string &input, const std::string &regex)
{
    // passing -1 as the submatch index parameter performs splitting
    std::regex re(regex);
    std::sregex_token_iterator
    first{input.begin(), input.end(), re, -1},
          last;
    return {first, last};
}

bool PegasusUPB::setupParams()
{
    // Get Max Focuser Speed
    char res[PEGASUS_LEN] = {0};
    if (sendCommand("SS\n", res))
    {
        FocuserSettingsN[SETTING_MAX_SPEED].value = std::stod(res) / 999.0 * 100;
    }

    return false;
}

