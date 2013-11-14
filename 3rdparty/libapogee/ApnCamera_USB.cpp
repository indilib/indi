//////////////////////////////////////////////////////////////////////
//
// ApnCamera_USB.cpp: implementation of the CApnCamera_USB class.
//
// Copyright (c) 2003-2006 Apogee Instruments, Inc.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"

#include "ApnCamera_USB.h"

#include "ApogeeUsb.h"
#include "ApogeeUsbErr.h"



//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

bool CApnCamera::GetDeviceHandle( void *hCamera, char *CameraInfo )
{

	strcpy( CameraInfo, m_SysDeviceName );

	return true;
}



bool CApnCamera::SimpleInitDriver( unsigned long	CamIdA, 
									   unsigned short	CamIdB, 
									   unsigned long	Option )
{
	m_CamIdA = CamIdA;
	m_CamIdB = CamIdB;
	m_Option = Option;

	m_pvtConnectionOpen = false;

	if ( ApnUsbOpen( (unsigned short)CamIdA,  m_SysDeviceName ) != APN_USB_SUCCESS )
	{
		m_CamIdA = 0x0;
		m_CamIdB = 0x0;
		m_Option = 0x0;

		return false;
	}

	m_pvtConnectionOpen	= true;
	ApnUsbReadVendorInfo( &m_pvtVendorId, &m_pvtProductId, &m_pvtDeviceId );

	if ( (m_pvtProductId != 0x0010) && (m_pvtProductId != 0x0020) )
	{
		return false;
	}


	return true;
}


bool CApnCamera::InitDriver( unsigned long	CamIdA, 
								 unsigned short CamIdB, 
								 unsigned long	Option )
{
	AltaDebugOutputString( "APOGEE.DLL - CApnCamera::InitDriver() -> BEGIN" );

	
	m_CamIdA = CamIdA;
	m_CamIdB = CamIdB;
	m_Option = Option;

	m_pvtConnectionOpen = false;

	if ( ApnUsbOpen( (unsigned short)CamIdA,  m_SysDeviceName ) != APN_USB_SUCCESS )
	{
		m_CamIdA = 0x0;
		m_CamIdB = 0x0;
		m_Option = 0x0;

		return false;
	}

	m_pvtConnectionOpen	= true;

	ApnUsbReadVendorInfo( &m_pvtVendorId, &m_pvtProductId, &m_pvtDeviceId );

	if ( (m_pvtProductId != 0x0010) && (m_pvtProductId != 0x0020) )
	{
		return false;
	}



	// Before trying to initialize, perform a simple loopback test
	unsigned short	RegData;
	unsigned short	NewRegData;

	RegData = 0x5AA5;
	if ( Write( FPGA_REG_SCRATCH, RegData )		!= APN_USB_SUCCESS ) return false;
	if ( Read( FPGA_REG_SCRATCH, NewRegData )	!= APN_USB_SUCCESS ) return false;		
	if ( RegData != NewRegData ) return false;

	RegData = 0xA55A;
	if ( Write( FPGA_REG_SCRATCH, RegData )		!= APN_USB_SUCCESS ) return false;
	if ( Read( FPGA_REG_SCRATCH, NewRegData )	!= APN_USB_SUCCESS ) return false;		
	if ( RegData != NewRegData ) return false;

	m_SysImgSizeBytes	= 0;

	ApnUsbReadVendorInfo(  &m_pvtVendorId, &m_pvtProductId, &m_pvtDeviceId );

	ApnUsbSysDriverVersion(  &m_SysDriverVersion );

	// Update the feature set structure
	if ( m_pvtDeviceId >= 16 )
	{
		m_pvtUseAdvancedStatus	= true;
	}
	else
	{
		m_pvtUseAdvancedStatus	= false;
	}

	m_pvtSequenceImagesDownloaded = 0;
	m_pvtExposeSequenceBulkDownload		= true;
	m_pvtExposeCI						= false;
	m_pvtExposeDualReadout			= read_DualReadout();
	
	m_pvtMostRecentFrame	= 0;
	m_pvtReadyFrame			= 0;
	m_pvtCurrentFrame		= 0;

	// The loopback test was successful.  Proceed with initialization.
	if ( InitDefaults() != 0 )
		return false;

	// Done
	AltaDebugOutputString( "APOGEE.DLL - CApnCamera::InitDriver() -> END" );

	return true;
}


