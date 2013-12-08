/*
    Moonlite Focuser
    Copyright (C) 2013 Jasem Mutlaq (mutlaqja@ikarustech.com)

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

#include "moonlite.h"
#include "indicom.h"

#include <stdio.h>
#include <termios.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>
#include <math.h>
#include <memory>

#define MOONLITE_TIMEOUT   10

#define POLLMS  250

std::auto_ptr<MoonLite> moonLite(0);

void ISInit()
{
    static int isInit =0;

    if (isInit == 1)
        return;

     isInit = 1;
     if(moonLite.get() == 0) moonLite.reset(new MoonLite());
}

void ISGetProperties(const char *dev)
{
         ISInit();
         moonLite->ISGetProperties(dev);
}

void ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int num)
{
         ISInit();
         moonLite->ISNewSwitch(dev, name, states, names, num);
}

void ISNewText(	const char *dev, const char *name, char *texts[], char *names[], int num)
{
         ISInit();
         moonLite->ISNewText(dev, name, texts, names, num);
}

void ISNewNumber(const char *dev, const char *name, double values[], char *names[], int num)
{
         ISInit();
         moonLite->ISNewNumber(dev, name, values, names, num);
}

void ISNewBLOB (const char *dev, const char *name, int sizes[], int blobsizes[], char *blobs[], char *formats[], char *names[], int n)
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
     ISInit();
}

MoonLite::MoonLite()
{

    // Can move in Absolute & Relative motions, can Abort motion, and has variable speed.
    setFocuserFeatures(true, true, true, true);

    lastPos = 0;
    lastTemperature = 0;
}

MoonLite::~MoonLite()
{

}

bool MoonLite::initProperties()
{
    INDI::Focuser::initProperties();

    FocusSpeedN[0].min = 1;
    FocusSpeedN[0].max = 5;
    FocusSpeedN[0].value = 1;    

    /* Step Mode */
    IUFillSwitch(&StepModeS[0], "Half Step", "", ISS_OFF);
    IUFillSwitch(&StepModeS[1], "Full Step", "", ISS_ON);
    IUFillSwitchVector(&StepModeSP, StepModeS, 2, getDeviceName(), "Step Mode", "", OPTIONS_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    /* Focuser temperature */
    IUFillNumber(&TemperatureN[0], "TEMPERATURE", "Celsius", "%6.2f", -50, 70., 0., 0.);
    IUFillNumberVector(&TemperatureNP, TemperatureN, 1, getDeviceName(), "FOCUS_TEMPERATURE", "Temperature", MAIN_CONTROL_TAB, IP_RO, 0, IPS_IDLE);

    // Maximum Travel
    IUFillNumber(&MaxTravelN[0], "MAXTRAVEL", "Maximum travel", "%6.0f", 1., 60000., 0., 10000.);
    IUFillNumberVector(&MaxTravelNP, MaxTravelN, 1, getDeviceName(), "FOCUS_MAXTRAVEL", "Max. travel", OPTIONS_TAB, IP_RW, 0, IPS_IDLE );

    // Temperature Settings
    IUFillNumber(&TemperatureSettingN[0], "Calibration", "", "%6.2f", -20, 20, 0.5, 0);
    IUFillNumber(&TemperatureSettingN[1], "Coefficient", "", "%6.2f", -20, 20, 0.5, 0);
    IUFillNumberVector(&TemperatureSettingNP, TemperatureSettingN, 2, getDeviceName(), "Temperature Settings", "", OPTIONS_TAB, IP_RW, 0, IPS_IDLE);

    // Compensate for temperature
    IUFillSwitch(&TemperatureCompensateS[0], "Enable", "", ISS_OFF);
    IUFillSwitch(&TemperatureCompensateS[1], "Disable", "", ISS_ON);
    IUFillSwitchVector(&TemperatureCompensateSP, TemperatureCompensateS, 2, getDeviceName(), "Temperature Compensate", "", MAIN_CONTROL_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    // Presets
    IUFillNumber(&PresetN[0], "Preset 1", "", "%6.2f", 0, 60000, 1000, 0);
    IUFillNumber(&PresetN[1], "Preset 2", "", "%6.2f", 0, 60000, 1000, 0);
    IUFillNumber(&PresetN[2], "Preset 3", "", "%6.2f", 0, 60000, 1000, 0);
    IUFillNumberVector(&PresetNP, PresetN, 3, getDeviceName(), "Presets", "", "Presets", IP_RW, 0, IPS_IDLE);

    //Preset GOTO
    IUFillSwitch(&PresetGotoS[0], "Preset 1", "", ISS_OFF);
    IUFillSwitch(&PresetGotoS[1], "Preset 2", "", ISS_OFF);
    IUFillSwitch(&PresetGotoS[2], "Preset 3", "", ISS_OFF);
    IUFillSwitchVector(&PresetGotoSP, PresetGotoS, 3, getDeviceName(), "Goto", "", "Presets", IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    /* Relative and absolute movement */
    FocusRelPosN[0].min = 0.;
    FocusRelPosN[0].max = 30000.;
    FocusRelPosN[0].value = 0;
    FocusRelPosN[0].step = 1000;

    FocusAbsPosN[0].min = 0.;
    FocusAbsPosN[0].max = 30000.;
    FocusAbsPosN[0].value = 0;
    FocusAbsPosN[0].step = 1000;

    addDebugControl();

    return true;

}

void MoonLite::ISGetProperties(const char *dev)
{
    INDI::Focuser::ISGetProperties(dev);

    defineText(&PortTP);


}

bool MoonLite::updateProperties()
{
    INDI::Focuser::updateProperties();

    if (isConnected())
    {
        defineNumber(&TemperatureNP);
        defineNumber(&MaxTravelNP);
        defineSwitch(&StepModeSP);
        defineNumber(&TemperatureSettingNP);
        defineSwitch(&TemperatureCompensateSP);
        defineNumber(&PresetNP);
        defineSwitch(&PresetGotoSP);

        GetFocusParams();

        loadConfig();

        DEBUG(INDI::Logger::DBG_SESSION, "MoonLite paramaters updated, focuser ready for use.");
    }
    else
    {

        deleteProperty(TemperatureNP.name);
        deleteProperty(MaxTravelNP.name);
        deleteProperty(StepModeSP.name);
        deleteProperty(TemperatureSettingNP.name);
        deleteProperty(TemperatureCompensateSP.name);
        deleteProperty(PresetNP.name);
        deleteProperty(PresetGotoSP.name);
    }

    return true;

}

bool MoonLite::Connect()
{
    int connectrc=0;
    char errorMsg[MAXRBUF];

    if ( (connectrc = tty_connect(PortT[0].text, 9600, 8, 0, 1, &PortFD)) != TTY_OK)
    {
        tty_error_msg(connectrc, errorMsg, MAXRBUF);

        DEBUGF(INDI::Logger::DBG_SESSION, "Failed to connect to port %s. Error: %s", PortT[0].text, errorMsg);

        return false;

    }

    DEBUG(INDI::Logger::DBG_SESSION, "MoonLite is online. Getting focus parameters...");

    SetTimer(POLLMS);

    return true;
}

bool MoonLite::Disconnect()
{
    tty_disconnect(PortFD);
    DEBUG(INDI::Logger::DBG_SESSION, "MoonLite is offline.");
    return true;
}

const char * MoonLite::getDefaultName()
{
    return "MoonLite";
}

bool MoonLite::updateStepMode()
{
    int nbytes_written=0, nbytes_read=0, rc=-1;
    char errstr[MAXRBUF];
    char resp[3];

    tcflush(PortFD, TCIOFLUSH);

    if ( (rc = tty_write(PortFD, ":GH#", 4, &nbytes_written)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        DEBUGF(INDI::Logger::DBG_ERROR, "updateStepMode error: %s.", errstr);
        return false;
    }

    if ( (rc = tty_read(PortFD, resp, 3, MOONLITE_TIMEOUT, &nbytes_read)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        DEBUGF(INDI::Logger::DBG_ERROR, "updateStepMode error: %s.", errstr);
        return false;
    }

    resp[3]='\0';
    IUResetSwitch(&StepModeSP);

    if (!strcmp(resp, "FF#"))
        StepModeS[0].s = ISS_ON;
    else if (!strcmp(resp, "00#"))
        StepModeS[1].s = ISS_ON;
    else
    {
        DEBUGF(INDI::Logger::DBG_ERROR, "Unknown error: focuser step value (%s)", resp);
        return false;
    }

  return true;
}

bool MoonLite::updateTemperature()
{
    int nbytes_written=0, nbytes_read=0, rc=-1;
    char errstr[MAXRBUF];
    char resp[5];
    unsigned int temp;

    tcflush(PortFD, TCIOFLUSH);

    tty_write(PortFD, ":C#", 3, &nbytes_written);

    if ( (rc = tty_write(PortFD, ":GT#", 4, &nbytes_written)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        DEBUGF(INDI::Logger::DBG_ERROR, "updateTemperature error: %s.", errstr);
        return false;
    }

    if ( (rc = tty_read(PortFD, resp, 5, MOONLITE_TIMEOUT, &nbytes_read)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        DEBUGF(INDI::Logger::DBG_ERROR, "updateTemperature error: %s.", errstr);
        return false;
    }

    rc = sscanf(resp, "%X#", &temp);

    if (rc > 0)
    {
        TemperatureN[0].value = ((int) temp)/2.0;
    }
    else
    {
        DEBUGF(INDI::Logger::DBG_ERROR, "Unknown error: focuser temperature value (%s)", resp);
        return false;
    }

  return true;
}

bool MoonLite::updatePosition()
{
    int nbytes_written=0, nbytes_read=0, rc=-1;
    char errstr[MAXRBUF];
    char resp[5];
    short pos=-1;

    if ( (rc = tty_write(PortFD, ":GP#", 4, &nbytes_written)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        DEBUGF(INDI::Logger::DBG_ERROR, "updatePostion error: %s.", errstr);
        return false;
    }

    if ( (rc = tty_read(PortFD, resp, 5, MOONLITE_TIMEOUT, &nbytes_read)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        DEBUGF(INDI::Logger::DBG_ERROR, "updatePostion error: %s.", errstr);
        return false;
    }

    rc = sscanf(resp, "%hX#", &pos);

    if (rc > 0)
    {
        FocusAbsPosN[0].value = pos;
    }
    else
    {
        DEBUGF(INDI::Logger::DBG_ERROR, "Unknown error: focuser position value (%s)", resp);
        return false;
    }

  return true;
}

bool MoonLite::updateSpeed()
{
    int nbytes_written=0, nbytes_read=0, rc=-1;
    char errstr[MAXRBUF];
    char resp[3];
    short speed;

    tcflush(PortFD, TCIOFLUSH);

    if ( (rc = tty_write(PortFD, ":GD#", 4, &nbytes_written)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        DEBUGF(INDI::Logger::DBG_ERROR, "updateSpeed error: %s.", errstr);
        return false;
    }

    if ( (rc = tty_read(PortFD, resp, 3, MOONLITE_TIMEOUT, &nbytes_read)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        DEBUGF(INDI::Logger::DBG_ERROR, "updateSpeed error: %s.", errstr);
        return false;
    }

    rc = sscanf(resp, "%hX#", &speed);

    if (rc > 0)
    {
        int focus_speed=-1;
        while (speed > 0)
        {
            speed >>= 1;
            focus_speed++;
        }

        currentSpeed = focus_speed;
        FocusSpeedN[0].value = focus_speed;
    }
    else
    {
        DEBUGF(INDI::Logger::DBG_ERROR, "Unknown error: focuser speed value (%s)", resp);
        return false;
    }

  return true;
}

bool MoonLite::isMoving()
{
    int nbytes_written=0, nbytes_read=0, rc=-1;
    char errstr[MAXRBUF];
    char resp[4];

    tcflush(PortFD, TCIOFLUSH);

    if ( (rc = tty_write(PortFD, ":GI#", 4, &nbytes_written)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        DEBUGF(INDI::Logger::DBG_ERROR, "isMoving error: %s.", errstr);
        return false;
    }

    if ( (rc = tty_read(PortFD, resp, 3, MOONLITE_TIMEOUT, &nbytes_read)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        DEBUGF(INDI::Logger::DBG_ERROR, "isMoving error: %s.", errstr);
        return false;
    }

    resp[3]='\0';
    if (!strcmp(resp, "01#"))
        return true;
    else if (!strcmp(resp, "00#"))
        return false;
    else
    {
        DEBUGF(INDI::Logger::DBG_ERROR, "Unknown error: isMoving value (%s)", resp);
        return false;
    }

}

bool MoonLite::setTemperatureCalibration(double calibration)
{
    int nbytes_written=0, rc=-1;
    char errstr[MAXRBUF];
    char cmd[7];

    int cal = calibration * 2;

    snprintf(cmd, 7, ":PO%02hhX#", cal);

    tcflush(PortFD, TCIOFLUSH);

    if ( (rc = tty_write(PortFD, cmd, 6, &nbytes_written)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        DEBUGF(INDI::Logger::DBG_ERROR, "setTemperatureCalibration error: %s.", errstr);
        return false;
    }

    return true;

}

bool MoonLite::setTemperatureCoefficient(double coefficient)
{
    int nbytes_written=0, rc=-1;
    char errstr[MAXRBUF];
    char cmd[7];

    int coeff = coefficient * 2;

    snprintf(cmd, 7, ":SC%02hhX#", coeff);

    tcflush(PortFD, TCIOFLUSH);

    if ( (rc = tty_write(PortFD, cmd, 6, &nbytes_written)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        DEBUGF(INDI::Logger::DBG_ERROR, "setTemperatureCoefficient error: %s.", errstr);
        return false;
    }

    return true;

}


bool MoonLite::Move(unsigned int position)
{
    int nbytes_written=0, rc=-1;
    char errstr[MAXRBUF];
    char cmd[9];

    if (position < FocusAbsPosN[0].min || position > FocusAbsPosN[0].max)
    {
        DEBUGF(INDI::Logger::DBG_ERROR, "Requested position value out of bound: %d", position);
        return false;
    }

    /*if (fabs(position - FocusAbsPosN[0].value) > MaxTravelN[0].value)
    {
        DEBUGF(INDI::Logger::DBG_ERROR, "Requested position value of %d exceeds maximum travel limit of %g", position, MaxTravelN[0].value);
        return false;
    }*/

    snprintf(cmd, 9, ":SN%04X#", position);

    // Set Position
    if ( (rc = tty_write(PortFD, cmd, 8, &nbytes_written)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        DEBUGF(INDI::Logger::DBG_ERROR, "setPosition error: %s.", errstr);
        return false;
    }

    // Move to Position
    if ( (rc = tty_write(PortFD, ":FG#", 4, &nbytes_written)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        DEBUGF(INDI::Logger::DBG_ERROR, "Move error: %s.", errstr);
        return false;
    }

    return true;
}

bool MoonLite::setStepMode(FocusStepMode mode)
{
    int nbytes_written=0, rc=-1;
    char errstr[MAXRBUF];
    char cmd[4];

    tcflush(PortFD, TCIOFLUSH);

    if (mode == FOCUS_HALF_STEP)
        strncpy(cmd, ":SH#", 4);
    else
        strncpy(cmd, ":SF#", 4);

    if ( (rc = tty_write(PortFD, cmd, 4, &nbytes_written)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        DEBUGF(INDI::Logger::DBG_ERROR, "setStepMode error: %s.", errstr);
        return false;
    }

    return true;
}

bool MoonLite::setSpeed(unsigned short speed)
{
    int nbytes_written=0, rc=-1;
    char errstr[MAXRBUF];
    char cmd[7];

    int hex_value=1;

    hex_value <<= speed;

    snprintf(cmd, 7, ":SD%02X#", hex_value);

    if ( (rc = tty_write(PortFD, cmd, 6, &nbytes_written)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        DEBUGF(INDI::Logger::DBG_ERROR, "setSpeed error: %s.", errstr);
        return false;
    }

    return true;
}

bool MoonLite::setTemperatureCompensation(bool enable)
{
    int nbytes_written=0, rc=-1;
    char errstr[MAXRBUF];
    char cmd[4];

    tcflush(PortFD, TCIOFLUSH);

    if (enable)
        strncpy(cmd, ":+#", 4);
    else
        strncpy(cmd, ":-#", 4);

    if ( (rc = tty_write(PortFD, cmd, 3, &nbytes_written)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        DEBUGF(INDI::Logger::DBG_ERROR, "setTemperatureCompensation error: %s.", errstr);
        return false;
    }

    return true;

}

bool MoonLite::ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if(strcmp(dev,getDeviceName())==0)
    {
        // Focus Step Mode
        if (!strcmp(StepModeSP.name, name))
        {
            bool rc=false;
            int current_mode = IUFindOnSwitchIndex(&StepModeSP);
            IUUpdateSwitch(&StepModeSP, states, names, n);
            int target_mode = IUFindOnSwitchIndex(&StepModeSP);
            if (current_mode == target_mode);
            {
                StepModeSP.s = IPS_OK;
                IDSetSwitch(&StepModeSP, NULL);
            }

            if (target_mode == 0)
                rc = setStepMode(FOCUS_HALF_STEP);
            else
                rc = setStepMode(FOCUS_FULL_STEP);

            if (rc == false)
            {
                IUResetSwitch(&StepModeSP);
                StepModeS[current_mode].s = ISS_ON;
                StepModeSP.s = IPS_ALERT;
                IDSetSwitch(&StepModeSP, NULL);
                return false;
            }

            StepModeSP.s = IPS_OK;
            IDSetSwitch(&StepModeSP, NULL);
            return true;
        }

        if (!strcmp(TemperatureCompensateSP.name, name))
        {
            int last_index = IUFindOnSwitchIndex(&TemperatureCompensateSP);
            IUUpdateSwitch(&TemperatureCompensateSP, states, names, n);

            bool rc = setTemperatureCompensation((TemperatureCompensateS[0].s == ISS_ON));

            if (rc == false)
            {
                TemperatureCompensateSP.s = IPS_ALERT;
                IUResetSwitch(&TemperatureCompensateSP);
                TemperatureCompensateS[last_index].s = ISS_ON;
                IDSetSwitch(&TemperatureCompensateSP, NULL);
                return false;
            }

            TemperatureCompensateSP.s = IPS_OK;
            IDSetSwitch(&TemperatureCompensateSP, NULL);
            return true;
        }

        if (!strcmp(PresetGotoSP.name, name))
        {
            IUUpdateSwitch(&PresetGotoSP, states, names, n);
            int index = IUFindOnSwitchIndex(&PresetGotoSP);
            int rc = MoveAbs(PresetN[index].value);
            if (rc >= 0)
            {
                PresetGotoSP.s = IPS_OK;
                DEBUGF(INDI::Logger::DBG_SESSION, "Moving to Preset %d with position %g.", index+1, PresetN[index].value);
                IDSetSwitch(&PresetGotoSP, NULL);
                return true;
            }

            PresetGotoSP.s = IPS_ALERT;
            IDSetSwitch(&PresetGotoSP, NULL);
            return false;
        }
    }


    return INDI::Focuser::ISNewSwitch(dev, name, states, names, n);
}

bool MoonLite::ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n)
{
    int nset=0,i=0;

    if(strcmp(dev,getDeviceName())==0)
    {        
        if (!strcmp (name, MaxTravelNP.name))
        {
             IUUpdateNumber(&MaxTravelNP, values, names, n);
             MaxTravelNP.s = IPS_OK;
             IDSetNumber(&MaxTravelNP, NULL);
             return true;
        }

        if (!strcmp(name, TemperatureSettingNP.name))
        {
            IUUpdateNumber(&TemperatureSettingNP, values, names, n);
            if (!setTemperatureCalibration(TemperatureSettingN[0].value) || !setTemperatureCoefficient(TemperatureSettingN[1].value))
            {
                TemperatureSettingNP.s = IPS_ALERT;
                IDSetNumber(&TemperatureSettingNP, NULL);
                return false;
            }

            TemperatureSettingNP.s = IPS_OK;
            IDSetNumber(&TemperatureSettingNP, NULL);
        }

        if (!strcmp(name, PresetNP.name))
        {
            IUUpdateNumber(&PresetNP, values, names, n);
            PresetNP.s = IPS_OK;
            IDSetNumber(&PresetNP, NULL);

            saveConfig();
        }
    }

    return INDI::Focuser::ISNewNumber(dev, name, values, names, n);

}

void MoonLite::GetFocusParams ()
{
    if (updatePosition())
        IDSetNumber(&FocusAbsPosNP, NULL);

    if (updateTemperature())
        IDSetNumber(&TemperatureNP, NULL);

    if (updateSpeed())
        IDSetNumber(&FocusSpeedNP, NULL);

    if (updateStepMode())
        IDSetSwitch(&StepModeSP, NULL);
}

bool MoonLite::SetSpeed(int speed)
{
    bool rc = false;

    rc = setSpeed(speed);

    if (rc == false)
        return false;

    currentSpeed = speed;

    FocusSpeedNP.s = IPS_OK;
    IDSetNumber(&FocusSpeedNP, NULL);

    return true;
}

bool MoonLite::Move(FocusDirection dir, int speed, int duration)
{
    if (speed != currentSpeed)
    {
        bool rc=false;

        rc = setSpeed(speed);
        if (rc == false)
            return false;
    }

    gettimeofday(&focusMoveStart,NULL);
    focusMoveRequest = duration/1000.0;

    if (dir == FOCUS_INWARD)
        Move(0);
    else
        Move(FocusAbsPosN[0].value + MaxTravelN[0].value - 1);

    if (duration <= POLLMS)
    {
        usleep(POLLMS * 1000);
        Abort();
    }

    FocusTimerNP.s = IPS_BUSY;

    return true;
}


int MoonLite::MoveAbs(int targetTicks)
{
    targetPos = targetTicks;

    bool rc = false;

    rc = Move(targetPos);

    if (rc == false)
        return -1;

    FocusAbsPosNP.s = IPS_BUSY;

    return 1;
}

int MoonLite::MoveRel(FocusDirection dir, unsigned int ticks)
{
    double newPosition=0;
    bool rc=false;

    if (dir == FOCUS_INWARD)
        newPosition = FocusAbsPosN[0].value - ticks;
    else
        newPosition = FocusAbsPosN[0].value + ticks;

    rc = Move(newPosition);

    if (rc == false)
        return -1;

    FocusRelPosN[0].value = ticks;
    FocusRelPosNP.s = IPS_BUSY;

    return 1;
}

bool MoonLite::saveConfigItems(FILE *fp)
{
    IUSaveConfigText(fp, &PortTP);
    IUSaveConfigNumber(fp, &PresetNP);
}

void MoonLite::TimerHit()
{
    if (isConnected() == false)
    {
        SetTimer(POLLMS);
        return;
    }

    bool rc = updatePosition();
    if (rc)
    {
        if (fabs(lastPos - FocusAbsPosN[0].value) > 5)
        {
            IDSetNumber(&FocusAbsPosNP, NULL);
            lastPos = FocusAbsPosN[0].value;
        }
    }

    rc = updateTemperature();
    if (rc)
    {
        if (fabs(lastTemperature - TemperatureN[0].value) >= 0.5)
        {
            IDSetNumber(&TemperatureNP, NULL);
            lastTemperature = TemperatureN[0].value;
        }
    }

    if (FocusTimerNP.s == IPS_BUSY)
    {
        float remaining = CalcTimeLeft(focusMoveStart, focusMoveRequest);

        if (remaining <= 0)
        {
            FocusTimerNP.s = IPS_OK;
            FocusTimerN[0].value = 0;
            Abort();
        }
        else
            FocusTimerN[0].value = remaining*1000.0;

        IDSetNumber(&FocusTimerNP, NULL);

    }

    if (FocusAbsPosNP.s == IPS_BUSY || FocusRelPosNP.s == IPS_BUSY)
    {
        if (isMoving() == false)
        {
            FocusAbsPosNP.s = IPS_OK;
            FocusRelPosNP.s = IPS_OK;
            IDSetNumber(&FocusAbsPosNP, NULL);
            IDSetNumber(&FocusRelPosNP, NULL);
            DEBUG(INDI::Logger::DBG_SESSION, "Focuser reached requested position.");
        }
    }



    SetTimer(POLLMS);

}

bool MoonLite::Abort()
{
    int nbytes_written;
    if (tty_write(PortFD, ":FQ#", 4, &nbytes_written) == TTY_OK)
    {
        FocusAbsPosNP.s = IPS_IDLE;
        FocusRelPosNP.s = IPS_IDLE;
        IDSetNumber(&FocusAbsPosNP, NULL);
        IDSetNumber(&FocusRelPosNP, NULL);
        return true;
    }
    else
        return false;
}

float MoonLite::CalcTimeLeft(timeval start,float req)
{
    double timesince;
    double timeleft;
    struct timeval now;
    gettimeofday(&now,NULL);

    timesince=(double)(now.tv_sec * 1000.0 + now.tv_usec/1000) - (double)(start.tv_sec * 1000.0 + start.tv_usec/1000);
    timesince=timesince/1000;
    timeleft=req-timesince;
    return timeleft;
}




