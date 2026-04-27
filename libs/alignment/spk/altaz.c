#include "astr.c"     /**/
#include "ctar.c"     /**/
#include "vtel.c"     /**/

/*--------------------------------------------------------------------*/

#include "spk.h"
#include <sofa.h>
#include <stdio.h>

/*
**  - - - - - -
**   A L T A Z
**  - - - - - -
**
**  Simple pointing kernel demonstrator for an altazimuth mount.
**
**  This revision:   2024 July 12
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

   spkTAR tar2;   /* another target */
   spkPO po2;     /* another hotspot */

/* Names of sky coordinate systems */
   static char *csys[] = {
      "illegal",
      "ICRS",
      "apparent",
      "altaz"
   };

/* Miscellaneous */
   char s;
   int i, j, i4[4], it;
   double rc, dc, pr, pd, px, rv, ypa, soln[5], tara, tare, tarr, tarp,
          dt, a, aold, bold, rold, da, db, dr;


/*
** --------------------------------------------
** Initialize a starting set of VT constituents
** --------------------------------------------
**
** Some of these will be used throughout the application, in some cases
** unchanged and in others periodically updated, while there will be
** other instances - different hotspots say - that are selected when
** needed.  The starting set is enough to get the system running and
** tracking a star.
*/

/* Earth orientation parameters. */
   eop.xp = 50.995e-3 * AS2R;   /* polar motion */
   eop.yp = 376.723e-3 * AS2R;  /* polar motion */
   eop.dut1 = 155.0675e-3;      /* UT1-UTC */

/* Site. */
   if ( iauAf2a ( '-', 5, 41, 54.2,           /* east longitude */
                  &obs.slon ) ) return -1;
   if ( iauAf2a ( '-', 15, 57, 42.8,          /* geodetic latitude */
                  &obs.slat ) ) return -1;
   obs.sh = 625.0;                            /* altitude */
   obs.mount = ALTAZ;                         /* mount type */
   obs.pavoid = 0.5 * D2R;                    /* zenith avoidance */

/* Weather. */
   air.p = 952.0;   /* pressure (hPA=mB) */
   air.t = 18.5;    /* temperature (Celsius) */
   air.h = 0.83;    /* humidity */

/* Optics. */
   opt.fl = 3000.0;  /* focal length (mm) */
   opt.wl = 0.55;    /* wavelength (micrometers) */

/* Pointing model. */
   pm.pia =   25.0 * AS2R;  /* +IA */
   pm.pib =   15.0 * AS2R;  /* +IE */
   pm.pvd =    0.0 * AS2R;  /* +FLOP */
   pm.pca = -110.0 * AS2R;  /* +CA */
   pm.pnp =    8.0 * AS2R;  /* +NPAE */
   pm.paw =  999.9 * AS2R;  /* +AW */
   pm.pan =  -12.0 * AS2R;  /* +AN */

   pm.ga = 0.0 * AS2R;   /* guiding correction to pm.pca */
   pm.gb = 0.0 * AS2R;   /* guiding correction to pm.pib */

/* The three axes. */
   ax3.a = 0.0 * D2R;   /* achieved azimuth (south zero right-handed) */
   ax3.b = 0.0 * D2R;   /* achieved elevation */
   ax3.r = 30.0 * D2R;  /* achieved rotator angle */

/* Hotspot (i.e. pointing origin). */
   po.x = 10.0;
   po.y = 20.0;

/* UTC date & time. */
   utc.iy = 2013;    /* year (CE) */
   utc.mo = 4;       /* month (1-12) */
   utc.id = 2;       /* day (1-28,29,30,31) */
   utc.ih = 23;      /* hour */
   utc.mi = 15;      /* minute */
   utc.sec = 43.55;  /* seconds (0-59.999... or 0-60.999...) */

/* Astrometry. */
   if ( spkAstr ( 1, utc, 0.0,
                  &eop, &obs, &air, &opt, &ast ) ) return -1;

