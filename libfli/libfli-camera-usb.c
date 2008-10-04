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

#ifdef WIN32
#include <winsock.h>
#else
#include <sys/param.h>
#include <netinet/in.h>
#include <unistd.h>
#endif

#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <math.h>

#include "libfli-libfli.h"
#include "libfli-debug.h"
#include "libfli-mem.h"
#include "libfli-camera.h"
#include "libfli-camera-usb.h"
#include "libfli-usb.h"

/* These routines are defined because everyone wants to change the size of long...
 * and we will now make everything BYTE aligned and the proper byte order
 */

#define MSW(x) (unsigned short) ((x >> 16) & 0xffff)
#define LSW(x) (unsigned short) (x & 0xffff)
#define MSB(x) (unsigned char) ((x >> 8) & 0xff)
#define LSB(x) (unsigned char) (x & 0xff)

#define IOBUF_MAX_SIZ (64)
typedef unsigned char iobuf_t;

#define IOREAD_U8(b, i, y)  { y = *(b + i); }
#define IOREAD_U16(b, i, y) { y = (*(b + i) << 8) | *(b + i + 1); }
#define IOREAD_U32(b, i, y) { y = (*(b + i) << 24) | *(b + i + 1) << 16 | \
																 *(b + i + 2) << 8 | *(b + i + 3); }
#define IOWRITE_U8(b, i, y)  { *(b + i) = (unsigned char) y; }
#define IOWRITE_U16(b, i, y) { *(b + i) = MSB(y); *(b + i + 1) = LSB(y); }
#define IOWRITE_U32(b, i, y) { *(b + i) = MSB(MSW(y)); *(b + i + 1) = LSB(MSW(y)); \
																 *(b + i + 2) = MSB(LSW(y)); *(b + i + 3) = LSB(LSW(y)); }
#define IOREAD_LF(b, i, y) { y = dconvert(b + i); }

double dconvert(void *buf)
{
  unsigned char *fnum = (unsigned char *) buf;
  double sign, exponent, mantissa, result;

  sign = (double) ((fnum[3] & 0x80)?(-1):(1));
  exponent = (double) ((fnum[3] & 0x7f) << 1 | ((fnum[2] & 0x80)?1:0));

  mantissa = 1.0 +
    ((double) ((fnum[2] & 0x7f) << 16 | fnum[1] << 8 | fnum[0]) /
     pow(2, 23));

  result = sign * (double) pow(2, (exponent - 127.0)) * mantissa;

  return result;
}

