/*
    Kuwait National Radio Observatory
    INDI driver of KNRO Primary Control System

    Copyright (C) 2009 Jasem Mutlaq (mutlaqja@ikarustech.com)

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

    Change Log:
    2009-03-28: Start. Imported from Ujari Observatory Control System.
    2009-05-11: Code cleanup.

*/

#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <math.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>
#include <sys/time.h>

#include <memory>

#include <indicom.h>

// For playing OGG
#include "ogg_util.h"

#include "knro.h"

using namespace std;

auto_ptr<knroObservatory> KNRO_observatory(0);		/* Autoptr to observatory */

const int POLLMS = 500;				/* Status loop runs every 500 ms or 2 Hz */

#define slewALTTolerance    30
#define slewAZTolerance     30

pthread_mutex_t knroObservatory::az_encoder_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t knroObservatory::alt_encoder_mutex = PTHREAD_MUTEX_INITIALIZER;

/**************************************************************************************
**
***************************************************************************************/
void ISInit()
{
  if(KNRO_observatory.get() == 0)
  {
    KNRO_observatory.reset(new knroObservatory());
  }
}

/**************************************************************************************
**
***************************************************************************************/
void ISGetProperties (const char *dev)
{ 
   ISInit();
  
  KNRO_observatory->ISGetProperties(dev);
}

/**************************************************************************************
**
***************************************************************************************/
void ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n)
{	    
  ISInit();
  
  KNRO_observatory->ISNewSwitch(dev, name, states, names, n);
}

/**************************************************************************************
**
***************************************************************************************/

void ISNewText (const char *dev, const char *name, char *texts[], char *names[], int n)
{
   ISInit();
   
   KNRO_observatory->ISNewText(dev, name, texts, names, n);
}

/**************************************************************************************
**
***************************************************************************************/
void ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n)
{	    
  ISInit();
  
  KNRO_observatory->ISNewNumber(dev, name, values, names, n);
}

/**************************************************************************************
**
***************************************************************************************/
void ISNewBLOB (const char *dev, const char *name, int sizes[], int blobsizes[], char *blobs[], char *formats[], char *names[], int n) {}
void ISSnoopDevice (XMLEle *root) {}


/**************************************************************************************
**
***************************************************************************************/
knroObservatory::knroObservatory()
{
  AzEncoder = new knroEncoder(knroEncoder::AZ_ENCODER, this);
  AltEncoder = new knroEncoder(knroEncoder::ALT_ENCODER, this);

  AzInverter = new knroInverter(knroInverter::AZ_INVERTER, this);
  AltInverter = new knroInverter(knroInverter::ALT_INVERTER, this);

  lastAlt =lastAz = 0;
  
}

/**************************************************************************************
**
***************************************************************************************/
knroObservatory::~knroObservatory()
{
    delete(AzEncoder);
    delete(AltEncoder);
    delete(AzInverter);
    delete(AltInverter);
}


const char *knroObservatory::getDefaultName()
{
    return (char *)"KNRO";
}

