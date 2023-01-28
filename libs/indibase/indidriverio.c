#if 0
INDI Driver Functions

Copyright (C) 2022 by Ludovic Pollet

This library is free software;
you can redistribute it and / or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation;
either
version 2.1 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
     but WITHOUT ANY WARRANTY;
without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library;
if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110 - 1301  USA

#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <errno.h>
#include <pthread.h>

#include "indidriver.h"
#include "userio.h"
#include "indiuserio.h"
#include "indidriverio.h"



/* Buffer size. Must be ^ 2 */
#define OUTPUTBUFF_ALLOC 32768

/* Dump whole buffer when growing over this */
#define OUTPUTBUFF_FLUSH_THRESOLD 65536

#define MAXFD_PER_MESSAGE 16

static void driverio_flush(driverio * dio, const void * additional, size_t add_size);

static pthread_mutex_t stdout_mutex = PTHREAD_MUTEX_INITIALIZER;

/* Return the buffer size required for storage (rounded to next OUTPUTBUFF_ALLOC) */
static unsigned int outBuffRequired(unsigned int storage)
{
    return (storage + OUTPUTBUFF_ALLOC - 1) & ~(OUTPUTBUFF_ALLOC - 1);
}

static int outBuffAllocated(struct driverio * dio)
{
    return outBuffRequired(dio->outPos);
}

static void outBuffGrow(struct driverio * dio, int required)
{
    dio->outBuff = realloc(dio->outBuff, required);
    if (dio->outBuff == NULL)
    {
        perror("malloc");
        _exit(1);
    }
}

static ssize_t driverio_write(void *user, const void * ptr, size_t count)
{
    struct driverio * dio = (struct driverio*) user;

    if (dio->outPos + count > OUTPUTBUFF_FLUSH_THRESOLD)
    {
        driverio_flush(dio, ptr, count);
    }
    else
    {
        unsigned int allocated = outBuffAllocated(dio);
        unsigned int required = outBuffRequired(dio->outPos + count);
        if (required != allocated)
        {
            outBuffGrow(dio, required);
        }
        memcpy(dio->outBuff + dio->outPos, ptr, count);

        dio->outPos += count;
    }
    return count;
}

static int driverio_vprintf(void *user, const char * fmt, va_list arg)
{
    struct driverio * dio = (struct driverio*) user;
    int available;
    int size = 0;

    unsigned int allocated = outBuffAllocated(dio);
    while(1)
    {
        available = allocated - dio->outPos;
        /* Determine required size */
        size = vsnprintf(dio->outBuff + dio->outPos, available, fmt, arg);

        if (size < 0)
            return size;

        if (size < available)
        {
            break;
        }
        allocated = outBuffRequired(available + size + 1);
        outBuffGrow(dio, allocated);
    }
    dio->outPos += size;
    return size;
}

static void driverio_join(void * user, const char * xml, void * blob, size_t bloblen)
{
    struct driverio * dio = (struct driverio*) user;
    dio->joinCount++;
    dio->joins = (void **)realloc((void*)dio->joins, sizeof(void*) * dio->joinCount);
    dio->joinSizes = (size_t *)realloc((void*)dio->joinSizes, sizeof(size_t) * dio->joinCount);

    dio->joins[dio->joinCount - 1] = blob;
    dio->joinSizes[dio->joinCount - 1] = bloblen;

    driverio_write(user, xml, strlen(xml));
}


