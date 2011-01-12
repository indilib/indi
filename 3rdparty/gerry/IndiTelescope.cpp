/*******************************************************************************
  Copyright(c) 2010 Gerry Rozema. All rights reserved.

  This program is free software; you can redistribute it and/or modify it
  under the terms of the GNU General Public License as published by the Free
  Software Foundation; either version 2 of the License, or (at your option)
  any later version.

  This program is distributed in the hope that it will be useful, but WITHOUT
  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
  more details.

  You should have received a copy of the GNU General Public License along with
  this program; if not, write to the Free Software Foundation, Inc., 59
  Temple Place - Suite 330, Boston, MA  02111-1307, USA.

  The full GNU General Public License is included in this distribution in the
  file called LICENSE.
*******************************************************************************/

#include "IndiTelescope.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <math.h>
#include <unistd.h>
#include <time.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <termios.h>


IndiTelescope::IndiTelescope()
{
    //ctor
}

IndiTelescope::~IndiTelescope()
{
    //dtor
}

int IndiTelescope::init_properties()
{
    IDLog("IndiTelescope::init_properties()\n");
    IndiDevice::init_properties();

    IDLog("IndiTelescope::init_properties() adding eq co-ordinates  MyDev=%s\n",deviceName());
    IUFillNumber(&EqN[0],"RA","Ra (hh:mm:ss)","%010.6m",0,24,0,0);
    IUFillNumber(&EqN[1],"DEC","Dec (dd:mm:ss)","%010.6m",-90,90,0,0);
    IUFillNumberVector(&EqNV,EqN,2,deviceName(),"EQUATORIAL_EOD_COORD","Eq. Coordinates","Main Control",IP_RW,60,IPS_IDLE);
    //IUFillNumberVector(&EqNV,EqN,2,deviceName(),"EQUATORIAL_EOD_COORD","Eq. Coordinates","Main Control",IP_RW,60,IPS_IDLE);

    IUFillNumber(&EqReqN[0],"RA","Ra (hh:mm:ss)","%010.6m",0,24,0,0);
    IUFillNumber(&EqReqN[1],"DEC","Dec (dd:mm:ss)","%010.6m",-90,90,0,0);
    IUFillNumberVector(&EqReqNV,EqReqN,2,deviceName(),"EQUATORIAL_EOD_COORD_REQUEST","Goto....","Controls",IP_WO,60,IPS_IDLE);

    IUFillNumber(&LocationN[0],"LAT","Lat (dd:mm:ss)","%010.6m",-90,90,0,48.433);
    IUFillNumber(&LocationN[1],"LONG","Lon (dd:mm:ss)","%010.6m",-180,360,0,-123.35 );
    IUFillNumberVector(&LocationNV,LocationN,2,deviceName(),"GEOGRAPHIC_COORD","Scope Location","Location",IP_RW,60,IPS_OK);

    IUFillSwitch(&CoordS[0],"TRACK","Track",ISS_OFF);
    IUFillSwitch(&CoordS[1],"SLEW","Slew",ISS_OFF);
    IUFillSwitch(&CoordS[2],"SYNC","Sync",ISS_OFF);
    IUFillSwitchVector(&CoordSV,CoordS,3,deviceName(),"ON_COORD_SET","On Set","Controls",IP_RW,ISR_1OFMANY,60,IPS_IDLE);

    IUFillSwitch(&ParkS[0],"PARK","Park",ISS_OFF);
    IUFillSwitchVector(&ParkSV,ParkS,1,deviceName(),"TELESCOPE_PARK","Park","Controls",IP_RW,ISR_1OFMANY,60,IPS_IDLE);

    IUFillText(&PortT[0],"PORT","Port","/dev/ttyUSB0");
    IUFillTextVector(&PortTV,PortT,1,deviceName(),"DEVICE_PORT","Ports","Options",IP_RW,60,IPS_IDLE);

    Connected=false;
    TrackState=PARKED;
    return 0;
}

void IndiTelescope::ISGetProperties (const char *dev)
{
    //  First we let our parent populate
    IDLog("IndiTelescope::ISGetProperties %s\n",dev);
    IndiDevice::ISGetProperties(dev);

    //  We may need the port set before we can connect
    IDDefText(&PortTV,NULL);

    if(Connected) {
    //  Now we add our telescope specific stuff
        IDDefSwitch(&CoordSV,NULL);
        IDDefNumber(&EqNV, NULL);
        IDDefNumber(&EqReqNV, NULL);
        IDDefNumber(&LocationNV,NULL);
        IDDefSwitch(&ParkSV,NULL);
    }
    return;
}

bool IndiTelescope::UpdateProperties()
{
    if(Connected) {
        //  Now we add our telescope specific stuff
        IDLog("indiTelescope adding properties\n");
        IDDefSwitch(&CoordSV,NULL);
        IDDefNumber(&EqNV, NULL);
        IDDefNumber(&EqReqNV, NULL);
        //IDDefText(&PortTV,NULL);
        IDDefNumber(&LocationNV,NULL);
        IDDefSwitch(&ParkSV,NULL);
    } else {
        IDLog("IndiTelescope deleting properties\n");
        DeleteProperty(CoordSV.name);
        DeleteProperty(EqNV.name);
        DeleteProperty(EqReqNV.name);
        //DeleteProperty(PortTV.name);
        DeleteProperty(LocationNV.name);
        DeleteProperty(ParkSV.name);

    }
    return true;
}

