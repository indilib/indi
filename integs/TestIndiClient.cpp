/*******************************************************************************
  Copyright(c) 2022 Ludovic Pollet. All rights reserved.

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

#include <stdexcept>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <system_error>
#include <thread>

#include "gtest/gtest.h"

#include "basedevice.h"
#include "baseclient.h"
#include "indiproperty.h"

#include "utils.h"

#include "ServerMock.h"
#include "IndiClientMock.h"

#define TEST_TCP_PORT 17624
#define TEST_UNIX_SOCKET "/tmp/indi-test-server"
#define STRINGIFY_TOK(x) #x
#define TO_STRING(x) STRINGIFY_TOK(x)

class MyClient : public INDI::BaseClient
{
        std::string dev, prop;
    public:
        MyClient(const std::string &dev, const std::string &prop) : dev(dev), prop(prop) {}
        virtual ~MyClient() {}
    protected:
        virtual void newDevice(INDI::BaseDevice) override
        {
            std::cerr << "new device\n";
        }
        virtual void removeDevice(INDI::BaseDevice) override
        {
            std::cerr << "remove device\n";
        }
        virtual void newProperty(INDI::Property) override
        {
        };
        virtual void removeProperty(INDI::Property)  override
        {
        }

        virtual void serverConnected()  override
        {
            std::cerr << "server connected\n";
        }
        virtual void serverDisconnected(int)  override
        {
            std::cerr << "server disconnected\n";
        }
};

TEST(IndiclientTcpConnect, ClientConnect)
{
    ServerMock fakeServer;
    IndiClientMock indiServerCnx;

    setupSigPipe();

    fakeServer.listen(TEST_TCP_PORT);

    MyClient * client = new MyClient("machin", "truc");
    client->setServer("127.0.0.1", TEST_TCP_PORT);

    // FIXME : async
    // auto wait = runAsync([&fakeServer]=>{
    std::thread t1([&fakeServer, &indiServerCnx]()
    {
        fakeServer.accept(indiServerCnx);
        indiServerCnx.cnx.expectXml("<getProperties version='1.7'/>");
    });
    // });
    bool connected = client->connectServer();
    ASSERT_EQ(connected, true);

    t1.join();

    // Check client reply to ping...
    indiServerCnx.cnx.send("<pingRequest uid='123456'/>");
    indiServerCnx.cnx.expectXml("<pingReply uid='123456'/>");
}
