/*
    MyFocuserPro2 Focuser
    Copyright (c) 2019 Alan Townshend

    Based on Moonlite focuser
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

#include "myfocuserpro2.h"
#include "connectionplugins/connectiontcp.h"
#include "indicom.h"

#include <cmath>
#include <cstring>
#include <memory>

#include <termios.h>
#include <unistd.h>

static std::unique_ptr<MyFocuserPro2> myFocuserPro2(new MyFocuserPro2());

MyFocuserPro2::MyFocuserPro2()
{
    // Can move in Absolute & Relative motions, can AbortFocuser motion, and has variable speed.
    FI::SetCapability(FOCUSER_CAN_ABS_MOVE | FOCUSER_CAN_REL_MOVE | FOCUSER_CAN_ABORT | FOCUSER_CAN_REVERSE |
                      FOCUSER_HAS_VARIABLE_SPEED |
                      FOCUSER_CAN_SYNC);

    setSupportedConnections(CONNECTION_SERIAL | CONNECTION_TCP);

    setVersion(0, 7);
}

bool MyFocuserPro2::initProperties()
{
    INDI::Focuser::initProperties();

    FocusSpeedN[0].min   = 0;
    FocusSpeedN[0].max   = 2;
    FocusSpeedN[0].value = 1;

    /* Relative and absolute movement */
    FocusRelPosN[0].min   = 0.;
    FocusRelPosN[0].max   = 50000.;
    FocusRelPosN[0].value = 0.;
    FocusRelPosN[0].step  = 1000;

    FocusAbsPosN[0].min   = 0.;
    FocusAbsPosN[0].max   = 200000.;
    FocusAbsPosN[0].value = 0.;
    FocusAbsPosN[0].step  = 1000;

    FocusMaxPosN[0].min   = 1024.;
    FocusMaxPosN[0].max   = 200000.;
    FocusMaxPosN[0].value = 0.;
    FocusMaxPosN[0].step  = 1000;

    //Backlash
    BacklashInStepsN[0].min   = 0;
    BacklashInStepsN[0].max   = 512;
    BacklashInStepsN[0].value = 0;
    BacklashInStepsN[0].step  = 2;

    BacklashOutStepsN[0].min   = 0;
    BacklashOutStepsN[0].max   = 512;
    BacklashOutStepsN[0].value = 0;
    BacklashOutStepsN[0].step  = 2;


    // Backlash In
    IUFillSwitch(&BacklashInS[INDI_ENABLED], "INDI_ENABLED", "On", ISS_OFF);
    IUFillSwitch(&BacklashInS[INDI_DISABLED], "INDI_DISABLED", "Off", ISS_OFF);
    IUFillSwitchVector(&BacklashInSP, BacklashInS, 2, getDeviceName(), "Backlash In", "", OPTIONS_TAB, IP_RW, ISR_1OFMANY, 0,
                       IPS_IDLE);

    IUFillNumber(&BacklashInStepsN[0], "Steps", "", "%3.0f", 0, 512, 2, 0);
    IUFillNumberVector(&BacklashInStepsNP, BacklashInStepsN, 1, getDeviceName(), "Backlash-In", "", OPTIONS_TAB, IP_RW, 0,
                       IPS_IDLE);

    // Backlash Out
    IUFillSwitch(&BacklashOutS[INDI_ENABLED], "INDI_ENABLED", "On", ISS_OFF);
    IUFillSwitch(&BacklashOutS[INDI_DISABLED], "INDI_DISABLED", "Off", ISS_OFF);
    IUFillSwitchVector(&BacklashOutSP, BacklashOutS, 2, getDeviceName(), "Backlash Out", "", OPTIONS_TAB, IP_RW, ISR_1OFMANY, 0,
                       IPS_IDLE);

    IUFillNumber(&BacklashOutStepsN[0], "Steps", "", "%3.0f", 0, 512, 2, 0);
    IUFillNumberVector(&BacklashOutStepsNP, BacklashOutStepsN, 1, getDeviceName(), "Backlash-Out", "", OPTIONS_TAB, IP_RW, 0,
                       IPS_IDLE);

    // Focuser temperature
    IUFillNumber(&TemperatureN[0], "TEMPERATURE", "Celsius", "%6.2f", -40, 80., 0., 0.);
    IUFillNumberVector(&TemperatureNP, TemperatureN, 1, getDeviceName(), "FOCUS_TEMPERATURE", "Temperature", MAIN_CONTROL_TAB,
                       IP_RO, 0, IPS_IDLE);

    // Temperature Settings
    IUFillNumber(&TemperatureSettingN[0], "Coefficient", "", "%6.2f", 0, 50, 1, 0);
    IUFillNumberVector(&TemperatureSettingNP, TemperatureSettingN, 1, getDeviceName(), "T. Settings", "", OPTIONS_TAB, IP_RW, 0,
                       IPS_IDLE);

    // Compensate for temperature
    IUFillSwitch(&TemperatureCompensateS[TEMP_COMPENSATE_ENABLE], "TEMP_COMPENSATE_ENABLE", "Enable", ISS_OFF);
    IUFillSwitch(&TemperatureCompensateS[TEMP_COMPENSATE_DISABLE], "TEMP_COMPENSATE_DISABLE", "Disable", ISS_OFF);
    IUFillSwitchVector(&TemperatureCompensateSP, TemperatureCompensateS, 2, getDeviceName(), "T. Compensate", "", OPTIONS_TAB,
                       IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    // Step Mode
    IUFillSwitch(&StepModeS[FOCUS_THIRTYSECOND_STEP], "FOCUS_THIRTYSECOND_STEP", "1/32 Step", ISS_OFF);
    IUFillSwitch(&StepModeS[FOCUS_SIXTEENTH_STEP], "FOCUS_SIXTEENTH_STEP", "1/16 Step", ISS_OFF);
    IUFillSwitch(&StepModeS[FOCUS_EIGHTH_STEP], "FOCUS_EIGHTH_STEP", "1/8 Step", ISS_OFF);
    IUFillSwitch(&StepModeS[FOCUS_QUARTER_STEP], "FOCUS_QUARTER_STEP", "1/4 Step", ISS_OFF);
    IUFillSwitch(&StepModeS[FOCUS_HALF_STEP], "FOCUS_HALF_STEP", "1/2 Step", ISS_OFF);
    IUFillSwitch(&StepModeS[FOCUS_FULL_STEP], "FOCUS_FULL_STEP", "Full Step", ISS_OFF);
    IUFillSwitchVector(&StepModeSP, StepModeS, 6, getDeviceName(), "Step Mode", "", OPTIONS_TAB, IP_RW, ISR_1OFMANY, 0,
                       IPS_IDLE);


    IUFillSwitch(&CoilPowerS[COIL_POWER_ON], "COIL_POWER_ON", "On", ISS_OFF);
    IUFillSwitch(&CoilPowerS[COIL_POWER_OFF], "COIL_POWER_OFF", "Off", ISS_OFF);
    IUFillSwitchVector(&CoilPowerSP, CoilPowerS, 2, getDeviceName(), "Coil Power", "", OPTIONS_TAB, IP_RW, ISR_1OFMANY, 0,
                       IPS_IDLE);

    IUFillSwitch(&DisplayS[DISPLAY_OFF], "DISPLAY_OFF", "Off", ISS_OFF);
    IUFillSwitch(&DisplayS[DISPLAY_ON], "DISPLAY_ON", "On", ISS_OFF);
    IUFillSwitchVector(&DisplaySP, DisplayS, 2, getDeviceName(), "Display", "", OPTIONS_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);


    IUFillSwitch(&GotoHomeS[0], "GOTO_HOME", "Go", ISS_OFF);
    IUFillSwitchVector(&GotoHomeSP, GotoHomeS, 1, getDeviceName(), "Goto Home Position", "", MAIN_CONTROL_TAB, IP_RW,
                       ISR_ATMOST1, 0, IPS_IDLE);

    setPollingPeriodRange(1000, 30000);
    setDefaultPollingPeriod(1000);

    tcpConnection->setDefaultHost("192.168.4.1");
    tcpConnection->setDefaultPort(2020);

    return true;
}

