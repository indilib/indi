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

#include "inditelescope.h"
#include "indicom.h"

#define POLLMS 1000

INDI::Telescope::Telescope()
{
    //ctor

    last_we_motion = last_ns_motion = -1;
}

INDI::Telescope::~Telescope()
{

}

bool INDI::Telescope::initProperties()
{
    DefaultDevice::initProperties();

    IUFillNumber(&EqN[0],"RA","RA (hh:mm:ss)","%010.6m",0,24,0,0);
    IUFillNumber(&EqN[1],"DEC","DEC (dd:mm:ss)","%010.6m",-90,90,0,0);
    IUFillNumberVector(&EqNP,EqN,2,getDeviceName(),"EQUATORIAL_EOD_COORD","Eq. Coordinates",MAIN_CONTROL_TAB,IP_RW,60,IPS_IDLE);

    IUFillText(&TimeT[0],"UTC","UTC Time","");
    IUFillText(&TimeT[1],"OFFSET","UTC Offset","");
    IUFillTextVector(&TimeTP,TimeT,2,getDeviceName(),"TIME_UTC","UTC",SITE_TAB,IP_RW,60,IPS_IDLE);

    IUFillNumber(&LocationN[0],"LAT","Lat (dd:mm:ss)","%010.6m",-90,90,0,0.0);
    IUFillNumber(&LocationN[1],"LONG","Lon (dd:mm:ss)","%010.6m",0,360,0,0.0 );
    IUFillNumber(&LocationN[2],"ELEV","Elevation (m)","%g",-200,10000,0,0 );
    IUFillNumberVector(&LocationNP,LocationN,3,getDeviceName(),"GEOGRAPHIC_COORD","Scope Location",SITE_TAB,IP_RW,60,IPS_OK);

    IUFillSwitch(&CoordS[0],"TRACK","Track",ISS_OFF);
    IUFillSwitch(&CoordS[1],"SLEW","Slew",ISS_OFF);
    IUFillSwitch(&CoordS[2],"SYNC","Sync",ISS_OFF);

    if (canSync())
        IUFillSwitchVector(&CoordSP,CoordS,3,getDeviceName(),"ON_COORD_SET","On Set",MAIN_CONTROL_TAB,IP_RW,ISR_ATMOST1,60,IPS_IDLE);
    else
        IUFillSwitchVector(&CoordSP,CoordS,2,getDeviceName(),"ON_COORD_SET","On Set",MAIN_CONTROL_TAB,IP_RW,ISR_ATMOST1,60,IPS_IDLE);


    IUFillSwitch(&ParkS[0],"PARK","Park",ISS_OFF);
    IUFillSwitchVector(&ParkSP,ParkS,1,getDeviceName(),"TELESCOPE_PARK","Park",MAIN_CONTROL_TAB,IP_RW,ISR_ATMOST1,60,IPS_IDLE);

    IUFillSwitch(&AbortS[0],"ABORT","Abort",ISS_OFF);
    IUFillSwitchVector(&AbortSP,AbortS,1,getDeviceName(),"TELESCOPE_ABORT_MOTION","Abort Motion",MAIN_CONTROL_TAB,IP_RW,ISR_ATMOST1,60,IPS_IDLE);

    IUFillText(&PortT[0],"PORT","Port","/dev/ttyUSB0");
    IUFillTextVector(&PortTP,PortT,1,getDeviceName(),"DEVICE_PORT","Ports",OPTIONS_TAB,IP_RW,60,IPS_IDLE);

    IUFillSwitch(&MovementNSS[MOTION_NORTH], "MOTION_NORTH", "North", ISS_OFF);
    IUFillSwitch(&MovementNSS[MOTION_SOUTH], "MOTION_SOUTH", "South", ISS_OFF);
    IUFillSwitchVector(&MovementNSSP, MovementNSS, 2, getDeviceName(),"TELESCOPE_MOTION_NS", "North/South", MOTION_TAB, IP_RW, ISR_ATMOST1, 60, IPS_IDLE);

    IUFillSwitch(&MovementWES[MOTION_WEST], "MOTION_WEST", "West", ISS_OFF);
    IUFillSwitch(&MovementWES[MOTION_EAST], "MOTION_EAST", "East", ISS_OFF);
    IUFillSwitchVector(&MovementWESP, MovementWES, 2, getDeviceName(),"TELESCOPE_MOTION_WE", "West/East", MOTION_TAB, IP_RW, ISR_ATMOST1, 60, IPS_IDLE);

    IUFillNumber(&ScopeParametersN[0],"TELESCOPE_APERTURE","Aperture (mm)","%g",50,4000,0,0.0);
    IUFillNumber(&ScopeParametersN[1],"TELESCOPE_FOCAL_LENGTH","Focal Length (mm)","%g",100,10000,0,0.0 );
    IUFillNumber(&ScopeParametersN[2],"GUIDER_APERTURE","Guider Aperture (mm)","%g",50,4000,0,0.0);
    IUFillNumber(&ScopeParametersN[3],"GUIDER_FOCAL_LENGTH","Guider Focal Length (mm)","%g",100,10000,0,0.0 );
    IUFillNumberVector(&ScopeParametersNP,ScopeParametersN,4,getDeviceName(),"TELESCOPE_INFO","Scope Properties",OPTIONS_TAB,IP_RW,60,IPS_OK);

    TrackState=SCOPE_PARKED;

    return true;
}

