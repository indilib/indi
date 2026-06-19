#include "s2e.c"      /**/

#include "spk.h"      /**/
#include "astr.c"     /**/
#include "ctar.c"     /**/
#include "vtel.c"     /**/

#include <stdio.h>

/*--------------------------------------------------------------------*/

/*
**  - - - - - -
**   P O I N T
**  - - - - - -
**
**  Simplest possible demonstration of SPK.
**
**  The one-call star-to-encoders function spkS2h is called for a given
**  set of cirumstances and predicts the mount encoder demands that will
**  acquire the specified target.
**
**  For diagnostic purposes the user may wish to adapt the present
**  program to generate test results for different circumstances.
**
**  This revision:   2024 July 18
**
**  Author P.T.Wallace - see license notice at end.
*/

int main ()
{

/* Arcseconds and degrees to radians */
   const double AS2R = 4.848136811095359935899141e-6;
   const double D2R = 1.745329251994329576923691e-2;

/* 180 deg in radians */
   const double D180 = 3.141592653589793238462643;

/* Miscellaneous */
   int mount, j, iy, im, id, ih, mi;
   double xp, yp, dut1, slon, slat, sh, pnogo, p, tc, h, fl, wl, pia,
          pib, pvd, pca, pnp, paw, pan, ga, gb, x, y, a, b, r, sec,
          rc, dc, pr, pd, px, rv, adem, bdem, ademb, bdemb, rdem;


/*
** --------------------
** The given conditions
** --------------------
*/
/* Earth orientation parameters. */
   xp = 50.995e-3 * AS2R;            /* polar motion x */
   yp = 376.723e-3 * AS2R;           /* polar motion y */
   dut1 = 155.0675e-3;               /* UT1-UTC */

/* Observatory. */
   if ( iauAf2a ( '-', 5, 41, 54.2, &slon ) ) return -1;    /* long */
   if ( iauAf2a ( '-', 15, 57, 42.8, &slat ) ) return -1;   /* lat */
   sh = 625.0;                       /* altitude */
   mount = EQUAT;                    /* mount type */
//                    mount = ALTAZ;
   pnogo  = 0.5 * D2R;               /* pole avoidance */

/* Ambient air conditions. */
   p = 952.0;                        /* pressure */
   tc = 18.5;                        /* temperature */
   h = 0.83;                         /* humidity */

/* Optics. */
   fl = 3000.0;                      /* focal length */
   wl = 0.55;                        /* wavelength */

/* Pointing model coefficients. */
   pia =   25.000 * AS2R;            /* -IH or +IA */
   pib =   15.000 * AS2R;            /* +ID or +IE */
   pvd =    6.788 * AS2R;            /* +FLOP */
   pca = -110.000 * AS2R;            /* -CH or +CA */
   pnp =    8.000 * AS2R;            /* -NP or +NPAE*/
   paw =  999.900 * AS2R;            /* -MA or +AN */
   pan =  -12.000 * AS2R;            /* +ME or +AW */
   ga = 0.0 * AS2R;                  /* guiding correction to pca */
   gb = 0.0 * AS2R;                  /* guiding correction to pib */

/* Hotspot. */
   x = 10.0;                         /* x coordinate in rotating FP */
   y = 20.0;                         /* y coordinate in rotating FP */

/* Mount and rotator angles (achieved). */
   a =  43.831205 * D2R;             /* -HA or pi-Az (radians) */
   b =  -13.155550 * D2R;            /* Dec or El */
   r = -179.806735 * D2R;            /* rotator angle*/

/* UTC. */
   iy = 2013;                        /* year (CE) */
   im = 4;                           /* month */
   id = 2;                           /* day */
   ih = 23;                          /* hour */
   mi = 15;                          /* minute */
   sec = 43.45;                      /* seconds */

/* Target. */
   if ( iauTf2a ( ' ', 14, 34, 16.81183, &rc ) ) return -1;  /* RA */
   if ( iauAf2a ( '-', 12, 31, 10.3965, &dc ) ) return -1;   /* Dec */
   pr = atan2 ( -354.45e-3 * AS2R, cos(dc) );                /* pmRA */
   pd = 595.35e-3 * AS2R;                                    /* pmDec */
   px = 164.99e-3;                                           /* parx */
   rv = 0.0;                                                 /* RV */

/*
** -----------------
** Report the givens
** -----------------
*/

   printf ( "\nCIRCUMSTANCES\n\n" );
   printf ( "  polar motion x = %+8.3f mas\n", xp*1e3/AS2R );
   printf ( "               y = %+8.3f\n\n", yp*1e3/AS2R );
   printf ( "  UT1-UTC = %+.3f ms\n\n", dut1*1e3 );
   printf ( "  site: longitude = %+.5f deg E\n", slon/D2R );
   printf ( "        latitude  = %+.5f deg\n", slat/D2R );
   printf ( "        altitude  = %.3f m\n\n", sh );
   printf ( "  %s mount\n", mount == EQUAT  ? "equatorial" :
                                              "altazimuth" );
   printf ( "  pole avoidance = %.3f deg\n\n", pnogo/D2R );
   printf ( "  ambient air pressure      = %.2f hPa\n", p );
   printf ( "          temperature       = %+.3f C\n", tc );
   printf ( "          relative humidity = %.1f %%\n\n", h*100.0 );
   printf ( "  focal length = %g user units\n", fl );
   printf ( "  wavelength = %.1f nm\n\n", wl*1e3 );
   printf ( "  pointing model: IA = %+.3f arcsec\n", pia/AS2R );
   printf ( "                  IB = %+.3f\n", pib/AS2R );
   printf ( "                  VD = %+.3f\n", pvd/AS2R );
   printf ( "                  CA = %+.3f\n", pca/AS2R );
   printf ( "                  NP = %+.3f\n", pnp/AS2R );
   printf ( "                  AW = %+.3f\n", paw/AS2R );
   printf ( "                  AN = %+.3f\n\n", pan/AS2R );
   printf ( "  guiding correction to CA = %+.3f arcsec\n", ga/AS2R );
   printf ( "                     to IB = %+.3f arcsec\n\n", gb/AS2R );
   printf ( "  hotspot x = %g user units\n", x );
   printf ( "          y = %g\n\n", y );
   printf ( "  achieved roll  = %+11.6f deg\n", a/D2R );
   printf ( "           pitch = %+11.6f deg\n", b/D2R );
   printf ( "           theta = %+11.6f deg\n\n", r/D2R );
   printf ( "  UTC = %d/%2.2d/%2.2d %d:%2.2d:%06.3f\n\n",
                                              iy, im, id, ih, mi, sec );
   printf ( "  target RA  = %10.6f deg\n", rc/D2R );
   printf ( "         Dec = %+10.6f\n\n",  dc/D2R );
   printf ( "  proper motion in RA  = %+11.7f s/y\n", pr/AS2R/15.0 );
   printf ( "                   Dec = %+10.6f \"/y\n\n", pd/AS2R );
   printf ( "  parallax = %7.3f mas\n", px*1e3 );
   printf ( "  radial velocity = %+.3f km/s\n\n", rv );

/*
** ---------------------------
** Compute the encoder demands
** ---------------------------
*/

   j = spkS2e ( xp, yp, dut1, slon, slat, sh, mount, pnogo,
                p, tc, h, fl, wl, pia, pib, pvd, pca, pnp, paw, pan,
                ga, gb, x, y, a, b, r, iy, im, id, ih, mi, sec,
                rc, dc, pr, pd, px, rv,
                &adem, &bdem, &ademb, &bdemb, &rdem );

/*
** ------------------
** Report the results
** ------------------
*/

   if ( !j ) {
      printf ( "\nRESULTS\n\n" );
      if ( mount == EQUAT ) {
         printf ( "  HA,Dec,rotator demands in degrees:\n\n" );
         printf ( "  H  = %+11.6f  ", -iauAnpm(adem)/D2R );
         printf ( "D  = %11.6f  (normal)\n\n", bdem/D2R );
         printf ( "  Hb = %+11.6f  ", -iauAnpm(ademb)/D2R );
         printf ( "Db = %11.6f  (beyond the pole)\n\n", bdemb/D2R );
         printf ( "  R  = %11.6f  "
                  "(for H,D axis demands and +y north)\n",
                                                     iauAnp(rdem)/D2R );
      } else {
         printf ( "  Az,El,rotator demands in degrees:\n\n" );
         printf ( "  A  = %11.6f   ", iauAnp(D180-adem)/D2R );
         printf ( "E  = %+11.6f  (normal)\n\n", bdem/D2R );
         printf ( "  Ab = %11.6f   ", iauAnp(D180-ademb)/D2R );
         printf ( "Eb = %+11.6f  (beyond the zenith)\n\n", bdemb/D2R );
         printf ( "  R  = %11.6f  "
                  "(for A,E axis demands and +y north)\n",
                                                     iauAnp(rdem)/D2R );
      }

   /* Success. */
      return 0;

   } else {
      printf ( "Call to spkS2e has failed!\n" );
      return 1;
   }
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