//////////////////////////////////////////////////////////////////////
//
// ApogeeNet.cpp : Library of basic networking functions for Apogee APn/Alta.
//
// Copyright (c) 2003, 2004 Apogee Instruments, Inc.
//
//////////////////////////////////////////////////////////////////////

#include "ApogeeNet.h"
#include "ApogeeNetErr.h"
#ifdef LINUX
#include "stdafx.h"
#include <fcntl.h>
#include <curl/curl.h>
#include <curl/types.h>
#include <curl/easy.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#define HINTERNET int
#define _snprintf snprintf
extern "C" {
extern int InternetOpen( char *iname, int itype, int *dum, int *dum2, int dum3);
extern int InternetOpenUrl( int g_hSession, char *url, int *dum1, int *dum2, int dum3, int dum4 );
extern void InternetQueryDataAvailable(int handle, long *bcount, int dum1,
int dum2);
extern void InternetReadFile(int handle, char *lpBuffer, long bcount, long
*bread);
extern void InternetCloseHandle( int handle );
           }
#endif
 
// Global variables used in this DLL
HINTERNET       g_hSession;
ULONG           g_NetImgSizeBytes;
BOOLEAN         g_NetImgInProgress;
BOOLEAN         g_FastDownload;
char            g_HostAddr[80];
 


// This is an example of an exported function.
APN_NET_TYPE ApnNetConnect(char *HostAddr )
{
	DWORD		dwBytesAvailable, dwBytesRead;
	HINTERNET	hService;
	int			RetLength;
	char		szUrl[8096];
	char		*lpBuffer;

        strcpy( g_HostAddr, HostAddr );
 	
	g_hSession = InternetOpen( "ApogeeNet", 
				  			   INTERNET_OPEN_TYPE_DIRECT, 
							   NULL, 
							   NULL, 
							   0 );

	if ( g_hSession == 0 )
	{
		return APN_NET_ERR_CONNECT;
	}

	sprintf( szUrl, "%s%s%s", HTTP_PREAMBLE, HostAddr, SESSION_OPEN );

	hService = InternetOpenUrl( g_hSession, szUrl, NULL, 0, 0, 0 );

	if ( hService == 0 )
	{
		return APN_NET_ERR_GENERIC_CGI;		// Failure
	}
	else
	{
		InternetQueryDataAvailable(hService, &dwBytesAvailable, 0, 0);
		lpBuffer = (char *)malloc(dwBytesAvailable+1);
		InternetReadFile(hService, lpBuffer, dwBytesAvailable, &dwBytesRead);
		lpBuffer[dwBytesRead] = 0;

		RetLength = strlen( SESSION_OPEN_RETVAL );

		if ( strncmp( lpBuffer, SESSION_OPEN_RETVAL, RetLength ) != 0 )
		{
			free( lpBuffer );
			return APN_NET_ERR_CONNECT;
		}

		free( lpBuffer );
		InternetCloseHandle( hService );
	}

        g_NetImgInProgress      = FALSE;
        g_NetImgSizeBytes       = 0;
                                                                           
	return APN_NET_SUCCESS;
}


APN_NET_TYPE ApnNetClose(char *HostAddr )
{
	DWORD		dwBytesAvailable, dwBytesRead;
	HINTERNET	hService;
	char		szUrl[8096];
	char		*lpBuffer;

	
	// First make sure we have a valid session already
	if ( g_hSession == 0)
	{
		// Return successful since there is no session open anyway
		return APN_NET_SUCCESS;
	}

	sprintf( szUrl, "%s%s%s", HTTP_PREAMBLE, HostAddr, SESSION_CLOSE );

	hService = InternetOpenUrl( g_hSession, szUrl, NULL, 0, 0, 0 );

	if ( hService == 0 )
	{
		return APN_NET_ERR_GENERIC_CGI;		// Failure
	}
	else
	{
		InternetQueryDataAvailable(hService, &dwBytesAvailable, 0, 0);
		lpBuffer = (char *)malloc(dwBytesAvailable+1);
		InternetReadFile(hService, lpBuffer, dwBytesAvailable, &dwBytesRead);
		lpBuffer[dwBytesRead] = 0;

		if ( strcmp( lpBuffer, SESSION_CLOSE_RETVAL ) != 0 )
		{
			free( lpBuffer );
			return APN_NET_ERR_CLOSE;
		}

		free( lpBuffer );
		InternetCloseHandle( hService );
	}


	g_hSession = 0;

	return APN_NET_SUCCESS;
}


