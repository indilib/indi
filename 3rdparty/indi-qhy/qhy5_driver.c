/*******************************************************************************
  Copyright(c) 2009 Geoffrey Hausheer. All rights reserved.
  
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

#include <stdio.h>
#include <string.h>
#include <usb.h>

//To build standalone app:
//gcc -DQHY5_TEST -Wall -o qhy5 qhy5_driver.c -lusb
#include "qhy5_driver.h"

struct _qhy5_driver {
	usb_dev_handle *handle;
	int width;
	int height;
	int binw;
	int binh;
	int gain;
	int offw;
	int offh;
	int bpp;
	void *image;
	size_t imagesize;
};

#define STORE_WORD_BE(var, val) *(var) = ((val) >> 8) & 0xff; *((var) + 1) = (val) & 0xff
static int debug = 0;

#define dprintf if(debug) printf

usb_dev_handle *locate_device(unsigned int vid, unsigned int pid) 
{
	unsigned char located = 0;
	struct usb_bus *bus;
	struct usb_device *dev;
	usb_dev_handle *device_handle = 0;
 		
	usb_find_busses();
	usb_find_devices();
 
 	for (bus = usb_busses; bus; bus = bus->next)
	{
		for (dev = bus->devices; dev; dev = dev->next)	
		{
			if (dev->descriptor.idVendor == vid
				&& dev->descriptor.idProduct == pid)
			{
				located++;
				device_handle = usb_open(dev);
				dprintf("Device Found: %s \n", dev->filename);
				dprintf("Vendor ID 0x0%x\n",dev->descriptor.idVendor);
				dprintf("Product ID 0x0%x\n",dev->descriptor.idProduct);
			}
		}	
  }

  if (device_handle==0) return (0);
	int open_status = usb_set_configuration(device_handle,1);

	open_status = usb_claim_interface(device_handle,0);
	open_status = usb_set_altinterface(device_handle,0);
	return (device_handle);  	
}

int ctrl_msg(usb_dev_handle *handle, unsigned char request_type, unsigned char request, unsigned int value, unsigned int index, char *data, unsigned char len)
{
	int i, result;
	
	dprintf("Sending %s command 0x%02x, 0x%02x, 0x%04x, 0x%04x, %d bytes\n",
		(request_type & USB_ENDPOINT_IN) ? "recv" : "send",
		request_type, request, value, index, len);
	result =  usb_control_msg(handle, request_type,
				request, value, index, data, len, 5000);
	for(i = 0; i < len; i++) {
		dprintf(" %02x", (unsigned char)data[i]);
	}
	dprintf("\n");
	return result;
}

int qhy5_start_exposure(qhy5_driver *qhy5, unsigned int exposure)
{

	int index, value;
	char buffer[10] = "DEADEADEAD"; // for debug purposes
	index = exposure >> 16;
	value = exposure & 0xffff;
	
	usleep(20000);
	return ctrl_msg(qhy5->handle, 0xc2, 0x12, value, index, buffer, 2);
}

int qhy5_read_exposure(qhy5_driver *qhy5)
{
	int result;
	dprintf("Reading %08x bytes\n", (unsigned int)qhy5->imagesize);
	result = usb_bulk_read(qhy5->handle, 0x82, qhy5->image, qhy5->imagesize, 20000);
	if (result == qhy5->imagesize) {
		dprintf( "Bytes: %d\n", result);
/*		for (i = 0; i < result; i++) {
			if((i% 16) == 0) {
				dprintf("\n%06x:", i);
			}
			dprintf(" %02x", imd[i]);
		}
*/
	} else {
		printf("Failed to read image. Got: %d, expected %u\n", result, (unsigned int)qhy5->imagesize);
		return 1;
	}
	return 0;
}

