/*
 * QHY5L-II CCD INDI Driver
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

//#define QHY_5II
#define QHY_5LII

#ifdef QHY_5II

const int GainTable[73] = {
	0x004, 0x005, 0x006, 0x007, 0x008, 0x009, 0x00A, 0x00B, 0x00C, 0x00D, 0x00E,
	0x00F, 0x010, 0x011, 0x012, 0x013, 0x014, 0x015, 0x016, 0x017, 0x018, 0x019,
	0x01A, 0x01B, 0x01C, 0x01D, 0x01E, 0x01F, 0x051, 0x052, 0x053, 0x054, 0x055,
	0x056, 0x057, 0x058, 0x059, 0x05A, 0x05B, 0x05C, 0x05D, 0x05E, 0x05F, 0x6CE,
	0x6CF, 0x6D0, 0x6D1, 0x6D2, 0x6D3, 0x6D4, 0x6D5, 0x6D6, 0x6D7, 0x6D8, 0x6D9,
	0x6DA, 0x6DB, 0x6DC, 0x6DD, 0x6DE, 0x6DF, 0x6E0, 0x6E1, 0x6E2, 0x6E3, 0x6E4,
	0x6E5, 0x6E6, 0x6E7, 0x6FC, 0x6FD, 0x6FE, 0x6FF
};
#endif

QHY5II::QHY5II(libusb_device *device) :
    QHYDevice(device) {
}

bool QHY5II::getParameters(unsigned* pixelCountX, unsigned* pixelCountY, float* pixelSizeX, float* pixelSizeY, unsigned *bitsPerPixel, unsigned* maxBinX, unsigned* maxBinY) {
#ifdef QHY_5LII
  *pixelCountX = 1280;
  *pixelCountY = 960;
  *pixelSizeX = 3.75;
  *pixelSizeY = 3.75;
  *bitsPerPixel = 8;
  *maxBinX = 1;
  *maxBinY = 1;
#endif
#ifdef QHY_5II
  *pixelCountX = 1280;
  *pixelCountY = 1024;
  *pixelSizeX = 5.2;
  *pixelSizeY = 5.2;
  *bitsPerPixel = 8;
  *maxBinX = 2;
  *maxBinY = 2;
#endif  
  return true;
}

bool QHY5II::setParameters(unsigned left, unsigned top, unsigned width, unsigned height, unsigned gain) {
  bool status = true;
// based on InitCamera()
  unsigned char buf[4];
  controlWrite(0xc1,buf,4);

// based on void SetSpeed(bool isHigh)
// based on void QHY5LII::SetSpeedQHY5LII(int i)
  buf[0] = 0;
  controlWrite(0xc8, buf, 1);

#ifdef QHY_5LII  
// based on void QHY5LII::InitQHY5LIIRegs(void)	
	status = status & i2cWrite(0x301A, 0x0001); 
	status = status & i2cWrite(0x301A, 0x10D8); 
	usleep(100000);
	status = status & i2cWrite(0x3088, 0x8000); 
	status = status & i2cWrite(0x3086, 0x0025); 
	status = status & i2cWrite(0x3086, 0x5050); 
	status = status & i2cWrite(0x3086, 0x2D26); 
	status = status & i2cWrite(0x3086, 0x0828); 
	status = status & i2cWrite(0x3086, 0x0D17); 
	status = status & i2cWrite(0x3086, 0x0926); 
	status = status & i2cWrite(0x3086, 0x0028); 
	status = status & i2cWrite(0x3086, 0x0526); 
	status = status & i2cWrite(0x3086, 0xA728); 
	status = status & i2cWrite(0x3086, 0x0725); 
	status = status & i2cWrite(0x3086, 0x8080); 
	status = status & i2cWrite(0x3086, 0x2925); 
	status = status & i2cWrite(0x3086, 0x0040); 
	status = status & i2cWrite(0x3086, 0x2702); 
	status = status & i2cWrite(0x3086, 0x1616); 
	status = status & i2cWrite(0x3086, 0x2706); 
	status = status & i2cWrite(0x3086, 0x1F17); 
	status = status & i2cWrite(0x3086, 0x3626); 
	status = status & i2cWrite(0x3086, 0xA617); 
	status = status & i2cWrite(0x3086, 0x0326); 
	status = status & i2cWrite(0x3086, 0xA417); 
	status = status & i2cWrite(0x3086, 0x1F28); 
	status = status & i2cWrite(0x3086, 0x0526); 
	status = status & i2cWrite(0x3086, 0x2028); 
	status = status & i2cWrite(0x3086, 0x0425); 
	status = status & i2cWrite(0x3086, 0x2020); 
	status = status & i2cWrite(0x3086, 0x2700); 
	status = status & i2cWrite(0x3086, 0x171D); 
	status = status & i2cWrite(0x3086, 0x2500); 
	status = status & i2cWrite(0x3086, 0x2017); 
	status = status & i2cWrite(0x3086, 0x1028); 
	status = status & i2cWrite(0x3086, 0x0519); 
	status = status & i2cWrite(0x3086, 0x1703); 
	status = status & i2cWrite(0x3086, 0x2706); 
	status = status & i2cWrite(0x3086, 0x1703); 
	status = status & i2cWrite(0x3086, 0x1741); 
	status = status & i2cWrite(0x3086, 0x2660); 
	status = status & i2cWrite(0x3086, 0x175A); 
	status = status & i2cWrite(0x3086, 0x2317); 
	status = status & i2cWrite(0x3086, 0x1122); 
	status = status & i2cWrite(0x3086, 0x1741); 
	status = status & i2cWrite(0x3086, 0x2500); 
	status = status & i2cWrite(0x3086, 0x9027); 
	status = status & i2cWrite(0x3086, 0x0026); 
	status = status & i2cWrite(0x3086, 0x1828); 
	status = status & i2cWrite(0x3086, 0x002E); 
	status = status & i2cWrite(0x3086, 0x2A28); 
	status = status & i2cWrite(0x3086, 0x081C); 
	status = status & i2cWrite(0x3086, 0x1470); 
	status = status & i2cWrite(0x3086, 0x7003); 
	status = status & i2cWrite(0x3086, 0x1470); 
	status = status & i2cWrite(0x3086, 0x7004); 
	status = status & i2cWrite(0x3086, 0x1470); 
	status = status & i2cWrite(0x3086, 0x7005); 
	status = status & i2cWrite(0x3086, 0x1470); 
	status = status & i2cWrite(0x3086, 0x7009); 
	status = status & i2cWrite(0x3086, 0x170C); 
	status = status & i2cWrite(0x3086, 0x0014); 
	status = status & i2cWrite(0x3086, 0x0020); 
	status = status & i2cWrite(0x3086, 0x0014); 
	status = status & i2cWrite(0x3086, 0x0050); 
	status = status & i2cWrite(0x3086, 0x0314); 
	status = status & i2cWrite(0x3086, 0x0020); 
	status = status & i2cWrite(0x3086, 0x0314); 
	status = status & i2cWrite(0x3086, 0x0050); 
	status = status & i2cWrite(0x3086, 0x0414); 
	status = status & i2cWrite(0x3086, 0x0020); 
	status = status & i2cWrite(0x3086, 0x0414); 
	status = status & i2cWrite(0x3086, 0x0050); 
	status = status & i2cWrite(0x3086, 0x0514); 
	status = status & i2cWrite(0x3086, 0x0020); 
	status = status & i2cWrite(0x3086, 0x2405); 
	status = status & i2cWrite(0x3086, 0x1400); 
	status = status & i2cWrite(0x3086, 0x5001); 
	status = status & i2cWrite(0x3086, 0x2550); 
	status = status & i2cWrite(0x3086, 0x502D); 
	status = status & i2cWrite(0x3086, 0x2608); 
	status = status & i2cWrite(0x3086, 0x280D); 
	status = status & i2cWrite(0x3086, 0x1709); 
	status = status & i2cWrite(0x3086, 0x2600); 
	status = status & i2cWrite(0x3086, 0x2805); 
	status = status & i2cWrite(0x3086, 0x26A7); 
	status = status & i2cWrite(0x3086, 0x2807); 
	status = status & i2cWrite(0x3086, 0x2580); 
	status = status & i2cWrite(0x3086, 0x8029); 
	status = status & i2cWrite(0x3086, 0x2500); 
	status = status & i2cWrite(0x3086, 0x4027); 
	status = status & i2cWrite(0x3086, 0x0216); 
	status = status & i2cWrite(0x3086, 0x1627); 
	status = status & i2cWrite(0x3086, 0x0620); 
	status = status & i2cWrite(0x3086, 0x1736); 
	status = status & i2cWrite(0x3086, 0x26A6); 
	status = status & i2cWrite(0x3086, 0x1703); 
	status = status & i2cWrite(0x3086, 0x26A4); 
	status = status & i2cWrite(0x3086, 0x171F); 
	status = status & i2cWrite(0x3086, 0x2805); 
	status = status & i2cWrite(0x3086, 0x2620); 
	status = status & i2cWrite(0x3086, 0x2804); 
	status = status & i2cWrite(0x3086, 0x2520); 
	status = status & i2cWrite(0x3086, 0x2027); 
	status = status & i2cWrite(0x3086, 0x0017); 
	status = status & i2cWrite(0x3086, 0x1D25); 
	status = status & i2cWrite(0x3086, 0x0020); 
	status = status & i2cWrite(0x3086, 0x1710); 
	status = status & i2cWrite(0x3086, 0x2805); 
	status = status & i2cWrite(0x3086, 0x1A17); 
	status = status & i2cWrite(0x3086, 0x0327); 
	status = status & i2cWrite(0x3086, 0x0617); 
	status = status & i2cWrite(0x3086, 0x0317); 
	status = status & i2cWrite(0x3086, 0x4126); 
	status = status & i2cWrite(0x3086, 0x6017); 
	status = status & i2cWrite(0x3086, 0xAE25); 
	status = status & i2cWrite(0x3086, 0x0090); 
	status = status & i2cWrite(0x3086, 0x2700); 
	status = status & i2cWrite(0x3086, 0x2618); 
	status = status & i2cWrite(0x3086, 0x2800); 
	status = status & i2cWrite(0x3086, 0x2E2A); 
	status = status & i2cWrite(0x3086, 0x2808); 
	status = status & i2cWrite(0x3086, 0x1D05); 
	status = status & i2cWrite(0x3086, 0x1470); 
	status = status & i2cWrite(0x3086, 0x7009); 
	status = status & i2cWrite(0x3086, 0x1720); 
	status = status & i2cWrite(0x3086, 0x1400); 
	status = status & i2cWrite(0x3086, 0x2024); 
	status = status & i2cWrite(0x3086, 0x1400); 
	status = status & i2cWrite(0x3086, 0x5002); 
	status = status & i2cWrite(0x3086, 0x2550); 
	status = status & i2cWrite(0x3086, 0x502D); 
	status = status & i2cWrite(0x3086, 0x2608); 
	status = status & i2cWrite(0x3086, 0x280D); 
	status = status & i2cWrite(0x3086, 0x1709); 
	status = status & i2cWrite(0x3086, 0x2600); 
	status = status & i2cWrite(0x3086, 0x2805); 
	status = status & i2cWrite(0x3086, 0x26A7); 
	status = status & i2cWrite(0x3086, 0x2807); 
	status = status & i2cWrite(0x3086, 0x2580); 
	status = status & i2cWrite(0x3086, 0x8029); 
	status = status & i2cWrite(0x3086, 0x2500); 
	status = status & i2cWrite(0x3086, 0x4027); 
	status = status & i2cWrite(0x3086, 0x0216); 
	status = status & i2cWrite(0x3086, 0x1627); 
	status = status & i2cWrite(0x3086, 0x0617); 
	status = status & i2cWrite(0x3086, 0x3626); 
	status = status & i2cWrite(0x3086, 0xA617); 
	status = status & i2cWrite(0x3086, 0x0326); 
	status = status & i2cWrite(0x3086, 0xA417); 
	status = status & i2cWrite(0x3086, 0x1F28); 
	status = status & i2cWrite(0x3086, 0x0526); 
	status = status & i2cWrite(0x3086, 0x2028); 
	status = status & i2cWrite(0x3086, 0x0425); 
	status = status & i2cWrite(0x3086, 0x2020); 
	status = status & i2cWrite(0x3086, 0x2700); 
	status = status & i2cWrite(0x3086, 0x171D); 
	status = status & i2cWrite(0x3086, 0x2500); 
	status = status & i2cWrite(0x3086, 0x2021); 
	status = status & i2cWrite(0x3086, 0x1710); 
	status = status & i2cWrite(0x3086, 0x2805); 
	status = status & i2cWrite(0x3086, 0x1B17); 
	status = status & i2cWrite(0x3086, 0x0327); 
	status = status & i2cWrite(0x3086, 0x0617); 
	status = status & i2cWrite(0x3086, 0x0317); 
	status = status & i2cWrite(0x3086, 0x4126); 
	status = status & i2cWrite(0x3086, 0x6017); 
	status = status & i2cWrite(0x3086, 0xAE25); 
	status = status & i2cWrite(0x3086, 0x0090); 
	status = status & i2cWrite(0x3086, 0x2700); 
	status = status & i2cWrite(0x3086, 0x2618); 
	status = status & i2cWrite(0x3086, 0x2800); 
	status = status & i2cWrite(0x3086, 0x2E2A); 
	status = status & i2cWrite(0x3086, 0x2808); 
	status = status & i2cWrite(0x3086, 0x1E17); 
	status = status & i2cWrite(0x3086, 0x0A05); 
	status = status & i2cWrite(0x3086, 0x1470); 
	status = status & i2cWrite(0x3086, 0x7009); 
	status = status & i2cWrite(0x3086, 0x1616); 
	status = status & i2cWrite(0x3086, 0x1616); 
	status = status & i2cWrite(0x3086, 0x1616); 
	status = status & i2cWrite(0x3086, 0x1616); 
	status = status & i2cWrite(0x3086, 0x1616); 
	status = status & i2cWrite(0x3086, 0x1616); 
	status = status & i2cWrite(0x3086, 0x1616); 
	status = status & i2cWrite(0x3086, 0x1616); 
	status = status & i2cWrite(0x3086, 0x1616); 
	status = status & i2cWrite(0x3086, 0x1616); 
	status = status & i2cWrite(0x3086, 0x1616); 
	status = status & i2cWrite(0x3086, 0x1616); 
	status = status & i2cWrite(0x3086, 0x1616); 
	status = status & i2cWrite(0x3086, 0x1616); 
	status = status & i2cWrite(0x3086, 0x1616); 
	status = status & i2cWrite(0x3086, 0x1616); 
	status = status & i2cWrite(0x3086, 0x1400); 
	status = status & i2cWrite(0x3086, 0x2024); 
	status = status & i2cWrite(0x3086, 0x1400); 
	status = status & i2cWrite(0x3086, 0x502B); 
	status = status & i2cWrite(0x3086, 0x302C); 
	status = status & i2cWrite(0x3086, 0x2C2C); 
	status = status & i2cWrite(0x3086, 0x2C00); 
	status = status & i2cWrite(0x3086, 0x0225); 
	status = status & i2cWrite(0x3086, 0x5050); 
	status = status & i2cWrite(0x3086, 0x2D26); 
	status = status & i2cWrite(0x3086, 0x0828); 
	status = status & i2cWrite(0x3086, 0x0D17); 
	status = status & i2cWrite(0x3086, 0x0926); 
	status = status & i2cWrite(0x3086, 0x0028); 
	status = status & i2cWrite(0x3086, 0x0526); 
	status = status & i2cWrite(0x3086, 0xA728); 
	status = status & i2cWrite(0x3086, 0x0725); 
	status = status & i2cWrite(0x3086, 0x8080); 
	status = status & i2cWrite(0x3086, 0x2917); 
	status = status & i2cWrite(0x3086, 0x0525); 
	status = status & i2cWrite(0x3086, 0x0040); 
	status = status & i2cWrite(0x3086, 0x2702); 
	status = status & i2cWrite(0x3086, 0x1616); 
	status = status & i2cWrite(0x3086, 0x2706); 
	status = status & i2cWrite(0x3086, 0x1736); 
	status = status & i2cWrite(0x3086, 0x26A6); 
	status = status & i2cWrite(0x3086, 0x1703); 
	status = status & i2cWrite(0x3086, 0x26A4); 
	status = status & i2cWrite(0x3086, 0x171F); 
	status = status & i2cWrite(0x3086, 0x2805); 
	status = status & i2cWrite(0x3086, 0x2620); 
	status = status & i2cWrite(0x3086, 0x2804); 
	status = status & i2cWrite(0x3086, 0x2520); 
	status = status & i2cWrite(0x3086, 0x2027); 
	status = status & i2cWrite(0x3086, 0x0017); 
	status = status & i2cWrite(0x3086, 0x1E25); 
	status = status & i2cWrite(0x3086, 0x0020); 
	status = status & i2cWrite(0x3086, 0x2117); 
	status = status & i2cWrite(0x3086, 0x1028); 
	status = status & i2cWrite(0x3086, 0x051B); 
	status = status & i2cWrite(0x3086, 0x1703); 
	status = status & i2cWrite(0x3086, 0x2706); 
	status = status & i2cWrite(0x3086, 0x1703); 
	status = status & i2cWrite(0x3086, 0x1747); 
	status = status & i2cWrite(0x3086, 0x2660); 
	status = status & i2cWrite(0x3086, 0x17AE); 
	status = status & i2cWrite(0x3086, 0x2500); 
	status = status & i2cWrite(0x3086, 0x9027); 
	status = status & i2cWrite(0x3086, 0x0026); 
	status = status & i2cWrite(0x3086, 0x1828); 
	status = status & i2cWrite(0x3086, 0x002E); 
	status = status & i2cWrite(0x3086, 0x2A28); 
	status = status & i2cWrite(0x3086, 0x081E); 
	status = status & i2cWrite(0x3086, 0x0831); 
	status = status & i2cWrite(0x3086, 0x1440); 
	status = status & i2cWrite(0x3086, 0x4014); 
	status = status & i2cWrite(0x3086, 0x2020); 
	status = status & i2cWrite(0x3086, 0x1410); 
	status = status & i2cWrite(0x3086, 0x1034); 
	status = status & i2cWrite(0x3086, 0x1400); 
	status = status & i2cWrite(0x3086, 0x1014); 
	status = status & i2cWrite(0x3086, 0x0020); 
	status = status & i2cWrite(0x3086, 0x1400); 
	status = status & i2cWrite(0x3086, 0x4013); 
	status = status & i2cWrite(0x3086, 0x1802); 
	status = status & i2cWrite(0x3086, 0x1470); 
	status = status & i2cWrite(0x3086, 0x7004); 
	status = status & i2cWrite(0x3086, 0x1470); 
	status = status & i2cWrite(0x3086, 0x7003); 
	status = status & i2cWrite(0x3086, 0x1470); 
	status = status & i2cWrite(0x3086, 0x7017); 
	status = status & i2cWrite(0x3086, 0x2002); 
	status = status & i2cWrite(0x3086, 0x1400); 
	status = status & i2cWrite(0x3086, 0x2002); 
	status = status & i2cWrite(0x3086, 0x1400); 
	status = status & i2cWrite(0x3086, 0x5004); 
	status = status & i2cWrite(0x3086, 0x1400); 
	status = status & i2cWrite(0x3086, 0x2004); 
	status = status & i2cWrite(0x3086, 0x1400); 
	status = status & i2cWrite(0x3086, 0x5022); 
	status = status & i2cWrite(0x3086, 0x0314); 
	status = status & i2cWrite(0x3086, 0x0020); 
	status = status & i2cWrite(0x3086, 0x0314); 
	status = status & i2cWrite(0x3086, 0x0050); 
	status = status & i2cWrite(0x3086, 0x2C2C); 
	status = status & i2cWrite(0x3086, 0x2C2C); 
	status = status & i2cWrite(0x309E, 0x018A); 
	status = status & i2cWrite(0x301A, 0x10D8); 
	status = status & i2cWrite(0x3082, 0x0029); 
	status = status & i2cWrite(0x301E, 0x00C8); 
	status = status & i2cWrite(0x3EDA, 0x0F03); 
	status = status & i2cWrite(0x3EDE, 0xC007); 
	status = status & i2cWrite(0x3ED8, 0x01EF); 
	status = status & i2cWrite(0x3EE2, 0xA46B); 
	status = status & i2cWrite(0x3EE0, 0x067D); 
	status = status & i2cWrite(0x3EDC, 0x0070); 
	status = status & i2cWrite(0x3044, 0x0404); 
	status = status & i2cWrite(0x3EE6, 0x4303); 
	status = status & i2cWrite(0x3EE4, 0xD208); 
	status = status & i2cWrite(0x3ED6, 0x00BD); 
	status = status & i2cWrite(0x3EE6, 0x8303); 
	status = status & i2cWrite(0x30E4, 0x6372); 
	status = status & i2cWrite(0x30E2, 0x7253); 
	status = status & i2cWrite(0x30E0, 0x5470); 
	status = status & i2cWrite(0x30E6, 0xC4CC); 
	status = status & i2cWrite(0x30E8, 0x8050); 
	usleep(200);
	status = status & i2cWrite(0x302A, 14); 
	status = status & i2cWrite(0x302C, 1); 
	status = status & i2cWrite(0x302E, 3); 
	status = status & i2cWrite(0x3030, 65); 
	status = status & i2cWrite(0x3082, 0x0029);
	status = status & i2cWrite(0x30B0, 0x5330);
	status = status & i2cWrite(0x305e, 0x00ff); 
	status = status & i2cWrite(0x3012, 0x0020);
	status = status & i2cWrite(0x3064, 0x1802);
#endif

#ifdef QHY_5LII
// based on double QHY5LII::setQHY5LREG_PLL(unsigned char clk)
  status = status & i2cWrite(0x302A, 14);
  status = status & i2cWrite(0x302C, 1);
  status = status & i2cWrite(0x302E, 3);
  status = status & i2cWrite(0x3030, 42);
  status = status & i2cWrite(0x3082, 0x0029);
  if (true) // ???
    status = status & i2cWrite(0x30B0, 0x5330);
  else
    status = status & i2cWrite(0x30B0, 0x1330);
  status = status & i2cWrite(0x305e, 0x00ff);
  status = status & i2cWrite(0x3012, 0x0020);
  status = status & i2cWrite(0x3064, 0x1802);
#endif

#ifdef QHY_5LII
// based on void QHY5LII::initQHY5LII_1280X960(void)
	int xstart = 4;
	int ystart = 4;
	int xsize = 1280 - 1;
	int ysize = 960 - 1;
	status = status & i2cWrite(0x3002, ystart); // y start
	status = status & i2cWrite(0x3004, xstart); // x start
	status = status & i2cWrite(0x3006, ystart + ysize); // y end
	status = status & i2cWrite(0x3008, xstart + xsize); // x end
	status = status & i2cWrite(0x300a, 990); // frame length
	status = status & i2cWrite(0x300c, 1650); // line  length
	status = status & i2cWrite(0x301A, 0x10DC); // RESET_REGISTER
#endif

#ifdef QHY_5II
// based on void QHY5II::initQHY5II_SXGA(void)
// based on void QHY5II::QHY5IISetResolution(int x, int y) and 
  int x = 1280
  int y = 1024;
	status = status & i2cWrite(0x09, 200);
	status = status & i2cWrite(0x01, 8 + (1024 - y) / 2); // y start
	status = status & i2cWrite(0x02, 16 + (1280 - x) / 2); // x start
	status = status & i2cWrite(0x03, (unsigned short)(y - 1)); // y size
	status = status & i2cWrite(0x04, (unsigned short)(x - 1)); // x size
	status = status & i2cWrite(0x22, 0x00); // normal bin
	status = status & i2cWrite(0x23, 0x00); // normal bin
#endif

#ifdef QHY_5II
// based on void SetUSBTraffic(int i)
status = status & i2cWrite(0x05, 0x0009 + 30*50);
#endif

#ifdef QHY_5LII
// based on void SetUSBTraffic(int i)
status = status & i2cWrite(0x300c, 1650 + 30*50);
#endif

#ifdef QHY_5II
// based on void QHY5II::SetQHY5IIGain(unsigned short gain)
  status = status & i2cWrite(0x35, GainTable[i]);
#endif

#ifdef QHY_5LII
// based on void QHY5LII::SetGainMonoQHY5LII(double gain)

	int Gain_Min = 0, Gain_Max = 0;
	Gain_Min = 0;
	Gain_Max = 796;
	gain = (Gain_Max - Gain_Min) * gain / 1000;
	gain = gain / 10; // range:0-39.8
	unsigned short REG30B0;
	if (true) // ???
		REG30B0 = 0X5330;
	else
		REG30B0 = 0X1330;
	unsigned short baseDGain;
	double C[8] = {10, 8, 5, 4, 2.5, 2, 1.25, 1};
	double S[8] = {0, 0, 0, 0, 0, 0, 0, 0};
	int A[8] = {0, 0, 0, 0, 0, 0, 0, 0};
	int B[8] = {0, 0, 0, 0, 0, 0, 0, 0};
	double Error[8];
  int i;
	for (i = 0; i < 8; i++) {
		S[i] = gain / C[i];
		A[i] = (int)(S[i]);
		B[i] = (int)((S[i] - A[i]) / 0.03125);
		if (A[i] > 7)
			A[i] = 10000; // 限制A的范围在1-3
		if (A[i] == 0)
			A[i] = 10000; // 限制A的范围在1-3
		Error[i] = abs(((double)(A[i])+(double)(B[i]) * 0.03125) * C[i] - gain);
	}
	double minValue;
	int minValuePosition;
	minValue = Error[0];
	minValuePosition = 0;
	for (i = 0; i < 8; i++) {
		if (minValue > Error[i]) {
			minValue = Error[i];
			minValuePosition = i;
		}
	}
	int AA, BB, CC;
	double DD;
	double EE;
	AA = A[minValuePosition];
	BB = B[minValuePosition];
	if (minValuePosition == 0) {
		CC = 8;
		DD = 1.25;
		status = status & i2cWrite(0x30B0, (REG30B0 &~0x0030) + 0x30);
		status = status & i2cWrite(0x3EE4, 0XD308);
	}
	if (minValuePosition == 1) {
		CC = 8;
		DD = 1;
		status = status & i2cWrite(0x30B0, (REG30B0 &~0x0030) + 0x30);
		status = status & i2cWrite(0x3EE4, 0XD208);
	}
	if (minValuePosition == 2) {
		CC = 4;
		DD = 1.25;
		status = status & i2cWrite(0x30B0, (REG30B0 &~0x0030) + 0x20);
		status = status & i2cWrite(0x3EE4, 0XD308);
	}
	if (minValuePosition == 3) {
		CC = 4;
		DD = 1;
		status = status & i2cWrite(0x30B0, (REG30B0 &~0x0030) + 0x20);
		status = status & i2cWrite(0x3EE4, 0XD208);
	}
	if (minValuePosition == 4) {
		CC = 2;
		DD = 1.25;
		status = status & i2cWrite(0x30B0, (REG30B0 &~0x0030) + 0x10);
		status = status & i2cWrite(0x3EE4, 0XD308);
	}
	if (minValuePosition == 5) {
		CC = 2;
		DD = 1;
		status = status & i2cWrite(0x30B0, (REG30B0 &~0x0030) + 0x10);
		status = status & i2cWrite(0x3EE4, 0XD208);
	}
	if (minValuePosition == 6) {
		CC = 1;
		DD = 1.25;
		status = status & i2cWrite(0x30B0, (REG30B0 &~0x0030) + 0x00);
		status = status & i2cWrite(0x3EE4, 0XD308);
	}
	if (minValuePosition == 7) {
		CC = 1;
		DD = 1;
		status = status & i2cWrite(0x30B0, (REG30B0 &~0x0030) + 0x00);
		status = status & i2cWrite(0x3EE4, 0XD208);
	}
	EE = abs(((double)(AA)+(double)(BB) * 0.03125) * CC * DD - gain);
	baseDGain = BB + AA * 32;
	status = status & i2cWrite(0x305E, baseDGain);
#endif

  this->left = left;
  this->top = top;
  this->width = width;
  this->height = height;
  return status;
}

bool QHY5II::startExposure(float time) {
#ifdef QHY_5II

#endif

#ifdef QHY_5LII

#endif
}

bool QHY5II::getCCDTemp(float *temperature) {
	unsigned short sensed, calib1, calib2;
	i2cWrite(0x30B4, 0x0011);
	calib1 = i2cRead(0x30C6, &calib1);
	calib2 = i2cRead(0x30C8, &calib2);
	i2cWrite(0x30B4, 0x0000);
	i2cRead(0x30B2, &sensed);
	double slope = (70.0 - 55.0)/(calib1 - calib2);
	double T0 = (slope * calib1 - 70.0);
	*temperature = slope * sensed - T0;
	return true;
}