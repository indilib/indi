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
#include "MsgQueue.hpp"
#include "Utils.hpp"
#include "Constants.hpp"
#include "SerializedMsg.hpp"
#include "Msg.hpp"
#include "CommandLineArgs.hpp"

#include <sys/socket.h>
#include <fcntl.h>
#include <unistd.h>

using namespace indiserver::constants;

void MsgQueue::writeToFd()
{
    ssize_t nw;
    void * data;
    ssize_t nsend;
    std::vector<int> sharedBuffers;

    /* get current message */
    auto mp = headMsg();
    if (mp == nullptr)
    {
        log("Unexpected write notification");
        return;
    }


    do
    {
        if (!mp->getContent(nsent, data, nsend, sharedBuffers))
        {
            wio.stop();
            return;
        }

        if (nsend == 0)
        {
            consumeHeadMsg();
            mp = headMsg();
            if (mp == nullptr)
            {
                return;
            }
        }
    }
    while(nsend == 0);

    /* send next chunk, never more than maxWriteBufferLength to reduce blocking */
    if (nsend > static_cast<ssize_t>(maxWriteBufferLength))
        nsend = static_cast<ssize_t>(maxWriteBufferLength);

    if (!useSharedBuffer)
    {
        nw = write(wFd, data, nsend);
    }
    else
    {
        struct msghdr msgh;
        struct iovec iov[1];
        int cmsghdrlength;
        struct cmsghdr * cmsgh;

        size_t fdCount = sharedBuffers.size();
        if (fdCount > 0)
        {
            if (fdCount > maxFDPerMessage)
            {
                log(fmt("attempt to send too many FD\n"));
                close();
                return;
            }

            cmsghdrlength = CMSG_SPACE((fdCount * sizeof(int)));
            // FIXME: abort on alloc error here
            cmsgh = (struct cmsghdr*)malloc(cmsghdrlength);
            memset(cmsgh, 0, cmsghdrlength);

            /* Write the fd as ancillary data */
            cmsgh->cmsg_len = CMSG_LEN(fdCount * sizeof(int));
            cmsgh->cmsg_level = SOL_SOCKET;
            cmsgh->cmsg_type = SCM_RIGHTS;
            msgh.msg_control = cmsgh;
            msgh.msg_controllen = cmsghdrlength;
            for(size_t i = 0; i < fdCount; ++i)
            {
                ((int *) CMSG_DATA(CMSG_FIRSTHDR(&msgh)))[i] = sharedBuffers[i];
            }
        }
        else
        {
            cmsgh = NULL;
            cmsghdrlength = 0;
            msgh.msg_control = cmsgh;
            msgh.msg_controllen = cmsghdrlength;
        }

        iov[0].iov_base = data;
        iov[0].iov_len = nsend;

        msgh.msg_flags = 0;
        msgh.msg_name = NULL;
        msgh.msg_namelen = 0;
        msgh.msg_iov = iov;
        msgh.msg_iovlen = 1;

        nw = sendmsg(wFd, &msgh,  MSG_NOSIGNAL);

        free(cmsgh);
    }

    /* shut down if trouble */
    if (nw <= 0)
    {
        if (nw == 0)
            log("write returned 0\n");
        else
            log(fmt("write: %s\n", strerror(errno)));

        // Keep the read part open
        closeWritePart();
        return;
    }

    /* trace */
    if (userConfigurableArguments->verbosity > 2)
    {
        log(fmt("sending msg nq %ld:\n%.*s\n",
                msgq.size(), (int)nw, data));
    }
    else if (userConfigurableArguments->verbosity > 1)
    {
        log(fmt("sending %.*s\n", (int)nw, data));
    }

    /* update amount sent. when complete: free message if we are the last
     * to use it and pop from our queue.
     */
    mp->advance(nsent, nw);
    if (nsent.done())
        consumeHeadMsg();
}

void MsgQueue::log(const std::string &str) const
{
    // This is only invoked from destructor
    std::string logLine = "Dying Connection ";
    logLine += ": ";
    logLine += str;
    ::log(logLine);
}



void MsgQueue::crackBLOB(const char *enableBLOB, BLOBHandling *bp)
{
    if (!strcmp(enableBLOB, "Also"))
        *bp = B_ALSO;
    else if (!strcmp(enableBLOB, "Only"))
        *bp = B_ONLY;
    else if (!strcmp(enableBLOB, "Never"))
        *bp = B_NEVER;
}



