/*****************************************************************************************
NAME
 USBWrapper

DESCRIPTION
 USB Wrapper

COPYRIGHT (C)
 QSI (Quantum Scientific Imaging) 2005-2009

REVISION HISTORY
 MWB 04.28.06 Original Version
******************************************************************************************/

#include "QSI_USBWrapper.h"

#if defined(USELIBFTD2XX) && defined(USELIBFTDIZERO)
	#error "Multiple FTDI stacks defined.  Use only one of libftdi and libftd2xx"
#endif

#if defined(USELIBFTDIZERO)
	#pragma message "libftdi-0.1 selected."
#elif defined(USELIBFTDIONE)
	#pragma message "libftdi-1.0 selected."
	#error "ftdi-1.0 NOT_IMPLEMENTED"
#elif defined (USELIBFTD2XX)
	#pragma message "libftd2xx selected."
#else
	#error "No ftdi library selected."
#endif


//****************************************************************************************
// CLASS FUNCTION DEFINITIONS
//****************************************************************************************

//////////////////////////////////////////////////////////////////////////////////////////
QSI_USBWrapper::QSI_USBWrapper()
{
	m_DLLHandle = NULL;
	m_bLoaded = false;
	m_iLoadStatus = 0;
	m_iStatus = 0;

	this->m_log = new QSILog("QSIINTERFACELOG.TXT", "LOGUSBTOFILE");

#if defined(USELIBFTDIZERO)
	m_iStatus = ftdi_init(&m_ftdi);
	m_ftdiIsOpen = false;
#elif defined(USELIBFTDIONE)

#elif defined(USELIBFTD2XX)
	m_DeviceHandle = NULL;
#endif
}

QSI_USBWrapper::~QSI_USBWrapper()
{
#if defined(USELIBFTDIZERO)
	ftdi_deinit(&m_ftdi);
#elif defined(USELIBFTDIONE)

#elif defined(USELIBFTD2XX)

#endif
	delete this->m_log;
}