bool MyFocuserPro2::updateProperties()
{
    INDI::Focuser::updateProperties();

    if (isConnected())
    {
        defineProperty(&GotoHomeSP);
        defineProperty(&TemperatureNP);
        defineProperty(&TemperatureSettingNP);
        defineProperty(&TemperatureCompensateSP);
        defineProperty(&BacklashInSP);
        defineProperty(&BacklashInStepsNP);
        defineProperty(&BacklashOutSP);
        defineProperty(&BacklashOutStepsNP);
        defineProperty(&StepModeSP);
        defineProperty(&DisplaySP);
        defineProperty(&CoilPowerSP);

        setTemperatureCelsius();

        LOG_INFO("MyFocuserPro2 parameters updated, focuser ready for use.");
    }
    else
    {
        deleteProperty(GotoHomeSP.name);
        deleteProperty(TemperatureNP.name);
        deleteProperty(TemperatureSettingNP.name);
        deleteProperty(TemperatureCompensateSP.name);
        deleteProperty(BacklashInSP.name);
        deleteProperty(BacklashInStepsNP.name);
        deleteProperty(BacklashOutSP.name);
        deleteProperty(BacklashOutStepsNP.name);
        deleteProperty(StepModeSP.name);
        deleteProperty(DisplaySP.name);
        deleteProperty(CoilPowerSP.name);
    }

    return true;
}

