/*
 QHY CCD INDI Driver

 Copyright (c) 2013 Cloudmakers, s. r. o.
 All Rights Reserved.

 This program is free software; you can redistribute it and/or modify it
 under the terms of the GNU General Public License as published by the Free
 Software Foundation; either version 2 of the License, or (at your option)
 any later version.

 This program is distributed in the hope that it will be useful, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 more details.

 You should have received a copy of the GNU General Public License along with
 this program; if not, write to the Free Software Foundation, Inc., 59
 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

 The full GNU General Public License is included in this distribution in the
 file called LICENSE.
 */

#include <stdio.h>
#include <memory.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <math.h>
#include <stdarg.h>

#include "qhygeneric.h"
#include "qhyspecific.h"

static struct uninitialized_cameras {
  int vid;
  int pid;
  const char *loader;
  const char *firmware;
} uninitialized_cameras[] = {
  { 0x1618, 0x0412, NULL, "QHY2.HEX" },
  { 0x16C0, 0x2970, NULL, "QHY2PRO.HEX" },
  { 0x1618, 0x0901, "QHY5LOADER.HEX", "QHY5.HEX" },
  { 0x1618, 0x1002, "QHY5LOADER.HEX", "QHY5.HEX" },
  { 0x0547, 0x1002, "QHY5LOADER.HEX", "QHY5.HEX" },
  { 0x16c0, 0x296a, "QHY5LOADER.HEX", "QHY5.HEX" },
  { 0x16c0, 0x0818, "QHY5LOADER.HEX", "QHY5.HEX" },
  { 0x16c0, 0x081a, "QHY5LOADER.HEX", "QHY5.HEX" },
  { 0x16c0, 0x296e, "QHY5LOADER.HEX", "QHY5.HEX" },
  { 0x16c0, 0x296c, "QHY5LOADER.HEX", "QHY5.HEX" },
  { 0x16c0, 0x2986, "QHY5LOADER.HEX", "QHY5.HEX" },
  { 0x1781, 0x0c7c, "QHY5LOADER.HEX", "QHY5.HEX" },
  { 0x1618, 0x0920, NULL, "QHY5II.HEX" },
  { 0x1618, 0x0259, NULL, "QHY6.HEX" },
  { 0x16C0, 0x2980, NULL, "QHY6PRO.HEX" },
  { 0x1618, 0x4022, NULL, "QHY7.HEX" },
  { 0x1618, 0x6000, NULL, "QHY8.HEX" },
  { 0x1618, 0x6002, NULL, "QHY8PRO.HEX" },
  { 0x1618, 0x6004, NULL, "QHY8L.HEX" },
  { 0x1618, 0x6006, NULL, "QHY8M.HEX" },
  { 0x1618, 0x8300, NULL, "QHY9.HEX" },
  { 0x1618, 0x8310, NULL, "QHY9L.HEX" },
  { 0x1618, 0x1000, NULL, "QHY10.HEX" },
  { 0x1618, 0x1110, NULL, "QHY11.HEX" },
  { 0x1618, 0x1600, NULL, "QHY16.HEX" },
  { 0x1618, 0x8050, NULL, "QHY20.HEX" },
  { 0x1618, 0x6740, NULL, "QHY21.HEX" },
  { 0x1618, 0x6940, NULL, "QHY22.HEX" },
  { 0x1618, 0x8140, NULL, "QHY23.HEX" },
  { 0, 0, NULL, NULL }
};

template <typename T> QHYDevice* _create(libusb_device *device) {
  return new T(device);
}

typedef QHYDevice* (*_constructor)(libusb_device *device);

