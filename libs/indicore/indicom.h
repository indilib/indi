/**  INDI LIB
 *   Common routines used by all drivers
 *   Copyright (C) 2003 by Jason Harris (jharris@30doradus.org)
 *                         Elwood C. Downey
 *                         Jasem Mutlaq
 *
 *   This library is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU Lesser General Public
 *   License as published by the Free Software Foundation; either
 *   version 2.1 of the License, or (at your option) any later version.
 *
 *   This library is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *   Lesser General Public License for more details.
 *
 *   You should have received a copy of the GNU Lesser General Public
 *   License along with this library; if not, write to the Free Software
 *   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

/** \file indicom.h
 *  \brief Implementations for common driver routines.
 *
 *  The INDI Common Routine Library provides formatting and serial routines employed by many INDI drivers. Currently, the library is composed of the following sections:
 *
 *  <ul>
 *  <li>Formatting Functions</li>
 *  <li>Conversion Functions</li>
 *  <li>TTY Functions</li>
 *  </ul>
 *
 *  \author Jason Harris
 *  \author Elwood C. Downey
 *  \author Jasem Mutlaq
 */

#pragma once

#if defined(_MSC_VER)
#define _USE_MATH_DEFINES
#endif

#include <math.h>

#ifndef J2000
#define J2000       2451545.0
#endif
#define ERRMSG_SIZE 1024

#ifndef STELLAR_DAY
#define STELLAR_DAY        86164.098903691
#endif
#ifndef TRACKRATE_SIDEREAL
#define TRACKRATE_SIDEREAL ((360.0 * 3600.0) / STELLAR_DAY)
#endif
#ifndef SOLAR_DAY
#define SOLAR_DAY          86400
#endif
#ifndef TRACKRATE_SOLAR
#define TRACKRATE_SOLAR    ((360.0 * 3600.0) / SOLAR_DAY)
#endif
#ifndef TRACKRATE_LUNAR
#define TRACKRATE_LUNAR    14.511415
#endif
#ifndef EARTHRADIUSEQUATORIAL
#define EARTHRADIUSEQUATORIAL 6378137.0
#endif
#ifndef EARTHRADIUSPOLAR
#define EARTHRADIUSPOLAR 6356752.0
#endif
#ifndef EARTHRADIUSMEAN
#define EARTHRADIUSMEAN 6372797.0
#endif
#ifndef SUNMASS
#define SUNMASS 1.98847E+30
#endif
#ifndef PLANK_H
#define PLANK_H 6.62607015E-34
#endif
#ifndef DIRAC_H
#define DIRAC_H (PLANK_H/(2*M_PI))
#endif
#ifndef EINSTEIN_G
#define EINSTEIN_G 6.67408E-11
#endif
#ifndef EULER
#define EULER 2.71828182845904523536028747135266249775724709369995
#endif
#ifndef ROOT2
#define ROOT2 1.41421356237309504880168872420969807856967187537694
#endif
#ifndef AIRY
#define AIRY 1.21966
#endif
#ifndef CIRCLE_DEG
#define CIRCLE_DEG 360
#endif
#ifndef CIRCLE_AM
#define CIRCLE_AM (CIRCLE_DEG * 60)
#endif
#ifndef CIRCLE_AS
#define CIRCLE_AS (CIRCLE_AM * 60)
#endif
#ifndef RAD_AS
#define RAD_AS (CIRCLE_AS/(M_PI*2))
#endif
#ifndef ASTRONOMICALUNIT
#define ASTRONOMICALUNIT 1.495978707E+11
#endif
#ifndef PARSEC
#define PARSEC (ASTRONOMICALUNIT*RAD_AS)
#endif
#ifndef LIGHTSPEED
#define LIGHTSPEED 299792458.0
#endif
#ifndef JULIAN_LY
#define JULIAN_LY (LIGHTSPEED * SOLAR_DAY * 365)
#endif
#ifndef STELLAR_LY
#define STELLAR_LY (LIGHTSPEED * STELLAR_DAY * 365)
#endif
#ifndef FLUX
#define FLUX(wavelength) (wavelength/(PLANK_H*LIGHTSPEED))
#endif
#ifndef CANDLE
#define CANDLE ((1.0/683.0)*FLUX(555))
#endif
#ifndef LUMEN
#define LUMEN(wavelength) (CANDLE/(4*M_PI)*pow((FLUX(wavelength)/FLUX(555)), 0.25))
#endif
#ifndef REDSHIFT
#define REDSHIFT(wavelength, reference) (1.0-(reference/wavelength))
#endif
#ifndef DOPPLER
#define DOPPLER(shift, speed) (speed*shift)
#endif

