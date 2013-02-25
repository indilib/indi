/*
 Starlight Xpress CCD INDI Driver

 Code is based on SX SDK by David Schmenk and Craig Stark
 Copyright (c) 2003 David Schmenk
 All rights reserved.

 Changes for INDI project by Peter Polakovic
 Copyright (c) 2012 Cloudmakers, s. r. o.
 All Rights Reserved.

 Permission is hereby granted, free of charge, to any person obtaining a
 copy of this software and associated documentation files (the
 "Software"), to deal in the Software without restriction, including
 without limitation the rights to use, copy, modify, merge, publish,
 distribute, and/or sell copies of the Software, and to permit persons
 to whom the Software is furnished to do so, provided that the above
 copyright notice(s) and this permission notice appear in all copies of
 the Software and that both the above copyright notice(s) and this
 permission notice appear in supporting documentation.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT
 OF THIRD PARTY RIGHTS. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
 HOLDERS INCLUDED IN THIS NOTICE BE LIABLE FOR ANY CLAIM, OR ANY SPECIAL
 INDIRECT OR CONSEQUENTIAL DAMAGES, OR ANY DAMAGES WHATSOEVER RESULTING
 FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
 NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION
 WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

 Except as contained in this notice, the name of a copyright holder
 shall not be used in advertising or otherwise to promote the sale, use
 or other dealings in this Software without prior written authorization
 of the copyright holder.
 */

#include <stdio.h>
#include <memory.h>

#include "sxconfig.h"
#include "sxccdusb.h"

/*
 * Control request fields.
 */
#define USB_REQ_TYPE                0
#define USB_REQ                     1
#define USB_REQ_VALUE_L             2
#define USB_REQ_VALUE_H             3
#define USB_REQ_INDEX_L             4
#define USB_REQ_INDEX_H             5
#define USB_REQ_LENGTH_L            6
#define USB_REQ_LENGTH_H            7
#define USB_REQ_DATA                8
#define USB_REQ_DIR(r)              ((r)&(1<<7))
#define USB_REQ_DATAOUT             0x00
#define USB_REQ_DATAIN              0x80
#define USB_REQ_KIND(r)             ((r)&(3<<5))
#define USB_REQ_VENDOR              (2<<5)
#define USB_REQ_STD                 0
#define USB_REQ_RECIP(r)            ((r)&31)
#define USB_REQ_DEVICE              0x00
#define USB_REQ_IFACE               0x01
#define USB_REQ_ENDPOINT            0x02
#define USB_DATAIN                  0x80
#define USB_DATAOUT                 0x00

/*
 * CCD camera control commands.
 */
#define SXUSB_GET_FIRMWARE_VERSION  255
#define SXUSB_ECHO                  0
#define SXUSB_CLEAR_PIXELS          1
#define SXUSB_READ_PIXELS_DELAYED   2
#define SXUSB_READ_PIXELS           3
#define SXUSB_SET_TIMER             4
#define SXUSB_GET_TIMER             5
#define SXUSB_RESET                 6
#define SXUSB_SET_CCD               7
#define SXUSB_GET_CCD               8
#define SXUSB_SET_STAR2K            9
#define SXUSB_WRITE_SERIAL_PORT     10
#define SXUSB_READ_SERIAL_PORT      11
#define SXUSB_SET_SERIAL            12
#define SXUSB_GET_SERIAL            13
#define SXUSB_CAMERA_MODEL          14
#define SXUSB_LOAD_EEPROM           15
#define SXUSB_SET_A2D               16
#define SXUSB_RED_A2D               17
#define SXUSB_READ_PIXELS_GATED     18
#define SXUSB_BUILD_NUMBER          19
#define SXUSB_COOLER_CONTROL        30
#define SXUSB_COOLER                30
#define SXUSB_COOLER_TEMPERATURE    31
#define SXUSB_SHUTTER_CONTROL       32
#define SXUSB_SHUTTER               32
#define SXUSB_READ_I2CPORT          33

#define SX_VID                      0x1278

#define SX_USB_VID                  0x4444
#define SX_USB_PID                  0x4220
#define SX_USB_NAME                 "SX-USB"

#define BULK_IN                     0x0082
#define BULK_OUT                    0x0001

#define BULK_COMMAND_TIMEOUT        3000
#define BULK_DATA_TIMEOUT           15000

#define TRACE(c) (c)
#define DEBUG(c) (c)

static const char *SX_NAMES[] = { "SXVF-M5", "SXVF-M5C", "SXVF-M7", "SXVF-M7C", "SXVF-M8C", "SXVF-M9", "SXVR-M25C", "SXVR-M26C", "SXVR-H18", "SXVR-H16", "SXVR-H35", "SXVR-H36", "SXVR-H9", "SXVR-H9C", "SXVR-H9", "SXVR-H9C", "Lodestar", "CoStar", "Superstar", "MX Camera", NULL };
static int SX_PIDS[] = { 0x0105, 0x0305, 0x0107, 0x0307, 0x0308, 0x0109, 0x0325, 0x0326, 0x0128, 0x0126, 0x0135, 0x0136, 0x0119, 0x0319, 0x100, 0x100, 0x0507, 0x0517, 0x0509, 0x0200, 0 };

