#ifndef _GNU_SOURCE
#define _GNU_SOURCE // needed for siginfo_t and sigaction
#endif

#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <signal.h>
#include <system_error>

#include "utils.h"

void setupSigPipe() {
    signal(SIGPIPE, SIG_IGN);
}

void initSocketAddr(const std::string & unixAddr, struct sockaddr_un & serv_addr_un, struct sockaddr * & sockaddr, socklen_t & addrlen)
{
    memset(&serv_addr_un, 0, sizeof(serv_addr_un));
    serv_addr_un.sun_family = AF_UNIX;

    // Using abstract socket path to avoid filesystem boilerplate
    // FIXME: is this supported on MACOS ?
    strncpy(serv_addr_un.sun_path + 1, unixAddr.c_str(), sizeof(serv_addr_un.sun_path) - 1);

    int len = offsetof(struct sockaddr_un, sun_path) + unixAddr.size() + 1;
    serv_addr_un.sun_path[0] = 0;

    sockaddr = (struct sockaddr *)&serv_addr_un;
    addrlen = len;
}


int unixSocketListen(const std::string & unixAddr) {
    struct sockaddr_un serv_addr_un;
    struct sockaddr *sockaddr;
    socklen_t addrlen;

    initSocketAddr(unixAddr, serv_addr_un, sockaddr, addrlen);

    int sockfd;
    if ((sockfd = socket(AF_UNIX, SOCK_STREAM, 0)) < 0)
    {
        throw std::system_error(errno, std::generic_category(), "Socket");
    }

    int reuse = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0)
    {
        throw std::system_error(errno, std::generic_category(), "setsockopt");
    }

    if (bind(sockfd, sockaddr, addrlen) < 0)
    {
        throw std::system_error(errno, std::generic_category(), "Bind to " + unixAddr);
    }

    /* willing to accept connections with a backlog of 5 pending */
    if (::listen(sockfd, 5) < 0)
    {
        throw std::system_error(errno, std::generic_category(), "Listen to " + unixAddr);
    }

    return sockfd;
}

int unixSocketAccept(int fd) {
    /* get a private connection to new client */
    int cli_fd  = ::accept(fd, 0, 0);
    if (cli_fd < 0)
    {
        throw std::system_error(errno, std::generic_category(), "Accept failed");
    }
    return cli_fd;
}

int unixSocketConnect(const std::string & unixAddr, bool failAllowed) {
    struct sockaddr_un serv_addr_un;
    struct sockaddr *sockaddr;
    socklen_t addrlen;

    initSocketAddr(unixAddr, serv_addr_un, sockaddr, addrlen);

    int sockfd;
    if ((sockfd = socket(AF_UNIX, SOCK_STREAM, 0)) < 0)
    {
        throw std::system_error(errno, std::generic_category(), "Socket");
    }


    int ret = ::connect(sockfd, sockaddr, addrlen);
    if (ret != -1) {
        return sockfd;
    }

    int e = errno;
    close(sockfd);
    if (!failAllowed) {
        throw std::system_error(e, std::generic_category(), "Connect to " + unixAddr);
    }
    return -1;
}

void unixSocketSendFds(int fd, int count, int * fds) {
    struct msghdr msgh;
    struct iovec iov[1];
    char buff[1] = {0};
    int cmsghdrlength;
    struct cmsghdr * cmsgh;


    cmsghdrlength = CMSG_SPACE((count * sizeof(int)));
    // FIXME: abort on alloc error here
    cmsgh = (struct cmsghdr*)malloc(cmsghdrlength);

    /* Write the fd as ancillary data */
    cmsgh->cmsg_len = CMSG_LEN(count * sizeof(int));
    cmsgh->cmsg_level = SOL_SOCKET;
    cmsgh->cmsg_type = SCM_RIGHTS;
    msgh.msg_control = cmsgh;
    msgh.msg_controllen = cmsghdrlength;
    for(int i = 0; i < count; ++i) {
        ((int *) CMSG_DATA(CMSG_FIRSTHDR(&msgh)))[i] = fds[i];
    }

    iov[0].iov_base = buff;
    iov[0].iov_len = 1;

    msgh.msg_flags = 0;
    msgh.msg_name = NULL;
    msgh.msg_namelen = 0;
    msgh.msg_iov = iov;
    msgh.msg_iovlen = 1;

    int ret = sendmsg(fd, &msgh,  MSG_NOSIGNAL);

    if (ret == -1) {
        throw std::system_error(errno, std::generic_category(), "Failed to send fds");
    }
    if (ret == 0) {
        throw std::runtime_error("Channel closed when sending fds");
    }
}

void unixSocketRecvFds(int fd, int count, int * fdsDest) {
    char buf[1];
    struct msghdr msgh;
    struct iovec iov;

    union {
        struct cmsghdr cmsgh;
        /* Space large enough to hold an 'int' */
        char control[CMSG_SPACE(16 * sizeof(int))];
    } control_un;

    if (count > 16) {
        throw std::runtime_error("Cannot pass that amount of fds");
    }



    iov.iov_base = buf;
    iov.iov_len = 1;

    msgh.msg_name = NULL;
    msgh.msg_namelen = 0;
    msgh.msg_iov = &iov;
    msgh.msg_iovlen = 1;
    msgh.msg_flags = 0;
    msgh.msg_control = control_un.control;
    msgh.msg_controllen = sizeof(control_un.control);

    int size = recvmsg(fd, &msgh, MSG_CMSG_CLOEXEC);
    if (size == -1) {
        throw std::system_error(errno, std::generic_category(), "Could not receive fds");
    }

    for (struct cmsghdr * cmsg = CMSG_FIRSTHDR(&msgh); cmsg != NULL; cmsg = CMSG_NXTHDR(&msgh, cmsg)) {
        if (cmsg->cmsg_level == SOL_SOCKET && cmsg->cmsg_type == SCM_RIGHTS) {
            int fdCount = 0;
            while(cmsg->cmsg_len >= CMSG_LEN((fdCount+1) * sizeof(int))) {
                fdCount++;
            }
            fprintf(stderr, "Received fd : %d\n", fdCount);
            if (fdCount != count) {
                throw std::runtime_error("Wrong number of fds received");
            }
            int * fds = (int*)CMSG_DATA(cmsg);
            for(int i = 0; i < fdCount; ++i) {
                fdsDest[i] = fds[i];
            }

            return;
        }
    }
    throw std::runtime_error("Did not receive fds");
}
