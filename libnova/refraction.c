/*
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *  
 *  Copyright (C) 2000 - 2005 Liam Girdwood  
 */

#include <math.h>
#include <libnova/refraction.h>
#include <libnova/utility.h>

/*! \fn double ln_get_refraction_adj (double altitude, double atm_pres, double temp)
* \param altitude The altitude of the object above the horizon in degrees
* \param atm_pres Atmospheric pressure in milibars
* \param temp Temperature in degrees C.
* \return Adjustment in objects altitude in degrees.
*
* Calculate the adjustment in altitude of a body due to atmosphric 
* refraction. This value varies over altitude, pressure and temperature.
* 
* Note: Default values for pressure and teperature are 1010 mBar and 10C 
* respectively.
*/
double ln_get_refraction_adj (double altitude, double atm_pres, double temp)
{
	long double R;

	/* equ 16.3 */
	R = 1.0 / tan (ln_deg_to_rad (altitude + (7.31 / (altitude + 4.4))));
	R -= 0.06 * sin (ln_deg_to_rad (14.7 * (R / 60.0) + 13.0));

	/* take into account of atm press and temp */
	R *= ((atm_pres / 1010) * (283 / (273 + temp)));
	
	/* convert from arcminutes to degrees */
	R /= 60.0;
	
	return R;
}
