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
#include <libnova/elliptic_motion.h>
#include <libnova/solar.h>
#include <libnova/earth.h>
#include <libnova/transform.h>
#include <libnova/rise_set.h>
#include <libnova/dynamical_time.h>
#include <libnova/sidereal_time.h>
#include <libnova/utility.h>

/* number of steps in calculation, 3.32 steps for each significant 
digit required */
#define KEPLER_STEPS	53

/* the BASIC SGN() function  for doubles */
static double sgn (double x)
{
	if (x == 0.0)
		return (x);
	else
		if (x < 0.0)
			return (-1.0);
		else
			return (1.0);
}

/*! \fn double ln_solve_kepler (double E, double M);
* \param E Orbital eccentricity
* \param M Mean anomaly
* \return Eccentric anomaly
*
* Calculate the eccentric anomaly. 
* This method was devised by Roger Sinnott. (Sky and Telescope, Vol 70, pg 159)
*/
double ln_solve_kepler (double e, double M)
{
	double Eo = M_PI_2;
	double F, M1;
	double D = M_PI_4;
	int i;
	
	/* covert to radians */
	M = ln_deg_to_rad (M);
	
	F = sgn (M); 
	M = fabs (M) / (2.0 * M_PI);
	M = (M - (int)M) * 2.0 * M_PI * F;
	
	if (M < 0)
		M = M + 2.0 * M_PI;
	F = 1.0;
	
	if (M > M_PI)
		F = -1.0;
	
	if (M > M_PI)
		M = 2.0 * M_PI - M;
	
	for (i = 0; i < KEPLER_STEPS; i++) {
		M1 = Eo - e * sin (Eo);
		Eo = Eo + D * sgn (M - M1);
		D /= 2.0;
	}
	Eo *= F;
	
	/* back to degrees */
	Eo = ln_rad_to_deg (Eo);
	return Eo;
}

/*! \fn double ln_get_ell_mean_anomaly (double n, double delta_JD);
* \param n Mean motion (degrees/day)
* \param delta_JD Time since perihelion
* \return Mean anomaly (degrees)
*
* Calculate the mean anomaly.
*/
double ln_get_ell_mean_anomaly (double n, double delta_JD)
{
	return delta_JD * n;
}

/*! \fn double ln_get_ell_true_anomaly (double e, double E);
* \param e Orbital eccentricity
* \param E Eccentric anomaly
* \return True anomaly (degrees)
*
* Calculate the true anomaly. 
*/
/* equ 30.1 */
double ln_get_ell_true_anomaly (double e, double E)
{
	double v;
	
	E = ln_deg_to_rad (E);
	v = sqrt ((1.0 + e) / (1.0 - e)) * tan (E / 2.0);
	v = 2.0 * atan (v);
	v = ln_range_degrees (ln_rad_to_deg (v));
	return v;
}

/*! \fn double ln_get_ell_radius_vector (double a, double e, double E);
* \param a Semi-Major axis in AU
* \param e Orbital eccentricity
* \param E Eccentric anomaly
* \return Radius vector AU
*
* Calculate the radius vector. 
*/
/* equ 30.2 */
double ln_get_ell_radius_vector (double a, double e, double E)
{	
	return a * ( 1.0 - e * cos (ln_deg_to_rad (E)));
}


/*! \fn double ln_get_ell_smajor_diam (double e, double q);
* \param e Eccentricity
* \param q Perihelion distance in AU
* \return Semi-major diameter in AU
*
* Calculate the semi major diameter. 
*/
double ln_get_ell_smajor_diam (double e, double q)
{
	return q / (1.0 - e);
}

/*! \fn double ln_get_ell_sminor_diam (double e, double a);
* \param e Eccentricity
* \param a Semi-Major diameter in AU
* \return Semi-minor diameter in AU
*
* Calculate the semi minor diameter. 
*/
double ln_get_ell_sminor_diam (double e, double a)
{
	return a * sqrt (1 - e * e);
}

/*! \fn double ln_get_ell_mean_motion (double a);
* \param a Semi major diameter in AU
* \return Mean daily motion (degrees/day)
*
* Calculate the mean daily motion (degrees/day).  
*/
double ln_get_ell_mean_motion (double a)
{
	double q = 0.9856076686; /* Gaussian gravitational constant (degrees)*/
	return q / (a * sqrt(a));
}