APN_NET_TYPE ApnNetDiscovery( unsigned long  Subnet,
							  unsigned short *CameraCount,
							  char			 *CameraList )
{
#ifndef LINUX
	WSADATA				WinSockData;
#endif
	int					LineLen, UdpPacketLen;
	int					sd;
	struct sockaddr_in  servaddr;
	struct in_addr		in_val;
	const int			on = 1;
	char				Line[DISCOVERY_MAXLINESIZE + 1];
	char				pUdpPacket[DISCOVERY_MAXBUFFER + 1];
	char				szTemp[256];
	char*				CurCamList;
	clock_t				ct;
	unsigned long		addr;

	
	// Initial count of cameras on network should be zero
	*CameraCount = 0;

#ifndef LINUX
	// Try to start the Windows sockets library
	if ( WSAStartup(MAKEWORD(1, 0), &WinSockData) != 0 )
	{
		return 1;
	}
#endif
	memset(&servaddr, 0, sizeof(servaddr));

	servaddr.sin_family			= AF_INET;
	servaddr.sin_port			= htons( APOGEE_IP_PORT_NUMBER );
	servaddr.sin_addr.s_addr	= Subnet;

	sd = socket( AF_INET, SOCK_DGRAM, 0 );
	
	setsockopt( sd, SOL_SOCKET, SO_BROADCAST, (char*) &on, sizeof(on) );

	LineLen = _snprintf( Line, 
						 DISCOVERY_MAXLINESIZE, 
						 "Discovery::Request-Except: \"%s\"; 0x%lX; %ld; %ld; %ld; %ld" APOGEE_CRLF APOGEE_CRLF,
						 "Apogee", 
						 0x12345678, 
						 0, 
						 DISCOVERY_TIMEOUT_SECS / 2, 
						 0, 0 );

	// Build the header for the message
	UdpPacketLen = _snprintf( pUdpPacket, 
							  DISCOVERY_MAXBUFFER, 
							  "MIME-Version: 1.0" APOGEE_CRLF
							  "Content-Type: application/octet-stream" APOGEE_CRLF
							  "Content-Transfer-Encoding: binary" APOGEE_CRLF
							  "Content-Length: 0x%X" APOGEE_CRLF
							  "X-Project: Apogee" APOGEE_CRLF
							  "X-Project-Version: 0.1" APOGEE_CRLF APOGEE_CRLF,
							  LineLen );
													

	memcpy(pUdpPacket + UdpPacketLen, Line, LineLen);
	UdpPacketLen += LineLen;

	sendto( sd, pUdpPacket, UdpPacketLen, 0, (struct sockaddr *)&servaddr, sizeof(servaddr) );

	// Wait for UDP replies
	struct	timeval tv;
	fd_set	rset;
	int		n;
	char	szResponseToken[DISCOVERY_TOKEN_ID_LENGTH];
	
	char	szId[DISCOVERY_TOKEN_LENGTH];
	char	szTag[DISCOVERY_TOKEN_LENGTH];
	char	szTime[DISCOVERY_TOKEN_LENGTH];
	
	char	szIpAddrToken[DISCOVERY_TOKEN_ID_LENGTH];
	char	szIpAddr[DISCOVERY_TOKEN_LENGTH];
	
	char	szPortToken[DISCOVERY_TOKEN_ID_LENGTH];
	char	szPort[DISCOVERY_TOKEN_LENGTH];
	
	char	szNameToken[DISCOVERY_TOKEN_ID_LENGTH];
	char	szName[DISCOVERY_TOKEN_LENGTH];

	char	szCamNumber[DISCOVERY_TOKEN_LENGTH];


	ct = clock() + DISCOVERY_TIMEOUT_SECS * CLOCKS_PER_SEC;
	
	tv.tv_sec	= 0;
	tv.tv_usec	= 0;
	size_t		CurLength;
	size_t		NewLength;


	while (clock() <= ct) 
	{
		FD_ZERO( &rset );
		FD_SET( sd, &rset );

		if ( select( sd + 1, &rset, NULL, NULL, &tv ) > 0 ) 
		{
			n = recvfrom(sd, pUdpPacket, DISCOVERY_MAXBUFFER, 0, NULL, NULL);

			// Process the returned packets. 
			pUdpPacket[n] = '\0';	// add a NULL to the close of the string

			sscanf( pUdpPacket, "%s %s %s %s"
								"%s %s"
								"%s %s"
								"%s",
								szResponseToken, szId, szTag, szTime,
								szIpAddrToken, szIpAddr,
								szPortToken, szPort,
								szNameToken );

			strtok(pUdpPacket, "\"");
			strtok(NULL, "\"");
			strtok(NULL, "\"");
			strcpy(szName, strtok(NULL, "\""));

			if ( strcmp("\r\nSessionId=", szName) == 0 )
			{
				strcpy(szName, "Camera Unnamed");
			}

			// Increment number of cameras on network
			(*CameraCount)++;

			sprintf( szCamNumber, "%u ", *CameraCount );

			sprintf( szTemp, "%u 0x%08X %u ", szCamNumber, szIpAddr, szPort );

			// determine string lengths  
			NewLength = strlen( szTemp );
			CurLength = strlen( CameraList );

			CurCamList = (char*)calloc( CurLength + 1, sizeof(char) );

			// copy current string for safe keeping
			strcpy( CurCamList, CameraList );

			// allocate space for combined string
			CameraList = (char*)calloc( CurLength + NewLength + 1, sizeof(char) );

			sprintf( CameraList, "%s%s", CameraList, szTemp );

		}
	}

	CurLength = strlen( CameraList );

	CurCamList = (char*)calloc( CurLength + 1, sizeof(char) );

	// copy current string for safe keeping
	strcpy( CurCamList, CameraList );

	// allocate space for combined string
	CameraList = (char*)calloc( CurLength + sizeof(szCamNumber) + 1, sizeof(char) );

	sprintf( CameraList, "%s%s", szCamNumber, CameraList );

	// Stop the Windows sockets library
#ifndef LINUX
	WSACleanup();
#endif
        g_NetImgInProgress      = FALSE;
        g_NetImgSizeBytes       = 0;
                                                                           

	return APN_NET_SUCCESS;
}


APN_NET_TYPE ApnNetReboot( char *HostAddr )
{
	HINTERNET	hService;
	DWORD		dwBytesAvailable, dwBytesRead;
	char		szUrl[8096];
	char*		lpBuffer;

	sprintf(szUrl, "%s%s%s", HTTP_PREAMBLE, HostAddr, REBOOT_CMD);

	hService = InternetOpenUrl( g_hSession, szUrl, NULL, 0, 0, 0 );
	
	if ( hService == 0 )
	{
		return APN_NET_ERR_GENERIC_CGI;
	}
	else
	{
		InternetQueryDataAvailable(hService, &dwBytesAvailable, 0, 0);
		lpBuffer = (char*)malloc(dwBytesAvailable+1);
		InternetReadFile(hService, lpBuffer, dwBytesAvailable, &dwBytesRead);
		lpBuffer[dwBytesRead] = 0;

		// we actually need to check the buffer passed back here.

		free( lpBuffer );
		InternetCloseHandle( hService );
	}

	return APN_NET_SUCCESS;
}


APN_NET_TYPE ApnNetReadReg(
						    char			*HostAddr, 
							short			FpgaReg, 
							unsigned short	*FpgaData )
{
	DWORD		dwBytesAvailable, dwBytesRead;
	HINTERNET	hService;
	char		szUrl[8096];
	char		*szStopStr;
	char		*lpBuffer;


	// First make sure we have a valid session already
	if ( g_hSession == 0)
	{
		return APN_NET_ERR_CONNECT;
	}

	sprintf(szUrl, "%s%s%s%s%d", HTTP_PREAMBLE, HostAddr, FPGA_ACCESS, READ_REG, FpgaReg);

	hService = InternetOpenUrl( g_hSession, szUrl, NULL, 0, 0, 0 );

	if ( hService == 0 )
	{
		return APN_NET_ERR_GENERIC_CGI;		// Failure
	}
	else
	{
		InternetQueryDataAvailable(hService, &dwBytesAvailable, 0, 0);
		lpBuffer = (char *)malloc(dwBytesAvailable+1);
		InternetReadFile(hService, lpBuffer, dwBytesAvailable, &dwBytesRead);
		lpBuffer[dwBytesRead] = 0;

		strtok( lpBuffer, "=");
		
		*FpgaData = (USHORT)strtoul( strtok( NULL, "="), &szStopStr, 16 );

		free(lpBuffer);

		InternetCloseHandle( hService );
	}

	return APN_NET_SUCCESS;			// Success
}


