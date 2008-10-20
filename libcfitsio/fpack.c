/* FPACK
 * R. Seaman, NOAO, with a few enhancements by W. Pence, HEASARC
 *
 * Calls fits_img_compress in the CFITSIO library by W. Pence, HEASARC
 */

#include <ctype.h>
#include <signal.h>
#include "fitsio.h"
#include "fpack.h"
/* ================================================================== */
int main(int argc, char *argv[])
{
	fpstate	fpvar;

	if (argc <= 1) { fp_usage (); fp_hint (); exit (-1); }

	fp_init (&fpvar);
	fp_get_param (argc, argv, &fpvar);

	if (fpvar.listonly) {
	    fp_list (argc, argv, fpvar);

	} else {
	    fp_preflight (argc, argv, FPACK, &fpvar);
	    fp_loop (argc, argv, FPACK, fpvar);
	}

	exit (0);
}
/* ================================================================== */
int fp_get_param (int argc, char *argv[], fpstate *fpptr)
{
	int	gottype=0, gottile=0, wholetile=0, iarg, len, ndim, ii;
	char	tmp[SZ_STR], tile[SZ_STR];

        if (fpptr->initialized != FP_INIT_MAGIC) {
            fp_msg ("Error: internal initialization error\n"); exit (-1);
        }

	tile[0] = (char) NULL;

	/* flags must come first and be separately specified
	 */
	for (iarg = 1; iarg < argc; iarg++) {
	    if (argv[iarg][0] == '-' && strlen (argv[iarg]) == 2) {

		/* Rice is the default, so -r is superfluous 
		 */
		if (       argv[iarg][1] == 'r') {
		    fpptr->comptype = RICE_1;
		    if (gottype) {
			fp_msg ("Error: multiple compression flags\n");
			fp_usage (); exit (-1);
		    } else
			gottype++;

		} else if (argv[iarg][1] == 'p') {
		    fpptr->comptype = PLIO_1;
		    if (gottype) {
			fp_msg ("Error: multiple compression flags\n");
			fp_usage (); exit (-1);
		    } else
			gottype++;

		} else if (argv[iarg][1] == 'g') {
		    fpptr->comptype = GZIP_1;
		    if (gottype) {
			fp_msg ("Error: multiple compression flags\n");
			fp_usage (); exit (-1);
		    } else
			gottype++;

		} else if (argv[iarg][1] == 'h') {
		    fpptr->comptype = HCOMPRESS_1;
		    if (gottype) {
			fp_msg ("Error: multiple compression flags\n");
			fp_usage (); exit (-1);
		    } else
			gottype++;

		} else if (argv[iarg][1] == 'd') {
		    fpptr->comptype = NOCOMPRESS;
		    if (gottype) {
			fp_msg ("Error: multiple compression flags\n");
			fp_usage (); exit (-1);
		    } else
			gottype++;

		} else if (argv[iarg][1] == 'q') {
		    if (++iarg >= argc) {
			fp_usage (); exit (-1);
		    } else {
			fpptr->quantize_level = (float) atof (argv[iarg]);
		    }
		} else if (argv[iarg][1] == 'n') {
		    if (++iarg >= argc) {
			fp_usage (); exit (-1);
		    } else {
			fpptr->rescale_noise = (float) atof (argv[iarg]);
		    }
		} else if (argv[iarg][1] == 's') {
		    if (++iarg >= argc) {
			fp_usage (); exit (-1);
		    } else {
			fpptr->scale = (float) atof (argv[iarg]);
		    }
		} else if (argv[iarg][1] == 't') {
		    if (gottile) {
			fp_msg ("Error: multiple tile specifications\n");
			fp_usage (); exit (-1);
		    } else
			gottile++;

		    if (++iarg >= argc) {
			fp_usage (); exit (-1);
		    } else
			strncpy (tile, argv[iarg], SZ_STR); /* checked below */

		} else if (argv[iarg][1] == 'v') {
		    fpptr->verbose = 1;

		} else if (argv[iarg][1] == 'w') {
		    wholetile++;
		    if (gottile) {
			fp_msg ("Error: multiple tile specifications\n");
			fp_usage (); exit (-1);
		    } else
			gottile++;

		} else if (argv[iarg][1] == 'F') {
		    fpptr->clobber++;       /* overwrite existing file */

		} else if (argv[iarg][1] == 'D') {
		    fpptr->delete_input++;

		} else if (argv[iarg][1] == 'Y') {
		    fpptr->do_not_prompt++;

		} else if (argv[iarg][1] == 'S') {
		    fpptr->to_stdout++;

		} else if (argv[iarg][1] == 'L') {
		    fpptr->listonly++;

		} else if (argv[iarg][1] == 'C') {
		    fpptr->do_checksums = 0;

		} else if (argv[iarg][1] == 'T') {
		    fpptr->test_all = 1;

		} else if (argv[iarg][1] == 'R') {
		    if (++iarg >= argc) {
			fp_usage (); fp_hint (); exit (-1);
		    } else
			strncpy (fpptr->outfile, argv[iarg], SZ_STR);

		} else if (argv[iarg][1] == 'H') {
		    fp_help (); exit (0);

		} else if (argv[iarg][1] == 'V') {
		    fp_version (); exit (0);

		} else {
		    fp_msg ("Error: unknown command line flag `");
		    fp_msg (argv[iarg]); fp_msg ("'\n");
		    fp_usage (); fp_hint (); exit (-1);
		}

	    } else
		break;
	}

	if (fpptr->scale != 0. && 
	         fpptr->comptype != HCOMPRESS_1 && fpptr->test_all != 1) {

	    fp_msg ("Error: `-s' requires `-h or -T'\n"); exit (-1);
	}


	if (wholetile) {
	    for (ndim=0; ndim < MAX_COMPRESS_DIM; ndim++)
		fpptr->ntile[ndim] = (long) 0;

	} else if (gottile) {
	    len = strlen (tile);
	    for (ii=0, ndim=0; ii < len; ) {
		if (! (isdigit (tile[ii]) || tile[ii] == ',')) {
		    fp_msg ("Error: `-t' requires comma separated tile dims, ");
		    fp_msg ("e.g., `-t 100,100'\n"); exit (-1);
		}

		if (tile[ii] == ',') { ii++; continue; }

		fpptr->ntile[ndim] = atol (&tile[ii]);
		for ( ; isdigit(tile[ii]); ii++);

		if (++ndim > MAX_COMPRESS_DIM) {
		    fp_msg ("Error: too many dimensions for `-t', max=");
		    sprintf (tmp, "%d\n", MAX_COMPRESS_DIM); fp_msg (tmp);
		    exit (-1);
		}
	    }
	}

	if (iarg >= argc) {
	    fp_msg ("Error: no FITS files to compress\n");
	    fp_usage (); exit (-1);
	} else
	    fpptr->firstfile = iarg;

	return(0);
}

