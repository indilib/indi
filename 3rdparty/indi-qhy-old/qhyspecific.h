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

#ifndef QHYSPECIFIC_H_
#define QHYSPECIFIC_H_

class QHY2 : public QHYDevice {
	public:
		QHY2(libusb_device *device);
		const char *getName() { return "QHY2"; }
};

class QHY2PRO : public QHYDevice {
	public:
		QHY2PRO(libusb_device *device);
		const char *getName() { return "QHY2PRO"; }
		bool hasCooler() { return true; }
		bool getParameters(unsigned* pixelCountX, unsigned* pixelCountY, float* pixelSizeX, float* pixelSizeY, unsigned *bitsPerPixel, unsigned* maxBinX, unsigned* maxBinY);
};

class QHY5 : public QHYDevice {
	public:
		QHY5(libusb_device *device);
		const char *getName() { return "QHY5"; }
		bool hasGuidePort() { return true; }
		bool getCCDTemp(float *temperature) { return false; }
		bool getParameters(unsigned* pixelCountX, unsigned* pixelCountY, float* pixelSizeX, float* pixelSizeY, unsigned *bitsPerPixel, unsigned* maxBinX, unsigned* maxBinY);
		bool setParameters(unsigned left, unsigned top, unsigned width, unsigned height, unsigned gain);
    bool startExposure(float time);
    bool readExposure(void *pixels);
		bool guidePulse(unsigned mask, unsigned duration);
    bool reset();
};

class QHY5II : public QHYDevice {
	public:
		QHY5II(libusb_device *device);
		const char *getName() { return "QHY5II"; }
		bool hasGuidePort() { return true; }
		bool getCCDTemp(float *temperature);
		bool getParameters(unsigned* pixelCountX, unsigned* pixelCountY, float* pixelSizeX, float* pixelSizeY, unsigned *bitsPerPixel, unsigned* maxBinX, unsigned* maxBinY);
		bool setParameters(unsigned left, unsigned top, unsigned width, unsigned height, unsigned gain);
    bool startExposure(float time);
};

class QHY6 : public QHYDevice {
	public:
		QHY6(libusb_device *device);
		const char *getName() { return "QHY6"; }
		bool hasGuidePort() { return true; }
		bool getParameters(unsigned* pixelCountX, unsigned* pixelCountY, float* pixelSizeX, float* pixelSizeY, unsigned *bitsPerPixel, unsigned* maxBinX, unsigned* maxBinY);
};

class QHY6PRO : public QHYDevice {
	public:
		QHY6PRO(libusb_device *device);
		const char *getName() { return "QHY6PRO"; }
		bool hasCooler() { return true; }
		bool getParameters(unsigned* pixelCountX, unsigned* pixelCountY, float* pixelSizeX, float* pixelSizeY, unsigned *bitsPerPixel, unsigned* maxBinX, unsigned* maxBinY);
};

class QHY7 : public QHYDevice {
	public:
		QHY7(libusb_device *device);
		const char *getName() { return "QHY7"; }
};

class QHY8 : public QHYDevice {
	public:
		QHY8(libusb_device *device);
		const char *getName() { return "QHY8"; }
		bool isOSC() { return true; }
		bool hasCooler() { return true; }
		bool getParameters(unsigned* pixelCountX, unsigned* pixelCountY, float* pixelSizeX, float* pixelSizeY, unsigned *bitsPerPixel, unsigned* maxBinX, unsigned* maxBinY);
};

class QHY8PRO : public QHYDevice {
	public:
		QHY8PRO(libusb_device *device);
		const char *getName() { return "QHY8PRO"; }
		bool isOSC() { return true; }
		bool hasCooler() { return true; }
		bool hasGuidePort() { return true; }
		bool getParameters(unsigned* pixelCountX, unsigned* pixelCountY, float* pixelSizeX, float* pixelSizeY, unsigned *bitsPerPixel, unsigned* maxBinX, unsigned* maxBinY);
};

class QHY8L : public QHYDevice {
	public:
		QHY8L(libusb_device *device);
		const char *getName() { return "QHY8L"; }
		bool hasCooler() { return true; }
		bool isOSC() { return true; }
		bool hasGuidePort() { return true; }
		bool getParameters(unsigned* pixelCountX, unsigned* pixelCountY, float* pixelSizeX, float* pixelSizeY, unsigned *bitsPerPixel, unsigned* maxBinX, unsigned* maxBinY);
};

class QHY8M : public QHYDevice {
	public:
		QHY8M(libusb_device *device);
		const char *getName() { return "QHY8M"; }
};

class QHY9 : public QHYDevice {
	public:
		QHY9(libusb_device *device);
		const char *getName() { return "QHY9"; }
		bool hasShutter() { return true; }
		bool getParameters(unsigned* pixelCountX, unsigned* pixelCountY, float* pixelSizeX, float* pixelSizeY, unsigned *bitsPerPixel, unsigned* maxBinX, unsigned* maxBinY);
};

class QHY9L : public QHYDevice {
	public:
		QHY9L(libusb_device *device);
		const char *getName() { return "QHY9L"; }
};

class QHY10 : public QHYDevice {
	public:
		QHY10(libusb_device *device);
		const char *getName() { return "QHY10"; }
		bool hasGuidePort() { return true; }
		bool getParameters(unsigned* pixelCountX, unsigned* pixelCountY, float* pixelSizeX, float* pixelSizeY, unsigned *bitsPerPixel, unsigned* maxBinX, unsigned* maxBinY);
};

class QHY11 : public QHYDevice {
	public:
		QHY11(libusb_device *device);
		const char *getName() { return "QHY11"; }
		bool getParameters(unsigned* pixelCountX, unsigned* pixelCountY, float* pixelSizeX, float* pixelSizeY, unsigned *bitsPerPixel, unsigned* maxBinX, unsigned* maxBinY);
};

class QHY12 : public QHYDevice {
	public:
		QHY12(libusb_device *device);
		const char *getName() { return "QHY12"; }
		bool hasGuidePort() { return true; }
		bool getParameters(unsigned* pixelCountX, unsigned* pixelCountY, float* pixelSizeX, float* pixelSizeY, unsigned *bitsPerPixel, unsigned* maxBinX, unsigned* maxBinY);
};

class QHY16 : public QHYDevice {
	public:
		QHY16(libusb_device *device);
		const char *getName() { return "QHY16"; }
};

class QHY20 : public QHYDevice {
	public:
		QHY20(libusb_device *device);
		const char *getName() { return "QHY20"; }
};

class QHY21 : public QHYDevice {
	public:
		QHY21(libusb_device *device);
		const char *getName() { return "QHY21"; }
		bool getParameters(unsigned* pixelCountX, unsigned* pixelCountY, float* pixelSizeX, float* pixelSizeY, unsigned *bitsPerPixel, unsigned* maxBinX, unsigned* maxBinY);
};

class QHY22 : public QHYDevice {
	public:
		QHY22(libusb_device *device);
		const char *getName() { return "QHY22"; }
		bool getParameters(unsigned* pixelCountX, unsigned* pixelCountY, float* pixelSizeX, float* pixelSizeY, unsigned *bitsPerPixel, unsigned* maxBinX, unsigned* maxBinY);
};

class QHY23 : public QHYDevice {
	public:
		QHY23(libusb_device *device);
		const char *getName() { return "QHY23"; }
		bool getParameters(unsigned* pixelCountX, unsigned* pixelCountY, float* pixelSizeX, float* pixelSizeY, unsigned *bitsPerPixel, unsigned* maxBinX, unsigned* maxBinY);
};


#endif
