/*******************************************************************************
  Copyright(c) 2010 Gerry Rozema. All rights reserved.

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

#include "synscanmount.h"
#include "indicom.h"

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

// We declare an auto pointer to Synscan.
std::auto_ptr<SynscanMount> synscan(0);

void ISPoll(void *p);

void ISInit()
{
   static int isInit =0;

   if (isInit == 1)
       return;

    isInit = 1;
    if(synscan.get() == 0) synscan.reset(new SynscanMount());
    //IEAddTimer(POLLMS, ISPoll, NULL);

}

void ISGetProperties(const char *dev)
{
        ISInit();
        synscan->ISGetProperties(dev);
}

void ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int num)
{
        ISInit();
        synscan->ISNewSwitch(dev, name, states, names, num);
}

void ISNewText(	const char *dev, const char *name, char *texts[], char *names[], int num)
{
        ISInit();
        synscan->ISNewText(dev, name, texts, names, num);
}

void ISNewNumber(const char *dev, const char *name, double values[], char *names[], int num)
{
        ISInit();
        synscan->ISNewNumber(dev, name, values, names, num);
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
    INDI_UNUSED(root);
}

SynscanMount::SynscanMount()
{
    //ctor
    TelescopeCapability cap;

    cap.canPark = true;
    cap.canSync = false;
    cap.canAbort = true;
    cap.nSlewRate=0;
    SetTelescopeCapability(&cap);
}

SynscanMount::~SynscanMount()
{
    //dtor
}

const char * SynscanMount::getDefaultName()
{
    return "SynScan";
}

/*bool SynscanMount::initProperties()
{
    //if (isDebug())
    //    IDLog("Synscan::init_properties\n");

    //setDeviceName("Synscan");
    INDI::Telescope::initProperties();

    return true;
}
void SynscanMount::ISGetProperties (const char *dev)
{
    //  First we let our parent class do it's thing
    INDI::Telescope::ISGetProperties(dev);

    //  Now we add anything that's specific to this telescope
    //  or we could just load from a skeleton file too
    return;
}*/

bool SynscanMount::ReadScopeStatus()
{
    char str[20];
    int bytesWritten, bytesRead;
    int numread;
    double ra,dec;
    long unsigned int n1,n2;

    //IDLog("SynScan Read Status\n");
    //tty_write(PortFD,(unsigned char *)"Ka",2, &bytesWritten);  //  test for an echo

    tty_write(PortFD,"Ka",2, &bytesWritten);  //  test for an echo
    tty_read(PortFD,str,2,2, &bytesRead);  //  Read 2 bytes of response
    if(str[1] != '#')
    {
        //  this is not a correct echo
        if (isDebug())
            IDLog("ReadStatus Echo Fail. %s\n", str);
        IDMessage(getDeviceName(),"Mount Not Responding");
        return false;
    }

    if(TrackState==SCOPE_SLEWING)
    {
        //  We have a slew in progress
        //  lets see if it's complete
        //  This only works for ra/dec goto commands
        //  The goto complete flag doesn't trip for ALT/AZ commands
        memset(str,0,3);
        tty_write(PortFD,"L",1, &bytesWritten);
        numread=tty_read(PortFD,str,2,3, &bytesRead);
        if(str[0]!=48)
        {
            //  Nothing to do here
        } else
        {
            if(TrackState==SCOPE_PARKING) TrackState=SCOPE_PARKED;
            else TrackState=SCOPE_TRACKING;
        }
    }
    if(TrackState==SCOPE_PARKING)
    {
        //  ok, lets try read where we are
        //  and see if we have reached the park position
        memset(str,0,20);
        tty_write(PortFD,"Z",1, &bytesWritten);
        numread=tty_read(PortFD,str,10,2, &bytesRead);
        //IDLog("PARK READ %s\n",str);
        if(strncmp((char *)str,"0000,4000",9)==0)
        {
            TrackState=SCOPE_PARKED;
            ParkSP.s=IPS_OK;
            IDSetSwitch(&ParkSP,NULL);
            IDMessage(getDeviceName(),"Telescope is Parked.");
        }

    }

    memset(str,0,20);
    tty_write(PortFD,"e",1, &bytesWritten);
    numread=tty_read(PortFD,str,18,1, &bytesRead);
    if (bytesRead!=18)
	//if(str[17] != '#')
    {
        IDLog("read status bytes didn't get a full read\n");
        return false;
    }

    if (isDebug())
        IDLog("Bytes read is (%s)\n", str);
    // bytes read is as expected
    //sscanf((char *)str,"%x",&n1);
    //sscanf((char *)&str[9],"%x",&n2);

    // JM read as unsigned long int, otherwise we get negative RA!
    sscanf(str, "%lx,%lx#", &n1, &n2);

    ra=(double)n1/0x100000000*24.0;
    dec=(double)n2/0x100000000*360.0;
    NewRaDec(ra,dec);
    return true;
}

bool SynscanMount::Goto(double ra,double dec)
{
    char str[20];
    int n1,n2;
    int numread, bytesWritten, bytesRead;

    //  not fleshed in yet
    tty_write(PortFD,"Ka",2, &bytesWritten);  //  test for an echo
    tty_read(PortFD,str,2,2, &bytesRead);  //  Read 2 bytes of response
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
    tty_write(PortFD,str,18, &bytesWritten);
    TrackState=SCOPE_SLEWING;
    numread=tty_read(PortFD,str,1,60, &bytesRead);
    if (bytesRead!=1||str[0]!='#')
    {
        if (isDebug())
            IDLog("Timeout waiting for scope to complete slewing.");
        return false;
    }

    return true;
}

bool SynscanMount::Park()
{
    char str[20];
    int numread, bytesWritten, bytesRead;


    memset(str,0,3);
    tty_write(PortFD,"Ka",2, &bytesWritten);  //  test for an echo
    tty_read(PortFD,str,2,2, &bytesRead);  //  Read 2 bytes of response
    if(str[1] != '#')
    {
        //  this is not a correct echo
        //  so we are not talking to a mount properly
        return false;
    }
    //  Now we stop tracking
    tty_write(PortFD,"T0",2, &bytesWritten);
    numread=tty_read(PortFD,str,1,60, &bytesRead);
    if (bytesRead!=1||str[0]!='#')
    {
        if (isDebug())
            IDLog("Timeout waiting for scope to stop tracking.");
        return false;
    }

    //sprintf((char *)str,"b%08X,%08X",0x0,0x40000000);
    tty_write(PortFD,"B0000,4000",10, &bytesWritten);
    numread=tty_read(PortFD,str,1,60, &bytesRead);
    if (bytesRead!=1||str[0]!='#')
    {
        if (isDebug())
            IDLog("Timeout waiting for scope to respond to park.");
        return false;
    }

    TrackState=SCOPE_PARKING;
    IDMessage(getDeviceName(),"Parking Telescope...");
    return true;
}

bool  SynscanMount::Abort()
{
    char str[20];
    int bytesWritten, bytesRead;

    // Hmmm twice only stops it
    tty_write(PortFD,"M",1, &bytesWritten);
    tty_read(PortFD,str,1,1, &bytesRead);  //  Read 1 bytes of response


    tty_write(PortFD,"M",1, &bytesWritten);
    tty_read(PortFD,str,1,1, &bytesRead);  //  Read 1 bytes of response

    return true;
}

