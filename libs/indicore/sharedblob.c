/** INDI
 *  Copyright (C) 2022 by Ludovic Pollet
 *
 *  This library is free software;
 *  you can redistribute it and / or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation;
 *  either
 *  version 2.1 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *       but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library;
 *  if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110 - 1301  USA
 */

#define _GNU_SOURCE

#ifdef __linux__
#include <linux/unistd.h>
#endif

#include <pthread.h>
#include <stdlib.h>
#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/mman.h>

#ifdef ENABLE_INDI_SHARED_MEMORY
#include "shm_open_anon.h"
#endif

// A shared buffer will be allocated by chunk of at least 1M (must be ^ 2)
#define BLOB_SIZE_UNIT 0x100000

static pthread_mutex_t shared_buffer_mutex = PTHREAD_MUTEX_INITIALIZER;

typedef struct shared_buffer
{
    void * mapstart;
    size_t size;
    size_t allocated;
    int fd;
    int sealed;
    struct shared_buffer * prev, *next;
} shared_buffer;

/* Return the buffer size required for storage (rounded to next BLOB_SIZE_UNIT) */
static size_t allocation(size_t storage)
{
    if (storage == 0)
    {
        return BLOB_SIZE_UNIT;
    }
    return (storage + BLOB_SIZE_UNIT - 1) & ~(BLOB_SIZE_UNIT - 1);
}

static void sharedBufferAdd(shared_buffer * sb);
static shared_buffer * sharedBufferRemove(void * mapstart);
static shared_buffer * sharedBufferFind(void * mapstart);


void * IDSharedBlobAlloc(size_t size)
{
#ifdef ENABLE_INDI_SHARED_MEMORY
    shared_buffer * sb = (shared_buffer*)malloc(sizeof(shared_buffer));
    if (sb == NULL) goto ERROR;

    sb->size = size;
    sb->allocated = allocation(size);
    sb->sealed = 0;
    sb->fd = shm_open_anon();
    if (sb->fd == -1)  goto ERROR;

    int ret = ftruncate(sb->fd, sb->allocated);
    if (ret == -1) goto ERROR;

    // FIXME: try to map far more than sb->allocated, to allow efficient mremap
    sb->mapstart = mmap(0, sb->allocated, PROT_READ | PROT_WRITE, MAP_SHARED, sb->fd, 0);
    if (sb->mapstart == MAP_FAILED) goto ERROR;

    sharedBufferAdd(sb);

    return sb->mapstart;
ERROR:
    if (sb)
    {
        int e = errno;
        if (sb->fd != -1) close(sb->fd);
        free(sb);
        errno = e;
    }
    return NULL;
#else
    return malloc(size);
#endif
}

void * IDSharedBlobAttach(int fd, size_t size)
{
    shared_buffer * sb = (shared_buffer*)malloc(sizeof(shared_buffer));
    if (sb == NULL) goto ERROR;
    sb->fd = fd;
    sb->size = size;
    sb->allocated = size;
    sb->sealed = 1;

    sb->mapstart = mmap(0, sb->allocated, PROT_READ, MAP_SHARED, sb->fd, 0);
    if (sb->mapstart == MAP_FAILED) goto ERROR;

    sharedBufferAdd(sb);

    return sb->mapstart;
ERROR:
    if (sb)
    {
        int e = errno;
        free(sb);
        errno = e;
    }
    return NULL;
}


void IDSharedBlobFree(void * ptr)
{
    shared_buffer * sb = sharedBufferRemove(ptr);
    if (sb == NULL)
    {
        // Not a memory attached to a blob
        free(ptr);
        return;
    }

    if (munmap(sb->mapstart, sb->allocated) == -1)
    {
        perror("shared buffer munmap");
        _exit(1);
    }
    if (close(sb->fd) == -1)
    {
        perror("shared buffer close");
    }
    free(sb);
}

void IDSharedBlobDettach(void * ptr)
{
    shared_buffer * sb = sharedBufferRemove(ptr);
    if (sb == NULL)
    {
        // Not a memory attached to a blob
        free(ptr);
        return;
    }
    if (munmap(sb->mapstart, sb->allocated) == -1)
    {
        perror("shared buffer munmap");
        _exit(1);
    }
    free(sb);
}

