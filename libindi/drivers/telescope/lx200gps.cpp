/*
    LX200 GPS
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

#include "lx200gps.h"
#include "lx200driver.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define GPSGroup   "Extended GPS Features"

extern LX200Generic *telescope;
extern int MaxReticleFlashRate;

static ISwitch GPSPowerS[]		= {{ "On", "", ISS_OFF, 0, 0}, {"Off", "", ISS_ON, 0, 0}};
static ISwitch GPSStatusS[]	  	= {{ "Sleep", "", ISS_OFF, 0, 0}, {"Wake up", "", ISS_OFF, 0 ,0}, {"Restart", "", ISS_OFF, 0, 0}};
static ISwitch GPSUpdateS[]	  	= { {"Update GPS", "", ISS_OFF, 0, 0}, {"Update Client", "", ISS_OFF, 0, 0}};
static ISwitch AltDecPecS[]		= {{ "Enable", "", ISS_OFF, 0 ,0}, {"Disable", "", ISS_OFF, 0 ,0}};
static ISwitch AzRaPecS[]		= {{ "Enable", "", ISS_OFF, 0, 0}, {"Disable", "", ISS_OFF, 0 ,0}};
static ISwitch SelenSyncS[]		= {{ "Sync", "",  ISS_OFF, 0, 0}};
static ISwitch AltDecBackSlashS[]	= {{ "Activate", "", ISS_OFF, 0, 0}};
static ISwitch AzRaBackSlashS[]		= {{ "Activate", "", ISS_OFF, 0, 0}};
static ISwitch OTAUpdateS[]		= {{ "Update", "", ISS_OFF, 0, 0}};

static ISwitchVectorProperty GPSPowerSP	   = { mydev, "GPS Power", "", GPSGroup, IP_RW, ISR_1OFMANY, 0 , IPS_IDLE, GPSPowerS, NARRAY(GPSPowerS), "", 0};
static ISwitchVectorProperty GPSStatusSP   = { mydev, "GPS Status", "", GPSGroup, IP_RW, ISR_1OFMANY, 0, IPS_IDLE, GPSStatusS, NARRAY(GPSStatusS), "", 0};
static ISwitchVectorProperty GPSUpdateSP   = { mydev, "GPS System", "", GPSGroup, IP_RW, ISR_1OFMANY, 0, IPS_IDLE, GPSUpdateS, NARRAY(GPSUpdateS), "", 0};
static ISwitchVectorProperty AltDecPecSP   = { mydev, "Alt/Dec PEC", "", GPSGroup, IP_RW, ISR_1OFMANY, 0, IPS_IDLE, AltDecPecS, NARRAY(AltDecPecS), "", 0};
static ISwitchVectorProperty AzRaPecSP	   = { mydev, "Az/Ra PEC", "", GPSGroup, IP_RW, ISR_1OFMANY, 0, IPS_IDLE, AzRaPecS, NARRAY(AzRaPecS), "", 0};
static ISwitchVectorProperty SelenSyncSP   = { mydev, "Selenographic Sync", "", GPSGroup, IP_RW, ISR_ATMOST1, 0, IPS_IDLE, SelenSyncS, NARRAY(SelenSyncS), "", 0};
static ISwitchVectorProperty AltDecBackSlashSP	= { mydev, "Alt/Dec Anti-backlash", "", GPSGroup, IP_RW, ISR_ATMOST1, 0, IPS_IDLE, AltDecBackSlashS, NARRAY(AltDecBackSlashS), "", 0};
static ISwitchVectorProperty AzRaBackSlashSP	= { mydev, "Az/Ra Anti-backlash", "", GPSGroup, IP_RW, ISR_ATMOST1, 0, IPS_IDLE, AzRaBackSlashS, NARRAY(AzRaBackSlashS), "", 0};
static ISwitchVectorProperty OTAUpdateSP	= { mydev, "OTA Update", "", GPSGroup, IP_RW, ISR_ATMOST1, 0, IPS_IDLE, OTAUpdateS, NARRAY(OTAUpdateS), "", 0};

static INumber OTATempN[]	= { {"Temp.", "", "%g", -200., 500., 0., 0., 0, 0, 0 } };
static INumberVectorProperty OTATempNP =   { mydev, "OTA Temperature (C)", "", GPSGroup, IP_RO, 0, IPS_IDLE, OTATempN, NARRAY(OTATempN), "", 0};

void updateTemp(void *p);

void changeLX200GPSDeviceName(const char *newName)
{
 strcpy(GPSPowerSP.device, newName);
 strcpy(GPSStatusSP.device, newName );
 strcpy(GPSUpdateSP.device, newName  );
 strcpy(AltDecPecSP.device, newName );
 strcpy(AzRaPecSP.device,newName  );
 strcpy(SelenSyncSP.device, newName );
 strcpy(AltDecBackSlashSP.device, newName );
 strcpy(AzRaBackSlashSP.device, newName );
 strcpy(OTATempNP.device, newName );
 strcpy(OTAUpdateSP.device, newName);
 
}

LX200GPS::LX200GPS() : LX200_16()
{
   IEAddTimer(900000, updateTemp, &fd);
   
}

void LX200GPS::ISGetProperties (const char *dev)
{

if (dev && strcmp (thisDevice, dev))
    return;

// process parent first
   LX200_16::ISGetProperties(dev);

IDDefSwitch (&GPSPowerSP, NULL);
IDDefSwitch (&GPSStatusSP, NULL);
IDDefSwitch (&GPSUpdateSP, NULL);
IDDefSwitch (&AltDecPecSP, NULL);
IDDefSwitch (&AzRaPecSP, NULL);
IDDefSwitch (&SelenSyncSP, NULL);
IDDefSwitch (&AltDecBackSlashSP, NULL);
IDDefSwitch (&AzRaBackSlashSP, NULL);
IDDefNumber (&OTATempNP, NULL);
IDDefSwitch (&OTAUpdateSP, NULL);

}

void LX200GPS::ISNewText (const char *dev, const char *name, char *texts[], char *names[], int n)
{
	// ignore if not ours //
	if (strcmp (dev, thisDevice))
	    return;

     LX200_16::ISNewText (dev, name, texts, names, n);
}

void LX200GPS::ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n)
{
        
    LX200_16::ISNewNumber (dev, name, values, names, n);

 }


 void LX200GPS::ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n)
 {
    int index=0, err=0;
    char msg[64];

    if (strcmp (dev, thisDevice))
	    return;

    /* GPS Power */
    if (!strcmp(name,GPSPowerSP.name))
    {
       if (checkPower(&GPSPowerSP))
       return;

      if (IUUpdateSwitch(&GPSPowerSP, states, names, n) < 0)
		return;

      index = getOnSwitch(&GPSPowerSP);
      if (index == 0)
	 turnGPSOn(fd);
      else
	turnGPSOff(fd);

      GPSPowerSP.s = IPS_OK;
      IDSetSwitch (&GPSPowerSP, index == 0 ? "GPS System is ON" : "GPS System is OFF" );
      return;
    }

    /* GPS Status Update */
    if (!strcmp(name,GPSStatusSP.name))
    {
       if (checkPower(&GPSStatusSP))
       return;

      if (IUUpdateSwitch(&GPSStatusSP, states, names, n) < 0)
		return;

      index = getOnSwitch(&GPSStatusSP);

      if (index == 0)
      {
	   err = gpsSleep(fd);
	   strcpy(msg, "GPS system is in sleep mode.");
      }
      else if (index == 1)
      {
	   err = gpsWakeUp(fd);
           strcpy(msg, "GPS system is reactivated.");
      }
      else
      {
	   err = gpsRestart(fd);
	   strcpy(msg, "GPS system is restarting...");
	   updateTime();
	   updateLocation();
      }

	GPSStatusSP.s = IPS_OK;
	IDSetSwitch (&GPSStatusSP, "%s", msg);
	return;

    }

    /* GPS Update */
    if (!strcmp(name,GPSUpdateSP.name))
    {
       if (checkPower(&GPSUpdateSP))
       return;

	if (IUUpdateSwitch(&GPSUpdateSP, states, names, n) < 0)
		return;

	index = getOnSwitch(&GPSUpdateSP);

	GPSUpdateSP.s = IPS_OK;

     if (index == 0)
     {
	     
     	     IDSetSwitch(&GPSUpdateSP, "Updating GPS system. This operation might take few minutes to complete...");
     	     if (updateGPS_System(fd))
     	     {
     			IDSetSwitch(&GPSUpdateSP, "GPS system update successful.");
			updateTime();
			updateLocation();
     		}
     		else
     		{
        		GPSUpdateSP.s = IPS_IDLE;
        		IDSetSwitch(&GPSUpdateSP, "GPS system update failed.");
     		}
	}
	else
	{
		updateTime();
		updateLocation();
		IDSetSwitch(&GPSUpdateSP, "Client time and location is synced to LX200 GPS Data.");
	
	}
     		return;
    }

    /* Alt Dec Periodic Error correction */
    if (!strcmp(name, AltDecPecSP.name))
    {
       if (checkPower(&AltDecPecSP))
       return;

      if (IUUpdateSwitch(&AltDecPecSP, states, names, n) < 0)
	return;

      index = getOnSwitch(&AltDecPecSP);
      
       if (index == 0)
      {
        err = enableDecAltPec(fd);
	strcpy (msg, "Alt/Dec Compensation Enabled");
      }
      else
      {
        err = disableDecAltPec(fd);
	strcpy (msg, "Alt/Dec Compensation Disabled");
      }

      AltDecPecSP.s = IPS_OK;
      IDSetSwitch(&AltDecPecSP, "%s", msg);

      return;
    }

    /* Az RA periodic error correction */
    if (!strcmp(name, AzRaPecSP.name))
    {
       if (checkPower(&AzRaPecSP))
       return; 

      if (IUUpdateSwitch(&AzRaPecSP, states, names, n) < 0)
		return;

      index = getOnSwitch(&AzRaPecSP);

       if (index == 0)
      {
        err = enableRaAzPec(fd);
	strcpy (msg, "Ra/Az Compensation Enabled");
      }
      else
      {
        err = disableRaAzPec(fd);
	strcpy (msg, "Ra/Az Compensation Disabled");
      }

      AzRaPecSP.s = IPS_OK;
      IDSetSwitch(&AzRaPecSP, "%s", msg);

      return;
    }

   if (!strcmp(name, AltDecBackSlashSP.name))
   {
      if (checkPower(&AltDecBackSlashSP))
      return;

     err = activateAltDecAntiBackSlash(fd);
     AltDecBackSlashSP.s = IPS_OK;
     IDSetSwitch(&AltDecBackSlashSP, "Alt/Dec Anti-backlash enabled");
     return;
   }

   if (!strcmp(name, AzRaBackSlashSP.name))
   {
     if (checkPower(&AzRaBackSlashSP))
      return;

     err = activateAzRaAntiBackSlash(fd);
     AzRaBackSlashSP.s = IPS_OK;
     IDSetSwitch(&AzRaBackSlashSP, "Az/Ra Anti-backlash enabled");
     return;
   }
   
   if (!strcmp(name, OTAUpdateSP.name))
   {
     int error_type=0;

     if (checkPower(&OTAUpdateSP))
      return;
      
      IUResetSwitch(&OTAUpdateSP);
      
      if ( (error_type = getOTATemp(fd, &OTATempNP.np[0].value)) < 0)
      {
	OTAUpdateSP.s = IPS_ALERT;
	OTATempNP.s = IPS_ALERT;
	IDSetNumber(&OTATempNP, "Error: OTA temperature read timed out.");
      }
      else
      {
        OTAUpdateSP.s = IPS_OK;
	OTATempNP.s = IPS_OK;
	IDSetNumber(&OTATempNP, NULL);
      }
      
      return;
   }

   LX200_16::ISNewSwitch (dev, name, states, names,  n);

}

 void LX200GPS::ISPoll ()
 {

   LX200_16::ISPoll();


 }
 
 void updateTemp(void * p)
 {
   
   int fd = *((int *) p);

   if (telescope->isTelescopeOn())
   {
     if (getOTATemp(fd, &OTATempNP.np[0].value) < 0)
     {
       OTATempNP.s = IPS_ALERT;
       IDSetNumber(&OTATempNP, "Error: OTA temperature read timed out.");
       return;
     }
     else
     {
        OTATempNP.s = IPS_OK; 
   	IDSetNumber(&OTATempNP, NULL);
     }
   }
 
   IEAddTimer(900000, updateTemp, &fd);
      
 }

 void LX200GPS::getBasicData()
 {

   //getOTATemp(&OTATempNP.np[0].value);
   //IDSetNumber(&OTATempNP, NULL);
   
   // process parent
   LX200_16::getBasicData();
 }