/**************************************************************************************
**
***************************************************************************************/
bool knroObservatory::initProperties()
{
  INDI::Telescope::initProperties();

  /**************************************************************************/
  IUFillNumber(&HorizontalCoordsN[0], "AZ",  "Az  H:M:S", "%10.6m",  0., 24., 0., 0.);
  IUFillNumber(&HorizontalCoordsN[1], "ALT", "Alt D:M:S", "%10.6m", -90., 90., 0., 0.);
  IUFillNumberVector(&HorizontalCoordsNP, HorizontalCoordsN, NARRAY(HorizontalCoordsN), getDeviceName(), "HORIZONTAL_COORD", "Horizontal Coords", BASIC_GROUP, IP_RO, 120, IPS_IDLE);
  /**************************************************************************/
  
  /**************************************************************************/
  IUFillLight(&AzSafetyL[0], "Azimuth Status", "", IPS_IDLE);
  IUFillLightVector(&AzSafetyLP, AzSafetyL, NARRAY(AzSafetyL), getDeviceName(), "Safety", "", TELESCOPE_GROUP, IPS_IDLE);
  /**************************************************************************/

  AzEncoder->initProperties();
  AltEncoder->initProperties();
  AzInverter->initProperties();
  AltInverter->initProperties();

#if 0

// TODO re-enable this!

  park_alert.set_looping( true );
  park_alert.load_file("/usr/share/media/alarm1.ogg");
  slew_complete.load_file("/usr/share/media/slew_complete.ogg");

  calibAZtion_error.set_looping(true);
  calibAZtion_error.load_file("/usr/share/media/calibration_error.ogg");

#endif

  slew_complete.load_file("/usr/share/indi/slew_complete.ogg");
  slew_error.load_file("/usr/share/indi/slew_error.ogg");
  slew_busy.load_file("/usr/share/indi/slew_busy.ogg");
  slew_busy.set_looping(true);

  simulation = false;

  TrackState = SCOPE_IDLE;

  // TODO Set it to parking Az??
  initialAz = 0;

  addAuxControls();
}

/**************************************************************************************
**
***************************************************************************************/
void knroObservatory::ISGetProperties(const char *dev)
{
	// Return if it's not our device
    if (dev && strcmp(getDeviceName(), dev))
	   return;

    INDI::Telescope::ISGetProperties(dev);

}

bool knroObservatory::updateProperties()
{
    INDI::Telescope::updateProperties();

    bool connected = isConnected();

    if (connected)
    {
        defineNumber(&HorizontalCoordsNP);
        defineLight(&AzSafetyLP);

        AzEncoder->updateProperties(connected);
        AltEncoder->updateProperties(connected);
        AzInverter->updateProperties(connected);
        AltInverter->updateProperties(connected);

    }
    else
    {
        deleteProperty(HorizontalCoordsNP.name);
        deleteProperty(AzSafetyLP.name);

        AzEncoder->updateProperties(connected);
        AltEncoder->updateProperties(connected);
        AzInverter->updateProperties(connected);
        AltInverter->updateProperties(connected);

    }

}

void knroObservatory::simulationTriggered(bool enable)
{
    if (simulation == enable)
        return;

    simulation = enable;

    if (enable)
    {
        // Simulate all the devices
        AzInverter->enable_simulation();
        AltInverter->enable_simulation();
        AzEncoder->enable_simulation();
        AltEncoder->enable_simulation();

        DEBUG(INDI::Logger::DBG_SESSION, "KNRO simulation is enabled.");
    }
    else
    {
        // disable simulation for all the devices
        AzInverter->disable_simulation();
        AltInverter->disable_simulation();
        AzEncoder->disable_simulation();
        AltEncoder->disable_simulation();

        DEBUG(INDI::Logger::DBG_SESSION, "KNRO simulation is disabled.");
    }


}

bool knroObservatory::MoveNS(TelescopeMotionNS dir)
{

    static int last_move=-1;
    int current_move=-1;

    if (TrackState == SCOPE_SLEWING && TrackState == SCOPE_PARKING)
        Abort();

    current_move = IUFindOnSwitchIndex(&MovementNSSP);

    // We need to stop motion
    if (MovementNSSP.s == IPS_BUSY && current_move == last_move)
    {
       AltInverter->stop();
       last_move = current_move;
       IUResetSwitch(&MovementNSSP);
       IDSetSwitch(&MovementNSSP, NULL);
       DEBUGF(INDI::Logger::DBG_DEBUG, "%s motion halted.", dir==MOTION_NORTH ? "Northward" : "Southward");
       return true;
    }

    if (dir == MOTION_NORTH)
    {

        // In case it's stopped, set it to half speed
        if (AltInverter->get_speed() == 0)
          AltInverter->set_speed(25);

        update_alt_dir(KNRO_NORTH);

        last_move = current_move;

    }
    else
    {
        // In case it's stopped, set it to half speed
        if (AltInverter->get_speed() == 0)
          AltInverter->set_speed(25);

        update_alt_dir(KNRO_SOUTH);

        last_move = current_move;
    }

    DEBUGF(INDI::Logger::DBG_DEBUG, "Moving toward %s.", dir==MOTION_NORTH ? "North" : "South");

    return true;

}

