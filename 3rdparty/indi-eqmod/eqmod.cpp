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

#include <libnova.h>

#include <indicom.h>

#include "eqmod.h"
#include "eqmoderror.h"
#include "config.h"

#include "skywatcher.h"

#include <memory>

#define DEVICE_NAME "EQMod Mount"

// We declare an auto pointer to EQMod.
std::auto_ptr<EQMod> eqmod(0);

#define	GOTO_RATE	2				/* slew rate, degrees/s */
#define	SLEW_RATE	0.5				/* slew rate, degrees/s */
#define FINE_SLEW_RATE  0.1                             /* slew rate, degrees/s */
#define SID_RATE	0.004178			/* sidereal rate, degrees/s */



#define GOTO_LIMIT      5                               /* Move at GOTO_RATE until distance from target is GOTO_LIMIT degrees */
#define SLEW_LIMIT      2                               /* Move at SLEW_LIMIT until distance from target is SLEW_LIMIT degrees */
#define FINE_SLEW_LIMIT 0.5                             /* Move at FINE_SLEW_RATE until distance from target is FINE_SLEW_LIMIT degrees */

#define	POLLMS		250				/* poll period, ms */

#define GOTO_ITERATIVE_LIMIT 5     /* Max GOTO Iterations */
#define RAGOTORESOLUTION 5        /* GOTO Resolution in arcsecs */
#define DEGOTORESOLUTION 5        /* GOTO Resolution in arcsecs */

#define STELLAR_DAY 86164.098903691
#define TRACKRATE_SIDEREAL ((360.0 * 3600.0) / STELLAR_DAY)
#define SOLAR_DAY 86400
#define TRACKRATE_SOLAR ((360.0 * 3600.0) / SOLAR_DAY)
#define TRACKRATE_LUNAR 14.511415

#define RA_AXIS         0
#define DEC_AXIS        1
#define GUIDE_NORTH     0
#define GUIDE_SOUTH     1
#define GUIDE_WEST      0
#define GUIDE_EAST      1



void ISPoll(void *p);

void ISInit()
{
   static int isInit =0;

   if (isInit == 1)
       return;

    isInit = 1;
    if(eqmod.get() == 0) eqmod.reset(new EQMod());
    //IEAddTimer(POLLMS, ISPoll, NULL);

}

void ISGetProperties(const char *dev)
{
        ISInit();
        eqmod->ISGetProperties(dev);
}

void ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int num)
{
        ISInit();
        eqmod->ISNewSwitch(dev, name, states, names, num);
}

void ISNewText(	const char *dev, const char *name, char *texts[], char *names[], int num)
{
        ISInit();
        eqmod->ISNewText(dev, name, texts, names, num);
}

void ISNewNumber(const char *dev, const char *name, double values[], char *names[], int num)
{
        ISInit();
        eqmod->ISNewNumber(dev, name, values, names, num);
}

void ISNewBLOB (const char *dev, const char *name, int sizes[], int blobsizes[], char *blobs[], char *formats[], char *names[], int n)
{
  INDI_UNUSED(dev);
  INDI_UNUSED(name);
  INDI_UNUSED(sizes);
  INDI_UNUSED(blobsizes);
  INDI_UNUSED(blobs);
  INDI_UNUSED(formats);
  INDI_UNUSED(names);
  INDI_UNUSED(n);
}
void ISSnoopDevice (XMLEle *root)
{
    INDI_UNUSED(root);
}


EQMod::EQMod()
{
  //ctor
  currentRA=15;
  currentDEC=15;
  Parked=false;
  
  mount=new Skywatcher();
  pierside = WEST;
  RAInverted = DEInverted = false;
  bzero(&syncdata, sizeof(syncdata));

  //align=new Align(this);
  /* initialize random seed: */
  srand ( time(NULL) );
}

EQMod::~EQMod()
{
  //dtor
  if (mount) delete mount;
  mount = NULL;

}

const char * EQMod::getDefaultName()
{
    return (char *)DEVICE_NAME;
}

bool EQMod::initProperties()
{
    /* Make sure to init parent properties first */
    INDI::Telescope::initProperties();

    //IDLog("initProperties: connected=%d %s", (isConnected()?1:0), this->getDeviceName());

    //if (!align->initProperties()) return false;

    INDI::GuiderInterface::initGuiderProperties(this->getDeviceName(), MOTION_TAB);

    /* Add debug controls so we may debug driver if necessary */
    addDebugControl();
    
    return true;
}

void EQMod::ISGetProperties (const char *dev)
{
  //if (dev && (strcmp(dev, this->getDeviceName()))) return;
    /* First we let our parent populate */
    INDI::Telescope::ISGetProperties(dev);
    //IDMessage(dev,"ISGetProperties: connected=%d %s", (isConnected()?1:0), dev);

    //align->ISGetProperties(dev);
    return;
}

