/*
** sofa.h - SOFA-over-ERFA compatibility shim for INDI SPK Math Plugin
**
** Maps the SOFA iauXxx API onto the ERFA eraXxx API so that the SPK C
** sources (spk_vtel.c, spk_pmfit.c, spk_astr.c) can be compiled unchanged
** against the system liberfa library.
**
** ERFA (Essential Routines for Fundamental Astronomy) is a BSD-licensed
** fork of SOFA with identical mathematics.  See https://github.com/liberfa/erfa
**
** Usage: add the directory containing this header to the compiler include
** path, then link against -lerfa.  This is a header-only shim.
*/

#ifndef SOFA_H
#define SOFA_H

#include <erfa.h>

#ifdef __cplusplus
extern "C" {
#endif

/* -----------------------------------------------------------------------
** Type aliases: map SOFA struct names onto the identical ERFA structs
** --------------------------------------------------------------------- */

typedef eraASTROM iauASTROM;
typedef eraLDBODY iauLDBODY;

/* -----------------------------------------------------------------------
** Function name mappings: iau* -> era*
** Only the subset used by spk_vtel.c, spk_pmfit.c, spk_astr.c and
** SPKMathPlugin.cpp is listed here.
** --------------------------------------------------------------------- */

/* Angle normalization */
#define iauAnp     eraAnp
#define iauAnpm    eraAnpm

/* Spherical <-> Cartesian */
#define iauS2c     eraS2c
#define iauC2s     eraC2s

/* Rotation matrices */
#define iauIr      eraIr
#define iauRx      eraRx
#define iauRy      eraRy
#define iauRz      eraRz
#define iauRxp     eraRxp
#define iauTrxp    eraTrxp

/* Geometry */
#define iauPas     eraPas
#define iauHd2ae   eraHd2ae
#define iauAe2hd   eraAe2hd

/* Time conversions */
#define iauDtf2d   eraDtf2d
#define iauJd2cal  eraJd2cal
#define iauUtctai  eraUtctai
#define iauTaiutc  eraTaiutc
#define iauUtcut1  eraUtcut1

/* Astrometry setup */
#define iauAper    eraAper
#define iauAper13  eraAper13
#define iauApco13  eraApco13

/* Coordinate transformations */
#define iauAtciqz  eraAtciqz
#define iauAticq   eraAticq
#define iauAtioq   eraAtioq
#define iauAtoiq   eraAtoiq

#ifdef __cplusplus
}
#endif

#endif /* SOFA_H */