void * IDSharedBlobRealloc(void * ptr, size_t size)
{
    if (ptr == NULL)
    {
        return IDSharedBlobAlloc(size);
    }

    shared_buffer * sb;
    sb = sharedBufferFind(ptr);

    if (sb == NULL)
    {
        return realloc(ptr, size);
    }

    if (sb->sealed)
    {
        IDSharedBlobFree(ptr);
        errno = EROFS;
        return NULL;
    }

    if (sb->size >= size)
    {
        // Shrinking is not implemented
        sb->size = size;
        return ptr;
    }

    size_t reallocated = allocation(size);
    if (reallocated == sb->allocated)
    {
        sb->size = size;
        return ptr;
    }

    int ret = ftruncate(sb->fd, reallocated);
    if (ret == -1) return NULL;

#ifdef HAVE_MREMAP
    void * remaped = mremap(sb->mapstart, sb->allocated, reallocated, MREMAP_MAYMOVE);
    if (remaped == MAP_FAILED) return NULL;

#else
    // compatibility path for MACOS
    if (munmap(sb->mapstart, sb->allocated) == -1)
    {
        perror("shared buffer munmap");
        _exit(1);
    }
    void * remaped = mmap(0, reallocated, PROT_READ | PROT_WRITE, MAP_SHARED, sb->fd, 0);
    if (remaped == MAP_FAILED) return NULL;
#endif
    sb->size = size;
    sb->allocated = reallocated;
    sb->mapstart = remaped;

    return remaped;
}

static void seal(shared_buffer * sb)
{
    void * ret = mmap(sb->mapstart, sb->allocated, PROT_READ, MAP_SHARED | MAP_FIXED, sb->fd, 0);
    if (ret == MAP_FAILED)
    {
        perror("remap readonly failed");
    }
    sb->sealed = 1;
}

int IDSharedBlobGetFd(void * ptr)
{
    shared_buffer * sb;
    sb = sharedBufferFind(ptr);
    if (sb == NULL)
    {
        errno = EINVAL;
        return -1;
    }

    // Make sure a shared blob is not modified after sharing
    seal(sb);

    return sb->fd;
}

void IDSharedBlobSeal(void * ptr)
{
    shared_buffer * sb;
    sb = sharedBufferFind(ptr);
    if (sb->sealed) return;
    seal(sb);
}

static shared_buffer * first = NULL, *last = NULL;

static void sharedBufferAdd(shared_buffer * sb)
{
    pthread_mutex_lock(&shared_buffer_mutex);
    // Chained insert at start
    sb->prev = NULL;
    sb->next = first;
    if (first)
    {
        first->prev = sb;
    }
    else
    {
        last = sb;
    }
    first = sb;
    pthread_mutex_unlock(&shared_buffer_mutex);
}
static shared_buffer * sharedBufferFindUnlocked(void * mapstart)
{
    shared_buffer * sb = first;
    while(sb)
    {
        if (sb->mapstart == mapstart)
        {
            return sb;
        }
        sb = sb->next;
    }
    return NULL;
}

static shared_buffer * sharedBufferRemove(void * mapstart)
{
    pthread_mutex_lock(&shared_buffer_mutex);
    shared_buffer * sb  = sharedBufferFindUnlocked(mapstart);
    if (sb != NULL)
    {
        if (sb->prev)
        {
            sb->prev->next = sb->next;
        }
        else
        {
            first = sb->next;
        }
        if (sb->next)
        {
            sb->next->prev = sb->prev;
        }
        else
        {
            last = sb->prev;
        }
    }
    pthread_mutex_unlock(&shared_buffer_mutex);
    return sb;
}

static shared_buffer * sharedBufferFind(void * mapstart)
{
    pthread_mutex_lock(&shared_buffer_mutex);
    shared_buffer * sb  = sharedBufferFindUnlocked(mapstart);
    pthread_mutex_unlock(&shared_buffer_mutex);
    return sb;
}
