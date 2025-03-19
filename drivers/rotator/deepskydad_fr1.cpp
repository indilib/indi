/*******************************************************************************
  Copyright(c) 2020 Jasem Mutlaq. All rights reserved.

  Deep Sky Dad Field Rotator 1

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

#include "deepskydad_fr1.h"
#include "indicom.h"
#include "connectionplugins/connectionserial.h"

#include <cerrno>
#include <cstring>
#include <memory>
#include <termios.h>
#include <unistd.h>
#include <inttypes.h>
#include <sys/ioctl.h>
#include <cmath>

#define DSD_CMD 40
#define DSD_RES 40
#define DSD_TIMEOUT 3

// We declare an auto pointer to DeepSkyDadFR1.
static std::unique_ptr<DeepSkyDadFR1> dsdFR1(new DeepSkyDadFR1());

DeepSkyDadFR1::DeepSkyDadFR1()
{
    setVersion(1, 0);
}

bool DeepSkyDadFR1::initProperties()
{
    INDI::Rotator::initProperties();

    SetCapability(ROTATOR_CAN_ABORT |
                  ROTATOR_CAN_REVERSE |
                  ROTATOR_CAN_SYNC);

    addAuxControls();

    // Speed mode
    SpeedModeSP[Slow].fill("SLOW", "Slow", ISS_OFF);
    SpeedModeSP[Fast].fill( "FAST", "Fast", ISS_OFF);
    SpeedModeSP.fill(getDeviceName(), "Speed mode", "Speed mode", MAIN_CONTROL_TAB, IP_RW,
                     ISR_1OFMANY, 0, IPS_IDLE);

    // Step mode
    StepSizeSP[One].fill("1", "1", ISS_OFF);
    StepSizeSP[Two].fill("2", "1/2", ISS_OFF);
    StepSizeSP[Four].fill( "4", "1/4", ISS_OFF);
    StepSizeSP[Eight].fill( "8", "1/8", ISS_OFF);
    StepSizeSP.fill(getDeviceName(), "Step mode", "Step mode", MAIN_CONTROL_TAB, IP_RW,
                    ISR_1OFMANY, 0, IPS_IDLE);

    // Firmware version
    FirmwareTP[0].fill("Version", "Version", nullptr);
    FirmwareTP.fill(getDeviceName(), "Firmware", "Firmware", MAIN_CONTROL_TAB, IP_RO, 60, IPS_IDLE);

    serialConnection->setDefaultPort("/dev/ttyACM0");
    serialConnection->registerHandshake([&]()
    {
        return Handshake();
    });
    serialConnection->setDefaultBaudRate(Connection::Serial::B_115200);
    return true;
}

bool DeepSkyDadFR1::updateProperties()
{
    INDI::Rotator::updateProperties();

    if (isConnected())
    {
        defineProperty(SpeedModeSP);
        defineProperty(StepSizeSP);
        defineProperty(FirmwareTP);
    }
    else
    {
        deleteProperty(SpeedModeSP);
        deleteProperty(StepSizeSP);
        deleteProperty(FirmwareTP);
    }

    return true;
}

const char * DeepSkyDadFR1::getDefaultName()
{
    return "Deep Sky Dad FR1";
}

bool DeepSkyDadFR1::Handshake()
{
    PortFD = serialConnection->getPortFD();
    return getInitialStatusData();
}

bool DeepSkyDadFR1::ISNewSwitch(const char * dev, const char * name, ISState * states, char * names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        if (SpeedModeSP.isNameMatch(name))
        {
            int current_mode = SpeedModeSP.findOnSwitchIndex();

            SpeedModeSP.update(states, names, n);

            int target_mode = SpeedModeSP.findOnSwitchIndex();

            if (current_mode == target_mode)
            {
                SpeedModeSP.setState(IPS_OK);
                SpeedModeSP.apply();
                return true;
            }

            char cmd[DSD_RES] = {0};
            char response[DSD_RES] = {0};

            snprintf(cmd, DSD_RES, "[SSPD%d]", target_mode);
            bool rc = sendCommand(cmd, response);
            if (!rc)
            {
                SpeedModeSP.reset();
                SpeedModeSP[current_mode].setState(ISS_ON);
                SpeedModeSP.setState(IPS_ALERT);
                SpeedModeSP.apply();
                return false;
            }

            SpeedModeSP.setState(IPS_OK);
            SpeedModeSP.apply();
            return true;
        }
        else  if (StepSizeSP.isNameMatch(name))
        {
            int current_mode = StepSizeSP.findOnSwitchIndex();

            StepSizeSP.update(states, names, n);

            int target_mode = StepSizeSP.findOnSwitchIndex();

            if (current_mode == target_mode)
            {
                StepSizeSP.setState(IPS_OK);
                StepSizeSP.apply();
                return true;
            }

            char cmd[DSD_RES] = {0};
            char response[DSD_RES] = {0};

            snprintf(cmd, DSD_RES, "[SSTP%d]", target_mode);
            bool rc = sendCommand(cmd, response);
            if (!rc)
            {
                StepSizeSP.reset();
                StepSizeSP[current_mode].setState(ISS_ON);
                StepSizeSP.setState(IPS_ALERT);
                StepSizeSP.apply();
                return false;
            }

            StepSizeSP.setState(IPS_OK);
            StepSizeSP.apply();
            return true;
        }
    }

    return Rotator::ISNewSwitch(dev, name, states, names, n);
}

IPState DeepSkyDadFR1::MoveRotator(double angle)
{
    char response[DSD_RES];
    char cmd[DSD_CMD];
    int angleInt = (int)(angle * 100);
    snprintf(cmd, DSD_CMD, "[STRG%d]", angleInt);
    if (!sendCommand(cmd, response) || !sendCommand("[SMOV]", response))
        return IPS_ALERT;

    if (strcmp(response, "(OK)") == 0)
    {
        return IPS_BUSY;
    }
    else
        return IPS_ALERT;
}

bool DeepSkyDadFR1::AbortRotator()
{
    char response[DSD_RES];
    if (!sendCommand("[STOP]", response))
        return false;

    if (strcmp(response, "(OK)") == 0)
    {
        return true;
    }
    else
        return false;
}

bool DeepSkyDadFR1::ReverseRotator(bool enabled)
{
    char response[DSD_RES];
    char cmd[DSD_CMD];
    snprintf(cmd, DSD_CMD, "[SREV%d]", enabled ? 1 : 0);
    if (!sendCommand(cmd, response))
        return false;

    if (strcmp(response, "(OK)") == 0)
    {
        return true;
    }
    else
        return false;
}

bool DeepSkyDadFR1::SyncRotator(double angle)
{
    char response[DSD_RES];
    char cmd[DSD_CMD];
    int angleInt = (int)(angle * 100);
    snprintf(cmd, DSD_CMD, "[SPOS%d]", angleInt);
    if (!sendCommand(cmd, response))
        return false;

    if (strcmp(response, "(OK)") == 0)
    {
        return true;
    }
    else
        return false;
}

void DeepSkyDadFR1::TimerHit()
{
    if (!isConnected())
        return;
    getStatusData();
    SetTimer(getCurrentPollingPeriod());
}

bool DeepSkyDadFR1::getStatusData()
{
    char response[DSD_RES];

    int motorStatus;
    int motorPosition;

    if (!sendCommand("[GMOV]", response))
        return false;
    else
        sscanf(response, "(%d)", &motorStatus);

    if (!sendCommand("[GPOS]", response))
        return false;
    else
        sscanf(response, "(%d)", &motorPosition);



    const IPState motionState = motorStatus == 1 ? IPS_BUSY : IPS_OK;

    double motorPositionDouble = (double)motorPosition / (double)100;
    if (std::abs(motorPositionDouble - GotoRotatorNP[0].getValue()) > 0.01 || GotoRotatorNP.getState() != motionState)
    {
        GotoRotatorNP[0].setValue(motorPositionDouble);
        GotoRotatorNP.setState(motionState);
        GotoRotatorNP.apply();
    }

    return true;
}

bool DeepSkyDadFR1::getInitialStatusData()
{
    char response[DSD_RES] = {0};
    if (!sendCommand("[GFRM]", response))
        return false;

    char versionString[6] = { 0 };
    snprintf(versionString, 6, "%s", response + 31);
    FirmwareTP[0].setText(response);
    FirmwareTP.apply();

    int motorReversed;

    if (!sendCommand("[GREV]", response))
        return false;
    else
        sscanf(response, "(%d)", &motorReversed);

    const bool wasReversed = ReverseRotatorSP[INDI_ENABLED].getState() == ISS_ON;
    if (motorReversed != wasReversed)
    {
        ReverseRotatorSP[INDI_ENABLED].setState(motorReversed ? ISS_ON : ISS_OFF);
        ReverseRotatorSP[INDI_DISABLED].setState(motorReversed ? ISS_OFF : ISS_ON);
        ReverseRotatorSP.apply();
    }

    if (!sendCommand("[GSPD]", response))
        return false;

    if(strcmp(response, "(2)") == 0)
        SpeedModeSP[Slow].setState(ISS_ON);
    else if(strcmp(response, "(3)") == 0)
        SpeedModeSP[Fast].setState(ISS_ON);

    if (!sendCommand("[GSTP]", response))
        return false;

    if(strcmp(response, "(1)") == 0)
        StepSizeSP[One].setState(ISS_ON);
    else if(strcmp(response, "(2)") == 0)
        StepSizeSP[Two].setState(ISS_ON);
    else if(strcmp(response, "(4)") == 0)
        StepSizeSP[Four].setState(ISS_ON);
    else if(strcmp(response, "(8)") == 0)
        StepSizeSP[Eight].setState(ISS_ON);

    return true;
}

/////////////////////////////////////////////////////////////////////////////
/// Send Command
/////////////////////////////////////////////////////////////////////////////
bool DeepSkyDadFR1::sendCommand(const char *cmd, char *res)
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

    if ((rc = tty_nread_section(PortFD, res, DSD_RES, ')', DSD_TIMEOUT, &nbytes_read)) != TTY_OK)
    {
        char errstr[MAXRBUF] = {0};
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("Serial read error: %s.", errstr);
        return false;
    }

    LOGF_DEBUG("RES <%s>", res);

    tcflush(PortFD, TCIOFLUSH);

    return true;
}
