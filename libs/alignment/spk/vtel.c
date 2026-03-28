#include <math.h>
#include <string.h>
#include "sofa.h"
#include "spk.h"

static void Aim ( double rm[3][3], double pae[3],
                  double *xaim, double *yaim, double *zaim );

static int A2e ( double xaim, double yaim, double zaim,
                 double xtel, double ytel, double ztel,
                 double pia, double pib, double pnp,
                 double srma, double crma,
                 double xpo, double ypo, double ga, double gb,
                 double *ea1, double *eb1,
                 double *ea2, double *eb2 );

static int A2tp ( double xaim, double yaim, double zaim,
                  double xtel, double ytel, double ztel,
                  double pnp, double a, double b,
                  double *xi, double *eta );

static void Bs ( double xtel, double ytel, double ztel,
                 double srma, double crma,
                 double xpo, double ypo, double ga, double gb,
                 double *xbs, double *ybs, double *zbs );

static int Sky ( double enca, double encb,
                 double xtel, double ytel, double ztel,
                 double pia, double pib, double pnp,
                 double srma, double crma,
                 double xpo, double ypo, double ga, double gb,
                 double rm[3][3], spkAST* ast, spkSYS sys,
                 double *tara, double *tarb );

int spkVtel ( spkVTS isoln, spkOBS* obs, spkOPT* opt, spkPM* pm,
              spkAST* ast, spkAX3* ax3, spkTAR* tar, spkPO* po,
              double* tara, double* tare, double* tarr, double* tarp,
              double soln[5] )