static struct initialized_cameras {
  int vid;
  int pid;
  const char *name;
  _constructor constructor;
} initialized_cameras[] = {
  { 0x16C0, 0x081E, "QHY2", &_create<QHY2> },
  { 0x16C0, 0x2971, "QHY2PRO", &_create<QHY2PRO> },
  { 0x16C0, 0x296D, "QHY5", &_create<QHY5> },
  { 0x1618, 0x0921, "QHY5II", &_create<QHY5II> },
  { 0x16C0, 0x025A, "QHY6", &_create<QHY6> },
  { 0x16C0, 0x081D, "QHY6", &_create<QHY6> },
  { 0x16C0, 0x2981, "QHY6PRO", &_create<QHY6PRO> },
  { 0x1618, 0x4023, "QHY7", &_create<QHY7> },
  { 0x16C0, 0x2972, "QHY8", &_create<QHY8> },
  { 0x1618, 0x6001, "QHY8", &_create<QHY8> },
  { 0x1618, 0x6003, "QHY8PRO", &_create<QHY8PRO> },
  { 0x1618, 0x6005, "QHY8L", &_create<QHY8L> },
  { 0x1618, 0x6007, "QHY8M", &_create<QHY8M> },
  { 0x1618, 0x8301, "QHY9", &_create<QHY9> },
  { 0x1618, 0x8311, "QHY9L", &_create<QHY9L> },
  { 0x1618, 0x1001, "QHY10", &_create<QHY10> },
  { 0x1618, 0x1111, "QHY11", &_create<QHY11> },
  { 0x1618, 0x1601, "QHY16", &_create<QHY16> },
  { 0x1618, 0x8051, "QHY20", &_create<QHY20> },
  { 0x1618, 0x6741, "QHY21", &_create<QHY21> },
  { 0x1618, 0x6941, "QHY22", &_create<QHY22> },
  { 0x1618, 0x8141, "QHY23", &_create<QHY23> },
  { 0, 0, NULL, NULL }
};

libusb_context *ctx = NULL;

void Log (const char *fmt, ...) {
  va_list ap;
  va_start (ap, fmt);
  vfprintf (stderr, fmt, ap);
  va_end (ap);
}

static double RToDegree(double R) {
  double   T;
  double LNR;
  if (R>400)
    R=400;
  if (R<1)
    R=1;
  LNR=log(R);
  T=1/(0.002679+0.000291*LNR+LNR*LNR*LNR*4.28e-7);
  T=T-273.15;
  return T;
}

static double mVToDegree(double V) {
  double R;
  double T;
  R=33/(V/1000+1.625)-10;
  T=RToDegree(R);
  return T;
}

static int poke(libusb_device_handle *handle, unsigned short addr, unsigned char *data, unsigned length) {
  int rc;
  unsigned retry = 0;
  while ((rc = libusb_control_transfer(handle, LIBUSB_ENDPOINT_OUT | LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_RECIPIENT_DEVICE, 0xA0, addr, 0, data, length, 3000)) < 0 && retry < 5) {
    if (errno != ETIMEDOUT)
      break;
    retry += 1;
  }
  _DEBUG(Log("libusb_control_transfer (firmware write at 0x%04x) -> %s\n", addr, rc < 0 ? libusb_error_name(rc) : "OK" ));
  return rc;
}

