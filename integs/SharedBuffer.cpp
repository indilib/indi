#include <system_error>

#include <SharedBuffer.h>


#ifdef __linux__
#define _GNU_SOURCE
#include <linux/unistd.h>
#endif

#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <fcntl.h>


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

    off_t o;

    o = lseek(fd, 0, SEEK_END);
    if (o == (off_t) -1) {
        int e = errno;
        throw std::system_error(e, std::generic_category(), "Cannot seek in buffer");
    }
    size = o;
    o = lseek(fd, 0, SEEK_SET);
    if (o == (off_t) -1) {
        int e = errno;
        throw std::system_error(e, std::generic_category(), "Cannot seek in buffer");
    }
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


void SharedBuffer::write(const void * data, size_t size) {
    auto ret = ::write(getFd(), data, size);
    if (ret == -1) {
        int e = errno;
        throw std::system_error(e, std::generic_category(), "Write to shared buffer");
    }

    if (ret != size) {
        throw std::runtime_error("Short write to shared buffer");
    }
}


void SharedBuffer::allocate(ssize_t nsize) {
    release();
    fd = memfd_create("indiblob", MFD_ALLOW_SEALING|MFD_CLOEXEC);
    if (fd == -1) {
        throw std::system_error(errno, std::generic_category(), "memfd_create");
    }

    int ret = ftruncate(fd, nsize);
    if (ret == -1) {
        throw std::system_error(errno, std::generic_category(), "memfd_create");
    }

    size = nsize;
}