/*
**  - - - - - - - -
**   s p k V t e l
**  - - - - - - - -
**
**  Solve a "virtual telescope" (Note 1).
**
**  Given:
**     isoln   spkVTS     what to solve for (Note 3)
**     obs     spkOBS*    observatory
**     opt     spkOPT*    optics (for focal length)
**     pm      spkPM*     pointing model (Note 4)
**     ast     spkAST*    astrometry parameters (Note 5)
**     ax3     spkAX3*    achieved roll, pitch, rotator angles (radians)
**     tar     spkTAR*    target (ICRS, apparent or altaz)
**     po      spkPO*     hotspot x,y (same units as focal length)
**
**  Returned:
**     tara    double*    target observed azimuth (N thru E, radians)
**     tare    double*    target observed elevation (radians)
**     tarr    double*    target roll (radians)
**     tarp    double*    target pitch (radians)
**     soln    double[5]  solution (Notes 3,8)
**
**  Returned (function value):
**            int        status: +1 = pole avoidance occurred
**                                0 = OK
**                               -1 = no solutions
**                               -2 = geometrically impossible
**                               -3 = illegal target system
**
**  Notes:
**
**  1) The virtual telescope (see reference) is a data construct that
**     describes the relationship between three aspects of pointing a
**     telescope, namely the coordinates of the celestial target, the
**     position in the focal plane of the imaged target, and the angles
**     to which the mount axes must be set in order to acquire the
**     image.  Given any two of these, along with certain other
**     information, the third can be deduced.  The scheme is as follows:
**
**                [ TARGET ]             <- target [a,b] and type
**                     v
**        astronomical transformations   <- TDB, UT1, site
**                     v
**                 refraction            <- weather, color
**                     v
**             [ observed Az,El ]
**                     v
**                 mount type
**                     v
**           [ nominal roll,pitch ]
**                     v
**            mount misorientation       <- AW, AN
**                     v
**                  [ AIM ]
**                     |                 }
**                   roll                }
**                     |                 } -> encoder demands
**            roll/pitch nonperp         }
**                     |                 } <- NP, IA, IB
**                   pitch               }
**                     |                 }
**               [ BORESIGHT ]
**                     ^
**                  guiding              <- ga, gb
**                     ^
**                  hotspot              <- [x,y] and rotator angle
**                     ^
**               [ TELESCOPE ]
**                     ^
**            vertical deflection        <- VD
**                     ^
**             tel/pitch nonperp         <- CA
**                     ^
**                 [ 1,0,0 ]
**
**     Thus a telescope can track a star by knowing the RA,Dec and the
**     desired image x,y, and repeatedly solving for mount angles as the
**     astronomical transformations progress.  Or the mapping between
**     sky coordinates and focal plane coordinates can be sampled, in
**     both directions.
**
**  2) The coordinate systems used are oriented as follows.
**
**     The longitude and latitude coordinates of the (arbitrarily
**     oriented) mount are called roll/pitch.  If the mount is an
**     altazimuth, the roll coordinate is pi-azimuth (where in this
**     case azimuth is north zero left-handed) and the pitch coordinate
**     is elevation.  If the mount is an equatorial, roll is -HA and
**     pitch is declination.
**
**     The AIM vector is the pointing direction in the mount roll/pitch
**     coordinate system (see Note 7).
**
**     The TELESCOPE vector is with respect to:
**
**       x-axis:  at right angles to both the roll and pitch axes
**       y-axis:  along the pitch axis
**       z-axis:  at right angles to the other two axes
**
**     In the absence of either collimation error or vertical deflection
**     the TELESCOPE vector is [1,0,0].
**
**     The BORESIGHT vector is expressed in a right-handed Cartesian
**     system where the x,y plane contains the telescope and the pitch
**     axis, and the x-axis is close to the telescope.
**
**     Focal plane x/y is right-handed as seen by the light approaching
**     the focal plane in a non-mirror-imaging telescope such as a
**     refractor.  This means that when projected onto the sky the x/y
**     axes are left-handed.
**
**     Zero rotator angle is when the projection on the sky of the
**     focal plane y axis points at the negative mount pole, hence
**     downwards in an altazimuth and south in an equatorial.  The angle
**     increases counterclockwise on the sky.
**
**  3) The argument isoln specifies what quantities are to be solved
**     for:
**
**     o  If isoln = AXES, the result is the mount axis angles required
**        to image the target onto the desired x,y, known as the
**        "hotspot".  There can be two sets of these angles, one
**        corresponding to the "beyond the pole" state of the mount.
**        Also computed is the field orientation, in the form of the
**        position angle of the rotating focal plane's y-axis.
**        n.b.  The given argument ax3 is needed for other purposes and
**        must contain valid "achieved" angles.
**
**     o  If isoln = TARG, the result is the celestial coordinates of
**        the target.  The supported systems are ICRS astrometric
**        [RA,Dec], geocentric apparent [RA,Dec] and observed [Az,El],
**        the latter left-handed (N thru E).  The given argument tar is
**        needed for other purposes and must contain a valid target.
**
**     o  If isoln = HOTS, the result is the x,y position of the image
**        in the rotating focal plane.  The given argument po is not
**        used and need not be initialized.
**
**     o  If isoln = DEFT (or any other value) no solution takes place -
**        see the next sentence.
**
**     Irrespective of the isoln value, the target's observed place as
**     right-handed [az,el] and mount [roll,pitch] are returned, for
**     general purposes including the calculation of pointing-model
**     terms.
**
**  4) The pointing model pm comprises seven terms, called IA, IB, VD,
**     CA, NP, AW and AN.  A typical operational pointing model would
**     contain a greater variety, each being mapped onto the most
**     natural one of the seven.
**
**  5) The astrometry parameters ast can be formed by calling the
**     spkAstr function with the appropriate UTC specified.  In spkAstr
**     the argument jfull controls whether a complete recalculation is
**     to be carried out or merely the Earth rotation angle.  A common
**     strategy would be to recompute the parameters prior to acquiring
**     a new target (or even once a night), and to update only the ERA
**     at the rate of the tracking loop, typically 20 Hz.
**
**  6) The returned tara and tare are the observed place (i.e. the
**     pointing direction) as left-handed az,el, where az runs north
**     through east.
**
**  7) The returned tarr and tarp are the target observed place in
**     mount roll,pitch, called AIM in the Note 1 diagram.  For an
**     equatorial mount, the nominal orientation of the roll,pitch
**     frame makes tarr minus hour angle and tarp declination.  For an
**     altazimuth mount, the nominal orientation makes tarr pi minus the
**     north-through-east azimuth and tarp elevation (more properly
**     called altitude).  For both types of mount, the actual
**     orientation depends on data members pan and paw in the spkPM
**     pointing-model structure.  These are typically a few tens of
**     arcseconds, and are called polar misalignment for an equatorial
**     and azimuth axis tilt in an altazimuth.
**
**  8) The soln array receives from one to five doubles, depending on
**     the isoln argument, as follows:
**
**            isoln      AXES      HOTS      TARG      else
**
**          soln[0]       r         x         f         -
**          soln[1]       p         y         g         -
**
**          soln[2]       rb        -         -         -
**          soln[3]       pb        -         -         -
**
**          soln[4]       th        -         -         -
**
**     Unused soln elements (marked -) are undefined.  Everything but
**     x and y are radians.
**
**     For the AXES solution:
**
**     . The results r,p and rb,pb are the roll,pitch mount pointing
**       demands needed to image the target onto the hotspot.
**
**       The "b" pair are for the "beyond the pole" configuration.  It
**       is up to the telescope control application to decide whether
**       r,p or rb,pb are to be used as drive demands.  For equatorials
**       the "b" set represents below the pole in a fork or horseshoe
**       mount and one of the "meridian flip" cases in GEMs and
**       cross-axis mounts.  The "b" set are not commonly used for
**       altazimuths, though they could be in designs where "through the
**       zenith" is allowed.
**
**     . The th result is the rotator demand that will, at the hotspot,
**       make the y-axis of the rotating focal plane point north (or up)
**       in the target's celestial coordinate system.
**
**       If the desired field orientation (i.e. the desired position
**       angle of the y-axis, counterclockwise from north) is ypa, the
**       rotator demand is th + ypa.  In altazimuth telescopes in
**       particular this will provide the ever-changing tracking demands
**       for the rotator.  The same is true for equatorials, though
**       usually the rotator is not driven continuously and the small
**       residual field rotations neglected.
**
**       It is important to understand that the soln[4] value is for the
**       achieved mount orientation, supplied in the ax3 structure, and
**       whether this is close to r,p or rb,pb depends on the current
**       "beyond the pole" status.
**
**     For the HOTS solution, the x,y results are the coordinates of the
**     target's image given the current mount pointing.  They are in the
**     units chosen for the telescope focal length in the spkOBS
**     structure, for example millimeters, and are on the rotating focal
**     plane, so will stay constant for a specific place on an
**     instrument, such as a spectrograph slit.
**
**     For the TARG solution, the f,g results are the sky position that
**     corresponds to the hotspot x,y given the current mount pointing.
**     This is an [RA,Dec] when the target's system is ICRS or APPT, or
**     [Az,El] (left-handed) for the AZEL case.
**
**  Reference:
**
**     Wallace, P.T., 2002, "A rigorous algorithm for telescope
**     pointing", Advanced Telescope and Instrumentation Control
**     Software II, edited by Lewis, Hilton, proceedings of the SPIE,
**     4848, 125 (referred to here as W02).
**
**  Called:  iauAnpm, iauAtciqz, iauAtioq, iauC2s, iauIr, iauPas, iauRx,
**           iauRy, iauRxp, iauS2c, iauAnp, A2e, A2tp, Aim, Sky
**
**  This revision:   2024 July 10
**
**  Author P.T.Wallace - see license notice at end.
*/
{
/* Angles in radians */
   const double D90 = 1.570796326794896619231322,       /* pi/2 */
               D180 = 3.141592653589793238462643,       /* pi */
             ARCSEC = 4.848136811095359935899141e-6;    /* 1 arcsec */

/* A tiny number */
   const double TINY = 1e-20;

   int j, jpav;
   double atar, etar, ri, di, aob, zob, hob, dob, rob, pae[3], rm[3][3],
          p[3], srma, crma, a, b, ad, ed, x, y, z, a1, b1, a2, b2, au,
          bu, xi, eta, f, xtel, ytel, ztel, w;


/*
** ---------------------------------------------
** Obtain the target direction as observed Az,El
** ---------------------------------------------
*/

/* Is the target Az,El? */
   if ( tar->sys == AZEL ) {

   /* Yes;  use it as it is. */
      atar = tar->a;
      etar = tar->b;

   } else {

   /* Otherwise it's an RA,Dec.  Which sort? */
      if ( tar->sys == ICRS ) {

      /* ICRS;  transform into CIRS. */
         iauAtciqz ( tar->a, tar->b, &ast->astrom, &ri, &di );

      } else if ( tar->sys == APPT ) {

      /* Apparent;  transform into CIRS. */
         ri = tar->a + ast->eo;
         di = tar->b;

      } else {

      /* Unrecognized target system;  give up. */
         return -3;
      }

   /* Transform CIRS RA,Dec to observed place. */
      iauAtioq ( ri, di, &ast->astrom, &aob, &zob, &hob, &dob, &rob );

   /* Pointing direction, Az,El (n.b. north zero, left-handed). */
      atar = aob;
      etar = D90 - zob;
   }

/* Return the left-handed az,el. */
   *tara = atar;
   *tare = etar;

/* Express as south zero right-handed az,el vector. */
   iauS2c ( D180 - atar, etar, pae );

/*
** --------------------------------
** Obtain the mount attitude matrix (W02 4.2.2, matrix M)
** --------------------------------
*/

/* Initialize the nominal mount attitude matrix M0 to identity. */
   iauIr ( rm );

/* If EQUAT, y-rotation by colatitude. */
   if ( obs->mount == EQUAT ) iauRy ( obs->slat - D90 , rm );

/* Apply the pole displacements to give matrix M. */
   iauRy ( -pm->pan, rm );
   iauRx ( pm->paw, rm );

/* Use it to return the observed place as mount roll,pitch. */
   iauRxp ( rm, pae, p );
   iauC2s ( p, tarr, tarp );

/*
** -----------------------------------
** Functions of achieved rotator angle
** -----------------------------------
*/

   srma = sin(ax3->r);
   crma = cos(ax3->r);

/*
** ---------------------------
** Obtain the TELESCOPE vector (W02 4.2.14)
** ---------------------------
*/

/* Remove index errors from the mount roll,pitch. */
   a = ax3->a + pm->pia;
   b = ax3->b + pm->pib;

/* Rotate into az,el (left-handed). */
   iauS2c ( a, b, p );
   iauTrxp ( rm, p, p );
   iauC2s ( p, &ad, &ed );

/* Raise by the vertical deflection to give undrooped version.
** Original: iauS2c(ad, ed + pm->pvd, p)
** The original constant formula is consistent with Wallace's own usage where
** pvd was tuned empirically against real observations (see point.c).  It is
** inconsistent, however, when pvd is set from Pmfit's fitted TF coefficient,
** because Pmfit models TF as bf[5]*sin(z) = bf[5]*cos(el) (Bfun Note 4).
** When pvd = pm[5] (the Pmfit coefficient), the sin(z) factor must be applied
** here; omitting it produces site-dependent residuals of several arcseconds. */
   iauS2c ( ad, ed + pm->pvd * cos(ed), p );

/* Rotate back into the mount system. */
   iauRxp ( rm, p, p );

/* The undrooped TELESCOPE vector. */
   x = cos(pm->pca);
   y = sin(pm->pca);
   z = 0.0;

/* Undrooped telescope direction to encoder roll,pitch. */
   j = A2e ( p[0], p[1], p[2], x, y, z, 0.0, 0.0, pm->pnp, srma, crma,
             0.0, 0.0, 0.0, 0.0, &a1, &b1, &a2, &b2 );
   if ( j ) return -1;

/* Select the solution that matches the current posture. */
   if ( fabs(ax3->b) < D90 ) {
      au = a1;
      bu = b1;
   } else {
      au = a2;
      bu = b2;
   }

/* Drooped telescope direction as xi,eta. */
   iauS2c ( a, b, p );
   j = A2tp ( p[0], p[1], p[2], x, y, z, pm->pnp, au, bu, &xi, &eta );
   if ( j ) return -2;

/* Use xi,eta to determine the TELESCOPE vector. */
   f = sqrt ( 1.0 + xi*xi + eta*eta );
   xtel = ( x - xi*y ) / f;
   ytel = ( y + xi*x ) / f;
   ztel = eta / f;

/*
** --------------------------------
** What is the VT to be solved for?
** --------------------------------
*/

   jpav = 0;
   switch ( isoln ) {

   case AXES:

   /*
   ** ----------------------------
   ** Solve for roll,pitch demands (W02 4.2.10)
   ** ----------------------------
   */

   /* Obtain the AIM vector. */
      Aim ( rm, pae, &x, &y, &z );

   /* Safe it if too close to the mount pole. */
      w = sqrt ( x*x + y*y );
      if ( w < obs->pavoid ){
         if ( x == 0.0 && y == 0.0 ) x = y = TINY;
         w = obs->pavoid / w;
         x *= w;
         y *= w;
         z = sqrt ( 1.0 - x*x - y*y );
         jpav = 1;
      }

   /* Safe AIM to ENCODER demands. */
      j = A2e ( x, y, z, xtel, ytel, ztel,
                pm->pia, pm->pib, pm->pnp, srma, crma,
                po->x/opt->fl, po->y/opt->fl, pm->ga, pm->gb,
                &a1, &b1, &a2, &b2 );
      if ( j ) return -1;

   /* Solution 1 roll demand, in range +/- pi. */
      soln[0] = iauAnpm(a1);

   /* Solution 1 pitch demand. */
      soln[1] = b1;

   /* Solution 2 roll demand, in range +/- pi. */
      soln[2] = iauAnpm(a2);

   /* Solution 2 pitch demand. */
      soln[3] = b2;

   /*
   ** -------------------------------
   ** Determine the field orientation (W02 4.3)
   ** -------------------------------
   **
   ** Note that the solution uses the roll,pitch supplied in the ax3
   ** struct, not the values just calculated.  A typical application
   ** will, during tracking, extrapolate the axis positions so that the
   ** starting point is is a good estimate of what is being solved for.
   **
   ** The achieved rotator angle used here is in the form of the sine
   ** and cosine srma,crma, obtained from ax3.r earlier.
   */

   /* The current pointing origin (in focal lengths). */
      x = po->x / opt->fl;
      y = po->y / opt->fl;

   /* Solve for celestial coordinates. */
      j = Sky ( ax3->a, ax3->b, xtel, ytel, ztel,
                pm->pia, pm->pib, pm->pnp, srma, crma,
                x, y, pm->ga, pm->gb, rm, ast, tar->sys,
                &a1, &b1 );
      if ( j ) return -3;

   /* Displace 1 arcsec in the +y direction. */
      y += ARCSEC;

   /* Solve for celestial coordinates. */
      j = Sky ( ax3->a, ax3->b, xtel, ytel, ztel,
                pm->pia, pm->pib, pm->pnp, srma, crma,
                x, y, pm->ga, pm->gb, rm, ast, tar->sys,
                &a2, &b2 );
      if ( j ) return -3;

   /* If Az,El change to right-handed coordinates. */
      if ( tar->sys == AZEL ) {
         a1 = -a1;
         a2 = -a2;
      }

   /* Use the original and displaced P.O. to obtain the P.A. of +y. */
      w = iauPas ( a1, b1, a2, b2 );

   /* The rotator demand that will make the +y axis point north/up. */
      soln[4] = iauAnp ( ax3->r - w );
      break;

   case HOTS:

   /*
   ** ------------------------
   ** Solve for image position (W02 4.2.12)
   ** ------------------------
   */

   /* Obtain the AIM vector. */
      Aim ( rm, pae, &x, &y, &z );

   /* Correct the demanded roll,pitch for index errors. */
      a = ax3->a + pm->pia;
      b = ax3->b + pm->pib;

   /* Determine the image [xi,eta]. */
      j = A2tp ( x, y, z, xtel, ytel, ztel, pm->pnp, a, b, &xi, &eta );
      if ( j ) return -2;

   /* Omit the guiding contributions from the result. */
      xi -= pm->ga;
      eta -= pm->gb;

   /* Rotate into [x,y] coordinates. */
      x = - xi*crma + eta*srma;
      y = - xi*srma - eta*crma;

   /* Return the x,y in user units. */
      soln[0] = x * opt->fl;
      soln[1] = y * opt->fl;

      break;

   case TARG:

   /*
   ** ----------------
   ** Solve for target (W02 4.2.13)
   ** ----------------
   */

   /* Solve for celestial coordinates. */
      j = Sky ( ax3->a, ax3->b, xtel, ytel, ztel,
                pm->pia, pm->pib, pm->pnp, srma, crma,
                po->x/opt->fl, po->y/opt->fl, pm->ga, pm->gb,
                rm, ast, tar->sys,
                &a, &b );
      if ( j ) return -3;

   /* Return the Az,El or RA,Dec. */
      soln[0] = iauAnp(a);
      soln[1] = b;

      break;

   default:

   /* Return only the star direction (altaz and mount). */
      break;
   }

/* Exit with pole avoidance warning. */
   if ( jpav ) return 1;

/* Success. */
   return 0;

}

