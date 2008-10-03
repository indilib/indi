// ApnSerial_USB.h: interface for the CApnSerial_USB class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_APNSERIAL_USB_H__D7A1A328_6505_438F_BCCE_FA3F3B5EECC2__INCLUDED_)
#define AFX_APNSERIAL_USB_H__D7A1A328_6505_438F_BCCE_FA3F3B5EECC2__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "ApnSerial.h"

class CApnSerial_USB : public CApnSerial  
{
public:
	CApnSerial_USB();
	virtual ~CApnSerial_USB();

	bool InitPort( unsigned long	CamIdA,
				   unsigned short	CamIdB,
				   unsigned short	SerialId );

	bool ClosePort();

	bool GetBaudRate(unsigned long *BaudRate );
	bool SetBaudRate(unsigned long BaudRate );

	bool GetFlowControl( Apn_SerialFlowControl *FlowControl );
	bool SetFlowControl( Apn_SerialFlowControl FlowControl );

	bool GetParity( Apn_SerialParity *Parity );
	bool SetParity( Apn_SerialParity Parity );

	bool Read(int dummy,  char			  *ReadBuffer,
			   unsigned short *ReadCount );

	bool Write( int dummy, char		   *WriteBuffer,
				unsigned short WriteCount );
	char *ReadBuffer(void);

private:
        bool    m_ConnectionOpen;
 
        long    m_hSysDriver;
        char    m_SysDeviceName[1024];
 
 
};

#endif // !defined(AFX_APNSERIAL_USB_H__D7A1A328_6505_438F_BCCE_FA3F3B5EECC2__INCLUDED_)
