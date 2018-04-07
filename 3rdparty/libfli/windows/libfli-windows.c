/*

  Copyright (c) 2002 Finger Lakes Instrumentation (FLI), L.L.C.
  All rights reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions
  are met:

        Redistributions of source code must retain the above copyright
        notice, this list of conditions and the following disclaimer.

        Redistributions in binary form must reproduce the above
        copyright notice, this list of conditions and the following
        disclaimer in the documentation and/or other materials
        provided with the distribution.

        Neither the name of Finger Lakes Instrumentation (FLI), LLC
        nor the names of its contributors may be used to endorse or
        promote products derived from this software without specific
        prior written permission.

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
  FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
  REGENTS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
  BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
  LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
  ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
  POSSIBILITY OF SUCH DAMAGE.

  ======================================================================

  Finger Lakes Instrumentation, L.L.C. (FLI)
  web: http://www.fli-cam.com
  email: support@fli-cam.com

*/

#include <windows.h>
#include <winioctl.h>
#include <stdio.h>
#include <errno.h>
#include <shlobj.h>

#include "../libfli-libfli.h"
#include "../libfli-debug.h"
#include "../libfli-mem.h"
#include "../libfli-camera.h"
#include "../libfli-raw.h"
#include "../libfli-filter-focuser.h"
#include "libfli-sys.h"
#include "libfli-parport.h"
#include "libfli-usb.h"
#include "libfli-serial.h"

#define MAX_SEARCH 16
#define MAX_SEARCH_DIGITS 3

static WSADATA WSAData;
static short WSEnabled;
static SOCKET sock = INVALID_SOCKET;
static OSVERSIONINFO OSVersionInfo;
static long OS = 0;

extern LARGE_INTEGER dlltime;
static long fli_resolve_serial_number(char **filename, char *serial, flidomain_t domain);

#ifndef SERVICE_MATCH
#define SERVICE_MATCH { \
		if (stricmp(pBuffer, "fliusb") == 0) match ++; \
		if (stricmp(pBuffer, "dnrusb") == 0) match ++; \
		if (stricmp(pBuffer, "reltusb") == 0) match ++; \
		if (stricmp(pBuffer, "pslcamusb") == 0) match ++; \
	}

#define FN_MATCH ( \
	(_strnicmp(name, "fci", 3) == 0) || \
	(_strnicmp(name, "psl", 3) == 0) || \
	(_strnicmp(name, "rel", 3) == 0) || \
	(_strnicmp(name, "fli", 3) == 0) || \
	(_strnicmp(name, "dnr", 3) == 0) || \
	(_strnicmp(name, "ccd", 3) == 0) \
	)

#define LIST_USB_CAM_PREFIX_LIST "flipro,flicam,pslcam,fcicam,reltcam-"
#define LIST_USB_FOCUSER_PREFIX_LIST "flifoc"
#define LIST_USB_FILTER_PREFIX_LIST "flifil"

#endif

#ifdef _USRDLL
BOOL APIENTRY DllMain( HANDLE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved )
{
	char strPath[ MAX_PATH ];
	HKEY hKey;
	DWORD len, forcedebug = 0;

	switch(ul_reason_for_call)
	{
		case DLL_PROCESS_ATTACH:
			QueryPerformanceCounter(&dlltime);
			WSEnabled = 0;
			if (WSAStartup(MAKEWORD(1, 1), &WSAData) == 0)
			{
				WSEnabled = 1;
			}

#define CREATEDEBUG
#ifdef CREATEDEBUG
//			OutputDebugString("libfli: loading\n");
			if (RegOpenKey(HKEY_CURRENT_USER,
				"SOFTWARE\\Finger Lakes Instrumentation\\libfli",
				&hKey) == ERROR_SUCCESS)
			{
				len = sizeof(DWORD);
				if (RegQueryValueEx(hKey, "debug", NULL, NULL, (LPBYTE) &forcedebug, &len) == ERROR_SUCCESS)
				{
//					OutputDebugString("libfli: debug registry key found!\n");
				}
				else
				{
//					OutputDebugString("libfli: debug registry key not found!\n");
				}
				RegCloseKey(hKey);
			}

			//{
			//	char _s[1024];
			//	sprintf(_s, "libfli: [%08x]\n", forcedebug);
			//	OutputDebugString(_s);
			//}

			if (forcedebug)
			{
				SHGetSpecialFolderPath(0, strPath,
						CSIDL_DESKTOPDIRECTORY, FALSE);

				strcat(strPath, "\\flidbg.txt");
				FLISetDebugLevel(strPath, forcedebug);
			}
#endif

			OSVersionInfo.dwOSVersionInfoSize=sizeof(OSVERSIONINFO);
			if(GetVersionEx(&OSVersionInfo)==0)
				return FALSE;

			switch (OSVersionInfo.dwPlatformId)
			{
				case VER_PLATFORM_WIN32_WINDOWS:
					OS = VER_PLATFORM_WIN32_WINDOWS;
					break;

				case VER_PLATFORM_WIN32_NT:
					OS = VER_PLATFORM_WIN32_NT;
					break;

			 default:
				return FALSE;
			}
			return TRUE;
			break;

		case DLL_THREAD_ATTACH:
			return TRUE;
			break;

		case DLL_THREAD_DETACH:
			return TRUE;
			break;

		case DLL_PROCESS_DETACH:
			xfree_all();
			if(WSEnabled == 1)
				WSACleanup();
			return TRUE;
			break;
	}

  return TRUE;
}
#endif

