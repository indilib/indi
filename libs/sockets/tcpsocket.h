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

#include <cstdint>
#include <string>
#include <memory>
#include <functional>

#include "indimacros.h"

class TcpSocketPrivate;
class TcpSocket
{
    public:
        enum SocketError
        {
            ConnectionRefusedError,
            RemoteHostClosedError,
            HostNotFoundError,
            SocketAccessError,
            SocketResourceError,
            SocketTimeoutError, /* 5 */
            DatagramTooLargeError,
            NetworkError,
            AddressInUseError,
            SocketAddressNotAvailableError,
            UnsupportedSocketOperationError, /* 10 */
            UnfinishedSocketOperationError,
            ProxyAuthenticationRequiredError,
            SslHandshakeFailedError,
            ProxyConnectionRefusedError,
            ProxyConnectionClosedError, /* 15 */
            ProxyConnectionTimeoutError,
            ProxyNotFoundError,
            ProxyProtocolError,
            OperationError,
            SslInternalError, /* 20 */
            SslInvalidUserDataError,
            TemporaryError,

            UnknownSocketError = -1
        };

        enum SocketState
        {
            UnconnectedState,
            HostLookupState,
            ConnectingState,
            ConnectedState,
            BoundState,
            ListeningState,
            ClosingState
        };

    public:
        TcpSocket();
        virtual ~TcpSocket();

    public:
        void setConnectionTimeout(int timeout);

    public:
        void connectToHost(const std::string &hostName, uint16_t port);
        void disconnectFromHost();

    public:
        ssize_t write(const char *data, size_t size);
        ssize_t write(const std::string &data);

    public:
        SocketError error() const;
        std::string errorString() const;

    public:
        void onConnected(const std::function<void()> &callback);
        void onDisconnected(const std::function<void()> &callback);
        void onData(const std::function<void(const char *, size_t)> &callback);
        void onErrorOccurred(const std::function<void(SocketError)> &callback);

    public:
        bool waitForDisconnected(int timeout = 2000) const;
        bool waitForConnected(int timeout = 2000) const;

    public:
        int *socketDescriptor() const;

    protected:
        virtual void connected();
        virtual void disconnected();
        virtual void readyRead();
        virtual void errorOccurred(SocketError);

    protected:
        void emitConnected() const;
        void emitDisconnected() const;
        void emitData(const char *data, size_t size) const;
        void emitErrorOccurred(SocketError error) const;

    protected:
        void setSocketError(SocketError socketError);

    protected:
        friend class TcpSocketPrivate;
        std::unique_ptr<TcpSocketPrivate> d_ptr;
        TcpSocket(std::unique_ptr<TcpSocketPrivate> &&d);
};
