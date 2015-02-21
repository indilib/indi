/*
    Dome Interface
    Copyright (C) 2014 Jasem Mutlaq (mutlaqja@ikarustech.com)

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

#include <string.h>
#include <math.h>

#include "indidomeinterface.h"
#include "indilogger.h"

INDI::DomeInterface::DomeInterface()
{
    capability.canAbort=false;
    capability.canAbsMove=false;
    capability.canRelMove=true;
    capability.hasShutter=false;
    capability.variableSpeed=false;

    shutterStatus = SHUTTER_UNKNOWN;
}

INDI::DomeInterface::~DomeInterface()
{

}

void INDI::DomeInterface::initDomeProperties(const char *deviceName, const char* groupName)
{
    strncpy(DomeName, deviceName, MAXINDIDEVICE);

    IUFillNumber(&DomeSpeedN[0],"DOME_SPEED_VALUE","RPM","%6.2f",0.0,10,0.1,1.0);
    IUFillNumberVector(&DomeSpeedNP,DomeSpeedN,1,deviceName,"DOME_SPEED","Speed",groupName,IP_RW,60,IPS_OK);

    IUFillNumber(&DomeTimerN[0],"DOME_TIMER_VALUE","Dome Timer (ms)","%4.0f",0.0,10000.0,50.0,1000.0);
    IUFillNumberVector(&DomeTimerNP,DomeTimerN,1,deviceName,"DOME_TIMER","Timer",groupName,IP_RW,60,IPS_OK);

    IUFillSwitch(&DomeMotionS[0],"DOME_CW","Dome CW",ISS_ON);
    IUFillSwitch(&DomeMotionS[1],"DOME_CCW","Dome CCW",ISS_OFF);
    IUFillSwitchVector(&DomeMotionSP,DomeMotionS,2,deviceName,"DOME_MOTION","Direction",groupName,IP_RW,ISR_ATMOST1,60,IPS_OK);

    // Driver can define those to clients if there is support
    IUFillNumber(&DomeAbsPosN[0],"DOME_ABSOLUTE_POSITION","Degrees","%6.2f",0.0,360.0,1.0,0.0);
    IUFillNumberVector(&DomeAbsPosNP,DomeAbsPosN,1,deviceName,"ABS_DOME_POSITION","Absolute Position",groupName,IP_RW,60,IPS_OK);

    IUFillNumber(&DomeRelPosN[0],"DOME_RELATIVE_POSITION","Degrees","%6.2f",0.0,180.0,1.0,0.0);
    IUFillNumberVector(&DomeRelPosNP,DomeRelPosN,1,deviceName,"REL_DOME_POSITION","Relative Position",groupName,IP_RW,60,IPS_OK);

    IUFillSwitch(&AbortS[0],"ABORT","Abort",ISS_OFF);
    IUFillSwitchVector(&AbortSP,AbortS,1,deviceName,"DOME_ABORT_MOTION","Abort Motion",groupName,IP_RW,ISR_ATMOST1,60,IPS_IDLE);

    IUFillNumber(&DomeParamN[DOME_HOME],"HOME_POSITION","Home (deg)","%6.2f",0.0,360.0,1.0,0.0);
    IUFillNumber(&DomeParamN[DOME_PARK],"PARK_POSITION","Park (deg)","%6.2f",0.0,360.0,1.0,0.0);
    IUFillNumber(&DomeParamN[DOME_AUTOSYNC],"AUTOSYNC_THRESHOLD","Autosync threshold (deg)","%6.2f",0.0,360.0,1.0,0.5);
    IUFillNumberVector(&DomeParamNP,DomeParamN,3,deviceName,"DOME_PARAMS","Params",groupName,IP_RW,60,IPS_OK);

    IUFillSwitch(&DomeGotoS[0],"DOME_HOME","Home",ISS_OFF);
    IUFillSwitch(&DomeGotoS[1],"DOME_PARK","Park",ISS_OFF);
    IUFillSwitchVector(&DomeGotoSP,DomeGotoS,2,deviceName,"DOME_GOTO","Goto",groupName,IP_RW,ISR_ATMOST1,60,IPS_OK);

    IUFillSwitch(&DomeShutterS[0],"SHUTTER_OPEN","Open",ISS_OFF);
    IUFillSwitch(&DomeShutterS[1],"SHUTTER_CLOSE","Close",ISS_ON);
    IUFillSwitchVector(&DomeShutterSP,DomeShutterS,2,deviceName,"DOME_SHUTTER","Shutter",groupName,IP_RW,ISR_1OFMANY,60,IPS_OK);

}

bool INDI::DomeInterface::processDomeNumber (const char *dev, const char *name, double values[], char *names[], int n)
{

    if (!strcmp(name, DomeParamNP.name))
    {
        IUUpdateNumber(&DomeParamNP, values, names, n);
        DomeParamNP.s = IPS_OK;
        IDSetNumber(&DomeParamNP, NULL);
        return true;
    }

    if(!strcmp(name, DomeTimerNP.name))
    {
        DomeDirection dir;
        int speed;
        int t, rc;

        //  first we get all the numbers just sent to us
        IUUpdateNumber(&DomeTimerNP,values,names,n);

        //  Now lets find what we need for this move
        speed=DomeSpeedN[0].value;
        if(DomeMotionS[0].s==ISS_ON) dir=DOME_CW;
        else dir=DOME_CCW;
        t=DomeTimerN[0].value;

        rc = MoveDome(dir,speed,t);

        if (rc == 0)
            DomeTimerNP.s = IPS_OK;
        else if (rc == 1)
            DomeTimerNP.s = IPS_BUSY;
        else
            DomeTimerNP.s = IPS_ALERT;

        IDSetNumber(&DomeTimerNP,NULL);
        return true;
    }


    if(!strcmp(name, DomeSpeedNP.name))
    {
        DomeSpeedNP.s=IPS_OK;
        double current_speed = DomeSpeedN[0].value;
        IUUpdateNumber(&DomeSpeedNP,values,names,n);

        if (SetDomeSpeed(DomeSpeedN[0].value) == false)
        {
            DomeSpeedN[0].value = current_speed;
            DomeSpeedNP.s = IPS_ALERT;
        }

        //  Update client display
        IDSetNumber(&DomeSpeedNP,NULL);
        return true;
    }

    if(!strcmp(name, DomeAbsPosNP.name))
    {

        double newPos = values[0];
        int ret =0;

        if (newPos < DomeAbsPosN[0].min || newPos > DomeAbsPosN[0].max)
        {
            IDMessage(dev, "Error: requested azimuth angle %g is out of range.", newPos);
            DomeAbsPosNP.s=IPS_ALERT;
            IDSetNumber(&DomeAbsPosNP, NULL);
            return false;
        }

        if ( (ret = MoveAbsDome(newPos)) == 0)
        {
           DomeAbsPosNP.s=IPS_OK;
           IUUpdateNumber(&DomeAbsPosNP,values,names,n);
           IDSetNumber(&DomeAbsPosNP, "Dome moved to position %g degrees.", newPos);
           return true;
        }
        else if (ret == 1)
        {
           DomeAbsPosNP.s=IPS_BUSY;
           IDSetNumber(&DomeAbsPosNP, "Dome is moving to position %g degrees...", newPos);
           return true;
        }


        DomeAbsPosNP.s = IPS_ALERT;
        IDSetNumber(&DomeAbsPosNP, "Dome failed to move to new requested position.");
        return false;
    }


    if(!strcmp(name, DomeRelPosNP.name))
    {
        double newPos = values[0];
        int ret =0;

        /*if (capability.canAbsMove)
        {
            if (DomeMotionS[0].s == ISS_ON)
            {
                if (DomeAbsPosN[0].value - newPos < DomeAbsPosN[0].min)
                {
                    DomeRelPosNP.s = IPS_ALERT;
                    IDSetNumber(&DomeRelPosNP, NULL);
                    DEBUGFDEVICE(dev, INDI::Logger::DBG_ERROR, "Requested position out of bound. Dome minimum position is %g", DomeAbsPosN[0].min);
                    return false;
                }
            }
            else
            {
                if (DomeAbsPosN[0].value + newPos > DomeAbsPosN[0].max)
                {
                    DomeRelPosNP.s = IPS_ALERT;
                    IDSetNumber(&DomeRelPosNP, NULL);
                    DEBUGFDEVICE(dev, INDI::Logger::DBG_ERROR, "Requested position out of bound. Dome maximum position is %g", DomeAbsPosN[0].max);
                    return false;
                }
            }
        }*/

        if ( (ret=MoveRelDome( (DomeMotionS[0].s == ISS_ON ? DOME_CW : DOME_CCW), newPos)) == 0)
        {
           DomeRelPosNP.s=IPS_OK;
           IUUpdateNumber(&DomeRelPosNP,values,names,n);
           IDSetNumber(&DomeRelPosNP, "Dome moved %g degrees %s.", newPos, (DomeMotionS[0].s == ISS_ON ? "clockwise" : "counter clockwise"));
           if (capability.canAbsMove)
           {
              DomeAbsPosNP.s=IPS_OK;
              IDSetNumber(&DomeAbsPosNP, NULL);
           }
           return true;
        }
        else if (ret == 1)
        {
             IUUpdateNumber(&DomeRelPosNP,values,names,n);
             DomeRelPosNP.s=IPS_BUSY;
             IDSetNumber(&DomeRelPosNP, "Dome is moving %g degrees %s...", newPos, (DomeMotionS[0].s == ISS_ON ? "clockwise" : "counter clockwise"));
             if (capability.canAbsMove)
             {
                DomeAbsPosNP.s=IPS_BUSY;
                IDSetNumber(&DomeAbsPosNP, NULL);
             }
             return true;
        }

        DomeRelPosNP.s = IPS_ALERT;
        IDSetNumber(&DomeRelPosNP, "Dome failed to move to new requested position.");
        return false;
    }

    return false;

}

