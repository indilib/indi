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
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#endif
#ifdef ENABLE_INDI_SHARED_MEMORY
#include "shm_open_anon.h"
#endif

// A shared buffer will be allocated by chunk of at least 1M (must be ^ 2)
#define BLOB_SIZE_UNIT 0x100000

static pthread_mutex_t shared_buffer_mutex = PTHREAD_MUTEX_INITIALIZER;

typedef struct shared_buffer
{
#ifdef _WIN32
    HANDLE hMapObject;
#endif
    void* mapstart;
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

void* IDSharedBlobAlloc(size_t size) {
#ifdef ENABLE_INDI_SHARED_MEMORY
    shared_buffer* sb = (shared_buffer*)malloc(sizeof(shared_buffer));
    if (sb == NULL) {
        // 错误处理逻辑
        goto ERROR_LABEL;
    }

    sb->size = size;
    sb->allocated = allocation(size);
    sb->sealed = 0;
    sb->fd = -1;

#ifdef _WIN32
    SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.lpSecurityDescriptor = NULL;
    sa.bInheritHandle = TRUE;

    sb->hMapObject = CreateFileMapping(INVALID_HANDLE_VALUE, &sa, PAGE_READWRITE, 0, sb->allocated, NULL);
    if (sb->hMapObject == NULL) {
        goto ERROR_LABEL;
    }

    sb->mapstart = MapViewOfFile(sb->hMapObject, FILE_MAP_ALL_ACCESS, 0, 0, sb->allocated);
    if (sb->mapstart == NULL) {
        goto ERROR_LABEL;
    }
#else
    int ret = ftruncate(sb->fd, sb->allocated);
    if (ret == -1) goto ERROR_LABEL;

    // FIXME: try to map far more than sb->allocated, to allow efficient mremap
    sb->mapstart = mmap(0, sb->allocated, PROT_READ | PROT_WRITE, MAP_SHARED, sb->fd, 0);
    if (sb->mapstart == MAP_FAILED) goto ERROR_LABEL;
#endif

    sharedBufferAdd(sb);

    // 返回正常的结果
    return sb->mapstart;

ERROR_LABEL:
    if (sb) {
#ifdef _WIN32
        DWORD e = GetLastError();
        if (sb->mapstart) UnmapViewOfFile(sb->mapstart);
        if (sb->hMapObject) CloseHandle(sb->hMapObject);
#else
        int e = errno;
        if (sb->mapstart) munmap(sb->mapstart, sb->allocated);
        if (sb->fd != -1) close(sb->fd);
#endif
        free(sb);
#ifdef _WIN32
        SetLastError(e);
#else
        errno = e;
#endif
    }
    
    // 返回错误结果
    return NULL;
#else
    return malloc(size);
#endif
}

#ifdef _WIN32
void* IDSharedBlobAttach(int fd, size_t size) {
    shared_buffer* sb = (shared_buffer*)malloc(sizeof(shared_buffer));
    if (sb == NULL) goto ERROR_LABEL;
    sb->fd = fd;
    sb->size = size;
    sb->allocated = size;
    sb->sealed = 1;

    sb->mapstart = MapViewOfFile((HANDLE)_get_osfhandle(fd), FILE_MAP_READ, 0, 0, size);
    if (sb->mapstart == NULL) goto ERROR_LABEL;

    sharedBufferAdd(sb);

    return sb->mapstart;

ERROR_LABEL:
    if (sb) {
        DWORD e = GetLastError();
        free(sb);
        SetLastError(e);
    }
    return NULL;
}
#else
void* IDSharedBlobAttach(int fd, size_t size) {
    shared_buffer* sb = (shared_buffer*)malloc(sizeof(shared_buffer));
    if (sb == NULL) goto ERROR_LABEL;
    sb->fd = fd;
    sb->size = size;
    sb->allocated = size;
    sb->sealed = 1;

    sb->mapstart = mmap(NULL, sb->allocated, PROT_READ, MAP_SHARED, sb->fd, 0);
    if (sb->mapstart == MAP_FAILED) goto ERROR_LABEL;

    sharedBufferAdd(sb);

    return sb->mapstart;

ERROR_LABEL:
    if (sb) {
        int e = errno;
        free(sb);
        errno = e;
    }
    return NULL;
}
#endif

#ifdef _WIN32
void IDSharedBlobFree(void* ptr) {
    shared_buffer* sb = sharedBufferRemove(ptr);
    if (sb == NULL) {
        // 不是连接到共享内存的内存块
        free(ptr);
        return;
    }

    if (!UnmapViewOfFile(sb->mapstart)) {
        perror("shared buffer UnmapViewOfFile");
        _exit(1);
    }
    if (!CloseHandle(sb->hMapObject)) {
        perror("shared buffer CloseHandle");
    }
    free(sb);
}
#else
void IDSharedBlobFree(void* ptr) {
    shared_buffer* sb = sharedBufferRemove(ptr);
    if (sb == NULL) {
        // 不是连接到共享内存的内存块
        free(ptr);
        return;
    }

    if (munmap(sb->mapstart, sb->allocated) == -1) {
        perror("shared buffer munmap");
        _exit(1);
    }
    if (close(sb->fd) == -1) {
        perror("shared buffer close");
    }
    free(sb);
}
#endif

#ifdef _WIN32
void IDSharedBlobDettach(void* ptr)
{
    shared_buffer* sb = sharedBufferRemove(ptr);
    if (sb == NULL)
    {
        free(ptr);
        return;
    }
    if (UnmapViewOfFile(sb->mapstart) == 0)
    {
        perror("shared buffer UnmapViewOfFile");
        exit(1);
    }
    free(sb);
}
#else
void IDSharedBlobDettach(void* ptr)
{
    shared_buffer* sb = sharedBufferRemove(ptr);
    if (sb == NULL)
    {
        free(ptr);
        return;
    }
    if (munmap(sb->mapstart, sb->allocated) == -1)
    {
        perror("shared buffer munmap");
        exit(1);
    }
    free(sb);
}
#endif


#ifdef _WIN32
void* IDSharedBlobRealloc(void* ptr, size_t size)
{
    if (ptr == NULL)
    {
        return IDSharedBlobAlloc(size);
    }

    shared_buffer* sb;
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
        // 缩小尺寸未实现
        sb->size = size;
        return ptr;
    }

    size_t reallocated = allocation(size);
    if (reallocated == sb->allocated)
    {
        sb->size = size;
        return ptr;
    }

    // 在Windows平台上使用VirtualAlloc重新分配内存，需要先取消当前内存映射，再重新映射
    if (UnmapViewOfFile(sb->mapstart) == 0)
    {
        perror("shared buffer UnmapViewOfFile");
        _exit(1);
    }
    void* remapped = VirtualAlloc(NULL, reallocated, MEM_COMMIT, PAGE_READWRITE);
    if (remapped == NULL)
    {
        perror("shared buffer VirtualAlloc");
        _exit(1);
    }
    void* ret = MapViewOfFileEx(sb->hMapObject, FILE_MAP_WRITE, 0, 0, 0, remapped);
    if (ret == NULL)
    {
        perror("shared buffer MapViewOfFileEx");
        _exit(1);
    }

    sb->size = size;
    sb->allocated = reallocated;
    sb->mapstart = remapped;

    return remapped;
}

