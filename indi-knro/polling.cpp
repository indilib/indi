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
    double delta_az=0;

     if (is_connected() == false)
	return;

    currentAz   = AzEncoder->get_angle();
    currentAlt  = AltEncoder->get_angle();
    IDSetNumber(&HorizontalCoordsNRP, NULL);

     delta_az = currentAz - targetAz;
	 
	/***************************************************************
	// #7: Check Status of Equatorial Coord Request
	****************************************************************/
	switch(HorizontalCoordsNWP.s)
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
					if (is_alt_done())
						stop_alt();
					else if (currentAlt - targetAlt > 0)
					{
						update_alt_speed();
						update_alt_dir(KNRO_NORTH);
					}
					else
					{
						update_alt_speed();
						update_alt_dir(KNRO_SOUTH);
					}
  
					if (is_az_done())
					{
						stop_az(); 
					}
					else if ((delta_az > 0 && delta_az < 180) || (delta_az > -360 && delta_az < -180))

					{
						update_az_speed();

						// Telescope moving west, but we need to avoid hitting limit switch which is @ 90
						if (initialAz > 90 && targetAz < 90)
							update_az_dir(KNRO_EAST);
						else
						// Otherwise proceed normally
							update_az_dir(KNRO_WEST);
					}
					else
					{
						update_az_speed();
						// Telescope moving east, but we need to avoid hitting limit switch which is @ 90
						if (initialAz < 90 && targetAz > 90)
							update_az_dir(KNRO_WEST);
						else
							update_az_dir(KNRO_EAST);
					}

					// if both ns and we are stopped, we're done and engage tracking.
					if (MovementNSSP.s == IPS_IDLE && MovementWESP.s == IPS_IDLE)
					{

						//if (slew_complete.is_playing() == false)
							//slew_complete.play();

						// If we're parking, finish off here
						
						if (ParkSP.s == IPS_BUSY)
						{
						        IUResetSwitch(&ParkSP);
							ParkSP.s = IPS_OK;
							HorizontalCoordsNWP.s = IPS_OK;
							HorizontalCoordsNRP.s = IPS_OK;
							
							slew_stage = SLEW_NONE;
							slew_busy.stop();
							slew_complete.play();


							IDSetSwitch(&ParkSP, "Telescope park complete.");
							IDSetNumber(&HorizontalCoordsNWP, NULL);
							IDSetNumber(&HorizontalCoordsNRP, NULL);
						
							return;
						}

						// FIXME Make sure to check whether on_set_coords is set to SLEW or TRACK
						// Because when slewing, we're done here, but if there is tracking, then
						// We go to that mode
						slew_busy.stop();
						slew_complete.play();
						slew_stage = SLEW_TRACK;
						HorizontalCoordsNWP.s = IPS_OK;
						HorizontalCoordsNRP.s = IPS_OK;
						IDSetNumber(&HorizontalCoordsNWP, "Slew complete. Engaging tracking...");
						IDSetNumber(&HorizontalCoordsNRP, NULL);
						
						char AzStr[32], AltStr[32];
						fs_sexa(AzStr, currentAz, 2, 3600);
						fs_sexa(AltStr, currentAlt, 2, 3600);
						//if (UJARI_DEBUG)
						//	IDMessage(mydev, "Raw RA: %s , Raw DEC: %s", RAStr, DecStr);
					}
					break;

				case SLEW_TRACK:
				        #if 0
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
					#endif
					break;
					
			}
			break;

		case IPS_ALERT:
			break;
	}

  return;
  

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
