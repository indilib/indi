// ApogeeUsb.cpp : Library of basic USB functions for Apogee APn/Alta.
//

#include <assert.h>
#ifndef OSX
#ifndef OSXI
#include <sys/io.h>
#endif
#endif
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
#include "usb.h"
extern int usb_debug;

/*#define HANDLE   unsigned long */
#define ULONG    unsigned int
#define BOOLEAN  unsigned int
#define USHORT   unsigned short
#define PUCHAR   unsigned char *
#define APUSB_VERSION_MAJOR 1
#define APUSB_VERSION_MINOR 4

#define APUSB_CUSTOM_SN_BYTE_COUNT      64
#define APUSB_8051_REV_BYTE_COUNT       3
 
 
#define APUSB_CUSTOM_SN_DID_SUPPORT     0x0011
#define APUSB_8051_REV_DID_SUPPORT      0x0011
#define APUSB_CI_DID_SUPPORT            0x0011
 
#define APUSB_PID_ALTA                          0x0010
#define APUSB_PID_ASCENT                        0x0020
 
 
#include "ApogeeUsb.h"
#include "ApogeeUsbErr.h"
#include "ApogeeLinux.h"
#include "ApnCamera_USB.h"

#define FALSE false
#define BYTE  char


#define APOGEE_USB_DEVICE "/dev/usb/alta"
#define INVALID_HANDLE_VALUE  -1

#define VND_ANCHOR_LOAD_INTERNAL		0xA0
#define VND_APOGEE_CMD_BASE			0xC0
#define VND_APOGEE_STATUS			( VND_APOGEE_CMD_BASE + 0x0 )
#define VND_APOGEE_CAMCON_REG			( VND_APOGEE_CMD_BASE + 0x2 )
#define VND_APOGEE_BUFCON_REG			( VND_APOGEE_CMD_BASE + 0x3 )
#define VND_APOGEE_SET_SERIAL			( VND_APOGEE_CMD_BASE + 0x4 )
#define VND_APOGEE_SERIAL			( VND_APOGEE_CMD_BASE + 0x5 )
#define VND_APOGEE_EEPROM			( VND_APOGEE_CMD_BASE + 0x6 )
#define VND_APOGEE_SOFT_RESET			( VND_APOGEE_CMD_BASE + 0x8 )
#define VND_APOGEE_GET_IMAGE			( VND_APOGEE_CMD_BASE + 0x9 )
#define VND_APOGEE_STOP_IMAGE			( VND_APOGEE_CMD_BASE + 0xA )
#define VND_APOGEE_VENDOR			( VND_APOGEE_CMD_BASE + 0xB )
#define VND_APOGEE_VERSION			( VND_APOGEE_CMD_BASE + 0xC )
#define VND_APOGEE_DATA_PORT                    ( VND_APOGEE_CMD_BASE + 0xD )
#define VND_APOGEE_CONTROL_PORT                 ( VND_APOGEE_CMD_BASE + 0xE )


#define USB_ALTA_VENDOR_ID	0x125c
#define USB_ALTA_PRODUCT_ID	0x0010
#define USB_ASCENT_PRODUCT_ID	0x0020
#define USB_DIR_IN  USB_ENDPOINT_IN
#define USB_DIR_OUT USB_ENDPOINT_OUT

typedef struct _APN_USB_REQUEST
{
        unsigned char   Request;
        unsigned char   Direction;
        unsigned short  Value;
        unsigned short  Index;
} APN_USB_REQUEST, *PAPN_USB_REQUEST;


// Global variables used in this DLL
struct usb_dev_handle	*g_hSysDriver;
ULONG	g_UsbImgSizeBytes;
char	controlBuffer[1024];
ULONG   firmwareRevision;
unsigned char idProduct;

#define IMAGE_BUFFER_SIZE	126976	// Number of requested bytes in a transfer
//#define IMAGE_BUFFER_SIZE	253952	// Number of requested bytes in a transfer

															
// This is an example of an exported function.
bool ApnUsbCreateRequest( unsigned char	Request,
							bool			InputRequest,
							unsigned short	Index,
							unsigned short	Value,
							unsigned long	Length,
							unsigned char	*pBuffer )
{
	APN_USB_REQUEST	UsbRequest;
	BOOLEAN			Success;
	DWORD			BytesReceived;
	UCHAR			Direction;
	PUCHAR			pSendBuffer;
	ULONG			SendBufferLen;
	unsigned short 		Detail;
	unsigned char 		sDetail[2];

	if ( InputRequest )
		Direction = USB_DIR_IN;
	else
		Direction = USB_DIR_OUT;

	UsbRequest.Request		= Request;
	UsbRequest.Direction	= Direction;
	UsbRequest.Index		= Index;
	UsbRequest.Value		= Value;
	sDetail[0] = Index;
	sDetail[1] = Value;

	if ( Direction == USB_DIR_IN )
	{
		SendBufferLen	= sizeof(APN_USB_REQUEST);
		pSendBuffer		= new UCHAR[SendBufferLen];

		memcpy( pSendBuffer, 
				&UsbRequest, 
				sizeof(APN_USB_REQUEST) );
	}
	else
	{
		SendBufferLen	= sizeof(APN_USB_REQUEST) + Length;
		pSendBuffer		= new UCHAR[SendBufferLen];

		memcpy( pSendBuffer, 
				&UsbRequest, 
				sizeof(APN_USB_REQUEST) );

		memcpy( pSendBuffer + sizeof(APN_USB_REQUEST),
				pBuffer,
				Length );
	}


        Success = usb_control_msg((struct usb_dev_handle *)g_hSysDriver, 
                                Direction | USB_TYPE_VENDOR | USB_RECIP_DEVICE, Request,
                                            0, Detail,(char *)&BytesReceived, 2, 10000);

	delete [] pSendBuffer;

	if ( !Success )
		return false;
	
	return true;
}


