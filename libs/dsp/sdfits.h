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

#ifndef _FITS_EXTENSION_SDFITS_H
#define _FITS_EXTENSION_SDFITS_H

#ifdef  __cplusplus
extern "C" {
#endif
#include <fits.h>

/// \defgroup dsp_FitsExtensionSDFITS DSP API SDFITS Extension
/**\{*/
///SDFITS Convention Table
#define FITS_TABLE_SDFITS "SINGLE DISH"

///SDFITS columns
///common FITS usage
#define SDFITS_COLUMN_OBJECT (dsp_fits_column){"OBJECT", "8A", "", "", "common FITS usage", (char*[]){""}}
///common FITS keyword
#define SDFITS_COLUMN_TELESCOP (dsp_fits_column){"TELESCOP", "8A", "", "", "common FITS keyword", (char*[]){""}}
///resolution may differ from spacing of backend, not one channel
#define SDFITS_COLUMN_FREQRES (dsp_fits_column){"FREQRES", EXTFITS_ELEMENT_DOUBLE.typestr, EXTFITS_MEASURE_UNIT_HZ, "", "resolution may differ from spacing of backend, not one channel", (char*[]){""}}
#define SDFITS_COLUMN_BANDWID (dsp_fits_column){"BANDWID", EXTFITS_ELEMENT_DOUBLE.typestr, EXTFITS_MEASURE_UNIT_HZ, "", "", (char*[]){""}}
///common FITS usage; JD preferable?
#define SDFITS_COLUMN_DATE_OBS (dsp_fits_column){"DATE-OBS", "8A", "", "", "common FITS usage; JD preferable?", (char*[]){""}}
///UT time of day; UT seconds since Oh UT
#define SDFITS_COLUMN_TIME (dsp_fits_column){"TIME", EXTFITS_ELEMENT_DOUBLE.typestr, EXTFITS_MEASURE_UNIT_SECOND, "", "UT time of day; UT seconds since Oh UT", (char*[]){""}}
///effective integration time
#define SDFITS_COLUMN_EXPOSURE (dsp_fits_column){"EXPOSURE", EXTFITS_ELEMENT_DOUBLE.typestr, EXTFITS_MEASURE_UNIT_SECOND, "", "effective integration time", (char*[]){""}}
///system, not receiver, temperature
#define SDFITS_COLUMN_TSYS (dsp_fits_column){"TSYS", EXTFITS_ELEMENT_DOUBLE.typestr, EXTFITS_MEASURE_UNIT_KELVIN, "", "system, not receiver, temperature", (char*[]){""}}
///Target right ascension coordinate
#define SDFITS_COLUMN_OBJCTRA (dsp_fits_column){"OBJCTRA", EXTFITS_ELEMENT_STRING.typestr, EXTFITS_MEASURE_UNIT_DEGREE, "", "Target right ascension coordinate", (char*[]){""}}
///Target declination coordinate
#define SDFITS_COLUMN_OBJCTDEC (dsp_fits_column){"OBJCTDEC", EXTFITS_ELEMENT_STRING.typestr, EXTFITS_MEASURE_UNIT_DEGREE, "", "Target declination coordinate", (char*[]){""}}
///Data buffer
#define SDFITS_COLUMN_DATA (dsp_fits_column){"DATA", "", "", "", "", (char*[]){""}}

///OBSMODE
///average
#define OBSTYPE_LINE "LINE"
///continuum
#define OBSTYPE_CONT "CONT"
///pulses
#define OBSTYPE_PULS "PULS"

///position switch
#define OBSMODE_PSSW "PSSW"
///frequency switch
#define OBSMODE_FQSW "FQSW"
///beam switch
#define OBSMODE_BMSW "BMSW"
///phase-lock switch
#define OBSMODE_PLSW "PLSW"
///load switch
#define OBSMODE_LDSW "LDSW"
///total power
#define OBSMODE_TLPW "TLPW"

///average
#define OBSMODE_LINE_PSSW OBSTYPE_LINE OBSMODE_PSSW
///frequency switch
#define OBSMODE_LINE_FQSW OBSTYPE_LINE OBSMODE_FQSW
///beam switch
#define OBSMODE_LINE_BMSW OBSTYPE_LINE OBSMODE_BMSW
///phase-lock switch
#define OBSMODE_LINE_PLSW OBSTYPE_LINE OBSMODE_PLSW
///load switch
#define OBSMODE_LINE_LDSW OBSTYPE_LINE OBSMODE_LDSW
///total power
#define OBSMODE_LINE_TLPW OBSTYPE_LINE OBSMODE_TLPW

///continuum
///position switch
#define OBSMODE_CONT_PSSW OBSTYPE_CONT OBSMODE_PSSW
///frequency switch
#define OBSMODE_CONT_FQSW OBSTYPE_CONT OBSMODE_FQSW
///beam switch
#define OBSMODE_CONT_BMSW OBSTYPE_CONT OBSMODE_BMSW
///phase-lock switch
#define OBSMODE_CONT_PLSW OBSTYPE_CONT OBSMODE_PLSW
///load switch
#define OBSMODE_CONT_LDSW OBSTYPE_CONT OBSMODE_LDSW
///total power
#define OBSMODE_CONT_TLPW OBSTYPE_CONT OBSMODE_TLPW

///pulses
///position switch
#define OBSMODE_PULS_PSSW OBSTYPE_PULS OBSMODE_PSSW
///frequency switch
#define OBSMODE_PULS_FQSW OBSTYPE_PULS OBSMODE_FQSW
///beam switch
#define OBSMODE_PULS_BMSW OBSTYPE_PULS OBSMODE_BMSW
///phase-lock switch
#define OBSMODE_PULS_PLSW OBSTYPE_PULS OBSMODE_PLSW
///load switch
#define OBSMODE_PULS_LDSW OBSTYPE_PULS OBSMODE_LDSW
///total power
#define OBSMODE_PULS_TLPW OBSTYPE_PULS OBSMODE_TLPW

///TEMPSCAL
#define TEMPSCAL_TB "TB"
#define TEMPSCAL_TA "TA"
#define TEMPSCAL_TA_TR "TA*TR"
#define TEMPSCAL_TR "TR*"

///VELDEF
#define VELDEF_RADI "*RADI"
#define VELDEF_OPTI "OPTI"
#define VELDEF_RELA "RELA"
#define VELDEF_LSR "LSR"
#define VELDEF_HELO "HELO"
#define VELDEF_EART "EART"
#define VELDEF_BARI "BARI"
#define VELDEF_OBS "-OBS"

///SDFITS columns
///Observer name
#define SDFITS_COLUMN_OBSERVER (dsp_fits_column){"OBSERVER", "8A", "", "", "Observer name", (char*[]){""}}
///Observer & operator initials;
#define SDFITS_COLUMN_OBSID (dsp_fits_column){"OBSID", "8A", "", "", "Observer & operator initials", (char*[]){""}}
///Project ID;
#define SDFITS_COLUMN_PROJID (dsp_fits_column){"PROJID", "8A", "", "", "Project ID", (char*[]){""}}
///Scan number
#define SDFITS_COLUMN_SCAN (dsp_fits_column){"SCAN", EXTFITS_ELEMENT_DOUBLE.typestr, "", "", "Scan number", (char*[]){""}}
///Type of data, observing mode;
#define SDFITS_COLUMN_OBSMODE (dsp_fits_column){"OBSMODE", "8A", "", "", "Type of data, observing mode", (char*[]){OBSMODE_LINE_PSSW, OBSMODE_LINE_FQSW, OBSMODE_LINE_BMSW, OBSMODE_LINE_PLSW, OBSMODE_LINE_LDSW, OBSMODE_LINE_TLPW, OBSMODE_CONT_PSSW, OBSMODE_CONT_FQSW, OBSMODE_CONT_BMSW, OBSMODE_CONT_PLSW, OBSMODE_CONT_LDSW, OBSMODE_CONT_TLPW, OBSMODE_PULS_PSSW, OBSMODE_PULS_FQSW, OBSMODE_PULS_BMSW, OBSMODE_PULS_PLSW, OBSMODE_PULS_LDSW, OBSMODE_PULS_TLPW}}
///Molecule observed or detected;
#define SDFITS_COLUMN_MOLECULE (dsp_fits_column){"MOLECULE", "8A", "", "", "Molecule observed or detected", (char*[]){""}}
///As appropriate;
#define SDFITS_COLUMN_TRANSITI (dsp_fits_column){"TRANSITI", "8A", "", "", "As appropriate", (char*[]){""}}
///Normalization of TA;
#define SDFITS_COLUMN_TEMPSCAL (dsp_fits_column){"TEMPSCAL", "8A", "", "", "Normalization of TA", (char*[]){""}}
///
#define SDFITS_COLUMN_FRONTEND (dsp_fits_column){"FRONTEND", "8A", "", "", "", (char*[]){""}}
///Calibration Temp (K)
#define SDFITS_COLUMN_TCAL (dsp_fits_column){"TCAL", EXTFITS_ELEMENT_DOUBLE.typestr, EXTFITS_MEASURE_UNIT_KELVIN, "", "Calibration Temp", (char*[]){""}}
///Hot load temperature (K)
#define SDFITS_COLUMN_THOT (dsp_fits_column){"THOT", EXTFITS_ELEMENT_DOUBLE.typestr, EXTFITS_MEASURE_UNIT_KELVIN, "", "Hot load temperature", (char*[]){""}}
///Cold load temperature (K)
#define SDFITS_COLUMN_TCOLD (dsp_fits_column){"TCOLD", EXTFITS_ELEMENT_DOUBLE.typestr, EXTFITS_MEASURE_UNIT_KELVIN, "", "Cold load temperature", (char*[]){""}}
///Receiver Temp (K), Float
#define SDFITS_COLUMN_TRX (dsp_fits_column){"TRX", EXTFITS_ELEMENT_DOUBLE.typestr, EXTFITS_MEASURE_UNIT_KELVIN, "", "Receiver Temp, Float", (char*[]){""}}
///Velocity definition & frame;
#define SDFITS_COLUMN_VELDEF (dsp_fits_column){"VELDEF", "8A", "", "", "Velocity definition & frame", (char*[]){VELDEF_RADI, VELDEF_OPTI, VELDEF_RELA, VELDEF_LSR, VELDEF_HELO, VELDEF_EART, VELDEF_BARI, VELDEF_OBS, ""}}
///radial velocity correction; Vref - Vtel
#define SDFITS_COLUMN_VCORR (dsp_fits_column){"VCORR", EXTFITS_ELEMENT_DOUBLE.typestr, "", "", "radial velocity correction; Vref - Vtel", (char*[]){""}}
///Observed Frequency (Hz)
#define SDFITS_COLUMN_OBSFREQ (dsp_fits_column){"OBSFREQ", EXTFITS_ELEMENT_DOUBLE.typestr, EXTFITS_MEASURE_UNIT_HZ, "", "Observed Frequency", (char*[]){""}}
///Image sideband freq (Hz)
#define SDFITS_COLUMN_IMAGFREQ (dsp_fits_column){"IMAGFREQ", EXTFITS_ELEMENT_DOUBLE.typestr, EXTFITS_MEASURE_UNIT_HZ, "", "Image sideband freq", (char*[]){""}}
///LST (seconds) at start of scan
#define SDFITS_COLUMN_LST (dsp_fits_column){"LST", EXTFITS_ELEMENT_DOUBLE.typestr, EXTFITS_MEASURE_UNIT_SECOND, "", "LST at start of scan", (char*[]){""}}
///LST (seconds) at start of scan
#define SDFITS_COLUMN_LST (dsp_fits_column){"LST", EXTFITS_ELEMENT_DOUBLE.typestr, EXTFITS_MEASURE_UNIT_SECOND, "", "LST at start of scan", (char*[]){""}}
///Commanded Azimuth (Degrees)
#define SDFITS_COLUMN_AZIMUTH (dsp_fits_column){"AZIMUTH", EXTFITS_ELEMENT_DOUBLE.typestr, EXTFITS_MEASURE_UNIT_DEGREE, "", "Commanded Azimuth", (char*[]){""}}
///Commanded Elevation (Degrees)
#define SDFITS_COLUMN_ELEVATIO (dsp_fits_column){"ELEVATIO", EXTFITS_ELEMENT_DOUBLE.typestr, EXTFITS_MEASURE_UNIT_DEGREE, "", "Commanded Elevation", (char*[]){""}}
///Opacity at signal freq
#define SDFITS_COLUMN_TAU (dsp_fits_column){"TAU", EXTFITS_ELEMENT_DOUBLE.typestr, EXTFITS_MEASURE_UNIT_PERCENT, "", "Opacity at signal freq", (char*[]){""}}
///Opacity at image freq
#define SDFITS_COLUMN_TAUIMAGE (dsp_fits_column){"TAUIMAGE", EXTFITS_ELEMENT_DOUBLE.typestr, EXTFITS_MEASURE_UNIT_PERCENT, "", "Opacity at image freq", (char*[]){""}}
///Opacity per unit air mass
#define SDFITS_COLUMN_TAUZENIT (dsp_fits_column){"TAUZENIT", EXTFITS_ELEMENT_DOUBLE.typestr, EXTFITS_MEASURE_UNIT_PERCENT, "", "Opacity per unit air mass", (char*[]){""}}
///Decimal fraction 0..1
#define SDFITS_COLUMN_HUMIDITY (dsp_fits_column){"HUMIDITY", EXTFITS_ELEMENT_DOUBLE.typestr, "", "", "Decimal fraction 0..1", (char*[]){""}}
///Ambient Temp (K)
#define SDFITS_COLUMN_TAMBIENT (dsp_fits_column){"TAMBIENT", EXTFITS_ELEMENT_DOUBLE.typestr, EXTFITS_MEASURE_UNIT_KELVIN, "", "Ambient Temp (K)", (char*[]){""}}
///Barometer reading mm Hg
#define SDFITS_COLUMN_PRESSURE (dsp_fits_column){"PRESSURE", EXTFITS_ELEMENT_DOUBLE.typestr, "mm Hg", "", "Barometer reading ", (char*[]){""}}
///Dew point (K)
#define SDFITS_COLUMN_DEWPOINT (dsp_fits_column){"DEWPOINT", EXTFITS_ELEMENT_DOUBLE.typestr, EXTFITS_MEASURE_UNIT_KELVIN, "", "Dew point", (char*[]){""}}
///Wind speed m/s
#define SDFITS_COLUMN_WINDSPEE (dsp_fits_column){"WINDSPEE", EXTFITS_ELEMENT_DOUBLE.typestr, "m/s", "", "Wind speed", (char*[]){""}}
///Degrees West of North
#define SDFITS_COLUMN_WINDDIRE (dsp_fits_column){"WINDDIRE", EXTFITS_ELEMENT_DOUBLE.typestr, EXTFITS_MEASURE_UNIT_DEGREE, "", "Degrees West of North", (char*[]){""}}
///Main-beam efficiency
#define SDFITS_COLUMN_BEAMEFF (dsp_fits_column){"BEAMEFF", EXTFITS_ELEMENT_DOUBLE.typestr, EXTFITS_MEASURE_UNIT_PERCENT, "", "Main-beam efficiency", (char*[]){""}}
///Antenna Aperature Efficiency
#define SDFITS_COLUMN_APEREFF (dsp_fits_column){"APEREFF", EXTFITS_ELEMENT_DOUBLE.typestr, EXTFITS_MEASURE_UNIT_PERCENT, "", "Antenna Aperature Efficiency", (char*[]){""}}
///Rear spillover
#define SDFITS_COLUMN_ETAL (dsp_fits_column){"ETAL", EXTFITS_ELEMENT_DOUBLE.typestr, "", "", "Rear spillover", (char*[]){""}}
///Accounts for forward loss
#define SDFITS_COLUMN_ETAFSS (dsp_fits_column){"ETAFSS", EXTFITS_ELEMENT_DOUBLE.typestr, "", "", "Accounts for forward loss", (char*[]){""}}
///K per Jy
#define SDFITS_COLUMN_ANTGAIN (dsp_fits_column){"ANTGAIN", EXTFITS_ELEMENT_DOUBLE.typestr, "K/Jy", "", "", (char*[]){""}}
///Large main-beam FWHM
#define SDFITS_COLUMN_BMAJ (dsp_fits_column){"BMAJ", EXTFITS_ELEMENT_DOUBLE.typestr, "", "", "Large main-beam FWHM", (char*[]){""}}
///Small main-beam FWHM
#define SDFITS_COLUMN_BMIN (dsp_fits_column){"BMIN", EXTFITS_ELEMENT_DOUBLE.typestr, "", "", "Small main-beam FWHM", (char*[]){""}}
///Beam position angle, measured East of North
#define SDFITS_COLUMN_BPA (dsp_fits_column){"BPA", EXTFITS_ELEMENT_DOUBLE.typestr, EXTFITS_MEASURE_UNIT_DEGREE, "", "Beam position angle", (char*[]){""}}
///Site longitude (Degrees)
#define SDFITS_COLUMN_SITELONG (dsp_fits_column){"SITELONG", EXTFITS_ELEMENT_DOUBLE.typestr, EXTFITS_MEASURE_UNIT_DEGREE, "", "Site longitude", (char*[]){""}}
///Site latitude (Degrees)
#define SDFITS_COLUMN_SITELAT (dsp_fits_column){"SITELAT", EXTFITS_ELEMENT_DOUBLE.typestr, EXTFITS_MEASURE_UNIT_DEGREE, "", "Site latitude", (char*[]){""}}
///site elevation in meters
#define SDFITS_COLUMN_SITEELEV (dsp_fits_column){"SITEELEV", EXTFITS_ELEMENT_DOUBLE.typestr, EXTFITS_MEASURE_UNIT_METER, "", "site elevation", (char*[]){""}}
///Epoch of observation (year)
#define SDFITS_COLUMN_EPOCH (dsp_fits_column){"EPOCH", EXTFITS_ELEMENT_DOUBLE.typestr, EXTFITS_MEASURE_UNIT_YEAR, "", "Epoch of observation", (char*[]){""}}
///Equinox of coords (year)
#define SDFITS_COLUMN_EQUINOX (dsp_fits_column){"EQUINOX", EXTFITS_ELEMENT_DOUBLE.typestr, EXTFITS_MEASURE_UNIT_YEAR, "", "Equinox of coords", (char*[]){""}}

#define SDFITS_TABLE_MAIN (dsp_fits_column[]){\
SDFITS_COLUMN_OBJECT,\
SDFITS_COLUMN_TELESCOP,\
SDFITS_COLUMN_FREQRES,\
SDFITS_COLUMN_BANDWID,\
SDFITS_COLUMN_DATE_OBS,\
SDFITS_COLUMN_TIME,\
SDFITS_COLUMN_EXPOSURE,\
SDFITS_COLUMN_TSYS,\
SDFITS_COLUMN_DATA,\
SDFITS_COLUMN_OBSERVER, \
SDFITS_COLUMN_OBSID, \
SDFITS_COLUMN_PROJID, \
SDFITS_COLUMN_SCAN, \
SDFITS_COLUMN_OBSMODE, \
SDFITS_COLUMN_MOLECULE, \
SDFITS_COLUMN_TRANSITI, \
SDFITS_COLUMN_TEMPSCAL, \
SDFITS_COLUMN_FRONTEND, \
SDFITS_COLUMN_TCAL, \
SDFITS_COLUMN_THOT, \
SDFITS_COLUMN_TCOLD, \
SDFITS_COLUMN_TRX, \
SDFITS_COLUMN_VELDEF, \
SDFITS_COLUMN_VCORR, \
SDFITS_COLUMN_OBSFREQ, \
SDFITS_COLUMN_IMAGFREQ, \
SDFITS_COLUMN_LST, \
SDFITS_COLUMN_LST, \
SDFITS_COLUMN_AZIMUTH, \
SDFITS_COLUMN_ELEVATIO, \
SDFITS_COLUMN_TAU, \
SDFITS_COLUMN_TAUIMAGE, \
SDFITS_COLUMN_TAUZENIT, \
SDFITS_COLUMN_HUMIDITY, \
SDFITS_COLUMN_TAMBIENT, \
SDFITS_COLUMN_PRESSURE, \
SDFITS_COLUMN_DEWPOINT, \
SDFITS_COLUMN_WINDSPEE, \
SDFITS_COLUMN_WINDDIRE, \
SDFITS_COLUMN_BEAMEFF, \
SDFITS_COLUMN_APEREFF, \
SDFITS_COLUMN_ETAL, \
SDFITS_COLUMN_ETAFSS, \
SDFITS_COLUMN_ANTGAIN, \
SDFITS_COLUMN_BMAJ, \
SDFITS_COLUMN_BMIN, \
SDFITS_COLUMN_BPA, \
SDFITS_COLUMN_SITELONG, \
SDFITS_COLUMN_SITELAT, \
SDFITS_COLUMN_SITEELEV, \
SDFITS_COLUMN_EPOCH, \
SDFITS_COLUMN_EQUINOX, \
}

