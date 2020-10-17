/*
 *   libDSPAU - a digital signal processing library for astronoms usage
 *   Copyright (C) 2017  Ilia Platone <info@iliaplatone.com>
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "dsp.h"
#include <fitsio.h>
#include <time.h>
#include <locale.h>
//#include <tiffio.h>
#include <jpeglib.h>

dsp_stream_p dsp_file_read_fits(char *filename, int stretch)
{
    fitsfile *fptr = (fitsfile*)malloc(sizeof(fitsfile));
    memset(fptr, 0, sizeof(fitsfile));
    int bpp = 16;
    int status = 0;
    char key[150];
    char value[150];
    char comment[150];
    char error_status[64];

    fits_open_file(&fptr, filename, READONLY_FILE, &status);

    if (status)
    {
        goto fail;
    }

    sprintf(key, "%s", "BITPIX");
    ffgkey(fptr, key, value, comment, &status);

    if (status)
    {
        goto fail;
    }

    bpp = atoi(value);
    sprintf(key, "%s", "NAXIS");
    ffgkey(fptr, key, value, comment, &status);

    if (status)
    {
        goto fail;
    }

    dsp_stream_p picture = dsp_stream_new();
    int dims = atoi(value);
    int dim;
    for(dim = 1; dim <= dims; dim++) {
        status = 0;
        sprintf(key, "%s%d", "NAXIS", dim);
        ffgkey(fptr, key, value, comment, &status);

        if (status)
        {
            goto fail_stream;
        }

        dsp_stream_add_dim(picture, atoi(value));

        sprintf(key, "%s%d", "PIXSIZE", dim);
        ffgkey(fptr, key, value, comment, &status);

        picture->pixel_sizes[dim - 1] = 0.000001;

        if (!status)
        {
            picture->pixel_sizes[dim - 1] = atof(value)*0.000001;
        }

    }

    dsp_stream_alloc_buffer(picture, picture->len);

    ffgkey(fptr, "XBAYROFF", value, comment, &status);

    if (!status)
    {
        picture->red = atoi(value);
    }

    ffgkey(fptr, "YBAYROFF", value, comment, &status);

    if (!status)
    {
        picture->red |= atoi(value)<<1;
    }

    ffgkey(fptr, "APTDIA", value, comment, &status);

    picture->diameter = 1.0;

    if (!status)
    {
        picture->diameter = atof(value)*0.001;
    }

    ffgkey(fptr, "FOCALLEN", value, comment, &status);

    picture->focal_ratio = 1.0;

    if (!status)
    {
        picture->focal_ratio = atof(value)*0.001/picture->diameter;
    }
    status = 0;
    long nelements = (long)picture->len;
    void *array = malloc(bpp * nelements / 8);
    int anynul = 0;
    dsp_t max = (dsp_t)((unsigned long)(1<<(8*sizeof(dsp_t)))/2-1);
    switch(bpp) {
    case 8:
        fits_read_img(fptr, TBYTE, 1, nelements, NULL, array, &anynul, &status);
        dsp_buffer_copy(((unsigned char*)(array)), picture->buf, nelements);
        break;
    case 16:
        fits_read_img(fptr, TUSHORT, 1, nelements, NULL, array, &anynul, &status);
        if(bpp > 8*sizeof(dsp_t))
            dsp_buffer_stretch(((unsigned short*)(array)), nelements, 0, max);
        dsp_buffer_copy(((unsigned short*)(array)), picture->buf, nelements);
        break;
    case 32:
        fits_read_img(fptr, TULONG, 1, nelements, NULL, array, &anynul, &status);
        if(bpp > 8*sizeof(dsp_t))
            dsp_buffer_stretch(((unsigned int*)(array)), nelements, 0, max);
        dsp_buffer_copy(((unsigned int*)(array)), picture->buf, nelements);
        break;
    case 64:
        fits_read_img(fptr, TLONGLONG, 1, nelements, NULL, array, &anynul, &status);
        if(bpp > 8*sizeof(dsp_t))
            dsp_buffer_stretch(((long*)(array)), nelements, 0, max);
        dsp_buffer_copy(((long*)(array)), picture->buf, nelements);
        break;
    case -32:
        fits_read_img(fptr, TFLOAT, 1, nelements, NULL, array, &anynul, &status);
        if(bpp > 8*sizeof(dsp_t))
            dsp_buffer_stretch(((float*)(array)), nelements, 0, max);
        dsp_buffer_copy(((float*)(array)), picture->buf, nelements);
        break;
    case -64:
        fits_read_img(fptr, TDOUBLE, 1, nelements, NULL, array, &anynul, &status);
        if(bpp > 8*sizeof(dsp_t))
            dsp_buffer_stretch(((double*)(array)), nelements, 0, max);
        dsp_buffer_copy(((double*)(array)), picture->buf, nelements);
        break;
    }
    if(status||anynul)
        goto fail_stream;
    fits_close_file(fptr, &status);
    if(status)
        goto fail_stream;
    if(stretch)
        dsp_buffer_stretch(picture->buf, picture->len, 0, max);
    free(array);
    return picture;
fail_stream:
    dsp_stream_free_buffer(picture);
    dsp_stream_free(picture);
fail:
    if(status) {
        fits_report_error(stderr, status); /* print out any error messages */
        fits_get_errstatus(status, error_status);
        fprintf(stderr, "FITS Error: %s\n", error_status);
    }
    free(fptr);
    return NULL;
}