APN_USB_TYPE ApnUsbOpen( unsigned short DevNumber ,  char *SysDeviceName )
{

	char deviceName[128];
	struct usb_bus *bus;
	struct usb_device *dev;
//	struct usb_dev_;
	struct usb_dev_handle	*hDevice;

	usb_init();

	usb_find_busses();
	usb_find_devices();

	char string[256];

	int found = 0;

	/* find ALTA device */
	for(bus = usb_busses; bus && !found; bus = bus->next) {
		for(dev = bus->devices; dev && !found; dev = dev->next) {
			if (dev->descriptor.idVendor == USB_ALTA_VENDOR_ID && 
			     ( (dev->descriptor.idProduct == USB_ALTA_PRODUCT_ID) ||
                               (dev->descriptor.idProduct == USB_ASCENT_PRODUCT_ID) ) ) {
				found = found+1;
				if (found == DevNumber) {
					hDevice = usb_open(dev);
					firmwareRevision = dev->descriptor.bcdDevice;
                                        idProduct = dev->descriptor.idProduct;
//					cerr << "Found ALTA USB. Attempting to open... ";
					if (hDevice) {
//					if (!usb_get_string_simple(hDevice, 
//						dev->descriptor.iSerialNumber, 
//						string, sizeof(string))) 
//							throw DevOpenError();
//					cerr << "Success.\n";
//					cerr << "Serial number: " << string << endl;
					}
					else return APN_USB_ERR_OPEN;
				}
			}
		}
	}

	if (found == 0) return APN_USB_ERR_OPEN;

#ifdef OSX
	usb_set_configuration(hDevice, 0x0);
	usb_claim_interface(hDevice, 0x0);
	printf("DRIVER: claimed interface\n");
#endif

	g_hSysDriver		= hDevice;
	g_UsbImgSizeBytes	= 0;
//	printf("DRIVER: opened device\n");
//        usb_debug = 4;

	return APN_USB_SUCCESS;		// Success
}


APN_USB_TYPE ApnUsbClose( void )
{

	if ( g_hSysDriver != 0 )
	{
		usb_release_interface(g_hSysDriver, 0x0);
		usb_close(g_hSysDriver);
		g_hSysDriver = 0;
	}

	return APN_USB_SUCCESS;		// Success
}



APN_USB_TYPE ApnUsbReadReg(   unsigned short FpgaReg, unsigned short *FpgaData )
{
    int Success;
    unsigned short RegData;

  
    Success = usb_control_msg((struct usb_dev_handle *)g_hSysDriver, 
                                USB_DIR_IN | USB_TYPE_VENDOR | USB_RECIP_DEVICE, VND_APOGEE_CAMCON_REG,
                                            0, FpgaReg,(char *)&RegData, 2, 10000);
#ifdef OSX 
    swab(&RegData,FpgaData,2);
#else
    *FpgaData = RegData;
#endif
//    printf("DRIVER: usb read reg=%x data=%x s=%x\n",FpgaReg,*FpgaData,Success);
    if ( !Success )
		return APN_USB_ERR_WRITE;
    return APN_USB_SUCCESS;		// Success
}

APN_USB_TYPE ApnUsbWriteReg(  unsigned short FpgaReg, unsigned short FpgaData )
{
    char *cbuf;
    int Success;

#ifdef OSX 
    char cswap;
    cbuf = (char *)&FpgaData;
    cswap = cbuf[0];
    cbuf[0] = cbuf[1];
    cbuf[1] = cswap;
#else
    cbuf = (char *)&FpgaData;
#endif
    
    Success = usb_control_msg((struct usb_dev_handle *)g_hSysDriver, 
                                  USB_DIR_OUT | USB_TYPE_VENDOR | USB_RECIP_DEVICE,  VND_APOGEE_CAMCON_REG,
                                   0, FpgaReg, cbuf, 2, 10000);
//    printf("DRIVER: usb write reg=%x data=%x s=%x\n",FpgaReg,FpgaData,Success); 
    if ( !Success )
		return APN_USB_ERR_WRITE;
    return APN_USB_SUCCESS;		// Success


}




APN_USB_TYPE ApnUsbWriteRegMulti(   unsigned short FpgaReg, unsigned short FpgaData[], unsigned short RegCount )
{
	unsigned short	Counter;

	for ( Counter=0; Counter<RegCount; Counter++ )
	{
		if ( ApnUsbWriteReg(FpgaReg, FpgaData[Counter] ) != APN_USB_SUCCESS )
		{
			return APN_USB_ERR_WRITE;
		}
	}

	return APN_USB_SUCCESS;		// Success
}

APN_USB_TYPE ApnUsbWriteRegMultiMRMD(   unsigned short FpgaReg[], 
				      unsigned short FpgaData[],
				      unsigned short RegCount )
{
	unsigned short	Counter;

	for ( Counter=0; Counter<RegCount; Counter++ )
	{
		if ( ApnUsbWriteReg(FpgaReg[Counter], 
				     FpgaData[Counter] ) != APN_USB_SUCCESS )
		{
			return APN_USB_ERR_WRITE;
		}
	}

	return APN_USB_SUCCESS;
}


