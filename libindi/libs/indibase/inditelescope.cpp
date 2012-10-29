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

#include "inditelescope.h"
#include "libs/indicom.h"

#define POLLMS 1000

INDI::Telescope::Telescope()
{
    //ctor

}

INDI::Telescope::~Telescope()
{

}

bool INDI::Telescope::initProperties()
{
    DefaultDevice::initProperties();

    IUFillNumber(&EqN[0],"RA","RA (hh:mm:ss)","%010.6m",0,24,0,0);
    IUFillNumber(&EqN[1],"DEC","DEC (dd:mm:ss)","%010.6m",-90,90,0,0);
    IUFillNumberVector(&EqNV,EqN,2,getDeviceName(),"EQUATORIAL_EOD_COORD","Eq. Coordinates",MAIN_CONTROL_TAB,IP_RO,60,IPS_IDLE);

    IUFillNumber(&EqReqN[0],"RA","RA (hh:mm:ss)","%010.6m",0,24,0,0);
    IUFillNumber(&EqReqN[1],"DEC","DEC (dd:mm:ss)","%010.6m",-90,90,0,0);
    IUFillNumberVector(&EqReqNV,EqReqN,2,getDeviceName(),"EQUATORIAL_EOD_COORD_REQUEST","GOTO",MAIN_CONTROL_TAB,IP_WO,60,IPS_IDLE);

    IUFillNumber(&LocationN[0],"LAT","Lat (dd:mm:ss)","%010.6m",-90,90,0,0.0);
    IUFillNumber(&LocationN[1],"LONG","Lon (dd:mm:ss)","%010.6m",-180,180,0,0.0 );
    IUFillNumberVector(&LocationNV,LocationN,2,getDeviceName(),"GEOGRAPHIC_COORD","Scope Location",SITE_TAB,IP_RW,60,IPS_OK);

    IUFillSwitch(&CoordS[0],"TRACK","Track",ISS_OFF);
    IUFillSwitch(&CoordS[1],"SLEW","Slew",ISS_OFF);
    IUFillSwitch(&CoordS[2],"SYNC","Sync",ISS_OFF);

    if (canSync())
        IUFillSwitchVector(&CoordSV,CoordS,3,getDeviceName(),"ON_COORD_SET","On Set",MAIN_CONTROL_TAB,IP_RW,ISR_1OFMANY,60,IPS_IDLE);
    else
        IUFillSwitchVector(&CoordSV,CoordS,2,getDeviceName(),"ON_COORD_SET","On Set",MAIN_CONTROL_TAB,IP_RW,ISR_1OFMANY,60,IPS_IDLE);

    IUFillSwitch(&ConfigS[0], "CONFIG_LOAD", "Load", ISS_OFF);
    IUFillSwitch(&ConfigS[1], "CONFIG_SAVE", "Save", ISS_OFF);
    IUFillSwitch(&ConfigS[2], "CONFIG_DEFAULT", "Default", ISS_OFF);
    IUFillSwitchVector(&ConfigSV, ConfigS, 3, getDeviceName(), "CONFIG_PROCESS", "Configuration", "Options", IP_RW, ISR_1OFMANY, 60, IPS_IDLE);


    IUFillSwitch(&ParkS[0],"PARK","Park",ISS_OFF);
    IUFillSwitchVector(&ParkSV,ParkS,1,getDeviceName(),"TELESCOPE_PARK","Park",MAIN_CONTROL_TAB,IP_RW,ISR_1OFMANY,60,IPS_IDLE);

    IUFillSwitch(&AbortS[0],"ABORT","Abort",ISS_OFF);
    IUFillSwitchVector(&AbortSV,AbortS,1,getDeviceName(),"TELESCOPE_ABORT_MOTION","Abort Motion",MAIN_CONTROL_TAB,IP_RW,ISR_1OFMANY,60,IPS_IDLE);

    IUFillText(&PortT[0],"PORT","Port","/dev/ttyUSB0");
    IUFillTextVector(&PortTP,PortT,1,getDeviceName(),"DEVICE_PORT","Ports",OPTIONS_TAB,IP_RW,60,IPS_IDLE);

    IUFillSwitch(&MovementNSS[MOTION_NORTH], "MOTION_NORTH", "North", ISS_OFF);
    IUFillSwitch(&MovementNSS[MOTION_SOUTH], "MOTION_SOUTH", "South", ISS_OFF);
    IUFillSwitchVector(&MovementNSSP, MovementNSS, 2, getDeviceName(),"TELESCOPE_MOTION_NS", "North/South", MOTION_TAB, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);

    IUFillSwitch(&MovementWES[MOTION_WEST], "MOTION_WEST", "West", ISS_OFF);
    IUFillSwitch(&MovementWES[MOTION_EAST], "MOTION_EAST", "East", ISS_OFF);
    IUFillSwitchVector(&MovementWESP, MovementWES, 2, getDeviceName(),"TELESCOPE_MOTION_WE", "West/East", MOTION_TAB, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);

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

    //  We may need the port set before we can connect
    IDDefText(&PortTP,NULL);

    if(isConnected())
    {
        //  Now we add our telescope specific stuff
        defineSwitch(&CoordSV);
        defineNumber(&EqNV);
        defineNumber(&EqReqNV);
        defineSwitch(&AbortSV);
        defineNumber(&LocationNV);
        defineSwitch(&ParkSV);
        defineSwitch(&MovementNSSP);
        defineSwitch(&MovementWESP);
        defineSwitch(&ConfigSV);
        defineNumber(&ScopeParametersNP);

    }
    return;
}

