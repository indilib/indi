#if 0
    INDI Ujari Driver
    Copyright (C) 2014 Jasem Mutlaq

    Based on EQMOD INDI Driver by Jean-Luc
    
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
#include <math.h>
#include <unistd.h>
#include <sys/time.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <libnova.h>

#include <indicom.h>

#include "ujari.h"
#include "forkmount.h"
#include "inverter.h"
#include "encoder.h"

#include <memory>

#define DEVICE_NAME "Ujari Observatory"

// We declare an auto pointer to Ujari.
std::auto_ptr<Ujari> ujari(0);

#define	GOTO_RATE	2				/* slew rate, degrees/s */
#define	SLEW_RATE	0.5				/* slew rate, degrees/s */
#define FINE_SLEW_RATE  0.1                             /* slew rate, degrees/s */
#define SID_RATE	0.004178			/* sidereal rate, degrees/s */

#define GOTO_LIMIT      5                               /* Move at GOTO_RATE until distance from target is GOTO_LIMIT degrees */
#define SLEW_LIMIT      2                               /* Move at SLEW_LIMIT until distance from target is SLEW_LIMIT degrees */
#define FINE_SLEW_LIMIT 0.5                             /* Move at FINE_SLEW_RATE until distance from target is FINE_SLEW_LIMIT degrees */

#define POLLMS 1000

#define MAX_HOUR_ANGLE  4

#define GOTO_ITERATIVE_LIMIT 5     /* Max GOTO Iterations */
#define RAGOTORESOLUTION 20        /* GOTO Resolution in arcsecs */
#define DEGOTORESOLUTION 20        /* GOTO Resolution in arcsecs */

#define STELLAR_DAY 86164.098903691
#define TRACKRATE_SIDEREAL ((360.0 * 3600.0) / STELLAR_DAY)
#define SOLAR_DAY 86400
#define TRACKRATE_SOLAR ((360.0 * 3600.0) / SOLAR_DAY)
#define TRACKRATE_LUNAR 14.511415

/* Preset Slew Speeds */
#define SLEWMODES 11
double slewspeeds[SLEWMODES - 1] = { 1.0, 2.0, 4.0, 8.0, 32.0, 64.0, 128.0, 200.0, 300.0, 400.0};
double defaultspeed=64.0;

#define RA_AXIS         0
#define DEC_AXIS        1
#define GUIDE_NORTH     0
#define GUIDE_SOUTH     1
#define GUIDE_WEST      0
#define GUIDE_EAST      1

int DBG_SCOPE_STATUS;
int DBG_COMM;
int DBG_MOUNT;

int timeval_subtract(struct timeval *result, struct timeval *x, struct timeval *y)
{
  /* Perform the carry for the later subtraction by updating y. */
  if (x->tv_usec < y->tv_usec) {
    int nsec = (y->tv_usec - x->tv_usec) / 1000000 + 1;
    y->tv_usec -= 1000000 * nsec;
    y->tv_sec += nsec;
  }
  if (x->tv_usec - y->tv_usec > 1000000) {
    int nsec = (x->tv_usec - y->tv_usec) / 1000000;
    y->tv_usec += 1000000 * nsec;
    y->tv_sec -= nsec;
  }

  /* Compute the time remaining to wait.
     tv_usec is certainly positive. */
  result->tv_sec = x->tv_sec - y->tv_sec;
  result->tv_usec = x->tv_usec - y->tv_usec;

  /* Return 1 if result is negative. */
  return x->tv_sec < y->tv_sec;
}

void ISPoll(void *p);

void ISInit()
{
   static int isInit =0;

   if (isInit == 1)
       return;

    isInit = 1;
    if(ujari.get() == 0) ujari.reset(new Ujari());
    //IEAddTimer(POLLMS, ISPoll, NULL);

}

void ISGetProperties(const char *dev)
{
        ISInit();
        ujari->ISGetProperties(dev);
}

void ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int num)
{
        ISInit();
        ujari->ISNewSwitch(dev, name, states, names, num);
}

void ISNewText(	const char *dev, const char *name, char *texts[], char *names[], int num)
{
        ISInit();
        ujari->ISNewText(dev, name, texts, names, num);
}

void ISNewNumber(const char *dev, const char *name, double values[], char *names[], int num)
{
        ISInit();
        ujari->ISNewNumber(dev, name, values, names, num);
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
    ISInit();
    ujari->ISSnoopDevice(root);
}


Ujari::Ujari()
{
  //ctor
  currentRA=0;
  currentDEC=90;
  Parked=false;
  gotoparams.completed=true;
  gotoparams.aborted=false;
  last_motion_ns=-1;
  last_motion_ew=-1;

  controller = new INDI::Controller(this);

  controller->setJoystickCallback(joystickHelper);
  controller->setButtonCallback(buttonHelper);

  DBG_SCOPE_STATUS = INDI::Logger::getInstance().addDebugLevel("Scope Status", "SCOPE");
  DBG_COMM         = INDI::Logger::getInstance().addDebugLevel("Serial Port", "COMM");
  DBG_MOUNT        = INDI::Logger::getInstance().addDebugLevel("Verbose Mount", "MOUNT");

  mount=new ForkMount(this);
  dome    = new Inverter(Inverter::DOME_INVERTER, this);
  domeEncoder = new Encoder(Encoder::DOME_ENCODER, this);
  shutter = new Inverter(Inverter::SHUTTER_INVERTER, this);

  pierside = EAST;
  RAInverted = DEInverted = false;

  string soundFile;

  soundFile = string(INDI_DATA_DIR) + string("/sounds/slew_complete.ogg");
  SlewCompleteAlarm.load_file(soundFile.c_str());

  soundFile = string(INDI_DATA_DIR) + string("/sounds/slew_error.ogg");
  SlewErrorAlarm.load_file(soundFile.c_str());

  soundFile = string(INDI_DATA_DIR) + string("/sounds/slew_busy.ogg");
  TrackBusyAlarm.load_file(soundFile.c_str());
  TrackBusyAlarm.set_looping(true);

  soundFile = string(INDI_DATA_DIR) + string("/sounds/panic_alarm.ogg");
  PanicAlarm.load_file(soundFile.c_str());
  PanicAlarm.set_looping(true);

  /* initialize time */
  tzset();
  gettimeofday(&lasttimeupdate, NULL); // takes care of DST
  gmtime_r(&lasttimeupdate.tv_sec, &utc);
  lndate.seconds = utc.tm_sec + ((double)lasttimeupdate.tv_usec / 1000000);
  lndate.minutes = utc.tm_min;
  lndate.hours = utc.tm_hour;
  lndate.days = utc.tm_mday;
  lndate.months = utc.tm_mon + 1;
  lndate.years = utc.tm_year + 1900;
  clock_gettime(CLOCK_MONOTONIC, &lastclockupdate);
  /* initialize random seed: */
  srand ( time(NULL) );
}

Ujari::~Ujari()
{
  //dtor
  if (mount) delete mount;
  mount = NULL;

}

const char * Ujari::getDefaultName()
{
    return (char *)DEVICE_NAME;
}

void Ujari::SetPanicAlarm(bool enable)
{
    if (enable)
    {
        if (PanicAlarm.is_playing() == false)
            PanicAlarm.play();
    }
    else
        PanicAlarm.stop();
}

double Ujari::getLongitude()
{
  return(IUFindNumber(&LocationNP, "LONG")->value);
}

double Ujari::getLatitude()
{
  return(IUFindNumber(&LocationNP, "LAT")->value);
}

double Ujari::getJulianDate()
{
    struct timespec currentclock, diffclock;
    double nsecs;
    clock_gettime(CLOCK_MONOTONIC, &currentclock);
    diffclock.tv_sec = currentclock.tv_sec - lastclockupdate.tv_sec;
    diffclock.tv_nsec = currentclock.tv_nsec - lastclockupdate.tv_nsec;
    while (diffclock.tv_nsec > 1000000000) {
      diffclock.tv_sec++;
      diffclock.tv_nsec -= 1000000000;
    }
    while (diffclock.tv_nsec < 0) {
      diffclock.tv_sec--;
      diffclock.tv_nsec += 1000000000;
    }

    lndate.seconds += (diffclock.tv_sec + ((double)diffclock.tv_nsec / 1000000000.0));
    nsecs=lndate.seconds - floor(lndate.seconds);
    utc.tm_sec=lndate.seconds;
    utc.tm_isdst = -1; // let mktime find if DST already in effect in utc
    //IDLog("Get julian: setting UTC secs to %f", utc.tm_sec);
    mktime(&utc); // normalize time
    //IDLog("Get Julian; UTC is now %s", asctime(&utc));
    ln_get_date_from_tm(&utc, &lndate);
    lndate.seconds+=nsecs;
    lastclockupdate=currentclock;
    juliandate=ln_get_julian_day(&lndate);

    return juliandate;
}

