/* used by FPACK and FUNPACK
 * R. Seaman, NOAO
 */

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>

#define	FPACK_VERSION	"1.1.0 (August 2008)"
#define	FP_INIT_MAGIC	42
#define	FPACK		0
#define	FUNPACK		1

#define	DEF_QLEVEL	16.
#define	DEF_HCOMP_SCALE	 0.
#define	DEF_HCOMP_SMOOTH 0
#define	DEF_RESCALE_NOISE 0

#define	SZ_STR		513
#define	SZ_CARD		81

char tempfilename[SZ_STR];

typedef struct
{
	int	comptype;
	float	quantize_level;
	float	scale;
	float	rescale_noise;
	int	smooth;
	long	ntile[MAX_COMPRESS_DIM];

	int	to_stdout;
	int	listonly;
	int	clobber;
	int	delete_input;
	int	do_not_prompt;
	int	do_checksums;
	int	do_gzip_file;
	int	test_all;
	int	verbose;

	char	prefix[SZ_STR];
	int	delete_suffix;
	char	outfile[SZ_STR];
	int	firstfile;

	int	initialized;
	int	preflight_checked;
} fpstate;

typedef struct
{
	int	n_nulls;
	double	minval;
	double 	maxval;
	double	mean;
	double	sigma;
	double	noise1;
	double	noise3;
} imgstats;

int fp_get_param (int argc, char *argv[], fpstate *fpptr);
void abort_fpack(int sig);
int fp_usage (void);
int fp_help (void);
int fp_hint (void); 
int fp_init (fpstate *fpptr);
int fp_list (int argc, char *argv[], fpstate fpvar);
int fp_info (char *infits);
int fp_info_hdu (fitsfile *infptr);
int fp_preflight (int argc, char *argv[], int unpack, fpstate *fpptr);
int fp_loop (int argc, char *argv[], int unpack, fpstate fpvar);
int fp_pack (char *infits, char *outfits, fpstate fpvar, int *islossless);
int fp_unpack (char *infits, char *outfits, fpstate fpvar);
int fp_test (char *infits, char *outfits, char *outfits2, fpstate fpvar);
int fp_pack_hdu (fitsfile *infptr, fitsfile *outfptr, fpstate fpvar, 
    int *islossless, int *status);
int fp_unpack_hdu (fitsfile *infptr, fitsfile *outfptr, int *status);
int fits_read_image_speed (fitsfile *infptr, float *whole_elapse, 
    float *whole_cpu, float *row_elapse, float *row_cpu, int *status);
int fp_test_hdu (fitsfile *infptr, fitsfile *outfptr, fitsfile *outfptr2, 
	fpstate fpvar, int *status);
int marktime(int *status);
int gettime(float *elapse, float *elapscpu, int *status);
int fits_read_image_speed (fitsfile *infptr, float *whole_elapse, 
    float *whole_cpu, float *row_elapse, float *row_cpu, int *status);

int fp_i2stat(fitsfile *infptr, int naxis, long *naxes, int *status);
int fp_i4stat(fitsfile *infptr, int naxis, long *naxes, int *status);
int fp_r4stat(fitsfile *infptr, int naxis, long *naxes, int *status);
int fp_i2rescale(fitsfile *infptr, int naxis, long *naxes, double rescale,
    fitsfile *outfptr, int *status);
int fp_i4rescale(fitsfile *infptr, int naxis, long *naxes, double rescale,
    fitsfile *outfptr, int *status);
    
int fp_msg (char *msg);
int fp_version (void);
int fp_noop (void);

int fu_get_param (int argc, char *argv[], fpstate *fpptr);
int fu_usage (void);
int fu_hint (void);
int fu_help (void);
