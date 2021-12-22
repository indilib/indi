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
#include <unistd.h>
#include <jpeglib.h>

dsp_stream_p* dsp_file_read_fits(char *filename, int *channels, int stretch)
{
    fitsfile *fptr = (fitsfile*)malloc(sizeof(fitsfile));
    memset(fptr, 0, sizeof(fitsfile));
    int bpp = 16;
    int status = 0;
    char value[150];
    char comment[150];
    char error_status[64];

    fits_open_file(&fptr, filename, READONLY_FILE, &status);

    if (status)
    {
        goto fail;
    }

    fits_movabs_hdu(fptr, 1, IMAGE_HDU, &status);
    if(status)
    {
        goto fail;
    }

    int dims;
    long naxes[3] = { 1, 1, 1 };
    fits_get_img_param(fptr, 3, &bpp, &dims, naxes, &status);
    if(status)
    {
        goto fail;
    }

    int dim, nelements = 1;
    for(dim = 0; dim < dims; dim++) {
        nelements *= naxes[dim];
    }
    void *array = malloc((unsigned int)(abs(bpp) * nelements / 8));
    int anynul = 0;
    dsp_t* buf = (dsp_t*)malloc(sizeof(dsp_t)*(unsigned int)nelements);
    switch(bpp) {
    case BYTE_IMG:
        fits_read_img(fptr, TBYTE, 1, (long)nelements, NULL, array, &anynul, &status);
        dsp_buffer_stretch(((unsigned char*)(array)), (long)nelements, 0, dsp_t_max);
        dsp_buffer_copy(((unsigned char*)array), buf, nelements);
        break;
    case SHORT_IMG:
        fits_read_img(fptr, TUSHORT, 1, (long)nelements, NULL, array, &anynul, &status);
        if(abs(bpp) > 8*(int)sizeof(dsp_t))
            dsp_buffer_stretch(((short*)(array)), (long)nelements, 0, dsp_t_max);
        dsp_buffer_copy(((short*)array), buf, nelements);
        break;
    case USHORT_IMG:
        fits_read_img(fptr, TUSHORT, 1, (long)nelements, NULL, array, &anynul, &status);
        if(abs(bpp) > 8*(int)sizeof(dsp_t))
            dsp_buffer_stretch(((unsigned short*)(array)), (long)nelements, 0, dsp_t_max);
        dsp_buffer_copy(((unsigned short*)array), buf, nelements);
        break;
    case LONG_IMG:
        fits_read_img(fptr, TULONG, 1, (long)nelements, NULL, array, &anynul, &status);
        if(abs(bpp) > 8*(int)sizeof(dsp_t))
            dsp_buffer_stretch(((int*)(array)), (long)nelements, 0, dsp_t_max);
        dsp_buffer_copy(((int*)array), buf, nelements);
        break;
    case ULONG_IMG:
        fits_read_img(fptr, TULONG, 1, (long)nelements, NULL, array, &anynul, &status);
        if(abs(bpp) > 8*(int)sizeof(dsp_t))
            dsp_buffer_stretch(((unsigned int*)(array)), (long)nelements, 0, dsp_t_max);
        dsp_buffer_copy(((unsigned int*)array), buf, nelements);
        break;
    case LONGLONG_IMG:
        fits_read_img(fptr, TLONGLONG, 1, (long)nelements, NULL, array, &anynul, &status);
        if(abs(bpp) > 8*(int)sizeof(dsp_t))
            dsp_buffer_stretch(((long*)(array)), (long)nelements, 0, dsp_t_max);
        dsp_buffer_copy(((long*)array), buf, nelements);
        break;
    case FLOAT_IMG:
        fits_read_img(fptr, TFLOAT, 1, (long)nelements, NULL, array, &anynul, &status);
        if(abs(bpp) > 8*(int)sizeof(dsp_t))
            dsp_buffer_stretch(((float*)(array)), (long)nelements, 0, dsp_t_max);
        dsp_buffer_copy(((float*)array), buf, nelements);
        break;
    case DOUBLE_IMG:
        fits_read_img(fptr, TDOUBLE, 1, (long)nelements, NULL, array, &anynul, &status);
        if(abs(bpp) > 8*(int)sizeof(dsp_t))
            dsp_buffer_stretch(((double*)(array)), (long)nelements, 0, dsp_t_max);
        dsp_buffer_copy(((double*)array), buf, nelements);
        break;
    }
    free(array);
    if(status||anynul)
        goto fail;
    if(stretch)
        dsp_buffer_stretch(buf, nelements, 0, dsp_t_max);

    int red = -1;
    ffgkey(fptr, "XBAYROFF", value, comment, &status);
    if (!status)
    {
        red = atoi(value);
    }

    ffgkey(fptr, "YBAYROFF", value, comment, &status);

    if (!status)
    {
        red |= atoi(value)<<1;
    }
    fits_close_file(fptr, &status);
    if(status)
        goto fail;
    int sizes[3] = { (int)naxes[0], (int)naxes[1], (int)naxes[2] };
    *channels = (int)naxes[2];
    return dsp_stream_from_components(buf, dims, sizes, sizes[2]);
fail:
    if(status) {
        fits_report_error(stderr, status); /* print out any error messages */
        fits_get_errstatus(status, error_status);
        fprintf(stderr, "FITS Error: %s\n", error_status);
    }
    return NULL;
}