/* Target (in this case from a star catalog entry). */
   if ( iauTf2a ( ' ', 14, 34, 16.81183, &rc ) ) return -1;
   if ( iauAf2a ( '-', 12, 31, 10.3965, &dc ) ) return -1;
   pr = atan2 ( -354.45e-3 * AS2R, cos(dc) );
   pd = 595.35e-3 * AS2R;
   px = 164.99e-3;
   rv = 0.0;
   spkCtar ( rc, dc, pr, pd, px, rv, &ast, &tar );

/* Required position angle of +y axis. */
   ypa = 30.0 * D2R;

/*
** --------------------
** Demonstrate tracking
** --------------------
**
** From one second ago up to and including now, at intervals of
** 0.05 seconds, compute three-axis angle demands and velocities.
**
** The seconds are strictly UT1, which in this application can be
** regarded as the same as SI seconds.
**
** It is assumed that the astrometry parameters have been computed for
** "now" using spkAstr with jfull = TRUE, and that the track is relative
** to this time and involves only jfull = FALSE calls to spkAstr.  In
** some implementations occasional use of the jfull = TRUE option during
** tracking may be best.
**
** Updating the model terms is performed each time through, but an
** economical implementation would probably elect to do it less
** frequently, say at 1 Hz.  If the update intervals are too long, the
** corrections will not keep up with changing flexure etc. and will
** also inject small glitches into the tracking demands.
**
** This demonstration shows just one way of computing the tracking
** demands, and gives some indication of the level of intricacy that a
** real TCS application would require.  At first sight all that is
** involved is to take the current target direction and hotspot
** coordinates and solve the VT for the axis demands.  However, there is
** an interplay between the three axes that has to be resolved, and
** the servo systems controlling the axis may well expect velocity
** feedforward information (and conceivably acceleration and higher
** derivatives).  Consequently generating each new set of demands is
** likely to involve multiple VT solutions, and the precise way this is
** done will be something for the application developer to decide.
**
** A feature of the demonstration is that the velocities are NOT
** computed by differencing successive demands, though this approach
** would be efficient and may well work as it would in the present
** simulation of sidereal tracking.  The potential difficulty is in the
** general case, where the observing program is free to introduce abrupt
** changes to target, hotspot etc.  The resulting velocity glitches can
** cause adverse reactions from servo systems and inject unwanted
** transients into the tracking.  The demonstration code instead
** recomputes the demands from last time, but using the current target
** and hotspot.  This means that the velocities always reflect a steady
** background track if if something changed since last time.
*/

   printf ( "\n-----------------------------------"
            "---------------------------------\n\n" );

   printf ( "    TAI              azimuth        elevation"
            "       rotator\n" );

/* Initialize the per-timestep changes in the three axes. */
   da = 0.0;
   db = 0.0;
   dr = 0.0;

/* Initialize the time offset. */
   dt = 0.0;

