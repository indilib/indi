/*
    Gemini Telescope Design Integra85 Focusing Rotator.
    Copyright (C) 2017 Hans Lambermont

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

#define INTEGRA_TIMEOUT_IN_S 3
#define INTEGRA_TEMPERATURE_TRESHOLD_IN_C 0.1

#define NC_25_STEPS 374920
#define INTEGRA85_FOCUSER_STEPS 188600 // 0.053 micron per motor step
#define INTEGRA85_ROTATOR_STEPS 61802

#define POLLMS 500
#define ROTATOR_TAB "Rotator"
#define SETTINGS_TAB "Settings"

std::unique_ptr<Integra> integra(new Integra());

void ISGetProperties(const char *dev)
{
    integra->ISGetProperties(dev);
}

void ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    integra->ISNewSwitch(dev, name, states, names, n);
}

void ISNewText(	const char *dev, const char *name, char *texts[], char *names[], int n)
{
    integra->ISNewText(dev, name, texts, names, n);
}

void ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    integra->ISNewNumber(dev, name, values, names, n);
}

void ISNewBLOB (const char *dev, const char *name, int sizes[], int blobsizes[], char *blobs[],
                char *formats[], char *names[], int n)
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

void ISSnoopDevice (XMLEle *root)
{
    integra->ISSnoopDevice(root);
}

Integra::Integra() : RotatorInterface(this)
{
    SetFocuserCapability(FOCUSER_CAN_ABS_MOVE | FOCUSER_CAN_REL_MOVE | FOCUSER_CAN_ABORT);
    SetRotatorCapability(ROTATOR_CAN_ABORT | ROTATOR_CAN_HOME | ROTATOR_CAN_SYNC | ROTATOR_CAN_REVERSE);
    setFocuserConnection(CONNECTION_SERIAL);
}

bool Integra::initProperties()
{
    INDI::Focuser::initProperties();

    FocusSpeedN[0].min = 1;
    FocusSpeedN[0].max = 1;
    FocusSpeedN[0].value = 1;

    // Temperature Sensor
    IUFillNumber(&SensorN[SENSOR_TEMPERATURE], "TEMPERATURE", "Temperature (C)", "%.2f", -100, 100., 1., 0.);
    IUFillNumberVector(&SensorNP, SensorN, 1, getDeviceName(), "SENSORS", "Sensors", MAIN_CONTROL_TAB, IP_RO, 0, IPS_IDLE );

// TODO we seem to have only 1
    // Limit Switch
    IUFillLight(&LimitSwitchL[ROTATION_SWITCH], "ROTATION_SWITCH", "Rotation Home", IPS_OK);
    IUFillLight(&LimitSwitchL[OUT_SWITCH], "OUT_SWITCH", "Focus Out Limit", IPS_OK);
    IUFillLight(&LimitSwitchL[IN_SWITCH], "IN_SWITCH", "Focus In Limit", IPS_OK);
    IUFillLightVector(&LimitSwitchLP, LimitSwitchL, 3, getDeviceName(), "LIMIT_SWITCHES", "Limit Switch", SETTINGS_TAB, IPS_IDLE);

// TODO we seem to have only focus homing
    // Home selection
    IUFillSwitch(&HomeSelectionS[MOTOR_FOCUS], "FOCUS", "Focuser", ISS_ON);
    IUFillSwitch(&HomeSelectionS[MOTOR_ROTATOR], "ROTATOR", "Rotator", ISS_ON);
    IUFillSwitchVector(&HomeSelectionSP, HomeSelectionS, 2, getDeviceName(), "HOME_SELECTION", "Home Select", SETTINGS_TAB, IP_RW, ISR_NOFMANY, 0, IPS_IDLE);

    // Home Find
    IUFillSwitch(&FindHomeS[0], "FIND", "Start", ISS_OFF);
    IUFillSwitchVector(&FindHomeSP, FindHomeS, 1, getDeviceName(), "FIND_HOME", "Home Find", SETTINGS_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    //////////////////////////////////////////////////////
    // Rotator Properties
    /////////////////////////////////////////////////////

    INDI::RotatorInterface::initProperties(ROTATOR_TAB);

    // Rotator Ticks
    IUFillNumber(&RotatorAbsPosN[0], "ROTATOR_ABSOLUTE_POSITION", "Ticks", "%.f", 0., 100000., 1000., 0.);
    IUFillNumberVector(&RotatorAbsPosNP, RotatorAbsPosN, 1, getDeviceName(), "ABS_ROTATOR_POSITION", "Goto", ROTATOR_TAB, IP_RW, 0, IPS_IDLE );

    /* Relative and absolute movement */
    FocusRelPosN[0].min = 0.;
    FocusRelPosN[0].max = 50000.;
    FocusRelPosN[0].value = 0;
    FocusRelPosN[0].step = 1000;

    FocusAbsPosN[0].min = 0.;
    FocusAbsPosN[0].max = 100000.;
    FocusAbsPosN[0].value = 0;
    FocusAbsPosN[0].step = 1000;

    addDebugControl();

    updatePeriodMS = POLLMS;

    serialConnection->setDefaultBaudRate(Connection::Serial::B_9600);

    return true;
}

