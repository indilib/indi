// ApnSerial.h: interface for the CApnSerial class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_APNSERIAL_H__A27F1749_FA8F_40E8_A03F_4A28C8378DD1__INCLUDED_)
#define AFX_APNSERIAL_H__A27F1749_FA8F_40E8_A03F_4A28C8378DD1__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "Apogee.h"


class CApnSerial  
{
public:

	CApnSerial();
	~CApnSerial();

	bool InitPort( unsigned long	CamIdA,
						   unsigned short	CamIdB,
						   unsigned short	SerialId );

	bool ClosePort();

	bool GetBaudRate( unsigned long *BaudRate );

	bool SetBaudRate( unsigned long BaudRate );

	bool GetFlowControl( Apn_SerialFlowControl *FlowControl );

	bool SetFlowControl( Apn_SerialFlowControl FlowControl );

	bool GetParity( Apn_SerialParity *Parity );

	bool SetParity( Apn_SerialParity Parity );

	bool Read( int SerialSock, char			  *ReadBuffer,
					   unsigned short *ReadCount );

	bool Write( int SerialSock, char		   *WriteBuffer,
						unsigned short WriteCount );
	char *ReadBuffer(void);

	// Variables
	Apn_Interface	m_CameraInterface;
	int		m_ConnectionOpen;
	char		m_SysDeviceName[80];
	int		m_SerialId;
        short           m_BytesRead;
        long            m_hSession;
        int             m_SerialSocket;
        char            m_HostAddr[80];
        unsigned short  m_PortNum;
 	char		m_SerialBuffer[4096];

};

#endif // !defined(AFX_APNSERIAL_H__A27F1749_FA8F_40E8_A03F_4A28C8378DD1__INCLUDED_)
