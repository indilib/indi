/*
    LX200 16"
    Copyright (C) 2003 Jasem Mutlaq (mutlaqja@ikarustech.com)

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

*/

#include "lx200_16.h"
#include "lx200driver.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <math.h>

#define LX16GROUP	"GPS/16 inch Features"

#define currentAZ	HorizontalCoordsRNP.np[0].value
#define currentALT	HorizontalCoordsRNP.np[1].value
#define targetAZ	HorizontalCoordsWNP.np[0].value
#define targetALT	HorizontalCoordsWNP.np[1].value

extern LX200Generic *telescope;
extern ITextVectorProperty Time;
extern int MaxReticleFlashRate;

static ISwitch FanStatusS[]		= { {"On", "", ISS_OFF, 0, 0}, {"Off", "", ISS_OFF, 0, 0}};
static ISwitch HomeSearchS[]		= { {"Save home", "", ISS_OFF, 0, 0} , {"Set home", "", ISS_OFF, 0, 0}};
static ISwitch FieldDeRotatorS[]	= { {"On", "", ISS_OFF, 0, 0}, {"Off", "", ISS_OFF,0 ,0}};

static ISwitchVectorProperty FanStatusSP	= { mydev, "Fan", "", LX16GROUP, IP_RW, ISR_1OFMANY, 0, IPS_IDLE, FanStatusS, NARRAY(FanStatusS), "", 0};

static ISwitchVectorProperty HomeSearchSP	= { mydev, "Park", "", LX16GROUP, IP_RW, ISR_1OFMANY, 0, IPS_IDLE, HomeSearchS, NARRAY(HomeSearchS), "", 0};

static ISwitchVectorProperty FieldDeRotatorSP	= { mydev, "Field De-rotator", "", LX16GROUP, IP_RW, ISR_1OFMANY, 0, IPS_IDLE, FieldDeRotatorS, NARRAY(FieldDeRotatorS), "", 0};

//static ISwitches SlewAltAzSw		= { mydev, "AltAzSet", "On Alt/Az Set",  SlewAltAzS, NARRAY(SlewAltAzS), ILS_IDLE, 0, LX16Group};

/* Horizontal Coordinates: Read Only */
static INumber HorizontalCoordsRN[] = {
    {"ALT",  "Alt  D:M:S", "%10.6m",  -90., 90., 0., 0., 0, 0, 0},
    {"AZ", "Az D:M:S", "%10.6m", 0., 360., 0., 0., 0, 0, 0}};
static INumberVectorProperty HorizontalCoordsRNP = {
    mydev, "HORIZONTAL_COORD", "Horizontal Coords", LX16GROUP, IP_RO, 120, IPS_IDLE,
    HorizontalCoordsRN, NARRAY(HorizontalCoordsRN), "", 0};

/* Horizontal Coordinates: Request Only */
static INumber HorizontalCoordsWN[] = {
    {"ALT",  "Alt  D:M:S", "%10.6m",  -90., 90., 0., 0., 0, 0, 0},
    {"AZ", "Az D:M:S", "%10.6m", 0., 360., 0., 0., 0, 0, 0}};
static INumberVectorProperty HorizontalCoordsWNP = {
    mydev, "HORIZONTAL_COORD_REQUEST", "Horizontal Coords", LX16GROUP, IP_WO, 120, IPS_IDLE,
    HorizontalCoordsWN, NARRAY(HorizontalCoordsWN), "", 0};


void changeLX200_16DeviceName(const char * newName)
{
  strcpy(HorizontalCoordsWNP.device, newName);
  strcpy(HorizontalCoordsRNP.device, newName);
  strcpy(FanStatusSP.device, newName);
  strcpy(HomeSearchSP.device, newName);
  strcpy(FieldDeRotatorSP.device,newName);
}

LX200_16::LX200_16() : LX200Autostar()
{

}
 
void LX200_16::ISGetProperties (const char *dev)
{

if (dev && strcmp (thisDevice, dev))
    return;

  // process parent first
  LX200Autostar::ISGetProperties(dev);

  IDDefNumber (&HorizontalCoordsWNP, NULL);
  IDDefNumber (&HorizontalCoordsRNP, NULL);

  IDDefSwitch (&FanStatusSP, NULL);
  IDDefSwitch (&HomeSearchSP, NULL);
  IDDefSwitch (&FieldDeRotatorSP, NULL);

}