static void init() {
  static int done = 0;
  if (!done) {
    usb_init();
    usb_find_busses();
    usb_find_devices();
    done = 1;
  }
}

int sxList(DEVICE *sxDevices, const char **names) {
  TRACE(fprintf(stderr, "-> sxList(...)\n"));
  init();
  int count=0;
  for (struct usb_bus *bus = usb_get_busses(); bus && count<20; bus = bus->next) {
    for (struct usb_device *dev = bus->devices; dev && count<20; dev = dev->next) {
      if (dev->descriptor.idVendor == SX_VID) {
        int pid = dev->descriptor.idProduct;
        for (int i = 0; SX_PIDS[i]; i++) {
          if (pid == SX_PIDS[i]) {
            TRACE(fprintf(stderr, "   '%s' [0x%x, 0x%x] found...\n", SX_NAMES[i], SX_VID, pid));
            names[count]=SX_NAMES[i];
            sxDevices[count++] = dev;
            break;
          }
        }
      } else if (dev->descriptor.idVendor == SX_USB_VID && dev->descriptor.idProduct == SX_USB_PID) {
        TRACE(fprintf(stderr, "   '%s' [0x%x, 0x%x] found...\n", SX_USB_NAME, SX_USB_VID, SX_USB_PID));
        names[count]=SX_USB_NAME;
        sxDevices[count++] = dev;
      }
    }
  }
  TRACE(fprintf(stderr, "<- sxList %d\n", count));
  return count;
}

int sxOpen(HANDLE *sxHandles) {
  TRACE(fprintf(stderr, "-> sxOpen(...)\n"));
  init();

  DEVICE devices[20];
  const char* names[20];
  int count = sxList(devices, names);

  for (int i = 0; i < count; i++) {
    TRACE(fprintf(stderr, "   opening '%s' [0x%x, 0x%x]\n", names[i], devices[i]->descriptor.idVendor, devices[i]->descriptor.idProduct));
    HANDLE handle = usb_open(devices[i]);
    TRACE(fprintf(stderr, "   usb_open() -> %ld\n", (long)handle));
    if (handle != NULL) {
      int rc;
      rc=usb_detach_kernel_driver_np(handle, 0);
      TRACE(fprintf(stderr, "   usb_detach_kernel_driver_np() -> %d\n", rc));
#ifdef __APPLE__
      rc=usb_claim_interface(handle,0);
#else
      rc = usb_claim_interface(handle, 1);
#endif
      TRACE(fprintf(stderr, "   usb_claim_interface() -> %d\n", rc));
      if (rc>=0) {
        sxHandles[i] = handle;
      }
    }

  }

  TRACE(fprintf(stderr, "<- sxOpen %d\n", count));
  return count;
}

void sxClose(HANDLE sxHandle) {
  usb_close(sxHandle);
}

int sxReset(HANDLE sxHandle) {
  TRACE(fprintf(stderr, "-> sxReset(...)\n"));
  unsigned char setup_data[8];
  setup_data[USB_REQ_TYPE ] = USB_REQ_VENDOR | USB_REQ_DATAOUT;
  setup_data[USB_REQ ] = SXUSB_RESET;
  setup_data[USB_REQ_VALUE_L ] = 0;
  setup_data[USB_REQ_VALUE_H ] = 0;
  setup_data[USB_REQ_INDEX_L ] = 0;
  setup_data[USB_REQ_INDEX_H ] = 0;
  setup_data[USB_REQ_LENGTH_L] = 0;
  setup_data[USB_REQ_LENGTH_H] = 0;
  int rc = usb_bulk_write(sxHandle, BULK_OUT, (char *)setup_data, 8, BULK_COMMAND_TIMEOUT);
  TRACE(fprintf(stderr, "   usb_bulk_write() -> %d\n", rc));
  TRACE(fprintf(stderr, "<- sxReset %d\n", rc >= 0));
  return rc >= 0;
}

unsigned short sxGetCameraModel(HANDLE sxHandle) {
  TRACE(fprintf(stderr, "-> sxGetCameraModel(...)\n"));
  unsigned char setup_data[8];
  setup_data[USB_REQ_TYPE ] = USB_REQ_VENDOR | USB_REQ_DATAIN;
  setup_data[USB_REQ ] = SXUSB_CAMERA_MODEL;
  setup_data[USB_REQ_VALUE_L ] = 0;
  setup_data[USB_REQ_VALUE_H ] = 0;
  setup_data[USB_REQ_INDEX_L ] = 0;
  setup_data[USB_REQ_INDEX_H ] = 0;
  setup_data[USB_REQ_LENGTH_L] = 2;
  setup_data[USB_REQ_LENGTH_H] = 0;
  int rc = usb_bulk_write(sxHandle, BULK_OUT, (char *)setup_data, 8, BULK_COMMAND_TIMEOUT);
  TRACE(fprintf(stderr, "   usb_bulk_write() -> %d\n", rc));
  if (rc == 8) {
    rc = usb_bulk_read(sxHandle, BULK_IN, (char *)setup_data, 2, BULK_COMMAND_TIMEOUT);
    TRACE(fprintf(stderr, "   usb_bulk_read() -> %d\n", rc));
    if (rc == 2) {
      int result=setup_data[0] | (setup_data[1] << 8);
      TRACE(fprintf(stderr, "   %s %s model %d\n", result & 0x40 ? "INTERLACED" : "NON-INTERLACED", result & 0x80 ? "COLOR" : "MONO", result & 0x1F));
      TRACE(fprintf(stderr, "<- 0x%x\n", result));
      return result;
    }
  }
  TRACE(fprintf(stderr, "<- sxGetCameraModel 0\n"));
  return 0;
}

