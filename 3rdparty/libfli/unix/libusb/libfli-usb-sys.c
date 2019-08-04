/*

  Copyright (c) 2002 Finger Lakes Instrumentation (FLI), L.L.C.
  All rights reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions
  are met:

        Redistributions of source code must retain the above copyright
        notice, this list of conditions and the following disclaimer.

        Redistributions in binary form must reproduce the above
        copyright notice, this list of conditions and the following
        disclaimer in the documentation and/or other materials
        provided with the distribution.

        Neither the name of Finger Lakes Instrumentation (FLI), LLC
        nor the names of its contributors may be used to endorse or
        promote products derived from this software without specific
        prior written permission.

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
  FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
  REGENTS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
  BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
  LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
  ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
  POSSIBILITY OF SUCH DAMAGE.

  ======================================================================

  Finger Lakes Instrumentation, L.L.C. (FLI)
  web: http://www.fli-cam.com
  email: support@fli-cam.com

*/

#include <unistd.h>
#include <stdio.h>
#include <libusb-1.0/libusb.h>

#include <errno.h>

#include "libfli-libfli.h"
#include "libfli-sys.h"
#include "libfli-mem.h"
#include "libfli-usb.h"

#define FLIUSB_MIN_TIMEOUT (5000)

libusb_device_handle * libusb_fli_find_handle(struct libusb_context *usb_ctx, char *name);

long libusb_usb_connect(flidev_t dev, fli_unixio_t *io, char *name)
{
  int r;
  libusb_device *usb_dev;
  libusb_device_handle *usb_han;
  struct libusb_device_descriptor usbdesc;
  unsigned char strdesc[64];

  r = libusb_init(NULL);
  if(r < 0)
  {
    debug(FLIDEBUG_FAIL, "%s: Could not initialize LibUSB: %s",
	  __PRETTY_FUNCTION__, libusb_error_name(r));

    return -ENODEV;
  }

//  libusb_set_debug(NULL,LIBUSB_LOG_LEVEL_DEBUG); 

  usb_han = libusb_fli_find_handle(NULL, name);
  io->han = usb_han;

  if (io->han == NULL)
  {
    return -errno;
  }

  debug(FLIDEBUG_INFO, "%s: Found Handle", __PRETTY_FUNCTION__);

  usb_dev = libusb_get_device(io->han);
  debug(FLIDEBUG_INFO, "%s: LibUSB Device found from Handle", __PRETTY_FUNCTION__);

  r = libusb_get_device_descriptor(usb_dev, &usbdesc);
  if (r < 0)
  {
    debug(FLIDEBUG_FAIL, "%s: Could not read descriptor: %s",
	  __PRETTY_FUNCTION__, strerror(errno));
    return -EIO;
  }
  
  if (usbdesc.idVendor != FLIUSB_VENDORID)
  {
    debug(FLIDEBUG_INFO, "%s: Not a FLI device!", __PRETTY_FUNCTION__);
    return -ENODEV;
  }

  switch (usbdesc.idProduct)
  {
    /* These are valid product IDs */
  case FLIUSB_CAM_ID:
  case FLIUSB_FOCUSER_ID:
  case FLIUSB_FILTER_ID:
  case FLIUSB_PROLINE_ID:
    break;

  default:
    /* Anything else is unknown */
    return -ENODEV;
  }

  DEVICE->devinfo.devid = usbdesc.idProduct;
  DEVICE->devinfo.fwrev = usbdesc.bcdDevice;

  if (usbdesc.iSerialNumber != 0)
  {
    memset(strdesc, '\0', sizeof(strdesc));

    if (libusb_get_string_descriptor_ascii(io->han, usbdesc.iSerialNumber,
      strdesc, sizeof(strdesc) - 1) < 0)
    {
      debug(FLIDEBUG_FAIL, "%s: Could not read descriptor ascii: %s",
        __PRETTY_FUNCTION__, strerror(errno));
    }
    else
    {
      DEVICE->devinfo.serial = xstrndup((const char *)strdesc, sizeof(strdesc));
  
      debug(FLIDEBUG_INFO, "Serial Number: %s", strdesc);
    }
  }
  else
  {
    debug(FLIDEBUG_INFO, "Device is not serialized.");
  }

  if((r = libusb_kernel_driver_active(io->han, 0)) == 1)
  {
    debug(FLIDEBUG_INFO, "Kernel Driver Active.");
    if(libusb_detach_kernel_driver(io->han, 0) == 0)
    {
      debug(FLIDEBUG_INFO, "Kernel Driver Detached.");
    }
  }

#ifdef LIBUSB_SETCONFIGURATION
  /* This is #defined out per notes on linux kernel USBFS documentation
   */

  /* Set the USB device configuration to index 0 */
  if ((r = libusb_set_configuration(io->han, 1)) != 0)
  {
    debug(FLIDEBUG_FAIL, "%s: Could not set device configuration: %s",
	    __PRETTY_FUNCTION__, libusb_error_name(r));
  }
#endif

  if ((r = libusb_claim_interface(io->han, 0)) != 0)
  {
    debug(FLIDEBUG_FAIL, "%s: Could not claim interface: %s",
	    __PRETTY_FUNCTION__, libusb_error_name(r));
      return -ENODEV;
  }

#ifdef CLEAR_HALT
  /* Clear the halt/stall condition for all endpoints in this configuration */
  {
    struct libusb_config_descriptor *dsc;
    {
      if ((r = libusb_get_active_config_descriptor(usb_dev, &dsc)) == 0)
      {
        int numep;
        int ep;
        int epaddr;

        numep = dsc->interface[0].altsetting[0].bNumEndpoints;
        
        debug(FLIDEBUG_INFO, "Config Desc: %d", dsc->bConfigurationValue);
        debug(FLIDEBUG_INFO, "NumEP: %d", dsc->interface[0].altsetting[0].bNumEndpoints);

        for (ep = 0; ep < numep; ep++)
        {
          epaddr = dsc->interface[0].altsetting[0].endpoint[ep].bEndpointAddress;
          debug(FLIDEBUG_INFO, "EP: %d %02x", ep, epaddr);
          libusb_clear_halt(io->han, epaddr);
        }

        libusb_free_config_descriptor(dsc);
      }
      else
      {
        debug(FLIDEBUG_FAIL, "%s: Could not obtain configuration descriptor: %s",
          __PRETTY_FUNCTION__, libusb_error_name(r));
      }
    }
  }  
#endif

  return 0;
}

