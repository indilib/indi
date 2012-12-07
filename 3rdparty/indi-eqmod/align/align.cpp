/* Copyright 2012 Geehalel (geehalel AT gmail DOT com) */
/* This file is part of the Skywatcher Protocol INDI driver.

    The Skywatcher Protocol INDI driver is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    The Skywatcher Protocol INDI driver is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with the Skywatcher Protocol INDI driver.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <unistd.h>
#include <sys/time.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <memory>

#include <libnova.h>

#include <indicom.h>

#include "align.h"
#include "pointset.h"

#include "config.h"


// We declare an auto pointer to Align.
//std::auto_ptr<Align> align(0);

Align::Align(INDI::Telescope *t) 
{
  telescope=t;
  pointset=new PointSet();
}

Align::~Align() 
{
  delete(pointset);
}

void Align::Init()
{
  char *loadres=NULL;
  pointset->Init();
  loadres=pointset->LoadDataFile(IUFindText(AlignDataFileTP,"ALIGNDATAFILENAME")->text);
  if (loadres) {
    IDMessage(telescope->getDeviceName(), "Can not load Align Data File %s: %s", 
	      IUFindText(AlignDataFileTP,"ALIGNDATAFILENAME")->text, loadres);
    return;
  }
}


bool Align::initProperties()
{
    /* Load properties from the skeleton file */
    char skelPath[MAX_PATH_LENGTH];
    const char *skelFileName = "indi_align_sk.xml";
    snprintf(skelPath, MAX_PATH_LENGTH, "%s/%s", INDI_DATA_DIR, skelFileName);
    struct stat st;
    
    char *skel = getenv("INDISKEL");
    if (skel) 
      telescope->buildSkeleton(skel);
    else if (stat(skelPath,&st) == 0) 
      telescope->buildSkeleton(skelPath);
    else 
      IDLog("No skeleton file was specified. Set environment variable INDISKEL to the skeleton path and try again.\n"); 
    AlignDataFileTP=telescope->getText("ALIGNDATAFILE");
    AlignDataBP=telescope->getBLOB("ALIGNDATA");
    AlignPointNP=telescope->getNumber("ALIGNPOINT");
    AlignListSP=telescope->getSwitch("ALIGNLIST");
    AlignModeSP=telescope->getSwitch("ALIGNMODE");
    AlignTelescopeCoordsNP=telescope->getNumber("ALIGNTELESCOPECOORDS");
    AlignOptionsSP=telescope->getSwitch("ALIGNOPTIONS");

    return true;
}

void Align::ISGetProperties (const char *dev)
{
  //if (dev && (strcmp(dev, telescope->getDeviceName()))) return;
  if (telescope->isConnected())
    {
      telescope->defineText(AlignDataFileTP);
      telescope->defineBLOB(AlignDataBP);
      telescope->defineNumber(AlignPointNP);
      telescope->defineSwitch(AlignListSP);
      telescope->defineNumber(AlignTelescopeCoordsNP);
      telescope->defineSwitch(AlignOptionsSP);
      telescope->defineSwitch(AlignModeSP);
      } else {
      telescope->deleteProperty(AlignDataBP->name);
      telescope->deleteProperty(AlignPointNP->name);
      telescope->deleteProperty(AlignListSP->name);
      telescope->deleteProperty(AlignTelescopeCoordsNP->name);
      telescope->deleteProperty(AlignOptionsSP->name);
      telescope->deleteProperty(AlignModeSP->name);
      telescope->deleteProperty(AlignDataFileTP->name);
   }
  
}

bool Align::updateProperties ()
{
  if (telescope->isConnected())
    {
      telescope->defineText(AlignDataFileTP);
      telescope->defineBLOB(AlignDataBP);
      telescope->defineNumber(AlignPointNP);
      telescope->defineSwitch(AlignListSP);
      telescope->defineNumber(AlignTelescopeCoordsNP);
      telescope->defineSwitch(AlignOptionsSP);
      telescope->defineSwitch(AlignModeSP);
    } else {
      telescope->deleteProperty(AlignDataBP->name);
      telescope->deleteProperty(AlignPointNP->name);
      telescope->deleteProperty(AlignListSP->name);
      telescope->deleteProperty(AlignTelescopeCoordsNP->name);
      telescope->deleteProperty(AlignOptionsSP->name);
      telescope->deleteProperty(AlignModeSP->name);
      telescope->deleteProperty(AlignDataFileTP->name);
    }
  return true;
}

void Align::AlignNStar(double currentRA, double currentDEC, double *alignedRA, double *alignedDEC)
{
  
}

void Align::AlignNearest(double currentRA, double currentDEC, double *alignedRA, double *alignedDEC)
{

}

void Align::AlignGoto(double *gotoRA, double *gotoDEC)
{
  switch (GetAlignmentMode()) {
  case NONE:
  default:
    break;
  }
}