APN_USB_TYPE ApnUsbReadStatusRegs( bool UseAdvStatus,
				   bool *DoneFlag,
				   unsigned short *StatusReg,
				   unsigned short *HeatsinkTempReg,
				   unsigned short *CcdTempReg,
				   unsigned short *CoolerDriveReg,
				   unsigned short *VoltageReg,
				   unsigned short *TdiCounter,
				   unsigned short *SequenceCounter,
				   unsigned short *MostRecentFrame,
				   unsigned short *ReadyFrame,
				   unsigned short *CurrentFrame)
{
	BOOLEAN		Success;
	unsigned int	BytesReceived;
	unsigned short *Data;
	unsigned char	StatusData[21];	
	unsigned char	AdvStatusData[27]; 

	if (UseAdvStatus) {

		Success = usb_control_msg(g_hSysDriver,
                                  USB_DIR_IN | USB_TYPE_VENDOR | USB_RECIP_DEVICE,  VND_APOGEE_STATUS,
                                    0, 0, (char *)&AdvStatusData, 27, 10000);

//	if ( !Success )
//		return APN_USB_ERR_STATUS;
	Data = (unsigned short *)AdvStatusData;
#ifdef OSX 
	    swab(&Data[0],HeatsinkTempReg,2);
	    swab(&Data[1],CcdTempReg,2);
	    swab(&Data[2],CoolerDriveReg,2);
	    swab(&Data[3],VoltageReg,2);
	    swab(&Data[4],TdiCounter,2);
	    swab(&Data[5],SequenceCounter,2);
	    swab(&Data[6],StatusReg,2);
	    swab(&Data[8],MostRecentFrame,2);
	    swab(&Data[9],ReadyFrame,2);
	    swab(&Data[10],CurrentFrame,2);
#else
		*HeatsinkTempReg	= Data[0];
		*CcdTempReg		= Data[1];
		*CoolerDriveReg		= Data[2];
		*VoltageReg		= Data[3];
		*TdiCounter		= Data[4];
		*SequenceCounter	= Data[5];
		*StatusReg		= Data[6];
		*MostRecentFrame	= Data[8]; 
		*ReadyFrame		= Data[9]; 
		*CurrentFrame		= Data[10]; 
#endif
		*DoneFlag = false;

		if ( (AdvStatusData[26] & 0x01) != 0 )
		{
			*DoneFlag = true;
//			*StatusReg |= 0x8;
		}
//    printf("DRIVER: usb status adv26=%x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x\n",
//             AdvStatusData[0],AdvStatusData[1],AdvStatusData[2],AdvStatusData[3],
//             AdvStatusData[4],AdvStatusData[5],AdvStatusData[6],AdvStatusData[7],
//             AdvStatusData[8],AdvStatusData[9],AdvStatusData[10],AdvStatusData[11],
//             AdvStatusData[12],AdvStatusData[13],AdvStatusData[14],AdvStatusData[15]
//          ); 
//    printf("DRIVER: usb status adv26=%x %x %x %x %x %x %x %x %x %x %x\n",
//             AdvStatusData[16],AdvStatusData[17],AdvStatusData[18],AdvStatusData[19],
//             AdvStatusData[20],AdvStatusData[21],AdvStatusData[22],AdvStatusData[23],
//             AdvStatusData[24],AdvStatusData[25],AdvStatusData[26]
//          ); 

	} else {

		Success = usb_control_msg(g_hSysDriver,
                                  USB_DIR_IN | USB_TYPE_VENDOR | USB_RECIP_DEVICE,  VND_APOGEE_STATUS,
                                    0, 0, (char *)&StatusData, 21, 10000);

//	if ( !Success )
//		return APN_USB_ERR_STATUS;
	Data = (unsigned short *)StatusData;
#ifdef OSX 
	    swab(&Data[0],HeatsinkTempReg,2);
	    swab(&Data[1],CcdTempReg,2);
	    swab(&Data[2],CoolerDriveReg,2);
	    swab(&Data[3],VoltageReg,2);
	    swab(&Data[4],TdiCounter,2);
	    swab(&Data[5],SequenceCounter,2);
	    swab(&Data[6],StatusReg,2);
#else
		*HeatsinkTempReg	= Data[0];
		*CcdTempReg		= Data[1];
		*CoolerDriveReg		= Data[2];
		*VoltageReg		= Data[3];
		*TdiCounter		= Data[4];
		*SequenceCounter	= Data[5];
		*StatusReg		= Data[6];
#endif
		*DoneFlag = false;

		if ( (StatusData[20] & 0x01) != 0 )
		{
			*DoneFlag = true;
			// *StatusReg |= 0x8;
		}


        }


	return APN_USB_SUCCESS;
}


APN_USB_TYPE ApnUsbStartExp( unsigned short ImageCount , unsigned short ImageWidth,
			     unsigned short ImageHeight )
{
	BOOLEAN Success;
	ULONG	ImageSize;
	unsigned short	BytesReceived;
	ULONG img1, img2, imgw;
	UCHAR	DeviceData[3]; 

//	if ( (g_hSysDriver) == 0)
//	{
//		return APN_USB_ERR_OPEN;
//	}
       
	g_UsbImgSizeBytes = ImageWidth * ImageHeight * 2;
	ImageSize = ImageWidth * ImageHeight;

	if ( g_UsbImgSizeBytes == 0 )
	{
	 	return APN_USB_ERR_START_EXP;
	}
        if (ImageCount == 1) {
           if ((idProduct == APUSB_PID_ALTA) &&  (firmwareRevision < 16)) {
		Success = usb_control_msg(g_hSysDriver,
                                   USB_DIR_IN | USB_TYPE_VENDOR | USB_RECIP_DEVICE,  VND_APOGEE_GET_IMAGE,
                                   (ImageSize >> 16) & 0xFFFF, 
				   (ImageSize & 0xFFFF), (char *)&BytesReceived, 4, 10000);
           } else {
		Success = usb_control_msg(g_hSysDriver,
                                   USB_DIR_OUT | USB_TYPE_VENDOR | USB_RECIP_DEVICE,  VND_APOGEE_GET_IMAGE,
                                   (ImageSize >> 16) & 0xFFFF, 
				   (ImageSize & 0xFFFF), NULL, 0, 10000);
           }
	} else {

	        DeviceData[0] = (UCHAR)(ImageCount & 0xFF);
	        DeviceData[1] = (UCHAR)((ImageCount >> 8) & 0xFF);
		DeviceData[2] = 0;
		Success = usb_control_msg(g_hSysDriver,
                                   USB_DIR_OUT | USB_TYPE_VENDOR | USB_RECIP_DEVICE,  VND_APOGEE_GET_IMAGE,
                                   (ImageSize >> 16) & 0xFFFF, 
				   (ImageSize & 0xFFFF), (char *)                                 &DeviceData, 
				   3, 10000);


	}

//        printf("DRIVER: startexp s=%x\n",Success); 
//	if ( !Success )
//	{
//		return APN_USB_ERR_START_EXP;
//	}

	return APN_USB_SUCCESS;
}

