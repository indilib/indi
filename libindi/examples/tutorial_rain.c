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

static ILight rainL[]			= {{"Status", "", IPS_IDLE, 0, 0}};
static ILightVectorProperty rainLP	= { mydev, "Rain Alert", "", MAIN_GROUP, IPS_IDLE, rainL , NARRAY(rainL), "", 0};

static ISwitch rainS[]           	= {{"On", "", ISS_OFF, 0, 0}, {"Off", "", ISS_ON, 0, 0}};
static ISwitchVectorProperty rainSP	= { mydev, "Control Rain", "", MAIN_GROUP, IP_RW, ISR_1OFMANY, 0, IPS_IDLE, rainS , NARRAY(rainS), "", 0};

static ISwitch propS[]           	= {{"Define", "", ISS_OFF, 0, 0}, {"Delete", "", ISS_OFF, 0, 0}, {"Crash", "", ISS_OFF, 0, 0}};
static ISwitchVectorProperty propSP	= { mydev, "Property Test", "", MAIN_GROUP, IP_RW, ISR_1OFMANY, 0, IPS_IDLE, propS , NARRAY(propS), "", 0};

void ISGetProperties (const char *dev)
{ 
  /* MAIN_GROUP */
  IDDefSwitch(&PowerSP, NULL);
  IDDefLight(&rainLP, NULL);
  IDDefSwitch(&rainSP, NULL);
  IDDefSwitch(&propSP, NULL);
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
		IDSetSwitch(&PowerSP, "Rain Collector is online.");
	  else
	  {
		PowerSP.s = IPS_IDLE;
		IDSetSwitch(&PowerSP, "Rain Collector is offline.");
	  }
	  return;
	}

	/* rain */
	if (!strcmp (name, rainSP.name))
	{
		if (PowerSP.s != IPS_OK)
		{
			IDMessage(mydev, "The Rain Collector is offline!");
			return;
		}
		
	  if (IUUpdateSwitches(&rainSP, states, names, n) < 0)
		return;

	 rainSP.s = IPS_OK;
	 IDSetSwitch(&rainSP, "Rain status updated.");
	 
	 if (rainS[0].s == ISS_ON)
	 {
		 rainL[0].s = IPS_ALERT;
		 rainLP.s   = IPS_ALERT;
		 IDSetLight(&rainLP, "Alert! Alert! Rain detected!");
	 }
	 else
	 {
		 rainL[0].s = IPS_IDLE;
		 rainLP.s   = IPS_OK;
		 IDSetLight(&rainLP, "Rain threat passed. The skies are clear.");
	 }
	 
	   return;
	}
	
	/* Property control for observer test */
	if (!strcmp (name, propSP.name))
	{
		if (PowerSP.s != IPS_OK)
		{
			IDMessage(mydev, "The Rain Collector is offline!");
			return;
		}
		
		if (IUUpdateSwitches(&propSP, states, names, n) < 0)
			return;
		
		/* #1 Define */
		if (propS[0].s == ISS_ON)
			IDDefLight(&rainLP, "Defining rain Alert Light property.");
		/* #2 Delete */
		else if (propS[1].s == ISS_ON)
			IDDelete(mydev, rainLP.name, "Deleting Rain Alert light property.");
		/* The fun part, crashing. This will cause the driver the 'die' The observer should notice that */
		else
		{
			char *p = "byebye";
			*p = '0';
		}
		
		IUResetSwitches(&propSP);
		propSP.s = IPS_OK;
		IDSetSwitch(&propSP, NULL);
		
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

