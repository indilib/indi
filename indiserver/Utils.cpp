/* INDI Server for protocol version 1.7.
 * Copyright (C) 2007 Elwood C. Downey <ecdowney@clearskyinstitute.com>
                 2013 Jasem Mutlaq <mutlaqja@ikarustech.com>
                 2022 Ludovic Pollet
    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include "Utils.hpp"
#include "Constants.hpp"
#include "CommandLineArgs.hpp"

#include <cstring>
#include <csignal>
#include <cstdarg>
#include <ctime>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/mman.h>

/* record we have started and our args */
void logStartup(int ac, char *av[])
{
    int i;

    std::string startupMsg = "startup:";
    for (i = 0; i < ac; i++)
    {
        startupMsg += " ";
        startupMsg += av[i];
    }
    startupMsg += '\n';
    log(startupMsg);
}

/* turn off SIGPIPE on bad write so we can handle it inline */
void noSIGPIPE()
{
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = SIG_IGN;
    sigemptyset(&sa.sa_mask);
    (void)sigaction(SIGPIPE, &sa, NULL);
}

/* fill s with current UT string.
* if no s, use a static buffer
* return s or buffer.
* N.B. if use our buffer, be sure to use before calling again
*/
char *indi_tstamp(char *s)
{
    static char sbuf[64];
    struct tm *tp;
    time_t t;

    time(&t);
    tp = gmtime(&t);
    if (!s)
        s = sbuf;
    strftime(s, sizeof(sbuf), "%Y-%m-%dT%H:%M:%S", tp);
    return (s);
}

/* log message in root known to be from device dev to loggingDir, if any.
*/
void logDMsg(XMLEle *root, const char *dev)
{
    char stamp[64];
    char logfn[1024];
    const char *ts, *ms;
    FILE *fp;

    /* get message, if any */
    ms = findXMLAttValu(root, "message");
    if (!ms[0])
        return;

    /* get timestamp now if not provided */
    ts = findXMLAttValu(root, "timestamp");
    if (!ts[0])
    {
        indi_tstamp(stamp);
        ts = stamp;
    }

    /* append to log file, name is date portion of time stamp */
    sprintf(logfn, "%s/%.10s.islog", userConfigurableArguments->loggingDir, ts);
    fp = fopen(logfn, "a");
    if (!fp)
        return; /* oh well */
    fprintf(fp, "%s: %s: %s\n", ts, dev, ms);
    fclose(fp);
}

/* log when then exit */
void Bye()
{
    fprintf(stderr, "%s: good bye\n", indi_tstamp(NULL));
    exit(1);
}

bool parseBlobSize(XMLEle * blobWithAttachedBuffer, ssize_t &size)
{
    std::string sizeStr = findXMLAttValu(blobWithAttachedBuffer, "size");
    if (sizeStr == "")
    {
        return false;
    }
    size_t pos;
    size = std::stoll(sizeStr, &pos, 10);
    if (pos != sizeStr.size())
    {
        log("Invalid size attribute value " + sizeStr);
        return false;
    }
    return true;
}

int xmlReplacementMapFind(void * self, XMLEle * source, XMLEle * * replace)
{
    auto map = (const std::unordered_map<XMLEle*, XMLEle*> *) self;
    auto idx = map->find(source);
    if (idx == map->end())
    {
        return 0;
    }
    *replace = (XMLEle*)idx->second;
    return 1;
}

XMLEle * cloneXMLEleWithReplacementMap(XMLEle * root, const std::unordered_map<XMLEle*, XMLEle*> &replacement)
{
    return cloneXMLEle(root, &xmlReplacementMapFind, (void*)&replacement);
}

std::vector<XMLEle *> findBlobElements(XMLEle * root)
{
    std::vector<XMLEle *> result;
    for (auto ep = nextXMLEle(root, 1); ep; ep = nextXMLEle(root, 0))
    {
        if (strcmp(tagXMLEle(ep), "oneBLOB") == 0)
        {
            result.push_back(ep);
        }
    }
    return result;
}

void log(const std::string &log)
{
    fprintf(stderr, "%s: ", indi_tstamp(NULL));
    fprintf(stderr, "%s", log.c_str());
}

int readFdError(int fd)
{
#ifdef MSG_ERRQUEUE
    char rcvbuf[128];  /* Buffer for normal data (not expected here...) */
    char cbuf[512];    /* Buffer for ancillary data (errors) */
    struct iovec  iov;
    struct msghdr msg;

    iov.iov_base = &rcvbuf;
    iov.iov_len = sizeof(rcvbuf);

    msg.msg_name = nullptr;
    msg.msg_namelen = 0;
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;
    msg.msg_flags = 0;
    msg.msg_control = cbuf;
    msg.msg_controllen = sizeof(cbuf);

    int recv_bytes = recvmsg(fd, &msg, MSG_ERRQUEUE | MSG_DONTWAIT);
    if (recv_bytes == -1)
    {
        if (errno == EAGAIN || errno == EWOULDBLOCK) return 0;
        return errno;
    }

    /* Receive auxiliary data in msgh */
    for (struct cmsghdr * cmsg = CMSG_FIRSTHDR(&msg); cmsg != NULL; cmsg = CMSG_NXTHDR(&msg, cmsg))
    {
        fprintf(stderr, "cmsg_len=%zu, cmsg_level=%u, cmsg_type=%u\n", cmsg->cmsg_len, cmsg->cmsg_level, cmsg->cmsg_type);

        if (cmsg->cmsg_level == SOL_IP && cmsg->cmsg_type == IP_RECVERR)
        {
            return ((struct sock_extended_err *)CMSG_DATA(cmsg))->ee_errno;
        }
    }
#else
    (void)fd;
#endif

    // Default to EIO as a generic error path
    return EIO;
}

void * attachSharedBuffer(int fd, size_t &size)
{
    struct stat sb;
    if (fstat(fd, &sb) == -1)
    {
        perror("invalid shared buffer fd");
        Bye();
    }
    size = sb.st_size;
    void * ret = mmap(0, size, PROT_READ, MAP_SHARED, fd, 0);

    if (ret == MAP_FAILED)
    {
        perror("mmap");
        Bye();
    }

    return ret;
}

void dettachSharedBuffer(int fd, void * ptr, size_t size)
{
    (void)fd;
    if (munmap(ptr, size) == -1)
    {
        perror("shared buffer munmap");
        Bye();
    }
}

std::string fmt(const char *fmt, ...)
{
    char buffer[128];
    int size = sizeof(buffer);
    char *p = buffer;
    va_list ap;

    /* Determine required size */
    va_start(ap, fmt);
    size = vsnprintf(p, size, fmt, ap);
    va_end(ap);

    if (size < 0)
    {
        perror("vnsprintf");
    }

    if ((unsigned)size < sizeof(buffer))
    {
        return std::string(buffer);
    }

    size++;             /* For '\0' */
    p = (char*)malloc(size);
    if (p == NULL)
    {
        perror("malloc");
        Bye();
    }

    va_start(ap, fmt);
    size = vsnprintf(p, size, fmt, ap);
    va_end(ap);

    if (size < 0)
    {
        free(p);
        perror("vnsprintf");
    }
    std::string ret(p);
    free(p);
    return ret;
}
