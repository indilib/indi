#include "pegasus_spb.h"
#include "indicom.h"
#include "connectionplugins/connectionserial.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <regex>
#include <termios.h>
#include <chrono>
#include <iomanip>

static std::unique_ptr<PegasusSPB> spb(new PegasusSPB());



//double result = map(100, 9, 255, 1, 100);
//double reverse = map(result, 1, 100, 9, 255);
//LOGF_DEBUG("%d", reverse);

PegasusSPB::PegasusSPB() : INDI::DefaultDevice(), WI(this), PI(this)
{
    setVersion(1, 3);
    lastSensorData.reserve(PA_N);
    lastConsumptionData.reserve(PS_N);
    lastMetricsData.reserve(PC_N);

}

double PegasusSPB::map(double value, double from1, double to1, double from2, double to2)
{
    // Make sure the value is within the source range [from1, to1]
    value = std::max(from1, std::min(to1, value));

    // Calculate the mapped value in the destination range [from2, to2]
    double mappedValue = from2 + (value - from1) * (to2 - from2) / (to1 - from1);

    return mappedValue;
}

bool PegasusSPB::initProperties()
{
    INDI::DefaultDevice::initProperties();
    setDriverInterface(AUX_INTERFACE | WEATHER_INTERFACE | POWER_INTERFACE);

    WI::initProperties(ENVIRONMENT_TAB, ENVIRONMENT_TAB);

    addAuxControls();

    serialConnection = new Connection::Serial(this);
    serialConnection->registerHandshake([&]()
    {
        return Handshake();
    });
    registerConnection(serialConnection);

    PI::SetCapability(POWER_HAS_DC_OUT | POWER_HAS_DEW_OUT | POWER_HAS_VOLTAGE_SENSOR |
                      POWER_HAS_OVERALL_CURRENT | POWER_HAS_PER_PORT_CURRENT | POWER_HAS_AUTO_DEW);
    PI::initProperties(POWER_TAB, 1, 2, 0, 1,
                       0); // 1 DC port (Quad Hub), 2 DEW ports (switchable), 0 Variable, 1 Auto Dew (global), 0 USB

    // Power-Dew SwitchA
    PowerDewSwitchASP[0].fill("DEW", "Dew", ISS_OFF);
    PowerDewSwitchASP[1].fill("POWER", "Power", ISS_ON);
    PowerDewSwitchASP.fill(getDeviceName(), "DEW_POWER_SWITCH_A", "Port A Mode", DEW_TAB, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);

    // Power-Dew SwitchB
    PowerDewSwitchBSP[0].fill("DEW", "Dew", ISS_OFF);
    PowerDewSwitchBSP[1].fill("POWER", "Power", ISS_ON);
    PowerDewSwitchBSP.fill(getDeviceName(), "DEW_POWER_SWITCH_B", "Port B Mode", DEW_TAB, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);

    //DewAggress
    DewAggressNP[0].fill("DEW_AGGRESS", "Agg Level", "%.2f", 0, 100, 1, 0);
    DewAggressNP.fill(getDeviceName(), "DEW-AGGESS", "Auto Dew", DEW_TAB, IP_RW, 60, IPS_IDLE);

    // overwrite labels to device labelling defaults
    DewChannelsSP.setLabel("Dew Heater");
    for (size_t i = 0; i < DewChannelsSP.size(); i++)
    {
        std::string name (1, static_cast<char>('A' + i));
        DewChannelLabelsTP[i].setLabel("Dew " + name);
        DewChannelLabelsTP[i].setText("Dew " + name);
    }
    if (PowerChannelLabelsTP.size() > 0 && PowerChannelCurrentNP.size() > 0)
    {
        PowerChannelLabelsTP[0].setLabel("Quad Output");
        PowerChannelLabelsTP[0].setText("Enable 12V ports");
        PowerChannelCurrentNP[0].setLabel("12 V current (A)");
    }

    // set the labels to the configured values
    DewChannelLabelsTP.load();
    for (size_t i = 0; i < DewChannelsSP.size(); i++)
    {
        char dewChannelLabel[MAXINDILABEL];
        char dewDutyCycleLabel[MAXINDILABEL];
        char dewChannelCurrentLabel[MAXINDILABEL];
        snprintf(dewChannelLabel, MAXINDILABEL, "%s", DewChannelLabelsTP[i].getText());
        snprintf(dewDutyCycleLabel, MAXINDILABEL, "%s (%%)", DewChannelLabelsTP[i].getText());
        snprintf(dewChannelCurrentLabel, MAXINDILABEL, "%s (A)", DewChannelLabelsTP[i].getText());
        DewChannelsSP[i].setLabel(dewChannelLabel);
        DewChannelDutyCycleNP[i].setLabel(dewDutyCycleLabel);
        DewChannelCurrentNP[i].setLabel(dewChannelCurrentLabel);
    }
    PowerChannelLabelsTP.load();
    if (PowerChannelLabelsTP.size() > 0 && PowerChannelsSP.size() > 0)
    {
        char powerChannelLabel[MAXINDILABEL];
        snprintf(powerChannelLabel, MAXINDILABEL, "%s", PowerChannelLabelsTP[0].getText());
        PowerChannelsSP[0].setLabel(powerChannelLabel);
    }

    AutoDewSP.setLabel("Dew Control");
    AutoDewSP[0].setLabel("Auto Dew Control");
    PowerChannelsSP.setLabel("Quad Output");

    // Power Sensors
    PowerStatisticsNP[STATS_AVG_AMPS].fill("STATS_AVG_AMPS", "Average Current (A)", "%4.2f", 0, 999, 100, 0);
    PowerStatisticsNP[STATS_AMP_HOURS].fill("STATS_AMP_HOURS", "Amp hours (Ah)", "%4.2f", 0, 999, 100, 0);
    PowerStatisticsNP[STATS_WATT_HOURS].fill("STATS_WATT_HOURS", "Watt hours (Wh)", "%4.2f", 0, 999, 100, 0);
    PowerStatisticsNP[STATS_TOTAL_CURRENT].fill("STATS_TOTAL_CURRENT", "Total current (A)", "%4.2f", 0, 999, 100, 0);
    PowerStatisticsNP.fill(getDeviceName(), "POWER_STATISTICS", "Power Statistics", POWER_TAB, IP_RO, 60, IPS_IDLE);

    // Firmware Group
    FirmwareTP[FIRMWARE_VERSION].fill("VERSION", "Version", "NA");
    FirmwareTP[FIRMWARE_UPTIME].fill("UPTIME", "Uptime (h)", "NA");
    FirmwareTP.fill(getDeviceName(), "FIRMWARE_INFO", "Firmware", FIRMWARE_TAB, IP_RO, 60,
                    IPS_IDLE);

    // Environment Group
    addParameter("WEATHER_TEMPERATURE", "Temperature (C)", -15, 35, 15);
    addParameter("WEATHER_HUMIDITY", "Humidity %", 0, 100, 15);
    addParameter("WEATHER_DEWPOINT", "Dew Point (C)", 0, 100, 15);
    setCriticalParameter("WEATHER_TEMPERATURE");

    // Sensor Offset
    HumidityOffsetNP[0].fill("HUM_OFFSET", "Level", "%.0f", -50, 50, 1, 0);
    HumidityOffsetNP.fill(getDeviceName(), "HUM-OFFSET", "Humidity Offset", ENVIRONMENT_TAB, IP_RW, 60, IPS_IDLE);

    TemperatureOffsetNP[0].fill("TEMP_OFFSET", "Level", "%.0f", -40, 40, 1, 0);
    TemperatureOffsetNP.fill(getDeviceName(), "TEMP-OFFSET", "Temperature Offset", ENVIRONMENT_TAB, IP_RW, 60, IPS_IDLE);

    return true;

}

