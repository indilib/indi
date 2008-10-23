/* $Id: utility.c,v 1.14 2008/05/16 11:29:43 pkubanek Exp $
 **
 * Copyright (C) 1999, 2000 Juan Carlos Remis
 * Copyright (C) 2002 Liam Girdwood
 *
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
 */

/*------------------------------------------------------------------------*/
/*                                                                        */
/*  Module:                                                               */
/*                                                                        */
/*  Description:                                                          */
/*                                                                        */
/*                                                                        */
/*  "CAVEAT UTILITOR".                                                    */
/*                                                                        */
/*                   "Non sunt multiplicanda entia praeter necessitatem"  */
/*                                                   Guillermo de Occam.  */
/*------------------------------------------------------------------------*/
/*  Revision History:                                                     */
/*                                                                        */
/*------------------------------------------------------------------------*/

/**/
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <string.h>
#include <math.h>
#include <ctype.h>
#include <libnova/libnova.h>

#ifndef __APPLE__
#include <malloc.h>
#endif

/* Include unistd.h only if not on a Win32 platform */
/* Include Win32 Headers sys/types.h and sys/timeb.h if on Win32 */
#ifndef __WIN32__
#include <unistd.h>
#else
#include <sys/types.h>
#include <sys/timeb.h>
#endif

/* Conversion factors between degrees and radians */
#define D2R  (1.7453292519943295769e-2)  /* deg->radian */
#define R2D  (5.7295779513082320877e1)   /* radian->deg */
#define R2S  (2.0626480624709635516e5)   /* arc seconds per radian */
#define S2R  (4.8481368110953599359e-6)  /* radians per arc second */

#define DM_PI (2*M_PI)
#define RADIAN (180.0 / M_PI)

static const char ln_version[] = LIBNOVA_VERSION;

/*! \fn char * ln_get_version (void)
* \return Null terminated version string.
*
* Return the libnova library version number string
* e.g. "0.4.0"
*/
const char * ln_get_version (void)
{
    return ln_version;
}


/* convert radians to degrees */
double ln_rad_to_deg (double radians)
{   
	return (radians * R2D);
}    

/* convert degrees to radians */
double ln_deg_to_rad (double degrees)
{   
	return (degrees * D2R);
}    

/* convert hours:mins:secs to degrees */
double ln_hms_to_deg (struct ln_hms *hms)
{
    double degrees;
    
    degrees = ((double)hms->hours / 24) * 360;
    degrees += ((double)hms->minutes / 60) * 15;
    degrees += ((double)hms->seconds / 60) * 0.25;
    
    return degrees;
}

/* convert hours:mins:secs to radians */
double ln_hms_to_rad (struct ln_hms *hms)
{
    double radians;
  
    radians = ((double)hms->hours / 24.0) * 2.0 * M_PI;
    radians += ((double)hms->minutes / 60.0) * 2.0 * M_PI / 24.0;
    radians += ((double)hms->seconds / 60.0) * 2.0 * M_PI / 1440.0;
    
    return radians;
}


/* convert degrees to hh:mm:ss */
void ln_deg_to_hms (double degrees, struct ln_hms * hms)
{
    double dtemp;
        
    degrees = ln_range_degrees (degrees);	
    
	/* divide degrees by 15 to get the hours */
    hms->hours = dtemp = degrees / 15.0;
    dtemp -= hms->hours;
    
    /* divide remainder by 60 to get minutes */
    hms->minutes = dtemp = dtemp * 60.0;
    dtemp -= hms->minutes;

    /* divide remainder by 60 to get seconds */
    hms->seconds = dtemp * 60.0;
    
    /* catch any overflows */
    if (hms->seconds > 59) {
    	hms->seconds = 0;
    	hms->minutes ++;
    }
    if (hms->minutes > 59) {
    	hms->minutes = 0;
    	hms->hours ++;
    }
}

/* convert radians to hh:mm:ss */
void ln_rad_to_hms (double radians, struct ln_hms * hms)
{
    double dtemp;
    double degrees;
         
    radians = ln_range_radians(radians);
    degrees = radians * 360.0 / (2.0 * M_PI);		
  
    /* divide radians by PI / 12 to get the hours */
    hms->hours = dtemp = degrees / 15.0;
    dtemp -= hms->hours;
    
    /* divide remainder by 60 to get minutes */
    hms->minutes = dtemp = dtemp * 60.0;
    dtemp -= hms->minutes;

    /* divide remainder by 60 to get seconds */
    hms->seconds = dtemp * 60.0;
    
    /* catch any overflows */
    if (hms->seconds > 59) {
    	hms->seconds = 0;
    	hms->minutes ++;
    }
    if (hms->minutes > 59) {
    	hms->minutes = 0;
    	hms->hours ++;
    }
}


