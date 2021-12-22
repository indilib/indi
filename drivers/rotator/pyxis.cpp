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
    // We do not have absolute ticks
    RI::SetCapability(ROTATOR_CAN_HOME | ROTATOR_CAN_REVERSE);

    setRotatorConnection(CONNECTION_SERIAL);
}

bool Pyxis::initProperties()
{
    INDI::Rotator::initProperties();

    // Rotation Rate
    IUFillNumber(&RotationRateN[0], "RATE", "Rate", "%.f", 0, 99, 10, 8);
    IUFillNumberVector(&RotationRateNP, RotationRateN, 1, getDeviceName(), "ROTATION_RATE", "Rotation", SETTINGS_TAB, IP_RW, 0, IPS_IDLE);

    // Stepping
    IUFillSwitch(&SteppingS[FULL_STEP], "FULL_STEP", "Full", ISS_OFF);
    IUFillSwitch(&SteppingS[HALF_STEP], "HALF_STEP", "Half", ISS_OFF);
    IUFillSwitchVector(&SteppingSP, SteppingS, 2, getDeviceName(), "STEPPING_RATE", "Stepping", SETTINGS_TAB, IP_RW, ISR_ATMOST1, 0, IPS_IDLE);

    // Power
    IUFillSwitch(&PowerS[POWER_SLEEP], "POWER_SLEEP", "Sleep", ISS_OFF);
    IUFillSwitch(&PowerS[POWER_WAKEUP], "POWER_WAKEUP", "Wake Up", ISS_OFF);
    IUFillSwitchVector(&PowerSP, PowerS, 2, getDeviceName(), "POWER_STATE", "Power", SETTINGS_TAB, IP_RW, ISR_ATMOST1, 0, IPS_IDLE);

    // Firmware version
    IUFillText(&FirmwareT[0], "FIRMWARE_VERSION", "Version", "Unknown");
    IUFillTextVector(&FirmwareTP, FirmwareT, 1, getDeviceName(), "FIRMWARE_VERSION", "Firmware", INFO_TAB, IP_RO, 0, IPS_IDLE);

    // Firmware version
    IUFillText(&ModelT[0], "HARDWARE_MODEL", "Model", "Unknown");
    IUFillTextVector(&ModelTP, ModelT, 1, getDeviceName(), "HARDWARE_MODEL", "Model", INFO_TAB, IP_RO, 0, IPS_IDLE);


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
        defineProperty(&RotationRateNP)  ;
        defineProperty(&SteppingSP);
        defineProperty(&PowerSP);
        defineProperty(&FirmwareTP) ;
        defineProperty(&ModelTP) ;

        queryParams();
    }
    else
    {
        deleteProperty(RotationRateNP.name);
        deleteProperty(SteppingSP.name);
        deleteProperty(PowerSP.name);
        deleteProperty(FirmwareTP.name) ;
        deleteProperty(ModelTP.name) ;
    }

    return true;
}