void dsp_file_write_fits(char *filename, int bpp, dsp_stream_p picture)
{
    dsp_stream_p tmp = dsp_stream_copy(picture);
    int img_type  = USHORT_IMG;
    int byte_type = TUSHORT;
    char bit_depth[64] = "16 bits per sample";
    void* buf = malloc(tmp->len * abs(bpp) / 8 + 512);
    fitsfile *fptr = NULL;
    int status    = 0;
    int naxis    = tmp->dims;
    long *naxes = (long*)malloc(sizeof(long) * tmp->dims);
    long nelements = tmp->len;
    char error_status[64];
    unsigned int i;
    dsp_t max = (1<<abs(bpp))/2-1;
    dsp_buffer_stretch(tmp->buf, tmp->len, 0, max);
    for (i = 0;  i < tmp->dims; i++)
        naxes[i] = tmp->sizes[i];
    switch (bpp)
    {
        case 8:
            byte_type = TBYTE;
            img_type  = BYTE_IMG;
            dsp_buffer_copy(tmp->buf, ((unsigned char*)buf), tmp->len);
            strcpy(bit_depth, "8 bits per sample");
            break;

        case 16:
            byte_type = TUSHORT;
            img_type  = USHORT_IMG;
            dsp_buffer_copy(tmp->buf, ((unsigned short*)buf), tmp->len);
            strcpy(bit_depth, "16 bits per sample");
            break;

        case 32:
            byte_type = TULONG;
            img_type  = ULONG_IMG;
            dsp_buffer_copy(tmp->buf, ((unsigned int*)buf), tmp->len);
            strcpy(bit_depth, "32 bits per sample");
            break;

        case 64:
            byte_type = TLONGLONG;
            img_type  = LONGLONG_IMG;
            dsp_buffer_copy(tmp->buf, ((unsigned long*)buf), tmp->len);
            strcpy(bit_depth, "64 bits double per sample");
            break;

        case -32:
            byte_type = TFLOAT;
            img_type  = FLOAT_IMG;
            dsp_buffer_copy(tmp->buf, ((float*)buf), tmp->len);
            strcpy(bit_depth, "32 bits double per sample");
            break;

        case -64:
            byte_type = TDOUBLE;
            img_type  = DOUBLE_IMG;
            dsp_buffer_copy(tmp->buf, ((double*)buf), tmp->len);
            strcpy(bit_depth, "64 bits double per sample");
            break;

        default:
            fprintf(stderr, "Unsupported bits per sample value %d", bpp);
            goto fail;
    }
    int memsize = 5760;
    void *memfile = malloc(memsize);
    fits_create_memfile(&fptr, &memfile, &memsize, 2880, realloc, &status);

    if (status)
    {
        goto fail_fptr;
    }

    fits_create_img(fptr, img_type, naxis, naxes, &status);

    if (status)
    {
        goto fail_fptr;
    }

    fits_write_img(fptr, byte_type, 1, nelements, buf, &status);
fail_fptr:
    fits_close_file(fptr, &status);
    if(status) {
        fits_report_error(stderr, status); /* print out any error messages */
        fits_get_errstatus(status, error_status);
        fprintf(stderr, "FITS Error: %s\n", error_status);
    } else {
        FILE *f = fopen(filename, "wb");
        if(f) {
            fwrite(memfile, 1, 2880 + tmp->len * abs(bpp) / 8, f);
            fclose(f);
        }
    }
    free(memfile);
fail:
    dsp_stream_free_buffer(tmp);
    dsp_stream_free(tmp);
    free(naxes);
    free (buf);
}

