#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <errno.h>
#include <pthread.h>

#include "indidriver.h"
#include "userio.h"
#include "indiuserio.h"
#include "indidriverio.h"



/* Buffer size. Must be ^ 2 */
#define OUTPUTBUFF_ALLOC 4096

#define MAXFD_PER_MESSAGE 16

static pthread_mutex_t stdout_mutex = PTHREAD_MUTEX_INITIALIZER;

/* Return the buffer size required for storage (rounded to next OUTPUTBUFF_ALLOC) */
static unsigned int outBuffRequired(unsigned int storage) {
    return (storage + OUTPUTBUFF_ALLOC - 1) & ~(OUTPUTBUFF_ALLOC - 1);
}

static int outBuffAllocated(struct driverio * dio) {
    return outBuffRequired(dio->outPos);
}

static void outBuffGrow(struct driverio * dio, int required) {
    dio->outBuff = realloc(dio->outBuff, required);
    if (dio->outBuff == NULL) {
        perror("malloc");
        _exit(1);
    }
}

static size_t driverio_write(void *user, const void * ptr, size_t count)
{
    struct driverio * dio = (struct driverio*) user;
    unsigned int allocated = outBuffAllocated(dio);
    unsigned int required = outBuffRequired(dio->outPos + count);
    if (required != allocated) {
        outBuffGrow(dio, required);
    }
    memcpy(dio->outBuff + dio->outPos, ptr, count);

    dio->outPos += count;

    return count;
}

static int driverio_vprintf(void *user, const char * fmt, va_list arg)
{
    struct driverio * dio = (struct driverio*) user;
    int available;
    int size = 0;

    unsigned int allocated = outBuffAllocated(dio);
    while(1) {
        available = allocated - dio->outPos;
        /* Determine required size */
        size = vsnprintf(dio->outBuff + dio->outPos, available, fmt, arg);

        if (size < 0)
            return size;

        if (size < available) {
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
    driverio_write(user, xml, strlen(xml));
    struct driverio * dio = (struct driverio*) user;
    dio->joinCount++;
    dio->joins = (void **)realloc((void*)dio->joins, sizeof(void*) * dio->joinCount);
    dio->joinSizes = (size_t *)realloc((void*)dio->joinSizes, sizeof(size_t) * dio->joinCount);

    dio->joins[dio->joinCount - 1] = blob;
    dio->joinSizes[dio->joinCount - 1] = bloblen;
}

static int driverio_is_unix = -1;

static int is_unix_io() {
    if (driverio_is_unix != -1) {
        return driverio_is_unix;
    }
    int domain;
    socklen_t result = sizeof(domain);
    if (getsockopt(1, SOL_SOCKET, SO_DOMAIN, (void*)&domain, &result) == -1) {
        driverio_is_unix = 0;
    } else if (result != sizeof(domain) || domain != AF_UNIX) {
        driverio_is_unix = 0;
    } else {
        driverio_is_unix = 1;
    }
    return driverio_is_unix;
}

/* Unix io allow attaching buffer in ancillary data. */
static void driverio_init_unix(driverio * dio) {
    dio->userio.vprintf = &driverio_vprintf;
    dio->userio.write = &driverio_write;
    dio->userio.joinbuff = &driverio_join;
    dio->user = (void*)dio;
    dio->joins = NULL;
    dio->joinSizes = NULL;
    dio->joinCount = 0;
    dio->outBuff = NULL;
    dio->outPos = 0;
}

static void driverio_finish_unix(driverio * dio) {
    struct msghdr msgh;
    struct iovec iov;
    int cmsghdrlength;
    struct cmsghdr * cmsgh;

    if (dio->outPos) {
        int ret = -1;
        void ** temporaryBuffers;
        if (dio->joinCount > 0) {
            if (dio->joinCount > MAXFD_PER_MESSAGE) {
                errno = EMSGSIZE;
                perror("sendmsg");
                exit(1);
            }

            cmsghdrlength = CMSG_SPACE((dio->joinCount * sizeof(int)));
            cmsgh = (struct cmsghdr*)malloc(cmsghdrlength);
            // FIXME: abort on alloc error here
            temporaryBuffers = (void**)malloc(sizeof(void*)*dio->joinCount);

            /* Write the fd as ancillary data */
            cmsgh->cmsg_len = CMSG_LEN(sizeof(int));
            cmsgh->cmsg_level = SOL_SOCKET;
            cmsgh->cmsg_type = SCM_RIGHTS;
            msgh.msg_control = cmsgh;
            msgh.msg_controllen = cmsghdrlength;
            for(int i = 0; i < dio->joinCount; ++i) {
                void * blob = dio->joins[i];

                int fd = IDSharedBlobGetFd(blob);
                if (fd == -1) {
                    // Can't avoid a copy here. Update the driver to change that
                    temporaryBuffers[i] = IDSharedBlobAlloc(dio->joinSizes[i]);
                    memcpy(temporaryBuffers[i], blob, dio->joinSizes[i]);
                    fd = IDSharedBlobGetFd(temporaryBuffers[i]);
                } else {
                    temporaryBuffers[i] = NULL;
                }

                ((int *) CMSG_DATA(CMSG_FIRSTHDR(&msgh)))[i] = fd;
            }
        } else {
            cmsgh = NULL;
            cmsghdrlength = 0;
            msgh.msg_control = cmsgh;
            msgh.msg_controllen = cmsghdrlength;
        }

        iov.iov_base = dio->outBuff;
        iov.iov_len = dio->outPos;
        msgh.msg_flags = 0;
        msgh.msg_name = NULL;
        msgh.msg_namelen = 0;
        msgh.msg_iov = &iov;
        msgh.msg_iovlen = 1;

        ret = sendmsg(1, &msgh, 0);
        if (ret == -1) {
            perror("sendmsg");
        } else if ((unsigned)ret != dio->outPos) {
            fprintf(stderr, "short write\n");
        }

        if (dio->joinCount > 0) {
            for(int i = 0; i < dio->joinCount; ++i) {
                if (temporaryBuffers[i] != NULL) {
                    IDSharedBlobFree(temporaryBuffers[i]);
                }
            }
            free(cmsgh);
            free(temporaryBuffers);
        }
    }

    if (dio->outBuff != NULL) {
        free(dio->outBuff);
    }
    if (dio->joins != NULL) {
        free(dio->joins);
    }
    if (dio->joinSizes != NULL) {
        free(dio->joinSizes);
    }
}

static void driverio_init_stdout(driverio * dio) {
    dio->userio = *userio_file();
    dio->user = stdout;
    pthread_mutex_lock(&stdout_mutex);
}

static void driverio_finish_stdout(driverio * dio) {
    fflush(stdout);
    pthread_mutex_unlock(&stdout_mutex);
}

void driverio_init(driverio * dio)
{
    if (is_unix_io()) {
        driverio_init_unix(dio);
    } else {
        driverio_init_stdout(dio);
    }
}

void driverio_finish(driverio * dio)
{
    if (is_unix_io()) {
        driverio_finish_unix(dio);
    } else {
        driverio_finish_stdout(dio);
    }
}