static bool upload(libusb_device_handle *handle, const char *hex) {
  int rc;
  unsigned char stop = 1;
  unsigned char reset = 0;
  FILE *image;
  char path[FILENAME_MAX];
  sprintf(path, "firmware/%s", hex);
  image = fopen(path, "r");
  if (!image) {
    sprintf(path, "/lib/firmware/%s", hex);
    image = fopen(path, "r");
  }
  if (!image) {
    sprintf(path, "/usr/lib/firmware/%s", hex);
    image = fopen(path, "r");
  }
  if (!image) {
    sprintf(path, "/usr/local/lib/firmware/%s", hex);
    image = fopen(path, "r");
  }
  if (image) {
    rc = libusb_control_transfer(handle, LIBUSB_ENDPOINT_OUT | LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_RECIPIENT_DEVICE, 0xA0, 0xe600, 0, &stop, 1, 3000);
    _DEBUG(Log("libusb_control_transfer (Stop CPU) -> %s\n", rc < 0 ? libusb_error_name(rc) : "OK" ));
    if (rc == 1) {
      unsigned char data[1023];
      unsigned short data_addr = 0;
      unsigned data_len = 0;
      bool first_line = 1;
      for (;;) {
        char buf[512], *cp;
        char tmp, type;
        unsigned len;
        unsigned idx, off;
        cp = fgets(buf, sizeof buf, image);
        if (cp == 0) {
          Log("EOF without EOF record in %s\n", hex);
          break;
        }
        if (buf[0] == '#')
          continue;
        if (buf[0] != ':') {
          Log("Invalid ihex record in %s\n", hex);
          break;
        }
        cp = strchr(buf, '\n');
        if (cp)
          *cp = 0;
        tmp = buf[3];
        buf[3] = 0;
        len = strtoul(buf + 1, 0, 16);
        buf[3] = tmp;
        tmp = buf[7];
        buf[7] = 0;
        off = strtoul(buf + 3, 0, 16);
        buf[7] = tmp;
        if (first_line) {
          data_addr = off;
          first_line = 0;
        }
        tmp = buf[9];
        buf[9] = 0;
        type = strtoul(buf + 7, 0, 16);
        buf[9] = tmp;
        if (type == 1) {
          break;
        }
        if (type != 0) {
          Log("Unsupported record type %u in %s\n", type, hex);
          break;
        }
        if ((len * 2) + 11 > strlen(buf)) {
          Log("Record too short in %s\n", hex);
          break;
        }
        if (data_len != 0 && (off != (data_addr + data_len)  || (data_len + len) > sizeof data)) {
          rc = poke(handle, data_addr, data, data_len);
          if (rc < 0)
            break;
          data_addr = off;
          data_len = 0;
        }
        for (idx = 0, cp = buf + 9; idx < len; idx += 1, cp += 2) {
          tmp = cp[2];
          cp[2] = 0;
          data[data_len + idx] = strtoul(cp, 0, 16);
          cp[2] = tmp;
        }
        data_len += len;
      }
      if (rc >= 0 && data_len != 0) {
        poke(handle, data_addr, data, data_len);
      }
      rc = libusb_control_transfer(handle, LIBUSB_ENDPOINT_OUT | LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_RECIPIENT_DEVICE, 0xA0, 0xe600, 0, &reset, 1, 3000);
      _DEBUG(Log("libusb_control_transfer (Reset CPU) -> %s\n", rc < 0 ? libusb_error_name(rc) : "OK" ));
    }
    fclose(image);
  } else {
    Log("Can't open %s\n", hex);
    return 0;
  }
  return rc >= 0;
}

static bool initialize(libusb_device *device, int index) {
  _DEBUG(Log("VID: %d, PID: %d\n", uninitialized_cameras[index].vid, uninitialized_cameras[index].pid));
  int rc;
  libusb_device_handle *handle;
  rc = libusb_open(device, &handle);
  _DEBUG(Log("libusb_open -> %s\n", rc < 0 ? libusb_error_name(rc) : "OK" ));
  if (rc >= 0) {
    if (libusb_kernel_driver_active(handle, 0) == 1) {
      rc = libusb_detach_kernel_driver(handle, 0);
      _DEBUG(Log("libusb_detach_kernel_driver -> %s\n", rc < 0 ? libusb_error_name(rc) : "OK" ));
    }
    if (rc >= 0) {
      rc = libusb_claim_interface(handle, 0);
      _DEBUG(Log("libusb_claim_interface -> %s\n", rc < 0 ? libusb_error_name(rc) : "OK" ));
    }
    if (rc >= 0) {
      if (uninitialized_cameras[index].loader) {
        if (!upload(handle, uninitialized_cameras[index].loader)) {
          Log("Can't upload loader\n");
          return 0;
        }
        usleep(5*1000*1000);
      }
      upload(handle, uninitialized_cameras[index].firmware);
      rc = libusb_release_interface(handle, 0);
      _DEBUG(Log("libusb_release_interface -> %s\n", rc < 0 ? libusb_error_name(rc) : "OK" ));
      libusb_close(handle);
      _DEBUG(Log("libusb_close\n"));
    }
  }
  return rc >= 0;
}

QHYDevice::QHYDevice(libusb_device *device) {
  this->device = device;
  handle = NULL;
  left = 0;
  top = 0;
  width = 0;
  height = 0;
  bufferSize = 0;
  buffer = NULL;
}

// qhyccd_vTXD
bool QHYDevice::controlWrite(unsigned req, unsigned char* data, unsigned length) {
  int rc = libusb_control_transfer(handle, QHYCCD_REQUEST_WRITE, req, 0, 0, data, length, 0);
  _DEBUG(Log("libusb_control_transfer -> %s\n", rc < 0 ? libusb_error_name(rc) : "OK" ));
  return rc >= 0;
}

