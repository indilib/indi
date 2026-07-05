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

#include <cerrno>
#include <cstring>
#include <memory>
#include <regex>
#include <termios.h>
#include <unistd.h>
#include <inttypes.h>
#include <sys/ioctl.h>
#include <math.h>


static std::unique_ptr<PegasusFlatMasterNeo> flatmaster(new PegasusFlatMasterNeo());


PegasusFlatMasterNeo::PegasusFlatMasterNeo() : LightBoxInterface(this), DustCapInterface(this), WeatherInterface(this)
{
    setVersion(1, 0);
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
    WeatherOffsetNP[OFFSET_HUMIDITY].fill(   "OFFSET_HUMIDITY",    "Humidity Offset (%)",    "%.1f", -20, 20, 0.1, 0);
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
    WifiChannelNP[0].fill("WIFI_CHANNEL_VALUE", "Channel (1-11)", "%.0f", 1, 11, 1, 6);
    WifiChannelNP.fill(getDeviceName(), "WIFI_CHANNEL", "WiFi Channel", NETWORK_TAB, IP_RW, 60, IPS_IDLE);

    // Save dew levels (DSTR)
    SaveDewSP[0].fill("SAVE_DEW_LEVELS", "Save", ISS_OFF);
    SaveDewSP.fill(getDeviceName(), "DEW_SAVE", "Dew Settings", DEW_TAB, IP_RW, ISR_ATMOST1, 60, IPS_IDLE);

    // Auto-close (DC:99 / DC:0 / DC:1)
    AutoCloseSP[0].fill("AUTO_CLOSE_ENABLE",  "Enable",  ISS_OFF);
    AutoCloseSP[1].fill("AUTO_CLOSE_DISABLE", "Disable", ISS_ON);
    AutoCloseSP.fill(getDeviceName(), "AUTO_CLOSE", "Auto Close", DUSTCAP_TAB, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);

    // Hotspot status (A? / AE:0 / AE:1)
    HotspotStatusSP[0].fill("HOTSPOT_ENABLE",  "Enable",  ISS_OFF);
    HotspotStatusSP[1].fill("HOTSPOT_DISABLE", "Disable", ISS_ON);
    HotspotStatusSP.fill(getDeviceName(), "HOTSPOT_STATUS", "Hotspot", NETWORK_TAB, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);

    // Hotspot credentials (AL / AN:<ssid> / AP:<password>)
    HotspotCredentialsTP[0].fill("HOTSPOT_SSID",     "SSID",     nullptr);
    HotspotCredentialsTP[1].fill("HOTSPOT_PASSWORD", "Password", nullptr);
    HotspotCredentialsTP.fill(getDeviceName(), "HOTSPOT_CREDENTIALS", "Hotspot Credentials", NETWORK_TAB, IP_RW, 60, IPS_IDLE);

    // Current WiFi connection (WI)
    CurrentWifiTP[0].fill("CURRENT_WIFI", "Connection", nullptr);
    CurrentWifiTP.fill(getDeviceName(), "CURRENT_WIFI", "Current WiFi", NETWORK_TAB, IP_RO, 60, IPS_IDLE);

    // WiFi network scan (WS — 15 s timeout)
    WifiScanSP[0].fill("WIFI_SCAN", "Scan", ISS_OFF);
    WifiScanSP.fill(getDeviceName(), "WIFI_SCAN", "Scan Networks", NETWORK_TAB, IP_RW, ISR_ATMOST1, 60, IPS_IDLE);
    WifiScanTP[0].fill("WIFI_SCAN_RESULTS", "Networks", nullptr);
    WifiScanTP.fill(getDeviceName(), "WIFI_SCAN_RESULTS", "Scan Results", NETWORK_TAB, IP_RO, 60, IPS_IDLE);

    // WiFi connect (WN:<ssid> / WP:<password>)
    WifiConnectTP[0].fill("WIFI_SSID",     "SSID",     nullptr);
    WifiConnectTP[1].fill("WIFI_PASSWORD", "Password", nullptr);
    WifiConnectTP.fill(getDeviceName(), "WIFI_CONNECT_CREDS", "WiFi Credentials", NETWORK_TAB, IP_RW, 60, IPS_IDLE);
    WifiConnectSP[0].fill("WIFI_CONNECT", "Connect", ISS_OFF);
    WifiConnectSP.fill(getDeviceName(), "WIFI_CONNECT", "Connect to WiFi", NETWORK_TAB, IP_RW, ISR_ATMOST1, 60, IPS_IDLE);

    // WiFi factory reset (WZ)
    WifiFactoryResetSP[0].fill("WIFI_FACTORY_RESET", "Reset", ISS_OFF);
    WifiFactoryResetSP.fill(getDeviceName(), "WIFI_FACTORY_RESET", "WiFi Factory Reset", NETWORK_TAB, IP_RW, ISR_ATMOST1, 60, IPS_IDLE);

    addAuxControls();

    serialConnection = new Connection::Serial(this);
    serialConnection->setDefaultBaudRate(Connection::Serial::B_9600);
    serialConnection->setPortMatchPattern("FlatMaster");
    serialConnection->registerHandshake([&]()
    {
        return Ack();
    });


    registerConnection(serialConnection);
    return true;
}

