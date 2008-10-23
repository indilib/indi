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

#ifndef _LN_PROPER_MOTION_H
#define _LN_PROPER_MOTION_H

#include <libnova/ln_types.h>

#ifdef __cplusplus
extern "C" {
#endif

/*! \defgroup motion Proper Motion.
* Proper motion is the motion in space of a star between 2 epochs. It has components
* in right ascension and in declination.
*
* All angles are expressed in degrees.
*/

/*! \fn void ln_get_equ_pm (struct ln_equ_posn * mean_position, struct ln_equ_posn * proper_motion, double JD, struct ln_equ_posn * position);
* \brief Calculate a stars equatorial position wrt proper motion (J2000).
* \ingroup motion
*/
/* Equ 20.2, 20.3, 20.4 pg 126 */
void ln_get_equ_pm (struct ln_equ_posn * mean_position, struct ln_equ_posn * proper_motion, double JD, struct ln_equ_posn * position);


/*! \fn void ln_get_equ_pm_epoch (struct ln_equ_posn * mean_position, struct ln_equ_posn * proper_motion, double JD, double epoch_JD, struct ln_equ_posn * position)
* \brief Calculate a stars equatorial position wrt proper motion and epoch.
*/
/* Equ 20.2, 20.3, 20.4 pg 126 
*/
void ln_get_equ_pm_epoch (struct ln_equ_posn * mean_position, struct ln_equ_posn * proper_motion, double JD, double epoch_JD, struct ln_equ_posn * position);
#ifdef __cplusplus
};
#endif

#endif
