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

constexpr char sep = '+';

starbook::DMS::DMS(string dms) : ln_dms{0, 0, 0, 0}
{
    static regex pattern(R"((-?)(\d+)\+(\d+))");
    smatch results;
    if (regex_search(dms, results, pattern))
    {
        neg = (unsigned short) (results[1].str().empty() ? 0 : 1);
        degrees = (unsigned short) stoi(results[2].str());
        minutes = (unsigned short) stoi(results[3].str());
        seconds = 0;
    }
    else
    {
        throw;
    }
}

ostream &starbook::operator<<(ostream &os, const starbook::DMS &obj)
{
    if (obj.neg != 0) os << "-";
    os << fixed << setprecision(0) << setfill('0')
       << setw(3) << obj.degrees
       << setw(0) << sep
       << setw(2) << obj.minutes
       << setw(0);
    return os;
}

ostream &starbook::operator<<(ostream &os, const starbook::HMS &obj)
{
    os << fixed << setprecision(0) << setfill('0')
       << setw(2) << obj.hours
       << setw(0) << sep
       << setw(2) << obj.minutes
       << setw(0) << "."
       << setw(1) << floor(obj.seconds / 6);
    return os;
}

std::istream &starbook::operator>>(std::istream &is, starbook::HMS &obj)
{
    unsigned short h, m, m_tenth;
    std::array<char, 2> ch = {{'\0'}};
    is >> h >> ch[0] >> m >> ch[1] >> m_tenth;

    if (!is) return is;
    if (ch[0] != sep || ch[1] != '.')
    {
        is.clear(ios_base::failbit);
        return is;
    }
    obj = HMS(h, m, static_cast<double>(m_tenth * 6));
    return is;
}

starbook::Equ::Equ(double ra, double dec) : lnh_equ_posn{{0, 0, 0},
    {0, 0, 0, 0}}
{
    ln_equ_posn target_d = {ra, dec};
    ln_equ_to_hequ(&target_d, this);
}

ostream &starbook::operator<<(ostream &os, const starbook::Equ &obj)
{
    os << "RA=";
    os << static_cast<const HMS &> (obj.ra);

    os << "&DEC=";
    os << static_cast<const DMS &> (obj.dec);
    return os;
}

ostream &starbook::operator<<(ostream &os, const starbook::DateTime &obj)
{
    os << setfill('0') << std::fixed << setprecision(0)
       << obj.years << sep
       << setw(2) << obj.months << sep
       << setw(2) << obj.days << sep
       << setw(2) << obj.hours << sep
       << setw(2) << obj.minutes << sep
       << setw(2) << floor(obj.seconds);
    return os;
}

std::istream &starbook::operator>>(std::istream &is, starbook::DateTime &utc)
{
    int Y, M, D, h, m, s;
    std::array<char, 5> ch = {{'\0'}};
    is >> Y >> ch[0] >> M >> ch[1] >> D >> ch[2] >> h >> ch[3] >> m >> ch[4] >> s;

    if (!is) return is;
    for (char i : ch)
        if (i != sep)
        {
            is.clear(ios_base::failbit);
            return is;
        }
    utc = DateTime(Y, M, D, h, m, static_cast<double>(s));
    return is;
}

std::ostream &starbook::operator<<(std::ostream &os, const starbook::LnLat &obj)
{
    lnh_lnlat_posn dms{{0, 0, 0, 0},
        {0, 0, 0, 0}};
    auto tmp = static_cast<ln_lnlat_posn>(obj);
    ln_lnlat_to_hlnlat(&tmp, &dms);

    os << setfill('0') << std::fixed << setprecision(0)
       << "longitude=" << ((dms.lng.neg == 0) ? "E" : "W")
       << setw(2) << dms.lng.degrees << sep << setw(2) << dms.lng.minutes
       << "latitude=" << ((dms.lat.neg == 0) ? "N" : "S")
       << setw(2) << dms.lat.degrees << sep << setw(2) << dms.lat.minutes;
    return os;
}

starbook::CommandResponse::CommandResponse(const std::string &url_like) : status(OK), raw(url_like)
{
    if (url_like.empty()) throw runtime_error("parsing error, no payload");
    if (url_like.rfind("OK", 0) == 0)
    {
        status = OK;
        return;
    }
    else if (url_like.rfind("ERROR", 0) == 0)
    {
        if (url_like.rfind("ERROR:FORMAT", 0) == 0)
            status = ERROR_FORMAT;
        else if (url_like.rfind("ERROR:ILLEGAL STATE", 0) == 0)
            status = ERROR_ILLEGAL_STATE;
        else if (url_like.rfind("ERROR:BELOW HORIZONE", 0) == 0) /* it's not a typo */
            status = ERROR_BELOW_HORIZON;
        else
            status = ERROR_UNKNOWN;
        return;
    }
    else
    {
        std::string str_remaining = url_like;
        std::regex param_re(R"((\w+)=(\-?[\w\+\.]+))");
        std::smatch sm;

        while (regex_search(str_remaining, sm, param_re))
        {
            std::string key = sm[1].str();
            std::string value = sm[2].str();

            // JM 2017-07-17: Should we make all uppercase to get around different version incompatibilities?
            std::transform(key.begin(), key.end(), key.begin(), ::toupper);
            std::transform(value.begin(), value.end(), value.begin(), ::toupper);

            payload[key] = value;
            str_remaining = sm.suffix();
        }

        if (payload.empty())
            throw std::runtime_error("parsing error, could not parse any field");
        if (!str_remaining.empty())
            throw std::runtime_error("parsing error, could not parse full payload");
        status = OK;
    }
}
