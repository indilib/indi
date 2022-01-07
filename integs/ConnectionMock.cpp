#include <system_error>
#include <unistd.h>
#include <string.h>

#include "ConnectionMock.h"
#include "utils.h"


ConnectionMock::ConnectionMock() {
    fds[0] = -1;
    fds[1] = -1;
}

void ConnectionMock::setFds(int rd, int wr) {
    fds[0] = rd;
    fds[1] = wr;
}

void ConnectionMock::expect(const std::string & str) {
    ssize_t l = str.size();
    char buff[l];
    // FIXME: could interrupt sooner if input does not match
    ssize_t rd = read(fds[0], buff, l);
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

void ConnectionMock::send(const std::string & str) {
    ssize_t l = str.size();
    ssize_t wr = write(fds[1], str.c_str(), l);
    if (wr == -1) {
        int e = errno;
        throw std::system_error(e, std::generic_category(), "Write failed while sending " + str);
    }
    if (wr < l) {
        throw std::runtime_error("Input closed while sending " + str);
    }
}
