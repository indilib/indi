/*
    libastro

    functions used for coordinate conversions, based on libnova

    Copyright (C) 2020 Chris Rowland
    Copyright (C) 2021 Jasem Mutlaq

    This library is free software; you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published
    by the Free Software Foundation; either version 2.1 of the License, or
    (at your option) any later version.

    This library is distributed in the hope that it will be useful, but
    WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
    or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
    License for more details.

    You should have received a copy of the GNU Lesser General Public License
    along with this library; if not, write to the Free Software Foundation,
    Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA

*/

// file libsatro.h

// functions used for coordinate conversions, based on libnova

#pragma once

#include <libnova/utility.h>

namespace INDI
{

#define RAD_TO_DEG(rad) (rad * 180.0/M_PI)
#define DEG_TO_RAD(deg) (deg * M_PI/180.0)

/**
 * \defgroup Position Structures

     Structure for Celestial Equatorial, horizontal, and geographic positions.
 */
/*@{*/

/** \typedef IEquatorialCoordinates
    \brief Celestial Equatorial Coordinates */
typedef struct
{
    double rightascension;  /*!< Right Ascension in Hours (0 to 24)*/
    double declination;     /*!< Delination in degrees (-90 to +90) */
} IEquatorialCoordinates;

/** \typedef IHorizontalCoordinates
    \brief Topocentric Horizontal Coordinates */
typedef struct
{
    double azimuth;  /*!< Azimuth in degrees (0 to 360 eastward. 0 North, 90 East, 180 South, 270 West)*/
    double altitude; /*!< Altitude in degrees (-90 to +90) */
} IHorizontalCoordinates;

/** \typedef IGeographicCoordinates
    \brief Geographic Coordinates */
typedef struct
{
    double longitude; /*!< Longitude in degrees (0 to 360 eastward.)*/
    double latitude;  /*!< Latitude in degrees (-90 to +90) */
    double elevation; /*!< Elevation from Mean Sea Level in meters */
} IGeographicCoordinates;

/*@}*/

/*
* \brief This provides astrometric helper functions
* based on the libnova library
*/

/**
* \brief ObservedToJ2000 converts an observed position to a J2000 catalogue position
*  removes aberration, nutation and precession
* \param observed position
* \param jd Julian day epoch of observed position
* \param J2000pos returns catalogue position
*/
void ObservedToJ2000(IEquatorialCoordinates *observed, double jd, IEquatorialCoordinates *J2000pos);

/**
* \brief J2000toObserved converts a J2000 catalogue position to an observed position for the epoch jd
*    applies precession, nutation and aberration
* \param J2000pos J2000 catalogue position
* \param jd Julian day epoch of observed position
* \param observed returns observed position for the JD epoch
*/
void J2000toObserved(IEquatorialCoordinates *J2000pos, double jd, IEquatorialCoordinates * observed);

/**
 * @brief EquatorialToHorizontal Calculate horizontal coordinates from equatorial coordinates.
 * @param object Equatorial Object Coordinates in INDI standaard (RA Hours, DE degrees).
 * @param observer Observer Location in INDI Standard (Longitude 0 to 360 Increasing Eastward)
 * @param JD Julian Date
 * @param position Calculated Horizontal Coordinates.
 * @note Use this instead of libnova ln_get_hrz_from_equ since it corrects libnova Azimuth (0 = North and not South).
 */
void EquatorialToHorizontal(IEquatorialCoordinates *object, IGeographicCoordinates *observer, double JD,
                            IHorizontalCoordinates *position);

/**
 * @brief HorizontalToEquatorial Calculate Equatorial EOD Coordinates from horizontal coordinates
 * @param object Horizontal Object Coordinates
 * @param observer Observer Location in INDI Standard (Longitude 0 to 360 Increasing Eastward)
 * @param JD Julian Date
 * @param position Calculated Equatorial Coordinates in INDI standards (RA hours, DE degrees).
 * @note Use this instead of libnova ln_get_equ_from_hrz since it corrects libnova Azimuth (0 = North and not South).
 */
void HorizontalToEquatorial(IHorizontalCoordinates *object, IGeographicCoordinates *observer, double JD,
                            IEquatorialCoordinates *position);

/**
* \brief ln_get_equ_nut applies or removes nutation in place for the epoch JD
* \param posn position, nutation is applied or removed in place
* \param jd
* \param reverse  set to true to remove nutation
*/
void ln_get_equ_nut(ln_equ_posn *posn, double jd, bool reverse = false);

}
