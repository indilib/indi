/*
    LX200 Classoc
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

#include "lx200classic.h"
#include "lx200driver.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern LX200Generic *telescope;
extern INumberVectorProperty EquatorialCoordsWNP;
extern INumberVectorProperty EquatorialCoordsRNP;
extern ITextVectorProperty TimeTP;
extern int MaxReticleFlashRate;

/* Handy Macros */
#define currentRA	EquatorialCoordsRNP.np[0].value
#define currentDEC	EquatorialCoordsRNP.np[1].value

#define BASIC_GROUP	"Main Control"
#define LIBRARY_GROUP	"Library"
#define MOVE_GROUP	"Movement Control"

static IText   ObjectText[] = {{"objectText", "Info", 0, 0, 0, 0}};
static ITextVectorProperty ObjectInfoTP = {mydev, "Object Info", "", BASIC_GROUP, IP_RO, 0, IPS_IDLE, ObjectText, NARRAY(ObjectText), "", 0};

/* Library group */
static ISwitch StarCatalogS[]    = {{"STAR", "", ISS_ON, 0, 0}, {"SAO", "", ISS_OFF, 0, 0}, {"GCVS", "", ISS_OFF, 0, 0}};
static ISwitch DeepSkyCatalogS[] = {{"NGC", "", ISS_ON, 0, 0}, {"IC", "", ISS_OFF, 0, 0}, {"UGC", "", ISS_OFF, 0, 0}, {"Caldwell", "", ISS_OFF, 0, 0}, {"Arp", "", ISS_OFF, 0, 0}, {"Abell", "", ISS_OFF, 0, 0}, {"Messier", "", ISS_OFF, 0, 0}};
static ISwitch SolarS[]          = { {"Select", "Select item...", ISS_ON, 0, 0}, {"1", "Mercury", ISS_OFF,0 , 0}, {"2", "Venus", ISS_OFF, 0, 0}, {"3", "Moon", ISS_OFF, 0, 0}, {"4", "Mars", ISS_OFF, 0, 0}, {"5", "Jupiter", ISS_OFF, 0, 0}, {"6", "Saturn", ISS_OFF, 0, 0}, {"7", "Uranus", ISS_OFF, 0, 0}, {"8", "Neptune", ISS_OFF, 0, 0}, {"9", "Pluto", ISS_OFF, 0 ,0}};

static ISwitchVectorProperty StarCatalogSP   = { mydev, "Star Catalogs", "", LIBRARY_GROUP, IP_RW, ISR_1OFMANY, 0, IPS_IDLE, StarCatalogS, NARRAY(StarCatalogS), "", 0};
static ISwitchVectorProperty DeepSkyCatalogSP= { mydev, "Deep Sky Catalogs", "", LIBRARY_GROUP, IP_RW, ISR_1OFMANY, 0, IPS_IDLE, DeepSkyCatalogS, NARRAY(DeepSkyCatalogS), "", 0};
static ISwitchVectorProperty SolarSP         = { mydev, "SOLAR_SYSTEM", "Solar System", LIBRARY_GROUP, IP_RW, ISR_1OFMANY, 0, IPS_IDLE, SolarS, NARRAY(SolarS), "", 0};

static INumber ObjectN[] = {{ "ObjectN", "Number", "%g", 1., 10000., 1., 0., 0, 0, 0}};
static INumberVectorProperty ObjectNoNP= { mydev, "Object Number", "", LIBRARY_GROUP, IP_RW, 0, IPS_IDLE, ObjectN, NARRAY(ObjectN), "", 0 };

static INumber MaxSlew[] = {{"maxSlew", "Rate", "%g", 2.0, 9.0, 1.0, 9., 0, 0 ,0}};
static INumberVectorProperty MaxSlewRateNP = { mydev, "Max slew Rate", "", MOVE_GROUP, IP_RW, 0, IPS_IDLE, MaxSlew, NARRAY(MaxSlew), "", 0};

