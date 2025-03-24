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

#ifdef ENABLE_INDI_SHARED_MEMORY
#define INDIUNIXSOCK "/tmp/indiserver" /* default unix socket path (local connections) */

#include <string>
#include <ev++.h>

class UnixServer
{
        std::string path;
        int sfd = -1;
        ev::io sfdev;

        void accept();
        void ioCb(ev::io &watcher, int revents);

        void log(const std::string &log) const;
    public:
        UnixServer(const std::string &path);

        /* create the public INDI Driver endpoint over UNIX (local) domain.
         * exit on failure
         */
        void listen();

        static std::string unixSocketPath;
};


#endif
