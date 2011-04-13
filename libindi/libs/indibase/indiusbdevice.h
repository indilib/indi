/*******************************************************************************
  Copyright(c) 2010 Gerry Rozema. All rights reserved.

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
*******************************************************************************/

#ifndef USBDEVICE_H
#define USBDEVICE_H

#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

#include <usb.h>

#include "indibase.h"

class INDI::USBDevice
{
protected:
	struct usb_device *dev;
	struct usb_dev_handle *usb_handle;

	int ProductId;
	int VendorId;

	int OutputType;
	int OutputEndpoint;
	int InputType;
	int InputEndpoint;

	struct usb_device * FindDevice(int,int,int);

public:
	int WriteInterrupt(char *,int,int);
	int ReadInterrupt(char *,int,int);

	int ControlMessage();

	int WriteBulk(char *,int,int);
	int ReadBulk(char *,int,int);
	int FindEndpoints();
	int Open();
        USBDevice(void);
        USBDevice(struct usb_device *);
        virtual ~USBDevice(void);
};

#endif // USBDEVICE_H
