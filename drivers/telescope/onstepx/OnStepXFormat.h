/*
    OnStepX INDI Driver — formatting and parsing helpers for the OnStepX protocol
    Copyright (C) 2026 Ed Lee

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA

    Header-only, no INDI dependencies.

    The firmware parsers (Convert::hmsToDouble / Convert::dmsToDouble) need
    fixed-width, zero-padded fields, and :Sd needs an explicit '+' or '-'.
    INDI's fs_sexa() pads with spaces and omits '+', so the firmware rejects
    RA < 10h and any Dec > -10 deg.  Use these helpers instead.

      :Sr  HH:MM:SS.SSSS   (PM_HIGHEST, 13 chars)
      :Sd  sDD:MM:SS.SSS   (PM_HIGHEST, 13 chars, sign mandatory)
*/

#pragma once

#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstdlib>

namespace OnStepXFormat
{

// Format RA in hours as "HH:MM:SS.SSSS". The value is wrapped into [0, 24).
// Returns false for non-finite input or if buf is too small.
inline bool formatRA(double hours, char *buf, std::size_t len)
{
    if (!std::isfinite(hours))
        return false;

    // Work in integer units of 0.0001 s so rounding carries correctly.
    constexpr long long UNITS_PER_SEC = 10000;
    constexpr long long UNITS_PER_DAY = 24LL * 3600LL * UNITS_PER_SEC;

    long long t = std::llround(hours * 3600.0 * UNITS_PER_SEC);
    t %= UNITS_PER_DAY;
    if (t < 0)
        t += UNITS_PER_DAY;

    const int frac = static_cast<int>(t % UNITS_PER_SEC);
    t /= UNITS_PER_SEC;
    const int s = static_cast<int>(t % 60);
    t /= 60;
    const int m = static_cast<int>(t % 60);
    const int h = static_cast<int>(t / 60);

    const int n = std::snprintf(buf, len, "%02d:%02d:%02d.%04d", h, m, s, frac);
    return n > 0 && static_cast<std::size_t>(n) < len;
}

// Format Dec in degrees as "sDD:MM:SS.SSS" with a mandatory sign.
// Returns false for non-finite or out-of-range (|dec| > 90) input, or if buf is too small.
inline bool formatDec(double degrees, char *buf, std::size_t len)
{
    if (!std::isfinite(degrees) || degrees < -90.0 || degrees > 90.0)
        return false;

    // Work in integer units of 0.001 arcsec so rounding carries correctly.
    constexpr long long UNITS_PER_SEC = 1000;

    const char sign = (degrees < 0.0) ? '-' : '+';
    long long t = std::llround(std::fabs(degrees) * 3600.0 * UNITS_PER_SEC);

    const int frac = static_cast<int>(t % UNITS_PER_SEC);
    t /= UNITS_PER_SEC;
    const int s = static_cast<int>(t % 60);
    t /= 60;
    const int m = static_cast<int>(t % 60);
    const int d = static_cast<int>(t / 60);

    const int n = std::snprintf(buf, len, "%c%02d:%02d:%02d.%03d", sign, d, m, s, frac);
    return n > 0 && static_cast<std::size_t>(n) < len;
}

// Parse a numeric firmware reply.  The firmware prints "nan" when a sensor
// value is unavailable (e.g. :GX9A# without a weather sensor) and strtod()
// accepts "nan"/"inf", so require the whole string to be a FINITE number.
// Leading/trailing spaces are allowed (sprintf("%3.1f") pads).
inline bool parseNumber(const char *s, double &out)
{
    if (!s)
        return false;
    char *end = nullptr;
    const double v = std::strtod(s, &end);
    if (end == s)
        return false;
    while (*end == ' ')
        end++;
    if (*end != '\0' || !std::isfinite(v))
        return false;
    out = v;
    return true;
}

} // namespace OnStepXFormat
