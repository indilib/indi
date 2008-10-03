// ApogeeUsb.cpp : Library of basic USB functions for Apogee APn/Alta.
//

#include <assert.h>
#include <sys/io.h>
#include <sys/time.h>                                                           
#include <sys/resource.h>
#include <sys/ioctl.h>
#include <string.h>
#include <sched.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>


#include "ApogeeUsb.h"
#include "ApogeeUsbErr.h"
#include "ApogeeLinux.h"


#define HANDLE   unsigned int
#define ULONG    unsigned int
#define BOOLEAN  unsigned int
#define USHORT   unsigned short

#define APOGEE_USB_DEVICE "/dev/usb/alta"
#define INVALID_HANDLE_VALUE  -1


// Global variables used in this DLL
HANDLE		g_hSysDriver;
ULONG		g_UsbImgSizeBytes;


// 1044480
// 520192
// 126976
// 61440
// 49152
// 4096
#define IMAGE_BUFFER_SIZE		126976		// Number of requested bytes in a transfer
//#define IMAGE_BUFFER_SIZE		253952		// Number of requested bytes in a transfer

															
// This is an example of an exported function.
APN_USB_TYPE ApnUsbOpen( unsigned short DevNumber )
{

	char deviceName[128];

	g_hSysDriver	= 0;
	g_UsbImgSizeBytes	= 0;

	// Open the driver
        sprintf(deviceName,"%s%d",APOGEE_USB_DEVICE,DevNumber);
        g_hSysDriver = ::open(deviceName,O_RDONLY);

	if ( g_hSysDriver == INVALID_HANDLE_VALUE )
	{
		return APN_USB_ERR_OPEN;		// Failure to open device
	}

	return APN_USB_SUCCESS;		// Success
}


APN_USB_TYPE ApnUsbClose( void )
{
	if ( (g_hSysDriver != INVALID_HANDLE_VALUE ) && (g_hSysDriver != 0) )
	{
		::close( g_hSysDriver );
		g_hSysDriver = 0;
	}

	return APN_USB_SUCCESS;		// Success
}


APN_USB_TYPE ApnUsbDiscovery( unsigned short *UsbCamCount, 
				APN_USB_CAMINFO UsbCamInfo[] )
{
	HANDLE	hDriver;
	char deviceName[64];
        unsigned short RegNumber;
	unsigned short retval;
        struct apIOparam request;
        USHORT RegData;
	*UsbCamCount = 0;
 

	for ( int i=0; i<APN_USB_MAXCAMERAS; i++ )
	{
													  NULL,
													  NULL,

		// Open the driver
                sprintf(deviceName,"%s%d",APOGEE_USB_DEVICE,i);
                hDriver = ::open(deviceName,O_RDONLY);

		if ( hDriver != INVALID_HANDLE_VALUE )
		{
			// first set the camera number
			UsbCamInfo[*UsbCamCount].CamNumber = i;

			// now determine the camera model with a read operation
			BOOLEAN Success;
			USHORT	FpgaReg;
			USHORT	RegData;
			ULONG	BytesReceived;

			FpgaReg = 100;
                        request.reg = FpgaReg;
                        request.param1=(unsigned long)&retval;
                        Success=ioctl(hDriver,APUSB_READ_USHORT,(unsigned long)&request);
                        RegData = (unsigned short)retval;

			if ( Success )
			{
				UsbCamInfo[*UsbCamCount].CamModel = RegData & 0x00FF;
				(*UsbCamCount)++;		
			}
		}

        }

	return APN_USB_SUCCESS;
}


APN_USB_TYPE ApnUsbReadReg( unsigned short FpgaReg, unsigned short *FpgaData )
{
	BOOLEAN Success;
	USHORT	RegData;
        unsigned short RegNumber;
        unsigned short retval;
        struct apIOparam request;

	if ( (g_hSysDriver == INVALID_HANDLE_VALUE) || (g_hSysDriver == 0) )
	{
		return APN_USB_ERR_OPEN;
	}


        request.reg = FpgaReg;
        request.param1=(unsigned long)&retval;
        Success=ioctl(g_hSysDriver,APUSB_READ_USHORT,(unsigned long)&request);
        RegData = (unsigned short)retval;

	if ( (!Success) )
	{
		return APN_USB_ERR_READ;
	}

	*FpgaData = RegData;

	return APN_USB_SUCCESS;		// Success
}