bool knroObservatory::MoveWE(TelescopeMotionWE dir)
{
    static int last_move=-1;
    int current_move=-1;

    if (TrackState == SCOPE_SLEWING && TrackState == SCOPE_PARKING)
        Abort();

    current_move = IUFindOnSwitchIndex(&MovementWESP);

    // We need to stop motion
    if (MovementWESP.s == IPS_BUSY && current_move == last_move)
    {
       AzInverter->stop();
       last_move = current_move;
       IUResetSwitch(&MovementWESP);
       IDSetSwitch(&MovementWESP, NULL);
       DEBUGF(INDI::Logger::DBG_DEBUG, "%s motion halted.", dir==MOTION_WEST ? "Westward" : "Eastward");
       return true;
    }

    if (dir == MOTION_WEST)
    {

        // In case it's stopped, set it to half speed
        if (AzInverter->get_speed() == 0)
          AzInverter->set_speed(25);

        update_az_dir(KNRO_WEST);

        last_move = current_move;

    }
    else
    {

        // In case it's stopped, set it to half speed
        if (AzInverter->get_speed() == 0)
          AzInverter->set_speed(25);

        update_az_dir(KNRO_EAST);

        last_move = current_move;

    }

    DEBUGF(INDI::Logger::DBG_DEBUG, "Moving toward %s.", dir==MOTION_WEST ? "West" : "East");

    return true;

}

bool knroObservatory::Abort()
{
    knroErrCode error_code= SUCCESS;

    if (TrackState == SCOPE_PARKING)
        DEBUG(INDI::Logger::DBG_SESSION, "Parking cancelled or terminated.");

    if ( (error_code = stop_all()) != SUCCESS)
    {
        DEBUGF(INDI::Logger::DBG_ERROR, "%s", get_knro_error_string(error_code));
        return false;
    }
    else
    {
        if (TrackState != SCOPE_PARKING)
            DEBUG(INDI::Logger::DBG_SESSION, "Aborting All Motion.");
        AzInverter->set_verbose(true);
        AltInverter->set_verbose(true);
        return true;
    }

}

bool knroObservatory::Goto(double RA,double DEC)
{
    char raSTR[16], DecSTR[16];

    targetEQCoords.ra  = RA*15.0;
    targetEQCoords.dec = DEC;

    fs_sexa(raSTR, RA, 2, 3600);
    fs_sexa(DecSTR, DEC, 2, 3600);

    DEBUGF(INDI::Logger::DBG_SESSION, "Slewing to RA: %s DEC: %s", raSTR, DecSTR);

    initialAz  = currentHorCoords.az;;

    HorizontalCoordsNP.s = IPS_BUSY;

    TrackState = SCOPE_SLEWING;

    slew_busy.play();

}

bool knroObservatory::Park()
{

    park_telescope();

    return true;

}

bool knroObservatory::updateLocation(double latitude, double longitude, double elevation)
{
    observer.lat = latitude;
    observer.lng = longitude;
    if (observer.lng > 180)
        observer.lng -= 360;

    if (simulation)
        currentEQCoords.dec = targetEQCoords.dec =  latitude;

    DEBUG(INDI::Logger::DBG_SESSION, "Geographical location updated.");
}

/**************************************************************************************
**
***************************************************************************************/
bool knroObservatory::ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n)
{

	// Return if it's not our device
    if (dev && strcmp(getDeviceName(), dev))
       return false;


	AzInverter->ISNewSwitch(dev, name, states, names, n);
	AltInverter->ISNewSwitch(dev, name, states, names, n);
	
	AzEncoder->ISNewSwitch(dev, name, states, names, n);
	AltEncoder->ISNewSwitch(dev, name, states, names, n);

    return INDI::Telescope::ISNewSwitch(dev, name, states, names, n);
}

