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
#include <libnova/transform.h>
#include <libnova/utility.h>
#include <libnova/sidereal_time.h>
#include <libnova/nutation.h>
#include <libnova/precession.h>

/*! \fn void ln_get_rect_from_helio (struct ln_helio_posn *object, struct ln_rect_posn * position); 
* \param object Object heliocentric coordinates
* \param position Pointer to store new position
*
* Transform an objects heliocentric ecliptical coordinates
* into heliocentric rectangular coordinates.
*/
/* Equ 37.1 Pg 264
*/
void ln_get_rect_from_helio 
	(struct ln_helio_posn *object,  
	struct ln_rect_posn * position)
{
	double sin_e, cos_e;
	double cos_B, sin_B, sin_L, cos_L;
	
	/* ecliptic J2000 */
	sin_e = 0.397777156;
	cos_e = 0.917482062;

	/* calc common values */
	cos_B = cos(ln_deg_to_rad(object->B));
	cos_L = cos(ln_deg_to_rad(object->L));
	sin_B = sin(ln_deg_to_rad(object->B));
	sin_L = sin(ln_deg_to_rad(object->L));
	
	/* equ 37.1 */
	position->X = object->R * cos_L * cos_B;
	position->Y = object->R * (sin_L * cos_B * cos_e - sin_B * sin_e);
	position->Z = object->R * (sin_L * cos_B * sin_e + sin_B * cos_e);
}

/*! \fn void ln_get_hrz_from_equ (struct ln_equ_posn * object, struct ln_lnlat_posn * observer, double JD, struct ln_hrz_posn * position)
* \param object Object coordinates.
* \param observer Observer cordinates.
* \param JD Julian day
* \param position Pointer to store new position.
*
* Transform an objects equatorial coordinates into horizontal coordinates
* for the given julian day and observers position.
* 
* 0 deg azimuth = south, 90 deg = west.
*/
/* Equ 12.1,12.2 pg 88 
*
* TODO:
* Transform horizontal coordinates to galactic coordinates.
*/

void ln_get_hrz_from_equ (struct ln_equ_posn * object, struct ln_lnlat_posn * observer, double JD, struct ln_hrz_posn * position)
{
	double sidereal;

	/* get mean sidereal time in hours*/
	sidereal = ln_get_mean_sidereal_time (JD);
	ln_get_hrz_from_equ_sidereal_time (object, observer, sidereal, position);
}


void ln_get_hrz_from_equ_sidereal_time (struct ln_equ_posn * object, struct ln_lnlat_posn * observer, double sidereal, struct ln_hrz_posn * position)
{
	long double H, ra, latitude, declination, A, Ac, As, h, Z, Zs;

	/* change sidereal_time from hours to radians*/
	sidereal *= 2.0 * M_PI / 24.0;

	/* calculate hour angle of object at observers position */
	ra = ln_deg_to_rad (object->ra);
	H = sidereal + ln_deg_to_rad (observer->lng) - ra;

	/* hence formula 12.5 and 12.6 give */
	/* convert to radians - hour angle, observers latitude, object declination */
	latitude = ln_deg_to_rad (observer->lat);
	declination = ln_deg_to_rad (object->dec);

	/* formula 12.6 *; missuse of A (you have been warned) */
	A = sin (latitude) * sin (declination) + cos (latitude) * cos (declination) * cos (H);
	h = asin (A);

	/* covert back to degrees */
	position->alt = ln_rad_to_deg (h);   

	/* zenith distance, Telescope Control 6.8a */
	Z = acos (A);

	/* is'n there better way to compute that? */
	Zs = sin (Z);

	/* sane check for zenith distance; don't try to divide by 0 */

	if (fabs(Zs) < 1e-5) {
		if (object->dec > 0)
			position->az = 180;
		else
			position->az = 0;
		if ((object->dec > 0 && observer->lat > 0)
		   || (object->dec < 0 && observer->lat < 0))
		  	position->alt = 90;
		else
		  	position->alt = -90;
		return;
	}

	/* formulas TC 6.8d Taff 1991, pp. 2 and 13 - vector transformations */
	As = (cos (declination) * sin (H)) / Zs;
	Ac = (sin (latitude) * cos (declination) * cos (H) - cos (latitude) * sin (declination)) / Zs;

	// don't blom at atan2
	if (Ac == 0 && As == 0) {
	        if (object->dec > 0)
			position->az = 180.0;
		else
			position->az = 0.0;
		return;
	}
	A = atan2 (As, Ac);

	/* covert back to degrees */
	position->az = ln_range_degrees(ln_rad_to_deg (A));
}

