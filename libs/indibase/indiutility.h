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

#include <string>
#include <sys/stat.h>
#include <ctime>

#include "indimacros.h"

namespace INDI
{

/**
 * @brief Create a path directory - this function uses 'mkdir'
 * @note Not available on Windows. Need to use C++17 cross-platform std::filesystem later.
 */
#ifndef _WINDOWS
int mkpath(std::string path, mode_t mode);
#endif
/**
 * @brief Converts the date and time to string - this function uses 'strftime'
 */
std::string format_time(const std::tm &tm, const char *format);

/**
 * @brief Replaces every occurrence of the string 'search' with the string 'replace'
 */
void replace_all(std::string &subject, const std::string &search, const std::string &replace);

}
