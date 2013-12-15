/*
 * QHY CCD INDI Driver
 *
 * Copyright (c) 2013 CloudMakers, s. r. o. All Rights Reserved.
 *
 * The code is based upon Linux library source developed by
 * QHYCCD Inc. It is provided by CloudMakers and contributors
 * "AS IS", without warranty of any kind.
 * 
 * Copyright (C) 2013 QHYCCD Inc.
 */

#include <stdio.h> 
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <libusb-1.0/libusb.h>

#include "qhygeneric.h"
#include "qhyspecific.h"

QHY2::QHY2(libusb_device *device) :
    QHYDevice(device) {
}

QHY2PRO::QHY2PRO(libusb_device *device) :
    QHYDevice(device) {
}

bool QHY2PRO::getParameters(unsigned* pixelCountX, unsigned* pixelCountY, float* pixelSizeX, float* pixelSizeY, unsigned *bitsPerPixel, unsigned* maxBinX, unsigned* maxBinY) {
  *pixelCountX = 1360;
  *pixelCountY = 1024;
  *pixelSizeX = 6.45;
  *pixelSizeY = 6.45;
  *bitsPerPixel = 16;
  *maxBinX = 4;
  *maxBinY = 4;
  return true;
}

QHY6::QHY6(libusb_device *device) :
    QHYDevice(device) {
}

bool QHY6::getParameters(unsigned* pixelCountX, unsigned* pixelCountY, float* pixelSizeX, float* pixelSizeY, unsigned *bitsPerPixel, unsigned* maxBinX, unsigned* maxBinY) {
  *pixelCountX = 752;
  *pixelCountY = 582;
  *pixelSizeX = 6.5;
  *pixelSizeY = 6.25;
  *bitsPerPixel = 16;
  *maxBinX = 1;
  *maxBinY = 1;
  return true;
}

QHY6PRO::QHY6PRO(libusb_device *device) :
    QHYDevice(device) {
}

bool QHY6PRO::getParameters(unsigned* pixelCountX, unsigned* pixelCountY, float* pixelSizeX, float* pixelSizeY, unsigned *bitsPerPixel, unsigned* maxBinX, unsigned* maxBinY) {
  *pixelCountX = 752;
  *pixelCountY = 582;
  *pixelSizeX = 8.6;
  *pixelSizeY = 8.3;
  *bitsPerPixel = 16;
  *maxBinX = 1;
  *maxBinY = 1;
  return true;
}

QHY7::QHY7(libusb_device *device) :
    QHYDevice(device) {
}

QHY8::QHY8(libusb_device *device) :
    QHYDevice(device) {
}

bool QHY8::getParameters(unsigned* pixelCountX, unsigned* pixelCountY, float* pixelSizeX, float* pixelSizeY, unsigned *bitsPerPixel, unsigned* maxBinX, unsigned* maxBinY) {
  *pixelCountX = 3032;
  *pixelCountY = 2016;
  *pixelSizeX = 7.8;
  *pixelSizeY = 7.8;
  *bitsPerPixel = 16;
  *maxBinX = 4;
  *maxBinY = 4;
  return true;
}

QHY8PRO::QHY8PRO(libusb_device *device) :
    QHYDevice(device) {
}

bool QHY8PRO::getParameters(unsigned* pixelCountX, unsigned* pixelCountY, float* pixelSizeX, float* pixelSizeY, unsigned *bitsPerPixel, unsigned* maxBinX, unsigned* maxBinY) {
  *pixelCountX = 3032;
  *pixelCountY = 2016;
  *pixelSizeX = 7.8;
  *pixelSizeY = 7.8;
  *bitsPerPixel = 16;
  *maxBinX = 4;
  *maxBinY = 4;
  return true;
}

QHY8L::QHY8L(libusb_device *device) :
    QHYDevice(device) {
}

bool QHY8L::getParameters(unsigned* pixelCountX, unsigned* pixelCountY, float* pixelSizeX, float* pixelSizeY, unsigned *bitsPerPixel, unsigned* maxBinX, unsigned* maxBinY) {
  *pixelCountX = 3032;
  *pixelCountY = 2016;
  *pixelSizeX = 7.8;
  *pixelSizeY = 7.8;
  *bitsPerPixel = 16;
  *maxBinX = 4;
  *maxBinY = 4;
  return true;
}

QHY8M::QHY8M(libusb_device *device) :
    QHYDevice(device) {
}

