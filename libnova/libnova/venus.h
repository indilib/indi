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

#ifndef _LN_VENUS_H
#define _LN_VENUS_H

#include <libnova/ln_types.h>

#ifdef __cplusplus
extern "C" {
#endif

/*! \defgroup venus Venus
*
* Functions relating to the planet Venus.
*
* All angles are expressed in degrees.
*/

/*! \fn double ln_get_venus_sdiam (double JD)
* \brief Calcaluate the semidiameter of Venus in arc seconds.
* \ingroup venus
*/
double ln_get_venus_sdiam (double JD);

/*! \fn double ln_get_venus_rst (double JD, struct ln_lnlat_posn * observer, struct ln_rst_time * rst);
* \brief Calculate the time of rise, set and transit for Venus.
* \ingroup venus
*/
int ln_get_venus_rst (double JD, struct ln_lnlat_posn * observer, struct ln_rst_time * rst);

/*! \fn void ln_get_venus_helio_coords (double JD, struct ln_helio_posn * position);
* \brief Calvulate Venus heliocentric coordinates 
* \ingroup venus
*/
/* Chapter 31 Pg 206-207 Equ 31.1 31.2 , 31.3 using VSOP 87 */
void ln_get_venus_helio_coords (double JD, struct ln_helio_posn * position);

/*! \fn void ln_get_venus_equ_coords (double JD, struct ln_equ_posn * position);
* \brief Calculate Venus equatorial coordinates
* \ingroup venus
*/ 
/* Chapter 31 Pg 206-207 Equ 31.1 31.2 , 31.3 using VSOP 87 */
void ln_get_venus_equ_coords (double JD, struct ln_equ_posn * position);

/*! \fn double ln_get_venus_earth_dist (double JD);
* \brief Calculate the distance between Venus and the Earth.
* \ingroup venus
* \return Distance in AU
*/ 
/* Chapter ?? */
double ln_get_venus_earth_dist (double JD);
	
/*! \fn double ln_get_venus_solar_dist (double JD);
* \brief Calculate the distance between Venus and the Sun.
* \ingroup venus
* \return Distance in AU
*/ 
/* Chapter ?? */
double ln_get_venus_solar_dist (double JD);
	
/*! \fn double ln_get_venus_magnitude (double JD);
* \brief Calculate the visible magnitude of Venus
* \ingroup venus
* \return Visible magnitude of Venus
*/ 
/* Chapter ?? */
double ln_get_venus_magnitude (double JD);

/*! \fn double ln_get_venus_disk (double JD);
* \brief Calculate the illuminated fraction of Venus disk
* \ingroup venus
* \return Illuminated fraction of Venus disk
*/ 
/* Chapter 41 */
double ln_get_venus_disk (double JD);

/*! \fn double ln_get_venus_phase (double JD);
* \brief Calculate the phase angle of Venus.
* \ingroup venus
* \return Phase angle of Venus (degrees)
*/ 
/* Chapter 41 */
double ln_get_venus_phase (double JD);

/*! \fn void ln_get_venus_rect_helio (double JD, struct ln_rect_posn * position)
* \ingroup venus
* \brief Calculate Venus rectangular heliocentric coordinates.
*/
void ln_get_venus_rect_helio (double JD, struct ln_rect_posn * position);
	
#ifdef __cplusplus
};
#endif

#endif
