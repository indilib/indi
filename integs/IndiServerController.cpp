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
#include <sys/wait.h>

#include "utils.h"
#include "IndiServerController.h"


#define TEST_TCP_PORT 17624
#define TEST_UNIX_SOCKET "/tmp/indi-test-server"
#define STRINGIFY_TOK(x) #x
#define TO_STRING(x) STRINGIFY_TOK(x)

void IndiServerController::start(const std::vector<std::string> & args) {
    ProcessController::start("../indiserver/indiserver", args);
}

void IndiServerController::startDriver(const std::string & path) {
    std::vector<std::string> args = { "-p", TO_STRING(TEST_TCP_PORT), "-r", "0", "-vvv" };
#ifdef ENABLE_INDI_SHARED_MEMORY
    args.push_back("-u");
    args.push_back(TEST_UNIX_SOCKET);
#endif
    args.push_back(path);

    start(args);
}

std::string IndiServerController::getUnixSocketPath() const {
    return TEST_UNIX_SOCKET;
}

int IndiServerController::getTcpPort() const {
  return TEST_TCP_PORT;
}