QHY9::QHY9(libusb_device *device) :
    QHYDevice(device) {
}

bool QHY9::getParameters(unsigned* pixelCountX, unsigned* pixelCountY, float* pixelSizeX, float* pixelSizeY, unsigned *bitsPerPixel, unsigned* maxBinX, unsigned* maxBinY) {
  *pixelCountX = 3358;
  *pixelCountY = 2536;
  *pixelSizeX = 5.4;
  *pixelSizeY = 5.4;
  *bitsPerPixel = 16;
  *maxBinX = 1;
  *maxBinY = 1;
  return true;
}

QHY9L::QHY9L(libusb_device *device) :
    QHYDevice(device) {
}

QHY10::QHY10(libusb_device *device) :
    QHYDevice(device) {
}

bool QHY10::getParameters(unsigned* pixelCountX, unsigned* pixelCountY, float* pixelSizeX, float* pixelSizeY, unsigned *bitsPerPixel, unsigned* maxBinX, unsigned* maxBinY) {
  *pixelCountX = 3900;
  *pixelCountY = 2616;
  *pixelSizeX = 6.05;
  *pixelSizeY = 6.05;
  *bitsPerPixel = 16;
  *maxBinX = 1;
  *maxBinY = 1;
  return true;
}

QHY11::QHY11(libusb_device *device) :
    QHYDevice(device) {
}

bool QHY11::getParameters(unsigned* pixelCountX, unsigned* pixelCountY, float* pixelSizeX, float* pixelSizeY, unsigned *bitsPerPixel, unsigned* maxBinX, unsigned* maxBinY) {
  *pixelCountX = 4032;
  *pixelCountY = 2688;
  *pixelSizeX = 9.0;
  *pixelSizeY = 9.0;
  *bitsPerPixel = 16;
  *maxBinX = 1;
  *maxBinY = 1;
  return true;
}

QHY12::QHY12(libusb_device *device) :
    QHYDevice(device) {
}

bool QHY12::getParameters(unsigned* pixelCountX, unsigned* pixelCountY, float* pixelSizeX, float* pixelSizeY, unsigned *bitsPerPixel, unsigned* maxBinX, unsigned* maxBinY) {
  *pixelCountX = 4610;
  *pixelCountY = 3080;
  *pixelSizeX = 5.12;
  *pixelSizeY = 5.12;
  *bitsPerPixel = 16;
  *maxBinX = 1;
  *maxBinY = 1;
  return true;
}

QHY16::QHY16(libusb_device *device) :
    QHYDevice(device) {
}

QHY20::QHY20(libusb_device *device) :
    QHYDevice(device) {
}

QHY21::QHY21(libusb_device *device) :
    QHYDevice(device) {
}

bool QHY21::getParameters(unsigned* pixelCountX, unsigned* pixelCountY, float* pixelSizeX, float* pixelSizeY, unsigned *bitsPerPixel, unsigned* maxBinX, unsigned* maxBinY) {
  *pixelCountX = 1940;
  *pixelCountY = 1460;
  *pixelSizeX = 4.54;
  *pixelSizeY = 4.54;
  *bitsPerPixel = 16;
  *maxBinX = 1;
  *maxBinY = 1;
  return true;
}

QHY22::QHY22(libusb_device *device) :
    QHYDevice(device) {
}

bool QHY22::getParameters(unsigned* pixelCountX, unsigned* pixelCountY, float* pixelSizeX, float* pixelSizeY, unsigned *bitsPerPixel, unsigned* maxBinX, unsigned* maxBinY) {
  *pixelCountX = 2758;
  *pixelCountY = 2208;
  *pixelSizeX = 4.54;
  *pixelSizeY = 4.54;
  *bitsPerPixel = 16;
  *maxBinX = 1;
  *maxBinY = 1;
  return true;
}

QHY23::QHY23(libusb_device *device) :
    QHYDevice(device) {
}

bool QHY23::getParameters(unsigned* pixelCountX, unsigned* pixelCountY, float* pixelSizeX, float* pixelSizeY, unsigned *bitsPerPixel, unsigned* maxBinX, unsigned* maxBinY) {
  *pixelCountX = 3388;
  *pixelCountY = 2712;
  *pixelSizeX = 3.69;
  *pixelSizeY = 3.69;
  *bitsPerPixel = 16;
  *maxBinX = 1;
  *maxBinY = 1;
  return true;
}