/*! \fn void ln_get_ell_helio_rect_posn (struct ln_ell_orbit* orbit, double JD, struct ln_rect_posn* posn);
* \param orbit Orbital parameters of object.
* \param JD Julian day
* \param posn Position pointer to store objects position
*
* Calculate the objects rectangular heliocentric position given it's orbital
* elements for the given julian day. 
*/
void ln_get_ell_helio_rect_posn (struct ln_ell_orbit* orbit, double JD, struct ln_rect_posn* posn)
{
	double A,B,C;
	double F,G,H;
	double P,Q,R;
	double sin_e, cos_e;
	double a,b,c;
	double sin_omega, sin_i, cos_omega, cos_i;
	double M,v,E,r;
	
	/* J2000 obliquity of the ecliptic */
	sin_e = 0.397777156;
	cos_e = 0.917482062;
	
	/* equ 33.7 */
	sin_omega = sin (ln_deg_to_rad (orbit->omega));
	cos_omega = cos (ln_deg_to_rad (orbit->omega));
	sin_i = sin (ln_deg_to_rad  (orbit->i));
	cos_i = cos (ln_deg_to_rad  (orbit->i));
	F = cos_omega;
	G = sin_omega * cos_e;
	H = sin_omega * sin_e;
	P = -sin_omega * cos_i;
	Q = cos_omega * cos_i * cos_e - sin_i * sin_e;
	R = cos_omega * cos_i * sin_e + sin_i * cos_e;
	
	/* equ 33.8 */
	A = atan2 (F,P);
	B = atan2 (G,Q);
	C = atan2 (H,R);
	a = sqrt (F * F + P * P);
	b = sqrt (G * G + Q * Q);
	c = sqrt (H * H + R * R);

	/* get daily motion */
	if (orbit->n == 0) 
			orbit->n = ln_get_ell_mean_motion (orbit->a);

	/* get mean anomaly */
	M = ln_get_ell_mean_anomaly (orbit->n, JD - orbit->JD);

	/* get eccentric anomaly */
	E = ln_solve_kepler (orbit->e, M);

	/* get true anomaly */
	v = ln_get_ell_true_anomaly (orbit->e, E);

	/* get radius vector */
	r = ln_get_ell_radius_vector (orbit->a, orbit->e, E);

	/* equ 33.9 */
	posn->X = r * a * sin (A + ln_deg_to_rad(orbit->w + v));
	posn->Y = r * b * sin (B + ln_deg_to_rad(orbit->w + v));
	posn->Z = r * c * sin (C + ln_deg_to_rad(orbit->w + v));
}

/*! \fn void ln_get_ell_geo_rect_posn (struct ln_ell_orbit* orbit, double JD, struct ln_rect_posn* posn);
* \param orbit Orbital parameters of object.
* \param JD Julian day
* \param posn Position pointer to store objects position
*
* Calculate the objects rectangular geocentric position given it's orbital
* elements for the given julian day. 
*/
void ln_get_ell_geo_rect_posn (struct ln_ell_orbit* orbit, double JD, struct ln_rect_posn* posn)
{
	struct ln_rect_posn p_posn, e_posn;
	struct ln_helio_posn earth;
	
	/* elliptic helio rect coords */
	ln_get_ell_helio_rect_posn (orbit, JD, &p_posn);
	
	/* earth rect coords */
	ln_get_earth_helio_coords (JD, &earth);
	ln_get_rect_from_helio (&earth, &e_posn);

	posn->X = e_posn.X - p_posn.X;
	posn->Y = e_posn.Y - p_posn.Y;
	posn->Z = e_posn.Z - p_posn.Z;
}


/*!
* \fn void ln_get_ell_body_equ_coords (double JD, struct ln_ell_orbit * orbit, struct ln_equ_posn * posn)
* \param JD Julian Day.
* \param orbit Orbital parameters.
* \param posn Pointer to hold asteroid position.
*
* Calculate a bodies equatorial coordinates for the given julian day.
*/
void ln_get_ell_body_equ_coords (double JD, struct ln_ell_orbit * orbit, struct ln_equ_posn * posn)
{
	struct ln_rect_posn body_rect_posn, sol_rect_posn;
	double dist, t;
	double x,y,z;
	
	/* get solar and body rect coords */
	ln_get_ell_helio_rect_posn (orbit, JD, &body_rect_posn);
	ln_get_solar_geo_coords (JD, &sol_rect_posn);
	
	/* calc distance and light time */
	dist = ln_get_rect_distance (&body_rect_posn, &sol_rect_posn);
	t = ln_get_light_time (dist);

	/* repeat calculation with new time (i.e. JD - t) */
	ln_get_ell_helio_rect_posn (orbit, JD - t, &body_rect_posn);
	
	/* calc equ coords equ 33.10 */
	x = sol_rect_posn.X + body_rect_posn.X;
	y = sol_rect_posn.Y + body_rect_posn.Y;
	z = sol_rect_posn.Z + body_rect_posn.Z;

	posn->ra = ln_range_degrees(ln_rad_to_deg(atan2 (y,x)));
	posn->dec = ln_rad_to_deg(asin (z / sqrt (x * x + y * y + z * z)));
}