void LX200_16::ISNewText (const char *dev, const char *name, char *texts[], char *names[], int n)
{

	// ignore if not ours //
	if (strcmp (dev, thisDevice))
	    return;

        LX200Autostar::ISNewText (dev, name, texts, names,  n);

}

void LX200_16::ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n)
{
  double newAlt=0, newAz=0;
  char altStr[64], azStr[64];
  int err;

  // ignore if not ours //
  if (strcmp (dev, thisDevice))
    return;

  if ( !strcmp (name, HorizontalCoordsWNP.name) )
  {
      int i=0, nset=0;

      if (checkPower(&HorizontalCoordsWNP))
	   return;

        for (nset = i = 0; i < n; i++)
	    {
		INumber *horp = IUFindNumber (&HorizontalCoordsWNP, names[i]);
		if (horp == &HorizontalCoordsWN[0])
		{
                    newAlt = values[i];
		    nset += newAlt >= -90. && newAlt <= 90.0;
		} else if (horp == &HorizontalCoordsWN[1])
		{
		    newAz = values[i];
		    nset += newAz >= 0. && newAz <= 360.0;
		}
	    }

	  if (nset == 2)
	  {
	   if ( (err = setObjAz(fd, newAz)) < 0 || (err = setObjAlt(fd, newAlt)) < 0)
	   {
	     handleError(&HorizontalCoordsWNP, err, "Setting Alt/Az");
	     return;
	   }
	       //HorizontalCoordsWNP.s = IPS_OK;
	       //HorizontalCoordsWNP.n[0].value = values[0];
	       //HorizontalCoordsWNP.n[1].value = values[1];
	       targetAZ  = newAz;
	       targetALT = newAlt;

	       fs_sexa(azStr, targetAZ, 2, 3600);
	       fs_sexa(altStr, targetALT, 2, 3600);

	       //IDSetNumber (&HorizontalCoordsWNP, "Attempting to slew to Alt %s - Az %s", altStr, azStr);
	       handleAltAzSlew();
	  }
	  else
	  {
		HorizontalCoordsWNP.s = IPS_ALERT;
		IDSetNumber(&HorizontalCoordsWNP, "Altitude or Azimuth missing or invalid");
	  }
	
	  return;  
    }

    LX200Autostar::ISNewNumber (dev, name, values, names, n);
}
    



void LX200_16::ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n)
{
   int index;
   int err;

  if (strcmp (dev, thisDevice))
    return;

   if (!strcmp(name, FanStatusSP.name))
   {
      if (checkPower(&FanStatusSP))
       return;

          IUResetSwitch(&FanStatusSP);
          IUUpdateSwitch(&FanStatusSP, states, names, n);
          index = getOnSwitch(&FanStatusSP);

	  if (index == 0)
	  {
	    if ( (err = turnFanOn(fd)) < 0)
	    {
	      handleError(&FanStatusSP, err, "Changing fan status");
	      return;
	    }
	  }
	  else
	  {
	    if ( (err = turnFanOff(fd)) < 0)
	    {
	      handleError(&FanStatusSP, err, "Changing fan status");
	      return;
	    }
	  }
	  
	  FanStatusSP.s = IPS_OK;
	  IDSetSwitch (&FanStatusSP, index == 0 ? "Fan is ON" : "Fan is OFF");
	  return;
   }

   if (!strcmp(name, HomeSearchSP.name))
   {
      if (checkPower(&HomeSearchSP))
       return;

          IUResetSwitch(&HomeSearchSP);
          IUUpdateSwitch(&HomeSearchSP, states, names, n);
          index = getOnSwitch(&HomeSearchSP);

	  if (index == 0)
		seekHomeAndSave(fd);
	  else 
		seekHomeAndSet(fd);

	  HomeSearchSP.s = IPS_BUSY;
	  IDSetSwitch (&HomeSearchSP, index == 0 ? "Seek Home and Save" : "Seek Home and Set");
	  return;
   }

   if (!strcmp(name, FieldDeRotatorSP.name))
   {
      if (checkPower(&FieldDeRotatorSP))
       return;

          IUResetSwitch(&FieldDeRotatorSP);
          IUUpdateSwitch(&FieldDeRotatorSP, states, names, n);
          index = getOnSwitch(&FieldDeRotatorSP);

	  if (index == 0)
	  	seekHomeAndSave(fd);
	  else
		seekHomeAndSet(fd);

	  FieldDeRotatorSP.s = IPS_OK;
	  IDSetSwitch (&FieldDeRotatorSP, index == 0 ? "Field deRotator is ON" : "Field deRotator is OFF");
	  return;
   }

    LX200Autostar::ISNewSwitch (dev, name, states, names, n);

}

