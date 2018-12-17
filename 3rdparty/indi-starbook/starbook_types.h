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

namespace starbook {
    struct DMS : ln_dms {
        explicit DMS(std::string dms);


    };

    struct HMS : ln_hms {
        explicit HMS(std::string hms);


    };

    struct Equ : lnh_equ_posn {
        Equ(double ra, double dec);

    };
}

std::ostream &operator<<(std::ostream &os, const starbook::DMS &hms);

std::ostream &operator<<(std::ostream &os, const starbook::HMS &hms);

std::ostream &operator<<(std::ostream &os, const starbook::Equ &equ);


enum StarbookState {
    SB_INIT, /* Initial state after boot */
    SB_GUIDE, /* ??? */
    SB_SCOPE, /* After START command or user input */
};

