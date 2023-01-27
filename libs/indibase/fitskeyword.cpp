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

#include "fitskeyword.h"
#include <cmath>
#include <sstream>

namespace INDI
{

FITSRecord::FITSRecord() :
    val_int64(0),
    m_type(VOID)
{
}

FITSRecord::FITSRecord(const char *key, const char *value, const char *comment) :
    m_key(key),
    m_type(STRING)
{
    if (value)
        val_str = std::string(value);

    if(comment)
        m_comment = std::string(comment);
}

FITSRecord::FITSRecord(const char *key, int64_t value, const char *comment) :
    val_int64(value),
    val_str(std::to_string(value)),
    m_key(key),
    m_type(LONGLONG)
{
    if(comment)
        m_comment = std::string(comment);
}

/*FITSRecord::FITSRecord(const char *key, uint64_t value, const char *comment) :
    val_uint64(value),
    val_str(std::to_string(value)),
    m_key(key),
    m_type(ULONGLONG)
{
    if(comment)
        m_comment = std::string(comment);
}*/

FITSRecord::FITSRecord(const char *key, double value, int decimal, const char *comment) :
    val_double(value),
    m_key(key),
    m_type(DOUBLE),
    m_decimal(decimal)
{
    std::stringstream ss;
    ss.precision(decimal);
    ss << value;
    val_str = ss.str();

    if(comment)
        m_comment = std::string(comment);
}

FITSRecord::FITSRecord(const char *comment) :
    m_key("COMMENT"),
    m_type(COMMENT)
{
    if(comment)
        m_comment = std::string(comment);
}

FITSRecord::Type FITSRecord::type() const
{
    return m_type;
}

const std::string &FITSRecord::key() const
{
    return m_key;
}

const std::string &FITSRecord::valueString() const
{
    return val_str;
}

int64_t FITSRecord::valueInt() const
{
    if (m_type == LONGLONG)
        return val_int64;
    else
        return 0;
}

/*uint64_t FITSRecord::valueUInt() const
{
    if (m_type == ULONGLONG)
        return val_uint64;
    else
        return 0;
}*/

double FITSRecord::valueDouble() const
{
    if (m_type == DOUBLE)
        return val_double;
    else
        return NAN;
}

const std::string &FITSRecord::comment() const
{
    return m_comment;
}

int FITSRecord::decimal() const
{
    return m_decimal;
}

}