void PegasusFlatMasterNeo::ISGetProperties(const char *dev)
{
    INDI::DefaultDevice::ISGetProperties(dev);

    // Get Light box properties
    LI::ISGetProperties(dev);
    
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
        defineProperty(WifiChannelNP);
        defineProperty(HotspotStatusSP);
        defineProperty(HotspotCredentialsTP);
        defineProperty(CurrentWifiTP);
        defineProperty(WifiScanSP);
        defineProperty(WifiScanTP);
        defineProperty(WifiConnectTP);
        defineProperty(WifiConnectSP);
        defineProperty(WifiFactoryResetSP);
        getWeatherOffsets();
        getThreshold();
        getAutoClose();
        getWifiChannel();
        getHotspotStatus();
        getHotspot();
        getCurrentWifi();
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
        deleteProperty(WifiChannelNP);
        deleteProperty(HotspotStatusSP);
        deleteProperty(HotspotCredentialsTP);
        deleteProperty(CurrentWifiTP);
        deleteProperty(WifiScanSP);
        deleteProperty(WifiScanTP);
        deleteProperty(WifiConnectTP);
        deleteProperty(WifiConnectSP);
        deleteProperty(WifiFactoryResetSP);
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
    PortFD = serialConnection->getPortFD();

    char response[16] = {0};
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
    char response[16] = {0};
    char cmd[16] = {0};

    snprintf(cmd, 16, "FE:%d", enable ? 1 : 0);

    if (!sendCommand(cmd, response))
    {
        LOGF_ERROR("EnableLightBox: send failed. %s", response);
        return false;
    }

    if (strstr(response, "FE:") == nullptr)
    {
        LOGF_ERROR("EnableLightBox: unexpected response '%s'", response);
        return false;
    }

    return true;
}

bool PegasusFlatMasterNeo::SetLightBoxBrightness(uint16_t value)
{
    char response[16] = {0};
    char cmd[16] = {0};

    snprintf(cmd, 16, "FL:%d", value);

    if (!sendCommand(cmd, response))
    {
        LOGF_ERROR("SetLightBoxBrightness: send failed. %s", response);
        return false;
    }

    if (strstr(response, "FL:") == nullptr)
    {
        LOGF_ERROR("SetLightBoxBrightness: unexpected response '%s'", response);
        return false;
    }

    return true;
}

bool PegasusFlatMasterNeo::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
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
            if (ok)
            {
                WeatherOffsetNP.update(values, names, n);
                WeatherOffsetNP.setState(IPS_OK);
            }
            else
                WeatherOffsetNP.setState(IPS_ALERT);
            WeatherOffsetNP.apply();
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

        if (WifiChannelNP.isNameMatch(name))
        {
            if (setWifiChannel(static_cast<uint8_t>(values[0])))
            {
                WifiChannelNP.update(values, names, n);
                WifiChannelNP.setState(IPS_OK);
            }
            else
                WifiChannelNP.setState(IPS_ALERT);
            WifiChannelNP.apply();
            return true;
        }
    }

    return INDI::DefaultDevice::ISNewNumber(dev, name, values, names, n);
}

