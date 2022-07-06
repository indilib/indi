/*
   WandererAstro WandererRotatorLite
   Copyright (C) 2022 Frank Wang (1010965596@qq.com)

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

#include "wanderer_rotator_lite.h"
#include "indicom.h"
#include "connectionplugins/connectionserial.h"
#include <cmath>
#include <cstring>
#include <memory>
#include <termios.h>
#include <unistd.h>
#include <stdio.h>
#include <sstream>
#include <iostream>


// We declare an auto pointer to WandererRotatorLite.
static std::unique_ptr<WandererRotatorLite> RotatorLite(new WandererRotatorLite());

WandererRotatorLite::WandererRotatorLite()
{
    setVersion(1, 0);
    RI::SetCapability(ROTATOR_CAN_REVERSE | ROTATOR_CAN_SYNC | ROTATOR_CAN_ABORT | ROTATOR_CAN_HOME | ROTATOR_HAS_BACKLASH);
}
bool WandererRotatorLite::initProperties()
{
    INDI::Rotator::initProperties();
    setDefaultPollingPeriod(500);
    serialConnection->setDefaultBaudRate(Connection::Serial::B_19200);
    IUFillSwitch(&HomeS[0], "SetHomeButton", "Set Current Position as Home", ISS_OFF);
    IUFillSwitchVector(&HomeSP, HomeS, 1, getDeviceName(), "SetHome", "Set Home", MAIN_CONTROL_TAB, IP_RW, ISR_ATMOST1, 5,
                       IPS_IDLE);

    return true;
}

bool WandererRotatorLite::updateProperties()
{
    INDI::Rotator::updateProperties();

    if (isConnected())
    {

        defineProperty(&HomeSP);
        deleteProperty(PresetNP.name);
        deleteProperty(PresetGotoSP.name);
    }
    else
    {
        deleteProperty(HomeSP.name);
        deleteProperty(PresetNP.name);
        deleteProperty(PresetGotoSP.name);
    }
    return true;
}

bool WandererRotatorLite::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if (dev && !strcmp(dev, getDeviceName()))
    {
        if (!strcmp(name, HomeSP.name))
        {

            HomeSP.s = SetHomePosition() ? IPS_OK : IPS_ALERT;
            GotoRotatorN[0].value = 0;
            LOG_INFO("Home is set");
            IDSetSwitch(&HomeSP, nullptr);
            IDSetNumber(&GotoRotatorNP, nullptr);
            return true;

        }
        if (!strcmp(name, HomeRotatorSP.name))
        {

            HomeRotatorSP.s = HomeRotator();
            LOG_INFO("Homing....");
            IDSetSwitch(&HomeRotatorSP, nullptr);
            return true;

        }
    }

    return Rotator::ISNewSwitch(dev, name, states, names, n);
}



const char *WandererRotatorLite::getDefaultName()
{
    return "WandererRotatorLite";
}
//////////////////////////////////////////////////////////////////////////////////////
/// \brief WandererRotatorLite::Handshake
/// \return
///
bool WandererRotatorLite::Handshake()
{
    tcflush(PortFD, TCIOFLUSH);
    int nbytes_read1 = 0, nbytes_written = 0, rc = -1;
    int nbytes_read2 = 0;
    int nbytes_read3 = 0;
    char res1[64] = {0};
    char res2[64] = {0};
    char res3[64] = {0};
    LOGF_DEBUG("CMD <%s>", "150001");
    if ((rc = tty_write_string(PortFD, "1500001", &nbytes_written)) != TTY_OK)
    {
        char errorMessage[MAXRBUF];
        tty_error_msg(rc, errorMessage, MAXRBUF);
        LOGF_ERROR("Serial write error: %s", errorMessage);
        return false;
    }

    if ((rc = tty_read_section(PortFD, res1, 'A', 5, &nbytes_read1)) != TTY_OK)
    {
        char errorMessage[MAXRBUF];
        tty_error_msg(rc, errorMessage, MAXRBUF);
        LOGF_ERROR("Device read error: %s", errorMessage);
        return false;
    }
    if ((rc = tty_read_section(PortFD, res2, 'A', 5, &nbytes_read2)) != TTY_OK)
    {
        char errorMessage[MAXRBUF];
        tty_error_msg(rc, errorMessage, MAXRBUF);
        LOGF_ERROR("Firmware read error: %s", errorMessage);
        return false;
    }
    if ((rc = tty_read_section(PortFD, res3, 'A', 5, &nbytes_read3)) != TTY_OK)
    {
        char errorMessage[MAXRBUF];
        tty_error_msg(rc, errorMessage, MAXRBUF);
        LOGF_ERROR("Position read error: %s", errorMessage);
        return false;
    }
    if(atof(res3) >= 100000)
    {
        GotoRotatorN[0].value = (atof(res3) - 100000) / 100;
    }
    else
    {
        GotoRotatorN[0].value = atof(res3) / 100;
    }
    res1[nbytes_read1 - 1] = '\0';
    res2[nbytes_read2 - 1] = '\0';
    res3[nbytes_read3 - 1] = '\0';
    LOGF_DEBUG("RES <%s>", res1);
    LOGF_INFO("Handshake successful:%s", res1);
    LOGF_INFO("Firmware Version:%s", res2);
    tcflush(PortFD, TCIOFLUSH);
    return true;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// \brief WandererRotatorLite::MoveRotator
/// \param angle
/// \return
///

IPState WandererRotatorLite::MoveRotator(double angle)
{
    backlashcompensation = 0;
    angle = angle - GotoRotatorN[0].value;
    if (angle * positionhistory < 0 && angle > 0)
    {
        angle = angle + backlash;
        backlashcompensation = -1 * backlash;
    }
    if (angle * positionhistory < 0 && angle < 0)
    {
        angle = angle - backlash;
        backlashcompensation = backlash;
    }
    char cmd[16];
    int position = (int)(reversecoefficient * angle * 1155);
    positionhistory = angle;
    snprintf(cmd, 16, "%d", position);
    Move(cmd);
    return IPS_BUSY;
}

/////////////////////////////////////////////////////////////////////////////////////////////
/// \brief WandererRotatorLite::AbortRotator
/// \return

bool WandererRotatorLite::AbortRotator()
{

    haltcommand = true;
    positioncount = 0;
    SetTimer(10);
    int nbytes_read1 = 0, nbytes_written = 0, rc = -1;
    int nbytes_read2 = 0;
    char res1[16] = {0};
    char res2[16] = {0};
    tcflush(PortFD, TCIOFLUSH);
    if ((rc = tty_write_string(PortFD, "Stop", &nbytes_written)) != TTY_OK)
    {
        char errorMessage[MAXRBUF];
        tty_error_msg(rc, errorMessage, MAXRBUF);
        LOGF_ERROR("Serial write error: %s", errorMessage);
        return false;
    }

    if ((rc = tty_read_section(PortFD, res1, 'A', 5, &nbytes_read1)) != TTY_OK)
    {
        char errorMessage[MAXRBUF];
        tty_error_msg(rc, errorMessage, MAXRBUF);
        LOGF_ERROR("Relative position read error: %s", errorMessage);
        return false;
    }
    if ((rc = tty_read_section(PortFD, res2, 'A', 5, &nbytes_read2)) != TTY_OK)
    {
        char errorMessage[MAXRBUF];
        tty_error_msg(rc, errorMessage, MAXRBUF);
        LOGF_ERROR("Mechanical position read error: %s", errorMessage);
        return false;
    }
    else
    {
        haltcommand = false;
    }
    GotoRotatorN[0].value = atof(res2) / 100;
    res1[nbytes_read1 - 1] = '\0';
    res2[nbytes_read2 - 1] = '\0';
    LOGF_DEBUG("Move Relative:%s", res1);
    LOGF_DEBUG("Move to Mechanical:%s", res2);
    tcflush(PortFD, TCIOFLUSH);
    return true;
}

//////////////////////////////////////////////////////////////////////////////////////////////
/// \brief WandererRotatorLite::HomeRotator
/// \return
///
IPState WandererRotatorLite::HomeRotator()
{

    double angle = -1 * reversecoefficient * GotoRotatorN[0].value;
    positionhistory = angle;
    char cmd[16];
    int position = (int)(angle * 1155);
    snprintf(cmd, 16, "%d", position);
    Move(cmd);
    return IPS_BUSY;
}

/////////////////////////////////////////////////////////////
/// \brief WandererRotatorLite::ReverseRotator
/// \param enabled
/// \return
///
bool WandererRotatorLite::ReverseRotator(bool enabled)
{

    if (enabled)
    {
        reversecoefficient = -1;
        ReverseState = true;
        return true;
    }

    else
    {
        reversecoefficient = 1;
        ReverseState = false;
        return true;
    }
    return false;
}

///////////////////////////////////////////////////////////////////////////////////////////
/// \brief WandererRotatorLite::TimerHit
///

void WandererRotatorLite::TimerHit()
{

    if(haltcommand)
    {
        GotoRotatorNP.s = IPS_OK;
        HomeRotatorSP.s = IPS_OK;
        IDSetSwitch(&HomeRotatorSP, nullptr);
        haltcommand = false;
        positioncount = 0;
        return;
    }
    if(positioncount > 0)
    {
        GotoRotatorN[0].value += reversecoefficient * abs(positionhistory) / positionhistory;
        positioncount--;
        IDSetNumber(&GotoRotatorNP, nullptr);
        SetTimer(150);
        return;
    }
    if (GotoRotatorNP.s == IPS_BUSY || HomeRotatorSP.s == IPS_BUSY)
    {
        LOG_INFO("Done");
        int nbytes_read1 = 0, rc = -1;
        int nbytes_read2 = 0;
        char res1[16] = {0};
        char res2[16] = {0};

        if ((rc = tty_read_section(PortFD, res1, 'A', 5, &nbytes_read1)) != TTY_OK)
        {
            char errorMessage[MAXRBUF];
            tty_error_msg(rc, errorMessage, MAXRBUF);
            LOGF_ERROR("Relative position read error: %s", errorMessage);
            return;
        }
        if ((rc = tty_read_section(PortFD, res2, 'A', 5, &nbytes_read2)) != TTY_OK)
        {
            char errorMessage[MAXRBUF];
            tty_error_msg(rc, errorMessage, MAXRBUF);
            LOGF_ERROR("Mechanical position read error: %s", errorMessage);
            return;
        }
        GotoRotatorN[0].value = positiontemp + reversecoefficient * atof(res1) + backlashcompensation;

        GotoRotatorNP.s = IPS_OK;
        HomeRotatorSP.s = IPS_OK;
        IDSetNumber(&GotoRotatorNP, nullptr);
        IDSetSwitch(&HomeRotatorSP, nullptr);
    }
    SetTimer(getCurrentPollingPeriod());
}

///////////////////////////////////////////////////////////////////////////////////////////////
/// \brief WandererRotatorLite::Move
/// \param cmd
/// \return
///

bool WandererRotatorLite::Move(const char *cmd)
{
    positiontemp = GotoRotatorN[0].value;
    int nbytes_written = 0, rc = -1;
    LOGF_DEBUG("CMD <%s>", cmd);
    positioncount = abs(atoi(cmd) / 1155);
    if ((rc = tty_write_string(PortFD, cmd, &nbytes_written)) != TTY_OK)
    {
        char errorMessage[MAXRBUF];
        tty_error_msg(rc, errorMessage, MAXRBUF);
        LOGF_ERROR("Serial write error: %s", errorMessage);
        return false;
    }
    usleep(1200000);
    SetTimer(150);
    return true;
}


///////////////////////////////////////////////////////////////////////////////////////////
/// \brief WandererRotatorLite::sendCommand
/// \param cmd
/// \return
///

bool WandererRotatorLite::sendCommand(const char *cmd)
{
    int nbytes_read = 0, nbytes_written = 0, rc = -1;
    char res[32] = {0};
    LOGF_DEBUG("CMD <%s>", cmd);

    tcflush(PortFD, TCIOFLUSH);
    if ((rc = tty_write_string(PortFD, cmd, &nbytes_written)) != TTY_OK)
    {
        char errorMessage[MAXRBUF];
        tty_error_msg(rc, errorMessage, MAXRBUF);
        LOGF_ERROR("Serial write error: %s", errorMessage);
        return false;
    }


    if ((rc = tty_read_section(PortFD, res, 'A', 60, &nbytes_read)) != TTY_OK)
    {
        char errorMessage[MAXRBUF];
        tty_error_msg(rc, errorMessage, MAXRBUF);
        LOGF_ERROR("Serial read error: %s", errorMessage);
        return false;
    }
    tcflush(PortFD, TCIOFLUSH);
    res[nbytes_read - 1] = '\0';
    LOGF_DEBUG("RES <%s>", res);
    return true;
}


/////////////////////////////////////////////////////////////////////////////////////
/// \brief WandererRotatorLite::SetHomePosition
/// \return
///
bool WandererRotatorLite::SetHomePosition()
{
    int nbytes_written = 0, rc = -1;
    LOGF_DEBUG("CMD <%s>", "1500002");

    tcflush(PortFD, TCIOFLUSH);
    if ((rc = tty_write_string(PortFD, "1500002", &nbytes_written)) != TTY_OK)
    {
        char errorMessage[MAXRBUF];
        tty_error_msg(rc, errorMessage, MAXRBUF);
        LOGF_ERROR("Serial write error: %s", errorMessage);
        return false;
    }

    tcflush(PortFD, TCIOFLUSH);
    GotoRotatorN[0].value = 0;
    return true;
}
/////////////////////////////////////////////////////////////////////////////
/// \brief WandererRotatorLite::SetRotatorBacklash
/// \param steps
/// \return
///
bool WandererRotatorLite::SetRotatorBacklash(int32_t steps)
{
    backlash = (double)(steps / 1155);
    return true;
}
////////////////////////////////////////////////////////////////////////////////
/// \brief WandererRotatorLite::SetRotatorBacklashEnabled
/// \param enabled
/// \return
///
bool WandererRotatorLite::SetRotatorBacklashEnabled(bool enabled)
{
    if(enabled)
    {
        return SetRotatorBacklash(RotatorBacklashN[0].value);
    }
    else
    {
        return SetRotatorBacklash(0);
    }

}
