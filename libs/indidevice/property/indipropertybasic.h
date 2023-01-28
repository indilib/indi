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

#include "indiproperty.h"
#include "indimacros.h"
#include <algorithm>

namespace INDI
{

using WidgetText   = INDI::WidgetViewText;
using WidgetNumber = INDI::WidgetViewNumber;
using WidgetSwitch = INDI::WidgetViewSwitch;
using WidgetLight  = INDI::WidgetViewLight;
using WidgetBlob   = INDI::WidgetViewBlob;

template <typename>
class PropertyBasicPrivateTemplate;

template <typename T>
class PropertyBasic : public INDI::Property
{
        using PropertyBasicPrivate = PropertyBasicPrivateTemplate<T>;
        DECLARE_PRIVATE(PropertyBasic)
    public:
        using ViewType = T;

    public:
        ~PropertyBasic();

    public:
        void setName(const char *name);
        void setName(const std::string &name);

        void setLabel(const char *label);
        void setLabel(const std::string &label);

        void setGroupName(const char *name);
        void setGroupName(const std::string &name);

        void setPermission(IPerm permission);
        void setTimeout(double timeout);
        void setState(IPState state);

        void setTimestamp(const char *timestamp);
        void setTimestamp(const std::string &timestamp);

    public:
        const char *getName()               const;
        const char *getLabel()              const;
        const char *getGroupName()          const;

        IPerm       getPermission()         const;
        const char *getPermissionAsString() const;

        double      getTimeout()            const;
        IPState     getState()              const;
        const char *getStateAsString()      const;

        const char *getTimestamp()          const;

    public:
        bool isEmpty() const;

        bool isNameMatch(const char *otherName) const;
        bool isNameMatch(const std::string &otherName) const;

        bool isLabelMatch(const char *otherLabel) const;
        bool isLabelMatch(const std::string &otherLabel) const;

    public:
        void save(FILE *f) const;

        void vapply(const char *format, va_list args) const;
        void vdefine(const char *format, va_list args) const;

        void apply(const char *format, ...) const ATTRIBUTE_FORMAT_PRINTF(2, 3);
        void define(const char *format, ...) const ATTRIBUTE_FORMAT_PRINTF(2, 3);

        void apply() const;
        void define() const;

    protected:
        PropertyView<T> * operator &();

    public:
        size_t size() const;
        size_t count() const { return size(); }

    public:
        void reserve(size_t size);
        void resize(size_t size);

        void shrink_to_fit();

        void push(WidgetView<T> &&item);
        void push(const WidgetView<T> &item);

        const WidgetView<T> *at(size_t index) const;

        WidgetView<T> &operator[](ssize_t index) const;

    public: // STL-style iterators
        WidgetView<T> *begin();
        WidgetView<T> *end();
        const WidgetView<T> *begin() const;
        const WidgetView<T> *end() const;

        template <typename Predicate>
        WidgetView<T> *find_if(Predicate pred)
        {
            return std::find_if(begin(), end(), pred);
        }

        template <typename Predicate>
        const WidgetView<T> *find_if(Predicate pred) const
        {
            return std::find_if(begin(), end(), pred);
        }

    public:
        WidgetView<T> *findWidgetByName(const char *name) const;
        int findWidgetIndexByName(const char *name) const;

    protected:
        PropertyBasic(PropertyBasicPrivate &dd);
        PropertyBasic(const std::shared_ptr<PropertyBasicPrivate> &dd);

#ifdef INDI_PROPERTY_BACKWARD_COMPATIBILE
    public: // deprecated
        INDI_DEPRECATED("Do not use INDI::PropertyXXX as pointer.")
        INDI::PropertyView<T> *operator->();

        INDI_DEPRECATED("Do not use INDI::PropertyXXX as pointer.")
        INDI::PropertyView<T>  operator*();
#endif
};

}
