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

#include "logger/Logger.h"


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

/* Preset Slew Speeds */
#define SLEWMODES 11
double slewspeeds[SLEWMODES - 1] = { 1.0, 2.0, 4.0, 8.0, 32.0, 64.0, 128.0, 200.0, 400.0, 800.0 };
double defaultspeed=64.0;

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
  setVersion(EQMOD_VERSION_MAJOR, EQMOD_VERSION_MINOR);
  currentRA=15;
  currentDEC=15;
  Parked=false;

#ifdef WITH_LOGGER
  DEBUG_CONF("/tmp/indi_eqmod_telescope",  Logger::file_on|Logger::screen_on, Logger::defaultlevel, Logger::defaultlevel);
#endif


  mount=new Skywatcher(this);

  pierside = EAST;
  RAInverted = DEInverted = false;
  bzero(&syncdata, sizeof(syncdata));

#ifdef WITH_ALIGN_GEEHALEL
  align=new Align(this);
#endif

#ifdef WITH_SIMULATOR
  simulator=new EQModSimulator(this);
#endif

  /* initialize random seed: */
  srand ( time(NULL) );
}

EQMod::~EQMod()
{
  //dtor
  if (mount) delete mount;
  mount = NULL;

}

void EQMod::setLogDebug (bool enable) 
{
  
  INDI::Telescope::setDebug(enable);
  if (not Logger::updateProperties(enable, this)) 
    DEBUG(Logger::DBG_WARNING,"setLogDebug: Logger error");
  //if (mount) mount->setDebug(enable);

}
#ifdef WITH_SIMULATOR
void EQMod::setStepperSimulation (bool enable) 
{
  if ((enable && !isSimulation()) || (!enable && isSimulation())) {
    mount->setSimulation(enable); 
    if (not simulator->updateProperties(enable)) 
      DEBUG(Logger::DBG_WARNING,"setStepperSimulator: Disable/Enable error");
  }
  INDI::Telescope::setSimulation(enable);
}
#endif

const char * EQMod::getDefaultName()
{
    return (char *)DEVICE_NAME;
}

double EQMod::getLongitude() 
{
  return(IUFindNumber(&LocationNV, "LONG")->value);
}

double EQMod::getLatitude() 
{
  return(IUFindNumber(&LocationNV, "LAT")->value);
}

bool EQMod::initProperties()
{
    /* Make sure to init parent properties first */
    INDI::Telescope::initProperties();

    //IDLog("initProperties: connected=%d %s", (isConnected()?1:0), this->getDeviceName());

    if (align) {
    if (!align->initProperties()) return false;
    }

    INDI::GuiderInterface::initGuiderProperties(this->getDeviceName(), MOTION_TAB);

    /* Add debug controls so we may debug driver if necessary */
    addDebugControl();
#ifdef WITH_SIMULATOR
    addSimulationControl();
#endif
    return true;
}