/*! \fn void ln_get_equ_from_hrz (struct ln_hrz_posn * object, struct ln_lnlat_posn * observer, double JD, struct ln_equ_posn * position)
* \param object Object coordinates.
* \param observer Observer cordinates.
* \param JD Julian day
* \param position Pointer to store new position.
*
* Transform an objects horizontal coordinates into equatorial coordinates
* for the given julian day and observers position.
*/
void ln_get_equ_from_hrz (struct ln_hrz_posn * object, struct ln_lnlat_posn * observer, double JD, struct ln_equ_posn * position)
{
	long double H, longitude, declination, latitude, A, h, sidereal;

	/* change observer/object position into radians */

	/* object alt/az */
	A = ln_deg_to_rad (object->az);
	h = ln_deg_to_rad (object->alt);

	/* observer long / lat */
	longitude = ln_deg_to_rad (observer->lng);
	latitude = ln_deg_to_rad (observer->lat);

	/* equ on pg89 */
	H = atan2 (sin (A), ( cos(A) * sin (latitude) + tan(h) * cos (latitude)));
	declination = sin(latitude) * sin(h) - cos(latitude) * cos(h) * cos(A);
	declination = asin (declination);

	/* get ra = sidereal - longitude + H and change sidereal to radians*/
	sidereal = ln_get_apparent_sidereal_time(JD);
	sidereal *= 2.0 * M_PI / 24.0;

	position->ra = ln_range_degrees(ln_rad_to_deg (sidereal - H + longitude));
	position->dec = ln_rad_to_deg (declination);
}

/*! \fn void ln_get_equ_from_ecl (struct ln_lnlat_posn * object, double JD, struct ln_equ_posn * position)
* \param object Object coordinates.
* \param JD Julian day
* \param position Pointer to store new position.
*
* Transform an objects ecliptical coordinates into equatorial coordinates
* for the given julian day.
*/
/* Equ 12.3, 12.4 pg 89 
*/
void ln_get_equ_from_ecl (struct ln_lnlat_posn * object, double JD, struct ln_equ_posn * position)
{
	double ra, declination, longitude, latitude;
	struct ln_nutation nutation;

	/* get obliquity of ecliptic and change it to rads */
	ln_get_nutation (JD, &nutation);
	nutation.ecliptic = ln_deg_to_rad (nutation.ecliptic); 

	/* change object's position into radians */

	/* object */
	longitude = ln_deg_to_rad(object->lng);
	latitude = ln_deg_to_rad(object->lat);

	/* Equ 12.3, 12.4 */
	ra = atan2 ((sin(longitude) * cos(nutation.ecliptic) - tan(latitude) * sin(nutation.ecliptic)), cos (longitude));
	declination = sin(latitude) * cos(nutation.ecliptic) + cos(latitude) * sin(nutation.ecliptic) * sin(longitude);
	declination = asin(declination);
	
	/* store in position */
	position->ra = ln_range_degrees(ln_rad_to_deg(ra));
	position->dec = ln_rad_to_deg(declination);
}

/*! \fn void ln_get_ecl_from_equ (struct ln_equ_posn * object, double JD, struct ln_lnlat_posn * position)
* \param object Object coordinates in B1950. Use ln_get_equ_prec2 to transform from J2000.
* \param JD Julian day
* \param position Pointer to store new position.
*
* Transform an objects equatorial cordinates into ecliptical coordinates
* for the given julian day.
*/
/* Equ 12.1, 12.2 Pg 88 
*/
void ln_get_ecl_from_equ (struct ln_equ_posn * object, double JD, struct ln_lnlat_posn * position)
{
	double ra, declination, latitude, longitude;
	struct ln_nutation nutation;
	
	/* object position */
	ra = ln_deg_to_rad (object->ra);
	declination = ln_deg_to_rad (object->dec);
	ln_get_nutation(JD, &nutation);
	nutation.ecliptic = ln_deg_to_rad (nutation.ecliptic);

	/* Equ 12.1, 12.2 */
	longitude = atan2 ((sin(ra) * cos(nutation.ecliptic) + tan(declination) * sin(nutation.ecliptic)), cos(ra));
	latitude = sin(declination) * cos(nutation.ecliptic) - cos(declination) * sin(nutation.ecliptic) * sin(ra);
	latitude = asin(latitude);

	/* store in position */
	position->lat = ln_rad_to_deg (latitude);
	position->lng = ln_range_degrees(ln_rad_to_deg (longitude));
}

