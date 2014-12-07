/*******************************************************************************
 Baader Planetarium Dome INDI Driver

 Copyright(c) 2014 Jasem Mutlaq. All rights reserved.

 Baader Dome INDI Driver

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Library General Public
 License version 2 as published by the Free Software Foundation.
 .
 This library is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 Library General Public License for more details.
 .
 You should have received a copy of the GNU Library General Public License
 along with this library; see the file COPYING.LIB.  If not, write to
 the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 Boston, MA 02110-1301, USA.
*******************************************************************************/
#include "indicom.h"
#include "baader_dome.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <math.h>
#include <string.h>
#include <termios.h>

#include <memory>


// We declare an auto pointer to BaaderDome.
std::auto_ptr<BaaderDome> baaderDome(0);

#define POLLMS              1000            /* Update frequency 1000 ms */
#define DOME_AZ_THRESHOLD   1               /* Error threshold in degrees*/
#define DOME_CMD            9               /* Dome command in bytes */
#define DOME_BUF            10              /* Dome command in bytes plus null terminating character */
#define DOME_TIMEOUT        3               /* 3 seconds comm timeout */

#define SIM_SHUTTER_TIMER   5.0             /* Simulated Shutter closes/open in 5 seconds */
#define SIM_DOME_SPEED      2.0             /* Simulated dome speed 2 degrees per second, constant */

void ISPoll(void *p);

void ISInit()
{
   static int isInit =0;

   if (isInit == 1)
       return;

    isInit = 1;
    if(baaderDome.get() == 0) baaderDome.reset(new BaaderDome());

}

void ISGetProperties(const char *dev)
{
        ISInit();
        baaderDome->ISGetProperties(dev);
}

void ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int num)
{
        ISInit();
        baaderDome->ISNewSwitch(dev, name, states, names, num);
}

void ISNewText(	const char *dev, const char *name, char *texts[], char *names[], int num)
{
        ISInit();
        baaderDome->ISNewText(dev, name, texts, names, num);
}

void ISNewNumber(const char *dev, const char *name, double values[], char *names[], int num)
{
        ISInit();
        baaderDome->ISNewNumber(dev, name, values, names, num);
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
    baaderDome->ISSnoopDevice(root);
}

BaaderDome::BaaderDome()
{

   targetAz = 0;
   targetShutter= shutterStatus= simShutterStatus = SHUTTER_CLOSED;
   prev_az=0;
   prev_alt=0;

   DomeCapability cap;

   cap.canAbort = false;
   cap.canAbsMove = true;
   cap.canRelMove = true;
   cap.hasShutter = true;
   cap.variableSpeed = false;

   SetDomeCapability(&cap);

}

/************************************************************************************
 *
* ***********************************************************************************/
BaaderDome::~BaaderDome() {}

/************************************************************************************
 *
* ***********************************************************************************/
bool BaaderDome::initProperties()
{
    INDI::Dome::initProperties();

    addAuxControls();

    return true;
}

/************************************************************************************
 *
* ***********************************************************************************/
bool BaaderDome::SetupParms()
{
    targetAz = 0;

    if (UpdatePosition())
        IDSetNumber(&DomeAbsPosNP, NULL);

    if (UpdateShutterStatus())
        IDSetSwitch(&DomeShutterSP, NULL);

    return true;
}

/************************************************************************************
 *
* ***********************************************************************************/
bool BaaderDome::Connect()
{
    int connectrc=0;
    char errorMsg[MAXRBUF];

    sim = isSimulation();

    if (!sim && (connectrc = tty_connect(PortT[0].text, 9600, 8, 0, 1, &PortFD)) != TTY_OK)
    {
        tty_error_msg(connectrc, errorMsg, MAXRBUF);

        DEBUGF(INDI::Logger::DBG_SESSION, "Failed to connect to port %s. Error: %s", PortT[0].text, errorMsg);

        return false;

    }

    if (Ack())
    {
        DEBUG(INDI::Logger::DBG_SESSION, "Dome is online. Getting dome parameters...");
        SetTimer(POLLMS);
        return true;
    }

    DEBUG(INDI::Logger::DBG_SESSION, "Error retreiving data from dome, please ensure dome controller is powered and the port is correct.");
    return false;
}

/************************************************************************************
 *
* ***********************************************************************************/
bool BaaderDome::Disconnect()
{
    if (!sim)
        tty_disconnect(PortFD);
    DEBUG(INDI::Logger::DBG_SESSION, "Dome is offline.");
    return true;
}

/************************************************************************************
 *
* ***********************************************************************************/
const char * BaaderDome::getDefaultName()
{
        return (char *)"Baader Dome";
}

