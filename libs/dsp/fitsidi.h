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

#ifndef _FITS_EXTENSION_FITSIDI_H
#define _FITS_EXTENSION_FITSIDI_H

#ifdef  __cplusplus
extern "C" {
#endif
#include <fits.h>

/// \defgroup dsp_FitsExtensionFITSIDI DSP API FITSIDI Extension
/**\{*/
///Antenna polarizations
///I
#define EXTFITS_STOKE_I "1"
///Q
#define EXTFITS_STOKE_Q "2"
///U
#define EXTFITS_STOKE_U "3"
///V
#define EXTFITS_STOKE_V "4"
///RR
#define EXTFITS_STOKE_RR "-1"
///LL
#define EXTFITS_STOKE_LL "-2"
///RL
#define EXTFITS_STOKE_RL "-3"
///LR
#define EXTFITS_STOKE_LR "-4"
///XX
#define EXTFITS_STOKE_XX "-5"
///YY
#define EXTFITS_STOKE_YY "-6"
///XY
#define EXTFITS_STOKE_XY "-7"
///YX
#define EXTFITS_STOKE_YX "-8"

///FITS-IDI Convention Tables
///Antenna polarization information
#define FITS_TABLE_FITSIDI_ANTENNA "ANTENNA"
///Time system information and antenna coordinates
#define FITS_TABLE_FITSIDI_ARRAY_GEOMETRY "ARRAY_GEOMETRY"
///Channel-dependent complex gains
#define FITS_TABLE_FITSIDI_BANDPASS "BANDPASS"
///Baseline-specific gain factors
#define FITS_TABLE_FITSIDI_BASELINE "BASELINE"
///Complex gains as a function of time
#define FITS_TABLE_FITSIDI_CALIBRATION "CALIBRATION"
///Information for flagging data
#define FITS_TABLE_FITSIDI_FLAG "FLAG"
///Frequency setups
#define FITS_TABLE_FITSIDI_FREQUENCY "FREQUENCY"
///Antenna gain curves
#define FITS_TABLE_FITSIDI_GAIN_CURVE "GAIN_CURVE"
///Correlator model parameters
#define FITS_TABLE_FITSIDI_INTERFEROMETER_MODEL "INTERFEROMETER_MODEL"
///Phase cal measurements
#define FITS_TABLE_FITSIDI_PHASE_CAL "PHASE-CAL"
///Information on sources observed
#define FITS_TABLE_FITSIDI_SOURCE "SOURCE"
///System and antenna temperatures
#define FITS_TABLE_FITSIDI_SYSTEM_TEMPERATURE "SYSTEM_TEMPERATURE"
///Visibility data
#define FITS_TABLE_FITSIDI_UV_DATA "UV_DATA"
///Meteorological data
#define FITS_TABLE_FITSIDI_WEATHER "WEATHER"

///FITS-IDI global keywords
///Name/type of correlator
#define FITSIDI_COLUMN_CORRELAT (dsp_fits_column){"CORRELAT", EXTFITS_ELEMENT_STRING.typestr, "", "", "Name/type of correlator", (char*[]){""}}
///Version number of the correlator software that produced the file
#define FITSIDI_COLUMN_FXCORVER (dsp_fits_column){"FXCORVER", EXTFITS_ELEMENT_STRING.typestr, "", "", "Version number of the correlator software that produced the file", (char*[]){""}}

///FITS-IDI common table keywords
///Revision number of the table definition
#define FITSIDI_KEYWORD_TABREV "TABREV"
///Observation identification
#define FITSIDI_KEYWORD_OBSCODE "OBSCODE"
///The number of Stokes parameters
#define FITSIDI_KEYWORD_NO_STKD "NO_STKD"
///The first Stokes parameter coordinate value
#define FITSIDI_KEYWORD_STK_1 "STK_1"
///The number of bands
#define FITSIDI_KEYWORD_NO_BAND "NO_BAND"
///The number of spectral channels per band
#define FITSIDI_KEYWORD_NO_CHAN "NO_CHAN"
///The file reference frequency in Hz
#define FITSIDI_KEYWORD_REF_FREQ "REF_FREQ"
///The channel bandwidth in Hz for the first band in the frequency setup with frequency ID number 1
#define FITSIDI_KEYWORD_CHAN_BW "CHAN_BW"
///The reference pixel for the frequency axis
#define FITSIDI_KEYWORD_REF_PIXL "REF_PIXL"

///Regular axes for the FITS-IDI UV_DATA table data matrix
///Real, imaginary, weight
#define FITSIDI_UV_DATA_AXIS_COMPLEX (dsp_fits_column){"COMPLEX", "", "", "", "Real, imaginary, weight", (char*[]){""}}
///Stokes parameter
#define FITSIDI_UV_DATA_AXIS_STOKES (dsp_fits_column){"STOKES", "", "", "", "Stokes parameter", (char*[]){""}}
///Frequency (spectral channel)
#define FITSIDI_UV_DATA_AXIS_FREQ (dsp_fits_column){"FREQ", "", "", EXTFITS_MEASURE_UNIT_HZ, "Frequency (spectral channel)", (char*[]){""}}
///Band number
#define FITSIDI_UV_DATA_AXIS_BAND (dsp_fits_column){"BAND", "", "", EXTFITS_MEASURE_UNIT_HZ, "Band number", (char*[]){""}}
///Right ascension of the phase center
#define FITSIDI_UV_DATA_AXIS_RA (dsp_fits_column){"RA", "", "", EXTFITS_MEASURE_UNIT_DEGREE, "Right ascension of the phase center", (char*[]){""}}
///Declination of the phase center
#define FITSIDI_UV_DATA_AXIS_DEC (dsp_fits_column){"DEC", "", "", EXTFITS_MEASURE_UNIT_DEGREE, "Declination of the phase center", (char*[]){""}}

///Random parameters for the FITS-IDI UV_DATA table
///seconds u baseline coordinate (-SIN system)
#define FITSIDI_UV_DATA_COLUMN_UU (dsp_fits_column){"UU", "1D", EXTFITS_MEASURE_UNIT_SECOND, "", "u baseline coordinate (-SIN system)", (char*[]){""}}
///seconds v baseline coordinate (-SIN system)
#define FITSIDI_UV_DATA_COLUMN_VV (dsp_fits_column){"VV", "1D", EXTFITS_MEASURE_UNIT_SECOND, "", "v baseline coordinate (-SIN system)", (char*[]){""}}
///seconds w baseline coordinate (-SIN system)
#define FITSIDI_UV_DATA_COLUMN_WW (dsp_fits_column){"WW", "1D", EXTFITS_MEASURE_UNIT_SECOND, "", "w baseline coordinate (-SIN system)", (char*[]){""}}
///seconds u baseline coordinate (-SIN system)
#define FITSIDI_UV_DATA_COLUMN_UU_SIN (dsp_fits_column){"UU---SIN", "1D", EXTFITS_MEASURE_UNIT_SECOND, "", "u baseline coordinate (-SIN system)", (char*[]){""}}
///seconds v baseline coordinate (-SIN system)
#define FITSIDI_UV_DATA_COLUMN_VV_SIN (dsp_fits_column){"VV---SIN", "1D", EXTFITS_MEASURE_UNIT_SECOND, "", "v baseline coordinate (-SIN system)", (char*[]){""}}
///seconds w baseline coordinate (-SIN system)
#define FITSIDI_UV_DATA_COLUMN_WW_SIN (dsp_fits_column){"WW---SIN", "1D", EXTFITS_MEASURE_UNIT_SECOND, "", "w baseline coordinate (-SIN system)", (char*[]){""}}
///seconds u baseline coordinate (-NCP system)
#define FITSIDI_UV_DATA_COLUMN_UU_NCP (dsp_fits_column){"UU---NCP", "1D", EXTFITS_MEASURE_UNIT_SECOND, "", "u baseline coordinate (-NCP system)", (char*[]){""}}
///seconds v baseline coordinate (-NCP system)
#define FITSIDI_UV_DATA_COLUMN_VV_NCP (dsp_fits_column){"VV---NCP", "1D", EXTFITS_MEASURE_UNIT_SECOND, "", "v baseline coordinate (-NCP system)", (char*[]){""}}
///seconds w baseline coordinate (-NCP system)
#define FITSIDI_UV_DATA_COLUMN_WW_NCP (dsp_fits_column){"WW---NCP", "1D", EXTFITS_MEASURE_UNIT_SECOND, "", "w baseline coordinate (-NCP system)", (char*[]){""}}
///days Julian date at 0 hours
#define FITSIDI_UV_DATA_COLUMN_DATE (dsp_fits_column){"DATE", "1D", EXTFITS_MEASURE_UNIT_DAY, "", "Julian date at 0 hours", (char*[]){""}}
///days Time elapsed since 0 hours
#define FITSIDI_UV_DATA_COLUMN_TIME (dsp_fits_column){"TIME", "1D", EXTFITS_MEASURE_UNIT_DAY, "", "Time elapsed since 0 hours", (char*[]){""}}
///Baseline number
#define FITSIDI_UV_DATA_COLUMN_BASELINE (dsp_fits_column){"BASELINE", "1J", "", "", "Baseline number", (char*[]){""}}
///Array number
#define FITSIDI_UV_DATA_COLUMN_ARRAY (dsp_fits_column){"ARRAY", "1J", "", "", "Array number", (char*[]){""}}
///Source ID number
#define FITSIDI_UV_DATA_COLUMN_SOURCE_ID (dsp_fits_column){"SOURCE_ID", "1J", "", "", "Source ID number", (char*[]){""}}
///Frequency setup ID number
#define FITSIDI_UV_DATA_COLUMN_FREQID (dsp_fits_column){"FREQID", "1J", "", "", "Frequency setup ID number", (char*[]){""}}
///seconds Integration time
#define FITSIDI_UV_DATA_COLUMN_INTTIM (dsp_fits_column){"INTTIM", "1D", EXTFITS_MEASURE_UNIT_SECOND, "", "Integration time", (char*[]){""}}
///Weights
#define FITSIDI_UV_DATA_COLUMN_WEIGHT(nstokes, nband) (dsp_fits_column){"WEIGHT", EXTFITS_ELEMENT_FLOAT.typestr itostr(nstokes) "," itostr(nband), "", "", "Weights", (char*[]){""}}

///Mandatory keywords for the FITS-IDI UV_DATA table
///2
#define FITSIDI_UV_DATA_KEYWORD_TABREV (dsp_fits_keyword){"TABREV", EXTFITS_ELEMENT_SHORT.typestr, "", "", "2", (char*[]){""}}

///The number of Stokes parameters
#define FITSIDI_UV_DATA_KEYWORD_NO_STKD (dsp_fits_keyword){"NO_STKD", EXTFITS_ELEMENT_SHORT.typestr, "", "", "The number of Stokes parameters", (char*[]){""}}
///The first Stokes parameter coordinate value
#define FITSIDI_UV_DATA_KEYWORD_STK_1 (dsp_fits_keyword){"STK_1", EXTFITS_ELEMENT_SHORT.typestr, "", "", "The first Stokes parameter coordinate value", (char*[]){""}}
///The number of bands
#define FITSIDI_UV_DATA_KEYWORD_NO_BAND (dsp_fits_keyword){"NO_BAND", EXTFITS_ELEMENT_SHORT.typestr, "", "", "The number of bands", (char*[]){""}}
///The number of spectral channels per band
#define FITSIDI_UV_DATA_KEYWORD_NO_CHAN (dsp_fits_keyword){"NO_CHAN", EXTFITS_ELEMENT_SHORT.typestr, "", "", "The number of spectral channels per band", (char*[]){""}}
///The file reference frequency in Hz
#define FITSIDI_UV_DATA_KEYWORD_REF_FREQ (dsp_fits_keyword){"REF_FREQ", EXTFITS_ELEMENT_FLOAT.typestr, "", "", "The file reference frequency in Hz", (char*[]){""}}
///The channel bandwidth in Hz for the first band in the frequency setup with frequency ID number 1
#define FITSIDI_UV_DATA_KEYWORD_CHAN_BW (dsp_fits_keyword){"CHAN_BW", EXTFITS_ELEMENT_FLOAT.typestr, "", "", "The channel bandwidth in Hz for the first band in the frequency setup with frequency ID number 1", (char*[]){""}}
///The reference pixel for the frequency axis
#define FITSIDI_UV_DATA_KEYWORD_REF_PIXL (dsp_fits_keyword){"REF_PIXL", EXTFITS_ELEMENT_FLOAT.typestr, "", "", "The reference pixel for the frequency axis", (char*[]){""}}
///Mean equinox
#define FITSIDI_UV_DATA_KEYWORD_EQUINOX (dsp_fits_keyword){"EQUINOX", "8A", "", "", "Mean equinox", (char*[]){""}}
///Type of data weights
#define FITSIDI_UV_DATA_KEYWORD_WEIGHTYP (dsp_fits_keyword){"WEIGHTYP", "8A", "", "", "Type of data weights", (char*[]){""}}

///Columns for the FITS-IDI ARRAY_GEOMETRY table
///Antenna name
#define FITSIDI_ARRAY_GEOMETRY_COLUMN_ANNAME (dsp_fits_column){"ANNAME", "8A", "", "", "Antenna name", (char*[]){""}}
///meters Antenna station coordinates (x, y, z)
#define FITSIDI_ARRAY_GEOMETRY_COLUMN_STABXYZ (dsp_fits_column){"STABXYZ", "3D", EXTFITS_MEASURE_UNIT_METER, "", "Antenna station coordinates (x, y, z)", (char*[]){""}}
///meters/s First-order derivatives of the station coordinates with respect to time
#define FITSIDI_ARRAY_GEOMETRY_COLUMN_DERXYZ (dsp_fits_column){"DERXYZ", "3E", "meters/s", "", "First-order derivatives of the station coordinates with respect to time", (char*[]){""}}
///Orbital parameters
#define FITSIDI_ARRAY_GEOMETRY_COLUMN_ORBPARM(norb) (dsp_fits_column){"ORBPARM", EXTFITS_ELEMENT_DOUBLE.typestr itostr(norb), "", "", "Orbital parameters", (char*[]){""}}
///Antenna number
#define FITSIDI_ARRAY_GEOMETRY_COLUMN_NOSTA (dsp_fits_column){"NOSTA", "1I", "", "", "Antenna number", (char*[]){""}}
///Mount type
#define FITSIDI_ARRAY_GEOMETRY_COLUMN_MNTSTA (dsp_fits_column){"MNTSTA", "1J", "", "", "Mount type", (char*[]){""}}
///meters Axis offset
#define FITSIDI_ARRAY_GEOMETRY_COLUMN_STAXOF (dsp_fits_column){"STAXOF", "3E", EXTFITS_MEASURE_UNIT_METER, "", "Axis offset", (char*[]){""}}
///meters Antenna diameter
#define FITSIDI_ARRAY_GEOMETRY_COLUMN_DIAMETER (dsp_fits_column){"DIAMETER", "1E", EXTFITS_MEASURE_UNIT_METER, "", "Antenna diameter", (char*[]){""}}

///Mandatory keywords for the FITS-IDI ARRAY_GEOMETRY table
///1
#define FITSIDI_ARRAY_GEOMETRY_KEYWORD_TABREV (dsp_fits_keyword){"TABREV", EXTFITS_ELEMENT_SHORT.typestr, "", "", "1", (char*[]){""}}
///Array number
#define FITSIDI_ARRAY_GEOMETRY_KEYWORD_EXTVER (dsp_fits_keyword){"EXTVER", EXTFITS_ELEMENT_SHORT.typestr, "", "", "Array number", (char*[]){""}}
///Array name
#define FITSIDI_ARRAY_GEOMETRY_KEYWORD_ARRNAM (dsp_fits_keyword){"ARRNAM", EXTFITS_ELEMENT_STRING.typestr, "", "", "Array name", (char*[]){""}}
///Coordinate frame
#define FITSIDI_ARRAY_GEOMETRY_KEYWORD_FRAME (dsp_fits_keyword){"FRAME", EXTFITS_ELEMENT_STRING.typestr, "", "", "Coordinate frame", (char*[]){""}}
///x coordinate of array center (m)
#define FITSIDI_ARRAY_GEOMETRY_KEYWORD_ARRAYX (dsp_fits_keyword){"ARRAYX", EXTFITS_ELEMENT_FLOAT.typestr, "", "", "x coordinate of array center (m)", (char*[]){""}}
///y coordinate of array center (m)
#define FITSIDI_ARRAY_GEOMETRY_KEYWORD_ARRAYY (dsp_fits_keyword){"ARRAYY", EXTFITS_ELEMENT_FLOAT.typestr, "", "", "y coordinate of array center (m)", (char*[]){""}}
///z coordinate of array center (m)
#define FITSIDI_ARRAY_GEOMETRY_KEYWORD_ARRAYZ (dsp_fits_keyword){"ARRAYZ", EXTFITS_ELEMENT_FLOAT.typestr, "", "", "z coordinate of array center (m)", (char*[]){""}}
///norb= number orbital parameters in table
#define FITSIDI_ARRAY_GEOMETRY_KEYWORD_NUMORB (dsp_fits_keyword){"NUMORB", EXTFITS_ELEMENT_SHORT.typestr, "", "", "norb= number orbital parameters in table", (char*[]){""}}
///Reference frequency (Hz)
#define FITSIDI_ARRAY_GEOMETRY_KEYWORD_FREQ (dsp_fits_keyword){"FREQ", EXTFITS_ELEMENT_FLOAT.typestr, "", "", "Reference frequency (Hz)", (char*[]){""}}
///Time system
#define FITSIDI_ARRAY_GEOMETRY_KEYWORD_TIMESYS (dsp_fits_keyword){"TIMESYS", EXTFITS_ELEMENT_STRING.typestr, "", "", "Time system", (char*[]){""}}
///Reference date
#define FITSIDI_ARRAY_GEOMETRY_KEYWORD_RDATE (dsp_fits_keyword){"RDATE", EXTFITS_ELEMENT_DOUBLE.typestr, "", "", "Reference date", (char*[]){""}}
///GST at 0h on reference date (degrees)
#define FITSIDI_ARRAY_GEOMETRY_KEYWORD_GSTIA0 (dsp_fits_keyword){"GSTIA0", EXTFITS_ELEMENT_FLOAT.typestr, "", "", "GST at 0h on reference date (degrees)", (char*[]){""}}
///Earth's rotation rate (degrees/day)
#define FITSIDI_ARRAY_GEOMETRY_KEYWORD_DEGPDY (dsp_fits_keyword){"DEGPDY", EXTFITS_ELEMENT_FLOAT.typestr, "", "", "Earth's rotation rate (degrees/day)", (char*[]){""}}
///UT1 - UTC (sec)
#define FITSIDI_ARRAY_GEOMETRY_KEYWORD_UT1UTC (dsp_fits_keyword){"UT1UTC", EXTFITS_ELEMENT_FLOAT.typestr, "", "", "UT1 - UTC (sec)", (char*[]){""}}
///IAT - UTC (sec)
#define FITSIDI_ARRAY_GEOMETRY_KEYWORD_IATUTC (dsp_fits_keyword){"IATUTC", EXTFITS_ELEMENT_FLOAT.typestr, "", "", "IAT - UTC (sec)", (char*[]){""}}
///x coordinate of North Pole (arc seconds)
#define FITSIDI_ARRAY_GEOMETRY_KEYWORD_POLARX (dsp_fits_keyword){"POLARX", EXTFITS_ELEMENT_FLOAT.typestr, "", "", "x coordinate of North Pole (arc seconds)", (char*[]){""}}
///y coordinate of North Pole (arc seconds)
#define FITSIDI_ARRAY_GEOMETRY_KEYWORD_POLARY (dsp_fits_keyword){"POLARY", EXTFITS_ELEMENT_FLOAT.typestr, "", "", "y coordinate of North Pole (arc seconds)", (char*[]){""}}

///Columns for the FITS-IDI ANTENNA table
///days Central time of period covered by record
#define FITSIDI_ANTENNA_COLUMN_TIME (dsp_fits_column){"TIME", "1D", EXTFITS_MEASURE_UNIT_DAY, "", "Central time of period covered by record", (char*[]){""}}
///days Duration of period covered by record
#define FITSIDI_ANTENNA_COLUMN_TIME_INTERVAL (dsp_fits_column){"TIME_INTERVAL", "1E", EXTFITS_MEASURE_UNIT_DAY, "", "Duration of period covered by record", (char*[]){""}}
///Antenna name
#define FITSIDI_ANTENNA_COLUMN_ANNAME (dsp_fits_column){"ANNAME", "8A", "", "", "Antenna name", (char*[]){""}}
///Antenna number
#define FITSIDI_ANTENNA_COLUMN_ANTENNA_NO (dsp_fits_column){"ANTENNA_NO", "1J", "", "", "Antenna number", (char*[]){""}}
///Array number
#define FITSIDI_ANTENNA_COLUMN_ARRAY (dsp_fits_column){"ARRAY", "1J", "", "", "Array number", (char*[]){""}}
///Frequency setup number
#define FITSIDI_ANTENNA_COLUMN_FREQID (dsp_fits_column){"FREQID", "1J", "", "", "Frequency setup number", (char*[]){""}}
///Number of digitizer levels
#define FITSIDI_ANTENNA_COLUMN_NO_LEVELS (dsp_fits_column){"NO_LEVELS", "1J", "", "", "Number of digitizer levels", (char*[]){""}}
///Feed A polarization label
#define FITSIDI_ANTENNA_COLUMN_POLTYA (dsp_fits_column){"POLTYA", "1A", "Feed A polarization label", (char*[]){""}}
///degrees Feed A orientation
#define FITSIDI_ANTENNA_COLUMN_POLAA(nband) (dsp_fits_column){"POLAA", EXTFITS_ELEMENT_FLOAT.typestr itostr(nband), EXTFITS_MEASURE_UNIT_DEGREE, "", "Feed A orientation", (char*[]){""}}
///Feed A polarization parameters
#define FITSIDI_ANTENNA_COLUMN_POLCALA(npcal, nband) (dsp_fits_column){"POLCALA", EXTFITS_ELEMENT_FLOAT.typestr itostr(npcal) "," itostr(nband), "", "", "Feed A polarization parameters", (char*[]){""}}
///Feed B polarization label
#define FITSIDI_ANTENNA_COLUMN_POLTYB (dsp_fits_column){"POLTYB", "1A", "Feed B polarization label", (char*[]){""}}
///degrees Feed B orientation
#define FITSIDI_ANTENNA_COLUMN_POLAB(nband) (dsp_fits_column){"POLAB", EXTFITS_ELEMENT_FLOAT.typestr itostr(nband), EXTFITS_MEASURE_UNIT_DEGREE, "", "Feed B orientation", (char*[]){""}}
///Feed B polarization parameters
#define FITSIDI_ANTENNA_COLUMN_POLCALB(npcal, nband) (dsp_fits_column){"POLCALB", EXTFITS_ELEMENT_FLOAT.typestr itostr(npcal) "," itostr(nband), "", "", "Feed B polarization parameters", (char*[]){""}}
///degrees / m Antenna beam fwhm
#define FITSIDI_ANTENNA_COLUMN_BEAMFWHM(nband) (dsp_fits_column){"BEAMFWHM", EXTFITS_ELEMENT_FLOAT.typestr itostr(nband), EXTFITS_MEASURE_UNIT_DEGREE, "", "/ m Antenna beam fwhm", (char*[]){""}}

///Polarization parameters
///Linear approximation for circular feeds
#define FITSIDI_ANTENNA_POLPARM_APPROX "APPROX"
///Linear approximation for linear feeds
#define FITSIDI_ANTENNA_POLPARM_LIN "X-Y LIN"
///Orientation and ellipticity
#define FITSIDI_ANTENNA_POLPARM_ORI_ELP "ORI-ELP"

///Mandatory keywords for the FITS-IDI ANTENNA table
///1
#define FITSIDI_ANTENNA_KEYWORD_TABREV (dsp_fits_keyword){"TABREV", EXTFITS_ELEMENT_SHORT.typestr, "", "", "1", (char*[]){""}}
///npcal = 0 or 2, number of polarization calibration constants
#define FITSIDI_ANTENNA_KEYWORD_NOPCAL (dsp_fits_keyword){"NOPCAL", EXTFITS_ELEMENT_SHORT.typestr, "", "", "npcal = 0 or 2, number of polarization calibration constants", (char*[]){""}}
///The feed polarization parameterization
#define FITSIDI_ANTENNA_KEYWORD_POLTYPE (dsp_fits_keyword){"POLTYPE", EXTFITS_ELEMENT_STRING.typestr, "", "", "The feed polarization parameterization", (char*[]){""}}

///Columns for the FITS-IDI FREQUENCY table
///Frequency setup number
#define FITSIDI_FREQUENCY_COLUMN_FREQID (dsp_fits_column){"FREQID", "1J", "", "", "Frequency setup number", (char*[]){""}}
///Hz Frequency offsets
#define FITSIDI_FREQUENCY_COLUMN_BANDFREQ(nband) (dsp_fits_column){"BANDFREQ", EXTFITS_ELEMENT_DOUBLE.typestr itostr(nband), EXTFITS_MEASURE_UNIT_HZ, "", "Frequency offsets", (char*[]){""}}
///Hz Individual channel widths
#define FITSIDI_FREQUENCY_COLUMN_CH_WIDTH(nband) (dsp_fits_column){"CH_WIDTH", EXTFITS_ELEMENT_FLOAT.typestr itostr(nband), EXTFITS_MEASURE_UNIT_HZ, "", "Individual channel widths", (char*[]){""}}
///Hz Total bandwidths of bands
#define FITSIDI_FREQUENCY_COLUMN_TOTAL_BANDWIDTH(nband) (dsp_fits_column){"TOTAL_BANDWIDTH", EXTFITS_ELEMENT_FLOAT.typestr itostr(nband), EXTFITS_MEASURE_UNIT_HZ, "", "Total bandwidths of bands", (char*[]){""}}
///Sideband flag
#define FITSIDI_FREQUENCY_COLUMN_SIDEBAND(nband) (dsp_fits_column){"SIDEBAND", EXTFITS_ELEMENT_INT.typestr itostr(nband), "", "", "Sideband flag", (char*[]){""}}

///Mandatory keywords for the FITS-IDI FREQUENCY table
///1
#define FITSIDI_FREQUENCY_KEYWORD_TABREV (dsp_fits_keyword){"TABREV", EXTFITS_ELEMENT_SHORT.typestr, "", "", "1", (char*[]){""}}

///Frames of reference for VELTYP
///Local standard of rest
#define FITSIDI_SOURCE_VELTYP_LSR "LSR"
///Solar system barycenter
#define FITSIDI_SOURCE_VELTYP_BARYCENT "BARYCENT"
///Center of mass of the Earth
#define FITSIDI_SOURCE_VELTYP_GEOCENTR "GEOCENTR"
///Uncorrected
#define FITSIDI_SOURCE_VELTYP_TOPOCENT "TOPOCENT"

///Columns for the FITS-IDI SOURCE table
///Source ID number
#define FITSIDI_SOURCE_COLUMN_SOURCE_ID (dsp_fits_column){"SOURCE_ID", "1J", "", "", "Source ID number", (char*[]){""}}
///Source name
#define FITSIDI_SOURCE_COLUMN_SOURCE (dsp_fits_column){"SOURCE", "16A", "", "", "Source name", (char*[]){""}}
///Source name numeric qualifier
#define FITSIDI_SOURCE_COLUMN_QUAL (dsp_fits_column){"QUAL", "1J", "", "", "Source name numeric qualifier", (char*[]){""}}
///Calibrator code
#define FITSIDI_SOURCE_COLUMN_CALCODE (dsp_fits_column){"CALCODE", "4A", "", "", "Calibrator code", (char*[]){""}}
///Frequency setup number
#define FITSIDI_SOURCE_COLUMN_FREQID (dsp_fits_column){"FREQID", "1J", "", "", "Frequency setup number", (char*[]){""}}
///Jy Stokes I flux density
#define FITSIDI_SOURCE_COLUMN_IFLUX(nband) (dsp_fits_column){"IFLUX", EXTFITS_ELEMENT_FLOAT.typestr itostr(nband), "Jy", "", "Stokes I flux density", (char*[]){""}}
///Jy Stokes Q flux density
#define FITSIDI_SOURCE_COLUMN_QFLUX(nband) (dsp_fits_column){"QFLUX", EXTFITS_ELEMENT_FLOAT.typestr itostr(nband), "Jy", "", "Stokes Q flux density", (char*[]){""}}
///Jy Stokes U flux density
#define FITSIDI_SOURCE_COLUMN_UFLUX(nband) (dsp_fits_column){"UFLUX", EXTFITS_ELEMENT_FLOAT.typestr itostr(nband), "Jy", "", "Stokes U flux density", (char*[]){""}}
///Jy Stokes V flux density
#define FITSIDI_SOURCE_COLUMN_VFLUX(nband) (dsp_fits_column){"VFLUX", EXTFITS_ELEMENT_FLOAT.typestr itostr(nband), "Jy", "", "Stokes V flux density", (char*[]){""}}
///Jy Spectral index for each band
#define FITSIDI_SOURCE_COLUMN_ALPHA(nband) (dsp_fits_column){"ALPHA", EXTFITS_ELEMENT_FLOAT.typestr itostr(nband), "Jy", "", "Spectral index for each band", (char*[]){""}}
///Hz Frequency offset for each band
#define FITSIDI_SOURCE_COLUMN_FREQOFF(nband) (dsp_fits_column){"FREQOFF", EXTFITS_ELEMENT_FLOAT.typestr itostr(nband), EXTFITS_MEASURE_UNIT_HZ, "", "Frequency offset for each band", (char*[]){""}}
///degrees Right ascension at mean equinox
#define FITSIDI_SOURCE_COLUMN_RAEPO (dsp_fits_column){"RAEPO", "1D", EXTFITS_MEASURE_UNIT_DEGREE, "", "Right ascension at mean equinox", (char*[]){""}}
///degrees Declination at mean equinox
#define FITSIDI_SOURCE_COLUMN_DECEPO (dsp_fits_column){"DECEPO", "1D", EXTFITS_MEASURE_UNIT_DEGREE, "", "Declination at mean equinox", (char*[]){""}}
///Mean equinox
#define FITSIDI_SOURCE_COLUMN_EQUINOX (dsp_fits_column){"EQUINOX", "8A", "", "", "Mean equinox", (char*[]){""}}
///degrees Apparent right ascension
#define FITSIDI_SOURCE_COLUMN_RAAPP (dsp_fits_column){"RAAPP", "1D", EXTFITS_MEASURE_UNIT_DEGREE, "", "Apparent right ascension", (char*[]){""}}
///degrees Apparent declination
#define FITSIDI_SOURCE_COLUMN_DECAPP (dsp_fits_column){"DECAPP", "1D", EXTFITS_MEASURE_UNIT_DEGREE, "", "Apparent declination", (char*[]){""}}
///meters/sec Systemic velocity for each band
#define FITSIDI_SOURCE_COLUMN_SYSVEL(nband) (dsp_fits_column){"SYSVEL", EXTFITS_ELEMENT_DOUBLE.typestr itostr(nband), "meters/sec", "", "Systemic velocity for each band", (char*[]){""}}
///Velocity type
#define FITSIDI_SOURCE_COLUMN_VELTYP (dsp_fits_column){"VELTYP", "8A", "", "", "Velocity type", (char*[]){""}}
///Velocity definition
#define FITSIDI_SOURCE_COLUMN_VELDEF (dsp_fits_column){"VELDEF", "8A", "", "", "Velocity definition", (char*[]){""}}
///Hz Line rest frequency for each band
#define FITSIDI_SOURCE_COLUMN_RESTFREQ(nband) (dsp_fits_column){"RESTFREQ", EXTFITS_ELEMENT_DOUBLE.typestr itostr(nband), EXTFITS_MEASURE_UNIT_HZ, "", "Line rest frequency for each band", (char*[]){""}}
///degrees/day Proper motion in right ascension
#define FITSIDI_SOURCE_COLUMN_PMRA (dsp_fits_column){"PMRA", "1D", "degrees/day", "", "Proper motion in right ascension", (char*[]){""}}
///degrees/day Proper motion in declination
#define FITSIDI_SOURCE_COLUMN_PMDEC (dsp_fits_column){"PMDEC", "1D", "degrees/day", "", "Proper motion in declination", (char*[]){""}}
///arcseconds Parallax of source
#define FITSIDI_SOURCE_COLUMN_PARALLAX (dsp_fits_column){"PARALLAX", "1E", EXTFITS_MEASURE_UNIT_ARCSEC, "", "Parallax of source", (char*[]){""}}
///years Epoch of observation
#define FITSIDI_SOURCE_COLUMN_EPOCH (dsp_fits_column){"EPOCH", "1D", EXTFITS_MEASURE_UNIT_YEAR, "", "Epoch of observation", (char*[]){""}}

///Mandatory keywords for the FITS-IDI SOURCE table
///1
#define FITSIDI_SOURCE_KEYWORD_TABREV (dsp_fits_keyword){"TABREV", EXTFITS_ELEMENT_SHORT.typestr, "", "", "1", (char*[]){""}}

///Columns for the FITS-IDI INTERFEROMETER_MODEL table
///days Starting time of interval
#define FITSIDI_INTERFEROMETER_MODEL_COLUMN_TIME (dsp_fits_column){"TIME", "1D", EXTFITS_MEASURE_UNIT_DAY, "", "Starting time of interval", (char*[]){""}}
///days Duration of interval
#define FITSIDI_INTERFEROMETER_MODEL_COLUMN_TIME_INTERVAL (dsp_fits_column){"TIME_INTERVAL", "1E", EXTFITS_MEASURE_UNIT_DAY, "", "Duration of interval", (char*[]){""}}
///Source ID number
#define FITSIDI_INTERFEROMETER_MODEL_COLUMN_SOURCE_ID (dsp_fits_column){"SOURCE_ID", "1J", "", "", "Source ID number", (char*[]){""}}
///Antenna number
#define FITSIDI_INTERFEROMETER_MODEL_COLUMN_ANTENNA_NO (dsp_fits_column){"ANTENNA_NO", "1J", "", "", "Antenna number", (char*[]){""}}
///Array number
#define FITSIDI_INTERFEROMETER_MODEL_COLUMN_ARRAY (dsp_fits_column){"ARRAY", "1J", "", "", "Array number", (char*[]){""}}
///Frequency setup number
#define FITSIDI_INTERFEROMETER_MODEL_COLUMN_FREQID (dsp_fits_column){"FREQID", "1J", "", "", "Frequency setup number", (char*[]){""}}
///rad m−2 Ionospheric Faraday rotation
#define FITSIDI_INTERFEROMETER_MODEL_COLUMN_I_FAR_ROT (dsp_fits_column){"I.FAR.ROT", "1E", "rad m−2", "", "Ionospheric Faraday rotation", (char*[]){""}}
///Hz Time variable frequency offsets
#define FITSIDI_INTERFEROMETER_MODEL_COLUMN_FREQ_VAR(nband) (dsp_fits_column){"FREQ.VAR", EXTFITS_ELEMENT_FLOAT.typestr itostr(nband), EXTFITS_MEASURE_UNIT_HZ, "", "Time variable frequency offsets", (char*[]){""}}
///turns Phase delay polynomials for polarization 1
#define FITSIDI_INTERFEROMETER_MODEL_COLUMN_PDELAY_1(npoly, nband) (dsp_fits_column){"PDELAY_1", EXTFITS_ELEMENT_DOUBLE.typestr itostr(npoly) "," itostr(nband), "turns", "", "Phase delay polynomials for polarization 1", (char*[]){""}}
///seconds Group delay polynomials for polarization 1
#define FITSIDI_INTERFEROMETER_MODEL_COLUMN_GDELAY_1(npoly, nband) (dsp_fits_column){"GDELAY_1", EXTFITS_ELEMENT_DOUBLE.typestr itostr(npoly) "," itostr(nband), EXTFITS_MEASURE_UNIT_SECOND, "", "Group delay polynomials for polarization 1", (char*[]){""}}
///Hz Phase delay rate polynomials for polarization 1
#define FITSIDI_INTERFEROMETER_MODEL_COLUMN_PRATE_1(npoly, nband) (dsp_fits_column){"PRATE_1", EXTFITS_ELEMENT_DOUBLE.typestr itostr(npoly) "," itostr(nband), EXTFITS_MEASURE_UNIT_HZ, "", "Phase delay rate polynomials for polarization 1", (char*[]){""}}
///sec/sec Group delay rate polynomials for polarization 1
#define FITSIDI_INTERFEROMETER_MODEL_COLUMN_GRATE_1(npoly, nband) (dsp_fits_column){"GRATE_1", EXTFITS_ELEMENT_DOUBLE.typestr itostr(npoly) "," itostr(nband), "sec/sec", "", "Group delay rate polynomials for polarization 1", (char*[]){""}}
///sec m−2 Dispersive delay for polarization 1
#define FITSIDI_INTERFEROMETER_MODEL_COLUMN_DISP_1 (dsp_fits_column){"DISP_1", "1E", EXTFITS_MEASURE_UNIT_SECOND, "", "Dispersive delay for polarization 1", (char*[]){""}}
///sec m−2/sec Rate of change of dispersive delay for polarization 1
#define FITSIDI_INTERFEROMETER_MODEL_COLUMN_DDISP_1 (dsp_fits_column){"DDISP_1", "1E", "sec m−2/sec", "", " Rate of change of dispersive delay for polarization 1", (char*[]){""}}
///turns Phase delay polynomials for polarization 2
#define FITSIDI_INTERFEROMETER_MODEL_COLUMN_PDELAY_2(npoly, nband) (dsp_fits_column){"PDELAY_2", EXTFITS_ELEMENT_DOUBLE.typestr itostr(npoly) "," itostr(nband), "turns", "", "Phase delay polynomials for polarization 2", (char*[]){""}}
///seconds Group delay polynomials for polarization 2
#define FITSIDI_INTERFEROMETER_MODEL_COLUMN_GDELAY_2(npoly, nband) (dsp_fits_column){"GDELAY_2", EXTFITS_ELEMENT_DOUBLE.typestr itostr(npoly) "," itostr(nband), EXTFITS_MEASURE_UNIT_SECOND, "", "Group delay polynomials for polarization 2", (char*[]){""}}
///Hz Phase delay rate polynomials for polarization 2
#define FITSIDI_INTERFEROMETER_MODEL_COLUMN_PRATE_2(npoly, nband) (dsp_fits_column){"PRATE_2", EXTFITS_ELEMENT_DOUBLE.typestr itostr(npoly) "," itostr(nband), EXTFITS_MEASURE_UNIT_HZ, "", "Phase delay rate polynomials for polarization 2", (char*[]){""}}
///sec/sec Group delay rate polynomials for polarization 2
#define FITSIDI_INTERFEROMETER_MODEL_COLUMN_GRATE_2(npoly, nband) (dsp_fits_column){"GRATE_2", EXTFITS_ELEMENT_DOUBLE.typestr itostr(npoly) "," itostr(nband), "sec/sec", "", "Group delay rate polynomials for polarization 2", (char*[]){""}}
///sec m−2 Dispersive delay for polarization 2
#define FITSIDI_INTERFEROMETER_MODEL_COLUMN_DISP_2 (dsp_fits_column){"DISP_2", "1E", EXTFITS_MEASURE_UNIT_SECOND, "", "Dispersive delay for polarization 2", (char*[]){""}}
///sec m−2/sec Rate of change of dispersive delay for polarization 2
#define FITSIDI_INTERFEROMETER_MODEL_COLUMN_DDISP_2 (dsp_fits_column){"DDISP_2", "1E", "sec m−2/sec", "", " Rate of change of dispersive delay for polarization 2", (char*[]){""}}

///Mandatory keywords for the FITS-IDI INTERFEROMETER_MODEL table
///2
#define FITSIDI_INTERFEROMETER_MODEL_KEYWORD_TABREV (dsp_fits_keyword){"TABREV", EXTFITS_ELEMENT_SHORT.typestr, "", "", "2", (char*[]){""}}
///Number of polynomial terms npoly
#define FITSIDI_INTERFEROMETER_MODEL_KEYWORD_NPOLY (dsp_fits_keyword){"NPOLY", EXTFITS_ELEMENT_SHORT.typestr, "", "", "Number of polynomial terms npoly", (char*[]){""}}
///Number of polarizations
#define FITSIDI_INTERFEROMETER_MODEL_KEYWORD_NO_POL (dsp_fits_keyword){"NO_POL", EXTFITS_ELEMENT_SHORT.typestr, "", "", "Number of polarizations", (char*[]){""}}

///Columns for the FITS-IDI SYSTEM_TEMPERATURE table
///days Central time of interval
#define FITSIDI_SYSTEM_TEMPERATURE_COLUMN_TIME (dsp_fits_column){"TIME", "1D", EXTFITS_MEASURE_UNIT_DAY, "", "Central time of interval", (char*[]){""}}
///days Duration of interval
#define FITSIDI_SYSTEM_TEMPERATURE_COLUMN_TIME_INTERVAL (dsp_fits_column){"TIME_INTERVAL", "1E", EXTFITS_MEASURE_UNIT_DAY, "", "Duration of interval", (char*[]){""}}
///Source ID number
#define FITSIDI_SYSTEM_TEMPERATURE_COLUMN_SOURCE_ID (dsp_fits_column){"SOURCE_ID", "1J", "", "", "Source ID number", (char*[]){""}}
///Antenna number
#define FITSIDI_SYSTEM_TEMPERATURE_COLUMN_ANTENNA_NO (dsp_fits_column){"ANTENNA_NO", "1J", "", "", "Antenna number", (char*[]){""}}
///Array number
#define FITSIDI_SYSTEM_TEMPERATURE_COLUMN_ARRAY (dsp_fits_column){"ARRAY", "1J", "", "", "Array number", (char*[]){""}}
///Frequency setup number
#define FITSIDI_SYSTEM_TEMPERATURE_COLUMN_FREQID (dsp_fits_column){"FREQID", "1J", "", "", "Frequency setup number", (char*[]){""}}
///Kelvin System temperatures for polarization 1
#define FITSIDI_SYSTEM_TEMPERATURE_COLUMN_TSYS_1(nband) (dsp_fits_column){"TSYS_1", EXTFITS_ELEMENT_FLOAT.typestr itostr(nband), EXTFITS_MEASURE_UNIT_KELVIN, "", "System temperatures for polarization 1", (char*[]){""}}
///Kelvin Antenna temperatures for polarization 1
#define FITSIDI_SYSTEM_TEMPERATURE_COLUMN_TANT_1(nband) (dsp_fits_column){"TANT_1", EXTFITS_ELEMENT_FLOAT.typestr itostr(nband), EXTFITS_MEASURE_UNIT_KELVIN, "", "Antenna temperatures for polarization 1", (char*[]){""}}
///Kelvin System temperatures for polarization 2
#define FITSIDI_SYSTEM_TEMPERATURE_COLUMN_TSYS_2(nband) (dsp_fits_column){"TSYS_2", EXTFITS_ELEMENT_FLOAT.typestr itostr(nband), EXTFITS_MEASURE_UNIT_KELVIN, "", "System temperatures for polarization 2", (char*[]){""}}
///Kelvin Antenna temperatures for polarization 2
#define FITSIDI_SYSTEM_TEMPERATURE_COLUMN_TANT_2(nband) (dsp_fits_column){"TANT_2", EXTFITS_ELEMENT_FLOAT.typestr itostr(nband), EXTFITS_MEASURE_UNIT_KELVIN, "", "Antenna temperatures for polarization 2", (char*[]){""}}

///Mandatory keywords for the FITS-IDI SYSTEM_TEMPERATURE table
///1
#define FITSIDI_SYSTEM_TEMPERATURE_KEYWORD_TABREV (dsp_fits_keyword){"TABREV", EXTFITS_ELEMENT_SHORT.typestr, "", "", "1", (char*[]){""}}
///Number of polarizations in the table
#define FITSIDI_SYSTEM_TEMPERATURE_KEYWORD_NO_POL (dsp_fits_keyword){"NO_POL", EXTFITS_ELEMENT_SHORT.typestr, "", "", "Number of polarizations in the table", (char*[]){""}}

///Types for x and y values
///None
#define XY_None "0"
///Elevation in degrees
#define XY_Elevation_in_degrees "1"
///Zenith angle in degrees
#define XY_Zenith_angle_in_degrees "2"
///Hour angle in degrees
#define XY_Hour_angle_in_degrees "3"
///Declination in degrees
#define XY_Declination_in_degrees "4"
///Co-declination in degrees
#define XY_Codeclination_in_degrees "5"

///Spherical harmonic coefficients in GAIN_1 and GAIN 2
///A00
#define spherical_harmonic_coefficients_A00 "1"
///A10
#define spherical_harmonic_coefficients_A10 "2"
///A11E
#define spherical_harmonic_coefficients_A11E "3"
///A110
#define spherical_harmonic_coefficients_A110 "4"
///A20
#define spherical_harmonic_coefficients_A20 "5"
///A21E
#define spherical_harmonic_coefficients_A21E "6"
///A210
#define spherical_harmonic_coefficients_A210 "7"
///A22E
#define spherical_harmonic_coefficients_A22E "8"
///A220
#define spherical_harmonic_coefficients_A220 "9"
///A30
#define spherical_harmonic_coefficients_A30 "10"

///Columns for the FITS-IDI GAIN_CURVE table
///Antenna number
#define FITSIDI_GAIN_CURVE_COLUMN_ANTENNA_NO (dsp_fits_column){"ANTENNA_NO", "1J", "", "", "Antenna number", (char*[]){""}}
///Array number
#define FITSIDI_GAIN_CURVE_COLUMN_ARRAY (dsp_fits_column){"ARRAY", "1J", "", "", "Array number", (char*[]){""}}
///Frequency setup number
#define FITSIDI_GAIN_CURVE_COLUMN_FREQID (dsp_fits_column){"FREQID", "1J", "", "", "Frequency setup number", (char*[]){""}}
///Gain curve types for polarization 1
#define FITSIDI_GAIN_CURVE_COLUMN_TYPE_1(nband) (dsp_fits_column){"TYPE_1", EXTFITS_ELEMENT_INT.typestr itostr(nband), "", "", "Gain curve types for polarization 1", (char*[]){""}}
///Number of terms or entries for polarization 1
#define FITSIDI_GAIN_CURVE_COLUMN_NTERM_1(nband) (dsp_fits_column){"NTERM_1", EXTFITS_ELEMENT_INT.typestr itostr(nband), "", "", "Number of terms or entries for polarization 1", (char*[]){""}}
///x value types for polarization 1
#define FITSIDI_GAIN_CURVE_COLUMN_X_TYP_1(nband) (dsp_fits_column){"X_TYP_1", EXTFITS_ELEMENT_INT.typestr itostr(nband), "", "", "x value types for polarization 1", (char*[]){""}}
///y value types for polarization 1
#define FITSIDI_GAIN_CURVE_COLUMN_Y_TYP_1(nband) (dsp_fits_column){"Y_TYP_1", EXTFITS_ELEMENT_INT.typestr itostr(nband), "", "", "y value types for polarization 1", (char*[]){""}}
///x values for polarization 1
#define FITSIDI_GAIN_CURVE_COLUMN_X_VAL_1(nband) (dsp_fits_column){"X_VAL_1", EXTFITS_ELEMENT_FLOAT.typestr itostr(nband), "", "", "x values for polarization 1", (char*[]){""}}
///y values for polarization 1
#define FITSIDI_GAIN_CURVE_COLUMN_Y_VAL_1(ntab, nband) (dsp_fits_column){"Y_VAL_1", EXTFITS_ELEMENT_FLOAT.typestr itostr(ntab) "," itostr(nband), "", "", "y values for polarization 1", (char*[]){""}}
///Relative gain values for polarization 1
#define FITSIDI_GAIN_CURVE_COLUMN_GAIN_1(ntab, nband) (dsp_fits_column){"GAIN_1", EXTFITS_ELEMENT_FLOAT.typestr itostr(ntab) "," itostr(nband), "", "", "Relative gain values for polarization 1", (char*[]){""}}
///K/Jy Sensitivities for polarization 1
#define FITSIDI_GAIN_CURVE_COLUMN_SENS_1(nband) (dsp_fits_column){"SENS_1", EXTFITS_ELEMENT_FLOAT.typestr itostr(nband), "K/Jy", "", " Sensitivities for polarization 1", (char*[]){""}}
///Gain curve types for polarization 2
#define FITSIDI_GAIN_CURVE_COLUMN_TYPE_2(nband) (dsp_fits_column){"TYPE_2", EXTFITS_ELEMENT_INT.typestr itostr(nband), "", "", "Gain curve types for polarization 2", (char*[]){""}}
///Number of terms or entries for polarization 2
#define FITSIDI_GAIN_CURVE_COLUMN_NTERM_2(nband) (dsp_fits_column){"NTERM_2", EXTFITS_ELEMENT_INT.typestr itostr(nband), "", "", "Number of terms or entries for polarization 2", (char*[]){""}}
///x value types for polarization 2
#define FITSIDI_GAIN_CURVE_COLUMN_X_TYP_2(nband) (dsp_fits_column){"X_TYP_2", EXTFITS_ELEMENT_INT.typestr itostr(nband), "", "", "x value types for polarization 2", (char*[]){""}}
///y value types for polarization 2
#define FITSIDI_GAIN_CURVE_COLUMN_Y_TYP_2(nband) (dsp_fits_column){"Y_TYP_2", EXTFITS_ELEMENT_INT.typestr itostr(nband), "", "", "y value types for polarization 2", (char*[]){""}}
///x values for polarization 2
#define FITSIDI_GAIN_CURVE_COLUMN_X_VAL_2(nband) (dsp_fits_column){"X_VAL_2", EXTFITS_ELEMENT_FLOAT.typestr itostr(nband), "", "", "x values for polarization 2", (char*[]){""}}
///y values for polarization 2
#define FITSIDI_GAIN_CURVE_COLUMN_Y_VAL_2(ntab, nband) (dsp_fits_column){"Y_VAL_2", EXTFITS_ELEMENT_FLOAT.typestr itostr(ntab) "," itostr(nband), "", "", "y values for polarization 2", (char*[]){""}}
///Relative gain values for polarization 2
#define FITSIDI_GAIN_CURVE_COLUMN_GAIN_2(ntab, nband) (dsp_fits_column){"GAIN_2", EXTFITS_ELEMENT_FLOAT.typestr itostr(ntab) "," itostr(nband), "", "", "Relative gain values for polarization 2", (char*[]){""}}
///K/Jy Sensitivities for polarization 2
#define FITSIDI_GAIN_CURVE_COLUMN_SENS_2(nband) (dsp_fits_column){"SENS_2", EXTFITS_ELEMENT_FLOAT.typestr itostr(nband), "K/Jy", "", " Sensitivities for polarization 2", (char*[]){""}}

///Mandatory keywords for the FITS-IDI GAIN_CURVE table
///1
#define FITSIDI_GAIN_CURVE_KEYWORD_TABREV (dsp_fits_keyword){"TABREV", EXTFITS_ELEMENT_SHORT.typestr, "", "", "1", (char*[]){""}}
///Number of polarizations in the table
#define FITSIDI_GAIN_CURVE_KEYWORD_NO_POL (dsp_fits_keyword){"NO_POL", EXTFITS_ELEMENT_SHORT.typestr, "", "", "Number of polarizations in the table", (char*[]){""}}
///Number of tabulated values ntab
#define FITSIDI_GAIN_CURVE_KEYWORD_NO_TABS (dsp_fits_keyword){"NO_TABS", EXTFITS_ELEMENT_SHORT.typestr, "", "", "Number of tabulated values ntab", (char*[]){""}}

///Columns for the FITS-IDI PHASE-CAL table
///days Central time of interval
#define FITSIDI_PHASE_CAL_COLUMN_TIME (dsp_fits_column){"TIME", "1D", EXTFITS_MEASURE_UNIT_DAY, "", "Central time of interval", (char*[]){""}}
///days Duration of interval
#define FITSIDI_PHASE_CAL_COLUMN_TIME_INTERVAL (dsp_fits_column){"TIME_INTERVAL", "1E", EXTFITS_MEASURE_UNIT_DAY, "", "Duration of interval", (char*[]){""}}
///Source ID number
#define FITSIDI_PHASE_CAL_COLUMN_SOURCE_ID (dsp_fits_column){"SOURCE_ID", "1J", "", "", "Source ID number", (char*[]){""}}
///Antenna number
#define FITSIDI_PHASE_CAL_COLUMN_ANTENNA_NO (dsp_fits_column){"ANTENNA_NO", "1J", "", "", "Antenna number", (char*[]){""}}
///Array number
#define FITSIDI_PHASE_CAL_COLUMN_ARRAY (dsp_fits_column){"ARRAY", "1J", "", "", "Array number", (char*[]){""}}
///Frequency setup number
#define FITSIDI_PHASE_CAL_COLUMN_FREQID (dsp_fits_column){"FREQID", "1J", "", "", "Frequency setup number", (char*[]){""}}
///seconds Cable calibration measurement
#define FITSIDI_PHASE_CAL_COLUMN_CABLE_CAL (dsp_fits_column){"CABLE_CAL", "1D", EXTFITS_MEASURE_UNIT_SECOND, "", "Cable calibration measurement", (char*[]){""}}
///percent State counts for polarization 1
#define FITSIDI_PHASE_CAL_COLUMN_STATE_1(nband) (dsp_fits_column){"STATE_1", "E4,", nband), EXTFITS_MEASURE_UNIT_PERCENT, "", "State counts for polarization 1", (char*[]){""}}
///Hz Phase-cal tone frequencies for polarization 1
#define FITSIDI_PHASE_CAL_COLUMN_PC_FREQ_1(ntone, nband) (dsp_fits_column){"PC_FREQ_1", EXTFITS_ELEMENT_DOUBLE.typestr itostr(ntone) "," itostr(nband), EXTFITS_MEASURE_UNIT_HZ, "", "Phase-cal tone frequencies for polarization 1", (char*[]){""}}
///Real parts of phase-cal measurements for polarization 1
#define FITSIDI_PHASE_CAL_COLUMN_PC_REAL_1(ntone, nband) (dsp_fits_column){"PC_REAL_1", EXTFITS_ELEMENT_FLOAT.typestr itostr(ntone) "," itostr(nband), "", "", "Real parts of phase-cal measurements for polarization 1", (char*[]){""}}
///Imaginary parts of phase-cal measurements for polarization 1
#define FITSIDI_PHASE_CAL_COLUMN_PC_IMAG_1(ntone, nband) (dsp_fits_column){"PC_IMAG_1", EXTFITS_ELEMENT_FLOAT.typestr itostr(ntone) "," itostr(nband), "", "", "Imaginary parts of phase-cal measurements for polarization 1", (char*[]){""}}
///sec/sec Phase-cal rates for polarization 1
#define FITSIDI_PHASE_CAL_COLUMN_PC_RATE_1(ntone, nband) (dsp_fits_column){"PC_RATE_1", EXTFITS_ELEMENT_FLOAT.typestr itostr(ntone) "," itostr(nband), "sec/sec", "", "Phase-cal rates for polarization 1", (char*[]){""}}
///percent State counts for polarization 2
#define FITSIDI_PHASE_CAL_COLUMN_STATE_2(nband) (dsp_fits_column){"STATE_2", "E4,", nband), EXTFITS_MEASURE_UNIT_PERCENT, "", "State counts for polarization 2", (char*[]){""}}
///Hz Phase-cal tone frequencies for polarization 2
#define FITSIDI_PHASE_CAL_COLUMN_PC_FREQ_2(ntone, nband) (dsp_fits_column){"PC_FREQ_2", EXTFITS_ELEMENT_DOUBLE.typestr itostr(ntone) "," itostr(nband), EXTFITS_MEASURE_UNIT_HZ, "", "Phase-cal tone frequencies for polarization 2", (char*[]){""}}
///Real parts of phase-cal measurements for polarization 2
#define FITSIDI_PHASE_CAL_COLUMN_PC_REAL_2(ntone, nband) (dsp_fits_column){"PC_REAL_2", EXTFITS_ELEMENT_FLOAT.typestr itostr(ntone) "," itostr(nband), "", "", "Real parts of phase-cal measurements for polarization 2", (char*[]){""}}
///Imaginary parts of phase-cal measurements for polarization 2
#define FITSIDI_PHASE_CAL_COLUMN_PC_IMAG_2(ntone, nband) (dsp_fits_column){"PC_IMAG_2", EXTFITS_ELEMENT_FLOAT.typestr itostr(ntone) "," itostr(nband), "", "", "Imaginary parts of phase-cal measurements for polarization 2", (char*[]){""}}
///sec/sec Phase-cal rates for polarization 2
#define FITSIDI_PHASE_CAL_COLUMN_PC_RATE_2(ntone, nband) (dsp_fits_column){"PC_RATE_2", EXTFITS_ELEMENT_FLOAT.typestr itostr(ntone) "," itostr(nband), "sec/sec", "", "Phase-cal rates for polarization 2", (char*[]){""}}

///Mandatory keywords for the FITS-IDI PHASE-CAL table
///2
#define FITSIDI_PHASE_CAL_KEYWORD_TABREV (dsp_fits_keyword){"TABREV", EXTFITS_ELEMENT_SHORT.typestr, "", "", "2", (char*[]){""}}
///Number of polarizations in the table
#define FITSIDI_PHASE_CAL_KEYWORD_NO_POL (dsp_fits_keyword){"NO_POL", EXTFITS_ELEMENT_SHORT.typestr, "", "", "Number of polarizations in the table", (char*[]){""}}
///Number of tones ntone
#define FITSIDI_PHASE_CAL_KEYWORD_NO_TABS (dsp_fits_keyword){"NO_TABS", EXTFITS_ELEMENT_SHORT.typestr, "", "", "Number of tones ntone", (char*[]){""}}

///Recommended SEVERITY codes
///No severity level assigned
#define severity_No_severity_level_assigned "-1"
///Data are known to be useless
#define severity_Data_are_known_to_be_useless "0"
///Data are probably useless
#define severity_Data_are_probably_useless "1"
///Data may be useless
#define severity_Data_may_be_useless "2"

///Columns for the FITS-IDI FLAG table
///Source ID number
#define FITSIDI_FLAG_COLUMN_SOURCE_ID (dsp_fits_column){"SOURCE_ID", "1J", "", "", "Source ID number", (char*[]){""}}
///Array number
#define FITSIDI_FLAG_COLUMN_ARRAY (dsp_fits_column){"ARRAY", "1J", "", "", "Array number", (char*[]){""}}
///Antenna numbers
#define FITSIDI_FLAG_COLUMN_ANTS (dsp_fits_column){"ANTS", "2J", "", "", "Antenna numbers", (char*[]){""}}
///Frequency setup number
#define FITSIDI_FLAG_COLUMN_FREQID (dsp_fits_column){"FREQID", "1J", "", "", "Frequency setup number", (char*[]){""}}
///days Time range
#define FITSIDI_FLAG_COLUMN_TIMERANG (dsp_fits_column){"TIMERANG", "2E", EXTFITS_MEASURE_UNIT_DAY, "", "Time range", (char*[]){""}}
///Band flags
#define FITSIDI_FLAG_COLUMN_BANDS(nband) (dsp_fits_column){"BANDS", EXTFITS_ELEMENT_INT.typestr itostr(nband), "", "", "Band flags", (char*[]){""}}
///Channel range
#define FITSIDI_FLAG_COLUMN_CHANS (dsp_fits_column){"CHANS", "2J", "", "", "Channel range", (char*[]){""}}
///Polarization flags
#define FITSIDI_FLAG_COLUMN_PFLAGS (dsp_fits_column){"PFLAGS", "4J", "", "", "Polarization flags", (char*[]){""}}
///Reason for flag
#define FITSIDI_FLAG_COLUMN_REASON(n) (dsp_fits_column){"REASON" itostr(n), EXTFITS_ELEMENT_STRING.typestr, "", "", "Reason for flag", (char*[]){""}}
///Severity code
#define FITSIDI_FLAG_COLUMN_SEVERITY (dsp_fits_column){"SEVERITY", "1J", "", "", "Severity code", (char*[]){""}}

///Mandatory keywords for the FITS-IDI FLAG table
///2
#define FITSIDI_FLAG_KEYWORD_TABREV (dsp_fits_keyword){"TABREV", EXTFITS_ELEMENT_SHORT.typestr, "", "", "2", (char*[]){""}}

///Columns for the FITS-IDI WEATHER table
///days Central time of interval
#define FITSIDI_WEATHER_COLUMN_TIME (dsp_fits_column){"TIME", "1D", EXTFITS_MEASURE_UNIT_DAY, "", "Central time of interval", (char*[]){""}}
///days Duration of interval
#define FITSIDI_WEATHER_COLUMN_TIME_INTERVAL (dsp_fits_column){"TIME_INTERVAL", "1E", EXTFITS_MEASURE_UNIT_DAY, "", "Duration of interval", (char*[]){""}}
///Antenna number
#define FITSIDI_WEATHER_COLUMN_ANTENNA_NO (dsp_fits_column){"ANTENNA_NO", "1J", "", "", "Antenna number", (char*[]){""}}
///Centigrade Surface air temperature
#define FITSIDI_WEATHER_COLUMN_TEMPERATURE (dsp_fits_column){"TEMPERATURE", "1E", "", "", "Centigrade Surface air temperature", (char*[]){""}}
///millibar Surface air pressure
#define FITSIDI_WEATHER_COLUMN_PRESSURE (dsp_fits_column){"PRESSURE", "1E", "millibar", "", "Surface air pressure", (char*[]){""}}
///Centigrade Dewpoint temperature
#define FITSIDI_WEATHER_COLUMN_DEWPOINT (dsp_fits_column){"DEWPOINT", "1E", "", "", "Centigrade Dewpoint temperature", (char*[]){""}}
///m s−1 Wind velocity
#define FITSIDI_WEATHER_COLUMN_WIND_VELOCITY (dsp_fits_column){"WIND_VELOCITY", "1E", "m s−1", "", " Wind velocity", (char*[]){""}}
///degrees Wind direction East from North
#define FITSIDI_WEATHER_COLUMN_WIND_DIRECTION (dsp_fits_column){"WIND_DIRECTION", "1E", EXTFITS_MEASURE_UNIT_DEGREE, "", "Wind direction East from North", (char*[]){""}}
///m−2 Water column
#define FITSIDI_WEATHER_COLUMN_WVR_H2O (dsp_fits_column){"WVR_H2O", "1E", "m−2", "", "Water column", (char*[]){""}}
///m−2 Electron column
#define FITSIDI_WEATHER_COLUMN_IONOS_ELECTRON (dsp_fits_column){"IONOS_ELECTRON", "1E", "m−2", "", "Electron column", (char*[]){""}}

///Mandatory keywords for the FITS-IDI WEATHER table
///2
#define FITSIDI_WEATHER_KEYWORD_TABREV (dsp_fits_keyword){"TABREV", EXTFITS_ELEMENT_SHORT.typestr, "", "", "2", (char*[]){""}}
///Reference date
#define FITSIDI_WEATHER_KEYWORD_RDATE (dsp_fits_keyword){"RDATE", EXTFITS_ELEMENT_DOUBLE.typestr, "", "", "Reference date", (char*[]){""}}

///Columns for the FITS-IDI BASELINE table
///days Central time of interval
#define FITSIDI_BASELINE_COLUMN_TIME (dsp_fits_column){"TIME", "1D", EXTFITS_MEASURE_UNIT_DAY, "", "Central time of interval", (char*[]){""}}
///Source ID number
#define FITSIDI_BASELINE_COLUMN_SOURCE_ID (dsp_fits_column){"SOURCE_ID", "1J", "", "", "Source ID number", (char*[]){""}}
///Array number
#define FITSIDI_BASELINE_COLUMN_ARRAY (dsp_fits_column){"ARRAY", "1J", "", "", "Array number", (char*[]){""}}
///Antenna numbers forming baseline
#define FITSIDI_BASELINE_COLUMN_ANTENNA_NOS (dsp_fits_column){"ANTENNA_NOS.", "2J", "", "", "Antenna numbers forming baseline", (char*[]){""}}
///Frequency setup number
#define FITSIDI_BASELINE_COLUMN_FREQID (dsp_fits_column){"FREQID", "1J", "", "", "Frequency setup number", (char*[]){""}}
///Real part of multiplicative correction
#define FITSIDI_BASELINE_COLUMN_REAL_M(nstokes, nband) (dsp_fits_column){"REAL_M", EXTFITS_ELEMENT_FLOAT.typestr itostr(nstokes) "," itostr(nband), "", "", "Real part of multiplicative correction", (char*[]){""}}
///Imaginary part of multiplicative correction
#define FITSIDI_BASELINE_COLUMN_IMAG_M(nstokes, nband) (dsp_fits_column){"IMAG_M", EXTFITS_ELEMENT_FLOAT.typestr itostr(nstokes) "," itostr(nband), "", "", "Imaginary part of multiplicative correction", (char*[]){""}}
///Real part of additive correction
#define FITSIDI_BASELINE_COLUMN_REAL_A(nstokes, nband) (dsp_fits_column){"REAL_A", EXTFITS_ELEMENT_FLOAT.typestr itostr(nstokes) "," itostr(nband), "", "", "Real part of additive correction", (char*[]){""}}
///Imaginary part of additive correction
#define FITSIDI_BASELINE_COLUMN_IMAG_A(nstokes, nband) (dsp_fits_column){"IMAG_A", EXTFITS_ELEMENT_FLOAT.typestr itostr(nstokes) "," itostr(nband), "", "", "Imaginary part of additive correction", (char*[]){""}}

///Mandatory keywords for the FITS-IDI BASELINE table
///1
#define FITSIDI_BASELINE_KEYWORD_TABREV (dsp_fits_keyword){"TABREV", EXTFITS_ELEMENT_SHORT.typestr, "", "", "1", (char*[]){""}}
///Maximum antenna number in the table
#define FITSIDI_BASELINE_KEYWORD_NO_ANT (dsp_fits_keyword){"NO_ANT", EXTFITS_ELEMENT_SHORT.typestr, "", "", "Maximum antenna number in the table", (char*[]){""}}

///Columns for the FITS-IDI BANDPASS table
///days Central time of interval
#define FITSIDI_BANDPASS_COLUMN_TIME (dsp_fits_column){"TIME", "1D", EXTFITS_MEASURE_UNIT_DAY, "", "Central time of interval", (char*[]){""}}
///days Duration of interval
#define FITSIDI_BANDPASS_COLUMN_TIME_INTERVAL (dsp_fits_column){"TIME_INTERVAL", "1E", EXTFITS_MEASURE_UNIT_DAY, "", "Duration of interval", (char*[]){""}}
///Source ID number
#define FITSIDI_BANDPASS_COLUMN_SOURCE_ID (dsp_fits_column){"SOURCE_ID", "1J", "", "", "Source ID number", (char*[]){""}}
///Antenna number
#define FITSIDI_BANDPASS_COLUMN_ANTENNA_NO (dsp_fits_column){"ANTENNA_NO", "1J", "", "", "Antenna number", (char*[]){""}}
///Array number
#define FITSIDI_BANDPASS_COLUMN_ARRAY (dsp_fits_column){"ARRAY", "1J", "", "", "Array number", (char*[]){""}}
///Frequency setup number
#define FITSIDI_BANDPASS_COLUMN_FREQID (dsp_fits_column){"FREQID", "1J", "", "", "Frequency setup number", (char*[]){""}}
///Hz Channel bandwidth
#define FITSIDI_BANDPASS_COLUMN_BANDWIDTH (dsp_fits_column){"BANDWIDTH", "1E", EXTFITS_MEASURE_UNIT_HZ, "", "Channel bandwidth", (char*[]){""}}
///Hz Frequency of each band
#define FITSIDI_BANDPASS_COLUMN_BAND_FREQ(nband) (dsp_fits_column){"BAND_FREQ", EXTFITS_ELEMENT_DOUBLE.typestr itostr(nband), EXTFITS_MEASURE_UNIT_HZ, "", "Frequency of each band", (char*[]){""}}
///Reference antenna for polarization 1
#define FITSIDI_BANDPASS_COLUMN_REFANT_1 (dsp_fits_column){"REFANT_1", "1J", "", "", "Reference antenna for polarization 1", (char*[]){""}}
///Real part of bandpass correction for polarization 1
#define FITSIDI_BANDPASS_COLUMN_BREAL_1(nbach, nband) (dsp_fits_column){"BREAL_1", EXTFITS_ELEMENT_FLOAT.typestr itostr(nbach) "," itostr(nband), "", "", "Real part of bandpass correction for polarization 1", (char*[]){""}}
///Imaginary part of bandpass correction for polarization 1
#define FITSIDI_BANDPASS_COLUMN_BIMAG_1(nbach, nband) (dsp_fits_column){"BIMAG_1", EXTFITS_ELEMENT_FLOAT.typestr itostr(nbach) "," itostr(nband), "", "", "Imaginary part of bandpass correction for polarization 1", (char*[]){""}}
///Reference antenna for polarization 2
#define FITSIDI_BANDPASS_COLUMN_REFANT_2 (dsp_fits_column){"REFANT_2", "1J", "", "", "Reference antenna for polarization 2", (char*[]){""}}
///Real part of bandpass correction for polarization 2
#define FITSIDI_BANDPASS_COLUMN_BREAL_2(nbach, nband) (dsp_fits_column){"BREAL_2", EXTFITS_ELEMENT_FLOAT.typestr itostr(nbach) "," itostr(nband), "", "", "Real part of bandpass correction for polarization 2", (char*[]){""}}
///Imaginary part of bandpass correction for polarization 2
#define FITSIDI_BANDPASS_COLUMN_BIMAG_2(nbach, nband) (dsp_fits_column){"BIMAG_2", EXTFITS_ELEMENT_FLOAT.typestr itostr(nbach) "," itostr(nband), "", "", "Imaginary part of bandpass correction for polarization 2", (char*[]){""}}

///Mandatory keywords for the FITS-IDI BANDPASS table
///1
#define FITSIDI_BANDPASS_KEYWORD_TABREV (dsp_fits_keyword){"TABREV", EXTFITS_ELEMENT_SHORT.typestr, "", "", "1", (char*[]){""}}
///Maximum antenna number in the table
#define FITSIDI_BANDPASS_KEYWORD_NO_ANT (dsp_fits_keyword){"NO_ANT", EXTFITS_ELEMENT_SHORT.typestr, "", "", "Maximum antenna number in the table", (char*[]){""}}
///Number of polarizations in the table
#define FITSIDI_BANDPASS_KEYWORD_NO_POL (dsp_fits_keyword){"NO_POL", EXTFITS_ELEMENT_SHORT.typestr, "", "", "Number of polarizations in the table", (char*[]){""}}
///Number of spectral channels in the table
#define FITSIDI_BANDPASS_KEYWORD_NO_BACH (dsp_fits_keyword){"NO_BACH", EXTFITS_ELEMENT_SHORT.typestr, "", "", "Number of spectral channels in the table", (char*[]){""}}
///Data channel number for first channel in the table
#define FITSIDI_BANDPASS_KEYWORD_STRT_CHN (dsp_fits_keyword){"STRT_CHN", EXTFITS_ELEMENT_SHORT.typestr, "", "", "Data channel number for first channel in the table", (char*[]){""}}

///Columns for the FITS-IDI CALIBRATION table
///days Central time of interval
#define FITSIDI_CALIBRATION_COLUMN_TIME (dsp_fits_column){"TIME", "1D", EXTFITS_MEASURE_UNIT_DAY, "", "Central time of interval", (char*[]){""}}
///days Duration of interval
#define FITSIDI_CALIBRATION_COLUMN_TIME_INTERVAL (dsp_fits_column){"TIME_INTERVAL", "1E", EXTFITS_MEASURE_UNIT_DAY, "", "Duration of interval", (char*[]){""}}
///Source ID number
#define FITSIDI_CALIBRATION_COLUMN_SOURCE_ID (dsp_fits_column){"SOURCE_ID", "1J", "", "", "Source ID number", (char*[]){""}}
///Antenna number
#define FITSIDI_CALIBRATION_COLUMN_ANTENNA_NO (dsp_fits_column){"ANTENNA_NO", "1J", "", "", "Antenna number", (char*[]){""}}
///Array number
#define FITSIDI_CALIBRATION_COLUMN_ARRAY (dsp_fits_column){"ARRAY", "1J", "", "", "Array number", (char*[]){""}}
///Frequency setup number
#define FITSIDI_CALIBRATION_COLUMN_FREQID (dsp_fits_column){"FREQID", "1J", "", "", "Frequency setup number", (char*[]){""}}
///Kelvin System temperature for polarization 1
#define FITSIDI_CALIBRATION_COLUMN_TSYS_1(nband) (dsp_fits_column){"TSYS_1", EXTFITS_ELEMENT_FLOAT.typestr itostr(nband), EXTFITS_MEASURE_UNIT_KELVIN, "", "System temperature for polarization 1", (char*[]){""}}
///Kelvin Antenna temperature for polarization 1
#define FITSIDI_CALIBRATION_COLUMN_TANT_1(nband) (dsp_fits_column){"TANT_1", EXTFITS_ELEMENT_FLOAT.typestr itostr(nband), EXTFITS_MEASURE_UNIT_KELVIN, "", "Antenna temperature for polarization 1", (char*[]){""}}
///Kelvin/Jy Sensitivity at polarization 1
#define FITSIDI_CALIBRATION_COLUMN_SENSITIVITY_1(nband) (dsp_fits_column){"SENSITIVITY_1", EXTFITS_ELEMENT_FLOAT.typestr itostr(nband), "Kelvin/Jy", "", "Sensitivity at polarization 1", (char*[]){""}}
///radians Phase at polarization 1
#define FITSIDI_CALIBRATION_COLUMN_PHASE_1(nband) (dsp_fits_column){"PHASE_1", EXTFITS_ELEMENT_FLOAT.typestr itostr(nband), EXTFITS_MEASURE_UNIT_RAD, "", "Phase at polarization 1", (char*[]){""}}
///sec/sec Rate of change of delay of polarization 1
#define FITSIDI_CALIBRATION_COLUMN_RATE_1(nband) (dsp_fits_column){"RATE_1", EXTFITS_ELEMENT_FLOAT.typestr itostr(nband), "sec/sec", "", "Rate of change of delay of polarization 1", (char*[]){""}}
///seconds Delay of polarization 1
#define FITSIDI_CALIBRATION_COLUMN_DELAY_1(nband) (dsp_fits_column){"DELAY_1", EXTFITS_ELEMENT_FLOAT.typestr itostr(nband), EXTFITS_MEASURE_UNIT_SECOND, "", "Delay of polarization 1", (char*[]){""}}
///Complex gain real part for polarization 1
#define FITSIDI_CALIBRATION_COLUMN_REAL_1(nband) (dsp_fits_column){"REAL_1", EXTFITS_ELEMENT_FLOAT.typestr itostr(nband), "", "", "Complex gain real part for polarization 1", (char*[]){""}}
///Complex gain imaginary part for polarization 1
#define FITSIDI_CALIBRATION_COLUMN_IMAG_1(nband) (dsp_fits_column){"IMAG_1", EXTFITS_ELEMENT_FLOAT.typestr itostr(nband), "", "", "Complex gain imaginary part for polarization 1", (char*[]){""}}
///Reliability weight of complex gain for polarization 1
#define FITSIDI_CALIBRATION_COLUMN_WEIGHT_1(nband) (dsp_fits_column){"WEIGHT_1", EXTFITS_ELEMENT_FLOAT.typestr itostr(nband), "", "", "Reliability weight of complex gain for polarization 1", (char*[]){""}}
///Reference antenna for polarization 1
#define FITSIDI_CALIBRATION_COLUMN_REFANT_1(nband) (dsp_fits_column){"REFANT_1", EXTFITS_ELEMENT_INT.typestr itostr(nband), "", "", "Reference antenna for polarization 1", (char*[]){""}}
///Kelvin System temperature for polarization 2
#define FITSIDI_CALIBRATION_COLUMN_TSYS_2(nband) (dsp_fits_column){"TSYS_2", EXTFITS_ELEMENT_FLOAT.typestr itostr(nband), EXTFITS_MEASURE_UNIT_KELVIN, "", "System temperature for polarization 2", (char*[]){""}}
///Kelvin Antenna temperature for polarization 2
#define FITSIDI_CALIBRATION_COLUMN_TANT_2(nband) (dsp_fits_column){"TANT_2", EXTFITS_ELEMENT_FLOAT.typestr itostr(nband), EXTFITS_MEASURE_UNIT_KELVIN, "", "Antenna temperature for polarization 2", (char*[]){""}}
///Kelvin/Jy Sensitivity at polarization 2
#define FITSIDI_CALIBRATION_COLUMN_SENSITIVITY_2(nband) (dsp_fits_column){"SENSITIVITY_2", EXTFITS_ELEMENT_FLOAT.typestr itostr(nband), "Kelvin/Jy", "", "Sensitivity at polarization 2", (char*[]){""}}
///radians Phase at polarization 2
#define FITSIDI_CALIBRATION_COLUMN_PHASE_2(nband) (dsp_fits_column){"PHASE_2", EXTFITS_ELEMENT_FLOAT.typestr itostr(nband), EXTFITS_MEASURE_UNIT_RAD, "", "Phase at polarization 2", (char*[]){""}}
///sec/sec Rate of change of delay of polarization 2
#define FITSIDI_CALIBRATION_COLUMN_RATE_2(nband) (dsp_fits_column){"RATE_2", EXTFITS_ELEMENT_FLOAT.typestr itostr(nband), "sec/sec", "", "Rate of change of delay of polarization 2", (char*[]){""}}
///seconds Delay of polarization 2
#define FITSIDI_CALIBRATION_COLUMN_DELAY_2(nband) (dsp_fits_column){"DELAY_2", EXTFITS_ELEMENT_FLOAT.typestr itostr(nband), EXTFITS_MEASURE_UNIT_SECOND, "", "Delay of polarization 2", (char*[]){""}}
///Complex gain real part for polarization 2
#define FITSIDI_CALIBRATION_COLUMN_REAL_2(nband) (dsp_fits_column){"REAL_2", EXTFITS_ELEMENT_FLOAT.typestr itostr(nband), "", "", "Complex gain real part for polarization 2", (char*[]){""}}
///Complex gain imaginary part for polarization 2
#define FITSIDI_CALIBRATION_COLUMN_IMAG_2(nband) (dsp_fits_column){"IMAG_2", EXTFITS_ELEMENT_FLOAT.typestr itostr(nband), "", "", "Complex gain imaginary part for polarization 2", (char*[]){""}}
///Reliability weight of complex gain for polarization 2
#define FITSIDI_CALIBRATION_COLUMN_WEIGHT_2(nband) (dsp_fits_column){"WEIGHT_2", EXTFITS_ELEMENT_FLOAT.typestr itostr(nband), "", "", "Reliability weight of complex gain for polarization 2", (char*[]){""}}
///Reference antenna for polarization 2
#define FITSIDI_CALIBRATION_COLUMN_REFANT_2(nband) (dsp_fits_column){"REFANT_2", EXTFITS_ELEMENT_INT.typestr itostr(nband), "", "", "Reference antenna for polarization 2", (char*[]){""}}

///Mandatory keywords for the FITS-IDI CALIBRATION table
///1
#define FITSIDI_CALIBRATION_KEYWORD_TABREV (dsp_fits_keyword){"TABREV", EXTFITS_ELEMENT_SHORT.typestr, "", "", "1", (char*[]){""}}
///Maximum antenna number in the table
#define FITSIDI_CALIBRATION_KEYWORD_NO_ANT (dsp_fits_keyword){"NO_ANT", EXTFITS_ELEMENT_SHORT.typestr, "", "", "Maximum antenna number in the table", (char*[]){""}}
///Number of polarizations in the table
#define FITSIDI_CALIBRATION_KEYWORD_NO_POL (dsp_fits_keyword){"NO_POL", EXTFITS_ELEMENT_SHORT.typestr, "", "", "Number of polarizations in the table", (char*[]){""}}

///Columns for the FITS-IDI MODEL_COMPS table
///days Central time of interval
#define FITSIDI_MODEL_COMPS_COLUMN_TIME (dsp_fits_column){"TIME", "1D", EXTFITS_MEASURE_UNIT_DAY, "", "Central time of interval", (char*[]){""}}
///Source ID number
#define FITSIDI_MODEL_COMPS_COLUMN_SOURCE_ID (dsp_fits_column){"SOURCE_ID", "1J", "", "", "Source ID number", (char*[]){""}}
///Antenna number
#define FITSIDI_MODEL_COMPS_COLUMN_ANTENNA_NO (dsp_fits_column){"ANTENNA_NO", "1J", "", "", "Antenna number", (char*[]){""}}
///Array number
#define FITSIDI_MODEL_COMPS_COLUMN_ARRAY (dsp_fits_column){"ARRAY", "1J", "", "", "Array number", (char*[]){""}}
///Frequency setup number
#define FITSIDI_MODEL_COMPS_COLUMN_FREQID (dsp_fits_column){"FREQID", "1J", "", "", "Frequency setup number", (char*[]){""}}
///sec Atmospheric delay
#define FITSIDI_MODEL_COMPS_COLUMN_ATMOS (dsp_fits_column){"ATMOS", "1D", EXTFITS_MEASURE_UNIT_SECOND, "", "Atmospheric delay", (char*[]){""}}
///sec/sec Time derivative of atmospheric delay
#define FITSIDI_MODEL_COMPS_COLUMN_DATMOS (dsp_fits_column){"DATMOS", "1D", "sec/sec", "", "Time derivative of atmospheric delay", (char*[]){""}}
///sec Group delay
#define FITSIDI_MODEL_COMPS_COLUMN_GDELAY (dsp_fits_column){"GDELAY", "1D", EXTFITS_MEASURE_UNIT_SECOND, "", "Group delay", (char*[]){""}}
///sec/sec Rate of change of group delay
#define FITSIDI_MODEL_COMPS_COLUMN_GRATE (dsp_fits_column){"GRATE", "1D", "sec/sec", "", "Rate of change of group delay", (char*[]){""}}
///sec 'Clock' epoch error
#define FITSIDI_MODEL_COMPS_COLUMN_CLOCK_1 (dsp_fits_column){"CLOCK_1", "1D", EXTFITS_MEASURE_UNIT_SECOND, "", "'Clock' epoch error", (char*[]){""}}
///sec/sec Time derivative of clock error
#define FITSIDI_MODEL_COMPS_COLUMN_DCLOCK_1 (dsp_fits_column){"DCLOCK_1", "1D", "sec/sec", "", "Time derivative of clock error", (char*[]){""}}
///Hz LO offset
#define FITSIDI_MODEL_COMPS_COLUMN_LO_OFFSET_1(nband) (dsp_fits_column){"LO_OFFSET_1", EXTFITS_ELEMENT_FLOAT.typestr itostr(nband), EXTFITS_MEASURE_UNIT_HZ, "", "LO offset", (char*[]){""}}
///Hz/sec Time derivative of LO offset
#define FITSIDI_MODEL_COMPS_COLUMN_DLO_OFFSET_1(nband) (dsp_fits_column){"DLO_OFFSET_1", EXTFITS_ELEMENT_FLOAT.typestr itostr(nband), "Hz/sec", "", "Time derivative of LO offset", (char*[]){""}}
///sec m−2 Dispersive delay
#define FITSIDI_MODEL_COMPS_COLUMN_DISP_1 (dsp_fits_column){"DISP_1", "1E", EXTFITS_MEASURE_UNIT_SECOND, "", "Dispersive delay", (char*[]){""}}
///sec m−2/sec Time derivative of dispersive delay
#define FITSIDI_MODEL_COMPS_COLUMN_DDISP_1 (dsp_fits_column){"DDISP_1", "1E", "sec m−2/sec", "", " Time derivative of dispersive delay", (char*[]){""}}
///sec 'Clock' epoch error
#define FITSIDI_MODEL_COMPS_COLUMN_CLOCK_2 (dsp_fits_column){"CLOCK_2", "1D", EXTFITS_MEASURE_UNIT_SECOND, "", "'Clock' epoch error", (char*[]){""}}
///sec/sec Time derivative of clock error
#define FITSIDI_MODEL_COMPS_COLUMN_DCLOCK_2 (dsp_fits_column){"DCLOCK_2", "1D", "sec/sec", "", "Time derivative of clock error", (char*[]){""}}
///Hz LO offset
#define FITSIDI_MODEL_COMPS_COLUMN_LO_OFFSET_2(nband) (dsp_fits_column){"LO_OFFSET_2", EXTFITS_ELEMENT_FLOAT.typestr itostr(nband), EXTFITS_MEASURE_UNIT_HZ, "", "LO offset", (char*[]){""}}
///Hz/sec Time derivative of LO offset
#define FITSIDI_MODEL_COMPS_COLUMN_DLO_OFFSET_2(nband) (dsp_fits_column){"DLO_OFFSET_2", EXTFITS_ELEMENT_FLOAT.typestr itostr(nband), "Hz/sec", "", "Time derivative of LO offset", (char*[]){""}}
///sec m−2 Dispersive delay
#define FITSIDI_MODEL_COMPS_COLUMN_DISP_2 (dsp_fits_column){"DISP_2", "1E", EXTFITS_MEASURE_UNIT_SECOND, "", "Dispersive delay", (char*[]){""}}
///sec m−2/sec Time derivative of dispersive delay
#define FITSIDI_MODEL_COMPS_COLUMN_DDISP_2 (dsp_fits_column){"DDISP_2", "1E", "sec m−2/sec", "", "Time derivative of dispersive delay", (char*[]){""}}

///Mandatory keywords for the FITS-IDI MODEL_COMPS table
///1
#define FITSIDI_MODEL_COMPS_KEYWORD_TABREV (dsp_fits_keyword){"TABREV", EXTFITS_ELEMENT_SHORT.typestr, "", "", "1", (char*[]){""}}
///Reference date
#define FITSIDI_MODEL_COMPS_KEYWORD_RDATE (dsp_fits_keyword){"RDATE", EXTFITS_ELEMENT_DOUBLE.typestr, "", "", "Reference date", (char*[]){""}}
///Number of polarizations in the table
#define FITSIDI_MODEL_COMPS_KEYWORD_NO_POL (dsp_fits_keyword){"NO_POL", EXTFITS_ELEMENT_SHORT.typestr, "", "", "Number of polarizations in the table", (char*[]){""}}
///FFT size
#define FITSIDI_MODEL_COMPS_KEYWORD_FFT_SIZE (dsp_fits_keyword){"FFT_SIZE", EXTFITS_ELEMENT_SHORT.typestr, "", "", "FFT size", (char*[]){""}}
///Oversampling factor
#define FITSIDI_MODEL_COMPS_KEYWORD_OVERSAMP (dsp_fits_keyword){"OVERSAMP", EXTFITS_ELEMENT_SHORT.typestr, "", "", "Oversampling factor", (char*[]){""}}
///Zero padding factor
#define FITSIDI_MODEL_COMPS_KEYWORD_ZERO_PAD (dsp_fits_keyword){"ZERO_PAD", EXTFITS_ELEMENT_SHORT.typestr, "", "", "Zero padding factor", (char*[]){""}}
///Tapering function ('HANNING' or 'UNIFORM')
#define FITSIDI_MODEL_COMPS_KEYWORD_TAPER_FN (dsp_fits_keyword){"TAPER_FN", EXTFITS_ELEMENT_STRING.typestr, "", "", "Tapering function ('HANNING' or 'UNIFORM')", (char*[]){""}}

/**
* \brief read a fits file containing a FITS-IDI Extension
* \param filename The file name of the fits to read
* \param nstreams The number of streams of the data matrix passed by reference
* \param maxes The number of dimensions of the data matrix passed by reference
* \param maxis The sizes of the data matrix
* \return dsp_fits_row pointer describing the fits file content
*/
DLL_EXPORT dsp_fits_row *dsp_fits_read_fitsidi(char *filename, long *nstreams, long *maxes, long **maxis);
/**\}*/

#ifdef __cplusplus
}
#endif

#endif //_FITS_EXTENSION_FITSIDI_H
