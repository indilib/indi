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

#ifndef _LN_URANUS_H
#define _LN_URANUS_H

#include <libnova/ln_types.h>

#ifdef __cplusplus
extern "C" {
#endif


/*! \defgroup uranus Uranus
*
* Functions relating to the planet Uranus.
*
* All angles are expressed in degrees.
*/

/*! \fn double ln_get_uranus_sdiam (double JD)
* \brief Calcaluate the semidiameter of Uranus in arc seconds.
* \ingroup uranus
*/
double ln_get_uranus_sdiam (double JD);

/*! \fn double ln_get_uranus_rst (double JD, struct ln_lnlat_posn * observer, struct ln_rst_time * rst);
* \brief Calculate the time of rise, set and transit for Uranus.
* \ingroup uranus
*/
int ln_get_uranus_rst (double JD, struct ln_lnlat_posn * observer, struct ln_rst_time * rst);

/*! \fn void ln_get_uranus_helio_coords (double JD, struct ln_helio_posn * position);
* \brief Calculate Uranus heliocentric coordinates
* \ingroup uranus
*/ 
/* Chapter 31 Pg 206-207 Equ 31.1 31.2 , 31.3 using VSOP 87 */
void ln_get_uranus_helio_coords (double JD, struct ln_helio_posn * position);

/*! \fn void ln_get_uranus_equ_coords (double JD, struct ln_equ_posn * position);
* \brief Calculate Uranus equatorial coordinates.
* \ingroup uranus
*/ 
/* Chapter 31 Pg 206-207 Equ 31.1 31.2 , 31.3 using VSOP 87 */
void ln_get_uranus_equ_coords (double JD, struct ln_equ_posn * position);

/*! \fn double ln_get_uranus_earth_dist (double JD);
* \brief Calculate the distance between Uranus and the Earth.
* \ingroup uranus
* \return Distance in AU
*/ 
/* Chapter ?? */
double ln_get_uranus_earth_dist (double JD);
	
/*! \fn double ln_get_uranus_solar_dist (double JD);
* \brief Calculate the distance between Uranus and the Sun.
* \ingroup uranus
* \return Distance in AU
*/ 
/* Chapter ?? */
double ln_get_uranus_solar_dist (double JD);
	
/*! \fn double ln_get_uranus_magnitude (double JD);
* \brief Calculate the visible magnitude of Uranus
* \ingroup uranus
* \return Visible magnitude of Uranus
*/ 
/* Chapter ?? */
double ln_get_uranus_magnitude (double JD);

/*! \fn double ln_get_uranus_disk (double JD);
* \brief Calculate the illuminated fraction of Uranus's disk
* \ingroup uranus
* \return Illuminated fraction of Uranus disk
*/ 
/* Chapter 41 */
double ln_get_uranus_disk (double JD);

/*! \fn double ln_get_uranus_phase (double JD);
* \brief Calculate the phase angle of Uranus.
* \ingroup uranus
* \return Phase angle of Uranus (degrees)
*/ 
/* Chapter 41 */
double ln_get_uranus_phase (double JD);

/*! \fn void ln_get_uranus_rect_helio (double JD, struct ln_rect_posn * position)
* \ingroup uranus
* \brief Calculate Uranus rectangular heliocentric coordinates.
*/
void ln_get_uranus_rect_helio (double JD, struct ln_rect_posn * position);

#ifdef __cplusplus
};
#endif

#endif