long fli_camera_usb_open(flidev_t dev)
{
	flicamdata_t *cam;
	iobuf_t buf[IOBUF_MAX_SIZ];
	long rlen, wlen;
//	long r = 0;

	cam = DEVICE->device_data;

#ifdef __linux__
	/* Linux needs this page aligned, hopefully this is 512 byte aligned too... */
	cam->max_usb_xfer = (USB_READ_SIZ_MAX / getpagesize()) * getpagesize();
	cam->gbuf_siz = 2 * cam->max_usb_xfer;

	if ((cam->gbuf = xmemalign(getpagesize(), cam->gbuf_siz)) == NULL)
		return -ENOMEM;
#else
	/* Just 512 byte align it... */
	cam->max_usb_xfer = (USB_READ_SIZ_MAX & 0xfffffe00);
	cam->gbuf_siz = 2 * cam->max_usb_xfer;

	if ((cam->gbuf = xmalloc(cam->gbuf_siz)) == NULL)
		return -ENOMEM;
#endif

	switch (DEVICE->devinfo.devid)
  {
		/* MaxCam and IMG cameras */
		case FLIUSB_CAM_ID:
		{
			short camtype;

			IOWRITE_U16(buf, 0, FLI_USBCAM_HARDWAREREV);
			rlen = 2; wlen = 2;
			IO(dev, buf, &wlen, &rlen);
			IOREAD_U16(buf, 0, DEVICE->devinfo.hwrev);

			IOWRITE_U16(buf, 0, FLI_USBCAM_DEVICEID);
			rlen = 2; wlen = 2;
			IO(dev, buf, &wlen, &rlen);
			IOREAD_U16(buf, 0, camtype);

			IOWRITE_U16(buf, 0, FLI_USBCAM_SERIALNUM);
			rlen = 2; wlen = 2;
			IO(dev, buf, &wlen, &rlen);
			IOREAD_U16(buf, 0, DEVICE->devinfo.serno);

			/* The following devices need information downloaded to them */
			if (DEVICE->devinfo.fwrev < 0x0201)
			{
				int id;

				for (id = 0; knowndev[id].index != 0; id++)
					if (knowndev[id].index == camtype)
						break;

				if (knowndev[id].index == 0)
					return -ENODEV;

				cam->ccd.pixelwidth = knowndev[id].pixelwidth;
				cam->ccd.pixelheight = knowndev[id].pixelheight;

				wlen = 14; rlen = 0;
				IOWRITE_U16(buf, 0, FLI_USBCAM_DEVINIT);
				IOWRITE_U16(buf, 2, (unsigned short) knowndev[id].array_area.lr.x);
				IOWRITE_U16(buf, 4, (unsigned short) knowndev[id].array_area.lr.y);
				IOWRITE_U16(buf, 6, (unsigned short) (knowndev[id].visible_area.lr.x -
								knowndev[id].visible_area.ul.x));
				IOWRITE_U16(buf, 8, (unsigned short) (knowndev[id].visible_area.lr.y -
								knowndev[id].visible_area.ul.y));
				IOWRITE_U16(buf, 10, (unsigned short) knowndev[id].visible_area.ul.x);
				IOWRITE_U16(buf, 12, (unsigned short) knowndev[id].visible_area.ul.y);
				IO(dev, buf, &wlen, &rlen);

				DEVICE->devinfo.model = xstrndup(knowndev[id].model, 32);

				switch(DEVICE->devinfo.fwrev & 0xff00)
				{
					case 0x0100:
						cam->tempslope = (70.0 / 215.75);
						cam->tempintercept = (-52.5681);
						break;

					case 0x0200:
						cam->tempslope = (100.0 / 201.1);
						cam->tempintercept = (-61.613);
						break;

					default:
						cam->tempslope = 1e-12;
						cam->tempintercept = 0;
				}
			}
			/* Here, all the parameters are stored on the camera */
			else if (DEVICE->devinfo.fwrev >= 0x0201)
			{
				rlen = 64; wlen = 2;
				IOWRITE_U16(buf, 0, FLI_USBCAM_READPARAMBLOCK);
				IO(dev, buf, &wlen, &rlen);

				IOREAD_LF(buf, 31, cam->ccd.pixelwidth);
				IOREAD_LF(buf, 35, cam->ccd.pixelheight);
				IOREAD_LF(buf, 23, cam->tempslope);
				IOREAD_LF(buf, 27, cam->tempintercept);
			}

			rlen = 32; wlen = 2;
			IOWRITE_U16(buf, 0, FLI_USBCAM_DEVICENAME);
			IO(dev, buf, &wlen, &rlen);

			DEVICE->devinfo.devnam = xstrndup((char *) buf, 32);

			if(DEVICE->devinfo.model == NULL)
			{
				DEVICE->devinfo.model = xstrndup(DEVICE->devinfo.devnam, 32);
			}

			rlen = 4; wlen = 2;
			IOWRITE_U16(buf, 0, FLI_USBCAM_ARRAYSIZE);
			IO(dev, buf, &wlen, &rlen);
			cam->ccd.array_area.ul.x = 0;
			cam->ccd.array_area.ul.y = 0;
			IOREAD_U16(buf, 0, cam->ccd.array_area.lr.x);
			IOREAD_U16(buf, 2, cam->ccd.array_area.lr.y);

			rlen = 4; wlen = 2;
			IOWRITE_U16(buf, 0, FLI_USBCAM_IMAGEOFFSET);
			IO(dev, buf, &wlen, &rlen);
			IOREAD_U16(buf, 0, cam->ccd.visible_area.ul.x);
			IOREAD_U16(buf, 2, cam->ccd.visible_area.ul.y);

			rlen = 4; wlen = 2;
			IOWRITE_U16(buf, 0, FLI_USBCAM_IMAGESIZE);
			IO(dev, buf, &wlen, &rlen);
			IOREAD_U16(buf, 0, cam->ccd.visible_area.lr.x);
			cam->ccd.visible_area.lr.x += cam->ccd.visible_area.ul.x;
			IOREAD_U16(buf, 2, cam->ccd.visible_area.lr.y);
			cam->ccd.visible_area.lr.y += cam->ccd.visible_area.ul.y;

#ifdef WIN32

			/* Check the registry to determine if we are overriding any settings */
			{
				HKEY hKey;
				DWORD overscan_x = 0, overscan_y = 0;
				DWORD whole_array = 0;
				DWORD len;

				if (RegOpenKey(HKEY_LOCAL_MACHINE,
					"SOFTWARE\\Finger Lakes Instrumentation\\libfli",
					&hKey) == ERROR_SUCCESS)
				{
					/* Check for overscan data */
					
					len = sizeof(DWORD);
					if (RegQueryValueEx(hKey, "overscan_x", NULL, NULL, (LPBYTE) &overscan_x, &len) == ERROR_SUCCESS)
					{
						debug(FLIDEBUG_INFO, "Found a request for horizontal overscan of %d pixels.", overscan_x);
					}

					len = sizeof(DWORD);
					if (RegQueryValueEx(hKey, "overscan_y", NULL, NULL, (LPBYTE) &overscan_y, &len) == ERROR_SUCCESS)
					{
						debug(FLIDEBUG_INFO, "Found a request for vertical overscan of %d pixels.", overscan_y);
					}

					len = sizeof(DWORD);
					RegQueryValueEx(hKey, "whole_array", NULL, NULL, (LPBYTE) &whole_array, &len);

					cam->ccd.array_area.ul.x = 0;
					cam->ccd.array_area.ul.y = 0;
					cam->ccd.array_area.lr.x += overscan_x;
					cam->ccd.array_area.lr.y += overscan_y;

					if (whole_array == 0)
					{
						cam->ccd.visible_area.lr.x += overscan_x;
						cam->ccd.visible_area.lr.y += overscan_y;
					}
					else
					{
						cam->ccd.visible_area.ul.x = 0;
						cam->ccd.visible_area.ul.y = 0;
						cam->ccd.visible_area.lr.x = cam->ccd.array_area.lr.x;
						cam->ccd.visible_area.lr.y = cam->ccd.array_area.lr.y;
					}
					RegCloseKey(hKey);
				}
				else
				{
					debug(FLIDEBUG_INFO, "Could not find registry key.");
				}
			}


#endif

			/* Initialize all variables to something */

			cam->vflushbin = 4;
			cam->hflushbin = 4;
			cam->vbin = 1;
			cam->hbin = 1;
			cam->image_area.ul.x = cam->ccd.visible_area.ul.x;
			cam->image_area.ul.y = cam->ccd.visible_area.ul.y;
			cam->image_area.lr.x = cam->ccd.visible_area.lr.x;
			cam->image_area.lr.y = cam->ccd.visible_area.lr.y;
			cam->exposure = 100;
			cam->frametype = FLI_FRAME_TYPE_NORMAL;
			cam->flushes = 0;
			cam->bitdepth = FLI_MODE_16BIT;
			cam->exttrigger = 0;
			cam->exttriggerpol = 0;
			cam->background_flush = 1;

			cam->grabrowwidth =
				(cam->image_area.lr.x - cam->image_area.ul.x) / cam->hbin;
			cam->grabrowcount = 1;
			cam->grabrowcounttot = cam->grabrowcount;
			cam->grabrowindex = 0;
			cam->grabrowbatchsize = 1;
			cam->grabrowbufferindex = cam->grabrowcount;
			cam->flushcountbeforefirstrow = 0;
			cam->flushcountafterlastrow = 0;

#ifdef _SETUPDEFAULTS
			/* Now to set up the camera defaults */
			rlen = 0; wlen = 6;
			IOWRITE_U16(buf, 0, FLI_USBCAM_SETFRAMEOFFSET);
			IOWRITE_U16(buf, 2, cam->image_area.ul.x);
			IOWRITE_U16(buf, 4, cam->image_area.ul.y);
			IO(dev, buf, &wlen, &rlen);

			rlen = 0; wlen = 6;
			IOWRITE_U16(buf, 0, FLI_USBCAM_SETBINFACTORS);
			IOWRITE_U16(buf, 2, cam->hbin);
			IOWRITE_U16(buf, 4, cam->vbin);
			IO(dev, buf, &wlen, &rlen);

			rlen = 0; wlen = 6;
			IOWRITE_U16(buf, 0, FLI_USBCAM_SETFLUSHBINFACTORS);
			IOWRITE_U16(buf, 2, cam->hflushbin);
			IOWRITE_U16(buf, 4, cam->vflushbin);
			IO(dev, buf, &wlen, &rlen);
#endif

		}
		break;

		/* Proline Camera */
		case FLIUSB_PROLINE_ID:
		{
			/* Let's get information about the hardware */
			wlen = 2; rlen = 6;
			IOWRITE_U16(buf, 0, PROLINE_GET_HARDWAREINFO);
			IO(dev, buf, &wlen, &rlen);
			IOREAD_U16(buf, 0, DEVICE->devinfo.hwrev);
			IOREAD_U16(buf, 2, DEVICE->devinfo.serno);
			IOREAD_U16(buf, 4, rlen);

			/* Configuration data from ProLine is little endian, I can't believe
			 * that I did this oh well, I'll deal with it!
			 */

			if (DEVICE->devinfo.hwrev >= 0x0100)
			{
				wlen = 2;
				IOWRITE_U16(buf, 0, PROLINE_GET_CAMERAINFO);
				IO(dev, buf, &wlen, &rlen);

				cam->ccd.array_area.ul.x = 0;
				cam->ccd.array_area.ul.y = 0;
				cam->ccd.array_area.lr.x = (buf[1] << 8) + buf[0];
				cam->ccd.array_area.lr.y = (buf[3] << 8) + buf[2];

				cam->ccd.visible_area.ul.x = (buf[9] << 8) + buf[8];
				cam->ccd.visible_area.ul.y = (buf[11] << 8) + buf[10];
				cam->ccd.visible_area.lr.x = (buf[5] << 8) + buf[4] + cam->ccd.visible_area.ul.x;
				cam->ccd.visible_area.lr.y = (buf[7] << 8) + buf[6] + cam->ccd.visible_area.ul.y;

				cam->ccd.pixelwidth = dconvert(&buf[12]);
				cam->ccd.pixelheight = dconvert(&buf[16]);

				rlen = 64; wlen = 2;
				IOWRITE_U16(buf, 0, PROLINE_GET_DEVICESTRINGS);
				IO(dev, buf, &wlen, &rlen);
				DEVICE->devinfo.devnam = xstrndup((char *) &buf[0], 32);
				DEVICE->devinfo.model = xstrndup((char *) &buf[32], 32);
			}

			/* Initialize all varaibles to something */
			cam->vflushbin = 0;
			cam->hflushbin = 0;
			cam->vbin = 1;
			cam->hbin = 1;
			cam->image_area.ul.x = cam->ccd.visible_area.ul.x;
			cam->image_area.ul.y = cam->ccd.visible_area.ul.y;
			cam->image_area.lr.x = cam->ccd.visible_area.lr.x;
			cam->image_area.lr.y = cam->ccd.visible_area.lr.y;
			cam->exposure = 100;
			cam->frametype = FLI_FRAME_TYPE_NORMAL;
			cam->flushes = 0;
			cam->bitdepth = FLI_MODE_16BIT;
			cam->exttrigger = 0;
			cam->exttriggerpol = 0;
			cam->background_flush = 1;
			cam->tempslope = 1.0;
			cam->tempintercept = 0.0;

			cam->grabrowwidth =
				(cam->image_area.lr.x - cam->image_area.ul.x) / cam->hbin;
			cam->grabrowcount = 1;
			cam->grabrowcounttot = cam->grabrowcount;
			cam->grabrowindex = 0;
			cam->grabrowbatchsize = 1;
			cam->grabrowbufferindex = cam->grabrowcount;
			cam->flushcountbeforefirstrow = 0;
			cam->flushcountafterlastrow = 0;

			/* Now to set up the camera defaults */
		}
		break;

		default:
			return -ENODEV;
	}

	debug(FLIDEBUG_INFO, "DeviceID %d", DEVICE->devinfo.devid);
	debug(FLIDEBUG_INFO, "SerialNum %d", DEVICE->devinfo.serno);
	debug(FLIDEBUG_INFO, "HWRev %d", DEVICE->devinfo.hwrev);
	debug(FLIDEBUG_INFO, "FWRev %d", DEVICE->devinfo.fwrev);

	debug(FLIDEBUG_INFO, "     Name: %s", DEVICE->devinfo.devnam);
	debug(FLIDEBUG_INFO, "    Array: (%4d,%4d),(%4d,%4d)",
	cam->ccd.array_area.ul.x,
	cam->ccd.array_area.ul.y,
	cam->ccd.array_area.lr.x,
	cam->ccd.array_area.lr.y);
	debug(FLIDEBUG_INFO, "  Visible: (%4d,%4d),(%4d,%4d)",
	cam->ccd.visible_area.ul.x,
	cam->ccd.visible_area.ul.y,
	cam->ccd.visible_area.lr.x,
	cam->ccd.visible_area.lr.y);

	debug(FLIDEBUG_INFO, " Pix Size: (%g, %g)", cam->ccd.pixelwidth, cam->ccd.pixelheight);
	debug(FLIDEBUG_INFO, "    Temp.: T = AD x %g + %g", cam->tempslope, cam->tempintercept);

	return 0;
}