/*--------------------------------------------------------------------*/

static void Aim ( double rm[3][3], double pae[3],
                  double *xaim, double *yaim, double *zaim )

/*
**  - - - -
**   A i m
**  - - - -
**
**  Obtain the AIM vector components.  (W02 4.2.3)
**
**  Given:
**     rm       double[3][3]  mount attitude matrix (W02 4.2.2, matrix M)
**     pae      double[3]     pointing direction (right-handed az,el)
**
**  Returned:
**     xaim     double*       AIM x-coordinate
**     yaim     double*       AIM y-coordinate
**     zaim     double*       AIM z-coordinate
*/
{
   double prp[3];


/* Rotate pointing direction into mount roll,pitch frame. */
   iauRxp ( rm, pae, prp );

/* The AIM vector components. */
   *xaim = prp[0];
   *yaim = prp[1];
   *zaim = prp[2];

}

/*--------------------------------------------------------------------*/

static int A2e ( double xaim, double yaim, double zaim,
                 double xtel, double ytel, double ztel,
                 double pia, double pib, double pnp,
                 double srma, double crma,
                 double xpo, double ypo, double ga, double gb,
                 double *ea1, double *eb1, double *ea2, double *eb2 )

/*
**  - - - -
**   A 2 e
**  - - - -
**
**  Partial sky to encoders tracking function that starts with the AIM
**  vector and ends with the equivalent ENCODER demands.  (W02 4.2.10.)
**
**  Given:
**     xaim     double     AIM x-coordinate (Note 1)
**     yaim     double     AIM y-coordinate (Note 1)
**     zaim     double     AIM z-coordinate (Note 1)
**     xtel     double     TELESCOPE vector, x-component (Note 1)
**     ytel     double     TELESCOPE vector, y-component (Note 1)
**     ztel     double     TELESCOPE vector, z-component (Note 1)
**     pia      double     7-term pointing model IA (radians)
**     pib      double     7-term pointing model IB (radians)
**     pnp      double     7-term pointing model NP (radians)
**     srma     double     sine of rotator mechanical angle
**     crma     double     cosine of rotator mechanical angle
**     xpo      double     pointing origin x (in focal lengths)
**     ypo      double     pointing origin y (in focal lengths)
**     ga       double     guiding correction, collimation
**     gb       double     guiding correction, pitch
**
**  Returned:
**     ea1      double*    roll coordinate, first solution (Note 2)
**     eb1      double*    pitch coordinate, first solution (Note 2)
**     ea2      double*    roll coordinate, second solution (Note 2)
**     eb2      double*    pitch coordinate, second solution (Note 2)
**
**  Returned (function value):
**              int        status:  0 = OK
**                               else = no solutions (Note 3)
**
**  Called:  Bs
**
**  Notes:
**
**  1  The AIM vector is the pointing direction in the mount roll/pitch
**     coordinate system (see Note 4).
**
**     The TELESCOPE vector [xt,yt,zt] is with respect to:
**
**       x-axis:  at right angles to both the roll and pitch axes
**       y-axis:  along the pitch axis
**       z-axis:  at right angles to the other two axes
**
**     In the absence of either collimation error or vertical deflection
**     the TELESCOPE vector is [1,0,0].
**
**     The BORESIGHT vector [xbs,ybs,zbs] is expressed in a right-handed
**     Cartesian system where the x,y plane contains the telescope and
**     the pitch axis, and the x-axis is close to the telescope.
**
**  2  For any accessible target, there are two solutions; depending on
**     the type of mount, the two solutions correspond to above/below
**     the pole, nearside/farside of the zenith, east/west of the pier,
**     and so on.
**
**  3  Near the pole of the mount, especially when the collimation error
**     or nonperpendicularity are large, there may be no combination of
**     mount angles that images the target at the desired place.  In
**     such cases, the returned status is the "no solutions" value, and
**     the returned roll and pitch are set to safe values.
**
**  4  The only supported case is where the rotator is mounted on the
**     telescope, for example Cass.
*/