double Ujari::getLst(double jd, double lng)
{
  double lst;
  //lst=ln_get_mean_sidereal_time(jd);
  lst=ln_get_apparent_sidereal_time(jd);
  lst+=(lng / 15.0);
  lst=range24(lst);
  return lst;
}

bool Ujari::initProperties()
{
    /* Make sure to init parent properties first */
    INDI::Telescope::initProperties();

    mount->initProperties();
    dome->initProperties();
    domeEncoder->initProperties();
    shutter->initProperties();

    return true;
}

void Ujari::ISGetProperties(const char *dev)
{
    INDI::Telescope::ISGetProperties(dev);

    mount->ISGetProperties(dev);
    dome->ISGetProperties();
    domeEncoder->ISGetProperties();
    shutter->ISGetProperties();

    /* Add debug controls so we may debug driver if necessary */
    addDebugControl();
    addSimulationControl();
}

bool Ujari::loadProperties()
{
    buildSkeleton("indi_ujari_sk.xml");

    //CurrentSteppersNP=getNumber("CURRENTSTEPPERS");
    JulianNP=getNumber("JULIAN");
    TimeLSTNP=getNumber("TIME_LST");
    //RAStatusLP=getLight("RASTATUS");
    //DEStatusLP=getLight("DESTATUS");
    SlewSpeedsNP=getNumber("SLEWSPEEDS");
    SlewModeSP=getSwitch("SLEWMODE");
    HemisphereSP=getSwitch("HEMISPHERE");
    PierSideSP=getSwitch("PIERSIDE");
    TrackModeSP=getSwitch("TRACKMODE");
    TrackDefaultSP=getSwitch("TRACKDEFAULT");
    TrackRatesNP=getNumber("TRACKRATES");
    ReverseDECSP=getSwitch("REVERSEDEC");

    HorizontalCoordNP=getNumber("HORIZONTAL_COORD");
    for (unsigned int i=1; i<SlewModeSP->nsp; i++)
    {
      if (i < SLEWMODES) {
        sprintf(SlewModeSP->sp[i].label, "%.2fx", slewspeeds[i-1]);
        SlewModeSP->sp[i].aux=(void *)&slewspeeds[i-1];
      } else {
        sprintf(SlewModeSP->sp[i].label, "%.2fx (default)", defaultspeed);
        SlewModeSP->sp[i].aux=(void *)&defaultspeed;
      }
    }
    ParkPositionNP=getNumber("PARKPOSITION");
    ParkOptionSP=getSwitch("PARKOPTION");

    controller->mapController("MOTIONDIR", "N/S/W/E Control", INDI::Controller::CONTROLLER_JOYSTICK, "JOYSTICK_1");
    controller->mapController("SLEWPRESET", "Slew Presets", INDI::Controller::CONTROLLER_JOYSTICK, "JOYSTICK_2");
    controller->mapController("ABORTBUTTON", "Abort", INDI::Controller::CONTROLLER_BUTTON, "BUTTON_1");
    controller->initProperties();

    return true;
}

bool Ujari::updateProperties()
{
    INumber *latitude;
    double parkpositionvalues[2];
    const char *parkpositionnames[]={"PARKRA", "PARKDE"};

    INDI::Telescope::updateProperties();
    //IDMessage(this->getDeviceName(),"updateProperties: connected=%d %s", (isConnected()?1:0), this->getDeviceName());
    if (isConnected())
    {
    loadProperties();

    defineSwitch(SlewModeSP);
    defineNumber(SlewSpeedsNP);
    //defineNumber(CurrentSteppersNP);
    defineNumber(JulianNP);
    defineNumber(TimeLSTNP);
    //defineLight(RAStatusLP);
    //defineLight(DEStatusLP);
    defineSwitch(HemisphereSP);
    defineSwitch(TrackModeSP);

    defineNumber(TrackRatesNP);
    defineNumber(HorizontalCoordNP);
    defineSwitch(PierSideSP);
    defineSwitch(ReverseDECSP);
    defineNumber(ParkPositionNP);
    defineSwitch(ParkOptionSP);

    defineSwitch(TrackDefaultSP);

    mount->updateProperties();
    dome->updateProperties(true);
    domeEncoder->updateProperties(true);
    shutter->updateProperties(true);

    try
    {
      mount->Init(&ParkSP);

      zeroRAEncoder=mount->GetRAEncoderZero();
      totalRAEncoder=mount->GetRAEncoderTotal();
      homeRAEncoder=mount->GetRAEncoderHome();
      zeroDEEncoder=mount->GetDEEncoderZero();
      totalDEEncoder=mount->GetDEEncoderTotal();
      homeDEEncoder=mount->GetDEEncoderHome();

      parkRAEncoder=mount->GetRAEncoderPark();
      parkDEEncoder=mount->GetDEEncoderPark();
      parkpositionvalues[0]=parkRAEncoder;
      parkpositionvalues[1]=parkDEEncoder;
      IUUpdateNumber(ParkPositionNP, parkpositionvalues, (char **)parkpositionnames, 2);
      IDSetNumber(ParkPositionNP, NULL);

      Parked=false;
      if (mount->isParked())
      {
        //TODO unpark mount if desired
        Parked=true;
      }

      IUResetSwitch(&ParkSP);
      if (Parked)
      {
        ParkSP.s=IPS_OK;
        IDSetSwitch(&ParkSP, "Mount is parked.");
        TrackState = SCOPE_PARKED;
      }
      else
        TrackState=SCOPE_IDLE;

      latitude=IUFindNumber(&LocationNP, "LAT");
      if ((latitude) && (latitude->value < 0.0)) SetSouthernHemisphere(true);
      else  SetSouthernHemisphere(false);


      loadConfig(true);

    }
    catch(UjariError e)
    {
      return(e.DefaultHandleException(this));
    }
      }
    else
      {
      //deleteProperty(CurrentSteppersNP->name);
      deleteProperty(JulianNP->name);
      deleteProperty(TimeLSTNP->name);
      //deleteProperty(RAStatusLP->name);
      //deleteProperty(DEStatusLP->name);
      deleteProperty(SlewSpeedsNP->name);
      deleteProperty(SlewModeSP->name);
      deleteProperty(HemisphereSP->name);
      deleteProperty(TrackModeSP->name);
      deleteProperty(TrackRatesNP->name);
      deleteProperty(HorizontalCoordNP->name);
      deleteProperty(PierSideSP->name);
      deleteProperty(ReverseDECSP->name);
      deleteProperty(ParkPositionNP->name);
      deleteProperty(ParkOptionSP->name);
      deleteProperty(TrackDefaultSP->name);

      mount->updateProperties();
      dome->updateProperties(false);
      domeEncoder->updateProperties(false);
      shutter->updateProperties(false);
    }

    controller->updateProperties();

    return true;
}


bool Ujari::Connect()
{
    bool mount_rc=false, dome_rc=false, shutter_rc=false, dome_encoder_rc=false;

    if(isConnected()) return true;

    try
    {
      dome_rc = dome->connect();
      dome_encoder_rc = domeEncoder->connect();
      shutter_rc = shutter->connect();
      mount_rc = mount->Connect();
    } catch(UjariError e)
    {
      return(e.DefaultHandleException(this));
    }

    if (mount_rc && dome_rc && shutter_rc && dome_encoder_rc)
    {
       DEBUG(INDI::Logger::DBG_SESSION, "Successfully connected to Ujari Mount.");
        SetTimer(POLLMS);
    }

    return (mount_rc && dome_rc && shutter_rc && dome_encoder_rc);
}

bool Ujari::Disconnect()
{
  if (isConnected()) {
    try {
      mount->Disconnect();
      dome->disconnect();
      domeEncoder->disconnect();
      shutter->disconnect();
    }
    catch(UjariError e) {
      DEBUGF(INDI::Logger::DBG_ERROR, "Error when disconnecting mount -> %s", e.message);
      return(false);
    }
    DEBUG(INDI::Logger::DBG_SESSION,"Disconnected from Ujari Mount.");
    return true;
  } else return false;
}

void Ujari::TimerHit()
{

  if(isConnected())
    {
      bool rc;

      mount->update();
      rc=ReadScopeStatus();

      //IDLog("TrackState after read is %d\n",TrackState);
      if(rc == false)
    {
      // read was not good
      EqNP.s=IPS_ALERT;
      IDSetNumber(&EqNP, NULL);
    }

      SetTimer(POLLMS);
    }
}

