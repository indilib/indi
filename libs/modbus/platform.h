#include <errno.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>

#include "nanomodbus.h"


#define UNUSED_PARAM(x) ((x) = (x))

// Connection management
int client_connection = -1;
int client_read_fd = -1;
fd_set client_connections;

void disconnect(void* conn)
{
    int fd = *(int*) conn;
    FD_CLR(fd, &client_connections);
    close(fd);
}

// Read/write/sleep platform functions

int32_t read_fd_linux(uint8_t* buf, uint16_t count, int32_t timeout_ms, void* arg)
{
    int fd = *(int*) arg;

    uint16_t total = 0;
    while (total != count)
    {
        fd_set rfds;
        FD_ZERO(&rfds);
        FD_SET(fd, &rfds);

        struct timeval* tv_p = NULL;
        struct timeval tv;
        if (timeout_ms >= 0)
        {
            tv_p = &tv;
            tv.tv_sec = timeout_ms / 1000;
            tv.tv_usec = (int64_t) (timeout_ms % 1000) * 1000;
        }

        int ret = select(fd + 1, &rfds, NULL, NULL, tv_p);
        if (ret == 0)
        {
            return total;
        }

        if (ret == 1)
        {
            ssize_t r = read(fd, buf + total, 1);
            if (r == 0)
            {
                disconnect(arg);
                return -1;
            }

            if (r < 0)
                return -1;

            total += r;
        }
        else
            return -1;
    }

    return total;
}


int32_t write_fd_linux(const uint8_t* buf, uint16_t count, int32_t timeout_ms, void* arg)
{
    int fd = *(int*) arg;

    uint16_t total = 0;
    while (total != count)
    {
        fd_set wfds;
        FD_ZERO(&wfds);
        FD_SET(fd, &wfds);

        struct timeval* tv_p = NULL;
        struct timeval tv;
        if (timeout_ms >= 0)
        {
            tv_p = &tv;
            tv.tv_sec = timeout_ms / 1000;
            tv.tv_usec = (int64_t) (timeout_ms % 1000) * 1000;
        }

        int ret = select(fd + 1, NULL, &wfds, NULL, tv_p);
        if (ret == 0)
        {
            return 0;
        }

        if (ret == 1)
        {
            ssize_t w = write(fd, buf + total, count);
            if (w == 0)
            {
                disconnect(arg);
                return -1;
            }

            if (w <= 0)
                return -1;

            total += (int32_t) w;
        }
        else
            return -1;
    }

    return total;
}