void MsgQueue::traceMsg(const std::string &logMsg, XMLEle *root)
{
    log(logMsg);

    static const char *prtags[] =
    {
        "defNumber", "oneNumber", "defText", "oneText", "defSwitch", "oneSwitch", "defLight", "oneLight",
    };
    XMLEle *e;
    const char *msg, *perm, *pcd;
    unsigned int i;

    /* print tag header */
    fprintf(stderr, "%s %s %s %s", tagXMLEle(root), findXMLAttValu(root, "device"), findXMLAttValu(root, "name"),
            findXMLAttValu(root, "state"));
    pcd = pcdataXMLEle(root);
    if (pcd[0])
        fprintf(stderr, " %s", pcd);
    perm = findXMLAttValu(root, "perm");
    if (perm[0])
        fprintf(stderr, " %s", perm);
    msg = findXMLAttValu(root, "message");
    if (msg[0])
        fprintf(stderr, " '%s'", msg);

    /* print each array value */
    for (e = nextXMLEle(root, 1); e; e = nextXMLEle(root, 0))
        for (i = 0; i < sizeof(prtags) / sizeof(prtags[0]); i++)
            if (strcmp(prtags[i], tagXMLEle(e)) == 0)
                fprintf(stderr, "\n %10s='%s'", findXMLAttValu(e, "name"), pcdataXMLEle(e));

    fprintf(stderr, "\n");
}

MsgQueue::MsgQueue(bool useSharedBuffer): useSharedBuffer(useSharedBuffer)
{
    lp = newLilXML();
    rio.set<MsgQueue, &MsgQueue::ioCb>(this);
    wio.set<MsgQueue, &MsgQueue::ioCb>(this);
    rFd = -1;
    wFd = -1;
}

MsgQueue::~MsgQueue()
{
    rio.stop();
    wio.stop();

    clearMsgQueue();
    delLilXML(lp);
    lp = nullptr;

    setFds(-1, -1);

    /* unreference messages queue for this client */
    auto msgqcp = msgq;
    msgq.clear();
    for(auto mp : msgqcp)
    {
        mp->release(this);
    }
}

void MsgQueue::closeWritePart()
{
    if (wFd == -1)
    {
        // Already closed
        return;
    }

    int oldWFd = wFd;

    wFd = -1;
    // Clear the queue and stop the io slot
    clearMsgQueue();

    if (oldWFd == rFd)
    {
        if (shutdown(oldWFd, SHUT_WR) == -1)
        {
            if (errno != ENOTCONN)
            {
                log(fmt("socket shutdown failed: %s\n", strerror(errno)));
                close();
            }
        }
    }
    else
    {
        if (::close(oldWFd) == -1)
        {
            log(fmt("socket close failed: %s\n", strerror(errno)));
            close();
        }
    }
}

void MsgQueue::setFds(int rFd, int wFd)
{
    if (this->rFd != -1)
    {
        rio.stop();
        wio.stop();
        ::close(this->rFd);
        if (this->rFd != this->wFd)
        {
            ::close(this->wFd);
        }
    }
    else if (this->wFd != -1)
    {
        wio.stop();
        ::close(this->wFd);
    }

    this->rFd = rFd;
    this->wFd = wFd;
    this->nsent.reset();

    if (rFd != -1)
    {
        fcntl(rFd, F_SETFL, fcntl(rFd, F_GETFL, 0) | O_NONBLOCK);
        if (wFd != rFd)
        {
            fcntl(wFd, F_SETFL, fcntl(wFd, F_GETFL, 0) | O_NONBLOCK);
        }

        rio.set(rFd, ev::READ);
        wio.set(wFd, ev::WRITE);
        updateIos();
    }
}

SerializedMsg * MsgQueue::headMsg() const
{
    if (msgq.empty()) return nullptr;
    return *(msgq.begin());
}

void MsgQueue::consumeHeadMsg()
{
    auto msg = headMsg();
    msgq.pop_front();
    msg->release(this);
    nsent.reset();

    updateIos();
}

void MsgQueue::pushMsg(Msg * mp)
{
    // Don't write messages to client that have been disconnected
    if (wFd == -1)
    {
        return;
    }

    auto serialized = mp->serialize(this);

    msgq.push_back(serialized);
    serialized->addAwaiter(this);

    // Register for client write
    updateIos();
}

