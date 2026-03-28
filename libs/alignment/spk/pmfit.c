#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include "sofa.h"
#include "spk.h"

int Bfun ( int, double, char, double, double, double* );
int Simeqn (int, double*, double* );

int Pmfit ( double, char, int, double*, int,
            double*, double*, double* );

/* Internal helper Bfun prototype */
int Bfun ( int, double, char, double, double, double* );
int Simeqn (int, double*, double* );

/*--------------------------------------------------------------------*/

#include <sofa.h>

int Bfun ( int, double, char, double, double, double* );
int Simeqn (int, double*, double* );

int Pmfit ( double phi, char mount, int n, double* obs, int nt,
            double* pm, double* sigmas, double* skysig )
/*
**  - - - - - -
**   P m f i t
**  - - - - - -
**
**  Fit a canonical model to pointing test observations.
**
**  Given:
**     phi     double        latitude (geodetic, radians)
**     mount   char          mount type: 'E' or 'A'
**     n       int           number of observations
**     obs     double*       observations (each 4 doubles)
**     nt      int           number of terms (see the function Bfun)
**
**  Returned:
**     pm      double[nt]    pointing model coefficients (radians)
**     sigmas  double[nt]    model coefficient standard deviations (radians)
**     skysig  double*       on-sky standard deviation of the fit (radians)
**
**  Returned (function value):
**             int           status: 0 = OK
**                                  -1 = illegal n
**                                  -2 = memory allocation error
**                                  -3 = unsupported mount
**                                  -4 = a matrix is singular
**
**  Notes:
**
**  1) The supported pointing model is IH ID CH ME MA TF.
**
**  2) Each of the n observations comprises four angles (all radians):
**
**      observed HA or Az
**      observed Dec or El
**      HA or Az demand
**      Dec or El demand
**
**  3) If an error status is returned, the contents of array pm are
**     undefined.
**
**  Called:   Bfun, Simeqn, iauAnpm
**
**  This revision:   23 September 2023
**
**  Author P.T.Wallace - see license notice at end.
*/
{
/* Basis function addressing */
   #define BF(i,j) bf[i*2+j]

/* Internal form of the observations */
   typedef struct { double r, p; double xy[2]; } SAM;

/* Pointer to the sample array */
   SAM* y;

/* Pointer to the [nt] basis functions */
   double* bf;

/* Pointers to the normal equations [nt][nt] matrix and inverse */
   double* a;
   double* ai;

/* Pointer to the normal equations [nt] vector */
   double* v;

/* Maximum number of iterations */
   const int nitmax = 20;

   int jstat, it, m, i, j, ij;
   double tst, oblon, oblat, elon, elat, enclon, enclat, w, adj, sumr2,
          skyvar;


/* Validate n. */
   if ( n < 1 ) return -1;

/* Safe the malloc pointers. */
   y = NULL;
   bf = NULL;
   a = NULL;
   ai = NULL;
   v = NULL;

/* Allocate space for the samples. */
   y = (SAM*) malloc ( n * sizeof(SAM) );

/* Allocate space for the basis function values. */
   bf = (double*) malloc ( nt * sizeof(double) * 2 );

/* Allocate space for the normal equations matrix... */
   a = (double*) malloc ( nt * nt * sizeof(double) );

/* ...and its inverse. */
   ai = (double*) malloc ( nt * nt * sizeof(double) );

/* Allocate space for the normal equations vector. */
   v = (double*) malloc ( nt * sizeof(double) );

/* Abort if any errors. */
   if ( ! ( y && bf && a && ai && v ) ) {
      jstat = -2;
      goto free;
   }

/* Preset the status to failure. */
   jstat = -4;

/* Initialize the pointing model. */
   for ( i = 0; i < nt; i++ ) pm[i] = 0.0;

/* Initialize convergence test. */
   tst = 9999.9;

/* Iterate, making pointing-model adjustments each time. */
   for ( it = 0; it < nitmax; it++ ) {

   /*
   ** -----------------------------------------
   ** Transform observations into internal form
   ** -----------------------------------------
   */

      for ( m = 0; m < n; m++ ) {

      /* The observed and demanded coordinates. */
         oblon = obs[4*m];
         oblat = obs[4*m+1];
         enclon = obs[4*m+2];
         enclat = obs[4*m+3];

      /* Apply the current model to correct the encoder demands. */
         elon = enclon;
         elat = enclat;
         if ( Bfun ( nt, phi, mount, elon, elat, bf ) ) goto free;
         for ( j = 0; j < nt; j++ ) {
            elon += ( pm[j] * BF(j,0) ) / cos(elat);
            elat += pm[j] * BF(j,1);
         }

      /* Create the sample. */
         y[m].r = oblon;
         y[m].p = oblat;
         y[m].xy[0] = iauAnpm(oblon-elon)*cos(oblat);
         y[m].xy[1] = iauAnpm(oblat-elat);

      /* Next observation. */
      }

   /* Initialize the normal equations. */
      for ( i = 0; i < nt; i++ ) {
         v[i] = 0.0;
         for ( j = 0; j < nt; j++ ) {
            ij = i*nt + j;
            a[ij] = 0.0;
         }
      }

   /* For each sample... */
      for ( m = 0; m < n; m++ ){

      /* Compute the xi,eta basis functions for this sample. */
         if ( Bfun ( nt, phi, mount, y[m].r, y[m].p, bf ) ) goto free;

      /* Update normal equations, combining xi and eta components. */
         for ( i = 0; i < nt; i++ ) {
            v[i] += BF(i,0)*y[m].xy[0] + BF(i,1)*y[m].xy[1];
            for ( j = 0; j < nt; j++ ) {
               ij = i*nt + j;
               a[ij] += BF(i,0)*BF(j,0) + BF(i,1)*BF(j,1);
            }
         }
      /* Next sample. */
      }

   /* Solve the normal equations. */
      if ( Simeqn ( nt, a, v ) ) goto free;

   /* Adjust the pointing model, testing for convergence. */
      w = 0.0;
      for ( i = 0; i < nt; i++ ) {
         adj = v[i];
         pm[i] += adj;
         w += adj*adj;
      }
      if ( w >= tst ) break;
      tst = w;

   /* Next iteration. */
   }

/*
** ------------------------------
** Sky RMS and coefficient sigmas
** ------------------------------
*/

/* Reset sum of distance on sky squared. */
   sumr2 = 0.0;

/* For each sample... */
   for ( m = 0; m < n; m++ ){

   /* The observed and demanded coordinates. */
      oblon = obs[4*m];
      oblat = obs[4*m+1];
      enclon = obs[4*m+2];
      enclat = obs[4*m+3];

   /* Predicted observed place using the fitted model. */
      elon = enclon;
      elat = enclat;
      if ( Bfun ( nt, phi, mount, elon, elat, bf ) ) goto free;
      for ( j = 0; j < nt; j++ ) {
         elon += ( pm[j] * BF(j,0) ) / cos(elat);
         elat += pm[j] * BF(j,1);
      }

   /* Accumulate residuals in xi and eta. */
      w = iauAnpm(oblon-elon)*cos(oblat);
      sumr2 += w*w;
      w = iauAnpm(oblat-elat);
      sumr2 += w*w;

   /* Next sample. */
   }

/* On-sky variance and RMS. */
   skyvar = ( 2*n > nt ) ? sumr2 / (double) ( 2*n - nt ) : 0.0;

/* Standard deviations of the model coefficients. */
   for ( i = 0; i < nt; i++ ) sigmas[i] = sqrt ( skyvar * a[i*(nt+1)] );

/* On-sky standard deviation. */
   *skysig = sqrt(skyvar);

/* Status = success. */
   jstat = 0;

/* Free the workspace. */
free:
   free ( y );
   free ( bf );
   free ( a );
   free ( ai );
   free ( v );

/* Return the status. */
   return jstat;
}

