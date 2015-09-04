/*******************************************************************************
  Copyright(c) 2011 Gerry Rozema, Jasem Mutlaq. All rights reserved.

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Library General Public
 License version 2 as published by the Free Software Foundation.

 This library is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 Library General Public License for more details.

 You should have received a copy of the GNU Library General Public License
 along with this library; see the file COPYING.LIB.  If not, write to
 the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 Boston, MA 02110-1301, USA.
*******************************************************************************/
#include <stdlib.h>
#include <wordexp.h>

#include "inditelescope.h"
#include "indicom.h"

#define POLLMS 1000

INDI::Telescope::Telescope()
{
    capability.canPark = capability.canSync = capability.canAbort = false;
    capability.nSlewRate = 0;
    last_we_motion = last_ns_motion = -1;
    parkDataType = PARK_NONE;
    Parkdatafile= "~/.indi/ParkData.xml";
    IsParked=false;
    SlewRateS = NULL;

    controller = new INDI::Controller(this);
    controller->setJoystickCallback(joystickHelper);
    controller->setButtonCallback(buttonHelper);

}

INDI::Telescope::~Telescope()
{
    delete (controller);
}

bool INDI::Telescope::initProperties()
{
    DefaultDevice::initProperties();

    // Active Devices
    IUFillText(&ActiveDeviceT[0],"ACTIVE_GPS","GPS","GPS Simulator");
    IUFillTextVector(&ActiveDeviceTP,ActiveDeviceT,1,getDeviceName(),"ACTIVE_DEVICES","Snoop devices",OPTIONS_TAB,IP_RW,60,IPS_IDLE);

    IUFillNumber(&EqN[AXIS_RA],"RA","RA (hh:mm:ss)","%010.6m",0,24,0,0);
    IUFillNumber(&EqN[AXIS_DE],"DEC","DEC (dd:mm:ss)","%010.6m",-90,90,0,0);
    IUFillNumberVector(&EqNP,EqN,2,getDeviceName(),"EQUATORIAL_EOD_COORD","Eq. Coordinates",MAIN_CONTROL_TAB,IP_RW,60,IPS_IDLE);
    lastEqState = IPS_IDLE;

    IUFillSwitch(&ParkOptionS[0],"PARK_CURRENT","Current",ISS_OFF);
    IUFillSwitch(&ParkOptionS[1],"PARK_DEFAULT","Default",ISS_OFF);
    IUFillSwitch(&ParkOptionS[2],"PARK_WRITE_DATA","Write Data",ISS_OFF);
    IUFillSwitchVector(&ParkOptionSP,ParkOptionS,3,getDeviceName(),"TELESCOPE_PARK_OPTION","Park Options", SITE_TAB,IP_RW,ISR_ATMOST1,60,IPS_IDLE);

    IUFillText(&TimeT[0],"UTC","UTC Time",NULL);
    IUFillText(&TimeT[1],"OFFSET","UTC Offset",NULL);
    IUFillTextVector(&TimeTP,TimeT,2,getDeviceName(),"TIME_UTC","UTC",SITE_TAB,IP_RW,60,IPS_IDLE);

    IUFillNumber(&LocationN[LOCATION_LATITUDE],"LAT","Lat (dd:mm:ss)","%010.6m",-90,90,0,0.0);
    IUFillNumber(&LocationN[LOCATION_LONGITUDE],"LONG","Lon (dd:mm:ss)","%010.6m",0,360,0,0.0 );
    IUFillNumber(&LocationN[LOCATION_ELEVATION],"ELEV","Elevation (m)","%g",-200,10000,0,0 );
    IUFillNumberVector(&LocationNP,LocationN,3,getDeviceName(),"GEOGRAPHIC_COORD","Scope Location",SITE_TAB,IP_RW,60,IPS_OK);

    IUFillSwitch(&CoordS[0],"TRACK","Track",ISS_ON);
    IUFillSwitch(&CoordS[1],"SLEW","Slew",ISS_OFF);
    IUFillSwitch(&CoordS[2],"SYNC","Sync",ISS_OFF);

    if (capability.canSync)
        IUFillSwitchVector(&CoordSP,CoordS,3,getDeviceName(),"ON_COORD_SET","On Set",MAIN_CONTROL_TAB,IP_RW,ISR_1OFMANY,60,IPS_IDLE);
    else
        IUFillSwitchVector(&CoordSP,CoordS,2,getDeviceName(),"ON_COORD_SET","On Set",MAIN_CONTROL_TAB,IP_RW,ISR_1OFMANY,60,IPS_IDLE);

    if (capability.nSlewRate >= 4)
        IUFillSwitchVector(&SlewRateSP, SlewRateS, capability.nSlewRate, getDeviceName(), "TELESCOPE_SLEW_RATE", "Slew Rate", MOTION_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    IUFillSwitch(&ParkS[0],"PARK","Park",ISS_OFF);
    IUFillSwitch(&ParkS[1],"UNPARK","UnPark",ISS_OFF);
    IUFillSwitchVector(&ParkSP,ParkS,2,getDeviceName(),"TELESCOPE_PARK","Parking",MAIN_CONTROL_TAB,IP_RW,ISR_1OFMANY,60,IPS_IDLE);

    IUFillSwitch(&AbortS[0],"ABORT","Abort",ISS_OFF);
    IUFillSwitchVector(&AbortSP,AbortS,1,getDeviceName(),"TELESCOPE_ABORT_MOTION","Abort Motion",MAIN_CONTROL_TAB,IP_RW,ISR_ATMOST1,60,IPS_IDLE);

    IUFillText(&PortT[0],"PORT","Port","/dev/ttyUSB0");
    IUFillTextVector(&PortTP,PortT,1,getDeviceName(),"DEVICE_PORT","Ports",OPTIONS_TAB,IP_RW,60,IPS_IDLE);

    IUFillSwitch(&BaudRateS[0], "9600", "", ISS_ON);
    IUFillSwitch(&BaudRateS[1], "19200", "", ISS_OFF);
    IUFillSwitch(&BaudRateS[2], "38400", "", ISS_OFF);
    IUFillSwitch(&BaudRateS[3], "57600", "", ISS_OFF);
    IUFillSwitch(&BaudRateS[4], "115200", "", ISS_OFF);
    IUFillSwitch(&BaudRateS[5], "230400", "", ISS_OFF);
    IUFillSwitchVector(&BaudRateSP, BaudRateS, 6, getDeviceName(),"TELESCOPE_BAUD_RATE", "Baud Rate", OPTIONS_TAB, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);

    IUFillSwitch(&MovementNSS[DIRECTION_NORTH], "MOTION_NORTH", "North", ISS_OFF);
    IUFillSwitch(&MovementNSS[DIRECTION_SOUTH], "MOTION_SOUTH", "South", ISS_OFF);
    IUFillSwitchVector(&MovementNSSP, MovementNSS, 2, getDeviceName(),"TELESCOPE_MOTION_NS", "Motion N/S", MOTION_TAB, IP_RW, ISR_ATMOST1, 60, IPS_IDLE);

    IUFillSwitch(&MovementWES[DIRECTION_WEST], "MOTION_WEST", "West", ISS_OFF);
    IUFillSwitch(&MovementWES[DIRECTION_EAST], "MOTION_EAST", "East", ISS_OFF);
    IUFillSwitchVector(&MovementWESP, MovementWES, 2, getDeviceName(),"TELESCOPE_MOTION_WE", "Motion W/E", MOTION_TAB, IP_RW, ISR_ATMOST1, 60, IPS_IDLE);

    IUFillNumber(&ScopeParametersN[0],"TELESCOPE_APERTURE","Aperture (mm)","%g",50,4000,0,0.0);
    IUFillNumber(&ScopeParametersN[1],"TELESCOPE_FOCAL_LENGTH","Focal Length (mm)","%g",100,10000,0,0.0 );
    IUFillNumber(&ScopeParametersN[2],"GUIDER_APERTURE","Guider Aperture (mm)","%g",50,4000,0,0.0);
    IUFillNumber(&ScopeParametersN[3],"GUIDER_FOCAL_LENGTH","Guider Focal Length (mm)","%g",100,10000,0,0.0 );
    IUFillNumberVector(&ScopeParametersNP,ScopeParametersN,4,getDeviceName(),"TELESCOPE_INFO","Scope Properties",OPTIONS_TAB,IP_RW,60,IPS_OK);

    controller->mapController("MOTIONDIR", "N/S/W/E Control", INDI::Controller::CONTROLLER_JOYSTICK, "JOYSTICK_1");
    if (capability.nSlewRate >= 4)
        controller->mapController("SLEWPRESET", "Slew Rate", INDI::Controller::CONTROLLER_JOYSTICK, "JOYSTICK_2");
    if (capability.canAbort)
        controller->mapController("ABORTBUTTON", "Abort", INDI::Controller::CONTROLLER_BUTTON, "BUTTON_1");
    if (capability.canPark)
    {
        controller->mapController("PARKBUTTON", "Park", INDI::Controller::CONTROLLER_BUTTON, "BUTTON_2");
        controller->mapController("UNPARKBUTTON", "UnPark", INDI::Controller::CONTROLLER_BUTTON, "BUTTON_3");
    }
    controller->initProperties();

    TrackState=SCOPE_IDLE;

    setInterfaceDescriptor(TELESCOPE_INTERFACE);

    IDSnoopDevice(ActiveDeviceT[0].text,"GEOGRAPHIC_COORD");
    IDSnoopDevice(ActiveDeviceT[0].text,"TIME_UTC");

    return true;
}

void INDI::Telescope::ISGetProperties (const char *dev)
{
    //  First we let our parent populate
    DefaultDevice::ISGetProperties(dev);

    defineText(&PortTP);
    defineSwitch(&BaudRateSP);

    if(isConnected())
    {
        //  Now we add our telescope specific stuff
        defineSwitch(&CoordSP);
        defineNumber(&EqNP);
        if (capability.canAbort)
            defineSwitch(&AbortSP);

        if (capability.hasTime)
            defineText(&TimeTP);
        if (capability.hasLocation)
            defineNumber(&LocationNP);

        if (capability.canPark)
        {
            defineSwitch(&ParkSP);
            if (parkDataType != PARK_NONE)
            {
                defineNumber(&ParkPositionNP);
                defineSwitch(&ParkOptionSP);
            }
        }
        defineSwitch(&MovementNSSP);
        defineSwitch(&MovementWESP);

        if (capability.nSlewRate >= 4)
            defineSwitch(&SlewRateSP);

        defineNumber(&ScopeParametersNP);

        if (capability.hasTime && capability.hasLocation)
            defineText(&ActiveDeviceTP);

    }

    controller->ISGetProperties(dev);

}

bool INDI::Telescope::updateProperties()
{

    if(isConnected())
    {
        //  Now we add our telescope specific stuff
        defineSwitch(&CoordSP);
        defineNumber(&EqNP);
        if (capability.canAbort)
            defineSwitch(&AbortSP);
        defineSwitch(&MovementNSSP);
        defineSwitch(&MovementWESP);
        if (capability.nSlewRate >= 4)
            defineSwitch(&SlewRateSP);

        if (capability.hasTime)
            defineText(&TimeTP);
        if (capability.hasLocation)
            defineNumber(&LocationNP);
        if (capability.canPark)
        {
            defineSwitch(&ParkSP);
            if (parkDataType != PARK_NONE)
            {
                defineNumber(&ParkPositionNP);
                defineSwitch(&ParkOptionSP);
            }
        }
        defineNumber(&ScopeParametersNP);

        if (capability.hasTime && capability.hasLocation)
            defineText(&ActiveDeviceTP);

    }
    else
    {
        deleteProperty(CoordSP.name);
        deleteProperty(EqNP.name);
        if (capability.canAbort)
            deleteProperty(AbortSP.name);
        deleteProperty(MovementNSSP.name);
        deleteProperty(MovementWESP.name);
        if (capability.nSlewRate >= 4)
            deleteProperty(SlewRateSP.name);

        if (capability.hasTime)
            deleteProperty(TimeTP.name);
        if (capability.hasLocation)
            deleteProperty(LocationNP.name);

        if (capability.canPark)
        {
            deleteProperty(ParkSP.name);
            if (parkDataType != PARK_NONE)
            {
                deleteProperty(ParkPositionNP.name);
                deleteProperty(ParkOptionSP.name);
            }
        }
        deleteProperty(ScopeParametersNP.name);

        if (capability.hasTime && capability.hasLocation)
            deleteProperty(ActiveDeviceTP.name);
    }

    controller->updateProperties();

    return true;
}

bool INDI::Telescope::ISSnoopDevice(XMLEle *root)
{
    controller->ISSnoopDevice(root);

    XMLEle *ep=NULL;
    const char *propName = findXMLAttValu(root, "name");

    if (isConnected())
    {
        if (capability.hasLocation && !strcmp(propName, "GEOGRAPHIC_COORD"))
        {
            // Only accept IPS_OK state
            if (strcmp(findXMLAttValu(root, "state"), "Ok"))
                return false;

            double longitude=-1, latitude=-1, elevation=-1;

            for (ep = nextXMLEle(root, 1) ; ep != NULL ; ep = nextXMLEle(root, 0))
            {
                const char *elemName = findXMLAttValu(ep, "name");

                if (!strcmp(elemName, "LAT"))
                    latitude = atof(pcdataXMLEle(ep));
                else if (!strcmp(elemName, "LONG"))
                    longitude = atof(pcdataXMLEle(ep));
                else if (!strcmp(elemName, "ELEV"))
                    elevation = atof(pcdataXMLEle(ep));

            }

            return processLocationInfo(latitude, longitude, elevation);
        }
        else if (capability.hasTime && !strcmp(propName, "TIME_UTC"))
        {
            // Only accept IPS_OK state
            if (strcmp(findXMLAttValu(root, "state"), "Ok"))
                return false;

            char utc[MAXINDITSTAMP], offset[MAXINDITSTAMP];

            for (ep = nextXMLEle(root, 1) ; ep != NULL ; ep = nextXMLEle(root, 0))
            {
                const char *elemName = findXMLAttValu(ep, "name");

                if (!strcmp(elemName, "UTC"))
                    strncpy(utc, pcdataXMLEle(ep), MAXINDITSTAMP);
                else if (!strcmp(elemName, "OFFSET"))
                    strncpy(offset, pcdataXMLEle(ep), MAXINDITSTAMP);
            }

            return processTimeInfo(utc, offset);
        }
    }

    return INDI::DefaultDevice::ISSnoopDevice(root);
}

bool INDI::Telescope::saveConfigItems(FILE *fp)
{
    IUSaveConfigText(fp, &ActiveDeviceTP);
    IUSaveConfigText(fp, &PortTP);
    IUSaveConfigSwitch(fp, &BaudRateSP);
    if (capability.hasLocation)
        IUSaveConfigNumber(fp,&LocationNP);
    IUSaveConfigNumber(fp, &ScopeParametersNP);

    controller->saveConfigItems(fp);

    return true;
}

void INDI::Telescope::NewRaDec(double ra,double dec)
{
    switch(TrackState)
    {
       case SCOPE_PARKED:
       case SCOPE_IDLE:
        EqNP.s=IPS_IDLE;
        break;

       case SCOPE_SLEWING:
        EqNP.s=IPS_BUSY;
        break;

       case SCOPE_TRACKING:
        EqNP.s=IPS_OK;
        break;

      default:
        break;
    }

    if (EqN[AXIS_RA].value != ra || EqN[AXIS_DE].value != dec || EqNP.s != lastEqState)
    {
        EqN[AXIS_RA].value=ra;
        EqN[AXIS_DE].value=dec;
        lastEqState = EqNP.s;
        IDSetNumber(&EqNP, NULL);
    }

}

bool INDI::Telescope::Sync(double ra,double dec)
{
    //  if we get here, our mount doesn't support sync
    DEBUG(Logger::DBG_ERROR, "Telescope does not support Sync.");
    return false;
}

bool INDI::Telescope::MoveNS(INDI_DIR_NS dir, TelescopeMotionCommand command)
{
    INDI_UNUSED(dir);
    INDI_UNUSED(command);
    DEBUG(Logger::DBG_ERROR, "Telescope does not support North/South motion.");
    IUResetSwitch(&MovementNSSP);
    MovementNSSP.s = IPS_IDLE;
    IDSetSwitch(&MovementNSSP, NULL);
    return false;
}

bool INDI::Telescope::MoveWE(INDI_DIR_WE dir, TelescopeMotionCommand command)
{
    INDI_UNUSED(dir);
    INDI_UNUSED(command);
    DEBUG(Logger::DBG_ERROR,"Telescope does not support West/East motion.");
    IUResetSwitch(&MovementWESP);
    MovementWESP.s = IPS_IDLE;
    IDSetSwitch(&MovementWESP, NULL);
    return false;
}

/**************************************************************************************
** Process Text properties
***************************************************************************************/
bool INDI::Telescope::ISNewText (const char *dev, const char *name, char *texts[], char *names[], int n)
{
    //  first check if it's for our device
    if(!strcmp(dev,getDeviceName()))
    {

        if(!strcmp(name,PortTP.name))
        {
            //  This is our port, so, lets process it
            int rc;
            PortTP.s=IPS_OK;
            rc=IUUpdateText(&PortTP,texts,names,n);
            //  Update client display
            IDSetText(&PortTP,NULL);
            //  We processed this one, so, tell the world we did it
            return true;
        }

      if(!strcmp(name,TimeTP.name))
      {
        int utcindex   = IUFindIndex("UTC", names, n);
        int offsetindex= IUFindIndex("OFFSET", names, n);

        return processTimeInfo(texts[utcindex], texts[offsetindex]);
      }

      if(!strcmp(name,ActiveDeviceTP.name))
      {
          ActiveDeviceTP.s=IPS_OK;
          IUUpdateText(&ActiveDeviceTP,texts,names,n);
          //  Update client display
          IDSetText(&ActiveDeviceTP,NULL);

          IDSnoopDevice(ActiveDeviceT[0].text,"GEOGRAPHIC_COORD");
          IDSnoopDevice(ActiveDeviceT[0].text,"TIME_UTC");
          return true;
      }
    }

    controller->ISNewText(dev, name, texts, names, n);

    return DefaultDevice::ISNewText(dev,name,texts,names,n);
}

/**************************************************************************************
**
***************************************************************************************/
bool INDI::Telescope::ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n)
{
    //  first check if it's for our device
    if(strcmp(dev,getDeviceName())==0)
    {
        if(strcmp(name,"EQUATORIAL_EOD_COORD")==0)
        {
            //  this is for us, and it is a goto
            bool rc=false;
            double ra=-1;
            double dec=-100;

            for (int x=0; x<n; x++)
            {

                //IDLog("request stuff %s %4.2f\n",names[x],values[x]);

                INumber *eqp = IUFindNumber (&EqNP, names[x]);
                if (eqp == &EqN[AXIS_RA])
                {
                    ra = values[x];
                } else if (eqp == &EqN[AXIS_DE])
                {
                    dec = values[x];
                }
            }
            if ((ra>=0)&&(ra<=24)&&(dec>=-90)&&(dec<=90))
            {
                // Check if it is already parked.
                if (capability.canPark)
                {
                    if (isParked())
                    {
                        DEBUG(INDI::Logger::DBG_WARNING, "Please unpark the mount before issuing any motion/sync commands.");
                        EqNP.s = lastEqState = IPS_IDLE;
                        IDSetNumber(&EqNP, NULL);
                        return false;
                    }
                }

                // Check if it can sync
                if (capability.canSync)
                {
                    ISwitch *sw;
                    sw=IUFindSwitch(&CoordSP,"SYNC");
                    if((sw != NULL)&&( sw->s==ISS_ON ))
                    {
                       rc=Sync(ra,dec);
                       if (rc)
                           EqNP .s = lastEqState = IPS_OK;
                       else
                           EqNP.s = lastEqState = IPS_ALERT;
                       IDSetNumber(&EqNP, NULL);
                       return rc;
                    }
                }

                // Issue GOTO
                rc=Goto(ra,dec);
                if (rc)
                    EqNP .s = lastEqState = IPS_BUSY;
                else
                    EqNP.s = lastEqState = IPS_ALERT;
                IDSetNumber(&EqNP, NULL);

            }
            return rc;
        }

        if(strcmp(name,"GEOGRAPHIC_COORD")==0)
        {
            int latindex = IUFindIndex("LAT", names, n);
            int longindex= IUFindIndex("LONG", names, n);
            int elevationindex = IUFindIndex("ELEV", names, n);

            if (latindex == -1 || longindex==-1 || elevationindex == -1)
            {
                LocationNP.s=IPS_ALERT;
                IDSetNumber(&LocationNP, "Location data missing or corrupted.");
            }

            double targetLat  = values[latindex];
            double targetLong = values[longindex];
            double targetElev = values[elevationindex];

            return processLocationInfo(targetLat, targetLong, targetElev);

        }

        if(strcmp(name,"TELESCOPE_INFO")==0)
        {
            ScopeParametersNP.s = IPS_OK;

            IUUpdateNumber(&ScopeParametersNP,values,names,n);
            IDSetNumber(&ScopeParametersNP,NULL);

            return true;
        }

      if(strcmp(name, ParkPositionNP.name) == 0)
      {
        IUUpdateNumber(&ParkPositionNP, values, names, n);
        ParkPositionNP.s = IPS_OK;

        Axis1ParkPosition = ParkPositionN[AXIS_RA].value;
        Axis2ParkPosition = ParkPositionN[AXIS_DE].value;

        IDSetNumber(&ParkPositionNP, NULL);
        return true;
      }

    }

    return DefaultDevice::ISNewNumber(dev,name,values,names,n);
}