/* convert dms to degrees */
double ln_dms_to_deg (struct ln_dms *dms)
{
    double degrees;
    
    degrees =  fabs((double)dms->degrees);
    degrees += fabs((double)dms->minutes / 60);
    degrees += fabs((double)dms->seconds / 3600);
	
	// negative ?
	if (dms->neg)
		degrees *= -1.0;

    return degrees;
}

/* convert dms to radians */
double ln_dms_to_rad (struct ln_dms *dms)
{
    double radians;
 
    radians =  fabs((double)dms->degrees / 360.0 * 2.0 * M_PI);
    radians += fabs((double)dms->minutes / 21600.0 * 2.0 * M_PI);
    radians += fabs((double)dms->seconds / 1296000.0 * 2.0 * M_PI);
	
	// negative ?
	if (dms->neg)
		radians *= -1.0;
	
    return radians;
}

/* convert degrees to dms */
void ln_deg_to_dms (double degrees, struct ln_dms * dms)
{
    double dtemp;

    if (degrees >= 0) 
		dms->neg = 0;
	else
		dms->neg = 1;

	degrees = fabs(degrees);
	dms->degrees = (int)degrees;
	dtemp = degrees - dms->degrees;
	
    /* divide remainder by 60 to get minutes */
    dms->minutes = dtemp = dtemp * 60;
    dtemp -= dms->minutes;
    
    /* divide remainder by 60 to get seconds */
    dms->seconds = dtemp * 60;
    
    /* catch any overflows */
    if (dms->seconds > 59) {
    	dms->seconds = 0;
    	dms->minutes ++;
    }
    if (dms->minutes > 59) {
    	dms->minutes = 0;
    	dms->degrees ++;
    }
}

/* convert radians to dms */
void ln_rad_to_dms (double radians, struct ln_dms * dms)
{
    double dtemp;
    double degrees;
	
    degrees = radians * 360.0 / (2.0 * M_PI);
    if (degrees >= 0) 
		dms->neg = 0;
	else
		dms->neg = 1;
	
    degrees = fabs(degrees);
	dms->degrees = (int)degrees;
	dtemp = degrees - dms->degrees;
	
    /* divide remainder by 60 to get minutes */
    dms->minutes = dtemp = dtemp * 60;
    dtemp -= dms->minutes;
    
    /* divide remainder by 60 to get seconds */
    dms->seconds = dtemp * 60;
    
     /* catch any overflows */
    if (dms->seconds > 59) {
    	dms->seconds = 0;
    	dms->minutes ++;
    }
    if (dms->minutes > 59) {
    	dms->minutes = 0;
    	dms->degrees ++;
    }
}


/* puts a large angle in the correct range 0 - 360 degrees */
double ln_range_degrees (double angle)
{
    double temp;
    
    if (angle >= 0.0 && angle < 360.0)
    	return angle;
 
	temp = (int)(angle / 360);
	if (angle < 0.0)
	   	temp --;
    temp *= 360;
	return angle - temp;
}

/* puts a large angle in the correct range 0 - 2PI radians */
double ln_range_radians (double angle)
{
    double temp;

    if (angle >= 0.0 && angle < (2.0 * M_PI))
    	return angle;
    
	temp = (int)(angle / (M_PI * 2.0));

	if (angle < 0.0)
		temp --;
	temp *= (M_PI * 2.0);
	return angle - temp;
}

/* puts a large angle in the correct range -2PI - 2PI radians */
/* preserve sign */
double ln_range_radians2 (double angle)
{
    double temp;
    
    if (angle > (-2.0 * M_PI) && angle < (2.0 * M_PI))
    	return angle;
    
	temp = (int)(angle / (M_PI * 2.0));
	temp *= (M_PI * 2.0);
	return angle - temp;
}


/* add seconds to hms */
void ln_add_secs_hms (struct ln_hms * hms, double seconds)
{
    struct ln_hms source_hms;
    
    /* breaks double seconds int hms */
    source_hms.hours = seconds / 3600;
    seconds -= source_hms.hours * 3600;
    source_hms.minutes = seconds / 60;
    seconds -= source_hms.minutes * 60; 
    source_hms.seconds = seconds;
    
    /* add hms to hms */
    ln_add_hms (&source_hms, hms);
}


