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

#ifndef _LN_PARABOLIC_MOTION_H
#define _LN_PARABOLIC_MOTION_H

#include <libnova/ln_types.h>

#ifdef __cplusplus
extern "C" {
#endif

/*! \defgroup parabolic  Parabolic Motion
*
* Functions relating to the Parabolic motion of bodies.
*
* All angles are expressed in degrees.
*/

/*! \fn double ln_solve_barker (double q, double t);
* \brief Solve Barkers equation. 
* \ingroup parabolic
*/
double ln_solve_barker (double q, double t);

/*! \fn double ln_get_par_true_anomaly (double q, double t);
* \ingroup parabolic
* \brief Calculate the true anomaly. 
*/
double ln_get_par_true_anomaly (double q, double t);

/*! \fn double ln_get_par_radius_vector (double q, double t);
* \ingroup parabolic
* \brief Calculate the radius vector. 
*/
double ln_get_par_radius_vector (double q, double t);

/*! \fn void ln_get_par_geo_rect_posn (struct ln_par_orbit* orbit, double JD, struct ln_rect_posn* posn);
* \ingroup parabolic
* \brief Calculate an objects rectangular geocentric position.
*/
void ln_get_par_geo_rect_posn (struct ln_par_orbit* orbit, double JD, struct ln_rect_posn* posn);


/*! \fn void ln_get_par_helio_rect_posn (struct ln_par_orbit* orbit, double JD, struct ln_rect_posn* posn);
* \ingroup parabolic
* \brief Calculate an objects rectangular heliocentric position. 
*/
void ln_get_par_helio_rect_posn (struct ln_par_orbit* orbit, double JD, struct ln_rect_posn* posn);
	
/*!
* \fn void ln_get_par_body_equ_coords (double JD, struct ln_par_orbit * orbit, struct ln_equ_posn * posn)
* \ingroup parabolic
* \brief Calculate a bodies equatorial coordinates.
*/
void ln_get_par_body_equ_coords (double JD, struct ln_par_orbit * orbit, struct ln_equ_posn * posn);
	
/*!
* \fn double ln_get_par_body_earth_dist (double JD, struct ln_par_orbit * orbit)
* \ingroup parabolic
* \brief Calculate the distance between a body and the Earth.
*/
double ln_get_par_body_earth_dist (double JD, struct ln_par_orbit * orbit);

/*!
* \fn double ln_get_par_body_solar_dist (double JD, struct ln_par_orbit * orbit) 
* \ingroup parabolic
* \brief Calculate the distance between a body and the Sun.
*/
double ln_get_par_body_solar_dist (double JD, struct ln_par_orbit * orbit);

/*! \fn double ln_get_par_body_phase_angle (double JD, struct ln_par_orbit * orbit);
* \ingroup parabolic
* \brief Calculate the pase angle of the body. 
*/
double ln_get_par_body_phase_angle (double JD, struct ln_par_orbit * orbit);

/*! \fn double ln_get_par_body_elong (double JD, struct ln_par_orbit * orbit);
* \ingroup parabolic
* \brief Calculate the bodies elongation to the Sun. 
*/
double ln_get_par_body_elong (double JD, struct ln_par_orbit * orbit);

/*! \fn double ln_get_par_body_rst (double JD, struct ln_lnlat_posn * observer, struct ln_par_orbit * orbit, struct ln_rst_time * rst);
* \brief Calculate the time of rise, set and transit for a body with a parabolic orbit.
* \ingroup parabolic
*/
int ln_get_par_body_rst (double JD, struct ln_lnlat_posn * observer, struct ln_par_orbit * orbit, struct ln_rst_time * rst);

/*! \fn double ln_get_par_body_rst_horizon (double JD, struct ln_lnlat_posn * observer, struct ln_par_orbit * orbit, double horizon, struct ln_rst_time * rst);
* \brief Calculate the time of rise, set and transit for a body with a parabolic orbit.
* \ingroup parabolic
*/
int ln_get_par_body_rst_horizon (double JD, struct ln_lnlat_posn * observer, struct ln_par_orbit * orbit, double horizon, struct ln_rst_time * rst);

/*! \fn double ln_get_par_body_next_rst (double JD, struct ln_lnlat_posn * observer, struct ln_par_orbit * orbit, struct ln_rst_time * rst);
* \brief Calculate the time of rise, set and transit for a body with an parabolic orbit.
* \ingroup parabolic 
*/
int ln_get_par_body_next_rst (double JD, struct ln_lnlat_posn * observer, struct ln_par_orbit * orbit, struct ln_rst_time * rst);

/*! \fn double ln_get_par_body_next_rst_horizon (double JD, struct ln_lnlat_posn * observer, struct ln_par_orbit * orbit, double horizon, struct ln_rst_time * rst);
* \brief Calculate the time of rise, set and transit for a body with an parabolic orbit.
* \ingroup parabolic 
*/
int ln_get_par_body_next_rst_horizon (double JD, struct ln_lnlat_posn * observer, struct ln_par_orbit * orbit, double horizon, struct ln_rst_time * rst);

/*! \fn double ln_get_par_body_next_rst_horizon_future (double JD, struct ln_lnlat_posn * observer, struct ln_par_orbit * orbit, double horizon, int day_limit, struct ln_rst_time * rst);
* \brief Calculate the time of rise, set and transit for a body with an parabolic orbit.
* \ingroup parabolic 
*/
int ln_get_par_body_next_rst_horizon_future (double JD, struct ln_lnlat_posn * observer, struct ln_par_orbit * orbit, double horizon, int day_limit, struct ln_rst_time * rst);

#ifdef __cplusplus
};
#endif

#endif
