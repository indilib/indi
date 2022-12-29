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

#ifdef _MSC_VER
#pragma comment(lib, "Ws2_32.lib")
#endif

bool TcpSocketPrivate::createSocket(int domain)
{
    WSADATA wsaData;

    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != NO_ERROR)
    {
        return false;
    }

    socketFd = socket(domain, SOCK_STREAM, IPPROTO_TCP);
    if (socketFd == INVALID_SOCKET)
    {
        WSACleanup();
        //IDLog("Socket error: %d\n", WSAGetLastError());
        return false;
    }
    return true;
}

bool TcpSocketPrivate::setNonblockSocket()
{
    u_long iMode = 0;
    int iResult = ioctlsocket(socketFd, FIONBIO, &iMode);
    return iResult == NO_ERROR;
}

int TcpSocketPrivate::recvSocket(void *dst, size_t size)
{
    return ::recv(socketFd, static_cast<char *>(dst), size, 0);
}

int TcpSocketPrivate::sendSocket(const void *src, size_t size)
{
    return ::send(socketFd, static_cast<const char *>(src), size, 0);
}

SocketAddress SocketAddress::afUnix(const std::string &)
{
    return SocketAddress();
}
