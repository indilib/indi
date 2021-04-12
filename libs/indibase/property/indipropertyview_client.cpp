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

#include "indipropertyview.h"

namespace INDI
{

static void errorUnavailable(const char *function)
{ fprintf(stderr, "%s method available only on driver side\n", function); }

template <>
void PropertyView<IText>::vapply(const char *, va_list) const
{ errorUnavailable(__FUNCTION__); }

template <>
void PropertyView<IText>::vdefine(const char *, va_list) const
{ errorUnavailable(__FUNCTION__); }

template <>
void PropertyView<INumber>::vapply(const char *, va_list) const
{ errorUnavailable(__FUNCTION__); }

template <>
void PropertyView<INumber>::vdefine(const char *, va_list) const
{ errorUnavailable(__FUNCTION__); }

template <>
void PropertyView<ISwitch>::vapply(const char *, va_list) const
{ errorUnavailable(__FUNCTION__); }

template <>
void PropertyView<ISwitch>::vdefine(const char *, va_list) const
{ errorUnavailable(__FUNCTION__); }

template <>
void PropertyView<ILight>::vapply(const char *, va_list) const
{ errorUnavailable(__FUNCTION__); }

template <>
void PropertyView<ILight>::vdefine(const char *, va_list) const
{ errorUnavailable(__FUNCTION__); }

template <>
void PropertyView<IBLOB>::vapply(const char *, va_list) const
{ errorUnavailable(__FUNCTION__); }

template <>
void PropertyView<IBLOB>::vdefine(const char *, va_list) const
{ errorUnavailable(__FUNCTION__); }

template <typename T>
void PropertyView<T>::apply(const char *, ...) const
{ errorUnavailable(__FUNCTION__); }

template <typename T>
void PropertyView<T>::define(const char *, ...) const
{ errorUnavailable(__FUNCTION__); }

template <> template <>
void PropertyView<IText>::fill(
    const char *, const char *, const char *, const char *,
    IPerm, double, IPState
)
{ errorUnavailable(__FUNCTION__); }

template <> template <>
void PropertyView<INumber>::fill(
    const char *, const char *, const char *, const char *,
    IPerm, double, IPState
)
{ errorUnavailable(__FUNCTION__); }

template <> template <>
void PropertyView<ISwitch>::fill(
    const char *, const char *, const char *, const char *,
    IPerm, ISRule, double, IPState
)
{ errorUnavailable(__FUNCTION__); }

template <> template <>
void PropertyView<ILight>::fill(
    const char *, const char *, const char *, const char *,
    IPState
)
{ errorUnavailable(__FUNCTION__); }

template <> template <>
void PropertyView<IBLOB>::fill(
    const char *, const char *, const char *, const char *,
    IPerm, double, IPState
)
{ errorUnavailable(__FUNCTION__); }

template <> template<>
bool PropertyView<IText>::update(const char * const [], const char * const [], int)
{ errorUnavailable(__FUNCTION__); return false; }

template <> template<>
bool PropertyView<INumber>::update(const double [], const char * const [], int)
{ errorUnavailable(__FUNCTION__); return false; }

template <> template<>
bool PropertyView<ISwitch>::update(const ISState [], const char * const [], int)
{ errorUnavailable(__FUNCTION__); return false; }

template <> template<>
bool PropertyView<IBLOB>::update(
    const int [], const int [], const char * const [], const char * const [],
    const char * const [], int
)
{ errorUnavailable(__FUNCTION__); return false; }

template <> template<>
void PropertyView<INumber>::updateMinMax()
{ errorUnavailable(__FUNCTION__); }


void WidgetView<IText>::fill(const char *, const char *, const char *)
{ errorUnavailable(__FUNCTION__); }

void WidgetView<ISwitch>::fill(const char *, const char *, ISState)
{ errorUnavailable(__FUNCTION__); }


void WidgetView<ILight>::fill(const char *, const char *, IPState)
{ errorUnavailable(__FUNCTION__); }

void WidgetView<INumber>::fill(const char *, const char *, const char *,
              double, double, double, double)
{ errorUnavailable(__FUNCTION__); }

void WidgetView<IBLOB>::fill(const char *, const char *, const char *)
{ errorUnavailable(__FUNCTION__); }

template struct PropertyView<INumber>;
template struct PropertyView<IText>;
template struct PropertyView<ISwitch>;
template struct PropertyView<ILight>;
template struct PropertyView<IBLOB>;

}
