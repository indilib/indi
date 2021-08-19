#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>

#include "indidriver.h"
#include "userio.h"
#include "indiuserio.h"
#include "indidriverio.h"

/* Buffer size. Must be ^ 2 */
#define OUTPUTBUFF_ALLOC 4096

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

void driverio_init(driverio * dio)
{
    dio->userio.vprintf = &driverio_vprintf;
    dio->userio.write = &driverio_write;
    dio->userio.joinbuff = NULL;
    dio->user = (void*)dio;
    dio->fds = NULL;
    dio->fdCount = 0;
    dio->outBuff = NULL;
    dio->outPos = 0;
}

void driverio_finish(driverio * dio)
{
    if (dio->outPos) {
        write(1, dio->outBuff, dio->outPos);
    }

    for(int i = 0; i < dio->fdCount; ++i) {
        close(dio->fds[i]);
    }

    if (dio->outBuff != NULL) {
        free(dio->outBuff);
    }
    if (dio->fds != NULL) {
        free(dio->fds);
    }
}
