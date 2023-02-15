/*
    USB_Dewpoint
    Copyright (C) 2017-2023 Jarno Paananen

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301
   USA

*/

#include "myDewControllerPro.h"
#include "connectionplugins/connectionserial.h"
#include "indicom.h"

#include <cstring>
#include <memory>

#include <termios.h>
#include <unistd.h>

#define MYDEWHEATERPRO_TIMEOUT 3
#define BOARD_FAN_TAB "Board Fan"
#define TEMPERATURE_OFFSETS_TAB "Temperature/Tracking Offsets"
#define LCD_DISPLAY_TAB "LCD Display"

std::unique_ptr<myDewControllerPro> mydewcontrollerpro(new myDewControllerPro());


myDewControllerPro::myDewControllerPro()
{
    setVersion(1, 0);
}

bool myDewControllerPro::initProperties()
{
    DefaultDevice::initProperties();

    /* Channel duty cycles */
    IUFillNumber(&OutputsN[DEW_STRAP_ONE_POWER], "CHANNEL1", "Strap 1", "%4.0f %%", 0., 100., 1., 0.);
    IUFillNumber(&OutputsN[DEW_STRAP_TWO_POWER], "CHANNEL2", "Strap 2", "%4.0f %%", 0., 100., 1., 0.);
    IUFillNumber(&OutputsN[DEW_STRAP_THREE_POWER], "CHANNEL3", "Strap 3", "%4.0f %%", 0., 100., 1., 0.);
    IUFillNumberVector(&OutputsNP, OutputsN, 3, getDeviceName(), "OUTPUT", "Outputs", MAIN_CONTROL_TAB, IP_RO, 0, IPS_IDLE);

    IUFillNumber(&FanSpeedN[0], "Fan Power", "Fan Speed", "%4.0f %%", 0., 100., 1., 0.);
    IUFillNumberVector(&FanSpeedNP, FanSpeedN, 1, getDeviceName(), "FanSpeed", "Board Fan", BOARD_FAN_TAB, IP_RW, 0, IPS_IDLE);

    IUFillSwitch(&FanModeS[0], "Board Temp", "Board Temp Sensor", ISS_OFF);
    IUFillSwitch(&FanModeS[1], "Manual", "Manual", ISS_ON);
    IUFillSwitchVector(&FanModeSP, FanModeS, 2, getDeviceName(), "Fan_Mode", "Fan Mode", BOARD_FAN_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    IUFillSwitch(&EEPROMS[0], "Reset EEPROM", "Reset EEPROM to Defaults", ISS_OFF);
    IUFillSwitch(&EEPROMS[1], "Save to EEPROM", "Save to EEPROM", ISS_OFF);
    IUFillSwitchVector(&EEPROMSP, EEPROMS, 2, getDeviceName(), "EEPROM", "EEPROM", OPTIONS_TAB, IP_WO, ISR_ATMOST1, 0, IPS_IDLE);

    IUFillNumber(&FanTempTriggerN[FANTEMPOFF], "Board_Temp_Off", "Board Fan Temp Off", "%4.0f \u2103", 0., 100., 1., 0.);
    IUFillNumber(&FanTempTriggerN[FANTEMPON], "Board_Temp_On", "Board Fan Temp On", "%4.0f \u2103", 0., 100., 1., 0.);
    IUFillNumberVector(&FanTempTriggerNP, FanTempTriggerN, 2, getDeviceName(), "Fan Trigger Temps", "Fan Trigger", BOARD_FAN_TAB, IP_RW, 0, IPS_IDLE);

    IUFillNumber(&LCDPageRefreshN[0], "Page Refresh Rate", "Page Refresh Rate", "%4.0f ms", 500., 5000., 500., 0.);
    IUFillNumberVector(&LCDPageRefreshNP, LCDPageRefreshN, 1, getDeviceName(), "LCD Page", "LCD Page", LCD_DISPLAY_TAB, IP_RW, 0, IPS_IDLE);

    IUFillSwitch(&LCDDisplayTempUnitsS[0], "Celsius", "Celsius", ISS_ON);
    IUFillSwitch(&LCDDisplayTempUnitsS[1], "Fahrenheit", "Fahrenheit", ISS_OFF);
    IUFillSwitchVector(&LCDDisplayTempUnitsSP, LCDDisplayTempUnitsS, 2, getDeviceName(), "Temp Units", "Temp Units", LCD_DISPLAY_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    IUFillSwitch(&EnableLCDDisplayS[0], "Disabled", "Disabled", ISS_ON);
    IUFillSwitch(&EnableLCDDisplayS[1], "Enabled", "Enabled", ISS_OFF);
    IUFillSwitchVector(&EnableLCDDisplaySP, EnableLCDDisplayS, 2, getDeviceName(), "LCD Status", "LCD Status", LCD_DISPLAY_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);



    /* Channel Manual and Boost */
    IUFillSwitch(&CH1CH2BoostS[CH1_BOOST_100], "BOOST_CH1", "Strap 1 Boost 100%", ISS_OFF);
    IUFillSwitch(&CH1CH2BoostS[CH2_BOOST_100], "BOOST_CH2", "Strap 2 Boost 100%", ISS_OFF);
    IUFillSwitchVector(&CH1CH2BoostSP, CH1CH2BoostS, 2, getDeviceName(), "CHANNEL_BOOST", "Heat Boost", MAIN_CONTROL_TAB, IP_RW, ISR_NOFMANY, 0, IPS_IDLE);

    IUFillSwitch(&CH3_ModeS[DISABLED], "STRAP_DISABLED", "Strap Disabled", ISS_ON);
    IUFillSwitch(&CH3_ModeS[DEWSTRAP_ONE], "SHADOW STRAP 1", "Shadow Strap 1", ISS_OFF);
    IUFillSwitch(&CH3_ModeS[DEWSTRAP_TWO], "SHADOW STRAP 2", "Shadow Strap 2", ISS_OFF);
    IUFillSwitch(&CH3_ModeS[MANUAL], "Manual", "Manual", ISS_OFF);
    IUFillSwitch(&CH3_ModeS[TEMP_PROBE_THREE], "TEMP_PROBE", "Temp Probe", ISS_OFF);
    IUFillSwitchVector(&CH3_ModeSP, CH3_ModeS, 5, getDeviceName(), "CHANEL 3 SHAWDOW", "Strap 3 Mode", MAIN_CONTROL_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    IUFillNumber(&CH3_Manual_PowerN[0], "MANUAL_POWER", "Strap 3 Manual Power", "%4.0f %%", 0., 100., 1., 0.);
    IUFillNumberVector(&CH3_Manual_PowerNP, CH3_Manual_PowerN, 1, getDeviceName(), "CH3_POWER", "Strap 3 Power", MAIN_CONTROL_TAB, IP_RW, 0, IPS_IDLE);



    /* Temperatures */
    IUFillNumber(&TemperaturesN[PROBE_1], "CHANNEL1", "Strap 1", "%3.2f \u2103", -50., 70., 0., 0.);
    IUFillNumber(&TemperaturesN[PROBE_2], "CHANNEL2", "Strap 2", "%3.2f \u2103", -50., 70., 0., 0.);
    IUFillNumber(&TemperaturesN[PROBE_3], "CHANNEL3", "Strap 3", "%3.2f \u2103", -50., 70., 0., 0.);
    IUFillNumber(&TemperaturesN[AMBIENT_PROBE], "AMBIENT", "Ambient", "%3.2f \u2103", -50., 70., 0., 0.);
    IUFillNumber(&TemperaturesN[BOARD_PROBE], "BOARD Temp", "Board", "%3.2f \u2103", -50., 100., 0., 0.);
    IUFillNumberVector(&TemperaturesNP, TemperaturesN, 5, getDeviceName(), "TEMPERATURES", "Temperatures", MAIN_CONTROL_TAB, IP_RO, 0, IPS_IDLE);

    /* Humidity */
    IUFillNumber(&HumidityN[0], "HUMIDITY", "Humidity", "%3.2f %%", 0., 100., 0., 0.);
    IUFillNumberVector(&HumidityNP, HumidityN, 1, getDeviceName(), "HUMIDITY", "Humidity", MAIN_CONTROL_TAB, IP_RO, 0, IPS_IDLE);

    /* Dew point */
    IUFillNumber(&DewpointN[0], "DEWPOINT", "Dew point", "%3.2f \u2103", -50., 70., 0., 0.);
    IUFillNumberVector(&DewpointNP, DewpointN, 1, getDeviceName(), "DEWPOINT", "Dew point", MAIN_CONTROL_TAB, IP_RO, 0, IPS_IDLE);

    /* Temperature calibration values */
    IUFillNumber(&TemperatureOffsetsN[TEMP_PROBE_ONE_OFFSET], "CHANNEL1", "Strap 1", "%1.0f \u2103", -10., 10., 1., 0.);
    IUFillNumber(&TemperatureOffsetsN[TEMP_PROBE_TWO_OFFSET], "CHANNEL2", "Strap 2", "%1.0f \u2103", -10., 10., 1., 0.);
    IUFillNumber(&TemperatureOffsetsN[TEMP_PROBE_THREE_OFFSET], "CHANNEL3", "Strap 3", "%1.0f \u2103", -10., 10., 1., 0.);
    IUFillNumber(&TemperatureOffsetsN[AMBIENT_TEMP_PROBE_OFFSET], "AMBIENT", "Ambient", "%4.0f \u2103", -4, 3, 1, 0);
    IUFillNumberVector(&TemperatureOffsetsNP, TemperatureOffsetsN, 4, getDeviceName(), "TEMP_CALIBRATIONS", "Temp Offsets", TEMPERATURE_OFFSETS_TAB, IP_RW, 0, IPS_IDLE);

    IUFillSwitch(&ZeroTempOffsetsS[0], "Zero_Temp", "Zero Temperature Offsets", ISS_OFF);
    IUFillSwitchVector(&ZeroTempOffsetsSP, ZeroTempOffsetsS, 1, getDeviceName(), "Zero Offsets", "Zero Offsets", TEMPERATURE_OFFSETS_TAB, IP_RW, ISR_ATMOST1, 0, IPS_IDLE);

    /* Tracking Mode Options */

    IUFillSwitch(&TrackingModeS[AMBIENT], "AMBIENT", "Ambient", ISS_OFF);
    IUFillSwitch(&TrackingModeS[DEWPOINT], "DEWPOINT", "Dew Point", ISS_ON);
    IUFillSwitch(&TrackingModeS[MIDPOINT], "MIDPOINT", "Mid Point", ISS_OFF);
    IUFillSwitchVector(&TrackingModeSP, TrackingModeS, 3, getDeviceName(), "Tracking Mode", "Tracking Mode", TEMPERATURE_OFFSETS_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    IUFillNumber(&TrackingModeOffsetN[0], "Offset", "Offset", "%4.0f \u2103", -4, 3, 1, 0);
    IUFillNumberVector(&TrackingModeOffsetNP, TrackingModeOffsetN, 1, getDeviceName(), "Tracking Offset", "Tracking Offset", TEMPERATURE_OFFSETS_TAB, IP_RW, 0, IPS_IDLE);



    /* Reset settings */
   // IUFillSwitch(&ResetS[0], "Reset", "", ISS_OFF);
   // IUFillSwitchVector(&ResetSP, ResetS, 1, getDeviceName(), "Reset", "", OPTIONS_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    /* Firmware version */
    IUFillNumber(&FWVersionN[0], "FIRMWARE", "Firmware Version", "%4.0f", 0., 65535., 1., 0.);
    IUFillNumberVector(&FWVersionNP, FWVersionN, 1, getDeviceName(), "FW_VERSION", "Firmware", OPTIONS_TAB, IP_RO, 0, IPS_IDLE);

    setDriverInterface(AUX_INTERFACE);

    addDebugControl();
    addConfigurationControl();
    setDefaultPollingPeriod(10000);
    addPollPeriodControl();

    // No simulation control for now

    serialConnection = new Connection::Serial(this);
    serialConnection->registerHandshake([&]()
    {
        return Handshake();
    });
    registerConnection(serialConnection);

    return true;
}

bool myDewControllerPro::cancelOutputBoost()
{
    if (sendCommand(MDCP_CANCEL_BOOST, nullptr)) {
        return true;
    } else {
        LOG_INFO("Failed to cancel Boost");
        LOG_INFO(MDCP_CANCEL_BOOST);
        return false;
    }

}

bool myDewControllerPro::updateProperties()
{
    DefaultDevice::updateProperties();

    if (isConnected())
    {
        defineProperty(&OutputsNP);
        defineProperty(&CH1CH2BoostSP);
        defineProperty(&CH3_ModeSP);
        defineProperty(&CH3_Manual_PowerNP);
        defineProperty(&TemperaturesNP);
        defineProperty(&HumidityNP);
        defineProperty(&DewpointNP);
        defineProperty(&FanSpeedNP);
        defineProperty(&FanModeSP);
        defineProperty(&TemperatureOffsetsNP);
        defineProperty(&ZeroTempOffsetsSP);
        defineProperty(&TrackingModeSP);
        defineProperty(&TrackingModeOffsetNP);
        defineProperty(&FanTempTriggerNP);
        defineProperty(&EnableLCDDisplaySP);
        defineProperty(&LCDDisplayTempUnitsSP);
        defineProperty(&LCDPageRefreshNP);
        defineProperty(&EEPROMSP);


        defineProperty(&FWVersionNP);
        cancelOutputBoost();

        loadConfig(true);
        if (!readSettings()) {
            LOG_INFO("Read Settings False");
        }
        LOG_INFO("myDewControllerPro parameters updated, device ready for use.");
        SetTimer(getCurrentPollingPeriod());
    }
    else
    {
        deleteProperty(OutputsNP.name);
        deleteProperty(CH1CH2BoostSP.name);
        deleteProperty(CH3_ModeSP.name);
        deleteProperty(CH3_Manual_PowerNP.name);
        deleteProperty(TemperaturesNP.name);
        deleteProperty(HumidityNP.name);
        deleteProperty(DewpointNP.name);
        deleteProperty(FanSpeedNP.name);
        deleteProperty(FanModeSP.name);
        deleteProperty(TemperatureOffsetsNP.name);
        deleteProperty(ZeroTempOffsetsSP.name);
        deleteProperty(TrackingModeSP.name);
        deleteProperty(TrackingModeOffsetNP.name);
        deleteProperty(FanTempTriggerNP.name);
        deleteProperty(EnableLCDDisplaySP.name);
        deleteProperty(LCDDisplayTempUnitsSP.name);
        deleteProperty(LCDPageRefreshNP.name);

        deleteProperty(EEPROMSP.name);
        deleteProperty(FWVersionNP.name);
    }

    return true;
}

const char *myDewControllerPro::getDefaultName()
{
    return "myDewContollerPro";
}

bool myDewControllerPro::sendCommand(const char *cmd, char *resp)
{
    int nbytes_written = 0, nbytes_read = 0, rc = -1;
    char errstr[MAXRBUF];
    LOGF_DEBUG("CMD: %s.", cmd);

    tcflush(PortFD, TCIOFLUSH);
    if ((rc = tty_write(PortFD, cmd, strlen(cmd), &nbytes_written)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("Error writing command %s: %s.", cmd, errstr);
        return false;
    }

    if (resp)
    {
        if ((rc = tty_nread_section(PortFD, resp, MDCP_RES_LEN, '$', MYDEWHEATERPRO_TIMEOUT, &nbytes_read)) != TTY_OK)
        {
            tty_error_msg(rc, errstr, MAXRBUF);
            LOGF_ERROR("Error reading response for command %s: %s.", cmd, errstr);
            return false;
        }
    }
    return true;
}



bool myDewControllerPro::Handshake()
{
    PortFD = serialConnection->getPortFD();

    int tries = 3;
    do
    {
        if (Ack())
        {
            LOG_INFO("myDewControllerPro is online. Getting device parameters...");
            return true;
        }
        LOG_INFO("Error retrieving data from myDewControllerPro, trying resync...");
    }
    while (--tries > 0);

    //LOG_INFO("Error retrieving data from myDewControllerPro, please ensure controller "
    //         "is powered and the port is correct.");
    return false;
}

bool myDewControllerPro::Ack()
{
    char resp[MDCP_RES_LEN] = {};
    tcflush(PortFD, TCIOFLUSH);

    if (!sendCommand(MDCP_GET_VERSION, resp))
        return false;

    int firmware = -1;
    int ok = sscanf(resp, MDCP_IDENTIFY_RESPONSE, &firmware);
    snprintf(resp, 40, "Firmware Version: %d", firmware);
    LOG_INFO(resp);

    if (ok != 1)
    {
        LOGF_ERROR("myDewControllerPro not properly identified! Answer was: %s.", resp);
        return false;
    }
    if (firmware < 340) {
        LOG_INFO("Please update myDewControllerPro firmware");
        LOG_INFO("https://sourceforge.net/projects/arduinonanodewcontrollerpro/files/myDewControllerPro%20v300%203channel/CODE%20ARDUINO/");
        return false;
    }
    int numberProbes = 0;
    if (!sendCommand(MDCP_GET_NUMBER_OF_PROBES, resp))
        return false;
    sscanf(resp, "g%u$", &numberProbes);
    snprintf(resp, 40, "The number of Temperature Probes are: %d", numberProbes);
    LOG_INFO(resp);
    if (numberProbes < 1) {
        LOG_INFO("Warning no temperature probes detected");
    }
    FWVersionNP.s = IPS_BUSY;
    FWVersionN[0].value = firmware;
    FWVersionNP.s = IPS_OK;
    IDSetNumber(&FWVersionNP, nullptr);

    return true;

}

bool myDewControllerPro::setOutputBoost(unsigned int channel)
{



    if (channel == 0) {
        return sendCommand(MDCP_BOOST_CH1, nullptr);
    } else if (channel == 1) {
        return sendCommand(MDCP_BOOST_CH2, nullptr);
    } else {
        LOG_INFO("No Channel Set");
        return false;
    }

}



bool myDewControllerPro::channelThreeModeSet(unsigned int mode)
{
    char cmd[MDCP_CMD_LEN + 1];

    snprintf(cmd, MDCP_CMD_LEN + 1, MDCP_SET_CH3_SETTINGS, mode);
    if (!sendCommand(cmd, nullptr)) {
        LOG_INFO("Failed to set CH3 Mode");
        LOG_INFO(cmd);
        return false;
    }
    return true;
}

bool myDewControllerPro::fanModeSet(unsigned int mode)
{
    char cmd[MDCP_CMD_LEN + 1];

    snprintf(cmd, MDCP_CMD_LEN + 1, MDCP_SET_FAN_MODE, mode);
    if (!sendCommand(cmd, nullptr)) {
        LOG_INFO("Failed to set Fan Mode");
        LOG_INFO(cmd);
        return false;
    }
    return true;
}

bool myDewControllerPro::setLCDEnable(int mode)
{
    const char* mask = mode == 1 ? MDCP_LCD_ENABLE : MDCP_LCD_DISABLE;
    if (!sendCommand(mask, nullptr)) {
        LOG_INFO("Failed to set LCD enable");

        return false;
    }
    return true;
}


bool myDewControllerPro::setEEPROM(int mode)
{
    const char* mask = mode == 1 ? MDCP_SAVE_TO_EEPROM : MDCP_RESET_EEPROM_TO_DEFAULT;
    const char* message = mode == 1 ? "Saved to EEPPROM Successfully" : "Reset EEPROM to Default";
    if (!sendCommand(mask, nullptr)) {
        LOG_INFO("Failed to set ");

        return false;
    }
    LOG_INFO(message);
    return true;
}

bool myDewControllerPro::setLCDTempDisplay(int mode)
{
    const char* mask = mode == 1 ? MDCP_LCD_DISPLAY_FAHRENHEIT : MDCP_LCD_DISPLAY_CELSIUS;
    if (!sendCommand(mask, nullptr)) {
        LOG_INFO("Failed to set temp display mode");

        return false;
    }
    return true;
}

bool myDewControllerPro::trackingModeSet(unsigned int mode)
{
    char cmd[MDCP_CMD_LEN + 1];

    snprintf(cmd, MDCP_CMD_LEN + 1, MDCP_SET_TRACKING_MODE, mode);
    if (!sendCommand(cmd, nullptr)) {
        LOG_INFO("Failed to set Tracking Mode");
        LOG_INFO(cmd);
        return false;
    }
    return true;
}

bool myDewControllerPro::changeTrackingOffsets(int offset) {

    char cmd[MDCP_CMD_LEN + 1];

    snprintf(cmd, MDCP_CMD_LEN + 1, MDCP_SET_TRACKING_MODE_OFFSET, offset);
    if (!sendCommand(cmd, nullptr)) {
        LOG_INFO("Failed to set Tracking offset");
        LOG_INFO(cmd);
        return false;
    }

    return true;
}

bool myDewControllerPro::setCHThreeManualPower(unsigned int power)
{
    char cmd[MDCP_CMD_LEN + 1];

    snprintf(cmd, MDCP_CMD_LEN + 1, MDCP_SET_CH3_MANUAL_POWER, power);
    if (!sendCommand(cmd, nullptr)) {
        LOG_INFO("Failed to set CH3 Power");
        LOG_INFO(cmd);
        return false;
    }
    return true;

}
bool myDewControllerPro::setFanSpeed(int speed)
{
    char cmd[MDCP_CMD_LEN + 1];

    snprintf(cmd, MDCP_CMD_LEN + 1, MDCP_SET_FAN_SPEED, speed);
    if (!sendCommand(cmd, nullptr)) {
        LOG_INFO("Failed to set Fan Speed");
        LOG_INFO(cmd);
        return false;
    }
    return true;

}

bool myDewControllerPro::setLCDPageRefreshRate(int time)
{
    char cmd[MDCP_CMD_LEN + 1];

    snprintf(cmd, MDCP_CMD_LEN + 1, MDCP_SET_LCD_DISPLAY_TIME, time);
    if (!sendCommand(cmd, nullptr)) {
        LOG_INFO("Failed to set LCD page refresh");
        LOG_INFO(cmd);
        return false;
    }
    return true;

}


bool myDewControllerPro::setTempCalibrations(float ch1, float ch2, float ch3, int ambient)
{
    char cmd[MDCP_CMD_LEN + 1];


    snprintf(cmd, MDCP_CMD_LEN + 1, MDCP_SET_TEMP_CH1_OFFSET, ch1);
    if (!sendCommand(cmd, nullptr)) {
        LOG_INFO("Failed to set CH1 offset");
        LOG_INFO(cmd);
        return false;
    }
    snprintf(cmd, MDCP_CMD_LEN + 1, MDCP_SET_TEMP_CH2_OFFSET, ch2);
    if (!sendCommand(cmd, nullptr)) {
        LOG_INFO("Failed to set CH2 offset");
        LOG_INFO(cmd);
        return false;
    }
    snprintf(cmd, MDCP_CMD_LEN + 1, MDCP_SET_TEMP_CH3_OFFSET, ch3);
    if (!sendCommand(cmd, nullptr)) {
        LOG_INFO("Failed to set CH3 offset");
        LOG_INFO(cmd);
        return false;
    }

    snprintf(cmd, MDCP_CMD_LEN + 1, MDCP_SET_AMB_TEMP_OFFSET, ambient);
    if (!sendCommand(cmd, nullptr)) {
        LOG_INFO("Failed to set CH3 offset");
        LOG_INFO(cmd);
        return false;
    }
    return true;



}

bool myDewControllerPro::setFanTempTrigger(int tempOn, int tempOff)
{
    char cmd[MDCP_CMD_LEN + 1];


    snprintf(cmd, MDCP_CMD_LEN + 1, MDCP_SET_FAN_ON_TEMP, tempOn);
    if (!sendCommand(cmd, nullptr)) {
        LOG_INFO("Failed to set temp on");
        LOG_INFO(cmd);
        return false;
    }
    snprintf(cmd, MDCP_CMD_LEN + 1, MDCP_SET_FAN_OFF_TEMP, tempOff);
    if (!sendCommand(cmd, nullptr)) {
        LOG_INFO("Failed to set CH2 offset");
        LOG_INFO(cmd);
        return false;
    }

    return true;



}

bool myDewControllerPro::zeroTempCalibrations() {

    if (!sendCommand(MDCP_CLEAR_TEMP_OFFSETS, nullptr)) {
        LOG_INFO("Failed to zero temp offset");

        return false;
    }
    if (!sendCommand("e0#", nullptr)) {
        LOG_INFO("Failed to zero ambtemp offset");

        return false;
    }
    return true;


}


bool myDewControllerPro::reset()
{
    //char resp[MDCP_RES_LEN];
    return true;
    //sendCommand(UDP_RESET_CMD, resp);
}





bool myDewControllerPro::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if (!dev || strcmp(dev, getDeviceName()))
        return false;

    if (!strcmp(name, CH1CH2BoostSP.name))
    {
        IUUpdateSwitch(&CH1CH2BoostSP, states, names, n);
        CH1CH2BoostSP.s = IPS_BUSY;
        cancelOutputBoost();
        if (CH1CH2BoostS[CH1_BOOST_100].s == ISS_ON) {
            setOutputBoost(CH1_BOOST_100);
        }
        if (CH1CH2BoostS[CH2_BOOST_100].s == ISS_ON) {
            setOutputBoost(CH2_BOOST_100);
        }
        CH1CH2BoostSP.s = IPS_OK;
        IDSetSwitch(&CH1CH2BoostSP, nullptr);
        readSettings();
        return true;

    }

    if (!strcmp(name, CH3_ModeSP.name))
    {
        IUUpdateSwitch(&CH3_ModeSP, states, names, n);
        CH3_ModeSP.s= IPS_BUSY;
        int mode = IUFindOnSwitchIndex(&CH3_ModeSP);
        channelThreeModeSet(mode);

        CH3_ModeSP.s = IPS_OK;
        IDSetSwitch(&CH3_ModeSP, nullptr);
        readSettings();
        return true;

    }

    if (!strcmp(name, ZeroTempOffsetsSP.name))
    {
        IUUpdateSwitch(&ZeroTempOffsetsSP, states, names, n);
        ZeroTempOffsetsSP.s = IPS_BUSY;
        zeroTempCalibrations();
        ZeroTempOffsetsSP.s = IPS_OK;
        ZeroTempOffsetsS[0].s = ISS_OFF;
        IDSetSwitch(&ZeroTempOffsetsSP, nullptr);
        readSettings();
        return true;
    }

    if (!strcmp(name, TrackingModeSP.name))
    {
        IUUpdateSwitch(&TrackingModeSP, states, names, n);
        TrackingModeSP.s= IPS_BUSY;
        int mode = IUFindOnSwitchIndex(&TrackingModeSP);
        trackingModeSet(mode);

        TrackingModeSP.s = IPS_OK;
        IDSetSwitch(&TrackingModeSP, nullptr);
        readSettings();
        return true;

    }

    if (!strcmp(name, FanModeSP.name))
    {
        IUUpdateSwitch(&FanModeSP, states, names, n);
        FanModeSP.s= IPS_BUSY;
        int mode = IUFindOnSwitchIndex(&FanModeSP);
        fanModeSet(mode);

        FanModeSP.s = IPS_OK;
        IDSetSwitch(&FanModeSP, nullptr);
        readSettings();
        return true;

    }

    if (!strcmp(name, EnableLCDDisplaySP.name))
    {
        IUUpdateSwitch(&EnableLCDDisplaySP, states, names, n);
        EnableLCDDisplaySP.s= IPS_BUSY;
        int mode = IUFindOnSwitchIndex(&EnableLCDDisplaySP);
        setLCDEnable(mode);

        EnableLCDDisplaySP.s = IPS_OK;
        IDSetSwitch(&EnableLCDDisplaySP, nullptr);
        readSettings();
        return true;

    }

    if (!strcmp(name, LCDDisplayTempUnitsSP.name))
    {
        IUUpdateSwitch(&LCDDisplayTempUnitsSP, states, names, n);
        LCDDisplayTempUnitsSP.s= IPS_BUSY;
        int mode = IUFindOnSwitchIndex(&LCDDisplayTempUnitsSP);
        setLCDTempDisplay(mode);

        LCDDisplayTempUnitsSP.s = IPS_OK;
        IDSetSwitch(&LCDDisplayTempUnitsSP, nullptr);
        readSettings();
        return true;

    }

    if (!strcmp(name, EEPROMSP.name))
    {
        IUUpdateSwitch(&EEPROMSP, states, names, n);
        EEPROMSP.s= IPS_BUSY;
        int mode = IUFindOnSwitchIndex(&EEPROMSP);
        setEEPROM(mode);

        EEPROMSP.s = IPS_OK;
        IDSetSwitch(&EEPROMSP, nullptr);
        return true;

    }




//    if (ResetSP.isNameMatch(name))
//    {
//        ResetSP.reset();

//        if (reset())
//        {
//            ResetSP.setState(IPS_OK);
//            readSettings();
//        }
//        else
//        {
//            ResetSP.setState(IPS_ALERT);
//        }

//        ResetSP.apply();
//        return true;
//    }

    return INDI::DefaultDevice::ISNewSwitch(dev, name, states, names, n);
}

bool myDewControllerPro::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    if (!dev || strcmp(dev, getDeviceName()))
        return false;


    if (!strcmp(name, CH3_Manual_PowerNP.name))
    {
        if (IUFindOnSwitchIndex(&CH3_ModeSP) == 3) {
            IUUpdateNumber(&CH3_Manual_PowerNP, values, names, n);
            CH3_Manual_PowerNP.s = IPS_BUSY;
            int power = CH3_Manual_PowerN[0].value;
            setCHThreeManualPower(power);
            CH3_Manual_PowerNP.s = IPS_OK;
            IDSetNumber(&CH3_Manual_PowerNP, nullptr);
            readSettings();
        } else {
            LOG_INFO("Power can only be manually adjusted in Strap 3 manual mode");
        }


        readSettings();
        return true;
    }

    if (!strcmp(name, TemperatureOffsetsNP.name)) {
        IUUpdateNumber(&TemperatureOffsetsNP, values, names, n);
        TemperatureOffsetsNP.s = IPS_BUSY;
        int ch1 = TemperatureOffsetsN[TEMP_PROBE_ONE_OFFSET].value;
        int ch2 = TemperatureOffsetsN[TEMP_PROBE_TWO_OFFSET].value;
        int ch3 = TemperatureOffsetsN[TEMP_PROBE_THREE_OFFSET].value;
        int ambient = TemperatureOffsetsN[AMBIENT_TEMP_PROBE_OFFSET].value;
        setTempCalibrations(ch1, ch2, ch3, ambient);
        TemperatureOffsetsNP.s = IPS_OK;
        IDSetNumber(&TemperatureOffsetsNP, nullptr);
        readSettings();
        return true;

    }

    if (!strcmp(name, TrackingModeOffsetNP.name)) {
        IUUpdateNumber(&TrackingModeOffsetNP, values, names, n);
        TrackingModeOffsetNP.s = IPS_BUSY;
        int offset = TrackingModeOffsetN[0].value;
        changeTrackingOffsets(offset);
        TrackingModeOffsetNP.s = IPS_OK;
        IDSetNumber(&TrackingModeOffsetNP, nullptr);
        readSettings();
        return true;
    }

    if (!strcmp(name, FanTempTriggerNP.name)) {
        IUUpdateNumber(&FanTempTriggerNP, values, names, n);
        FanTempTriggerNP.s = IPS_BUSY;
        int tempOn = FanTempTriggerN[FANTEMPON].value;
        int tempOff = FanTempTriggerN[FANTEMPOFF].value;
        setFanTempTrigger(tempOn, tempOff);
        FanTempTriggerNP.s = IPS_OK;
        IDSetNumber(&FanTempTriggerNP, nullptr);
        readSettings();
        return true;

    }
    if (!strcmp(name, FanSpeedNP.name)) {
        IUUpdateNumber(&FanSpeedNP, values, names, n);
        FanSpeedNP.s = IPS_BUSY;
        int speed = FanSpeedN[0].value;
        setFanSpeed(speed);
        FanSpeedNP.s = IPS_OK;
        IDSetNumber(&FanSpeedNP, nullptr);
        readSettings();
        return true;

    }

    if (!strcmp(name, LCDPageRefreshNP.name)) {
        IUUpdateNumber(&LCDPageRefreshNP, values, names, n);
        LCDPageRefreshNP.s = IPS_BUSY;
        int time = LCDPageRefreshN[0].value;
        setLCDPageRefreshRate(time);
        LCDPageRefreshNP.s = IPS_OK;
        IDSetNumber(&LCDPageRefreshNP, nullptr);
        readSettings();
        return true;

    }


    return INDI::DefaultDevice::ISNewNumber(dev, name, values, names, n);
}

bool myDewControllerPro::readSettings()
{

    char resp[MDCP_RES_LEN];
    float temp1, temp2, temp3, temp_ambient, dewpoint, humidity;


    if (!sendCommand(MDCP_GET_PROBE_TEMPS, resp)) {
        LOG_INFO(resp);
        return false;
    }

    if (sscanf(resp, MDCP_GET_TEMP_RESPONSE, &temp1, &temp2, &temp3) == 3) {
        TemperaturesN[PROBE_1].value = temp1;
        TemperaturesN[PROBE_2].value = temp2;
        TemperaturesN[PROBE_3].value = temp3;
        TemperaturesNP.s = IPS_OK;
        IDSetNumber(&TemperaturesNP, nullptr);
    }

    if (!sendCommand(MDCP_GET_AMB_TEMP, resp)) {
        LOG_INFO(resp);
        return false;
    }

    if (sscanf(resp, MDCP_GET_AMB_TEMP_REPSONSE, &temp_ambient) == 1) {
        TemperaturesN[AMBIENT_PROBE].value = temp_ambient;
        TemperaturesNP.s = IPS_OK;
        IDSetNumber(&TemperaturesNP, nullptr);
    }

    if (!sendCommand(MDCP_GET_BOARD_TEMP, resp)) {
        LOG_INFO(resp);
        return false;
    }

    if (sscanf(resp, MDCP_GET_BOARD_TEMP_RESPONSE, &temp_ambient) == 1) {
        TemperaturesN[BOARD_PROBE].value = temp_ambient;
        TemperaturesNP.s = IPS_OK;
        IDSetNumber(&TemperaturesNP, nullptr);
    }

    if (!sendCommand(MDCP_GET_REL_HUMIDITY, resp)) {
        LOG_INFO(resp);
        return false;
    }

    if (sscanf(resp, MDCP_GET_REL_HUMIDITY_REPSONSE, &humidity) == 1) {

        HumidityN[0].value = humidity;
        HumidityNP.s = IPS_OK;
        IDSetNumber(&HumidityNP, nullptr);
    } else {
        LOG_INFO(resp);
    }

    if (!sendCommand(MDCP_GET_DEW_POINT, resp)) {
        LOG_INFO(resp);
        return false;
    }

    if (sscanf(resp, MDCP_GET_DEW_POINT_RESPONSE, &dewpoint) == 1) {
        DewpointN[0].value = dewpoint;
        DewpointNP.s = IPS_OK;
        IDSetNumber(&DewpointNP, nullptr);
    }

    int power1, power2, power3;

    if (!sendCommand(MDCP_GET_CHANNEL_POWER, resp)) {
        LOG_INFO(resp);
        return false;
    }

    if (sscanf(resp, MDCP_GET_CHANNEL_POWER_RESPONSE, &power1, &power2, &power3) == 3) {
        OutputsN[DEW_STRAP_ONE_POWER].value = power1;
        OutputsN[DEW_STRAP_TWO_POWER].value = power2;
        OutputsN[DEW_STRAP_THREE_POWER].value = power3;
        OutputsNP.s = IPS_OK;
        IDSetNumber(&OutputsNP, nullptr);
        CH3_Manual_PowerN[0].value = power3;
        IDSetNumber(&CH3_Manual_PowerNP, nullptr);
    } else {
        LOG_INFO(resp);
    }

    int mode;

    if (!sendCommand(MDCP_GET_CH3_SETTINGS, resp)) {
        LOG_INFO(resp);
        return false;
    }

    if (sscanf(resp, MDCP_GET_CH3_SETTINGS_RESPONSE, &mode) == 1) {
        IUResetSwitch(&CH3_ModeSP);
        CH3_ModeS[mode].s = ISS_ON;
        CH3_ModeSP.s = IPS_OK;
        IDSetSwitch(&CH3_ModeSP, nullptr);
    } else {
        LOG_INFO(resp);
    }
    if (!sendCommand(MDCP_GET_TEMP_OFFSETS, resp)) {
        LOG_INFO(resp);
        return false;
    }

    if (sscanf(resp, MDCP_GET_TEMP_OFFSETS_RESPONSE, &temp1, &temp2, &temp3) == 3) {
        TemperatureOffsetsN[TEMP_PROBE_ONE_OFFSET].value = temp1;
        TemperatureOffsetsN[TEMP_PROBE_TWO_OFFSET].value = temp2;
        TemperatureOffsetsN[TEMP_PROBE_THREE_OFFSET].value = temp3;
        TemperatureOffsetsNP.s = IPS_OK;
        IDSetNumber(&TemperatureOffsetsNP, nullptr);
    }

    if (!sendCommand(MDCP_GET_AMB_TEMP_OFFSET, resp)) {
        LOG_INFO(resp);
        return false;
    }
    int atBias = 0;
    if (sscanf(resp, MDCP_GET_AMB_TEMP_OFFSET_RESPONSE, &atBias) == 1) {
        TemperatureOffsetsN[AMBIENT_TEMP_PROBE_OFFSET].value = atBias;
        TemperatureOffsetsNP.s = IPS_OK;
        IDSetNumber(&TemperatureOffsetsNP, nullptr);
    }
    if (!sendCommand(MDCP_GET_TRACKING_MODE, resp)) {
        LOG_INFO(resp);
        return false;
    }

    if (sscanf(resp, MDCP_GET_TRACKING_MODE_RESPONSE, &mode) == 1) {
        IUResetSwitch(&TrackingModeSP);
        TrackingModeS[mode].s = ISS_ON;
        TrackingModeSP.s = IPS_OK;
        IDSetSwitch(&TrackingModeSP, nullptr);
    }

    if (!sendCommand(MDCP_GET_TRACKING_MODE_OFFSET, resp)) {
        LOG_INFO(resp);
        return false;
    }
    int toffset = 0;

    if (sscanf(resp, "y%d$", &toffset) == 1) {
        TrackingModeOffsetN[0].value = toffset;
        TrackingModeOffsetNP.s = IPS_OK;
        IDSetNumber(&TrackingModeOffsetNP, nullptr);
    }

    if (!sendCommand(MDCP_GET_FAN_SPEED, resp)) {
        LOG_INFO(resp);
        return false;
    }
    int fanSpeed;

    if (sscanf(resp, "F%d$", &fanSpeed) == 1) {
        FanSpeedN[0].value = fanSpeed;
        FanSpeedNP.s = IPS_OK;
        IDSetNumber(&FanSpeedNP, nullptr);
    }

    if (!sendCommand(MDCP_GET_FAN_MODE, resp)) {
        LOG_INFO(resp);
        return false;
    }

    if (sscanf(resp, MDCP_GET_FAN_MODE_RESPONSE, &mode) == 1) {
        IUResetSwitch(&FanModeSP);

        FanModeS[mode].s = ISS_ON;
        FanModeSP.s = IPS_OK;
        IDSetSwitch(&FanModeSP, nullptr);
    }

    if (!sendCommand(MDCP_GET_FAN_ON_TEMP, resp)) {
        LOG_INFO(resp);
        return false;
    }

    int fanTemp;

    if (sscanf(resp, MDCP_GET_FAN_ON_TEMP_RESPONSE, &fanTemp) == 1) {
        FanTempTriggerN[FANTEMPON].value = fanTemp;
        FanTempTriggerNP.s = IPS_OK;
        IDSetNumber(&FanTempTriggerNP, nullptr);
    }

    if (!sendCommand(MDCP_GET_FAN_OFF_TEMP, resp)) {
        LOG_INFO(resp);
        return false;
    }

    if (sscanf(resp, MDCP_GET_FAN_OFF_TEMP_RESPONSE, &fanTemp) == 1) {
        FanTempTriggerN[FANTEMPOFF].value = fanTemp;
        FanTempTriggerNP.s = IPS_OK;
        IDSetNumber(&FanTempTriggerNP, nullptr);
    }
    if (!sendCommand(MDCP_GET_LCD_DISPLAY_TIME, resp)) {
        LOG_INFO(resp);
        return false;
    }

    if (sscanf(resp, MDCP_GET_LCD_DISPLAY_TIME_RESPONSE, &fanTemp) == 1) {
        LCDPageRefreshN[0].value = fanTemp;
        LCDPageRefreshNP.s = IPS_OK;
        IDSetNumber(&LCDPageRefreshNP, nullptr);
    }

    if (!sendCommand(MDCP_GET_LCD_STATE, resp)) {
        LOG_INFO(resp);
        return false;
    }

    if (sscanf(resp, MDCP_GET_LCD_STATE_RESPONSE, &mode) == 1) {
        IUResetSwitch(&EnableLCDDisplaySP);

        EnableLCDDisplayS[mode].s = ISS_ON;
        EnableLCDDisplaySP.s = IPS_OK;
        IDSetSwitch(&EnableLCDDisplaySP, nullptr);
    }

    if (!sendCommand(MDCP_GET_TEMP_DISPLAY, resp)) {
        LOG_INFO(resp);
        return false;
    }

    if (sscanf(resp, MDCP_GET_TEMP_DISPLAY_RESPONSE, &mode) == 1) {
        IUResetSwitch(&LCDDisplayTempUnitsSP);

        LCDDisplayTempUnitsS[mode-1].s = ISS_ON;
        LCDDisplayTempUnitsSP.s = IPS_OK;
        IDSetSwitch(&LCDDisplayTempUnitsSP, nullptr);
    }





         return true;
}

void myDewControllerPro::TimerHit()
{
    if (!isConnected())
    {
        return;
    }

    // Get temperatures etc.
    readSettings();
    SetTimer(getCurrentPollingPeriod());
}
