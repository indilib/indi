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

#include "gtest/gtest.h"

#include "utils.h"

#include "SharedBuffer.h"
#include "DriverMock.h"
#include "IndiServerController.h"
#include "IndiClientMock.h"

#define PROP_COUNT 5

static void driverSendsProps(DriverMock & fakeDriver) {
    fprintf(stderr, "Driver sends properties\n");
    for(int i = 0; i < PROP_COUNT; ++i) {
        fakeDriver.cnx.send("<defNumberVector device='fakedev1' name='testnumber" + std::to_string(i) + "' label='test label' group='test_group' state='Idle' perm='rw' timeout='100' timestamp='2018-01-01T00:00:00'>\n");
        fakeDriver.cnx.send("<defNumber name='content' label='content' min='0' max='100' step='1'>50</defNumber>\n");
        fakeDriver.cnx.send("</defNumberVector>\n");
    }
}

static void clientReceivesProps(IndiClientMock & indiClient) {
    fprintf(stderr, "Client reveives properties\n");
    for(int i = 0; i < PROP_COUNT; ++i) {
        indiClient.cnx.expectXml("<defNumberVector device='fakedev1' name='testnumber" + std::to_string(i) + "' label='test label' group='test_group' state='Idle' perm='rw' timeout='100' timestamp='2018-01-01T00:00:00'>");
        indiClient.cnx.expectXml("<defNumber name='content' label='content' min='0' max='100' step='1'>");
        indiClient.cnx.expect("\n50");
        indiClient.cnx.expectXml("</defNumber>");
        indiClient.cnx.expectXml("</defNumberVector>");
    }
}


static void startFakeDev1(IndiServerController & indiServer, DriverMock & fakeDriver) {
    setupSigPipe();

    fakeDriver.setup();

    std::string fakeDriverPath = getTestExePath("fakedriver");

    // Start indiserver with one instance, repeat 0
    indiServer.startDriver(fakeDriverPath);
    fprintf(stderr, "indiserver started\n");

    fakeDriver.waitEstablish();
    fprintf(stderr, "fake driver started\n");

    fakeDriver.cnx.expectXml("<getProperties version='1.7'/>");
    fprintf(stderr, "getProperties received\n");

    driverSendsProps(fakeDriver);
}

static void connectFakeDev1Client(IndiServerController &, DriverMock & fakeDriver, IndiClientMock & indiClient) {
    fprintf(stderr, "Client asks properties\n");
    indiClient.cnx.send("<getProperties version='1.7'/>\n");
    fakeDriver.cnx.expectXml("<getProperties version='1.7'/>");

    driverSendsProps(fakeDriver);
    fprintf(stderr, "Driver sends properties\n");

    fprintf(stderr, "Client receive properties\n");
    clientReceivesProps(indiClient);
}

TEST(TestClientQueries, ServerForwardRequest) {
    DriverMock fakeDriver;
    IndiServerController indiServer;

    startFakeDev1(indiServer, fakeDriver);

    IndiClientMock indiClient;

    indiClient.connect(indiServer);

    connectFakeDev1Client(indiServer, fakeDriver, indiClient);

    indiClient.cnx.send("<newNumberVector device='fakedev1' name='testnumber' timestamp='2018-01-01T00:00:00'>");
    indiClient.cnx.send("<oneNumber name='content' > 51 </oneNumber>");
    indiClient.cnx.send("</newNumberVector>");

    fakeDriver.cnx.expectXml("<newNumberVector device='fakedev1' name='testnumber' timestamp='2018-01-01T00:00:00'>");
    fakeDriver.cnx.expectXml("<oneNumber name='content'>");
    fakeDriver.cnx.expect("\n51");
    fakeDriver.cnx.expectXml("</oneNumber>");
    fakeDriver.cnx.expectXml("</newNumberVector>");

    fakeDriver.terminateDriver();
    // Exit code 1 is expected when driver stopped
    indiServer.waitProcessEnd(1);

}

TEST(TestClientQueries, ServerForwardRequestOfHalfDeadClient) {
    DriverMock fakeDriver;
    IndiServerController indiServer;

    startFakeDev1(indiServer, fakeDriver);

    IndiClientMock indiClient;

    indiClient.connect(indiServer);

    connectFakeDev1Client(indiServer, fakeDriver, indiClient);

    indiClient.cnx.send("<getProperties version='1.7'/>\n");
    fakeDriver.cnx.expectXml("<getProperties version='1.7'/>");

    driverSendsProps(fakeDriver);

    // client writes before receiving any data
    indiClient.cnx.shutdown(true, false);

    // Make sure the server sees the client shutdown. Get a full interaction with it
    fakeDriver.cnx.send("<pingRequest uid='1'/>\n");
    fakeDriver.cnx.expectXml("<pingReply uid='1'/>");

    indiClient.cnx.send("<newNumberVector device='fakedev1' name='testnumber' timestamp='2018-01-01T00:00:00'>");
    indiClient.cnx.send("<oneNumber name='content' > 51 </oneNumber>");
    indiClient.cnx.send("</newNumberVector>");


    fakeDriver.cnx.expectXml("<newNumberVector device='fakedev1' name='testnumber' timestamp='2018-01-01T00:00:00'>");
    fakeDriver.cnx.expectXml("<oneNumber name='content'>");
    fakeDriver.cnx.expect("\n51");
    fakeDriver.cnx.expectXml("</oneNumber>");
    fakeDriver.cnx.expectXml("</newNumberVector>");

    indiClient.close();

    fakeDriver.terminateDriver();
    // Exit code 1 is expected when driver stopped
    indiServer.waitProcessEnd(1);
}