unsigned long sxGetFirmwareVersion(HANDLE sxHandle) {
  TRACE(fprintf(stderr, "-> sxGetFirmwareVersion(...)\n"));
  unsigned char setup_data[8];
  setup_data[USB_REQ_TYPE ] = USB_REQ_VENDOR | USB_REQ_DATAIN;
  setup_data[USB_REQ ] = SXUSB_GET_FIRMWARE_VERSION;
  setup_data[USB_REQ_VALUE_L ] = 0;
  setup_data[USB_REQ_VALUE_H ] = 0;
  setup_data[USB_REQ_INDEX_L ] = 0;
  setup_data[USB_REQ_INDEX_H ] = 0;
  setup_data[USB_REQ_LENGTH_L] = 4;
  setup_data[USB_REQ_LENGTH_H] = 0;
  int rc = usb_bulk_write(sxHandle, BULK_OUT, (char *)setup_data, 8, BULK_COMMAND_TIMEOUT);
  TRACE(fprintf(stderr, "   usb_bulk_write() -> %d\n", rc));
  if (rc == 8) {
    rc = usb_bulk_read(sxHandle, BULK_IN, (char *)setup_data, 4, BULK_COMMAND_TIMEOUT);
    TRACE(fprintf(stderr, "   usb_bulk_read() -> %d\n", rc));
    if (rc == 4) {
      unsigned long result=((unsigned long)setup_data[0] | ((unsigned long)setup_data[1] << 8) | ((unsigned long)setup_data[2] << 16) | ((unsigned long)setup_data[3] << 24));
      TRACE(fprintf(stderr, "<- 0x%lx\n", result));
      return result;
    }
  }
  TRACE(fprintf(stderr, "<- sxGetFirmwareVersion 0\n"));
  return 0;
}

unsigned short sxGetBuildNumber(HANDLE sxHandle) {
  TRACE(fprintf(stderr, "-> sxGetBuildNumber(...)\n"));
  unsigned char setup_data[8];
  setup_data[USB_REQ_TYPE ] = USB_REQ_VENDOR | USB_REQ_DATAIN;
  setup_data[USB_REQ ] = SXUSB_BUILD_NUMBER;;
  setup_data[USB_REQ_VALUE_L ] = 0;
  setup_data[USB_REQ_VALUE_H ] = 0;
  setup_data[USB_REQ_INDEX_L ] = 0;
  setup_data[USB_REQ_INDEX_H ] = 0;
  setup_data[USB_REQ_LENGTH_L] = 4;
  setup_data[USB_REQ_LENGTH_H] = 0;
  int rc = usb_bulk_write(sxHandle, BULK_OUT, (char *)setup_data, 8, BULK_COMMAND_TIMEOUT);
  TRACE(fprintf(stderr, "   usb_bulk_write() -> %d\n", rc));
  if (rc == 8) {
    rc = usb_bulk_read(sxHandle, BULK_IN, (char *)setup_data, 2, BULK_COMMAND_TIMEOUT);
    TRACE(fprintf(stderr, "   usb_bulk_read() -> %d\n", rc));
    if (rc == 2) {
      int result=setup_data[0] | (setup_data[1] << 8);
      TRACE(fprintf(stderr, "<- 0x%x\n", result));
      return result;
    }
  }
  TRACE(fprintf(stderr, "<- sxGetBuildNumber 0\n"));
  return 0;
}

