/*  Program : test_apogee.cpp
 *  Version : 1.2
 *  Author  : Dave Mills
 *  Copyright : The Random Factory 2004
 *  License : GPL
 *
 *
 *  This program provides a limited test environment for Apogee PCI/ISA/PPort 
 *  series cameras. It utilises the same low-level API provided by the
 *  apogee_PCI.so , apogee_ISA.so, and apogee_PPI.so shared object libraries.
 * 
 *  To build for ISA interface models
 * 
       g++ -I/opt/apogee/include  test_apogee.cpp \
			CameraIO_Linux.o CameraIO_LinuxISA.o \
			-o test_apogeeisa -DAPOGEE_ISA \
                        -L/opt/apogee/lib -ltcl8.3 -lccd -lfitsTcl

 *  To build for PCI interface models
 * 
       g++ -I/opt/apogee/include test_apogee.cpp \
			CameraIO_Linux.o CameraIO_LinuxPCI.o \
			-o test_apogeepci  -DAPOGEE_PCI \
                        -L/opt/apogee/lib -ltcl8.3 -lccd -lfitsTcl

 *  To build for Parallel Port interface models
 * 
       g++ -I/opt/apogee/include  test_apogee.cpp \
			CameraIO_Linux.o CameraIO_LinuxPPI.o \
			-o test_apogeeppi  -DAPOGEE_PPI \
                        -L/opt/apogee/lib -ltcl8.3 -lccd -lfitsTcl

 *
 *  The program is controlled by a set of command line options
 *  Usage information is obtained by invoking the program with -h
 *
 *  eg   ./alta_testppi -h
 *
 *  Functions provided include full frame, subregion, binning, image sequences,
 *  and cooling control.
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
#include <ctype.h>
#include <string.h>
#include "tcl.h"
#include "CameraIO_Linux.h"
#include "ccd.h"

int parse_options (int argc, char **argv);
int saveimage(unsigned short *src_buffer, char *filename, short nx, short ny);
int dobiassubtract(unsigned short *src,unsigned short *dest, int nx, int ny);

/* Declare the camera object. All camera functions and parameters are
 * accessed using the methods and instance variables of this object
 *
 * Their declarations can be found in CameraIO_Linux.h
 */

CCameraIO *cam;

/* Declare globals to store the input command line options */

char imagename[256];
char cfgname[256];
double exposure=1.0;
int  shutter=1;
int ip[4]={0,0,0,0};
int xbin=1;
int ybin=1;
int xstart=0;
int xend=0;
int ystart=0;
int yend=0;
int biascols=0;
int bcols=0;
int fanmode=0;
double cooling=99.0;
int numexp=1;
int ipause=0;
int verbose=0;
 
/* Error codes returned from config_load */
const long CCD_OPEN_NOERR   = 0;          // No error detected
const long CCD_OPEN_CFGNAME = 1;          // No config file specified
const long CCD_OPEN_CFGDATA = 2;          // Config missing or missing required data
const long CCD_OPEN_LOOPTST = 3;          // Loopback test failed, no camera found
const long CCD_OPEN_ALLOC   = 4;          // Memory alloc failed - system error

long config_load( char* cfgname, short BaseAddress, short RegOffset );
bool CfgGet ( FILE* inifp,
		   char* inisect,
		   char* iniparm,
		   char* retbuff,
		   short bufflen,
		   short* parmlen);

unsigned short hextoi(char* instr);
void trimstr(char* s);

#define MAXCOLUMNS 	16383
#define MAXROWS 	16383
#define MAXTOTALCOLUMNS 16383
#define MAXTOTALROWS 	16383
#define MAXHBIN 	8
#define MAXVBIN 	64                                                                            

/* Bias definitions used in the libccd functions */

extern int bias_start, bias_end, bcols;

/* Functions from libccd, provide simple named image buffer management */

typedef void *PDATA;
#define MAX_CCD_BUFFERS  1000
PDATA CCD_locate_buffer(char *name, int idepth, short imgcols, short imgrows, short hbin, short vbin);
int   CCD_free_buffer();
int   CCD_locate_buffernum(char *name);
 
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
 
CCD_FRAME CCD_Frame[MAX_CCD_BUFFERS];
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

/*	Default the bias to no-bias-subtraction */
        bias_start = 0;
        bias_end = 0;
        bcols = 0;