long libusb_bulktransfer(flidev_t dev, int ep, void *buf, long *len)
{
  fli_unixio_t *io;
  unsigned int remaining;
  int r, err = 0;

#define _DEBUG

#ifdef _DEBUG
  debug(FLIDEBUG_INFO, "%s: attempting %ld bytes %s",
	__PRETTY_FUNCTION__, *len, (ep & LIBUSB_ENDPOINT_IN) ? "in" : "out");
#endif

  io = DEVICE->io_data;

#ifdef _DEBUG
  if ((ep & 0xf0) == 0) {
    char buffer[1024];
    int i;

    sprintf(buffer, "OUT %6ld: ", *len);
    for (i = 0; i < ((*len > 16)?16:*len); i++) {
      sprintf(buffer + strlen(buffer), "%02x ", ((unsigned char *) buf)[i]);
    }

    debug(FLIDEBUG_INFO, buffer);
  }

#endif /* _DEBUG */

  remaining = *len;

  while (remaining)  /* read up to USB_READ_SIZ_MAX bytes at a time */
  {
    int bytes;
    int count;

    count = MIN(remaining, USB_READ_SIZ_MAX);

     r = libusb_bulk_transfer(io->han, ep,
      (unsigned char *) (buf + *len - remaining), count, &bytes,
      (DEVICE->io_timeout < FLIUSB_MIN_TIMEOUT)?FLIUSB_MIN_TIMEOUT:DEVICE->io_timeout);
    if( r != 0)
    {
      debug(FLIDEBUG_WARN, "LibUSB Error: %s", libusb_error_name(r));
      break;
    }

    remaining -= bytes;
    if (bytes < count)
      break;
  }

  /* Set *len to the number of bytes actually transfered */
  if (remaining)
    err = -errno;
  *len -= remaining;

#ifdef _DEBUG

  if ((ep & 0xf0) != 0) {
    char buffer[1024];
    int i;

    sprintf(buffer, " IN %6ld: ", *len);
    for (i = 0; i < ((*len > 16)?16:*len); i++) {
      sprintf(buffer + strlen(buffer), "%02x ", ((unsigned char *) buf)[i]);
    }

    debug(FLIDEBUG_INFO, buffer);
  }

#endif /* _DEBUG */

  return err;
}