/**************************************************************************************
**
***************************************************************************************/
bool INDI::Telescope::ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if(strcmp(dev,getDeviceName())==0)
    {
        //  This one is for us
        if(!strcmp(name,CoordSP.name))
        {
            //  client is telling us what to do with co-ordinate requests
            CoordSP.s=IPS_OK;
            IUUpdateSwitch(&CoordSP,states,names,n);
            //  Update client display
            IDSetSwitch(&CoordSP, NULL);
            return true;
        }

        // Slew Rate
        if (!strcmp (name, SlewRateSP.name))
        {
          int preIndex = IUFindOnSwitchIndex(&SlewRateSP);
          IUUpdateSwitch(&SlewRateSP, states, names, n);
          int nowIndex = IUFindOnSwitchIndex(&SlewRateSP);
          if (SetSlewRate(nowIndex) == false)
          {
              IUResetSwitch(&SlewRateSP);
              SlewRateS[preIndex].s = ISS_ON;
              SlewRateSP.s = IPS_ALERT;
          }
          else
            SlewRateSP.s = IPS_OK;
          IDSetSwitch(&SlewRateSP, NULL);
          return true;
        }

        if(!strcmp(name,ParkSP.name))
        {
            if (TrackState == SCOPE_PARKING)
            {
                IUResetSwitch(&ParkSP);
                ParkSP.s == IPS_IDLE;
                Abort();
                DEBUG(INDI::Logger::DBG_SESSION, "Parking/Unparking aborted.");
                IDSetSwitch(&ParkSP, NULL);
                return true;
            }

            int preIndex = IUFindOnSwitchIndex(&ParkSP);
            IUUpdateSwitch(&ParkSP, states, names, n);

            bool toPark = (ParkS[0].s == ISS_ON);

            if (toPark == false && TrackState != SCOPE_PARKED)
            {
                IUResetSwitch(&ParkSP);
                ParkS[1].s = ISS_ON;
                ParkSP.s = IPS_IDLE;
                DEBUG(INDI::Logger::DBG_SESSION, "Telescope already unparked.");
                IDSetSwitch(&ParkSP, NULL);
                return true;
            }

            if (toPark && TrackState == SCOPE_PARKED)
            {
                IUResetSwitch(&ParkSP);
                ParkS[0].s = ISS_ON;
                ParkSP.s = IPS_IDLE;
                DEBUG(INDI::Logger::DBG_SESSION, "Telescope already parked.");
                IDSetSwitch(&ParkSP, NULL);
                return true;
            }

            IUResetSwitch(&ParkSP);
            bool rc =  toPark ? Park() : UnPark();
            if (rc)
            {
                if (TrackState == SCOPE_PARKING)
                {
                     ParkS[0].s = toPark ? ISS_ON : ISS_OFF;
                     ParkS[1].s = toPark ? ISS_OFF : ISS_ON;
                     ParkSP.s = IPS_BUSY;
                }
                else
                {
                    ParkS[0].s = toPark ? ISS_ON : ISS_OFF;
                    ParkS[1].s = toPark ? ISS_OFF : ISS_ON;
                    ParkSP.s = IPS_OK;
                }
            }
            else
            {
                ParkS[preIndex].s = ISS_ON;
                ParkSP.s = IPS_ALERT;
            }

            IDSetSwitch(&ParkSP, NULL);
            return true;
        }

        if(!strcmp(name,MovementNSSP.name))
        {            
            // Check if it is already parked.
            if (capability.canPark)
            {
                if (isParked())
                {
                    DEBUG(INDI::Logger::DBG_WARNING, "Please unpark the mount before issuing any motion/sync commands.");
                    MovementNSSP.s = IPS_IDLE;
                    IDSetSwitch(&MovementNSSP, NULL);
                    return false;
                }
            }

            IUUpdateSwitch(&MovementNSSP,states,names,n);

            int current_motion = IUFindOnSwitchIndex(&MovementNSSP);

            // if same move requested, return
            if ( MovementNSSP.s == IPS_BUSY && current_motion == last_ns_motion)
                return true;

            // Time to stop motion
            if (current_motion == -1 || (last_ns_motion != -1 && current_motion != last_ns_motion))
            {
                if (MoveNS(last_ns_motion == 0 ? DIRECTION_NORTH : DIRECTION_SOUTH, MOTION_STOP))
                {
                    IUResetSwitch(&MovementNSSP);
                    MovementNSSP.s = IPS_IDLE;
                    last_ns_motion = -1;
                }
                else
                    MovementNSSP.s = IPS_ALERT;
            }
            else
            {
                if (MoveNS(current_motion == 0 ? DIRECTION_NORTH : DIRECTION_SOUTH, MOTION_START))
                {
                    MovementNSSP.s = IPS_BUSY;
                    last_ns_motion = current_motion;
                }
                else
                {
                    IUResetSwitch(&MovementNSSP);
                    MovementNSSP.s = IPS_ALERT;
                    last_ns_motion = -1;
                }
            }

            IDSetSwitch(&MovementNSSP, NULL);

            return true;
        }

        if(!strcmp(name,MovementWESP.name))
        {
            // Check if it is already parked.
            if (capability.canPark)
            {
                if (isParked())
                {
                    DEBUG(INDI::Logger::DBG_WARNING, "Please unpark the mount before issuing any motion/sync commands.");
                    MovementWESP.s = IPS_IDLE;
                    IDSetSwitch(&MovementWESP, NULL);
                    return false;
                }
            }

            IUUpdateSwitch(&MovementWESP,states,names,n);

            int current_motion = IUFindOnSwitchIndex(&MovementWESP);

            // if same move requested, return
            if ( MovementWESP.s == IPS_BUSY && current_motion == last_we_motion)
                return true;

            // Time to stop motion
            if (current_motion == -1 || (last_we_motion != -1 && current_motion != last_we_motion))
            {
                if (MoveWE(last_we_motion == 0 ? DIRECTION_WEST : DIRECTION_EAST, MOTION_STOP))
                {
                    IUResetSwitch(&MovementWESP);
                    MovementWESP.s = IPS_IDLE;
                    last_we_motion = -1;
                }
                else
                    MovementWESP.s = IPS_ALERT;
            }
            else
            {
                if (MoveWE(current_motion == 0 ? DIRECTION_WEST : DIRECTION_EAST, MOTION_START))
                {
                    MovementWESP.s = IPS_BUSY;
                    last_we_motion = current_motion;
                }
                else
                {
                    IUResetSwitch(&MovementWESP);
                    MovementWESP.s = IPS_ALERT;
                    last_we_motion = -1;
                }
            }

            IDSetSwitch(&MovementWESP, NULL);

            return true;
        }

        if(!strcmp(name,AbortSP.name))
        {
            IUResetSwitch(&AbortSP);

            if (Abort())
            {
                AbortSP.s = IPS_OK;
                if (ParkSP.s == IPS_BUSY)
                {
                    ParkSP.s = IPS_IDLE;
                    IDSetSwitch(&ParkSP, NULL);
                }
                if (EqNP.s == IPS_BUSY)
                {
                    EqNP.s = lastEqState = IPS_IDLE;
                    IDSetNumber(&EqNP, NULL);
                }
                if (MovementWESP.s == IPS_BUSY)
                {
                    IUResetSwitch(&MovementWESP);
                    MovementWESP.s = IPS_IDLE;
                    IDSetSwitch(&MovementWESP, NULL);
                }

                if (MovementNSSP.s == IPS_BUSY)
                {
                    IUResetSwitch(&MovementNSSP);
                    MovementNSSP.s = IPS_IDLE;
                    IDSetSwitch(&MovementNSSP, NULL);
                }

                if (EqNP.s == IPS_BUSY)
                {
                    EqNP.s = lastEqState = IPS_IDLE;
                    IDSetNumber(&EqNP, NULL);
                }

                last_ns_motion=last_we_motion=-1;
                if (TrackState != SCOPE_PARKED)
                    TrackState = SCOPE_IDLE;
            }
            else
                AbortSP.s = IPS_ALERT;

            IDSetSwitch(&AbortSP, NULL);

            return true;
        }

      if (!strcmp(name, ParkOptionSP.name))
      {
        IUUpdateSwitch(&ParkOptionSP, states, names, n);
        ISwitch *sp = IUFindOnSwitch(&ParkOptionSP);
        if (!sp)
          return false;

        IUResetSwitch(&ParkOptionSP);

        if ( (TrackState != SCOPE_IDLE && TrackState != SCOPE_TRACKING) || MovementNSSP.s == IPS_BUSY || MovementWESP.s == IPS_BUSY)
        {
          DEBUG(INDI::Logger::DBG_SESSION, "Can not change park position while slewing or already parked...");
          ParkOptionSP.s=IPS_ALERT;
          IDSetSwitch(&ParkOptionSP, NULL);
          return false;
        }

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
            DEBUG(INDI::Logger::DBG_SESSION, "Saved Park Status/Position.");
          else
            DEBUG(INDI::Logger::DBG_WARNING, "Can not save Park Status/Position.");
        }

        ParkOptionSP.s = IPS_OK;
        IDSetSwitch(&ParkOptionSP, NULL);

        return true;
      }

      if (!strcmp(name, BaudRateSP.name))
      {
          IUUpdateSwitch(&BaudRateSP, states, names, n);
          BaudRateSP.s = IPS_OK;
          IDSetSwitch(&BaudRateSP, NULL);
          return true;
      }

    }

    controller->ISNewSwitch(dev, name, states, names, n);

    //  Nobody has claimed this, so, ignore it
    return DefaultDevice::ISNewSwitch(dev,name,states,names,n);
}