#ifdef _LIB
LIBFLIAPI FLILibAttach(void)
{
	char strPath[MAX_PATH];
	HKEY hKey;
	DWORD len, forcedebug = 0;

	QueryPerformanceCounter(&dlltime);
	WSEnabled = 0;
	if (WSAStartup(MAKEWORD(1, 1), &WSAData) == 0)
	{
		WSEnabled = 1;
	}

//#define CREATEDEBUG
#ifdef CREATEDEBUG
			SHGetSpecialFolderPath(0, strPath,
					CSIDL_DESKTOPDIRECTORY, FALSE );

			strcat(strPath, "\\flidbg.txt");
			FLISetDebugLevel(strPath, FLIDEBUG_ALL);
#endif

			if (RegOpenKey(HKEY_CURRENT_USER,
				"SOFTWARE\\Finger Lakes Instrumentation\\libfli",
				&hKey) == ERROR_SUCCESS)
			{
				len = sizeof(DWORD);
				if (RegQueryValueEx(hKey, "debug", NULL, NULL, (LPBYTE) &forcedebug, &len) == ERROR_SUCCESS)
				{
//					OutputDebugString("libfli: debug registry key found!\n");
				}
				else
				{
//					OutputDebugString("libfli: debug registry key not found!\n");
				}
				RegCloseKey(hKey);
			}

			//{
			//	char _s[1024];
			//	sprintf(_s, "libfli: [%08x]\n", forcedebug);
			//	OutputDebugString(_s);
			//}

			if (forcedebug)
			{
				SHGetSpecialFolderPath(0, strPath,
						CSIDL_DESKTOPDIRECTORY, FALSE);

				strcat(strPath, "\\flidbg.txt");
				FLISetDebugLevel(strPath, forcedebug);
			}

	OSVersionInfo.dwOSVersionInfoSize=sizeof(OSVERSIONINFO);
	if(GetVersionEx(&OSVersionInfo)==0)
		return 0;

	switch (OSVersionInfo.dwPlatformId)
	{
		case VER_PLATFORM_WIN32_WINDOWS:
			OS = VER_PLATFORM_WIN32_WINDOWS;
			break;

		case VER_PLATFORM_WIN32_NT:
			OS = VER_PLATFORM_WIN32_NT;
			break;

	 default:
			;
	}

	return 0;
}


LIBFLIAPI FLILibDetach(void)
{
	xfree_all();
	if(WSEnabled == 1)
		WSACleanup();

	return 0;
}

#endif