bool INDI::Telescope::updateProperties()
{
    defineText(&PortTP);

    if(isConnected())
    {
        //  Now we add our telescope specific stuff
        defineSwitch(&CoordSV);
        defineNumber(&EqNV);
        defineNumber(&EqReqNV);
        defineSwitch(&AbortSV);
        defineSwitch(&MovementNSSP);
        defineSwitch(&MovementWESP);
        defineNumber(&LocationNV);
        if (canPark())
            defineSwitch(&ParkSV);
        defineNumber(&ScopeParametersNP);

    }
    else
    {
        deleteProperty(CoordSV.name);
        deleteProperty(EqNV.name);
        deleteProperty(EqReqNV.name);
        deleteProperty(AbortSV.name);
        deleteProperty(MovementNSSP.name);
        deleteProperty(MovementWESP.name);
        deleteProperty(LocationNV.name);
        if (canPark())
            deleteProperty(ParkSV.name);
        deleteProperty(ScopeParametersNP.name);

    }

    defineSwitch(&ConfigSV);

    return true;
}

bool INDI::Telescope::saveConfigItems(FILE *fp)
{

    IUSaveConfigText(fp, &PortTP);
    IUSaveConfigNumber(fp,&LocationNV);
    IUSaveConfigNumber(fp, &ScopeParametersNP);

    return true;
}

void INDI::Telescope::NewRaDec(double ra,double dec)
{
    //  Lets set our eq values to these numbers
    //  which came from the hardware
    static int last_state=-1;

    if (TrackState == SCOPE_TRACKING && EqReqNV.s != IPS_OK)
    {
        EqReqNV.s = IPS_OK;
        IDSetNumber(&EqReqNV, NULL);
    }
    else if ( (TrackState == SCOPE_PARKED || TrackState == SCOPE_IDLE) && EqReqNV.s != IPS_IDLE)
    {
        EqReqNV.s = IPS_IDLE;
        IDSetNumber(&EqReqNV, NULL);
    }

    //IDLog("newRA DEC RA %g - DEC %g --- EqN[0] %g --- EqN[1] %g --- EqN.state %d\n", ra, dec, EqN[0].value, EqN[1].value, EqNV.s);
    if (EqN[0].value != ra || EqN[1].value != dec || EqNV.s != last_state)
    {
      //  IDLog("Not equal , send update to client...\n");
        EqN[0].value=ra;
        EqN[1].value=dec;
        last_state = EqNV.s;
        IDSetNumber(&EqNV, NULL);
    }

}

bool INDI::Telescope::ReadScopeStatus()
{
    //  Return an error, because we sholdn't get here
    return false;
}

bool INDI::Telescope::Goto(double ra,double dec)
{
    //  if we get here, it's because our derived hardware class
    //  does not support goto
    return false;
}

bool INDI::Telescope::Abort()
{
    //  if we get here, it's because our derived hardware class
    //  does not support abort
    return false;
}

bool INDI::Telescope::Sync(double ra,double dec)
{
    //  if we get here, our mount doesn't support sync
    IDMessage(getDeviceName(),"Mount does not support Sync.");
    return false;
}

bool INDI::Telescope::MoveNS(TelescopeMotionNS dir)
{
    IDMessage(getDeviceName(),"Mount does not support North/South motion.");
    IUResetSwitch(&MovementNSSP);
    MovementNSSP.s = IPS_IDLE;
    IDSetSwitch(&MovementNSSP, NULL);
    return false;
}

