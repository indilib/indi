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
 *  Copyright (C) 2002 - 2005 Liam Girdwood
 *  Copyright (C) 2004 Petr Kubanek
 */

#define _GNU_SOURCE

#include <math.h>
#include <stdlib.h>
#include <libnova/parabolic_motion.h>
#include <libnova/hyperbolic_motion.h>
#include <libnova/solar.h>
#include <libnova/earth.h>
#include <libnova/transform.h>
#include <libnova/rise_set.h>
#include <libnova/dynamical_time.h>
#include <libnova/sidereal_time.h>
#include <libnova/utility.h>

#define GAUS_GRAV	0.01720209895	// Gaussian gravitational constant k

/*! \fn double ln_solve_hyp_barker (double Q1, double G, double t);
* \param Q1 See 35.0
* \param G See 35.0
* \param t Time since perihelion in days
* \return Solution of Barkers equation
*
* Solve Barkers equation. LIAM add more 
*/
/* Equ 34.3, Barkers Equation */
double ln_solve_hyp_barker (double Q1, double G, double t)
{
	double S,S0,S1,Y,G1,Q2,Q3,Z1,F;
#define PREC   1e-10
	
	int Z,L;
        
        Q2 = Q1 * t;
        S = 2 / (3 * fabs (Q2));
        S = 2 / tan (2 * atan (cbrt (tan (atan (S) / 2))));
        if (t < 0)
                S = -S;
        L = 0;
	/* we have initial s, so now do the iteration */
        do
        {
	        S0 = S;
                Z = 1;
                Y = S * S;
                G1 = -Y * S;
                Q3 = Q2 + 2.0 * G * S * Y / 3.0;
next_z:
                Z++;
                G1 = -G1 * G * Y;
                Z1 = (Z - (Z + 1) * G) / (2.0 * Z + 1.0);
                F = Z1 * G1;
                Q3 = Q3 + F;
                if (Z > 100 || fabs(F) > 10000)
                        return nan("0");
                if (fabs(F) > PREC)
                        goto next_z;
                L++;
                if (L > 100)
                        return nan("0");
                do
                {
                        S1 = S;
                        S = (2 * S * S * S / 3 + Q3) / (S * S + 1);
                } while (fabs (S - S1) > PREC);
        } while (fabs (S - S0) > PREC);

	return S;
}

/*! \fn double ln_get_hyp_true_anomaly (double q, double e, double t);
* \param q Perihelion distance in AU
* \param e Orbit eccentricity
* \param t Time since perihelion
* \return True anomaly (degrees)
*
* Calculate the true anomaly. 
*/
/* equ 30.1 */
double ln_get_hyp_true_anomaly (double q, double e, double t)
{
	double v;
	double s;
	double Q, gama;

	Q = (GAUS_GRAV / (2 * q)) * sqrt ((1 + e) / q);
	gama = (1 - e) / (1 + e); 
	
	s = ln_solve_hyp_barker (Q,gama,t);
	v = 2.0 * atan (s);

	return ln_range_degrees(ln_rad_to_deg(v));
}

/*! \fn double ln_get_hyp_radius_vector (double q, double e, double t);
* \param q Perihelion distance in AU
* \param e Orbit eccentricity
* \param t Time since perihelion in days
* \return Radius vector AU
*
* Calculate the radius vector. 
*/
/* equ 30.2 */
double ln_get_hyp_radius_vector (double q, double e, double t)
{
	return q * ( 1.0 + e) / (1 + e * cos (ln_deg_to_rad (ln_get_hyp_true_anomaly (q,e,t))));
}

/*! \fn void ln_get_hyp_helio_rect_posn (struct ln_hyp_orbit* orbit, double JD, struct ln_rect_posn* posn);
* \param orbit Orbital parameters of object.
* \param JD Julian day
* \param posn Position pointer to store objects position
*
* Calculate the objects rectangular heliocentric position given it's orbital
* elements for the given julian day. 
*/
void ln_get_hyp_helio_rect_posn (struct ln_hyp_orbit* orbit, double JD, struct ln_rect_posn* posn)
{
	double A,B,C;
	double F,G,H;
	double P,Q,R;
	double sin_e, cos_e;
	double a,b,c;
	double sin_omega, sin_i, cos_omega, cos_i;
	double r,v,t;
	
	/* time since perihelion */
	t = JD - orbit->JD;

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

	/* get true anomaly */
	v = ln_get_hyp_true_anomaly (orbit->q, orbit->e, t);
	
	/* get radius vector */
	r = ln_get_hyp_radius_vector (orbit->q, orbit->e, t);

	/* equ 33.9 */
	posn->X = r * a * sin (A + ln_deg_to_rad(orbit->w + v));
	posn->Y = r * b * sin (B + ln_deg_to_rad(orbit->w + v));
	posn->Z = r * c * sin (C + ln_deg_to_rad(orbit->w + v));
}