void EQMod::ISGetProperties (const char *dev)
{
  //if (dev && (strcmp(dev, this->getDeviceName()))) return;
    /* First we let our parent populate */
    INDI::Telescope::ISGetProperties(dev);
    //IDMessage(dev,"ISGetProperties: connected=%d %s", (isConnected()?1:0), dev);
    if (align){
      align->ISGetProperties(dev);
    }
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
	INumber *latitude;
	
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
	CurrentSteppersNP=getNumber("CURRENTSTEPPERS");
	PeriodsNP=getNumber("PERIODS");
	DateNP=getNumber("DATE");
	RAStatusLP=getLight("RASTATUS");
	DEStatusLP=getLight("DESTATUS");
	SlewSpeedsNP=getNumber("SLEWSPEEDS");
	SlewModeSP=getSwitch("SLEWMODE");
	HemisphereSP=getSwitch("HEMISPHERE");
	PierSideSP=getSwitch("PIERSIDE");
	TrackModeSP=getSwitch("TRACKMODE");
	TrackRatesNP=getNumber("TRACKRATES");
	//AbortMotionSP=getSwitch("TELESCOPE_ABORT_MOTION");
	HorizontalCoordsNP=getNumber("HORIZONTAL_COORDS");
	for (i=1; i<SlewModeSP->nsp; i++) {
	  if (i < SLEWMODES) {
	    sprintf(SlewModeSP->sp[i].label, "%.2fx", slewspeeds[i-1]);
	    SlewModeSP->sp[i].aux=(void *)&slewspeeds[i-1];
	  } else {
	    sprintf(SlewModeSP->sp[i].label, "%.2fx (default)", defaultspeed);
	    SlewModeSP->sp[i].aux=(void *)&defaultspeed;
	  }
	}
	defineNumber(&GuideNSP);
	defineNumber(&GuideEWP);
	defineSwitch(SlewModeSP);
	defineNumber(SlewSpeedsNP);
	defineNumber(GuideRateNP);
	defineText(MountInformationTP);
	defineNumber(SteppersNP);
	defineNumber(CurrentSteppersNP);
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

	  if (isDebug()) {
	    for (i=0;i<MountInformationTP->ntp;i++) 
	      DEBUGF(Logger::DBG_DEBUG, "Got Board Property %s: %s\n", MountInformationTP->tp[i].name, MountInformationTP->tp[i].text);
	  }
	
	  mount->InquireRAEncoderInfo(SteppersNP);
	  mount->InquireDEEncoderInfo(SteppersNP);
	  if (isDebug()) {
	    for (i=0;i<SteppersNP->nnp;i++) 
	      DEBUGF(Logger::DBG_DEBUG,"Got Encoder Property %s: %.0f\n", SteppersNP->np[i].label, SteppersNP->np[i].value);
	  }
	
	  mount->Init(&ParkSV);
	
	  zeroRAEncoder=mount->GetRAEncoderZero();
	  totalRAEncoder=mount->GetRAEncoderTotal();
	  zeroDEEncoder=mount->GetDEEncoderZero();
	  totalDEEncoder=mount->GetDEEncoderTotal();

	  latitude=IUFindNumber(&LocationNV, "LAT");
	  if ((latitude) && (latitude->value < 0.0)) SetSouthernHemisphere(true);
	  else  SetSouthernHemisphere(false);
	
	  if (ParkSV.sp[0].s==ISS_ON) {
	    //TODO unpark mount if desired
	  }
	
	  //TODO start tracking ? 
	  TrackState=SCOPE_IDLE;
	} 
	catch(EQModError e) {
	  return(e.DefaultHandleException(this));
	}
      }
    else
      {
	if (MountInformationTP) {
	  deleteProperty(GuideNSP.name);
	  deleteProperty(GuideEWP.name);
	  deleteProperty(GuideRateNP->name);
	  deleteProperty(MountInformationTP->name);
	  deleteProperty(SteppersNP->name);
	  deleteProperty(CurrentSteppersNP->name);
	  deleteProperty(PeriodsNP->name);
	  deleteProperty(DateNP->name);
	  deleteProperty(RAStatusLP->name);
	  deleteProperty(DEStatusLP->name);
	  deleteProperty(SlewSpeedsNP->name);
	  deleteProperty(SlewModeSP->name);
	  deleteProperty(HemisphereSP->name);
	  deleteProperty(TrackModeSP->name);
	  deleteProperty(TrackRatesNP->name);
	  deleteProperty(HorizontalCoordsNP->name);
	  deleteProperty(PierSideSP->name);
	  //deleteProperty(AbortMotionSP->name);
	  MountInformationTP=NULL;
	} 
      }
    if (align) {
      if (!align->updateProperties()) return false;
    }
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
  } catch(EQModError e) {
    return(e.DefaultHandleException(this));
  }

  DEBUG(Logger::DBG_SESSION, "Successfully connected to EQMod Mount.");
  return true;
}