bool INDI::Telescope::MoveWE(TelescopeMotionWE dir)
{
    IDMessage(getDeviceName(),"Mount does not support West/East motion.");
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
        if(strcmp(name,"EQUATORIAL_EOD_COORD_REQUEST")==0)
        {
            //  this is for us, and it is a goto
            bool rc=false;
            double ra=-1;
            double dec=-100;

            for (int x=0; x<n; x++)
            {

                //IDLog("request stuff %s %4.2f\n",names[x],values[x]);

                INumber *eqp = IUFindNumber (&EqNV, names[x]);
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
                ISwitch *sw;
                sw=IUFindSwitch(&CoordSV,"SYNC");
                if((sw != NULL)&&( sw->s==ISS_ON ))
                {
                    //IDLog("Got a sync, we dont process yet\n");
                    rc=Sync(ra,dec);
                } else {
                    //  Ensure we are not showing Parked status
                    ParkSV.s=IPS_IDLE;
                    IUResetSwitch(&ParkSV);
                    rc=Goto(ra,dec);
                }
                //  Ok, now we have to put our switches back
            }
            return rc;
        }

        if(strcmp(name,"GEOGRAPHIC_COORD")==0)
        {
            //  Client wants to update the lat/long
            //  For now, we'll allow this, but, in future
            //  If we have lat/lon from gps, we'll prevent this
            //  from being updated
            LocationNV.s=IPS_OK;
            IUUpdateNumber(&LocationNV,values,names,n);
            //  Update client display
            IDSetNumber(&LocationNV,NULL);
        }

        if(strcmp(name,"TELESCOPE_INFO")==0)
        {
            ScopeParametersNP.s = IPS_OK;

            IUUpdateNumber(&ScopeParametersNP,values,names,n);
            IDSetNumber(&ScopeParametersNP,NULL);

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
            CoordSV.s=IPS_OK;
            IUUpdateSwitch(&CoordSV,states,names,n);
            //  Update client display
            IDSetSwitch(&CoordSV, NULL);
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

            MovementNSSP.s = IPS_BUSY;

            if (MovementNSS[MOTION_NORTH].s == ISS_ON)
                MoveNS(MOTION_NORTH);
            else
                MoveNS(MOTION_SOUTH);

            return true;
        }

        if(strcmp(name,"TELESCOPE_MOTION_WE")==0)
        {
            IUUpdateSwitch(&MovementWESP,states,names,n);

            MovementWESP.s = IPS_BUSY;

            if (MovementWES[MOTION_WEST].s == ISS_ON)
                MoveWE(MOTION_WEST);
            else
                MoveWE(MOTION_EAST);

            return true;
        }

        if(strcmp(name,"TELESCOPE_ABORT_MOTION")==0)
        {
            IUResetSwitch(&AbortSV);

            if (Abort())
            {
                AbortSV.s = IPS_OK;
                if (ParkSV.s == IPS_BUSY)
                {
                    ParkSV.s = IPS_IDLE;
                    IDSetSwitch(&ParkSV, NULL);
                }
                if (EqNV.s == IPS_BUSY)
                {
                    EqNV.s = IPS_IDLE;
                    IDSetNumber(&EqNV, NULL);
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

                if (EqReqNV.s == IPS_BUSY)
                {
                    EqReqNV.s = IPS_IDLE;
                    IDSetNumber(&EqReqNV, NULL);
                }
            }
            else
                AbortSV.s = IPS_ALERT;

            IDSetSwitch(&AbortSV, NULL);

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

    if (isDebug())
        IDLog("INDI::Telescope connecting to %s\n",port);

    if ( (connectrc = tty_connect(port, 9600, 8, 0, 1, &PortFD)) != TTY_OK)
    {
        tty_error_msg(connectrc, errorMsg, MAXRBUF);

        //if (isDebug())
            IDLog("Failed to connect o port %s. Error: %s", port, errorMsg);
        IDMessage(getDeviceName(), "Failed to connect to port %s. Error: %s", port, errorMsg);

        return false;

    }

    if (isDebug())
        IDLog("Port Fd %d\n",PortFD);


    /* Test connection */
    rc=ReadScopeStatus();
    if(rc)
    {
        //  We got a valid scope status read
        IDMessage(getDeviceName(),"Telescope is online.");
        return rc;
    }

    //  Ok, we didn't get a valid read
    //  So, we need to close our handle and send error messages
    tty_disconnect(PortFD);

    return false;
}

bool INDI::Telescope::Disconnect()
{
    //  We dont actually close the serial connection
    //  because clients can screw each other up if we allow that
    //Connected=false;

    if (isDebug())
        IDLog("INDI::Telescope Disconnect\n");

    tty_disconnect(PortFD);
    IDMessage(getDeviceName(),"Telescope is offline.");

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
            EqNV.s=IPS_ALERT;
            IDSetNumber(&EqNV, NULL);
        }      
        else EqNV.s=IPS_OK;

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
