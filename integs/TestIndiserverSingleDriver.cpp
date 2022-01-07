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

    fprintf(stderr, "Testing indiserver running one missing driver from cmd line");

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

    fprintf(stderr, "Testing indiserver running one driver from cmd line");

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

TEST(IndiserverSingleDriver, ForwardToClient) {
    DriverMock fakeDriver;
    IndiServerController indiServer;

    fprintf(stderr, "Testing indiserver running one driver from cmd line");

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

    // Give one props to the driver
    fakeDriver.cnx.send("<defBLOBVector device='fakedev1' name='testblob' label='test label' group='test_group' state='Idle' perm='ro' timeout='100' timestamp='2018-01-01T00:00:00'>\n");
    fakeDriver.cnx.send("<defBLOB name='content' label='content'/>\n");
    fakeDriver.cnx.send("</defBLOBVector>\n");

    IndiClientMock indiClient;
    indiClient.connectUnix();

    printf("Client asks properties\n");
    indiClient.cnx.send("<getProperties version='1.7'/>\n");
    fakeDriver.cnx.expect("<getProperties version=\"1.7\"/>\n");

    printf("Driver sends properties\n");
    fakeDriver.cnx.send("<defBLOBVector device='fakedev1' name='testblob' label='test label' group='test_group' state='Idle' perm='ro' timeout='100' timestamp='2018-01-01T00:00:00'>\n");
    fakeDriver.cnx.send("<defBLOB name='content' label='content'/>\n");
    fakeDriver.cnx.send("</defBLOBVector>\n");

    printf("Client receive properties\n");
    indiClient.cnx.expect("<defBLOBVector device=\"fakedev1\" name=\"testblob\" label=\"test label\" group=\"test_group\" state=\"Idle\" perm=\"ro\" timeout=\"100\" timestamp=\"2018-01-01T00:00:00\">\n");
    indiClient.cnx.expect("    <defBLOB name=\"content\" label=\"content\"/>\n");
    indiClient.cnx.expect("</defBLOBVector>\n");

    fakeDriver.terminateDriver();

    // Exit code 1 is expected when driver stopped
    indiServer.waitProcessEnd(1);
}

