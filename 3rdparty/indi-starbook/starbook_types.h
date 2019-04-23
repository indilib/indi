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
#include <map>
#include <array>

namespace starbook {
    struct DMS : ln_dms {
        explicit DMS(std::string dms);

        friend std::ostream &operator<<(std::ostream &os, const DMS &obj);

        friend std::istream &operator>>(std::istream &is, DMS &obj);
    };

    struct HMS : ln_hms {
        explicit HMS(unsigned short h = 0, unsigned short m = 0, double s = 0) : ln_hms{h, m, s} {}

        friend std::ostream &operator<<(std::ostream &os, const HMS &obj);

        friend std::istream &operator>>(std::istream &is, HMS &obj);
    };

    struct Equ : lnh_equ_posn {
        Equ(double ra, double dec);

        friend std::ostream &operator<<(std::ostream &os, const Equ &obj);
    };

    struct DateTime : ln_date {
        // I regret my life decisions
        DateTime(int years, int months, int days, int hours, int minutes, double seconds)
                : ln_date{years, months, days, hours, minutes, seconds} {};

        friend std::ostream &operator<<(std::ostream &os, const DateTime &obj);

        friend std::istream &operator>>(std::istream &is, DateTime &obj);

    };

    struct LnLat : ln_lnlat_posn {
        // I regret my life decisions
        LnLat(double ln, double lat)
                : ln_lnlat_posn{ln, lat} {};

        friend std::ostream &operator<<(std::ostream &os, const LnLat &obj);

        friend std::istream &operator>>(std::istream &is, LnLat &obj);
    };

    std::ostream &operator<<(std::ostream &os, const DMS &obj);

    std::istream &operator>>(std::istream &is, DMS &obj);

    std::ostream &operator<<(std::ostream &os, const HMS &obj);

    std::istream &operator>>(std::istream &is, HMS &obj);

    std::ostream &operator<<(std::ostream &os, const Equ &obj);

    std::ostream &operator<<(std::ostream &os, const DateTime &obj);

    std::istream &operator>>(std::istream &is, DateTime &obj);

    std::ostream &operator<<(std::ostream &os, const LnLat &obj);

    std::istream &operator>>(std::istream &is, LnLat &obj);

    enum StarbookState {
        INIT, /* Initial state after boot */
        GUIDE, /* ??? */
        SCOPE, /* After START command or user input, when user can move mount */
        CHART, /* When users explores internal sky map??? */
        USER, /* user dialog or something */
        ALTAZ, /* No idea, also found in code dump */
        UNKNOWN, /* We haven't got starbook yet */
    };

    static const std::map<StarbookState, std::string> STATE_TO_STR = {
            {INIT,    "INIT"},
            {GUIDE,   "GUIDE"},
            {SCOPE,   "SCOPE"},
            {CHART,   "CHART"},
            {USER,    "USER"},
            {ALTAZ,   "ALTAZ"},
            {UNKNOWN, "UNKNOWN"},
    };

    /// @brief possible response codes returned by starbook
    enum ResponseCode {
        OK = 0,
        ERROR_ILLEGAL_STATE, /* Starbook has wrong internal state to accept command */
        ERROR_FORMAT, /* who knows... */
        ERROR_BELOW_HORIZON, /* Starbook thinks that issued movement command will bring scope horizon */
        ERROR_POINT, /* Found in code dump, no idea */
        ERROR_UNKNOWN, /* no specified reason */
    };

    struct CommandResponse {
        explicit CommandResponse(const std::string &url_like);

        ResponseCode status;
        std::string raw;
        std::map<std::string, std::string> payload;
    };
}