void LX200_16::handleAltAzSlew()
{
        int i=0;
	char altStr[64], azStr[64];

	  if (HorizontalCoordsWNP.s == IPS_BUSY)
	  {
	     abortSlew(fd);

	     // sleep for 100 mseconds
	     usleep(100000);
	  }

	  if ((i = slewToAltAz(fd)))
	  {
	    HorizontalCoordsWNP.s = IPS_ALERT;
	    IDSetNumber(&HorizontalCoordsWNP, "Slew is not possible.");
	    return;
	  }

	  HorizontalCoordsWNP.s = IPS_BUSY;
	  HorizontalCoordsRNP.s = IPS_BUSY;
	  fs_sexa(azStr, targetAZ, 2, 3600);
	  fs_sexa(altStr, targetALT, 2, 3600);

	  IDSetNumber(&HorizontalCoordsWNP, "Slewing to Alt %s - Az %s", altStr, azStr);
	  IDSetNumber(&HorizontalCoordsRNP, NULL);
	  return;
}

 void LX200_16::ISPoll ()
 {
   int searchResult=0;
   double dx, dy;
   int err;
   
   LX200Autostar::ISPoll();

   	switch (HomeSearchSP.s)
	{
	case IPS_IDLE:
	     break;

	case IPS_BUSY:

	    if ( (err = getHomeSearchStatus(fd, &searchResult)) < 0)
	    {
	      handleError(&HomeSearchSP, err, "Home search");
	      return;
	    }

	    if (searchResult == 0)
	    {
	      HomeSearchSP.s = IPS_ALERT;
	      IDSetSwitch(&HomeSearchSP, "Home search failed.");
	    }
	    else if (searchResult == 1)
	    {
	      HomeSearchSP.s = IPS_OK;
	      IDSetSwitch(&HomeSearchSP, "Home search successful.");
	    }
	    else if (searchResult == 2)
	      IDSetSwitch(&HomeSearchSP, "Home search in progress...");
	    else
	    {
	      HomeSearchSP.s = IPS_ALERT;
	      IDSetSwitch(&HomeSearchSP, "Home search error.");
	    }
	    break;

	 case IPS_OK:
	   break;
	 case IPS_ALERT:
	  break;
	}

	switch (HorizontalCoordsWNP.s)
	{
	case IPS_IDLE:
	     break;

	case IPS_BUSY:

	    if ( (err = getLX200Az(fd, &currentAZ)) < 0 || (err = getLX200Alt(fd, &currentALT)) < 0)
	    {
	      handleError(&HorizontalCoordsWNP, err, "Get Alt/Az");
	      return;
	    }
	    
	    dx = targetAZ - currentAZ;
	    dy = targetALT - currentALT;

            HorizontalCoordsRNP.np[0].value = currentALT;
	    HorizontalCoordsRNP.np[1].value = currentAZ;

	    // accuracy threshold (3'), can be changed as desired.
	    if ( fabs(dx) <= 0.05 && fabs(dy) <= 0.05)
	    {

		HorizontalCoordsWNP.s = IPS_OK;
		HorizontalCoordsRNP.s = IPS_OK;
		currentAZ = targetAZ;
		currentALT = targetALT;
                IDSetNumber (&HorizontalCoordsWNP, "Slew is complete.");
		IDSetNumber (&HorizontalCoordsRNP, NULL);
	    } else
	    	IDSetNumber (&HorizontalCoordsRNP, NULL);
	    break;

	case IPS_OK:
	    break;

	case IPS_ALERT:
	    break;
	}

 }

 void LX200_16::getBasicData()
 {

   getLX200Az(fd, &currentAZ);
   getLX200Alt(fd, &currentALT);
   IDSetNumber (&HorizontalCoordsRNP, NULL);

   LX200Autostar::getBasicData();

 }
