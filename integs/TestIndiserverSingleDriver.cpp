#include <stdexcept>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <system_error>

#include "gtest/gtest.h"

#include "utils.h"

#include "DriverMock.h"
#include "IndiServerController.h"
#include "IndiClientMock.h"


TEST(IndiserverSingleDriver, MissingDriver) {
    DriverMock fakeDriver;
    IndiServerController indiServer;

    setupSigPipe();

    // Start indiserver with one instance, repeat 0
    const char * args[] = { "-r", "0", "./fakedriver-not-existing", nullptr };
    indiServer.start(args);
    fprintf(stderr, "indiserver started\n");

    // Exit code 1 is expected when driver stopped
    indiServer.waitProcessEnd(1);
}

TEST(IndiserverSingleDriver, ReplyToPing) {
    DriverMock fakeDriver;
    IndiServerController indiServer;

    setupSigPipe();

    fakeDriver.setup();

    // Start indiserver with one instance, repeat 0
    const char * args[] = { "-r", "0", "./fakedriver", nullptr };
    indiServer.start(args);
    fprintf(stderr, "indiserver started\n");

    fakeDriver.waitEstablish();
    fprintf(stderr, "fake driver started\n");

    fakeDriver.cnx.expect("<getProperties version='1.7'/>\n");
    fprintf(stderr, "getProperties received");

    // Establish a client & send ping
    IndiClientMock client;
    client.connectUnix();

    // Send ping from driver
    fakeDriver.cnx.send("<serverPingRequest uid='1'/>\n");
    fakeDriver.cnx.expect("<serverPingReply uid=\"1\"/>\n");

    client.cnx.send("<serverPingRequest uid='2'/>\n");
    client.cnx.expect("<serverPingReply uid=\"2\"/>\n");

    fakeDriver.cnx.send("<serverPingRequest uid='3'/>\n");
    fakeDriver.cnx.expect("<serverPingReply uid=\"3\"/>\n");

    client.cnx.send("<serverPingRequest uid='4'/>\n");
    client.cnx.expect("<serverPingReply uid=\"4\"/>\n");

    fakeDriver.terminateDriver();

    // Exit code 1 is expected when driver stopped
    indiServer.waitProcessEnd(1);
}


void startFakeDev1(IndiServerController & indiServer, DriverMock & fakeDriver) {
    setupSigPipe();

    fakeDriver.setup();

    // Start indiserver with one instance, repeat 0
    const char * args[] = { "-vvv", "-r", "0", "./fakedriver", nullptr };
    indiServer.start(args);
    fprintf(stderr, "indiserver started\n");

    fakeDriver.waitEstablish();
    fprintf(stderr, "fake driver started\n");

    fakeDriver.cnx.expect("<getProperties version='1.7'/>\n");
    fprintf(stderr, "getProperties received\n");

    // Give one props to the driver
    fakeDriver.cnx.send("<defBLOBVector device='fakedev1' name='testblob' label='test label' group='test_group' state='Idle' perm='ro' timeout='100' timestamp='2018-01-01T00:00:00'>\n");
    fakeDriver.cnx.send("<defBLOB name='content' label='content'/>\n");
    fakeDriver.cnx.send("</defBLOBVector>\n");
}

void connectFakeDev1Client(IndiServerController & indiServer, DriverMock & fakeDriver, IndiClientMock & indiClient) {
    fprintf(stderr, "Client asks properties\n");
    indiClient.cnx.send("<getProperties version='1.7'/>\n");
    fakeDriver.cnx.expect("<getProperties version=\"1.7\"/>\n");

    fprintf(stderr, "Driver sends properties\n");
    fakeDriver.cnx.send("<defBLOBVector device='fakedev1' name='testblob' label='test label' group='test_group' state='Idle' perm='ro' timeout='100' timestamp='2018-01-01T00:00:00'>\n");
    fakeDriver.cnx.send("<defBLOB name='content' label='content'/>\n");
    fakeDriver.cnx.send("</defBLOBVector>\n");

    fprintf(stderr, "Client receive properties\n");
    indiClient.cnx.expect("<defBLOBVector device=\"fakedev1\" name=\"testblob\" label=\"test label\" group=\"test_group\" state=\"Idle\" perm=\"ro\" timeout=\"100\" timestamp=\"2018-01-01T00:00:00\">\n");
    indiClient.cnx.expect("    <defBLOB name=\"content\" label=\"content\"/>\n");
    indiClient.cnx.expect("</defBLOBVector>\n");
}

TEST(IndiserverSingleDriver, DontForwardUnaskedBlobDefToClient) {
    DriverMock fakeDriver;
    IndiServerController indiServer;

    startFakeDev1(indiServer, fakeDriver);

    IndiClientMock indiClient;

    indiClient.connectUnix();

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

TEST(IndiserverSingleDriver, DontForwardOtherBlobDefToClient) {
    DriverMock fakeDriver;
    IndiServerController indiServer;

    startFakeDev1(indiServer, fakeDriver);

    IndiClientMock indiClient;

    indiClient.connectUnix();

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

TEST(IndiserverSingleDriver, ForwardBlobValueToClient) {
    DriverMock fakeDriver;
    IndiServerController indiServer;

    startFakeDev1(indiServer, fakeDriver);

    IndiClientMock indiClient;

    indiClient.connectUnix();

    connectFakeDev1Client(indiServer, fakeDriver, indiClient);

    fprintf(stderr, "Client ask blobs\n");
    indiClient.cnx.send("<enableBLOB device='fakedev1' name='testblob'>Also</enableBLOB>\n");
    indiClient.ping();

    fprintf(stderr, "Driver send new blob value\n");
    fakeDriver.cnx.send("<setBLOBVector device='fakedev1' name='testblob' timestamp='2018-01-01T00:01:00'>\n");
    fakeDriver.cnx.send("<oneBLOB name='content' size='21' format='.fits' enclen='29'>\n");
    fakeDriver.cnx.send("MDEyMzQ1Njc4OTAxMjM0NTY3ODkK\n");
    fakeDriver.cnx.send("</oneBLOB>\n");
    fakeDriver.cnx.send("</setBLOBVector>\n");
    fakeDriver.ping();

    fprintf(stderr, "Client receive blob\n");
    indiClient.cnx.expectXml("<setBLOBVector device='fakedev1' name='testblob' timestamp='2018-01-01T00:01:00'>");
    indiClient.cnx.expectXml("<oneBLOB name='content' size='21' format='.fits' enclen='29'>");
    indiClient.cnx.expect("\nMDEyMzQ1Njc4OTAxMjM0NTY3ODkK\n");
    indiClient.cnx.expectXml("</oneBLOB>");
    indiClient.cnx.expectXml("</setBLOBVector>");

    fakeDriver.terminateDriver();
    // Exit code 1 is expected when driver stopped
    indiServer.waitProcessEnd(1);
}

