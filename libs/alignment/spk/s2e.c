#include "spk.h"

int spkS2e ( double xp, double yp, double dut1, double slon,
             double slat, double sh, int mount, double pnogo, double p,
             double tc, double h, double fl, double wl, double pia,
             double pib, double pvd, double pca, double pnp, double paw,
             double pan, double ga, double gb, double x, double y,
             double a, double b, double r, int iy, int im, int id,
             int ih, int mi, double sec, double rc, double dc,
             double pr, double pd, double px, double rv,
             double* adem, double* bdem, double* ademb, double* bdemb,
             double* rdem )
/*
**  - - - - - - -
**   s p k S 2 e
**  - - - - - - -
**
**  One-call star to mount calculation.
**
**  The function performs the end-to-end pointing transformation,
**  starting with an ICRS star catalog entry and predicting the mount
**  encoder demands required to acquire the star image.  It does so in
**  the simplest possible way, by first populating the SPK data
**  structures and then calling spkAstr, spkCtar and spkVtel, with no
**  consideration for computational efficiency.
**
**  Its main purpose is as the core component of the SPK demonstrator
**  POINT, though it could be used in a basic telescope control system
**  where only the pointing/tracking capability of SPK is needed and
**  efficiency is not an important consideration.
**
**  Given:
**
**   Earth orientation parameters (Note 1)
**     xp     double   polar motion x coordinate (radians)
**     yp     double   polar motion y coordinate (radians)
**     dut1   double   Delta UT1: UT1-UTC (seconds)
**
**   Observatory
**     slon   double   site east longitude (ITRF, radians)
**     slat   double   site geodetic latitude (ITRF, radians)
**     sh     double   site altitude above WGS84 ellipsoid (m)
**     mount  int      mount type:  1 = equatorial, else = altazimuth
**     pnogo  double   closest to mount pole allowed (radians)
**
**   Ambient air conditions
**     p      double   pressure (hPa = mB)
**     tc     double   temperature (degrees Celsius)
**     h      double   relative humidity (0-1)
**
**   Optics
**     fl     double   focal length (user units)
**     wl     double   wavelength (micrometers)
**
**   Pointing model coefficients (radians)
**     pia    double   IA: roll (-HA or pi-azimuth) index error
**     pib    double   IB: pitch (Dec or elevation) index error
**     pvd    double   VD: droop
**     pca    double   CA: telescope/pitch nonperpendicularity
**     pnp    double   NP: roll/pitch nonperpendicularity
**     paw    double   AW: roll axis misalignment across meridian
**     pan    double   AN: roll axis misalignment along meridian
**     ga     double   dCA guiding correction
**     gb     double   dIB guiding correction
**
**   Hotspot
**     x      double   x-coordinate in rotating FP (user units)
**     y      double   y-coordinate in rotating FP (user units)
**
**   Mount and rotator angles (achieved)
**     a      double   achieved -HA or pi-Az (radians)
**     b      double   achieved Dec or El (radians)
**     r      double   achieved rotator angle (radians, Note 3)
**
**   UTC
**     iy     int      year CE
**     im     int      month
**     id     int      day
**     ih     int      hour
**     mi     int      minute
**     sec    double   seconds
**
**   Target (Note 2)
**     rc     double   ICRS right ascension at J2000.0 (radians)
**     dc     double   ICRS declination at J2000.0 (radians)
**     pr     double   RA proper motion (radians/year, Note 2)
**     pd     double   Dec proper motion (radians/year)
**     px     double   parallax (arcsec, Note 3)
**     rv     double   radial velocity (km/s, +ve if receding)
**
**  Returned:
**     adem   double*  demand -HA or pi-Az (radians)
**     bdem   double*  demand Dec or El (radians)
**     ademb  double*  demand -HA or pi-Az (radians, Note 4)
**     bdemb  double*  demand Dec or El (radians, Note 4)
**     rdem   double*  demand rotator angle (radians, Note 5)
**
**  Returned (function value):
**            int      0 = OK, else = fail (Note 5)
**
**  Notes:
**
**  1) The Earth Orientation Parameters can be obtained from tabulations
**     provided by the International Earth Rotation and Reference
**     Systems Service.
**
**     xp and yp are the coordinates (in radians) of the Celestial
**     Intermediate Pole with respect to the International Terrestrial
**     Reference System (see IERS Conventions), measured along the
**     meridians 0 and 90 deg west respectively.
**
**     The Delta UT1 changes very slowly except for UTC leap seconds,
**     where it jumps by one second.
**
**  2) Star data for an epoch other than J2000.0 (for example from the
**     Hipparcos catalog, which has an epoch of J1991.25) will require
**     preliminary treatment.
**
**     The target's proper motion in RA is dRA/dt rather than
**     cos(Dec)*dRA/dt.  In catalogs where the RA and Dec proper motions
**     are given in milliarcseconds per year, both will need scaling
**     into radians before the RA value is divided by cos(Dec).
**     n.b. Some catalogs quote proper motions per century, others per
**     year.
**
**     The parallax value is in arcseconds rather than radians as it is
**     a distance measure rather than an angle.
**
**  3) The achieved rotator angle r is zero when the +y axis points
**     toward the projection of the mount's south pole, and increases
**     counterclockwise.
**
**  4) The second set of mount demands, ademb,bdemb, is for the "beyond
**     the pole" solution.
**
**  5) The demand rotator angle rdem is that required to align the
**     +y axis to ICRS north when the mount is in its normal (i.e. not
**     beyond the pole) attitude.
**
**  6) This function does not itself validate the given arguments, but
**     does return the error status if any of the called functions
**     fails.
**
**  7) Although grossly inefficient, it will run at kilohertz rates on
**     any modern processor.
**
**  Called:
**      spkAstr     update astrometry parameters
**      spkCtar     ICRS to astrometric
**      spkVtel     solve a virtual telescope
**
**  This revision:   2024 July 18
**
**  Author P.T.Wallace - see license notice at end.
*/