APN_USB_TYPE ApnUsbWriteReg( unsigned short FpgaReg, unsigned short FpgaData )
{
	BOOLEAN Success;
        unsigned short RegNumber;
        struct apIOparam request;

	if ( (g_hSysDriver == INVALID_HANDLE_VALUE) || (g_hSysDriver == 0) )
	{
		return APN_USB_ERR_OPEN;
	}

        request.reg = FpgaReg;
        request.param1=(int)FpgaData;
        Success=ioctl(g_hSysDriver,APUSB_WRITE_USHORT,(unsigned long)&request);
	if ( !Success )
		return APN_USB_ERR_WRITE;

	return APN_USB_SUCCESS;		// Success
}


APN_USB_TYPE ApnUsbWriteRegMulti( unsigned short FpgaReg, unsigned short FpgaData[], unsigned short RegCount )
{
	unsigned short	Counter;

	for ( Counter=0; Counter<RegCount; Counter++ )
	{
		if ( ApnUsbWriteReg( FpgaReg, FpgaData[Counter] ) != APN_USB_SUCCESS )
		{
			return APN_USB_ERR_WRITE;
		}
	}

	return APN_USB_SUCCESS;		// Success
}

APN_USB_TYPE ApnUsbWriteRegMultiMRMD( unsigned short FpgaReg[], 
									  unsigned short FpgaData[],
									  unsigned short RegCount )
{
	unsigned short	Counter;

	for ( Counter=0; Counter<RegCount; Counter++ )
	{
		if ( ApnUsbWriteReg( FpgaReg[Counter], FpgaData[Counter] ) != APN_USB_SUCCESS )
		{
			return APN_USB_ERR_WRITE;
		}
	}

	return APN_USB_SUCCESS;
}


APN_USB_TYPE ApnUsbReadStatusRegs( unsigned short *StatusReg,
				   unsigned short *HeatsinkTempReg,
				   unsigned short *CcdTempReg,
				   unsigned short *CoolerDriveReg,
				   unsigned short *VoltageReg,
				   unsigned short *TdiCounter,
				   unsigned short *SequenceCounter )
{
	BOOLEAN		Success;
	unsigned int	BytesReceived;
        unsigned short RegNumber;
        struct apIOparam request;
	unsigned short *Data;
	unsigned char	StatusData[21];	
	
        request.reg = 0;  //check this *******************
        request.param1=(unsigned long)&StatusData;
        Success=ioctl(g_hSysDriver,APUSB_READ_STATUS,(unsigned long)&request);

//	if ( !Success )
//		return APN_USB_ERR_STATUS;
	Data = (unsigned short *)StatusData;

	*HeatsinkTempReg	= Data[0];
	*CcdTempReg		= Data[1];
	*CoolerDriveReg		= Data[2];
	*VoltageReg		= Data[3];
	*TdiCounter		= Data[4];
	*SequenceCounter	= Data[5];
	*StatusReg		= Data[6];

	if ( (StatusData[20] & 0x01) != 0 )
	{
		*StatusReg |= 0x8;
	}

	return APN_USB_SUCCESS;
}


APN_USB_TYPE ApnUsbStartExp( unsigned short ImageWidth,
							 unsigned short ImageHeight )
{
	BOOLEAN Success;
	ULONG	ImageSize;
	ULONG	BytesReceived;
        unsigned short RegNumber;
        struct apIOparam request;


	if ( (g_hSysDriver == INVALID_HANDLE_VALUE) || (g_hSysDriver == 0) )
	{
		return APN_USB_ERR_OPEN;
	}

	g_UsbImgSizeBytes = ImageWidth * ImageHeight * 2;
	ImageSize = ImageWidth * ImageHeight;

	if ( g_UsbImgSizeBytes == 0 )
	{
	 	return APN_USB_ERR_START_EXP;
	}

        request.reg = (int)ImageSize;
        request.param1= 0;
        Success=ioctl(g_hSysDriver,APUSB_PRIME_USB_DOWNLOAD,(unsigned long)&request);
    
	if ( !Success )
	{
		return APN_USB_ERR_START_EXP;
	}

	return APN_USB_SUCCESS;
}


APN_USB_TYPE ApnUsbStopExp( bool DigitizeData )
{
	BOOLEAN Success;
	ULONG	BytesReceived;
        unsigned short RegNumber;
        struct apIOparam request;


	if ( (g_hSysDriver == INVALID_HANDLE_VALUE) || (g_hSysDriver == 0) )
	{
		return APN_USB_ERR_OPEN;
	}

	if ( DigitizeData == false )
	{
                request.reg = 0;
                request.param1 = 0;
                Success=ioctl(g_hSysDriver,APUSB_STOP_USB_IMAGE,(unsigned long)&request);
    
		if ( !Success )
		{
			return APN_USB_ERR_STOP_EXP;
		}
	}

	return APN_USB_SUCCESS;
}


