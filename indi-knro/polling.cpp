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

#define RA_TOLERANCE 0.01
#define DEC_TOLERANCE 0.01

/**************************************************************************************
**
***************************************************************************************/
void knroObservatory::ISPoll()
{
    double delta_az=0, newRA, newDEC;
    ln_equ_posn EqObjectCoords;

     if (is_connected() == false)
	return;

    AzEncoder->update_client();
    AltEncoder->update_client();
    currentAz   = AzEncoder->get_angle();
    currentAlt  = AltEncoder->get_angle();
    IDSetNumber(&HorizontalCoordsNRP, NULL);

    // Convert to Libnova odd Azimuth convention where Az=0 is due south.
    HorObjectCoords.az = currentAz - 180;
    if (HorObjectCoords.az < 0)
        HorObjectCoords.az += 360;
    HorObjectCoords.alt = currentAlt;

    ln_get_equ_from_hrz (&HorObjectCoords, &observer, ln_get_julian_from_sys(), &EqObjectCoords);

    newRA  = EqObjectCoords.ra/15.0;
    newDEC = EqObjectCoords.dec;

    if (fabs(EquatorialCoordsRNP.np[0].value - newRA)  >= RA_TOLERANCE ||
        fabs(EquatorialCoordsRNP.np[1].value - newDEC) >= DEC_TOLERANCE)
    {
        EquatorialCoordsRNP.np[0].value = newRA;
        EquatorialCoordsRNP.np[1].value = newDEC;
        IDSetNumber(&EquatorialCoordsRNP, NULL);
    }

     delta_az = currentAz - targetAz;
	 
	/***************************************************************
        // Check Status of Horizontal Coord Request
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

                                                // Telescope moving west, but we need to avoid hitting limit switch which is @ 0
                                                if (initialAz < 180 && targetAz > 180)
							update_az_dir(KNRO_EAST);
						else
						// Otherwise proceed normally
							update_az_dir(KNRO_WEST);
					}
					else
					{
						update_az_speed();
                                                // Telescope moving east, but we need to avoid hitting limit switch which is @ 0
                                                if (initialAz > 180 && targetAz < 180)
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
                                                        EquatorialCoordsRNP.s = IPS_OK;
							
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
						EquatorialCoordsRNP.s = IPS_OK;
						IDSetNumber(&HorizontalCoordsNWP, "Slew complete. Engaging tracking...");
						IDSetNumber(&HorizontalCoordsNRP, NULL);
						IDSetNumber(&EquatorialCoordsRNP, NULL);
						
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
  


}
