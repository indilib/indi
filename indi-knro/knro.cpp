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

const int KNRO_MINIMUM_ALT = 20;		/* Minimum Altitude of KNRO, in degrees */

pthread_mutex_t knroObservatory::az_encoder_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t knroObservatory::alt_encoder_mutex = PTHREAD_MUTEX_INITIALIZER;

/**************************************************************************************
**
***************************************************************************************/
void ISPoll(void *)
{
 KNRO_observatory->ISPoll(); 
 IEAddTimer (POLLMS, ISPoll, NULL);
}

/**************************************************************************************
**
***************************************************************************************/
void ISInit()
{
  if(KNRO_observatory.get() == 0)
  {
    KNRO_observatory.reset(new knroObservatory());
  
    IEAddTimer (POLLMS, ISPoll, NULL);
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
  AzEncoder = new knroEncoder(knroEncoder::AZ_ENCODER);
  AltEncoder = new knroEncoder(knroEncoder::ALT_ENCODER);

  AzInverter = new knroInverter(knroInverter::AZ_INVERTER);
  AltInverter = new knroInverter(knroInverter::ALT_INVERTER);

  init_properties();
  
  if (KNRO_DEBUG)
	enable_debug();
  else
	disable_debug();

}

/**************************************************************************************
**
***************************************************************************************/
knroObservatory::~knroObservatory()
{

}

/**************************************************************************************
**
***************************************************************************************/
void knroObservatory::init_properties()
{
  
  /**************************************************************************/
  IUFillSwitch(&ConnectS[0], "CONNECT", "Connect", ISS_OFF);
  IUFillSwitch(&ConnectS[1], "DISCONNECT", "Disconnect", ISS_ON);
  IUFillSwitchVector(&ConnectSP, ConnectS, NARRAY(ConnectS), mydev, "CONNECTION", "Connection", BASIC_GROUP, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);
  switch_list.push_back(&ConnectSP);
  /**************************************************************************/

  /**************************************************************************/
  IUFillSwitch(&AbortSlewS[0], "ABORT_MOTION", "Abort All", ISS_OFF);
  IUFillSwitchVector(&AbortSlewSP, AbortSlewS, NARRAY(AbortSlewS), mydev, "TELESCOPE_ABORT_MOTION", "ABORT", BASIC_GROUP, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);
  switch_list.push_back(&AbortSlewSP);
  /**************************************************************************/
  
  /**************************************************************************/
  IUFillSwitch(&ParkS[0], "PARK", "Park Telescope", ISS_OFF);
  IUFillSwitchVector(&ParkSP, ParkS, NARRAY(ParkS), mydev, "TELESCOPE_PARK", "Park", BASIC_GROUP, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);
  switch_list.push_back(&ParkSP);
  /**************************************************************************/

  /**************************************************************************/
  IUFillNumber(&HorizontalCoordsNR[0], "AZ",  "Az  H:M:S", "%10.6m",  0., 24., 0., 0.);
  IUFillNumber(&HorizontalCoordsNR[1], "ALT", "Alt D:M:S", "%10.6m", -90., 90., 0., 0.);
  IUFillNumberVector(&HorizontalCoordsNRP, HorizontalCoordsNR, NARRAY(HorizontalCoordsNR), mydev, "HORIZONTAL_COORD", "Horizontal Coords", BASIC_GROUP, IP_RO, 120, IPS_IDLE);
  number_list.push_back(&HorizontalCoordsNRP);
  /**************************************************************************/
  
  /**************************************************************************/
  IUFillNumber(&HorizontalCoordsNW[0], "AZ",  "Az  H:M:S", "%10.6m",  0., 24., 0., 0.);
  IUFillNumber(&HorizontalCoordsNW[1], "ALT", "Alt D:M:S", "%10.6m", -90., 90., 0., 0.);
  IUFillNumberVector(&HorizontalCoordsNWP, HorizontalCoordsNW, NARRAY(HorizontalCoordsNW), mydev, "HORIZONTAL_COORD_REQUEST", "Horizontal Request", BASIC_GROUP, IP_RW, 120, IPS_IDLE);
  number_list.push_back(&HorizontalCoordsNWP);
  /**************************************************************************/

  /**************************************************************************/
  IUFillSwitch(&OnCoordSetS[0],"TRACK", "Track", ISS_ON);
  IUFillSwitchVector(&OnCoordSetSP, OnCoordSetS, NARRAY(OnCoordSetS), mydev, "ON_COORD_SET", "On Set", BASIC_GROUP, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);
  switch_list.push_back(&OnCoordSetSP);
  /**************************************************************************/

  /**************************************************************************/
  IUFillNumber(&GeoCoordsN[0], "LAT",  "Lat.  D:M:S +N", "%10.6m",  -90.,  90., 0., 0.);
  IUFillNumber(&GeoCoordsN[1], "LONG", "Long. D:M:S +E", "%10.6m", 0., 360., 0., 0.);
  IUFillNumberVector(&GeoCoordsNP,  GeoCoordsN, NARRAY(GeoCoordsN),  mydev, "GEOGRAPHIC_COORD", "Geographic Location", SITE_GROUP, IP_RW, 0., IPS_IDLE);
  number_list.push_back(&GeoCoordsNP);
  /**************************************************************************/

  /**************************************************************************/
  IUFillNumber(&UTCOffsetN[0], "OFFSET", "Offset", "%0.3g" , -12.,12.,0.5,3.);
  IUFillNumberVector(&UTCOffsetNP, UTCOffsetN, NARRAY(UTCOffsetN), mydev, "OFFSET_UTC", "UTC Offset", SITE_GROUP, IP_RW, 0, IPS_IDLE);
  number_list.push_back(&UTCOffsetNP);
  /**************************************************************************/

  /**************************************************************************/
  IUFillSwitch(&MovementNSS[KNRO_NORTH], "MOTION_NORTH", "Up", ISS_OFF);
  IUFillSwitch(&MovementNSS[KNRO_SOUTH], "MOTION_SOUTH", "Down", ISS_OFF);
  IUFillSwitchVector(&MovementNSSP, MovementNSS, NARRAY(MovementNSS), mydev, "TELESCOPE_MOTION_NS", "Up/Down", TELESCOPE_GROUP, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);
  switch_list.push_back(&MovementNSSP);
  /**************************************************************************/
  
  /**************************************************************************/
  IUFillSwitch(&MovementWES[KNRO_WEST], "MOTION_WEST", "West", ISS_OFF);
  IUFillSwitch(&MovementWES[KNRO_EAST], "MOTION_EAST", "East", ISS_OFF);
  IUFillSwitchVector(&MovementWESP, MovementWES, NARRAY(MovementWES), mydev, "TELESCOPE_MOTION_WE", "West/East", TELESCOPE_GROUP, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);
  switch_list.push_back(&MovementWESP);
  /**************************************************************************/
  
  /**************************************************************************/
  IUFillSwitch(&StopAllS[0], "Stop", "", ISS_OFF);
  IUFillSwitchVector(&StopAllSP, StopAllS, NARRAY(StopAllS), mydev, "All Motion", "", TELESCOPE_GROUP, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);
  switch_list.push_back(&StopAllSP);
  /**************************************************************************/
  
  /**************************************************************************/
  IUFillLight(&AzSafetyL[0], "Azimuth Status", "", IPS_IDLE);
  IUFillLightVector(&AzSafetyLP, AzSafetyL, NARRAY(AzSafetyL), mydev, "Safety", "", TELESCOPE_GROUP, IPS_IDLE);
  light_list.push_back(&AzSafetyLP);
  /**************************************************************************/
 
  /**************************************************************************/
  IUFillNumber(&SlewPrecisionN[0], "SlewAZ",  "Az (arcmin)", "%10.6m",  0., 90., 1, 25);
  IUFillNumber(&SlewPrecisionN[1], "SlewALT", "Alt (arcmin)", "%10.6m", 0., 90., 1, 10);
  IUFillNumberVector(&SlewPrecisionNP, SlewPrecisionN, NARRAY(SlewPrecisionN), mydev, "Slew Precision", "", OPTIONS_GROUP, IP_RW, 0, IPS_IDLE);
  number_list.push_back(&SlewPrecisionNP);
  /**************************************************************************/

  /**************************************************************************/
  IUFillNumber(&TrackPrecisionN[0], "TrackAZ", "Az (arcmin)", "%10.6m",  0., 90, 1., 25);
  IUFillNumber(&TrackPrecisionN[1], "TrackALT", "Alt (arcmin)", "%10.6m", 0., 90, 1., 10);
  IUFillNumberVector(&TrackPrecisionNP, TrackPrecisionN, NARRAY(TrackPrecisionN), mydev, "Tracking Precision", "", OPTIONS_GROUP, IP_RW, 0, IPS_IDLE);
  number_list.push_back(&TrackPrecisionNP);
  /**************************************************************************/

  /**************************************************************************/
  IUFillSwitch(&DebugS[0], "Enable", "", ISS_OFF);
  IUFillSwitch(&DebugS[1], "Disable", "", ISS_ON);
  IUFillSwitchVector(&DebugSP, DebugS, NARRAY(DebugS), mydev, "Debug", "", OPTIONS_GROUP, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);
  switch_list.push_back(&DebugSP);
  /**************************************************************************/
  
  /**************************************************************************/
  IUFillSwitch(&SimulationS[0], "Enable", "", ISS_OFF);
  IUFillSwitch(&SimulationS[1], "Disable", "", ISS_ON);
  IUFillSwitchVector(&SimulationSP, SimulationS, NARRAY(SimulationS), mydev, "Simulation", "", OPTIONS_GROUP, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);
  switch_list.push_back(&SimulationSP);
  /**************************************************************************/
  

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

  slew_stage = SLEW_NONE;

  // TODO Set it to parking Az??
  initialAz = 0;
}

/**************************************************************************************
**
***************************************************************************************/
void knroObservatory::ISGetProperties(const char *dev)
{
	// Return if it's not our device
	if (dev && strcmp(mydev, dev))
	   return;

	// Main Control Group
	IDDefSwitch(&ConnectSP, NULL);
	IDDefNumber(&HorizontalCoordsNRP, NULL);
	IDDefNumber(&HorizontalCoordsNWP, NULL);
	IDDefSwitch(&OnCoordSetSP, NULL);
        IDDefSwitch(&ParkSP, NULL);
	IDDefSwitch(&AbortSlewSP, NULL);

	// Telescope Group
	IDDefSwitch(&MovementNSSP, NULL);
	IDDefSwitch(&MovementWESP, NULL);
	IDDefSwitch(&StopAllSP, NULL);
	IDDefLight(&AzSafetyLP, NULL);
	
	// Encoder Group
	AzEncoder->ISGetProperties();
	AltEncoder->ISGetProperties();
	
	// Inverter Group
	AzInverter->ISGetProperties();
	AltInverter->ISGetProperties();

	// Site Group
	IDDefNumber(&GeoCoordsNP, NULL);
        IDDefNumber(&UTCOffsetNP, NULL);

	// Options Group
	IDDefNumber(&SlewPrecisionNP, NULL);
	IDDefNumber(&TrackPrecisionNP, NULL);
	IDDefSwitch(&DebugSP, NULL);
	IDDefSwitch(&SimulationSP, NULL);
}

/**************************************************************************************
**
***************************************************************************************/
void knroObservatory::ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n)
{
	knroErrCode error_code= SUCCESS;

	// Return if it's not our device
	if (dev && strcmp(mydev, dev))
	   return;

	// ===================================
	// Connect Switch
	if (!strcmp(ConnectSP.name, name))
	{
		if (IUUpdateSwitch(&ConnectSP, states, names, n) < 0)
			return;

		if (is_connected() == true)
			connect();
		else
		{
			stop_all();
			disconnect();
			
			//TODO
			//park_alert.stop();
			reset_all_properties();
			ConnectSP. s = IPS_OK;
			IDSetSwitch(&ConnectSP, "KNRO is offline.");
		}
		
		return;
	}
	
	// ===================================
	// Simulation Switch
	if (!strcmp(SimulationSP.name, name))
	{
		if (IUUpdateSwitch(&SimulationSP, states, names, n) < 0)
			return;

		if (SimulationS[0].s == ISS_ON)
			enable_simulation();
		else
			disable_simulation();
		
		return;
	}
	
	// ===================================
	// Debug Switch
	// ===================================
	if (!strcmp(DebugSP.name, name))
	{
		
		if (IUUpdateSwitch(&DebugSP, states, names, n) < 0)
			return;

		DebugSP.s = IPS_OK;
		IDSetSwitch(&DebugSP, NULL);
	
		if (DebugS[0].s == ISS_ON)
		{
		  AzInverter->enable_debug();
		  AltInverter->enable_debug();
		  AzEncoder->enable_debug();
		  AltEncoder->enable_debug();
		}
		else
		{
		  AzInverter->disable_debug();
		  AltInverter->disable_debug();
		  AzEncoder->disable_debug();
		  AltEncoder->disable_debug();
		}
	
		return;
	}
	
	// If we're not connected, we return
	if (is_connected() == false)
	{
		IDMessage(mydev, "KNRO is offline. Please connect before issuing any commands.");
		reset_all_properties();
		return;
	}

	// ===================================
	// Abort Switch
	if (!strcmp(AbortSlewSP.name, name))
	{
		if (IUUpdateSwitch(&AbortSlewSP, states, names, n) < 0)
			return;

		IUResetSwitch(&AbortSlewSP);
		
		if ( (error_code = stop_all()) != SUCCESS)
		{
			AbortSlewSP.s = IPS_ALERT;
			IDSetSwitch(&AbortSlewSP, "%s", get_knro_error_string(error_code));
		}
		else
		{
			AbortSlewSP.s = IPS_OK;
			IDSetSwitch(&AbortSlewSP, "Aborting All Motion.");
		}

		return;
	}

	// ===================================
	// Stop Switch (For Motion Group), just in case.
	if (!strcmp(StopAllSP.name, name))
	{
		if (IUUpdateSwitch(&StopAllSP, states, names, n) < 0)
			return;

		IUResetSwitch(&StopAllSP);
		if ( (error_code = stop_all()) != SUCCESS)
		{
			StopAllSP.s = IPS_ALERT;
			IDSetSwitch(&StopAllSP, "%s", get_knro_error_string(error_code));
			
		}
		else
		{
		        StopAllSP.s = IPS_OK;
			IDSetSwitch(&StopAllSP, "Aborting All Motion.");
		}

		return;
	}

	// ===================================
	// Park telescope to home position
	if (!strcmp(ParkSP.name, name))
	{
		if (IUUpdateSwitch(&ParkSP, states, names, n) < 0)
			return;

		check_slew_state();
		
		park_telescope();

		return;
	}
	
	// ===================================
	// On Coord Set Switch
	if (!strcmp(OnCoordSetSP.name, name))
	{
		if (IUUpdateSwitch(&OnCoordSetSP, states, names, n) < 0)
			return;

		OnCoordSetSP.s = IPS_OK;
		IDSetSwitch(&OnCoordSetSP, NULL);
		return;

	}
	
	// ===================================
	// Alt Movement Switch
	// ===================================
	if (!strcmp(MovementNSSP.name, name))
	{
	  if (!strcmp(names[0], "MOTION_NORTH"))
	  {
	    // Check if it's north already
	    if (MovementNSS[KNRO_NORTH].s == ISS_ON)
	      return;
	    
	    check_slew_state();
	      
	    // In case it's stopped, set it to half speed
	    if (AltInverter->get_speed() == 0)
	      AltInverter->set_speed(25);
	    
	    update_alt_dir(KNRO_NORTH);
	  }
	  else
	  {
	     // Check if it's south already
	    if (MovementNSS[KNRO_SOUTH].s == ISS_ON)
	      return;
	    
	    check_slew_state();
	    
	    // In case it's stopped, set it to half speed
	    if (AltInverter->get_speed() == 0)
	      AltInverter->set_speed(25);
	    
	    update_alt_dir(KNRO_SOUTH);
		    
	    return;
	 }
	}

	// ===================================
	// Az Movement Switch
	// ===================================
	if (!strcmp(MovementWESP.name, name))
	{
	  if (!strcmp(names[0], "MOTION_WEST"))
	  {
	    // Check if it's west already
	    if (MovementWES[KNRO_WEST].s == ISS_ON)
	      return;
	    
	    check_slew_state();
	    
	    // In case it's stopped, set it to half speed
	    if (AzInverter->get_speed() == 0)
	      AzInverter->set_speed(25);
      
	    update_az_dir(KNRO_WEST);
	  }
	  else
	  {
	     // Check if it's east already
	    if (MovementWES[KNRO_EAST].s == ISS_ON)
	      return;
	    
	    check_slew_state();
	    
	    // In case it's stopped, set it to half speed
	    if (AzInverter->get_speed() == 0)
	      AzInverter->set_speed(25);
	    
	    update_az_dir(KNRO_EAST);
	    
	  }
	    return;
	}
	
	AzInverter->ISNewSwitch(dev, name, states, names, n);
	AltInverter->ISNewSwitch(dev, name, states, names, n);
	
	AzEncoder->ISNewSwitch(dev, name, states, names, n);
	AltEncoder->ISNewSwitch(dev, name, states, names, n);
}

