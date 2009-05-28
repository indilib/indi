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
#include "observer.h"

#define mydev           "Dome"

#define MAIN_GROUP	"Main"

void check_rain(const char *dev, const char *name, IDState driver_state, IPState *states, char *names[], int n);

/* Connect/Disconnect */
static ISwitch PowerS[]          	= {{"CONNECT" , "Connect" , ISS_OFF, 0, 0},{"DISCONNECT", "Disconnect", ISS_ON, 0, 0}};
static ISwitchVectorProperty PowerSP	= { mydev, "CONNECTION" , "Connection", MAIN_GROUP, IP_RW, ISR_1OFMANY, 60, IPS_IDLE, PowerS, NARRAY(PowerS), "", 0};

static ISwitch DomeS[]           	= {{"Open", "", ISS_ON, 0, 0}, {"Close", "", ISS_OFF, 0, 0}};
static ISwitchVectorProperty DomeSP	= { mydev, "Dome Status", "", MAIN_GROUP, IP_RW, ISR_1OFMANY, 0, IPS_IDLE, DomeS , NARRAY(DomeS), "", 0};

void ISGetProperties (const char *dev)
{ 
  /* MAIN_GROUP */
  IDDefSwitch(&PowerSP, NULL);
  IDDefSwitch(&DomeSP, NULL);

  IOSubscribeProperty("Rain", "Rain Alert", IPT_LIGHT, IDT_ALL, check_rain);
}

void ISNewBLOB (const char *dev, const char *name, int sizes[], char *blobs[], char *formats[], char *names[], int n)
{
  dev=dev;name=name;sizes=sizes;blobs=blobs;formats=formats;names=names;n=n;
}
  
void ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n)
{
	/* ignore if not ours */
	if (dev && strcmp (dev, mydev))
	    return;

	/* Connection */
	if (!strcmp (name, PowerSP.name))
	{
	  if (IUUpdateSwitches(&PowerSP, states, names, n) < 0)
		return;

   	  PowerSP.s = IPS_OK;

	  if (PowerS[0].s == ISS_ON)
		IDSetSwitch(&PowerSP, "Dome is online.");
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
	  
	  if (IUUpdateSwitches(&DomeSP, states, names, n) < 0)
		return;

	DomeSP.s = IPS_BUSY;

	if (DomeS[0].s == ISS_ON)
		IDSetSwitch(&DomeSP, "Dome is opening.");
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


void ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n)
{
       
}

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

/* This is the callback function called by INDI when a new state from the rain collector driver is recieved.
   Please note that the prototype of this function is specific for SWITCH properties. There are unique callback 
   prototypes defined for all property types in the INDI API */
void check_rain(const char *dev, const char *name, IDState driver_state, IPState *states, char *names[], int n)
{

	switch (driver_state)
	{
		case IDS_DEFINED:
			IDMessage(mydev, "Property %s was defined.", name);
			break;

		case IDS_UPDATED:
			IDMessage(mydev, "Property %s was updated. The rain collector alert is %s.", name, (states[0] == IPS_ALERT) ? "Alert!" : "Ok");
			
			/* Only do stuff if we're online */
			if (PowerSP.s == IPS_OK && states[0] == IPS_ALERT)
			{
				if (DomeS[0].s == ISS_ON)
					closeDome();
				else
					IDMessage(mydev, "Rain Alert Detected! Dome is already closed.");
			}
			
			break;

		case IDS_DELETED:
			IDMessage(mydev, "Property %s was deleted.", name);
			break;
			
		case IDS_DIED:
			IDMessage(mydev, "Property %s is dead along with its driver %s.", name, dev);
			break;
	}

}


