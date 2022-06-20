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
#include <unistd.h>

#include "IndiClientMock.h"
#include "utils.h"

IndiClientMock::IndiClientMock()
{
    fd = -1;
}

IndiClientMock::~IndiClientMock()
{
    close();
}

void IndiClientMock::close()
{
    if (fd != -1) ::close(fd);
    cnx.setFds(-1, -1);
}

void IndiClientMock::connectUnix(const std::string &path)
{
    fd = unixSocketConnect(path);
    cnx.setFds(fd, fd);
}

void IndiClientMock::connectTcp(const std::string &host, int port)
{
    fd = tcpSocketConnect(host, port, false);
    cnx.setFds(fd, fd);
}

void IndiClientMock::associate(int fd)
{
    this->fd = fd;
    cnx.setFds(fd, fd);
}

void IndiClientMock::ping()
{
    cnx.send("<pingRequest uid='flush'/>\n");
    cnx.expectXml("<pingReply uid='flush'/>\n");
}
