//////////////////////////////////////////////////////////////////////
//
// ApnCamera_NET.cpp: implementation of the CApnCamera_NET class.
//
// Copyright (c) 2003, 2004 Apogee Instruments, Inc.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "ApnCamera_NET.h"

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "ApogeeNet.h"
#include "ApogeeNetErr.h"


//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////


bool CApnCamera::GetDeviceHandle( void *hCamera, char *CameraInfo )
{

	strcpy( CameraInfo, m_HostAddr );

	return true;
}

bool CApnCamera::SimpleInitDriver(	unsigned long	CamIdA, 
										unsigned short	CamIdB, 
										unsigned long	Option )
{
	BYTE			ipAddr[4];


	ipAddr[0] = (BYTE)(CamIdA & 0xFF);
	ipAddr[1] = (BYTE)((CamIdA >> 8) & 0xFF);
	ipAddr[2] = (BYTE)((CamIdA >> 16) & 0xFF);
	ipAddr[3] = (BYTE)((CamIdA >> 24) & 0xFF);

	sprintf( m_HostAddr, "%u.%u.%u.%u:%u", ipAddr[3], ipAddr[2], ipAddr[1], ipAddr[0], CamIdB );

	if ( ApnNetConnect(  m_HostAddr ) != APN_NET_SUCCESS )
	{
		return false;
	}

	return true;
}


bool CApnCamera::InitDriver( unsigned long	CamIdA, 
								 unsigned short CamIdB, 
								 unsigned long	Option )
{
	BYTE			ipAddr[4];


	ipAddr[0] = (BYTE)(CamIdA & 0xFF);
	ipAddr[1] = (BYTE)((CamIdA >> 8) & 0xFF);
	ipAddr[2] = (BYTE)((CamIdA >> 16) & 0xFF);
	ipAddr[3] = (BYTE)((CamIdA >> 24) & 0xFF);

	sprintf( m_HostAddr, "%u.%u.%u.%u:%u", ipAddr[3], ipAddr[2], ipAddr[1], ipAddr[0], CamIdB );

	if ( ApnNetConnect(  m_HostAddr ) != APN_NET_SUCCESS )
	{
		return false;
	}

	m_ImageSizeBytes	= 0;
	m_ImageInProgress	= false;

	// Before trying to initialize, perform a simple loopback test
	unsigned short	RegData;
	unsigned short	NewRegData;

	RegData = 0x5AA5;
	if ( Write( FPGA_REG_SCRATCH, RegData )		!= APN_NET_SUCCESS ) return false;
	if ( Read( FPGA_REG_SCRATCH, NewRegData )	!= APN_NET_SUCCESS ) return false;		
	if ( RegData != NewRegData ) return false;

	RegData = 0xA55A;
	if ( Write( FPGA_REG_SCRATCH, RegData )		!= APN_NET_SUCCESS ) return false;
	if ( Read( FPGA_REG_SCRATCH, NewRegData )	!= APN_NET_SUCCESS ) return false;		
	if ( RegData != NewRegData ) return false;

	// The loopback test was successful.  Proceed with initialization.
	if ( InitDefaults() != 0 )
		return false;

	return true;
}


Apn_Interface CApnCamera::GetCameraInterface()
{
	return Apn_Interface_NET;
}


long CApnCamera::GetCameraSerialNumber( char *CameraSerialNumber, long *BufferLength )
{
	char pBuffer[256];


	if ( *BufferLength < (MAC_ADDRESS_LENGTH + 1) )
	{
		*BufferLength = 0;

		return CAPNCAMERA_ERR_SN;
	}

	if ( ApnNetGetMacAddress(  m_HostAddr, pBuffer ) != APN_NET_SUCCESS )
	{
		*BufferLength = 0;

		return CAPNCAMERA_ERR_SN;
	}
	else
	{
		strcpy( CameraSerialNumber, pBuffer );
		*BufferLength = strlen( CameraSerialNumber );
	}

	return CAPNCAMERA_SUCCESS;
}


long CApnCamera::GetSystemDriverVersion( char *SystemDriverVersion, long *BufferLength )
{
	strcpy( SystemDriverVersion, "N/A" );

	*BufferLength = strlen( SystemDriverVersion );

	return CAPNCAMERA_SUCCESS;
}