bool EQMod::Disconnect()
{
  if (isConnected()) {
    try {
      mount->Disconnect();
    }
    catch(EQModError e) {
      DEBUGF(Logger::DBG_ERROR, "Error when disconnecting mount -> %s", e.message);
      return(false);
    }
    DEBUG(Logger::DBG_SESSION,"Disconnected from EQMod Mount.");
    return true;
  } else return false;
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
  double steppervalues[2];
  const char *steppernames[]={"RAStepsCurrent", "DEStepsCurrent"};


  lst+=(IUFindNumber(&LocationNV, "LONG")->value /15.0); // add longitude ha of observer
  lst=range24(lst);
  
  fs_sexa(hrlst, lst, 2, 360000);
  hrlst[11]='\0';
  DEBUGF(Logger::DBG_SCOPE_STATUS, "Compute local time: lst=%2.8f (%s) - julian date=%8.8f", lst, hrlst, juliandate); 
  //DateNP->s=IPS_BUSY;
  datevalues[0]=lst; datevalues[1]=juliandate;
  IUUpdateNumber(DateNP, datevalues, (char **)datenames, 2);
  DateNP->s=IPS_OK;
  IDSetNumber(DateNP, NULL); 
  try {
    currentRAEncoder=mount->GetRAEncoder();
    currentDEEncoder=mount->GetDEEncoder();
    DEBUGF(Logger::DBG_SCOPE_STATUS, "Current encoders RA=%ld DE=%ld", currentRAEncoder, currentDEEncoder);
    EncodersToRADec(currentRAEncoder, currentDEEncoder, lst, &currentRA, &currentDEC, &currentHA);
    alignedRA=currentRA; alignedDEC=currentDEC;
    if (align && align->isReady())
      align->GetAlignedCoords(lst, currentRA, currentDEC, &alignedRA, &alignedDEC);
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
    /* libnova measures azimuth from south towards west */
    horizvalues[0]=range360(lnaltaz.az + 180);
    horizvalues[1]=lnaltaz.alt;
    IUUpdateNumber(HorizontalCoordsNP, horizvalues, (char **)horiznames, 2);
    IDSetNumber(HorizontalCoordsNP, NULL);

    /* TODO should we consider currentHA after alignment ? */
    pierside=SideOfPier(currentHA);
    if (pierside == EAST) {
      piersidevalues[0]=ISS_ON; piersidevalues[1]=ISS_OFF;
      IUUpdateSwitch(PierSideSP, piersidevalues, (char **)piersidenames, 2);
    } else {
      piersidevalues[0]=ISS_OFF; piersidevalues[1]=ISS_ON;
      IUUpdateSwitch(PierSideSP, piersidevalues, (char **)piersidenames, 2);
    }
    IDSetSwitch(PierSideSP, NULL);

    steppervalues[0]=currentRAEncoder;
    steppervalues[1]=currentDEEncoder;
    IUUpdateNumber(CurrentSteppersNP, steppervalues, (char **)steppernames, 2);
    IDSetNumber(CurrentSteppersNP, NULL);

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
	DEBUGF(Logger::DBG_SESSION, "Iterative Goto (%d): RA diff = %4.2f arcsecs DE diff = %4.2f arcsecs",
	       gotoparams.iterative_count, 3600 * fabs(gotoparams.ratarget - currentRA),  3600 * fabs(gotoparams.detarget - currentDEC));
        if ((gotoparams.iterative_count <= GOTO_ITERATIVE_LIMIT) &&
	    (((3600 * fabs(gotoparams.ratarget - currentRA)) > RAGOTORESOLUTION) || 
	     ((3600 * fabs(gotoparams.detarget - currentDEC)) > DEGOTORESOLUTION))) {
	  gotoparams.racurrent = currentRA; gotoparams.decurrent = currentDEC;
	  gotoparams.racurrentencoder = currentRAEncoder; gotoparams.decurrentencoder = currentDEEncoder;
	  EncoderTarget(&gotoparams);
	  // Start iterative slewing
	  DEBUGF(Logger::DBG_SESSION, "Iterative goto (%d): slew mount to RA increment = %ld, DE increment = %ld", gotoparams.iterative_count, 
		    gotoparams.ratargetencoder - gotoparams.racurrentencoder, gotoparams.detargetencoder - gotoparams.decurrentencoder);
	  mount->SlewTo(gotoparams.ratargetencoder - gotoparams.racurrentencoder, gotoparams.detargetencoder - gotoparams.decurrentencoder);

	} else {
	  ISwitch *sw;
	  sw=IUFindSwitch(&CoordSV,"TRACK");
	  if ((gotoparams.iterative_count > GOTO_ITERATIVE_LIMIT) &&
	    (((3600 * abs(gotoparams.ratarget - currentRA)) > RAGOTORESOLUTION) || 
	     ((3600 * abs(gotoparams.detarget - currentDEC)) > DEGOTORESOLUTION))) {
	    DEBUGF(Logger::DBG_SESSION, "Iterative Goto Limit reached (%d iterations): RA diff = %4.2f arcsecs DE diff = %4.2f arcsecs",
		   gotoparams.iterative_count, 3600 * fabs(gotoparams.ratarget - currentRA),  3600 * fabs(gotoparams.detarget - currentDEC));
	  }
	  if ((RememberTrackState == SCOPE_TRACKING) || ((sw != NULL) && (sw->s == ISS_ON))) {
	    TrackState = SCOPE_TRACKING;
	    TrackModeSP->s=IPS_BUSY;
	    IDSetSwitch(TrackModeSP,NULL);
	    mount->StartRATracking(GetRATrackRate());
	    mount->StartDETracking(GetDETrackRate());
	    DEBUG(Logger::DBG_SESSION, "Telescope slew is complete. Tracking...");
	  } else {
	    TrackState = SCOPE_IDLE;
	    DEBUG(Logger::DBG_SESSION, "Telescope slew is complete. Stopping...");
	  }
	  EqNV.s = IPS_OK;

	}
      } 
    }
  } catch(EQModError e) {
    return(e.DefaultHandleException(this));
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
  DEBUGF(Logger::DBG_DEBUG, "Set southern %s\n", (southern?"true":"false"));
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
  HemisphereSP->s=IPS_IDLE;
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
      DEBUG(Logger::DBG_WARNING, "Goto: RA Limits prevent Counterweights-up slew.");
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
  double juliandate=ln_get_julian_from_sys();
  double lst=ln_get_mean_sidereal_time(juliandate);

  lst+=(IUFindNumber(&LocationNV, "LONG")->value /15.0); // add longitude ha of observer
  lst=range24(lst);

  DEBUGF(Logger::DBG_SESSION,"Starting Goto RA=%g DE=%g (current RA=%g DE=%g)", r, d, currentRA, currentDEC);
    targetRA=r;
    targetDEC=d;
    char RAStr[64], DecStr[64];

    // Compute encoder targets and check RA limits if forced
    bzero(&gotoparams, sizeof(gotoparams));
    gotoparams.ratarget = r;  gotoparams.detarget = d;
    if (align && align->isReady())
      align->AlignGoto(lst, &gotoparams.ratarget, &gotoparams.detarget);
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
      DEBUGF(Logger::DBG_SESSION, "Slewing mount: RA increment = %ld, DE increment = %ld", 
		gotoparams.ratargetencoder - gotoparams.racurrentencoder, gotoparams.detargetencoder - gotoparams.decurrentencoder);
      mount->SlewTo(gotoparams.ratargetencoder - gotoparams.racurrentencoder, gotoparams.detargetencoder - gotoparams.decurrentencoder);
    } catch(EQModError e) {
      return(e.DefaultHandleException(this));
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

    DEBUGF(Logger::DBG_SESSION, "Slewing to RA: %s - DEC: %s", RAStr, DecStr);
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
    DEBUG(Logger::DBG_SESSION, "Parking telescope in progress...");
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
    DEBUG(Logger::DBG_WARNING,"Syncs are allowed only when Tracking");
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
    return(e.DefaultHandleException(this));
  }    

  syncdata.deltaRA = syncdata.targetRA - syncdata.telescopeRA;
  syncdata.deltaDEC= syncdata.targetDEC - syncdata.telescopeDEC;
  //EqReqNV.s=IPS_IDLE;
  //EqNV.s=IPS_OK;
  //IDSetNumber(&EqReqNV, NULL);
  if (align && align->isReady()) align->AlignSync(syncdata.lst, syncdata.jd, syncdata.targetRA, syncdata.targetDEC, syncdata.telescopeRA, syncdata.telescopeDEC);
  DEBUGF(Logger::DBG_SESSION, "Mount Synced (deltaRA = %.6f deltaDEC = %.6f)", syncdata.deltaRA, syncdata.deltaDEC);
  //IDLog("Mount Synced (deltaRA = %.6f deltaDEC = %.6f)\n", syncdata.deltaRA, syncdata.deltaDEC);
  return true;
}