void Align::AlignSync(double lst, double jd, double targetRA, double targetDEC, double telescopeRA, double telescopeDEC)
{
  double values[6] = { lst, jd, targetRA, targetDEC, telescopeRA, telescopeDEC };
  const char *names[6] = {"ALIGNPOINT_SYNCTIME", "ALIGNPOINT_JD", "ALIGNPOINT_CELESTIAL_RA", "ALIGNPOINT_CELESTIAL_DE", 
			  "ALIGNPOINT_TELESCOPE_RA", "ALIGNPOINT_TELESCOPE_DE" };
  syncdata.lst = lst; syncdata.jd = jd; 
  syncdata.targetRA = targetRA;  syncdata.targetDEC = targetDEC;  
  syncdata.telescopeRA = telescopeRA;  syncdata.telescopeDEC = telescopeDEC;  
  pointset->AddPoint(syncdata);
  IDLog("Add sync point: %.8f %.8f %.8f %.8f %.8f\n", lst, targetRA, targetDEC, telescopeRA, telescopeDEC);
  IUUpdateNumber(AlignPointNP, values, (char **)names, 6);
  IDSetNumber(AlignPointNP, NULL);
}

Align::AlignmentMode Align::GetAlignmentMode() 
{
  ISwitch *sw;
  sw=IUFindOnSwitch(AlignModeSP);
  if (!sw) return NONE;
  if (!strcmp(sw->name,"NOALIGN")) {
    return NONE;;
  } else if (!strcmp(sw->name,"ALIGNSYNC")) {
    return SYNCS;
  } else if (!strcmp(sw->name,"ALIGNNEAREST")) {
    return NEAREST;
  } else if (!strcmp(sw->name,"ALIGNNSTAR")) {
    return NSTAR;
  } else return NONE;
}

void Align::GetAlignedCoords(double currentRA, double currentDEC, double *alignedRA, double *alignedDEC)
{
  double values[2] = {currentRA, currentDEC };
  const char *names[2] = {"ALIGNTELESCOPE_RA", "ALIGNTELESCOPE_DE" };
  IUUpdateNumber(AlignTelescopeCoordsNP, values, (char **)names, 2);
  IDSetNumber(AlignTelescopeCoordsNP, NULL);
  switch (GetAlignmentMode()) {
  case NSTAR:
    AlignNStar(currentRA, currentDEC, alignedRA, alignedDEC);
    break;
  case NEAREST:
    AlignNearest(currentRA, currentDEC, alignedRA, alignedDEC);
    break;
  case SYNCS:
    *alignedRA = currentRA; *alignedDEC = currentDEC;
    if (syncdata.lst != 0.0) {
      *alignedRA += (syncdata.targetRA - syncdata.telescopeRA);
      *alignedDEC += (syncdata.targetDEC - syncdata.telescopeDEC);
    }
    break;
  case NONE:
    *alignedRA = currentRA;
    *alignedDEC = currentDEC;
    break;
  default:
    *alignedRA = currentRA;
    *alignedDEC = currentDEC;
    break;
  }
}

bool Align::ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n)
{
  //  first check if it's for our device

  if(strcmp(dev,telescope->getDeviceName())==0)
    {
     if(strcmp(name, AlignPointNP->name)==0)
	{
	  AlignPointNP->s=IPS_OK;
	  IUUpdateNumber(AlignPointNP,values,names,n);
	  IDSetNumber(AlignPointNP,NULL);
	  return true;
	}
    }
    return true;
}

bool Align::ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n)
{

    //IDLog("Enter IsNewSwitch for %s\n",name);
    //for(int x=0; x<n; x++) {
    //    IDLog("Switch %s %d\n",names[x],states[x]);
    //}

    if(strcmp(dev,telescope->getDeviceName())==0)
    {   
      if(strcmp(name, AlignModeSP->name)==0)
	{
	  AlignModeSP->s=IPS_OK;
	  IUUpdateSwitch(AlignModeSP,states,names,n);
	  IDSetSwitch(AlignModeSP,NULL);
	  return true;
	}
      if(strcmp(name, AlignListSP->name)==0)
	{
	  ISwitch *sw;
	  IUUpdateSwitch(AlignListSP,states,names,n);
	  sw=IUFindOnSwitch(AlignListSP);
	  if (!strcmp(sw->name,"ALIGNLISTADD")) {
	    IDMessage(telescope->getDeviceName(), "Align: added point to list");;
	  } else if (!strcmp(sw->name,"ALIGNLISTCLEAR")) {
	    IDMessage(telescope->getDeviceName(), "Align: list cleared");;
	  }

	  AlignListSP->s=IPS_OK;
	  IUUpdateSwitch(AlignListSP,states,names,n);
	  IDSetSwitch(AlignListSP,NULL);
	  return true;
	}
    }

  return true;
}

bool Align::ISNewText (const char *dev, const char *name, char *texts[], char *names[], int n) 
{
  if(strcmp(dev,telescope->getDeviceName())==0)
    {
      if(strcmp(name, AlignDataFileTP->name)==0)
	{
	  char *loadres=NULL;
	  pointset->Reset();
	  IUUpdateText(AlignDataFileTP,texts,names,n);
	  loadres=pointset->LoadDataFile(IUFindText(AlignDataFileTP,"ALIGNDATAFILENAME")->text);
	  if (loadres) {
	    IDMessage(telescope->getDeviceName(), "Can not load Align Data File %s: %s", 
		      IUFindText(AlignDataFileTP,"ALIGNDATAFILENAME")->text, loadres);
	    AlignDataFileTP->s=IPS_ALERT;
	  } else 
	    AlignDataFileTP->s=IPS_OK;
	  IDSetText(AlignDataFileTP,NULL);
	  return true;
 	}

     }
  return true;
}

bool Align::ISNewBLOB(const char *dev, const char *name, int sizes[], int blobsizes[], char *blobs[], char *formats[], char *names[], int num)
{
  if(strcmp(dev,telescope->getDeviceName())==0)
    {      
    }

  return true;
}