long fli_camera_usb_get_array_area(flidev_t dev, long *ul_x, long *ul_y,
				   long *lr_x, long *lr_y)
{
  flicamdata_t *cam = DEVICE->device_data;

	switch (DEVICE->devinfo.devid)
  {
		/* MaxCam and IMG cameras */
		case FLIUSB_CAM_ID:
		{


		}
		break;

		/* Proline Camera */
		case FLIUSB_PROLINE_ID:
		{


		}
		break;

		default:
			debug(FLIDEBUG_WARN, "Hmmm, shouldn't be here, operation on NO camera...");
			break;
	}

	*ul_x = cam->ccd.array_area.ul.x;
	*ul_y = cam->ccd.array_area.ul.y;
	*lr_x = cam->ccd.array_area.lr.x;
	*lr_y = cam->ccd.array_area.lr.y;
	return 0;
}

long fli_camera_usb_get_visible_area(flidev_t dev, long *ul_x, long *ul_y,
				     long *lr_x, long *lr_y)
{
  flicamdata_t *cam = DEVICE->device_data;

	switch (DEVICE->devinfo.devid)
  {
		/* MaxCam and IMG cameras */
		case FLIUSB_CAM_ID:
		{


		}
		break;

		/* Proline Camera */
		case FLIUSB_PROLINE_ID:
		{


		}
		break;

		default:
			debug(FLIDEBUG_WARN, "Hmmm, shouldn't be here, operation on NO camera...");
			break;
	}

  *ul_x = cam->ccd.visible_area.ul.x;
  *ul_y = cam->ccd.visible_area.ul.y;
  *lr_x = cam->ccd.visible_area.lr.x;
  *lr_y = cam->ccd.visible_area.lr.y;

  return 0;
}

long fli_camera_usb_set_exposure_time(flidev_t dev, long exptime)
{
  flicamdata_t *cam = DEVICE->device_data;

  if (exptime < 0)
    return -EINVAL;

	switch (DEVICE->devinfo.devid)
  {
		/* MaxCam and IMG cameras */
		case FLIUSB_CAM_ID:
		{
			long rlen, wlen;
			iobuf_t buf[8];
			rlen = 0; wlen = 8;
			IOWRITE_U16(buf, 0, FLI_USBCAM_SETEXPOSURE);
			IOWRITE_U32(buf, 4, exptime);
			IO(dev, buf, &wlen, &rlen);
		}
		break;

		/* Proline Camera */
		case FLIUSB_PROLINE_ID:
		{

		}
		break;

		default:
			debug(FLIDEBUG_WARN, "Hmmm, shouldn't be here, operation on NO camera...");
			break;
	}

  cam->exposure = exptime;

	return 0;
}

long fli_camera_usb_set_image_area(flidev_t dev, long ul_x, long ul_y,
				   long lr_x, long lr_y)
{
  flicamdata_t *cam = DEVICE->device_data;
	int r = 0;

	if( (DEVICE->devinfo.fwrev < 0x0300) &&
			((DEVICE->devinfo.hwrev & 0xff00) == 0x0100) &&
			(DEVICE->devinfo.devid != FLIUSB_PROLINE_ID))
	{
		if( (lr_x > (cam->ccd.visible_area.lr.x * cam->hbin)) ||
				(lr_y > (cam->ccd.visible_area.lr.y * cam->vbin)) )
		{
			debug(FLIDEBUG_WARN,
				"FLISetImageArea(), area out of bounds: (%4d,%4d),(%4d,%4d)",
				ul_x, ul_y, lr_x, lr_y);
			return -EINVAL;
		}
	}

	if( (ul_x < 0) ||
			(ul_y < 0) )
	{
		debug(FLIDEBUG_FAIL,
			"FLISetImageArea(), area out of bounds: (%4d,%4d),(%4d,%4d)",
			ul_x, ul_y, lr_x, lr_y);
		return -EINVAL;
	}

	debug(FLIDEBUG_INFO,
		"Setting image area to: (%4d,%4d),(%4d,%4d)", ul_x, ul_y, lr_x, lr_y);

	switch (DEVICE->devinfo.devid)
  {
		/* MaxCam and IMG cameras */
		case FLIUSB_CAM_ID:
		{
			long rlen, wlen;
			iobuf_t buf[IOBUF_MAX_SIZ];

			rlen = 0; wlen = 6;
			IOWRITE_U16(buf, 0, FLI_USBCAM_SETFRAMEOFFSET);
			IOWRITE_U16(buf, 2, ul_x);
			IOWRITE_U16(buf, 4, ul_y);
			IO(dev, buf, &wlen, &rlen);
		}
		break;

		/* Proline Camera */
		case FLIUSB_PROLINE_ID:
		{
			/* JIM! perform some bounds checking... */
		}
		break;

		default:
			debug(FLIDEBUG_WARN, "Hmmm, shouldn't be here, operation on NO camera...");
			break;
	}

	if (r == 0)
	{
		cam->image_area.ul.x = ul_x;
	  cam->image_area.ul.y = ul_y;
		cam->image_area.lr.x = lr_x;
		cam->image_area.lr.y = lr_y;
		cam->grabrowwidth = (cam->image_area.lr.x - cam->image_area.ul.x) / cam->hbin;
	}

  return 0;
}