int qhy5_timed_move(qhy5_driver *qhy5, int direction, int duration_msec)
{
	unsigned int ret;
	int duration[2] = {-1, -1};
	int cmd = 0x00;

	if (! (direction & (QHY_NORTH | QHY_SOUTH | QHY_EAST | QHY_WEST))) {
		fprintf(stderr, "No direction specified to qhy5_timed_move\n");
		return 1;
	}
	if (duration_msec == 0) {
		//cancel quiding
		if ((direction & (QHY_NORTH | QHY_SOUTH)) &&
		    (direction & (QHY_EAST | QHY_WEST)))
		{
			cmd = 0x18;
		} else if(direction & (QHY_NORTH | QHY_SOUTH)) {
			cmd = 0x21;
		} else {
			cmd = 0x22;
		}
		return ctrl_msg(qhy5->handle, 0xc2, cmd, 0, 0, (char *)&ret, sizeof(ret));
	}
	if (direction &= QHY_NORTH) {
		cmd |= 0x20;
		duration[1] = duration_msec;
	} else if (direction &= QHY_SOUTH) {
		cmd |= 0x40;
		duration[1] = duration_msec;
	}
	if (direction &= QHY_EAST) {
		cmd |= 0x10;
		duration[0] = duration_msec;
	} else if (direction &= QHY_WEST) {
		cmd |= 0x80;
		duration[0] = duration_msec;
	}
	return ctrl_msg(qhy5->handle, 0x42, cmd, 0, 0, (char *)&duration, sizeof(duration));
}

void *qhy5_get_row(qhy5_driver *qhy5, int row)
{
	return qhy5->image + 1558 * row + qhy5->offh + 20;
}

int qhy5_set_params(qhy5_driver *qhy5, int width, int height, int binw, int binh, int offw, int offh, int gain, int *pixw, int *pixh)
{
	char reg[19];
	int offset, value, index;
	int gain_val;

        height -=  height % 4;
	offset = (1048 - height) / 2;
	index = (1558 * (height + 26)) >> 16;
	value = (1558 * (height + 26)) & 0xffff;
	gain_val = gain * 0x6ff / 100;
	STORE_WORD_BE(reg + 0,  gain_val);
	STORE_WORD_BE(reg + 2,  gain_val);
	STORE_WORD_BE(reg + 4,  gain_val);
	STORE_WORD_BE(reg + 6,  gain_val);
	STORE_WORD_BE(reg + 8,  offset);
	STORE_WORD_BE(reg + 10, 0);
	STORE_WORD_BE(reg + 12, height - 1);
	STORE_WORD_BE(reg + 14, 0x0521);
	STORE_WORD_BE(reg + 16, height + 25);
	reg[18] = 0xcc;
	ctrl_msg(qhy5->handle, 0x42, 0x13, value, index, reg, 19);
	usleep(20000);
	ctrl_msg(qhy5->handle, 0x42, 0x14, 0x31a5, 0, reg, 0);
	usleep(10000);
	ctrl_msg(qhy5->handle, 0x42, 0x16, 0, 0, reg, 0);

	qhy5->width = width;
	qhy5->height = height;
	qhy5->binw = binw;
	qhy5->binh = binh;
	qhy5->offw = offw;
	qhy5->offh = offh;
	qhy5->gain = gain;
	qhy5->bpp = 1;

	if (pixw)
		*pixw = width;
	if (pixh)
		*pixh = height;

	if (qhy5->imagesize < 1558 * (height + 26) * qhy5->bpp) {
		qhy5->imagesize = 1558 * (height + 26) * qhy5->bpp;
		qhy5->image = realloc(qhy5->image, qhy5->imagesize);
	}
	return(0);
}

int qhy5_query_capabilities(qhy5_driver *qhy5, int *width, int *height, int *binw, int *binh, int *gain)
{
	*width = 1280;
	*height = 1024;
	*binw = 1;
	*binh = 1;
	*gain = 100;
	return 0;
}

int qhy5_close(qhy5_driver *qhy5)
{
	if(! qhy5)
		return 0;
	if(qhy5->image)
		free(qhy5->image);
	if(qhy5->handle)
		usb_close(qhy5->handle);
	free(qhy5);
	return 0;
}

qhy5_driver *qhy5_open()
{
	struct usb_dev_handle *handle;
	qhy5_driver *qhy5;
	usb_init();

 	if ((handle = locate_device(0x16c0, 0x296d))==0) 
	{
		printf("Could not open the QHY5 device\n");
		return NULL;
	}

	qhy5 = calloc(sizeof(qhy5_driver), 1);
	qhy5->handle = handle;
	return qhy5;
}