{
   const double TINY = 1e-20;

   int j;
   double xbs, ybs, zbs, snp, cnp, p, s, c, b1, b2, sb, cb, x, y,
          a1, a2;


/* Initialize status to OK. */
   j= 0;

/*
** ---------------------------
** Obtain the BORESIGHT vector
** ---------------------------
*/

   Bs ( xtel, ytel, ztel, srma, crma, xpo, ypo, ga, gb,
        &xbs, &ybs, &zbs );

/* Deal with extreme case. */
   if ( xbs == 0.0 ) {
      j = -1;
      xbs = TINY;
   }

/*
** ----------------------------------------
** From AIM and BORESIGHT solve for posture
** ----------------------------------------
*/

/* Functions of a/b nonperpendicularity. */
   snp = sin(pnp);
   cnp = cos(pnp);

/* Deal with extreme NP = +/-90 deg case. */
   if ( cnp == 0.0 ) {
      j = -1;
      cnp = TINY;
   }

/* Solve for pitch angle (two solutions). */
   p = atan2(zbs,xbs);
   s = zaim + snp*ybs;
   c = xaim*xaim + yaim*yaim - ybs*(2.0*zaim*snp+ybs) - snp*snp;
   if ( c >= 0.0 ) {
      c = sqrt(c);
   } else {
      j = -1;
      c = 0.0;
   }
   b1 = atan2(s,c) - p;
   b2 = atan2(s,-c) - p;

/* Solution 1:  rotate boresight vector, first Ry(b) then Rx(np). */
   sb = sin(b1);
   cb = cos(b1);
   x = cb*xbs - sb*zbs;
   y = snp*sb*xbs + cnp*ybs + snp*cb*zbs;

/* Solve for remaining rotation, Rz(-a). */
   a1 = ( x*x + y*y ) != 0.0 ?
           atan2 ( yaim*x - xaim*y, x*xaim + y*yaim ) : 0.0;

/* Same for solution 2. */
   sb = sin(b2);
   cb = cos(b2);
   x = cb*xbs - sb*zbs;
   y = snp*sb*xbs + cnp*ybs + snp*cb*zbs;
   a2 = ( x*x + y*y ) != 0.0 ?
           atan2 ( yaim*x - xaim*y, x*xaim + y*yaim ) : 0.0;

/* Apply the index errors. */
   *ea1 = a1 - pia;
   *eb1 = b1 - pib;
   *ea2 = a2 - pia;
   *eb2 = b2 - pib;

/* Status. */
   return j;

}

