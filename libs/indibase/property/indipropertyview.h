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

#include "indiapi.h"
#include "indidevapi.h"
#include "indiwidgettraits.h"

#include "indiwidgetview.h"

#include <string>
#include <cstring>

namespace INDI
{

/**
 * \class INDI::PropertyView
 * \brief Provides decorator for Low-Level INDI VectorProperty
 */
template <typename T>
struct PropertyView: public WidgetTraits<T>::PropertyType
{
    using PropertyType = typename WidgetTraits<T>::PropertyType;
    using WidgetType = WidgetView<T>;

public:
    PropertyView()                                  { memset(this, 0, sizeof(*this)); }
    ~PropertyView()                                 { for(auto &p: *this) {p.clear();} free(widget()); }

public: // setters
    void setDeviceName(const char *name)            { strncpy(this->device, name, MAXINDIDEVICE); }
    void setDeviceName(const std::string &name)     { setDeviceName(name.data()); }

    void setName(const char *name)                  { strncpy(this->name, name, MAXINDINAME); }
    void setName(const std::string &name)           { setName(name.data()); }

    void setLabel(const char *label)                { strncpy(this->label, label, MAXINDILABEL); }
    void setLabel(const std::string &label)         { setLabel(label.data()); }

    void setGroupName(const char *name)             { strncpy(this->group, name, MAXINDIGROUP); }
    void setGroupName(const std::string &name)      { setGroupName(name.data()); }

    void setPermission(IPerm permission);
    void setRule(ISRule rule);
    bool setRule(const std::string &rule);

    void setTimeout(double timeout);
    void setState(IPState state)                    { this->s = state; }

    void setTimestamp(const char *timestamp)        { strncpy(this->timestamp, timestamp, MAXINDITSTAMP); }
    void setTimestamp(const std::string &timestamp) { setTimestamp(timestamp.data()); }

    void setAux(void *user)                         { this->aux = user; }
    void setWidgets(WidgetType *w, size_t count);

public: //getters
    const char *getDeviceName() const               { return this->device; }
    const char *getName()       const               { return this->name; }
    const char *getLabel()      const               { return this->label; }
    const char *getGroupName()  const               { return this->group; }

    IPerm getPermission()       const;
    ISRule getRule()            const;
    double getTimeout()         const;
    IPState getState()          const               { return this->s; }

    const char *getTimestamp()  const               { return this->timestamp; }
    void *getAux()              const               { return this->aux; }

    int count() const;
    WidgetType *widget() const;

public:
    WidgetType *begin() const                       { return widget();           }
    WidgetType *end()   const                       { return widget() + count(); }

public:
    void clear()
    {
        for(auto &p: *this) { p.clear(); }
        free(widget());
        memset(this, 0, sizeof(*this));
    }

};





template <typename T>
inline void PropertyView<T>::setTimeout(double timeout)
{ this->timeout = timeout; }

template <>
inline void PropertyView<ILight>::setTimeout(double)
{ }

template <typename T>
inline void PropertyView<T>::setPermission(IPerm permission)
{ this->p = permission; }

template <>
inline void PropertyView<ILight>::setPermission(IPerm)
{ }

template <typename T>
inline void PropertyView<T>::setRule(ISRule)
{ }

template <>
inline void PropertyView<ISwitch>::setRule(ISRule rule)
{ this->r = rule; }

template <typename T>
inline bool PropertyView<T>::setRule(const std::string &)
{ return false; }

template <>
inline bool PropertyView<ISwitch>::setRule(const std::string &rule)
{ return crackISRule(rule.data(), &this->r) == 0; }

template <typename T>
inline IPerm PropertyView<T>::getPermission() const
{ return this->p; }

template <>
inline IPerm PropertyView<ILight>::getPermission() const
{ return IP_RO; }

template <typename T>
inline ISRule PropertyView<T>::getRule() const
{ return ISR_NOFMANY; }

template <>
inline ISRule PropertyView<ISwitch>::getRule() const
{ return this->r; }

template <typename T>
inline double PropertyView<T>::getTimeout() const
{ return this->timeout; }

template <>
inline double PropertyView<ILight>::getTimeout() const
{ return 0; }

template <>
inline void PropertyView<IText>::setWidgets(WidgetType *w, size_t size)
{ this->tp = w; this->ntp = size; }

template <>
inline void PropertyView<INumber>::setWidgets(WidgetType *w, size_t size)
{ this->np = w; this->nnp = size; }

template <>
inline void PropertyView<ISwitch>::setWidgets(WidgetType *w, size_t size)
{ this->sp = w; this->nsp = size; }

template <>
inline void PropertyView<ILight>::setWidgets(WidgetType *w, size_t size)
{ this->lp = w; this->nlp = size; }

template <>
inline void PropertyView<IBLOB>::setWidgets(WidgetType *w, size_t size)
{ this->bp = w; this->nbp = size; }

template <>
inline int PropertyView<IText>::count() const
{ return this->ntp; }

template <>
inline int PropertyView<INumber>::count() const
{ return this->nnp; }

template <>
inline int PropertyView<ISwitch>::count() const
{ return this->nsp; }

template <>
inline int PropertyView<ILight>::count() const
{ return this->nlp; }

template <>
inline int PropertyView<IBLOB>::count() const
{ return this->nbp; }

template <>
inline PropertyView<IText>::WidgetType *PropertyView<IText>::widget() const
{ return static_cast<WidgetType*>(this->tp); }

template <>
inline PropertyView<INumber>::WidgetType *PropertyView<INumber>::widget() const
{ return static_cast<WidgetType*>(this->np); }

template <>
inline PropertyView<ISwitch>::WidgetType *PropertyView<ISwitch>::widget() const
{ return static_cast<WidgetType*>(this->sp); }

template <>
inline PropertyView<ILight>::WidgetType *PropertyView<ILight>::widget() const
{ return static_cast<WidgetType*>(this->lp); }

template <>
inline PropertyView<IBLOB>::WidgetType *PropertyView<IBLOB>::widget() const
{ return static_cast<WidgetType*>(this->bp); }

}
