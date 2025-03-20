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
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <vector>

#include "ConnectionMock.h"
#include "SharedBuffer.h"
#include "XmlAwaiter.h"
#include "utils.h"


ConnectionMock::ConnectionMock()
{
    fds[0] = -1;
    fds[1] = -1;
    pendingChar = -1;
    bufferReceiveAllowed = false;
}

ConnectionMock::~ConnectionMock()
{
    release();
}

void ConnectionMock::release()
{
    for(auto i : receivedFds)
    {
        ::close(i);
    }
    receivedFds.clear();
    bufferReceiveAllowed = false;
    fds[0] = -1;
    fds[1] = -1;
    pendingChar = -1;
}


void ConnectionMock::setFds(int rd, int wr)
{
    release();
    fds[0] = rd;
    fds[1] = wr;
    bufferReceiveAllowed = false;
}

void ConnectionMock::allowBufferReceive(bool state)
{
    if ((!state) && receivedFds.size())
    {
        throw std::runtime_error("More buffer were received");
    }
    bufferReceiveAllowed = state;
}

ssize_t ConnectionMock::read(void * vbuffer, size_t len)
{
    unsigned char * buffer = (unsigned char*) vbuffer;
    ssize_t baseLength = 0;
    if (len && (pendingChar != -1)) {
        ((char*)buffer)[0] = pendingChar;
        pendingChar = -1;
        len--;
        buffer++;
        if (len == 0)
        {
            return 1;
        }
        baseLength = 1;
    }

    struct msghdr msgh;
    struct iovec iov;

    union
    {
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

    int recvflag;
#ifdef __linux__
    recvflag = MSG_CMSG_CLOEXEC;
#else
    recvflag = 0;
#endif

    auto size = recvmsg(fds[0], &msgh, recvflag);
    if (size == -1)
    {
        return -1;
    }

    for (struct cmsghdr * cmsg = CMSG_FIRSTHDR(&msgh); cmsg != NULL; cmsg = CMSG_NXTHDR(&msgh, cmsg))
    {
        if (cmsg->cmsg_level == SOL_SOCKET && cmsg->cmsg_type == SCM_RIGHTS)
        {
            int fdCount = 0;
            while(cmsg->cmsg_len >= CMSG_LEN((fdCount + 1) * sizeof(int)))
            {
                fdCount++;
            }
            int * fds = (int*)CMSG_DATA(cmsg);
            for(int i = 0; i < fdCount; ++i)
            {
                if (!bufferReceiveAllowed)
                {
                    pendingData = std::string((char*)buffer, size);
                    throw std::runtime_error("Received unexpected buffer");
                }
#ifndef __linux__
                fcntl(fds[i], F_SETFD, FD_CLOEXEC);
#endif
                receivedFds.push_back(fds[i]);
            }
        }
    }
    return size + baseLength;
}

void ConnectionMock::expectBuffer(SharedBuffer &sb)
{
    if (receivedFds.empty())
    {
        throw std::runtime_error("Buffer not received");
    }
    int fd = receivedFds.front();
    receivedFds.pop_front();
    sb.attach(fd);
}

void ConnectionMock::expect(const std::string &str)
{
    ssize_t l = str.size();
    std::vector<char> buff(l);

    char * in = buff.data();
    ssize_t left = l;
    while(left)
    {
        // FIXME: could interrupt sooner if input does not match
        ssize_t rd = read(in, left);
        if (rd == 0)
        {
            throw std::runtime_error("Input closed while expecting " + str);
        }
        if (rd == -1)
        {
            int e = errno;
            throw std::system_error(e, std::generic_category(), "Read failed while expecting " + str);
        }
        left -= rd;
        in += rd;
    }
    if (strncmp(str.c_str(), buff.data(), l))
    {
        throw std::runtime_error("Received unexpected content while expecting " + str + ": " + std::string(buff.data(), l));
    }
}

char ConnectionMock::readChar(const std::string &expected)
{
    if (pendingChar != -1)
    {
        char c = pendingChar;
        pendingChar = -1;
        return c;
    }

    char buff[1];
    ssize_t rd = read(buff, 1);
    if (rd == 0)
    {
        throw std::runtime_error("Input closed while expecting " + expected);
    }
    if (rd == -1)
    {
        int e = errno;
        throw std::system_error(e, std::generic_category(), "Read failed while expecting " + expected);
    }
    return buff[0];
}

char ConnectionMock::peekChar(const std::string & expected) {
    if (pendingChar != -1)
    {
        return pendingChar;
    }
    char ret = readChar(expected);
    pendingChar = ret;
    return ret;
}

static std::string parseXmlFragmentFromString(const std::string &str)
{
    ssize_t pos = 0;
    auto lambda = [&pos, &str]()->char
    {
        return str[pos++];
    };
    std::string result = parseXmlFragment(lambda);
    while((unsigned)pos < str.length() && str[pos] == '\n') pos++;
    if ((unsigned)pos != str.length())
    {
        throw std::runtime_error("Expected string contains garbage: " + str);
    }
    return result;
}


std::string ConnectionMock::receiveMore()
{

    auto currentFlag = fcntl(fds[0], F_GETFL, 0);
    if (! (currentFlag & O_NONBLOCK))
    {
        fcntl(fds[0], F_SETFL, currentFlag | O_NONBLOCK);
    }

    char buffer[256];
    auto r = read(buffer, 256);
    if (! (currentFlag & O_NONBLOCK))
    {
        auto errno_cpy = errno;
        fcntl(fds[0], F_SETFL, currentFlag);
        errno = errno_cpy;
    }

    if (r != -1)
    {
        return pendingData + std::string(buffer, r);
    }
    perror("receiveMore");
    return pendingData;
}

std::string ConnectionMock::expectBase64() {
    std::string result;

    while(true) {
        char c = peekChar("base64");
        if (c == '<') break;

        result += readChar("base64");
    }

    return result;
}


void ConnectionMock::expectXml(const std::string &expected)
{
    std::string expectedCanonical = parseXmlFragmentFromString(expected);

    std::string received;
    auto readchar = [this, expected, &received]()->char
    {
        char c = readChar(expected);
        received += c;
        return c;
    };
    try
    {
        auto fragment = parseXmlFragment(readchar);
        if (fragment != expectedCanonical)
        {
            fprintf(stderr, "canonicalized as %s\n", fragment.c_str());
            throw std::runtime_error("xml fragment does not match");
        }
    }
    catch(std::runtime_error &e)
    {
        received += receiveMore();
        throw std::runtime_error(std::string(e.what()) + "\nexpected: " + expected + "\nReceived: " + received);
    }
}

void ConnectionMock::send(const std::string &str)
{
    ssize_t l = str.size();
    ssize_t wr = write(fds[1], str.c_str(), l);
    if (wr == -1)
    {
        int e = errno;
        throw std::system_error(e, std::generic_category(), "Write failed while sending " + str);
    }
    if (wr < l)
    {
        throw std::runtime_error("Input closed while sending " + str);
    }
}

void ConnectionMock::shutdown(bool rd, bool wr)
{
    if (fds[0] != fds[1]) {
        if (rd && fds[0] != -1) {
            if (::shutdown(fds[0], SHUT_RD) == -1) {
                perror("shutdown read fd");
            }
        }
        if (wr && fds[1] != -1) {
            if (::shutdown(fds[1], SHUT_WR) == -1) {
                perror("shutdown write fd");
            }
        }
    } else {
        if (!(rd || wr)) return;

        int how = (rd && wr) ? SHUT_RDWR : rd ? SHUT_RD : SHUT_WR;
        if (::shutdown(fds[0], how) == -1) {
            if (rd && wr) {
                perror("shutdown rdwr");
            } else if (rd) {
                perror("shutdown rd");
            } else {
                perror("shutdown wr");
            }
        }
    }
}

void ConnectionMock::send(const std::string &str, const SharedBuffer ** buffers)
{
    int fdCount = 0;
    while(buffers[fdCount]) fdCount++;

    if (fdCount == 0)
    {
        send(str);
        return;
    }

    if (str.size() == 0)
    {
        throw std::runtime_error("Can't attach buffer to empty message");
    }

    struct msghdr msgh;
    struct iovec iov[2];
    int cmsghdrlength;
    struct cmsghdr * cmsgh;


    cmsghdrlength = CMSG_SPACE((fdCount * sizeof(int)));
    cmsgh = (struct cmsghdr*)malloc(cmsghdrlength);
    memset(cmsgh, 0, cmsghdrlength);

    /* Write the fd as ancillary data */
    cmsgh->cmsg_len = CMSG_LEN(sizeof(int));
    cmsgh->cmsg_level = SOL_SOCKET;
    cmsgh->cmsg_type = SCM_RIGHTS;
    msgh.msg_control = cmsgh;
    msgh.msg_controllen = cmsghdrlength;
    for(int i = 0; i < fdCount; ++i)
    {
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
    if (ret == -1)
    {
        int e = errno;
        throw std::system_error(e, std::generic_category(), "Write with buffer failed for " + str);
    }

    if ((unsigned)ret < str.size())
    {
        throw std::runtime_error("Input closed while buffer sending " + str);
    }
}

void ConnectionMock::send(const std::string &str, const SharedBuffer &buffer)
{
    const SharedBuffer * bufferPtrs[2];
    bufferPtrs[0] = &buffer;
    bufferPtrs[1] = nullptr;
    send(str, bufferPtrs);
}
