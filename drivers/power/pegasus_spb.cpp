#include "pegasus_spb.h"
#include "indicom.h"
#include "connectionplugins/connectionserial.h"

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
    setVersion(1, 0);
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

    // Extended Power Sensors
    ExtendedPowerNP[SENSOR_AVG_AMPS].fill("AVG_AMPS", "Avg Amps", "%.2f", 0, 10, 0, 0);
    ExtendedPowerNP[SENSOR_AMP_HOURS].fill("AMP_HOURS", "Amp Hours", "%.2f", 0, 1000, 0, 0);
    ExtendedPowerNP[SENSOR_WATT_HOURS].fill("WATT_HOURS", "Watt Hours", "%.2f", 0, 10000, 0, 0);
    ExtendedPowerNP.fill(getDeviceName(), "EXT_POWER_SENSORS", "Extended Power Sensors", MAIN_CONTROL_TAB, IP_RO, 60, IPS_IDLE);

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
        int aggressiveness = getDewAggressiveness();
        DewAggressNP[0].setValue(static_cast<double>(aggressiveness));
        DewAggressNP.setState(IPS_OK);
        DewAggressNP.apply();

        defineProperty(PowerDewSwitchASP);
        int portNumber = 1;
        int mode = getPowerDewPortMode(portNumber);
        PowerDewSwitchASP.reset();
        PowerDewSwitchASP[DEW].setState(mode == PegasusSPB::PORT_MODE::DEW ? ISS_ON : ISS_OFF);
        PowerDewSwitchASP[POWER].setState(mode == PegasusSPB::PORT_MODE::POWER ? ISS_ON : ISS_OFF);
        PowerDewSwitchASP.setState(IPS_OK);
        PowerDewSwitchASP.apply();

        defineProperty(PowerDewSwitchBSP);
        portNumber = 2;
        mode = getPowerDewPortMode(portNumber);
        PowerDewSwitchBSP.reset();
        PowerDewSwitchBSP[DEW].setState(mode == PegasusSPB::PORT_MODE::DEW ? ISS_ON : ISS_OFF);
        PowerDewSwitchBSP[POWER].setState(mode == PegasusSPB::PORT_MODE::POWER ? ISS_ON : ISS_OFF);
        PowerDewSwitchBSP.setState(IPS_OK);
        PowerDewSwitchBSP.apply();

        defineProperty(ExtendedPowerNP);

        defineProperty(HumidityOffsetNP);
        int humidityOffset = getHumidityOffset();
        HumidityOffsetNP[0].setValue(static_cast<double>(humidityOffset));
        HumidityOffsetNP.setState(IPS_OK);
        HumidityOffsetNP.apply();

        defineProperty(TemperatureOffsetNP);
        int temperatureOffset = getTemperatureOffset();
        TemperatureOffsetNP[0].setValue(static_cast<double>(temperatureOffset));
        TemperatureOffsetNP.setState(IPS_OK);
        TemperatureOffsetNP.apply();

        WI::updateProperties();
        PI::updateProperties();
        setupComplete = true;
    }
    else
    {
        deleteProperty(DewAggressNP);
        deleteProperty(PowerDewSwitchASP);
        deleteProperty(PowerDewSwitchBSP);
        deleteProperty(ExtendedPowerNP);
        deleteProperty(HumidityOffsetNP);
        deleteProperty(TemperatureOffsetNP);
        WI::updateProperties();
        PI::updateProperties();
        setupComplete = false;
    }

    return true;
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

    return (strstr(response, "SPB") != nullptr);
}

