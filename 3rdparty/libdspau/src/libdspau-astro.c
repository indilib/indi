/*
 *   libDSPAU - a digital signal processing library for astronomy usage
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

#include "libdspau.h"
#define ratio (max - min) / (mx - mn + 1)

double dspau_astro_ra2ha(double Ra, double Lst)
{
    return Lst - (Ra * 360.0 / 24.0);
}

void dspau_astro_hadec2altaz(double Ha, double Dec, double Lat, double* Alt, double *Az)
{
    double alt, az;
    Ha *= PI / 180.0;
    Dec *= PI / 180.0;
    Lat *= PI / 180.0;
    alt = asin(sin(Dec) * sin(Lat) + cos(Dec) * cos(Lat) * cos(Ha));
    az = acos((sin(Dec) - sin(alt)*sin(Lat)) / (cos(alt) * cos(Lat)));
    alt *= 180.0 / PI;
    az *= 180.0 / PI;
    if (sin(Ha) >= 0.0)
        az = 360 - az;
    *Alt = alt;
    *Az = az;
}

double dspau_astro_elevation(double Lat, double El)
{
    Lat *= PI / 180.0;
    Lat = sin(Lat);
    El += Lat * (EarthRadiusPolar - EarthRadiusEquatorial);
    return El;
}

double dspau_astro_field_rotation_rate(double Alt, double Az, double Lat)
{
    Alt *= PI / 180.0;
    Az *= PI / 180.0;
    Lat *= PI / 180.0;
    double ret = cos(Lat) * cos(Az) / cos(Alt);
    ret *= 180.0 / PI;
    return ret;
}

double dspau_astro_field_rotation(double HA, double rate)
{
    HA *= rate;
    while(HA >= 360.0)
        HA -= 360.0;
    while(HA < 0)
        HA += 360.0;
    return HA;
}

double dspau_astro_frtoas(double fr)
{
    return RAD_AS * acos(fr);
}

double dspau_astro_as2parsec(double as)
{
    return as * Parsec;
}

double dspau_astro_parsecmag2absmag(double parsec, double deltamag, int lambda_index, double* ref_specrum, int ref_len, double* spectrum, int len)
{
    double* r_spectrum = dspau_buffer_stretch(ref_specrum, ref_len, 0, 1.0);
    double ref = 1.0/r_spectrum[lambda_index];
    double* t_spectrum = dspau_buffer_stretch(spectrum, ref_len, 0, ref);
    deltamag *= dspau_buffer_compare(r_spectrum, ref_len, t_spectrum, len);
    return deltamag * parsec;
}
