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

#define MATRIX_LOG(name, in) \
  IDLog("Matrix %s:\n%g %g %g\n%g %g %g\n%g %g %g\n", name, in[0][0], in[0][1], in[0][2], in[1][0], in[1][1], in[1][2], in[2][0], in[2][1], in[2][2])

void inverse_matrix_3x3(double in[3][3], double out[3][3]) {
  double det;
  double a,b,c,d,e,f,g,h,i;
  a=in[0][0];b=in[0][1];c=in[0][2];
  d=in[1][0];e=in[1][1];f=in[1][2];
  g=in[2][0];h=in[2][1];i=in[2][2];
  det=(a*e*i)+(b*f*g)+(c*d*h)-(c*e*g)-(f*h*a)-(i*b*d);
  /* if (abs(det) < 0.000001) {
    IDLog("Align: Matrix determinant is lower than 0.000001 (%g)\n", det); 
    det=0.000001;
    }*/
  out[0][0]=(e*i - f*h) / det;
  out[0][1]=(c*h - b*i) / det;
  out[0][2]=(b*f - c*e) / det;
  out[1][0]=(f*g - d*i) / det;
  out[1][1]=(a*i - c*g) / det;
  out[1][2]=(c*d - a*f) / det;  
  out[2][0]=(d*h - e*g) / det;
  out[2][1]=(b*g - a*h) / det;
  out[2][2]=(a*e - b*d) / det;
} 

void mult_matrix_3x3(double in1[3][3], double in2[3][3], double out[3][3]) {
  for (int i=0; i < 3; i++) {
    for (int j=0; j < 3; j++) {
      out[i][j] = 0.0;
      for (int k=0; k < 3; k++)
	out[i][j] += in1[i][k] * in2[k][j];
    }
  }
}

// We declare an auto pointer to Align.
//std::auto_ptr<Align> align(0);

Align::Align(INDI::Telescope *t) 
{
  telescope=t;
  pointset=new PointSet(t);
  alignReady = false;
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
    /*
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
    */
    return true;
}

void Align::ISGetProperties (const char *dev)
{
  //if (dev && (strcmp(dev, telescope->getDeviceName()))) return;
  /* if (telescope->isConnected())
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
  */
}

bool Align::updateProperties ()
{
  /*IDLog("Align update properties connected = %d.\n",(telescope->isConnected()?1:0) ); */
  if (telescope->isConnected())
    {
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
      telescope->defineText(AlignDataFileTP);
      telescope->defineBLOB(AlignDataBP);
      telescope->defineNumber(AlignPointNP);
      telescope->defineSwitch(AlignListSP);
      telescope->defineNumber(AlignTelescopeCoordsNP);
      telescope->defineSwitch(AlignOptionsSP);
      telescope->defineSwitch(AlignModeSP);
    Init();
    alignReady = true;
    } else {
    if (AlignDataBP) {
      telescope->deleteProperty(AlignDataBP->name);
      telescope->deleteProperty(AlignPointNP->name);
      telescope->deleteProperty(AlignListSP->name);
      telescope->deleteProperty(AlignTelescopeCoordsNP->name);
      telescope->deleteProperty(AlignOptionsSP->name);
      telescope->deleteProperty(AlignModeSP->name);
      telescope->deleteProperty(AlignDataFileTP->name);
      AlignDataFileTP=NULL;
      AlignDataBP=NULL;
      AlignPointNP=NULL;
      AlignListSP=NULL;
      AlignModeSP=NULL;
      AlignTelescopeCoordsNP=NULL;
      AlignOptionsSP=NULL;
      alignReady = false;
    }
    }


  return true;
}

