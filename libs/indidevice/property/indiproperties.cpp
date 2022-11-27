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
#include "indiproperties.h"
#include "indiproperties_p.h"

namespace INDI
{

PropertiesPrivate::PropertiesPrivate()
{ }

PropertiesPrivate::~PropertiesPrivate()
{ }

Properties::Properties()
    : d_ptr(new PropertiesPrivate)
{ }

Properties::~Properties()
{ }

Properties::Properties(std::shared_ptr<PropertiesPrivate> dd)
    : d_ptr(dd)
{ }

void Properties::push_back(const INDI::Property &property)
{
    D_PTR(Properties);
    d->properties.push_back(property);
}

void Properties::push_back(INDI::Property &&property)
{
    D_PTR(Properties);
    d->properties.push_back(std::move(property));
}

void Properties::clear()
{
    D_PTR(Properties);
    d->properties.clear();
}

Properties::size_type Properties::size() const
{
    D_PTR(const Properties);
    return d->properties.size();
}

Properties::reference Properties::at(size_type pos)
{
    D_PTR(Properties);
    return d->properties.at(pos);
}

Properties::const_reference Properties::at(size_type pos) const
{
    D_PTR(const Properties);
    return d->properties.at(pos);
}

Properties::reference Properties::operator[](Properties::size_type pos)
{
    D_PTR(Properties);
    return d->properties.at(pos);
}

Properties::const_reference Properties::operator[](Properties::size_type pos) const
{
    D_PTR(const Properties);
    return d->properties.at(pos);
}

Properties::reference Properties::front()
{
    D_PTR(Properties);
    return d->properties.front();
}

Properties::const_reference Properties::front() const
{
    D_PTR(const Properties);
    return d->properties.front();
}

Properties::reference Properties::back()
{
    D_PTR(Properties);
    return d->properties.back();
}

Properties::const_reference Properties::back() const
{
    D_PTR(const Properties);
    return d->properties.back();
}

Properties::iterator Properties::begin()
{
    D_PTR(Properties);
    return d->properties.begin();
}

Properties::iterator Properties::end()
{
    D_PTR(Properties);
    return d->properties.end();
}

Properties::const_iterator Properties::begin() const
{
    D_PTR(const Properties);
    return d->properties.begin();
}

Properties::const_iterator Properties::end() const
{
    D_PTR(const Properties);
    return d->properties.end();
}

Properties::iterator Properties::erase(iterator pos)
{
    D_PTR(Properties);
    return d->properties.erase(pos);
}

Properties::iterator Properties::erase(const_iterator pos)
{
    D_PTR(Properties);
    return d->properties.erase(pos);
}

Properties::iterator Properties::erase(iterator first, iterator last)
{
    D_PTR(Properties);
    return d->properties.erase(first, last);
}

Properties::iterator Properties::erase(const_iterator first, const_iterator last)
{
    D_PTR(Properties);
    return d->properties.erase(first, last);
}

#ifdef INDI_PROPERTIES_BACKWARD_COMPATIBILE
INDI::Properties Properties::operator *()
{
    return *this;
}

const INDI::Properties Properties::operator *() const
{
    return *this;
}

Properties *Properties::operator->()
{
    return this;
}

const Properties *Properties::operator->() const
{
    return this;
}

Properties::operator Properties *()
{
    D_PTR(Properties);
    return &d->self;
}

Properties::operator const Properties *() const
{
    D_PTR(const Properties);
    return &d->self;
}

Properties::operator std::vector<INDI::Property *> *()
{
    D_PTR(Properties);
    d->propertiesBC.resize(0);
    d->propertiesBC.reserve(d->properties.size());
    for (auto &it : d->properties)
        d->propertiesBC.push_back(&it);

    return &d->propertiesBC;
}

Properties::operator const std::vector<INDI::Property *> *() const
{
    D_PTR(const Properties);
    d->propertiesBC.resize(0);
    d->propertiesBC.reserve(d->properties.size());
    for (auto &it : d->properties)
        d->propertiesBC.push_back(const_cast<INDI::Property *>(&it));

    return &d->propertiesBC;
}
#endif
}