#ifdef QHY5_TEST
#include <getopt.h>

void show_help()
{
	printf("qhy5 [options]\n");
	printf("\t\t-x/--width <width>                specify width (default: 1280)\n");
	printf("\t\t-y/--height <height>              specify height (default: 1024)\n");
	printf("\t\t-g/--gain <gain>                  specify gain in percent (default 10)\n");
	printf("\t\t-e/--exposure <exposure>          specify exposure in msec (default: 100)\n");
	printf("\t\t-f/--file <filename>              specify filename to write to\n");
	printf("\t\t-c/--count <count>                specify how many sequential images to take\n");
	printf("\t\t-d/--debug                        enable debugging\n");
	printf("\t\t-h//-help                         show this message\n");
	exit(0);
}

void write_ppm(qhy5_driver *qhy5, int width, int height, char *filename)
{
	int row, col;
	FILE *h = fopen(filename, "w");
	fprintf(h, "P5\n");

	fprintf(h, "%d %d\n", width, height);

	fprintf(h, "255\n");

	for(row = 0; row < height; row++) {
		unsigned char *ptr = qhy5_get_row(qhy5, row);
		for(col = 0; col < width; col++)
			fprintf(h, "%c", *(ptr++));
	}
	fclose(h);
}

int main (int argc,char **argv)
{
	qhy5_driver *qhy5;
	char image_name[80];
	int i;
	int width = 1280;
	int height = 1024;
	int count = 0;
	unsigned int gain = 10;
	unsigned int exposure = 100;
	char basename[256] = "image.ppm";

	struct option long_options[] = {
		{"exposure" ,required_argument, NULL, 'e'},
		{"gain", required_argument, NULL, 'g'},
		{"width", required_argument, NULL, 'x'},
		{"height", required_argument, NULL, 'y'},
		{"debug", required_argument, NULL, 'd'},
		{"file", required_argument, NULL, 'f'},
		{"count", required_argument, NULL, 'c'},
		{"help", no_argument , NULL, 'h'},
		{0, 0, 0, 0}
	};

	while (1) {
		char c;
		c = getopt_long (argc, argv, 
                     "e:g:x:y:df:c:h",
                     long_options, NULL);
		if(c == EOF)
			break;
		switch(c) {
		case 'e':
			exposure = strtol(optarg, NULL, 0);
			break;
		case 'g':
			gain = strtol(optarg, NULL, 0);
			break;
		case 'x':
			width = strtol(optarg, NULL, 0);
			break;
		case 'y':
			height = strtol(optarg, NULL, 0);
			break;
		case 'f':
			strncpy(basename, optarg, 255);
			break;
		case 'd':
			debug = 1;
			break;
		case 'h':
			show_help();
			break;
		}
	}
	if(width > 1280 || width < 1) {
		printf("width must be between 1 and 1280\n");
		exit(1);
	}
	if(height > 1024 || height < 1) {
		printf("height must be between 1 and 1024\n");
		exit(1);
	}

	printf("Capturing %dx%d\n", width, height);
	printf("Exposing for %f sec at gain: %d%%\n", exposure / 1000.0, gain);

	if(! (qhy5 = qhy5_open())) {
		printf("Could not open the QHY5 device\n");
		return -1;
	}

	qhy5_set_params(qhy5, width, height, 1, 1, (1280 - width) / 2, (1024 - height) / 2, gain, NULL, NULL);
	// Trigger an initial exposure (we won't use this one)
	qhy5_start_exposure(qhy5, exposure);
	usleep(exposure * 1000);
	if(count == 0) {
		qhy5_start_exposure(qhy5, exposure);
		usleep(exposure * 1000);
		qhy5_read_exposure(qhy5);
		write_ppm(qhy5, width, height, basename);
	}
	for(i = 0; i < count; i++) {
		sprintf(image_name, "%s%d.ppm", basename, i);
		qhy5_start_exposure(qhy5, exposure);
		usleep(exposure * 1000);
		qhy5_read_exposure(qhy5);
		write_ppm(qhy5, width, height, image_name);
	}
	qhy5_close(qhy5);
	return 0;
}
#endif
