#include <system_error>

#include "DriverMock.h"
#include "utils.h"

void DriverMock::setup() {
    // Create a socket that will not have the clo on exec flag ?
    abstractPath = "/tmp/fakedriver-test";
    setenv("FAKEDRIVER_ADDRESS", abstractPath.c_str(), 1);

    serverConnection = unixSocketListen(abstractPath);
}

void DriverMock::waitEstablish() {
    driverConnection = unixSocketAccept(serverConnection);
    unixSocketRecvFds(driverConnection, 2, driverFds);
    cnx.setFds(driverFds[0], driverFds[1]);
}

void DriverMock::terminateDriver() {
    cnx.setFds(-1, -1);
    if (driverConnection != -1) close(driverConnection);
    if (driverFds[0] != -1) close(driverFds[0]);
    if (driverFds[1] != -1) close(driverFds[1]);
    driverConnection = -1;
    driverFds[0] = -1;
    driverFds[1] = -1;
}

void DriverMock::unsetup() {
    if (serverConnection != -1) {
        close(serverConnection);
        serverConnection = -1;
    }
}

DriverMock::DriverMock() {
    serverConnection = -1;
    driverConnection = -1;
    driverFds[0] = -1;
    driverFds[1] = -1;
}

DriverMock::~DriverMock() {
    terminateDriver();
    unsetup();
}