bool EQMod::updateProperties()
{
    INDI::Telescope::updateProperties();
    //IDMessage(this->getDeviceName(),"updateProperties: connected=%d %s", (isConnected()?1:0), this->getDeviceName());
    if (isConnected())
      {
	char skelPath[MAX_PATH_LENGTH];
	const char *skelFileName = "indi_eqmod_sk.xml";
	snprintf(skelPath, MAX_PATH_LENGTH, "%s/%s", INDI_DATA_DIR, skelFileName);
	struct stat st;
	unsigned int i;
	INumber *latitude=IUFindNumber(&LocationNV, "LAT");
	
	char *skel = getenv("INDISKEL");
	if (skel) 
	  buildSkeleton(skel);
	else if (stat(skelPath,&st) == 0) 
	  buildSkeleton(skelPath);
	else 
	  IDLog("No skeleton file was specified. Set environment variable INDISKEL to the skeleton path and try again.\n"); 
	
	GuideRateNP=getNumber("GUIDE_RATE");
	GuideRateN=GuideRateNP->np;
	
	MountInformationTP=getText("MOUNTINFORMATION");
	SteppersNP=getNumber("STEPPERS");
	PeriodsNP=getNumber("PERIODS");
	DateNP=getNumber("DATE");
	RAStatusLP=getLight("RASTATUS");
	DEStatusLP=getLight("DESTATUS");
	SlewSpeedsNP=getNumber("SLEWSPEEDS");
	HemisphereSP=getSwitch("HEMISPHERE");
	PierSideSP=getSwitch("PIERSIDE");
	TrackModeSP=getSwitch("TRACKMODE");
	TrackRatesNP=getNumber("TRACKRATES");
	//AbortMotionSP=getSwitch("TELESCOPE_ABORT_MOTION");
	HorizontalCoordsNP=getNumber("HORIZONTAL_COORDS");
	defineNumber(&GuideNSP);
	defineNumber(&GuideEWP);
	defineNumber(SlewSpeedsNP);
	defineNumber(GuideRateNP);
	defineText(MountInformationTP);
	defineNumber(SteppersNP);
	defineNumber(PeriodsNP);
	defineNumber(DateNP);
	defineLight(RAStatusLP);
	defineLight(DEStatusLP);
	defineSwitch(HemisphereSP);
	defineSwitch(TrackModeSP);
	defineNumber(TrackRatesNP);
	defineNumber(HorizontalCoordsNP);
	defineSwitch(PierSideSP);
	//defineSwitch(AbortMotionSP);
	try {
	  mount->InquireBoardVersion(MountInformationTP);
	}   
	catch(EQModError e) {
	  IDMessage(DEVICE_NAME, "%s", e.message);
	  Disconnect();
	  return false;
	}
	if (isDebug()) {
	  for (i=0;i<MountInformationTP->ntp;i++) 
	    IDLog("Got Board Property %s: %s\n", MountInformationTP->tp[i].name, MountInformationTP->tp[i].text);
	}
	
	mount->InquireRAEncoderInfo(SteppersNP);
	mount->InquireDEEncoderInfo(SteppersNP);
	if (isDebug()) {
	  for (i=0;i<SteppersNP->nnp;i++) 
	    IDLog("Got Encoder Property %s: %g\n", SteppersNP->np[i].label, SteppersNP->np[i].value);
	}
	
	mount->Init(&ParkSV);
	
	zeroRAEncoder=mount->GetRAEncoderZero();
	totalRAEncoder=mount->GetRAEncoderTotal();
	zeroDEEncoder=mount->GetDEEncoderZero();
	totalDEEncoder=mount->GetDEEncoderTotal();
	
	if ((latitude) && (latitude->value < 0.0)) SetSouthernHemisphere(true);
	else  SetSouthernHemisphere(false);
	
	if (ParkSV.sp[0].s==ISS_ON) {
	  //TODO unpark mount if desired
	}
	
	//TODO start tracking ? 
	TrackState=SCOPE_IDLE;
	
      }
    else
      {
	deleteProperty(GuideNSP.name);
	deleteProperty(GuideEWP.name);
	deleteProperty(GuideRateNP->name);
	deleteProperty(MountInformationTP->name);
	deleteProperty(SteppersNP->name);
	deleteProperty(PeriodsNP->name);
	deleteProperty(DateNP->name);
	deleteProperty(RAStatusLP->name);
	deleteProperty(DEStatusLP->name);
	deleteProperty(SlewSpeedsNP->name);
	deleteProperty(HemisphereSP->name);
	deleteProperty(TrackModeSP->name);
	deleteProperty(TrackRatesNP->name);
	deleteProperty(HorizontalCoordsNP->name);
	deleteProperty(PierSideSP->name);
	//deleteProperty(AbortMotionSP->name);
      }
    //if (!align->updateProperties()) return false;
    return true;
}


bool EQMod::Connect()
{
    bool rc=false;

    if(isConnected()) return true;

    rc=Connect(PortT[0].text);

    if(rc) {
 
        SetTimer(POLLMS);
}
    return rc;
}

bool EQMod::Connect(char *port)
{
  int i;
  ISwitchVectorProperty *connect=getSwitch("CONNECTION");
  INumber *latitude=IUFindNumber(&LocationNV, "LAT");
  if (connect) {
    connect->s=IPS_BUSY;
    IDSetSwitch(connect,"connecting to port %s",port);
  }
  try {
    mount->Connect(port);
    // Mount initialisation is in updateProperties as it sets directly Indi properties which should be defined 
  }
  catch(EQModError e) {
    IDMessage(DEVICE_NAME, "%s", e.message);
    Disconnect();
    return false;
  }

  //if (align) align->Init();
  IDMessage(DEVICE_NAME, "Successfully connected to EQMod Mount.");
  return true;
}

bool EQMod::Disconnect()
{
  try {
    mount->Disconnect();
  }
 catch(EQModError e) {
   IDMessage(DEVICE_NAME, "%s", e.message);
 }
  IDMessage(DEVICE_NAME, "Disconnected from EQMod Mount.");
  return true;
}

void EQMod::TimerHit()
{
  //IDLog("Telescope Timer Hit\n");
  if(isConnected())
    {
      bool rc;
      
      rc=ReadScopeStatus();
      //IDLog("TrackState after read is %d\n",TrackState);
      if(rc == false)
 	{
	  // read was not good
	  EqNV.s=IPS_ALERT;
	  IDSetNumber(&EqNV, NULL);
 	}
      
      SetTimer(POLLMS);
    }
}

