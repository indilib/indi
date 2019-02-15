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

#include "dsp.h"

double estimate_distance(double parsec, double deltamag, int lambda_index, double* ref_specrum, int ref_len, double* spectrum, int len)
{
    double* r_spectrum = dsp_buffer_stretch(ref_specrum, ref_len, 0, 1.0);
    double ref = 1.0/r_spectrum[lambda_index];
    double* t_spectrum = dsp_buffer_stretch(spectrum, ref_len, 0, ref);
    deltamag *= dsp_buffer_compare(r_spectrum, ref_len, t_spectrum, len);
    return deltamag * parsec;
}
