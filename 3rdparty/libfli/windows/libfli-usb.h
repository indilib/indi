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

#ifndef _LIBFLI_USB_H_
#define _LIBFLI_USB_H_

#define MAXCAM_EP_IN (0x82)
#define MAXCAM_EP_OUT (0x02)

#define USB_DIR_OUT (0x00)
#define USB_DIR_IN (0x80)

#define USB_READ_SIZ_MAX (65535)

typedef struct fli_usb_epio_t
{
	WORD Endpoint; /* USB Endpoint */
	DWORD Timeout; /* USB I/O Timeout in mSec */
} FLI_USB_EPIO, *PFLI_USB_EPIO;

long usb_bulktransfer(flidev_t dev, int ep, void *buf, long *len);
long usbio(flidev_t dev, void *buf, long *wlen, long *rlen);

/* Legacy stuff to work with old driver */
typedef struct _FLI_PIPE_INFORMATION {
	USHORT MaximumPacketSize;
	UCHAR EndpointAddress;
	UCHAR Interval;
	UCHAR PipeType;
	DWORD PipeHandle; /* This is a pointer, causes problems in 64 bit driver */
	ULONG MaximumTransferSize;
	ULONG PipeFlags;                                   
} FLI_PIPE_INFORMATION, *PFLI_PIPE_INFORMATION;

typedef struct _FLI_INTERFACE_INFORMATION {
	USHORT Length;
	UCHAR InterfaceNumber;
	UCHAR AlternateSetting;
	UCHAR Class;
	UCHAR SubClass;
	UCHAR Protocol;
	UCHAR Reserved;
	DWORD InterfaceHandle;
	ULONG NumberOfPipes; 
	FLI_PIPE_INFORMATION Pipes[1];
} FLI_INTERFACE_INFORMATION, *PFLI_INTERFACE_INFORMATION;

/* USB Descriptor (from DDK) */
typedef struct __usb_Dev_Descriptor__ {
    UCHAR bLength;
    UCHAR bDescriptorType;
    USHORT bcdUSB;
    UCHAR bDeviceClass;
    UCHAR bDeviceSubClass;
    UCHAR bDeviceProtocol;
    UCHAR bMaxPacketSize0;
    USHORT idVendor;
    USHORT idProduct;
    USHORT bcdDevice;
    UCHAR iManufacturer;
    UCHAR iProduct;
    UCHAR iSerialNumber;
    UCHAR bNumConfigurations;
} Usb_Device_Descriptor, *pUsb_Device_Descriptor;

/* Taken initially from EZUSB before driver was rewritten */
typedef struct _GET_STRING_DESCRIPTOR_IN
{
   UCHAR    Index;
   USHORT   LanguageId;
} GET_STRING_DESCRIPTOR_IN, *PGET_STRING_DESCRIPTOR_IN;

#define IOCTL_GET_DEVICE_DESCRIPTOR CTL_CODE(FILE_DEVICE_UNKNOWN,  \
	0x0800 + 1,\
	METHOD_BUFFERED,  \
	FILE_ANY_ACCESS)

#define IOCTL_BULK_READ CTL_CODE(FILE_DEVICE_UNKNOWN, \
	0x0800 + 19,\
	METHOD_OUT_DIRECT,  \
	FILE_ANY_ACCESS)

#define IOCTL_BULK_WRITE CTL_CODE(FILE_DEVICE_UNKNOWN,  \
	0x0800 + 20,\
	METHOD_IN_DIRECT,  \
	FILE_ANY_ACCESS)

#define IOCTL_GET_LAST_USBD_ERROR CTL_CODE(FILE_DEVICE_UNKNOWN,  \
	0x0800 + 23,\
	METHOD_BUFFERED,  \
	FILE_ANY_ACCESS)

#define IOCTL_GET_STRING_DESCRIPTOR CTL_CODE(FILE_DEVICE_UNKNOWN,  \
	0x0800 + 17,\
	METHOD_BUFFERED,  \
	FILE_ANY_ACCESS)

#ifdef OLDUSBDRIVER

typedef PVOID USBD_PIPE_HANDLE;
typedef PVOID USBD_CONFIGURATION_HANDLE;
typedef PVOID USBD_INTERFACE_HANDLE;

typedef enum _USBD_PIPE_TYPE {
	UsbdPipeTypeControl,
	UsbdPipeTypeIsochronous,
	UsbdPipeTypeBulk,
	UsbdPipeTypeInterrupt
} USBD_PIPE_TYPE;

typedef struct _USBD_PIPE_INFORMATION {
	USHORT MaximumPacketSize;
	UCHAR EndpointAddress;
	UCHAR Interval;
	USBD_PIPE_TYPE PipeType;
	USBD_PIPE_HANDLE PipeHandle;
	ULONG MaximumTransferSize;
	ULONG PipeFlags;                                   
} USBD_PIPE_INFORMATION, *PUSBD_PIPE_INFORMATION;

typedef struct _USBD_INTERFACE_INFORMATION {
    USHORT Length;       // Length of this structure, including
                         // all pipe information structures that
                         // follow.
    //
    // INPUT
    //
    // Interface number and Alternate setting this
    // structure is associated with
    //
    UCHAR InterfaceNumber;
    UCHAR AlternateSetting;
    
    //
    // OUTPUT
    // These fields are filled in by USBD
    //
    UCHAR Class;
    UCHAR SubClass;
    UCHAR Protocol;
    UCHAR Reserved;
    
    USBD_INTERFACE_HANDLE InterfaceHandle;
    ULONG NumberOfPipes; 

    //
    // INPUT/OUPUT
    // see PIPE_INFORMATION
//TPM    USBD_PIPE_INFORMATION Pipes[0];
    USBD_PIPE_INFORMATION Pipes[1];

} USBD_INTERFACE_INFORMATION, *PUSBD_INTERFACE_INFORMATION;
#endif

#endif /* _LIBFLI_UNIX_USB_H_ */
