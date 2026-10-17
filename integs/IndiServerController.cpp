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


IndiServerController::IndiServerController() {
    fifo = false;
    // Use resources unique to this process so that tests can run in parallel
    unixSocketPath = uniqueTestName("/tmp/indi-test-server");
    fifoPath = uniqueTestName("/tmp/indi-test-fifo");
    tcpPort = findFreeTcpPort();
}

IndiServerController::~IndiServerController() {
    // Abort any pending indi server
    kill();
    join();
    if (fifo) {
        unlink(fifoPath.c_str());
    }
}


void IndiServerController::setFifo(bool fifo) {
    this->fifo = fifo;
}

void IndiServerController::start(const std::vector<std::string> & args) {
    ProcessController::start(INDISERVER_PATH, args);
}

void IndiServerController::startDriver(const std::string & path) {
    std::vector<std::string> args = { "-p", std::to_string(tcpPort), "-r", "0", "-vvv" };
#ifdef ENABLE_INDI_SHARED_MEMORY
    args.push_back("-u");
    args.push_back(unixSocketPath);
#endif

    if (fifo) {
        unlink(fifoPath.c_str());
        if (mkfifo(fifoPath.c_str(), 0600) == -1) {
            throw std::system_error(errno, std::generic_category(), "mkfifo");
        }
        args.push_back("-f");
        args.push_back(fifoPath);
    }
    args.push_back(path);

    start(args);
}

void IndiServerController::addDriver(const std::string & driver) {
    if (!fifo) {
        throw new std::runtime_error("Fifo is not enabled - cannot add driver");
    }

    int fifoFd = open(fifoPath.c_str(), O_WRONLY);
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
    return unixSocketPath;
}

int IndiServerController::getTcpPort() const {
  return tcpPort;
}

