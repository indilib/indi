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
#include <vector>
#include <deque>
#include <algorithm>

#define INDI_PROPERTIES_BACKWARD_COMPATIBILE
namespace INDI
{

class PropertiesPrivate;
class Properties
{
        DECLARE_PRIVATE(Properties)

    public:
        using iterator        = std::deque<INDI::Property>::iterator;
        using const_iterator  = std::deque<INDI::Property>::const_iterator;
        using reference       = std::deque<INDI::Property>::reference;
        using const_reference = std::deque<INDI::Property>::const_reference;
        using size_type       = std::deque<INDI::Property>::size_type;

    public:
        Properties();
        ~Properties();

    public:
        void push_back(const INDI::Property &property);
        void push_back(INDI::Property &&property);
        void clear();

    public:
        bool empty() const;

    public:
        size_type size() const;

    public:
        reference at(size_type pos);
        const_reference at(size_type pos) const;

        reference operator[](size_type pos);
        const_reference operator[](size_type pos) const;

        reference front();
        const_reference front() const;

        reference back();
        const_reference back() const;

    public:
        iterator begin();
        iterator end();

        const_iterator begin() const;
        const_iterator end() const;

    public:
        iterator erase(iterator pos);
        iterator erase(const_iterator pos);
        iterator erase(iterator first, iterator last);
        iterator erase(const_iterator first, const_iterator last);

        template<typename Predicate>
        iterator erase_if(Predicate predicate);

    public:
#ifdef INDI_PROPERTIES_BACKWARD_COMPATIBILE
        INDI::Properties operator *();
        const INDI::Properties operator *() const;

        Properties *operator->();
        const Properties *operator->() const;

        operator std::vector<INDI::Property *> *();
        operator const std::vector<INDI::Property *> *() const;

        operator Properties *();
        operator const Properties *() const;
#endif

    protected:
        std::shared_ptr<PropertiesPrivate> d_ptr;
        Properties(std::shared_ptr<PropertiesPrivate> dd);
};

template<typename Predicate>
inline Properties::iterator Properties::erase_if(Predicate predicate)
{
    return erase(std::remove_if(begin(), end(), predicate), end());
}

}