void INDI::Telescope::ISGetProperties (const char *dev)
{
    //  First we let our parent populate
    DefaultDevice::ISGetProperties(dev);

    defineText(&PortTP);

    if(isConnected())
    {
        //  Now we add our telescope specific stuff
        defineSwitch(&CoordSP);
        defineNumber(&EqNP);
        defineSwitch(&AbortSP);
        defineText(&TimeTP);
        defineNumber(&LocationNP);
        defineSwitch(&ParkSP);
        defineSwitch(&MovementNSSP);
        defineSwitch(&MovementWESP);
        defineNumber(&ScopeParametersNP);

    }
    return;
}

bool INDI::Telescope::updateProperties()
{

    if(isConnected())
    {
        //  Now we add our telescope specific stuff
        defineSwitch(&CoordSP);
        defineNumber(&EqNP);
        defineSwitch(&AbortSP);
        defineSwitch(&MovementNSSP);
        defineSwitch(&MovementWESP);
        defineText(&TimeTP);
        defineNumber(&LocationNP);
        if (canPark())
            defineSwitch(&ParkSP);
        defineNumber(&ScopeParametersNP);

    }
    else
    {
        deleteProperty(CoordSP.name);
        deleteProperty(EqNP.name);
        deleteProperty(AbortSP.name);
        deleteProperty(MovementNSSP.name);
        deleteProperty(MovementWESP.name);
        deleteProperty(TimeTP.name);
        deleteProperty(LocationNP.name);
        if (canPark())
            deleteProperty(ParkSP.name);
        deleteProperty(ScopeParametersNP.name);
    }

    return true;
}

bool INDI::Telescope::ISSnoopDevice(XMLEle *root)
{
    return INDI::DefaultDevice::ISSnoopDevice(root);
}

bool INDI::Telescope::saveConfigItems(FILE *fp)
{

    IUSaveConfigText(fp, &PortTP);
    IUSaveConfigNumber(fp,&LocationNP);
    IUSaveConfigNumber(fp, &ScopeParametersNP);

    return true;
}

void INDI::Telescope::NewRaDec(double ra,double dec)
{
    //  Lets set our eq values to these numbers
    //  which came from the hardware
    static int last_state=-1;

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

    //IDLog("newRA DEC RA %g - DEC %g --- EqN[0] %g --- EqN[1] %g --- EqN.state %d\n", ra, dec, EqN[0].value, EqN[1].value, EqNP.s);
    if (EqN[0].value != ra || EqN[1].value != dec || EqNP.s != last_state)
    {
        EqN[0].value=ra;
        EqN[1].value=dec;
        last_state = EqNP.s;
        IDSetNumber(&EqNP, NULL);
    }

}

bool INDI::Telescope::Sync(double ra,double dec)
{
    //  if we get here, our mount doesn't support sync
    DEBUG(Logger::DBG_ERROR, "Mount does not support Sync.");
    return false;
}

bool INDI::Telescope::MoveNS(TelescopeMotionNS dir, TelescopeMotionCommand command)
{
    INDI_UNUSED(dir);
    INDI_UNUSED(command);
    DEBUG(Logger::DBG_ERROR, "Mount does not support North/South motion.");
    IUResetSwitch(&MovementNSSP);
    MovementNSSP.s = IPS_IDLE;
    IDSetSwitch(&MovementNSSP, NULL);
    return false;
}