static void seal(shared_buffer* sb)
{
    void* ret = MapViewOfFileEx(sb->hMapObject, FILE_MAP_READ, 0, 0, 0, sb->mapstart);
    if (ret == NULL)
    {
        perror("remap readonly failed");
        _exit(1);
    }
    sb->sealed = 1;
}
#else
void* IDSharedBlobRealloc(void* ptr, size_t size)
{
    if (ptr == NULL)
    {
        return IDSharedBlobAlloc(size);
    }

    shared_buffer* sb;
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
        // 缩小尺寸未实现
        sb->size = size;
        return ptr;
    }

    size_t reallocated = allocation(size);
    if (reallocated == sb->allocated)
    {
        sb->size = size;
        return ptr;
    }

#ifdef HAVE_MREMAP
    void* remapped = mremap(sb->mapstart, sb->allocated, reallocated, MREMAP_MAYMOVE);
    if (remapped == MAP_FAILED)
    {
        perror("shared buffer mremap");
        _exit(1);
    }
#else
    // 兼容MACOS平台的路径
    if (munmap(sb->mapstart, sb->allocated) == -1)
    {
        perror("shared buffer munmap");
        _exit(1);
    }
    void* remapped = mmap(0, reallocated, PROT_READ | PROT_WRITE, MAP_SHARED, sb->fd, 0);
    if (remapped == MAP_FAILED)
    {
        perror("shared buffer mmap");
        _exit(1);
    }
#endif

    sb->size = size;
    sb->allocated = reallocated;
    sb->mapstart = remapped;

    return remapped;
}

static void seal(shared_buffer* sb)
{
    void* ret = mmap(sb->mapstart, sb->allocated, PROT_READ, MAP_SHARED | MAP_FIXED, sb->fd, 0);
    if (ret == MAP_FAILED)
    {
        perror("remap readonly failed");
        _exit(1);
    }
    sb->sealed = 1;
}
#endif

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
