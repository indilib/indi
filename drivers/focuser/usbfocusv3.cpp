/*
    USB Focus V3
    Copyright (C) 2016 G. Schmidt
    Copyright (C) 2018 Jarno Paananen

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

void ISGetProperties(const char *dev)
{
    usbFocusV3->ISGetProperties(dev);
}

void ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    usbFocusV3->ISNewSwitch(dev, name, states, names, n);
}

void ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n)
{
    usbFocusV3->ISNewText(dev, name, texts, names, n);
}

void ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    usbFocusV3->ISNewNumber(dev, name, values, names, n);
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
    usbFocusV3->ISSnoopDevice(root);
}

USBFocusV3::USBFocusV3()
{
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

    FocusSpeedN[0].min   = 1;
    FocusSpeedN[0].max   = 3;
    FocusSpeedN[0].value = 2;

    // Step Mode
    IUFillSwitch(&StepModeS[UFOPHSTEPS], "HALF", "Half Step", ISS_ON);
    IUFillSwitch(&StepModeS[UFOPFSTEPS], "FULL", "Full Step", ISS_OFF);
    IUFillSwitchVector(&StepModeSP, StepModeS, 2, getDeviceName(), "STEP_MODE", "Step Mode", OPTIONS_TAB, IP_RW,
                       ISR_1OFMANY, 0, IPS_IDLE);

    // Direction
    IUFillSwitch(&RotDirS[UFOPSDIR], "STANDARD", "Standard rotation", ISS_ON);
    IUFillSwitch(&RotDirS[UFOPRDIR], "REVERSE", "Reverse rotation", ISS_OFF);
    IUFillSwitchVector(&RotDirSP, RotDirS, 2, getDeviceName(), "ROTATION_MODE", "Rotation Mode", OPTIONS_TAB, IP_RW,
                       ISR_1OFMANY, 0, IPS_IDLE);

    // Focuser temperature
    IUFillNumber(&TemperatureN[0], "TEMPERATURE", "Celsius", "%6.2f", -50., 70., 0., 0.);
    IUFillNumberVector(&TemperatureNP, TemperatureN, 1, getDeviceName(), "FOCUS_TEMPERATURE", "Temperature",
                       MAIN_CONTROL_TAB, IP_RO, 0, IPS_IDLE);

    // Maximum Position
    IUFillNumber(&MaxPositionN[0], "MAXPOSITION", "Maximum position", "%5.0f", 1., 65535., 0., 65535.);
    IUFillNumberVector(&MaxPositionNP, MaxPositionN, 1, getDeviceName(), "FOCUS_MAXPOSITION", "Max. Position",
                       OPTIONS_TAB, IP_RW, 0, IPS_IDLE);

    // Temperature Settings
    IUFillNumber(&TemperatureSettingN[0], "COEFFICIENT", "Coefficient", "%3.0f", 0., 999., 1., 15.);
    IUFillNumber(&TemperatureSettingN[1], "THRESHOLD", "Threshold", "%3.0f", 0., 999., 1., 10.);
    IUFillNumberVector(&TemperatureSettingNP, TemperatureSettingN, 2, getDeviceName(), "TEMPERATURE_SETTINGS",
                       "Temp. Settings", OPTIONS_TAB, IP_RW, 0, IPS_IDLE);

    // Temperature Compensation Sign
    IUFillSwitch(&TempCompSignS[UFOPNSIGN], "NEGATIVE", "Negative", ISS_OFF);
    IUFillSwitch(&TempCompSignS[UFOPPSIGN], "POSITIVE", "Positive", ISS_ON);
    IUFillSwitchVector(&TempCompSignSP, TempCompSignS, 2, getDeviceName(), "TCOMP_SIGN", "TComp. Sign", OPTIONS_TAB,
                       IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    // Compensate for temperature
    IUFillSwitch(&TemperatureCompensateS[0], "ENABLE", "Enable", ISS_OFF);
    IUFillSwitch(&TemperatureCompensateS[1], "DISABLE", "Disable", ISS_ON);
    IUFillSwitchVector(&TemperatureCompensateSP, TemperatureCompensateS, 2, getDeviceName(), "TEMP_COMPENSATION",
                       "Temp. Comp.", MAIN_CONTROL_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    // Compensate for temperature
    //    IUFillSwitch(&BacklashDirectionS[BACKLASH_IN], "IN", "In", ISS_OFF);
    //    IUFillSwitch(&BacklashDirectionS[BACKLASH_OUT], "OUT", "Out", ISS_ON);
    //    IUFillSwitchVector(&BacklashDirectionSP, BacklashDirectionS, 2, getDeviceName(), "BACKLASH_DIRECTION",
    //                       "Backlash direction", OPTIONS_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    //    // Backlash compensation steps
    //    IUFillNumber(&BacklashSettingN[0], "FOCUS_BACKLASH_VALUE", "Steps", "%5.0f", 0., 65535., 1., 0.);
    //    IUFillNumberVector(&BacklashSettingNP, BacklashSettingN, 1, getDeviceName(), "FOCUS_BACKLASH_STEPS", "Backlash steps",
    //                       OPTIONS_TAB, IP_RW, 0, IPS_IDLE);

    FocusBacklashN[0].min = -65535.;
    FocusBacklashN[0].max = 65535.;
    FocusBacklashN[0].step = 1000.;
    FocusBacklashN[0].value = 0.;

    // Reset
    IUFillSwitch(&ResetS[0], "RESET", "Reset", ISS_OFF);
    IUFillSwitchVector(&ResetSP, ResetS, 1, getDeviceName(), "RESET", "Reset", MAIN_CONTROL_TAB, IP_RW, ISR_1OFMANY, 0,
                       IPS_IDLE);

    // Firmware version
    IUFillNumber(&FWversionN[0], "FIRMWARE", "Firmware Version", "%5.0f", 0., 65535., 1., 0.);
    IUFillNumberVector(&FWversionNP, FWversionN, 1, getDeviceName(), "FW_VERSION", "Firmware", OPTIONS_TAB, IP_RO, 0,
                       IPS_IDLE);

    /* Relative and absolute movement */
    FocusRelPosN[0].min   = 0.;
    FocusRelPosN[0].max   = float(maxpos);
    FocusRelPosN[0].value = 0;
    FocusRelPosN[0].step  = 1;

    FocusAbsPosN[0].min   = 0.;
    FocusAbsPosN[0].max   = float(maxpos);
    FocusAbsPosN[0].value = 0;
    FocusAbsPosN[0].step  = 1;

    addDebugControl();

    setDefaultPollingPeriod(500);

    return true;
}