bool Integra::updateProperties()
{
    INDI::Focuser::updateProperties();

    if (isConnected())
    {
        // Focus
        defineNumber(&SensorNP);
        defineLight(&LimitSwitchLP);
        defineSwitch(&HomeSelectionSP);
        defineSwitch(&FindHomeSP);

        // Rotator
        INDI::RotatorInterface::updateProperties();
        defineNumber(&RotatorAbsPosNP);
    }
    else
    {
        // Focus
        deleteProperty(SensorNP.name);
        deleteProperty(LimitSwitchLP.name);
        deleteProperty(FindHomeSP.name);
        deleteProperty(HomeSelectionSP.name);

        // Rotator
        INDI::RotatorInterface::updateProperties();
        deleteProperty(RotatorAbsPosNP.name);
    }

    return true;
}

bool Integra::Handshake()
{
    if (Ack())
        return true;

    DEBUG(INDI::Logger::DBG_SESSION, "Error retreiving data from Integra, please ensure Integra controller is powered and the port is correct.");
    return false;
}

const char * Integra::getDefaultName()
{
    return "Integra85";
}

bool Integra::Ack()
{
    bool rcFirmware = getFirmware();
    bool rcType = getFocuserType();

    return (rcFirmware && rcType);
}

bool Integra::getFirmware()
{
    char resp[64] = "1.0";

    DEBUGF(INDI::Logger::DBG_SESSION, "Firmware %s", resp);
    return true;
}

bool Integra::getFocuserType()
{
    char resp[64] = "Integra85";
    DEBUGF(INDI::Logger::DBG_SESSION, "Focuser Type %s", resp);

    if (strcmp(resp, "Integra85") == 0)
    {
        RotatorAbsPosN[0].min = -NC_25_STEPS;
        RotatorAbsPosN[0].max = NC_25_STEPS;
    }
    ticksPerDegree = RotatorAbsPosN[0].max / 360.0;

    return true;
}