bool PegasusFlatMasterNeo::ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        if (LI::processText(dev, name, texts, names, n))
            return true;

        if (HotspotCredentialsTP.isNameMatch(name))
        {
            HotspotCredentialsTP.update(texts, names, n);
            const char *ssid = HotspotCredentialsTP[0].getText();
            const char *pwd  = HotspotCredentialsTP[1].getText();
            if (ssid && pwd && setHotspot(ssid, pwd))
                HotspotCredentialsTP.setState(IPS_OK);
            else
                HotspotCredentialsTP.setState(IPS_ALERT);
            HotspotCredentialsTP.apply();
            return true;
        }

        if (WifiConnectTP.isNameMatch(name))
        {
            WifiConnectTP.update(texts, names, n);
            WifiConnectTP.setState(IPS_OK);
            WifiConnectTP.apply();
            return true;
        }
    }

    return INDI::DefaultDevice::ISNewText(dev, name, texts, names, n);
}

bool PegasusFlatMasterNeo::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
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
            char response[16] = {0};
            sendCommand("FQ", response);
            RebootSP.setState(IPS_OK);
            RebootSP.apply();
            LOG_INFO("Reboot command sent to device.");
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

        if (HotspotStatusSP.isNameMatch(name))
        {
            HotspotStatusSP.update(states, names, n);
            bool enable = HotspotStatusSP[0].getState() == ISS_ON;
            if (setHotspotStatus(enable))
                HotspotStatusSP.setState(IPS_OK);
            else
                HotspotStatusSP.setState(IPS_ALERT);
            HotspotStatusSP.apply();
            return true;
        }

        if (WifiScanSP.isNameMatch(name))
        {
            if (scanWifiNetworks())
                WifiScanSP.setState(IPS_OK);
            else
                WifiScanSP.setState(IPS_ALERT);
            WifiScanSP.apply();
            return true;
        }

        if (WifiConnectSP.isNameMatch(name))
        {
            const char *ssid = WifiConnectTP[0].getText();
            const char *pwd  = WifiConnectTP[1].getText();
            if (ssid && connectToWifi(ssid, pwd))
            {
                WifiConnectSP.setState(IPS_OK);
                getCurrentWifi();
            }
            else
                WifiConnectSP.setState(IPS_ALERT);
            WifiConnectSP.apply();
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
    WeatherOffsetNP.save(fp);
    DewHeaterNP.save(fp);
    AutoDewSP.save(fp);
    DewAggressivenessNP.save(fp);
    ThresholdNP.save(fp);
    CapAngleNP.save(fp);
    WifiChannelNP.save(fp);
    return true;
}

