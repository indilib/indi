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

#ifndef QHY_GENERIC_H_
#define QHY_GENERIC_H_

#include <libusb-1.0/libusb.h>

#include "qhyconfig.h"

#define GUIDE_EAST             0x00010010     /* RA+ */
#define GUIDE_NORTH            0x00020020     /* DEC+ */
#define GUIDE_SOUTH            0x00020040     /* DEC- */
#define GUIDE_WEST             0x00010080     /* RA- */

#define STORE_WORD_BE(var, val) *(var) = ((val) >> 8) & 0xff; *((var) + 1) = (val) & 0xff

enum qhyccd_request_type { QHYCCD_REQUEST_READ = 0xC0, QHYCCD_REQUEST_WRITE = 0x40 };

enum qhyccd_endpoint_type { QHYCCD_INTERRUPT_READ_ENDPOINT = 0x81, QHYCCD_INTERRUPT_WRITE_ENDPOINT = 0x01, QHYCCD_DATA_READ_ENDPOINT = 0x82 };

#define _DEBUG(c) (c)

extern void Log (const char *fmt, ...);

class QHYDevice {
  protected:
    libusb_device * device;
    libusb_device_handle *handle;
    unsigned left, top, width, height;
    unsigned bufferSize;
    void *buffer;

    QHYDevice(libusb_device *device);
    bool controlWrite(unsigned req, unsigned char* data, unsigned length);
    bool controlRead(unsigned req, unsigned char* data, unsigned length);
    bool write(unsigned char *data, unsigned length);
    bool read(unsigned char *data, unsigned length);
    bool i2cWrite(unsigned addr,unsigned short value);
    bool i2cRead(unsigned addr, unsigned short *value);

	  ~QHYDevice();
	public:
		static void makeRules();
		static int list(QHYDevice **cameras, int maxCount);

    virtual const char *getName() = 0;

    virtual bool open();
    virtual bool isOSC() { return false; }
    virtual bool hasCooler() { return false; }
    virtual bool hasShutter() { return false; }
    virtual bool hasGuidePort() { return false; }
    virtual bool getParameters(unsigned* pixelCountX, unsigned* pixelCountY, float* pixelSizeX, float* pixelSizeY, unsigned *bitsPerPixel, unsigned* maxBinX, unsigned* maxBinY);
    virtual bool setParameters(unsigned left, unsigned top, unsigned width, unsigned height, unsigned gain);
    virtual bool readEEPROM(unsigned address, unsigned char *data, unsigned length);
    virtual bool getCCDTemp(float *temperature);
    virtual bool setCooler(unsigned power, bool fan);
    virtual bool guidePulse(unsigned mask, unsigned duration);
    virtual bool startExposure(float time);
    virtual bool readExposure(void *pixels);
    virtual bool reset();
    virtual void close();
};
#endif
