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
    driverConnection = socketAccept(serverConnection);
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


void DriverMock::ping() {
    cnx.send("<pingRequest uid='flush'/>\n");
    cnx.expectXml("<pingReply uid=\"flush\"/>");
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