void* dsp_file_write_fits(int bpp, size_t* memsize, dsp_stream_p stream)
{
    dsp_stream_p tmp = dsp_stream_copy(stream);
    int img_type  = USHORT_IMG;
    int byte_type = TUSHORT;
    char bit_depth[64] = "16 bits per sample";
    void* buf = malloc((unsigned int)(tmp->len * abs(bpp) / 8 + 512));
    void *memfile = NULL;
    int status    = 0;
    int naxis    = tmp->dims;
    long *naxes = (long*)malloc(sizeof(long) * (unsigned int)tmp->dims);
    long nelements = tmp->len;
    char error_status[64];
    unsigned int i;

    for (i = 0;  i < (unsigned int)tmp->dims; i++)
        naxes[i] = tmp->sizes[i];
    switch (bpp)
    {
        case 8:
            byte_type = TBYTE;
            img_type  = BYTE_IMG;
            dsp_buffer_copy(tmp->buf, ((unsigned char*)buf), tmp->len);
            strcpy(bit_depth, "8 bits unsigned integer per sample");
            break;

        case 16:
            byte_type = TUSHORT;
            img_type  = USHORT_IMG;
            dsp_buffer_copy(tmp->buf, ((unsigned short*)buf), tmp->len);
            strcpy(bit_depth, "16 bits unsigned integer per sample");
            break;

        case 32:
            byte_type = TULONG;
            img_type  = ULONG_IMG;
            dsp_buffer_copy(tmp->buf, ((unsigned int*)buf), tmp->len);
            strcpy(bit_depth, "32 bits unsigned integer per sample");
            break;

        case 64:
            byte_type = TLONGLONG;
            img_type  = LONGLONG_IMG;
            dsp_buffer_copy(tmp->buf, ((unsigned long*)buf), tmp->len);
            strcpy(bit_depth, "64 bits unsigned integer per sample");
            break;

        case -32:
            byte_type = TFLOAT;
            img_type  = FLOAT_IMG;
            dsp_buffer_copy(tmp->buf, ((float*)buf), tmp->len);
            strcpy(bit_depth, "32 bits floating point per sample");
            break;

        case -64:
            byte_type = TDOUBLE;
            img_type  = DOUBLE_IMG;
            dsp_buffer_copy(tmp->buf, ((double*)buf), tmp->len);
            strcpy(bit_depth, "64 bits floating point per sample");
            break;

        default:
            fprintf(stderr, "Unsupported bits per sample value %d", bpp);
            goto fail;
    }

    fitsfile *fptr = NULL;
    size_t len = 5760;
    memfile = malloc(len);
    fits_create_memfile(&fptr, &memfile, &len, 2880, realloc, &status);

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

    if (status)
    {
        goto fail_fptr;
    }

    fits_close_file(fptr, &status);

    *memsize = len;

fail_fptr:
    if(status) {
        fits_report_error(stderr, status);
        fits_get_errstatus(status, error_status);
        fprintf(stderr, "FITS Error: %s\n", error_status);
        free(memfile);
    }
fail:
    dsp_stream_free_buffer(tmp);
    dsp_stream_free(tmp);
    free(naxes);
    free(buf);
    return memfile;
}