void MsgQueue::updateIos()
{
    if (wFd != -1)
    {
        if (msgq.empty() || !msgq.front()->requestContent(nsent))
        {
            wio.stop();
        }
        else
        {
            wio.start();
        }
    }
    if (rFd != -1)
    {
        rio.start();
    }
}

void MsgQueue::messageMayHaveProgressed(const SerializedMsg * msg)
{
    if ((!msgq.empty()) && (msgq.front() == msg))
    {
        updateIos();
    }
}

void MsgQueue::clearMsgQueue()
{
    nsent.reset();

    auto queueCopy = msgq;
    for(auto mp : queueCopy)
    {
        mp->release(this);
    }
    msgq.clear();

    // Cancel io write events
    updateIos();
    wio.stop();
}

unsigned long MsgQueue::msgQSize() const
{
    unsigned long l = 0;

    for (auto mp : msgq)
    {
        l += sizeof(Msg);
        l += mp->queueSize();
    }

    return (l);
}

void MsgQueue::ioCb(ev::io &, int revents)
{
    if (EV_ERROR & revents)
    {
        int sockErrno = readFdError(this->rFd);
        if ((!sockErrno) && this->wFd != this->rFd)
        {
            sockErrno = readFdError(this->wFd);
        }

        if (sockErrno)
        {
            log(fmt("Communication error: %s\n", strerror(sockErrno)));
            close();
            return;
        }
    }

    if (revents & EV_READ)
        readFromFd();

    if (revents & EV_WRITE)
        writeToFd();
}

size_t MsgQueue::doRead(char * buf, size_t nr)
{
    if (!useSharedBuffer)
    {
        /* read client - works for all kinds of fds incl pipe*/
        return read(rFd, buf, sizeof(buf));
    }
    else
    {
        // Use recvmsg for ancillary data
        struct msghdr msgh;
        struct iovec iov;

        union
        {
            struct cmsghdr cmsgh;
            /* Space large enough to hold an 'int' */
            char control[CMSG_SPACE(maxFDPerMessage * sizeof(int))];
        } control_un;

        iov.iov_base = buf;
        iov.iov_len = nr;

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
        int size = recvmsg(rFd, &msgh, recvflag);
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
                //log(fmt("Received %d fds\n", fdCount));
                int * fds = (int*)CMSG_DATA(cmsg);
                for(int i = 0; i < fdCount; ++i)
                {
#ifndef __linux__
                    fcntl(fds[i], F_SETFD, FD_CLOEXEC);
#endif
                    incomingSharedBuffers.push_back(fds[i]);
                }
            }
            else
            {
                log(fmt("Ignoring ancillary data level %d, type %d\n", cmsg->cmsg_level, cmsg->cmsg_type));
            }
        }
        return size;
    }
}

void MsgQueue::readFromFd()
{
    char buf[maxReadBufferLength];
    ssize_t nr;

    /* read client */
    nr = doRead(buf, sizeof(buf));
    if (nr <= 0)
    {
        if (errno == EAGAIN || errno == EWOULDBLOCK) return;

        if (nr < 0)
            log(fmt("read: %s\n", strerror(errno)));
        else if (userConfigurableArguments->verbosity > 0)
            log(fmt("read EOF\n"));
        close();
        return;
    }

    /* process XML chunk */
    char err[1024];
    XMLEle **nodes = parseXMLChunk(lp, buf, nr, err);
    if (!nodes)
    {
        log(fmt("XML error: %s\n", err));
        log(fmt("XML read: %.*s\n", (int)nr, buf));
        close();
        return;
    }

    int inode = 0;

    XMLEle *root = nodes[inode];
    // Stop processing message in case of deletion...
    auto hb = heartBeat();
    while (root)
    {
        if (hb.alive())
        {
            if (userConfigurableArguments->verbosity > 2)
                traceMsg("read ", root);
            else if (userConfigurableArguments->verbosity > 1)
            {
                log(fmt("read <%s device='%s' name='%s'>\n",
                        tagXMLEle(root), findXMLAttValu(root, "device"), findXMLAttValu(root, "name")));
            }

            onMessage(root, incomingSharedBuffers);
        }
        else
        {
            // Otherwise, client got killed. Just release pending messages
            delXMLEle(root);
        }
        inode++;
        root = nodes[inode];
    }

    free(nodes);
}
