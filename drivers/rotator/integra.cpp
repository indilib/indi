/*
    Gemini Telescope Design Integra85 Focusing Rotator.
    Copyright (C) 2017-2021 Hans Lambermont

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

#include "integra.h"

#include "indicom.h"
#include "connectionplugins/connectionserial.h"

#include <cmath>
#include <cstring>
#include <memory>
#include <termios.h>

#define INTEGRA_TIMEOUT_IN_S 5
#define INTEGRA_TEMPERATURE_LOOP_SKIPS 60
#define INTEGRA_TEMPERATURE_TRESHOLD_IN_C 0.1
#define INTEGRA_ROUNDING_FUDGE 0.001

#define ROTATOR_TAB "Rotator"
#define SETTINGS_TAB "Settings"

std::unique_ptr<Integra> integra(new Integra());

typedef struct COMMANDDESC
{
    const char cmd[12];
    char ret[2][3];
} COMMANDDESC;

static const COMMANDDESC IntegraProtocol[] =
{
    { "@SW%d,0\r\n",  { "S", "SW"}},
    { "@CS%d,0\r\n",  { "C", "CS"}},
    { "@CE%d,0\r\n",  { "CE", "CE"}},
    { "@CR%d,0\r\n",  { "CR", "CR"}},
    { "@TR\r\n",      { "T", "TR"}},
    { "@PW%d,0\r\n",  { "P", "PW"}},
    { "@PR%d,0\r\n",  { "P", "PR"}},
    { "@MI%d,%d\r\n", { "M", "MI"}},
    { "@MO%d,%d\r\n", { "M", "MO"}},
    { "@RR%d,0\r\n",  { "R", "RR"}},
    { "X\r\n",        { "", "X"}},
    { "@IW%d,0\r\n",  { "I", "IW"}},
    { "@ZW\r\n",      { "", "ZW"}}
};

enum
{
    stop_motor,
    calibrate,
    calibrate_interrupt,
    calibration_state,
    get_temperature,
    set_motstep,
    get_motstep,
    move_mot_in,
    move_mot_out,
    get_motrange,
    is_moving,
    invert_dir,
    EEPROMwrite
};

Integra::Integra() : RotatorInterface(this)
{
    FI::SetCapability(FOCUSER_CAN_ABS_MOVE | FOCUSER_CAN_REL_MOVE | FOCUSER_CAN_ABORT);
    RI::SetCapability(ROTATOR_CAN_ABORT | ROTATOR_CAN_SYNC | ROTATOR_CAN_REVERSE);

    setSupportedConnections(CONNECTION_SERIAL);
    setVersion(1, 1);
}

bool Integra::initProperties()
{
    INDI::Focuser::initProperties();

    MaxPositionNP[FOCUSER].fill("FOCUSER", "Focuser", "%.f",
                                0, wellKnownIntegra85FocusMax, 0., wellKnownIntegra85FocusMax);
    MaxPositionNP[ROTATOR].fill("ROTATOR", "Rotator", "%.f",
                                0, wellKnownIntegra85RotateMax, 0., wellKnownIntegra85RotateMax);
    MaxPositionNP.fill(getDeviceName(), "MAX_POSITION", "Max position",
                       SETTINGS_TAB, IP_RO, 0, IPS_IDLE);

    FocusSpeedNP[0].setMin(1);
    FocusSpeedNP[0].setMax(1);
    FocusSpeedNP[0].setValue(1);

    // Temperature Sensor
    SensorNP[SENSOR_TEMPERATURE].fill("TEMPERATURE", "Temperature (C)", "%.2f", -100, 100., 1., 0.);
    SensorNP.fill(getDeviceName(), "SENSORS", "Sensors", MAIN_CONTROL_TAB, IP_RO, 0, IPS_IDLE );

    // Home Find
    FindHomeSP[HOMING_IDLE].fill("HOMING_IDLE", "Idle", ISS_ON);
    FindHomeSP[HOMING_START].fill("HOMING_START", "Start", ISS_OFF);
    FindHomeSP[HOMING_ABORT].fill("HOMING_ABORT", "Abort", ISS_OFF);
    FindHomeSP.fill(getDeviceName(), "HOMING", "Home at Center", SETTINGS_TAB, IP_RW,
                    ISR_1OFMANY, 60, IPS_IDLE);

    // Relative and absolute movement
    FocusAbsPosNP[0].setMin(0);
    FocusAbsPosNP[0].setMax(MaxPositionNP[FOCUSER].getValue());
    FocusAbsPosNP[0].setStep(MaxPositionNP[ROTATOR].getValue() / 50.0);
    FocusAbsPosNP[0].setValue(0);

    FocusRelPosNP[0].setMin(0);
    FocusRelPosNP[0].setMax((FocusAbsPosNP[0].getMax() - FocusAbsPosNP[0].getMin()) / 2);
    FocusRelPosNP[0].setStep(FocusRelPosNP[0].getMax() / 100.0);
    FocusRelPosNP[0].setValue(100);

    RI::initProperties(ROTATOR_TAB);

    // Rotator Ticks
    RotatorAbsPosNP[0].fill("ROTATOR_ABSOLUTE_POSITION", "Ticks", "%.f", 0., 61802., 1., 0.);
    RotatorAbsPosNP.fill(getDeviceName(), "ABS_ROTATOR_POSITION", "Goto", ROTATOR_TAB, IP_RW,
                         0, IPS_IDLE );
    RotatorAbsPosNP[0].setMin(0);

    addDebugControl();

    // The device uses an Arduino which shows up as /dev/ttyACM0 on Linux
    // An udev rule example is provided that can create a more logical name like /dev/integra_focusing_rotator0
    serialConnection->setDefaultPort("/dev/ttyACM0");
    // Set mandatory baud speed. The device does not work with anything else.
    serialConnection->setDefaultBaudRate(Connection::Serial::B_115200);

    setDriverInterface(getDriverInterface() | ROTATOR_INTERFACE);

    return true;
}

bool Integra::updateProperties()
{
    INDI::Focuser::updateProperties();

    if (isConnected())
    {
        defineProperty(MaxPositionNP);
        // Focus
        defineProperty(SensorNP);
        defineProperty(FindHomeSP);

        // Rotator
        RI::updateProperties();
        defineProperty(RotatorAbsPosNP);
    }
    else
    {
        deleteProperty(MaxPositionNP);

        // Focus
        deleteProperty(SensorNP);
        deleteProperty(FindHomeSP);

        // Rotator
        RI::updateProperties();
        deleteProperty(RotatorAbsPosNP);
    }

    return true;
}

// This is called from Serial::processHandshake
bool Integra::Handshake()
{
    bool rcFirmware = getFirmware();
    bool rcMaxPositionMotorFocus = getMaxPosition(MOTOR_FOCUS);
    bool rcMaxPositionMotorRotator = getMaxPosition(MOTOR_ROTATOR);
    bool rcType = getFocuserType(); // keep this after the getMaxPositions
    if (rcFirmware && rcMaxPositionMotorFocus && rcMaxPositionMotorRotator && rcType)
    {
        return true;
    }

    LOG_ERROR("Error retrieving data from Integra, please ensure Integra controller is powered, port choice is correct and baud rate is 115200.");
    return false;
}

const char * Integra::getDefaultName()
{
    return "Integra85";
}

void Integra::cleanPrint(const char *cmd, char *cleancmd)
{
    size_t len = strlen(cmd);
    int j = 0;
    for (size_t i = 0; i <= len; i++)
    {
        if (cmd[i] == 0xA)
        {
            cleancmd[j++] = '\\';
            cleancmd[j++] = 'n';
        }
        else if (cmd[i] == 0xD)
        {
            cleancmd[j++] = '\\';
            cleancmd[j++] = 'r';
        }
        else
        {
            cleancmd[j++] = cmd[i];
        }
    }
}

// Called from our ::Handshake
bool Integra::getFirmware()
{
    // two firmware versions (in ISO date format) :  2017-01-25 and 2017-12-20
    // still no direct command to retrieve the version but the protocol
    // has changed. the later version is returning the full command prefix as response prefix
    // version 2017-01-25 : cmd RR1,0 => R188600#
    // version 2017-12-20 : cmd RR1.0 => RR188600#

    // to identify the version we try both protocols, newest first.
    if ( genericIntegraCommand(__FUNCTION__, "@RR1,0\r\n", "RR", nullptr))
    {
        LOGF_INFO("Firmware version is %s", "2017-12-20");
        this->firmwareVersion = VERSION_20171220;
    }
    else if ( genericIntegraCommand(__FUNCTION__, "@RR1,0\r\n", "R", nullptr))
    {
        LOGF_INFO("Firmware version is %s, note: there is a firmware upgrade available.", "2017-01-25");
        this->firmwareVersion = VERSION_20170125;
    }
    else
    {
        LOG_ERROR("Cannot determine firmware version, there may be a firmware upgrade available.");
        return false;   // cannot retrieve firmware session.
    }

    return true;
}

// Called from our ::Handshake
bool Integra::getFocuserType()
{
    int focus_max = int(FocusAbsPosNP[0].getMax());
    int rotator_max = int(RotatorAbsPosNP[0].getMax());
    if (focus_max != wellKnownIntegra85FocusMax)
    {
        LOGF_ERROR("This is no Integra85 because focus max position %d != %d, trying to continue still", focus_max,
                   wellKnownIntegra85FocusMax);
        // return false;
    }
    if (rotator_max != wellKnownIntegra85RotateMax)
    {
        LOGF_ERROR("This is no Integra85 because rotator max position %d != %d, trying to continue still", rotator_max,
                   wellKnownIntegra85RotateMax);
        // return false;
    }

    char resp[64] = "Integra85"; // TODO this is an assumption until the device can report its type
    LOGF_INFO("Focuser Type %s", resp);
    if (strcmp(resp, "Integra85") == 0)
    {
        // RotatorAbsPosN[0].max is already set by getMaxPosition
        rotatorTicksPerDegree = RotatorAbsPosNP[0].getMax() / 360.0;
        rotatorDegreesPerTick = 360.0 / RotatorAbsPosNP[0].getMax();
    }

    return true;
}

bool Integra::relativeGotoMotor(MotorType type, int32_t relativePosition)
{
    int motorMoveCommand;

    LOGF_DEBUG("Start relativeGotoMotor to %d ...", relativePosition);
    if (relativePosition > 0)
        motorMoveCommand = move_mot_out;
    else
        motorMoveCommand = move_mot_in;

    if (type == MOTOR_FOCUS)
    {
        if (relativePosition > 0)
        {
            if (lastFocuserPosition + relativePosition > MaxPositionNP[MOTOR_FOCUS].getValue())
            {
                int newRelativePosition = (int32_t)floor(MaxPositionNP[MOTOR_FOCUS].getValue()) - lastFocuserPosition;
                LOGF_INFO("Focus position change %d clipped to %d to stay at MAX %d",
                          relativePosition, newRelativePosition, MaxPositionNP[MOTOR_FOCUS].getValue());
                relativePosition = newRelativePosition;
            }
        }
        else
        {
            if ((int32_t )lastFocuserPosition + relativePosition < 0)
            {
                int newRelativePosition = -lastFocuserPosition;
                LOGF_INFO("Focus position change %d clipped to %d to stay at MIN 0",
                          relativePosition, newRelativePosition);
                relativePosition = newRelativePosition;
            }
        }
    }
    else if (type == MOTOR_ROTATOR)
    {
        if (relativePosition > 0)
        {
            if (lastRotatorPosition + relativePosition > MaxPositionNP[MOTOR_ROTATOR].getValue())
            {
                int newRelativePosition = (int32_t)floor(MaxPositionNP[MOTOR_ROTATOR].getValue()) - lastRotatorPosition;
                LOGF_INFO("Rotator position change %d clipped to %d to stay at MAX %d",
                          relativePosition, newRelativePosition, MaxPositionNP[MOTOR_ROTATOR].getValue());
                relativePosition = newRelativePosition;
            }
        }
        else
        {
            if (lastRotatorPosition + relativePosition < - MaxPositionNP[MOTOR_ROTATOR].getValue())
            {
                int newRelativePosition = - (int32_t)floor(MaxPositionNP[MOTOR_ROTATOR].getValue()) - lastRotatorPosition;
                LOGF_INFO("Rotator position change %d clipped to %d to stay at MIN %d",
                          relativePosition, newRelativePosition, - MaxPositionNP[MOTOR_ROTATOR].getValue());
                relativePosition = newRelativePosition;
            }
        }
    }

    return  integraMotorSetCommand( __FUNCTION__, motorMoveCommand, type, abs(relativePosition), nullptr);
}

bool Integra::gotoMotor(MotorType type, int32_t position)
{
    LOGF_DEBUG("Start gotoMotor to %d", position);
    if (type == MOTOR_FOCUS)
    {
        return relativeGotoMotor(type, position - lastFocuserPosition);
    }
    else if (type == MOTOR_ROTATOR)
    {
        return relativeGotoMotor(type, position - lastRotatorPosition);
    }
    else
    {
        LOGF_ERROR("%s error: MotorType %d is unknown.", __FUNCTION__, type);
    }
    return false;
}

bool Integra::getPosition(MotorType type)
{
    char res[16] = {0};
    if ( !integraMotorGetCommand(__FUNCTION__, get_motstep, type, res ))
    {
        return false;
    }
    int position = -1e6;
    position = atoi(res);
    if (position != -1e6)
    {
        if (type == MOTOR_FOCUS)
        {
            if (FocusAbsPosNP[0].getValue() != position)
            {
                auto position_from = (int) FocusAbsPosNP[0].getValue();
                int position_to = position;
                if (haveReadFocusPositionAtLeastOnce)
                {
                    LOGF_DEBUG("Focus position changed from %d to %d", position_from, position_to);
                }
                else
                {
                    LOGF_DEBUG("Focus position is %d", position_to);
                }
                FocusAbsPosNP[0].setValue(position);
            }
        }
        else if (type == MOTOR_ROTATOR)
        {
            if (RotatorAbsPosNP[0].getValue() != position)
            {
                auto position_from = (int) RotatorAbsPosNP[0].getValue();
                int position_to = position;
                if (haveReadRotatorPositionAtLeastOnce)
                {
                    LOGF_DEBUG("Rotator changed angle from %.2f to %.2f, position from %d to %d",
                               rotatorTicksToDegrees(position_from), rotatorTicksToDegrees(position_to), position_from, position_to);
                }
                else
                {
                    LOGF_DEBUG("Rotator angle is %.2f, position is %d",
                               rotatorTicksToDegrees(position_to), position_to);
                }
                RotatorAbsPosNP[0].setValue(position);
            }
        }
        else
        {
            LOGF_ERROR("%s error: motor type %d is unknown", __FUNCTION__, type);
        }

        return true;
    }

    LOGF_DEBUG("Invalid Position! %d", position);
    return false;
}

bool Integra::ISNewSwitch (const char * dev, const char * name, ISState * states, char * names[], int n)
{
    if(strcmp(dev, getDeviceName()) == 0)
    {
        if (FindHomeSP.isNameMatch(name))
        {
            FindHomeSP.update(states, names, n);
            int index = FindHomeSP.findOnSwitchIndex();
            switch (index)
            {
                case HOMING_IDLE:
                    LOG_INFO("Homing state is IDLE");
                    FindHomeSP[HOMING_IDLE].setState(ISS_ON);
                    FindHomeSP.setState(IPS_OK);
                    break;
                case HOMING_START:
                    if (findHome())
                    {
                        FindHomeSP.setState(IPS_BUSY);
                        FindHomeSP[HOMING_START].setState(ISS_ON);
                        DEBUG(INDI::Logger::DBG_WARNING,
                              "Homing process can take up to 2 minutes. You cannot control the unit until the process is fully complete.");
                    }
                    else
                    {
                        FindHomeSP.setState(IPS_ALERT);
                        FindHomeSP[HOMING_START].setState(ISS_OFF);
                        LOG_ERROR("Failed to start homing process.");
                    }
                    break;
                case HOMING_ABORT:
                    if (abortHome())
                    {
                        FindHomeSP.setState(IPS_IDLE);
                        FindHomeSP[HOMING_ABORT].setState(ISS_ON);
                        LOG_WARN("Homing aborted");
                    }
                    else
                    {
                        FindHomeSP.setState(IPS_ALERT);
                        FindHomeSP[HOMING_ABORT].setState(ISS_OFF);
                        LOG_ERROR("Failed to abort homing process.");
                    }
                    break;
                default:
                    FindHomeSP.setState(IPS_ALERT);
                    LOG_INFO("Unknown homing index %d");
                    FindHomeSP.apply();
                    return false;
            }
            FindHomeSP.apply();
            return true;
        }
        else if (strstr(name, "ROTATOR"))
        {
            if (INDI::RotatorInterface::processSwitch(dev, name, states, names, n))
                return true;
        }
    }

    return INDI::Focuser::ISNewSwitch(dev, name, states, names, n);
}

bool Integra::ISNewNumber (const char * dev, const char * name, double values[], char * names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        if (RotatorAbsPosNP.isNameMatch(name))
        {
            RotatorAbsPosNP.setState((gotoMotor(MOTOR_ROTATOR, static_cast<int32_t>(values[0])) ? IPS_BUSY : IPS_ALERT));
            RotatorAbsPosNP.apply();
            if (RotatorAbsPosNP.getState() == IPS_BUSY)
                LOGF_DEBUG("Rotator moving from %d to %.f ticks...", lastRotatorPosition, values[0]);
            return true;
        }
        else if (strstr(name, "ROTATOR"))
        {
            if (INDI::RotatorInterface::processNumber(dev, name, values, names, n))
                return true;
        }
    }

    return INDI::Focuser::ISNewNumber(dev, name, values, names, n);
}

IPState Integra::MoveAbsFocuser(uint32_t targetTicks)
{
    targetPosition = targetTicks;
    LOGF_DEBUG("Focuser will move absolute from %d to %d ...", lastFocuserPosition, targetTicks);

    bool rc = false;
    rc = gotoMotor(MOTOR_FOCUS, targetPosition);
    if (!rc)
        return IPS_ALERT;

    FocusAbsPosNP.setState(IPS_BUSY);

    return IPS_BUSY;
}

IPState Integra::MoveRelFocuser(FocusDirection dir, uint32_t ticks)
{
    double newPosition = 0;
    bool rc = false;
    LOGF_DEBUG("Focuser will move in direction %d relative %d ticks...", dir, ticks);

    if (dir == FOCUS_INWARD)
        newPosition = FocusAbsPosNP[0].getValue() - ticks;
    else
        newPosition = FocusAbsPosNP[0].getValue() + ticks;

    rc = gotoMotor(MOTOR_FOCUS, newPosition);
    if (!rc)
        return IPS_ALERT;

    FocusRelPosNP[0].setValue(ticks);
    FocusRelPosNP.setState(IPS_BUSY);

    return IPS_BUSY;
}

void Integra::TimerHit()
{
    if (!isConnected())
    {
        SetTimer(getCurrentPollingPeriod());
        return;
    }

    bool rc = false;
    bool savePositionsToEEPROM = false;

    // sanity check warning ...
    if (int(FocusAbsPosNP[0].getMax()) != wellKnownIntegra85FocusMax
            || int(RotatorAbsPosNP[0].getMax()) != wellKnownIntegra85RotateMax)
    {
        LOGF_WARN("Warning: Focus motor max position %d should be %d and Rotator motor max position %d should be %d",
                  int(FocusAbsPosNP[0].getMax()), wellKnownIntegra85FocusMax,
                  int(RotatorAbsPosNP[0].getMax()), wellKnownIntegra85RotateMax);
    }

    // #1 If we're homing, we check if homing is complete as we cannot check for anything else
    if (FindHomeSP.getState() == IPS_BUSY)
    {
        if (isHomingComplete())
        {
            FindHomeSP[HOMING_IDLE].setState(ISS_OFF);
            FindHomeSP.setState(IPS_OK);
            FindHomeSP.apply();

            LOG_INFO("Homing is complete");
            // Next read positions and save to EEPROM :
            haveReadFocusPositionAtLeastOnce = false;
            haveReadRotatorPositionAtLeastOnce = false;
        }
        else
        {
            LOG_DEBUG("Homing");
        }

        SetTimer(getCurrentPollingPeriod());
        return;
    }

    // #2 Get Temperature, only read this when no motors are active, and about once per minute
    if (FocusAbsPosNP.getState() != IPS_BUSY && FocusRelPosNP.getState() != IPS_BUSY
            && RotatorAbsPosNP.getState() != IPS_BUSY
            && timeToReadTemperature <= 0)
    {
        rc = getTemperature();
        if ( ! rc)
            rc = getTemperature();
        if (rc)
        {
            timeToReadTemperature = INTEGRA_TEMPERATURE_LOOP_SKIPS;
            if (fabs(SensorNP[SENSOR_TEMPERATURE].getValue() - lastTemperature) > INTEGRA_TEMPERATURE_TRESHOLD_IN_C)
            {
                lastTemperature = SensorNP[SENSOR_TEMPERATURE].getValue();
                SensorNP.apply();
            }
        }
    }
    else
    {
        timeToReadTemperature--;
    }

    // #3 is not used on Integra85
    // #4 is not used on Integra85

    // #5 Focus Position & Status
    if (!haveReadFocusPositionAtLeastOnce || FocusAbsPosNP.getState() == IPS_BUSY || FocusRelPosNP.getState() == IPS_BUSY)
    {
        if ( ! isMotorMoving(MOTOR_FOCUS))
        {
            LOG_DEBUG("Focuser stopped");
            FocusAbsPosNP.setState(IPS_OK);
            FocusRelPosNP.setState(IPS_OK);
            rc = getPosition(MOTOR_FOCUS);
            if (rc)
            {
                if (FocusAbsPosNP[0].getValue() != lastFocuserPosition)
                {
                    lastFocuserPosition = FocusAbsPosNP[0].getValue();
                    FocusAbsPosNP.apply();
                    FocusRelPosNP.apply();
                    if (haveReadFocusPositionAtLeastOnce)
                    {
                        LOGF_INFO("Focuser reached requested position %d", lastFocuserPosition);
                    }
                    else
                    {
                        LOGF_INFO("Focuser position is %d", lastFocuserPosition);
                        haveReadFocusPositionAtLeastOnce = true;
                    }
                    savePositionsToEEPROM = true;
                }
            }
        }
        else
        {
            LOG_DEBUG("Focusing");
        }
    }

    // #6 Rotator Position & Status
    if (!haveReadRotatorPositionAtLeastOnce  || RotatorAbsPosNP.getState() == IPS_BUSY)
    {
        if ( ! isMotorMoving(MOTOR_ROTATOR))
        {
            LOG_DEBUG("Rotator stopped");
            RotatorAbsPosNP.setState(IPS_OK);
            GotoRotatorNP.setState(IPS_OK);
            rc = getPosition(MOTOR_ROTATOR);
            if (rc)
            {
                if (RotatorAbsPosNP[0].getValue() != lastRotatorPosition)
                {
                    lastRotatorPosition = RotatorAbsPosNP[0].getValue();
                    GotoRotatorNP[0].setValue(rotatorTicksToDegrees(
                                                lastRotatorPosition)); //range360(RotatorAbsPosN[0].value / rotatorTicksPerDegree);
                    RotatorAbsPosNP.apply( );
                    GotoRotatorNP.apply();
                    if (haveReadRotatorPositionAtLeastOnce)
                        LOGF_INFO("Rotator reached requested angle %.2f, position %d",
                                  rotatorTicksToDegrees(lastRotatorPosition), lastRotatorPosition);
                    else
                    {
                        LOGF_INFO("Rotator is at angle %.2f, position %d",
                                  rotatorTicksToDegrees(lastRotatorPosition), lastRotatorPosition);
                        haveReadRotatorPositionAtLeastOnce = true;
                    }
                    savePositionsToEEPROM = true;
                }
            }
        }
        else
        {
            LOG_DEBUG("Rotating");
        }
    }

    // #7 is not used on Integra85

    if (savePositionsToEEPROM)
    {
        saveToEEPROM();
    }
    SetTimer(getCurrentPollingPeriod());
}

bool Integra::AbortFocuser()
{
    return stopMotor(MOTOR_FOCUS);
}

bool Integra::stopMotor(MotorType type)
{
    // TODO (if focuser?) handle CR 2
    if (integraMotorGetCommand(__FUNCTION__, stop_motor, type, nullptr))
    {
        if (type == MOTOR_FOCUS)
        {
            haveReadFocusPositionAtLeastOnce = false;
        }
        else
        {
            haveReadRotatorPositionAtLeastOnce = false;
        }
        return true;
    }

    return false;
}

bool Integra::isMotorMoving(MotorType type)
{
    char res[16] = {0};
    if ( ! integraGetCommand( __FUNCTION__, is_moving, res))
    {
        return false;
    }
    if (type == MOTOR_FOCUS)
    {
        if (res[0] == '1')
        {
            LOG_DEBUG("Focus motor is running");
            return true;
        }
        else
        {
            LOG_DEBUG("Focus motor is not running");
            return false;
        }
    }
    else
    {
        // bug, both motors return 1 at res[0] when running
        //  return (res[0] == '2');
        if (res[0] == '1')
        {
            LOG_DEBUG("Rotator motor is running");
            return true;
        }
        else
        {
            LOG_DEBUG("Rotator motor is not running");
            return false;
        }
    }
}

// Called from our ::Handshake
bool Integra::getMaxPosition(MotorType type)
{
    char res[16] = {0};
    if ( ! integraMotorGetCommand(__FUNCTION__, get_motrange, type, res))
    {
        return false;
    }
    int position = atoi(res);
    if (MaxPositionNP[type].getValue() == position)
    {
        LOGF_INFO("%s motor max position is %d", (type == MOTOR_FOCUS) ? "Focuser" : "Rotator", position);
    }
    else
    {
        LOGF_WARN("Updated %s motor max position from %d to %d",
                  (type == MOTOR_FOCUS) ? "Focuser" : "Rotator", MaxPositionNP[type].getValue(), position);
        MaxPositionNP[type].setValue(position);
        if (type == MOTOR_FOCUS)
        {
            FocusAbsPosNP[0].setMax(MaxPositionNP[type].getValue());
        }
        else if (type == MOTOR_ROTATOR)
        {
            RotatorAbsPosNP[0].setMax(MaxPositionNP[type].getValue());
        }
        else
        {
            LOG_ERROR("Unknown Motor type");
        }
    }
    return position > 0; // cannot consider a max position == 0 as a valid max.
}

bool Integra::saveToEEPROM()
{
    return integraGetCommand(__FUNCTION__, EEPROMwrite, nullptr);
}

bool Integra::getTemperature()
{
    char res[16] = {0};
    if (integraGetCommand(__FUNCTION__, get_temperature, res ) )
    {
        SensorNP[SENSOR_TEMPERATURE].setValue(strtod(res, nullptr));
        return true;
    }
    return false;
}

bool Integra::findHome()
{
    return integraMotorGetCommand(__FUNCTION__, calibrate, MOTOR_FOCUS, nullptr);
}

bool Integra::abortHome()
{
    return integraMotorGetCommand(__FUNCTION__, calibrate_interrupt, MOTOR_FOCUS, nullptr);
}

bool Integra::isHomingComplete()
{
    char res[16] = {0};
    if (integraMotorGetCommand(__FUNCTION__, calibration_state, MOTOR_FOCUS, res))
    {
        return (res[0] == '1');
    }
    return false;
}

bool Integra::saveConfigItems(FILE *fp)
{
    Focuser::saveConfigItems(fp);
    return true;
}

// Integra position 0..61802 ticks , angle 0..360 deg, position 0 corresponds to 180 deg
// We need to map the Integra frame to that of the IndiRotatorInterface.
// INDI rotatorInterface: Only absolute position Rotators are supported.
// Angle is ranged from 0 to 360 increasing clockwise when looking at the back of the camera.
IPState Integra::MoveRotator(double angle)
{
    uint32_t p1 = lastRotatorPosition;
    uint32_t p2 = rotatorDegreesToTicks(angle);

    LOGF_INFO("MoveRotator from %.2f to %.2f degrees, from position %d to %d ...",
              rotatorTicksToDegrees(lastRotatorPosition), angle, p1, p2);
    bool rc = relativeGotoMotor(MOTOR_ROTATOR, p2 - p1);
    if (rc)
    {
        RotatorAbsPosNP.setState(IPS_BUSY);
        RotatorAbsPosNP.apply();
        return IPS_BUSY;
    }

    return IPS_ALERT;
}

bool Integra::AbortRotator()
{
    bool rc = stopMotor(MOTOR_ROTATOR);
    if (rc && RotatorAbsPosNP.getState() != IPS_OK)
    {
        RotatorAbsPosNP.setState(IPS_OK);
        RotatorAbsPosNP.apply();
    }

    return rc;
}

uint32_t Integra::rotatorDegreesToTicks(double angle)
{
    uint32_t position = 61802 / 2;
    if (angle >= 0.0 && angle <= 180.0)
    {
        position = (uint32_t) lround(61802.0 - (180.0 - angle) / rotatorDegreesPerTick);
    }
    else if (angle > 180 && angle <= 360)
    {
        position = (uint32_t) lround(61802.0 - (540.0 - angle) / rotatorDegreesPerTick);
    }
    else
    {
        LOGF_ERROR("%s error: %.2f is out of range", __FUNCTION__, angle);
    }
    return position;
}

double Integra::rotatorTicksToDegrees(uint32_t ticks)
{
    double degrees = range360(180.0 + ticks * rotatorDegreesPerTick + INTEGRA_ROUNDING_FUDGE);
    return degrees;
}

bool Integra::SyncRotator(double angle)
{
    uint32_t position = rotatorDegreesToTicks(angle);
    if ( integraMotorSetCommand(__FUNCTION__, set_motstep, MOTOR_ROTATOR, position, nullptr ))
    {
        haveReadRotatorPositionAtLeastOnce = false;
        return true;
    }
    return false;
}

bool Integra::ReverseRotator(bool)
{
    return  integraMotorGetCommand(__FUNCTION__, invert_dir, MOTOR_ROTATOR, nullptr);
}

bool Integra::integraGetCommand( const char *name, int commandIdx, char *returnValueString )
{
    char cmd[16] = {0};
    snprintf(cmd, 16, "%s", IntegraProtocol[commandIdx].cmd);
    return genericIntegraCommand(name, cmd, IntegraProtocol[commandIdx].ret[this->firmwareVersion], returnValueString);
}

bool Integra::integraMotorGetCommand( const char *name, int commandIdx, MotorType motor, char *returnValueString )
{
    char cmd[16] = {0};
    snprintf(cmd, 16, IntegraProtocol[commandIdx].cmd, motor + 1);
    return genericIntegraCommand(name, cmd, IntegraProtocol[commandIdx].ret[this->firmwareVersion], returnValueString);
}

bool Integra::integraMotorSetCommand(const char *name, int commandIdx, MotorType motor, int value,
                                     char *returnValueString )
{
    char cmd[16] = {0};
    snprintf(cmd, 16, IntegraProtocol[commandIdx].cmd, motor + 1, value);
    return genericIntegraCommand(name, cmd, IntegraProtocol[commandIdx].ret[this->firmwareVersion], returnValueString);
}

bool Integra::genericIntegraCommand(const char *name, const char *cmd, const char *expectStart, char *returnValueString)
{
    char cmdnocrlf[16] = {0};
    int nbytes_written = 0, nbytes_read = 0, rc = -1;
    char res[16] = {0};
    char *correctRes = nullptr;
    char errstr[MAXRBUF];

    cleanPrint(cmd, cmdnocrlf);
    LOGF_DEBUG("CMD %s (%s)", name, cmdnocrlf);

    tcflush(PortFD, TCIOFLUSH);
    if ( (rc = tty_write(PortFD, cmd, (int)strlen(cmd), &nbytes_written)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("%s: %s.", name, errstr);
        return false;
    }

    if ( (rc = tty_read_section(PortFD, res, '#', INTEGRA_TIMEOUT_IN_S, &nbytes_read)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("%s error: %s.", name, errstr);
        return false;
    }

    LOGF_DEBUG("RES %s (%s)", name, res);

    // check begin of result string
    if (expectStart != nullptr)
    {
        correctRes = strstr(res, expectStart);      // the hw sometimes returns /r or /n at the beginning of the response
        if (correctRes == nullptr)
        {
            LOGF_ERROR("%s error: invalid response (%s)", name, res);
            return false;
        }
    }
    // check end of result string
    if (res[nbytes_read - 1] != '#')
    {
        LOGF_ERROR("%s error: invalid response 2 (%s)", name, res);
        return false;
    }
    res[nbytes_read - 1] = '\0'; // wipe the #

    if (returnValueString != nullptr && expectStart != nullptr)
    {
        size_t expectStrlen = strlen(expectStart);
        strcpy(returnValueString, correctRes + expectStrlen);
    }

    return true;
}