Apn_Interface CApnCamera::GetCameraInterface()
{
//	AltaDebugOutputString( "APOGEE.DLL - CApnCamera::GetCameraInterface()" );

	
	return Apn_Interface_USB;
}


long CApnCamera::GetCameraSerialNumber( char *CameraSerialNumber, long *BufferLength )
{
	AltaDebugOutputString( "APOGEE.DLL - CApnCamera::GetCameraSerialNumber()" );


	char			pBuffer[256];
	unsigned short	Length;


	if ( *BufferLength < (APN_USB_SN_BYTE_COUNT + 1) )
	{
		if ( *BufferLength > 7 )
		{
			strcpy( CameraSerialNumber, "Unknown" );
			*BufferLength = strlen( CameraSerialNumber );
		}

		return CAPNCAMERA_ERR_SN;
	}

	if ( m_pvtDeviceId < 0x0011 )
	{
		strcpy( CameraSerialNumber, "Unknown" );
		*BufferLength = strlen( CameraSerialNumber );
	}
	else
	{
		if ( ApnUsbReadCustomSerialNumber(  pBuffer, &Length ) != APN_USB_SUCCESS )
		{
			strcpy( CameraSerialNumber, "Unknown" );
			*BufferLength = strlen( CameraSerialNumber );
		}
		else
		{
			strcpy( CameraSerialNumber, pBuffer );
			*BufferLength = Length;
		}
	}

	return CAPNCAMERA_SUCCESS;
}


long CApnCamera::GetSystemDriverVersion( char *SystemDriverVersion, long *BufferLength )
{
	AltaDebugOutputString( "APOGEE.DLL - CApnCamera::GetSystemDriverVersion()" );


	if ( m_SysDriverVersion == 0.0 )
		sprintf( SystemDriverVersion, "Unknown" );
	else
		sprintf( SystemDriverVersion, "%.2f", m_SysDriverVersion );

	*BufferLength = strlen( SystemDriverVersion );

	return CAPNCAMERA_SUCCESS;
}


long CApnCamera::GetUsb8051FirmwareRev( char *FirmwareRev, long *BufferLength )
{
	AltaDebugOutputString( "APOGEE.DLL - CApnCamera::GetUsb8051FirmwareRev()" );


	ApnUsbRead8051FirmwareRevision(  FirmwareRev );

	*BufferLength = strlen( FirmwareRev );

	return CAPNCAMERA_SUCCESS;
}


long CApnCamera::GetUsbProductId( unsigned short *pProductId )
{
	AltaDebugOutputString( "APOGEE.DLL - CApnCamera::GetUsbProductId()" );


	*pProductId = m_pvtProductId;

	return CAPNCAMERA_SUCCESS;
}


long CApnCamera::GetUsbDeviceId( unsigned short *pDeviceId )
{
	AltaDebugOutputString( "APOGEE.DLL - CApnCamera::GetUsbDeviceId()" );


	*pDeviceId = m_pvtDeviceId;

	return CAPNCAMERA_SUCCESS;
}


bool CApnCamera::CloseDriver()
{
	AltaDebugOutputString( "APOGEE.DLL - CApnCamera::CloseDriver()" );


	ApnUsbClose( );

	return true;
}

void CApnCamera::SetNetworkTransferMode( int TransferMode )
{
        return;
}
 

