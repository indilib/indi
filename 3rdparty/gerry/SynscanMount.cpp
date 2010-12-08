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
#include "SynscanMount.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <math.h>
#include <unistd.h>
#include <time.h>
#include <memory>

#include <string.h>
#include <sys/stat.h>

IndiDevice * _create_device()
{
    IndiDevice *mount;
    IDLog("Create a synscan mount\n");
    mount=new SynscanMount();
    return mount;
}


SynscanMount::SynscanMount()
{
    //ctor
}

SynscanMount::~SynscanMount()
{
    //dtor
}

char * SynscanMount::getDefaultName()
{
    return (char *)"SynScan";
}

int SynscanMount::init_properties()
{
    IDLog("Synscan::init_properties\n");

    //setDeviceName("Synscan");
    IndiTelescope::init_properties();
    //addConfigurationControl();

    return 0;
}
void SynscanMount::ISGetProperties (const char *dev)
{
    IDLog("Enter SynscanMount::ISGetProperties %s\n",dev);
    //  First we let our parent class do it's thing
    IndiTelescope::ISGetProperties(dev);

    //  Now we add anything that's specific to this telescope
    //  or we could just load from a skeleton file too
    return;
}

bool SynscanMount::ReadScopeStatus()
{
    unsigned char str[20];
    int numread;
    double ra,dec;
    int n1,n2;

    //IDLog("Read Status\n");
    writen(PortFD,(unsigned char *)"Ka",2);  //  test for an echo
    readn(PortFD,str,2,2);  //  Read 2 bytes of response
    if(str[1] != '#') {
        //  this is not a correct echo
        IDLog("ReadStatus Echo Fail\n");
        IDMessage(deviceName(),"Mount Not Responding");
        return false;
    }

    if(TrackState==SLEWING) {
        //  We have a slew in progress
        //  lets see if it's complete
        //  This only works for ra/dec goto commands
        //  The goto complete flag doesn't trip for ALT/AZ commands
        memset(str,0,3);
        writen(PortFD,(unsigned char*)"L",1);
        numread=readn(PortFD,str,2,3);
        if(str[0]!=48) {
            //  Nothing to do here
        } else {
            if(TrackState==PARKING) TrackState=PARKED;
            else TrackState=TRACKING;
        }
    }
    if(TrackState==PARKING) {
        //  ok, lets try read where we are
        //  and see if we have reached the park position
        memset(str,0,20);
        writen(PortFD,(unsigned char *)"Z",1);
        numread=readn(PortFD,str,10,2);
        //IDLog("PARK READ %s\n",str);
        if(strncmp((char *)str,"0000,4000",9)==0) {
            TrackState=PARKED;
            ParkSV.s=IPS_OK;
            IDSetSwitch(&ParkSV,NULL);
            IDMessage(deviceName(),"Telescope is Parked.");
        }

    }

    memset(str,0,20);
    writen(PortFD,(unsigned char*)"e",1);
    numread=readn(PortFD,str,18,1);
    if (numread!=18) {
        IDLog("read status bytes didn't get a full read\n");
        return false;
    }
    // bytes read is as expected
    sscanf((char *)str,"%x",&n1);
    sscanf((char *)&str[9],"%x",&n2);
    ra=(double)n1/0x100000000*24.0;
    dec=(double)n2/0x100000000*360.0;
    NewRaDec(ra,dec);
    return true;
}

bool SynscanMount::Goto(double ra,double dec)
{
    unsigned char str[20];
    int n1,n2;
    int numread;
    //  not fleshed in yet
    writen(PortFD,(unsigned char *)"Ka",2);  //  test for an echo
    readn(PortFD,str,2,2);  //  Read 2 bytes of response
    if(str[1] != '#') {
        //  this is not a correct echo
        //  so we are not talking to a mount properly
        return false;
    }
    //  Ok, mount is alive and well
    //  so, lets format up a goto command
    n1=ra*0x1000000/24;
    n2=dec*0x1000000/360;
    n1=n1<<8;
    n2=n2<<8;
    sprintf((char *)str,"r%08X,%08X",n1,n2);
    writen(PortFD,str,18);
    TrackState=SLEWING;
    numread=readn(PortFD,str,1,60);
    if (numread!=1||str[0]!='#') {
        IDLog("Timeout waiting for scope to complete slewing.");
        return false;
    }

    return true;
}

bool SynscanMount::Park()
{
    unsigned char str[20];
    int numread;

    memset(str,0,3);
    writen(PortFD,(unsigned char *)"Ka",2);  //  test for an echo
    readn(PortFD,str,2,2);  //  Read 2 bytes of response
    if(str[1] != '#') {
        //  this is not a correct echo
        //  so we are not talking to a mount properly
        return false;
    }
    //  Now we stop tracking
    writen(PortFD,(unsigned char *)"T0",2);
    numread=readn(PortFD,str,1,60);
    if (numread!=1||str[0]!='#') {
        IDLog("Timeout waiting for scope to stop tracking.");
        return false;
    }
    //sprintf((char *)str,"b%08X,%08X",0x0,0x40000000);
    writen(PortFD,(unsigned char *)"B0000,4000",10);
    numread=readn(PortFD,str,1,60);
    if (numread!=1||str[0]!='#') {
        IDLog("Timeout waiting for scope to respond to park.");
        return false;
    }
    TrackState=PARKING;
    IDMessage(deviceName(),"Parking Telescope...");
    return true;
}