long libusb_bulkwrite(flidev_t dev, void *buf, long *wlen)
{
  int ep;

  switch (DEVICE->devinfo.devid)
  {
  case FLIUSB_CAM_ID:
  case FLIUSB_FOCUSER_ID:
  case FLIUSB_FILTER_ID:
    ep = 0x02;
    break;

  case FLIUSB_PROLINE_ID:
    ep = 0x01;
    break;

  default:
    debug(FLIDEBUG_FAIL, "Unknown device type.");
    return -EINVAL;
  }

  return libusb_bulktransfer(dev, ep | LIBUSB_ENDPOINT_OUT, buf, wlen);
}

long libusb_bulkread(flidev_t dev, void *buf, long *rlen)
{
  int ep;

  switch (DEVICE->devinfo.devid)
  {
  case FLIUSB_CAM_ID:
  case FLIUSB_FOCUSER_ID:
  case FLIUSB_FILTER_ID:
    ep = 0x02;
    break;

  case FLIUSB_PROLINE_ID:
    ep = 0x01;
    break;

  default:
    debug(FLIDEBUG_FAIL, "Unknown device type.");
    return -EINVAL;
  }

  return libusb_bulktransfer(dev, ep | LIBUSB_ENDPOINT_IN, buf, rlen);
}

long libusb_usb_disconnect(flidev_t dev,  fli_unixio_t *io)
{
  long err = 0;
	
  debug(FLIDEBUG_INFO, "Disconnecting");

  if (io->han != NULL)
  {
		libusb_release_interface(io->han, 0);
    libusb_close(io->han);
  }
	io->han = NULL;

  libusb_exit(NULL);
	
  return err;
}

int libusb_fli_get_serial(libusb_device *usb_dev, char *serial, size_t max_serial)
{
  struct libusb_device_descriptor usb_desc;
  libusb_device_handle *usb_han;
  int r = (-1);

  if (libusb_get_device_descriptor(usb_dev, &usb_desc) == LIBUSB_SUCCESS)
  {
    if (usb_desc.iSerialNumber != 0)
    {
      r = libusb_open(usb_dev, &usb_han);
      if(r == 0)
      {
        libusb_get_string_descriptor_ascii(usb_han, usb_desc.iSerialNumber,
          (unsigned char *) serial, max_serial);

        libusb_close(usb_han);
      }
    }
  }

  /* Handle no serial */
  if (r != 0)
  {
    if (max_serial > 0) serial[0] = '\0';
  }
  
  return r;
}

int libusb_fli_create_name(libusb_device *usb_dev, char *name, size_t max_name)
{
  uint8_t addr, port_nums[7];
  int numports;
  size_t len = 0;
  char name_prefix[] = "FLI-";
  char *p, *s;
  int port;

  if (name == NULL)
  {
    return 0;
  }

  numports = libusb_get_port_numbers(usb_dev, port_nums, sizeof(port_nums));
  addr = libusb_get_device_address(usb_dev);

  if ( (numports < 0) || (numports > sizeof(port_nums)) )
  {
    return 0;
  }

  p = name;

  /* Copy over the prefix */
  for (s = name_prefix; (*s != '\0') && (len < max_name); p++, s++, len ++)
  {
    *p = *s;
  }

  /* Now append the port numbers as hexadecimal strings */
  for (port = 0; (len < max_name) && (port < numports); port ++)
  {
    /* Most significant nybble */
    if (len < max_name)
    {
      uint8_t c = port_nums[port] >> 4;
      *p = (c > 9)?c - 10 + 'A':c + '0';
      p++; len ++;
    }

    /* Least significant nybble */
    if (len < max_name)
    {
      uint8_t c = port_nums[port];
      *p = (c > 9)?c - 10 + 'A':c + '0';
      p++; len ++;
    }
  }

  #ifdef ADD_ADDRESS
  /* Add a delimiter to the end of the name to prevent false detection */
  if (len < max_name)
  {
    *p = 'A';
    p++; len ++;
  }

  /* Now append the address */
  /* Most significant nybble */
  if (len < max_name)
  {
    uint8_t c = addr >> 4;
    *p = (c > 9)?c - 10 + 'A':c + '0';
    p++; len ++;
  }

  /* Least significant nybble */
  if (len < max_name)
  {
    uint8_t c = addr;
    *p = (c > 9)?c - 10 + 'A':c + '0';
    p++; len ++;
  }
#endif

  if (len < max_name)
  {
    *p = '\0';
  }

  return len;  
}

