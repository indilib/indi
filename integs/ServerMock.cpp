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