bool Ujari::ReadScopeStatus()
{
  // Time
  double juliandate;
  double lst;
  char hrlst[12];
  const char *datenames[]={"LST", "JULIANDATE", "UTC"};
  const char *piersidenames[]={"EAST", "WEST"};
  ISState piersidevalues[2];
  double horizvalues[2];
  const char *horiznames[2]={"AZ", "ALT"};
  //double steppervalues[2];
  //const char *steppernames[]={"RAStepsCurrent", "DEStepsCurrent"};

  juliandate=getJulianDate();
  lst=getLst(juliandate, getLongitude());

  fs_sexa(hrlst, lst, 2, 360000);
  hrlst[11]='\0';
  DEBUGF(DBG_SCOPE_STATUS, "Compute local time: lst=%2.8f (%s) - julian date=%8.8f", lst, hrlst, juliandate);
  IUUpdateNumber(TimeLSTNP, &lst, (char **)(datenames), 1);
  TimeLSTNP->s=IPS_OK;
  IDSetNumber(TimeLSTNP, NULL);
  IUUpdateNumber(JulianNP, &juliandate, (char **)(datenames  +1), 1);
  JulianNP->s=IPS_OK;
  IDSetNumber(JulianNP, NULL);
  strftime(IUFindText(&TimeTP, "UTC")->text, 32, "%Y-%m-%dT%H:%M:%S", &utc);
  TimeTP.s=IPS_OK;
  IDSetText(&TimeTP, NULL);

  try
  {
    currentRAEncoder=mount->GetRAEncoder();
    currentDEEncoder=mount->GetDEEncoder();
    DEBUGF(DBG_SCOPE_STATUS, "Current encoders RA=%ld DE=%ld", currentRAEncoder, currentDEEncoder);
    EncodersToRADec(currentRAEncoder, currentDEEncoder, lst, &currentRA, &currentDEC, &currentHA);

    NewRaDec(currentRA, currentDEC);
    lnradec.ra =(currentRA * 360.0) / 24.0;
    lnradec.dec =currentDEC;
    /* uses sidereal time, not local sidereal time */
    ln_get_hrz_from_equ(&lnradec, &lnobserver, juliandate, &lnaltaz);
    /* libnova measures azimuth from south towards west */
    horizvalues[0]=range360(lnaltaz.az + 180);
    horizvalues[1]=lnaltaz.alt;
    IUUpdateNumber(HorizontalCoordNP, horizvalues, (char **)horiznames, 2);
    IDSetNumber(HorizontalCoordNP, NULL);

    /* TODO should we consider currentHA after alignment ? */
    pierside=SideOfPier(currentHA);
    if (pierside == EAST)
    {
      piersidevalues[0]=ISS_ON; piersidevalues[1]=ISS_OFF;
      IUUpdateSwitch(PierSideSP, piersidevalues, (char **)piersidenames, 2);
    } else {
      piersidevalues[0]=ISS_OFF; piersidevalues[1]=ISS_ON;
      IUUpdateSwitch(PierSideSP, piersidevalues, (char **)piersidenames, 2);
    }
    IDSetSwitch(PierSideSP, NULL);

    //steppervalues[0]=currentRAEncoder;
    //steppervalues[1]=currentDEEncoder;
    //IUUpdateNumber(CurrentSteppersNP, steppervalues, (char **)steppernames, 2);
    //IDSetNumber(CurrentSteppersNP, NULL);

    //mount->GetRAMotorStatus(RAStatusLP);
    //mount->GetDEMotorStatus(DEStatusLP);
    //IDSetLight(RAStatusLP, NULL);
    //IDSetLight(DEStatusLP, NULL);

    if (mount->isProtectionTrigged())
    {
        SetPanicAlarm(true);

        DEBUG(INDI::Logger::DBG_WARNING, "Controller Fault Detected. Check Motor Status Immediately.");

        try
        {
          // stop motor
          mount->StopRA();
          mount->StopDE();
        }
         catch(UjariError e)
         {
               return(e.DefaultHandleException(this));
         }
    }
    else
        SetPanicAlarm(false);

    if (gotoInProgress())
    {
      if (!(mount->IsRARunning()) && !(mount->IsDERunning()))
      {
            // Goto iteration
            gotoparams.iterative_count += 1;
                DEBUGF(INDI::Logger::DBG_SESSION, "Iterative Goto (%d): RA diff = %4.2f arcsecs DE diff = %4.2f arcsecs",
                    gotoparams.iterative_count, 3600 * fabs(gotoparams.ratarget - currentRA),  3600 * fabs(gotoparams.detarget - currentDEC));
            if ((gotoparams.iterative_count <= GOTO_ITERATIVE_LIMIT) && (((3600 * fabs(gotoparams.ratarget - currentRA)) > RAGOTORESOLUTION) ||
                    ((3600 * fabs(gotoparams.detarget - currentDEC)) > DEGOTORESOLUTION)))
            {
                gotoparams.racurrent = currentRA;
                gotoparams.decurrent = currentDEC;
                gotoparams.racurrentencoder = currentRAEncoder;
                gotoparams.decurrentencoder = currentDEEncoder;
                EncoderTarget(&gotoparams);
                // Start iterative slewing
                DEBUGF(INDI::Logger::DBG_SESSION, "Iterative goto (%d): slew mount to RA increment = %ld, DE increment = %ld", gotoparams.iterative_count,
                    gotoparams.ratargetencoder - gotoparams.racurrentencoder, gotoparams.detargetencoder - gotoparams.decurrentencoder);
                //mount->SlewTo(gotoparams.ratargetencoder - gotoparams.racurrentencoder, gotoparams.detargetencoder - gotoparams.decurrentencoder);
                mount->SlewTo(gotoparams.ratargetencoder, gotoparams.detargetencoder);

           }
           else
            {
                ISwitch *sw;
                sw=IUFindSwitch(&CoordSP,"TRACK");
                if ((gotoparams.iterative_count > GOTO_ITERATIVE_LIMIT) && (((3600 * abs(gotoparams.ratarget - currentRA)) > RAGOTORESOLUTION) ||
                    ((3600 * abs(gotoparams.detarget - currentDEC)) > DEGOTORESOLUTION)))
                {
                    DEBUGF(INDI::Logger::DBG_SESSION, "Iterative Goto Limit reached (%d iterations): RA diff = %4.2f arcsecs DE diff = %4.2f arcsecs",
                    gotoparams.iterative_count, 3600 * fabs(gotoparams.ratarget - currentRA),  3600 * fabs(gotoparams.detarget - currentDEC));
                }

                if ((RememberTrackState == SCOPE_TRACKING) || ((sw != NULL) && (sw->s == ISS_ON)))
                {
                    char *name;
                    TrackState = SCOPE_TRACKING;

                    if (RememberTrackState == SCOPE_TRACKING)
                    {
                        sw = IUFindOnSwitch(TrackModeSP);
                        name=sw->name;
                        mount->StartRATracking(GetRATrackRate());
                        mount->StartDETracking(GetDETrackRate());
                    }
                    else
                    {
                        ISState   	state;
                        sw = IUFindOnSwitch(TrackDefaultSP);
                        name=sw->name;
                        state=ISS_ON;
                        mount->StartRATracking(GetDefaultRATrackRate());
                        mount->StartDETracking(GetDefaultDETrackRate());
                        IUResetSwitch(TrackModeSP);
                        IUUpdateSwitch(TrackModeSP,&state,&name,1);
                    }
                    TrackModeSP->s=IPS_BUSY;
                    IDSetSwitch(TrackModeSP,NULL);
                    DEBUGF(INDI::Logger::DBG_SESSION, "Telescope slew is complete. Tracking %s...", name);
                }
                else
                {
                    TrackState = SCOPE_IDLE;
                    DEBUG(INDI::Logger::DBG_SESSION, "Telescope slew is complete. Stopping...");
                }

                SlewCompleteAlarm.play();
                gotoparams.completed=true;
                EqNP.s = IPS_OK;

            }
      }
      else
      {
          //WARN We will continiously update the mount regarding on the updated encoder positions since GOTO started.
          gotoparams.racurrent = currentRA;
          gotoparams.decurrent = currentDEC;
          gotoparams.racurrentencoder = currentRAEncoder;
          gotoparams.decurrentencoder = currentDEEncoder;
          EncoderTarget(&gotoparams);
          mount->SetRATargetEncoder(gotoparams.ratargetencoder);
          mount->SetDETargetEncoder(gotoparams.detargetencoder);
      }
    }

    if (TrackState == SCOPE_PARKING)
    {
      if (!(mount->IsRARunning()) && !(mount->IsDERunning()))
      {
        ParkSP.s=IPS_OK;
        IDSetSwitch(&ParkSP, NULL);
        Parked=true;
        TrackState = SCOPE_PARKED;
        mount->SetParked(true);
        DEBUG(INDI::Logger::DBG_SESSION, "Telescope Parked...");
      }
    }
  } catch(UjariError e)
  {
    return(e.DefaultHandleException(this));
  }

  return true;

}


