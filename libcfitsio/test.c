#include <string.h>
#include <stdio.h>
#include "fitsio2.h"
#include <math.h>


 // diametre du soleil en metres

// x = crval1 + cdelt1 ( i - crpix1 )

typedef struct
{
  // parametres de l'image
  int naxis1, naxis2;		// taille de l'image (x,y)
  double crpix1, crpix2;	// pixel centre (sur 1..naxis)
  double crval1, crval2;	// coord du pixel au centre
  double cdelt1, cdelt2;	// pixel spacing
  char ctype1[20], ctype2[20];	// (XXXX-YYY) coord type / map projection
  char cunit1[20], cunit2[20];	// units of coordinate system
  double dsun_obs;		// distance entre l'observateur et le soleil, en metres
  double lonpole, latpole;	// spherical transform angles
  double pc[4];			// 2x2 transform matrix PC11,12,21,22
  unsigned short *data;		// l'image
  double hgln_obs, hglt_obs;	// stonyhurst heliocentric lat/long of observer
} fits;


int
readFITS (char *name, fits * F)
{
  fitsfile *fptr;
  int status;
  char value[100], comment[100];
  int bitpix;
  long fpixel, nbuffer;
  unsigned short nulval;
  int anynull;
  int i;
  fits_open_file (&fptr, name, READONLY, &status);
  if (status)
    {
      printf ("yo\n");
      fits_report_error (stderr, status);
      return (-1);
    }

  // un autre systeme
  fits_read_keyword (fptr, "NAXIS1", value, comment, &status);
  F->naxis1 = atoi (value);
  fits_read_keyword (fptr, "NAXIS2", value, comment, &status);
  F->naxis2 = atoi (value);

  fits_read_keyword (fptr, "CRPIX1", value, comment, &status);
  F->crpix1 = atof (value);
  fits_read_keyword (fptr, "CRPIX2", value, comment, &status);
  F->crpix2 = atof (value);

  fits_read_keyword (fptr, "CRVAL1", value, comment, &status);
  F->crval1 = atof (value);
  fits_read_keyword (fptr, "CRVAL2", value, comment, &status);
  F->crval2 = atof (value);

  fits_read_keyword (fptr, "CDELT1", value, comment, &status);
  F->cdelt1 = atof (value);
  fits_read_keyword (fptr, "CDELT2", value, comment, &status);
  F->cdelt2 = atof (value);

  fits_read_keyword (fptr, "PC1_1", value, comment, &status);
  F->pc[0] = atof (value);
  fits_read_keyword (fptr, "PC1_2", value, comment, &status);
  F->pc[1] = atof (value);
  fits_read_keyword (fptr, "PC2_1", value, comment, &status);
  F->pc[2] = atof (value);
  fits_read_keyword (fptr, "PC2_2", value, comment, &status);
  F->pc[3] = atof (value);

  fits_read_keyword (fptr, "CTYPE1", F->ctype1, comment, &status);
  fits_read_keyword (fptr, "CTYPE2", F->ctype2, comment, &status);

  fits_read_keyword (fptr, "CUNIT1", F->cunit1, comment, &status);
  fits_read_keyword (fptr, "CUNIT2", F->cunit2, comment, &status);

  fits_read_keyword (fptr, "LONPOLE", value, comment, &status);
  F->latpole = 0.0;

  fits_read_keyword (fptr, "DSUN_OBS", value, comment, &status);
  F->dsun_obs = atof (value);

  fits_read_keyword (fptr, "HGLN_OBS", value, comment, &status);
  F->hgln_obs = atof (value);
  fits_read_keyword (fptr, "HGLT_OBS", value, comment, &status);
  F->hglt_obs = atof (value);

  //////////////////////////////////

  // lit l'image... should check that BITPIX is 16
  F->data =
    (unsigned short *) malloc (F->naxis1 * F->naxis2 *
			       sizeof (unsigned short));

  for (i = 0; i < F->naxis1 * F->naxis2; i++)
    F->data[i] = 0;

  fpixel = 1;
  nulval = 0;			// no check for invalid pixels
  nbuffer = F->naxis1 * F->naxis2;

  fits_read_img (fptr, TUSHORT, fpixel, nbuffer, &nulval, F->data, &anynull,
		 &status);
  printf ("status=%d\n", status);

  fits_close_file (fptr, &status);
  return (0);
}


main()
{
fits F1, F2,F3,F4,F5,F6,F7,F8;//now it does not work. I get some FITSIO error message with unknown status. If you make the variables global (not on stack) its fine.

  if (readFITS ("20070328_235900_n4euA.fts", &F1))
    exit (0);
 if (readFITS ("20070328_235900_n4euA.fts", &F2))
    exit (0);
 if (readFITS ("20070328_235900_n4euA.fts", &F3))
    exit (0);
 if (readFITS ("20070328_235900_n4euA.fts", &F4))
    exit (0);
 if (readFITS ("20070328_235900_n4euA.fts", &F5))
    exit (0);
 if (readFITS ("20070328_235900_n4euA.fts", &F6))
    exit (0);
 if (readFITS ("20070328_235900_n4euA.fts", &F7))
    exit (0);
 if (readFITS ("20070328_235900_n4euA.fts", &F8))
    exit (0);

free(F1.data);
free(F2.data);
free(F3.data);
free(F4.data);
free(F5.data);
free(F6.data);
free(F7.data);
free(F8.data);
}

