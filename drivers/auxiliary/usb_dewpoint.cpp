/*
    USB_Dewpoint
    Copyright (C) 2017 Jarno Paananen

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

#include <cmath>
#include <cstring>
#include <memory>

#include <termios.h>
#include <unistd.h>

#define USBDEWPOINT_TIMEOUT 3

std::unique_ptr<USBDewpoint> usbDewpoint(new USBDewpoint());

USBDewpoint::USBDewpoint()
{
    setVersion(1, 1);
}

bool USBDewpoint::initProperties()
{
    DefaultDevice::initProperties();

    /* Channel duty cycles */
    IUFillNumber(&OutputsN[0], "CHANNEL1", "Channel 1", "%3.0f", 0., 100., 10., 0.);
    IUFillNumber(&OutputsN[1], "CHANNEL2", "Channel 2", "%3.0f", 0., 100., 10., 0.);
    IUFillNumber(&OutputsN[2], "CHANNEL3", "Channel 3", "%3.0f", 0., 100., 10., 0.);
    IUFillNumberVector(&OutputsNP, OutputsN, 3, getDeviceName(), "OUTPUT", "Outputs", MAIN_CONTROL_TAB, IP_RW, 0,
                       IPS_IDLE);

    /* Temperatures */
    IUFillNumber(&TemperaturesN[0], "CHANNEL1", "Channel 1", "%3.2f", -50., 70., 0., 0.);
    IUFillNumber(&TemperaturesN[1], "CHANNEL2", "Channel 2", "%3.2f", -50., 70., 0., 0.);
    IUFillNumber(&TemperaturesN[2], "AMBIENT", "Ambient", "%3.2f", -50., 70., 0., 0.);
    IUFillNumberVector(&TemperaturesNP, TemperaturesN, 3, getDeviceName(), "TEMPERATURES", "Temperatures",
                       MAIN_CONTROL_TAB, IP_RO, 0, IPS_IDLE);

    /* Humidity */
    IUFillNumber(&HumidityN[0], "HUMIDITY", "Humidity", "%3.2f", 0., 100., 0., 0.);
    IUFillNumberVector(&HumidityNP, HumidityN, 1, getDeviceName(), "HUMIDITY", "Humidity", MAIN_CONTROL_TAB, IP_RO, 0,
                       IPS_IDLE);

    /* Dew point */
    IUFillNumber(&DewpointN[0], "DEWPOINT", "Dew point", "%3.2f", -50., 70., 0., 0.);
    IUFillNumberVector(&DewpointNP, DewpointN, 1, getDeviceName(), "DEWPOINT", "Dew point", MAIN_CONTROL_TAB, IP_RO, 0,
                       IPS_IDLE);

    /* Temperature calibration values */
    IUFillNumber(&CalibrationsN[0], "CHANNEL1", "Channel 1", "%1.0f", 0., 9., 1., 0.);
    IUFillNumber(&CalibrationsN[1], "CHANNEL2", "Channel 2", "%1.0f", 0., 9., 1., 0.);
    IUFillNumber(&CalibrationsN[2], "AMBIENT", "Ambient", "%1.0f", 0., 9., 1., 0.);
    IUFillNumberVector(&CalibrationsNP, CalibrationsN, 3, getDeviceName(), "CALIBRATIONS", "Calibrations", OPTIONS_TAB,
                       IP_RW, 0, IPS_IDLE);

    /* Temperature threshold values */
    IUFillNumber(&ThresholdsN[0], "CHANNEL1", "Channel 1", "%1.0f", 0., 9., 1., 0.);
    IUFillNumber(&ThresholdsN[1], "CHANNEL2", "Channel 2", "%1.0f", 0., 9., 1., 0.);
    IUFillNumberVector(&ThresholdsNP, ThresholdsN, 2, getDeviceName(), "THRESHOLDS", "Thresholds", OPTIONS_TAB, IP_RW,
                       0, IPS_IDLE);

    /* Heating aggressivity */
    IUFillNumber(&AggressivityN[0], "AGGRESSIVITY", "Aggressivity", "%1.0f", 1., 4., 1., 1.);
    IUFillNumberVector(&AggressivityNP, AggressivityN, 1, getDeviceName(), "AGGRESSIVITY", "Aggressivity", OPTIONS_TAB,
                       IP_RW, 0, IPS_IDLE);

    /*  Automatic mode enable */
    IUFillSwitch(&AutoModeS[0], "MANUAL", "Manual", ISS_OFF);
    IUFillSwitch(&AutoModeS[1], "AUTO", "Automatic", ISS_ON);
    IUFillSwitchVector(&AutoModeSP, AutoModeS, 2, getDeviceName(), "MODE", "Operating mode", MAIN_CONTROL_TAB, IP_RW,
                       ISR_1OFMANY, 0, IPS_IDLE);

    /* Link channel 2 & 3 */
    IUFillSwitch(&LinkOut23S[0], "INDEPENDENT", "Independent", ISS_ON);
    IUFillSwitch(&LinkOut23S[1], "LINK", "Link", ISS_OFF);
    IUFillSwitchVector(&LinkOut23SP, LinkOut23S, 2, getDeviceName(), "LINK23", "Link ch 2&3", OPTIONS_TAB, IP_RW,
                       ISR_1OFMANY, 0, IPS_IDLE);
    /* Reset settings */
    IUFillSwitch(&ResetS[0], "Reset", "", ISS_OFF);
    IUFillSwitchVector(&ResetSP, ResetS, 1, getDeviceName(), "Reset", "", OPTIONS_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    /* Firmware version */
    IUFillNumber(&FWversionN[0], "FIRMWARE", "Firmware Version", "%4.0f", 0., 65535., 1., 0.);
    IUFillNumberVector(&FWversionNP, FWversionN, 1, getDeviceName(), "FW_VERSION", "Firmware", OPTIONS_TAB, IP_RO, 0,
                       IPS_IDLE);

    setDriverInterface(AUX_INTERFACE);

    addDebugControl();
    addConfigurationControl();
    setDefaultPollingPeriod(10000);
    addPollPeriodControl();

    // No simulation control for now

    serialConnection = new Connection::Serial(this);
    serialConnection->registerHandshake([&]() { return Handshake(); });
    registerConnection(serialConnection);

    return true;
}