bool MyFocuserPro2::Handshake()
{
    if (Ack())
    {
        LOG_INFO("MyFocuserPro2 is online. Getting focus parameters...");

        getStartupValues();

        return true;
    }

    LOG_INFO(
        "Error retrieving data from MyFocuserPro2, please ensure MyFocuserPro2 controller is powered and the port is correct.");
    return false;
}

const char * MyFocuserPro2::getDefaultName()
{
    return "MyFocuserPro2";
}

bool MyFocuserPro2::Ack()
{
    int nbytes_written = 0, nbytes_read = 0, rc = -1;
    char errstr[MAXRBUF];
    char resp[5] = {0};
    int firmWareVersion = 0;

    tcflush(PortFD, TCIOFLUSH);

    //Try to request the firmware version
    //Test for success on transmission and response
    //If either one fails, try again, up to 3 times, waiting 1 sec each time
    //If that fails, then return false.
    //If success then check the firmware version

    int numChecks = 0;
    bool success = false;
    while(numChecks < 3 && !success)
    {
        numChecks++;
        sleep(1); //wait 1 second between each test.

        bool transmissionSuccess = (rc = tty_write(PortFD, ":03#", 4, &nbytes_written)) == TTY_OK;
        if(!transmissionSuccess)
        {
            tty_error_msg(rc, errstr, MAXRBUF);
            LOGF_ERROR("Handshake Attempt %i, tty transmission error: %s.", numChecks, errstr);
        }

        bool responseSuccess = (rc = tty_read(PortFD, resp, 5, ML_TIMEOUT, &nbytes_read)) == TTY_OK;
        if(!responseSuccess)
        {
            tty_error_msg(rc, errstr, MAXRBUF);
            LOGF_ERROR("Handshake Attempt %i, updatePosition response error: %s.", numChecks, errstr);
        }

        success = transmissionSuccess && responseSuccess;
    }

    if(!success)
    {
        LOG_INFO("Handshake failed after 3 attempts");
        return false;
    }

    tcflush(PortFD, TCIOFLUSH);

    rc = sscanf(resp, "F%d#", &firmWareVersion);

    if (rc > 0)
    {
        // Minimum check only applicable to serial version
        if (getActiveConnection()->type() == Connection::Interface::CONNECTION_SERIAL)
        {
            if(firmWareVersion >= MINIMUM_FIRMWARE_VERSION)
            {
                LOGF_INFO("MyFP2 reported firmware %d", firmWareVersion);
                return true;

            }
            else
            {
                LOGF_ERROR("Invalid Firmware: focuser firmware version value %d, minimum supported is %d", firmWareVersion,
                           MINIMUM_FIRMWARE_VERSION );
            }
        }
        else
        {
            LOG_INFO("Connection to network focuser is successful.");
            return true;
        }
    }
    else
    {
        LOGF_ERROR("Invalid Response: focuser firmware version value (%s)", resp);
    }
    return false;
}

bool MyFocuserPro2::readCoilPowerState()
{
    char res[ML_RES] = {0};

    if (sendCommand(":11#", res) == false)
        return false;

    uint32_t temp = 0;

    int rc = sscanf(res, "O%u#", &temp);

    if (rc > 0)

        if(temp == 0)
            CoilPowerS[COIL_POWER_OFF].s = ISS_ON;
        else if (temp == 1)
            CoilPowerS[COIL_POWER_ON].s = ISS_ON;
        else
        {
            LOGF_ERROR("Invalid Response: focuser Coil Power value (%s)", res);
            return false;
        }
    else
    {
        LOGF_ERROR("Unknown error: focuser Coil Power value (%s)", res);
        return false;
    }


    return true;
}

bool MyFocuserPro2::readReverseDirection()
{
    char res[ML_RES] = {0};

    if (sendCommand(":13#", res) == false)
        return false;

    uint32_t temp = 0;

    int rc = sscanf(res, "R%u#", &temp);

    if (rc > 0)

        if(temp == 0)
        {
            FocusReverseS[INDI_DISABLED].s = ISS_ON;
        }
        else if (temp == 1)
        {
            FocusReverseS[INDI_ENABLED].s = ISS_ON;
        }
        else
        {
            LOGF_ERROR("Invalid Response: focuser Reverse direction value (%s)", res);
            return false;
        }
    else
    {
        LOGF_ERROR("Unknown error: focuser Reverse direction value (%s)", res);
        return false;
    }

    return true;
}