bool INDI::Telescope::MoveWE(TelescopeMotionWE dir, TelescopeMotionCommand command)
{
    INDI_UNUSED(dir);
    INDI_UNUSED(command);
    DEBUG(Logger::DBG_ERROR,"Mount does not support West/East motion.");
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
    if(strcmp(dev,getDeviceName())==0)
    {
        //  This is for our device
        //  Now lets see if it's something we process here
        if(strcmp(name,PortTP.name)==0)
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

      if(strcmp(name,"TIME_UTC")==0)
      {
        int utcindex   = IUFindIndex("UTC", names, n);
        int offsetindex= IUFindIndex("OFFSET", names, n);
        struct ln_date utc;
        double utc_offset=0;

        if (extractISOTime(texts[utcindex], &utc) == -1)
        {
          TimeTP.s = IPS_ALERT;
          IDSetText(&TimeTP, "Date/Time is invalid: %s.", texts[utcindex]);
          return false;
        }

        utc_offset = atof(texts[offsetindex]);

        if (updateTime(&utc, utc_offset))
        {
            IUUpdateText(&TimeTP, texts, names, n);
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

   }

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
                if (eqp == &EqN[0])
                {
                    ra = values[x];
                } else if (eqp == &EqN[1])
                {
                    dec = values[x];
                }
            }
            if ((ra>=0)&&(ra<=24)&&(dec>=-90)&&(dec<=90))
            {
                //  we got an ra and a dec, both in range
                //  And now we let the underlying hardware specific class
                //  perform the goto
                //  Ok, lets see if we should be doing a goto
                //  or a sync
                if (canSync())
                {
                    ISwitch *sw;
                    sw=IUFindSwitch(&CoordSP,"SYNC");
                    if((sw != NULL)&&( sw->s==ISS_ON ))
                    {
                       rc=Sync(ra,dec);
                       if (rc)
                           CoordSP .s = IPS_OK;
                       else
                           CoordSP.s = IPS_ALERT;
                       IDSetSwitch(&CoordSP, NULL);
                       return rc;
                    }
                }

                //  Ensure we are not showing Parked status
                ParkSP.s=IPS_IDLE;
                IUResetSwitch(&ParkSP);
                rc=Goto(ra,dec);
                if (rc)
                    CoordSP .s = IPS_OK;
                else
                    CoordSP.s = IPS_ALERT;
                IDSetSwitch(&CoordSP, NULL);

                //  Ok, now we have to put our switches back
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

            if (updateLocation(targetLat, targetLong, targetElev))
            {
                LocationNP.s=IPS_OK;
                IUUpdateNumber(&LocationNP,values,names,n);
                //  Update client display
                IDSetNumber(&LocationNP,NULL);
            }
            else
            {
                LocationNP.s=IPS_ALERT;
                //  Update client display
                IDSetNumber(&LocationNP,NULL);
            }
        }

        if(strcmp(name,"TELESCOPE_INFO")==0)
        {
            ScopeParametersNP.s = IPS_OK;

            IUUpdateNumber(&ScopeParametersNP,values,names,n);
            IDSetNumber(&ScopeParametersNP,NULL);

            saveConfig();

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
        if(strcmp(name,"ON_COORD_SET")==0)
        {
            //  client is telling us what to do with co-ordinate requests
            CoordSP.s=IPS_OK;
            IUUpdateSwitch(&CoordSP,states,names,n);
            //  Update client display
            IDSetSwitch(&CoordSP, NULL);
            return true;
        }

        if(strcmp(name,"TELESCOPE_PARK")==0)
        {
            Park();
            return true;
        }

        if(strcmp(name,"TELESCOPE_MOTION_NS")==0)
        {
            IUUpdateSwitch(&MovementNSSP,states,names,n);

            int current_motion = IUFindOnSwitchIndex(&MovementNSSP);

            // if same move requested, return
            if ( MovementNSSP.s == IPS_BUSY && current_motion == last_ns_motion)
                return true;

            // Time to stop motion
            if (current_motion == -1 || (last_ns_motion != -1 && current_motion != last_ns_motion))
            {
                if (MoveNS(last_ns_motion == 0 ? MOTION_NORTH : MOTION_SOUTH, MOTION_STOP))
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
                if (MoveNS(current_motion == 0 ? MOTION_NORTH : MOTION_SOUTH, MOTION_START))
                {
                    MovementNSSP.s = IPS_BUSY;
                    last_ns_motion = current_motion;
                }
                else
                    MovementNSSP.s = IPS_ALERT;
            }

            IDSetSwitch(&MovementNSSP, NULL);

            return true;
        }

        if(strcmp(name,"TELESCOPE_MOTION_WE")==0)
        {
            IUUpdateSwitch(&MovementWESP,states,names,n);

            int current_motion = IUFindOnSwitchIndex(&MovementWESP);

            // if same move requested, return
            if ( MovementWESP.s == IPS_BUSY && current_motion == last_we_motion)
                return true;

            // Time to stop motion
            if (current_motion == -1 || (last_we_motion != -1 && current_motion != last_we_motion))
            {
                if (MoveWE(last_we_motion == 0 ? MOTION_WEST : MOTION_EAST, MOTION_STOP))
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
                if (MoveWE(current_motion == 0 ? MOTION_WEST : MOTION_EAST, MOTION_START))
                {
                    MovementWESP.s = IPS_BUSY;
                    last_we_motion = current_motion;
                }
                else
                    MovementWESP.s = IPS_ALERT;
            }

            IDSetSwitch(&MovementWESP, NULL);

            return true;
        }

        if(strcmp(name,"TELESCOPE_ABORT_MOTION")==0)
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
                    EqNP.s = IPS_IDLE;
                    IDSetNumber(&EqNP, NULL);
                }
                if (MovementWESP.s == IPS_BUSY)
                {
                    MovementWESP.s = IPS_IDLE;
                    IDSetSwitch(&MovementWESP, NULL);
                }

                if (MovementNSSP.s == IPS_BUSY)
                {
                    MovementNSSP.s = IPS_IDLE;
                    IDSetSwitch(&MovementNSSP, NULL);
                }

                if (EqNP.s == IPS_BUSY)
                {
                    EqNP.s = IPS_IDLE;
                    IDSetNumber(&EqNP, NULL);
                }

                TrackState = SCOPE_IDLE;
            }
            else
                AbortSP.s = IPS_ALERT;

            IDSetSwitch(&AbortSP, NULL);

            return true;
        }

    }

    //  Nobody has claimed this, so, ignore it
    return DefaultDevice::ISNewSwitch(dev,name,states,names,n);
}


