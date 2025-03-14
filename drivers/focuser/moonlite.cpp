/*
    Moonlite Focuser
    Copyright (C) 2013-2019 Jasem Mutlaq (mutlaqja@ikarustech.com)

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

#include "moonlite.h"

#include "indicom.h"

#include <cmath>
#include <cstring>
#include <memory>

#include <termios.h>
#include <unistd.h>

static std::unique_ptr<MoonLite> moonLite(new MoonLite());

MoonLite::MoonLite()
{
    setVersion(1, 1);

    // Can move in Absolute & Relative motions, can AbortFocuser motion, and has variable speed.
    FI::SetCapability(FOCUSER_CAN_ABS_MOVE | FOCUSER_CAN_REL_MOVE | FOCUSER_CAN_ABORT | FOCUSER_HAS_VARIABLE_SPEED |
                      FOCUSER_CAN_SYNC);
}

bool MoonLite::initProperties()
{
    INDI::Focuser::initProperties();

    FocusSpeedNP[0].setMin(1);
    FocusSpeedNP[0].setMax(5);
    FocusSpeedNP[0].setValue(1);

    // Step Mode
    StepModeSP[FOCUS_HALF_STEP].fill("FOCUS_HALF_STEP", "Half Step", ISS_OFF);
    StepModeSP[FOCUS_FULL_STEP].fill("FOCUS_FULL_STEP", "Full Step", ISS_ON);
    StepModeSP.fill(getDeviceName(), "Step Mode", "", OPTIONS_TAB, IP_RW, ISR_1OFMANY, 0,
                    IPS_IDLE);

    // Focuser temperature
    TemperatureNP[0].fill("TEMPERATURE", "Celsius", "%6.2f", -50, 70., 0., 0.);
    TemperatureNP.fill(getDeviceName(), "FOCUS_TEMPERATURE", "Temperature",
                       MAIN_CONTROL_TAB, IP_RO, 0, IPS_IDLE);

    // Temperature Settings
    TemperatureSettingNP[Calibration].fill("Calibration", "", "%6.2f", -100, 100, 0.5, 0);
    TemperatureSettingNP[Coefficient].fill("Coefficient", "", "%6.2f", -100, 100, 0.5, 0);
    TemperatureSettingNP.fill(getDeviceName(), "T. Settings", "",
                              OPTIONS_TAB, IP_RW, 0, IPS_IDLE);

    // Compensate for temperature
    TemperatureCompensateSP[INDI_ENABLED].fill("Enable", "", ISS_OFF);
    TemperatureCompensateSP[INDI_DISABLED].fill("Disable", "", ISS_ON);
    TemperatureCompensateSP.fill(getDeviceName(), "T. Compensate",
                                 "", MAIN_CONTROL_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    /* Relative and absolute movement */
    FocusRelPosNP[0].setMin(0.);
    FocusRelPosNP[0].setMax(50000.);
    FocusRelPosNP[0].setValue(0);
    FocusRelPosNP[0].setStep(1000);

    FocusAbsPosNP[0].setMin(0.);
    FocusAbsPosNP[0].setMax(100000.);
    FocusAbsPosNP[0].setValue(0);
    FocusAbsPosNP[0].setStep(1000);

    setDefaultPollingPeriod(500);
    addDebugControl();

    return true;
}

bool MoonLite::updateProperties()
{
    INDI::Focuser::updateProperties();

    if (isConnected())
    {
        defineProperty(TemperatureNP);
        defineProperty(StepModeSP);
        defineProperty(TemperatureSettingNP);
        defineProperty(TemperatureCompensateSP);

        GetFocusParams();

        LOG_INFO("MoonLite parameters updated, focuser ready for use.");
    }
    else
    {
        deleteProperty(TemperatureNP.getName());
        deleteProperty(StepModeSP.getName());
        deleteProperty(TemperatureSettingNP.getName());
        deleteProperty(TemperatureCompensateSP.getName());
    }

    return true;
}

bool MoonLite::Handshake()
{
    if (Ack())
    {
        LOG_INFO("MoonLite is online. Getting focus parameters...");
        return true;
    }

    LOG_INFO(
        "Error retrieving data from MoonLite, please ensure MoonLite controller is powered and the port is correct.");
    return false;
}

const char * MoonLite::getDefaultName()
{
    return "MoonLite";
}

bool MoonLite::Ack()
{
    bool success = false;

    for (int i = 0; i < 3; i++)
    {
        if (readVersion())
        {
            success = true;
            break;
        }

        sleep(1);
    }

    return success;
}

