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
 
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <libnova/julian_day.h>

/* Standard Win32 apps do not have POSIX support. */
#ifndef __WIN32__
#include <sys/time.h>
#endif

/* should be in math.h, but isn't on FC3 even with _GNU_SOURCE */
double round (double __x); 

/*! \fn double ln_get_julian_day (struct ln_date * date)
* \param date Date required.
* \return Julian day
*
* Calculate the julian day from a calendar day. 
* Valid for positive and negative years but not for negative JD.
*/
/* Formula 7.1 on pg 61 
*/
double ln_get_julian_day (struct ln_date * date)
{
    double JD;
    double days;
    int a,b;
	struct ln_date local_date;
		
	/* create local copy */
    memcpy (&local_date, date, sizeof (struct ln_date));
		
    /* check for month = January or February */
    if (local_date.months < 3 ) {
        local_date.years--;
	    local_date.months += 12;
	}
	
	a = local_date.years / 100;
	
	/* check for Julian or Gregorian calendar (starts Oct 4th 1582) */
	if (local_date.years > 1582 || 
		(local_date.years == 1582 && 
		(local_date.months > 10 || 
		(local_date.months == 10 && local_date.days >= 4)))) {
	    /* Gregorian calendar */    
	    b = 2 - a + (a / 4);
	} else {
	    /* Julian calendar */
	    b = 0;
	}
	
	/* add a fraction of hours, minutes and secs to days*/
	days = local_date.days + (double)(local_date.hours / 24.0) + (double)(local_date.minutes / 1440.0) + (double)(local_date.seconds /  86400.0);

	/* now get the JD */
	JD = (int)(365.25 * (local_date.years + 4716)) + 
	    (int)(30.6001 * (local_date.months + 1)) + days + b - 1524.5;
	
	return JD;
}


/*! \fn unsigned int ln_get_day_of_week (struct ln_date *date)
* \param date Date required
* \return Day of the week
*
* Calculate the day of the week. 
* Returns 0 = Sunday .. 6 = Saturday
*/
unsigned int ln_get_day_of_week (struct ln_date *date)
{
    unsigned int day;
    double JD;
    
    /* get julian day */
    JD = ln_get_julian_day (date);
    JD += 1.5;
    day = (int)JD % 7; 
    
    return day;
}	

/*! \fn void ln_get_date (double JD, struct ln_date * date)
* \param JD Julian day
* \param date Pointer to new calendar date.
*
* Calculate the date from the Julian day  
*/
void ln_get_date (double JD, struct ln_date * date)
{
   int A,a,B,C,D,E;
   double F,Z;
   
   JD += 0.5;
   Z = (int) JD;
   F = JD - Z;
   
   if (Z < 2299161)
       A = (int) Z;
   else {
       a = (int) ((Z - 1867216.25) / 36524.25);
       A = (int) (Z + 1 + a - (int)(a / 4));
   }
   
   B = A + 1524;
   C = (int) ((B - 122.1) / 365.25);
   D = (int) (365.25 * C);
   E = (int) ((B - D) / 30.6001);
   
   /* get the hms */
   date->hours = (int) (F * 24);
   F -= (double)date->hours / 24;
   date->minutes = (int) (F * 1440);
   F -= (double)date->minutes / 1440;
   date->seconds = F * 86400;
   
   /* get the day */
   date->days = B - D - (int)(30.6001 * E);
   
   /* get the month */
   if (E < 14)
       date->months = E - 1;
   else
       date->months = E - 13;
   
   /* get the year */
   if (date->months > 2)
       date->years = C - 4716;
   else
       date->years = C - 4715;
}	

/*! \fn void ln_get_date_from_timet (time_t * t, struct ln_date * date)
* \param t system time
* \param date Pointer to new calendar date.
*
* Set date from system time
*/
#ifndef __WIN32__
void ln_get_date_from_timet (time_t * t, struct ln_date * date)
{
	struct tm gmt;

	/* convert to UTC time representation */
	gmtime_r (t, &gmt);
    	
	ln_get_date_from_tm (&gmt, date);
}
#endif