bool INDI::Telescope::Connect()
{
    bool rc=false;

    if(isConnected())
        return true;

    rc=Connect(PortT[0].text, atoi(IUFindOnSwitch(&BaudRateSP)->name));

    if(rc)
        SetTimer(POLLMS);
    return rc;
}


bool INDI::Telescope::Connect(const char *port, uint16_t baud)
{
    //  We want to connect to a port
    //  For now, we will assume it's a serial port
    int connectrc=0;
    char errorMsg[MAXRBUF];
    bool rc;

    DEBUGF(Logger::DBG_DEBUG, "INDI::Telescope connecting to %s",port);

    if ( (connectrc = tty_connect(port, baud, 8, 0, 1, &PortFD)) != TTY_OK)
    {
        tty_error_msg(connectrc, errorMsg, MAXRBUF);

        DEBUGF(Logger::DBG_ERROR,"Failed to connect to port %s. Error: %s", port, errorMsg);

        return false;

    }

    DEBUGF(Logger::DBG_DEBUG, "Port FD %d",PortFD);

    /* Test connection */
    rc=ReadScopeStatus();
    if(rc)
    {
        //  We got a valid scope status read
        DEBUG(Logger::DBG_SESSION,"Telescope is online.");
        return rc;
    }

    //  Ok, we didn't get a valid read
    //  So, we need to close our handle and send error messages
    tty_disconnect(PortFD);

    return false;
}

