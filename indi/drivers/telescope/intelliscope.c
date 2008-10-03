#if 0
    Intelliscope INDI driver
    Copyright (C) 2005 Douglas Philipson (dougp AT intermind DOT net)

    Based on code by Jasem Mutlaq (mutlaqja@ikarustech.com)

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
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA

#endif

#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <math.h>
#include <unistd.h>
#include <time.h>

#include "indidevapi.h"
#include "indicom.h"
#include "lx200driver.h"

#define mydev		"Intelliscope"
#define BASIC_GROUP	"Main Control"
#define POLLMS		1000
#define currentRA	eq[0].value
#define currentDEC	eq[1].value

static void ISPoll(void *);
static void ISInit(void);
static void connectTelescope(void);

int fd;

static ISwitch PowerS[]          = {{"CONNECT" , "Connect" , ISS_OFF, 0, 0},{"DISCONNECT", "Disconnect", ISS_ON, 0, 0}};
ISwitchVectorProperty PowerSP		= { mydev, "CONNECTION" , "Connection", BASIC_GROUP, IP_RW, ISR_1OFMANY, 0, IPS_IDLE, PowerS, NARRAY(PowerS), "", 0};

static IText PortT[]			= {{"PORT", "Port", 0, 0, 0, 0}};
static ITextVectorProperty PortTP		= { mydev, "DEVICE_PORT", "Ports", BASIC_GROUP, IP_RW, 0, IPS_IDLE, PortT, NARRAY(PortT), "", 0};

/* equatorial position */
INumber eq[] = {
    {"RA",  "RA  H:M:S", "%10.6m",  0., 24., 0., 0., 0, 0, 0},
    {"DEC", "Dec D:M:S", "%10.6m", -90., 90., 0., 0., 0, 0, 0},
};
INumberVectorProperty eqNum = {
    mydev, "EQUATORIAL_EOD_COORD", "Equatorial JNow", BASIC_GROUP, IP_RO, 0, IPS_IDLE,
    eq, NARRAY(eq), "", 0};


void ISInit(void)
{
  static int isInit=0;

  if (isInit) return;

  isInit = 1;
  fd = -1;

  IEAddTimer (POLLMS, ISPoll, NULL);

}

void ISGetProperties (const char *dev)
{
  
  ISInit(); 

  dev=dev;

  IDDefSwitch(&PowerSP, NULL);
  IDDefText(&PortTP, NULL);
  IDDefNumber(&eqNum, NULL);
}

void ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n)
{
   ISInit();

   dev = dev;

   if (!strcmp(name, PowerSP.name))
   {
      IUResetSwitch(&PowerSP);
      IUUpdateSwitch(&PowerSP, states, names, n);
      connectTelescope();
      return;
   }
}

void ISNewText (const char *dev, const char *name, char *texts[], char *names[], int n)
{

  ISInit();

  dev=dev; names=names; n=n;

  if (!strcmp(name, PortTP.name))
  {
    IUSaveText(&PortT[0], texts[0]);
    PortTP.s = IPS_OK;
    IDSetText(&PortTP, NULL);
    return;
  }

}
void ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n)
{
 dev=dev;name=name;values=values;names=names;n=n;
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

void ISPoll (void *p)
{
  p=p;

  if (PowerS[0].s == ISS_ON)
  {
  	switch (eqNum.s)
		{
		case IPS_IDLE:
	        case IPS_OK:
	        case IPS_BUSY:
		if (updateIntelliscopeCoord(fd, &currentRA, &currentDEC) < 0)
                 {
                   eqNum.s = IPS_ALERT;
                   IDSetNumber(&eqNum, "Unknown error while reading telescope coordinates");
                   IDLog("Unknown error while reading telescope coordinates\n");
                   break;
                 }

	        IDSetNumber(&eqNum, NULL);
	        break;
	        
	        case IPS_ALERT:
	    	break;
		}
  }

  IEAddTimer(POLLMS, ISPoll, NULL);
}

void connectTelescope(void)
{

  switch (PowerS[0].s)
  {
    case ISS_ON:
     if (tty_connect(PortT[0].text, 9600, 8, 0, 1, &fd) != TTY_OK)
     {
       PowerSP.s = IPS_ALERT;
       IUResetSwitch(&PowerSP);
       IDSetSwitch(&PowerSP, "Error connecting to port %s", PortT[0].text);
       return;
     }

     PowerSP.s = IPS_OK;
     IDSetSwitch(&PowerSP, "Intelliscope is online.");
     break;

   case ISS_OFF:
	tty_disconnect(fd);
	IUResetSwitch(&PowerSP);
        eqNum.s = PortTP.s = PowerSP.s = IPS_IDLE;
	IDSetSwitch(&PowerSP, "Intelliscope is offline.");
        IDSetText(&PortTP, NULL);
	IDSetNumber(&eqNum, NULL);
	break;
  }

}