bool EQMod::ReadScopeStatus() {
  static struct timeval ltv;
  struct timeval tv;
  double dt=0;

  /* update elapsed time since last poll, don't presume exactly POLLMS */
  // gettimeofday (&tv, NULL);
  
  //if (ltv.tv_sec == 0 && ltv.tv_usec == 0)
  //  ltv = tv;
  
  //dt = tv.tv_sec - ltv.tv_sec + (tv.tv_usec - ltv.tv_usec)/1e6;
  //ltv = tv;
  //TODO use dt to track mount desynchronisation/inactivity?
  
  // Time
  double juliandate=ln_get_julian_from_sys();
  double lst=ln_get_mean_sidereal_time(juliandate); // Greenwich mean (precession nut/obliq)  sidereal time of today's julian date
  double datevalues[2];
  char hrlst[12];
  const char *datenames[]={"LST", "JULIAN"};
  const char *piersidenames[]={"EAST", "WEST"};
  ISState piersidevalues[2];
  double periods[2];
  const char *periodsnames[]={"RAPERIOD", "DEPERIOD"};
  double horizvalues[2];
  const char *horiznames[2]={"AZ", "ALT"};

  lst+=(IUFindNumber(&LocationNV, "LONG")->value /15.0); // add longitude ha of observer
  lst=range24(lst);
  
  fs_sexa(hrlst, lst, 2, 360000);
  hrlst[11]='\0';
  if (isDebug()) IDMessage(DEVICE_NAME, "Compute local time: lst=%2.8f (%s) - julian date=%8.8f", lst, hrlst, juliandate); 
  //DateNP->s=IPS_BUSY;
  datevalues[0]=lst; datevalues[1]=juliandate;
  IUUpdateNumber(DateNP, datevalues, (char **)datenames, 2);
  DateNP->s=IPS_OK;
  IDSetNumber(DateNP, NULL); 
  try {
    currentRAEncoder=mount->GetRAEncoder();
    currentDEEncoder=mount->GetDEEncoder();
    if (isDebug()) IDMessage(DEVICE_NAME, "Current encoders RA=%ld DE=%ld", currentRAEncoder, currentDEEncoder);
    EncodersToRADec(currentRAEncoder, currentDEEncoder, lst, &currentRA, &currentDEC, &currentHA);
    alignedRA=currentRA; alignedDEC=currentDEC;
    if (align) 
      align->GetAlignedCoords(currentRA, currentDEC, &alignedRA, &alignedDEC);
    else {
      if (syncdata.lst != 0.0) {
	alignedRA += syncdata.deltaRA;
	alignedDEC += syncdata.deltaDEC;
      }
    }
    NewRaDec(alignedRA, alignedDEC);
    lnradec.ra =(alignedRA * 360.0) / 24.0;
    lnradec.dec =alignedDEC;
    ln_get_hrz_from_equ_sidereal_time(&lnradec, &lnobserver, lst, &lnaltaz);
    horizvalues[0]=lnaltaz.az;
    horizvalues[1]=lnaltaz.alt;
    IUUpdateNumber(HorizontalCoordsNP, horizvalues, (char **)horiznames, 2);
    IDSetNumber(HorizontalCoordsNP, NULL);

    pierside=SideOfPier(currentHA);
    if (pierside == EAST) {
      piersidevalues[0]=ISS_ON; piersidevalues[1]=ISS_OFF;
      IUUpdateSwitch(PierSideSP, piersidevalues, (char **)piersidenames, 2);
    } else {
      piersidevalues[0]=ISS_OFF; piersidevalues[1]=ISS_ON;
      IUUpdateSwitch(PierSideSP, piersidevalues, (char **)piersidenames, 2);
    }
    IDSetSwitch(PierSideSP, NULL);

    mount->GetRAMotorStatus(RAStatusLP);
    mount->GetDEMotorStatus(DEStatusLP);
    IDSetLight(RAStatusLP, NULL);
    IDSetLight(DEStatusLP, NULL);

    periods[0]=mount->GetRAPeriod();
    periods[1]=mount->GetDEPeriod();
    IUUpdateNumber(PeriodsNP, periods, (char **)periodsnames, 2);
    IDSetNumber(PeriodsNP, NULL);

    if (TrackState == SCOPE_SLEWING) {
      if (!(mount->IsRARunning()) && !(mount->IsDERunning())) {
	// Goto iteration
	gotoparams.iterative_count += 1;
	IDMessage(getDeviceName(), "Iterative Goto (%d): RA diff = %4.2f arcsecs DE diff = %4.2f arcsecs",
		      gotoparams.iterative_count, 3600 * fabs(gotoparams.ratarget - currentRA),  3600 * fabs(gotoparams.detarget - currentDEC));
        if ((gotoparams.iterative_count <= GOTO_ITERATIVE_LIMIT) &&
	    (((3600 * fabs(gotoparams.ratarget - currentRA)) > RAGOTORESOLUTION) || 
	     ((3600 * fabs(gotoparams.detarget - currentDEC)) > DEGOTORESOLUTION))) {
	  gotoparams.racurrent = currentRA; gotoparams.decurrent = currentDEC;
	  gotoparams.racurrentencoder = currentRAEncoder; gotoparams.decurrentencoder = currentDEEncoder;
	  EncoderTarget(&gotoparams);
	  try {    
	    // Start iterative slewing
	    IDMessage(DEVICE_NAME, "Iterative goto (%d): slew mount to RA increment = %ld, DE increment = %ld", gotoparams.iterative_count, 
		      gotoparams.ratargetencoder - gotoparams.racurrentencoder, gotoparams.detargetencoder - gotoparams.decurrentencoder);
	    mount->SlewTo(gotoparams.ratargetencoder - gotoparams.racurrentencoder, gotoparams.detargetencoder - gotoparams.decurrentencoder);
	  } catch(EQModError e) {
	    IDMessage(DEVICE_NAME, "%s", e.message);
	    return false;
	  }
	} else {
	  ISwitch *sw;
	  sw=IUFindSwitch(&CoordSV,"TRACK");
	  if ((gotoparams.iterative_count > GOTO_ITERATIVE_LIMIT) &&
	    (((3600 * abs(gotoparams.ratarget - currentRA)) > RAGOTORESOLUTION) || 
	     ((3600 * abs(gotoparams.detarget - currentDEC)) > DEGOTORESOLUTION))) {
	    IDMessage(getDeviceName(), "Iterative Goto Limit reached (%d iterations): RA diff = %4.2f arcsecs DE diff = %4.2f arcsecs",
		      gotoparams.iterative_count, 3600 * fabs(gotoparams.ratarget - currentRA),  3600 * fabs(gotoparams.detarget - currentDEC));
	  }
	  if ((RememberTrackState == SCOPE_TRACKING) || ((sw != NULL) && (sw->s == ISS_ON))) {
	    TrackState = SCOPE_TRACKING;
	    TrackModeSP->s=IPS_BUSY;
	    IDSetSwitch(TrackModeSP,NULL);
	    mount->StartRATracking(GetRATrackRate());
	    mount->StartDETracking(GetDETrackRate());
	    IDMessage(getDeviceName(), "Telescope slew is complete. Tracking...");
	  } else {
	    TrackState = SCOPE_IDLE;
	    IDMessage(getDeviceName(), "Telescope slew is complete. Stopping...");
	  }
	  EqNV.s = IPS_OK;

	}
      } 
    }
    
  } catch(EQModError e) {
      IDMessage(DEVICE_NAME, "%s", e.message);
      return false;
    }
  return true;

}


