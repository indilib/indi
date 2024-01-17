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

PegasusSPB::PegasusSPB() : WI(this)
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
    setDriverInterface(AUX_INTERFACE | WEATHER_INTERFACE);

    WI::initProperties(ENVIRONMENT_TAB, ENVIRONMENT_TAB);

    addAuxControls();


    serialConnection = new Connection::Serial(this);
    serialConnection->registerHandshake([&]()
    {
        return Handshake();
    });
    registerConnection(serialConnection);

    ////////////////////////////////////////////////////////////////////////////
    /// Adjustable HUB Group
    ////////////////////////////////////////////////////////////////////////////

    //Power-Dew Switch A
    PowerDewSwitchASP[DEW].fill("DEW", "DEW", ISS_OFF);
    PowerDewSwitchASP[POWER].fill("POWER", "POWER(12v)", ISS_OFF);
    PowerDewSwitchASP.fill(getDeviceName(), "POWER-DEWA", "Port A Mode", MAIN_CONTROL_TAB, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);

    //Power-Dew Switch B
    PowerDewSwitchBSP[DEW].fill("DEW", "DEW", ISS_OFF);
    PowerDewSwitchBSP[POWER].fill("POWER", "POWER(12v)", ISS_OFF);
    PowerDewSwitchBSP.fill(getDeviceName(), "POWER-DEWB", "Port B Mode", MAIN_CONTROL_TAB, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);


    ////////////////////////////////////////////////////////////////////////////
    /// Dew Group
    ////////////////////////////////////////////////////////////////////////////

    //DewAdjA
    DewAdjANP[0].fill("DEW_A", "Dew A (%)", "%.2f", 0, 100, 10, 0);
    DewAdjANP.fill(getDeviceName(), "DEW-ADJA", "DEW A", DEW_TAB, IP_RW, 60, IPS_IDLE);

    //DewAdjB
    DewAdjBNP[0].fill("DEW_B", "Dew B (%)", "%.2f", 0, 100, 10, 0);
    DewAdjBNP.fill(getDeviceName(), "DEW-ADJB", "DEW B", DEW_TAB, IP_RW, 60, IPS_IDLE);

    //DewAuto
    DewAutoSP[INDI_ENABLED].fill("DEWAUTO_ON", "Enabled", ISS_OFF);
    DewAutoSP[INDI_DISABLED].fill("DEWAUTO_OFF", "Disabled", ISS_OFF);
    DewAutoSP.fill(getDeviceName(), "DEWAUTO", "Auto Dew", DEW_TAB, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);

    //DewAggress
    DewAggressNP[0].fill("DEW_AGGRESS", "Agg Level", "%.2f", 0, 100, 1, 0);
    DewAggressNP.fill(getDeviceName(), "DEW-AGGESS", "Auto Dew", DEW_TAB, IP_RW, 60, IPS_IDLE);


    ////////////////////////////////////////////////////////////////////////////
    /// Power Group
    ////////////////////////////////////////////////////////////////////////////

    // Quad 12v Power
    QuadPowerSP[INDI_ENABLED].fill("QUAD_ON", "Enabled", ISS_OFF);
    QuadPowerSP[INDI_DISABLED].fill("QUAD_OFF", "Disabled", ISS_OFF);
    QuadPowerSP.fill(getDeviceName(), "QUAD_HUB", "Power Quad Hub", POWER_TAB, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);

    //PowerAdjA
    PowerAdjASP[INDI_ENABLED].fill("POWERA_ON", "Enabled", ISS_OFF);
    PowerAdjASP[INDI_DISABLED].fill("POWERA_OFF", "Disabled", ISS_ON);
    PowerAdjASP.fill(getDeviceName(), "POWER-ADJA", "Power A", POWER_TAB, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);


    //PowerAdjB
    PowerAdjBSP[INDI_ENABLED].fill("POWERB_ON", "Enabled", ISS_OFF);
    PowerAdjBSP[INDI_DISABLED].fill("POWERB_OFF", "Disabled", ISS_ON);
    PowerAdjBSP.fill(getDeviceName(), "POWER-ADJB", "Power B", POWER_TAB, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);


    // Power Sensors
    PowerSensorsNP[SENSOR_VOLTAGE].fill("SENSOR_VOLTAGE", "Voltage (V)", "%4.2f", 0, 99, 100, 0);
    PowerSensorsNP[SENSOR_CURRENT].fill("SENSOR_CURRENT", "Current (A)", "%4.2f", 0, 99, 100, 0);
    PowerSensorsNP[SENSOR_AVG_AMPS].fill("SENSOR_AVG_AMPS", "Average Current (A)", "%4.2f", 0, 99, 100, 0);
    PowerSensorsNP[SENSOR_AMP_HOURS].fill("SENSOR_AMP_HOURS", "Amp hours (Ah)", "%4.2f", 0, 99, 100, 0);
    PowerSensorsNP[SENSOR_WATT_HOURS].fill("SENSOR_WATT_HOURS", "Watt hours (Wh)", "%4.2f", 0, 99, 100, 0);
    PowerSensorsNP[SENSOR_TOTAL_CURRENT].fill("SENSOR_TOTAL_CURRENT", "Total current (A)", "%4.2f", 0, 99, 100, 0);
    PowerSensorsNP[SENSOR_12V_CURRENT].fill("SENSOR_12V_CURRENT", "12V current (A)", "%4.2f", 0, 99, 100, 0);
    PowerSensorsNP[SENSOR_DEWA_CURRENT].fill("SENSOR_DEWA_CURRENT", "DewA current (A)", "%4.2f", 0, 99, 100, 0);
    PowerSensorsNP[SENSOR_DEWB_CURRENT].fill("SENSOR_DEWB_CURRENT", "DewB current (A)", "%4.2f", 0, 99, 100, 0);
    PowerSensorsNP.fill(getDeviceName(), "POWER_SENSORS", "Sensors", MAIN_CONTROL_TAB, IP_RO, 60, IPS_IDLE);

    ////////////////////////////////////////////////////////////////////////////
    /// Environment Group
    ////////////////////////////////////////////////////////////////////////////
    addParameter("WEATHER_TEMPERATURE", "Temperature (C)", -15, 35, 15);
    addParameter("WEATHER_HUMIDITY", "Humidity %", 0, 100, 15);
    addParameter("WEATHER_DEWPOINT", "Dew Point (C)", 0, 100, 15);
    setCriticalParameter("WEATHER_TEMPERATURE");


    ////////////////////////////////////////////////////////////////////////////////////
    /// Sensor Offset
    ////////////////////////////////////////////////////////////////////////////////////
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
        defineProperty(DewAutoSP);
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
        updatePropertiesPowerDewMode(portNumber, mode);

        defineProperty(PowerDewSwitchBSP);
        portNumber = 2;
        mode = getPowerDewPortMode(portNumber);
        PowerDewSwitchBSP.reset();
        PowerDewSwitchBSP[DEW].setState(mode == PegasusSPB::PORT_MODE::DEW ? ISS_ON : ISS_OFF);
        PowerDewSwitchBSP[POWER].setState(mode == PegasusSPB::PORT_MODE::POWER ? ISS_ON : ISS_OFF);
        PowerDewSwitchBSP.setState(IPS_OK);
        PowerDewSwitchBSP.apply();
        updatePropertiesPowerDewMode(portNumber, mode);

        // Main Control
        defineProperty(QuadPowerSP);
        defineProperty(PowerSensorsNP);


        //Sensor Offsets
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
        setupComplete = true;
    }
    else
    {
        // Main Control
        deleteProperty(DewAutoSP);
        deleteProperty(DewAggressNP);
        deleteProperty(QuadPowerSP);
        deleteProperty(PowerSensorsNP);
        deleteProperty(PowerDewSwitchASP);
        deleteProperty(PowerDewSwitchBSP);
        deleteProperty(PowerAdjASP);
        deleteProperty(PowerAdjBSP);
        deleteProperty(DewAdjANP);
        deleteProperty(DewAdjBNP);
        deleteProperty(HumidityOffsetNP);
        deleteProperty(TemperatureOffsetNP);
        WI::updateProperties();
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
        // Quad 12V Power
        if (QuadPowerSP.isNameMatch(name))
        {
            QuadPowerSP.update(states, names, n);
            IPState result = IPS_OK;
            if (isConnected())
            {
                auto newState = QuadPowerSP[INDI_ENABLED].getState() == ISS_ON;
                result = setQuadPowerState(newState) ? IPS_OK : IPS_ALERT;
            }
            QuadPowerSP.setState(result);
            QuadPowerSP.apply();
            return true;
        }
        //Power-Dew SwitchA
        else if(PowerDewSwitchASP.isNameMatch(name))
        {
            PowerDewSwitchASP.update(states, names, n);
            IPState result = IPS_OK;
            if (isConnected())
            {
                auto newState = PowerDewSwitchASP[DEW].getState() == ISS_ON ? 0 : 1;
                result = setPowerDewPortMode(1, newState) ? IPS_OK : IPS_ALERT;
                updatePropertiesPowerDewMode(1, newState);
            }
            PowerDewSwitchASP.setState(result);
            PowerDewSwitchASP.apply();
            return true;
        }
        //Power-Dew SwitchB
        else if(PowerDewSwitchBSP.isNameMatch(name))
        {
            PowerDewSwitchBSP.update(states, names, n);
            IPState result = IPS_OK;
            if (isConnected())
            {
                auto newState = PowerDewSwitchBSP[DEW].getState() == ISS_ON ? 0 : 1;
                result = setPowerDewPortMode(2, newState) ? IPS_OK : IPS_ALERT;
                updatePropertiesPowerDewMode(2, newState);
            }
            PowerDewSwitchBSP.setState(result);
            PowerDewSwitchBSP.apply();
            return true;
        }
        else if(PowerAdjASP.isNameMatch(name))
        {
            PowerAdjASP.update(states, names, n);
            IPState result = IPS_OK;
            if (isConnected())
            {
                bool state = PowerAdjASP[INDI_ENABLED].getState() == ISS_ON;
                if(!setPowerPortState(1, state ))
                {
                    result = IPS_ALERT;
                }
            }

            PowerAdjASP.setState(result);
            PowerAdjASP.apply();

        }
        else if(PowerAdjBSP.isNameMatch(name))
        {
            PowerAdjBSP.update(states, names, n);
            IPState result = IPS_OK;
            if (isConnected())
            {
                bool state = PowerAdjBSP[INDI_ENABLED].getState() == ISS_ON;
                if(!setPowerPortState(2, state))
                {
                    result = IPS_ALERT;
                }
            }
            PowerAdjBSP.setState(result);
            PowerAdjBSP.apply();
        }
        else if(DewAutoSP.isNameMatch(name))
        {
            DewAutoSP.update(states, names, n);
            IPState result = IPS_OK;
            if(isConnected())
            {
                bool state = DewAutoSP[INDI_ENABLED].getState() == ISS_ON;
                if(!setDewAutoState(state))
                {
                    result = IPS_ALERT;
                }
            }
            DewAutoSP.setState(result);
            DewAutoSP.apply();

        }
    }

    return DefaultDevice::ISNewSwitch(dev, name, states, names, n);
}