int QSI_USBWrapper::USB_ListAllDevices( std::vector<CameraID> & vID )
{
	m_log->Write(2, "GetDeviceDesc");

	char * pacSerialNumber[USB_MAX_DEVICES]; 
	char * pacDescription[USB_MAX_DEVICES];

	// Create an array of pointers to the buffers
	for (int i = 0; i < USB_MAX_DEVICES; i++)
	{
		pacSerialNumber[i] = new char[USB_SERIAL_LENGTH];
		pacSerialNumber[i][0] = 0;
		pacDescription[i] = new char[USB_DESCRIPTION_LENGTH];
		pacDescription[i][0] = 0;
	}

#if defined(USELIBFTDIZERO)

	ftdi_device_list* devlist=NULL;
	ftdi_device_list* curdev=NULL;
	char  szMan[256];
	int count;

	m_iStatus = ftdi_usb_find_all(&m_ftdi, &devlist, 0x0403, 0xeb48);
	curdev = devlist;
	if (m_iStatus > 0 )
	{
		count = m_iStatus < USB_MAX_DEVICES ? m_iStatus : USB_MAX_DEVICES;
		for (int i = 0; i < count; i++)
		{ 
			ftdi_usb_get_strings(&m_ftdi, curdev->dev, szMan, 256, pacDescription[i], USB_DESCRIPTION_LENGTH, pacSerialNumber[i], USB_SERIAL_LENGTH);
			curdev = curdev->next;
			CameraID id = CameraID(std::string(pacSerialNumber[i]), std::string(pacDescription[i]), 0x0403, 0xeb48);
			vID.push_back(id);
		}
		m_iStatus = 0;
	}

	if (devlist != NULL)
		ftdi_list_free(&devlist);

	m_iStatus = ftdi_usb_find_all(&m_ftdi, &devlist, 0x0403, 0xeb49);
	curdev = devlist;
	if (m_iStatus > 0 )
	{
		count = m_iStatus < USB_MAX_DEVICES ? m_iStatus : USB_MAX_DEVICES;
		for (int i = 0; i < count; i++)
		{ 
			ftdi_usb_get_strings(&m_ftdi, curdev->dev, szMan, 256, pacDescription[i], USB_DESCRIPTION_LENGTH, pacSerialNumber[i], USB_SERIAL_LENGTH);
			curdev = curdev->next;
			CameraID id = CameraID(pacSerialNumber[i], pacDescription[i], 0x0403, 0xeb49);
			vID.push_back(id);
		}
		m_iStatus = 0;
	}

	if (devlist != NULL)
		ftdi_list_free(&devlist);

	m_iStatus = -m_iStatus; // Error codes from ftdi are neg, we expect pos

#elif defined(USELIBFTDIONE)

#elif defined(USELIBFTD2XX)
	DWORD numDevs;
	FT_STATUS ftStatus;
	FT_DEVICE_LIST_INFO_NODE *devinfo;

	ftStatus = FT_SetVIDPID(0x0403, 0xeb48);
	ftStatus = FT_SetVIDPID(0x0403, 0xeb49);
	ftStatus = FT_ListDevices(&numDevs, NULL, FT_LIST_NUMBER_ONLY);
	ftStatus = FT_CreateDeviceInfoList( &numDevs );

	if (ftStatus !=FT_OK)
		numDevs = 0;

	if (numDevs > 0)
	{
		devinfo = (FT_DEVICE_LIST_INFO_NODE*)malloc(sizeof(FT_DEVICE_LIST_INFO_NODE)*(numDevs+1));
		ftStatus = FT_GetDeviceInfoList(devinfo, &numDevs);
		if (ftStatus == FT_OK)
		{
			for (int i = 0; i < (int)numDevs; i++)
			{
				std::string Serial(devinfo[i].SerialNumber);

				if (devinfo[i].ID != 0) //&& (Serial.find_last_of("B") == std::string::npos))  // skip the B channel
				{
					CameraID id = CameraID(std::string(devinfo[i].SerialNumber), std::string(devinfo[i].Description),
							(devinfo[i].ID >> 16) & 0x0000FFFF, devinfo[i].ID & 0x0000FFFF);
					vID.push_back(id);
				}
			}
		}
		free (devinfo);
	}
	m_iStatus = ftStatus;
#endif

	for (int i = 0; i < USB_MAX_DEVICES; i++)
	{
		delete[] pacSerialNumber[i];
		delete[] pacDescription[i];
	}

	m_log->Write(2, "GetDeviceDesc done %x", m_iStatus);
	return m_iStatus;
}

//////////////////////////////////////////////////////////////////////////////////////////
int QSI_USBWrapper::USB_OpenEx(CameraID  cID )
{
	m_log->Write(2, "OpenEx name: %s", cID.Description.c_str());

#if defined(USELIBFTDIZERO)
	m_iStatus = ftdi_set_interface( &m_ftdi, INTERFACE_A ); 

	m_iStatus = ftdi_usb_open_desc(&m_ftdi, cID.VendorID, cID.ProductID, cID.Description.c_str(), cID.SerialNumber.c_str());
	if (m_iStatus == 0)
		m_ftdiIsOpen = true;
	else
	{
		m_ftdiIsOpen = false;
		m_iStatus = -m_iStatus; // Error codes from ftdi are neg, we expect pos
	}

	if (cID.ProductID == 0xeb49 ) // high speed ftdi
	{
		m_iStatus = ftdi_set_bitmode(&m_ftdi, 0x00, BITMODE_SYNCFF);
		if (m_log->LoggingEnabled()) m_log->Write(2, _T("SetBitMode (HS) Done status: %x"), m_iStatus);
	}

	if (m_ftdiIsOpen)
	{
		m_iStatus = ftdi_set_latency_timer(&m_ftdi, LATENCY_TIMER_MS);
		m_iStatus = ftdi_read_data_set_chunksize(&m_ftdi, (1<<14));
		m_iStatus = ftdi_setflowctrl(&m_ftdi, SIO_RTS_CTS_HS);
		m_iStatus |= ftdi_usb_purge_rx_buffer(&m_ftdi);

		if (m_iStatus == 0)
			m_ftdiIsOpen = true;
		else
		{
			m_ftdiIsOpen = false;
			m_iStatus = -m_iStatus; // Error codes from ftdi are neg, we expect pos
		}
	}

#elif defined(USELIBFTDIONE)

#elif defined(USELIBFTD2XX)
	m_iStatus = FT_OpenEx((void*)cID.SerialToOpen.c_str(), FT_OPEN_BY_SERIAL_NUMBER, &m_DeviceHandle);

	if (cID.ProductID == 0xeb49 ) // high speed ftdi
	{
		m_iStatus |= FT_SetBitMode(m_DeviceHandle, 0xff, FT_BITMODE_RESET);
		usleep(10000);	// Delay 10ms as per ftdi example
		m_iStatus |= FT_SetBitMode(m_DeviceHandle, 0xff, FT_BITMODE_SYNC_FIFO);
		m_log->Write(2, _T("SetBitMode (HS) Done status: %x"), m_iStatus);
	}

	// Startup Settings
	m_iStatus |= USB_SetLatencyTimer(LATENCY_TIMER_MS);
	m_iStatus |= USB_SetUSBParameters(0x10000, 0x10000);
	m_iStatus |= FT_SetFlowControl(m_DeviceHandle, FT_FLOW_RTS_CTS, 0, 0);
	m_iStatus |= FT_Purge(m_DeviceHandle, FT_PURGE_RX | FT_PURGE_TX);
	m_iStatus |= FT_SetTimeouts(m_DeviceHandle, READ_TIMEOUT, WRITE_TIMEOUT);
	m_iStatus |= FT_SetChars(m_DeviceHandle, false, 0, false, 0);

#endif

	m_log->Write(2, "OpenEx Done status: %x", m_iStatus);
	return m_iStatus;
}

