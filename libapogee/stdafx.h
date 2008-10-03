#if !defined(STDAFX__INCLUDED_)
#define STDAFX__INCLUDED_

#ifdef LINUX
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#define ULONG unsigned long
#define USHORT unsigned short
#define PUSHORT unsigned short *
#define BYTE unsigned char
#define DWORD long
#define BOOLEAN unsigned long
#define TRUE 1
#define FALSE 0
#define INTERNET_OPEN_TYPE_DIRECT 1
#define INTERNET_FLAG_NO_CACHE_WRITE 1
#define INTERNET_FLAG_KEEP_CONNECTION 1
#define Sleep(x) usleep(1000*x)
#endif


#endif





