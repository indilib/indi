/* libcqcam - shared Color Quickcam routines
 * Copyright (C) 1996-1998 by Patrick Reynolds
 * Email: <no.email@noemail.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

// I/O ports wrapper code
// This file might need tweaking if you're trying to port my code to other
// x86 Unix platforms.  Code is already available for Linux, FreeBSD, and    
// QNX; see the Makefile.
//
// QNX code by: Anders Arpteg <aa11ac@hik.se>
// FreeBSD code by: Patrick Reynolds <reynolds@cs.duke.edu> and Charles
//                  Henrich <henrich@msu.edu>


//#include "config.h"

#include <stdio.h>
#include <errno.h>

#ifdef LOCKING
#include <fcntl.h>
#include <sys/stat.h>
#endif /* LOCKING */

#include "port.h"

port_t::port_t(int iport) {
  port = -1;

#ifdef LOCKING
  if (lock(iport) == -1) {
#ifdef DEBUG
    fprintf(stderr, "port 0x%x already locked\n", iport);
#endif /* DEBUG */
    return;
  }
#endif /* LOCKING */

#ifdef LINUX
#ifdef NO_SYSIO
  if ((devport = open("/dev/port", O_RDWR)) < 0) {
    perror("open /dev/port");
    return;
  }
#else
  if (ioperm(iport, 3, 1) == -1) {
    perror("ioperm()");
    return;
  }
#endif /* NO_SYSIO */
#elif defined(FREEBSD)
  if ((devio = fopen("/dev/io", "r+")) == NULL) {
    perror("fopen /dev/io");
    return;
  }
#elif defined(OPENBSD)
  if (i386_iopl(1) == -1) {
    perror("i386_iopl");
    return;
  }
#elif defined(LYNX)
  if (io_access() < 0) {
    perror("io_access");
    return;
  }
#elif defined(SOLARIS)
  if (openiop()) {
    perror("openiop");
    return;
  }
#endif /* which OS */

  port = iport;
  port1 = port + 1;
  port2 = port + 2;
  control_reg = read_control();
}

port_t::~port_t(void) {
#ifdef LOCKING
  unlock(port);
#endif /* LOCKING */
#ifdef LINUX
#ifdef NO_SYSIO
  if (devport >= 0)
    close(devport);
#else
  if (port > 0 && geteuid() == 0)
    if (ioperm(port, 3, 0) != 0)  // drop port permissions -- still must
                                  // be root
      perror("ioperm()");
#endif /* NO_SYSIO */
#elif defined(FREEBSD)
  if (devio != NULL)
    fclose(devio);
#elif defined(SOLARIS)
  close(iopfd);
#endif /* which OS */
}

#ifdef LOCKING
int port_t::lock(int portnum) {
  char lockfile[80];
  sprintf(lockfile, "/tmp/LOCK.qcam.0x%x", portnum);
  while ((lock_fd = open(lockfile, O_WRONLY | O_CREAT | O_EXCL, 0600)) == -1) {
    if (errno != EEXIST) {
      perror(lockfile);
      return -1;
    }
    struct stat stat_buf;
    if (lstat(lockfile, &stat_buf) < 0) continue;
    if (S_ISLNK(stat_buf.st_mode) || stat_buf.st_uid != 0) {
      if (unlink(lockfile)) {
        if (errno == ENOENT) continue;
        if (errno != EISDIR || (rmdir(lockfile) && errno != ENOENT)) {
            /* known problem: if lockfile exists and is a non-empty
               directory, we give up instead of doing an rm-r of it */
          perror(lockfile);
          return -1;
        }
      }
      continue;
    }
    lock_fd = open(lockfile, O_WRONLY, 0600);
    if (lock_fd == -1) {
      perror(lockfile);
      return -1;
    }
    break;
  }

  static struct flock lock_info;
  lock_info.l_type = F_WRLCK;
#ifdef LOCK_FAIL
  if (fcntl(lock_fd, F_SETLK, &lock_info) != 0) {
#else
  if (fcntl(lock_fd, F_SETLKW, &lock_info) != 0) {
#endif /* LOCK_FAIL */
    if (errno != EAGAIN)
      perror("fcntl");
    return -1;
  }
  chown(lockfile, getuid(), getgid());
#ifdef DEBUG
  fprintf(stderr, "Locked port 0x%x\n", portnum);
#endif /* DEBUG */
  return 0;
}
    
void port_t::unlock(int portnum) {
  if (portnum == -1)
    return;
  close(lock_fd);    // this clears the lock
  char lockfile[80];
  sprintf(lockfile, "/tmp/LOCK.qcam.0x%x", portnum);
  if (unlink(lockfile)) perror(lockfile);
#ifdef DEBUG
  fprintf(stderr, "Unlocked port 0x%x\n", portnum);
#endif /* DEBUG */
}
#endif /* LOCKING */