long fli_connect(flidev_t dev, char *name, long domain)
{
  fli_io_t *io;
	char *tname, *mname;
	HANDLE mutex;
	fli_sysinfo_t *sys;
	Usb_Device_Descriptor usbdesc;
	DWORD read;
	int i;
	COMMCONFIG IO_Config;
	DWORD ConfigSize=sizeof(COMMCONFIG);

  CHKDEVICE(dev);

  if(name == NULL)
    return -EINVAL;

  /* Lock functions should be set before any other functions used */
  DEVICE->fli_lock = fli_lock;
  DEVICE->fli_unlock = fli_unlock;

  DEVICE->domain = domain & 0x00ff;
  DEVICE->devinfo.type = domain & 0x7f00;

  debug(FLIDEBUG_INFO, "Domain: 0x%04x", DEVICE->domain);
  debug(FLIDEBUG_INFO, "  Type: 0x%04x", DEVICE->devinfo.type);

	if ((io = xcalloc(1, sizeof(fli_io_t))) == NULL)
	{
		fli_disconnect(dev);
		return -ENOMEM;
	}
	io->fd = INVALID_HANDLE_VALUE;
	DEVICE->io_data = io;

	if ((sys = xcalloc(1, sizeof(fli_sysinfo_t))) == NULL)
	{
		fli_disconnect(dev);
		return -ENOMEM;
	}
	DEVICE->sys_data = sys;
	sys->OS = OS;

	/* Hack for COM port devices */
	if ( ((DEVICE->domain & 0x00ff) == FLIDOMAIN_SERIAL) &&
		(_strnicmp(name, "COM", 3) == 0) )
	{
		if (xasprintf(&tname, "\\\\.\\%s", name) == (-1))
		{
			tname = NULL;
			fli_disconnect(dev);
			return -ENOMEM;
		}
	}
	/* Determine if we are receiving a proper filename */
	else if (strncmp(name, "\\\\", 2) == 0)
	{
		if (xasprintf(&tname, "%s", name) == (-1))
		{
			tname = NULL;
			fli_disconnect(dev);
			return -ENOMEM;
		}
	}
	else if (FN_MATCH)
	{
		if (xasprintf(&tname, "\\\\.\\%s", name) == (-1))
		{
			tname = NULL;
			fli_disconnect(dev);
			return -ENOMEM;
		}
	}
	else /* Maybe we are opening by serial number, so, lets look for it... */
	{
		fli_resolve_serial_number(&tname, name, domain);

		if (tname == NULL)
		{
			fli_disconnect(dev);
			return -ENODEV;
		}
	}

	DEVICE->name = tname;

  switch (DEVICE->domain)
  {
	  case FLIDOMAIN_PARALLEL_PORT:
    {
			if (OS == VER_PLATFORM_WIN32_NT)
			{
				io->fd = CreateFile(tname, GENERIC_READ | GENERIC_WRITE,
					0, NULL, OPEN_EXISTING, 0, NULL);
				if (io->fd == INVALID_HANDLE_VALUE)
				{
					fli_disconnect(dev);
					return -ENODEV;
				}
			}
			else
			{
				io->port = strtol(name, NULL, 0);
				if(stricmp(name, "ccdpar0") == 0)
				{
					io->port = 0x378;
				}

				if(stricmp(name, "ccdpar1") == 0)
				{
					io->port = 0x278;
				}
			}

			if(ECPInit(dev) != 0)
			{
				fli_disconnect(dev);
				return -ENODEV;
			}
			DEVICE->fli_io = parportio;
		}
    break;

	  case FLIDOMAIN_SERIAL:
	  case FLIDOMAIN_SERIAL_1200:
	  case FLIDOMAIN_SERIAL_19200:
    {
			debug(FLIDEBUG_INFO, "Serial, opening port.");

			io->fd = CreateFile(tname, GENERIC_READ | GENERIC_WRITE,
				0, NULL, OPEN_EXISTING, 0, NULL);
			if (io->fd == INVALID_HANDLE_VALUE)
			{
				fli_disconnect(dev);
				return -ENODEV;
			}
			DEVICE->fli_io = serportio;

			debug(FLIDEBUG_INFO, "Attempting at 19200 baud...");
		  GetCommConfig(io->fd, &IO_Config, &ConfigSize);
		  IO_Config.dcb.BaudRate = 19200;
		  IO_Config.dcb.Parity = NOPARITY;
		  IO_Config.dcb.ByteSize = 8;
		  IO_Config.dcb.StopBits = ONESTOPBIT;
			IO_Config.dcb.fRtsControl = RTS_CONTROL_DISABLE;
			IO_Config.dcb.fOutxCtsFlow = FALSE;
			if(SetCommConfig(io->fd, &IO_Config, ConfigSize)==FALSE)
			{
				fli_disconnect(dev);
				return -ENODEV;
			}

			/* We need to probe the serial port... */
			if (fli_filter_focuser_probe(dev) == 0)
			{
				debug(FLIDEBUG_INFO, "Found device at 19200 baud...");
				break;
			}

			Sleep(50);	/* Wait for probe to timeout */

			debug(FLIDEBUG_INFO, "Attempting at 1200 baud...");
		  GetCommConfig(io->fd, &IO_Config, &ConfigSize);
		  IO_Config.dcb.BaudRate = 1200;
		  IO_Config.dcb.Parity = NOPARITY;
		  IO_Config.dcb.ByteSize = 8;
		  IO_Config.dcb.StopBits = ONESTOPBIT;
			IO_Config.dcb.fRtsControl = RTS_CONTROL_DISABLE;
			IO_Config.dcb.fOutxCtsFlow = FALSE;
			if(SetCommConfig(io->fd, &IO_Config, ConfigSize)==FALSE)
			{
				fli_disconnect(dev);
				return -ENODEV;
			}

			/* We need to probe the serial port... */
			if (fli_filter_focuser_probe(dev) == 0)
			{
				debug(FLIDEBUG_INFO, "Found device at 1200 baud...");
				break;
			}

			debug(FLIDEBUG_INFO, "Did not find a serial device.");
			fli_disconnect(dev);
			return -ENODEV;
		}
    break;

	  case FLIDOMAIN_USB:
    {
#ifdef OLDUSBDRIVER
			unsigned char buf[1024];
			DWORD bytes;

			PFLI_INTERFACE_INFORMATION pInterface;
			PFLI_PIPE_INFORMATION pPipe;
			int i;
#endif

			io->fd = CreateFile(tname, GENERIC_WRITE, FILE_SHARE_WRITE,
													NULL, OPEN_EXISTING, 0, NULL);
			if (io->fd == INVALID_HANDLE_VALUE)
			{
				fli_disconnect(dev);
				return -ENODEV;
			}

			debug(FLIDEBUG_INFO, "Getting Device configuration.");
			if (DeviceIoControl(io->fd, IOCTL_GET_DEVICE_DESCRIPTOR,
													  NULL, 0, &usbdesc, sizeof(usbdesc),
													  &read, NULL) == FALSE)
			{
				debug(FLIDEBUG_WARN, "Couldn't read device description, error: %d", GetLastError());
				fli_disconnect(dev);
				return -ENODEV;
			}

			DEVICE->devinfo.devid = usbdesc.idProduct;
			DEVICE->devinfo.fwrev = usbdesc.bcdDevice;

			/* Hack for incorrect FW rev on early proline cameras */
			if ( (usbdesc.idProduct == 0x0a) &&
				(usbdesc.iSerialNumber == 3) &&
				(usbdesc.bcdDevice == 0x0100) )
			{
				DEVICE->devinfo.fwrev = 0x0101;
			}

			/* Get serial string */
			if (usbdesc.iSerialNumber == 3)
			{
				GET_STRING_DESCRIPTOR_IN sd;
				char name[MAX_PATH];

				sd.Index = 3;
				sd.LanguageId = 0x00;
				ZeroMemory(name, sizeof(name));

				if (DeviceIoControl(io->fd, IOCTL_GET_STRING_DESCRIPTOR,
														&sd, sizeof(sd), name, sizeof(name),
														&read, NULL) != FALSE)
				{
					DWORD i;
					/* de-unicode it */
					for (i = 0; i < read; i++)
					{
						name[i] = name[(i + 1) * 2];
					}
					name[i] = '\0';
					debug(FLIDEBUG_INFO, "Serial: %s [%d]", name, i);
					DEVICE->devinfo.serial = xstrdup(name);
				}
			}

#ifdef OLDUSBDRIVER
			/* Get pipe information */
			if (DeviceIoControl(io->fd, IOCTL_Ezusb_GET_PIPE_INFO,
													NULL, 0, buf, 1024, &bytes,	NULL) == FALSE)
			{
				debug(FLIDEBUG_FAIL, "Error getting USB pipe information, error: %d", GetLastError());
				fli_disconnect(dev);
				return -ENODEV;
			}

			pInterface = (PFLI_INTERFACE_INFORMATION) buf;
			pPipe = pInterface->Pipes;

			for(i = 0; (i < (int) pInterface->NumberOfPipes) && (i < USB_MAX_PIPES); i++)
			{
				debug(FLIDEBUG_INFO, "Pipe: %d Type: %02x Endpoint: %02x MaxSize: %02x MaxXfer: %04x",
					i,
					pPipe[i].PipeType,
					pPipe[i].EndpointAddress,
					pPipe[i].MaximumPacketSize,
					pPipe[i].MaximumTransferSize
					);
				io->endpointlist[i] = pPipe[i].EndpointAddress;
			}
#endif

			debug(FLIDEBUG_INFO, "    id: 0x%04x", DEVICE->devinfo.devid);
			debug(FLIDEBUG_INFO, " fwrev: 0x%04x", DEVICE->devinfo.fwrev);
			DEVICE->fli_io = usbio;
    }
		break;

	  default:
			fli_disconnect(dev);
			return -EINVAL;
  }

  switch (DEVICE->devinfo.type)
  {
	  case FLIDEVICE_CAMERA:
	   DEVICE->fli_open = fli_camera_open;
	   DEVICE->fli_close = fli_camera_close;
	   DEVICE->fli_command = fli_camera_command;
		 break;

	  case FLIDEVICE_FOCUSER:
	   DEVICE->fli_open = fli_focuser_open;
	   DEVICE->fli_close = fli_focuser_close;
	   DEVICE->fli_command = fli_focuser_command;
	   break;

		case FLIDEVICE_FILTERWHEEL:
	   DEVICE->fli_open = fli_filter_open;
	   DEVICE->fli_close = fli_filter_close;
	   DEVICE->fli_command = fli_filter_command;
		 break;

		case FLIDEVICE_RAW:
		 DEVICE->fli_open = fli_raw_open;
		 DEVICE->fli_close = fli_raw_close;
		 DEVICE->fli_command = fli_raw_command;
		 break;

	  default:
			fli_disconnect(dev);
			return -EINVAL;
  }

	/* Now create the synchronization object */
	mname = (char *) xcalloc(MAX_PATH, sizeof(char));
	if(mname != NULL) /* We can allocate the mutex */
	{
		int j = 0;
		strcpy(mname, "CEC615E9-917D-4cee-BC2F-2DE1B6D3E03B_");
		strcat(mname, name);
		for(i = 0; mname[i] != '\0'; i++) /* Convert case and strip nasties */
		{
			if (isalnum(mname[i]))
			{
				mname[j] = toupper(mname[i]);
				j++;
			}
		}
		mname[j] = '\0';

		debug(FLIDEBUG_INFO, "Creating named mutex \"%s\"", mname);

		mutex = OpenMutex(MUTEX_ALL_ACCESS | SYNCHRONIZE, TRUE, mname);
		if(mutex == NULL)
		{
			mutex = CreateMutex(NULL, FALSE, mname);
		}

		if(mutex == NULL)
		{
			debug(FLIDEBUG_WARN, "Failed to create mutex object, error: %d", GetLastError());
		}

		((fli_sysinfo_t *) (DEVICE->sys_data))->mutex = mutex;
		xfree(mname);
	}
	else
	{
		debug(FLIDEBUG_WARN, "Failed to allocate name for mutex.");
	}

  DEVICE->io_timeout = 20 * 1000; /* 20 seconds. */

  return 0;
}

