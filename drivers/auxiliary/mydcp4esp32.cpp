/*
    myDCP4ESP32 
    Copyright (C) 2023 Stephen Hillier

    Based on MyFocuserPro2 Focuser
    Copyright (C) 2019 Alan Townshend

    As well as USB_Dewpoint
    Copyright (C) 2017-2023 Jarno Paananen

    And INDI Sky Quality Meter Driver
    Copyright(c) 2016 Jasem Mutlaq. All rights reserved.

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

#include "mydcp4esp32.h"
#include "connectionplugins/connectiontcp.h"
#include "connectionplugins/connectionserial.h"
#include "indicom.h"

#include <cstring>
#include <string>
#include <memory>

#include <termios.h>
#include <unistd.h>

#define USBDEWPOINT_TIMEOUT 3

std::unique_ptr<MyDCP4ESP> mydcp4esp(new MyDCP4ESP());

MyDCP4ESP::MyDCP4ESP()
{

    setVersion(CDRIVER_VERSION_MAJOR, CDRIVER_VERSION_MINOR);

}

bool MyDCP4ESP::initProperties()
{
    DefaultDevice::initProperties();

    /* Channel duty cycles */
    ChannelPowerNP[0].fill("CHANNEL1", "Channel 1", "%3.0f", 0., 100., 0., 0.);
    ChannelPowerNP[1].fill("CHANNEL2", "Channel 2", "%3.0f", 0., 100., 0., 0.);
    ChannelPowerNP[2].fill("CHANNEL3", "Channel 3", "%3.0f", 0., 100., 0., 0.);
    ChannelPowerNP[3].fill("CHANNEL4", "Channel 4", "%3.0f", 0., 100., 0., 0.);
    ChannelPowerNP.fill(getDeviceName(), "OUTPUT", "Power", MAIN_CONTROL_TAB, IP_RO, 0, IPS_IDLE);

    /* Temperature Probe Found status */
    TempProbeFoundSP[0].fill("PROBE1", "Probe 1", ISS_OFF);
    TempProbeFoundSP[1].fill("PROBE2", "Probe 2", ISS_OFF);
    TempProbeFoundSP[2].fill("PROBE3", "Probe 3", ISS_OFF);
    TempProbeFoundSP[3].fill("PROBE4", "Probe 4", ISS_OFF);
    TempProbeFoundSP.fill(getDeviceName(), "SENSORS", "Sensors", MAIN_CONTROL_TAB, IP_RO, ISR_NOFMANY, 0, IPS_IDLE);

    /* Temperatures */
    TemperatureNP[0].fill("CHANNEL1", "Channel 1", "%3.2f", -50., 120., 0., 0.);
    TemperatureNP[1].fill("CHANNEL2", "Channel 2", "%3.2f", -50., 120., 0., 0.);
    TemperatureNP[2].fill("CHANNEL3", "Channel 3", "%3.2f", -50., 120., 0., 0.);
    TemperatureNP[3].fill("CHANNEL4", "Channel 4", "%3.2f", -50., 120., 0., 0.);
    TemperatureNP.fill(getDeviceName(), "TEMPERATURE", "Temperature", MAIN_CONTROL_TAB, IP_RO, 0, IPS_IDLE);

    /* Ambient Temperature */
    AmbientTemperatureNP[0].fill("AMBIENT", "Ambient", "%3.2f", 0., 100., 0., 0.);
    AmbientTemperatureNP.fill(getDeviceName(), "AMBIENT", "Temperature", MAIN_CONTROL_TAB, IP_RO, 0, IPS_IDLE);

    /* Humidity */
    HumidityNP[0].fill("HUMIDITY", "Humidity", "%3.2f", 0., 100., 0., 0.);
    HumidityNP.fill(getDeviceName(), "HUMIDITY", "Humidity", MAIN_CONTROL_TAB, IP_RO, 0, IPS_IDLE);

    /* Dew point */
    DewpointNP[0].fill("DEWPOINT", "Dew point", "%3.2f", -50., 120., 0., 0.);
    DewpointNP.fill(getDeviceName(), "DEWPOINT", "Dew point", MAIN_CONTROL_TAB, IP_RO, 0, IPS_IDLE);

    /* Temperature calibration values */
    ChannelOffsetNP[0].fill("CHANNEL1", "Channel 1", "%1.2f", -5., 5., 0.25, 0.);
    ChannelOffsetNP[1].fill("CHANNEL2", "Channel 2", "%1.2f", -5., 5., 0.25, 0.);
    ChannelOffsetNP[2].fill("CHANNEL3", "Channel 3", "%1.2f", -5., 5., 0.25, 0.);
    ChannelOffsetNP[3].fill("CHANNEL4", "Channel 4", "%1.2f", -5., 5., 0.25, 0.);
    ChannelOffsetNP.fill(getDeviceName(), "TEMPOFFSET", "T Offset", OPTIONS_TAB, IP_RW, 0, IPS_IDLE);

    /* Ambient Temperature Offset */
    AmbientOffsetNP[0].fill("AMBIENT", "Ambient", "%1.2f", -4., 3., 0.25, 0.);
    AmbientOffsetNP.fill(getDeviceName(), "AMBIENTOFFSET", "T Offset", OPTIONS_TAB, IP_RW, 0, IPS_IDLE);

    /*  Tracking Offset */
    TrackingOffsetNP[0].fill("TRACKING", "Tracking", "%1.0f", -4, 3, 1, 0);
    TrackingOffsetNP.fill(getDeviceName(), "TRACKING", "T Offset", OPTIONS_TAB, IP_RW, 0, IPS_IDLE);

    /*  Tracking mode */
    TrackingModeSP[0].fill("AMBIENT", "Ambient", ISS_ON);
    TrackingModeSP[1].fill("DEWPOINT", "Dewpoint", ISS_OFF);
    TrackingModeSP[2].fill("MIDPOINT", "Midpoint", ISS_OFF);
    TrackingModeSP.fill(getDeviceName(), "TRACKINGMODE", "Tracking", MAIN_CONTROL_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    /* Channel 3 operating Mode */
    Ch3ModeSP[0].fill("DISABLED", "Disabled", ISS_OFF);
    Ch3ModeSP[1].fill("CHANNEL1", "Channel 1", ISS_OFF);
    Ch3ModeSP[2].fill("CHANNEL2", "Channel 2", ISS_OFF);
    Ch3ModeSP[3].fill("MANUAL", "Manual", ISS_OFF);
    Ch3ModeSP[4].fill("CHANNEL3", "Channel 3", ISS_ON);
    Ch3ModeSP.fill(getDeviceName(), "CH3MODE", "Ch3 Mode", OPTIONS_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    /* Channel 3 Manual Power Setting */
    Ch3ManualPowerNP[0].fill("CH3MANUAL", "Power", "%3.0f", 0, 100, 10, 0);
    Ch3ManualPowerNP.fill(getDeviceName(), "CH3MANUAL", "Ch3 Manual", OPTIONS_TAB, IP_RW, 0, IPS_IDLE);
    
    /* Channel 100% Boost On/Off */
    ChannelBoostSP[0].fill("CHANNEL1", "Channel 1", ISS_OFF);
    ChannelBoostSP[1].fill("CHANNEL2", "Channel 2", ISS_OFF);
    ChannelBoostSP[2].fill("CHANNEL3", "Channel 3", ISS_OFF);
    ChannelBoostSP[3].fill("CHANNEL4", "Channel 4", ISS_OFF);
    ChannelBoostSP[4].fill("RESETALL", "Reset All", ISS_OFF);
    ChannelBoostSP.fill(getDeviceName(), "CHANNELBOOST", "100% Boost", OPTIONS_TAB, IP_RW, ISR_NOFMANY, 0, IPS_IDLE);

    /* Reset settings */
    RebootSP[0].fill("REBOOT", "Reboot", ISS_OFF);
    RebootSP.fill(getDeviceName(), "REBOOT", "Controller", CONNECTION_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    /* Firmware version */
    FWversionNP[0].fill("FIRMWARE", "Firmware Version", "%3.0f", 0, 999, 0, 0);
    FWversionNP.fill(getDeviceName(), "FW_VERSION", "Firmware", CONNECTION_TAB, IP_RO, 0, IPS_IDLE);

    /* Controller check code */
    CheckCodeTP[0].fill("CNTR_CODE", "Handshake Code", nullptr);
    CheckCodeTP.fill(getDeviceName(), "CNTR_CODE", "Controller", CONNECTION_TAB, IP_RO, 0, IPS_IDLE);

    setDriverInterface(AUX_INTERFACE);

    addDebugControl();
    addConfigurationControl();
    setDefaultPollingPeriod(10000);
    addPollPeriodControl();

    // No simulation control for now

    if (mdcpConnection & CONNECTION_SERIAL)
    {
        serialConnection = new Connection::Serial(this);
        serialConnection->registerHandshake([&]()
        {
            return Handshake();
        });
        serialConnection->setDefaultBaudRate(Connection::Serial::B_57600);
        registerConnection(serialConnection);
    }

    if (mdcpConnection & CONNECTION_TCP)
    {
        tcpConnection = new Connection::TCP(this);
        tcpConnection->setDefaultHost("192.168.4.1");
        tcpConnection->setDefaultPort(3131);
        tcpConnection->registerHandshake([&]()
        {
            return Handshake();
        });

        registerConnection(tcpConnection);
    }


    return true;
}

bool MyDCP4ESP::updateProperties()
{
    DefaultDevice::updateProperties();

    if (isConnected())
    {
        defineProperty(TrackingModeSP);
        if (myDCP4Firmware > 109) // Firmware 109 has a bug with setting Tracking offsets
            defineProperty(TrackingOffsetNP);
        defineProperty(AmbientTemperatureNP);
        defineProperty(AmbientOffsetNP);
        defineProperty(HumidityNP);
        defineProperty(DewpointNP);
        defineProperty(TempProbeFoundSP);
        defineProperty(TemperatureNP);
        defineProperty(ChannelPowerNP);
        defineProperty(ChannelOffsetNP); 
        if (myDCP4Firmware > 109) // Firmware 109 has a bug with the 100% boost settings
            defineProperty(ChannelBoostSP);
        defineProperty(Ch3ModeSP);
        defineProperty(RebootSP);
        defineProperty(FWversionNP);
        defineProperty(CheckCodeTP);
        ch3ManualPower = false;
        loadConfig(true);
        readSettings();
        LOG_INFO("myDCP4ESP32 parameters updated, device ready for use.");
        timerIndex = SetTimer(getCurrentPollingPeriod());
    }
    else
    {
        deleteProperty(TrackingModeSP);
        if (myDCP4Firmware > 109) // Firmware 109 has a bug with setting Tracking offsets
            deleteProperty(TrackingOffsetNP);
        deleteProperty(AmbientTemperatureNP);
        deleteProperty(AmbientOffsetNP);
        deleteProperty(HumidityNP);
        deleteProperty(DewpointNP);
        deleteProperty(TempProbeFoundSP);
        deleteProperty(TemperatureNP);
        deleteProperty(ChannelPowerNP);
        deleteProperty(ChannelOffsetNP);
        if (myDCP4Firmware > 109) // Firmware 109 has a bug with the 100% boost settings
            deleteProperty(ChannelBoostSP);
        deleteProperty(Ch3ModeSP);
        deleteProperty(RebootSP);
        deleteProperty(FWversionNP);
        deleteProperty(CheckCodeTP);
        if (ch3ManualPower == true)
        {
            deleteProperty(Ch3ManualPowerNP);
            ch3ManualPower = false;
        }
    }

    return true;
}

const char *MyDCP4ESP::getDefaultName()
{
    return "MyDCP4ESP32";
}

// sleep for a number of milliseconds
int MyDCP4ESP::msleep( long duration)
{
    struct timespec ts;
    int res;

    if (duration < 0)
    {
        errno = EINVAL;
        return -1;
    }

    ts.tv_sec = duration / 1000;
    ts.tv_nsec = (duration % 1000) * 1000000;

    do
    {
        res = nanosleep(&ts, &ts);
    }
    while (res && errno == EINTR);

    return res;
}

bool MyDCP4ESP::sendCommand(const char *cmd, char *resp)
{
    int nbytes_written = 0, nbytes_read = 0, rc = -1;
    char errstr[MAXRBUF];

    LOGF_DEBUG("CMD <%s>", cmd);
    tcflush(PortFD, TCIOFLUSH);
    if ((rc = tty_write_string(PortFD, cmd, &nbytes_written)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("Error writing command %s: %s.", cmd, errstr);
        return false;
    }

    // Small delay to allow controller to process command
    msleep(MDCP_SMALL_DELAY);
    
    if (resp)
    {
        if ((rc = tty_nread_section(PortFD, resp, MDCP_RESPONSE_LENGTH, '#', MDCP_READ_TIMEOUT, &nbytes_read)) != TTY_OK)
        {
            tty_error_msg(rc, errstr, MAXRBUF);
            LOGF_ERROR("Error reading response for command <%s>: %s.", cmd, errstr);
            return false;
        }

        if (nbytes_read < 2)
        {
            LOGF_ERROR("Invalid response <%s> for command <%s>.", resp, cmd);
            return false;
        }

        LOGF_DEBUG("RESP <%s>", resp);
        resp[nbytes_read - 1] = '\0'; // Strip "#" termination character
    }

    return true;
}

// Determine which of the 4 channels have temperature probes attached. Only those with probes can be active
// except for Channel 3 which can mirrior Channels 1 & 2 or be controlled manually.
// Check to see if each channel can be set to Override if it doesn't currently have a power output
bool MyDCP4ESP::getActiveChannels()
{
    char cmd[MDCP_CMD_LENGTH] = {};
    char resp[MDCP_RESPONSE_LENGTH] = {};
    int output[4] = {0, 0, 0, 0};
    int ok, i; 
    unsigned int channel_boost = 0;
    unsigned int currentCh3Mode = 0;
    
    // Default all channels to active in case channel testing fails
    i = 0;
    while (i <= 3)
    {
        channelActive[i] = 1;
        TempProbeFoundSP[i].setState(ISS_ON);
        i++;
    }

    TempProbeFoundSP.setState(IPS_IDLE); // Keep state Idle unless positive exit
    TempProbeFoundSP.apply();

    if (myDCP4Firmware > 109)
    {
        // TempProbeFoundSP.setState(IPS_BUSY);
        
        // Get current channel output to trim the test to those at zero
        if (!sendCommand(MDCP_GET_ALL_CH_POWER_CMD, resp))
            return false;

        ok = sscanf(resp, MDCP_GET_ALL_CH_POWER_RES, &output[0], &output[1], &output[2], &output[3] );

        if (ok == 4)
        {
        
            i = 0;

            while (i <=3 ) // Go through each channel
            {

                if ((output[i] == 0) || (i == 2)) // Only do test if Channel power is zero or is Channel 3 (i=2)
                {
                    if (i == 2) // Channel 3 (i=2) is special and needs to be in use temp probe mode to test
                    {
                        memset(resp, '\0', MDCP_RESPONSE_LENGTH); 
    
                        if (!sendCommand(MDCP_GET_CH3_MODE_CMD, resp))
                            return false;

                        ok = sscanf(resp, MDCP_GET_CH3_MODE_RES, &currentCh3Mode);

                        if ((ok != 1) || (currentCh3Mode > 4))
                            return false;

                        if (currentCh3Mode != CH3MODE_CH3TEMP)
                            if (!setCh3Mode( CH3MODE_CH3TEMP ))
                                return false;
                    }
                        
                    if (!setChannelBoost( i+1, 1))
                        return false;

                    snprintf(cmd, MDCP_CMD_LENGTH, MDCP_GET_CH_OVERIDE_CMD, i+1);

                    memset(resp, '\0', MDCP_RESPONSE_LENGTH);

                    if (!sendCommand(cmd, resp))
                        return false;

                    ok = sscanf(resp, MDCP_GET_CH_OVERIDE_RES, &channel_boost);

                    if ((ok == 1) && (channel_boost == 0))
                    {
                        TempProbeFoundSP[i].setState(ISS_OFF);
                        channelActive[i] = 0;
                    }

                    if (!setChannelBoost( i+1, 0))
                        return false;
                    
                    if ((i == 2) && (currentCh3Mode != CH3MODE_CH3TEMP)) // Return Ch3 to previous mode
                        if (!setCh3Mode(currentCh3Mode))
                            return false;

                }

                i++;
            }
        }
        else
            return false;
    }
    else
        return false;
    
    TempProbeFoundSP.setState(IPS_OK);
    TempProbeFoundSP.apply();
    return true;
}

bool MyDCP4ESP::Handshake()
{
     if (getActiveConnection() == serialConnection)
    {
        PortFD = serialConnection->getPortFD();
    }
    else if (getActiveConnection() == tcpConnection)
    {
        PortFD = tcpConnection->getPortFD();
    }
    
    int tries = 2;
    do
    {
        if (Ack())
        {
            LOG_INFO("myDCP4ESP32 is online. Getting device parameters...");
            if (!getActiveChannels())
                LOG_INFO("Could not determine active channels. Default to all active.");
            return true;
        }
        LOG_INFO("Error retrieving data from myDCP4ESP32, retrying...");
    }
    while (--tries > 0 );

    LOG_INFO("Error retrieving data from myDCP4ESP32, please ensure controller "
             "is powered and the port is correct.");
  
    return false;
}

bool MyDCP4ESP::Ack()
{
    
    char resp[MDCP_RESPONSE_LENGTH] = {};
    int firmware;
    char code[10] = {};


    if (!sendCommand(MDCP_GET_CONTROLLER_CODE_CMD, resp))
        return false;


    int ok = sscanf(resp, MDCP_GET_CONTROLLER_CODE_RES, code);

    if (ok != 1)
    {
        LOGF_ERROR("Get Handshake Code: Response <%s> for Command <%s> not recognized.", resp, MDCP_GET_CONTROLLER_CODE_CMD);
        return false;
    }

    CheckCodeTP[0].setText(code);
    CheckCodeTP.setState(IPS_OK);
    CheckCodeTP.apply();
        
    tcflush(PortFD, TCIOFLUSH);
    memset(resp, '\0', MDCP_RESPONSE_LENGTH);

    if (!sendCommand(MDCP_GET_VERSION_CMD, resp))
        return false;

    ok = sscanf(resp, MDCP_GET_VERSION_RES, &firmware);

    if (ok != 1)
    {
        LOGF_ERROR("Get Firmware Version: Response <%s> for Command <%s> not recognized.", resp, MDCP_GET_VERSION_CMD);
        return false;
    }
  
    myDCP4Firmware = firmware;
    FWversionNP[0].setValue(firmware);
    FWversionNP.setState(IPS_OK);
    FWversionNP.apply();
    return true;
}

// Set the temperature offset for a channel
bool MyDCP4ESP::setChannelOffset(unsigned int channel, float value)
{
    char cmd[MDCP_CMD_LENGTH] = {};

    switch (channel) {
        case 1:
            snprintf(cmd, MDCP_CMD_LENGTH, MDCP_SET_CH1_OFFSET_CMD, value);
            break;

        case 2:
            snprintf(cmd, MDCP_CMD_LENGTH, MDCP_SET_CH2_OFFSET_CMD, value);
            break;

        case 3:
            snprintf(cmd, MDCP_CMD_LENGTH, MDCP_SET_CH3_OFFSET_CMD, value);
            break;

        case 4:
            snprintf(cmd, MDCP_CMD_LENGTH, MDCP_SET_CH4_OFFSET_CMD, value);
            break;

        default:
            return false;
    }
    
    return sendCommand(cmd, nullptr);
}

// Set the offset for ambient temperature
bool MyDCP4ESP::setAmbientOffset(float value)
{
    char cmd[MDCP_CMD_LENGTH] = {};

    snprintf(cmd, MDCP_CMD_LENGTH, MDCP_SET_AMBIENT_OFFSET_CMD, value);
    return sendCommand(cmd, nullptr); 
}

// set or reset Channel override. Channel = 5 resets all channels
bool MyDCP4ESP::setChannelBoost( unsigned int channel, unsigned int value)
{
    char cmd[MDCP_CMD_LENGTH] = {};
    if ((channel == 5) || (value == 0))
    {
        snprintf(cmd, MDCP_CMD_LENGTH, MDCP_RESET_CH_100_CMD, channel);
        return sendCommand(cmd, nullptr); 
    }
    else if (channel != 5)
    {
        snprintf(cmd, MDCP_CMD_LENGTH, MDCP_SET_CH_100_CMD, channel);
        return sendCommand(cmd, nullptr);
    }

    return false;
}

// Set Tracking Mode (1=Ambient, 2=Dewpoint, 3=Midpoint)
bool MyDCP4ESP::setTrackingMode(unsigned int value)
{
    char cmd[MDCP_CMD_LENGTH] = {};

    snprintf(cmd, MDCP_CMD_LENGTH, MDCP_SET_TRACKING_MODE_CMD, value);
    return sendCommand(cmd, nullptr); 
}

// Set the mode for Channel 3 control (0=disabled, 1=Channel 1, 2=Channel 2, 3=Manual, 4=use temp probe3)
bool MyDCP4ESP::setCh3Mode(unsigned int value)
{
    char cmd[MDCP_CMD_LENGTH] = {};

    snprintf(cmd, MDCP_CMD_LENGTH, MDCP_SET_CH3_MODE_CMD, value);
    return sendCommand(cmd, nullptr); 
}

// Set Channel 3 power output - Channel 3 must be in Manual mode.
bool MyDCP4ESP::setCh3Output(unsigned int value)
{
    char cmd[MDCP_CMD_LENGTH] = {};

    snprintf(cmd, MDCP_CMD_LENGTH, MDCP_SET_CH3_MANUAL_POWER_CMD, value);
    return sendCommand(cmd, nullptr); 
}

// Set Tracking Offset
bool MyDCP4ESP::setTrackingOffset(int value)
{
    char cmd[MDCP_CMD_LENGTH] = {};

    snprintf(cmd, MDCP_CMD_LENGTH, MDCP_SET_TRACKING_OFFSET_CMD, value);
    return sendCommand(cmd, nullptr); 
}

// Reboot the Dew Controller then wait to reconnect
bool MyDCP4ESP::rebootController()
{   
    LOG_INFO("Rebooting Controller and Disconnecting.");
    sendCommand(MDCP_REBOOT_CMD, nullptr);

    if (!Disconnect())
        LOG_INFO("Disconnect failed");
    setConnected(false, IPS_IDLE);
    updateProperties();
    LOG_INFO("Waiting 10 seconds before attempting to reconnect.");
    RemoveTimer(timerIndex);

    int i = 1;
    do
    {
        sleep(10);
        if (!Connect()) 
        {
            i++;
            if (i <= 5)            
                LOGF_INFO("Could not reconnect waiting 10 seconds before attempt %d of 5.", i);
            else
            {
                LOGF_ERROR("Could not reconnect after %d attempts", i-1);
                setConnected(false, IPS_OK);
            }
        }
        else
        {
            i = 0;
            setConnected(true, IPS_OK);
        }
    } while ((i != 0) && (i <= 5));

    return updateProperties();
}

bool MyDCP4ESP::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    
    if (!dev || strcmp(dev, getDeviceName()))
        return false;

    if (ChannelBoostSP.isNameMatch(name))
    {
        if (states[4] == ISS_ON) // Reset all to ISS_OFF
        {
            setChannelBoost(5, 1);
        }
        else // Check current versus new state and only invoke for changes
        {
            for (int i = 0; i < 4; i++)
            {
                if (ChannelBoostSP[i].getState() != states[i] )
                    setChannelBoost(i+1, states[i]);
            }
            
        } 
        ChannelBoostSP.update(states, names, n);
        ChannelBoostSP.setState(IPS_BUSY);
        ChannelBoostSP.apply();
        readSettings();
        return true;
    }

    if (TrackingModeSP.isNameMatch(name))
    {
        TrackingModeSP.update(states, names, n);
        TrackingModeSP.setState(IPS_BUSY);
        TrackingModeSP.apply();
        setTrackingMode(TrackingModeSP.findOnSwitchIndex() + 1);
        readSettings();
        return true;
    }

    if (Ch3ModeSP.isNameMatch(name))
    {
        Ch3ModeSP.update(states, names, n);
        Ch3ModeSP.setState(IPS_BUSY);
        Ch3ModeSP.apply();
        setCh3Mode(Ch3ModeSP.findOnSwitchIndex());
        readSettings();
        return true;
    }

    if (RebootSP.isNameMatch(name))
    {
        RebootSP.reset();

        if (rebootController())
        {
            RebootSP.setState(IPS_OK);
        }
        else
        {
            RebootSP.setState(IPS_ALERT);
        }

        RebootSP.apply();
        return true;
    }

    return INDI::DefaultDevice::ISNewSwitch(dev, name, states, names, n);
}

bool MyDCP4ESP::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    
    if (!dev || strcmp(dev, getDeviceName()))
        return false;

    if (ChannelOffsetNP.isNameMatch(name))
    {
        ChannelOffsetNP.update(values, names, n);
        ChannelOffsetNP.setState(IPS_BUSY);
        ChannelOffsetNP.apply();
        setChannelOffset(1, ChannelOffsetNP[0].getValue());
        setChannelOffset(2, ChannelOffsetNP[1].getValue());
        setChannelOffset(3, ChannelOffsetNP[2].getValue());
        setChannelOffset(4, ChannelOffsetNP[3].getValue());
        readSettings();
        return true;
    }
    
    if (AmbientOffsetNP.isNameMatch(name))
    {
        AmbientOffsetNP.update(values, names, n);
        AmbientOffsetNP.setState(IPS_BUSY);
        AmbientOffsetNP.apply();
        setAmbientOffset(AmbientOffsetNP[0].getValue());
        readSettings();
        return true;
    }

    if (TrackingOffsetNP.isNameMatch(name))
    {
        TrackingOffsetNP.update(values, names, n);
        TrackingOffsetNP.setState(IPS_BUSY);
        TrackingOffsetNP.apply();
        setTrackingOffset(TrackingOffsetNP[0].getValue());
        readSettings();
        return true;
    }

    if (Ch3ManualPowerNP.isNameMatch(name))
    {
        Ch3ManualPowerNP.update(values, names, n);
        Ch3ManualPowerNP.setState(IPS_BUSY);
        Ch3ManualPowerNP.apply();
        setCh3Output(Ch3ManualPowerNP[0].getValue());
        readSettings();
        return true;
    }

    return INDI::DefaultDevice::ISNewNumber(dev, name, values, names, n);
}

bool MyDCP4ESP::readSettings()
{
    char resp[MDCP_RESPONSE_LENGTH] = {};
    int ok = -1;
    float temp1, temp2, temp3, temp4, temp_ambient, humidity, dewpoint;
    unsigned int output1, output2, output3, output4;
    float ambient_offset, offset1, offset2, offset3, offset4; 
    int tracking_offset;
    unsigned int tracking_mode, ch3_mode, channel_boost;

    
    // Get Ambient offset first
    if (!sendCommand(MDCP_GET_AMBIENT_OFFSET_CMD, resp))
        return false;

    ok = sscanf(resp, MDCP_GET_AMBIENT_OFFSET_RES, &ambient_offset );

    if (ok == 1)
    {
        AmbientOffsetNP[0].setValue(ambient_offset);
        AmbientOffsetNP.setState(IPS_OK);
        AmbientOffsetNP.apply();
    }
    else
        LOGF_ERROR("Get Ambient Offset: Response <%s> for Command <%s> not recognized.", resp, MDCP_GET_AMBIENT_OFFSET_CMD);

    // Get the Ambient Temperature
    memset(resp, '\0', MDCP_RESPONSE_LENGTH); 

    if (!sendCommand(MDCP_GET_AMBIENT_TEMPERATURE_CMD, resp))
        return false;

    ok = sscanf(resp, MDCP_GET_AMBIENT_TEMPERATURE_RES, &temp_ambient );

    // Set Ambient temperature adjusted by offset
    if (ok == 1)
    {
        AmbientTemperatureNP[0].setValue(temp_ambient + ambient_offset);
        AmbientTemperatureNP.setState(IPS_OK);
        AmbientTemperatureNP.apply();
    }
    else
        LOGF_ERROR("Get Ambient Temperature: Response <%s> for Command <%s> not recognized.", resp, MDCP_GET_AMBIENT_TEMPERATURE_CMD);

    // Get Humidity
    memset(resp, '\0', MDCP_RESPONSE_LENGTH); 
    
    if (!sendCommand(MDCP_GET_HUMIDITY_CMD, resp))
        return false;

    ok = sscanf(resp, MDCP_GET_HUMIDITY_RES, &humidity );

    if (ok == 1)
    {
        HumidityNP[0].setValue(humidity);
        HumidityNP.setState(IPS_OK);
        HumidityNP.apply();
    }
    else
        LOGF_ERROR("Get Humidity: Response <%s> for Command <%s> not recognized.", resp, MDCP_GET_HUMIDITY_CMD);

    // Get Dew Point
    memset(resp, '\0', MDCP_RESPONSE_LENGTH); 
    
    if (!sendCommand(MDCP_GET_DEWPOINT_CMD, resp))
        return false;

    ok = sscanf(resp, MDCP_GET_DEWPOINT_RES, &dewpoint );

    if (ok == 1)
    {
        DewpointNP[0].setValue(dewpoint);
        DewpointNP.setState(IPS_OK);
        DewpointNP.apply();
    }
    else
        LOGF_ERROR("Get Dew point: Response <%s> for Command <%s> not recognized.", resp, MDCP_GET_DEWPOINT_CMD);

    // Get Power output for all channels
    memset(resp, '\0', MDCP_RESPONSE_LENGTH); 
        
    if (!sendCommand(MDCP_GET_ALL_CH_POWER_CMD, resp))
        return false;

    ok = sscanf(resp, MDCP_GET_ALL_CH_POWER_RES, &output1, &output2, &output3, &output4 );

    if (ok == 4)
    {
        ChannelPowerNP[0].setValue(output1);
        ChannelPowerNP[1].setValue(output2);
        ChannelPowerNP[2].setValue(output3);
        ChannelPowerNP[3].setValue(output4);
        ChannelPowerNP.setState(IPS_OK);
        ChannelPowerNP.apply();
    }
    else
        LOGF_ERROR("Get Power Outputs: Response <%s> for Command <%s> not recognized.", resp, MDCP_GET_ALL_CH_POWER_CMD);

    // Get Channel 3 Mode
    memset(resp, '\0', MDCP_RESPONSE_LENGTH); 
    
    if (!sendCommand(MDCP_GET_CH3_MODE_CMD, resp))
        return false;

    ok = sscanf(resp, MDCP_GET_CH3_MODE_RES, &ch3_mode );

    if ((ok == 1) && (ch3_mode <= 4))
    {
        // Enable/Disable Ch3 Manual Power setting if Ch3 Mode Manual enabled
        if ((ch3_mode == CH3MODE_MANUAL) && (!ch3ManualPower))
        {
            defineProperty(Ch3ManualPowerNP);
            ch3ManualPower = true;
        }
        else if ((ch3_mode != CH3MODE_MANUAL) && ch3ManualPower)
        {
            deleteProperty(Ch3ManualPowerNP);
            ch3ManualPower = false;
        }

        Ch3ModeSP.reset();
        Ch3ModeSP[ch3_mode].setState(ISS_ON);
        Ch3ModeSP.setState(IPS_OK);
        Ch3ModeSP.apply();
       
    }
    else
        LOGF_ERROR("Get Channel 3 Mode: Response <%s> for Command <%s> not recognized.", resp, MDCP_GET_CH3_MODE_CMD);

    if (ch3ManualPower == true)
    {
        Ch3ManualPowerNP[0].setValue(ChannelPowerNP[2].getValue());
        Ch3ManualPowerNP.setState(IPS_OK);
        Ch3ManualPowerNP.apply();
    }

    // Get Temperature offsets for all channels then use to set the channel temperatures
    memset(resp, '\0', MDCP_RESPONSE_LENGTH);

    // Get offset values first
    if (!sendCommand(MDCP_GET_ALL_CH_OFFSET_CMD, resp))
        return false;

    ok = sscanf(resp, MDCP_GET_ALL_CH_OFFSET_RES, &offset1, &offset2, &offset3, &offset4 );

    if (ok == 4)
    {
        ChannelOffsetNP[0].setValue(offset1);
        ChannelOffsetNP[1].setValue(offset2);
        ChannelOffsetNP[2].setValue(offset3);
        ChannelOffsetNP[3].setValue(offset4);
        ChannelOffsetNP.setState(IPS_OK);
        ChannelOffsetNP.apply();
    }
    else
        LOGF_ERROR("Get Channel Offset: Response <%s> for Command <%s> not recognized.", resp, MDCP_GET_ALL_CH_OFFSET_CMD);

    memset(resp, '\0', MDCP_RESPONSE_LENGTH);

    if (!sendCommand(MDCP_GET_CHANNEL_TEMPS_CMD, resp))
        return false;

    ok = sscanf(resp, MDCP_GET_CHANNEL_TEMPS_RES, &temp1, &temp2, &temp3, &temp4 );

    // Display adjusted temperature with offset but only for Channels with Probes
    // the controller returns zero for temp if no probe but will continue to return offset
    // channelActive will be zero for channels with no probe
    if (ok == 4)
    {
        TemperatureNP[0].setValue(temp1 + (offset1 * channelActive[0]));
        TemperatureNP[1].setValue(temp2 + (offset2 * channelActive[1]));
        // Channel 3 is special and setting depends on CH3Mode only show in  CH3TEMP mode
        if (Ch3ModeSP.findOnSwitchIndex() == CH3MODE_CH3TEMP)
            TemperatureNP[2].setValue(temp3 + (offset3 * channelActive[2]));
        else
            TemperatureNP[2].setValue( 0 );

        TemperatureNP[3].setValue(temp4 + (offset4 * channelActive[3]));
        TemperatureNP.setState(IPS_OK);
        TemperatureNP.apply();
    }
    else
        LOGF_ERROR("Get Channel Temperatures: Response <%s> for Command <%s> not recognized.", resp, MDCP_GET_CHANNEL_TEMPS_CMD);

    // Get Tracking Mode
    memset(resp, '\0', MDCP_RESPONSE_LENGTH); 
    
    if (!sendCommand(MDCP_GET_TRACKING_MODE_CMD, resp))
        return false;

    ok = sscanf(resp, MDCP_GET_TRACKING_MODE_RES, &tracking_mode );

    if ((ok == 1) && (tracking_mode > 0) && (tracking_mode <= 3))
    {
        TrackingModeSP.reset();
        TrackingModeSP[tracking_mode-1].setState(ISS_ON);
        TrackingModeSP.setState(IPS_OK);
        TrackingModeSP.apply();
    }
    else
        LOGF_ERROR("Get Tracking Mode: Response <%s> for Command <%s> not recognized.", resp, MDCP_GET_TRACKING_MODE_CMD);

    //Get Tracking Offset but Firmware 109 has a bug with setting offsets so only for >109
    if (myDCP4Firmware > 109) 
    {
        memset(resp, '\0', MDCP_RESPONSE_LENGTH); 
    
        if (!sendCommand(MDCP_GET_TRACKING_OFFSET_CMD, resp))
            return false;

        ok = sscanf(resp, MDCP_GET_TRACKING_OFFSET_RES, &tracking_offset );

        if (ok == 1)
        {
            TrackingOffsetNP[0].setValue(tracking_offset);
            TrackingOffsetNP.setState(IPS_OK);
            TrackingOffsetNP.apply();
        }
        else
            LOGF_ERROR("Get Tracking Offset: Response <%s> for Command <%s> not recognized.", resp, MDCP_GET_TRACKING_OFFSET_CMD);

    }

    // Current Channel 100% boost state but only for firmware >109 due to firmware bug
    if (myDCP4Firmware > 109) 
    {
        // Always clear the Channel boost reset checkbox
        ChannelBoostSP[4].setState(ISS_OFF);

        for (int i = 1; i <= 4; i++)
        {
            char cmd[MDCP_CMD_LENGTH] = {};

            snprintf(cmd, MDCP_CMD_LENGTH, MDCP_GET_CH_OVERIDE_CMD, i);

            memset(resp, '\0', MDCP_RESPONSE_LENGTH);

            if (!sendCommand(cmd, resp))
                return false;

            ok = sscanf(resp, MDCP_GET_CH_OVERIDE_RES, &channel_boost);

            if ((ok == 1) && (channel_boost <= 1))
            {
                ChannelBoostSP[i-1].setState( (ISState) channel_boost);
                ChannelBoostSP.setState(IPS_OK);
                ChannelBoostSP.apply();
            }
            else
                LOGF_ERROR("Get Channel Overrides: Response <%s> for Command <%s> not recognized.", resp, cmd);
        }
    }    
    
    return true;
}

void MyDCP4ESP::TimerHit()
{
    if (!isConnected())
    {
        return;
    }

    // Get temperatures etc.
    readSettings();
    timerIndex = SetTimer(getCurrentPollingPeriod());
}
