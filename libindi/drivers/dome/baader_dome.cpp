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
#define DOME_BUF            16              /* Dome command buffer */
#define DOME_TIMEOUT        3               /* 3 seconds comm timeout */

#define SIM_SHUTTER_TIMER   5.0             /* Simulated Shutter closes/open in 5 seconds */
#define SIM_DOME_HI_SPEED   5.0             /* Simulated dome speed 5.0 degrees per second, constant */
#define SIM_DOME_LO_SPEED   0.5             /* Simulated dome speed 0.5 degrees per second, constant */

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

   status = DOME_UNKNOWN;

   DomeCapability cap;

   cap.canAbort = true;         // no real abort, we set target position to current position to "abort"
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

    IUFillSwitch(&CalibrateS[0], "Start", "", ISS_OFF);
    IUFillSwitchVector(&CalibrateSP, CalibrateS, 1, getDeviceName(), "Calibrate", "", MAIN_CONTROL_TAB, IP_RW, ISR_ATMOST1, 0, IPS_IDLE);

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

        defineSwitch(&CalibrateSP);

        SetupParms();
    }
    else
        deleteProperty(CalibrateSP.name);


    return true;
}

/************************************************************************************
 *
* ***********************************************************************************/
bool BaaderDome::ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if(strcmp(dev,getDeviceName())==0)
    {
        if (!strcmp(name, CalibrateSP.name))
        {
            IUResetSwitch(&CalibrateSP);

            if (status == DOME_READY)
            {
                CalibrateSP.s = IPS_OK;
                DEBUG(INDI::Logger::DBG_SESSION, "Dome is already calibrated.");
                IDSetSwitch(&CalibrateSP, NULL);
                return true;
            }

            if (CalibrateSP.s == IPS_BUSY)
            {
                AbortDome();
                DEBUG(INDI::Logger::DBG_SESSION, "Calibration aborted.");
                status = DOME_UNKNOWN;
                CalibrateSP.s = IPS_IDLE;
                IDSetSwitch(&CalibrateSP, NULL);
                return true;
            }

            status = DOME_CALIBRATING;

            DEBUG(INDI::Logger::DBG_SESSION, "Starting calibration procedure...");

            calibrationStage = CALIBRATION_STAGE1;

            calibrationStart = DomeAbsPosN[0].value;

            // Goal of procedure is to reach south point to hit sensor
            calibrationTarget1 = calibrationStart + 179;
            if (calibrationTarget1 > 360)
                calibrationTarget1 -= 360;

            if (MoveAbsDome(calibrationTarget1) == false)
            {
                CalibrateSP.s = IPS_ALERT;
                DEBUG(INDI::Logger::DBG_ERROR, "Calibration failue due to dome motion failure.");
                status = DOME_UNKNOWN;
                IDSetSwitch(&CalibrateSP, NULL);
                return false;
            }

            DomeAbsPosNP.s = IPS_BUSY;
            CalibrateSP.s = IPS_BUSY;
            DEBUGF(INDI::Logger::DBG_SESSION, "Calibration is in progress. Moving to position %g.", calibrationTarget1);
            IDSetSwitch(&CalibrateSP, NULL);
            return true;
        }
    }

    return INDI::Dome::ISNewSwitch(dev, name, states, names, n);
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
            strncpy(resp, "d#shutclo", DOME_CMD);
        else if (simShutterStatus == SHUTTER_OPENED)
            strncpy(resp, "d#shutope", DOME_CMD);
        else if (simShutterStatus == SHUTTER_MOVING)
            strncpy(resp, "d#shutrun", DOME_CMD);
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

        if (status == DOME_READY || calibrationStage == CALIBRATION_COMPLETE)
            snprintf(resp, DOME_BUF, "d#azr%04d", MountAzToDomeAz(DomeAbsPosN[0].value));
        else
            snprintf(resp, DOME_BUF, "d#azi%04d", MountAzToDomeAz(DomeAbsPosN[0].value));
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
        if (status == DOME_CALIBRATING)
        {
            status = DOME_READY;
            calibrationStage = CALIBRATION_STAGE1;
            DEBUG(INDI::Logger::DBG_SESSION, "Calibration complete.");
            CalibrateSP.s = IPS_OK;
            IDSetSwitch(&CalibrateSP, NULL);
        }

        DomeAbsPosN[0].value = DomeAzToMountAz(domeAz);
        return true;
    }
    else
    {
        rc = sscanf(resp, "d#azi%hu", &domeAz);
        if (rc > 0)
        {
            DomeAbsPosN[0].value = DomeAzToMountAz(domeAz);
            return true;
        }
        else
            return false;
    }
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

    UpdatePosition();

    if (DomeAbsPosNP.s == IPS_BUSY)
    {
        if (sim)
        {
            double speed = 0;
            if (fabs(targetAz - DomeAbsPosN[0].value) > SIM_DOME_HI_SPEED)
                speed = SIM_DOME_HI_SPEED;
            else
                speed = SIM_DOME_LO_SPEED;

            if (targetAz > DomeAbsPosN[0].value)
            {
                DomeAbsPosN[0].value += speed;
            }
            else if (targetAz < DomeAbsPosN[0].value)
            {
                DomeAbsPosN[0].value -= speed;
            }

            if (DomeAbsPosN[0].value < DomeAbsPosN[0].min)
                DomeAbsPosN[0].value += DomeAbsPosN[0].max;
            if (DomeAbsPosN[0].value > DomeAbsPosN[0].max)
                DomeAbsPosN[0].value -= DomeAbsPosN[0].max;
        }

        if (fabs(targetAz - DomeAbsPosN[0].value) < DomeParamN[DOME_AUTOSYNC].value)
        {
            DomeAbsPosN[0].value = targetAz;
            DomeAbsPosNP.s = IPS_OK;
            DEBUG(INDI::Logger::DBG_SESSION, "Dome reached requested azimuth angle.");
            if (DomeGotoSP.s == IPS_BUSY)
            {
                DomeGotoSP.s = IPS_OK;
                IDSetSwitch(&DomeGotoSP, NULL);
            }
            if (DomeRelPosNP.s == IPS_BUSY)
            {
                DomeRelPosNP.s = IPS_OK;
                IDSetNumber(&DomeRelPosNP, NULL);
            }

            if (status == DOME_CALIBRATING)
            {
                if (calibrationStage == CALIBRATION_STAGE1)
                {
                    DEBUG(INDI::Logger::DBG_SESSION, "Calibration stage 1 complete. Starting stage 2...");
                    calibrationTarget2 = DomeAbsPosN[0].value + 2;
                    calibrationStage = CALIBRATION_STAGE2;
                    MoveAbsDome(calibrationTarget2);
                    DomeAbsPosNP.s = IPS_BUSY;
                }
                else if (calibrationStage == CALIBRATION_STAGE2)
                {
                    DEBUGF(INDI::Logger::DBG_SESSION, "Calibration stage 2 complete. Returning to initial position %g...", calibrationStart);
                    calibrationStage = CALIBRATION_STAGE3;
                    MoveAbsDome(calibrationStart);
                    DomeAbsPosNP.s = IPS_BUSY;
                }
                else if (calibrationStage == CALIBRATION_STAGE3)
                {
                    calibrationStage = CALIBRATION_COMPLETE;
                    DEBUG(INDI::Logger::DBG_SESSION, "Dome reached initial position.");
                }
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
    int nbytes_written=0, nbytes_read=0, rc=-1;
    char errstr[MAXRBUF];
    char cmd[DOME_BUF];
    char resp[DOME_BUF];

    if (status == DOME_UNKNOWN)
    {
        DEBUG(INDI::Logger::DBG_WARNING, "Dome is not calibrated. Please calibrate dome before issuing any commands.");
        return -1;
    }

    targetAz = az;

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
        strncpy(resp, "d#gotmess", DOME_CMD);
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
        strncpy(cmd, "d#opeshut", DOME_CMD);
    }
    else
    {
        targetShutter = SHUTTER_CLOSED;
        strncpy(cmd, "d#closhut", DOME_CMD);
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
        strncpy(resp, "d#gotmess", DOME_CMD);
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

/************************************************************************************
 *
* ***********************************************************************************/
bool BaaderDome::AbortDome()
{
    DEBUGF(INDI::Logger::DBG_SESSION, "Attempting to abort dome motion by stopping at %g", DomeAbsPosN[0].value);
    MoveAbsDome(DomeAbsPosN[0].value);
    return true;
}
