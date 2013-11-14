#include "stdafx.h"
#include "ApogeeNet.h"
#include "ApogeeNetErr.h"
#define HINTERNET int

#ifdef __cplusplus
extern "C" {
#endif

extern int InternetOpen( char *iname, int itype, int *dum, int *dum2, int dum3);
extern int InternetOpenUrl( int g_hSession, char *url, int *dum1, int *dum2, int dum3, int dum4 );
extern void InternetQueryDataAvailable(int handle, long *bcount, int dum1, int dum2);
extern void InternetReadFile(int handle, char *lpBuffer, long bcount, long *bread);
extern void InternetCloseHandle( int handle );
#ifdef __cplusplus
}
#endif

int main(int argc, char **argv) 
{
	int i,h;
	DWORD		dwBytesAvailable, dwBytesRead;
	HINTERNET	hService;
	int		RetLength;
	char		*lpBuffer;
	
	i =  InternetOpen( "ApogeeNet", 
				   0, 
				  NULL, 
				  NULL, 
				  0 );


	h = InternetOpenUrl( i, "http://www.randomfactory.com/" , NULL, 0, 0, 0 );

	InternetQueryDataAvailable(h, &dwBytesAvailable, 0, 0);
	lpBuffer = (char *)malloc(dwBytesAvailable+1);
	InternetReadFile(h, lpBuffer, dwBytesAvailable, &dwBytesRead);

	lpBuffer[dwBytesRead] = 0;
	RetLength = strlen( SESSION_OPEN_RETVAL );

	free( lpBuffer );
	InternetCloseHandle( h );

}




