// ApnSerial_NET.cpp: implementation of the CApnSerial class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "ApnSerial_NET.h"

#include "ApogeeNet.h"
#include "ApogeeNetErr.h"


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
	char			Hostname[25];
	BYTE			ipAddr[4];


	ipAddr[0] = (BYTE)(CamIdA & 0xFF);
	ipAddr[1] = (BYTE)((CamIdA >> 8) & 0xFF);
	ipAddr[2] = (BYTE)((CamIdA >> 16) & 0xFF);
	ipAddr[3] = (BYTE)((CamIdA >> 24) & 0xFF);

	sprintf( m_HostAddr, "%u.%u.%u.%u", ipAddr[3], ipAddr[2], ipAddr[1], ipAddr[0] );

	if ( m_SerialId != -1 )
	{
		return false;
	}

	if ( (SerialId != 0) && (SerialId != 1) )
	{
		return false;
	}

	if ( ApnNetStartSockets() != APN_NET_SUCCESS )
		return false;

        if ( ApnNetSerialPortOpen( &m_SerialSocket, m_HostAddr, CamIdB ) != APN_NET_SUCCESS )
                return false;
                                                                           
        m_PortNum       = CamIdB;
        m_SerialId      = SerialId;
        m_BytesRead = 0;
                                                                           
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
	ApnNetSerialPortClose( &m_SerialSocket );

	ApnNetStopSockets();

	m_SerialId = -1;

	return true;
}

bool CApnSerial::GetBaudRate( unsigned long *BaudRate )
{
        if ( m_SerialId == -1 )
                return false;
                                                                           
        if ( ApnNetSerialReadBaudRate( m_HostAddr, m_SerialId, BaudRate) != 0 )
        {
                *BaudRate = 0;
                                                                           
                return false;
        }
                                                                           
        return true;

}

bool CApnSerial::SetBaudRate(unsigned long BaudRate )
{
        if ( m_SerialId == -1 )
                return false;
                                                                           
        if ( ApnNetSerialWriteBaudRate( m_HostAddr, m_SerialId, BaudRate) != APN_NET_SUCCESS )
        {
                return false;
        }
        else
        {
                ApnNetSerialPortClose( &m_SerialSocket );
                ApnNetSerialPortOpen( &m_SerialSocket, m_HostAddr, m_PortNum );
        }
                                                                           
        return true;

}

bool CApnSerial::GetFlowControl( Apn_SerialFlowControl *FlowControl )
{
        bool FlowControlRead;
 
 
        *FlowControl = Apn_SerialFlowControl_Unknown;
 
        if ( m_SerialId == -1 )
                return false;
 
        if ( ApnNetSerialReadFlowControl(m_HostAddr, m_SerialId, &FlowControlRead) != APN_NET_SUCCESS )
        {
                return false;
        }
        else
        {
                if ( FlowControlRead )
                        *FlowControl = Apn_SerialFlowControl_On;
                else
                        *FlowControl = Apn_SerialFlowControl_Off;
        }
 
        return true;

}

bool CApnSerial::SetFlowControl( Apn_SerialFlowControl FlowControl )
{
        bool FlowControlWrite;
 
 
        if ( m_SerialId == -1 )
                return false;
 
        if ( FlowControl == Apn_SerialFlowControl_On )
                FlowControlWrite = true;
        else
                FlowControlWrite = false;
 
        if ( ApnNetSerialWriteFlowControl(m_HostAddr, m_SerialId, FlowControlWrite) != APN_NET_SUCCESS )
        {
                return false;
        }
        else
        {
                ApnNetSerialPortClose( &m_SerialSocket );
                ApnNetSerialPortOpen( &m_SerialSocket, m_HostAddr, m_PortNum );
        }
 
        return true;

}

bool CApnSerial::GetParity( Apn_SerialParity *Parity )
{
        ApnNetParity ParityRead;
                                                                           
                                                                           
        *Parity = Apn_SerialParity_Unknown;
                                                                           
        if ( m_SerialId == -1 )
                return false;
                                                                           
        if ( ApnNetSerialReadParity(m_HostAddr, m_SerialId, &ParityRead) != APN_NET_SUCCESS )
                return false;
                                                                           
        if ( ParityRead == ApnNetParity_None )
                *Parity = Apn_SerialParity_None;
        else if ( ParityRead == ApnNetParity_Even )
                *Parity = Apn_SerialParity_Even;
        else if ( ParityRead == ApnNetParity_Odd )
                *Parity = Apn_SerialParity_Odd;
                                                                           
        return true;

}

bool CApnSerial::SetParity( Apn_SerialParity Parity )
{
        ApnNetParity ParityWrite;
                                                                           
                                                                           
        if ( m_SerialId == -1 )
                return false;
                                                                           
        if ( Parity == Apn_SerialParity_None )
                ParityWrite = ApnNetParity_None;
        else if ( Parity == Apn_SerialParity_Even )
                ParityWrite = ApnNetParity_Even;
        else if ( Parity == Apn_SerialParity_Odd )
                ParityWrite = ApnNetParity_Odd;
        else
                return false;
                                                                           
        if ( ApnNetSerialWriteParity(m_HostAddr, m_SerialId, ParityWrite) != APN_NET_SUCCESS )
        {
                return false;
        }
        else
        {
                ApnNetSerialPortClose( &m_SerialSocket );
                ApnNetSerialPortOpen( &m_SerialSocket, m_HostAddr, m_PortNum );
        }
                                                                           
        return true;

}

bool CApnSerial::Read(int dsocket ,char	*ReadBuffer, unsigned short *ReadCount )
{
        if ( m_SerialId == -1 )
                return false;
                                                                           
        if ( ApnNetSerialRead(&m_SerialSocket, ReadBuffer, ReadCount) != APN_NET_SUCCESS )
        {
                *ReadCount      = 0;
                                                                           
                m_BytesRead = 0;
                                                                           
                return false;
        }
                                                                           
        m_BytesRead = *ReadCount;
                                                                           
        return true;

}

char *CApnSerial::ReadBuffer(void)
{

    int dummy;
    unsigned short count;

    Read(dummy,m_SerialBuffer,&count);
    return m_SerialBuffer;

}


bool CApnSerial::Write(int dsocket,char *WriteBuffer, unsigned short WriteCount )
{
        if ( m_SerialId == -1 )
                return false;
                                                                           
        if ( ApnNetSerialWrite(&m_SerialSocket, WriteBuffer, WriteCount) != APN_NET_SUCCESS )
                return false;
                                                                           
        return true;

}