bool USBFocusV3::updateProperties()
{
    INDI::Focuser::updateProperties();

    if (isConnected())
    {
        defineNumber(&TemperatureNP);
        defineNumber(&MaxPositionNP);
        defineSwitch(&StepModeSP);
        defineSwitch(&RotDirSP);
        defineNumber(&MaxPositionNP);
        defineNumber(&TemperatureSettingNP);
        defineSwitch(&TempCompSignSP);
        defineSwitch(&TemperatureCompensateSP);
        //        defineSwitch(&BacklashDirectionSP);
        //        defineNumber(&BacklashSettingNP);
        defineSwitch(&ResetSP);
        defineNumber(&FWversionNP);

        GetFocusParams();

        loadConfig(true);

        LOG_INFO("USBFocusV3 parameters updated, focuser ready for use.");
    }
    else
    {
        deleteProperty(TemperatureNP.name);
        deleteProperty(MaxPositionNP.name);
        deleteProperty(StepModeSP.name);
        deleteProperty(RotDirSP.name);
        deleteProperty(TemperatureSettingNP.name);
        deleteProperty(TempCompSignSP.name);
        deleteProperty(TemperatureCompensateSP.name);
        //        deleteProperty(BacklashDirectionSP.name);
        //        deleteProperty(BacklashSettingNP.name);
        deleteProperty(ResetSP.name);
        deleteProperty(FWversionNP.name);
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
        LOG_INFO("Error retreiving data from USBFocusV3, trying resync...");
    }
    while (--tries > 0 && Resync());

    LOG_INFO("Error retreiving data from USBFocusV3, please ensure controller "
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
    IUResetSwitch(&StepModeSP);

    if (stepmode == UFOPHSTEPS)
        StepModeS[UFOPHSTEPS].s = ISS_ON;
    else if (stepmode == UFOPFSTEPS)
        StepModeS[UFOPFSTEPS].s = ISS_ON;
    else
    {
        LOGF_ERROR("Unknown error: focuser step value (%d)", stepmode);
        return false;
    }

    return true;
}

bool USBFocusV3::updateRotDir()
{
    IUResetSwitch(&RotDirSP);

    if (direction == UFOPSDIR)
        RotDirS[UFOPSDIR].s = ISS_ON;
    else if (direction == UFOPRDIR)
        RotDirS[UFOPRDIR].s = ISS_ON;
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
                TemperatureN[0].value = temp;
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
    FWversionN[0].value = firmware;
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
                FocusAbsPosN[0].value = pos;
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
    MaxPositionN[0].value = maxpos;
    FocusAbsPosN[0].max   = maxpos;
    return true;
}

bool USBFocusV3::updateTempCompSettings()
{
    TemperatureSettingN[0].value = stepsdeg;
    TemperatureSettingN[1].value = tcomp_thr;
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
        IUResetSwitch(&TempCompSignSP);

        if (sign == UFOPNSIGN)
            TempCompSignS[UFOPNSIGN].s = ISS_ON;
        else if (sign == UFOPPSIGN)
            TempCompSignS[UFOPPSIGN].s = ISS_ON;
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
        FocusSpeedN[0].value = drvspeed;
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

    if ((dir == FOCUS_INWARD) && (rticks > FocusAbsPosN[0].value))
    {
        ticks = FocusAbsPosN[0].value;
        LOGF_WARN("Requested %u ticks but inward movement has been limited to %u ticks", rticks, ticks);
    }
    else if ((dir == FOCUS_OUTWARD) && ((FocusAbsPosN[0].value + rticks) > MaxPositionN[0].value))
    {
        ticks = MaxPositionN[0].value - FocusAbsPosN[0].value;
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
        FocusAbsPosN[0].max = maxpos;
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
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        if (strcmp(StepModeSP.name, name) == 0)
        {
            bool rc          = false;
            int current_mode = IUFindOnSwitchIndex(&StepModeSP);
            IUUpdateSwitch(&StepModeSP, states, names, n);
            int target_mode = IUFindOnSwitchIndex(&StepModeSP);
            if (current_mode == target_mode)
            {
                StepModeSP.s = IPS_OK;
                IDSetSwitch(&StepModeSP, nullptr);
                return true;
            }

            if (target_mode == UFOPHSTEPS)
                rc = setStepMode(FOCUS_HALF_STEP);
            else
                rc = setStepMode(FOCUS_FULL_STEP);

            if (!rc)
            {
                IUResetSwitch(&StepModeSP);
                StepModeS[current_mode].s = ISS_ON;
                StepModeSP.s              = IPS_ALERT;
                IDSetSwitch(&StepModeSP, nullptr);
                return false;
            }

            StepModeSP.s = IPS_OK;
            IDSetSwitch(&StepModeSP, nullptr);
            return true;
        }

        if (strcmp(RotDirSP.name, name) == 0)
        {
            bool rc          = false;
            int current_mode = IUFindOnSwitchIndex(&RotDirSP);
            IUUpdateSwitch(&RotDirSP, states, names, n);
            int target_mode = IUFindOnSwitchIndex(&RotDirSP);
            if (current_mode == target_mode)
            {
                RotDirSP.s = IPS_OK;
                IDSetSwitch(&RotDirSP, nullptr);
                return true;
            }

            rc = setRotDir(target_mode);
            if (!rc)
            {
                IUResetSwitch(&RotDirSP);
                RotDirS[current_mode].s = ISS_ON;
                RotDirSP.s              = IPS_ALERT;
                IDSetSwitch(&RotDirSP, nullptr);
                return false;
            }

            RotDirSP.s = IPS_OK;
            IDSetSwitch(&RotDirSP, nullptr);
            return true;
        }

        if (strcmp(TemperatureCompensateSP.name, name) == 0)
        {
            int last_index = IUFindOnSwitchIndex(&TemperatureCompensateSP);
            IUUpdateSwitch(&TemperatureCompensateSP, states, names, n);

            bool rc = setTemperatureCompensation((TemperatureCompensateS[0].s == ISS_ON));

            if (!rc)
            {
                TemperatureCompensateSP.s = IPS_ALERT;
                IUResetSwitch(&TemperatureCompensateSP);
                TemperatureCompensateS[last_index].s = ISS_ON;
                IDSetSwitch(&TemperatureCompensateSP, nullptr);
                return false;
            }

            TemperatureCompensateSP.s = IPS_OK;
            IDSetSwitch(&TemperatureCompensateSP, nullptr);
            return true;
        }

        if (strcmp(TempCompSignSP.name, name) == 0)
        {
            bool rc          = false;
            int current_mode = IUFindOnSwitchIndex(&TempCompSignSP);
            IUUpdateSwitch(&TempCompSignSP, states, names, n);
            int target_mode = IUFindOnSwitchIndex(&TempCompSignSP);
            if (current_mode == target_mode)
            {
                TempCompSignSP.s = IPS_OK;
                IDSetSwitch(&TempCompSignSP, nullptr);
                return true;
            }

            rc = setTempCompSign(target_mode);
            if (!rc)
            {
                IUResetSwitch(&TempCompSignSP);
                TempCompSignS[current_mode].s = ISS_ON;
                TempCompSignSP.s              = IPS_ALERT;
                IDSetSwitch(&TempCompSignSP, nullptr);
                return false;
            }

            TempCompSignSP.s = IPS_OK;
            IDSetSwitch(&TempCompSignSP, nullptr);
            return true;
        }

        //        if (strcmp(BacklashDirectionSP.name, name) == 0)
        //        {
        //            IUUpdateSwitch(&BacklashDirectionSP, states, names, n);
        //            int target_direction  = IUFindOnSwitchIndex(&BacklashDirectionSP);
        //            backlashIn            = (target_direction == BACKLASH_IN);
        //            BacklashDirectionSP.s = IPS_OK;
        //            IDSetSwitch(&BacklashDirectionSP, nullptr);
        //            return true;
        //        }

        if (strcmp(ResetSP.name, name) == 0)
        {
            IUResetSwitch(&ResetSP);

            if (reset())
                ResetSP.s = IPS_OK;
            else
                ResetSP.s = IPS_ALERT;

            IDSetSwitch(&ResetSP, nullptr);
            return true;
        }
    }

    return INDI::Focuser::ISNewSwitch(dev, name, states, names, n);
}

bool USBFocusV3::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        if (strcmp(name, MaxPositionNP.name) == 0)
        {
            IUUpdateNumber(&MaxPositionNP, values, names, n);
            if (!setMaxPos(MaxPositionN[0].value))
            {
                MaxPositionNP.s = IPS_ALERT;
                IDSetNumber(&MaxPositionNP, nullptr);
                return false;
            }
            MaxPositionNP.s = IPS_OK;
            IDSetNumber(&MaxPositionNP, nullptr);
            return true;
        }

        if (strcmp(name, TemperatureSettingNP.name) == 0)
        {
            IUUpdateNumber(&TemperatureSettingNP, values, names, n);
            if (!setAutoTempCompThreshold(TemperatureSettingN[1].value) ||
                    !setTemperatureCoefficient(TemperatureSettingN[UFOPNSIGN].value))
            {
                TemperatureSettingNP.s = IPS_ALERT;
                IDSetNumber(&TemperatureSettingNP, nullptr);
                return false;
            }

            TemperatureSettingNP.s = IPS_OK;
            IDSetNumber(&TemperatureSettingNP, nullptr);
            return true;
        }

        //        if (strcmp(name, BacklashSettingNP.name) == 0)
        //        {
        //            IUUpdateNumber(&BacklashSettingNP, values, names, n);
        //            backlashSteps       = std::round(BacklashSettingN[0].value);
        //            BacklashSettingNP.s = IPS_OK;
        //            IDSetNumber(&BacklashSettingNP, nullptr);
        //            return true;
        //        }

        if (strcmp(name, FWversionNP.name) == 0)
        {
            IUUpdateNumber(&FWversionNP, values, names, n);
            FWversionNP.s = IPS_OK;
            IDSetNumber(&FWversionNP, nullptr);
            return true;
        }
    }

    return INDI::Focuser::ISNewNumber(dev, name, values, names, n);
}