/*! \fn void ln_get_date_from_tm (struct tm * t, struct ln_date * date)
* \param tm system tm structure
* \param date Pointer to new calendar date.
*
* Set date from system tm structure
*/
void ln_get_date_from_tm (struct tm * t, struct ln_date * date)
{
	/* fill in date struct */
	date->seconds = t->tm_sec;
	date->minutes = t->tm_min;
	date->hours = t->tm_hour;
	date->days = t->tm_mday;
	date->months = t->tm_mon + 1;
	date->years = t->tm_year + 1900;
}

/*! \fn void ln_get_date_from_sys (struct ln_date * date)
* \param date Pointer to store date.
*
* Calculate local date from system date.
*/
void ln_get_date_from_sys (struct ln_date * date)
{
	struct tm * gmt;
#ifndef __WIN32__
        struct timeval tv;
        struct timezone tz;
#else
	time_t now;
#endif 
		
#ifndef __WIN32__
	/* get current time with microseconds precission*/
	gettimeofday (&tv, &tz);

	/* convert to UTC time representation */
	gmt = gmtime(&tv.tv_sec);
#else
	now = time (NULL);
	gmtime (&gmt, &now);
#endif
    	
	/* fill in date struct */
#ifndef __WIN32__
	date->seconds = gmt->tm_sec + ((double)tv.tv_usec / 1000000);
#else
	date->seconds = gmt->tm_sec;
#endif
	date->minutes = gmt->tm_min;
	date->hours = gmt->tm_hour;
	date->days = gmt->tm_mday;
	date->months = gmt->tm_mon + 1;
	date->years = gmt->tm_year + 1900;
}


/*! \fn double ln_get_julian_from_timet (time_t * in_time)
* \param time The time_t.
* \return Julian day.
*
* Calculate Julian day from time_t.
*/
#ifndef __WIN32__
double ln_get_julian_from_timet (time_t * in_time)
{
	// 1.1.1970 = JD 2440587.5
	return (double)(2440587.5 + (double)(*in_time / (double) 86400.0));
}
#endif

/*! \fn void ln_get_timet_from_julian (double JD, time_t * in_time)
* \param JD Julian day
* \param in_time Pointer to store time_t
*
* Calculate time_t from julian day
*/
#ifndef __WIN32__
void ln_get_timet_from_julian (double JD, time_t * in_time)
{	
	*in_time = (time_t)round((JD - (double) 2440587.5) * (double) 86400.0);
}
#endif

/*! \fn double ln_get_julian_from_sys()
* \return Julian day (UT)
*
* Calculate the julian day (UT) from the local system time
*/
double ln_get_julian_from_sys()
{
	double JD;
	struct ln_date date;
		
	/* get sys date */
	ln_get_date_from_sys (&date);
	JD = ln_get_julian_day (&date);

	return JD;
}

/*! \fn double ln_get_julian_local_date(struct ln_zonedate* zonedate)
* \param zonedate Local date
* \return Julian day (UT)
*
* Calculate Julian day (UT) from zone date
*/
double ln_get_julian_local_date(struct ln_zonedate* zonedate)
{
	struct ln_date date;
	
	ln_zonedate_to_date (zonedate, &date);

	return ln_get_julian_day (&date);
}

/*! \fn void ln_get_local_date (double JD, struct ln_zonedate * zonedate)
* \param JD Julian day
* \param zonedate Pointer to new calendar date.
*
* Calculate the zone date from the Julian day (UT). Get zone info from 
* system using either _timezone or tm_gmtoff fields.
*/
void ln_get_local_date (double JD, struct ln_zonedate * zonedate)
{
	struct ln_date date;
#ifndef __WIN32__
	time_t curtime;
	struct tm *loctime;
#endif
	long gmtoff;

	ln_get_date (JD, &date);

	/* add day light savings time and hour angle */
#ifdef __WIN32__
 	_tzset();
 	gmtoff = _timezone;
 	if (_daylight)
 		gmtoff += 3600;
#else
#ifdef _BSD_SOURCE
 	curtime = time (NULL);
 	loctime = localtime(&curtime);
 	gmtoff = loctime->tm_gmtoff;
	// otherwise there is no reasonable way how to get that:(
	// tm_gmtoff already included DST
#endif
#endif
	ln_date_to_zonedate (&date, zonedate, gmtoff);
}

