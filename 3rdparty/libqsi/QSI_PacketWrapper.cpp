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
QSI_PacketWrapper::QSI_PacketWrapper()
{
	m_log = new QSILog(_T("QSIINTERFACELOG.TXT"), _T("LOGUSBTOFILE"), _T("PACKET"));
}

//////////////////////////////////////////////////////////////////////////////////////////
QSI_PacketWrapper::~QSI_PacketWrapper()
{
	delete m_log;
}

//////////////////////////////////////////////////////////////////////////////////////////
int QSI_PacketWrapper::PKT_CheckQueues( IHostIO * con )
{
	// Declare variables
	int AmountInRxQueue = 0, AmountInTxQueue = 0;

	// Get number of characters in Rx and Tx queues
	m_iStatus = con->GetReadWriteQueueStatus(&AmountInRxQueue, &AmountInTxQueue);
	if (m_iStatus != ALL_OK)
		return m_iStatus + ERR_PKT_CheckQueuesFailed;

	// Return error if both or one of the queues is dirty
	if( (AmountInRxQueue != 0) && (AmountInTxQueue != 0) )
		return ERR_PKT_BothQueuesDirty;
	else if (AmountInRxQueue != 0)
	{
		do
		{
			int AmountRead;
			unsigned char * ucReadbuf = new unsigned char[AmountInRxQueue];
			con->Read(ucReadbuf, AmountInRxQueue, &AmountRead);
			m_log->Write(2, _T("*** Dirty Read Queue with %d pending in queue. Dumping data: ***"), AmountInRxQueue);
			m_log->WriteBuffer(2, (void *)ucReadbuf, AmountInRxQueue, AmountRead, 256);
			m_log->Write(2, _T("*** End Dirty Single Read Queue Dump, (there may be more remaining...) ***"));
			delete[] ucReadbuf;
			usleep(1000*100);	// Allow for more pending data to accumulate on host.
			con->GetReadWriteQueueStatus(&AmountInRxQueue, &AmountInTxQueue);
		} while (AmountInRxQueue != 0);
		return ERR_PKT_RxQueueDirty;
	}
	else if (AmountInTxQueue != 0)
	  return ERR_PKT_TxQueueDirty;

	return ALL_OK;
}