APN_NET_TYPE ApnNetWriteReg( 
							 char			*HostAddr, 
							 short			FpgaReg, 
							 unsigned short FpgaData )
{
	DWORD		dwBytesAvailable, dwBytesRead;
	HINTERNET	hService;
	char		szUrl[8096];
	char		*szStopStr;
	char		*pBuffer;
	char		*pTokenBuffer;
	USHORT		RetVal;


	// First make sure we have a valid session already
	if ( g_hSession == 0)
	{
		return APN_NET_ERR_CONNECT;	// Failure
	}

	/*
	if ( FpgaReg == 0 )
	{
		sprintf(szUrl, "%s%s%s%s%u&%s0x%X", HTTP_PREAMBLE, g_HostAddr, FPGA_DEFERRED_ACCESS, 
			WRITE_REG, FpgaReg, WRITE_DATA, FpgaData);
	}
	else
	{
		sprintf(szUrl, "%s%s%s%s%u&%s0x%X", HTTP_PREAMBLE, g_HostAddr, FPGA_ACCESS, 
			WRITE_REG, FpgaReg, WRITE_DATA, FpgaData);
	}
	*/

	sprintf(szUrl, "%s%s%s%s%u&%s0x%X", HTTP_PREAMBLE, HostAddr, FPGA_ACCESS, 
		WRITE_REG, FpgaReg, WRITE_DATA, FpgaData);

	hService = InternetOpenUrl( g_hSession, szUrl, NULL, 0, 0, 0 );

	if ( hService == 0 )
	{
		return APN_NET_ERR_GENERIC_CGI;		// Failure
	}
	else
	{
		InternetQueryDataAvailable( hService, &dwBytesAvailable, 0, 0 );
		pBuffer = (char*)malloc( dwBytesAvailable+1 );
		InternetReadFile( hService, pBuffer, dwBytesAvailable, &dwBytesRead );
		pBuffer[dwBytesRead]=0;

		pTokenBuffer = (char*)malloc( dwBytesAvailable+1 );
		// strcpy( pTokenBuffer, strtok( pBuffer, "=") );
		// OutputDebugString( pTokenBuffer );

		// RetVal = (USHORT)strtoul( strtok( NULL, "="), &szStopStr, 16 );

		/*
		if ( (FpgaData != RetVal) && (FpgaReg != 1) )
		{
			return 1;	// Failed
		}
		*/

		free( pBuffer );
		free( pTokenBuffer );

		InternetCloseHandle( hService );
	}

	return APN_NET_SUCCESS;	// Success
}


APN_NET_TYPE ApnNetWriteRegMulti(
								  char			 *HostAddr,
								  unsigned short FpgaReg, 
								  unsigned short FpgaData[], 
								  unsigned short RegCount )
{
	DWORD		dwBytesAvailable, dwBytesRead;
	HINTERNET	hService;
	char		szUrl[8096];
	char		szTemp[50];
	char		*szStopStr;
	char		*lpBuffer;
	USHORT		RetVal;
	USHORT		LoopCount;
	USHORT		LoopCountRemainder;
	int			i;
	USHORT		StartIndex;
	USHORT		EndIndex;


	// First make sure we have a valid session already
	if ( g_hSession == 0)
	{
		return APN_NET_ERR_CONNECT;	// Failure
	}

	LoopCount			= (unsigned short)( RegCount / APN_MAX_WRITES_PER_URL );
	LoopCountRemainder	= (unsigned short)( RegCount % APN_MAX_WRITES_PER_URL );

	// Process the main blocks of the loop
	for ( i=0; i<LoopCount; i++ )
	{
		// Build the initial URL string
		sprintf(szUrl, "%s%s%s%s%u", HTTP_PREAMBLE, HostAddr, FPGA_ACCESS, 
			WRITE_REG, FpgaReg );

		StartIndex = i * APN_MAX_WRITES_PER_URL;
		EndIndex = StartIndex + APN_MAX_WRITES_PER_URL;
		for ( int j=StartIndex; j<EndIndex; j++ )
		{
			sprintf( szTemp, "&%s0x%04X", WRITE_DATA, FpgaData[j] );
			strcat( szUrl, szTemp ); 
		}

		hService = InternetOpenUrl( g_hSession, szUrl, NULL, 0, 0, 0 );

		if ( hService == 0 )
		{
			return APN_NET_ERR_GENERIC_CGI;		// Failure
		}
		else
		{
			InternetQueryDataAvailable(hService, &dwBytesAvailable, 0, 0);
			lpBuffer = (char*)malloc(dwBytesAvailable+1);
			InternetReadFile(hService, lpBuffer, dwBytesAvailable, &dwBytesRead);
			lpBuffer[dwBytesRead]=0;

			strtok( lpBuffer, "=");
			RetVal = (USHORT)strtoul( strtok( NULL, "="), &szStopStr, 16 );

			/*
			if ( (FpgaData != RetVal) && (FpgaReg != 1) )
			{
				return 1;	// Failed
			}
			*/

			free(lpBuffer);

			InternetCloseHandle( hService );
		}
	}

	if ( LoopCountRemainder != 0 )
	{
		// Build the initial URL string
		sprintf(szUrl, "%s%s%s%s%u", HTTP_PREAMBLE, HostAddr, FPGA_ACCESS, 
			WRITE_REG, FpgaReg );

		StartIndex = LoopCount * APN_MAX_WRITES_PER_URL;
		EndIndex = StartIndex + LoopCountRemainder;
		for ( int j=StartIndex; j<EndIndex; j++ )
		{
			sprintf( szTemp, "&%s0x%04X", WRITE_DATA, FpgaData[j] );
			strcat( szUrl, szTemp ); 
		}

		hService = InternetOpenUrl( g_hSession, szUrl, NULL, 0, 0, 0 );

		if ( hService == 0 )
		{
			return APN_NET_ERR_GENERIC_CGI;		// Failure
		}
		else
		{
			InternetQueryDataAvailable(hService, &dwBytesAvailable, 0, 0);
			lpBuffer = (char*)malloc(dwBytesAvailable+1);
			InternetReadFile(hService, lpBuffer, dwBytesAvailable, &dwBytesRead);
			lpBuffer[dwBytesRead]=0;

			strtok( lpBuffer, "=");
			RetVal = strtoul( strtok( NULL, "="), &szStopStr, 16 );

			/*
			if ( (FpgaData != RetVal) && (FpgaReg != 1) )
			{
				return 1;	// Failed
			}
			*/

			free(lpBuffer);

			InternetCloseHandle( hService );
		}
	}

	return APN_NET_SUCCESS;	// Success
}


