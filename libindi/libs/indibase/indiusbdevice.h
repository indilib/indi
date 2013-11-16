/*******************************************************************************
 Copyright(c) 2011 Gerry Rozema. All rights reserved.
 
 Upgrade to libusb 1.0 by CloudMakers, s. r. o.
 Copyright(c) 2013 CloudMakers, s. r. o. All rights reserved.

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Library General Public
 License version 2 as published by the Free Software Foundation.

 This library is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 Library General Public License for more details.

 You should have received a copy of the GNU Library General Public License
 along with this library; see the file COPYING.LIB.  If not, write to
 the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 Boston, MA 02110-1301, USA.
*******************************************************************************/

#ifndef USBDEVICE_H
#define USBDEVICE_H

#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

#include <libusb-1.0/libusb.h>

#include "indibase.h"

/**
 * \class INDI::USBDevice
   \brief Class to provide general functionality of a generic USB device.

   Developers need to subclass INDI::USBDevice to implement any driver within INDI that requires direct read/write/control over USB.
*/
class INDI::USBDevice
{
protected:
	libusb_device *dev;
	libusb_device_handle *usb_handle;

	int ProductId;
	int VendorId;

	int OutputType;
	int OutputEndpoint;
	int InputType;
	int InputEndpoint;

	libusb_device *FindDevice(int,int,int);

public:
	int WriteInterrupt(unsigned char *, int, int);
	int ReadInterrupt(unsigned char *, int, int);
  int ControlMessage(unsigned char request_type, unsigned char request, unsigned int value, unsigned int index, unsigned char *data, unsigned char len);

  int WriteBulk(unsigned char *buf, int nbytes, int timeout);
  int ReadBulk(unsigned char *buf, int nbytes, int timeout);
	int FindEndpoints();
	int Open();
	void Close();
  USBDevice(void);
  USBDevice(libusb_device *dev);
  virtual ~USBDevice(void);
};

#endif // USBDEVICE_H