bool PegasusFlatMasterNeo::sendCommand(const char *command, char *res, int timeout_sec)
{
    int nbytes_written = 0, nbytes_read = 0, rc = -1;
    char errstr[MAXRBUF];

    char cmd[32] = {0};
    snprintf(cmd, sizeof(cmd), "%s\n", command);

    LOGF_DEBUG("CMD <%s>", command);

    tcflush(PortFD, TCIOFLUSH);

    if ((rc = tty_write(PortFD, cmd, strlen(cmd), &nbytes_written)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("command: %s error: %s.", cmd, errstr);
        return false;
    }

    if ((rc = tty_read_section(PortFD, res, 0xA, timeout_sec, &nbytes_read)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("command: %s error: %s.", cmd, errstr);
        return false;
    }

    // Strip trailing newline and carriage return
    res[nbytes_read - 1] = 0;
    if (nbytes_read > 1 && res[nbytes_read - 2] == '\r')
        res[nbytes_read - 2] = 0;

    LOGF_DEBUG("RES <%s>", res);

    tcflush(PortFD, TCIOFLUSH);

    return true;
}

IPState PegasusFlatMasterNeo::ParkCap()
{
    char response[16] = {0};

    if (!sendCommand("FS:0", response))
    {
        LOGF_ERROR("Error on ParkCap. %s", response);
        return IPS_ALERT;
    }

    if (strstr(response, "FS:") == nullptr)
        return IPS_ALERT;

    // Command accepted — cap is now moving; TimerHit will finalize the state
    return IPS_BUSY;
}

IPState PegasusFlatMasterNeo::UnParkCap()
{
    char response[16] = {0};

    if (!sendCommand("FS:1", response))
    {
        LOGF_ERROR("Error on UnParkCap. %s", response);
        return IPS_ALERT;
    }

    if (strstr(response, "FS:") == nullptr)
        return IPS_ALERT;

    // Command accepted — cap is now moving; TimerHit will finalize the state
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
    char response[16] = {0};
    snprintf(cmd, 16, "DH:%d", value);
    if (sendCommand(cmd, response))
        return strstr(response, "DH:") != nullptr;
    return false;
}

bool PegasusFlatMasterNeo::setAutoDew(bool enable)
{
    char cmd[16] = {0};
    char response[16] = {0};
    snprintf(cmd, 16, "PD:%d", enable ? 1 : 0);

    if (!sendCommand(cmd, response) || strstr(response, "PD:") == nullptr)
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
    char response[16] = {0};
    snprintf(cmd, 16, "CD:%d", angle);
    if (sendCommand(cmd, response))
        return strstr(response, "CD:") != nullptr;
    return false;
}

void PegasusFlatMasterNeo::TimerHit()
{
    if (!isConnected())
    {
        SetTimer(getCurrentPollingPeriod());
        return;
    }

    getStatusData();
    getWeatherData();
    getUptime();
    getCurrentWifi();

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
        DeviceStatusNP[STATUS_DEW_AGGRESSIVE].setValue(dewAggressive);
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
        if (capStatus == CAP_STATUS_PARKED)
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
    if (sendCommand(cmd, response))
        return strstr(response, "CT:") != nullptr;
    return false;
}

bool PegasusFlatMasterNeo::setHumidityOffset(double value)
{
    char cmd[16] = {0};
    char response[16] = {0};
    snprintf(cmd, 16, "CH:%.1f", value);
    if (sendCommand(cmd, response))
        return strstr(response, "CH:") != nullptr;
    return false;
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
        setParameterValue("WEATHER_TEMPERATURE", std::stod(result[WI_TEMPERATURE]));
        setParameterValue("WEATHER_HUMIDITY", std::stod(result[WI_HUMIDITY]));
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
    char response[16] = {0};
    snprintf(cmd, sizeof(cmd), "DT:%d", value);
    if (sendCommand(cmd, response))
        return strstr(response, "DT:") != nullptr;
    return false;
}

// level 0..10 (logical) → DA:<level> (cache) + PD:<mapped_byte> (apply)
// mapped_byte = 9 + round(level * 246 / 10), covering byte range 9..255
bool PegasusFlatMasterNeo::setDewAggressiveness(uint8_t level)
{
    char cmd[16] = {0};
    char response[16] = {0};

    snprintf(cmd, sizeof(cmd), "DA:%d", level);
    if (!sendCommand(cmd, response) || strstr(response, "DA:") == nullptr)
        return false;

    const uint8_t mapped = static_cast<uint8_t>(9 + static_cast<int>(round(level * 246.0 / 10.0)));
    char cmd2[16] = {0};
    char response2[16] = {0};
    snprintf(cmd2, sizeof(cmd2), "PD:%d", mapped);
    if (!sendCommand(cmd2, response2) || strstr(response2, "PD:") == nullptr)
        return false;

    return true;
}

bool PegasusFlatMasterNeo::saveDewLevels()
{
    char response[16] = {0};
    sendCommand("DSTR", response);
    return true;
}

bool PegasusFlatMasterNeo::getWifiChannel()
{
    char response[16] = {0};
    if (!sendCommand("AC", response))
        return false;

    int channel = atoi(response);
    if (channel < 1 || channel > 11)
        return false;

    WifiChannelNP[0].setValue(channel);
    WifiChannelNP.setState(IPS_OK);
    WifiChannelNP.apply();
    return true;
}

bool PegasusFlatMasterNeo::setWifiChannel(uint8_t channel)
{
    char cmd[16] = {0};
    char response[16] = {0};
    snprintf(cmd, sizeof(cmd), "AC:%d", channel);
    if (sendCommand(cmd, response))
        return atoi(response) != 0;
    return false;
}

bool PegasusFlatMasterNeo::getAutoClose()
{
    char response[16] = {0};
    if (!sendCommand("DC:99", response))
        return false;

    const bool on = (strstr(response, "DC:1") != nullptr);
    AutoCloseSP.reset();
    AutoCloseSP[on ? 0 : 1].setState(ISS_ON);
    AutoCloseSP.setState(IPS_OK);
    AutoCloseSP.apply();
    return true;
}

bool PegasusFlatMasterNeo::setAutoClose(bool enable)
{
    char cmd[16] = {0};
    char response[16] = {0};
    snprintf(cmd, sizeof(cmd), "DC:%d", enable ? 1 : 0);
    if (!sendCommand(cmd, response))
        return false;
    return strstr(response, enable ? "DC:1" : "DC:0") != nullptr;
}

bool PegasusFlatMasterNeo::getHotspotStatus()
{
    char response[16] = {0};
    if (!sendCommand("A?", response))
        return false;

    const bool on = (response[0] == '1');
    HotspotStatusSP.reset();
    HotspotStatusSP[on ? 0 : 1].setState(ISS_ON);
    HotspotStatusSP.setState(IPS_OK);
    HotspotStatusSP.apply();
    return true;
}

bool PegasusFlatMasterNeo::setHotspotStatus(bool enable)
{
    char cmd[16] = {0};
    char response[16] = {0};
    snprintf(cmd, sizeof(cmd), "AE:%d", enable ? 1 : 0);
    sendCommand(cmd, response);
    return getHotspotStatus();
}

bool PegasusFlatMasterNeo::getHotspot()
{
    char response[NEO_LEN] = {0};
    if (!sendCommand("AL", response))
        return false;

    // Response: <ssid>:<password>
    std::vector<std::string> parts = split(response, ":");
    if (parts.size() < 2)
        return false;

    HotspotCredentialsTP[0].setText(parts[0].c_str());
    HotspotCredentialsTP[1].setText(parts[1].c_str());
    HotspotCredentialsTP.setState(IPS_OK);
    HotspotCredentialsTP.apply();
    return true;
}

bool PegasusFlatMasterNeo::setHotspot(const char *ssid, const char *password)
{
    char cmd[NEO_LEN] = {0};
    char response[16] = {0};

    snprintf(cmd, sizeof(cmd), "AN:%s", ssid);
    if (!sendCommand(cmd, response) || strstr(response, "AN:") == nullptr)
        return false;

    snprintf(cmd, sizeof(cmd), "AP:%s", password);
    if (!sendCommand(cmd, response) || strstr(response, "AP:") == nullptr)
        return false;

    return true;
}

bool PegasusFlatMasterNeo::getCurrentWifi()
{
    char response[NEO_LEN] = {0};
    if (!sendCommand("WI", response))
        return false;

    if (strcmp(response, "0") == 0)
    {
        CurrentWifiTP[0].setText("Disconnected");
    }
    else
    {
        CurrentWifiTP[0].setText(response);
    }
    CurrentWifiTP.setState(IPS_OK);
    CurrentWifiTP.apply();
    return true;
}

bool PegasusFlatMasterNeo::scanWifiNetworks()
{
    char response[512] = {0};
    // WS has up to 15 second scan timeout
    if (!sendCommand("WS", response, 16))
        return false;

    if (strcmp(response, "0") == 0)
        WifiScanTP[0].setText("No networks found");
    else
        WifiScanTP[0].setText(response);

    WifiScanTP.setState(IPS_OK);
    WifiScanTP.apply();
    return true;
}

bool PegasusFlatMasterNeo::connectToWifi(const char *ssid, const char *password)
{
    if (!ssid || ssid[0] == '\0')
    {
        LOG_ERROR("WiFi SSID cannot be empty.");
        return false;
    }

    char cmd[NEO_LEN]  = {0};
    char response[16]  = {0};

    snprintf(cmd, sizeof(cmd), "WN:%s", ssid);
    if (!sendCommand(cmd, response) || strstr(response, "WN:") == nullptr)
        return false;

    // Empty password: send "PASSWORD" placeholder; otherwise min 8 chars
    if (!password || password[0] == '\0')
    {
        if (!sendCommand("WP:PASSWORD", response) || strstr(response, "WP:") == nullptr)
            return false;
    }
    else
    {
        if (strlen(password) < 8)
        {
            LOG_ERROR("WiFi password must be at least 8 characters.");
            return false;
        }
        snprintf(cmd, sizeof(cmd), "WP:%s", password);
        if (!sendCommand(cmd, response) || strstr(response, "WP:") == nullptr)
            return false;
    }

    return true;
}

bool PegasusFlatMasterNeo::wifiFactoryReset()
{
    char response[16] = {0};
    // WZ sends without waiting for a response (Channel.Write behavior)
    tty_write(PortFD, "WZ\n", 3, nullptr);
    (void)response;
    return true;
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