bool EQMod::GuideNorth(float ms) {
  double rateshift=0.0;
  rateshift = TRACKRATE_SIDEREAL * IUFindNumber(GuideRateNP, "GUIDE_RATE_NS")->value;
  DEBUGF(Logger::DBG_SESSION, "Timed guide North %d ms at rate %g",(int)(ms), rateshift);
  if (DEInverted) rateshift = -rateshift;
  try {
    if (ms > 0.0) {
      mount->StartDETracking(GetDETrackRate() + rateshift);
      GuideTimerNS = IEAddTimer((int)(ms), (IE_TCF *)timedguideNSCallback, this);
    }
  } catch(EQModError e) {
    return(e.DefaultHandleException(this));
  }    
}

bool EQMod::GuideSouth(float ms) {
  double rateshift=0.0;
  rateshift = TRACKRATE_SIDEREAL * IUFindNumber(GuideRateNP, "GUIDE_RATE_NS")->value;
  DEBUGF(Logger::DBG_SESSION, "Timed guide South %d ms at rate %g",(int)(ms), rateshift);
  if (DEInverted) rateshift = -rateshift;
  try {
    if (ms > 0.0) {
      mount->StartDETracking(GetDETrackRate() - rateshift);
      GuideTimerNS = IEAddTimer((int)(ms), (IE_TCF *)timedguideNSCallback, this);
    }
  } catch(EQModError e) {
    return(e.DefaultHandleException(this));
  }   
}

