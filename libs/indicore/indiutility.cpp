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
#include "indiutility.h"
#include <cerrno>

#ifdef _MSC_VER

#include <direct.h>

#ifndef S_ISDIR
#define S_ISDIR(m) (((m) & _S_IFDIR) == _S_IFDIR)
#endif

#define mkdir _mkdir

#endif

namespace INDI
{

int mkdir(const char *path, mode_t mode)
{
#ifdef _WIN32
    INDI_UNUSED(mode);
    return ::mkdir(path);
#else
    return ::mkdir(path, mode);
#endif
}

int mkpath(std::string s, mode_t mode)
{
    size_t pre = 0, pos;
    std::string dir;
    int mdret = 0;
    struct stat st;

    if (s[s.size() - 1] != '/')
        s += '/';

    while ((pos = s.find_first_of('/', pre)) != std::string::npos)
    {
        dir = s.substr(0, pos++);
        pre = pos;
        if (dir.size() == 0)
            continue;

        if (stat(dir.c_str(), &st))
        {
            if (errno != ENOENT || ((mdret = mkdir(dir.c_str(), mode)) && errno != EEXIST))
            {
                return mdret;
            }
        }
        else
        {
            if (!S_ISDIR(st.st_mode))
            {
                return -1;
            }
        }
    }
    return mdret;
}

std::string format_time(const std::tm &tm, const char *format)
{
    char cstr[32];

    size_t size = strftime(cstr, sizeof(cstr), format, &tm);

    return std::string(cstr, size);
}

void replace_all(std::string &subject, const std::string &search, const std::string &replace)
{
    size_t pos = 0;
    while ((pos = subject.find(search, pos)) != std::string::npos)
    {
        subject.replace(pos, search.length(), replace);
        pos += replace.length();
    }
}

}