{
/* VT constituents */
   spkEOP eop;    /* polar motion and UT1-UTC (from IERS) */
   spkOBS obs;    /* site location and mount type */
   spkAIR air;    /* local weather readings */
   spkOPT opt;    /* focal length and color */
   spkPM pm;      /* 7-term pointing model */
   spkPO po;      /* image position in focal plane */
   spkAX3 ax3;    /* achieved roll, pitch, rotator angles */
   spkUTC utc;    /* UTC date and time */
   spkAST ast;    /* target-independent astrometric parameters */
   spkTAR tar;    /* sky target */

/* Miscellaneous */
   int j;
   double tara, tare, tarr, tarp, soln[5];

/*
** ----------------------------
** Populate the VT constituents
** ----------------------------
*/

/* Earth orientation parameters. */
   eop.xp = xp;                              /* polar motion */
   eop.yp = yp;                              /* polar motion */
   eop.dut1 = dut1;                          /* UT1-UTC */

/* Observatory. */
   obs.slon = slon;                          /* east longitude */
   obs.slat = slat;                          /* geodetic latitude */
   obs.sh = sh;                              /* altitude */
   obs.mount = mount == 1 ? EQUAT : ALTAZ;   /* mount type */
   obs.pavoid = pnogo;                       /* pole avoidance */

/* Ambient air conditions. */
   air.p = p;                                /* pressure */
   air.t = tc;                               /* temperature */
   air.h = h;                                /* humidity */

/* Optics. */
   opt.fl = fl;                              /* focal length */
   opt.wl = wl;                              /* wavelength */

/* Pointing model. */
   pm.pia = pia;                             /* -IH or +IA */
   pm.pib = pib;                             /* +ID or +IE */
   pm.pvd = pvd;                             /* +FLOP */
   pm.pca = pca;                             /* -CH or +CA */
   pm.pnp = pnp;                             /* -NP or +NPAE*/
   pm.paw = paw;                             /* -MA or +AW */
   pm.pan = pan;                             /* +ME or +AN*/

   pm.ga = ga;                               /* correction to pca */
   pm.gb = gb;                               /* correction to pib */

/* Hotspot. */
   po.x = x;                                 /* image x */
   po.y = y;                                 /* image y */

/* Mount and rotator angles, achieved. */
   ax3.a = a;                                /* -HA or pi-Az */
   ax3.b = b;                                /* Dec or El */
   ax3.r = r;                                /* rotator angle */

/* UTC date & time. */
   utc.iy = iy;                              /* year (CE) */
   utc.mo = im;                              /* month (1-12) */
   utc.id = id;                              /* day (1-28,29,30,31) */
   utc.ih = ih;                              /* hour */
   utc.mi = mi;                              /* minute */
   utc.sec = sec;                            /* seconds */

/* Astrometry. */
   if ( spkAstr ( 1, utc, 0.0,
                  &eop, &obs, &air, &opt, &ast ) ) return -1;

/* Target. */
   spkCtar ( rc, dc, pr, pd, px, rv, &ast, &tar );

/*
** --------------------------
** Solve for the axis demands
** --------------------------
*/

   j = spkVtel ( AXES, &obs, &opt, &pm, &ast, &ax3, &tar, &po,
                 &tara, &tare, &tarr, &tarp, soln );
   if ( j ) return -1;

   *adem = soln[0];
   *bdem = soln[1];
   *ademb = soln[2];
   *bdemb = soln[3];
   *rdem = soln[4];

/* Success. */
   return 0;
}

/*----------------------------------------------------------------------
**
**  Copyright (C) 2024 by P.T.Wallace
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