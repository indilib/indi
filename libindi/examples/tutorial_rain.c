#if 0
    Inter-driver communications tutorial - Rain Driver 
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

#define mydev           "Rain"

#define MAIN_GROUP	"Main"

/* Connect/Disconnect */
static ISwitch PowerS[]          	= {{"CONNECT" , "Connect" , ISS_OFF, 0, 0},{"DISCONNECT", "Disconnect", ISS_ON, 0, 0}};
static ISwitchVectorProperty PowerSP	= { mydev, "CONNECTION" , "Connection", MAIN_GROUP, IP_RW, ISR_1OFMANY, 60, IPS_IDLE, PowerS, NARRAY(PowerS), "", 0};

/* Rain Alert Property. The dome driver will listen to updates on this property. 
   The rain driver does not need to take any action to make this property available to other driver,
   the INDI framework takes care of handling inter-driver communication
*/
static ILight RainL[]			= {{"Status", "", IPS_IDLE, 0, 0}};
static ILightVectorProperty RainLP	= { mydev, "Rain Alert", "", MAIN_GROUP, IPS_IDLE, RainL , NARRAY(RainL), "", 0};

static ISwitch RainS[]           	= {{"On", "", ISS_OFF, 0, 0}, {"Off", "", ISS_ON, 0, 0}};
static ISwitchVectorProperty RainSP	= { mydev, "Control Rain", "", MAIN_GROUP, IP_RW, ISR_1OFMANY, 0, IPS_IDLE, RainS , NARRAY(RainS), "", 0};

static ISwitch DomeControlS[]          		= {{"Open", "", ISS_OFF, 0, 0}, {"Close", "", ISS_OFF, 0, 0}};
static ISwitchVectorProperty DomeControlSP	= { mydev, "Control Dome", "", MAIN_GROUP, IP_RW, ISR_1OFMANY, 0, IPS_IDLE, DomeControlS , NARRAY(DomeControlS), "", 0};

/* The property we're trying to fetch from the dome driver. It's a carbon copy of the property define in dome.c, we initilize its structure here like any
   other local property, but we don't define it to client, it's only used internally within the driver. 

   MAKE SURE to set the property's device name to the device we're snooping on! If you copy/paste the property definition from the snooped driver
   don't forget to remove "mydev" and replace it with the driver name.
*/

/* Dome Open/Close */
static ISwitch DomeS[]           	= {{"Open", "", ISS_ON, 0, 0}, {"Close", "", ISS_OFF, 0, 0}};
static ISwitchVectorProperty DomeSP	= { "Dome", "Dome Status", "", MAIN_GROUP, IP_RW, ISR_1OFMANY, 0, IPS_IDLE, DomeS , NARRAY(DomeS), "", 0};

void ISGetProperties (const char *dev)
{ 
  /* MAIN_GROUP */
  IDDefSwitch(&PowerSP, NULL);
  IDDefLight(&RainLP, NULL);
  IDDefSwitch(&RainSP, NULL);
  IDDefSwitch(&DomeControlSP, NULL);

  /* Let's listen for Dome Status property in the device Dome */
/*  IDSnoopDevice("Rain", "Dome Status");*/
}

void ISNewBLOB (const char *dev, const char *name, int sizes[], int blobsizes[], char *blobs[], char *formats[], char *names[], int n) {}

/* While we can listen to changes in Dome Status property as they come from the dome driver, we're not interested to know that information for now */
void ISSnoopDevice (XMLEle *root) {}
  
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
		IDSetSwitch(&PowerSP, "Rain Collector is online.");
	  else
	  {
		PowerSP.s = IPS_IDLE;
		IDSetSwitch(&PowerSP, "Rain Collector is offline.");
	  }
	  return;
	}

	/* rain */
	if (!strcmp (name, RainSP.name))
	{
		if (PowerSP.s != IPS_OK)
		{
			IDMessage(mydev, "The Rain Collector is offline!");
			return;
		}
		
	  if (IUUpdateSwitch(&RainSP, states, names, n) < 0)
		return;

	 RainSP.s = IPS_OK;
	 IDSetSwitch(&RainSP, "Rain status updated.");
	 
	 if (RainS[0].s == ISS_ON)
	 {
		 RainL[0].s = IPS_ALERT;
		 RainLP.s   = IPS_ALERT;
		 IDSetLight(&RainLP, "Alert! Alert! Rain detected!");
	 }
	 else
	 {
		 RainL[0].s = IPS_IDLE;
		 RainLP.s   = IPS_OK;
		 IDSetLight(&RainLP, "Rain threat passed. The skies are clear.");
	 }
	 
	   return;
	}

	/* Control Dome */
	if (!strcmp (name, DomeControlSP.name))
	{
	  if (IUUpdateSwitch(&DomeControlSP, states, names, n) < 0)
		return;

	   DomeControlSP.s = IPS_OK;

	   /* Update the local copy of Dome Status property then send it over to the dome driver */
	   DomeS[0].s = DomeControlS[0].s;
	   DomeS[1].s = DomeControlS[1].s;

	   IDSetSwitch(&DomeControlSP, "Sending control command over to Dome driver...");
	   IDNewSwitch(&DomeSP, NULL);

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

