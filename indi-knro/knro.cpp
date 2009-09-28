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

//=========================================================
//=========================================================
// ======== FIXME DISABLE SIMULATION FOR PRODUCTION =======
//#define SIMULATION
//=========================================================
//=========================================================

using namespace std;

auto_ptr<knroObservatory> KNRO_observatory(0);		/* Autoptr to observatory */

#ifdef SIMULATION
const int POLLMS = 1000;	
#else
const int POLLMS = 1000;						/* Status loop runs every 500 ms or 5 Hz */
#endif

pthread_mutex_t knroObservatory::encoder_mutex = PTHREAD_MUTEX_INITIALIZER;

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

  spectrometer = new knroSpectrometer();
  
  init_properties();

  #ifndef SIMULATION
  init_encoder_thread();
  #endif
}

/**************************************************************************************
**
***************************************************************************************/
knroObservatory::~knroObservatory()
{
  pthread_cancel(encoder_thread);
}

/**************************************************************************************
**
***************************************************************************************/
void knroObservatory::init_properties()
{
  
  /**************************************************************************/
  IUFillSwitch(&ConnectS[0], "CONNECT", "Connect", ISS_OFF);
  IUFillSwitch(&ConnectS[1], "DISCONNECT", "Disconnect", ISS_ON);
  IUFillSwitchVector(&ConnectSP, ConnectS, NARRAY(ConnectS), mydev, "CONNECTION", "Connection", COMM_GROUP, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);
  /**************************************************************************/

  /**************************************************************************/
  /**************************************************************************/

  /**************************************************************************/
  IUFillSwitch(&AbortSlewS[0], "ABORT", "Abort All", ISS_OFF);
  IUFillSwitchVector(&AbortSlewSP, AbortSlewS, NARRAY(AbortSlewS), mydev, "ABORT_MOTION", "ABORT", BASIC_GROUP, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);
  /**************************************************************************/
  
  /**************************************************************************/
  IUFillSwitch(&ParkS[0], "PARK", "Park Telescope", ISS_OFF);
  IUFillSwitchVector(&ParkSP, ParkS, NARRAY(ParkS), mydev, "PARK", "Park", BASIC_GROUP, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);
  /**************************************************************************/

  /**************************************************************************/
  IUFillNumber(&HorizontalCoordsN[0], "AZ",  "AZ  H:M:S", "%10.6m",  0., 24., 0., 0.);
  IUFillNumber(&HorizontalCoordsN[1], "ALT", "ALT D:M:S", "%10.6m", -90., 90., 0., 0.);
  IUFillNumberVector(&HorizontalCoordsNP, HorizontalCoordsN, NARRAY(HorizontalCoordsN), mydev, "Horizontal_EOD_COORD", "Horizontal Coords", BASIC_GROUP, IP_RW, 120, IPS_IDLE);
  /**************************************************************************/

  /**************************************************************************/
  IUFillSwitch(&OnCoordSetS[0],"TRACK", "Track", ISS_ON);
  IUFillSwitchVector(&OnCoordSetSP, OnCoordSetS, NARRAY(OnCoordSetS), mydev, "ON_COORD_SET", "On Set", BASIC_GROUP, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);
  /**************************************************************************/

  /**************************************************************************/
  IUFillNumber(&GeoCoordsN[0], "LAT",  "Lat.  D:M:S +N", "%10.6m",  -90.,  90., 0., 0.);
  IUFillNumber(&GeoCoordsN[1], "LONG", "Long. D:M:S +E", "%10.6m", 0., 360., 0., 0.);
  IUFillNumberVector(&GeoCoordsNP,  GeoCoordsN, NARRAY(GeoCoordsN),  mydev, "GEOGAZPHIC_COORD", "Geographic Location", SITE_GROUP, IP_RW, 0., IPS_IDLE);
  /**************************************************************************/

  /**************************************************************************/
  IUFillNumber(&UTCOffsetN[0], "OFFSET", "Offset", "%0.3g" , -12.,12.,0.5,3.);
  IUFillNumberVector(&UTCOffsetNP, UTCOffsetN, NARRAY(UTCOffsetN), mydev, "OFFSET_UTC", "UTC Offset", SITE_GROUP, IP_RW, 0, IPS_IDLE);
  /**************************************************************************/

  /**************************************************************************/
  IUFillLight(&AltSafetyL[0], "Altitude Status", "", IPS_IDLE);
  IUFillLightVector(&AltSafetyLP, AltSafetyL, NARRAY(AltSafetyL), mydev, "Safety", "", TELESCOPE_GROUP, IPS_IDLE);
  /**************************************************************************/
 
  /**************************************************************************/
  IUFillNumber(&SlewPrecisionN[0], "SlewAZ",  "AZ (arcmin)", "%10.6m",  0., 10., 0.1, 0.8);
  IUFillNumber(&SlewPrecisionN[1], "SlewALT", "ALT (arcmin)", "%10.6m", 0., 10., 0.1, 0.8);
  IUFillNumberVector(&SlewPrecisionNP, SlewPrecisionN, NARRAY(SlewPrecisionN), mydev, "Slew Precision", "", OPTIONS_GROUP, IP_RW, 0, IPS_IDLE);
  /**************************************************************************/

  /**************************************************************************/
  IUFillNumber(&TrackPrecisionN[0], "TrackAZ", "AZ (arcmin)", "%10.6m",  0., 0.1, 1., 1.0);
  IUFillNumber(&TrackPrecisionN[1], "TrackALT", "ALT (arcmin)", "%10.6m", 0., 0.1, 1., 1.0);
  IUFillNumberVector(&TrackPrecisionNP, TrackPrecisionN, NARRAY(TrackPrecisionN), mydev, "Tracking Precision", "", OPTIONS_GROUP, IP_RW, 0, IPS_IDLE);
  /**************************************************************************/

  /**************************************************************************/
  IUFillSwitch(&DebugS[0], "Enable", "", ISS_OFF);
  IUFillSwitch(&DebugS[1], "Disable", "", ISS_ON);
  IUFillSwitchVector(&DebugSP, DebugS, NARRAY(DebugS), mydev, "Debug", "", OPTIONS_GROUP, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);
  /**************************************************************************/
  
  /**************************************************************************/
  IUFillSwitch(&SimulationS[0], "Enable", "", ISS_OFF);
  IUFillSwitch(&SimulationS[1], "Disable", "", ISS_ON);
  IUFillSwitchVector(&SimulationSP, SimulationS, NARRAY(SimulationS), mydev, "Simulation", "", OPTIONS_GROUP, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);
  /**************************************************************************/
  

#if 0

// TODO re-enable this!

  park_alert.set_looping( true );
  park_alert.load_file("/usr/share/media/alarm1.ogg");
  slew_complete.load_file("/usr/share/media/slew_complete.ogg");
  slew_error.load_file("/usr/share/media/slew_error.ogg");
  calibAZtion_error.set_looping(true);
  calibAZtion_error.load_file("/usr/share/media/calibration_error.ogg");

#endif

  //en_record = fopen("/home/knro/en_record.txt", "w");

  //if (en_record == NULL)
//	IDLog("Error: cannot open file en_record.txt for writing. %s\n", strerror(errno));

  simulation = false;
  targetAZ  = 0;
  targetALT = 0;
  currentAlt = 0;
  targetObjectAz = -1;
  currentLST = 0;

  slew_stage = SLEW_NONE;

  time(&now);
  last_execute_time=now;

  az_speed_change = false;
  alt_speed_change  = false;

}