bool INDI::DomeInterface::processDomeSwitch (const char *dev, const char *name, ISState *states, char *names[], int n)
{
    //  This one is for us
    if(!strcmp(name, DomeMotionSP.name))
    {
        //  client is telling us what to do with Dome direction
        DomeMotionSP.s=IPS_OK;
        IUUpdateSwitch(&DomeMotionSP,states,names,n);
        //  Update client display
        IDSetSwitch(&DomeMotionSP,NULL);

        return true;
    }

    if(!strcmp(name, AbortSP.name))
    {
        IUResetSwitch(&AbortSP);

        if (AbortDome())
            AbortSP.s = IPS_OK;
        else
            AbortSP.s = IPS_ALERT;

        IDSetSwitch(&AbortSP, NULL);
        return true;
    }

    if (!strcmp(name, DomeShutterSP.name))
    {
        int ret=0;
        int prevStatus = IUFindOnSwitchIndex(&DomeShutterSP);
        IUUpdateSwitch(&DomeShutterSP, states, names, n);
        int shutterDome = IUFindOnSwitchIndex(&DomeShutterSP);

        // No change of status, let's return
        if (prevStatus == shutterDome)
        {
            DomeShutterSP.s=IPS_OK;
            IDSetSwitch(&DomeShutterSP,NULL);
        }

        // go back to prev status in case of failure
        IUResetSwitch(&DomeShutterSP);
        DomeShutterS[prevStatus].s = ISS_ON;

        if (shutterDome == 0)
            ret= ControlDomeShutter(SHUTTER_OPEN);
        else
            ret= ControlDomeShutter(SHUTTER_CLOSE);

        if ( ret == 0)
        {
           DomeShutterSP.s=IPS_OK;
           IUResetSwitch(&DomeShutterSP);
           DomeShutterS[shutterDome].s = ISS_ON;
           IDSetSwitch(&DomeShutterSP, "Shutter is %s.", (shutterDome == 0 ? "open" : "closed"));
           return true;
        }
        else if (ret == 1)
        {
             DomeShutterSP.s=IPS_BUSY;
             IUResetSwitch(&DomeShutterSP);
             DomeShutterS[shutterDome].s = ISS_ON;
             IDSetSwitch(&DomeShutterSP, "Shutter is %s...", (shutterDome == 0 ? "opening" : "closing"));
             return true;
        }

        DomeShutterSP.s= IPS_ALERT;
        IDSetSwitch(&DomeShutterSP, "Shutter failed to %s.", (shutterDome == 0 ? "open" : "close"));
        return false;

    }

    if (!strcmp(name, DomeGotoSP.name))
    {
        int ret=0;
        IUUpdateSwitch(&DomeGotoSP, states, names, n);

        int gotoDome = IUFindOnSwitchIndex(&DomeGotoSP);

        if (gotoDome == DOME_HOME)
            ret= HomeDome();
        else
            ret = ParkDome();

        if ( ret == 0)
        {
           DomeGotoSP.s=IPS_OK;
           IUResetSwitch(&DomeGotoSP);
           IDSetSwitch(&DomeGotoSP, "Dome is %s.", (gotoDome == DOME_HOME ? "at home position" : "parked"));
           return true;
        }
        else if (ret == 1)
        {
             DomeGotoSP.s=IPS_BUSY;
             IUResetSwitch(&DomeGotoSP);
             IDSetSwitch(&DomeGotoSP, "Dome is %s...", (gotoDome == DOME_HOME ? "moving to home position" : "parking"));
             DomeAbsPosNP.s = IPS_BUSY;
             IDSetNumber(&DomeAbsPosNP, NULL);
             return true;
        }

        DomeGotoSP.s= IPS_ALERT;
        IDSetNumber(&DomeRelPosNP, "Dome failed to %s.", (gotoDome == DOME_HOME ? "move to home position" : "park"));
        return false;
    }

    return false;

}