dsp_stream_p* dsp_file_read_jpeg(char *filename, int *channels, int stretch)
{
    int width, height;
    unsigned int components;
    unsigned int bpp = 8;
    unsigned char * buf;
    struct jpeg_decompress_struct info;
    struct jpeg_error_mgr err;

    info.err = jpeg_std_error(& err);
    jpeg_create_decompress(& info);

    FILE *jpeg = fopen (filename, "r");
    if(jpeg == NULL)
        return NULL;

    jpeg_stdio_src(&info, jpeg);
    jpeg_read_header(&info, TRUE);
    info.dct_method = JDCT_FLOAT;
    jpeg_start_decompress(&info);
    width = (int)info.output_width;
    height = (int)info.output_height;
    components = (unsigned int)info.num_components;

    int row_stride = (int)components * (int)width;
    buf = (unsigned char *)malloc((unsigned int)(width * height) * components);
    unsigned char *image = buf;
    unsigned int row;
    for (row = 0; row < (unsigned int)height; row++)
    {
        jpeg_read_scanlines(&info, &image, 1);
        image += row_stride;
    }
    jpeg_finish_decompress(&info);
    *channels = (int)components;
    return dsp_buffer_rgb_to_components(buf, 2, (int[]){width, height}, (int)components, (int)bpp, stretch);
}

void dsp_file_write_jpeg_composite(char *filename, int components, int quality, dsp_stream_p* stream)
{
    int bpp = 8;
    unsigned int row_stride;
    unsigned int width = (unsigned int)stream[0]->sizes[0];
    unsigned int height = (unsigned int)stream[0]->sizes[1];
    void *buf = malloc((unsigned int)(stream[0]->len*components*bpp/8));
    unsigned char *image = (unsigned char *)buf;
    dsp_buffer_components_to_rgb(stream, buf, components, bpp);

    struct jpeg_compress_struct cinfo;
    struct jpeg_error_mgr jerr;
    FILE * outfile;
    cinfo.err = jpeg_std_error(&jerr);
    jpeg_create_compress(&cinfo);
    if ((outfile = fopen(filename, "wb")) == NULL) {
        fprintf(stderr, "can't open %s\n", filename);
        return;
    }
    jpeg_stdio_dest(&cinfo, outfile);
    cinfo.image_width = width;
    cinfo.image_height = height;
    cinfo.in_color_space = (components == 1 ? JCS_GRAYSCALE : JCS_RGB);
    cinfo.input_components = components;
    jpeg_set_defaults(&cinfo);
    cinfo.dct_method = JDCT_FLOAT;
    cinfo.optimize_coding = TRUE;
    cinfo.restart_in_rows = 1;

    jpeg_set_quality(&cinfo, quality, TRUE);
    jpeg_start_compress(&cinfo, TRUE);
    row_stride = width * (unsigned int)components;
    int row;
    for (row = 0; row < (int)height; row++)
    {
        jpeg_write_scanlines(&cinfo, &image, 1);
        image += row_stride;
    }

    jpeg_finish_compress(&cinfo);
    jpeg_destroy_compress(&cinfo);
    fclose(outfile);
    free(buf);
}

dsp_stream_p *dsp_stream_from_components(dsp_t* buf, int dims, int *sizes, int components)
{
    int d, y, x;
    dsp_stream_p* picture = (dsp_stream_p*) malloc(sizeof(dsp_stream_p)*(unsigned int)(components+1));
    for(y = 0; y <= components; y++) {
        picture[y] = dsp_stream_new();
        for(d = 0; d < dims; d++)
            dsp_stream_add_dim(picture[y], sizes[d]);
        dsp_stream_alloc_buffer(picture[y], picture[y]->len);
        if(y < components) {
            dsp_buffer_copy(((dsp_t*)&buf[y*picture[y]->len]), picture[y]->buf, picture[y]->len);
        } else {
            for(x = 0; x < picture[y]->len; x++) {
                double val = 0;
                for(d = 0; d < components; d++) {
                    val += buf[x+d*picture[y]->len];
                }
                picture[y]->buf[x] = (dsp_t)val/components;
            }
        }
    }
    free (buf);
    return picture;
}

