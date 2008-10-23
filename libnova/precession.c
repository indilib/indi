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
#include <libnova/precession.h>
#include <libnova/utility.h>

/*
** Precession
*/

/*! \fn void ln_get_equ_prec (struct ln_equ_posn * mean_position, double JD, struct ln_equ_posn * position)
* \param mean_position Mean object position
* \param JD Julian day
* \param position Pointer to store new object position.
*
* Calculate equatorial coordinates with the effects of precession for a given Julian Day. 
* Uses mean equatorial coordinates and is
* only for initial epoch J2000.0 
*/
/* Equ 20.3, 20.4 pg 126 
*/
void ln_get_equ_prec (struct ln_equ_posn * mean_position, double JD, struct ln_equ_posn * position)
{
	long double t, t2, t3, A, B, C, zeta, eta, theta, ra, dec, mean_ra, mean_dec;
	
	/* change original ra and dec to radians */
	mean_ra = ln_deg_to_rad (mean_position->ra);
	mean_dec = ln_deg_to_rad (mean_position->dec);

	/* calc t, zeta, eta and theta for J2000.0 Equ 20.3 */
	t = (JD - JD2000) / 36525.0;
	t *= 1.0 / 3600.0;
	t2 = t * t;
	t3 = t2 *t;
	zeta = 2306.2181 * t + 0.30188 * t2 + 0.017998 * t3;
	eta = 2306.2181 * t + 1.09468 * t2 + 0.041833 * t3;
	theta = 2004.3109 * t - 0.42665 * t2 - 0.041833 * t3;
	zeta = ln_deg_to_rad (zeta);
	eta = ln_deg_to_rad (eta);
	theta = ln_deg_to_rad (theta); 

	/* calc A,B,C equ 20.4 */
	A = cosl (mean_dec) * sinl (mean_ra + zeta);
	B = cosl (theta) * cosl (mean_dec) * cosl (mean_ra + zeta) - sinl (theta) * sinl (mean_dec);
	C = sinl (theta) * cosl (mean_dec) * cosl (mean_ra + zeta) + cosl (theta) * sinl (mean_dec);
	
	ra = atan2l (A,B) + eta;
	
	/* check for object near celestial pole */
	if (mean_dec > (0.4 * M_PI) || mean_dec < (-0.4 * M_PI)) {
		/* close to pole */
		dec = acosl (sqrt(A * A + B * B));
		if (mean_dec < 0.)
		  dec *= -1; /* 0 <= acos() <= PI */
	} else {
		/* not close to pole */
		dec = asinl (C);
	}

	/* change to degrees */
	position->ra = ln_range_degrees (ln_rad_to_deg (ra));
	position->dec = ln_rad_to_deg (dec);
}

/*! \fn void ln_get_equ_prec2 (struct ln_equ_posn * mean_position, double fromJD, double toJD, struct ln_equ_posn * position);
*
* \param mean_position Mean object position
* \param fromJD Julian day (start)
* \param toJD Julian day (end)
* \param position Pointer to store new object position.
*
* Calculate the effects of precession on equatorial coordinates, between arbitary Jxxxx epochs.
* Use fromJD and toJD parameters to specify required Jxxxx epochs.
*/

/* Equ 20.2, 20.4 pg 126 */
void ln_get_equ_prec2 (struct ln_equ_posn * mean_position, double fromJD, double toJD, struct ln_equ_posn * position)
{
	long double t, t2, t3, A, B, C, zeta, eta, theta, ra, dec, mean_ra, mean_dec, T, T2;
	
	/* change original ra and dec to radians */
	mean_ra = ln_deg_to_rad (mean_position->ra);
	mean_dec = ln_deg_to_rad (mean_position->dec);

	/* calc t, T, zeta, eta and theta Equ 20.2 */
	T = ((long double) (fromJD - JD2000)) / 36525.0;
	T *= 1.0 / 3600.0;
	t = ((long double) (toJD - fromJD)) / 36525.0;
	t *= 1.0 / 3600.0;
	T2 = T * T;
	t2 = t * t;
	t3 = t2 *t;
	zeta = (2306.2181 + 1.39656 * T - 0.000139 * T2) * t + (0.30188 - 0.000344 * T) * t2 + 0.017998 * t3;
	eta = (2306.2181 + 1.39656 * T - 0.000139 * T2) * t + (1.09468 + 0.000066 * T) * t2 + 0.018203 * t3;
	theta = (2004.3109 - 0.85330 * T - 0.000217 * T2) * t - (0.42665 + 0.000217 * T) * t2 - 0.041833 * t3;
	zeta = ln_deg_to_rad (zeta);
	eta = ln_deg_to_rad (eta);
	theta = ln_deg_to_rad (theta); 

	/* calc A,B,C equ 20.4 */
	A = cosl (mean_dec) * sinl (mean_ra + zeta);
	B = cosl (theta) * cosl (mean_dec) * cosl (mean_ra + zeta) - sinl (theta) * sinl (mean_dec);
	C = sinl (theta) * cosl (mean_dec) * cosl (mean_ra + zeta) + cosl (theta) * sinl (mean_dec);
	
	ra = atan2l (A,B) + eta;
	
	/* check for object near celestial pole */
	if (mean_dec > (0.4 * M_PI) || mean_dec < (-0.4 * M_PI)) {
		/* close to pole */
		dec = acosl (sqrt(A * A + B * B));
		if (mean_dec < 0.)
		  dec *= -1; /* 0 <= acos() <= PI */
	} else {
		/* not close to pole */
		dec = asinl (C);
	}

	/* change to degrees */
	position->ra = ln_range_degrees (ln_rad_to_deg (ra));
	position->dec = ln_rad_to_deg (dec);
}

/*! \fn void ln_get_ecl_prec (struct ln_lnlat_posn * mean_position, double JD, struct ln_lnlat_posn * position)
* \param mean_position Mean object position
* \param JD Julian day
* \param position Pointer to store new object position.
*
* Calculate ecliptical coordinates with the effects of precession for a given Julian Day. 
* Uses mean ecliptical coordinates and is
* only for initial epoch J2000.0  
* \todo To be implemented. 
*/
/* Equ 20.5, 20.6 pg 128
*/
void ln_get_ecl_prec (struct ln_lnlat_posn * mean_position, double JD, struct ln_lnlat_posn * position)
{

}