bool INDI::Telescope::Disconnect()
{
    DEBUG(Logger::DBG_DEBUG, "INDI::Telescope Disconnect\n");

    tty_disconnect(PortFD);
    DEBUG(Logger::DBG_SESSION,"Telescope is offline.");

    return true;
}


void INDI::Telescope::TimerHit()
{
    if(isConnected())
    {
        bool rc;

        rc=ReadScopeStatus();

        if(rc == false)
        {
            //  read was not good
            EqNP.s= lastEqState = IPS_ALERT;
            IDSetNumber(&EqNP, NULL);
        }

        SetTimer(POLLMS);
    }
}


bool INDI::Telescope::Park()
{
    DEBUG(INDI::Logger::DBG_WARNING, "Parking is not supported.");
    return false;
}

bool  INDI::Telescope::UnPark()
{
    DEBUG(INDI::Logger::DBG_WARNING, "UnParking is not supported.");
    return false;
}

void INDI::Telescope::SetCurrentPark()
{
    DEBUG(INDI::Logger::DBG_WARNING, "Parking is not supported.");
}

void INDI::Telescope::SetDefaultPark()
{
    DEBUG(INDI::Logger::DBG_WARNING, "Parking is not supported.");
}

bool INDI::Telescope::processTimeInfo(const char *utc, const char *offset)
{
    struct ln_date utc_date;
    double utc_offset=0;

    if (extractISOTime(utc, &utc_date) == -1)
    {
      TimeTP.s = IPS_ALERT;
      IDSetText(&TimeTP, "Date/Time is invalid: %s.", utc);
      return false;
    }

    utc_offset = atof(offset);

    if (updateTime(&utc_date, utc_offset))
    {
        IUSaveText(&TimeT[0], utc);
        IUSaveText(&TimeT[1], offset);
        TimeTP.s = IPS_OK;
        IDSetText(&TimeTP, NULL);
        return true;
    }
    else
    {
        TimeTP.s = IPS_ALERT;
        IDSetText(&TimeTP, NULL);
        return false;
    }
}

