/* FPACK utility routines
 * R. Seaman, NOAO & W. Pence, NASA/GSFC
 */

#include <time.h>
#include <float.h>
#include <signal.h>
#include "fitsio.h"
#include "fpack.h"
#include <sys/time.h>
#include <time.h>
#include <stdlib.h>

/* nearest integer function */
# define NINT(x)  ((x >= 0.) ? (int) (x + 0.5) : (int) (x - 0.5))
# define NSHRT(x) ((x >= 0.) ? (short) (x + 0.5) : (short) (x - 0.5))

/* define variables for measuring elapsed time */
clock_t scpu, ecpu;
long startsec;   /* start of elapsed time interval */
int startmilli;  /* start of elapsed time interval */

/*  CLOCKS_PER_SEC should be defined by most compilers */
#if defined(CLOCKS_PER_SEC)
#define CLOCKTICKS CLOCKS_PER_SEC
#else
/* on SUN OS machine, CLOCKS_PER_SEC is not defined, so set its value */
#define CLOCKTICKS 1000000
#endif

/* structure to hold image statistics (defined in fpack.h) */
imgstats imagestats;
FILE *outreport;

/* dimension of central image area to be sampled for test statistics */
int XSAMPLE = 4100;
int YSAMPLE = 4100;
    
int fp_msg (char *msg) { printf ("%s", msg); }
int fp_version (void) { fp_msg (FPACK_VERSION); fp_msg ("\n"); }
int fp_noop (void) { fp_msg ("Input and output files are unchanged.\n"); }

/*--------------------------------------------------------------------------*/
int fp_init (fpstate *fpptr)
{
	int	ii;

	fpptr->comptype = RICE_1;
	fpptr->quantize_level = DEF_QLEVEL;
	fpptr->scale = DEF_HCOMP_SCALE;
	fpptr->smooth = DEF_HCOMP_SMOOTH;
	fpptr->rescale_noise = DEF_RESCALE_NOISE;
	fpptr->ntile[0] = (long) 0;	/* 0 means extent of axis */

	for (ii=1; ii < MAX_COMPRESS_DIM; ii++)
	    fpptr->ntile[ii] = (long) 1;

	fpptr->to_stdout = 0;
	fpptr->listonly = 0;
	fpptr->clobber = 0;
	fpptr->delete_input = 0;
	fpptr->do_not_prompt = 0;
	fpptr->do_checksums = 1;
	fpptr->do_gzip_file = 0;
	fpptr->test_all = 0;
	fpptr->verbose = 0;

	fpptr->prefix[0] = (char) NULL;
	fpptr->delete_suffix = 0;
	fpptr->outfile[0] = (char) NULL;

	fpptr->firstfile = 1;

	/* magic number for initialization check, boolean for preflight
	 */
	fpptr->initialized = FP_INIT_MAGIC;
	fpptr->preflight_checked = 0;
	return(0);
}

/*--------------------------------------------------------------------------*/
int fp_list (int argc, char *argv[], fpstate fpvar)
{
	fitsfile *infptr;
	char	infits[SZ_STR];
	int	hdunum, iarg, stat=0;

	if (fpvar.initialized != FP_INIT_MAGIC) {
	    fp_msg ("Error: internal initialization error\n"); exit (-1);
	}

	for (iarg=fpvar.firstfile; iarg < argc; iarg++) {
	    strncpy (infits, argv[iarg], SZ_STR);

	    if (strchr (infits, '[') || strchr (infits, ']')) {
		fp_msg ("Error: section/extension notation not supported: ");
		fp_msg (infits); fp_msg ("\n"); exit (-1);
	    }

	    if (access (infits, R_OK) != 0) {
		fp_msg ("Error: can't find or open input file ");
		fp_msg (infits); fp_msg ("\n"); exit (-1);
	    }

	    fits_open_file (&infptr, infits, READONLY, &stat);
	    if (stat) { fits_report_error (stderr, stat); exit (stat); }

	    fp_info (infits);

	    fits_get_num_hdus (infptr, &hdunum, &stat);
	    if (stat) { fits_report_error (stderr, stat); exit (stat); }

	    fp_info_hdu (infptr);

	    fits_close_file (infptr, &stat);
	    if (stat) { fits_report_error (stderr, stat); exit (stat); }
	}
	return(0);
}

/*--------------------------------------------------------------------------*/
int fp_info (char *infits)
{
	struct  stat    sbuf;
	char	msg[SZ_STR];
	int     mtime, size, uid, gid, nlink;
	unsigned mode;

	if (stat (infits, &sbuf) != 0) {
	    fp_msg ("Error: can't stat "); fp_msg (infits); fp_msg ("\n");

	} else {
            size = (int) sbuf.st_size;
            mtime = (int) sbuf.st_mtime;
            uid = (int) sbuf.st_uid;
            gid = (int) sbuf.st_gid;
            mode = (unsigned) sbuf.st_mode;
            nlink = (int) sbuf.st_nlink;

	    sprintf (msg, "%s: %d bytes\n", infits, size); fp_msg (msg);
	}
	return(0);
}

/*--------------------------------------------------------------------------*/
int fp_info_hdu (fitsfile *infptr)
{
	long	naxes[9] = {1, 1, 1, 1, 1, 1, 1, 1, 1};
	char	msg[SZ_STR], val[SZ_CARD], com[SZ_CARD];
	int	naxis=0, hdutype, bitpix, hdupos, stat=0, ii;

	fits_movabs_hdu (infptr, 1, NULL, &stat);
	if (stat) { fits_report_error (stderr, stat); exit (stat); }

	for (hdupos=1; ! stat; hdupos++) {
	    fits_get_hdu_type (infptr, &hdutype, &stat);
/*	    fits_read_keyword (infptr, FILE_KEY, val, com, &stat);
 */
	    if (stat) { fits_report_error (stderr, stat); exit (stat); }

	    if (hdutype == IMAGE_HDU) {
		sprintf (msg, "  %d IMAGE", hdupos); fp_msg (msg);

/*		sprintf (msg, "  %d IMAGE %s", hdupos, val); fp_msg (msg);
 */
		fits_get_img_param (infptr, 9, &bitpix, &naxis, naxes, &stat);

		if (naxis == 0) {
		    sprintf (msg, " [no pixels]"); fp_msg (msg);
		} else if (naxis == 1) {
		    sprintf (msg, " [%d]", naxes[1]); fp_msg (msg);
		} else {
		    sprintf (msg, " [%d", naxes[0]); fp_msg (msg);
		    for (ii=1; ii < naxis; ii++) {
			sprintf (msg, "x%d", naxes[ii]); fp_msg (msg);
		    }
		    fp_msg ("]");
		}

		if (fits_is_compressed_image (infptr, &stat))
		    fp_msg (" (compressed)\n");
		else
		    fp_msg ("\n");

	    } else if (hdutype == ASCII_TBL) {
		sprintf (msg, "  %d ASCII TABLE\n", hdupos); fp_msg (msg);

	    } else if (hdutype == BINARY_TBL) {
		sprintf (msg, "  %d BINARY TABLE\n", hdupos); fp_msg (msg);

	    } else {
		sprintf (msg, "  %d UNKNOWN EXTENSION\n", hdupos); fp_msg (msg);
	    }

	    fits_movrel_hdu (infptr, 1, NULL, &stat);
	}
	return(0);
}

