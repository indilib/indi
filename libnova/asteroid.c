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
#include <libnova/asteroid.h>
#include <libnova/elliptic_motion.h>
#include <libnova/utility.h>

/*!
* \fn double ln_get_asteroid_mag (double JD, struct ln_ell_orbit * orbit, double H, double G)
* \param JD Julian day.
* \param orbit Orbital parameters
* \param H Mean absolute visual magnitude
* \param G Slope parameter
* \return The visual magnitude. 
*
* Calculate the visual magnitude of an asteroid.
*/
double ln_get_asteroid_mag (double JD, struct ln_ell_orbit * orbit, double H, double G)
{
	double t1,t2;
	double b,r,d;
	double E,M;
	
	/* get phase angle */
	b = ln_get_ell_body_phase_angle (JD, orbit);
	b = ln_deg_to_rad (b);
	
	/* get mean anomaly */
	if (orbit->n == 0)
		orbit->n = ln_get_ell_mean_motion (orbit->a);
	M = ln_get_ell_mean_anomaly (orbit->n, JD - orbit->JD);
	
	/* get eccentric anomaly */
	E = ln_solve_kepler (orbit->e, M);
	
	/* get radius vector */
	r = ln_get_ell_radius_vector (orbit->a, orbit->e, E);
	d = ln_get_ell_body_solar_dist (JD, orbit);
	
	t1 = exp (-3.33 * pow (tan (b / 2.0), 0.63));
	t2 = exp (-.187 * pow (tan (b / 2.0), 1.22));
	
	return H + 5 * log10 (r * d) - 2.5 * log10 ((1.0 - G) * t1 + G * t2);
}

/*! \fn double ln_get_asteroid_sdiam_km (double H, double A)
* \param H Absolute magnitude of asteroid
* \param A Albedo of asteroid
* \return Semidiameter in km
*
* Calcaluate the semidiameter of an asteroid in km.
*
* Note: Many asteroids have an irregular shape and therefore this function returns
* an approximate value of the diameter.
*/
double ln_get_asteroid_sdiam_km (double H, double A)
{	
	return 3.13 - 0.2 * H - (0.5 * log10 (A));
}

/*! \fn double ln_get_asteroid_sdiam_arc (double JD, struct ln_ell_orbit * orbit, double H, double A)
* \param JD Julian day
* \param orbit Orbital parameters
* \param H Absolute magnitude of asteroid
* \param A Albedo of asteroid
* \return Semidiameter in seconds of arc
*
* Calcaluate the semidiameter of an asteroid in arc seconds.
*
* Note: Many asteroids have an irregular shape and therefore this function returns
* an approximate value of the diameter.
*/
double ln_get_asteroid_sdiam_arc (double JD, struct ln_ell_orbit * orbit, double H, double A)
{
	double d, dist;
	
	/* calc distance to Earth in AU */
	dist = ln_get_ell_body_earth_dist (JD, orbit);
	
	d = 3.13 - 0.2 * H - (0.5 * log10 (A));
	return 0.0013788 * d / dist;
}

/*! \example asteroid.c
 * 
 * Examples of how to use asteroid functions. 
 */
