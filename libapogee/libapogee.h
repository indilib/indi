/* glue so C can make use of key operations available in CApnCamera.
 */

#ifndef LIBAPOGEE_H
#define LIBAPOGEE_H

extern "C" int ApnGlueOpen(unsigned int id);

extern "C" void ApnGlueGetMaxValues (double *exptime, int *roiw, int *roih, int *osw,
    int *osh, int *binw, int *binh, int *shutter, double *mintemp);

extern "C" int ApnGlueSetExpGeom (int roiw, int roih, int osw, int osh, int binw,
    int binh, int roix, int roiy, int *impixw, int *impixh, char whynot[]);

extern "C" void ApnGlueExpAbort(void);

extern "C" int ApnGlueStartExp (double *exptime, int shutter);

extern "C" int ApnGlueExpDone(void);

extern "C" int ApnGlueReadPixels (unsigned short *buf, int nbuf, char whynot[]);

extern "C" void ApnGlueSetTemp (double C);

extern "C" int ApnGlueGetTemp (double *Cp);

extern "C" void ApnGlueSetFan (int speed);

extern "C" void ApnGlueGetName(char **sensor, char **camera);

#endif