/*--------------------------------------------------------------------------*/
int fp_preflight (int argc, char *argv[], int unpack, fpstate *fpptr)
{
	char	infits[SZ_STR], outfits[SZ_STR], temp[SZ_STR], *cptr;
	int	iarg, suflen, namelen, nfiles = 0;

	if (fpptr->initialized != FP_INIT_MAGIC) {
	    fp_msg ("Error: internal initialization error\n"); exit (-1);
	}

	for (iarg=fpptr->firstfile; iarg < argc; iarg++) {

            outfits[0] = '\0';
	    
            if (strlen(argv[iarg]) > SZ_STR - 4) {  /* allow for .fz or .gz suffix */
		    fp_msg ("Error: input file name\n   "); fp_msg (argv[iarg]);
		    fp_msg ("\n   is too long\n"); fp_noop (); exit (-1);
	    }

	    strncpy (infits, argv[iarg], SZ_STR);

	    if (strchr (infits, '[') || strchr (infits, ']')) {
		fp_msg ("Error: section/extension notation not supported: ");
		fp_msg (infits); fp_msg ("\n"); fp_noop (); exit (-1);
	    }
	    
            if (unpack) {
	  	/* ********** This section applies to funpack ************ */

	      /* check that input file  exists */
	      if (access (infits, R_OK) != 0) {  /* if not, then check if */
		    strcat(infits, ".fz");       /* a .fz version exsits */
	            if (access (infits, R_OK) != 0) {
                        namelen = strlen(infits);
                        infits[namelen - 3] = '\0';  /* remove the .fz suffix */
		        fp_msg ("Error: can't find or read input file "); fp_msg (infits);
		        fp_msg ("\n"); fp_noop (); exit (-1);
                    }
	      } else {   /* make sure a .fz version of the same file doesn't exist */
                    namelen = strlen(infits);
		    strcat(infits, ".fz");   
	            if (access (infits, R_OK) == 0) {
                        infits[namelen] = '\0';  /* remove the .fz suffix */
		        fp_msg ("Error: ambiguous input file name.  Which file should be unpacked?:\n  ");
		        fp_msg (infits); fp_msg ("\n  "); 
		        fp_msg (infits); fp_msg (".fz\n"); 
		        fp_noop (); exit (-1);
                    } else {
                        infits[namelen] = '\0';  /* remove the .fz suffix */
		    }
	      }

              /* if writing to stdout or just testing, then we are all done */
	      if (fpptr->to_stdout || fpptr->test_all) {
                      continue;
	      }

              if (fpptr->outfile[0]) {  /* user specified output file name */
	          nfiles++;
		  if (nfiles > 1) {
		      fp_msg ("Error: cannot use same output file name for multiple files:\n   ");
		      fp_msg (fpptr->outfile);
		      fp_msg ("\n"); fp_noop (); exit (-1);
	          }

                  /* check that output file doesn't exist */
	          if (access (fpptr->outfile, F_OK) == 0) {
		            fp_msg ("Error: output file already exists:\n "); 
			    fp_msg (fpptr->outfile);
		            fp_msg ("\n "); fp_noop (); exit (-1);
		  } 
                  continue;
	      }     

	      /* construct output file name to test */
	      if (fpptr->prefix[0]) {
                  if (strlen(fpptr->prefix) + strlen(infits) > SZ_STR - 1) {
		      fp_msg ("Error: output file name for\n   "); fp_msg (infits);
		      fp_msg ("\n   is too long with the prefix\n"); fp_noop (); exit (-1);
	          }
	          strcat(outfits,fpptr->prefix);
	      }

	      strcat(outfits, infits);

	      /* remove .gz suffix, if present (output is not gzipped) */
              namelen = strlen(outfits);
	      if ( !strcmp(".gz", outfits + namelen - 3) ) {
                        outfits[namelen - 3] = '\0';
	      }

	      /* check for .fz suffix that is sometimes required */
	      /* and remove it if present */
              namelen = strlen(outfits);
	      if ( !strcmp(".fz", outfits + namelen - 3) ) { /* suffix is present */
                        outfits[namelen - 3] = '\0';
	      } else if (fpptr->delete_suffix) {  /* required suffix is missing */
		    fp_msg ("Error: input compressed file "); fp_msg (infits);
		    fp_msg ("\n does not have the default .fz suffix.\n"); 
		    fp_noop (); exit (-1);
	      }

	      /* if infits != outfits, make sure outfits doesn't already exist */
              if (strcmp(infits, outfits)) {
	                if (access (outfits, F_OK) == 0) {
		            fp_msg ("Error: output file already exists:\n "); fp_msg (outfits);
		            fp_msg ("\n "); fp_noop (); exit (-1);
		        }       
	      }

	      /* if gzipping the output, make sure .gz file doesn't exist */
	      if (fpptr->do_gzip_file) {
	                strcat(outfits, ".gz");
	                if (access (outfits, F_OK) == 0) {
		            fp_msg ("Error: output file already exists:\n "); fp_msg (outfits);
		            fp_msg ("\n "); fp_noop (); exit (-1);
		        }       
              		namelen = strlen(outfits);
                        outfits[namelen - 3] = '\0';  /* remove the .gz suffix again */
	      }
	  } else {
	  	/* ********** This section applies to fpack ************ */

	      /* check that input file  exists */
	      if (access (infits, R_OK) != 0) {  /* if not, then check if */
		    strcat(infits, ".gz");     /* a gzipped version exsits */
	            if (access (infits, R_OK) != 0) {
                        namelen = strlen(infits);
                        infits[namelen - 3] = '\0';  /* remove the .gz suffix */
		        fp_msg ("Error: can't find or read input file "); fp_msg (infits);
		        fp_msg ("\n"); fp_noop (); exit (-1);
                    }
	      }

              /* make sure the file to pack does not already have a .fz suffix */
              namelen = strlen(infits);
	      if ( !strcmp(".fz", infits + namelen - 3) ) {
		        fp_msg ("Error: fpack input file already has '.fz' suffix\n" ); fp_msg (infits);
		        fp_msg ("\n"); fp_noop (); exit (-1);
	      }

              /* if writing to stdout, or just testing the files, then we are all done */
	      if (fpptr->to_stdout || fpptr->test_all) {
                        continue;
	      }

	      /* construct output file name */
	      strcpy(outfits, infits);
	      
	      /* remove .gz suffix, if present (output is not gzipped) */
              namelen = strlen(outfits);
	      if ( !strcmp(".gz", outfits + namelen - 3) ) {
                        outfits[namelen - 3] = '\0';
	      }
	      
	      /* If not clobbering the input file, add .fz suffix to output name */
	      if (! fpptr->clobber)
		        strcat(outfits, ".fz");
			
	      /* if infits != outfits, make sure outfits doesn't already exist */
              if (strcmp(infits, outfits)) {
	                if (access (outfits, F_OK) == 0) {
		            fp_msg ("Error: output file already exists:\n "); fp_msg (outfits);
		            fp_msg ("\n "); fp_noop (); exit (-1);
		        }       
	      }
	  }   /* end of fpack section */
	}

	fpptr->preflight_checked++;
	return(0);
}

/*--------------------------------------------------------------------------*/
/* must run fp_preflight() before fp_loop()
 */