/* The demonstration track (20Hz for 1s). */
   for ( i = 0; i <= 20; i++ ) {

   /* Solve for previous roll,pitch demands and field orientation. */
      j = spkVtel ( AXES, &obs, &opt, &pm, &ast, &ax3, &tar, &po,
                    &tara, &tare, &tarr, &tarp, soln );
      if ( j ) {
         printf ( "spkVtel status = %d\n", j);
         return -1;
      }

   /* The roll, pitch and rotator demands then. */
      aold = soln[0];
      bold = soln[1];
      rold = iauAnpm ( soln[4] + ypa );

   /* New time offset (seconds) relative to provided UTC. */
      dt = ( (double) (i-20) ) / 20.0;

   /* Update astrometry to that time. */
      if ( spkAstr ( 0, utc, dt,
           &eop, &obs, &air, &opt, &ast ) ) return -1;

   /* Iterate to let roll/pitch/rotator angles settle. */
      for ( it = 0; it < 2; it++ ) {

      /* Extrapolated encoder readings. */
         ax3.a = aold + da;
         ax3.b = bold + db;
         ax3.r = rold + dr;

      /* Solve for roll,pitch demands and field orientation. */
         j = spkVtel ( AXES, &obs, &opt, &pm, &ast, &ax3, &tar, &po,
                       &tara, &tare, &tarr, &tarp, soln );
         if ( j ) {
            printf ( "spkVtel status = %d\n", j);
            return -1;
         }

      /* Set achieved roll,pitch,rotator to match. */
         ax3.a = soln[0];
         ax3.b = soln[1];
         ax3.r = iauAnp ( soln[4] + ypa );

      /* Changes since last time. */
         da = iauAnpm ( ax3.a - aold );
         db = iauAnpm ( ax3.b - bold );
         dr = iauAnpm ( ax3.r - rold );

      /* Next iteration. */
      }

   /* Update the pointing model (see note). */
      pm.pia =   25.0 * AS2R;              /* +IA */
      pm.pib =   15.0 * AS2R;              /* +IE */
      pm.pvd =   10.0 * AS2R * cos(tare);  /* +TF */
      pm.pca = -110.0 * AS2R;              /* +CA */
      pm.pnp =    8.0 * AS2R;              /* +NPAE */
      pm.paw =  999.9 * AS2R;              /* +AW */
      pm.pan =  -12.0 * AS2R;              /* +AN */

   /* Report the demands (as lefthanded altaz) and velocities. */
      a = iauAnp ( D180 - ax3.a );
      if ( i > 17 ) {
         printf ( "\n%16.10f %15.10f %14.10f %15.10f  deg\n",
                  ast.tai, a/D2R, ax3.b/D2R, iauAnp(ax3.r)/D2R );
         printf ( "%+32.10f %+14.10f %+15.10f  \"/s\n",
                  -20.0*da/AS2R, 20.0*db/AS2R, 20.0*dr/AS2R );
      }
   }
   printf ( "    MJD\n" );

/*
** -----------------------------------
** Demonstrate solving for axis angles
** -----------------------------------
*/

   j = spkVtel ( AXES, &obs, &opt, &pm, &ast, &ax3, &tar, &po,
                 &tara, &tare, &tarr, &tarp, soln );
   if ( j ) {
      printf ( "spkVtel status = %d\n", j);
      return -1;
   }

   printf ( "\n-----------------------------------"
            "---------------------------------\n\n" );
   printf ( "Given:\n" );
   printf ( "   TAI = %15.9f MJD\n", ast.tai );
   printf ( "   RMA = %15.9f deg\n", ax3.r/D2R );
   iauA2tf ( 4, iauAnp(tar.a), &s, i4 );
   printf ( "   target = %2.2d %2.2d %2.2d.%4.4d",
            i4[0], i4[1], i4[2], i4[3] );
   iauA2af ( 3, iauAnpm(tar.b), &s, i4 );
   printf ( "  %c%2.2d %2.2d %2.2d.%3.3d",
               (int) s, i4[0], i4[1], i4[2], i4[3] );
   printf ( "  %s\n", csys[tar.sys] );
   printf ( "   hotspot [x,y] = %12.6f, %12.6f mm\n", po.x, po.y );
   printf ( "Returned:\n" );
   printf ( "   encoder [Az,El] = %15.9f (N thru E), %15.9f deg\n",
            180.0-soln[0]/D2R, soln[1]/D2R );
   printf ( "   PA of +y = %15.9f deg (%s)\n",
                             iauAnp(ax3.r-soln[4])/D2R, csys[tar.sys] );

