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

#include <cmath>
#include <cstring>
#include <memory>

#include <termios.h>
#include <unistd.h>

#define DMFC_TIMEOUT 3
#define FOCUS_SETTINGS_TAB "Settings"

#define POLLMS 500

std::unique_ptr<DMFC> dmfc(new DMFC());

void ISGetProperties(const char *dev)
{
    dmfc->ISGetProperties(dev);
}

void ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    dmfc->ISNewSwitch(dev, name, states, names, n);
}

void ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n)
{
    dmfc->ISNewText(dev, name, texts, names, n);
}

void ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    dmfc->ISNewNumber(dev, name, values, names, n);
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
    dmfc->ISSnoopDevice(root);
}

DMFC::DMFC()
{
    // Can move in Absolute & Relative motions, can AbortFocuser motion, and has variable speed.
    // FIXME variable speed should probably be disabled since there is no open loop control?
    SetFocuserCapability(FOCUSER_CAN_ABS_MOVE | FOCUSER_CAN_REL_MOVE | FOCUSER_CAN_ABORT | FOCUSER_HAS_VARIABLE_SPEED);
}

bool DMFC::initProperties()
{
    INDI::Focuser::initProperties();

    // Sync
    IUFillNumber(&SyncN[0], "FOCUS_SYNC_OFFSET", "Offset", "%6.0f", 0, 60000., 0., 0.);
    IUFillNumberVector(&SyncNP, SyncN, 1, getDeviceName(), "FOCUS_SYNC", "Sync", MAIN_CONTROL_TAB, IP_RW, 0, IPS_IDLE);

    // Reverse direction
    IUFillSwitch(&ReverseS[0], "Enable", "", ISS_OFF);
    IUFillSwitch(&ReverseS[1], "Disable", "", ISS_ON);
    IUFillSwitchVector(&ReverseSP, ReverseS, 2, getDeviceName(), "Reverse", "", FOCUS_SETTINGS_TAB, IP_RW, ISR_1OFMANY,
                       0, IPS_IDLE);

    // Focuser temperature
    IUFillNumber(&TemperatureN[0], "TEMPERATURE", "Celsius", "%6.2f", -50, 70., 0., 0.);
    IUFillNumberVector(&TemperatureNP, TemperatureN, 1, getDeviceName(), "FOCUS_TEMPERATURE", "Temperature",
                       MAIN_CONTROL_TAB, IP_RO, 0, IPS_IDLE);

    // Enable/Disable backlash
    IUFillSwitch(&BacklashCompensationS[0], "Enable", "", ISS_OFF);
    IUFillSwitch(&BacklashCompensationS[1], "Disable", "", ISS_ON);
    IUFillSwitchVector(&BacklashCompensationSP, BacklashCompensationS, 2, getDeviceName(), "Backlash Compensation", "",
                       FOCUS_SETTINGS_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    // Backlash Value
    IUFillNumber(&BacklashN[0], "Value", "", "%.f", 0, 99, 5., 0.);
    IUFillNumberVector(&BacklashNP, BacklashN, 1, getDeviceName(), "Backlash", "", FOCUS_SETTINGS_TAB, IP_RW, 0,
                       IPS_IDLE);

    // Encoders
    IUFillSwitch(&EncoderS[0], "Enable", "", ISS_ON);
    IUFillSwitch(&EncoderS[1], "Disable", "", ISS_OFF);
    IUFillSwitchVector(&EncoderSP, EncoderS, 2, getDeviceName(), "Encoders", "", FOCUS_SETTINGS_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    // Motor Modes
    IUFillSwitch(&MotorModeS[MOTOR_STEPPER], "Stepper", "", ISS_ON);
    IUFillSwitch(&MotorModeS[MOTOR_DC], "DC", "", ISS_OFF);
    IUFillSwitchVector(&MotorModeSP, MotorModeS, 2, getDeviceName(), "Motor Modes", "", FOCUS_SETTINGS_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    // LED
    IUFillSwitch(&LEDS[0], "On", "", ISS_ON);
    IUFillSwitch(&LEDS[1], "Off", "", ISS_OFF);
    IUFillSwitchVector(&LEDSP, LEDS, 2, getDeviceName(), "LED", "", FOCUS_SETTINGS_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    // Relative and absolute movement
    FocusRelPosN[0].min   = 0.;
    FocusRelPosN[0].max   = 50000.;
    FocusRelPosN[0].value = 0;
    FocusRelPosN[0].step  = 1000;

    FocusAbsPosN[0].min   = 0.;
    FocusAbsPosN[0].max   = 100000.;
    FocusAbsPosN[0].value = 0;
    FocusAbsPosN[0].step  = 1000;

    FocusSpeedN[0].min   = 1;
    FocusSpeedN[0].max   = 400;
    FocusSpeedN[0].value = 1;

    addDebugControl();

    updatePeriodMS = POLLMS;

    return true;
}

bool DMFC::updateProperties()
{
    INDI::Focuser::updateProperties();

    if (isConnected())
    {
        defineNumber(&TemperatureNP);        
        defineNumber(&SyncNP);

        GetFocusParams();

        DEBUG(INDI::Logger::DBG_SESSION, "DMFC paramaters updated, focuser ready for use.");
    }
    else
    {
        deleteProperty(TemperatureNP.name);
        deleteProperty(SyncNP.name);
    }

    return true;
}

bool DMFC::Handshake()
{
    if (Ack())
    {
        DEBUG(INDI::Logger::DBG_SESSION, "DMFC is online. Getting focus parameters...");
        return true;
    }

    DEBUG(INDI::Logger::DBG_SESSION,
          "Error retreiving data from DMFC, please ensure DMFC controller is powered and the port is correct.");
    return false;
}

const char *DMFC::getDefaultName()
{
    return "Pegasus DMFC";
}

bool DMFC::Ack()
{
    int nbytes_written = 0, nbytes_read = 0, rc = -1;
    char errstr[MAXRBUF];
    char resp[5]={0};
    short pos = -1;

    tcflush(PortFD, TCIOFLUSH);

    if ((rc = tty_write(PortFD, ":GP#", 4, &nbytes_written)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        DEBUGF(INDI::Logger::DBG_ERROR, "updatePostion error: %s.", errstr);
        return false;
    }

    if ((rc = tty_read(PortFD, resp, 5, 2, &nbytes_read)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        DEBUGF(INDI::Logger::DBG_ERROR, "updatePostion error: %s.", errstr);
        return false;
    }

    tcflush(PortFD, TCIOFLUSH);

    rc = sscanf(resp, "%hX#", &pos);

    return rc > 0;
}

bool DMFC::updateTemperature()
{
    int nbytes_written = 0, nbytes_read = 0, rc = -1;
    char errstr[MAXRBUF];
    char resp[5]={0};
    unsigned int temp;

    tcflush(PortFD, TCIOFLUSH);

    tty_write(PortFD, ":C#", 3, &nbytes_written);

    if ((rc = tty_write(PortFD, ":GT#", 4, &nbytes_written)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        DEBUGF(INDI::Logger::DBG_ERROR, "updateTemperature error: %s.", errstr);
        return false;
    }

    if ((rc = tty_read(PortFD, resp, 5, DMFC_TIMEOUT, &nbytes_read)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        DEBUGF(INDI::Logger::DBG_ERROR, "updateTemperature error: %s.", errstr);
        return false;
    }

    tcflush(PortFD, TCIOFLUSH);

    rc = sscanf(resp, "%X#", &temp);

    if (rc > 0)
    {
        TemperatureN[0].value = ((int)temp) / 2.0;
    }
    else
    {
        DEBUGF(INDI::Logger::DBG_ERROR, "Unknown error: focuser temperature value (%s)", resp);
        return false;
    }

    return true;
}

bool DMFC::updatePosition()
{
    int nbytes_written = 0, nbytes_read = 0, rc = -1;
    char errstr[MAXRBUF];
    char resp[5]={0};
    int pos = -1;

    tcflush(PortFD, TCIOFLUSH);

    if ((rc = tty_write(PortFD, ":GP#", 4, &nbytes_written)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        DEBUGF(INDI::Logger::DBG_ERROR, "updatePostion error: %s.", errstr);
        return false;
    }

    if ((rc = tty_read(PortFD, resp, 5, DMFC_TIMEOUT, &nbytes_read)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        DEBUGF(INDI::Logger::DBG_ERROR, "updatePostion error: %s.", errstr);
        return false;
    }

    tcflush(PortFD, TCIOFLUSH);

    rc = sscanf(resp, "%X#", &pos);

    if (rc > 0)
    {
        FocusAbsPosN[0].value = pos;
    }
    else
    {
        DEBUGF(INDI::Logger::DBG_ERROR, "Unknown error: focuser position value (%s)", resp);
        return false;
    }

    return true;
}

bool DMFC::updateSpeed()
{
    int nbytes_written = 0, nbytes_read = 0, rc = -1;
    char errstr[MAXRBUF];
    char resp[3]={0};
    short speed;

    tcflush(PortFD, TCIOFLUSH);

    if ((rc = tty_write(PortFD, ":GD#", 4, &nbytes_written)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        DEBUGF(INDI::Logger::DBG_ERROR, "updateSpeed error: %s.", errstr);
        return false;
    }

    if ((rc = tty_read(PortFD, resp, 3, DMFC_TIMEOUT, &nbytes_read)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        DEBUGF(INDI::Logger::DBG_ERROR, "updateSpeed error: %s.", errstr);
        return false;
    }

    tcflush(PortFD, TCIOFLUSH);

    rc = sscanf(resp, "%hX#", &speed);

    if (rc > 0)
    {
        int focus_speed = -1;
        while (speed > 0)
        {
            speed >>= 1;
            focus_speed++;
        }

        currentSpeed         = focus_speed;
        FocusSpeedN[0].value = focus_speed;
    }
    else
    {
        DEBUGF(INDI::Logger::DBG_ERROR, "Unknown error: focuser speed value (%s)", resp);
        return false;
    }

    return true;
}

bool DMFC::isMoving()
{
    int nbytes_written = 0, nbytes_read = 0, rc = -1;
    char errstr[MAXRBUF];
    char resp[4]={0};

    tcflush(PortFD, TCIOFLUSH);

    if ((rc = tty_write(PortFD, ":GI#", 4, &nbytes_written)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        DEBUGF(INDI::Logger::DBG_ERROR, "isMoving error: %s.", errstr);
        return false;
    }

    if ((rc = tty_read(PortFD, resp, 3, DMFC_TIMEOUT, &nbytes_read)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        DEBUGF(INDI::Logger::DBG_ERROR, "isMoving error: %s.", errstr);
        return false;
    }

    tcflush(PortFD, TCIOFLUSH);

    resp[3] = '\0';
    if (strcmp(resp, "01#") == 0)
        return true;
    else if (strcmp(resp, "00#") == 0)
        return false;

    DEBUGF(INDI::Logger::DBG_ERROR, "Unknown error: isMoving value (%s)", resp);
    return false;
}

bool DMFC::sync(uint16_t offset)
{
    int nbytes_written = 0, rc = -1;
    char errstr[MAXRBUF];
    char cmd[9]={0};

    snprintf(cmd, 9, ":SP%04X#", offset);

    // Set Position
    if ((rc = tty_write(PortFD, cmd, 8, &nbytes_written)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        DEBUGF(INDI::Logger::DBG_ERROR, "reset error: %s.", errstr);
        return false;
    }

    return true;
}

bool DMFC::MoveFocuser(unsigned int position)
{
    int nbytes_written = 0, rc = -1;
    char errstr[MAXRBUF];
    char cmd[9]={0};

    if (position < FocusAbsPosN[0].min || position > FocusAbsPosN[0].max)
    {
        DEBUGF(INDI::Logger::DBG_ERROR, "Requested position value out of bound: %d", position);
        return false;
    }

    /*if (fabs(position - FocusAbsPosN[0].value) > MaxTravelN[0].value)
    {
        DEBUGF(INDI::Logger::DBG_ERROR, "Requested position value of %d exceeds maximum travel limit of %g", position, MaxTravelN[0].value);
        return false;
    }*/

    snprintf(cmd, 9, ":SN%04X#", position);

    // Set Position
    if ((rc = tty_write(PortFD, cmd, 8, &nbytes_written)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        DEBUGF(INDI::Logger::DBG_ERROR, "setPosition error: %s.", errstr);
        return false;
    }

    // MoveFocuser to Position
    if ((rc = tty_write(PortFD, ":FG#", 4, &nbytes_written)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        DEBUGF(INDI::Logger::DBG_ERROR, "MoveFocuser error: %s.", errstr);
        return false;
    }

    return true;
}

bool DMFC::setSpeed(unsigned short speed)
{
    int nbytes_written = 0, rc = -1;
    char errstr[MAXRBUF];
    char cmd[7]={0};

    int hex_value = 1;

    hex_value <<= speed;

    snprintf(cmd, 7, ":SD%02X#", hex_value);

    if ((rc = tty_write(PortFD, cmd, 6, &nbytes_written)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        DEBUGF(INDI::Logger::DBG_ERROR, "setSpeed error: %s.", errstr);
        return false;
    }

    return true;
}

bool DMFC::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {

    }

    return INDI::Focuser::ISNewSwitch(dev, name, states, names, n);
}

bool DMFC::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        if (strcmp(name, SyncNP.name) == 0)
        {
            IUUpdateNumber(&SyncNP, values, names, n);
            if (sync(SyncN[0].value))
                SyncNP.s = IPS_OK;
            else
                SyncNP.s = IPS_ALERT;
            IDSetNumber(&SyncNP, nullptr);
            return true;
        }
    }

    return INDI::Focuser::ISNewNumber(dev, name, values, names, n);
}

void DMFC::GetFocusParams()
{
    if (updatePosition())
        IDSetNumber(&FocusAbsPosNP, nullptr);

    if (updateTemperature())
        IDSetNumber(&TemperatureNP, nullptr);

    if (updateSpeed())
        IDSetNumber(&FocusSpeedNP, nullptr);

}

bool DMFC::SetFocuserSpeed(int speed)
{
    bool rc = false;

    rc = setSpeed(speed);

    if (!rc)
        return false;

    currentSpeed = speed;

    FocusSpeedNP.s = IPS_OK;
    IDSetNumber(&FocusSpeedNP, nullptr);

    return true;
}

// TODO Probably should disable this and define speed manually
IPState DMFC::MoveFocuser(FocusDirection dir, int speed, uint16_t duration)
{
    if (speed != (int)currentSpeed)
    {
        bool rc = setSpeed(speed);

        if (!rc)
            return IPS_ALERT;
    }

    gettimeofday(&focusMoveStart, nullptr);
    focusMoveRequest = duration / 1000.0;

    if (dir == FOCUS_INWARD)
        MoveFocuser(0);
    else
        MoveFocuser(FocusAbsPosN[0].value);

    if (duration <= POLLMS)
    {
        usleep(duration * 1000);
        AbortFocuser();
        return IPS_OK;
    }

    return IPS_BUSY;
}

IPState DMFC::MoveAbsFocuser(uint32_t targetTicks)
{
    targetPos = targetTicks;

    bool rc = false;

    rc = MoveFocuser(targetPos);

    if (!rc)
        return IPS_ALERT;

    FocusAbsPosNP.s = IPS_BUSY;

    return IPS_BUSY;
}

IPState DMFC::MoveRelFocuser(FocusDirection dir, uint32_t ticks)
{
    double newPosition = 0;
    bool rc            = false;

    if (dir == FOCUS_INWARD)
        newPosition = FocusAbsPosN[0].value - ticks;
    else
        newPosition = FocusAbsPosN[0].value + ticks;

    rc = MoveFocuser(newPosition);

    if (!rc)
        return IPS_ALERT;

    FocusRelPosN[0].value = ticks;
    FocusRelPosNP.s       = IPS_BUSY;

    return IPS_BUSY;
}

void DMFC::TimerHit()
{
    if (!isConnected())
    {
        SetTimer(POLLMS);
        return;
    }

    bool rc = updatePosition();
    if (rc)
    {
        if (fabs(lastPos - FocusAbsPosN[0].value) > 5)
        {
            IDSetNumber(&FocusAbsPosNP, nullptr);
            lastPos = FocusAbsPosN[0].value;
        }
    }

    rc = updateTemperature();
    if (rc)
    {
        if (fabs(lastTemperature - TemperatureN[0].value) >= 0.5)
        {
            IDSetNumber(&TemperatureNP, nullptr);
            lastTemperature = TemperatureN[0].value;
        }
    }

    if (FocusTimerNP.s == IPS_BUSY)
    {
        float remaining = CalcTimeLeft(focusMoveStart, focusMoveRequest);

        if (remaining <= 0)
        {
            FocusTimerNP.s       = IPS_OK;
            FocusTimerN[0].value = 0;
            AbortFocuser();
        }
        else
            FocusTimerN[0].value = remaining * 1000.0;

        IDSetNumber(&FocusTimerNP, nullptr);
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
            DEBUG(INDI::Logger::DBG_SESSION, "Focuser reached requested position.");
        }
    }

    SetTimer(POLLMS);
}

bool DMFC::AbortFocuser()
{
    int nbytes_written;
    if (tty_write(PortFD, ":FQ#", 4, &nbytes_written) == TTY_OK)
    {
        FocusAbsPosNP.s = IPS_IDLE;
        FocusRelPosNP.s = IPS_IDLE;
        IDSetNumber(&FocusAbsPosNP, nullptr);
        IDSetNumber(&FocusRelPosNP, nullptr);
        return true;
    }
    else
        return false;
}

float DMFC::CalcTimeLeft(timeval start, float req)
{
    double timesince;
    double timeleft;
    struct timeval now { 0, 0 };
    gettimeofday(&now, nullptr);

    timesince =
        (double)(now.tv_sec * 1000.0 + now.tv_usec / 1000) - (double)(start.tv_sec * 1000.0 + start.tv_usec / 1000);
    timesince = timesince / 1000;
    timeleft  = req - timesince;
    return timeleft;
}