bool PegasusSPB::ISNewNumber(const char * dev, const char * name, double values[], char * names[], int n)
{
    if (dev != nullptr && !strcmp(dev, getDeviceName()))
    {
        if(DewAdjANP.isNameMatch(name))
        {
            DewAdjANP.update(values, names, n);
            IPState result = IPS_OK;
            if (isConnected())
            {
                if(!setDewPortPower(1, values[0]))
                {
                    result = IPS_ALERT;
                }
            }
            DewAdjANP.setState(result);
            DewAdjANP.apply();
        }
        else if(DewAdjBNP.isNameMatch(name))
        {
            DewAdjBNP.update(values, names, n);
            IPState result = IPS_OK;
            if (isConnected())
            {
                if(!setDewPortPower(2, values[0]))
                {
                    result = IPS_ALERT;
                }
            }
            DewAdjBNP.setState(result);
            DewAdjBNP.apply();
        }
        else if(DewAggressNP.isNameMatch(name))
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
        }
        else if(HumidityOffsetNP.isNameMatch(name))
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
        }
        else if(TemperatureOffsetNP.isNameMatch(name))
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
        }
    }
    return INDI::DefaultDevice::ISNewNumber(dev, name, values, names, n);
}

bool PegasusSPB::setPowerPortState(int portNumber, bool enabled)
{
    int power = 0;

    if(enabled)
        power = 100;

    return setDewPortPower(portNumber, power);
}