int INDI::DomeInterface::MoveDome(DomeDirection dir, double speed, int duration)
{
    //  This should be a virtual function, because the low level hardware class
    //  must override this
    //  but it's much easier early development if the method actually
    //  exists for now
    return -1;
}

int INDI::DomeInterface::MoveRelDome(DomeDirection dir, double azDiff)
{
    //  This should be a virtual function, because the low level hardware class
    //  must override this
    //  but it's much easier early development if the method actually
    //  exists for now
    return -1;
}

int INDI::DomeInterface::MoveAbsDome(double az)
{
    //  This should be a virtual function, because the low level hardware class
    //  must override this
    //  but it's much easier early development if the method actually
    //  exists for now
    return -1;
}


bool INDI::DomeInterface::AbortDome()
{
    //  This should be a virtual function, because the low level hardware class
    //  must override this
    DEBUGDEVICE(DomeName, INDI::Logger::DBG_ERROR, "Dome does not support abort motion.");
    return false;
}

bool INDI::DomeInterface::SetDomeSpeed(double speed)
{
    INDI_UNUSED(speed);

    //  This should be a virtual function, because the low level hardware class
    //  must override this
    DEBUGDEVICE(DomeName, INDI::Logger::DBG_ERROR, "Dome does not support variable speed.");
    return false;
}

