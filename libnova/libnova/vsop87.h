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

#ifndef _LN_VSOP87_H
#define _LN_VSOP87_H

#include <libnova/ln_types.h>

#ifdef __cplusplus
extern "C" {
#endif

/*! \defgroup VSOP87 VSOP87 Theory
*
* Thanks to Messrs. Bretagnon and Francou for publishing planetary 
* solution VSOP87.
*/

/*! \fn void ln_vsop87_to_fk5 (struct ln_helio_posn * position, double JD);
* \ingroup VSOP87
* \brief Transform from VSOP87 to FK5 reference system. 
*/
/* equation 31.3 Pg 207         */
/* JD Julian Day */
void ln_vsop87_to_fk5 (struct ln_helio_posn * position, double JD);


struct ln_vsop
{
	double A;
	double B;
	double C;
};


double ln_calc_series (const struct ln_vsop * data, int terms, double t);

#ifdef __cplusplus
};
#endif

#endif