bool MoonLite::readStepMode()
{
    char res[ML_RES] = {0};

    if (sendCommand(":GH#", res) == false)
        return false;

    if (strcmp(res, "FF#") == 0)
        StepModeSP[FOCUS_HALF_STEP].setState(ISS_ON);
    else if (strcmp(res, "00#") == 0)
        StepModeSP[FOCUS_FULL_STEP].setState(ISS_ON);
    else
    {
        LOGF_ERROR("Unknown error: focuser step value (%s)", res);
        return false;
    }

    return true;
}

bool MoonLite::readVersion()
{
    char res[ML_RES] = {0};

    if (sendCommand(":GV#", res, true, 2) == false)
        return false;

    LOGF_INFO("Detected firmware version %c.%c", res[0], res[1]);

    return true;
}

bool MoonLite::readTemperature()
{
    char res[ML_RES] = {0};

    sendCommand(":C#");

    if (sendCommand(":GT#", res) == false)
        return false;

    uint32_t temp = 0;
    int rc = sscanf(res, "%X", &temp);
    if (rc > 0)
        // Signed hex
        TemperatureNP[0].setValue(static_cast<int16_t>(temp) / 2.0);
    else
    {
        LOGF_ERROR("Unknown error: focuser temperature value (%s)", res);
        return false;
    }

    return true;
}

bool MoonLite::readTemperatureCoefficient()
{
    char res[ML_RES] = {0};

    if (sendCommand(":GC#", res) == false)
        return false;

    uint8_t coefficient = 0;
    int rc = sscanf(res, "%hhX", &coefficient);
    if (rc > 0)
        // Signed HEX of two digits
        TemperatureSettingNP[1].setValue(static_cast<int8_t>(coefficient) / 2.0);
    else
    {
        LOGF_ERROR("Unknown error: focuser temperature coefficient value (%s)", res);
        return false;
    }

    return true;
}

bool MoonLite::readPosition()
{
    char res[ML_RES] = {0};

    if (sendCommand(":GP#", res) == false)
        return false;

    int32_t pos;
    int rc = sscanf(res, "%X#", &pos);

    if (rc > 0)
        FocusAbsPosNP[0].setValue(pos);
    else
    {
        LOGF_ERROR("Unknown error: focuser position value (%s)", res);
        return false;
    }

    return true;
}

bool MoonLite::readSpeed()
{
    char res[ML_RES] = {0};

    if (sendCommand(":GD#", res) == false)
        return false;

    uint16_t speed = 0;
    int rc = sscanf(res, "%hX#", &speed);

    if (rc > 0)
    {
        int focus_speed = -1;
        while (speed > 0)
        {
            speed >>= 1;
            focus_speed++;
        }
        FocusSpeedNP[0].setValue(focus_speed);
    }
    else
    {
        LOGF_ERROR("Unknown error: focuser speed value (%s)", res);
        return false;
    }

    return true;
}

bool MoonLite::isMoving()
{
    char res[ML_RES] = {0};

    if (sendCommand(":GI#", res) == false)
        return false;

    // JM 2020-03-13: 01# and 1# should be both accepted
    if (strstr(res, "1#"))
        return true;
    else if (strstr(res, "0#"))
        return false;

    LOGF_ERROR("Unknown error: isMoving value (%s)", res);
    return false;
}

bool MoonLite::setTemperatureCalibration(double calibration)
{
    char cmd[ML_RES] = {0};
    uint8_t hex = static_cast<int8_t>(calibration * 2) & 0xFF;
    snprintf(cmd, ML_RES, ":PO%02X#", hex);
    return sendCommand(cmd);
}

bool MoonLite::setTemperatureCoefficient(double coefficient)
{
    char cmd[ML_RES] = {0};
    uint8_t hex = static_cast<int8_t>(coefficient * 2) & 0xFF;
    snprintf(cmd, ML_RES, ":SC%02X#", hex);
    return sendCommand(cmd);
}

bool MoonLite::SyncFocuser(uint32_t ticks)
{
    char cmd[ML_RES] = {0};
    snprintf(cmd, ML_RES, ":SP%04X#", ticks);
    return sendCommand(cmd);
}

bool MoonLite::MoveFocuser(uint32_t position)
{
    char cmd[ML_RES] = {0};
    snprintf(cmd, ML_RES, ":SN%04X#", position);
    // Set Position First
    if (sendCommand(cmd) == false)
        return false;
    // Now start motion toward position
    if (sendCommand(":FG#") == false)
        return false;

    return true;
}

