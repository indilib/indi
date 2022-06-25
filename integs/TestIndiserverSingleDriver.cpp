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

// Repeat blob operation for more stress
#define BLOB_REPEAT_COUNT 5

TEST(IndiserverSingleDriver, MissingDriver)
{
    DriverMock fakeDriver;
    IndiServerController indiServer;

    setupSigPipe();

    std::string fakeDriverPath = getTestExePath("fakedriver-not-existing");

    // Start indiserver with one instance, repeat 0
    indiServer.startDriver(fakeDriverPath);
    fprintf(stderr, "indiserver started\n");

    // Exit code 1 is expected when driver stopped
    indiServer.waitProcessEnd(1);
}

TEST(IndiserverSingleDriver, ReplyToPing)
{
    DriverMock fakeDriver;
    IndiServerController indiServer;

    setupSigPipe();

    fakeDriver.setup();

    std::string fakeDriverPath = getTestExePath("fakedriver");

    // Start indiserver with one instance, repeat 0
    indiServer.startDriver(fakeDriverPath);
    fprintf(stderr, "indiserver started\n");

    fakeDriver.waitEstablish();
    fprintf(stderr, "fake driver started\n");

    fakeDriver.cnx.expectXml("<getProperties version='1.7'/>");
    fprintf(stderr, "getProperties received");

    // Establish a client & send ping
    IndiClientMock client;
    client.connect(indiServer);

    // Send ping from driver
    fakeDriver.cnx.send("<pingRequest uid='1'/>\n");
    fakeDriver.cnx.expectXml("<pingReply uid='1'/>");

    client.cnx.send("<pingRequest uid='2'/>\n");
    client.cnx.expectXml("<pingReply uid='2'/>\n");

    fakeDriver.cnx.send("<pingRequest uid='3'/>\n");
    fakeDriver.cnx.expectXml("<pingReply uid='3'/>\n");

    client.cnx.send("<pingRequest uid='4'/>\n");
    client.cnx.expectXml("<pingReply uid='4'/>\n");

    fakeDriver.terminateDriver();

    // Exit code 1 is expected when driver stopped
    indiServer.waitProcessEnd(1);
}


void startFakeDev1(IndiServerController &indiServer, DriverMock &fakeDriver)
{
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

    // Give one props to the driver
    fakeDriver.cnx.send("<defBLOBVector device='fakedev1' name='testblob' label='test label' group='test_group' state='Idle' perm='ro' timeout='100' timestamp='2018-01-01T00:00:00'>\n");
    fakeDriver.cnx.send("<defBLOB name='content' label='content'/>\n");
    fakeDriver.cnx.send("</defBLOBVector>\n");
}

void connectFakeDev1Client(IndiServerController &, DriverMock &fakeDriver, IndiClientMock &indiClient)
{
    fprintf(stderr, "Client asks properties\n");
    indiClient.cnx.send("<getProperties version='1.7'/>\n");
    fakeDriver.cnx.expectXml("<getProperties version='1.7'/>");

    fprintf(stderr, "Driver sends properties\n");
    fakeDriver.cnx.send("<defBLOBVector device='fakedev1' name='testblob' label='test label' group='test_group' state='Idle' perm='ro' timeout='100' timestamp='2018-01-01T00:00:00'>\n");
    fakeDriver.cnx.send("<defBLOB name='content' label='content'/>\n");
    fakeDriver.cnx.send("</defBLOBVector>\n");

    fprintf(stderr, "Client receive properties\n");
    indiClient.cnx.expectXml("<defBLOBVector device=\"fakedev1\" name=\"testblob\" label=\"test label\" group=\"test_group\" state=\"Idle\" perm=\"ro\" timeout=\"100\" timestamp=\"2018-01-01T00:00:00\">");
    indiClient.cnx.expectXml("<defBLOB name=\"content\" label=\"content\"/>");
    indiClient.cnx.expectXml("</defBLOBVector>");
}