extern const char *Direction[];
extern const char *SolarSystem[];

struct ln_date;
struct ln_equ_posn;
struct ln_lnlat_posn;
struct ln_hrz_posn;

/* TTY Error Codes */
enum TTY_ERROR
{
    TTY_OK           = 0,
    TTY_READ_ERROR   = -1,
    TTY_WRITE_ERROR  = -2,
    TTY_SELECT_ERROR = -3,
    TTY_TIME_OUT     = -4,
    TTY_PORT_FAILURE = -5,
    TTY_PARAM_ERROR  = -6,
    TTY_ERRNO        = -7,
    TTY_OVERFLOW     = -8,
    TTY_PORT_BUSY    = -9,
};

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \defgroup ttyFunctions TTY Functions: Functions to perform common terminal access routines.
 */

/* @{ */

/** \brief read buffer from terminal
 *  \param fd file descriptor
 *  \param buf pointer to store data. Must be initialized and big enough to hold data.
 *  \param nbytes number of bytes to read.
 *  \param timeout number of seconds to wait for terminal before a timeout error is issued.
 *  \param nbytes_read the number of bytes read.
 *  \return On success, it returns TTY_OK, otherwise, a TTY_ERROR code.
 */
int tty_read(int fd, char *buf, int nbytes, int timeout, int *nbytes_read);

/** \brief read buffer from terminal with a delimiter
 *  \param fd file descriptor
 *  \param buf pointer to store data. Must be initialized and big enough to hold data.
 *  \param stop_char if the function encounters \e stop_char then it stops reading and returns the buffer.
 *  \param timeout_seconds number of seconds to wait for terminal before a timeout error is issued.
 *
 *  (Total time = timeout_seconds + timeout_microseconds)
 *  \param timeout_microseconds number of microseconds to wait for terminal before a timeout error is issued.
 *
 *  (Total time = timeout_seconds + timeout_microseconds)
 *  \param nbytes_read the number of bytes read.
 *  \return On success, it returns TTY_OK, otherwise, a TTY_ERROR code.
 */
int tty_read_expanded(int fd, char *buf, int nbytes, long timeout_seconds, long timeout_microseconds, int *nbytes_read);

/** \brief read buffer from terminal with a delimiter
 *  \param fd file descriptor
 *  \param buf pointer to store data. Must be initialized and big enough to hold data.
 *  \param stop_char if the function encounters \e stop_char then it stops reading and returns the buffer.
 *  \param timeout number of seconds to wait for terminal before a timeout error is issued.
 *  \param nbytes_read the number of bytes read.
 *  \return On success, it returns TTY_OK, otherwise, a TTY_ERROR code.
 */
int tty_read_section(int fd, char *buf, char stop_char, int timeout, int *nbytes_read);

/** \brief read buffer from terminal with a delimiter
 *  \param fd file descriptor
 *  \param buf pointer to store data. Must be initialized and big enough to hold data.
 *  \param stop_char if the function encounters \e stop_char then it stops reading and returns the buffer.
 *  \param nsize size of buf. If stop character is not encountered before nsize, the function aborts.
 *  \param timeout_seconds number of seconds to wait for terminal before a timeout error is issued.
 *
 *  (Total Timeout is timeout_seconds + timeout_microseconds)
 *  \param timeout_microseconds number of microseconds to wait for terminal before a timeout error is issued.
 *
 *  (Total Timeout  is timeout_seconds + timeout_microseconds)
 *  \param nbytes_read the number of bytes read.
 *  \return On success, it returns TTY_OK, otherwise, a TTY_ERROR code.
 */
