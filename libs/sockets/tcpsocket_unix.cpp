/*
    Copyright (C) 2022 by Pawel Soja <kernel32.pl@gmail.com>

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
#include "tcpsocket.h"
#include "tcpsocket_p.h"

#include <sys/un.h>

bool TcpSocketPrivate::createSocket(int domain)
{
    socketFd = ::socket(domain, SOCK_STREAM, 0);
    return socketFd >= 0;
}

bool TcpSocketPrivate::setNonblockSocket()
{
    int flags = fcntl(socketFd, F_GETFL, 0);
    if (flags < 0)
    {
        return false;
    }

    if (fcntl(socketFd, F_SETFL, flags | O_NONBLOCK) < 0)
    {
        return false;
    }

    return true;
}

int TcpSocketPrivate::recvSocket(void *dst, size_t size)
{
    return ::read(socketFd, dst, size);
}

int TcpSocketPrivate::sendSocket(const void *src, size_t size)
{
    return ::write(socketFd, src, size);
}

SocketAddress SocketAddress::afUnix(const std::string &unixPath)
{
    struct sockaddr_un *sa_un = new struct sockaddr_un;

    (void)memset(sa_un, 0, sizeof(struct sockaddr_un));
    sa_un->sun_family = AF_UNIX;

#ifdef __linux__
    // Using abstract socket path to avoid filesystem boilerplate
    const int offset = 1;
#else
    // Using filesystem socket path
    const int offset = 0;
#endif
    strncpy(sa_un->sun_path + offset, unixPath.c_str(), sizeof(sa_un->sun_path) - offset - 1);

    SocketAddress result;
    result.mData.reset(reinterpret_cast<struct sockaddr *>(sa_un));
    result.mSize = offsetof(struct sockaddr_un, sun_path) + unixPath.size() + offset;
    return result;
}
