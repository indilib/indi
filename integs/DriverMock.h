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

#ifndef DRIVER_MOCK_H_
#define DRIVER_MOCK_H_ 1

#include <string>
#include <unistd.h>

#include "ConnectionMock.h"

/**
 * Interface to the the fake driver that forward its indi communication pipes
 * to the test process.
 *
 */
class DriverMock
{
        int driverConnection;

        int driverFds[2];
    public:
        DriverMock();
        virtual ~DriverMock();

        // Start the listening socket that will receive driver upon their starts
        void setup();

        void waitEstablish();

        void terminateDriver();

        void ping();

        ConnectionMock cnx;
};


#endif // DRIVER_MOCK_H_