bool EQMod::GuideEast(float ms) {
  double rateshift=0.0;
  rateshift = TRACKRATE_SIDEREAL * IUFindNumber(GuideRateNP, "GUIDE_RATE_WE")->value;
  DEBUGF(Logger::DBG_SESSION, "Timed guide East %d ms at rate %g",(int)(ms), rateshift);
  if (RAInverted) rateshift = -rateshift;
  try {
    if (ms > 0.0) {
      mount->StartRATracking(GetRATrackRate() - rateshift);
      GuideTimerWE = IEAddTimer((int)(ms), (IE_TCF *)timedguideWECallback, this);
    }
  } catch(EQModError e) {
    return(e.DefaultHandleException(this));
  }   
}

bool EQMod::GuideWest(float ms) {
  double rateshift=0.0;
  rateshift = TRACKRATE_SIDEREAL * IUFindNumber(GuideRateNP, "GUIDE_RATE_WE")->value;
  DEBUGF(Logger::DBG_SESSION, "Timed guide West %d ms at rate %g",(int)(ms), rateshift);
  if (RAInverted) rateshift = -rateshift;
  try {
    if (ms > 0.0) {
      mount->StartRATracking(GetRATrackRate() + rateshift);
      GuideTimerWE = IEAddTimer((int)(ms), (IE_TCF *)timedguideWECallback, this);
    }
  } catch(EQModError e) {
    return(e.DefaultHandleException(this));
  }   
}

bool EQMod::ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n)
{
  bool compose=true;
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
	      return(e.DefaultHandleException(this));
	    }   
	  }
	  IUUpdateNumber(SlewSpeedsNP, values, names, n);
	  SlewSpeedsNP->s = IPS_OK;
	  IDSetNumber(SlewSpeedsNP, NULL);
	  DEBUGF(Logger::DBG_SESSION, "Setting Slew rates - RA=%.2fx DE=%.2fx", 
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
	      return(e.DefaultHandleException(this));
	    }   
	  }
	  IUUpdateNumber(TrackRatesNP, values, names, n);
	  TrackRatesNP->s = IPS_OK;
	  IDSetNumber(TrackRatesNP, NULL);
	  DEBUGF(Logger::DBG_SESSION, "Setting Custom Tracking Rates - RA=%.6f  DE=%.6f arcsec/s", 
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
	      DEBUG(Logger::DBG_WARNING, "Can not guide if not tracking.");
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
	  DEBUGF(Logger::DBG_SESSION, "Setting Custom Tracking Rates - RA=%1.1f arcsec/s DE=%1.1f arcsec/s", 
		      IUFindNumber(GuideRateNP,"GUIDE_RATE_WE")->value, IUFindNumber(GuideRateNP,"GUIDE_RATE_NS")->value);
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
	  DEBUGF(Logger::DBG_SESSION,"Changed observer: long = %f lat = %f", lnobserver.lng, lnobserver.lat);
	  return true;
	}
      
    }


  if (align && align->isReady()) { compose=align->ISNewNumber(dev,name,values,names,n); if (compose) return true;}

#ifdef WITH_SIMULATOR
  if (simulator) { 
      compose=simulator->ISNewNumber(dev,name,values,names,n); if (compose) return true;
  }
#endif

    //  if we didn't process it, continue up the chain, let somebody else
    //  give it a shot
    return INDI::Telescope::ISNewNumber(dev,name,values,names,n);
}