int tty_read_section_expanded(int fd, char *buf, char stop_char, long timeout_seconds, long timeout_microseconds,
                              int *nbytes_read);

/** \brief read buffer from terminal with a delimiter
 *  \param fd file descriptor
 *  \param buf pointer to store data. Must be initialized and big enough to hold data.
 *  \param stop_char if the function encounters \e stop_char then it stops reading and returns the buffer.
 *  \param nsize size of buf. If stop character is not encountered before nsize, the function aborts.
 *  \param timeout number of seconds to wait for terminal before a timeout error is issued.
 *  \param nbytes_read the number of bytes read.
 *  \return On success, it returns TTY_OK, otherwise, a TTY_ERROR code.
 */
int tty_nread_section(int fd, char *buf, int nsize, char stop_char, int timeout, int *nbytes_read);

/** \brief read buffer from terminal with a delimiter
 *  \param fd file descriptor
 *  \param buf pointer to store data. Must be initialized and big enough to hold data.
 *  \param stop_char if the function encounters \e stop_char then it stops reading and returns the buffer.
 *  \param nsize size of buf. If stop character is not encountered before nsize, the function aborts.
 *  \param timeout number of seconds to wait for terminal before a timeout error is issued.
 *
 *  (Total Timeout is timeout_seconds + timeout_microseconds)
 *  \param timeout_microseconds number of microseconds to wait for terminal before a timeout error is issued.
 *
 *  \param nbytes_read the number of bytes read.
 *  \return On success, it returns TTY_OK, otherwise, a TTY_ERROR code.
 */
int tty_nread_section_expanded(int fd, char *buf, int nsize, char stop_char, long timeout_seconds, long timeout_microseconds,
                               int *nbytes_read);

/** \brief Writes a buffer to fd.
 *  \param fd file descriptor
 *  \param buffer a null-terminated buffer to write to fd.
 *  \param nbytes number of bytes to write from \e buffer
 *  \param nbytes_written the number of bytes written
 *  \return On success, it returns TTY_OK, otherwise, a TTY_ERROR code.
 */
int tty_write(int fd, const char *buffer, int nbytes, int *nbytes_written);

/** \brief Writes a null terminated string to fd.
 *  \param fd file descriptor
 *  \param buffer the buffer to write to fd.
 *  \param nbytes_written the number of bytes written
 *  \return On success, it returns TTY_OK, otherwise, a TTY_ERROR code.
 */
int tty_write_string(int fd, const char *buffer, int *nbytes_written);

/** \brief Establishes a tty connection to a terminal device.
 *  \param device the device node. e.g. /dev/ttyS0
 *  \param bit_rate bit rate
 *  \param word_size number of data bits, 7 or 8, USE 8 DATA BITS with modbus
 *  \param parity 0=no parity, 1=parity EVEN, 2=parity ODD
 *  \param stop_bits number of stop bits : 1 or 2
 *  \param fd \e fd is set to the file descriptor value on success.
 *  \return On success, it returns TTY_OK, otherwise, a TTY_ERROR code.
 *  \author Wildi Markus
 */
int tty_connect(const char *device, int bit_rate, int word_size, int parity, int stop_bits, int *fd);

/** \brief Closes a tty connection and flushes the bus.
 *  \param fd the file descriptor to close.
 *  \return On success, it returns TTY_OK, otherwise, a TTY_ERROR code.
 */
int tty_disconnect(int fd);

/** \brief Retrieve the tty error message
 *  \param err_code the error code return by any TTY function.
 *  \param err_msg an initialized buffer to hold the error message.
 *  \param err_msg_len length in bytes of \e err_msg
 */
void tty_error_msg(int err_code, char *err_msg, int err_msg_len);

/** \brief tty_set_debug Enable or disable debug which prints verbose information.
 *  \param debug 1 to enable, 0 to disable
 */
void tty_set_debug(int debug);
void tty_set_gemini_udp_format(int enabled);
void tty_set_generic_udp_format(int enabled);
void tty_clr_trailing_read_lf(int enabled);

int tty_timeout(int fd, int timeout);

int tty_timeout_microseconds(int fd, long timeout_seconds, long timeout_microseconds);


