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

void ISGetProperties(const char * dev)
{
    myFocuserPro2->ISGetProperties(dev);
}

void ISNewSwitch(const char * dev, const char * name, ISState * states, char * names[], int n)
{
    myFocuserPro2->ISNewSwitch(dev, name, states, names, n);
}

void ISNewText(const char * dev, const char * name, char * texts[], char * names[], int n)
{
    myFocuserPro2->ISNewText(dev, name, texts, names, n);
}

void ISNewNumber(const char * dev, const char * name, double values[], char * names[], int n)
{
    myFocuserPro2->ISNewNumber(dev, name, values, names, n);
}

void ISNewBLOB(const char * dev, const char * name, int sizes[], int blobsizes[], char * blobs[], char * formats[],
               char * names[], int n)
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

void ISSnoopDevice(XMLEle * root)
{
    myFocuserPro2->ISSnoopDevice(root);
}

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
    BacklashInStepsNP[0].setMin(0);
    BacklashInStepsNP[0].setMax(512);
    BacklashInStepsNP[0].setValue(0);
    BacklashInStepsNP[0].setStep(2);

    BacklashOutStepsNP[0].setMin(0);
    BacklashOutStepsNP[0].setMax(512);
    BacklashOutStepsNP[0].setValue(0);
    BacklashOutStepsNP[0].setStep(2);


    // Backlash In
    BacklashInSP[INDI_ENABLED].fill("INDI_ENABLED", "On", ISS_OFF);
    BacklashInSP[INDI_DISABLED].fill("INDI_DISABLED", "Off", ISS_OFF);
    BacklashInSP.fill(getDeviceName(), "Backlash In", "", OPTIONS_TAB, IP_RW, ISR_1OFMANY, 0,
                       IPS_IDLE);

    BacklashInStepsNP[0].fill("Steps", "", "%3.0f", 0, 512, 2, 0);
    BacklashInStepsNP.fill(getDeviceName(), "Backlash-In", "", OPTIONS_TAB, IP_RW, 0,
                       IPS_IDLE);

    // Backlash Out
    BacklashOutSP[INDI_ENABLED].fill("INDI_ENABLED", "On", ISS_OFF);
    BacklashOutSP[INDI_DISABLED].fill("INDI_DISABLED", "Off", ISS_OFF);
    BacklashOutSP.fill(getDeviceName(), "Backlash Out", "", OPTIONS_TAB, IP_RW, ISR_1OFMANY, 0,
                       IPS_IDLE);

    BacklashOutStepsNP[0].fill("Steps", "", "%3.0f", 0, 512, 2, 0);
    BacklashOutStepsNP.fill(getDeviceName(), "Backlash-Out", "", OPTIONS_TAB, IP_RW, 0,
                       IPS_IDLE);

    // Focuser temperature
    TemperatureNP[0].fill("TEMPERATURE", "Celsius", "%6.2f", -40, 80., 0., 0.);
    TemperatureNP.fill(getDeviceName(), "FOCUS_TEMPERATURE", "Temperature", MAIN_CONTROL_TAB,
                       IP_RO, 0, IPS_IDLE);

    // Temperature Settings
    TemperatureSettingNP[0].fill("Coefficient", "", "%6.2f", 0, 50, 1, 0);
    TemperatureSettingNP.fill(getDeviceName(), "T. Settings", "", OPTIONS_TAB, IP_RW, 0,
                       IPS_IDLE);

    // Compensate for temperature
    TemperatureCompensateSP[TEMP_COMPENSATE_ENABLE].fill("TEMP_COMPENSATE_ENABLE", "Enable", ISS_OFF);
    TemperatureCompensateSP[TEMP_COMPENSATE_DISABLE].fill("TEMP_COMPENSATE_DISABLE", "Disable", ISS_OFF);
    TemperatureCompensateSP.fill(getDeviceName(), "T. Compensate", "", OPTIONS_TAB,
                       IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    // Step Mode
    StepModeSP[FOCUS_THIRTYSECOND_STEP].fill("FOCUS_THIRTYSECOND_STEP", "1/32 Step", ISS_OFF);
    StepModeSP[FOCUS_SIXTEENTH_STEP].fill("FOCUS_SIXTEENTH_STEP", "1/16 Step", ISS_OFF);
    StepModeSP[FOCUS_EIGHTH_STEP].fill("FOCUS_EIGHTH_STEP", "1/8 Step", ISS_OFF);
    StepModeSP[FOCUS_QUARTER_STEP].fill("FOCUS_QUARTER_STEP", "1/4 Step", ISS_OFF);
    StepModeSP[FOCUS_HALF_STEP].fill("FOCUS_HALF_STEP", "1/2 Step", ISS_OFF);
    StepModeSP[FOCUS_FULL_STEP].fill("FOCUS_FULL_STEP", "Full Step", ISS_OFF);
    StepModeSP.fill(getDeviceName(), "Step Mode", "", OPTIONS_TAB, IP_RW, ISR_1OFMANY, 0,
                       IPS_IDLE);


    CoilPowerSP[COIL_POWER_ON].fill("COIL_POWER_ON", "On", ISS_OFF);
    CoilPowerSP[COIL_POWER_OFF].fill("COIL_POWER_OFF", "Off", ISS_OFF);
    CoilPowerSP.fill(getDeviceName(), "Coil Power", "", OPTIONS_TAB, IP_RW, ISR_1OFMANY, 0,
                       IPS_IDLE);

    DisplaySP[DISPLAY_OFF].fill("DISPLAY_OFF", "Off", ISS_OFF);
    DisplaySP[DISPLAY_ON].fill("DISPLAY_ON", "On", ISS_OFF);
    DisplaySP.fill(getDeviceName(), "Display", "", OPTIONS_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);


    GotoHomeSP[0].fill("GOTO_HOME", "Go", ISS_OFF);
    GotoHomeSP.fill(getDeviceName(), "Goto Home Position", "", MAIN_CONTROL_TAB, IP_RW,
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
        defineProperty(GotoHomeSP);
        defineProperty(TemperatureNP);
        defineProperty(TemperatureSettingNP);
        defineProperty(TemperatureCompensateSP);
        defineProperty(BacklashInSP);
        defineProperty(BacklashInStepsNP);
        defineProperty(BacklashOutSP);
        defineProperty(BacklashOutStepsNP);
        defineProperty(StepModeSP);
        defineProperty(DisplaySP);
        defineProperty(CoilPowerSP);

        setTemperatureCelsius();

        LOG_INFO("MyFocuserPro2 parameters updated, focuser ready for use.");
    }
    else
    {
        deleteProperty(GotoHomeSP.getName());
        deleteProperty(TemperatureNP.getName());
        deleteProperty(TemperatureSettingNP.getName());
        deleteProperty(TemperatureCompensateSP.getName());
        deleteProperty(BacklashInSP.getName());
        deleteProperty(BacklashInStepsNP.getName());
        deleteProperty(BacklashOutSP.getName());
        deleteProperty(BacklashOutStepsNP.getName());
        deleteProperty(StepModeSP.getName());
        deleteProperty(DisplaySP.getName());
        deleteProperty(CoilPowerSP.getName());
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
            CoilPowerSP[COIL_POWER_OFF].setState(ISS_ON);
        else if (temp == 1)
            CoilPowerSP[COIL_POWER_ON].setState(ISS_ON);
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
        StepModeSP[FOCUS_FULL_STEP].setState(ISS_ON);
    else if (strcmp(res, "S2#") == 0)
        StepModeSP[FOCUS_HALF_STEP].setState(ISS_ON);
    else if (strcmp(res, "S4#") == 0)
        StepModeSP[FOCUS_QUARTER_STEP].setState(ISS_ON);
    else if (strcmp(res, "S8#") == 0)
        StepModeSP[FOCUS_EIGHTH_STEP].setState(ISS_ON);
    else if (strcmp(res, "S16#") == 0)
        StepModeSP[FOCUS_SIXTEENTH_STEP].setState(ISS_ON);
    else if (strcmp(res, "S32#") == 0)
        StepModeSP[FOCUS_THIRTYSECOND_STEP].setState(ISS_ON);
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
        TemperatureNP[0].setValue(temp);
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
            TemperatureCompensateSP[TEMP_COMPENSATE_DISABLE].setState(ISS_ON);
        else if (temp == 1)
            TemperatureCompensateSP[TEMP_COMPENSATE_ENABLE].setState(ISS_ON);
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
        FocusAbsPosNP[0].setValue(pos);
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
        TemperatureSettingNP[0].setValue(val);
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
        return false;

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
        return false;

    uint32_t backlash = 0;
    int rc = sscanf(res, "6%u#", &backlash);

    if (rc > 0)
    {
        BacklashInStepsNP[0].setValue(backlash);
    }
    else
    {
        BacklashInStepsNP[0].setValue(0);
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
            BacklashInSP[INDI_DISABLED].setState(ISS_ON);
        else if (temp == 1)
            BacklashInSP[INDI_ENABLED].setState(ISS_ON);
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
        BacklashOutStepsNP[0].setValue(backlash);
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
            BacklashOutSP[INDI_DISABLED].setState(ISS_ON);
        else if (temp == 1)
            BacklashOutSP[INDI_ENABLED].setState(ISS_ON);
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
            DisplaySP[DISPLAY_OFF].setState(ISS_ON);
        else if (temp == 1)
            DisplaySP[DISPLAY_ON].setState(ISS_ON);
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
    snprintf(cmd, ML_RES, ":12%01d#", static_cast<int>(enable));
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

            bool rc = setStepMode(static_cast<FocusStepMode>(target_mode));
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

        // Goto Home Position
        if (GotoHomeSP.isNameMatch(name))
        {
            bool rc = setGotoHome();
            if (!rc)
            {
                GotoHomeSP.reset();
                CoilPowerSP.setState(IPS_ALERT);
                GotoHomeSP.apply();
                return false;
            }

            GotoHomeSP.setState(IPS_OK);
            GotoHomeSP.apply();
            return true;
        }

        // Coil Power Mode
        if (CoilPowerSP.isNameMatch(name))
        {
            int current_mode = CoilPowerSP.findOnSwitchIndex();

            CoilPowerSP.update(states, names, n);

            int target_mode = CoilPowerSP.findOnSwitchIndex();

            if (current_mode == target_mode)
            {
                CoilPowerSP.setState(IPS_OK);
                CoilPowerSP.apply();
            }

            bool rc = setCoilPowerState(static_cast<CoilPower>(target_mode));
            if (!rc)
            {
                CoilPowerSP.reset();
                CoilPowerSP[current_mode].setState(ISS_ON);
                CoilPowerSP.setState(IPS_ALERT);
                CoilPowerSP.apply();
                return false;
            }

            CoilPowerSP.setState(IPS_OK);
            CoilPowerSP.apply();
            return true;
        }


        // Display Control
        if (DisplaySP.isNameMatch(name))
        {
            int current_mode = DisplaySP.findOnSwitchIndex();

            DisplaySP.update(states, names, n);

            int target_mode = DisplaySP.findOnSwitchIndex();

            if (current_mode == target_mode)
            {
                DisplaySP.setState(IPS_OK);
                DisplaySP.apply();
            }

            bool rc = setDisplayVisible(static_cast<DisplayMode>(target_mode));
            if (!rc)
            {
                DisplaySP.reset();
                DisplaySP[current_mode].setState(ISS_ON);
                DisplaySP.setState(IPS_ALERT);
                DisplaySP.apply();
                return false;
            }

            DisplaySP.setState(IPS_OK);
            DisplaySP.apply();
            return true;
        }

        // Backlash In Enable
        if (BacklashInSP.isNameMatch(name))
        {
            int current_mode = BacklashInSP.findOnSwitchIndex();

            BacklashInSP.update(states, names, n);

            int target_mode = BacklashInSP.findOnSwitchIndex();

            if (current_mode == target_mode)
            {
                BacklashInSP.setState(IPS_OK);
                BacklashInSP.apply();
            }

            bool rc = setBacklashInEnabled(target_mode == INDI_ENABLED);
            if (!rc)
            {
                BacklashInSP.reset();
                BacklashInSP[current_mode].setState(ISS_ON);
                BacklashInSP.setState(IPS_ALERT);
                BacklashInSP.apply();
                return false;
            }

            BacklashInSP.setState(IPS_OK);
            BacklashInSP.apply();
            return true;
        }

        // Backlash Out Enable
        if (BacklashOutSP.isNameMatch(name))
        {
            int current_mode = BacklashOutSP.findOnSwitchIndex();

            BacklashOutSP.update(states, names, n);

            int target_mode = BacklashOutSP.findOnSwitchIndex();

            if (current_mode == target_mode)
            {
                BacklashOutSP.setState(IPS_OK);
                BacklashOutSP.apply();
            }

            bool rc = setBacklashOutEnabled(target_mode == INDI_ENABLED);
            if (!rc)
            {
                BacklashOutSP.reset();
                BacklashOutSP[current_mode].setState(ISS_ON);
                BacklashOutSP.setState(IPS_ALERT);
                BacklashOutSP.apply();
                return false;
            }

            BacklashOutSP.setState(IPS_OK);
            BacklashOutSP.apply();
            return true;
        }

        // Temperature Compensation Mode
        if (TemperatureCompensateSP.isNameMatch(name))
        {
            int last_index = TemperatureCompensateSP.findOnSwitchIndex();
            TemperatureCompensateSP.update(states, names, n);

            bool rc = setTemperatureCompensation((TemperatureCompensateSP[0].getState() == ISS_ON));

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

bool MyFocuserPro2::ISNewNumber(const char * dev, const char * name, double values[], char * names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        // Temperature Settings
        if (TemperatureSettingNP.isNameMatch(name))
        {
            TemperatureSettingNP.update(values, names, n);
            if (!setTemperatureCoefficient(TemperatureSettingNP[0].value))
            {
                TemperatureSettingNP.setState(IPS_ALERT);
                TemperatureSettingNP.apply();
                return false;
            }

            TemperatureSettingNP.setState(IPS_OK);
            TemperatureSettingNP.apply();
            return true;
        }

        // Backlash In
        if (BacklashInStepsNP.isNameMatch(name))
        {
            BacklashInStepsNP.update(values, names, n);
            if (!setBacklashInSteps(BacklashInStepsNP[0].value))
            {
                BacklashInStepsNP.setState(IPS_ALERT);
                BacklashInStepsNP.apply();
                return false;
            }

            BacklashInStepsNP.setState(IPS_OK);
            BacklashInStepsNP.apply();
            return true;
        }

        // Backlash Out
        if (BacklashOutStepsNP.isNameMatch(name))
        {
            BacklashOutStepsNP.update(values, names, n);
            if (!setBacklashOutSteps(BacklashOutStepsNP[0].value))
            {
                BacklashOutStepsNP.setState(IPS_ALERT);
                BacklashOutStepsNP.apply();
                return false;
            }

            BacklashOutStepsNP.setState(IPS_OK);
            BacklashOutStepsNP.apply();
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
    if (speed != static_cast<int>(FocusSpeedNP[0].value))
    {
        if (!setSpeed(speed))
            return IPS_ALERT;
    }

    // either go all the way in or all the way out
    // then use timer to stop
    if (dir == FOCUS_INWARD)
        MoveFocuser(0);
    else
        MoveFocuser(FocusMaxPosNP[0].value);

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
        return IPS_ALERT;

    return IPS_BUSY;
}

IPState MyFocuserPro2::MoveRelFocuser(FocusDirection dir, uint32_t ticks)
{
    int32_t newPosition = 0;

    if (dir == FOCUS_INWARD)
        newPosition = FocusAbsPosNP[0].getValue() - ticks;
    else
        newPosition = FocusAbsPosNP[0].getValue() + ticks;

    // Clamp
    newPosition = std::max(0, std::min(static_cast<int32_t>(FocusAbsPosNP[0].max), newPosition));
    if (!MoveFocuser(newPosition))
        return IPS_ALERT;

    FocusRelPosNP[0].setValue(ticks);
    FocusRelPosNP.setState(IPS_BUSY);

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
        if (fabs(lastPos - FocusAbsPosNP[0].getValue()) > 5)
        {
            FocusAbsPosNP.apply();
            lastPos = FocusAbsPosNP[0].getValue();
        }
    }

    rc = readTemperature();
    if (rc)
    {
        if (fabs(lastTemperature - TemperatureNP[0].getValue()) >= 0.5)
        {
            TemperatureNP.apply();
            lastTemperature = TemperatureNP[0].getValue();
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

    SetTimer(getCurrentPollingPeriod());
}

bool MyFocuserPro2::AbortFocuser()
{
    return sendCommand(":27#");
}

bool MyFocuserPro2::saveConfigItems(FILE * fp)
{
    Focuser::saveConfigItems(fp);

    StepModeSP.save(fp);

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
