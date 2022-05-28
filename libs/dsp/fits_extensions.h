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

#ifndef _FITS_EXTENSIONS_H
#define _FITS_EXTENSIONS_H

#ifdef  __cplusplus
extern "C" {
#endif

#ifndef DLL_EXPORT
#define DLL_EXPORT extern
#endif

/**
 * \defgroup dsp_FitsExtensions DSP API FITS Extensions functions
*/
/**\{*/
/// \defgroup dsp_FitsExtensionSDFITS DSP API SDFITS Extension
#include <sdfits.h>
/// \defgroup dsp_FitsExtensionFITSIDI DSP API FITSIDI Extension
#include <fitsidi.h>
/**\}*/

#ifdef __cplusplus
}
#endif

#endif //_FITS_EXTENSIONS_H