/* add hms to hms */
void ln_add_hms (struct ln_hms * source, struct ln_hms * dest)
{
    dest->seconds += source->seconds;
    if (dest->seconds >= 60) {
        /* carry */
	    source->minutes ++;
	    dest->seconds -= 60;
	} else {
	    if (dest->seconds < 0) {
	        /* carry */
		    source->minutes --;
		    dest->seconds += 60;
		}
	}
	
	dest->minutes += source->minutes;
    if (dest->minutes >= 60) {
        /* carry */
	    source->hours ++;
	    dest->minutes -= 60;
	} else {
	    if (dest->seconds < 0) {
	        /* carry */
		    source->hours --;
		    dest->minutes += 60;
		}
	}
    
    dest->hours += source->hours;
}

/*! \fn void ln_hequ_to_equ (struct lnh_equ_posn * hpos, struct ln_equ_posn * pos)
* \brief human readable equatorial position to double equatorial position
* \ingroup conversion
*/
void ln_hequ_to_equ (struct lnh_equ_posn * hpos, struct ln_equ_posn * pos)
{
	pos->ra = ln_hms_to_deg (&hpos->ra);
	pos->dec = ln_dms_to_deg (&hpos->dec);
}
	
/*! \fn void ln_equ_to_hequ (struct ln_equ_posn * pos, struct lnh_equ_posn * hpos)
* \brief human double equatorial position to human readable equatorial position
* \ingroup conversion
*/
void ln_equ_to_hequ (struct ln_equ_posn * pos, struct lnh_equ_posn * hpos)
{
	ln_deg_to_hms (pos->ra, &hpos->ra);
	ln_deg_to_dms (pos->dec, &hpos->dec);
}
	
/*! \fn void ln_hhrz_to_hrz (struct lnh_hrz_posn * hpos, struct ln_hrz_posn * pos)
* \brief human readable horizontal position to double horizontal position
* \ingroup conversion
*/
void ln_hhrz_to_hrz (struct lnh_hrz_posn * hpos, struct ln_hrz_posn * pos)
{
	pos->alt = ln_dms_to_deg (&hpos->alt);
	pos->az = ln_dms_to_deg (&hpos->az);
}

/*! \fn void ln_hrz_to_hhrz (struct ln_hrz_posn * pos, struct lnh_hrz_posn * hpos)
* \brief double horizontal position to human readable horizontal position
* \ingroup conversion
*/
void ln_hrz_to_hhrz (struct ln_hrz_posn * pos, struct lnh_hrz_posn * hpos)
{
	ln_deg_to_dms (pos->alt, &hpos->alt);
	ln_deg_to_dms (pos->az, &hpos->az);
}

/*! \fn const char * ln_hrz_to_nswe (struct ln_hrz_posn * pos);
 * \brief returns direction of given azimut - like N,S,W,E,NSW,...
 * \ingroup conversion
 */ 
const char * ln_hrz_to_nswe (struct ln_hrz_posn * pos)
{
	char * directions[] = {"S", "SSW", "SW", "SWW", "W", "NWW", "NW", "NNW", "N", "NNE", "NE", "NEE", 
		"E", "SEE", "SE", "SSE"};
	return directions[(int)(pos->az / 22.5)];
}
	
/*! \fn void ln_hlnlat_to_lnlat (struct lnh_lnlat_posn * hpos, struct ln_lnlat_posn * pos)
* \brief human readable long/lat position to double long/lat position
* \ingroup conversion
*/
void ln_hlnlat_to_lnlat (struct lnh_lnlat_posn * hpos, struct ln_lnlat_posn * pos)
{
	pos->lng = ln_dms_to_deg (&hpos->lng);
	pos->lat = ln_dms_to_deg (&hpos->lat);
}
	
/*! \fn void ln_lnlat_to_hlnlat (struct ln_lnlat_posn * pos, struct lnh_lnlat_posn * hpos)
* \brief double long/lat position to human readable long/lat position
* \ingroup conversion
*/
void ln_lnlat_to_hlnlat (struct ln_lnlat_posn * pos, struct lnh_lnlat_posn * hpos)
{
	ln_deg_to_dms (pos->lng, &hpos->lng);
	ln_deg_to_dms (pos->lat, &hpos->lat);
}

/*
* \fn double ln_get_rect_distance (struct ln_rect_posn * a, struct ln_rect_posn * b)
* \param a First recatngular coordinate
* \param b Second rectangular coordinate
* \return Distance between a and b.
*
* Calculate the distance between rectangular points a and b.
*/
double ln_get_rect_distance (struct ln_rect_posn * a, struct ln_rect_posn * b)
{
	double x,y,z;

	x = a->X - b->X;
	y = a->Y - b->Y;
	z = a->Z - b->Z;
	
	x *=x;
	y *=y;
	z *=z;
	
	return sqrt (x + y + z);
}