/*! \fn int ln_get_date_from_mpc (struct ln_date* date, char* mpc_date)
* \param date Pointer to new calendar date.
* \param mpc_date Pointer to string MPC date
* \return 0 for valid date
*
* Calculate the local date from the a MPC packed date.
* See http://cfa-www.harvard.edu/iau/info/PackedDates.html for info.
*/
int ln_get_date_from_mpc (struct ln_date* date, char* mpc_date)
{
	char year[3];
	char month[2];
	char day[2];
	
	/* is mpc_date correct length */
	if (strlen(mpc_date) !=5)
		return -1;
	
	/* get the century */
	switch (*mpc_date) {
		case 'I':
			date->years = 1800;
		break;
		case 'J':
			date->years = 1900;
		break;
		case 'K':
			date->years = 2000;
		break;
		default:
			return -1;
	}
	
	/* get the year */
	year[0] = *(mpc_date + 1);
	year[1] = *(mpc_date + 2);
	year[2] = 0;
	date->years += strtol (year,0,10);
	
	/* month */
	month[0] = *(mpc_date + 3);
	month[1] = 0;
	date->months = strtol (month, 0, 16);
	
	/* day */
	day[0] = *(mpc_date + 4);
	day[1] = 0;
	date->days = strtol (day, 0, 31);
	
	/* reset hours,min,secs to 0 */
	date->hours = 0;
	date->minutes = 0;
	date->seconds = 0;
	return 0;
}

/*! \fn double ln_get_julian_from_mpc (char* mpc_date)
* \param mpc_date Pointer to string MPC date
* \return Julian day.
*
* Calculate the julian day from the a MPC packed date.
* See http://cfa-www.harvard.edu/iau/info/PackedDates.html for info.
*/
double ln_get_julian_from_mpc (char* mpc_date)
{
	struct ln_date date;
	double JD;
	
	ln_get_date_from_mpc(&date, mpc_date);	
	JD = ln_get_julian_day(&date);
	
	return JD;
}

/*! \fn void ln_date_to_zonedate (struct ln_date * date, struct ln_zonedate * zonedate, long gmtoff)
* \param zonedate Ptr to zonedate
* \param gmtoff Offset in seconds from UT
* \param date Ptr to date
*
* Converts a ln_date (UT) to a ln_zonedate (local time). 
*/
void ln_date_to_zonedate (struct ln_date * date, struct ln_zonedate * zonedate, long gmtoff)
{
	double jd;
	struct ln_date dat;

	jd = ln_get_julian_day (date);
	jd += gmtoff / 86400.0;
	ln_get_date (jd, &dat);

	zonedate->years   = dat.years;
	zonedate->months  = dat.months;
	zonedate->days    = dat.days;
	zonedate->hours   = dat.hours;
	zonedate->minutes = dat.minutes;
	zonedate->seconds = dat.seconds;

	zonedate->gmtoff  = gmtoff;
}

/*! \fn void ln_zonedate_to_date (struct ln_zonedate * zonedate, struct ln_date * date)
* \param zonedate Ptr to zonedate
* \param date Ptr to date
*
* Converts a ln_zonedate (local time) to a ln_date (UT). 
*/
void ln_zonedate_to_date (struct ln_zonedate * zonedate, struct ln_date * date)
{
	double jd;
	struct ln_date dat;

	dat.years   = zonedate->years;
	dat.months  = zonedate->months;
	dat.days    = zonedate->days;
	dat.hours   = zonedate->hours;
	dat.minutes = zonedate->minutes;
	dat.seconds = zonedate->seconds;

	jd = ln_get_julian_day (&dat);
	jd -= zonedate->gmtoff / 86400.0;
	ln_get_date (jd, date);
}
