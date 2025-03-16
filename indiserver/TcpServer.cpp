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
#include "TcpServer.hpp"
#include "Constants.hpp"
#include "Utils.hpp"
#include "ClInfo.hpp"
#include "CommandLineArgs.hpp"

#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>

TcpServer::TcpServer(int port): port(port)
{
    sfdev.set<TcpServer, &TcpServer::ioCb>(this);
}

void TcpServer::ioCb(ev::io &, int revents)
{
    if (revents & EV_ERROR)
    {
        int sockErrno = readFdError(this->sfd);
        if (sockErrno)
        {
            log(fmt("Error on tcp server socket: %s\n", strerror(sockErrno)));
            Bye();
        }
    }
    if (revents & EV_READ)
    {
        accept();
    }
}

void TcpServer::listen()
{
    struct sockaddr_in serv_socket;
    int reuse = 1;

    /* make socket endpoint */
    if ((sfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        log(fmt("socket: %s\n", strerror(errno)));
        Bye();
    }

    /* bind to given port for any IP address */
    memset(&serv_socket, 0, sizeof(serv_socket));
    serv_socket.sin_family = AF_INET;
#ifdef SSH_TUNNEL
    serv_socket.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
#else
    serv_socket.sin_addr.s_addr = htonl(INADDR_ANY);
#endif
    serv_socket.sin_port = htons((unsigned short)port);
    if (setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0)
    {
        log(fmt("setsockopt: %s\n", strerror(errno)));
        Bye();
    }
    if (bind(sfd, (struct sockaddr *)&serv_socket, sizeof(serv_socket)) < 0)
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
        log(fmt("listening to port %d on fd %d\n", port, sfd));
}

void TcpServer::accept()
{
    struct sockaddr_in cli_socket;
    socklen_t cli_len;
    int cli_fd;

    /* get a private connection to new client */
    cli_len = sizeof(cli_socket);
    cli_fd  = ::accept(sfd, (struct sockaddr *)&cli_socket, &cli_len);
    if (cli_fd < 0)
    {
        if (errno == EAGAIN || errno == EWOULDBLOCK) return;

        log(fmt("accept: %s\n", strerror(errno)));
        Bye();
    }

    ClInfo * cp = new ClInfo(false);

    /* rig up new clinfo entry */
    cp->setFds(cli_fd, cli_fd);

    if (userConfigurableArguments->verbosity > 0)
    {
        cp->log(fmt("new arrival from %s:%d - welcome!\n",
                    inet_ntoa(cli_socket.sin_addr), ntohs(cli_socket.sin_port)));
    }
#ifdef OSX_EMBEDED_MODE
    fprintf(stderr, "CLIENTS %d\n", clients.size());
    fflush(stderr);
#endif
}
