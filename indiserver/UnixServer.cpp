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
#include "UnixServer.hpp"
#include "Utils.hpp"
#include "Constants.hpp"
#include "ClInfo.hpp"
#include "CommandLineArgs.hpp"

#include <sys/un.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <unistd.h>
#ifdef ENABLE_INDI_SHARED_MEMORY

std::string UnixServer::unixSocketPath = INDIUNIXSOCK;

UnixServer::UnixServer(const std::string &path): path(path)
{
    sfdev.set<UnixServer, &UnixServer::ioCb>(this);
}

void UnixServer::log(const std::string &str) const
{
    std::string logLine = "Local server: ";
    logLine += str;
    ::log(logLine);
}

void UnixServer::ioCb(ev::io &, int revents)
{
    if (revents & EV_ERROR)
    {
        int sockErrno = readFdError(this->sfd);
        if (sockErrno)
        {
            log(fmt("Error on unix socket: %s\n", strerror(sockErrno)));
            Bye();
        }
    }
    if (revents & EV_READ)
    {
        accept();
    }
}

void initUnixSocketAddr(const std::string &unixAddr, struct sockaddr_un &serv_addr_un, socklen_t &addrlen, bool bind)
{
    memset(&serv_addr_un, 0, sizeof(serv_addr_un));
    serv_addr_un.sun_family = AF_UNIX;

#ifdef __linux__
    (void) bind;

    // Using abstract socket path to avoid filesystem boilerplate
    strncpy(serv_addr_un.sun_path + 1, unixAddr.c_str(), sizeof(serv_addr_un.sun_path) - 2);

    int len = offsetof(struct sockaddr_un, sun_path) + unixAddr.size() + 1;

    addrlen = len;
#else
    // Using filesystem socket path
    strncpy(serv_addr_un.sun_path, unixAddr.c_str(), sizeof(serv_addr_un.sun_path) - 1);

    int len = offsetof(struct sockaddr_un, sun_path) + unixAddr.size();

    if (bind)
    {
        unlink(unixAddr.c_str());
    }
#endif
    addrlen = len;
}

void UnixServer::listen()
{
    struct sockaddr_un serv_socket;

    /* make socket endpoint */
    if ((sfd = socket(AF_UNIX, SOCK_STREAM, 0)) < 0)
    {
        log(fmt("socket: %s\n", strerror(errno)));
        Bye();
    }

    int reuse = 1;
    if (setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0)
    {
        log(fmt("setsockopt: %s\n", strerror(errno)));
        Bye();
    }

    /* bind to given path as unix address */
    socklen_t len;
    initUnixSocketAddr(path, serv_socket, len, true);

    if (bind(sfd, (struct sockaddr *)&serv_socket, len) < 0)
    {
        log(fmt("bind: %s\n", strerror(errno)));
        Bye();
    }

    /* willing to accept connections with a backlog of 5 pending */
    if (::listen(sfd, 5) < 0)
    {
        log(fmt("listen: %s\n", strerror(errno)));
        Bye();
    }

    fcntl(sfd, F_SETFL, fcntl(sfd, F_GETFL, 0) | O_NONBLOCK);
    sfdev.start(sfd, EV_READ);

    /* ok */
    if (userConfigurableArguments->verbosity > 0)
        log(fmt("listening on local domain at: @%s\n", path.c_str()));
}

void UnixServer::accept()
{
    int cli_fd;

    /* get a private connection to new client */
    cli_fd  = ::accept(sfd, 0, 0);
    if (cli_fd < 0)
    {
        if (errno == EAGAIN || errno == EWOULDBLOCK) return;

        log(fmt("accept: %s\n", strerror(errno)));
        Bye();
    }

    ClInfo * cp = new ClInfo(true);

    /* rig up new clinfo entry */
    cp->setFds(cli_fd, cli_fd);

    if (userConfigurableArguments->verbosity > 0)
    {
#ifdef SO_PEERCRED
#ifdef __OpenBSD__
        struct sockpeercred ucred;
        socklen_t len = sizeof(struct sockpeercred);
#else
        struct ucred ucred;
        socklen_t len = sizeof(struct ucred);
#endif

        if (getsockopt(cli_fd, SOL_SOCKET, SO_PEERCRED, &ucred, &len) == -1)
        {
            log(fmt("getsockopt failed: %s\n", strerror(errno)));
            Bye();
        }

        cp->log(fmt("new arrival from local pid %ld (user: %ld:%ld) - welcome!\n", (long)ucred.pid, (long)ucred.uid,
                    (long)ucred.gid));
#else
        cp->log(fmt("new arrival from local domain  - welcome!\n"));
#endif
    }

#ifdef OSX_EMBEDED_MODE
    fprintf(stderr, "CLIENTS %d\n", clients.size());
    fflush(stderr);
#endif
}

#endif // ENABLE_INDI_SHARED_MEMORY