long fli_disconnect(flidev_t dev)
{
  int err = 0;
	fli_io_t *io;

  CHKDEVICE(dev);

	io = DEVICE->io_data;

  switch (DEVICE->domain)
  {
		case FLIDOMAIN_PARALLEL_PORT:
			ECPClose(dev);
			break;

		case FLIDOMAIN_USB:
			break;

		default:
			err = -EINVAL;
  }

	if(io != NULL)
	{
		if(io->fd != INVALID_HANDLE_VALUE)
		{
			if (CloseHandle(io->fd) == FALSE)
				err = -EIO;
			else
				err = 0;
		}
	}

	if(((fli_sysinfo_t *) (DEVICE->sys_data))->mutex != NULL)
	{
		CloseHandle(((fli_sysinfo_t *) (DEVICE->sys_data))->mutex);
	}

	if (DEVICE->devinfo.serial != NULL)
	{
		xfree(DEVICE->devinfo.serial);
		DEVICE->devinfo.serial = NULL;
	}

  if (DEVICE->io_data != NULL)
  {
    xfree(DEVICE->io_data);
    DEVICE->io_data = NULL;
  }

  if (DEVICE->sys_data != NULL)
  {
    xfree(DEVICE->sys_data);
    DEVICE->sys_data = NULL;
  }

	if(DEVICE->name != NULL)
	{
		xfree(DEVICE->name);
		DEVICE->name = NULL;
	}

  DEVICE->fli_lock = NULL;
  DEVICE->fli_unlock = NULL;
  DEVICE->fli_io = NULL;
  DEVICE->fli_open = NULL;
  DEVICE->fli_close = NULL;
  DEVICE->fli_command = NULL;

  return err;
}