long fli_camera_usb_set_hbin(flidev_t dev, long hbin)
{
	iobuf_t buf[IOBUF_MAX_SIZ];
	long rlen, wlen;
//	long r = 0;
	flicamdata_t *cam = DEVICE->device_data;

  if ((hbin < 1) || (hbin > 16))
    return -EINVAL;

	switch (DEVICE->devinfo.devid)
  {
		/* MaxCam and IMG cameras */
		case FLIUSB_CAM_ID:
		{
			rlen = 0; wlen = 6;
			IOWRITE_U16(buf, 0, FLI_USBCAM_SETBINFACTORS);
			IOWRITE_U16(buf, 2, hbin);
			IOWRITE_U16(buf, 4, cam->vbin);
			IO(dev, buf, &wlen, &rlen);
		}
		break;

		/* Proline Camera */
		case FLIUSB_PROLINE_ID:
		{
			/* We do nothing here, h_bin is sent with start exposure command
			   this is a bug, TDI imaging will require this */
		}
		break;

		default:
			debug(FLIDEBUG_WARN, "Hmmm, shouldn't be here, operation on NO camera...");
			break;
	}

  cam->hbin = hbin;
  cam->grabrowwidth =
    (cam->image_area.lr.x - cam->image_area.ul.x) / cam->hbin;

  return 0;
}

long fli_camera_usb_set_vbin(flidev_t dev, long vbin)
{
	iobuf_t buf[IOBUF_MAX_SIZ];
	long rlen, wlen;
//	long r = 0;

	flicamdata_t *cam = DEVICE->device_data;

  if ((vbin < 1) || (vbin > 16))
    return -EINVAL;

	switch (DEVICE->devinfo.devid)
  {
		/* MaxCam and IMG cameras */
		case FLIUSB_CAM_ID:
		{
			rlen = 0; wlen = 6;
			IOWRITE_U16(buf, 0, FLI_USBCAM_SETBINFACTORS);
			IOWRITE_U16(buf, 2, cam->hbin);
			IOWRITE_U16(buf, 4, vbin);
			IO(dev, buf, &wlen, &rlen);
		}
		break;

		/* Proline Camera */
		case FLIUSB_PROLINE_ID:
		{
			/* We do nothing here, h_bin is sent with start exposure command
			   this is a bug, TDI imaging will require this */
		}
		break;

		default:
			debug(FLIDEBUG_WARN, "Hmmm, shouldn't be here, operation on NO camera...");
			break;
	}

  cam->vbin = vbin;
  return 0;
}

long fli_camera_usb_get_exposure_status(flidev_t dev, long *timeleft)
{
	switch (DEVICE->devinfo.devid)
  {
		/* MaxCam and IMG cameras */
		case FLIUSB_CAM_ID:
		{
			long rlen, wlen;
			iobuf_t buf[4];
			rlen = 4; wlen = 2;
			IOWRITE_U16(buf, 0, FLI_USBCAM_EXPOSURESTATUS);
			IO(dev, buf, &wlen, &rlen);
			IOREAD_U32(buf, 0, *timeleft);
		}
		break;

		/* Proline Camera */
		case FLIUSB_PROLINE_ID:
		{
			long rlen, wlen;
			iobuf_t buf[IOBUF_MAX_SIZ];

			rlen = 4; wlen = 2;
			IOWRITE_U16(buf, 0, PROLINE_COMMAND_GET_EXPOSURE_STATUS);
			IO(dev, buf, &wlen, &rlen);

			*timeleft = (buf[0] << 24) + (buf[1] << 16) + (buf[2] << 8) + buf[3];
		}
		break;

		default:
			debug(FLIDEBUG_WARN, "Hmmm, shouldn't be here, operation on NO camera...");
			break;
	}

	return 0;
}

long fli_camera_usb_cancel_exposure(flidev_t dev)
{
/*	flicamdata_t *cam = DEVICE->device_data; */

	switch (DEVICE->devinfo.devid)
  {
		/* MaxCam and IMG cameras */
		case FLIUSB_CAM_ID:
		{
			long rlen, wlen;
			iobuf_t buf[IOBUF_MAX_SIZ];

			rlen = 0; wlen = 4;
			IOWRITE_U16(buf, 0, FLI_USBCAM_ABORTEXPOSURE);
			IO(dev, buf, &wlen, &rlen);

			/* MaxCam (bug in firmware prevents shutter closing), so issue quick exposure... */
			rlen = 0; wlen = 8; /* Bias frame */
			IOWRITE_U16(buf, 0, FLI_USBCAM_SETEXPOSURE);
			IOWRITE_U32(buf, 4, 10);
			IO(dev, buf, &wlen, &rlen);

			/* Expose the bias frame */
			rlen = 0; wlen = 4;
			IOWRITE_U16(buf, 0, FLI_USBCAM_STARTEXPOSURE);
			IOWRITE_U16(buf, 2, 0);
			IO(dev, buf, &wlen, &rlen);
		}
		break;

		/* Proline Camera */
		case FLIUSB_PROLINE_ID:
		{
			long rlen = 2, wlen = 2;
			iobuf_t buf[IOBUF_MAX_SIZ];

			IOWRITE_U16(buf, 0, PROLINE_COMMAND_CANCEL_EXPOSURE);
			IO(dev, buf, &wlen, &rlen);
		}
		break;

		default:
			debug(FLIDEBUG_WARN, "Hmmm, shouldn't be here, operation on NO camera...");
			break;
	}

	return 0;
}


long fli_camera_usb_set_temperature(flidev_t dev, double temperature)
{
  flicamdata_t *cam = DEVICE->device_data;

	switch (DEVICE->devinfo.devid)
  {
		/* MaxCam and IMG cameras */
		case FLIUSB_CAM_ID:
		{
			unsigned short ad;
			long rlen, wlen;
			iobuf_t buf[4];

			if(DEVICE->devinfo.fwrev < 0x0200)
				return 0;

			if(cam->tempslope == 0.0)
				ad = 255;
			else
				ad = (unsigned short) ((temperature - cam->tempintercept) / cam->tempslope);

			debug(FLIDEBUG_INFO, "Temperature slope, intercept, AD val, %f %f %f %d", temperature, cam->tempslope, cam->tempintercept, ad);

			rlen = 0; wlen = 4;
			IOWRITE_U16(buf, 0, FLI_USBCAM_TEMPERATURE);
			IOWRITE_U16(buf, 2, ad);
			IO(dev, buf, &wlen, &rlen);
		}
		break;

		/* Proline Camera */
		case FLIUSB_PROLINE_ID:
		{
			long rlen, wlen;
			iobuf_t buf[IOBUF_MAX_SIZ];

			unsigned short a;

			short s_temp;

			s_temp = (short) (temperature * 256.0);
			rlen = 2; wlen = 4;
			IOWRITE_U16(buf, 0, PROLINE_COMMAND_SET_TEMPERATURE);
			IOWRITE_U16(buf, 2, s_temp);
			IO(dev, buf, &wlen, &rlen);

			a = (buf[0] << 8) + buf[1];
			debug(FLIDEBUG_INFO, "Got %d from camera.", a);
		}
		break;

		default:
			debug(FLIDEBUG_WARN, "Hmmm, shouldn't be here, operation on NO camera...");
			break;
	}

	return 0;
}