TEST(IndiserverSingleDriver, DontLeakFds)
{
    DriverMock fakeDriver;
    IndiServerController indiServer;

    startFakeDev1(indiServer, fakeDriver);

    IndiClientMock indiClient;

    fakeDriver.ping();
    int fdCountIdle = indiServer.getOpenFdCount();
#ifdef ENABLE_INDI_SHARED_MEMORY
    indiClient.connectUnix(indiServer.getUnixSocketPath());
    fakeDriver.ping();
    indiServer.checkOpenFdCount(fdCountIdle + 1, "First unix connection");
    indiClient.close();

    // Make sur the server processed the close as well.
    fakeDriver.ping();
    indiServer.checkOpenFdCount(fdCountIdle, "First unix connection released");

    indiClient.connectUnix(indiServer.getUnixSocketPath());
    fakeDriver.ping();
    indiServer.checkOpenFdCount(fdCountIdle + 1, "Second unix connection");
    indiClient.close();

    fakeDriver.ping();
    indiServer.checkOpenFdCount(fdCountIdle, "Second unix connection released");

#endif // ENABLE_INDI_SHARED_MEMORY

    indiClient.connectTcp("127.0.0.1", indiServer.getTcpPort());
    fakeDriver.ping();
    indiServer.checkOpenFdCount(fdCountIdle + 1, "First tcp connection");
    indiClient.close();

    fakeDriver.ping();
    indiServer.checkOpenFdCount(fdCountIdle, "First tcp connection released");

    indiClient.connectTcp("127.0.0.1", indiServer.getTcpPort());
    fakeDriver.ping();
    indiServer.checkOpenFdCount(fdCountIdle + 1, "Second tcp connection");
    indiClient.close();

    fakeDriver.ping();
    indiServer.checkOpenFdCount(fdCountIdle, "First tcp connection released");
}


TEST(IndiserverSingleDriver, DontForwardUnaskedBlobDefToClient)
{
    DriverMock fakeDriver;
    IndiServerController indiServer;

    startFakeDev1(indiServer, fakeDriver);

    IndiClientMock indiClient;

    indiClient.connect(indiServer);

    connectFakeDev1Client(indiServer, fakeDriver, indiClient);

    fprintf(stderr, "Driver send new blob value\n");
    fakeDriver.cnx.send("<setBLOBVector device='fakedev1' name='testblob' timestamp='2018-01-01T00:01:00'>\n");
    fakeDriver.cnx.send("<oneBLOB name='content' size='21' format='.fits' enclen='29'>\n");
    fakeDriver.cnx.send("MDEyMzQ1Njc4OTAxMjM0NTY3ODkK\n");
    fakeDriver.cnx.send("</oneBLOB>\n");
    fakeDriver.cnx.send("</setBLOBVector>\n");
    fakeDriver.ping();

    fprintf(stderr, "Client don't receive blob\n");
    indiClient.ping();

    fakeDriver.terminateDriver();
    // Exit code 1 is expected when driver stopped
    indiServer.waitProcessEnd(1);
}

TEST(IndiserverSingleDriver, DontForwardOtherBlobDefToClient)
{
    DriverMock fakeDriver;
    IndiServerController indiServer;

    startFakeDev1(indiServer, fakeDriver);

    IndiClientMock indiClient;

    indiClient.connect(indiServer);

    connectFakeDev1Client(indiServer, fakeDriver, indiClient);

    fprintf(stderr, "Client ask blobs\n");
    indiClient.cnx.send("<enableBLOB device='fakedev1' name='testblob2'>Also</enableBLOB>\n");
    indiClient.ping();


    fprintf(stderr, "Driver send new blob value\n");
    fakeDriver.cnx.send("<setBLOBVector device='fakedev1' name='testblob' timestamp='2018-01-01T00:01:00'>\n");
    fakeDriver.cnx.send("<oneBLOB name='content' size='21' format='.fits' enclen='29'>\n");
    fakeDriver.cnx.send("MDEyMzQ1Njc4OTAxMjM0NTY3ODkK\n");
    fakeDriver.cnx.send("</oneBLOB>\n");
    fakeDriver.cnx.send("</setBLOBVector>\n");
    fakeDriver.ping();

    fprintf(stderr, "Client don't receive blob\n");
    indiClient.ping();

    fakeDriver.terminateDriver();
    // Exit code 1 is expected when driver stopped
    indiServer.waitProcessEnd(1);
}

TEST(IndiserverSingleDriver, DropMisbehavingDriver)
{
    DriverMock fakeDriver;
    IndiServerController indiServer;

    startFakeDev1(indiServer, fakeDriver);

    IndiClientMock indiClient;

    indiClient.connect(indiServer);

    connectFakeDev1Client(indiServer, fakeDriver, indiClient);

    fprintf(stderr, "Client ask blobs\n");
    indiClient.cnx.send("<enableBLOB device='fakedev1' name='testblob'>Also</enableBLOB>\n");
    indiClient.ping();

    fprintf(stderr, "Driver send new blob value - without actual attachment\n");
    fakeDriver.cnx.send("<setBLOBVector device='fakedev1' name='testblob' timestamp='2018-01-01T00:01:00'>\n");
    fakeDriver.cnx.send("<oneBLOB name='content' size='21' format='.fits' attached='true'/>\n");
    fakeDriver.cnx.send("</setBLOBVector>\n");

    indiServer.waitProcessEnd(1);
}