/** \brief create new UDP session to prevent packet conflict
 *
 * Create a new UDP connection socket (and thus new session) and replace existing descriptor with it (using dup2)
 *
 * Note: it is effective if and only if \a tty_set_generic_udp_format is enabled
 * 
 * \param fd file descriptor of UDP socket
 * \param only_if_timeout_happened of 0 create always, if 1 do it if there was IO timout on the socket in past, resets this flag
 */
void tty_reset_udp_session(int fd,int only_if_timeout_happened);

/** \brief automatically reset UDP session on write
  *
  * Allows reset UDP session automatically if error/timeout or on every IO to prevent packet misshandling
  * in protocols that do not mark request/response relation by design
  *
  * \param flag 0 - disable, 1 - reset only if read timeout happened, 2 - every write regardless of timeout
  */
void tty_set_auto_reset_udp_session(int flag);


/* @} */

/**
 * \defgroup convertFunctions Formatting Functions: Functions to perform handy formatting and conversion routines.
 */

/* @{ */

/** \brief Converts a sexagesimal number to a string.
 *  sprint the variable a in sexagesimal format into out[].
 *  \param out a pointer to store the sexagesimal number.
 *  \param a the sexagesimal number to convert.
 *  \param w the number of spaces in the whole part.
 *  \param fracbase is the number of pieces a whole is to broken into; valid options:\n
 *      \li 360000: \<w\>:mm:ss.ss
 *      \li 36000:  \<w\>:mm:ss.s
 *      \li 3600:   \<w\>:mm:ss
 *      \li 600:    \<w\>:mm.m
 *      \li 60:     \<w\>:mm
 *  \return number of characters written to out, not counting final null terminator.
 */
int fs_sexa(char *out, double a, int w, int fracbase);

/** \brief convert sexagesimal string str AxBxC to double.
 *  x can be anything non-numeric. Any missing A, B or C will be assumed 0. Optional - and + can be anywhere.
 *  \param str0 string containing sexagesimal number.
 *  \param dp pointer to a double to store the sexagesimal number.
 *  \return return 0 if ok, -1 if can't find a thing.
 */
int f_scansexa(const char *str0, double *dp);

/** \brief Extract ISO 8601 time and store it in a tm struct.
 *  \param timestr a string containing date and time in ISO 8601 format.
 *  \param iso_date a pointer to a \e ln_date structure to store the extracted time and date (libnova).
 *  \return 0 on success, -1 on failure.
 */
#ifndef _WIN32
int extractISOTime(const char *timestr, struct ln_date *iso_date);
#endif

void getSexComponents(double value, int *d, int *m, int *s);
void getSexComponentsIID(double value, int *d, int *m, double *s);

/** \brief Fill buffer with properly formatted INumber string.
 *  \param buf to store the formatted string.
 *  \param format format in sprintf style.
 *  \param value the number to format.
 *  \return length of string.
 *  \note buf must be of length MAXINDIFORMAT at minimum
 */
int numberFormat(char *buf, const char *format, double value);

/** \brief Get a unix timestamp with nanosecond precision
 *  \return Seconds since UNIX Epoch.
 */
double time_ns();

/** \brief Create an ISO 8601 formatted time stamp. The format is YYYY-MM-DDTHH:MM:SS
 *  \return The formatted time stamp.
 */
const char *indi_timestamp();

/** \brief rangeHA Limits the hour angle value to be between -12 ---> 12
 *  \param r current hour angle value
 *  \return Limited value (-12,12)
 */
double rangeHA(double r);

/** \brief range24 Limits a number to be between 0-24 range.
 *  \param r number to be limited
 *  \return Limited number
 */
double range24(double r);

/** \brief range360 Limits an angle to be between 0-360 degrees.
 *  \param r angle
 *  \return Limited angle
 */
double range360(double r);

/**
 * \brief range180 Limits an angle to be between -180 to +180 degrees
 * \param r angle in degrees
 * \return Limited angle
 */
double range180(double r);

/** \brief rangeDec Limits declination value to be in -90 to 90 range.
 *  \param r declination angle
 *  \return limited declination
 */
double rangeDec(double r);

