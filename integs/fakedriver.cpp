/*******************************************************************************
  Copyright(c) 2022 Ludovic Pollet. All rights reserved.

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Library General Public
 License version 2 as published by the Free Software Foundation.

 This library is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 Library General Public License for more details.

 You should have received a copy of the GNU Library General Public License
 along with this library; see the file COPYING.LIB.  If not, write to
 the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 Boston, MA 02110-1301, USA.
*******************************************************************************/

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