TEST(IndiserverSingleDriver, ForwardBase64BlobToIPClient)
{
    // This tests decoding of base64 by driver
    DriverMock fakeDriver;
    IndiServerController indiServer;

    startFakeDev1(indiServer, fakeDriver);

    IndiClientMock indiClient;

    indiClient.connectTcp(indiServer);

    connectFakeDev1Client(indiServer, fakeDriver, indiClient);

    fprintf(stderr, "Client ask blobs\n");
    indiClient.cnx.send("<enableBLOB device='fakedev1' name='testblob'>Also</enableBLOB>\n");
    indiClient.ping();

    fprintf(stderr, "Driver send new blob value\n");
    fakeDriver.cnx.send("<setBLOBVector device='fakedev1' name='testblob' timestamp='2018-01-01T00:01:00'>\n");
    fakeDriver.cnx.send("<oneBLOB name='content' size='20' format='.fits' enclen='29'>\n");
    fakeDriver.cnx.send("MDEyMzQ1Njc4OTAxMjM0NTY3ODkK\n");
    fakeDriver.cnx.send("</oneBLOB>\n");
    fakeDriver.cnx.send("</setBLOBVector>\n");
    fakeDriver.ping();

    fprintf(stderr, "Client receive blob\n");
    indiClient.cnx.allowBufferReceive(true);
    indiClient.cnx.expectXml("<setBLOBVector device='fakedev1' name='testblob' timestamp='2018-01-01T00:01:00'>");
    indiClient.cnx.expectXml("<oneBLOB name='content' size='20' format='.fits' enclen='29'>");
    indiClient.cnx.expect("\nMDEyMzQ1Njc4OTAxMjM0NTY3ODkK");
    indiClient.cnx.expectXml("</oneBLOB>\n");
    indiClient.cnx.expectXml("</setBLOBVector>");

    fakeDriver.terminateDriver();
    // Exit code 1 is expected when driver stopped
    indiServer.waitProcessEnd(1);
}


#define DUMMY_BLOB_SIZE 64

#ifdef ENABLE_INDI_SHARED_MEMORY

TEST(IndiserverSingleDriver, ForwardBase64BlobToUnixClient)
{
    // This tests decoding of base64 by driver
    DriverMock fakeDriver;
    IndiServerController indiServer;

    startFakeDev1(indiServer, fakeDriver);

    IndiClientMock indiClient;

    indiClient.connectUnix(indiServer);

    connectFakeDev1Client(indiServer, fakeDriver, indiClient);

    fprintf(stderr, "Client ask blobs\n");
    indiClient.cnx.send("<enableBLOB device='fakedev1' name='testblob'>Also</enableBLOB>\n");
    indiClient.ping();

    fprintf(stderr, "Driver send new blob value\n");
    fakeDriver.cnx.send("<setBLOBVector device='fakedev1' name='testblob' timestamp='2018-01-01T00:01:00'>\n");
    fakeDriver.cnx.send("<oneBLOB name='content' size='20' format='.fits' enclen='29'>\n");
    fakeDriver.cnx.send("MDEyMzQ1Njc4OTAxMjM0NTY3ODkK\n");
    fakeDriver.cnx.send("</oneBLOB>\n");
    fakeDriver.cnx.send("</setBLOBVector>\n");
    fakeDriver.ping();

    fprintf(stderr, "Client receive blob\n");
    indiClient.cnx.allowBufferReceive(true);
    indiClient.cnx.expectXml("<setBLOBVector device='fakedev1' name='testblob' timestamp='2018-01-01T00:01:00'>");
    indiClient.cnx.expectXml("<oneBLOB name='content' size='20' format='.fits' attached='true'/>");
    indiClient.cnx.expectXml("</setBLOBVector>");

    SharedBuffer receivedFd;
    indiClient.cnx.expectBuffer(receivedFd);
    indiClient.cnx.allowBufferReceive(false);

    EXPECT_GE( receivedFd.getSize(), 20);

    fakeDriver.terminateDriver();
    // Exit code 1 is expected when driver stopped
    indiServer.waitProcessEnd(1);
}

