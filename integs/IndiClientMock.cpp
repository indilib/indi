#include <system_error>
#include <unistd.h>

#include "IndiClientMock.h"
#include "utils.h"

IndiClientMock::IndiClientMock() {
    fd = -1;
}

IndiClientMock::~IndiClientMock() {
    close();
}

void IndiClientMock::close() {
    if (fd != -1) ::close(fd);
    cnx.setFds(-1, -1);
}

void IndiClientMock::connectUnix(const std::string & path) {
    fd = unixSocketConnect(path);
    cnx.setFds(fd, fd);
}

void IndiClientMock::connectTcp(const std::string & host, int port) {
    fd = tcpSocketConnect(host, port, false);
    cnx.setFds(fd, fd);
}

void IndiClientMock::ping() {
    cnx.send("<pingRequest uid='flush'/>\n");
    cnx.expectXml("<pingReply uid='flush'/>\n");
}
