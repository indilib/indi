/*
*   DSP API - a digital signal processing library for astronomy usage
*   Copyright © 2017-2022  Ilia Platone
*
*   This program is free software; you can redistribute it and/or
*   modify it under the terms of the GNU Lesser General Public
*   License as published by the Free Software Foundation; either
*   version 3 of the License, or (at your option) any later version.
*
*   This program is distributed in the hope that it will be useful,
*   but WITHOUT ANY WARRANTY; without even the implied warranty of
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
*   Lesser General Public License for more details.
*
*   You should have received a copy of the GNU Lesser General Public License
*   along with this program; if not, write to the Free Software Foundation,
*   Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/

#include "dsp.h"
#include <fitsio.h>
#include <time.h>
#include <locale.h>
#include <unistd.h>
#include <jpeglib.h>
#include <png.h>

dsp_stream_p* dsp_file_read_fits(const char* filename, int *channels, int stretch)
{
    fitsfile *fptr;
    int bpp = 16;
    int status = 0;
    char value[150];
    char comment[150];
    char error_status[64];

    fits_open_file(&fptr, filename, READONLY, &status);

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
    bpp = fmax(16, bpp);
    int dim, nelements = 1;
    for(dim = 0; dim < dims; dim++) {
        nelements *= naxes[dim];
    }
    void *array = malloc((size_t)(abs(bpp) * nelements / 8));
    int anynul = 0;
    dsp_t* buf = (dsp_t*)malloc(sizeof(dsp_t)*(size_t)(nelements+1));
    switch(bpp) {
    case BYTE_IMG:
        fits_read_img(fptr, TBYTE, 1, (long)nelements, NULL, array, &anynul, &status);
        dsp_buffer_stretch(((unsigned char*)(array)), (long)nelements, 0, dsp_t_max);
        dsp_buffer_copy(((unsigned char*)array), buf, nelements);
        break;
    case SHORT_IMG:
        fits_read_img(fptr, TUSHORT, 1, (long)nelements, NULL, array, &anynul, &status);
        if(abs(bpp) > 8*sizeof(dsp_t))
            dsp_buffer_stretch(((unsigned short*)(array)), (long)nelements, 0, dsp_t_max);
        dsp_buffer_copy(((unsigned short*)array), buf, nelements);
        break;
    case USHORT_IMG:
        fits_read_img(fptr, TUSHORT, 1, (long)nelements, NULL, array, &anynul, &status);
        if(abs(bpp) > 8*sizeof(dsp_t))
            dsp_buffer_stretch(((unsigned short*)(array)), (long)nelements, 0, dsp_t_max);
        dsp_buffer_copy(((unsigned short*)array), buf, nelements);
        break;
    case LONG_IMG:
        fits_read_img(fptr, TULONG, 1, (long)nelements, NULL, array, &anynul, &status);
        if(abs(bpp) > 8*sizeof(dsp_t))
            dsp_buffer_stretch(((int*)(array)), (long)nelements, 0, dsp_t_max);
        dsp_buffer_copy(((int*)array), buf, nelements);
        break;
    case ULONG_IMG:
        fits_read_img(fptr, TULONG, 1, (long)nelements, NULL, array, &anynul, &status);
        if(abs(bpp) > 8*sizeof(dsp_t))
            dsp_buffer_stretch(((unsigned int*)(array)), (long)nelements, 0, dsp_t_max);
        dsp_buffer_copy(((unsigned int*)array), buf, nelements);
        break;
    case LONGLONG_IMG:
        fits_read_img(fptr, TLONGLONG, 1, (long)nelements, NULL, array, &anynul, &status);
        if(abs(bpp) > 8*sizeof(dsp_t))
            dsp_buffer_stretch(((long*)(array)), (long)nelements, 0, dsp_t_max);
        dsp_buffer_copy(((long*)array), buf, nelements);
        break;
    case FLOAT_IMG:
        fits_read_img(fptr, TFLOAT, 1, (long)nelements, NULL, array, &anynul, &status);
        if(abs(bpp) > 8*sizeof(dsp_t))
            dsp_buffer_stretch(((float*)(array)), (long)nelements, 0, dsp_t_max);
        dsp_buffer_copy(((float*)array), buf, nelements);
        break;
    case DOUBLE_IMG:
        fits_read_img(fptr, TDOUBLE, 1, (long)nelements, NULL, array, &anynul, &status);
        if(abs(bpp) > 8*sizeof(dsp_t))
            dsp_buffer_stretch(((double*)(array)), (long)nelements, 0, dsp_t_max);
        dsp_buffer_copy(((double*)array), buf, nelements);
        break;
    }
    free(array);
    if(status||anynul)
        goto fail;
    int red = -1;
    ffgkey(fptr, "XBAYROFF", value, comment, &status);
    if (!status)
    {
        red = atoi(value);
    }
    status = 0;
    ffgkey(fptr, "YBAYROFF", value, comment, &status);

    if (!status)
    {
        red |= atoi(value)<<1;
    }
    status = 0;
    fits_close_file(fptr, &status);
    if(status)
        goto fail;
    dsp_stream_p* stream;
    int x;
    void* rgb = NULL;
    int sizes[2] = { naxes[0], naxes[1] };
    if(red > -1) {
        *channels = 3;
        rgb = dsp_file_bayer_2_rgb(buf, red, naxes[0], naxes[1]);
        stream =  dsp_buffer_rgb_to_components(rgb, dims, sizes, *channels, 32, 0);
    } else {
        *channels = naxes[2];
        dims = 2;
        stream = dsp_stream_from_components(buf, dims, sizes, *channels);
    }
    free(buf);
    if(stream) {
        if(stretch) {
            for (x = 0; x < *channels; x++) {
                dsp_buffer_pow1(stream[x], 0.5);
                dsp_buffer_stretch(stream[x]->buf, stream[x]->len, 0, dsp_t_max);
            }
        }
        return stream;
    }
fail:
    if(status) {
        fits_get_errstatus(status, error_status);
        perr("FITS Error: %s\n", error_status);
    }
    return NULL;
}

void dsp_file_write_fits(const char* filename, int bpp, dsp_stream_p stream)
{
    dsp_stream_p tmp = dsp_stream_copy(stream);
    int img_type  = USHORT_IMG;
    int byte_type = TUSHORT;
    char bit_depth[64] = "16 bits per sample";
    void* buf = malloc((size_t)(tmp->len * abs(bpp) / 8 + 512));
    int status    = 0;
    int naxis    = tmp->dims;
    long *naxes = (long*)malloc(sizeof(long) * (size_t)tmp->dims);
    long nelements = tmp->len;
    char error_status[64];
    int i;
    dsp_buffer_stretch(tmp->buf, tmp->len, 0, dsp_t_max);
    for (i = 0;  i < tmp->dims; i++)
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
            perr("Unsupported bits per sample value %d", bpp);
            goto fail;
    }

    unlink(filename);
    fitsfile *fptr;
    fits_create_file(&fptr, filename, &status);

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

fail_fptr:
    if(status) {
        fits_get_errstatus(status, error_status);
        perr("FITS Error: %s\n", error_status);
    }
fail:
    dsp_stream_free_buffer(tmp);
    dsp_stream_free(tmp);
    free(naxes);
    free (buf);
}

void dsp_file_write_fits_composite(const char* filename, int components, int bpp, dsp_stream_p* stream)
{
    int x;
    dsp_stream_p tmp = stream[components];
    int img_type  = USHORT_IMG;
    int byte_type = TUSHORT;
    char bit_depth[64] = "16 bits per sample";
    void* buf = malloc((size_t)(tmp->len * components * abs(bpp) / 8 + 512));
    int status    = 0;
    int naxis    = tmp->dims + 1;
    long *naxes = (long*)malloc(sizeof(long) * (size_t)(tmp->dims + 1));
    long nelements = tmp->len * components;
    char error_status[64];
    int i;
    for (i = 0;  i < tmp->dims; i++)
        naxes[i] = tmp->sizes[i];
    naxes[i] = components;
    dsp_t max = (1<<abs(bpp))/2-1;
    for(x = 0; x < components; x++) {
        tmp = dsp_stream_copy(stream[x]);
        dsp_buffer_stretch(tmp->buf, tmp->len, 0, (dsp_t)max);
        switch (bpp)
        {
            case 8:
                byte_type = TBYTE;
                img_type  = BYTE_IMG;
                dsp_buffer_copy(tmp->buf, ((unsigned char*)&((unsigned char*)buf)[tmp->len*x]), tmp->len);
                strcpy(bit_depth, "8 bits unsigned integer per sample");
                break;

            case 16:
                byte_type = TUSHORT;
                img_type  = USHORT_IMG;
                dsp_buffer_copy(tmp->buf, ((unsigned short*)&((unsigned short*)buf)[tmp->len*x]), tmp->len);
                strcpy(bit_depth, "16 bits unsigned integer per sample");
                break;

            case 32:
                byte_type = TULONG;
                img_type  = ULONG_IMG;
                dsp_buffer_copy(tmp->buf, ((unsigned int*)&((unsigned int*)buf)[tmp->len*x]), tmp->len);
                strcpy(bit_depth, "32 bits unsigned integer per sample");
                break;

            case 64:
                byte_type = TLONGLONG;
                img_type  = LONGLONG_IMG;
                dsp_buffer_copy(tmp->buf, ((unsigned long*)&((unsigned long*)buf)[tmp->len*x]), tmp->len);
                strcpy(bit_depth, "64 bits unsigned integer per sample");
                break;

            case -32:
                byte_type = TFLOAT;
                img_type  = FLOAT_IMG;
                dsp_buffer_copy(tmp->buf, ((float*)&((float*)buf)[tmp->len*x]), tmp->len);
                strcpy(bit_depth, "32 bits floating point per sample");
                break;

            case -64:
                byte_type = TDOUBLE;
                img_type  = DOUBLE_IMG;
                dsp_buffer_copy(tmp->buf, ((double*)&((double*)buf)[tmp->len*x]), tmp->len);
                strcpy(bit_depth, "64 bits floating point per sample");
            break;

            default:
                perr("Unsupported bits per sample value %d", bpp);
                break;
        }
        dsp_stream_free_buffer(tmp);
        dsp_stream_free(tmp);
    }

    unlink(filename);
    fitsfile *fptr;
    fits_create_file(&fptr, filename, &status);

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

fail_fptr:
    if(status) {
        fits_get_errstatus(status, error_status);
        perr("FITS Error: %s\n", error_status);
    }
    free(naxes);
    free (buf);
}


void dsp_file_write_fits_bayer(const char* filename, int components, int bpp, dsp_stream_p* stream)
{
    int red = 0;
    int x;
    dsp_stream_p tmp = dsp_stream_copy(stream[components]);
    int img_type  = USHORT_IMG;
    int byte_type = TUSHORT;
    char bit_depth[64] = "16 bits per sample";
    int len = tmp->len;
    void* data = malloc((size_t)(len * abs(bpp) / 8 + 512));
    int status    = 0;
    int naxis    = tmp->dims;
    long *naxes = (long*)malloc(sizeof(long) * (size_t)(tmp->dims));
    long nelements = tmp->len;
    char error_status[64];
    int i;
    for (i = 0;  i < tmp->dims; i++)
        naxes[i] = tmp->sizes[i];
    dsp_t *buf = dsp_file_composite_2_bayer(stream, red, tmp->sizes[0], tmp->sizes[1]);
    dsp_stream_free_buffer(tmp);
    dsp_stream_free(tmp);
    for(x = 0; x < components; x++) {
        dsp_buffer_stretch(buf, stream[components]->len, 0, (dsp_t)(1<<abs(bpp))-1);
        switch (bpp)
        {
            case 8:
                byte_type = TBYTE;
                img_type  = BYTE_IMG;
                dsp_buffer_copy(buf, ((unsigned char*)((unsigned char*)data)), stream[components]->len);
                strcpy(bit_depth, "8 bits unsigned integer per sample");
                break;

            case 16:
                byte_type = TUSHORT;
                img_type  = USHORT_IMG;
                dsp_buffer_copy(buf, ((unsigned short*)((unsigned short*)data)), stream[components]->len);
                strcpy(bit_depth, "16 bits unsigned integer per sample");
                break;

            case 32:
                byte_type = TULONG;
                img_type  = ULONG_IMG;
                dsp_buffer_copy(buf, ((unsigned int*)((unsigned int*)data)), stream[components]->len);
                strcpy(bit_depth, "32 bits unsigned integer per sample");
                break;

            case 64:
                byte_type = TLONGLONG;
                img_type  = LONGLONG_IMG;
                dsp_buffer_copy(buf, ((unsigned long*)((unsigned long*)data)), stream[components]->len);
                strcpy(bit_depth, "64 bits unsigned integer per sample");
                break;

            case -32:
                byte_type = TFLOAT;
                img_type  = FLOAT_IMG;
                dsp_buffer_copy(buf, ((float*)((float*)data)), stream[components]->len);
                strcpy(bit_depth, "32 bits floating point per sample");
                break;

            case -64:
                byte_type = TDOUBLE;
                img_type  = DOUBLE_IMG;
                dsp_buffer_copy(buf, ((double*)((double*)data)), stream[components]->len);
                strcpy(bit_depth, "64 bits floating point per sample");
            break;

            default:
                perr("Unsupported bits per sample value %d", bpp);
                break;
        }
    }

    unlink(filename);
    fitsfile *fptr;
    status = 0;
    fits_create_file(&fptr, filename, &status);

    if (status)
    {
        goto fail_fptr;
    }

    fits_create_img(fptr, img_type, naxis, naxes, &status);

    if (status)
    {
        goto fail_fptr;
    }

    int redx = red&1;
    int redy = (red>>1)&1;

    fits_write_key(fptr, TINT, "XBAYROFF", &redx, "X Bayer Offset", &status);
    if (status)
    {
        goto fail_fptr;
    }

    fits_write_key(fptr, TINT, "YBAYROFF", &redy, "Y Bayer Offset", &status);
    if (status)
    {
        goto fail_fptr;
    }
    switch (red) {
    case 0:
        fits_write_key(fptr, TSTRING, "BAYERPAT", "RGGB", "Y Bayer Offset", &status);
        break;
    case 1:
        fits_write_key(fptr, TSTRING, "BAYERPAT", "GRGB", "Y Bayer Offset", &status);
        break;
    case 2:
        fits_write_key(fptr, TSTRING, "BAYERPAT", "GBRG", "Y Bayer Offset", &status);
        break;
    case 3:
        fits_write_key(fptr, TSTRING, "BAYERPAT", "BGGR", "Y Bayer Offset", &status);
        break;
    }
    if (status)
    {
        goto fail_fptr;
    }

    fits_write_img(fptr, byte_type, 1, nelements, data, &status);

    if (status)
    {
        goto fail_fptr;
    }

    fits_close_file(fptr, &status);

fail_fptr:
    if(status) {
        fits_get_errstatus(status, error_status);
        perr("FITS Error: %s\n", error_status);
    }
    free(naxes);
    free (data);
}
dsp_stream_p* dsp_file_read_jpeg(const char* filename, int *channels, int stretch)
{
    int width, height;
    int components;
    int bpp = 8;
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
    components = (int)info.num_components;

    int row_stride = (components * width);
    buf = (unsigned char *)malloc((size_t)(width * height * components));
    unsigned char *image = buf;
    int row;
    for (row = 0; row < height; row++)
    {
        jpeg_read_scanlines(&info, &image, 1);
        image += row_stride;
    }
    jpeg_finish_decompress(&info);
    *channels = components;
    return dsp_buffer_rgb_to_components(buf, 2, (int[]){width, height}, components, bpp, stretch);
}

void dsp_file_write_jpeg(const char* filename, int quality, dsp_stream_p stream)
{
    int width = stream->sizes[0];
    int height = stream->sizes[1];
    int components = (stream->red>=0) ? 3 : 1;
    void *buf = malloc((size_t)(stream->len*(stream->red>=0?3:1)));
    unsigned char *image = (unsigned char *)buf;
    dsp_t* data = stream->buf;

    if(components > 1)
        data = dsp_file_bayer_2_rgb(stream->buf, stream->red, width, height);
    dsp_buffer_stretch(data, stream->len*(stream->red>=0?3:1), 0, 255);
    dsp_buffer_copy(data, image, stream->len*(stream->red>=0?3:1));

    struct jpeg_compress_struct cinfo;
    struct jpeg_error_mgr jerr;
    FILE * outfile;
    int row_stride;
    cinfo.err = jpeg_std_error(&jerr);
    jpeg_create_compress(&cinfo);
    if ((outfile = fopen(filename, "wb")) == NULL) {
        perr("can't open %s\n", filename);
        return;
    }
    jpeg_stdio_dest(&cinfo, outfile);
    cinfo.image_width = (unsigned int)width;
    cinfo.image_height = (unsigned int)height;
    cinfo.in_color_space = (components == 1 ? JCS_GRAYSCALE : JCS_RGB);
    cinfo.input_components = components;
    jpeg_set_defaults(&cinfo);
    cinfo.dct_method = JDCT_FLOAT;
    cinfo.optimize_coding = TRUE;
    cinfo.restart_in_rows = 1;

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

void dsp_file_write_jpeg_composite(const char* filename, int components, int quality, dsp_stream_p* stream)
{
    int bpp = 8;
    unsigned int row_stride;
    int width = stream[components]->sizes[0];
    int height = stream[components]->sizes[1];
    void *buf = malloc((size_t)(stream[components]->len*components));
    unsigned char *image = (unsigned char *)buf;
    dsp_buffer_components_to_rgb(stream, buf, components, bpp);

    struct jpeg_compress_struct cinfo;
    struct jpeg_error_mgr jerr;
    FILE * outfile;
    cinfo.err = jpeg_std_error(&jerr);
    if ((outfile = fopen(filename, "wb")) == NULL) {
        perr("can't open %s\n", filename);
        return;
    }
    jpeg_create_compress(&cinfo);
    jpeg_stdio_dest(&cinfo, outfile);
    cinfo.image_width = (unsigned int)width;
    cinfo.image_height = (unsigned int)height;
    cinfo.in_color_space = (components == 1 ? JCS_GRAYSCALE : JCS_RGB);
    cinfo.input_components = components;
    cinfo.dct_method = JDCT_FLOAT;
    cinfo.optimize_coding = TRUE;
    cinfo.restart_in_rows = 1;
    jpeg_set_defaults(&cinfo);

    jpeg_set_quality(&cinfo, quality, TRUE);
    jpeg_start_compress(&cinfo, TRUE);
    row_stride = (unsigned int)width * (unsigned int)components;
    int row;
    for (row = 0; row < height; row++)
    {
        jpeg_write_scanlines(&cinfo, &image, 1);
        image += row_stride;
    }

    jpeg_finish_compress(&cinfo);
    jpeg_destroy_compress(&cinfo);
    fclose(outfile);
    free(buf);
}

dsp_stream_p* dsp_file_read_png(const char* filename, int *channels, int stretch)
{
    int width, height;
    int components;
    unsigned int type;
    unsigned int row_stride;
    int bpp;
    unsigned char * buf;

    FILE *infile = fopen (filename, "r");
    if(infile == NULL)
        return NULL;
    png_structp png = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png)
        return NULL;
    png_infop info = png_create_info_struct(png);
    if (!info)
        return NULL;
    png_infop end_info = png_create_info_struct(png);
    if (!end_info)
        return NULL;
    if (setjmp(png_jmpbuf(png)))
        return NULL;
    png_init_io(png, infile);
    png_read_info(png, info);
    width = (int)png_get_image_width(png, info);
    height = (int)png_get_image_height(png, info);
    type = png_get_color_type(png, info);
    bpp = png_get_bit_depth(png, info);
    if (type & PNG_COLOR_MASK_PALETTE)
        png_set_palette_to_rgb(png);
    if (type == PNG_COLOR_TYPE_GRAY && bpp < 8) {
        png_set_expand_gray_1_2_4_to_8(png);
        bpp = 8;
    }
    png_read_update_info(png, info);
    components = (type & (PNG_COLOR_MASK_COLOR|PNG_COLOR_MASK_PALETTE)) ? 3 : 1;
    if (type & PNG_COLOR_MASK_ALPHA)
        components++;
    row_stride = (unsigned int)(width * components * bpp / 8);
    buf = (unsigned char *)malloc(row_stride * (size_t)(height));
    unsigned char *image = (unsigned char *)buf;
    int row;
    for (row = 0; row < height; row++) {
        png_read_row(png, image, NULL);
        image += row_stride;
    }
    png_destroy_read_struct(&png, &info, &end_info);
    fclose(infile);
    if (type & PNG_COLOR_MASK_ALPHA)
        components--;
    *channels = components;
    if(bpp == 16)
        dsp_buffer_swap(((unsigned short*)buf), width * height * components);
    return dsp_buffer_rgb_to_components(buf, 2, (int[]){width, height}, components, bpp, stretch);
}

void dsp_file_write_png_composite(const char* filename, int components, int compression, dsp_stream_p* stream)
{
    int bpp = 16;
    unsigned int row_stride;
    int width = stream[0]->sizes[0];
    int height = stream[0]->sizes[1];

    png_structp png = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png)
        return;
    png_infop info = png_create_info_struct(png);
    if (!info)
        return;
    if (setjmp(png_jmpbuf(png)))
        return;
    FILE * outfile;
    if ((outfile = fopen(filename, "wb")) == NULL) {
        perr("can't open %s\n", filename);
        return;
    }
    png_init_io(png, outfile);
    png_set_IHDR(png,
                 info,
                 (unsigned int)width,
                 (unsigned int)height,
                 bpp,
                 components == 1 ? PNG_COLOR_TYPE_GRAY : PNG_COLOR_TYPE_RGB,
                 PNG_INTERLACE_NONE,
                 PNG_COMPRESSION_TYPE_DEFAULT,
                 PNG_FILTER_TYPE_DEFAULT);
    png_set_compression_level(png, compression);
    png_write_info(png, info);
    row_stride = (unsigned int)(width * components * bpp / 8);
    int row;
    void *buf = malloc((size_t)(stream[0]->len*components*bpp/8));
    unsigned char *image = (unsigned char *)buf;
    dsp_buffer_components_to_rgb(stream, image, components, bpp);
    for (row = 0; row < height; row++) {
        png_write_row(png, image);
        image += row_stride;
    }
    png_destroy_write_struct(&png, &info);
    free(buf);
    fclose(outfile);
}

dsp_t* dsp_file_bayer_2_gray(dsp_t *src, int width, int height)
{
    int i;
    dsp_t *rawpt, *scanpt;
    int size;

    dsp_t * dst = (dsp_t*)malloc(sizeof(dsp_t)*(size_t)(width*height));
    rawpt  = src;
    scanpt = dst;
    size   = width * height;
    dsp_t val = 0;
    for (i = 0; i < size; i++)
    {
        if ((i / width) % 2 == 0)
        {
            if ((i % 2) == 0)
            {
                /* B */
                if ((i > width) && ((i % width) > 0))
                {
                    val =
                        (*(rawpt - width - 1) + *(rawpt - width + 1) + *(rawpt + width - 1) + *(rawpt + width + 1)) /
                        4;                                                                               /* R */
                    val += (*(rawpt - 1) + *(rawpt + 1) + *(rawpt + width) + *(rawpt - width)) / 4; /* G */
                    val += *rawpt;                                                                  /* B */
                }
                else
                {
                    val = *(rawpt + width + 1);                  /* R */
                    val += (*(rawpt + 1) + *(rawpt + width)) / 2; /* G */
                    val += *rawpt;                                /* B */
                }
            }
            else
            {
                /* (B)G */
                if ((i > width) && ((i % width) < (width - 1)))
                {
                    val = (*(rawpt + width) + *(rawpt - width)) / 2; /* R */
                    val += *rawpt;                                    /* G */
                    val += (*(rawpt - 1) + *(rawpt + 1)) / 2;         /* B */
                }
                else
                {
                    val = *(rawpt + width); /* R */
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
                if ((i < (width * (height - 1))) && ((i % width) > 0))
                {
                    val = (*(rawpt - 1) + *(rawpt + 1)) / 2;         /* R */
                    val += *rawpt;                                    /* G */
                    val += (*(rawpt + width) + *(rawpt - width)) / 2; /* B */
                }
                else
                {
                    /* bottom line or left column */
                    val = *(rawpt + 1);     /* R */
                    val += *rawpt;           /* G */
                    val += *(rawpt - width); /* B */
                }
            }
            else
            {
                /* R */
                if (i < (width * (height - 1)) && ((i % width) < (width - 1)))
                {
                    val = *rawpt;                                                                  /* R */
                    val += (*(rawpt - 1) + *(rawpt + 1) + *(rawpt - width) + *(rawpt + width)) / 4; /* G */
                    val +=
                        (*(rawpt - width - 1) + *(rawpt - width + 1) + *(rawpt + width - 1) + *(rawpt + width + 1)) /
                        4; /* B */
                }
                else
                {
                    /* bottom line or right column */
                    val = *rawpt;                                /* R */
                    val += (*(rawpt - 1) + *(rawpt - width)) / 2; /* G */
                    val += *(rawpt - width - 1);                  /* B */
                }
            }
        }
        *scanpt++ = val;
        rawpt++;
    }
    return dst;
}