void Ujari::EncodersToRADec(unsigned long rastep, unsigned long destep, double lst, double *ra, double *de, double *ha)
{
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

  // Ujari observatory zero home position is at Zenith
  DECurrent += lnobserver.lat;

  *ra=RACurrent;
  *de=DECurrent;
  if (ha) *ha=HACurrent;
}

double Ujari::EncoderToHours(unsigned long step, unsigned long initstep, unsigned long totalstep, enum Hemisphere h)
{
  double result=0.0;

  if (step > initstep)
    result = ((double)(step - initstep) / totalstep) * 24.0;
  else
      result = ((double)(initstep - step) / totalstep) * -24.0;

  /*if (step > initstep) {
    result = ((double)(step - initstep) / totalstep) * 24.0;
    result = 24.0 - result;
  } else {
    result = ((double)(initstep - step) / totalstep) * 24.0;
  }

  //TODO See if removing the 6 introduces any issues...
  if (h == NORTH)
    //result = range24(result + 6.0);
      result = range24(result);
  else
    //result = range24((24 - result) + 6.0);
      result = range24((24 - result));
  */
  return result;
}

double Ujari::EncoderToDegrees(unsigned long step, unsigned long initstep, unsigned long totalstep, enum Hemisphere h)
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

double Ujari::EncoderFromHour(double hour, unsigned long initstep, unsigned long totalstep, enum Hemisphere h)
{
  double shifthour=0.0;
  //shifthour=range24(hour - 6);
  //TODO Check if this creates any problem since we remove substraction of 6 hours
  shifthour=range24(hour);
  if (h == NORTH)
    if (shifthour < 12.0)
      return (initstep + ((shifthour / 24.0) * totalstep));
    else
      return (initstep - (((24.0 - shifthour) / 24.0) * totalstep));
  else
    if (shifthour < 12.0)
      return (initstep - ((shifthour / 24.0) * totalstep));
    else
      return (initstep + (((24.0 - shifthour) / 24.0) * totalstep));
}

double Ujari::EncoderFromRA(double ratarget, double detarget, double lst,
                unsigned long initstep, unsigned long totalstep, enum Hemisphere h)
{
  double ha=0.0;
  ha = ratarget - lst;

  // used only in ion??
  if (h == NORTH)
    if ((detarget > 90.0) && (detarget <= 270.0)) ha = ha - 12.0;
  if (h == SOUTH)
    if ((detarget > 90.0) && (detarget <= 270.0)) ha = ha + 12.0;

  ha = range24(ha);
  return EncoderFromHour(ha, initstep, totalstep, h);
}

double Ujari::EncoderFromDegree(double degree, PierSide p, unsigned long initstep, unsigned long totalstep, enum Hemisphere h)
{
  double target = 0.0;
  target = degree;
  if (h == SOUTH)  target = 360.0 - target;

  //TODO check if this one works for Ujari
  target -= lnobserver.lat;

  return (initstep + ((target / 360.0) * totalstep));

  /*if ((target > 180.0) && (p == EAST))
    return (initstep - (((360.0 - target) / 360.0) * totalstep));
  else
    return (initstep + ((target / 360.0) * totalstep));*/
}
double Ujari::EncoderFromDec( double detarget, PierSide p,
                  unsigned long initstep, unsigned long totalstep, enum Hemisphere h)
{
  double target=0.0;
  target = detarget;
  // FIXME Check if disabling this breaks anything
  //if (p == WEST) target = 180.0 - target;
  return EncoderFromDegree(target, p, initstep, totalstep, h);
}

double Ujari::rangeHA(double r)
{
  double res = r;
  while (res< -12.0) res+=24.0;
  while (res>= 12.0) res-=24.0;
  return res;
}


double Ujari::range24(double r)
{
  double res = r;
  while (res<0.0) res+=24.0;
  while (res>24.0) res-=24.0;
  return res;
}

double Ujari::range360(double r)
{
  double res = r;
  while (res<0.0) res+=360.0;
  while (res>360.0) res-=360.0;
  return res;
}

double Ujari::rangeDec(double decdegrees)
{
  if ((decdegrees >= 270.0) && (decdegrees <= 360.0))
    return (decdegrees - 360.0);
  if ((decdegrees >= 180.0) && (decdegrees < 270.0))
    return (180.0 - decdegrees);
  if ((decdegrees >= 90.0) && (decdegrees < 180.0))
    return (180.0 - decdegrees);
  return decdegrees;
}



