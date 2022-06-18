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

#include <unistd.h>
#include <stdexcept>

#include "IndiClientMock.h"
#include "ServerMock.h"
#include "utils.h"


ServerMock::ServerMock() {
    fd = -1;
}

ServerMock::~ServerMock() {
    close();
}

void ServerMock::close() {
    if (fd != -1) {
        ::close(fd);
        fd = -1;
    }
}


// Start the listening socket that will receive driver upon their starts
void ServerMock::listen(int tcpPort)
{
    close();
    fd = tcpSocketListen(tcpPort);
}

void ServerMock::listen(const std::string & unixPath)
{
    close();
    fd = unixSocketListen(unixPath);
}

void ServerMock::accept(IndiClientMock & into)
{
    if (fd == -1) {
        throw std::runtime_error("Accept called on non listening server");
    }
    int child = socketAccept(fd);
    into.associate(child);
}
