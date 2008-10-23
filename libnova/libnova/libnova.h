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

#define LIBNOVA_VERSION "0.12.2"

/*! \mainpage libnova
* \image html libnova-logo.jpg
* Celestial Mechanics, Astrometry and Astrodynamics Library
* 
* \section intro Introduction
* libnova is a general purpose, double precision, Celestial Mechanics, Astrometry and Astrodynamics library.
*
* The intended audience of libnova is C / C++ programmers, astronomers and anyone else interested in calculating positions of astronomical objects or celestial mechanics.
* libnova is the calculation engine used by the <A href="http://nova.sf.net">Nova</A> project and most importantly, is free software.
*
* \section features Features
* The current version of libnova can calculate:
*
* - Aberration
* - Nutation
* - Apparent Position
* - Dynamical Time
* - Julian Day
* - Precession
* - Proper Motion
* - Sidereal Time
* - Solar Coordinates (using VSOP87)
* - Coordinate Transformations
* - Planetary Positions Mercury - Pluto (Mercury - Neptune using VSOP87)
* - Planetary Magnitude, illuminated disk and phase angle.
* - Lunar Position (using ELP82), phase angle.
* - Elliptic Motion of bodies (Asteroid + Comet positional and orbit data)
* - Asteroid + Comet magnitudes
* - Parabolic Motion of bodies (Comet positional data)
* - Orbit velocities and lengths
* - Atmospheric refraction
* - Rise, Set and Transit times.
* - Semidiameters of the Sun, Moon, Planets and asteroids.
* - Angular separation of bodies
* - Hyperbolic motion of bodies
*
* \section docs Documentation
* API documentation for libnova is included in the source. It can also be found in this website and an offline tarball is available <A href="http://libnova.sf.net/libnovadocs.tar.gz">here</A>.
*
* \section download Download
* The latest released version of libnova is 0.11.
* It is available for download <A href="http://sf.net/project/showfiles.php?group_id=57697">here.</A>
*
* \section cvs CVS
* The latest CVS version of libnova is available via CVS <A href="http://sf.net/cvs/?group_id=57697">here.</A>
*
* \section licence Licence
* libnova is released under the <A href="http://www.gnu.org">GNU</A> LGPL.
*
* \section help Help
* If you are interested in helping in the future development of libnova, then please get in touch.
* Currently, we are needing help in the folowing areas.
* - Documentation. (Not just API reference, but also astronomy info for novice users)
* - Programming (in C) astronomical solutions or algorithms.
* - Algorithms and Solutions.
*
* \section authors Authors
* libnova is maintained by <A href="mailto:liam@gnova.org">Liam Girdwood</A> and <A href="mailto:petr@kubanek.net">Petr Kubanek</A>.
*
* \section thanks Thanks
* Thanks to Jean Meeus for most of the algorithms used in this library.
* From his book "Astronomical Algorithms".
* 
* Thanks to Michelle Chapront-Touze and Jean Chapront for publishing
* the Lunar Solution ELP 2000-82B.
*
* Thanks to Messrs. Bretagnon and Francou for publishing planetary 
* solution VSOP87.
*
* Thanks to everyone who has submitted patches. NOTE: I sufferend a disk failure 
* this year and lost a windows patch. I would be grateful if the author could
* resubmit it.
*
* Also thanks to Sourceforge for hosting this project. <A href="http://sourceforge.net"> <IMG src="http://sourceforge.net/sflogo.php?group_id=57697&amp;type=5" width="210" height="62" border="0" alt="SourceForge Logo"></A> 
*/

#ifndef _LN_LIBNOVA_H
#define _LN_LIBNOVA_H

#include <libnova/ln_types.h>
#include <libnova/julian_day.h>
#include <libnova/dynamical_time.h>
#include <libnova/sidereal_time.h>
#include <libnova/transform.h>
#include <libnova/nutation.h>
#include <libnova/aberration.h>
#include <libnova/apparent_position.h>
#include <libnova/solar.h>
#include <libnova/precession.h>
#include <libnova/proper_motion.h>
#include <libnova/mercury.h>
#include <libnova/venus.h>
#include <libnova/earth.h>
#include <libnova/mars.h>
#include <libnova/jupiter.h>
#include <libnova/saturn.h>
#include <libnova/uranus.h>
#include <libnova/neptune.h>
#include <libnova/pluto.h>
#include <libnova/vsop87.h>
#include <libnova/lunar.h>
#include <libnova/elliptic_motion.h>
#include <libnova/asteroid.h>
#include <libnova/comet.h>
#include <libnova/parabolic_motion.h>
#include <libnova/refraction.h>
#include <libnova/rise_set.h>
#include <libnova/angular_separation.h>
#include <libnova/ln_types.h>
#include <libnova/utility.h>
#include <libnova/hyperbolic_motion.h>
#include <libnova/parallax.h>
#include <libnova/airmass.h>

#endif