//////////////////////////////////////////////////////////////////////////////////////////
int QSI_USBWrapper::USB_SetTimeouts(uint32_t dwReadTimeout, uint32_t dwWriteTimeout)
{
	m_log->Write(2, "SetTimeouts %d ReadTimeout %d WriteTimeout", dwReadTimeout, dwWriteTimeout);

	if (dwReadTimeout  < 1000) dwReadTimeout = 1000;
	if (dwWriteTimeout < 1000) dwWriteTimeout = 1000;

#if defined(USELIBFTDIZERO)
	m_ftdi.usb_read_timeout = dwReadTimeout;
	m_ftdi.usb_write_timeout = dwWriteTimeout;
	m_iStatus = 0;
#elif defined(USELIBFTDIONE)

#elif defined(USELIBFTD2XX)
	m_iStatus =  FT_SetTimeouts(m_DeviceHandle, dwReadTimeout, dwWriteTimeout);
#endif

	m_log->Write(2, "SetTimeouts Done %x", m_iStatus);
	return m_iStatus;
}

//////////////////////////////////////////////////////////////////////////////////////////
int QSI_USBWrapper::USB_Close()
{

	m_log->Write(2, "Close");

#if defined(USELIBFTDIZERO)
	if (m_ftdiIsOpen)
	{
		m_iStatus = ftdi_usb_close(&m_ftdi);
		m_ftdiIsOpen = false;
	}
	ftdi_deinit(&m_ftdi);
	m_iStatus = ftdi_init(&m_ftdi);
	m_iStatus = -m_iStatus; // Error codes from ftdi are neg, we expect pos
#elif defined(USELIBFTDIONE)

#elif defined(USELIBFTD2XX)
	m_iStatus = FT_Close(m_DeviceHandle);
#endif

	m_log->Write(2, "Close Done status: %x", m_iStatus);
	m_log->Close();				// flush to log file
	m_log->TestForLogging();	// turn off logging is appropriate

	return m_iStatus;
}

//////////////////////////////////////////////////////////////////////////////////////////
int QSI_USBWrapper::USB_Write(LPVOID lpvBuffer, DWORD dwBuffSize, LPDWORD lpdwBytes)
{

	m_log->Write(2, _T("Write %d bytes, Data:"), dwBuffSize);
	m_log->WriteBuffer(2, lpvBuffer, dwBuffSize, dwBuffSize, 256);

#if defined(USELIBFTDIZERO)
	m_iStatus = ftdi_write_data(&m_ftdi, (unsigned char *)lpvBuffer, dwBuffSize);
	if (m_iStatus >= 0)
	{
		*lpdwBytes = m_iStatus;
		m_iStatus = 0;
	}
	else
	{
		*lpdwBytes = 0;
		m_iStatus = -m_iStatus; // Error codes from ftdi are negative, we expect positive
	}
#elif defined(USELIBFTDIONE)

#elif defined(USELIBFTD2XX)
	m_iStatus =  FT_Write(m_DeviceHandle, (unsigned char *)lpvBuffer, dwBuffSize, lpdwBytes);
#endif

	m_log->Write(2, "Write Done %d bytes written, status: %x", *lpdwBytes, m_iStatus);
	return m_iStatus;
}