long fli_lock(flidev_t dev)
{
	long r = -ENODEV;
	HANDLE mutex;
		
	CHKDEVICE(dev);

	mutex = ((fli_sysinfo_t *) (DEVICE->sys_data))->mutex;

//	debug(FLIDEBUG_INFO, "Acquiring lock...");

	if (mutex != NULL)
	{
		switch(WaitForSingleObject(mutex, INFINITE))
		{
			case WAIT_OBJECT_0:
//				debug(FLIDEBUG_INFO, "Lock acquired...");
				r = 0;
				break;

			default:
				debug(FLIDEBUG_WARN, "Could not acquire mutex: %d", GetLastError());
				r = -ENODEV;
				break;
		}
	}
	else
	{
		debug(FLIDEBUG_WARN, "lock(): Mutex is NULL!");
	}

	return r;
}

long fli_unlock(flidev_t dev)
{
	HANDLE mutex;
	
	CHKDEVICE(dev);

	mutex = ((fli_sysinfo_t *) (DEVICE->sys_data))->mutex;

//	debug(FLIDEBUG_INFO, "Releasing lock...");

	if (mutex != NULL)
	{
		if(ReleaseMutex(mutex) == FALSE)
		{
			debug(FLIDEBUG_WARN, "Could not release mutex: %d", GetLastError());
			return -ENODEV;
		}
		return 0;
	}
	else
	{
		debug(FLIDEBUG_WARN, "unlock(): Mutex is NULL!");
	}
	return -ENODEV;
}

static long fli_list_usb(flidomain_t domain, char ***names);
static long fli_list_parport(flidomain_t domain, char ***names);
static long fli_list_serial(flidomain_t domain, char ***names);

long fli_list(flidomain_t domain, char ***names)
{
	*names = NULL;

  switch (domain & 0x00ff)
  {
	  case FLIDOMAIN_PARALLEL_PORT:
	    return fli_list_parport(domain, names);
	    break;

	  case FLIDOMAIN_SERIAL:
	  case FLIDOMAIN_SERIAL_1200:
		case FLIDOMAIN_SERIAL_19200:
	    return fli_list_serial(domain, names);
	    break;
		
	  case FLIDOMAIN_USB:
	    return fli_list_usb(domain, names);
	    break;

	  default:
	    return -EINVAL;
  }

  /* Not reached */
  return -EINVAL;
}

#define NAME_LEN_MAX 4096

/* This function enumerates FLI devices by port, this removes boot time
 * configuration problems */

#include <devguid.h>
#include <setupapi.h>

static const GUID GUID_DEVINTERFACE_USB_DEVICE = 
{ 0xA5DCBF10L, 0x6530, 0x11D2, { 0x90, 0x1F, 0x00, 0xC0, 0x4F, 0xB9, 0x51, 0xED } };