///Global keywords for the SDFITS SINGLE DISH table
///Designation of Telescope.
#define SDFITS_KEYWORD_TELESCOP (dsp_fits_keyword){"TELESCOP", "8A", "", "", "", (char*[]){""}}
///Name of observer.
#define SDFITS_KEYWORD_OBSERVER (dsp_fits_keyword){"OBSERVER", "8A", "", "", "", (char*[]){""}}
///UT date of observation (dd/mm/yy) .
#define SDFITS_KEYWORD_DATE_OBS (dsp_fits_keyword){"DATE-OBS", "8A", "", "", "", (char*[]){""}}
///Max spectral value (K) - for whole file.
#define SDFITS_KEYWORD_DATAMAX (dsp_fits_keyword){"DATAMAX", EXTFITS_ELEMENT_DOUBLE.typestr, "", "", "", (char*[]){""}}
///Min spectral value (K) - for whole file.
#define SDFITS_KEYWORD_DATAMIN (dsp_fits_keyword){"DATAMIN", EXTFITS_ELEMENT_DOUBLE.typestr, "", "", "", (char*[]){""}}

/**
* \brief read a fits file containing a SDFITS Extension
* \param filename The file name of the fits to read
* \param nstreams The number of streams of the data matrix passed by reference
* \param maxes The number of dimensions of the data matrix passed by reference
* \param maxis The sizes of the data matrix
* \return dsp_fits_row pointer describing the fits file content
*/
DLL_EXPORT dsp_fits_row *dsp_fits_read_sdfits(char *filename, long *nstreams, long *maxes, long **maxis);
/**\}*/

#ifdef __cplusplus
}
#endif

#endif //_FITS_EXTENSION_SDFITS_H
