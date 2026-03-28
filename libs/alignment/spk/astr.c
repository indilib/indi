#include "spk.h"
#include <sofa.h>

int spkAstr ( int jfull, spkUTC utc, double dt, spkEOP* eop,
              spkOBS* obs, spkAIR* air, spkOPT* opt, spkAST* ast )
/*
**  - - - - - - - -
**   s p k A s t r
**  - - - - - - - -
**
**  For a specified UTC, update star-independent astrometry parameters.
**
**  Given:
**     jfull   int       TRUE/FALSE = full/partial update (Notes 1,2)
**     utc     spkUTC    UTC date and time (Notes 2-4)
**     dt      double    offset (SI seconds, Note 2)
**     eop     spkEOP*   Earth orientation parameters
**     obs     spkOBS*   site location and mount type
**     air     spkAIR*   ambient weather conditions
**     opt     spkOPT*   for color
**
**  Returned:
**     ast     spkAST*   astrometry parameters (Note 1)
**
**  Returned (function value):
**             int       status: +3 = both of next two
**                               +2 = time is after end of day (Note 3)
**                               +1 = dubious year (Note 4)
**                                0 = OK
**                               -1 = bad year
**                               -2 = bad month
**                               -3 = bad day
**                               -4 = bad hour
**                               -5 = bad minute
**                               -6 = bad second (<0)
**                               -7 = other error (Note 5)
**
**  Notes:
**
**  1) The flag jfull selects whether the entire set of star-independent
**     astrometry parameters are refreshed (jfull = TRUE) or just the
**     Earth rotation angle.  Other quantities such as precession
**     matrices are not recomputed in the jfull = FALSE case, and will
**     as time goes on become less accurate.  How frequently the
**     parameters are fully refreshed is a decision for the application
**     developer, but once for each new target could be adequate.  The
**     TAI included in the star-independent astrometry parameters
**     corresponds to the ERA.
**
**  2) The UTC date and time is supplied as an spkUTC structure,
**     containing y,m,d,h,m,s fields, with an offset in seconds.  The
**     user may elect to supply a fresh spkUTC structure each time, with
**     the offset always zero, or can supply the same spkUTC structure
**     each time and move forward using the offset alone.
**
**  3) The warning status "time is after end of day" usually means that
**     the second argument is greater than 60.0.  However, in a day
**     ending in a leap second the limit changes to 61.0 (or 59.0 in the
**     case of a negative leap second).
**
**  4) The warning status "dubious year" flags UTCs that predate the
**     introduction of the time scale or that are too far in the future
**     to be trusted.  See the iauDat function for further details.
**
**  5) The error status -7 should be impossible.
**
**  Called:
**      iauDtf2d    encode date and time fields into quasi-JD
**      iauUtctai   UTC to TAI
**      iauTaiutc   TAI to UTC
**      iauApco13   prepare star-independent astrometry parameters
**      iauAper13   update Earth rotation
**
**  This revision:   2021 November 22
**
**  Author P.T.Wallace - see license notice at end.
*/

{
   int jstat, j;
   double utc1, utc2, tai1, tai2, ut11, ut12;


/* Translate UTC into SOFA internal quasi-JD. */
   jstat = iauDtf2d ( "UTC", utc.iy, utc.mo, utc.id,
                             utc.ih, utc.mi, utc.sec, &utc1, &utc2 );
   if ( jstat < 0 ) return jstat;

/* Translate into TAI. */
   j = iauUtctai ( utc1, utc2, &tai1, &tai2 );
   if ( j < 0 ) return -7;

/* Include the offset. */
   tai2 += dt / DAYSEC;

/* Record the TAI as MJD. */
   ast->tai = ( tai1 - DJM0 ) + tai2;

/* Translate TAI back into UTC. */
   j = iauTaiutc ( tai1, tai2, &utc1, &utc2 );
   if ( j < 0 ) return -7;

/* Full or partial update? */
   if ( jfull ) {

   /* Full:  complete refresh. */
      j = iauApco13 ( utc1, utc2, eop->dut1, obs->slon, obs->slat,
                      obs->sh, eop->xp, eop->yp, air->p, air->t, air->h,
                      opt->wl, &ast->astrom, &ast->eo );
      if ( j < 0 ) return -7;

   } else {

   /* Partial:  UTC to UT1. */
      j = iauUtcut1 ( utc1, utc2, eop->dut1, &ut11, &ut12 );
      if ( j < 0 ) return -7;

   /* Refresh the ERA. */
      iauAper13 ( ut11, ut12, &ast->astrom );
   }

/* Return the status. */
   return jstat;

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