bool PegasusSPB::updateProperties()
{
    INDI::DefaultDevice::updateProperties();

    if (isConnected())
    {
        defineProperty(DewAggressNP);
        int aggressiveness = 0;
        if (getDewAggressiveness(aggressiveness))
        {
            DewAggressNP[0].setValue(aggressiveness);
            DewAggressNP.setState(IPS_OK);
        }
        else
            DewAggressNP.setState(IPS_ALERT);
        DewAggressNP.apply();

        defineProperty(PowerDewSwitchASP);
        updatePortMode(PowerDewSwitchASP, getPowerDewPortMode(1));
        defineProperty(PowerDewSwitchBSP);
        updatePortMode(PowerDewSwitchBSP, getPowerDewPortMode(2));

        defineProperty(HumidityOffsetNP);
        defineProperty(TemperatureOffsetNP);
        int temperatureOffset = 0, humidityOffset = 0;
        const bool offsetsRead = getOffsets(temperatureOffset, humidityOffset);
        HumidityOffsetNP[0].setValue(humidityOffset);
        TemperatureOffsetNP[0].setValue(temperatureOffset);
        HumidityOffsetNP.setState(offsetsRead ? IPS_OK : IPS_ALERT);
        TemperatureOffsetNP.setState(offsetsRead ? IPS_OK : IPS_ALERT);
        HumidityOffsetNP.apply();
        TemperatureOffsetNP.apply();

        WI::updateProperties();
        PI::updateProperties();
        defineProperty(PowerStatisticsNP);

        // Firmware
        defineProperty(FirmwareTP);
        getFirmware();

        setupComplete = true;
    }
    else
    {
        deleteProperty(DewAggressNP);
        deleteProperty(PowerDewSwitchASP);
        deleteProperty(PowerDewSwitchBSP);
        deleteProperty(HumidityOffsetNP);
        deleteProperty(TemperatureOffsetNP);
        WI::updateProperties();
        PI::updateProperties();
        deleteProperty(FirmwareTP);
        deleteProperty(PowerStatisticsNP);
        setupComplete = false;
    }

    return true;
}

