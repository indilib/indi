/*  Program : test_alta.cpp
 *  Version : 2.0
 *  Author  : Dave Mills
 *  Copyright : The Random Factory 2004-2008
 *  License : GPL
 *
 *
 *  This program provides a limited test environment for Apogee ALTA 
 *  series cameras. It utilises the same low-level API provided by the
 *  apogee_USB.so and apogee_NET.so shared object libraries.
 * 
 *  To build for ALTA-E
 * 
        g++ -I/opt/apogee/include -DALTA_NET -c test_alta.cpp -IFpgaRegs -o test_altae \
                        ApnCamera.o ApnCamera_Linux.o ApnCamera_NET.o  \
                        ApogeeNetLinux.o ApnCamData*.o ApnCamTable.o  \
                        -L/opt/apogee/lib -ltcl8.3 -lccd -lfitsTcl
 *
 *  To build for ALTA-U
 *

       g++ -I/opt/apogee/include  -c test_alta.cpp -IFpgaRegs -o test_altau \
                        ApnCamera.o ApnCamera_Linux.o ApnCamera_USB.o  \
                        ApogeeUsbLinux.o ApnCamData*.o ApnCamTable.o  \
                        -L/opt/apogee/lib -ltcl8.3 -lccd -lfitsTcl /opt/apogee/lib/libusb.so

 *
 *  The program is controlled by a set of command line options
 *  Usage information is obtained by invoking the program with -h
 *
 *  eg   ./alta_teste -h
 *
 *  Functions provided include full frame, subregion, binning, image sequences,
 *  fan and cooling control.
 *
 *  Caveats : There is limited error checking on the input options, if you 
 * 	      hang the camera onboard software, simply power cycle the 
 * 	      camera.
 */


#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "fitsio.h"
#include <math.h>
#include <unistd.h>
#include <time.h>
#include "tcl.h"
#include "ApnCamera.h"
#include "ccd.h"

int parse_options (int argc, char **argv);
int saveimage(unsigned short *src_buffer, char *filename, short nx, short ny);
int dobiassubtract(unsigned short *src,unsigned short *dest, int nx, int ny);

/* Declare the camera object. All camera functions and parameters are
 * accessed using the methods and instance variables of this object
 *
 * Their declarations can be found in ApnCamera.h
 */

CApnCamera *alta;

/* Declare globals to store the input command line options */

char imagename[256];
double texposure=1.0;
int  shutter=1;
int ip[4]={0,0,0,0};
int xbin=1;
int ybin=1;
int xstart=0;
int xend=0;
int ystart=0;
int yend=0;
int biascols=0;
int fanmode=0;
double cooling=99.0;
int numexp=1;
int ipause=0;
int verbose=0;
int camnum=1;
int highspeed=0;
int tdimode=0;
int tdirows=0;

/* Bias definitions used in the libccd functions */

extern int bias_start, bias_end, bcols;

/* Functions from libccd, provide simple named image buffer management */
typedef struct {
     unsigned short *pixels;
     int            size;
     short          xdim;
     short          ydim;
     short          zdim;
     short          xbin;
     short          ybin;
     short          type;
     char           name[64];
     int            shmid;
     size_t         shmsize;
     char           *shmem;
} CCD_FRAME;

typedef void *PDATA;
#define MAX_CCD_BUFFERS  1000
PDATA CCD_locate_buffer(char *name, int idepth, short imgcols, short imgrows, short hbin, short vbin);
int   CCD_free_buffer();
int   CCD_locate_buffernum(char *name);
extern CCD_FRAME CCD_Frame[MAX_CCD_BUFFERS];
extern int CCD_free_buffer(char *name);


/* Main executable starts here -----------------------------------------------*/