void Pyxis::queryParams()
{
    ////////////////////////////////////////////
    // Reverse Parameter
    ////////////////////////////////////////////
    int dir = getReverseStatus();

    IUResetSwitch(&ReverseRotatorSP);
    ReverseRotatorSP.s = IPS_OK;
    if (dir == 0)
        ReverseRotatorS[INDI_DISABLED].s = ISS_ON;
    else if (dir == 1)
        ReverseRotatorS[INDI_ENABLED].s = ISS_ON;
    else
        ReverseRotatorSP.s = IPS_ALERT;

    IDSetSwitch(&ReverseRotatorSP, nullptr);

    // Firmware version parameter
    std::string sversion = getVersion() ;
    IUSaveText(&FirmwareT[0], sversion.c_str()) ;
    FirmwareTP.s = IPS_OK;
    IDSetText(&FirmwareTP, nullptr) ;

    LOGF_DEBUG("queryParms firmware = %s", sversion.c_str()) ;

    // Firmware tells us device type, 3 inch or 2 inch, which defines the correct default rotation rate
    if (atof(sversion.c_str()) >= 3)
    {
        uint16_t rate = (atof(sversion.c_str()) >= 3 ? PYXIS_3INCH_RATE : PYXIS_2INCH_RATE) ;
        bool rc = setRotationRate(rate) ;
        LOGF_DEBUG("queryParms rate = %d, firmware = %s", rate, sversion.c_str()) ;
        if (rc)
        {
            RotationRateNP.s = IPS_OK ;
            RotationRateN[0].value = rate ;
            IDSetNumber(&RotationRateNP, nullptr) ;

            IUSaveText(&ModelT[0], "Pyxis 3 Inch") ;
            ModelTP.s = IPS_OK;
            IDSetText(&ModelTP, nullptr)  ;
        }
    }
    else
    {
        IUSaveText(&ModelT[0], "Pyxis 2 Inch") ;
        ModelTP.s = IPS_OK;
        IDSetText(&ModelTP, nullptr) ;
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
        if (!strcmp(name, RotationRateNP.name))
        {
            bool rc = setRotationRate(static_cast<uint8_t>(values[0]));
            if (rc)
            {
                RotationRateNP.s = IPS_OK;
                RotationRateN[0].value = values[0];
            }
            else
                RotationRateNP.s = IPS_ALERT;

            IDSetNumber(&RotationRateNP, nullptr);
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
        if (!strcmp(name, SteppingSP.name))
        {
            bool rc = false;
            if (!strcmp(IUFindOnSwitchName(states, names, n), SteppingS[FULL_STEP].name))
                rc = setSteppingMode(FULL_STEP);
            else
                rc = setSteppingMode(HALF_STEP);


            if (rc)
            {
                IUUpdateSwitch(&SteppingSP, states, names, n);
                SteppingSP.s = IPS_OK;
            }
            else
                SteppingSP.s = IPS_ALERT;

            IDSetSwitch(&SteppingSP, nullptr);
            return true;
        }

        /////////////////////////////////////////////////////
        // Power
        ////////////////////////////////////////////////////
        if (!strcmp(name, PowerSP.name))
        {
            bool rc = false;
            if (!strcmp(IUFindOnSwitchName(states, names, n), PowerS[POWER_WAKEUP].name))
            {
                // If not sleeping
                if (PowerS[POWER_SLEEP].s == ISS_OFF)
                {
                    PowerSP.s = IPS_OK;
                    LOG_WARN("Controller is not in sleep mode.");
                    IDSetSwitch(&PowerSP, nullptr);
                    return true;
                }

                rc = wakeupController();

                if (rc)
                {
                    IUResetSwitch(&PowerSP);
                    PowerSP.s = IPS_OK;
                    LOG_INFO("Controller is awake.");
                }
                else
                    PowerSP.s = IPS_ALERT;

                IDSetSwitch(&PowerSP, nullptr);
                return true;
            }
            else
            {
                bool rc = sleepController();
                IUResetSwitch(&PowerSP);
                if (rc)
                {
                    PowerSP.s = IPS_OK;
                    PowerS[POWER_SLEEP].s = ISS_ON;
                    LOG_INFO("Controller in sleep mode. No functions can be used until controller is waken up.");
                }
                else
                    PowerSP.s = IPS_ALERT;

                IDSetSwitch(&PowerSP, nullptr);
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

    uint16_t current = static_cast<uint16_t>(GotoRotatorN[0].value) ;

    targetPA = static_cast<uint16_t>(round(angle));

    if (targetPA > 359)
        targetPA = 0;

    // Rotator will only rotation +-180 degress from home (0 degrees) so it make take
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
    if (!isConnected() || PowerS[POWER_SLEEP].s == ISS_ON)
    {
        SetTimer(getCurrentPollingPeriod());
        return;
    }

    if (HomeRotatorSP.s == IPS_BUSY)
    {
        if (isMotionComplete())
        {
            HomeRotatorSP.s = IPS_OK;
            HomeRotatorS[0].s = ISS_OFF;
            IDSetSwitch(&HomeRotatorSP, nullptr);
            LOG_INFO("Homing is complete.");
        }
        else
        {
            // Fast timer
            SetTimer(getCurrentPollingPeriod());
            return;
        }
    }
    else if (GotoRotatorNP.s == IPS_BUSY)
    {
        if (!isMotionComplete())
        {
            LOGF_DEBUG("Motion in %s", "progress") ;
            SetTimer(POLL_100MS) ;
            return ;
        }
        GotoRotatorNP.s = IPS_OK;
    }

    uint16_t PA = 0;
    if (getPA(PA) && (PA != static_cast<uint16_t>(GotoRotatorN[0].value)))
    {
        GotoRotatorN[0].value = PA;
        IDSetNumber(&GotoRotatorNP, nullptr);
    }

    SetTimer(getCurrentPollingPeriod());
}

bool Pyxis::isMotionComplete()
{
    int nbytes_read = 0, rc = -1;
    char errstr[MAXRBUF];
    char res[PYXIS_3INCH_PER_DEG + 1] = { 0 };

    bool pyxis3inch = atoi(FirmwareT[0].text) >= 3 ;

    if ( (rc = tty_nread_section(PortFD, res, (pyxis3inch ? PYXIS_3INCH_PER_DEG : PYXIS_2INCH_PER_DEG), 'F', 1, &nbytes_read)) != TTY_OK)
    {
        // '!' motion is not complete yet
        if (rc == TTY_TIME_OUT)
            return false;
        else if (rc == TTY_OVERFLOW)
        {
            LOGF_DEBUG("RES <%s>", res);

            int current = static_cast<uint16_t>(GotoRotatorN[0].value) ;

            current = current + direction ;
            if (current < 0) current = 359 ;
            if (current > 360) current = 1 ;

            GotoRotatorN[0].value = current ;
            IDSetNumber(&GotoRotatorNP, nullptr);

            LOGF_DEBUG("ANGLE = %d", current) ;
            LOGF_DEBUG("TTY_OVERFLOW, nbytes_read = %d", nbytes_read) ;
            return false ;
        }

        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("%s error: %s.", __FUNCTION__, errstr);

        if (HomeRotatorSP.s == IPS_BUSY)
        {
            HomeRotatorS[0].s = ISS_OFF;
            HomeRotatorSP.s = IPS_ALERT;
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
        HomeRotatorS[0].s = ISS_OFF;
        HomeRotatorSP.s = IPS_ALERT;
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
