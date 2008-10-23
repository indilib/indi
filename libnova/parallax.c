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
 *  Copyright (C) 2000 - 2005 Petr Kubanek
 */

#include <math.h>
#include <libnova/parallax.h>
#include <libnova/utility.h>
#include <libnova/sidereal_time.h>

/* Equ on page 77 - chapter 10, The Earth's globe
*/
void get_topocentric (struct ln_lnlat_posn * observer, double height, double * ro_sin, double * ro_cos)
{
	double u, lat_rad;
	lat_rad = ln_deg_to_rad (observer->lat);
	u = atan (0.99664719 * tan (lat_rad));
	*ro_sin = 0.99664719 * sin (u) + (height / 6378140) * sin (lat_rad);
	*ro_cos = cos (u) + (height / 6378140) * cos (lat_rad);
	// the quantity ro_sin is positive in the northern hemisphere, negative in the southern one
	if (observer->lat > 0)
	  *ro_sin = fabs (*ro_sin);
	else
	  *ro_sin = fabs (*ro_sin) * -1;
	// ro_cos is always positive
	*ro_cos = fabs (*ro_cos);
}

/*! \fn void ln_get_parallax (struct ln_equ_posn * object, double au_distance, struct ln_lnlat_posn * observer, double height, double JD, struct ln_equ_posn * parallax);
* \param object Object geocentric coordinates
* \param au_distance Distance of object from Earth in AU
* \param observer Geographics observer positions
* \param height Observer height in m
* \param JD  Julian day of observation
* \param parallax RA and DEC parallax
*
* Calculate body parallax, which is need to calculate topocentric position of the body.
*/
/* Equ 39.1, 39.2, 39.3 Pg 263 and 264
*/
void ln_get_parallax
	(struct ln_equ_posn * object,
	 double au_distance,
	 struct ln_lnlat_posn * observer,
	 double height,
	 double JD,
	 struct ln_equ_posn * parallax)
{
  	double H;
	H = ln_get_apparent_sidereal_time (JD) + (observer->lng - object->ra) / 15.0;
	ln_get_parallax_ha (object, au_distance, observer, height, H, parallax);
}


/*! \fn void ln_get_parallax_ha (struct ln_equ_posn * object, double au_distance, struct ln_lnlat_posn * observer, double height, double H, struct ln_equ_posn * parallax);
* \param object Object geocentric coordinates
* \param au_distance Distance of object from Earth in AU
* \param observer Geographics observer positions
* \param height Observer height in m
* \param H Hour angle of object in hours
* \param parallax RA and DEC parallax
*
* Calculate body parallax, which is need to calculate topocentric position of the body.
* Uses hour angle as time reference (handy in case we already compute it).
*/
/* Equ 39.1, 39.2, 39.3 Pg 263 and 264
*/
void ln_get_parallax_ha
	(struct ln_equ_posn * object,
	 double au_distance,
	 struct ln_lnlat_posn * observer,
	 double height,
	 double H,
	 struct ln_equ_posn * parallax)
{
	double sin_pi, ro_sin, ro_cos, sin_H, cos_H, dec_rad, cos_dec;
	get_topocentric (observer, height, &ro_sin, &ro_cos);
	sin_pi = sin (ln_deg_to_rad ((8.794 / au_distance) / 3600.0));  // (39.1)

	/* change hour angle from hours to radians*/
	H *= M_PI / 12.0;

	sin_H = sin (H);
	cos_H = cos (H);

	dec_rad = ln_deg_to_rad (object->dec);
	cos_dec = cos (dec_rad);
	
	parallax->ra = atan2 (-ro_cos * sin_pi * sin_H, cos_dec  - ro_cos * sin_pi * cos_H); // (39.2)
	parallax->dec = atan2 ((sin (dec_rad) - ro_sin * sin_pi) * cos (parallax->ra), cos_dec - ro_cos * sin_pi * cos_H); // (39.3)

	parallax->ra = ln_rad_to_deg (parallax->ra);
	parallax->dec = ln_rad_to_deg (parallax->dec) - object->dec;
}