/**************************************************************************************
**
***************************************************************************************/
void knroObservatory::init_encoder_thread()
{
   int thread_result=0;

   // Create the encoder thread.
   //thread_result = pthread_create( &encoder_thread, NULL, init_encoders, this);
}

/**************************************************************************************
**
***************************************************************************************/
void knroObservatory::ISGetProperties(const char *dev)
{
	// Return if it's not our device
	if (dev && strcmp(mydev, dev))
	   return;

	// Comm Group
	IDDefSwitch(&ConnectSP, NULL);
	//IDDefText(&PortTP, NULL);

	// Main Control Group
	IDDefNumber(&HorizontalCoordsNP, NULL);
	IDDefSwitch(&OnCoordSetSP, NULL);
    IDDefSwitch(&ParkSP, NULL);
	IDDefSwitch(&AbortSlewSP, NULL);

	// Telescope Group
	IDDefLight(&AltSafetyLP, NULL);
	
	// Encoder Group
	AzEncoder->ISGetProperties();
	AltEncoder->ISGetProperties();
	
	// Inverter Group
	AzInverter->ISGetProperties();
	AltInverter->ISGetProperties();

	spectrometer->ISGetProperties();
	
/* FOR SIMULATION ONLY */
	//IDDefSwitch(&AZEncSP, NULL);
	//IDDefSwitch(&ALTEncSP, NULL);
	/********** END SIMULATION */

	// Site Group
	IDDefNumber(&GeoCoordsNP, NULL);
        IDDefNumber(&UTCOffsetNP, NULL);

	// Options Group
	IDDefNumber(&SlewPrecisionNP, NULL);
	IDDefNumber(&TrackPrecisionNP, NULL);
	IDDefSwitch(&DebugSP, NULL);
	IDDefSwitch(&SimulationSP, NULL);

	/*double h=3.92994;
	double d=0.376669;
	standard_standard_model.apply_standard_model(h,d);
	IDLog("H: %g\nd: %g\n", h, d);*/
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
			init_knro();
		else
		{
			stop_all();
			//TODO
			//park_alert.stop();
			reset_all_properties(true);
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
			IDSetSwitch(&AbortSlewSP, "Aborting All Motion.");

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
			IDSetSwitch(&StopAllSP, "Aborting All Motion.");

		return;
	}

	// ===================================
	// Park telescope to home position
	if (!strcmp(ParkSP.name, name))
	{
		if (IUUpdateSwitch(&ParkSP, states, names, n) < 0)
			return;

		park_telescope();

		return;
	}
	
	// ===================================
	// On Coord Set Switch
	if (!strcmp(OnCoordSetSP.name, name))
	{
		if (IUUpdateSwitch(&OnCoordSetSP, states, names, n) < 0)
			return;

		// We don't do much here, this opeAZtion is always successful
		// as we only have slew, we don't need tAZck or other options.
		OnCoordSetSP.s = IPS_OK;
		IDSetSwitch(&OnCoordSetSP, NULL);
		return;

	}

	// ===================================
	// ALT Encoder Simulation
	if (!strcmp(ALTEncSP.name, name))
	{
		if (IUUpdateSwitch(&ALTEncSP, states, names, n) < 0)
			return;

		// Increase
		if (ALTEncS[0].s == ISS_ON)
			alt_absolute_position += 1000;			
		else
			alt_absolute_position -= 1000;

		/*if (alt_absolute_position < S_ENCODER_LIMIT)
			alt_absolute_position = S_ENCODER_LIMIT;
		else if (alt_absolute_position > N_ENCODER_LIMIT)
			alt_absolute_position = N_ENCODER_LIMIT;*/

		IUResetSwitch(&ALTEncSP);
		ALTEncSP.s = IPS_OK;
		IDSetSwitch(&ALTEncSP, NULL);

		//IDLog("alt_absolute_position: %d\n", alt_absolute_position);
		return;
	}

	// ===================================
	// AZ Encoder Simulation
	if (!strcmp(AZEncSP.name, name))
	{
		if (IUUpdateSwitch(&AZEncSP, states, names, n) < 0)
			return;

		// Increase
		if (AZEncS[0].s == ISS_ON)
			az_absolute_position += 100;			
		else
			az_absolute_position -= 100;

		/*if (az_absolute_position < E_ENCODER_LIMIT)
			az_absolute_position = E_ENCODER_LIMIT;
		else if (az_absolute_position > W_ENCODER_LIMIT)
			az_absolute_position = W_ENCODER_LIMIT;*/

		IUResetSwitch(&AZEncSP);
		AZEncSP.s = IPS_OK;
		IDSetSwitch(&AZEncSP, NULL);

		//IDLog("az_absolute_position: %d\n", az_absolute_position);
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
		return;
	}
	
	
	AzInverter->ISNewSwitch(dev, name, states, names, n);
	AltInverter->ISNewSwitch(dev, name, states, names, n);
	
	AzEncoder->ISNewSwitch(dev, name, states, names, n);
	AltEncoder->ISNewSwitch(dev, name, states, names, n);
	
	spectrometer->ISNewSwitch(dev, name, states, names, n);
	

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
	
	spectrometer->ISNewText(dev, name, texts, names, n);

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
	// EquatoAZil Coords set
	if (!strcmp(HorizontalCoordsNP.name, name))
	{
	    double newAZ =0, newALT =0;
	    // FIXME replace the following with standard
	    //SkyPoint target_coords;
	    //dms SD_Time;
	    int i=0, nset=0;

	    for (nset = i = 0; i < n; i++)
	    {
		INumber *eqp = IUFindNumber (&HorizontalCoordsNP, names[i]);
		if (eqp == &HorizontalCoordsN[0])
		{
                    newAZ = values[i];
		    nset += newAZ >= 0 && newAZ <= 24.0;
		} else if (eqp == &HorizontalCoordsN[1])
		{
		    newALT = values[i];
		    nset += newALT >= -90.0 && newALT <= 90.0;
		}
	    }

	  if (nset == 2)
	  {

	   targetAZ  = newAZ;
	   targetALT = newALT;

	   return;
	}
	else
	{
	        HorizontalCoordsNP.s = IPS_ALERT;
		IDSetNumber(&HorizontalCoordsNP, "AZ or ALT missing or badly formatted.");
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
		IDSetNumber(&GeoCoordsNP, "GeogAZphical location updated.");

		// FIXME replace this with standard
		//observatory.setLat( dms(currentLat));
	  	//observatory.setLong( dms(currentLong));	

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

		//observatory.setTZ( (int) UTCOffsetN[0].value);

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
void knroObservatory::reset_all_properties(bool reset_to_idle)
{
	if (reset_to_idle)
	{
		ConnectSP.s		= IPS_IDLE;
		AbortSlewSP.s		= IPS_IDLE;
		HorizontalCoordsNP.s 	= IPS_IDLE;
        OnCoordSetSP.s		= IPS_IDLE;
		AltSafetyLP.s		= IPS_IDLE;
		SlewPrecisionNP.s	= IPS_IDLE;
		TrackPrecisionNP.s	= IPS_IDLE;
		DebugSP.s		= IPS_IDLE;
		
	}

	IDSetSwitch(&ConnectSP, NULL);
	IDSetSwitch(&AbortSlewSP, NULL);
	IDSetNumber(&HorizontalCoordsNP, NULL);
	IDSetSwitch(&OnCoordSetSP, NULL);
	IDSetLight(&AltSafetyLP, NULL);
	IDSetNumber(&SlewPrecisionNP, NULL);
	IDSetNumber(&TrackPrecisionNP, NULL);
	IDSetSwitch(&DebugSP, NULL);
	
	AzInverter->reset_all_properties(reset_to_idle);
	AltInverter->reset_all_properties(reset_to_idle);
	
	AzEncoder->reset_all_properties(reset_to_idle);
	AltEncoder->reset_all_properties(reset_to_idle);

	spectrometer->reset_all_properties(reset_to_idle);


	
}

/**************************************************************************************
**
// ***************************************************************************************/
void knroObservatory::init_knro()
{
	//FIXME disable spectrometer simulation
	spectrometer->enable_simulation();
	//AzInverter->enable_simulation();
	//AltInverter->enable_simulation();

	// For now, make sure that both inverters are connected
//	if (AzInverter->connect() && AltInverter->connect() && AzEncoder->connect() && AltEncoder->connect())
	if (spectrometer->connect())
//	if (AzInverter->connect() && AltInverter->connect() && spectrometer->connect())
	{
		ConnectSP. s = IPS_OK;
		
		// Insure that last_execute_time is up to date. Otherwise, emergency parking
		// might kick in due to elapsed time.
		// time(&last_execute_time);
		
		IDSetSwitch(&ConnectSP, "KNRO is online.");
	}
	else
	{
		IUResetSwitch(&ConnectSP);
		ConnectS[1].s = ISS_ON;
		ConnectSP. s = IPS_ALERT;
		IDSetSwitch(&ConnectSP, "Due to the above errors, KNRO is offline.");
	}
}
			
/**************************************************************************************
**
// ***************************************************************************************/
knroObservatory::knroErrCode knroObservatory::stop_all()
{
   knroErrCode error_code = SUCCESS;

   // TODO
   //park_alert.stop();

   if (error_code != SUCCESS)
	return error_code;

   return SUCCESS;
}

/**************************************************************************************
**
***************************************************************************************/
void  knroObservatory::park_telescope(bool is_emergency)
{
	targetAZ   = currentLST;
	targetALT  = currentLat;

	ParkS[0].s = ISS_ON;

	if (is_emergency)
	{
		ParkSP.s = IPS_ALERT;
		IDSetSwitch(&ParkSP, "Alert! Initiating emergency park procedure.");
	}
	else
	{
		ParkSP.s = IPS_BUSY;
		IDSetSwitch(&ParkSP, "Parking telescope, please stand by...");
	}

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
	char AZStr[32], ALTStr[32];

	fs_sexa(AZStr, targetAZ, 2, 3600);
	fs_sexa(ALTStr, targetALT, 2, 3600);

	slew_stage = SLEW_NOW;
	
	HorizontalCoordsNP.s = IPS_BUSY;
	
	IDSetNumber(&HorizontalCoordsNP, "Slewing to AZ: %s ALT: %s ...", AZStr, ALTStr );
}

/**************************************************************************************
** ToleAZnce is in arc minutes
***************************************************************************************/
bool knroObservatory::is_alt_done()
{
	double delta = 0;

	delta = targetALT - currentALT;

	// For parking, we check againt absolute encoder
	if (ParkSP.s == IPS_BUSY)
	{
		/* TODO 
		if (fabs(EncodeAZbsPosN[EQ_ALT].value) <= ALT_CPD/60.0)
			return true;
		else
			return false;
		*/
	}

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

	delta = targetAZ - currentAZ;

	// For parking, we check againt absolute encoder
	if (ParkSP.s == IPS_BUSY)
	{

		/* TODO 
		if (fabs(EncodeAZbsPosN[EQ_AZ].value) <= AZ_CPD/60.0)
			return true;
		else
		{
			targetAZ = currentLST;
			return false;
		}
		*/
	}

	if (slew_stage == SLEW_NOW)
	{
		if (fabs(delta) < (slewAZTolerance / 900.0))
			return true;
	}
	else if (slew_stage == SLEW_TRACK)
	{
		if (fabs(delta) < (trackAZTolerance / 900.0))
			return true;
	}

	return false;
}

/**************************************************************************************
**
***************************************************************************************/
void knroObservatory::check_safety()
{
	// TODO: Redo this
	#if 0

  // If it's critical, just park the telescope.
  // There is still a problem of the telescope going south by itself. Probably due to motor problems.
  if (currentAlt <= ALT_CRITICAL_ZONE)
  {
	park_telescope(true);

	return;
  }

  if (currentAlt <= MINIMUM_SAFE_ALT && ParkSP.s != IPS_ALERT)
  {
	double newAlt=0, delta_AZ=0, delta_ALT=0;

	if (MovementNSS[KNRO_NORTH].s == ISS_ON)
		delta_ALT += 0.1;
	else if (MovementNSS[KNRO_SOUTH].s == ISS_ON)
		delta_ALT -= 0.1;

	if (MovementWES[KNRO_EAST].s == ISS_ON)
		delta_AZ += 0.1;
	else if (MovementWES[KNRO_WEST].s == ISS_ON)
		delta_AZ -= 0.1;

	if (newAlt > currentAlt)
		return;

	if (MovementNSSP.s == IPS_BUSY || MovementWESP.s == IPS_BUSY || slew_stage == SLEW_TRACK)
	{
		IDMessage(mydev, "Emergency stop. Current Alt: %g, new Alt: %g", currentAlt, newAlt);
		stop_telescope();
	}
  }

  if ( currentAlt <= ALT_DANGER_ZONE )
  {
	if (AltSafetyLP.s == IPS_ALERT) return;

	AltSafetyL[0].s = IPS_ALERT;
	AltSafetyLP.s = IPS_ALERT;
	IDSetLight(&AltSafetyLP, "Attention! Telescpe entering danger zone!");
	return;
  }
  else if ( currentAlt <= ALT_WARNING_ZONE )
  {
	if (AltSafetyLP.s == IPS_BUSY) return;

	AltSafetyL[0].s = IPS_BUSY;
	AltSafetyLP.s = IPS_BUSY;
	IDSetLight(&AltSafetyLP, "Attention. Telescpe entering warning zone.");
	return;
  }
  else if (currentAlt >= MINIMUM_SAFE_ALT)
  {
	if (AltSafetyLP.s == IPS_OK) return;

	AltSafetyL[0].s = IPS_OK;
	AltSafetyLP.s = IPS_OK;
	IDSetLight(&AltSafetyLP, "Telescope is within safety limits.");
	return;
  }
  
  #endif
  
}


/**************************************************************************************
**
***************************************************************************************/
const char * knroObservatory::get_knro_error_string(knroErrCode code)
{
	switch (code)
	{

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
	
	// Simulate all the devices
	AzInverter->disable_simulation();
	AltInverter->disable_simulation();
	AzEncoder->disable_simulation();	
	AltEncoder->disable_simulation();
	
	SimulationSP.s = IPS_OK;
	IDSetSwitch(&SimulationSP, "KNRO simulation is disabled.");
}