long fli_camera_usb_read_temperature(flidev_t dev, flichannel_t channel, double *temperature)
{
	flicamdata_t *cam = DEVICE->device_data;
	iobuf_t buf[IOBUF_MAX_SIZ];
	long rlen, wlen;
	long r = 0;

	switch (DEVICE->devinfo.devid)
  {
		/* MaxCam and IMG cameras */
		case FLIUSB_CAM_ID:
		{
			if (channel == 0)
			{
				rlen = 2; wlen = 2;
				IOWRITE_U16(buf, 0, FLI_USBCAM_TEMPERATURE);
				IO(dev, buf, &wlen, &rlen);
				*temperature = cam->tempslope * ((double) buf[1]) +
					cam->tempintercept;
			}
			else
			{
				r = -EINVAL;
			}
		}
		break;

		/* Proline Camera */
		case FLIUSB_PROLINE_ID:
		{
			double base, ccd;

			rlen = 14; wlen = 2;
			IOWRITE_U16(buf, 0, PROLINE_COMMAND_GET_TEMPERATURE);
			IO(dev, buf, &wlen, &rlen);

			ccd = (double) ((signed char) buf[0]) + ((double) buf[1] / 256);
			base = (double) ((signed char) buf[2]) + ((double) buf[3] / 256);

			switch (channel)
			{
				case FLI_TEMPERATURE_CCD:
					*temperature = ccd;
				break;

				case FLI_TEMPERATURE_BASE:
					*temperature = base;
				break;

				default:
					r = -EINVAL;
				break;
			}

//#define CHECK_ERRLIM
#ifdef CHECK_ERRLIM
			{
				unsigned long cnt;

				rlen = 4; wlen = 2;
				IOWRITE_U16(buf, 0, 0x0103);
				IO(dev, buf, &wlen, &rlen);
				IOREAD_U32(buf, 0, cnt);

				debug(FLIDEBUG_WARN, "USBERRCNT: %d", cnt);
			}
#endif

		}
		break;

		default:
			debug(FLIDEBUG_WARN, "Hmmm, shouldn't be here, operation on NO camera...");
			break;
	}

  return r;
}

long fli_camera_usb_get_temperature(flidev_t dev, double *temperature)
{
	return fli_camera_usb_read_temperature(dev, 0, temperature);
}

long fli_camera_usb_grab_row(flidev_t dev, void *buff, size_t width)
{
  flicamdata_t *cam = DEVICE->device_data;

	if(width > (size_t) (cam->image_area.lr.x - cam->image_area.ul.x))
	{
		debug(FLIDEBUG_FAIL, "FLIGrabRow(), requested row too wide.");
		debug(FLIDEBUG_FAIL, "  Requested width: %d", width);
		debug(FLIDEBUG_FAIL, "  FLISetImageArea() width: %d",
			cam->image_area.lr.x - cam->image_area.ul.x);
		return -EINVAL;
	}

	switch (DEVICE->devinfo.devid)
  {
		/* MaxCam and IMG cameras */
		case FLIUSB_CAM_ID:
		{
			long x;
			long r;

			if (cam->flushcountbeforefirstrow > 0)
			{
				debug(FLIDEBUG_INFO, "Flushing %d rows before image download.", cam->flushcountbeforefirstrow);
				if ((r = fli_camera_usb_flush_rows(dev, cam->flushcountbeforefirstrow, 1)))
					return r;

				cam->flushcountbeforefirstrow = 0;
			}

			if (cam->grabrowbufferindex >= cam->grabrowbatchsize)
			{
				/* We don't have the row in memory */
				long rlen, wlen;

				/* Do we have less than GrabRowBatchSize rows to grab? */
				if (cam->grabrowbatchsize > (cam->grabrowcounttot - cam->grabrowindex))
				{
					cam->grabrowbatchsize = cam->grabrowcounttot - cam->grabrowindex;

					if (cam->grabrowbatchsize < 1)
						cam->grabrowbatchsize = 1;
				}

				debug(FLIDEBUG_INFO, "Grabbing %d rows of width %d.", cam->grabrowbatchsize, cam->grabrowwidth);
				rlen = cam->grabrowwidth * 2 * cam->grabrowbatchsize;
				wlen = 6;
				cam->gbuf[0] = htons(FLI_USBCAM_SENDROW);
				cam->gbuf[1] = htons((unsigned short) cam->grabrowwidth);
				cam->gbuf[2] = htons((unsigned short) cam->grabrowbatchsize);
				IO(dev, cam->gbuf, &wlen, &rlen);

				for (x = 0; x < (cam->grabrowwidth * cam->grabrowbatchsize); x++)
				{
					if ((DEVICE->devinfo.hwrev & 0xff00) == 0x0100)
					{
						cam->gbuf[x] = ntohs(cam->gbuf[x]) + 32768;
					}
					else
					{
						cam->gbuf[x] = ntohs(cam->gbuf[x]);
					}
				}
				cam->grabrowbufferindex = 0;
			}

			for (x = 0; x < (long)width; x++)
			{
				((unsigned short *)buff)[x] =
					cam->gbuf[x + (cam->grabrowbufferindex * cam->grabrowwidth)];
			}

			cam->grabrowbufferindex++;
			cam->grabrowindex++;

			if (cam->grabrowcount > 0)
			{
				cam->grabrowcount--;
				if (cam->grabrowcount == 0)
				{
					if (cam->flushcountafterlastrow > 0)
					{
						debug(FLIDEBUG_INFO, "Flushing %d rows after image download.", cam->flushcountafterlastrow);
						if ((r = fli_camera_usb_flush_rows(dev, cam->flushcountafterlastrow, 1)))
							return r;
					}

					cam->flushcountafterlastrow = 0;
					cam->grabrowbatchsize = 1;
				}
			}


		}
		break;

		/* Proline Camera */

		/*
			grabrowindex -- current row being grabbed
			grabrowbatchsize -- number of words to grab
			grabrowcounttot -- number of words left in buffer 
			grabrowbufferindex -- location of the beginning of the row in the buffer
			flushcountafterlastrow -- unused
		*/

		case FLIUSB_PROLINE_ID:
		{
			long rlen, rtotal;

			/* First we need to determine if the row is in memory */
			if (cam->grabrowcounttot < cam->grabrowwidth)
			{
				long loadindex = 0;

				/* Ok, the buffer is double sized (cam->max_usb_xfer * 2) and can
				 * hold cam->max_usb_xfer / 2 words. We need to figure out which
				 * half of the buffer to fill with data
				 */

				/* Which half of the buffer are we in, which half to load? */
				if (cam->grabrowbufferindex == 0)
				{
					loadindex = 0;
				}
				else if (cam->grabrowbufferindex < (cam->max_usb_xfer / 2))
				{
					loadindex = (cam->max_usb_xfer / 2);
				}
				else if (cam->grabrowbufferindex == (cam->max_usb_xfer / 2))
				{
					loadindex = (cam->max_usb_xfer / 2);
				}
				else if (cam->grabrowbufferindex > (cam->max_usb_xfer / 2))
				{
					loadindex = 0;
				}

				/* Determine how many bytes to transfer */
				rlen = (((cam->grabrowcount - cam->grabrowindex) * cam->grabrowwidth) - cam->grabrowcounttot) * 2;

				if (rlen > cam->max_usb_xfer)
					rlen = cam->max_usb_xfer;

				memset(&cam->gbuf[loadindex], 0x00, rlen);

				rtotal = rlen;
				debug(FLIDEBUG_INFO, "Transferring %d starting at %d, buffer starts at %d.", rlen, cam->grabrowcounttot, cam->grabrowbufferindex);
				if ((usb_bulktransfer(dev, 0x82, &cam->gbuf[loadindex], &rlen)) != 0) /* Grab the buffer */
				{
					debug(FLIDEBUG_FAIL, "Read failed...");
				}

				if (rlen != rtotal)
				{
					debug(FLIDEBUG_FAIL, "Transfer did not complete, padding...");
					memset(&cam->gbuf[cam->grabrowcounttot], 0x00, (rtotal - rlen));
				}
				cam->grabrowcounttot += (rlen / 2);
			}

			/* Double check that row is in memory (an IO operation could have failed.) */
			if (cam->grabrowcounttot >= cam->grabrowwidth)
			{
				long l = 0;

				while (l < cam->grabrowwidth)
				{
					/* Are we at the end of the buffer? */
					if ((cam->grabrowbufferindex + cam->grabrowwidth) < ((cam->max_usb_xfer / 2) * 2) )
					{
						/* Not near end of buffer */
						while (l < cam->grabrowwidth)
						{
							((unsigned short *) buff)[l] = ((cam->gbuf[cam->grabrowbufferindex] << 8) & 0xff00) | ((cam->gbuf[cam->grabrowbufferindex] >> 8) & 0x00ff);
							cam->grabrowbufferindex ++;
							l ++;
						}
					}
					else
					{
						/* Near end of buffer */
						while (cam->grabrowbufferindex < ((cam->max_usb_xfer / 2) * 2))
						{
							((unsigned short *) buff)[l] = ((cam->gbuf[cam->grabrowbufferindex] << 8) & 0xff00) | ((cam->gbuf[cam->grabrowbufferindex] >> 8) & 0x00ff);
							cam->grabrowbufferindex ++;
							l ++;
						}
						cam->grabrowbufferindex = 0;
					}
				}

				cam->grabrowcounttot -= cam->grabrowwidth;
				cam->grabrowindex ++;
			}
		}
		break;

		default:
			debug(FLIDEBUG_WARN, "Hmmm, shouldn't be here, operation on NO camera...");
			break;
	}

	return 0;
}

