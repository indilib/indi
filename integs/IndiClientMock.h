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

#ifndef INDI_CLIENT_MOCK_H_
#define INDI_CLIENT_MOCK_H_ 1

#include <string>

#include "ConnectionMock.h"

/**
 * Interface to a mocked connection to indi server
 */
class IndiClientMock {
    int fd;
public:
    ConnectionMock cnx;
    IndiClientMock();
    virtual ~IndiClientMock();
    void connectUnix(const std::string & path= "/tmp/indiserver");
    void connectTcp(const std::string & host = "127.0.0.1", int port = 7624);
    void associate(int fd);

    // This ensure that previous orders were received
    void ping();

    void close();
};


#endif // INDI_CLIENT_MOCK_H_