static long fli_resolve_serial_number(char **filename, char *serial, flidomain_t domain)
{
	HDEVINFO hDevInfo;
	SP_DEVICE_INTERFACE_DATA devInterfaceData;
	BOOL bRet = FALSE;
	DWORD dwIndex = 0;

	*filename = NULL;

	hDevInfo = SetupDiGetClassDevs(&GUID_DEVINTERFACE_USB_DEVICE,
		NULL, NULL, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);

	if (hDevInfo == INVALID_HANDLE_VALUE)
	{
		debug(FLIDEBUG_FAIL, "Could not obtain handle from SetupDiGetClassDevs(), error %d", GetLastError());
		return 0;
	}

	ZeroMemory(&devInterfaceData, sizeof(SP_DEVICE_INTERFACE_DATA));
	devInterfaceData.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);

	while(*filename == NULL)
	{
		bRet = SetupDiEnumDeviceInterfaces(hDevInfo,
			NULL, /* PSP_DEVINFO_DATA DeviceInfoData */ 
			&GUID_DEVINTERFACE_USB_DEVICE,
			dwIndex, &devInterfaceData);

		if (bRet == TRUE)
		{
			PSP_DEVICE_INTERFACE_DETAIL_DATA detailData;
			DWORD DataSize, RequiredSize;
			SP_DEVINFO_DATA DeviceInfoData;
			DWORD dwRegType, dwRegSize;
			int match = 0;
			char *pBuffer;

			ZeroMemory(&DeviceInfoData, sizeof(SP_DEVINFO_DATA));
			DeviceInfoData.cbSize = sizeof(SP_DEVINFO_DATA);

			/* Get required size of detail data and the DEVINFO_DATA structure */
			if (SetupDiGetDeviceInterfaceDetail(hDevInfo,
				&devInterfaceData, NULL, 0, &DataSize, &DeviceInfoData) != TRUE)
			{
				if (GetLastError() != ERROR_INSUFFICIENT_BUFFER)
				{
					debug(FLIDEBUG_FAIL, "Could not obtain size for interface detail data, error %d",
					GetLastError());
					break;
				}
			}

			detailData = (PSP_DEVICE_INTERFACE_DETAIL_DATA) xmalloc(DataSize);
			ZeroMemory(detailData, DataSize);
			detailData->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);

			/* Get the detail data */
			if (SetupDiGetDeviceInterfaceDetail(hDevInfo, &devInterfaceData,
				detailData, DataSize, &RequiredSize, NULL) != TRUE)
			{
				debug(FLIDEBUG_FAIL, "Could not obtain interface detail data, error %d",
					GetLastError());

				xfree(detailData);
				break;
			}

			dwRegSize = 0;
			pBuffer = NULL;

			/* Get the service name (this should be "fliusb") */
			if (SetupDiGetDeviceRegistryProperty(hDevInfo,
				&DeviceInfoData, SPDRP_SERVICE, &dwRegType,
        NULL, 0, &dwRegSize) != TRUE)
			{
				if (GetLastError() != ERROR_INSUFFICIENT_BUFFER)
				{
					debug(FLIDEBUG_FAIL, "Could not obtain size for service name, error %d",
						GetLastError());

					xfree(detailData);
					break;
				}
			}

			/* Allocate buffer for the friendly name. */
			pBuffer = (TCHAR *) xmalloc (dwRegSize * sizeof(TCHAR));

			/* Retreive the service name */
			if (SetupDiGetDeviceRegistryProperty(hDevInfo,
				&DeviceInfoData, SPDRP_SERVICE, &dwRegType, (PBYTE) pBuffer,
				dwRegSize, &dwRegSize) != TRUE)
			{
				debug(FLIDEBUG_FAIL, "Could not get service name, error %d",
					GetLastError());

				xfree(detailData);
				xfree(pBuffer);
				break;
			}
			debug(FLIDEBUG_INFO, "Found [%s] [%s]", detailData->DevicePath, pBuffer);

			/* Is it our driver? */
			SERVICE_MATCH

			if (match != 0)
			{
				Usb_Device_Descriptor usbdesc;
				HANDLE fd;
				int pid;
				DWORD read;

				do
				{
					fd = CreateFile(detailData->DevicePath, GENERIC_WRITE,
						FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);

					if (fd == INVALID_HANDLE_VALUE)
					{
						break;
					}

					if (DeviceIoControl(fd, IOCTL_GET_DEVICE_DESCRIPTOR,
						NULL, 0, &usbdesc, sizeof(usbdesc), &read, NULL) == FALSE)
					{
						debug(FLIDEBUG_WARN, "Couldn't read device description, error: %d", GetLastError());
						CloseHandle(fd);
						break;
					}

					pid = usbdesc.idProduct;

					if (
						((pid == FLIUSB_CAM_ID) && ((domain & 0x7f00) == FLIDEVICE_CAMERA)) ||
						((pid == FLIUSB_PROLINE_ID) && ((domain & 0x7f00) == FLIDEVICE_CAMERA)) ||
						((pid >= 0x0100) && (pid < 0x0110) && ((domain & 0x7f00) == FLIDEVICE_CAMERA)) ||
						((pid == FLIUSB_FOCUSER_ID) && ((domain & 0x7f00) == FLIDEVICE_FOCUSER)) ||
						((pid == FLIUSB_FILTER_ID) && ((domain & 0x7f00) == FLIDEVICE_FILTERWHEEL))
						)
					{
						GET_STRING_DESCRIPTOR_IN sd;
						char name[MAX_PATH];

						sd.Index = 3;
						sd.LanguageId = 0x00;
						ZeroMemory(name, sizeof(name));

						if ( (usbdesc.iSerialNumber == 3) &&
								 (DeviceIoControl(fd, IOCTL_GET_STRING_DESCRIPTOR,
																	&sd, sizeof(sd), name, sizeof(name),
																	&read, NULL) == FALSE) )
						{
							/* No serial number, so it can't match */
						}
						else
						{
							DWORD i;
							/* de-unicode it */
							for (i = 0; i < read; i++)
							{
								name[i] = name[(i + 1) * 2];
							}
							name[i] = '\0';

							if (stricmp(name, serial) == 0)
							{
								debug(FLIDEBUG_INFO, "Found %s as [%s]", serial, detailData->DevicePath);
								xasprintf(filename, "%s", detailData->DevicePath);
							}
						}
					}
					else
					{
//						debug(FLIDEBUG_INFO, "Not the device we are looking for.");
					}
					CloseHandle(fd);
				}
				while (0 == 1);
			}

			xfree(detailData);
			xfree(pBuffer);
		}
		else
		{
			int err = GetLastError();

			if ( (err == ERROR_NO_MORE_ITEMS) || (err == ERROR_FILE_NOT_FOUND) )
			{
				break;
			}
		}

		dwIndex++; 
	}

	SetupDiDestroyDeviceInfoList(hDevInfo);
	return 0;
}

