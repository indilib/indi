/////////////////////////////////////////////////////////////
//
// ApnCamera_NET.h:  Interface file for the CApnCamera_NET class.
//
// Copyright (c) 2003, 2004 Apogee Instruments, Inc.
//
/////////////////////////////////////////////////////////////

#if !defined(AFX_APNCAMERA_NET_H__D6F0E3AB_536C_4937_9E2B_DCF682D0DD31__INCLUDED_)
#define AFX_APNCAMERA_NET_H__D6F0E3AB_536C_4937_9E2B_DCF682D0DD31__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "ApnCamera.h"

#include "ApogeeNet.h"
#include "ApogeeNetErr.h"


class CApnCamera_NET : public CApnCamera  
{
private:
	HINTERNET		m_hSession;
	ULONG			m_ImageSizeBytes;
	BOOLEAN			m_ImageInProgress;
	bool			m_FastDownload;
	char			m_HostAddr[80];

	unsigned short	m_pvtBitsPerPixel;
	unsigned short	m_pvtExposeBitsPerPixel;
        unsigned short  m_pvtExposeCameraMode;
        unsigned short  m_pvtExposeWidth;
        unsigned short  m_pvtExposeHeight;
                                                                               
        bool                    m_pvtExposeExternalShutter;
                                                                               

	unsigned short	m_pvtNumImages;

public:
	CApnCamera_NET();
	virtual ~CApnCamera_NET();

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

	void SetNetworkTransferMode( Apn_NetworkMode TransferMode );

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

#endif // !defined(AFX_APNCAMERA_NET_H__D6F0E3AB_536C_4937_9E2B_DCF682D0DD31__INCLUDED_)
