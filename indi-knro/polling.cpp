/*
    Ujari Observatory INDI Driver
    Copyright (C) 2007 Jasem Mutlaq (mutlaqja@ikarustech.com)

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
    2008-05-10: Moving polling to a seperate file.

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

#include "indicom.h"

// For playing OGG
#include "ogg_util.h"

#include "knro.h"
/**************************************************************************************
**
***************************************************************************************/
void knroObservatory::ISPoll()
{

	
	//AzEncoder->update_encoder_count();
	//AltEncoder->update_encoder_count();



#if 0

	if (is_connected() == false)
		return;

	double JD=0, SD=0, delta_ra=0, dome_delta;
        dms SD_Time;
	SkyPoint requested_coords;

	/***********************************************************************************************************
	************************************************** NOTE ****************************************************
	************************************************************************************************************
	***************************************** NORTH is POSITIVE COUNT ******************************************
        ***************************************** WEST is NEGATIVE COUNT *******************************************
	************************************************************************************************************
	************************************************** NOTE ****************************************************
        ************************************************************************************************************/

	/***************************************************************
	// #1: Get encoder counts
	****************************************************************/
	/* Update Encoder Values */
	pthread_mutex_lock( &encoder_mutex );
	EncoderPosN[EQ_DEC].value = ns_encoder_count;
	EncoderPosN[EQ_RA].value = we_encoder_count;
	EncoderPosN[2].value = dome_encoder_count;

	EncoderAbsPosN[EQ_DEC].value = ns_absolute_position;
	EncoderAbsPosN[EQ_RA].value = we_absolute_position;
	EncoderAbsPosN[2].value = dome_absolute_position;

	// Calculate Dome Angle
	dome_delta = ( (int) ((dome_absolute_position / DOME_TPD) + DOME_HOME_DEVIATION)) % 360;
	if (dome_delta < 0)
		dome_delta += 360;
	EncoderAbsPosN[3].value = dome_delta;

	//IDLog("We got encoder values of %g and %g\n", EncoderPosN[0].value, EncoderPosN[1].value);
	pthread_mutex_unlock( &encoder_mutex );
	/****************************************************************/

	// Update Dome Az
	currentDomeAz = EncoderAbsPosN[3].value;
	IDSetNumber(&DomeAzNP, NULL);

	#ifdef SIMULATION
	if (MovementNSSP.s == IPS_BUSY)
	{
		if (MovementNSS[UJARI_NORTH].s == ISS_ON)
			ns_absolute_position += current_dec_rate;
		else
			ns_absolute_position -= current_dec_rate;
	}
	
	if (MovementWESP.s == IPS_BUSY)
	{
		if (MovementWES[UJARI_WEST].s == ISS_ON)
			we_absolute_position -= current_ra_rate;
		else
			we_absolute_position += current_ra_rate;
	}
	#endif
	
	/***************************************************************
	// #2: Setup JD & LST
	****************************************************************/
	JD = ln_get_julian_from_sys();				// Current Julian Day
	SD = ln_get_mean_sidereal_time(JD);			// Current Mean Sideral Time
	SD_Time.setH(SD + observatory.lng()->Hours());		// Current Local Apparent Sideral Time
	/* Store LST */
	currentLST = SD_Time.Hours();

	/***************************************************************
	// #3: Calculate current RA, DEC
	****************************************************************/
	// Calculate HA, DEC
	currentHA = ( (EncoderAbsPosN[EQ_RA].value) / (15.0 * RA_CPD));
	currentDEC = (EncoderAbsPosN[EQ_DEC].value) / DEC_CPD + currentLat;

	rawRA   = currentLST + currentHA;
        rawDEC  = currentDEC;

        // Correction derived from Excel
	//currentDEC -= (-0.02 * currentHA * currentHA) + (0.001 * currentHA) + 0.027;

	// Check if we want to apply TPoint
	if (TPointS[0].s == ISS_ON)
		standard_model.apply_model(currentHA, currentDEC);

	// Calculate RA
	currentRA  = currentLST + currentHA;

	// 2008-03-17 ROUGH CORRECTION
	currentRA += currentHA / 68.1818;
	
	if (currentRA > 24.)
		currentRA -= 24.;
	else if (currentRA < 0.)
		currentRA += 24.;
	
	if (currentDEC > 90.)
		currentDEC = 180. - currentDEC;
	else if (currentDEC < -90.)
		currentDEC = -180. - currentDEC;
	/****************************************************************/

	/***************************************************************
	// #4: Calculate current Alt
	****************************************************************/
	requested_coords.setRA(currentRA);
	requested_coords.setDec(currentDEC);
	requested_coords.EquatorialToHorizontal(&SD_Time, observatory.lat());
	/* Store Current Alt */
	currentAlt = requested_coords.alt()->Degrees();

	/***************************************************************
	// #5: Check Safety
	****************************************************************/
	check_safety();

	/***************************************************************
	// #6: Set all properties
	****************************************************************/
	IDSetNumber(&EquatorialCoordsNP, NULL);
	IDSetNumber(&EncoderPosNP, NULL);
	IDSetNumber(&EncoderAbsPosNP, NULL);
	//IDLog("We've got SD (Hours) = %g and we_absolute_position = %d\n", SD_Time.Hours(), we_absolute_position);

	delta_ra = currentRA - targetRA;
	/***************************************************************
	// #7: Check Status of Equatorial Coord Request
	****************************************************************/
	switch(EquatorialCoordsNP.s)
	{
		case IPS_IDLE:
			break;

		case IPS_OK:
			break;

		case IPS_BUSY:
			switch (slew_stage)
			{
				case SLEW_NONE:
				break;

				case SLEW_NOW:
					// If declination is done, stop n/s
					if (is_dec_done())
						stop_ns();
					else if (targetDEC - currentDEC > 0)
					{
						update_ns_speed();
						update_ns_dir(UJARI_NORTH);
					}
					else
					{
						update_ns_speed();
						update_ns_dir(UJARI_SOUTH);
					}

					// if right ascension is done, stop w/e
					//IDMessage(mydev, "The delta RA is %g", delta_ra);
					if (is_ra_done())
					{
						//IDMessage(mydev, "Yup RA is done!");
						stop_we(); 
					}
					else if ( (delta_ra > 0 && delta_ra < 12) || (delta_ra > -24 && delta_ra < -12))
					{
						update_we_speed();
						update_we_dir(UJARI_WEST);
					}
					else
					{
						update_we_speed();
						update_we_dir(UJARI_EAST);
					}

					// if both ns and we are stopped, we're done and engage tracking.
					if (MovementNSSP.s == IPS_IDLE && MovementWESP.s == IPS_IDLE && DomeWESP.s != IPS_BUSY)
					{

						if (slew_complete.is_playing() == false)
							slew_complete.play();

						// If we're parking, finish off here
						if (ParkSP.s == IPS_BUSY || ParkSP.s == IPS_ALERT)
						{
							ParkSP.s = IPS_OK;
							EquatorialCoordsNP.s = IPS_IDLE;
							IUResetSwitch(&ParkSP);
							slew_stage = SLEW_NONE;

							if (park_alert.is_playing())
							{
								time(&last_execute_time);
								park_alert.stop();
							}

							if (calibration_error.is_playing())
								calibration_error.stop();

							IDSetSwitch(&ParkSP, "Telescope park complete. Please lock the telescope now.");
							IDSetNumber(&EquatorialCoordsNP, NULL);
							return;
						}

						slew_stage = SLEW_TRACK;
						start_sidereal();
						IDSetNumber(&EquatorialCoordsNP, "Slew complete. Engaging tracking...");
						char RAStr[32], DecStr[32];
						fs_sexa(RAStr, rawRA, 2, 3600);
						fs_sexa(DecStr, rawDEC, 2, 3600);
						if (UJARI_DEBUG)
							IDMessage(mydev, "Raw RA: %s , Raw DEC: %s", RAStr, DecStr);
					}
					break;

				case SLEW_TRACK:
					if (is_dec_done())
						stop_ns();
					else if (targetDEC - currentDEC > 0)
					{
						update_ns_speed();
						update_ns_dir(UJARI_NORTH);
					}
					else
					{
						update_ns_speed();
						update_ns_dir(UJARI_SOUTH);
					}

					requested_coords.setRA(targetRA);
					requested_coords.setDec(targetDEC);
					requested_coords.EquatorialToHorizontal(&SD_Time, observatory.lat());
					/* Store Target Az */
					targetDomeAz  = ( (int) requested_coords.az()->Degrees() ) % 360;
					/*if (is_ra_done())
						stop_we();
					else if ( (delta_ra > 0 && delta_ra < 12) || delta_ra < -12)
					{
						update_we_speed();
						update_we_dir(UJARI_WEST);
					}
					else
					{
						update_we_speed();
						update_we_dir(UJARI_EAST);
					}*/
					break;
					
			}
			break;

		case IPS_ALERT:
			break;
	}


	// TEMP ONLY, write encoder values
	#if 0
	if (en_record != NULL && (MovementWESP.s == IPS_BUSY || MovementNSSP.s == IPS_BUSY))
	{
		static double last_abs_ra= EncoderAbsPosN[EQ_RA].value;
		char LST_str[32], full_line[512];
		fs_sexa(LST_str, currentLST, 2, 3600);

		snprintf(full_line, sizeof(full_line), "%s	%g	%g	%g	%g	%g\n", LST_str, EncoderAbsPosN[EQ_RA].value, EncoderPosN[EQ_RA].value, (EncoderAbsPosN[EQ_RA].value - last_abs_ra) ,EncoderAbsPosN[EQ_DEC].value, EncoderPosN[EQ_DEC].value);
		fputs( full_line, en_record);
		last_abs_ra= EncoderAbsPosN[EQ_RA].value;
   	}
	#endif

	/*if (DomeWESP.s == IPS_BUSY)
	{
		static double last_abs_dome= EncoderAbsPosN[2].value;
		char LST_str[32], full_line[512];
		//fs_sexa(LST_str, currentLST, 2, 3600);

		snprintf(full_line, sizeof(full_line), "%g	%g\n", EncoderAbsPosN[2].value, EncoderAbsPosN[2].value - last_abs_dome);
		IDLog("%s", full_line);
		//fputs(full_line, en_record);
		last_abs_dome= EncoderAbsPosN[2].value;
   	}*/

	/***************************************************************
	// #8: Check Status of Equatorial Coord Request
	****************************************************************/
	time_t delta;
	time(&now);
	delta= now - last_execute_time;

	if (EquatorialCoordsNP.s == IPS_IDLE && ParkSP.s == IPS_IDLE && MovementNSSP.s == IPS_IDLE && MovementWESP.s == IPS_IDLE && fabs(currentDEC - currentLat) > 1)
	{
		// Play alert 10 seconds before starting park
		if (delta > (MAXIMUM_IDLE_TIME - 10) && !park_alert.is_playing() && is_connected() == true)
		{
			
			park_alert.play();
			IDMessage(mydev, "The telescope has been idle for more than %d minutes. Initiating automatic shutdown procedure in 10 seconds.", MAXIMUM_IDLE_TIME/60);
		}
		else if (delta > MAXIMUM_IDLE_TIME)
			park_telescope(true);
	}

	if (SDSP.s == IPS_BUSY)
	{
		SDRateN[0].value = fabs((lastRA - currentRA) * 15.0) / delta;
		IDSetNumber(&SDRateNP, NULL);
	}

	/***************************************************************
	// #8: Check Dome
	****************************************************************/
	static double last_dome_az=0;
	dome_delta= fabs(targetDomeAz - currentDomeAz);

	switch (DomeAzNP.s)
	{
		case IPS_IDLE:
		case IPS_OK:
			// Only if AutoDome is On
			if (AutoDomeS[0].s == ISS_ON)
			{
				if (dome_delta > DOME_TOLERANCE)
				{
					sync_dome(targetDomeAz);
					return;
				}
			}
			if (fabs(last_dome_az - currentDomeAz) >= 1)
			{
				last_dome_az = currentDomeAz;
				IDSetNumber(&DomeAzNP, NULL);
			}
			break;
		case IPS_ALERT:
			break;

		case IPS_BUSY:
			if (dome_delta <= DOME_TOLERANCE)
			{
				stop_dome();
				DomeAzNP.s = IPS_OK;
			}
			IDSetNumber(&DomeAzNP, NULL);
			break;
	}

#endif
	
}
