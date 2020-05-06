/*
    libastro

    functions used for coordinate conversions, based on libnova
    
    Copyright (C) 2020 Chris Rowland    

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

#include <math.h>

#include <libnova/precession.h>
#include <libnova/aberration.h>
#include <libnova/nutation.h>

// converts the Observed (JNow) position to a J2000 catalogue position by removing
// aberration, nutation and precession using the libnova library
void LibAstro::ObservedToJ2000(ln_equ_posn * observed, double jd, ln_equ_posn * J2000pos)
{
    ln_equ_posn tempPos;
    // remove the aberration
    ln_get_equ_aber(observed, jd, &tempPos);
    // this conversion has added the aberration, we want to subtract it
    tempPos.ra = observed->ra - (tempPos.ra - observed->ra);
    tempPos.dec = observed->dec * 2 - tempPos.dec;


    // remove the nutation
    ln_get_equ_nut(&tempPos, jd, true);

    // precess from now to J2000
    ln_get_equ_prec2(&tempPos, jd, JD2000, J2000pos);
}

///
/// \brief LibAstro::J2000toObserved converts catalogue to observed
/// \param J2000pos catalogue position
/// \param jd julian day for the observed epoch
/// \param observed returns observed position
///
void LibAstro::J2000toObserved(ln_equ_posn *J2000pos, double jd, ln_equ_posn * observed)
{
	ln_equ_posn tempPosn;
	
    // apply precession from J2000 to jd
    ln_get_equ_prec2(J2000pos, JD2000, jd, &tempPosn);

	// apply nutation
    ln_get_equ_nut(&tempPosn, jd, false);

	// apply aberration
    ln_get_equ_aber(&tempPosn, jd, observed);
}

// apply or remove nutation
void LibAstro::ln_get_equ_nut(ln_equ_posn *posn, double jd, bool reverse)
{
    // code lifted from libnova ln_get_equ_nut
    // with the option to add or remove nutation
    struct ln_nutation nut;
    ln_get_nutation (jd, &nut);

    double mean_ra, mean_dec, delta_ra, delta_dec;

    mean_ra = ln_deg_to_rad(posn->ra);
    mean_dec = ln_deg_to_rad(posn->dec);

    // Equ 22.1

    double nut_ecliptic = ln_deg_to_rad(nut.ecliptic + nut.obliquity);
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
