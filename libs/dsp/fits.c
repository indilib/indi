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

void dsp_fits_update_fits_key(fitsfile *fptr, int type, char* name, void *p, char* explanation, int *status)
{
    fits_update_key(fptr, type, name, p, explanation, status);
}

long dsp_fits_alloc_fits_rows(fitsfile *fptr, unsigned long num_rows)
{
    int status = 0;
    long nrows = 0;
    fits_get_num_rows(fptr, &nrows, &status);
    fits_insert_rows(fptr, nrows, num_rows, &status);
    return nrows;
}


int dsp_fits_fill_fits_col(fitsfile *fptr, char* name, unsigned char *buf, int typecode, long num_elements, unsigned long rown)
{
    int status = 0;
    int ncol = 0;
    fits_get_colnum(fptr, CASESEN, (char*)(name), &ncol, &status);
    if(status != COL_NOT_FOUND)
    {
        fits_write_col(fptr, typecode, ncol, rown, 1, num_elements, buf, &status);
    }
    return status;
}

int dsp_fits_append_fits_col(fitsfile *fptr, char* name, char* format)
{
    int status = 0;
    int ncols = 0;
    fits_get_colnum(fptr, CASESEN, (char*)(name), &ncols, &status);
    if(status == COL_NOT_FOUND)
    {
        fits_get_num_cols(fptr, &ncols, &status);
        fits_insert_col(fptr, ncols++, name, format, &status);
    }
    return ncols;
}

void dsp_fits_delete_fits_col(fitsfile *fptr, char* name)
{
    int status = 0;
    int ncol = 0;
    fits_get_colnum(fptr, CASESEN, (char*)(name), &ncol, &status);
    while(status != COL_NOT_FOUND)
        fits_delete_col(fptr, ncol, &status);
}

fitsfile* dsp_fits_create_fits(size_t *size, void **buf)
{
    fitsfile *fptr = NULL;

    size_t memsize;
    int status    = 0;
    char error_status[64];

    //  Now we have to send fits format data to the client
    memsize = 5760;
    void* memptr  = malloc(memsize);
    if (!memptr)
    {
        perr("Error: failed to allocate memory: %lu", (unsigned long)(memsize));
    }

    fits_create_memfile(&fptr, &memptr, &memsize, 2880, realloc, &status);

    if (status)
    {
        fits_get_errstatus(status, error_status);
        perr("FITS Error: %s", error_status);
        if(memptr != NULL)
            free(memptr);
        return NULL;
    }
    if (status)
    {
        fits_get_errstatus(status, error_status);
        perr("FITS Error: %s", error_status);
        if(memptr != NULL)
            free(memptr);
        return NULL;
    }

    if (status)
    {
        fits_get_errstatus(status, error_status);
        perr("FITS Error: %s", error_status);
        if(memptr != NULL)
            free(memptr);
        return NULL;
    }

    *size = memsize;
    *buf = memptr;

    return fptr;
}

int dsp_fits_close_fits(fitsfile *fptr)
{
    int status = 0;
    fits_close_file(fptr, &status);

    return status;
}

int dsp_fits_get_value(fitsfile *fptr, char* column, long rown, void **retval)
{
    int err = 1, n = 0, anynul = 0, status = 0, typecode;
    long repeat = 1;
    long width;
    char name[64];
    if(column == NULL)
        goto err_return;
    fits_get_colname(fptr, 0, column, name, &n, &status);
    if(status)
        goto err_return;
    fits_get_coltype(fptr, n, &typecode, &repeat, &width, &status);
    void *value = malloc(dsp_fits_get_element_size(typecode)*(size_t)(repeat*width));
    fits_read_col(fptr, typecode, n, rown, 1, repeat, NULL, value, &anynul, &status);
    *retval = value;
err_return:
    return err;
}