dsp_t* dsp_file_bayer_2_gray(dsp_t *src, long int WIDTH, long int HEIGHT)
{
    long int i;
    dsp_t *rawpt, *scanpt;
    long int size;

    dsp_t * dst = (dsp_t*)malloc(sizeof(dsp_t)*WIDTH*HEIGHT);
    rawpt  = src;
    scanpt = dst;
    size   = WIDTH * HEIGHT;
    dsp_t val = 0;
    for (i = 0; i < size; i++)
    {
        if ((i / WIDTH) % 2 == 0)
        {
            if ((i % 2) == 0)
            {
                /* B */
                if ((i > WIDTH) && ((i % WIDTH) > 0))
                {
                    val =
                        (*(rawpt - WIDTH - 1) + *(rawpt - WIDTH + 1) + *(rawpt + WIDTH - 1) + *(rawpt + WIDTH + 1)) /
                        4;                                                                               /* R */
                    val += (*(rawpt - 1) + *(rawpt + 1) + *(rawpt + WIDTH) + *(rawpt - WIDTH)) / 4; /* G */
                    val += *rawpt;                                                                  /* B */
                }
                else
                {
                    val = *(rawpt + WIDTH + 1);                  /* R */
                    val += (*(rawpt + 1) + *(rawpt + WIDTH)) / 2; /* G */
                    val += *rawpt;                                /* B */
                }
            }
            else
            {
                /* (B)G */
                if ((i > WIDTH) && ((i % WIDTH) < (WIDTH - 1)))
                {
                    val = (*(rawpt + WIDTH) + *(rawpt - WIDTH)) / 2; /* R */
                    val += *rawpt;                                    /* G */
                    val += (*(rawpt - 1) + *(rawpt + 1)) / 2;         /* B */
                }
                else
                {
                    val = *(rawpt + WIDTH); /* R */
                    val += *rawpt;           /* G */
                    val += *(rawpt - 1);     /* B */
                }
            }
        }
        else
        {
            if ((i % 2) == 0)
            {
                /* G(R) */
                if ((i < (WIDTH * (HEIGHT - 1))) && ((i % WIDTH) > 0))
                {
                    val = (*(rawpt - 1) + *(rawpt + 1)) / 2;         /* R */
                    val += *rawpt;                                    /* G */
                    val += (*(rawpt + WIDTH) + *(rawpt - WIDTH)) / 2; /* B */
                }
                else
                {
                    /* bottom line or left column */
                    val = *(rawpt + 1);     /* R */
                    val += *rawpt;           /* G */
                    val += *(rawpt - WIDTH); /* B */
                }
            }
            else
            {
                /* R */
                if (i < (WIDTH * (HEIGHT - 1)) && ((i % WIDTH) < (WIDTH - 1)))
                {
                    val = *rawpt;                                                                  /* R */
                    val += (*(rawpt - 1) + *(rawpt + 1) + *(rawpt - WIDTH) + *(rawpt + WIDTH)) / 4; /* G */
                    val +=
                        (*(rawpt - WIDTH - 1) + *(rawpt - WIDTH + 1) + *(rawpt + WIDTH - 1) + *(rawpt + WIDTH + 1)) /
                        4; /* B */
                }
                else
                {
                    /* bottom line or right column */
                    val = *rawpt;                                /* R */
                    val += (*(rawpt - 1) + *(rawpt - WIDTH)) / 2; /* G */
                    val += *(rawpt - WIDTH - 1);                  /* B */
                }
            }
        }
        *scanpt++ = val;
        rawpt++;
    }
    return dst;
}