bool USBDewpoint::updateProperties()
{
    DefaultDevice::updateProperties();

    if (isConnected())
    {
        defineProperty(&OutputsNP);
        defineProperty(&TemperaturesNP);
        defineProperty(&HumidityNP);
        defineProperty(&DewpointNP);
        defineProperty(&CalibrationsNP);
        defineProperty(&ThresholdsNP);
        defineProperty(&AggressivityNP);
        defineProperty(&AutoModeSP);
        defineProperty(&LinkOut23SP);
        defineProperty(&ResetSP);
        defineProperty(&FWversionNP);

        loadConfig(true);
        readSettings();
        LOG_INFO("USB_Dewpoint parameters updated, device ready for use.");
        SetTimer(getCurrentPollingPeriod());
    }
    else
    {
        deleteProperty(OutputsNP.name);
        deleteProperty(TemperaturesNP.name);
        deleteProperty(HumidityNP.name);
        deleteProperty(DewpointNP.name);
        deleteProperty(CalibrationsNP.name);
        deleteProperty(ThresholdsNP.name);
        deleteProperty(AggressivityNP.name);
        deleteProperty(AutoModeSP.name);
        deleteProperty(LinkOut23SP.name);
        deleteProperty(ResetSP.name);
        deleteProperty(FWversionNP.name);
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
    } while (--tries > 0 && Resync());

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

    FWversionN[0].value = firmware;
    FWversionNP.s       = IPS_OK;
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

bool USBDewpoint::setLinkMode(bool enable)
{
    char cmd[UDP_CMD_LEN + 1];
    char resp[UDP_RES_LEN];

    snprintf(cmd, UDP_CMD_LEN + 1, UDP_LINK_CMD, enable ? 1 : 0);
    return sendCommand(cmd, resp);
}

bool USBDewpoint::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        if (strcmp(AutoModeSP.name, name) == 0)
        {
            IUUpdateSwitch(&AutoModeSP, states, names, n);
            int target_mode = IUFindOnSwitchIndex(&AutoModeSP);
            AutoModeSP.s    = IPS_BUSY;
            IDSetSwitch(&AutoModeSP, nullptr);
            setAutoMode(target_mode == 1);
            readSettings();
            return true;
        }
        if (strcmp(LinkOut23SP.name, name) == 0)
        {
            IUUpdateSwitch(&LinkOut23SP, states, names, n);
            int target_mode = IUFindOnSwitchIndex(&LinkOut23SP);
            LinkOut23SP.s   = IPS_BUSY;
            IDSetSwitch(&LinkOut23SP, nullptr);
            setLinkMode(target_mode == 1);
            readSettings();
            return true;
        }
        if (strcmp(ResetSP.name, name) == 0)
        {
            IUResetSwitch(&ResetSP);

            if (reset())
            {
                ResetSP.s = IPS_OK;
                readSettings();
            }
            else
                ResetSP.s = IPS_ALERT;

            IDSetSwitch(&ResetSP, nullptr);
            return true;
        }
    }

    return INDI::DefaultDevice::ISNewSwitch(dev, name, states, names, n);
}

