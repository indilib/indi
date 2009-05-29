#if 0
    Inter-driver communications tutorial - Dome Driver
    Copyright (C) 2007 Jasem Mutlaq (mutlaqja@ikarustech.com)

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

#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <math.h>
#include <unistd.h>
#include <time.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <zlib.h>

#include "indidevapi.h"
#include "eventloop.h"
#include "indicom.h"

#define mydev           "Dome"

#define MAIN_GROUP	"Main"
#define SNOOP_GROUP     "Snooped"

void turnRainAlertOn();
void closeDome();

/* Connect/Disconnect */
static ISwitch PowerS[]          	= {{"CONNECT" , "Connect" , ISS_OFF, 0, 0},{"DISCONNECT", "Disconnect", ISS_ON, 0, 0}};
static ISwitchVectorProperty PowerSP	= { mydev, "CONNECTION" , "Connection", MAIN_GROUP, IP_RW, ISR_1OFMANY, 60, IPS_IDLE, PowerS, NARRAY(PowerS), "", 0};

/* Dome Open/Close */
static ISwitch DomeS[]           	= {{"Open", "", ISS_ON, 0, 0}, {"Close", "", ISS_OFF, 0, 0}};
static ISwitchVectorProperty DomeSP	= { mydev, "Dome Status", "", MAIN_GROUP, IP_RW, ISR_1OFMANY, 0, IPS_IDLE, DomeS , NARRAY(DomeS), "", 0};

/* The property we're trying to fetch from the rain driver. It's a carbon copy of the property define in rain.c, we initilize its structure here like any
   other local property, but we don't define it to client, it's only used internally within the driver. 

   MAKE SURE to set the property's device name to the device we're snooping on! If you copy/paste the property definition from the snooped driver
   don't forget to remove "mydev" and replace it with the driver name.
*/
static ILight RainL[]			= {{"Status", "", IPS_IDLE, 0, 0}};
static ILightVectorProperty RainLP	= { "Rain", "Rain Alert", "", SNOOP_GROUP, IPS_IDLE, RainL , NARRAY(RainL), "", 0};

void ISGetProperties (const char *dev)
{ 
  /* MAIN_GROUP */
  IDDefSwitch(&PowerSP, NULL);
  IDDefSwitch(&DomeSP, NULL);

  /* Let's listen for Rain Alert property in the device Rain */
  IDSnoopDevice("Rain", "Rain Alert");

}

void ISNewBLOB (const char *dev, const char *name, int sizes[], int blobsizes[], char *blobs[], char *formats[], char *names[], int n) {}

void ISSnoopDevice (XMLEle *root)
{

  IPState old_state = RainL[0].s;

  /* If the "Rain Alert" property gets updated in the Rain device, we will receive a notification. We need to process the new values of Rain Alert and update the local version
     of the property.*/
  if (IUSnoopLight(root, &RainLP) == 0)
  {

    // If the dome is connected and rain is Alert */
    if (PowerSP.s == IPS_OK && RainL[0].s == IPS_ALERT)
    {
	// If dome is open, then close it */
	if (DomeS[0].s == ISS_ON)
		closeDome();
	else
		IDMessage(mydev, "Rain Alert Detected! Dome is already closed.");
    }
    else if (old_state == IPS_ALERT && RainL[0].s != IPS_ALERT)
		IDMessage(mydev, "Rain threat passed. Opening the dome is now safe.");

  }

}

void ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n)
{
	/* ignore if not ours */
	if (dev && strcmp (dev, mydev))
	    return;

	/* Connection */
	if (!strcmp (name, PowerSP.name))
	{
	  if (IUUpdateSwitch(&PowerSP, states, names, n) < 0)
		return;

   	  PowerSP.s = IPS_OK;

	  if (PowerS[0].s == ISS_ON)
	  {
		IDSetSwitch(&PowerSP, "Dome is online.");

		/* Check if Rain Alert is already on upon connecting */
		if (RainL[0].s == IPS_ALERT && DomeS[0].s == ISS_ON)
			closeDome();
	  }
	else
	{
		PowerSP.s = IPS_IDLE;
		IDSetSwitch(&PowerSP, "Dome is offline.");
	}

	  return;
	}

	/* Dome */
	if (!strcmp (name, DomeSP.name))
	{
	
	  if (PowerSP.s != IPS_OK)
	  {
		  IDMessage(mydev, "Dome is offline!");
		  return;
	  }
	  
	  if (IUUpdateSwitch(&DomeSP, states, names, n) < 0)
		return;

	DomeSP.s = IPS_BUSY;

	if (DomeS[0].s == ISS_ON)
	{
		if (RainL[0].s == IPS_ALERT)
		{
			DomeSP.s = IPS_ALERT;
			DomeS[0].s = ISS_OFF;
			DomeS[1].s = ISS_ON;
			IDSetSwitch(&DomeSP, "It is raining, cannot open dome.");
			return;
		}
				
		IDSetSwitch(&DomeSP, "Dome is opening.");
	}
	else
		IDSetSwitch(&DomeSP, "Dome is closing.");

	sleep(5);

	 DomeSP.s = IPS_OK;

	if (DomeS[0].s == ISS_ON)
		IDSetSwitch(&DomeSP, "Dome is open.");
	else
		IDSetSwitch(&DomeSP, "Dome is closed.");

	  return;
	}
     
}

void ISNewText (const char *dev, const char *name, char *texts[], char *names[], int n)
{
       /* ignore if not ours */ 
       if (dev && strcmp (mydev, dev))
         return;

	/* suppress warning */
	n=n; dev=dev; name=name; names=names; texts=texts;
}


void ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n) {}

void closeDome()
{
	DomeSP.s = IPS_BUSY;

	IDSetSwitch(&DomeSP, "Rain Alert! Dome is closing...");

	sleep(5);

	DomeS[0].s = ISS_OFF;
	DomeS[1].s = ISS_ON;
	
	DomeSP.s = IPS_OK;

	IDSetSwitch(&DomeSP, "Dome is closed.");
	
	
}