dsp_t* dsp_file_bayer_2_composite(dsp_t *src, int r, long int WIDTH, long int HEIGHT)
{
    long int i;
    dsp_t *rawpt, *red, *green, *blue;
    long int size;

    dsp_t *dst = (dsp_t *)malloc(sizeof(dsp_t)*WIDTH*HEIGHT*3);
    rawpt  = src;
    green = &dst[0];
    red = &dst[WIDTH*HEIGHT];
    blue = &dst[WIDTH*HEIGHT*2];
    size   = WIDTH * HEIGHT;
    for (i = 0; i < size; i++)
    {
        if ((i / WIDTH) % 2 == (r&1))
        {
            if ((i % 2) == ((r&2)>>1))
            {
                /* B */
                if ((i > WIDTH) && ((i % WIDTH) > 0))
                {
                    *green++ =
                        (*(rawpt - WIDTH - 1) + *(rawpt - WIDTH + 1) + *(rawpt + WIDTH - 1) + *(rawpt + WIDTH + 1)) /
                        4;                                                                               /* R */
                    *red++ = (*(rawpt - 1) + *(rawpt + 1) + *(rawpt + WIDTH) + *(rawpt - WIDTH)) / 4; /* G */
                    *blue++ = *rawpt;                                                                  /* B */
                }
                else
                {
                    *green++ = *(rawpt + WIDTH + 1);                  /* R */
                    *red++ = (*(rawpt + 1) + *(rawpt + WIDTH)) / 2; /* G */
                    *blue++ = *rawpt;                                /* B */
                }
            }
            else
            {
                /* (B)G */
                if ((i > WIDTH) && ((i % WIDTH) < (WIDTH - 1)))
                {
                    *green++ = (*(rawpt + WIDTH) + *(rawpt - WIDTH)) / 2; /* R */
                    *red++ = *rawpt;                                    /* G */
                    *blue++ = (*(rawpt - 1) + *(rawpt + 1)) / 2;         /* B */
                }
                else
                {
                    *green++ = *(rawpt + WIDTH); /* R */
                    *red++ = *rawpt;           /* G */
                    *blue++ = *(rawpt - 1);     /* B */
                }
            }
        }
        else
        {
            if ((i % 2) == ((r&2)>>1))
            {
                /* G(R) */
                if ((i < (WIDTH * (HEIGHT - 1))) && ((i % WIDTH) > 0))
                {
                    *green++ = (*(rawpt - 1) + *(rawpt + 1)) / 2;         /* R */
                    *red++ = *rawpt;                                    /* G */
                    *blue++ = (*(rawpt + WIDTH) + *(rawpt - WIDTH)) / 2; /* B */
                }
                else
                {
                    /* bottom line or left column */
                    *green++ = *(rawpt + 1);     /* R */
                    *red++ = *rawpt;           /* G */
                    *blue++ = *(rawpt - WIDTH); /* B */
                }
            }
            else
            {
                /* R */
                if (i < (WIDTH * (HEIGHT - 1)) && ((i % WIDTH) < (WIDTH - 1)))
                {
                    *green++ = *rawpt;                                                                  /* R */
                    *red++ = (*(rawpt - 1) + *(rawpt + 1) + *(rawpt - WIDTH) + *(rawpt + WIDTH)) / 4; /* G */
                    *blue++ =
                        (*(rawpt - WIDTH - 1) + *(rawpt - WIDTH + 1) + *(rawpt + WIDTH - 1) + *(rawpt + WIDTH + 1)) /
                        4; /* B */
                }
                else
                {
                    /* bottom line or right column */
                    *green++ = *rawpt;                                /* R */
                    *red++ = (*(rawpt - 1) + *(rawpt - WIDTH)) / 2; /* G */
                    *blue++ = *(rawpt - WIDTH - 1);                  /* B */
                }
            }
        }
        rawpt++;
    }
    return dst;
}