/*	Obtain user provided options */
	status = parse_options(argc,argv);

/*	Parse camera configuration  file */
	long ret = config_load( cfgname, -1, -1 );
	if ( ret != 0 ) {
		switch ( ret )
		{
		case CCD_OPEN_CFGNAME:
			printf("No config file specified.\n");
			break;
		case CCD_OPEN_CFGDATA:
			printf("Config file missing or missing required data.\n");
			break;
		case CCD_OPEN_LOOPTST:
			printf("Loopback test failed, no camera found\n");
			break;
		case CCD_OPEN_ALLOC:
			printf("Memory allocation failed - system error\n");
			break;
		}
		exit(1);
        }


/*	Assume only 1 camera, and it is number 0 */
	cam->InitDriver(0);

/*	Do a system reset to ensure known state, flushing enabled etc */
	cam->Reset();

/*	Parse config file for camera parameters */

	cam->Flush();

/*	If bias subtraction requested, set it up */
        if (biascols != 0) {
           bcols = biascols;
           bias_start = 1;
           bias_end = biascols;
           cam->m_BIC = cam->m_BIC-biascols;
        }

/*	Setup binning, defaults to full frame */
        i = 0;
        cam->m_BinX = xbin;
        cam->m_BinY = ybin;

/*	Set up a region of interest, defaults to full frame */
        if (xstart > 0) {
          cam->m_StartX = xstart;
          cam->m_StartY = ystart;
          cam->m_NumX = (xend-xstart+1);
          cam->m_NumY = (yend-ystart+1);
        }


/*	If a particular CCD temperature was requested, then enable 
	cooling and set the correct setpoint value */
        if (cooling < 99.0) {
           printf("Waiting for requested temperature of %6.1lf \r",cooling);
           fflush(NULL);
           cam->write_CoolerMode(0);
           cam->write_CoolerMode(1);
           cam->write_CoolerSetPoint(cooling);
           t = cam->read_Temperature();

/*	   Then loop until we get within 0.2 degrees, about the best we can hope for */
           while (fabs(t-cooling) > 0.2) {
               printf("Waiting for requested temperature of %6.1lf, current value is %6.1lf \r",cooling,t);
               fflush(NULL);
               sleep(1);
               t = cam->read_CoolerStatus();
               t = cam->read_Temperature();
           }
           printf("\nTemperature is now %6.1lf, starting exposure(s)\n",t);
        }

/*	Add a second to ensure readout will be complete when we try to read */
        iexposure = (int)exposure+1;

/*	Loop until all exposures completed */
        while ( i < numexp ) {

/*	    Start an exposure */
            status = cam->Expose(exposure,shutter);

/*	    Wait until done, we could continously poll the camera here instead */
            sleep(iexposure);

/*	    Readout the image and save in a named buffer (tempobs) */
            status = cam->BufferImage("tempobs"); 

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
	       printf("Saving image as %s\n",seqname);
               saveimage(image, seqname, nx, ny);
            } else {
	       printf("Saving image as %s\n",imagename);
               saveimage(image, imagename, nx, ny);
            }

/*	    Wait requested interval between exposures (default is 0) */
            sleep(ipause);
            i++;
         }

/*	 All done, tidy up */


}

/*  Helper routines start here-------------------------------------------------*/

/*  This routine provides a very simple command line parser
 *  Unknown options should be ignored, but strict type
 *  checking is NOT done.
 */