void PegasusSPB::updatePortMode(INDI::PropertySwitch &property, int mode)
{
    property.reset();
    property[DEW].setState(mode == DEW ? ISS_ON : ISS_OFF);
    property[POWER].setState(mode == POWER ? ISS_ON : ISS_OFF);
    property.setState(mode < 0 ? IPS_ALERT : IPS_OK);
    property.apply();
}

const char * PegasusSPB::getDefaultName()
{
    return "Pegasus SPB";
}


bool PegasusSPB::Handshake()
{
    int tty_rc = 0, nbytes_written = 0, nbytes_read = 0;
    char command[PEGASUS_LEN] = {0}, response[PEGASUS_LEN] = {0};

    PortFD = serialConnection->getPortFD();

    LOG_DEBUG("CMD <P#>");

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

    tcflush(PortFD, TCIOFLUSH);
    response[nbytes_read - 1] = '\0';
    LOGF_DEBUG("RES <%s>", response);

    setupComplete = false;
    // Send the real states after a reconnection even if nothing changed on the device.
    lastSensorData.clear();
    lastConsumptionData.clear();
    lastMetricsData.clear();

    return (strstr(response, "SPB") != nullptr);
}

void PegasusSPB::TimerHit()
{
    if (!isConnected() || setupComplete == false)
    {
        SetTimer(getCurrentPollingPeriod());
        return;
    }

    // A garbled reply must not end the driver with an uncaught std::stod exception.
    try
    {
        getSensorData();
        getConsumptionData();
        getMetricsData();
    }
    catch (const std::exception &e)
    {
        LOGF_WARN("Failed to parse the device status: %s", e.what());
    }

    SetTimer(getCurrentPollingPeriod());
}

bool PegasusSPB::ISNewSwitch(const char * dev, const char * name, ISState * states, char * names[], int n)
{
    if (dev != nullptr && !strcmp(dev, getDeviceName()))
    {
        // Process power-related switches via PowerInterface
        if (PI::processSwitch(dev, name, states, names, n))
            return true;

        // Power-Dew SwitchA
        if(PowerDewSwitchASP.isNameMatch(name))
        {
            if (isConfigLoading())
                return true;
            PowerDewSwitchASP.update(states, names, n);
            auto newState = PowerDewSwitchASP[DEW].getState() == ISS_ON ? DEW : POWER;
            PowerDewSwitchASP.setState(setPowerDewPortMode(1, newState) ? IPS_OK : IPS_ALERT);
            PowerDewSwitchASP.apply();
            return true;
        }
        //Power-Dew SwitchB
        if(PowerDewSwitchBSP.isNameMatch(name))
        {
            if (isConfigLoading())
                return true;
            PowerDewSwitchBSP.update(states, names, n);
            auto newState = PowerDewSwitchBSP[DEW].getState() == ISS_ON ? DEW : POWER;
            PowerDewSwitchBSP.setState(setPowerDewPortMode(2, newState) ? IPS_OK : IPS_ALERT);
            PowerDewSwitchBSP.apply();
            return true;
        }
    }

    return DefaultDevice::ISNewSwitch(dev, name, states, names, n);
}