int sxGetCameraParams(HANDLE sxHandle, unsigned short camIndex, struct t_sxccd_params *params) {
  TRACE(fprintf(stderr, "-> sxGetCameraParams(..., %d, ...)\n", camIndex));
  unsigned char setup_data[17];
  setup_data[USB_REQ_TYPE ] = USB_REQ_VENDOR | USB_REQ_DATAIN;
  setup_data[USB_REQ ] = SXUSB_GET_CCD;
  setup_data[USB_REQ_VALUE_L ] = 0;
  setup_data[USB_REQ_VALUE_H ] = 0;
  setup_data[USB_REQ_INDEX_L ] = (char)camIndex;
  setup_data[USB_REQ_INDEX_H ] = 0;
  setup_data[USB_REQ_LENGTH_L] = 17;
  setup_data[USB_REQ_LENGTH_H] = 0;
  int rc = usb_bulk_write(sxHandle, BULK_OUT, (char *)setup_data, 8, BULK_COMMAND_TIMEOUT);
  TRACE(fprintf(stderr, "   usb_bulk_write() -> %d\n", rc));
  if (rc == 8) {
    rc = usb_bulk_read(sxHandle, BULK_IN, (char *)setup_data, 17, BULK_COMMAND_TIMEOUT);
    TRACE(fprintf(stderr, "   usb_bulk_read() -> %d\n", rc));
    if (rc == 17) {
      params->hfront_porch = setup_data[0];
      params->hback_porch = setup_data[1];
      params->width = setup_data[2] | (setup_data[3] << 8);
      params->vfront_porch = setup_data[4];
      params->vback_porch = setup_data[5];
      params->height = setup_data[6] | (setup_data[7] << 8);
      params->pix_width = (float)((setup_data[8] | (setup_data[9] << 8)) / 256.0);
      params->pix_height = (float)((setup_data[10] | (setup_data[11] << 8)) / 256.0);
      params->color_matrix = setup_data[12] | (setup_data[13] << 8);
      params->bits_per_pixel = setup_data[14];
      params->num_serial_ports = setup_data[15];
      params->extra_caps = setup_data[16];
      TRACE(fprintf(stderr, " chip size: %d x %d x %d, pixel size: %4.2f x %4.2f, matrix type: %x\n", params->width, params->height, params->bits_per_pixel, params->pix_width, params->pix_height, params->color_matrix));
      TRACE(fprintf(stderr, " capabilities:%s%s%s%s\n", (params->extra_caps & SXCCD_CAPS_GUIDER ? " GUIDER" : ""), (params->extra_caps & SXCCD_CAPS_STAR2K ? " STAR2K" : ""), (params->extra_caps & SXUSB_CAPS_COOLER ? " COOLER" : ""), (params->extra_caps & SXUSB_CAPS_SHUTTER ? " SHUTTER" : "")));
      TRACE(fprintf(stderr, " serial ports: %d\n", params->num_serial_ports));
    }
  }
  TRACE(fprintf(stderr, "<- sxGetCameraParams %d\n", rc >= 0));
  return rc >= 0;
}

int sxSetShutter(HANDLE sxHandle, unsigned short state) {
  TRACE(fprintf(stderr, "-> sxSetShutter(..., %d)\n", state));
  unsigned char setup_data[8];
  setup_data[USB_REQ_TYPE ] = USB_REQ_VENDOR;
  setup_data[USB_REQ ] = SXUSB_SHUTTER;
  setup_data[USB_REQ_VALUE_L ] = 0;
  setup_data[USB_REQ_VALUE_H ] = state?128:64;
  setup_data[USB_REQ_INDEX_L ] = 0;
  setup_data[USB_REQ_INDEX_H ] = 0;
  setup_data[USB_REQ_LENGTH_L] = 0;
  setup_data[USB_REQ_LENGTH_H] = 0;
  int rc = usb_bulk_write(sxHandle, BULK_OUT, (char *)setup_data, 8, BULK_COMMAND_TIMEOUT);
  TRACE(fprintf(stderr, "   usb_bulk_write() -> %d\n", rc));
  if (rc == 8) {
    rc = usb_bulk_read(sxHandle, BULK_IN, (char *)setup_data, 2, BULK_COMMAND_TIMEOUT);
    TRACE(fprintf(stderr, "   usb_bulk_read() -> %d\n", rc));
    if (rc == 2) {
      int result=setup_data[0] | (setup_data[1] << 8);
      TRACE(fprintf(stderr, "<- 0x%x\n", result));
      return result;
    }
  }
  TRACE(fprintf(stderr, "<- sxSetShutter 0\n"));
  return 0;
}

int sxSetTimer(HANDLE sxHandle, unsigned long msec) {
  TRACE(fprintf(stderr, "-> sxSetTimer(..., %lu)\n", msec));
  unsigned char setup_data[12];
  setup_data[USB_REQ_TYPE ] = USB_REQ_VENDOR | USB_REQ_DATAOUT;
  setup_data[USB_REQ ] = SXUSB_SET_TIMER;
  setup_data[USB_REQ_VALUE_L ] = 0;
  setup_data[USB_REQ_VALUE_H ] = 0;
  setup_data[USB_REQ_INDEX_L ] = 0;
  setup_data[USB_REQ_INDEX_H ] = 0;
  setup_data[USB_REQ_LENGTH_L] = 4;
  setup_data[USB_REQ_LENGTH_H] = 0;
  setup_data[USB_REQ_DATA + 0] = (unsigned char)msec;
  setup_data[USB_REQ_DATA + 1] = (unsigned char)(msec >> 8);
  setup_data[USB_REQ_DATA + 2] = (unsigned char)(msec >> 16);
  setup_data[USB_REQ_DATA + 3] = (unsigned char)(msec >> 24);
  int rc = usb_bulk_write(sxHandle, BULK_OUT, (char *)setup_data, 12, BULK_COMMAND_TIMEOUT);
  TRACE(fprintf(stderr, "   usb_bulk_write() -> %d\n", rc));
  TRACE(fprintf(stderr, "<- sxSetTimer %d\n", rc >= 0));
  return rc >= 0;
}

