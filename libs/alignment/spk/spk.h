#ifndef SPKHDEF
#define SPKHDEF

/*
**  - - - - - -
**   s p k . h
**  - - - - - -
**
**  Prototype function declarations for SPK library.
**
**  This revision:   2021 December 4
**
**  Copyright P.T.Wallace.  All rights reserved.
*/

#include "sofa.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
** ---------
** Constants
** ---------
*/

/* Julian Date of Modified Julian Date zero */
#define DJM0 (2400000.5)

/* Day length in seconds */
#define DAYSEC (86400.0)

/*
** ------------
** Enumerations
** ------------
*/

/* Supported mount types */
typedef enum { EQUAT = 1,      /* equatorial */
               ALTAZ = 2       /* altazimuth (left-handed */
             } spkMOUNT;

/* Supported target systems */
typedef enum { ICRS = 1,       /* ICRS astrometric RA,Dec */
               APPT = 2,       /* geocentric apparent RA,Dec */
               AZEL = 3        /* observed az,el */
             } spkSYS;

/* Virtual Telescope solution options */
typedef enum { AXES = 1,       /* mount demands and field orientation */
               TARG = 2,       /* sky coordinates */
               HOTS = 3,       /* focal plane x,y */
               DEFT = 4        /* default */
             } spkVTS;


/*
** ----------
** Structures
** ----------
**
** n.b. All data members are public.  In most cases (the astrometry
**      structure is the exception), no "initialization" functions
**      are provided, and simple direct assignment to data members
**      must be made.
*/

/* Earth orientation parameters */
typedef struct {
   double xp;         /* polar motion in x (radians) */
   double yp;         /* polar motion in y (radians) */
   double dut1;       /* UT1-UTC (s) */
} spkEOP;

/* Observatory */
typedef struct {
   double slon;       /* site east longitude (ITRF, radians) */
   double slat;       /* site geodetic latitude (ITRF, radians) */
   double sh;         /* site altitude above WGS84 ellipsoid (m) */
   spkMOUNT mount;    /* mount type */
   double pavoid;     /* closest to mount pole allowed (radians) */
} spkOBS;

/* Ambient air conditions */
typedef struct {
   double p;            /* pressure (hPa = mB) */
   double t;            /* temperature (degrees Celsius) */
   double h;            /* relative humidity (0-1) */
} spkAIR;

/* Optics */
typedef struct {
   double fl;           /* focal length (user units) */
   double wl;           /* wavelength (micrometers) */
} spkOPT;

/* Pointing model */
typedef struct {
   double pia;          /* IA: roll (-HA or pi-azimuth) index error */
   double pib;          /* IB: pitch (Dec or elevation) index error */
   double pvd;          /* VD: downward droop */
   double pca;          /* CA: telescope/pitch nonperpendicularity */
   double pnp;          /* NP: roll/pitch nonperpendicularity */
   double paw;          /* AW: roll axis misalignment across meridian */
   double pan;          /* AN: roll axis misalignment along meridian */
   double ga;           /* dCA guiding correction */
   double gb;           /* dIB guiding correction */
} spkPM;

/* Pointing origin (aka hotspot) */
typedef struct {
   double x;            /* x-coordinate in rotating FP (user units) */
   double y;            /* y-coordinate in rotating FP (user units) */
} spkPO;

/* Orientations of the three axes */
typedef struct {
   double a;            /* achieved -HA or pi-Az (radians) */
   double b;            /* achieved Dec or El (radians) */
   double r;            /* achieved rotator angle (radians) */
} spkAX3;

/* Time (UTC) */
typedef struct {
   int iy;              /* year CE */
   int mo;              /* month */
   int id;              /* day */
   int ih;              /* hour */
   int mi;              /* minute */
   double sec;          /* seconds */
} spkUTC;

/* Astrometry */
typedef struct {
   double tai;         /* TAI MJD (JD-2400000.5) */
   iauASTROM astrom;   /* star-independent astrometry quantities */
   double eo;          /* equation of the origins (ERA-GST, radians) */
} spkAST;

/* Target (celestial pointing direction) */
typedef struct {
   spkSYS sys;          /* ICRS, APPT or AZEL */
   double a;            /* RA or left-handed azimuth (radians) */
   double b;            /* Dec or elevation (radians) */
} spkTAR;

/*
** -------------------
** Function prototypes
** -------------------
*/

int spkAstr ( int jfull, spkUTC utc, double ds, spkEOP* eop,
              spkOBS* obs, spkAIR* air, spkOPT* opt, spkAST* ast );

void spkCtar ( double rc, double dc, double pr, double pd, double px,
               double rv, spkAST* ast, spkTAR* tar );

int spkVtel ( spkVTS isoln, spkOBS* obs, spkOPT* opt, spkPM* pm,
              spkAST* ast, spkAX3* ax3, spkTAR* tar, spkPO* po,
              double* tara, double* tare, double* tarr, double* tarp,
              double soln[5] );

/* Added for Pmfit compatibility in INDI */
int Pmfit ( double phi, char mount, int n, double* obs, int nt,
            double* pm, double* sigmas, double* skysig );

#ifdef __cplusplus
}
#endif

#endif
