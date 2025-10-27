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

#include "usb_dewpoint.h"
#include "connectionplugins/connectionserial.h"
#include "indicom.h"

#include <cstring>
#include <memory>

#include <termios.h>
#include <unistd.h>

#define USBDEWPOINT_TIMEOUT 3

std::unique_ptr<USBDewpoint> usbDewpoint(new USBDewpoint());

USBDewpoint::USBDewpoint() : INDI::PowerInterface(this)
{
    setVersion(1, 2);
}

bool USBDewpoint::initProperties()
{
    DefaultDevice::initProperties();

    SetCapability(INDI::PowerInterface::POWER_HAS_DEW_OUT | INDI::PowerInterface::POWER_HAS_AUTO_DEW);
    INDI::PowerInterface::initProperties(POWER_TAB, 0, 3, 0, 1, 0);

    /* Channel duty cycles */
    // OutputsNP[0].fill("CHANNEL1", "Channel 1", "%3.0f", 0., 100., 10., 0.);
    // OutputsNP[1].fill("CHANNEL2", "Channel 2", "%3.0f", 0., 100., 10., 0.);
    // OutputsNP[2].fill("CHANNEL3", "Channel 3", "%3.0f", 0., 100., 10., 0.);
    // OutputsNP.fill(getDeviceName(), "OUTPUT", "Outputs", MAIN_CONTROL_TAB, IP_RW, 0, IPS_IDLE);

    /* Temperatures */
    TemperaturesNP[0].fill("CHANNEL1", "Channel 1", "%3.2f", -50., 70., 0., 0.);
    TemperaturesNP[1].fill("CHANNEL2", "Channel 2", "%3.2f", -50., 70., 0., 0.);
    TemperaturesNP[2].fill("AMBIENT", "Ambient", "%3.2f", -50., 70., 0., 0.);
    TemperaturesNP.fill(getDeviceName(), "TEMPERATURES", "Temperatures", MAIN_CONTROL_TAB, IP_RO, 0, IPS_IDLE);

    /* Humidity */
    HumidityNP[0].fill("HUMIDITY", "Humidity", "%3.2f", 0., 100., 0., 0.);
    HumidityNP.fill(getDeviceName(), "HUMIDITY", "Humidity", MAIN_CONTROL_TAB, IP_RO, 0, IPS_IDLE);

    /* Dew point */
    DewpointNP[0].fill("DEWPOINT", "Dew point", "%3.2f", -50., 70., 0., 0.);
    DewpointNP.fill(getDeviceName(), "DEWPOINT", "Dew point", MAIN_CONTROL_TAB, IP_RO, 0, IPS_IDLE);

    /* Temperature calibration values */
    CalibrationsNP[0].fill("CHANNEL1", "Channel 1", "%1.0f", 0., 9., 1., 0.);
    CalibrationsNP[1].fill("CHANNEL2", "Channel 2", "%1.0f", 0., 9., 1., 0.);
    CalibrationsNP[2].fill("AMBIENT", "Ambient", "%1.0f", 0., 9., 1., 0.);
    CalibrationsNP.fill(getDeviceName(), "CALIBRATIONS", "Calibrations", OPTIONS_TAB, IP_RW, 0, IPS_IDLE);

    /* Temperature threshold values */
    ThresholdsNP[0].fill("CHANNEL1", "Channel 1", "%1.0f", 0., 9., 1., 0.);
    ThresholdsNP[1].fill("CHANNEL2", "Channel 2", "%1.0f", 0., 9., 1., 0.);
    ThresholdsNP.fill(getDeviceName(), "THRESHOLDS", "Thresholds", OPTIONS_TAB, IP_RW, 0, IPS_IDLE);

    /* Heating aggressivity */
    AggressivityNP[0].fill("AGGRESSIVITY", "Aggressivity", "%1.0f", 1., 4., 1., 1.);
    AggressivityNP.fill(getDeviceName(), "AGGRESSIVITY", "Aggressivity", OPTIONS_TAB, IP_RW, 0, IPS_IDLE);

    /*  Automatic mode enable */
    // AutoModeSP[0].fill("MANUAL", "Manual", ISS_OFF);
    // AutoModeSP[1].fill("AUTO", "Automatic", ISS_ON);
    // AutoModeSP.fill(getDeviceName(), "MODE", "Operating mode", MAIN_CONTROL_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    /* Link channel 2 & 3 */
    LinkOut23SP[0].fill("INDEPENDENT", "Independent", ISS_ON);
    LinkOut23SP[1].fill("LINK", "Link", ISS_OFF);
    LinkOut23SP.fill(getDeviceName(), "LINK23", "Link ch 2&3", OPTIONS_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    /* Reset settings */
    ResetSP[0].fill("Reset", "", ISS_OFF);
    ResetSP.fill(getDeviceName(), "Reset", "", OPTIONS_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    /* Firmware version */
    FWversionNP[0].fill("FIRMWARE", "Firmware Version", "%4.0f", 0., 65535., 1., 0.);
    FWversionNP.fill(getDeviceName(), "FW_VERSION", "Firmware", OPTIONS_TAB, IP_RO, 0, IPS_IDLE);

    setDriverInterface(AUX_INTERFACE | POWER_INTERFACE);

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

bool USBDewpoint::updateProperties()
{
    DefaultDevice::updateProperties();
    INDI::PowerInterface::updateProperties();

    if (isConnected())
    {
        // defineProperty(OutputsNP); // Handled by PowerInterface
        defineProperty(TemperaturesNP);
        defineProperty(HumidityNP);
        defineProperty(DewpointNP);
        defineProperty(CalibrationsNP);
        defineProperty(ThresholdsNP);
        defineProperty(AggressivityNP);
        // defineProperty(AutoModeSP); // Handled by PowerInterface
        defineProperty(LinkOut23SP);
        defineProperty(ResetSP);
        defineProperty(FWversionNP);

        loadConfig(true);
        readSettings();
        LOG_INFO("USB_Dewpoint parameters updated, device ready for use.");
        SetTimer(getCurrentPollingPeriod());
    }
    else
    {
        // deleteProperty(OutputsNP); // Handled by PowerInterface
        deleteProperty(TemperaturesNP);
        deleteProperty(HumidityNP);
        deleteProperty(DewpointNP);
        deleteProperty(CalibrationsNP);
        deleteProperty(ThresholdsNP);
        deleteProperty(AggressivityNP);
        // deleteProperty(AutoModeSP); // Handled by PowerInterface
        deleteProperty(LinkOut23SP);
        deleteProperty(ResetSP);
        deleteProperty(FWversionNP);
    }

    return true;
}

const char *USBDewpoint::getDefaultName()
{
    return "USB_Dewpoint";
}

bool USBDewpoint::sendCommand(const char *cmd, char *resp)
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
        if ((rc = tty_nread_section(PortFD, resp, UDP_RES_LEN, '\r', USBDEWPOINT_TIMEOUT, &nbytes_read)) != TTY_OK)
        {
            tty_error_msg(rc, errstr, MAXRBUF);
            LOGF_ERROR("Error reading response for command %s: %s.", cmd, errstr);
            return false;
        }

        if (nbytes_read < 2)
        {
            LOGF_ERROR("Invalid response for command %s: %s.", cmd, resp);
            return false;
        }
        resp[nbytes_read - 2] = '\0'; // Strip \n\r
        LOGF_DEBUG("RES: %s.", resp);
    }
    return true;
}

bool USBDewpoint::Resync()
{
    char cmd[]         = " "; // Illegal command to trigger error response
    int nbytes_written = 0, nbytes_read = 0, rc = -1;
    char errstr[MAXRBUF];
    char resp[UDP_RES_LEN] = {};

    // Send up to 5 space characters and wait for error
    // response ("ER=1") after which the communication
    // is back in sync
    tcflush(PortFD, TCIOFLUSH);

    for (int resync = 0; resync < UDP_CMD_LEN; resync++)
    {
        LOGF_INFO("Retry %d...", resync + 1);

        if ((rc = tty_write(PortFD, cmd, 1, &nbytes_written)) != TTY_OK)
        {
            tty_error_msg(rc, errstr, MAXRBUF);
            LOGF_ERROR("Error writing resync: %s.", errstr);
            return false;
        }

        rc = tty_nread_section(PortFD, resp, UDP_RES_LEN, '\r', 3, &nbytes_read);
        if (rc == TTY_OK && nbytes_read > 0)
        {
            // We got a response
            return true;
        }
        // We didn't get response yet, retry
    }
    LOG_ERROR("No valid resync response.");
    return false;
}

bool USBDewpoint::Handshake()
{
    PortFD = serialConnection->getPortFD();

    int tries = 2;
    do
    {
        if (Ack())
        {
            LOG_INFO("USB_Dewpoint is online. Getting device parameters...");
            return true;
        }
        LOG_INFO("Error retrieving data from USB_Dewpoint, trying resync...");
    }
    while (--tries > 0 && Resync());

    LOG_INFO("Error retrieving data from USB_Dewpoint, please ensure controller "
             "is powered and the port is correct.");
    return false;
}

bool USBDewpoint::Ack()
{
    char resp[UDP_RES_LEN] = {};
    tcflush(PortFD, TCIOFLUSH);

    if (!sendCommand(UDP_IDENTIFY_CMD, resp))
        return false;

    int firmware = -1;

    int ok = sscanf(resp, UDP_IDENTIFY_RESPONSE, &firmware);

    if (ok != 1)
    {
        LOGF_ERROR("USB_Dewpoint not properly identified! Answer was: %s.", resp);
        return false;
    }

    FWversionNP[0].setValue(firmware);
    FWversionNP.setState(IPS_OK);
    FWversionNP.apply();
    return true;
}

bool USBDewpoint::setOutput(unsigned int channel, unsigned int value)
{
    char cmd[UDP_CMD_LEN + 1];
    char resp[UDP_RES_LEN];

    snprintf(cmd, UDP_CMD_LEN + 1, UDP_OUTPUT_CMD, channel, value);
    return sendCommand(cmd, resp);
}

bool USBDewpoint::setCalibrations(unsigned int ch1, unsigned int ch2, unsigned int ambient)
{
    char cmd[UDP_CMD_LEN + 1];
    char resp[UDP_RES_LEN];

    snprintf(cmd, UDP_CMD_LEN + 1, UDP_CALIBRATION_CMD, ch1, ch2, ambient);
    return sendCommand(cmd, resp);
}

bool USBDewpoint::setThresholds(unsigned int ch1, unsigned int ch2)
{
    char cmd[UDP_CMD_LEN + 1];
    char resp[UDP_RES_LEN];

    snprintf(cmd, UDP_CMD_LEN + 1, UDP_THRESHOLD_CMD, ch1, ch2);
    return sendCommand(cmd, resp);
}

bool USBDewpoint::setAggressivity(unsigned int aggressivity)
{
    char cmd[UDP_CMD_LEN + 1];
    char resp[UDP_RES_LEN];

    snprintf(cmd, UDP_CMD_LEN + 1, UDP_AGGRESSIVITY_CMD, aggressivity);
    return sendCommand(cmd, resp);
}

bool USBDewpoint::reset()
{
    char resp[UDP_RES_LEN];
    return sendCommand(UDP_RESET_CMD, resp);
}

bool USBDewpoint::setAutoMode(bool enable)
{
    char cmd[UDP_CMD_LEN + 1];
    char resp[UDP_RES_LEN];

    snprintf(cmd, UDP_CMD_LEN + 1, UDP_AUTO_CMD, enable ? 1 : 0);
    return sendCommand(cmd, resp);
}

bool USBDewpoint::SetAutoDewEnabled(size_t port, bool enabled)
{
    INDI_UNUSED(port);
    return setAutoMode(enabled);
}

bool USBDewpoint::SetDewPort(size_t port, bool enabled, double dutyCycle)
{
    INDI_UNUSED(enabled);
    return setOutput(port + 1, static_cast<unsigned int>(dutyCycle));
}

bool USBDewpoint::setLinkMode(bool enable)
{
    char cmd[UDP_CMD_LEN + 1];
    char resp[UDP_RES_LEN];

    snprintf(cmd, UDP_CMD_LEN + 1, UDP_LINK_CMD, enable ? 1 : 0);
    return sendCommand(cmd, resp);
}

bool USBDewpoint::SetPowerPort(size_t port, bool enabled)
{
    INDI_UNUSED(port);
    INDI_UNUSED(enabled);
    LOG_DEBUG("SetPowerPort not supported by USB_Dewpoint.");
    return false;
}

bool USBDewpoint::SetVariablePort(size_t port, bool enabled, double voltage)
{
    INDI_UNUSED(port);
    INDI_UNUSED(enabled);
    INDI_UNUSED(voltage);
    LOG_DEBUG("SetVariablePort not supported by USB_Dewpoint.");
    return false;
}

bool USBDewpoint::SetLEDEnabled(bool enabled)
{
    INDI_UNUSED(enabled);
    LOG_DEBUG("SetLEDEnabled not supported by USB_Dewpoint.");
    return false;
}

bool USBDewpoint::CyclePower()
{
    LOG_DEBUG("CyclePower not supported by USB_Dewpoint.");
    return false;
}

bool USBDewpoint::SetUSBPort(size_t port, bool enabled)
{
    INDI_UNUSED(port);
    INDI_UNUSED(enabled);
    LOG_DEBUG("SetUSBPort not supported by USB_Dewpoint.");
    return false;
}

bool USBDewpoint::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if (!dev || strcmp(dev, getDeviceName()))
        return false;

    if (INDI::PowerInterface::processSwitch(dev, name, states, names, n))
        return true;

    // AutoModeSP is now handled by PowerInterface
    // if (AutoModeSP.isNameMatch(name))
    // {
    //     AutoModeSP.update(states, names, n);
    //     int target_mode = AutoModeSP.findOnSwitchIndex();
    //     AutoModeSP.setState(IPS_BUSY);
    //     AutoModeSP.apply();
    //     setAutoMode(target_mode == 1);
    //     readSettings();
    //     return true;
    // }
    if (LinkOut23SP.isNameMatch(name))
    {
        LinkOut23SP.update(states, names, n);
        int target_mode = LinkOut23SP.findOnSwitchIndex();
        LinkOut23SP.setState(IPS_BUSY);
        LinkOut23SP.apply();
        setLinkMode(target_mode == 1);
        readSettings();
        return true;
    }
    if (ResetSP.isNameMatch(name))
    {
        ResetSP.reset();

        if (reset())
        {
            ResetSP.setState(IPS_OK);
            readSettings();
        }
        else
        {
            ResetSP.setState(IPS_ALERT);
        }

        ResetSP.apply();
        return true;
    }

    return INDI::DefaultDevice::ISNewSwitch(dev, name, states, names, n);
}