void EQMod::EncodersToRADec(unsigned long rastep, unsigned long destep, double lst, double *ra, double *de, double *ha) {
  double RACurrent=0.0, DECurrent=0.0, HACurrent=0.0;
  HACurrent=EncoderToHours(rastep, zeroRAEncoder, totalRAEncoder, Hemisphere);
  RACurrent=HACurrent+lst;
  DECurrent=EncoderToDegrees(destep, zeroDEEncoder, totalDEEncoder, Hemisphere);
  //IDLog("EncodersToRADec: destep=%6X zeroDEncoder=%6X totalDEEncoder=%6x DECurrent=%f\n", destep, zeroDEEncoder , totalDEEncoder, DECurrent);
  if (Hemisphere==NORTH) {
    if ((DECurrent > 90.0) && (DECurrent <= 270.0)) RACurrent = RACurrent - 12.0;
  }
  else
    if ((DECurrent <= 90.0) || (DECurrent > 270.0)) RACurrent = RACurrent + 12.0;
  HACurrent = rangeHA(HACurrent);
  RACurrent = range24(RACurrent);
  DECurrent = rangeDec(DECurrent); 
  *ra=RACurrent;
  *de=DECurrent;
  if (ha) *ha=HACurrent;
}

double EQMod::EncoderToHours(unsigned long step, unsigned long initstep, unsigned long totalstep, enum Hemisphere h) {
  double result=0.0;
  if (step > initstep) {
    result = ((double)(step - initstep) / totalstep) * 24.0;
    result = 24.0 - result;
  } else {
    result = ((double)(initstep - step) / totalstep) * 24.0;
  }
    
  if (h == NORTH)
    result = range24(result + 6.0);
  else
    result = range24((24 - result) + 6.0);
  return result;
} 

double EQMod::EncoderToDegrees(unsigned long step, unsigned long initstep, unsigned long totalstep, enum Hemisphere h)
{
  double result=0.0;
  if (step > initstep) {
    result = ((double)(step - initstep) / totalstep) * 360.0;
  } else {
    result = ((double)(initstep - step) / totalstep) * 360.0;
    result = 360.0 - result;
  }
  //IDLog("EncodersToDegrees: step=%6X initstep=%6x result=%f hemisphere %s \n", step, initstep, result, (h==NORTH?"North":"South"));    
  if (h == NORTH)
    result = range360(result);
  else
    result = range360(360.0 - result);
  //IDLog("EncodersToDegrees: returning result=%f\n", result);    

  return result;
} 

double EQMod::EncoderFromHour(double hour, unsigned long initstep, unsigned long totalstep, enum Hemisphere h)
{
  double shifthour=0.0;
  shifthour=range24(hour - 6);
  if (h == NORTH) 
    if (shifthour < 12.0)
      return (initstep - ((shifthour / 24.0) * totalstep));
    else
      return (initstep + (((24.0 - shifthour) / 24.0) * totalstep));
  else
    if (shifthour < 12.0)
      return (initstep + ((shifthour / 24.0) * totalstep));
    else
      return (initstep - (((24.0 - shifthour) / 24.0) * totalstep));
}

double EQMod::EncoderFromRA(double ratarget, double detarget, double lst, 
			    unsigned long initstep, unsigned long totalstep, enum Hemisphere h)
{
  double ha=0.0;
  ha = ratarget - lst;

  // used only in simulation??
  if (h == NORTH) 
    if ((detarget > 90.0) && (detarget <= 270.0)) ha = ha - 12.0;
  if (h == SOUTH)
    if ((detarget > 90.0) && (detarget <= 270.0)) ha = ha + 12.0;

  ha = range24(ha);
  return EncoderFromHour(ha, initstep, totalstep, h);
}

double EQMod::EncoderFromDegree(double degree, PierSide p, unsigned long initstep, unsigned long totalstep, enum Hemisphere h)
{
  double target = 0.0;
  target = degree;
  if (h == SOUTH)  target = 360.0 - target;
  if ((target > 180.0) && (p == EAST)) 
    return (initstep - (((360.0 - target) / 360.0) * totalstep));
  else 
    return (initstep + ((target / 360.0) * totalstep));
}
double EQMod::EncoderFromDec( double detarget, PierSide p, 
			      unsigned long initstep, unsigned long totalstep, enum Hemisphere h)
{
  double target=0.0;
  target = detarget;
  if (p == WEST) target = 180.0 - target;
  return EncoderFromDegree(target, p, initstep, totalstep, h);
}

double EQMod::rangeHA(double r) {
  double res = r;
  while (res< -12.0) res+=24.0;
  while (res>= 12.0) res-=24.0;
  return res;
}


double EQMod::range24(double r) {
  double res = r;
  while (res<0.0) res+=24.0;
  while (res>24.0) res-=24.0;
  return res;
}

double EQMod::range360(double r) {
  double res = r;
  while (res<0.0) res+=360.0;
  while (res>360.0) res-=360.0;
  return res;
}

double EQMod::rangeDec(double decdegrees) {
  if ((decdegrees >= 270.0) && (decdegrees <= 360.0))
    return (decdegrees - 360.0);
  if ((decdegrees >= 180.0) && (decdegrees < 270.0))
    return (180.0 - decdegrees);
  if ((decdegrees >= 90.0) && (decdegrees < 180.0))
    return (180.0 - decdegrees);
  return decdegrees;
}



void EQMod::SetSouthernHemisphere(bool southern) {
  const char *hemispherenames[]={"NORTH", "SOUTH"};
  ISState hemispherevalues[2];
  //IDLog("Set southern %s\n", (southern?"true":"false"));
  if (southern) Hemisphere=SOUTH;
  else Hemisphere=NORTH;
  RAInverted = (Hemisphere==SOUTH);
  DEInverted = ((Hemisphere==SOUTH) ^ (pierside==WEST));
  if (Hemisphere == NORTH) {
    hemispherevalues[0]=ISS_ON; hemispherevalues[1]=ISS_OFF;
    IUUpdateSwitch(HemisphereSP, hemispherevalues, (char **)hemispherenames, 2);
  } else {
    hemispherevalues[0]=ISS_OFF; hemispherevalues[1]=ISS_ON;
    IUUpdateSwitch(HemisphereSP, hemispherevalues, (char **)hemispherenames, 2);
  }
  IDSetSwitch(HemisphereSP, NULL);
}

EQMod::PierSide EQMod::SideOfPier(double ha) {
  double shiftha;
  shiftha=rangeHA(ha - 6.0);
  if (shiftha >= 0.0) return EAST; else return WEST; 
}

