/* glue so C can make use of key operations available in CApnCamera.
 */

#ifdef __cplusplus
extern "C" {
#endif

extern int ApnGlueOpen(void);

extern void ApnGlueGetMaxValues (double *exptime, int *roiw, int *roih, int *osw,
    int *osh, int *binw, int *binh, int *shutter, double *mintemp);

extern int ApnGlueSetExpGeom (int roiw, int roih, int osw, int osh, int binw,
    int binh, int roix, int roiy, int *impixw, int *impixh, char whynot[]);

extern void ApnGlueExpAbort(void);

extern int ApnGlueStartExp (double *exptime, int shutter);

extern int ApnGlueExpDone(void);

extern int ApnGlueReadPixels (unsigned short *buf, int nbuf, char whynot[]);

extern void ApnGlueSetTemp (double C);

extern int ApnGlueGetTemp (double *Cp);

extern void ApnGlueSetFan (int speed);

extern void ApnGlueGetName(char **sensor, char **camera);

#ifdef __cplusplus
}
#endif