void Ujari::SetSouthernHemisphere(bool southern)
{
  const char *hemispherenames[]={"NORTH", "SOUTH"};
  ISState hemispherevalues[2];
  DEBUGF(INDI::Logger::DBG_DEBUG, "Set southern %s\n", (southern?"true":"false"));
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

Ujari::PierSide Ujari::SideOfPier(double ha)
{
  double shiftha;
  shiftha=rangeHA(ha - 6.0);
  if (shiftha >= 0.0) return EAST; else return WEST;
}

void Ujari::EncoderTarget(GotoParams *g)
{
  double r, d;
  double ha=0.0, targetra=0.0;
  double juliandate;
  double lst;
  PierSide targetpier;
  unsigned long targetraencoder=0, targetdecencoder=0;
  bool outsidelimits=false;
  r = g->ratarget; d = g->detarget;

  juliandate=getJulianDate();
  lst=getLst(juliandate, getLongitude());

  targetra = r;
  ha = rangeHA(r - lst);

  if (ha < 0.0)
  {
    // target EAST
    if (Hemisphere == NORTH)
        targetpier = WEST;
    else targetpier = EAST;

  }
  else
  {
    if (Hemisphere == NORTH)
        targetpier = EAST;
    else
        targetpier = WEST;

      targetra = r;    
  }

  targetraencoder=EncoderFromRA(targetra, 0.0, lst, zeroRAEncoder, totalRAEncoder, Hemisphere);
  targetdecencoder=EncoderFromDec(d, targetpier, zeroDEEncoder, totalDEEncoder, Hemisphere);

  if ((g->forcecwup) && (g->checklimits)) {
    if (Hemisphere == NORTH) {
      if ((targetraencoder < g->limiteast) || (targetraencoder > g->limitwest)) outsidelimits=true;
    } else {
      if ((targetraencoder > g->limiteast) || (targetraencoder < g->limitwest)) outsidelimits=true;
    }

    if (outsidelimits)
    {
      DEBUGF(INDI::Logger::DBG_ERROR, "Goto: RA Limits exceeed. Requested HA %g", ha);
    /*  if (ha < 0.0) {// target EAST
    if (Hemisphere == NORTH) targetpier = WEST; else targetpier = EAST;
    targetra = range24(r - 12.0);
      } else {
    if (Hemisphere == NORTH) targetpier = EAST; else targetpier = WEST;
    targetra = r;
      }
      targetraencoder=EncoderFromRA(targetra, 0.0, lst, zeroRAEncoder, totalRAEncoder, Hemisphere);
      targetdecencoder=EncoderFromDec(d, targetpier, zeroDEEncoder, totalDEEncoder, Hemisphere);
    }*/
    }
  }
  g->outsidelimits = outsidelimits;
  g->ratargetencoder = targetraencoder;
  g->detargetencoder = targetdecencoder;
}

double Ujari::GetRATrackRate()
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

double Ujari::GetDETrackRate()
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

double Ujari::GetDefaultRATrackRate()
{
  double rate = 0.0;
  ISwitch *sw;
  sw=IUFindOnSwitch(TrackDefaultSP);
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

double Ujari::GetDefaultDETrackRate()
{
  double rate = 0.0;
  ISwitch *sw;
  sw=IUFindOnSwitch(TrackDefaultSP);
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

bool Ujari::gotoInProgress()
{
  return (!gotoparams.completed && !gotoparams.aborted);
}

bool Ujari::Goto(double r,double d)
{
  double juliandate;
  double lst;

  if ((TrackState == SCOPE_SLEWING)  || (TrackState == SCOPE_PARKING) || (TrackState == SCOPE_PARKED))
  {
    DEBUG(INDI::Logger::DBG_WARNING, "Can not perform goto while goto/park in progress, or scope parked.");
    EqNP.s    = IPS_IDLE;
    IDSetNumber(&EqNP, NULL);
    return true;
  }

  juliandate=getJulianDate();
  lst=getLst(juliandate, getLongitude());

  DEBUGF(INDI::Logger::DBG_SESSION,"Starting Goto RA=%g DE=%g (current RA=%g DE=%g)", r, d, currentRA, currentDEC);
    targetRA=r;
    targetDEC=d;
    char RAStr[64], DecStr[64];

    // Compute encoder targets and check RA limits if forced
    bzero(&gotoparams, sizeof(gotoparams));
    gotoparams.ratarget = r;
    gotoparams.detarget = d;
    gotoparams.racurrent = currentRA;
    gotoparams.decurrent = currentDEC;

    gotoparams.racurrentencoder = currentRAEncoder;
    gotoparams.decurrentencoder = currentDEEncoder;
    gotoparams.completed = false;
    gotoparams.aborted = false;
    gotoparams.checklimits = true;
    //TODO made forcecwup true, check if this is causes trouble
    gotoparams.forcecwup = true;
    gotoparams.outsidelimits = false;
    gotoparams.limiteast = zeroRAEncoder - (totalRAEncoder / 24)*MAX_HOUR_ANGLE ; // -4 HA
    gotoparams.limitwest = zeroRAEncoder + (totalRAEncoder / 24)*MAX_HOUR_ANGLE; //  +4 HA
    EncoderTarget(&gotoparams);

    if (gotoparams.outsidelimits)
    {
        try
        {
          // stop motor
          mount->StopRA();
          mount->StopDE();
        }
         catch(UjariError e)
         {
               return(e.DefaultHandleException(this));
         }

        SlewErrorAlarm.play();
        gotoparams.aborted = true;
        return false;
    }

    try {
      // stop motor
      mount->StopRA();
      mount->StopDE();
      // Start slewing
      DEBUGF(INDI::Logger::DBG_SESSION, "Slewing mount: RA increment = %ld, DE increment = %ld",
        gotoparams.ratargetencoder - gotoparams.racurrentencoder, gotoparams.detargetencoder - gotoparams.decurrentencoder);
      //mount->SlewTo(gotoparams.ratargetencoder - gotoparams.racurrentencoder, gotoparams.detargetencoder - gotoparams.decurrentencoder);
      mount->SlewTo(gotoparams.ratargetencoder, gotoparams.detargetencoder);
    } catch(UjariError e)
    {
      SlewErrorAlarm.play();
      return(e.DefaultHandleException(this));
    }

    fs_sexa(RAStr, targetRA, 2, 3600);
    fs_sexa(DecStr, targetDEC, 2, 3600);

    RememberTrackState = TrackState;
    TrackState = SCOPE_SLEWING;

    //EqREqNP.s = IPS_BUSY;
    EqNP.s    = IPS_BUSY;

    TrackModeSP->s=IPS_IDLE;
    IDSetSwitch(TrackModeSP,NULL);

    DEBUGF(INDI::Logger::DBG_SESSION, "Slewing to RA: %s - DEC: %s", RAStr, DecStr);
    return true;
}

bool Ujari::canSync()
{
  return true;
}

bool Ujari::canPark()
{
  return true;
}

bool Ujari::Park()
{
  if (!Parked) {
    if (TrackState == SCOPE_SLEWING)
    {
      DEBUG(INDI::Logger::DBG_SESSION, "Can not park while slewing...");
      ParkSP.s=IPS_ALERT;
      IDSetSwitch(&ParkSP, NULL);
      return false;
    }

    try
    {
      // stop motor
      mount->StopRA();
      mount->StopDE();
      currentRAEncoder=mount->GetRAEncoder();
      currentDEEncoder=mount->GetDEEncoder();
      // Start slewing
      DEBUGF(INDI::Logger::DBG_SESSION, "Parking mount: RA increment = %ld, DE increment = %ld",
        parkRAEncoder - currentRAEncoder, parkDEEncoder - currentDEEncoder);
     // mount->SlewTo(parkRAEncoder - currentRAEncoder, parkDEEncoder - currentDEEncoder);
       mount->SlewTo(parkRAEncoder , parkDEEncoder );
    } catch(UjariError e)
    {
      return(e.DefaultHandleException(this));
    }
    TrackModeSP->s=IPS_IDLE;
    IDSetSwitch(TrackModeSP,NULL);
    TrackState = SCOPE_PARKING;
    ParkSP.s=IPS_BUSY;
    IDSetSwitch(&ParkSP, NULL);
    //TrackState = SCOPE_PARKED;
    DEBUG(INDI::Logger::DBG_SESSION, "Telescope park in progress...");
    //DEBUG(INDI::Logger::DBG_SESSION, "Telescope Parked...");
  } else
  {
    Parked=false;
    mount->SetParked(false);
    TrackState = SCOPE_IDLE;
    ParkSP.s=IPS_IDLE;
    IDSetSwitch(&ParkSP, NULL);
    DEBUG(INDI::Logger::DBG_SESSION, "Telescope unparked.");
  }
  return true;
}

bool Ujari::Sync(double ra,double dec)
{
  double juliandate;
  double lst;
  double ha, targetra;
  PierSide targetpier;

// get current mount position asap
 // tmpsyncdata.telescopeRAEncoder=mount->GetRAEncoder();
  //tmpsyncdata.telescopeDECEncoder=mount->GetDEEncoder();

  juliandate=getJulianDate();
  lst=getLst(juliandate, getLongitude());

  if (TrackState != SCOPE_TRACKING)
  {
    //EqREqNP.s=IPS_IDLE;
    EqNP.s=IPS_ALERT;
    //IDSetNumber(&EqREqNP, NULL);
    IDSetNumber(&EqNP, NULL);
    DEBUG(INDI::Logger::DBG_WARNING,"Syncs are allowed only when Tracking");
    return false;
  }
  /* remember the two last syncs to compute Polar alignment */
  //tmpsyncdata.lst=lst;
  //tmpsyncdata.jd=juliandate;
  //tmpsyncdata.targetRA=ra;
  //tmpsyncdata.targetDEC=dec;

  ha = rangeHA(ra - lst);
  if (ha < 0.0) {// target EAST
    if (Hemisphere == NORTH) targetpier = WEST; else targetpier = EAST;
    targetra = range24(ra - 12.0);
  } else {
    if (Hemisphere == NORTH) targetpier = EAST; else targetpier = WEST;
    targetra = ra;
  }

  //tmpsyncdata.targetRAEncoder=EncoderFromRA(targetra, 0.0, lst, zeroRAEncoder, totalRAEncoder, Hemisphere);
  //tmpsyncdata.targetDECEncoder=EncoderFromDec(dec, targetpier, zeroDEEncoder, totalDEEncoder, Hemisphere);

  try {
    //EncodersToRADec( tmpsyncdata.telescopeRAEncoder, tmpsyncdata.telescopeDECEncoder, lst, &tmpsyncdata.telescopeRA, &tmpsyncdata.telescopeDEC, NULL);
  } catch(UjariError e) {
    return(e.DefaultHandleException(this));
  }


  //tmpsyncdata.deltaRA = tmpsyncdata.targetRA - tmpsyncdata.telescopeRA;
  //tmpsyncdata.deltaDEC= tmpsyncdata.targetDEC - tmpsyncdata.telescopeDEC;
  //tmpsyncdata.deltaRAEncoder = tmpsyncdata.targetRAEncoder - tmpsyncdata.telescopeRAEncoder;
  //tmpsyncdata.deltaDECEncoder= tmpsyncdata.targetDECEncoder - tmpsyncdata.telescopeDECEncoder;

  //EqREqNP.s=IPS_IDLE;
  //EqNP.s=IPS_OK;
  //IDSetNumber(&EqREqNP, NULL);

  //DEBUGF(INDI::Logger::DBG_SESSION, "Mount Synced (deltaRA = %.6f deltaDEC = %.6f)", syncdata.deltaRA, syncdata.deltaDEC);
  //IDLog("Mount Synced (deltaRA = %.6f deltaDEC = %.6f)\n", syncdata.deltaRA, syncdata.deltaDEC);

  SlewCompleteAlarm.play();

  return true;
}

bool Ujari::ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n)
{
  //  first check if it's for our device
  if(strcmp(dev,getDeviceName())==0)
    {
      //  This is for our device
      //  Now lets see if it's something we process here


      if(strcmp(name,"SLEWSPEEDS")==0)
    {
      /* TODO: don't change speed in gotos gotoparams.inprogress... */
      if (TrackState != SCOPE_TRACKING) {
        try {
          for (int i=0; i<n; i++) {
        if (strcmp(names[i], "RASLEW") == 0) mount->SetRARate(values[i]);
        else if  (strcmp(names[i], "DESLEW") == 0) mount->SetDERate(values[i]);
          }
        } catch(UjariError e) {
          return(e.DefaultHandleException(this));
        }
      }
      IUUpdateNumber(SlewSpeedsNP, values, names, n);
      SlewSpeedsNP->s = IPS_OK;
      IDSetNumber(SlewSpeedsNP, NULL);
      DEBUGF(INDI::Logger::DBG_SESSION, "Setting Slew rates - RA=%.2fx DE=%.2fx",
              IUFindNumber(SlewSpeedsNP,"RASLEW")->value, IUFindNumber(SlewSpeedsNP,"DESLEW")->value);
      return true;
    }

      if(strcmp(name,"TRACKRATES")==0)
    {
      ISwitch *sw;
      sw=IUFindOnSwitch(TrackModeSP);
      if ((!sw) && (!strcmp(sw->name, "CUSTOM"))) {
        try {
          for (int i=0; i<n; i++) {
        if (strcmp(names[i], "RATRACKRATE") == 0) mount->SetRARate(values[i] / FORKMOUNT_STELLAR_SPEED);
        else if  (strcmp(names[i], "DETRACKRATE") == 0) mount->SetDERate(values[i] / FORKMOUNT_STELLAR_SPEED);
          }
        } catch(UjariError e) {
          return(e.DefaultHandleException(this));
        }
      }
      IUUpdateNumber(TrackRatesNP, values, names, n);
      TrackRatesNP->s = IPS_OK;
      IDSetNumber(TrackRatesNP, NULL);
      DEBUGF(INDI::Logger::DBG_SESSION, "Setting Custom Tracking Rates - RA=%.6f  DE=%.6f arcsec/s",
              IUFindNumber(TrackRatesNP,"RATRACKRATE")->value, IUFindNumber(TrackRatesNP,"DETRACKRATE")->value);
      return true;
    }

      if(strcmp(name,"PARKPOSITION")==0)
    {
      for (int i=0; i<n; i++) {
        if (strcmp(names[i], "PARKRA") == 0) mount->SetRAEncoderPark(values[i]);
        else if  (strcmp(names[i], "PARKDE") == 0) mount->SetDEEncoderPark(values[i]);
      }
      parkRAEncoder=mount->GetRAEncoderPark();
      parkDEEncoder=mount->GetDEEncoderPark();
      for (int i=0; i<n; i++) {
        if (strcmp(names[i], "PARKRA") == 0) values[i]=parkRAEncoder;
        else if  (strcmp(names[i], "PARKDE") == 0) values[i]=parkDEEncoder;
      }
      IUUpdateNumber(ParkPositionNP, values, names, n);
      ParkPositionNP->s = IPS_OK;
      IDSetNumber(ParkPositionNP, NULL);
      DEBUGF(INDI::Logger::DBG_SESSION, "Setting Park Position - RA Encoder=%ld DE Encoder=%ld",
              parkRAEncoder, parkDEEncoder);
      return true;
    }

    }


    mount->ISNewNumber(dev, name, values, names, n);
    dome->ISNewNumber(dev, name, values, names, n);
    shutter->ISNewNumber(dev, name, values, names, n);

    //  if we didn't process it, continue up the chain, let somebody else
    //  give it a shot
    return INDI::Telescope::ISNewNumber(dev,name,values,names,n);
}

bool Ujari::ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n)
{
  bool compose=true;
    //IDLog("Enter IsNewSwitch for %s\n",name);
    //for(int x=0; x<n; x++) {
    //    IDLog("Switch %s %d\n",names[x],states[x]);
    //}

    if(strcmp(dev,getDeviceName())==0)
    {

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
      DEBUGF(INDI::Logger::DBG_SESSION, "Slew mode :  %s", sw->label);
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
      //DEBUGF(INDI::Logger::DBG_SESSION, "Track mode :  from %s to %s.", (swbefore?swbefore->name:"None"), swafter->name);
      try {
        if (swbefore == NULL) {
          if (TrackState == SCOPE_IDLE) {
        DEBUGF(INDI::Logger::DBG_SESSION, "Start Tracking (%s).", swafter->name);
        TrackState = SCOPE_TRACKING;
        TrackModeSP->s=IPS_BUSY;
        IDSetSwitch(TrackModeSP,NULL);
        mount->StartRATracking(GetRATrackRate());
        mount->StartDETracking(GetDETrackRate());
          } else {
        TrackModeSP->s=IPS_IDLE;
        IDSetSwitch(TrackModeSP,NULL);
        DEBUGF(INDI::Logger::DBG_WARNING, "Can not start Tracking (%s). Scope not idle", swafter->name);
          }
        } else {
          if (swbefore == swafter) {
        if ( TrackState == SCOPE_TRACKING) {
          DEBUGF(INDI::Logger::DBG_SESSION, "Stop Tracking (%s).", swafter->name);
          TrackState = SCOPE_IDLE;
          TrackModeSP->s=IPS_IDLE;
          IUResetSwitch(TrackModeSP);
          IDSetSwitch(TrackModeSP,NULL);
          mount->StopRA();
          mount->StopDE();
        }
          } else {
        if (TrackState == SCOPE_TRACKING) {
          DEBUGF(INDI::Logger::DBG_SESSION, "Changed Tracking rate (%s).", swafter->name);
          mount->StartRATracking(GetRATrackRate());
          mount->StartDETracking(GetDETrackRate());
        }
          }
        }
      } catch(UjariError e) {
        return(e.DefaultHandleException(this));
      }
      return true;
    }

      if(strcmp(name,"TRACKDEFAULT")==0)
    {
      ISwitch *swbefore, *swafter;
      swbefore=IUFindOnSwitch(TrackDefaultSP);
      IUUpdateSwitch(TrackDefaultSP,states,names,n);
      swafter=IUFindOnSwitch(TrackDefaultSP);
      if (swbefore != swafter) {
        TrackDefaultSP->s=IPS_IDLE;
        IDSetSwitch(TrackDefaultSP,NULL);
        DEBUGF(INDI::Logger::DBG_SESSION,"Changed Track Default (from %s to %s).", swbefore->name, swafter->name);
      }
      return true;
    }

      if (!strcmp(name, "PARKOPTION"))
    {
      ISwitchVectorProperty *svp = getSwitch(name);
      IUUpdateSwitch(svp, states, names, n);
      ISwitch *sp = IUFindOnSwitch(svp);
      if (!sp)
        return false;
      IDSetSwitch(svp, NULL);
      if (TrackState != SCOPE_IDLE) {
        DEBUG(INDI::Logger::DBG_SESSION, "Can not change park position while moving...");
        svp->s=IPS_ALERT;
        IDSetSwitch(svp, NULL);
        return false;
      }

      if (!strcmp(sp->name, "PARKSETCURRENT")) {
        parkRAEncoder=currentRAEncoder;
        parkDEEncoder=currentDEEncoder;
        mount->SetRAEncoderPark(parkRAEncoder);
        mount->SetDEEncoderPark(parkDEEncoder);
        IUFindNumber(ParkPositionNP, "PARKRA")->value=parkRAEncoder;
        IUFindNumber(ParkPositionNP, "PARKDE")->value=parkDEEncoder;
        IDSetNumber(ParkPositionNP, NULL);
        DEBUGF(INDI::Logger::DBG_SESSION, "Setting Park Position to current- RA Encoder=%ld DE Encoder=%ld",
              parkRAEncoder, parkDEEncoder);
      }

      if (!strcmp(sp->name, "PARKSETDEFAULT")) {
        parkRAEncoder=mount->GetRAEncoderParkDefault();
        parkDEEncoder=mount->GetDEEncoderParkDefault();
        mount->SetRAEncoderPark(parkRAEncoder);
        mount->SetDEEncoderPark(parkDEEncoder);
        IUFindNumber(ParkPositionNP, "PARKRA")->value=parkRAEncoder;
        IUFindNumber(ParkPositionNP, "PARKDE")->value=parkDEEncoder;
        IDSetNumber(ParkPositionNP, NULL);
        DEBUGF(INDI::Logger::DBG_SESSION, "Setting Park Position to default- RA Encoder=%ld DE Encoder=%ld",
              parkRAEncoder, parkDEEncoder);
      }

      if (!strcmp(sp->name, "PARKWRITEDATA")) {
        if (mount->WriteParkData())
          DEBUGF(INDI::Logger::DBG_SESSION, "Saved Park Status/Position- RA Encoder=%ld DE Encoder=%ld, Parked=%s",
             parkRAEncoder, parkDEEncoder, (Parked?"yes":"no"));
        else
          DEBUG(INDI::Logger::DBG_WARNING, "Can not save Park Status/Position");
      }

      return true;
    }

      if (!strcmp(name, "REVERSEDEC"))
      {
          IUUpdateSwitch(ReverseDECSP, states, names, n);

          ReverseDECSP->s = IPS_OK;

          DEInverted = (ReverseDECSP->sp[0].s == ISS_ON) ? true : false;

          DEBUG(INDI::Logger::DBG_SESSION, "Inverting Declination Axis.");

          IDSetSwitch(ReverseDECSP, NULL);

      }


    }

    mount->ISNewSwitch(dev, name, states, names, n);
    dome->ISNewSwitch(dev, name, states, names, n);
    shutter->ISNewSwitch(dev, name, states, names, n);

    controller->ISNewSwitch(dev, name, states, names, n);
    INDI::Logger::ISNewSwitch(dev,name,states,names,n);

    //  Nobody has claimed this, so, ignore it
    return INDI::Telescope::ISNewSwitch(dev,name,states,names,n);
}