APN_USB_TYPE ApnUsbStartCI(	
							unsigned short	ImageWidth, 
							unsigned short	ImageHeight ) 
{ 
	BOOLEAN Success; 
	ULONG	ImageSize; 
	ULONG	BytesReceived; 
	UCHAR	DeviceData[3]; 
 
 
//	if ( (g_hSysDriver) == 0)
//	{
//		return APN_USB_ERR_OPEN;
//	}
      

	if ( (ImageWidth == 0) || (ImageHeight == 0 ) ) 
	{ 
		return APN_USB_ERR_START_CI; 
	} 
 
	ImageSize	= ImageWidth * ImageHeight; 
 
	DeviceData[0] = 0x2; 
	DeviceData[1] = 0xFF; 
	DeviceData[2] = 0xFF; 
 
	Success = usb_control_msg(g_hSysDriver,
                                   USB_DIR_OUT | USB_TYPE_VENDOR | USB_RECIP_DEVICE,  VND_APOGEE_GET_IMAGE,
                                   (ImageSize >> 16) & 0xFFFF, 
				   (ImageSize & 0xFFFF), 
				   (char *)&DeviceData,
				   3, 10000);
							    
	if ( !Success ) 
	{ 
		return APN_USB_ERR_START_CI; 
	} 
 
	return APN_USB_SUCCESS; 
} 
 

APN_USB_TYPE ApnUsbStopExp(   bool DigitizeData )
{
	BOOLEAN Success;
	unsigned short	BytesReceived;


//	if ( (g_hSysDriver) == 0)
//	{
//		return APN_USB_ERR_OPEN;
//	}
      

	if ( DigitizeData == false )
	{
		  Success = usb_control_msg(g_hSysDriver,
                               USB_DIR_OUT | USB_TYPE_VENDOR | USB_RECIP_DEVICE,  VND_APOGEE_STOP_IMAGE,
                                            0, 0,(char *)&BytesReceived, 2, 10000);
    
//		if ( !Success )
//		{
//			return APN_USB_ERR_STOP_EXP;
//		}
	}

	return APN_USB_SUCCESS;
}
 
APN_USB_TYPE ApnUsbStopCI( unsigned short PostStopCount ) 
{ 
	BOOLEAN Success; 
	ULONG	BytesReceived; 
	ULONG	DownloadCountPostStop; 
 
 
//	if ( (g_hSysDriver) == 0)
//	{
//		return APN_USB_ERR_OPEN;
//	}
     
    //JM: This fails to compile on x86_64 platforms
	//DownloadCountPostStop = (unsigned int)&PostStopCount;
	//JM: Temporary solution:
	DownloadCountPostStop = (unsigned int) PostStopCount;
	
	Success = usb_control_msg(g_hSysDriver,
                               USB_DIR_OUT | USB_TYPE_VENDOR | USB_RECIP_DEVICE,  VND_APOGEE_STOP_IMAGE,
                                 DownloadCountPostStop,
				 1,
		 		 0, 0, 10000);
 
	if ( !Success ) 
	{ 
		return APN_USB_ERR_STOP_CI; 
	} 
 
	return APN_USB_SUCCESS; 
} 
 

APN_USB_TYPE ApnUsbGetImage(  
                                                         unsigned long ImgSizeBytes,
unsigned short *pMem )
{
	BOOLEAN Success;
	ULONG	ImageBytesRemaining;
	ULONG	ReceivedSize;
 
        PUCHAR  pRequestData;
                                                                           
                                                                           
     

	Success = 1;
//	if ( (g_hSysDriver) == 0 )
//	{
//		return APN_USB_ERR_OPEN;
//	}

 
	ImageBytesRemaining = ImgSizeBytes;

                                                                           

	////////////////////////
	ULONG LoopCount = ImgSizeBytes / IMAGE_BUFFER_SIZE;
	ULONG Remainder	= ImgSizeBytes - ( LoopCount * IMAGE_BUFFER_SIZE );
	ULONG MemIterator = IMAGE_BUFFER_SIZE / 2;
	ULONG Counter;
        ULONG obuffersize,swabbuffersize;
        ULONG TotalPixels = 0;
                                                                  
                                                                           
        Success = 1;
                                                                           
                                                                         
	for ( Counter=0; Counter<LoopCount; Counter++ )
	{
                                                                       
		ReceivedSize = usb_bulk_read(g_hSysDriver, 0x86, 
				 (char *)pMem, IMAGE_BUFFER_SIZE, 10000);

//        printf("DRIVER: bulkread size=%x\n",ReceivedSize);

		TotalPixels += (ReceivedSize / 2); 
 
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
		ReceivedSize = usb_bulk_read(g_hSysDriver, 0x86, 
				(char *)pMem, Remainder, 10000);
        printf("DRIVER: bulkread2 size=%x\n",ReceivedSize);

//		if ( ReceivedSize != Remainder )
//			Success = 0;
	}

	printf("\n");

	if ( !Success )
		return APN_USB_ERR_IMAGE_DOWNLOAD;

	return APN_USB_SUCCESS;		// Success
}


