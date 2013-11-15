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
bool knroObservatory::ReadScopeStatus()
{
    double delta_az=0, newRA=0, newDEC=0;

    if (isConnected() == false)
        return false;


    // Convert to Libnova odd Azimuth convention where Az=0 is due south.
    currentHorCoords.az = AzEncoder->get_angle() - 180;
    if (currentHorCoords.az < 0)
        currentHorCoords.az += 360;
    currentHorCoords.alt = AltEncoder->get_angle();

    // Get RA/DEC from current horizontal coords
    ln_get_equ_from_hrz (&currentHorCoords, &observer, ln_get_julian_from_sys(), &currentEQCoords);

    // Use our convention again
    currentHorCoords.az = AzEncoder->get_angle();

    // Update targetAz, targetDEC from targetRA, targetDEC
    ln_get_hrz_from_equ(&targetEQCoords, &observer, ln_get_julian_from_sys(), &targetHorCoords);

    newRA  = currentEQCoords.ra/15.0;
    newDEC = currentEQCoords.dec;

    targetHorCoords.az += 180;
    if (targetHorCoords.az > 360)
        targetHorCoords.az -= 360;

    delta_az = currentHorCoords.az - targetHorCoords.az;

    DEBUGF(INDI::Logger::DBG_DEBUG, "Current Alt %g - Current Az %g - target Alt %g - target Az %g", currentHorCoords.alt, currentHorCoords.az, targetHorCoords.alt, targetHorCoords.az);
	 
	/***************************************************************
        // Check Status of Horizontal Coord Request
	****************************************************************/
    switch(TrackState)
	{
        case SCOPE_IDLE:
        case SCOPE_PARKED:
			break;

        case SCOPE_TRACKING:
        if (fabs(currentHorCoords.az - targetHorCoords.az) > AZ_TRACKING_THRESHOLD || fabs(currentHorCoords.alt - targetHorCoords.alt) > ALT_TRACKING_THRESHOLD)
            TrackState = SCOPE_SLEWING;
            break;

        case SCOPE_SLEWING:
        case SCOPE_PARKING:
					// If declination is done, stop n/s
					if (is_alt_done())
						stop_alt();
                    else if (currentHorCoords.alt - targetHorCoords.alt > 0)
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
                        if (initialAz < 180 && targetHorCoords.az > 180)
                             update_az_dir(KNRO_EAST);
						else
						// Otherwise proceed normally
                             update_az_dir(KNRO_WEST);
					}
					else
					{
						update_az_speed();
                        // Telescope moving east, but we need to avoid hitting limit switch which is @ 0
                        if (initialAz > 180 && targetHorCoords.az < 180)
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
						
                        if (TrackState == SCOPE_PARKING)
						{
                            IUResetSwitch(&ParkSP);
							ParkSP.s = IPS_OK;
                            HorizontalCoordsNP.s = IPS_OK;
                            TrackState = SCOPE_PARKED;

							slew_busy.stop();
							slew_complete.play();


                            DEBUG(INDI::Logger::DBG_SESSION, "Telescope park complete.");
                            IDSetSwitch(&ParkSP, NULL);

						}
                        else
                        {
                            // FIXME Make sure to check whether on_set_coords is set to SLEW or TRACK
                            // Because when slewing, we're done here, but if there is tracking, then
                            // We go to that mode
                            slew_busy.stop();
                            slew_complete.play();

                            HorizontalCoordsNP.s = IPS_OK;

                            DEBUG(INDI::Logger::DBG_SESSION, "Slew complete. Engaging tracking...");
                            TrackState = SCOPE_TRACKING;

                            AzInverter->set_verbose(false);
                            AltInverter->set_verbose(false);
                        }

					}
            break;

					
     }

    AzEncoder->update_client();
    AltEncoder->update_client();
    HorizontalCoordsN[0].value = currentHorCoords.az;
    HorizontalCoordsN[1].value = currentHorCoords.alt;
    IDSetNumber(&HorizontalCoordsNP, NULL);

    NewRaDec(newRA, newDEC);

    return true;

}
