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

#ifndef SERVER_MOCK_H_
#define SERVER_MOCK_H_ 1

#include <string>
#include <unistd.h>

class IndiClientMock;

/**
 * Instantiate a fake indi server
 */
class ServerMock
{
        int fd;

    public:
        ServerMock();
        virtual ~ServerMock();

        // Start the listening socket that will receive driver upon their starts
        void listen(int tcpPort);
        void listen(const std::string &unixPath);

        void accept(IndiClientMock &into);

        void close();
};


#endif // SERVER_MOCK_H_