dsp_t* dsp_file_bayer_2_rgb(dsp_t *src, int red, long int WIDTH, long int HEIGHT)
{
    long int i;
    dsp_t *rawpt, *scanpt;
    long int size;

    dsp_t * dst = (dsp_t*)malloc(sizeof(dsp_t)*WIDTH*HEIGHT*3);
    rawpt  = src;
    scanpt = dst;
    size   = WIDTH * HEIGHT;
    *scanpt++;
    for (i = 0; i < size; i++)
    {
        if ((i / WIDTH) % 2 == (red&1))
        {
            if ((i % 2) == ((red&2)>>1))
            {
                /* B */
                if ((i > WIDTH) && ((i % WIDTH) > 0))
                {
                    *scanpt++ =
                        (*(rawpt - WIDTH - 1) + *(rawpt - WIDTH + 1) + *(rawpt + WIDTH - 1) + *(rawpt + WIDTH + 1)) /
                        4;                                                                               /* R */
                    *scanpt++ = (*(rawpt - 1) + *(rawpt + 1) + *(rawpt + WIDTH) + *(rawpt - WIDTH)) / 4; /* G */
                    *scanpt++ = *rawpt;                                                                  /* B */
                }
                else
                {
                    *scanpt++ = *(rawpt + WIDTH + 1);                  /* R */
                    *scanpt++ = (*(rawpt + 1) + *(rawpt + WIDTH)) / 2; /* G */
                    *scanpt++ = *rawpt;                                /* B */
                }
            }
            else
            {
                /* (B)G */
                if ((i > WIDTH) && ((i % WIDTH) < (WIDTH - 1)))
                {
                    *scanpt++ = (*(rawpt + WIDTH) + *(rawpt - WIDTH)) / 2; /* R */
                    *scanpt++ = *rawpt;                                    /* G */
                    *scanpt++ = (*(rawpt - 1) + *(rawpt + 1)) / 2;         /* B */
                }
                else
                {
                    *scanpt++ = *(rawpt + WIDTH); /* R */
                    *scanpt++ = *rawpt;           /* G */
                    *scanpt++ = *(rawpt - 1);     /* B */
                }
            }
        }
        else
        {
            if ((i % 2) == ((red&2)>>1))
            {
                /* G(R) */
                if ((i < (WIDTH * (HEIGHT - 1))) && ((i % WIDTH) > 0))
                {
                    *scanpt++ = (*(rawpt - 1) + *(rawpt + 1)) / 2;         /* R */
                    *scanpt++ = *rawpt;                                    /* G */
                    *scanpt++ = (*(rawpt + WIDTH) + *(rawpt - WIDTH)) / 2; /* B */
                }
                else
                {
                    /* bottom line or left column */
                    *scanpt++ = *(rawpt + 1);     /* R */
                    *scanpt++ = *rawpt;           /* G */
                    *scanpt++ = *(rawpt - WIDTH); /* B */
                }
            }
            else
            {
                /* R */
                if (i < (WIDTH * (HEIGHT - 1)) && ((i % WIDTH) < (WIDTH - 1)))
                {
                    *scanpt++ = *rawpt;                                                                  /* R */
                    *scanpt++ = (*(rawpt - 1) + *(rawpt + 1) + *(rawpt - WIDTH) + *(rawpt + WIDTH)) / 4; /* G */
                    *scanpt++ =
                        (*(rawpt - WIDTH - 1) + *(rawpt - WIDTH + 1) + *(rawpt + WIDTH - 1) + *(rawpt + WIDTH + 1)) /
                        4; /* B */
                }
                else
                {
                    /* bottom line or right column */
                    *scanpt++ = *rawpt;                                /* R */
                    *scanpt++ = (*(rawpt - 1) + *(rawpt - WIDTH)) / 2; /* G */
                    *scanpt++ = *(rawpt - WIDTH - 1);                  /* B */
                }
            }
        }
        rawpt++;
    }
    return dst;
}

void dsp_file_write_jpeg(char *filename, int quality, dsp_stream_p stream)
{
    int width = stream->sizes[0];
    int height = stream->sizes[1];

    void *buf = malloc(stream->len*(stream->red>=0?3:1));
    unsigned char *image = (unsigned char *)buf;
    dsp_t* data = stream->buf;

    if(stream->red>=0)
        data = dsp_file_bayer_2_rgb(stream->buf, stream->red, width, height);
    dsp_buffer_copy(data, image, stream->len*(stream->red>=0?3:1));

    struct jpeg_compress_struct cinfo;
    struct jpeg_error_mgr jerr;
    FILE * outfile;
    int row_stride;
    cinfo.err = jpeg_std_error(&jerr);
    jpeg_create_compress(&cinfo);
    if ((outfile = fopen(filename, "wb")) == NULL) {
        fprintf(stderr, "can't open %s\n", filename);
        return;
    }
    jpeg_stdio_dest(&cinfo, outfile);
    cinfo.image_width = width;
    cinfo.image_height = height;
    cinfo.input_components = (stream->red>=0?3:1);
    cinfo.in_color_space = (stream->red>=0?JCS_RGB:JCS_GRAYSCALE);

    jpeg_set_defaults(&cinfo);
    jpeg_set_quality(&cinfo, quality, TRUE);
    jpeg_start_compress(&cinfo, TRUE);
    row_stride = width * (stream->red>=0?3:1);

    int row;
    for (row = 0; row < height; row++)
    {
        jpeg_write_scanlines(&cinfo, &image, 1);
        image += row_stride;
    }

    free(buf);
    jpeg_finish_compress(&cinfo);
    fclose(outfile);
    jpeg_destroy_compress(&cinfo);
}