/*! \fn double ln_get_ell_orbit_len (struct ln_ell_orbit * orbit);
* \param orbit Orbital parameters
* \return Orbital length in AU
*
* Calculate the orbital length in AU. 
* 
* Accuracy: 
* - 0.001% for e < 0.88
* - 0.01% for e < 0.95
* - 1% for e = 0.9997
* - 3% for e = 1
*/
double ln_get_ell_orbit_len (struct ln_ell_orbit * orbit)
{
	double A,G,H;
	double b;
	
	b = ln_get_ell_sminor_diam (orbit->e, orbit->a);
	
	A = (orbit->a + b) / 2.0;
	G = sqrt (orbit->a * b);
	H = (2.0 * orbit->a * b) / (orbit->a + b);
	
	return M_PI * (( 21.0 * A - 2.0 * G - 3.0 * G) / 8.0);
}

/*! \fn double ln_get_ell_orbit_vel (double JD, struct ln_ell_orbit * orbit);
* \param JD Julian day.
* \param orbit Orbital parameters
* \return Orbital velocity in km/s.
*
* Calculate orbital velocity in km/s for the given julian day. 
*/
double ln_get_ell_orbit_vel (double JD, struct ln_ell_orbit * orbit)
{
	double V;
	double r;
	
	r = ln_get_ell_body_solar_dist (JD, orbit);
	V = 1.0 / r - 1.0 / (2.0 * orbit->a);
	V = 42.1219 * sqrt (V);
	return V;
}

/*! \fn double ln_get_ell_orbit_pvel (struct ln_ell_orbit * orbit);
* \param orbit Orbital parameters
* \return Orbital velocity in km/s.
*
* Calculate orbital velocity at perihelion in km/s. 
*/
double ln_get_ell_orbit_pvel (struct ln_ell_orbit * orbit)
{
	double V;
	
	V = 29.7847 / sqrt (orbit->a);
	V *= sqrt ((1.0 + orbit->e) / (1.0 - orbit->e));
	return V;
}

/*! \fn double ln_get_ell_orbit_avel (struct ln_ell_orbit * orbit);
* \param orbit Orbital parameters
* \return Orbital velocity in km/s.
*
* Calculate the orbital velocity at aphelion in km/s. 
*/
double ln_get_ell_orbit_avel (struct ln_ell_orbit * orbit)
{
	double V;
	
	V = 29.7847 / sqrt (orbit->a);
	V *= sqrt ((1.0 - orbit->e) / (1.0 + orbit->e));
	return V;
}

/*!
* \fn double ln_get_ell_body_solar_dist (double JD, struct ln_ell_orbit * orbit)
* \param JD Julian Day.
* \param orbit Orbital parameters
* \return The distance in AU between the Sun and the body. 
*
* Calculate the distance between a body and the Sun.
*/
double ln_get_ell_body_solar_dist (double JD, struct ln_ell_orbit * orbit)
{
	struct ln_rect_posn body_rect_posn, sol_rect_posn;
	
	/* get solar and body rect coords */
	ln_get_ell_helio_rect_posn (orbit, JD, &body_rect_posn);
	sol_rect_posn.X = 0;
	sol_rect_posn.Y = 0;
	sol_rect_posn.Z = 0;
	
	/* calc distance */
	return ln_get_rect_distance (&body_rect_posn, &sol_rect_posn);
}

/*!
* \fn double ln_get_ell_body_earth_dist (double JD, struct ln_ell_orbit * orbit)
* \param JD Julian day.
* \param orbit Orbital parameters
* \returns Distance in AU
*
* Calculate the distance between an body and the Earth
* for the given julian day.
*/
double ln_get_ell_body_earth_dist (double JD, struct ln_ell_orbit * orbit)
{
	struct ln_rect_posn body_rect_posn, earth_rect_posn;
			
	/* get solar and body rect coords */
	ln_get_ell_geo_rect_posn (orbit, JD, &body_rect_posn);
	earth_rect_posn.X = 0;
	earth_rect_posn.Y = 0;
	earth_rect_posn.Z = 0;
	
	/* calc distance */
	return ln_get_rect_distance (&body_rect_posn, &earth_rect_posn);
}


