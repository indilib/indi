// ApnUsbSys.h    
//
// Copyright (c) 2003, 2004 Apogee Instruments, Inc.
//
// Defines common data structure(s) for sharing between application
// layer and the ApUSB.sys device driver.
//

#if !defined(_APNUSBSYS_H__INCLUDED_)
#define _APNUSBSYS_H__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000



#define VND_ANCHOR_LOAD_INTERNAL		0xA0

#define VND_APOGEE_CMD_BASE			0xC0
#define VND_APOGEE_STATUS			( VND_APOGEE_CMD_BASE + 0x0 )
#define VND_APOGEE_CAMCON_REG			( VND_APOGEE_CMD_BASE + 0x2 )
#define VND_APOGEE_BUFCON_REG			( VND_APOGEE_CMD_BASE + 0x3 )
#define VND_APOGEE_SET_SERIAL			( VND_APOGEE_CMD_BASE + 0x4 )
#define VND_APOGEE_SERIAL			( VND_APOGEE_CMD_BASE + 0x5 )
#define VND_APOGEE_EEPROM			( VND_APOGEE_CMD_BASE + 0x6 )
#define VND_APOGEE_SOFT_RESET			( VND_APOGEE_CMD_BASE + 0x8 )
#define VND_APOGEE_GET_IMAGE			( VND_APOGEE_CMD_BASE + 0x9 )
#define VND_APOGEE_STOP_IMAGE			( VND_APOGEE_CMD_BASE + 0xA )
#define VND_APOGEE_VERSION			( VND_APOGEE_CMD_BASE + 0xB )
#define VND_APOGEE_VENDOR			( VND_APOGEE_CMD_BASE + 0xC )
#define VND_APOGEE_DATA_PORT                    ( VND_APOGEE_CMD_BASE + 0xD )
#define VND_APOGEE_CONTROL_PORT                 ( VND_APOGEE_CMD_BASE + 0xE )



#define	REQUEST_IN	0x1
#define REQUEST_OUT	0x0


typedef struct _APN_USB_REQUEST
{
	unsigned char	Request;
	unsigned char	Direction;
	unsigned short	Value;
	unsigned short	Index;
} APN_USB_REQUEST, *PAPN_USB_REQUEST;



#endif  // !defined(_APNUSBSYS_H__INCLUDED_)
