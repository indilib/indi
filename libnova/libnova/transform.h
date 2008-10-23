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

#ifndef _LN_TRANSFORM_H
#define _LN_TRANSFORM_H

#include <libnova/ln_types.h>

#ifdef __cplusplus
extern "C" {
#endif

/*! \defgroup transform Transformation of Coordinates
*
* Transformations from one coordinate system to another.
*/

/*! \fn void ln_get_hrz_from_equ (struct ln_equ_posn * object, struct ln_lnlat_posn * observer, double JD, struct ln_hrz_posn *position);
* \brief Calculate horizontal coordinates from equatorial coordinates 
* \ingroup transform 
*/
/* Use get_mean_sidereal_time, get_hrz_from_equ_siderealtime */
void ln_get_hrz_from_equ (struct ln_equ_posn * object, struct ln_lnlat_posn * observer, double JD, struct ln_hrz_posn *position);

/*! \fn void ln_get_hrz_from_equ_sidereal_time (struct ln_equ_posn * object, struct ln_lnlat_posn * observer, double sidereal_time, struct ln_hrz_posn *position);
* \brief Calculate horizontal coordinates from equatorial coordinates,
* using mean sidereal time.
* \ingroup transform 
*/
/* Equ 12.5,12.6 pg 88 */
void ln_get_hrz_from_equ_sidereal_time (struct ln_equ_posn * object, struct ln_lnlat_posn * observer, double sidereal, struct ln_hrz_posn *position);

/*! \fn void ln_get_equ_from_ecl (struct ln_lnlat_posn * object, double JD, struct ln_equ_posn * position);
* \brief Calculate equatorial coordinates from ecliptical coordinates
* \ingroup transform
*/
/* Equ 12.3, 12.4 pg 89 */
void ln_get_equ_from_ecl (struct ln_lnlat_posn * object, double JD, struct ln_equ_posn * position);

/*! \fn void ln_get_ecl_from_equ (struct ln_equ_posn * object, double JD, struct ln_lnlat_posn * position);
* \brief Calculate ecliptical cordinates from equatorial coordinates 
* \ingroup transform
*/
/* Equ 12.1, 12.2 Pg 88 */
void ln_get_ecl_from_equ (struct ln_equ_posn * object, double JD, struct ln_lnlat_posn * position);

/*! \fn void ln_get_equ_from_hrz (struct ln_hrz_posn *object, struct ln_lnlat_posn * observer, double JD, struct ln_equ_posn * position); 
* \brief Calculate equatorial coordinates from horizontal coordinates  
* \ingroup transform
*/
/* Pg 89 */
void ln_get_equ_from_hrz (struct ln_hrz_posn *object, struct ln_lnlat_posn * observer, double JD, struct ln_equ_posn * position); 

/*! \fn void ln_get_rect_from_helio (struct ln_helio_posn *object, struct ln_rect_posn * position); 
* \brief Calculate geocentric coordinates from heliocentric coordinates  
* \ingroup transform
*/
/* Pg ?? */
void ln_get_rect_from_helio (struct ln_helio_posn *object, struct ln_rect_posn * position); 

/*! \fn void ln_get_ecl_from_rect (struct ln_rect_posn * rect, struct ln_lnlat_posn * posn)
* \ingroup transform
* \brief Transform an objects rectangular coordinates into ecliptical coordinates.
*/
/* Equ 33.2
*/
void ln_get_ecl_from_rect (struct ln_rect_posn * rect, struct ln_lnlat_posn * posn);

/*! \fn void ln_get_equ_from_gal (struct ln_gal_posn *gal, struct ln_equ_posn *equ)
* \ingroup transform
* \brief Transform an object galactic coordinates into equatorial coordinates.
*/
/* Pg 94 */
void ln_get_equ_from_gal (struct ln_gal_posn *gal, struct ln_equ_posn *equ);

/*! \fn void ln_get_equ2000_from_gal (struct ln_gal_posn *gal, struct ln_equ_posn *equ)
* \ingroup transform
* \brief Transform an object galactic coordinate into J2000 equatorial coordinates.
*/
void ln_get_equ2000_from_gal (struct ln_gal_posn *gal, struct ln_equ_posn *equ);

/*! \fn void ln_get_gal_from_equ (struct ln_equ_posn *equ, struct ln_gal_posn *gal)
* \ingroup transform
* \brief Transform an object equatorial coordinates into galactic coordinates.
*/
/* Pg 94 */
void ln_get_gal_from_equ (struct ln_equ_posn *equ, struct ln_gal_posn *gal);

/*! \fn void ln_get_gal_from_equ2000 (struct ln_equ_posn *equ, struct ln_gal_posn *gal)
* \ingroup transform
* \brief Transform an object J2000 equatorial coordinates into galactic coordinates.
*/
void ln_get_gal_from_equ2000 (struct ln_equ_posn *equ, struct ln_gal_posn *gal);

#ifdef __cplusplus
};
#endif

#endif