/**************************************************************************************
**
***************************************************************************************/
bool knroObservatory::ISNewText (const char *dev, const char *name, char *texts[], char *names[], int n)
{
	// Return if it's not our device
    if (dev && strcmp(getDeviceName(), dev))
       return false;

	// Call respective functions for Inverters
	AzInverter->ISNewText(dev, name, texts, names, n);
	AltInverter->ISNewText(dev, name, texts, names, n);
	
	AzEncoder->ISNewText(dev, name, texts, names, n);
	AltEncoder->ISNewText(dev, name, texts, names, n);

    return INDI::Telescope::ISNewText(dev, name, texts, names, n);
	
    return true;
  
}

/**************************************************************************************
**
***************************************************************************************/
bool knroObservatory::ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n)
{
	// Return if it's not our device
    if (dev && strcmp(getDeviceName(), dev))
       return false;


	// Call respective functions for inverter
	AzInverter->ISNewNumber(dev, name, values, names, n);
	AltInverter->ISNewNumber(dev, name, values, names, n);
	
	AzEncoder->ISNewNumber(dev, name, values, names, n);
	AltEncoder->ISNewNumber(dev, name, values, names, n);

    return INDI::Telescope::ISNewNumber(dev, name, values, names, n);

    return true;
}


/**************************************************************************************
**
// ***************************************************************************************/
bool knroObservatory::Connect()
{

    if (simulation)
        currentEQCoords.ra  =  targetEQCoords.ra =  ln_get_mean_sidereal_time(ln_get_julian_from_sys())*15;

    if (AzInverter->connect() && AltInverter->connect() && AzEncoder->connect() && AltEncoder->connect())
	{

		
		// TODO Insure that last_execute_time is up to date. Otherwise, emergency parking
		// might kick in due to elapsed time.
		// time(&last_execute_time);
        DEBUG(INDI::Logger::DBG_SESSION, "KNRO is online.");
        pthread_create( &az_encoder_thread, NULL, &knroEncoder::update_helper, AzEncoder);
        pthread_create( &alt_encoder_thread, NULL, &knroEncoder::update_helper, AltEncoder);

        SetTimer(POLLMS);

        return true;

	}
	else
	{
        DEBUG(INDI::Logger::DBG_SESSION, "Due to the above errors, KNRO is offline.");

        return false;

	}
}

bool knroObservatory::Disconnect()
{
	pthread_cancel(az_encoder_thread);
	pthread_cancel(alt_encoder_thread);
	
	stop_all();
	
	AzInverter->disconnect();
	AltInverter->disconnect();
	AltEncoder->disconnect();
	AzEncoder->disconnect();

    return true;
}

			
/**************************************************************************************
**
// ***************************************************************************************/
knroObservatory::knroErrCode knroObservatory::stop_all()
{
   // TODO
   //park_alert.stop();

   slew_busy.stop();

   TrackState = SCOPE_IDLE;

   // No need to do anything if we're stopped already
   if (!AzInverter->is_in_motion() && !AltInverter->is_in_motion())
     return SUCCESS;
      
   bool az_stop = stop_az();
   bool alt_stop = stop_alt();
   
   if (az_stop && alt_stop)
   {
      HorizontalCoordsNP.s	= IPS_IDLE;
      IDSetNumber(&HorizontalCoordsNP, NULL);

      if (ParkSP.s == IPS_BUSY)
      {
        IUResetSwitch(&ParkSP);
        ParkSP.s = IPS_IDLE;
        DEBUG(INDI::Logger::DBG_SESSION, "Telescope park terminated.");
        IDSetSwitch(&ParkSP, NULL);
      }
      return SUCCESS;
   }
   
   DEBUG(INDI::Logger::DBG_DEBUG, "Stopping telescope failed. Please try again.");
   // TODO Play alarm if stopping failed.
   return INVERTER_ERROR;
     
}

