/*******************************************************************************
  Copyright(c) 2023 Chrysikos Efstathios. All rights reserved.

  Pegasus FocusCube

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
#include "pegasus_focuscube3.h"

#include "indicom.h"
#include "connectionplugins/connectionserial.h"

#include <cmath>
#include <cstring>
#include <memory>
#include <regex>
#include <termios.h>
#include <unistd.h>

#define DMFC_TIMEOUT 3
#define FOCUS_SETTINGS_TAB "Settings"
#define TEMPERATURE_THRESHOLD 0.1

static std::unique_ptr<PegasusFocusCube3> focusCube(new PegasusFocusCube3());


PegasusFocusCube3::PegasusFocusCube3()
{
    setVersion(1, 0);
    // Can move in Absolute & Relative motions, can AbortFocuser motion.
    FI::SetCapability(FOCUSER_CAN_ABS_MOVE |
                      FOCUSER_CAN_REL_MOVE |
                      FOCUSER_CAN_ABORT |
                      FOCUSER_CAN_REVERSE |
                      FOCUSER_CAN_SYNC |
                      FOCUSER_HAS_BACKLASH);
}


bool PegasusFocusCube3::Handshake()
{
    int tty_rc = 0, nbytes_written = 0, nbytes_read = 0;
    char command[PEGASUS_LEN] = {0}, response[PEGASUS_LEN] = {0};
    PortFD = serialConnection->getPortFD();
    LOG_DEBUG("CMD <##>");




    tcflush(PortFD, TCIOFLUSH);
    strncpy(command, "##\r\n", PEGASUS_LEN);
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

    return (strstr(response, "FC3") != nullptr);
}

const char *PegasusFocusCube3::getDefaultName()
{
    return "Pegasus FocusCube3";
}

bool PegasusFocusCube3::initProperties()
{
    INDI::Focuser::initProperties();

    TemperatureNP[0].fill("TEMP", "Level", "%.0f", -40, 40, 1, 0);
    TemperatureNP.fill(getDeviceName(), "TEMP", "Temperature", FOCUS_SETTINGS_TAB, IP_RW, 60, IPS_IDLE);

    FirmwareVersionTP[0].fill("Version", "Version", "");
    FirmwareVersionTP.fill(getDeviceName(), "Firmware", "Firmware", MAIN_CONTROL_TAB, IP_RO, 0, IPS_IDLE);

    SpeedNP[0].fill("Speed", "Value", "%6.2f", 100, 1000., 100., 400.);
    SpeedNP.fill(getDeviceName(), "MaxSpeed", "", FOCUS_SETTINGS_TAB, IP_RW, 0, IPS_IDLE);

    // Relative and absolute movement
    FocusRelPosNP[0].setMin(0.);
    FocusRelPosNP[0].setMax(50000.);
    FocusRelPosNP[0].setValue(0);
    FocusRelPosNP[0].setStep(1000);

    FocusAbsPosNP[0].setMin(0.);
    FocusAbsPosNP[0].setValue(0);
    FocusAbsPosNP[0].setStep(1000);

    // Backlash compensation
    FocusBacklashNP[0].setMin(1); // 0 is off.
    FocusBacklashNP[0].setMax(1000);
    FocusBacklashNP[0].setValue(1);
    FocusBacklashNP[0].setStep(1);

    FocusMaxPosNP[0].setMax(1317500);
    FocusMaxPosNP[0].setValue(1317500);
    FocusAbsPosNP[0].setMax(1317500);

    addDebugControl();
    setDefaultPollingPeriod(200);
    serialConnection->setDefaultBaudRate(Connection::Serial::B_115200);

    return true;
}

bool PegasusFocusCube3::updateProperties()
{
    INDI::Focuser::updateProperties();

    if (isConnected())
    {
        defineProperty(TemperatureNP);

        defineProperty(FirmwareVersionTP);
        std::string firmware = getFirmwareVersion();
        FirmwareVersionTP[0].setText(firmware);
        FirmwareVersionTP.setState(IPS_OK);
        FirmwareVersionTP.apply();
        defineProperty(SpeedNP);


    }
    else
    {
        deleteProperty(TemperatureNP);
        deleteProperty(FirmwareVersionTP);
        deleteProperty(SpeedNP);
    }

    return true;
}


bool PegasusFocusCube3::ISNewNumber(const char * dev, const char * name, double values[], char * names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()))
    {

        if(SpeedNP.isNameMatch(name))
        {
            SpeedNP.update(values, names, n);
            IPState result = IPS_OK;
            if (isConnected())
            {
                if(!setSpeed(values[0]))
                {
                    result = IPS_ALERT;
                }
            }
            SpeedNP.setState(result);
            SpeedNP.apply();
        }

    }

    return INDI::Focuser::ISNewNumber(dev, name, values, names, n);
}

IPState PegasusFocusCube3::MoveAbsFocuser(uint32_t targetTicks)
{
    targetPosition = targetTicks;

    bool rc = move(targetPosition);

    if (!rc)
        return IPS_ALERT;

    FocusAbsPosNP.setState(IPS_BUSY);

    return IPS_BUSY;
}

IPState PegasusFocusCube3::MoveRelFocuser(FocusDirection dir, uint32_t ticks)
{
    double newPosition = 0;
    bool rc = false;

    if (dir == FOCUS_INWARD)
        newPosition = FocusAbsPosNP[0].getValue() - ticks;
    else
        newPosition = FocusAbsPosNP[0].getValue() + ticks;

    rc = move(newPosition);

    if (!rc)
        return IPS_ALERT;

    FocusRelPosNP[0].setValue(ticks);
    FocusRelPosNP.setState(IPS_BUSY);

    return IPS_BUSY;
}

bool PegasusFocusCube3::move(uint32_t newPosition)
{
    char cmd[PEGASUS_LEN] = {0}, res[PEGASUS_LEN] = {0}, expectedResponse[PEGASUS_LEN] = {0};
    snprintf(expectedResponse, PEGASUS_LEN, "%u", newPosition);
    snprintf(cmd, PEGASUS_LEN, "FM:%d", newPosition);
    if(sendCommand(cmd, res))
    {
        if(!strstr(expectedResponse, res))
        {
            LOGF_ERROR("Error on move [Cmd=%s Res=%s]", cmd, res);
            return false;
        }
    }
    else
    {
        LOGF_ERROR("Error on move [Position=%d]", newPosition);
        return false;
    }
    return true;
}


bool PegasusFocusCube3::sendCommand(const char * cmd, char * res)
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

bool PegasusFocusCube3::setSpeed(uint16_t speed)
{

    char cmd[PEGASUS_LEN] = {0}, res[PEGASUS_LEN] = {0}, expectedResponse[PEGASUS_LEN] = {0};
    snprintf(expectedResponse, PEGASUS_LEN, "%u", speed);
    snprintf(cmd, PEGASUS_LEN, "SP:%d", speed);
    if(sendCommand(expectedResponse, res))
    {
        if(!strstr(cmd, res))
        {
            LOGF_ERROR("Error on set speed [Cmd=%s Res=%s]", cmd, res);
            return false;
        }
    }
    else
    {
        LOGF_ERROR("Error on set speed [Speed=%d]", speed);
        return false;
    }
    return true;
}

std::string  PegasusFocusCube3::getFirmwareVersion()
{
    char cmd[PEGASUS_LEN] = {0}, res[PEGASUS_LEN] = {0};
    snprintf(cmd, PEGASUS_LEN, "FV");
    if(sendCommand(cmd, res))
    {

    }
    else
    {
        LOG_ERROR("Error on get Firmware");
    }
    return res;
}

std::vector<std::string> PegasusFocusCube3::split(const std::string &input, const std::string &regex)
{
    // passing -1 as the submatch index parameter performs splitting
    std::regex re(regex);
    std::sregex_token_iterator
    first{input.begin(), input.end(), re, -1},
          last;
    return {first, last};
}

bool PegasusFocusCube3::AbortFocuser()
{
    char cmd[PEGASUS_LEN] = {0}, res[PEGASUS_LEN] = {0};
    snprintf(cmd, PEGASUS_LEN, "FH");
    if(sendCommand(cmd, res))
    {

    }
    else
    {
        LOG_ERROR("Error on abort");
        return false;
    }


    FocusAbsPosNP.setState(IPS_IDLE);
    FocusRelPosNP.setState(IPS_IDLE);
    FocusAbsPosNP.apply();
    FocusRelPosNP.apply();
    return true;
}

bool PegasusFocusCube3::SyncFocuser(uint32_t ticks)
{
    char cmd[PEGASUS_LEN] = {0}, res[PEGASUS_LEN] = {0}, expectedResponse[PEGASUS_LEN] = {0};
    snprintf(expectedResponse, PEGASUS_LEN, "%u", ticks);
    snprintf(cmd, PEGASUS_LEN, "FN:%ud", ticks);
    if(sendCommand(cmd, res))
    {
        if(!strstr(expectedResponse, res))
        {
            LOGF_ERROR("Error on sync  [Cmd=%s Res=%s]", cmd, res);
            return false;
        }
    }
    else
    {
        LOG_ERROR("Error on sync");
        return false;
    }

    return true;
}

bool PegasusFocusCube3::ReverseFocuser(bool enabled)
{
    char cmd[PEGASUS_LEN] = {0}, res[PEGASUS_LEN] = {0};
    snprintf(cmd, PEGASUS_LEN, "FD:%d", enabled ? 1 : 0);
    if(sendCommand(cmd, res))
    {

    }
    else
    {
        LOG_ERROR("Error on sync");
        return false;
    }

    return true;
}

bool PegasusFocusCube3::SetFocuserBacklash(int32_t steps)
{
    char cmd[PEGASUS_LEN] = {0}, res[PEGASUS_LEN] = {0}, expectedResponse[PEGASUS_LEN] = {0};
    snprintf(expectedResponse, PEGASUS_LEN, "%u", steps);
    snprintf(cmd, PEGASUS_LEN, "BL:%d", steps);
    if(sendCommand(cmd, res))
    {
        if(!strstr(expectedResponse, res))
        {
            LOGF_ERROR("Error on sync  [Cmd=%s Res=%s]", cmd, res);
            return false;
        }
    }
    else
    {
        LOG_ERROR("Error on sync");
        return false;
    }

    return true;
}

void PegasusFocusCube3::TimerHit()
{
    if (!isConnected())
    {
        SetTimer(getCurrentPollingPeriod());
        return;
    }

    bool rc = updateFocusParams();

    if (rc)
    {
        if (FocusAbsPosNP.getState() == IPS_BUSY || FocusRelPosNP.getState() == IPS_BUSY)
        {
            if (isMoving == false)
            {
                FocusAbsPosNP.setState(IPS_OK);
                FocusRelPosNP.setState(IPS_OK);
                FocusAbsPosNP.apply();
                FocusRelPosNP.apply();
                LOG_INFO("Focuser reached requested position.");
            }
        }
    }

    SetTimer(getCurrentPollingPeriod());
}

bool PegasusFocusCube3::updateFocusParams()
{
    char cmd[PEGASUS_LEN] = {0}, res[PEGASUS_LEN] = {0};
    snprintf(cmd, PEGASUS_LEN, "FA");
    if(sendCommand(cmd, res))
    {

    }
    else
    {
        LOGF_ERROR("Error on [Cmd=%s Res=%s]", cmd, res);
        return false;
    }

    std::vector<std::string> result = split(res, ":");

    // Status
    if(result[0] != "FC3")
        return false;


    // #1 Position
    currentPosition = atoi(result[1].c_str());
    if (currentPosition != FocusAbsPosNP[0].getValue())
    {
        FocusAbsPosNP[0].setValue(currentPosition);
        FocusAbsPosNP.apply();
    }

    // #2 Moving Status
    isMoving = (result[2] == "1");


    // #3 Temperature
    TemperatureNP[0].setValue(atof(result[3].c_str()));
    TemperatureNP.setState(IPS_OK);
    TemperatureNP.apply();


    // #4 Reverse Status
    int reverseStatus = atoi(result[4].c_str());
    if (reverseStatus >= 0 && reverseStatus <= 1)
    {
        FocusReverseSP.reset();
        FocusReverseSP[INDI_ENABLED].setState((reverseStatus == 1) ? ISS_ON : ISS_OFF);
        FocusReverseSP[INDI_DISABLED].setState((reverseStatus == 0) ? ISS_ON : ISS_OFF);
        FocusReverseSP.setState(IPS_OK);
        FocusReverseSP.apply();
    }


    // #5 Backlash

    int backlash = atoi(result[5].c_str());
    // If backlash is zero then compensation is disabled
    if (backlash == 0 && FocusBacklashSP[INDI_ENABLED].getState() == ISS_ON)
    {
        LOG_WARN("Backlash value is zero, disabling backlash switch...");

        FocusBacklashSP[INDI_ENABLED].setState(ISS_OFF);
        FocusBacklashSP[INDI_DISABLED].setState(ISS_ON);
        FocusBacklashSP.setState(IPS_IDLE);
        FocusBacklashSP.apply();
    }
    else if (backlash > 0 && (FocusBacklashSP[INDI_DISABLED].getState() == ISS_ON || backlash != FocusBacklashNP[0].getValue()))
    {
        if (backlash != FocusBacklashNP[0].getValue())
        {
            FocusBacklashNP[0].setValue(backlash);
            FocusBacklashNP.setState(IPS_OK);
            FocusBacklashNP.apply();
        }

        if (FocusBacklashSP[INDI_DISABLED].getState() == ISS_ON)
        {
            FocusBacklashSP[INDI_ENABLED].setState(ISS_OFF);
            FocusBacklashSP[INDI_DISABLED].setState(ISS_ON);
            FocusBacklashSP.setState(IPS_IDLE);
            FocusBacklashSP.apply();
        }
    }

    return true;
}
