#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <ctype.h>
#include <arpa/inet.h>

#include <unistd.h>

#include <jpeglib.h>
#include <fitsio.h>
#include "gphoto_readimage.h"

char dcraw_cmd[] = "dcraw";

#define err_printf IDLog,
struct dcraw_header {
	time_t time;
	float exposure;
	int width;
	int height;
	int cfa_type;
	float wbr;
	float wbg;
	float wbgp;
	float wbb;
};

enum {
	CFA_RGGB,
};

void *tstrealloc(void *ptr, size_t size) {
	fprintf(stderr, "Realloc: %lu\n", (unsigned long)size);
	return realloc(ptr, size);
}

static void skip_line( FILE *fp )
{
	int ch;

	while( (ch = fgetc( fp )) != '\n' )
		;
}

static void skip_white_space( FILE * fp )
{
	int ch;
	while( isspace( ch = fgetc( fp ) ) )
		;
	ungetc( ch, fp );

	if( ch == '#' ) {
		skip_line( fp );
		skip_white_space( fp );
	}
}

static unsigned int read_uint( FILE *fp )
{
	int i;
	char buf[80];
	int ch;

	skip_white_space( fp );

	/* Stop complaints about used-before-set on ch.
	 */
	ch = -1;

	for( i = 0; i < 80 - 1 && isdigit( ch = fgetc( fp ) ); i++ )
		buf[i] = ch;
	buf[i] = '\0';

	if( i == 0 ) {
		return( -1 );
	}

	ungetc( ch, fp );

	return( atoi( buf ) );
}

void addFITSKeywords(fitsfile *fptr)
{
  int status=0;

  /* TODO add other data later */
  fits_write_date(fptr, &status);
}

int read_ppm(FILE *handle, struct dcraw_header *header, char **memptr, size_t *memsize, int *n_axis, int *w, int *h, int *bitsperpixel)
{
	char prefix[] = {0, 0};
	int bpp, maxcolor, row, i;
	unsigned char *ppm = NULL;
	unsigned char *r_data = NULL, *g_data, *b_data;
	int width, height;
	int naxis = 2;

	prefix[0] = fgetc(handle);
	prefix[1] = fgetc(handle);
        if (prefix[0] != 'P' || (prefix[1] != '6' && prefix[1] != '5')) {
		fprintf(stderr, "read_ppm: got unexpected prefix %x %x\n", prefix[0], prefix[1]);
        return -1;
	}

    if (prefix[1] == '6')
		naxis = 3;

    *n_axis = naxis;

	width = read_uint(handle);
	height = read_uint(handle);
    if (width != header->width || height != header->height)
    {
		fprintf(stderr, "read_ppm: Expected (%d x %d) but image is actually (%d x %d)\n", header->width, header->height, width, height);
        return -1;
	}
    *w = width;
    *h = height;
	maxcolor = read_uint(handle);
	fgetc(handle);
    if (maxcolor > 65535)
    {
		fprintf(stderr, "read_ppm: 32bit PPM isn't supported\n");
        return -1;
    } else if (maxcolor > 255)
    {
		bpp = 2;
        *bitsperpixel = 16;
    } else
    {
		bpp = 1;
        *bitsperpixel = 8;
	}

    *memsize = width * height * bpp * (naxis == 2 ? 1 : 3);
    *memptr = realloc(*memptr, *memsize);
    ppm = malloc(width * bpp);
    if (naxis == 3)
    {
        r_data = (unsigned char *) *memptr;
        g_data = r_data + width * height * bpp;
        b_data = r_data + 2 * width * height * bpp;
	}

    for (row = 0; row < height; row++)
    {
		int len;
        len = fread(ppm, 1, width * bpp, handle);
        if (len != width * bpp)
        {
			fprintf(stderr, "read_ppm: aborted during PPM reading at row: %d, read %d bytes\n", row, len);
            free(ppm);
            return -1;
		}
        if (bpp == 2)
        {
			unsigned short *ppm16 = (unsigned short *)ppm;
            if (htons(0x55aa) != 0x55aa)
            {
				swab(ppm, ppm,  width * bpp);
			}
            if (naxis == 3)
            {
                for (i = 0; i < width; i++)
                {
                    *(unsigned short *)r_data++ = *ppm16++;
                    *(unsigned short *)g_data++ = *ppm16++;
                    *(unsigned short *)b_data++ = *ppm16++;
				}

            } else
            {
                memcpy(*memptr, ppm16, width*bpp);
                *memptr += width*bpp;
			}
			
        } else
        {
			unsigned char *ppm8 = ppm;
            if (naxis == 3)
            {
                for (i = 0; i < width; i++)
                {
                    *r_data++ = *ppm8++;
                    *g_data++ = *ppm8++;
                    *b_data++ = *ppm8++;
				}

            } else
            {
                memcpy(*memptr, ppm8, width*bpp);
                *memptr += width*bpp;
			}
		}
	}

    free(ppm);
	return 0;

}