/*--------------------------------------------------------------------*/

static int A2tp ( double xaim, double yaim, double zaim,
                  double xtel, double ytel, double ztel,
                  double pnp, double a, double b,
                  double *xi, double *eta )

/*
**  - - - - - - - -
**   t c s A 2 t p
**  - - - - - - - -
**
**  In a "virtual telescope", given the AIM vector, and knowing the
**  mount roll/pitch, determine the [xi,eta] position of the image in
**  the focal plane.  (W02 4.2.11.)
**
**  Given:
**     xaim     double     AIM x-coordinate
**     yaim     double     AIM y-coordinate
**     zaim     double     AIM z-coordinate
**     xtel     double     TELESCOPE vector, x-component
**     ytel     double     TELESCOPE vector, y-component
**     ztel     double     TELESCOPE vector, z-component
**     pnp      double     7-term pointing model NP (radians)
**     a        double     mount "roll" (Note 1)
**     b        double     mount "pitch" (Note 1)
**
**  Returned:
**     xi       double*    image xi-coordinate (Note 2)
**     eta      double*    image eta-coordinate (Note 2)
**
**  Returned (function value):
**              int*       status:    0 = OK
**                                 else = geometrically impossible
**
**  Notes:
**
**  1  The roll and pitch (a,b) are corrected for index errors,
**     i.e. they are the encoder settings plus the index errors.
**
**  2  In impossible cases, the status is set to an error value, and
**     xi and eta are undefined.
*/