unsigned long sxGetTimer(HANDLE sxHandle) {
  TRACE(fprintf(stderr, "-> sxGetTimer(...)\n"));
  unsigned char setup_data[8];
  setup_data[USB_REQ_TYPE ] = USB_REQ_VENDOR | USB_REQ_DATAIN;
  setup_data[USB_REQ ] = SXUSB_GET_TIMER;
  setup_data[USB_REQ_VALUE_L ] = 0;
  setup_data[USB_REQ_VALUE_H ] = 0;
  setup_data[USB_REQ_INDEX_L ] = 0;
  setup_data[USB_REQ_INDEX_H ] = 0;
  setup_data[USB_REQ_LENGTH_L] = 4;
  setup_data[USB_REQ_LENGTH_H] = 0;
  int rc = usb_bulk_write(sxHandle, BULK_OUT, (char *)setup_data, 8, BULK_COMMAND_TIMEOUT);
  TRACE(fprintf(stderr, "   usb_bulk_write() -> %d\n", rc));
  if (rc == 8) {
    rc = usb_bulk_read(sxHandle, BULK_IN, (char *)setup_data, 4, BULK_COMMAND_TIMEOUT);
    TRACE(fprintf(stderr, "   usb_bulk_read() -> %d\n", rc));
    if (rc == 4) {
      unsigned long result=((unsigned long)setup_data[0] | ((unsigned long)setup_data[1] << 8) | ((unsigned long)setup_data[2] << 16) | ((unsigned long)setup_data[3] << 24));
      TRACE(fprintf(stderr, "<- sxGetTimer %lu\n", result));
      return result;
    }
  }
  TRACE(fprintf(stderr, "<- sxGetTimer 0\n"));
  return 0;
}

int sxSetCooler(HANDLE sxHandle, unsigned char setStatus, unsigned short setTemp, unsigned char *retStatus, unsigned short *retTemp) {
  TRACE(fprintf(stderr, "-> sxSetCooler(..., %d, %d, ...)\n", setStatus, setTemp));
  unsigned char setup_data[8];
  setup_data[USB_REQ_TYPE ] = USB_REQ_VENDOR;
  setup_data[USB_REQ ] = SXUSB_COOLER;
  setup_data[USB_REQ_VALUE_L ] = setTemp & 0xFF;
  setup_data[USB_REQ_VALUE_H ] = (setTemp >> 8) & 0xFF;
  setup_data[USB_REQ_INDEX_L ] = setStatus ? 1 : 0;
  setup_data[USB_REQ_INDEX_H ] = 0;
  setup_data[USB_REQ_LENGTH_L] = 0;
  setup_data[USB_REQ_LENGTH_H] = 0;
  int rc = usb_bulk_write(sxHandle, BULK_OUT, (char *)setup_data, 8, BULK_COMMAND_TIMEOUT);
  TRACE(fprintf(stderr, "   usb_bulk_write() -> %d\n", rc));
  if (rc == 8) {
    rc = usb_bulk_read(sxHandle, BULK_IN, (char *)setup_data, 3, BULK_COMMAND_TIMEOUT);
    TRACE(fprintf(stderr, "   usb_bulk_read() -> %d\n", rc));
    if (rc == 3) {
      *retTemp = (setup_data[1] * 256) + setup_data[0];
      if (setup_data[2]) {
        *retStatus = 1;
      }
      else {
        *retStatus=0;
      }
      TRACE(fprintf(stderr, "   status: %d -> %d\n", setStatus, *retStatus));
      TRACE(fprintf(stderr, "   temperature: %4.1f -> %4.1f\n", (setTemp-2730)/10.0, (*retTemp-2730)/10.0));
      TRACE(fprintf(stderr, "<- 1\n"));
      return 1;
    }
  }
  TRACE(fprintf(stderr, "<- sxSetCooler 0\n"));
  return 0;
}

int sxClearPixels(HANDLE sxHandle, unsigned short flags, unsigned short camIndex) {
  TRACE(fprintf(stderr, "-> sxClearPixels(..., 0x%x, %d)\n", flags, camIndex));
  unsigned char setup_data[8];
  setup_data[USB_REQ_TYPE ] = USB_REQ_VENDOR | USB_REQ_DATAOUT;
  setup_data[USB_REQ ] = SXUSB_CLEAR_PIXELS;
  setup_data[USB_REQ_VALUE_L ] = (unsigned char)flags;
  setup_data[USB_REQ_VALUE_H ] = flags >> 8;
  setup_data[USB_REQ_INDEX_L ] = (unsigned char)camIndex;
  setup_data[USB_REQ_INDEX_H ] = 0;
  setup_data[USB_REQ_LENGTH_L] = 0;
  setup_data[USB_REQ_LENGTH_H] = 0;
  int rc = usb_bulk_write(sxHandle, BULK_OUT, (char *)setup_data, 8, BULK_COMMAND_TIMEOUT);
  TRACE(fprintf(stderr, "   usb_bulk_write() -> %d\n", rc));
  TRACE(fprintf(stderr, "<- sxClearPixels %d\n", rc >= 0));
  return rc >= 0;
}