APN_NET_TYPE ApnNetWriteRegMultiMRMD( 
									  char			 *HostAddr,
									  unsigned short FpgaReg[],
									  unsigned short FpgaData[],
									  unsigned short RegCount )
{
	DWORD		dwBytesAvailable, dwBytesRead;
	HINTERNET	hService;
	char		szUrl[8096];
	char		szTemp[50];
	char		*szStopStr;
	char		*lpBuffer;
	USHORT		RetVal;
	USHORT		LoopCount;
	USHORT		LoopCountRemainder;
	int		i,j;
	USHORT		StartIndex;
	USHORT		EndIndex;


	// First make sure we have a valid session already
	if ( g_hSession == 0)
	{
		return APN_NET_ERR_CONNECT;	// Failure
	}

	LoopCount			= (unsigned short)( RegCount / APN_MAX_WRITES_PER_URL );
	LoopCountRemainder	= (unsigned short)( RegCount % APN_MAX_WRITES_PER_URL );

	// Process the main blocks of the loop
	for ( i=0; i<LoopCount; i++ )
	{
		// Build the initial URL string
		sprintf(szUrl, "%s%s%s", HTTP_PREAMBLE, HostAddr, FPGA_ACCESS );

		StartIndex = i * APN_MAX_WRITES_PER_URL;
		EndIndex = StartIndex + APN_MAX_WRITES_PER_URL;
		for ( j=StartIndex; j<EndIndex; j++ )
		{
			if ( j != StartIndex )
				strcat( szUrl, "&" );

			sprintf( szTemp, "%s%u&%s0x%04X", WRITE_REG, FpgaReg[j], WRITE_DATA, FpgaData[j] );
			strcat( szUrl, szTemp ); 
		}

		hService = InternetOpenUrl( g_hSession, szUrl, NULL, 0, 0, 0 );

		if ( hService == 0 )
		{
			return APN_NET_ERR_GENERIC_CGI;		// Failure
		}
		else
		{
			InternetQueryDataAvailable(hService, &dwBytesAvailable, 0, 0);
			lpBuffer = (char*)malloc(dwBytesAvailable+1);
			InternetReadFile(hService, lpBuffer, dwBytesAvailable, &dwBytesRead);
			lpBuffer[dwBytesRead]=0;

			free(lpBuffer);

			InternetCloseHandle( hService );
		}
	}

	if ( LoopCountRemainder != 0 )
	{
		// Build the initial URL string
		sprintf(szUrl, "%s%s%s", HTTP_PREAMBLE, HostAddr, FPGA_ACCESS );

		StartIndex = LoopCount * APN_MAX_READS_PER_URL;
		EndIndex = StartIndex + LoopCountRemainder;
		for ( j=StartIndex; j<EndIndex; j++ )
		{
			if ( j != StartIndex )
				strcat( szUrl, "&" );

			sprintf( szTemp, "%s%u&%s0x%04X", WRITE_REG, FpgaReg[j], WRITE_DATA, FpgaData[j] );
			strcat( szUrl, szTemp ); 
		}

		hService = InternetOpenUrl( g_hSession, szUrl, NULL, 0, 0, 0 );

		if ( hService == 0 )
		{
			return APN_NET_ERR_GENERIC_CGI;		// Failure
		}
		else
		{
			InternetQueryDataAvailable(hService, &dwBytesAvailable, 0, 0);
			lpBuffer = (char*)malloc(dwBytesAvailable+1);
			InternetReadFile(hService, lpBuffer, dwBytesAvailable, &dwBytesRead);
			lpBuffer[dwBytesRead]=0;

			free(lpBuffer);

			InternetCloseHandle( hService );
		}
	}

	return APN_NET_SUCCESS;	// Success
}


APN_NET_TYPE ApnNetReadRegMulti( 
								 char			*HostAddr,
								 unsigned short FpgaReg[], 
								 unsigned short FpgaData[], 
								 unsigned short RegCount )
{
	DWORD		dwBytesAvailable, dwBytesRead;
	HINTERNET	hService;
	char		szUrl[8096];
	char		szTemp[50];
	char		*szStopStr;
	char		*lpBuffer;
	USHORT		RetVal;
	USHORT		LoopCount;
	USHORT		LoopCountRemainder;
	int		i,j;
	USHORT		StartIndex;
	USHORT		EndIndex;


	// First make sure we have a valid session already
	if ( g_hSession == 0)
	{
		return APN_NET_ERR_CONNECT;	// Failure
	}

	LoopCount			= (unsigned short)( RegCount / APN_MAX_READS_PER_URL );
	LoopCountRemainder	= (unsigned short)( RegCount % APN_MAX_READS_PER_URL );

	// Process the main blocks of the loop
	for ( i=0; i<LoopCount; i++ )
	{
		// Build the initial URL string
		sprintf(szUrl, "%s%s%s", HTTP_PREAMBLE, HostAddr, FPGA_ACCESS );

		StartIndex = i * APN_MAX_READS_PER_URL;
		EndIndex = StartIndex + APN_MAX_READS_PER_URL;
		for ( j=StartIndex; j<EndIndex; j++ )
		{
			if ( j != StartIndex )
				strcat( szUrl, "&" );

			sprintf( szTemp, "%s%u", READ_REG, FpgaReg[j] );
			strcat( szUrl, szTemp ); 
		}

		hService = InternetOpenUrl( g_hSession, szUrl, NULL, 0, 0, 0 );

		if ( hService == 0 )
		{
			return APN_NET_ERR_GENERIC_CGI;		// Failure
		}
		else
		{
			InternetQueryDataAvailable(hService, &dwBytesAvailable, 0, 0);
			lpBuffer = (char*)malloc(dwBytesAvailable+1);
			InternetReadFile(hService, lpBuffer, dwBytesAvailable, &dwBytesRead);
			lpBuffer[dwBytesRead]=0;

			strtok( lpBuffer, "=");
			for ( j=StartIndex; j<EndIndex; j++ )
			{
				FpgaData[j] = (USHORT)strtoul( strtok( NULL, "="), &szStopStr, 16 );
			}

			/*
			if ( (FpgaData != RetVal) && (FpgaReg != 1) )
			{
				return 1;	// Failed
			}
			*/

			free(lpBuffer);

			InternetCloseHandle( hService );
		}
	}

	if ( LoopCountRemainder != 0 )
	{
		// Build the initial URL string
		sprintf( szUrl, "%s%s%s", HTTP_PREAMBLE, HostAddr, FPGA_ACCESS );

		StartIndex = LoopCount * APN_MAX_READS_PER_URL;
		EndIndex = StartIndex + LoopCountRemainder;
		for ( j=StartIndex; j<EndIndex; j++ )
		{
			if ( j != StartIndex )
				strcat( szUrl, "&" );

			sprintf( szTemp, "%s%u", READ_REG, FpgaReg[j] );
			strcat( szUrl, szTemp ); 
		}

		hService = InternetOpenUrl( g_hSession, szUrl, NULL, 0, 0, 0 );

		if ( hService == 0 )
		{
			return APN_NET_ERR_GENERIC_CGI;		// Failure
		}
		else
		{
			InternetQueryDataAvailable(hService, &dwBytesAvailable, 0, 0);
			lpBuffer = (char*)malloc(dwBytesAvailable+1);
			InternetReadFile(hService, lpBuffer, dwBytesAvailable, &dwBytesRead);
			lpBuffer[dwBytesRead]=0;

			strtok( lpBuffer, "=");
			for ( j=StartIndex; j<EndIndex; j++ )
			{
				FpgaData[j] = (USHORT)strtoul( strtok( NULL, "="), &szStopStr, 16 );
			}

			/*
			if ( (FpgaData != RetVal) && (FpgaReg != 1) )
			{
				return 1;	// Failed
			}
			*/

			free(lpBuffer);

			InternetCloseHandle( hService );
		}
	}

	return APN_NET_SUCCESS;	// Success
}