/*
* \fn double ln_get_light_time (double dist)
* \param dist Distance in AU
* \return Distance in light days.
*
* Convert units of AU into light days.
*/
double ln_get_light_time (double dist)
{
	return dist * 0.005775183;
}


/* local types and macros */
typedef int BOOL;
#define TRUE 1
#define FALSE 0
#define iswhite(c)  ((c)== ' ' || (c)=='\t')

/*
[]------------------------------------------------------------------------[]
|  trim() & strip()                                                        |
|                                                                          |
|  strips trailing whitespaces from buf.                                   |
|                                                                          |
[]------------------------------------------------------------------------[]
*/
static char *trim(char *x)
{
    char *y;

    if(!x)
        return(x);
    y = x + strlen(x)-1;
    while (y >= x && isspace(*y)) 
        *y-- = 0; /* skip white space */
    return x;
}


/*
[]------------------------------------------------------------------------[]
|                                                                          |
|   skipwhite()                                                            |
|   salta espacios en blanco                                               |
|                                                                          |
[]------------------------------------------------------------------------[]
*/
static void skipwhite(char **s)
{
   while(iswhite(**s))
        (*s)++;
}


/*! \fn double ln_get_dec_location(char * s)
* \param s Location string
* \return angle in degrees
*
* Obtains Latitude, Longitude, RA or Declination from a string.
*
*  If the last char is N/S doesn't accept more than 90 degrees.            
*  If it is E/W doesn't accept more than 180 degrees.                      
*  If they are hours don't accept more than 24:00                          
*                                                                          
*  Any position can be expressed as follows:                               
*  (please use a 8 bits charset if you want                                
*  to view the degrees separator char '0xba')                              
*
*  42.30.35,53                                                             
*  90º0'0,01 W                                                             
*  42º30'35.53 N                                                           
*  42º30'35.53S                                                            
*  42º30'N                                                                 
*  - 42.30.35.53                                                           
*   42:30:35.53 S                                                          
*  + 42.30.35.53                                                           
*  +42º30 35,53                                                            
*   23h36'45,0                                                             
*                                                                          
*                                                                          
*  42:30:35.53 S = -42º30'35.53"                                           
*  + 42 30.35.53 S the same previous position, the plus (+) sign is        
*  considered like an error, the last 'S' has precedence over the sign     
*                                                                          
*  90º0'0,01 N ERROR: +- 90º0'00.00" latitude limit                        
*
*/
double ln_get_dec_location(char *s)
{
	char *ptr, *dec, *hh, *ame, *tok_ptr;
	BOOL negative = FALSE;
	char delim1[] = " :.,;DdHhMm'\n\t";
	char delim2[] = " NSEWnsew\"\n\t";
	int dghh = 0, minutes = 0;
	double seconds = 0.0, pos;
	short count;
	enum _type{
		HOURS, DEGREES, LAT, LONG
		}type;

	if (s == NULL || !*s)
		return(-0.0);
	
	count = strlen(s) + 1;
	if ((ptr = (char *) alloca(count)) == NULL)
		return (-0.0);
	
	memcpy(ptr, s, count);
	trim(ptr);
	skipwhite(&ptr);
	if (*ptr == '+' || *ptr == '-')
		negative = (char) (*ptr++ == '-' ? TRUE : negative);
	
	/* the last letter has precedence over the sign */
	if (strpbrk(ptr,"SsWw") != NULL) 
		negative = TRUE;
	
	skipwhite(&ptr);
	if ((hh = strpbrk(ptr,"Hh")) != NULL && hh < ptr + 3) {
		type = HOURS;
		if (negative) /* if RA no negative numbers */
			negative = FALSE;
		} else if ((ame = strpbrk(ptr,"SsNn")) != NULL) {
			type = LAT;
			if (ame == ptr) /* the North/South found before data */
				ptr++;
			} else 
				type = DEGREES; /* unspecified, the caller must control it */
		if ((ptr = strtok_r(ptr,delim1, &tok_ptr)) != NULL)
			dghh = atoi (ptr);
		else
			return (-0.0);
		if ((ptr = strtok_r(NULL,delim1, &tok_ptr)) != NULL) {
			minutes = atoi (ptr);
			if (minutes > 59)
				return (-0.0);
		} else
			return (-0.0);
		
		if ((ptr = strtok_r(NULL,delim2,&tok_ptr)) != NULL) {
			if ((dec = strchr(ptr,',')) != NULL)
				*dec = '.';
			seconds = strtod (ptr, NULL);
			if (seconds >= 60)
				return (-0.0);
		}
        
		if ((ptr = strtok(NULL," \n\t")) != NULL) {
			skipwhite(&ptr);
			if (*ptr == 'S' || *ptr == 'W' || *ptr == 's' || *ptr == 'W')
				negative = TRUE;
		}
		
		pos = dghh + minutes /60.0 + seconds / 3600.0;
		if (type == HOURS && pos > 24.0)
			return (-0.0);
		if (type == LAT && pos > 90.0)
			return (-0.0);
		if (negative == TRUE)
			pos = 0.0 - pos;
	return pos;
}