int sxLatchPixels(HANDLE sxHandle, unsigned short flags, unsigned short camIndex, unsigned short xoffset, unsigned short yoffset, unsigned short width, unsigned short height, unsigned short xbin, unsigned short ybin) {
  TRACE(fprintf(stderr, "-> sxLatchPixels(..., 0x%x, %d, %d, %d, %d, %d, %d, %d)\n", flags, camIndex, xoffset, yoffset, width, height, xbin, ybin));
  unsigned char setup_data[18];
  setup_data[USB_REQ_TYPE ] = USB_REQ_VENDOR | USB_REQ_DATAOUT;
  setup_data[USB_REQ ] = SXUSB_READ_PIXELS;
  setup_data[USB_REQ_VALUE_L ] = (unsigned char)flags;
  setup_data[USB_REQ_VALUE_H ] = flags >> 8;
  setup_data[USB_REQ_INDEX_L ] = (unsigned char)camIndex;
  setup_data[USB_REQ_INDEX_H ] = 0;
  setup_data[USB_REQ_LENGTH_L] = 10;
  setup_data[USB_REQ_LENGTH_H] = 0;
  setup_data[USB_REQ_DATA + 0] = xoffset & 0xFF;
  setup_data[USB_REQ_DATA + 1] = xoffset >> 8;
  setup_data[USB_REQ_DATA + 2] = yoffset & 0xFF;
  setup_data[USB_REQ_DATA + 3] = yoffset >> 8;
  setup_data[USB_REQ_DATA + 4] = width & 0xFF;
  setup_data[USB_REQ_DATA + 5] = width >> 8;
  setup_data[USB_REQ_DATA + 6] = height & 0xFF;
  setup_data[USB_REQ_DATA + 7] = height >> 8;
  setup_data[USB_REQ_DATA + 8] = (unsigned char)xbin;
  setup_data[USB_REQ_DATA + 9] = (unsigned char)ybin;
  int rc = usb_bulk_write(sxHandle, BULK_OUT, (char *)setup_data, 18, BULK_COMMAND_TIMEOUT);
  TRACE(fprintf(stderr, "   usb_bulk_write() -> %d\n", rc));
  TRACE(fprintf(stderr, "<- sxLatchPixels %d\n", rc >= 0));
  return rc >= 0;
}

int sxExposePixels(HANDLE sxHandle, unsigned short flags, unsigned short camIndex, unsigned short xoffset, unsigned short yoffset, unsigned short width, unsigned short height, unsigned short xbin, unsigned short ybin, unsigned long msec) {
  TRACE(fprintf(stderr, "-> sxExposePixels(..., 0x%x, %d, %d, %d, %d, %d, %d, %d, %lu)\n", flags, camIndex, xoffset, yoffset, width, height, xbin, ybin, msec));
  unsigned char setup_data[22];
  setup_data[USB_REQ_TYPE ] = USB_REQ_VENDOR | USB_REQ_DATAOUT;
  setup_data[USB_REQ ] = SXUSB_READ_PIXELS_DELAYED;
  setup_data[USB_REQ_VALUE_L ] = (unsigned char)flags;
  setup_data[USB_REQ_VALUE_H ] = flags >> 8;
  setup_data[USB_REQ_INDEX_L ] = (unsigned char)camIndex;
  setup_data[USB_REQ_INDEX_H ] = 0;
  setup_data[USB_REQ_LENGTH_L] = 14;
  setup_data[USB_REQ_LENGTH_H] = 0;
  setup_data[USB_REQ_DATA + 0] = xoffset & 0xFF;
  setup_data[USB_REQ_DATA + 1] = xoffset >> 8;
  setup_data[USB_REQ_DATA + 2] = yoffset & 0xFF;
  setup_data[USB_REQ_DATA + 3] = yoffset >> 8;
  setup_data[USB_REQ_DATA + 4] = width & 0xFF;
  setup_data[USB_REQ_DATA + 5] = width >> 8;
  setup_data[USB_REQ_DATA + 6] = height & 0xFF;
  setup_data[USB_REQ_DATA + 7] = height >> 8;
  setup_data[USB_REQ_DATA + 8] = (unsigned char)xbin;
  setup_data[USB_REQ_DATA + 9] = (unsigned char)ybin;
  setup_data[USB_REQ_DATA + 10] = (unsigned char)msec;
  setup_data[USB_REQ_DATA + 11] = (unsigned char)(msec >> 8);
  setup_data[USB_REQ_DATA + 12] = (unsigned char)(msec >> 16);
  setup_data[USB_REQ_DATA + 13] = (unsigned char)(msec >> 24);
  int rc = usb_bulk_write(sxHandle, BULK_OUT, (char *)setup_data, 22, BULK_COMMAND_TIMEOUT);
  TRACE(fprintf(stderr, "   usb_bulk_write() -> %d\n", rc));
  TRACE(fprintf(stderr, "<- sxExposePixels %d\n", rc >= 0));
  return rc >= 0;
}