int dcraw_parse_time(char *month, int day, int year, char *timestr)
{
	char mon_map[12][3] = {
		"Jan", "Feb", "Mar", "Apr", "May", "Jun",
		"Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
		};
	struct tm tm;
	int i;

	memset(&tm, 0, sizeof(struct tm));


	for (i = 0; i < 12; i++) {
		if(strncmp(month, mon_map[i], 3) == 0) {
			tm.tm_mon = i;
			break;
		}
	}
	tm.tm_year = year - 1900;
	tm.tm_mday = day;

	sscanf(timestr, "%d:%d:%d", &tm.tm_hour, &tm.tm_min, &tm.tm_sec);
	return mktime(&tm);
}

int dcraw_parse_header_info(const char *filename, struct dcraw_header *header)
{
	FILE *handle;
	char line[256], cfa[80];
	char *cmd, timestr[10], month[10], daystr[10];
	int day, year;
	float r, g, b, gp;

	memset(header, 0, sizeof(struct dcraw_header));
	asprintf(&cmd, "%s -i -v %s 2> /dev/null", dcraw_cmd, filename);
	handle = popen(cmd, "r");
	free(cmd);
	if (handle == NULL) {
		return 1;
	}

	while (fgets(line, sizeof(line), handle)) {
		if (sscanf(line, "Timestamp: %s %s %d %s %d", daystr, month, &day, timestr, &year) )
			header->time = dcraw_parse_time(month, day, year, timestr);
		else if (sscanf(line, "Shutter: 1/%f sec", &header->exposure) )
			header->exposure = 1.0 / header->exposure;
		else if (sscanf(line, "Shutter: %f sec", &header->exposure) )
			;
		else if (sscanf(line, "Output size: %d x %d", &header->width, &header->height) )
			;
		else if (sscanf(line, "Filter pattern: %s", cfa) ) {
			if(strncmp(cfa, "RGGBRGGBRGGBRGGB", sizeof(cfa)) == 0) {
				header->cfa_type = CFA_RGGB;
			}
		}
		else if (sscanf(line, "Camera multipliers: %f %f %f %f", &r, &g, &b, &gp)
			 && r > 0.0) {
			header->wbr = 1.0;
			header->wbg = g/r;
			header->wbgp = gp/r;
			header->wbb = b/r;
		}
	}

	pclose(handle);
	return 0;
}

int read_dcraw(const char *filename, char **memptr, size_t *memsize, int *n_axis, int *w, int *h, int *bitsperpixel)
{
	struct dcraw_header header;
	FILE *handle = NULL;
	char *cmd;

    if (dcraw_parse_header_info(filename, &header)  || ! header.width  || ! header.height)
	{
		fprintf(stderr, "read_file_from_dcraw: failed to parse header\n");
        return -1;
	}

	fprintf(stderr, "Reading exposure %d x %d\n", header.width, header.height);
	asprintf(&cmd, "%s -c -4 -D %s", dcraw_cmd, filename);
	handle = popen(cmd, "r");
	free(cmd);
    if (handle == NULL)
    {
		fprintf(stderr, "read_file_from_dcraw: failed to run dcraw\n");
        return -1;
	}

    int rc= read_ppm(handle, &header, memptr, memsize, n_axis, w, h, bitsperpixel);

	pclose(handle);

    return rc;
}

