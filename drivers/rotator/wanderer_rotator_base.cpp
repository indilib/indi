/*******************************************************************************
  Copyright(c) 2025 Frank Wang & Jérémie Klein. All rights reserved.

  WandererRotator Lite V1

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

#include "wanderer_rotator_base.h"
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



WandererRotatorBase::WandererRotatorBase()
{
    setVersion(1, 0);

}
bool WandererRotatorBase::initProperties()
{

    INDI::Rotator::initProperties();

    SetCapability(ROTATOR_CAN_REVERSE | ROTATOR_CAN_ABORT | ROTATOR_CAN_HOME);

    addAuxControls();
    // Calibrate
    SetZeroSP[0].fill("Set_Zero", "Mechanical Zero", ISS_OFF);
    SetZeroSP.fill(getDeviceName(), "Set_Zero", "Set Current As", MAIN_CONTROL_TAB, IP_RW, ISR_ATMOST1, 60, IPS_IDLE);

    // BACKLASH
    BacklashNP[BACKLASH].fill( "BACKLASH", "Degree", "%.2f", 0, 3, 0.1, 0);
    BacklashNP.fill(getDeviceName(), "BACKLASH", "Backlash", MAIN_CONTROL_TAB, IP_RW, 60, IPS_IDLE);

    serialConnection->setDefaultBaudRate(Connection::Serial::B_19200);


    return true;
}

bool WandererRotatorBase::updateProperties()
{
    INDI::Rotator::updateProperties();

    if (isConnected())
    {
        defineProperty(SetZeroSP);
        defineProperty(BacklashNP);
    }
    else
    {
        deleteProperty(SetZeroSP);
        deleteProperty(BacklashNP);
    }
    return true;
}

bool WandererRotatorBase::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{

    if (dev && !strcmp(dev, getDeviceName()))
    {
        if (SetZeroSP.isNameMatch(name))
        {
            SetZeroSP.setState(sendCommand("1500002") ? IPS_OK : IPS_ALERT);
            SetZeroSP.apply();
            GotoRotatorNP[0].setValue(0);
            GotoRotatorNP.apply();
            LOG_INFO("Virtual Mechanical Angle is set to zero.");
            return true;
        }
    }
    return Rotator::ISNewSwitch(dev, name, states, names, n);
}

bool WandererRotatorBase::ISNewNumber(const char * dev, const char * name, double values[], char * names[], int n)
{
    if (dev && !strcmp(dev, getDeviceName()))
    {
        // backlash
        if (BacklashNP.isNameMatch(name))
        {
            bool rc1 = false;
            BacklashNP.update(values, names, n);
            backlash = BacklashNP[BACKLASH].value;

            char cmd[16];
            snprintf(cmd, 16, "%d", (int)(backlash * 10 + 1600000));
            rc1 = sendCommand(cmd);

            BacklashNP.setState( (rc1) ? IPS_OK : IPS_ALERT);
            if (BacklashNP.getState() == IPS_OK)
                BacklashNP.update(values, names, n);
            BacklashNP.apply();
            LOG_INFO("Backlash Set");
            return true;
        }

    }
    return Rotator::ISNewNumber(dev, name, values, names, n);
}

bool WandererRotatorBase::Handshake()
{
    PortFD = serialConnection->getPortFD();
    tcflush(PortFD, TCIOFLUSH);
    int nbytes_read_name = 0, nbytes_written = 0, rc = -1;
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
            LOGF_INFO("No data received, the device may not be WandererRotator, please check the serial port!", "Updated");
            LOGF_ERROR("Device read error: %s", errorMessage);
            return false;
        }
    }
    name[nbytes_read_name - 1] = '\0';
    if(strcmp(name, getRotatorHandshakeName()) != 0)
    {
        LOGF_ERROR("The device is not %s", getDefaultName());
        LOGF_INFO("The device is %s.", name);
        return false;
    }
    // Frimware version/////////////////////////////////////////////////////////////////////////////////////////////
    int nbytes_read_version = 0;
    char version[64] = {0};
    tty_read_section(PortFD, version, 'A', 5, &nbytes_read_version);

    version[nbytes_read_version - 1] = '\0';
    LOGF_INFO("Firmware Version:%s", version);
    firmware = std::atoi(version);
    if(firmware < getMinimumCompatibleFirmwareVersion())
    {
        LOG_ERROR("The firmware is outdated, please upgrade to the latest firmware!");
        LOGF_ERROR("The current firmware is %s.", firmware);
        return false;
    }

    // Angle//////////////////////////////////////////////////////////////////////////////////////////
    char M_angle[64] = {0};
    int nbytes_read_M_angle = 0;
    tty_read_section(PortFD, M_angle, 'A', 5, &nbytes_read_M_angle);
    M_angle[nbytes_read_M_angle - 1] = '\0';
    M_angleread = std::strtod(M_angle, NULL);

    if(abs(M_angleread) > 400000)
    {
        rc = sendCommand("1500002");
        LOG_WARN("Virtual Mechanical Angle is too large, it is now set to zero!");
        rc = sendCommand("1500001");
        rc = tty_read_section(PortFD, M_angle, 'A', 5, &nbytes_read_M_angle);
        rc = tty_read_section(PortFD, M_angle, 'A', 5, &nbytes_read_M_angle);
        rc = tty_read_section(PortFD, M_angle, 'A', 5, &nbytes_read_M_angle);
        M_angle[nbytes_read_M_angle - 1] = '\0';
        M_angleread = std::strtod(M_angle, NULL);
    }
    GotoRotatorNP[0].setValue(abs(M_angleread / 1000));
    //backlash/////////////////////////////////////////////////////////////////////
    char M_backlash[64] = {0};
    int nbytes_read_M_backlash = 0;
    tty_read_section(PortFD, M_backlash, 'A', 5, &nbytes_read_M_backlash);
    M_backlash[nbytes_read_M_backlash - 1] = '\0';
    M_backlashread = std::strtod(M_backlash, NULL);

    BacklashNP[BACKLASH].setValue(M_backlashread);
    BacklashNP.setState(IPS_OK);
    BacklashNP.apply();
    //reverse/////////////////////////////////////////////////////////////////////
    char M_reverse[64] = {0};
    int nbytes_read_M_reverse = 0;
    tty_read_section(PortFD, M_reverse, 'A', 5, &nbytes_read_M_reverse);
    M_reverse[nbytes_read_M_angle - 1] = '\0';
    M_reverseread = std::strtod(M_reverse, NULL);
    if(M_reverseread == 0)
    {
        ReverseRotator(false);
    }
    else
    {
        ReverseRotator(true);
    }

    tcflush(PortFD, TCIOFLUSH);
    return true;
}


IPState WandererRotatorBase::MoveRotator(double angle)
{
    angle = angle - GotoRotatorNP[0].getValue();

    char cmd[16];
    int position = (int)(angle * getStepsPerDegree() + 1000000);
    positionhistory = angle;
    snprintf(cmd, 16, "%d", position);
    Move(cmd);

    return IPS_BUSY;
}


bool WandererRotatorBase::AbortRotator()
{


    if (GotoRotatorNP.getState() == IPS_BUSY)
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
    }
    return true;
}

//////////////////////////////////////////////////////////////////////////////////////////////
/// \brief WandererRotatorBase::HomeRotator
/// \return
///
IPState WandererRotatorBase::HomeRotator()
{
    if(GotoRotatorNP[0].getValue() != 0)
    {
        double angle = -1 * GotoRotatorNP[0].getValue();
        positionhistory = angle;
        char cmd[16];
        int position = (int)(angle * getStepsPerDegree() + 1000000);
        snprintf(cmd, 16, "%d", position);
        GotoRotatorNP.setState(IPS_BUSY);
        Move(cmd);
        LOG_INFO("Moving to zero...");
    }
    return IPS_OK;
}


bool WandererRotatorBase::ReverseRotator(bool enabled)
{

    if (enabled)
    {
        char cmd[16];
        snprintf(cmd, 16, "%d", 1700001);
        if (sendCommand(cmd) != true)
        {
            LOG_ERROR("Serial write error.");
            return false;
        }
        ReverseState = true;
        return true;
    }
    else
    {
        char cmd[16];
        snprintf(cmd, 16, "%d", 1700000);
        if (sendCommand(cmd) != true)
        {
            LOG_ERROR("Serial write error.");
            return false;
        }
        ReverseState = false;
        return true;
    }
    return false;
}



void WandererRotatorBase::TimerHit()
{

    if (GotoRotatorNP.getState() == IPS_BUSY || haltcommand == true)
    {

        if(nowtime < estime && haltcommand == false)
        {
            GotoRotatorNP[0].setValue(GotoRotatorNP[0].getValue() + 1 * positionhistory / abs(positionhistory));
            GotoRotatorNP.apply();
            nowtime = nowtime + 240;
            SetTimer(240);
            return;
        }
        else
        {
            int rc = -1;
            estime = 0;
            nowtime = 0;
            char M_angle[64] = {0};
            int nbytes_read_M_angle = 0;
            rc = tty_read_section(PortFD, M_angle, 'A', 5, &nbytes_read_M_angle);
            if(rc != TTY_OK)
            {
                LOG_ERROR("Rotator not powered!");
                GotoRotatorNP[0].setValue(initangle);
                GotoRotatorNP.apply();
                haltcommand = false;
                return;
            }
            tty_read_section(PortFD, M_angle, 'A', 5, &nbytes_read_M_angle);

            M_angle[nbytes_read_M_angle - 1] = '\0';
            M_angleread = std::strtod(M_angle, NULL);
            GotoRotatorNP[0].setValue(abs(M_angleread / 1000));
            GotoRotatorNP.setState(IPS_OK);
            GotoRotatorNP.apply();
            haltcommand = false;
        }
    }


    SetTimer(2000);
}



bool WandererRotatorBase::Move(const char *cmd)
{
    initangle = GotoRotatorNP[0].getValue();
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
    nowtime = 0;
    estime = abs((std::atoi(cmd) - 1000000) / getStepsPerDegree() * 240);
    return true;
}


bool WandererRotatorBase::sendCommand(std::string command)
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