bool MoonLite::setStepMode(FocusStepMode mode)
{
    char cmd[ML_RES] = {0};
    snprintf(cmd, ML_RES, ":S%c#", (mode == FOCUS_HALF_STEP) ? 'H' : 'F');
    return sendCommand(cmd);
}

bool MoonLite::setSpeed(int speed)
{
    char cmd[ML_RES] = {0};
    int hex_value = 1;
    hex_value <<= speed;
    snprintf(cmd, ML_RES, ":SD%02X#", hex_value);
    return sendCommand(cmd);
}

bool MoonLite::setTemperatureCompensation(bool enable)
{
    char cmd[ML_RES] = {0};
    snprintf(cmd, ML_RES, ":%c#", enable ? '+' : '-');
    return sendCommand(cmd);
}

bool MoonLite::ISNewSwitch(const char * dev, const char * name, ISState * states, char * names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        // Focus Step Mode
        if (StepModeSP.isNameMatch(name))
        {
            int current_mode = StepModeSP.findOnSwitchIndex();

            StepModeSP.update(states, names, n);

            int target_mode = StepModeSP.findOnSwitchIndex();

            if (current_mode == target_mode)
            {
                StepModeSP.setState(IPS_OK);
                StepModeSP.apply();
            }

            bool rc = setStepMode(target_mode == 0 ? FOCUS_HALF_STEP : FOCUS_FULL_STEP);
            if (!rc)
            {
                StepModeSP.reset();
                StepModeSP[current_mode].setState(ISS_ON);
                StepModeSP.setState(IPS_ALERT);
                StepModeSP.apply();
                return false;
            }

            StepModeSP.setState(IPS_OK);
            StepModeSP.apply();
            return true;
        }

        // Temperature Compensation Mode
        if (TemperatureCompensateSP.isNameMatch(name))
        {
            int last_index = TemperatureCompensateSP.findOnSwitchIndex();
            TemperatureCompensateSP.update(states, names, n);

            bool rc = setTemperatureCompensation((TemperatureCompensateSP[INDI_ENABLED].getState() == ISS_ON));

            if (!rc)
            {
                TemperatureCompensateSP.setState(IPS_ALERT);
                TemperatureCompensateSP.reset();
                TemperatureCompensateSP[last_index].setState(ISS_ON);
                TemperatureCompensateSP.apply();
                return false;
            }

            TemperatureCompensateSP.setState(IPS_OK);
            TemperatureCompensateSP.apply();
            return true;
        }
    }

    return INDI::Focuser::ISNewSwitch(dev, name, states, names, n);
}

bool MoonLite::ISNewNumber(const char * dev, const char * name, double values[], char * names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        // Temperature Settings
        if (TemperatureSettingNP.isNameMatch(name))
        {
            TemperatureSettingNP.update(values, names, n);
            if (!setTemperatureCalibration(TemperatureSettingNP[Calibration].getValue()) ||
                    !setTemperatureCoefficient(TemperatureSettingNP[Coefficient].getValue()))
            {
                TemperatureSettingNP.setState(IPS_ALERT);
                TemperatureSettingNP.apply();
                return false;
            }

            TemperatureSettingNP.setState(IPS_OK);
            TemperatureSettingNP.apply();
            return true;
        }
    }

    return INDI::Focuser::ISNewNumber(dev, name, values, names, n);
}

void MoonLite::GetFocusParams()
{
    if (readPosition())
        FocusAbsPosNP.apply();

    if (readTemperature())
        TemperatureNP.apply();

    if (readTemperatureCoefficient())
        TemperatureSettingNP.apply();

    if (readSpeed())
        FocusSpeedNP.apply();

    if (readStepMode())
        StepModeSP.apply();
}

bool MoonLite::SetFocuserSpeed(int speed)
{
    return setSpeed(speed);
}

IPState MoonLite::MoveFocuser(FocusDirection dir, int speed, uint16_t duration)
{
    if (speed != static_cast<int>(FocusSpeedNP[0].getValue()))
    {
        if (!setSpeed(speed))
            return IPS_ALERT;
    }

    // either go all the way in or all the way out
    // then use timer to stop
    if (dir == FOCUS_INWARD)
        MoveFocuser(0);
    else
        MoveFocuser(static_cast<uint32_t>(FocusMaxPosNP[0].getValue()));

    IEAddTimer(duration, &MoonLite::timedMoveHelper, this);
    return IPS_BUSY;
}

