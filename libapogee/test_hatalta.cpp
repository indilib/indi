//============================================================================
// Apogee minimal CCD test program
// Zoltan Csubry
// zcsubry@cfa.harvard.edu
//============================================================================

#include <iostream>

#include <stdafx.h>
#include <ApnCamera_NET.h>
#include <ApogeeNet.h>

//--- Static function declarations -------------------------------------------

static bool			ping( std::string address );
static std::string	downloadFile( std::string address, std::string file );

//============================================================================

int main()
{
	//--- allocate memory for ccd object ---------------------------------
	CApnCamera	*ccd = new CApnCamera();

	//--- define ccd network information ---------------------------------
	std::string		cam_ip_str	= "192.168.0.198";
	unsigned long	cam_ip_long	= 3232235718;
	unsigned short	cam_port	= 80;
	std::string		cam_mac		= "00095100008A";

	//--- send ping to camera --------------------------------------------
	if ( !ping( cam_ip_str ) ) {
		std::cout<<"\n=== Address is not available ===\n";
		exit ( -1 );
	}

	//--- download session close file ------------------------------------
	downloadFile( cam_ip_str, "SESSION?Close" );

	//--- init CCD driver ------------------------------------------------  
	if ( ! ccd->InitDriver( cam_ip_long, cam_port, 0 ) ) {
		std::cout<<"\n=== Failed to initialize CCD driver ===\n";
		exit( -1 );
	}
	std::cout<<"\n=== Initialized Apogee CCD ===\n";

	//--- make some CCD action -------------------------------------------
	ccd->read_CoolerEnable();
	ccd->read_FanMode();
	ccd->read_CoolerSetPoint();
	ccd->read_CoolerBackoffPoint();
	ccd->read_ShutterState();
	ccd->read_CoolerStatus();
	ccd->read_ImagingStatus();
	ccd->read_TempCCD();
	ccd->read_TempHeatsink();

	//--- close CCD driver -----------------------------------------------	
	if ( ! ccd->CloseDriver() ) {
		std::cout<<"\n=== Failed to close CCD driver ===\n";
		exit( -1 );
	}
	std::cout<<"\n=== Closed Apogee CCD ===\n";

}

//============================================================================ 
// Network utility functions (using Apogee network functions)
//============================================================================

extern "C" {
    extern int  InternetOpen( char *iname, int itype, int *dum, int *dum2, int dum3);
    extern int  InternetOpenUrl( int g_hSession, char *url, int *dum1, int *dum2, int dum3, int dum4 );
    extern void InternetQueryDataAvailable(int handle, long *bcount, int dum1, int dum2);
    extern void InternetReadFile(int handle, char *lpBuffer, long bcount, long *bread);
    extern void InternetCloseHandle( int handle );
}

//============================================================================
// Send ping command to address
//============================================================================

bool ping( std::string address )
{
	std::string cmd = "ping -c 2 -w 5 " + address;
	if ( system( cmd.c_str() ) != 0 )
		return false;
	return true;
}
                    
//============================================================================
// Download a file from a web server and return string buffer
//============================================================================

std::string downloadFile( std::string address, std::string file )
{
	std::string szUrl;
	HINTERNET   hService, g_hSession;
	DWORD       dwBytesAvailable, dwBytesRead = 0;
	char        *lpBuffer;

	//--- Open internet session ------------------------------------------
	g_hSession = InternetOpen( "ApogeeNet", INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, 0 );
	if ( g_hSession == 0 )
		return "";

	//--- Open url, close session if failed ------------------------------
	szUrl = HTTP_PREAMBLE + address + "/" + file;
	hService = InternetOpenUrl( g_hSession, (char*) szUrl.c_str(), NULL, 0, 0, 0 );
	if ( hService == 0 ) {
		InternetCloseHandle( g_hSession );
		return "";
	}

	//--- Get file content into a buffer ---------------------------------
	InternetQueryDataAvailable(hService, &dwBytesAvailable, 0, 0);
	lpBuffer = new char[dwBytesAvailable+1];
	InternetReadFile(hService, lpBuffer, dwBytesAvailable, &dwBytesRead);
	lpBuffer[dwBytesRead] = 0;
	std::string fbuffer( lpBuffer );
	delete lpBuffer;

	//--- Close session --------------------------------------------------
	InternetCloseHandle( g_hSession );
	return fbuffer;
}

//============================================================================

