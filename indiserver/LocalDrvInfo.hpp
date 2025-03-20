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

#include "DvrInfo.hpp"

#include <ev++.h>
#include <string>

class LocalDvrInfo: public DvrInfo
{
        char errbuff[1024];     /* buffer for stderr pipe. line too long will be clipped */
        int errbuffpos = 0;     /* first free pos in buffer */
        ev::io     eio;         /* Event loop io events */
        ev::child  pidwatcher;
        void onEfdEvent(ev::io &watcher, int revents); /* callback for data on efd */
        void onPidEvent(ev::child &watcher, int revents);

        int pid = 0;            /* process id or 0 for N/A (not started/terminated) */
        int efd = -1;           /* stderr from driver, or -1 when N/A */

        void closeEfd();
        void closePid();
    protected:
        LocalDvrInfo(const LocalDvrInfo &model);

    public:
        std::string envDev;
        std::string envConfig;
        std::string envSkel;
        std::string envPrefix;

        LocalDvrInfo();
        virtual ~LocalDvrInfo();

        void start() override;

        LocalDvrInfo * clone() const override;

        const std::string remoteServerUid() const override
        {
            return "";
        }
};