int parse_options (int argc, char **argv)
{
   int i;
   int goti,gott,gots;
   char *hdir;

/* Zero out counters for required options */
   goti=0;
   gott=0;
   gots=0;
   i=1;

/* Default fanmode to medium */
   fanmode = 2;

/* Default location of config file is users home directory */
   hdir = getenv("HOME");
   sprintf(cfgname,"%s/.apccd.ini",hdir);

/* Loop thru all provided options */
   while (i<argc) {

/*     Config file name (usually found in /opt/apogee/config/...) */
       if (!strncmp(argv[i],"-C",2)) {
          strcpy(cfgname,argv[i+1]);
       }

/*     Image name */
       if (!strncmp(argv[i],"-i",2)) {
          strcpy(imagename,argv[i+1]);
          goti = 1;
       }

/*     Exposure time */
       if (!strncmp(argv[i],"-t",2)) {
          sscanf(argv[i+1],"%lf",&exposure);
          gott = 1;
       }

/*     Shutter state */
       if (!strncmp(argv[i],"-s",2)) {
          sscanf(argv[i+1],"%d",&shutter);
          gots= 1;
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


/*     Setpoint temperature */
       if (!strncmp(argv[i],"-c",2)) {
          sscanf(argv[i+1],"%lf",&cooling);
       }

/*     Sequence of exposures */
       if (!strncmp(argv[i],"-n",2)) {
          sscanf(argv[i+1],"%d",&numexp);
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
	 -C configfile   Name of camera config file (default is ~/.apccd.ini) \n \
	 -i imagename    Name of image (required) \n \
	 -t time         Exposure time is seconds (required) \n \
	 -s 0/1          1 = Shutter open, 0 = Shutter closed (required) \n \
	 -x num          Binning factor in x, default 1 \n \
	 -y num          Binning factor in y, default 1 \n \
	 -r xs,ys,xe,ye  Image subregion in the format startx,starty,endx,endy \n \
	 -b biascols     Number of Bias columns to subtract \n \
	 -c temp         Required temperature for exposure, default is current value \n \
	 -n num          Number of exposures \n \
	 -p time         Number of seconds to pause between multiple exposures \n \
	 -v verbosity    Print more details about exposure \n");                                                          
         exit(0);
        }

/*      All options are 2 args long! */
        i = i+2;
   } 

/* Complain about missing required options, then give up */
   if ( goti == 0 ) printf("Missing argument  -i imagename\n");
   if ( gott == 0 ) printf("Missing argument  -t exposure time\n");
   if ( gots == 0 ) printf("Missing argument  -s shutter state\n");
   if (goti+gots+gott != 3) exit(1);

/* Print exposure details */
   if (verbose > 0) {
      printf("Apogee CCD image test - V1.2\n");
      printf("	Image name is %s\n",imagename);
      printf("	Exposure time is %lf\n",exposure);
      if (numexp    > 1) printf("	Sequence of %d exposures requested\n",numexp);
      if (ipause    > 0.0) printf("	Pause of %d seconds between exposures\n",ipause);
      printf("	Shutter state during exposure will be %d\n",shutter);
      if (xbin      > 1) printf("	X binning selected xbin=%d\n",xbin);
      if (ybin      > 1) printf("	Y binning selected ybin=%d\n",ybin);
      if (xstart    != 0) printf("	Subregion readout %d,%d,%d,%d\n",xstart,xend,ystart,yend);
      if (biascols  != 0) printf("	Bias subtraction using %d columns\n",biascols);
      if (cooling < 99.0) printf("	Requested ccd temperature for exposure is %lf\n",cooling);
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


/*  This routine should do bias subtraction. 
 *  src is the input image with bias columns
 *  dest is a smaller output image with the bias columns trimmed off
 *       and the "bias" subtracted from the image pixels.
 */

int dobiassubtract(unsigned short *src,unsigned short *dest, int nx, int ny)
{
   double biases[8192];
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
            dest[opix] = src[ipix] - (int)biases[iy];
          }
          oix++;
        }
      }
   }
   return(0);
}



// Convert a string to a decimal or hexadecimal integer
unsigned short hextoi(char *instr)
{
    unsigned short val, tot = 0;
	bool IsHEX = false;

	long n = strlen( instr );
	if ( n > 1 )
	{	// Look for hex format e.g. 8Fh, A3H or 0x5D
		if ( instr[ n - 1 ] == 'h' || instr[ n - 1 ] == 'H' )
			IsHEX = true;
		else if ( *instr == '0' && *(instr+1) == 'x' )
		{
			IsHEX = true;
			instr += 2;
		}
	}

	if ( IsHEX )
	{
		while (instr && *instr && isxdigit(*instr))
		{
			val = *instr++ - '0';
			if (9 < val)
				val -= 7;
			tot <<= 4;
			tot |= (val & 0x0f);
		}
	}
	else
		tot = atoi( instr );

	return tot;
}

// Trim trailing spaces from a string
void trimstr(char *s)
{
    char *p;

    p = s + (strlen(s) - 1);
    while (isspace(*p))
        p--;
    *(++p) = '\0';
}



