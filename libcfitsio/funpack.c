/* FUNPACK
 * R. Seaman, NOAO
 * uses fits_img_compress by W. Pence, HEASARC
 */

#include "fitsio.h"
#include "fpack.h"

int main (int argc, char *argv[])
{
	fpstate	fpvar;

	if (argc <= 1) { fu_usage (); fu_hint (); exit (-1); }

	fp_init (&fpvar);
	fu_get_param (argc, argv, &fpvar);

	if (fpvar.listonly) {
	    fp_list (argc, argv, fpvar);

	} else {
	    fp_preflight (argc, argv, FUNPACK, &fpvar);
	    fp_loop (argc, argv, FUNPACK, fpvar);
	}

	exit (0);
}

int fu_get_param (int argc, char *argv[], fpstate *fpptr)
{
	int	gottype=0, gottile=0, wholetile=0, iarg, len, ndim, ii;
	char	tmp[SZ_STR], tile[SZ_STR];

        if (fpptr->initialized != FP_INIT_MAGIC) {
            fp_msg ("Error: internal initialization error\n"); exit (-1);
        }

	tile[0] = (char) NULL;

        /* by default, .fz suffix characters to be deleted from compressed file */
	fpptr->delete_suffix = 1;

	/* flags must come first and be separately specified
	 */
	for (iarg = 1; iarg < argc; iarg++) {
	    if (argv[iarg][0] == '-' && strlen (argv[iarg]) == 2) {

		if (argv[iarg][1] == 'F') {
		    fpptr->clobber++;
                    fpptr->delete_suffix = 0;  /* no suffix in this case */

		} else if (argv[iarg][1] == 'D') {
		    fpptr->delete_input++;

		} else if (argv[iarg][1] == 'P') {
		    if (++iarg >= argc) {
			fu_usage (); fu_hint (); exit (-1);
		    } else
			strncpy (fpptr->prefix, argv[iarg], SZ_STR);

		} else if (argv[iarg][1] == 'S') {
		    fpptr->to_stdout++;

		} else if (argv[iarg][1] == 'L') {
		    fpptr->listonly++;

		} else if (argv[iarg][1] == 'C') {
		    fpptr->do_checksums = 0;

		} else if (argv[iarg][1] == 'H') {
		    fu_help (); exit (0);

		} else if (argv[iarg][1] == 'V') {
		    fp_version (); exit (0);

		} else if (argv[iarg][1] == 'Z') {
		    fpptr->do_gzip_file++;

		} else if (argv[iarg][1] == 'v') {
		    fpptr->verbose = 1;

		} else if (argv[iarg][1] == 'O') {
		    if (++iarg >= argc) {
			fu_usage (); fu_hint (); exit (-1);
		    } else
			strncpy (fpptr->outfile, argv[iarg], SZ_STR);

		} else {
		    fp_msg ("Error: unknown command line flag `");
		    fp_msg (argv[iarg]); fp_msg ("'\n");
		    fu_usage (); fu_hint (); exit (-1);
		}

	    } else
		break;
	}

	if (iarg >= argc) {
	    fp_msg ("Error: no FITS files to uncompress\n");
	    fu_usage (); exit (-1);
	} else
	    fpptr->firstfile = iarg;

	return(0);
}

int fu_usage (void)
{
	fp_msg ("usage: funpack [-F] [-D] [-Z] [-P <pre>] [-O <name>] [-S] [-L] [-C] [-H] [-V] <FITS>\n");
	return(0);
}

int fu_hint (void)
{
	fp_msg ("      `funpack -H' for help\n");
	return(0);
}

int fu_help (void)
{
fp_msg ("funpack, decompress fpacked files.  Version ");
fp_version ();
fu_usage ();
fp_msg ("\n");

fp_msg ("Flags must be separate and appear before filenames:\n");
fp_msg ("   -v          verbose mode; list each file as it is processed\n");
fp_msg ("   -F          overwrite input file by output file with same name\n");
fp_msg ("   -D          delete input file after writing output\n");
fp_msg ("   -P <pre>    prepend <pre> to create new output filenames\n");
fp_msg ("   -O <name>   specify full output file name\n");
fp_msg ("   -S          output uncompressed file to STDOUT\n");
fp_msg ("   -Z          recompress the output file with host GZIP program\n");
fp_msg ("   -L          list contents, files unchanged\n");

fp_msg ("   -C          don't update FITS checksum keywords\n");

fp_msg ("   -H          print this message\n");
fp_msg ("   -V          print version number\n");

fp_msg (" <FITS>        FITS files to unpack\n");
	return(0);
}