long CApnCamera::GetImageData( unsigned short *pImageBuffer, 
								   unsigned short &Width,
								   unsigned short &Height,
								   unsigned long  &Count )
{
	AltaDebugOutputString( "APOGEE.DLL - CApnCamera::GetImageData()" );


	unsigned short	Offset;
	unsigned short	*pTempBuffer;
	unsigned long	DownloadHeight;
	unsigned long	i, j;
	char			szOutputText[128];


	if ( m_pvtConnectionOpen == false )
		return CAPNCAMERA_ERR_CONNECT;

	if ( !ImageInProgress() )
		return CAPNCAMERA_ERR_IMAGE;

	// Make sure it is okay to get the image data
	// The app *should* have done this on its own, but we have to make sure
	if ( (m_pvtNumImages == 1) || (m_pvtExposeSequenceBulkDownload) )
	{
		while ( !ImageReady() )
		{
			Sleep( 50 );
			read_ImagingStatus();
		}
	}

	Width	= m_pvtExposeWidth;
	Height	= m_pvtExposeHeight;

	if ( m_pvtExposeCameraMode != Apn_CameraMode_Test )
	{
		if ( m_pvtExposeBitsPerPixel == 16 )
			Offset = 1;

		if ( m_pvtExposeBitsPerPixel == 12 )
			Offset = 10;

		Width -= Offset;	// Calculate the true image width
	}

	if ( m_pvtExposeSequenceBulkDownload )
		DownloadHeight = Height * m_pvtNumImages;
	else
		DownloadHeight = Height;

	pTempBuffer = new unsigned short[(Width+Offset) * DownloadHeight];
	
	if ( ApnUsbGetImage(  m_SysImgSizeBytes, pTempBuffer ) != APN_USB_SUCCESS )
	{
		AltaDebugOutputString( "APOGEE.DLL - CApnCamera::GetImageData() -> ERROR: Failed ApnUsbGetImage()!!" );
		
		ApnUsbClose( );

		delete [] pTempBuffer;

		SignalImagingDone();

		m_pvtConnectionOpen = false;

		return CAPNCAMERA_ERR_IMAGE;
	}

	unsigned long TermOne;
	unsigned long TermTwo;

	for ( i=0; i<DownloadHeight; i++ )
	{
		TermOne = i*Width;
		TermTwo = (i*(Width+Offset))+Offset;

		for ( j=0; j<Width; j++ )
		{
			// Below is the non-optimized formula for the data re-arrangement
			// pImageBuffer[(i*Width)+j] = pTempBuffer[(i*(Width+Offset))+j+Offset];
			pImageBuffer[TermOne+j] = pTempBuffer[TermTwo+j];
		}
	}

	if ( m_pvtExposeDualReadout == true )
	{
		// rearrange
	}

	delete [] pTempBuffer;

	if ( m_pvtExposeSequenceBulkDownload == true )
	{
		Count = read_ImageCount();
	}
	else
	{
		Count = 1;
	}

	if ( m_pvtExposeCameraMode == Apn_CameraMode_TDI )
	{
		m_pvtTdiLinesDownloaded++;

		AltaDebugPrint( szOutputText, "APOGEE.DLL - CApnCamera::GetImage() -> TdiLinesDownloaded = %d", m_pvtTdiLinesDownloaded );
		AltaDebugOutputString( szOutputText );

		if ( m_pvtTdiLinesDownloaded == read_TDIRows() )
		{
			SignalImagingDone();

			ResetSystem();
		}
	}
	else
	{
		if ( (m_pvtNumImages == 1) || (m_pvtExposeSequenceBulkDownload) )
		{
			AltaDebugOutputString( "APOGEE.DLL - CApnCamera::GetImage() -> Single Image Done" );

			SignalImagingDone();
		}

		if ( (m_pvtNumImages > 1) && (!m_pvtExposeSequenceBulkDownload) )
		{
			m_pvtSequenceImagesDownloaded++;

			AltaDebugPrint( szOutputText, "APOGEE.DLL - CApnCamera::GetImage() -> SequenceImagesDownloaded = %d", m_pvtSequenceImagesDownloaded );
			AltaDebugOutputString( szOutputText );

			if ( m_pvtSequenceImagesDownloaded == m_pvtNumImages )
				SignalImagingDone();
		}
	}

	return CAPNCAMERA_SUCCESS;
}


long CApnCamera::GetLineData( unsigned short *pLineBuffer,
								  unsigned short &Size )
{
	unsigned short	Offset;
	unsigned short	*pTempBuffer;
	unsigned short	Width;
	unsigned long	BytesPerLine;
	unsigned long	i;


	if ( m_pvtConnectionOpen == false )
		return CAPNCAMERA_ERR_CONNECT;

	// image must already be in progress
	if ( !ImageInProgress() )
		return CAPNCAMERA_ERR_IMAGE;

	// the SequenceBulkDownload var *must* be set to FALSE
	if ( m_pvtExposeSequenceBulkDownload )	
		return CAPNCAMERA_ERR_IMAGE;

	Width			= m_pvtExposeWidth;
	BytesPerLine	= Width * 2;

	if ( m_pvtExposeBitsPerPixel == 16 )
		Offset = 1;

	if ( m_pvtExposeBitsPerPixel == 12 )
		Offset = 10;

	Width -= Offset;	// Calculate the true image width

	pTempBuffer = new unsigned short[Width+Offset];
	
	if ( ApnUsbGetImage(  BytesPerLine, pTempBuffer ) != APN_USB_SUCCESS )
	{
		ApnUsbClose( );

		delete [] pTempBuffer;

		SignalImagingDone();

		m_pvtConnectionOpen = false;

		return CAPNCAMERA_ERR_IMAGE;
	}

	for ( i=0; i<Width; i++ )
	{
		pLineBuffer[i] = pTempBuffer[i+Offset];
	}

	delete [] pTempBuffer;

	if ( m_pvtTdiLinesDownloaded == read_TDIRows() )
		SignalImagingDone();
	
	Size = Width;

	return CAPNCAMERA_SUCCESS;
}