long libusb_list(char *pattern, flidomain_t domain, char ***names)
{
  int r, i;
  char **list;
  libusb_device **usb_devs;
  libusb_device *usb_dev;
  struct libusb_device_descriptor usb_desc;
  int num_usb_devices, num_fli_devices=0;

  r = libusb_init(NULL);
  if(r < 0)
  {
    debug(FLIDEBUG_FAIL, "%s: Could not initialize LibUSB: %s",
	  __PRETTY_FUNCTION__, libusb_error_name(r));
    libusb_exit(NULL);
    return -ENODEV;
  }
	
  num_usb_devices = libusb_get_device_list(NULL, &usb_devs);
  if(num_usb_devices < 0)
  {
    debug(FLIDEBUG_WARN, "LibUSB Get Device List Failed");
    libusb_free_device_list(usb_devs, 1);
    libusb_exit(NULL);
    return -ENODEV;
  }

  list = xmalloc(sizeof(*list));
  *list = NULL;

  num_fli_devices = 0;
  i = -1;

  while((usb_dev = usb_devs[++i]) != NULL)
  {
    flidev_t dev;
    char fli_usb_name[32];
    char fli_serial_name[32];
    char * fli_device_name = NULL;
    char fli_model_name[32];

    if(libusb_get_device_descriptor(usb_dev, &usb_desc) != LIBUSB_SUCCESS)
    {
      debug(FLIDEBUG_WARN, "USB Device Descriptor not obtained.");
      continue;
    }
    
    /* FLI device? */
    if(usb_desc.idVendor != FLIUSB_VENDORID)
      continue;

    /* Determine if device matches domain */
    switch(domain & FLIDOMAIN_DEVICE_MASK)
    {
      case FLIDEVICE_CAMERA:
        if (!((usb_desc.idProduct == FLIUSB_CAM_ID) ||
          (usb_desc.idProduct == FLIUSB_PROLINE_ID)))
        {
          continue;
        }
        break;

      case FLIDEVICE_FOCUSER:
        if (usb_desc.idProduct != FLIUSB_FOCUSER_ID)
        {
          continue;
        }
        break;

      case FLIDEVICE_FILTERWHEEL:
        if (!((usb_desc.idProduct == FLIUSB_FILTER_ID) ||
          (usb_desc.idProduct == FLIUSB_CFW4_ID)))
        {
          continue;
        }
        break;

      default:
        continue;
        break;
    }

    memset(fli_usb_name, '\0', sizeof(fli_usb_name));
    memset(fli_serial_name, '\0', sizeof(fli_serial_name));
    memset(fli_model_name, '\0', sizeof(fli_model_name));
    libusb_fli_create_name(usb_dev, fli_usb_name, sizeof(fli_usb_name) - 1);
    libusb_fli_get_serial(usb_dev, fli_serial_name, sizeof(fli_serial_name) - 1);

    if ((domain & FLIDEVICE_ENUMERATE_BY_SERIAL) &&
      (fli_serial_name[0] != '\0'))
    {
      fli_device_name = fli_serial_name;
    }
    else
    {
      fli_device_name = fli_usb_name;
    }

    debug(FLIDEBUG_INFO, "Device Name: '%s'", fli_device_name);

    /* FLIOpen is needed for a proper description (model) from the device. If the device is opened by
     * another process and clamied, this will fail, so we will return the second string descriptor.
     *
     * Another option is to move libusb_clam_xxx to the IO functions, but since there doesn't seem to
     * be a straightforward method to wait for a device to become available...
     */

    if (FLIOpen(&dev, fli_device_name, domain) == 0) /* Opened and should have model */
    {
		if(DEVICE->devinfo.model == NULL)
			DEVICE->devinfo.model = strdup("DEVICE->devinfo.model is NULL");
	  strncpy(fli_model_name, DEVICE->devinfo.model, sizeof(fli_model_name) - 1);
      FLIClose(dev);
    }
    else
    {
      libusb_device_handle *usb_han;

      if( (libusb_open(usb_dev, &usb_han) == 0) && (usb_desc.iProduct > 0) )
      {
        libusb_get_string_descriptor_ascii(usb_han, usb_desc.iProduct,
          (unsigned char *) fli_model_name, sizeof(fli_model_name) - 1);
  
        libusb_close(usb_han);
      }
      else
      {
        strncpy(fli_model_name, "Model unavailable", sizeof(fli_model_name) - 1);
      }
    }

    /* Add it to the list */
    if ((list[num_fli_devices] = xmalloc(strlen(fli_device_name) +
            strlen(fli_model_name) + 2)) == NULL)
    {
      int j;
      
      /* Free the list */
      for (j = 0; j < num_fli_devices; j++)
        xfree(list[j]);
      xfree(list);

      libusb_exit(NULL);
      return -ENOMEM;
    }
    else
    {
      sprintf(list[num_fli_devices], "%s;%s", fli_device_name, fli_model_name);
      FLIClose(dev);
      num_fli_devices++;
    }
  }

  libusb_free_device_list(usb_devs, 1);

  debug(FLIDEBUG_INFO, "Number of FLI Devices: %d", num_fli_devices);

  /* Terminate the list */
  list[num_fli_devices++] = NULL;
  list = xrealloc(list, num_fli_devices * sizeof(char *));
  *names = list;

  libusb_exit(NULL);
  return 0;
}