void PegasusSPB::TimerHit()
{
    if (!isConnected() || setupComplete == false)
    {
        SetTimer(getCurrentPollingPeriod());
        return;
    }

    getSensorData();
    getConsumptionData();
    getMetricsData();

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
            PowerDewSwitchASP.update(states, names, n);
            auto newState = PowerDewSwitchASP[DEW].getState() == ISS_ON ? 0 : 1;
            auto result = setPowerDewPortMode(1, newState) ? IPS_OK : IPS_ALERT;
            PowerDewSwitchASP.setState(result ? IPS_OK : IPS_ALERT);
            PowerDewSwitchASP.apply();
            saveConfig(PowerDewSwitchASP);

            return true;
        }
        //Power-Dew SwitchB
        if(PowerDewSwitchBSP.isNameMatch(name))
        {
            PowerDewSwitchBSP.update(states, names, n);
            auto newState = PowerDewSwitchBSP[DEW].getState() == ISS_ON ? 0 : 1;
            auto result = setPowerDewPortMode(2, newState) ? IPS_OK : IPS_ALERT;
            PowerDewSwitchBSP.setState(result ? IPS_OK : IPS_ALERT);
            PowerDewSwitchBSP.apply();
            saveConfig(PowerDewSwitchBSP);
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

int PegasusSPB::getDewPortPower(int portNumber)
{
    char cmd[PEGASUS_LEN] = {0}, res[PEGASUS_LEN] = {0};
    snprintf(cmd, PEGASUS_LEN, "PA");
    int power = -1;
    if(sendCommand(cmd, res))
    {
        std::vector<std::string> result = split(res, ":");

        if(portNumber == 1)
            power =  std::stod(result[PA_DEW_1]);
        else if(portNumber == 2)
            power = std::stod(result[PA_DEW_2]);
    }
    else
    {
        LOGF_ERROR("Error on set power-dew port mode [Port=%d]", portNumber);
    }
    return power / 255.0 * 100.0;
}



bool PegasusSPB::setDewPortPower(int portNumber, int power)
{
    char cmd[PEGASUS_LEN] = {0}, res[PEGASUS_LEN] = {0};
    snprintf(cmd, PEGASUS_LEN, "P%d:%d", portNumber + 2, static_cast<uint8_t>(power / 100.0 * 255.0));
    if(sendCommand(cmd, res))
    {
        if(!strstr(cmd, res))
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
        return std::stod(result[1]);
    }
    else
    {
        LOGF_ERROR("Error on set power-dew port mode [Port=%d]", portNumber);
    }
    return -1;
}

bool PegasusSPB::setPowerDewPortMode(int portNumber, int mode)
{
    char cmd[PEGASUS_LEN] = {0}, res[PEGASUS_LEN] = {0};
    snprintf(cmd, PEGASUS_LEN, "D%d:%d", portNumber + 2, mode);
    if(sendCommand(cmd, res))
    {
        if(!strstr(cmd, res))
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
        if(!strstr(cmd, res))
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
    int mappedLevel = map(level, 0, 100, 10, 255);
    char cmd[PEGASUS_LEN] = {0}, res[PEGASUS_LEN] = {0};
    snprintf(cmd, PEGASUS_LEN, "PD:%d", mappedLevel);
    if(sendCommand(cmd, res))
    {
        if(!strstr(cmd, res))
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

int PegasusSPB::getDewAggressiveness()
{
    char cmd[PEGASUS_LEN] = {0}, res[PEGASUS_LEN] = {0};
    snprintf(cmd, PEGASUS_LEN, "DA");
    if(sendCommand(cmd, res))
    {
        std::vector<std::string> result = split(res, ":");
        int value = map(std::stoi(result[1]), 10, 255, 0, 100);
        return value;
    }
    else
    {
        LOGF_ERROR("Error on get dew aggresiveness [Cmd=%s Res=%s]", cmd, res);
    }
    return -1;
}

bool PegasusSPB::setHumdityOffset(int level)
{
    char cmd[PEGASUS_LEN] = {0}, res[PEGASUS_LEN] = {0};
    snprintf(cmd, PEGASUS_LEN, "CH:%d", level);
    if(sendCommand(cmd, res))
    {
        if(!strstr(cmd, res))
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

int PegasusSPB::getHumidityOffset()
{
    char cmd[PEGASUS_LEN] = {0}, res[PEGASUS_LEN] = {0};
    snprintf(cmd, PEGASUS_LEN, "CR");
    if(sendCommand(cmd, res))
    {
        std::vector<std::string> result = split(res, ":");
        int value = std::stoi(result[2]);
        return value;
    }
    else
    {
        LOGF_ERROR("Error on get humidity offset [Cmd=%s Res=%s]", cmd, res);
    }
    return -1;
}

bool PegasusSPB::setTemperatureOffset(int level)
{
    char cmd[PEGASUS_LEN] = {0}, res[PEGASUS_LEN] = {0};
    snprintf(cmd, PEGASUS_LEN, "CT:%d", (level * 100));
    if(sendCommand(cmd, res))
    {
        if(!strstr(cmd, res))
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

int PegasusSPB::getTemperatureOffset()
{
    char cmd[PEGASUS_LEN] = {0}, res[PEGASUS_LEN] = {0};
    snprintf(cmd, PEGASUS_LEN, "CR");
    if(sendCommand(cmd, res))
    {
        std::vector<std::string> result = split(res, ":");
        int value = std::stoi(result[1]);
        return (value / 100);
    }
    else
    {
        LOGF_ERROR("Error on get temperature offset [Cmd=%s Res=%s]", cmd, res);
    }
    return -1;
}

// Removed updatePropertiesPowerDewMode function definition

bool PegasusSPB::setFixedPowerPortState(int portNumber, bool enabled)
{
    char cmd[PEGASUS_LEN] = {0}, res[PEGASUS_LEN] = {0};
    // Assuming fixed power ports are P1, P2, P3, P4.
    // INDI::PowerInterface ports 0-3 map to physical ports 1-4.
    snprintf(cmd, PEGASUS_LEN, "P%d:%d", portNumber + 1, enabled ? 1 : 0);
    if (sendCommand(cmd, res))
    {
        return (!strstr(cmd, res));
    }
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
        PI::PowerSensorsNP[PI::SENSOR_CURRENT].setValue(std::stod(result[PA_CURRENT]) / 65.0);
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
        if (lastSensorData[PA_PORT_STATUS] != result[PA_PORT_STATUS])
            PI::PowerChannelsSP.apply();

        // Dew Channels (switchable ports)
        // Port 0 maps to original Port A (physical port 1).
        // Port 1 maps to original Port B (physical port 2).
        // Update DewChannelDutyCycleNP
        PI::DewChannelDutyCycleNP[0].setValue(std::stod(result[PA_DEW_1]) / 255.0 * 100.0);
        PI::DewChannelDutyCycleNP[1].setValue(std::stod(result[PA_DEW_2]) / 255.0 * 100.0);
        PI::DewChannelDutyCycleNP.setState(IPS_OK);
        if (lastSensorData[PA_DEW_1] != result[PA_DEW_1] || lastSensorData[PA_DEW_2] != result[PA_DEW_2])
            PI::DewChannelDutyCycleNP.apply();

        // Update DewChannelsSP (on/off state) based on duty cycle
        PI::DewChannelsSP[0].setState(PI::DewChannelDutyCycleNP[0].getValue() > 0 ? ISS_ON : ISS_OFF);
        PI::DewChannelsSP[1].setState(PI::DewChannelDutyCycleNP[1].getValue() > 0 ? ISS_ON : ISS_OFF);
        PI::DewChannelsSP.setState(IPS_OK);
        if (lastSensorData[PA_DEW_1] != result[PA_DEW_1] || lastSensorData[PA_DEW_2] != result[PA_DEW_2])
            PI::DewChannelsSP.apply();

        // Auto Dew
        PI::AutoDewSP[0].setState((std::stoi(result[PA_AUTO_DEW]) == 1) ? ISS_ON : ISS_OFF);
        PI::AutoDewSP.setState(IPS_OK);
        if (lastSensorData[PA_AUTO_DEW] != result[PA_AUTO_DEW])
            PI::AutoDewSP.apply();

        // Environment Sensors (remain as is, handled by WeatherInterface)
        setParameterValue("WEATHER_TEMPERATURE", std::stod(result[PA_TEMPERATURE]));
        setParameterValue("WEATHER_HUMIDITY", std::stod(result[PA_HUMIDITY]));
        setParameterValue("WEATHER_DEWPOINT", std::stod(result[PA_DEW_POINT]));
        if (lastSensorData[PA_TEMPERATURE] != result[PA_TEMPERATURE] ||
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

        // These are custom consumption metrics, not directly mapped to INDI::PowerInterface's standard sensors.
        // Keep them as is, or consider if they should be mapped to custom INDI properties if needed.
        // For now, we will keep them as is, as they are not part of the core INDI::PowerInterface.
        ExtendedPowerNP[SENSOR_AVG_AMPS].setValue(std::stod(result[PS_AVG_AMPS]));
        ExtendedPowerNP[SENSOR_AMP_HOURS].setValue(std::stod(result[PS_AMP_HOURS]));
        ExtendedPowerNP[SENSOR_WATT_HOURS].setValue(std::stod(result[PS_WATT_HOURS]));
        ExtendedPowerNP.setState(IPS_OK);
        if (lastConsumptionData[PS_AVG_AMPS] != result[PS_AVG_AMPS] || lastConsumptionData[PS_AMP_HOURS] != result[PS_AMP_HOURS]
                || lastConsumptionData[PS_WATT_HOURS] != result[PS_WATT_HOURS])
            ExtendedPowerNP.apply();

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

        // Update PI::PowerSensorsNP with total current
        PI::PowerSensorsNP[PI::SENSOR_CURRENT].setValue(std::stod(result[PC_TOTAL_CURRENT]));
        PI::PowerSensorsNP.setState(IPS_OK);
        if (lastMetricsData[PC_TOTAL_CURRENT] != result[PC_TOTAL_CURRENT])
            PI::PowerSensorsNP.apply();

        // Update PI::PowerChannelCurrentNP for the quad hub
        if (PI::PowerChannelCurrentNP.size() > 0)
        {
            PI::PowerChannelCurrentNP[0].setValue(std::stod(result[PC_12V_CURRENT]));
            PI::PowerChannelCurrentNP.setState(IPS_OK);
            if (lastMetricsData[PC_12V_CURRENT] != result[PC_12V_CURRENT])
                PI::PowerChannelCurrentNP.apply();
        }

        // Update PI::DewChannelCurrentNP for Dew ports
        if (PI::DewChannelCurrentNP.size() > 0)
        {
            PI::DewChannelCurrentNP[0].setValue(std::stod(result[PC_DEWA_CURRENT]));
            PI::DewChannelCurrentNP[1].setValue(std::stod(result[PC_DEWB_CURRENT]));
            PI::DewChannelCurrentNP.setState(IPS_OK);
            if (lastMetricsData[PC_DEWA_CURRENT] != result[PC_DEWA_CURRENT] ||
                    lastMetricsData[PC_DEWB_CURRENT] != result[PC_DEWB_CURRENT])
                PI::DewChannelCurrentNP.apply();
        }

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

    DewAggressNP.save(fp);
    HumidityOffsetNP.save(fp);
    TemperatureOffsetNP.save(fp);
    PowerDewSwitchASP.save(fp);
    PowerDewSwitchBSP.save(fp);

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
    // INDI::PowerInterface ports 0-1 map to the 2 switchable Dew ports.
    // Port 0 maps to original Port A (physical port 1).
    // Port 1 maps to original Port B (physical port 2).
    int originalPortNumber = port + 1; // Map 0-based INDI port to 1-based original port

    if (getPowerDewPortMode(originalPortNumber) == PegasusSPB::PORT_MODE::DEW)
    {
        return setDewPortPower(originalPortNumber, enabled ? dutyCycle : 0);
    }
    else // If the port is in POWER mode, control it as a fixed power port
    {
        // INDI::PowerInterface dew port 0 (original port 1) maps to fixed power port 3 (P3)
        // INDI::PowerInterface dew port 1 (original port 2) maps to fixed power port 4 (P4)
        // setFixedPowerPortState expects 0-indexed ports for P1-P4, so P3 is port 2, P4 is port 3.
        // originalPortNumber is 1-indexed (1 or 2).
        // So, originalPortNumber + 1 will map 1->2 (P3) and 2->3 (P4).
        return setFixedPowerPortState(originalPortNumber + 1, enabled);
    }
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
