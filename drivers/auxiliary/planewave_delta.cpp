/*
    PlaneWave Delta Protocol

    Copyright (C) 2020 Jasem Mutlaq (mutlaqja@ikarustech.com)

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

#include "planewave_delta.h"
#include "indicom.h"
#include "connectionplugins/connectionserial.h"

#include <cmath>
#include <memory>
#include <cstring>
#include <termios.h>
#include <unistd.h>
#include <regex>
#include <sys/ioctl.h>

static std::unique_ptr<DeltaT> deltat(new DeltaT());

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
DeltaT::DeltaT()
{
    setVersion(1, 1);
}

bool DeltaT::initProperties()
{
    INDI::DefaultDevice::initProperties();

    // Version
    IUFillText(&InfoT[INFO_VERSION], "INFO_VERSION", "Version", "NA");
    IUFillTextVector(&InfoTP, InfoT, 1, getDeviceName(), "INFO", "Info", MAIN_CONTROL_TAB, IP_RO, 60, IPS_IDLE);

    // Reset
    IUFillSwitch(&ForceS[FORCE_RESET], "FORCE_RESET", "Reset", ISS_OFF);
    IUFillSwitch(&ForceS[FORCE_BOOT], "FORCE_BOOT", "Boot", ISS_OFF);
    IUFillSwitchVector(&ForceSP, ForceS, 2, getDeviceName(), "FORCE_CONTROL", "Force", MAIN_CONTROL_TAB, IP_RW, ISR_ATMOST1, 60,
                       IPS_IDLE);

    // Temperature
    IUFillNumber(&TemperatureN[TEMPERATURE_AMBIENT], "TEMPERATURE_AMBIENT", "Ambient (c)", "%.2f", -50, 70., 0., 0.);
    IUFillNumber(&TemperatureN[TEMPERATURE_SECONDARY], "TEMPERATURE_SECONDARY", "Secondary (c)", "%.2f", -50, 70., 0., 0.);
    IUFillNumber(&TemperatureN[TEMPERATURE_BACKPLATE], "TEMPERATURE_BACKPLATE", "Backplate (c)", "%.2f", -50, 70., 0., 0.);
    IUFillNumberVector(&TemperatureNP, TemperatureN, 3, getDeviceName(), "DELTA_TEMPERATURE", "Temperature",
                       MAIN_CONTROL_TAB, IP_RO, 0, IPS_IDLE);

    serialConnection = new Connection::Serial(this);
    serialConnection->registerHandshake([&]()
    {
        return Handshake();
    });
    serialConnection->setDefaultBaudRate(Connection::Serial::B_19200);
    registerConnection(serialConnection);

    setDriverInterface(AUX_INTERFACE);

    setDefaultPollingPeriod(1000);
    addAuxControls();

    return true;
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
bool DeltaT::updateProperties()
{
    INDI::DefaultDevice::updateProperties();

    if (isConnected())
    {
        initializeHeaters();

        defineProperty(&InfoTP);
        defineProperty(&TemperatureNP);
        defineProperty(&ForceSP);

        for (auto &oneSP : HeaterControlSP)
            defineProperty(oneSP.get());

        for (auto &oneNP : HeaterParamNP)
            defineProperty(oneNP.get());

        for (auto &oneNP : HeaterMonitorNP)
            defineProperty(oneNP.get());

    }
    else
    {
        deleteProperty(InfoTP.name);
        deleteProperty(TemperatureNP.name);
        deleteProperty(ForceSP.name);

        for (auto &oneSP : HeaterControlSP)
            deleteProperty(oneSP->name);

        for (auto &oneNP : HeaterParamNP)
            deleteProperty(oneNP->name);

        for (auto &oneNP : HeaterMonitorNP)
            deleteProperty(oneNP->name);
    }

    return true;
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
bool DeltaT::Handshake()
{
    std::string version;

    PortFD = serialConnection->getPortFD();

    char cmd[DRIVER_LEN] = {0}, res[DRIVER_LEN] = {0};

    cmd[0] = DRIVER_SOM;
    cmd[1] = 0x03;
    cmd[2] = DEVICE_PC;
    cmd[3] = DEVICE_DELTA;
    cmd[4] = CMD_GET_VERSION;
    cmd[5] = calculateCheckSum(cmd, 6);

    if (!sendCommand(cmd, res, 6, 10))
        return false;

    uint16_t bld = res[7] << 8 | res[8];

    version = std::to_string(res[5]) + "." +
              std::to_string(res[6]) +
              " (" + std::to_string(bld) + ")";

    IUSaveText(&InfoT[0], version.c_str());

    LOGF_INFO("Detected version %s", version.c_str());

    return true;
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
const char *DeltaT::getDefaultName()
{
    return "PlaneWave DeltaT";
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
const char *DeltaT::getHeaterName(int index)
{
    switch (index)
    {
        case 0:
            return "Primary Backplate Heater";
        case 1:
            return "Secondary Mirror Heater";
        case 2:
            return "Terietary Heater";
        default:
            return "Uknkown heater";
    }
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
bool DeltaT::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        // Reset/Boot
        if (!strcmp(ForceSP.name, name))
        {
            IUUpdateSwitch(&ForceSP, states, names, n);
            bool rc = false;
            if (!strcmp(IUFindOnSwitchName(states, names, n), ForceS[FORCE_RESET].name))
                rc = forceReset();
            else if (!strcmp(IUFindOnSwitchName(states, names, n), ForceS[FORCE_BOOT].name))
                rc = forceBoot();

            IUResetSwitch(&ForceSP);
            ForceSP.s = rc ? IPS_OK : IPS_ALERT;
            IDSetSwitch(&ForceSP, nullptr);
            return true;
        }
        // Heater Control
        else
        {
            for (uint8_t i = 0; i < HeaterControlSP.size(); i++)
            {
                if (!strcmp(HeaterControlSP[i]->name, name))
                {
                    IUUpdateSwitch(HeaterControlSP[i].get(), states, names, n);

                    int index = IUFindOnSwitchIndex(HeaterControlSP[i].get());
                    switch (index)
                    {
                        case HEATER_OFF:
                            setHeaterEnabled(i, false);
                            LOGF_INFO("%s is off.", getHeaterName(i));
                            break;

                        case HEATER_ON:
                            setHeaterEnabled(i, true);
                            LOGF_INFO("%s is on.", getHeaterName(i));
                            break;

                        case HEATER_CONTROL:
                            LOGF_INFO("%s automatic control is enabled. Temperature delta will kept at %.2f C", getHeaterName(i),
                                      HeaterParamNP[i]->np[PARAM_CONTROL].value);
                            break;

                        case HEATER_THRESHOLD:
                            LOGF_INFO("%s threshold control is enabled. When ambient temperature falls below %.2f C, heater would be turned on at %.f%% power.",
                                      getHeaterName(i),
                                      HeaterParamNP[i]->np[PARAM_THRESHOLD].value,
                                      HeaterParamNP[i]->np[PARAM_DUTY].value);
                            break;
                    }

                    HeaterControlSP[i]->s = IPS_OK;
                    IDSetSwitch(HeaterControlSP[i].get(), nullptr);
                    return true;
                }
            }
        }

    }

    return INDI::DefaultDevice::ISNewSwitch(dev, name, states, names, n);
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
bool DeltaT::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        for (uint8_t i = 0; i < HeaterParamNP.size(); i++)
        {
            if (!strcmp(HeaterParamNP[i]->name, name))
            {
                IUUpdateNumber(HeaterParamNP[i].get(), values, names, n);
                HeaterParamNP[i]->s = IPS_OK;
                IDSetNumber(HeaterParamNP[i].get(), nullptr);

                // Update existing on heater
                if (IUFindOnSwitchIndex(HeaterControlSP[i].get()) == HEATER_ON)
                    setHeaterEnabled(i, true);

                saveConfig(true, HeaterParamNP[i]->name);
                return true;
            }
        }
    }

    return INDI::DefaultDevice::ISNewNumber(dev, name, values, names, n);
}


/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
void DeltaT::TimerHit()
{
    if (!isConnected())
        return;

    for (uint8_t i = 0 ; i < HeaterControlSP.size(); i++)
    {
        if (readReport(i))
        {
            //IDSetSwitch(HeaterControlSP[i].get(), nullptr);
            IDSetNumber(HeaterMonitorNP[i].get(), nullptr);
        }
    }

    if (readTemperature())
    {
        // Send temperature is above threshold
        bool aboveThreshold = false;
        for (int i = 0; i < TemperatureNP.nnp; i++)
        {
            if (std::fabs(TemperatureN[i].value - m_LastTemperature[i]) > TEMPERATURE_REPORT_THRESHOLD)
            {
                aboveThreshold = true;
                m_LastTemperature[i] = TemperatureN[i].value;
            }
        }

        if (aboveThreshold)
            IDSetNumber(&TemperatureNP, nullptr);
    }

    for (uint8_t i = 0 ; i < HeaterControlSP.size(); i++)
    {
        const int controlMode = IUFindOnSwitchIndex(HeaterControlSP[i].get());
        switch (controlMode)
        {
            // Manual controls
            case HEATER_OFF:
            case HEATER_ON:
                break;

            case HEATER_CONTROL:
            {
                // Current Surface Temperature
                double surfaceTemperature = TemperatureN[i == 0 ? TEMPERATURE_BACKPLATE : TEMPERATURE_SECONDARY].value;
                // Temperature Temperature
                double targetTemperature = HeaterParamNP[i]->np[PARAM_CONTROL].value + TemperatureN[TEMPERATURE_AMBIENT].value;
                // Get target duty
                double targetDuty = m_Controllers[i]->calculate(targetTemperature, surfaceTemperature);
                // Limit to 0 - 100
                double heaterDuty = std::max(0., std::min(100., targetDuty));
                // Only update if there is a difference
                if (std::abs(heaterDuty - HeaterMonitorNP[i]->np[MONITOR_DUTY].value) > 0.001 ||
                        std::abs(HeaterParamNP[i]->np[PARAM_PERIOD].value - HeaterMonitorNP[i]->np[MONITOR_PERIOD].value) > 0)
                    setHeaterParam(i, HeaterParamNP[i]->np[PARAM_PERIOD].value, heaterDuty);
            }
            break;

            case HEATER_THRESHOLD:
            {
                // If ambient is within threshold, don't do anything
                // so that we don't over-control
                if (std::fabs(TemperatureN[TEMPERATURE_AMBIENT].value - HeaterParamNP[i]->np[PARAM_THRESHOLD].value)
                        < TEMPERATURE_CONTROL_THRESHOLD)
                    break;

                // If heater is off, check if we need to turn it on.
                if (HeaterMonitorNP[i]->s == IPS_IDLE
                        && (TemperatureN[TEMPERATURE_AMBIENT].value < HeaterParamNP[i]->np[PARAM_THRESHOLD].value))
                    setHeaterEnabled(i, true);
                else if (HeaterMonitorNP[i]->s == IPS_BUSY
                         && (TemperatureN[TEMPERATURE_AMBIENT].value > HeaterParamNP[i]->np[PARAM_THRESHOLD].value))
                    setHeaterEnabled(i, false);
                break;
            }
        }
    }

    SetTimer(getCurrentPollingPeriod());
}


/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
bool DeltaT::saveConfigItems(FILE * fp)
{
    INDI::DefaultDevice::saveConfigItems(fp);
    for (uint8_t i = 0; i < HeaterParamNP.size(); i++)
        IUSaveConfigNumber(fp, HeaterMonitorNP[i].get());
    return true;
}

/////////////////////////////////////////////////////////////////////////////
/// Read Report
/////////////////////////////////////////////////////////////////////////////
bool DeltaT::readReport(uint8_t index)
{
    char cmd[DRIVER_LEN] = {0}, res[DRIVER_LEN] = {0};

    cmd[0] = DRIVER_SOM;
    cmd[1] = 0x04;
    cmd[2] = DEVICE_PC;
    cmd[3] = DEVICE_DELTA;
    cmd[4] = COH_REPORT;
    cmd[5] = index;
    cmd[6] = calculateCheckSum(cmd, 7);

    if (!sendCommand(cmd, res, 7, 19))
        return false;

    if (static_cast<uint8_t>(res[5]) != 0x80)
        return false;

    HeaterReport report;

    report.StateUB      = res[6];
    report.ModeUB       = res[7];
    report.SetPointUW   = res[9] << 8 | res[8];
    report.TempHtrIdUB  = res[10];
    report.TempHtrUW    = res[12] << 8 | res[11];
    report.TempAmbUW    = res[14] << 8 | res[13];
    report.PeriodUW     = res[16] << 8 | res[15];
    report.DutyCycleUB  = res[17];

    //bool stateChanged = false, paramChanged = false;

    //    bool wasOn = IUFindOnSwitchIndex(HeaterControlSP[index].get()) == HEATER_ON;

    //    IUResetSwitch(HeaterControlSP[index].get());
    //    HeaterControlSP[index]->sp[HEATER_ON].s = report.StateUB == 1 ? ISS_ON : ISS_OFF;
    //    HeaterControlSP[index]->sp[HEATER_OFF].s = report.StateUB == 1 ? ISS_OFF : ISS_ON;

    //    bool isOn = IUFindOnSwitchIndex(HeaterControlSP[index].get()) == HEATER_ON;

    //    stateChanged = wasOn != isOn;

    double currentPeriod = HeaterMonitorNP[index]->np[MONITOR_PERIOD].value;
    double currentDuty = HeaterMonitorNP[index]->np[MONITOR_DUTY].value;
    IPState currentState = HeaterMonitorNP[index]->s;

    HeaterMonitorNP[index]->np[MONITOR_PERIOD].value = report.PeriodUW / 10.0;
    HeaterMonitorNP[index]->np[MONITOR_DUTY].value = report.DutyCycleUB;
    HeaterMonitorNP[index]->s = (report.StateUB == 1) ? IPS_BUSY : IPS_IDLE;

    bool paramChanged = std::fabs(currentPeriod - HeaterMonitorNP[index]->np[MONITOR_PERIOD].value) > 0.1 ||
                        std::fabs(currentDuty - HeaterMonitorNP[index]->np[MONITOR_DUTY].value) > 0 ||
                        currentState != HeaterMonitorNP[index]->s;

    // Return true if only something changed.
    return (paramChanged);
}

/////////////////////////////////////////////////////////////////////////////
/// Initialize Heaters
/////////////////////////////////////////////////////////////////////////////
bool DeltaT::initializeHeaters()
{
    char cmd[DRIVER_LEN] = {0}, res[DRIVER_LEN] = {0};

    cmd[0] = DRIVER_SOM;
    cmd[1] = 0x03;
    cmd[2] = DEVICE_PC;
    cmd[3] = DEVICE_DELTA;
    cmd[4] = COH_NUMHEATERS;
    cmd[5] = calculateCheckSum(cmd, 6);

    if (!sendCommand(cmd, res, 6, 7))
        return false;

    uint8_t nHeaters = res[5];

    LOGF_INFO("Detected %d heaters", nHeaters);

    // Create heater controls
    for (uint8_t i = 0; i < nHeaters; i++)
    {
        // TODO fine tune the params
        std::unique_ptr<PID> Controller;
        Controller.reset(new PID(1, 100, 0, 200, 0, 0.75));
        m_Controllers.push_back(std::move(Controller));

        std::unique_ptr<ISwitchVectorProperty> ControlSP;
        ControlSP.reset(new ISwitchVectorProperty);
        std::unique_ptr<ISwitch[]> ControlS;
        ControlS.reset(new ISwitch[4]);

        char switchName[MAXINDINAME] = {0}, groupLabel[MAXINDINAME] = {0};
        snprintf(switchName, MAXINDINAME, "HEATER_%d", i + 1);
        snprintf(groupLabel, MAXINDINAME, "%s", getHeaterName(i));
        IUFillSwitch(&ControlS[HEATER_OFF], "HEATER_OFF", "Off", ISS_ON);
        IUFillSwitch(&ControlS[HEATER_ON], "HEATER_ON", "On", ISS_OFF);
        IUFillSwitch(&ControlS[HEATER_CONTROL], "HEATER_CONTROL", "Control", ISS_OFF);
        IUFillSwitch(&ControlS[HEATER_THRESHOLD], "HEATER_THRESHOLD", "Theshold", ISS_OFF);
        IUFillSwitchVector(ControlSP.get(), ControlS.get(), 4, getDeviceName(), switchName, "Heater",
                           groupLabel, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);

        HeaterControlSP.push_back(std::move(ControlSP));
        HeaterControlS.push_back(std::move(ControlS));
    }

    // Create heater parameters
    for (uint8_t i = 0; i < nHeaters; i++)
    {

        // Control Parameters
        std::unique_ptr<INumberVectorProperty> ControlNP;
        ControlNP.reset(new INumberVectorProperty);
        std::unique_ptr<INumber[]> ControlN;
        ControlN.reset(new INumber[4]);

        char numberName[MAXINDINAME] = {0}, groupLabel[MAXINDINAME] = {0};
        snprintf(numberName, MAXINDINAME, "PARAM_%d", i + 1);
        snprintf(groupLabel, MAXINDINAME, "%s", getHeaterName(i));
        IUFillNumber(&ControlN[PARAM_PERIOD], "PARAM_PERIOD", "Period", "%.1f", 0.1, 60, 1, 1);
        IUFillNumber(&ControlN[PARAM_DUTY], "PARAM_DUTY", "Duty", "%.f", 1, 100, 5, 1);
        IUFillNumber(&ControlN[PARAM_CONTROL], "PARAM_CONTROL", "ΔAmbient =", "%.1f", 0, 100, 5, 2.5);
        IUFillNumber(&ControlN[PARAM_THRESHOLD], "PARAM_THRESHOLD", "Ambient less", "%.1f", -50, 50, 5, 2.5);
        IUFillNumberVector(ControlNP.get(), ControlN.get(), 4, getDeviceName(), numberName, "Params",
                           groupLabel, IP_RW, 60, IPS_IDLE);

        HeaterParamNP.push_back(std::move(ControlNP));
        HeaterParamN.push_back(std::move(ControlN));


        // Monitoring
        std::unique_ptr<INumberVectorProperty> MonitorNP;
        MonitorNP.reset(new INumberVectorProperty);
        std::unique_ptr<INumber[]> MonitorN;
        MonitorN.reset(new INumber[2]);

        snprintf(numberName, MAXINDINAME, "MONITOR_%d", i + 1);
        snprintf(groupLabel, MAXINDINAME, "%s", getHeaterName(i));
        IUFillNumber(&MonitorN[MONITOR_PERIOD], "MONITOR_PERIOD", "Period", "%.1f", 0.1, 60, 1, 1);
        IUFillNumber(&MonitorN[MONITOR_DUTY], "MONITOR_DUTY", "Duty", "%.f", 1, 100, 5, 1);
        IUFillNumberVector(MonitorNP.get(), MonitorN.get(), 2, getDeviceName(), numberName, "Monitor",
                           groupLabel, IP_RO, 60, IPS_IDLE);

        HeaterMonitorNP.push_back(std::move(MonitorNP));
        HeaterMonitorN.push_back(std::move(MonitorN));
    }

    return true;
}

/////////////////////////////////////////////////////////////////////////////
/// Enable/Disable heater
/////////////////////////////////////////////////////////////////////////////
bool DeltaT::setHeaterEnabled(uint8_t index, bool enabled)
{
    if (!enabled)
    {
        char cmd[DRIVER_LEN] = {0}, res[DRIVER_LEN] = {0};

        cmd[0] = DRIVER_SOM;
        cmd[1] = 0x04;
        cmd[2] = DEVICE_PC;
        cmd[3] = DEVICE_DELTA;
        cmd[4] = COH_OFF;
        cmd[5] = index;
        cmd[6] = calculateCheckSum(cmd, 7);

        if (!sendCommand(cmd, res, 7, 7))
            return false;

        // 0x80 is OK
        return (static_cast<uint8_t>(res[5]) == 0x80);
    }
    else
    {
        return setHeaterParam(index, HeaterParamNP[index]->np[PARAM_PERIOD].value, HeaterParamNP[index]->np[PARAM_DUTY].value);
    }
}

/////////////////////////////////////////////////////////////////////////////
/// Set Heater Param
/////////////////////////////////////////////////////////////////////////////
bool DeltaT::setHeaterParam(uint8_t index, double period, double duty)
{
    char cmd[DRIVER_LEN] = {0}, res[DRIVER_LEN] = {0};

    uint16_t seconds = period * 10;

    cmd[0] = DRIVER_SOM;
    cmd[1] = 0x07;
    cmd[2] = DEVICE_PC;
    cmd[3] = DEVICE_DELTA;
    cmd[4] = COH_ON_MANUAL;
    cmd[5] = index;
    // LSB
    cmd[6] = seconds & 0xFF;
    // MSB
    cmd[7] = (seconds >> 8) & 0xFF;
    cmd[8] = static_cast<uint8_t>(duty);
    cmd[9] = calculateCheckSum(cmd, 10);

    if (!sendCommand(cmd, res, 10, 7))
        return false;

    // 0x80 is OK
    return (static_cast<uint8_t>(res[5]) == 0x80);
}

/////////////////////////////////////////////////////////////////////////////
/// Force Boot
/////////////////////////////////////////////////////////////////////////////
bool DeltaT::forceBoot()
{
    char cmd[DRIVER_LEN] = {0};
    cmd[0] = DRIVER_SOM;
    cmd[1] = 0x03;
    cmd[2] = DEVICE_PC;
    cmd[3] = DEVICE_DELTA;
    cmd[4] = CMD_FORCE_BOOT;
    cmd[5] = calculateCheckSum(cmd, 6);
    return sendCommand(cmd, nullptr, 6, 0);
}

/////////////////////////////////////////////////////////////////////////////
/// force Reset
/////////////////////////////////////////////////////////////////////////////
bool DeltaT::forceReset()
{
    char cmd[DRIVER_LEN] = {0};
    cmd[0] = DRIVER_SOM;
    cmd[1] = 0x03;
    cmd[2] = DEVICE_PC;
    cmd[3] = DEVICE_DELTA;
    cmd[4] = CMD_FORCE_RESET;
    cmd[5] = calculateCheckSum(cmd, 6);
    return sendCommand(cmd, nullptr, 6, 0);
}

/////////////////////////////////////////////////////////////////////////////
/// Get Temperature
/////////////////////////////////////////////////////////////////////////////
bool DeltaT::readTemperature()
{
    char cmd[DRIVER_LEN] = {0}, res[DRIVER_LEN] = {0};

    for (uint8_t i = 0; i < 3; i++)
    {
        cmd[0] = DRIVER_SOM;
        cmd[1] = 0x04;
        cmd[2] = DEVICE_PC;
        cmd[3] = DEVICE_DELTA;
        cmd[4] = TEMP_GET;
        cmd[5] = i + 1;
        cmd[6] = calculateCheckSum(cmd, 7);

        if (!sendCommand(cmd, res, 7, 8))
            return false;

        double newTemperature = calculateTemperature(res[5], res[6]);
        // Temperature fluctuates a lot so let's change it only when the delta > 0.1
        if (std::abs(TemperatureN[i].value - newTemperature) > TEMPERATURE_CONTROL_THRESHOLD)
            TemperatureN[i].value = newTemperature;

    }

    return true;
}

/////////////////////////////////////////////////////////////////////////////
/// Calculate temperature from bytes
/////////////////////////////////////////////////////////////////////////////
double DeltaT::calculateTemperature(uint8_t lsb, uint8_t msb)
{
    if (lsb == 0x7F && msb == 0x7F)
        return -100;

    int raw_temperature = lsb << 8 | msb;
    if (raw_temperature & 0x8000)
        raw_temperature = raw_temperature - 0x10000;

    return raw_temperature / 16.0;
}

/////////////////////////////////////////////////////////////////////////////
/// Send Command
/////////////////////////////////////////////////////////////////////////////
bool DeltaT::sendCommand(const char * cmd, char * res, uint32_t cmd_len, uint32_t res_len)
{
    int nbytes_written = 0, nbytes_read = 0, rc = 0;

    for (int j = 0; j < 3; j++)
    {
        char hex_cmd[DRIVER_LEN * 3] = {0};
        hexDump(hex_cmd, cmd, cmd_len);
        LOGF_DEBUG("CMD <%s>", hex_cmd);
        rc = tty_write(PortFD, cmd, cmd_len, &nbytes_written);

        if (rc != TTY_OK)
        {
            char errstr[MAXRBUF] = {0};
            tty_error_msg(rc, errstr, MAXRBUF);
            LOGF_ERROR("Serial write error: %s.", errstr);
            return false;
        }

        if (res == nullptr || res_len == 0)
            return true;

        // Next read the response from DeltaT
        rc = tty_read(PortFD, res, res_len, DRIVER_TIMEOUT, &nbytes_read);

        if (rc == TTY_OK)
            break;

        usleep(100000);
    }

    if (rc != TTY_OK)
    {
        char errstr[MAXRBUF] = {0};
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("Serial read error: %s.", errstr);
        return false;
    }

    uint8_t chk = calculateCheckSum(res, res_len);

    if (res_len > 0 && chk != res[res_len - 1])
    {
        LOG_ERROR("Invalid checksum!");
        return false;
    }

    char hex_res[DRIVER_LEN * 3] = {0};
    hexDump(hex_res, res, res_len);
    LOGF_DEBUG("RES <%s>", hex_res);

    return true;
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
void DeltaT::hexDump(char * buf, const char * data, uint32_t size)
{
    for (uint32_t i = 0; i < size; i++)
        sprintf(buf + 3 * i, "%02X ", static_cast<uint8_t>(data[i]));

    if (size > 0)
        buf[3 * size - 1] = '\0';
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
std::vector<std::string> DeltaT::split(const std::string &input, const std::string &regex)
{
    // passing -1 as the submatch index parameter performs splitting
    std::regex re(regex);
    std::sregex_token_iterator
    first{input.begin(), input.end(), re, -1},
          last;
    return {first, last};
}

/////////////////////////////////////////////////////////////////////////////
/// From stack overflow #16605967
/////////////////////////////////////////////////////////////////////////////
template <typename T>
std::string DeltaT::to_string(const T a_value, const int n)
{
    std::ostringstream out;
    out.precision(n);
    out << std::fixed << a_value;
    return out.str();
}

/////////////////////////////////////////////////////////////////////////////
/// Calculate Checksum
/////////////////////////////////////////////////////////////////////////////
uint8_t DeltaT::calculateCheckSum(const char *cmd, uint32_t len)
{
    int32_t sum = 0;
    for (uint32_t i = 1; i < len - 1; i++)
        sum += cmd[i];
    return (-sum & 0xFF);
}