static long fli_list_usb_by_port(flidomain_t domain, char ***names)
{
	HDEVINFO hDevInfo;
	SP_DEVICE_INTERFACE_DATA devInterfaceData;
	BOOL bRet = FALSE;
	DWORD dwIndex = 0;
	char **list = NULL;
	int matched = 0;

	hDevInfo = SetupDiGetClassDevs(&GUID_DEVINTERFACE_USB_DEVICE,
		NULL, NULL, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);

	debug(FLIDEBUG_FAIL, "Searching by port.");

	if (hDevInfo == INVALID_HANDLE_VALUE)
	{
		debug(FLIDEBUG_FAIL, "Could not obtain handle from SetupDiGetClassDevs(), error %d", GetLastError());
		return 0;
	}

	ZeroMemory(&devInterfaceData, sizeof(SP_DEVICE_INTERFACE_DATA));
	devInterfaceData.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);

	/* Allocate the list */
  if ((list = xcalloc((MAX_SEARCH + 1) * sizeof(char *), 1)) == NULL)
    return -ENOMEM;

	while(TRUE)
	{
		bRet = SetupDiEnumDeviceInterfaces(hDevInfo,
			NULL, /* PSP_DEVINFO_DATA DeviceInfoData */ 
			&GUID_DEVINTERFACE_USB_DEVICE,
			dwIndex, &devInterfaceData);

		if (bRet == TRUE)
		{
			PSP_DEVICE_INTERFACE_DETAIL_DATA detailData;
			DWORD DataSize, RequiredSize;
			SP_DEVINFO_DATA DeviceInfoData;
			DWORD dwRegType, dwRegSize;
			int match = 0;
			char *pBuffer;

			ZeroMemory(&DeviceInfoData, sizeof(SP_DEVINFO_DATA));
			DeviceInfoData.cbSize = sizeof(SP_DEVINFO_DATA);

			/* Get required size of detail data and the DEVINFO_DATA structure */
			if (SetupDiGetDeviceInterfaceDetail(hDevInfo,
				&devInterfaceData, NULL, 0, &DataSize, &DeviceInfoData) != TRUE)
			{
				if (GetLastError() != ERROR_INSUFFICIENT_BUFFER)
				{
					debug(FLIDEBUG_FAIL, "Could not obtain size for interface detail data, error %d",
					GetLastError());
					break;
				}
			}

			detailData = (PSP_DEVICE_INTERFACE_DETAIL_DATA) xmalloc(DataSize);
			ZeroMemory(detailData, DataSize);
			detailData->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);

			/* Get the detail data */
			if (SetupDiGetDeviceInterfaceDetail(hDevInfo, &devInterfaceData,
				detailData, DataSize, &RequiredSize, NULL) != TRUE)
			{
				debug(FLIDEBUG_FAIL, "Could not obtain interface detail data, error %d",
					GetLastError());

				xfree(detailData);
				break;
			}

			dwRegSize = 0;
			pBuffer = NULL;

			/* Get the service name (this should be "fliusb") */
			if (SetupDiGetDeviceRegistryProperty(hDevInfo,
				&DeviceInfoData, SPDRP_SERVICE, &dwRegType,
        NULL, 0, &dwRegSize) != TRUE)
			{
				if (GetLastError() != ERROR_INSUFFICIENT_BUFFER)
				{
					debug(FLIDEBUG_FAIL, "Could not obtain size for service name, error %d",
						GetLastError());

					xfree(detailData);
					break;
				}
			}

			/* Allocate buffer for the friendly name. */
			pBuffer = (TCHAR *) xmalloc (dwRegSize * sizeof(TCHAR));

			/* Retreive the service name */
			if (SetupDiGetDeviceRegistryProperty(hDevInfo,
				&DeviceInfoData, SPDRP_SERVICE, &dwRegType, (PBYTE) pBuffer,
				dwRegSize, &dwRegSize) != TRUE)
			{
				debug(FLIDEBUG_FAIL, "Could not get service name, error %d",
					GetLastError());

				xfree(detailData);
				xfree(pBuffer);
				break;
			}
			debug(FLIDEBUG_INFO, "Found [%s] [%s]", detailData->DevicePath, pBuffer);

			/* Is it our driver? */
			SERVICE_MATCH

			if (match != 0)
			{
				Usb_Device_Descriptor usbdesc;
				HANDLE fd;
				int pid;
				DWORD read;

				do
				{
					fd = CreateFile(detailData->DevicePath, GENERIC_WRITE,
						FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);

					if (fd == INVALID_HANDLE_VALUE)
					{
						break;
					}

					if (DeviceIoControl(fd, IOCTL_GET_DEVICE_DESCRIPTOR,
						NULL, 0, &usbdesc, sizeof(usbdesc), &read, NULL) == FALSE)
					{
						debug(FLIDEBUG_WARN, "Couldn't read device description, error: %d", GetLastError());
						CloseHandle(fd);
						break;
					}

					pid = usbdesc.idProduct;

					debug(FLIDEBUG_INFO, "Found USB PID: 0x%04x", pid);

					if (
						((pid == FLIUSB_CAM_ID) && ((domain & 0x7f00) == FLIDEVICE_CAMERA)) ||
						((pid == FLIUSB_PROLINE_ID) && ((domain & 0x7f00) == FLIDEVICE_CAMERA)) ||
						((pid == FLIUSB_FOCUSER_ID) && ((domain & 0x7f00) == FLIDEVICE_FOCUSER)) ||
						((pid == FLIUSB_FILTER_ID) && ((domain & 0x7f00) == FLIDEVICE_FILTERWHEEL)) ||
						((pid >= 0x0100) && (pid < 0x0110) && ((domain & 0x7f00) == FLIDEVICE_CAMERA))
						)
					{
						GET_STRING_DESCRIPTOR_IN sd;
						char name[MAX_PATH];
						char model[MAX_PATH];
						flidev_t dev;

						sd.Index = 3;
						sd.LanguageId = 0x00;
						ZeroMemory(name, sizeof(name));
						ZeroMemory(model, sizeof(model));

						if ( (usbdesc.iSerialNumber == 3) &&
							   (DeviceIoControl(fd, IOCTL_GET_STRING_DESCRIPTOR,
																	&sd, sizeof(sd), name, sizeof(name),
																	&read, NULL) != FALSE) )
						{
							DWORD i;
							/* de-unicode it */
							for (i = 0; i < read; i++)
							{
								name[i] = name[(i + 1) * 2];
							}
							name[i] = '\0';
							debug(FLIDEBUG_INFO, "Adding %s", name);
						}
						else
						{
							strncpy(name, detailData->DevicePath, MAX_PATH - 1);
						}

						/* Get model information */
						if (FLIOpen(&dev, detailData->DevicePath, domain) == 0)
						{
							xasprintf(&list[matched], "%s;%s", name, DEVICE->devinfo.model);
							matched ++;

							FLIClose(dev);
						}
					}
					else
					{
						debug(FLIDEBUG_INFO, "Not the device we are looking for.");
					}
					CloseHandle(fd);
				}
				while (0 == 1);
			}

			xfree(detailData);
			xfree(pBuffer);
		}
		else
		{
			int err = GetLastError();

			if ( (err == ERROR_NO_MORE_ITEMS) || (err == ERROR_FILE_NOT_FOUND) )
			{
				break;
			}
		}

		dwIndex++; 
	}

	SetupDiDestroyDeviceInfoList(hDevInfo);

	if(matched == 0)
	{
		*names = NULL;
		xfree(list);
		return 0;
	}

	list[matched++] = NULL;
	*names = list;

	return 0;
}