int dsp_fits_check_column(fitsfile *fptr, char* column, char **expected, long rown)
{
    int err = 1, n = 0, anynul = 0, status = 0, x, y, typecode;
    long repeat = 1;
    long width;
    char name[64];
    if(column == NULL || expected == NULL)
        goto err_return;
    fits_get_colname(fptr, 0, column, name, &n, &status);
    if(status)
        goto err_return;
    fits_get_coltype(fptr, n, &typecode, &repeat, &width, &status);
    if(typecode != TSTRING)
        goto err_return;
    char **value = (char **)malloc(sizeof(char*)*(size_t)repeat);
    for(x = 0; x < repeat; x++) {
        value[x] = (char*) malloc((size_t)width);
        fits_read_col_str(fptr, n, rown, 1, 1, NULL, value, &anynul, &status);
        for(y = 0; strcmp(expected[y], ""); y++) {
            if(!strcmp(value[x], expected[y])) {
                err &= 1;
                break;
            }
        }
        if(y == 0)
            err = 0;
    }
    for(x = 0; x < repeat; x++)
        free(value[x]);
    free(value);
err_return:
    return err;
}

int dsp_fits_check_key(fitsfile *fptr, char* keyname, char **expected)
{
    int err = 1, status = 0, y;
    char value[64];
    if(keyname == NULL || expected == NULL)
        goto err_return;
    fits_read_key_str(fptr, keyname, value, NULL, &status);
    if(status)
        goto err_return;
    for(y = 0; strcmp(expected[y], ""); y++) {
        if(!strcmp(value, expected[y])) {
            err &= 1;
            break;
        }
    }
    if(y == 0)
        err = 0;
err_return:
    return err;
}

int dsp_fits_read_typecode(char* typestr, int *typecode, long *width, long *repeat)
{
    int w, r, t;
    char c;
    sscanf(typestr, "%d%c%d", &w, &c, &r);
    t = (int)c;
    if (t == EXTFITS_ELEMENT_BIT.typestr[0])
        t = EXTFITS_ELEMENT_BIT.typecode;
    if (t == EXTFITS_ELEMENT_STRING.typestr[0])
        t = EXTFITS_ELEMENT_STRING.typecode;
    if (t == EXTFITS_ELEMENT_LOGICAL.typestr[0])
        t = EXTFITS_ELEMENT_LOGICAL.typecode;
    if (t == EXTFITS_ELEMENT_BYTE.typestr[0])
        t = EXTFITS_ELEMENT_BYTE.typecode;
    if (t == EXTFITS_ELEMENT_SBYTE.typestr[0])
        t = EXTFITS_ELEMENT_SBYTE.typecode;
    if (t == EXTFITS_ELEMENT_SHORT.typestr[0])
        t = EXTFITS_ELEMENT_SHORT.typecode;
    if (t == EXTFITS_ELEMENT_USHORT.typestr[0])
        t = EXTFITS_ELEMENT_USHORT.typecode;
    if (t == EXTFITS_ELEMENT_INT.typestr[0])
        t = EXTFITS_ELEMENT_INT.typecode;
    if (t == EXTFITS_ELEMENT_UINT.typestr[0])
        t = EXTFITS_ELEMENT_UINT.typecode;
    if (t == EXTFITS_ELEMENT_LONG.typestr[0])
        t = EXTFITS_ELEMENT_LONG.typecode;
    if (t == EXTFITS_ELEMENT_FLOAT.typestr[0])
        t = EXTFITS_ELEMENT_FLOAT.typecode;
    if (t == EXTFITS_ELEMENT_DOUBLE.typestr[0])
        t = EXTFITS_ELEMENT_DOUBLE.typecode;
    if (t == EXTFITS_ELEMENT_COMPLEX.typestr[0])
        t = EXTFITS_ELEMENT_COMPLEX.typecode;
    if (t == EXTFITS_ELEMENT_DBLCOMPLEX.typestr[0])
        t = EXTFITS_ELEMENT_DBLCOMPLEX.typecode;
    else return -1;
    *typecode = t;
    *width = w;
    *repeat = r;
    return 0;
}

