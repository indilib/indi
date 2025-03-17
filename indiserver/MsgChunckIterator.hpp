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

class MsgChunckIterator
{
        friend class SerializedMsg;
        size_t chunckId;
        unsigned long chunckOffset;
        bool endReached;
    public:
        MsgChunckIterator()
        {
            reset();
        }

        // Point to start of message.
        void reset()
        {
            chunckId = 0;
            chunckOffset = 0;
            // No risk of 0 length message, so always false here
            endReached = false;
        }

        bool done() const
        {
            return endReached;
        }
};