/* ------------------------------------------------------------------ */

#include <math.h>

int Bfun ( int nt, double phi, char mount, double rdem, double pdem,
           double* bf )
/*
**  - - - - - -
**   B a s i s
**  - - - - - -
**
**  The basis functions for the default model.
**
**  Given:
**     nt      int             number of terms
**     phi     double          latitude (geodetic, radians)
**     mount   char            mount type: 'E' or 'A'
**     rdem    double          demand HA or Az (radians)
**     pdem    double          demand Dec or El (radians)
**
**  Returned:
**     bf      double*         [nt][2] basis functions
**
**  Returned (function value):
**             int             status: 0 = OK
**                                    -1 = invalid mount
**
**  Notes:
**
**  1) It is the caller's responsibility to supply a usable observation,
**     for example in the visible sky.
**
**  2) The specific six-term model implemented here is a good starting
**     point for most telescopes but can also be regarded as a template
**     for further development.
**
**  3) Each basis function comprises the "collimation" and "pitch"
**     components, respectively, of the pointing correction.
**
**  4) The relationship between the default six-term model fitted in the
**     present function and the internal seven-term model set out in
**     Wallace (2002) Section 4.1.3 is as follows:
**
**      generic       equat          altaz
**
**         IA        -bf[0]          bf[0]
**         IB         bf[1]          bf[1]
**         VD         bf[5]*sin(z)   bf[5]*sin(z)    z = zenith angle
**         CA        -bf[2]          bf[2]
**         NP          0              0
**         AW        -bf[3]          bf[3]
**         AN         bf[4]          bf[4]
**
**  5) The omission of the roll/pitch nonperpendicularity term NP from
**     the default model is because (i) the three terms IA, NP and CA
**     are sufficiently correlated that typical pointing tests find it
**     difficult to disentangle them, and it is often best to fix one of
**     the three at zero and let the other two take up the slack, and
**     (ii) mount manufacturers routinely achieve mechanical
**     perpendicularity better than pointing tests can measure, and so
**     it is better to assume zero for this model coefficient.
**
**  Reference (see Note 4):
**
**     Wallace, P.T., 2002, "A rigorous algorithm for telescope
**     pointing", Advanced Telescope and Instrumentation Control
**     Software II, edited by Lewis, Hilton, proceedings of the SPIE,
**     4848, 125.
**
**  Called:  iauHd2ae, iauAe2hd
**
**  This revision:   20 September 2023
**
**  Author P.T.Wallace - see license notice at end.
*/
{
/* Basis function addressing */
   #define BF(i,j) bf[i*2+j]

/* Number of terms supported by the present function */
   const double nterms = 6;

   double sp, cp, hdem, ddem, adem, edem,
          sh, ch, sd, cd, sa, ca, se, ce;


/* Verify that the model length is valid. */
   if ( nt < 1 || nt > (int)nterms ) return -1;

/* Functions of latitude. */
   sp = sin(phi);
   cp = cos(phi);

/* Observed coordinates and functions. */
   switch ( (int) mount ) {

   case 'E':

   /* Equatorial:  copy HA,Dec and compute Az,El. */
      hdem = rdem;
      ddem = pdem;
      iauHd2ae ( hdem, ddem, phi, &adem, &edem );
      break;

   case 'A':

   /* Altazimuth: copy Az,El and compute HA,Dec. */
      adem = rdem;
      edem = pdem;
      iauAe2hd ( adem, edem, phi, &hdem, &ddem );
      break;

   default:
      return -1;
   }

/* Functions that may be needed in basis function formulas. */
   sh = sin(hdem);
   ch = cos(hdem);
   sd = sin(ddem);
   cd = cos(ddem);
   sa = sin(adem);
   ca = cos(adem);
   se = sin(edem);
   ce = cos(edem);

/*
** ---------------------------------------------------
** The basis functions at the current point in the sky
** ---------------------------------------------------
*/

   switch ( (int) mount ) {

   case 'E':

   /* Equatorial  */

      if (nt > 0) { BF(0,0) = cd;      BF(0,1) = 0.0; }               /* IH */
      if (nt > 1) { BF(1,0) = 0.0;     BF(1,1) = 1.0; }               /* ID */
      if (nt > 2) { BF(2,0) = 1.0;     BF(2,1) = 0.0; }               /* CH */
      if (nt > 3) { BF(3,0) = sh*sd;   BF(3,1) = ch;  }               /* ME */
      if (nt > 4) { BF(4,0) = -ch*sd;  BF(4,1) = sh;  }               /* MA */
      if (nt > 5) { BF(5,0) = cp*sh;   BF(5,1) = cp*ch*sd-sp*cd; }    /* TF */

      break;

   default:

   /* Altazimuth */

      if (nt > 0) { BF(0,0) = -ce;     BF(0,1) = 0.0; }               /* IA */
      if (nt > 1) { BF(1,0) = 0.0;     BF(1,1) = 1.0; }               /* IE */
      if (nt > 2) { BF(2,0) = -1.0;    BF(2,1) = 0.0; }               /* CA */
      if (nt > 3) { BF(3,0) = -sa*se;  BF(3,1) = -ca; }               /* AN */
      if (nt > 4) { BF(4,0) = -ca*se;  BF(4,1) = sa;  }               /* AW */
      if (nt > 5) { BF(5,0) = 0.0;     BF(5,1) = -ce; }               /* TF */

   }

/* Success. */
   return 0;
}