/** \brief get_local_sidereal_time Returns local sidereal time given longitude and system clock.
 *  \param longitude Longitude in INDI format (0 to 360) increasing eastward.
 *  \return Local Sidereal Time.
 */
double get_local_sidereal_time(double longitude);

/** \brief get_local_hour_angle Returns local hour angle of an object
 *  \param local_sidereal_time Local Sidereal Time
 *  \param ra RA of object
 *  \return Hour angle in hours (-12 to 12)
 */
double get_local_hour_angle(double local_sidereal_time, double ra);


/** \brief get_hrz_from_equ Calculate horizontal coordinates from equatorial coordinates.
 *  \param object Equatorial Object Coordinates
 *  \param observer Observer Location
 *  \param JD Julian Date
 *  \param position Calculated Horizontal Coordinates.
 *  \note Use this instead of libnova ln_get_hrz_from_equ since it corrects libnova Azimuth (0 = North and not South).
 */
void get_hrz_from_equ(struct ln_equ_posn *object, struct ln_lnlat_posn *observer, double JD, struct ln_hrz_posn *position);

/** \brief ln_get_equ_from_hrz Calculate Equatorial EOD Coordinates from horizontal coordinates
 *  \param object Horizontal Object Coordinates
 *  \param observer Observer Location
 *  \param JD Julian Date
 *  \param position Calculated Equatorial Coordinates.
 *  \note Use this instead of libnova ln_get_equ_from_hrz since it corrects libnova Azimuth (0 = North and not South).
 */
void get_equ_from_hrz(struct ln_hrz_posn *object, struct ln_lnlat_posn *observer, double JD,
                      struct ln_equ_posn *position);
/** \brief get_alt_az_coordinates Returns alt-azimuth coordinates of an object
 *  \param hour_angle Hour angle in hours (-12 to 12)
 *  \param dec DEC of object
 *  \param latitude latitude in INDI format (-90 to +90)
 *  \param alt ALT of object will be returned here
 *  \param az AZ of object will be returned here
 */
void get_alt_az_coordinates(double hour_angle, double dec, double latitude, double* alt, double *az);

/** \brief estimate_geocentric_elevation Returns an estimation of the actual geocentric elevation
 *  \param latitude latitude in INDI format (-90 to +90)
 *  \param sea_level_elevation sea level elevation
 *  \return Approximated geocentric elevation
 */
double estimate_geocentric_elevation(double latitude, double sea_level_elevation);

/** \brief estimate_field_rotation_rate Returns an estimation of the field rotation rate of the object
 *  \param Alt altitude coordinate of the object
 *  \param Az azimuth coordinate of the object
 *  \param latitude latitude in INDI format (-90 to +90)
 *  \return Approximation of the field rotation rate
 */
double estimate_field_rotation_rate(double Alt, double Az, double latitude);

/** \brief estimate_field_rotation Returns an estimation of the field rotation rate of the object
 *  \param hour_angle Hour angle in hours (-12 to 12)
 *  \param field_rotation_rate the field rotation rate
 *  \return Approximation of the absolute field rotation
 */
double estimate_field_rotation(double hour_angle, double field_rotation_rate);

/** \brief as2rad Convert arcseconds into radians
 *  \param as the arcseconds to convert
 *  \return radians corresponding as angle value
 */
double as2rad(double as);

/** \brief rad2as Convert radians into arcseconds
 *  \param as the radians to convert
 *  \return arcseconds corresponding as angle value
 */
double rad2as(double rad);

/** \brief estimate_distance Convert parallax arcseconds into meters
 *  \param parsec the parallax arcseconds to convert
 *  \return Estimation of the distance in measure units used in parallax_radius
 */
double estimate_distance(double parsecs, double parallax_radius);

/** @brief m2au Convert meters into astronomical units
 *  @param m the distance in meters to convert
 *  @return Estimation of the distance in astronomical units
 */
double m2au(double m);

/** \brief calc_delta_magnitude Returns the difference of magnitudes given two spectra
 *  \param mag_ratio Reference magnitude
 *  \param spectrum The spectrum of the star under exam
 *  \param ref_spectrum The spectrum of the reference star
 *  \param spectrum_size The size of the spectrum array in elements
 *  \return the magnitude difference
 */
