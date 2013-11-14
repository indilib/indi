// CameraIO_PCI.cpp: implementation of the CCameraIO_PCI class.
//
// Copyright (c) 2000 Apogee Instruments Inc.
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include <winioctl.h>

#include "ApogeeLinux.h"        // This defines the IOCTL constants.
#include "CameraIO_PCI.h"
#include "time.h"


//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CCameraIO_PCI::CCameraIO_PCI()
{
	m_IsWDM		= false;
	m_hDriver	= NULL;
}

CCameraIO_PCI::~CCameraIO_PCI()
{
	CloseDriver();
}

long CCameraIO_PCI::Read(unsigned short reg, unsigned short& val)
{
	BOOLEAN IoctlResult;
	ULONG	ReturnedLength;
	USHORT	RegNumber;
	USHORT	ReadBuffer;

	switch ( reg )
	{
	case Reg_Command:
		RegNumber = RegPCI_CommandRead;
		break;
	case Reg_Timer:
		RegNumber = RegPCI_TimerRead;
		break;
	case Reg_VBinning:
		RegNumber = RegPCI_VBinningRead;
		break;
	case Reg_AICCounter:
		RegNumber = RegPCI_AICCounterRead;
		break;
	case Reg_TempSetPoint:
		RegNumber = RegPCI_TempSetPointRead;
		break;
	case Reg_PixelCounter:
		RegNumber = RegPCI_PixelCounterRead;
		break;
	case Reg_LineCounter:
		RegNumber = RegPCI_LineCounterRead;
		break;
	case Reg_BICCounter:
		RegNumber = RegPCI_BICCounterRead;
		break;
	case Reg_ImageData:
		RegNumber = RegPCI_ImageData;
		break;
	case Reg_TempData:
		RegNumber = RegPCI_TempData;
		break;
	case Reg_Status:
		RegNumber = RegPCI_Status;
		break;
	case Reg_CommandReadback:
		RegNumber = RegPCI_CommandReadback;
		break;
	default:
		_ASSERT( FALSE );	// Application program bug
		val = 0;
		return 0;
	}

	if ( m_IsWDM )
	{
		IoctlResult = DeviceIoControl(
			m_hDriver,					// Handle to device
			IOCTL_WDM_READ_PCI_USHORT,	// IO Control code for Read
			&RegNumber,					// Buffer to driver.
			sizeof(RegNumber),			// Length of buffer in bytes.
			&ReadBuffer,				// Buffer from driver.
			sizeof(ReadBuffer),			// Length of buffer in bytes.
			&ReturnedLength,			// Bytes placed in DataBuffer.
			NULL						// NULL means wait till op. completes.
			);
	}
	else
	{
		IoctlResult = DeviceIoControl(
			m_hDriver,					// Handle to device
			IOCTL_GPD_READ_PCI_USHORT,	// IO Control code for Read
			&RegNumber,					// Buffer to driver.
			sizeof(RegNumber),			// Length of buffer in bytes.
			&ReadBuffer,				// Buffer from driver.
			sizeof(ReadBuffer),			// Length of buffer in bytes.
			&ReturnedLength,			// Bytes placed in DataBuffer.
			NULL						// NULL means wait till op. completes.
			);
	}

	if ( (ReturnedLength != 2) || (IoctlResult == FALSE) )
	{
		return 1;
	}

	val = ReadBuffer;
	
	return 0;
}