bool INDI::Telescope::processLocationInfo(double latitude, double longitude, double elevation)
{
    if (updateLocation(latitude, longitude, elevation))
    {
        LocationNP.s=IPS_OK;
        LocationN[LOCATION_LATITUDE].value  = latitude;
        LocationN[LOCATION_LONGITUDE].value = longitude;
        LocationN[LOCATION_ELEVATION].value = elevation;
        //  Update client display
        IDSetNumber(&LocationNP,NULL);

        return true;
    }
    else
    {
        LocationNP.s=IPS_ALERT;
        //  Update client display
        IDSetNumber(&LocationNP,NULL);

        return false;
    }
}

bool INDI::Telescope::updateTime(ln_date *utc, double utc_offset)
{
    INDI_UNUSED(utc);
    INDI_UNUSED(utc_offset);

    return true;
}

bool INDI::Telescope::updateLocation(double latitude, double longitude, double elevation)
{
    INDI_UNUSED(latitude);
    INDI_UNUSED(longitude);
    INDI_UNUSED(elevation);

    return true;
}

void INDI::Telescope::SetTelescopeCapability(TelescopeCapability *cap)
{
    capability.canPark      = cap->canPark;
    capability.canSync      = cap->canSync;
    capability.canAbort     = cap->canAbort;
    capability.hasTime      = cap->hasTime;
    capability.hasLocation  = cap->hasLocation;
    capability.nSlewRate    = cap->nSlewRate;

    if (capability.canSync)
        IUFillSwitchVector(&CoordSP,CoordS,3,getDeviceName(),"ON_COORD_SET","On Set",MAIN_CONTROL_TAB,IP_RW,ISR_1OFMANY,60,IPS_IDLE);
    else
        IUFillSwitchVector(&CoordSP,CoordS,2,getDeviceName(),"ON_COORD_SET","On Set",MAIN_CONTROL_TAB,IP_RW,ISR_1OFMANY,60,IPS_IDLE);

    if (capability.nSlewRate >= 4)
    {
        free(SlewRateS);
        SlewRateS = (ISwitch *) malloc(sizeof(ISwitch) * capability.nSlewRate);
        int step = capability.nSlewRate / 4;
        for (int i=0; i < capability.nSlewRate; i++)
        {
            char name[4];
            snprintf(name, 4, "%dx", i+1);
            IUFillSwitch(SlewRateS+i, name, name, ISS_OFF);
        }

        strncpy( (SlewRateS+(step*0))->name, "SLEW_GUIDE", MAXINDINAME);
        strncpy( (SlewRateS+(step*1))->name, "SLEW_CENTERING", MAXINDINAME);
        strncpy( (SlewRateS+(step*2))->name, "SLEW_FIND", MAXINDINAME);
        strncpy( (SlewRateS+(capability.nSlewRate-1))->name, "SLEW_MAX", MAXINDINAME);

        // By Default we set current Slew Rate to 0.5 of max
        (SlewRateS+(capability.nSlewRate/2))->s = ISS_ON;

        IUFillSwitchVector(&SlewRateSP, SlewRateS, capability.nSlewRate, getDeviceName(), "TELESCOPE_SLEW_RATE", "Slew Rate", MOTION_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);
    }
}

