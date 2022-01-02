#include <system_error>

#include "FakeDriverCnx.h"
#include "utils.h"
#include <string.h>

void FakeDriverCnx::setup() {
    // Create a socket that will not have the clo on exec flag ?
    abstractPath = "/tmp/fakedriver-test";
    setenv("FAKEDRIVER_ADDRESS", abstractPath.c_str(), 1);

    serverConnection = unixSocketListen(abstractPath);
}

void FakeDriverCnx::waitEstablish() {
    driverConnection = unixSocketAccept(serverConnection);
    unixSocketRecvFds(driverConnection, 2, driverFds);
}

void FakeDriverCnx::terminate() {
    close(driverConnection);
    close(driverFds[0]);
    close(driverFds[1]);
    driverConnection = -1;
    driverFds[0] = -1;
    driverFds[1] = -1;
}

void FakeDriverCnx::expect(const std::string & str) {
    size_t l = str.size();
    char buff[l];
    // FIXME: could interrupt sooner if input does not match
    ssize_t rd = read(driverFds[0], buff, l);
    if (rd == 0) {
        throw std::runtime_error("Input closed while expecting " + str);
    }
    if (rd == -1) {
        int e = errno;
        throw std::system_error(e, std::generic_category(), "Read failed while expecting " + str);
    }
    if (rd < l || strncmp(str.c_str(), buff, l)) {
        throw std::runtime_error("Received unexpected content while expecting " + str + ": " + std::string(buff, rd));
    }
}

void FakeDriverCnx::send(const std::string & str) {
    size_t l = str.size();
    ssize_t wr = write(driverFds[1], str.c_str(), l);
    if (wr == -1) {
        int e = errno;
        throw std::system_error(e, std::generic_category(), "Write failed while sending " + str);
    }
    if (wr < l) {
        throw std::runtime_error("Input closed while sending " + str);
    }
}
