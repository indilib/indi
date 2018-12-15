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

struct timespec dspau_time_mktimespec(int year, int month, int dom, int hour, int minute, int second, long nanosecond)
{
    struct timespec ret;
    struct tm t_tm;
    time_t t_time;
    t_tm.tm_sec = second;
    t_tm.tm_min = minute;
    t_tm.tm_hour = hour;
    t_tm.tm_mday = dom;
    t_tm.tm_mon = month - 1;
    t_tm.tm_year = year - 1900;
    t_time = mktime(&t_tm);
    ret.tv_sec = t_time + nanosecond / 1000000000;
    ret.tv_nsec = nanosecond % 1000000000;
    return ret;
}

dspau_t dspau_time_timespec_to_J2000time(struct timespec tp)
{
    struct tm j2000_tm;
    time_t j2000;
    j2000_tm.tm_sec = 0;
    j2000_tm.tm_min = 0;
    j2000_tm.tm_hour = 12;
    j2000_tm.tm_mday = 1;
    j2000_tm.tm_mon = 0;
    j2000_tm.tm_year = 100;
    j2000_tm.tm_wday = 6;
    j2000_tm.tm_yday = 0;
    j2000_tm.tm_isdst = 0;
    j2000 = mktime(&j2000_tm);
    return ((dspau_t)(tp.tv_sec - j2000) + (dspau_t)tp.tv_nsec / 1000000000.0);
}

dspau_t dspau_time_J2000time_to_lst(dspau_t secs_since_J2000, dspau_t Long)
{
    dspau_t Lst = GammaJ2000 + 24.0 * secs_since_J2000 / SIDEREAL_DAY;
    Lst *= 360.0 / 24.0;
    while (Lst < 0)
        Lst += 360.0;
    while (Lst >= 360.0)
        Lst -= 360.0;
    return Lst + Long;
}

struct timespec dspau_time_J2000time_to_timespec(dspau_t secs)
{
    struct timespec ret;
    struct tm j2000_tm;
    time_t j2000;
    j2000_tm.tm_sec = 0;
    j2000_tm.tm_min = 0;
    j2000_tm.tm_hour = 12;
    j2000_tm.tm_mday = 1;
    j2000_tm.tm_mon = 0;
    j2000_tm.tm_year = 100;
    j2000_tm.tm_wday = 6;
    j2000_tm.tm_yday = 0;
    j2000_tm.tm_isdst = 0;
    j2000 = mktime(&j2000_tm);
    ret.tv_sec = secs + j2000;
    ret.tv_nsec = ((long)(secs * 1000000000.0)) % 1000000000;
    return ret;
}

struct timespec dspau_time_YmdHMSn_to_timespec(int Y, int m, int d, int H, int M, int S, long n)
{
    struct timespec ret;
    struct tm _tm;
    time_t _time;
    _tm.tm_sec = S;
    _tm.tm_min = M;
    _tm.tm_hour = H;
    _tm.tm_mday = d;
    _tm.tm_mon = m - 1;
    _tm.tm_year = Y - 1900;
    _time = mktime(&_tm);
    ret.tv_sec = n / 1000000000 + _time;
    ret.tv_nsec = n % 1000000000;
    return ret;
}