static long fli_list_tree(const char *root, flidomain_t domain, char ***names)
{
  int matched = 0, device = 0, max_search = MAX_SEARCH;
  char fname[NAME_LEN_MAX], **list, name[NAME_LEN_MAX];
	flidev_t dev;
	char prefix[NAME_LEN_MAX];
	int cnt, index;

	/* Allocate the list */
  if ((list = xcalloc((MAX_SEARCH + 1) * sizeof(char *), 1)) == NULL)
    return -ENOMEM;

	index = 0;
	while (root[index] != '\0')
	{
		cnt = 0;
		while ((root[index] != '\0') &&
					 (root[index] != ',') &&
					 (cnt < NAME_LEN_MAX))
		{
			prefix[cnt] = root[index];
			cnt++;
			index++;
		}
		prefix[cnt] = '\0';
		if (root[index] != '\0')
			index++;

		device = 0;
		while(device < max_search)
		{
			if (snprintf(fname, NAME_LEN_MAX, "%s%d", prefix,
				device) >= NAME_LEN_MAX)
			{
				xfree(list);
				return -EOVERFLOW;
			}

			if (FLIOpen(&dev, fname, domain) == 0)
			{
				if (snprintf(name, NAME_LEN_MAX, "%s;%s", fname,
					DEVICE->devinfo.model) >= NAME_LEN_MAX)
				{
					xfree(list);
					return -EOVERFLOW;
				}

				list[matched++] = xstrdup(name);

				FLIClose(dev);
			}
			device++;
		}
	}

	if(matched == 0)
	{
		*names = NULL;
		xfree(list);
		return 0;
	}

  /* Terminate the list */
  list[matched++] = NULL;

//  list = xrealloc(list, matched * sizeof(char *));
  *names = list;
  return 0;
}

static long fli_list_usb(flidomain_t domain, char ***names)
{
	switch (domain & 0x7f00)
	{
		case FLIDEVICE_CAMERA:
			if ( (domain & 0x8000) == 0 )
			  return fli_list_tree(LIST_USB_CAM_PREFIX_LIST, domain, names);
			else
				return fli_list_usb_by_port(domain, names);

		case FLIDEVICE_FOCUSER:
		  return fli_list_tree(LIST_USB_FOCUSER_PREFIX_LIST, domain, names);

		case FLIDEVICE_FILTERWHEEL:
		  return fli_list_tree(LIST_USB_FILTER_PREFIX_LIST, domain, names);

		default:
			return -EINVAL;
	}

  /* Not reached */
	return -EINVAL;
}

static long fli_list_serial(flidomain_t domain, char ***names)
{
	switch (domain & 0xff00)
	{
		case FLIDEVICE_FOCUSER:
		  return fli_list_tree("\\\\?\\COM", domain, names);

		case FLIDEVICE_FILTERWHEEL:
		  return fli_list_tree("\\\\?\\COM", domain, names);

		default:
			return -EINVAL;
	}

  /* Not reached */
	return -EINVAL;
}

static long fli_list_parport(flidomain_t domain, char ***names)
{
	switch (domain & 0xff00)
	{
		case FLIDEVICE_CAMERA:
		  return fli_list_tree("ccdpar", domain, names);

		default:
			return -EINVAL;
	}

  /* Not reached */
	return -EINVAL;
}

#undef NAME_LEN_MAX

