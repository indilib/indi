/*
 QHY CCD INDI Driver

 Copyright (c) 2013 Cloudmakers, s. r. o.
 All Rights Reserved.

 Code is based on QHY INDI Driver by Jasem Mutlaq
 Copyright(c) 2012 Jasem Mutlaq. All rights reserved.

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
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <libusb-1.0/libusb.h>

#include "qhygeneric.h"
#include "qhyspecific.h"

static const int qhy5_gain_map[] = { 0x000, 0x004, 0x005, 0x006, 0x007, 0x008, 0x009, 0x00A, 0x00B, 0x00C, 0x00D, 0x00E, 0x00F, 0x010, 0x011, 0x012, 0x013, 0x014, 0x015, 0x016, 0x017, 0x018, 0x019, 0x01A, 0x01B, 0x01C, 0x01D, 0x01E, 0x01F, 0x051, 0x052, 0x053, 0x054, 0x055, 0x056, 0x057, 0x058, 0x059, 0x05A, 0x05B, 0x05C, 0x05D, 0x05E, 0x05F, 0x6CE, 0x6CF, 0x6D0, 0x6D1, 0x6D2, 0x6D3, 0x6D4, 0x6D5, 0x6D6, 0x6D7, 0x6D8, 0x6D9, 0x6DA, 0x6DB, 0x6DC, 0x6DD, 0x6DE, 0x6DF, 0x6E0, 0x6E1, 0x6E2, 0x6E3, 0x6E4, 0x6E5, 0x6E6, 0x6E7, 0x6FC, 0x6FD, 0x6FE, 0x6FF };

static const int qhy5_gain_map_size = (sizeof(qhy5_gain_map) / sizeof(int));

QHY5::QHY5(libusb_device *device) :
    QHYDevice(device) {
}

bool QHY5::getParameters(unsigned* pixelCountX, unsigned* pixelCountY, float* pixelSizeX, float* pixelSizeY, unsigned *bitsPerPixel, unsigned* maxBinX, unsigned* maxBinY) {
  *pixelCountX = 1280;
  *pixelCountY = 1024;
  *pixelSizeX = 5.2;
  *pixelSizeY = 5.2;
  *bitsPerPixel = 8;
  *maxBinX = 1;
  *maxBinY = 1;
  return true;
}

bool QHY5::setParameters(unsigned left, unsigned top, unsigned width, unsigned height, unsigned gain) {
  unsigned char reg[19];
  int offset, value, index;
  height -= height % 4;
  offset = (1048 - height) / 2;
  index = (1558 * (height + 26)) >> 16;
  value = (1558 * (height + 26)) & 0xffff;
  gain = qhy5_gain_map[(int) (0.5 + gain * qhy5_gain_map_size / 100.0)];
  STORE_WORD_BE(reg + 0, gain);
  STORE_WORD_BE(reg + 2, gain);
  STORE_WORD_BE(reg + 4, gain);
  STORE_WORD_BE(reg + 6, gain);
  STORE_WORD_BE(reg + 8, offset);
  STORE_WORD_BE(reg + 10, 0);
  STORE_WORD_BE(reg + 12, height - 1);
  STORE_WORD_BE(reg + 14, 0x0521);
  STORE_WORD_BE(reg + 16, height + 25);
  reg[18] = 0xcc;
  int newBufferSize = 1558 * (height + 26);
  if (bufferSize < newBufferSize) {
    buffer = (char *) realloc(buffer, bufferSize = newBufferSize);
    _DEBUG(Log("%d bytes allocated for internal buffer\n", bufferSize));
  }
  int rc = libusb_control_transfer(handle, 0x42, 0x13, value, index, reg, 19, 0);
  _DEBUG(Log("libusb_control_transfer -> %s\n", rc < 0 ? libusb_error_name(rc) : "OK"));
  if (rc >= 0) {
    usleep(20000);
    rc = libusb_control_transfer(handle, 0x42, 0x14, 0x31a5, 0, reg, 0, 0);
    _DEBUG(Log("libusb_control_transfer -> %s\n", rc < 0 ? libusb_error_name(rc) : "OK"));
  }
  if (rc >= 0) {
    usleep(10000);
    rc = libusb_control_transfer(handle, 0x42, 0x16, 0, 0, reg, 0, 0);
    _DEBUG(Log("libusb_control_transfer -> %s\n", rc < 0 ? libusb_error_name(rc) : "OK"));
  }
  this->left = left;
  this->top = top;
  this->width = width;
  this->height = height;
  return rc >= 0;
}

bool QHY5::startExposure(float time) {
  int index, value;
  unsigned char buffer[2];
  unsigned exposure = (unsigned) time;
  index = exposure >> 16;
  value = exposure & 0xffff;
  int rc = libusb_control_transfer(handle, 0xc2, 0x12, value, index, buffer, 2, 0);
  _DEBUG(Log("libusb_control_transfer -> %s\n", rc < 0 ? libusb_error_name(rc) : "OK"));
  return true;
}

bool QHY5::readExposure(void *pixels) {
  int transferred;
  int rc = libusb_bulk_transfer(handle, 0x82, (unsigned char *) buffer, bufferSize, &transferred, 5000);
  _DEBUG(Log("libusb_bulk_transfer -> %d %s\n", transferred, rc < 0 ? libusb_error_name(rc) : "OK"));
  for (int j = 0; j < height; j++)
    memcpy(((unsigned char *) pixels) + j * width, ((unsigned char *) buffer) + 1558 * (j + top) + 20, width);
  return true;
}

bool QHY5::guidePulse(unsigned mask, unsigned duration) {
  int rc;
  if (duration == 0) {
    if (mask & 0x00030000)
      rc = libusb_control_transfer(handle, 0xc2, 0x10, 0, 0x18, NULL, 0, 500);
    else if (mask & 0x00010000)
      rc = libusb_control_transfer(handle, 0xc2, 0x10, 0, 0x21, NULL, 0, 500);
    else
      rc = libusb_control_transfer(handle, 0xc2, 0x10, 0, 0x22, NULL, 0, 500);
  } else {
    int data[2] = { -1, -1 };
    unsigned cmd = mask & 0x000000FF;
    if (mask & 0x00010000) {
      data[0] = duration;
    }
    if (mask & 0x00020000) {
      data[1] = duration;
    }
    rc = libusb_control_transfer(handle, 0x42, 0x10, 0, cmd, (unsigned char *) data, sizeof(data), 500);
  }
  _DEBUG(Log("libusb_control_transfer -> %s\n", rc < 0 ? libusb_error_name(rc) : "OK"));
  return rc >= 0;
}

bool QHY5::reset() {
  unsigned char data = 0x00;
  int transferred;
  int rc = libusb_bulk_transfer(handle, 1, &data, 1, &transferred, 5000);
  _DEBUG(Log("libusb_bulk_transfer -> %s\n", rc < 0 ? libusb_error_name(rc) : "OK"));
  setParameters(0, 0, 1280, 1024, 100);
  return rc >= 0;
}
