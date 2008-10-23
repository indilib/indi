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
#include <libnova/sidereal_time.h>
#include <libnova/nutation.h>
#include <libnova/utility.h>

/*! \fn double ln_get_mean_sidereal_time (double JD)
* \param JD Julian Day
* \return Mean sidereal time.
*
* Calculate the mean sidereal time at the meridian of Greenwich of a given date.
*/
/* Formula 11.1, 11.4 pg 83 
*/

double ln_get_mean_sidereal_time (double JD)
{
    long double sidereal;
    long double T;
    
    T = (JD - 2451545.0) / 36525.0;
        
    /* calc mean angle */
    sidereal = 280.46061837 + (360.98564736629 * (JD - 2451545.0)) + (0.000387933 * T * T) - (T * T * T / 38710000.0);
    
    /* add a convenient multiple of 360 degrees */
    sidereal = ln_range_degrees (sidereal);
    
    /* change to hours */
    sidereal *= 24.0 / 360.0;
        
    return sidereal;
} 

/*! \fn double ln_get_apparent_sidereal_time (double JD)
* \param JD Julian Day
* /return Apparent sidereal time (hours).
*
* Calculate the apparent sidereal time at the meridian of Greenwich of a given date. 
*/
/* Formula 11.1, 11.4 pg 83 
*/

double ln_get_apparent_sidereal_time (double JD)
{
   double correction, hours, sidereal;
   struct ln_nutation nutation;  
   
   /* get the mean sidereal time */
   sidereal = ln_get_mean_sidereal_time (JD);
        
   /* add corrections for nutation in longitude and for the true obliquity of 
   the ecliptic */   
   ln_get_nutation (JD, &nutation); 
    
   correction = (nutation.longitude / 15.0 * cos (ln_deg_to_rad(nutation.obliquity)));
  
   /* value is in degrees so change it to hours and add to mean sidereal time */
   hours = (24.0 / 360.0) * correction;

   sidereal += hours;
   
   return sidereal;
}
