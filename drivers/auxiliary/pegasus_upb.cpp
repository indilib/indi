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
#include <chrono>
#include <math.h>
#include <iomanip>

// We declare an auto pointer to PegasusUPB.
static std::unique_ptr<PegasusUPB> upb(new PegasusUPB());

PegasusUPB::PegasusUPB() : FI(this), WI(this)
{
    setVersion(1, 5);

    lastSensorData.reserve(21);
    lastPowerData.reserve(4);
    lastStepperData.reserve(4);
    lastDewAggData.reserve(1);
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
    IUFillSwitchVector(&PowerCycleAllSP, PowerCycleAllS, 2, getDeviceName(), "POWER_CYCLE", "Cycle Power", MAIN_CONTROL_TAB,
                       IP_RW, ISR_ATMOST1, 60, IPS_IDLE);

    // Reboot
    IUFillSwitch(&RebootS[0], "REBOOT", "Reboot Device", ISS_OFF);
    IUFillSwitchVector(&RebootSP, RebootS, 1, getDeviceName(), "REBOOT_DEVICE", "Device", MAIN_CONTROL_TAB, IP_RW, ISR_ATMOST1,
                       60, IPS_IDLE);

    // Power Sensors
    IUFillNumber(&PowerSensorsN[SENSOR_VOLTAGE], "SENSOR_VOLTAGE", "Voltage (V)", "%4.2f", 0, 999, 100, 0);
    IUFillNumber(&PowerSensorsN[SENSOR_CURRENT], "SENSOR_CURRENT", "Current (A)", "%4.2f", 0, 999, 100, 0);
    IUFillNumber(&PowerSensorsN[SENSOR_POWER], "SENSOR_POWER", "Power (W)", "%4.2f", 0, 999, 100, 0);
    IUFillNumberVector(&PowerSensorsNP, PowerSensorsN, 3, getDeviceName(), "POWER_SENSORS", "Sensors", MAIN_CONTROL_TAB, IP_RO,
                       60, IPS_IDLE);

    // Overall Power Consumption
    IUFillNumber(&PowerConsumptionN[CONSUMPTION_AVG_AMPS], "CONSUMPTION_AVG_AMPS", "Avg. Amps", "%4.2f", 0, 999, 100, 0);
    IUFillNumber(&PowerConsumptionN[CONSUMPTION_AMP_HOURS], "CONSUMPTION_AMP_HOURS", "Amp Hours", "%4.2f", 0, 999, 100, 0);
    IUFillNumber(&PowerConsumptionN[CONSUMPTION_WATT_HOURS], "CONSUMPTION_WATT_HOURS", "Watt Hours", "%4.2f", 0, 999, 100, 0);
    IUFillNumberVector(&PowerConsumptionNP, PowerConsumptionN, 3, getDeviceName(), "POWER_CONSUMPTION", "Consumption",
                       MAIN_CONTROL_TAB, IP_RO, 60, IPS_IDLE);

    ////////////////////////////////////////////////////////////////////////////
    /// Power Group
    ////////////////////////////////////////////////////////////////////////////

    // Dew Labels. Need to delare them here to use in the Power usage section
    IUFillText(&DewControlsLabelsT[0], "DEW_LABEL_1", "Dew A", "Dew A");
    IUFillText(&DewControlsLabelsT[1], "DEW_LABEL_2", "Dew B", "Dew B");
    IUFillText(&DewControlsLabelsT[2], "DEW_LABEL_3", "Dew C", "Dew C");
    IUFillTextVector(&DewControlsLabelsTP, DewControlsLabelsT, 3, getDeviceName(), "DEW_CONTROL_LABEL", "Dew Labels",
                     DEW_TAB, IP_WO, 60, IPS_IDLE);

    char dewLabel[MAXINDILABEL];

    // Turn on/off power and power boot up
    memset(dewLabel, 0, MAXINDILABEL);
    int dewRC = IUGetConfigText(getDeviceName(), DewControlsLabelsTP.name, DewControlsLabelsT[0].name, dewLabel,
                                MAXINDILABEL);
    IUFillSwitch(&AutoDewV2S[DEW_PWM_A], "DEW_A", dewRC == -1 ? "Dew A" : dewLabel, ISS_OFF);
    memset(dewLabel, 0, MAXINDILABEL);
    dewRC = IUGetConfigText(getDeviceName(), DewControlsLabelsTP.name, DewControlsLabelsT[1].name, dewLabel,
                            MAXINDILABEL);
    IUFillSwitch(&AutoDewV2S[DEW_PWM_B], "DEW_B", dewRC == -1 ? "Dew B" : dewLabel, ISS_OFF);
    memset(dewLabel, 0, MAXINDILABEL);
    dewRC = IUGetConfigText(getDeviceName(), DewControlsLabelsTP.name, DewControlsLabelsT[2].name, dewLabel,
                            MAXINDILABEL);
    IUFillSwitch(&AutoDewV2S[DEW_PWM_C], "DEW_C", dewRC == -1 ? "Dew C" : dewLabel, ISS_OFF);
    IUFillSwitchVector(&AutoDewV2SP, AutoDewV2S, 3, getDeviceName(), "AUTO_DEW", "Auto Dew", DEW_TAB, IP_RW, ISR_NOFMANY, 60,
                       IPS_IDLE);

    // Dew Labels with custom labels
    IUFillText(&DewControlsLabelsT[0], "DEW_LABEL_1", "Dew A", AutoDewV2S[0].label);
    IUFillText(&DewControlsLabelsT[1], "DEW_LABEL_2", "Dew B", AutoDewV2S[1].label);
    IUFillText(&DewControlsLabelsT[2], "DEW_LABEL_3", "Dew C", AutoDewV2S[2].label);
    IUFillTextVector(&DewControlsLabelsTP, DewControlsLabelsT, 3, getDeviceName(), "DEW_CONTROL_LABEL", "DEW Labels",
                     DEW_TAB, IP_WO, 60, IPS_IDLE);
    // Power Labels
    IUFillText(&PowerControlsLabelsT[0], "POWER_LABEL_1", "Port 1", "Port 1");
    IUFillText(&PowerControlsLabelsT[1], "POWER_LABEL_2", "Port 2", "Port 2");
    IUFillText(&PowerControlsLabelsT[2], "POWER_LABEL_3", "Port 3", "Port 3");
    IUFillText(&PowerControlsLabelsT[3], "POWER_LABEL_4", "Port 4", "Port 4");
    IUFillTextVector(&PowerControlsLabelsTP, PowerControlsLabelsT, 4, getDeviceName(), "POWER_CONTROL_LABEL", "Power Labels",
                     POWER_TAB, IP_WO, 60, IPS_IDLE);

    char portLabel[MAXINDILABEL];

    // Turn on/off power and power boot up
    memset(portLabel, 0, MAXINDILABEL);
    int portRC = IUGetConfigText(getDeviceName(), PowerControlsLabelsTP.name, PowerControlsLabelsT[0].name, portLabel,
                                 MAXINDILABEL);
    IUFillSwitch(&PowerControlS[0], "POWER_CONTROL_1", portRC == -1 ? "Port 1" : portLabel, ISS_OFF);

    memset(portLabel, 0, MAXINDILABEL);
    portRC = IUGetConfigText(getDeviceName(), PowerControlsLabelsTP.name, PowerControlsLabelsT[1].name, portLabel,
                             MAXINDILABEL);
    IUFillSwitch(&PowerControlS[1], "POWER_CONTROL_2", portRC == -1 ? "Port 2" : portLabel, ISS_OFF);

    memset(portLabel, 0, MAXINDILABEL);
    portRC = IUGetConfigText(getDeviceName(), PowerControlsLabelsTP.name, PowerControlsLabelsT[2].name, portLabel,
                             MAXINDILABEL);
    IUFillSwitch(&PowerControlS[2], "POWER_CONTROL_3", portRC == -1 ? "Port 3" : portLabel, ISS_OFF);

    memset(portLabel, 0, MAXINDILABEL);
    portRC = IUGetConfigText(getDeviceName(), PowerControlsLabelsTP.name, PowerControlsLabelsT[3].name, portLabel,
                             MAXINDILABEL);
    IUFillSwitch(&PowerControlS[3], "POWER_CONTROL_4", portRC == -1 ? "Port 4" : portLabel, ISS_OFF);

    IUFillSwitchVector(&PowerControlSP, PowerControlS, 4, getDeviceName(), "POWER_CONTROL", "Power Control", POWER_TAB, IP_RW,
                       ISR_NOFMANY, 60, IPS_IDLE);

    // Power Labels
    IUFillText(&PowerControlsLabelsT[0], "POWER_LABEL_1", "Port 1", PowerControlS[0].label);
    IUFillText(&PowerControlsLabelsT[1], "POWER_LABEL_2", "Port 2", PowerControlS[1].label);
    IUFillText(&PowerControlsLabelsT[2], "POWER_LABEL_3", "Port 3", PowerControlS[2].label);
    IUFillText(&PowerControlsLabelsT[3], "POWER_LABEL_4", "Port 4", PowerControlS[3].label);
    IUFillTextVector(&PowerControlsLabelsTP, PowerControlsLabelsT, 4, getDeviceName(), "POWER_CONTROL_LABEL", "Power Labels",
                     POWER_TAB, IP_WO, 60, IPS_IDLE);

    // Current Draw
    IUFillNumber(&PowerCurrentN[0], "POWER_CURRENT_1", PowerControlS[0].label, "%4.2f A", 0, 1000, 0, 0);
    IUFillNumber(&PowerCurrentN[1], "POWER_CURRENT_2", PowerControlS[1].label, "%4.2f A", 0, 1000, 0, 0);
    IUFillNumber(&PowerCurrentN[2], "POWER_CURRENT_3", PowerControlS[2].label, "%4.2f A", 0, 1000, 0, 0);
    IUFillNumber(&PowerCurrentN[3], "POWER_CURRENT_4", PowerControlS[3].label, "%4.2f A", 0, 1000, 0, 0);
    IUFillNumberVector(&PowerCurrentNP, PowerCurrentN, 4, getDeviceName(), "POWER_CURRENT", "Current Draw", POWER_TAB, IP_RO,
                       60, IPS_IDLE);

    // Power on Boot
    IUFillSwitch(&PowerOnBootS[0], "POWER_PORT_1", PowerControlS[0].label, ISS_ON);
    IUFillSwitch(&PowerOnBootS[1], "POWER_PORT_2", PowerControlS[1].label, ISS_ON);
    IUFillSwitch(&PowerOnBootS[2], "POWER_PORT_3", PowerControlS[2].label, ISS_ON);
    IUFillSwitch(&PowerOnBootS[3], "POWER_PORT_4", PowerControlS[3].label, ISS_ON);
    IUFillSwitchVector(&PowerOnBootSP, PowerOnBootS, 4, getDeviceName(), "POWER_ON_BOOT", "Power On Boot", POWER_TAB, IP_RW,
                       ISR_NOFMANY, 60, IPS_IDLE);

    // Over Current
    IUFillLight(&OverCurrentL[0], "POWER_PORT_1", PowerControlS[0].label, IPS_OK);
    IUFillLight(&OverCurrentL[1], "POWER_PORT_2", PowerControlS[1].label, IPS_OK);
    IUFillLight(&OverCurrentL[2], "POWER_PORT_3", PowerControlS[2].label, IPS_OK);
    IUFillLight(&OverCurrentL[3], "POWER_PORT_4", PowerControlS[3].label, IPS_OK);

    char tempLabel[MAXINDILABEL + 5];
    memset(tempLabel, 0, MAXINDILABEL + 5);
    sprintf(tempLabel, "%s %s", "Dew:", AutoDewV2S[0].label);
    IUFillLight(&OverCurrentL[4], "DEW_A", tempLabel, IPS_OK);
    memset(tempLabel, 0, MAXINDILABEL);
    sprintf(tempLabel, "%s %s", "Dew:", AutoDewV2S[1].label);
    IUFillLight(&OverCurrentL[5], "DEW_B", tempLabel, IPS_OK);
    memset(tempLabel, 0, MAXINDILABEL);
    sprintf(tempLabel, "%s %s", "Dew:", AutoDewV2S[2].label);
    IUFillLight(&OverCurrentL[6], "DEW_C", tempLabel, IPS_OK);
    IUFillLightVector(&OverCurrentLP, OverCurrentL, 7, getDeviceName(), "POWER_OVER_CURRENT", "Over Current", POWER_TAB,
                      IPS_IDLE);

    // Power LED
    IUFillSwitch(&PowerLEDS[POWER_LED_ON], "POWER_LED_ON", "On", ISS_ON);
    IUFillSwitch(&PowerLEDS[POWER_LED_OFF], "POWER_LED_OFF", "Off", ISS_OFF);
    IUFillSwitchVector(&PowerLEDSP, PowerLEDS, 2, getDeviceName(), "POWER_LED", "LED", POWER_TAB, IP_RW, ISR_1OFMANY, 60,
                       IPS_IDLE);

    IUFillNumber(&AdjustableOutputN[0], "ADJUSTABLE_VOLTAGE_VALUE", "Voltage (V)", "%.f", 3, 12, 1, 12);
    IUFillNumberVector(&AdjustableOutputNP, AdjustableOutputN, 1, getDeviceName(), "ADJUSTABLE_VOLTAGE", "Adj. Output",
                       POWER_TAB, IP_RW, 60, IPS_IDLE);


    ////////////////////////////////////////////////////////////////////////////
    /// Dew Group
    ////////////////////////////////////////////////////////////////////////////

    // Automatic Dew v1
    IUFillSwitch(&AutoDewS[INDI_ENABLED], "INDI_ENABLED", "Enabled", ISS_OFF);
    IUFillSwitch(&AutoDewS[INDI_DISABLED], "INDI_DISABLED", "Disabled", ISS_ON);
    IUFillSwitchVector(&AutoDewSP, AutoDewS, 2, getDeviceName(), "AUTO_DEW", "Auto Dew", DEW_TAB, IP_RW, ISR_1OFMANY, 60,
                       IPS_IDLE);

    // Automatic Dew Aggressiveness v2
    IUFillNumber(&AutoDewAggN[AUTO_DEW_AGG], "AUTO_DEW_AGG_VALUE", "Auto Dew Agg (50-250)", "%.2f", 50, 250, 20, 0);
    IUFillNumberVector(&AutoDewAggNP, AutoDewAggN, 1, getDeviceName(), "AUTO_DEW_AGG", "Auto Dew Agg", DEW_TAB, IP_RW, 60,
                       IPS_IDLE);

    // Dew PWM
    IUFillNumber(&DewPWMN[DEW_PWM_A], "DEW_A", AutoDewV2S[0].label, "%.2f %%", 0, 100, 10, 0);
    IUFillNumber(&DewPWMN[DEW_PWM_B], "DEW_B", AutoDewV2S[1].label, "%.2f %%", 0, 100, 10, 0);
    IUFillNumber(&DewPWMN[DEW_PWM_C], "DEW_C", AutoDewV2S[2].label, "%.2f %%", 0, 100, 10, 0);
    IUFillNumberVector(&DewPWMNP, DewPWMN, 3, getDeviceName(), "DEW_PWM", "Dew PWM", DEW_TAB, IP_RW, 60, IPS_IDLE);

    // Dew current draw
    IUFillNumber(&DewCurrentDrawN[DEW_PWM_A], "DEW_CURRENT_A", AutoDewV2S[0].label, "%4.2f A", 0, 1000, 10, 0);
    IUFillNumber(&DewCurrentDrawN[DEW_PWM_B], "DEW_CURRENT_B", AutoDewV2S[1].label, "%4.2f A", 0, 1000, 10, 0);
    IUFillNumber(&DewCurrentDrawN[DEW_PWM_C], "DEW_CURRENT_C", AutoDewV2S[2].label, "%4.2f A", 0, 1000, 10, 0);
    IUFillNumberVector(&DewCurrentDrawNP, DewCurrentDrawN, 3, getDeviceName(), "DEW_CURRENT", "Dew Current", DEW_TAB, IP_RO, 60,
                       IPS_IDLE);

    ////////////////////////////////////////////////////////////////////////////
    /// USB Group
    ////////////////////////////////////////////////////////////////////////////

    // USB Hub control v1
    IUFillSwitch(&USBControlS[INDI_ENABLED], "INDI_ENABLED", "Enabled", ISS_ON);
    IUFillSwitch(&USBControlS[INDI_DISABLED], "INDI_DISABLED", "Disabled", ISS_OFF);
    IUFillSwitchVector(&USBControlSP, USBControlS, 2, getDeviceName(), "USB_HUB_CONTROL", "Hub", USB_TAB, IP_RW, ISR_1OFMANY,
                       60, IPS_IDLE);

    // USB Labels
    IUFillText(&USBControlsLabelsT[0], "USB_LABEL_1", "USB3 Port1", "USB3 Port1");
    IUFillText(&USBControlsLabelsT[1], "USB_LABEL_2", "USB3 Port2", "USB3 Port2");
    IUFillText(&USBControlsLabelsT[2], "USB_LABEL_3", "USB3 Port3", "USB3 Port3");
    IUFillText(&USBControlsLabelsT[3], "USB_LABEL_4", "USB3 Port4", "USB3 Port4");
    IUFillText(&USBControlsLabelsT[4], "USB_LABEL_5", "USB2 Port5", "USB2 Port5");
    IUFillText(&USBControlsLabelsT[5], "USB_LABEL_6", "USB2 Port6", "USB2 Port6");

    IUFillTextVector(&USBControlsLabelsTP, USBControlsLabelsT, 6, getDeviceName(), "USB_CONTROL_LABEL", "USB Labels",
                     USB_TAB, IP_WO, 60, IPS_IDLE);

    // USB Hub control v2

    char USBLabel[MAXINDILABEL];

    // Turn on/off power and power boot up
    memset(USBLabel, 0, MAXINDILABEL);
    int USBRC = IUGetConfigText(getDeviceName(), USBControlsLabelsTP.name, USBControlsLabelsT[0].name, USBLabel,
                                MAXINDILABEL);
    IUFillSwitch(&USBControlV2S[0], "PORT_1", USBRC == -1 ? "USB3 Port1" : USBLabel, ISS_ON);
    memset(USBLabel, 0, MAXINDILABEL);
    USBRC = IUGetConfigText(getDeviceName(), USBControlsLabelsTP.name, USBControlsLabelsT[1].name, USBLabel,
                            MAXINDILABEL);
    IUFillSwitch(&USBControlV2S[1], "PORT_2", USBRC == -1 ? "USB3 Port2" : USBLabel, ISS_ON);
    memset(USBLabel, 0, MAXINDILABEL);
    USBRC = IUGetConfigText(getDeviceName(), USBControlsLabelsTP.name, USBControlsLabelsT[2].name, USBLabel,
                            MAXINDILABEL);
    IUFillSwitch(&USBControlV2S[2], "PORT_3", USBRC == -1 ? "USB3 Port3" : USBLabel, ISS_ON);
    memset(USBLabel, 0, MAXINDILABEL);
    USBRC = IUGetConfigText(getDeviceName(), USBControlsLabelsTP.name, USBControlsLabelsT[3].name, USBLabel,
                            MAXINDILABEL);
    IUFillSwitch(&USBControlV2S[3], "PORT_4", USBRC == -1 ? "USB3 Port4" : USBLabel, ISS_ON);
    memset(USBLabel, 0, MAXINDILABEL);
    USBRC = IUGetConfigText(getDeviceName(), USBControlsLabelsTP.name, USBControlsLabelsT[4].name, USBLabel,
                            MAXINDILABEL);
    IUFillSwitch(&USBControlV2S[4], "PORT_5", USBRC == -1 ? "USB2 Port5" : USBLabel, ISS_ON);
    memset(USBLabel, 0, MAXINDILABEL);
    USBRC = IUGetConfigText(getDeviceName(), USBControlsLabelsTP.name, USBControlsLabelsT[5].name, USBLabel,
                            MAXINDILABEL);
    IUFillSwitch(&USBControlV2S[5], "PORT_6", USBRC == -1 ? "USB2 Port6" : USBLabel, ISS_ON);

    IUFillSwitchVector(&USBControlV2SP, USBControlV2S, 6, getDeviceName(), "USB_PORT_CONTROL", "Ports", USB_TAB, IP_RW,
                       ISR_NOFMANY, 60, IPS_IDLE);

    // USB Labels update with custom values
    IUFillText(&USBControlsLabelsT[0], "USB_LABEL_1", "USB3 Port1", USBControlV2S[0].label);
    IUFillText(&USBControlsLabelsT[1], "USB_LABEL_2", "USB3 Port2", USBControlV2S[1].label);
    IUFillText(&USBControlsLabelsT[2], "USB_LABEL_3", "USB3 Port3", USBControlV2S[2].label);
    IUFillText(&USBControlsLabelsT[3], "USB_LABEL_4", "USB3 Port4", USBControlV2S[3].label);
    IUFillText(&USBControlsLabelsT[4], "USB_LABEL_5", "USB2 Port5", USBControlV2S[4].label);
    IUFillText(&USBControlsLabelsT[5], "USB_LABEL_6", "USB2 Port6", USBControlV2S[5].label);

    IUFillTextVector(&USBControlsLabelsTP, USBControlsLabelsT, 6, getDeviceName(), "USB_CONTROL_LABEL", "USB Labels",
                     USB_TAB, IP_WO, 60, IPS_IDLE);
    // USB Hub Status
    IUFillLight(&USBStatusL[0], "PORT_1", USBControlV2S[0].label, IPS_OK);
    IUFillLight(&USBStatusL[1], "PORT_2", USBControlV2S[1].label, IPS_OK);
    IUFillLight(&USBStatusL[2], "PORT_3", USBControlV2S[2].label, IPS_OK);
    IUFillLight(&USBStatusL[3], "PORT_4", USBControlV2S[3].label, IPS_OK);
    IUFillLight(&USBStatusL[4], "PORT_5", USBControlV2S[4].label, IPS_OK);
    IUFillLight(&USBStatusL[5], "PORT_6", USBControlV2S[5].label, IPS_OK);
    IUFillLightVector(&USBStatusLP, USBStatusL, 6, getDeviceName(), "USB_PORT_STATUS", "Status", USB_TAB, IPS_IDLE);

    ////////////////////////////////////////////////////////////////////////////
    /// Focuser Group
    ////////////////////////////////////////////////////////////////////////////

    // Settings
    //    IUFillNumber(&FocusBacklashN[0], "SETTING_BACKLASH", "Backlash (steps)", "%.f", 0, 999, 100, 0);
    IUFillNumber(&FocuserSettingsN[SETTING_MAX_SPEED], "SETTING_MAX_SPEED", "Max Speed (%)", "%.f", 0, 900, 100, 400);
    IUFillNumberVector(&FocuserSettingsNP, FocuserSettingsN, 1, getDeviceName(), "FOCUSER_SETTINGS", "Settings", FOCUS_TAB,
                       IP_RW, 60, IPS_IDLE);

    // Backlash
    //    IUFillSwitch(&FocusBacklashS[INDI_ENABLED], "INDI_ENABLED", "Enabled", ISS_OFF);
    //    IUFillSwitch(&FocusBacklashS[INDI_DISABLED], "INDI_DISABLED", "Disabled", ISS_ON);
    //    IUFillSwitchVector(&FocusBacklashSP, FocusBacklashS, 2, getDeviceName(), "FOCUSER_BACKLASH", "Backlash", FOCUS_TAB, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);

    // Temperature
    IUFillNumber(&FocuserTemperatureN[0], "FOCUSER_TEMPERATURE_VALUE", "Value (C)", "%4.2f", -50, 85, 1, 0);
    IUFillNumberVector(&FocuserTemperatureNP, FocuserTemperatureN, 1, getDeviceName(), "FOCUSER_TEMPERATURE", "Temperature",
                       FOCUS_TAB, IP_RO, 60, IPS_IDLE);

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
        defineProperty(&PowerCycleAllSP);
        defineProperty(&PowerSensorsNP);
        defineProperty(&PowerConsumptionNP);
        defineProperty(&RebootSP);

        // Power
        defineProperty(&PowerControlSP);
        defineProperty(&PowerControlsLabelsTP);
        defineProperty(&PowerCurrentNP);
        defineProperty(&PowerOnBootSP);
        OverCurrentLP.nlp = (version == UPB_V1) ? 4 : 7;
        defineProperty(&OverCurrentLP);
        if (version == UPB_V1)
            defineProperty(&PowerLEDSP);
        if (version == UPB_V2)
            defineProperty(&AdjustableOutputNP);

        // Dew
        if (version == UPB_V1)
            defineProperty(&AutoDewSP);
        else
            defineProperty(&AutoDewV2SP);

        DewControlsLabelsTP.ntp = (version == UPB_V1) ? 2 : 3;
        defineProperty(&DewControlsLabelsTP);

        if (version == UPB_V2)
            defineProperty(&AutoDewAggNP);

        DewPWMNP.nnp = (version == UPB_V1) ? 2 : 3;
        defineProperty(&DewPWMNP);

        DewCurrentDrawNP.nnp = (version == UPB_V1) ? 2 : 3;
        defineProperty(&DewCurrentDrawNP);

        // USB
        defineProperty(&USBControlSP);
        if (version == UPB_V2)
            defineProperty(&USBControlV2SP);
        if (version == UPB_V1)
            defineProperty(&USBStatusLP);
        defineProperty(&USBControlsLabelsTP);

        // Focuser
        FI::updateProperties();
        defineProperty(&FocuserSettingsNP);
        //defineProperty(&FocusBacklashSP);
        defineProperty(&FocuserTemperatureNP);

        WI::updateProperties();

        // Firmware
        defineProperty(&FirmwareTP);

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
        if (version == UPB_V1)
            deleteProperty(PowerLEDSP.name);
        if (version == UPB_V2)
            deleteProperty(AdjustableOutputNP.name);

        // Dew
        if (version == UPB_V1)
            deleteProperty(AutoDewSP.name);
        else
        {
            deleteProperty(AutoDewV2SP.name);
            deleteProperty(DewControlsLabelsTP.name);
            deleteProperty(AutoDewAggNP.name);
        }

        deleteProperty(DewPWMNP.name);
        deleteProperty(DewCurrentDrawNP.name);

        // USB
        deleteProperty(USBControlSP.name);
        if (version == UPB_V2)
            deleteProperty(USBControlV2SP.name);
        if (version == UPB_V1)
            deleteProperty(USBStatusLP.name);
        deleteProperty(USBControlsLabelsTP.name);

        // Focuser
        FI::updateProperties();
        deleteProperty(FocuserSettingsNP.name);
        deleteProperty(FocuserTemperatureNP.name);

        WI::updateProperties();

        deleteProperty(FirmwareTP.name);

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

    return true;
}

