/*******************************************************************************
  Copyright(c) 2026 Dominik Merkel. All rights reserved.

  Pegasus FlatMaster Neo

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

#include "pegasus_flatmaster_neo.h"

#include "indicom.h"
#include "connectionplugins/connectionserial.h"
#include "connectionplugins/connectiontcp.h"

#include <algorithm>
#include <cerrno>
#include <cmath>
#include <cstring>
#include <memory>
#include <regex>
#include <termios.h>
#include <unistd.h>
#include <inttypes.h>
#include <sys/ioctl.h>
#include <vector>
#include <math.h>


static std::unique_ptr<PegasusFlatMasterNeo> flatmaster(new PegasusFlatMasterNeo());


PegasusFlatMasterNeo::PegasusFlatMasterNeo() : LightBoxInterface(this), DustCapInterface(this), WeatherInterface(this)
{
    setVersion(1, 5);
}

bool PegasusFlatMasterNeo::initProperties()
{
    INDI::DefaultDevice::initProperties();

    // Firmware version
    IUFillText(&FirmwareT[0], "Version", "Version", nullptr);
    IUFillTextVector(&FirmwareTP, FirmwareT, 1, getDeviceName(), "Firmware", "Firmware", MAIN_CONTROL_TAB, IP_RO, 60, IPS_IDLE);

    LI::initProperties(LIGHTBOX_TAB, LI::CAN_DIM);
    DI::initProperties(DUSTCAP_TAB, DI::CAN_ABORT);
    WI::initProperties(ENVIRONMENT_TAB, ENVIRONMENT_TAB);

    setDriverInterface(AUX_INTERFACE | LIGHTBOX_INTERFACE | DUSTCAP_INTERFACE | WEATHER_INTERFACE);

    addParameter("WEATHER_TEMPERATURE", "Temperature (C)", -15, 35, 15);
    addParameter("WEATHER_HUMIDITY", "Humidity %", 0, 100, 15);
    addParameter("WEATHER_DEWPOINT", "Dew Point (C)", 0, 100, 15);
    setCriticalParameter("WEATHER_TEMPERATURE");

    WeatherOffsetNP[OFFSET_TEMPERATURE].fill("OFFSET_TEMPERATURE", "Temperature Offset (C)", "%.1f", -20, 20, 0.1, 0);
    WeatherOffsetNP[OFFSET_HUMIDITY].fill(   "OFFSET_HUMIDITY",    "Humidity Offset (%)",    "%.0f", -50, 50, 1, 0);
    WeatherOffsetNP.fill(getDeviceName(), "WEATHER_OFFSET", "Calibration", ENVIRONMENT_TAB, IP_RW, 60, IPS_IDLE);

    LightIntensityNP[0].setMin(0);
    LightIntensityNP[0].setMax(100);
    LightIntensityNP[0].setStep(1);

    // Dew Control
    DewHeaterNP[0].fill("DEW_HEATER_VALUE", "Heater (%)", "%.0f", 0, 100, 1, 0);
    DewHeaterNP.fill(getDeviceName(), "DEW_HEATER", "Dew Heater", DEW_TAB, IP_RW, 60, IPS_IDLE);

    AutoDewSP[0].fill("AUTO_DEW_ENABLE", "Enable", ISS_OFF);
    AutoDewSP[1].fill("AUTO_DEW_DISABLE", "Disable", ISS_ON);
    AutoDewSP.fill(getDeviceName(), "AUTO_DEW", "Auto Dew", DEW_TAB, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);

    // Cap opening angle
    CapAngleNP[0].fill("CAP_ANGLE_VALUE", "Angle (°)", "%.0f", 0, 270, 1, 270);
    CapAngleNP.fill(getDeviceName(), "CAP_ANGLE", "Opening Angle", DUSTCAP_TAB, IP_RW, 60, IPS_IDLE);

    // Device overview (FA)
    DeviceStatusNP[STATUS_LIGHT_INTENSITY].fill( "STATUS_LIGHT_INTENSITY", "Light Intensity",       "%.0f", 0, 100,  1, 0);
    DeviceStatusNP[STATUS_LIGHT_ACTIVE].fill(    "STATUS_LIGHT_ACTIVE",    "Light Active",          "%.0f", 0, 1,    1, 0);
    DeviceStatusNP[STATUS_CAP_ACTUAL_ANGLE].fill("STATUS_CAP_ACTUAL",      "Cap Actual Angle (°)",  "%.0f", 0, 270,  1, 0);
    DeviceStatusNP[STATUS_CAP_TARGET_ANGLE].fill("STATUS_CAP_TARGET",      "Cap Target Angle (°)",  "%.0f", 0, 270,  1, 0);
    DeviceStatusNP[STATUS_CAP_STATUS].fill(      "STATUS_CAP_STATUS",      "Cap Status",            "%.0f", 0, 9999, 1, 0);
    DeviceStatusNP[STATUS_DEW_POWER].fill(        "STATUS_DEW_POWER",          "Dew Power",               "%.0f", 0, 100, 1, 0);
    DeviceStatusNP[STATUS_AUTO_DEW].fill(        "STATUS_AUTO_DEW",         "Auto Dew Active",       "%.0f", 0, 1,    1, 0);
    DeviceStatusNP[STATUS_DEW_AGGRESSIVE].fill(        "STATUS_DEW_AGGRESSIVE",          "Dew Aggressiveness (0-10)",     "%.0f", 0, 10, 1, 0);
    DeviceStatusNP[STATUS_LIGHT_SENSOR].fill(        "STATUS_LIGHT_SENSOR",          "Light Sensor",               "%.0f", 0, 100, 1, 0);
    DeviceStatusNP.fill(getDeviceName(), "DEVICE_STATUS", "Device Status", STATUS_TAB, IP_RO, 60, IPS_IDLE);

    // Uptime (FU)
    UptimeNP[0].fill("UPTIME_SECONDS", "Uptime (s)", "%.0f", 0, 1e9, 1, 0);
    UptimeNP.fill(getDeviceName(), "UPTIME", "Uptime", STATUS_TAB, IP_RO, 60, IPS_IDLE);

    // Reboot (FQ)
    RebootSP[0].fill("REBOOT", "Reboot", ISS_OFF);
    RebootSP.fill(getDeviceName(), "DEVICE_REBOOT", "Device", STATUS_TAB, IP_RW, ISR_ATMOST1, 60, IPS_IDLE);

    // Dew aggressiveness logical level 0..10 → DA:<level> + PD:<mapped_byte>
    DewAggressivenessNP[0].fill("DEW_AGGRESSIVENESS_VALUE", "Aggressiveness (0-10)", "%.0f", 0, 10, 1, 5);
    DewAggressivenessNP.fill(getDeviceName(), "DEW_AGGRESSIVENESS", "Dew Aggressiveness", DEW_TAB, IP_RW, 60, IPS_IDLE);

    // Dew threshold (DT:1..50)
    ThresholdNP[0].fill("THRESHOLD_VALUE", "Threshold (1-50)", "%.0f", 1, 50, 1, 10);
    ThresholdNP.fill(getDeviceName(), "DEW_THRESHOLD", "Dew Threshold", DEW_TAB, IP_RW, 60, IPS_IDLE);

    // WiFi channel — AC (read) / AC:<channel> (write), valid 1..11

    // Save dew levels (DSTR)
    SaveDewSP[0].fill("SAVE_DEW_LEVELS", "Save", ISS_OFF);
    SaveDewSP.fill(getDeviceName(), "DEW_SAVE", "Dew Settings", DEW_TAB, IP_RW, ISR_ATMOST1, 60, IPS_IDLE);

    // Auto-close (DC:99 / DC:0 / DC:1)
    AutoCloseSP[0].fill("AUTO_CLOSE_ENABLE",  "Enable",  ISS_OFF);
    AutoCloseSP[1].fill("AUTO_CLOSE_DISABLE", "Disable", ISS_ON);
    AutoCloseSP.fill(getDeviceName(), "AUTO_CLOSE", "Auto Close", DUSTCAP_TAB, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);

    // Hotspot status (A? / AE:0 / AE:1)
    // WiFi factory reset (WZ)
    WifiFactoryResetSP[0].fill("WIFI_FACTORY_RESET", "Reset", ISS_OFF);
    WifiFactoryResetSP.fill(getDeviceName(), "WIFI_FACTORY_RESET", "WiFi Factory Reset", NETWORK_TAB, IP_RW, ISR_ATMOST1, 60, IPS_IDLE);

    addAuxControls();

    wifi.initProperties(NETWORK_TAB,
                        [this](const char *name) { loadConfig(true, name); },
                        [this](INDI::Property & property) { saveConfig(property); });
    // The Neo joins open networks with the placeholder password "PASSWORD", as before.
    wifi.allowOpenNetworks(true);

    serialConnection = new Connection::Serial(this);
    serialConnection->setDefaultBaudRate(Connection::Serial::B_9600);
    serialConnection->setPortMatchPattern("FlatMaster");
    serialConnection->registerHandshake([&]()
    {
        return Ack();
    });
    registerConnection(serialConnection);

    // Over WiFi the device announces itself as fmneo.local and takes the same commands on TCP port 9999.
    tcpConnection = new Connection::TCP(this);
    tcpConnection->setDefaultHost("fmneo.local");
    tcpConnection->setDefaultPort(PegasusAstro::WiFi::TCP_PORT);
    tcpConnection->registerHandshake([&]()
    {
        return Ack();
    });
    registerConnection(tcpConnection);
    return true;
}

void PegasusFlatMasterNeo::ISGetProperties(const char *dev)
{
    INDI::DefaultDevice::ISGetProperties(dev);

    // Get Light box properties
    LI::ISGetProperties(dev);
    wifi.ISGetProperties();
    
}

bool PegasusFlatMasterNeo::updateProperties()
{
    INDI::DefaultDevice::updateProperties();

    if (isConnected())
    {
        defineProperty(&FirmwareTP);
        defineProperty(WeatherOffsetNP);
        defineProperty(DewHeaterNP);
        defineProperty(AutoDewSP);
        defineProperty(DewAggressivenessNP);
        defineProperty(ThresholdNP);
        defineProperty(SaveDewSP);
        defineProperty(AutoCloseSP);
        defineProperty(CapAngleNP);
        defineProperty(DeviceStatusNP);
        defineProperty(UptimeNP);
        defineProperty(RebootSP);
        defineProperty(WifiFactoryResetSP);
        wifi.updateProperties(true, PortFD);
        getWeatherOffsets();
        getThreshold();
        getAutoClose();
    }
    else
    {
        deleteProperty(FirmwareTP.name);
        deleteProperty(WeatherOffsetNP);
        deleteProperty(DewHeaterNP);
        deleteProperty(AutoDewSP);
        deleteProperty(DewAggressivenessNP);
        deleteProperty(ThresholdNP);
        deleteProperty(SaveDewSP);
        deleteProperty(AutoCloseSP);
        deleteProperty(CapAngleNP);
        deleteProperty(DeviceStatusNP);
        deleteProperty(UptimeNP);
        deleteProperty(RebootSP);
        deleteProperty(WifiFactoryResetSP);
        wifi.updateProperties(false, -1);
    }

    LI::updateProperties();
    DI::updateProperties();
    WI::updateProperties();

    // Fetch current device state immediately after all properties are defined
    // so the UI reflects the actual hardware state on connection.
    if (isConnected())
        getStatusData();

    return true;
}

const char *PegasusFlatMasterNeo::getDefaultName()
{
    return "Pegasus FlatMaster Neo";
}

void PegasusFlatMasterNeo::updateFirmwareVersion()
{
    char response[16] = {0};

    if(sendCommand("FV", response))
    {   
        IUSaveText(&FirmwareT[0], response + 3);
        FirmwareTP.s = IPS_OK;
        IDSetText(&FirmwareTP, nullptr);
    }
    else
    {
        FirmwareTP.s = IPS_ALERT;
        LOG_ERROR("Error on updateFirmware.");
    }
}

bool PegasusFlatMasterNeo::Ack()
{
    // Over TCP, log in with the hotspot password before any command.
    m_IsTCP = getActiveConnection() == tcpConnection;
    PortFD = m_IsTCP ? tcpConnection->getPortFD() : serialConnection->getPortFD();
    m_FailedPolls = 0;
    if (m_IsTCP && !wifi.login(PortFD))
        return false;

    // The reply "FMNEO_<id>_A\r\n" is 16 bytes, more than a 16-byte buffer holds with its terminating zero.
    char response[NEO_LEN] = {0};
    if (sendCommand("F#", response))
    {
        if (strstr(response, "FMNEO") != nullptr)
        {
            updateFirmwareVersion();
            return true;
        }
    }
    LOGF_ERROR("Ack failed: unexpected response '%s'", response);
    return false;
}

bool PegasusFlatMasterNeo::EnableLightBox(bool enable)
{
    char cmd[16] = {0};
    snprintf(cmd, 16, "FE:%d", enable ? 1 : 0);
    return sendEchoCommand(cmd);
}

bool PegasusFlatMasterNeo::SetLightBoxBrightness(uint16_t value)
{
    char cmd[16] = {0};
    snprintf(cmd, 16, "FL:%d", value);
    if (!sendEchoCommand(cmd))
        return false;

    // Brightness 0 also switches the panel off, as the Pegasus software does.
    if (value == 0)
        return EnableLightBox(false);

    return true;
}

bool PegasusFlatMasterNeo::ignoredWhileLoadingConfig(const char *dev, const char *name) const
{
    // Versions up to 1.2 saved these settings, so older configurations still contain them.
    static const char *const stored[] = {"DEW_HEATER", "CAP_ANGLE", "WEATHER_OFFSET", "DEW_AGGRESSIVENESS",
                                         "DEW_THRESHOLD", "WIFI_CHANNEL", "HOTSPOT_CREDENTIALS", "AUTO_DEW",
                                         "AUTO_CLOSE", "HOTSPOT_STATUS"
                                        };
    if (!isConfigLoading() || dev == nullptr || strcmp(dev, getDeviceName()) != 0)
        return false;

    for (const char *property : stored)
        if (strcmp(name, property) == 0)
            return true;
    return false;
}

bool PegasusFlatMasterNeo::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    if (ignoredWhileLoadingConfig(dev, name))
        return true;

    if (wifi.ISNewNumber(dev, name, values, names, n))
        return true;

    if (LI::processNumber(dev, name, values, names, n))
        return true;

    if (strstr(name, "WEATHER_"))
    {
        if (WI::processNumber(dev, name, values, names, n))
            return true;
    }

    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        if (DewHeaterNP.isNameMatch(name))
        {
            if (setDewHeater(static_cast<uint8_t>(values[0])))
            {
                DewHeaterNP.update(values, names, n);
                DewHeaterNP.setState(IPS_OK);
            }
            else
                DewHeaterNP.setState(IPS_ALERT);
            DewHeaterNP.apply();
            return true;
        }

        if (CapAngleNP.isNameMatch(name))
        {
            if (setCapAngle(static_cast<uint16_t>(values[0])))
            {
                CapAngleNP.update(values, names, n);
                CapAngleNP.setState(IPS_OK);
            }
            else
                CapAngleNP.setState(IPS_ALERT);
            CapAngleNP.apply();
            return true;
        }

        if (WeatherOffsetNP.isNameMatch(name))
        {
            bool ok = true;
            for (int i = 0; i < n; i++)
            {
                if (strcmp(names[i], "OFFSET_TEMPERATURE") == 0)
                    ok &= setTemperatureOffset(values[i]);
                else if (strcmp(names[i], "OFFSET_HUMIDITY") == 0)
                    ok &= setHumidityOffset(values[i]);
            }
            // Show the offsets the device actually uses.
            if (!getWeatherOffsets())
                ok = false;
            if (!ok)
            {
                WeatherOffsetNP.setState(IPS_ALERT);
                WeatherOffsetNP.apply();
            }
            return true;
        }

        if (DewAggressivenessNP.isNameMatch(name))
        {
            if (setDewAggressiveness(static_cast<uint8_t>(values[0])))
            {
                DewAggressivenessNP.update(values, names, n);
                DewAggressivenessNP.setState(IPS_OK);
            }
            else
                DewAggressivenessNP.setState(IPS_ALERT);
            DewAggressivenessNP.apply();
            return true;
        }

        if (ThresholdNP.isNameMatch(name))
        {
            if (setThreshold(static_cast<uint8_t>(values[0])))
            {
                ThresholdNP.update(values, names, n);
                ThresholdNP.setState(IPS_OK);
            }
            else
                ThresholdNP.setState(IPS_ALERT);
            ThresholdNP.apply();
            return true;
        }

    }

    return INDI::DefaultDevice::ISNewNumber(dev, name, values, names, n);
}

bool PegasusFlatMasterNeo::ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n)
{
    // Before the WiFi helper, so old configurations do not send hotspot credentials to the device.
    if (ignoredWhileLoadingConfig(dev, name))
        return true;

    if (wifi.ISNewText(dev, name, texts, names, n))
        return true;

    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        if (LI::processText(dev, name, texts, names, n))
            return true;


    }

    return INDI::DefaultDevice::ISNewText(dev, name, texts, names, n);
}

bool PegasusFlatMasterNeo::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    // Before the WiFi helper, so old configurations do not switch the hotspot.
    if (ignoredWhileLoadingConfig(dev, name))
        return true;

    if (wifi.ISNewSwitch(dev, name, states, names, n))
        return true;

    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        if (DI::processSwitch(dev, name, states, names, n))
            return true;

        if (LI::processSwitch(dev, name, states, names, n))
            return true;

        if (WI::processSwitch(dev, name, states, names, n))
            return true;

        if (AutoDewSP.isNameMatch(name))
        {
            AutoDewSP.update(states, names, n);
            bool enable = AutoDewSP[0].getState() == ISS_ON;
            if (setAutoDew(enable))
                AutoDewSP.setState(IPS_OK);
            else
                AutoDewSP.setState(IPS_ALERT);
            AutoDewSP.apply();
            return true;
        }

        if (RebootSP.isNameMatch(name))
        {
            // FQ restarts the device at once and sends no reply; its USB port goes away for a moment,
            // so disconnect instead of polling a dead port.
            int nbytes_written = 0;
            tty_write_string(PortFD, m_IsTCP ? "FQ\r\n" : "FQ\n", &nbytes_written);
            RebootSP.setState(IPS_OK);
            RebootSP.apply();
            LOG_INFO("Device restarts. Connect again in a few seconds.");
            Disconnect();
            setConnected(false, IPS_IDLE);
            updateProperties();
            return true;
        }

        if (SaveDewSP.isNameMatch(name))
        {
            if (saveDewLevels())
                SaveDewSP.setState(IPS_OK);
            else
                SaveDewSP.setState(IPS_ALERT);
            SaveDewSP.apply();
            return true;
        }

        if (AutoCloseSP.isNameMatch(name))
        {
            AutoCloseSP.update(states, names, n);
            bool enable = AutoCloseSP[0].getState() == ISS_ON;
            if (setAutoClose(enable))
                AutoCloseSP.setState(IPS_OK);
            else
                AutoCloseSP.setState(IPS_ALERT);
            AutoCloseSP.apply();
            return true;
        }




        if (WifiFactoryResetSP.isNameMatch(name))
        {
            wifiFactoryReset();
            WifiFactoryResetSP.setState(IPS_OK);
            WifiFactoryResetSP.apply();
            LOG_INFO("WiFi factory reset sent to device.");
            return true;
        }
    }

    return INDI::DefaultDevice::ISNewSwitch(dev, name, states, names, n);
}

bool PegasusFlatMasterNeo::ISSnoopDevice(XMLEle *root)
{
    LI::snoop(root);

    return INDI::DefaultDevice::ISSnoopDevice(root);
}

bool PegasusFlatMasterNeo::saveConfigItems(FILE *fp)
{
    INDI::DefaultDevice::saveConfigItems(fp);
    LI::saveConfigItems(fp);
    WI::saveConfigItems(fp);
    wifi.saveConfigItems(fp);
    // Dew, cap, offset and WiFi settings are stored in the device and read from it on
    // connection. Saving them here would send old values back every time the config loads.
    return true;
}

bool PegasusFlatMasterNeo::sendCommandBuffer(const char *command, char *res, int res_len, int timeout_sec)
{
    int nbytes_written = 0, nbytes_read = 0, rc = -1;
    char errstr[MAXRBUF];

    // Room for WN:/WP: with a 63-character password; a smaller buffer cut long passwords silently.
    char cmd[128] = {0};
    // Over TCP the firmware only executes a command after a carriage return.
    snprintf(cmd, sizeof(cmd), m_IsTCP ? "%s\r\n" : "%s\n", command);

    // Do not write WiFi and hotspot passwords to the log.
    const bool secret = strncmp(command, "AP:", 3) == 0 || strncmp(command, "WP:", 3) == 0;
    LOGF_DEBUG("CMD <%s>", secret ? "<password>" : command);

    // tcflush() does not discard data on a TCP socket.
    PegasusAstro::WiFi::drain(PortFD);

    if ((rc = tty_write(PortFD, cmd, strlen(cmd), &nbytes_written)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("command: %s error: %s.", cmd, errstr);
        return false;
    }

    // Leave room for the terminating zero.
    if ((rc = tty_nread_section(PortFD, res, res_len - 1, 0xA, timeout_sec, &nbytes_read)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("command: %s error: %s.", command, errstr);
        res[0] = 0;
        // Discard the rest of a reply that did not fit, so the next reply starts clean.
        if (rc == TTY_OVERFLOW)
        {
            // Bounded: after a reboot (FQ) the old port only returns end-of-file, which also
            // counts as an overflow and would otherwise loop forever.
            char discard[256];
            int discarded = 0;
            for (int i = 0; i < 8 && tty_nread_section(PortFD, discard, sizeof(discard), 0xA, 1, &discarded) == TTY_OVERFLOW; i++)
                ;
        }
        tcflush(PortFD, TCIOFLUSH);
        return false;
    }

    // Strip trailing newline and carriage return
    res[nbytes_read - 1] = 0;
    if (nbytes_read > 1 && res[nbytes_read - 2] == '\r')
        res[nbytes_read - 2] = 0;

    // AL answers <ssid>:<password>, and AP:/WP: may echo the password.
    LOGF_DEBUG("RES <%s>", strcmp(command, "AL") == 0 ? "<ssid>:<password>" : secret ? "<password>" : res);

    tcflush(PortFD, TCIOFLUSH);

    return true;
}

bool PegasusFlatMasterNeo::sendEchoCommand(const char *cmd)
{
    char response[32] = {0};
    if (!sendCommand(cmd, response))
        return false;

    if (strcmp(response, cmd) != 0)
    {
        LOGF_ERROR("Unexpected response to %s: '%s'", cmd, response);
        return false;
    }

    return true;
}

// FA reports the aggressiveness either as level 0..10 or as the byte 9..255 sent with PD.
uint8_t PegasusFlatMasterNeo::aggressivenessLevel(double value)
{
    if (value <= 10)
        return static_cast<uint8_t>(std::max(0.0, value));
    return static_cast<uint8_t>(std::min(10.0, std::max(0.0, round((value - 9) * 10.0 / 246.0))));
}

IPState PegasusFlatMasterNeo::ParkCap()
{
    if (!sendEchoCommand("FS:0"))
        return IPS_ALERT;

    // Command accepted — cap is now moving; TimerHit will finalize the state
    m_CapTarget = CAP_STATUS_PARKED;
    m_CapMoveStarted = std::chrono::steady_clock::now();
    return IPS_BUSY;
}

IPState PegasusFlatMasterNeo::UnParkCap()
{
    if (!sendEchoCommand("FS:1"))
        return IPS_ALERT;

    // Command accepted — cap is now moving; TimerHit will finalize the state
    m_CapTarget = CAP_STATUS_UNPARKED;
    m_CapMoveStarted = std::chrono::steady_clock::now();
    return IPS_BUSY;
}

IPState PegasusFlatMasterNeo::AbortCap()
{
    char response[16] = {0};
    if (sendCommand("FH", response) && strstr(response, "FH:1") != nullptr)
        return IPS_OK;
    return IPS_ALERT;
}

bool PegasusFlatMasterNeo::setDewHeater(uint8_t value)
{
    char cmd[16] = {0};
    snprintf(cmd, 16, "DH:%d", value);
    return sendEchoCommand(cmd);
}

bool PegasusFlatMasterNeo::setAutoDew(bool enable)
{
    char cmd[16] = {0};
    snprintf(cmd, 16, "PD:%d", enable ? 1 : 0);

    if (!sendEchoCommand(cmd))
        return false;

    if (!getStatusData())
        return false;

    const bool actual = (DeviceStatusNP[STATUS_AUTO_DEW].getValue() == 1);
    if (actual != enable)
    {
        LOGF_ERROR("AutoDew %s failed: device reports %s.", enable ? "enable" : "disable", actual ? "enabled" : "disabled");
        return false;
    }

    return true;
}

bool PegasusFlatMasterNeo::setCapAngle(uint16_t angle)
{
    char cmd[16] = {0};
    snprintf(cmd, 16, "CD:%d", angle);
    return sendEchoCommand(cmd);
}

void PegasusFlatMasterNeo::TimerHit()
{
    if (!isConnected())
    {
        SetTimer(getCurrentPollingPeriod());
        return;
    }

    // Without a status reply the other queries of this poll would each wait for their timeout
    // as well, which blocks the driver for seconds per poll, so skip them.
    if (!getStatusData())
    {
        // A device that left the WiFi network (e.g. after a WiFi factory reset) keeps the TCP
        // connection open but never answers again, so stop after a few polls in a row.
        if (++m_FailedPolls >= MAX_FAILED_POLLS)
        {
            LOGF_ERROR("No reply to %d status queries in a row, disconnecting. %s", MAX_FAILED_POLLS,
                       m_IsTCP ? "Over WiFi, check over USB that the device is still in the network (Current WiFi)." :
                       "Check the USB connection.");
            Disconnect();
            setConnected(false, IPS_ALERT);
            updateProperties();
        }
        SetTimer(getCurrentPollingPeriod());
        return;
    }
    m_FailedPolls = 0;

    getWeatherData();
    getUptime();
    wifi.updateConnection();

    SetTimer(getCurrentPollingPeriod());
}

bool PegasusFlatMasterNeo::getStatusData()
{
    char response[NEO_LEN] = {0};

    if (!sendCommand("FA", response))
    {
        LOG_ERROR("Error querying device status.");
        return false;
    }

    // FA response: "FMNEO:lightIntensity:lightActive:capActual:capTarget:capStatus:dewPower:autoDew:dewAggressive:lightSensor"
    std::vector<std::string> result = split(response, ":");

    if (result.size() < FA_N)
    {
        LOGF_WARN("Received unexpected FA status format: %s", response);
        return false;
    }

    // Skip update if nothing changed, but always re-evaluate when a park/unpark is in progress
    // to ensure IPS_BUSY is resolved even if the cap was already at the target position.
    if (result == lastStatusData && ParkCapSP.getState() != IPS_BUSY)
        return true;

    try
    {
        const double lightIntensity  = std::stod(result[FA_LIGHT_INTENSITY]);
        const double lightActive     = std::stod(result[FA_LIGHT_ACTIVE]);
        const double capTargetAngle  = std::stod(result[FA_CAP_TARGET_ANGLE]);
        const double capActualAngle  = std::stod(result[FA_CAP_ACTUAL_ANGLE]);
        const double capStatus       = std::stod(result[FA_CAP_STATUS]);
        const double dewPower        = std::stod(result[FA_DEW_POWER]);
        const double autoDew         = std::stod(result[FA_AUTO_DEW_STATUS]);
        const double dewAggressive   = std::stod(result[FA_DEW_AGGRESSIVE]);
        const double lightSensor     = std::stod(result[FA_LIGHT_SENSOR]);  


        // --- Overview tab ---
        DeviceStatusNP[STATUS_LIGHT_INTENSITY].setValue(lightIntensity);
        DeviceStatusNP[STATUS_LIGHT_ACTIVE].setValue(lightActive);
        DeviceStatusNP[STATUS_CAP_TARGET_ANGLE].setValue(capTargetAngle);
        DeviceStatusNP[STATUS_CAP_ACTUAL_ANGLE].setValue(capActualAngle);
        DeviceStatusNP[STATUS_CAP_STATUS].setValue(capStatus);
        DeviceStatusNP[STATUS_DEW_POWER].setValue(dewPower);
        DeviceStatusNP[STATUS_AUTO_DEW].setValue(autoDew);
        DeviceStatusNP[STATUS_DEW_AGGRESSIVE].setValue(aggressivenessLevel(dewAggressive));
        DeviceStatusNP[STATUS_LIGHT_SENSOR].setValue(lightSensor);
        DeviceStatusNP.setState(IPS_OK);
        DeviceStatusNP.apply();

        // --- LightBox interface ---
        const bool lightOn = (lightActive != 0);
        if (LightSP[FLAT_LIGHT_ON].getState() != (lightOn ? ISS_ON : ISS_OFF))
        {
            LightSP.reset();
            LightSP[lightOn ? FLAT_LIGHT_ON : FLAT_LIGHT_OFF].setState(ISS_ON);
            LightSP.setState(IPS_OK);
            LightSP.apply();
        }
        if (LightIntensityNP[0].getValue() != lightIntensity)
        {
            LightIntensityNP[0].setValue(lightIntensity);
            LightIntensityNP.setState(IPS_OK);
            LightIntensityNP.apply();
        }

        // --- DustCap interface ---
        // Any other value (e.g. in-motion) leaves IPS_BUSY in place until the
        // final state arrives; the switch selection is not changed mid-motion.
        if (ParkCapSP.getState() == IPS_BUSY && (capStatus == CAP_STATUS_PARKED || capStatus == CAP_STATUS_UNPARKED)
                && capStatus != m_CapTarget)
        {
            // Right after FS:x the cap may still report the old end position.
            if (std::chrono::steady_clock::now() - m_CapMoveStarted > CAP_START_TIMEOUT)
            {
                LOGF_ERROR("Cap did not move, it is still %s.", capStatus == CAP_STATUS_PARKED ? "closed" : "open");
                ParkCapSP.reset();
                ParkCapSP[capStatus == CAP_STATUS_PARKED ? CAP_PARK : CAP_UNPARK].setState(ISS_ON);
                ParkCapSP.setState(IPS_ALERT);
                ParkCapSP.apply();
            }
        }
        else if (capStatus == CAP_STATUS_PARKED)
        {
            if (ParkCapSP[CAP_PARK].getState() != ISS_ON || ParkCapSP.getState() != IPS_OK)
            {
                ParkCapSP.reset();
                ParkCapSP[CAP_PARK].setState(ISS_ON);
                ParkCapSP.setState(IPS_OK);
                ParkCapSP.apply();
            }
        }
        else if (capStatus == CAP_STATUS_UNPARKED)
        {
            if (ParkCapSP[CAP_UNPARK].getState() != ISS_ON || ParkCapSP.getState() != IPS_OK)
            {
                ParkCapSP.reset();
                ParkCapSP[CAP_UNPARK].setState(ISS_ON);
                ParkCapSP.setState(IPS_OK);
                ParkCapSP.apply();
            }
        }
        if (CapAngleNP[0].getValue() != capTargetAngle)
        {
            CapAngleNP[0].setValue(capTargetAngle);
            CapAngleNP.setState(IPS_OK);
            CapAngleNP.apply();
        }

        // --- Dew controls: follow the device, which may also be set from Pegasus Unity ---
        if (DewHeaterNP[0].getValue() != dewPower || DewHeaterNP.getState() == IPS_IDLE)
        {
            DewHeaterNP[0].setValue(dewPower);
            DewHeaterNP.setState(IPS_OK);
            DewHeaterNP.apply();
        }
        const bool autoDewOn = (autoDew != 0);
        if (AutoDewSP[0].getState() != (autoDewOn ? ISS_ON : ISS_OFF) || AutoDewSP.getState() == IPS_IDLE)
        {
            AutoDewSP.reset();
            AutoDewSP[autoDewOn ? 0 : 1].setState(ISS_ON);
            AutoDewSP.setState(IPS_OK);
            AutoDewSP.apply();
        }
        const uint8_t level = aggressivenessLevel(dewAggressive);
        if (DewAggressivenessNP[0].getValue() != level || DewAggressivenessNP.getState() == IPS_IDLE)
        {
            DewAggressivenessNP[0].setValue(level);
            DewAggressivenessNP.setState(IPS_OK);
            DewAggressivenessNP.apply();
        }
    }
    catch (const std::exception &e)
    {
        LOGF_ERROR("Error parsing FA status data: %s", e.what());
        return false;
    }

    lastStatusData = result;
    return true;
}

bool PegasusFlatMasterNeo::getWeatherOffsets()
{
    char response[NEO_LEN] = {0};

    if (!sendCommand("CR", response))
    {
        LOG_ERROR("Error querying weather offsets.");
        return false;
    }

    // CR response: "CR:temp_offset:humidity_offset"
    std::vector<std::string> result = split(response, ":");

    if (result.size() < CR_N)
    {
        LOGF_WARN("Received unexpected CR offset format: %s", response);
        return false;
    }

    try
    {
        WeatherOffsetNP[OFFSET_TEMPERATURE].setValue(std::stod(result[CR_TEMP_OFFSET]));
        WeatherOffsetNP[OFFSET_HUMIDITY].setValue(std::stod(result[CR_HUM_OFFSET]));
        WeatherOffsetNP.setState(IPS_OK);
        WeatherOffsetNP.apply();
    }
    catch (const std::exception &e)
    {
        LOGF_ERROR("Error parsing CR offset data: %s", e.what());
        return false;
    }

    return true;
}

bool PegasusFlatMasterNeo::setTemperatureOffset(double value)
{
    char cmd[16] = {0};
    char response[16] = {0};
    snprintf(cmd, 16, "CT:%.1f", value);
    // The reply is not checked, as in the Pegasus software; the caller reads the offsets back with CR.
    if (!sendCommand(cmd, response))
        return false;
    LOGF_DEBUG("CT reply: %s", response);
    return true;
}

bool PegasusFlatMasterNeo::setHumidityOffset(double value)
{
    // The humidity offset is a whole percentage, -50..50.
    char cmd[16] = {0};
    snprintf(cmd, 16, "CH:%d", static_cast<int>(std::lround(value)));
    return sendEchoCommand(cmd);
}

bool PegasusFlatMasterNeo::getWeatherData()
{
    char response[NEO_LEN] = {0};

    if (!sendCommand("ES", response))
    {
        LOG_ERROR("Error querying weather data.");
        return false;
    }

    // ES response: "FlatMasterNeo:temperature:humidity:dewpoint"
    // Field separator is ':' — adjust if device uses a different delimiter
    std::vector<std::string> result = split(response, ":");

    if (result.size() < WI_N)
    {
        LOGF_WARN("Received unexpected weather data format: %s", response);
        return false;
    }

    if (result == lastWeatherData)
        return true;

    try
    {
        // The device reports nan or -127 without a sensor; do not pass that on as a temperature.
        const double temperature = std::stod(result[WI_TEMPERATURE]);
        if (std::isnan(temperature) || temperature <= -127 || temperature > 80)
        {
            if (lastWeatherData.empty())
                LOG_WARN("No temperature/humidity sensor data available.");
            lastWeatherData = result;
            return false;
        }
        setParameterValue("WEATHER_TEMPERATURE", temperature);
        setParameterValue("WEATHER_HUMIDITY", std::fabs(std::stod(result[WI_HUMIDITY])));
        setParameterValue("WEATHER_DEWPOINT", std::stod(result[WI_DEWPOINT]));
    }
    catch (const std::exception &e)
    {
        LOGF_ERROR("Error parsing weather data: %s", e.what());
        return false;
    }

    if (WI::syncCriticalParameters())
        critialParametersLP.apply();

    ParametersNP.setState(IPS_OK);
    ParametersNP.apply();

    lastWeatherData = result;
    return true;
}

bool PegasusFlatMasterNeo::getUptime()
{
    char response[16] = {0};
    if (!sendCommand("FU", response))
        return false;

    // Response is either a plain integer or FU:<seconds>
    const char *val = strstr(response, "FU:");
    UptimeNP[0].setValue(atof(val ? val + 3 : response));
    UptimeNP.setState(IPS_OK);
    UptimeNP.apply();
    return true;
}

bool PegasusFlatMasterNeo::getThreshold()
{
    char response[16] = {0};
    if (!sendCommand("DT", response))
        return false;

    const char *val = strstr(response, "DT:");
    if (!val)
        return false;

    int threshold = atoi(val + 3);
    if (threshold < 1 || threshold > 50)
    {
        LOGF_WARN("DT returned %d which is outside the valid range [1..50], ignoring.", threshold);
        return false;
    }

    ThresholdNP[0].setValue(threshold);
    ThresholdNP.setState(IPS_OK);
    ThresholdNP.apply();
    return true;
}

bool PegasusFlatMasterNeo::setThreshold(uint8_t value)
{
    if (value < 1 || value > 50)
    {
        LOGF_ERROR("Threshold %d is outside the valid range [1..50].", value);
        return false;
    }
    char cmd[16] = {0};
    snprintf(cmd, sizeof(cmd), "DT:%d", value);
    return sendEchoCommand(cmd);
}

// level 0..10 → DA:<level>. Firmware 1.4 applies it right away (FA field 8 follows). The Unity
// Platform also sends PD:<9..255>, but the firmware answers that with PD:1 and treats it as
// "auto dew on", so it would switch auto dew on as a side effect.
bool PegasusFlatMasterNeo::setDewAggressiveness(uint8_t level)
{
    char cmd[16] = {0};
    snprintf(cmd, sizeof(cmd), "DA:%d", level);
    return sendEchoCommand(cmd);
}

bool PegasusFlatMasterNeo::saveDewLevels()
{
    // Pegasus firmware answers an unknown command with ">>"; firmware 1.4 does so for DSTR.
    char response[16] = {0};
    sendCommand("DSTR", response);
    if (strncmp(response, ">>", 2) == 0)
    {
        LOG_WARN("This firmware does not support saving the dew levels (DSTR).");
        return false;
    }
    return true;
}

bool PegasusFlatMasterNeo::getAutoClose()
{
    char response[16] = {0};
    if (!sendCommand("DC:99", response))
        return false;

    const bool on = (strcmp(response, "DC:1") == 0);
    AutoCloseSP.reset();
    AutoCloseSP[on ? 0 : 1].setState(ISS_ON);
    AutoCloseSP.setState(IPS_OK);
    AutoCloseSP.apply();
    return true;
}

bool PegasusFlatMasterNeo::setAutoClose(bool enable)
{
    char cmd[16] = {0};
    snprintf(cmd, sizeof(cmd), "DC:%d", enable ? 1 : 0);
    return sendEchoCommand(cmd);
}

bool PegasusFlatMasterNeo::wifiFactoryReset()
{
    // WZ sends without waiting for a response (Channel.Write behavior). Over TCP the firmware only
    // executes a command after a carriage return.
    int nbytes_written = 0;
    return tty_write_string(PortFD, m_IsTCP ? "WZ\r\n" : "WZ\n", &nbytes_written) == TTY_OK;
}

std::vector<std::string> PegasusFlatMasterNeo::split(const std::string &input, const std::string &sep)
{
    std::vector<std::string> result;
    std::regex re(sep);
    std::sregex_token_iterator it(input.begin(), input.end(), re, -1);
    std::sregex_token_iterator end;
    for (; it != end; ++it)
        result.push_back(it->str());
    return result;
}