bool PegasusSPB::ISNewNumber(const char * dev, const char * name, double values[], char * names[], int n)
{
    if (dev != nullptr && !strcmp(dev, getDeviceName()))
    {
        // Process power-related numbers via PowerInterface
        if (PI::processNumber(dev, name, values, names, n))
            return true;

        // DewAggress (custom property)
        if(DewAggressNP.isNameMatch(name))
        {
            // Stored in the device; configurations of version 1.2 and older still contain it.
            if (isConfigLoading())
                return true;
            DewAggressNP.update(values, names, n);
            IPState result = IPS_OK;
            if (isConnected())
            {
                if(!setDewAggressiveness(static_cast<uint8_t>(values[0])))
                {
                    result = IPS_ALERT;
                }
            }
            DewAggressNP.setState(result);
            DewAggressNP.apply();
            return true;
        }
        // HumidityOffset (custom property)
        if(HumidityOffsetNP.isNameMatch(name))
        {
            // Stored in the device; configurations of version 1.2 and older still contain it.
            if (isConfigLoading())
                return true;
            HumidityOffsetNP.update(values, names, n);
            IPState result = IPS_OK;
            if (isConnected())
            {
                if(!setHumdityOffset(static_cast<int>(values[0])))
                {
                    result = IPS_ALERT;
                }
            }
            HumidityOffsetNP.setState(result);
            HumidityOffsetNP.apply();
            return true;
        }
        // TemperatureOffset (custom property)
        if(TemperatureOffsetNP.isNameMatch(name))
        {
            // Stored in the device; configurations of version 1.2 and older still contain it.
            if (isConfigLoading())
                return true;
            TemperatureOffsetNP.update(values, names, n);
            IPState result = IPS_OK;
            if (isConnected())
            {
                if(!setTemperatureOffset(static_cast<int>(values[0])))
                {
                    result = IPS_ALERT;
                }
            }
            TemperatureOffsetNP.setState(result);
            TemperatureOffsetNP.apply();
            return true;
        }
    }
    return INDI::DefaultDevice::ISNewNumber(dev, name, values, names, n);
}

bool PegasusSPB::ISNewText(const char * dev, const char * name, char * texts[], char * names[], int n)
{
    if (dev != nullptr && !strcmp(dev, getDeviceName()))
    {
        // Process power-related text via PowerInterface
        if (PI::processText(dev, name, texts, names, n))
            return true;
    }
    return INDI::DefaultDevice::ISNewText(dev, name, texts, names, n);
}

bool PegasusSPB::setDewPortPower(int portNumber, double power)
{
    char cmd[PEGASUS_LEN] = {0}, res[PEGASUS_LEN] = {0};
    snprintf(cmd, PEGASUS_LEN, "P%d:%ld", portNumber + 2, std::lround(std::clamp(power, 0.0, 100.0) / 100.0 * 255.0));
    if(sendCommand(cmd, res))
    {
        if(strcmp(cmd, res))
        {
            LOGF_ERROR("Error on set dew port power [Cmd=%s Res=%s]", cmd, res);
            return false;
        }
    }
    else
    {
        LOGF_ERROR("Error on set dew port power [Port=%d Power=%d]", portNumber, power);
        return false;
    }
    return true;
}

int PegasusSPB::getPowerDewPortMode(int portNumber)
{
    char cmd[PEGASUS_LEN] = {0}, res[PEGASUS_LEN] = {0};
    snprintf(cmd, PEGASUS_LEN, "D%d:99", portNumber + 2);
    if(sendCommand(cmd, res))
    {
        std::vector<std::string> result = split(res, ":");
        if (result.size() == 2 && (result[1] == "0" || result[1] == "1"))
            return std::stoi(result[1]);
    }
    LOGF_ERROR("Error on get power-dew port mode [Port=%d Res=%s]", portNumber, res);
    return -1;
}

bool PegasusSPB::setPowerDewPortMode(int portNumber, int mode)
{
    char cmd[PEGASUS_LEN] = {0}, res[PEGASUS_LEN] = {0};
    snprintf(cmd, PEGASUS_LEN, "D%d:%d", portNumber + 2, mode);
    if(sendCommand(cmd, res))
    {
        if(strcmp(cmd, res))
        {
            LOGF_ERROR("Error on set power-dew port mode [Cmd=%s Res=%s]", cmd, res);
            return false;
        }
    }
    else
    {
        LOGF_ERROR("Error on set power-dew port mode [Port=%d Mode=%d]", portNumber, mode);
        return false;
    }
    return true;
}

