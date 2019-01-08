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

dspau_t dspau_astro_ra2ha(dspau_t Ra, dspau_t Lst)
{
    return Lst - (Ra * 360.0 / 24.0);
}

void dspau_astro_hadec2altaz(dspau_t Ha, dspau_t Dec, dspau_t Lat, dspau_t* Alt, dspau_t *Az)
{
    dspau_t alt, az;
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

dspau_t dspau_astro_elevation(dspau_t Lat, dspau_t El)
{
    Lat *= PI / 180.0;
    Lat = sin(Lat);
    El += Lat * (EarthRadiusPolar - EarthRadiusEquatorial);
    return El;
}

dspau_t dspau_astro_field_rotation_rate(dspau_t Alt, dspau_t Az, dspau_t Lat)
{
    Alt *= PI / 180.0;
    Az *= PI / 180.0;
    Lat *= PI / 180.0;
    dspau_t ret = cos(Lat) * cos(Az) / cos(Alt);
    ret *= 180.0 / PI;
    return ret;
}

dspau_t dspau_astro_field_rotation(dspau_t HA, dspau_t rate)
{
    HA *= rate;
    while(HA >= 360.0)
        HA -= 360.0;
    while(HA < 0)
        HA += 360.0;
    return HA;
}

dspau_t dspau_astro_frtoas(dspau_t fr)
{
    return RAD_AS * acos(fr);
}