void USBFocusV3::GetFocusParams()
{
    getControllerStatus();

    if (updatePosition())
        IDSetNumber(&FocusAbsPosNP, nullptr);

    if (updateMaxPos())
    {
        IDSetNumber(&MaxPositionNP, nullptr);
        IDSetNumber(&FocusAbsPosNP, nullptr);
    }

    if (updateTemperature())
        IDSetNumber(&TemperatureNP, nullptr);

    if (updateTempCompSettings())
        IDSetNumber(&TemperatureSettingNP, nullptr);

    if (updateTempCompSign())
        IDSetSwitch(&TempCompSignSP, nullptr);

    if (updateSpeed())
        IDSetNumber(&FocusSpeedNP, nullptr);

    if (updateStepMode())
        IDSetSwitch(&StepModeSP, nullptr);

    if (updateRotDir())
        IDSetSwitch(&RotDirSP, nullptr);

    if (updateFWversion())
        IDSetNumber(&FWversionNP, nullptr);
}

bool USBFocusV3::SetFocuserSpeed(int speed)
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

IPState USBFocusV3::MoveAbsFocuser(uint32_t targetTicks)
{
    long ticks;

    targetPos = targetTicks;

    ticks = targetPos - FocusAbsPosN[0].value;

    bool rc = false;

    if (ticks < 0)
        rc = MoveFocuserUF(FOCUS_INWARD, -ticks);
    else if (ticks > 0)
        rc = MoveFocuserUF(FOCUS_OUTWARD, ticks);

    if (!rc)
        return IPS_ALERT;

    FocusAbsPosNP.s = IPS_BUSY;

    return IPS_BUSY;
}

