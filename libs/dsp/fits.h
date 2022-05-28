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

#ifndef _DSP_FITS_H
#define _DSP_FITS_H

#ifdef  __cplusplus
extern "C" {
#endif

#ifndef DLL_EXPORT
#define DLL_EXPORT extern
#endif

#include <fitsio.h>

/**
 * \defgroup dsp_FitsExtensions DSP API FITS Extensions functions
*/
/**\{*/
///FITS format
typedef struct
{
    ///String format
    char typestr[8];
    ///FITSIO typecode
    int typecode;
    ///Number of repetitions
    long repeat;
    ///Width of each element
    long width;
} dsp_fits_format;

///FITS keyword
typedef struct
{
    ///Name of the keyword
    char *name;
    ///Format of the content of the keyword
    char *format;
    ///Measure unit of the value
    char *unit;
    ///Value
    char *value;
    ///Description of the keyword or value
    char *comment;
    ///Expected value if checking when read
    char ** expected;
} dsp_fits_keyword;

///Binary table FITS extension column
typedef struct
{
    ///Name of the column (title, TTYPE)
    char *name;
    ///Format string of the content of the column (TFORM)
    char *format;
    ///Measure unit of the column elements (TUNIT)
    char *unit;
    ///Default initial value
    char *value;
    ///Description of the column or data
    char *comment;
    ///Expected values if checking when read
    char ** expected;
} dsp_fits_column;

///Binary table FITS extension row
typedef struct
{
    ///Columns array
    dsp_fits_column* columns;
    ///Columns data
    size_t num_columns;
} dsp_fits_row;

///Binary table FITS Matrix axis
typedef struct
{
    ///Axis name
    char *name;
    ///Axis format string
    char *format;
    ///Axis measure unit string
    char *unit;
    ///Axis default value
    char *value;
    ///Axis description
    char *comment;
    ///Data definition
    struct
    {
        ///Data name
        dsp_fits_keyword name;
        ///Data increment step
        dsp_fits_keyword increment;
        ///Data reference pixel
        dsp_fits_keyword refpix;
        ///Data reference pixel value
        dsp_fits_keyword value;
    } definition;
} dsp_fits_axis;

///Binary table FITS Matrix
typedef struct
{
    ///Matrix name
    char *name;
    ///Matrix format string
    char *format;
    ///Matrix measure unit string
    char *value;
    ///Matrix description
    char *comment;
    ///Axes definition
    struct
    {
        ///Axes name
        dsp_fits_keyword name;
        ///Axes format
        dsp_fits_keyword format;
        ///Axes measure unit
        dsp_fits_keyword unit;
        ///Axes quantity
        dsp_fits_keyword dims;
    } axes_definition;
} dsp_fits_matrix;

///Returns non-zero decimal conversion of integer into string
#ifndef itostr
#define its(x) #x
#define itostr(x) its(x)
#endif

///FITS element types
#define EXTFITS_ELEMENT_STRING (dsp_fits_format){"A", TSTRING, 0, 0}
#define EXTFITS_ELEMENT_LOGICAL (dsp_fits_format){"L", TLOGICAL, 0, 0}
#define EXTFITS_ELEMENT_BIT (dsp_fits_format){"X", TBIT, 0, 0}
#define EXTFITS_ELEMENT_BYTE (dsp_fits_format){"B", TBYTE, 0, 0}
#define EXTFITS_ELEMENT_SBYTE (dsp_fits_format){"S", TSBYTE, 0, 0}
#define EXTFITS_ELEMENT_SHORT (dsp_fits_format){"I", TSHORT, 0, 0}
#define EXTFITS_ELEMENT_USHORT (dsp_fits_format){"U", TUSHORT, 0, 0}
#define EXTFITS_ELEMENT_INT (dsp_fits_format){"J", TINT, 0, 0}
#define EXTFITS_ELEMENT_UINT (dsp_fits_format){"V", TUINT, 0, 0}
#define EXTFITS_ELEMENT_LONG (dsp_fits_format){"K", TLONG, 0, 0}
#define EXTFITS_ELEMENT_FLOAT (dsp_fits_format){"E", TFLOAT, 0, 0}
#define EXTFITS_ELEMENT_DOUBLE (dsp_fits_format){"D", TDOUBLE, 0, 0}
#define EXTFITS_ELEMENT_COMPLEX (dsp_fits_format){"C", TCOMPLEX, 0, 0}
#define EXTFITS_ELEMENT_DBLCOMPLEX (dsp_fits_format){"M", TDBLCOMPLEX, 0, 0}

///FITS Measure units
#define EXTFITS_MEASURE_UNIT_HZ "Hz"
#define EXTFITS_MEASURE_UNIT_SECOND "sec"
#define EXTFITS_MEASURE_UNIT_MINUTE "min"
#define EXTFITS_MEASURE_UNIT_HOUR "hour"
#define EXTFITS_MEASURE_UNIT_DAY "day"
#define EXTFITS_MEASURE_UNIT_MONTH "month"
#define EXTFITS_MEASURE_UNIT_YEAR "year"
#define EXTFITS_MEASURE_UNIT_JANSKY "Jy"
#define EXTFITS_MEASURE_UNIT_KELVIN "K"
#define EXTFITS_MEASURE_UNIT_ANGSTROM "Angstrom"
#define EXTFITS_MEASURE_UNIT_ARCSEC "arcsec"
#define EXTFITS_MEASURE_UNIT_ARCMIN "arcmin"
#define EXTFITS_MEASURE_UNIT_DEGREE "degree"
#define EXTFITS_MEASURE_UNIT_PERCENT "percent"
#define EXTFITS_MEASURE_UNIT_METER "meter"
#define EXTFITS_MEASURE_UNIT_MILLIBAR "millibar"

///Set to 'FLUX' or 'DATA' for matrix buffers
#define EXTFITS_KEYWORD_TTYPE(n) (dsp_fits_keyword){"TTYPE" itostr(n), "8A", "", "", "Set to 'FLUX'", (char*[]){"FLUX", "DATA", ""}}
///shall have the value 'K', 'JY' or 'UNCALIB'
#define EXTFITS_KEYWORD_TUNIT(n) (dsp_fits_keyword){"TUNIT" itostr(n), "8A", "", "", "Shall have the value 'JY' or 'UNCALIB'", (char*[]){""}}
///Size in pixels of data buffer
#define EXTFITS_KEYWORD_TDIM(n) (dsp_fits_keyword){"TDIM" itostr(n), "8A", "", "", "Size in pixels of data buffer", (char*[]){""}}
///shall have the format of the column
#define EXTFITS_KEYWORD_TFORM(n) (dsp_fits_keyword){"TFORM" itostr(n), "8A", "", "", "Shall be a character string", (char*[]){""}}

///Name of regular axis m = 1 to M
#define EXTFITS_KEYWORD_CTYPE(m) (dsp_fits_keyword){"CTYPE" itostr(m), EXTFITS_ELEMENT_STRING.typestr, "", "", "Name of regular axis m = 1 to M", (char*[]){""}}
///Coordinate increment on axis m = 1 to M
#define EXTFITS_KEYWORD_CDELT(m) (dsp_fits_keyword){"CDELT" itostr(m), EXTFITS_ELEMENT_FLOAT.typestr, "", "", "Coordinate increment on axis m = 1 to M", (char*[]){""}}
///Reference pixel on axis m = 1 to M
#define EXTFITS_KEYWORD_CRPIX(m) (dsp_fits_keyword){"CRPIX" itostr(m), EXTFITS_ELEMENT_FLOAT.typestr, "", "", "Reference pixel on axis m = 1 to M", (char*[]){""}}
///Coordinate value at reference pixel on axis m = 1 to M
#define EXTFITS_KEYWORD_CRVAL(m) (dsp_fits_keyword){"CRVAL" itostr(m), EXTFITS_ELEMENT_FLOAT.typestr, "", "", "Coordinate value at reference pixel on axis m = 1 to M", (char*[]){""}}

///NMATRIX shall be present with the value 1
#define EXTFITS_KEYWORD_NMATRIX (dsp_fits_keyword){"NMATRIX", EXTFITS_ELEMENT_SHORT.typestr, "", "1", "NMATRIX shall be present with the value 1", (char*[]){"1", ""}}
///Set to 'T' — column n contains the visibility matrix
#define EXTFITS_KEYWORD_TMATX(n) (dsp_fits_matrix){"TMATX" itostr(n), "8A", "T", "Set to 'T'", {EXTFITS_KEYWORD_TTYPE(n), EXTFITS_KEYWORD_TFORM(n), EXTFITS_KEYWORD_TUNIT(n), EXTFITS_KEYWORD_TDIM(n)}}
///M = number axes in regular matrix, Number pixels on axis m = 1 to M
#define EXTFITS_KEYWORD_MAXIS(m) (dsp_fits_axis){"MAXIS" itostr(m), EXTFITS_ELEMENT_SHORT.typestr, "", "", "M = number axes in regular matrix, Number pixels on axis m = 1 to M", {EXTFITS_KEYWORD_CTYPE(m), EXTFITS_KEYWORD_CDELT(m), EXTFITS_KEYWORD_CRPIX(m), EXTFITS_KEYWORD_CRVAL(m)}}

///Target right ascension coordinate
#define EXTFITS_KEYWORD_OBJCTRA (dsp_fits_column){"OBJCTRA", EXTFITS_ELEMENT_STRING.typestr, EXTFITS_MEASURE_UNIT_DEGREE, "", "Target right ascension coordinate", (char*[]){""}}
///Target declination coordinate
#define EXTFITS_KEYWORD_OBJCTDEC (dsp_fits_column){"OBJCTDEC", EXTFITS_ELEMENT_STRING.typestr, EXTFITS_MEASURE_UNIT_DEGREE, "", "Target declination coordinate", (char*[]){""}}

#define FITS_KEYWORD_EXTEND (dsp_fits_keyword){"EXTEND", "A", "", "T", "", (char*[]){""}}
#define FITS_KEYWORD_EXTNAME (dsp_fits_keyword){"EXTNAME", "", "", "", "", (char*[]){""}}

/**
* \brief Create or update a new fits header key
* \param fptr The fits file pointer created by dsp_fits_create_fits
* \param type The typecode of the value
* \param name The keyword to be updated
* \param value The new value of the specified keyword
* \param comment The keyword or assigned value description or explanation
* \param status This variable will be updated with the status of the update operation
*/
void dsp_fits_update_fits_key(fitsfile *fptr, int type, char* name, void *value, char* comment, int *status);

/**
* \brief Convert an RGB color dsp_t array into a dsp_stream_p array each element containing the single components
* \param fptr The fits file pointer created by dsp_fits_create_fits
* \param num_rows The number of rows to be allocated
* \return The number of rows incremented by the allocated ones
* \sa dsp_fits_create_fits
*/
long dsp_fits_alloc_fits_rows(fitsfile *fptr, unsigned long num_rows);

/**
* \brief Fill a column at the given row position with the valued buffer
* \param fptr The fits file pointer created by dsp_fits_create_fits
* \param name The name of the column
* \param buf The buffer that will be copied into the selected field.
* \param typecode The element type size
* \param num_elements The total field size in elements, this should take into account the width and repeat multipliers
* \param rown The row number where the field is located
* \return non-zero if any error occured
* \sa dsp_fits_create_fits
*/
int dsp_fits_fill_fits_col(fitsfile *fptr, char* name, unsigned char *buf, int typecode, long num_elements,
                           unsigned long rown);

/**
* \brief Add a column to the binary table
* \param fptr The fits file pointer created by dsp_fits_create_fits
* \param name The name of the column
* \param format This field should indicate the element size, width of each element and repetition eventually
* \return non-zero if any error occured
*/
int dsp_fits_append_fits_col(fitsfile *fptr, char* name, char* format);

/**
* \brief Delete a column from the binary table
* \param fptr The fits file pointer created by dsp_fits_create_fits
* \param name The name of the column
*/
void dsp_fits_delete_fits_col(fitsfile *fptr, char* name);

/**
* \brief Obtain the single element size in bytes
* \param typecode The typecode of each single element
* \return the size of the single element
*/
size_t dsp_fits_get_element_size(int typecode);

/**
* \brief Decode a typecode format string
* \param typestr The element format string
* \param typecode This function will return the typecode to this variable
* \param width This function will return the width to this variable
* \param repeat This function will return the repeatition count to this variable
* \return non-zero if any error occured
*/
int dsp_fits_read_typecode(char* typestr, int *typecode, long *width, long *repeat);

/**
* \brief Obtain the value of the specified field
* \param fptr The fits file pointer created by dsp_fits_create_fits
* \param column The column name of the selected field
* \param rown The row position of the field
* \param retval A preallocated buffer where the field value will be stored into
* \return non-zero if any error occured
*/
int dsp_fits_get_value(fitsfile *fptr, char* column, long rown, void **retval);

/**
* \brief Check if the value of the specified field corresponds to a subset of values
* \param fptr The fits file pointer created by dsp_fits_create_fits
* \param column The column name of the selected field
* \param expected A buffer array containing expected values, terminating with an empty value
* \param rown The row position of the field
* \return zero if any of the values was matched
*/
int dsp_fits_check_column(fitsfile *fptr, char* column, char **expected, long rown);

/**
* \brief Create an open fits file pointer to be updated later
* \param size This variable will contain the initial size of the fits file pointer
* \param buf This buffer will contain the fits memfile
* \return fitsfile the fits file pointer
*/
fitsfile* dsp_fits_create_fits(size_t *size, void **buf);

/**
* \brief Add a binary table extension into a fits file
* \param fptr Pointer to a fits file
* \param columns An array of dsp_fits_column structs
* \param ncols The dsp_fits_column array length
* \param tablename The extension table name
* \return non-zero if any error occured
*/
int dsp_fits_add_table(fitsfile* fptr, dsp_fits_column *columns, int ncols,  const char* tablename);

/**
* \brief Close a fits file pointer
* \param fptr The fits file pointer created by dsp_fits_create_fits
* \return non-zero if any error occured
*/
int dsp_fits_close_fits(fitsfile *fptr);
/**\}*/

#ifdef __cplusplus
}
#endif

#endif //_DSP_FITS_H