int main (int argc, char **argv) 
{

	int status;
	unsigned short *image;
	int bnum,i;
	int nx,ny;
	int iexposure;
        unsigned int ipaddr;
        double t;
        char seqname[256];
        unsigned short ir, Reg;

/*	Default the bias to no-bias-subtraction */
        bias_start = 0;
        bias_end = 0;
        bcols = 0;

/*	Obtain user provided options */
	status = parse_options(argc,argv);

/*	Create the camera object , this will reserve memory */
	alta = (CApnCamera *)new CApnCamera();

/*	If this is an ALTA-E model, then add the ip address to the 
	driver initialization. The second argument is the default
	port used (80 is the http default). If you have altered 
	this on your camera, then change the source here too!
 */
#ifdef ALTA_NET
        ipaddr = ip[0]*256*256*256 + ip[1]*256*256 + ip[2]*256 + ip[3];
	alta->InitDriver(ipaddr,80,0);
#else
	alta->InitDriver(camnum,0,0);
#endif

/*	Do a system reset to ensure known state, flushing enabled etc */
	alta->ResetSystem();

/*      Special verbosity to dump regs and exit */
        if (verbose == 99) {
           for(ir=0;ir<106;ir++) {
              alta->Read(ir,Reg);
              printf ("Register %d = %d (%x)\n",ir,Reg,Reg);
           }
           exit(0);
        }

/*	If bias subtraction requested, set it up */
        if (biascols != 0) {
           bcols = biascols;
           bias_start = alta->m_ApnSensorInfo->m_ImagingColumns+1;
           bias_end = alta->m_ApnSensorInfo->m_ImagingColumns+bcols;
           alta->m_pvtRoiPixelsH = alta->m_ApnSensorInfo->m_ImagingColumns + bcols;
           alta->m_ApnSensorInfo->m_PostRoiSkipColumns = bcols;
        }

/*	Setup binning, defaults to full frame */
        i = 0;

/*	Set up a region of interest, defaults to full frame */
        if (xstart > 0) {
          alta->m_pvtRoiStartX = xstart;
          alta->m_pvtRoiStartY = ystart;
          alta->m_pvtRoiPixelsH = (xend-xstart+1);
          alta->m_pvtRoiPixelsV = (yend-ystart+1);
        }

/*      Set up binning */
        alta->m_pvtRoiPixelsH = alta->m_pvtRoiPixelsH / xbin;
        alta->m_pvtRoiPixelsV = alta->m_pvtRoiPixelsV / ybin;
	alta->write_RoiBinningH(xbin);
	alta->write_RoiBinningV(ybin);

/*	Set the required fan mode */
        alta->write_FanMode(fanmode);

#ifndef ALTA_NET
/*	Set the highspeed mode if required */
        if (highspeed > 0) {
           alta->write_DataBits(highspeed);
        }
#endif

/*	If a particular CCD temperature was requested, then enable 
	cooling and set the correct setpoint value */
        if (cooling < 99.0) {
           printf("Waiting for requested temperature of %6.1lf \r",cooling);
           alta->write_CoolerEnable(1);
           alta->write_CoolerSetPoint(cooling);
           t = alta->read_TempCCD();

/*	   Then loop until we get within 0.2 degrees, about the best we can hope for */
           while (fabs(t-cooling) > 0.2) {
               printf("Waiting for requested temperature of %6.1lf, current value is %6.1lf \r",cooling,t);
               sleep(1);
               t = alta->read_CoolerStatus();
               t = alta->read_TempCCD();
           }
           printf("\n	Temperature is now %6.1lf\n",t);
        }

/*	Add a second to ensure readout will be complete when we try to read */
        iexposure = (int)texposure+1;
        
/*	Single image per download */
	alta->write_ImageCount(1);

/*	Loop until all exposures completed */
        while ( i < numexp ) {

/*          Setup kinetics if requested */
            if (tdimode > 0) {
              alta->m_pvtRoiPixelsV = tdirows;

              // Set the TDI row count
              alta->write_TDIRows (tdirows);

              // Set the TDI rate
              alta->write_TDIRate (texposure);

              // Toggle the camera mode for TDI
              alta->write_CameraMode (Apn_CameraMode_TDI);

              // Toggle the sequence download variable
              alta->write_SequenceBulkDownload (true);
/*              iexposure = (int)(tdirows*texposure+2.0); */
            }

/*	    Start an exposure */
            status = alta->Expose(texposure,shutter);

/*	    Wait until done, we could continously poll the camera here instead */
/*            if (tdimode > 0) {
              iexposure = (int)(tdirows*texposure+2.0); 
              sleep(iexposure);
            }
*/

/*	    Readout the image and save in a named buffer (tempobs) */
            status = alta->BufferImage("tempobs"); 

/*	    Use the libccd routine to find the corresponding buffer index */
            bnum = CCD_locate_buffernum("tempobs");

/*	    Print details about the buffer for debug purposes */
            printf("Buffer %4d %s = %d bytes cols=%d rows=%d depth=%d\n",bnum,CCD_Frame[bnum].name,
              CCD_Frame[bnum].size,CCD_Frame[bnum].xdim,CCD_Frame[bnum].ydim,CCD_Frame[bnum].zdim);

/*	    Obtain the memory address of the actual image data, and x,y dimensions */
            image = CCD_Frame[bnum].pixels;
            nx = CCD_Frame[bnum].xdim;
            ny = CCD_Frame[bnum].ydim;

/*	    If this is part of a sequence, prefix image name with the number */
            if (numexp > 1) {
               sprintf(seqname,"%d_%s",i,imagename);
               saveimage(image, seqname, nx, ny);
            } else {
               saveimage(image, imagename, nx, ny);
            }

/*	    Wait requested interval between exposures (default is 0) */
            sleep(ipause);
            i++;
         }

/*	 All done, tidy up */
	 alta->CloseDriver();


}