bool MyFocuserPro2::readStepMode()
{
    char res[ML_RES] = {0};

    if (sendCommand(":29#", res) == false)
        return false;

    if (strcmp(res, "S1#") == 0)
        StepModeS[FOCUS_FULL_STEP].s = ISS_ON;
    else if (strcmp(res, "S2#") == 0)
        StepModeS[FOCUS_HALF_STEP].s = ISS_ON;
    else if (strcmp(res, "S4#") == 0)
        StepModeS[FOCUS_QUARTER_STEP].s = ISS_ON;
    else if (strcmp(res, "S8#") == 0)
        StepModeS[FOCUS_EIGHTH_STEP].s = ISS_ON;
    else if (strcmp(res, "S16#") == 0)
        StepModeS[FOCUS_SIXTEENTH_STEP].s = ISS_ON;
    else if (strcmp(res, "S32#") == 0)
        StepModeS[FOCUS_THIRTYSECOND_STEP].s = ISS_ON;
    else
    {
        LOGF_ERROR("Unknown error: focuser Step Mode value (%s)", res);
        return false;
    }

    return true;
}

bool MyFocuserPro2::readTemperature()
{
    char res[ML_RES] = {0};

    if (sendCommand(":06#", res) == false)
        return false;

    double temp = 0;
    int rc = sscanf(res, "Z%lf#", &temp);
    if (rc > 0)
        // Signed hex
        TemperatureN[0].value = temp;
    else
    {
        LOGF_ERROR("Unknown error: focuser temperature value (%s)", res);
        return false;
    }

    return true;
}

bool MyFocuserPro2::readTempCompensateEnable()
{
    char res[ML_RES] = {0};

    if (sendCommand(":24#", res) == false)
        return false;

    uint32_t temp = 0;

    int rc = sscanf(res, "1%u#", &temp);

    if (rc > 0)

        if(temp == 0)
            TemperatureCompensateS[TEMP_COMPENSATE_DISABLE].s = ISS_ON;
        else if (temp == 1)
            TemperatureCompensateS[TEMP_COMPENSATE_ENABLE].s = ISS_ON;
        else
        {
            LOGF_ERROR("Invalid Response: focuser T.Compensate value (%s)", res);
            return false;
        }
    else
    {
        LOGF_ERROR("Unknown error: focuser T.Compensate value (%s)", res);
        return false;
    }

    return true;
}


bool MyFocuserPro2::readPosition()
{
    char res[ML_RES] = {0};

    if (sendCommand(":00#", res) == false)
        return false;

    int32_t pos;
    int rc = sscanf(res, "%*c%d#", &pos);

    if (rc > 0)
        FocusAbsPosN[0].value = pos;
    else
    {
        LOGF_ERROR("Unknown error: focuser position value (%s)", res);
        return false;
    }

    return true;
}

bool MyFocuserPro2::readTempeartureCoefficient()
{
    char res[ML_RES] = {0};

    if (sendCommand(":26#", res) == false)
        return false;

    int32_t val;
    int rc = sscanf(res, "B%d#", &val);

    if (rc > 0)
        TemperatureSettingN[0].value = val;
    else
    {
        LOGF_ERROR("Unknown error: Temperature Coefficient value (%s)", res);
        return false;
    }

    return true;
}

bool MyFocuserPro2::readSpeed()
{
    char res[ML_RES] = {0};

    if (sendCommand(":43#", res) == false)
        return false;

    int speed = 0;
    int rc = sscanf(res, "C%d#", &speed);

    if (rc > 0)
    {
        FocusSpeedN[0].value = speed;
    }
    else
    {
        LOGF_ERROR("Unknown error: focuser speed value (%s)", res);
        return false;
    }

    return true;
}

bool MyFocuserPro2::readMaxPos()
{
    char res[ML_RES] = {0};

    if (sendCommand(":08#", res) == false)
        return false;

    uint32_t maxPos = 0;
    int rc = sscanf(res, "M%u#", &maxPos);

    if (rc > 0)
    {
        FocusMaxPosN[0].value = maxPos;
        Focuser::SyncPresets(maxPos);
    }
    else
    {
        LOGF_ERROR("Unknown error: focuser max position value (%s)", res);
        return false;
    }

    return true;
}

bool MyFocuserPro2::readBacklashInSteps()
{
    char res[ML_RES] = {0};

    if (sendCommand(":78#", res) == false)
        return false;

    uint32_t backlash = 0;
    int rc = sscanf(res, "6%u#", &backlash);

    if (rc > 0)
    {
        BacklashInStepsN[0].value = backlash;
    }
    else
    {
        BacklashInStepsN[0].value = 0;
        LOGF_ERROR("Unknown error: focuser Backlash IN value (%s)", res);
        return false;
    }

    return true;
}

