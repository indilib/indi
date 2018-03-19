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

//==========================================================================
// Include Files for Mac OSX IO
//
//==========================================================================
#include <CoreFoundation/CoreFoundation.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <limits.h>

#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <signal.h>
#include <stdlib.h> 
#include <glob.h>

#include <IOKit/IOKitLib.h>
#include <IOKit/IOMessage.h>
#include <IOKit/IOCFPlugIn.h>
#include <IOKit/usb/IOUSBLib.h>

#include "libfli.h"
#include "libfli-libfli.h"
#include "libfli-usb.h"
#include "libfli-camera.h"
#include "libfli-filter-focuser.h"
//-------------------------------------------------------------------------

/* Structure to describe string descriptor transfers */
typedef struct {
    unsigned int index;
    char buf[64];
} fliusb_string_descriptor_t;

/* Structure to describe bulk transfers */
typedef struct {
    u_int8_t ep;
    void *buf;
    size_t count;
    unsigned int timeout; /* in msec */
} fliusb_bulktransfer_t;

/* 8-bit special value to identify ioctl 'type' */
#define FLIUSB_IOC_TYPE			0xf1

/* Recognized ioctl commands */
#define FLIUSB_GETRDEPADDR		_IOR(FLIUSB_IOC_TYPE, 1, u_int8_t)
#define FLIUSB_SETRDEPADDR		_IOW(FLIUSB_IOC_TYPE, 2, u_int8_t)

#define FLIUSB_GETWREPADDR		_IOR(FLIUSB_IOC_TYPE, 3, u_int8_t)
#define FLIUSB_SETWREPADDR		_IOW(FLIUSB_IOC_TYPE, 4, u_int8_t)

#define FLIUSB_GETBUFFERSIZE		_IOR(FLIUSB_IOC_TYPE, 5, size_t)
#define FLIUSB_SETBUFFERSIZE		_IOW(FLIUSB_IOC_TYPE, 6, size_t)

#define FLIUSB_GETTIMEOUT		_IOR(FLIUSB_IOC_TYPE, 7, unsigned int)
#define FLIUSB_SETTIMEOUT		_IOW(FLIUSB_IOC_TYPE, 8, unsigned int)

#define FLIUSB_BULKREAD			_IOW(FLIUSB_IOC_TYPE, 9, fliusb_bulktransfer_t)
#define FLIUSB_BULKWRITE		_IOW(FLIUSB_IOC_TYPE, 10, fliusb_bulktransfer_t)

#define FLIUSB_GET_DEVICE_DESCRIPTOR	_IOR(FLIUSB_IOC_TYPE, 11, struct usb_device_descriptor)
#define FLIUSB_GET_STRING_DESCRIPTOR	_IOR(FLIUSB_IOC_TYPE, 12, struct usb_device_descriptor)

#define FLIUSB_IOC_MAX			12

#define FLIUSB_PROD(name,id) case id:

#define FLIUSB_PRODUCTS                  \
	FLIUSB_PROD(FLIUSB_MAXCAM,     0x0002) \
	FLIUSB_PROD(FLIUSB_STEPPER,    0x0005) \
	FLIUSB_PROD(FLIUSB_FOCUSER,    0x0006) \
	FLIUSB_PROD(FLIUSB_FILTERWHEEL,0x0007) \
	FLIUSB_PROD(FLIUSB_PROLINECAM, 0x000A)

#undef FLIUSB_PROD

enum
{
#define FLIUSB_PROD(name,id) name=id,
	FLIUSB_PRODUCTS
#undef FLIUSB_PROD
};

#define FLIUSB_PROD(name,id) case id:

#define USB_DIR_OUT (0x00)
#define USB_DIR_IN (0x80)

#define BUFFERSIZE 65536

#define FLI_VENDOR_ID 0x0f18

#define MAX_SEARCH 32

//-------------------------------------------------------------------------

typedef struct _mac_device_info
{
    IOUSBInterfaceInterface190 **interface; // Requires >= 10.2
    IOUSBDeviceInterface182 **device;
    UInt8 interfaceNumEndpoints;
    unsigned int epWrite, epRead, epReadBulk;
    
    
} mac_device_info;

#define DEVICE_DATA ((mac_device_info*)DEVICE->sys_data)

long mac_fli_list(flidomain_t domain, char ***names);

long mac_fli_connect(flidev_t dev, char *name, long domain);
long mac_fli_disconnect(flidev_t dev, fli_unixio_t *io);

long mac_usb_connect(flidev_t dev, fli_unixio_t *io, char *name);
IOReturn mac_usb_configure_device(IOUSBDeviceInterface182 **dev);
IOReturn mac_usb_find_interfaces(flidev_t dev, IOUSBDeviceInterface182 **device);

long mac_usb_disconnect(flidev_t dev, fli_unixio_t *io);

long mac_bulktransfer(flidev_t dev, int ep, void *buf, long *len);

long mac_bulkread(flidev_t dev, void *buf, long *rlen);
long mac_bulkwrite(flidev_t dev, void *buf, long *wlen);

ssize_t mac_usb_piperead(flidev_t dev, void *buf,size_t size, unsigned pipe, unsigned timeout);
ssize_t mac_usb_pipewrite(flidev_t dev, void *buf, size_t size, unsigned pipe, unsigned timeout);

long mac_fli_lock(flidev_t dev);
long mac_fli_unlock(flidev_t dev);