long CApnCamera::GetUsb8051FirmwareRev( char *FirmwareRev, long *BufferLength )
{
	strcpy( FirmwareRev, "N/A" );

	*BufferLength = strlen( FirmwareRev );

	return CAPNCAMERA_SUCCESS;
}


long CApnCamera::GetUsbProductId( unsigned short *pProductId )
{
	*pProductId = 0x0;

	return CAPNCAMERA_SUCCESS;
}


long CApnCamera::GetUsbDeviceId( unsigned short *pDeviceId )
{
	*pDeviceId = 0x0;

	return CAPNCAMERA_SUCCESS;
}


bool CApnCamera::CloseDriver()
{
	ApnNetClose(  m_HostAddr );

	if ( m_ImageInProgress )
	{
		m_ImageInProgress = false;
	}

	return true;
}


long CApnCamera::GetImageData( unsigned short *pImageBuffer, 
								   unsigned short &Width,
								   unsigned short &Height,
								   unsigned long  &Count )
{
	unsigned short	Offset;
	unsigned short	*pTempBuffer;
	unsigned long	SequenceHeight;
	unsigned long	i, j;

	
	// Check to see if ApnNetStartExp was called first 
	if ( !m_ImageInProgress )
	{
		return CAPNCAMERA_ERR_IMAGE;		// Failure -- Image never started
	}

	// Make sure it is okay to get the image data
	// The app *should* have done this on its own, but we have to make sure
	while ( !ImageReady() )
	{
		Sleep( 50 );
		read_ImagingStatus();
	}

	Width	= m_pvtExposeWidth;
	Height	= m_pvtExposeHeight;

	if ( m_pvtBitsPerPixel == 16 )
		Offset = 1;

	if ( m_pvtBitsPerPixel == 12 )
		Offset = 10;

	Width -= Offset;	// Calculate the true image width

	SequenceHeight = Height * m_pvtNumImages;

	pTempBuffer = new unsigned short[(Width+Offset) * SequenceHeight];
	
	ApnNetGetImageTcp(  m_HostAddr, m_ImageSizeBytes, pTempBuffer );

	unsigned long TermOne;
	unsigned long TermTwo;

	for ( i=0; i<SequenceHeight; i++ )
	{
		TermOne = i*Width;
		TermTwo = (i*(Width+Offset))+Offset;

		for ( j=0; j<Width; j++ )
		{
			// pImageBuffer[(i*Width)+j] = pTempBuffer[(i*(Width+Offset))+j+Offset];
			pImageBuffer[TermOne+j] = pTempBuffer[TermTwo+j];
		}
	}

	delete [] pTempBuffer;

	Count = m_pvtNumImages;

	m_ImageInProgress = false;	

	SignalImagingDone();

	return CAPNCAMERA_SUCCESS;
}


long CApnCamera::GetLineData( unsigned short *pLineBuffer,
								  unsigned short &Size )
{
	Size = 0;

	return CAPNCAMERA_SUCCESS;
}


long CApnCamera::PreStartExpose( unsigned short BitsPerPixel )
{
	if ( (BitsPerPixel != 16) && (BitsPerPixel != 12) )
	{
		// Invalid bit depth request
		return CAPNCAMERA_ERR_START_EXP;
	}

	// Check to make sure an image isn't already in progress
	if ( m_ImageInProgress )
	{
		return CAPNCAMERA_ERR_START_EXP;	// Failure
	}

	m_pvtExposeWidth			= GetExposurePixelsH();
	// m_pvtExposeHeight			= GetExposurePixelsV();
	m_pvtNumImages				= read_ImageCount();
	m_pvtBitsPerPixel			= BitsPerPixel;
	m_pvtExposeExternalShutter	= read_ExternalShutter();
	m_pvtExposeCameraMode		= read_CameraMode();

	if ( m_pvtExposeCameraMode == Apn_CameraMode_TDI )
	{
		m_pvtExposeHeight		= read_TDIRows();
	}
	else
	{
		m_pvtExposeHeight		= GetExposurePixelsV();
	}

 
	if ( BitsPerPixel == 16 )
		m_pvtExposeWidth += 1;

	if ( BitsPerPixel == 12 )
		m_pvtExposeWidth += 10;

	// multiply the height by the ImageCount variable
	// ethernet sequences result in series of images concatenated together
	if ( ApnNetStartExp(  m_HostAddr, m_pvtExposeWidth, (m_pvtExposeHeight*m_pvtNumImages) ) != APN_NET_SUCCESS )
	{
		return CAPNCAMERA_ERR_START_EXP;
	}

	m_ImageSizeBytes	= m_pvtExposeWidth * m_pvtExposeHeight * m_pvtNumImages * 2;
	m_ImageInProgress	= true;

	return CAPNCAMERA_SUCCESS;
}


