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
#include "ProcessController.h"
#include "IndiClientMock.h"

// Having a large number of properties ensures cases with buffer not empty on exits occur on server side
#define PROP_COUNT 100

static void startIndiSetProp(ProcessController & indiSetProp, const std::vector<std::string>& args) {
    setupSigPipe();
    std::string indiSetPropPath = getTestExePath("../indi_setprop");

    indiSetProp.start(indiSetPropPath, args);
}

static void driverIsAskedProps(DriverMock & fakeDriver) {
    fakeDriver.cnx.expectXml("<getProperties version='1.7'/>");
    fprintf(stderr, "getProperties received\n");

    for(int i = 0; i < PROP_COUNT; ++i) {
        fakeDriver.cnx.send("<defNumberVector device='fakedev1' name='testnumber" + std::to_string(i) + "' label='test label' group='test_group' state='Idle' perm='rw' timeout='100' timestamp='2018-01-01T00:00:00'>\n");
        fakeDriver.cnx.send("<defNumber name='content' label='content' min='0' max='100' step='1'>50</defNumber>\n");
        fakeDriver.cnx.send("</defNumberVector>\n");
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

    driverIsAskedProps(fakeDriver);
}


TEST(TestIndiSetProperties, SetFirstPropertyUntyped) {
    DriverMock fakeDriver;
    IndiServerController indiServer;

    startFakeDev1(indiServer, fakeDriver);

    ProcessController indiSetProp;
    startIndiSetProp(indiSetProp, { "-p", std::to_string(indiServer.getTcpPort()), "-v", "fakedev1.testnumber0.content=8" });

    driverIsAskedProps(fakeDriver);

    indiSetProp.join();
    indiSetProp.expectExitCode(0);

    // // Expect the query to come
    fakeDriver.cnx.expectXml("<newNumberVector device='fakedev1' name='testnumber0'>");
    fakeDriver.cnx.expectXml("<oneNumber name='content'>");
    fakeDriver.cnx.expect("\n8");
    fakeDriver.cnx.expectXml("</oneNumber>");
    fakeDriver.cnx.expectXml("</newNumberVector>");

    // Close properly
    fakeDriver.terminateDriver();
    // Exit code 1 is expected when driver stopped
    indiServer.waitProcessEnd(1);

}

TEST(TestIndiSetProperties, SetFirstPropertyTyped) {
    DriverMock fakeDriver;
    IndiServerController indiServer;

    startFakeDev1(indiServer, fakeDriver);

    ProcessController indiSetProp;
    startIndiSetProp(indiSetProp, { "-p", std::to_string(indiServer.getTcpPort()), "-v", "-n", "fakedev1.testnumber0.content=8" });

    indiSetProp.join();
    indiSetProp.expectExitCode(0);

    // // Expect the query to come
    fakeDriver.cnx.expectXml("<newNumberVector device='fakedev1' name='testnumber0'>");
    fakeDriver.cnx.expectXml("<oneNumber name='content'>");
    fakeDriver.cnx.expect("\n8");
    fakeDriver.cnx.expectXml("</oneNumber>");
    fakeDriver.cnx.expectXml("</newNumberVector>");

    // Close properly
    fakeDriver.terminateDriver();
    // Exit code 1 is expected when driver stopped
    indiServer.waitProcessEnd(1);

}

TEST(TestIndiSetProperties, SetLastProperty) {
    DriverMock fakeDriver;
    IndiServerController indiServer;

    startFakeDev1(indiServer, fakeDriver);

    ProcessController indiSetProp;
    startIndiSetProp(indiSetProp, { "-p", std::to_string(indiServer.getTcpPort()), "-v", "fakedev1.testnumber" + std::to_string(PROP_COUNT - 1) + ".content=8" });

    driverIsAskedProps(fakeDriver);

    indiSetProp.join();
    indiSetProp.expectExitCode(0);

    // // Expect the query to come
    fakeDriver.cnx.expectXml("<newNumberVector device='fakedev1' name='testnumber" + std::to_string(PROP_COUNT - 1) + "'>");
    fakeDriver.cnx.expectXml("<oneNumber name='content'>");
    fakeDriver.cnx.expect("\n8");
    fakeDriver.cnx.expectXml("</oneNumber>");
    fakeDriver.cnx.expectXml("</newNumberVector>");

    // Close properly
    fakeDriver.terminateDriver();
    // Exit code 1 is expected when driver stopped
    indiServer.waitProcessEnd(1);

}

TEST(TestIndiSetProperties, SetLastPropertyTyped) {
    DriverMock fakeDriver;
    IndiServerController indiServer;

    startFakeDev1(indiServer, fakeDriver);

    ProcessController indiSetProp;
    startIndiSetProp(indiSetProp, { "-p", std::to_string(indiServer.getTcpPort()), "-v", "-n", "fakedev1.testnumber" + std::to_string(PROP_COUNT - 1) + ".content=8" });

    indiSetProp.join();
    indiSetProp.expectExitCode(0);

    // // Expect the query to come
    fakeDriver.cnx.expectXml("<newNumberVector device='fakedev1' name='testnumber" + std::to_string(PROP_COUNT - 1) + "'>");
    fakeDriver.cnx.expectXml("<oneNumber name='content'>");
    fakeDriver.cnx.expect("\n8");
    fakeDriver.cnx.expectXml("</oneNumber>");
    fakeDriver.cnx.expectXml("</newNumberVector>");

    // Close properly
    fakeDriver.terminateDriver();
    // Exit code 1 is expected when driver stopped
    indiServer.waitProcessEnd(1);

}