//////////////////////////////////////////////////////////////////////////////////////////
int QSI_USBWrapper::USB_Read(LPVOID lpvBuffer, DWORD dwBuffSize, LPDWORD lpdwBytesRead)
{
	m_log->Write(2, "Read buffer size: %d bytes", dwBuffSize);

#if defined(USELIBFTDIZERO)
	m_iStatus = my_ftdi_read_data(&m_ftdi, (unsigned char *)lpvBuffer, dwBuffSize);
	if (m_iStatus > 0)
	{
		*lpdwBytesRead = m_iStatus;
		m_iStatus = 0;
	}
	else
	{
		*lpdwBytesRead = 0;
		m_iStatus = -m_iStatus; // Error codes from ftdi are negative, we expect positive
		if (m_iStatus == 0) m_iStatus = 4; // read returned with zero bytes.
	}
#elif defined(USELIBFTDIONE)

#elif defined(USELIBFTD2XX)
	m_iStatus = FT_Read(m_DeviceHandle, lpvBuffer, dwBuffSize, lpdwBytesRead);
#endif
	m_log->Write(2, _T("Read Done %d bytes read, status: %x, data: "), *lpdwBytesRead, m_iStatus);
	m_log->WriteBuffer(2, lpvBuffer, dwBuffSize, *lpdwBytesRead, 256);

	return m_iStatus;
}

//////////////////////////////////////////////////////////////////////////////////////////
int QSI_USBWrapper::USB_GetStatus(LPDWORD lpdwAmountInRxQueue, LPDWORD lpdwAmountInTxQueue)
{
	m_log->Write(2, "GetStatus");

#if defined(USELIBFTDIZERO)
	m_iStatus = 0;
	*lpdwAmountInRxQueue = m_ftdi.readbuffer_remaining;
	*lpdwAmountInTxQueue = 0;	
#elif defined(USELIBFTDIONE)

#elif defined(USELIBFTD2XX)
	DWORD dwDummy = 0;  				// Used in place of lpdwEventStatus
	m_iStatus = FT_GetStatus(m_DeviceHandle, lpdwAmountInRxQueue, lpdwAmountInTxQueue, &dwDummy);	
#endif

	m_log->Write(2, "GetStatus Done %d bytes read queue, %d bytes write queue, status: %x", *lpdwAmountInRxQueue, *lpdwAmountInTxQueue, m_iStatus);
	return m_iStatus;
}

//////////////////////////////////////////////////////////////////////////////////////////
int QSI_USBWrapper::USB_SetLatencyTimer(UCHAR ucTimer)
{
	m_log->Write(2, "SetLatencyTimer %0hx", ucTimer);

#if defined(USELIBFTDIZERO)
	m_iStatus = ftdi_set_latency_timer(&m_ftdi, ucTimer);
	m_iStatus = -m_iStatus;
#elif defined(USELIBFTDIONE)

#elif defined(USELIBFTD2XX)
	m_iStatus = FT_SetLatencyTimer(m_DeviceHandle, ucTimer);
#endif

	m_log->Write(2, "SetLatencyTimer Done status: %x", m_iStatus);
	return m_iStatus;
}

//////////////////////////////////////////////////////////////////////////////////////////
int QSI_USBWrapper::USB_ResetDevice()
{
	m_log->Write(2, "ResetDevice");

#if defined(USELIBFTDIZERO)
	m_iStatus = ftdi_usb_reset(&m_ftdi);
	m_iStatus = -m_iStatus;
#elif defined(USELIBFTDIONE)

#elif defined(USELIBFTD2XX)
	m_iStatus = FT_ResetDevice(m_DeviceHandle);
#endif

	m_log->Write(2, "ResetDevice Done status: %x", m_iStatus);
	return m_iStatus;
}

