/**  INDI LIB
 *   FITS keyword class
 *   Copyright (C) 2023 Dusan Poizl
 *
 *   This library is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU Lesser General Public
 *   License as published by the Free Software Foundation; either
 *   version 2.1 of the License, or (at your option) any later version.
 *
 *   This library is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *   Lesser General Public License for more details.
 *
 *   You should have received a copy of the GNU Lesser General Public
 *   License along with this library; if not, write to the Free Software
 *   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#pragma once

#include <string>
#include <fitsio.h>
#include <cstdint>

namespace INDI
{

class FITSRecord
{
public:
    enum Type
    {
        VOID,
        COMMENT,
        STRING = TSTRING,
        LONGLONG = TLONGLONG,
        //ULONGLONG = TULONGLONG,
        DOUBLE = TDOUBLE
    };
    FITSRecord();
    FITSRecord(const char *key, const char *value, const char *comment = nullptr);
    FITSRecord(const char *key, int64_t value, const char *comment = nullptr);
    //FITSRecord(const char *key, uint64_t value, const char *comment = nullptr);
    FITSRecord(const char *key, double value, int decimal = 6, const char *comment = nullptr);
    explicit FITSRecord(const char *comment);
    Type type() const;
    const std::string& key() const;
    const std::string& valueString() const;
    int64_t valueInt() const;
    //uint64_t valueUInt() const;
    double valueDouble() const;
    const std::string& comment() const;
    int decimal() const;
private:
    union
    {
        int64_t val_int64;
        uint64_t val_uint64;
        double val_double;
    };
    std::string val_str;
    std::string m_key;
    Type m_type = VOID;
    std::string m_comment;
    int m_decimal = 6;
};

}
