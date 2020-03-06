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

    serialConnection = new Connection::Serial(this);
    serialConnection->registerHandshake([&]()
    {
        return Handshake();
    });
    serialConnection->setDefaultBaudRate(Connection::Serial::B_19200);
    registerConnection(serialConnection);


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
        defineText(&InfoTP);

    }
    else
    {
        deleteProperty(InfoTP.name);
    }

    return true;
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
bool DeltaT::Handshake()
{
    std::string version;

    char cmd[DRIVER_LEN] = {0}, res[DRIVER_LEN] = {0};

    cmd[0] = DRIVER_SOM;
    cmd[1] = 0x03;
    cmd[2] = DEVICE_PC;
    cmd[3] = DEVICE_DELTA;
    cmd[4] = CMD_GET_VERSION;
    cmd[5] = calculateCheckSum(cmd, 6);

    if (!sendCommand(cmd, res, 6, 10))
        return false;

    version = std::to_string(res[5]) + "." + std::to_string(res[6]) + "(" + std::to_string(res[7]) + std::to_string(res[8]) + ")";

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
        // Reset/Force
        if (!strcmp(ForceSP.name, name))
        {

        }
        // Heater Control
        else
        {

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

    if (chk != res[res_len - 1])
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
/// Read Version
/////////////////////////////////////////////////////////////////////////////
bool DeltaT::readVersion()
{
    return false;
}

/////////////////////////////////////////////////////////////////////////////
/// Read Report
/////////////////////////////////////////////////////////////////////////////
bool DeltaT::readReport()
{
    return false;
}

/////////////////////////////////////////////////////////////////////////////
/// Initialize Heaters
/////////////////////////////////////////////////////////////////////////////
bool DeltaT::initializeHeaters()
{
    return false;
}

/////////////////////////////////////////////////////////////////////////////
/// Set PWM
/////////////////////////////////////////////////////////////////////////////
bool DeltaT::setPWMEnabled(bool enabled, double period, double duty)
{
    INDI_UNUSED(enabled);
    INDI_UNUSED(period);
    INDI_UNUSED(duty);
    return false;
}

/////////////////////////////////////////////////////////////////////////////
/// Force Reboot
/////////////////////////////////////////////////////////////////////////////
bool DeltaT::forceReboot()
{
    return false;
}

/////////////////////////////////////////////////////////////////////////////
/// force Reset
/////////////////////////////////////////////////////////////////////////////
bool DeltaT::forceReset()
{
    return false;
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
