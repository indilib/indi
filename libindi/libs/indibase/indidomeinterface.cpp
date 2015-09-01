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
#include <wordexp.h>

#include "indidomeinterface.h"
#include "indilogger.h"

INDI::DomeInterface::DomeInterface()
{
    capability.canAbort=false;
    capability.canAbsMove=false;
    capability.canRelMove=true;
    capability.hasShutter=false;
    capability.hasVariableSpeed=false;

    shutterState = SHUTTER_UNKNOWN;
    domeState    = DOME_IDLE;


    parkDataType = PARK_NONE;
    Parkdatafile= "~/.indi/ParkData.xml";
    IsParked=false;
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
    IUFillNumber(&DomeParamN[DOME_AUTOSYNC],"AUTOSYNC_THRESHOLD","Autosync threshold (deg)","%6.2f",0.0,360.0,1.0,0.5);
    IUFillNumberVector(&DomeParamNP,DomeParamN,2,deviceName,"DOME_PARAMS","Params",groupName,IP_RW,60,IPS_OK);

    IUFillSwitch(&DomeGotoS[0],"DOME_HOME","Home",ISS_OFF);    
    IUFillSwitchVector(&DomeGotoSP,DomeGotoS,1,deviceName,"DOME_GOTO","Goto",groupName,IP_RW,ISR_ATMOST1,60,IPS_OK);

    IUFillSwitch(&ParkS[0],"PARK","Park",ISS_OFF);
    IUFillSwitch(&ParkS[1],"UNPARK","UnPark",ISS_OFF);
    IUFillSwitchVector(&ParkSP,ParkS,2,deviceName,"DOME_PARK","Parking",groupName,IP_RW,ISR_1OFMANY,60,IPS_OK);

    IUFillSwitch(&DomeShutterS[0],"SHUTTER_OPEN","Open",ISS_OFF);
    IUFillSwitch(&DomeShutterS[1],"SHUTTER_CLOSE","Close",ISS_ON);
    IUFillSwitchVector(&DomeShutterSP,DomeShutterS,2,deviceName,"DOME_SHUTTER","Shutter",groupName,IP_RW,ISR_1OFMANY,60,IPS_OK);

    IUFillSwitch(&ParkOptionS[0],"PARK_CURRENT","Current",ISS_OFF);
    IUFillSwitch(&ParkOptionS[1],"PARK_DEFAULT","Default",ISS_OFF);
    IUFillSwitch(&ParkOptionS[2],"PARK_WRITE_DATA","Write Data",ISS_OFF);
    IUFillSwitchVector(&ParkOptionSP,ParkOptionS,3,deviceName,"DOME_PARK_OPTION","Park Options", SITE_TAB,IP_RW,ISR_ATMOST1,60,IPS_IDLE);

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
        if (domeState == DOME_PARKED)
        {
            DEBUGDEVICE(DomeName, INDI::Logger::DBG_ERROR, "Dome is parked. Please unpark before issuing any motion commands.");
            DomeTimerNP.s = IPS_ALERT;
            IDSetNumber(&DomeTimerNP, NULL);
            return false;
        }

        DomeDirection dir;
        int speed;
        int t;

        //  first we get all the numbers just sent to us
        IUUpdateNumber(&DomeTimerNP,values,names,n);

        //  Now lets find what we need for this move
        speed=DomeSpeedN[0].value;

        if(DomeMotionS[0].s==ISS_ON)
            dir=DOME_CW;
        else
            dir=DOME_CCW;
        t=DomeTimerN[0].value;

        DomeTimerNP.s = Move(dir,speed,t);

        if (DomeTimerNP.s == IPS_BUSY)
            domeState = DOME_MOVING;

        IDSetNumber(&DomeTimerNP,NULL);
        return true;
    }


    if(!strcmp(name, DomeSpeedNP.name))
    {
        DomeSpeedNP.s=IPS_OK;
        double current_speed = DomeSpeedN[0].value;
        IUUpdateNumber(&DomeSpeedNP,values,names,n);

        if (SetSpeed(DomeSpeedN[0].value) == false)
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
        if (domeState == DOME_PARKED)
        {
            DEBUGDEVICE(DomeName, INDI::Logger::DBG_ERROR, "Dome is parked. Please unpark before issuing any motion commands.");
            DomeAbsPosNP.s = IPS_ALERT;
            IDSetNumber(&DomeAbsPosNP, NULL);
            return false;
        }

        double newPos = values[0];
        IPState rc;

        if (newPos < DomeAbsPosN[0].min || newPos > DomeAbsPosN[0].max)
        {
            IDMessage(dev, "Error: requested azimuth angle %g is out of range.", newPos);
            DomeAbsPosNP.s=IPS_ALERT;
            IDSetNumber(&DomeAbsPosNP, NULL);
            return false;
        }

        if ( (rc = MoveAbs(newPos)) == IPS_OK)
        {
           domeState = DOME_IDLE;
           DomeAbsPosNP.s=IPS_OK;
           IUUpdateNumber(&DomeAbsPosNP,values,names,n);
           IDSetNumber(&DomeAbsPosNP, "Dome moved to position %g degrees.", newPos);
           return true;
        }
        else if (rc == IPS_BUSY)
        {
           domeState = DOME_MOVING;
           DomeAbsPosNP.s=IPS_BUSY;
           IDSetNumber(&DomeAbsPosNP, "Dome is moving to position %g degrees...", newPos);
           return true;
        }

        domeState = DOME_IDLE;
        DomeAbsPosNP.s = IPS_ALERT;
        IDSetNumber(&DomeAbsPosNP, "Dome failed to move to new requested position.");
        return false;
    }


    if(!strcmp(name, DomeRelPosNP.name))
    {
        if (domeState == DOME_PARKED)
        {
            DEBUGDEVICE(DomeName, INDI::Logger::DBG_ERROR, "Dome is parked. Please unpark before issuing any motion commands.");
            DomeRelPosNP.s = IPS_ALERT;
            IDSetNumber(&DomeRelPosNP, NULL);
            return false;
        }

        double newPos = values[0];
        IPState rc;

        if ( (rc=MoveRel( (DomeMotionS[0].s == ISS_ON ? DOME_CW : DOME_CCW), newPos)) == IPS_OK)
        {
           domeState = DOME_IDLE;
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
        else if (rc == IPS_BUSY)
        {
             domeState = DOME_MOVING;
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

        domeState = DOME_IDLE;
        DomeRelPosNP.s = IPS_ALERT;
        IDSetNumber(&DomeRelPosNP, "Dome failed to move to new requested position.");
        return false;
    }

    if(strcmp(name, ParkPositionNP.name) == 0)
    {
      IUUpdateNumber(&ParkPositionNP, values, names, n);
      ParkPositionNP.s = IPS_OK;

      Axis1ParkPosition = ParkPositionN[AXIS_RA].value;
      IDSetNumber(&ParkPositionNP, NULL);
      return true;
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

        if (Abort())
        {            
            AbortSP.s = IPS_OK;

            if (domeState == DOME_PARKING)
            {
                DEBUGDEVICE(DomeName, INDI::Logger::DBG_SESSION, "Parking aborted.");
                domeState = DOME_IDLE;
                IUResetSwitch(&ParkSP);
                ParkSP.s = IPS_ALERT;
                IDSetSwitch(&ParkSP, NULL);
            }
        }
        else
            AbortSP.s = IPS_ALERT;

        IDSetSwitch(&AbortSP, NULL);
        return true;
    }

    if (!strcmp(name, DomeShutterSP.name))
    {
        IPState rc;
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
            rc= ControlShutter(SHUTTER_OPEN);
        else
            rc= ControlShutter(SHUTTER_CLOSE);

        if ( rc == IPS_OK)
        {
           DomeShutterSP.s=IPS_OK;
           IUResetSwitch(&DomeShutterSP);
           DomeShutterS[shutterDome].s = ISS_ON;
           IDSetSwitch(&DomeShutterSP, "Shutter is %s.", (shutterDome == 0 ? "open" : "closed"));
           return true;
        }
        else if (rc == IPS_BUSY)
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
        IUResetSwitch(&DomeGotoSP);

        if (domeState == DOME_PARKED)
        {
            DEBUGDEVICE(DomeName, INDI::Logger::DBG_ERROR, "Dome is parked. Please unpark before issuing any motion commands.");
            DomeGotoSP.s = IPS_ALERT;
            IDSetSwitch(&DomeGotoSP, NULL);
            return false;
        }

        DomeAbsPosNP.s = DomeGotoSP.s = Home();

        if (DomeGotoSP.s == IPS_OK)
        {
            domeState = DOME_IDLE;
            DEBUGDEVICE(DomeName, INDI::Logger::DBG_SESSION, "Dome is at home position.");
        }
        else if (DomeGotoSP.s == IPS_BUSY)
        {
            domeState = DOME_MOVING;
            DEBUGDEVICE(DomeName, INDI::Logger::DBG_SESSION, "Dome is moving to home position.");
        }
        else if (DomeGotoSP.s == IPS_ALERT)
            DEBUGDEVICE(DomeName, INDI::Logger::DBG_SESSION, "Dome failed to move to home position.");

        IDSetSwitch(&DomeGotoSP, NULL);
        return true;
    }

    if(!strcmp(name,ParkSP.name))
    {
        /*if (domeState == SCOPE_PARKING)
        {
            IUResetSwitch(&ParkSP);
            ParkSP.s == IPS_IDLE;
            Abort();
            DEBUG(INDI::Logger::DBG_SESSION, "Parking/Unparking aborted.");
            IDSetSwitch(&ParkSP, NULL);
            return true;
        }*/

        IPState rc;
        int preIndex = IUFindOnSwitchIndex(&ParkSP);
        IUUpdateSwitch(&ParkSP, states, names, n);

        bool toPark = (ParkS[0].s == ISS_ON);

        if (toPark == false && domeState != DOME_PARKED)
        {
            IUResetSwitch(&ParkSP);
            ParkS[1].s = ISS_ON;
            ParkSP.s = IPS_IDLE;
            DEBUGDEVICE(DomeName, INDI::Logger::DBG_SESSION, "Dome already unparked.");
            IDSetSwitch(&ParkSP, NULL);
            return true;
        }

        if (toPark && domeState == DOME_PARKED)
        {
            IUResetSwitch(&ParkSP);
            ParkS[0].s = ISS_ON;
            ParkSP.s = IPS_IDLE;
            DEBUGDEVICE(DomeName, INDI::Logger::DBG_SESSION, "Dome already parked.");
            IDSetSwitch(&ParkSP, NULL);
            return true;
        }

        IUResetSwitch(&ParkSP);

        rc = toPark ? Park() : UnPark();

        if (toPark)
        {
            if (rc == IPS_OK)
                SetParked(true);
            else if (rc == IPS_BUSY)
            {
                domeState = DOME_PARKING;

                 if (capability.canAbsMove)
                     DomeAbsPosNP.s=IPS_BUSY;
                 else if (capability.hasVariableSpeed)
                     DomeTimerNP.s = IPS_BUSY;

                 ParkS[0].s = ISS_ON;
            }
        }
        else
        {
            if (rc == IPS_OK)
                SetParked(false);
            else if (rc == IPS_BUSY)
            {
                ParkS[1].s = ISS_ON;
            }
        }

        ParkSP.s = rc;
        if (rc == IPS_ALERT)
            ParkS[preIndex].s = ISS_ON;

        IDSetSwitch(&ParkSP, NULL);
        return true;
    }

    if (!strcmp(name, ParkOptionSP.name))
    {
      IUUpdateSwitch(&ParkOptionSP, states, names, n);
      ISwitch *sp = IUFindOnSwitch(&ParkOptionSP);
      if (!sp)
        return false;

      IUResetSwitch(&ParkOptionSP);

      if (!strcmp(sp->name, "PARK_CURRENT"))
      {
          SetCurrentPark();
      }
      else if (!strcmp(sp->name, "PARK_DEFAULT"))
      {
          SetDefaultPark();
      }
      else if (!strcmp(sp->name, "PARK_WRITE_DATA"))
      {
        if (WriteParkData())
          DEBUGDEVICE(DomeName, INDI::Logger::DBG_SESSION, "Saved Park Status/Position.");
        else
          DEBUGDEVICE(DomeName, INDI::Logger::DBG_WARNING, "Can not save Park Status/Position.");
      }

      ParkOptionSP.s = IPS_OK;
      IDSetSwitch(&ParkOptionSP, NULL);

      return true;
    }

    return false;

}

IPState INDI::DomeInterface::Move(DomeDirection dir, double speed, int duration)
{
    //  This should be a virtual function, because the low level hardware class
    //  must override this
    //  but it's much easier early development if the method actually
    //  exists for now
    return IPS_ALERT;
}

IPState INDI::DomeInterface::MoveRel(DomeDirection dir, double azDiff)
{
    //  This should be a virtual function, because the low level hardware class
    //  must override this
    //  but it's much easier early development if the method actually
    //  exists for now
    return IPS_ALERT;
}

IPState INDI::DomeInterface::MoveAbs(double az)
{
    //  This should be a virtual function, because the low level hardware class
    //  must override this
    //  but it's much easier early development if the method actually
    //  exists for now
    return IPS_ALERT;
}


bool INDI::DomeInterface::Abort()
{
    //  This should be a virtual function, because the low level hardware class
    //  must override this
    DEBUGDEVICE(DomeName, INDI::Logger::DBG_ERROR, "Dome does not support abort motion.");
    return false;
}

bool INDI::DomeInterface::SetSpeed(double speed)
{
    INDI_UNUSED(speed);

    //  This should be a virtual function, because the low level hardware class
    //  must override this
    DEBUGDEVICE(DomeName, INDI::Logger::DBG_ERROR, "Dome does not support variable speed.");
    return false;
}

IPState INDI::DomeInterface::ControlShutter(ShutterOperation operation)
{
    INDI_UNUSED(operation);

    //  This should be a virtual function, because the low level hardware class
    //  must override this
    DEBUGDEVICE(DomeName, INDI::Logger::DBG_ERROR, "Dome does not have shutter control.");
    return IPS_ALERT;
}

IPState INDI::DomeInterface::Park()
{
    //  This should be a virtual function, because the low level hardware class
    //  must override this
    DEBUGDEVICE(DomeName, INDI::Logger::DBG_ERROR, "Dome does not support park.");
    return IPS_ALERT;
}

IPState INDI::DomeInterface::UnPark()
{
    //  This should be a virtual function, because the low level hardware class
    //  must override this
    DEBUGDEVICE(DomeName, INDI::Logger::DBG_ERROR, "Dome does not support park.");
    return IPS_ALERT;
}

void INDI::DomeInterface::SetCurrentPark()
{
    DEBUGDEVICE(DomeName, INDI::Logger::DBG_WARNING, "Parking is not supported.");
}

void INDI::DomeInterface::SetDefaultPark()
{
    DEBUGDEVICE(DomeName, INDI::Logger::DBG_WARNING, "Parking is not supported.");
}



IPState INDI::DomeInterface::Home()
{
    //  This should be a virtual function, because the low level hardware class
    //  must override this
    DEBUGDEVICE(DomeName, INDI::Logger::DBG_ERROR, "Dome does not support homing.");
    return IPS_ALERT;
}

void INDI::DomeInterface::SetDomeCapability(DomeCapability *cap)
{
    capability.canAbort     = cap->canAbort;
    capability.canAbsMove   = cap->canAbsMove;
    capability.canRelMove   = cap->canRelMove;
    capability.canPark      = cap->canPark;
    capability.hasShutter   = cap->hasShutter;
    capability.hasVariableSpeed= cap->hasVariableSpeed;
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

void INDI::DomeInterface::SetParkDataType(INDI::DomeInterface::DomeParkData type)
{
    parkDataType = type;

    if (parkDataType != PARK_NONE)
    {
        switch (parkDataType)
        {
            case PARK_AZ:
            IUFillNumber(&ParkPositionN[AXIS_AZ],"PARK_AZ","AZ D:M:S", "%10.6m", 0.0, 360.0, 0.0, 0);
            break;

            case PARK_AZ_ENCODER:
            IUFillNumber(&ParkPositionN[AXIS_AZ],"PARK_AZ" ,"AZ Encoder","%.0f" ,0,16777215,1,0);
            break;

        default:
            break;
        }

        IUFillNumberVector(&ParkPositionNP,ParkPositionN,1,DomeName,"DOME_PARK_POSITION","Park Position", SITE_TAB,IP_RW,60,IPS_IDLE);
    }
}

void INDI::DomeInterface::SetParked(bool isparked)
{
  IsParked=isparked;
  IUResetSwitch(&ParkSP);
  if (IsParked)
  {
      domeState = DOME_PARKED;
      ParkSP.s = IPS_OK;
      ParkS[0].s = ISS_ON;
      DEBUGDEVICE(DomeName, INDI::Logger::DBG_SESSION, "Dome is parked.");
  }
  else
  {
      domeState = DOME_IDLE;
      ParkSP.s=IPS_IDLE;
      ParkS[1].s = ISS_ON;
      DEBUGDEVICE(DomeName, INDI::Logger::DBG_SESSION, "Dome is unparked.");
  }

  IDSetSwitch(&ParkSP, NULL);

  WriteParkData();
}

bool INDI::DomeInterface::isParked()
{
  return IsParked;
}

bool INDI::DomeInterface::InitPark()
{
  char *loadres;
  loadres=LoadParkData();
  if (loadres)
  {
    DEBUGFDEVICE(DomeName, INDI::Logger::DBG_SESSION, "InitPark: No Park data in file %s: %s", Parkdatafile, loadres);
    SetParked(false);
    return false;
  }

  SetParked(isParked());

  ParkPositionN[AXIS_AZ].value = Axis1ParkPosition;
  IDSetNumber(&ParkPositionNP, NULL);

  // If parked, store the position as current azimuth angle or encoder ticks
  if (isParked() && capability.canAbsMove)
  {
      DomeAbsPosN[0].value = ParkPositionN[AXIS_AZ].value;
      IDSetNumber(&DomeAbsPosNP, NULL);
  }

  return true;
}

char *INDI::DomeInterface::LoadParkData()
{
  wordexp_t wexp;
  FILE *fp;
  LilXML *lp;
  static char errmsg[512];

  XMLEle *parkxml;
  XMLAtt *ap;
  bool devicefound=false;

  ParkDeviceName = DomeName;
  ParkstatusXml=NULL;
  ParkdeviceXml=NULL;
  ParkpositionXml = NULL;
  ParkpositionAxis1Xml = NULL;

  if (wordexp(Parkdatafile, &wexp, 0))
  {
    wordfree(&wexp);
    return (char *)("Badly formed filename.");
  }

  if (!(fp=fopen(wexp.we_wordv[0], "r")))
  {
    wordfree(&wexp);
    return strerror(errno);
  }
  wordfree(&wexp);

 lp = newLilXML();

 if (ParkdataXmlRoot)
    delXMLEle(ParkdataXmlRoot);

  ParkdataXmlRoot = readXMLFile(fp, lp, errmsg);

  delLilXML(lp);
  if (!ParkdataXmlRoot)
      return errmsg;

  if (!strcmp(tagXMLEle(nextXMLEle(ParkdataXmlRoot, 1)), "parkdata"))
      return (char *)("Not a park data file");

  parkxml=nextXMLEle(ParkdataXmlRoot, 1);

  while (parkxml)
  {
    if (strcmp(tagXMLEle(parkxml), "device"))
    {
        parkxml=nextXMLEle(ParkdataXmlRoot, 0);
        continue;
    }
    ap = findXMLAtt(parkxml, "name");
    if (ap && (!strcmp(valuXMLAtt(ap), ParkDeviceName)))
    {
        devicefound = true;
        break;
    }
    parkxml=nextXMLEle(ParkdataXmlRoot, 0);
  }

  if (!devicefound)
      return (char *)"No park data found for this device";

  IsParked=false;
  ParkdeviceXml=parkxml;
  ParkstatusXml = findXMLEle(parkxml, "parkstatus");

  if (ParkstatusXml == NULL)
  {
      return (char *)("Park data invalid or missing.");
  }

  if (parkDataType != PARK_NONE)
  {
      ParkpositionXml = findXMLEle(parkxml, "parkposition");
      ParkpositionAxis1Xml = findXMLEle(ParkpositionXml, "axis1position");

      if (ParkpositionAxis1Xml == NULL)
      {
          return (char *)("Park data invalid or missing.");
      }
  }


  if (!strcmp(pcdataXMLEle(ParkstatusXml), "true"))
      IsParked=true;

  if (parkDataType != PARK_NONE)
    sscanf(pcdataXMLEle(ParkpositionAxis1Xml), "%lf", &Axis1ParkPosition);

  return NULL;
}

bool INDI::DomeInterface::WriteParkData()
{
  wordexp_t wexp;
  FILE *fp;
  char pcdata[30];

  if (wordexp(Parkdatafile, &wexp, 0))
  {
    wordfree(&wexp);
    DEBUGFDEVICE(DomeName, INDI::Logger::DBG_SESSION, "WriteParkData: can not write file %s: Badly formed filename.", Parkdatafile);
    return false;
  }

  if (!(fp=fopen(wexp.we_wordv[0], "w")))
  {
    wordfree(&wexp);
    DEBUGFDEVICE(DomeName, INDI::Logger::DBG_SESSION, "WriteParkData: can not write file %s: %s", Parkdatafile, strerror(errno));
    return false;
  }

  if (!ParkdataXmlRoot)
      ParkdataXmlRoot=addXMLEle(NULL, "parkdata");

  if (!ParkdeviceXml)
  {
    ParkdeviceXml=addXMLEle(ParkdataXmlRoot, "device");
    addXMLAtt(ParkdeviceXml, "name", ParkDeviceName);
  }

  if (!ParkstatusXml)
      ParkstatusXml=addXMLEle(ParkdeviceXml, "parkstatus");

  if (parkDataType != PARK_NONE)
  {
      if (!ParkpositionXml)
          ParkpositionXml=addXMLEle(ParkdeviceXml, "parkposition");
      if (!ParkpositionAxis1Xml)
          ParkpositionAxis1Xml=addXMLEle(ParkpositionXml, "axis1position");
  }

  editXMLEle(ParkstatusXml, (IsParked?"true":"false"));

  if (parkDataType != PARK_NONE)
  {
      snprintf(pcdata, sizeof(pcdata), "%f", Axis1ParkPosition);
      editXMLEle(ParkpositionAxis1Xml, pcdata);
  }

  prXMLEle(fp, ParkdataXmlRoot, 0);
  fclose(fp);

  return true;
}

double INDI::DomeInterface::GetAxis1Park()
{
  return Axis1ParkPosition;
}

double INDI::DomeInterface::GetAxis1ParkDefault()
{
  return Axis1DefaultParkPosition;
}

void INDI::DomeInterface::SetAxis1Park(double value)
{
  Axis1ParkPosition=value;
  ParkPositionN[AXIS_RA].value = value;
  IDSetNumber(&ParkPositionNP, NULL);
}

void INDI::DomeInterface::SetAxis1ParkDefault(double value)
{
  Axis1DefaultParkPosition=value;
}