long fli_camera_usb_expose_frame(flidev_t dev)
{
  flicamdata_t *cam = DEVICE->device_data;
	iobuf_t buf[IOBUF_MAX_SIZ];
	long rlen, wlen;
	long r = 0;

	switch (DEVICE->devinfo.devid)
  {
		/* MaxCam and IMG cameras */
		case FLIUSB_CAM_ID:
		{
			short flags = 0;

			rlen = 0; wlen = 6;
			IOWRITE_U16(buf, 0, FLI_USBCAM_SETFRAMEOFFSET);
			IOWRITE_U16(buf, 2, cam->image_area.ul.x);
			IOWRITE_U16(buf, 4, cam->image_area.ul.y);
			IO(dev, buf, &wlen, &rlen);

			rlen = 0; wlen = 6;
			IOWRITE_U16(buf, 0, FLI_USBCAM_SETBINFACTORS);
			IOWRITE_U16(buf, 2, cam->hbin);
			IOWRITE_U16(buf, 4, cam->vbin);
			IO(dev, buf, &wlen, &rlen);

			rlen = 0; wlen = 6;
			IOWRITE_U16(buf, 0, FLI_USBCAM_SETFLUSHBINFACTORS);
			IOWRITE_U16(buf, 2, cam->hflushbin);
			IOWRITE_U16(buf, 4, cam->vflushbin);
			IO(dev, buf, &wlen, &rlen);

			rlen = 0; wlen = 8;
			IOWRITE_U16(buf, 0, FLI_USBCAM_SETEXPOSURE);
			IOWRITE_U32(buf, 4, cam->exposure);
			IO(dev, buf, &wlen, &rlen);

			/* What flags do we need to send... */
			/* Dark Frame */
			flags |= (cam->frametype == FLI_FRAME_TYPE_DARK) ? 0x01 : 0x00;
			/* External trigger */
			flags |= (cam->exttrigger != 0) ? 0x04 : 0x00;
			flags |= (cam->exttriggerpol != 0) ? 0x08 : 0x00;

			debug(FLIDEBUG_INFO, "Exposure flags: %04x", flags);
			debug(FLIDEBUG_INFO, "Flushing %d times.", cam->flushes);
		//	debug(FLIDEBUG_INFO, "Flushing bin factor(X,Y) %d, %d.", cam->hflushbin, cam->vflushbin);

			if (cam->flushes > 0)
			{
				long r;

				if ((r = fli_camera_usb_flush_rows(dev,
									cam->ccd.array_area.lr.y - cam->ccd.array_area.ul.y,
									cam->flushes)))
					return r;
			}

			debug(FLIDEBUG_INFO, "Starting exposure.");
			rlen = 0; wlen = 4;
			IOWRITE_U16(buf, 0, FLI_USBCAM_STARTEXPOSURE);
			IOWRITE_U16(buf, 2, flags);
			IO(dev, buf, &wlen, &rlen);

			cam->grabrowcount = cam->image_area.lr.y - cam->image_area.ul.y;
			cam->grabrowcounttot = cam->grabrowcount;
			cam->grabrowwidth = cam->image_area.lr.x - cam->image_area.ul.x;
			cam->grabrowindex = 0;
			if (cam->grabrowwidth > 0){
				cam->grabrowbatchsize = USB_READ_SIZ_MAX / (cam->grabrowwidth * 2);
			}
			else
			{
				return -1;
			}

			/* Lets put some bounds on this... */
			if (cam->grabrowbatchsize > cam->grabrowcounttot)
				cam->grabrowbatchsize = cam->grabrowcounttot;

			if (cam->grabrowbatchsize > 64)
				cam->grabrowbatchsize = 64;

			/* We need to get a whole new buffer by default */
			cam->grabrowbufferindex = cam->grabrowbatchsize;

			cam->flushcountbeforefirstrow = cam->image_area.ul.y;
			cam->flushcountafterlastrow =
				(cam->ccd.array_area.lr.y - cam->ccd.array_area.ul.y) -
				((cam->image_area.lr.y - cam->image_area.ul.y) * cam->vbin) -
				cam->image_area.ul.y;

			if (cam->flushcountbeforefirstrow < 0)
				cam->flushcountbeforefirstrow = 0;

			if (cam->flushcountafterlastrow < 0)
				cam->flushcountafterlastrow = 0;
		}
		break;

		case FLIUSB_PROLINE_ID:
		{
			short h_offset;

			cam->grabrowcount = cam->image_area.lr.y - cam->image_area.ul.y; // Rows High
			cam->grabrowwidth = cam->image_area.lr.x - cam->image_area.ul.x; // Pixels Wide
			cam->flushcountbeforefirstrow = cam->image_area.ul.y; // Vertical Offset
			h_offset = cam->image_area.ul.x; // Horizontal Offset

			cam->grabrowindex = 0;
			cam->grabrowbatchsize = 0;
			cam->grabrowcounttot = 0;
			cam->grabrowbufferindex = 0;
			cam->flushcountafterlastrow = 0;

			if (cam->grabrowwidth <= 0)
				return -EINVAL;

			rlen = 0; wlen = 32;
			IOWRITE_U16(buf, 0, PROLINE_COMMAND_EXPOSE);

			/* Number of pixels wide */
			IOWRITE_U16(buf, 2, cam->grabrowwidth);

			/* Horizontal offset */
			IOWRITE_U16(buf, 4, h_offset);

			/* Number of vertical rows to grab */
			IOWRITE_U16(buf, 6, cam->grabrowcount);

			/* Vertical offset */
			IOWRITE_U16(buf, 8, cam->flushcountbeforefirstrow);

			/* Horizontal bin */
			IOWRITE_U8(buf, 10, cam->hbin);

			/* Vertical bin */
			IOWRITE_U8(buf, 11, cam->vbin);

			/* Exposure */
			IOWRITE_U32(buf, 12, cam->exposure);

			/* Now the exposure flags */
			buf[16]  = (cam->frametype == FLI_FRAME_TYPE_DARK) ? 0x01 : 0x00;

			buf[16] |= ((cam->exttrigger != 0) && (cam->exttriggerpol == 0)) ? 0x02 : 0x00;
			buf[16] |= ((cam->exttrigger != 0) && (cam->exttriggerpol != 0)) ? 0x04 : 0x00;

			/* Perform the transation */
			IO(dev, buf, &wlen, &rlen);
		}
		break;

		default:
			debug(FLIDEBUG_WARN, "Hmmm, shouldn't be here, operation on NO camera...");
			break;
	}

	return r;
}

