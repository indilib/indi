#include <windows.h>
#include <stdio.h>
#include <time.h>
#include <errno.h>

#include "../libfli-libfli.h"
#include "../libfli-debug.h"
#include "../libfli-mem.h"

#define MAX_DEBUG_STRING (1024)

LARGE_INTEGER dlltime;
static int _level = 0;
static int _forced = 0;
static int _debugstring = 0;
static char *_debugfile = NULL;
static HANDLE dfile = INVALID_HANDLE_VALUE;
static HANDLE debugmutex = NULL;
static void _debug(int level, const char *buffer);

static char _mutexname[] = { "1CE1A58C33904535873088172EFF34A0" };

int debugclose(void)
{
	if (dfile != INVALID_HANDLE_VALUE)
	{
		CloseHandle(dfile);
		dfile = INVALID_HANDLE_VALUE;
	}

	if(_debugfile != NULL)
	{
		debug(FLIDEBUG_ALL, "Closing debug file.");
		xfree(_debugfile);
		_debugfile = NULL;
	}

	if (debugmutex != NULL)
	{
		CloseHandle(debugmutex);
//		OutputDebugString("libfli: debug mutex destroyed");
		debugmutex = NULL;
	}

	return 0;
}

int debugopen(char *host)
{
	char date[12], time[12];

	_strdate(date);
	_strtime(time);
	QueryPerformanceCounter(&dlltime);

	debugclose();

	if(host != NULL)
	{
		_debugfile = xstrdup(host);
	}

	/* Open the mutex for debug information */
	if (debugmutex == NULL)
	{
		debugmutex = CreateMutex(NULL, FALSE, _mutexname);

		if (debugmutex == NULL)
		{
//			OutputDebugString("libfli: failed to create debug mutex");
		}
		else
		{
//			OutputDebugString("libfli: debug mutex created");
		}
	}

	debug(FLIDEBUG_ALL, "*** %s %s ***", date, time);
	debug(FLIDEBUG_ALL, "%s - Compiled %s %s", version, __DATE__, __TIME__);
	return 0;
}

void __cdecl FLIDebug(int level, char *format, ...)
{
	char buffer[MAX_DEBUG_STRING];
	int ret;

	if( ((_debugstring != 0) || (_debugfile != NULL )) && (level & _level) )
	{
		va_list ap;
		va_start(ap, format);
		ret = _vsnprintf(buffer, MAX_DEBUG_STRING - 1, format, ap);
		va_end(ap);

		if (ret >= 0)
			_debug(level, buffer);
	}
}

void debug(int level, char *format, ...)
{
	char buffer[MAX_DEBUG_STRING];
	int ret;
	
	if( ((_debugstring != 0) || (_debugfile != NULL )) && (level & _level) )
	{
		va_list ap;
		va_start(ap, format);
		ret = _vsnprintf(buffer, MAX_DEBUG_STRING - 1, format, ap);
		va_end(ap);

		if (ret >= 0)
			_debug(level, buffer);
	}
}

void _debug(int level, const char *buffer)
{
	char stime[16];
	char output[MAX_DEBUG_STRING];
	int ret;
	LARGE_INTEGER time, freq;
	double dtime;
	unsigned long pid, tid;
	
//#ifndef _DEBUGSTRING
//	if( ((_debugstring != 0) || (_debugfile != NULL )) && (level & _level) )
//#endif
	{
//		va_list ap;
//		va_start(ap, format);
//		ret = _vsnprintf(buffer, MAX_DEBUG_STRING - 1, format, ap);
//		va_end(ap);

		QueryPerformanceCounter(&time);
		QueryPerformanceFrequency(&freq);

		dtime = ((double) time.QuadPart - (double) dlltime.QuadPart ) / (double) freq.QuadPart;

		_snprintf(stime, 15, "%8.3f", dtime);

		pid = GetCurrentProcessId();
		tid = GetCurrentThreadId();

		switch (level)
		{
			case FLIDEBUG_INFO:
				ret = _snprintf(output, MAX_DEBUG_STRING - 1, "INFO<%s:%04X:%04X>: %s\n", stime, pid, tid, buffer);
				break;

			case FLIDEBUG_WARN:
				ret = _snprintf(output, MAX_DEBUG_STRING - 1, "WARN<%s:%04X:%04X>: %s\n", stime, pid, tid, buffer);
				break;

			case FLIDEBUG_FAIL:
				ret = _snprintf(output, MAX_DEBUG_STRING - 1, "FAIL<%s:%04X:%04X>: %s\n", stime, pid, tid, buffer);
				break;

			default:
				ret = _snprintf(output, MAX_DEBUG_STRING - 1, " ALL<%s:%04X:%04X>: %s\n", stime, pid, tid, buffer);
				break;
		}

		if(ret >= 0)
		{
			long locked = 0;

			if (debugmutex != NULL)
			{
				switch(WaitForSingleObject(debugmutex, 1000))
				{
					case WAIT_OBJECT_0:
						locked = 1;
						break;

					default:
						OutputDebugString("libfli: failed to obtain debug mutex!\n");
					break;
				}
			}

			if (_debugstring)
			{
				OutputDebugString(output);
			}

			if (_debugfile != NULL)
			{
				if (dfile == INVALID_HANDLE_VALUE)
				{
					dfile = CreateFile(_debugfile, GENERIC_WRITE, FILE_SHARE_READ, NULL,
						OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
					SetFilePointer(dfile, 0, NULL, FILE_END);
				}

				if(dfile != INVALID_HANDLE_VALUE)
				{
					DWORD bytes;

					WriteFile(dfile, output, (DWORD) strlen(output), &bytes, NULL);
				
					//CloseHandle(dfile);
					//dfile = INVALID_HANDLE_VALUE;
				}
			}

			if (locked != 0)
			{
				ReleaseMutex(debugmutex);
			}
		}
	}

	return;
}

void setdebuglevel(char *host, long level)
{
	if (_forced == 1)
		return;

	if (stricmp(host, "C:\\FLIDBG.TXT") == 0)
		_forced = 1;

	debug(FLIDEBUG_INFO, "Changing debug level to %d.", level);

	_level = level;
	_debugstring = (level & 0x80000000)?1:0;

	if (level == 0)
	{
		debug(FLIDEBUG_INFO, "Disabling debugging.");
		debugclose();
	}
	else
	{
		debugopen(host);
	}

	return;
}