libusb_device_handle * libusb_fli_find_handle(struct libusb_context *usb_ctx, char *name)
{
  int r, i;
  libusb_device **usb_devs;
  libusb_device *usb_dev;
  libusb_device_handle *usb_han;
  struct libusb_device_descriptor usb_desc;
  int num_usb_devices;
  unsigned char serial[64];

  num_usb_devices = libusb_get_device_list(NULL, &usb_devs);
  if(num_usb_devices < 0)
  {
    debug(FLIDEBUG_FAIL, "LibUSB Get Device Failed with %s", libusb_error_name(num_usb_devices));
    return NULL;
  }

  for (i = 0; (usb_dev = usb_devs[i]) != NULL; i++)
  {
    char fli_usb_name[24];

    if(libusb_get_device_descriptor(usb_dev, &usb_desc) != LIBUSB_SUCCESS)
      continue;
      
    if(usb_desc.idVendor == FLIUSB_VENDORID)
    {
      memset(serial, '\0', sizeof(serial));
      memset(fli_usb_name, '\0', sizeof(fli_usb_name));
      libusb_fli_create_name(usb_dev, fli_usb_name, sizeof(fli_usb_name) - 1);

      /* May be a serial number as the name, so let's quickly read it */
      if (usb_desc.iSerialNumber != 0)
      {
        r = libusb_open(usb_dev, &usb_han);
        if(r == 0)
        {
          libusb_get_string_descriptor_ascii(usb_han, usb_desc.iSerialNumber,
            serial, sizeof(serial) - 1);

          libusb_close(usb_han);
        }
      }

      /* Is it the same as the one we are asking to open */
      if( (strncasecmp(fli_usb_name, name, sizeof(fli_usb_name)) == 0) ||
       (strncasecmp((char *) serial, name, sizeof(serial)) == 0 ))
      {
        r = libusb_open(usb_dev, &usb_han);
        if(r == 0)
        {
          // Found device handle, cleanup and return
          debug(FLIDEBUG_INFO, "Found Device Handle");
          libusb_free_device_list(usb_devs, 1);

          return usb_han;
        }
        else
        {
          debug(FLIDEBUG_WARN, "Get USB Device Handle Failed");
        }
      }
    }
  }

  libusb_free_device_list(usb_devs, 1);
  return NULL;
}