bool PegasusSPB::setDewAutoState(bool enabled)
{
    char cmd[PEGASUS_LEN] = {0}, res[PEGASUS_LEN] = {0};
    snprintf(cmd, PEGASUS_LEN, "PD:%d", enabled ? 1 : 0);
    if(sendCommand(cmd, res))
    {
        if(strcmp(cmd, res))
        {
            LOGF_ERROR("Error on set dew auto state [Cmd=%s Res=%s]", cmd, res);
            return false;
        }
    }
    else
    {
        LOG_ERROR("Error on set set dew auto state");
        return false;
    }
    return true;
}

bool PegasusSPB::setDewAggressiveness(uint8_t level)
{
    int mappedLevel = std::lround(map(level, 0, 100, 10, 254));
    char cmd[PEGASUS_LEN] = {0}, res[PEGASUS_LEN] = {0};
    snprintf(cmd, PEGASUS_LEN, "PD:%d", mappedLevel);
    if(sendCommand(cmd, res))
    {
        if(strcmp(cmd, res))
        {
            LOGF_ERROR("Error on set dew aggresiveness [Cmd=%s Res=%s]", cmd, res);
            return false;
        }
    }
    else
    {
        LOG_ERROR("Error on set dew aggresiveness");
        return false;
    }
    return true;
}

bool PegasusSPB::getDewAggressiveness(int &level)
{
    char res[PEGASUS_LEN] = {0};
    if (sendCommand("DA", res))
    {
        std::vector<std::string> result = split(res, ":");
        try
        {
            if (result.size() == 2)
            {
                level = std::lround(map(std::stoi(result[1]), 10, 254, 0, 100));
                return true;
            }
        }
        catch (const std::exception &) {}
    }
    LOGF_ERROR("Error on get dew aggressiveness [Res=%s]", res);
    return false;
}

bool PegasusSPB::setHumdityOffset(int level)
{
    char cmd[PEGASUS_LEN] = {0}, res[PEGASUS_LEN] = {0};
    snprintf(cmd, PEGASUS_LEN, "CH:%d", level);
    if(sendCommand(cmd, res))
    {
        if(strcmp(cmd, res))
        {
            LOGF_ERROR("Error on set humidity offset [Cmd=%s Res=%s]", cmd, res);
            return false;
        }
    }
    else
    {
        LOG_ERROR("Error on set humidity offset");
        return false;
    }
    return true;
}

bool PegasusSPB::getOffsets(int &temperature, int &humidity)
{
    char res[PEGASUS_LEN] = {0};
    if (sendCommand("CR", res))
    {
        std::vector<std::string> result = split(res, ":");
        try
        {
            if (result.size() == 3)
            {
                // The device keeps the temperature offset in hundredths of a degree (CT:100 = +1 °C).
                temperature = std::lround(std::stoi(result[1]) / 100.0);
                humidity = std::stoi(result[2]);
                return true;
            }
        }
        catch (const std::exception &) {}
    }
    LOGF_ERROR("Error on get sensor offsets [Res=%s]", res);
    return false;
}

bool PegasusSPB::setTemperatureOffset(int level)
{
    char cmd[PEGASUS_LEN] = {0}, res[PEGASUS_LEN] = {0};
    snprintf(cmd, PEGASUS_LEN, "CT:%d", (level * 100));
    if(sendCommand(cmd, res))
    {
        if(strcmp(cmd, res))
        {
            LOGF_ERROR("Error on set temperature offset [Cmd=%s Res=%s]", cmd, res);
            return false;
        }
    }
    else
    {
        LOG_ERROR("Error on set temperature offset");
        return false;
    }
    return true;
}

bool PegasusSPB::getFirmware()
{
    char res[PEGASUS_LEN] = {0};
    if (sendCommand("PV", res))
    {
        std::vector<std::string> result = split(res, ":");
        if (result.size() > 1)
        {
            LOGF_INFO("Detected firmware %s", result[1].c_str());
            FirmwareTP[FIRMWARE_VERSION].setText(result[1]);
            FirmwareTP.setState(IPS_OK);
            FirmwareTP.apply();
            return true;
        }
        else
        {
            LOGF_WARN("Unexpected firmware response: %s", res);
            FirmwareTP.setState(IPS_ALERT);
            FirmwareTP.apply();
            return false;
        }
    }
    LOGF_WARN("Retreiving firmware failed: %s", res);
    FirmwareTP.setState(IPS_ALERT);
    FirmwareTP.apply();
    return false;
}

