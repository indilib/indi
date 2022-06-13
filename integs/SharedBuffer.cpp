extern "C" {
// Load shm_open_anon but force it to have static symbol only
static int shm_open_anon(void);
#include "../shm_open_anon.c"
}

#include <system_error>

#include <SharedBuffer.h>


#ifdef __linux__
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <linux/unistd.h>
#endif

#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>

#include <string.h>

#include <sys/mman.h>


SharedBuffer::SharedBuffer() {
    fd = -1;
    size = 0;
}

SharedBuffer::~SharedBuffer() {
    release();
}

void SharedBuffer::attach(int fd) {
    release();
    this->fd = fd;
    if (fd == -1) {
        size = 0;
        return;
    }

    struct stat sb;

    if (fstat(fd, &sb) == -1) {
        int e = errno;
        throw std::system_error(e, std::generic_category(), "Unable to stat buffer");
    }

    size = sb.st_size;
}

void SharedBuffer::release() {
    if (fd != -1) {
        close(fd);
        fd = -1;
    }
    size = 0;
}

int SharedBuffer::getFd() const {
    if (fd == -1) {
        throw std::runtime_error("SharedBuffer is not allocated");
    }
    return fd;
}


void SharedBuffer::write(const void * data, ssize_t offset, ssize_t writeSize) {
    if (writeSize == 0) {
        return;
    }

    if (size < offset + writeSize) {
        throw std::runtime_error("Attempt to write beyond past");
    }

    void * mmapped = mmap(0, size, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
    if (mmapped == MAP_FAILED) {
        perror("mmap");
        throw std::runtime_error("Mmap failed");
    }

    memcpy((void*)((char*)mmapped + offset), data, writeSize);

    if (munmap(mmapped, size) == -1) {
        perror("munmap");
        throw std::runtime_error("Munmap failed");
    }

}

void SharedBuffer::allocate(ssize_t nsize) {
    release();
    fd = shm_open_anon();
    if (fd == -1) {
        throw std::system_error(errno, std::generic_category(), "memfd_create");
    }

    int ret = ftruncate(fd, nsize);
    if (ret == -1) {
        throw std::system_error(errno, std::generic_category(), "memfd_create");
    }

    size = nsize;
}