APN_USB_TYPE OrigApnUsbGetImage( unsigned short *pMem )
{
	BOOLEAN Success;
	ULONG	ImageBytesRemaining;
	ULONG	ReceivedSize;
	ULONG   retval;
	unsigned char  *pRequestData;
        unsigned short RegNumber;
        struct apIOparam request;


	if ( (g_hSysDriver == INVALID_HANDLE_VALUE) || (g_hSysDriver == 0) )
	{
		return APN_USB_ERR_OPEN;
	}

	ImageBytesRemaining = g_UsbImgSizeBytes;

//	pRequestData = new UCHAR[IMAGE_BUFFER_SIZE];

	////////////////////////
	ULONG LoopCount = g_UsbImgSizeBytes / IMAGE_BUFFER_SIZE;
	ULONG Remainder	= g_UsbImgSizeBytes - ( LoopCount * IMAGE_BUFFER_SIZE );
	ULONG MemIterator = IMAGE_BUFFER_SIZE / 2;
	ULONG Counter;


	for ( Counter=0; Counter<LoopCount; Counter++ )
	{
                request.reg = 0;  //check this ***************
                request.param1= *pMem;
                request.param2=IMAGE_BUFFER_SIZE;
                Success=ioctl(g_hSysDriver,APUSB_READ_USB_IMAGE,(unsigned long)&request);
                ReceivedSize = (unsigned short)retval;

		if ( (!Success) || (ReceivedSize != IMAGE_BUFFER_SIZE) )
		{
			Success = 0;
			break;
		}
		else
		{
			pMem += MemIterator;
		}
	}
	
	if ( (Success) && (Remainder != 0) )
	{
                request.reg = 0; //check this *************************8
                request.param1= *pMem;
                request.param2=Remainder;
                Success=ioctl(g_hSysDriver,APUSB_READ_USB_IMAGE,(unsigned long)&request);
                ReceivedSize = (unsigned short)retval;

		if ( ReceivedSize != Remainder )
			Success = 0;
	}

//	delete [] pRequestData;

	if ( !Success )
		return APN_USB_ERR_IMAGE_DOWNLOAD;

	return APN_USB_SUCCESS;		// Success
}

APN_USB_TYPE ApnUsbGetImage( unsigned short *pMem )
{
	BOOLEAN Success;
	ULONG	ImageBytesRemaining;
	ULONG	ReceivedSize;
	ULONG   retval;
	unsigned char  *pRequestData;
        unsigned short RegNumber;
        struct apIOparam request;

	Success = 1;
	if ( (g_hSysDriver == INVALID_HANDLE_VALUE) || (g_hSysDriver == 0) )
	{
		return APN_USB_ERR_OPEN;
	}

	ImageBytesRemaining = g_UsbImgSizeBytes;


	////////////////////////
	ULONG LoopCount = g_UsbImgSizeBytes / IMAGE_BUFFER_SIZE;
	ULONG Remainder	= g_UsbImgSizeBytes - ( LoopCount * IMAGE_BUFFER_SIZE );
	ULONG MemIterator = IMAGE_BUFFER_SIZE / 2;
	ULONG Counter;


	for ( Counter=0; Counter<LoopCount; Counter++ )
	{
		ReceivedSize = read(g_hSysDriver,pMem,IMAGE_BUFFER_SIZE);

		if ( ReceivedSize != IMAGE_BUFFER_SIZE )
		{
			Success = 0;
			break;
		}
		else
		{
			pMem += MemIterator;
	printf(".");
		}
	}
	
	if ( Remainder != 0 )
	{
		ReceivedSize = read(g_hSysDriver,pMem,Remainder);

		if ( ReceivedSize != Remainder )
			Success = 0;
	}
	printf("\n");

	if ( !Success )
		return APN_USB_ERR_IMAGE_DOWNLOAD;

	return APN_USB_SUCCESS;		// Success
}


APN_USB_TYPE ApnUsbReset()
{
	BOOLEAN Success;
	ULONG	BytesReceived;
        unsigned short RegNumber;
        struct apIOparam request;


	if ( (g_hSysDriver == INVALID_HANDLE_VALUE) || (g_hSysDriver == 0) )
	{
		return APN_USB_ERR_OPEN;
	}

        request.reg = 0;
        request.param1 = 0;
        Success=ioctl(g_hSysDriver,APUSB_USB_RESET,(unsigned long)&request);
    
	if ( !Success )
	{
		return APN_USB_ERR_RESET;
	}

	return APN_USB_SUCCESS;
}


















