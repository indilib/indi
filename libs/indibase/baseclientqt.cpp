/*******************************************************************************
  Copyright(c) 2016 Jasem Mutlaq. All rights reserved.

 INDI Qt Client

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Library General Public
 License version 2 as published by the Free Software Foundation.

 This library is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 Library General Public License for more details.

 You should have received a copy of the GNU Library General Public License
 along with this library; see the file COPYING.LIB.  If not, write to
 the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 Boston, MA 02110-1301, USA.
*******************************************************************************/

#include "baseclientqt.h"
#include "baseclientqt_p.h"

#include "abstractbaseclient.h"
#include "abstractbaseclient_p.h"
namespace INDI
{

// BaseClientQtPrivate

BaseClientQtPrivate::BaseClientQtPrivate(BaseClientQt *parent)
    : AbstractBaseClientPrivate(parent)
{
    io.write = [](void *user, const void * ptr, size_t count) -> size_t
    {
        auto self = static_cast<BaseClientQtPrivate *>(static_cast<AbstractBaseClientPrivate *>(user));
        return self->clientSocket.write(static_cast<const char *>(ptr), count);
    };

    io.vprintf = [](void *user, const char * format, va_list ap) -> int
    {
        auto self = static_cast<BaseClientQtPrivate *>(static_cast<AbstractBaseClientPrivate *>(user));
        char message[MAXRBUF];
        vsnprintf(message, MAXRBUF, format, ap);
        return self->clientSocket.write(message, strlen(message));
    };
}


void BaseClientQtPrivate::listenINDI()
{
    char msg[MAXRBUF];

    if (sConnected == false)
        return;

    while (clientSocket.bytesAvailable() > 0)
    {
        const QByteArray data = clientSocket.readAll();
        
        auto documents = xmlParser.parseChunk(data.constData(), data.size());

        if (documents.size() == 0)
        {
            if (xmlParser.hasErrorMessage())
            {
                IDLog("Bad XML from %s/%d: %s\n%.*s\n", cServer.c_str(), cPort, xmlParser.errorMessage(), data.size(), data.constData());
            }
            break;
        }

        for (const auto &doc: documents)
        {
            LilXmlElement root = doc.root();

            if (verbose)
                    root.print(stderr, 0);

            int err_code = dispatchCommand(root, msg);

            if (err_code < 0)
            {
                // Silenty ignore property duplication errors
                if (err_code != INDI_PROPERTY_DUPLICATED)
                {
                    IDLog("Dispatch command error(%d): %s\n", err_code, msg);
                    root.print(stderr, 0);
                }
            }
        }
    }
}

// BaseClientQt

BaseClientQt::BaseClientQt(QObject *parent)
    : QObject(parent)
    , AbstractBaseClient(std::unique_ptr<AbstractBaseClientPrivate>(new BaseClientQtPrivate(this)))
{
    D_PTR(BaseClientQt);
    connect(&d->clientSocket, &QTcpSocket::readyRead, this, [d]()
    {
        d->listenINDI();
    });

    connect(&d->clientSocket, &QTcpSocket::errorOccurred, this, [d, this](QAbstractSocket::SocketError socketError)
    {
        if (d->sConnected == false)
            return;

        // TODO Handle what happens on socket failure!
        INDI_UNUSED(socketError);
        IDLog("Socket Error: %s\n", d->clientSocket.errorString().toLatin1().constData());
        fprintf(stderr, "INDI server %s/%d disconnected.\n", d->cServer.c_str(), d->cPort);
        d->clientSocket.close();
        // Let client handle server disconnection
        serverDisconnected(-1);
    });
}

BaseClientQt::~BaseClientQt()
{
    D_PTR(BaseClientQt);
    d->clear();
}

bool BaseClientQt::connectServer()
{
    D_PTR(BaseClientQt);
    d->clientSocket.connectToHost(d->cServer.c_str(), d->cPort);

    if (d->clientSocket.waitForConnected(d->timeout_sec * 1000) == false)
    {
        d->sConnected = false;
        return false;
    }

    d->clear();

    d->sConnected = true;

    serverConnected();

    d->userIoGetProperties();

    return true;
}

bool BaseClientQt::disconnectServer()
{
    D_PTR(BaseClientQt);

    if (d->sConnected == false)
        return true;

    d->sConnected = false;

    d->clientSocket.close();

    d->clear();

    d->watchDevice.unwatchDevices();

    serverDisconnected(0);

    return true;
}

}