int fp_loop (int argc, char *argv[], int unpack, fpstate fpvar)
{
	char	infits[SZ_STR], outfits[SZ_STR], outfits2[SZ_STR];
	char	temp[SZ_STR], answer[30], *cptr;
	int	iarg, islossless, namelen;
        
	if (fpvar.initialized != FP_INIT_MAGIC) {
	    fp_msg ("Error: internal initialization error\n"); exit (-1);
	} else if (! fpvar.preflight_checked) {
	    fp_msg ("Error: internal preflight error\n"); exit (-1);
	}


	if (fpvar.test_all && fpvar.outfile[0]) {
	    outreport = fopen(fpvar.outfile, "w");
		fprintf(outreport," Filename Extension BITPIX NAXIS1 NAXIS2 Size N_nulls Minval Maxval Mean Sigm Noise1 Noise3 T_whole T_rowbyrow ");
		fprintf(outreport,"[Comp_ratio, Pack_cpu, Unpack_cpu, Lossless readtimes] (repeated for Rice, Hcompress and GZIP)\n");
	}

/* set up signal handler to delete temporary file on abort */	    
#ifdef SIGINT
    if (signal(SIGINT, SIG_IGN) != SIG_IGN) {
	(void) signal(SIGINT,  abort_fpack); 
    }
#endif

#ifdef SIGTERM
    if (signal(SIGTERM, SIG_IGN) != SIG_IGN) {
	(void) signal(SIGTERM,  abort_fpack); 
    }
#endif

#ifdef SIGHUP
    if (signal(SIGHUP, SIG_IGN) != SIG_IGN) {
	(void) signal(SIGHUP,  abort_fpack);
    }
#endif

	for (iarg=fpvar.firstfile; iarg < argc; iarg++) {

          temp[0] = '\0';
	  outfits[0] = '\0';
          islossless = 1;

	  strncpy (infits, argv[iarg], SZ_STR - 1);

          if (unpack) {
	  	/* ********** This section applies to funpack ************ */

	      /* find input file */
	      if (access (infits, R_OK) != 0) {  /* if not, then */
		    strcat(infits, ".fz");       /* a .fz version must exsit */
	      }

	      if (fpvar.to_stdout) {
		strcpy(outfits, "-");

              } else if (fpvar.outfile[0]) {  /* user specified output file name */
	          strcpy(outfits, fpvar.outfile);

	      } else if (! fpvar.test_all) {
	          /* construct output file name */
	          if (fpvar.prefix[0]) {
	              strcat(outfits,fpvar.prefix);
	          }

	          strcat(outfits, infits);

	          /* remove .gz suffix, if present (output is not gzipped) */
                  namelen = strlen(outfits);
	          if ( !strcmp(".gz", outfits + namelen - 3) ) {
                        outfits[namelen - 3] = '\0';
	          }

	          /* check for .fz suffix that is sometimes required */
	          /* and remove it if present */
                  namelen = strlen(outfits);
	          if ( !strcmp(".fz", outfits + namelen - 3) ) { /* suffix is present */
                        outfits[namelen - 3] = '\0';
	          }
	      }

	  } else {
	  	/* ********** This section applies to fpack ************ */

	      if (fpvar.to_stdout) {
		strcpy(outfits, "-");
	      } else if (! fpvar.test_all) {
	          /* construct output file name */
	          strcpy(outfits, infits);
	      
	          /* remove .gz suffix, if present (output is not gzipped) */
                  namelen = strlen(outfits);
	          if ( !strcmp(".gz", outfits + namelen - 3) ) {
                        outfits[namelen - 3] = '\0';
	          }
	      
	          /* If not clobbering the input file, add .fz suffix to output name */
	          if (! fpvar.clobber)
		        strcat(outfits, ".fz");
	      }
	  }

          strncpy(temp, outfits, SZ_STR-1);

          if (!strcmp(infits, outfits) ) {  /* are input and output names the same? */

                /* clobber the input file with the output file with the same name */
                if (! fpvar.clobber) {
                    fp_msg ("\nError: must use -F flag to clobber input file.\n");
		    exit (-1);
		} 

	        /* create temporary output file */
		strcpy (outfits, "fpack_tmp.XXXXXX"); mktemp (outfits);

		if (access (outfits, F_OK) == 0) {
		    /* unlikely name collision, try again (once) */
		    strcpy (outfits, "fpack_tmp.XXXXXX"); mktemp (outfits);

		    if (access (outfits, F_OK) == 0) {
			fp_msg ("Error: temporary file "); fp_msg (outfits);
			fp_msg (" already exists\n"); exit (-1);
		    }
		}
	    }


            /* *************** now do the real work ********************* */
	    
	    if (fpvar.verbose && ! fpvar.to_stdout)
		printf("%s ", infits);
		
	    if (fpvar.test_all) {   /* compare all the algorithms */

		strcpy (outfits,  "fpack_tmp.XXXXXX"); mktemp (outfits);
		strcpy (outfits2, "fpack_tmp.XXXXXX"); mktemp (outfits2);

		fp_test (infits, outfits, outfits2, fpvar);

		remove(outfits);
		remove(outfits2);
                return(0);

	    } else if (unpack) {
		fp_unpack (infits, outfits, fpvar);

	    }  else {
		fp_pack (infits, outfits, fpvar, &islossless);
	    }

	    if (fpvar.to_stdout) {
		return(0);
	    }

            /* ********** clobber and/or delete files, if needed ************** */

            if (!strcmp(infits, temp) && fpvar.clobber ) {  

		if (!islossless && ! fpvar.do_not_prompt) {
		    fp_msg ("\nFile ");
		    fp_msg (infits); 
		    fp_msg ("\nwas compressed with a LOSSY method.  Overwrite the\n");
		    fp_msg ("original file with the compressed version? (Y/N) ");
		    fgets(answer, 29, stdin);
		    if (answer[0] != 'Y' && answer[0] != 'y') {
		        fp_msg ("\noriginal file NOT overwritten!\n");
			remove(outfits);
                        break;
		    }
		}
 
	        /* rename clobbers input, may be unix/shell version dependent */
		if (rename (outfits, temp) != 0) {
		        fp_msg ("\nError renaming tmp file to ");
		        fp_msg (temp); fp_msg ("\n"); exit (-1);
		}
                strcpy(outfits, temp);

	    } else if (fpvar.clobber || fpvar.delete_input) {      /* delete the input file */
	         if (!islossless && !fpvar.do_not_prompt) {
		    fp_msg ("\nFile ");
		    fp_msg (infits); 
		    fp_msg ("\nwas compressed with a LOSSY method.  \n");
		    fp_msg ("Delete the original file? (Y/N) ");
		    fgets(answer, 29, stdin);
		    if (answer[0] != 'Y' && answer[0] != 'y') {
		        fp_msg ("\noriginal file NOT deleted!\n");
		    } else if (remove(infits) != 0) {
		        fp_msg ("\nError deleting input file ");
		        fp_msg (infits); fp_msg ("\n"); exit (-1);
		    }
		  } else {
		     if (remove(infits) != 0) {
		        fp_msg ("\nError deleting input file ");
		        fp_msg (infits); fp_msg ("\n"); exit (-1);
		     }
		  }
	    }

	    if (fpvar.do_gzip_file) {       /* gzip the output file */
		strcpy(temp, "gzip -1 ");
		strcat(temp,outfits);
                system(temp);
	        strcat(outfits, ".gz");    /* only possibible with funpack */
	    }

	    if (fpvar.verbose && ! fpvar.to_stdout)
		printf("-> %s\n", outfits);

	}

	if (fpvar.test_all && fpvar.outfile[0])
	    fclose(outreport);
	return(0);
}

/*--------------------------------------------------------------------------*/
/* fp_pack assumes the output file does not exist (checked by preflight)
 */