/*! \fn void ln_get_hyp_geo_rect_posn (struct ln_hyp_orbit* orbit, double JD, struct ln_rect_posn* posn);
* \param orbit Orbital parameters of object.
* \param JD Julian day
* \param posn Position pointer to store objects position
*
* Calculate the objects rectangular geocentric position given it's orbital
* elements for the given julian day. 
*/
void ln_get_hyp_geo_rect_posn (struct ln_hyp_orbit* orbit, double JD, struct ln_rect_posn* posn)
{
	struct ln_rect_posn p_posn, e_posn;
	struct ln_helio_posn earth;
	
	/* parabolic helio rect coords */
	ln_get_hyp_helio_rect_posn (orbit, JD, &p_posn);
	
	/* earth rect coords */
	ln_get_earth_helio_coords (JD, &earth);
	
	ln_get_rect_from_helio (&earth, &e_posn);
	posn->X = p_posn.X - e_posn.X;
	posn->Y = p_posn.Y - e_posn.Y;
	posn->Z = p_posn.Z - e_posn.Z;
}


/*!
* \fn void ln_get_hyp_body_equ_coords (double JD, struct ln_hyp_orbit* orbit, struct ln_equ_posn * posn)
* \param JD Julian Day.
* \param orbit Orbital parameters.
* \param posn Pointer to hold asteroid position.
*
* Calculate a bodies equatorial coordinates for the given julian day.
*/
void ln_get_hyp_body_equ_coords (double JD, struct ln_hyp_orbit* orbit, struct ln_equ_posn * posn)
{
	struct ln_rect_posn body_rect_posn, sol_rect_posn;
	double dist, t;
	double x,y,z;
	
	/* get solar and body rect coords */
	ln_get_hyp_helio_rect_posn (orbit, JD, &body_rect_posn);
	ln_get_solar_geo_coords (JD, &sol_rect_posn);

	/* calc distance and light time */
	dist = ln_get_rect_distance (&body_rect_posn, &sol_rect_posn);
	t = ln_get_light_time (dist);
	
	/* repeat calculation with new time (i.e. JD - t) */
	ln_get_hyp_helio_rect_posn (orbit, JD - t, &body_rect_posn);
	
	/* calc equ coords equ 33.10 */
	x = sol_rect_posn.X + body_rect_posn.X;
	y = sol_rect_posn.Y + body_rect_posn.Y;
	z = sol_rect_posn.Z + body_rect_posn.Z;

	posn->ra = ln_range_degrees (ln_rad_to_deg(atan2 (y,x)));
	posn->dec = ln_rad_to_deg(atan2 (z,sqrt (x * x + y * y)));
}

/*!
* \fn double ln_get_hyp_body_earth_dist (double JD, struct ln_hyp_orbit* orbit)
* \param JD Julian day.
* \param orbit Orbital parameters
* \returns Distance in AU
*
* Calculate the distance between a body and the Earth
* for the given julian day.
*/
double ln_get_hyp_body_earth_dist (double JD, struct ln_hyp_orbit* orbit)
{
	struct ln_rect_posn body_rect_posn, earth_rect_posn;
			
	/* get solar and body rect coords */
	ln_get_hyp_geo_rect_posn (orbit, JD, &body_rect_posn);
	earth_rect_posn.X = 0;
	earth_rect_posn.Y = 0;
	earth_rect_posn.Z = 0;
	
	/* calc distance */
	return ln_get_rect_distance (&body_rect_posn, &earth_rect_posn);
}