void EQMod::EncoderTarget(GotoParams *g)
{
  double r, d;
  double ha=0.0, targetra=0.0;
  double juliandate=ln_get_julian_from_sys();
  double lst=ln_get_mean_sidereal_time(juliandate);
  PierSide targetpier;
  unsigned long targetraencoder=0, targetdecencoder=0;
  bool outsidelimits=false;
  r = g->ratarget; d = g->detarget;
  
  lst+=(IUFindNumber(&LocationNV, "LONG")->value /15.0); // add longitude ha of observer
  lst=range24(lst);
  ha = rangeHA(r - lst);
  if (ha < 0.0) {// target EAST
    if (g->forcecwup) {
      if (Hemisphere == NORTH) targetpier = EAST; else targetpier = WEST;
      targetra = r;
    } else {
      if (Hemisphere == NORTH) targetpier = WEST; else targetpier = EAST;
      targetra = range24(r - 12.0);
    }
  } else {
    if (g->forcecwup) {
      if (Hemisphere == NORTH) targetpier = WEST; else targetpier = EAST;
      targetra = range24(r - 12.0);
    } else {
      if (Hemisphere == NORTH) targetpier = EAST; else targetpier = WEST;
      targetra = r;
    }
  }

  targetraencoder=EncoderFromRA(targetra, 0.0, lst, zeroRAEncoder, totalRAEncoder, Hemisphere);
  targetdecencoder=EncoderFromDec(d, targetpier, zeroDEEncoder, totalDEEncoder, Hemisphere);

  if ((g->forcecwup) && (g->checklimits)) {
    if (Hemisphere == NORTH) {
      if ((targetraencoder < g->limiteast) || (targetraencoder > g->limitwest)) outsidelimits=true;
    } else {
      if ((targetraencoder > g->limiteast) || (targetraencoder < g->limitwest)) outsidelimits=true;
    }
    if (outsidelimits) {
      IDMessage(DEVICE_NAME, "Goto: RA Limits prevent Counterweights-up slew.");
      if (ha < 0.0) {// target EAST
	if (Hemisphere == NORTH) targetpier = WEST; else targetpier = EAST;
	targetra = range24(r - 12.0);
      } else {
	if (Hemisphere == NORTH) targetpier = EAST; else targetpier = WEST;
	targetra = r;
      }
      targetraencoder=EncoderFromRA(targetra, 0.0, lst, zeroRAEncoder, totalRAEncoder, Hemisphere);
      targetdecencoder=EncoderFromDec(d, targetpier, zeroDEEncoder, totalDEEncoder, Hemisphere);
    }
  }
  g->outsidelimits = outsidelimits;
  g->ratargetencoder = targetraencoder;
  g->detargetencoder = targetdecencoder;
}

double EQMod::GetRATrackRate() 
{
  double rate = 0.0;
  ISwitch *sw;
  sw=IUFindOnSwitch(TrackModeSP);
  if (!sw) return 0.0;
  if (!strcmp(sw->name,"SIDEREAL")) {
    rate = TRACKRATE_SIDEREAL;
  } else if (!strcmp(sw->name,"LUNAR")) {
    rate = TRACKRATE_LUNAR;
  } else if (!strcmp(sw->name,"SOLAR")) {
    rate = TRACKRATE_SOLAR;
  } else if (!strcmp(sw->name,"CUSTOM")) {
    rate = IUFindNumber(TrackRatesNP, "RATRACKRATE")->value;
  } else return 0.0;
  if (RAInverted) rate=-rate;
  return rate;
}

double EQMod::GetDETrackRate() 
{
  double rate = 0.0;
  ISwitch *sw;
  sw=IUFindOnSwitch(TrackModeSP);
  if (!sw) return 0.0;
  if (!strcmp(sw->name,"SIDEREAL")) {
    rate = 0.0;
  } else if (!strcmp(sw->name,"LUNAR")) {
    rate = 0.0;
  } else if (!strcmp(sw->name,"SOLAR")) {
    rate = 0.0;
  } else if (!strcmp(sw->name,"CUSTOM")) {
    rate = IUFindNumber(TrackRatesNP, "DETRACKRATE")->value;
  } else return 0.0;
  if (DEInverted) rate=-rate;
  return rate;
}

bool EQMod::Goto(double r,double d)
{
    //IDLog("EQMod Goto\n");
    targetRA=r;
    targetDEC=d;
    char RAStr[64], DecStr[64];

    // Compute encoder targets and check RA limits if forced
    bzero(&gotoparams, sizeof(gotoparams));
    gotoparams.ratarget = r;  gotoparams.detarget = d;
    if (align) 
      align->AlignGoto(&gotoparams.ratarget, &gotoparams.detarget);
    else {
      if (syncdata.lst != 0.0) {
	gotoparams.ratarget -= syncdata.deltaRA;
	gotoparams.detarget -= syncdata.deltaDEC;
      }
    }
    gotoparams.racurrent = currentRA; gotoparams.decurrent = currentDEC;
    gotoparams.racurrentencoder = currentRAEncoder; gotoparams.decurrentencoder = currentDEEncoder;
    gotoparams.completed = false; 
    gotoparams.checklimits = true; 
    gotoparams.forcecwup = false; 
    gotoparams.outsidelimits = false; 
    gotoparams.limiteast = zeroRAEncoder - (totalRAEncoder / 4) - (totalRAEncoder / 24); // 13h
    gotoparams.limitwest = zeroRAEncoder + (totalRAEncoder / 4) + (totalRAEncoder / 24); // 23h
    EncoderTarget(&gotoparams);
    try {    
      // stop motor
      mount->StopRA();
      mount->StopDE();
      // Start slewing
      IDMessage(DEVICE_NAME, "Slew mount to RA increment = %ld, DE increment = %ld", 
		gotoparams.ratargetencoder - gotoparams.racurrentencoder, gotoparams.detargetencoder - gotoparams.decurrentencoder);
      mount->SlewTo(gotoparams.ratargetencoder - gotoparams.racurrentencoder, gotoparams.detargetencoder - gotoparams.decurrentencoder);

    } catch(EQModError e) {
      IDMessage(DEVICE_NAME, "%s", e.message);
      return false;
    }
    
    fs_sexa(RAStr, targetRA, 2, 3600);
    fs_sexa(DecStr, targetDEC, 2, 3600);

    Parked=false;
    RememberTrackState = TrackState;
    TrackState = SCOPE_SLEWING;

    //EqReqNV.s = IPS_BUSY;
    EqNV.s    = IPS_BUSY;

    TrackModeSP->s=IPS_IDLE;
    IDSetSwitch(TrackModeSP,NULL);

    IDMessage(getDeviceName(), "Slewing to RA: %s - DEC: %s", RAStr, DecStr);
    return true;
}

