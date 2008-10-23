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
 
#include <math.h>
#include <libnova/angular_separation.h>
#include <libnova/utility.h>

/*! \fn double ln_get_angular_separation (struct ln_equ_posn* posn1, struct ln_equ_posn* posn2);
* \param posn1 Equatorial position of body 1
* \param posn2 Equatorial position of body 2
* \return Angular separation in degrees
*
* Calculates the angular separation of 2 bodies.
* This method was devised by Mr Thierry Pauwels of the
* Royal Observatory Belgium.
*/	
/* Chap 17 page 115 */
double ln_get_angular_separation (struct ln_equ_posn* posn1, struct ln_equ_posn* posn2)
{
	double d;
	double x,y,z;
	double a1,a2,d1,d2;
	
	/* covert to radians */
	a1 = ln_deg_to_rad(posn1->ra);
	d1 = ln_deg_to_rad(posn1->dec);
	a2 = ln_deg_to_rad(posn2->ra);
	d2 = ln_deg_to_rad(posn2->dec);
	
	x = (cos(d1) * sin (d2)) 
		- (sin(d1) * cos(d2) * cos(a2 - a1));
	y = cos(d2) * sin(a2 - a1);
	z = (sin (d1) * sin (d2)) + (cos(d1) * cos(d2) * cos(a2 - a1));

	x = x * x;
	y = y * y;
	d = atan2(sqrt(x + y), z);
	
	return ln_rad_to_deg(d);
}

/*! \fn double ln_get_rel_posn_angle (struct ln_equ_posn* posn1, struct ln_equ_posn* posn2);
* \param posn1 Equatorial position of body 1
* \param posn2 Equatorial position of body 2
* \return Position angle in degrees
*
* Calculates the position angle of a body with respect to another body.
*/	
/* Chapt 17, page 116 */
double ln_get_rel_posn_angle (struct ln_equ_posn* posn1, struct ln_equ_posn* posn2)
{
	double P;
	double a1,a2,d1,d2;
	double x,y;
	
	/* covert to radians */
	a1 = ln_deg_to_rad(posn1->ra);
	d1 = ln_deg_to_rad(posn1->dec);
	a2 = ln_deg_to_rad(posn2->ra);
	d2 = ln_deg_to_rad(posn2->dec);
	
	y = sin (a1 - a2);
	x = (cos(d2) * tan(d1)) - (sin(d2) * cos(a1 - a2));
	
	P = atan2(y, x);
	return ln_rad_to_deg(P);
}