bool Ujari::ISNewText (const char *dev, const char *name, char *texts[], char *names[], int n)
{


  mount->ISNewText(dev, name, texts, names, n);
  dome->ISNewText(dev, name, texts, names, n);
  shutter->ISNewText(dev, name, texts, names, n);
  controller->ISNewText(dev, name, texts, names, n);

  //  Nobody has claimed this, so, ignore it
  return INDI::Telescope::ISNewText(dev,name,texts,names,n);
}

bool Ujari::updateTime(ln_date *lndate_utc, double utc_offset)
{
   char utc_time[32];
   lndate.seconds = lndate_utc->seconds;
   lndate.minutes = lndate_utc->minutes;
   lndate.hours   = lndate_utc->hours;
   lndate.days    = lndate_utc->days;
   lndate.months  = lndate_utc->months;
   lndate.years   = lndate_utc->years;

   utc.tm_sec = lndate.seconds;
   utc.tm_min= lndate.minutes;
   utc.tm_hour = lndate.hours;
   utc.tm_mday = lndate.days;
   utc.tm_mon = lndate.months -1;
   utc.tm_year = lndate.years - 1900;

   gettimeofday(&lasttimeupdate, NULL);

   strftime(utc_time, 32, "%Y-%m-%dT%H:%M:%S", &utc);

   DEBUGF(INDI::Logger::DBG_SESSION, "Setting UTC Time to %s, Offset %g",
                utc_time, utc_offset);

   return true;
}

