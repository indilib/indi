/*******************************************************************************
  Copyright(c) 2021 Chrysikos Efstathios. All rights reserved.

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
#include "pegasus_focuscube.h"

#include "indicom.h"
#include "connectionplugins/connectionserial.h"

#include <cmath>
#include <cstring>
#include <memory>

#include <termios.h>
#include <unistd.h>

#define DMFC_TIMEOUT 3
#define FOCUS_SETTINGS_TAB "Settings"
#define TEMPERATURE_THRESHOLD 0.1

static std::unique_ptr<PegasusFocusCube> focusCube(new PegasusFocusCube());

PegasusFocusCube::PegasusFocusCube()
{
    // Can move in Absolute & Relative motions, can AbortFocuser motion.
    FI::SetCapability(FOCUSER_CAN_ABS_MOVE |
                      FOCUSER_CAN_REL_MOVE |
                      FOCUSER_CAN_ABORT |
                      FOCUSER_CAN_REVERSE |
                      FOCUSER_CAN_SYNC |
                      FOCUSER_HAS_BACKLASH);
}

bool PegasusFocusCube::initProperties()
{
    INDI::Focuser::initProperties();

    // Focuser temperature
    IUFillNumber(&TemperatureN[0], "TEMPERATURE", "Celsius", "%6.2f", -50, 70., 0., 0.);
    IUFillNumberVector(&TemperatureNP, TemperatureN, 1, getDeviceName(), "FOCUS_TEMPERATURE", "Temperature",
                       MAIN_CONTROL_TAB, IP_RO, 0, IPS_IDLE);

    // Max Speed
    IUFillNumber(&MaxSpeedN[0], "Value", "", "%6.2f", 100, 1000., 100., 400.);
    IUFillNumberVector(&MaxSpeedNP, MaxSpeedN, 1, getDeviceName(), "MaxSpeed", "", FOCUS_SETTINGS_TAB, IP_RW, 0, IPS_IDLE);

    // Encoders
    IUFillSwitch(&EncoderS[ENCODERS_ON], "On", "", ISS_ON);
    IUFillSwitch(&EncoderS[ENCODERS_OFF], "Off", "", ISS_OFF);
    IUFillSwitchVector(&EncoderSP, EncoderS, 2, getDeviceName(), "Encoders", "", FOCUS_SETTINGS_TAB, IP_RW, ISR_1OFMANY, 0,
                       IPS_IDLE);

    // LED
    IUFillSwitch(&LEDS[LED_OFF], "Off", "", ISS_ON);
    IUFillSwitch(&LEDS[LED_ON], "On", "", ISS_OFF);
    IUFillSwitchVector(&LEDSP, LEDS, 2, getDeviceName(), "LED", "", FOCUS_SETTINGS_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    // Firmware Version
    IUFillText(&FirmwareVersionT[0], "Version", "Version", "");
    IUFillTextVector(&FirmwareVersionTP, FirmwareVersionT, 1, getDeviceName(), "Firmware", "Firmware", MAIN_CONTROL_TAB, IP_RO,
                     0, IPS_IDLE);

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
    FocusBacklashNP[0].setMax(10000);
    FocusBacklashNP[0].setValue(1);
    FocusBacklashNP[0].setStep(1);

    FocusMaxPosNP[0].setMax(1317500);
    FocusMaxPosNP[0].setValue(1317500);
    FocusAbsPosNP[0].setMax(1317500);

    //LED Default ON
    LEDS[LED_ON].s = ISS_ON;
    LEDS[LED_OFF].s = ISS_OFF;

    addDebugControl();
    setDefaultPollingPeriod(200);
    serialConnection->setDefaultBaudRate(Connection::Serial::B_19200);

    return true;
}

bool PegasusFocusCube::updateProperties()
{
    INDI::Focuser::updateProperties();

    if (isConnected())
    {
        defineProperty(&MaxSpeedNP);
        defineProperty(&TemperatureNP);
        defineProperty(&EncoderSP);
        defineProperty(&LEDSP);
        defineProperty(&FirmwareVersionTP);
    }
    else
    {
        deleteProperty(MaxSpeedNP.name);
        deleteProperty(TemperatureNP.name);
        deleteProperty(EncoderSP.name);
        deleteProperty(LEDSP.name);
        deleteProperty(FirmwareVersionTP.name);
    }

    return true;
}

bool PegasusFocusCube::Handshake()
{
    if (ack())
    {
        LOGF_INFO("%s is online. Getting focus parameters...", this->getDeviceName());
        return true;
    }

    LOGF_INFO(
        "Error retrieving data from %s, please ensure device is powered and the port is correct.", this->getDeviceName());
    return false;
}

const char *PegasusFocusCube::getDefaultName()
{
    return "Pegasus FocusCube";
}

bool PegasusFocusCube::ack()
{
    int nbytes_written = 0, nbytes_read = 0, rc = -1;
    char errstr[MAXRBUF];
    char cmd[2] = {0};
    char res[16] = {0};

    cmd[0] = '#';
    cmd[1] = 0xA;

    LOGF_DEBUG("CMD <%#02X>", cmd[0]);

    tcflush(PortFD, TCIOFLUSH);

    if ((rc = tty_write(PortFD, cmd, 2, &nbytes_written)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("Ack error: %s.", errstr);
        return false;
    }

    if ((rc = tty_read_section(PortFD, res, 0xA, DMFC_TIMEOUT, &nbytes_read)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("Ack error: %s.", errstr);
        return false;
    }

    // Get rid of 0xA
    res[nbytes_read - 1] = 0;


    if( res[nbytes_read - 2] == '\r') res[nbytes_read - 2] = 0;

    LOGF_DEBUG("RES <%s>", res);

    tcflush(PortFD, TCIOFLUSH);

    if(strstr(res, "OK_FC") != nullptr)
        return true;

    return false;
}


bool PegasusFocusCube::SyncFocuser(uint32_t ticks)
{
    int nbytes_written = 0, rc = -1;
    char cmd[16] = {0};

    snprintf(cmd, 16, "W:%ud", ticks);
    cmd[strlen(cmd)] = 0xA;

    LOGF_DEBUG("CMD <%s>", cmd);

    // Set Position
    if ((rc = tty_write(PortFD, cmd, strlen(cmd), &nbytes_written)) != TTY_OK)
    {
        char errstr[MAXRBUF];
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("sync error: %s.", errstr);
        return false;
    }

    this->ignoreResponse();

    return true;
}

bool PegasusFocusCube::move(uint32_t newPosition)
{
    int nbytes_written = 0, rc = -1;
    char cmd[16] = {0};

    snprintf(cmd, 16, "M:%ud", newPosition);
    cmd[strlen(cmd)] = 0xA;

    LOGF_DEBUG("CMD <%s>", cmd);

    // Set Position
    if ((rc = tty_write(PortFD, cmd, strlen(cmd), &nbytes_written)) != TTY_OK)
    {
        char errstr[MAXRBUF];
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("move error: %s.", errstr);
        return false;
    }

    this->ignoreResponse();

    return true;
}

bool PegasusFocusCube::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {

        // Encoders
        if (!strcmp(name, EncoderSP.name))
        {
            IUUpdateSwitch(&EncoderSP, states, names, n);
            bool rc = setEncodersEnabled(EncoderS[ENCODERS_ON].s == ISS_ON);
            EncoderSP.s = rc ? IPS_OK : IPS_ALERT;
            IDSetSwitch(&EncoderSP, nullptr);
            return true;
        }

        // LED
        if (!strcmp(name, LEDSP.name))
        {
            IUUpdateSwitch(&LEDSP, states, names, n);
            bool rc = setLedEnabled(LEDS[LED_ON].s == ISS_ON);
            LEDSP.s = rc ? IPS_OK : IPS_ALERT;
            IDSetSwitch(&LEDSP, nullptr);
            return true;
        }
    }
    return INDI::Focuser::ISNewSwitch(dev, name, states, names, n);
}

bool PegasusFocusCube::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        // MaxSpeed
        if (strcmp(name, MaxSpeedNP.name) == 0)
        {
            IUUpdateNumber(&MaxSpeedNP, values, names, n);
            bool rc = setMaxSpeed(MaxSpeedN[0].value);
            MaxSpeedNP.s = rc ? IPS_OK : IPS_ALERT;
            IDSetNumber(&MaxSpeedNP, nullptr);
            return true;
        }

    }

    return INDI::Focuser::ISNewNumber(dev, name, values, names, n);
}

void PegasusFocusCube::ignoreResponse()
{
    int nbytes_read = 0;
    char res[64];
    tty_read_section(PortFD, res, 0xA, DMFC_TIMEOUT, &nbytes_read);
}

bool PegasusFocusCube::updateFocusParams()
{
    int nbytes_written = 0, nbytes_read = 0, rc = -1;
    char errstr[MAXRBUF];
    char cmd[3] = {0};
    char res[64];
    cmd[0] = 'A';
    cmd[1] = 0xA;

    LOGF_DEBUG("CMD <%#02X>", cmd[0]);

    tcflush(PortFD, TCIOFLUSH);

    if ((rc = tty_write(PortFD, cmd, 2, &nbytes_written)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("GetFocusParams error: %s.", errstr);
        return false;
    }


    if ((rc = tty_read_section(PortFD, res, 0xA, DMFC_TIMEOUT, &nbytes_read)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("GetFocusParams error: %s.", errstr);
        return false;
    }

    res[nbytes_read - 1] = 0;

    // Check for '\r' at end of string and replace with nullptr (DMFC firmware version 2.8)
    if( res[nbytes_read - 2] == '\r') res[nbytes_read - 2] = 0;

    LOGF_DEBUG("RES <%s>", res);

    tcflush(PortFD, TCIOFLUSH);

    char *token = std::strtok(res, ":");

    // #1 Status
    if (token == nullptr || (strstr(token, "OK_FC") == nullptr))
    {
        LOGF_ERROR("Invalid status response. %s", res);
        return false;
    }

    // #2 Version
    token = std::strtok(nullptr, ":");

    if (token == nullptr)
    {
        LOG_ERROR("Invalid version response.");
        return false;
    }

    if (FirmwareVersionT[0].text == nullptr || strcmp(FirmwareVersionT[0].text, token))
    {
        IUSaveText(&FirmwareVersionT[0], token);
        FirmwareVersionTP.s = IPS_OK;
        IDSetText(&FirmwareVersionTP, nullptr);
    }

    // #3 Motor Type
    token = std::strtok(nullptr, ":");

    // #4 Temperature
    token = std::strtok(nullptr, ":");

    if (token == nullptr)
    {
        LOG_ERROR("Invalid temperature response.");
        return false;
    }

    double temperature = atof(token);
    if (temperature == -127)
    {
        TemperatureNP.s = IPS_ALERT;
        IDSetNumber(&TemperatureNP, nullptr);
    }
    else
    {
        if (fabs(temperature - TemperatureN[0].value) > TEMPERATURE_THRESHOLD)
        {
            TemperatureN[0].value = temperature;
            TemperatureNP.s = IPS_OK;
            IDSetNumber(&TemperatureNP, nullptr);
        }
    }

    // #5 Position
    token = std::strtok(nullptr, ":");

    if (token == nullptr)
    {
        LOG_ERROR("Invalid position response.");
        return false;
    }

    currentPosition = atoi(token);
    if (currentPosition != FocusAbsPosNP[0].getValue())
    {
        FocusAbsPosNP[0].setValue(currentPosition);
        FocusAbsPosNP.apply();
    }

    // #6 Moving Status
    token = std::strtok(nullptr, ":");

    if (token == nullptr)
    {
        LOG_ERROR("Invalid moving status response.");
        return false;
    }

    isMoving = (token[0] == '1');

    // #7 LED Status
    token = std::strtok(nullptr, ":");

    if (token == nullptr)
    {
        LOG_ERROR("Invalid LED response.");
        return false;
    }

    int ledStatus = atoi(token);
    if (ledStatus >= 0 && ledStatus <= 1)
    {
        IUResetSwitch(&LEDSP);
        LEDS[ledStatus].s = ISS_ON;
        LEDSP.s = IPS_OK;
        IDSetSwitch(&LEDSP, nullptr);
    }

    // #8 Reverse Status
    token = std::strtok(nullptr, ":");

    if (token == nullptr)
    {
        LOG_ERROR("Invalid reverse response.");
        return false;
    }

    int reverseStatus = atoi(token);
    if (reverseStatus >= 0 && reverseStatus <= 1)
    {
        FocusReverseSP.reset();
        FocusReverseSP[INDI_ENABLED].setState((reverseStatus == 1) ? ISS_ON : ISS_OFF);
        FocusReverseSP[INDI_DISABLED].setState((reverseStatus == 0) ? ISS_ON : ISS_OFF);
        FocusReverseSP.setState(IPS_OK);
        FocusReverseSP.apply();
    }

    // #9 Encoder status
    token = std::strtok(nullptr, ":");

    if (token == nullptr)
    {
        LOG_ERROR("Invalid encoder response.");
        return false;
    }

    int encoderStatus = atoi(token);
    if (encoderStatus >= 0 && encoderStatus <= 1)
    {
        IUResetSwitch(&EncoderSP);
        EncoderS[encoderStatus].s = ISS_ON;
        EncoderSP.s = IPS_OK;
        IDSetSwitch(&EncoderSP, nullptr);
    }

    // #10 Backlash
    token = std::strtok(nullptr, ":");

    if (token == nullptr)
    {
        LOG_ERROR("Invalid encoder response.");
        return false;
    }

    int backlash = atoi(token);
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

bool PegasusFocusCube::setMaxSpeed(uint16_t speed)
{
    int nbytes_written = 0, rc = -1;
    char cmd[16] = {0};

    snprintf(cmd, 16, "S:%d", speed);
    cmd[strlen(cmd)] = 0xA;

    LOGF_DEBUG("CMD <%s>", cmd);

    tcflush(PortFD, TCIOFLUSH);

    // Set Speed
    if ((rc = tty_write(PortFD, cmd, strlen(cmd), &nbytes_written)) != TTY_OK)
    {
        char errstr[MAXRBUF];
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("setMaxSpeed error: %s.", errstr);
        return false;
    }

    this->ignoreResponse();
    return true;
}

bool PegasusFocusCube::ReverseFocuser(bool enabled)
{
    int nbytes_written = 0, rc = -1;
    char cmd[16] = {0};

    snprintf(cmd, 16, "N:%d", enabled ? 1 : 0);
    cmd[strlen(cmd)] = 0xA;

    LOGF_DEBUG("CMD <%s>", cmd);

    tcflush(PortFD, TCIOFLUSH);

    // Reverse
    if ((rc = tty_write(PortFD, cmd, strlen(cmd), &nbytes_written)) != TTY_OK)
    {
        char errstr[MAXRBUF];
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("Reverse error: %s.", errstr);
        return false;
    }

    this->ignoreResponse();

    return true;
}

bool PegasusFocusCube::setLedEnabled(bool enable)
{
    int nbytes_written = 0, rc = -1;
    char cmd[16] = {0};

    snprintf(cmd, 16, "L:%d", enable ? 2 : 1);
    cmd[strlen(cmd)] = 0xA;

    LOGF_DEBUG("CMD <%s>", cmd);

    tcflush(PortFD, TCIOFLUSH);

    // Led
    if ((rc = tty_write(PortFD, cmd, strlen(cmd), &nbytes_written)) != TTY_OK)
    {
        char errstr[MAXRBUF];
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("Led error: %s.", errstr);
        return false;
    }

    this->ignoreResponse();
    return true;
}

bool PegasusFocusCube::setEncodersEnabled(bool enable)
{
    int nbytes_written = 0, rc = -1;
    char cmd[16] = {0};

    snprintf(cmd, 16, "E:%d", enable ? 0 : 1);
    cmd[strlen(cmd)] = 0xA;

    LOGF_DEBUG("CMD <%s>", cmd);

    tcflush(PortFD, TCIOFLUSH);

    // Encoders
    if ((rc = tty_write(PortFD, cmd, strlen(cmd), &nbytes_written)) != TTY_OK)
    {
        char errstr[MAXRBUF];
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("Encoder error: %s.", errstr);
        return false;
    }

    this->ignoreResponse();
    return true;
}

bool PegasusFocusCube::SetFocuserBacklash(int32_t steps)
{
    int nbytes_written = 0, rc = -1;
    char cmd[16] = {0};

    snprintf(cmd, 16, "C:%d", steps);
    cmd[strlen(cmd)] = 0xA;

    LOGF_DEBUG("CMD <%s>", cmd);

    tcflush(PortFD, TCIOFLUSH);

    // Backlash
    if ((rc = tty_write(PortFD, cmd, strlen(cmd), &nbytes_written)) != TTY_OK)
    {
        char errstr[MAXRBUF];
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("Backlash error: %s.", errstr);
        return false;
    }

    this->ignoreResponse();
    return true;
}

bool PegasusFocusCube::SetFocuserBacklashEnabled(bool enabled)
{
    if (!enabled)
        return SetFocuserBacklash(0);

    return SetFocuserBacklash(FocusBacklashNP[0].getValue() > 0 ? FocusBacklashNP[0].getValue() : 1);
}


IPState PegasusFocusCube::MoveAbsFocuser(uint32_t targetTicks)
{
    targetPosition = targetTicks;

    bool rc = move(targetPosition);

    if (!rc)
        return IPS_ALERT;

    FocusAbsPosNP.setState(IPS_BUSY);

    return IPS_BUSY;
}

IPState PegasusFocusCube::MoveRelFocuser(FocusDirection dir, uint32_t ticks)
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

void PegasusFocusCube::TimerHit()
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

bool PegasusFocusCube::AbortFocuser()
{
    int nbytes_written;
    char cmd[2] = { 'H', 0xA };

    if (tty_write(PortFD, cmd, 2, &nbytes_written) == TTY_OK)
    {
        FocusAbsPosNP.setState(IPS_IDLE);
        FocusRelPosNP.setState(IPS_IDLE);
        FocusAbsPosNP.apply();
        FocusRelPosNP.apply();
        this->ignoreResponse();
        return true;
    }
    else
        return false;
}

bool PegasusFocusCube::saveConfigItems(FILE *fp)
{
    INDI::Focuser::saveConfigItems(fp);
    IUSaveConfigSwitch(fp, &EncoderSP);
    IUSaveConfigNumber(fp, &MaxSpeedNP);
    IUSaveConfigSwitch(fp, &LEDSP);

    return true;
}
