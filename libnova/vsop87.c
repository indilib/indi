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
 *  Some functions in this file use the VSOP87 solution by
 *  Messrs. Bretagnon and Francou.
 * 
 *  Copyright (C) 2000 - 2005 Liam Girdwood  
 */


#include <math.h>
#include <libnova/vsop87.h>
#include <libnova/utility.h>

double ln_calc_series (const struct ln_vsop * data, int terms, double t)
{
	double value = 0;
	int i;
	
	for (i=0; i<terms; i++) {
		value += data->A * cos(data->B + data->C * t);
		data++;
	}
	
	return value;
}


/*! \fn void ln_vsop87_to_fk5 (struct ln_helio_posn * position, double JD)
* \param position Position to transform. 
* \param JD Julian day
*
* Transform from VSOP87 to FK5 reference frame. 
*/
/* Equation 31.3 Pg 207.         
*/
void ln_vsop87_to_fk5 (struct ln_helio_posn * position, double JD)
{
	double LL, T, delta_L, delta_B, B;
	
	/* get julian centuries from 2000 */
	T = (JD - 2451545.0)/ 36525.0;
	
	LL = position->L - 1.397 * T - 0.00031 * T * T;
	LL = ln_deg_to_rad (LL);
	B = ln_deg_to_rad(position->B);
	
	delta_L = (-0.09033 / 3600.0) + (0.03916 / 3600.0) * (cos (LL) + sin (LL)) * tan (B);
	delta_B = (0.03916 / 3600.0) * (cos(LL) - sin(LL));
	
	position->L += delta_L;
	position->B += delta_B;
}