APN_NET_TYPE ApnNetStartExp( 
							 char			*HostAddr,
							 unsigned long	ImageWidth, 
							 unsigned long	ImageHeight )
{
	HINTERNET	hService;
	DWORD		dwBytesAvailable, dwBytesRead;
	char		szUrl[8096];


	// First make sure we have a valid session already
	if ( g_hSession == 0)
	{
		return APN_NET_ERR_CONNECT;			// Failure
	}

	// Check to make sure width and height are non-zero
	if ( ( ImageWidth == 0 ) || ( ImageHeight == 0 ) )
	{
		return APN_NET_ERR_IMAGE_PARAMS;		// Failure
	}

	// Check to make sure our total image size is less than or equal to what we can do
	if ( ( ImageWidth * ImageHeight * 2 ) > ( 1024 * 1024 * 28 ) )
	{
		return APN_NET_ERR_IMAGE_PARAMS;		// Failure
	}

	///////////////////////////////////////////////////////////////////////
	// time code
	// ci= Mode --- poll mode is bit 0 
	// ci= Mode --- zero mem is bit 3
	// Format:  CI=Mode,UdpBlockSize,Width,Height
	sprintf(szUrl, "http://%s/FPGA?CI=0,0,%u,%u,0xFFFFFFFF", HostAddr, ImageWidth, ImageHeight);

	hService = InternetOpenUrl( g_hSession, 
								szUrl, 
								NULL, 
								0, 
								0, 
								0 );

	if ( hService == 0 )
	{
		return APN_NET_ERR_GENERIC_CGI;		// Failure
	}
	else
	{
		InternetQueryDataAvailable(hService, &dwBytesAvailable, 0, 0);
		char *lpBuffer = (char*)malloc(dwBytesAvailable+1);
		InternetReadFile(hService, lpBuffer, dwBytesAvailable, &dwBytesRead);
		lpBuffer[dwBytesRead]=0;

		free(lpBuffer);

		InternetCloseHandle( hService );
	}

	return APN_NET_SUCCESS;	// Success
}


APN_NET_TYPE ApnNetStopExp( 
							char		*HostAddr,
							bool		DigitizeData )
{
	// First make sure we have a valid session already
	if ( g_hSession == 0)
	{
		return APN_NET_ERR_CONNECT;			// Failure
	}

	return APN_NET_SUCCESS;	// Success
}


APN_NET_TYPE ApnNetSetSpeed( 
							 char		*HostAddr,
							 bool		HighSpeed )
{
	// High speed mode is UDP
	// Normal mode is TCP

	if ( HighSpeed )
	{
	}
	else
	{
	}

	return APN_NET_SUCCESS;	// Success
}


APN_NET_TYPE ApnNetGetImageTcp( 
								char			*HostAddr,
								unsigned long	ImageByteCount,
								unsigned short	*pMem )
{
	HINTERNET	hService;
	DWORD		dwBytesAvailable, dwBytesRead;
	ULONG		dwImgSize;
	char		szUrl[8096];


	// First make sure we have a valid session already
	if ( g_hSession == 0)
	{
		return APN_NET_ERR_CONNECT;
	}

	sprintf( szUrl, "http://%s/UE/image.bin", HostAddr );

	hService = InternetOpenUrl( g_hSession, 
								szUrl, 
								NULL, 
								0,
								INTERNET_FLAG_NO_CACHE_WRITE |
								INTERNET_FLAG_KEEP_CONNECTION,
								0 );

	if ( hService == 0 )
	{
		return APN_NET_ERR_GENERIC_CGI;		// Failure
	}
	else
	{
		InternetReadFile( hService, (char *)pMem, ImageByteCount, &dwBytesRead );
                if (dwBytesRead == 0) 
		{
			return APN_NET_ERR_IMAGE_DATA;	// Failure -- Retrieved incorrect amount of data
		}
		
		InternetCloseHandle( hService );
	}
	
	dwImgSize = ImageByteCount / 2;

	for (ULONG j=0; j<dwImgSize; j++)
	{
		pMem[j] = ntohs(pMem[j]);
	}

	return APN_NET_SUCCESS;	// Success
}


APN_NET_TYPE ApnNetGetNvramData( 
                                                                 char *HostAddr, 
                                                                 char *pMem )
{
        HINTERNET       hService;
        DWORD           dwBytesAvailable, dwBytesRead;
        char            *lpBuffer;
        char            szUrl[80];
 
 
        // First make sure we have a valid session already
        if ( g_hSession == 0)
        {
                return APN_NET_ERR_CONNECT;
        }
 
        sprintf(szUrl, "http://%s/UE/nvram.bin", HostAddr);
 
        hService = InternetOpenUrl( g_hSession,
                                                                szUrl,
                                                                NULL,
                                                                0,
                                                                INTERNET_FLAG_NO_CACHE_WRITE |
                                                                INTERNET_FLAG_KEEP_CONNECTION,
                                                                0 );
                                                                                
        if ( hService == 0 )
        {
                return APN_NET_ERR_GENERIC_CGI;           // Failure
        }
        else
        {
                InternetQueryDataAvailable(hService, &dwBytesAvailable, 0, 0);
                lpBuffer = (char*)malloc(dwBytesAvailable);
                InternetReadFile(hService, lpBuffer, dwBytesAvailable, &dwBytesRead);
                                                                                
                // Copy the downloaded buffer back to the caller's buffer
                // pMem must be greater than lpBuffer!
                memcpy( pMem, lpBuffer, dwBytesAvailable );
                                                                                
                free( lpBuffer );
                InternetCloseHandle( hService );
        }
        return APN_NET_SUCCESS;   // Success
}
 
