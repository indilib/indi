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
    FI::SetCapability(FOCUSER_CAN_ABS_MOVE | FOCUSER_CAN_REL_MOVE | FOCUSER_CAN_ABORT | FOCUSER_HAS_VARIABLE_SPEED |
                      FOCUSER_CAN_SYNC);
    setSupportedConnections(CONNECTION_SERIAL);


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

    /*
    //Focus Speed
    IUFillSwitch(&FocusSpeedS[FOCUS_SPEED_SLOW], "FOCUS_SPEED_SLOW", "Slow", ISS_OFF);
    IUFillSwitch(&FocusSpeedS[FOCUS_SPEED_MEDIUM], "FOCUS_SPEED_MEDIUM", "Slow", ISS_OFF);
    IUFillSwitch(&FocusSpeedS[FOCUS_SPEED_FAST], "FOCUS_SPEED_FAST", "Slow", ISS_OFF);
    IUFillSwitchVector(&FocusSpeedSP, FocusSpeedS, 3, getDeviceName(), "Focus Speed", "", MAIN_CONTROL_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);
    */

    // Focuser temperature
    IUFillNumber(&TemperatureN[0], "TEMPERATURE", "Celsius", "%6.2f", -40, 80., 0., 0.);
    IUFillNumberVector(&TemperatureNP, TemperatureN, 1, getDeviceName(), "FOCUS_TEMPERATURE", "Temperature", MAIN_CONTROL_TAB, IP_RO, 0, IPS_IDLE);

    // Temperature Settings
    IUFillNumber(&TemperatureSettingN[0], "Coefficient", "", "%6.2f", 0, 50, 1, 0);
    IUFillNumberVector(&TemperatureSettingNP, TemperatureSettingN, 1, getDeviceName(), "T. Settings", "", OPTIONS_TAB, IP_RW, 0, IPS_IDLE);

    // Compensate for temperature
    IUFillSwitch(&TemperatureCompensateS[TEMP_COMPENSATE_ENABLE], "TEMP_COMPENSATE_ENABLE", "Enable", ISS_OFF);
    IUFillSwitch(&TemperatureCompensateS[TEMP_COMPENSATE_DISABLE], "TEMP_COMPENSATE_DISABLE", "Disable", ISS_OFF);
    IUFillSwitchVector(&TemperatureCompensateSP, TemperatureCompensateS, 2, getDeviceName(), "T. Compensate", "", MAIN_CONTROL_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);


    // Step Mode
    IUFillSwitch(&StepModeS[FOCUS_THIRTYSECOND_STEP], "FOCUS_THIRTYSECOND_STEP", "1/32 Step", ISS_OFF);
    IUFillSwitch(&StepModeS[FOCUS_SIXTEENTH_STEP], "FOCUS_SIXTEENTH_STEP", "1/16 Step", ISS_OFF);
    IUFillSwitch(&StepModeS[FOCUS_EIGHTH_STEP], "FOCUS_EIGHTH_STEP", "1/8 Step", ISS_OFF);
    IUFillSwitch(&StepModeS[FOCUS_QUARTER_STEP], "FOCUS_QUARTER_STEP", "1/4 Step", ISS_OFF);
    IUFillSwitch(&StepModeS[FOCUS_HALF_STEP], "FOCUS_HALF_STEP", "1/2 Step", ISS_OFF);
    IUFillSwitch(&StepModeS[FOCUS_FULL_STEP], "FOCUS_FULL_STEP", "Full Step", ISS_OFF);
    IUFillSwitchVector(&StepModeSP, StepModeS, 6, getDeviceName(), "Step Mode", "", OPTIONS_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);


    IUFillSwitch(&CoilPowerS[COIL_POWER_ON], "COIL_POWER_ON","On", ISS_OFF);
    IUFillSwitch(&CoilPowerS[COIL_POWER_OFF], "COIL_POWER_OFF","Off", ISS_OFF);
    IUFillSwitchVector(&CoilPowerSP, CoilPowerS, 2, getDeviceName(), "Coil Power", "", OPTIONS_TAB, IP_RW, ISR_1OFMANY,0, IPS_IDLE);

    IUFillSwitch(&DisplayS[DISPLAY_OFF], "DISPLAY_OFF","Off", ISS_OFF);
    IUFillSwitch(&DisplayS[DISPLAY_ON], "DISPLAY_ON","On", ISS_OFF);
    IUFillSwitchVector(&DisplaySP, DisplayS, 2, getDeviceName(), "Display", "", OPTIONS_TAB, IP_RW, ISR_1OFMANY,0, IPS_IDLE);

    IUFillSwitch(&ReverseDirectionS[REVERSE_DIRECTION_ON], "REVERSE_DIRECTION_ON","On", ISS_OFF);
    IUFillSwitch(&ReverseDirectionS[REVERSE_DIRECTION_OFF], "REVERSE_DIRECTION_OFF","Off", ISS_OFF);
    IUFillSwitchVector(&ReverseDirectionSP, ReverseDirectionS, 2, getDeviceName(), "Reverse Direction", "", OPTIONS_TAB, IP_RW, ISR_1OFMANY,0, IPS_IDLE);

    setDefaultPollingPeriod(500);
    addDebugControl();

    return true;
}

