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
#include "indiusbdevice.h"

#include <string.h>

INDI::USBDevice::USBDevice()
{
	dev=NULL;
	usb_handle=NULL;
	OutputEndpoint=0;
	InputEndpoint=0;

	usb_init();
	usb_find_busses();
	usb_find_devices();
}


INDI::USBDevice::~USBDevice()
{
}

struct usb_device * INDI::USBDevice::FindDevice(int vendor, int product, int searchindex)
{
    struct usb_device *dev;
    struct usb_bus *usb_bus;
    int index=0;

    for(usb_bus=usb_busses; usb_bus; usb_bus=usb_bus->next) {
        for(dev=usb_bus->devices; dev; dev=dev->next) {
            if(dev->descriptor.idVendor==vendor) {
                if(dev->descriptor.idProduct==product) {
                    if(index==searchindex) {
                        fprintf(stderr,"Device has %d configurations\n",dev->descriptor.bNumConfigurations);
                        return dev;
                    }
                    else index++;
                }
            }
        }
    }
    return NULL;

}

int INDI::USBDevice::Open()
{
	if(dev==NULL) return -1;

	usb_handle=usb_open(dev);
	if(usb_handle != NULL) {
		//printf("Opened ok\n");

		return FindEndpoints();
		//return 0;
	}
	return -1;
}

int INDI::USBDevice::FindEndpoints()
{

	int rc=0;
	struct usb_interface_descriptor *intf;



	intf=&dev->config[0].interface[0].altsetting[0];
	for(int i=0; i<intf->bNumEndpoints; i++) {
		fprintf(stderr,"%04x %04x\n",
			   intf->endpoint[i].bEndpointAddress,
			   intf->endpoint[i].bmAttributes
			   );

		int dir;
		int addr;
		addr=intf->endpoint[i].bEndpointAddress;
		addr = addr & (USB_ENDPOINT_DIR_MASK^0xffff);
		//printf("%02x ",addr);

		int attr;
		int tp;
		attr=intf->endpoint[i].bmAttributes;
		tp=attr&USB_ENDPOINT_TYPE_MASK;
		//if(tp==USB_ENDPOINT_TYPE_BULK) printf("Bulk  ");
		//if(tp==USB_ENDPOINT_TYPE_INTERRUPT) printf("Interrupt ");



		dir=intf->endpoint[i].bEndpointAddress;
		dir=dir&USB_ENDPOINT_DIR_MASK;
		if(dir==USB_ENDPOINT_IN) {
			//printf("Input  ");
			fprintf(stderr,"Got an input endpoint\n");
			InputEndpoint=addr;
			InputType=tp;
		}
		if(dir==USB_ENDPOINT_OUT) {
			//printf("Output ");
			fprintf(stderr,"got an output endpoint\n");
			OutputEndpoint=addr;
			OutputType=tp;
		}
		//printf("\n");
	}

	//printf("claim interface returns %d\n",rc);
	return rc;

}

int INDI::USBDevice::ReadInterrupt(char *buf,int c,int timeout)
{
	int rc;

	rc=usb_interrupt_read(usb_handle,InputEndpoint,buf,c,timeout);
	//rc=usb_bulk_read(usb_handle,InputEndpoint,buf,c,timeout);
	return rc;

}

int INDI::USBDevice::WriteInterrupt(char *buf,int c,int timeout)
{
	int rc;

	//printf("Writing %02x to endpoint %d\n",buf[0],OutputEndpoint);
	rc=usb_interrupt_write(usb_handle,OutputEndpoint,buf,c,timeout);
	return rc;

}

int INDI::USBDevice::ReadBulk(char *buf,int c,int timeout)
{
	int rc;

	//rc=usb_interrupt_read(usb_handle,InputEndpoint,buf,c,timeout);
	rc=usb_bulk_read(usb_handle,InputEndpoint,buf,c,timeout);
	return rc;

}

int INDI::USBDevice::WriteBulk(char *buf,int c,int timeout)
{
	int rc;

	//printf("Writing %02x to endpoint %d\n",buf[0],OutputEndpoint);
	//rc=usb_interrupt_write(usb_handle,OutputEndpoint,buf,c,timeout);
	rc=usb_bulk_write(usb_handle,OutputEndpoint,buf,c,timeout);
	return rc;

}

int INDI::USBDevice::ControlMessage()
{
    char buf[3];
    int rc;

    buf[0]=0;
    buf[1]=0;
    buf[2]=0;

    rc=usb_control_msg(usb_handle,0xc2&USB_ENDPOINT_IN, 0x12,0,0,buf,3,1000);
    fprintf(stderr,"UsbControlMessage returns %d\n",rc);
    return 0;
}