// qhyccd_vRXD
bool QHYDevice::controlRead(unsigned req, unsigned char* data, unsigned length) {
  int rc = libusb_control_transfer(handle, QHYCCD_REQUEST_READ, req, 0, 0, data, length, 0);
  _DEBUG(Log("libusb_control_transfer -> %s\n", rc < 0 ? libusb_error_name(rc) : "OK" ));
  return rc >= 0;
}

// qhyccd_iTXD
bool QHYDevice::write(unsigned char *data, unsigned length) {
  int length_transfered;
  int rc = libusb_bulk_transfer(handle, QHYCCD_INTERRUPT_WRITE_ENDPOINT, data, length, &length_transfered, 0);
  _DEBUG(Log("libusb_bulk_transfer -> %s\n", rc < 0 ? libusb_error_name(rc) : "OK" ));
  return rc >= 0;
}

// qhyccd_iRXD
bool QHYDevice::read(unsigned char *data, unsigned length) {
  int length_transfered;
  int rc = libusb_bulk_transfer(handle, QHYCCD_INTERRUPT_READ_ENDPOINT, data, length, &length_transfered, 0);
  _DEBUG(Log("libusb_bulk_transfer -> %s\n", rc < 0 ? libusb_error_name(rc) : "OK" ));
  return rc >= 0;
}

bool QHYDevice::i2cWrite(unsigned addr,unsigned short value) {
  unsigned char data[2];
  data[0] = (value & 0xff00) >> 8;
  data[1] = value & 0x00FF;
  int rc = libusb_control_transfer(handle, QHYCCD_REQUEST_WRITE, 0xbb, 0, addr, data, 2, 0);
  _DEBUG(Log("libusb_control_transfer -> %s\n", rc < 0 ? libusb_error_name(rc) : "OK" ));
  return rc >= 0;
}

bool QHYDevice::i2cRead(unsigned addr, unsigned short *value) {
  unsigned char data[2];
  int rc = libusb_control_transfer(handle, QHYCCD_REQUEST_READ, 0xb7, 0, addr, data, 2, 0);
  _DEBUG(Log("libusb_control_transfer -> %s\n", rc < 0 ? libusb_error_name(rc) : "OK" ));
  *value = data[0] * 256 + data[1];
  return rc >= 0;
}

QHYDevice::~QHYDevice() {
  if (handle)
    close();
  if (buffer != NULL)
    free(buffer);
  libusb_unref_device(device);
}

void QHYDevice::makeRules() {
  FILE *rules = fopen("99-qhyccd.rules", "w");
  fprintf(rules, "# 99-qhyccd.rules generated by CloudMakers QHY CCD INDI driver version %d.%d\n", VERSION_MAJOR, VERSION_MINOR);
  fprintf(rules, "\n# uninitialized devices\n\n");
  for (int i=0; uninitialized_cameras[i].vid; i++)
    fprintf(rules, "SUBSYSTEMS==\"usb\", ATTRS{idVendor}==\"%04x\", ATTRS{idProduct}==\"%04x\", MODE=\"0666\"\n", uninitialized_cameras[i].vid, uninitialized_cameras[i].pid);
  fprintf(rules, "\n# initialized devices\n\n");
  for (int i=0; initialized_cameras[i].vid; i++)
    fprintf(rules, "SUBSYSTEMS==\"usb\", ATTRS{idVendor}==\"%04x\", ATTRS{idProduct}==\"%04x\", MODE=\"0666\"\n", initialized_cameras[i].vid, initialized_cameras[i].pid);
  fclose(rules);
}

