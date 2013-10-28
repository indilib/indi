/*
    LX200 Autostar
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

#include "libs/indibase/inditelescope.h"
#include "lx200autostar.h"
#include "lx200driver.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define FIRMWARE_TAB "Firmware data"

extern int MaxReticleFlashRate;



/********************************************
 Property: Park telescope to HOME
*********************************************/


LX200Autostar::LX200Autostar() : LX200Generic()
{

}

bool LX200Autostar::initProperties()
{
    IUFillText(&VersionT[0], "Date", "", "");
    IUFillText(&VersionT[1], "Time", "", "");
    IUFillText(&VersionT[2], "Number", "", "");
    IUFillText(&VersionT[3], "Full", "", "");
    IUFillText(&VersionT[4], "Name", "", "");
    IUFillTextVector(&VersionTP, VersionT, 5, getDeviceName(), "Firmware Info", "", FIRMWARE_TAB, IP_RO, 0, IPS_IDLE);

    IUFillNumber(&FocusSpeedN[0], "SPEED", "Speed", "%0.f", 0, 4.0, 1.0, 0);
    IUFillNumberVector(&FocusSpeedNP, FocusSpeedN, 1, getDeviceName(), "FOCUS_SPEED", "Speed", FOCUS_TAB, IP_RW, 0, IPS_IDLE);

}

void LX200Autostar::ISGetProperties (const char *dev)
{

    LX200Generic::ISGetProperties(dev);

    //IDDefSwitch (&ParkSP, NULL);
    //IDDefText   (&VersionTP, NULL);
    //IDDefNumber (&FocusSpeedNP, NULL);

    // For Autostar, we have a different focus speed method
    // Therefore, we don't need the classical one
    deleteProperty(FocusModeSP.name);
    //IDDelete(thisDevice, "FOCUS_MODE", NULL);

    if (isConnected())
    {
        defineText(&VersionTP);
        defineNumber(&FocusSpeedNP);
    }

}

bool LX200Autostar::updateProperties()
{

    LX200Generic::updateProperties();

    if (isConnected())
    {
        defineText(&VersionTP);
        defineNumber(&FocusSpeedNP);
    }
    else
    {
        deleteProperty(VersionTP.name);
        deleteProperty(FocusSpeedNP.name);
    }
}

bool LX200Autostar::ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n)
{
   if(strcmp(dev,getDeviceName())==0)
   {
       // Focus speed
       if (!strcmp (name, FocusSpeedNP.name))
       {

         if (IUUpdateNumber(&FocusSpeedNP, values, names, n) < 0)
           return false;

         setGPSFocuserSpeed(PortFD,  ( (int) FocusSpeedN[0].value));
         FocusSpeedNP.s = IPS_OK;
         IDSetNumber(&FocusSpeedNP, NULL);
         return true;
       }
   }

    return LX200Generic::ISNewNumber (dev, name, values, names, n);
}

 bool LX200Autostar::ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n)
 {
   int index=0, err=0;
 
   if(strcmp(dev,getDeviceName())==0)
   {
        // Focus Motion
        if (!strcmp (name, FocusMotionSP.name))
        {
            IUResetSwitch(&FocusMotionSP);
	  
            // If speed is "halt"
            if (FocusSpeedN[0].value == 0)
            {
                FocusMotionSP.s = IPS_IDLE;
                IDSetSwitch(&FocusMotionSP, NULL);
                return false;
            }
	  
            IUUpdateSwitch(&FocusMotionSP, states, names, n);
            index = IUFindOnSwitchIndex(&FocusMotionSP);
	  
           if (setFocuserMotion(PortFD, index) < 0)
           {
               FocusMotionSP.s = IPS_ALERT;
               IDSetSwitch(&FocusMotionSP, "Error setting focuser speed.");
               return false;
           }

           FocusMotionSP.s = IPS_BUSY;
	  
           // with a timer
           if (FocusTimerNP.np[0].value > 0)
           {
                FocusTimerNP.s  = IPS_BUSY;
                if (isDebug())
                    IDLog("Starting Focus Timer BUSY\n");

                IEAddTimer(50, LX200Generic::updateFocusHelper, this);
           }
	  
           IDSetSwitch(&FocusMotionSP, NULL);
           return true;
        }
	}

   return LX200Generic::ISNewSwitch (dev, name, states, names,  n);

 }

 void LX200Autostar::getBasicData()
 {

   // process parent
   LX200Generic::getBasicData();

   VersionTP.tp[0].text = new char[64];
   getVersionDate(PortFD, VersionTP.tp[0].text);
   VersionTP.tp[1].text = new char[64];
   getVersionTime(PortFD, VersionTP.tp[1].text);
   VersionTP.tp[2].text = new char[64];
   getVersionNumber(PortFD, VersionTP.tp[2].text);
   VersionTP.tp[3].text = new char[128];
   getFullVersion(PortFD, VersionTP.tp[3].text);
   VersionTP.tp[4].text = new char[128];
   getProductName(PortFD, VersionTP.tp[4].text);

   IDSetText(&VersionTP, NULL);
   


 }