void INDI::Telescope::SetParkDataType(TelescopeParkData type)
{
    parkDataType = type;

    if (parkDataType != PARK_NONE)
    {
        switch (parkDataType)
        {
            case PARK_RA_DEC:
            IUFillNumber(&ParkPositionN[AXIS_RA],"PARK_RA","RA (hh:mm:ss)","%010.6m",0,24,0,0);
            IUFillNumber(&ParkPositionN[AXIS_DE],"PARK_DEC","DEC (dd:mm:ss)","%010.6m",-90,90,0,0);
            break;

            case PARK_AZ_ALT:
            IUFillNumber(&ParkPositionN[AXIS_AZ],"PARK_AZ","AZ D:M:S", "%10.6m", 0.0, 360.0, 0.0, 0);
            IUFillNumber(&ParkPositionN[AXIS_ALT],"PARK_ALT", "Alt  D:M:S", "%10.6m", -90., 90.0, 0.0, 0);
            break;

            case PARK_RA_DEC_ENCODER:
            IUFillNumber(&ParkPositionN[AXIS_RA],"PARK_RA" ,"RA Encoder","%.0f" ,0,16777215,1,0);
            IUFillNumber(&ParkPositionN[AXIS_DE],"PARK_DEC","DEC Encoder","%.0f",0,16777215,1,0);
            break;

            case PARK_AZ_ALT_ENCODER:
            IUFillNumber(&ParkPositionN[AXIS_RA],"PARK_AZ" ,"AZ Encoder","%.0f" ,0,16777215,1,0);
            IUFillNumber(&ParkPositionN[AXIS_DE],"PARK_ALT","ALT Encoder","%.0f",0,16777215,1,0);
            break;

        default:
            break;
        }

        IUFillNumberVector(&ParkPositionNP,ParkPositionN,2,getDeviceName(),"TELESCOPE_PARK_POSITION","Park Position", SITE_TAB,IP_RW,60,IPS_IDLE);
    }
}