void Align::AlignNStar(double lst, double currentRA, double currentDEC, double *alignedRA, double *alignedDEC, bool ingoto)
{
  //double pointaz = (pointset->range24(lst - currentRA - 12.0) * 360.0) / 24.0;
  //double pointalt = currentDEC + pointset->lat;
  double pointaz, pointalt;
  std::set<PointSet::Distance, bool (*)(PointSet::Distance, PointSet::Distance)> *sortedpoints;
  pointset->AltAzFromRaDec(currentRA, currentDEC, lst, &pointalt, &pointaz, NULL);
  sortedpoints=pointset->ComputeDistances(pointalt, pointaz, PointSet::None);
  if (sortedpoints->size() < 2) {
    //IDLog("AlignNstar: only %d points in set - using Nearest mode\n", sortedpoints->size());
    AlignNearest(lst, currentRA, currentDEC, alignedRA, alignedDEC, ingoto);
  } else {
    /* Taki's Algorithm (p33): http://www.geocities.jp/toshimi_taki/matrix/matrix_method_rev_e.pdf */
    std::set<PointSet::Distance>::iterator it = sortedpoints->begin();
    PointSet::Point *point = pointset->getPoint(it->htmID);
    double celestialMatrix[3][3];
    double invcelestialMatrix[3][3];
    double telescopeMatrix[3][3];
    double T[3][3];
    double invT[3][3];
    double l, m, n;
    double L, M, N;
    int max=(sortedpoints->size() == 2)?2:3;

    for (int i=0; i < max; i++) {
      //IDLog("Align NStar: align point %d htm = %s, telescope alt=%g az=%g\n", i, point->htmname, point->telescopeALT, point->telescopeAZ);
      /*
      celestialMatrix[0][i]=cos(point->aligndata.targetDEC * M_PI / 180.0) * 
	cos(((pointset->range24(point->aligndata.targetRA - point->aligndata.lst) * 360) / 24.0) * M_PI / 180.0); 
      celestialMatrix[1][i]=cos(point->aligndata.targetDEC * M_PI / 180.0) * 
	sin(((pointset->range24(point->aligndata.targetRA - point->aligndata.lst) * 360) / 24.0) * M_PI / 180.0); 
      celestialMatrix[2][i]=sin(point->aligndata.targetDEC * M_PI / 180.0);
      telescopeMatrix[0][i]=cos(point->aligndata.telescopeDEC * M_PI / 180.0) * 
	cos(((pointset->range24(point->aligndata.telescopeRA - point->aligndata.lst) * 360) / 24.0) * M_PI / 180.0);
      telescopeMatrix[1][i]=cos(point->aligndata.telescopeDEC * M_PI / 180.0) * 
	sin(((pointset->range24(point->aligndata.telescopeRA - point->aligndata.lst) * 360) / 24.0) * M_PI / 180.0);
      telescopeMatrix[2][i]=sin(point->aligndata.telescopeDEC * M_PI / 180.0); 
      */
      celestialMatrix[0][i]=cos(point->celestialALT * M_PI / 180.0) * 
	cos(point->celestialAZ * M_PI / 180.0); 
      celestialMatrix[1][i]=cos(point->celestialALT * M_PI / 180.0) * 
	sin(point->celestialAZ * M_PI / 180.0); 
      celestialMatrix[2][i]=sin(point->celestialALT * M_PI / 180.0);
      telescopeMatrix[0][i]=cos(point->telescopeALT * M_PI / 180.0) * 
	cos(point->telescopeAZ  * M_PI / 180.0);
      telescopeMatrix[1][i]=cos(point->telescopeALT * M_PI / 180.0) * 
	sin(point->telescopeAZ  * M_PI / 180.0);
      telescopeMatrix[2][i]=sin(point->telescopeALT * M_PI / 180.0); 
      it++;
      point = pointset->getPoint(it->htmID);
    }

    if (sortedpoints->size() == 2) {
      /* compute vector product of the two points */
      /* and insert it in the set */
      double norm=1.0;
      celestialMatrix[0][2]=celestialMatrix[1][0] * celestialMatrix[2][1] - celestialMatrix[2][0] * celestialMatrix[1][1];
      celestialMatrix[1][2]=celestialMatrix[2][0] * celestialMatrix[0][1] - celestialMatrix[0][0] * celestialMatrix[2][1];
      celestialMatrix[2][2]=celestialMatrix[0][0] * celestialMatrix[1][1] - celestialMatrix[1][0] * celestialMatrix[0][1];
      norm=sqrt(celestialMatrix[0][2]*celestialMatrix[0][2] + celestialMatrix[1][2]*celestialMatrix[1][2] + celestialMatrix[2][2]*celestialMatrix[2][2]);
      celestialMatrix[0][2]=celestialMatrix[0][2]/norm;celestialMatrix[1][2]=celestialMatrix[1][2]/norm;celestialMatrix[2][2]=celestialMatrix[2][2]/norm;
       
      telescopeMatrix[0][2]=telescopeMatrix[1][0] * telescopeMatrix[2][1] - telescopeMatrix[2][0] * telescopeMatrix[1][1];
      telescopeMatrix[1][2]=telescopeMatrix[2][0] * telescopeMatrix[0][1] - telescopeMatrix[0][0] * telescopeMatrix[2][1];
      telescopeMatrix[2][2]=telescopeMatrix[0][0] * telescopeMatrix[1][1] - telescopeMatrix[1][0] * telescopeMatrix[0][1];
      norm=sqrt(telescopeMatrix[0][2]*telescopeMatrix[0][2] + telescopeMatrix[1][2]*telescopeMatrix[1][2] + telescopeMatrix[2][2]*telescopeMatrix[2][2]);
      telescopeMatrix[0][2]=telescopeMatrix[0][2]/norm;telescopeMatrix[1][2]=telescopeMatrix[1][2]/norm;telescopeMatrix[2][2]=telescopeMatrix[2][2]/norm;
      
    } 
    //MATRIX_LOG("celestialMatrix", celestialMatrix);
    //MATRIX_LOG("telescopeMatrix", telescopeMatrix);
    inverse_matrix_3x3(celestialMatrix, invcelestialMatrix);
    //MATRIX_LOG("invcelestialMatrix", invcelestialMatrix);
    mult_matrix_3x3(telescopeMatrix, invcelestialMatrix, T);
    inverse_matrix_3x3(T, invT);
    //MATRIX_LOG("T", T);
    //MATRIX_LOG("invT", invT);
    if (!(ingoto)) {
      double alignedalt, alignedaz;
      /*
      l = cos(currentDEC * M_PI / 180.0) * cos(((pointset->range24(currentRA - lst) * 360) / 24.0) * M_PI / 180.0);
      m = cos(currentDEC * M_PI / 180.0) * sin(((pointset->range24(currentRA - lst) * 360) / 24.0) * M_PI / 180.0);
      n = sin(currentDEC * M_PI / 180.0);
      */
      l = cos(pointalt * M_PI / 180.0) * cos(pointaz * M_PI / 180.0);
      m = cos(pointalt * M_PI / 180.0) * sin(pointaz * M_PI / 180.0);
      n = sin(pointalt * M_PI / 180.0);      

      L = invT[0][0] * l + invT[0][1] * m + invT[0][2] * n;
      M = invT[1][0] * l + invT[1][1] * m + invT[1][2] * n;
      N = invT[2][0] * l + invT[2][1] * m + invT[2][2] * n;

      /*
      *alignedRA = atan(M/L) * 12.0 / M_PI;
      if (L < 0) *alignedRA += 12.0;
      if (*alignedRA < 0) *alignedRA = 24.0 + *alignedRA;
      
      *alignedDEC = asin(N) * 180.0 / M_PI;
      */

      alignedaz = atan(M/L) * 180.0 / M_PI;
      if (L < 0) alignedaz += 180.0;
      if (alignedaz < 0) alignedaz = 360.0 + alignedaz;
      
      alignedalt = asin(N) * 180.0 / M_PI;
      /* julian date is taken now, should be lst */
      pointset->RaDecFromAltAz(alignedalt, alignedaz, ln_get_julian_from_sys(), alignedRA, alignedDEC, NULL);


    } else {
      double alignedalt, alignedaz;
      /*
      L = cos(currentDEC * M_PI / 180.0) * cos(((pointset->range24(currentRA - lst) * 360) / 24.0) * M_PI / 180.0);
      M = cos(currentDEC * M_PI / 180.0) * sin(((pointset->range24(currentRA - lst) * 360) / 24.0) * M_PI / 180.0);
      N = sin(currentDEC * M_PI / 180.0);
      */
      L = cos(pointalt * M_PI / 180.0) * cos(pointaz * M_PI / 180.0);
      M = cos(pointalt * M_PI / 180.0) * sin(pointaz * M_PI / 180.0);
      N = sin(pointalt * M_PI / 180.0);      


      l = T[0][0] * L + T[0][1] * M + T[0][2] * N;
      m = T[1][0] * L + T[1][1] * M + T[1][2] * N;
      n = T[2][0] * L + T[2][1] * M + T[2][2] * N;
      /*
      *alignedRA = atan(m/l) * 12.0 / M_PI;
      if (l < 0) *alignedRA += 12.0;
      if (*alignedRA < 0) *alignedRA = 24.0 + *alignedRA;
      
      *alignedDEC = asin(n) * 180.0 / M_PI;	
      */
      alignedaz = atan(m/l) * 180.0 / M_PI;
      if (l < 0) alignedaz += 180.0;
      if (alignedaz < 0) alignedaz = 360.0 + alignedaz;
      
      alignedalt = asin(n) * 180.0 / M_PI;
      /* julian date is taken now, should be lst */
      pointset->RaDecFromAltAz(alignedalt, alignedaz, ln_get_julian_from_sys(), alignedRA, alignedDEC, NULL);
      IDMessage(telescope->getDeviceName(), "GOTO ALign NStar: delta RA = %f, delta DEC  = %f\n", (*alignedRA - currentRA), (*alignedDEC - currentDEC));

    }
    //IDLog("ALign NStar: delta RA = %f, delta DEC = %f\n", (*alignedRA - currentRA), (*alignedDEC - currentDEC));
  }
}