/**************************************************************************************
**
***************************************************************************************/
void knroObservatory::ISNewText (const char *dev, const char *name, char *texts[], char *names[], int n)
{
	// Return if it's not our device
	if (dev && strcmp(mydev, dev))
	   return;

	// Call respective functions for Inverters
	AzInverter->ISNewText(dev, name, texts, names, n);
	AltInverter->ISNewText(dev, name, texts, names, n);
	
	AzEncoder->ISNewText(dev, name, texts, names, n);
	AltEncoder->ISNewText(dev, name, texts, names, n);
	
	// If we're not connected, we return
	/*if (is_connected() == false)
	{
		IDMessage(mydev, "KNRO is offline. Please connect before issuing any commands.");
		reset_all_properties();
		return;
	}*/
	
	
  
}

/**************************************************************************************
**
***************************************************************************************/
void knroObservatory::ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n)
{
	knroErrCode error_code= SUCCESS;

	// Return if it's not our device
	if (dev && strcmp(mydev, dev))
	   return;

	// If we're not connected, we return
	if (is_connected() == false)
	{
		IDMessage(mydev, "KNRO is offline. Please connect before issuing any commands.");
		reset_all_properties();
		return;
	}

	// ===================================
	//  Alt/Az Coords set
	if (!strcmp(HorizontalCoordsNWP.name, name))
	{
	    double newAZ =0, newALT =0;
	    int i=0, nset=0;

	    for (nset = i = 0; i < n; i++)
	    {
		INumber *horp = IUFindNumber (&HorizontalCoordsNWP, names[i]);
		if (horp == &HorizontalCoordsNW[KNRO_AZ])
		{
                    newAZ = values[i];
		    nset += newAZ >= 0 && newAZ <= 360.0;
		} else if (horp == &HorizontalCoordsNW[KNRO_ALT])
		{
		    newALT = values[i];
		    nset += newALT >= 0.0 && newALT <= 90.0;
		}
	    }

	  if (nset == 2)
	  {

	   targetAz  = newAZ;
	   targetAlt = newALT;
	   slew_stage = SLEW_NOW;
	   execute_slew();
	   return;
	}
	else
	{
	        HorizontalCoordsNWP.s = IPS_ALERT;
		slew_error.play();
		if (newALT < KNRO_MINIMUM_ALT)
		  IDSetNumber(&HorizontalCoordsNWP, "Error: requested coordinates are below KNRO minimum altitude limit of %d degrees.", KNRO_MINIMUM_ALT);
		else
		  IDSetNumber(&HorizontalCoordsNWP, "Az or Alt missing or badly formatted.");
		return;
	}

      }

	// ===================================
	// GeogAZphical Coords set
	if (!strcmp(GeoCoordsNP.name, name))
	{
		if (IUUpdateNumber(&GeoCoordsNP, values, names, n) < 0)
			return;

		GeoCoordsNP.s = IPS_OK;
		IDSetNumber(&GeoCoordsNP, "Geographical location updated.");

		return;
	}

	// ===================================
	// UTC Offset
	if (!strcmp(UTCOffsetNP.name, name))
	{
		if (IUUpdateNumber(&UTCOffsetNP, values, names, n) < 0)
			return;

		UTCOffsetNP.s = IPS_OK;
		IDSetNumber(&UTCOffsetNP, "UTC offset updated.");

		return;
	}

	// ===================================
	// Slew Precision
	if (!strcmp(SlewPrecisionNP.name, name))
	{
		if (IUUpdateNumber(&SlewPrecisionNP, values, names, n) < 0)
			return;

		SlewPrecisionNP.s = IPS_OK;
		IDSetNumber(&SlewPrecisionNP, NULL);
		return;
	}

	// ===================================
	// Track Precision
	if (!strcmp(TrackPrecisionNP.name, name))
	{
		if (IUUpdateNumber(&TrackPrecisionNP, values, names, n) < 0)
			return;

		TrackPrecisionNP.s = IPS_OK;
		IDSetNumber(&TrackPrecisionNP, NULL);
		return;
	}
	
	// Call respective functions for inverter
	AzInverter->ISNewNumber(dev, name, values, names, n);
	AltInverter->ISNewNumber(dev, name, values, names, n);
	
	AzEncoder->ISNewNumber(dev, name, values, names, n);
	AltEncoder->ISNewNumber(dev, name, values, names, n);

}