APN_NET_TYPE ApnNetGetMacAddress( 
                                                                  char *HostAddr,
                                                                  char *pMacAddr )
{
        HINTERNET               hService;
        APN_NET_TYPE    ApStatus;
        DWORD                   dwBytesAvailable, dwBytesRead;
        char                    *lpBuffer;
        char                    szUrl[80];
 
 
        // First make sure we have a valid session already
        if ( g_hSession == 0)
        {
                return APN_NET_ERR_CONNECT;
        }
 
        sprintf(szUrl, "%s%s%s", HTTP_PREAMBLE, HostAddr, MAC_ADDRESS_READ );
 
        hService = InternetOpenUrl( g_hSession, szUrl, NULL, 0, 0, 0 );
 
        if ( hService == 0 )
        {
                return APN_NET_ERR_GENERIC_CGI;
        }
        else
        {
                InternetQueryDataAvailable(hService, &dwBytesAvailable, 0, 0);
                lpBuffer = (char*)malloc(dwBytesAvailable+1);
                InternetReadFile(hService, lpBuffer, dwBytesAvailable, &dwBytesRead);
                lpBuffer[dwBytesRead] = 0;
 
                // need to check the buffer passed back here.
 
                free( lpBuffer );
                InternetCloseHandle( hService );
        }
 
        unsigned short buffer[3];
                                                                                
        ApStatus = ApnNetGetNvramData( HostAddr, (char*) buffer );
                                                                                
        sprintf( pMacAddr, "%04X%04X%04X", ntohs(buffer[0]), ntohs(buffer[1]), ntohs(buffer[2]) );
                                                                                
        return APN_NET_SUCCESS;
}
                                                                                



APN_NET_TYPE ApnNetSerialReadIpPort( 
									 char			*HostAddr,
									 unsigned short SerialId,
									 unsigned short	*PortNumber )
{
	DWORD		dwBytesAvailable, dwBytesRead;
	HINTERNET	hService;
	char		szUrl[8096];
	char		szSerialId[5];
	char		*szStopStr;
	char		*lpBuffer;


	// First make sure we have a valid session already
	if ( g_hSession == 0)
	{
		return APN_NET_ERR_CONNECT;
	}

	// Make sure the serial port selection is valid
	if ( (SerialId != 0) && (SerialId != 1) )
	{
		return APN_NET_ERR_SERIAL_ID;
	}

	// Create a string based on the particular serial port
	if ( SerialId == 0 )
		strcpy( szSerialId, SERIAL_PORT_A );
	else
		strcpy( szSerialId, SERIAL_PORT_B );

	sprintf(szUrl, "%s%s%s%s", HTTP_PREAMBLE, HostAddr, SERIAL_GET_IP_PORT, szSerialId);

	hService = InternetOpenUrl( g_hSession, szUrl, NULL, 0, 0, 0 );

	if ( hService == 0 )
	{
		return APN_NET_ERR_GENERIC_CGI;		// Failure
	}
	else
	{
		InternetQueryDataAvailable(hService, &dwBytesAvailable, 0, 0);
		lpBuffer = (char *)malloc(dwBytesAvailable+1);
		InternetReadFile(hService, lpBuffer, dwBytesAvailable, &dwBytesRead);
		lpBuffer[dwBytesRead] = 0;

		strtok( lpBuffer, "=");
		
		*PortNumber = (USHORT)strtoul( strtok( NULL, ","), &szStopStr, 10 );

		free(lpBuffer);

		InternetCloseHandle( hService );
	}

	return APN_NET_SUCCESS;	// Success
}


APN_NET_TYPE ApnNetStartSockets( ) 
{
#ifndef LINUX
	WSADATA stWSAData;
	int		nRet;

	nRet = WSAStartup( MAKEWORD(2, 0), &stWSAData );
#endif
	return APN_NET_SUCCESS;
} 


APN_NET_TYPE ApnNetStopSockets( )
{
#ifndef LINUX
	WSACleanup();
#endif
	return APN_NET_SUCCESS;
} 


APN_NET_TYPE ApnNetSerialPortOpen( SOCKET		  *SerialSocket,
								   char			  *HostAddr,
								   unsigned short PortNumber )
{
	unsigned long		CamIpAddress;
	struct sockaddr_in  CamPortAddr;
	int oldopts;


	CamIpAddress = inet_addr( HostAddr );

	*SerialSocket = socket( AF_INET, SOCK_STREAM, IPPROTO_TCP );
	
	CamPortAddr.sin_family		= AF_INET;
	CamPortAddr.sin_port		= htons( PortNumber );
	CamPortAddr.sin_addr.s_addr	= CamIpAddress;

	if ( connect(*SerialSocket, (struct sockaddr *)&CamPortAddr, sizeof(CamPortAddr)) == -1 )
	{
		return APN_NET_ERR_SERIAL_CONNECT;
	}
	oldopts = fcntl(*SerialSocket, F_GETFL, 0);
	fcntl(*SerialSocket, F_SETFL, oldopts | O_NONBLOCK);

	return APN_NET_SUCCESS;	// Success
}


APN_NET_TYPE ApnNetSerialPortClose( SOCKET *SerialSocket )
{
	close( *SerialSocket );

	return APN_NET_SUCCESS;	// Success
}


APN_NET_TYPE ApnNetSerialReadBaudRate( char			  *HostAddr,
									   unsigned short SerialId,
									   unsigned long  *BaudRate )
{
	DWORD		dwBytesAvailable, dwBytesRead;
	HINTERNET	hSession;
	HINTERNET	hService;
	char		szUrl[8096];
	char		szSerialId[5];
	char		*szStopStr;
	char		*pBuffer;

	int			RetLength;


	// Make sure the serial port selection is valid
	if ( (SerialId != 0) && (SerialId != 1) )
	{
		return APN_NET_ERR_SERIAL_ID;
	}

//	ApnNetConnect( &hSession, HostAddr );

	// Create a string based on the particular serial port
	if ( SerialId == 0 )
		strcpy( szSerialId, SERIAL_PORT_A );
	else
		strcpy( szSerialId, SERIAL_PORT_B );

	sprintf(szUrl, "%s%s%s%s", HTTP_PREAMBLE, HostAddr, SERIAL_GET_BAUD_RATE, szSerialId);

	hService = InternetOpenUrl( hSession, szUrl, NULL, 0, 0, 0);

	if ( hService == 0 )
	{
		return APN_NET_ERR_GENERIC_CGI;		// Failure
	}
	else
	{
		InternetQueryDataAvailable(hService, &dwBytesAvailable, 0, 0);
		pBuffer = (char *)malloc(dwBytesAvailable+1);
		InternetReadFile(hService, pBuffer, dwBytesAvailable, &dwBytesRead);
		pBuffer[dwBytesRead] = '\0';

		strtok( pBuffer, "," );
		
		*BaudRate = strtoul( strtok( NULL, ","), &szStopStr, 10 );

		free( pBuffer );

		InternetCloseHandle( hService );
	}

//	ApnNetClose( &hSession, HostAddr );

	return APN_NET_SUCCESS;	// Success
}