{
   const double TINY = 1e-20;

   int j;
   double rp[3][3], p[3], xb, yb, zb, x, y, z, r2, r, w, d;

/* Obtain the posture matrix (W02 4.2.5). */
   iauIr ( rp );
   iauRy ( b, rp );
   iauRx ( pnp, rp );
   iauRz ( -a, rp );

/* Use it to derotate AIM, giving BORESIGHT. */
   p[0] = xaim;
   p[1] = yaim;
   p[2] = zaim;
   iauTrxp ( rp, p, p );
   xb = p[0];
   yb = p[1];
   zb = p[2];

/* Deduce the image [xi,eta] that makes BORESIGHT and TELESCOPE match. */
   x = xtel;
   y = ytel;
   z = ztel;
   r2 = x*x + y*y;
   r = sqrt ( r2 );
   if ( r == 0.0 ) {
      r = 1e-20;
      x = r;
   }
   w = xb*x + yb*y;
   d = w + zb*z;
   if ( d > TINY ) {
      j = 0;
   } else if ( d >= 0.0 ) {
      j = 1;
      d = TINY;
   } else if ( d > -TINY ) {
      j = 2;
      d = -TINY;
   } else {
      j = 3;
   }
   d *= r;
   *xi = ( yb*x - xb*y ) / d;
   *eta = ( zb*r2 - z*w ) / d;

/* Return the status. */
   return j;

}