/* ================================================================== */
int fp_usage (void)
{
fp_msg ("usage: fpack ");
fp_msg (
"[-r|-h|-g|-p] [-w|-t <axes>] [-q <level>] [-s <scale>] [-n <noise>] -v <FITS>\n");
fp_msg ("more:   [-T] [-F] [-D] [-Y] [-S] [-L] [-C] [-H] [-V]\n");
return(0);
}

/* ================================================================== */
int fp_hint (void) 
{ fp_msg ("      `fpack -H' for help\n"); 
return(0);
}

/* ================================================================== */
int fp_help (void)
{
fp_msg ("fpack, a FITS tile-compression engine.  Version ");
fp_version ();
fp_usage ();
fp_msg ("\n");

fp_msg ("Flags must be separate and appear before filenames:\n");
fp_msg ("   -r          Rice compression [default], or\n");
fp_msg ("   -h          Hcompress compression, or\n");
fp_msg ("   -g          GZIP (per-tile) compression, or\n");
fp_msg ("   -p          PLIO compression (only for positive 8 or 16-bit integer images)\n");
fp_msg ("   -d          tile the image without compression (debugging mode)\n");

fp_msg ("   -w          compress the whole image,as a single large tile\n");
fp_msg ("   -t <axes>   comma separated list of tile dimensions [default=row by row]\n");
fp_msg ("   -q <level>  quantization level for floating point images [default=16]\n");
fp_msg ("               (+values relative to RMS noise; -value is absolute)\n");

fp_msg ("   -s <scale>  scale factor for lossy Hcompress [default = 0 = lossless]\n");
fp_msg ("               (+values relative to RMS noise; -value is absolute)\n");
fp_msg ("   -n <noise>  rescale scaled-integer images to reduce noise\n");

fp_msg ("   -v          verbose mode; list each file as it is processed\n");
fp_msg ("   -T          print test comparison report of compression algorithms\n");
fp_msg ("   -R <file>   write test report results to text file\n");

fp_msg ("\nkeywords shared with funpack:\n");

fp_msg ("   -F          overwrite input file by output file with same name\n");
fp_msg ("   -D          delete input file after writing output\n");
fp_msg ("   -Y          suppress prompts to confirm -F or -D options\n");

fp_msg ("   -S          output compressed FITS files to STDOUT\n");
fp_msg ("   -L          list contents, files unchanged\n");

fp_msg ("   -C          don't update FITS checksum keywords\n");

fp_msg ("   -H          print this message\n");
fp_msg ("   -V          print version number\n");

fp_msg (" <FITS>        FITS files to pack\n");
return(0);
}