APN_NET_TYPE ApnNetSerialWriteBaudRate( char		   *HostAddr,
										unsigned short SerialId,
										unsigned long  BaudRate )
{
	DWORD		dwBytesAvailable, dwBytesRead;
	HINTERNET	hSession;
	HINTERNET	hService;
	char		szUrl[8096];
	char		szSerialId[5];
	char		*szStopStr;
	char		*pBuffer;
	USHORT		RetVal;


	// Make sure the serial port selection is valid
	if ( (SerialId != 0) && (SerialId != 1) )
	{
		return APN_NET_ERR_SERIAL_ID;
	}

	// Verify that the baud rate is a valid number
	if ( (BaudRate != SERIAL_BAUD_RATE_1200) && (BaudRate != SERIAL_BAUD_RATE_2400) &&
		 (BaudRate != SERIAL_BAUD_RATE_4800) && (BaudRate != SERIAL_BAUD_RATE_9600) &&
		 (BaudRate != SERIAL_BAUD_RATE_19200) && (BaudRate != SERIAL_BAUD_RATE_38400) &&
		 (BaudRate != SERIAL_BAUD_RATE_57600) && (BaudRate != SERIAL_BAUD_RATE_115200) )
	{
		return APN_NET_ERR_SERIAL_BAUDRATE;
	}

//	ApnNetConnect( &hSession, HostAddr );

	// Create a string based on the particular serial port
	if ( SerialId == 0 )
		strcpy( szSerialId, SERIAL_PORT_A );
	else
		strcpy( szSerialId, SERIAL_PORT_B );

	sprintf(szUrl, "%s%s%s%s,%ul", HTTP_PREAMBLE, 
								   HostAddr, 
								   SERIAL_SET_BAUD_RATE, 
								   szSerialId,
								   BaudRate );

	hService = InternetOpenUrl( hSession, szUrl, NULL, 0, 0, 0 );

	if ( hService == 0 )
	{
		return APN_NET_ERR_GENERIC_CGI;		// Failure
	}
	else
	{
		InternetQueryDataAvailable(hService, &dwBytesAvailable, 0, 0);
		pBuffer = (char *)malloc(dwBytesAvailable+1);
		InternetReadFile(hService, pBuffer, dwBytesAvailable, &dwBytesRead);
		pBuffer[dwBytesRead] = 0;

		free( pBuffer );

		InternetCloseHandle( hService );
	}

//	ApnNetClose( &hSession, HostAddr );

	return APN_NET_SUCCESS;	// Success
}


APN_NET_TYPE ApnNetSerialReadFlowControl( char			 *HostAddr,
										  unsigned short SerialId,
								 		  bool			 *FlowControl )
{
	DWORD		dwBytesAvailable, dwBytesRead;
	HINTERNET	hSession;
	HINTERNET	hService;
	char		szUrl[8096];
	char		szSerialId[5];
	char		szRetVal[5];
	char		*szStopStr;
	char		*pBuffer;


	// Make sure the serial port selection is valid
	if ( (SerialId != 0) && (SerialId != 1) )
	{
		return APN_NET_ERR_SERIAL_ID;
	}

//	ApnNetConnect( &hSession, HostAddr );

	// Create a string based on the particular serial port
	if ( SerialId == 0 )
		strcpy( szSerialId, SERIAL_PORT_A );
	else
		strcpy( szSerialId, SERIAL_PORT_B );

	// Construct the URL
	sprintf(szUrl, "%s%s%s%s", HTTP_PREAMBLE, 
							   HostAddr, 
							   SERIAL_GET_FLOW_CONTROL, 
							   szSerialId);

	hService = InternetOpenUrl( hSession, szUrl, NULL, 0, 0, 0 );

	if ( hService == 0 )
	{
		return APN_NET_ERR_GENERIC_CGI;		// Failure
	}
	else
	{
		InternetQueryDataAvailable(hService, &dwBytesAvailable, 0, 0);
		pBuffer = (char *)malloc(dwBytesAvailable+1);
		InternetReadFile(hService, pBuffer, dwBytesAvailable, &dwBytesRead);
		pBuffer[dwBytesRead] = '\0';

		strtok( pBuffer, ",");
		
		strcpy( szRetVal, strtok(NULL, ",") );

		if ( strncmp(szRetVal, SERIAL_FLOW_CONTROL_NONE, 1) == 0 )
			*FlowControl = false;
		else
			*FlowControl = true;

		free( pBuffer );

		InternetCloseHandle( hService );
	}

//	ApnNetClose( &hSession, HostAddr );

	return APN_NET_SUCCESS;	// Success
}


APN_NET_TYPE ApnNetSerialWriteFlowControl( char			  *HostAddr,
										   unsigned short SerialId,
								 		   bool			  FlowControl )
{
	DWORD		dwBytesAvailable, dwBytesRead;
	HINTERNET	hSession;
	HINTERNET	hService;
	char		szUrl[8096];
	char		szSerialId[5];
	char		szFlowControl[5];
	char		*szStopStr;
	char		*pBuffer;
	USHORT		RetVal;


	// Make sure the serial port selection is valid
	if ( (SerialId != 0) && (SerialId != 1) )
	{
		return APN_NET_ERR_SERIAL_ID;
	}

//	ApnNetConnect( &hSession, HostAddr );

	// Create a string based on the particular serial port
	if ( SerialId == 0 )
		strcpy( szSerialId, SERIAL_PORT_A );
	else
		strcpy( szSerialId, SERIAL_PORT_B );

	// Determine the flow control setting
	if ( FlowControl )
		strcpy( szFlowControl, SERIAL_FLOW_CONTROL_SW );
	else
		strcpy( szFlowControl, SERIAL_FLOW_CONTROL_NONE );

	// Construct the URL
	sprintf(szUrl, "%s%s%s%s,%s", HTTP_PREAMBLE, 
								  HostAddr, 
								  SERIAL_SET_FLOW_CONTROL, 
								  szSerialId,
								  szFlowControl );

	hService = InternetOpenUrl( hSession, szUrl, NULL, 0, 0, 0 );

	if ( hService == 0 )
	{
		return APN_NET_ERR_GENERIC_CGI;		// Failure
	}
	else
	{
		InternetQueryDataAvailable(hService, &dwBytesAvailable, 0, 0);
		pBuffer = (char *)malloc(dwBytesAvailable+1);
		InternetReadFile(hService, pBuffer, dwBytesAvailable, &dwBytesRead);
		pBuffer[dwBytesRead] = 0;

		free( pBuffer );

		InternetCloseHandle( hService );
	}

//	ApnNetClose( &hSession, HostAddr );

	return APN_NET_SUCCESS;	// Success
}