bool EQMod::canSync()
{
  return true;
}

bool EQMod::canPark()
{
  return false;
}

bool EQMod::Park()
{
    targetRA=0;
    targetDEC=90;
    Parked=true;
    TrackState = SCOPE_PARKING;
    IDMessage(getDeviceName(), "Parking telescope in progress...");
    return true;
}

bool EQMod::Sync(double ra,double dec)
{
  double juliandate=ln_get_julian_from_sys();
  double lst=ln_get_mean_sidereal_time(juliandate);

  if (TrackState != SCOPE_TRACKING) {
    //EqReqNV.s=IPS_IDLE;
    EqNV.s=IPS_IDLE;
    //IDSetNumber(&EqReqNV, NULL);
    IDSetNumber(&EqNV, NULL);
    IDMessage(getDeviceName(),"Syncs are allowed only when Tracking");
    return false;
  }
  lst+=(IUFindNumber(&LocationNV, "LONG")->value /15.0); // add longitude ha of observer
  lst=range24(lst);
  syncdata.lst=lst;
  syncdata.jd=juliandate;
  syncdata.targetRA=ra;
  syncdata.targetDEC=dec;
  try {
    EncodersToRADec(mount->GetRAEncoder(), mount->GetDEEncoder(), lst, &syncdata.telescopeRA, &syncdata.telescopeDEC, NULL);
  } catch(EQModError e) {
    IDMessage(DEVICE_NAME, "%s", e.message);
    return false;
  }
  syncdata.deltaRA = syncdata.targetRA - syncdata.telescopeRA;
  syncdata.deltaDEC= syncdata.targetDEC - syncdata.telescopeDEC;
  //EqReqNV.s=IPS_IDLE;
  //EqNV.s=IPS_OK;
  //IDSetNumber(&EqReqNV, NULL);
  if (align) align->AlignSync(syncdata.lst, syncdata.jd, syncdata.targetRA, syncdata.targetDEC, syncdata.telescopeRA, syncdata.telescopeDEC);
  IDMessage(getDeviceName(),"Mount Synced (deltaRA = %.6f deltaDEC = %.6f)", syncdata.deltaRA, syncdata.deltaDEC);
  return true;
}

bool EQMod::GuideNorth(float ms) {
  double rateshift=0.0;
  rateshift = TRACKRATE_SIDEREAL * IUFindNumber(GuideRateNP, "GUIDE_RATE_NS")->value;
  if (DEInverted) rateshift = -rateshift;
  if (ms > 0.0) {
    mount->StartDETracking(GetDETrackRate() + rateshift);
    GuideTimerNS = IEAddTimer((int)(ms * 1000.0), (IE_TCF *)timedguideNSCallback, this);
    IDMessage(getDeviceName(), "Timed guide North %d ms",(int)(ms * 1000.0));
  }
}

bool EQMod::GuideSouth(float ms) {
  double rateshift=0.0;
  rateshift = TRACKRATE_SIDEREAL * IUFindNumber(GuideRateNP, "GUIDE_RATE_NS")->value;
  if (ms > 0.0) {
    mount->StartDETracking(GetDETrackRate() - rateshift);
    GuideTimerNS = IEAddTimer((int)(ms * 1000.0), (IE_TCF *)timedguideNSCallback, this);
    IDMessage(getDeviceName(), "Timed guide South %d ms",(int)(ms * 1000.0));
  }
}

bool EQMod::GuideEast(float ms) {
  double rateshift=0.0;
  rateshift = TRACKRATE_SIDEREAL * IUFindNumber(GuideRateNP, "GUIDE_RATE_WE")->value;
  if (RAInverted) rateshift = -rateshift;
  if (ms > 0.0) {
    mount->StartRATracking(GetRATrackRate() - rateshift);
    GuideTimerWE = IEAddTimer((int)(ms * 1000.0), (IE_TCF *)timedguideWECallback, this);
    IDMessage(getDeviceName(), "Timed guide EAST %d ms",(int)(ms * 1000.0));
  }
}

bool EQMod::GuideWest(float ms) {
  double rateshift=0.0;
  rateshift = TRACKRATE_SIDEREAL * IUFindNumber(GuideRateNP, "GUIDE_RATE_WE")->value;
  if (RAInverted) rateshift = -rateshift;
  if (ms > 0.0) {
    mount->StartRATracking(GetRATrackRate() + rateshift);
    GuideTimerWE = IEAddTimer((int)(ms * 1000.0), (IE_TCF *)timedguideWECallback, this);
    IDMessage(getDeviceName(), "Timed guide West %d ms",(int)(ms * 1000.0));
  }
}