static INumber altLimit[] = {
       {"minAlt", "min Alt", "%+03f", -90., 90., 0., 0., 0, 0, 0},
       {"maxAlt", "max Alt", "%+03f", -90., 90., 0., 0., 0, 0, 0}};
static INumberVectorProperty ElevationLimitNP = { mydev, "altLimit", "Slew elevation Limit", BASIC_GROUP, IP_RW, 0, IPS_IDLE, altLimit, NARRAY(altLimit), "", 0};

LX200Classic::LX200Classic() : LX200Generic()
{
   ObjectInfoTP.tp[0].text = NULL;
   
   currentCatalog = LX200_STAR_C;
   currentSubCatalog = 0;

}

void LX200Classic::ISGetProperties (const char *dev)
{

if (dev && strcmp (thisDevice, dev))
    return;

  LX200Generic::ISGetProperties(dev);

  IDDefNumber (&ElevationLimitNP, NULL);
  IDDefText   (&ObjectInfoTP, NULL);
  IDDefSwitch (&SolarSP, NULL);
  IDDefSwitch (&StarCatalogSP, NULL);
  IDDefSwitch (&DeepSkyCatalogSP, NULL);
  IDDefNumber (&ObjectNoNP, NULL);
  IDDefNumber (&MaxSlewRateNP, NULL);

}