//-------------------------------------------------------------
// CfgGet
//
// Retrieve a parameter from an INI file. Returns a status code
// and the paramter string in retbuff.
//-------------------------------------------------------------
bool CfgGet ( FILE* inifp,
               char  *inisect,
               char  *iniparm,
               char  *retbuff,
               short bufflen,
               short *parmlen)
{
    short gotsect;
    char  tbuf[256];
    char  *ss, *eq, *ps, *vs, *ptr;

	rewind( inifp );

    // find the target section

    gotsect = 0;
    while (fgets(tbuf,256,inifp) != NULL) {
        if ((ss = strchr(tbuf,'[')) != NULL) {
            if (strncasecmp(ss+1,inisect,strlen(inisect)) == 0) {
                gotsect = 1;
                break;
                }
            }
        }

    if (!gotsect) {                             // section not found
        return false;
        }

    while (fgets(tbuf,256,inifp) != NULL) {     // find parameter in sect

        if ((ptr = strrchr(tbuf,'\n')) != NULL) // remove newline if there
            *ptr = '\0';

        ps = tbuf+strspn(tbuf," \t");           // find the first non-blank

        if (*ps == ';')                         // Skip line if comment
            continue;

        if (*ps == '[') {                       // Start of next section
            return false;
            }

        eq = strchr(ps,'=');                    // Find '=' sign in string

        if (eq)
            vs = eq + 1 + strspn(eq+1," \t");   // Find start of value str
        else
            continue;

        // found the target parameter

        if (strncasecmp(ps,iniparm,strlen(iniparm)) == 0) {

            if ((ptr = strchr(vs,';')) != NULL) // cut off an EOL comment
                *ptr = '\0';

            if (short(strlen(vs)) > bufflen - 1) {// not enough buffer space
                strncpy(retbuff,vs,bufflen - 1);
                retbuff[bufflen - 1] = '\0';    // put EOL in string
                *parmlen = bufflen;
                if (verbose) printf("Configuration %s.%s = %s\n",inisect,iniparm,retbuff);
                return true;
                }
            else {
                strcpy(retbuff,vs);             // got it
                trimstr(retbuff);               // trim any trailing blanks
                *parmlen = strlen(retbuff);
                if (verbose) printf("Configuration %s.%s = %s\n",inisect,iniparm,retbuff);
                return true;
                }
            }
        }

    return false;                         // parameter not found
}

