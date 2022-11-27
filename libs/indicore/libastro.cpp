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

// file libastro.c
//
// holds extensions to the libnova library to provide useful functions for INDI
//
// Chris Rowland April 2020
//


#include "libastro.h"
#include "indicom.h"

#include <math.h>

#include <libnova/precession.h>
#include <libnova/aberration.h>
#include <libnova/transform.h>
#include <libnova/nutation.h>

namespace INDI
{

//////////////////////////////////////////////////////////////////////////////////////////////
// converts the Observed (JNow) position to a J2000 catalogue position by removing
// aberration, nutation and precession using the libnova library
//////////////////////////////////////////////////////////////////////////////////////////////
void ObservedToJ2000(IEquatorialCoordinates * observed, double jd, IEquatorialCoordinates * J2000pos)
{
    ln_equ_posn tempPos;
    // RA Hours --> Degrees
    struct ln_equ_posn libnova_observed = {observed->rightascension * 15.0, observed->declination};
    // remove the aberration
    ln_get_equ_aber(&libnova_observed, jd, &tempPos);
    // this conversion has added the aberration, we want to subtract it
    tempPos.ra = libnova_observed.ra - (tempPos.ra - libnova_observed.ra);
    tempPos.dec = libnova_observed.dec * 2 - tempPos.dec;

    // remove the nutation
    ln_get_equ_nut(&tempPos, jd, true);

    struct ln_equ_posn libnova_J2000Pos;
    // precess from now to J2000
    ln_get_equ_prec2(&tempPos, jd, JD2000, &libnova_J2000Pos);

    J2000pos->rightascension = libnova_J2000Pos.ra / 15.0;
    J2000pos->declination = libnova_J2000Pos.dec;


}

//////////////////////////////////////////////////////////////////////////////////////////////
/// \brief *J2000toObserved converts catalogue to observed
/// \param J2000pos catalogue position
/// \param jd julian day for the observed epoch
/// \param observed returns observed position
//////////////////////////////////////////////////////////////////////////////////////////////
void J2000toObserved(IEquatorialCoordinates *J2000pos, double jd, IEquatorialCoordinates *observed)
{
    ln_equ_posn tempPosn;
    struct ln_equ_posn libnova_J2000Pos = {J2000pos->rightascension * 15.0, J2000pos->declination };

    // apply precession from J2000 to jd
    ln_get_equ_prec2(&libnova_J2000Pos, JD2000, jd, &tempPosn);

    // apply nutation
    ln_get_equ_nut(&tempPosn, jd, false);

    struct ln_equ_posn libnova_observed;
    // apply aberration
    ln_get_equ_aber(&tempPosn, jd, &libnova_observed);

    observed->rightascension = libnova_observed.ra / 15.0;
    observed->declination = libnova_observed.dec;
}

//////////////////////////////////////////////////////////////////////////////////////////////
/// apply or remove nutation
//////////////////////////////////////////////////////////////////////////////////////////////
void ln_get_equ_nut(ln_equ_posn *posn, double jd, bool reverse)
{
    // code lifted from libnova ln_get_equ_nut
    // with the option to add or remove nutation
    struct ln_nutation nut;
    ln_get_nutation (jd, &nut);

    double mean_ra, mean_dec, delta_ra, delta_dec;

    mean_ra = DEG_TO_RAD(posn->ra);
    mean_dec = DEG_TO_RAD(posn->dec);

    // Equ 22.1

    double nut_ecliptic = DEG_TO_RAD(nut.ecliptic + nut.obliquity);
    double sin_ecliptic = sin(nut_ecliptic);

    double sin_ra = sin(mean_ra);
    double cos_ra = cos(mean_ra);

    double tan_dec = tan(mean_dec);

    delta_ra = (cos (nut_ecliptic) + sin_ecliptic * sin_ra * tan_dec) * nut.longitude - cos_ra * tan_dec * nut.obliquity;
    delta_dec = (sin_ecliptic * cos_ra) * nut.longitude + sin_ra * nut.obliquity;

    // the sign changed to remove nutation
    if (reverse)
    {
        delta_ra = -delta_ra;
        delta_dec = -delta_dec;
    }
    posn->ra += delta_ra;
    posn->dec += delta_dec;
}

//////////////////////////////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////////////////////////////
void EquatorialToHorizontal(IEquatorialCoordinates *object, IGeographicCoordinates *observer, double JD,
                            IHorizontalCoordinates *position)
{
    // Convert from INDI standard location to libnova standard location
    struct ln_lnlat_posn libnova_location = {observer->longitude > 180 ? observer->longitude - 360 : observer->longitude, observer->latitude};
    // RA Hours --> Degrees
    struct ln_equ_posn libnova_object = {object->rightascension * 15.0, object->declination};
    struct ln_hrz_posn horizontalPos;
    ln_get_hrz_from_equ(&libnova_object, &libnova_location, JD, &horizontalPos);
    position->azimuth = range360(180 + horizontalPos.az);
    position->altitude = horizontalPos.alt;
}

//////////////////////////////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////////////////////////////
void HorizontalToEquatorial(IHorizontalCoordinates *object, IGeographicCoordinates *observer, double JD,
                            IEquatorialCoordinates *position)
{
    // Convert from INDI standard location to libnova standard location
    struct ln_lnlat_posn libnova_location = {observer->longitude > 180 ? observer->longitude - 360 : observer->longitude, observer->latitude};
    // Convert from INDI standard location to libnova standard location
    struct ln_hrz_posn libnova_object = {range360(object->azimuth + 180), object->altitude};
    struct ln_equ_posn equatorialPos;
    ln_get_equ_from_hrz(&libnova_object, &libnova_location, JD, &equatorialPos);
    // Degrees --> Hours
    position->rightascension = equatorialPos.ra / 15.0;
    position->declination = equatorialPos.dec;

}


}