/*!
* \fn double ln_get_hyp_body_solar_dist (double JD, struct ln_hyp_orbit* orbit)
* \param JD Julian Day.
* \param orbit Orbital parameters
* \return The distance in AU between the Sun and the body. 
*
* Calculate the distance between a body and the Sun.
*/
double ln_get_hyp_body_solar_dist (double JD, struct ln_hyp_orbit* orbit)
{
	struct ln_rect_posn body_rect_posn, sol_rect_posn;
	
	/* get solar and body rect coords */
	ln_get_hyp_helio_rect_posn (orbit, JD, &body_rect_posn);
	sol_rect_posn.X = 0;
	sol_rect_posn.Y = 0;
	sol_rect_posn.Z = 0;
	
	/* calc distance */
	return ln_get_rect_distance (&body_rect_posn, &sol_rect_posn);
}

/*! \fn double ln_get_hyp_body_phase_angle (double JD, struct ln_hyp_orbit* orbit);
* \param JD Julian day
* \param orbit Orbital parameters
* \return Phase angle.
*
* Calculate the phase angle of the body. The angle Sun - body - Earth. 
*/
double ln_get_hyp_body_phase_angle (double JD, struct ln_hyp_orbit* orbit)
{
	double r,R,d;
	double t;
	double phase;
	
	/* time since perihelion */
	t = JD - orbit->JD;
	
	/* get radius vector */
	r = ln_get_hyp_radius_vector (orbit->q, orbit->e, t);
	
	/* get solar and Earth-Sun distances */
	R = ln_get_earth_solar_dist (JD);
	d = ln_get_hyp_body_solar_dist (JD, orbit);

	phase = (r * r + d * d - R * R) / ( 2.0 * r * d );
	return ln_range_degrees (ln_rad_to_deg (acos (phase)));
}

/*! \fn double ln_get_hyp_body_elong (double JD, struct ln_hyp_orbit* orbit);
* \param JD Julian day
* \param orbit Orbital parameters
* \return Elongation to the Sun.
*
* Calculate the bodies elongation to the Sun.. 
*/
double ln_get_hyp_body_elong (double JD, struct ln_hyp_orbit* orbit)
{
	double r,R,d;
	double t;
	double elong;
	
	/* time since perihelion */
	t = JD - orbit->JD;
	
	/* get radius vector */
	r = ln_get_hyp_radius_vector (orbit->q, orbit->e, t);
	
	/* get solar and Earth-Sun distances */
	R = ln_get_earth_solar_dist (JD);
	d = ln_get_hyp_body_solar_dist (JD, orbit);

	elong = (R * R + d * d - r * r) / ( 2.0 * R * d );
	return ln_range_degrees (ln_rad_to_deg (acos (elong)));
}

/*! \fn double ln_get_hyp_body_rst (double JD, struct ln_lnlat_posn * observer, struct ln_hyp_orbit* orbit, struct ln_rst_time * rst);
* \param JD Julian day
* \param observer Observers position
* \param orbit Orbital parameters
* \param rst Pointer to store Rise, Set and Transit time in JD
* \return 0 for success, else 1 for circumpolar.
*
* Calculate the time the rise, set and transit (crosses the local meridian at upper culmination)
* time of a body with a parabolic orbit for the given Julian day.
*
* Note: this functions returns 1 if the body is circumpolar, that is it remains the whole
* day above or below the horizon. Returns -1 when it remains whole day below the horizon.
*/
int ln_get_hyp_body_rst (double JD, struct ln_lnlat_posn * observer, struct ln_hyp_orbit* orbit, struct ln_rst_time * rst)
{
	return ln_get_hyp_body_rst_horizon (JD, observer, orbit, LN_STAR_STANDART_HORIZON, rst);
}

/*! \fn double ln_get_hyp_body_rst_horizon (double JD, struct ln_lnlat_posn * observer, struct ln_hyp_orbit* orbit, double horizon, struct ln_rst_time * rst);
* \param JD Julian day
* \param observer Observers position
* \param orbit Orbital parameters
* \param horizon Horizon height
* \param rst Pointer to store Rise, Set and Transit time in JD
* \return 0 for success, 1 for circumpolar (above the horizon), -1 for circumpolar (bellow the horizon)
*
* Calculate the time the rise, set and transit (crosses the local meridian at upper culmination)
* time of a body with a parabolic orbit for the given Julian day.
*
* Note: this functions returns 1 if the body is circumpolar, that is it remains the whole
* day above or below the horizon. Returns -1 when it remains whole day below the horizon.
*/
int ln_get_hyp_body_rst_horizon (double JD, struct ln_lnlat_posn * observer, struct ln_hyp_orbit* orbit, double horizon, struct ln_rst_time * rst)
{
	return ln_get_motion_body_rst_horizon (JD, observer, (get_motion_body_coords_t) ln_get_hyp_body_equ_coords, orbit, horizon, rst);
}

