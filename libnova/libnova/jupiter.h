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

#ifndef _LN_JUPITER_H
#define _LN_JUPITER_H

#include <libnova/ln_types.h>

#ifdef __cplusplus
extern "C" {
#endif


/*! \defgroup jupiter Jupiter
*
* Functions relating to the planet Jupiter.
*
* All angles are expressed in degrees.
*/

/*! \fn double ln_get_jupiter_equ_sdiam (double JD)
* \brief Calcaluate the eqatorial semidiameter of Jupiter in arc seconds.
* \ingroup jupiter
*/
double ln_get_jupiter_equ_sdiam (double JD);

/*! \fn double ln_get_jupiter_pol_sdiam (double JD)
* \brief Calcaluate the polar semidiameter of Jupiter in arc seconds.
* \ingroup jupiter
*/
double ln_get_jupiter_pol_sdiam (double JD);

/*! \fn double ln_get_jupiter_rst (double JD, struct ln_lnlat_posn * observer, struct ln_rst_time * rst);
* \brief Calculate the time of rise, set and transit for Jupiter.
* \ingroup jupiter
*/
int ln_get_jupiter_rst (double JD, struct ln_lnlat_posn * observer, struct ln_rst_time * rst);

/*! \fn void ln_get_jupiter_helio_coords (double JD, struct ln_helio_posn * position);
* \brief Calculate Jupiter's heliocentric coordinates
* \ingroup jupiter
*/ 
/* Chapter 31 Pg 206-207 Equ 31.1 31.2 , 31.3 using VSOP 87 */
void ln_get_jupiter_helio_coords (double JD, struct ln_helio_posn * position);

/*! \fn void ln_get_jupiter_equ_coords (double JD, struct ln_equ_posn * position);
* \brief Calculate Jupiter's equatorial coordinates.
* \ingroup jupiter
*/ 
/* Chapter 31 Pg 206-207 Equ 31.1 31.2 , 31.3 using VSOP 87 */
void ln_get_jupiter_equ_coords (double JD, struct ln_equ_posn * position);

/*! \fn double ln_get_jupiter_earth_dist (double JD);
* \brief Calculate the distance between Jupiter and the Earth.
* \ingroup jupiter
* \return Distance in AU
*/ 
/* Chapter ?? */
double ln_get_jupiter_earth_dist (double JD);
	
/*! \fn double ln_get_jupiter_solar_dist (double JD);
* \brief Calculate the distance between Jupiter and the Sun.
* \ingroup jupiter
* \return Distance in AU
*/ 
/* Chapter ?? */
double ln_get_jupiter_solar_dist (double JD);
	
/*! \fn double ln_get_jupiter_magnitude (double JD);
* \brief Calculate the visible magnitude of Jupiter
* \ingroup jupiter
* \return Visible magnitude of Jupiter
*/ 
/* Chapter ?? */
double ln_get_jupiter_magnitude (double JD);

/*! \fn double ln_get_jupiter_disk (double JD);
* \brief Calculate the illuminated fraction of Jupiter's disk
* \ingroup jupiter
* \return Illuminated fraction of Jupiter's disk
*/ 
/* Chapter 41 */
double ln_get_jupiter_disk (double JD);

/*! \fn double ln_get_jupiter_phase (double JD);
* \brief Calculate the phase angle of Jupiter.
* \ingroup jupiter
* \return Phase angle of Jupiter (degrees)
*/ 
/* Chapter 41 */
double ln_get_jupiter_phase (double JD);

/*! \fn void ln_get_jupiter_rect_helio (double JD, struct ln_rect_posn * position)
* \ingroup jupiter
* \brief Calculate Jupiters rectangular heliocentric coordinates.
*/
void ln_get_jupiter_rect_helio (double JD, struct ln_rect_posn * position);
	
#ifdef __cplusplus
};
#endif

#endif