/*  Helper routines start here-------------------------------------------------*/

/*  This routine provides a very simple command line parser
 *  Unknown options should be ignored, but strict type
 *  checking is NOT done.
 */

int parse_options (int argc, char **argv)
{
   int i;
   int goti,gott,gots,gota;

/* Zero out counters for required options */
   goti=0;
   gott=0;
   gots=0;
   gota=0;
   i=1;

/* Default fanmode to medium */
   fanmode = 2;

/* Loop thru all provided options */
   while (i<argc) {

/*     Image name */
       if (!strncmp(argv[i],"-i",2)) {
          strcpy(imagename,argv[i+1]);
          goti = 1;
       }

/*     Exposure time */
       if (!strncmp(argv[i],"-t",2)) {
          sscanf(argv[i+1],"%lf",&texposure);
          gott = 1;
       }

/*     Shutter state */
       if (!strncmp(argv[i],"-s",2)) {
          sscanf(argv[i+1],"%d",&shutter);
          gots= 1;
       }

/*     IP address for ALTA-E models */
       if (!strncmp(argv[i],"-a",2)) {
          sscanf(argv[i+1],"%d.%d.%d.%d",ip,ip+1,ip+2,ip+3);
          gota = 1;
       }

/*     Fast readout mode for ALTA-U models */
       if (!strncmp(argv[i],"-F",2)) {
          sscanf(argv[i+1],"%d",&highspeed);
       }

/*     Drift readout mode - number of rows */
       if (!strncmp(argv[i],"-d",2)) {
          sscanf(argv[i+1],"%d",&tdirows);
       }

/*     Drift readout mode - TDI */
       if (!strncmp(argv[i],"-D",2)) {
          sscanf(argv[i+1],"%d",&tdimode);
       }

/*     Horizontal binning */
       if (!strncmp(argv[i],"-x",2)) {
          sscanf(argv[i+1],"%d",&xbin);
       }

/*     Vertical binning */
       if (!strncmp(argv[i],"-y",2)) {
          sscanf(argv[i+1],"%d",&ybin);
       }

/*     Region of interest */
       if (!strncmp(argv[i],"-r",2)) {
          sscanf(argv[i+1],"%d,%d,%d,%d",&xstart,&ystart,&xend,&yend);
       }

/*     Bias subtraction */
       if (!strncmp(argv[i],"-b",2)) {
          sscanf(argv[i+1],"%d",&biascols);
       }

/*     Fan mode */
       if (!strncmp(argv[i],"-f",2)) {
          if (!strncmp(argv[i+1],"off",3)==0) fanmode=0;
          if (!strncmp(argv[i+1],"slow",4)==0) fanmode=1;
          if (!strncmp(argv[i+1],"medium",6)==0) fanmode=2;
          if (!strncmp(argv[i+1],"fast",4)==0) fanmode=3;
       }

/*     Setpoint temperature */
       if (!strncmp(argv[i],"-c",2)) {
          sscanf(argv[i+1],"%lf",&cooling);
       }

/*     Sequence of exposures */
       if (!strncmp(argv[i],"-n",2)) {
          sscanf(argv[i+1],"%d",&numexp);
       }

/*     USB camera number */
       if (!strncmp(argv[i],"-u",2)) {
          sscanf(argv[i+1],"%d",&camnum);
       }

/*     Interval to pause between exposures */
       if (!strncmp(argv[i],"-p",2)) {
          sscanf(argv[i+1],"%d",&ipause);
       }

/*     Be more verbose */
       if (!strncmp(argv[i],"-v",2)) {
          sscanf(argv[i+1],"%d",&verbose);
       }

/*     Print usage info */
       if (!strncmp(argv[i],"-h",2)) {
          printf("Apogee image tester -  Usage: \n \
	 -i imagename    Name of image (required) \n \
	 -t time         Exposure time is seconds (required)\n \
	 -s 0/1          1 = Shutter open, 0 = Shutter closed (required)\n \
	 -a a.b.c.d      IP address of camera e.g. 192.168.0.1 (required for ALTA-E models only)\n \
	 -F 0/1          Fast readout mode (ALTA-U models only)\n \
	 -D 0/1          Drift readout mode - TDI, exposure time specifies time-per-row\n \
	 -d num          Number of rows for Drift mode readout\n \
	 -u num          Camera number (default 1 , ALTA-U only) \n \
	 -x num          Binning factor in x, default 1 \n \
	 -y num          Binning factor in y, default 1 \n \
	 -r xs,ys,xe,ye  Image subregion in the format startx,starty,endx,endy \n \
	 -b biascols     Number of Bias columns to subtract \n \
	 -f mode         Fanmode during exposure, off,slow,medium,fast (default medium) \n \
	 -c temp         Required temperature for exposure, default is current value \n \
	 -n num          Number of exposures \n \
	 -p time         Number of seconds to pause between multiple exposures \n \
	 -v verbosity    Print more details about exposure\n");
         exit(0);
        }

/*      All options are 2 args long! */
        i = i+2;
   } 

/* Complain about missing required options, then give up */
   if ( goti == 0 ) printf("Missing argument  -i imagename\n");
   if ( gott == 0 ) printf("Missing argument  -t exposure time\n");
   if ( gots == 0 ) printf("Missing argument  -s shutter state\n");
#ifdef ALTA_NET
   if ( gota == 0 ) printf("Missing argument  -a IP address\n");
   if (goti+gots+gott+gota != 4) exit(1);
#else
   if (goti+gots+gott != 3) exit(1);
#endif

/* Print exposure details */
   if (verbose > 0) {
      printf("Apogee ALTA image test - V2.0\n");
      printf("	Image name is %s\n",imagename);
      printf("	Exposure time is %lf\n",texposure);
      if (numexp    > 1) printf("	Sequence of %d exposures requested\n",numexp);
      if (ipause    > 0.0) printf("	Pause of %d seconds between exposures\n",ipause);
      printf("	Shutter state during exposure will be %d\n",shutter);
#ifdef ALTA_NET
      if (ip[0]     != 0) printf("	ALTA-E ip address is %d.%d.%d.%d\n",ip[0],ip[1],ip[2],ip[3]);
#endif
      if (xbin      > 1) printf("	X binning selected xbin=%d\n",xbin);
      if (ybin      > 1) printf("	Y binning selected ybin=%d\n",ybin);
      if (xstart    != 0) printf("	Subregion readout %d,%d,%d,%d\n",xstart,xend,ystart,yend);
      if (biascols  != 0) printf("	Bias subtraction using %d columns\n",biascols);
      if (fanmode > 0) printf("	Fan set to mode = %d\n",fanmode);
      if (cooling < 99.0) printf("	Requested ccd temperature for exposure is %lf\n",cooling);
      if (tdimode == 1) printf("	TDI mode , number of rows = %d, %d secs per row\n",tdirows,texposure);
   }
   return(0);

}