/*! \fn double ln_get_ell_body_phase_angle (double JD, struct ln_ell_orbit * orbit);
* \param JD Julian day
* \param orbit Orbital parameters
* \return Phase angle.
*
* Calculate the phase angle of the body. The angle Sun - body - Earth. 
*/
double ln_get_ell_body_phase_angle (double JD, struct ln_ell_orbit * orbit)
{
	double r,R,d;
	double E,M;
	double phase;
	
	/* get mean anomaly */
	if (orbit->n == 0)
		orbit->n = ln_get_ell_mean_motion (orbit->a);
	M = ln_get_ell_mean_anomaly (orbit->n, JD - orbit->JD);
	
	/* get eccentric anomaly */
	E = ln_solve_kepler (orbit->e, M);
	
	/* get radius vector */
	r = ln_get_ell_radius_vector (orbit->a, orbit->e, E);
	
	/* get solar and Earth distances */
	R = ln_get_ell_body_earth_dist (JD, orbit);
	d = ln_get_ell_body_solar_dist (JD, orbit);
	
	phase = (r * r + d * d - R * R) / ( 2.0 * r * d );
	return ln_range_degrees(acos (ln_deg_to_rad (phase)));
}


/*! \fn double ln_get_ell_body_elong (double JD, struct ln_ell_orbit * orbit);
* \param JD Julian day
* \param orbit Orbital parameters
* \return Elongation to the Sun.
*
* Calculate the bodies elongation to the Sun.. 
*/
double ln_get_ell_body_elong (double JD, struct ln_ell_orbit * orbit)
{
	double r,R,d;
	double t;
	double elong;
	double E,M;
	
	/* time since perihelion */
	t = JD - orbit->JD;
	
	/* get mean anomaly */
	if (orbit->n == 0)
		orbit->n = ln_get_ell_mean_motion (orbit->a);
	M = ln_get_ell_mean_anomaly (orbit->n, t);
	
	/* get eccentric anomaly */
	E = ln_solve_kepler (orbit->e, M);
	
	/* get radius vector */
	r = ln_get_ell_radius_vector (orbit->a, orbit->e, E);
	
	/* get solar and Earth-Sun distances */
	R = ln_get_earth_solar_dist (JD);
	d = ln_get_ell_body_solar_dist (JD, orbit);

	elong = (R * R + d * d - r * r) / ( 2.0 * R * d );
	return ln_range_degrees (ln_rad_to_deg (acos (elong)));
}

/*! \fn double ln_get_ell_body_rst (double JD, struct ln_lnlat_posn * observer, struct ln_ell_orbit * orbit, struct ln_rst_time * rst);
* \param JD Julian day
* \param observer Observers position
* \param orbit Orbital parameters
* \param rst Pointer to store Rise, Set and Transit time in JD
* \return 0 for success, else 1 for circumpolar (above the horizon), -1 for circumpolar (bellow the horizon)
*
* Calculate the time the rise, set and transit (crosses the local meridian at upper culmination)
* time of a body with an elliptic orbit for the given Julian day.
*
* Note: this functions returns 1 if the body is circumpolar, that is it remains the whole
* day above the horizon. Returns -1 when it remains the whole day below the horizon.
*/
int ln_get_ell_body_rst (double JD, struct ln_lnlat_posn * observer, struct ln_ell_orbit * orbit, struct ln_rst_time * rst)
{
	return ln_get_ell_body_rst_horizon (JD, observer, orbit, LN_STAR_STANDART_HORIZON, rst);
}

/*! \fn double ln_get_ell_body_rst_horizon (double JD, struct ln_lnlat_posn * observer, struct ln_ell_orbit * orbit, double horizon, struct ln_rst_time * rst);
* \param JD Julian day
* \param observer Observers position
* \param orbit Orbital parameters
* \param horizon Horizon height
* \param rst Pointer to store Rise, Set and Transit time in JD
* \return 0 for success, else 1 for circumpolar (above the horizon), -1 for circumpolar (bellow the horizon)
*
* Calculate the time the rise, set and transit (crosses the local meridian at upper culmination)
* time of a body with an elliptic orbit for the given Julian day.
*
* Note: this functions returns 1 if the body is circumpolar, that is it remains the whole
* day above the horizon. Returns -1 when it remains the whole day below the horizon.
*/
int ln_get_ell_body_rst_horizon (double JD, struct ln_lnlat_posn * observer, struct ln_ell_orbit * orbit, double horizon, struct ln_rst_time * rst)
{
	return ln_get_motion_body_rst_horizon (JD, observer, (get_motion_body_coords_t) ln_get_ell_body_equ_coords, orbit, horizon, rst);
}

