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


#ifndef _LN_ABERRATION_H
#define _LN_ABERRATION_H

#include <libnova/ln_types.h>

#ifdef __cplusplus
extern "C" {
#endif

/*! \defgroup aberration Aberration
*
* Aberration: need a description.
*
* All angles are expressed in degrees.
*/

/*! \fn void ln_get_equ_aber (struct ln_equ_posn * mean_position, double JD, struct ln_equ_posn * position);
* \brief Calculate equatorial coordinates with the effects of aberration.
* \ingroup aberration
*/
/* Equ 22.1, 22.3, 22.4 and Ron-Vondrak expression */
void ln_get_equ_aber (struct ln_equ_posn * mean_position, double JD, struct ln_equ_posn * position);

/*! \fn void ln_get_ecl_aber (struct ln_lnlat_posn * mean_position, double JD, struct ln_lnlat_posn * position); 
* \brief Calculate ecliptical coordinates with the effects of aberration.
* \ingroup aberration
*/
/* Equ 22.1, 22.2 pg 139 */
void ln_get_ecl_aber (struct ln_lnlat_posn * mean_position, double JD, struct ln_lnlat_posn * position); 

#ifdef __cplusplus
};
#endif

#endif