bool MyFocuserPro2::readBacklashInEnabled()
{
    char res[ML_RES] = {0};

    if (sendCommand(":74#", res) == false)
        return false;

    uint32_t temp = 0;
    int rc = sscanf(res, "4%u#", &temp);

    if (rc > 0)
    {
        if(temp == 0)
            BacklashInS[INDI_DISABLED].s = ISS_ON;
        else if (temp == 1)
            BacklashInS[INDI_ENABLED].s = ISS_ON;
        else
            LOGF_ERROR("Unknown Repsonse: focuser Backlash IN enabled (%s)", res);
        return false;
    }
    else
    {
        LOGF_ERROR("Unknown error: focuser Backlash IN enabled (%s)", res);
        return false;
    }
}

bool MyFocuserPro2::readBacklashOutSteps()
{
    char res[ML_RES] = {0};

    if (sendCommand(":80#", res) == false)
        return false;

    uint32_t backlash = 0;
    int rc = sscanf(res, "7%u#", &backlash);

    if (rc > 0)
    {
        BacklashOutStepsN[0].value = backlash;
    }
    else
    {
        LOGF_ERROR("Unknown error: focuser Backlash OUT value (%s)", res);
        return false;
    }

    return true;
}

bool MyFocuserPro2::readBacklashOutEnabled()
{
    char res[ML_RES] = {0};

    if (sendCommand(":76#", res) == false)
        return false;

    uint32_t temp = 0;
    int rc = sscanf(res, "5%u#", &temp);

    if (rc > 0)
    {
        if(temp == 0)
            BacklashOutS[INDI_DISABLED].s = ISS_ON;
        else if (temp == 1)
            BacklashOutS[INDI_ENABLED].s = ISS_ON;
        else
            LOGF_ERROR("Unknown response: focuser Backlash OUT enabled (%s)", res);
        return false;
    }
    else
    {
        LOGF_ERROR("Unknown error: focuser Backlash OUT enabled (%s)", res);
        return false;
    }

}

bool MyFocuserPro2::readDisplayVisible()
{
    char res[ML_RES] = {0};

    if (sendCommand(":37#", res) == false)
        return false;

    uint32_t temp = 0;

    int rc = sscanf(res, "D%u#", &temp);

    if (rc > 0)
    {
        if(temp == 0)
            DisplayS[DISPLAY_OFF].s = ISS_ON;
        else if (temp == 1)
            DisplayS[DISPLAY_ON].s = ISS_ON;
        else
        {
            LOGF_ERROR("Invalid Response: focuser Display value (%s)", res);
            return false;
        }
    }
    else
    {
        LOGF_ERROR("Unknown error: focuser Display value (%s)", res);
        return false;
    }

    return true;
}

bool MyFocuserPro2::isMoving()
{
    char res[ML_RES] = {0};

    if (sendCommand(":01#", res) == false)
        return false;

    uint32_t temp = 0;

    int rc = sscanf(res, "I%u#", &temp);

    if (rc > 0)
    {
        if(temp == 0)
            return false;
        else if (temp == 1)
            return true;
        else
        {
            LOGF_ERROR("Invalid Response: focuser isMoving value (%s)", res);
            return false;
        }
    }
    else
    {
        LOGF_ERROR("Unknown error: focuser isMoving value (%s)", res);
        return false;
    }
}


bool MyFocuserPro2::setTemperatureCelsius()
{
    char cmd[ML_RES] = {0};
    snprintf(cmd, ML_RES, ":161#");
    return sendCommand(cmd);
}

bool MyFocuserPro2::setTemperatureCoefficient(double coefficient)
{
    char cmd[ML_RES] = {0};
    int coeff = coefficient;
    snprintf(cmd, ML_RES, ":22%d#", coeff);
    return sendCommand(cmd);
}

bool MyFocuserPro2::SyncFocuser(uint32_t ticks)
{
    char cmd[ML_RES] = {0};
    snprintf(cmd, ML_RES, ":31%u#", ticks);
    return sendCommand(cmd);
}

bool MyFocuserPro2::MoveFocuser(uint32_t position)
{
    char cmd[ML_RES] = {0};
    if(isMoving())
    {
        AbortFocuser();
    }
    snprintf(cmd, ML_RES, ":05%u#", position);
    return sendCommand(cmd);
}

bool MyFocuserPro2::setBacklashInSteps(int16_t steps)
{
    char cmd[ML_RES] = {0};
    snprintf(cmd, ML_RES, ":77%d#", steps);
    return sendCommand(cmd);
}

bool MyFocuserPro2::setBacklashInEnabled(bool enabled)
{
    char cmd[ML_RES] = {0};
    snprintf(cmd, ML_RES, ":73%c#", enabled ? '1' : '0');
    return sendCommand(cmd);
}

bool MyFocuserPro2::setBacklashOutSteps(int16_t steps)
{
    char cmd[ML_RES] = {0};
    snprintf(cmd, ML_RES, ":79%d#", steps);
    return sendCommand(cmd);
}