void driverSendAttachedBlob(DriverMock &fakeDriver, ssize_t size)
{
    fprintf(stderr, "Driver send new blob value - without actual attachment\n");

    // Allocate more memory than asked (simulate kernel BSD kernel rounding up)
    ssize_t physical_size=0x10000;
    if (physical_size < size) {
        physical_size = size;
    }

    // The attachment must be done before EOF
    SharedBuffer fd;
    fd.allocate(physical_size);

    char * buffer = (char*)malloc(physical_size);
    for(auto i = 0; i < physical_size; ++i)
    {
        buffer[i] = {(char)('0' + (i % 10))};
    }
    fd.write(buffer, 0, physical_size);
    free(buffer);

    fakeDriver.cnx.send("<setBLOBVector device='fakedev1' name='testblob' timestamp='2018-01-01T00:01:00'>\n");
    fakeDriver.cnx.send("<oneBLOB name='content' size='" + std::to_string(size) + "' format='.fits' attached='true'/>\n", fd);
    fakeDriver.cnx.send("</setBLOBVector>");

    fd.release();

/*#else
    fakeDriver.cnx.send("<setBLOBVector device='fakedev1' name='testblob' timestamp='2018-01-01T00:01:00'>\n");
    fakeDriver.cnx.send("<oneBLOB name='content' size='" + std::to_string(size) + "' format='.fits'>\n");
    fakeDriver.cnx.send("MDEyMzQ1Njc4OTAxMjM0NTY3ODkwMTIzNDU2Nzg5MDE=\n");
    fakeDriver.cnx.send("</oneBLOB>\n");
    fakeDriver.cnx.send("</setBLOBVector>");
#endif
*/

    fakeDriver.ping();
}

TEST(IndiserverSingleDriver, ForwardAttachedBlobToUnixClient)
{
    // This tests attached blob pass through
    DriverMock fakeDriver;
    IndiServerController indiServer;

    startFakeDev1(indiServer, fakeDriver);

    IndiClientMock indiClient;

    indiClient.connectUnix(indiServer);

    connectFakeDev1Client(indiServer, fakeDriver, indiClient);

    fprintf(stderr, "Client ask blobs\n");
    indiClient.cnx.send("<enableBLOB device='fakedev1' name='testblob'>Also</enableBLOB>\n");
    // This ping ensures enableBLOB is handled before the blob is received

    for(int i = 0; i < BLOB_REPEAT_COUNT; ++i) {
        indiClient.ping();


        ssize_t size = 32;
        driverSendAttachedBlob(fakeDriver, size);

        // Now receive on client side
        fprintf(stderr, "Client receive blob\n");
        indiClient.cnx.allowBufferReceive(true);
        indiClient.cnx.expectXml("<setBLOBVector device='fakedev1' name='testblob' timestamp='2018-01-01T00:01:00'>");
        indiClient.cnx.expectXml("<oneBLOB name='content' size='" + std::to_string(size) + "' format='.fits' attached='true'/>");
        indiClient.cnx.expectXml("</setBLOBVector>");

        SharedBuffer receivedFd;
        indiClient.cnx.expectBuffer(receivedFd);
        indiClient.cnx.allowBufferReceive(false);

        EXPECT_GE( receivedFd.getSize(), size);
    }

    fakeDriver.terminateDriver();
    // Exit code 1 is expected when driver stopped
    indiServer.waitProcessEnd(1);
}

TEST(IndiserverSingleDriver, ForwardAttachedBlobToIPClient)
{
    // This tests base64 encoding by server
    DriverMock fakeDriver;
    IndiServerController indiServer;

    startFakeDev1(indiServer, fakeDriver);

    IndiClientMock indiClient;

    indiClient.connectTcp(indiServer);

    connectFakeDev1Client(indiServer, fakeDriver, indiClient);

    fprintf(stderr, "Client ask blobs\n");
    indiClient.cnx.send("<enableBLOB device='fakedev1' name='testblob'>Also</enableBLOB>\n");

    for(int i = 0; i < BLOB_REPEAT_COUNT; ++i) {
        indiClient.ping();

        ssize_t size = 32;
        driverSendAttachedBlob(fakeDriver, size);

        // Now receive on client side
        fprintf(stderr, "Client receive blob\n");
        indiClient.cnx.expectXml("<setBLOBVector device='fakedev1' name='testblob' timestamp='2018-01-01T00:01:00'>");
        indiClient.cnx.expectXml("<oneBLOB name='content' size='" + std::to_string(size) + "' format='.fits'>");
        // FIXME: get this from size
        indiClient.cnx.expect("\nMDEyMzQ1Njc4OTAxMjM0NTY3ODkwMTIzNDU2Nzg5MDE=");
        indiClient.cnx.expectXml("</oneBLOB>");
        indiClient.cnx.expectXml("</setBLOBVector>");
    }
    fakeDriver.terminateDriver();
    // Exit code 1 is expected when driver stopped
    indiServer.waitProcessEnd(1);
}

#endif