/*  This routine provides simple FITS writer. It uses the routines
 *  provided by the fitsTcl/cfitsio libraries
 *
 *  NOTE : It will fail if the image already exists
 */

int saveimage(unsigned short *src_buffer, char *filename, short nx, short ny)
{
  

  fitsfile *fptr;       /* pointer to the FITS file, defined in fitsio.h */
  long  fpixel, nelements;
  unsigned short *array;
  unsigned short *simg;
  int status;
  /* initialize FITS image parameters */
  int bitpix   =  USHORT_IMG; /* 16-bit unsigned short pixel values       */
  long naxis    =   2;  /* 2-dimensional image                            */
  long naxes[2];   

    naxes[0] = nx-bcols;
    naxes[1] = ny; 
    array = src_buffer;
    status = 0;         /* initialize status before calling fitsio routines */
    simg = (unsigned short *)CCD_locate_buffer("stemp",2,nx-bcols,ny,1,1);
 
    if (fits_create_file(&fptr, filename, &status)) /* create new FITS file */
         printerror( status );           /* call printerror if error occurs */
 
    /* write the required keywords for the primary array image.     */
    /* Since bitpix = USHORT_IMG, this will cause cfitsio to create */
    /* a FITS image with BITPIX = 16 (signed short integers) with   */
    /* BSCALE = 1.0 and BZERO = 32768.  This is the convention that */
    /* FITS uses to store unsigned integers.  Note that the BSCALE  */
    /* and BZERO keywords will be automatically written by cfitsio  */
    /* in this case.                                                */          
 
    if ( fits_create_img(fptr,  bitpix, naxis, naxes, &status) )
         printerror( status );
 
 
    fpixel = 1;                               /* first pixel to write      */
    nelements = naxes[0] * naxes[1];          /* number of pixels to write */
    dobiassubtract(src_buffer,simg,naxes[0],naxes[1]);
 
    /* write the array of unsigned integers to the FITS file */
    if ( fits_write_img(fptr, TUSHORT, fpixel, nelements, simg, &status) )
        printerror( status );
 
 
    if ( fits_close_file(fptr, &status) )                /* close the file */
         printerror( status );

  
    return(status);
}                                                                               