int read_jpeg(const char *filename, char **memptr, size_t *memsize, int *naxis, int *w, int *h )
{
	int row;
    unsigned char *r_data = NULL, *g_data, *b_data;

	/* these are standard libjpeg structures for reading(decompression) */
	struct jpeg_decompress_struct cinfo;
	struct jpeg_error_mgr jerr;
	/* libjpeg data structure for storing one row, that is, scanline of an image */
	JSAMPROW row_pointer[1] = {NULL};
	
	FILE *infile = fopen( filename, "rb" );
	int i = 0;
	
	if ( !infile )
	{
		fprintf(stderr, "Error opening jpeg file %s\n!", filename );
		return -1;
	}
	/* here we set up the standard libjpeg error handler */
	cinfo.err = jpeg_std_error( &jerr );
	/* setup decompression process and source, then read JPEG header */
	jpeg_create_decompress( &cinfo );
	/* this makes the library read from infile */
	jpeg_stdio_src( &cinfo, infile );
	/* reading the image header which contains image information */
	jpeg_read_header( &cinfo, TRUE );

    //*memsize = 2 * 2048;
    //*memptr = malloc(*memsize);
    /*fits_create_memfile(&fptr, memptr, memsize, 2880, &tstrealloc, &status);
	if (status) {
		fprintf(stderr, "Error: Failed to create memfile (memsize: %lu)\n", *(unsigned long *)memsize);
        fits_report_error(stderr, status);
		goto err_release;
    }*/
    /*if (cinfo.num_components == 3)
    {

		naxis = 3;
		naxes[2] = 3;
    }*/

    //naxes[0] = cinfo.image_width;
    //naxes[1] = cinfo.image_height;
    /*fits_create_img(fptr, BYTE_IMG, naxis, naxes, &status);
	if (status)
	{
		fprintf(stderr, "Error: Failed to create FITS image\n");
        fits_report_error(stderr, status);
		goto err_release;
    }*/

	/* Start decompression jpeg here */
	jpeg_start_decompress( &cinfo );

    *memsize = cinfo.output_width * cinfo.output_height * cinfo.num_components;
    *memptr = realloc(*memptr, *memsize);
    *naxis = cinfo.num_components;
    *w = cinfo.output_width;
    *h = cinfo.output_height;

	/* now actually read the jpeg into the raw buffer */
	row_pointer[0] = (unsigned char *)malloc( cinfo.output_width*cinfo.num_components );
    if (cinfo.num_components)
    {
        r_data = (unsigned char *) *memptr;
        g_data = r_data + cinfo.output_width * cinfo.output_height;
        b_data = r_data + 2 * cinfo.output_width * cinfo.output_height;
	}
	/* read one scan line at a time */
	for (row = 0; row < cinfo.image_height; row++)
	{
		unsigned char *ppm8 = row_pointer[0];
		jpeg_read_scanlines( &cinfo, row_pointer, 1 );

        if (cinfo.num_components == 3)
        {
            for (i = 0; i < cinfo.output_width; i++)
            {
                //r_data[i] = *ppm8++;
                //g_data[i] = *ppm8++;
                //b_data[i] = *ppm8++;
                *r_data++ = *ppm8++;
                *g_data++ = *ppm8++;
                *b_data++ = *ppm8++;
			}
        }
        else
        {
            memcpy(*memptr, ppm8, cinfo.output_width);
            *memptr += cinfo.output_width;
		}
	}

    // naxis =3
    //fits_write_img(fptr, TBYTE, 1                                               + row * cinfo.output_width, cinfo.output_width, r_data, &status);
    //fits_write_img(fptr, TBYTE, 1 + cinfo.output_width * cinfo.image_height     + row * cinfo.output_width, cinfo.output_width, g_data, &status);
    //fits_write_img(fptr, TBYTE, 1 + cinfo.output_width * cinfo.image_height * 2 + row * cinfo.output_width, cinfo.output_width, b_data, &status);

    //naxis = 2
    //fits_write_img(fptr, TBYTE, 1 + row * cinfo.output_width, cinfo.output_width, ppm8, &status);

	/* wrap up decompression, destroy objects, free pointers and close open files */
	jpeg_finish_decompress( &cinfo );
	jpeg_destroy_decompress( &cinfo );

	if (row_pointer[0] )
		free( row_pointer[0] );
    //if(r_data)
        //free(r_data);
	if(infile)
		fclose( infile );
    //fits_close_file(fptr, &status);            /* close the file */
	return 0;
/*err_release:
	if (row_pointer[0] )
		free( row_pointer[0] );
	if(r_data)
		free(r_data);
	if(infile)
		fclose( infile );
    fits_close_file(fptr, &status);            */

    //return 1;
}