static void driverio_flush(driverio * dio, const void * additional, size_t add_size)
{
    struct msghdr msgh;
    struct iovec iov[2];
    int cmsghdrlength;
    struct cmsghdr * cmsgh;

    if (dio->outPos + add_size)
    {
        int ret = -1;
        void ** temporaryBuffers = NULL;
        int fdCount = dio->joinCount;
        if (fdCount > 0)
        {

            if (dio->joinCount > MAXFD_PER_MESSAGE)
            {
                errno = EMSGSIZE;
                perror("sendmsg");
                exit(1);
            }

            cmsghdrlength = CMSG_SPACE((fdCount * sizeof(int)));
            cmsgh = (struct cmsghdr*)malloc(cmsghdrlength);
            // FIXME: abort on alloc error here
            temporaryBuffers = (void**)malloc(sizeof(void*)*fdCount);

            /* Write the fd as ancillary data */
            cmsgh->cmsg_len = CMSG_LEN(sizeof(int));
            cmsgh->cmsg_level = SOL_SOCKET;
            cmsgh->cmsg_type = SCM_RIGHTS;
            msgh.msg_control = cmsgh;
            msgh.msg_controllen = cmsghdrlength;
            for(int i = 0; i < fdCount; ++i)
            {
                void * blob = dio->joins[i];
                size_t size = dio->joinSizes[i];

                int fd = IDSharedBlobGetFd(blob);
                if (fd == -1)
                {
                    // Can't avoid a copy here. Update the driver to change that
                    temporaryBuffers[i] = IDSharedBlobAlloc(size);
                    memcpy(temporaryBuffers[i], blob, size);
                    fd = IDSharedBlobGetFd(temporaryBuffers[i]);
                }
                else
                {
                    temporaryBuffers[i] = NULL;
                }

                ((int *) CMSG_DATA(CMSG_FIRSTHDR(&msgh)))[i] = fd;
            }
        }
        else
        {
            cmsgh = NULL;
            cmsghdrlength = 0;
            msgh.msg_control = cmsgh;
            msgh.msg_controllen = cmsghdrlength;
        }

        iov[0].iov_base = dio->outBuff;
        iov[0].iov_len = dio->outPos;
        if (add_size)
        {
            iov[1].iov_base = (void*)additional;
            iov[1].iov_len = add_size;
        }

        msgh.msg_flags = 0;
        msgh.msg_name = NULL;
        msgh.msg_namelen = 0;
        msgh.msg_iov = iov;
        msgh.msg_iovlen = add_size ? 2 : 1;

        if (!dio->locked)
        {
            pthread_mutex_lock(&stdout_mutex);
            dio->locked = 1;
        }

        ret = sendmsg(1, &msgh, 0);
        if (ret == -1)
        {
            perror("sendmsg");
            // FIXME: exiting the driver seems abrupt. Is this the right thing to do ? what about cleanup ?
            exit(1);
        }
        else if ((unsigned)ret != dio->outPos + add_size)
        {
            // This is not expected on blocking socket
            fprintf(stderr, "short write\n");
            exit(1);
        }

        if (fdCount > 0)
        {
            for(int i = 0; i < fdCount; ++i)
            {
                if (temporaryBuffers[i] != NULL)
                {
                    IDSharedBlobFree(temporaryBuffers[i]);
                }
            }
            free(cmsgh);
            free(temporaryBuffers);
        }
    }

    if (dio->joins != NULL)
    {
        free(dio->joins);
    }
    dio->joins = NULL;

    if (dio->joinSizes != NULL)
    {
        free(dio->joinSizes);
    }
    dio->joinSizes = NULL;

    if (dio->outBuff != NULL)
    {
        free(dio->outBuff);
    }
    dio->outPos = 0;

}


static int driverio_is_unix = -1;

static int is_unix_io()
{
#ifndef ENABLE_INDI_SHARED_MEMORY
    return 0;
#endif
    if (driverio_is_unix != -1)
    {
        return driverio_is_unix;
    }

#ifdef SO_DOMAIN
    int domain;
    socklen_t result = sizeof(domain);

    if (getsockopt(1, SOL_SOCKET, SO_DOMAIN, (void*)&domain, &result) == -1)
    {
        driverio_is_unix = 0;
    }
    else if (result != sizeof(domain) || domain != AF_UNIX)
    {
        driverio_is_unix = 0;
    }
    else
    {
        driverio_is_unix = 1;
    }
#else
    struct sockaddr_un sockName;
    socklen_t sockNameLen = sizeof(sockName);

    if (getsockname(1, (struct sockaddr*)&sockName, &sockNameLen) == -1)
    {
        driverio_is_unix = 0;
    }
    else if (sockName.sun_family == AF_UNIX)
    {
        driverio_is_unix = 1;
    }
    else
    {
        driverio_is_unix = 0;
    }
#endif
    return driverio_is_unix;
}

/* Unix io allow attaching buffer in ancillary data. */
static void driverio_init_unix(driverio * dio)
{
    dio->userio.vprintf = &driverio_vprintf;
    dio->userio.write = &driverio_write;
    dio->userio.joinbuff = &driverio_join;
    dio->user = (void*)dio;
    dio->joins = NULL;
    dio->joinSizes = NULL;
    dio->locked = 0;
    dio->joinCount = 0;
    dio->outBuff = NULL;
    dio->outPos = 0;
}

static void driverio_finish_unix(driverio * dio)
{
    driverio_flush(dio, NULL, 0);
    if (dio->locked)
    {
        pthread_mutex_unlock(&stdout_mutex);
        dio->locked = 0;
    }
}

static void driverio_init_stdout(driverio * dio)
{
    dio->userio = *userio_file();
    dio->user = stdout;
    pthread_mutex_lock(&stdout_mutex);
}

static void driverio_finish_stdout(driverio * dio)
{
    (void)dio;
    fflush(stdout);
    pthread_mutex_unlock(&stdout_mutex);
}

void driverio_init(driverio * dio)
{
    if (is_unix_io())
    {
        driverio_init_unix(dio);
    }
    else
    {
        driverio_init_stdout(dio);
    }
}

void driverio_finish(driverio * dio)
{
    if (is_unix_io())
    {
        driverio_finish_unix(dio);
    }
    else
    {
        driverio_finish_stdout(dio);
    }
}