dsp_t* dsp_file_composite_2_bayer(dsp_stream_p *src, int r, int width, int height)
{
    int i;
    dsp_t *rawpt, *red, *green, *blue;
    int size;
    dsp_t *dst = (dsp_t *)malloc(sizeof(dsp_t)*(size_t)(width*height));
    rawpt  = dst;
    blue = src[0]->buf;
    green = src[1]->buf;
    red = src[2]->buf;
    size   = width * height;
    for (i = 0; i < size; i++)
    {
        if ((i / width) % 2 == ((r&2)>>1))
        {
            if ((i % 2) == (r&1))
            {
                /* B */
                if ((i > width) && ((i % width) > 0))
                {
                    rawpt[i - width - 1] += red[i];
                    rawpt[i - width + 1] += red[i];
                    rawpt[i + width - 1] += red[i];
                    rawpt[i + width + 1] += red[i];
                    rawpt[i - 1] += green[i];
                    rawpt[i + 1] += green[i];
                    rawpt[i + width] += green[i];
                    rawpt[i - width] += green[i];
                    rawpt[i] += blue[i];
                }
                else
                {
                    rawpt[i + width + 1] += red[i];
                    rawpt[i + 1] += green[i];
                    rawpt[i + width] += green[i];
                    rawpt[i] += blue[i];
                }
            }
            else
            {
                /* (B)G */
                if ((i > width) && ((i % width) < (width - 1)))
                {
                    rawpt[i + width] += red[i];
                    rawpt[i - width] += red[i];
                    rawpt[i] += green[i];
                    rawpt[i - 1] += blue[i];
                    rawpt[i + 1] += blue[i];
                }
                else
                {
                    rawpt[i + width] += red[i];
                    rawpt[i] += green[i];
                    rawpt[i - 1] += blue[i];
                }
            }
        }
        else
        {
            if ((i % 2) == (r&1))
            {
                /* G(R) */
                if ((i < (width * (height - 1))) && ((i % width) > 0))
                {
                    rawpt[i - 1] += red[i];
                    rawpt[i + 1] += red[i];
                    rawpt[i] += green[i];
                    rawpt[i + width] += blue[i];
                    rawpt[i - width] += blue[i];
                }
                else
                {
                    rawpt[i + 1] += red[i];
                    rawpt[i] += green[i];
                    rawpt[i - width] += blue[i];
                }
            }
            else
            {
                if (i < (width * (height - 1)) && ((i % width) < (width - 1)))
                {
                    rawpt[i] = red[i];
                    rawpt[i - 1] += green[i];
                    rawpt[i + 1] += green[i];
                    rawpt[i - width] += green[i];
                    rawpt[i + width] += green[i];
                    rawpt[i - width - 1] += blue[i];
                    rawpt[i - width + 1] += blue[i];
                    rawpt[i + width + 1] += blue[i];
                    rawpt[i + width + 1] += blue[i];
                }
                else
                {
                    /* bottom line or right column */
                    rawpt[i] += red[i];
                    rawpt[i - 1] += green[i];
                    rawpt[i - width] += green[i];
                    rawpt[i - width - 1] += blue[i];
                }
            }
        }
    }
    return dst;
}


