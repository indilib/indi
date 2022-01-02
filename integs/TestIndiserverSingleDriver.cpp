#include <stdexcept>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <system_error>

#include "gtest/gtest.h"

#include "utils.h"

#include "FakeDriverCnx.h"
#include "IndiServerCnx.h"


TEST(IndiserverSingleDriver, MissingDriver) {
    FakeDriverCnx fakeDriver;
    IndiServerCnx indiServer;

    fprintf(stderr, "Testing indiserver running one missing driver from cmd line");

    setupSigPipe();

    // Start indiserver with one instance, repeat 0
    const char * args[] = { "-r", "0", "./fakedriver-not-existing", nullptr };
    indiServer.start(args);
    fprintf(stderr, "indiserver started\n");

    // Exit code 1 is expected when driver stopped
    indiServer.waitProcessEnd(1);
}

TEST(IndiserverSingleDriver, StartOnce) {
    FakeDriverCnx fakeDriver;
    IndiServerCnx indiServer;

    fprintf(stderr, "Testing indiserver running one driver from cmd line");

    setupSigPipe();

    fakeDriver.setup();

    // Start indiserver with one instance, repeat 0
    const char * args[] = { "-r", "0", "./fakedriver", nullptr };
    indiServer.start(args);
    fprintf(stderr, "indiserver started\n");

    fakeDriver.waitEstablish();
    fprintf(stderr, "fake driver started\n");

    fakeDriver.expect("<getProperties version='1.7'/>\n");
    fprintf(stderr, "getProperties received");

    fakeDriver.send("<defBLOBVector device='fakedev1' name='testblob' label='test label' group='test_group' state='Idle' perm='ro' timeout='100' timestamp='2018-01-01T00:00:00'>\n");
    fakeDriver.send("  <defBLOB name='content' label='content'/>\n");
    fakeDriver.send("</defBLOBVector>\n");

    fakeDriver.terminate();

    // Exit code 1 is expected when driver stopped
    indiServer.waitProcessEnd(1);
}

