/*

  Copyright (c) 2002 Finger Lakes Instrumentation (FLI), L.L.C.
  All rights reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions
  are met:

        Redistributions of source code must retain the above copyright
        notice, this list of conditions and the following disclaimer.

        Redistributions in binary form must reproduce the above
        copyright notice, this list of conditions and the following
        disclaimer in the documentation and/or other materials
        provided with the distribution.

        Neither the name of Finger Lakes Instrumentation (FLI), LLC
        nor the names of its contributors may be used to endorse or
        promote products derived from this software without specific
        prior written permission.

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
  FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
  REGENTS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
  BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
  LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
  ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
  POSSIBILITY OF SUCH DAMAGE.

  ======================================================================

  Finger Lakes Instrumentation, L.L.C. (FLI)
  web: http://www.fli-cam.com
  email: support@fli-cam.com

*/

#include <sys/types.h>
#include <dev/usb/usb.h>
#include <sys/ioctl.h>

#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>

#include "libfli-libfli.h"
#include "libfli-sys.h"
#include "libfli-usb.h"

long bsd_usb_connect(flidev_t dev, fli_unixio_t *io, char *name)
{
  usb_device_descriptor_t usbdesc;
  int tmp, ep;
  char tmpname[PATH_MAX];

  if (ioctl(io->fd, USB_GET_DEVICE_DESC, &usbdesc) == -1)
  {
    debug(FLIDEBUG_FAIL, "%s: Could not read descriptor: %s",
	  __PRETTY_FUNCTION__, strerror(errno));
    return -EIO;
  }

  if (UGETW(usbdesc.idVendor) != FLIUSB_VENDORID)
  {
    debug(FLIDEBUG_INFO, "%s: Not a FLI device!", __PRETTY_FUNCTION__);
    return -ENODEV;
  }

  switch (UGETW(usbdesc.idProduct))
  {
    /* These are valid product IDs */
  case FLIUSB_CAM_ID:
  case FLIUSB_FOCUSER_ID:
  case FLIUSB_FILTER_ID:
  case FLIUSB_PROLINE_ID:
    break;

  default:
    /* Anything else is unknown */
    return -ENODEV;
  }

  DEVICE->devinfo.devid = UGETW(usbdesc.idProduct);
  DEVICE->devinfo.fwrev = UGETW(usbdesc.bcdDevice);

  switch (DEVICE->devinfo.devid)
  {
  case FLIUSB_CAM_ID:
  case FLIUSB_FOCUSER_ID:
  case FLIUSB_FILTER_ID:
    ep = 0x02;
    break;

  case FLIUSB_PROLINE_ID:
    ep = 0x01;
    break;

  default:
    debug(FLIDEBUG_FAIL, "Unknown device type."); /* This shouldn't happen */
    return -ENODEV;
  }

  if (snprintf(tmpname, sizeof(tmpname), "%s.%d", name, ep) >= sizeof(tmpname))
    return -EOVERFLOW;

  if ((tmp = open(tmpname, O_RDWR)) == -1)
  {
    debug(FLIDEBUG_FAIL, "%s: open(%s) failed: %s",
	  __PRETTY_FUNCTION__, tmpname, strerror(errno));
    return -errno;
  }
  close(io->fd);
  io->fd = tmp;

  return 0;
}

long bsd_usb_disconnect(flidev_t dev)
{
  return 0;
}

long bsd_bulkwrite(flidev_t dev, void *buf, long *wlen)
{
  fli_unixio_t *io;
  long org_wlen = *wlen;
  int to;

  io = DEVICE->io_data;
  to = DEVICE->io_timeout;

  if (ioctl(io->fd, USB_SET_TIMEOUT, &to) == -1)
    return -errno;

  *wlen = write(io->fd, buf, *wlen);

  if (*wlen != org_wlen)
    return -errno;
  else
    return 0;
}

long bsd_bulkread(flidev_t dev, void *buf, long *rlen)
{
  fli_unixio_t *io;
  long org_rlen = *rlen;
  int to;

  io = DEVICE->io_data;
  to = DEVICE->io_timeout;

  if (ioctl(io->fd, USB_SET_TIMEOUT, &to) == -1)
    return -errno;

  *rlen = read(io->fd, buf, *rlen);

  if (*rlen != org_rlen)
    return -errno;
  else
    return 0;
}

long bsd_bulktransfer(flidev_t dev, int ep, void *buf, long *len)
{
  char name[PATH_MAX];
  long orglen = *len;
  int fd, to, err;

  if (snprintf(name, sizeof(name), "%s.%d", DEVICE->name, ep) >= sizeof(name))
    return -EOVERFLOW;

  if ((fd = open(name, O_RDWR)) == -1)
  {
    debug(FLIDEBUG_FAIL, "%s: open(%s) failed: %s",
	  __PRETTY_FUNCTION__, name, strerror(errno));
    return -errno;
  }

  to = DEVICE->io_timeout;
  if (ioctl(fd, USB_SET_TIMEOUT, &to) == -1)
  {
    err = -errno;
    goto done;
  }

  if (ep & UE_DIR_IN)
    *len = read(fd, buf, *len);
  else
    *len = write(fd, buf, *len);

  if (*len != orglen)
    err = -errno;
  else
    err = 0;

 done:

  close(fd);
  return err;
}