int fp_pack (char *infits, char *outfits, fpstate fpvar, int *islossless)
{
	fitsfile *infptr, *outfptr;
	int	stat=0;

	fits_open_file (&infptr, infits, READONLY, &stat);
	fits_create_file (&outfptr, outfits, &stat);

	if (stat) { fits_report_error (stderr, stat); exit (stat); }

	fits_set_compression_type (outfptr, fpvar.comptype, &stat);
	fits_set_quantize_level (outfptr, fpvar.quantize_level, &stat);
	fits_set_hcomp_scale (outfptr, fpvar.scale, &stat);
	fits_set_hcomp_smooth (outfptr, fpvar.smooth, &stat);
	fits_set_tile_dim (outfptr, 6, fpvar.ntile, &stat);

	if (stat) { fits_report_error (stderr, stat); exit (stat); }
	
	while (! stat) {

	    fp_pack_hdu (infptr, outfptr, fpvar, islossless, &stat);

	    if (fpvar.do_checksums) {
	        fits_write_chksum (outfptr, &stat);
	    }

	    fits_movrel_hdu (infptr, 1, NULL, &stat);
	}

	if (stat == END_OF_FILE) stat = 0;

	/* set checksum for case of newly created primary HDU
	 */
	if (fpvar.do_checksums) {
	    fits_movabs_hdu (outfptr, 1, NULL, &stat);
	    fits_write_chksum (outfptr, &stat);
	}

	fits_close_file (outfptr, &stat);
	fits_close_file (infptr, &stat);

	if (stat) { fits_report_error (stderr, stat); exit (stat); }
	return(0);
}

/*--------------------------------------------------------------------------*/
/* fp_unpack assumes the output file does not exist
 */
int fp_unpack (char *infits, char *outfits, fpstate fpvar)
{
        fitsfile *infptr, *outfptr;
        int     stat=0;

        fits_open_file (&infptr, infits, READONLY, &stat);
        fits_create_file (&outfptr, outfits, &stat);

        if (stat) { fits_report_error (stderr, stat); exit (stat); }

        while (! stat) {
            fp_unpack_hdu (infptr, outfptr, &stat);

	    if (fpvar.do_checksums) {
	        fits_write_chksum (outfptr, &stat);
	    }

            fits_movrel_hdu (infptr, 1, NULL, &stat);
        }

        if (stat == END_OF_FILE) stat = 0;

        /* set checksum for case of newly created primary HDU
         */
	if (fpvar.do_checksums) {
	    fits_movabs_hdu (outfptr, 1, NULL, &stat);
	    fits_write_chksum (outfptr, &stat);
	}

        fits_close_file (outfptr, &stat);
        fits_close_file (infptr, &stat);

        if (stat) { fits_report_error (stderr, stat); exit (stat); }
	return(0);
}

/*--------------------------------------------------------------------------*/
/* fp_test assumes the output files do not exist
 */
