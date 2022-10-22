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
// internal use only
#pragma once

#include <stdio.h>
#include <unistd.h>
#include <algorithm>

#ifdef _WIN32
#define NOMINMAX
#include <ws2tcpip.h>
#include <winsock2.h>
#include <windows.h>
typedef SOCKET SocketFileDescriptor;
static const int SocketInvalid = INVALID_SOCKET;
#else
#define HAS_EVENT_FD
#include <netdb.h>
#include <sys/socket.h> // select
typedef int SocketFileDescriptor;
static const int SocketInvalid = -1;
#endif

#ifdef _MSC_VER
#include <BaseTsd.h>
typedef SSIZE_T ssize_t;
#endif

#ifdef HAS_EVENT_FD
class EventFd
{
public:
    EventFd()
    {
        if (socketpair(PF_UNIX, SOCK_STREAM, 0, pipefd) < 0)
        {
            perror("socketpair");
        }
    }

    ~EventFd()
    {
        close(pipefd[0]);
        close(pipefd[1]);
    }

public:
    void wakeUp()
    {
        size_t c = 1;
        // wakeup 'select' function
        ssize_t ret = write(pipefd[1], &c, sizeof(c));

        if (ret != sizeof(c))
        {
            perror("the socket cannot be woken up");
        }
        total += ret;
    }

    int fd() const
    {
        return pipefd[0];
    }

    void clear()
    {
        size_t c = 0;
        while (total > 0)
        {
            total -= read(pipefd[0], &c, sizeof(c));
        }
    }

private:
    int pipefd[2] = {-1, -1};
    int total = 0;
};
#endif

// wrapper for unix select function
class Select
{
public:
    Select()
    {
        clear();
    }

public:
    void wakeUp()
    {
#ifdef HAS_EVENT_FD
        eventFd.wakeUp();
#endif
    }

    void clear()
    {
        FD_ZERO(&readEvent);
        FD_ZERO(&writeEvent);
        FD_ZERO(&exceptionEvent);
        fdMax = 0;
#ifdef HAS_EVENT_FD
        eventFd.clear();
#endif
    }

public:
    void setTimeout(int timeout)
    {
        ts.tv_sec = timeout / 1000;
        ts.tv_usec = (timeout % 1000) * 1000;
    }

    void select()
    {
#ifdef HAS_EVENT_FD
        setReadEvent(eventFd.fd());
#endif
        readyDesc = ::select(fdMax + 1, &readEvent, &writeEvent, &exceptionEvent, &ts);
#ifdef HAS_EVENT_FD
        if (isReadEvent(eventFd.fd()))
        {
            eventFd.clear();
        }
#endif
    }

    void select(int timeout)
    {
        setTimeout(timeout);
        select();
    }

public:
    void setReadEvent(SocketFileDescriptor fd)
    {
        FD_SET(fd, &readEvent);
        fdMax = std::max(fdMax, fd);
    }

    void setWriteEvent(SocketFileDescriptor fd)
    {
        FD_SET(fd, &writeEvent);
        fdMax = std::max(fdMax, fd);
    }

    void setExceptionEvent(SocketFileDescriptor fd)
    {
        FD_SET(fd, &exceptionEvent);
        fdMax = std::max(fdMax, fd);
    }

    void setReadWriteEvent(SocketFileDescriptor fd)
    {
        FD_SET(fd, &readEvent);
        FD_SET(fd, &writeEvent);
        fdMax = std::max(fdMax, fd);
    }

    void setReadWriteExceptionEvent(SocketFileDescriptor fd)
    {
        FD_SET(fd, &readEvent);
        FD_SET(fd, &writeEvent);
        FD_SET(fd, &exceptionEvent);
        fdMax = std::max(fdMax, fd);
    }

public:
#ifdef HAS_EVENT_FD
    bool isWakedUp()              const { return FD_ISSET(eventFd.fd(), &readEvent); }
#else
    bool isWakedUp()              const { return false; }
#endif
    bool isTimeout()              const { return readyDesc == 0; }
    bool isError()                const { return readyDesc < 0;  }
    bool isReadEvent(SocketFileDescriptor fd)      const { return FD_ISSET(fd, &readEvent);      }
    bool isWriteEvent(SocketFileDescriptor fd)     const { return FD_ISSET(fd, &writeEvent);     }
    bool isExceptionEvent(SocketFileDescriptor fd) const { return FD_ISSET(fd, &exceptionEvent); }

protected:
    fd_set readEvent;
    fd_set writeEvent;
    fd_set exceptionEvent;
    SocketFileDescriptor fdMax = 0;
    int readyDesc = 0;

    struct timeval ts = {1, 0};

#ifdef HAS_EVENT_FD
    EventFd eventFd;
#endif
};