long CCameraIO_PCI::Write(unsigned short reg, unsigned short val)
{
	BOOLEAN IoctlResult;
	ULONG	InBuffer[2];
	ULONG	ReturnedLength;
	USHORT	RegNumber;

	switch ( reg )
	{
	case Reg_Command:
		RegNumber = RegPCI_Command;
		break;
	case Reg_Timer:
		RegNumber = RegPCI_Timer;
		break;
	case Reg_VBinning:
		RegNumber = RegPCI_VBinning;
		break;
	case Reg_AICCounter:
		RegNumber = RegPCI_AICCounter;
		break;
	case Reg_TempSetPoint:
		RegNumber = RegPCI_TempSetPoint;
		break;
	case Reg_PixelCounter:
		RegNumber = RegPCI_PixelCounter;
		break;
	case Reg_LineCounter:
		RegNumber = RegPCI_LineCounter;
		break;
	case Reg_BICCounter:
		RegNumber = RegPCI_BICCounter;
		break;
	default:
		_ASSERT ( false );
		return 0;
	}

	InBuffer[0] = RegNumber;
	InBuffer[1] = val;

	// Do an I/O write		
	if ( m_IsWDM )
	{
		IoctlResult = DeviceIoControl(
			m_hDriver,					// Handle to device
			IOCTL_WDM_WRITE_PCI_USHORT,	// IO Control code for Write
			&InBuffer,					// Buffer to driver.  Holds register/data.
			sizeof ( InBuffer ),		// Length of buffer in bytes.
			NULL,						// Buffer from driver.   Not used.
			0,							// Length of buffer in bytes.
			&ReturnedLength,			// Bytes placed in outbuf.  Should be 0.
			NULL						// NULL means wait till I/O completes.
			);
	}
	else
	{
		IoctlResult = DeviceIoControl(
			m_hDriver,					// Handle to device
			IOCTL_GPD_WRITE_PCI_USHORT,	// IO Control code for Write
			&InBuffer,					// Buffer to driver.  Holds register/data.
			sizeof ( InBuffer ),		// Length of buffer in bytes.
			NULL,						// Buffer from driver.   Not used.
			0,							// Length of buffer in bytes.
			&ReturnedLength,			// Bytes placed in outbuf.  Should be 0.
			NULL						// NULL means wait till I/O completes.
			);
	}

	if ( (IoctlResult == FALSE) || (ReturnedLength != 0) )
	{
		return 1;
	}

	return 0;

}

long CCameraIO_PCI::ReadLine( long SkipPixels, long Pixels, unsigned short* pLineBuffer )
{
	BOOLEAN	IoctlResult;
	ULONG	InBuffer[3];
	ULONG	ReturnedLength; // Number of bytes returned in output buffer
	ULONG	NumBytes;
	USHORT*	DataBuffer;

	InBuffer[0] = RegPCI_ImageData;
	InBuffer[1] = SkipPixels;			// Data points to skip
	InBuffer[2] = Pixels;				// Data points to keep
	
	NumBytes	= Pixels * sizeof( unsigned short );
	DataBuffer	= pLineBuffer;

	if ( !m_TDI )
	{
		/////////////////////////////////////
		// Clock out the line
		m_RegShadow[ Reg_Command ] |= RegBit_StartNextLine;		// set bit to 1
		Write( Reg_Command, m_RegShadow[ Reg_Command ] );
		
		m_RegShadow[ Reg_Command ] &= ~RegBit_StartNextLine;	// set bit to 0
		Write( Reg_Command, m_RegShadow[ Reg_Command ] );
		/////////////////////////////////////
	}

	if ( m_IsWDM )
	{
		IoctlResult = DeviceIoControl(
			m_hDriver,					// Handle to device
			IOCTL_WDM_READ_PCI_LINE,	// IO Control code for Read line
			&InBuffer,					// Buffer to driver.
			sizeof(InBuffer),			// Length of buffer in bytes.
			DataBuffer,					// Buffer from driver.
			NumBytes,					// Length of buffer in bytes.
			&ReturnedLength,			// Bytes placed in DataBuffer.
			NULL						// NULL means wait till op. completes.
			);
	}
	else
	{
		IoctlResult = DeviceIoControl(
			m_hDriver,					// Handle to device
			IOCTL_GPD_READ_PCI_LINE,	// IO Control code for Read line
			&InBuffer,					// Buffer to driver.
			sizeof(InBuffer),			// Length of buffer in bytes.
			DataBuffer,					// Buffer from driver.
			NumBytes,					// Length of buffer in bytes.
			&ReturnedLength,			// Bytes placed in DataBuffer.
			NULL						// NULL means wait till op. completes.
			);
	}

	if ( (ReturnedLength != NumBytes) || (!IoctlResult) )
	{
		return 1;					// Failed to get line info
	}

	/////////////////////////////////////
	// Assert done reading line
	m_RegShadow[ Reg_Command ] |= RegBit_DoneReading;	// set bit to 1
	Write( Reg_Command, m_RegShadow[ Reg_Command ] );
	
	m_RegShadow[ Reg_Command ] &= ~RegBit_DoneReading;	// set bit to 0
	Write( Reg_Command, m_RegShadow[ Reg_Command ] );
	/////////////////////////////////////

	if ( !m_TDI )
	{
		/////////////////////////////////////
		// Wait until camera is done
		clock_t StopTime = clock() + CLOCKS_PER_SEC;	// wait at most one second
		while ( true )
		{
			unsigned short val = 0;
			Read( Reg_Status, val );
			if ( ( val & RegBit_LineDone ) != 0 ) break;// Line done
			
			if ( clock() > StopTime ) return 1;		// Timed out
		}
	}

	return 0;
}