bool INDI::Telescope::Connect()
{
    //  Parent class is wanting a connection
    if (isDebug())
        IDLog("INDI::Telescope arrived in connect with %s\n",PortT[0].text);
    bool rc=false;

    if(isConnected()) return true;


    if (isDebug())
        IDLog("Telescope Calling Connect\n");

    rc=Connect(PortT[0].text);

    if (isDebug())
        IDLog("Telescope Connect returns %d\n",rc);

    if(rc)
        SetTimer(POLLMS);
    return rc;
}


bool INDI::Telescope::Connect(const char *port)
{
    //  We want to connect to a port
    //  For now, we will assume it's a serial port
    int connectrc=0;
    char errorMsg[MAXRBUF];
    bool rc;

    DEBUGF(Logger::DBG_DEBUG, "INDI::Telescope connecting to %s\n",port);

    if ( (connectrc = tty_connect(port, 9600, 8, 0, 1, &PortFD)) != TTY_OK)
    {
        tty_error_msg(connectrc, errorMsg, MAXRBUF);

        DEBUGF(Logger::DBG_ERROR,"Failed to connect to port %s. Error: %s", port, errorMsg);

        return false;

    }

    DEBUGF(Logger::DBG_DEBUG, "Port Fd %d\n",PortFD);

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
            EqNP.s=IPS_ALERT;
            IDSetNumber(&EqNP, NULL);
        }

        SetTimer(POLLMS);
    }
}


bool INDI::Telescope::Park()
{
    //  We want to park our telescope
    //  but the scope doesn't seem to support park
    //  or it wouldn't have gotten here
    return false;
}

bool INDI::Telescope::canSync()
{
    return false;
}

bool INDI::Telescope::canPark()
{
    return false;
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
