#if 0
    INDI
    Copyright (C) 2003-2006 Elwood C. Downey

                        Updated by Jasem Mutlaq (2003-2010)

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

#endif

/* main() for one INDI driver process.
 * Drivers define IS*() functions we call to deliver INDI XML arriving on stdin.
 * Drivers call ID*() functions to send INDI XML commands to stdout.
 * Drivers call IE*() functions to build an event-driver program.
 * Drivers call IU*() functions to perform various common utility tasks.
 * Troubles are reported on stderr then we exit.
 *
 * This requires liblilxml.
 */

#include "base64.h"
#include "eventloop.h"
#include "indidevapi.h"
#include "indidriver.h"
#include "lilxml.h"

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <pthread.h>

#if defined(_WIN32) || defined(__CYGWIN__)
#include <sys/select.h>
#endif

#define MAXRBUF 2048

static void usage(void);
static void deferMessage(XMLEle * root);
static void handlePingReply(XMLEle * root);

static LilXML *clixml = NULL;

#define PROCEED_IMMEDIATE 1
#define PROCEED_DEFERRED 0
static int messageHandling = PROCEED_IMMEDIATE;


/* callback when INDI client message arrives on stdin.
 * collect and dispatch when see outter element closure.
 * exit if OS trouble or see incompatable INDI version.
 * arg is not used.
 */
static void clientMsgCB(int fd, void *arg)
{
    char buf[MAXRBUF], msg[MAXRBUF], *bp;
    int nr;

    (void) arg;

    // FIXME: not ready for receiving shared buffer blobs here
    // Also this is completely similar to indidriver code... Common code ?

    /* one read */
    nr = read(fd, buf, sizeof(buf));
    if (nr < 0)
    {
        if ((errno == EAGAIN) || (errno == EINTR))
        {
            return;
        }
        fprintf(stderr, "%s: %s\n", me, strerror(errno));
        exit(1);
    }
    if (nr == 0)
    {
        fprintf(stderr, "%s: EOF\n", me);
        exit(1);
    }

    /* crack and dispatch when complete */
    for (bp = buf; nr-- > 0; bp++)
    {
        XMLEle *root = readXMLEle(clixml, *bp, msg);
        if (root)
        {
            if (strcmp(tagXMLEle(root), "pingReply") == 0)
            {
                handlePingReply(root);
                delXMLEle(root);
                continue;
            }
            deferMessage(root);
        }
        else if (msg[0])
            fprintf(stderr, "%s XML error: %s\n", me, msg);
    }
}

typedef struct DeferredMessage
{
    XMLEle * root;
    struct DeferredMessage * next;
    struct DeferredMessage * prev;
} DeferredMessage;

// Messages will accumulate here until beeing processed
static DeferredMessage * firstDeferredMessage = NULL;
static DeferredMessage * lastDeferredMessage = NULL;

static void flushDeferredMessages(void * arg)
{
    DeferredMessage * p;
    char msg[MAXRBUF];

    (void) arg;

    while((p = firstDeferredMessage))
    {
        firstDeferredMessage = p->next;
        if (firstDeferredMessage)
        {
            firstDeferredMessage->prev = NULL;
        }
        else
        {
            lastDeferredMessage = NULL;
        }

        if (dispatch(p->root, msg) < 0)
            fprintf(stderr, "%s dispatch error: %s\n", me, msg);

        delXMLEle(p->root);
        free(p);
    }
}

static void deferMessage(XMLEle * root)
{
    if (firstDeferredMessage == NULL)
    {
        addImmediateWork(flushDeferredMessages, NULL);
    }

    DeferredMessage * newDeferredMessage = (DeferredMessage *)malloc(sizeof(DeferredMessage));
    newDeferredMessage->root = root;
    newDeferredMessage->next = NULL;
    newDeferredMessage->prev = lastDeferredMessage;
    if (lastDeferredMessage == NULL)
    {
        firstDeferredMessage = newDeferredMessage;
    }
    else
    {
        lastDeferredMessage->next = newDeferredMessage;
    }
    lastDeferredMessage = newDeferredMessage;
}

#define MAX_PING_UID_LEN 64

typedef struct PingReply
{
    struct PingReply * prev;
    struct PingReply * next;
    char uid[MAX_PING_UID_LEN + 1];
} PingReply;

static pthread_t eventLoopThread;
static PingReply * firstReceivedPing = NULL;
static PingReply * lastReceivedPing = NULL;
static pthread_mutex_t pingReplyMutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t pingReplyCond = PTHREAD_COND_INITIALIZER;

