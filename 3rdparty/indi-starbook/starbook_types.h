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

namespace starbook {
    struct DMS : ln_dms {
        explicit DMS(std::string dms);

        friend std::ostream &operator<<(std::ostream &os, const DMS &dms);
    };

    struct HMS : ln_hms {
        explicit HMS(std::string hms);

        friend std::ostream &operator<<(std::ostream &os, const HMS &hms);
    };

    struct Equ : lnh_equ_posn {
        Equ(double ra, double dec);

        friend std::ostream &operator<<(std::ostream &os, const Equ &equ);
    };

    struct UTC : ln_date {
        // I regret my life decisions
        UTC(int years, int months, int days, int hours, int minutes, double seconds)
                : ln_date{years, months, days, hours, minutes, seconds} {};

        friend std::ostream &operator<<(std::ostream &os, const UTC &utc);
    };

    std::ostream &operator<<(std::ostream &os, const DMS &dms);

    std::ostream &operator<<(std::ostream &os, const HMS &hms);

    std::ostream &operator<<(std::ostream &os, const Equ &equ);

    std::ostream &operator<<(std::ostream &os, const UTC &utc);

    enum StarbookState {
        INIT, /* Initial state after boot */
        GUIDE, /* ??? */
        SCOPE, /* After START command or user input */
        UNKNOWN,
    };

    enum ResponseCode {
        OK,
        ERROR_ILLEGAL_STATE, /* Starbook has wrong internal state to accept command */
        ERROR_FORMAT, /* who knows... */
        ERROR_BELOW_HORIZON, /* Starbook thinks that issued movement command will bring scope horizon */
        ERROR_UNKNOWN, /* no specified reason */
    };
}
