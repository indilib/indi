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
#include "indidriver.h"

namespace INDI
{

template <>
void PropertyView<IText>::vapply(const char *format, va_list arg) const
{ IDSetTextVA(this, format, arg); }

template <>
void PropertyView<IText>::vdefine(const char *format, va_list arg) const
{ IDDefTextVA(this, format, arg); }

template <>
void PropertyView<INumber>::vapply(const char *format, va_list arg) const
{ IDSetNumberVA(this, format, arg); }

template <>
void PropertyView<INumber>::vdefine(const char *format, va_list arg) const
{ IDDefNumberVA(this, format, arg); }

template <>
void PropertyView<ISwitch>::vapply(const char *format, va_list arg) const
{ IDSetSwitchVA(this, format, arg); }

template <>
void PropertyView<ISwitch>::vdefine(const char *format, va_list arg) const
{ IDDefSwitchVA(this, format, arg); }

template <>
void PropertyView<ILight>::vapply(const char *format, va_list arg) const
{ IDSetLightVA(this, format, arg); }

template <>
void PropertyView<ILight>::vdefine(const char *format, va_list arg) const
{ IDDefLightVA(this, format, arg); }

template <>
void PropertyView<IBLOB>::vapply(const char *format, va_list arg) const
{ IDSetBLOBVA(this, format, arg); }

template <>
void PropertyView<IBLOB>::vdefine(const char *format, va_list arg) const
{ IDDefBLOBVA(this, format, arg); }

template <typename T>
void PropertyView<T>::apply(const char *format, ...) const
{ va_list ap; va_start(ap, format); this->vapply(format, ap); va_end(ap); }

template <typename T>
void PropertyView<T>::define(const char *format, ...) const
{ va_list ap; va_start(ap, format); this->vdefine(format, ap); va_end(ap); }

template <> template <>
void PropertyView<IText>::fill(
    const char *device, const char *name, const char *label, const char *group,
    IPerm permission, double timeout, IPState state
)
{
    IUFillTextVector(
        this, begin(), count(), device, name, label, group,
        permission, timeout, state
    );
}

template <> template <>
void PropertyView<INumber>::fill(
    const char *device, const char *name, const char *label, const char *group,
    IPerm permission, double timeout, IPState state
)
{
    IUFillNumberVector(
        this, begin(), count(), device, name, label, group,
        permission, timeout, state
    );
}

template <> template <>
void PropertyView<ISwitch>::fill(
    const char *device, const char *name, const char *label, const char *group,
    IPerm permission, ISRule rule, double timeout, IPState state
)
{
    IUFillSwitchVector(
        this, begin(), count(), device, name, label, group,
        permission, rule, timeout, state
    );
}

template <> template <>
void PropertyView<ILight>::fill(
    const char *device, const char *name, const char *label, const char *group,
    IPState state
)
{
    IUFillLightVector(
        this, begin(), count(), device, name, label, group,
        state
    );
}

template <> template <>
void PropertyView<IBLOB>::fill(
    const char *device, const char *name, const char *label, const char *group,
    IPerm permission, double timeout, IPState state
)
{
    IUFillBLOBVector(
        this, begin(), count(), device, name, label, group,
        permission, timeout, state
    );
}

template <> template<>
bool PropertyView<IText>::update(const char * const texts[], const char * const names[], int n)
{ return IUUpdateText(this, const_cast<char**>(texts), const_cast<char**>(names), n) == 0; }

template <> template<>
bool PropertyView<INumber>::update(const double values[], const char * const names[], int n)
{ return IUUpdateNumber(this, const_cast<double*>(values), const_cast<char**>(names), n) == 0; }

template <> template<>
bool PropertyView<ISwitch>::update(const ISState states[], const char * const names[], int n)
{ return IUUpdateSwitch(this, const_cast<ISState*>(states), const_cast<char**>(names), n) == 0; }

template <> template<>
bool PropertyView<IBLOB>::update(
    const int sizes[], const int blobsizes[], const char *const blobs[], const char *const formats[],
    const char * const names[], int n
)
{
    return IUUpdateBLOB(
        this,
        const_cast<int *>(sizes), const_cast<int *>(blobsizes),
        const_cast<char **>(blobs), const_cast<char **>(formats),
        const_cast<char **>(names), n
    ) == 0;
}

template <> template<>
void PropertyView<INumber>::updateMinMax()
{ IUUpdateMinMax(this); }

void WidgetView<IText>::fill(const char *name, const char *label, const char *initialText)
{ IUFillText(this, name, label, initialText); }


void WidgetView<ISwitch>::fill(const char *name, const char *label, ISState state)
{ IUFillSwitch(this, name, label, state); }


void WidgetView<ILight>::fill(const char *name, const char *label, IPState state)
{ IUFillLight(this, name, label, state); }

void WidgetView<INumber>::fill(const char *name, const char *label, const char *format,
              double min, double max, double step, double value)
{ IUFillNumber(this, name, label, format, min, max, step, value); }

void WidgetView<IBLOB>::fill(const char *name, const char *label, const char *format)
{ IUFillBLOB(this, name, label, format); }

template struct PropertyView<INumber>;
template struct PropertyView<IText>;
template struct PropertyView<ISwitch>;
template struct PropertyView<ILight>;
template struct PropertyView<IBLOB>;

}