bool USBDewpoint::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        if (strcmp(name, OutputsNP.name) == 0)
        {
            // Warn if we are in auto mode
            int target_mode = IUFindOnSwitchIndex(&AutoModeSP);
            if (target_mode == 1)
            {
                LOG_WARN("Setting output power is ignored in auto mode!");
                return true;
            }
            IUUpdateNumber(&OutputsNP, values, names, n);
            OutputsNP.s = IPS_BUSY;
            IDSetNumber(&OutputsNP, nullptr);
            setOutput(1, OutputsN[0].value);
            setOutput(2, OutputsN[1].value);
            setOutput(3, OutputsN[2].value);
            readSettings();
            return true;
        }
        if (strcmp(name, CalibrationsNP.name) == 0)
        {
            IUUpdateNumber(&CalibrationsNP, values, names, n);
            CalibrationsNP.s = IPS_BUSY;
            IDSetNumber(&CalibrationsNP, nullptr);
            setCalibrations(CalibrationsN[0].value, CalibrationsN[1].value, CalibrationsN[2].value);
            readSettings();
            return true;
        }
        if (strcmp(name, ThresholdsNP.name) == 0)
        {
            IUUpdateNumber(&ThresholdsNP, values, names, n);
            ThresholdsNP.s = IPS_BUSY;
            IDSetNumber(&ThresholdsNP, nullptr);
            setThresholds(ThresholdsN[0].value, ThresholdsN[1].value);
            readSettings();
            return true;
        }
        if (strcmp(name, AggressivityNP.name) == 0)
        {
            IUUpdateNumber(&AggressivityNP, values, names, n);
            AggressivityNP.s = IPS_BUSY;
            IDSetNumber(&AggressivityNP, nullptr);
            setAggressivity(AggressivityN[0].value);
            readSettings();
            return true;
        }
        if (strcmp(name, FWversionNP.name) == 0)
        {
            IUUpdateNumber(&FWversionNP, values, names, n);
            FWversionNP.s = IPS_OK;
            IDSetNumber(&FWversionNP, nullptr);
            return true;
        }
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
        TemperaturesN[0].value = temp1;
        TemperaturesN[1].value = temp2;
        TemperaturesN[2].value = temp_ambient;
        TemperaturesNP.s       = IPS_OK;
        IDSetNumber(&TemperaturesNP, nullptr);

        HumidityN[0].value = humidity;
        HumidityNP.s       = IPS_OK;
        IDSetNumber(&HumidityNP, nullptr);

        DewpointN[0].value = dewpoint;
        DewpointNP.s       = IPS_OK;
        IDSetNumber(&DewpointNP, nullptr);

        OutputsN[0].value = output1;
        OutputsN[1].value = output2;
        OutputsN[2].value = output3;
        OutputsNP.s       = IPS_OK;
        IDSetNumber(&OutputsNP, nullptr);

        CalibrationsN[0].value = calibration1;
        CalibrationsN[1].value = calibration2;
        CalibrationsN[2].value = calibration_ambient;
        CalibrationsNP.s       = IPS_OK;
        IDSetNumber(&CalibrationsNP, nullptr);

        ThresholdsN[0].value = threshold1;
        ThresholdsN[1].value = threshold2;
        ThresholdsNP.s       = IPS_OK;
        IDSetNumber(&ThresholdsNP, nullptr);

        IUResetSwitch(&AutoModeSP);
        AutoModeS[automode].s = ISS_ON;
        AutoModeSP.s          = IPS_OK;
        IDSetSwitch(&AutoModeSP, nullptr);

        IUResetSwitch(&LinkOut23SP);
        LinkOut23S[linkout23].s = ISS_ON;
        LinkOut23SP.s           = IPS_OK;
        IDSetSwitch(&LinkOut23SP, nullptr);

        AggressivityN[0].value = aggressivity;
        AggressivityNP.s       = IPS_OK;
        IDSetNumber(&AggressivityNP, nullptr);
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