dsp_t* dsp_file_bayer_2_composite(dsp_t *src, int r, int width, int height)
{
    int i;
    dsp_t *rawpt, *red, *green, *blue;
    int size;

    dsp_t *dst = (dsp_t *)malloc(sizeof(dsp_t)*(size_t)(width*height*3));
    rawpt  = src;
    blue = &dst[0];
    green = &dst[width*height];
    red = &dst[width*height*2];
    size   = width * height;
    for (i = 0; i < size; i++)
    {
        if ((i / width) % 2 == ((r&2)>>1))
        {
            if ((i % 2) == (r&1))
            {
                /* B */
                if ((i > width) && ((i % width) > 0))
                {
                    *red++ =
                        (*(rawpt - width - 1) + *(rawpt - width + 1) + *(rawpt + width - 1) + *(rawpt + width + 1)) /
                        4;                                                                               /* R */
                    *green++ = (*(rawpt - 1) + *(rawpt + 1) + *(rawpt + width) + *(rawpt - width)) / 4; /* G */
                    *blue++ = *rawpt;                                                                  /* B */
                }
                else
                {
                    *red++ = *(rawpt + width + 1);                  /* R */
                    *green++ = (*(rawpt + 1) + *(rawpt + width)) / 2; /* G */
                    *blue++ = *rawpt;                                /* B */
                }
            }
            else
            {
                /* (B)G */
                if ((i > width) && ((i % width) < (width - 1)))
                {
                    *red++ = (*(rawpt + width) + *(rawpt - width)) / 2; /* R */
                    *green++ = *rawpt;                                    /* G */
                    *blue++ = (*(rawpt - 1) + *(rawpt + 1)) / 2;         /* B */
                }
                else
                {
                    *red++ = *(rawpt + width); /* R */
                    *green++ = *rawpt;           /* G */
                    *blue++ = *(rawpt - 1);     /* B */
                }
            }
        }
        else
        {
            if ((i % 2) == (r&1))
            {
                /* G(R) */
                if ((i < (width * (height - 1))) && ((i % width) > 0))
                {
                    *red++ = (*(rawpt - 1) + *(rawpt + 1)) / 2;         /* R */
                    *green++ = *rawpt;                                    /* G */
                    *blue++ = (*(rawpt + width) + *(rawpt - width)) / 2; /* B */
                }
                else
                {
                    /* bottom line or left column */
                    *red++ = *(rawpt + 1);     /* R */
                    *green++ = *rawpt;           /* G */
                    *blue++ = *(rawpt - width); /* B */
                }
            }
            else
            {
                /* R */
                if (i < (width * (height - 1)) && ((i % width) < (width - 1)))
                {
                    *red++ = *rawpt;                                                                  /* R */
                    *green++ = (*(rawpt - 1) + *(rawpt + 1) + *(rawpt - width) + *(rawpt + width)) / 4; /* G */
                    *blue++ =
                        (*(rawpt - width - 1) + *(rawpt - width + 1) + *(rawpt + width - 1) + *(rawpt + width + 1)) /
                        4; /* B */
                }
                else
                {
                    /* bottom line or right column */
                    *red++ = *rawpt;                                /* R */
                    *green++ = (*(rawpt - 1) + *(rawpt - width)) / 2; /* G */
                    *blue++ = *(rawpt - width - 1);                  /* B */
                }
            }
        }
        rawpt++;
    }
    return dst;
}