bool EQMod::ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n)
{
  //  first check if it's for our device

  if(strcmp(dev,getDeviceName())==0)
    {
      //  This is for our device
      //  Now lets see if it's something we process here
      
      if(strcmp(name,"SLEWSPEEDS")==0)
	{ 
	  unsigned int i;
	  /* TODO: don't change speed in gotos gotoparams.inprogress... */
	  if (TrackState != SCOPE_TRACKING) {
	    try {
	      for (int i=0; i<n; i++) {
		if (strcmp(names[i], "RASLEW") == 0) mount->SetRARate(values[i]);
		else if  (strcmp(names[i], "DESLEW") == 0) mount->SetDERate(values[i]);
	      }
	    } catch(EQModError e) {
	      IDMessage(DEVICE_NAME, "%s", e.message);
	      return false;
	    }
	  }
	  IUUpdateNumber(SlewSpeedsNP, values, names, n);
	  SlewSpeedsNP->s = IPS_OK;
	  IDSetNumber(SlewSpeedsNP, "Setting Slew rates - RA=%.2fx DE=%.2fx", 
		      IUFindNumber(SlewSpeedsNP,"RASLEW")->value, IUFindNumber(SlewSpeedsNP,"DESLEW")->value);
	  return true;
	}

      if(strcmp(name,"TRACKRATES")==0)
	{   
	  ISwitch *sw;
	  sw=IUFindOnSwitch(TrackModeSP);
	  if ((!sw) && (!strcmp(sw->name, "CUSTOM"))) { 
	    unsigned int i;
	    try {
	      for (int i=0; i<n; i++) {
		if (strcmp(names[i], "RATRACKRATE") == 0) mount->SetRARate(values[i] / SKYWATCHER_STELLAR_SPEED);
		else if  (strcmp(names[i], "DETRACKRATE") == 0) mount->SetDERate(values[i] / SKYWATCHER_STELLAR_SPEED);
	      }
	    } catch(EQModError e) {
	      IDMessage(DEVICE_NAME, "%s", e.message);
	      return false;
	    }
	  }
	  IUUpdateNumber(TrackRatesNP, values, names, n);
	  TrackRatesNP->s = IPS_OK;
	  IDSetNumber(TrackRatesNP, "Setting Custom Tracking Rates - RA=%.6f arcsec/s DE=%.6f arcsec/s", 
		      IUFindNumber(TrackRatesNP,"RATRACKRATE")->value, IUFindNumber(TrackRatesNP,"DETRACKRATE")->value);
	  return true;
	}

      // Guider interface
      if (!strcmp(name,GuideNSP.name) || !strcmp(name,GuideEWP.name))
	{
	  // Unless we're in track mode, we don't obey guide commands.
	  if (TrackState != SCOPE_TRACKING)
	    {
	      GuideNSP.s = IPS_IDLE;
	      IDSetNumber(&GuideNSP, NULL);
	      GuideEWP.s = IPS_IDLE;
	      IDSetNumber(&GuideEWP, NULL);
	      IDMessage(this->getDeviceName(), "Can not guide if not tracking.");
	      return true;
	    }

	  processGuiderProperties(name, values, names, n);
	 
	  return true;
	}

      if(strcmp(name,"GUIDE_RATE")==0)
	{
	  IUUpdateNumber(GuideRateNP, values, names, n);
	  GuideRateNP->s = IPS_OK;
	  IDSetNumber(GuideRateNP, NULL);
	  return true;
	}
      
      // Observer
      if(strcmp(name,"GEOGRAPHIC_COORD")==0)
	{
	  unsigned int i;
	  //don't return boolean! if (!INDI::Telescope::ISNewNumber(dev,name,values,names,n)) return false;
	  INDI::Telescope::ISNewNumber(dev,name,values,names,n);
	  for (i=0; i< n; i++) {
	    if (strcmp(names[i], "LONG")==0) lnobserver.lng =  values[i];
	    if (strcmp(names[i], "LAT")==0) {
	      lnobserver.lat =  values[i];
	      if (values[i] < 0.0) SetSouthernHemisphere(true); 
	      else SetSouthernHemisphere(false);
	    }
	  }   
	  IDMessage(DEVICE_NAME,"Changed observer: %f %f", lnobserver.lng, lnobserver.lat);
	  return true;
	}
      
    }
  
    //  if we didn't process it, continue up the chain, let somebody else
    //  give it a shot
    return INDI::Telescope::ISNewNumber(dev,name,values,names,n);
}

bool EQMod::ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n)
{

    //IDLog("Enter IsNewSwitch for %s\n",name);
    //for(int x=0; x<n; x++) {
    //    IDLog("Switch %s %d\n",names[x],states[x]);
    //}

    if(strcmp(dev,getDeviceName())==0)
    {      

      if(strcmp(name,"TRACKMODE")==0)
 	{  
	  ISwitch *swbefore, *swafter;
	  swbefore=IUFindOnSwitch(TrackModeSP);
	  IUUpdateSwitch(TrackModeSP,states,names,n);
	  swafter=IUFindOnSwitch(TrackModeSP);
	  IDMessage(getDeviceName(), "Track mode :  from %s to %s.", swbefore->name, swafter->name);
	  if (swbefore == swafter) {
	    if ( TrackState == SCOPE_TRACKING) {
	      TrackState = SCOPE_IDLE;
	      TrackModeSP->s=IPS_IDLE;
	      IDSetSwitch(TrackModeSP,NULL);
	      mount->StopRA();
	      mount->StopDE();
	      IDMessage(getDeviceName(), "Stop Tracking (%s).", swafter->name);
	    } else {
	      if (TrackState == SCOPE_IDLE) {
		TrackState = SCOPE_TRACKING;
		TrackModeSP->s=IPS_BUSY;
		IDSetSwitch(TrackModeSP,NULL);
		mount->StartRATracking(GetRATrackRate());
		mount->StartDETracking(GetDETrackRate());
		IDMessage(getDeviceName(), "Start Tracking (%s).", swafter->name);
	      } else {
		TrackModeSP->s=IPS_IDLE;
		IDSetSwitch(TrackModeSP,NULL);
		IDMessage(getDeviceName(), "Can not start Tracking (%s).", swafter->name);
	      }
	    }
	  } else {
	    if (TrackState == SCOPE_TRACKING) {
	      mount->StartRATracking(GetRATrackRate());
	      mount->StartDETracking(GetDETrackRate());
	    } else {
	      TrackModeSP->s=IPS_IDLE;
	      IDSetSwitch(TrackModeSP,NULL);
	    }
	    IDMessage(getDeviceName(), "Changing Track mode (from %s to %s).", swbefore->name, swafter->name);
	  }
	  return true;	
	}
    }

    //  Nobody has claimed this, so, ignore it
    return INDI::Telescope::ISNewSwitch(dev,name,states,names,n);
}

bool EQMod::ISNewText (const char *dev, const char *name, char *texts[], char *names[], int n) 
{

  if(strcmp(dev,getDeviceName())==0)
    {

    }
  if (align) align->ISNewText(dev,name,texts,names,n);
  //  Nobody has claimed this, so, ignore it
  return INDI::Telescope::ISNewText(dev,name,texts,names,n);
}