/*! \fn double ln_get_hyp_body_next_rst (double JD, struct ln_lnlat_posn * observer, struct ln_hyp_orbit * orbit, struct ln_rst_time * rst);
* \param JD Julian day
* \param observer Observers position
* \param orbit Orbital parameters
* \param rst Pointer to store Rise, Set and Transit time in JD
* \return 0 for success, else 1 for circumpolar (above the horizon), -1 for circumpolar (bellow the horizon)
*
* Calculate the time of next rise, set and transit (crosses the local meridian at upper culmination)
* time of a body with an hyperbolic orbit for the given Julian day.
*
* This function guarantee, that rise, set and transit will be in <JD, JD+1> range.
*
* Note: this functions returns 1 if the body is circumpolar, that is it remains the whole
* day above the horizon. Returns -1 when it remains the whole day below the horizon.
*/
int ln_get_hyp_body_next_rst (double JD, struct ln_lnlat_posn * observer, struct ln_hyp_orbit * orbit, struct ln_rst_time * rst)
{
	return ln_get_hyp_body_next_rst_horizon (JD, observer, orbit, LN_STAR_STANDART_HORIZON, rst);
}

/*! \fn double ln_get_hyp_body_next_rst_horizon (double JD, struct ln_lnlat_posn * observer, struct ln_hyp_orbit * orbit, double horizon, struct ln_rst_time * rst);
* \param JD Julian day
* \param observer Observers position
* \param orbit Orbital parameters
* \param horizon Horizon height
* \param rst Pointer to store Rise, Set and Transit time in JD
* \return 0 for success, else 1 for circumpolar (above the horizon), -1 for circumpolar (bellow the horizon)
*
* Calculate the time of next rise, set and transit (crosses the local meridian at upper culmination)
* time of a body with an hyperbolic orbit for the given Julian day.
*
* This function guarantee, that rise, set and transit will be in <JD, JD+1> range.
*
* Note: this functions returns 1 if the body is circumpolar, that is it remains the whole
* day above the horizon. Returns -1 when it remains the whole day below the horizon.
*/
int ln_get_hyp_body_next_rst_horizon (double JD, struct ln_lnlat_posn * observer, struct ln_hyp_orbit * orbit, double horizon, struct ln_rst_time * rst)
{
	return ln_get_motion_body_next_rst_horizon (JD, observer, (get_motion_body_coords_t) ln_get_hyp_body_equ_coords, orbit, horizon, rst);
}

/*! \fn double ln_get_hyp_body_next_rst_horizon_future (double JD, struct ln_lnlat_posn * observer, struct ln_hyp_orbit * orbit, double horizon, int day_limit, struct ln_rst_time * rst);
* \param JD Julian day
* \param observer Observers position
* \param orbit Orbital parameters
* \param horizon Horizon height
* \param day_limit Maximal number of days that will be searched for next rise and set
* \param rst Pointer to store Rise, Set and Transit time in JD
* \return 0 for success, else 1 for circumpolar (above the horizon), -1 for circumpolar (bellow the horizon)
*
* Calculate the time of next rise, set and transit (crosses the local meridian at upper culmination)
* time of a body with an hyperbolic orbit for the given Julian day.
*
* This function guarantee, that rise, set and transit will be in <JD, JD + day_limit> range.
*
* Note: this functions returns 1 if the body is circumpolar, that is it remains the whole
* day above the horizon. Returns -1 when it remains the whole day below the horizon.
*/
int ln_get_hyp_body_next_rst_horizon_future (double JD, struct ln_lnlat_posn * observer, struct ln_hyp_orbit * orbit, double horizon, int day_limit, struct ln_rst_time * rst)
{
	return ln_get_motion_body_next_rst_horizon_future (JD, observer, (get_motion_body_coords_t) ln_get_hyp_body_equ_coords, orbit, horizon, day_limit, rst);
}