void dsp_file_write_jpeg_composite(char *filename, int components, int quality, dsp_stream_p* stream)
{
    int width = stream[components]->sizes[0];
    int height = stream[components]->sizes[1];

    void *buf = malloc(stream[components]->len*components);
    unsigned char *image = (unsigned char *)buf;
    int x;
    for(int y = 0; y < components; y++) {
        dsp_stream_p in = dsp_stream_copy(stream[y]);
        dsp_buffer_stretch(in->buf, in->len, 0, 255);
        for(x = 0; x < in->len; x++) {
            image[x*components+y] =  in->buf[x];
        }
        dsp_stream_free_buffer(in);
        dsp_stream_free(in);
    }

    struct jpeg_compress_struct cinfo;
    struct jpeg_error_mgr jerr;
    FILE * outfile;
    int row_stride;
    cinfo.err = jpeg_std_error(&jerr);
    jpeg_create_compress(&cinfo);
    if ((outfile = fopen(filename, "wb")) == NULL) {
        fprintf(stderr, "can't open %s\n", filename);
        return;
    }
    jpeg_stdio_dest(&cinfo, outfile);
    cinfo.image_width = width;
    cinfo.image_height = height;
    cinfo.input_components = components;
    cinfo.in_color_space = (components == 1 ? JCS_GRAYSCALE : JCS_RGB);

    jpeg_set_defaults(&cinfo);
    jpeg_set_quality(&cinfo, quality, TRUE);
    jpeg_start_compress(&cinfo, TRUE);
    row_stride = width * components;

    int row;
    for (row = 0; row < height; row++)
    {
        jpeg_write_scanlines(&cinfo, &image, 1);
        image += row_stride;
    }

    free(buf);
    jpeg_finish_compress(&cinfo);
    fclose(outfile);
    jpeg_destroy_compress(&cinfo);
}

dsp_stream_p* dsp_file_read_jpeg(char *filename, int *channels, int stretch)
{
    unsigned long width, height;
    unsigned long data_size;
    int c;
    unsigned int type;
    unsigned char * rowptr[1];
    unsigned char * jdata;
    struct jpeg_decompress_struct info;
    struct jpeg_error_mgr err;

    info.err = jpeg_std_error(& err);
    jpeg_create_decompress(& info);

    FILE *jpeg = fopen (filename, "r");
    if(jpeg == NULL)
        return NULL;
    jpeg_stdio_src(&info, jpeg);
    jpeg_read_header(&info, TRUE);

    jpeg_start_decompress(&info);

    width = info.output_width;
    height = info.output_height;
    c = info.num_components;

    data_size = width * height * c;

    jdata = (unsigned char *)malloc(data_size);
    while (info.output_scanline < info.output_height)
    {
        rowptr[0] = (unsigned char *)jdata +  c* width * info.output_scanline;

        jpeg_read_scanlines(&info, rowptr, 1);
    }
    jpeg_finish_decompress(&info);

    dsp_stream_p* components = (dsp_stream_p*) malloc(sizeof(dsp_stream_p)*(c+1));
    int x, y, z;
    dsp_t* buf = (dsp_t*)malloc(sizeof(dsp_t)*width*height*c);
    for(x = 0; x < c; x++) {
        components[x] = dsp_stream_new();
        dsp_stream_add_dim(components[x], width);
        dsp_stream_add_dim(components[x], height);
        dsp_stream_set_buffer(components[x], &buf[x*components[x]->len], components[x]->len);
        for(y = 0; y < components[x]->len; y++) {
            components[x]->buf[y] = jdata[y*c+x];
        }
        if(stretch) {
            double max = dsp_stats_max(components[x]->buf, components[x]->len)*((1 << (8*sizeof(dsp_t)))-1)/256;
            dsp_buffer_stretch(components[x]->buf, components[x]->len, 0, max);
        }
    }
    components[x] = dsp_stream_new();
    dsp_stream_add_dim(components[x], width);
    dsp_stream_add_dim(components[x], height);
    dsp_stream_alloc_buffer(components[x], components[x]->len);
    for(y = 0; y < components[x]->len; y++) {
        unsigned short v = 0;
        for(z = 0; z < c; z++)
            v += jdata[y*c+z];
        components[c]->buf[y] = v/c;
    }
    if(stretch) {
        double max = dsp_stats_max(components[x]->buf, components[x]->len)*((1 << (8*sizeof(dsp_t)))-1)/256;
        dsp_buffer_stretch(components[x]->buf, components[x]->len, 0, max);
    }
    jpeg_destroy_decompress(&info);
    free(jdata);
    fclose(jpeg);
    *channels = c;
    return components;
}
