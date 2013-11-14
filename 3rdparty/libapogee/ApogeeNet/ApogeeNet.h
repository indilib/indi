#if !defined(_APOGEENET_H__INCLUDED_)
#define _APOGEENET_H__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000


#define WIN32_LEAN_AND_MEAN		// Exclude rarely-used stuff from Windows headers

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#ifndef LINUX
#include <winsock2.h>
#include <wininet.h>
#endif

#include "Apogee.h"

#define HINTERNET int
#define SOCKET int
#ifndef APN_NET_TYPE
#define APN_NET_TYPE unsigned short
#endif


#ifndef IN
#define IN
#endif

#ifndef OUT
#define OUT
#endif

#define ApnNetParity_None 0
#define ApnNetParity_Odd  1
#define ApnNetParity_Even 2


#define APOGEE_IP_PORT_NUMBER	2571
#define MAX_FILENAME_LENGTH		256

#define MAX_REG_NUMBER			80
#define MAX_WRITE_VAL			0xFFFF
#define OPS_PER_BATCH			16
#define OUTPUT_PATH				"results.txt"

#define APOGEE_USERNAME			"admin"
#define APOGEE_USERNAME_LENGTH	5	
#define APOGEE_PASSWORD			"configure"
#define APOGEE_PASSWORD_LENGTH	9

#define HTTP_PREAMBLE			"http://"
#define FPGA_DEFERRED_ACCESS	"/FPGADPC?"
#define FPGA_ACCESS				"/FPGA?"
#define FPGA_FW_UPLOAD			"Load"			
#define FPGA_FW_CLEAR			"InitStart"		
#define MAC_ADDRESS_READ                "/NVRAM?Tag=10&Length=6&Get"
#define MAC_ADDRESS_LENGTH              6
 

#define SERIAL_GET_IP_PORT		"/SERCFG?GetIpPort="
#define SERIAL_GET_BAUD_RATE	"/SERCFG?GetBitRate="
#define SERIAL_SET_BAUD_RATE	"/SERCFG?SetBitRate="
#define SERIAL_GET_FLOW_CONTROL	"/SERCFG?GetFlowControl="
#define SERIAL_SET_FLOW_CONTROL	"/SERCFG?SetFlowControl="
#define SERIAL_GET_PARITY		"/SERCFG?GetParityBits="
#define SERIAL_SET_PARITY		"/SERCFG?SetParityBits="

#define SERIAL_PORT_A			"A"
#define SERIAL_PORT_B			"B"

#define SERIAL_BAUD_RATE_1200	1200
#define SERIAL_BAUD_RATE_2400	2400
#define SERIAL_BAUD_RATE_4800	4800
#define SERIAL_BAUD_RATE_9600	9600
#define SERIAL_BAUD_RATE_19200	19200
#define SERIAL_BAUD_RATE_38400	38400
#define SERIAL_BAUD_RATE_57600	57600
#define SERIAL_BAUD_RATE_115200	115200

#define SERIAL_PARITY_NONE		"N"
#define SERIAL_PARITY_ODD		"O"
#define SERIAL_PARITY_EVEN		"E"

#define SERIAL_FLOW_CONTROL_NONE	"N"
#define SERIAL_FLOW_CONTROL_SW		"S"

#define REBOOT_CMD				"/REBOOT?Submit=Reboot"

#define SESSION_OPEN			"/SESSION?Open"
#define SESSION_CLOSE			"/SESSION?Close"

#define WRITE_REG				"WR="
#define WRITE_DATA				"WD="
#define READ_REG				"RR="

#define APN_UPLOAD_FILE_NAME	"ApnFile.bin"

#define APN_MAX_WRITES_PER_URL	40
#define APN_MAX_READS_PER_URL	40

#define APOGEE_BOUNDARY			"-----------------------------7270616F4070616F6E65742E6F7267"
#define APOGEE_CRLF				"\r\n"

#define DISCOVERY_TIMEOUT_SECS		10
#define DISCOVERY_MAXLINESIZE		255
#define DISCOVERY_MAXBUFFER			(16 * 1024) 
#define DISCOVERY_TOKEN_LENGTH		40
#define DISCOVERY_TOKEN_ID_LENGTH	60