long CApnCamera::PreStartExpose( unsigned short BitsPerPixel )
{
	AltaDebugOutputString( "APOGEE.DLL - CApnCamera::PreStartExpose() -> BEGIN" );


	if ( m_pvtConnectionOpen == false )
	{
		return CAPNCAMERA_ERR_CONNECT;
	}

	if ( (BitsPerPixel != 16) && (BitsPerPixel != 12) )
	{
		// Invalid bit depth request
		return CAPNCAMERA_ERR_START_EXP;
	}


	m_pvtExposeWidth				= GetExposurePixelsH();
	m_pvtExposeBitsPerPixel			= BitsPerPixel;
	m_pvtExposeHBinning				= read_RoiBinningH();
	m_pvtExposeSequenceBulkDownload	= read_SequenceBulkDownload();
	m_pvtExposeExternalShutter		= read_ExternalShutter();
	m_pvtExposeCameraMode			= read_CameraMode();
	m_pvtExposeCI					= read_ContinuousImaging();

	if ( m_pvtExposeCameraMode != Apn_CameraMode_Test )
	{
		if ( m_pvtExposeBitsPerPixel == 16 )
			m_pvtExposeWidth += 1;

		if ( m_pvtExposeBitsPerPixel == 12 )
			m_pvtExposeWidth += 10;
	}

	if ( m_pvtExposeCameraMode == Apn_CameraMode_TDI )
	{
		m_pvtTdiLinesDownloaded = 0;
		m_pvtExposeHeight		= 1;
		m_pvtNumImages			= read_TDIRows();
	}
	else
	{
		m_pvtExposeHeight		= GetExposurePixelsV();
		m_pvtNumImages			= read_ImageCount();
	}

	if ( (m_pvtExposeCI) && (m_pvtExposeCameraMode == Apn_CameraMode_Normal) )
	{
		if ( ApnUsbStartCI(  m_pvtExposeWidth, m_pvtExposeHeight ) != APN_USB_SUCCESS )
		{
			ApnUsbClose( );

			if ( ApnUsbOpen( (unsigned short)m_CamIdA,  m_SysDeviceName ) != APN_USB_SUCCESS )
			{
				m_pvtConnectionOpen	= false;
				m_SysImgSizeBytes	= 0;

				return CAPNCAMERA_ERR_CONNECT;		// Failure
			}
			else
			{
				if ( ApnUsbStartCI(  m_pvtExposeWidth, m_pvtExposeHeight ) != APN_USB_SUCCESS )
					return CAPNCAMERA_ERR_START_EXP;
			}
		}

		m_SysImgSizeBytes = m_pvtExposeWidth * m_pvtExposeHeight * 2;
	}
	else
	{
		if ( m_pvtExposeSequenceBulkDownload == true )
		{
			if ( ApnUsbStartExp(  1, m_pvtExposeWidth, (m_pvtExposeHeight*m_pvtNumImages) ) != APN_USB_SUCCESS )
			{
				ApnUsbClose( );

				if ( ApnUsbOpen( (unsigned short)m_CamIdA,  m_SysDeviceName ) != APN_USB_SUCCESS )
				{
					m_pvtConnectionOpen	= false;
					m_SysImgSizeBytes	= 0;

					return CAPNCAMERA_ERR_CONNECT;		// Failure
				}
				else
				{
					if ( ApnUsbStartExp(  1, m_pvtExposeWidth, (m_pvtExposeHeight*m_pvtNumImages) ) != APN_USB_SUCCESS )
						return CAPNCAMERA_ERR_START_EXP;
				}
			}

			m_SysImgSizeBytes = m_pvtExposeWidth * m_pvtExposeHeight * m_pvtNumImages * 2;
		}
		else
		{
			// first check DID and .sys driver version

			// reset our vars that will be updated during future status calls
			m_pvtMostRecentFrame	= 0;
			m_pvtReadyFrame			= 0;
			m_pvtCurrentFrame		= 0;

			m_pvtSequenceImagesDownloaded = 0;

			if ( ApnUsbStartExp(  m_pvtNumImages, m_pvtExposeWidth, m_pvtExposeHeight ) != APN_USB_SUCCESS )
			{
				ApnUsbClose( );

				if ( ApnUsbOpen( (unsigned short)m_CamIdA,  m_SysDeviceName ) != APN_USB_SUCCESS )
				{
					m_pvtConnectionOpen	= false;
					m_SysImgSizeBytes	= 0;

					return CAPNCAMERA_ERR_CONNECT;		// Failure
				}
				else
				{
					if ( ApnUsbStartExp(  m_pvtNumImages, m_pvtExposeWidth, m_pvtExposeHeight ) != APN_USB_SUCCESS )
						return CAPNCAMERA_ERR_START_EXP;
				}
			}

			m_SysImgSizeBytes = m_pvtExposeWidth * m_pvtExposeHeight * 2;
		}
	}

	AltaDebugOutputString( "APOGEE.DLL - CApnCamera::PreStartExpose() -> END" );

	return CAPNCAMERA_SUCCESS;
}


