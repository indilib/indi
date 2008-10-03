/////////////////////////////////////////////////////////////
//
// ApnCamera_USB.h:  Interface file for the CApnCamera_USB class.
//
// Copyright (c) 2003, 2004 Apogee Instruments, Inc.
//
/////////////////////////////////////////////////////////////

#if !defined(AFX_APNCAMERA_USB_H__E83248CA_F0AA_4221_8E10_22FA70CEFAA6__INCLUDED_)
#define AFX_APNCAMERA_USB_H__E83248CA_F0AA_4221_8E10_22FA70CEFAA6__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "ApnCamera.h"


class CApnCamera_USB : public CApnCamera 
{
private:
	bool			m_pvtConnectionOpen;

	double			m_SysDriverVersion;

//	unsigned long	m_CamIdA;
	unsigned short	m_CamIdB;
	unsigned long	m_Option;

/*   moved to ApnCamera.h
	unsigned short	m_pvtVendorId;
	unsigned short	m_pvtProductId;
	unsigned short	m_pvtDeviceId;

        bool                    m_pvtUseAdvancedStatus;

        Apn_CameraMode  m_pvtExposeCameraMode;

        unsigned short  m_pvtExposeBitsPerPixel;
        unsigned short  m_pvtExposeHBinning;

        bool                    m_pvtExposeDualReadout;
        bool                    m_pvtExposeSequenceBulkDownload;
        bool                    m_pvtExposeCI;

        unsigned short  m_pvtExposeWidth;
        unsigned short  m_pvtExposeHeight;

        bool                    m_pvtExposeExternalShutter;

        unsigned short  m_pvtNumImages;

        unsigned short  m_pvtSequenceImagesDownloaded;

        unsigned short  m_pvtTdiLinesDownloaded;

        unsigned short  m_pvtMostRecentFrame;
        unsigned short  m_pvtReadyFrame;
        unsigned short  m_pvtCurrentFrame;

        unsigned long   m_SysImgSizeBytes;
	char			m_SysDeviceName[1024];
*/
 

public:
	CApnCamera_USB();
	virtual ~CApnCamera_USB();


	bool GetDeviceHandle( void *hCamera, char *CameraInfo );

	bool InitDriver( unsigned long	CamIdA, 
					 unsigned short CamIdB, 
					 unsigned long	Option );

	bool SimpleInitDriver( unsigned long	CamIdA, 
						   unsigned short	CamIdB, 
						   unsigned long	Option );

	Apn_Interface GetCameraInterface(); 

	long GetCameraSerialNumber( char *CameraSerialNumber, long *BufferLength );
	
	long GetSystemDriverVersion( char *SystemDriverVersion, long *BufferLength );

	long GetUsb8051FirmwareRev( char *FirmwareRev, long *BufferLength );

	long GetUsbProductId( unsigned short *pProductId );
	long GetUsbDeviceId( unsigned short *pDeviceId );

	bool CloseDriver();

	long PreStartExpose( unsigned short BitsPerPixel );

	long PostStopExposure( bool DigitizeData );

	long GetImageData( unsigned short *pImageData, 
					   unsigned short &Width,
					   unsigned short &Height,
					   unsigned long  &Count );

	long GetLineData( unsigned short *pLineBuffer,
					  unsigned short &Size );		

	long Read( unsigned short reg, unsigned short& val );
	long Write( unsigned short reg, unsigned short val );

	long WriteMultiSRMD( unsigned short reg, 
						 unsigned short val[], 
						 unsigned short count );

	long WriteMultiMRMD( unsigned short reg[], 
						 unsigned short val[], 
						 unsigned short count );

	long QueryStatusRegs( unsigned short&	StatusReg,
						  unsigned short&	HeatsinkTempReg,
						  unsigned short&	CcdTempReg,
						  unsigned short&	CoolerDriveReg,
						  unsigned short&	VoltageReg,
						  unsigned short&	TdiCounter,
						  unsigned short&	SequenceCounter,
						  unsigned short&	MostRecentFrame,
						  unsigned short&	ReadyFrame,
						  unsigned short&	CurrentFrame );

};

#endif // !defined(AFX_APNCAMERA_USB_H__E83248CA_F0AA_4221_8E10_22FA70CEFAA6__INCLUDED_)