#define SESSION_OPEN_RETVAL		"SessionId="
#define SESSION_CLOSE_RETVAL	"SessionId=\"\"\r\n"


APN_NET_TYPE ApnNetConnect(
						    char	  *HostName );


APN_NET_TYPE ApnNetClose(	
							char	  *HostName  );


APN_NET_TYPE ApnNetDiscovery( unsigned long	Subnet,
							  unsigned short *CameraCount,
							  char			 *CameraList );


APN_NET_TYPE ApnNetReboot(  
							char	  *HostName  );


APN_NET_TYPE ApnNetReadReg(
							char		   *HostName,
							short		   FpgaReg, 
							unsigned short *FpgaData );


APN_NET_TYPE ApnNetWriteReg( 
							 char			*HostName,
							 short			FpgaReg, 
							 unsigned short	FpgaData );


APN_NET_TYPE ApnNetReadRegMulti( 
								 char			*HostName,
								 unsigned short FpgaReg[], 
								 unsigned short FpgaData[], 
								 unsigned short	RegCount );


APN_NET_TYPE ApnNetWriteRegMulti(
								  char			 *HostName,
								  unsigned short FpgaReg,
								  unsigned short FpgaData[],
								  unsigned short RegCount );


APN_NET_TYPE ApnNetWriteRegMultiMRMD(
									  char			 *HostName,
									  unsigned short FpgaReg[],
									  unsigned short FpgaData[],
									  unsigned short RegCount );


APN_NET_TYPE ApnNetStartExp(
							 char		   *HostName,
							 unsigned long ImageWidth,
							 unsigned long ImageHeight );


APN_NET_TYPE ApnNetStopExp( 
							char	  *HostName,
							bool	  DigitizeData );


APN_NET_TYPE ApnNetSetSpeed(
							 char	   *HostName,
							 bool	   HighSpeed );


APN_NET_TYPE ApnNetGetImageTcp( 
							    char		   *HostName,
							    unsigned long  ImageByteCount,
								unsigned short *pMem );

APN_NET_TYPE ApnNetGetNvramData( 
                                                                 char
*HostAddr,
                                                                 char
*pMem );
 
 
APN_NET_TYPE ApnNetGetMacAddress( 
                                                                  char
*HostAddr,
                                                                  char
*pMacAddr );
 

APN_NET_TYPE ApnNetSerialReadIpPort( 
									 char			*HostName,
									 unsigned short SerialId,
									 unsigned short *PortNumber );


APN_NET_TYPE ApnNetStartSockets( );


APN_NET_TYPE ApnNetStopSockets( );


APN_NET_TYPE ApnNetSerialPortOpen( SOCKET		  *SerialSocket,
								   char			  *HostAddr,
								   unsigned short PortNumber );


APN_NET_TYPE ApnNetSerialPortClose( SOCKET *SerialSocket );


APN_NET_TYPE ApnNetSerialReadBaudRate( char			  *HostName,
									   unsigned short SerialId,
									   unsigned long  *BaudRate );


APN_NET_TYPE ApnNetSerialWriteBaudRate( char		   *HostName,
										unsigned short SerialId,
										unsigned long  BaudRate );


APN_NET_TYPE ApnNetSerialReadFlowControl( char			 *HostName,
										  unsigned short SerialId,
								 		  bool			 *FlowControl );


APN_NET_TYPE ApnNetSerialWriteFlowControl( char			  *HostName,
										   unsigned short SerialId,
								 		   bool			  FlowControl );


APN_NET_TYPE ApnNetSerialReadParity( char			*HostName,
									 unsigned short	SerialId,
									 ApnNetParity	*Parity );


APN_NET_TYPE ApnNetSerialWriteParity( char			 *HostName,
									  unsigned short SerialId,
									  ApnNetParity   Parity );


APN_NET_TYPE ApnNetSerialRead( SOCKET			*SerialSocket,
							   char				*ReadBuffer,
							   unsigned short	*BufferCount );


APN_NET_TYPE ApnNetSerialWrite( SOCKET			*SerialSocket,
							    char			*WriteBuffer,
								unsigned short	BufferCount );

#endif