dsp_t* dsp_file_bayer_2_rgb(dsp_t *src, int red, int width, int height)
{
    int i;
    dsp_t *rawpt, *scanpt;
    int size;

    dsp_t * dst = (dsp_t*)malloc(sizeof(dsp_t)*(size_t)(width*height*3));
    rawpt  = src;
    scanpt = dst;
    size   = width * height;
    (void)*scanpt++;
    for (i = 0; i < size; i++)
    {
        if ((i / width) % 2 == ((red&2)>>1))
        {
            if ((i % 2) == (red&1))
            {
                /* B */
                if ((i > width) && ((i % width) > 0))
                {
                    *scanpt++ =
                        (*(rawpt - width - 1) + *(rawpt - width + 1) + *(rawpt + width - 1) + *(rawpt + width + 1)) /
                        4;                                                                               /* R */
                    *scanpt++ = (*(rawpt - 1) + *(rawpt + 1) + *(rawpt + width) + *(rawpt - width)) / 4; /* G */
                    *scanpt++ = *rawpt;                                                                  /* B */
                }
                else
                {
                    *scanpt++ = *(rawpt + width + 1);                  /* R */
                    *scanpt++ = (*(rawpt + 1) + *(rawpt + width)) / 2; /* G */
                    *scanpt++ = *rawpt;                                /* B */
                }
            }
            else
            {
                /* (B)G */
                if ((i > width) && ((i % width) < (width - 1)))
                {
                    *scanpt++ = (*(rawpt + width) + *(rawpt - width)) / 2; /* R */
                    *scanpt++ = *rawpt;                                    /* G */
                    *scanpt++ = (*(rawpt - 1) + *(rawpt + 1)) / 2;         /* B */
                }
                else
                {
                    *scanpt++ = *(rawpt + width); /* R */
                    *scanpt++ = *rawpt;           /* G */
                    *scanpt++ = *(rawpt - 1);     /* B */
                }
            }
        }
        else
        {
            if ((i % 2) == (red&1))
            {
                /* G(R) */
                if ((i < (width * (height - 1))) && ((i % width) > 0))
                {
                    *scanpt++ = (*(rawpt - 1) + *(rawpt + 1)) / 2;         /* R */
                    *scanpt++ = *rawpt;                                    /* G */
                    *scanpt++ = (*(rawpt + width) + *(rawpt - width)) / 2; /* B */
                }
                else
                {
                    /* bottom line or left column */
                    *scanpt++ = *(rawpt + 1);     /* R */
                    *scanpt++ = *rawpt;           /* G */
                    *scanpt++ = *(rawpt - width); /* B */
                }
            }
            else
            {
                /* R */
                if (i < (width * (height - 1)) && ((i % width) < (width - 1)))
                {
                    *scanpt++ = *rawpt;                                                                  /* R */
                    *scanpt++ = (*(rawpt - 1) + *(rawpt + 1) + *(rawpt - width) + *(rawpt + width)) / 4; /* G */
                    *scanpt++ =
                        (*(rawpt - width - 1) + *(rawpt - width + 1) + *(rawpt + width - 1) + *(rawpt + width + 1)) /
                        4; /* B */
                }
                else
                {
                    /* bottom line or right column */
                    *scanpt++ = *rawpt;                                /* R */
                    *scanpt++ = (*(rawpt - 1) + *(rawpt - width)) / 2; /* G */
                    *scanpt++ = *(rawpt - width - 1);                  /* B */
                }
            }
        }
        rawpt++;
    }
    return dst;
}

