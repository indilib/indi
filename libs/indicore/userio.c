/*
    Copyright (C) 2021 by Pawel Soja <kernel32.pl@gmail.com>

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
#include "userio.h"
#include <stddef.h>
#include <stdio.h>
#include <string.h>

static ssize_t s_file_write(void *user, const void * ptr, size_t count)
{
    return fwrite(ptr, 1, count, (FILE *)user);
}

static int s_file_printf(void *user, const char * format, va_list arg)
{
    return vfprintf((FILE *)user, format, arg);
}

static const struct userio s_userio_file = {
    .write = s_file_write,
    .vprintf = s_file_printf,
    .joinbuff = NULL,
};

const struct userio *userio_file()
{
    return &s_userio_file;
}

ssize_t userio_printf(const struct userio *io, void *user, const char * format, ...)
{
    int ret;
    va_list ap;
    va_start(ap, format);
    ret = io->vprintf(user, format, ap);
    va_end(ap);
    return ret;
}

ssize_t userio_vprintf(const struct userio *io, void *user, const char * format, va_list arg)
{
    return io->vprintf(user, format, arg);
}

ssize_t userio_write(const struct userio *io, void *user, const void * ptr, size_t count)
{
    return io->write(user, ptr, count);
}

ssize_t userio_prints(const struct userio *io, void *user, const char *str)
{
    return io->write(user, str, strlen(str));
}

ssize_t userio_putc(const struct userio *io, void *user, int ch)
{
    char c = ch;
    return io->write(user, &c, sizeof(c));
}

size_t userio_xml_escape(const struct userio *io, void *user, const char *src)
{
    size_t total = 0;
    const char *ptr = src;
    const char *replacement;

    for(; *ptr; ++ptr)
    {
        switch(*ptr)
        {
        case  '&': replacement = "&amp;";  break;
        case '\'': replacement = "&apos;"; break;
        case  '"': replacement = "&quot;"; break;
        case  '<': replacement = "&lt;";   break;
        case  '>': replacement = "&gt;";   break;
        default:   replacement = NULL;
        }

        if (replacement != NULL)
        {
            total += userio_write(io, user, src, (size_t)(ptr - src));
            src = ptr + 1;
            total += userio_write(io, user, replacement, strlen(replacement));
        }
    }
    total += userio_write(io, user, src, (size_t)(ptr - src));
    return total;
}

void userio_xmlv1(const userio *io, void *user)
{
    userio_prints(io, user, "<?xml version='1.0'?>\n");
}
