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

void ISGetProperties(const char *dev)
{
    deltat->ISGetProperties(dev);
}

void ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    deltat->ISNewSwitch(dev, name, states, names, n);
}

void ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n)
{
    deltat->ISNewText(dev, name, texts, names, n);
}

void ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    deltat->ISNewNumber(dev, name, values, names, n);
}

void ISNewBLOB(const char *dev, const char *name, int sizes[], int blobsizes[], char *blobs[], char *formats[],
               char *names[], int n)
{
    INDI_UNUSED(dev);
    INDI_UNUSED(name);
    INDI_UNUSED(sizes);
    INDI_UNUSED(blobsizes);
    INDI_UNUSED(blobs);
    INDI_UNUSED(formats);
    INDI_UNUSED(names);
    INDI_UNUSED(n);
}

void ISSnoopDevice(XMLEle *root)
{
    deltat->ISSnoopDevice(root);
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
DeltaT::DeltaT()
{
    setVersion(1, 0);
}

bool DeltaT::initProperties()
{
    INDI::DefaultDevice::initProperties();

    // Focuser Information
    IUFillText(&InfoT[INFO_VERSION], "INFO_VERSION", "Version", "NA");
    IUFillTextVector(&InfoTP, InfoT, 1, getDeviceName(), "INFO", "Info", MAIN_CONTROL_TAB, IP_RO, 60, IPS_IDLE);

    IUFillSwitch(&ForceS[FORCE_RESET], "FORCE_RESET", "Reset", ISS_OFF);
    IUFillSwitch(&ForceS[FORCE_BOOT], "FORCE_BOOT", "Boot", ISS_OFF);
    IUFillSwitchVector(&ForceSP, ForceS, 2, getDeviceName(), "FORCE_CONTROL", "Force", MAIN_CONTROL_TAB, IP_RW, ISR_ATMOST1, 60,
                       IPS_IDLE);

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

        defineText(&InfoTP);
        defineSwitch(&ForceSP);

        for (auto &oneSP : HeaterControlSP)
            defineSwitch(oneSP.get());

        for (auto &oneNP : HeaterParamNP)
            defineNumber(oneNP.get());

    }
    else
    {
        deleteProperty(InfoTP.name);
        deleteProperty(ForceSP.name);

        for (auto &oneSP : HeaterControlSP)
            deleteProperty(oneSP->name);

        for (auto &oneNP : HeaterParamNP)
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
                    bool wasOn = IUFindOnSwitchIndex(HeaterControlSP[i].get()) == HEATER_ON;
                    IUUpdateSwitch(HeaterControlSP[i].get(), states, names, n);
                    if (setHeaterEnabled(i, IUFindOnSwitchIndex(HeaterControlSP[i].get()) == HEATER_ON))
                    {
                        HeaterControlSP[i]->s = IPS_OK;
                    }
                    else
                    {
                        IUResetSwitch(HeaterControlSP[i].get());
                        HeaterControlSP[i]->sp[HEATER_ON].s = wasOn ? ISS_ON : ISS_OFF;
                        HeaterControlSP[i]->sp[HEATER_OFF].s = wasOn ? ISS_OFF : ISS_ON;
                        HeaterControlSP[i]->s = IPS_ALERT;
                    }

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
                double prevPeriod = HeaterParamNP[i]->np[PARAM_PERIOD].value;
                double prevDuty   = HeaterParamNP[i]->np[PARAM_DUTY].value;

                IUUpdateNumber(HeaterParamNP[i].get(), values, names, n);
                if (setHeaterParam(i, HeaterParamNP[i]->np[PARAM_PERIOD].value, HeaterParamNP[i]->np[PARAM_DUTY].value))
                {
                    HeaterParamNP[i]->s = IPS_OK;
                }
                else
                {
                    HeaterParamNP[i]->s = IPS_ALERT;
                    HeaterParamNP[i]->np[PARAM_PERIOD].value = prevPeriod;
                    HeaterParamNP[i]->np[PARAM_DUTY].value = prevDuty;
                }

                IDSetNumber(HeaterParamNP[i].get(), nullptr);
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
            IDSetSwitch(HeaterControlSP[i].get(), nullptr);
            IDSetNumber(HeaterParamNP[i].get(), nullptr);
        }
    }

    SetTimer(POLLMS);
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
bool DeltaT::saveConfigItems(FILE *fp)
{
    INDI::DefaultDevice::saveConfigItems(fp);
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

    bool stateChanged = false, paramChanged = false;

    bool wasOn = IUFindOnSwitchIndex(HeaterControlSP[index].get()) == HEATER_ON;

    IUResetSwitch(HeaterControlSP[index].get());
    HeaterControlSP[index]->sp[HEATER_ON].s = report.StateUB == 1 ? ISS_ON : ISS_OFF;
    HeaterControlSP[index]->sp[HEATER_OFF].s = report.StateUB == 1 ? ISS_OFF : ISS_ON;

    bool isOn = IUFindOnSwitchIndex(HeaterControlSP[index].get()) == HEATER_ON;

    stateChanged = wasOn != isOn;

    double currentPeriod = HeaterParamNP[index]->np[PARAM_PERIOD].value;
    double currentDuty = HeaterParamNP[index]->np[PARAM_DUTY].value;

    HeaterParamNP[index]->np[PARAM_PERIOD].value = report.PeriodUW / 10.0;
    HeaterParamNP[index]->np[PARAM_DUTY].value = report.DutyCycleUB;

    paramChanged = std::fabs(currentPeriod - HeaterParamNP[index]->np[PARAM_PERIOD].value) > 0.1 ||
                   std::fabs(currentDuty - HeaterParamNP[index]->np[PARAM_DUTY].value) > 0;

    // Return true if only something changed.
    return (stateChanged || paramChanged);
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
        std::unique_ptr<ISwitchVectorProperty> ControlSP;
        ControlSP.reset(new ISwitchVectorProperty);
        std::unique_ptr<ISwitch[]> ControlS;
        ControlS.reset(new ISwitch[2]);

        char switchName[MAXINDINAME] = {0}, groupLabel[MAXINDINAME] = {0};
        snprintf(switchName, MAXINDINAME, "DEW_%d", i + 1);
        snprintf(groupLabel, MAXINDINAME, "Dew #%d", i + 1);
        IUFillSwitch(&ControlS[HEATER_ON], "HEATER_ON", "On", ISS_OFF);
        IUFillSwitch(&ControlS[HEATER_OFF], "HEATER_OFF", "Off", ISS_ON);
        IUFillSwitchVector(ControlSP.get(), ControlS.get(), 2, getDeviceName(), switchName, "Dew",
                           groupLabel, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);

        HeaterControlSP.push_back(std::move(ControlSP));
        HeaterControlS.push_back(std::move(ControlS));
    }

    // Create heater parameters
    for (uint8_t i = 0; i < nHeaters; i++)
    {
        std::unique_ptr<INumberVectorProperty> ControlNP;
        ControlNP.reset(new INumberVectorProperty);
        std::unique_ptr<INumber[]> ControlN;
        ControlN.reset(new INumber[2]);

        char numberName[MAXINDINAME] = {0}, groupLabel[MAXINDINAME] = {0};
        snprintf(numberName, MAXINDINAME, "PARAM_%d", i + 1);
        snprintf(groupLabel, MAXINDINAME, "Dew #%d", i + 1);
        IUFillNumber(&ControlN[PARAM_PERIOD], "PARAM_PERIOD", "Period", "%.1f", 0.1, 60, 1, 1);
        IUFillNumber(&ControlN[PARAM_DUTY], "PARAM_DUTY", "Duty", "%.f", 1, 100, 5, 1);
        IUFillNumberVector(ControlNP.get(), ControlN.get(), 2, getDeviceName(), numberName, "Params",
                           groupLabel, IP_RW, 60, IPS_IDLE);

        HeaterParamNP.push_back(std::move(ControlNP));
        HeaterParamN.push_back(std::move(ControlN));
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
        cmd[3] = DEVICE_TEMP;
        cmd[4] = TEMP_GET;
        cmd[5] = i + 1;
        cmd[6] = calculateCheckSum(cmd, 7);

        if (!sendCommand(cmd, res, 7, 8))
            return false;

        TemperatureN[i].value = calculateTemperature(res[6], res[5]);
    }

    return true;
}

/////////////////////////////////////////////////////////////////////////////
/// Calculate temperature from bytes
/////////////////////////////////////////////////////////////////////////////
double DeltaT::calculateTemperature(uint8_t byte2, uint8_t byte3)
{
    if (byte2 == 0x7F && byte3 == 0x7F)
        return -100;

    int raw_temperature = byte3 << 8 | byte2;
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