/**************************************************************************************
**
***************************************************************************************/
void knroObservatory::reset_all_properties()
{
	  for(list<ISwitchVectorProperty *>::iterator switch_iter = switch_list.begin(); switch_iter != switch_list.end(); switch_iter++)
	  {
	    (*switch_iter)->s	= IPS_IDLE;
	    IDSetSwitch((*switch_iter), NULL);
	  }
	  
	  for(list<INumberVectorProperty *>::iterator number_iter = number_list.begin(); number_iter != number_list.end(); number_iter++)
	  {
	    (*number_iter)->s	= IPS_IDLE;
	    IDSetNumber((*number_iter), NULL);
	  }
	  
	  for(list<ITextVectorProperty *>::iterator text_iter = text_list.begin(); text_iter != text_list.end(); text_iter++)
	  {
	    (*text_iter)->s	= IPS_IDLE;
	    IDSetText((*text_iter), NULL);
	  }
	  
	  for(list<ILightVectorProperty *>::iterator light_iter = light_list.begin(); light_iter != light_list.end(); light_iter++)
	  {
	    (*light_iter)->s	= IPS_IDLE;
	    IDSetLight((*light_iter), NULL);
	  }
	
	AzInverter->reset_all_properties();
	AltInverter->reset_all_properties();
	
	AzEncoder->reset_all_properties();
	AltEncoder->reset_all_properties();

}

