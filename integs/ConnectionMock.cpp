#include <system_error>
#include <unistd.h>
#include <string.h>

#include "ConnectionMock.h"
#include "XmlAwaiter.h"
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

char ConnectionMock::readChar(const std::string & expected) const {
    char buff[1];
    ssize_t rd = read(fds[0], buff, 1);
    if (rd == 0) {
        throw std::runtime_error("Input closed while expecting " + expected);
    }
    if (rd == -1) {
        int e = errno;
        throw std::system_error(e, std::generic_category(), "Read failed while expecting " + expected);
    }
    return buff[0];
}

enum XmlStatus { PRE, TAGNAME, WAIT_ATTRIB, ATTRIB, QUOTE, WAIT_CLOSE };

void ConnectionMock::expectXml(const std::string & expected) {
    std::string received;
    auto readchar = [this, expected, &received]()->char{
        char c = readChar(expected);
        received += c;
        return c;
    };
    try {
        auto fragment = parseXmlFragment(readchar);
        if (fragment != expected) {
            fprintf(stderr, "canonicalized as %s\n", fragment.c_str());
            throw std::runtime_error("xml fragment does not match");
        }
    } catch(std::runtime_error & e) {
        throw std::runtime_error(std::string(e.what()) + "\nexpected: " + expected + "\nReceived:" + received);
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