void Align::AlignNearest(double lst, double currentRA, double currentDEC, double *alignedRA, double *alignedDEC, bool ingoto)
{
  //double pointaz = (pointset->range24(lst - currentRA - 12.0) * 360.0) / 24.0;
  //double pointalt = currentDEC + pointset->lat;
  double pointaz, pointalt;
  std::set<PointSet::Distance, bool (*)(PointSet::Distance, PointSet::Distance)> *sortedpoints;
  pointset->AltAzFromRaDec(currentRA, currentDEC, lst, &pointalt, &pointaz, NULL);
  sortedpoints=pointset->ComputeDistances(pointalt, pointaz, PointSet::None);
  if (sortedpoints->empty()) {
    *alignedRA = currentRA;
    *alignedDEC = currentDEC;
    //IDLog("AlignNearest: empty set\n");
  } else {
    PointSet::Point *point = pointset->getPoint(sortedpoints->begin()->htmID);
    *alignedRA = currentRA;
    *alignedDEC = currentDEC;
    if (!(ingoto)) {
      *alignedRA += (point->aligndata.targetRA - point->aligndata.telescopeRA);
      *alignedDEC += (point->aligndata.targetDEC - point->aligndata.telescopeDEC);
    } else {
      *alignedRA -= (point->aligndata.targetRA - point->aligndata.telescopeRA);
      *alignedDEC -= (point->aligndata.targetDEC - point->aligndata.telescopeDEC);
      IDMessage(telescope->getDeviceName(), "GOTO ALign Nearest: delta RA = %f, delta DEC  = %f\n", (*alignedRA - currentRA), (*alignedDEC - currentDEC));
    }
    //IDLog("ALign Nearest: align point %s telescope alt = %f, az =%f\n", point->htmname, point->telescopeALT, point->telescopeAZ);
    //IDLog("ALign Nearest: delta RA = %c %f, delta DEC = %c %f\n", (ingoto?'-':'+'), (point->aligndata.targetRA - point->aligndata.telescopeRA), 
    //  (ingoto?'-':'+'), (point->aligndata.targetDEC - point->aligndata.telescopeDEC));
  }
}

