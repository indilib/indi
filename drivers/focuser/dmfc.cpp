/*
    Pegasus DMFC Focuser
    Copyright (C) 2017 Jasem Mutlaq (mutlaqja@ikarustech.com)

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

#include "dmfc.h"

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

static std::unique_ptr<DMFC> dmfc(new DMFC());

DMFC::DMFC()
{
    setVersion(1, 1);

    // Can move in Absolute & Relative motions, can AbortFocuser motion.
    FI::SetCapability(FOCUSER_CAN_ABS_MOVE |
                      FOCUSER_CAN_REL_MOVE |
                      FOCUSER_CAN_ABORT |
                      FOCUSER_CAN_REVERSE |
                      FOCUSER_CAN_SYNC |
                      FOCUSER_HAS_BACKLASH);
}

bool DMFC::initProperties()
{
    INDI::Focuser::initProperties();

    // Focuser temperature
    TemperatureNP[0].fill("TEMPERATURE", "Celsius", "%6.2f", -50, 70., 0., 0.);
    TemperatureNP.fill(getDeviceName(), "FOCUS_TEMPERATURE", "Temperature",
                       MAIN_CONTROL_TAB, IP_RO, 0, IPS_IDLE);

    // Max Speed
    MaxSpeedNP[0].fill("Value", "", "%6.2f", 100, 1000., 100., 400.);
    MaxSpeedNP.fill(getDeviceName(), "MaxSpeed", "", FOCUS_SETTINGS_TAB, IP_RW, 0, IPS_IDLE);

    // Encoders
    EncoderSP[ENCODERS_ON].fill("On", "", ISS_ON);
    EncoderSP[ENCODERS_OFF].fill("Off", "", ISS_OFF);
    EncoderSP.fill(getDeviceName(), "Encoders", "", FOCUS_SETTINGS_TAB, IP_RW, ISR_1OFMANY, 0,
                       IPS_IDLE);

    // Motor Modes
    MotorTypeSP[MOTOR_DC].fill("DC", "DC", ISS_OFF);
    MotorTypeSP[MOTOR_STEPPER].fill("Stepper", "Stepper", ISS_ON);
    MotorTypeSP.fill(getDeviceName(), "Motor Type", "", MAIN_CONTROL_TAB, IP_RW, ISR_1OFMANY,
                       0, IPS_IDLE);

    // LED
    LEDSP[LED_OFF].fill("Off", "", ISS_ON);
    LEDSP[LED_ON].fill("On", "", ISS_OFF);
    LEDSP.fill(getDeviceName(), "LED", "", FOCUS_SETTINGS_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    // Firmware Version
    FirmwareVersionTP[0].fill("Version", "Version", "");
    FirmwareVersionTP.fill(getDeviceName(), "Firmware", "Firmware", MAIN_CONTROL_TAB, IP_RO,
                     0, IPS_IDLE);

    // Relative and absolute movement
    FocusRelPosNP[0].setMin(0.);
    FocusRelPosNP[0].setMax(50000.);
    FocusRelPosNP[0].setValue(0);
    FocusRelPosNP[0].setStep(1000);

    FocusAbsPosNP[0].setMin(0.);
    FocusAbsPosNP[0].setMax(100000);
    FocusAbsPosNP[0].setValue(0);
    FocusAbsPosNP[0].setStep(1000);

    // Backlash compensation
    FocusBacklashNP[0].setMin(1); // 0 is off.
    FocusBacklashNP[0].setMax(1000);
    FocusBacklashNP[0].setValue(1);
    FocusBacklashNP[0].setStep(1);

    //LED Default ON
    LEDSP[LED_ON].setState(ISS_ON);
    LEDSP[LED_OFF].setState(ISS_OFF);

    addDebugControl();
    setDefaultPollingPeriod(200);
    serialConnection->setDefaultBaudRate(Connection::Serial::B_19200);

    return true;
}

void DMFC::ISGetProperties(const char *dev)
{
    INDI::Focuser::ISGetProperties(dev);
    int configSwitch = MOTOR_STEPPER;

    // Try to read config value for motor type
    // If relative, then update the capability of the focuser as such.
    if (IUGetConfigOnSwitchIndex(getDeviceName(), MotorTypeSP.getName(), &configSwitch) == 0)
    {
        // We are only relative focuser now.
        if (configSwitch == MOTOR_DC)
            FI::SetCapability(GetCapability() & ~(FOCUSER_CAN_ABS_MOVE | FOCUSER_CAN_SYNC));
        else
            FI::SetCapability(GetCapability() | (FOCUSER_CAN_ABS_MOVE | FOCUSER_CAN_SYNC));

        MotorTypeSP.setState(IPS_OK);
    }

    MotorTypeSP.reset();
    MotorTypeSP[configSwitch].setState(ISS_ON);
    defineProperty(MotorTypeSP);
}

bool DMFC::updateProperties()
{
    INDI::Focuser::updateProperties();

    if (isConnected())
    {
        defineProperty(TemperatureNP);
        defineProperty(EncoderSP);
        defineProperty(MaxSpeedNP);
        defineProperty(LEDSP);
        defineProperty(FirmwareVersionTP);
    }
    else
    {
        deleteProperty(TemperatureNP);
        deleteProperty(EncoderSP);
        deleteProperty(MaxSpeedNP);
        deleteProperty(LEDSP);
        deleteProperty(FirmwareVersionTP);
    }

    return true;
}

bool DMFC::Handshake()
{
    if (ack())
    {
        LOGF_INFO("%s is online. Getting focus parameters...", this->getDeviceName());

        // Set motor type on startup only.
        setMotorType((MotorTypeSP[MOTOR_DC].getState() == ISS_ON) ? MOTOR_DC : MOTOR_STEPPER);

        return true;
    }

    LOGF_INFO(
        "Error retrieving data from %s, please ensure device is powered and the port is correct.", this->getDeviceName());
    return false;
}


const char *DMFC::getDefaultName()
{
    return "Pegasus DMFC";
}

bool DMFC::ack()
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

    if((strstr(res, "OK_DMFCN") != nullptr) || (strstr(res, "OK_SMFC") != nullptr) || (strstr(res, "OK_PRDG") != nullptr))
        return true;

    return false;
}


bool DMFC::SyncFocuser(uint32_t ticks)
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

bool DMFC::moveAbsolute(uint32_t newPosition)
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
        LOGF_ERROR("Absolute move error: %s.", errstr);
        return false;
    }

    this->ignoreResponse();

    return true;
}

bool DMFC::moveRelative(int relativePosition)
{
    int nbytes_written = 0, rc = -1;
    char cmd[16] = {0};

    snprintf(cmd, 16, "G:%d", relativePosition);
    cmd[strlen(cmd)] = 0xA;

    LOGF_DEBUG("CMD <%s>", cmd);

    // Set Relative Position
    if ((rc = tty_write(PortFD, cmd, strlen(cmd), &nbytes_written)) != TTY_OK)
    {
        char errstr[MAXRBUF];
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("Relative move error: %s.", errstr);
        return false;
    }

    this->ignoreResponse();

    return true;
}

bool DMFC::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        // Motor Type
        if (MotorTypeSP.isNameMatch(name))
        {
            MotorTypeSP.update(states, names, n);
            MotorTypeSP.setState(IPS_OK);
            saveConfig(true, MotorTypeSP.getName());

            // If we're not connected, let's then set the capability now.
            if (!isConnected())
            {
                // We are only relative focuser now.
                if (MotorTypeSP[MOTOR_DC].getState() == ISS_ON)
                    FI::SetCapability(GetCapability() & ~(FOCUSER_CAN_ABS_MOVE | FOCUSER_CAN_SYNC));
                else
                    FI::SetCapability(GetCapability() | (FOCUSER_CAN_ABS_MOVE | FOCUSER_CAN_SYNC));
            }
            else
                LOG_INFO("Motor type changed. Please restart driver for this change to take effect.");

            MotorTypeSP.apply( );
            return true;
        }

        // Encoders
        if (EncoderSP.isNameMatch(name))
        {
            EncoderSP.update(states, names, n);
            bool rc = setEncodersEnabled(EncoderSP[ENCODERS_ON].getState() == ISS_ON);
            EncoderSP.setState(rc ? IPS_OK : IPS_ALERT);
            EncoderSP.apply();
            return true;
        }

        // LED
        if (LEDSP.isNameMatch(name))
        {
            LEDSP.update(states, names, n);
            bool rc = setLedEnabled(LEDSP[LED_ON].getState() == ISS_ON);
            LEDSP.setState(rc ? IPS_OK : IPS_ALERT);
            LEDSP.apply();
            return true;
        }
    }
    return INDI::Focuser::ISNewSwitch(dev, name, states, names, n);
}

bool DMFC::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        // MaxSpeed
        if (MaxSpeedNP.isNameMatch(name))
        {
            MaxSpeedNP.update(values, names, n);
            bool rc = setMaxSpeed(MaxSpeedNP[0].getValue());
            MaxSpeedNP.setState(rc ? IPS_OK : IPS_ALERT);
            MaxSpeedNP.apply();
            return true;
        }
    }

    return INDI::Focuser::ISNewNumber(dev, name, values, names, n);
}

void DMFC::ignoreResponse()
{
    int nbytes_read = 0;
    char res[64];
    tty_read_section(PortFD, res, 0xA, DMFC_TIMEOUT, &nbytes_read);
}

bool DMFC::updateFocusParams()
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
    if (token == nullptr || ((strstr(token, "OK_DMFCN") == nullptr) && (strstr(token, "OK_SMFC") == nullptr)
                             && (strstr(token, "OK_PRDG") == nullptr)))
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

    if (FirmwareVersionTP[0].isEmpty() || FirmwareVersionTP[0].isNameMatch(token))
    {
        FirmwareVersionTP[0].setText(token);
        FirmwareVersionTP.setState(IPS_OK);
        FirmwareVersionTP.apply();
    }

    // #3 Motor Type
    token = std::strtok(nullptr, ":");

    if (token == nullptr)
    {
        LOG_ERROR("Invalid motor mode response.");
        return false;
    }

    int motorType = atoi(token);


    if (motorType >= 0 && motorType <= 1)
    {
        //1 stepper
        //0 dc
        MotorTypeSP.reset();
        MotorTypeSP[MOTOR_DC].setState((motorType == 0) ? ISS_ON : ISS_OFF);
        MotorTypeSP[MOTOR_STEPPER].s = (motorType == 1) ? ISS_ON : ISS_OFF;
        MotorTypeSP.setState(IPS_OK);
        MotorTypeSP.apply();
    }

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
        TemperatureNP.setState(IPS_ALERT);
        TemperatureNP.apply();
    }
    else
    {
        if (fabs(temperature - TemperatureNP[0].getValue()) > TEMPERATURE_THRESHOLD)
        {
            TemperatureNP[0].setValue(temperature);
            TemperatureNP.setState(IPS_OK);
            TemperatureNP.apply();
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
        LEDSP.reset();
        LEDSP[ledStatus].setState(ISS_ON);
        LEDSP.setState(IPS_OK);
        LEDSP.apply();
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
        EncoderSP.reset();
        EncoderSP[encoderStatus].setState(ISS_ON);
        EncoderSP.setState(IPS_OK);
        EncoderSP.apply();
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

bool DMFC::setMaxSpeed(uint16_t speed)
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

bool DMFC::ReverseFocuser(bool enabled)
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

bool DMFC::setLedEnabled(bool enable)
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

bool DMFC::setEncodersEnabled(bool enable)
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

bool DMFC::SetFocuserBacklash(int32_t steps)
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

bool DMFC::SetFocuserBacklashEnabled(bool enabled)
{
    if (!enabled)
        return SetFocuserBacklash(0);

    return SetFocuserBacklash(FocusBacklashNP[0].getValue() > 0 ? FocusBacklashNP[0].getValue() : 1);
}

bool DMFC::setMotorType(uint8_t type)
{

    int nbytes_written = 0, rc = -1;
    char cmd[16] = {0};

    //commands:
    //2 dc
    //1 stepper
    snprintf(cmd, 16, "R:%d", (type == MOTOR_STEPPER) ? 1 : 2);
    cmd[strlen(cmd)] = 0xA;

    LOGF_DEBUG("CMD <%s>", cmd);

    tcflush(PortFD, TCIOFLUSH);

    // Motor Type
    if ((rc = tty_write(PortFD, cmd, strlen(cmd), &nbytes_written)) != TTY_OK)
    {
        char errstr[MAXRBUF];
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("Motor type error: %s.", errstr);
        return false;
    }

    this->ignoreResponse();

    return true;
}

IPState DMFC::MoveAbsFocuser(uint32_t targetTicks)
{
    targetPosition = targetTicks;

    bool rc = moveAbsolute(targetPosition);

    if (!rc)
        return IPS_ALERT;

    FocusAbsPosNP.setState(IPS_BUSY);

    return IPS_BUSY;
}

IPState DMFC::MoveRelFocuser(FocusDirection dir, uint32_t ticks)
{
    int relativePosition = ticks * ((dir == FOCUS_INWARD) ? -1 : 1);
    return moveRelative(relativePosition) ? IPS_BUSY : IPS_ALERT;
}

void DMFC::TimerHit()
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

bool DMFC::AbortFocuser()
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

bool DMFC::saveConfigItems(FILE *fp)
{
    INDI::Focuser::saveConfigItems(fp);
    MotorTypeSP.save(fp);
    EncoderSP.save(fp);
    MaxSpeedNP.save(fp);
    LEDSP.save(fp);

    return true;
}