long CApnCamera::PostStopExposure( bool DigitizeData )
{
	unsigned short*	pRequestData;
	unsigned long	ByteCount;


	AltaDebugOutputString( "APOGEE.DLL - CApnCamera::PostStopExposure() -> BEGIN" );


	// All this call is really doing (at this point) is making sure that a valid connection
	// exists to the camera
	if ( ApnNetStopExp(  m_HostAddr, DigitizeData ) != APN_NET_SUCCESS )
	{
		return 1;
	}

	switch( m_pvtExposeCameraMode )
	{
		case Apn_CameraMode_Normal:

			// First, if we are not triggered in some manner, do a normal stop exposure routine
			// We check the condition "read_ImagingStatus() == Apn_Status_WaitingOnTrigger"
			// after this because we don't usually want to read ImagingStatus in the driver
			if ( !read_ExposureTriggerGroup() && !read_ExposureTriggerEach() && !read_ExposureExternalShutter() )
			{
				AltaDebugOutputString( "APOGEE.DLL - CApnCamera::PostStopExposure() -> Not using H/W trigger" );

				if ( !DigitizeData )
				{
					while ( !ImageReady() )
					{
						Sleep( 50 );
						read_ImagingStatus();
					}

					ByteCount		= m_pvtExposeWidth * m_pvtExposeHeight;
					pRequestData	= new unsigned short[ByteCount];

					if ( ApnNetGetImageTcp(  m_HostAddr, ByteCount, pRequestData ) != APN_NET_SUCCESS )
					{
						delete [] pRequestData;

						SignalImagingDone();
						m_ImageInProgress = false;

						return APN_NET_ERR_IMAGE_DATA;
					}

					delete [] pRequestData;

					SignalImagingDone();
					m_ImageInProgress = false;
				}
			}
			else
			{
				AltaDebugOutputString( "APOGEE.DLL - CApnCamera::PostStopExposure() -> Using H/W trigger" );

				if ( read_ImagingStatus() == Apn_Status_WaitingOnTrigger )
				{
					AltaDebugOutputString( "APOGEE.DLL - CApnCamera::PostStopExposure() -> Detected Apn_Status_WaitingOnTrigger" );

					SignalImagingDone();
					m_ImageInProgress = false;
					ResetSystem();
				}
				else
				{
					AltaDebugOutputString( "APOGEE.DLL - CApnCamera::PostStopExposure() -> Did NOT detect Apn_Status_WaitingOnTrigger" );

					if ( !DigitizeData )
					{
						while ( !ImageReady() )
						{
							Sleep( 50 );
							read_ImagingStatus();
						}

						ByteCount		= m_pvtExposeWidth * m_pvtExposeHeight;
						pRequestData	= new unsigned short[ByteCount];

						if ( ApnNetGetImageTcp(  m_HostAddr, ByteCount, pRequestData ) != APN_NET_SUCCESS )
						{
							delete [] pRequestData;

							SignalImagingDone();
							m_ImageInProgress = false;

							return APN_NET_ERR_IMAGE_DATA;
						}

						delete [] pRequestData;

						SignalImagingDone();
						m_ImageInProgress = false;
						
						if ( m_pvtExposeExternalShutter )
						{
							ResetSystem();
						}
					}
				}
			}
			break;
		case Apn_CameraMode_TDI:
			// Clean up after the stop

			// Restart the system to flush normally
			SignalImagingDone();
			m_ImageInProgress = false;
			ResetSystem();
			break;
		case Apn_CameraMode_ExternalTrigger:
			// Included for stopping "legacy" externally triggered exposures
			if ( !DigitizeData )
			{
				while ( !ImageReady() )
				{
					Sleep( 50 );
					read_ImagingStatus();
				}

				ByteCount		= m_pvtExposeWidth * m_pvtExposeHeight;
				pRequestData	= new unsigned short[ByteCount];

				if ( ApnNetGetImageTcp(  m_HostAddr, ByteCount, pRequestData ) != APN_NET_SUCCESS )
				{
					delete [] pRequestData;

					SignalImagingDone();
					m_ImageInProgress = false;

					return APN_NET_ERR_IMAGE_DATA;
				}

				delete [] pRequestData;

				SignalImagingDone();
				m_ImageInProgress = false;
			}
			break;
		case Apn_CameraMode_Kinetics:
			// Clean up after the stop

			// Restart the system to flush normally
			SignalImagingDone();
			m_ImageInProgress = false;
			ResetSystem();
			break;
		default:
			break;
	}

	AltaDebugOutputString( "APOGEE.DLL - CApnCamera::PostStopExposure() -> END" );

	return CAPNCAMERA_SUCCESS;
}