APN_USB_TYPE ApnUsbReset(  )
{
	BOOLEAN Success;
	unsigned short	BytesReceived;

//	if ( (g_hSysDriver) == 0)
//	{
//		return APN_USB_ERR_OPEN;
//	}
     

	Success = usb_control_msg(g_hSysDriver,
                                   USB_DIR_OUT | USB_TYPE_VENDOR | USB_RECIP_DEVICE,  VND_APOGEE_SOFT_RESET,
                                   0, 0, (char *)&BytesReceived, 2, 10000);
//        printf("DRIVER: reset s=%x\n",Success);
   
	if ( !Success )
	{
		return APN_USB_ERR_RESET;
	}

	return APN_USB_SUCCESS;
}





APN_USB_TYPE ApnUsbSerialReadSettings(
								unsigned short SerialId,
									   unsigned char  *Settings )
{
	BOOLEAN	Success;
	DWORD	BytesReceived;
	UCHAR	Port;
#ifdef OSX
        char localSettings[APN_USB_SERIAL_SETTINGS_BYTE_COUNT+1];
#endif	

	// Require a valid handle to the device
//	if ( (*hDevice == INVALID_HANDLE_VALUE) || (*hDevice == NULL) )
//	{
//		return APN_USB_ERR_SERIAL_OPEN;
//	}

	// Require a valid serial port selection
	if ( (SerialId != 0) && (SerialId != 1) )
	{
		return APN_USB_ERR_SERIAL_ID;
	}

	Port = (UCHAR)SerialId;

	// Issue the IOCTL to the USB device

#ifdef OSX
        Success = usb_control_msg((struct usb_dev_handle *)g_hSysDriver, 
                                USB_DIR_IN | USB_TYPE_VENDOR | USB_RECIP_DEVICE, VND_APOGEE_SET_SERIAL,
                                            Port, Port,(char *)&localSettings, APN_USB_SERIAL_SETTINGS_BYTE_COUNT, 10000);
        swab(&localSettings,Settings,APN_USB_SERIAL_SETTINGS_BYTE_COUNT+1);
#else
        Success = usb_control_msg((struct usb_dev_handle *)g_hSysDriver, 
                                USB_DIR_IN | USB_TYPE_VENDOR | USB_RECIP_DEVICE, VND_APOGEE_SET_SERIAL,
                                            Port, Port,(char *)Settings, APN_USB_SERIAL_SETTINGS_BYTE_COUNT, 10000);
#endif        

	if ( (!Success) || (BytesReceived != APN_USB_SERIAL_SETTINGS_BYTE_COUNT) )
		return APN_USB_ERR_SERIAL_READ_SETTINGS;

	return APN_USB_SUCCESS;
}


APN_USB_TYPE ApnUsbSerialWriteSettings(
										unsigned short SerialId,
										unsigned char  *Settings )
{
	BOOLEAN			Success;
	DWORD			BytesReceived;
	unsigned char	OutBuffer[1+APN_USB_SERIAL_SETTINGS_BYTE_COUNT];
	char *cbuf;

	// Require a valid handle to the device
//	if ( (*hDevice == INVALID_HANDLE_VALUE) || (*hDevice == NULL) )
//	{
//		return APN_USB_ERR_SERIAL_OPEN;
//	}

	// Require a valid serial port selection
	if ( (SerialId != 0) && (SerialId != 1) )
	{
		return APN_USB_ERR_SERIAL_ID;
	}

	OutBuffer[0] = (BYTE)SerialId;
	memcpy( &OutBuffer[1], Settings, APN_USB_SERIAL_SETTINGS_BYTE_COUNT );
	
        cbuf = (char *)&OutBuffer;
        Success = usb_control_msg((struct usb_dev_handle *)g_hSysDriver, 
                                  USB_DIR_OUT | USB_TYPE_VENDOR | USB_RECIP_DEVICE,  VND_APOGEE_SET_SERIAL,
                                   0, 0, cbuf, APN_USB_SERIAL_SETTINGS_BYTE_COUNT,10000);
	if ( !Success ) 
		return APN_USB_ERR_SERIAL_WRITE_SETTINGS;

	return APN_USB_SUCCESS;
}


APN_USB_TYPE ApnUsbSerialReadBaudRate(
									   unsigned short SerialId,
									   unsigned long  *BaudRate )
{
	unsigned char SettingsBuffer[APN_USB_SERIAL_SETTINGS_BYTE_COUNT];	


	// Require a valid handle to the device
//	if ( (*hDevice == INVALID_HANDLE_VALUE) || (*hDevice == NULL) )
//	{
//		return APN_USB_ERR_SERIAL_OPEN;
//	}

	// Require a valid serial port selection
	if ( (SerialId != 0) && (SerialId != 1) )
	{
		return APN_USB_ERR_SERIAL_ID;
	}

	// Read the current settings for the serial port
	if ( ApnUsbSerialReadSettings(SerialId, SettingsBuffer) != APN_USB_SUCCESS )
	{
		return APN_USB_ERR_SERIAL_READ_SETTINGS;
		
		*BaudRate = 0;
	}
	
	*BaudRate = ((unsigned long *)SettingsBuffer)[0];
	return APN_USB_SUCCESS;
}