int IndiTelescope::NewRaDec(double ra,double dec)
{
    //  Lets set our eq values to these numbers
    //  which came from the hardware
    EqN[0].value=ra;
    EqN[1].value=dec;
    return 0;
}

bool IndiTelescope::ReadScopeStatus()
{
    //  Return an error, because we sholdn't get here
    return false;
}

bool IndiTelescope::Goto(double ra,double dec)
{
    //  if we get here, it's because our derived hardware class
    //  does not support goto
    return false;
}

bool IndiTelescope::Sync(double ra,double dec)
{
    //  if we get here, our mount doesn't support sync
    IDMessage(deviceName(),"Mount does not support Sync");
    return false;
}

/**************************************************************************************
** Process Text properties
***************************************************************************************/
bool IndiTelescope::ISNewText (const char *dev, const char *name, char *texts[], char *names[], int n)
{
    //  Ok, lets see if this is a property wer process
    IDLog("IndiTelescope got %d new text items name %s\n",n,name);
    //  first check if it's for our device
    if(strcmp(dev,deviceName())==0) {
        //  This is for our device
        //  Now lets see if it's something we process here
        if(strcmp(name,PortTV.name)==0) {
            //  This is our port, so, lets process it

            //  Some clients insist on sending a port
            //  and they may not be configured for the
            //  correct port
            //  If we are already connected
            //  and running, it makes absolutely no sense
            //  to accept a new port value
            //  so lets just lie to them and say
            //  we did this, but, dont actually change anything
            if(Connected) return true;

            int rc;
            IDLog("calling update text\n");
            PortTV.s=IPS_OK;
            rc=IUUpdateText(&PortTV,texts,names,n);
            IDLog("update text returns %d\n",rc);
            //  Update client display
            IDSetText(&PortTV,NULL);

            //  We processed this one, so, tell the world we did it
            return true;
        }

    }

    return IndiDevice::ISNewText(dev,name,texts,names,n);
}

/**************************************************************************************
**
***************************************************************************************/
bool IndiTelescope::ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n)
{
    //  first check if it's for our device
    IDLog("IndiTelescope::ISNewNumber %s\n",name);
    if(strcmp(dev,deviceName())==0) {
        //  This is for our device
        //  Now lets see if it's something we process here
        //  Cartes sends the REQUEST
        //  But KStars sends just the co-ordinates
        //if((strcmp(name,"EQUATORIAL_EOD_COORD_REQUEST")==0)||(strcmp(name,"EQUATORIAL_EOD_COORD")==0)) {
        if((strcmp(name,"EQUATORIAL_EOD_COORD_REQUEST")==0)||(strcmp(name,"EQUATORIAL_EOD_COORD")==0)) {
            //  this is for us, and it is a goto
            bool rc=false;
            double ra=-1;
            double dec=-100;

            for (int x=0; x<n; x++) {

                //IDLog("request stuff %s %4.2f\n",names[x],values[x]);

                INumber *eqp = IUFindNumber (&EqNV, names[x]);
                if (eqp == &EqN[0]) {
                    ra = values[x];
                } else if (eqp == &EqN[1]) {
                    dec = values[x];
                }
            }
            //IDLog("Got %4.2f %4.2f\n",ra,dec);
            if ((ra>=0)&&(ra<=24)&&(dec>=-90)&&(dec<=90)) {
                //  we got an ra and a dec, both in range
                //  And now we let the underlying hardware specific class
                //  perform the goto
                //  Ok, lets see if we should be doing a goto
                //  or a sync
                ISwitch *sw;
                sw=IUFindSwitch(&CoordSV,"SYNC");
                if((sw != NULL)&&( sw->s==ISS_ON )) {
                    //IDLog("Got a sync, we dont process yet\n");
                    rc=Sync(ra,dec);
                } else {
                    //  Ensure we are not showing Parked status
                    ParkSV.s=IPS_IDLE;
                    IDSetSwitch(&ParkSV,NULL);
                    rc=Goto(ra,dec);
                }
                //  Ok, now we have to put our switches back
            }
            return rc;
        }
        if(strcmp(name,"GEOGRAPHIC_COORD")==0) {
            //  Client wants to update the lat/long
            //  For now, we'll allow this, but, in future
            //  If we have lat/lon from gps, we'll prevent this
            //  from being updated
            LocationNV.s=IPS_OK;
            IUUpdateNumber(&LocationNV,values,names,n);
            //  Update client display
            IDSetNumber(&LocationNV,NULL);
        }
    }



    return IndiDevice::ISNewNumber(dev,name,values,names,n);
}

