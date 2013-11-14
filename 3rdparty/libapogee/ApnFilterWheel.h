// ApnFilterWheel.h: interface for the CApnFilterWheel class.
//
// Copyright (c) 2007 Apogee Instruments, Inc.
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_APNFILTERWHEEL_H__6B02FF9C_AB8E_4488_A166_04C6B378E35E__INCLUDED_)
#define AFX_APNFILTERWHEEL_H__6B02FF9C_AB8E_4488_A166_04C6B378E35E__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "Apogee.h"


class CApnFilterWheel
{
public:
	CApnFilterWheel();
	virtual ~CApnFilterWheel();

	bool			Init( Apn_Filter FilterType, unsigned long DeviceNum );
	bool			Close();

	bool			GetVendorId( unsigned short *VendorId );
	bool			GetProductId( unsigned short *ProductId );
	bool			GetDeviceId( unsigned short *DeviceId );

	bool			GetUsbFirmwareRev( char *FirmwareRev, long *BufferLength );

	bool			GetWheelType( Apn_Filter *WheelType );

	bool			GetWheelModel( char *WheelDescr, long *BufferLength );

	bool			GetStatus( Apn_FilterStatus *FilterStatus );

	bool			GetMaxPositions( unsigned long *MaxPositions );

	bool			SetPosition( unsigned long Position );
	bool			GetPosition( unsigned long *Position );

private:
	bool			m_ConnectionOpen;

	unsigned long	m_DeviceNum;

	unsigned short	m_VendorId;
	unsigned short	m_ProductId;
	unsigned short	m_DeviceId;

	Apn_Filter		m_WheelType;

	unsigned long 	m_MaxPositions;

	char			m_SysDeviceName[1024];


	void			InitMemberVars();

	// message map functions
};

#endif // !defined(AFX_APNFILTERWHEEL_H__6B02FF9C_AB8E_4488_A166_04C6B378E35E__INCLUDED_)
