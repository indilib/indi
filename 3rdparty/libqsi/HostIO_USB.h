/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * qsilib
 * Copyright (C) QSI (Quantum Scientific Imaging) 2005-2013 <dchallis@qsimaging.com>
 * 
 */
#pragma once
//
// Select the approriate ftdi library.  Default is libftdi-0.1.
// use "./configure --enable-ftd2xx" to switch to libftd2xx.
// and "./configure --enable-libftdi" for the open source stack.
// "./condifure" with no options defaults to libftdi
//
#if defined(USELIBFTDIZERO)
	#include <ftdi.h>
	#define FT_PURGE_TX 1
	#define FT_PURGE_RX 2
#elif defined(USELIBFTDIONE)

#elif defined (USELIBFTD2XX)
	#include <ftd2xx.h>
#else

#endif
//

#include "IHostIO.h"
#include "QSILog.h"
#include "QSI_Global.h"
#include "VidPid.h"

class HostIO_USB : public IHostIO
{

public:
	HostIO_USB(void);
	virtual ~HostIO_USB(void);
	virtual int ListDevices(std::vector<CameraID> &);
	virtual int OpenEx(CameraID);
	virtual int SetTimeouts(int, int);
	virtual int Close();
	virtual int Write(unsigned char *, int, int *);
	virtual int Read(unsigned char *, int, int *);
	virtual int GetReadWriteQueueStatus(int *, int *);
	virtual int ResetDevice();
	virtual int Purge();
	virtual int GetReadQueueStatus(int *);
	virtual int SetStandardReadTimeout ( int ulTimeout);
	virtual int SetStandardWriteTimeout( int ulTimeout);
	virtual int SetIOTimeout (IOTimeout ioTimeout);
	// USB Specific calls
	int SetLatencyTimer(UCHAR);
#ifdef WIN32
#ifdef USELIBFTD2XX
	int QSI_FT_EE_Program(  int DeviceIndex, int VID, int PID, LPTSTR SerialNumber, LPTSTR ManufacturerID, 
							LPTSTR Manufacturer, LPTSTR Description, bool  BusPowered, int MaxPower, bool RemoteWakeup);
	int Open(int iDevice, FT_HANDLE ftHandle);	
#endif
#endif

protected:
	int				SetUSBParameters(DWORD, DWORD);
	QSI_IOTimeouts	m_IOTimeouts;
	QSILog			*m_log;				// Log USB transactions

private:
	int				m_iUSBStatus;	    // Generic return status
	void* 			m_DLLHandle;        // Holds pointer to FTDI DLL in memory
	bool 			m_bLoaded;          // True if the FTDI USB DLL is loaded
	int 			m_iLoadStatus;      //
	std::vector<VidPid> 	m_vidpids;	// Table of Vendor and Product IDs to try
	
#if defined(USELIBFTDIZERO)
	int my_ftdi_read_data(struct ftdi_context *ftdi, unsigned char *buf, int size);		
	ftdi_context m_ftdi;
	bool m_ftdiIsOpen;
#elif defined(USELIBFTDIONE)

#elif defined(USELIBFTD2XX)
	FT_HANDLE 				m_DeviceHandle; // Holds handle to usb device when connected
#endif

	
};