dsp_stream_p *dsp_stream_from_components(dsp_t* buf, int dims, int *sizes, int components)
{
    int d, y, x;
    dsp_stream_p* picture = (dsp_stream_p*) malloc(sizeof(dsp_stream_p)*(size_t)(components+1));
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
    return picture;
}

dsp_stream_p *dsp_buffer_rgb_to_components(void* buf, int dims, int *sizes, int components, int bpp, int stretch)
{
    dsp_stream_p* picture = (dsp_stream_p*) malloc(sizeof(dsp_stream_p)*(size_t)(components+1));
    int x, y, z, d;
    dsp_stream_p channel;
    for(y = 0; y < components; y++) {
        channel = dsp_stream_new();
        for(d = 0; d < dims; d++)
            dsp_stream_add_dim(channel, sizes[d]);
        dsp_stream_alloc_buffer(channel, channel->len);
        switch(bpp) {
        case 8:
            dsp_buffer_copy_stepping(((unsigned char*)&((unsigned char*)buf)[y]), channel->buf, channel->len*components, channel->len, components, 1);
            break;
        case 16:
            dsp_buffer_copy_stepping(((unsigned short*)&((unsigned short*)buf)[y]), channel->buf, channel->len*components, channel->len, components, 1);
            break;
        case 32:
            dsp_buffer_copy_stepping(((unsigned int*)&((unsigned int*)buf)[y]), channel->buf, channel->len*components, channel->len, components, 1);
            break;
        case 64:
            dsp_buffer_copy_stepping(((unsigned long*)&((unsigned long*)buf)[y]), channel->buf, channel->len*components, channel->len, components, 1);
            break;
        case -32:
            dsp_buffer_copy_stepping(((float*)&((float*)buf)[y]), channel->buf, channel->len*components, channel->len, components, 1);
            break;
        case -64:
            dsp_buffer_copy_stepping(((double*)&((double*)buf)[y]), channel->buf, channel->len*components, channel->len, components, 1);
            break;
        default:
            break;
        }
        if(stretch) {
            dsp_buffer_stretch(channel->buf, channel->len, 0, dsp_t_max);
        }
        picture[y] = channel;
    }
    channel = dsp_stream_new();
    for(d = 0; d < dims; d++)
        dsp_stream_add_dim(channel, sizes[d]);
    dsp_stream_alloc_buffer(channel, channel->len);
    for(x = 0; x < channel->len; x++) {
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
            for(z = 0; z < y; z++) v += ((float*)buf)[x*y+z];
            break;
        case -64:
            for(z = 0; z < y; z++) v += ((double*)buf)[x*y+z];
            break;
        default:
            break;
        }
        channel->buf[x] = (dsp_t)(v/y);
    }
    if(stretch) {
        dsp_buffer_stretch(channel->buf, channel->len, 0, dsp_t_max);
    }
    picture[y] = channel;
    free (buf);
    return picture;
}

void dsp_buffer_components_to_rgb(dsp_stream_p *stream, void* rgb, int components, int bpp)
{
    ssize_t y;
    int len = stream[0]->len * components;
    dsp_t max = (dsp_t)((double)((1<<abs(bpp))-1));
    max = Min(max, dsp_t_max);
    for(y = 0; y < components; y++) {
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