/*! \fn void ln_get_ecl_from_rect (struct ln_rect_posn * rect, struct ln_lnlat_posn * posn)
* \param rect Rectangular coordinates.
* \param posn Pointer to store new position.
*
* Transform an objects rectangular coordinates into ecliptical coordinates.
*/
/* Equ 33.2
*/
void ln_get_ecl_from_rect (struct ln_rect_posn * rect, struct ln_lnlat_posn * posn)
{
	double t;
	
	t = sqrt (rect->X * rect->X + rect->Y * rect->Y);
	posn->lng = ln_range_degrees(ln_rad_to_deg (atan2 (rect->X, rect->Y)));
	posn->lat = ln_rad_to_deg (atan2 (t, rect->Z));
}

/*! \fn void ln_get_equ_from_gal (struct ln_gal_posn *gal, struct ln_equ_posn *equ)
* \param gal Galactic coordinates.
* \param equ B1950 equatorial coordinates. Use ln_get_equ_prec2 to transform to J2000.
* 
* Transform an object galactic coordinates into B1950 equatorial coordinate.
*/
/* Pg 94 */
void ln_get_equ_from_gal (struct ln_gal_posn *gal, struct ln_equ_posn *equ)
{
	double RAD_27_4, SIN_27_4, COS_27_4;

	double l_123, cos_l_123;
	double sin_b, cos_b, rad_gal_b;
	double y;

	RAD_27_4 = ln_deg_to_rad (27.4);
	SIN_27_4 = sin (RAD_27_4);
	COS_27_4 = cos (RAD_27_4);

	l_123 = ln_deg_to_rad (gal->l - 123);
	cos_l_123 = cos (l_123);

	rad_gal_b = ln_deg_to_rad (gal->b);

	sin_b = sin (rad_gal_b);
	cos_b = cos (rad_gal_b);

	y = atan2 (sin (l_123), cos_l_123 * SIN_27_4 - (sin_b / cos_b) * COS_27_4);
	equ->ra = ln_range_degrees (ln_rad_to_deg (y) + 12.25);
	equ->dec = ln_rad_to_deg (asin (sin_b * SIN_27_4 + cos_b * COS_27_4 * cos_l_123));
}

/*! \fn void ln_get_equ2000_from_gal (struct ln_gal_posn *gal, struct ln_equ_posn *equ)
* \param gal Galactic coordinates.
* \param equ J2000 equatorial coordinates.
* 
* Transform an object galactic coordinates into equatorial coordinate.
*/
void ln_get_equ2000_from_gal (struct ln_gal_posn *gal, struct ln_equ_posn *equ)
{
	ln_get_equ_from_gal (gal, equ);
	ln_get_equ_prec2 (equ, B1950, JD2000, equ);
}

/*! \fn ln_get_gal_from_equ (struct ln_equ_posn *equ, struct ln_gal_posn *gal)
* \param equ B1950 equatorial coordinates.
* \param gal Galactic coordinates.
* 
* Transform an object B1950 equatorial coordinate into galactic coordinates.
*/
/* Pg 94 */
void ln_get_gal_from_equ (struct ln_equ_posn *equ, struct ln_gal_posn *gal)
{
	double RAD_27_4, SIN_27_4, COS_27_4;

	double ra_192_25, cos_ra_192_25;
	double rad_equ_dec;
	double cos_dec, sin_dec;
	double x;

	RAD_27_4 = ln_deg_to_rad (27.4);
	SIN_27_4 = sin (RAD_27_4);
	COS_27_4 = cos (RAD_27_4);

	ra_192_25 = ln_deg_to_rad (192.25 - equ->ra);
	cos_ra_192_25 = cos (ra_192_25);

	rad_equ_dec = ln_deg_to_rad (equ->dec);

	sin_dec = sin (rad_equ_dec);
	cos_dec = cos (rad_equ_dec);

	x = atan2 (sin (ra_192_25), cos_ra_192_25 * SIN_27_4 - (sin_dec / cos_dec) * COS_27_4);
	gal->l = ln_range_degrees (303 - ln_rad_to_deg (x));
	gal->b = ln_rad_to_deg (asin (sin_dec * SIN_27_4 + cos_dec * COS_27_4 * cos_ra_192_25));
}

/*! \fn void ln_get_gal_from_equ2000 (struct ln_equ_posn *equ, struct ln_gal_posn *gal)
* \param equ J2000 equatorial coordinates.
* \param gal Galactic coordinates.
* 
* Transform an object J2000 equatorial coordinate into galactic coordinates.
*/
void ln_get_gal_from_equ2000 (struct ln_equ_posn *equ, struct ln_gal_posn *gal)
{
	struct ln_equ_posn equ_1950;
	ln_get_equ_prec2 (equ, JD2000, B1950, &equ_1950);
	ln_get_gal_from_equ (&equ_1950, gal);
}

/*! \example transforms.c
 * 
 * Examples of how to use transformation functions. 
 */