int sxExposePixelsGated(HANDLE sxHandle, unsigned short flags, unsigned short camIndex, unsigned short xoffset, unsigned short yoffset, unsigned short width, unsigned short height, unsigned short xbin, unsigned short ybin, unsigned long msec) {
  TRACE(fprintf(stderr, "-> sxExposePixelsGated(..., 0x%x, %d, %d, %d, %d, %d, %d, %d, %lu)\n", flags, camIndex, xoffset, yoffset, width, height, xbin, ybin, msec));
  unsigned char setup_data[22];
  setup_data[USB_REQ_TYPE ] = USB_REQ_VENDOR | USB_REQ_DATAOUT;
  setup_data[USB_REQ ] = SXUSB_READ_PIXELS_GATED;
  setup_data[USB_REQ_VALUE_L ] = (unsigned char)flags;
  setup_data[USB_REQ_VALUE_H ] = flags >> 8;
  setup_data[USB_REQ_INDEX_L ] = (unsigned char)camIndex;
  setup_data[USB_REQ_INDEX_H ] = 0;
  setup_data[USB_REQ_LENGTH_L] = 14;
  setup_data[USB_REQ_LENGTH_H] = 0;
  setup_data[USB_REQ_DATA + 0] = xoffset & 0xFF;
  setup_data[USB_REQ_DATA + 1] = xoffset >> 8;
  setup_data[USB_REQ_DATA + 2] = yoffset & 0xFF;
  setup_data[USB_REQ_DATA + 3] = yoffset >> 8;
  setup_data[USB_REQ_DATA + 4] = width & 0xFF;
  setup_data[USB_REQ_DATA + 5] = width >> 8;
  setup_data[USB_REQ_DATA + 6] = height & 0xFF;
  setup_data[USB_REQ_DATA + 7] = height >> 8;
  setup_data[USB_REQ_DATA + 8] = (unsigned char)xbin;
  setup_data[USB_REQ_DATA + 9] = (unsigned char)ybin;
  setup_data[USB_REQ_DATA + 10] = (unsigned char)msec;
  setup_data[USB_REQ_DATA + 11] = (unsigned char)(msec >> 8);
  setup_data[USB_REQ_DATA + 12] = (unsigned char)(msec >> 16);
  setup_data[USB_REQ_DATA + 13] = (unsigned char)(msec >> 24);
  int rc = usb_bulk_write(sxHandle, BULK_OUT, (char *)setup_data, 22, BULK_COMMAND_TIMEOUT);
  TRACE(fprintf(stderr, "   usb_bulk_write() -> %d\n", rc));
  TRACE(fprintf(stderr, "<- sxExposePixelsGated %d\n", rc >= 0));
  return rc >= 0;
}

int sxReadPixels(HANDLE sxHandle, unsigned short *pixels, unsigned long count) {
  TRACE(fprintf(stderr, "-> sxReadPixels(..., %lu)\n", count));
  count *= 2;
  unsigned long read=0;
  int rc=0;
  while (read < count && rc >= 0) {
    TRACE(fprintf(stderr, "   %ld %ld\n", read, count));
    rc = usb_bulk_read(sxHandle, BULK_IN, ((char *)pixels) + read, count - read, BULK_DATA_TIMEOUT);
    TRACE(fprintf(stderr, "   usb_bulk_read() -> %d\n", rc));
    if (rc >= 0) {
      read+=rc;
      usleep(50);
    }
  }
  TRACE(fprintf(stderr, "<- sxReadPixels %d\n", rc >= 0));
  return rc >= 0;
}

int sxSetSTAR2000(HANDLE sxHandle, char star2k) {
  TRACE(fprintf(stderr, "-> sxSetSTAR2000(..., %d)\n", star2k));
  unsigned char setup_data[8];
  setup_data[USB_REQ_TYPE ] = USB_REQ_VENDOR | USB_REQ_DATAOUT;
  setup_data[USB_REQ ] = SXUSB_SET_STAR2K;
  setup_data[USB_REQ_VALUE_L ] = star2k;
  setup_data[USB_REQ_VALUE_H ] = 0;
  setup_data[USB_REQ_INDEX_L ] = 0;
  setup_data[USB_REQ_INDEX_H ] = 0;
  setup_data[USB_REQ_LENGTH_L] = 0;
  setup_data[USB_REQ_LENGTH_H] = 0;
  int rc = usb_bulk_write(sxHandle, BULK_OUT, (char *)setup_data, 8, BULK_COMMAND_TIMEOUT);
  TRACE(fprintf(stderr, "   usb_bulk_write() -> %d\n", rc));
  TRACE(fprintf(stderr, "<- sxSetSTAR2000 %d\n", rc >= 0));
  return rc >= 0;
}

int sxSetSerialPort(HANDLE sxHandle, unsigned short portIndex, unsigned short property, unsigned short value) {
  TRACE(fprintf(stderr, "-> sxSetSerialPort(..., %d, %d, %u)\n", portIndex, property, value));
  TRACE(fprintf(stderr, "<- sxSetSerialPort 1 (not implemented)\n"));
  return 1;
}

unsigned short sxGetSerialPort(HANDLE sxHandle, unsigned short portIndex, unsigned short property) {
  TRACE(fprintf(stderr, "-> sxGetSerialPort(..., %d, %d)\n", portIndex, property));
  unsigned char setup_data[8];
  setup_data[USB_REQ_TYPE ] = USB_REQ_VENDOR | USB_REQ_DATAIN;
  setup_data[USB_REQ ] = SXUSB_GET_SERIAL;
  setup_data[USB_REQ_VALUE_L ] = (unsigned char)property;
  setup_data[USB_REQ_VALUE_H ] = property >> 8;
  setup_data[USB_REQ_INDEX_L ] = (unsigned char)portIndex;
  setup_data[USB_REQ_INDEX_H ] = 0;
  setup_data[USB_REQ_LENGTH_L] = 2;
  setup_data[USB_REQ_LENGTH_H] = 0;
  int rc = usb_bulk_write(sxHandle, BULK_OUT, (char *)setup_data, 8, BULK_COMMAND_TIMEOUT);
  TRACE(fprintf(stderr, "   usb_bulk_write() -> %d\n", rc));
  if (rc == 8) {
    rc = usb_bulk_read(sxHandle, BULK_IN, (char *)setup_data, 2, BULK_COMMAND_TIMEOUT);
    TRACE(fprintf(stderr, "   usb_bulk_read() -> %d\n", rc));
    if (rc == 2) {
      int result=setup_data[0] | (setup_data[1] << 8);
      TRACE(fprintf(stderr, "<- sxGetSerialPort 0x%x\n", result));
      return result;
    }
  }
  TRACE(fprintf(stderr, "<- sxGetSerialPort 0\n"));
  return 0;
}