long CApnCamera::PostStopExposure( bool DigitizeData )
{
	AltaDebugOutputString( "APOGEE.DLL - CApnCamera::PostStopExposure() -> BEGIN" );


	PUSHORT			pRequestData;


	if ( m_pvtConnectionOpen == false )
		return CAPNCAMERA_ERR_CONNECT;


	switch( m_pvtExposeCameraMode )
	{
		case Apn_CameraMode_Normal:
			// If in continuous imaging mode, issue the stop
			if ( m_pvtExposeCI )
			{
				ApnUsbStopCI(  1 );
			}

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

					pRequestData = new USHORT[m_pvtExposeWidth*m_pvtExposeHeight];

					if ( ApnUsbGetImage(  m_SysImgSizeBytes, pRequestData ) != APN_USB_SUCCESS )
					{
						ApnUsbClose( );

						delete [] pRequestData;
						
						SignalImagingDone();

						m_pvtConnectionOpen = false;

						return CAPNCAMERA_ERR_STOP_EXP;
					}

					delete [] pRequestData;

					SignalImagingDone();
				}
			}
			else
			{
				AltaDebugOutputString( "APOGEE.DLL - CApnCamera::PostStopExposure() -> Using H/W trigger" );

				if ( read_ImagingStatus() == Apn_Status_WaitingOnTrigger )
				{
					AltaDebugOutputString( "APOGEE.DLL - CApnCamera::PostStopExposure() -> Detected Apn_Status_WaitingOnTrigger" );

					ApnUsbStopExp(  false );
					SignalImagingDone();
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

						pRequestData = new USHORT[m_pvtExposeWidth*m_pvtExposeHeight];

						if ( ApnUsbGetImage(  m_SysImgSizeBytes, pRequestData ) != APN_USB_SUCCESS )
						{
							AltaDebugOutputString( "APOGEE.DLL - CApnCamera::PostStopExposure() -> ERROR: Failed ApnUsbGetImage()!!" );
						
							ApnUsbClose( );

							delete [] pRequestData;
							
							SignalImagingDone();

							m_pvtConnectionOpen = false;

							return CAPNCAMERA_ERR_STOP_EXP;
						}

						delete [] pRequestData;

						SignalImagingDone();
						
						if ( m_pvtExposeExternalShutter )
						{
							ResetSystem();
						}
					}
				}
			}
			break;
		case Apn_CameraMode_TDI:
			// Issue the Stop command
			ApnUsbStopExp(  DigitizeData );
			// Clean up after the stop

			// Restart the system to flush normally
			SignalImagingDone();
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

				pRequestData = new USHORT[m_pvtExposeWidth*m_pvtExposeHeight];

				if ( ApnUsbGetImage(  m_SysImgSizeBytes, pRequestData ) != APN_USB_SUCCESS )
				{
					ApnUsbClose( );

					delete [] pRequestData;
					
					SignalImagingDone();

					m_pvtConnectionOpen = false;

					return CAPNCAMERA_ERR_STOP_EXP;
				}

				delete [] pRequestData;

				SignalImagingDone();
			}
			break;
		case Apn_CameraMode_Kinetics:
			// Issue the Stop command
			ApnUsbStopExp(  DigitizeData );
			// Clean up after the stop

			// Restart the system to flush normally
			SignalImagingDone();
			ResetSystem();
			break;
		default:
			break;
	}

	AltaDebugOutputString( "APOGEE.DLL - CApnCamera::PostStopExposure() -> END" );

	return CAPNCAMERA_SUCCESS;
}


