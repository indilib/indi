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
#include <libnova/comet.h>
#include <libnova/elliptic_motion.h>
#include <libnova/parabolic_motion.h>

/*!
* \fn double ln_get_ell_comet_mag (double JD, struct ln_ell_orbit * orbit, double g, double k)
* \param JD Julian day.
* \param orbit Orbital parameters
* \param g Absolute magnitude
* \param k Comet constant
* \return The visual magnitude. 
*
* Calculate the visual magnitude of a comet in an elliptical orbit.
*/
double ln_get_ell_comet_mag (double JD, struct ln_ell_orbit * orbit, double g, double k)
{
	double d, r;
	double E,M;
	
	/* get mean anomaly */
	if (orbit->n == 0)
		orbit->n = ln_get_ell_mean_motion (orbit->a);
	M = ln_get_ell_mean_anomaly (orbit->n, JD - orbit->JD);
	
	/* get eccentric anomaly */
	E = ln_solve_kepler (orbit->e, M);
	
	/* get radius vector */
	r = ln_get_ell_radius_vector (orbit->a, orbit->e, E);
	d = ln_get_ell_body_solar_dist (JD, orbit);
	
	return g + 5.0 * log10 (d) + k * log10 (r);
}

/*!
* \fn double ln_get_par_comet_mag (double JD, struct ln_par_orbit * orbit, double g, double k)
* \param JD Julian day.
* \param orbit Orbital parameters
* \param g Absolute magnitude
* \param k Comet constant
* \return The visual magnitude. 
*
* Calculate the visual magnitude of a comet in a parabolic orbit.
*/
double ln_get_par_comet_mag (double JD, struct ln_par_orbit * orbit, double g, double k)
{
	double d,r,t;
	
	/* time since perihelion */
	t = JD - orbit->JD;
	
	/* get radius vector */
	r = ln_get_par_radius_vector (orbit->q, t);
	d = ln_get_par_body_solar_dist (JD, orbit);

	return g + 5.0 * log10 (d) + k * log10 (r);
}

/*! \example comet.c
 * 
 * Examples of how to use comet functions. 
 */
