/*******************************************************************************
  Copyright(c) 2024 Frank Wang. All rights reserved.

  WandererRotator Mini V1/V2

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

#include "wanderer_rotator_mini.h"
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


// We declare an auto pointer to WandererRotatorMini.
static std::unique_ptr<WandererRotatorMini> wandererrotatormini(new WandererRotatorMini());

WandererRotatorMini::WandererRotatorMini()
{
    setVersion(1, 0);

}
bool WandererRotatorMini::initProperties()
{

    INDI::Rotator::initProperties();

    SetCapability(ROTATOR_CAN_REVERSE | ROTATOR_CAN_ABORT | ROTATOR_CAN_HOME | ROTATOR_HAS_BACKLASH);

    addAuxControls();
    // Calibrate
    SetZeroSP[0].fill("Set_Zero", "Mechanical Zero", ISS_OFF);
    SetZeroSP.fill(getDeviceName(), "Set_Zero", "Set Current As", MAIN_CONTROL_TAB, IP_RW, ISR_ATMOST1,60, IPS_IDLE);


    serialConnection->setDefaultBaudRate(Connection::Serial::B_19200);


    return true;
}

bool WandererRotatorMini::updateProperties()
{
    INDI::Rotator::updateProperties();

    if (isConnected())
    {
        defineProperty(SetZeroSP);
        if(firmware<20240208)
        {
            LOG_ERROR("The firmware is outdated, please upgrade to the latest firmware, or the driver cannot function properly!");
        }
    }
    else
    {
        deleteProperty(SetZeroSP);
    }
    return true;
}

bool WandererRotatorMini::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{

    if (dev && !strcmp(dev, getDeviceName()))
    {
        if (SetZeroSP.isNameMatch(name))
        {
            SetZeroSP.setState(sendCommand("1500002") ? IPS_OK : IPS_ALERT);
            SetZeroSP.apply();
            GotoRotatorN[0].value=0;
            IDSetNumber(&GotoRotatorNP, nullptr);
            LOG_INFO("Virtual Mechanical Angle is set to zero.");
            return true;
        }
    }
    return Rotator::ISNewSwitch(dev, name, states, names, n);
}



const char *WandererRotatorMini::getDefaultName()
{
    return "WandererRotator Mini";
}

bool WandererRotatorMini::Handshake()
{
    PortFD = serialConnection->getPortFD();
    tcflush(PortFD, TCIOFLUSH);
    int nbytes_read_name = 0,nbytes_written=0,rc=-1;
    char name[64] = {0};

    if ((rc = tty_write_string(PortFD, "1500001", &nbytes_written)) != TTY_OK)
    {
        char errorMessage[MAXRBUF];
        tty_error_msg(rc, errorMessage, MAXRBUF);
        LOGF_ERROR("Serial write error: %s", errorMessage);
        return false;
    }

    //Device Model//////////////////////////////////////////////////////////////////////////////////////////////////////
    if ((rc = tty_read_section(PortFD, name, 'A', 3, &nbytes_read_name)) != TTY_OK)
    {
            tcflush(PortFD, TCIOFLUSH);
        if ((rc = tty_write_string(PortFD, "1500001", &nbytes_written)) != TTY_OK)
        {
            char errorMessage[MAXRBUF];
            tty_error_msg(rc, errorMessage, MAXRBUF);
            LOGF_ERROR("Serial write error: %s", errorMessage);
            return false;
        }
        if ((rc = tty_read_section(PortFD, name, 'A', 3, &nbytes_read_name)) != TTY_OK)
        {
            char errorMessage[MAXRBUF];
            tty_error_msg(rc, errorMessage, MAXRBUF);
            LOGF_INFO("No data received, the device may not be WandererRotator, please check the serial port!","Updated");
            LOGF_ERROR("Device read error: %s", errorMessage);
            return false;
        }
    }
    name[nbytes_read_name - 1] = '\0';
    if(strcmp(name, "WandererRotatorMini")!=0)
    {
        LOGF_ERROR("The device is not WandererRotator Mini!","Updated");
        LOGF_INFO("The device is %s",name);
        return false;
    }
    // Frimware version/////////////////////////////////////////////////////////////////////////////////////////////
    int nbytes_read_version = 0;
    char version[64] = {0};
    tty_read_section(PortFD, version, 'A', 5, &nbytes_read_version);

    version[nbytes_read_version - 1] = '\0';
    LOGF_INFO("Firmware Version:%s", version);
    firmware=std::atoi(version);
    // Angle//////////////////////////////////////////////////////////////////////////////////////////
    char M_angle[64] = {0};
    int nbytes_read_M_angle= 0;
    tty_read_section(PortFD, M_angle, 'A', 5, &nbytes_read_M_angle);
    M_angle[nbytes_read_M_angle - 1] = '\0';
    M_angleread = std::strtod(M_angle,NULL);

    if(abs(M_angleread)>400000)
    {
        rc=sendCommand("1500002");
        LOG_WARN("Virtual Mechanical Angle is too large, it is now set to zero!");
        rc=sendCommand("1500001");
        rc=tty_read_section(PortFD, M_angle, 'A', 5, &nbytes_read_M_angle);
        rc=tty_read_section(PortFD, M_angle, 'A', 5, &nbytes_read_M_angle);
        rc=tty_read_section(PortFD, M_angle, 'A', 5, &nbytes_read_M_angle);
        M_angle[nbytes_read_M_angle - 1] = '\0';
        M_angleread = std::strtod(M_angle,NULL);

    }
    GotoRotatorN[0].value=abs(M_angleread/1000);
    tcflush(PortFD, TCIOFLUSH);
    return true;
}


IPState WandererRotatorMini::MoveRotator(double angle)
{
    angle = angle - GotoRotatorN[0].value;
    if (angle * positionhistory < 0 && angle > 0)
    {
        angle = angle + backlash;
    }
    if (angle * positionhistory < 0 && angle < 0)
    {
        angle = angle - backlash;
    }
    char cmd[16];
    int position = (int)(reversecoefficient * angle * 1142);
    positionhistory = angle;
    snprintf(cmd, 16, "%d", position);
    Move(cmd);

    return IPS_BUSY;
}



bool WandererRotatorMini::AbortRotator()
{

    haltcommand = true;

    int nbytes_written = 0, rc = -1;
    tcflush(PortFD, TCIOFLUSH);
    if ((rc = tty_write_string(PortFD, "Stop", &nbytes_written)) != TTY_OK)
    {
        char errorMessage[MAXRBUF];
        tty_error_msg(rc, errorMessage, MAXRBUF);
        LOGF_ERROR("Serial write error: %s", errorMessage);
        return false;
    }

    SetTimer(100);
    return true;
}

//////////////////////////////////////////////////////////////////////////////////////////////
/// \brief WandererRotatorMini::HomeRotator
/// \return
///
IPState WandererRotatorMini::HomeRotator()
{

    double angle = -1 * reversecoefficient * GotoRotatorN[0].value;
    positionhistory = -1* GotoRotatorN[0].value;
    char cmd[16];
    int position = (int)(angle * 1142);
    snprintf(cmd, 16, "%d", position);
    GotoRotatorNP.s = IPS_BUSY;
    Move(cmd);
    LOG_INFO("Moving to zero...");
    return IPS_OK;
}


bool WandererRotatorMini::ReverseRotator(bool enabled)
{

    if (enabled)
    {
        if(M_angleread>0)
        {
            HomeRotator();
            LOG_WARN("The rotator will first move to zero and then reverse the rotation direction to prevent cable wrap...");
        }
        reversecoefficient = -1;
        ReverseState = true;
        return true;
    }
    else
    {
        if(M_angleread<0)
        {
            HomeRotator();
            LOG_WARN("The rotator will first move to zero and then reverse the rotation direction to prevent cable wrap...");
        }
        reversecoefficient = 1;
        ReverseState = false;
        return true;
    }
    return false;
}



void WandererRotatorMini::TimerHit()
{

    if (GotoRotatorNP.s == IPS_BUSY || haltcommand == true)
    {

        if(nowtime<estime&&haltcommand == false)
        {
            GotoRotatorN[0].value=GotoRotatorN[0].value+1*positionhistory/abs(positionhistory);
            IDSetNumber(&GotoRotatorNP, nullptr);
            nowtime=nowtime+270;
            SetTimer(270);
            return;
        }
        else
        {
            int rc=-1;
            estime=0;
            nowtime=0;
            char M_angle[64] = {0};
            int nbytes_read_M_angle= 0;
            rc=tty_read_section(PortFD, M_angle, 'A', 5, &nbytes_read_M_angle);
            if(rc!=TTY_OK)
            {
                LOG_ERROR("Rotator not powered!");
                GotoRotatorN[0].value=initangle;
                IDSetNumber(&GotoRotatorNP, nullptr);
                haltcommand = false;
                return;
            }
            tty_read_section(PortFD, M_angle, 'A', 5, &nbytes_read_M_angle);

            M_angle[nbytes_read_M_angle - 1] = '\0';
            M_angleread = std::strtod(M_angle,NULL);
            GotoRotatorN[0].value=abs(M_angleread/1000);
            GotoRotatorNP.s = IPS_OK;
            IDSetNumber(&GotoRotatorNP, nullptr);
            haltcommand = false;
        }
    }


    SetTimer(2000);
}



bool WandererRotatorMini::Move(const char *cmd)
{
    initangle=GotoRotatorN[0].value;
    int nbytes_written = 0, rc = -1;
    LOGF_DEBUG("CMD <%s>", cmd);
    if ((rc = tty_write_string(PortFD, cmd, &nbytes_written)) != TTY_OK)
    {
        char errorMessage[MAXRBUF];
        tty_error_msg(rc, errorMessage, MAXRBUF);
        LOGF_ERROR("Serial write error: %s", errorMessage);
        return false;
    }
    SetTimer(2000);
    nowtime=0;
    estime=abs(std::atoi(cmd)/1142*260);
    return true;
}


bool WandererRotatorMini::sendCommand(std::string command)
{
    int nbytes_written = 0, rc = -1;
    std::string command_termination = "\n";
    LOGF_DEBUG("CMD: %s", command.c_str());
    if ((rc = tty_write_string(PortFD, (command + command_termination).c_str(), &nbytes_written)) != TTY_OK)
    {
        char errorMessage[MAXRBUF];
        tty_error_msg(rc, errorMessage, MAXRBUF);
        LOGF_ERROR("Serial write error: %s", errorMessage);
        return false;
    }
    return true;
}


bool WandererRotatorMini::SetRotatorBacklash(int32_t steps)
{
    backlash = (double)(steps / 1142);
    return true;
}

bool WandererRotatorMini::SetRotatorBacklashEnabled(bool enabled)
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