/**************************************************************************************
**
***************************************************************************************/
void  knroObservatory::park_telescope()
{	
    targetEQCoords.dec = 90 - observer.lat;
    targetEQCoords.ra  = ln_get_mean_sidereal_time(ln_get_julian_from_sys()) * 15.0;

    TrackState = SCOPE_PARKING;

    DEBUG(INDI::Logger::DBG_SESSION, "Parking telescope, please stand by...");
    IDSetSwitch(&ParkSP, NULL);

}


/**************************************************************************************
** Slew algorithm is as following:
** We divide each slew to two stages:
** 1. Slew to requested AZ, and ALT and stop if we reach within SLEW toleAZnces.
** 2. Maintain lock on the current coordinates withing TAZCK toleAZnces.
** Slew speed is determined by the angular sepeAZtion between current and target coords.
** Once the slew stage is accomplished, tAZck state kicks in.
**************************************************************************************

/**************************************************************************************
** ToleAZnce is in arc minutes
***************************************************************************************/
bool knroObservatory::is_alt_done()
{
    double delta = 0, threshold=0;

    delta = targetHorCoords.alt - currentHorCoords.alt;

    if (TrackState == SCOPE_SLEWING)
        threshold = ALT_SLEWING_THRESHOLD;
    else
        threshold = ALT_TRACKING_THRESHOLD;

    if (fabs(delta) < threshold)
			return true;


	return false;
}

/**************************************************************************************
** Tolerance is in arc minutes
***************************************************************************************/
bool knroObservatory::is_az_done()
{
    double delta = 0, threshold=0;

    delta = targetHorCoords.az - currentHorCoords.az;

    if (TrackState == SCOPE_SLEWING)
        threshold = AZ_SLEWING_THRESHOLD;
    else
        threshold = AZ_TRACKING_THRESHOLD;

    if (fabs(delta) < threshold)
			return true;


	return false;
}

/**************************************************************************************
**
***************************************************************************************/
const char * knroObservatory::get_knro_error_string(knroErrCode code)
{
	switch (code)
	{

	        case INVERTER_ERROR:
			return "Error: Inverter error. Check logs.";
			break;
			
		case BELOW_HORIZON_ERROR:
			return "Error: requested object is below horizon.";
			break;

		case SAFETY_LIMIT_ERROR:
			return "Error: requested coordinates exceed telescope safety limits.";
			break;

		default:
			return "Unknown error";
			break;
	}
}

void knroObservatory::update_alt_speed()
{
  double delta_alt=0, current_alt_speed=0;
  
  //currentHorCoords.alt = AltEncoder->get_current_angle();
  delta_alt = fabs(currentHorCoords.alt - targetHorCoords.alt);
  current_alt_speed = AltInverter->get_speed();
  
  DEBUGF(INDI::Logger::DBG_DEBUG, "currentHorCoords.alt:  %g ## targetHorCoords.alt: %g ## DeltaAlt is %g ## Current Speed is: %g\n", currentHorCoords.alt, targetHorCoords.alt, delta_alt, current_alt_speed);
  
  if (TrackState == SCOPE_TRACKING)
  {
      if (current_alt_speed != ALT_KNRO_TRACK)
      {
        if (AltInverter->set_speed(ALT_KNRO_TRACK) == false)
        {
                stop_all();
                DEBUG(INDI::Logger::DBG_ERROR,  "Error in changing Alt inverter speed. Checks logs.");
          }
            if (simulation)
               AltEncoder->simulate_track();
      }

    return;
  }

  if (delta_alt <= ALT_SLOW_REGION)
  {
    if (current_alt_speed != ALT_KNRO_SLOW)
      if (AltInverter->set_speed(ALT_KNRO_SLOW) == false)
      {
        stop_all();
        DEBUG(INDI::Logger::DBG_ERROR,  "Error in changing Alt inverter speed. Checks logs.");
      }
    if (simulation)
       AltEncoder->simulate_slow();


  }
  else if (delta_alt <= ALT_MEDIUM_REGION)
  {
    if (current_alt_speed != ALT_KNRO_MEDIUM)
      if (AltInverter->set_speed(ALT_KNRO_MEDIUM) == false)
      {
        stop_all();
        DEBUG(INDI::Logger::DBG_ERROR,  "Error in changing Alt inverter speed. Checks logs.");
      }
    if (simulation)
       AltEncoder->simulate_medium();
  }
  else
  {

    if (current_alt_speed != ALT_KNRO_FAST)
      if (AltInverter->set_speed(ALT_KNRO_FAST) == false)
      {
        stop_all();
        DEBUG(INDI::Logger::DBG_ERROR,  "Error in changing Alt inverter speed. Checks logs.");
      }
    if (simulation)
       AltEncoder->simulate_fast();    
  }
  
}

