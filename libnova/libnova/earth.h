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

#ifndef _LN_EARTH_H
#define _LN_EARTH_H

#include <libnova/ln_types.h>

#ifdef __cplusplus
extern "C" {
#endif


/*! \defgroup earth Earth
*
* Functions relating to the planet Earth.
*
* All angles are expressed in degrees.
*/

/*
** Earth
*/

/*! \fn void ln_get_earth_helio_coords (double JD, struct ln_helio_posn * position);
* \brief Calculate Earth's heliocentric coordinates
* \ingroup earth
*/ 
/* Chapter 31 Pg 206-207 Equ 31.1 31.2 , 31.3 using VSOP 87 */
void ln_get_earth_helio_coords (double JD, struct ln_helio_posn * position);

/*! \fn void ln_get_earth_solar_dist (double JD);
* \brief Calculate the distance between Earth and the Sun.
* \ingroup earth
* \return Distance in AU
*/ 
/* Chapter ?? */
double ln_get_earth_solar_dist (double JD);
	
/*! \fn void ln_get_earth_rect_helio (double JD, struct ln_rect_posn * position)
* \ingroup earth
* \brief Calculate the Earths rectangular heliocentric coordinates.
*/
void ln_get_earth_rect_helio (double JD, struct ln_rect_posn * position);

/*! \fn void ln_get_earth_centre_dist (float height, double latitude, double * p_sin_o, double * p_cos_o);
* \ingroup earth
* \brief Calculate Earth globe centre distance.
*/
void ln_get_earth_centre_dist (float height, double latitude, double * p_sin_o, double * p_cos_o);

#ifdef __cplusplus
};
#endif

#endif
