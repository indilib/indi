#ifndef SHAREDBUFFER_H_
#define SHAREDBUFFER_H_ 1

class SharedBuffer {
    int fd;
    ssize_t size;

public:
    SharedBuffer();
    ~SharedBuffer();

    int getFd() const;

    void allocate(ssize_t size);
    void write(const void * buffer, size_t size);
    void release();

    void attach(int fd);

    ssize_t getSize() const { return size; }
};

#endif // SHAREDBUFFER_H_