void knroObservatory::update_alt_dir(AltDirection dir)
{
  switch (dir)
  {
    case KNRO_NORTH:
      if (MovementNSS[KNRO_NORTH].s == ISS_ON)
	  return;
      if (AltInverter->move_forward())
	{
	  IUResetSwitch(&MovementNSSP);
	  MovementNSSP.s = IPS_BUSY;
	  MovementNSS[KNRO_NORTH].s = ISS_ON; 
      if (TrackState != SCOPE_TRACKING)
          DEBUGF(INDI::Logger::DBG_SESSION, "Moving northward with speed %g Hz...", AltInverter->get_speed());

      IDSetSwitch(&MovementNSSP, NULL);

	  if (simulation)
	       AltEncoder->simulate_forward();
	}
	else
	{
        MovementNSSP.s = IPS_ALERT;
        DEBUG(INDI::Logger::DBG_ERROR, "Moving northward failed. Check logs.");
        IDSetSwitch(&MovementNSSP, NULL);
	}
      break;
  
    case KNRO_SOUTH:
      if (MovementNSS[KNRO_SOUTH].s == ISS_ON)
	  return;
      if (AltInverter->move_reverse())
	{
 	 IUResetSwitch(&MovementNSSP);
	 MovementNSSP.s = IPS_BUSY;
	 MovementNSS[KNRO_SOUTH].s = ISS_ON;
     if (TrackState != SCOPE_TRACKING)
        DEBUGF(INDI::Logger::DBG_SESSION, "Moving southward with speed %g Hz...", AltInverter->get_speed());

     IDSetSwitch(&MovementNSSP, NULL);

	  if (simulation)
	      AltEncoder->simulate_reverse();

	}
	else
	{
        MovementNSSP.s = IPS_ALERT;
        DEBUG(INDI::Logger::DBG_ERROR, "Moving southward failed. Check logs.");
        IDSetSwitch(&MovementNSSP, NULL);
	}

      break;
  }
  
}

void knroObservatory::update_az_speed()
{
  
  double delta_az=0, current_az_speed=0;
  
  //currentHorCoords.az = AzEncoder->get_current_angle();
  delta_az = fabs(currentHorCoords.az - targetHorCoords.az);
  current_az_speed = AzInverter->get_speed();
  
  if (delta_az > 180)
    delta_az = 360 - delta_az;
    
  DEBUGF(INDI::Logger::DBG_DEBUG, "currentHorCoords.az:  %g ## targetHorCoords.az: %g ## DeltaAz is %g ## Current Speed is: %g\n", currentHorCoords.az, targetHorCoords.az, delta_az, current_az_speed);
  
  if (TrackState == SCOPE_TRACKING)
  {
      if (current_az_speed != AZ_KNRO_TRACK)
      {
        if (AzInverter->set_speed(AZ_KNRO_TRACK) == false)
        {
                stop_all();
                DEBUG(INDI::Logger::DBG_ERROR, "Error in changing Az inverter speed. Checks logs.");
          }
            if (simulation)
               AzEncoder->simulate_track();
      }

    return;
  }


  if (delta_az <= AZ_SLOW_REGION)
  {
    if (current_az_speed != AZ_KNRO_SLOW)
      if (AzInverter->set_speed(AZ_KNRO_SLOW) == false)
      {
            stop_all();
            DEBUG(INDI::Logger::DBG_ERROR, "Error in changing Az inverter speed. Checks logs.");
      }
    if (simulation)
       AzEncoder->simulate_slow();


  }
  else if (delta_az <= AZ_MEDIUM_REGION)
  {
    if (current_az_speed != AZ_KNRO_MEDIUM)
      if (AzInverter->set_speed(AZ_KNRO_MEDIUM) == false)
      {
        stop_all();
        DEBUG(INDI::Logger::DBG_ERROR, "Error in changing Az inverter speed. Checks logs.");
      }
    if (simulation)
       AzEncoder->simulate_medium();
  }
  else
  {

    if (current_az_speed != AZ_KNRO_FAST)
      if (AzInverter->set_speed(AZ_KNRO_FAST) == false)
      {
        stop_all();
        DEBUG(INDI::Logger::DBG_ERROR, "Error in changing Az inverter speed. Checks logs.");
      }
    if (simulation)
       AzEncoder->simulate_fast();    
  }
}