// Removed updatePropertiesPowerDewMode function definition

bool PegasusSPB::setFixedPowerPortState(int portNumber, bool enabled)
{
    char cmd[PEGASUS_LEN] = {0}, res[PEGASUS_LEN] = {0};
    // Assuming fixed power ports are P1, P2, P3, P4.
    // INDI::PowerInterface ports 0-3 map to physical ports 1-4.
    snprintf(cmd, PEGASUS_LEN, "P%d:%d", portNumber + 1, enabled ? 1 : 0);
    if (sendCommand(cmd, res) && !strcmp(cmd, res))
        return true;

    LOGF_ERROR("Error on set power port [Cmd=%s Res=%s]", cmd, res);
    return false;
}

bool PegasusSPB::getSensorData()
{
    char res[PEGASUS_LEN] = {0};
    if (sendCommand("PA", res))
    {
        std::vector<std::string> result = split(res, ":");
        if (result.size() < PA_N)
        {
            LOG_WARN("Received wrong number of detailed sensor data. Retrying...");
            return false;
        }

        if (result == lastSensorData)
            return true;

        // Power Sensors
        IPState oldPowerSensorsState = PI::PowerSensorsNP.getState();
        double oldVoltage = PI::PowerSensorsNP[PI::SENSOR_VOLTAGE].getValue();
        double oldCurrent = PI::PowerSensorsNP[PI::SENSOR_CURRENT].getValue();
        double oldPower = PI::PowerSensorsNP[PI::SENSOR_POWER].getValue();

        PI::PowerSensorsNP[PI::SENSOR_VOLTAGE].setValue(std::stod(result[PA_VOLTAGE]));
        // The current sensor can report a negative offset (e.g. -39) after the device booted.
        PI::PowerSensorsNP[PI::SENSOR_CURRENT].setValue(std::max(0.0, std::stod(result[PA_CURRENT]) / 65.0));
        PI::PowerSensorsNP[PI::SENSOR_POWER].setValue(PI::PowerSensorsNP[PI::SENSOR_VOLTAGE].getValue() *
                PI::PowerSensorsNP[PI::SENSOR_CURRENT].getValue());
        PI::PowerSensorsNP.setState(IPS_OK);

        if (oldVoltage != PI::PowerSensorsNP[PI::SENSOR_VOLTAGE].getValue() ||
                oldCurrent != PI::PowerSensorsNP[PI::SENSOR_CURRENT].getValue() ||
                oldPower != PI::PowerSensorsNP[PI::SENSOR_POWER].getValue() ||
                oldPowerSensorsState != PI::PowerSensorsNP.getState())
        {
            PI::PowerSensorsNP.apply();
        }

        // Power Channels (fixed DC outputs)
        // Assuming PI::PowerChannelsSP[0-3] map to the 4 fixed DC outputs.
        // The original driver only has QuadPowerSP for group control.
        // We need to update the state of PI::PowerChannelsSP based on the QuadPowerSP state.
        bool quadPowerEnabled = (std::stoi(result[PA_PORT_STATUS]) == 1);
        PI::PowerChannelsSP[0].setState(quadPowerEnabled ? ISS_ON : ISS_OFF);
        PI::PowerChannelsSP.setState(IPS_OK);
        if (lastSensorData.size() < PA_N || lastSensorData[PA_PORT_STATUS] != result[PA_PORT_STATUS])
            PI::PowerChannelsSP.apply();

        // Dew Channels (switchable ports)
        // Port 0 maps to original Port A (physical port 1).
        // Port 1 maps to original Port B (physical port 2).
        // Update DewChannelDutyCycleNP
        // A channel that is off reports 0; keep the duty cycle set for it then, otherwise switching
        // the channel on would send 0 % (the duty cycle is only stored while the channel is off).
        const double dewA = std::stod(result[PA_DEW_1]), dewB = std::stod(result[PA_DEW_2]);
        if (dewA > 0)
            PI::DewChannelDutyCycleNP[0].setValue(dewA / 255.0 * 100.0);
        if (dewB > 0)
            PI::DewChannelDutyCycleNP[1].setValue(dewB / 255.0 * 100.0);
        PI::DewChannelDutyCycleNP.setState(IPS_OK);
        if (lastSensorData.size() < PA_N || lastSensorData[PA_DEW_1] != result[PA_DEW_1]
                || lastSensorData[PA_DEW_2] != result[PA_DEW_2])
            PI::DewChannelDutyCycleNP.apply();

        // Auto Dew
        bool autodew = std::stoi(result[PA_AUTO_DEW]);
        PI::AutoDewSP[0].setState(autodew ? ISS_ON : ISS_OFF);
        PI::AutoDewSP.setState(IPS_OK);
        if (lastSensorData.size() < PA_N || lastSensorData[PA_AUTO_DEW] != result[PA_AUTO_DEW])
            PI::AutoDewSP.apply();

        // With auto dew the device drives both ports, so show the channels on. Without it, a channel is on
        // when the device reports a duty cycle; otherwise switching it on again would send nothing.
        bool changed = false;
        for (size_t i = 0; i < DewChannelsSP.size() && i < 2; i++)
        {
            const bool on = autodew || std::stod(result[i == 0 ? PA_DEW_1 : PA_DEW_2]) > 0;
            if ((DewChannelsSP[i].getState() == ISS_ON) != on)
            {
                DewChannelsSP[i].setState(on ? ISS_ON : ISS_OFF);
                if (autodew)
                    LOGF_INFO("Auto dew on, enabling channel %s", (i == 0 ? "A" : "B"));
                changed = true;
            }
        }
        if (changed)
            DewChannelsSP.apply();

        // Environment Sensors (remain as is, handled by WeatherInterface)
        setParameterValue("WEATHER_TEMPERATURE", std::stod(result[PA_TEMPERATURE]));
        setParameterValue("WEATHER_HUMIDITY", std::stod(result[PA_HUMIDITY]));
        setParameterValue("WEATHER_DEWPOINT", std::stod(result[PA_DEW_POINT]));
        if (lastSensorData.size() < PA_N ||
                lastSensorData[PA_TEMPERATURE] != result[PA_TEMPERATURE] ||
                lastSensorData[PA_HUMIDITY] != result[PA_HUMIDITY] ||
                lastSensorData[PA_DEW_POINT] != result[PA_DEW_POINT])
        {
            if (WI::syncCriticalParameters())
                critialParametersLP.apply();
            ParametersNP.setState(IPS_OK);
            ParametersNP.apply();
        }

        lastSensorData = result;
        return true;
    }

    return false;
}

