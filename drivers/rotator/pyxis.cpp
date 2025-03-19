/*
    Optec Pyrix Rotator
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

#include "pyxis.h"

#include "indicom.h"
#include "connectionplugins/connectionserial.h"

#include <cmath>
#include <cstring>
#include <memory>
#include <termios.h>

#define PYXIS_TIMEOUT 3
#define PYRIX_BUF 7
#define PYRIX_CMD 6
#define SETTINGS_TAB    "Settings"

// Recommended default rates for 3 inch and 2 inch rotators
#define PYXIS_3INCH_RATE 6
#define PYXIS_2INCH_RATE 8

// Number of steps per degree for rotators
#define PYXIS_3INCH_PER_DEG (128)
#define PYXIS_2INCH_PER_DEG 14

// 100ms poll rate while rotating
#define POLL_100MS 100

std::unique_ptr<Pyxis> pyxis(new Pyxis());

Pyxis::Pyxis()
{
    setVersion(1, 1);
    // We do not have absolute ticks
    RI::SetCapability(ROTATOR_CAN_HOME | ROTATOR_CAN_REVERSE);
    setRotatorConnection(CONNECTION_SERIAL);
}

bool Pyxis::initProperties()
{
    INDI::Rotator::initProperties();

    // Rotation Rate
    RotationRateNP[0].fill("RATE", "Rate", "%.f", 0, 99, 10, 8);
    RotationRateNP.fill(getDeviceName(), "ROTATION_RATE", "Rotation", SETTINGS_TAB, IP_RW, 0,
                        IPS_IDLE);

    // Stepping
    SteppingSP[FULL_STEP].fill("FULL_STEP", "Full", ISS_OFF);
    SteppingSP[HALF_STEP].fill("HALF_STEP", "Half", ISS_OFF);
    SteppingSP.fill( getDeviceName(), "STEPPING_RATE", "Stepping", SETTINGS_TAB, IP_RW,
                     ISR_ATMOST1, 0, IPS_IDLE);

    // Power
    PowerSP[POWER_SLEEP].fill("POWER_SLEEP", "Sleep", ISS_OFF);
    PowerSP[POWER_WAKEUP].fill("POWER_WAKEUP", "Wake Up", ISS_OFF);
    PowerSP.fill(getDeviceName(), "POWER_STATE", "Power", SETTINGS_TAB, IP_RW, ISR_ATMOST1, 0,
                 IPS_IDLE);

    // Firmware version
    FirmwareTP[0].fill("FIRMWARE_VERSION", "Version", "Unknown");
    FirmwareTP.fill(getDeviceName(), "FIRMWARE_VERSION", "Firmware", INFO_TAB, IP_RO, 0, IPS_IDLE);

    // Firmware version
    ModelTP[0].fill("HARDWARE_MODEL", "Model", "Unknown");
    ModelTP.fill(getDeviceName(), "HARDWARE_MODEL", "Model", INFO_TAB, IP_RO, 0, IPS_IDLE);


    serialConnection->setDefaultBaudRate(Connection::Serial::B_19200);

    return true ;
}

bool Pyxis::Handshake()
{
    if (Ack())
        return true;

    LOG_INFO("Error retrieving data from Pyxis, please ensure Pyxis controller is powered and the port is correct.");
    return false;
}

const char * Pyxis::getDefaultName()
{
    return "Pyxis";
}

bool Pyxis::updateProperties()
{
    INDI::Rotator::updateProperties();

    if (isConnected())
    {
        defineProperty(RotationRateNP)  ;
        defineProperty(SteppingSP);
        defineProperty(PowerSP);
        defineProperty(FirmwareTP) ;
        defineProperty(ModelTP) ;

        queryParams();
    }
    else
    {
        deleteProperty(RotationRateNP);
        deleteProperty(SteppingSP);
        deleteProperty(PowerSP);
        deleteProperty(FirmwareTP) ;
        deleteProperty(ModelTP) ;
    }

    return true;
}

void Pyxis::queryParams()
{
    ////////////////////////////////////////////
    // Reverse Parameter
    ////////////////////////////////////////////
    int dir = getReverseStatus();

    ReverseRotatorSP.reset();
    ReverseRotatorSP.setState(IPS_OK);
    if (dir == 0)
        ReverseRotatorSP[INDI_DISABLED].setState(ISS_ON);
    else if (dir == 1)
        ReverseRotatorSP[INDI_ENABLED].setState(ISS_ON);
    else
        ReverseRotatorSP.setState(IPS_ALERT);

    ReverseRotatorSP.apply();

    // Firmware version parameter
    std::string sversion = getVersion() ;
    FirmwareTP[0].setText(sversion.c_str()) ;
    FirmwareTP.setState(IPS_OK);
    FirmwareTP.apply() ;

    LOGF_DEBUG("queryParms firmware = %s", sversion.c_str()) ;

    // Firmware tells us device type, 3 inch or 2 inch, which defines the correct default rotation rate
    if (atof(sversion.c_str()) >= 3)
    {
        uint16_t rate = (atof(sversion.c_str()) >= 3 ? PYXIS_3INCH_RATE : PYXIS_2INCH_RATE) ;
        bool rc = setRotationRate(rate) ;
        LOGF_DEBUG("queryParms rate = %d, firmware = %s", rate, sversion.c_str()) ;
        if (rc)
        {
            RotationRateNP.setState(IPS_OK) ;
            RotationRateNP[0].setValue(rate) ;
            RotationRateNP.apply() ;

            ModelTP[0].setText("Pyxis 3 Inch");
            ModelTP.setState(IPS_OK);
            ModelTP.apply();
        }
    }
    else
    {
        ModelTP[0].setText("Pyxis 2 Inch") ;
        ModelTP.setState(IPS_OK);
        ModelTP.apply() ;
    }

}

bool Pyxis::Ack()
{
    const char *cmd = "CCLINK";
    char res[1] = {0};

    int nbytes_written = 0, nbytes_read = 0, rc = -1;
    char errstr[MAXRBUF];

    LOGF_DEBUG("CMD <%s>", cmd);

    tcflush(PortFD, TCIOFLUSH);

    if ( (rc = tty_write(PortFD, cmd, PYRIX_CMD, &nbytes_written)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("%s: %s.", __FUNCTION__, errstr);
        return false;
    }

    if ( (rc = tty_read(PortFD, res, 1, PYXIS_TIMEOUT, &nbytes_read)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("%s error: %s.", __FUNCTION__, errstr);
        return false;
    }

    LOGF_DEBUG("RES <%c>", res[0]);

    tcflush(PortFD, TCIOFLUSH);

    if (res[0] != '!')
    {
        LOG_ERROR("Cannot establish communication. Check power is on and homing is complete.");
        return false;
    }

    return true;
}

bool Pyxis::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        if (RotationRateNP.isNameMatch(name))
        {
            bool rc = setRotationRate(static_cast<uint8_t>(values[0]));
            if (rc)
            {
                RotationRateNP.setState(IPS_OK);
                RotationRateNP[0].setValue(values[0]);
            }
            else
                RotationRateNP.setState(IPS_ALERT);

            RotationRateNP.apply();
            return true;
        }
    }

    return Rotator::ISNewNumber(dev, name, values, names, n);
}

bool Pyxis::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        /////////////////////////////////////////////////////
        // Stepping
        ////////////////////////////////////////////////////
        if (SteppingSP.isNameMatch(name))
        {
            bool rc = false;
            if (!strcmp(IUFindOnSwitchName(states, names, n), SteppingSP[FULL_STEP].getName()))
                rc = setSteppingMode(FULL_STEP);
            else
                rc = setSteppingMode(HALF_STEP);


            if (rc)
            {
                SteppingSP.update(states, names, n);
                SteppingSP.setState(IPS_OK);
            }
            else
                SteppingSP.setState(IPS_ALERT);

            SteppingSP.apply();
            return true;
        }

        /////////////////////////////////////////////////////
        // Power
        ////////////////////////////////////////////////////
        if (PowerSP.isNameMatch(name))
        {
            bool rc = false;
            if (!strcmp(IUFindOnSwitchName(states, names, n), PowerSP[POWER_WAKEUP].getName()))
            {
                // If not sleeping
                if (PowerSP[POWER_SLEEP].getState() == ISS_OFF)
                {
                    PowerSP.setState(IPS_OK);
                    LOG_WARN("Controller is not in sleep mode.");
                    PowerSP.apply();
                    return true;
                }

                rc = wakeupController();

                if (rc)
                {
                    PowerSP.reset();
                    PowerSP.setState(IPS_OK);
                    LOG_INFO("Controller is awake.");
                }
                else
                    PowerSP.setState(IPS_ALERT);

                PowerSP.apply();
                return true;
            }
            else
            {
                bool rc = sleepController();
                PowerSP.reset();
                if (rc)
                {
                    PowerSP.setState(IPS_OK);
                    PowerSP[POWER_SLEEP].setState(ISS_ON);
                    LOG_INFO("Controller in sleep mode. No functions can be used until controller is waken up.");
                }
                else
                    PowerSP.setState(IPS_ALERT);

                PowerSP.apply();
                return true;
            }
        }
    }

    return Rotator::ISNewSwitch(dev, name, states, names, n);
}

bool Pyxis::setSteppingMode(uint8_t mode)
{
    char cmd[PYRIX_BUF] = {0};

    int nbytes_written = 0, rc = -1;
    char errstr[MAXRBUF];

    snprintf(cmd, PYRIX_BUF, "CZ%dxxx", mode);

    LOGF_DEBUG("CMD <%s>", cmd);

    tcflush(PortFD, TCIOFLUSH);

    if ( (rc = tty_write(PortFD, cmd, PYRIX_CMD, &nbytes_written)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("%s: %s.", __FUNCTION__, errstr);
        return false;
    }

    return true;
}

bool Pyxis::setRotationRate(uint8_t rate)
{
    char cmd[PYRIX_BUF] = {0};
    int nbytes_written = 0, rc = -1;
    char errstr[MAXRBUF];

    char res[1] = { 0 } ;
    int nbytes_read = 0 ;

    snprintf(cmd, PYRIX_BUF, "CTxx%02d", rate);

    LOGF_DEBUG("CMD <%s>", cmd);

    tcflush(PortFD, TCIOFLUSH);

    if ( (rc = tty_write(PortFD, cmd, PYRIX_CMD, &nbytes_written)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("%s: %s.", __FUNCTION__, errstr);
        return false;
    }

    if ( (rc = tty_read(PortFD, res, 1, PYXIS_TIMEOUT, &nbytes_read)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("%s error: %s.", __FUNCTION__, errstr);
        return false;
    }

    tcflush(PortFD, TCIOFLUSH);

    LOGF_DEBUG("RES <%c>", res[0]);

    return (res[0] == '!');
}

bool Pyxis::sleepController()
{
    const char *cmd = "CSLEEP";

    int nbytes_written = 0, rc = -1;
    char errstr[MAXRBUF];

    LOGF_DEBUG("CMD <%s>", cmd);

    tcflush(PortFD, TCIOFLUSH);

    if ( (rc = tty_write(PortFD, cmd, PYRIX_CMD, &nbytes_written)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("%s: %s.", __FUNCTION__, errstr);
        return false;
    }

    return true;
}

bool Pyxis::wakeupController()
{
    const char *cmd = "CWAKEUP";
    char res[1] = { 0 };

    int nbytes_written = 0, nbytes_read = 0, rc = -1;
    char errstr[MAXRBUF];

    LOGF_DEBUG("CMD <%s>", cmd);

    tcflush(PortFD, TCIOFLUSH);

    if ( (rc = tty_write(PortFD, cmd, PYRIX_CMD, &nbytes_written)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("%s: %s.", __FUNCTION__, errstr);
        return false;
    }

    if ( (rc = tty_read(PortFD, res, 1, PYXIS_TIMEOUT, &nbytes_read)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("%s error: %s.", __FUNCTION__, errstr);
        return false;
    }

    tcflush(PortFD, TCIOFLUSH);

    LOGF_DEBUG("RES <%c>", res[0]);

    return (res[0] == '!');
}

IPState Pyxis::HomeRotator()
{
    const char *cmd = "CHOMES";

    int nbytes_written = 0, rc = -1;
    char errstr[MAXRBUF];

    LOGF_DEBUG("CMD <%s>", cmd);

    tcflush(PortFD, TCIOFLUSH);

    if ( (rc = tty_write(PortFD, cmd, PYRIX_CMD, &nbytes_written)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("%s: %s.", __FUNCTION__, errstr);
        return IPS_ALERT;
    }

    return IPS_BUSY;
}

IPState Pyxis::MoveRotator(double angle)
{
    char cmd[PYRIX_BUF] = {0};

    int nbytes_written = 0, rc = -1;
    char errstr[MAXRBUF];

    uint16_t current = static_cast<uint16_t>(GotoRotatorNP[0].getValue()) ;

    targetPA = static_cast<uint16_t>(round(angle));

    if (targetPA > 359)
        targetPA = 0;

    // Rotator will only rotation +-180 degrees from home (0 degrees) so it make take
    // the long way to avoid cable wrap
    if (current <= 180 && targetPA < 180)
        direction = (targetPA >= current ? 1 : -1) ;
    else if (current <= 180 && targetPA > 180)
        direction = -1 ;
    else if (current > 180 && targetPA >= 180)
        direction = (targetPA >= current ? 1 : -1) ;
    else if (current > 180 && targetPA < 180)
        direction = 1 ;

    snprintf(cmd, PYRIX_BUF, "CPA%03d", targetPA);

    LOGF_DEBUG("CMD <%s>", cmd);

    tcflush(PortFD, TCIOFLUSH);

    if ( (rc = tty_write(PortFD, cmd, strlen(cmd), &nbytes_written)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("%s: %s.", __FUNCTION__, errstr);
        return IPS_ALERT;
    }

    return IPS_BUSY;
}

bool Pyxis::ReverseRotator(bool enabled)
{
    char cmd[PYRIX_BUF] = {0};

    int nbytes_written = 0, rc = -1;
    char errstr[MAXRBUF];

    snprintf(cmd, PYRIX_BUF, "CD%dxxx", enabled ? 1 : 0);

    LOGF_DEBUG("CMD <%s>", cmd);

    tcflush(PortFD, TCIOFLUSH);

    if ( (rc = tty_write(PortFD, cmd, strlen(cmd), &nbytes_written)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("%s: %s.", __FUNCTION__, errstr);
        return false;
    }

    return true;
}

void Pyxis::TimerHit()
{
    if (!isConnected() || PowerSP[POWER_SLEEP].getState() == ISS_ON)
    {
        SetTimer(getCurrentPollingPeriod());
        return;
    }

    // Record last state
    auto currentState = GotoRotatorNP.getState();

    if (HomeRotatorSP.getState() == IPS_BUSY)
    {
        if (isMotionComplete())
        {
            currentState = IPS_OK;
            HomeRotatorSP.setState(IPS_OK);
            HomeRotatorSP[0].setState(ISS_OFF);
            HomeRotatorSP.apply();
            LOG_INFO("Homing is complete.");
        }
        else
        {
            // Fast timer
            SetTimer(getCurrentPollingPeriod());
            return;
        }
    }
    else if (GotoRotatorNP.getState() == IPS_BUSY)
    {
        if (!isMotionComplete())
        {
            LOG_DEBUG("Motion in progress.") ;
            SetTimer(POLL_100MS) ;
            return;
        }

        currentState = IPS_OK;
        LOG_INFO("Motion complete.") ;
    }

    // Update PA
    uint16_t PA = GotoRotatorNP[0].getValue();
    getPA(PA);

    // If either PA or state changed, update the property.
    if ( (PA != static_cast<uint16_t>(GotoRotatorNP[0].getValue())) || currentState != GotoRotatorNP.getState())
    {
        GotoRotatorNP[0].setValue(PA);
        GotoRotatorNP.setState(currentState);
        GotoRotatorNP.apply();
    }

    SetTimer(getCurrentPollingPeriod());
}

bool Pyxis::isMotionComplete()
{
    int nbytes_read = 0, rc = -1;
    char errstr[MAXRBUF];
    char res[PYXIS_3INCH_PER_DEG + 1] = { 0 };

    bool pyxis3inch = atoi(FirmwareTP[0].getText()) >= 3 ;

    if ( (rc = tty_nread_section(PortFD, res, (pyxis3inch ? PYXIS_3INCH_PER_DEG : PYXIS_2INCH_PER_DEG), 'F', 1,
                                 &nbytes_read)) != TTY_OK)
    {
        // '!' motion is not complete yet
        if (rc == TTY_TIME_OUT)
            return false;
        else if (rc == TTY_OVERFLOW)
        {
            LOGF_DEBUG("RES <%s>", res);

            int current = static_cast<uint16_t>(GotoRotatorNP[0].getValue()) ;

            current = current + direction ;
            if (current < 0) current = 359 ;
            if (current > 360) current = 1 ;

            GotoRotatorNP[0].setValue(current );
            GotoRotatorNP.apply();

            LOGF_DEBUG("ANGLE = %d", current) ;
            LOGF_DEBUG("TTY_OVERFLOW, nbytes_read = %d", nbytes_read) ;
            return false ;
        }

        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("%s error: %s.", __FUNCTION__, errstr);

        if (HomeRotatorSP.getState() == IPS_BUSY)
        {
            HomeRotatorSP[0].setState(ISS_OFF);
            HomeRotatorSP.setState(IPS_ALERT);
            LOG_ERROR("Homing failed. Check possible jam.");
            tcflush(PortFD, TCIOFLUSH);
        }

        return false;
    }

    LOGF_DEBUG("RES <%s>", res);
    return true;
}

#if 0
bool Pyxis::isMotionComplete()
{
    int nbytes_read = 0, rc = -1;
    char errstr[MAXRBUF];
    char res[1] = { 0 };

    if ( (rc = tty_read(PortFD, res, 1, PYXIS_TIMEOUT, &nbytes_read)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("%s error: %s.", __FUNCTION__, errstr);
        return false;
    }

    LOGF_DEBUG("RES <%c>", res[0]);

    // Homing still in progress
    if (res[0] == '!')
        return false;
    // Homing is complete
    else if (res[0] == 'F')
        return true;
    // Error
    else if (HomeRotatorSP.s == IPS_BUSY)
    {
        HomeRotatorSP[0].setState(ISS_OFF);
        HomeRotatorSP.setState(IPS_ALERT);
        LOG_ERROR("Homing failed. Check possible jam.");
        tcflush(PortFD, TCIOFLUSH);
    }

    return false;
}
#endif

std::string Pyxis::getVersion()
{
    const char *cmd = "CVxxxx";
    char res[4] = {0};

    int nbytes_written = 0, nbytes_read = 0, rc = -1;
    char errstr[MAXRBUF];

    LOGF_DEBUG("CMD <%s>", cmd);

    tcflush(PortFD, TCIOFLUSH);

    if ( (rc = tty_write(PortFD, cmd, PYRIX_CMD, &nbytes_written)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("%s: %s.", __FUNCTION__, errstr);
        return std::string("");;
    }

    if ( (rc = tty_read(PortFD, res, 3, PYXIS_TIMEOUT, &nbytes_read)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("%s error: %s.", __FUNCTION__, errstr);
        return std::string("") ;
    }

    tcflush(PortFD, TCIOFLUSH);

    LOGF_DEBUG("RES <%s>", res);

    if (res[0] == '!')
        return std::string("");

    return std::string(res) ;;
}


bool Pyxis::getPA(uint16_t &PA)
{
    const char *cmd = "CGETPA";
    char res[4] = {0};

    int nbytes_written = 0, nbytes_read = 0, rc = -1;
    char errstr[MAXRBUF];

    LOGF_DEBUG("CMD <%s>", cmd);

    tcflush(PortFD, TCIOFLUSH);

    if ( (rc = tty_write(PortFD, cmd, PYRIX_CMD, &nbytes_written)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("%s: %s.", __FUNCTION__, errstr);
        return false;
    }

    if ( (rc = tty_read(PortFD, res, 3, PYXIS_TIMEOUT, &nbytes_read)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("%s error: %s.", __FUNCTION__, errstr);
        return false;
    }

    tcflush(PortFD, TCIOFLUSH);

    LOGF_DEBUG("RES <%s>", res);

    if (res[0] == '!')
        return false;

    PA = atoi(res);

    return true;
}

int Pyxis::getReverseStatus()
{
    const char *cmd = "CMREAD";
    char res[1] = {0};

    int nbytes_written = 0, nbytes_read = 0, rc = -1;
    char errstr[MAXRBUF];

    LOGF_DEBUG("CMD <%s>", cmd);

    tcflush(PortFD, TCIOFLUSH);

    if ( (rc = tty_write(PortFD, cmd, PYRIX_CMD, &nbytes_written)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("%s: %s.", __FUNCTION__, errstr);
        return -1;
    }

    if ( (rc = tty_read(PortFD, res, 1, PYXIS_TIMEOUT, &nbytes_read)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("%s error: %s.", __FUNCTION__, errstr);
        return -1;
    }

    tcflush(PortFD, TCIOFLUSH);

    LOGF_DEBUG("RES <%c>", res[0]);

    return (res[0] == '1' ? 1 : 0);
}
