/*
    Copyright (C) 2020 by Pawel Soja <kernel32.pl@gmail.com>
    Copyright (C) 2015 by Jasem Mutlaq <mutlaqja@ikarustech.com>
    Copyright (C) 2014 by geehalel <geehalel@gmail.com>

    Stream Recorder

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


#ifdef __cplusplus
#include <string>
#include <cstring>
#include <sys/stat.h>
#include <ctime>

#include "indimacros.h"
#else
#include <string.h>
#endif

// C
#ifdef __cplusplus
extern "C"
{
#endif
/**
 * @brief The strlcpy() function copy strings respectively.
 * They are designed to be safer, more consistent, and less error prone replacements for strncpy.
 */
inline static size_t indi_strlcpy(char * dst, const char * src, size_t maxlen)
{
    const size_t srclen = strlen(src);
    if (srclen + 1 < maxlen)
    {
        memcpy(dst, src, srclen + 1);
    }
    else if (maxlen != 0)
    {
        memcpy(dst, src, maxlen - 1);
        dst[maxlen - 1] = '\0';
    }
    return srclen;
}
#ifdef __cplusplus
}
#endif

// C++
#ifdef __cplusplus

#ifdef _WINDOWS
typedef int mode_t;
#endif

namespace INDI
{
/**
 * @brief Create directory.
 */
int mkdir(const char *path, mode_t mode);

/**
 * @brief Create a path directory - this function uses 'mkdir'
 */
int mkpath(std::string path, mode_t mode);

/**
 * @brief Converts the date and time to string - this function uses 'strftime'
 */
std::string format_time(const std::tm &tm, const char *format);

/**
 * @brief Replaces every occurrence of the string 'search' with the string 'replace'
 */
void replace_all(std::string &subject, const std::string &search, const std::string &replace);

/**
 * @brief The strlcpy() function copy strings respectively.
 * They are designed to be safer, more consistent, and less error prone replacements for strncpy.
 */
inline size_t strlcpy(char * dst, const char * src, size_t maxlen)
{
    return indi_strlcpy(dst, src, maxlen);
}

/**
 * @brief The strlcpy() function copy strings respectively.
 * They are designed to be safer, more consistent, and less error prone replacements for strncpy.
 */
template <size_t N>
inline size_t strlcpy(char (&dst)[N], const char * src)
{
    return indi_strlcpy(dst, src, N);
}

}
#endif