/**************************************************************************************
**
// ***************************************************************************************/
void knroObservatory::connect()
{
	// For now, make sure that both inverters are connected
	if (AzInverter->connect() && AltInverter->connect() && AzEncoder->connect() && AltEncoder->connect())
	{
		ConnectSP. s = IPS_OK;
		
		// TODO Insure that last_execute_time is up to date. Otherwise, emergency parking
		// might kick in due to elapsed time.
		// time(&last_execute_time);
		
		IDSetSwitch(&ConnectSP, "KNRO is online.");
		
		pthread_create( &az_encoder_thread, NULL, &knroEncoder::update_helper, AzEncoder);
		pthread_create( &alt_encoder_thread, NULL, &knroEncoder::update_helper, AltEncoder);

	}
	else
	{
		IUResetSwitch(&ConnectSP);
		ConnectS[1].s = ISS_ON;
		ConnectSP. s = IPS_ALERT;
		IDSetSwitch(&ConnectSP, "Due to the above errors, KNRO is offline.");

	}
}

void knroObservatory::disconnect()
{
	pthread_cancel(az_encoder_thread);
	pthread_cancel(alt_encoder_thread);
	
	stop_all();
	
	AzInverter->disconnect();
	AltInverter->disconnect();
	AltEncoder->disconnect();
	AzEncoder->disconnect();

	
}

			
/**************************************************************************************
**
// ***************************************************************************************/
knroObservatory::knroErrCode knroObservatory::stop_all()
{
   // TODO
   //park_alert.stop();

   slew_busy.stop();

   // No need to do anything if we're stopped already
   if (!AzInverter->is_in_motion() && !AltInverter->is_in_motion())
     return SUCCESS;
   
   bool az_stop = stop_az();
   bool alt_stop = stop_alt();
   
   if (az_stop && alt_stop)
   {
      HorizontalCoordsNRP.s	= IPS_IDLE;
      HorizontalCoordsNWP.s	= IPS_IDLE;
      IDSetNumber(&HorizontalCoordsNRP, NULL);
      IDSetNumber(&HorizontalCoordsNWP, NULL);
      
      if (ParkSP.s == IPS_BUSY)
      {
	IUResetSwitch(&ParkSP);
	ParkSP.s = IPS_IDLE;
	IDSetSwitch(&ParkSP, "Telescope park terminated.");
      }
      return SUCCESS;
   }
   
   IDMessage(mydev, "Stopping telescope failed. Please try again.");
     // TODO Play alarm if stopping failed. 
     return INVERTER_ERROR;
     
}