/*--------------------------------------------------------------------*/

#include "stdlib.h"
#include "math.h"

int Simeqn (int n, double *a, double *y )
/*
**  - - - - - - -
**   S i m e q n
**  - - - - - - -
**
**  Solve simultaneous equations Ax = b, returning also the inverse of
**  matrix A.
**
**  Given:
**     n     int           number of equations
**     a     double*       the n x n matrix A
**     y     double*       the right-hand-side variables b
**
**  Returned:
**     a     double*       the inverse of A
**     y     double*       the solution x
**
**  Returned (function value):
**           int           status: 0 = OK
**                                -1 = illegal n
**                                -2 = memory allocation error
**                                -3 = matrix is singular
**
**  Notes:
**
**  1) If the matrix a is singular, a -3 status is returned and the
**     results are undefined.
**
**  2) The algorithm used is Gaussian elimination with partial pivoting;
**     this is fast and respectably accurate.
**
**  3) Example call:
**
**        const int n = ...
**        double a[n][n], y[n];
**        int j;
**         :
**        j = spkDmati ( n, (double*) a, double y[] );
**
**     n.b. the [n][n] matrix a is passed as a single pointer.
**
**  Last revision:   3 September 2023
**
**  Author P.T.Wallace - see license notice at end.
*/
{
   #define TINY 1e-30

   #define SWAP(a,b) ( t=a, a=b, b=t )

   int* iw;

   int js, k, imx, i, j, ki;
   double t, amx, yk;

/* Pointers to beginnings of rows in n x n matrix A */
   double *ak,    /* row k    */
          *ai,    /* row i    */
          *aimx;  /* row imx  */


/* Validate n. */
   if ( n < 1 ) return -1;

/* Workspace. */
   iw = (int*) malloc ( n * sizeof(int) );
   if ( ! iw ) return -2;

/* Preset status to fail. */
   js = -3;

/* Find the inverse of A and the solution x. */
   for ( k = 0, ak = a; k < n; k++, ak += n ) {
      amx = fabs ( ak[k] );
      imx = k;
      aimx = ak;
      if ( k != n ) {
         for ( i = k+1, ai = ak+n; i < n; i++, ai += n ) {
            t = fabs ( ai[k] );
            if ( t > amx ) {
               amx = t;
               imx = i;
               aimx = ai;
            }
         }
      }
      if ( amx < TINY ) goto free;
      if ( imx != k ) {
         for ( j = 0; j < n; j++ ) {
            SWAP ( ak[j], aimx[j] );
         }
         SWAP ( y[k], y[imx] );
      }
      iw[k] = imx;
      ak[k] = 1.0 / ak[k];
      for ( j = 0; j < n; j++ ) {
         if ( j != k ) {
            ak[j] *= ak[k];
         }
      }
      yk = y[k] * ak[k];
      y[k] = yk;
      for ( i = 0, ai = a; i < n; i++, ai += n ) {
         if ( i != k ) {
            for ( j = 0; j < n; j++ ) {
               if ( j != k ) {
                  ai[j] -= ai[k] * ak[j];
               }
            }
            y[i] -= ai[k] * yk;
         }
      }
      for ( i = 0, ai = a; i < n; i++, ai += n ) {
         if ( i != k ) {
            ai[k] *= - ak[k];
         }
      }
   }
   for ( k = n; k-- > 0; ) {
      ki = iw[k];
      if ( k != ki ) {
         for ( i = 0, ai = a; i < n; i++, ai += n ) {
            SWAP ( ai[k], ai[ki] );
         }
      }
   }

/* Success. */
   js = 0;

/* Discard the workspace. */
free:
   free ( iw );

/* Return the status. */
   return js;
}

/*----------------------------------------------------------------------
**
**  Copyright (C) 2025 by P.T.Wallace
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