APN_USB_TYPE ApnUsbSerialWriteBaudRate(
									    unsigned short SerialId,
										unsigned long  BaudRate )
{
	unsigned char SettingsBuffer[APN_USB_SERIAL_SETTINGS_BYTE_COUNT];	


	// Require a valid handle to the device
//	if ( (*hDevice == INVALID_HANDLE_VALUE) || (*hDevice == NULL) )
//	{
//		return APN_USB_ERR_SERIAL_OPEN;
//	}

	// Require a valid serial port selection
	if ( (SerialId != 0) && (SerialId != 1) )
	{
		return APN_USB_ERR_SERIAL_ID;
	}

	// Read the current settings for the serial port
	if ( ApnUsbSerialReadSettings(SerialId, SettingsBuffer) != APN_USB_SUCCESS )
	{
		return APN_USB_ERR_SERIAL_READ_SETTINGS;
	}

	// Modify our baud rate variable
	((unsigned long *)SettingsBuffer)[0] = BaudRate;
	
	// Write the new settings for the serial port
	if ( ApnUsbSerialWriteSettings(SerialId, SettingsBuffer) != APN_USB_SUCCESS )
	{
		return APN_USB_ERR_SERIAL_WRITE_SETTINGS;
	}

	return APN_USB_SUCCESS;
}


APN_USB_TYPE ApnUsbSerialReadFlowControl(
										  unsigned short SerialId,
								 		  bool			 *FlowControl )
{
	unsigned char SettingsBuffer[APN_USB_SERIAL_SETTINGS_BYTE_COUNT];	


	// Require a valid handle to the device
//	if ( (*hDevice == INVALID_HANDLE_VALUE) || (*hDevice == NULL) )
//	{
//		return APN_USB_ERR_SERIAL_OPEN;
//	}

	// Require a valid serial port selection
	if ( (SerialId != 0) && (SerialId != 1) )
	{
		return APN_USB_ERR_SERIAL_ID;
	}
	
	// Read the current settings for the serial port
	if ( ApnUsbSerialReadSettings(SerialId, SettingsBuffer) != APN_USB_SUCCESS )
	{
		return APN_USB_ERR_SERIAL_READ_SETTINGS;
	}

	if ( (SettingsBuffer[APN_USB_SERIAL_SETTINGS_CTRL_INDEX] & APN_USB_SERIAL_BIT_FLOW_CONTROL) == 0 )
		*FlowControl = false;
	else
		*FlowControl = true;

	return APN_USB_SUCCESS;
}


APN_USB_TYPE ApnUsbSerialWriteFlowControl( 

										   unsigned short SerialId,
								 		   bool			  FlowControl )
{
	unsigned char SettingsBuffer[APN_USB_SERIAL_SETTINGS_BYTE_COUNT];	


	// Require a valid handle to the device
//	if ( (*hDevice == INVALID_HANDLE_VALUE) || (*hDevice == NULL) )
//	{
//		return APN_USB_ERR_SERIAL_OPEN;
//	}

	// Require a valid serial port selection
	if ( (SerialId != 0) && (SerialId != 1) )
	{
		return APN_USB_ERR_SERIAL_ID;
	}

	// Read the current settings for the serial port
	if ( ApnUsbSerialReadSettings(SerialId, SettingsBuffer) != APN_USB_SUCCESS )
	{
		return APN_USB_ERR_SERIAL_READ_SETTINGS;
	}

	// Modify the flow control bit
	if ( FlowControl )
		SettingsBuffer[APN_USB_SERIAL_SETTINGS_CTRL_INDEX] |= APN_USB_SERIAL_BIT_FLOW_CONTROL;
	else
		SettingsBuffer[APN_USB_SERIAL_SETTINGS_CTRL_INDEX] &= ~APN_USB_SERIAL_BIT_FLOW_CONTROL;

	// Write the new settings for the serial port
	if ( ApnUsbSerialWriteSettings(SerialId, SettingsBuffer) != APN_USB_SUCCESS )
	{
		return APN_USB_ERR_SERIAL_WRITE_SETTINGS;
	}

	return APN_USB_SUCCESS;
}


APN_USB_TYPE ApnUsbSerialReadParity( 

									 unsigned short SerialId,
									 ApnUsbParity   *Parity )
{
	unsigned char	SettingsBuffer[APN_USB_SERIAL_SETTINGS_BYTE_COUNT];	
	bool			bParityEnable;
	bool			bParityOdd;


	// Require a valid handle to the device
//	if ( (*hDevice == INVALID_HANDLE_VALUE) || (*hDevice == NULL) )
//	{
//		return APN_USB_ERR_SERIAL_OPEN;
//	}

	// Require a valid serial port selection
	if ( (SerialId != 0) && (SerialId != 1) )
	{
		return APN_USB_ERR_SERIAL_ID;
	}

	// Read the current settings for the serial port
	if ( ApnUsbSerialReadSettings(SerialId, SettingsBuffer) != APN_USB_SUCCESS )
	{
		return APN_USB_ERR_SERIAL_READ_SETTINGS;
	}

	if ( (SettingsBuffer[APN_USB_SERIAL_SETTINGS_CTRL_INDEX] & APN_USB_SERIAL_BIT_PARITY_ENABLE) == 0 )
		bParityEnable = false;
	else
		bParityEnable = true;

	if ( (SettingsBuffer[APN_USB_SERIAL_SETTINGS_CTRL_INDEX] & APN_USB_SERIAL_BIT_PARITY_ODD) == 0 )
		bParityOdd = false;
	else
		bParityOdd = true;

	if ( !bParityEnable )
	{
		*Parity = ApnUsbParity_None;
	}
	else
	{
		if ( bParityOdd )
			*Parity = ApnUsbParity_Odd;
		else
			*Parity = ApnUsbParity_Even;
	}

	return APN_USB_SUCCESS;
}