void MoonLite::timedMoveHelper(void * context)
{
    static_cast<MoonLite *>(context)->timedMoveCallback();
}

void MoonLite::timedMoveCallback()
{
    AbortFocuser();
    FocusAbsPosNP.setState(IPS_IDLE);
    FocusRelPosNP.setState(IPS_IDLE);
    FocusTimerNP.setState(IPS_IDLE);
    FocusTimerNP[0].setValue(0);
    FocusAbsPosNP.apply();
    FocusRelPosNP.apply();
    FocusTimerNP.apply();
}

IPState MoonLite::MoveAbsFocuser(uint32_t targetTicks)
{
    targetPos = targetTicks;

    if (!MoveFocuser(targetPos))
        return IPS_ALERT;

    return IPS_BUSY;
}

IPState MoonLite::MoveRelFocuser(FocusDirection dir, uint32_t ticks)
{
    // Clamp
    int32_t offset = ((dir == FOCUS_INWARD) ? -1 : 1) * static_cast<int32_t>(ticks);
    int32_t newPosition = FocusAbsPosNP[0].getValue() + offset;
    newPosition = std::max(static_cast<int32_t>(FocusAbsPosNP[0].getMin()),
                           std::min(static_cast<int32_t>(FocusAbsPosNP[0].getMax()),
                                    newPosition));

    if (!MoveFocuser(newPosition))
        return IPS_ALERT;

    FocusRelPosNP[0].setValue(ticks);
    FocusRelPosNP.setState(IPS_BUSY);

    return IPS_BUSY;
}

void MoonLite::TimerHit()
{
    if (!isConnected())
        return;

    bool rc = readPosition();
    if (rc)
    {
        if (fabs(lastPos - FocusAbsPosNP[0].getValue()) > 5)
        {
            FocusAbsPosNP.apply();
            lastPos = static_cast<uint32_t>(FocusAbsPosNP[0].getValue());
        }
    }

    rc = readTemperature();
    if (rc)
    {
        if (std::abs(lastTemperature - TemperatureNP[0].getValue()) >= 0.5)
        {
            TemperatureNP.apply();
            lastTemperature = static_cast<uint32_t>(TemperatureNP[0].getValue());
        }
    }

    if (FocusAbsPosNP.getState() == IPS_BUSY || FocusRelPosNP.getState() == IPS_BUSY)
    {
        if (!isMoving())
        {
            FocusAbsPosNP.setState(IPS_OK);
            FocusRelPosNP.setState(IPS_OK);
            FocusAbsPosNP.apply();
            FocusRelPosNP.apply();
            lastPos = static_cast<uint32_t>(FocusAbsPosNP[0].getValue());
            LOG_INFO("Focuser reached requested position.");
        }
    }

    SetTimer(getCurrentPollingPeriod());
}

bool MoonLite::AbortFocuser()
{
    return sendCommand(":FQ#");
}

bool MoonLite::saveConfigItems(FILE * fp)
{
    Focuser::saveConfigItems(fp);

    StepModeSP.save(fp);

    return true;
}

bool MoonLite::sendCommand(const char * cmd, char * res, bool silent, int nret)
{
    int nbytes_written = 0, nbytes_read = 0, rc = -1;

    tcflush(PortFD, TCIOFLUSH);

    LOGF_DEBUG("CMD <%s>", cmd);

    if ((rc = tty_write_string(PortFD, cmd, &nbytes_written)) != TTY_OK)
    {
        char errstr[MAXRBUF] = {0};
        tty_error_msg(rc, errstr, MAXRBUF);
        if (!silent)
            LOGF_ERROR("Serial write error: %s.", errstr);
        return false;
    }

    if (res == nullptr)
    {
        tcdrain(PortFD);
        return true;
    }

    // this is to handle the GV command which doesn't return the terminator, use the number of chars expected
    if (nret == 0)
    {
        rc = tty_nread_section(PortFD, res, ML_RES, ML_DEL, ML_TIMEOUT, &nbytes_read);
    }
    else
    {
        rc = tty_read(PortFD, res, nret, ML_TIMEOUT, &nbytes_read);
    }
    if (rc != TTY_OK)
    {
        char errstr[MAXRBUF] = {0};
        tty_error_msg(rc, errstr, MAXRBUF);
        if (!silent)
            LOGF_ERROR("Serial read error: %s.", errstr);
        return false;
    }

    LOGF_DEBUG("RES <%s>", res);

    tcflush(PortFD, TCIOFLUSH);

    return true;
}
