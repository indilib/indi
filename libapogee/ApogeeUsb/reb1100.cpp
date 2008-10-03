/***************************************************************************
              reb1100.cpp  -  REB1100 communication class
                             -------------------
    begin                : Thu Mar 27 2003
    copyright            : (C) 2003 by Igor Izyumin
    email                : igor@mlug.missouri.edu
 ***************************************************************************/

/***************************************************************************

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

    **********************************************************************/

#include "reb1100.h"

REB1100::REB1100(){
	struct usb_bus *bus;
	struct usb_device *dev;

	usb_init();

	usb_find_busses();
	usb_find_devices();

	char string[256];

	int found = 0;

	/* find ebook device */
	for(bus = usb_busses; bus && !found; bus = bus->next) {
		for(dev = bus->devices; dev && !found; dev = dev->next) {
			if (dev->descriptor.idVendor == 0x993 && dev->descriptor.idProduct == 0x1) {
				hDevice = usb_open(dev);
				cerr << "Found eBook. Attempting to open... ";
				found = 1;
				if (hDevice) {
//					if (!usb_get_string_simple(hDevice, dev->descriptor.iSerialNumber, string, sizeof(string))) throw DevOpenError();
					cerr << "Success.\n";
//					cerr << "Serial number: " << string << endl;
				}
				else throw DevOpenError();
			}
		}
	}
	if (!found) throw DevNotFoundError();

	if (!usb_set_configuration(hDevice, 0x0)) throw DevOpenError();
	if (!usb_claim_interface(hDevice, 0x1)) throw DevOpenError();

	memTarget = INTERNAL;
}

REB1100::~REB1100(){
	usb_release_interface(hDevice, 0x0);
	usb_close(hDevice);
}

void REB1100::getFile(string filename, string &data) {
	long flength = filename.length();
	char buf[4096];
	char zeros[4] = {0, 0, 0, 0};
	int ret;
	string request;
	// first four bytes are the length of the filename
	// (low-endian)
	char *byte = reinterpret_cast<char*>(&flength);
	request += *byte;
	byte++;
	request += *byte;
	byte++;
	request += *byte;
	byte++;
	request += *byte;
	// the rest is the filename
	request += filename;
	
	// send a USB control request to tell the device what file we want
	char *temp;
	temp = new char[request.length()];
	request.copy(temp, string::npos);
	ret = usb_control_msg(hDevice, 0x42, 0x01, 0x00, 0x00, temp, request.length(), 300);
	if (ret == -1) throw DevControlError();
	delete temp;
	temp = NULL;
	
	// read the return code
	ret = usb_control_msg(hDevice, 0xc2, 0x02, 0x00, 0x00, zeros, 4, 300);
	if (ret == -1) throw DevControlError();
	
	// read file from pipe
	do {
		ret = usb_bulk_read(hDevice, 2, buf, 4096, 1000);
		if (ret == -1) throw DevReadError();
		for(int i = 0; i < ret; i++) {
				data += buf[i];
		}
	}
	while(ret == 4096);
}
	
void REB1100::sendFile(string filename, string &data) {
	string prefix = "";
	if (memTarget == MEMCARD) { // prefix with \SM\ when sending to memory card
		prefix = "\\SM\\";
	}
	filename = prefix + filename;

	long flength = data.length();
	long fnlength = filename.length();
	
	// prepare initial request
	string request;
	
	// first four bytes are the length of the file
	// (low-endian)
	char *byte = reinterpret_cast<char*>(&flength);
	request += *byte;
	byte++;
	request += *byte;
	byte++;
	request += *byte;
	byte++;
	request += *byte;
	
	// next four bytes are the length of the filename
	// (low-endian)
	byte = reinterpret_cast<char*>(&fnlength);
	request += *byte;
	byte++;
	request += *byte;
	byte++;
	request += *byte;
	byte++;
	request += *byte;
	
	// append filename
	request += filename;
	
	// send message to device
	int ret;
	char *temp;
	temp = new char[request.length()];
	request.copy(temp, string::npos);
	ret = usb_control_msg(hDevice, 0x42, 0x00, 0x00, 0x00, temp, request.length(), 3000);
	delete temp;
	temp = NULL;
	if (ret == -1) throw DevControlError();
	
	// read from device and check for error
	char temp2[4] = {0, 0, 0, 0};
	ret = usb_control_msg(hDevice, 0xc2, 0x03, 0x00, 0x00, temp2, 4, 3000);
	if (ret == -1) throw DevControlError();
	if (temp2[0] || temp2[1] || temp2[2] || temp2[3]) throw DevControlError();
	
	// now start bulk writing to the device
	string buf;
	int n, offset = 0;
	char *temp3;
	do {
		buf = data.substr(offset, 4096);
		n = buf.length();
		if (buf.length() > 0) {
			temp3 = new char[buf.length()];
			buf.copy(temp3, string::npos);
//			cout << "Sending block (" << n << " bytes)\n";
			ret = usb_bulk_write(hDevice, 2, temp3, n, 3000);
			if (ret == -1) throw DevWriteError();
			delete temp3;
			temp3 = NULL;
			offset += 4096;
		}
	}
	while(offset + 1 < data.length());
	// send empty packet to signify end of file
	ret = usb_bulk_write(hDevice, 2, 0, 0, 3000);
	if (ret == -1) throw DevWriteError();
}

void REB1100::setTarget(bool target) {
	memTarget = target;
}