void Align::AlignGoto(double lst, double *gotoRA, double *gotoDEC)
{
  ISwitch *aligngotosw;
  aligngotosw=IUFindSwitch(AlignOptionsSP,"ALIGNONGOTO");
  if (aligngotosw->s == ISS_ON) {
    switch (GetAlignmentMode()) {
    case SYNCS:
      *gotoRA -= (syncdata.targetRA - syncdata.telescopeRA);
      *gotoDEC -= (syncdata.targetDEC - syncdata.telescopeDEC);
      break;
    case NEAREST:
      AlignNearest(lst, *gotoRA, *gotoDEC, gotoRA, gotoDEC, true);
      break;
    case NSTAR:
      AlignNStar(lst, *gotoRA, *gotoDEC, gotoRA, gotoDEC, true);
      break;
    case NONE:
    default:
      break;
    }
  }
}

void Align::AlignSync(double lst, double jd, double targetRA, double targetDEC, double telescopeRA, double telescopeDEC)
{
  double values[6] = { lst, jd, targetRA, targetDEC, telescopeRA, telescopeDEC };
  const char *names[6] = {"ALIGNPOINT_SYNCTIME", "ALIGNPOINT_JD", "ALIGNPOINT_CELESTIAL_RA", "ALIGNPOINT_CELESTIAL_DE", 
			  "ALIGNPOINT_TELESCOPE_RA", "ALIGNPOINT_TELESCOPE_DE" };
  ISwitch *alignsyncsw;
  syncdata.lst = lst; syncdata.jd = jd; 
  syncdata.targetRA = targetRA;  syncdata.targetDEC = targetDEC;  
  syncdata.telescopeRA = telescopeRA;  syncdata.telescopeDEC = telescopeDEC;  
  IDLog("AlignSync \n");
  // add point on sync
  alignsyncsw=IUFindSwitch(AlignOptionsSP,"ADDONSYNC");
  if (alignsyncsw->s == ISS_ON) {
    pointset->AddPoint(syncdata, NULL);
    IDLog(" Add sync point: %.8f %.8f %.8f %.8f %.8f\n", lst, targetRA, targetDEC, telescopeRA, telescopeDEC);
  }
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

void Align::GetAlignedCoords(double lst, double currentRA, double currentDEC, double *alignedRA, double *alignedDEC)
{
  double values[2] = {currentRA, currentDEC };
  const char *names[2] = {"ALIGNTELESCOPE_RA", "ALIGNTELESCOPE_DE" };
  IUUpdateNumber(AlignTelescopeCoordsNP, values, (char **)names, 2);
  IDSetNumber(AlignTelescopeCoordsNP, NULL);
  switch (GetAlignmentMode()) {
  case NSTAR:
    AlignNStar(lst, currentRA, currentDEC, alignedRA, alignedDEC, false);
    break;
  case NEAREST:    
    AlignNearest(lst, currentRA, currentDEC, alignedRA, alignedDEC, false);
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
     if(AlignPointNP && strcmp(name, AlignPointNP->name)==0)
	{
	  AlignPointNP->s=IPS_OK;
	  IUUpdateNumber(AlignPointNP,values,names,n);
	  IDSetNumber(AlignPointNP,NULL);
	  return true;
	}
    }
    return false;
}

bool Align::ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n)
{

  /*
    IDLog("Align: Enter IsNewSwitch for %s\n",name);
    for(int x=0; x<n; x++) {
        IDLog("Align: Switch %s %d\n",names[x],states[x]);
    }
  */
    if(strcmp(dev,telescope->getDeviceName())==0)
    {   
      if(AlignModeSP && strcmp(name, AlignModeSP->name)==0)
	{
	  ISwitch *sw;
	  AlignModeSP->s=IPS_OK;
	  IUUpdateSwitch(AlignModeSP,states,names,n);
	  sw=IUFindOnSwitch(AlignModeSP);
	  IDSetSwitch(AlignModeSP,"Alignment mode set to %s", sw->label);
	  return true;
	}

      if(AlignOptionsSP && strcmp(name, AlignOptionsSP->name)==0)
	{
	  //ISwitch *sw;
	  AlignOptionsSP->s=IPS_OK;
	  IUUpdateSwitch(AlignOptionsSP,states,names,n);
	  for (int i=0; i < n; i++)
	    IDLog("AlignOptions Switch %s %d\n", names[i], states[i]);
	  //sw=IUFindOnSwitch(AlignModeSP);
	  IDSetSwitch(AlignOptionsSP, NULL);
	  return true;
	}
      if(AlignListSP && strcmp(name, AlignListSP->name)==0)
	{
	  ISwitch *sw;
	  IUUpdateSwitch(AlignListSP,states,names,n);
	  sw=IUFindOnSwitch(AlignListSP);
	  if (!strcmp(sw->name,"ALIGNLISTADD")) {
	    pointset->AddPoint(syncdata, NULL);
	    IDMessage(telescope->getDeviceName(), "Align: added point to list");;
	  } else if (!strcmp(sw->name,"ALIGNLISTCLEAR")) {
	    pointset->Reset();
	    IDMessage(telescope->getDeviceName(), "Align: list cleared");;
	  } else if (!strcmp(sw->name,"ALIGNWRITEFILE")) {
	    char *res;
	    res=pointset->WriteDataFile(IUFindText(AlignDataFileTP,"ALIGNDATAFILENAME")->text);
	    if (res)
	       IDMessage(telescope->getDeviceName(), "Can not save Align Data to file %s: %s", 
			 IUFindText(AlignDataFileTP,"ALIGNDATAFILENAME")->text, res);
	    else
	      IDMessage(telescope->getDeviceName(), "Align: Data saved in file %s", IUFindText(AlignDataFileTP,"ALIGNDATAFILENAME")->text);
	  } else if (!strcmp(sw->name,"ALIGNLOADFILE")) {
	    char *res;
	    pointset->Reset();
	    res=pointset->LoadDataFile(IUFindText(AlignDataFileTP,"ALIGNDATAFILENAME")->text);
	    if (res) 
	      IDMessage(telescope->getDeviceName(), "Can not load Align Data File %s: %s", 
			IUFindText(AlignDataFileTP,"ALIGNDATAFILENAME")->text, res);
	    else
	      IDMessage(telescope->getDeviceName(), "Align: Data loaded from file %s", IUFindText(AlignDataFileTP,"ALIGNDATAFILENAME")->text);
	  }

	  AlignListSP->s=IPS_OK;
	  IUUpdateSwitch(AlignListSP,states,names,n);
	  IDSetSwitch(AlignListSP,NULL);
	  return true;
	}
    }

  return false;
}

bool Align::ISNewText (const char *dev, const char *name, char *texts[], char *names[], int n) 
{
  if (strcmp(dev,telescope->getDeviceName())==0)
    {
      if (AlignDataFileTP && (strcmp(name, AlignDataFileTP->name)==0))
	{
	  /*	  char *loadres=NULL;
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
	  */
	  IUUpdateText(AlignDataFileTP,texts,names,n);
	  IDSetText(AlignDataFileTP,NULL);
	  return true;
 	}

     }
  return false;
}

bool Align::ISNewBLOB(const char *dev, const char *name, int sizes[], int blobsizes[], char *blobs[], char *formats[], char *names[], int num)
{
  if(strcmp(dev,telescope->getDeviceName())==0)
    {      
    }

  return false;
}