bool MyFocuserPro2::setBacklashOutEnabled(bool enabled)
{
    char cmd[ML_RES] = {0};
    snprintf(cmd, ML_RES, ":75%c#", enabled ? '1' : '0');
    return sendCommand(cmd);
}

bool MyFocuserPro2::setCoilPowerState(CoilPower enable)
{
    char cmd[ML_RES] = {0};
    snprintf(cmd, ML_RES, ":12%d#", static_cast<int>(enable));
    return sendCommand(cmd);
}


bool MyFocuserPro2::ReverseFocuser(bool enable)
{
    char cmd[ML_RES] = {0};
    snprintf(cmd, ML_RES, ":14%c#", enable ? '1' : '0');
    return sendCommand(cmd);
}


bool MyFocuserPro2::setDisplayVisible(DisplayMode enable)
{
    char cmd[ML_RES] = {0};
    snprintf(cmd, ML_RES, ":36%d#", enable);
    return sendCommand(cmd);
}

bool MyFocuserPro2::setGotoHome()
{
    char cmd[ML_RES] = {0};
    if(isMoving())
    {
        AbortFocuser();
    }
    snprintf(cmd, ML_RES, ":28#");
    return sendCommand(cmd);
}

bool MyFocuserPro2::setStepMode(FocusStepMode mode)
{
    char cmd[ML_RES] = {0};
    int setMode = 1 << static_cast<int>(mode);
    snprintf(cmd, ML_RES, ":30%02d#", setMode);
    return sendCommand(cmd);
}

bool MyFocuserPro2::setSpeed(uint16_t speed)
{
    char cmd[ML_RES] = {0};
    snprintf(cmd, ML_RES, ":150%d#", speed);
    return sendCommand(cmd);
}

bool MyFocuserPro2::setTemperatureCompensation(bool enable)
{
    char cmd[ML_RES] = {0};
    snprintf(cmd, ML_RES, ":23%c#", enable ? '1' : '0');
    return sendCommand(cmd);
}

bool MyFocuserPro2::ISNewSwitch(const char * dev, const char * name, ISState * states, char * names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        // Focus Step Mode
        if (strcmp(StepModeSP.name, name) == 0)
        {
            int current_mode = IUFindOnSwitchIndex(&StepModeSP);

            IUUpdateSwitch(&StepModeSP, states, names, n);

            int target_mode = IUFindOnSwitchIndex(&StepModeSP);

            if (current_mode == target_mode)
            {
                StepModeSP.s = IPS_OK;
                IDSetSwitch(&StepModeSP, nullptr);
            }

            bool rc = setStepMode(static_cast<FocusStepMode>(target_mode));
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

        // Goto Home Position
        if (strcmp(GotoHomeSP.name, name) == 0)
        {
            bool rc = setGotoHome();
            if (!rc)
            {
                IUResetSwitch(&GotoHomeSP);
                CoilPowerSP.s              = IPS_ALERT;
                IDSetSwitch(&GotoHomeSP, nullptr);
                return false;
            }

            GotoHomeSP.s = IPS_OK;
            IDSetSwitch(&GotoHomeSP, nullptr);
            return true;
        }

        // Coil Power Mode
        if (strcmp(CoilPowerSP.name, name) == 0)
        {
            int current_mode = IUFindOnSwitchIndex(&CoilPowerSP);

            IUUpdateSwitch(&CoilPowerSP, states, names, n);

            int target_mode = IUFindOnSwitchIndex(&CoilPowerSP);

            if (current_mode == target_mode)
            {
                CoilPowerSP.s = IPS_OK;
                IDSetSwitch(&CoilPowerSP, nullptr);
            }

            bool rc = setCoilPowerState(static_cast<CoilPower>(target_mode));
            if (!rc)
            {
                IUResetSwitch(&CoilPowerSP);
                CoilPowerS[current_mode].s = ISS_ON;
                CoilPowerSP.s              = IPS_ALERT;
                IDSetSwitch(&CoilPowerSP, nullptr);
                return false;
            }

            CoilPowerSP.s = IPS_OK;
            IDSetSwitch(&CoilPowerSP, nullptr);
            return true;
        }


        // Display Control
        if (strcmp(DisplaySP.name, name) == 0)
        {
            int current_mode = IUFindOnSwitchIndex(&DisplaySP);

            IUUpdateSwitch(&DisplaySP, states, names, n);

            int target_mode = IUFindOnSwitchIndex(&DisplaySP);

            if (current_mode == target_mode)
            {
                DisplaySP.s = IPS_OK;
                IDSetSwitch(&DisplaySP, nullptr);
            }

            bool rc = setDisplayVisible(static_cast<DisplayMode>(target_mode));
            if (!rc)
            {
                IUResetSwitch(&DisplaySP);
                DisplayS[current_mode].s = ISS_ON;
                DisplaySP.s              = IPS_ALERT;
                IDSetSwitch(&DisplaySP, nullptr);
                return false;
            }

            DisplaySP.s = IPS_OK;
            IDSetSwitch(&DisplaySP, nullptr);
            return true;
        }

        // Backlash In Enable
        if (strcmp(BacklashInSP.name, name) == 0)
        {
            int current_mode = IUFindOnSwitchIndex(&BacklashInSP);

            IUUpdateSwitch(&BacklashInSP, states, names, n);

            int target_mode = IUFindOnSwitchIndex(&BacklashInSP);

            if (current_mode == target_mode)
            {
                BacklashInSP.s = IPS_OK;
                IDSetSwitch(&BacklashInSP, nullptr);
            }

            bool rc = setBacklashInEnabled(target_mode == INDI_ENABLED);
            if (!rc)
            {
                IUResetSwitch(&BacklashInSP);
                BacklashInS[current_mode].s = ISS_ON;
                BacklashInSP.s              = IPS_ALERT;
                IDSetSwitch(&BacklashInSP, nullptr);
                return false;
            }

            BacklashInSP.s = IPS_OK;
            IDSetSwitch(&BacklashInSP, nullptr);
            return true;
        }

        // Backlash Out Enable
        if (strcmp(BacklashOutSP.name, name) == 0)
        {
            int current_mode = IUFindOnSwitchIndex(&BacklashOutSP);

            IUUpdateSwitch(&BacklashOutSP, states, names, n);

            int target_mode = IUFindOnSwitchIndex(&BacklashOutSP);

            if (current_mode == target_mode)
            {
                BacklashOutSP.s = IPS_OK;
                IDSetSwitch(&BacklashOutSP, nullptr);
            }

            bool rc = setBacklashOutEnabled(target_mode == INDI_ENABLED);
            if (!rc)
            {
                IUResetSwitch(&BacklashOutSP);
                BacklashOutS[current_mode].s = ISS_ON;
                BacklashOutSP.s              = IPS_ALERT;
                IDSetSwitch(&BacklashOutSP, nullptr);
                return false;
            }

            BacklashOutSP.s = IPS_OK;
            IDSetSwitch(&BacklashOutSP, nullptr);
            return true;
        }

        // Temperature Compensation Mode
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
    }

    return INDI::Focuser::ISNewSwitch(dev, name, states, names, n);
}

