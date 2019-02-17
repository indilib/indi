/*
    Celestron Focuser for SCT and EDGEHD

    Copyright (C) 2019 Jasem Mutlaq (mutlaqja@ikarustech.com)
    Copyright (C) 2019 Chris Rowland

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

#include "celestron.h"
#include "connectionplugins/connectionserial.h"
#include "indicom.h"

#include <cmath>
#include <cstring>
#include <memory>

#include <termios.h>
#include <unistd.h>

static std::unique_ptr<CelestronSCT> celestronSCT(new CelestronSCT());

void ISGetProperties(const char * dev)
{
    celestronSCT->ISGetProperties(dev);
}

void ISNewSwitch(const char * dev, const char * name, ISState * states, char * names[], int n)
{
    celestronSCT->ISNewSwitch(dev, name, states, names, n);
}

void ISNewText(const char * dev, const char * name, char * texts[], char * names[], int n)
{
    celestronSCT->ISNewText(dev, name, texts, names, n);
}

void ISNewNumber(const char * dev, const char * name, double values[], char * names[], int n)
{
    celestronSCT->ISNewNumber(dev, name, values, names, n);
}

void ISNewBLOB(const char * dev, const char * name, int sizes[], int blobsizes[], char * blobs[], char * formats[],
               char * names[], int n)
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

void ISSnoopDevice(XMLEle * root)
{
    celestronSCT->ISSnoopDevice(root);
}

CelestronSCT::CelestronSCT()
{
    // Can move in Absolute & Relative motions, can AbortFocuser motion, and has variable speed.
    FI::SetCapability(FOCUSER_CAN_ABS_MOVE | FOCUSER_CAN_REL_MOVE | FOCUSER_CAN_ABORT | FOCUSER_HAS_VARIABLE_SPEED |
                      FOCUSER_CAN_SYNC);
}

bool CelestronSCT::initProperties()
{
    INDI::Focuser::initProperties();

    // Focuser temperature
    IUFillNumber(&BacklashN[0], "STEPS", "Steps", "%.f", 0, 99., 1., 0.);
    IUFillNumberVector(&BacklashNP, BacklashN, 1, getDeviceName(), "FOCUS_BACKLASH", "Backlash",
                       MAIN_CONTROL_TAB, IP_RO, 0, IPS_IDLE);

    // Speed range
    FocusSpeedN[0].min   = 1;
    FocusSpeedN[0].max   = 3;
    FocusSpeedN[0].value = 1;

    // From online screenshots, seems maximum value is 60,000 steps

    // Relative Position Range
    FocusRelPosN[0].min   = 0.;
    FocusRelPosN[0].max   = 30000.;
    FocusRelPosN[0].value = 0;
    FocusRelPosN[0].step  = 1000;

    // Absolute Postition Range
    FocusAbsPosN[0].min   = 0.;
    FocusAbsPosN[0].max   = 60000.;
    FocusAbsPosN[0].value = 0;
    FocusAbsPosN[0].step  = 1000;

    // Maximum Position Settings
    FocusMaxPosN[0].max   = 60000;
    FocusMaxPosN[0].min   = 1000;
    FocusMaxPosN[0].value = 60000;

    // Poll every 500ms
    setDefaultPollingPeriod(500);

    // Add debugging support
    addDebugControl();

    // Set default baud rate to 19200
    serialConnection->setDefaultBaudRate(Connection::Serial::B_19200);

    return true;
}

bool CelestronSCT::updateProperties()
{
    INDI::Focuser::updateProperties();

    if (isConnected())
    {
        defineNumber(&BacklashNP);

        if (getStartupParameters())
            LOG_INFO("Celestron SCT focuser paramaters updated, focuser ready for use.");
        else
            LOG_WARN("Failed to retrieve some focuser parameters. Check logs.");
    }
    else
    {
        deleteProperty(BacklashNP.name);
    }

    return true;
}

bool CelestronSCT::Handshake()
{
    if (Ack())
    {
        LOG_INFO("Celestron SCT Focuser is online. Getting focus parameters...");
        return true;
    }

    LOG_INFO("Error retreiving data from Celestron SCT, please ensure Celestron SCT controller is powered and the port is correct.");
    return false;
}

const char * CelestronSCT::getDefaultName()
{
    return "Celestron SCT";
}

bool CelestronSCT::Ack()
{
    // send simple command to focuser and check response to make sure
    // it is online and responding
    return false;
}

bool CelestronSCT::readBacklash()
{
    char cmd[CELESTRON_LEN] = {0}, res[CELESTRON_LEN] = {0};

    // If command is a null-terminated string, use snprintf below
    snprintf(cmd, CELESTRON_LEN, "Celestron_Command_Here");

    // This would send the command and reads the response (until it encouters the delimiter).
    if (sendCommand(cmd, res) == false)
        return false;

    // If the command is NOT null-terminated string, fill each byte like this:
    // cmd[0] = 0xA
    // cmd[0] = 0xB
    // cmd[0] = 0xC
    // cmd[0] = 0xD
    // cmd[0] = 0xE
    // We have 5 bytes
    // This would send the command, and read 8 bytes as response. If you want to read until delimeter, skip the last param.
    if (sendCommand(cmd, res, 5, 8) == false)
        return false;

    // Now use scanf to extract the value in the buffer. For example:
    int backlash = 0;
    if (sscanf(res, "%d", &backlash) != 1)
        return false;

    // Now set it to property
    BacklashN[0].value = backlash;

    BacklashNP.s = IPS_OK;

    return true;
}

bool CelestronSCT::readPosition()
{
    // Same as readBacklash
    //FocusAbsPosN[0].value = position;
    return true;
}

bool CelestronSCT::readSpeed()
{
    // Same as readBacklash
    //FocusSpeedN[0].value = speed;
    return true;
}

bool CelestronSCT::isMoving()
{
    // Ditto
    return false;
}


bool CelestronSCT::sendBacklash(uint32_t steps)
{
    // Ditto
    // If there is no response required then we simply send the following:
    //return sendCommand(cmd);

    return true;
}

bool CelestronSCT::SetFocuserSpeed(int speed)
{
    // Ditto
    return true;
}

bool CelestronSCT::SyncFocuser(uint32_t ticks)
{
    // Can Celestron focuser be synced? Not sure
    //char cmd[ML_RES] = {0};
    //snprintf(cmd, ML_RES, ":SP%04X#", ticks);
    //return sendCommand(cmd);

    return true;
}

bool CelestronSCT::ISNewNumber(const char * dev, const char * name, double values[], char * names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        // Backlash
        if (!strcmp(name, BacklashNP.name))
        {

            if (sendBacklash(values[0]))
            {
                IUUpdateNumber(&BacklashNP, values, names, n);
                BacklashNP.s = IPS_OK;
            }
            else
                BacklashNP.s = IPS_ALERT;

            IDSetNumber(&BacklashNP, nullptr);
            return true;
        }
    }

    return INDI::Focuser::ISNewNumber(dev, name, values, names, n);
}

bool CelestronSCT::getStartupParameters()
{
    bool rc1 = false, rc2 = false, rc3 = false;

    if ( (rc1 = readPosition()))
        IDSetNumber(&FocusAbsPosNP, nullptr);

    if ( (rc2 = readBacklash()))
        IDSetNumber(&BacklashNP, nullptr);

    if ( (rc3 = readSpeed()))
        IDSetNumber(&FocusSpeedNP, nullptr);

    return (rc1 && rc2 && rc3);
}

IPState CelestronSCT::MoveAbsFocuser(uint32_t targetTicks)
{
    // Send command to focuser
    // If OK and moving, return IPS_BUSY
    // If OK and motion already done (was very small), return IPS_OK
    // If error, return IPS_ALERT

    return IPS_BUSY;
}

IPState CelestronSCT::MoveRelFocuser(FocusDirection dir, uint32_t ticks)
{
    int32_t newPosition = 0;

    if (dir == FOCUS_INWARD)
        newPosition = FocusAbsPosN[0].value - ticks;
    else
        newPosition = FocusAbsPosN[0].value + ticks;

    // Clamp
    newPosition = std::max(0, std::min(static_cast<int32_t>(FocusAbsPosN[0].max), newPosition));
    return MoveAbsFocuser(newPosition);
}

void CelestronSCT::TimerHit()
{
    if (!isConnected())
    {
        SetTimer(POLLMS);
        return;
    }

    // Check position
    double lastPosition = FocusAbsPosN[0].value;
    bool rc = readPosition();
    if (rc)
    {
        // Only update if there is actual change
        if (fabs(lastPosition - FocusAbsPosN[0].value) > 1)
            IDSetNumber(&FocusAbsPosNP, nullptr);
    }

    if (FocusAbsPosNP.s == IPS_BUSY || FocusRelPosNP.s == IPS_BUSY)
    {
        // There are two ways to know when focuser motion is over
        // define class variable uint32_t m_TargetPosition and set it in MoveAbsFocuser(..) function
        // then compare current value to m_TargetPosition
        // The other way is to have a function that calls a focuser specific function about motion
        if (!isMoving())
        {
            FocusAbsPosNP.s = IPS_OK;
            FocusRelPosNP.s = IPS_OK;
            IDSetNumber(&FocusAbsPosNP, nullptr);
            IDSetNumber(&FocusRelPosNP, nullptr);
            LOG_INFO("Focuser reached requested position.");
        }
    }

    SetTimer(POLLMS);
}

bool CelestronSCT::AbortFocuser()
{
    //return sendCommand(":FQ#");
    return false;
}

bool CelestronSCT::saveConfigItems(FILE * fp)
{
    Focuser::saveConfigItems(fp);

    IUSaveConfigNumber(fp, &BacklashNP);

    return true;
}

bool CelestronSCT::sendCommand(const char * cmd, char * res, int cmd_len, int res_len)
{
    int nbytes_written = 0, nbytes_read = 0, rc = -1;

    tcflush(PortFD, TCIOFLUSH);

    if (cmd_len > 0)
    {
        char hex_cmd[CELESTRON_LEN * 3] = {0};
        hexDump(hex_cmd, cmd, cmd_len);
        LOGF_DEBUG("CMD <%s>", hex_cmd);
        rc = tty_write(PortFD, cmd, cmd_len, &nbytes_written);
    }
    else
    {
        LOGF_DEBUG("CMD <%s>", cmd);
        rc = tty_write_string(PortFD, cmd, &nbytes_written);
    }

    if (rc != TTY_OK)
    {
        char errstr[MAXRBUF] = {0};
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("Serial write error: %s.", errstr);
        return false;
    }

    if (res == nullptr)
        return true;

    if (res_len > 0)
        rc = tty_read(PortFD, res, res_len, CELESTRON_TIMEOUT, &nbytes_read);
    else
        rc = tty_nread_section(PortFD, res, CELESTRON_LEN, CELESTRON_DEL, CELESTRON_TIMEOUT, &nbytes_read);

    if (rc != TTY_OK)
    {
        char errstr[MAXRBUF] = {0};
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("Serial read error: %s.", errstr);
        return false;
    }

    if (res_len > 0)
    {
        char hex_res[CELESTRON_LEN * 3] = {0};
        hexDump(hex_res, res, res_len);
        LOGF_DEBUG("RES <%s>", hex_res);
    }
    else
    {
        LOGF_DEBUG("RES <%s>", res);
    }

    tcflush(PortFD, TCIOFLUSH);

    return true;
}

void CelestronSCT::hexDump(char * buf, const char * data, int size)
{
    for (int i = 0; i < size; i++)
        sprintf(buf + 3 * i, "%02X ", static_cast<uint8_t>(data[i]));

    if (size > 0)
        buf[3 * size - 1] = '\0';
}