/**************************************************************************************
**
***************************************************************************************/
bool IndiTelescope::ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n)
{
    IDLog("Enter IsNewSwitch for %s\n",name);
    //for(int x=0; x<n; x++) {
    //    IDLog("Switch %s %d\n",names[x],states[x]);
    //}

    if(strcmp(dev,deviceName())==0) {
        //  This one is for us
        if(strcmp(name,"ON_COORD_SET")==0) {
            //  client is telling us what to do with co-ordinate requests
            CoordSV.s=IPS_OK;
            IUUpdateSwitch(&CoordSV,states,names,n);
            //  Update client display
            IDSetSwitch(&CoordSV,NULL);

            for(int x=0; x<n; x++) {


            }
            return true;
        }

        if(strcmp(name,"TELESCOPE_PARK")==0) {
            Park();

        }
    }

    //  Nobody has claimed this, so, ignore it
    return IndiDevice::ISNewSwitch(dev,name,states,names,n);
}

bool IndiTelescope::Connect()
{
    //  Parent class is wanting a connection
    IDLog("IndiTelescope calling connect with %s\n",PortT[0].text);

    if(Connected) return true;

    IDLog("Calling Connect\n");

    Connected=Connect(PortT[0].text);
    if(Connected) {
        SetTimer(1000);
    }
    return Connected;
}

bool IndiTelescope::Connect(char *port)
{
    //  We want to connect to a port
    //  For now, we will assume it's a serial port
    struct termios tty;
    bool rc;

    /* Make the connection */
    IDLog("Trying to open %s\n",port);
    PortFD = open(port,O_RDWR);
    if(PortFD == -1) {
        IDMessage(deviceName(),"Could not open port");
        return false;
    }

    tcgetattr(PortFD,&tty);
    cfsetospeed(&tty, (speed_t) B9600);
    cfsetispeed(&tty, (speed_t) B9600);
    tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS8;
    tty.c_iflag =  IGNBRK;
    tty.c_lflag = 0;
    tty.c_oflag = 0;
    tty.c_cflag |= CLOCAL | CREAD;
    tty.c_cc[VMIN] = 1;
    tty.c_cc[VTIME] = 5;
    tty.c_iflag &= ~(IXON|IXOFF|IXANY);
    tty.c_cflag &= ~(PARENB | PARODD);
    tcsetattr(PortFD, TCSANOW, &tty);

    /* Flush the input (read) buffer */

    tcflush(PortFD,TCIOFLUSH);

    /* Test connection */
    rc=ReadScopeStatus();
    if(rc) {
        //  We got a valid scope status read
        IDMessage(deviceName(),"Telescope is online.");
        return rc;
    }

    //  Ok, we didn't get a valid read
    //  So, we need to close our handle and send error messages
    close(PortFD);
    //IDMessage(deviceName(),"Didn't find a synscan mount on Serial Port");

    return false;
}

bool IndiTelescope::Disconnect()
{
    //  We dont actually close the serial connection
    //  because clients can screw each other up if we allow that
    //Connected=false;

    close(PortFD);
    IDMessage(deviceName(),"Telescope is offline.");

    return true;
}


void IndiTelescope::TimerHit()
{
    //IDLog("Timer Hit\n");
    if(Connected) {
        bool rc;

        rc=ReadScopeStatus();
        //IDLog("TrackState after read is %d\n",TrackState);
        if(rc) {
            //  read was good
            switch(TrackState) {
                case PARKED:
                EqNV.s=IPS_IDLE;
                break;

                case SLEWING:
                EqNV.s=IPS_BUSY;
                break;

                default:
                EqNV.s=IPS_OK;
                break;

            }
        } else {
            //  read was not good
            EqNV.s=IPS_ALERT;
        }
        //IDLog("IsConnected, re-arm timer\n");
        //  Now update the Client data
        IDSetNumber(&EqNV,NULL);
        SetTimer(1000);
    }
}

int IndiTelescope::writen(int fd, unsigned char* ptr, int nbytes)
{
    int nleft, nwritten;
    nleft = nbytes;
    while (nleft > 0) {
        nwritten = write (fd, ptr, nleft);
        if (nwritten <=0 ) break;
        nleft -= nwritten;
        ptr += nwritten;
    }
    return (nbytes - nleft);
}

int IndiTelescope::readn(int fd, unsigned char* ptr, int nbytes, int sec)
{
    int status;
    int nleft, nread;
    nleft = nbytes;
    while (nleft > 0) {
        status = portstat(fd,sec,0);
        if (status <=  0 ) break;
        nread  = read (fd, ptr, nleft);
        if (nread <= 0)  break;
        nleft -= nread;
        ptr += nread;
    }
    return (nbytes - nleft);
}

int IndiTelescope::portstat(int fd,int sec,int usec)
{
    int ret;
    int width;
    struct timeval timeout;
    fd_set readfds;

    memset((char *)&readfds,0,sizeof(readfds));
    FD_SET(fd, &readfds);
    width = fd+1;
    timeout.tv_sec = sec;
    timeout.tv_usec = usec;
    ret = select(width,&readfds,(fd_set *)0,(fd_set *)0,&timeout);
    return(ret);
}

bool IndiTelescope::Park()
{
    //  We want to park our telescope
    //  but the scope doesn't seem to support park
    //  or it wouldn't have gotten here
    return false;
}
