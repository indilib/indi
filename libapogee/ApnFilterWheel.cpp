//////////////////////////////////////////////////////////////////////
//
// ApnFilterWheel.cpp: implementation of the CApnFilterWheel class.
//
// Copyright (c) 2007 Apogee Instruments, Inc.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "dbt.h"


#include "ApnFilterWheel.h"

#include "ApogeeUsb.h"
#include "ApogeeUsbErr.h"

#include "Apn.h"



#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CApnFilterWheel::CApnFilterWheel()
{

	InitMemberVars();
}

CApnFilterWheel::~CApnFilterWheel()
{

}


/////////////////////////////////////////////////////////////////////////////
// CApnSerial_USB message handlers

void CApnFilterWheel::InitMemberVars()
{
	m_VendorId			= 0;
	m_ProductId			= 0;
	m_DeviceId			= 0;

	m_MaxPositions			= 0;

	m_WheelType			= Apn_Filter_Unknown;

	m_ConnectionOpen		= false;
	m_DeviceNum			= 0;

}

bool CApnFilterWheel::Init( Apn_Filter FilterType, unsigned long DeviceNum )
{
	unsigned short IoAssignment;


	m_ConnectionOpen = false;

	if ( (FilterType != Apn_Filter_FW50_9R) && (FilterType != Apn_Filter_FW50_7S) )
	{
		return false;
	}

	if ( ApnUsbOpen( (unsigned short)DeviceNum, m_SysDeviceName ) != APN_USB_SUCCESS )
	{
		return false;
	}

	ApnUsbReadVendorInfo( &m_VendorId, &m_ProductId, &m_DeviceId );

	if ( m_ProductId != 0x0100 )
	{
		return false;
	}


	m_ConnectionOpen = true;

	m_WheelType = FilterType;

	GetMaxPositions( &m_MaxPositions );

	SetPosition( 1 );

	return true;
}

bool CApnFilterWheel::Close()
{
	ApnUsbClose( );

	InitMemberVars();

	return true;
}

bool CApnFilterWheel::GetVendorId( unsigned short *VendorId )
{
	bool RetVal;

	RetVal = true;

	if ( !m_ConnectionOpen )
		return false;

	if ( m_WheelType != Apn_Filter_Unknown )
	{
		*VendorId	= m_VendorId;
	}
	else
	{
		*VendorId	= 0x0;
		RetVal		= false; 
	}

	return RetVal;
}

bool CApnFilterWheel::GetProductId( unsigned short *ProductId )
{
	bool RetVal;

	RetVal = true;

	if ( !m_ConnectionOpen )
		return false;

	if ( m_WheelType != Apn_Filter_Unknown )
	{
		*ProductId	= m_ProductId;
	}
	else
	{
		*ProductId	= 0x0;
		RetVal		= false; 
	}

	return RetVal;
}

bool CApnFilterWheel::GetDeviceId( unsigned short *DeviceId )
{
	bool RetVal;

	RetVal = true;

	if ( !m_ConnectionOpen )
		return false;

	if ( m_WheelType != Apn_Filter_Unknown )
	{
		*DeviceId	= m_DeviceId;
	}
	else
	{
		*DeviceId	= 0x0;
		RetVal		= false; 
	}

	return RetVal;
}

bool CApnFilterWheel::GetUsbFirmwareRev( char *FirmwareRev, long *BufferLength )
{
	bool RetVal;

	RetVal = true;

	if ( !m_ConnectionOpen )
		return false;

	if ( ApnUsbRead8051FirmwareRevision( FirmwareRev ) == APN_USB_SUCCESS )
	{
		*BufferLength = strlen( FirmwareRev );
	}
	else
	{
		RetVal = false;
	}

	return RetVal;
}

bool CApnFilterWheel::GetWheelType( Apn_Filter *WheelType )
{
	bool RetVal;

	RetVal = true;

	if ( m_WheelType == Apn_Filter_Unknown )
		RetVal = false; 

	*WheelType = m_WheelType;

	return RetVal;
}

bool CApnFilterWheel::GetWheelModel( char *WheelDescr, long *BufferLength )
{
	bool RetVal;

	RetVal = true;

	char szModel[256];

	switch ( m_WheelType )
	{
		case Apn_Filter_FW50_9R:
			strcpy( szModel, APN_FILTER_AFW30_7R_DESCR );
			break;
		case Apn_Filter_FW50_7S:
			strcpy( szModel, APN_FILTER_AFW50_5R_DESCR );
			break;
		case Apn_Filter_Unknown:
			strcpy( szModel, APN_FILTER_UNKNOWN_DESCR );
			break;
		default:
			strcpy( szModel, APN_FILTER_UNKNOWN_DESCR );
			break;
	}

	*BufferLength = strlen( szModel );

	strncpy( WheelDescr, szModel, *BufferLength + 1);

	return RetVal;
}

bool CApnFilterWheel::GetStatus( Apn_FilterStatus *FilterStatus )
{
	bool				RetVal;
	unsigned char		Data;
	unsigned char		Pins;
	Apn_FilterStatus	TempStatus;

	RetVal = true;

	if ( !m_ConnectionOpen )
	{
		*FilterStatus = Apn_FilterStatus_NotConnected;
	}
	else
	{
		ApnUsbReadControlPort( &Data, &Pins );

		if ( Pins & 0x01 )
		{
			*FilterStatus = Apn_FilterStatus_Active;
		}
		else
		{
			*FilterStatus = Apn_FilterStatus_Ready;
		}
	}

	return RetVal;
}

bool CApnFilterWheel::GetMaxPositions( unsigned long *MaxPositions )
{
	bool			RetVal;

	RetVal = true;

	if ( !m_ConnectionOpen )
	{
		*MaxPositions = 0;
		return false;
	}

	switch ( m_WheelType )
	{
	case Apn_Filter_FW50_9R:
		*MaxPositions = APN_FILTER_FW50_9R_MAX_POSITIONS;
		break;
	case Apn_Filter_FW50_7S:
		*MaxPositions = APN_FILTER_FW50_7S_MAX_POSITIONS;
		break;
	case Apn_Filter_Unknown:
		*MaxPositions = 0;
		RetVal = false;
		break;
	default:
		*MaxPositions = 0;
		RetVal = false;
		break;
	}

	return RetVal;
}

bool CApnFilterWheel::SetPosition( unsigned long Position )
{
	bool			RetVal;
	unsigned char	Data;
	unsigned char	Pins;


	RetVal = true;

	if ( !m_ConnectionOpen )
	{
		return false;
	}

	if ( (Position < 1) || (Position > m_MaxPositions) )
	{
		return false;
	}

	// ApnUsbConfigureControlPort( 0x0F );

	Data = Position - 1;	// hardware is zero-based
	Pins = 0x0;

	ApnUsbWriteControlPort( Data, Pins );

	return RetVal;
}

bool CApnFilterWheel::GetPosition( unsigned long *Position )
{
	bool			RetVal;
	unsigned char	Data;
	unsigned char	Pins;


	RetVal = true;

	if ( !m_ConnectionOpen )
	{
		*Position = 0;
		return false;
	}

	// ApnUsbConfigureControlPort( 0x00 );

	ApnUsbReadControlPort( &Data, &Pins );

	*Position = (Data & 0x0F) + 1;	// hardware is zero-based

	return RetVal;
}

