//
// Created by not7cd on 13/12/18.
//

#include "starbook_types.h"
#include <regex>

starbook::DMS::DMS(std::string dms) : ln_dms{0, 0, 0, 0} {
    static std::regex pattern(R"((-?)(\d+)\+(\d+))");
    std::smatch results;
    if (std::regex_search(dms, results, pattern)) {
        neg = (unsigned short) (results[1].str().empty() ? 0 : 1);
        degrees = (unsigned short) std::stoi(results[2].str());
        minutes = (unsigned short) std::stoi(results[3].str());
        seconds = 0;
    } else {
        throw;
    }
}

std::ostream &starbook::operator<<(std::ostream &os, const starbook::DMS &dms) {
    if (dms.neg != 0) os << "-";
    os << std::fixed << std::setprecision(0) << std::setfill('0')
       << std::setw(3) << dms.degrees
       << std::setw(0) << "+"
       << std::setw(2) << dms.minutes
       << std::setw(0);
    return os;
}

starbook::HMS::HMS(std::string hms) : ln_hms{0, 0, 0} {
    static std::regex pattern(R"((\d+)\+(\d+)\.(\d+))");
    std::smatch results;
    if (std::regex_search(hms, results, pattern)) {
        hours = (unsigned short) std::stoi(results[1].str());
        minutes = (unsigned short) std::stoi(results[2].str());
        seconds = (double) std::stoi(results[3].str());
    } else {
        throw;
    }
}

std::ostream &starbook::operator<<(std::ostream &os, const starbook::HMS &hms) {
    os << std::fixed << std::setprecision(0) << std::setfill('0')
       << std::setw(2) << hms.hours
       << std::setw(0) << "+"
       << std::setw(2) << hms.minutes
       << std::setw(0) << "." << hms.seconds;
    return os;
}

starbook::Equ::Equ(double ra, double dec) : lnh_equ_posn{{0, 0, 0},
                                                         {0, 0, 0, 0}} {
    ln_equ_posn target_d = {ra, dec};
    ln_equ_to_hequ(&target_d, this);
}

std::ostream &starbook::operator<<(std::ostream &os, const starbook::Equ &equ) {
    os << "RA=";
    os << static_cast<const HMS &> (equ.ra);

    os << "&DEC=";
    os << static_cast<const DMS &> (equ.dec);
    return os;
}
