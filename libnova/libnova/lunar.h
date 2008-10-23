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

#ifndef _LN_LUNAR_H
#define _LN_LUNAR_H

#include <libnova/ln_types.h>

#define LN_LUNAR_STANDART_HORIZON		0.125

#ifdef __cplusplus
extern "C" {
#endif

/*! \defgroup lunar Lunar
*
* Functions relating to the Moon.
*
* All angles are expressed in degrees.
*/

/*! \fn double ln_get_lunar_sdiam (double JD)
* \brief Calcaluate the semidiameter of the Moon in arc seconds.
* \ingroup lunar
*/
double ln_get_lunar_sdiam (double JD);

/*! \fn double ln_get_lunar_rst (double JD, struct ln_lnlat_posn * observer, struct ln_rst_time * rst);
* \brief Calculate the time of rise, set and transit for the Moon.
* \ingroup lunar
*/

int ln_get_lunar_rst (double JD, struct ln_lnlat_posn * observer, struct ln_rst_time * rst);

/*! \fn void ln_get_lunar_geo_posn (double JD, struct ln_rect_posn * moon, double precision);
* \brief Calculate the rectangular geocentric lunar cordinates.
* \ingroup lunar
*/ 
/* ELP 2000-82B theory */
void ln_get_lunar_geo_posn (double JD, struct ln_rect_posn * moon, double precision);

/*! \fn void ln_get_lunar_equ_coords_prec (double JD, struct ln_equ_posn * position, double precision);
* \brief Calculate lunar equatorial coordinates.
* \ingroup lunar
*/ 
void ln_get_lunar_equ_coords_prec (double JD, struct ln_equ_posn * position, double precision);

/*! \fn void ln_get_lunar_equ_coords (double JD, struct ln_equ_posn * position);
* \brief Calculate lunar equatorial coordinates.
* \ingroup lunar
*/ 
void ln_get_lunar_equ_coords (double JD, struct ln_equ_posn * position);

/*! \fn void ln_get_lunar_ecl_coords (double JD, struct ln_lnlat_posn * position, double precision);
* \brief Calculate lunar ecliptical coordinates.
* \ingroup lunar
*/ 
void ln_get_lunar_ecl_coords (double JD, struct ln_lnlat_posn * position, double precision);

/*! \fn double ln_get_lunar_phase (double JD);
* \brief Calculate the phase angle of the Moon.
* \ingroup lunar
*/ 
double ln_get_lunar_phase (double JD);

/*! \fn double ln_get_lunar_disk (double JD);
* \brief Calculate the illuminated fraction of the Moons disk
* \ingroup lunar
*/ 
double ln_get_lunar_disk (double JD);
	
/*! \fn double ln_get_lunar_earth_dist (double JD);
* \brief Calculate the distance between the Earth and the Moon.
* \ingroup lunar
*/ 
double ln_get_lunar_earth_dist (double JD);	
	
/*! \fn double ln_get_lunar_bright_limb (double JD);
* \brief Calculate the position angle of the Moon's bright limb.
* \ingroup lunar
*/ 
double ln_get_lunar_bright_limb (double JD);

/*! \fn double ln_get_lunar_long_asc_node (double JD);
* \brief Calculate the longitude of the Moon's mean ascending node.
* \ingroup lunar
*/ 
double ln_get_lunar_long_asc_node (double JD);

/*! \fn double ln_get_lunar_long_perigee (double JD);
* \brief Calculate the longitude of the Moon's mean perigee.
* \ingroup lunar
*/ 
double ln_get_lunar_long_perigee (double JD);

#ifdef __cplusplus
};
#endif

#endif