bool LX200Classic::ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n)
{
    int err=0;
    
    // ignore if not ours //
	if (strcmp (dev, thisDevice))
	    return;

    if ( !strcmp (name, ObjectNoNP.name) )
	{

	  char object_name[256];

	  if (checkPower(&ObjectNoNP))
	    return;

	  if (selectCatalogObject(fd, currentCatalog, (int) values[0]) < 0)
	  {
		ObjectNoNP.s = IPS_ALERT;
		IDSetNumber(&ObjectNoNP, "Failed to select catalog object.");
		return;
	  }
           
	  getLX200RA(fd, &targetRA);
	  getLX200DEC(fd, &targetDEC);

	  ObjectNoNP.s = IPS_OK;
	  IDSetNumber(&ObjectNoNP , "Object updated.");

	  if (getObjectInfo(fd, object_name) < 0)
	    IDMessage(thisDevice, "Getting object info failed.");
	  else
	  {
	    IUSaveText(&ObjectInfoTP.tp[0], object_name);
	    IDSetText  (&ObjectInfoTP, NULL);
	  }

	  handleCoordSet();
	  return;
        }
	
    if ( !strcmp (name, MaxSlewRateNP.name) )
    {

	 if (checkPower(&MaxSlewRateNP))
	  return;

	 if ( ( err = setMaxSlewRate(fd, (int) values[0]) < 0) )
	 {
	        handleError(&MaxSlewRateNP, err, "Setting maximum slew rate");
		return;
	 }
	  MaxSlewRateNP.s = IPS_OK;
	  MaxSlewRateNP.np[0].value = values[0];
	  IDSetNumber(&MaxSlewRateNP, NULL);
	  return;
    }


    if (!strcmp (name, ElevationLimitNP.name))
	{
	    // new elevation limits
	    double minAlt = 0, maxAlt = 0;
	    int i, nset;

	  if (checkPower(&ElevationLimitNP))
	   return;

	    for (nset = i = 0; i < n; i++)
	    {
		INumber *altp = IUFindNumber (&ElevationLimitNP, names[i]);
		if (altp == &altLimit[0])
		{
		    minAlt = values[i];
		    nset += minAlt >= -90.0 && minAlt <= 90.0;
		} else if (altp == &altLimit[1])
		{
		    maxAlt = values[i];
		    nset += maxAlt >= -90.0 && maxAlt <= 90.0;
		}
	    }
	    if (nset == 2)
	    {
		//char l[32], L[32];
		if ( ( err = setMinElevationLimit(fd, (int) minAlt) < 0) )
	 	{
	         handleError(&ElevationLimitNP, err, "Setting elevation limit");
	 	}
		setMaxElevationLimit(fd, (int) maxAlt);
		ElevationLimitNP.np[0].value = minAlt;
		ElevationLimitNP.np[1].value = maxAlt;
		ElevationLimitNP.s = IPS_OK;
		IDSetNumber (&ElevationLimitNP, NULL);
	    } else
	    {
		ElevationLimitNP.s = IPS_IDLE;
		IDSetNumber(&ElevationLimitNP, "elevation limit missing or invalid");
	    }

	    return;
	}

    return LX200Generic::ISNewNumber (dev, name, values, names, n);
}

 bool LX200Classic::ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n)
 {

      int index=0;
      
       // ignore if not ours //
	if (strcmp (dev, thisDevice))
	    return;
      
        // Star Catalog
	if (!strcmp (name, StarCatalogSP.name))
	{
	  if (checkPower(&StarCatalogSP))
	   return;

	 IUResetSwitch(&StarCatalogSP);
	 IUUpdateSwitch(&StarCatalogSP, states, names, n);
	 index = getOnSwitch(&StarCatalogSP);

	 currentCatalog = LX200_STAR_C;

	  if (selectSubCatalog(fd, currentCatalog, index))
	  {
	   currentSubCatalog = index;
	   StarCatalogSP.s = IPS_OK;
	   IDSetSwitch(&StarCatalogSP, NULL);
	  }
	  else
	  {
	   StarCatalogSP.s = IPS_IDLE;
	   IDSetSwitch(&StarCatalogSP, "Catalog unavailable");
	  }
	  return;
	}

	// Deep sky catalog
	if (!strcmp (name, DeepSkyCatalogSP.name))
	{
	  if (checkPower(&DeepSkyCatalogSP))
	   return;

	IUResetSwitch(&DeepSkyCatalogSP);
	IUUpdateSwitch(&DeepSkyCatalogSP, states, names, n);
	index = getOnSwitch(&DeepSkyCatalogSP);

	  if (index == LX200_MESSIER_C)
	  {
	    currentCatalog = index;
	    DeepSkyCatalogSP.s = IPS_OK;
	    IDSetSwitch(&DeepSkyCatalogSP, NULL);
	    return;
	  }
	  else
	    currentCatalog = LX200_DEEPSKY_C;

	  if (selectSubCatalog(fd, currentCatalog, index))
	  {
	   currentSubCatalog = index;
	   DeepSkyCatalogSP.s = IPS_OK;
	   IDSetSwitch(&DeepSkyCatalogSP, NULL);
	  }
	  else
	  {
	   DeepSkyCatalogSP.s = IPS_IDLE;
	   IDSetSwitch(&DeepSkyCatalogSP, "Catalog unavailable");
	  }
	  return;
	}

	// Solar system
	if (!strcmp (name, SolarSP.name))
	{

	  if (checkPower(&SolarSP))
	   return;

	   if (IUUpdateSwitch(&SolarSP, states, names, n) < 0)
		return;

	   index = getOnSwitch(&SolarSP);

	  // We ignore the first option : "Select item"
	  if (index == 0)
	  {
	    SolarSP.s  = IPS_IDLE;
	    IDSetSwitch(&SolarSP, NULL);
	    return;
	  }

          selectSubCatalog (fd, LX200_STAR_C, LX200_STAR);
	  selectCatalogObject(fd, LX200_STAR_C, index + 900);

	  ObjectNoNP.s = IPS_OK;
	  SolarSP.s  = IPS_OK;

	  getObjectInfo(fd, ObjectInfoTP.tp[0].text);
	  IDSetNumber(&ObjectNoNP , "Object updated.");
	  IDSetSwitch(&SolarSP, NULL);

	  if (currentCatalog == LX200_STAR_C || currentCatalog == LX200_DEEPSKY_C)
	  	selectSubCatalog(fd, currentCatalog, currentSubCatalog);

	  getObjectRA(fd, &targetRA);
	  getObjectDEC(fd, &targetDEC);

	  handleCoordSet();

	  return;
	}

   return LX200Generic::ISNewSwitch (dev, name, states, names,  n);

 }


