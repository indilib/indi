/*****************************************************************************************
NAME
 USBWrapper

DESCRIPTION
 USB Wrapper

COPYRIGHT (C)
 QSI (Quantum Scientific Imaging) 2005-2006

REVISION HISTORY
 MWB 04.28.06 Original Version
 *****************************************************************************************/

#pragma once

// Modify the following defines to select the ftdi driver stack of your choice.
// Be sure to also adjust the Makefile.am in the lib directory prior to running ./configure and make all
// Also be sure to define only one.
//
// which ftdi USB library to use
// open source is 
// #define USBLIBFTDI0 for libftdi
// #define USELIBFT2XX for libftd2xx.so

#define USBLIBFTDI0
#undef USELIBFTDI1
#undef USELIBFTD2XX

#ifdef USBLIBFTDI0
	#include <ftdi.h>
	#define FT_PURGE_TX 1
	#define FT_PURGE_RX 2
#else
	#include <ftd2xx.h>
#endif
//
#include "CameraID.h"
#include "QSI_Global.h"
#include "QSILog.h"
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <vector>

#define INTERFACERETRYMS 2500

//****************************************************************************************
// CLASS PROTOTYPE
//****************************************************************************************

class QSI_USBWrapper  // USB Driver Wrapper
{
public:

	//////////////////////////////////////////////////////////////////////////////////////
	// FUNCTION PROTOTYPES

	QSI_USBWrapper();        // Constructor function used to initialize variables
	~QSI_USBWrapper();

	int USB_ListAllDevices( std::vector<CameraID> & vID );
	int USB_OpenEx(CameraID);
	int USB_SetTimeouts(uint32_t, uint32_t);
	int USB_Close();
	int USB_Write(LPVOID, DWORD, LPDWORD);
	int USB_Read(LPVOID, DWORD, LPDWORD);
	int USB_GetStatus(LPDWORD, LPDWORD);
	int USB_SetLatencyTimer(UCHAR);
	int USB_ResetDevice();
	int USB_Purge(uint32_t);
	int USB_GetQueueStatus(LPDWORD);
	int USB_SetUSBParameters(DWORD, DWORD);

private:

	#ifdef USBLIBFTDI0
	int my_ftdi_read_data(struct ftdi_context *ftdi, unsigned char *buf, int size);
	#endif

  //////////////////////////////////////////////////////////////////////////////////////
  // VARIABLES

	void* m_DLLHandle;        // Holds pointer to FTDI DLL in memory
	bool m_bLoaded;             // True if the FTDI USB DLL is loaded
	int m_iLoadStatus;          //
	int m_iStatus;				// Generic return status

#if defined(USBLIBFTDI0)
	ftdi_context m_ftdi;
	bool m_ftdiIsOpen;
#elif defined(USELIBFTDI1)

#elif defined(USELIBFTD2XX)
	FT_HANDLE m_DeviceHandle;   // Holds handle to usb device when connected
#endif

protected:
	QSILog *m_log;				// Log USB transactions
};
