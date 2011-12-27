/*****************************************************************************************
NAME
 PacketWrapper

DESCRIPTION
 Packet interface

COPYRIGHT (C)
 QSI (Quantum Scientific Imaging) 2005-2006

REVISION HISTORY
 MWB 04.28.06 Original Version
 *****************************************************************************************/

#include "QSI_PacketWrapper.h"
#include "QSI_Registry.h"

//////////////////////////////////////////////////////////////////////////////////////////
QSI_PacketWrapper::QSI_PacketWrapper() : 	QSI_USBWrapper()
{

}

//////////////////////////////////////////////////////////////////////////////////////////
QSI_PacketWrapper::~QSI_PacketWrapper()
{

}

//////////////////////////////////////////////////////////////////////////////////////////
int QSI_PacketWrapper::PKT_OpenDevice( CameraID pcID )
{
	// Open device by serial number
	if (pcID.InterfaceName != "USB")
		return ERR_PKT_OpenFailed;
#ifdef USEFTDILIB
	m_iStatus = USB_OpenEx( pcID, 0);
#else
	m_iStatus = USB_OpenEx( pcID );
#endif

	if (m_iStatus != ALL_OK) 
	  return m_iStatus + ERR_PKT_OpenFailed;

	QSI_Registry rReg;

	DWORD dwUSBInSize = rReg.GetUSBInSize( USB_IN_TRANSFER_SIZE );
	if (dwUSBInSize < 1000) 
		dwUSBInSize = 0;		//Zero means leave at default value

	DWORD dwUSBOutSize = rReg.GetUSBOutSize( USB_OUT_TRANSFER_SIZE );
	if (dwUSBOutSize < 1000) 
		dwUSBOutSize = 0;		//Zero means leave at default value

	// Set USB Driver Buffer Sizes
	m_iStatus = USB_SetUSBParameters(dwUSBInSize, dwUSBOutSize);
	if (m_iStatus != ALL_OK) 
		return m_iStatus + ERR_PKT_SetUSBParmsFailed;
	
	// Set timeouts
	int iReadTO = rReg.GetUSBReadTimeout( READ_TIMEOUT );
	int iWriteTO = rReg.GetUSBWriteTimeout( WRITE_TIMEOUT );

	m_iStatus = USB_SetTimeouts(iReadTO, iWriteTO);
	if (m_iStatus != ALL_OK) 
	  return m_iStatus + ERR_PKT_SetTimeOutFailed;

#ifdef USEFTDILIB
	m_iStatus = USB_Purge(0);
#else
	m_iStatus = USB_Purge(FT_PURGE_TX | FT_PURGE_TX);
#endif

	if (m_iStatus != ALL_OK) 	
		return m_iStatus + ERR_PKT_ResetDeviceFailed;

	return ALL_OK;
}

//////////////////////////////////////////////////////////////////////////////////////////
int QSI_PacketWrapper::PKT_CloseDevice( void )
{
  // Close current device
  m_iStatus = USB_Close();
  if (m_iStatus != ALL_OK) 
	  return m_iStatus + ERR_PKT_CloseFailed;

  return ALL_OK;
}

//////////////////////////////////////////////////////////////////////////////////////////
int QSI_PacketWrapper::PKT_CheckQueues( void )
{
	// Declare variables
	DWORD AmountInRxQueue = 0, AmountInTxQueue = 0;

	// Get number of characters in Rx and Tx queues
	m_iStatus = USB_GetStatus(&AmountInRxQueue, &AmountInTxQueue);
	if (m_iStatus != ALL_OK)
		return m_iStatus + ERR_PKT_CheckQueuesFailed;

	// Return error if both or one of the queues is dirty
	if( (AmountInRxQueue != 0) && (AmountInTxQueue != 0) )
		return ERR_PKT_BothQueuesDirty;
	else if (AmountInRxQueue != 0)
	{
		do
		{
			DWORD AmountRead;
			unsigned char * ucReadbuf = new unsigned char[AmountInRxQueue];
			USB_Read(ucReadbuf, AmountInRxQueue, &AmountRead);
			m_log->Write(2, _T("*** Dirty Read Queue with %d pending in queue. Dumping data: ***"), AmountInRxQueue);
			m_log->WriteBuffer(2, (void *)ucReadbuf, AmountInRxQueue, AmountRead, 256);
			m_log->Write(2, _T("*** End Dirty Single Read Queue Dump, (there may be more remaining...) ***"));
			delete[] ucReadbuf;
			usleep(1000*100);	// Allow for more pending data to accumulate on host.
			USB_GetStatus(&AmountInRxQueue, &AmountInTxQueue);
		} while (AmountInRxQueue != 0);
		return ERR_PKT_RxQueueDirty;
	}
	else if (AmountInTxQueue != 0)
	  return ERR_PKT_TxQueueDirty;

	return ALL_OK;
}