bool USBDewpoint::ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n)
{
    if (!dev || strcmp(dev, getDeviceName()))
        return false;

    if (INDI::PowerInterface::processText(dev, name, texts, names, n))
        return true;

    return INDI::DefaultDevice::ISNewText(dev, name, texts, names, n);
}

bool USBDewpoint::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    if (!dev || strcmp(dev, getDeviceName()))
        return false;

    if (INDI::PowerInterface::processNumber(dev, name, values, names, n))
        return true;

    // OutputsNP is now handled by PowerInterface
    // if (OutputsNP.isNameMatch(name))
    // {
    //     // Warn if we are in auto mode
    //     if (AutoModeSP.findOnSwitchIndex() == 1)
    //     {
    //         LOG_WARN("Setting output power is ignored in auto mode!");
    //         return true;
    //     }
    //     OutputsNP.update(values, names, n);
    //     OutputsNP.setState(IPS_BUSY);
    //     OutputsNP.apply();
    //     setOutput(1, OutputsNP[0].getValue());
    //     setOutput(2, OutputsNP[1].getValue());
    //     setOutput(3, OutputsNP[2].getValue());
    //     readSettings();
    //     return true;
    // }
    if (CalibrationsNP.isNameMatch(name))
    {
        CalibrationsNP.update(values, names, n);
        CalibrationsNP.setState(IPS_BUSY);
        CalibrationsNP.apply();
        setCalibrations(CalibrationsNP[0].getValue(), CalibrationsNP[1].getValue(), CalibrationsNP[2].getValue());
        readSettings();
        return true;
    }
    if (ThresholdsNP.isNameMatch(name))
    {
        ThresholdsNP.update(values, names, n);
        ThresholdsNP.setState(IPS_BUSY);
        ThresholdsNP.apply();
        setThresholds(ThresholdsNP[0].getValue(), ThresholdsNP[1].getValue());
        readSettings();
        return true;
    }
    if (AggressivityNP.isNameMatch(name))
    {
        AggressivityNP.update(values, names, n);
        AggressivityNP.setState(IPS_BUSY);
        AggressivityNP.apply();
        setAggressivity(AggressivityNP[0].getValue());
        readSettings();
        return true;
    }
    if (FWversionNP.isNameMatch(name))
    {
        FWversionNP.update(values, names, n);
        FWversionNP.setState(IPS_OK);
        FWversionNP.apply();
        return true;
    }

    return INDI::DefaultDevice::ISNewNumber(dev, name, values, names, n);
}