int sxWriteSerialPort(HANDLE sxHandle, unsigned short portIndex, unsigned short flush, unsigned short count, char *data) {
  TRACE(fprintf(stderr, "-> sxWriteSerialPort(..., %d, %d, %d, ...)\n", portIndex, flush, count));
  unsigned char setup_data[72];
  setup_data[USB_REQ_TYPE ] = USB_REQ_VENDOR | USB_REQ_DATAIN;
  setup_data[USB_REQ ] = SXUSB_WRITE_SERIAL_PORT;
  setup_data[USB_REQ_VALUE_L ] = (unsigned char)flush;
  setup_data[USB_REQ_VALUE_H ] = 0;
  setup_data[USB_REQ_INDEX_L ] = (unsigned char)portIndex;
  setup_data[USB_REQ_INDEX_H ] = 0;
  setup_data[USB_REQ_LENGTH_L] = (unsigned char)count;
  setup_data[USB_REQ_LENGTH_H] = 0;
  memcpy(setup_data + USB_REQ_DATA, data, count);
  int rc = usb_bulk_write(sxHandle, BULK_OUT, (char *)setup_data, USB_REQ_DATA + count, BULK_COMMAND_TIMEOUT);
  TRACE(fprintf(stderr, "   usb_bulk_write() -> %d\n", rc));
  TRACE(fprintf(stderr, "<- sxWriteSerialPort %d\n", rc >= 0));
  return rc >= 0;
}

int sxReadSerialPort(HANDLE sxHandle, unsigned short portIndex, unsigned short count, char *data) {
  TRACE(fprintf(stderr, "-> sxReadSerialPort(..., %d, %d, ...)\n", portIndex, count));
  unsigned char setup_data[8];
  setup_data[USB_REQ_TYPE ] = USB_REQ_VENDOR | USB_REQ_DATAIN;
  setup_data[USB_REQ ] = SXUSB_READ_SERIAL_PORT;
  setup_data[USB_REQ_VALUE_L ] = 0;
  setup_data[USB_REQ_VALUE_H ] = 0;
  setup_data[USB_REQ_INDEX_L ] = (unsigned char)portIndex;
  setup_data[USB_REQ_INDEX_H ] = 0;
  setup_data[USB_REQ_LENGTH_L] = (unsigned char)count;
  setup_data[USB_REQ_LENGTH_H] = 0;
  int rc = usb_bulk_write(sxHandle, BULK_OUT, (char *)setup_data, 8, BULK_COMMAND_TIMEOUT);
  TRACE(fprintf(stderr, "   usb_bulk_write() -> %d\n", rc));
  if (rc == 8) {
    rc = usb_bulk_read(sxHandle, BULK_IN, (char *)data, count, BULK_COMMAND_TIMEOUT);
    TRACE(fprintf(stderr, "   usb_bulk_read() -> %d\n", rc));
    TRACE(fprintf(stderr, "<- sxReadSerialPort %d\n", rc >= 0));
  }
  TRACE(fprintf(stderr, "<- sxReadSerialPort 0\n"));
  return 0;
}

int sxReadEEPROM(HANDLE sxHandle, unsigned short address, unsigned short count, char *data) {
  TRACE(fprintf(stderr, "-> sxReadEEPROM(..., %d, %d, ...)\n", address, count));
  unsigned char setup_data[8];
  setup_data[USB_REQ_TYPE ] = USB_REQ_VENDOR | USB_REQ_DATAIN;
  setup_data[USB_REQ ] = SXUSB_LOAD_EEPROM;
  setup_data[USB_REQ_VALUE_L ] = (unsigned char)address;
  setup_data[USB_REQ_VALUE_H ] = address >> 8;
  setup_data[USB_REQ_INDEX_L ] = 0;
  setup_data[USB_REQ_INDEX_H ] = 0;
  setup_data[USB_REQ_LENGTH_L] = (unsigned char)count;
  setup_data[USB_REQ_LENGTH_H] = 0;
  int rc = usb_bulk_write(sxHandle, BULK_OUT, (char *)setup_data, 8, BULK_COMMAND_TIMEOUT);
  TRACE(fprintf(stderr, "   usb_bulk_write() -> %d\n", rc));
  if (rc == 8) {
    rc = usb_bulk_read(sxHandle, BULK_IN, (char *)data, count, BULK_COMMAND_TIMEOUT);
    TRACE(fprintf(stderr, "   usb_bulk_read() -> %d\n", rc));
    TRACE(fprintf(stderr, "<- sxReadEEPROM %d\n", rc >= 0));
  }
  TRACE(fprintf(stderr, "<- sxReadEEPROM 0\n"));
  return 0;
}