/**************************************************************************************
**
***************************************************************************************/
void  knroObservatory::park_telescope()
{
	ParkS[0].s = ISS_ON;

	ParkSP.s = IPS_BUSY;
	
	targetAz = 0;
	targetAlt = 90.0;
	
	IDSetSwitch(&ParkSP, "Parking telescope, please stand by...");

	execute_slew();

}

/**************************************************************************************
**
***************************************************************************************/
void knroObservatory::terminate_parking()
{
	if (ParkSP.s != IPS_IDLE)
	{
		ParkSP.s = IPS_IDLE;
		IUResetSwitch(&ParkSP);
		IDSetSwitch(&ParkSP, "Parking cancelled or terminated.");
	}
}

/**************************************************************************************
** Slew algorithm is as following:
** We divide each slew to two stages:
** 1. Slew to requested AZ, and ALT and stop if we reach within SLEW toleAZnces.
** 2. Maintain lock on the current coordinates withing TAZCK toleAZnces.
** Slew speed is determined by the angular sepeAZtion between current and target coords.
** Once the slew stage is accomplished, tAZck state kicks in.
***************************************************************************************/
void knroObservatory::execute_slew()
{
	char AzStr[32], AltStr[32];

	fs_sexa(AzStr, targetAz, 2, 3600);
	fs_sexa(AltStr, targetAlt, 2, 3600);

	slew_stage = SLEW_NOW;
	initialAz  = currentAz;
	
	HorizontalCoordsNWP.s = IPS_BUSY;
	HorizontalCoordsNRP.s = IPS_BUSY;
	
	IDSetNumber(&HorizontalCoordsNWP, "Slewing to Az: %s Alt: %s ...", AzStr, AltStr );
	IDSetNumber(&HorizontalCoordsNRP, NULL);

	slew_busy.play();
}

