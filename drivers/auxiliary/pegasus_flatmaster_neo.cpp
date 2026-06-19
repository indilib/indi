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
    DeviceStatusNP[STATUS_VALUE_6].fill(        "STATUS_VALUE_6",          "Value 6",               "%.0f", 0, 9999, 1, 0);
    DeviceStatusNP[STATUS_AUTO_DEW].fill(        "STATUS_AUTO_DEW",         "Auto Dew Active",       "%.0f", 0, 1,    1, 0);
    DeviceStatusNP[STATUS_VALUE_8].fill(        "STATUS_VALUE_8",          "Value 8",               "%.0f", 0, 9999, 1, 0);
    DeviceStatusNP[STATUS_VALUE_9].fill(        "STATUS_VALUE_9",          "Value 9",               "%.0f", 0, 9999, 1, 0);
    DeviceStatusNP.fill(getDeviceName(), "DEVICE_STATUS", "Device Status", STATUS_TAB, IP_RO, 60, IPS_IDLE);

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
        defineProperty(CapAngleNP);
        defineProperty(DeviceStatusNP);
        getWeatherOffsets();
    }
    else
    {
        deleteProperty(FirmwareTP.name);
        deleteProperty(WeatherOffsetNP);
        deleteProperty(DewHeaterNP);
        deleteProperty(AutoDewSP);
        deleteProperty(CapAngleNP);
        deleteProperty(DeviceStatusNP);
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
    }

    return INDI::DefaultDevice::ISNewNumber(dev, name, values, names, n);
}

bool PegasusFlatMasterNeo::ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        if (LI::processText(dev, name, texts, names, n))
            return true;
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
    CapAngleNP.save(fp);
    return true;
}

bool PegasusFlatMasterNeo::sendCommand(const char *command, char *res)
{
    int nbytes_written = 0, nbytes_read = 0, rc = -1;
    char errstr[MAXRBUF];

    char cmd[16] = {0};
    snprintf(cmd, 16, "%s\n", command);

    LOGF_DEBUG("CMD <%#02X>", cmd[0]);

    tcflush(PortFD, TCIOFLUSH);

    if ((rc = tty_write(PortFD, cmd, strlen(cmd), &nbytes_written)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("command: %s error: %s.", cmd, errstr);
        return false;
    }

    if ((rc = tty_read_section(PortFD, res, 0xA, 3, &nbytes_read)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("command: %s error: %s.", cmd, errstr);
        return  false;
    }

    // Get rid of 0xA
    res[nbytes_read - 1] = 0;

    if(res[nbytes_read - 2] == '\r')
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
    if (sendCommand("FS:0", response) && strstr(response, "FS:") != nullptr)
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

    // FA response: "FMNEO:lightIntensity:lightActive:capActual:capTarget:capStatus:v6:autoDew:v8:v9"
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

        // --- Overview tab ---
        DeviceStatusNP[STATUS_LIGHT_INTENSITY].setValue(lightIntensity);
        DeviceStatusNP[STATUS_LIGHT_ACTIVE].setValue(lightActive);
        DeviceStatusNP[STATUS_CAP_TARGET_ANGLE].setValue(capTargetAngle);
        DeviceStatusNP[STATUS_CAP_ACTUAL_ANGLE].setValue(capActualAngle);
        DeviceStatusNP[STATUS_CAP_STATUS].setValue(capStatus);
        DeviceStatusNP[STATUS_VALUE_6].setValue(std::stod(result[FA_VALUE_6]));
        DeviceStatusNP[STATUS_AUTO_DEW].setValue(std::stod(result[FA_AUTO_DEW_STATUS]));
        DeviceStatusNP[STATUS_VALUE_8].setValue(std::stod(result[FA_VALUE_8]));
        DeviceStatusNP[STATUS_VALUE_9].setValue(std::stod(result[FA_VALUE_9]));
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