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

// file libsatro.h

// functions used for coordinate conversions, based on libnova

#include <libnova/utility.h>

///
/// \brief The LibAstro class, this provides astrometric helper functions
/// based on the libnova library
///
class LibAstro
{
public:
	
    ///
    /// \brief ObservedToJ2000 converts an observed position to a J2000 catalogue position
    ///  removes aberration, nutation and precession
    /// \param observed position
    /// \param jd Julian day epoch of observed position
    /// \param J2000pos returns catalogue position
    ///
    static void ObservedToJ2000(ln_equ_posn * observed, double jd, ln_equ_posn * J2000pos);
		
    ///
    /// \brief J2000toObserved converts a J2000 catalogue position to an observed position for the epoch jd
    ///    applies precession, nutation and aberration
    /// \param J2000pos J2000 catalogue position
    /// \param jd Julian day epoch of observed position
    /// \param observed returns observed position for the JD epoch
    ///
    static void J2000toObserved(ln_equ_posn *J2000pos, double jd, ln_equ_posn * observed);
	
protected:
	
    ///
    /// \brief ln_get_equ_nut applies or removes nutation in place for the epoch JD
    /// \param posn position, nutation is applied or removed in place
    /// \param jd
    /// \param reverse  set to true to remove nutation
    ///
    static void ln_get_equ_nut(ln_equ_posn *posn, double jd, bool reverse = false);
};