/**************************************************************************************
** ToleAZnce is in arc minutes
***************************************************************************************/
bool knroObservatory::is_alt_done()
{
	double delta = 0;

	delta = targetAlt - currentAlt;

	if (slew_stage == SLEW_NOW)
	{
		if (fabs(delta) < (slewALTTolerance / 60.0))
			return true;
	}
	else if (slew_stage == SLEW_TRACK)
	{
	 	if (fabs(delta) < (trackALTTolerance / 60.0))
			return true;

	}

	return false;
}

/**************************************************************************************
** Tolerance is in arc minutes
***************************************************************************************/
bool knroObservatory::is_az_done()
{
	double delta = 0;

	delta = targetAz - currentAz;

	if (slew_stage == SLEW_NOW)
	{
		if (fabs(delta) < (slewAZTolerance / 60.0))
			return true;
	}
	else if (slew_stage == SLEW_TRACK)
	{
		if (fabs(delta) < (trackAZTolerance / 60.0))
			return true;
	}

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

/**************************************************************************************
**
***************************************************************************************/
void knroObservatory::enable_simulation()
{
	if (simulation)
		return;
	
	simulation = true;
	
	// Simulate all the devices
	AzInverter->enable_simulation();	
	AltInverter->enable_simulation();
	AzEncoder->enable_simulation();	
	AltEncoder->enable_simulation();
	
	SimulationSP.s = IPS_OK;
	IDSetSwitch(&SimulationSP, "KNRO simulation is enabled.");

}

/**************************************************************************************
**
***************************************************************************************/
void knroObservatory::disable_simulation()
{
	if (!simulation)
		return;	
	
	simulation = false;
	
	// disable simulation for all the devices
	AzInverter->disable_simulation();
	AltInverter->disable_simulation();
	AzEncoder->disable_simulation();	
	AltEncoder->disable_simulation();
	
	SimulationSP.s = IPS_OK;
	IDSetSwitch(&SimulationSP, "KNRO simulation is disabled.");
}

void knroObservatory::update_alt_speed()
{
  double delta_alt=0, current_alt_speed=0;
  
  //currentAlt = AltEncoder->get_current_angle();
  delta_alt = fabs(currentAlt - targetAlt);
  current_alt_speed = AltInverter->get_speed();
  
//  IDLog("currentAlt:  %g ## targetAlt: %g ## DeltaAlt is %g ## Current Speed is: %g\n", currentAlt, targetAlt, delta_alt, current_alt_speed);
  
  if (delta_alt <= ALT_SLOW_REGION)
  {
    if (current_alt_speed != ALT_KNRO_SLOW)
      if (AltInverter->set_speed(ALT_KNRO_SLOW) == false)
      {
	stop_all();
	IDMessage(mydev, "Error in changing Alt inverter speed. Checks logs.");
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
	IDMessage(mydev, "Error in changing Alt inverter speed. Checks logs.");
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
	IDMessage(mydev, "Error in changing Alt inverter speed. Checks logs.");
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
	  IDSetSwitch(&MovementNSSP, "Moving northward with speed %g Hz...", AltInverter->get_speed());
	  IDSetSwitch(&MovementNSSP, NULL);

	  if (simulation)
	       AltEncoder->simulate_forward();
	}
	else
	{
	        MovementNSSP.s = IPS_ALERT;
		IDSetSwitch(&MovementNSSP, "Moving northward failed. Check logs.");
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
	 IDSetSwitch(&MovementNSSP, "Moving southward with speed %g Hz...", AltInverter->get_speed());
	 IDSetSwitch(&MovementNSSP, NULL);

	  if (simulation)
	      AltEncoder->simulate_reverse();

	}
	else
	{
	        MovementNSSP.s = IPS_ALERT;
		IDSetSwitch(&MovementNSSP, "Moving southward failed. Check logs.");
	}

      break;
  }
  
}

void knroObservatory::update_az_speed()
{
  
  double delta_az=0, current_az_speed=0;
  
  //currentAz = AzEncoder->get_current_angle();
  delta_az = fabs(currentAz - targetAz);
  current_az_speed = AzInverter->get_speed();
  
  if (delta_az > 180)
    delta_az = 360 - delta_az;
    
//  IDLog("currentAz:  %g ## targetAz: %g ## DeltaAz is %g ## Current Speed is: %g\n", currentAz, targetAz, delta_az, current_az_speed);
  
  if (delta_az <= AZ_SLOW_REGION)
  {
    if (current_az_speed != AZ_KNRO_SLOW)
      if (AzInverter->set_speed(AZ_KNRO_SLOW) == false)
      {
	stop_all();
	IDMessage(mydev, "Error in changing Az inverter speed. Checks logs.");
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
	IDMessage(mydev, "Error in changing Az inverter speed. Checks logs.");
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
	IDMessage(mydev, "Error in changing Az inverter speed. Checks logs.");
      }
    if (simulation)
       AzEncoder->simulate_fast();    
  }
}

void knroObservatory::update_az_dir(AzDirection dir)
{
  switch (dir)
  {
    case KNRO_EAST:
      if (MovementWES[KNRO_EAST].s == ISS_ON)
	  return;
      
      if (AzInverter->move_forward())
	{
	  IUResetSwitch(&MovementWESP);
	  MovementWESP.s = IPS_BUSY;
	  MovementWES[KNRO_EAST].s = ISS_ON; 
	  IDSetSwitch(&MovementWESP, "Moving eastward with speed %g Hz...", AzInverter->get_speed());
	  IDSetSwitch(&MovementWESP, NULL);

	  if (simulation)
	       AzEncoder->simulate_forward();
	}
	else
	{
	        MovementWESP.s = IPS_ALERT;
		IDSetSwitch(&MovementWESP, "Moving eastward failed. Check logs.");
	}
      break;
  
    case KNRO_WEST:
      if (MovementWES[KNRO_WEST].s == ISS_ON)
	  return;
      if (AzInverter->move_reverse())
	{
 	 IUResetSwitch(&MovementWESP);
	 MovementWESP.s = IPS_BUSY;
	 MovementWES[KNRO_WEST].s = ISS_ON;
	 IDSetSwitch(&MovementWESP, "Moving westward with speed %g Hz...", AzInverter->get_speed());
	 IDSetSwitch(&MovementWESP, NULL);

	  if (simulation)
	      AzEncoder->simulate_reverse();

	}
	else
	{
	        MovementWESP.s = IPS_ALERT;
		IDSetSwitch(&MovementWESP, "Moving westward failed. Check logs.");
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

void knroObservatory::enable_debug()
{
  AzInverter->enable_debug();
  AltInverter->enable_debug();
  AzEncoder->enable_debug();
  AltEncoder->enable_debug();
}

void knroObservatory::disable_debug()
{
  AzInverter->disable_debug();
  AltInverter->disable_debug();
  AzEncoder->disable_debug();
  AltEncoder->disable_debug();
}

 
void knroObservatory::check_slew_state()
{ 
    // Check if we're slewing or tracking
    // TODO check for tracking later..etc.
    if (HorizontalCoordsNWP.s == IPS_BUSY)
    {
      slew_stage = SLEW_NONE;
      HorizontalCoordsNWP.s = IPS_IDLE;
      HorizontalCoordsNRP.s = IPS_IDLE;
      IDSetNumber(&HorizontalCoordsNWP, "Automatic slew interrupted by motion command.");
      IDSetNumber(&HorizontalCoordsNRP, NULL);
    }   
}
