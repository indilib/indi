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
#include <set>
class Msg;
class SerializedMsg;

class SerializationRequirement
{
        friend class Msg;
        friend class SerializedMsg;

        // If the xml is still required
        bool xml{false};
        // Set of sharedBuffer that are still required
        std::set<int> sharedBuffers{};

        SerializationRequirement() noexcept = default;

        void add(const SerializationRequirement &from);

        bool operator==(const SerializationRequirement   &sr) const;
};