bool MyFocuserPro2::ISNewNumber(const char * dev, const char * name, double values[], char * names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        // Temperature Settings
        if (strcmp(name, TemperatureSettingNP.name) == 0)
        {
            IUUpdateNumber(&TemperatureSettingNP, values, names, n);
            if (!setTemperatureCoefficient(TemperatureSettingN[0].value))
            {
                TemperatureSettingNP.s = IPS_ALERT;
                IDSetNumber(&TemperatureSettingNP, nullptr);
                return false;
            }

            TemperatureSettingNP.s = IPS_OK;
            IDSetNumber(&TemperatureSettingNP, nullptr);
            return true;
        }

        // Backlash In
        if (strcmp(name, BacklashInStepsNP.name) == 0)
        {
            IUUpdateNumber(&BacklashInStepsNP, values, names, n);
            if (!setBacklashInSteps(BacklashInStepsN[0].value))
            {
                BacklashInStepsNP.s = IPS_ALERT;
                IDSetNumber(&BacklashInStepsNP, nullptr);
                return false;
            }

            BacklashInStepsNP.s = IPS_OK;
            IDSetNumber(&BacklashInStepsNP, nullptr);
            return true;
        }

        // Backlash Out
        if (strcmp(name, BacklashOutStepsNP.name) == 0)
        {
            IUUpdateNumber(&BacklashOutStepsNP, values, names, n);
            if (!setBacklashOutSteps(BacklashOutStepsN[0].value))
            {
                BacklashOutStepsNP.s = IPS_ALERT;
                IDSetNumber(&BacklashOutStepsNP, nullptr);
                return false;
            }

            BacklashOutStepsNP.s = IPS_OK;
            IDSetNumber(&BacklashOutStepsNP, nullptr);
            return true;
        }

    }

    return INDI::Focuser::ISNewNumber(dev, name, values, names, n);
}

void MyFocuserPro2::getStartupValues()
{
    readMaxPos();
    readPosition();
    readTemperature();
    readTempeartureCoefficient();
    readSpeed();
    readTempCompensateEnable();
    readStepMode();
    readCoilPowerState();
    readDisplayVisible();
    readReverseDirection();
    readBacklashInEnabled();
    readBacklashOutEnabled();
    readBacklashInSteps();
    readBacklashOutSteps();
}