APN_USB_TYPE ApnUsbSerialWriteParity( 

									  unsigned short SerialId,
									  ApnUsbParity   Parity )
{
	unsigned char SettingsBuffer[APN_USB_SERIAL_SETTINGS_BYTE_COUNT];	


	// Require a valid handle to the device
//	if ( (*hDevice == INVALID_HANDLE_VALUE) || (*hDevice == NULL) )
//	{
//		return APN_USB_ERR_SERIAL_OPEN;
//	}

	// Require a valid serial port selection
	if ( (SerialId != 0) && (SerialId != 1) )
	{
		return APN_USB_ERR_SERIAL_ID;
	}

	// Read the current settings for the serial port
	if ( ApnUsbSerialReadSettings(SerialId, SettingsBuffer) != APN_USB_SUCCESS )
	{
		return APN_USB_ERR_SERIAL_READ_SETTINGS;
	}

	if ( Parity == ApnUsbParity_None )
	{
		SettingsBuffer[APN_USB_SERIAL_SETTINGS_CTRL_INDEX] &= ~APN_USB_SERIAL_BIT_PARITY_ENABLE;
	}
	else
	{
		SettingsBuffer[APN_USB_SERIAL_SETTINGS_CTRL_INDEX] |= APN_USB_SERIAL_BIT_PARITY_ENABLE;

		if ( Parity == ApnUsbParity_Odd )
			SettingsBuffer[APN_USB_SERIAL_SETTINGS_CTRL_INDEX] |= APN_USB_SERIAL_BIT_PARITY_ODD;
		else
			SettingsBuffer[APN_USB_SERIAL_SETTINGS_CTRL_INDEX] &= ~APN_USB_SERIAL_BIT_PARITY_ODD;
	}

	// Write the new settings for the serial port
	if ( ApnUsbSerialWriteSettings(SerialId, SettingsBuffer) != APN_USB_SUCCESS )
	{
		return APN_USB_ERR_SERIAL_WRITE_SETTINGS;
	}

	return APN_USB_SUCCESS;
}


APN_USB_TYPE ApnUsbSerialRead( 

							   unsigned short SerialId,
							   char			  *ReadBuffer,
							   unsigned short *BufferCount )
{
	BOOLEAN	Success;
	DWORD	BytesReceived;
	BYTE	PortNumber;
	char	TempBuffer[64];
	DWORD	TempBufferSize;


	// Require a valid handle to the device
//	if ( (*hDevice == INVALID_HANDLE_VALUE) || (*hDevice == NULL) )
//	{
//		return APN_USB_ERR_SERIAL_OPEN;
//	}

	// Require a valid serial port selection
	if ( (SerialId != 0) && (SerialId != 1) )
	{
		return APN_USB_ERR_SERIAL_ID;
	}

	PortNumber = (BYTE)SerialId;

	TempBufferSize = 64;

	// Issue the IOCTL to the USB device
        Success = usb_control_msg((struct usb_dev_handle *)g_hSysDriver, 
                                USB_DIR_IN | USB_TYPE_VENDOR | USB_RECIP_DEVICE, VND_APOGEE_SERIAL,
                                            PortNumber, PortNumber,(char *)&TempBuffer, TempBufferSize, 10000);
	if ( !Success )
		return APN_USB_ERR_SERIAL_READ_PORT;

	TempBuffer[BytesReceived] = '\0';
#ifdef OSX
        swab(&TempBuffer,ReadBuffer,((BytesReceived+1)/2)*2);
#else
	strcpy( ReadBuffer, TempBuffer );
#endif
	*BufferCount = (unsigned short)BytesReceived;

	return APN_USB_SUCCESS;
}


APN_USB_TYPE ApnUsbSerialWrite(

							    unsigned short SerialId,
							    char		   *WriteBuffer,
								unsigned short BufferCount )
{
	BOOLEAN	Success;
	DWORD	BytesReceived;
	DWORD	DeviceBufferByteCount;
	PUCHAR	DeviceBuffer;
	char *cbuf;

	// Require a valid handle to the device
//	if ( (*hDevice == INVALID_HANDLE_VALUE) || (*hDevice == NULL) )
//	{
//		return APN_USB_ERR_SERIAL_OPEN;
//	}

	// Require a valid serial port selection
	if ( (SerialId != 0) && (SerialId != 1) )
	{
		return APN_USB_ERR_SERIAL_ID;
	}
	
	DeviceBufferByteCount = BufferCount + 1;
	DeviceBuffer = new UCHAR[DeviceBufferByteCount];

	DeviceBuffer[0] = (UCHAR)SerialId;
	memcpy( &(DeviceBuffer[1]), WriteBuffer, BufferCount );

	// Issue the IOCTL to the USB device
        cbuf = (char *)&DeviceBuffer;
        Success = usb_control_msg((struct usb_dev_handle *)g_hSysDriver, 
                                  USB_DIR_OUT | USB_TYPE_VENDOR | USB_RECIP_DEVICE,  VND_APOGEE_SERIAL,
                                   0, 0, cbuf, DeviceBufferByteCount, 10000);

	delete [] DeviceBuffer;

	if ( !Success )
		return APN_USB_ERR_SERIAL_READ_PORT;

	return APN_USB_SUCCESS;
}


APN_USB_TYPE ApnUsbReadVendorInfo( 

								   unsigned short *VendorId,
			 					   unsigned short *ProductId,
								   unsigned short *DeviceId )
{


	*VendorId	= (unsigned short)USB_ALTA_VENDOR_ID;
	*ProductId	= (unsigned short)USB_ALTA_PRODUCT_ID;
	*DeviceId	= (unsigned short)firmwareRevision;

	return APN_USB_SUCCESS;
}


