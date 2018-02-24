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

#ifndef _LIBFLI_SYSDEP_H
#define _LIBFLI_SYSDEP_H
#include <windows.h>
#include <limits.h>

#ifndef LIBFLIAPI
#define LIBFLIAPI __declspec(dllimport) long __stdcall
#endif /* LIBFLIAPI */

#define __SYSNAME__ "Windows"
#define __LIBFLI_MINOR__ 104

#define snprintf _snprintf

#define USB_READ_SIZ_MAX (65535)
#define USB_MAX_PIPES (10)

#ifndef EOVERFLOW
#define EOVERFLOW ERROR_INSUFFICIENT_BUFFER
#endif

typedef struct {
  HANDLE fd;
	int port;
	int dir;
	int notecp;
	char portval[6];
	/* This is used to map and endpoint to a pipe */
	int endpointlist[USB_MAX_PIPES];
} fli_io_t;

/* System specific information */
typedef struct {
  HANDLE mutex;
	long locked;
	long OS;
} fli_sysinfo_t;

long fli_connect(flidev_t dev, char *name, long domain);
long fli_disconnect(flidev_t dev);
long fli_lock(flidev_t dev);
long fli_unlock(flidev_t dev);
long fli_list(flidomain_t domain, char ***names);

#endif /* _LIBFLI_SYSDEP_H */
