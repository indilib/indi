#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include "utils.h"

// This fake driver will just forward its 0 & 1 FD to a process over unix socket
int main() {
    fprintf(stderr, "fake driver starting\n");
    setupSigPipe();

    const char * path = getenv("FAKEDRIVER_ADDRESS");
    if (!path) {
        fprintf(stderr, "FAKEDRIVER_ADDRESS not set\n");
    }
    int cnx = unixSocketConnect(path);

    fprintf(stderr, "fake driver connected to %s on %d\n", path, cnx);

    int fds[2] = {0, 1};
    unixSocketSendFds(cnx, 2, fds);
    close(fds[0]);
    close(fds[1]);
    fprintf(stderr, "fake driver pipes sent\n");

    char buff[1];
    ssize_t ret = read(cnx, &buff, 1);
    if (ret == -1) {
        perror("read failed");
    }
    return 0;
}