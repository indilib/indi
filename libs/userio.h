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
#pragma once

#include <stdarg.h>

#ifdef _WINDOWS
#include <windows.h>
#else
#include <sys/types.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef struct userio
{
    size_t (*write)(void *user, const void * ptr, size_t count);
    int (*vprintf)(void *user, const char * format, va_list arg);
} userio;

const struct userio *userio_file();

int userio_printf(const struct userio *io, void *user, const char * format, ...);
int userio_vprintf(const struct userio *io, void *user, const char * format, va_list arg);

size_t userio_write(const struct userio *io, void *user, const void * ptr, size_t count);

int userio_putc(const struct userio *io, void *user, int ch);

// extras
int userio_prints(const struct userio *io, void *user, const char *str);
size_t userio_xml_escape(const struct userio *io, void *user, const char *src);
void userio_xmlv1(const userio *io, void *user);

#ifdef __cplusplus
}
#endif
