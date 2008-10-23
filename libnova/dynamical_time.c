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

#include <libnova/dynamical_time.h>
#include <stdio.h>
#define TERMS 192

struct year_TD
{
    int year;
	double TD;
};

/* Stephenson and Houlden  for years prior to 948 A.D.*/
static double get_dynamical_diff_sh1 (double JD);

/* Stephenson and Houlden  for years between 948 A.D. and 1600 A.D.*/
static double get_dynamical_diff_sh2 (double JD);

/* Table 9.a pg 72 for years 1620..1992.*/
static double get_dynamical_diff_table (double JD);

/* get the dynamical time diff in the near past / future 1992 .. 2010 */
static double get_dynamical_diff_near (double JD);

/* uses equation 9.1 pg 73 to calc JDE for othe JD values */          
static double get_dynamical_diff_other (double JD);


/* dynamical time in seconds for every second year from 1620 to 1992 */
const static double delta_t[TERMS] =
{   124.0, 115.0, 106.0, 98.0, 91.0,
    85.0, 79.0, 74.0, 70.0, 65.0,
    62.0, 58.0, 55.0, 53.0, 50.0,
    48.0, 46.0, 44.0, 42.0, 40.0,
    37.0, 35.0, 33.0, 31.0, 28.0,
    26.0, 24.0, 22.0, 20.0, 18.0,
    16.0, 14.0, 13.0, 12.0, 11.0,
    10.0, 9.0, 9.0, 9.0, 9.0,
    9.0, 9.0, 9.0, 9.0, 10.0,
    10.0, 10.0, 10.0, 10.0, 11.0,
    11.0, 11.0, 11.0, 11.0, 11.0,
    11.0, 12.0, 12.0, 12.0, 12.0,
    12.0, 12.0, 13.0, 13.0, 13.0,
    13.0, 14.0, 14.0, 14.0, 15.0,
    15.0, 15.0, 15.0, 16.0, 16.0,
    16.0, 16.0, 16.0, 17.0, 17.0,
    17.0, 17.0, 17.0, 17.0, 17.0,
    17.0, 16.0, 16.0, 15.0, 14.0,
    13.7, 13.1, 12.7, 12.5, 12.5,
    12.5, 12.5, 12.5, 12.5, 12.3,
    12.0, 11.4, 10.6, 9.6, 8.6,
    7.5, 6.6, 6.0, 5.7, 5.6,
    5.7, 5.9, 6.2, 6.5, 6.8,
    7.1, 7.3, 7.5, 7.7, 7.8,
    7.9, 7.5, 6.4, 5.4, 2.9,
    1.6, -1.0, -2.7, -3.6, -4.7,
    -5.4, -5.2, -5.5, -5.6, -5.8,
    -5.9, -6.2, -6.4, -6.1, -4.7,
    -2.7, 0.0, 2.6, 5.4, 7.7,
    10.5, 13.4, 16.0, 18.2, 20.2,
    21.2, 22.4, 23.5, 23.9, 24.3,
    24.0, 23.9, 23.9, 23.7, 24.0,
    24.3, 25.3, 26.2, 27.3, 28.2,
    29.1, 30.0, 30.7, 31.4, 32.2,
    33.1, 34.0, 35.0, 36.5, 38.3,
    40.2, 42.2, 44.5, 46.5, 48.5,
    50.5, 52.2, 53.8, 54.9, 55.8,
    56.9, 58.3
    };
 					

/* Stephenson and Houlden  for years prior to 948 A.D.*/
static double get_dynamical_diff_sh1 (double JD)
{
    double TD,E;
    
    /* number of centuries from 948 */
    E = (JD - 2067314.5) / 36525.0;
    
    TD = 1830.0 - 405.0 * E + 46.5 * E * E;
    return (TD);
}

/* Stephenson and Houlden for years between 948 A.D. and 1600 A.D.*/
static double get_dynamical_diff_sh2 (double JD)
{
    double TD,t;
    
    /* number of centuries from 1850 */
    t = (JD - 2396758.5) / 36525.0;
    
    TD = 22.5 * t * t;
    return TD;
}

/* Table 9.a pg 72 for years 1600..1992.*/
/* uses interpolation formula 3.3 on pg 25 */
static double get_dynamical_diff_table (double JD)
{
    double TD = 0;
    double a,b,c,n;
    int i;
    
    /* get no days since 1620 and divide by 2 years */
    i = (int)((JD - 2312752.5) / 730.5);
    
    /* get the base interpolation factor in the table */
    if (i > (TERMS - 2))
        i = TERMS - 2;
	
	/* calc a,b,c,n */
	a = delta_t[i+1] - delta_t[i];
	b = delta_t[i+2] - delta_t[i+1];
	c = a - b;
	n = ((JD - (2312752.5 + (730.5 * i))) / 730.5);
	
	TD = delta_t[i+1] + n / 2 * (a + b + n * c);

    return TD;
}

/* get the dynamical time diff in the near past / future 1992 .. 2010 */
/* uses interpolation formula 3.3 on pg 25 */
static double get_dynamical_diff_near (double JD)
{
    double TD = 0;
    /* TD for 1990, 2000, 2010 */
    double delta_T[3] = {56.86, 63.83, 70.0};
    double a,b,c,n;
         
    /* calculate TD by interpolating value */
    a = delta_T[1] - delta_T[0];
    b = delta_T[2] - delta_T[1];
    c = b - a;
    
    /* get number of days since 2000 and divide by 10 years */
	n = (JD - 2451544.5) / 3652.5; 
	TD = delta_T[1] + (n / 2) * (a + b + n * c);
	     
    return TD;
} 

/* uses equation 9.1 pg 73 to calc JDE for othe JD values */          
static double get_dynamical_diff_other (double JD)
{     
    double TD;
    double a;
    
    a = (JD - 2382148);
    a *= a;

    TD = -15 + a / 41048480;
       
    return (TD);
}  

/*! \fn double ln_get_dynamical_time_diff (double JD)
* \param JD Julian Day
* \return TD
*
* Calculates the dynamical time (TD) difference in seconds (delta T) from 
* universal time.
*/
/* Equation 9.1 on pg 73.
*/
double ln_get_dynamical_time_diff (double JD)
{
    double TD;

    /* check when JD is, and use corresponding formula */
    /* check for date < 948 A.D. */
    if ( JD < 2067314.5 )
        /* Stephenson and Houlden */
	    TD = get_dynamical_diff_sh1 (JD);
    else if ( JD >= 2067314.5 && JD < 2305447.5 )
	    /* check for date 948..1600 A.D. Stephenson and Houlden */
    	TD = get_dynamical_diff_sh2 (JD);
	else if ( JD >= 2312752.5 && JD < 2448622.5 )
		/* check for value in table 1620..1992  interpolation of table */
		TD = get_dynamical_diff_table (JD);
	else if ( JD >= 2448622.5 && JD <= 2455197.5 )
		/* check for near future 1992..2010 interpolation */
		TD = get_dynamical_diff_near (JD);       
	else
	    /* other time period outside */
	    TD = get_dynamical_diff_other (JD);   	    
		    
	return TD;
}
      
     
/*! \fn double ln_get_jde (double JD)
* \param JD Julian Day
* \return Julian Ephemeris day 
*     
* Calculates the Julian Ephemeris Day (JDE) from the given julian day
*/     
    
double ln_get_jde (double JD)
{
    double JDE;
    double secs_in_day = 24 * 60 * 60;
    
    JDE = JD +  ln_get_dynamical_time_diff (JD) / secs_in_day;
    
    return JDE;
}
