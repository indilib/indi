#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "fitsio.h"

int main(int argc, char *argv[])
{
    fitsfile  *fptr;   /* FITS file pointers defined in fitsio.h */
    int status = 0, ii = 1;
    int  bitpix, datatype, naxis = 2, nkeys, anynul, blank;
    long naxes[9] = {100, 10, 1, 1, 1, 1, 1, 1, 1};
    long first, totpix = 0, npix;
    double *array, bscale = 1.0, bzero = 0.0, nulval = 0.;

    char carray[300], carray2[300];
    char cnullval, cnullval2;
    
    short sarray[300], sarray2[300];
    short snullval, snullval2;

    unsigned short usarray[300], usarray2[300];
    unsigned short usnullval, usnullval2;

    int iarray[300], iarray2[300];
    int inullval, inullval2;

    long larray[300], larray2[300];
    long lnullval, lnullval2;

    float farray[300], farray2[300];
    float fnullval, fnullval2;

    double darray[300], darray2[300];
    double dnullval, dnullval2;


    npix = naxes[0];
    
    for (ii=0; ii< npix; ii++) {
       carray[ii] = ii;
       sarray[ii] = ii;
       iarray[ii] = ii;
       larray[ii] = ii;
       farray[ii] = ii;
       darray[ii] = ii;
       usarray[ii] = ii;
    }

    cnullval  = snullval  = usnullval  = inullval  = lnullval  = 7;
    cnullval2 = snullval2 = usnullval2 = inullval2 = lnullval2 = 20;
    fnullval  = dnullval  = 7.;
    fnullval2 = dnullval2 = 20;

    printf("Null pixels set to value = %d on write, and %d on readback\n",
     inullval, inullval2);
     
    first = 1;
    fits_create_file(&fptr, argv[1], &status);
    fits_set_compression_type(fptr,RICE_1,&status); 


    /* ====================== write/read to SHORT image ==================================== */
    printf("\n=====================================================\n");
    printf("\nWRITE then READ to/from a SHORT image\n");
    
    bitpix = SHORT_IMG;
    fits_create_img(fptr, bitpix, naxis, naxes, &status);

    /*  must define a null value with BLANK keyword, otherwise nulls are ignored */
    blank = 99;
    fits_write_key(fptr, TINT, "BLANK", &blank, "Null value", &status); 
    printf("BLANK = %d\n", blank);


    if (status != 0) {    
        fits_report_error(stderr, status);
        return(status);
    }

    datatype = TBYTE;  
    fits_write_imgnull(fptr, datatype, first, npix, carray, &cnullval, &status); 
    fits_read_img(fptr, datatype, first, npix, &cnullval2, carray2, &anynul, &status);
    printf("TBYTE    anynull = %d,  values: %d %d %d %d %d  status =%d\n",anynul, carray2[5], carray2[6], carray2[7], 
       carray2[8], carray2[9], status);  

    fits_write_img(fptr, datatype, first, npix, carray, &status);
       
    fits_read_img(fptr, datatype, first, npix, &cnullval2, carray2, &anynul, &status);
    printf("TBYTE    anynull = %d,  values: %d %d %d %d %d %d\n",anynul, carray2[5], carray2[6], carray2[7], 
       carray2[8], carray2[9], status);  
       


    datatype = TSHORT;  
    fits_write_imgnull(fptr, datatype, first, npix, sarray, &snullval, &status); 
    fits_read_img(fptr, datatype, first, npix, &snullval2, sarray2, &anynul, &status);
    printf("TSHORT   anynull = %d,  values: %d %d %d %d %d \n",anynul, sarray2[5], sarray2[6], sarray2[7], 
       sarray2[8], sarray2[9]);  
    fits_write_img(fptr, datatype, first, npix, sarray, &status);
    fits_read_img(fptr, datatype, first, npix, &snullval2, sarray2, &anynul, &status);
    printf("TSHORT   anynull = %d,  values: %d %d %d %d %d \n",anynul, sarray2[5], sarray2[6], sarray2[7], 
       sarray2[8], sarray2[9]);  



    datatype = TUSHORT;  
    fits_write_imgnull(fptr, datatype, first, npix, usarray, &usnullval, &status); 
    fits_read_img(fptr, datatype, first, npix, &usnullval2, usarray2, &anynul, &status);
    printf("TUSHORT  anynull = %d,  values: %d %d %d %d %d \n",anynul, usarray2[5], usarray2[6], usarray2[7], 
       usarray2[8], usarray2[9]);  
    fits_write_img(fptr, datatype, first, npix, usarray, &status);
    fits_read_img(fptr, datatype, first, npix, &usnullval2, usarray2, &anynul, &status);
    printf("TUSHORT  anynull = %d,  values: %d %d %d %d %d \n",anynul, usarray2[5], usarray2[6], usarray2[7], 
       usarray2[8], usarray2[9]);  
       
    datatype = TINT;  
    fits_write_imgnull(fptr, datatype, first, npix, iarray, &inullval, &status); 
    fits_read_img(fptr, datatype, first, npix, &inullval2, iarray2, &anynul, &status);
    printf("TINT     anynull = %d,  values: %d %d %d %d %d \n",anynul, iarray2[5], iarray2[6], iarray2[7], 
       iarray2[8], iarray2[9]);  
    fits_write_img(fptr, datatype, first, npix, iarray, &status);
    fits_read_img(fptr, datatype, first, npix, &inullval2, iarray2, &anynul, &status);
    printf("TINT     anynull = %d,  values: %d %d %d %d %d \n",anynul, iarray2[5], iarray2[6], iarray2[7], 
       iarray2[8], iarray2[9]);  

    datatype = TLONG;  
    fits_write_imgnull(fptr, datatype, first, npix, larray, &lnullval, &status); 
    fits_read_img(fptr, datatype, first, npix, &lnullval2, larray2, &anynul, &status);
    printf("TLONG    anynull = %d,  values: %d %d %d %d %d \n",anynul, larray2[5], larray2[6], larray2[7], 
       larray2[8], larray2[9]);  
    fits_write_img(fptr, datatype, first, npix, larray, &status);
    fits_read_img(fptr, datatype, first, npix, &lnullval2, larray2, &anynul, &status);
    printf("TLONG    anynull = %d,  values: %d %d %d %d %d \n",anynul, larray2[5], larray2[6], larray2[7], 
       larray2[8], larray2[9]);  
       
    datatype = TFLOAT;  
    fits_write_imgnull(fptr, datatype, first, npix, farray, &fnullval, &status); 
    fits_read_img(fptr, datatype, first, npix, &fnullval2, farray2, &anynul, &status);
    printf("TFLOAT   anynull = %d,  values: %f %f %f %f %f \n",anynul, farray2[5], farray2[6], farray2[7], 
       farray2[8], farray2[9]);  
    fits_write_img(fptr, datatype, first, npix, farray, &status);
    fits_read_img(fptr, datatype, first, npix, &fnullval2, farray2, &anynul, &status);
    printf("TFLOAT   anynull = %d,  values: %f %f %f %f %f \n",anynul, farray2[5], farray2[6], farray2[7], 
       farray2[8], farray2[9]);  
       
    datatype = TDOUBLE;  
    fits_write_imgnull(fptr, datatype, first, npix, darray, &dnullval, &status); 
    fits_read_img(fptr, datatype, first, npix, &dnullval2, darray2, &anynul, &status);
    printf("TDOUBLE  anynull = %d,  values: %f %f %f %f %f \n",anynul, darray2[5], darray2[6], darray2[7], 
       darray2[8], darray2[9]);  
    fits_write_img(fptr, datatype, first, npix, darray, &status);
    fits_read_img(fptr, datatype, first, npix, &dnullval2, darray2, &anynul, &status);
    printf("TDOUBLE  anynull = %d,  values: %f %f %f %f %f \n",anynul, darray2[5], darray2[6], darray2[7], 
       darray2[8], darray2[9]);  

    /* ========================= write/read to FLOAT image ================================= */
    printf("\nWRITE then READ to/from a FLOAT image\n");

    bitpix = FLOAT_IMG;
    fits_create_img(fptr, bitpix, naxis, naxes, &status);

    if (status != 0) {    
        fits_report_error(stderr, status);
        return(status);
    }
    printf(" Tests WITHOUT null pixels: \n");

        /*  write/read without any null pixels */
    datatype = TBYTE;  
    fits_write_img(fptr, datatype, first, npix, carray, &status);    
    fits_read_img(fptr, datatype, first, npix, &cnullval2, carray2, &anynul, &status);
    printf("TBYTE    anynull = %d,  values: %d %d %d %d %d %d\n",anynul, carray2[5], carray2[6], carray2[7], 
       carray2[8], carray2[9], status);  

    datatype = TSHORT;  
    fits_write_img(fptr, datatype, first, npix, sarray, &status);
    fits_read_img(fptr, datatype, first, npix, &snullval2, sarray2, &anynul, &status);
    printf("TSHORT   anynull = %d,  values: %d %d %d %d %d \n",anynul, sarray2[5], sarray2[6], sarray2[7], 
       sarray2[8], sarray2[9]);  

    datatype = TUSHORT;  
    fits_write_img(fptr, datatype, first, npix, usarray, &status);
    fits_read_img(fptr, datatype, first, npix, &usnullval2, usarray2, &anynul, &status);
    printf("TUSHORT  anynull = %d,  values: %d %d %d %d %d \n",anynul, usarray2[5], usarray2[6], usarray2[7], 
       usarray2[8], usarray2[9]);  
       
    datatype = TINT;  
    fits_write_img(fptr, datatype, first, npix, iarray, &status);
    fits_read_img(fptr, datatype, first, npix, &inullval2, iarray2, &anynul, &status);
    printf("TINT     anynull = %d,  values: %d %d %d %d %d \n",anynul, iarray2[5], iarray2[6], iarray2[7], 
       iarray2[8], iarray2[9]);  

    datatype = TLONG;  
    fits_write_img(fptr, datatype, first, npix, larray, &status);
    fits_read_img(fptr, datatype, first, npix, &lnullval2, larray2, &anynul, &status);
    printf("TLONG    anynull = %d,  values: %d %d %d %d %d \n",anynul, larray2[5], larray2[6], larray2[7], 
       larray2[8], larray2[9]);  
       
    datatype = TFLOAT;  
    fits_write_img(fptr, datatype, first, npix, farray, &status);
    fits_read_img(fptr, datatype, first, npix, &fnullval2, farray2, &anynul, &status);
    printf("TFLOAT   anynull = %d,  values: %f %f %f %f %f \n",anynul, farray2[5], farray2[6], farray2[7], 
       farray2[8], farray2[9]);  

    datatype = TDOUBLE;  
    fits_write_img(fptr, datatype, first, npix, darray, &status);
    fits_read_img(fptr, datatype, first, npix, &dnullval2, darray2, &anynul, &status);
    printf("TDOUBLE  anynull = %d,  values: %f %f %f %f %f \n",anynul, darray2[5], darray2[6], darray2[7], 
       darray2[8], darray2[9]);  



    /* ----- write/read with null pixels ----------------------------------------------------*/
    printf(" Tests WITH null pixels: \n");
    datatype = TBYTE;  
    fits_write_imgnull(fptr, datatype, first, npix, carray, &cnullval, &status); 
    
    fits_read_img(fptr, datatype, first, npix, &cnullval2, carray2, &anynul, &status);
    printf("TBYTE    anynull = %d,  values: %d %d %d %d %d  status =%d\n",anynul, carray2[5], carray2[6], carray2[7], 
       carray2[8], carray2[9], status);  


    datatype = TSHORT;  
    fits_write_imgnull(fptr, datatype, first, npix, sarray, &snullval, &status); 
    fits_read_img(fptr, datatype, first, npix, &snullval2, sarray2, &anynul, &status);
    printf("TSHORT   anynull = %d,  values: %d %d %d %d %d \n",anynul, sarray2[5], sarray2[6], sarray2[7], 
       sarray2[8], sarray2[9]);  

    datatype = TUSHORT;  
    fits_write_imgnull(fptr, datatype, first, npix, usarray, &usnullval, &status); 
    fits_read_img(fptr, datatype, first, npix, &usnullval2, usarray2, &anynul, &status);
    printf("TUSHORT  anynull = %d,  values: %d %d %d %d %d \n",anynul, usarray2[5], usarray2[6], usarray2[7], 
       usarray2[8], usarray2[9]);  

       
    datatype = TINT;  
    fits_write_imgnull(fptr, datatype, first, npix, iarray, &inullval, &status); 
    fits_read_img(fptr, datatype, first, npix, &inullval2, iarray2, &anynul, &status);
    printf("TINT     anynull = %d,  values: %d %d %d %d %d \n",anynul, iarray2[5], iarray2[6], iarray2[7], 
       iarray2[8], iarray2[9]);  

    datatype = TLONG;  
    fits_write_imgnull(fptr, datatype, first, npix, larray, &lnullval, &status); 
    fits_read_img(fptr, datatype, first, npix, &lnullval2, larray2, &anynul, &status);
    printf("TLONG    anynull = %d,  values: %d %d %d %d %d \n",anynul, larray2[5], larray2[6], larray2[7], 
       larray2[8], larray2[9]);  

    datatype = TFLOAT;  
    fits_write_imgnull(fptr, datatype, first, npix, farray, &fnullval, &status); 
    fits_read_img(fptr, datatype, first, npix, &fnullval2, farray2, &anynul, &status);
    printf("TFLOAT   anynull = %d,  values: %f %f %f %f %f \n",anynul, farray2[5], farray2[6], farray2[7], 
       farray2[8], farray2[9]);  

       
    datatype = TDOUBLE;  
    fits_write_imgnull(fptr, datatype, first, npix, darray, &dnullval, &status); 
    fits_read_img(fptr, datatype, first, npix, &dnullval2, darray2, &anynul, &status);
    printf("TDOUBLE  anynull = %d,  values: %f %f %f %f %f \n",anynul, darray2[5], darray2[6], darray2[7], 
       darray2[8], darray2[9]);  


    /* ========================= write/read to UNSIGNED SHORT image (i.e., a scaled image)  =========== */
    printf("\nWRITE then READ to/from a UNSIGNED SHORT image (i.e., scaled)\n");
    bitpix = USHORT_IMG;
    fits_create_img(fptr, bitpix, naxis, naxes, &status);

    /*  must define a null value with BLANK keyword, otherwise nulls are ignored */
    blank = 99;
    fits_write_key(fptr, TINT, "BLANK", &blank, "Null value", &status); 
    printf("BLANK = %d\n", blank);



    if (status != 0) {    
        fits_report_error(stderr, status);
        return(status);
    }

        /*  write/read without any null pixels */
    printf(" Tests WITHOUT null pixels: \n");
    datatype = TBYTE;  
    fits_write_img(fptr, datatype, first, npix, carray, &status);    
    fits_read_img(fptr, datatype, first, npix, &cnullval2, carray2, &anynul, &status);
    printf("TBYTE    anynull = %d,  values: %d %d %d %d %d %d\n",anynul, carray2[5], carray2[6], carray2[7], 
       carray2[8], carray2[9], status);  

    datatype = TSHORT;  
    fits_write_img(fptr, datatype, first, npix, sarray, &status);
    fits_read_img(fptr, datatype, first, npix, &snullval2, sarray2, &anynul, &status);
    printf("TSHORT   anynull = %d,  values: %d %d %d %d %d \n",anynul, sarray2[5], sarray2[6], sarray2[7], 
       sarray2[8], sarray2[9]);  

    datatype = TUSHORT;  
    fits_write_img(fptr, datatype, first, npix, usarray, &status);
    fits_read_img(fptr, datatype, first, npix, &usnullval2, usarray2, &anynul, &status);
    printf("TUSHORT  anynull = %d,  values: %d %d %d %d %d \n",anynul, usarray2[5], usarray2[6], usarray2[7], 
       usarray2[8], usarray2[9]);  
       
    datatype = TINT;  
    fits_write_img(fptr, datatype, first, npix, iarray, &status);
    fits_read_img(fptr, datatype, first, npix, &inullval2, iarray2, &anynul, &status);
    printf("TINT     anynull = %d,  values: %d %d %d %d %d \n",anynul, iarray2[5], iarray2[6], iarray2[7], 
       iarray2[8], iarray2[9]);  

    datatype = TLONG;  
    fits_write_img(fptr, datatype, first, npix, larray, &status);
    fits_read_img(fptr, datatype, first, npix, &lnullval2, larray2, &anynul, &status);
    printf("TLONG    anynull = %d,  values: %d %d %d %d %d \n",anynul, larray2[5], larray2[6], larray2[7], 
       larray2[8], larray2[9]);  
       
    datatype = TFLOAT;  
    fits_write_img(fptr, datatype, first, npix, farray, &status);
    fits_read_img(fptr, datatype, first, npix, &fnullval2, farray2, &anynul, &status);
    printf("TFLOAT   anynull = %d,  values: %f %f %f %f %f \n",anynul, farray2[5], farray2[6], farray2[7], 
       farray2[8], farray2[9]);  

    datatype = TDOUBLE;  
    fits_write_img(fptr, datatype, first, npix, darray, &status);
    fits_read_img(fptr, datatype, first, npix, &dnullval2, darray2, &anynul, &status);
    printf("TDOUBLE  anynull = %d,  values: %f %f %f %f %f \n",anynul, darray2[5], darray2[6], darray2[7], 
       darray2[8], darray2[9]);  



    /* ----- write/read with null pixels ----------------------------------------------------*/
    printf(" Tests WITH null pixels: \n");
    datatype = TBYTE;  
    fits_write_imgnull(fptr, datatype, first, npix, carray, &cnullval, &status); 

    
    fits_read_img(fptr, datatype, first, npix, &cnullval2, carray2, &anynul, &status);
    printf("TBYTE    anynull = %d,  values: %d %d %d %d %d  status =%d\n",anynul, carray2[5], carray2[6], carray2[7], 
       carray2[8], carray2[9], status);  


    datatype = TSHORT;  
    fits_write_imgnull(fptr, datatype, first, npix, sarray, &snullval, &status); 
    fits_read_img(fptr, datatype, first, npix, &snullval2, sarray2, &anynul, &status);
    printf("TSHORT   anynull = %d,  values: %d %d %d %d %d \n",anynul, sarray2[5], sarray2[6], sarray2[7], 
       sarray2[8], sarray2[9]);  

    datatype = TUSHORT;  
    fits_write_imgnull(fptr, datatype, first, npix, usarray, &usnullval, &status); 
    fits_read_img(fptr, datatype, first, npix, &usnullval2, usarray2, &anynul, &status);
    printf("TUSHORT  anynull = %d,  values: %d %d %d %d %d \n",anynul, usarray2[5], usarray2[6], usarray2[7], 
       usarray2[8], usarray2[9]);  

       
    datatype = TINT;  
    fits_write_imgnull(fptr, datatype, first, npix, iarray, &inullval, &status); 
    fits_read_img(fptr, datatype, first, npix, &inullval2, iarray2, &anynul, &status);
    printf("TINT     anynull = %d,  values: %d %d %d %d %d \n",anynul, iarray2[5], iarray2[6], iarray2[7], 
       iarray2[8], iarray2[9]);  

    datatype = TLONG;  
    fits_write_imgnull(fptr, datatype, first, npix, larray, &lnullval, &status); 
    fits_read_img(fptr, datatype, first, npix, &lnullval2, larray2, &anynul, &status);
    printf("TLONG    anynull = %d,  values: %d %d %d %d %d \n",anynul, larray2[5], larray2[6], larray2[7], 
       larray2[8], larray2[9]);  

    datatype = TFLOAT;  
    fits_write_imgnull(fptr, datatype, first, npix, farray, &fnullval, &status); 
    fits_read_img(fptr, datatype, first, npix, &fnullval2, farray2, &anynul, &status);
    printf("TFLOAT   anynull = %d,  values: %f %f %f %f %f \n",anynul, farray2[5], farray2[6], farray2[7], 
       farray2[8], farray2[9]);  

       
    datatype = TDOUBLE;  
    fits_write_imgnull(fptr, datatype, first, npix, darray, &dnullval, &status); 
    fits_read_img(fptr, datatype, first, npix, &dnullval2, darray2, &anynul, &status);
    printf("TDOUBLE  anynull = %d,  values: %f %f %f %f %f \n",anynul, darray2[5], darray2[6], darray2[7], 
       darray2[8], darray2[9]);  

    /* ========================= write/read to SIGNED BYTE image (i.e., a scaled image)  =========== */
    printf("\nWRITE then READ to/from a SIGNED BYTE image (i.e., scaled)\n");
    bitpix = SBYTE_IMG;
    fits_create_img(fptr, bitpix, naxis, naxes, &status);

    /*  must define a null value with BLANK keyword, otherwise nulls are ignored */
    blank = 99;
    fits_write_key(fptr, TINT, "BLANK", &blank, "Null value", &status); 
    printf("BLANK = %d\n", blank);



    if (status != 0) {    
        fits_report_error(stderr, status);
        return(status);
    }

        /*  write/read without any null pixels */
    printf(" Tests WITHOUT null pixels: \n");
    datatype = TBYTE;  
    fits_write_img(fptr, datatype, first, npix, carray, &status);    
    fits_read_img(fptr, datatype, first, npix, &cnullval2, carray2, &anynul, &status);
    printf("TBYTE    anynull = %d,  values: %d %d %d %d %d %d\n",anynul, carray2[5], carray2[6], carray2[7], 
       carray2[8], carray2[9], status);  

    datatype = TSHORT;  
    fits_write_img(fptr, datatype, first, npix, sarray, &status);
    fits_read_img(fptr, datatype, first, npix, &snullval2, sarray2, &anynul, &status);
    printf("TSHORT   anynull = %d,  values: %d %d %d %d %d \n",anynul, sarray2[5], sarray2[6], sarray2[7], 
       sarray2[8], sarray2[9]);  

    datatype = TUSHORT;  
    fits_write_img(fptr, datatype, first, npix, usarray, &status);
    fits_read_img(fptr, datatype, first, npix, &usnullval2, usarray2, &anynul, &status);
    printf("TUSHORT  anynull = %d,  values: %d %d %d %d %d \n",anynul, usarray2[5], usarray2[6], usarray2[7], 
       usarray2[8], usarray2[9]);  
       
    datatype = TINT;  
    fits_write_img(fptr, datatype, first, npix, iarray, &status);
    fits_read_img(fptr, datatype, first, npix, &inullval2, iarray2, &anynul, &status);
    printf("TINT     anynull = %d,  values: %d %d %d %d %d \n",anynul, iarray2[5], iarray2[6], iarray2[7], 
       iarray2[8], iarray2[9]);  

    datatype = TLONG;  
    fits_write_img(fptr, datatype, first, npix, larray, &status);
    fits_read_img(fptr, datatype, first, npix, &lnullval2, larray2, &anynul, &status);
    printf("TLONG    anynull = %d,  values: %d %d %d %d %d \n",anynul, larray2[5], larray2[6], larray2[7], 
       larray2[8], larray2[9]);  
       
    datatype = TFLOAT;  
    fits_write_img(fptr, datatype, first, npix, farray, &status);
    fits_read_img(fptr, datatype, first, npix, &fnullval2, farray2, &anynul, &status);
    printf("TFLOAT   anynull = %d,  values: %f %f %f %f %f \n",anynul, farray2[5], farray2[6], farray2[7], 
       farray2[8], farray2[9]);  

    datatype = TDOUBLE;  
    fits_write_img(fptr, datatype, first, npix, darray, &status);
    fits_read_img(fptr, datatype, first, npix, &dnullval2, darray2, &anynul, &status);
    printf("TDOUBLE  anynull = %d,  values: %f %f %f %f %f \n",anynul, darray2[5], darray2[6], darray2[7], 
       darray2[8], darray2[9]);  



    /* ----- write/read with null pixels ----------------------------------------------------*/
    printf(" Tests WITH null pixels: \n");
    datatype = TBYTE;  
    fits_write_imgnull(fptr, datatype, first, npix, carray, &cnullval, &status); 
    
    fits_read_img(fptr, datatype, first, npix, &cnullval2, carray2, &anynul, &status);
    printf("TBYTE    anynull = %d,  values: %d %d %d %d %d  status =%d\n",anynul, carray2[5], carray2[6], carray2[7], 
       carray2[8], carray2[9], status);  


    datatype = TSHORT;  
    fits_write_imgnull(fptr, datatype, first, npix, sarray, &snullval, &status); 
    fits_read_img(fptr, datatype, first, npix, &snullval2, sarray2, &anynul, &status);
    printf("TSHORT   anynull = %d,  values: %d %d %d %d %d \n",anynul, sarray2[5], sarray2[6], sarray2[7], 
       sarray2[8], sarray2[9]);  

    datatype = TUSHORT;  
    fits_write_imgnull(fptr, datatype, first, npix, usarray, &usnullval, &status); 
    fits_read_img(fptr, datatype, first, npix, &usnullval2, usarray2, &anynul, &status);
    printf("TUSHORT  anynull = %d,  values: %d %d %d %d %d \n",anynul, usarray2[5], usarray2[6], usarray2[7], 
       usarray2[8], usarray2[9]);  

       
    datatype = TINT;  
    fits_write_imgnull(fptr, datatype, first, npix, iarray, &inullval, &status); 
    fits_read_img(fptr, datatype, first, npix, &inullval2, iarray2, &anynul, &status);
    printf("TINT     anynull = %d,  values: %d %d %d %d %d \n",anynul, iarray2[5], iarray2[6], iarray2[7], 
       iarray2[8], iarray2[9]);  

    datatype = TLONG;  
    fits_write_imgnull(fptr, datatype, first, npix, larray, &lnullval, &status); 
    fits_read_img(fptr, datatype, first, npix, &lnullval2, larray2, &anynul, &status);
    printf("TLONG    anynull = %d,  values: %d %d %d %d %d \n",anynul, larray2[5], larray2[6], larray2[7], 
       larray2[8], larray2[9]);  

    datatype = TFLOAT;  
    fits_write_imgnull(fptr, datatype, first, npix, farray, &fnullval, &status); 
    fits_read_img(fptr, datatype, first, npix, &fnullval2, farray2, &anynul, &status);
    printf("TFLOAT   anynull = %d,  values: %f %f %f %f %f \n",anynul, farray2[5], farray2[6], farray2[7], 
       farray2[8], farray2[9]);  

       
    datatype = TDOUBLE;  
    fits_write_imgnull(fptr, datatype, first, npix, darray, &dnullval, &status); 
    fits_read_img(fptr, datatype, first, npix, &dnullval2, darray2, &anynul, &status);
    printf("TDOUBLE  anynull = %d,  values: %f %f %f %f %f \n",anynul, darray2[5], darray2[6], darray2[7], 
       darray2[8], darray2[9]);  



    fits_close_file(fptr, &status);

    /* if error occurred, print out error message */
    if (status)
       fits_report_error(stderr, status);
    return(status);
}