int INDI::DomeInterface::ControlDomeShutter(ShutterOperation operation)
{
    INDI_UNUSED(operation);

    //  This should be a virtual function, because the low level hardware class
    //  must override this
    DEBUGDEVICE(DomeName, INDI::Logger::DBG_ERROR, "Dome does not have shutter control.");
    return false;
}

int INDI::DomeInterface::ParkDome()
{
    //  This should be a virtual function, because the low level hardware class
    //  must override this
    DEBUGDEVICE(DomeName, INDI::Logger::DBG_ERROR, "Dome does not support park.");
    return -1;
}

int INDI::DomeInterface::HomeDome()
{
    //  This should be a virtual function, because the low level hardware class
    //  must override this
    DEBUGDEVICE(DomeName, INDI::Logger::DBG_ERROR, "Dome does not support homing.");
    return -1;
}

void INDI::DomeInterface::SetDomeCapability(DomeCapability *cap)
{
    capability.canAbort     = cap->canAbort;
    capability.canAbsMove   = cap->canAbsMove;
    capability.canRelMove   = cap->canRelMove;
    capability.hasShutter   = cap->hasShutter;
    capability.variableSpeed= cap->variableSpeed;
}

const char * INDI::DomeInterface::GetShutterStatusString(ShutterStatus status)
{
    switch (status)
    {
        case SHUTTER_OPENED:
            return "Shutter is open.";
            break;
        case SHUTTER_CLOSED:
            return "Shutter is closed.";
            break;
        case SHUTTER_MOVING:
            return "Shutter is in motion.";
            break;
        case SHUTTER_UNKNOWN:
        default:
            return "Shutter status is unknown.";
            break;
    }
}