double Ujari::GetRASlew()
{
  ISwitch *sw;
  double rate=1.0;
  sw=IUFindOnSwitch(SlewModeSP);
  if (!strcmp(sw->name, "SLEWCUSTOM"))
    rate=IUFindNumber(SlewSpeedsNP, "RASLEW")->value;
  else
    rate = *((double *)sw->aux);
  return rate;
}

double Ujari::GetDESlew()
{
  ISwitch *sw;
  double rate=1.0;
  sw=IUFindOnSwitch(SlewModeSP);
  if (!strcmp(sw->name, "SLEWCUSTOM"))
    rate=IUFindNumber(SlewSpeedsNP, "DESLEW")->value;
  else
    rate = *((double *)sw->aux);
  return rate;
}

bool Ujari::MoveNS(TelescopeMotionNS dir)
{

  try
    {
  switch (dir)
    {
    case MOTION_NORTH:
      if (last_motion_ns != MOTION_NORTH)
      {
    double rate=GetDESlew();
    if (gotoInProgress()  || (TrackState == SCOPE_PARKING) || (TrackState == SCOPE_PARKED))
    {
      DEBUG(INDI::Logger::DBG_WARNING, "Can not slew while goto/park in progress, or scope parked.");
      IUResetSwitch(&MovementNSSP);
      MovementNSSP.s = IPS_IDLE;
      IDSetSwitch(&MovementNSSP, NULL);
      return true;
    }
    DEBUG(INDI::Logger::DBG_SESSION, "Starting North slew.");
    if (DEInverted) rate=-rate;
    mount->SlewDE(rate);
    last_motion_ns = MOTION_NORTH;
    RememberTrackState = TrackState;
    TrackState = SCOPE_SLEWING;
      } else {
    DEBUG(INDI::Logger::DBG_SESSION, "North Slew stopped");
    mount->StopDE();
    last_motion_ns=-1;
    if (RememberTrackState == SCOPE_TRACKING) {
      DEBUG(INDI::Logger::DBG_SESSION, "Restarting DE Tracking...");
      TrackState = SCOPE_TRACKING;
      mount->StartDETracking(GetDETrackRate());
    } else {
      if (last_motion_ew == -1) TrackState = SCOPE_IDLE;
    }
    IUResetSwitch(&MovementNSSP);
    MovementNSSP.s = IPS_IDLE;
    IDSetSwitch(&MovementNSSP, NULL);
      }
      break;

    case MOTION_SOUTH:
      if (last_motion_ns != MOTION_SOUTH)
      {
    double rate=-GetDESlew();
    if (gotoInProgress() || (TrackState == SCOPE_PARKING) || (TrackState == SCOPE_PARKED)){
      DEBUG(INDI::Logger::DBG_WARNING, "Can not slew while goto/park in progress, or scope parked.");
      IUResetSwitch(&MovementNSSP);
      MovementNSSP.s = IPS_IDLE;
      IDSetSwitch(&MovementNSSP, NULL);
      return true;
    }
    DEBUG(INDI::Logger::DBG_SESSION, "Starting South slew");
    if (DEInverted) rate=-rate;
    mount->SlewDE(rate);
    last_motion_ns = MOTION_SOUTH;
    RememberTrackState = TrackState;
    TrackState = SCOPE_SLEWING;
      } else {
    DEBUG(INDI::Logger::DBG_SESSION, "South Slew stopped.");
    mount->StopDE();
    last_motion_ns=-1;
    if (RememberTrackState == SCOPE_TRACKING) {
      DEBUG(INDI::Logger::DBG_SESSION, "Restarting DE Tracking...");
      TrackState = SCOPE_TRACKING;
      mount->StartDETracking(GetDETrackRate());
    } else {
      if (last_motion_ew == -1) TrackState = SCOPE_IDLE;
    }
    IUResetSwitch(&MovementNSSP);
    MovementNSSP.s = IPS_IDLE;
    IDSetSwitch(&MovementNSSP, NULL);
      }
      break;
    }
  } catch (UjariError e) {
    return e.DefaultHandleException(this);
  }
  return true;
}

bool Ujari::MoveWE(TelescopeMotionWE dir)
{

    try {
      switch (dir)
    {
    case MOTION_WEST:
      if (last_motion_ew != MOTION_WEST) {
        double rate=GetRASlew();
        if (gotoInProgress() || (TrackState == SCOPE_PARKING) || (TrackState == SCOPE_PARKED))
        {
          DEBUG(INDI::Logger::DBG_WARNING, "Can not slew while goto/park in progress, or scope parked.");
          IUResetSwitch(&MovementWESP);
          MovementWESP.s = IPS_IDLE;
          IDSetSwitch(&MovementWESP, NULL);
          return true;
        }
        DEBUG(INDI::Logger::DBG_SESSION, "Starting West Slew");
        if (RAInverted) rate=-rate;
        mount->SlewRA(rate);
        last_motion_ew = MOTION_WEST;
        RememberTrackState = TrackState;
        TrackState = SCOPE_SLEWING;
      } else {
        DEBUG(INDI::Logger::DBG_SESSION, "West Slew stopped");
        mount->StopRA();
        last_motion_ew=-1;
        if (RememberTrackState == SCOPE_TRACKING) {
          DEBUG(INDI::Logger::DBG_SESSION, "Restarting RA Tracking...");
          TrackState = SCOPE_TRACKING;
          mount->StartRATracking(GetRATrackRate());
        } else {
          if (last_motion_ns == -1) TrackState = SCOPE_IDLE;
        }
        IUResetSwitch(&MovementWESP);
        MovementWESP.s = IPS_IDLE;
        IDSetSwitch(&MovementWESP, NULL);
      }
      break;

    case MOTION_EAST:
      if (last_motion_ew != MOTION_EAST) {
        double rate=-GetRASlew();
        if (gotoInProgress() || (TrackState == SCOPE_PARKING) || (TrackState == SCOPE_PARKED)) {
          DEBUG(INDI::Logger::DBG_WARNING, "Can not slew while goto/park in progress, or scope parked.");
          IUResetSwitch(&MovementWESP);
          MovementWESP.s = IPS_IDLE;
          IDSetSwitch(&MovementWESP, NULL);
          return true;
        }
        DEBUG(INDI::Logger::DBG_SESSION,  "Starting East Slew");
        if (RAInverted) rate=-rate;
        mount->SlewRA(rate);
        last_motion_ew = MOTION_EAST;
        RememberTrackState = TrackState;
        TrackState = SCOPE_SLEWING;
      } else {
        DEBUG(INDI::Logger::DBG_SESSION,  "East Slew stopped");
        mount->StopRA();
        last_motion_ew=-1;
        if (RememberTrackState == SCOPE_TRACKING) {
          DEBUG(INDI::Logger::DBG_SESSION,  "Restarting RA Tracking...");
          TrackState = SCOPE_TRACKING;
          mount->StartRATracking(GetRATrackRate());
        } else {
          if (last_motion_ns == -1) TrackState = SCOPE_IDLE;
        }
        IUResetSwitch(&MovementWESP);
        MovementWESP.s = IPS_IDLE;
        IDSetSwitch(&MovementWESP, NULL);
      }
      break;
    }
    } catch(UjariError e) {
      return(e.DefaultHandleException(this));
    }
    return true;

}