bool MyFocuserPro2::updateProperties()
{
    INDI::Focuser::updateProperties();

    if (isConnected())
    {
        defineNumber(&TemperatureNP);
        defineNumber(&TemperatureSettingNP);
        defineSwitch(&TemperatureCompensateSP);
        defineSwitch(&StepModeSP);
        defineSwitch(&DisplaySP);
        defineSwitch(&CoilPowerSP);
        defineSwitch(&ReverseDirectionSP);

        setTemperatureCelsius();
        GetFocusParams();

        LOG_INFO("MyFocuserPro2 paramaters updated, focuser ready for use.");
    }
    else
    {
        deleteProperty(TemperatureNP.name);
        deleteProperty(TemperatureSettingNP.name);
        deleteProperty(TemperatureCompensateSP.name);
        deleteProperty(StepModeSP.name);
        deleteProperty(DisplaySP.name);
        deleteProperty(CoilPowerSP.name);
        deleteProperty(ReverseDirectionSP.name);
    }

    return true;
}

bool MyFocuserPro2::Handshake()
{
    if (Ack())
    {
        LOG_INFO("MyFocuserPro2 is online. Getting focus parameters...");
        return true;
    }

    LOG_INFO(
        "Error retreiving data from MyFocuserPro2, please ensure MyFocuserPro2 controller is powered and the port is correct.");
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
    char resp[5]= {0};
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

    if (rc >0)
    {
        if(firmWareVersion >= minimumFirwareVersion)
        {
            LOGF_INFO("MyFP2 reported firmware %d", firmWareVersion);
            return true;

        }
        else
        {
            LOGF_ERROR("Invalid Firmware: focuser firmware version value %d, minimum supported is %d", firmWareVersion, minimumFirwareVersion );
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
    char res[ML_RES]= {0};

    if (sendCommand(":11#", res) == false)
        return false;

    uint32_t temp = 0;

    int rc = sscanf(res, "O%d#", &temp);

    if (rc > 0)

        if(temp==0)
            CoilPowerS[COIL_POWER_OFF].s = ISS_ON;
        else if (temp==1)
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
    char res[ML_RES]= {0};

    if (sendCommand(":13#", res) == false)
        return false;

    uint32_t temp = 0;

    int rc = sscanf(res, "R%d#", &temp);

    if (rc > 0)

        if(temp==0)
            ReverseDirectionS[REVERSE_DIRECTION_OFF].s = ISS_ON;
        else if (temp==1)
            ReverseDirectionS[REVERSE_DIRECTION_ON].s = ISS_ON;
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
    char res[ML_RES]= {0};

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
    char res[ML_RES]= {0};

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
    char res[ML_RES]= {0};

    if (sendCommand(":24#", res) == false)
        return false;

    uint32_t temp = 0;

    int rc = sscanf(res, "1%d#", &temp);

    if (rc > 0)

        if(temp==0)
            TemperatureCompensateS[TEMP_COMPENSATE_DISABLE].s = ISS_ON;
        else if (temp==1)
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
    char res[ML_RES]= {0};

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
    char res[ML_RES]= {0};

    if (sendCommand(":26#", res) == false)
        return false;

    int32_t val;
    int rc = sscanf(res, "B%d#", &val);

    if (rc > 0)
        TemperatureSettingN[1].value = val;
    else
    {
        LOGF_ERROR("Unknown error: Temperature Coefficient value (%s)", res);
        return false;
    }

    return true;
}

bool MyFocuserPro2::readSpeed()
{
    char res[ML_RES]= {0};

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
    char res[ML_RES]= {0};

    if (sendCommand(":08#", res) == false)
        return false;

    uint32_t maxPos = 0;
    int rc = sscanf(res, "M%d#", &maxPos);

    if (rc > 0)
    {
        FocusMaxPosN[0].value = maxPos;
    }
    else
    {
        LOGF_ERROR("Unknown error: focuser max position value (%s)", res);
        return false;
    }

    return true;
}

bool MyFocuserPro2::readDisplayVisible()
{
    char res[ML_RES]= {0};

    if (sendCommand(":37#", res) == false)
        return false;

    uint32_t temp = 0;

    int rc = sscanf(res, "D%d#", &temp);

    if (rc > 0)

        if(temp==0)
            DisplayS[DISPLAY_OFF].s = ISS_ON;
        else if (temp==1)
            DisplayS[DISPLAY_ON].s = ISS_ON;
        else
        {
            LOGF_ERROR("Invalid Response: focuser Display value (%s)", res);
            return false;
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
    char res[ML_RES]= {0};

    if (sendCommand(":01#", res) == false)
        return false;

    if (strcmp(res, "I01#") == 0)
        return true;
    else if (strcmp(res, "I00#") == 0)
        return false;

    LOGF_ERROR("Unknown error: isMoving value (%s)", res);
    return false;
}


bool MyFocuserPro2::setTemperatureCelsius()
{
    char cmd[ML_RES]= {0};
    snprintf(cmd, ML_RES, ":161#");
    return sendCommand(cmd);
}

bool MyFocuserPro2::setTemperatureCoefficient(double coefficient)
{
    char cmd[ML_RES]= {0};
    int coeff = coefficient;
    snprintf(cmd, ML_RES, ":22%d#", coeff);
    return sendCommand(cmd);
}

bool MyFocuserPro2::SyncFocuser(uint32_t ticks)
{
    char cmd[ML_RES]= {0};
    snprintf(cmd, ML_RES, ":31%d#", ticks);
    return sendCommand(cmd);
}

bool MyFocuserPro2::MoveFocuser(uint32_t position)
{
    char cmd[ML_RES]= {0};
    snprintf(cmd, ML_RES, ":05%d#", position);
    // Set Position First
    if (sendCommand(cmd) == false)
        return false;

    return true;
}

bool MyFocuserPro2::setCoilPowerState(CoilPower enable)
{
    char cmd[ML_RES]= {0};
    snprintf(cmd, ML_RES, ":12%02d#", (int)enable);
    return sendCommand(cmd);
}

bool MyFocuserPro2::setReverseDirection(ReverseDirection enable)
{
    char cmd[ML_RES]= {0};
    snprintf(cmd, ML_RES, ":14%02d#", (int)enable);
    return sendCommand(cmd);
}

bool MyFocuserPro2::setDisplayVisible(DisplayMode enable)
{
    char cmd[ML_RES]= {0};
    snprintf(cmd, ML_RES, ":36%d#", enable);
    return sendCommand(cmd);
}

bool MyFocuserPro2::setStepMode(FocusStepMode mode)
{
    char cmd[ML_RES]= {0};
    int setMode=1 << (int)mode;
    snprintf(cmd, ML_RES, ":30%02d#", setMode);
    return sendCommand(cmd);
}

bool MyFocuserPro2::setSpeed(uint16_t speed)
{
    char cmd[ML_RES]= {0};
    snprintf(cmd, ML_RES, ":150%d#", speed);
    return sendCommand(cmd);
}

bool MyFocuserPro2::setTemperatureCompensation(bool enable)
{
    char cmd[ML_RES]= {0};
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

            bool rc = setStepMode((FocusStepMode)target_mode);
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

            bool rc = setCoilPowerState((CoilPower)target_mode);
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

        // Reverse Direction
        if (strcmp(ReverseDirectionSP.name, name) == 0)
        {
            int current_mode = IUFindOnSwitchIndex(&ReverseDirectionSP);

            IUUpdateSwitch(&ReverseDirectionSP, states, names, n);

            int target_mode = IUFindOnSwitchIndex(&ReverseDirectionSP);

            if (current_mode == target_mode)
            {
                ReverseDirectionSP.s = IPS_OK;
                IDSetSwitch(&ReverseDirectionSP, nullptr);
            }

            bool rc = setReverseDirection((ReverseDirection)target_mode);
            if (!rc)
            {
                IUResetSwitch(&ReverseDirectionSP);
                ReverseDirectionS[current_mode].s = ISS_ON;
                ReverseDirectionSP.s              = IPS_ALERT;
                IDSetSwitch(&ReverseDirectionSP, nullptr);
                return false;
            }

            ReverseDirectionSP.s = IPS_OK;
            IDSetSwitch(&ReverseDirectionSP, nullptr);
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

            bool rc = setDisplayVisible((DisplayMode)target_mode);
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
            if (!setTemperatureCoefficient(TemperatureSettingN[1].value))
            {
                TemperatureSettingNP.s = IPS_ALERT;
                IDSetNumber(&TemperatureSettingNP, nullptr);
                return false;
            }

            TemperatureSettingNP.s = IPS_OK;
            IDSetNumber(&TemperatureSettingNP, nullptr);
            return true;
        }
    }

    return INDI::Focuser::ISNewNumber(dev, name, values, names, n);
}

void MyFocuserPro2::GetFocusParams()
{
    if (readMaxPos())
        IDSetNumber(&FocusMaxPosNP, nullptr);

    if (readPosition())
        IDSetNumber(&FocusAbsPosNP, nullptr);

    if (readTemperature())
        IDSetNumber(&TemperatureNP, nullptr);

    if (readTempeartureCoefficient())
        IDSetNumber(&TemperatureSettingNP, nullptr);

    if (readSpeed())
        IDSetNumber(&FocusSpeedNP, nullptr);

    if (readTempCompensateEnable())
        IDSetSwitch(&TemperatureCompensateSP, nullptr);

    if (readStepMode())
        IDSetSwitch(&StepModeSP, nullptr);

    if (readCoilPowerState())
        IDSetSwitch(&CoilPowerSP, nullptr);

    if (readDisplayVisible())
        IDSetSwitch(&DisplaySP, nullptr);

    if (readReverseDirection())
        IDSetSwitch(&ReverseDirectionSP, nullptr);

}

bool MyFocuserPro2::SetFocuserSpeed(int speed)
{
    return setSpeed(speed);
}

bool MyFocuserPro2::SetFocuserMaxPosition(uint32_t maxPos)
{
    char cmd[ML_RES]= {0};

    snprintf(cmd, ML_RES, ":07%06d#",maxPos);

    if(sendCommand(cmd))
    {
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
        SetTimer(POLLMS);
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

    SetTimer(POLLMS);
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
        char errstr[MAXRBUF]= {0};
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("Serial write error: %s.", errstr);
        return false;
    }

    if (res == nullptr)
        return true;

    if ((rc = tty_nread_section(PortFD, res, ML_RES, ML_DEL, ML_TIMEOUT, &nbytes_read)) != TTY_OK)
    {
        char errstr[MAXRBUF]= {0};
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("Serial read error: %s.", errstr);
        return false;
    }

    LOGF_DEBUG("RES <%s>", res);

    tcflush(PortFD, TCIOFLUSH);

    return true;
}