bool EQMod::ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n)
{
  bool compose=true;
    //IDLog("Enter IsNewSwitch for %s\n",name);
    //for(int x=0; x<n; x++) {
    //    IDLog("Switch %s %d\n",names[x],states[x]);
    //}

    if(strcmp(dev,getDeviceName())==0)
    {      
      if (!strcmp(name, "DEBUG"))
	{
	  ISwitchVectorProperty *svp = getSwitch(name);
	  IUUpdateSwitch(svp, states, names, n);
	  ISwitch *sp = IUFindOnSwitch(svp);
	  if (!sp)
	    return false;
	  
	  if (!strcmp(sp->name, "ENABLE"))
	    setLogDebug(true);
	  else
	    setLogDebug(false);
	  return true;
	}

#ifdef WITH_SIMULATOR
      if (!strcmp(name, "SIMULATION"))
	{
	  ISwitchVectorProperty *svp = getSwitch(name);
	  IUUpdateSwitch(svp, states, names, n);
	  ISwitch *sp = IUFindOnSwitch(svp);
	  if (!sp)
	    return false;
	  
	  if (!strcmp(sp->name, "ENABLE"))
	    setStepperSimulation(true);
	  else
	    setStepperSimulation(false);
	  return true;
	}
#endif

      if(strcmp(name,"HEMISPHERE")==0)
	{
	  /* Read-only property */
	  SetSouthernHemisphere(Hemisphere==SOUTH);
	  return true;
	}

      if(strcmp(name,"SLEWMODE")==0)
 	{  
	  ISwitch *sw;
	  IUUpdateSwitch(SlewModeSP,states,names,n);
	  sw=IUFindOnSwitch(SlewModeSP);
	  DEBUGF(Logger::DBG_SESSION, "Slew mode :  %s", sw->label);
	  SlewModeSP->s=IPS_IDLE;
	  IDSetSwitch(SlewModeSP,NULL);
	  return true;
	}

      if(strcmp(name,"TRACKMODE")==0)
 	{  
	  ISwitch *swbefore, *swafter;
	  swbefore=IUFindOnSwitch(TrackModeSP);
	  IUUpdateSwitch(TrackModeSP,states,names,n);
	  swafter=IUFindOnSwitch(TrackModeSP);
	  //DEBUGF(Logger::DBG_SESSION, "Track mode :  from %s to %s.", swbefore->name, swafter->name);
	  try {
	    if (swbefore == swafter) {
	      if ( TrackState == SCOPE_TRACKING) {
		DEBUGF(Logger::DBG_SESSION, "Stop Tracking (%s).", swafter->name);
		TrackState = SCOPE_IDLE;
		TrackModeSP->s=IPS_IDLE;
		IDSetSwitch(TrackModeSP,NULL);
		mount->StopRA();
		mount->StopDE();
	      } else {
		if (TrackState == SCOPE_IDLE) {
		  DEBUGF(Logger::DBG_SESSION, "Start Tracking (%s).", swafter->name);
		  TrackState = SCOPE_TRACKING;
		  TrackModeSP->s=IPS_BUSY;
		  IDSetSwitch(TrackModeSP,NULL);
		  mount->StartRATracking(GetRATrackRate());
		  mount->StartDETracking(GetDETrackRate());
		} else {
		  TrackModeSP->s=IPS_IDLE;
		  IDSetSwitch(TrackModeSP,NULL);
		  DEBUGF(Logger::DBG_WARNING, "Can not start Tracking (%s).", swafter->name);
		}
	      }
	    } else {
	      if (TrackState == SCOPE_TRACKING) {
		DEBUGF(Logger::DBG_SESSION, "Changed Tracking rate (%s).", swafter->name);
		mount->StartRATracking(GetRATrackRate());
		mount->StartDETracking(GetDETrackRate());
	      } else {
		TrackModeSP->s=IPS_IDLE;
		IDSetSwitch(TrackModeSP,NULL);
		DEBUGF(Logger::DBG_SESSION,"Changed Tracking mode (from %s to %s).", swbefore->name, swafter->name);
	      }
	    }
	    } catch(EQModError e) {
	      return(e.DefaultHandleException(this));
	  }   
	  return true;	
	}
    }

    if (align && align->isReady()) { compose=align->ISNewSwitch(dev,name,states,names,n); if (compose) return true;}

#ifdef WITH_SIMULATOR
    if (simulator) { 
      compose=simulator->ISNewSwitch(dev,name,states,names,n); if (compose) return true;
  }
#endif

    Logger::ISNewSwitch(dev,name,states,names,n);

    //  Nobody has claimed this, so, ignore it
    return INDI::Telescope::ISNewSwitch(dev,name,states,names,n);
}