/*  This routine should do bias subtraction. At present it
 *  uses the minium pixel DN as the bias value, instead of
 *  averaging the bias columns. This is because on the 
 *  test unit I have, averaging these columns does not seem
 *  to give values consistently lower than those in the 
 *  exposed region.
 *
 *  src is the input image with bias columns
 *  dest is a smaller output image with the bias columns trimmed off
 *       and the "bias" subtracted from the image pixels.
 */

int dobiassubtract(unsigned short *src,unsigned short *dest, int nx, int ny)
{
   double biases[128000];
   double abiases;
   int ix,iy, oix;
   int ipix, opix;
   unsigned short minbias;
   minbias = 65535;
   if (bcols == 0) {
     for (iy=0;iy<ny;iy++) {
        biases[iy] = 0.0;
     }
     minbias = 0;
   } else {
     for (iy=0;iy<ny;iy++) {
        biases[iy] = 0.0;
        for (ix=bias_start;ix<=bias_end;ix++) {
            ipix = (nx+bcols)*iy + ix-1;
            biases[iy] = biases[iy] + (float)src[ipix];
            if (src[ipix]<minbias) minbias = src[ipix];
        }
        biases[iy] =  biases[iy] / (float)bcols;
     }
   }
  
   for (iy=0;iy<ny;iy++) {
      oix = 0;
      for (ix=0;ix<nx+bcols;ix++) {
        if (ix < bias_start || ix > bias_end) {
          ipix = (nx+bcols)*iy + ix;
          opix = nx*iy + oix;
          if (src[ipix] < minbias) {
             dest[opix] = 0;
          } else {
            dest[opix] = src[ipix] - (int)minbias;
          }
          oix++;
        }
      }
   }
   return(0);
}




