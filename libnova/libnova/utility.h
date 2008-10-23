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

#ifndef _LN_UTILITY_H
#define _LN_UTILITY_H

#include <libnova/ln_types.h>

#ifdef __WIN32__
#include <time.h>
// cbrt replacement
#define cbrt(x)   pow (x,1.0/3.0)
// nan
#define nan(x)    0
#endif

#ifdef __cplusplus
extern "C" {
#endif


/*! \defgroup version Libnova library version information
*/

/*! \fn const char * ln_get_version (void)
* \brief Library Version Number
* \ingroup version
*/
const char * ln_get_version (void);

/*! \defgroup misc Misc. Functions
*
* Misc functions.
*/

/*! \fn double ln_get_dec_location(char * s)
* \ingroup misc
* \brief Obtains Latitude, Longitude, RA or Declination from a string.                     
*/
double ln_get_dec_location(char *s);


/*! \fn char * ln_get_humanr_location(double location)    
*  \ingroup misc
*  \brief Obtains a human readable location in the form: ddºmm'ss.ss"             
*/
const char * ln_get_humanr_location(double location);

/*
* \fn double ln_get_rect_distance (struct ln_rect_posn * a, struct ln_rect_posn * b)
* \ingroup misc
*/
double ln_get_rect_distance (struct ln_rect_posn * a, struct ln_rect_posn * b);

/*! \defgroup conversion General Conversion Functions
*
* Conversion from one libnova type to another.
*/

/*! \fn double ln_rad_to_deg (double radians)
* \brief radians to degrees
* \ingroup conversion
*/
double ln_rad_to_deg (double radians);

/*! \fn double ln_deg_to_rad (double radians)
* \brief degrees to radians
* \ingroup conversion
*/
double ln_deg_to_rad (double degrees);

/*! \fn double ln_hms_to_deg (struct ln_hms * hms)
* \brief hours to degrees 
* \ingroup conversion 
*/
double ln_hms_to_deg (struct ln_hms * hms);

/*! \fn void ln_deg_to_hms (double degrees, struct ln_hms * hms)
* \brief degrees to hours 
* \ingroup conversion 
*/
void ln_deg_to_hms (double degrees, struct ln_hms * hms);

/*! \fn double ln_hms_to_rad (struct ln_hms * hms)
* \brief hours to radians. 
* \ingroup conversion 
*/
double ln_hms_to_rad (struct ln_hms * hms);

/*! \fn void ln_deg_to_hms (double radians, struct ln_hms * hms)
* \brief radians to hours 
* \ingroup conversion 
*/
void ln_rad_to_hms (double radians, struct ln_hms * hms);

/*! \fn double ln_dms_to_deg (struct ln_dms * dms)
* \brief dms to degrees 
* \ingroup conversion 
*/
double ln_dms_to_deg (struct ln_dms * dms);

/*! \fn void ln_deg_to_dms (double degrees, struct ln_dms * dms)
* \brief degrees to dms
* \ingroup conversion 
*/
void ln_deg_to_dms (double degrees, struct ln_dms * dms);

/*! \fn double ln_dms_to_rad (struct ln_dms * dms)
* \brief dms to radians
* \ingroup conversion 
*/
double ln_dms_to_rad (struct ln_dms * dms);

/*! \fn void ln_rad_to_dms (double radians, struct ln_dms * dms)
* \brief radians to dms 
* \ingroup conversion
*/
void ln_rad_to_dms (double radians, struct ln_dms * dms);

/*! \fn void ln_hequ_to_equ (struct lnh_equ_posn * hpos, struct ln_equ_posn * pos)
* \brief human readable equatorial position to double equatorial position
* \ingroup conversion
*/
void ln_hequ_to_equ (struct lnh_equ_posn * hpos, struct ln_equ_posn * pos);
	
/*! \fn void ln_equ_to_hequ (struct ln_equ_posn * pos, struct lnh_equ_posn * hpos)
* \brief human double equatorial position to human readable equatorial position
* \ingroup conversion
*/
void ln_equ_to_hequ (struct ln_equ_posn * pos, struct lnh_equ_posn * hpos);
	
/*! \fn void ln_hhrz_to_hrz (struct lnh_hrz_posn * hpos, struct ln_hrz_posn * pos)
* \brief human readable horizontal position to double horizontal position
* \ingroup conversion
*/
void ln_hhrz_to_hrz (struct lnh_hrz_posn * hpos, struct ln_hrz_posn * pos);
	
/*! \fn void ln_hrz_to_hhrz (struct ln_hrz_posn * pos, struct lnh_hrz_posn * hpos)
* \brief double horizontal position to human readable horizontal position
* \ingroup conversion
*/
void ln_hrz_to_hhrz (struct ln_hrz_posn * pos, struct lnh_hrz_posn * hpos);

/*! \fn const char * ln_hrz_to_nswe (struct ln_hrz_posn * pos);
 * \brief returns direction of given azimut - like N,S,W,E,NSW,...
 * \ingroup conversion
 */ 
const char * ln_hrz_to_nswe (struct ln_hrz_posn * pos);
	
/*! \fn void ln_hlnlat_to_lnlat (struct lnh_lnlat_posn * hpos, struct ln_lnlat_posn * pos)
* \brief human readable long/lat position to double long/lat position
* \ingroup conversion
*/
void ln_hlnlat_to_lnlat (struct lnh_lnlat_posn * hpos, struct ln_lnlat_posn * pos);
	
/*! \fn void ln_lnlat_to_hlnlat (struct ln_lnlat_posn * pos, struct lnh_lnlat_posn * hpos)
* \brief double long/lat position to human readable long/lat position
* \ingroup conversion
*/
void ln_lnlat_to_hlnlat (struct ln_lnlat_posn * pos, struct lnh_lnlat_posn * hpos);
	
/*! \fn void ln_add_secs_hms (struct ln_hms * hms, double seconds)
* \brief add seconds to hms 
* \ingroup conversion 
*/
void ln_add_secs_hms (struct ln_hms * hms, double seconds);

/*! \fn void ln_add_hms (struct ln_hms * source, struct ln_hms * dest)
* \brief add hms to hms 
* \ingroup conversion 
*/
void ln_add_hms (struct ln_hms * source, struct ln_hms * dest);

/*! \fn void ln_range_degrees (double angle)
* \brief puts a large angle in the correct range 0 - 360 degrees 
* \ingroup conversion 
*/
double ln_range_degrees (double angle);

/*! \fn void ln_range_radians (double angle)
* \brief puts a large angle in the correct range 0 - 2PI radians 
* \ingroup conversion 
*/
double ln_range_radians (double angle);
double ln_range_radians2 (double angle);

/*
* \fn double ln_get_light_time (double dist)
* \brief Convert units of AU into light days.
* \ingroup conversion
*/
double ln_get_light_time (double dist);

/*! \fn double ln_interpolate3 (double n, double y1, double y2, double y3)
* \ingroup misc
* \brief Calculate an intermediate value of the 3 arguments.
*/
double ln_interpolate3 (double n, double y1, double y2, double y3);

/*! \fn double ln_interpolate5 (double n, double y1, double y2, double y3, double y4, double y5)
* \ingroup misc
* \brief Calculate an intermediate value of the 5 arguments.
*/
double ln_interpolate5 (double n, double y1, double y2, double y3, double y4, double y5);

#ifdef __WIN32__

/* Catches calls to the POSIX gmtime_r and converts them to a related WIN32 version. */
struct tm *gmtime_r (time_t *t, struct tm *gmt);

/* Catches calls to the POSIX gettimeofday and converts them to a related WIN32 version. */
int gettimeofday(struct timeval *tp, struct timezone *tzp);

/* Catches calls to the POSIX strtok_r and converts them to a related WIN32 version. */
char *strtok_r(char *str, const char *sep, char **last);

#endif /* __WIN32__ */

/* C89 substitutions for C99 functions. */
#ifdef __C89_SUB__

/* Simple cube root */
double cbrt (double x);

/* Not a Number function generator */
double nan (const char *code);

/* Simple round to nearest */
double round (double x);

#endif /* __C89_SUB__ */

#ifdef __cplusplus
};
#endif

#endif
