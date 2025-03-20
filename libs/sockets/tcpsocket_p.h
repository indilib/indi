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
#pragma once

#include "tcpsocket.h"
#include "select.h"

#include <fcntl.h>
#include <signal.h>
#include <cstring>
#include <cstdio>

#include <functional>
#include <mutex>
#include <thread>
#include <atomic>
#include <condition_variable>

class SocketAddress
{
    public:
        static const char *unixDomainPrefix;

    public:
        SocketAddress() = default;
        explicit SocketAddress(const std::string &hostName, unsigned short port);

    public:
        static bool isUnix(const std::string &hostName);

    public:
        bool isValid() const
        {
            return data() != nullptr;
        }

        const struct sockaddr *data() const
        {
            return mData.get();
        }

        size_t size() const
        {
            return mSize;
        }

    public:
        const struct sockaddr *operator&() const
        {
            return data();
        }

    protected:
        static SocketAddress afInet(const std::string &hostName, unsigned short port);
        static SocketAddress afUnix(const std::string &hostName);

    protected:
        std::unique_ptr<struct sockaddr> mData;
        size_t mSize;
};

class TcpSocketPrivate
{
    public:
        TcpSocketPrivate(TcpSocket *parent);
        virtual ~TcpSocketPrivate();

    public: // platform dependent
        bool createSocket(int domain);
        void closeSocket();
        ssize_t recvSocket(void *dst, size_t size);
        ssize_t sendSocket(const void *src, size_t size);
        bool setNonblockSocket();

    public: // low level helpers
        bool connectSocket(const std::string &hostName, unsigned short port);
        bool waitForConnectedSockets();
        bool processSocket();

    public: // TcpSocketPrivate API
        ssize_t write(const void *data, size_t size);

        void connectToHost(const std::string &hostName, unsigned short port);
        void aboutToClose();

        void joinThread(std::thread &thread);

    public:
        enum ErrorType
        {
            ErrorTypeSystem,
            ErrorTypeInternal
        };
        void setSocketError(TcpSocket::SocketError error, ErrorType errorType = ErrorTypeSystem,
                            const std::string &errorString = "");
        void setSocketState(TcpSocket::SocketState state);

    public:
        TcpSocket *parent;
        SocketFileDescriptor socketFd = SocketInvalid;
        Select select;
        int timeout{30000};

        std::thread thread;
        std::atomic<bool> isAboutToClose{false};

        mutable std::mutex socketStateMutex;
        mutable std::condition_variable socketStateChanged;

        std::atomic<TcpSocket::SocketState> socketState{TcpSocket::UnconnectedState};
        TcpSocket::SocketError socketError;
        std::string errorString;

        // events
        std::function<void()> onConnected;
        std::function<void()> onDisconnected;
        std::function<void(const char *, size_t)> onData;
        std::function<void(TcpSocket::SocketError)> onErrorOccurred;
};
