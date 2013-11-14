/* glue so C can make use of key operations available in CApnCamera.
 */

#ifndef LIBAPOGEE_H
#define LIBAPOGEE_H

#define APOGEE_USB_ONLY	0
#define APOGEE_ETH_ONLY 1

#ifdef __cplusplus
extern "C" {
#endif

int ApnGlueOpen(unsigned int id);

void ApnGlueGetMaxValues (double *exptime, int *roiw, int *roih, int *osw,
    int *osh, int *binw, int *binh, int *shutter, double *mintemp);

int ApnGlueSetExpGeom (int roiw, int roih, int osw, int osh, int binw,
    int binh, int roix, int roiy, int *impixw, int *impixh, char whynot[]);

void ApnGlueExpAbort(void);

int ApnGlueStartExp (double *exptime, int shutter);

int ApnGlueExpDone(void);

int ApnGlueReadPixels (unsigned short *buf, int nbuf, char whynot[]);

void ApnGlueSetTemp (double C);

int ApnGlueGetTemp (double *Cp);

void ApnGlueSetFan (int speed);

void ApnGlueGetName(char **sensor, char **camera);

#ifdef __cplusplus
}
#endif

#endif