APN_USB_TYPE ApnUsbSysDriverVersion( 
 double *VersionNumber )
{
	
	*VersionNumber = (double)(APUSB_VERSION_MAJOR + 
				APUSB_VERSION_MINOR/10.);

	return APN_USB_SUCCESS;
}



 
APN_USB_TYPE ApnUsbReadCustomSerialNumber( 
										   char				*SerialNumber, 
										   unsigned short	*SerialNumberLength ) 
{ 
	BOOLEAN	Success; 
	char	pBuffer[APN_USB_SN_BYTE_COUNT+1]; 
	DWORD	BytesReceived; 
	DWORD	Length; 
 
 
	/* 
	UsbRequest.Request		= VND_APOGEE_EEPROM; 
	UsbRequest.Direction		= REQUEST_IN; 
	UsbRequest.Index		= APN_USB_SN_EEPROM_ADDR; 
	UsbRequest.Value		= (unsigned short)( (APN_USB_SN_EEPROM_BANK<<8) | APN_USB_SN_EEPROM_CHIP ); 
	*/ 
    
 
        if ((idProduct == APUSB_PID_ALTA) && (firmwareRevision >= APUSB_CUSTOM_SN_DID_SUPPORT)) {

		Length = APN_USB_SN_BYTE_COUNT + 1; 
   		Success = usb_control_msg((struct usb_dev_handle *)g_hSysDriver, 
                                USB_DIR_IN | USB_TYPE_VENDOR | USB_RECIP_DEVICE, VND_APOGEE_CAMCON_REG,
                                            1, 0,(char *)&pBuffer,APUSB_CUSTOM_SN_BYTE_COUNT, 10000);

 
		if ( !Success ) 
		{ 
			strncpy( SerialNumber, pBuffer, APUSB_CUSTOM_SN_BYTE_COUNT ); 
 
			*SerialNumberLength = 0; 
 
			return APN_USB_ERR_CUSTOM_SN_READ; 
		} 
 
		pBuffer[APN_USB_SN_BYTE_COUNT] = '\0'; 
 
		strcpy( SerialNumber, strtok( pBuffer, "" ) ); 
 
		*SerialNumberLength = strlen( SerialNumber ); 

	} 
	return APN_USB_SUCCESS; 
} 
 
 
APN_USB_TYPE ApnUsbRead8051FirmwareRevision(
											 char		*Revision ) 
{ 
	BOOLEAN	Success; 
	char	pBuffer[APN_USB_8051_REV_BYTE_COUNT+1]; 
	DWORD	BytesReceived; 
	DWORD	Length; 
    
 
	if ( (idProduct == APUSB_PID_ALTA) && (firmwareRevision >=  APUSB_8051_REV_DID_SUPPORT)) {

		Length = APN_USB_8051_REV_BYTE_COUNT+1; 
   	        Success = usb_control_msg((struct usb_dev_handle *)g_hSysDriver, 
                                USB_DIR_IN | USB_TYPE_VENDOR | USB_RECIP_DEVICE, VND_APOGEE_CAMCON_REG,
                                            2, 0,(char *)&pBuffer, APUSB_8051_REV_BYTE_COUNT, 10000);
 
		if ( !Success ) 
		{ 
			Revision[0] = 'F'; 
			Revision[1] = 'F'; 
			Revision[2] = 'F'; 
 
			Revision[APN_USB_8051_REV_BYTE_COUNT] = '\0'; 
 	
			return APN_USB_ERR_8051_REV_READ; 
		} 
 
		Revision[APN_USB_8051_REV_BYTE_COUNT] = '\0'; 
 	} else {
			Revision[0] = 0x0; 
			Revision[1] = 0x0; 
			Revision[2] = 0x0; 
	}


	return APN_USB_SUCCESS; 
} 
 


APN_USB_TYPE ApnUsbConfigureDataPort( unsigned short	Assignment )
{
	bool			Success;
	unsigned short	TempValue;

	TempValue = Assignment;

	Success = ApnUsbCreateRequest( VND_APOGEE_DATA_PORT,
								   false,
								   0,
								   TempValue,
								   0,
								   NULL );

	return APN_USB_SUCCESS;
}


APN_USB_TYPE ApnUsbReadDataPort( unsigned short *DataValue )
{
	bool			Success;
	unsigned short	TempValue;
	
	Success = ApnUsbCreateRequest( VND_APOGEE_DATA_PORT,
								   true,
								   0,
								   2,
								   2,
								   (unsigned char *)&TempValue );

	*DataValue = TempValue;

	return APN_USB_SUCCESS;
}


APN_USB_TYPE ApnUsbWriteDataPort( unsigned short	DataValue )
{
	bool			Success;
	unsigned short	TempValue;

	TempValue = DataValue;

	Success = ApnUsbCreateRequest( VND_APOGEE_DATA_PORT,
								   false,
								   0,
								   2,
								   2,
								   (unsigned char *)&TempValue );

	return APN_USB_SUCCESS;
}


APN_USB_TYPE ApnUsbConfigureControlPort( unsigned short	Assignment )
{
	bool			Success;
	unsigned short	TempValue;

	TempValue = Assignment;

	Success = ApnUsbCreateRequest( VND_APOGEE_CONTROL_PORT,
								   false,
								   0,
								   TempValue,
								   0,
								   NULL );

	return APN_USB_SUCCESS;
}


APN_USB_TYPE ApnUsbReadControlPort( unsigned char	*ControlValue,
									unsigned char	*OptionPinValue )
{
	bool			Success;
	unsigned short	TempValue;
	
	Success = ApnUsbCreateRequest( VND_APOGEE_CONTROL_PORT,
								   true,
								   0,
								   2,
								   2,
								   (unsigned char *)&TempValue );

	*ControlValue	= TempValue & 0x00FF;
	*OptionPinValue	= (TempValue & 0xFF00) >> 8;

	return APN_USB_SUCCESS;
}


APN_USB_TYPE ApnUsbWriteControlPort( unsigned char	ControlValue,
									 unsigned char	OptionPinValue )
{
	bool			Success;
	unsigned short	TempValue;

	TempValue = 0x0;

	TempValue = OptionPinValue << 8;
	TempValue |= ControlValue;

	Success = ApnUsbCreateRequest( VND_APOGEE_CONTROL_PORT,
								   false,
								   0,
								   2,
								   2,
								   (unsigned char *)&TempValue );

	return APN_USB_SUCCESS;
}












