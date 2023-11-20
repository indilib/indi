/*
   IOPTRON iEAF Focuser 
   
    Copyright (C) 2018 Paul de Backer (74.0632@gmail.com)

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

#include "ieaffocus.h"
#include "indicom.h"

#include <stdio.h>
#include <termios.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>
#include <math.h>
#include <memory>

#define iEAFFOCUS_TIMEOUT 4

#define POLLMS_OVERRIDE  1500

std::unique_ptr<iEAFFocus> ieafFocus(new iEAFFocus());

iEAFFocus::iEAFFocus()
{
    FI::SetCapability(FOCUSER_CAN_ABS_MOVE | FOCUSER_CAN_REL_MOVE | FOCUSER_CAN_ABORT|FOCUSER_CAN_REVERSE);
    lastPos = 0;
}

iEAFFocus::~iEAFFocus()
{
}

bool iEAFFocus::initProperties()
{
    INDI::Focuser::initProperties();
    // SetZero
    IUFillSwitch(&SetZeroS[0], "SETZERO", "Set Current Position to 0", ISS_OFF);
    IUFillSwitchVector(&SetZeroSP, SetZeroS, 1, getDeviceName(), "Zero Position", "", OPTIONS_TAB, IP_RW, ISR_1OFMANY, 0,
                       IPS_IDLE);
    // Maximum Travel
    IUFillNumber(&MaxPosN[0], "MAXPOS", "Maximum Out Position", "%5.0f", 1., 99999., 0, 0);
    IUFillNumberVector(&MaxPosNP, MaxPosN, 1, getDeviceName(), "FOCUS_MAXPOS", "Position", OPTIONS_TAB, IP_RW, 0, IPS_IDLE );

    /* Focuser temperature */
    IUFillNumber(&TemperatureN[0], "TEMPERATURE", "Celsius", "%2.2f", 0, 50., 0., 50.);
    IUFillNumberVector(&TemperatureNP, TemperatureN, 1, getDeviceName(), "FOCUS_TEMPERATURE", "Temperature",
                       MAIN_CONTROL_TAB, IP_RO, 0, IPS_IDLE);

    /* Relative and absolute movement */
    FocusRelPosN[0].min = 0.;
    FocusRelPosN[0].max = 5000.;
    FocusRelPosN[0].value = 0.;
    FocusRelPosN[0].step = 10.;

    FocusAbsPosN[0].min = 0.;
    FocusAbsPosN[0].max = 99999.;
    FocusAbsPosN[0].value = 0.;
    FocusAbsPosN[0].step = 10.;

    addDebugControl();

    return true;

}

bool iEAFFocus::updateProperties()
{
    INDI::Focuser::updateProperties();
    if (isConnected())
    {
	defineProperty(&TemperatureNP);
        defineProperty(&MaxPosNP);
        defineProperty(&SetZeroSP);
        GetFocusParams();
        loadConfig(true);

        DEBUG(INDI::Logger::DBG_SESSION, "iEAF Focus parameters updated, focuser ready for use.");
    }
    else
    {
   	deleteProperty(TemperatureNP.name);
	deleteProperty(MaxPosNP.name);
        deleteProperty(SetZeroSP.name);
    }

    return true;

}

bool iEAFFocus::Handshake()
{
    if (Ack())
    {
        DEBUG(INDI::Logger::DBG_SESSION, "iEAFFocus is online. Getting focus parameters...");
        return true;
    }
    return false;
}

const char * iEAFFocus::getDefaultName()
{
    return "iEAFFocus";
}