IPState USBFocusV3::MoveRelFocuser(FocusDirection dir, uint32_t ticks)
{
    bool rc = false;
    uint32_t aticks;

    if ((dir == FOCUS_INWARD) && (ticks > FocusAbsPosN[0].value))
    {
        aticks = FocusAbsPosN[0].value;
        DEBUGF(INDI::Logger::DBG_WARNING,
               "Requested %u ticks but relative inward movement has been limited to %u ticks", ticks, aticks);
        ticks = aticks;
    }
    else if ((dir == FOCUS_OUTWARD) && ((FocusAbsPosN[0].value + ticks) > MaxPositionN[0].value))
    {
        aticks = MaxPositionN[0].value - FocusAbsPosN[0].value;
        DEBUGF(INDI::Logger::DBG_WARNING,
               "Requested %u ticks but relative outward movement has been limited to %u ticks", ticks, aticks);
        ticks = aticks;
    }

    rc = MoveFocuserUF(dir, (unsigned int)ticks);

    if (!rc)
        return IPS_ALERT;

    FocusRelPosN[0].value = ticks;
    FocusRelPosNP.s       = IPS_BUSY;

    return IPS_BUSY;
}

void USBFocusV3::TimerHit()
{
    if (!isConnected())
    {
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
        if (backlashMove && (fabs(backlashTargetPos - FocusAbsPosN[0].value) < 1))
        {
            // Backlash target reached, now go to real target
            MoveAbsFocuser(targetPos);
            backlashMove = false;
        }
        else
        {
            if (fabs(targetPos - FocusAbsPosN[0].value) < 1)
            {
                FocusAbsPosNP.s = IPS_OK;
                FocusRelPosNP.s = IPS_OK;
                IDSetNumber(&FocusAbsPosNP, nullptr);
                IDSetNumber(&FocusRelPosNP, nullptr);
                lastPos = FocusAbsPosN[0].value;
                LOG_INFO("Focuser reached requested position.");
            }
        }
    }

    SetTimer(POLLMS);
}

bool USBFocusV3::AbortFocuser()
{
    char resp[UFORESLEN] = {};
    if (!sendCommand(UFOCABORT, resp))
        return false;

    FocusAbsPosNP.s = IPS_IDLE;
    FocusRelPosNP.s = IPS_IDLE;
    IDSetNumber(&FocusAbsPosNP, nullptr);
    IDSetNumber(&FocusRelPosNP, nullptr);
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

//bool USBFocusV3::saveConfigItems(FILE *fp)
//{
//    INDI::Focuser::saveConfigItems(fp);

//    IUSaveConfigNumber(fp, &BacklashSettingNP);
//    IUSaveConfigSwitch(fp, &BacklashDirectionSP);
//    return true;
//}