bool Ujari::Abort()
{
  try {
    mount->StopRA();
  } catch(UjariError e) {
    if (!(e.DefaultHandleException(this))) {
      DEBUG(INDI::Logger::DBG_WARNING,  "Abort: error while stopping RA motor");
    }
  }
  try {
    mount->StopDE();
  } catch(UjariError e) {
    if (!(e.DefaultHandleException(this))) {
      DEBUG(INDI::Logger::DBG_WARNING,  "Abort: error while stopping DE motor");
    }
  }

  if (TrackState==SCOPE_TRACKING) {
    // How to know we are also guiding: GuideTimer != 0 ??
  }
  //INDI::Telescope::Abort();
  // Reset switches
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

  if (ParkSP.s == IPS_BUSY)
    {
      ParkSP.s       = IPS_IDLE;
      IUResetSwitch(&ParkSP);
      IDSetSwitch(&ParkSP, NULL);
    }

  if (EqNP.s == IPS_BUSY)
    {
      EqNP.s = IPS_IDLE;
      IDSetNumber(&EqNP, NULL);
    }

  TrackState=SCOPE_IDLE;

  AbortSP.s=IPS_OK;
  IUResetSwitch(&AbortSP);
  IDSetSwitch(&AbortSP, NULL);
  if (gotoInProgress())
      gotoparams.aborted = true;
  DEBUG(INDI::Logger::DBG_SESSION, "Telescope Aborted");

  return true;
}

bool Ujari::ISSnoopDevice(XMLEle *root)
{
    controller->ISSnoopDevice(root);

    return INDI::Telescope::ISSnoopDevice(root);
}

void Ujari::processButton(const char *button_n, ISState state)
{

    //ignore OFF
    if (state == ISS_OFF)
        return;

    if (!strcmp(button_n, "ABORTBUTTON"))
    {
        // Only abort if we have some sort of motion going on
        if (MovementNSSP.s == IPS_BUSY || MovementWESP.s == IPS_BUSY || EqNP.s == IPS_BUSY)
        {

            Abort();
        }
    }
}

void Ujari::processJoystick(const char * joystick_n, double mag, double angle)
{
    if (!strcmp(joystick_n, "MOTIONDIR"))
        processNSWE(mag, angle);
    else if (!strcmp(joystick_n, "SLEWPRESET"))
        processSlewPresets(mag, angle);
}

void Ujari::processNSWE(double mag, double angle)
{
    if (mag == 0)
    {
        // Moving in the same direction will make it stop
        if (MovementNSSP.s == IPS_BUSY)
            MoveNS(MovementNSSP.sp[0].s == ISS_ON ? MOTION_NORTH : MOTION_SOUTH);

        // Moving in the same direction will make it stop
        if (MovementWESP.s == IPS_BUSY)
            MoveWE(MovementWESP.sp[0].s == ISS_ON ? MOTION_WEST : MOTION_EAST);

    }
    // Put high threshold
    else if (mag > 0.9)
    {
        // North
        if (angle > 0 && angle < 180)
        {
            // Don't try to move if you're busy and moving in the same direction
            if (MovementNSSP.s != IPS_BUSY || MovementNSS[0].s != ISS_ON)
                MoveNS(MOTION_NORTH);

            MovementNSSP.s = IPS_BUSY;
            MovementNSSP.sp[0].s = ISS_ON;
            MovementNSSP.sp[1].s = ISS_OFF;
            IDSetSwitch(&MovementNSSP, NULL);
        }
        // South
        if (angle > 180 && angle < 360)
        {
            // Don't try to move if you're busy and moving in the same direction
           if (MovementNSSP.s != IPS_BUSY  || MovementNSS[1].s != ISS_ON)
            MoveNS(MOTION_SOUTH);

            MovementNSSP.s = IPS_BUSY;
            MovementNSSP.sp[0].s = ISS_OFF;
            MovementNSSP.sp[1].s = ISS_ON;
            IDSetSwitch(&MovementNSSP, NULL);
        }
        // East
        if (angle < 90 || angle > 270)
        {
            // Don't try to move if you're busy and moving in the same direction
           if (MovementWESP.s != IPS_BUSY  || MovementWES[1].s != ISS_ON)
                MoveWE(MOTION_EAST);

           MovementWESP.s = IPS_BUSY;
           MovementWESP.sp[0].s = ISS_OFF;
           MovementWESP.sp[1].s = ISS_ON;
           IDSetSwitch(&MovementWESP, NULL);
        }

        // West
        if (angle > 90 && angle < 270)
        {

            // Don't try to move if you're busy and moving in the same direction
           if (MovementWESP.s != IPS_BUSY  || MovementWES[0].s != ISS_ON)
                MoveWE(MOTION_WEST);

           MovementWESP.s = IPS_BUSY;
           MovementWESP.sp[0].s = ISS_ON;
           MovementWESP.sp[1].s = ISS_OFF;
           IDSetSwitch(&MovementWESP, NULL);
        }
    }

}

void Ujari::processSlewPresets(double mag, double angle)
{
    // high threshold, only 1 is accepted
    if (mag != 1)
        return;

    int currentIndex = IUFindOnSwitchIndex(SlewModeSP);

    // Up
    if (angle > 0 && angle < 180)
    {
        if (currentIndex <= 0)
            return;

        IUResetSwitch(SlewModeSP);
        SlewModeSP->sp[currentIndex-1].s = ISS_ON;
    }
    // Down
    else
    {
        if (currentIndex >= SlewModeSP->nsp-1)
            return;

         IUResetSwitch(SlewModeSP);
         SlewModeSP->sp[currentIndex+1].s = ISS_ON;

    }
    IDSetSwitch(SlewModeSP, NULL);

}

bool Ujari::saveConfigItems(FILE *fp)
{
    controller->saveConfigItems(fp);

    mount->saveConfigItems(fp);
    domeEncoder->saveConfigItems(fp);

    return INDI::Telescope::saveConfigItems(fp);

}

void Ujari::joystickHelper(const char * joystick_n, double mag, double angle)
{
    ujari->processJoystick(joystick_n, mag, angle);

}

void Ujari::buttonHelper(const char * button_n, ISState state)
{
    ujari->processButton(button_n, state);
}

bool Ujari::updateLocation(double latitude, double longitude, double elevation)
{
  lnobserver.lng =  longitude;
  lnobserver.lat =  latitude;
  if (latitude < 0.0) SetSouthernHemisphere(true);
  else SetSouthernHemisphere(false);
  DEBUGF(INDI::Logger::DBG_SESSION,"updateLocation: long = %g lat = %g", lnobserver.lng, lnobserver.lat);
  return true;
}

void Ujari::debugTriggered(bool enable)
{
    mount->setDebug(enable);
    dome->setDebug(enable);
    shutter->setDebug(enable);
}

void Ujari::simulationTriggered(bool enable)
{

   mount->setSimulation(enable);
   dome->setSimulation(enable);
   domeEncoder->setSimulation(enable);
   shutter->setSimulation(enable);
   DEBUGF(INDI::Logger::DBG_SESSION, "Simulation is %s.", enable ? "Enabled":"Disabled");


}
