#include "spk.h"
#include <sofa.h>

void spkCtar ( double rc, double dc, double pr, double pd, double px,
               double rv, spkAST* ast, spkTAR* tar )
/*
**  - - - - - - - -
**   s p k C t a r
**  - - - - - - - -
**
**  Transform an epoch J2000.0 ICRS star catalog entry into an ICRS
**  astrometric target.
**
**  Given:
**     rc     double   ICRS right ascension at J2000.0 (radians, Note 1)
**     dc     double   ICRS declination at J2000.0 (radians, Note 1)
**     pr     double   RA proper motion (radians/year, Note 2)
**     pd     double   Dec proper motion (radians/year)
**     px     double   parallax (arcsec, Note 3)
**     rv     double   radial velocity (km/s, +ve if receding)
**     ast    spkAST*  astrometry parameters
**
**  Returned:
**     tar    spkTAR*  target, ICRS astrometric
**
**  Notes:
**
**  1) Star data for an epoch other than J2000.0 (for example from the
**     Hipparcos catalog, which has an epoch of J1991.25) will require
**     preliminary treatment.
**
**  2) The proper motion in RA is dRA/dt rather than cos(Dec)*dRA/dt.
**     In catalogs where the RA and Dec proper motions are given in
**     milliarcseconds per year, both will need scaling into radians
**     before the RA value is divided by cos(Dec).  n.b. Some catalogs
**     quote proper motions per century rather than per year.
**
**  3) The parallax value is in arcseconds rather than radians as it is
**     a distance measure rather than an angle.
**
**  Called:
**     iauPmpx      proper motion and parallax
**     iauC2s       p-vector to spherical
**
**  This revision:   2021 September 3
**
**  Author P.T.Wallace - see license notice at end.
*/
{
   double p[3];


/* Insert coordinate system into target structure. */
   tar->sys = ICRS;

/* Proper motion and parallax, giving BCRS coordinate direction. */
   iauPmpx ( rc, dc, pr, pd, px, rv, ast->astrom.pmt, ast->astrom.eb,
             p );

/* Insert astrometric RA,Dec into target structure. */
   iauC2s ( p, &tar->a, &tar->b );

}

/*----------------------------------------------------------------------
**
**  Copyright (C) 2021 by P.T.Wallace
**
**  Permission to use, copy, modify, and/or distribute this software for
**  any purpose with or without fee is hereby granted.
**
**  THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
**  WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
**  WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE
**  AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR
**  CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
**  OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
**  NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
**  CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
**
**--------------------------------------------------------------------*/
