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

#include "ConcurrentSet.hpp"
/* An object that can be put in a ConcurrentSet, and provide a heartbeat
 * to detect removal from ConcurrentSet
 */
class Collectable
{
        template<class P> friend class ConcurrentSet;
        unsigned long id = 0;
        const ConcurrentSet<void> * current;

        /* Keep the id */
        class HeartBeat
        {
                friend class Collectable;
                unsigned long id;
                const ConcurrentSet<void> * current;
                HeartBeat(unsigned long id, const ConcurrentSet<void> * current)
                    : id(id), current(current) {}
            public:
                bool alive() const
                {
                    return id != 0 && (*current)[id] != nullptr;
                }
        };

    protected:
        /* heartbeat.alive will return true as long as this item has not changed collection.
         * Also detect deletion of the Collectable */
        HeartBeat heartBeat() const
        {
            return HeartBeat(id, current);
        }
};
