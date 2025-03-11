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

    setVersion(CDRIVER_VERSION_MAJOR, CDRIVER_VERSION_MINOR);
}

bool MyFocuserPro2::initProperties()
{
    INDI::Focuser::initProperties();

    FocusSpeedNP[0].setMin(0);
    FocusSpeedNP[0].setMax(2);
    FocusSpeedNP[0].setValue(1);

    /* Relative and absolute movement */
    FocusRelPosNP[0].setMin(0.);
    FocusRelPosNP[0].setMax(50000.);
    FocusRelPosNP[0].setValue(0.);
    FocusRelPosNP[0].setStep(1000);

    FocusAbsPosNP[0].setMin(0.);
    FocusAbsPosNP[0].setMax(200000.);
    FocusAbsPosNP[0].setValue(0.);
    FocusAbsPosNP[0].setStep(1000);

    FocusMaxPosNP[0].setMin(1024.);
    FocusMaxPosNP[0].setMax(200000.);
    FocusMaxPosNP[0].setValue(0.);
    FocusMaxPosNP[0].setStep(1000);

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
    IUFillSwitch(&BacklashInS[INDI_DISABLED], "INDI_DISABLED", "Off", ISS_ON);
    IUFillSwitchVector(&BacklashInSP, BacklashInS, 2, getDeviceName(), "BACKLASH_IN_TOGGLE", "Backlash In", SETTINGS_TAB, IP_RW,
                       ISR_1OFMANY, 0, IPS_IDLE);

    IUFillNumber(&BacklashInStepsN[0], "Steps", "", "%3.0f", 0, 512, 2, 0);
    IUFillNumberVector(&BacklashInStepsNP, BacklashInStepsN, 1, getDeviceName(), "BACKLASH_IN_VALUE", "Backlash In",
                       SETTINGS_TAB, IP_RW, 0,  IPS_IDLE);

    // Backlash Out
    IUFillSwitch(&BacklashOutS[INDI_ENABLED], "INDI_ENABLED", "On", ISS_OFF);
    IUFillSwitch(&BacklashOutS[INDI_DISABLED], "INDI_DISABLED", "Off", ISS_ON);
    IUFillSwitchVector(&BacklashOutSP, BacklashOutS, 2, getDeviceName(), "BACKLASH_OUT_TOGGLE", "Backlash Out", SETTINGS_TAB,
                       IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    IUFillNumber(&BacklashOutStepsN[0], "Steps", "", "%3.0f", 0, 512, 2, 0);
    IUFillNumberVector(&BacklashOutStepsNP, BacklashOutStepsN, 1, getDeviceName(), "BACKLASH_OUT_VALUE", "Backlash Out",
                       SETTINGS_TAB, IP_RW, 0, IPS_IDLE);

    // Focuser temperature
    IUFillNumber(&TemperatureN[0], "TEMPERATURE", "Celsius", "%6.2f", -40, 80., 0., 0.);
    IUFillNumberVector(&TemperatureNP, TemperatureN, 1, getDeviceName(), "FOCUS_TEMPERATURE", "Temperature", MAIN_CONTROL_TAB,
                       IP_RO, 0, IPS_IDLE);

    // Temperature Settings
    IUFillNumber(&TemperatureSettingN[0], "Coefficient", "", "%6.2f", 0, 50, 1, 0);
    IUFillNumberVector(&TemperatureSettingNP, TemperatureSettingN, 1, getDeviceName(), "FOCUS_TEMPERATURE_SETTINGS",
                       "T. Settings", SETTINGS_TAB, IP_RW, 0, IPS_IDLE);

    // Compensate for temperature
    IUFillSwitch(&TemperatureCompensateS[TEMP_COMPENSATE_ENABLE], "TEMP_COMPENSATE_ENABLE", "Enable", ISS_OFF);
    IUFillSwitch(&TemperatureCompensateS[TEMP_COMPENSATE_DISABLE], "TEMP_COMPENSATE_DISABLE", "Disable", ISS_ON);
    IUFillSwitchVector(&TemperatureCompensateSP, TemperatureCompensateS, 2, getDeviceName(), "FOCUS_TEMPERATURE_COMPENSATION",
                       "T. Compensation", SETTINGS_TAB,  IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    // Add step modes 1/64 to 1/256 for TMC type drivers R Brown June 2021
    IUFillSwitch(&StepModeS[TWOHUNDREDFIFTYSIX_STEP], "TWOHUNDREDFIFTYSIX_STEP", "1/256 Step", ISS_OFF);
    IUFillSwitch(&StepModeS[ONEHUNDREDTWENTYEIGHT_STEP], "ONEHUNDREDTWENTYEIGHT_STEP", "1/128 Step", ISS_OFF);
    IUFillSwitch(&StepModeS[SIXTYFOUR_STEP], "SIXTYFOUR_STEP", "1/64 Step", ISS_OFF);
    IUFillSwitch(&StepModeS[THIRTYSECOND_STEP], "THIRTYSECOND_STEP", "1/32 Step", ISS_OFF);
    IUFillSwitch(&StepModeS[SIXTEENTH_STEP], "SIXTEENTH_STEP", "1/16 Step", ISS_OFF);
    IUFillSwitch(&StepModeS[EIGHTH_STEP], "EIGHTH_STEP", "1/8 Step", ISS_OFF);
    IUFillSwitch(&StepModeS[QUARTER_STEP], "QUARTER_STEP", "1/4 Step", ISS_OFF);
    IUFillSwitch(&StepModeS[HALF_STEP], "HALF_STEP", "1/2 Step", ISS_OFF);
    IUFillSwitch(&StepModeS[FULL_STEP], "FULL_STEP", "Full Step", ISS_OFF);
    IUFillSwitchVector(&StepModeSP, StepModeS, 9, getDeviceName(), "FOCUS_STEP_MODE", "Step Mode", SETTINGS_TAB, IP_RW,
                       ISR_1OFMANY, 0, IPS_IDLE);

    IUFillSwitch(&CoilPowerS[COIL_POWER_ON], "COIL_POWER_ON", "On", ISS_OFF);
    IUFillSwitch(&CoilPowerS[COIL_POWER_OFF], "COIL_POWER_OFF", "Off", ISS_ON);
    IUFillSwitchVector(&CoilPowerSP, CoilPowerS, 2, getDeviceName(), "FOCUS_COIL_POWER", "Coil Power", SETTINGS_TAB, IP_RW,
                       ISR_1OFMANY, 0,
                       IPS_IDLE);

    IUFillSwitch(&DisplayS[DISPLAY_OFF], "DISPLAY_OFF", "Off", ISS_OFF);
    IUFillSwitch(&DisplayS[DISPLAY_ON], "DISPLAY_ON", "On", ISS_ON);
    IUFillSwitchVector(&DisplaySP, DisplayS, 2, getDeviceName(), "FOCUS_DISPLAY", "Display", SETTINGS_TAB, IP_RW, ISR_1OFMANY,
                       0, IPS_IDLE);


    IUFillSwitch(&GotoHomeS[0], "GOTO_HOME", "Go", ISS_OFF);
    IUFillSwitchVector(&GotoHomeSP, GotoHomeS, 1, getDeviceName(), "FOCUS_HOME", "Home", MAIN_CONTROL_TAB, IP_RW,
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
            LOGF_ERROR("Handshake Attempt %i, Connection transmission error: %s.", numChecks, errstr);
        }

        bool responseSuccess;
        if (getActiveConnection()->type() == Connection::Interface::CONNECTION_SERIAL)
        {
            responseSuccess = (rc = tty_read(PortFD, resp, 5, MYFOCUSERPRO2_SERIAL_TIMEOUT, &nbytes_read)) == TTY_OK;
        }
        else
        {
            // assume TCPIP Connection
            responseSuccess = (rc = tty_read(PortFD, resp, 5, MYFOCUSERPRO2_TCPIP_TIMEOUT, &nbytes_read)) == TTY_OK;
        }

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
        // remove check for firmware => 291, assume user is not using older firmware
        LOGF_INFO("MyFP2 reported firmware %d", firmWareVersion);
        LOG_INFO("Connection to focuser is successful.");
        return true;
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
    {
        return false;
    }

    uint32_t temp = 0;

    int rc = sscanf(res, "O%u#", &temp);

    if (rc > 0)
    {
        if(temp == 0)
        {
            CoilPowerS[COIL_POWER_OFF].s = ISS_ON;
        }
        else if (temp == 1)
        {
            CoilPowerS[COIL_POWER_ON].s = ISS_ON;
        }
        else
        {
            LOGF_ERROR("Invalid Response: focuser Coil Power value (%s)", res);
            return false;
        }
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
    {
        if(temp == 0)
        {
            FocusReverseSP[INDI_DISABLED].setState(ISS_ON);
        }
        else if (temp == 1)
        {
            FocusReverseSP[INDI_ENABLED].setState(ISS_ON);
        }
        else
        {
            LOGF_ERROR("Invalid Response: focuser Reverse direction value (%s)", res);
            return false;
        }
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
    {
        return false;
    }

    uint32_t stepmode = 0;

    int rc = sscanf(res, "S%u#", &stepmode);

    if( rc > 0 )
    {
        switch( stepmode )
        {
            case STEPMODE_FULL:
                StepModeS[FULL_STEP].s = ISS_ON;
                break;
            case STEPMODE_HALF:
                StepModeS[HALF_STEP].s = ISS_ON;
                break;
            case STEPMODE_QUARTER:
                StepModeS[QUARTER_STEP].s = ISS_ON;
                break;
            case STEPMODE_EIGHTH:
                StepModeS[EIGHTH_STEP].s = ISS_ON;
                break;
            case STEPMODE_SIXTEENTH:
                StepModeS[SIXTEENTH_STEP].s = ISS_ON;
                break;
            case STEPMODE_THIRTYSECOND:
                StepModeS[THIRTYSECOND_STEP].s = ISS_ON;
                break;
            case STEPMODE_SIXTYFOUR:
                StepModeS[SIXTYFOUR_STEP].s = ISS_ON;
                break;
            case STEPMODE_ONEHUNDREDTWENTYEIGHT:
                StepModeS[ONEHUNDREDTWENTYEIGHT_STEP].s = ISS_ON;
                break;
            case STEPMODE_TWOHUNDREDFIFTYSIX:
                StepModeS[TWOHUNDREDFIFTYSIX_STEP].s = ISS_ON;
                break;
            default:
                LOGF_ERROR("Unknown error: Step mode value (%d)", stepmode);
                return false;
                break;
        }
    }
    else
    {
        LOGF_ERROR("Unknown error: Step mode value (%s)", res);
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
    {
        TemperatureN[0].value = temp;
    }
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
    {
        return false;
    }

    uint32_t temp = 0;

    int rc = sscanf(res, "1%u#", &temp);

    if (rc > 0)
    {
        if(temp == 0)
        {
            TemperatureCompensateS[TEMP_COMPENSATE_DISABLE].s = ISS_ON;
        }
        else if (temp == 1)
        {
            TemperatureCompensateS[TEMP_COMPENSATE_ENABLE].s = ISS_ON;
        }
        else
        {
            LOGF_ERROR("Invalid Response: focuser T.Compensate value (%s)", res);
            return false;
        }
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
    {
        return false;
    }

    int32_t pos;
    int rc = sscanf(res, "%*c%d#", &pos);

    if (rc > 0)
    {
        FocusAbsPosNP[0].setValue(pos);
    }
    else
    {
        LOGF_ERROR("Unknown error: focuser position value (%s)", res);
        return false;
    }
    return true;
}

bool MyFocuserPro2::readTemperatureCoefficient()
{
    char res[ML_RES] = {0};

    if (sendCommand(":26#", res) == false)
    {
        return false;
    }

    int32_t val;
    int rc = sscanf(res, "B%d#", &val);

    if (rc > 0)
    {
        TemperatureSettingN[0].value = val;
    }
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
    {
        return false;
    }

    int speed = 0;
    int rc = sscanf(res, "C%d#", &speed);

    if (rc > 0)
    {
        FocusSpeedNP[0].setValue(speed);
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
    {
        return false;
    }

    uint32_t maxPos = 0;
    int rc = sscanf(res, "M%u#", &maxPos);

    if (rc > 0)
    {
        FocusMaxPosNP[0].setValue(maxPos);
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
    {
        return false;
    }

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
    {
        return false;
    }

    uint32_t temp = 0;
    int rc = sscanf(res, "4%u#", &temp);

    if (rc > 0)
    {
        if(temp == 0)
        {
            BacklashInS[INDI_DISABLED].s = ISS_ON;
        }
        else if (temp == 1)
        {
            BacklashInS[INDI_ENABLED].s = ISS_ON;
        }
        else
        {
            LOGF_ERROR("Unknown Response: focuser Backlash IN enabled (%s)", res);
        }
        return false;
    }
    else
    {
        LOGF_ERROR("Unknown error: focuser Backlash IN enabled (%s)", res);
        return false;
    }
    return true;                // fix missing return statement R Brown June 2020
}

bool MyFocuserPro2::readBacklashOutSteps()
{
    char res[ML_RES] = {0};

    if (sendCommand(":80#", res) == false)
    {
        return false;
    }

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
    {
        return false;
    }

    uint32_t temp = 0;
    int rc = sscanf(res, "5%u#", &temp);

    if (rc > 0)
    {
        if(temp == 0)
        {
            BacklashOutS[INDI_DISABLED].s = ISS_ON;
        }
        else if (temp == 1)
        {
            BacklashOutS[INDI_ENABLED].s = ISS_ON;
        }
        else
        {
            LOGF_ERROR("Unknown response: focuser Backlash OUT enabled (%s)", res);
        }
        return false;
    }
    else
    {
        LOGF_ERROR("Unknown error: focuser Backlash OUT enabled (%s)", res);
        return false;
    }
    return true;                // fix missing return statement R Brown June 2020
}

bool MyFocuserPro2::readDisplayVisible()
{
    char res[ML_RES] = {0};

    if (sendCommand(":37#", res) == false)
    {
        return false;
    }

    uint32_t temp = 0;

    int rc = sscanf(res, "D%u#", &temp);

    if (rc > 0)
    {
        if(temp == 0)
        {
            DisplayS[DISPLAY_OFF].s = ISS_ON;
        }
        else if (temp == 1)
        {
            DisplayS[DISPLAY_ON].s = ISS_ON;
        }
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

    readPosition(); // Fix for Ekos autofocus

    if (sendCommand(":01#", res) == false)
    {
        return false;
    }

    uint32_t temp = 0;

    int rc = sscanf(res, "I%u#", &temp);

    if (rc > 0)
    {
        if(temp == 0)
        {
            return false;
        }
        else if (temp == 1)
        {
            return true;
        }
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
    snprintf(cmd, ML_RES, ":16#");          // fix R Brown July 2021
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
    //snprintf(cmd, ML_RES, ":12%d#", static_cast<int>(enable));
    snprintf(cmd, ML_RES, ":12%d#", enable);
    return sendCommand(cmd);
}

bool MyFocuserPro2::ReverseFocuser(bool enable)
{
    char cmd[ML_RES] = {0};
    snprintf(cmd, ML_RES, ":14%d#", static_cast<int>(enable));
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

bool MyFocuserPro2::setStepMode(FocusStepMode smode)
{
    char cmd[ML_RES] = {0};
    //int setMode = 1 << static_cast<int>(mode);
    //snprintf(cmd, ML_RES, ":30%d#", setMode);           // fix for step modes 64-256 R Brown July 2021
    int stepmode = 1;
    switch( smode )
    {
        case FULL_STEP:
            stepmode = 1;
            break;
        case HALF_STEP:
            stepmode = 2;
            break;
        case QUARTER_STEP:
            stepmode = 4;
            break;
        case EIGHTH_STEP:
            stepmode = 8;
            break;
        case SIXTEENTH_STEP:
            stepmode = 16;
            break;
        case THIRTYSECOND_STEP:
            stepmode = 32;
            break;
        case SIXTYFOUR_STEP:
            stepmode = 64;
            break;
        case ONEHUNDREDTWENTYEIGHT_STEP:
            stepmode = 128;
            break;
        case TWOHUNDREDFIFTYSIX_STEP:
            stepmode = 256;
            break;
        default:
            LOGF_ERROR("Invalid step mode: (%d)", smode);
            return false;
            break;
    }
    snprintf(cmd, ML_RES, ":30%d#", stepmode);
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
    readTemperatureCoefficient();
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
    if (speed != static_cast<int>(FocusSpeedNP[0].getValue()))
    {
        if (!setSpeed(speed))
        {
            return IPS_ALERT;
        }
    }

    // either go all the way in or all the way out
    // then use timer to stop
    if (dir == FOCUS_INWARD)
    {
        MoveFocuser(0);
    }
    else
    {
        MoveFocuser(FocusMaxPosNP[0].getValue());
    }

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
    FocusAbsPosNP.setState(IPS_IDLE);
    FocusRelPosNP.setState(IPS_IDLE);
    FocusTimerNP.setState(IPS_IDLE);
    FocusTimerNP[0].setValue(0);
    FocusAbsPosNP.apply();
    FocusRelPosNP.apply();
    FocusTimerNP.apply();
}

IPState MyFocuserPro2::MoveAbsFocuser(uint32_t targetTicks)
{
    targetPos = targetTicks;

    if (!MoveFocuser(targetPos))
    {
        return IPS_ALERT;
    }
    return IPS_BUSY;
}

IPState MyFocuserPro2::MoveRelFocuser(FocusDirection dir, uint32_t ticks)
{
    int32_t newPosition = 0;

    if (dir == FOCUS_INWARD)
    {
        newPosition = FocusAbsPosNP[0].getValue() - ticks;
    }
    else
    {
        newPosition = FocusAbsPosNP[0].getValue() + ticks;
    }

    // Clamp
    newPosition = std::max(0, std::min(static_cast<int32_t>(FocusAbsPosNP[0].getMax()), newPosition));
    if (!MoveFocuser(newPosition))
    {
        return IPS_ALERT;
    }

    FocusRelPosNP[0].setValue(ticks);
    FocusRelPosNP.setState(IPS_BUSY);

    return IPS_BUSY;
}

// this is occurring every 500ms
void MyFocuserPro2::TimerHit()
{
    if (!isConnected())
    {
        SetTimer(getCurrentPollingPeriod());
        return;
    }

    // update position every 1s
    if (Position_Counter++ == GET_POSITION_FREQ)
    {
        //LOG_INFO("Focuser refresh position.");
        Position_Counter = 0;
        bool rc = readPosition();
        if (rc)
        {
            if (fabs(lastPos - FocusAbsPosNP[0].getValue()) > 5)
            {
                FocusAbsPosNP.apply();
                lastPos = FocusAbsPosNP[0].getValue();
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
                lastPos = FocusAbsPosNP[0].getValue();
                LOG_INFO("Focuser reached requested position.");
            }
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
            lastPos = FocusAbsPosNP[0].getValue();
            LOG_INFO("Focuser reached requested position.");
        }
    }

    // update temperature every 5s
    if (Temperature_Counter++ == GET_TEMPERATURE_FREQ)
    {
        //LOG_INFO("Focuser refresh Temperature.");
        Temperature_Counter = 0;
        bool rc = readTemperature();
        if (rc)
        {
            if (fabs(lastTemperature - TemperatureN[0].value) >= 0.5)
            {
                IDSetNumber(&TemperatureNP, nullptr);
                lastTemperature = TemperatureN[0].value;
            }
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

    IUSaveConfigNumber(fp, &TemperatureSettingNP);
    IUSaveConfigSwitch(fp, &TemperatureCompensateSP);
    IUSaveConfigSwitch(fp, &BacklashInSP);
    IUSaveConfigNumber(fp, &BacklashInStepsNP);
    IUSaveConfigSwitch(fp, &BacklashOutSP);
    IUSaveConfigNumber(fp, &BacklashOutStepsNP);
    IUSaveConfigSwitch(fp, &StepModeSP);
    IUSaveConfigSwitch(fp, &DisplaySP);

    return true;
}

// sleep for a number of milliseconds
int MyFocuserPro2::msleep( long duration)
{
    struct timespec ts;
    int res;

    if (duration < 0)
    {
        errno = EINVAL;
        return -1;
    }

    ts.tv_sec = duration / 1000;
    ts.tv_nsec = (duration % 1000) * 1000000;

    do
    {
        res = nanosleep(&ts, &ts);
    }
    while (res && errno == EINTR);

    return res;
}

void MyFocuserPro2::clearbufferonerror()
{
    bool rc;
    char res[ML_RES] = {0};
    int nbytes_read = 0;

    // if the driver and controller get out of sync, this is an attempt to recover from that situation
    // attempt to clear the receive buffer if the controller response arrived after a serial timeout
    pthread_mutex_lock(&cmdlock);
    msleep(MYFOCUSERPRO2_RECOVER_DELAY);
    if (getActiveConnection()->type() == Connection::Interface::CONNECTION_SERIAL)
    {
        rc = tty_nread_section(PortFD, res, ML_RES, ML_DEL, MYFOCUSERPRO2_SERIAL_TIMEOUT, &nbytes_read);
    }
    else
    {
        // assume TCP/IP Connection
        rc = tty_nread_section(PortFD, res, ML_RES, ML_DEL, MYFOCUSERPRO2_TCPIP_TIMEOUT, &nbytes_read);
    }
    pthread_mutex_unlock(&cmdlock);
    if( rc != TTY_OK)
    {
        //LOGF_ERROR("No data read from controller");
    }
    else
    {
        LOGF_ERROR("Data read from controller: %s.", res);
    }
}

bool MyFocuserPro2::sendCommand(const char * cmd, char * res)
{
    int nbytes_written = 0, nbytes_read = 0, rc = -1;

    tcflush(PortFD, TCIOFLUSH);

    LOGF_DEBUG("CMD <%s>", cmd);

    pthread_mutex_lock(&cmdlock);
    if ((rc = tty_write_string(PortFD, cmd, &nbytes_written)) != TTY_OK)
    {
        char errstr[MAXRBUF] = {0};
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("Connection write error: %s.", errstr);
        pthread_mutex_unlock(&cmdlock);
        return false;
    }

    if (res == nullptr)
    {
        tcdrain(PortFD);
        pthread_mutex_unlock(&cmdlock);
        return true;
    }
    //pthread_mutex_unlock(&cmdlock);

    // sleep for 20ms to give time for controller to process command before attempting to read reply
    msleep(MYFOCUSERPRO2_SMALL_DELAY);

    //pthread_mutex_lock(&cmdlock);
    // replace ML_TIMEOUT, needs to be dependant on connection type
    if (getActiveConnection()->type() == Connection::Interface::CONNECTION_SERIAL)
    {
        rc = tty_nread_section(PortFD, res, ML_RES, ML_DEL, MYFOCUSERPRO2_SERIAL_TIMEOUT, &nbytes_read);
    }
    else
    {
        // assume TCP/IP Connection
        rc = tty_nread_section(PortFD, res, ML_RES, ML_DEL, MYFOCUSERPRO2_TCPIP_TIMEOUT, &nbytes_read);
    }

    if (rc != TTY_OK)
    {
        char errstr[MAXRBUF] = {0};
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("Connection read error: %s.", errstr);
        pthread_mutex_unlock(&cmdlock);
        return false;
    }

    LOGF_DEBUG("RES <%s>", res);

    tcflush(PortFD, TCIOFLUSH);
    pthread_mutex_unlock(&cmdlock);
    return true;
}