dsp_stream_p *dsp_buffer_rgb_to_components(void* buf, int dims, int *sizes, int components, int bpp, int stretch)
{
    dsp_stream_p* picture = (dsp_stream_p*) malloc(sizeof(dsp_stream_p)*(unsigned int)(components+1));
    int x, y, z, d;
    for(y = 0; y < components; y++) {
        picture[y] = dsp_stream_new();
        for(d = 0; d < dims; d++)
            dsp_stream_add_dim(picture[y], sizes[d]);
        dsp_stream_alloc_buffer(picture[y], picture[y]->len);
        switch(bpp) {
        case 8:
            dsp_buffer_copy_stepping(((unsigned char*)&((unsigned char*)buf)[y]), picture[y]->buf, picture[y]->len*components, picture[y]->len, components, 1);
            break;
        case 16:
            dsp_buffer_copy_stepping(((unsigned short*)&((unsigned short*)buf)[y]), picture[y]->buf, picture[y]->len*components, picture[y]->len, components, 1);
            break;
        case 32:
            dsp_buffer_copy_stepping(((unsigned int*)&((unsigned int*)buf)[y]), picture[y]->buf, picture[y]->len*components, picture[y]->len, components, 1);
            break;
        case 64:
            dsp_buffer_copy_stepping(((unsigned long*)&((unsigned long*)buf)[y]), picture[y]->buf, picture[y]->len*components, picture[y]->len, components, 1);
            break;
        case -32:
            dsp_buffer_copy_stepping(((float*)&((float*)buf)[y]), picture[y]->buf, picture[y]->len*components, picture[y]->len, components, 1);
            break;
        case -64:
            dsp_buffer_copy_stepping(((double*)&((double*)buf)[y]), picture[y]->buf, picture[y]->len*components, picture[y]->len, components, 1);
            break;
        default:
            break;
        }
        if(stretch) {
            dsp_buffer_stretch(picture[y]->buf, picture[y]->len, 0, dsp_t_max);
        }
    }
    picture[y] = dsp_stream_new();
    for(d = 0; d < dims; d++)
        dsp_stream_add_dim(picture[y], sizes[d]);
    dsp_stream_alloc_buffer(picture[y], picture[y]->len);
    for(x = 0; x < picture[y]->len; x++) {
        double v = 0;
        switch(bpp) {
        case 8:
            for(z = 0; z < y; z++) v += ((unsigned char*)buf)[x*y+z];
            break;
        case 16:
            for(z = 0; z < y; z++) v += ((unsigned short*)buf)[x*y+z];
            break;
        case 32:
            for(z = 0; z < y; z++) v += ((unsigned int*)buf)[x*y+z];
            break;
        case 64:
            for(z = 0; z < y; z++) v += ((unsigned long*)buf)[x*y+z];
            break;
        case -32:
            for(z = 0; z < y; z++) v += (double)((float*)buf)[x*y+z];
            break;
        case -64:
            for(z = 0; z < y; z++) v += ((double*)buf)[x*y+z];
            break;
        default:
            break;
        }
        picture[y]->buf[x] = (dsp_t)(v/y);
    }
    if(stretch) {
        dsp_buffer_stretch(picture[y]->buf, picture[y]->len, 0, dsp_t_max);
    }
    free (buf);
    return picture;
}

void dsp_buffer_components_to_rgb(dsp_stream_p *stream, void* rgb, int components, int bpp)
{
    size_t y;
    int len = stream[0]->len * components;
    dsp_t max = (dsp_t)((double)((1<<abs(bpp))-1));
    max = Min(max, (dsp_t)~0);
    for(y = 0; y < (size_t)components; y++) {
        dsp_stream_p in = dsp_stream_copy(stream[y]);
        dsp_buffer_stretch(in->buf, in->len, 0, max);
        switch(bpp) {
        case 8:
            dsp_buffer_copy_stepping(in->buf, ((unsigned char*)&((unsigned char*)rgb)[y]), in->len, len, 1, components);
            break;
        case 16:
            dsp_buffer_copy_stepping(in->buf, ((unsigned short*)&((unsigned short*)rgb)[y]), in->len, len, 1, components);
            break;
        case 32:
            dsp_buffer_copy_stepping(in->buf, ((unsigned int*)&((unsigned int*)rgb)[y]), in->len, len, 1, components);
            break;
        case 64:
            dsp_buffer_copy_stepping(in->buf, ((unsigned long*)&((unsigned long*)rgb)[y]), in->len, len, 1, components);
            break;
        case -32:
            dsp_buffer_copy_stepping(in->buf, ((float*)&((float*)rgb)[y]), in->len, len, 1, components);
            break;
        case -64:
            dsp_buffer_copy_stepping(in->buf, ((double*)&((double*)rgb)[y]), in->len, len, 1, components);
            break;
        }
        dsp_stream_free_buffer(in);
        dsp_stream_free(in);
    }
}