int QHYDevice::list(QHYDevice **cameras, int maxCount) {
  int rc;
  struct libusb_device_descriptor descriptor;
  if (ctx == NULL) {
    rc = libusb_init(&ctx);
    if (rc < 0) {
      _DEBUG(Log("libusb_init -> %s\n", rc < 0 ? libusb_error_name(rc) : "OK" ));
      Log("Can't initialize libusb\n");
      return 0;
    }
  }
  libusb_device **usb_devices;
  ssize_t total = libusb_get_device_list(ctx, &usb_devices);
  if (total < 0) {
    Log("Can't get device list\n");
    return 0;
  }
  int count = 0;
  for (int i = 0; i < total; i++) {
    libusb_device *device = usb_devices[i];
    for (int j = 0; uninitialized_cameras[j].vid; j++) {
      if (!libusb_get_device_descriptor(device, &descriptor) && descriptor.idVendor == uninitialized_cameras[j].vid && descriptor.idProduct == uninitialized_cameras[j].pid) {
         if (initialize(device, j)) {
           count++;
         }
      }
    }
  }
  libusb_free_device_list(usb_devices, 1);
  if (count>0) {
    usleep(5*1000*1000);
  }
  count = 0;
  total = libusb_get_device_list(ctx, &usb_devices);
  if (total < 0) {
    Log("Can't get device list\n");
    return 0;
  }
  for (int i = 0; i < total && count < maxCount; i++) {
    libusb_device *device = usb_devices[i];
    for (int j = 0; initialized_cameras[j].vid; j++) {
      if (!libusb_get_device_descriptor(device, &descriptor) && descriptor.idVendor == initialized_cameras[j].vid && descriptor.idProduct == initialized_cameras[j].pid) {
         cameras[count++] = initialized_cameras[j].constructor(device);
        libusb_ref_device(device);
      }
    }
  }
   libusb_free_device_list(usb_devices, 1);
  return count;
}

bool QHYDevice::open() {
  int rc = libusb_open(device, &handle);
  _DEBUG(Log("libusb_open -> %s\n", rc < 0 ? libusb_error_name(rc) : "OK" ));
  if (rc >= 0) {
    if (libusb_kernel_driver_active(handle, 0) == 1) {
      rc = libusb_detach_kernel_driver(handle, 0);
      _DEBUG(Log("libusb_detach_kernel_driver -> %s\n", rc < 0 ? libusb_error_name(rc) : "OK" ));
    }
    if (rc >= 0) {
      rc = libusb_set_configuration(handle, 1);
      _DEBUG(Log("libusb_set_configuration -> %s\n", rc < 0 ? libusb_error_name(rc) : "OK" ));
    }
    if (rc >= 0) {
      rc = libusb_claim_interface(handle, 0);
      _DEBUG(Log("libusb_claim_interface -> %s\n", rc < 0 ? libusb_error_name(rc) : "OK" ));
    }
  }
  return rc >= 0;
}

bool QHYDevice::getParameters(unsigned* pixelCountX, unsigned* pixelCountY, float* pixelSizeX, float* pixelSizeY, unsigned *bitsPerPixel, unsigned* maxBinX, unsigned* maxBinY) {
  return false;
}

bool QHYDevice::setParameters(unsigned left, unsigned top, unsigned width, unsigned height, unsigned gain) {
  return false;
}

bool QHYDevice::readEEPROM(unsigned address, unsigned char *data, unsigned length) {
  int rc = libusb_control_transfer(handle, QHYCCD_REQUEST_READ, 0xCA, 0, address, data,length, 0);
  return rc >= 0;
}

bool QHYDevice::getCCDTemp(float *temperature) {
  unsigned char data[4];
  if (read(data, 4)) {
    *temperature=mVToDegree(1.024*(signed short)(data[1]*256+data[2]));
    return true;
  }
  return false;
}

bool QHYDevice::setCooler(unsigned power, bool fan) {
  unsigned char data[3] = { 0x01, power, 0x00 };
  if (power == 0)
    data[2]=data[2] &~ 0x80;
   else
    data[2]=data[2] | 0x80;
  if (fan)
     data[2]=data[2] | 0x01;
  else
    data[2]=data[2] &~ 0x01;
  bool result=write(data, 3);
  return result;
}

bool QHYDevice::guidePulse(unsigned mask, unsigned duration) {
  return false;
}

bool QHYDevice::startExposure(float time) {
  return false;
}

bool QHYDevice::readExposure(void *pixels) {
  return false;
}

bool QHYDevice::reset() {
  return false;
}

void QHYDevice::close() {
  int rc = libusb_release_interface(handle, 0);
  _DEBUG(Log("libusb_release_interface -> %s\n", rc < 0 ? libusb_error_name(rc) : "OK" ));
  libusb_close(handle);
  handle = NULL;
  _DEBUG(Log("libusb_close\n"));
}

