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

void IndiClientMock::ping() {
    cnx.send("<serverPingRequest uid='flush'/>\n");
    cnx.expect("<serverPingReply uid=\"flush\"/>\n");
}
