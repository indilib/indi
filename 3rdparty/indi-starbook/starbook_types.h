/*
 Starbook mount driver

 Copyright (C) 2018 Norbert Szulc (not7cd)

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

 */

#pragma once

#include <libnova/utility.h>
#include <iostream>
#include <iomanip>

enum StarbookState {
    SB_INIT, /* Initial state after start */
    SB_GUIDE, /* ??? */
    SB_SCOPE, /* ??? */
};


class StarbookEqu {
public:
    StarbookEqu(double ra, double dec) {
        ln_equ_posn target_d = {0, 0};
        target_d.ra = ra * 15;
        target_d.dec = dec;
        equ_posn = {{0, 0, 0},
                    {0, 0, 0, 0}};
        ln_equ_to_hequ(&target_d, &equ_posn);
    }

    explicit StarbookEqu(lnh_equ_posn h_equ) {
        equ_posn = h_equ;
    }

    friend std::ostream &operator<<(std::ostream &os, const StarbookEqu &e) {
        os << std::fixed << std::setprecision(0);
        os << "RA=";
        os << std::setfill('0') << std::setw(2) << e.equ_posn.ra.hours
           << std::setw(0) << "+"
           << std::setw(2) << e.equ_posn.ra.minutes
           << std::setw(0) << "." << e.equ_posn.ra.seconds;

        os << "&DEC=";
        if (e.equ_posn.dec.neg != 0) os << "-";
        os << std::setfill('0') << std::setw(3) << e.equ_posn.dec.degrees
           << std::setw(0) << "+"
           << std::setw(2) << e.equ_posn.dec.minutes
           << std::setw(0);
        return os;
    }

//    friend std::istream &operator>>(std::istream &is, StarbookEqu &e) {
//        is >> "RA=";
//        is >> e.equ_posn.ra.hours << "+" << e.equ_posn.ra.minutes << "." << e.equ_posn.ra.seconds;
//
//        is >> "&DEC=";
//        if (e.equ_posn.dec.neg != 0) os << "-";
//        is >> e.equ_posn.dec.degrees >> "+" >> e.equ_posn.dec.minutes;
//        return is >> f.numo >> f.ch >> f.deno;
//    }


public:
    lnh_equ_posn equ_posn;

};