int fp_test (char *infits, char *outfits, char *outfits2, fpstate fpvar)
{
	fitsfile *inputfptr, *infptr, *outfptr, *outfptr2, *tempfile;

	long	naxes[9] = {1, 1, 1, 1, 1, 1, 1, 1, 1};
	long	tilesize[9] = {0,1,1,1,1,1,1,1,1};
	int	stat=0, totpix=0, naxis=0, ii, hdutype, bitpix, extnum = 0, len;
	int     tstatus = 0, hdunum, rescale_flag;
	char	dtype[8], dimen[100], tempfilename[30];
	double  bscale, rescale;
	long headstart, datastart, dataend;
	float origdata = 0., whole_cpu, whole_elapse, row_elapse, row_cpu;

	fits_open_file (&inputfptr, infits, READONLY, &stat);
	fits_create_file (&outfptr, outfits, &stat);
	fits_create_file (&outfptr2, outfits2, &stat);

	if (stat) { fits_report_error (stderr, stat); exit (stat); }

	fits_set_quantize_level (outfptr, fpvar.quantize_level, &stat);
	fits_set_hcomp_scale (outfptr, fpvar.scale, &stat);
	fits_set_hcomp_smooth (outfptr, fpvar.smooth, &stat);
	fits_set_tile_dim (outfptr, 6, fpvar.ntile, &stat);

	while (! stat) {
	
	    rescale_flag = 0;
	    
	    /*  LOOP OVER EACH HDU */
	    fits_get_hdu_type (inputfptr, &hdutype, &stat);

	    if (hdutype == IMAGE_HDU) {
	        fits_get_img_param (inputfptr, 9, &bitpix, &naxis, naxes, &stat);
	        for (totpix=1, ii=0; ii < 9; ii++) totpix *= naxes[ii];
	    }

	    if ( !fits_is_compressed_image (inputfptr,  &stat) &&
		hdutype == IMAGE_HDU && naxis != 0 && totpix != 0) {

		/* rescale a scaled integer image to reduce noise? */
		if (fpvar.rescale_noise != 0. && bitpix > 0 && bitpix < LONGLONG_IMG) {

		   tstatus = 0;
		   fits_read_key(inputfptr, TDOUBLE, "BSCALE", &bscale, 0, &tstatus);

		   if (tstatus == 0 && bscale != 1.0) {  /* image must be scaled */

			if (bitpix == LONG_IMG)
			  fp_i4stat(inputfptr, naxis, naxes, &stat);
			else
			  fp_i2stat(inputfptr, naxis, naxes, &stat);

			rescale = imagestats.noise3 / fpvar.rescale_noise;
			if (rescale > 1.0) {
			  
			  /* all the criteria are met, so create a temporary file that */
			  /* contains a rescaled version of the image */
			  
			  strcpy (tempfilename, "fptmp.XXXXXX"); mktemp (tempfilename);
			  fits_create_file(&tempfile, tempfilename, &stat);

			  fits_get_hdu_num(inputfptr, &hdunum);
			  if (hdunum != 1) {

			     /* the input hdu is an image extension, so create dummy primary */
			     fits_create_img(tempfile, 8, 0, naxes, &stat);
			  }

			  fits_copy_header(inputfptr, tempfile, &stat); /* copy the header */
			  
			  /* rescale the data, so that it will compress more efficiently */
			  if (bitpix == LONG_IMG)
			    fp_i4rescale(inputfptr, naxis, naxes, rescale, tempfile, &stat);
			  else
			    fp_i2rescale(inputfptr, naxis, naxes, rescale, tempfile, &stat);

			  /* scale the BSCALE keyword by the inverse factor */

			  bscale = bscale * rescale;  
			  fits_update_key(tempfile, TDOUBLE, "BSCALE", &bscale, 0, &stat);

			  /* rescan the header, to reset the actual scaling parameters */
			  fits_set_hdustruc(tempfile, &stat);

			  infptr = tempfile;
			  rescale_flag = 1;
			}
		   }
		 }

		if (!rescale_flag)   /* just compress the input file, without rescaling */
		   infptr = inputfptr;

		/* compute basic statistics about the input image */
                if (bitpix == BYTE_IMG) {
		   strcpy(dtype, "Int*1");
		   fp_i2stat(infptr, naxis, naxes, &stat);
		} else if (bitpix == SHORT_IMG) {
		   strcpy(dtype, "Int*2");
		   fp_i2stat(infptr, naxis, naxes, &stat);
		} else if (bitpix == LONG_IMG) {
		   strcpy(dtype, "Int*4");
		   fp_i4stat(infptr, naxis, naxes, &stat);
		} else if (bitpix == LONGLONG_IMG) {
		   strcpy(dtype, "Int*8");
		} else if (bitpix == FLOAT_IMG)   {
		   strcpy(dtype, "Real*4");
		   fp_r4stat(infptr, naxis, naxes, &stat);
		} else if (bitpix == DOUBLE_IMG)  {
		   strcpy(dtype, "REAL*8");
		   fp_r4stat(infptr, naxis, naxes, &stat);
		}

		printf("\n File: %s\n", infits);
		printf("  Ext BITPIX Dimensions  Nulls    Min    Max     Mean    Sigma    Noise1    Noise3 TElpN TCPUN TElp1 TCPU1\n");

		printf("  %3d  %s", extnum, dtype);
		sprintf(dimen," (%d", naxes[0]);
		len =strlen(dimen);
		for (ii = 1; ii < naxis; ii++)
		    sprintf(dimen+len,",%d", naxes[ii]);
		strcat(dimen, ")");
		printf("%-12s",dimen);

		fits_get_hduaddr(inputfptr, &headstart, &datastart, &dataend, &stat);
		origdata = (dataend - datastart)/1000000.;

		/* get elapsed and cpu times need to read the uncompressed image */
		fits_read_image_speed (infptr, &whole_elapse, &whole_cpu,
		               &row_elapse, &row_cpu, &stat);

		printf(" %5d %6.0f %6.0f %8.1f %#8.2g %#9.3g %#9.3g %5.3f %5.3f %5.3f %5.3f\n",
		        imagestats.n_nulls, imagestats.minval, imagestats.maxval, 
		      imagestats.mean, imagestats.sigma, imagestats.noise1, 
		      imagestats.noise3, whole_elapse, whole_cpu, row_elapse, row_cpu);

		printf("\n       Type   Ratio       Size (MB)     Pk (Sec) UnPk Exact ElpN CPUN  Elp1  CPU1\n");


		if (fpvar.outfile[0]) {
		    fprintf(outreport,
	" %s  %d %d %d %d %#10.4g %d %#10.4g %#10.4g %#10.4g %#10.4g %#10.4g %#10.4g %#10.4g %#10.4g %#10.4g %#10.4g",
		      infits, extnum, bitpix, naxes[0], naxes[1], origdata, imagestats.n_nulls, imagestats.minval, 
		      imagestats.maxval, imagestats.mean, imagestats.sigma, 
		      imagestats.noise1, imagestats.noise3, whole_elapse, whole_cpu, row_elapse, row_cpu);
		}


		/* test compression ratio and speed for each algorithm */
		fits_set_compression_type (outfptr, RICE_1, &stat);
		fits_set_tile_dim (outfptr, 6, fpvar.ntile, &stat);
		fp_test_hdu(infptr, outfptr, outfptr2, fpvar, &stat);

  		fits_set_compression_type (outfptr, HCOMPRESS_1, &stat);
		fits_set_tile_dim (outfptr, 6, fpvar.ntile, &stat);
		fp_test_hdu(infptr, outfptr, outfptr2, fpvar, &stat);

		fits_set_compression_type (outfptr, GZIP_1, &stat);
		fits_set_tile_dim (outfptr, 6, fpvar.ntile, &stat);
		fp_test_hdu(infptr, outfptr, outfptr2, fpvar, &stat);

                if (bitpix == SHORT_IMG || bitpix == LONG_IMG) {
		  fits_set_compression_type (outfptr, NOCOMPRESS, &stat);
		  fits_set_tile_dim (outfptr, 6, fpvar.ntile, &stat);
		  fp_test_hdu(infptr, outfptr, outfptr2, fpvar, &stat);	  
		}

		if (fpvar.outfile[0])
		    fprintf(outreport,"\n");

		/* delete the temporary file */
		if (rescale_flag)   
		    fits_delete_file (infptr, &stat);

	    } else {
		fits_copy_hdu (inputfptr, outfptr, 0, &stat);
		fits_copy_hdu (inputfptr, outfptr2, 0, &stat);
	    }

	    fits_movrel_hdu (inputfptr, 1, NULL, &stat);
            extnum++;
	}


	if (stat == END_OF_FILE) stat = 0;

	fits_close_file (outfptr2, &stat);
	fits_close_file (outfptr, &stat);
	fits_close_file (inputfptr, &stat);

	if (stat) {
	  fits_report_error (stderr, stat);
	}
	return(0);
}
/*--------------------------------------------------------------------------*/
int fp_pack_hdu (fitsfile *infptr, fitsfile *outfptr, fpstate fpvar,
   int *islossless, int *status)
{
	fitsfile *tempfile;
	long	naxes[9] = {1, 1, 1, 1, 1, 1, 1, 1, 1};
	int	stat=0, totpix=0, naxis=0, ii, hdutype, bitpix;
	int	tstatus, hdunum, rescale_flag = 0;
	double  bscale, rescale;

	if (*status) return(0);

	fits_get_hdu_type (infptr, &hdutype, &stat);

	if (hdutype == IMAGE_HDU) {
	    fits_get_img_param (infptr, 9, &bitpix, &naxis, naxes, &stat);
	    for (totpix=1, ii=0; ii < 9; ii++) totpix *= naxes[ii];
	}

	if (fits_is_compressed_image (infptr,  &stat) ||
	    hdutype != IMAGE_HDU || naxis == 0 || totpix == 0) {

	    fits_copy_hdu (infptr, outfptr, 0, &stat);

	} else {

		/* rescale a scaled integer image to reduce noise? */
		if (fpvar.rescale_noise != 0. && bitpix > 0 && bitpix < LONGLONG_IMG) {

		   tstatus = 0;
		   fits_read_key(infptr, TDOUBLE, "BSCALE", &bscale, 0, &tstatus);
		   if (tstatus == 0 && bscale != 1.0) {  /* image must be scaled */

			if (bitpix == LONG_IMG)
			  fp_i4stat(infptr, naxis, naxes, &stat);
			else
			  fp_i2stat(infptr, naxis, naxes, &stat);

			rescale = imagestats.noise3 / fpvar.rescale_noise;
			if (rescale > 1.0) {
			  
			  /* all the criteria are met, so create a temporary file that */
			  /* contains a rescaled version of the image */
			  
			  fits_create_file(&tempfile, "!fptmpqqqq.qqqr", &stat);

			  fits_get_hdu_num(infptr, &hdunum);
			  if (hdunum != 1) {

			     /* the input hdu is an image extension, so create dummy primary */
			     fits_create_img(tempfile, 8, 0, naxes, &stat);
			  }

			  fits_copy_header(infptr, tempfile, &stat); /* copy the header */
			  
			  /* rescale the data, so that it will compress more efficiently */
			  if (bitpix == LONG_IMG)
			    fp_i4rescale(infptr, naxis, naxes, rescale, tempfile, &stat);
			  else
			    fp_i2rescale(infptr, naxis, naxes, rescale, tempfile, &stat);

			  /* scale the BSCALE keyword by the inverse factor */

			  bscale = bscale * rescale;  
			  fits_update_key(tempfile, TDOUBLE, "BSCALE", &bscale, 0, &stat);

			  /* rescan the header, to reset the actual scaling parameters */
			  fits_set_hdustruc(tempfile, &stat);

			  rescale_flag = 1;
			}
		   }
		}

		if (rescale_flag) {
		    fits_img_compress (tempfile, outfptr, &stat);
		    fits_delete_file  (tempfile, &stat);
		} else {
		    fits_img_compress (infptr, outfptr, &stat);
		}

		if (bitpix < 0 || rescale_flag || 
		    (fpvar.comptype == HCOMPRESS_1 && fpvar.scale != 0.)) {

		    /* compressed image is not identical to original */
		    *islossless = 0;  
		}
	}

	*status = stat;
	return(0);
}

