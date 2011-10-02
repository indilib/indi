#if 0
    LX200 FS2 Driver
	Copyright (C) 2009 Ferran Casarramona (ferran.casarramona@gmail.com)
	Based on LX200 driver from:
    Copyright (C) 2005 Jasem Mutlaq (mutlaqja@ikarustech.com)

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

#include <string.h>

/* LX200 Command Set */
#include "lx200driver.h"

/* Our driver header */
#include "lx200fs2.h"

using namespace std;

/* Our telescope auto pointer */
extern LX200Generic *telescope;

// Access to properties defined in LX200Generic
extern ISwitchVectorProperty ConnectSP;
extern ITextVectorProperty   PortTP;

const char *BASIC_GROUP    = "Main Control";		// Main Group
const char *OPTIONS_GROUP  = "Options";			// Options Group
const char *MOTION_GROUP   = "Motion Control";

/* Handy Macros */
#define currentRA	EquatorialCoordsRN[0].value
#define currentDEC	EquatorialCoordsRN[1].value
#define targetRA	EquatorialCoordsWN[0].value
#define targetDEC	EquatorialCoordsWN[1].value

static void retryConnection(void *);

void changeLX200FS2DeviceName(const char *newName)
{

}

/**************************************************************************************
** LX200 FS2 constructor
***************************************************************************************/
LX200Fs2::LX200Fs2() : LX200Generic()
{
   DeviceName = "LX200 FS2";
   init_properties();

   IDLog("Initilizing from LX200 FS2 device...\n");
   IDLog("Driver Version: 2011-01-02\n");
 
   //enableSimulation(true);  
}

/**************************************************************************************
**
***************************************************************************************/
LX200Fs2::~LX200Fs2()
{

}

/**************************************************************************************
** Initialize all properties & set default values.
***************************************************************************************/
void LX200Fs2::init_properties()
{
    // Firmware Version
    IUFillNumber(&FirmwareVerN[0], "FirmwareVer", "Firmware version", "%2.2f", 1.16, 1.25, 0.01, 1.18);
    IUFillNumberVector(&FirmwareVerNP, FirmwareVerN, NARRAY(FirmwareVerN), DeviceName, "FIRMWARE_VER" , "Firmware", BASIC_GROUP, IP_RW, 0, IPS_IDLE);
}

/**************************************************************************************
** Define LX200 FS2 properties to clients.
***************************************************************************************/
void LX200Fs2::ISGetProperties(const char *dev)
{

 if (dev && strcmp (DeviceName, dev))
    return;

  LX200Generic::ISGetProperties(dev);
  // Delete not supported properties 
		IDDelete(DeviceName, "Alignment", NULL);
		IDDelete(DeviceName, "Tracking Mode", NULL);
		IDDelete(DeviceName, "Tracking Frequency", NULL);
		// Focus
		IDDelete(DeviceName, "FOCUS_MOTION", NULL);
		IDDelete(DeviceName, "FOCUS_TIMER", NULL);
		IDDelete(DeviceName, "FOCUS_MODE", NULL);
		// Time
		IDDelete(DeviceName, "TIME_UTC", NULL);
		IDDelete(DeviceName, "TIME_UTC_OFFSET", NULL);
		IDDelete(DeviceName, "TIME_LST", NULL);
		// Sites
		IDDelete(DeviceName, "Sites", NULL);
		IDDelete(DeviceName, "Site Name", NULL);
		
  IDDefNumber(&FirmwareVerNP, NULL);
}

/**************************************************************************************
**
***************************************************************************************/
void LX200Fs2::ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n)
{
	
	// Ignore if not ours
	if (strcmp (dev, DeviceName))
	    return;

	// ===================================
        // Update Firmware version
	// ===================================
	if (!strcmp (name, FirmwareVerNP.name))
	{
		if (IUUpdateNumber(&FirmwareVerNP, values, names, n) < 0)
			return;

		FirmwareVerNP.s = IPS_OK;

		IDSetNumber(&FirmwareVerNP, NULL);
		return;
	}
	
	LX200Generic::ISNewNumber (dev, name, values, names, n);
}

/**************************************************************************************
**
***************************************************************************************/
int LX200Fs2::check_fs2_connection(int fd)
{
  double x;
  
  if (FirmwareVerNP.np[0].value < 1.19)
	return getLX200RA(fd, &x); // Version 1.18 and below don't support ACK command
  else
	return check_lx200_connection(fd);
}