bool CCameraIO_PCI::InitDriver()
{
	OSVERSIONINFO	OSVerInfo;
	BOOLEAN			IsPostWin98OS;
	BOOLEAN			IsNT4OS;
	BOOLEAN			IsPostNT4OS;

	IsPostWin98OS	= false;
	IsNT4OS			= false;
	IsPostNT4OS		= false;

	CloseDriver();

	OSVerInfo.dwOSVersionInfoSize  = sizeof ( OSVERSIONINFO );
	GetVersionEx( &OSVerInfo );

	// Check for Win9x versions.  Pre-Win98 is unsupported.
	if ( OSVerInfo.dwPlatformId == VER_PLATFORM_WIN32_WINDOWS ) 
	{
		// Check for pre-Win98
		if (( OSVerInfo.dwMajorVersion < 4 ) ||
			(( OSVerInfo.dwMajorVersion == 4 ) && ( OSVerInfo.dwMinorVersion == 0 )))
		{
			return false;		// Pre-Win98 not supported
		}
		else
		{
			IsPostWin98OS = true;
		}

	}
	else if ( OSVerInfo.dwPlatformId == VER_PLATFORM_WIN32_NT )
	{
		// Check if NT4
		if ( OSVerInfo.dwMajorVersion < 4 )
		{
			// NT3.51 is not supported.  Right??
			return false;
		}
		else if (OSVerInfo.dwMajorVersion == 4 )
		{
			IsNT4OS = true;
		}
		else if (OSVerInfo.dwMajorVersion > 4 )
		{
			IsPostNT4OS = true;
		}
	}

	if ( IsNT4OS )
	{
		ULONG ReturnedLength;
		ULONG DataBuffer[2];

		// Open the driver
		m_hDriver = CreateFile(
			"\\\\.\\ApogeeIO",
			GENERIC_WRITE | GENERIC_READ,
			FILE_SHARE_WRITE | FILE_SHARE_READ,
			NULL,
			OPEN_EXISTING,
			0,
			NULL);

		if ( m_hDriver == INVALID_HANDLE_VALUE )
		{
			m_hDriver = NULL;
			return false;
		}
		
		BOOL IoctlResult = DeviceIoControl(
			m_hDriver,				// Handle to device
			IOCTL_PCI_BUS_SCAN,		// IO Control code for PCI Bus Scan
			NULL,					// Buffer to driver.
			0,						// Length of buffer in bytes.
			DataBuffer,				// Buffer from driver.
			sizeof( DataBuffer ),	// Length of buffer in bytes.
			&ReturnedLength,		// Bytes placed in DataBuffer.
			NULL					// NULL means wait till op. completes.
			);

		if ( (!IoctlResult) || (ReturnedLength != sizeof(DataBuffer)) )
		{
			return false;
		}
	}
	else if ( IsPostWin98OS || IsPostNT4OS )
	{
		// Should be okay to use the WDM driver.  Note that the kernel 
		// driver will actually check to see if WDM services are available

		// Open the driver
		m_hDriver = CreateFile(
			"\\\\.\\ApPCI",
			GENERIC_WRITE | GENERIC_READ,
			FILE_SHARE_WRITE | FILE_SHARE_READ,
			NULL,
			OPEN_EXISTING,
			0,
			NULL
			);

		if ( m_hDriver == INVALID_HANDLE_VALUE )
		{
			m_hDriver = NULL;
			return false;
		}

		// Safe to assume we're using the WDM driver at this point.
		m_IsWDM = true;
	}

	return true;
}

void CCameraIO_PCI::CloseDriver()
{    
	// Close the driver if it already exists
	if ( m_hDriver != NULL ) 
	{
		CloseHandle ( m_hDriver );
	}

	m_hDriver = NULL;
}