long CApnCamera::Read( unsigned short reg, unsigned short& val )
{
	if ( m_pvtConnectionOpen == false )
		return CAPNCAMERA_ERR_CONNECT;

	if ( ApnUsbReadReg(  reg, &val ) != APN_USB_SUCCESS )
	{
		ApnUsbClose( );

		if ( ApnUsbOpen( (unsigned short)m_CamIdA,  m_SysDeviceName ) != APN_USB_SUCCESS )
		{
			m_pvtConnectionOpen = false;

			return CAPNCAMERA_ERR_CONNECT;		// Failure
		}
		else
		{
			if ( ApnUsbReadReg(  reg, &val ) != APN_USB_SUCCESS )
				return CAPNCAMERA_ERR_READ;
		}
	}

	return CAPNCAMERA_SUCCESS;
}


long CApnCamera::Write( unsigned short reg, unsigned short val )
{
	if ( m_pvtConnectionOpen == false )
		return CAPNCAMERA_ERR_CONNECT;

	if ( ApnUsbWriteReg(  reg, val ) != APN_USB_SUCCESS )
	{
		ApnUsbClose( );

		if ( ApnUsbOpen( (unsigned short)m_CamIdA,  m_SysDeviceName ) != APN_USB_SUCCESS )
		{
			m_pvtConnectionOpen = false;

			return CAPNCAMERA_ERR_CONNECT;		// Failure
		}
		else
		{
			if ( ApnUsbWriteReg(  reg, val ) != APN_USB_SUCCESS )
				return CAPNCAMERA_ERR_WRITE;
		}
	}

	return CAPNCAMERA_SUCCESS;
}


long CApnCamera::WriteMultiSRMD( unsigned short reg, unsigned short val[], unsigned short count )
{
	if ( m_pvtConnectionOpen == false )
		return CAPNCAMERA_ERR_CONNECT;

	if ( ApnUsbWriteRegMulti(  reg, val, count ) != APN_USB_SUCCESS )
	{
		ApnUsbClose( );

		if ( ApnUsbOpen( (unsigned short)m_CamIdA,  m_SysDeviceName ) != APN_USB_SUCCESS )
		{
			m_pvtConnectionOpen = false;

			return CAPNCAMERA_ERR_CONNECT;		// Failure
		}
		else
		{
			if ( ApnUsbWriteRegMulti(  reg, val, count ) != APN_USB_SUCCESS )
				return CAPNCAMERA_ERR_WRITE;
		}
	}

	return CAPNCAMERA_SUCCESS;
}