/*--------------------------------------------------------------------------*/
int fp_unpack_hdu (fitsfile *infptr, fitsfile *outfptr, int *status)
{
        int stat=0;

        if (*status) return(0);

        if (fits_is_compressed_image (infptr,  &stat))
            fits_img_decompress (infptr, outfptr, &stat);
        else
            fits_copy_hdu (infptr, outfptr, 0, &stat);

        *status = stat;
	return(0);
}
/*--------------------------------------------------------------------------*/
int fits_read_image_speed (fitsfile *infptr, float *whole_elapse, 
    float *whole_cpu, float *row_elapse, float *row_cpu, int *status)
{
        unsigned char *carray, cnull = 0;
	short *sarray, snull=0;
	int bitpix, naxis, anynull, *iarray, inull = 0;
	long ii, naxes[9], fpixel[9]={1,1,1,1,1,1,1,1,1}, lpixel[9]={1,1,1,1,1,1,1,1,1};
	long inc[9]={1,1,1,1,1,1,1,1,1} ;
	float *earray, enull = 0, filesize;
	double *darray, dnull = 0;
	LONGLONG fpixelll[9];
	
	if (*status) return(*status);

	fits_get_img_param (infptr, 9, &bitpix, &naxis, naxes, status);

	if (naxis != 2)return(*status);
	
	lpixel[0] = naxes[0];
	lpixel[1] = naxes[1];
	
        /* filesize in MB */
	filesize = naxes[0] * abs(bitpix) / 8000000. * naxes[1];

	/* measure time required to read the raw image */
	fits_set_bscale(infptr, 1.0, 0.0, status);
	*whole_elapse = 0.;
	*whole_cpu = 0;

        if (bitpix == BYTE_IMG) {
		carray = calloc(naxes[1]*naxes[0], sizeof(char));

		marktime(status);
		fits_read_subset(infptr, TBYTE, fpixel, lpixel, inc, &cnull, 
		      carray, &anynull, status);
		
		/* get elapsped times */
		gettime(whole_elapse, whole_cpu, status);

		/* now read the image again, row by row */
		if (row_elapse) {
		  marktime(status);
		  for (ii = 0; ii < naxes[1]; ii++) {
		   fpixel[1] = ii+1;
		   fits_read_pix(infptr, TBYTE, fpixel, naxes[0], &cnull, 
		      carray, &anynull, status);
		   }
		   /* get elapsped times */
		   gettime(row_elapse, row_cpu, status);
		}
 		free(carray);
 
	} else if (bitpix == SHORT_IMG) {
		sarray = calloc(naxes[0]*naxes[1], sizeof(short));

		marktime(status);

		fits_read_subset(infptr, TSHORT, fpixel, lpixel, inc, &snull, 
		      sarray, &anynull, status);

		gettime(whole_elapse, whole_cpu, status);   /* get elapsped times */

		/* now read the image again, row by row */
		if (row_elapse) {
		  marktime(status);
		  for (ii = 0; ii < naxes[1]; ii++) {
		   fpixel[1] = ii+1;
		   fits_read_pix(infptr, TSHORT, fpixel, naxes[0], &snull, 
		      sarray, &anynull, status);
		  }
		  /* get elapsped times */
		  gettime(row_elapse, row_cpu, status);
		}

		free(sarray);	

	} else if (bitpix == LONG_IMG) {
		iarray = calloc(naxes[0]*naxes[1], sizeof(int));

		marktime(status);

		fits_read_subset(infptr, TINT, fpixel, lpixel, inc, &inull, 
		      iarray, &anynull, status);
		
		/* get elapsped times */
		gettime(whole_elapse, whole_cpu, status);


		/* now read the image again, row by row */
		if (row_elapse) {
		  marktime(status);
		  for (ii = 0; ii < naxes[1]; ii++) {
		   fpixel[1] = ii+1;
		   fits_read_pix(infptr, TINT, fpixel, naxes[0], &inull, 
		      iarray, &anynull, status);
		  }
		  /* get elapsped times */
		  gettime(row_elapse, row_cpu, status);
		}


 		free(iarray);	

	} else if (bitpix == FLOAT_IMG)   {
		earray = calloc(naxes[1]*naxes[0], sizeof(float));

		marktime(status);

		fits_read_subset(infptr, TFLOAT, fpixel, lpixel, inc, &enull, 
		      earray, &anynull, status);
		
		/* get elapsped times */
		gettime(whole_elapse, whole_cpu, status);

		/* now read the image again, row by row */
		if (row_elapse) {
		  marktime(status);
		  for (ii = 0; ii < naxes[1]; ii++) {
		   fpixel[1] = ii+1;
		   fits_read_pix(infptr, TFLOAT, fpixel, naxes[0], &enull, 
		      earray, &anynull, status);
		  }
		  /* get elapsped times */
		  gettime(row_elapse, row_cpu, status);
		}

 		free(earray);	

	} else if (bitpix == DOUBLE_IMG)  {
		darray = calloc(naxes[1]*naxes[0], sizeof(double));

		marktime(status);

		fits_read_subset(infptr, TDOUBLE, fpixel, lpixel, inc, &dnull, 
		      darray, &anynull, status);
		
		/* get elapsped times */
		gettime(whole_elapse, whole_cpu, status);

		/* now read the image again, row by row */
		if (row_elapse) {
		  marktime(status);
		  for (ii = 0; ii < naxes[1]; ii++) {
		   fpixel[1] = ii+1;
		   fits_read_pix(infptr, TDOUBLE, fpixel, naxes[0], &dnull, 
		      darray, &anynull, status);
		  }
		  /* get elapsped times */
		  gettime(row_elapse, row_cpu, status);
		}

		free(darray);
	}

        if (whole_elapse) *whole_elapse = *whole_elapse / filesize;
        if (row_elapse)   *row_elapse   = *row_elapse / filesize;
        if (whole_cpu)    *whole_cpu    = *whole_cpu / filesize;
        if (row_cpu)      *row_cpu      = *row_cpu / filesize;

	return(*status);
}
/*--------------------------------------------------------------------------*/
int fp_test_hdu (fitsfile *infptr, fitsfile *outfptr, fitsfile *outfptr2, 
	fpstate fpvar, int *status)
{

	int stat = 0, hdutype, comptype, noloss = 0;
        char ctype[20], lossless[4];
	long headstart, datastart, dataend;
	float origdata = 0., compressdata = 0.;
	float compratio = 0., packcpu = 0., unpackcpu = 0., readcpu;
	float elapse, whole_elapse, row_elapse, whole_cpu, row_cpu;
	unsigned long datasum1, datasum2, hdusum;

	if (*status) return(0);

	origdata = 0;
	compressdata = 0;
	compratio = 0.;
	lossless[0] = '\0';

	fits_get_compression_type(outfptr, &comptype, &stat);
	if (comptype == RICE_1)
		strcpy(ctype, "RICE");
	else if (comptype == GZIP_1)
		strcpy(ctype, "GZIP");
	else if (comptype == PLIO_1)
		strcpy(ctype, "PLIO");
	else if (comptype == HCOMPRESS_1)
		strcpy(ctype, "HCOMP");
	else if (comptype == NOCOMPRESS)
		strcpy(ctype, "NONE");
	/* -------------- COMPRESS the image ------------------ */

	marktime(&stat);
	
	fits_img_compress (infptr, outfptr, &stat);

	/* get elapsped times */
	gettime(&elapse, &packcpu, &stat);

	/* get elapsed and cpu times need to read the compressed image */
/*
	if (comptype == HCOMPRESS_1) {
	  fits_read_image_speed (outfptr, &whole_elapse, &whole_cpu, 
	   0, 0, &stat);
	  row_elapse = 0; row_cpu = 0;
	} else 
*/
	  fits_read_image_speed (outfptr, &whole_elapse, &whole_cpu, 
	   &row_elapse, &row_cpu, &stat);
	

        if (!stat) {

		/* -------------- UNCOMPRESS the image ------------------ */
		marktime(&stat);

 	       fits_img_decompress (outfptr, outfptr2, &stat);

		/* get elapsped times */
		gettime(&elapse, &unpackcpu, &stat);

		/* ----------------------------------------------------- */

		/* get sizes of original and compressed images */

		fits_get_hduaddr(infptr, &headstart, &datastart, &dataend, &stat);
		origdata = (dataend - datastart)/1000000.;
		
		fits_get_hduaddr(outfptr, &headstart, &datastart, &dataend, &stat);
		compressdata = (dataend - datastart)/1000000.;

		if (compressdata != 0)
			compratio = (float) origdata / (float) compressdata;

		/* is this uncompressed image identical to the original? */

		fits_get_chksum(infptr, &datasum1, &hdusum, &stat);	    
		fits_get_chksum(outfptr2, &datasum2, &hdusum, &stat);

		if ( datasum1 == datasum2) {
			strcpy(lossless, "Yes");
			noloss = 1;
		} else {
			strcpy(lossless, "No");
		}

		printf("       %-5s %6.2f %7.2f ->%7.2f %7.2f %7.2f %s %5.3f %5.3f %5.3f %5.3f\n", 
			ctype, compratio, origdata, compressdata, 
			packcpu, unpackcpu, lossless, whole_elapse, whole_cpu,
			row_elapse, row_cpu);


		if (fpvar.outfile[0]) {
		    fprintf(outreport," %6.3f %5.2f %5.2f %s %7.3f %7.3f %7.3f %7.3f", 
		       compratio, packcpu, unpackcpu, lossless,  whole_elapse, whole_cpu,
		       row_elapse, row_cpu);
		}

 	       /* delete the output HDUs to concerve disk space */

		fits_delete_hdu(outfptr, &hdutype, &stat);
		fits_delete_hdu(outfptr2, &hdutype, &stat);

	} else {
		printf("       %-5s     (unable to compress image)\n", ctype);
	}

	/* try to recover from any compression errors */
	if (stat == DATA_COMPRESSION_ERR) stat = 0;

	*status = stat;
        return(0);
}