static void handlePingReply(XMLEle * root)
{
    XMLAtt *uidA = findXMLAtt(root, "uid");

    if (!uidA)
        return;

    char * uid = valuXMLAtt(uidA);
    if (!uid || !uid[0] || strlen(uid) > MAX_PING_UID_LEN) {
        return;
    }

    PingReply *pr = (PingReply *)malloc(sizeof(PingReply));
    strncpy(pr->uid, uid, MAX_PING_UID_LEN + 1);

    pthread_mutex_lock(&pingReplyMutex);

    pr->next = NULL;
    pr->prev = lastReceivedPing;
    if (lastReceivedPing) {
        lastReceivedPing->next = pr;
    } else {
        firstReceivedPing = pr;
    }
    lastReceivedPing = pr;
    pthread_cond_broadcast(&pingReplyCond);
    pthread_mutex_unlock(&pingReplyMutex);
}

// This must be called under protection of pingReplyMutex
static int consumePingReply(const char * uid) {
    PingReply * cur = firstReceivedPing;
    while(cur) {
        if (!strcmp(cur->uid, uid)) {

            if (cur->prev) {
                cur->prev->next = cur->next;
            } else {
                firstReceivedPing = cur->next;
            }
            if (cur->next) {
                cur->next->prev = cur->prev;
            } else {
                lastReceivedPing = cur->prev;
            }

            free(cur);
            return 1;
        }
        cur = cur->next;

    }
    return 0;
}

static void waitPingReplyFromEventLoopThread(const char * uid) {
    pthread_mutex_lock(&pingReplyMutex);
    while(!consumePingReply(uid)) {
        pthread_cond_wait(&pingReplyCond, &pingReplyMutex);
    }
    pthread_mutex_unlock(&pingReplyMutex);
}

static void waitPingReplyFromOtherThread(const char * uid) {
    int fd = 0;
    fd_set rfd;

    messageHandling = PROCEED_DEFERRED;
    pthread_mutex_lock(&pingReplyMutex);
    while(!consumePingReply(uid))
    {

        pthread_mutex_unlock(&pingReplyMutex);

        FD_ZERO(&rfd);
        FD_SET(fd, &rfd);

        int ns = select(fd + 1, &rfd, NULL, NULL, NULL);
        if (ns < 0)
        {
            perror("select");
            exit(1);
        }

        clientMsgCB(0, NULL);

        pthread_mutex_lock(&pingReplyMutex);
    }
    pthread_mutex_unlock(&pingReplyMutex);
    messageHandling = PROCEED_IMMEDIATE;
}

void waitPingReply(const char * uid) {
    // Check if same thread than eventloop

    pthread_t currentThread = pthread_self();
    if (!pthread_equal(currentThread, eventLoopThread)) {
        waitPingReplyFromOtherThread(uid);
    } else {
        waitPingReplyFromEventLoopThread(uid);
    }
}

int main(int ac, char *av[])
{
#ifndef _WIN32
    int ret = 0;

    if ( (ret = setgid(getgid())) != 0)
        IDLog("setgid: %s", strerror(ret));

    if ( (ret = setuid(getuid())) != 0)
        IDLog("getuid: %s", strerror(ret));

    if (geteuid() != getuid())
        exit(255);
#endif

    eventLoopThread = pthread_self();

    /* save handy pointer to our base name */
    // #PS: maybe use 'program_invocation_short_name'?
    for (me = av[0]; av[0][0]; av[0]++)
        if (av[0][0] == '/')
            me = &av[0][1];

    /* crack args */
    while (--ac && (*++av)[0] == '-')
        while (*++(*av))
            switch (*(*av))
            {
                case 'v': /* verbose */
                    verbose++;
                    break;
                default:
                    usage();
            }

    /* ac remaining args starting at av[0] */
    if (ac > 0)
        usage();

    /* init */
    clixml = newLilXML();
    addCallback(0, clientMsgCB, clixml);

    /* service client */
    eventLoop();

    /* eh?? */
    fprintf(stderr, "%s: inf loop ended\n", me);
    return (1);
}

/* print usage message and exit (1) */
static void usage(void)
{
    fprintf(stderr, "Usage: %s [options]\n", me);
    fprintf(stderr, "Purpose: INDI Device driver framework.\n");
    fprintf(stderr, "Options:\n");
    fprintf(stderr, " -v    : more verbose to stderr\n");

    exit(1);
}
