// ApnSerial_USB.cpp: implementation of the CApnSerial class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "ApogeeUsb.h"
#include "ApogeeUsbErr.h"
#include "ApnSerial_USB.h"

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////



bool CApnSerial::InitPort( unsigned long  CamIdA, 
							   unsigned short CamIdB,
							   unsigned short SerialId )
{
        double                  VersionNum;
        unsigned short  VendorId;
        unsigned short  ProductId;
        unsigned short  DeviceId;
                                                                           
                                                                           
        if ( m_SerialId != -1 )
        {
                return false;
        }
                                                                           
        if ( (SerialId != 0) && (SerialId != 1) )
        {
                return false;
        }
                                                                           
        m_ConnectionOpen = false;
                                                                           
        if ( ApnUsbOpen( (unsigned short)CamIdA,  m_SysDeviceName ) != APN_USB_SUCCESS )
        {
                return false;
        }
                                                                           
        if ( (ApnUsbSysDriverVersion( &VersionNum) != APN_USB_SUCCESS) ||
                 (ApnUsbReadVendorInfo( &VendorId, &ProductId, &DeviceId) != APN_USB_SUCCESS) )
        {
                ClosePort();
                return false;
        }
                                                                           
        if ( (VersionNum < 1.3) || (DeviceId < 5) )
        {
                // Serial port operation requires AltaUsb.sys version 1.3 or higher
                // and the Device ID of the firmware on the USB board should be 5 or higher
                ClosePort();
                return false;
        }
                                                                           
        m_ConnectionOpen        = true;
        m_SerialId                      = SerialId;
        m_BytesRead                     = 0;
                                                                           
        SetBaudRate( 9600 );
        SetFlowControl( Apn_SerialFlowControl_Off );
        SetParity( Apn_SerialParity_None );
                                                                           
        return true;

}

bool CApnSerial::ClosePort()
{
        if ( m_SerialId == -1 )
                return false;
                                                                           
        // just close the port and not care whether it was successful.  if it was,
        // great.  if not, we'll still set m_SerialId to -1 so that another call
        // can at least be tried to connect to the port.
        ApnUsbClose(  );
                                                                           
        m_SerialId = -1;
                                                                           
        return true;

}

bool CApnSerial::GetBaudRate(unsigned long *BaudRate )
{
        if ( (m_ConnectionOpen == false) || (m_SerialId == -1) )
                return false;
                                                                           
        if ( ApnUsbSerialReadBaudRate( m_SerialId, BaudRate)
!= APN_USB_SUCCESS )
        {
                *BaudRate = 0;
                                                                           
                return false;
        }
                                                                           
        return true;

}

bool CApnSerial::SetBaudRate(unsigned long BaudRate )
{
        if ( (m_ConnectionOpen == false) || (m_SerialId == -1) )
                return false;
                                                                           
        if ( ApnUsbSerialWriteBaudRate(  m_SerialId, BaudRate ) != APN_USB_SUCCESS )
        {
                return false;
        }
                                                                           
        return true;

}

bool CApnSerial::GetFlowControl( Apn_SerialFlowControl *FlowControl )
{
        bool EnableFlowControl;
                                                                           
                                                                           
        if ( (m_ConnectionOpen == false) || (m_SerialId == -1) )
                return false;
                                                                           
        if ( ApnUsbSerialReadFlowControl( m_SerialId, &EnableFlowControl) != APN_USB_SUCCESS )
        {
                *FlowControl = Apn_SerialFlowControl_Unknown;
                return false;
        }
                                                                           
        if ( EnableFlowControl )
                *FlowControl = Apn_SerialFlowControl_On;
        else
                *FlowControl = Apn_SerialFlowControl_Off;
                                                                           
        return true;

}

bool CApnSerial::SetFlowControl( Apn_SerialFlowControl FlowControl )
{
        bool EnableFlowControl;
                                                                           
                                                                           
        if ( (m_ConnectionOpen == false) || (m_SerialId == -1) )
                return false;
                                                                           
        if ( FlowControl == Apn_SerialFlowControl_Unknown )
                return false;
                                                                           
        if ( FlowControl == Apn_SerialFlowControl_On )
                EnableFlowControl = true;
        else
                EnableFlowControl = false;
                                                                           
        if ( ApnUsbSerialWriteFlowControl( m_SerialId, EnableFlowControl) != APN_USB_SUCCESS )
        {
                return false;
        }
                                                                           
        return true;

}

bool CApnSerial::GetParity( Apn_SerialParity *Parity )
{
        ApnUsbParity ParityRead;
                                                                           
                                                                           
        if ( (m_ConnectionOpen == false) || (m_SerialId == -1) )
                return false;
                                                                           
        if ( ApnUsbSerialReadParity( m_SerialId, &ParityRead) != APN_USB_SUCCESS )
        {
                *Parity = Apn_SerialParity_Unknown;
                                                                           
                return false;
        }
                                                                           
        if ( ParityRead == ApnUsbParity_None )
                *Parity = Apn_SerialParity_None;
        else if ( ParityRead == ApnUsbParity_Even )
                *Parity = Apn_SerialParity_Even;
        else if ( ParityRead == ApnUsbParity_Odd )
                *Parity = Apn_SerialParity_Odd;
                                                                           
        return true;

}

bool CApnSerial::SetParity( Apn_SerialParity Parity )
{
        ApnUsbParity ParityWrite;
                                                                           
                                                                           
        if ( (m_ConnectionOpen == false) || (m_SerialId == -1) )
                return false;
                                                                           
        if ( Parity == Apn_SerialParity_Unknown )
                return false;
                                                                           
        if ( Parity == Apn_SerialParity_None )
                ParityWrite = ApnUsbParity_None;
        else if ( Parity == Apn_SerialParity_Even )
                ParityWrite = ApnUsbParity_Even;
        else if ( Parity == Apn_SerialParity_Odd )
                ParityWrite = ApnUsbParity_Odd;
                                                                           
        if ( ApnUsbSerialWriteParity( m_SerialId, ParityWrite) != APN_USB_SUCCESS )
        {
                return false;
        }
                                                                           
        return true;

}

bool CApnSerial::Read(int dummy,char	*ReadBuffer, unsigned short *ReadCount )
{
        if ( (m_ConnectionOpen == false) || (m_SerialId == -1) )
                return false;
                                                                           
        if ( ApnUsbSerialRead( m_SerialId, ReadBuffer, ReadCount) != APN_USB_SUCCESS )
        {
                *ReadCount      = 0;
                                                                           
                m_BytesRead = 0;
                                                                           
                return false;
        }
                                                                           
        m_BytesRead = *ReadCount;
                                                                           
        return true;

}

bool CApnSerial::Write(int dummy, char *WriteBuffer, unsigned short WriteCount )
{
        if ( (m_ConnectionOpen == false) || (m_SerialId == -1) )
                return false;
                                                                           
        if ( ApnUsbSerialWrite( m_SerialId, WriteBuffer, WriteCount) != APN_USB_SUCCESS )
        {
                return false;
        }
                                                                           
        return true;

}


char *CApnSerial::ReadBuffer(void)
{

    int dummy;
    unsigned short count;

    Read(dummy,m_SerialBuffer,&count);
    return m_SerialBuffer;

}