/*--------------------------------------------------------------------------*/
int marktime(int *status)
{
        struct  timeval tv;
/*        struct  timezone tz; */

/*        gettimeofday (&tv, &tz); */
        gettimeofday (&tv, NULL);

	startsec = tv.tv_sec;
        startmilli = tv.tv_usec/1000;

        scpu = clock();

	return( *status );
}
/*--------------------------------------------------------------------------*/
int gettime(float *elapse, float *elapscpu, int *status)
{
        struct  timeval tv;
/*        struct  timezone tz; */
	int stopmilli;
	long stopsec;

/*        gettimeofday (&tv, &tz); */
        gettimeofday (&tv, NULL);
	ecpu = clock();

        stopmilli = tv.tv_usec/1000;
	stopsec = tv.tv_sec;
	
	*elapse = (stopsec - startsec) + (stopmilli - startmilli)/1000.;
	*elapscpu = (ecpu - scpu) * 1.0 / CLOCKTICKS;
/*
printf(" (start: %ld + %d), stop: (%ld + %d) elapse: %f\n ",
startsec,startmilli,stopsec, stopmilli, *elapse);
*/
	return( *status );
}
/*--------------------------------------------------------------------------*/
int fp_i2stat(fitsfile *infptr, int naxis, long *naxes, int *status)
{
/*
    read the central XSAMPLE by YSAMPLE region of pixels in the int*2 image, 
    and then compute basic statistics: min, max, mean, sigma, mean diff, etc.
*/

	long fpixel[9] = {1,1,1,1,1,1,1,1,1};
	long lpixel[9] = {1,1,1,1,1,1,1,1,1};
	long inc[9]    = {1,1,1,1,1,1,1,1,1};
	long i1, i2, npix, ii, ngood, nx, ny;
	short *intarray, minvalue, maxvalue, nullvalue;
	int anynul, tstatus, checknull = 1;
	double mean, sigma, noise1, noise3;
	
         /* select the middle XSAMPLE by YSAMPLE area of the image */
	i1 = naxes[0]/2 - (XSAMPLE/2 - 1);
	i2 = naxes[0]/2 + (XSAMPLE/2);
	if (i1 < 1) i1 = 1;
	if (i2 > naxes[0]) i2 = naxes[0];
	fpixel[0] = i1;
	lpixel[0] = i2;
	nx = i2 - i1 +1;

        if (naxis > 1) {
	    i1 = naxes[1]/2 - (YSAMPLE/2 - 1);
	    i2 = naxes[1]/2 + (YSAMPLE/2);
	    if (i1 < 1) i1 = 1;
	    if (i2 > naxes[1]) i2 = naxes[1];
	    fpixel[1] = i1;
	    lpixel[1] = i2;
	}
	ny = i2 - i1 +1;

	npix = nx * ny;

        /* if there are higher dimensions, read the middle plane of the cube */
	if (naxis > 2) {
	    fpixel[2] = naxes[2]/2 + 1;
	    lpixel[2] = naxes[2]/2 + 1;
	}

	intarray = calloc(npix, sizeof(short));
	if (!intarray) {
		*status = MEMORY_ALLOCATION;
		return(*status);
	}

	/* turn off any scaling of the integer pixel values */
	fits_set_bscale(infptr, 1.0, 0.0, status);

	fits_read_subset_sht(infptr, 0, naxis, naxes, fpixel, lpixel, inc,
	    0, intarray, &anynul, status);

	/* read the null value keyword (BLANK) if present */
	tstatus = 0;
	fits_read_key(infptr, TSHORT, "BLANK", &nullvalue, 0, &tstatus);
	if (tstatus) {
	   nullvalue = 0;
	   checknull = 0;
	}

	/* compute statistics of the image */

        fits_img_stats_short(intarray, nx, ny, checknull, nullvalue, 
	&ngood, &minvalue, &maxvalue, &mean, &sigma, &noise1, &noise3, status);

	imagestats.n_nulls = npix - ngood;
	imagestats.minval = minvalue;
	imagestats.maxval = maxvalue; 
	imagestats.mean = mean; 
	imagestats.sigma = sigma; 
	imagestats.noise1 = noise1; 
	imagestats.noise3 = noise3; 
    
	free(intarray);
	return(*status);
}
/*--------------------------------------------------------------------------*/
int fp_i4stat(fitsfile *infptr, int naxis, long *naxes, int *status)
{
/*
    read the central XSAMPLE by YSAMPLE region of pixels in the int*2 image, 
    and then compute basic statistics: min, max, mean, sigma, mean diff, etc.
*/

	long fpixel[9] = {1,1,1,1,1,1,1,1,1};
	long lpixel[9] = {1,1,1,1,1,1,1,1,1};
	long inc[9]    = {1,1,1,1,1,1,1,1,1};
	long i1, i2, npix, ii, ngood, nx, ny;
	int *intarray, minvalue, maxvalue, nullvalue;
	int anynul, tstatus, checknull = 1;
	double mean, sigma, noise1, noise3;
	
         /* select the middle XSAMPLE by YSAMPLE area of the image */
	i1 = naxes[0]/2 - (XSAMPLE/2 - 1);
	i2 = naxes[0]/2 + (XSAMPLE/2);
	if (i1 < 1) i1 = 1;
	if (i2 > naxes[0]) i2 = naxes[0];
	fpixel[0] = i1;
	lpixel[0] = i2;
	nx = i2 - i1 +1;

        if (naxis > 1) {
	    i1 = naxes[1]/2 - (YSAMPLE/2 - 1);
	    i2 = naxes[1]/2 + (YSAMPLE/2);
	    if (i1 < 1) i1 = 1;
	    if (i2 > naxes[1]) i2 = naxes[1];
	    fpixel[1] = i1;
	    lpixel[1] = i2;
	}
	ny = i2 - i1 +1;

	npix = nx * ny;

        /* if there are higher dimensions, read the middle plane of the cube */
	if (naxis > 2) {
	    fpixel[2] = naxes[2]/2 + 1;
	    lpixel[2] = naxes[2]/2 + 1;
	}

	intarray = calloc(npix, sizeof(int));
	if (!intarray) {
		*status = MEMORY_ALLOCATION;
		return(*status);
	}

	/* turn off any scaling of the integer pixel values */
	fits_set_bscale(infptr, 1.0, 0.0, status);

	fits_read_subset_int(infptr, 0, naxis, naxes, fpixel, lpixel, inc,
	    0, intarray, &anynul, status);

	/* read the null value keyword (BLANK) if present */
	tstatus = 0;
	fits_read_key(infptr, TINT, "BLANK", &nullvalue, 0, &tstatus);
	if (tstatus) {
	   nullvalue = 0;
	   checknull = 0;
	}

	/* compute statistics of the image */

        fits_img_stats_int(intarray, nx, ny, checknull, nullvalue, 
	&ngood, &minvalue, &maxvalue, &mean, &sigma, &noise1, &noise3, status);

	imagestats.n_nulls = npix - ngood;
	imagestats.minval = minvalue;
	imagestats.maxval = maxvalue; 
	imagestats.mean = mean; 
	imagestats.sigma = sigma; 
	imagestats.noise1 = noise1; 
	imagestats.noise3 = noise3; 
    
	free(intarray);
	return(*status);
}
/*--------------------------------------------------------------------------*/
int fp_r4stat(fitsfile *infptr, int naxis, long *naxes, int *status)
{
/*
    read the central XSAMPLE by YSAMPLE region of pixels in the int*2 image, 
    and then compute basic statistics: min, max, mean, sigma, mean diff, etc.
*/

	long fpixel[9] = {1,1,1,1,1,1,1,1,1};
	long lpixel[9] = {1,1,1,1,1,1,1,1,1};
	long inc[9]    = {1,1,1,1,1,1,1,1,1};
	long i1, i2, npix, ii, ngood, nx, ny;
	float *array, minvalue, maxvalue, nullvalue = FLOATNULLVALUE;
	int anynul,checknull = 1;
	double mean, sigma, noise1, noise3;
	
         /* select the middle XSAMPLE by YSAMPLE area of the image */
	i1 = naxes[0]/2 - (XSAMPLE/2 - 1);
	i2 = naxes[0]/2 + (XSAMPLE/2);
	if (i1 < 1) i1 = 1;
	if (i2 > naxes[0]) i2 = naxes[0];
	fpixel[0] = i1;
	lpixel[0] = i2;
	nx = i2 - i1 +1;

        if (naxis > 1) {
	    i1 = naxes[1]/2 - (YSAMPLE/2 - 1);
	    i2 = naxes[1]/2 + (YSAMPLE/2);
	    if (i1 < 1) i1 = 1;
	    if (i2 > naxes[1]) i2 = naxes[1];
	    fpixel[1] = i1;
	    lpixel[1] = i2;
	}
	ny = i2 - i1 +1;

	npix = nx * ny;

        /* if there are higher dimensions, read the middle plane of the cube */
	if (naxis > 2) {
	    fpixel[2] = naxes[2]/2 + 1;
	    lpixel[2] = naxes[2]/2 + 1;
	}

	array = calloc(npix, sizeof(float));
	if (!array) {
		*status = MEMORY_ALLOCATION;
		return(*status);
	}

	fits_read_subset_flt(infptr, 0, naxis, naxes, fpixel, lpixel, inc,
	    nullvalue, array, &anynul, status);

	/* are there any null values in the array? */
	if (!anynul) {
	   nullvalue = 0.;
	   checknull = 0;
	}

	/* compute statistics of the image */

        fits_img_stats_float(array, nx, ny, checknull, nullvalue, 
	&ngood, &minvalue, &maxvalue, &mean, &sigma, &noise1, &noise3, status);

	imagestats.n_nulls = npix - ngood;
	imagestats.minval = minvalue;
	imagestats.maxval = maxvalue; 
	imagestats.mean = mean; 
	imagestats.sigma = sigma; 
	imagestats.noise1 = noise1; 
	imagestats.noise3 = noise3; 
    
	free(array);
	return(*status);
}
/*--------------------------------------------------------------------------*/
int fp_i2rescale(fitsfile *infptr, int naxis, long *naxes, double rescale,
    fitsfile *outfptr, int *status)
{
/*
    divide the integer pixel values in the input file by rescale,
    and write back out to the output file..
*/

	long ii, jj, nelem = 1, nx, ny;
	short *intarray, nullvalue;
	int anynul, tstatus, checknull = 1;
	
	nx = naxes[0];
	ny = 1;
	
	for (ii = 1; ii < naxis; ii++) {
	    ny = ny * naxes[ii];
	}

	intarray = calloc(nx, sizeof(short));
	if (!intarray) {
		*status = MEMORY_ALLOCATION;
		return(*status);
	}

	/* read the null value keyword (BLANK) if present */
	tstatus = 0;
	fits_read_key(infptr, TSHORT, "BLANK", &nullvalue, 0, &tstatus);
	if (tstatus) {
	   checknull = 0;
	}

	/* turn off any scaling of the integer pixel values */
	fits_set_bscale(infptr,  1.0, 0.0, status);
	fits_set_bscale(outfptr, 1.0, 0.0, status);

	for (ii = 0; ii < ny; ii++) {

	    fits_read_img_sht(infptr, 1, nelem, nx,
	        0, intarray, &anynul, status);

	    if (checknull) {
	        for (jj = 0; jj < nx; jj++) {
	            if (intarray[jj] != nullvalue)
		        intarray[jj] = NSHRT( (intarray[jj] / rescale) );
		}
	    } else {
	        for (jj = 0; jj < nx; jj++)
		        intarray[jj] = NSHRT( (intarray[jj] / rescale) );
	    }

	    fits_write_img_sht(outfptr, 1, nelem, nx, intarray, status);
	      
	    nelem += nx;
	}

	free(intarray);
	return(*status);
}
/*--------------------------------------------------------------------------*/
int fp_i4rescale(fitsfile *infptr, int naxis, long *naxes, double rescale,
    fitsfile *outfptr, int *status)
{
/*
    divide the integer pixel values in the input file by rescale,
    and write back out to the output file..
*/

	long ii, jj, nelem = 1, nx, ny;
	int *intarray, nullvalue;
	int anynul, tstatus, checknull = 1;
	
	nx = naxes[0];
	ny = 1;
	
	for (ii = 1; ii < naxis; ii++) {
	    ny = ny * naxes[ii];
	}

	intarray = calloc(nx, sizeof(int));
	if (!intarray) {
		*status = MEMORY_ALLOCATION;
		return(*status);
	}

	/* read the null value keyword (BLANK) if present */
	tstatus = 0;
	fits_read_key(infptr, TINT, "BLANK", &nullvalue, 0, &tstatus);
	if (tstatus) {
	   checknull = 0;
	}

	/* turn off any scaling of the integer pixel values */
	fits_set_bscale(infptr,  1.0, 0.0, status);
	fits_set_bscale(outfptr, 1.0, 0.0, status);

	for (ii = 0; ii < ny; ii++) {

	    fits_read_img_int(infptr, 1, nelem, nx,
	        0, intarray, &anynul, status);

	    if (checknull) {
	        for (jj = 0; jj < nx; jj++) {
	            if (intarray[jj] != nullvalue)
		        intarray[jj] = NINT( (intarray[jj] / rescale) );
		}
	    } else {
	        for (jj = 0; jj < nx; jj++)
		        intarray[jj] = NINT( (intarray[jj] / rescale) );
	    }

	    fits_write_img_int(outfptr, 1, nelem, nx, intarray, status);
	      
	    nelem += nx;
	}

	free(intarray);
	return(*status);
}
/* ========================================================================
 * Signal and error handler.
 */
void abort_fpack(int sig)
{
      if (tempfilename[0]) {
         remove(tempfilename);
      }
      
      exit(-1); 
}
