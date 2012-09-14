/*****************************************************************************************
NAME
 PacketWrapper

DESCRIPTION
 Packet interface

COPYRIGHT (C)
 QSI (Quantum Scientific Imaging) 2005-2006

REVISION HISTORY
 MWB 04.28.06 Original Version
 *****************************************************************************************/

#pragma once

#include "QSI_Global.h"
#include "QSI_USBWrapper.h"

//****************************************************************************************
// CLASS PROTOTYPE
//****************************************************************************************

class QSI_PacketWrapper : public QSI_USBWrapper
{

  private:

    //////////////////////////////////////////////////////////////////////////////////////
    // VARIABLES

    int m_iStatus;          // Stores last status received

    //////////////////////////////////////////////////////////////////////////////////////
    // FUNCTION PROTOTYPES

    int PKT_CheckQueues();  // Returns number of characters in Rx & Tx queues

  public:

    //////////////////////////////////////////////////////////////////////////////////////
    // FUNCTION PROTOTYPES
	QSI_PacketWrapper();
	~QSI_PacketWrapper();

    //int PKT_ListDevices(CSTRING[],int&);        // Lists and returns all FTDI devices on USB
    int PKT_OpenDevice( CameraID cID );           // Opens device by serial number
    int PKT_CloseDevice(void);                    // Closes opened device
    int PKT_SendPacket(PVOID, PVOID, bool);       //
    int PKT_ReadBlock(PVOID, DWORD);              //

};