// Initializes internal variables to their default value and reads the parameters in the
// specified INI file. Then initializes the camera using current settings. If BaseAddress
// or RegOffset parameters are specified (not equal to -1) then one or both of these
// values are used for the m_BaseAddress and m_RegisterOffset properties overriding those
// settings in the INI file.
long config_load( char* cfgname, short BaseAddress, short RegOffset )
{
    short plen;
    char retbuf[256];
    int m_BaseAddress;

    if ((strlen(cfgname) == 0) || (cfgname[0] == '\0')) return CCD_OPEN_CFGNAME;

    // attempt to open INI file
    FILE* inifp = NULL;

	if ((inifp = fopen(cfgname,"r")) == NULL) return CCD_OPEN_CFGDATA;


    // System
    if (CfgGet (inifp, "system", "interface", retbuf, sizeof(retbuf), &plen))
	{
		cam = new CCameraIO();

		if ( cam == NULL )
		{
			fclose( inifp );
			return CCD_OPEN_ALLOC;
		}
	}
    else
	{
		fclose( inifp );
		return CCD_OPEN_CFGDATA;
	}

	/////////////////////////////////////////////////////////////////////////////////
	// Settings which are stored in a class member (not in firmware) are already set
	// to a default value in the constructor. Settings accessed by get/put functions
	// must be set to a default value in this routine, after the base address and
	// communication protocal is setup.

	/////////////////////////////////////////////////////////////////////////////////
	// These settings must done first since they affect communication with the camera
	// In the Linux drivers this is taken care of by the /dev/apppi0 device  file
	// so we just set a dummy m_BaseAddress instead
		if ( BaseAddress == -1 )
		{
			if (CfgGet (inifp, "system", "base", retbuf, sizeof(retbuf), &plen))
				m_BaseAddress = hextoi(retbuf) & 0xFFF;
			else
			{
				fclose( inifp );
				delete cam;
				cam = NULL;
				return CCD_OPEN_CFGDATA;           // base address MUST be defined
			}
		}
		else
			m_BaseAddress = BaseAddress & 0xFFF;

		if ( RegOffset == -1 )
		{
			if (CfgGet (inifp, "system", "reg_offset", retbuf, sizeof(retbuf), &plen))
			{
				unsigned short val = hextoi(retbuf);
				cam->m_RegisterOffset = val & 0xF0;
			}
		}
		else
		{
			if ( RegOffset >= 0x0 && RegOffset <= 0xF0 ) cam->m_RegisterOffset = RegOffset & 0xF0;
		}

		/////////////////////////////////////////////////////////////////////////////////
		// Necessary geometry settings

		if (CfgGet (inifp, "geometry", "rows", retbuf, sizeof(retbuf), &plen))
		{
			short val = hextoi(retbuf);
			if ( val >= 1 && val <= MAXTOTALROWS ) cam->m_Rows = val;
		}
		else
		{
			fclose( inifp );
			delete cam;
			cam = NULL;
			return CCD_OPEN_CFGDATA;           // rows MUST be defined
		}

		if (CfgGet (inifp, "geometry", "columns", retbuf, sizeof(retbuf), &plen))
		{
			short val = hextoi(retbuf);
			if ( val >= 1 && val <= MAXTOTALCOLUMNS ) cam->m_Columns = val;
		}
		else
		{
			fclose( inifp );
			delete cam;
			cam = NULL;
			return CCD_OPEN_CFGDATA;           // columns MUST be defined
		}

		/////////////////////////////////////////////////////////////////////////////////

		if (CfgGet (inifp, "system", "pp_repeat", retbuf, sizeof(retbuf), &plen))
		{
			short val = hextoi( retbuf );
			if ( val > 0 && val <= 1000 ) cam->m_PPRepeat = val;
		}

		/////////////////////////////////////////////////////////////////////////////////
		// First actual communication with camera if in PPI mode
		if ( !cam->InitDriver(0) )
		{
			delete cam;
			cam = NULL;
			fclose( inifp );
			return CCD_OPEN_LOOPTST;
		}
		/////////////////////////////////////////////////////////////////////////////////
		// First actual communication with camera if in ISA mode
		cam->Reset();	// Read in command register to set shadow register known state
		/////////////////////////////////////////////////////////////////////////////////

		if (CfgGet (inifp, "system", "cable", retbuf, sizeof(retbuf), &plen))
		{
			if (!strcmp("LONG",retbuf))
				cam->write_LongCable( true );
			else if ( !strcmp("SHORT",retbuf) )
				cam->write_LongCable( false );
		}
		else
			cam->write_LongCable( false );	// default

		if ( !cam->read_Present() )
		{
			delete cam;
			cam = NULL;
			fclose( inifp );

			return CCD_OPEN_LOOPTST;
		}
	/////////////////////////////////////////////////////////////////////////////////
	// Set default setting and read other settings from ini file

	cam->write_UseTrigger( false );
	cam->write_ForceShutterOpen( false );

	if (CfgGet (inifp, "system", "high_priority", retbuf, sizeof(retbuf), &plen))
	{
		if (!strcmp("ON",retbuf) || !strcmp("TRUE",retbuf) || !strcmp("1",retbuf))
		{
			cam->m_HighPriority = true;
		}
		  else if (!strcmp("OFF",retbuf) || !strcmp("FALSE",retbuf) || !strcmp("0",retbuf))
		{
			cam->m_HighPriority = false;
		}
	}

	if (CfgGet (inifp, "system", "data_bits", retbuf, sizeof(retbuf), &plen))
	{
		short val = hextoi( retbuf );
		if ( val >= 8 && val <= 18 ) cam->m_DataBits = val;
	}

	if (CfgGet (inifp, "system", "sensor", retbuf, sizeof(retbuf), &plen))
	{
		if ( strcmp( "ccd", retbuf ) == 0 )
		{
			cam->m_SensorType = Camera_SensorType_CCD;
		}
		else if ( strcmp( "cmos", retbuf ) == 0 )
		{
			cam->m_SensorType = Camera_SensorType_CMOS;
		}
	}

    if (CfgGet (inifp, "system", "mode", retbuf, sizeof(retbuf), &plen))
	{
        unsigned short val = hextoi(retbuf) & 0xF;
        cam->write_Mode( val );
    }
	else
		cam->write_Mode( 0 );			// default

    if (CfgGet (inifp, "system", "test", retbuf, sizeof(retbuf), &plen))
	{
        unsigned short val = hextoi(retbuf) & 0xF;
        cam->write_TestBits( val );
    }
	else
		cam->write_TestBits( 0 );		//default

    if (CfgGet (inifp, "system", "test2", retbuf, sizeof(retbuf), &plen))
	{
        unsigned short val = hextoi(retbuf) & 0xF;
        cam->write_Test2Bits( val );
    }
	else
		cam->write_Test2Bits( 0 );	// default

	cam->write_FastReadout( false );	//default

    if (CfgGet (inifp, "system", "shutter_speed", retbuf, sizeof(retbuf), &plen))
	{
        if (!strcmp("normal",retbuf))
		{
			cam->m_FastShutter = false;
			cam->m_MaxExposure = 10485.75;
			cam->m_MinExposure = 0.01;
		}
		else if (!strcmp("fast",retbuf))
		{
			cam->m_FastShutter = true;
			cam->m_MaxExposure = 1048.575;
			cam->m_MinExposure = 0.001;
		}
		else if ( !strcmp("dual",retbuf))
		{
			cam->m_FastShutter = true;
			cam->m_MaxExposure = 10485.75;
			cam->m_MinExposure = 0.001;
		}
    }

    if (CfgGet (inifp, "system", "shutter_bits", retbuf, sizeof(retbuf), &plen))
	{
		unsigned short val = hextoi(retbuf);
		cam->m_FastShutterBits_Mode = val & 0x0F;
		cam->m_FastShutterBits_Test = ( val & 0xF0 ) >> 4;
	}

    if (CfgGet (inifp, "system", "maxbinx", retbuf, sizeof(retbuf), &plen))
	{
        short val = hextoi(retbuf);
        if ( val >= 1 && val <= MAXHBIN ) cam->m_MaxBinX = val;
	}

    if (CfgGet (inifp, "system", "maxbiny", retbuf, sizeof(retbuf), &plen))
	{
        short val = hextoi(retbuf);
        if ( val >= 1 && val <= MAXVBIN ) cam->m_MaxBinY = val;
	}

    if (CfgGet (inifp, "system", "guider_relays", retbuf, sizeof(retbuf), &plen))
	{
		if (!strcmp("ON",retbuf) || !strcmp("TRUE",retbuf) || !strcmp("1",retbuf))
		{
			cam->m_GuiderRelays = true;
		}
		  else if (!strcmp("OFF",retbuf) || !strcmp("FALSE",retbuf) || !strcmp("0",retbuf))
		{
			cam->m_GuiderRelays = false;
		}
	}

    if (CfgGet (inifp, "system", "timeout", retbuf, sizeof(retbuf), &plen))
	{
        double val = atof(retbuf);
		if ( val >= 0.0 && val <= 10000.0 ) cam->m_Timeout = val;
    }

	/////////////////////////////////////////////////////////////////////////////////
    // Geometry

    if (CfgGet (inifp, "geometry", "bic", retbuf, sizeof(retbuf), &plen))
	{
        short val = hextoi(retbuf);
        if ( val >= 1 && val <= MAXCOLUMNS ) cam->m_BIC = val;
	}

    if (CfgGet (inifp, "geometry", "bir", retbuf, sizeof(retbuf), &plen))
	{
        short val = hextoi(retbuf);
        if ( val >= 1 && val <= MAXROWS ) cam->m_BIR = val;
	}

	if (CfgGet (inifp, "geometry", "skipc", retbuf, sizeof(retbuf), &plen))
	{
        short val = hextoi(retbuf);
        if ( val >= 0 && val <= MAXCOLUMNS ) cam->m_SkipC = val;
	}

	if (CfgGet (inifp, "geometry", "skipr", retbuf, sizeof(retbuf), &plen))
	{
        short val = hextoi(retbuf);
        if ( val >= 0 && val <= MAXROWS ) cam->m_SkipR = val;
	}

    if (CfgGet (inifp, "geometry", "imgcols", retbuf, sizeof(retbuf), &plen))
	{
        short val = hextoi(retbuf);
        if ( val >= 1 && val <= MAXTOTALCOLUMNS ) cam->m_ImgColumns = val;
	}
	else
		cam->m_ImgColumns = cam->m_Columns - cam->m_BIC - cam->m_SkipC;

    if (CfgGet (inifp, "geometry", "imgrows", retbuf, sizeof(retbuf), &plen))
	{
        short val = hextoi(retbuf);
        if ( val >= 1 && val <= MAXTOTALROWS ) cam->m_ImgRows = val;
	}
	else
		cam->m_ImgRows = cam->m_Rows - cam->m_BIR - cam->m_SkipR;

    if (CfgGet (inifp, "geometry", "hflush", retbuf, sizeof(retbuf), &plen))
	{
        short val = hextoi(retbuf);
        if ( val >= 1 && val <= MAXHBIN ) cam->m_HFlush = val;
	}

    if (CfgGet (inifp, "geometry", "vflush", retbuf, sizeof(retbuf), &plen))
	{
        short val = hextoi(retbuf);
        if ( val >= 1 && val <= MAXVBIN ) cam->m_VFlush = val;
	}

	// Default to full frame
	cam->m_NumX = cam->m_ImgColumns;
	cam->m_NumY = cam->m_ImgRows;

	/////////////////////////////////////////////////////////////////////////////////
	// Temperature

    if (CfgGet (inifp, "temp", "control", retbuf, sizeof(retbuf), &plen))
	{
		if (!strcmp("ON",retbuf) || !strcmp("TRUE",retbuf) || !strcmp("1",retbuf))
		{
			cam->m_TempControl = true;
		}
		  else if (!strcmp("OFF",retbuf) || !strcmp("FALSE",retbuf) || !strcmp("0",retbuf))
		{
			cam->m_TempControl = false;
		}
    }

    if (CfgGet (inifp, "temp", "cal", retbuf, sizeof(retbuf), &plen))
	{
        short val = hextoi(retbuf);
		if ( val >= 1 && val <= 255 ) cam->m_TempCalibration = val;
    }

    if (CfgGet (inifp, "temp", "scale", retbuf, sizeof(retbuf), &plen))
	{
        double val = atof(retbuf);
		if ( val >= 1.0 && val <= 10.0 ) cam->m_TempScale = val;
    }

    if (CfgGet (inifp, "temp", "target", retbuf, sizeof(retbuf), &plen))
	{
        double val = atof(retbuf);
        if ( val >= -60.0 && val <= 40.0 )
			cam->write_CoolerSetPoint( val );
		else
			cam->write_CoolerSetPoint( -10.0 );
    }
	else
		cam->write_CoolerSetPoint( -10.0 );	//default

	/////////////////////////////////////////////////////////////////////////////////
	// CCD

	if (CfgGet (inifp, "ccd", "sensor", retbuf, sizeof(retbuf), &plen))
	{
		if ( plen > 256 ) plen = 256;
		memcpy( cam->m_Sensor, retbuf, plen );
    }

	if (CfgGet (inifp, "ccd", "color", retbuf, sizeof(retbuf), &plen))
	{
		if (!strcmp("ON",retbuf) || !strcmp("TRUE",retbuf) || !strcmp("1",retbuf))
		{
			cam->m_Color = true;
		}
		  else if (!strcmp("OFF",retbuf) || !strcmp("FALSE",retbuf) || !strcmp("0",retbuf))
		{
			cam->m_Color = false;
		}
    }

    if (CfgGet (inifp, "ccd", "noise", retbuf, sizeof(retbuf), &plen))
	{
		cam->m_Noise = atof( retbuf );
    }

	if (CfgGet (inifp, "ccd", "gain", retbuf, sizeof(retbuf), &plen))
	{
		cam->m_Gain = atof( retbuf );
    }

    if (CfgGet (inifp, "ccd", "pixelxsize", retbuf, sizeof(retbuf), &plen))
	{
		cam->m_PixelXSize = atof( retbuf );
    }

    if (CfgGet (inifp, "ccd", "pixelysize", retbuf, sizeof(retbuf), &plen))
	{
		cam->m_PixelYSize = atof( retbuf );
    }

	fclose( inifp );
    return CCD_OPEN_NOERR;
}