//////////////////////////////////////////////////////////////////////////////////////////
int QSI_USBWrapper::USB_Purge(uint32_t dwMask)
{
	m_log->Write(2, "Purge mask: %08lx", dwMask);

#if defined(USELIBFTDIZERO)
	m_iStatus = ftdi_usb_purge_buffers(&m_ftdi);
	m_iStatus = -m_iStatus;
#elif defined(USELIBFTDIONE)

#elif defined(USELIBFTD2XX)
	m_iStatus = FT_Purge(m_DeviceHandle, dwMask);
#endif

	m_log->Write(2, "Purge Done status: %x", m_iStatus);
	return m_iStatus;
}

//////////////////////////////////////////////////////////////////////////////////////////
int QSI_USBWrapper::USB_GetQueueStatus(LPDWORD lpdwAmountInRxQueue)
{
	m_log->Write(2, "GetQueueStatus");

#if defined(USELIBFTDIZERO)
	m_iStatus = 0;
	*lpdwAmountInRxQueue = m_ftdi.readbuffer_remaining;	
#elif defined(USELIBFTDIONE)

#elif defined(USELIBFTD2XX)
	m_iStatus = FT_GetQueueStatus(m_DeviceHandle, lpdwAmountInRxQueue);
#endif

	m_log->Write(2, "GetQueueStatus Done %d in Rx queue, status: %x", *lpdwAmountInRxQueue, m_iStatus);
	return m_iStatus;
}

//////////////////////////////////////////////////////////////////////////////////////////
int QSI_USBWrapper::USB_SetUSBParameters(DWORD dwInSize, DWORD dwOutSize)
{
	m_log->Write(2, "SetUSBParamters %d In Size, %d Out Size", dwInSize, dwOutSize);

#if defined(USELIBFTDIZERO)
	m_iStatus = 0;
	if (dwInSize  != 0) 
		// m_iStatus = ftdi_read_data_set_chunksize(&m_ftdi, dwInSize);
		ftdi_read_data_set_chunksize(&m_ftdi, 1<<14);
	if (dwOutSize != 0) 
		m_iStatus += ftdi_write_data_set_chunksize(&m_ftdi, dwOutSize);
	m_iStatus = -m_iStatus;
#elif defined(USELIBFTDIONE)

#elif defined(USELIBFTD2XX)
	m_iStatus = FT_SetUSBParameters(m_DeviceHandle, dwInSize, dwOutSize);
#endif

	m_log->Write(2, "SetUSBParamters Done status: %x", m_iStatus);
	return m_iStatus;
}

#if defined(USELIBFTDIZERO)

int QSI_USBWrapper::my_ftdi_read_data(struct ftdi_context *ftdi, unsigned char *buf, int size)
{
	const int uSECPERSEC = 1000000;
	const int uSECPERMILLISEC = 1000;

	int offset;
	int result;
	// Sleep interval, 1 microsecond
	timespec tm;
	tm.tv_sec = 0;
	tm.tv_nsec = 1000L;
	// Read timeout
	timeval startTime;
	timeval timeout;
	timeval now;

	gettimeofday(&startTime, NULL);
	// usb_read_timeout in milliseconds
	// Calculate read timeout time of day
	timeout.tv_sec = startTime.tv_sec + ftdi->usb_read_timeout / uSECPERMILLISEC;
	timeout.tv_usec = startTime.tv_usec + ((ftdi->usb_read_timeout % uSECPERMILLISEC)*uSECPERMILLISEC);
	if (timeout.tv_usec >= uSECPERSEC)
	{
		timeout.tv_sec++;
		timeout.tv_usec -= uSECPERSEC;
	}

	offset = 0;
	result = 0;

	while (size > 0)
	{
		result = ftdi_read_data(ftdi, buf+offset, size);
		if (result < 0) 
			break;
		if (result == 0)
		{
			gettimeofday(&now, NULL);
			if (now.tv_sec > timeout.tv_sec || (now.tv_sec == timeout.tv_sec && now.tv_usec > timeout.tv_usec))
				break;
			nanosleep(&tm, NULL); //sleep for 1 microsecond
			continue;
		}
		size -= result;
		offset += result;
	}
	return offset;
}

#elif defined(USELIBFTDIONE)

#elif defined(USELIBFTD2XX)

#endif