bool PegasusSPB::getConsumptionData()
{
    char res[PEGASUS_LEN] = {0};
    if (sendCommand("PS", res))
    {
        std::vector<std::string> result = split(res, ":");
        if (result.size() < PS_N)
        {
            LOG_WARN("Received wrong number of detailed consumption data. Retrying...");
            return false;
        }

        if (result == lastConsumptionData)
            return true;

        // Power Statistics
        PowerStatisticsNP[STATS_AVG_AMPS].setValue(std::stod(result[PS_AVG_AMPS]));
        PowerStatisticsNP[STATS_AMP_HOURS].setValue(std::stod(result[PS_AMP_HOURS]));
        PowerStatisticsNP[STATS_WATT_HOURS].setValue(std::stod(result[PS_WATT_HOURS]));
        PowerStatisticsNP.setState(IPS_OK);
        if (lastConsumptionData.size() < PS_N ||
                lastConsumptionData[PS_AVG_AMPS] != result[PS_AVG_AMPS] ||
                lastConsumptionData[PS_AMP_HOURS] != result[PS_AMP_HOURS] ||
                lastConsumptionData[PS_WATT_HOURS] != result[PS_WATT_HOURS])
            PowerStatisticsNP.apply();

        lastConsumptionData = result;

        return true;
    }

    return false;
}

