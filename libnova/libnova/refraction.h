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

#ifndef _LN_REFRACTION_H
#define _LN_REFRACTION_H

#include <libnova/ln_types.h>

#ifdef __cplusplus
extern "C" {
#endif

/*! \defgroup refraction Atmospheric Refraction
*
* Functions relating to Atmospheric Refraction
*
* All angles are expressed in degrees.
*/

/*! \fn double ln_get_refraction_adj (double altitude, double atm_pres, double temp)
* \brief Calculate the adjustment in altitude of a body due to atmospheric 
* refraction.
* \ingroup refraction
*/
double ln_get_refraction_adj (double altitude, double atm_pres, double temp);
	
#ifdef __cplusplus
};
#endif

#endif