bool iEAFFocus::Ack()
{
    int nbytes_written = 0, nbytes_read = 0, rc = -1;
    char errstr[MAXRBUF];
    char resp[16];
    int ieafpos,ieafmodel,ieaflast;
    sleep(2);
    tcflush(PortFD, TCIOFLUSH);
    if ( (rc = tty_write(PortFD, ":DeviceInfo#",12, &nbytes_written)) != TTY_OK)
    {
	errstr[MAXRBUF] = {0};    
        tty_error_msg(rc, errstr, MAXRBUF);
        DEBUGF(INDI::Logger::DBG_ERROR, "Init send getdeviceinfo  error: %s.", errstr);
        return false;
    }

    if ( (rc = tty_read_section(PortFD, resp, '#', iEAFFOCUS_TIMEOUT * 2, &nbytes_read)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        DEBUGF(INDI::Logger::DBG_ERROR, "Init read deviceinfo error: %s.", errstr);
        return false;
    }
    tcflush(PortFD, TCIOFLUSH);
    resp[nbytes_read] = '\0';
    sscanf(resp,"%6d%2d%4d",&ieafpos,&ieafmodel,&ieaflast);
    if (ieafmodel==2)
    {
        return true;
    }
    else
    {
        DEBUGF(INDI::Logger::DBG_ERROR, "Ack Response: %s", resp);
        return false;
    }


    return true;
}

bool iEAFFocus::readReverseDirection()
{

    int nbytes_written = 0, nbytes_read = 0, rc = -1;
    char errstr[MAXRBUF];
    char resp[16];
    char joedeviceinfo[12];

    int ieafpos,ieafmove,ieaftemp,ieafdir;
    float current_ieaf_temp;
    sleep(2);

    tcflush(PortFD, TCIOFLUSH);
    if ( (rc = tty_write(PortFD, ":FI#", 4, &nbytes_written)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        DEBUGF(INDI::Logger::DBG_ERROR, "readReverseDirection error: %s.", errstr);
    }
    if ( (rc = tty_read_section(PortFD, resp, '#', iEAFFOCUS_TIMEOUT, &nbytes_read)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        DEBUGF(INDI::Logger::DBG_ERROR, "readReverseDirection  error: %s.", errstr);
	return false;
    }

    tcflush(PortFD, TCIOFLUSH);

    resp[nbytes_read] = '\0';


    sscanf(resp,"%14s",joedeviceinfo);
    rc=sscanf(resp,"%7d%1d%5d%1d",&ieafpos,&ieafmove,&ieaftemp,&ieafdir);
    FocusReverseS[INDI_DISABLED].s = ISS_ON;
        if(ieafdir == 0)
        {
            FocusReverseS[INDI_DISABLED].s = ISS_ON;
        }
        else if (ieafdir == 1)
        {
            FocusReverseS[INDI_ENABLED].s = ISS_ON;
        }
        else
        {
            DEBUGF(INDI::Logger::DBG_ERROR,"Invalid Response: focuser Reverse direction value (%s)", resp);
	    return false;
        }


    return true;
}


bool iEAFFocus::updateTemperature()
{
    int nbytes_written = 0, nbytes_read = 0, rc = -1;
    char errstr[MAXRBUF];
    char resp[16];
    char joedeviceinfo[12];

    int ieafpos,ieafmove,ieaftemp,ieafdir;
    float current_ieaf_temp;
    sleep(2);

    tcflush(PortFD, TCIOFLUSH);
    if ( (rc = tty_write(PortFD, ":FI#", 4, &nbytes_written)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        DEBUGF(INDI::Logger::DBG_ERROR, "updatePosition error: %s.", errstr);
        return false;
    
    }
    if ( (rc = tty_read_section(PortFD, resp, '#', iEAFFOCUS_TIMEOUT, &nbytes_read)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        DEBUGF(INDI::Logger::DBG_ERROR, "updatePosition error: %s.", errstr);
        return false;
    }

    tcflush(PortFD, TCIOFLUSH);

    resp[nbytes_read] = '\0';


    sscanf(resp,"%14s",joedeviceinfo);
    rc=sscanf(resp,"%7d%1d%5d%1d",&ieafpos,&ieafmove,&ieaftemp,&ieafdir);
    current_ieaf_temp=ieaftemp/100.0-273.15;
    if (rc > 0)
    {
        TemperatureN[0].value = current_ieaf_temp;
        IDSetNumber(&TemperatureNP, NULL);

    }
    else
    {
        DEBUGF(INDI::Logger::DBG_ERROR, "Unknown error: focuser position value (%s)", resp);
        return false;
    }

    return true;



}	

bool iEAFFocus::updatePosition()
{
    int nbytes_written = 0, nbytes_read = 0, rc = -1;
    char errstr[MAXRBUF];
    char resp[16];
    char joedeviceinfo[12];

    int ieafpos,ieafmove,ieaftemp,ieafdir;
    float current_ieaf_temp;
    sleep(2);

    tcflush(PortFD, TCIOFLUSH);
    if ( (rc = tty_write(PortFD, ":FI#", 4, &nbytes_written)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        DEBUGF(INDI::Logger::DBG_ERROR, "updatePosition error: %s.", errstr);
        return false;
    }


    if ( (rc = tty_read_section(PortFD, resp, '#', iEAFFOCUS_TIMEOUT, &nbytes_read)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        DEBUGF(INDI::Logger::DBG_ERROR, "updatePosition error: %s.", errstr);
        return false;
    }

    tcflush(PortFD, TCIOFLUSH);

    resp[nbytes_read] = '\0';


    sscanf(resp,"%14s",joedeviceinfo);
    rc=sscanf(resp,"%7d%1d%5d%1d",&ieafpos,&ieafmove,&ieaftemp,&ieafdir);
    current_ieaf_temp=ieaftemp/100.0-273.15;

    if (rc > 0)
    {
        FocusAbsPosN[0].value = ieafpos;
        IDSetNumber(&FocusAbsPosNP, NULL);
	TemperatureN[0].value = current_ieaf_temp;
        IDSetNumber(&TemperatureNP, NULL);

    }
    else
    {
        DEBUGF(INDI::Logger::DBG_ERROR, "Unknown error: focuser position value (%s)", resp);
        return false;
    }

    return true;


}

bool iEAFFocus::updateMaxPos()
{

	MaxPosN[0].value = 99999;
        FocusAbsPosN[0].max = 99999;
        IDSetNumber(&FocusAbsPosNP, NULL);
        IDSetNumber(&MaxPosNP, NULL);

	return true;
}



bool iEAFFocus::isMoving()
{
    int nbytes_written = 0, nbytes_read = 0, rc = -1;
    char errstr[MAXRBUF];
    char resp[16];
    int ieafpos,ieafmove,ieaftemp,ieafdir;

    tcflush(PortFD, TCIOFLUSH);

    if ( (rc = tty_write(PortFD, ":FI#", 4, &nbytes_written)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        DEBUGF(INDI::Logger::DBG_ERROR, "isMoving error: %s.", errstr);
        return false;
    }

    if ( (rc = tty_read_section(PortFD, resp, '#', iEAFFOCUS_TIMEOUT, &nbytes_read)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        DEBUGF(INDI::Logger::DBG_ERROR, "isMoving error: %s.", errstr);
        return false;
    }

    tcflush(PortFD, TCIOFLUSH);

    resp[nbytes_read] = '\0';
    
    rc=sscanf(resp,"%7d%d%5d%d",&ieafpos,&ieafmove,&ieaftemp,&ieafdir);

    if (rc > 0)
    {
         if (ieafmove==1) 
	 {
		 return true;
         }
	 else
	 {
	       	return false;
	 }
    }
    else
    {
        DEBUGF(INDI::Logger::DBG_ERROR, "Unknown error: focuser moving value (%s)", resp);
        return false;
    }



}

bool iEAFFocus::MoveMyFocuser(uint32_t position)
{
    int nbytes_written = 0, rc = -1;
    char errstr[MAXRBUF];
    char cmd[12];

    snprintf(cmd, 12, ":FM%7d#", position);

    // Set Position
    if ( (rc = tty_write(PortFD, cmd, strlen(cmd), &nbytes_written)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        DEBUGF(INDI::Logger::DBG_ERROR, "setPosition error: %s.", errstr);
        return false;
    }
    return true;
}


bool iEAFFocus::ReverseFocuser(bool enabled)
{
    int nbytes_written = 0, rc = -1;
    char errstr[MAXRBUF];
    char cmd[12];


    // Change Direction
    if ( (rc = tty_write(PortFD, ":FR#", 4, &nbytes_written)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        DEBUGF(INDI::Logger::DBG_ERROR, "change Direction error: %s.", errstr);
        return false;
    }

/*
	if (enabled)
	{
	 FocusReverseS[INDI_ENABLED].s = ISS_ON;
	 FocusReverseS[INDI_DISABLED].s = ISS_OFF;
	}
	else
	{
         FocusReverseS[INDI_ENABLED].s = ISS_OFF;
         FocusReverseS[INDI_DISABLED].s = ISS_ON;

	}
*/

/*
    if     (FocusReverseS[INDI_DISABLED].s == ISS_ON)
	{
		FocusReverseS[INDI_ENABLED].s = ISS_ON;
	}
    else 
	{
	FocusReverseS[INDI_DISABLED].s = ISS_ON;
	}
*/

    return true;
}



void iEAFFocus::setZero()
{
    int nbytes_written = 0, rc = -1;
    char errstr[MAXRBUF];
    // Set Zero
    if ( (rc = tty_write(PortFD, ":FZ#", 4, &nbytes_written)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        DEBUGF(INDI::Logger::DBG_ERROR, "set Zero error: %s.", errstr);
        return;
    }
    updateMaxPos();
    return;
}

bool iEAFFocus::setMaxPos(uint32_t maxPos)
{
    uint32_t maxp=maxPos;
    maxp=maxp+0;
    updateMaxPos();
    return true;
}

bool iEAFFocus::ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if(strcmp(dev, getDeviceName()) == 0)
    {
        if (!strcmp(SetZeroSP.name, name))
        {
            setZero();
            IUResetSwitch(&SetZeroSP);
            SetZeroSP.s = IPS_OK;
            IDSetSwitch(&SetZeroSP, NULL);
            return true;
        }

        return INDI::Focuser::ISNewSwitch(dev, name, states, names, n);
    }
    return false;
}

bool iEAFFocus::ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n)
{
    if(strcmp(dev, getDeviceName()) == 0)
    {
        if (!strcmp (name, MaxPosNP.name))
        {
            if (values[0] < FocusAbsPosN[0].value)
            {
                DEBUGF(INDI::Logger::DBG_ERROR, "Can't set max position lower than current absolute position ( %6.0f )",
                       FocusAbsPosN[0].value);
                return false;
            }
            IUUpdateNumber(&MaxPosNP, values, names, n);
            FocusAbsPosN[0].max = MaxPosN[0].value;
            setMaxPos(MaxPosN[0].value);
            MaxPosNP.s = IPS_OK;
            return true;
        }
    }

    return INDI::Focuser::ISNewNumber(dev, name, values, names, n);

}

void iEAFFocus::GetFocusParams ()
{
//    updatePosition();
    updateTemperature();
    updateMaxPos();
    readReverseDirection();
    updatePosition();

}


IPState iEAFFocus::MoveAbsFocuser(uint32_t targetTicks)
{
    uint32_t targetPos = targetTicks;

    bool rc = false;

    rc = MoveMyFocuser(targetPos);

    if (rc == false)
        return IPS_ALERT;

    FocusAbsPosNP.s = IPS_BUSY;

    return IPS_BUSY;
}

IPState iEAFFocus::MoveRelFocuser(FocusDirection dir, uint32_t ticks)
{
    uint32_t newPosition = 0;
    bool rc = false;

    if (dir == FOCUS_INWARD)
        newPosition = uint32_t(FocusAbsPosN[0].value) - ticks;
    else
        newPosition = uint32_t(FocusAbsPosN[0].value) + ticks;

    rc = MoveMyFocuser(newPosition);

    if (rc == false)
        return IPS_ALERT;

    FocusRelPosN[0].value = ticks;
    FocusRelPosNP.s = IPS_BUSY;

    return IPS_BUSY;
}

void iEAFFocus::TimerHit()
{
    if (isConnected() == false)
    {
        SetTimer(POLLMS_OVERRIDE);
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


    if (FocusAbsPosNP.s == IPS_BUSY || FocusRelPosNP.s == IPS_BUSY)
    {
        if (isMoving() == false)
        {
            FocusAbsPosNP.s = IPS_OK;
            FocusRelPosNP.s = IPS_OK;
            IDSetNumber(&FocusAbsPosNP, NULL);
            IDSetNumber(&FocusRelPosNP, NULL);
            lastPos = FocusAbsPosN[0].value;
            DEBUG(INDI::Logger::DBG_SESSION, "Focuser reached requested position.");
        }
    }
    SetTimer(POLLMS_OVERRIDE);

}

bool iEAFFocus::AbortFocuser()
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


bool iEAFFocus::saveConfigItems(FILE * fp)
{
    Focuser::saveConfigItems(fp);
/*
    IUSaveConfigNumber(fp, &TemperatureSettingNP);
    IUSaveConfigSwitch(fp, &TemperatureCompensateSP);
    IUSaveConfigSwitch(fp, &BacklashInSP);
    IUSaveConfigNumber(fp, &BacklashInStepsNP);
    IUSaveConfigSwitch(fp, &BacklashOutSP);
    IUSaveConfigNumber(fp, &BacklashOutStepsNP);
    IUSaveConfigSwitch(fp, &StepModeSP);
    IUSaveConfigSwitch(fp, &DisplaySP);
*/
    //IUSaveConfigSwitch(fp, &DisplaySP);
    return true;
}
