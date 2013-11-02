/*
 Starlight Xpress CCD INDI Driver

 Code is based on SX SDK by David Schmenk and Craig Stark
 Copyright (c) 2003 David Schmenk
 All rights reserved.

 Changes for INDI project by Peter Polakovic
 Copyright (c) 2012-2013 Cloudmakers, s. r. o.
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
#include <stdarg.h>
#include <unistd.h>

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

#define BULK_COMMAND_TIMEOUT        1000
#define BULK_DATA_TIMEOUT           15000

#if 1
#define TRACE(c) (c)
#define DEBUG(c) (c)
#else
#define TRACE(c)
#define DEBUG(c)
#endif

static struct {
  int pid;
  const char *name;
  } SX_PIDS[] = {
    { 0x105, "SXVF-M5" },
    { 0x305, "SXVF-M5C" },
    { 0x107, "SXVF-M7" },
    { 0x307, "SXVF-M7C" },
    { 0x308, "SXVF-M8C" },
    { 0x109, "SXVF-M9" },
    { 0x325, "SXVR-M25C" },
    { 0x326, "SXVR-M26C" },
    { 0x115, "SXVR-H5" },
    { 0x119, "SXVR-H9" },
    { 0x319, "SXVR-H9C" },
    { 0x100, "SXVR-H9" },
    { 0x300, "SXVR-H9C" },
    { 0x126, "SXVR-H16" },
    { 0x128, "SXVR-H18" },
    { 0x135, "SXVR-H35" },
    { 0x136, "SXVR-H36" },
    { 0x194, "SXVR-H694" },
    { 0x394, "SXVR-H694C" },
    { 0x174, "SXVR-H674" },
    { 0x374, "SXVR-H674C" },
    { 0x507, "LodeStar" },
    { 0x517, "CoStar" },
    { 0x509, "SuperStar" },
    { 0x200, "MX Camera" },
    { 0, NULL }
  };
  
libusb_context *ctx = NULL;

void log(const char *fmt, ...) {
  va_list ap;
  va_start (ap, fmt);
  vfprintf (stderr, fmt, ap);
  va_end (ap);
}

static void init() {
  if (ctx == NULL) {
    int rc = libusb_init(&ctx);
    if (rc < 0) {
      DEBUG(log("init: libusb_init -> %s\n", rc < 0 ? libusb_error_name(rc) : "OK"));
      log("Can't initialize libusb\n");
    }
  }
}

#ifdef NO_ERROR_NAME
static char *libusb_error_name(int rc) {
  static char buffer[30];
  sprintf(buffer, "error %d", rc);
  return buffer;
}
#endif

bool sxIsInterlaced(short model) {
  bool interlaced = model & 0x40;
  if (model == 0x84)
    return true;
  model &= 0x1F;
  if (model == 0x16 || model == 0x17 || model == 0x18 || model == 0x19)
    return false;
  return interlaced;
}

bool sxIsColor(short model) {
  return model & 0x80;
}

int sxList(DEVICE *sxDevices, const char **names, int maxCount) {
  init();
  int count=0;
  libusb_device **usb_devices;
  struct libusb_device_descriptor descriptor;
  ssize_t total = libusb_get_device_list(ctx, &usb_devices);
  if (total < 0) {
    log("Can't get device list\n");
    return 0;
  }
  for (int i = 0; i < total && count < maxCount; i++) {
    libusb_device *device = usb_devices[i];
    if (!libusb_get_device_descriptor(device, &descriptor)) {
      if (descriptor.idVendor == SX_VID) {
        int pid = descriptor.idProduct;
        for (int i = 0; SX_PIDS[i].pid; i++) {
          if (pid == SX_PIDS[i].pid) {
            DEBUG(log("sxList: '%s' [0x%x, 0x%x] found\n", SX_PIDS[i].name, SX_VID, pid));
            names[count] = SX_PIDS[i].name;
            sxDevices[count++] = device;
            libusb_ref_device(device);
            break;
          }
        }
      } else if (descriptor.idVendor == SX_USB_VID && descriptor.idProduct == SX_USB_PID) {
        TRACE(log("sxList: '%s' [0x%x, 0x%x] found\n", SX_USB_NAME, SX_USB_VID, SX_USB_PID));
        names[count]=SX_USB_NAME;
        sxDevices[count++] = device;
        libusb_ref_device(device);
      }
    }
  }
   libusb_free_device_list(usb_devices, 1);
  return count;
}

int sxOpen(DEVICE sxDevice, HANDLE *sxHandle) {
  init();
  int rc = libusb_open(sxDevice, sxHandle);
  DEBUG(log("sxOpen: libusb_open -> %s\n", rc < 0 ? libusb_error_name(rc) : "OK"));
  if (rc >= 0) {
    if (libusb_kernel_driver_active(*sxHandle, 0) == 1) {
      rc = libusb_detach_kernel_driver(*sxHandle, 0);
      DEBUG(log("sxOpen: libusb_detach_kernel_driver -> %s\n", rc < 0 ? libusb_error_name(rc) : "OK"));
    }    
//    if (rc >= 0) {
//      rc = libusb_set_configuration(*sxHandle, 1);
//      DEBUG(log("sxOpen: libusb_set_configuration -> %s\n", rc < 0 ? libusb_error_name(rc) : "OK"));
//    }
    if (rc >= 0) {
#ifdef __APPLE__
      rc = libusb_claim_interface(*sxHandle, 0);
#else
      rc = libusb_claim_interface(*sxHandle, 1);
#endif
      DEBUG(log("sxOpen: libusb_claim_interface -> %s\n", rc < 0 ? libusb_error_name(rc) : "OK"));
    }
  }
  return rc >= 0;
}

int sxOpen(HANDLE *sxHandles) {
  init();
  DEVICE devices[20];
  const char* names[20];
  int count = sxList(devices, names, 20);
  int result = 0;
  int rc = 0;
  for (int i = 0; rc >=0 && i < count; i++) {
    HANDLE handle;
    rc = sxOpen(devices[i], &handle);
    if (rc >= 0) {
      sxHandles[result++] = handle;
    }
    if (rc < 0)
      return rc;
  }
  return result;
}

void sxClose(HANDLE *sxHandle) {
  int rc;
//  rc = libusb_release_interface(*sxHandle, 0);
//  DEBUG(log("sxClose: libusb_release_interface -> %s\n", rc < 0 ? libusb_error_name(rc) : "OK"));
  libusb_close(*sxHandle);
  *sxHandle = NULL;
  DEBUG(log("sxClose: libusb_close\n"));
}

int sxReset(HANDLE sxHandle) {
  unsigned char setup_data[8];
  int transferred;
  setup_data[USB_REQ_TYPE ] = USB_REQ_VENDOR | USB_REQ_DATAOUT;
  setup_data[USB_REQ ] = SXUSB_RESET;
  setup_data[USB_REQ_VALUE_L ] = 0;
  setup_data[USB_REQ_VALUE_H ] = 0;
  setup_data[USB_REQ_INDEX_L ] = 0;
  setup_data[USB_REQ_INDEX_H ] = 0;
  setup_data[USB_REQ_LENGTH_L] = 0;
  setup_data[USB_REQ_LENGTH_H] = 0;
  int rc = libusb_bulk_transfer(sxHandle, BULK_OUT, setup_data, 8, &transferred, BULK_COMMAND_TIMEOUT);
  DEBUG(log("sxReset: libusb_control_transfer -> %s\n", rc < 0 ? libusb_error_name(rc) : "OK"));
  usleep(1000);
  return rc >= 0;
}

unsigned short sxGetCameraModel(HANDLE sxHandle) {
  unsigned char setup_data[8];
  int transferred;
  setup_data[USB_REQ_TYPE ] = USB_REQ_VENDOR | USB_REQ_DATAIN;
  setup_data[USB_REQ ] = SXUSB_CAMERA_MODEL;
  setup_data[USB_REQ_VALUE_L ] = 0;
  setup_data[USB_REQ_VALUE_H ] = 0;
  setup_data[USB_REQ_INDEX_L ] = 0;
  setup_data[USB_REQ_INDEX_H ] = 0;
  setup_data[USB_REQ_LENGTH_L] = 2;
  setup_data[USB_REQ_LENGTH_H] = 0;
  int rc = libusb_bulk_transfer(sxHandle, BULK_OUT, setup_data, 8, &transferred, BULK_COMMAND_TIMEOUT);
  DEBUG(log("sxGetCameraModel: libusb_control_transfer -> %s\n", rc < 0 ? libusb_error_name(rc) : "OK"));
  if (transferred == 8) {
    rc = libusb_bulk_transfer(sxHandle, BULK_IN, setup_data, 2, &transferred, BULK_COMMAND_TIMEOUT);
    DEBUG(log("sxGetCameraModel: libusb_control_transfer -> %s\n", rc < 0 ? libusb_error_name(rc) : "OK"));
    if (transferred == 2) {
      int result=setup_data[0] | (setup_data[1] << 8);
      DEBUG(log("sxGetCameraModel: %s %s model %d\n", sxIsInterlaced(result) ? "INTERLACED" : "NON-INTERLACED", sxIsColor(result) ? "COLOR" : "MONO", result & 0x1F));
      return result;
    }
  }
  return 0;
}

unsigned long sxGetFirmwareVersion(HANDLE sxHandle) {
  unsigned char setup_data[8];
  int transferred;
  setup_data[USB_REQ_TYPE ] = USB_REQ_VENDOR | USB_REQ_DATAIN;
  setup_data[USB_REQ ] = SXUSB_GET_FIRMWARE_VERSION;
  setup_data[USB_REQ_VALUE_L ] = 0;
  setup_data[USB_REQ_VALUE_H ] = 0;
  setup_data[USB_REQ_INDEX_L ] = 0;
  setup_data[USB_REQ_INDEX_H ] = 0;
  setup_data[USB_REQ_LENGTH_L] = 4;
  setup_data[USB_REQ_LENGTH_H] = 0;
  int rc = libusb_bulk_transfer(sxHandle, BULK_OUT, setup_data, 8, &transferred, BULK_COMMAND_TIMEOUT);
  DEBUG(log("sxGetFirmwareVersion: libusb_control_transfer -> %s\n", rc < 0 ? libusb_error_name(rc) : "OK"));
  if (transferred == 8) {
    rc = libusb_bulk_transfer(sxHandle, BULK_IN, setup_data, 4, &transferred, BULK_COMMAND_TIMEOUT);
    DEBUG(log("sxGetFirmwareVersion: libusb_control_transfer -> %s\n", rc < 0 ? libusb_error_name(rc) : "OK"));
    if (transferred) {
      unsigned long result=((unsigned long)setup_data[0] | ((unsigned long)setup_data[1] << 8) | ((unsigned long)setup_data[2] << 16) | ((unsigned long)setup_data[3] << 24));
      return result;
    }
  }
  return 0;
}

unsigned short sxGetBuildNumber(HANDLE sxHandle) {
  unsigned char setup_data[8];
  int transferred;
  setup_data[USB_REQ_TYPE ] = USB_REQ_VENDOR | USB_REQ_DATAIN;
  setup_data[USB_REQ ] = SXUSB_BUILD_NUMBER;;
  setup_data[USB_REQ_VALUE_L ] = 0;
  setup_data[USB_REQ_VALUE_H ] = 0;
  setup_data[USB_REQ_INDEX_L ] = 0;
  setup_data[USB_REQ_INDEX_H ] = 0;
  setup_data[USB_REQ_LENGTH_L] = 4;
  setup_data[USB_REQ_LENGTH_H] = 0;
  int rc = libusb_bulk_transfer(sxHandle, BULK_OUT, setup_data, 8, &transferred, BULK_COMMAND_TIMEOUT);
  DEBUG(log("sxGetBuildNumber: libusb_control_transfer -> %s\n", rc < 0 ? libusb_error_name(rc) : "OK"));
  if (transferred == 8) {
    rc = libusb_bulk_transfer(sxHandle, BULK_IN, setup_data, 2, &transferred, BULK_COMMAND_TIMEOUT);
    DEBUG(log("sxGetBuildNumber: libusb_control_transfer -> %s\n", rc < 0 ? libusb_error_name(rc) : "OK"));
    if (transferred == 2) {
      int result=setup_data[0] | (setup_data[1] << 8);
      return result;
    }
  }
  return 0;
}

int sxGetCameraParams(HANDLE sxHandle, unsigned short camIndex, struct t_sxccd_params *params) {
  unsigned char setup_data[17];
  int transferred;
  setup_data[USB_REQ_TYPE ] = USB_REQ_VENDOR | USB_REQ_DATAIN;
  setup_data[USB_REQ ] = SXUSB_GET_CCD;
  setup_data[USB_REQ_VALUE_L ] = 0;
  setup_data[USB_REQ_VALUE_H ] = 0;
  setup_data[USB_REQ_INDEX_L ] = (char)camIndex;
  setup_data[USB_REQ_INDEX_H ] = 0;
  setup_data[USB_REQ_LENGTH_L] = 17;
  setup_data[USB_REQ_LENGTH_H] = 0;
  int rc = libusb_bulk_transfer(sxHandle, BULK_OUT, setup_data, 8, &transferred, BULK_COMMAND_TIMEOUT);
  DEBUG(log("sxGetCameraParams: libusb_control_transfer -> %s\n", rc < 0 ? libusb_error_name(rc) : "OK"));
  if (transferred == 8) {
    rc = libusb_bulk_transfer(sxHandle, BULK_IN, setup_data, 17, &transferred, BULK_COMMAND_TIMEOUT);
    DEBUG(log("sxGetCameraParams: libusb_control_transfer -> %s\n", rc < 0 ? libusb_error_name(rc) : "OK"));
    if (transferred == 17) {
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
      DEBUG(log("sxGetCameraParams: chip size: %d x %d x %d, pixel size: %4.2f x %4.2f, matrix type: %x\n", params->width, params->height, params->bits_per_pixel, params->pix_width, params->pix_height, params->color_matrix));
      DEBUG(log("sxGetCameraParams: capabilities:%s%s%s%s\n", (params->extra_caps & SXCCD_CAPS_GUIDER ? " GUIDER" : ""), (params->extra_caps & SXCCD_CAPS_STAR2K ? " STAR2K" : ""), (params->extra_caps & SXUSB_CAPS_COOLER ? " COOLER" : ""), (params->extra_caps & SXUSB_CAPS_SHUTTER ? " SHUTTER" : "")));
      DEBUG(log("sxGetCameraParams: serial ports: %d\n", params->num_serial_ports));
    }
  }
  return rc >= 0;
}

int sxSetShutter(HANDLE sxHandle, unsigned short state) {
  unsigned char setup_data[8];
  int transferred;
  setup_data[USB_REQ_TYPE ] = USB_REQ_VENDOR;
  setup_data[USB_REQ ] = SXUSB_SHUTTER;
  setup_data[USB_REQ_VALUE_L ] = 0;
  setup_data[USB_REQ_VALUE_H ] = state?128:64;
  setup_data[USB_REQ_INDEX_L ] = 0;
  setup_data[USB_REQ_INDEX_H ] = 0;
  setup_data[USB_REQ_LENGTH_L] = 0;
  setup_data[USB_REQ_LENGTH_H] = 0;
  int rc = libusb_bulk_transfer(sxHandle, BULK_OUT, setup_data, 8, &transferred, BULK_COMMAND_TIMEOUT);
  DEBUG(log("sxSetShutter: libusb_control_transfer -> %s\n", rc < 0 ? libusb_error_name(rc) : "OK"));
  if (transferred == 8) {
    rc = libusb_bulk_transfer(sxHandle, BULK_IN, setup_data, 2, &transferred, BULK_COMMAND_TIMEOUT);
    DEBUG(log("sxSetShutter: libusb_control_transfer -> %s\n", rc < 0 ? libusb_error_name(rc) : "OK"));
    if (transferred == 2) {
      int result=setup_data[0] | (setup_data[1] << 8);
      return result;
    }
  }
  return 0;
}

int sxSetTimer(HANDLE sxHandle, unsigned long msec) {
  unsigned char setup_data[12];
  int transferred;
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
  int rc = libusb_bulk_transfer(sxHandle, BULK_OUT, setup_data, 12, &transferred, BULK_COMMAND_TIMEOUT);
  DEBUG(log("sxSetTimer: libusb_control_transfer -> %s\n", rc < 0 ? libusb_error_name(rc) : "OK"));
  return rc >= 0;
}

unsigned long sxGetTimer(HANDLE sxHandle) {
  unsigned char setup_data[8];
  int transferred;
  setup_data[USB_REQ_TYPE ] = USB_REQ_VENDOR | USB_REQ_DATAIN;
  setup_data[USB_REQ ] = SXUSB_GET_TIMER;
  setup_data[USB_REQ_VALUE_L ] = 0;
  setup_data[USB_REQ_VALUE_H ] = 0;
  setup_data[USB_REQ_INDEX_L ] = 0;
  setup_data[USB_REQ_INDEX_H ] = 0;
  setup_data[USB_REQ_LENGTH_L] = 4;
  setup_data[USB_REQ_LENGTH_H] = 0;
  int rc = libusb_bulk_transfer(sxHandle, BULK_OUT, setup_data, 8, &transferred, BULK_COMMAND_TIMEOUT);
  DEBUG(log("sxGetTimer: libusb_control_transfer -> %s\n", rc < 0 ? libusb_error_name(rc) : "OK"));
  if (transferred == 8) {
    rc = libusb_bulk_transfer(sxHandle, BULK_IN, setup_data, 4, &transferred, BULK_COMMAND_TIMEOUT);
    DEBUG(log("sxGetTimer: libusb_control_transfer -> %s\n", rc < 0 ? libusb_error_name(rc) : "OK"));
    if (transferred == 4) {
      unsigned long result=((unsigned long)setup_data[0] | ((unsigned long)setup_data[1] << 8) | ((unsigned long)setup_data[2] << 16) | ((unsigned long)setup_data[3] << 24));
      return result;
    }
  }
  return 0;
}

int sxSetCooler(HANDLE sxHandle, unsigned char setStatus, unsigned short setTemp, unsigned char *retStatus, unsigned short *retTemp) {
  unsigned char setup_data[8];
  int transferred;
  setup_data[USB_REQ_TYPE ] = USB_REQ_VENDOR;
  setup_data[USB_REQ ] = SXUSB_COOLER;
  setup_data[USB_REQ_VALUE_L ] = setTemp & 0xFF;
  setup_data[USB_REQ_VALUE_H ] = (setTemp >> 8) & 0xFF;
  setup_data[USB_REQ_INDEX_L ] = setStatus ? 1 : 0;
  setup_data[USB_REQ_INDEX_H ] = 0;
  setup_data[USB_REQ_LENGTH_L] = 0;
  setup_data[USB_REQ_LENGTH_H] = 0;
  int rc = libusb_bulk_transfer(sxHandle, BULK_OUT, setup_data, 8, &transferred, BULK_COMMAND_TIMEOUT);
  DEBUG(log("sxSetCooler: libusb_control_transfer -> %s\n", rc < 0 ? libusb_error_name(rc) : "OK"));
  if (transferred == 8) {
    rc = libusb_bulk_transfer(sxHandle, BULK_IN, setup_data, 3, &transferred, BULK_COMMAND_TIMEOUT);
    DEBUG(log("sxSetCooler: libusb_control_transfer -> %s\n", rc < 0 ? libusb_error_name(rc) : "OK"));
    if (transferred) {
      *retTemp = (setup_data[1] * 256) + setup_data[0];
      if (setup_data[2]) {
        *retStatus = 1;
      }
      else {
        *retStatus=0;
      }
      DEBUG(log("sxSetCooler: status: %d -> %d\n", setStatus, *retStatus));
      DEBUG(log("sxSetCooler: temperature: %4.1f -> %4.1f\n", (setTemp-2730)/10.0, (*retTemp-2730)/10.0));
      return 1;
    }
  }
  return 0;
}

int sxClearPixels(HANDLE sxHandle, unsigned short flags, unsigned short camIndex) {
  unsigned char setup_data[8];
  int transferred;
  setup_data[USB_REQ_TYPE ] = USB_REQ_VENDOR | USB_REQ_DATAOUT;
  setup_data[USB_REQ ] = SXUSB_CLEAR_PIXELS;
  setup_data[USB_REQ_VALUE_L ] = (unsigned char)flags;
  setup_data[USB_REQ_VALUE_H ] = flags >> 8;
  setup_data[USB_REQ_INDEX_L ] = (unsigned char)camIndex;
  setup_data[USB_REQ_INDEX_H ] = 0;
  setup_data[USB_REQ_LENGTH_L] = 0;
  setup_data[USB_REQ_LENGTH_H] = 0;
  int rc = libusb_bulk_transfer(sxHandle, BULK_OUT, setup_data, 8, &transferred, BULK_COMMAND_TIMEOUT);
  DEBUG(log("sxClearPixels: libusb_control_transfer -> %s\n", rc < 0 ? libusb_error_name(rc) : "OK"));
  return rc >= 0;
}

int sxLatchPixels(HANDLE sxHandle, unsigned short flags, unsigned short camIndex, unsigned short xoffset, unsigned short yoffset, unsigned short width, unsigned short height, unsigned short xbin, unsigned short ybin) {
  unsigned char setup_data[18];
  int transferred;
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
  int rc = libusb_bulk_transfer(sxHandle, BULK_OUT, setup_data, 18, &transferred, BULK_COMMAND_TIMEOUT);
  DEBUG(log("sxLatchPixels: libusb_control_transfer -> %s\n", rc < 0 ? libusb_error_name(rc) : "OK"));
  return rc >= 0;
}

int sxExposePixels(HANDLE sxHandle, unsigned short flags, unsigned short camIndex, unsigned short xoffset, unsigned short yoffset, unsigned short width, unsigned short height, unsigned short xbin, unsigned short ybin, unsigned long msec) {
  unsigned char setup_data[22];
  int transferred;
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
  int rc = libusb_bulk_transfer(sxHandle, BULK_OUT, setup_data, 22, &transferred, BULK_COMMAND_TIMEOUT);
  DEBUG(log("sxExposePixels: libusb_control_transfer -> %s\n", rc < 0 ? libusb_error_name(rc) : "OK"));
  return rc >= 0;
}

int sxExposePixelsGated(HANDLE sxHandle, unsigned short flags, unsigned short camIndex, unsigned short xoffset, unsigned short yoffset, unsigned short width, unsigned short height, unsigned short xbin, unsigned short ybin, unsigned long msec) {
  unsigned char setup_data[22];
  int transferred;
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
  int rc = libusb_bulk_transfer(sxHandle, BULK_OUT, setup_data, 22, &transferred, BULK_COMMAND_TIMEOUT);
  DEBUG(log("sxExposePixelsGated: libusb_control_transfer -> %s\n", rc < 0 ? libusb_error_name(rc) : "OK"));
  return rc >= 0;
}

int sxReadPixels(HANDLE sxHandle, unsigned short *pixels, unsigned long count) {
  count *= 2;
  int transferred;
  unsigned long read=0;
  int rc=0;
  while (read < count && rc >= 0) {
    rc = libusb_bulk_transfer(sxHandle, BULK_IN, (unsigned char *)pixels + read, count - read, &transferred, BULK_DATA_TIMEOUT);
    DEBUG(log("sxReadPixels: libusb_control_transfer -> %s\n", rc < 0 ? libusb_error_name(rc) : "OK"));
    if (transferred >= 0) {
      read+=transferred;
      usleep(50);
    }
  }
  return rc >= 0;
}

int sxSetSTAR2000(HANDLE sxHandle, char star2k) {
  unsigned char setup_data[8];
  int transferred;
  setup_data[USB_REQ_TYPE ] = USB_REQ_VENDOR | USB_REQ_DATAOUT;
  setup_data[USB_REQ ] = SXUSB_SET_STAR2K;
  setup_data[USB_REQ_VALUE_L ] = star2k;
  setup_data[USB_REQ_VALUE_H ] = 0;
  setup_data[USB_REQ_INDEX_L ] = 0;
  setup_data[USB_REQ_INDEX_H ] = 0;
  setup_data[USB_REQ_LENGTH_L] = 0;
  setup_data[USB_REQ_LENGTH_H] = 0;
  int rc = libusb_bulk_transfer(sxHandle, BULK_OUT, setup_data, 8, &transferred, BULK_COMMAND_TIMEOUT);
  DEBUG(log("sxSetSTAR2000: libusb_control_transfer -> %s\n", rc < 0 ? libusb_error_name(rc) : "OK"));
  return rc >= 0;
}

int sxSetSerialPort(HANDLE sxHandle, unsigned short portIndex, unsigned short property, unsigned short value) {
  log("sxSetSerialPort is not implemented");
  return 0;
}

unsigned short sxGetSerialPort(HANDLE sxHandle, unsigned short portIndex, unsigned short property) {
  unsigned char setup_data[8];
  int transferred;
  setup_data[USB_REQ_TYPE ] = USB_REQ_VENDOR | USB_REQ_DATAIN;
  setup_data[USB_REQ ] = SXUSB_GET_SERIAL;
  setup_data[USB_REQ_VALUE_L ] = (unsigned char)property;
  setup_data[USB_REQ_VALUE_H ] = property >> 8;
  setup_data[USB_REQ_INDEX_L ] = (unsigned char)portIndex;
  setup_data[USB_REQ_INDEX_H ] = 0;
  setup_data[USB_REQ_LENGTH_L] = 2;
  setup_data[USB_REQ_LENGTH_H] = 0;
  int rc = libusb_bulk_transfer(sxHandle, BULK_OUT, setup_data, 8, &transferred, BULK_COMMAND_TIMEOUT);
  DEBUG(log("sxGetSerialPort: libusb_control_transfer -> %s\n", rc < 0 ? libusb_error_name(rc) : "OK"));
  if (transferred == 8) {
    rc = libusb_bulk_transfer(sxHandle, BULK_IN, setup_data, 2, &transferred, BULK_COMMAND_TIMEOUT);
    DEBUG(log("sxGetSerialPort: libusb_control_transfer -> %s\n", rc < 0 ? libusb_error_name(rc) : "OK"));
    if (transferred) {
      int result=setup_data[0] | (setup_data[1] << 8);
      return result;
    }
  }
  return 0;
}

int sxWriteSerialPort(HANDLE sxHandle, unsigned short portIndex, unsigned short flush, unsigned short count, char *data) {
  unsigned char setup_data[72];
  int transferred;
  setup_data[USB_REQ_TYPE ] = USB_REQ_VENDOR | USB_REQ_DATAIN;
  setup_data[USB_REQ ] = SXUSB_WRITE_SERIAL_PORT;
  setup_data[USB_REQ_VALUE_L ] = (unsigned char)flush;
  setup_data[USB_REQ_VALUE_H ] = 0;
  setup_data[USB_REQ_INDEX_L ] = (unsigned char)portIndex;
  setup_data[USB_REQ_INDEX_H ] = 0;
  setup_data[USB_REQ_LENGTH_L] = (unsigned char)count;
  setup_data[USB_REQ_LENGTH_H] = 0;
  memcpy(setup_data + USB_REQ_DATA, data, count);
  int rc = libusb_bulk_transfer(sxHandle, BULK_OUT, setup_data, USB_REQ_DATA + count, &transferred, BULK_COMMAND_TIMEOUT);
  DEBUG(log("sxWriteSerialPort: libusb_control_transfer -> %s\n", rc < 0 ? libusb_error_name(rc) : "OK"));
  return rc >= 0;
}

int sxReadSerialPort(HANDLE sxHandle, unsigned short portIndex, unsigned short count, char *data) {
  unsigned char setup_data[8];
  int transferred;
  setup_data[USB_REQ_TYPE ] = USB_REQ_VENDOR | USB_REQ_DATAIN;
  setup_data[USB_REQ_TYPE ] = USB_REQ_VENDOR | USB_REQ_DATAIN;
  setup_data[USB_REQ ] = SXUSB_READ_SERIAL_PORT;
  setup_data[USB_REQ_VALUE_L ] = 0;
  setup_data[USB_REQ_VALUE_H ] = 0;
  setup_data[USB_REQ_INDEX_L ] = (unsigned char)portIndex;
  setup_data[USB_REQ_INDEX_H ] = 0;
  setup_data[USB_REQ_LENGTH_L] = (unsigned char)count;
  setup_data[USB_REQ_LENGTH_H] = 0;
  int rc = libusb_bulk_transfer(sxHandle, BULK_OUT, setup_data, 8, &transferred, BULK_COMMAND_TIMEOUT);
  DEBUG(log("sxReadSerialPort: libusb_control_transfer -> %s\n", rc < 0 ? libusb_error_name(rc) : "OK"));
  if (transferred == 8) {
    rc = libusb_bulk_transfer(sxHandle, BULK_IN, (unsigned char *)data, count, &transferred, BULK_COMMAND_TIMEOUT);
    DEBUG(log("sxReadSerialPort: libusb_control_transfer -> %s\n", rc < 0 ? libusb_error_name(rc) : "OK"));
  }
  return rc >= 0;
}

int sxReadEEPROM(HANDLE sxHandle, unsigned short address, unsigned short count, char *data) {
  unsigned char setup_data[8];
  int transferred;
  setup_data[USB_REQ_TYPE ] = USB_REQ_VENDOR | USB_REQ_DATAIN;
  setup_data[USB_REQ_TYPE ] = USB_REQ_VENDOR | USB_REQ_DATAIN;
  setup_data[USB_REQ ] = SXUSB_LOAD_EEPROM;
  setup_data[USB_REQ_VALUE_L ] = (unsigned char)address;
  setup_data[USB_REQ_VALUE_H ] = address >> 8;
  setup_data[USB_REQ_INDEX_L ] = 0;
  setup_data[USB_REQ_INDEX_H ] = 0;
  setup_data[USB_REQ_LENGTH_L] = (unsigned char)count;
  setup_data[USB_REQ_LENGTH_H] = 0;
  int rc = libusb_bulk_transfer(sxHandle, BULK_OUT, setup_data, 8, &transferred, BULK_COMMAND_TIMEOUT);
  DEBUG(log("sxReadEEPROM: libusb_control_transfer -> %s\n", rc < 0 ? libusb_error_name(rc) : "OK"));
  if (transferred == 8) {
    rc = libusb_bulk_transfer(sxHandle, BULK_IN, (unsigned char *)data, count, &transferred, BULK_COMMAND_TIMEOUT);
    DEBUG(log("sxReadEEPROM: libusb_control_transfer -> %s\n", rc < 0 ? libusb_error_name(rc) : "OK"));
  }
  return rc >= 0;
}