/*
** ----------------------------------------------------
** Demonstrate solving for RA,Dec given focal plane x,y
** ----------------------------------------------------
*/

   po2.x = 25.0;
   po2.y = 27.5;
   j = spkVtel ( TARG, &obs, &opt, &pm, &ast, &ax3, &tar, &po2,
                 &tara, &tare, &tarr, &tarp, soln );
   if ( j ) {
      printf ( "spkVtel status = %d\n", j);
      return -1;
   }

   printf ( "\n-----------------------------------"
            "---------------------------------\n\n" );
   printf ( "Given:\n" );
   printf ( "   TAI = %15.9f MJD\n", ast.tai );
   printf ( "   RMA = %15.9f deg\n", ax3.r/D2R );
   printf ( "   hotspot [x,y] = %12.6f, %12.6f mm\n", po2.x, po2.y );
   printf ( "   encoder [Az,El] = %15.9f (N thru E), %15.9f deg\n",
            180.0-ax3.a/D2R, ax3.b/D2R );
   printf ( "Returned:\n" );
   iauA2tf ( 4, iauAnp(soln[0]), &s, i4 );
   printf ( "   target = %2.2d %2.2d %2.2d.%4.4d",
            i4[0], i4[1], i4[2], i4[3] );
   iauA2af ( 3, iauAnpm(soln[1]), &s, i4 );
   printf ( "  %c%2.2d %2.2d %2.2d.%3.3d",
               (int) s, i4[0], i4[1], i4[2], i4[3] );
   printf ( "  %s\n", csys[tar.sys] );

/*
** ----------------------------------------------------
** Demonstrate solving for focal plane x,y given RA,Dec
** ----------------------------------------------------
*/

   if ( iauTf2a ( ' ', 14, 35, 35.1363560, &rc ) ) return -1;
   if ( iauAf2a ( '-', 12, 32, 10.771790, &dc ) ) return -1;
   spkCtar ( rc, dc, 0.0, 0.0, 0.0, 0.0, &ast, &tar2 );
   j = spkVtel ( HOTS, &obs, &opt, &pm, &ast, &ax3, &tar2, &po,
                 &tara, &tare, &tarr, &tarp, soln );
   if ( j ) {
      printf ( "spkVtel status = %d\n", j);
      return -1;
   }

   printf ( "\n-----------------------------------"
            "---------------------------------\n\n" );
   printf ( "Given:\n" );
   printf ( "   TAI = %15.9f MJD\n", ast.tai );
   printf ( "   RMA = %15.9f deg\n", ax3.r/D2R );
   iauA2tf ( 4, iauAnp(tar2.a), &s, i4 );
   printf ( "   target = %2.2d %2.2d %2.2d.%4.4d",
            i4[0], i4[1], i4[2], i4[3] );
   iauA2af ( 3, iauAnpm(tar2.b), &s, i4 );
   printf ( "  %c%2.2d %2.2d %2.2d.%3.3d",
               (int) s, i4[0], i4[1], i4[2], i4[3] );
   printf ( "  %s\n", csys[tar2.sys] );
   printf ( "   encoder [Az,El] = %15.9f (N thru E), %15.9f deg\n",
            180.0-ax3.a/D2R, ax3.b/D2R );
   printf ( "Returned:\n" );
   printf ( "   hotspot [x,y] = %12.6f, %12.6f mm\n",
            soln[0], soln[1] );

/*
** -------------------------------
** Determine direction of vertical
** -------------------------------
*/

   tar2.sys = AZEL;
   tar2.a = D180 - tara;
   tar2.b = tare;
   j = spkVtel ( AXES, &obs, &opt, &pm, &ast, &ax3, &tar2, &po,
                 &tara, &tare, &tarr, &tarp, soln );

   printf ( "\n-----------------------------------"
            "---------------------------------\n\n" );
   printf ( "Given:\n" );
   printf ( "   TAI = %15.9f MJD\n", ast.tai );
   printf ( "   RMA = %15.9f deg\n", ax3.r/D2R );
   printf ( "   target = %15.9f (N thru E), %15.9f deg (%s)\n",
            tar2.a/D2R, tar2.b/D2R, csys[tar2.sys] );
   printf ( "   hotspot [x,y] = %12.6f, %12.6f mm\n", po.x, po.y );
   printf ( "Returned:\n" );
   printf ( "   encoder [Az,El] = %15.9f (N thru E), %15.9f deg\n",
            180.0-ax3.a/D2R, ax3.b/D2R );
   printf ( "   PA of +y = %15.9f deg (%s)\n",
                            iauAnp(ax3.r-soln[4])/D2R, csys[tar2.sys] );

   printf ( "\n-----------------------------------"
            "---------------------------------\n" );

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