bool EQMod::ISNewText (const char *dev, const char *name, char *texts[], char *names[], int n) 
{
  bool compose;
  if(strcmp(dev,getDeviceName())==0)
    {

    }
  if (align && align->isReady()) { compose=align->ISNewText(dev,name,texts,names,n); if (compose) return true;}

#ifdef WITH_SIMULATOR
  if (simulator) { 
    compose=simulator->ISNewText(dev,name,texts,names,n); if (compose) return true;
  }
#endif
  //  Nobody has claimed this, so, ignore it
  return INDI::Telescope::ISNewText(dev,name,texts,names,n);
}

double EQMod::GetRASlew() {	  
  ISwitch *sw;
  double rate=1.0;
  sw=IUFindOnSwitch(SlewModeSP);
  if (!strcmp(sw->name, "SLEWCUSTOM")) 
    rate=IUFindNumber(SlewSpeedsNP, "RASLEW")->value;
  else
    rate = *((double *)sw->aux);
  return rate;
}

double EQMod::GetDESlew() {
  ISwitch *sw;
  double rate=1.0;
  sw=IUFindOnSwitch(SlewModeSP);
  if (!strcmp(sw->name, "SLEWCUSTOM")) 
    rate=IUFindNumber(SlewSpeedsNP, "DESLEW")->value;
  else
    rate = *((double *)sw->aux);
  return rate;
}

bool EQMod::MoveNS(TelescopeMotionNS dir)
{
  static int last_motion_ns=-1;
  if (TrackState == SCOPE_SLEWING) {
    DEBUG(Logger::DBG_WARNING, "Can not slew while goto in progress.");
    IUResetSwitch(&MovementNSSP);
    MovementNSSP.s = IPS_IDLE;
    IDSetSwitch(&MovementNSSP, NULL);
    return true;
  }
  try {
  switch (dir)
    {
    case MOTION_NORTH:
      if (last_motion_ns != MOTION_NORTH)  {
	double rate=GetDESlew();
	DEBUG(Logger::DBG_SESSION, "Starting North slew.");
	if (DEInverted) rate=-rate;
	mount->SlewDE(rate);
	last_motion_ns = MOTION_NORTH;
	RememberTrackState = TrackState;
      } else {
	DEBUG(Logger::DBG_SESSION, "North Slew stopped");
	mount->StopDE();
	last_motion_ns=-1;
	if (RememberTrackState == SCOPE_TRACKING) {
	  DEBUG(Logger::DBG_SESSION, "Restarting DE Tracking...");
	  TrackState = SCOPE_TRACKING;
	  mount->StartDETracking(GetDETrackRate());
	} else {
	  TrackState = SCOPE_IDLE;
	}
	IUResetSwitch(&MovementNSSP);
	MovementNSSP.s = IPS_IDLE;
	IDSetSwitch(&MovementNSSP, NULL);
      }
      break;
      
    case MOTION_SOUTH:
      if (last_motion_ns != MOTION_SOUTH) {
	double rate=-GetDESlew();
	DEBUG(Logger::DBG_SESSION, "Starting South slew");
	if (DEInverted) rate=-rate;
	mount->SlewDE(rate);
	last_motion_ns = MOTION_SOUTH;
	RememberTrackState = TrackState;
      } else {
	DEBUG(Logger::DBG_SESSION, "South Slew stopped.");
	mount->StopDE();
	last_motion_ns=-1;
	if (RememberTrackState == SCOPE_TRACKING) {
	    DEBUG(Logger::DBG_SESSION, "Restarting DE Tracking...");
	    TrackState = SCOPE_TRACKING;
	    mount->StartDETracking(GetDETrackRate());
	  } else {
	    TrackState = SCOPE_IDLE;
	  }
	IUResetSwitch(&MovementNSSP);
	MovementNSSP.s = IPS_IDLE;
	IDSetSwitch(&MovementNSSP, NULL);
      }
      break;
    }
  } catch (EQModError e) {
    return e.DefaultHandleException(this);
  }
  return true;
}