bool Integra::gotoMotor(MotorType type, int32_t position)
{
    char cmd[16] = {0};
    char res[16] = {0};

    int nbytes_written = 0, nbytes_read = 0, rc = -1;
    char errstr[MAXRBUF];

    snprintf(cmd, 16, "@PW%d,%d\r\n", type+1, position);

    DEBUGF(INDI::Logger::DBG_DEBUG, "CMD <%s>", cmd);

    tcflush(PortFD, TCIOFLUSH);

    if ( (rc = tty_write(PortFD, cmd, strlen(cmd), &nbytes_written)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        DEBUGF(INDI::Logger::DBG_ERROR, "%s: %s.", __FUNCTION__, errstr);
        return false;
    }

    if ( (rc = tty_read(PortFD, res, 1, INTEGRA_TIMEOUT_IN_S, &nbytes_read)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        DEBUGF(INDI::Logger::DBG_ERROR, "%s error: %s.", __FUNCTION__, errstr);
        return false;
    }

    DEBUGF(INDI::Logger::DBG_DEBUG, "RES <%s>", res);
}

bool Integra::getPosition(MotorType type)
{
    char cmd[16] = {0};
    char res[16] = {0};
    int position = -1e6;

    int nbytes_written = 0, nbytes_read = 0, rc = -1;
    char errstr[MAXRBUF];

    snprintf(cmd, 16, "@PR%d\r\n", type+1);

    DEBUGF(INDI::Logger::DBG_DEBUG, "CMD <%s>", cmd);

    tcflush(PortFD, TCIOFLUSH);

    if ( (rc = tty_write(PortFD, cmd, strlen(cmd), &nbytes_written)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        DEBUGF(INDI::Logger::DBG_ERROR, "%s: %s.", __FUNCTION__, errstr);
        return false;
    }

    if ( (rc = tty_read(PortFD, res, 8, INTEGRA_TIMEOUT_IN_S, &nbytes_read)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        DEBUGF(INDI::Logger::DBG_ERROR, "%s error: %s.", __FUNCTION__, errstr);
        return false;
    }

    res[nbytes_read] = '\0';

    DEBUGF(INDI::Logger::DBG_DEBUG, "RES <%s>", res);

    position = atoi(res+1); // skip the initial P

    if (position != -1e6)
    {
        if (type == MOTOR_FOCUS)
            FocusAbsPosN[0].value = position;
        else if (type == MOTOR_ROTATOR)
            RotatorAbsPosN[0].value = position;

        return true;
    }

    DEBUGF(INDI::Logger::DBG_DEBUG, "Invalid Position! %d", position);
    return false;
}

bool Integra::ISNewSwitch (const char * dev, const char * name, ISState * states, char * names[], int n)
{
    if(strcmp(dev, getDeviceName()) == 0)
    {
        if (strcmp(name, HomeSelectionSP.name) == 0)
        {
            bool atLeastOne = false;

            for (int i=0; i < n; i++)
            {
                if (states[i] == ISS_ON)
                {
                    atLeastOne = true;
                    break;
                }
            }

            if (!atLeastOne)
            {
                HomeSelectionSP.s = IPS_ALERT;
                DEBUG(INDI::Logger::DBG_ERROR, "At least one selection must be on.");
                IDSetSwitch(&HomeSelectionSP, nullptr);
                return false;
            }

            IUUpdateSwitch(&HomeSelectionSP, states, names, n);
            HomeSelectionSP.s = IPS_OK;
            IDSetSwitch(&HomeSelectionSP, nullptr);
            return true;
        }
        else if (strcmp(name, FindHomeSP.name) == 0)
        {
            uint8_t selection = 0;

            if (HomeSelectionS[MOTOR_FOCUS].s == ISS_ON)
                selection |= 0x01;
            if (HomeSelectionS[MOTOR_ROTATOR].s == ISS_ON)
                selection |= 0x02;

            if (findHome(selection))
            {
                FindHomeSP.s = IPS_BUSY;
                FindHomeS[0].s = ISS_ON;
                DEBUG(INDI::Logger::DBG_WARNING, "Homing process can take up to 10 minutes. You cannot control the unit until the process is fully complete.");
            }
            else
            {
                FindHomeSP.s = IPS_ALERT;
                FindHomeS[0].s = ISS_OFF;
                DEBUG(INDI::Logger::DBG_ERROR, "Failed to start homing process.");
            }

            IDSetSwitch(&FindHomeSP, nullptr);
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
        if (strcmp(name, RotatorAbsPosNP.name) == 0)
        {
            RotatorAbsPosNP.s = (gotoMotor(MOTOR_ROTATOR, static_cast<int32_t>(values[0])) ? IPS_BUSY : IPS_ALERT);
            IDSetNumber(&RotatorAbsPosNP, nullptr);
            if (RotatorAbsPosNP.s == IPS_BUSY)
                DEBUGF(INDI::Logger::DBG_SESSION, "Rotator moving to %.f ticks...", values[0]);
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

    bool rc = false;

    rc = gotoMotor(MOTOR_FOCUS, targetPosition);

    if (!rc)
        return IPS_ALERT;

    FocusAbsPosNP.s = IPS_BUSY;

    return IPS_BUSY;
}

IPState Integra::MoveRelFocuser(FocusDirection dir, uint32_t ticks)
{
    double newPosition = 0;
    bool rc = false;

    if (dir == FOCUS_INWARD)
        newPosition = FocusAbsPosN[0].value - ticks;
    else
        newPosition = FocusAbsPosN[0].value + ticks;

    rc = gotoMotor(MOTOR_FOCUS, newPosition);

    if (!rc)
        return IPS_ALERT;

    FocusRelPosN[0].value = ticks;
    FocusRelPosNP.s = IPS_BUSY;

    return IPS_BUSY;
}

void Integra::TimerHit()
{
    if (!isConnected())
    {
        SetTimer(POLLMS);
        return;
    }

    bool rc = false;
    bool sensorsUpdated=false;

    // #1 If we're homing, we check if homing is complete as we cannot check for anything else
    if (FindHomeSP.s == IPS_BUSY || HomeRotatorSP.s == IPS_BUSY)
    {
        if (isHomingComplete())
        {
            HomeRotatorS[0].s = ISS_OFF;
            HomeRotatorSP.s = IPS_OK;
            IDSetSwitch(&HomeRotatorSP, nullptr);

            FindHomeS[0].s = ISS_OFF;
            FindHomeSP.s = IPS_OK;
            IDSetSwitch(&FindHomeSP, nullptr);

            DEBUG(INDI::Logger::DBG_SESSION, "Homing is complete.");
        }

        SetTimer(POLLMS);
        return;
    }

    // #2 Get Temperature
    rc = getTemperature();
    if (rc && fabs(SensorN[SENSOR_TEMPERATURE].value - lastTemperature) > INTEGRA_TEMPERATURE_TRESHOLD_IN_C)
    {
        lastTemperature = SensorN[SENSOR_TEMPERATURE].value;
        sensorsUpdated = true;
    }

    if (sensorsUpdated)
        IDSetNumber(&SensorNP, nullptr);

    // #5 Focus Position & Status
    bool absFocusUpdated = false;

    if (FocusAbsPosNP.s == IPS_BUSY)
    {
        // Stopped moving
        if (!isMotorMoving(MOTOR_FOCUS))
        {
            FocusAbsPosNP.s = IPS_OK;
            if (FocusRelPosNP.s != IPS_OK)
            {
                FocusRelPosNP.s = IPS_OK;
                IDSetNumber(&FocusRelPosNP, nullptr);
            }
            absFocusUpdated = true;
        }
    }
    rc = getPosition(MOTOR_FOCUS);
    if (rc && FocusAbsPosN[0].value != lastFocuserPosition)
    {
        lastFocuserPosition = FocusAbsPosN[0].value;
        absFocusUpdated = true;
    }
    if (absFocusUpdated)
        IDSetNumber(&FocusAbsPosNP, nullptr);

    // #6 Rotator Position & Status
    bool absRotatorUpdated = false;

    if (RotatorAbsPosNP.s == IPS_BUSY)
    {
        // Stopped moving
        if (!isMotorMoving(MOTOR_ROTATOR))
        {
            RotatorAbsPosNP.s = IPS_OK;
            GotoRotatorNP.s = IPS_OK;
            absRotatorUpdated = true;
            DEBUG(INDI::Logger::DBG_SESSION, "Rotator motion complete.");
        }
    }
    rc = getPosition(MOTOR_ROTATOR);
    if (rc && RotatorAbsPosN[0].value != lastRotatorPosition)
    {
        lastRotatorPosition = RotatorAbsPosN[0].value;
        GotoRotatorN[0].value = range360(RotatorAbsPosN[0].value / ticksPerDegree);
        absRotatorUpdated = true;
    }
    if (absRotatorUpdated)
    {
        IDSetNumber(&RotatorAbsPosNP, nullptr);
        IDSetNumber(&GotoRotatorNP, nullptr);
    }

    SetTimer(POLLMS);
}

bool Integra::AbortFocuser()
{
    return stopMotor(MOTOR_FOCUS);
}

bool Integra::stopMotor(MotorType type)
{
    char cmd[16] = {0};
    char res[16] = {0};

    int nbytes_written = 0, nbytes_read = 0, rc = -1;
    char errstr[MAXRBUF];

    snprintf(cmd, 16, "@SW%d,0\r\n", type+1);

    DEBUGF(INDI::Logger::DBG_DEBUG, "CMD <%s>", cmd);

    tcflush(PortFD, TCIOFLUSH);

    if ( (rc = tty_write(PortFD, cmd, strlen(cmd), &nbytes_written)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        DEBUGF(INDI::Logger::DBG_ERROR, "%s: %s.", __FUNCTION__, errstr);
        return false;
    }

    if ( (rc = tty_read(PortFD, res, 2, INTEGRA_TIMEOUT_IN_S, &nbytes_read)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        DEBUGF(INDI::Logger::DBG_ERROR, "%s error: %s.", __FUNCTION__, errstr);
        return false;
    }

    res[nbytes_read] = '\0';

    DEBUGF(INDI::Logger::DBG_DEBUG, "RES <%s>", res);

    return (res[0] == 'S');
}

bool Integra::isMotorMoving(MotorType type)
{
    char cmd[16] = {0};
    char res[16] = {0};

    int nbytes_written = 0, nbytes_read = 0, rc = -1;
    char errstr[MAXRBUF];

    snprintf(cmd, 16, "X");

    DEBUGF(INDI::Logger::DBG_DEBUG, "CMD <%s>", cmd);

    tcflush(PortFD, TCIOFLUSH);

    if ( (rc = tty_write(PortFD, cmd, strlen(cmd), &nbytes_written)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        DEBUGF(INDI::Logger::DBG_ERROR, "%s error: %s.", __FUNCTION__, errstr);
        return false;
    }

    if ( (rc = tty_read_section(PortFD, res, '#', INTEGRA_TIMEOUT_IN_S, &nbytes_read)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        DEBUGF(INDI::Logger::DBG_ERROR, "%s error: %s.", __FUNCTION__, errstr);
        return false;
    }

    res[nbytes_read-1] = '\0';

    DEBUGF(INDI::Logger::DBG_DEBUG, "RES <%s>", res);

    if (type == MOTOR_FOCUS)
        return (strcmp("01", res) == 0);
    else
        return (strcmp("02", res) == 0);
}

bool Integra::getTemperature()
{
    char cmd[16] = "@TR\r\n";
    char res[16] = {0};

    int nbytes_written = 0, nbytes_read = 0, rc = -1;
    char errstr[MAXRBUF];

    DEBUGF(INDI::Logger::DBG_DEBUG, "CMD <%s>", cmd);

    tcflush(PortFD, TCIOFLUSH);

    if ( (rc = tty_write(PortFD, cmd, strlen(cmd), &nbytes_written)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        DEBUGF(INDI::Logger::DBG_ERROR, "%s error: %s.", __FUNCTION__, errstr);
        return false;
    }

    if ( (rc = tty_read_section(PortFD, res, '#', INTEGRA_TIMEOUT_IN_S, &nbytes_read)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        DEBUGF(INDI::Logger::DBG_ERROR, "%s error: %s.", __FUNCTION__, errstr);
        return false;
    }

    res[nbytes_read-1] = '\0';

    DEBUGF(INDI::Logger::DBG_DEBUG, "RES <%s>", res);

    SensorN[SENSOR_TEMPERATURE].value = atoi(res+1);

    return true;
}

bool Integra::findHome(uint8_t motorTypes)
{
    char cmd[16] = {0};
    char res[16] = {0};

    int nbytes_written = 0, nbytes_read = 0, rc = -1;
    char errstr[MAXRBUF];

    snprintf(cmd, 16, "SH %02d#", motorTypes);

    DEBUGF(INDI::Logger::DBG_DEBUG, "CMD <%s>", cmd);

    tcflush(PortFD, TCIOFLUSH);

    if ( (rc = tty_write(PortFD, cmd, strlen(cmd), &nbytes_written)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        DEBUGF(INDI::Logger::DBG_ERROR, "%s error: %s.", __FUNCTION__, errstr);
        return false;
    }

    if ( (rc = tty_read(PortFD, res, 1, INTEGRA_TIMEOUT_IN_S, &nbytes_read)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        DEBUGF(INDI::Logger::DBG_ERROR, "%s error: %s.", __FUNCTION__, errstr);
        return false;
    }

    res[nbytes_read] = '\0';

    DEBUGF(INDI::Logger::DBG_DEBUG, "RES <%s>", res);

    return (res[0] == '#');
}

bool Integra::isHomingComplete()
{
    char res[16] = {0};
    int nbytes_read = 0, rc = -1;

    if ( (rc = tty_read_section(PortFD, res, '#', INTEGRA_TIMEOUT_IN_S, &nbytes_read)) != TTY_OK)
    {
        // No error as we are waiting until controller returns "OK#"
        DEBUG(INDI::Logger::DBG_DEBUG, "Waiting for Integra to complete homing...");
        return false;
    }

    res[nbytes_read-1] = '\0';

    DEBUGF(INDI::Logger::DBG_DEBUG, "RES <%s>", res);

    return (strcmp("OK", res) == 0);
}

bool Integra::saveConfigItems(FILE *fp)
{
    Focuser::saveConfigItems(fp);

//    IUSaveConfigNumber(fp, &FocusStepDelayNP);

    return true;
}

IPState Integra::HomeRotator()
{
    if (findHome(0x02))
    {
        FindHomeSP.s = IPS_BUSY;
        FindHomeS[0].s = ISS_ON;
        IDSetSwitch(&FindHomeSP, nullptr);
        DEBUG(INDI::Logger::DBG_WARNING, "Homing process can take up to 5 minutes. You cannot control the unit until the process is fully complete.");
        return IPS_BUSY;
    }
    else
    {
        FindHomeSP.s = IPS_ALERT;
        FindHomeS[0].s = ISS_OFF;
        IDSetSwitch(&FindHomeSP, nullptr);
        DEBUG(INDI::Logger::DBG_ERROR, "Failed to start homing process.");
        return IPS_ALERT;
    }

    return IPS_ALERT;
}

IPState Integra::MoveRotator(double angle)
{
    // Find shortest distance given target degree
    double a=angle;
    double b=GotoRotatorN[0].value;
    double d=fabs(a-b);
    double r=(d > 180) ? 360 - d : d;
    int sign = (a - b >= 0 && a - b <= 180) || (a - b <=-180 && a- b>= -360) ? 1 : -1;

    r *= sign;

    double newTarget = (r+b) * ticksPerDegree;

    if (newTarget < RotatorAbsPosN[0].min)
        newTarget -= RotatorAbsPosN[0].min;
    else if (newTarget > RotatorAbsPosN[0].max)
        newTarget -= RotatorAbsPosN[0].max;

    bool rc = gotoMotor(MOTOR_ROTATOR, static_cast<int32_t>(newTarget));

    if (rc)
    {
        RotatorAbsPosNP.s = IPS_BUSY;
        IDSetNumber(&RotatorAbsPosNP, nullptr);
        return IPS_BUSY;
    }

    return IPS_ALERT;
}

bool Integra::AbortRotator()
{
    bool rc = stopMotor(MOTOR_ROTATOR);
    if (rc && RotatorAbsPosNP.s != IPS_OK)
    {
        RotatorAbsPosNP.s = IPS_OK;
        IDSetNumber(&RotatorAbsPosNP, nullptr);
    }

    return rc;
}
