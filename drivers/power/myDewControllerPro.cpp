/*
    myDewControllerPro Driver
    Copyright (C) 2017-2023 Chemistorge

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
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA

*/

#include "myDewControllerPro.h"
#include "connectionplugins/connectionserial.h"
#include "indicom.h"

#include <termios.h>

#define MYDEWHEATERPRO_TIMEOUT 3
#define BOARD_FAN_TAB "Board Fan"
#define TEMPERATURE_OFFSETS_TAB "Temperature/Tracking Offsets"
#define LCD_DISPLAY_TAB "LCD Display"

std::unique_ptr<myDewControllerPro> mydewcontrollerpro(new myDewControllerPro());


myDewControllerPro::myDewControllerPro() : INDI::PowerInterface(this), INDI::WeatherInterface(this)
{
    setVersion(1, 0);
}

bool myDewControllerPro::initProperties()
{
    DefaultDevice::initProperties();

    setDriverInterface(AUX_INTERFACE | POWER_INTERFACE | WEATHER_INTERFACE);

    // Initialize PowerInterface with 3 dew ports and 1 auto dew port (for tracking mode)
    PI::initProperties(POWER_TAB, 0, 3, 0, 1, 0);
    // Initialize WeatherInterface
    WI::initProperties(ENVIRONMENT_TAB, ENVIRONMENT_TAB);

    // Set PowerInterface capabilities
    PI::SetCapability(POWER_HAS_DEW_OUT | POWER_HAS_AUTO_DEW);

    // Add WeatherInterface parameters
    WI::addParameter("AMBIENT_TEMPERATURE", "Ambient Temperature", -50, 70, 15);
    WI::addParameter("HUMIDITY", "Humidity", 0, 100, 15);
    WI::addParameter("DEWPOINT", "Dew Point", -50, 70, 15);
    WI::setCriticalParameter("AMBIENT_TEMPERATURE");

    FanSpeedNP[0].fill("Fan Power", "Fan Speed", "%4.0f %%", 0., 100., 1., 0.);
    FanSpeedNP.fill(getDeviceName(), "FanSpeed", "Board Fan", BOARD_FAN_TAB, IP_RW, 0, IPS_IDLE);

    FanModeSP[BOARD_TEMP].fill("Board Temp", "Board Temp Sensor", ISS_OFF);
    FanModeSP[MANUAL_FAN].fill("Manual", "Manual", ISS_ON);
    FanModeSP.fill(getDeviceName(), "Fan_Mode", "Fan Mode", BOARD_FAN_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    EEPROMSP[RESET_EEPROM].fill("Reset EEPROM", "Reset EEPROM to Defaults", ISS_OFF);
    EEPROMSP[SAVE_TO_EEPROM].fill("Save to EEPROM", "Save to EEPROM", ISS_OFF);
    EEPROMSP.fill(getDeviceName(), "EEPROM", "EEPROM", OPTIONS_TAB, IP_WO, ISR_ATMOST1, 0, IPS_IDLE);

    FanTempTriggerNP[FANTEMPOFF].fill("Board_Temp_Off", "Board Fan Temp Off", "%4.0f \u2103", 0., 100., 1., 0.);
    FanTempTriggerNP[FANTEMPON].fill("Board_Temp_On", "Board Fan Temp On", "%4.0f \u2103", 0., 100., 1., 0.);
    FanTempTriggerNP.fill(getDeviceName(), "Fan Trigger Temps", "Fan Trigger", BOARD_FAN_TAB, IP_RW, 0, IPS_IDLE);

    LCDPageRefreshNP[0].fill("Page Refresh Rate", "Page Refresh Rate", "%4.0f ms", 500., 5000., 500., 0.);
    LCDPageRefreshNP.fill(getDeviceName(), "LCD Page", "LCD Page", LCD_DISPLAY_TAB, IP_RW, 0, IPS_IDLE);

    LCDDisplayTempUnitsSP[CELCIUS].fill("Celsius", "Celsius", ISS_ON);
    LCDDisplayTempUnitsSP[FAHRENHEIT].fill("Fahrenheit", "Fahrenheit", ISS_OFF);
    LCDDisplayTempUnitsSP.fill(getDeviceName(), "Temp Units", "Temp Units", LCD_DISPLAY_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    EnableLCDDisplaySP[DISABLE_LCD].fill("Disabled", "Disabled", ISS_ON);
    EnableLCDDisplaySP[ENABLE_LCD].fill("Enabled", "Enabled", ISS_OFF);
    EnableLCDDisplaySP.fill(getDeviceName(), "LCD Status", "LCD Status", LCD_DISPLAY_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    /* Channel Manual and Boost */
    CH1CH2BoostSP[CH1_BOOST_100].fill("BOOST_CH1", "Strap 1 Boost 100%", ISS_OFF);
    CH1CH2BoostSP[CH2_BOOST_100].fill("BOOST_CH2", "Strap 2 Boost 100%", ISS_OFF);
    CH1CH2BoostSP.fill(getDeviceName(), "CHANNEL_BOOST", "Heat Boost", MAIN_CONTROL_TAB, IP_RW, ISR_NOFMANY, 0, IPS_IDLE);

    CH3_ModeSP[DISABLED_STRAP].fill("STRAP_DISABLED", "Strap Disabled", ISS_ON);
    CH3_ModeSP[DEWSTRAP_ONE].fill("SHADOW STRAP 1", "Shadow Strap 1", ISS_OFF);
    CH3_ModeSP[DEWSTRAP_TWO].fill("SHADOW STRAP 2", "Shadow Strap 2", ISS_OFF);
    CH3_ModeSP[MANUAL_STRAP].fill("Manual", "Manual", ISS_OFF);
    CH3_ModeSP[TEMP_PROBE_THREE].fill("TEMP_PROBE", "Temp Probe", ISS_OFF);
    CH3_ModeSP.fill(getDeviceName(), "CHANEL 3 SHAWDOW", "Strap 3 Mode", MAIN_CONTROL_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    CH3_Manual_PowerNP[0].fill("MANUAL_POWER", "Strap 3 Manual Power", "%4.0f %%", 0., 100., 1., 0.);
    CH3_Manual_PowerNP.fill(getDeviceName(), "CH3_POWER", "Strap 3 Power", MAIN_CONTROL_TAB, IP_RW, 0, IPS_IDLE);

    /* Temperatures */
    TemperaturesNP[PROBE_1].fill("CHANNEL1", "Strap 1", "%3.2f \u2103", -50., 70., 0., 0.);
    TemperaturesNP[PROBE_2].fill("CHANNEL2", "Strap 2", "%3.2f \u2103", -50., 70., 0., 0.);
    TemperaturesNP[PROBE_3].fill("CHANNEL3", "Strap 3", "%3.2f \u2103", -50., 70., 0., 0.);
    TemperaturesNP[BOARD_PROBE].fill("BOARD Temp", "Board", "%3.2f \u2103", -50., 100., 0., 0.);
    TemperaturesNP.fill(getDeviceName(), "TEMPERATURES", "Temperatures", MAIN_CONTROL_TAB, IP_RO, 0, IPS_IDLE);

    /* Temperature calibration values */
    TemperatureOffsetsNP[TEMP_PROBE_ONE_OFFSET].fill("CHANNEL1", "Strap 1", "%1.0f \u2103", -10., 10., 1., 0.);
    TemperatureOffsetsNP[TEMP_PROBE_TWO_OFFSET].fill("CHANNEL2", "Strap 2", "%1.0f \u2103", -10., 10., 1., 0.);
    TemperatureOffsetsNP[TEMP_PROBE_THREE_OFFSET].fill("CHANNEL3", "Strap 3", "%1.0f \u2103", -10., 10., 1., 0.);
    TemperatureOffsetsNP[AMBIENT_TEMP_PROBE_OFFSET].fill("AMBIENT", "Ambient", "%4.0f \u2103", -4, 3, 1, 0);
    TemperatureOffsetsNP.fill(getDeviceName(), "TEMP_CALIBRATIONS", "Temp Offsets", TEMPERATURE_OFFSETS_TAB, IP_RW, 0,
                              IPS_IDLE);

    ZeroTempOffsetsSP[0].fill("Zero_Temp", "Zero Temperature Offsets", ISS_OFF);
    ZeroTempOffsetsSP.fill(getDeviceName(), "Zero Offsets", "Zero Offsets", TEMPERATURE_OFFSETS_TAB, IP_RW, ISR_ATMOST1, 0,
                           IPS_IDLE);

    /* Tracking Mode Options */

    TrackingModeSP[AMBIENT].fill("AMBIENT", "Ambient", ISS_OFF);
    TrackingModeSP[DEWPOINT].fill("DEWPOINT", "Dew Point", ISS_ON);
    TrackingModeSP[MIDPOINT].fill("MIDPOINT", "Mid Point", ISS_OFF);
    TrackingModeSP.fill(getDeviceName(), "Tracking Mode", "Tracking Mode", TEMPERATURE_OFFSETS_TAB, IP_RW, ISR_1OFMANY, 0,
                        IPS_IDLE);

    TrackingModeOffsetNP[0].fill("Offset", "Offset", "%4.0f \u2103", -4, 3, 1, 0);
    TrackingModeOffsetNP.fill(getDeviceName(), "Tracking Offset", "Tracking Offset", TEMPERATURE_OFFSETS_TAB, IP_RW, 0,
                              IPS_IDLE);
    /* Firmware version */
    FWVersionNP[0].fill("FIRMWARE", "Firmware Version", "%4.0f", 0., 65535., 1., 0.);
    FWVersionNP.fill(getDeviceName(), "FW_VERSION", "Firmware", OPTIONS_TAB, IP_RO, 0, IPS_IDLE);

    addDebugControl();
    addConfigurationControl();
    setDefaultPollingPeriod(10000);
    addPollPeriodControl();

    // No simulation control for now


    serialConnection = new Connection::Serial(this);

    serialConnection->setDefaultBaudRate(serialConnection->B_57600);
    serialConnection->registerHandshake([&]()
    {
        return Handshake();
    });
    registerConnection(serialConnection);

    return true;
}

bool myDewControllerPro::SetDewPort(size_t port, bool enabled, double dutyCycle)
{
    INDI_UNUSED(port);
    // DEW ports are 0, 1, 2 for dew heaters 1, 2, 3 (device uses 1-based indexing)
    // Convert duty cycle percentage (0-100) to 0-255 range
    auto pwmValue = static_cast<uint8_t>(dutyCycle / 100.0 * 255.0);
    return setInt(enabled ? pwmValue : 0, "S%d#", "Failed to set Dew Port Power");
}

bool myDewControllerPro::SetAutoDewEnabled(size_t port, bool enabled)
{
    INDI_UNUSED(port); // myDewControllerPro has a single auto dew control (Tracking Mode)
    int mode = enabled ? DEWPOINT : AMBIENT; // Map enabled to DEWPOINT, disabled to AMBIENT
    return setInt(mode, MDCP_SET_TRACKING_MODE, "Failed to set Tracking Mode");
}

bool myDewControllerPro::cancelOutputBoost()
{
    if (sendCommand(MDCP_CANCEL_BOOST, nullptr))
    {
        return true;
    }
    else
    {
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
        // Update PowerInterface properties
        PI::updateProperties();
        // Update WeatherInterface properties
        WI::updateProperties();

        defineProperty(CH1CH2BoostSP);
        defineProperty(CH3_ModeSP);
        defineProperty(CH3_Manual_PowerNP);
        defineProperty(TemperaturesNP);
        defineProperty(HumidityNP);
        defineProperty(DewpointNP);
        defineProperty(FanSpeedNP);
        defineProperty(FanModeSP);
        defineProperty(TemperatureOffsetsNP);
        defineProperty(ZeroTempOffsetsSP);
        defineProperty(TrackingModeSP);
        defineProperty(TrackingModeOffsetNP);
        defineProperty(FanTempTriggerNP);
        defineProperty(EnableLCDDisplaySP);
        defineProperty(LCDDisplayTempUnitsSP);
        defineProperty(LCDPageRefreshNP);
        defineProperty(EEPROMSP);
        defineProperty(FWVersionNP);


        cancelOutputBoost();

        loadConfig(true);
        if (!readMainValues())
        {
            LOG_INFO("Reading Main Values Error");
        }
        if (!readLCDDisplayValues())
        {
            LOG_INFO("Reading LCD Display Values Error");
        }
        if (!readBoardFanValues())
        {
            LOG_INFO("Reading Board Fan Values Error");
        }
        if (!readOffsetValues())
        {
            LOG_INFO("Reading Offset Values Error");
        }
        LOG_INFO("myDewControllerPro parameters updated, device ready for use.");
        SetTimer(getCurrentPollingPeriod());
    }
    else
    {
        // Delete PowerInterface properties
        PI::updateProperties();
        // Delete WeatherInterface properties
        WI::updateProperties();

        deleteProperty(CH1CH2BoostSP);
        deleteProperty(CH3_ModeSP);
        deleteProperty(CH3_Manual_PowerNP);
        deleteProperty(TemperaturesNP);
        deleteProperty(HumidityNP);
        deleteProperty(DewpointNP);
        deleteProperty(FanSpeedNP);
        deleteProperty(FanModeSP);
        deleteProperty(TemperatureOffsetsNP);
        deleteProperty(ZeroTempOffsetsSP);
        deleteProperty(TrackingModeSP);
        deleteProperty(TrackingModeOffsetNP);
        deleteProperty(FanTempTriggerNP);
        deleteProperty(EnableLCDDisplaySP);
        deleteProperty(LCDDisplayTempUnitsSP);
        deleteProperty(LCDPageRefreshNP);
        deleteProperty(EEPROMSP);
        deleteProperty(FWVersionNP);
    }

    return true;
}

// Add saveConfigItems implementation
bool myDewControllerPro::saveConfigItems(FILE *fp)
{
    DefaultDevice::saveConfigItems(fp);
    PI::saveConfigItems(fp);
    WI::saveConfigItems(fp);

    // Save custom properties
    CH1CH2BoostSP.save(fp);
    CH3_ModeSP.save(fp);
    CH3_Manual_PowerNP.save(fp);
    TemperaturesNP.save(fp);
    HumidityNP.save(fp);
    DewpointNP.save(fp);
    FanSpeedNP.save(fp);
    FanModeSP.save(fp);
    TemperatureOffsetsNP.save(fp);
    ZeroTempOffsetsSP.save(fp);
    TrackingModeSP.save(fp);
    TrackingModeOffsetNP.save(fp);
    FanTempTriggerNP.save(fp);
    EnableLCDDisplaySP.save(fp);
    LCDDisplayTempUnitsSP.save(fp);
    LCDPageRefreshNP.save(fp);
    EEPROMSP.save(fp);
    FWVersionNP.save(fp);

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
    if (firmware < 340)
    {
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
    if (numberProbes < 1)
    {
        LOG_INFO("Warning no temperature probes detected");
    }
    FWVersionNP.setState(IPS_BUSY);
    FWVersionNP[0].setValue(firmware);
    FWVersionNP.setState(IPS_OK);
    FWVersionNP.apply();

    return true;

}

bool myDewControllerPro::setOutputBoost(unsigned int channel)
{



    if (channel == 0)
    {
        return sendCommand(MDCP_BOOST_CH1, nullptr);
    }
    else if (channel == 1)
    {
        return sendCommand(MDCP_BOOST_CH2, nullptr);
    }
    else
    {
        LOG_INFO("No Channel Set");
        return false;
    }

}

bool myDewControllerPro::setInt(int mode, const char *mask, const char *errMessage)
{
    char cmd[MDCP_CMD_LEN + 1];

    snprintf(cmd, MDCP_CMD_LEN + 1, mask, mode);
    if (!sendCommand(cmd, nullptr))
    {
        LOG_INFO(errMessage);
        LOG_INFO(cmd);
        return false;
    }
    return true;

}

bool myDewControllerPro::setChoice(int testInt, const char *positiveChoice, const char *negativeChoice,
                                   const char *errMessage)
{
    const char* mask = testInt == 1 ? positiveChoice : negativeChoice;
    if (!sendCommand(mask, nullptr))
    {
        LOG_INFO(errMessage);

        return false;
    }
    return true;
}



bool myDewControllerPro::setTempCalibrations(float ch1, float ch2, float ch3, int ambient)
{
    char cmd[MDCP_CMD_LEN + 1];


    snprintf(cmd, MDCP_CMD_LEN + 1, MDCP_SET_TEMP_CH1_OFFSET, ch1);
    if (!sendCommand(cmd, nullptr))
    {
        LOG_INFO("Failed to set CH1 offset");
        LOG_INFO(cmd);
        return false;
    }
    snprintf(cmd, MDCP_CMD_LEN + 1, MDCP_SET_TEMP_CH2_OFFSET, ch2);
    if (!sendCommand(cmd, nullptr))
    {
        LOG_INFO("Failed to set CH2 offset");
        LOG_INFO(cmd);
        return false;
    }
    snprintf(cmd, MDCP_CMD_LEN + 1, MDCP_SET_TEMP_CH3_OFFSET, ch3);
    if (!sendCommand(cmd, nullptr))
    {
        LOG_INFO("Failed to set CH3 offset");
        LOG_INFO(cmd);
        return false;
    }

    snprintf(cmd, MDCP_CMD_LEN + 1, MDCP_SET_AMB_TEMP_OFFSET, ambient);
    if (!sendCommand(cmd, nullptr))
    {
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
    if (!sendCommand(cmd, nullptr))
    {
        LOG_INFO("Failed to set temp on");
        LOG_INFO(cmd);
        return false;
    }
    snprintf(cmd, MDCP_CMD_LEN + 1, MDCP_SET_FAN_OFF_TEMP, tempOff);
    if (!sendCommand(cmd, nullptr))
    {
        LOG_INFO("Failed to set CH2 offset");
        LOG_INFO(cmd);
        return false;
    }

    return true;



}

bool myDewControllerPro::zeroTempCalibrations()
{

    if (!sendCommand(MDCP_CLEAR_TEMP_OFFSETS, nullptr))
    {
        LOG_INFO("Failed to zero temp offset");

        return false;
    }
    if (!sendCommand("e0#", nullptr))
    {
        LOG_INFO("Failed to zero ambtemp offset");

        return false;
    }
    return true;


}








bool myDewControllerPro::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if (!dev || strcmp(dev, getDeviceName()))
        return false;

    // Process PowerInterface switches
    if (PI::processSwitch(dev, name, states, names, n))
        return true;

    // Process WeatherInterface switches
    if (WI::processSwitch(dev, name, states, names, n))
        return true;

    if (CH1CH2BoostSP.isNameMatch(name))
    {
        CH1CH2BoostSP.update( states, names, n);
        CH1CH2BoostSP.setState(IPS_BUSY);
        cancelOutputBoost();
        if (CH1CH2BoostSP[CH1_BOOST_100].getState() == ISS_ON)
        {
            setOutputBoost(CH1_BOOST_100);
        }
        if (CH1CH2BoostSP[CH2_BOOST_100].getState() == ISS_ON)
        {
            setOutputBoost(CH2_BOOST_100);
        }
        CH1CH2BoostSP.setState(IPS_OK);
        CH1CH2BoostSP.apply();
        readMainValues();
        return true;

    }

    if (CH3_ModeSP.isNameMatch(name))
    {
        CH3_ModeSP.update(states, names, n);
        CH3_ModeSP.setState(IPS_BUSY);
        int mode = CH3_ModeSP.findOnSwitchIndex();
        setInt(mode, MDCP_SET_CH3_SETTINGS, "Failed to set CH3 mode");
        CH3_ModeSP.setState(IPS_OK);
        CH3_ModeSP.apply();
        readMainValues();
        return true;

    }

    if (ZeroTempOffsetsSP.isNameMatch(name))
    {
        ZeroTempOffsetsSP.update(states, names, n);
        ZeroTempOffsetsSP.setState(IPS_BUSY);
        zeroTempCalibrations();
        ZeroTempOffsetsSP.setState(IPS_OK);
        ZeroTempOffsetsSP[0].setState(ISS_OFF);
        ZeroTempOffsetsSP.apply();
        readOffsetValues();
        return true;
    }

    if (TrackingModeSP.isNameMatch(name))
    {
        TrackingModeSP.update(states, names, n);
        TrackingModeSP.setState(IPS_BUSY);
        int mode = TrackingModeSP.findOnSwitchIndex();
        setInt(mode, MDCP_SET_TRACKING_MODE, "Failed to set Tracking Mode");
        TrackingModeSP.setState(IPS_OK);
        TrackingModeSP.apply();
        readOffsetValues();
        return true;
    }

    if (FanModeSP.isNameMatch(name))
    {
        FanModeSP.update(states, names, n);
        FanModeSP.setState(IPS_BUSY);
        int mode = FanModeSP.findOnSwitchIndex();
        setInt(mode, MDCP_SET_FAN_MODE, "Failed to set Fan Mode");
        FanModeSP.setState(IPS_OK);
        FanModeSP.apply();
        readBoardFanValues();
        return true;
    }

    if (EnableLCDDisplaySP.isNameMatch(name))
    {
        EnableLCDDisplaySP.update(states, names, n);
        EnableLCDDisplaySP.setState(IPS_BUSY);
        int mode = EnableLCDDisplaySP.findOnSwitchIndex();
        setChoice(mode, MDCP_LCD_ENABLE, MDCP_LCD_DISABLE, "Failed to set LCD enable");
        EnableLCDDisplaySP.setState(IPS_OK);
        EnableLCDDisplaySP.apply();
        readLCDDisplayValues();
        return true;

    }

    if (LCDDisplayTempUnitsSP.isNameMatch(name))
    {
        LCDDisplayTempUnitsSP.update(states, names, n);
        LCDDisplayTempUnitsSP.setState(IPS_BUSY);
        int mode = LCDDisplayTempUnitsSP.findOnSwitchIndex();
        setChoice(mode, MDCP_LCD_DISPLAY_FAHRENHEIT, MDCP_LCD_DISPLAY_CELSIUS, "Failed to set temp display mode");
        LCDDisplayTempUnitsSP.setState(IPS_OK);
        LCDDisplayTempUnitsSP.apply();
        readLCDDisplayValues();
        return true;

    }

    if (EEPROMSP.isNameMatch(name))
    {
        EEPROMSP.update(states, names, n);
        EEPROMSP.setState(IPS_BUSY);
        int mode = EEPROMSP.findOnSwitchIndex();
        if (setChoice(mode, MDCP_SAVE_TO_EEPROM, MDCP_RESET_EEPROM_TO_DEFAULT, "Failed to Save/reset EEPROM"))
        {
            const char* message = mode == 1 ? "Saved to EEPPROM Successfully" : "Reset EEPROM to Default";
            LOG_INFO(message);
        }
        readMainValues();
        readOffsetValues();
        readBoardFanValues();
        readLCDDisplayValues();
        EEPROMSP.setState(IPS_OK);
        EEPROMSP.apply();
        return true;

    }
    return INDI::DefaultDevice::ISNewSwitch(dev, name, states, names, n);
}

bool myDewControllerPro::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    if (!dev || strcmp(dev, getDeviceName()))
        return false;

    // Process PowerInterface numbers
    if (PI::processNumber(dev, name, values, names, n))
        return true;

    // Process WeatherInterface numbers
    if (WI::processNumber(dev, name, values, names, n))
        return true;

    if (CH3_Manual_PowerNP.isNameMatch(name))
    {
        if (CH3_ModeSP.findOnSwitchIndex() == 3)
        {
            CH3_Manual_PowerNP.update(values, names, n);
            CH3_Manual_PowerNP.setState(IPS_BUSY);
            int power = CH3_Manual_PowerNP[0].getValue();
            setInt(power, MDCP_SET_CH3_MANUAL_POWER, "Failed to set CH3 Power");
            CH3_Manual_PowerNP.setState(IPS_OK);
            CH3_Manual_PowerNP.apply();
        }
        else
        {
            LOG_INFO("Power can only be manually adjusted in Strap 3 manual mode");
        }
        readMainValues();
        return true;
    }

    if (TemperatureOffsetsNP.isNameMatch(name))
    {
        TemperatureOffsetsNP.update(values, names, n);
        TemperatureOffsetsNP.setState(IPS_BUSY);
        int ch1 = TemperatureOffsetsNP[TEMP_PROBE_ONE_OFFSET].getValue();
        int ch2 = TemperatureOffsetsNP[TEMP_PROBE_TWO_OFFSET].getValue();
        int ch3 = TemperatureOffsetsNP[TEMP_PROBE_THREE_OFFSET].getValue();
        int ambient = TemperatureOffsetsNP[AMBIENT_TEMP_PROBE_OFFSET].getValue();
        setTempCalibrations(ch1, ch2, ch3, ambient);
        TemperatureOffsetsNP.setState(IPS_OK);
        TemperatureOffsetsNP.apply();
        readOffsetValues();
        return true;

    }

    if (TrackingModeOffsetNP.isNameMatch(name))
    {
        TrackingModeOffsetNP.update(values, names, n);
        TrackingModeOffsetNP.setState(IPS_BUSY);
        int offset = TrackingModeOffsetNP[0].getValue();
        setInt(offset, MDCP_SET_TRACKING_MODE_OFFSET, "Failed to set Tracking Mode offsets");
        TrackingModeOffsetNP.setState(IPS_OK);
        TrackingModeOffsetNP.apply();
        readOffsetValues();
        return true;
    }

    if (FanTempTriggerNP.isNameMatch(name))
    {
        FanTempTriggerNP.update(values, names, n);
        FanTempTriggerNP.setState(IPS_BUSY);
        int tempOn = FanTempTriggerNP[FANTEMPON].getValue();
        int tempOff = FanTempTriggerNP[FANTEMPOFF].getValue();
        setFanTempTrigger(tempOn, tempOff);
        FanTempTriggerNP.setState(IPS_OK);
        FanTempTriggerNP.apply();
        readBoardFanValues();
        return true;

    }
    if (FanSpeedNP.isNameMatch(name))
    {
        FanSpeedNP.update(values, names, n);
        FanSpeedNP.setState(IPS_BUSY);
        int speed = FanSpeedNP[0].getValue();
        setInt(speed, MDCP_SET_FAN_SPEED, "Failed to set Fan Speed");
        FanSpeedNP.setState(IPS_OK);
        FanSpeedNP.apply();
        readBoardFanValues();
        return true;

    }

    if (LCDPageRefreshNP.isNameMatch(name))
    {
        LCDPageRefreshNP.update(values, names, n);
        LCDPageRefreshNP.setState(IPS_BUSY);
        int time = LCDPageRefreshNP[0].getValue();
        setInt(time, MDCP_SET_LCD_DISPLAY_TIME, "Failed to set LCD Page refressh");
        LCDPageRefreshNP.setState(IPS_OK);
        LCDPageRefreshNP.apply();
        readLCDDisplayValues();
        return true;

    }

    return INDI::DefaultDevice::ISNewNumber(dev, name, values, names, n);
}

bool myDewControllerPro::readMainValues()
{

    char resp[MDCP_RES_LEN];
    float temp1, temp2, temp3, temp_ambient, dewpoint, humidity;


    if (!sendCommand(MDCP_GET_PROBE_TEMPS, resp))
    {
        LOG_INFO(resp);
        return false;
    }

    if (sscanf(resp, MDCP_GET_TEMP_RESPONSE, &temp1, &temp2, &temp3) == 3)
    {
        TemperaturesNP[PROBE_1].setValue(temp1);
        TemperaturesNP[PROBE_2].setValue(temp2);
        TemperaturesNP[PROBE_3].setValue(temp3);
        TemperaturesNP.setState(IPS_OK);
        TemperaturesNP.apply();
    }

    if (!sendCommand(MDCP_GET_AMB_TEMP, resp))
    {
        LOG_INFO(resp);
        return false;
    }

    if (sscanf(resp, MDCP_GET_AMB_TEMP_REPSONSE, &temp_ambient) == 1)
    {
        TemperaturesNP[AMBIENT_PROBE].setValue(temp_ambient);
        TemperaturesNP.setState(IPS_OK);
        TemperaturesNP.apply();
    }

    if (!sendCommand(MDCP_GET_BOARD_TEMP, resp))
    {
        LOG_INFO(resp);
        return false;
    }

    if (sscanf(resp, MDCP_GET_BOARD_TEMP_RESPONSE, &temp_ambient) == 1)
    {
        TemperaturesNP[BOARD_PROBE].setValue(temp_ambient);
        TemperaturesNP.setState(IPS_OK);
        TemperaturesNP.apply();
    }

    if (!sendCommand(MDCP_GET_REL_HUMIDITY, resp))
    {
        LOG_INFO(resp);
        return false;
    }

    if (sscanf(resp, MDCP_GET_REL_HUMIDITY_REPSONSE, &humidity) == 1)
    {

        HumidityNP[0].setValue(humidity);
        HumidityNP.setState(IPS_OK);
        HumidityNP.apply();
    }
    else
    {
        LOG_INFO(resp);
    }

    if (!sendCommand(MDCP_GET_DEW_POINT, resp))
    {
        LOG_INFO(resp);
        return false;
    }

    if (sscanf(resp, MDCP_GET_DEW_POINT_RESPONSE, &dewpoint) == 1)
    {
        DewpointNP[0].setValue(dewpoint);
        DewpointNP.setState(IPS_OK);
        DewpointNP.apply();
    }

    int power1, power2, power3;

    if (!sendCommand(MDCP_GET_CHANNEL_POWER, resp))
    {
        LOG_INFO(resp);
        return false;
    }

    if (sscanf(resp, MDCP_GET_CHANNEL_POWER_RESPONSE, &power1, &power2, &power3) == 3)
    {
        OutputsNP[DEW_STRAP_ONE_POWER].setValue(power1);
        OutputsNP[DEW_STRAP_TWO_POWER].setValue(power2);
        OutputsNP[DEW_STRAP_THREE_POWER].setValue(power3);
        OutputsNP.setState(IPS_OK);
        OutputsNP.apply();
        CH3_Manual_PowerNP[0].setValue(power3);
        CH3_Manual_PowerNP.apply();
    }
    else
    {
        LOG_INFO(resp);
    }

    int mode;

    if (!sendCommand(MDCP_GET_CH3_SETTINGS, resp))
    {
        LOG_INFO(resp);
        return false;
    }

    if (sscanf(resp, MDCP_GET_CH3_SETTINGS_RESPONSE, &mode) == 1)
    {
        CH3_ModeSP.reset();
        CH3_ModeSP[mode].setState(ISS_ON);
        CH3_ModeSP.setState(IPS_OK);
        CH3_ModeSP.apply();
    }
    else
    {
        LOG_INFO(resp);
    }

    if (!sendCommand(MDCP_GET_FAN_SPEED, resp))
    {
        LOG_INFO(resp);
        return false;
    }
    int fanSpeed;

    if (sscanf(resp, "F%d$", &fanSpeed) == 1)
    {
        FanSpeedNP[0].setValue(fanSpeed);
        FanSpeedNP.setState(IPS_OK);
        FanSpeedNP.apply();
    }

    return true;
}

bool myDewControllerPro::readOffsetValues()
{
    char resp[MDCP_RES_LEN];
    float temp1, temp2, temp3;


    if (!sendCommand(MDCP_GET_TEMP_OFFSETS, resp))
    {
        LOG_INFO(resp);
        return false;
    }

    if (sscanf(resp, MDCP_GET_TEMP_OFFSETS_RESPONSE, &temp1, &temp2, &temp3) == 3)
    {
        TemperatureOffsetsNP[TEMP_PROBE_ONE_OFFSET].setValue(temp1);
        TemperatureOffsetsNP[TEMP_PROBE_TWO_OFFSET].setValue(temp2);
        TemperatureOffsetsNP[TEMP_PROBE_THREE_OFFSET].setValue(temp3);
        TemperatureOffsetsNP.setState(IPS_OK);
        TemperatureOffsetsNP.apply();
    }

    if (!sendCommand(MDCP_GET_AMB_TEMP_OFFSET, resp))
    {
        LOG_INFO(resp);
        return false;
    }
    int atBias = 0;
    if (sscanf(resp, MDCP_GET_AMB_TEMP_OFFSET_RESPONSE, &atBias) == 1)
    {
        TemperatureOffsetsNP[AMBIENT_TEMP_PROBE_OFFSET].setValue(atBias);
        TemperatureOffsetsNP.setState(IPS_OK);
        TemperatureOffsetsNP.apply();
    }
    if (!sendCommand(MDCP_GET_TRACKING_MODE, resp))
    {
        LOG_INFO(resp);
        return false;
    }
    int mode;

    if (sscanf(resp, MDCP_GET_TRACKING_MODE_RESPONSE, &mode) == 1)
    {
        TrackingModeSP.reset();
        TrackingModeSP[mode].setState(ISS_ON);
        TrackingModeSP.setState(IPS_OK);
        TrackingModeSP.apply();
    }

    if (!sendCommand(MDCP_GET_TRACKING_MODE_OFFSET, resp))
    {
        LOG_INFO(resp);
        return false;
    }
    int toffset = 0;

    if (sscanf(resp, "y%d$", &toffset) == 1)
    {
        TrackingModeOffsetNP[0].setValue(toffset);
        TrackingModeOffsetNP.setState(IPS_OK);
        TrackingModeOffsetNP.apply();
    }
    return true;
}


bool myDewControllerPro::readBoardFanValues()
{
    char resp[MDCP_RES_LEN];

    if (!sendCommand(MDCP_GET_FAN_SPEED, resp))
    {
        LOG_INFO(resp);
        return false;
    }
    int fanSpeed;

    if (sscanf(resp, "F%d$", &fanSpeed) == 1)
    {
        FanSpeedNP[0].setValue(fanSpeed);
        FanSpeedNP.setState(IPS_OK);
        FanSpeedNP.apply();
    }

    if (!sendCommand(MDCP_GET_FAN_MODE, resp))
    {
        LOG_INFO(resp);
        return false;
    }
    int mode;
    if (sscanf(resp, MDCP_GET_FAN_MODE_RESPONSE, &mode) == 1)
    {
        FanModeSP.reset();
        FanModeSP[mode].setState(ISS_ON);
        FanModeSP.setState(IPS_OK);
        FanModeSP.apply();
    }

    if (!sendCommand(MDCP_GET_FAN_ON_TEMP, resp))
    {
        LOG_INFO(resp);
        return false;
    }

    int fanTemp;

    if (sscanf(resp, MDCP_GET_FAN_ON_TEMP_RESPONSE, &fanTemp) == 1)
    {
        FanTempTriggerNP[FANTEMPON].setValue(fanTemp);
        FanTempTriggerNP.setState(IPS_OK);
        FanTempTriggerNP.apply();
    }

    if (!sendCommand(MDCP_GET_FAN_OFF_TEMP, resp))
    {
        LOG_INFO(resp);
        return false;
    }

    if (sscanf(resp, MDCP_GET_FAN_OFF_TEMP_RESPONSE, &fanTemp) == 1)
    {
        FanTempTriggerNP[FANTEMPOFF].setValue(fanTemp);
        FanTempTriggerNP.setState(IPS_OK);
        FanTempTriggerNP.apply();
    }

    return true;
}

bool myDewControllerPro::readLCDDisplayValues()
{
    char resp[MDCP_RES_LEN];
    int value;

    if (!sendCommand(MDCP_GET_LCD_DISPLAY_TIME, resp))
    {
        LOG_INFO(resp);
        return false;
    }
    if (sscanf(resp, MDCP_GET_LCD_DISPLAY_TIME_RESPONSE, &value) == 1)
    {
        LCDPageRefreshNP[0].setValue(value);
        LCDPageRefreshNP.setState(IPS_OK);
        LCDPageRefreshNP.apply();
    }

    if (!sendCommand(MDCP_GET_LCD_STATE, resp))
    {
        LOG_INFO(resp);
        return false;
    }

    if (sscanf(resp, MDCP_GET_LCD_STATE_RESPONSE, &value) == 1)
    {
        EnableLCDDisplaySP.reset();
        EnableLCDDisplaySP[value].setState(ISS_ON);
        EnableLCDDisplaySP.setState(IPS_OK);
        EnableLCDDisplaySP.apply();
    }

    if (!sendCommand(MDCP_GET_TEMP_DISPLAY, resp))
    {
        LOG_INFO(resp);
        return false;
    }

    if (sscanf(resp, MDCP_GET_TEMP_DISPLAY_RESPONSE, &value) == 1)
    {
        LCDDisplayTempUnitsSP.reset();
        LCDDisplayTempUnitsSP[value - 1].setState(ISS_ON);
        LCDDisplayTempUnitsSP.setState(IPS_OK);
        LCDDisplayTempUnitsSP.apply();
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
    readMainValues();
    SetTimer(getCurrentPollingPeriod());
}