/*! \fn double ln_get_ell_body_next_rst (double JD, struct ln_lnlat_posn * observer, struct ln_ell_orbit * orbit, struct ln_rst_time * rst);
* \param JD Julian day
* \param observer Observers position
* \param orbit Orbital parameters
* \param rst Pointer to store Rise, Set and Transit time in JD
* \return 0 for success, else 1 for circumpolar (above the horizon), -1 for circumpolar (bellow the horizon)
*
* Calculate the time of next rise, set and transit (crosses the local meridian at upper culmination)
* time of a body with an elliptic orbit for the given Julian day.
*
* This function guarantee, that rise, set and transit will be in <JD, JD+1> range.
*
* Note: this functions returns 1 if the body is circumpolar, that is it remains the whole
* day above the horizon. Returns -1 when it remains the whole day below the horizon.
*/
int ln_get_ell_body_next_rst (double JD, struct ln_lnlat_posn * observer, struct ln_ell_orbit * orbit, struct ln_rst_time * rst)
{
	return ln_get_ell_body_next_rst_horizon (JD, observer, orbit, LN_STAR_STANDART_HORIZON, rst);
}

/*! \fn double ln_get_ell_body_next_rst_horizon (double JD, struct ln_lnlat_posn * observer, struct ln_ell_orbit * orbit, double horizon, struct ln_rst_time * rst);
* \param JD Julian day
* \param observer Observers position
* \param orbit Orbital parameters
* \param horizon Horizon height
* \param rst Pointer to store Rise, Set and Transit time in JD
* \return 0 for success, else 1 for circumpolar (above the horizon), -1 for circumpolar (bellow the horizon)
*
* Calculate the time of next rise, set and transit (crosses the local meridian at upper culmination)
* time of a body with an elliptic orbit for the given Julian day.
*
* This function guarantee, that rise, set and transit will be in <JD, JD+1> range.
*
* Note: this functions returns 1 if the body is circumpolar, that is it remains the whole
* day above the horizon. Returns -1 when it remains the whole day below the horizon.
*/
int ln_get_ell_body_next_rst_horizon (double JD, struct ln_lnlat_posn * observer, struct ln_ell_orbit * orbit, double horizon, struct ln_rst_time * rst)
{
	return ln_get_motion_body_next_rst_horizon (JD, observer, (get_motion_body_coords_t) ln_get_ell_body_equ_coords, orbit, horizon, rst);
}

/*! \fn double ln_get_ell_body_next_rst_horizon (double JD, struct ln_lnlat_posn * observer, struct ln_ell_orbit * orbit, double horizon, struct ln_rst_time * rst);
* \param JD Julian day
* \param observer Observers position
* \param orbit Orbital parameters
* \param horizon Horizon height
* \param day_limit Maximal number of days that will be searched for next rise and set
* \param rst Pointer to store Rise, Set and Transit time in JD
* \return 0 for success, else 1 for circumpolar (above the horizon), -1 for circumpolar (bellow the horizon)
*
* Calculate the time of next rise, set and transit (crosses the local meridian at upper culmination)
* time of a body with an elliptic orbit for the given Julian day.
*
* This function guarantee, that rise, set and transit will be in <JD, JD + day_limit> range.
*
* Note: this functions returns 1 if the body is circumpolar, that is it remains the whole
* day above the horizon. Returns -1 when it remains the whole day below the horizon.
*/
int ln_get_ell_body_next_rst_horizon_future (double JD, struct ln_lnlat_posn * observer, struct ln_ell_orbit * orbit, double horizon, int day_limit, struct ln_rst_time * rst)
{
	return ln_get_motion_body_next_rst_horizon_future (JD, observer, (get_motion_body_coords_t) ln_get_ell_body_equ_coords, orbit, horizon, day_limit, rst);
}

/*!\fn double ln_get_ell_last_perihelion (double epoch_JD, double M, double n);
* \param epoch_JD Julian day of epoch
* \param M Mean anomaly
* \param n daily motion in degrees
* 
* Calculate the julian day of the last perihelion.
*/
double ln_get_ell_last_perihelion (double epoch_JD, double M, double n)
{
	return epoch_JD - (M / n);
}