//////////////////////////////////////////////////////////////////////////////////////////
int QSI_PacketWrapper::PKT_SendPacket( PVOID pvTxBuffer, PVOID pvRxBuffer, bool bPostCheckQueues )
{
  // Declare variables
  UCHAR *ucTxBuffer = (UCHAR*) pvTxBuffer,  // UCHAR pointer to Tx Buffer
        *ucRxBuffer = (UCHAR*) pvRxBuffer,  // UCHAR pointer to Rx Buffer
        ucTxCommand = 0,      // Holds Tx packet command byte
        ucRxCommand = 0;      // Holds Rx packet command byte

  DWORD dwBytesToWrite = 0,   // Holds # of bytes to write for Tx packet
        dwBytesWritten = 0,   // Holds # of bytes written of Tx packet
        dwBytesToRead = 0,    // Holds # of bytes to read for Rx packet
        dwBytesReturned = 0;  // Holds # of bytes read of Rx packet

  // Make sure we're starting with clean queues
  m_iStatus = PKT_CheckQueues();
  if (m_iStatus != ALL_OK) 
	  return m_iStatus;
  
  // Get command and length from Tx packet
  ucTxCommand = *(ucTxBuffer+PKT_COMMAND);
  dwBytesToWrite = *(ucTxBuffer+PKT_LENGTH) + PKT_HEAD_LENGTH;
  
  // Make sure Tx packet isn't greater than allowed
  if (dwBytesToWrite + (unsigned int)PKT_HEAD_LENGTH > (unsigned int)MAX_PKT_LENGTH) 
	  return ERR_PKT_TxPacketTooLong;
  
  m_log->Write(2, _T("***Send Request Packet*** %d bytes total length. Packet Data Follows:"), dwBytesToWrite);
  m_log->WriteBuffer(2, ucTxBuffer, dwBytesToWrite, dwBytesToWrite, 256);
  m_log->Write(2, _T("***Send Request Packet*** Done"));
	
  // Write Tx packet to buffer
  m_iStatus = USB_Write(ucTxBuffer, dwBytesToWrite, &dwBytesWritten);
  if (m_iStatus != ALL_OK) 
	  return m_iStatus + ERR_PKT_TxFailed;
  
  // Make sure the entire Tx packet was sent
  if (dwBytesWritten == 0) 
	  return ERR_PKT_TxNone;
  else if (dwBytesWritten < dwBytesToWrite) 
	  return ERR_PKT_TxTooLittle;
  
  //////////////////////////////////////////////////////////////////////////////////////////////////////////////
	  
  // Read command and length of Rx packet
  m_iStatus = USB_Read(ucRxBuffer, PKT_HEAD_LENGTH, &dwBytesReturned);
  if (m_iStatus != ALL_OK)
  {
    //AfxMessageBox ( "ERROR", 0, 0 );
    return m_iStatus + ERR_PKT_RxHeaderFailed;
  }
  // Make sure the entire Rx packet header was read
  if (dwBytesReturned != (unsigned int)PKT_HEAD_LENGTH) 
	  return ERR_PKT_RxHeaderFailed;

  // Get command and length from Rx packet
  ucRxCommand = *(ucRxBuffer+PKT_COMMAND);
  dwBytesToRead = (DWORD) *(ucRxBuffer+PKT_LENGTH);
  
  // Make sure Tx and Rx commands match
  if (ucTxCommand != ucRxCommand) 
	  return ERR_PKT_RxBadHeader;
  
  // Make sure Rx packet isn't greater than allowed
  if (dwBytesToRead + (unsigned int)PKT_HEAD_LENGTH > (unsigned int)MAX_PKT_LENGTH) 
	  return ERR_PKT_RxPacketTooLong;

  // Get remaining data of Rx packet
  m_iStatus = USB_Read(ucRxBuffer+PKT_HEAD_LENGTH, dwBytesToRead, &dwBytesReturned);
  if (m_iStatus != ALL_OK) 
	  return m_iStatus + ERR_PKT_RxFailed;
  
  // Make sure the entire Rx packet was read
  if (dwBytesReturned == 0) 
	  return ERR_PKT_RxNone;
  else if (dwBytesReturned < dwBytesToRead) 
	  return ERR_PKT_RxTooLittle;
  
  m_log->Write(2, _T("***Read Request Packet Response*** %d bytes total length. Packet Data Follows:"), dwBytesReturned+2);
  m_log->WriteBuffer(2, ucRxBuffer, dwBytesReturned+2, dwBytesReturned+2, 256);
  m_log->Write(2, _T("***Read Request Packet Response*** Done."));

  // Make sure queues are clean
  //
  // if the read image command doesn't happen fast enough, or if the camera is very fast
  // returning from a transfer image or AutoZero request, this could throw an error.
  // This should only be done at points where the camera should be idle.
  // To avoid a race condition with the camera this can only be done on all commands except 
  // TransferImage and AutoZero.
  //
  // This is implemented in those routines in QSI_Interface by setting bPostCheckQueues false
  //

  if (bPostCheckQueues)
  {
	  // Make sure queues are clean
	  m_iStatus = PKT_CheckQueues();
	  if (m_iStatus != ALL_OK) 
		  return m_iStatus;
  }
  
  return ALL_OK;
}

//////////////////////////////////////////////////////////////////////////////////////////
int QSI_PacketWrapper::PKT_ReadBlock( PVOID pvRxBuffer, DWORD dwBytesToRead )
{
  // Declare variables
  UCHAR *pucRxBuffer = (UCHAR*) pvRxBuffer;
  DWORD dwBytesReturned = 0;
  DWORD dwTotalBytesRead = 0;

  // Make sure the entire Rx packet was read
  int retry = 2;
  while (dwTotalBytesRead < dwBytesToRead)
  {
	  DWORD dwTotBytesReturned;
	  dwBytesReturned = 0;
	  m_iStatus = USB_Read(pucRxBuffer + dwTotalBytesRead, dwBytesToRead - dwTotalBytesRead,  &dwBytesReturned);
	  if (m_iStatus != ALL_OK)
		  return m_iStatus + ERR_PKT_BlockRxFailed;
	  dwTotalBytesRead += dwBytesReturned;
  }
  if (dwBytesToRead != dwTotalBytesRead)
	  return ERR_PKT_BlockRxTooLittle;
  
  return ALL_OK;
}