/************************************************************************************
 *
* ***********************************************************************************/
bool BaaderDome::updateProperties()
{
    INDI::Dome::updateProperties();

    if (isConnected())
    {
        SetupParms();
    }

    return true;
}

/************************************************************************************
 *
* ***********************************************************************************/
bool BaaderDome::Ack()
{
    int nbytes_written=0, nbytes_read=0, rc=-1;
    char errstr[MAXRBUF];
    char resp[DOME_BUF];
    char status[DOME_BUF];

    tcflush(PortFD, TCIOFLUSH);

    if (!sim && (rc = tty_write(PortFD, "d#getshut", DOME_CMD, &nbytes_written)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        DEBUGF(INDI::Logger::DBG_ERROR, "d#getshut Ack error: %s.", errstr);
        return false;
    }

    DEBUG(INDI::Logger::DBG_DEBUG, "CMD (d#getshut)");

    if (sim)
    {
        strncpy(resp, "d#shutclo", DOME_BUF);
        nbytes_read=DOME_CMD;
    }
    else if ( (rc = tty_read(PortFD, resp, DOME_CMD, DOME_TIMEOUT, &nbytes_read)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        DEBUGF(INDI::Logger::DBG_ERROR, "Ack error: %s.", errstr);
        return false;
    }

    resp[nbytes_read] = '\0';

    DEBUGF(INDI::Logger::DBG_DEBUG, "RES (%s)", resp);

    rc = sscanf(resp, "d#%s", status);

    if (rc > 0)
        return true;
    else
        return false;

}

/************************************************************************************
 *
* ***********************************************************************************/
bool BaaderDome::UpdateShutterStatus()
{
    int nbytes_written=0, nbytes_read=0, rc=-1;
    char errstr[MAXRBUF];
    char resp[DOME_BUF];
    char status[DOME_BUF];

    tcflush(PortFD, TCIOFLUSH);

    if (!sim && (rc = tty_write(PortFD, "d#getshut", DOME_CMD, &nbytes_written)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        DEBUGF(INDI::Logger::DBG_ERROR, "d#getshut UpdateShutterStatus error: %s.", errstr);
        return false;
    }

    DEBUG(INDI::Logger::DBG_DEBUG, "CMD (d#getshut)");

    if (sim)
    {

        if (simShutterStatus == SHUTTER_CLOSED)
            strncpy(resp, "d#shutclo", DOME_BUF);
        else if (simShutterStatus == SHUTTER_OPENED)
            strncpy(resp, "d#shutope", DOME_BUF);
        else if (simShutterStatus == SHUTTER_MOVING)
            strncpy(resp, "d#shutrun", DOME_BUF);
        nbytes_read=DOME_CMD;
    }
    else if ( (rc = tty_read(PortFD, resp, DOME_CMD, DOME_TIMEOUT, &nbytes_read)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        DEBUGF(INDI::Logger::DBG_ERROR, "UpdateShutterStatus error: %s.", errstr);
        return false;
    }

    resp[nbytes_read] = '\0';

    DEBUGF(INDI::Logger::DBG_DEBUG, "RES (%s)", resp);

    rc = sscanf(resp, "d#shut%s", status);

    if (rc > 0)
    {
        DomeShutterSP.s = IPS_IDLE;
        IUResetSwitch(&DomeShutterSP);

        if (!strcmp(status, "ope"))
        {
            if (shutterStatus == SHUTTER_MOVING && targetShutter == SHUTTER_OPENED)
                DEBUGF(INDI::Logger::DBG_SESSION, "%s", GetShutterStatusString(SHUTTER_OPENED));

            shutterStatus = SHUTTER_OPENED;
            DomeShutterS[SHUTTER_OPEN].s = ISS_ON;
        }
        else if (!strcmp(status, "clo"))
        {
            if (shutterStatus == SHUTTER_MOVING && targetShutter == SHUTTER_CLOSED)
                DEBUGF(INDI::Logger::DBG_SESSION, "%s", GetShutterStatusString(SHUTTER_CLOSED));

            shutterStatus = SHUTTER_CLOSED;
            DomeShutterS[SHUTTER_CLOSE].s = ISS_ON;
        }
        else if (!strcmp(status, "run"))
        {
            shutterStatus = SHUTTER_MOVING;
            DomeShutterSP.s = IPS_BUSY;
        }
        else
        {
            shutterStatus = SHUTTER_UNKNOWN;
            DomeShutterSP.s = IPS_ALERT;
            DEBUGF(INDI::Logger::DBG_ERROR, "Unknown shutter status: %s.", resp);
        }

        return true;
    }
    else
        return false;

}

/************************************************************************************
 *
* ***********************************************************************************/
bool BaaderDome::UpdatePosition()
{
    int nbytes_written=0, nbytes_read=0, rc=-1;
    char errstr[MAXRBUF];
    char resp[DOME_BUF];
    unsigned short domeAz=0;

    tcflush(PortFD, TCIOFLUSH);

    if (!sim && (rc = tty_write(PortFD, "d#getazim", DOME_CMD, &nbytes_written)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        DEBUGF(INDI::Logger::DBG_ERROR, "d#getazim UpdatePosition error: %s.", errstr);
        return false;
    }

    DEBUG(INDI::Logger::DBG_DEBUG, "CMD (d#getazim)");

    if (sim)
    {

        snprintf(resp, DOME_BUF, "d#azr%04d", MountAzToDomeAz(DomeAbsPosN[0].value));
        nbytes_read=DOME_CMD;
    }
    else if ( (rc = tty_read(PortFD, resp, DOME_CMD, DOME_TIMEOUT, &nbytes_read)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        DEBUGF(INDI::Logger::DBG_ERROR, "UpdatePosition error: %s.", errstr);
        return false;
    }

    resp[nbytes_read] = '\0';

    DEBUGF(INDI::Logger::DBG_DEBUG, "RES (%s)", resp);

    rc = sscanf(resp, "d#azr%hu", &domeAz);

    if (rc > 0)
    {
        DomeAbsPosN[0].value = DomeAzToMountAz(domeAz);
        return true;
    }
    else
        return false;
}

/************************************************************************************
 *
* ***********************************************************************************/
unsigned short BaaderDome::MountAzToDomeAz(double mountAz)
{
    int domeAz=0;

    domeAz = (mountAz) * 10.0 - 1800;

    if (mountAz >=0 && mountAz <= 179.9)
        domeAz += 3600;

    if (domeAz > 3599)
        domeAz = 3599;
    else if (domeAz < 0)
        domeAz = 0;

    return ((unsigned short) (domeAz));
}

/************************************************************************************
 *
* ***********************************************************************************/
double BaaderDome::DomeAzToMountAz(unsigned short domeAz)
{
    double mountAz=0;

    mountAz = ((double) (domeAz + 1800)) / 10.0;

    if (domeAz >= 1800)
        mountAz -= 360;

    if (mountAz > 360)
        mountAz -= 360;
    else if (mountAz < 0)
        mountAz += 360;

    return mountAz;
}

/************************************************************************************
 *
* ***********************************************************************************/
void BaaderDome::TimerHit()
{

    if(isConnected() == false)
        return;  //  No need to reset timer if we are not connected anymore

    // Get Mount Az angle
    ln_get_hrz_from_equ(&equ, &observer, ln_get_julian_from_sys(), &hrz);

    hrz.az += 180;
    if (hrz.az > 360)
        hrz.az -= 360;
    if (hrz.az < 0)
        hrz.az += 360;

    // Control debug flooding
    if (fabs(hrz.az - prev_az) > 0.2 || fabs(hrz.alt - prev_alt) > 0.2)
    {
        prev_az  = hrz.az;
        prev_alt = hrz.alt;
        DEBUGF(INDI::Logger::DBG_DEBUG, "Updated telescope Az: %g - Alt: %g", prev_az, prev_alt);
    }

    UpdatePosition();

    if (DomeAbsPosNP.s == IPS_BUSY)
    {
        if (sim)
        {
            if (targetAz > DomeAbsPosN[0].value)
            {
                DomeAbsPosN[0].value += SIM_DOME_SPEED;
            }
            else if (targetAz < DomeAbsPosN[0].value)
            {
                DomeAbsPosN[0].value -= SIM_DOME_SPEED;
            }

            if (DomeAbsPosN[0].value < DomeAbsPosN[0].min)
                DomeAbsPosN[0].value += DomeAbsPosN[0].max;
            if (DomeAbsPosN[0].value > DomeAbsPosN[0].max)
                DomeAbsPosN[0].value -= DomeAbsPosN[0].max;
        }

        if (fabs(targetAz - DomeAbsPosN[0].value) <= DOME_AZ_THRESHOLD)
        {
            DomeAbsPosN[0].value = targetAz;
            DomeAbsPosNP.s = IPS_OK;
            DEBUG(INDI::Logger::DBG_SESSION, "Dome reached requested azimuth angle.");
            if (DomeGotoSP.s == IPS_BUSY)
            {
                DomeGotoSP.s = IPS_OK;
                IDSetSwitch(&DomeGotoSP, NULL);
            }
            if (GetDomeCapability().canRelMove && DomeRelPosNP.s == IPS_BUSY)
            {
                DomeRelPosNP.s = IPS_OK;
                IDSetNumber(&DomeRelPosNP, NULL);
            }
        }

        IDSetNumber(&DomeAbsPosNP, NULL);
    }
    else
        IDSetNumber(&DomeAbsPosNP, NULL);

    UpdateShutterStatus();

    if (sim && DomeShutterSP.s == IPS_BUSY)
    {
            if (simShutterTimer-- <= 0)
            {
                simShutterTimer=0;
                simShutterStatus = targetShutter;
            }
    }
    else
        IDSetSwitch(&DomeShutterSP, NULL);

    SetTimer(POLLMS);
    return;

}

/************************************************************************************
 *
* ***********************************************************************************/
int BaaderDome::MoveAbsDome(double az)
{
    targetAz = az;
    int nbytes_written=0, nbytes_read=0, rc=-1;
    char errstr[MAXRBUF];
    char cmd[DOME_BUF];
    char resp[DOME_BUF];

    snprintf(cmd, DOME_BUF, "d#azi04%d", MountAzToDomeAz(targetAz));

    tcflush(PortFD, TCIOFLUSH);

    if (!sim && (rc = tty_write(PortFD, cmd, DOME_CMD, &nbytes_written)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        DEBUGF(INDI::Logger::DBG_ERROR, "%s MoveAbsDome error: %s.", cmd, errstr);
        return false;
    }

    DEBUGF(INDI::Logger::DBG_DEBUG, "CMD (%s)", cmd);

    if (sim)
    {
        strncpy(resp, "d#gotmess", DOME_BUF);
        nbytes_read=DOME_CMD;
    }
    else if ( (rc = tty_read(PortFD, resp, DOME_CMD, DOME_TIMEOUT, &nbytes_read)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        DEBUGF(INDI::Logger::DBG_ERROR, "MoveAbsDome error: %s.", errstr);
        return false;
    }

    resp[nbytes_read] = '\0';

    DEBUGF(INDI::Logger::DBG_DEBUG, "RES (%s)", resp);

    if (!strcmp(resp, "d#gotmess"))
        return 1;
    else
        return -1;

}

/************************************************************************************
 *
* ***********************************************************************************/
int BaaderDome::MoveRelDome(DomeDirection dir, double azDiff)
{
    targetAz = DomeAbsPosN[0].value + (azDiff * (dir==DOME_CW ? 1 : -1));

    if (targetAz < DomeAbsPosN[0].min)
        targetAz += DomeAbsPosN[0].max;
    if (targetAz > DomeAbsPosN[0].max)
        targetAz -= DomeAbsPosN[0].max;

    // It will take a few cycles to reach final position
    return MoveAbsDome(targetAz);

}

/************************************************************************************
 *
* ***********************************************************************************/
int BaaderDome::ParkDome()
{
    targetAz = DomeParamN[DOME_PARK].value;

    return MoveAbsDome(targetAz);
}

/************************************************************************************
 *
* ***********************************************************************************/
int BaaderDome::HomeDome()
{
    targetAz = DomeParamN[DOME_HOME].value;

    return MoveAbsDome(targetAz);
}

/************************************************************************************
 *
* ***********************************************************************************/
int BaaderDome::ControlDomeShutter(ShutterOperation operation)
{
    int nbytes_written=0, nbytes_read=0, rc=-1;
    char errstr[MAXRBUF];
    char cmd[DOME_BUF];
    char resp[DOME_BUF];

    if (operation == SHUTTER_OPEN)
    {
        targetShutter = SHUTTER_OPENED;
        strncpy(cmd, "d#opeshut", DOME_BUF);
    }
    else
    {
        targetShutter = SHUTTER_CLOSED;
        strncpy(cmd, "d#closhut", DOME_BUF);
    }

    tcflush(PortFD, TCIOFLUSH);

    if (!sim && (rc = tty_write(PortFD, cmd, DOME_CMD, &nbytes_written)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        DEBUGF(INDI::Logger::DBG_ERROR, "%s ControlDomeShutter error: %s.", cmd, errstr);
        return false;
    }

    DEBUGF(INDI::Logger::DBG_DEBUG, "CMD (%s)", cmd);

    if (sim)
    {
        simShutterTimer = SIM_SHUTTER_TIMER;
        strncpy(resp, "d#gotmess", DOME_BUF);
        nbytes_read=DOME_CMD;
    }
    else if ( (rc = tty_read(PortFD, resp, DOME_CMD, DOME_TIMEOUT, &nbytes_read)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        DEBUGF(INDI::Logger::DBG_ERROR, "ControlDomeShutter error: %s.", errstr);
        return false;
    }

    resp[nbytes_read] = '\0';

    DEBUGF(INDI::Logger::DBG_DEBUG, "RES (%s)", resp);

    if (!strcmp(resp, "d#gotmess"))
    {
         shutterStatus = simShutterStatus = SHUTTER_MOVING;
        return 1;
    }
    else
        return -1;
}
