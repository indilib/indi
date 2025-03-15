/*
    USB Focus V3
    Copyright (C) 2016 G. Schmidt
    Copyright (C) 2018-2023 Jarno Paananen

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

#include "usbfocusv3.h"

#include "indicom.h"

#include <cmath>
#include <cstring>
#include <memory>

#include <termios.h>
#include <unistd.h>

#define USBFOCUSV3_TIMEOUT 5

/***************************** Class USBFocusV3 *******************************/

static std::unique_ptr<USBFocusV3> usbFocusV3(new USBFocusV3());

USBFocusV3::USBFocusV3()
{
    setVersion(1, 1);
    // Can move in Absolute & Relative motions, can AbortFocuser motion, and has variable speed.
    FI::SetCapability(FOCUSER_CAN_ABS_MOVE |
                      FOCUSER_CAN_REL_MOVE |
                      FOCUSER_CAN_ABORT    |
                      FOCUSER_HAS_BACKLASH |
                      FOCUSER_HAS_VARIABLE_SPEED);
}

bool USBFocusV3::initProperties()
{
    INDI::Focuser::initProperties();

    /*** init controller parameters ***/

    direction = 0;
    stepmode  = 1;
    speed     = 3;
    stepsdeg  = 20;
    tcomp_thr = 5;
    firmware  = 0;
    maxpos    = 65535;

    /*** init driver parameters ***/

    FocusSpeedNP[0].setMin(1);
    FocusSpeedNP[0].setMax(3);
    FocusSpeedNP[0].setValue(2);

    // Step Mode
    StepModeSP[UFOPHSTEPS].fill("HALF", "Half Step", ISS_ON);
    StepModeSP[UFOPFSTEPS].fill("FULL", "Full Step", ISS_OFF);
    StepModeSP.fill(getDeviceName(), "STEP_MODE", "Step Mode", OPTIONS_TAB, IP_RW,
                    ISR_1OFMANY, 0, IPS_IDLE);

    // Direction
    RotDirSP[UFOPSDIR].fill("STANDARD", "Standard rotation", ISS_ON);
    RotDirSP[UFOPRDIR].fill("REVERSE", "Reverse rotation", ISS_OFF);
    RotDirSP.fill(getDeviceName(), "ROTATION_MODE", "Rotation Mode", OPTIONS_TAB, IP_RW,
                  ISR_1OFMANY, 0, IPS_IDLE);

    // Focuser temperature
    TemperatureNP[0].fill("TEMPERATURE", "Celsius", "%6.2f", -50., 70., 0., 0.);
    TemperatureNP.fill(getDeviceName(), "FOCUS_TEMPERATURE", "Temperature",
                       MAIN_CONTROL_TAB, IP_RO, 0, IPS_IDLE);

    // Maximum Position
    MaxPositionNP[0].fill("MAXPOSITION", "Maximum position", "%5.0f", 1., 65535., 0., 65535.);
    MaxPositionNP.fill(getDeviceName(), "FOCUS_MAXPOSITION", "Max. Position",
                       OPTIONS_TAB, IP_RW, 0, IPS_IDLE);

    // Temperature Settings
    TemperatureSettingNP[0].fill("COEFFICIENT", "Coefficient", "%3.0f", 0., 999., 1., 15.);
    TemperatureSettingNP[1].fill("THRESHOLD", "Threshold", "%3.0f", 0., 999., 1., 10.);
    TemperatureSettingNP.fill(getDeviceName(), "TEMPERATURE_SETTINGS",
                              "Temp. Settings", OPTIONS_TAB, IP_RW, 0, IPS_IDLE);

    // Temperature Compensation Sign
    TempCompSignSP[UFOPNSIGN].fill("NEGATIVE", "Negative", ISS_OFF);
    TempCompSignSP[UFOPPSIGN].fill("POSITIVE", "Positive", ISS_ON);
    TempCompSignSP.fill(getDeviceName(), "TCOMP_SIGN", "TComp. Sign", OPTIONS_TAB,
                        IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    // Compensate for temperature
    TemperatureCompensateSP[0].fill("ENABLE", "Enable", ISS_OFF);
    TemperatureCompensateSP[1].fill("DISABLE", "Disable", ISS_ON);
    TemperatureCompensateSP.fill(getDeviceName(), "TEMP_COMPENSATION",
                                 "Temp. Comp.", MAIN_CONTROL_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    FocusBacklashNP[0].setMin(-65535.);
    FocusBacklashNP[0].setMax(65535.);
    FocusBacklashNP[0].setStep(1000.);
    FocusBacklashNP[0].setValue(0.);

    // Reset
    ResetSP[0].fill("RESET", "Reset", ISS_OFF);
    ResetSP.fill(getDeviceName(), "RESET", "Reset", MAIN_CONTROL_TAB, IP_RW, ISR_1OFMANY, 0,
                 IPS_IDLE);

    // Firmware version
    FWversionNP[0].fill("FIRMWARE", "Firmware Version", "%5.0f", 0., 65535., 1., 0.);
    FWversionNP.fill(getDeviceName(), "FW_VERSION", "Firmware", OPTIONS_TAB, IP_RO, 0,
                     IPS_IDLE);

    /* Relative and absolute movement */
    FocusRelPosNP[0].setMin(0.);
    FocusRelPosNP[0].setMax(float(maxpos));
    FocusRelPosNP[0].setValue(0);
    FocusRelPosNP[0].setStep(1);

    FocusAbsPosNP[0].setMin(0.);
    FocusAbsPosNP[0].setMax(float(maxpos));
    FocusAbsPosNP[0].setValue(0);
    FocusAbsPosNP[0].setStep(1);

    addDebugControl();

    setDefaultPollingPeriod(500);

    return true;
}

bool USBFocusV3::updateProperties()
{
    INDI::Focuser::updateProperties();

    if (isConnected())
    {
        defineProperty(TemperatureNP);
        defineProperty(MaxPositionNP);
        defineProperty(StepModeSP);
        defineProperty(RotDirSP);
        defineProperty(MaxPositionNP);
        defineProperty(TemperatureSettingNP);
        defineProperty(TempCompSignSP);
        defineProperty(TemperatureCompensateSP);
        defineProperty(ResetSP);
        defineProperty(FWversionNP);

        GetFocusParams();

        loadConfig(true);

        LOG_INFO("USBFocusV3 parameters updated, focuser ready for use.");
    }
    else
    {
        deleteProperty(TemperatureNP);
        deleteProperty(MaxPositionNP);
        deleteProperty(StepModeSP);
        deleteProperty(RotDirSP);
        deleteProperty(TemperatureSettingNP);
        deleteProperty(TempCompSignSP);
        deleteProperty(TemperatureCompensateSP);
        deleteProperty(ResetSP);
        deleteProperty(FWversionNP);
    }

    return true;
}

bool USBFocusV3::Handshake()
{
    int tries = 2;

    do
    {
        if (Ack())
        {
            LOG_INFO("USBFocusV3 is online. Getting focus parameters...");
            return true;
        }
        LOG_INFO("Error retrieving data from USBFocusV3, trying resync...");
    }
    while (--tries > 0 && Resync());

    LOG_INFO("Error retrieving data from USBFocusV3, please ensure controller "
             "is powered and the port is correct.");
    return false;
}

const char *USBFocusV3::getDefaultName()
{
    return "USBFocusV3";
}

bool USBFocusV3::Resync()
{
    char cmd[]         = " "; // Illegal command to trigger error response
    int nbytes_written = 0, nbytes_read = 0, rc = -1;
    char errstr[MAXRBUF];
    char resp[UFORESLEN] = {};

    // Send up to 5 space characters and wait for error
    // response ("ER=1") after which the communication
    // is back in sync
    tcflush(PortFD, TCIOFLUSH);

    for (int resync = 0; resync < UFOCMDLEN; resync++)
    {
        LOGF_INFO("Retry %d...", resync + 1);

        if ((rc = tty_write(PortFD, cmd, 1, &nbytes_written)) != TTY_OK)
        {
            tty_error_msg(rc, errstr, MAXRBUF);
            LOGF_ERROR("Error writing resync: %s.", errstr);
            return false;
        }

        rc = tty_nread_section(PortFD, resp, UFORESLEN, '\r', 3, &nbytes_read);
        if (rc == TTY_OK && nbytes_read > 0)
        {
            // We got a response
            return true;
        }
        // We didn't get response yet, retry
    }
    LOG_ERROR("No valid resync response.");
    return false;
}

bool USBFocusV3::sendCommand(const char *cmd, char *resp)
{
    pthread_mutex_lock(&cmdlock);

    int nbytes_written = 0, nbytes_read = 0, rc = -1;
    char errstr[MAXRBUF];
    LOGF_DEBUG("CMD: %s.", cmd);

    tcflush(PortFD, TCIOFLUSH);
    if ((rc = tty_write(PortFD, cmd, strlen(cmd), &nbytes_written)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_WARN("Error writing command %s: %s.", cmd, errstr);
        pthread_mutex_unlock(&cmdlock);
        return false;
    }

    if (resp)
    {
        if ((rc = tty_nread_section(PortFD, resp, UFORESLEN, '\r', USBFOCUSV3_TIMEOUT, &nbytes_read)) != TTY_OK)
        {
            tty_error_msg(rc, errstr, MAXRBUF);
            LOGF_WARN("Error reading response for command %s: %s.", cmd, errstr);
            pthread_mutex_unlock(&cmdlock);
            return false;
        }

        // If we are moving, check for ACK * response first
        if (moving && nbytes_read > 0 && resp[0] == UFORSACK)
        {
            moving = false;
            if (nbytes_read > 1)
            {
                // Move rest of the response down one byte
                nbytes_read--;
                memmove(&resp[0], &resp[1], nbytes_read);
            }
        }

        if (nbytes_read < 2)
        {
            LOGF_WARN("Invalid response for command %s: %s.", cmd, resp);
            pthread_mutex_unlock(&cmdlock);
            return false;
        }
        // Be sure we receive the \n\r to not truncate the response
        int rc  = sscanf(resp, "%s\n\r", resp);
        if (rc <= 0)
        {
            LOGF_WARN("Invalid response for command  %s: missing cr+lf", cmd);
            pthread_mutex_unlock(&cmdlock);
            return false;
        }
        LOGF_DEBUG("RES: %s.", resp);
    }
    pthread_mutex_unlock(&cmdlock);
    return true;
}

// Special version to work around command FTAXXA, which replies without \n\r
bool USBFocusV3::sendCommandSpecial(const char *cmd, char *resp)
{
    pthread_mutex_lock(&cmdlock);

    int nbytes_written = 0, nbytes_read = 0, rc = -1;
    char errstr[MAXRBUF];
    LOGF_DEBUG("CMD: %s.", cmd);

    tcflush(PortFD, TCIOFLUSH);
    if ((rc = tty_write(PortFD, cmd, strlen(cmd), &nbytes_written)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_WARN("Error writing command %s: %s.", cmd, errstr);
        pthread_mutex_unlock(&cmdlock);
        return false;
    }

    if (resp)
    {
        // We assume answer A=x where x is 0 or 1
        if ((rc = tty_read(PortFD, resp, 3, USBFOCUSV3_TIMEOUT, &nbytes_read)) != TTY_OK)
        {
            tty_error_msg(rc, errstr, MAXRBUF);
            LOGF_WARN("Error reading response for command %s: %s.", cmd, errstr);
            pthread_mutex_unlock(&cmdlock);
            return false;
        }
        resp[nbytes_read] = '\0';
        LOGF_DEBUG("RES: %s.", resp);
    }
    pthread_mutex_unlock(&cmdlock);
    return true;
}

bool USBFocusV3::Ack()
{
    char resp[UFORESLEN] = {};
    tcflush(PortFD, TCIOFLUSH);

    if (!sendCommand(UFOCDEVID, resp))
        return false;

    if (strncmp(resp, UFOID, UFORESLEN) != 0)
    {
        LOGF_ERROR("USBFocusV3 not properly identified! Answer was: %s.", resp);
        return false;
    }
    return true;
}

bool USBFocusV3::getControllerStatus()
{
    char resp[UFORESLEN] = {};
    if (!sendCommand(UFOCREADPARAM, resp))
        return false;

    sscanf(resp, "C=%u-%u-%u-%u-%u-%u-%u", &direction, &stepmode, &speed, &stepsdeg, &tcomp_thr, &firmware, &maxpos);
    return true;
}

bool USBFocusV3::updateStepMode()
{
    StepModeSP.reset();

    if (stepmode == UFOPHSTEPS)
        StepModeSP[UFOPHSTEPS].setState(ISS_ON);
    else if (stepmode == UFOPFSTEPS)
        StepModeSP[UFOPFSTEPS].setState(ISS_ON);
    else
    {
        LOGF_ERROR("Unknown error: focuser step value (%d)", stepmode);
        return false;
    }
    return true;
}

bool USBFocusV3::updateRotDir()
{
    RotDirSP.reset();

    if (direction == UFOPSDIR)
        RotDirSP[UFOPSDIR].setState(ISS_ON);
    else if (direction == UFOPRDIR)
        RotDirSP[UFOPRDIR].setState(ISS_ON);
    else
    {
        LOGF_ERROR("Unknown error: rotation direction  (%d)", direction);
        return false;
    }

    return true;
}

bool USBFocusV3::updateTemperature()
{
    char resp[UFORESLEN] = {};
    int j = 0;

    // retry up to 5 time to fix data desyncronization.
    // see: https://bitbucket.org/usb-focus/ascom-driver/src/f0ec7d0faee605f9a3d9e2c4d599f9d537211201/USB_Focus/Driver.cs?at=master&fileviewer=file-view-default#Driver.cs-898

    while (j <= 5)
    {
        if (sendCommand(UFOCREADTEMP, resp))
        {

            float temp;
            int rc = sscanf(resp, "T=%f", &temp);

            if (rc > 0)
            {
                TemperatureNP[0].setValue(temp);
                break;
            }
            else
            {
                LOGF_DEBUG("Unknown error: focuser temperature value (%s)", resp);
            }
        }
        j++;
    }
    if (j > 5)
    {
        LOGF_ERROR("Unknown error: focuser temperature value (%s)", resp);
        return false;
    }

    return true;
}

bool USBFocusV3::updateFWversion()
{
    FWversionNP[0].setValue(firmware);
    return true;
}

bool USBFocusV3::updatePosition()
{
    char resp[UFORESLEN] = {};
    int j = 0;

    // retry up to 5 time to fix data desyncronization.
    // see: https://bitbucket.org/usb-focus/ascom-driver/src/f0ec7d0faee605f9a3d9e2c4d599f9d537211201/USB_Focus/Driver.cs?at=master&fileviewer=file-view-default#Driver.cs-746

    while (j <= 5)
    {
        if (sendCommand(UFOCREADPOS, resp))
        {

            int pos = -1;
            int rc  = sscanf(resp, "P=%u", &pos);

            if (rc > 0)
            {
                FocusAbsPosNP[0].setValue(pos);
                break;
            }
            else
            {
                LOGF_DEBUG("Unknown error: focuser position value (%s)", resp);
            }
        }
        j++;
    }
    if (j > 5)
    {
        LOGF_ERROR("Unknown error: focuser position value (%s)", resp);
        return false;
    }

    return true;
}

bool USBFocusV3::updateMaxPos()
{
    MaxPositionNP[0].setValue(maxpos);
    FocusAbsPosNP[0].setMax(maxpos);
    return true;
}

bool USBFocusV3::updateTempCompSettings()
{
    TemperatureSettingNP[0].setValue(stepsdeg);
    TemperatureSettingNP[1].setValue(tcomp_thr);
    return true;
}

bool USBFocusV3::updateTempCompSign()
{
    char resp[UFORESLEN] = {};
    // This command seems to have a bug in firmware 1505 that it
    // doesn't send \n\r in reply like all others except movement
    // commands so use a special version for it
    if (!sendCommandSpecial(UFOCGETSIGN, resp))
        return false;

    unsigned int sign;
    int rc = sscanf(resp, "A=%u", &sign);

    if (rc > 0)
    {
        TempCompSignSP.reset();

        if (sign == UFOPNSIGN)
            TempCompSignSP[UFOPNSIGN].setState(ISS_ON);
        else if (sign == UFOPPSIGN)
            TempCompSignSP[UFOPPSIGN].setState(ISS_ON);
        else
        {
            LOGF_ERROR("Unknown error: temp. comp. sign  (%d)", sign);
            return false;
        }
    }
    else
    {
        LOGF_ERROR("Unknown error: temp. comp. sign value (%s)", resp);
        return false;
    }

    return true;
}

bool USBFocusV3::updateSpeed()
{
    int drvspeed;

    switch (speed)
    {
        case UFOPSPDAV:
            drvspeed = 3;
            break;
        case UFOPSPDSL:
            drvspeed = 2;
            break;
        case UFOPSPDUS:
            drvspeed = 1;
            break;
        default:
            drvspeed = 0;
            break;
    }

    if (drvspeed != 0)
    {
        currentSpeed         = drvspeed;
        FocusSpeedNP[0].setValue(drvspeed);
    }
    else
    {
        LOGF_ERROR("Unknown error: focuser speed value (%d)", speed);
        return false;
    }

    return true;
}

bool USBFocusV3::setAutoTempCompThreshold(unsigned int thr)
{
    char cmd[UFOCMDLEN + 1];
    snprintf(cmd, UFOCMDLEN + 1, UFOCSETTCTHR, thr);

    char resp[UFORESLEN] = {};
    if (!sendCommand(cmd, resp))
        return false;

    if (strncmp(resp, UFORSDONE, strlen(UFORSDONE)) == 0)
    {
        tcomp_thr = thr;
        return true;
    }

    LOG_ERROR("setAutoTempCompThreshold error: did not receive DONE.");

    return false;
}

bool USBFocusV3::setTemperatureCoefficient(unsigned int coefficient)
{
    char cmd[UFOCMDLEN + 1];
    snprintf(cmd, UFOCMDLEN + 1, UFOCSETSTDEG, coefficient);

    char resp[UFORESLEN] = {};
    if (!sendCommand(cmd, resp))
        return false;

    if (strncmp(resp, UFORSDONE, strlen(UFORSDONE)) == 0)
    {
        stepsdeg = coefficient;
        return true;
    }

    LOG_ERROR("setTemperatureCoefficient error: did not receive DONE.");

    return false;
}

bool USBFocusV3::reset()
{
    char resp[UFORESLEN] = {};
    if (!sendCommand(UFOCRESET, resp))
        return false;

    GetFocusParams();

    return true;
}

bool USBFocusV3::MoveFocuserUF(FocusDirection dir, unsigned int rticks)
{
    char cmd[UFOCMDLEN + 1];

    unsigned int ticks;

    if ((dir == FOCUS_INWARD) && (rticks > FocusAbsPosNP[0].getValue()))
    {
        ticks = FocusAbsPosNP[0].getValue();
        LOGF_WARN("Requested %u ticks but inward movement has been limited to %u ticks", rticks, ticks);
    }
    else if ((dir == FOCUS_OUTWARD) && ((FocusAbsPosNP[0].getValue() + rticks) > MaxPositionNP[0].getValue()))
    {
        ticks = MaxPositionNP[0].getValue() - FocusAbsPosNP[0].getValue();
        LOGF_WARN("Requested %u ticks but outward movement has been limited to %u ticks", rticks, ticks);
    }
    else
    {
        ticks = rticks;
    }

    if (dir == FOCUS_INWARD)
    {
        if (backlashMove == false && backlashIn == true && backlashSteps != 0)
        {
            ticks += backlashSteps;
            backlashTargetPos = targetPos - backlashSteps;
            backlashMove      = true;
        }
        snprintf(cmd, UFOCMDLEN + 1, UFOCMOVEIN, ticks);
    }
    else
    {
        if (backlashMove == false && backlashIn == false && backlashSteps != 0)
        {
            ticks += backlashSteps;
            backlashTargetPos = targetPos + backlashSteps;
            backlashMove      = true;
        }
        snprintf(cmd, UFOCMDLEN + 1, UFOCMOVEOUT, ticks);
    }
    moving = true;
    return sendCommand(cmd, nullptr);
}

bool USBFocusV3::setStepMode(FocusStepMode mode)
{
    char resp[UFORESLEN] = {};
    char cmd[UFOCMDLEN + 1];

    if (mode == FOCUS_HALF_STEP)
        snprintf(cmd, UFOCMDLEN + 1, "%s", UFOCSETHSTEPS);
    else
        snprintf(cmd, UFOCMDLEN + 1, "%s", UFOCSETFSTEPS);

    if (!sendCommand(cmd, resp))
        return false;

    stepmode = mode;
    return true;
}

bool USBFocusV3::setRotDir(unsigned int dir)
{
    char resp[UFORESLEN] = {};
    char cmd[UFOCMDLEN + 1];

    if (dir == UFOPSDIR)
        snprintf(cmd, UFOCMDLEN + 1, "%s", UFOCSETSDIR);
    else
        snprintf(cmd, UFOCMDLEN + 1, "%s", UFOCSETRDIR);

    if (!sendCommand(cmd, resp))
        return false;

    direction = dir;
    return true;
}

bool USBFocusV3::setMaxPos(unsigned int maxp)
{
    char cmd[UFOCMDLEN + 1];
    char resp[UFORESLEN] = {};

    if (maxp >= 1 && maxp <= 65535)
    {
        snprintf(cmd, UFOCMDLEN + 1, UFOCSETMAX, maxp);
    }
    else
    {
        LOGF_ERROR("Focuser max. pos. value %d out of bounds", maxp);
        return false;
    }

    if (!sendCommand(cmd, resp))
        return false;

    if (strncmp(resp, UFORSDONE, strlen(UFORSDONE)) == 0)
    {
        maxpos              = maxp;
        FocusAbsPosNP[0].setMax(maxpos);
        return true;
    }

    LOG_ERROR("setMaxPos error: did not receive DONE.");

    return false;
}

bool USBFocusV3::setSpeed(unsigned short drvspeed)
{
    char cmd[UFOCMDLEN + 1];
    char resp[UFORESLEN] = {};

    unsigned int spd;

    switch (drvspeed)
    {
        case 3:
            spd = UFOPSPDAV;
            break;
        case 2:
            spd = UFOPSPDSL;
            break;
        case 1:
            spd = UFOPSPDUS;
            break;
        default:
            spd = UFOPSPDERR;
            break;
    }

    if (spd != UFOPSPDERR)
    {
        snprintf(cmd, UFOCMDLEN + 1, UFOCSETSPEED, spd);
    }
    else
    {
        LOGF_ERROR("Focuser speed value %d out of bounds", drvspeed);
        return false;
    }

    if (!sendCommand(cmd, resp))
        return false;

    if (strncmp(resp, UFORSDONE, strlen(UFORSDONE)) == 0)
    {
        speed = spd;
        return true;
    }

    LOG_ERROR("setSpeed error: did not receive DONE.");

    return false;
}

bool USBFocusV3::setTemperatureCompensation(bool enable)
{
    char cmd[UFOCMDLEN + 1];
    char resp[UFORESLEN] = {};

    if (enable)
        snprintf(cmd, UFOCMDLEN + 1, "%s", UFOCSETAUTO);
    else
        snprintf(cmd, UFOCMDLEN + 1, "%s", UFOCSETMANU);

    if (!sendCommand(cmd, resp))
        return false;

    return true;
}

bool USBFocusV3::setTempCompSign(unsigned int sign)
{
    char cmd[UFOCMDLEN + 1];
    char resp[UFORESLEN] = {};

    snprintf(cmd, UFOCMDLEN + 1, UFOCSETSIGN, sign);

    if (!sendCommand(cmd, resp))
        return false;

    if (strncmp(resp, UFORSDONE, strlen(UFORSDONE)) == 0)
    {
        return true;
    }

    LOG_ERROR("setTempCompSign error: did not receive DONE.");

    return false;
}

bool USBFocusV3::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if (!dev || strcmp(dev, getDeviceName()))
        return false;

    if (StepModeSP.isNameMatch(name))
    {
        int current_mode = StepModeSP.findOnSwitchIndex();
        StepModeSP.update(states, names, n);
        int target_mode = StepModeSP.findOnSwitchIndex();
        if (current_mode != target_mode)
        {
            bool rc;
            if (target_mode == UFOPHSTEPS)
                rc = setStepMode(FOCUS_HALF_STEP);
            else
                rc = setStepMode(FOCUS_FULL_STEP);

            if (!rc)
            {
                StepModeSP.reset();
                StepModeSP[current_mode].setState(ISS_ON);
                StepModeSP.setState(IPS_ALERT);
                StepModeSP.apply();
                return false;
            }
        }
        StepModeSP.setState(IPS_OK);
        StepModeSP.apply();
        return true;
    }

    if (RotDirSP.isNameMatch(name))
    {
        int current_mode = RotDirSP.findOnSwitchIndex();
        RotDirSP.update(states, names, n);
        int target_mode = RotDirSP.findOnSwitchIndex();
        if (current_mode != target_mode)
        {
            if (!setRotDir(target_mode))
            {
                RotDirSP.reset();
                RotDirSP[current_mode].setState(ISS_ON);
                RotDirSP.setState(IPS_ALERT);
                RotDirSP.apply();
                return false;
            }
        }

        RotDirSP.setState(IPS_OK);
        RotDirSP.apply();
        return true;
    }

    if (TemperatureCompensateSP.isNameMatch(name))
    {
        int last_index = TemperatureCompensateSP.findOnSwitchIndex();
        TemperatureCompensateSP.update(states, names, n);
        int target_index = TemperatureCompensateSP.findOnSwitchIndex();

        if (last_index != target_index)
        {
            if (!setTemperatureCompensation((TemperatureCompensateSP[0].getState() == ISS_ON)))
            {
                TemperatureCompensateSP.setState(IPS_ALERT);
                TemperatureCompensateSP.reset();
                TemperatureCompensateSP[last_index].setState(ISS_ON);
                TemperatureCompensateSP.apply();
                return false;
            }
        }
        TemperatureCompensateSP.setState(IPS_OK);
        TemperatureCompensateSP.apply();
        return true;
    }

    if (TempCompSignSP.isNameMatch(name))
    {
        int current_mode = TempCompSignSP.findOnSwitchIndex();
        TempCompSignSP.update(states, names, n);
        int target_mode = TempCompSignSP.findOnSwitchIndex();
        if (current_mode != target_mode)
        {
            if (!setTempCompSign(target_mode))
            {
                TempCompSignSP.reset();
                TempCompSignSP[current_mode].setState(ISS_ON);
                TempCompSignSP.setState(IPS_ALERT);
                TempCompSignSP.apply();
                return false;
            }
        }
        TempCompSignSP.setState(IPS_OK);
        TempCompSignSP.apply();
        return true;
    }

    if (ResetSP.isNameMatch(name))
    {
        ResetSP.reset();

        if (reset())
            ResetSP.setState(IPS_OK);
        else
            ResetSP.setState(IPS_ALERT);

        ResetSP.apply();
        return true;
    }
    return INDI::Focuser::ISNewSwitch(dev, name, states, names, n);
}

bool USBFocusV3::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    if (!dev || strcmp(dev, getDeviceName()))
        return false;

    if (MaxPositionNP.isNameMatch(name))
    {
        MaxPositionNP.update(values, names, n);
        if (!setMaxPos(MaxPositionNP[0].getValue()))
        {
            MaxPositionNP.setState(IPS_ALERT);
            MaxPositionNP.apply();
            return false;
        }
        MaxPositionNP.setState(IPS_OK);
        MaxPositionNP.apply();
        return true;
    }

    if (TemperatureSettingNP.isNameMatch(name))
    {
        TemperatureSettingNP.update(values, names, n);
        if (!setAutoTempCompThreshold(TemperatureSettingNP[1].getValue()) ||
                !setTemperatureCoefficient(TemperatureSettingNP[UFOPNSIGN].getValue()))
        {
            TemperatureSettingNP.setState(IPS_ALERT);
            TemperatureSettingNP.apply();
            return false;
        }

        TemperatureSettingNP.setState(IPS_OK);
        TemperatureSettingNP.apply();
        return true;
    }
    return INDI::Focuser::ISNewNumber(dev, name, values, names, n);
}

void USBFocusV3::GetFocusParams()
{
    getControllerStatus();

    if (updatePosition())
        FocusAbsPosNP.apply();

    if (updateMaxPos())
    {
        MaxPositionNP.apply();
        FocusAbsPosNP.apply();
    }

    if (updateTemperature())
        TemperatureNP.apply();

    if (updateTempCompSettings())
        TemperatureSettingNP.apply();

    if (updateTempCompSign())
        TempCompSignSP.apply();

    if (updateSpeed())
        FocusSpeedNP.apply();

    if (updateStepMode())
        StepModeSP.apply();

    if (updateRotDir())
        RotDirSP.apply();

    if (updateFWversion())
        FWversionNP.apply();
}

bool USBFocusV3::SetFocuserSpeed(int speed)
{
    if (!setSpeed(speed))
        return false;

    currentSpeed = speed;

    FocusSpeedNP.setState(IPS_OK);
    FocusSpeedNP.apply();

    return true;
}

IPState USBFocusV3::MoveAbsFocuser(uint32_t targetTicks)
{
    long ticks;

    targetPos = targetTicks;

    ticks = targetPos - FocusAbsPosNP[0].getValue();

    bool rc = false;

    if (ticks < 0)
        rc = MoveFocuserUF(FOCUS_INWARD, -ticks);
    else if (ticks > 0)
        rc = MoveFocuserUF(FOCUS_OUTWARD, ticks);

    if (!rc)
        return IPS_ALERT;

    FocusAbsPosNP.setState(IPS_BUSY);

    return IPS_BUSY;
}

IPState USBFocusV3::MoveRelFocuser(FocusDirection dir, uint32_t ticks)
{
    uint32_t aticks;

    if ((dir == FOCUS_INWARD) && (ticks > FocusAbsPosNP[0].getValue()))
    {
        aticks = FocusAbsPosNP[0].getValue();
        DEBUGF(INDI::Logger::DBG_WARNING,
               "Requested %u ticks but relative inward movement has been limited to %u ticks", ticks, aticks);
        ticks = aticks;
    }
    else if ((dir == FOCUS_OUTWARD) && ((FocusAbsPosNP[0].getValue() + ticks) > MaxPositionNP[0].getValue()))
    {
        aticks = MaxPositionNP[0].getValue() - FocusAbsPosNP[0].getValue();
        DEBUGF(INDI::Logger::DBG_WARNING,
               "Requested %u ticks but relative outward movement has been limited to %u ticks", ticks, aticks);
        ticks = aticks;
    }

    if (!MoveFocuserUF(dir, (unsigned int)ticks))
        return IPS_ALERT;

    FocusRelPosNP[0].setValue(ticks);
    FocusRelPosNP.setState(IPS_BUSY);

    return IPS_BUSY;
}

void USBFocusV3::TimerHit()
{
    if (!isConnected())
    {
        return;
    }

    if (updatePosition())
    {
        if (fabs(lastPos - FocusAbsPosNP[0].getValue()) > 5)
        {
            FocusAbsPosNP.apply();
            lastPos = FocusAbsPosNP[0].getValue();
        }
    }

    if (updateTemperature())
    {
        if (fabs(lastTemperature - TemperatureNP[0].getValue()) >= 0.5)
        {
            TemperatureNP.apply();
            lastTemperature = TemperatureNP[0].getValue();
        }
    }

    if (FocusTimerNP.getState() == IPS_BUSY)
    {
        float remaining = CalcTimeLeft(focusMoveStart, focusMoveRequest);

        if (remaining <= 0)
        {
            FocusTimerNP.setState(IPS_OK);
            FocusTimerNP[0].setValue(0);
            AbortFocuser();
        }
        else
            FocusTimerNP[0].setValue(remaining * 1000.0);

        FocusTimerNP.apply();
    }

    if (FocusAbsPosNP.getState() == IPS_BUSY || FocusRelPosNP.getState() == IPS_BUSY)
    {
        if (backlashMove && (fabs(backlashTargetPos - FocusAbsPosNP[0].getValue()) < 1))
        {
            // Backlash target reached, now go to real target
            MoveAbsFocuser(targetPos);
            backlashMove = false;
        }
        else
        {
            if (fabs(targetPos - FocusAbsPosNP[0].getValue()) < 1)
            {
                FocusAbsPosNP.setState(IPS_OK);
                FocusRelPosNP.setState(IPS_OK);
                FocusAbsPosNP.apply();
                FocusRelPosNP.apply();
                lastPos = FocusAbsPosNP[0].getValue();
                LOG_INFO("Focuser reached requested position.");
            }
        }
    }

    SetTimer(getCurrentPollingPeriod());
}

bool USBFocusV3::AbortFocuser()
{
    char resp[UFORESLEN] = {};
    if (!sendCommand(UFOCABORT, resp))
        return false;

    FocusAbsPosNP.setState(IPS_IDLE);
    FocusRelPosNP.setState(IPS_IDLE);
    FocusAbsPosNP.apply();
    FocusRelPosNP.apply();
    backlashMove = false;
    moving       = false;
    return true;
}

float USBFocusV3::CalcTimeLeft(timeval start, float req)
{
    double timesince;
    double timeleft;
    struct timeval now
    {
        0, 0
    };
    gettimeofday(&now, nullptr);

    timesince =
        (double)(now.tv_sec * 1000.0 + now.tv_usec / 1000) - (double)(start.tv_sec * 1000.0 + start.tv_usec / 1000);
    timesince = timesince / 1000;
    timeleft  = req - timesince;
    return timeleft;
}

bool USBFocusV3::SetFocuserBacklash(int32_t steps)
{
    backlashIn = steps < 0;
    backlashSteps = std::abs(steps);

    return true;
}