double calc_delta_magnitude(double mag_ratio, double *spectrum, double *ref_spectrum, int spectrum_size);

/** \brief calc_photon_flux Returns the photon flux of the object with the given magnitude observed at a determined wavelength using a passband filter through a steradian expressed cone
 *  \param rel_magnitude Relative magnitude of the object observed
 *  \param filter_bandwidth Filter bandwidth in meters
 *  \param wavelength Wavelength in meters
 *  \param steradian The light cone in steradians
 *  \return the photon flux in Lumen
 */
double calc_photon_flux(double rel_magnitude, double filter_bandwidth, double wavelength, double steradian);

/** \brief calc_rel_magnitude Returns the relative magnitude of the object with the given photon flux measured at a determined wavelength using a passband filter over an incident surface
 *  \param photon_flux The photon flux in Lumen
 *  \param filter_bandwidth Filter bandwidth in meters
 *  \param wavelength Wavelength in meters
 *  \param incident_surface The incident surface in square meters
 *  \return the relative magnitude of the object observed
 */
double calc_rel_magnitude(double photon_flux, double filter_bandwidth, double wavelength, double steradian);

/** \brief estimate_absolute_magnitude Returns an estimation of the absolute magnitude of an object given its distance and the difference of its magnitude with a reference object
 *  \param dist The distance in parallax radiuses
 *  \param delta_mag The difference of magnitudes
 *  \return Approximation of the absolute magnitude in Δmag
 */
double estimate_absolute_magnitude(double dist, double delta_mag);

/** \brief estimate the star mass in ref_size units e.g. sun masses or kgs
 *  \param delta_mag The absolute magnitude ratio between the reference object used as unit in ref_size.
 *  \param ref_mass The mass of the reference object used to calculate the magnitude difference.
 */
double estimate_star_mass(double delta_mag, double ref_mass);

/** \brief estimate the orbit radius of an object with known mass orbiting around a star.
 *  \param obs_lambda The observed wavelength of a spectral line observed on the star affected by redshift or blueshift.
 *  \param ref_lambda The reference wavelength of the spectral line observed on earth or the nullshift spectral line position.
 *  \param period The orbital period.
 */
double estimate_orbit_radius(double obs_lambda, double ref_lambda, double period);

/** \brief estimate the mass of an object with known mass orbiting around a star.
 *  \param star_mass The mass of the star hosting an orbiting object.
 *  \param star_drift The star lagrange point L1 (observed drift of the star).
 *  \param orbit_radius The estimated orbit radius of the companion object (star, planet, cloud).
 */
double estimate_secondary_mass(double star_mass, double star_drift, double orbit_radius);

/** \brief estimate the size of an object occulting a star in star_size units.
 *  \param star_size The size of the occulted star.
 *  \param dropoff_ratio The light curve dropoff during the transit. Linear scale. 0.0=no dropoff 1.0=totally occulted.
 */
double estimate_secondary_size(double star_size, double dropoff_ratio);

/** \brief baseline_2d_projection Returns the coordinates of the projection of a single baseline targeting the object by coordinates
 *  \param alt current altitude of the target.
 *  \param az azimuth position of the target.
 *  \param baseline the baseline in meters. Three-dimensional xyz north is z axis y is UTC0 x is UTC0+90°.
 *  \param wavelength The observing electromagnetic wavelength, the lower the size increases.
 *  \param uvresult result plane coordinates of the current projection given the baseline and target vector.
 */
void baseline_2d_projection(double alt, double az, double baseline[3], double wavelength, double uvresult[2]);

/** \brief baseline_delay Returns the delay in meters of a single baseline targeting the object by coordinates
 *  \param alt current altitude of the target.
 *  \param az azimuth position of the target.
 *  \param baseline the baseline in meters. Three-dimensional xyz north is z axis y is UTC0 x is UTC0+90°.
 *  \param wavelength The observing electromagnetic wavelength, the lower the size increases.
 *  \return double[2] UV plane coordinates of the current projection given the baseline and target vector.
 */
double baseline_delay(double alt, double az, double baseline[3]);

/* @} */

#ifdef __cplusplus
}
#endif