APN_NET_TYPE ApnNetSerialReadParity( char			*HostAddr,
									 unsigned short SerialId,
									 ApnNetParity   *Parity )
{
	DWORD		dwBytesAvailable, dwBytesRead;
	HINTERNET	hSession;
	HINTERNET	hService;
	char		szUrl[8096];
	char		szSerialId[5];
	char		szRetVal[5];
	char		*szStopStr;
	char		*pBuffer;


	// Make sure the serial port selection is valid
	if ( (SerialId != 0) && (SerialId != 1) )
	{
		return APN_NET_ERR_SERIAL_ID;
	}

//	ApnNetConnect( &hSession, HostAddr );

	// Create a string based on the particular serial port
	if ( SerialId == 0 )
		strcpy( szSerialId, SERIAL_PORT_A );
	else
		strcpy( szSerialId, SERIAL_PORT_B );

	sprintf(szUrl, "%s%s%s%s", HTTP_PREAMBLE, HostAddr, SERIAL_GET_PARITY, szSerialId);

	hService = InternetOpenUrl( hSession, szUrl, NULL, 0, 0, 0 );

	if ( hService == 0 )
	{
		return APN_NET_ERR_GENERIC_CGI;		// Failure
	}
	else
	{
		InternetQueryDataAvailable(hService, &dwBytesAvailable, 0, 0);
		pBuffer = (char *)malloc(dwBytesAvailable+1);
		InternetReadFile(hService, pBuffer, dwBytesAvailable, &dwBytesRead);
		pBuffer[dwBytesRead] = 0;

		strtok( pBuffer, ",");
		
		strcpy( szRetVal, strtok(NULL, ",") );

		if ( strncmp(szRetVal, SERIAL_PARITY_NONE, 1) == 0 )
		{
			*Parity = ApnNetParity_None;
		}
		else if ( strncmp(szRetVal, SERIAL_PARITY_EVEN, 1) == 0 )
		{
			*Parity = ApnNetParity_Even;
		}
		else if ( strncmp(szRetVal, SERIAL_PARITY_ODD, 1) == 0 )
		{
			*Parity = ApnNetParity_Odd;
		}

		free( pBuffer );

		InternetCloseHandle( hService );
	}

//	ApnNetClose( &hSession, HostAddr );

	return APN_NET_SUCCESS;	// Success
}


APN_NET_TYPE ApnNetSerialWriteParity( char			 *HostAddr,
									  unsigned short SerialId,
									  ApnNetParity   Parity )
{
	DWORD		dwBytesAvailable, dwBytesRead;
	HINTERNET	hSession;
	HINTERNET	hService;
	char		szUrl[8096];
	char		szSerialId[5];
	char		szParity[5];
	char		*szStopStr;
	char		*pBuffer;
	USHORT		RetVal;


	// Make sure the serial port selection is valid
	if ( (SerialId != 0) && (SerialId != 1) )
	{
		return APN_NET_ERR_SERIAL_ID;
	}

//	ApnNetConnect( &hSession, HostAddr );

	// Create a string based on the particular serial port
	if ( SerialId == 0 )
		strcpy( szSerialId, SERIAL_PORT_A );
	else
		strcpy( szSerialId, SERIAL_PORT_B );

	// Determine new parity setting
	switch ( Parity )
	{
	case ApnNetParity_None:
		strcpy( szParity, SERIAL_PARITY_NONE );
		break;
	case ApnNetParity_Even:
		strcpy( szParity, SERIAL_PARITY_EVEN );
		break;
	case ApnNetParity_Odd:
		strcpy( szParity, SERIAL_PARITY_ODD );
		break;
	default:
		return APN_NET_ERR_SERIAL_PARITY;
		break;
	}

	sprintf(szUrl, "%s%s%s%s,%s", HTTP_PREAMBLE, 
								  HostAddr, 
								  SERIAL_SET_PARITY, 
								  szSerialId,
								  szParity );

	hService = InternetOpenUrl( hSession, szUrl, NULL, 0, 0, 0 );

	if ( hService == 0 )
	{
		return APN_NET_ERR_GENERIC_CGI;		// Failure
	}
	else
	{
		InternetQueryDataAvailable(hService, &dwBytesAvailable, 0, 0);
		pBuffer = (char *)malloc(dwBytesAvailable+1);
		InternetReadFile(hService, pBuffer, dwBytesAvailable, &dwBytesRead);
		pBuffer[dwBytesRead] = 0;

		free( pBuffer );

		InternetCloseHandle( hService );
	}

//	ApnNetClose( &hSession, HostAddr );

	return APN_NET_SUCCESS;	// Success
}


APN_NET_TYPE ApnNetSerialRead( SOCKET		  *SerialSocket,
							   char			  *ReadBuffer,
							   unsigned short *BufferCount )
{
	int				RetVal;
	char			Buffer[256];
	unsigned short	BufferSize;


	BufferSize = 255;

	// Make sure the input params are valid
	if ( (ReadBuffer == NULL) || (BufferCount == NULL) )
		return APN_NET_ERR_SERIAL_READ_INPUT;

	// Do the operation
	RetVal = recv( *SerialSocket, Buffer, BufferSize, 0 );

	if ( RetVal == -1 )
	{
		*BufferCount = 3;
		strcpy( ReadBuffer, "EOF" );
		ReadBuffer[*BufferCount] = '\0';
		return APN_NET_SUCCESS;
	}

	*BufferCount = RetVal;

	strncpy( ReadBuffer, Buffer, *BufferCount );

	ReadBuffer[*BufferCount] = '\0';

	return APN_NET_SUCCESS;	// Success
}


APN_NET_TYPE ApnNetSerialWrite( SOCKET		   *SerialSocket,
							    char		   *WriteBuffer,
								unsigned short BufferCount )
{
	int RetVal;


	// Make sure the input params are valid
	if ( (BufferCount == 0) || (WriteBuffer == NULL) )
		return APN_NET_ERR_SERIAL_WRITE_INPUT;

	RetVal = send( *SerialSocket, WriteBuffer, BufferCount, 0 );

	if ( RetVal == 0 )
		return APN_NET_ERR_SERIAL_NO_CONNECTION;
	else if ( RetVal != BufferCount )
		return APN_NET_ERR_SERIAL_WRITE_FAILURE;

	return APN_NET_SUCCESS;	// Success
}