bool EQMod::MoveNS(TelescopeMotionNS dir)
{
  static int last_motion=-1;

  switch (dir)
    {
    case MOTION_NORTH:
      if (last_motion != MOTION_NORTH)  {
	double rate=IUFindNumber(SlewSpeedsNP, "DESLEW")->value;
	if (DEInverted) rate=-rate;
	mount->SlewDE(rate);
	last_motion = MOTION_NORTH;
	RememberTrackState = TrackState;
	IDMessage(getDeviceName(), "Starting North slew.");
      } else {
	mount->StopDE();
	last_motion=-1;
	if (RememberTrackState == SCOPE_TRACKING) {
	  TrackState = SCOPE_TRACKING;
	  mount->StartDETracking(GetDETrackRate());
	  IDMessage(getDeviceName(), "North Slew stopped. Restarting DE Tracking...");
	} else {
	  TrackState = SCOPE_IDLE;
	  IDMessage(getDeviceName(), "North Slew stopped");
	}
	IUResetSwitch(&MovementNSSP);
	MovementNSSP.s = IPS_IDLE;
	IDSetSwitch(&MovementNSSP, NULL);
      }
      break;
      
    case MOTION_SOUTH:
      if (last_motion != MOTION_SOUTH) {
	double rate=-(IUFindNumber(SlewSpeedsNP, "DESLEW")->value);
	if (DEInverted) rate=-rate;
	mount->SlewDE(rate);
	last_motion = MOTION_SOUTH;
	RememberTrackState = TrackState;
	IDMessage(getDeviceName(), "Starting South slew");
      } else {
	mount->StopDE();
	last_motion=-1;
	if (RememberTrackState == SCOPE_TRACKING) {
	    TrackState = SCOPE_TRACKING;
	    mount->StartDETracking(GetDETrackRate());
	    IDMessage(getDeviceName(), "South Slew stopped. Restarting DE Tracking...");
	  } else {
	    TrackState = SCOPE_IDLE;
	    IDMessage(getDeviceName(), "South Slew stopped.");
	  }
	IUResetSwitch(&MovementNSSP);
	MovementNSSP.s = IPS_IDLE;
	IDSetSwitch(&MovementNSSP, NULL);
      }
      break;
    }
  
  return true;
}

bool EQMod::MoveWE(TelescopeMotionWE dir)
{
    static int last_motion=-1;

    switch (dir)
    {
    case MOTION_WEST:
      if (last_motion != MOTION_WEST) {
	double rate=IUFindNumber(SlewSpeedsNP, "RASLEW")->value;
	if (RAInverted) rate=-rate;
	mount->SlewRA(rate);
	last_motion = MOTION_WEST;
	RememberTrackState = TrackState;
	IDMessage(getDeviceName(), "Starting West Slew");
      } else {
	mount->StopRA();
	last_motion=-1;
	if (RememberTrackState == SCOPE_TRACKING) {
	  TrackState = SCOPE_TRACKING;
	  mount->StartRATracking(GetRATrackRate());
	  IDMessage(getDeviceName(), "West Slew stopped. Restarting RA Tracking...");
	} else {
	  TrackState = SCOPE_IDLE;
	  IDMessage(getDeviceName(), "West Slew stopped");
	}
	IUResetSwitch(&MovementWESP);
	MovementWESP.s = IPS_IDLE;
	IDSetSwitch(&MovementWESP, NULL);
      }
      break;
      
    case MOTION_EAST:
      if (last_motion != MOTION_EAST) {
	double rate=-(IUFindNumber(SlewSpeedsNP, "RASLEW")->value);
	if (RAInverted) rate=-rate;
	mount->SlewRA(rate);
	last_motion = MOTION_EAST;
	RememberTrackState = TrackState;
	IDMessage(getDeviceName(), "Starting East Slew");
      } else {
	mount->StopRA();
	last_motion=-1;
	if (RememberTrackState == SCOPE_TRACKING) {
	  TrackState = SCOPE_TRACKING;
	  mount->StartRATracking(GetRATrackRate());
	  IDMessage(getDeviceName(), "East Slew stopped. Restarting RA Tracking...");
	} else {
	  TrackState = SCOPE_IDLE;
	  IDMessage(getDeviceName(), "East Slew stopped");
	}
	IUResetSwitch(&MovementWESP);
	MovementWESP.s = IPS_IDLE;
	IDSetSwitch(&MovementWESP, NULL);
      }
      break;
    }

    return true;

}

bool EQMod::Abort()
{
  mount->StopRA();
  mount->StopDE();
  //INDI::Telescope::Abort();
  // Reset switches
  GuideNSP.s = IPS_IDLE;
  IDSetNumber(&GuideNSP, NULL);
  GuideEWP.s = IPS_IDLE;
  IDSetNumber(&GuideEWP, NULL);
  TrackModeSP->s=IPS_IDLE;
  IUResetSwitch(TrackModeSP);
  IDSetSwitch(TrackModeSP,NULL);    

  if (MovementNSSP.s == IPS_BUSY)
    {
      IUResetSwitch(&MovementNSSP);
      MovementNSSP.s = IPS_IDLE;
      IDSetSwitch(&MovementNSSP, NULL);
    }
  
  if (MovementWESP.s == IPS_BUSY)
    {
      MovementWESP.s = IPS_IDLE;
      IUResetSwitch(&MovementWESP);
      IDSetSwitch(&MovementWESP, NULL);
    }
  
  if (ParkSV.s == IPS_BUSY)
    {
      ParkSV.s       = IPS_IDLE;
      IUResetSwitch(&ParkSV);
      IDSetSwitch(&ParkSV, NULL);
    }
  
  /*if (EqReqNV.s == IPS_BUSY)
    {
      EqReqNV.s      = IPS_IDLE;
      IDSetNumber(&EqReqNV, NULL);
    }
  */
  if (EqNV.s == IPS_BUSY)
    {
      EqNV.s = IPS_IDLE;
      IDSetNumber(&EqNV, NULL);
    }   

  TrackState=SCOPE_IDLE;

  AbortSV.s=IPS_OK;
  IUResetSwitch(&AbortSV);
  IDSetSwitch(&AbortSV, "Telescope Aborted");

  return true;
}

void EQMod::timedguideNSCallback(void *userpointer) {
  EQMod *p = ((EQMod *)userpointer);
  p->mount->StartDETracking(p->GetDETrackRate());
  p->GuideNSP.s = IPS_IDLE;
  //p->GuideNSN[GUIDE_NORTH].value = p->GuideNSN[GUIDE_SOUTH].value = 0;
  IDSetNumber(&(p->GuideNSP), NULL);
  IDMessage(p->getDeviceName(), "End Timed guide North/South");
  IERmTimer(p->GuideTimerNS);
}

void EQMod::timedguideWECallback(void *userpointer) {
  EQMod *p = ((EQMod *)userpointer);
  p->mount->StartRATracking(p->GetRATrackRate());
  p->GuideEWP.s = IPS_IDLE;
  //p->GuideWEN[GUIDE_WEST].value = p->GuideWEN[GUIDE_EAST].value = 0;
  IDSetNumber(&(p->GuideEWP), NULL);
  IDMessage(p->getDeviceName(), "End Timed guide West/East");
  IERmTimer(p->GuideTimerWE);
}