void knroObservatory::update_az_dir(AzDirection dir)
{
  switch (dir)
  {
   // EAST is CLOCK-WISE direction
    case KNRO_EAST:
      if (MovementWES[KNRO_EAST].s == ISS_ON)
	  return;
      
      if (AzInverter->move_forward())
	{

	  IUResetSwitch(&MovementWESP);
	  MovementWESP.s = IPS_BUSY;
	  MovementWES[KNRO_EAST].s = ISS_ON; 
      if (TrackState != SCOPE_TRACKING)
        DEBUGF(INDI::Logger::DBG_SESSION, "Moving eastward with speed %g Hz...", AzInverter->get_speed());


      IDSetSwitch(&MovementWESP, NULL);

	  if (simulation)
	       AzEncoder->simulate_forward();
	}
	else
	{
        MovementWESP.s = IPS_ALERT;
        DEBUG(INDI::Logger::DBG_ERROR, "Moving eastward failed. Check logs.");
	}
      break;
  
    case KNRO_WEST:
    // WEST is counter CLOCK-WISE direction
      if (MovementWES[KNRO_WEST].s == ISS_ON)
	  return;
      if (AzInverter->move_reverse())
	{
 	 IUResetSwitch(&MovementWESP);
	 MovementWESP.s = IPS_BUSY;
	 MovementWES[KNRO_WEST].s = ISS_ON;
     if (TrackState != SCOPE_TRACKING)
        DEBUGF(INDI::Logger::DBG_SESSION, "Moving westward with speed %g Hz...", AzInverter->get_speed());

     IDSetSwitch(&MovementWESP, NULL);

	  if (simulation)
	      AzEncoder->simulate_reverse();
	}
	else
	{
        MovementWESP.s = IPS_ALERT;
        DEBUG(INDI::Logger::DBG_ERROR, "Moving westward failed. Check logs.");
	}

      break;
  }
}

bool knroObservatory::stop_az()
{

  if (AzInverter->is_in_motion())
  {
      if (AzInverter->stop())
      {
	IUResetSwitch(&MovementWESP);
	MovementWESP.s = IPS_IDLE;
	IDSetSwitch(&MovementWESP, NULL);
        maxAzStopCounter=0;
	  if (simulation)
	       AzEncoder->simulate_stop();

	return true;
      }
      else
      {
	MovementWESP.s = IPS_ALERT;
	IDSetSwitch(&MovementWESP, "Stopping azimuth motion failed. Check logs.");
	return false;
      }
  }
  


  return true;
}

bool knroObservatory::stop_alt()
{

   if (AltInverter->is_in_motion())
  {
      if (AltInverter->stop())
      {
	IUResetSwitch(&MovementNSSP);
	MovementNSSP.s = IPS_IDLE;
	IDSetSwitch(&MovementNSSP, NULL);
        maxAltStopCounter=0;
	  if (simulation)
	       AltEncoder->simulate_stop();

	return true;
      }
      else
      {
	MovementNSSP.s = IPS_ALERT;
	IDSetSwitch(&MovementNSSP, "Stopping alititude motion failed. Check logs.");
	return false;
      }
  }
  
  return true;
  
}


