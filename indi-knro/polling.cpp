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
    double delta_az=0, delta_alt=0, newRA=0, newDEC=0, objAz=0, objAlt=0;
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


    if (fabs(EquatorialCoordsNP.np[0].value - newRA)  >= RA_TOLERANCE ||
        fabs(EquatorialCoordsNP.np[1].value - newDEC) >= DEC_TOLERANCE)
    {
        EquatorialCoordsNP.np[0].value = newRA;
        EquatorialCoordsNP.np[1].value = newDEC;
        IDSetNumber(&EquatorialCoordsNP, NULL);
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
            if (slew_stage != SLEW_TRACK)
                break;




            // Convert to Horizontal coords
            ln_get_hrz_from_equ(&EqTrackObjCoords, &observer, ln_get_julian_from_sys(), &HorObjectCoords);

            HorObjectCoords.az += 180;
            if (HorObjectCoords.az > 360)
                HorObjectCoords.az -= 360;

            objAz  = HorObjectCoords.az;
            objAlt = HorObjectCoords.alt;

            /*
                IDLog("EqTrackObj RA: %g - DEC: %g - its Horz Az: %g - Alt: %g\n, CurrentAz: %g - CurrentAlt: %g - fabs_az: %g, fabs_alt: %g\n",
                  EqTrackObjCoords.ra, EqTrackObjCoords.dec, HorObjectCoords.az, HorObjectCoords.alt, currentAz, currentAlt,
                  fabs(objAz - currentAz)*60, fabs(objAlt - currentAlt)*60);
             */

            // Now let's compare it to current encoder values
            if (fabs (objAz - currentAz) * 60.0 > trackAZTolerance || fabs (objAlt - currentAlt) * 60.0 > trackALTTolerance)
            {
                targetAz  = objAz;
                targetAlt = objAlt;

                execute_slew(true);

            }

            break;

		case IPS_BUSY:
		      
			switch (slew_stage)
			{
				case SLEW_NONE:
				break;

				case SLEW_NOW:
                case SLEW_TRACK:
				  
				    
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
                            EquatorialCoordsNP.s = IPS_OK;
							
							slew_stage = SLEW_NONE;
							slew_busy.stop();
							slew_complete.play();


							IDSetSwitch(&ParkSP, "Telescope park complete.");
							IDSetNumber(&HorizontalCoordsNWP, NULL);
							IDSetNumber(&HorizontalCoordsNRP, NULL);
                            IDSetNumber(&EquatorialCoordsNP, NULL);
						
							return;
						}

						// FIXME Make sure to check whether on_set_coords is set to SLEW or TRACK
						// Because when slewing, we're done here, but if there is tracking, then
						// We go to that mode
						slew_busy.stop();
						slew_complete.play();

						HorizontalCoordsNWP.s = IPS_OK;
						HorizontalCoordsNRP.s = IPS_OK;
                        EquatorialCoordsNP.s = IPS_OK;

                        if (slew_stage != SLEW_TRACK)
                        {
                            IDSetNumber(&HorizontalCoordsNWP, "Slew complete. Engaging tracking...");
                            slew_stage = SLEW_TRACK;
                            AzInverter->set_verbose(false);
                            AltInverter->set_verbose(false);
                        }

						IDSetNumber(&HorizontalCoordsNRP, NULL);
                        IDSetNumber(&EquatorialCoordsNP, NULL);
						
					}
					break;

					
			}
			break;

		case IPS_ALERT:
			break;
	}

  return;

}
