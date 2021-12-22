/*
    Arduino ASCOM Focuser 2 (AAF2) INDI Focuser

    Copyright (C) 2019 Jasem Mutlaq (mutlaqja@ikarustech.com)

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

#include "aaf2.h"

#include "indicom.h"

#include <cmath>
#include <cstring>
#include <memory>

#include <termios.h>
#include <unistd.h>

static std::unique_ptr<AAF2> aaf2(new AAF2());

AAF2::AAF2()
{
    // Absolute, Abort, and Sync
    FI::SetCapability(FOCUSER_CAN_ABS_MOVE | FOCUSER_CAN_REL_MOVE | FOCUSER_CAN_ABORT | FOCUSER_CAN_SYNC);

    setVersion(1, 0);
}

bool AAF2::initProperties()
{
    INDI::Focuser::initProperties();

    // Focuser temperature
    IUFillNumber(&TemperatureN[0], "TEMPERATURE", "Celsius", "%6.2f", -50, 70., 0., 0.);
    IUFillNumberVector(&TemperatureNP, TemperatureN, 1, getDeviceName(), "FOCUS_TEMPERATURE", "Temperature",
                       MAIN_CONTROL_TAB, IP_RO, 0, IPS_IDLE);

    // Relative and absolute movement
    FocusRelPosN[0].min   = 0.;
    FocusRelPosN[0].max   = 50000.;
    FocusRelPosN[0].value = 0;
    FocusRelPosN[0].step  = 1000;

    FocusAbsPosN[0].min   = 0.;
    FocusAbsPosN[0].max   = 100000.;
    FocusAbsPosN[0].value = 0;
    FocusAbsPosN[0].step  = 1000;

    addDebugControl();

    return true;
}

bool AAF2::updateProperties()
{
    INDI::Focuser::updateProperties();

    if (isConnected())
    {
        defineProperty(&TemperatureNP);

        LOG_INFO("Focuser ready.");
    }
    else
    {
        deleteProperty(TemperatureNP.name);
    }

    return true;
}

bool AAF2::Handshake()
{
    if (Ack())
    {
        LOG_INFO("AAF2 is online.");

        readVersion();

        return true;
    }

    LOG_INFO("Error retrieving data from AAF2, please ensure AAF2 controller is powered and the port is correct.");
    return false;
}

const char * AAF2::getDefaultName()
{
    return "AAF2";
}

bool AAF2::Ack()
{
    int nbytes_written = 0, nbytes_read = 0, rc = -1;
    char errstr[MAXRBUF];
    char resp[5] = {0};

    tcflush(PortFD, TCIOFLUSH);

    int numChecks = 0;
    bool success = false;
    while (numChecks < 3 && !success)
    {
        numChecks++;
        //wait 1 second between each test.
        sleep(1);

        bool transmissionSuccess = (rc = tty_write(PortFD, "#", 1, &nbytes_written)) == TTY_OK;
        if(!transmissionSuccess)
        {
            tty_error_msg(rc, errstr, MAXRBUF);
            LOGF_ERROR("Handshake Attempt %i, tty transmission error: %s.", numChecks, errstr);
        }

        bool responseSuccess = (rc = tty_read(PortFD, resp, 4, DRIVER_TIMEOUT, &nbytes_read)) == TTY_OK;
        if(!responseSuccess)
        {
            tty_error_msg(rc, errstr, MAXRBUF);
            LOGF_ERROR("Handshake Attempt %i, updatePosition response error: %s.", numChecks, errstr);
        }

        success = transmissionSuccess && responseSuccess;
    }

    if(!success)
    {
        LOG_INFO("Handshake failed after 3 attempts");
        return false;
    }

    tcflush(PortFD, TCIOFLUSH);

    return !strcmp(resp, "OK!#");
}


bool AAF2::readTemperature()
{
    char res[DRIVER_RES] = {0};

    if (sendCommand("C#", res) == false)
        return false;

    int32_t temp = 0;
    int rc = sscanf(res, "C%d:OK", &temp);
    if (rc > 0)
        // Hundredth of a degree
        TemperatureN[0].value = temp / 100.0;
    else
    {
        LOGF_ERROR("Unknown error: focuser temperature value (%s)", res);
        return false;
    }

    return true;
}

bool AAF2::readVersion()
{
    char res[DRIVER_RES] = {0};

    if (sendCommand("V#", res) == false)
        return false;

    LOGF_INFO("Detected %s", res);

    return true;
}

bool AAF2::readPosition()
{
    char res[DRIVER_RES] = {0};

    if (sendCommand("P#", res) == false)
        return false;

    int32_t pos;
    int rc = sscanf(res, "P%d:OK", &pos);

    if (rc > 0)
        FocusAbsPosN[0].value = pos;
    else
    {
        LOGF_ERROR("Unknown error: focuser position value (%s)", res);
        return false;
    }

    return true;
}

bool AAF2::isMoving()
{
    char res[DRIVER_RES] = {0};

    if (sendCommand("M#", res) == false)
        return false;

    if (strcmp(res, "M1:OK") == 0)
        return true;
    else if (strcmp(res, "M0:OK") == 0)
        return false;

    LOGF_ERROR("Unknown error: isMoving value (%s)", res);
    return false;
}


bool AAF2::SyncFocuser(uint32_t ticks)
{
    char cmd[DRIVER_RES] = {0};
    snprintf(cmd, DRIVER_RES, "I%d#", ticks);
    return sendCommand(cmd);
}

IPState AAF2::MoveAbsFocuser(uint32_t targetTicks)
{
    char cmd[DRIVER_RES] = {0}, res[DRIVER_RES] = {0}, expected[DRIVER_RES] = {0};
    snprintf(cmd, DRIVER_RES, "T%d#", targetTicks);
    snprintf(expected, DRIVER_RES, "T%d:OK", targetTicks);
    if (sendCommand(cmd, res) == false)
        return IPS_ALERT;

    targetPos = targetTicks;

    if (!strcmp(res, expected))
        return IPS_BUSY;
    else
        return IPS_ALERT;
}

IPState AAF2::MoveRelFocuser(FocusDirection dir, uint32_t ticks)
{
    int32_t newPosition = 0;

    if (dir == FOCUS_INWARD)
        newPosition = FocusAbsPosN[0].value - ticks;
    else
        newPosition = FocusAbsPosN[0].value + ticks;

    // Clamp
    newPosition = std::max(0, std::min(static_cast<int32_t>(FocusAbsPosN[0].max), newPosition));
    if (MoveAbsFocuser(newPosition) != IPS_BUSY)
        return IPS_ALERT;

    FocusRelPosN[0].value = ticks;
    FocusRelPosNP.s       = IPS_BUSY;

    return IPS_BUSY;
}

void AAF2::TimerHit()
{
    if (!isConnected())
    {
        SetTimer(getCurrentPollingPeriod());
        return;
    }

    bool rc = readPosition();
    if (rc)
    {
        if (fabs(lastPos - FocusAbsPosN[0].value) > 5)
        {
            IDSetNumber(&FocusAbsPosNP, nullptr);
            lastPos = FocusAbsPosN[0].value;
        }
    }

    rc = readTemperature();
    if (rc)
    {
        if (fabs(lastTemperature - TemperatureN[0].value) >= 0.5)
        {
            IDSetNumber(&TemperatureNP, nullptr);
            lastTemperature = TemperatureN[0].value;
        }
    }

    if (FocusAbsPosNP.s == IPS_BUSY || FocusRelPosNP.s == IPS_BUSY)
    {
        if (!isMoving())
        {
            FocusAbsPosNP.s = IPS_OK;
            FocusRelPosNP.s = IPS_OK;
            IDSetNumber(&FocusAbsPosNP, nullptr);
            IDSetNumber(&FocusRelPosNP, nullptr);
            lastPos = FocusAbsPosN[0].value;
            LOG_INFO("Focuser reached requested position.");
        }
    }

    SetTimer(getCurrentPollingPeriod());
}

bool AAF2::AbortFocuser()
{
    return sendCommand("H#");
}


bool AAF2::sendCommand(const char * cmd, char * res)
{
    int nbytes_written = 0, nbytes_read = 0, rc = -1;

    tcflush(PortFD, TCIOFLUSH);

    LOGF_DEBUG("CMD <%s>", cmd);

    if ((rc = tty_write_string(PortFD, cmd, &nbytes_written)) != TTY_OK)
    {
        char errstr[MAXRBUF] = {0};
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("Serial write error: %s.", errstr);
        return false;
    }

    if (res == nullptr)
        return true;

    if ((rc = tty_nread_section(PortFD, res, DRIVER_RES, DRIVER_DEL, DRIVER_TIMEOUT, &nbytes_read)) != TTY_OK)
    {
        char errstr[MAXRBUF] = {0};
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("Serial read error: %s.", errstr);
        return false;
    }

    // Remove the #
    res[nbytes_read - 1] = 0;

    LOGF_DEBUG("RES <%s>", res);

    tcflush(PortFD, TCIOFLUSH);

    return true;
}