long fli_camera_usb_flush_rows(flidev_t dev, long rows, long repeat)
{
  flicamdata_t *cam = DEVICE->device_data;
	iobuf_t buf[IOBUF_MAX_SIZ];
	long rlen, wlen;
	long r = 0;

  if (rows < 0)
    return -EINVAL;

  if (rows == 0)
    return 0;

	switch (DEVICE->devinfo.devid)
  {
		/* MaxCam and IMG cameras */
		case FLIUSB_CAM_ID:
		{
			rlen = 0; wlen = 6;
			IOWRITE_U16(buf, 0, FLI_USBCAM_SETFLUSHBINFACTORS);
			IOWRITE_U16(buf, 2, cam->hflushbin);
			IOWRITE_U16(buf, 4, cam->vflushbin);
			IO(dev, buf, &wlen, &rlen);

			while (repeat > 0)
			{
				debug(FLIDEBUG_INFO, "Flushing %d rows.", rows);
				rlen = 0; wlen = 4;
				IOWRITE_U16(buf, 0, FLI_USBCAM_FLUSHROWS);
				IOWRITE_U16(buf, 2, rows);
				IO(dev, buf, &wlen, &rlen);
				repeat--;
			}
		}
		break;

		/* Proline Camera */
		case FLIUSB_PROLINE_ID:
		{

		}
		break;

		default:
			debug(FLIDEBUG_WARN, "Hmmm, shouldn't be here, operation on NO camera...");
			break;
	}

	return r;
}

long fli_camera_usb_set_bit_depth(flidev_t dev, flibitdepth_t bitdepth)
{
//	flicamdata_t *cam = DEVICE->device_data;
//	iobuf_t buf[IOBUF_MAX_SIZ];
//	long rlen = 0, wlen = 0;
	long r = 0;

	switch (DEVICE->devinfo.devid)
  {
		/* MaxCam and IMG cameras */
		case FLIUSB_CAM_ID:
		{
			r = -EINVAL;
		}
		break;

		/* Proline Camera */
		case FLIUSB_PROLINE_ID:
		{
			r = -EINVAL;
		}
		break;

		default:
			debug(FLIDEBUG_WARN, "Hmmm, shouldn't be here, operation on NO camera...");
			break;
	}

	return r;
}

long fli_camera_usb_read_ioport(flidev_t dev, long *ioportset)
{
//  flicamdata_t *cam = DEVICE->device_data;
	iobuf_t buf[IOBUF_MAX_SIZ];
	long rlen, wlen;
	long r = 0;

	switch (DEVICE->devinfo.devid)
  {
		/* MaxCam and IMG cameras */
		case FLIUSB_CAM_ID:
		{
			rlen = 1; wlen = 2;
			IOWRITE_U16(buf, 0, FLI_USBCAM_READIO);
			IO(dev, buf, &wlen, &rlen);
			IOREAD_U8(buf, 0, *ioportset);
		}
		break;

		/* Proline Camera */
		case FLIUSB_PROLINE_ID:
		{
			rlen = 2; wlen = 2;
			IOWRITE_U16(buf, 0, PROLINE_COMMAND_READ_IOPORT);
			IO(dev, buf, &wlen, &rlen);
			IOREAD_U8(buf, 1, *ioportset);
		}
		break;

		default:
			debug(FLIDEBUG_WARN, "Hmmm, shouldn't be here, operation on NO camera...");
			break;
	}

  return r;
}

long fli_camera_usb_write_ioport(flidev_t dev, long ioportset)
{
//  flicamdata_t *cam = DEVICE->device_data;
	iobuf_t buf[IOBUF_MAX_SIZ];
	long rlen, wlen;
	long r = 0;

	switch (DEVICE->devinfo.devid)
  {
		/* MaxCam and IMG cameras */
		case FLIUSB_CAM_ID:
		{
			rlen = 0; wlen = 3;
			IOWRITE_U16(buf, 0, FLI_USBCAM_WRITEIO);
			IOWRITE_U8(buf, 2, ioportset);
			IO(dev, buf, &wlen, &rlen);
		}
		break;

		/* Proline Camera */
		case FLIUSB_PROLINE_ID:
		{
			rlen = 2; wlen = 4;
			IOWRITE_U16(buf, 0, PROLINE_COMMAND_WRITE_IOPORT);
			IOWRITE_U16(buf, 2, ioportset);
			IO(dev, buf, &wlen, &rlen);
		}
		break;

		default:
			debug(FLIDEBUG_WARN, "Hmmm, shouldn't be here, operation on NO camera...");
			break;
	}

  return r;
}

long fli_camera_usb_configure_ioport(flidev_t dev, long ioportset)
{
//  flicamdata_t *cam = DEVICE->device_data;
	iobuf_t buf[IOBUF_MAX_SIZ];
	long rlen, wlen;
	long r = 0;

	switch (DEVICE->devinfo.devid)
  {
		/* MaxCam and IMG cameras */
		case FLIUSB_CAM_ID:
		{
			rlen = 0; wlen = 3;
			IOWRITE_U16(buf, 0, FLI_USBCAM_WRITEDIR);
			IOWRITE_U8(buf, 2, ioportset);
			IO(dev, buf, &wlen, &rlen);
		}
		break;

		/* Proline Camera */
		case FLIUSB_PROLINE_ID:
		{
			rlen = 2; wlen = 4;
			IOWRITE_U16(buf, 0, PROLINE_COMMAND_CONFIGURE_IOPORT);
			IOWRITE_U16(buf, 2, ioportset);
			IO(dev, buf, &wlen, &rlen);
		}
		break;

		default:
			debug(FLIDEBUG_WARN, "Hmmm, shouldn't be here, operation on NO camera...");
			break;
	}

  return r;
}

long fli_camera_usb_control_shutter(flidev_t dev, long shutter)
{
//  flicamdata_t *cam = DEVICE->device_data;
	iobuf_t buf[IOBUF_MAX_SIZ];
	long rlen, wlen;
	long r = 0;

	switch (DEVICE->devinfo.devid)
  {
		/* MaxCam and IMG cameras */
		case FLIUSB_CAM_ID:
		{
			rlen = 0; wlen = 3;
			IOWRITE_U16(buf, 0, FLI_USBCAM_SHUTTER);
			IOWRITE_U8(buf, 2, shutter);
			IO(dev, buf, &wlen, &rlen);
		}
		break;

		/* Proline Camera */
		case FLIUSB_PROLINE_ID:
		{
			unsigned char c = 0;

			debug(FLIDEBUG_INFO, "JIM!!");


			switch (shutter)
			{
				case FLI_SHUTTER_CLOSE:
					c = 0x00;
					break;

				case FLI_SHUTTER_OPEN:
					c = 0x01;
					break;

				default:
					r = -EINVAL;
					break;
			}

			if (r != 0)
				break;

			rlen = 2; wlen = 3;
			IOWRITE_U16(buf, 0, PROLINE_COMMAND_SET_SHUTTER);
			IOWRITE_U8(buf, 2, c);

			debug(FLIDEBUG_INFO, "%s shutter.", (buf[2] == 0)? "Closing":"Opening");
			IO(dev, buf, &wlen, &rlen);
		}
		break;

		default:
			debug(FLIDEBUG_WARN, "Hmmm, shouldn't be here, operation on NO camera...");
			break;
	}

	return r;
}