bool USBDewpoint::readSettings()
{
    char resp[UDP_RES_LEN];

    if (!sendCommand(UDP_STATUS_CMD, resp))
        return false;

    // Status response is like:
    // ##22.37/22.62/23.35/50.77/12.55/0/0/0/0/0/0/2/2/0/0/4**

    float temp1, temp2, temp_ambient, humidity, dewpoint;
    unsigned int output1, output2, output3;
    unsigned int calibration1, calibration2, calibration_ambient;
    unsigned int threshold1, threshold2;
    unsigned int automode, linkout23, aggressivity;

    int ok = sscanf(resp, UDP_STATUS_RESPONSE, &temp1, &temp2, &temp_ambient, &humidity, &dewpoint, &output1, &output2,
                    &output3, &calibration1, &calibration2, &calibration_ambient, &threshold1, &threshold2, &automode,
                    &linkout23, &aggressivity);

    if (ok == 16)
    {
        TemperaturesNP[0].setValue(temp1);
        TemperaturesNP[1].setValue(temp2);
        TemperaturesNP[2].setValue(temp_ambient);
        TemperaturesNP.setState(IPS_OK);
        TemperaturesNP.apply();

        HumidityNP[0].setValue(humidity);
        HumidityNP.setState(IPS_OK);
        HumidityNP.apply();

        DewpointNP[0].setValue(dewpoint);
        DewpointNP.setState(IPS_OK);
        DewpointNP.apply();

        DewChannelsSP[0].setState(output1 > 0 ? ISS_ON : ISS_OFF);
        DewChannelsSP[1].setState(output2 > 0 ? ISS_ON : ISS_OFF);
        DewChannelsSP[2].setState(output3 > 0 ? ISS_ON : ISS_OFF);
        DewChannelsSP.setState(IPS_OK);
        DewChannelsSP.apply();

        CalibrationsNP[0].setValue(calibration1);
        CalibrationsNP[1].setValue(calibration2);
        CalibrationsNP[2].setValue(calibration_ambient);
        CalibrationsNP.setState(IPS_OK);
        CalibrationsNP.apply();

        ThresholdsNP[0].setValue(threshold1);
        ThresholdsNP[1].setValue(threshold2);
        ThresholdsNP.setState(IPS_OK);
        ThresholdsNP.apply();

        AutoDewSP.reset();
        AutoDewSP[automode].setState(ISS_ON);
        AutoDewSP.setState(IPS_OK);
        AutoDewSP.apply();

        LinkOut23SP.reset();
        LinkOut23SP[linkout23].setState(ISS_ON);
        LinkOut23SP.setState(IPS_OK);
        LinkOut23SP.apply();

        AggressivityNP[0].setValue(aggressivity);
        AggressivityNP.setState(IPS_OK);
        AggressivityNP.apply();
    }
    return true;
}

void USBDewpoint::TimerHit()
{
    if (!isConnected())
    {
        return;
    }

    // Get temperatures etc.
    readSettings();
    SetTimer(getCurrentPollingPeriod());
}

bool USBDewpoint::saveConfigItems(FILE *fp)
{
    INDI::DefaultDevice::saveConfigItems(fp);
    INDI::PowerInterface::saveConfigItems(fp);
    LinkOut23SP.save(fp);
    AggressivityNP.save(fp);
    CalibrationsNP.save(fp);
    ThresholdsNP.save(fp);
    return true;
}