bool MyFocuserPro2::SetFocuserSpeed(int speed)
{
    return setSpeed(speed);
}


bool MyFocuserPro2::SetFocuserMaxPosition(uint32_t maxPos)
{
    char cmd[ML_RES] = {0};

    snprintf(cmd, ML_RES, ":07%06d#", maxPos);

    if(sendCommand(cmd))
    {
        Focuser::SyncPresets(maxPos);

        return true;
    }
    return false;
}

IPState MyFocuserPro2::MoveFocuser(FocusDirection dir, int speed, uint16_t duration)
{
    if (speed != static_cast<int>(FocusSpeedN[0].value))
    {
        if (!setSpeed(speed))
            return IPS_ALERT;
    }

    // either go all the way in or all the way out
    // then use timer to stop
    if (dir == FOCUS_INWARD)
        MoveFocuser(0);
    else
        MoveFocuser(FocusMaxPosN[0].value);

    IEAddTimer(duration, &MyFocuserPro2::timedMoveHelper, this);
    return IPS_BUSY;
}

void MyFocuserPro2::timedMoveHelper(void * context)
{
    static_cast<MyFocuserPro2 *>(context)->timedMoveCallback();
}

void MyFocuserPro2::timedMoveCallback()
{
    AbortFocuser();
    FocusAbsPosNP.s = IPS_IDLE;
    FocusRelPosNP.s = IPS_IDLE;
    FocusTimerNP.s = IPS_IDLE;
    FocusTimerN[0].value = 0;
    IDSetNumber(&FocusAbsPosNP, nullptr);
    IDSetNumber(&FocusRelPosNP, nullptr);
    IDSetNumber(&FocusTimerNP, nullptr);
}

IPState MyFocuserPro2::MoveAbsFocuser(uint32_t targetTicks)
{
    targetPos = targetTicks;

    if (!MoveFocuser(targetPos))
        return IPS_ALERT;

    return IPS_BUSY;
}

IPState MyFocuserPro2::MoveRelFocuser(FocusDirection dir, uint32_t ticks)
{
    int32_t newPosition = 0;

    if (dir == FOCUS_INWARD)
        newPosition = FocusAbsPosN[0].value - ticks;
    else
        newPosition = FocusAbsPosN[0].value + ticks;

    // Clamp
    newPosition = std::max(0, std::min(static_cast<int32_t>(FocusAbsPosN[0].max), newPosition));
    if (!MoveFocuser(newPosition))
        return IPS_ALERT;

    FocusRelPosN[0].value = ticks;
    FocusRelPosNP.s       = IPS_BUSY;

    return IPS_BUSY;
}

void MyFocuserPro2::TimerHit()
{
    if (!isConnected())
    {
        SetTimer(getCurrentPollingPeriod());
        return;
    }

    bool rc = readPosition();
    if (rc)
    {
        if (fabs(lastPos - FocusAbsPosN[0].value) > 5)
        {
            IDSetNumber(&FocusAbsPosNP, nullptr);
            lastPos = FocusAbsPosN[0].value;
        }
    }

    rc = readTemperature();
    if (rc)
    {
        if (fabs(lastTemperature - TemperatureN[0].value) >= 0.5)
        {
            IDSetNumber(&TemperatureNP, nullptr);
            lastTemperature = TemperatureN[0].value;
        }
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
            LOG_INFO("Focuser reached requested position.");
        }
    }

    SetTimer(getCurrentPollingPeriod());
}

bool MyFocuserPro2::AbortFocuser()
{
    return sendCommand(":27#");
}

bool MyFocuserPro2::saveConfigItems(FILE * fp)
{
    Focuser::saveConfigItems(fp);

    IUSaveConfigSwitch(fp, &StepModeSP);

    return true;
}

bool MyFocuserPro2::sendCommand(const char * cmd, char * res)
{
    int nbytes_written = 0, nbytes_read = 0, rc = -1;

    tcflush(PortFD, TCIOFLUSH);

    LOGF_DEBUG("CMD <%s>", cmd);

    if ((rc = tty_write_string(PortFD, cmd, &nbytes_written)) != TTY_OK)
    {
        char errstr[MAXRBUF] = {0};
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("Serial write error: %s.", errstr);
        return false;
    }

    if (res == nullptr)
        return true;

    if ((rc = tty_nread_section(PortFD, res, ML_RES, ML_DEL, ML_TIMEOUT, &nbytes_read)) != TTY_OK)
    {
        char errstr[MAXRBUF] = {0};
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("Serial read error: %s.", errstr);
        return false;
    }

    LOGF_DEBUG("RES <%s>", res);

    tcflush(PortFD, TCIOFLUSH);

    return true;
}