/*! \fn const char * ln_get_humanr_location(double location)    
* \param location Location angle in degress
* \return Angle string
*
* Obtains a human readable location in the form: ddºmm'ss.ss"             
*/
const char * ln_get_humanr_location(double location)
{
    static char buf[16];
    double deg = 0.0;
    double min = 0.0;
    double sec = 0.0;
    *buf = 0;
    sec = 60.0 * (modf(location, &deg));
    if (sec < 0.0)
        sec *= -1;
    sec = 60.0 * (modf(sec, &min));
    sprintf(buf,"%+dº%d'%.2f\"",(int)deg, (int) min, sec);
    return buf;
}

/*! \fn double ln_interpolate3 (double n, double y1, double y2, double y3)
* \return interpolation value
* \param n Interpolation factor
* \param y1 Argument 1
* \param y2 Argument 2
* \param y3 Argument 3
*
* Calculate an intermediate value of the 3 arguments for the given interpolation
* factor.
*/
double ln_interpolate3 (double n, double y1, double y2, double y3)
{
	double y,a,b,c;
	
	/* equ 3.2 */
	a = y2 - y1;
	b = y3 - y2;
	c = b - a;
	
	/* equ 3.3 */
	y = y2 + n / 2.0 * (a + b + n * c);
	
	return y;
}


/*! \fn double ln_interpolate5 (double n, double y1, double y2, double y3, double y4, double y5)
* \return interpolation value
* \param n Interpolation factor
* \param y1 Argument 1
* \param y2 Argument 2
* \param y3 Argument 3
* \param y4 Argument 4
* \param y5 Argument 5
*
* Calculate an intermediate value of the 5 arguments for the given interpolation
* factor.
*/
double ln_interpolate5 (double n, double y1, double y2, double y3, double y4, double y5)
{
	double y,A,B,C,D,E,F,G,H,J,K;
	double n2,n3,n4;
	
	/* equ 3.8 */
	A = y2 - y1;
	B = y3 - y2;
	C = y4 - y3;
	D = y5 - y4;
	E = B - A;
	F = C - B;
	G = D - C;
	H = F - E;
	J = G - F;
	K = J - H;
	
	y = 0.0;
	n2 = n* n;
	n3 = n2 * n;
	n4 = n3 * n;
	
	y += y3;
	y += n * ((B + C ) / 2.0 - (H + J) / 12.0);
	y += n2 * (F / 2.0 - K / 24.0);
	y += n3 * ((H + J) / 12.0);
	y += n4 * (K / 24.0);
	
	return y;
}

/* This section is for Win32 substitutions. */
#ifdef __WIN32__

/* Catches calls to the POSIX gettimeofday and converts them to a related WIN32 version. */
int gettimeofday(struct timeval *tv, struct timezone *tz)
{
	struct _timeb timeptr;

	_ftime_s (&timeptr);

	tv->tv_sec = timeptr.time + timeptr.timezone * 60;
	tv->tv_sec = timeptr.millitm;

	tz->tz_dsttime = timeptr.dstflag;
	tz->tz_dsttime = timeptr.timezone;

	return 0;
}

/* Catches calls to the POSIX gmtime_r and converts them to a related WIN32 version. */
struct tm *gmtime_r (time_t *t, struct tm *gmt)
{
	gmtime_s (gmt, t);

	return gmt;
}

/* Catches calls to the POSIX strtok_r and converts them to a related WIN32 version. */
char *strtok_r(char *str, const char *sep, char **last)
{
	return strtok_s(str, sep, last);
}

#endif /* __WIN32__ */

/* C89 substitutions for C99 functions. */
#ifdef __C89_SUB__

/* Simple cube root */
double cbrt (double x)
{
	return pow (x, 1.0/3.0);
}

/* Not a Number function generator */
double nan (const char *code)
{
	double zero = 0.0;

	return zero/0.0;
}

/* Simple round to nearest */
double round (double x)
{
	double y;

	y = x - floor (x);

	return (y >= 0.5) ? (ceil(x)) : (floor (x));
}

#endif /* __C89_SUB__ */