//////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////
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

        // Auto Dew v1.
        if ((!strcmp(name, AutoDewSP.name)) && (version == UPB_V1))
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

        // Auto Dew v2.
        if ((!strcmp(name, AutoDewV2SP.name)) && (version == UPB_V2))
        {
            ISState Dew1 = AutoDewV2S[DEW_PWM_A].s;
            ISState Dew2 = AutoDewV2S[DEW_PWM_B].s;
            ISState Dew3 = AutoDewV2S[DEW_PWM_C].s;
            IUUpdateSwitch(&AutoDewV2SP, states, names, n);
            if (toggleAutoDewV2())
            {
                Dew1 = AutoDewV2S[DEW_PWM_A].s;
                Dew2 = AutoDewV2S[DEW_PWM_B].s;
                Dew3 = AutoDewV2S[DEW_PWM_C].s;
                if (Dew1 == ISS_OFF && Dew2 == ISS_OFF && Dew3 == ISS_OFF)
                    AutoDewV2SP.s = IPS_IDLE;
                else
                    AutoDewV2SP.s = IPS_OK;
            }
            else
            {
                IUResetSwitch(&AutoDewV2SP);
                AutoDewV2S[DEW_PWM_A].s = Dew1;
                AutoDewV2S[DEW_PWM_B].s = Dew2;
                AutoDewV2S[DEW_PWM_C].s = Dew3;
                AutoDewV2SP.s = IPS_ALERT;
            }

            IDSetSwitch(&AutoDewV2SP, nullptr);
            return true;
        }

        // USB Hub Control v1
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

        // USB Hub Control v2
        if (!strcmp(name, USBControlV2SP.name))
        {
            bool rc[6] = {true};
            ISState ports[6] = {ISS_ON};

            for (int i = 0; i < USBControlV2SP.nsp; i++)
                ports[i] = USBControlV2S[i].s;

            IUUpdateSwitch(&USBControlV2SP, states, names, n);
            for (int i = 0; i < USBControlV2SP.nsp; i++)
            {
                if (ports[i] != USBControlV2S[i].s)
                    rc[i] = setUSBPortEnabled(i, USBControlV2S[i].s == ISS_ON);
            }

            // All is OK
            if (rc[0] && rc[1] && rc[2] && rc[3] && rc[4] && rc[5])
            {
                USBControlSP.s = IPS_OK;
            }
            else
            {
                IUResetSwitch(&USBControlV2SP);
                for (int i = 0; i < USBControlV2SP.nsp; i++)
                    USBControlV2S[i].s = ports[i];
                USBControlV2SP.s = IPS_ALERT;
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
        if (!strcmp(name, PowerLEDSP.name) && (version == UPB_V1))
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

//////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////
bool PegasusUPB::ISNewNumber(const char * dev, const char * name, double values[], char * names[], int n)
{
    if (dev && !strcmp(dev, getDeviceName()))
    {
        // Adjustable output
        if (!strcmp(name, AdjustableOutputNP.name))
        {
            if (setAdjustableOutput(static_cast<uint8_t>(values[0])))
            {
                IUUpdateNumber(&AdjustableOutputNP, values, names, n);
                AdjustableOutputNP.s = IPS_OK;
            }
            else
                AdjustableOutputNP.s = IPS_ALERT;

            IDSetNumber(&AdjustableOutputNP, nullptr);
            return true;
        }

        // Dew PWM
        if (!strcmp(name, DewPWMNP.name))
        {
            bool rc1 = false, rc2 = false, rc3 = false;
            for (int i = 0; i < n; i++)
            {
                if (!strcmp(names[i], DewPWMN[DEW_PWM_A].name))
                    rc1 = setDewPWM(5, static_cast<uint8_t>(values[i] / 100.0 * 255.0));
                else if (!strcmp(names[i], DewPWMN[DEW_PWM_B].name))
                    rc2 = setDewPWM(6, static_cast<uint8_t>(values[i] / 100.0 * 255.0));
                else if (!strcmp(names[i], DewPWMN[DEW_PWM_C].name))
                    rc3 = setDewPWM(7, static_cast<uint8_t>(values[i] / 100.0 * 255.0));
            }

            DewPWMNP.s = (rc1 && rc2 && rc3) ? IPS_OK : IPS_ALERT;
            if (DewPWMNP.s == IPS_OK)
                IUUpdateNumber(&DewPWMNP, values, names, n);
            IDSetNumber(&DewPWMNP, nullptr);
            return true;
        }

        // Auto Dew Aggressiveness
        if (!strcmp(name, AutoDewAggNP.name))
        {
            if (setAutoDewAgg(values[0]))
            {
                AutoDewAggN[0].value = values[0];
                AutoDewAggNP.s = IPS_OK;
            }
            else
            {
                AutoDewAggNP.s = IPS_ALERT;
            }

            IDSetNumber(&AutoDewAggNP, nullptr);
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

//////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////
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
        // Dew Labels
        if (!strcmp(name, DewControlsLabelsTP.name))
        {
            IUUpdateText(&DewControlsLabelsTP, texts, names, n);
            DewControlsLabelsTP.s = IPS_OK;
            LOG_INFO("Dew labels saved. Driver must be restarted for the labels to take effect.");
            saveConfig();
            IDSetText(&DewControlsLabelsTP, nullptr);
            return true;
        }
        // USB Labels
        if (!strcmp(name, USBControlsLabelsTP.name))
        {
            IUUpdateText(&USBControlsLabelsTP, texts, names, n);
            USBControlsLabelsTP.s = IPS_OK;
            LOG_INFO("USB labels saved. Driver must be restarted for the labels to take effect.");
            saveConfig();
            IDSetText(&USBControlsLabelsTP, nullptr);
            return true;
        }
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
    return MoveAbsFocuser(dir == FOCUS_INWARD ? FocusAbsPosN[0].value - ticks : FocusAbsPosN[0].value + ticks);
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
        PowerOnBootS[0].s = (status[0] == '1') ? ISS_ON : ISS_OFF;
        PowerOnBootS[1].s = (status[1] == '1') ? ISS_ON : ISS_OFF;
        PowerOnBootS[2].s = (status[2] == '1') ? ISS_ON : ISS_OFF;
        PowerOnBootS[3].s = (status[3] == '1') ? ISS_ON : ISS_OFF;

        AdjustableOutputN[0].value = std::stod(result[2]);
        AdjustableOutputNP.s = IPS_OK;

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
bool PegasusUPB::toggleAutoDewV2()
{
    char cmd[PEGASUS_LEN] = {0}, expected[PEGASUS_LEN] = {0}, res[PEGASUS_LEN] = {0};

    uint8_t value = 0;

    if (IUFindOnSwitchIndex(&AutoDewV2SP) == -1)
        value = 0;
    else if (AutoDewV2S[DEW_PWM_A].s == ISS_ON && AutoDewV2S[DEW_PWM_B].s == ISS_ON && AutoDewV2S[DEW_PWM_C].s == ISS_ON)
        value = 1;
    else if (AutoDewV2S[DEW_PWM_A].s == ISS_ON && AutoDewV2S[DEW_PWM_B].s == ISS_ON)
        value = 5;
    else if (AutoDewV2S[DEW_PWM_A].s == ISS_ON && AutoDewV2S[DEW_PWM_C].s == ISS_ON)
        value = 6;
    else if (AutoDewV2S[DEW_PWM_B].s == ISS_ON && AutoDewV2S[DEW_PWM_C].s == ISS_ON)
        value = 7;
    else if (AutoDewV2S[DEW_PWM_A].s == ISS_ON)
        value = 2;
    else if (AutoDewV2S[DEW_PWM_B].s == ISS_ON)
        value = 3;
    else if (AutoDewV2S[DEW_PWM_C].s == ISS_ON)
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

    IUSaveConfigSwitch(fp, &PowerLEDSP);
    IUSaveConfigSwitch(fp, &AutoDewSP);
    if (version == UPB_V2)
        IUSaveConfigNumber(fp, &AutoDewAggNP);
    IUSaveConfigNumber(fp, &FocuserSettingsNP);
    IUSaveConfigText(fp, &PowerControlsLabelsTP);
    IUSaveConfigText(fp, &DewControlsLabelsTP);
    IUSaveConfigText(fp, &USBControlsLabelsTP);
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
        IUSaveText(&FirmwareT[FIRMWARE_VERSION], res);
        IDSetText(&FirmwareTP, nullptr);
        return true;
    }

    return false;
}

//////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////
bool PegasusUPB::sensorUpdated(const std::vector<std::string> &result, uint8_t start, uint8_t end)
{
    for (uint8_t index = start; index <= end; index++)
        if (result[index] != lastSensorData[index])
            return true;

    return false;
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
        PowerSensorsN[SENSOR_VOLTAGE].value = std::stod(result[1]);
        PowerSensorsN[SENSOR_CURRENT].value = std::stod(result[2]);
        PowerSensorsN[SENSOR_POWER].value = std::stod(result[3]);
        PowerSensorsNP.s = IPS_OK;
        //if (lastSensorData[0] != result[0] || lastSensorData[1] != result[1] || lastSensorData[2] != result[2])
        if (sensorUpdated(result, 0, 2))
            IDSetNumber(&PowerSensorsNP, nullptr);

        // Environment Sensors
        setParameterValue("WEATHER_TEMPERATURE", std::stod(result[4]));
        setParameterValue("WEATHER_HUMIDITY", std::stod(result[5]));
        setParameterValue("WEATHER_DEWPOINT", std::stod(result[6]));
        //if (lastSensorData[4] != result[4] || lastSensorData[5] != result[5] || lastSensorData[6] != result[6])
        if (sensorUpdated(result, 4, 6))
        {
            if (WI::syncCriticalParameters())
                IDSetLight(&critialParametersLP, nullptr);
            ParametersNP.s = IPS_OK;
            IDSetNumber(&ParametersNP, nullptr);
        }

        // Port Status
        const char * portStatus = result[7].c_str();
        PowerControlS[0].s = (portStatus[0] == '1') ? ISS_ON : ISS_OFF;
        PowerControlS[1].s = (portStatus[1] == '1') ? ISS_ON : ISS_OFF;
        PowerControlS[2].s = (portStatus[2] == '1') ? ISS_ON : ISS_OFF;
        PowerControlS[3].s = (portStatus[3] == '1') ? ISS_ON : ISS_OFF;
        //if (lastSensorData[7] != result[7])
        if (sensorUpdated(result, 7, 7))
            IDSetSwitch(&PowerControlSP, nullptr);

        // Hub Status
        const char * usb_status = result[8].c_str();
        if (version == UPB_V1)
        {
            USBControlS[0].s = (usb_status[0] == '0') ? ISS_ON : ISS_OFF;
            USBControlS[1].s = (usb_status[0] == '0') ? ISS_OFF : ISS_ON;
            USBStatusL[0].s = (USBControlS[0].s == ISS_ON) ? IPS_OK : IPS_IDLE;
            USBStatusL[1].s = (USBControlS[0].s == ISS_ON) ? IPS_OK : IPS_IDLE;
            USBStatusL[2].s = (USBControlS[0].s == ISS_ON) ? IPS_OK : IPS_IDLE;
            USBStatusL[3].s = (USBControlS[0].s == ISS_ON) ? IPS_OK : IPS_IDLE;
            USBStatusL[4].s = (USBControlS[0].s == ISS_ON) ? IPS_OK : IPS_IDLE;
            //if (lastSensorData[8] != result[8])
            if (sensorUpdated(result, 8, 8))
            {
                USBControlSP.s = (IUFindOnSwitchIndex(&USBControlSP) == 0) ? IPS_OK : IPS_IDLE;
                IDSetSwitch(&USBControlSP, nullptr);
                IDSetLight(&USBStatusLP, nullptr);
            }
        }
        else
        {
            USBControlV2S[0].s = (usb_status[0] == '1') ? ISS_ON : ISS_OFF;
            USBControlV2S[1].s = (usb_status[1] == '1') ? ISS_ON : ISS_OFF;
            USBControlV2S[2].s = (usb_status[2] == '1') ? ISS_ON : ISS_OFF;
            USBControlV2S[3].s = (usb_status[3] == '1') ? ISS_ON : ISS_OFF;
            USBControlV2S[4].s = (usb_status[4] == '1') ? ISS_ON : ISS_OFF;
            USBControlV2S[5].s = (usb_status[5] == '1') ? ISS_ON : ISS_OFF;
            USBControlV2SP.s = IPS_OK;
            //if (lastSensorData[8] != result[8])
            if (sensorUpdated(result, 8, 8))
            {
                IDSetSwitch(&USBControlV2SP, nullptr);
            }
        }

        // From here, we get differences between v1 and v2 readings
        int index = 9;
        // Dew PWM
        DewPWMN[DEW_PWM_A].value = std::stod(result[index]) / 255.0 * 100.0;
        DewPWMN[DEW_PWM_B].value = std::stod(result[index + 1]) / 255.0 * 100.0;
        if (version == UPB_V2)
            DewPWMN[DEW_PWM_C].value = std::stod(result[index + 2]) / 255.0 * 100.0;
        //        if (lastSensorData[index] != result[index] ||
        //                lastSensorData[index + 1] != result[index + 1] ||
        //                (version == UPB_V2 && lastSensorData[index +2] != result[index + 2]))
        if (sensorUpdated(result, index, version == UPB_V1 ? index + 1 : index + 2))
            IDSetNumber(&DewPWMNP, nullptr);

        index = (version == UPB_V1) ? 11 : 12;

        const double ampDivision = (version == UPB_V1) ? 400.0 : 300.0;

        // Current draw
        PowerCurrentN[0].value = std::stod(result[index]) / ampDivision;
        PowerCurrentN[1].value = std::stod(result[index + 1]) / ampDivision;
        PowerCurrentN[2].value = std::stod(result[index + 2]) / ampDivision;
        PowerCurrentN[3].value = std::stod(result[index + 3]) / ampDivision;
        //        if (lastSensorData[index] != result[index] ||
        //                lastSensorData[index + 1] != result[index + 1] ||
        //                lastSensorData[index + 2] != result[index + 2] ||
        //                lastSensorData[index + 3] != result[index + 3])
        if (sensorUpdated(result, index, index + 3))
            IDSetNumber(&PowerCurrentNP, nullptr);

        index = (version == UPB_V1) ? 15 : 16;

        DewCurrentDrawN[DEW_PWM_A].value = std::stod(result[index]) / ampDivision;
        DewCurrentDrawN[DEW_PWM_B].value = std::stod(result[index + 1]) / ampDivision;
        if (version == UPB_V2)
            DewCurrentDrawN[DEW_PWM_C].value = std::stod(result[index + 2]) / (ampDivision * 2);
        //        if (lastSensorData[index] != result[index] ||
        //                lastSensorData[index + 1] != result[index + 1] ||
        //                (version == UPB_V2 && lastSensorData[index + 2] != result[index + 2]))
        if (sensorUpdated(result, index, version == UPB_V1 ? index + 1 : index + 2))
            IDSetNumber(&DewCurrentDrawNP, nullptr);

        index = (version == UPB_V1) ? 17 : 19;

        // Over Current
        //if (lastSensorData[index] != result[index])
        if (sensorUpdated(result, index, index))
        {
            const char * over_curent = result[index].c_str();
            OverCurrentL[0].s = (over_curent[0] == '0') ? IPS_OK : IPS_ALERT;
            OverCurrentL[1].s = (over_curent[1] == '0') ? IPS_OK : IPS_ALERT;
            OverCurrentL[2].s = (over_curent[2] == '0') ? IPS_OK : IPS_ALERT;
            OverCurrentL[3].s = (over_curent[3] == '0') ? IPS_OK : IPS_ALERT;
            if (version == UPB_V2)
            {
                OverCurrentL[4].s = (over_curent[4] == '0') ? IPS_OK : IPS_ALERT;
                OverCurrentL[5].s = (over_curent[5] == '0') ? IPS_OK : IPS_ALERT;
                OverCurrentL[6].s = (over_curent[6] == '0') ? IPS_OK : IPS_ALERT;
            }

            IDSetLight(&OverCurrentLP, nullptr);
        }

        index = (version == UPB_V1) ? 18 : 20;

        // Auto Dew
        if (version == UPB_V1)
        {
            //if (lastSensorData[index] != result[index])
            if (sensorUpdated(result, index, index))
            {
                AutoDewS[INDI_ENABLED].s  = (std::stoi(result[index]) == 1) ? ISS_ON : ISS_OFF;
                AutoDewS[INDI_DISABLED].s = (std::stoi(result[index]) == 1) ? ISS_OFF : ISS_ON;
                IDSetSwitch(&AutoDewSP, nullptr);
            }
        }
        else
        {
            //if (lastSensorData[index] != result[index])
            if (sensorUpdated(result, index, index))
            {
                int value = std::stoi(result[index]);
                IUResetSwitch(&AutoDewV2SP);
                switch (value)
                {
                    case 1:
                        AutoDewV2S[DEW_PWM_A].s = AutoDewV2S[DEW_PWM_B].s = AutoDewV2S[DEW_PWM_C].s = ISS_ON;
                        break;

                    case 2:
                        AutoDewV2S[DEW_PWM_A].s = ISS_ON;
                        break;

                    case 3:
                        AutoDewV2S[DEW_PWM_B].s = ISS_ON;
                        break;

                    case 4:
                        AutoDewV2S[DEW_PWM_C].s = ISS_ON;
                        break;

                    case 5:
                        AutoDewV2S[DEW_PWM_A].s = ISS_ON;
                        AutoDewV2S[DEW_PWM_B].s = ISS_ON;
                        break;

                    case 6:
                        AutoDewV2S[DEW_PWM_A].s = ISS_ON;
                        AutoDewV2S[DEW_PWM_C].s = ISS_ON;
                        break;

                    case 7:
                        AutoDewV2S[DEW_PWM_B].s = ISS_ON;
                        AutoDewV2S[DEW_PWM_C].s = ISS_ON;
                        break;
                    default:
                        break;
                }
                IDSetSwitch(&AutoDewV2SP, nullptr);
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
        if ( (version == UPB_V1 && result.size() != 3) ||
                (version == UPB_V2 && result.size() != 4))
        {
            LOGF_WARN("Received wrong number (%i) of power sensor data (%s). Retrying...", result.size(), res);
            return false;
        }

        if (result == lastPowerData)
            return true;

        PowerConsumptionN[CONSUMPTION_AVG_AMPS].value = std::stod(result[0]);
        PowerConsumptionN[CONSUMPTION_AMP_HOURS].value = std::stod(result[1]);
        PowerConsumptionN[CONSUMPTION_WATT_HOURS].value = std::stod(result[2]);
        PowerConsumptionNP.s = IPS_OK;
        IDSetNumber(&PowerConsumptionNP, nullptr);

        if (version == UPB_V2)
        {
            std::chrono::milliseconds uptime(std::stol(result[3]));
            using dhours = std::chrono::duration<double, std::ratio<3600>>;
            std::stringstream ss;
            ss << std::fixed << std::setprecision(3) << dhours(uptime).count();
            IUSaveText(&FirmwareT[FIRMWARE_UPTIME], ss.str().c_str());
            IDSetText(&FirmwareTP, nullptr);
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

        FocusAbsPosN[0].value = std::stoi(result[0]);
        focusMotorRunning = (std::stoi(result[1]) == 1);

        if (FocusAbsPosNP.s == IPS_BUSY && focusMotorRunning == false)
        {
            FocusAbsPosNP.s = IPS_OK;
            FocusRelPosNP.s = IPS_OK;
            IDSetNumber(&FocusAbsPosNP, nullptr);
            IDSetNumber(&FocusRelPosNP, nullptr);
        }
        else if (result[0] != lastStepperData[0])
            IDSetNumber(&FocusAbsPosNP, nullptr);

        FocusReverseS[INDI_ENABLED].s = (std::stoi(result[2]) == 1) ? ISS_ON : ISS_OFF;
        FocusReverseS[INDI_DISABLED].s = (std::stoi(result[2]) == 1) ? ISS_OFF : ISS_ON;

        if (result[2] != lastStepperData[2])
            IDSetSwitch(&FocusReverseSP, nullptr);

        uint16_t backlash = std::stoi(result[3]);
        if (backlash == 0)
        {
            FocusBacklashN[0].value = backlash;
            FocusBacklashS[INDI_ENABLED].s = ISS_OFF;
            FocusBacklashS[INDI_DISABLED].s = ISS_ON;
            if (result[3] != lastStepperData[3])
            {
                IDSetSwitch(&FocusBacklashSP, nullptr);
                IDSetNumber(&FocuserSettingsNP, nullptr);
            }
        }
        else
        {
            FocusBacklashS[INDI_ENABLED].s = ISS_ON;
            FocusBacklashS[INDI_DISABLED].s = ISS_OFF;
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

        AutoDewAggN[0].value = std::stod(result[1]);
        AutoDewAggNP.s = IPS_OK;
        IDSetNumber(&AutoDewAggNP, nullptr);

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
        uint32_t value = std::stol(res);
        if (value == UINT16_MAX)
        {
            LOGF_WARN("Invalid maximum speed detected: %u. Please set maximum speed appropiate for your motor focus type (0-900)",
                      value);
            FocuserSettingsNP.s = IPS_ALERT;
        }
        else
        {
            FocuserSettingsN[SETTING_MAX_SPEED].value = value;
            FocuserSettingsNP.s = IPS_OK;
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
