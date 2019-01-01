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

#include "starbook_types.h"
#include <iomanip>
#include <regex>
#include <cmath>

using namespace std;

starbook::DMS::DMS(string dms) : ln_dms{0, 0, 0, 0} {
    static regex pattern(R"((-?)(\d+)\+(\d+))");
    smatch results;
    if (regex_search(dms, results, pattern)) {
        neg = (unsigned short) (results[1].str().empty() ? 0 : 1);
        degrees = (unsigned short) stoi(results[2].str());
        minutes = (unsigned short) stoi(results[3].str());
        seconds = 0;
    } else {
        throw;
    }
}

ostream &starbook::operator<<(ostream &os, const starbook::DMS &dms) {
    if (dms.neg != 0) os << "-";
    os << fixed << setprecision(0) << setfill('0')
       << setw(3) << dms.degrees
       << setw(0) << "+"
       << setw(2) << dms.minutes
       << setw(0);
    return os;
}

starbook::HMS::HMS(string hms) : ln_hms{0, 0, 0} {
    static regex pattern(R"((\d+)\+(\d+)\.(\d+))");
    smatch results;
    if (regex_search(hms, results, pattern)) {
        hours = (unsigned short) stoi(results[1].str());
        minutes = (unsigned short) stoi(results[2].str());
        seconds = (double) stoi(results[3].str());
    } else {
        throw;
    }
}

ostream &starbook::operator<<(ostream &os, const starbook::HMS &hms) {
    os << fixed << setprecision(0) << setfill('0')
       << setw(2) << hms.hours
       << setw(0) << "+"
       << setw(2) << hms.minutes
       << setw(0) << "." << floor(hms.seconds);
    return os;
}

starbook::Equ::Equ(double ra, double dec) : lnh_equ_posn{{0, 0, 0},
                                                         {0, 0, 0, 0}} {
    ln_equ_posn target_d = {ra, dec};
    ln_equ_to_hequ(&target_d, this);
}

ostream &starbook::operator<<(ostream &os, const starbook::Equ &equ) {
    os << "RA=";
    os << static_cast<const HMS &> (equ.ra);

    os << "&DEC=";
    os << static_cast<const DMS &> (equ.dec);
    return os;
}

ostream &starbook::operator<<(ostream &os, const starbook::UTC &utc) {
    os << setfill('0') << std::fixed << setprecision(0)
       << utc.years << "+"
       << setw(2) << utc.months << "+"
       << setw(2) << utc.days << "+"
       << setw(2) << utc.hours << "+"
       << setw(2) << utc.minutes << "+"
       << setw(2) << floor(utc.seconds);
    return os;
}