size_t dsp_fits_get_element_size(int typecode)
{
    size_t typesize = 1;

    switch(typecode)
    {
        case TSHORT:
        case TUSHORT:
            typesize *= 2;
            break;
        case TINT:
        case TUINT:
        case TFLOAT:
            typesize *= 4;
            break;
        case TLONG:
        case TULONG:
        case TDOUBLE:
        case TCOMPLEX:
            typesize *= 8;
            break;
        case TDBLCOMPLEX:
            typesize *= 16;
            break;
        default:
            break;
    }

    return typesize;
}

int dsp_fits_append_table(fitsfile* fptr, dsp_fits_column *columns, int ncols, char* tablename)
{
    int status = 0;
    int x = 0;
    fits_update_key(fptr, TSTRING, "EXTNAME", tablename, "", &status);
    for(x = 0; x < ncols; x++) {
        dsp_fits_append_fits_col(fptr, columns[x].name, columns[x].format);
    }
    return status;
}

dsp_fits_row* dsp_fits_read_sdfits(char *filename, long *num_rows, long *maxes, long **maxis)
{
    fitsfile *fptr = (fitsfile*)malloc(sizeof(fitsfile));
    memset(fptr, 0, sizeof(fitsfile));
    int status = 0;
    int sdfits_hdu = 0;
    long nrows = 0;
    long r = 0;
    long nmatrix = 0;
    int ncols = 0;
    int typecode = 0;
    long width = 0;
    long repeat = 0;
    int k = 0;
    int i = 0;
    int n = 0;
    int dim;
    int anynul = 0;
    long naxes[3] = { 1, 1, 1 };
    dsp_fits_column* columns = (dsp_fits_column*)malloc(sizeof(dsp_fits_column));
    dsp_fits_row* rows = (dsp_fits_row*)malloc(sizeof(dsp_fits_row));
    char value[150];
    char comment[150];
    char error_status[64];

    fits_open_file(&fptr, filename, READONLY, &status);
    if (status)
    {
        goto fail;
    }

    ffgkey(fptr, FITS_KEYWORD_EXTEND.name, value, comment, &status);
    if(status || strcmp(value, FITS_KEYWORD_EXTEND.value))
    {
        goto fail;
    }

    ffgkey(fptr, SDFITS_KEYWORD_TELESCOP.name, value, comment, &status);
    if (!status)
    {
    }
    status = 0;

    ffgkey(fptr, SDFITS_KEYWORD_OBSERVER.name, value, comment, &status);
    if (!status)
    {
    }
    status = 0;

    ffgkey(fptr, SDFITS_KEYWORD_DATE_OBS.name, value, comment, &status);
    if (!status)
    {
    }
    status = 0;

    ffgkey(fptr, SDFITS_KEYWORD_DATAMAX.name, value, comment, &status);
    if (!status)
    {
    }
    status = 0;

    ffgkey(fptr, SDFITS_KEYWORD_DATAMIN.name, value, comment, &status);
    if (!status)
    {
    }
    status = 0;

    fits_movabs_hdu(fptr, 1, &sdfits_hdu, &status);
    if(status || sdfits_hdu != BINARY_TBL)
    {
        goto fail;
    }

    fits_read_key_str(fptr, "EXTNAME", value, comment, &status);
    if(status || strcmp(value, FITS_TABLE_SDFITS))
    {
        goto fail;
    }

    fits_read_key_str(fptr, EXTFITS_KEYWORD_NMATRIX.name, value, NULL, &status);
    if(status || strcmp(value, EXTFITS_KEYWORD_NMATRIX.value)) {
        goto fail;
    }

    fits_get_num_rows(fptr, &nrows, &status);
    if(status)
    {
        goto fail;
    }

    fits_get_num_cols(fptr, &ncols, &status);
    if(status)
    {
        goto fail;
    }

    fits_read_key_lng(fptr, EXTFITS_KEYWORD_NMATRIX.name, &nmatrix, NULL, &status);
    if(status || nmatrix < 1)
    {
        goto fail;
    }

    columns = (dsp_fits_column*)realloc(columns, sizeof(dsp_fits_column)*((size_t)ncols+1));
    rows = (dsp_fits_row*)realloc(rows, sizeof(dsp_fits_row)*((size_t)nrows+1));

    for(r = 0; r < nrows; r++) {
        for(k = 0; k < ncols; k++) {
            columns[k].name = (char*)malloc(150);
            columns[k].format = (char*)malloc(150);
            columns[k].unit = (char*)malloc(150);
            columns[k].value = (char*)malloc(150);
            columns[k].comment = (char*)malloc(150);

            status = 0;
            fits_get_colname(fptr, 0, SDFITS_TABLE_MAIN[i].name, value, &n, &status);
            strcpy(columns[k].name, value);
            if(!dsp_fits_check_key(fptr, EXTFITS_KEYWORD_TMATX(k).name, &EXTFITS_KEYWORD_TMATX(k).value)) {
                int max_dims = 5;
                int dims;
                long *sizes =(long*)malloc(sizeof(long)*(size_t)max_dims);
                fits_read_tdim(fptr, k, max_dims, &dims, sizes, &status);
                if(dims < 2) {
                    long d = 0;
                    fits_read_key_lng(fptr, EXTFITS_KEYWORD_MAXIS().name, &d, NULL, &status);
                    sizes = (long*)malloc(sizeof(long)*(size_t)dims);
                    for(dim = 0; dim < d; dim++)
                        fits_read_key_lng(fptr, EXTFITS_KEYWORD_MAXIS(dim).name, &sizes[dim], NULL, &status);
                    dims = (int)d;
                }
                if(dims > 0) {
                    void *tcs = NULL;
                    dsp_fits_get_value(fptr, EXTFITS_KEYWORD_TMATX(k).axes_definition.format.name, r, &tcs);
                    strcpy(columns[k].format, tcs);
                    dsp_fits_get_value(fptr, EXTFITS_KEYWORD_TMATX(k).axes_definition.unit.name, r, &tcs);
                    strcpy(columns[k].unit, tcs);
                    if (!dsp_fits_read_typecode((char*)tcs, &typecode, &width, &repeat)) {
                        size_t element_size = dsp_fits_get_element_size(typecode);
                        long nelements = 1;
                        for(dim = 0; dim < dims; dim++) {
                            nelements *= naxes[dim];
                        }
                        columns[k].value = (char*)malloc(element_size*(size_t)nelements);
                        fits_read_col(fptr, typecode, k, r, 1, nelements, NULL, columns->value, &anynul, &status);
                        if(!anynul && !status) {
                            *maxis = (long*)malloc(sizeof(long)*(size_t)dims);
                            for(dim = 0; dim < dims; dim++)
                                *maxis[dim] = naxes[dim];
                            *maxes = dims;
                        }
                    }
                }
            } else {
                int typecode;
                long repeat, width;
                fits_get_eqcoltype(fptr, n, &typecode, &repeat, &width, &status);
                if(status) continue;
                if(dsp_fits_check_column(fptr, columns[k].name, columns[k].expected, r))
                    continue;
                void *val = &columns[k].value;
                dsp_fits_get_value(fptr, columns[k].name, r, &val);
            }
        }
        rows[r].columns = (dsp_fits_column*)malloc(sizeof(dsp_fits_column)*rows[r].num_columns);
        rows[r].num_columns = (size_t)ncols;
    }
    *num_rows = nrows;
    status = 0;
    fits_close_file(fptr, &status);
    if(status)
        goto fail;
    return rows;
fail:
    free(rows);
    free(columns);
    if(status)
    {
        fits_get_errstatus(status, error_status);
        perr("FITS Error: %s\n", error_status);
    }
    return NULL;
}