/**************************************************************************************
**
***************************************************************************************/
void LX200Fs2::connectTelescope()
{
     switch (ConnectSP.sp[0].s)
     {
      case ISS_ON:  
	
		fprintf (stderr, "Trace: LX200FS2 connectTelescope ON");
        if (simulation)
		{
			ConnectSP.s = IPS_OK;
			IDSetSwitch (&ConnectSP, "Simulated telescope is online.");
			return;
		}
	
        if (tty_connect(PortTP.tp[0].text, 9600, 8, 0, 1, &fd) != TTY_OK)
		{
			ConnectSP.sp[0].s = ISS_OFF;
			ConnectSP.sp[1].s = ISS_ON;
			IDSetSwitch (&ConnectSP, "Error connecting to port %s. Make sure you have BOTH read and write permission to the port.", PortTP.tp[0].text);
			return;
		}

		if (check_fs2_connection(fd))
		{   
			ConnectSP.sp[0].s = ISS_OFF;
			ConnectSP.sp[1].s = ISS_ON;
			IDSetSwitch (&ConnectSP, "Error connecting to Telescope. Telescope is offline.");
			return;
		}

		ConnectSP.s = IPS_OK;
		IDSetSwitch (&ConnectSP, "Telescope is online. Retrieving basic data...");
		getBasicData();
		break;

     case ISS_OFF:
        ConnectSP.sp[0].s = ISS_OFF;
		ConnectSP.sp[1].s = ISS_ON;
        ConnectSP.s = IPS_IDLE;
		if (simulation)
         {
			IDSetSwitch (&ConnectSP, "Simulated Telescope is offline.");
			return;
         }
         IDSetSwitch (&ConnectSP, "Telescope is offline.");
		 IDLog("Telescope is offline.");
         
		 tty_disconnect(fd);
		 break;
    }
}

/**************************************************************************************
**
***************************************************************************************/
void LX200Fs2::getBasicData()
{

  // process parent first
   LX200Generic::getBasicData();
  // Make sure short
  //checkLX200Format(fd);

  // Get current RA/DEC
  //getLX200RA(fd, &currentRA);
  //getLX200DEC(fd, &currentDEC);

  //IDSetNumber (&EquatorialCoordsRNP, NULL);  
}

/**********************************************************************************
**  handleError functions. Redefined because they call check_lx200_connection and this
**                         driver needs to call check_fs2_connection
**************************************************************************************/
static void retryConnection(void * p)
{
  int fd = *((int *) p);

  if (((LX200Fs2 *)telescope)->check_fs2_connection(fd))
  {
    ConnectSP.s = IPS_IDLE;
    IDSetSwitch(&ConnectSP, "The connection to the telescope is lost.");
    return;
  }
  
  ConnectSP.sp[0].s = ISS_ON;
  ConnectSP.sp[1].s = ISS_OFF;
  ConnectSP.s = IPS_OK;
   
  IDSetSwitch(&ConnectSP, "The connection to the telescope has been resumed.");
}

void LX200Fs2::handleError(ISwitchVectorProperty *svp, int err, const char *msg)
{
  
  svp->s = IPS_ALERT;
  
  /* First check to see if the telescope is connected */
    if (check_fs2_connection(fd))
    {
      /* The telescope is off locally */
      ConnectSP.sp[0].s = ISS_OFF;
      ConnectSP.sp[1].s = ISS_ON;
      ConnectSP.s = IPS_BUSY;
      IDSetSwitch(&ConnectSP, "Telescope is not responding to commands, will retry in 10 seconds.");
      
      IDSetSwitch(svp, NULL);
      IEAddTimer(10000, retryConnection, &fd);
      return;
    }
    
   /* If the error is a time out, then the device doesn't support this property or busy*/
      if (err == -2)
      {
       svp->s = IPS_ALERT;
       IDSetSwitch(svp, "Device timed out. Current device may be busy or does not support %s. Will retry again.", msg);
      }
      else
    /* Changing property failed, user should retry. */
       IDSetSwitch( svp , "%s failed.", msg);
       
       fault = true;
}

void LX200Fs2::handleError(INumberVectorProperty *nvp, int err, const char *msg)
{
  
  nvp->s = IPS_ALERT;
  
  /* First check to see if the telescope is connected */
    if (check_fs2_connection(fd))
    {
      /* The telescope is off locally */
      ConnectSP.sp[0].s = ISS_OFF;
      ConnectSP.sp[1].s = ISS_ON;
      ConnectSP.s = IPS_BUSY;
      IDSetSwitch(&ConnectSP, "Telescope is not responding to commands, will retry in 10 seconds.");
      
      IDSetNumber(nvp, NULL);
      IEAddTimer(10000, retryConnection, &fd);
      return;
    }
    
   /* If the error is a time out, then the device doesn't support this property */
      if (err == -2)
      {
       nvp->s = IPS_ALERT;
       IDSetNumber(nvp, "Device timed out. Current device may be busy or does not support %s. Will retry again.", msg);
      }
      else
    /* Changing property failed, user should retry. */
       IDSetNumber( nvp , "%s failed.", msg);
       
       fault = true;
}

void LX200Fs2::handleError(ITextVectorProperty *tvp, int err, const char *msg)
{
  
  tvp->s = IPS_ALERT;
  
  /* First check to see if the telescope is connected */
    if (check_fs2_connection(fd))
    {
      /* The telescope is off locally */
      ConnectSP.sp[0].s = ISS_OFF;
      ConnectSP.sp[1].s = ISS_ON;
      ConnectSP.s = IPS_BUSY;
      IDSetSwitch(&ConnectSP, "Telescope is not responding to commands, will retry in 10 seconds.");
      
      IDSetText(tvp, NULL);
      IEAddTimer(10000, retryConnection, &fd);
      return;
    }
    
   /* If the error is a time out, then the device doesn't support this property */
      if (err == -2)
      {
       tvp->s = IPS_ALERT;
       IDSetText(tvp, "Device timed out. Current device may be busy or does not support %s. Will retry again.", msg);
      }
       
      else
    /* Changing property failed, user should retry. */
       IDSetText( tvp , "%s failed.", msg);
       
       fault = true;
}