long fli_camera_usb_control_bgflush(flidev_t dev, long bgflush)
{
  flicamdata_t *cam = DEVICE->device_data;
	iobuf_t buf[IOBUF_MAX_SIZ];
	long rlen, wlen;
	long r = 0;

	if( (bgflush != FLI_BGFLUSH_STOP) &&
		(bgflush != FLI_BGFLUSH_START) )
	return -EINVAL;

	cam->background_flush = (bgflush == FLI_BGFLUSH_STOP)? 0 : 1;

	switch (DEVICE->devinfo.devid)
  {
		/* MaxCam and IMG cameras */
		case FLIUSB_CAM_ID:
		{
			if(DEVICE->devinfo.fwrev < 0x0300)
			{
				debug(FLIDEBUG_WARN, "Background flush commanded on early firmware.");
				return -EFAULT;
			}

			rlen = 0; wlen = 4;
			IOWRITE_U16(buf, 0, FLI_USBCAM_BGFLUSH);
			IOWRITE_U16(buf, 2, bgflush);
			IO(dev, buf, &wlen, &rlen);
		}
		break;

		/* Proline Camera */
		case FLIUSB_PROLINE_ID:
		{

		}
		break;

		default:
			debug(FLIDEBUG_WARN, "Hmmm, shouldn't be here, operation on NO camera...");
			break;
	}

  return r;
}

long fli_camera_usb_get_cooler_power(flidev_t dev, double *power)
{
//  flicamdata_t *cam = DEVICE->device_data;
	iobuf_t buf[IOBUF_MAX_SIZ];
	long rlen, wlen;
	long r = 0;

	*power = 0.0;
	switch (DEVICE->devinfo.devid)
  {
		/* MaxCam and IMG cameras */
		case FLIUSB_CAM_ID:
		{
			return -EFAULT;
		}
		break;

		/* Proline Camera */
		case FLIUSB_PROLINE_ID:
		{
			short pwm;

			rlen = 14; wlen = 2;
			IOWRITE_U16(buf, 0, PROLINE_COMMAND_GET_TEMPERATURE);
			IO(dev, buf, &wlen, &rlen);

			IOREAD_U16(buf, 4, pwm);
			*power = (double) pwm;
		}
		break;

		default:
			debug(FLIDEBUG_WARN, "Hmmm, shouldn't be here, operation on NO camera...");
			break;
	}

	debug(FLIDEBUG_INFO, "Cooler power: %f", *power);
  return r;
}

long fli_camera_usb_get_camera_status(flidev_t dev, long *camera_status)
{
//  flicamdata_t *cam = DEVICE->device_data;
	iobuf_t buf[IOBUF_MAX_SIZ];
	long rlen, wlen;
	long r = 0;

	switch (DEVICE->devinfo.devid)
  {
		/* MaxCam and IMG cameras */
		case FLIUSB_CAM_ID:
		{

		}
		break;

		/* Proline Camera */
		case FLIUSB_PROLINE_ID:
		{
			rlen = 4; wlen = 2;
			IOWRITE_U16(buf, 0, PROLINE_COMMAND_GET_STATUS);
			IO(dev, buf, &wlen, &rlen);
			IOREAD_U32(buf, 0, *camera_status);
		}
		break;

		default:
			debug(FLIDEBUG_WARN, "Hmmm, shouldn't be here, operation on NO camera...");
			break;
	}

  return r;
}

long fli_camera_usb_get_camera_mode(flidev_t dev, flimode_t *camera_mode)
{
//  flicamdata_t *cam = DEVICE->device_data;
	iobuf_t buf[IOBUF_MAX_SIZ];
	long rlen, wlen;
	long r = 0;

	switch (DEVICE->devinfo.devid)
  {
		/* MaxCam and IMG cameras */
		case FLIUSB_CAM_ID:
		{
			*camera_mode = 0;
		}
		break;

		/* Proline Camera */
		case FLIUSB_PROLINE_ID:
		{
			rlen = 2; wlen = 2;
			IOWRITE_U16(buf, 0, PROLINE_COMMAND_GET_CURRENT_MODE);
			IO(dev, buf, &wlen, &rlen);
			IOREAD_U16(buf, 0, *camera_mode);
		}
		break;

		default:
			debug(FLIDEBUG_WARN, "Hmmm, shouldn't be here, operation on NO camera...");
			break;
	}

  return r;
}

long fli_camera_usb_set_camera_mode(flidev_t dev, flimode_t camera_mode)
{
//  flicamdata_t *cam = DEVICE->device_data;
	iobuf_t buf[IOBUF_MAX_SIZ];
	long rlen, wlen;
	long r = 0;

	switch (DEVICE->devinfo.devid)
  {
		/* MaxCam and IMG cameras */
		case FLIUSB_CAM_ID:
		{
			if (camera_mode > 0)
				r = -EINVAL;
		}
		break;

		/* Proline Camera */
		case FLIUSB_PROLINE_ID:
		{
			flimode_t mode;

			rlen = 2; wlen = 4;
			IOWRITE_U16(buf, 0, PROLINE_COMMAND_SET_MODE);
			IOWRITE_U16(buf, 2, camera_mode);
			IO(dev, buf, &wlen, &rlen);
			IOREAD_U16(buf, 0, mode);

			if (mode != camera_mode)
			{
				debug(FLIDEBUG_FAIL, "Error setting camera mode, tried %d, performed %d.", camera_mode, mode);
				r = -EINVAL;
			}

		}
		break;

		default:
			debug(FLIDEBUG_WARN, "Hmmm, shouldn't be here, operation on NO camera...");
			break;
	}

  return r;
}

long fli_camera_usb_get_camera_mode_string(flidev_t dev, flimode_t camera_mode, char *dest, size_t siz)
{
//  flicamdata_t *cam = DEVICE->device_data;
	iobuf_t buf[IOBUF_MAX_SIZ];
	long rlen, wlen;
	long r = 0;

	switch (DEVICE->devinfo.devid)
  {
		/* MaxCam and IMG cameras */
		case FLIUSB_CAM_ID:
		{
			if (camera_mode > 0)
				r = -EINVAL;
			else
				strncpy((char *) dest, "Default Mode", siz - 1);
		}
		break;

		/* Proline Camera */
		case FLIUSB_PROLINE_ID:
		{
			rlen = 32; wlen = 4;
			IOWRITE_U16(buf, 0, PROLINE_COMMAND_GET_MODE_STRING);
			IOWRITE_U16(buf, 2, camera_mode);
			IO(dev, buf, &wlen, &rlen);

			strncpy((char *) dest, (char *) buf, MIN(siz - 1, 31));
			if (dest[0] == '\0')
				r = -EINVAL;
		}
		break;

		default:
			debug(FLIDEBUG_WARN, "Hmmm, shouldn't be here, operation on NO camera...");
			break;
	}

  return r;
}