void INDI::Telescope::SetParked(bool isparked)
{
  IsParked=isparked;
  IUResetSwitch(&ParkSP);
  if (IsParked)
  {
      ParkSP.s = IPS_OK;
      ParkS[0].s = ISS_ON;
      TrackState = SCOPE_PARKED;
      DEBUG(INDI::Logger::DBG_SESSION, "Mount is parked.");
  }
  else
  {
      ParkSP.s=IPS_IDLE;
      ParkS[1].s = ISS_ON;
      TrackState = SCOPE_IDLE;
      DEBUG(INDI::Logger::DBG_SESSION, "Mount is unparked.");
  }

  IDSetSwitch(&ParkSP, NULL);

  if (parkDataType != PARK_NONE)
    WriteParkData();
}

bool INDI::Telescope::isParked()
{
  return IsParked;
}

bool INDI::Telescope::InitPark()
{
  char *loadres;
  loadres=LoadParkData();
  if (loadres)
  {
    DEBUGF(INDI::Logger::DBG_SESSION, "InitPark: No Park data in file %s: %s", Parkdatafile, loadres);
    SetParked(false);
    return false;
  }

  SetParked(isParked());

  ParkPositionN[AXIS_RA].value = Axis1ParkPosition;
  ParkPositionN[AXIS_DE].value = Axis2ParkPosition;
  IDSetNumber(&ParkPositionNP, NULL);

  return true;
}

char *INDI::Telescope::LoadParkData()
{
  wordexp_t wexp;
  FILE *fp;
  LilXML *lp;
  static char errmsg[512];

  XMLEle *parkxml;
  XMLAtt *ap;
  bool devicefound=false;

  ParkDeviceName = getDeviceName();
  ParkstatusXml=NULL;
  ParkdeviceXml=NULL;
  ParkpositionXml = NULL;
  ParkpositionAxis1Xml = NULL;
  ParkpositionAxis2Xml = NULL;

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

  ParkdeviceXml=parkxml;
  ParkstatusXml = findXMLEle(parkxml, "parkstatus");
  ParkpositionXml = findXMLEle(parkxml, "parkposition");
  ParkpositionAxis1Xml = findXMLEle(ParkpositionXml, "axis1position");
  ParkpositionAxis2Xml = findXMLEle(ParkpositionXml, "axis2position");
  IsParked=false;

  if (ParkstatusXml == NULL || ParkpositionAxis1Xml == NULL || ParkpositionAxis2Xml == NULL)
  {
      return (char *)("Park data invalid or missing.");
  }

  if (!strcmp(pcdataXMLEle(ParkstatusXml), "true"))
      IsParked=true; 

  sscanf(pcdataXMLEle(ParkpositionAxis1Xml), "%lf", &Axis1ParkPosition);
  sscanf(pcdataXMLEle(ParkpositionAxis2Xml), "%lf", &Axis2ParkPosition);

  return NULL;
}

