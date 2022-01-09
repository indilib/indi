#include <system_error>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <errno.h>

#include "ConnectionMock.h"
#include "SharedBuffer.h"
#include "XmlAwaiter.h"
#include "utils.h"


ConnectionMock::ConnectionMock() {
    fds[0] = -1;
    fds[1] = -1;
    bufferReceiveAllowed = false;
}

ConnectionMock::~ConnectionMock() {
    release();
}

void ConnectionMock::release() {
    for(auto i : receivedFds) {
        ::close(i);
    }
    receivedFds.clear();
    bufferReceiveAllowed = false;
    fds[0] = -1;
    fds[1] = -1;
}


void ConnectionMock::setFds(int rd, int wr) {
    release();
    fds[0] = rd;
    fds[1] = wr;
    bufferReceiveAllowed = false;
}

void ConnectionMock::allowBufferReceive(bool state) {
    if ((!state) && receivedFds.size()) {
        throw std::runtime_error("More buffer were received");
    }
    bufferReceiveAllowed = state;
}

ssize_t ConnectionMock::read(void * buffer, size_t len)
{
    struct msghdr msgh;
    struct iovec iov;

    union {
        struct cmsghdr cmsgh;
        /* Space large enough to hold an 'int' */
        char control[CMSG_SPACE(256 * sizeof(int))];
    } control_un;

    iov.iov_base = buffer;
    iov.iov_len = len;

    msgh.msg_name = NULL;
    msgh.msg_namelen = 0;
    msgh.msg_iov = &iov;
    msgh.msg_iovlen = 1;
    msgh.msg_flags = 0;
    msgh.msg_control = control_un.control;
    msgh.msg_controllen = sizeof(control_un.control);

    auto size = recvmsg(fds[0], &msgh, MSG_CMSG_CLOEXEC);
    if (size == -1) {
        return -1;
    }

    for (struct cmsghdr * cmsg = CMSG_FIRSTHDR(&msgh); cmsg != NULL; cmsg = CMSG_NXTHDR(&msgh, cmsg)) {
        if (cmsg->cmsg_level == SOL_SOCKET && cmsg->cmsg_type == SCM_RIGHTS) {
            int fdCount = 0;
            while(cmsg->cmsg_len >= CMSG_LEN((fdCount+1) * sizeof(int))) {
                fdCount++;
            }
            int * fds = (int*)CMSG_DATA(cmsg);
            for(int i = 0; i < fdCount; ++i) {
                if (!bufferReceiveAllowed) {
                    throw std::runtime_error("Received unexpected buffer");
                }
                receivedFds.push_back(fds[i]);
            }
        }
    }
    return size;
}

void ConnectionMock::expectBuffer(SharedBuffer & sb) {
    if (receivedFds.empty()) {
        throw std::runtime_error("Buffer not received");
    }
    int fd = receivedFds.front();
    receivedFds.pop_front();
    sb.attach(fd);
}

void ConnectionMock::expect(const std::string & str) {
    ssize_t l = str.size();
    char buff[l];
    // FIXME: could interrupt sooner if input does not match
    ssize_t rd = read(buff, l);
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

char ConnectionMock::readChar(const std::string & expected) {
    char buff[1];
    ssize_t rd = read(buff, 1);
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

void ConnectionMock::send(const std::string & str, const SharedBuffer ** buffers) {
    int fdCount = 0;
    while(buffers[fdCount]) fdCount++;

    if (fdCount == 0) {
        send(str);
        return;
    }

    if (str.size() == 0) {
        throw std::runtime_error("Can't attach buffer to empty message");
    }

    struct msghdr msgh;
    struct iovec iov[2];
    int cmsghdrlength;
    struct cmsghdr * cmsgh;


    cmsghdrlength = CMSG_SPACE((fdCount * sizeof(int)));
    cmsgh = (struct cmsghdr*)malloc(cmsghdrlength);

    /* Write the fd as ancillary data */
    cmsgh->cmsg_len = CMSG_LEN(sizeof(int));
    cmsgh->cmsg_level = SOL_SOCKET;
    cmsgh->cmsg_type = SCM_RIGHTS;
    msgh.msg_control = cmsgh;
    msgh.msg_controllen = cmsghdrlength;
    for(int i = 0; i < fdCount; ++i) {
        int fd = buffers[i]->getFd();
        ((int *) CMSG_DATA(CMSG_FIRSTHDR(&msgh)))[i] = fd;
    }

    iov[0].iov_base = (void*)str.data();
    iov[0].iov_len = str.size();

    msgh.msg_flags = 0;
    msgh.msg_name = NULL;
    msgh.msg_namelen = 0;
    msgh.msg_iov = iov;
    msgh.msg_iovlen = 1;


    int ret = sendmsg(fds[1], &msgh, 0);
    if (ret == -1) {
        int e = errno;
        throw std::system_error(e, std::generic_category(), "Write with buffer failed for " + str);
    }

    if ((unsigned)ret < str.size()) {
        throw std::runtime_error("Input closed while buffer sending " + str);
    }
}

void ConnectionMock::send(const std::string & str, const SharedBuffer & buffer) {
    const SharedBuffer * bufferPtrs[2];
    bufferPtrs[0] = &buffer;
    bufferPtrs[1] = nullptr;
    send(str, bufferPtrs);
}
