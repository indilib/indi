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

#include <errno.h>

#include "..\libfli-libfli.h"
#include "..\libfli-debug.h"
#include "libfli-sys.h"
#include "libfli-serial.h"

long serportio(flidev_t dev, void *buf, long *wlen, long *rlen)
{
  fli_io_t *io;
	fli_sysinfo_t *sys;
  int err = 0, locked = 0;
  long org_wlen = *wlen, org_rlen = *rlen;
	COMMTIMEOUTS CommTimeout;

  io = DEVICE->io_data;
//  cam = DEVICE->device_data;
	sys = DEVICE->sys_data;

  locked = 1;

	if (fli_lock(dev))
  {
    debug(FLIDEBUG_WARN, "Could not lock device");
		locked = 0;
  }

	CommTimeout.ReadIntervalTimeout = 100;
	CommTimeout.ReadTotalTimeoutConstant = DEVICE->io_timeout;
	CommTimeout.ReadTotalTimeoutMultiplier = 1; // 1 unit per character
	CommTimeout.WriteTotalTimeoutConstant = 0;
	CommTimeout.WriteTotalTimeoutMultiplier = 0;
	if(SetCommTimeouts(io->fd, &CommTimeout) == FALSE)
	{
		debug(FLIDEBUG_WARN, "Error setting communications timeouts, %d", GetLastError());
	}

	if (*wlen > 0)
	{
		debug(FLIDEBUG_INFO, "SER IOW: %02x [%02x %02x]", *wlen,
			((unsigned char *) buf)[0], ((unsigned char *) buf)[1]);

		WriteFile(io->fd, buf, *wlen, wlen, NULL);
		if (*wlen != org_wlen)
		{
			debug(FLIDEBUG_WARN, "write failed, only %d of %d bytes written",
			*wlen, org_wlen);
			err = -EIO;
			goto done;
		}
	}

  if (*rlen > 0)
  {
		if(ReadFile(io->fd, buf, *rlen, rlen, NULL) != TRUE)
		{
			debug(FLIDEBUG_WARN, "read failed.");
			err = -EIO;
			goto done;
		}
		debug(FLIDEBUG_INFO, "SER IOR: %02x [%02x %02x]", *wlen,
			((unsigned char *) buf)[0], ((unsigned char *) buf)[1]);

		if (*rlen != org_rlen)
		{
      debug(FLIDEBUG_WARN, "read failed, only %d of %d bytes read",
	    *rlen, org_rlen);
      err = -EIO;
      goto done;
    }
  }

 done:

  if (locked)
  {
    int r;

    if ((r = fli_unlock(dev)))
      debug(FLIDEBUG_WARN, "Unlock failed");
    if (err == 0)
      err = r;
  }

  return err;
}