void CApnCamera::SetNetworkTransferMode( Apn_NetworkMode TransferMode )
{
	switch ( TransferMode )
	{
	case Apn_NetworkMode_Tcp:
		if ( ApnNetSetSpeed(  m_HostAddr, false ) == APN_NET_SUCCESS )
		{
			m_FastDownload = false;
		}
		break;
	case Apn_NetworkMode_Udp:
		if ( ApnNetSetSpeed(  m_HostAddr, true ) == APN_NET_SUCCESS )
		{
			m_FastDownload = true;
		}
		break;
	}
}


long CApnCamera::Read( unsigned short reg, unsigned short& val )
{
	if ( ApnNetReadReg(  m_HostAddr, reg, &val ) != APN_NET_SUCCESS )
	{
		return CAPNCAMERA_ERR_READ;		// Failure
	}

	return CAPNCAMERA_SUCCESS;
}


long CApnCamera::Write( unsigned short reg, unsigned short val )
{
	if ( ApnNetWriteReg(  m_HostAddr, reg, val ) != APN_NET_SUCCESS )
	{
		return CAPNCAMERA_ERR_WRITE;		// Failure
	}

	return CAPNCAMERA_SUCCESS;
}


long CApnCamera::WriteMultiSRMD( unsigned short reg, unsigned short val[], unsigned short count )
{
	if ( ApnNetWriteRegMulti(  m_HostAddr, reg, val, count ) != APN_NET_SUCCESS )
	{
		return CAPNCAMERA_ERR_WRITE;
	}

	return CAPNCAMERA_SUCCESS;
}


long CApnCamera::WriteMultiMRMD( unsigned short reg[], unsigned short val[], unsigned short count )
{
	if ( ApnNetWriteRegMultiMRMD(  m_HostAddr, reg, val, count ) != APN_NET_SUCCESS )
	{
		return CAPNCAMERA_ERR_WRITE;
	}

	return CAPNCAMERA_SUCCESS;
}


long CApnCamera::QueryStatusRegs( unsigned short&	StatusReg,
									  unsigned short&	HeatsinkTempReg,
									  unsigned short&	CcdTempReg,
									  unsigned short&	CoolerDriveReg,
									  unsigned short&	VoltageReg,
									  unsigned short&	TdiCounter,
									  unsigned short&	SequenceCounter,
									  unsigned short&	MostRecentFrame,
									  unsigned short&	ReadyFrame,
									  unsigned short&	CurrentFrame )
{
	unsigned short RegNumber[7];
	unsigned short RegData[7];


	RegNumber[0] = 91;
	RegNumber[1] = 93;
	RegNumber[2] = 94;
	RegNumber[3] = 95;
	RegNumber[4] = 96;
	RegNumber[5] = 104;
	RegNumber[6] = 105;

	if ( ApnNetReadRegMulti(  m_HostAddr, RegNumber, RegData, 7 ) != APN_NET_SUCCESS )
	{
		return 1;
	}

	StatusReg		= RegData[0];
	HeatsinkTempReg	= RegData[1];
	CcdTempReg		= RegData[2];
	CoolerDriveReg	= RegData[3];
	VoltageReg		= RegData[4];
	TdiCounter		= RegData[5];
	SequenceCounter	= RegData[6];

	MostRecentFrame = 0;
	ReadyFrame		= 0;
	CurrentFrame	= 0;

	return CAPNCAMERA_SUCCESS;
}

