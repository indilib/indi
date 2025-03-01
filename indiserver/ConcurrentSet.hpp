/* INDI Server for protocol version 1.7.
 * Copyright (C) 2007 Elwood C. Downey <ecdowney@clearskyinstitute.com>
                 2013 Jasem Mutlaq <mutlaqja@ikarustech.com>
                 2022 Ludovic Pollet
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

#include <map>
#include <vector>

template<class M>
class ConcurrentSet
{
        unsigned long identifier = 1;
        std::map<unsigned long, M*> items;

    public:
        void insert(M* item)
        {
            item->id = identifier++;
            items[item->id] = item;
            item->current = (ConcurrentSet<void>*)this;
        }

        void erase(M* item)
        {
            items.erase(item->id);
            item->id = 0;
            item->current = nullptr;
        }

        std::vector<unsigned long> ids() const
        {
            std::vector<unsigned long> result;
            for(auto item : items)
            {
                result.push_back(item.first);
            }
            return result;
        }

        M* operator[](unsigned long id) const
        {
            auto e = items.find(id);
            if (e == items.end())
            {
                return nullptr;
            }
            return e->second;
        }

        class iterator
        {
                friend class ConcurrentSet<M>;
                const ConcurrentSet<M> * parent;
                std::vector<unsigned long> ids;
                // Will be -1 when done
                long int pos = 0;

                void skip()
                {
                    if (pos == -1) return;
                    while(pos < (long int)ids.size() && !(*parent)[ids[pos]])
                    {
                        pos++;
                    }
                    if (pos == (long int)ids.size())
                    {
                        pos = -1;
                    }
                }
            public:
                iterator(const ConcurrentSet<M> * parent) : parent(parent) {}

                bool operator!=(const iterator &o)
                {
                    return pos != o.pos;
                }

                iterator &operator++()
                {
                    if (pos != -1)
                    {
                        pos++;
                        skip();
                    }
                    return *this;
                }

                M * operator*() const
                {
                    return (*parent)[ids[pos]];
                }
        };

        iterator begin() const
        {
            iterator result(this);
            for(auto item : items)
            {
                result.ids.push_back(item.first);
            }
            result.skip();
            return result;
        }

        iterator end() const
        {
            iterator result(nullptr);
            result.pos = -1;
            return result;
        }
};

