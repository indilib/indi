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
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "utils.h"
#include "IndiServerController.h"


#define TEST_TCP_PORT 17624
#define TEST_UNIX_SOCKET "/tmp/indi-test-server"
#define TEST_INDI_FIFO "/tmp/indi-test-fifo"
#define STRINGIFY_TOK(x) #x
#define TO_STRING(x) STRINGIFY_TOK(x)

IndiServerController::IndiServerController() {
    fifo = false;
}

IndiServerController::~IndiServerController() {
    // Abort any pending indi server
    kill();
    join();
}


void IndiServerController::setFifo(bool fifo) {
    this->fifo = fifo;
}

void IndiServerController::start(const std::vector<std::string> & args) {
    ProcessController::start("../indiserver/indiserver", args);
}

void IndiServerController::startDriver(const std::string & path) {
    std::vector<std::string> args = { "-p", TO_STRING(TEST_TCP_PORT), "-r", "0", "-vvv" };
#ifdef ENABLE_INDI_SHARED_MEMORY
    args.push_back("-u");
    args.push_back(TEST_UNIX_SOCKET);
#endif

    if (fifo) {
        unlink(TEST_INDI_FIFO);
        if (mkfifo(TEST_INDI_FIFO, 0600) == -1) {
            throw std::system_error(errno, std::generic_category(), "mkfifo");
        }
        args.push_back("-f");
        args.push_back(TEST_INDI_FIFO);
    }
    args.push_back(path);

    start(args);
}

void IndiServerController::addDriver(const std::string & driver) {
    if (!fifo) {
        throw new std::runtime_error("Fifo is not enabled - cannot add driver");
    }

    int fifoFd = open(TEST_INDI_FIFO, O_WRONLY);
    if (fifoFd == -1) {
        throw std::system_error(errno, std::generic_category(), "opening fifo");
    }

    std::string cmd = "start " + driver +"\n";
    int wr = write(fifoFd, cmd.data(), cmd.length());
    if (wr == -1) {
        auto e = errno;
        close(fifoFd);
        throw std::system_error(e, std::generic_category(), "write to fifo");
    }

    close(fifoFd);
}

std::string IndiServerController::getUnixSocketPath() const {
    return TEST_UNIX_SOCKET;
}

int IndiServerController::getTcpPort() const {
  return TEST_TCP_PORT;
}