/*--------------------------------------------------------------------*/

static void Bs ( double xtel, double ytel, double ztel,
                 double srma, double crma,
                 double xpo, double ypo, double ga, double gb,
                 double *xbs, double *ybs, double *zbs )

/*
**  - - -
**   B s
**  - - -
**
**  Determine the telescope BORESIGHT direction, in a right-handed
**  Cartesian system where the x,y plane contains the nominal telescope
**  and the pitch axis, and the x-axis is close to the telescope.
**
**  See W02 4.2.9.
**
**  Given:
**     xtel     double     TELESCOPE vector, x-component
**     ytel     double     TELESCOPE vector, y-component
**     ztel     double     TELESCOPE vector, z-component
**     srma     double     sine of rotator mechanical angle
**     crma     double     cosine of rotator mechanical angle
**     xpo      double     pointing-origin x (in focal lengths)
**     ypo      double     pointing-origin y (in focal lengths)
**     ga       double     guiding correction, collimation
**     gb       double     guiding correction, pitch
**
**  Returned:
**     xbs      double*    BORESIGHT x-coordinate
**     ybs      double*    BORESIGHT y-coordinate
**     zbs      double*    BORESIGHT z-coordinate
**
**  Note:
**
**     The only supported case is where the rotator is mounted on the
**     telescope - Newtonian, Cassegrain etc.
*/