long CApnCamera::WriteMultiMRMD( unsigned short reg[], unsigned short val[], unsigned short count )
{
	if ( m_pvtConnectionOpen == false )
		return CAPNCAMERA_ERR_CONNECT;

	if ( ApnUsbWriteRegMultiMRMD(  reg, val, count ) != APN_USB_SUCCESS )
	{
		ApnUsbClose( );

		if ( ApnUsbOpen( (unsigned short)m_CamIdA,  m_SysDeviceName ) != APN_USB_SUCCESS )
		{
			m_pvtConnectionOpen = false;

			return CAPNCAMERA_ERR_CONNECT;		// Failure
		}
		else
		{
			if ( ApnUsbWriteRegMultiMRMD(  reg, val, count ) != APN_USB_SUCCESS )
				return CAPNCAMERA_ERR_WRITE;
		}
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
//	AltaDebugOutputString( "APOGEE.DLL - CApnCamera::QueryStatusRegs()" );


	bool DoneFlag;


	if ( m_pvtConnectionOpen == false )
		return CAPNCAMERA_ERR_CONNECT;

	if ( ApnUsbReadStatusRegs( 
							   m_pvtUseAdvancedStatus,
							   &DoneFlag,
							   &StatusReg,
							   &HeatsinkTempReg,
							   &CcdTempReg,
							   &CoolerDriveReg,
							   &VoltageReg,
							   &TdiCounter,
							   &SequenceCounter,
							   &MostRecentFrame,
							   &ReadyFrame,
							   &CurrentFrame ) != APN_USB_SUCCESS )
	{
		ApnUsbClose( );

		if ( ApnUsbOpen( (unsigned short)m_CamIdA,  m_SysDeviceName ) != APN_USB_SUCCESS )
		{
			m_pvtConnectionOpen = false;

			return CAPNCAMERA_ERR_CONNECT;		// Failure
		}
		else
		{
			if ( ApnUsbReadStatusRegs( 
									   m_pvtUseAdvancedStatus,
									   &DoneFlag,
									   &StatusReg,
									   &HeatsinkTempReg,
									   &CcdTempReg,
									   &CoolerDriveReg,
									   &VoltageReg,
									   &TdiCounter,
									   &SequenceCounter,
									   &MostRecentFrame,
									   &ReadyFrame,
									   &CurrentFrame ) != APN_USB_SUCCESS )
			{
				return CAPNCAMERA_ERR_QUERY;
			}
		}
	}

#ifdef APOGEE_DLL_GENERAL_STATUS_OUTPUT
	char szOutputText[256];
	AltaDebugPrint( szOutputText, "APOGEE.DLL - CApnCamera::QueryStatusRegs() -> UseAdvancedStatus (Driver Flag) = %d", m_pvtUseAdvancedStatus );
	AltaDebugOutputString( szOutputText );
	AltaDebugPrint( szOutputText, "APOGEE.DLL - CApnCamera::QueryStatusRegs() -> DoneFlag (USB FW Flag) = %d", DoneFlag );
	AltaDebugOutputString( szOutputText );
	AltaDebugPrint( szOutputText, "APOGEE.DLL - CApnCamera::QueryStatusRegs() -> StatusReg (R91) = 0x%04X", StatusReg );
	AltaDebugOutputString( szOutputText );
	AltaDebugPrint( szOutputText, "APOGEE.DLL - CApnCamera::QueryStatusRegs() -> HeatsinkTempReg (R93) = 0x%04X", HeatsinkTempReg );
	AltaDebugOutputString( szOutputText );
	AltaDebugPrint( szOutputText, "APOGEE.DLL - CApnCamera::QueryStatusRegs() -> CcdTempReg (R94) = 0x%04X", CcdTempReg );
	AltaDebugOutputString( szOutputText );
	AltaDebugPrint( szOutputText, "APOGEE.DLL - CApnCamera::QueryStatusRegs() -> CoolerDriveReg (R95) = 0x%04X", CoolerDriveReg );
	AltaDebugOutputString( szOutputText );
	AltaDebugPrint( szOutputText, "APOGEE.DLL - CApnCamera::QueryStatusRegs() -> VoltageReg (R96) = 0x%04X", VoltageReg );
	AltaDebugOutputString( szOutputText );
	AltaDebugPrint( szOutputText, "APOGEE.DLL - CApnCamera::QueryStatusRegs() -> TdiCounter (R104) = %d", TdiCounter );
	AltaDebugOutputString( szOutputText );
	AltaDebugPrint( szOutputText, "APOGEE.DLL - CApnCamera::QueryStatusRegs() -> SequenceCounter (R105) = %d", SequenceCounter );
	AltaDebugOutputString( szOutputText );
	AltaDebugPrint( szOutputText, "APOGEE.DLL - CApnCamera::QueryStatusRegs() -> MostRecentFrame (USB FW Counter) = %d", MostRecentFrame );
	AltaDebugOutputString( szOutputText );
	AltaDebugPrint( szOutputText, "APOGEE.DLL - CApnCamera::QueryStatusRegs() -> ReadyFrame (USB FW Counter) = %d", ReadyFrame );
	AltaDebugOutputString( szOutputText );
	AltaDebugPrint( szOutputText, "APOGEE.DLL - CApnCamera::QueryStatusRegs() -> CurrentFrame (USB FW Counter) = %d", CurrentFrame );
	AltaDebugOutputString( szOutputText );
#endif

	if ( DoneFlag )
	{
		StatusReg |= FPGA_BIT_STATUS_IMAGE_DONE;
	}


	m_pvtMostRecentFrame	= MostRecentFrame;
	m_pvtReadyFrame			= ReadyFrame;
	m_pvtCurrentFrame		= CurrentFrame;

	return CAPNCAMERA_SUCCESS;
}