//////////////////////////////////////////////////////////////////////////////////////////
int QSI_PacketWrapper::PKT_SendPacket( IHostIO * con, unsigned char * pvTxBuffer, unsigned char * pvRxBuffer, 
                                      bool bPostCheckQueues, IOTimeout ioTimeout  )
{
	// Declare variables
	UCHAR *ucTxBuffer = (UCHAR*) pvTxBuffer,  // UCHAR pointer to Tx Buffer
		*ucRxBuffer = (UCHAR*) pvRxBuffer,  // UCHAR pointer to Rx Buffer
		ucTxCommand = 0,      // Holds Tx packet command byte
		ucRxCommand = 0;      // Holds Rx packet command byte

	int dwBytesToWrite = 0,   // Holds # of bytes to write for Tx packet
		dwBytesWritten = 0,   // Holds # of bytes written of Tx packet
		dwBytesToRead = 0,    // Holds # of bytes to read for Rx packet
		dwBytesReturned = 0;  // Holds # of bytes read of Rx packet

	// Make sure we're starting with clean queues
	m_iStatus = PKT_CheckQueues(con);
	if (m_iStatus != ALL_OK)
		goto SendPacketExit;

	// Get command and length from Tx packet
	ucTxCommand = *(ucTxBuffer+PKT_COMMAND);
	dwBytesToWrite = *(ucTxBuffer+PKT_LENGTH) + PKT_HEAD_LENGTH;

	// Make sure Tx packet isn't greater than allowed
	if (dwBytesToWrite + (unsigned int)PKT_HEAD_LENGTH > (unsigned int)MAX_PKT_LENGTH)
	{
		m_iStatus = ERR_PKT_TxPacketTooLong;
		goto SendPacketExit;
	}

	m_log->Write(2, _T("***Send Request Packet*** %d bytes total length. Packet Data Follows:"), dwBytesToWrite);
	m_log->WriteBuffer(2, ucTxBuffer, dwBytesToWrite, dwBytesToWrite, 256);
	m_log->Write(2, _T("***Send Request Packet*** Done"));

	if (ioTimeout != IOTimeout_Normal)
	{
		m_log->Write(2, _T("***Reset timeout***"));
		con->SetIOTimeout(ioTimeout);
	}
	// Write Tx packet to buffer
	m_iStatus = con->Write(ucTxBuffer, dwBytesToWrite, &dwBytesWritten);
	if (m_iStatus != ALL_OK)
	{
		m_log->Write(2, _T("***USB Write Error %d***"), m_iStatus);
		m_iStatus += ERR_PKT_TxFailed;
		goto SendPacketExit;
	}

	// Make sure the entire Tx packet was sent
	if (dwBytesWritten == 0)
	{
		m_log->Write(2, _T("***Zero Bytes Written!***"));
		m_iStatus = ERR_PKT_TxNone;
		goto SendPacketExit;
	}
	else if (dwBytesWritten < dwBytesToWrite)
	{
		m_log->Write(2, _T("***Not Enough Bytes Written!*** Write Request: %d, Written %d"), dwBytesToWrite, dwBytesWritten);
		m_iStatus = ERR_PKT_TxTooLittle;
		goto SendPacketExit;
	}

	//////////////////////////////////////////////////////////////////////////////////////////////////////////////
	  
	// Read command and length of Rx packet
	m_iStatus = con->Read(ucRxBuffer, PKT_HEAD_LENGTH, &dwBytesReturned);
	if (m_iStatus != ALL_OK)
	{
		m_iStatus += ERR_PKT_RxHeaderFailed;
		goto SendPacketExit;
	}
	// Make sure the entire Rx packet header was read
	if (dwBytesReturned != (unsigned int)PKT_HEAD_LENGTH)
	{
		m_iStatus = ERR_PKT_RxHeaderFailed;
		goto SendPacketExit;
	}

	// Get command and length from Rx packet
	ucRxCommand = *(ucRxBuffer+PKT_COMMAND);
	dwBytesToRead = (DWORD) *(ucRxBuffer+PKT_LENGTH);

	// Make sure Tx and Rx commands match
	if (ucTxCommand != ucRxCommand)
	{
		m_iStatus = ERR_PKT_RxBadHeader;
		goto SendPacketExit;
	}

	// Make sure Rx packet isn't greater than allowed
	if (dwBytesToRead + (unsigned int)PKT_HEAD_LENGTH > (unsigned int)MAX_PKT_LENGTH)
	{
		m_iStatus = ERR_PKT_RxPacketTooLong;
		goto SendPacketExit;
	}

	// Get remaining data of Rx packet
	m_iStatus = con->Read(ucRxBuffer+PKT_HEAD_LENGTH, dwBytesToRead, &dwBytesReturned);
	if (m_iStatus != ALL_OK)
	{
		m_iStatus += ERR_PKT_RxFailed;
		goto SendPacketExit;
	}

	// Make sure the entire Rx packet was read
	if (dwBytesReturned == 0)
	{
		m_iStatus = ERR_PKT_RxNone;
		goto SendPacketExit;
	}
	else if (dwBytesReturned < dwBytesToRead)
	{
		m_iStatus = ERR_PKT_RxTooLittle;
		goto SendPacketExit;
	}	

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
		m_iStatus = PKT_CheckQueues(con);
	}

	// Common Exit routine to insure IOTimeouts get reset on errors.
SendPacketExit:
  	if (ioTimeout != IOTimeout_Normal || m_iStatus != ALL_OK) con->SetIOTimeout(IOTimeout_Normal);
	return m_iStatus;
}

//////////////////////////////////////////////////////////////////////////////////////////
int QSI_PacketWrapper::PKT_ReadBlock( IHostIO * con, PVOID pRxBuffer, int iBytesToRead, int * iBytesReturned )
{
  // Declare variables

  int iReadBytesReturned = 0;
  int iTotalBytesRead = 0;

  *iBytesReturned = 0;
  // Make sure the entire Rx packet was read
  int retry = 2;
  while (iTotalBytesRead < iBytesToRead)
  {
	  iReadBytesReturned = 0;
	  m_iStatus = con->Read((BYTE*)pRxBuffer + iTotalBytesRead, iBytesToRead - iTotalBytesRead,  &iReadBytesReturned);
	  if (m_iStatus != ALL_OK)
		  return m_iStatus + ERR_PKT_BlockRxFailed;
	  
	  iTotalBytesRead += iReadBytesReturned;
  }
  if (iBytesToRead != iTotalBytesRead)
	  return ERR_PKT_BlockRxTooLittle;
	
  *iBytesReturned = iTotalBytesRead;
  return ALL_OK;
}