bool PegasusSPB::getPowerPortState(int portNumber)
{
    int power = getDewPortPower(portNumber);

    if(power == 100)
        return true;


    return false;
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

void PegasusSPB::updatePropertiesPowerDewMode(int portNumber, int mode)
{
    if(portNumber == 1)
    {
        switch(mode)
        {
            case PegasusSPB::PORT_MODE::DEW:
                deleteProperty(PowerAdjASP);
                defineProperty(DewAdjANP);
                break;
            case PegasusSPB::PORT_MODE::POWER:
                deleteProperty(DewAdjANP);
                bool enabled = getPowerPortState(1);
                PowerAdjASP.reset();
                PowerAdjASP[INDI_ENABLED].setState(enabled ? ISS_ON : ISS_OFF);
                PowerAdjASP[INDI_DISABLED].setState(enabled ? ISS_OFF : ISS_ON);
                PowerAdjASP.setState(IPS_OK);
                PowerAdjASP.apply();
                defineProperty(PowerAdjASP);
                break;
        }
    }
    else if(portNumber == 2)
    {
        switch(mode)
        {
            case PegasusSPB::PORT_MODE::DEW:
                deleteProperty(PowerAdjBSP);
                defineProperty(DewAdjBNP);
                break;
            case PegasusSPB::PORT_MODE::POWER:
                deleteProperty(DewAdjBNP);
                bool enabled = getPowerPortState(2);
                PowerAdjBSP.reset();
                PowerAdjBSP[INDI_ENABLED].setState(enabled ? ISS_ON : ISS_OFF);
                PowerAdjBSP[INDI_DISABLED].setState(enabled ? ISS_OFF : ISS_ON);
                PowerAdjBSP.setState(IPS_OK);
                PowerAdjBSP.apply();
                defineProperty(PowerAdjBSP);
                break;
        }
    }
}

bool PegasusSPB::setQuadPowerState(bool enabled)
{
    char cmd[PEGASUS_LEN] = {0}, res[PEGASUS_LEN] = {0};
    snprintf(cmd, PEGASUS_LEN, "P1:%d", enabled ? 1 : 0);
    return sendCommand(cmd, res);
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
        PowerSensorsNP[SENSOR_VOLTAGE].setValue(std::stod(result[PA_VOLTAGE]));
        PowerSensorsNP[SENSOR_CURRENT].setValue(std::stod(result[PA_CURRENT]) / 65.0);
        PowerSensorsNP.setState(IPS_OK);
        if (lastSensorData[PA_VOLTAGE] != result[PA_VOLTAGE] || lastSensorData[PA_CURRENT] != result[PA_CURRENT])
            PowerSensorsNP.apply();


        // Environment Sensors
        setParameterValue("WEATHER_TEMPERATURE", std::stod(result[PA_TEMPERATURE]));
        setParameterValue("WEATHER_HUMIDITY", std::stod(result[PA_HUMIDITY]));
        setParameterValue("WEATHER_DEWPOINT", std::stod(result[PA_DEW_POINT]));
        if (lastSensorData[PA_TEMPERATURE] != result[PA_TEMPERATURE] ||
                lastSensorData[PA_HUMIDITY] != result[PA_HUMIDITY] ||
                lastSensorData[PA_DEW_POINT] != result[PA_DEW_POINT])
        {
            if (WI::syncCriticalParameters())
                IDSetLight(&critialParametersLP, nullptr);
            ParametersNP.s = IPS_OK;
            IDSetNumber(&ParametersNP, nullptr);
        }

        // Power Quad Status
        QuadPowerSP[INDI_ENABLED].setState((std::stoi(result[PA_PORT_STATUS]) == 1) ? ISS_ON : ISS_OFF);
        QuadPowerSP[INDI_DISABLED].setState((std::stoi(result[PA_PORT_STATUS]) == 1) ? ISS_OFF : ISS_ON);
        QuadPowerSP.setState((std::stoi(result[6]) == 1) ? IPS_OK : IPS_IDLE);
        if (lastSensorData[PA_PORT_STATUS] != result[PA_PORT_STATUS])
            QuadPowerSP.apply();

        //        // Power Warn
        //        PowerWarnL[0].s = (std::stoi(result[PA_PWR_WARN]) == 1) ? IPS_ALERT : IPS_OK;
        //        PowerWarnLP.s = (std::stoi(result[PA_PWR_WARN]) == 1) ? IPS_ALERT : IPS_OK;
        //        if (lastSensorData[PA_PWR_WARN] != result[PA_PWR_WARN])
        //            IDSetLight(&PowerWarnLP, nullptr);

        // Dew PWM
        double dewA = std::stod(result[PA_DEW_1]) / 255.0 * 100.0;
        double dewB = std::stod(result[PA_DEW_2]) / 255.0 * 100.0;
        DewAdjANP[0].setValue(dewA);
        DewAdjANP.setState(IPS_OK);
        DewAdjBNP[0].setValue(dewB);
        DewAdjBNP.setState(IPS_OK);
        if (lastSensorData[PA_DEW_1] != result[PA_DEW_1] || lastSensorData[PA_DEW_2] != result[PA_DEW_2])
        {
            DewAdjANP.apply();
            DewAdjBNP.apply();
        }


        // Auto Dew
        DewAutoSP[INDI_ENABLED].setState((std::stoi(result[PA_AUTO_DEW]) == 1) ? ISS_ON : ISS_OFF);
        DewAutoSP[INDI_DISABLED].setState((std::stoi(result[PA_AUTO_DEW]) == 1) ? ISS_OFF : ISS_ON);
        DewAutoSP.setState((std::stoi(result[6]) == 1) ? IPS_OK : IPS_IDLE);
        if (lastSensorData[PA_AUTO_DEW] != result[PA_AUTO_DEW])
            DewAutoSP.apply();

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

        // Power Sensors
        PowerSensorsNP[SENSOR_AVG_AMPS].setValue(std::stod(result[PS_AVG_AMPS]));
        PowerSensorsNP[SENSOR_AMP_HOURS].setValue(std::stod(result[PS_AMP_HOURS]));
        PowerSensorsNP[SENSOR_WATT_HOURS].setValue(std::stod(result[PS_WATT_HOURS]));
        PowerSensorsNP.setState(IPS_OK);
        if (lastConsumptionData[PS_AVG_AMPS] != result[PS_AVG_AMPS] || lastConsumptionData[PS_AMP_HOURS] != result[PS_AMP_HOURS]
                || lastConsumptionData[PS_WATT_HOURS] != result[PS_WATT_HOURS])
            PowerSensorsNP.apply();

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
        PowerSensorsNP[SENSOR_TOTAL_CURRENT].setValue(std::stod(result[PC_TOTAL_CURRENT]));
        PowerSensorsNP[SENSOR_12V_CURRENT].setValue(std::stod(result[PC_12V_CURRENT]));
        PowerSensorsNP[SENSOR_DEWA_CURRENT].setValue(std::stod(result[PC_DEWA_CURRENT]));
        PowerSensorsNP[SENSOR_DEWB_CURRENT].setValue(std::stod(result[PC_DEWB_CURRENT]));
        PowerSensorsNP.setState(IPS_OK);
        if (lastMetricsData[PC_TOTAL_CURRENT] != result[PC_TOTAL_CURRENT] ||
                lastMetricsData[PC_12V_CURRENT] != result[PC_12V_CURRENT] ||
                lastMetricsData[PC_DEWA_CURRENT] != result[PC_DEWA_CURRENT] ||
                lastMetricsData[PC_DEWB_CURRENT] != result[PC_DEWB_CURRENT])
            PowerSensorsNP.apply();

        //        std::chrono::milliseconds uptime(std::stol(result[PC_UPTIME]));
        //        using dhours = std::chrono::duration<double, std::ratio<3600>>;
        //        std::stringstream ss;
        //        ss << std::fixed << std::setprecision(3) << dhours(uptime).count();
        //        IUSaveText(&FirmwareT[FIRMWARE_UPTIME], ss.str().c_str());
        //        IDSetText(&FirmwareTP, nullptr);

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