bool EQMod::MoveWE(TelescopeMotionWE dir)
{
    static int last_motion=-1;
    if (TrackState == SCOPE_SLEWING) {
      DEBUG(Logger::DBG_WARNING, "Can not slew while goto in progress.");
      IUResetSwitch(&MovementWESP);
      MovementWESP.s = IPS_IDLE;
      IDSetSwitch(&MovementWESP, NULL);
      return true;
    }
    try {
      switch (dir)
	{
	case MOTION_WEST:
	  if (last_motion != MOTION_WEST) {
	    double rate=GetRASlew();
	    DEBUG(Logger::DBG_SESSION, "Starting West Slew");
	    if (RAInverted) rate=-rate;
	    mount->SlewRA(rate);
	    last_motion = MOTION_WEST;
	    RememberTrackState = TrackState;
	  } else {
	    DEBUG(Logger::DBG_SESSION, "West Slew stopped");
	    mount->StopRA();
	    last_motion=-1;
	    if (RememberTrackState == SCOPE_TRACKING) {
	      DEBUG(Logger::DBG_SESSION, "Restarting RA Tracking...");
	      TrackState = SCOPE_TRACKING;
	      mount->StartRATracking(GetRATrackRate());
	    } else {
	      TrackState = SCOPE_IDLE;
	    }
	    IUResetSwitch(&MovementWESP);
	    MovementWESP.s = IPS_IDLE;
	    IDSetSwitch(&MovementWESP, NULL);
	  }
	  break;
	  
	case MOTION_EAST:
	  if (last_motion != MOTION_EAST) {
	    double rate=-GetRASlew();
	    DEBUG(Logger::DBG_SESSION,  "Starting East Slew");
	    if (RAInverted) rate=-rate;
	    mount->SlewRA(rate);
	    last_motion = MOTION_EAST;
	    RememberTrackState = TrackState;
	  } else {
	    DEBUG(Logger::DBG_SESSION,  "East Slew stopped");
	    mount->StopRA();
	    last_motion=-1;
	    if (RememberTrackState == SCOPE_TRACKING) {
	      DEBUG(Logger::DBG_SESSION,  "Restarting RA Tracking...");
	      TrackState = SCOPE_TRACKING;
	      mount->StartRATracking(GetRATrackRate());
	    } else {
	      TrackState = SCOPE_IDLE;
	    }
	    IUResetSwitch(&MovementWESP);
	    MovementWESP.s = IPS_IDLE;
	    IDSetSwitch(&MovementWESP, NULL);
	  }
	  break;
	}
    } catch(EQModError e) {
      return(e.DefaultHandleException(this));
    }   
    return true;

}

bool EQMod::Abort()
{
  try {
    mount->StopRA();
  } catch(EQModError e) {
    if (!(e.DefaultHandleException(this))) {
      DEBUG(Logger::DBG_WARNING,  "Abort: error while stopping RA motor");
    }   
  }   
  try {
    mount->StopDE();
  } catch(EQModError e) {
    if (!(e.DefaultHandleException(this))) {
      DEBUG(Logger::DBG_WARNING,  "Abort: error while stopping DE motor");
    }   
  }   

  if (TrackState==SCOPE_TRACKING) {
    // How to know we are also guiding: GuideTimer != 0 ??
  }
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
  IDSetSwitch(&AbortSV, NULL);
  DEBUG(Logger::DBG_SESSION, "Telescope Aborted");
  
  return true;
}

void EQMod::timedguideNSCallback(void *userpointer) {
  EQMod *p = ((EQMod *)userpointer);
  try {
    p->mount->StartDETracking(p->GetDETrackRate());
  } catch(EQModError e) {
    if (!(e.DefaultHandleException(p))) {
      DEBUGDEVICE(p->getDeviceName(), Logger::DBG_WARNING, "Timed guide North/South Error: can not restart tracking");
    }   
  }
  p->GuideNSP.s = IPS_IDLE;
  //p->GuideNSN[GUIDE_NORTH].value = p->GuideNSN[GUIDE_SOUTH].value = 0;
  IDSetNumber(&(p->GuideNSP), NULL);
  DEBUGDEVICE(p->getDeviceName(), Logger::DBG_SESSION, "End Timed guide North/South");
  IERmTimer(p->GuideTimerNS);
}

void EQMod::timedguideWECallback(void *userpointer) {
  EQMod *p = ((EQMod *)userpointer);
  try {
  p->mount->StartRATracking(p->GetRATrackRate());
  } catch(EQModError e) {
    if (!(e.DefaultHandleException(p))) {
      DEBUGDEVICE(p->getDeviceName(), Logger::DBG_WARNING, "Timed guide West/East Error: can not restart tracking");
    }   
  }
  p->GuideEWP.s = IPS_IDLE;
  //p->GuideWEN[GUIDE_WEST].value = p->GuideWEN[GUIDE_EAST].value = 0;
  IDSetNumber(&(p->GuideEWP), NULL);
  DEBUGDEVICE(p->getDeviceName(), Logger::DBG_SESSION, "End Timed guide West/East");
  IERmTimer(p->GuideTimerWE);
}