bool INDI::Telescope::WriteParkData()
{
  wordexp_t wexp;
  FILE *fp;
  char pcdata[30];
  ParkDeviceName = getDeviceName();

  if (wordexp(Parkdatafile, &wexp, 0))
  {
    wordfree(&wexp);
    DEBUGF(INDI::Logger::DBG_SESSION, "WriteParkData: can not write file %s: Badly formed filename.", Parkdatafile);
    return false;
  }

  if (!(fp=fopen(wexp.we_wordv[0], "w")))
  {
    wordfree(&wexp);
    DEBUGF(INDI::Logger::DBG_SESSION, "WriteParkData: can not write file %s: %s", Parkdatafile, strerror(errno));
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
  if (!ParkpositionXml)
      ParkpositionXml=addXMLEle(ParkdeviceXml, "parkposition");
  if (!ParkpositionAxis1Xml)
      ParkpositionAxis1Xml=addXMLEle(ParkpositionXml, "axis1position");
  if (!ParkpositionAxis2Xml)
      ParkpositionAxis2Xml=addXMLEle(ParkpositionXml, "axis2position");

  editXMLEle(ParkstatusXml, (IsParked?"true":"false"));

  snprintf(pcdata, sizeof(pcdata), "%f", Axis1ParkPosition);
  editXMLEle(ParkpositionAxis1Xml, pcdata);
  snprintf(pcdata, sizeof(pcdata), "%f", Axis2ParkPosition);
  editXMLEle(ParkpositionAxis2Xml, pcdata);

  prXMLEle(fp, ParkdataXmlRoot, 0);
  fclose(fp);

  return true;
}

double INDI::Telescope::GetAxis1Park()
{
  return Axis1ParkPosition;
}
double INDI::Telescope::GetAxis1ParkDefault()
{
  return Axis1DefaultParkPosition;
}
double INDI::Telescope::GetAxis2Park()
{
  return Axis2ParkPosition;
}
double INDI::Telescope::GetAxis2ParkDefault()
{
  return Axis2DefaultParkPosition;
}

void INDI::Telescope::SetAxis1Park(double value)
{
  Axis1ParkPosition=value;
  ParkPositionN[AXIS_RA].value = value;
  IDSetNumber(&ParkPositionNP, NULL);
}

void INDI::Telescope::SetAxis1ParkDefault(double value)
{
  Axis1DefaultParkPosition=value;
}

void INDI::Telescope::SetAxis2Park(double value)
{
  Axis2ParkPosition=value;
  ParkPositionN[AXIS_DE].value = value;
  IDSetNumber(&ParkPositionNP, NULL);
}

void INDI::Telescope::SetAxis2ParkDefault(double value)
{
  Axis2DefaultParkPosition=value;
}

bool INDI::Telescope::SetSlewRate(int index)
{
    INDI_UNUSED(index);
    return true;
}

void INDI::Telescope::processButton(const char *button_n, ISState state)
{
    //ignore OFF
    if (state == ISS_OFF)
        return;

    if (!strcmp(button_n, "ABORTBUTTON"))
    {
        // Only abort if we have some sort of motion going on
        if (MovementNSSP.s == IPS_BUSY || MovementWESP.s == IPS_BUSY || EqNP.s == IPS_BUSY)
        {

            Abort();
        }
    }
    else if (!strcmp(button_n, "PARKBUTTON"))
    {
        ISState states[2] = { ISS_ON, ISS_OFF };
        char *names[2] = { ParkS[0].name, ParkS[1].name };
        ISNewSwitch(getDeviceName(), ParkSP.name, states, names, 2);
    }
    else if (!strcmp(button_n, "UNPARKBUTTON"))
    {
        ISState states[2] = { ISS_OFF, ISS_ON };
        char *names[2] = { ParkS[0].name, ParkS[1].name };
        ISNewSwitch(getDeviceName(), ParkSP.name, states, names, 2);
    }
}

void INDI::Telescope::processJoystick(const char * joystick_n, double mag, double angle)
{
    if (!strcmp(joystick_n, "MOTIONDIR"))
        processNSWE(mag, angle);
    else if (!strcmp(joystick_n, "SLEWPRESET"))
        processSlewPresets(mag, angle);
}

void INDI::Telescope::processNSWE(double mag, double angle)
{
    if (mag < 0.5)
    {
        // Moving in the same direction will make it stop
        if (MovementNSSP.s == IPS_BUSY)
        {
            if (MoveNS( MovementNSSP.sp[0].s == ISS_ON ? DIRECTION_NORTH : DIRECTION_SOUTH, MOTION_STOP))
            {
                IUResetSwitch(&MovementNSSP);
                MovementNSSP.s = IPS_IDLE;
                IDSetSwitch(&MovementNSSP, NULL);
            }
            else
            {
                MovementNSSP.s = IPS_ALERT;
                IDSetSwitch(&MovementNSSP, NULL);
            }
        }

        if (MovementWESP.s == IPS_BUSY)
        {
            if (MoveWE( MovementWESP.sp[0].s == ISS_ON ? DIRECTION_WEST : DIRECTION_EAST, MOTION_STOP))
            {
                IUResetSwitch(&MovementWESP);
                MovementWESP.s = IPS_IDLE;
                IDSetSwitch(&MovementWESP, NULL);
            }
            else
            {
                MovementWESP.s = IPS_ALERT;
                IDSetSwitch(&MovementWESP, NULL);
            }
        }
    }
    // Put high threshold
    else if (mag > 0.9)
    {
        // North
        if (angle > 0 && angle < 180)
        {
            // Don't try to move if you're busy and moving in the same direction
            if (MovementNSSP.s != IPS_BUSY || MovementNSS[0].s != ISS_ON)
                MoveNS(DIRECTION_NORTH, MOTION_START);

            // If angle is close to 90, make it exactly 90 to reduce noise that could trigger east/west motion as well
            if (angle > 80 && angle < 110)
                angle = 90;

            MovementNSSP.s = IPS_BUSY;
            MovementNSSP.sp[DIRECTION_NORTH].s = ISS_ON;
            MovementNSSP.sp[DIRECTION_SOUTH].s = ISS_OFF;
            IDSetSwitch(&MovementNSSP, NULL);
        }
        // South
        if (angle > 180 && angle < 360)
        {
            // Don't try to move if you're busy and moving in the same direction
           if (MovementNSSP.s != IPS_BUSY  || MovementNSS[1].s != ISS_ON)
            MoveNS(DIRECTION_SOUTH, MOTION_START);

           // If angle is close to 270, make it exactly 270 to reduce noise that could trigger east/west motion as well
           if (angle > 260 && angle < 280)
               angle = 270;

            MovementNSSP.s = IPS_BUSY;
            MovementNSSP.sp[DIRECTION_NORTH].s = ISS_OFF;
            MovementNSSP.sp[DIRECTION_SOUTH].s = ISS_ON;
            IDSetSwitch(&MovementNSSP, NULL);
        }
        // East
        if (angle < 90 || angle > 270)
        {
            // Don't try to move if you're busy and moving in the same direction
           if (MovementWESP.s != IPS_BUSY  || MovementWES[1].s != ISS_ON)
                MoveWE(DIRECTION_EAST, MOTION_START);

           MovementWESP.s = IPS_BUSY;
           MovementWESP.sp[DIRECTION_WEST].s = ISS_OFF;
           MovementWESP.sp[DIRECTION_EAST].s = ISS_ON;
           IDSetSwitch(&MovementWESP, NULL);
        }

        // West
        if (angle > 90 && angle < 270)
        {

            // Don't try to move if you're busy and moving in the same direction
           if (MovementWESP.s != IPS_BUSY  || MovementWES[0].s != ISS_ON)
                MoveWE(DIRECTION_WEST, MOTION_START);

           MovementWESP.s = IPS_BUSY;
           MovementWESP.sp[DIRECTION_WEST].s = ISS_ON;
           MovementWESP.sp[DIRECTION_EAST].s = ISS_OFF;
           IDSetSwitch(&MovementWESP, NULL);
        }
    }
}

void INDI::Telescope::processSlewPresets(double mag, double angle)
{
    // high threshold, only 1 is accepted
    if (mag != 1)
        return;

    int currentIndex = IUFindOnSwitchIndex(&SlewRateSP);

    // Up
    if (angle > 0 && angle < 180)
    {
        if (currentIndex <= 0)
            return;

        IUResetSwitch(&SlewRateSP);
        SlewRateS[currentIndex-1].s = ISS_ON;
        SetSlewRate(currentIndex-1);
    }
    // Down
    else
    {
        if (currentIndex >= SlewRateSP.nsp-1)
            return;

         IUResetSwitch(&SlewRateSP);
         SlewRateS[currentIndex+1].s = ISS_ON;
         SetSlewRate(currentIndex-1);
    }

    IDSetSwitch(&SlewRateSP, NULL);
}

void INDI::Telescope::joystickHelper(const char * joystick_n, double mag, double angle, void *context)
{
    static_cast<INDI::Telescope*>(context)->processJoystick(joystick_n, mag, angle);
}

void INDI::Telescope::buttonHelper(const char * button_n, ISState state, void *context)
{
    static_cast<INDI::Telescope*>(context)->processButton(button_n, state);
}
