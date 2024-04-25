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
    setVersion(1, 6);

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
    PowerCycleAllSP[POWER_CYCLE_ON].fill("POWER_CYCLE_ON", "All On", ISS_OFF);
    PowerCycleAllSP[POWER_CYCLE_OFF].fill("POWER_CYCLE_OFF", "All Off", ISS_OFF);
    PowerCycleAllSP.fill(getDeviceName(), "POWER_CYCLE", "Cycle Power", MAIN_CONTROL_TAB,
                       IP_RW, ISR_ATMOST1, 60, IPS_IDLE);

    // Reboot
    RebootSP[0].fill("REBOOT", "Reboot Device", ISS_OFF);
    RebootSP.fill(getDeviceName(), "REBOOT_DEVICE", "Device", MAIN_CONTROL_TAB, IP_RW, ISR_ATMOST1,
                       60, IPS_IDLE);

    // Power Sensors
    PowerSensorsNP[SENSOR_VOLTAGE].fill("SENSOR_VOLTAGE", "Voltage (V)", "%4.2f", 0, 999, 100, 0);
    PowerSensorsNP[SENSOR_CURRENT].fill("SENSOR_CURRENT", "Current (A)", "%4.2f", 0, 999, 100, 0);
    PowerSensorsNP[SENSOR_POWER].fill("SENSOR_POWER", "Power (W)", "%4.2f", 0, 999, 100, 0);
    PowerSensorsNP.fill(getDeviceName(), "POWER_SENSORS", "Sensors", MAIN_CONTROL_TAB, IP_RO,
                       60, IPS_IDLE);

    // Overall Power Consumption
    PowerConsumptionNP[CONSUMPTION_AVG_AMPS].fill("CONSUMPTION_AVG_AMPS", "Avg. Amps", "%4.2f", 0, 999, 100, 0);
    PowerConsumptionNP[CONSUMPTION_AMP_HOURS].fill("CONSUMPTION_AMP_HOURS", "Amp Hours", "%4.2f", 0, 999, 100, 0);
    PowerConsumptionNP[CONSUMPTION_WATT_HOURS].fill("CONSUMPTION_WATT_HOURS", "Watt Hours", "%4.2f", 0, 999, 100, 0);
    PowerConsumptionNP.fill(getDeviceName(), "POWER_CONSUMPTION", "Consumption",
                       MAIN_CONTROL_TAB, IP_RO, 60, IPS_IDLE);

    ////////////////////////////////////////////////////////////////////////////
    /// Power Group
    ////////////////////////////////////////////////////////////////////////////

    // Dew Labels. Need to declare them here to use in the Power usage section
    DewControlsLabelsTP[DEW_LABEL_1].fill("DEW_LABEL_1", "Dew A", "Dew A");
    DewControlsLabelsTP[DEW_LABEL_2].fill("DEW_LABEL_2", "Dew B", "Dew B");
    DewControlsLabelsTP[DEW_LABEL_3].fill("DEW_LABEL_3", "Dew C", "Dew C");
    DewControlsLabelsTP.fill(getDeviceName(), "DEW_CONTROL_LABEL", "Dew Labels",
                     DEW_TAB, IP_WO, 60, IPS_IDLE);

    char dewLabel[MAXINDILABEL];

    // Turn on/off power and power boot up
    memset(dewLabel, 0, MAXINDILABEL);
    int dewRC = IUGetConfigText(getDeviceName(), DewControlsLabelsTP.getName(), DewControlsLabelsTP[DEW_LABEL_1].getName(), dewLabel,
                                MAXINDILABEL);
    IUFillSwitch(&AutoDewV2S[DEW_PWM_A], "DEW_A", dewRC == -1 ? "Dew A" : dewLabel, ISS_OFF);
    memset(dewLabel, 0, MAXINDILABEL);
    dewRC = IUGetConfigText(getDeviceName(), DewControlsLabelsTP.getName(), DewControlsLabelsTP[DEW_LABEL_2].getName(), dewLabel,
                            MAXINDILABEL);
    IUFillSwitch(&AutoDewV2S[DEW_PWM_B], "DEW_B", dewRC == -1 ? "Dew B" : dewLabel, ISS_OFF);
    memset(dewLabel, 0, MAXINDILABEL);
    dewRC = IUGetConfigText(getDeviceName(), DewControlsLabelsTP.getName(), DewControlsLabelsTP[DEW_LABEL_3].getName(), dewLabel,
                            MAXINDILABEL);
    IUFillSwitch(&AutoDewV2S[DEW_PWM_C], "DEW_C", dewRC == -1 ? "Dew C" : dewLabel, ISS_OFF);
    IUFillSwitchVector(&AutoDewV2SP, AutoDewV2S, 3, getDeviceName(), "AUTO_DEW", "Auto Dew", DEW_TAB, IP_RW, ISR_NOFMANY, 60,
                       IPS_IDLE);

    // Dew Labels with custom labels
    DewControlsLabelsTP[DEW_LABEL_1].fill("DEW_LABEL_1", "Dew A", AutoDewV2S[0].label);
    DewControlsLabelsTP[DEW_LABEL_2].fill("DEW_LABEL_2", "Dew B", AutoDewV2S[1].label);
    DewControlsLabelsTP[DEW_LABEL_3].fill("DEW_LABEL_3", "Dew C", AutoDewV2S[2].label);
    DewControlsLabelsTP.fill(getDeviceName(), "DEW_CONTROL_LABEL", "DEW Labels",
                     DEW_TAB, IP_WO, 60, IPS_IDLE);
    // Power Labels
    PowerControlsLabelsTP[POWER_LABEL_1].fill("POWER_LABEL_1", "Port 1", "Port 1");
    PowerControlsLabelsTP[POWER_LABEL_2].fill("POWER_LABEL_2", "Port 2", "Port 2");
    PowerControlsLabelsTP[POWER_LABEL_3].fill("POWER_LABEL_3", "Port 3", "Port 3");
    PowerControlsLabelsTP[POWER_LABEL_4].fill("POWER_LABEL_4", "Port 4", "Port 4");
    PowerControlsLabelsTP.fill(getDeviceName(), "POWER_CONTROL_LABEL", "Power Labels",
                     POWER_TAB, IP_WO, 60, IPS_IDLE);

    char portLabel[MAXINDILABEL];

    // Turn on/off power and power boot up
    memset(portLabel, 0, MAXINDILABEL);
    int portRC = IUGetConfigText(getDeviceName(), PowerControlsLabelsTP.getName(), PowerControlsLabelsTP[POWER_LABEL_1].getName(), portLabel,
                                 MAXINDILABEL);
    PowerControlSP[POWER_CONTROL_1].fill("POWER_CONTROL_1", portRC == -1 ? "Port 1" : portLabel, ISS_OFF);

    memset(portLabel, 0, MAXINDILABEL);
    portRC = IUGetConfigText(getDeviceName(), PowerControlsLabelsTP.getName(), PowerControlsLabelsTP[POWER_LABEL_2].getName(), portLabel,
                             MAXINDILABEL);
    PowerControlSP[POWER_CONTROL_2].fill("POWER_CONTROL_2", portRC == -1 ? "Port 2" : portLabel, ISS_OFF);

    memset(portLabel, 0, MAXINDILABEL);
    portRC = IUGetConfigText(getDeviceName(), PowerControlsLabelsTP.getName(), PowerControlsLabelsTP[POWER_LABEL_3].getName(), portLabel,
                             MAXINDILABEL);
    PowerControlSP[POWER_CONTROL_3].fill("POWER_CONTROL_3", portRC == -1 ? "Port 3" : portLabel, ISS_OFF);

    memset(portLabel, 0, MAXINDILABEL);
    portRC = IUGetConfigText(getDeviceName(), PowerControlsLabelsTP.getName(), PowerControlsLabelsTP[POWER_LABEL_4].getName(), portLabel,
                             MAXINDILABEL);
   PowerControlSP[POWER_CONTROL_4].fill("POWER_CONTROL_4", portRC == -1 ? "Port 4" : portLabel, ISS_OFF);

    PowerControlSP.fill(getDeviceName(), "POWER_CONTROL", "Power Control", POWER_TAB, IP_RW,
                       ISR_NOFMANY, 60, IPS_IDLE);

    // Power Labels
    PowerControlsLabelsTP[POWER_LABEL_1].fill("POWER_LABEL_1", "Port 1", PowerControlSP[POWER_CONTROL_1].getLabel());
    PowerControlsLabelsTP[POWER_LABEL_2].fill("POWER_LABEL_2", "Port 2", PowerControlSP[POWER_CONTROL_2].getLabel());
    PowerControlsLabelsTP[POWER_LABEL_3].fill( "POWER_LABEL_3", "Port 3", PowerControlSP[POWER_CONTROL_3].getLabel());
    PowerControlsLabelsTP[POWER_LABEL_4].fill( "POWER_LABEL_4", "Port 4", PowerControlSP[POWER_CONTROL_4].getLabel());
    PowerControlsLabelsTP.fill(getDeviceName(), "POWER_CONTROL_LABEL", "Power Labels",
                     POWER_TAB, IP_WO, 60, IPS_IDLE);

    // Current Draw
    PowerCurrentNP[POWER_CURRENT_1].fill("POWER_CURRENT_1", PowerControlSP[POWER_CONTROL_1].getLabel(), "%4.2f A", 0, 1000, 0, 0);
    PowerCurrentNP[POWER_CURRENT_2].fill("POWER_CURRENT_2", PowerControlSP[POWER_CONTROL_2].getLabel(), "%4.2f A", 0, 1000, 0, 0);
    PowerCurrentNP[POWER_CURRENT_3].fill("POWER_CURRENT_3", PowerControlSP[POWER_CONTROL_3].getLabel(), "%4.2f A", 0, 1000, 0, 0);
    PowerCurrentNP[POWER_CURRENT_4].fill("POWER_CURRENT_4", PowerControlSP[POWER_CONTROL_4].getLabel(), "%4.2f A", 0, 1000, 0, 0);
    PowerCurrentNP.fill(getDeviceName(), "POWER_CURRENT", "Current Draw", POWER_TAB, IP_RO,
                       60, IPS_IDLE);

    // Power on Boot
    PowerOnBootSP[POWER_PORT_1].fill("POWER_PORT_1", PowerControlSP[POWER_CONTROL_1].getLabel(), ISS_ON);
    PowerOnBootSP[POWER_PORT_2].fill("POWER_PORT_2", PowerControlSP[POWER_CONTROL_2].getLabel(), ISS_ON);
    PowerOnBootSP[POWER_PORT_3].fill("POWER_PORT_3", PowerControlSP[POWER_CONTROL_3].getLabel(), ISS_ON);
    PowerOnBootSP[POWER_PORT_4].fill("POWER_PORT_4", PowerControlSP[POWER_CONTROL_4].getLabel(), ISS_ON);
    PowerOnBootSP.fill(getDeviceName(), "POWER_ON_BOOT", "Power On Boot", POWER_TAB, IP_RW,
                       ISR_NOFMANY, 60, IPS_IDLE);

    // Over Current
    OverCurrentLP[POWER_PORT_1].fill("POWER_PORT_1", PowerControlSP[POWER_CONTROL_1].getLabel(), IPS_OK);
    OverCurrentLP[POWER_PORT_2].fill("POWER_PORT_2", PowerControlSP[POWER_CONTROL_2].getLabel(), IPS_OK);
    OverCurrentLP[POWER_PORT_3].fill("POWER_PORT_3", PowerControlSP[POWER_CONTROL_3].getLabel(), IPS_OK);
    OverCurrentLP[POWER_PORT_4].fill("POWER_PORT_4", PowerControlSP[POWER_CONTROL_4].getLabel(), IPS_OK);

    char tempLabel[MAXINDILABEL + 5];
    memset(tempLabel, 0, MAXINDILABEL + 5);
    sprintf(tempLabel, "%s %s", "Dew:", AutoDewV2S[0].label);
    OverCurrentLP[DEW_A].fill("DEW_A", tempLabel, IPS_OK);
    memset(tempLabel, 0, MAXINDILABEL);
    sprintf(tempLabel, "%s %s", "Dew:", AutoDewV2S[1].label);
    OverCurrentLP[DEW_B].fill("DEW_B", tempLabel, IPS_OK);
    memset(tempLabel, 0, MAXINDILABEL);
    sprintf(tempLabel, "%s %s", "Dew:", AutoDewV2S[2].label);
    OverCurrentLP[DEW_C].fill("DEW_C", tempLabel, IPS_OK);
    OverCurrentLP.fill(getDeviceName(), "POWER_OVER_CURRENT", "Over Current", POWER_TAB,
                      IPS_IDLE);

    // Power LED
    PowerLEDSP[POWER_LED_ON].fill("POWER_LED_ON", "On", ISS_ON);
    PowerLEDSP[POWER_LED_OFF].fill("POWER_LED_OFF", "Off", ISS_OFF);
    PowerLEDSP.fill(getDeviceName(), "POWER_LED", "LED", POWER_TAB, IP_RW, ISR_1OFMANY, 60,
                       IPS_IDLE);

    AdjustableOutputNP[0].fill("ADJUSTABLE_VOLTAGE_VALUE", "Voltage (V)", "%.f", 3, 12, 1, 12);
    AdjustableOutputNP.fill(getDeviceName(), "ADJUSTABLE_VOLTAGE", "Adj. Output",
                       POWER_TAB, IP_RW, 60, IPS_IDLE);


    ////////////////////////////////////////////////////////////////////////////
    /// Dew Group
    ////////////////////////////////////////////////////////////////////////////

    // Automatic Dew v1
    AutoDewSP[INDI_ENABLED].fill("INDI_ENABLED", "Enabled", ISS_OFF);
    AutoDewSP[INDI_DISABLED].fill("INDI_DISABLED", "Disabled", ISS_ON);
    AutoDewSP.fill(getDeviceName(), "AUTO_DEW", "Auto Dew", DEW_TAB, IP_RW, ISR_1OFMANY, 60,
                       IPS_IDLE);

    // Automatic Dew Aggressiveness v2
    AutoDewAggNP[AUTO_DEW_AGG].fill("AUTO_DEW_AGG_VALUE", "Auto Dew Agg (50-250)", "%.2f", 50, 250, 20, 0);
    AutoDewAggNP.fill(getDeviceName(), "AUTO_DEW_AGG", "Auto Dew Agg", DEW_TAB, IP_RW, 60,
                       IPS_IDLE);

    // Dew PWM
    DewPWMNP[DEW_PWM_A].fill("DEW_A", AutoDewV2S[0].label, "%.2f %%", 0, 100, 10, 0);
    DewPWMNP[DEW_PWM_B].fill("DEW_B", AutoDewV2S[1].label, "%.2f %%", 0, 100, 10, 0);
    DewPWMNP[DEW_PWM_C].fill("DEW_C", AutoDewV2S[2].label, "%.2f %%", 0, 100, 10, 0);
    DewPWMNP.fill(getDeviceName(), "DEW_PWM", "Dew PWM", DEW_TAB, IP_RW, 60, IPS_IDLE);

    // Dew current draw
    DewCurrentDrawNP[DEW_PWM_A].fill("DEW_CURRENT_A", AutoDewV2S[0].label, "%4.2f A", 0, 1000, 10, 0);
    DewCurrentDrawNP[DEW_PWM_B].fill("DEW_CURRENT_B", AutoDewV2S[1].label, "%4.2f A", 0, 1000, 10, 0);
    DewCurrentDrawNP[DEW_PWM_C].fill("DEW_CURRENT_C", AutoDewV2S[2].label, "%4.2f A", 0, 1000, 10, 0);
    DewCurrentDrawNP.fill(getDeviceName(), "DEW_CURRENT", "Dew Current", DEW_TAB, IP_RO, 60,
                       IPS_IDLE);

    ////////////////////////////////////////////////////////////////////////////
    /// USB Group
    ////////////////////////////////////////////////////////////////////////////

    // USB Hub control v1
    USBControlSP[INDI_ENABLED].fill("INDI_ENABLED", "Enabled", ISS_ON);
    USBControlSP[INDI_DISABLED].fill("INDI_DISABLED", "Disabled", ISS_OFF);
    USBControlSP.fill(getDeviceName(), "USB_HUB_CONTROL", "Hub", USB_TAB, IP_RW, ISR_1OFMANY,
                       60, IPS_IDLE);

    // USB Labels
    USBControlsLabelsTP[USB_LABEL_1].fill("USB_LABEL_1", "USB3 Port1", "USB3 Port1");
    USBControlsLabelsTP[USB_LABEL_2].fill("USB_LABEL_2", "USB3 Port2", "USB3 Port2");
    USBControlsLabelsTP[USB_LABEL_3].fill("USB_LABEL_3", "USB3 Port3", "USB3 Port3");
    USBControlsLabelsTP[USB_LABEL_4].fill("USB_LABEL_4", "USB3 Port4", "USB3 Port4");
    USBControlsLabelsTP[USB_LABEL_5].fill("USB_LABEL_5", "USB2 Port5", "USB2 Port5");
    USBControlsLabelsTP[USB_LABEL_6].fill("USB_LABEL_6", "USB2 Port6", "USB2 Port6");

    USBControlsLabelsTP.fill(getDeviceName(), "USB_CONTROL_LABEL", "USB Labels",
                     USB_TAB, IP_WO, 60, IPS_IDLE);

    // USB Hub control v2

    char USBLabel[MAXINDILABEL];

    // Turn on/off power and power boot up
    memset(USBLabel, 0, MAXINDILABEL);
    int USBRC = IUGetConfigText(getDeviceName(), USBControlsLabelsTP.getName(), USBControlsLabelsTP[USB_LABEL_1].name, USBLabel,
                                MAXINDILABEL);
    USBControlV2SP[PORT_1].fill("PORT_1", USBRC == -1 ? "USB3 Port1" : USBLabel, ISS_ON);
    memset(USBLabel, 0, MAXINDILABEL);
    USBRC = IUGetConfigText(getDeviceName(), USBControlsLabelsTP.getName(), USBControlsLabelsTP[USB_LABEL_2].name, USBLabel,
                            MAXINDILABEL);
    USBControlV2SP[PORT_2].fill("PORT_2", USBRC == -1 ? "USB3 Port2" : USBLabel, ISS_ON);
    memset(USBLabel, 0, MAXINDILABEL);
    USBRC = IUGetConfigText(getDeviceName(), USBControlsLabelsTP.getName(), USBControlsLabelsTP[USB_LABEL_3].name, USBLabel,
                            MAXINDILABEL);
    USBControlV2SP[PORT_3].fill("PORT_3", USBRC == -1 ? "USB3 Port3" : USBLabel, ISS_ON);
    memset(USBLabel, 0, MAXINDILABEL);
    USBRC = IUGetConfigText(getDeviceName(), USBControlsLabelsTP.getName(), USBControlsLabelsTP[USB_LABEL_4].name, USBLabel,
                            MAXINDILABEL);
    USBControlV2SP[PORT_4].fill("PORT_4", USBRC == -1 ? "USB3 Port4" : USBLabel, ISS_ON);
    memset(USBLabel, 0, MAXINDILABEL);
    USBRC = IUGetConfigText(getDeviceName(), USBControlsLabelsTP.getName(), USBControlsLabelsTP[USB_LABEL_5].name, USBLabel,
                            MAXINDILABEL);
    USBControlV2SP[PORT_5].fill("PORT_5", USBRC == -1 ? "USB2 Port5" : USBLabel, ISS_ON);
    memset(USBLabel, 0, MAXINDILABEL);
    USBRC = IUGetConfigText(getDeviceName(), USBControlsLabelsTP.getName(), USBControlsLabelsTP[USB_LABEL_6].name, USBLabel,
                            MAXINDILABEL);
    USBControlV2SP[PORT_6].fill("PORT_6", USBRC == -1 ? "USB2 Port6" : USBLabel, ISS_ON);

    USBControlV2SP.fill(getDeviceName(), "USB_PORT_CONTROL", "Ports", USB_TAB, IP_RW,
                       ISR_NOFMANY, 60, IPS_IDLE);

    // USB Labels update with custom values
    USBControlsLabelsTP[USB_LABEL_1].fill("USB_LABEL_1", "USB3 Port1", USBControlV2SP[PORT_1].getLabel());
    USBControlsLabelsTP[USB_LABEL_2].fill("USB_LABEL_2", "USB3 Port2", USBControlV2SP[PORT_2].getLabel());
    USBControlsLabelsTP[USB_LABEL_3].fill("USB_LABEL_3", "USB3 Port3", USBControlV2SP[PORT_3].getLabel());
    USBControlsLabelsTP[USB_LABEL_4].fill("USB_LABEL_4", "USB3 Port4", USBControlV2SP[PORT_4].getLabel());
    USBControlsLabelsTP[USB_LABEL_5].fill("USB_LABEL_5", "USB2 Port5", USBControlV2SP[PORT_5].getLabel());
    USBControlsLabelsTP[USB_LABEL_6].fill("USB_LABEL_6", "USB2 Port6", USBControlV2SP[PORT_6].getLabel());

    USBControlsLabelsTP.fill(getDeviceName(), "USB_CONTROL_LABEL", "USB Labels",
                     USB_TAB, IP_WO, 60, IPS_IDLE);
    // USB Hub Status
    USBStatusLP[PORT_1].fill("PORT_1", USBControlV2SP[PORT_1].getLabel(), IPS_OK);
    USBStatusLP[PORT_2].fill("PORT_2", USBControlV2SP[PORT_2].getLabel(), IPS_OK);
    USBStatusLP[PORT_3].fill("PORT_3", USBControlV2SP[PORT_3].getLabel(), IPS_OK);
    USBStatusLP[PORT_4].fill("PORT_4", USBControlV2SP[PORT_4].getLabel(), IPS_OK);
    USBStatusLP[PORT_5].fill( "PORT_5", USBControlV2SP[PORT_5].getLabel(), IPS_OK);
    USBStatusLP[PORT_6].fill("PORT_6", USBControlV2SP[PORT_6].getLabel(), IPS_OK);
    USBStatusLP.fill(getDeviceName(), "USB_PORT_STATUS", "Status", USB_TAB, IPS_IDLE);

    ////////////////////////////////////////////////////////////////////////////
    /// Focuser Group
    ////////////////////////////////////////////////////////////////////////////

    // Settings
    //    IUFillNumber(&FocusBacklashN[0], "SETTING_BACKLASH", "Backlash (steps)", "%.f", 0, 999, 100, 0);
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
        defineProperty(PowerCycleAllSP);
        defineProperty(PowerSensorsNP);
        defineProperty(PowerConsumptionNP);
        defineProperty(RebootSP);

        // Power
        defineProperty(PowerControlSP);
        defineProperty(PowerControlsLabelsTP);
        defineProperty(PowerCurrentNP);
        defineProperty(PowerOnBootSP);
        //FixMe: nlp not defined in INDI Properties
        // OverCurrentLP.nlp = (version == UPB_V1) ? 4 : 7;
        defineProperty(OverCurrentLP);
        if (version == UPB_V1)
            defineProperty(PowerLEDSP);
        if (version == UPB_V2)
            defineProperty(AdjustableOutputNP);

        // Dew
        if (version == UPB_V1)
            defineProperty(AutoDewSP);
        else
            defineProperty(&AutoDewV2SP);

        // FixMe: ntp not defined in INDI Properties
        // DewControlsLabelsTP.ntp = (version == UPB_V1) ? 2 : 3;
        defineProperty(DewControlsLabelsTP);

        if (version == UPB_V2)
            defineProperty(AutoDewAggNP);

        // FixMe: nnp not defined in INDI Properties
        // DewPWMNP.nnp = (version == UPB_V1) ? 2 : 3;
        defineProperty(DewPWMNP);

        // FixMe: nnp not defined in INDI Properties
        // DewCurrentDrawNP.nnp = (version == UPB_V1) ? 2 : 3;
        defineProperty(DewCurrentDrawNP);

        // USB
        defineProperty(USBControlSP);
        if (version == UPB_V2)
            defineProperty(USBControlV2SP);
        if (version == UPB_V1)
            defineProperty(USBStatusLP);
        defineProperty(USBControlsLabelsTP);

        // Focuser
        FI::updateProperties();
        defineProperty(FocuserSettingsNP);

        WI::updateProperties();

        // Firmware
        defineProperty(FirmwareTP);

        setupComplete = true;
    }
    else
    {
        // Main Control
        deleteProperty(PowerCycleAllSP);
        deleteProperty(PowerSensorsNP);
        deleteProperty(PowerConsumptionNP);
        deleteProperty(RebootSP);

        // Power
        deleteProperty(PowerControlSP);
        deleteProperty(PowerControlsLabelsTP);
        deleteProperty(PowerCurrentNP);
        deleteProperty(PowerOnBootSP);
        deleteProperty(OverCurrentLP);
        if (version == UPB_V1)
            deleteProperty(PowerLEDSP);
        if (version == UPB_V2)
            deleteProperty(AdjustableOutputNP);

        // Dew
        if (version == UPB_V1)
            deleteProperty(AutoDewSP);
        else
        {
            deleteProperty(AutoDewV2SP.name);
            deleteProperty(DewControlsLabelsTP);
            deleteProperty(AutoDewAggNP);
        }

        deleteProperty(DewPWMNP);
        deleteProperty(DewCurrentDrawNP);

        // USB
        deleteProperty(USBControlSP);
        if (version == UPB_V2)
            deleteProperty(USBControlV2SP);
        if (version == UPB_V1)
            deleteProperty(USBStatusLP);
        deleteProperty(USBControlsLabelsTP);

        // Focuser
        FI::updateProperties();
        deleteProperty(FocuserSettingsNP);

        WI::updateProperties();

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
        if (PowerCycleAllSP.isNameMatch(name))
        {
            PowerCycleAllSP.update(states, names, n);

            PowerCycleAllSP.setState(IPS_ALERT);
            char cmd[PEGASUS_LEN] = {0}, res[PEGASUS_LEN] = {0};
            snprintf(cmd, PEGASUS_LEN, "PZ:%d", PowerCycleAllSP.findOnSwitchIndex());
            if (sendCommand(cmd, res))
            {
                PowerCycleAllSP.setState(!strcmp(cmd, res) ? IPS_OK : IPS_ALERT);
            }

            PowerCycleAllSP.reset();
            PowerCycleAllSP.apply();
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

        // Control Power per port
        if (PowerControlSP.isNameMatch(name))
        {
            bool failed = false;
            for (int i = 0; i < n; i++)
            {
                if (!strcmp(names[i], PowerControlSP[i].getName()) && states[i] != PowerControlSP[i].getState())
                {
                    if (setPowerEnabled(i + 1, states[i] == ISS_ON) == false)
                    {
                        failed = true;
                        break;
                    }
                }
            }

            if (failed)
                PowerControlSP.setState(IPS_ALERT);
            else
            {
                PowerControlSP.setState(IPS_OK);
                PowerControlSP.update(states, names, n);
            }

            PowerControlSP.apply();
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

        // Auto Dew v1.
        if ((AutoDewSP.isNameMatch(name)) && (version == UPB_V1))
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
        if (USBControlSP.isNameMatch(name))
        {
            int prevIndex = USBControlSP.findOnSwitchIndex();
            USBControlSP.update(states, names, n);
            if (setUSBHubEnabled(USBControlSP[INDI_ENABLED].getState() == ISS_ON))
            {
                USBControlSP.setState(IPS_OK);
            }
            else
            {
                USBControlSP.reset();
                USBControlSP[prevIndex].setState(ISS_ON);
                USBControlSP.setState(IPS_ALERT);
            }

            USBControlSP.apply();
            return true;
        }

        // USB Hub Control v2
        if (USBControlV2SP.isNameMatch(name))
        {
            bool rc[6] = {false};
            std::fill_n(rc, 6, true);
            ISState ports[6] = {ISS_ON};

            // FixMe: Fix warning
            for (int i = 0; i < USBControlV2SP.count(); i++)
                ports[i] = USBControlV2SP[i].getState();

            USBControlV2SP.update(states, names, n);

            // FixMe: Fix warning
            for (int i = 0; i < USBControlV2SP.count(); i++)
            {
                if (ports[i] != USBControlV2SP[i].getState())
                    rc[i] = setUSBPortEnabled(i, USBControlV2SP[i].getState() == ISS_ON);
            }

            // All is OK
            if (rc[0] && rc[1] && rc[2] && rc[3] && rc[4] && rc[5])
            {
                USBControlSP.setState(IPS_OK);
            }
            else
            {
                USBControlV2SP.reset();
                // FixMe: Fix warning
                for (int i = 0; i < USBControlV2SP.count(); i++)
                    USBControlV2SP[i].setState(ports[i]);
                USBControlV2SP.setState(IPS_ALERT);
            }

            USBControlV2SP.apply();

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
        if (PowerLEDSP.isNameMatch(name) && (version == UPB_V1))
        {
            int prevIndex = PowerLEDSP.findOnSwitchIndex();
            PowerLEDSP.update(states, names, n);
            if (setPowerLEDEnabled(PowerLEDSP[0].getState() == ISS_ON))
            {
                PowerLEDSP.setState(IPS_OK);
            }
            else
            {
                PowerLEDSP.reset();
                PowerLEDSP[prevIndex].setState(ISS_ON);
                PowerLEDSP.setState(IPS_ALERT);
            }

            PowerLEDSP.apply();
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
        if (AdjustableOutputNP.isNameMatch(name))
        {
            if (setAdjustableOutput(static_cast<uint8_t>(values[0])))
            {
                AdjustableOutputNP.update(values, names, n);
                AdjustableOutputNP.setState(IPS_OK);
            }
            else
                AdjustableOutputNP.setState(IPS_ALERT);

            AdjustableOutputNP.apply();
            return true;
        }

        // Dew PWM
        if (DewPWMNP.isNameMatch(name))
        {
            bool rc1 = false, rc2 = false, rc3 = false;
            for (int i = 0; i < n; i++)
            {
                if (!strcmp(names[i], DewPWMNP[DEW_PWM_A].getName()))
                    rc1 = setDewPWM(5, static_cast<uint8_t>(values[i] / 100.0 * 255.0));
                else if (!strcmp(names[i], DewPWMNP[DEW_PWM_B].getName()))
                    rc2 = setDewPWM(6, static_cast<uint8_t>(values[i] / 100.0 * 255.0));
                else if (!strcmp(names[i], DewPWMNP[DEW_PWM_C].getName()))
                    rc3 = setDewPWM(7, static_cast<uint8_t>(values[i] / 100.0 * 255.0));
            }

            DewPWMNP.setState((rc1 && rc2 && rc3) ? IPS_OK : IPS_ALERT);
            if (DewPWMNP.getState() == IPS_OK)
                DewPWMNP.update(values, names, n);
            DewPWMNP.apply();
            return true;
        }

        // Auto Dew Aggressiveness
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
        if (PowerControlsLabelsTP.isNameMatch(name))
        {
            PowerControlsLabelsTP.update(texts, names, n);
            PowerControlsLabelsTP.setState(IPS_OK);
            LOG_INFO("Power port labels saved. Driver must be restarted for the labels to take effect.");
            saveConfig();
            PowerControlsLabelsTP.apply();
            return true;
        }
        // Dew Labels
        if (DewControlsLabelsTP.isNameMatch(name))
        {
            DewControlsLabelsTP.update(texts, names, n);
            DewControlsLabelsTP.setState(IPS_OK);
            LOG_INFO("Dew labels saved. Driver must be restarted for the labels to take effect.");
            saveConfig();
            DewControlsLabelsTP.apply();
            return true;
        }
        // USB Labels
        if (USBControlsLabelsTP.isNameMatch(name))
        {
            USBControlsLabelsTP.update(texts, names, n);
            USBControlsLabelsTP.setState(IPS_OK);
            LOG_INFO("USB labels saved. Driver must be restarted for the labels to take effect.");
            saveConfig();
            USBControlsLabelsTP.apply();
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

        AdjustableOutputNP[0].setValue(std::stod(result[2]));
        AdjustableOutputNP.setState(IPS_OK);

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

    PowerLEDSP.save(fp);
    AutoDewSP.save(fp);
    if (version == UPB_V2)
        AutoDewAggNP.save(fp);
    FocuserSettingsNP.save(fp);
    PowerControlsLabelsTP.save(fp);
    DewControlsLabelsTP.apply();
    USBControlsLabelsTP.save(fp);
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
        PowerControlSP[POWER_CONTROL_1].setState((portStatus[0] == '1') ? ISS_ON : ISS_OFF);
        PowerControlSP[POWER_CONTROL_2].setState((portStatus[1] == '1') ? ISS_ON : ISS_OFF);
        PowerControlSP[POWER_CONTROL_3].setState((portStatus[2] == '1') ? ISS_ON : ISS_OFF);
        PowerControlSP[POWER_CONTROL_4].setState((portStatus[3] == '1') ? ISS_ON : ISS_OFF);
        //if (lastSensorData[7] != result[7])
        if (sensorUpdated(result, 7, 7))
            PowerControlSP.apply();

        // Hub Status
        const char * usb_status = result[8].c_str();
        if (version == UPB_V1)
        {
            USBControlSP[INDI_ENABLED].setState((usb_status[0] == '0') ? ISS_ON : ISS_OFF);
            USBControlSP[INDI_DISABLED].setState((usb_status[0] == '0') ? ISS_OFF : ISS_ON);
            USBStatusLP[PORT_1].setState((USBControlSP[INDI_ENABLED].getState() == ISS_ON) ? IPS_OK : IPS_IDLE);
            USBStatusLP[PORT_2].setState((USBControlSP[INDI_ENABLED].getState() == ISS_ON) ? IPS_OK : IPS_IDLE);
            USBStatusLP[PORT_3].setState((USBControlSP[INDI_ENABLED].getState() == ISS_ON) ? IPS_OK : IPS_IDLE);
            USBStatusLP[PORT_4].setState((USBControlSP[INDI_ENABLED].getState() == ISS_ON) ? IPS_OK : IPS_IDLE);
            USBStatusLP[PORT_5].setState((USBControlSP[INDI_ENABLED].getState() == ISS_ON) ? IPS_OK : IPS_IDLE);
            //if (lastSensorData[8] != result[8])
            if (sensorUpdated(result, 8, 8))
            {
                USBControlSP.setState((USBControlSP.findOnSwitchIndex() == 0) ? IPS_OK : IPS_IDLE);
                USBControlSP.apply();
                USBStatusLP.apply();
            }
        }
        else
        {
            USBControlV2SP[PORT_1].setState((usb_status[0] == '1') ? ISS_ON : ISS_OFF);
            USBControlV2SP[PORT_2].setState((usb_status[1] == '1') ? ISS_ON : ISS_OFF);
            USBControlV2SP[PORT_3].setState((usb_status[2] == '1') ? ISS_ON : ISS_OFF);
            USBControlV2SP[PORT_4].setState((usb_status[3] == '1') ? ISS_ON : ISS_OFF);
            USBControlV2SP[PORT_5].setState((usb_status[4] == '1') ? ISS_ON : ISS_OFF);
            USBControlV2SP[PORT_6].setState((usb_status[5] == '1') ? ISS_ON : ISS_OFF);
            USBControlV2SP.setState(IPS_OK);
            //if (lastSensorData[8] != result[8])
            if (sensorUpdated(result, 8, 8))
            {
                USBControlV2SP.apply();
            }
        }

        // From here, we get differences between v1 and v2 readings
        int index = 9;
        // Dew PWM
        DewPWMNP[DEW_PWM_A].setValue(std::stod(result[index]) / 255.0 * 100.0);
        DewPWMNP[DEW_PWM_B].setValue(std::stod(result[index + 1]) / 255.0 * 100.0);
        if (version == UPB_V2)
            DewPWMNP[DEW_PWM_C].setValue(std::stod(result[index + 2]) / 255.0 * 100.0);
        //        if (lastSensorData[index] != result[index] ||
        //                lastSensorData[index + 1] != result[index + 1] ||
        //                (version == UPB_V2 && lastSensorData[index +2] != result[index + 2]))
        if (sensorUpdated(result, index, version == UPB_V1 ? index + 1 : index + 2))
            DewPWMNP.apply();

        index = (version == UPB_V1) ? 11 : 12;

        const double ampDivision = (version == UPB_V1) ? 400.0 : 480.0;

        // Current draw
        PowerCurrentNP[POWER_CURRENT_1].setValue(std::stod(result[index]) / ampDivision);
        PowerCurrentNP[POWER_CURRENT_2].setValue(std::stod(result[index + 1]) / ampDivision);
        PowerCurrentNP[POWER_CURRENT_3].setValue(std::stod(result[index + 2]) / ampDivision);
        PowerCurrentNP[POWER_CURRENT_4].setValue(std::stod(result[index + 3]) / ampDivision);
        //        if (lastSensorData[index] != result[index] ||
        //                lastSensorData[index + 1] != result[index + 1] ||
        //                lastSensorData[index + 2] != result[index + 2] ||
        //                lastSensorData[index + 3] != result[index + 3])
        if (sensorUpdated(result, index, index + 3))
            PowerCurrentNP.apply();

        index = (version == UPB_V1) ? 15 : 16;

        DewCurrentDrawNP[DEW_PWM_A].setValue(std::stod(result[index]) / ampDivision);
        DewCurrentDrawNP[DEW_PWM_B].setValue(std::stod(result[index + 1]) / ampDivision);
        if (version == UPB_V2)
            DewCurrentDrawNP[DEW_PWM_C].setValue(std::stod(result[index + 2]) / 700);
        //        if (lastSensorData[index] != result[index] ||
        //                lastSensorData[index + 1] != result[index + 1] ||
        //                (version == UPB_V2 && lastSensorData[index + 2] != result[index + 2]))
        if (sensorUpdated(result, index, version == UPB_V1 ? index + 1 : index + 2))
            DewCurrentDrawNP.apply();

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
                AutoDewSP[INDI_ENABLED].setState((std::stoi(result[index]) == 1) ? ISS_ON : ISS_OFF);
                AutoDewSP[INDI_DISABLED].setState((std::stoi(result[index]) == 1) ? ISS_OFF : ISS_ON);
                AutoDewSP.apply();
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
        if (result.size() != 4)
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
        else if (stepperUpdated(result, 0))
            IDSetNumber(&FocusAbsPosNP, nullptr);

        FocusReverseS[INDI_ENABLED].s = (std::stoi(result[2]) == 1) ? ISS_ON : ISS_OFF;
        FocusReverseS[INDI_DISABLED].s = (std::stoi(result[2]) == 1) ? ISS_OFF : ISS_ON;

        if (stepperUpdated(result, 1))
            IDSetSwitch(&FocusReverseSP, nullptr);

        uint16_t backlash = std::stoi(result[3]);
        if (backlash == 0)
        {
            FocusBacklashN[0].value = backlash;
            FocusBacklashS[INDI_ENABLED].s = ISS_OFF;
            FocusBacklashS[INDI_DISABLED].s = ISS_ON;
            if (stepperUpdated(result, 3))
            {
                IDSetSwitch(&FocusBacklashSP, nullptr);
                FocuserSettingsNP.apply();
            }
        }
        else
        {
            FocusBacklashS[INDI_ENABLED].s = ISS_ON;
            FocusBacklashS[INDI_DISABLED].s = ISS_OFF;
            FocusBacklashN[0].value = backlash;
            if (stepperUpdated(result, 3))
            {
                IDSetSwitch(&FocusBacklashSP, nullptr);
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
