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

#include <string>

void setupSigPipe();

int unixSocketListen(const std::string & path);
int unixSocketConnect(const std::string & unixAddr, bool failAllowed = false);
void unixSocketSendFds(int fd, int count, int * fds);
void unixSocketRecvFds(int fd, int count, int * fds);

int tcpSocketListen(int port);
int tcpSocketConnect(const std::string & unixAddr, int port, bool failAllowed = false);

int socketAccept(int fd);

std::string getTestExePath(const std::string & basename);