bool PegasusSPB::getMetricsData()
{
    char res[PEGASUS_LEN] = {0};
    if (sendCommand("PC", res))
    {
        std::vector<std::string> result = split(res, ":");
        if (result.size() < PC_N)
        {
            LOG_WARN("Received wrong number of detailed metrics data. Retrying...");
            return false;
        }

        if (result == lastMetricsData)
            return true;

        // Power Sensors
        PowerStatisticsNP[STATS_TOTAL_CURRENT].setValue(std::max(0.0, std::stod(result[PC_TOTAL_CURRENT])));
        if (lastMetricsData.size() < PC_N || lastMetricsData[PC_TOTAL_CURRENT] != result[PC_TOTAL_CURRENT])
            PowerStatisticsNP.apply();

        // Power Sensors (Per-port current monitoring)
        if (PI::PowerChannelCurrentNP.size() > 0)
        {
            PI::PowerChannelCurrentNP[0].setValue(std::max(0.0, std::stod(result[PC_12V_CURRENT])));
            if (lastMetricsData.size() < PC_N || lastMetricsData[PC_12V_CURRENT] != result[PC_12V_CURRENT])
            {
                PI::PowerChannelCurrentNP.setState(IPS_OK);
                PI::PowerChannelCurrentNP.apply();
            }
        }

        // Update PI::DewChannelCurrentNP for Dew ports
        if (PI::DewChannelCurrentNP.size() > 1)
        {
            PI::DewChannelCurrentNP[0].setValue(std::stod(result[PC_DEWA_CURRENT]));
            PI::DewChannelCurrentNP[1].setValue(std::stod(result[PC_DEWB_CURRENT]));
            PI::DewChannelCurrentNP.setState(IPS_OK);
            if (lastMetricsData.size() < PC_N ||
                    lastMetricsData[PC_DEWA_CURRENT] != result[PC_DEWA_CURRENT] ||
                    lastMetricsData[PC_DEWB_CURRENT] != result[PC_DEWB_CURRENT])
                PI::DewChannelCurrentNP.apply();
        }

        std::chrono::milliseconds uptime(std::stol(result[PC_UPTIME]));
        using dhours = std::chrono::duration<double, std::ratio<3600 >>;
        std::stringstream ss;
        ss << std::fixed << std::setprecision(3) << dhours(uptime).count();
        FirmwareTP[FIRMWARE_UPTIME].setText(ss.str().c_str());
        FirmwareTP.apply();

        lastMetricsData = result;

        return true;
    }

    return false;
}


bool PegasusSPB::sendCommand(const char * cmd, char * res)
{
    int nbytes_read = 0, nbytes_written = 0, tty_rc = 0;
    char command[PEGASUS_LEN] = {0};
    LOGF_DEBUG("CMD <%s>", cmd);

    for (int i = 0; i < 2; i++)
    {
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
        res[nbytes_read - 1] = '\0';
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

std::vector<std::string> PegasusSPB::split(const std::string &input, const std::string &regex)
{
    // passing -1 as the submatch index parameter performs splitting
    std::regex re(regex);
    std::sregex_token_iterator
    first{input.begin(), input.end(), re, -1},
          last;
    return {first, last};
}

bool PegasusSPB::saveConfigItems(FILE *fp)
{
    INDI::DefaultDevice::saveConfigItems(fp);
    PI::saveConfigItems(fp);
    WI::saveConfigItems(fp);

    // Dew aggressiveness, sensor offsets and the port modes are stored in the device itself.

    return true;
}

//////////////////////////////////////////////////////////////////////
/// Power Interface Implementations
//////////////////////////////////////////////////////////////////////
bool PegasusSPB::SetPowerPort(size_t port, bool enabled)
{
    return setFixedPowerPortState(port, enabled);
}

bool PegasusSPB::SetDewPort(size_t port, bool enabled, double dutyCycle)
{
    // INDI dew ports 0 and 1 are the device ports A and B (P3 and P4).
    int originalPortNumber = port + 1;

    // In power mode P3/P4 still take a PWM value (P3:1 reads back as 1); 255 is fully on either way.
    if (getPowerDewPortMode(originalPortNumber) == PegasusSPB::PORT_MODE::POWER)
        dutyCycle = 100;

    return setDewPortPower(originalPortNumber, enabled ? dutyCycle : 0);
}

bool PegasusSPB::SetVariablePort(size_t port, bool enabled, double voltage)
{
    INDI_UNUSED(port);
    INDI_UNUSED(enabled);
    INDI_UNUSED(voltage);
    // Pegasus SPB does not have variable voltage outputs.
    return false;
}

bool PegasusSPB::SetLEDEnabled(bool enabled)
{
    INDI_UNUSED(enabled);
    // Pegasus SPB does not have LED control.
    return false;
}

bool PegasusSPB::SetAutoDewEnabled(size_t port, bool enabled)
{
    INDI_UNUSED(port);
    // Pegasus SPB has global auto dew control.
    return setDewAutoState(enabled);
}

bool PegasusSPB::SetUSBPort(size_t port, bool enabled)
{
    INDI_UNUSED(port);
    INDI_UNUSED(enabled);
    // Pegasus SPB does not have USB port control.
    return false;
}