{
   double xi, eta, f, r, w;


/* Pointing-origin position wrt rotator axis (W02 4.2.6). */
   xi = - xpo*crma - ypo*srma;
   eta =  xpo*srma - ypo*crma;

/* Displace rotator axis to allow for guiding (W02 4.2.8). */
   xi += ga;
   eta += gb;

/* Apply to TELESCOPE, giving BORESIGHT (W02 4.2.9). */
   f = sqrt ( 1.0 + xi*xi + eta*eta );
   r = sqrt ( xtel*xtel + ytel*ytel );
   if ( r == 0.0 ) {
      r = 1e-20;
      xtel = r;
   }
   w = eta*ztel;
   *xbs = ( xtel - ( xi*ytel + w*xtel ) / r ) / f;
   *ybs = ( ytel + ( xi*xtel - w*ytel ) / r ) / f;
   *zbs = ( ztel + eta*r ) / f;

}

/*--------------------------------------------------------------------*/

static int Sky ( double enca, double encb,
                 double xtel, double ytel, double ztel,
                 double pia, double pib, double pnp,
                 double srma, double crma,
                 double xpo, double ypo, double ga, double gb,
                 double rm[3][3], spkAST* ast, spkSYS sys,
                 double *tara, double *tarb )
/*
**  - - - -
**   S k y
**  - - - -
**
**  In a "virtual telescope", calculate the target coordinates that
**  correspond to the specified mount encoder demands and pointing-
**  origin.
**
**  Given:
**     enca     double       mount "roll" encoder demand
**     encb     double       mount "pitch" encoder demand
**     xtel     double       TELESCOPE vector, x-component
**     ytel     double       TELESCOPE vector, y-component
**     ztel     double       TELESCOPE vector, z-component
**     pia      double       7-term pointing model IA (radians)
**     pib      double       7-term pointing model IB (radians)
**     pnp      double       7-term pointing model NP (radians)
**     srma     double       sine of rotator mechanical angle
**     crma     double       cosine of rotator mechanical angle
**     xpo      double       pointing-origin x (in focal lengths)
**     ypo      double       pointing-origin y (in focal lengths)
**     ga       double       guiding correction, collimation
**     gb       double       guiding correction, pitch
**     rm       double       mount attitude matrix (Note 1)
**     ast      spkAST*      astrometry quantities
**     sys      spkSYS       system for results
**
**  Returned:
**     tara     double*      target "longitude" (radians, Note 2)
**     tarb     double*      target "latitude" (radians, Note 2)
**
**  Returned (function value):
**              int*         status:  0 = OK
**                                 else = illegal sys
**
**  Called:  iauAticq, iauAtoiq, iauC2s, iauIr, iauRx, iauRy, iauRz,
**           iauRxp, iauTrxp, Bs
**
**  Notes:
**
**  1  The mount attitude matrix is matrix M in W02 4.2.2.  It rotates
**     az,el (south zero right-handed) into roll,pitch.
**
**  2  The returned coordinates targa,targb are in the specified
**     coordinate system.  For sys = ALTAZ, tara is the azimuth (north
**     zero left-handed) and tarb is the elevation.  For sys = APPT
**     they are geocentric apparent RA,Dec (equinox based) and for
**     sys = ICRS  they are ICRS astrometric RA,Dec.
*/

{
   const double D180 = 3.141592653589793238462643;  /* pi */
   const double D90 = 1.570796326794896619231322;   /* pi/2 */

   double a, b, rp[3][3], p[3], az, el, aob, ri, di;


/* Incorporate the index errors into the mount roll,pitch. */
   a = enca + pia;
   b = encb + pib;

/* Obtain the posture matrix (W02 4.2.5). */
   iauIr ( rp );
   iauRy ( b, rp );
   iauRx ( pnp, rp );
   iauRz ( -a, rp );

/* Obtain the BORESIGHT vector. */
   Bs ( xtel, ytel, ztel, srma, crma, xpo, ypo, ga, gb, p, p+1, p+2 );

/* Rotate coordinate system of BORESIGHT, giving AIM. */
   iauRxp ( rp, p, p );

/* Derotate aim into observed az,el (south zero right-handed). */
   iauTrxp ( rm, p, p );
   iauC2s ( p, &az, &el );

/* Conventional north zero left-handed azimuth. */
   aob = D180 - az;

/* What is the target coordinate system? */
   if ( sys == AZEL ) {

   /* Return observed az,el (north zero left-handed). */
      *tara = aob;
      *tarb = el;

   } else {

   /* RA,Dec:  get geocentric apparent CIRS. */
      iauAtoiq ( "A", aob, D90-el, &ast->astrom, &ri, &di );

   /* Apparent or astrometric? */
      if ( sys == APPT ) {

      /* Return equinox based geocentric apparent place. */
         *tara = ri - ast->eo;
         *tarb = di;

      } else if ( sys == ICRS) {

      /* Return ICRS astrometric place. */
         iauAticq ( ri, di, &ast->astrom, tara, tarb );

      } else {

      /* Illegal sys:  abort. */
         return -1;
      }
   }

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
