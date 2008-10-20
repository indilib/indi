#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "fitsio.h"

void printerror( int status);

int main(int argc, char *argv[])
{
    fitsfile *fptr, *outfptr; /* pointer to the FITS file, defined in fitsio.h */

    int status, nkeys, keypos, hdutype, ii, jj;
    char filename[FLEN_FILENAME];    /* input FITS file */
    char outfilename[FLEN_FILENAME];    /* output FITS file */
    char card[FLEN_CARD];   /* standard string lengths defined in fitsioc.h */

    status = 0;

    if (argc == 1)
        strcpy(filename, "-");  /* no command line name, so assume stdin */
    else
        strcpy(filename, argv[1] );   /* name of file to list */

    strcpy(outfilename, argv[2] );   

    if ( fits_open_file(&fptr, filename, READONLY, &status) ) 
         printerror( status );
printf("opened %s\n", filename);
    if ( fits_open_file(&outfptr, outfilename, READWRITE, &status) ) 
         printerror( status );
printf("opened %s\n", outfilename);

    /* get the current HDU number */
    fits_get_hdu_num(fptr, &ii);

 
        ffcphd(fptr, outfptr, &status);
printf("copied header %d status =  %d\n",ii, status);
 

       printerror( status );     /* got an unexpected error                */

    if ( fits_close_file(fptr, &status) )
         printerror( status );

    if ( fits_close_file(outfptr, &status) )
         printerror( status );

    return(0);
}
/*--------------------------------------------------------------------------*/
void printerror( int status)
{
    /*****************************************************/
    /* Print out cfitsio error messages and exit program */
    /*****************************************************/

    char status_str[FLEN_STATUS], errmsg[FLEN_ERRMSG];
  
    if (status)
      fprintf(stderr, "\n*** Error occurred during program execution ***\n");

    fits_get_errstatus(status, status_str);   /* get the error description */
    fprintf(stderr, "\nstatus = %d: %s\n", status, status_str);

    /* get first message; null if stack is empty */
    if ( fits_read_errmsg(errmsg) ) 
    {
         fprintf(stderr, "\nError message stack:\n");
         fprintf(stderr, " %s\n", errmsg);

         while ( fits_read_errmsg(errmsg) )  /* get remaining messages */
             fprintf(stderr, " %s\n", errmsg);
    }

  return;
}
