/*****************************************************************************************
NAME
 Global

DESCRIPTION
 Contains global constants (including error conditions) and type definitions

COPYRIGHT (C)
 QSI (Quantum Scientific Imaging) 2005-2006

REVISION HISTORY
 MWB 04.16.06 Original Version
*****************************************************************************************/

#pragma once
#include "WinTypes.h"
#include "wincompat.h"
#include "FilterWheel.h"

//Optionally include Logging code
#define LOG

//****************************************************************************************
// Constants
//****************************************************************************************

const int MAX_DEVICES = 31;       // Maximum devices supported
const int MAX_PKT_LENGTH = 128;   // Maximum packet length (in bytes)
const int READ_TIMEOUT = 5000;    // Amount of time in milliseconds
const int WRITE_TIMEOUT = 5000;   // Amount of time in milliseconds 
const int SHORT_READ_TIMEOUT = 100;
const int SHORT_WRITE_TIMEOUT = 100;

const int MAX_PIXELS_READ_PER_BLOCK = 510 * 128 / 2;
// Maximum number of pixels (not bytes) to read per block
// Limited by ftdi constraints. 62 bytes of real data per packet, 510 for ft2232H
// Max is 65536 BYTES total

// FTDI buffering size, Zero means leave as default
// Transfer size in bytes.
const int USB_IN_TRANSFER_SIZE = 64 * 1024; // Max allowed by fdti
const int USB_OUT_TRANSFER_SIZE = 64 * 1024;
const int LATENCY_TIMER_MS = 16;

const int USB_SERIAL_LENGTH = 32; // Length of character array to hold device's USB serial number
const int USB_DESCRIPTION_LENGTH = 32;
const int USB_MAX_DEVICES = 128;

const int PKT_COMMAND = 0;        // Offset to packet command byte
const int PKT_LENGTH = 1;         // Offset to packet length byte
const int PKT_HEAD_LENGTH = 2;    // Number of bytes for the packet header

const int AUTOZEROSATTHRESHOLD = 10000;
const int AUTOZEROMAXADU = 64000;
const int AUTOZEROSKIPSTARTPIXELS = 32;
const int AUTOZEROSKIPENDPIXELS = 32;

//****************************************************************************************
// TYPE DEFINITIONS
//****************************************************************************************

//////////////////////////////////////////////////////////////////////////////////////////
// ...
typedef struct QSI_DeviceDetails_t
{
	bool    HasCamera;
	bool    HasShutter;
	bool    HasFilter;
	bool    HasRelays;
	bool    HasTempReg;
	int     ArrayColumns;
	int     ArrayRows;
	double  XAspect;
	double  YAspect;
	int     MaxHBinning;
	int     MaxVBinning;
	bool    AsymBin;
	bool    TwoTimesBinning;
	USHORT  NumRowsPerBlock;    // Not currently used; calculated in "QSI_PlugIn::TransferImage" function, see "iPixelsPerRead"
	bool    ControlEachBlock;   // Not currently used; handled by "Show D/L Progress" in Advanced Dialog
	double  minExp;
	double  maxExp;
	int     MaxADU;
	double  EADUHigh;
	double  EADULow;
	double  EFull;
	int     NumFilters;
	char    ModelNumber[33];
	char    ModelName[33];
	char    SerialNumber[33];
	bool    HasFilterTrim;
	bool	HasCMD_GetTemperatureEx;
	bool	HasCMD_StartExposureEx;
	bool	HasCMD_SetFilterTrim;
	bool	HasCMD_HSRExposure;	
	bool	HasCMD_PVIMode;
	bool	HasCMD_LockCamera;
	bool	HasCMD_BasicHWTrigger;
} QSI_DeviceDetails;

//////////////////////////////////////////////////////////////////////////////////////////
// ...
typedef struct QSI_ExposureSettings_t
{
  UINT Duration;
  BYTE DurationUSec;
  int ColumnOffset;
  int RowOffset;
  int ColumnsToRead;
  int RowsToRead;
  int BinFactorX;
  int BinFactorY;
  bool OpenShutter;
  bool FastReadout;
  bool HoldShutterOpen;
  bool UseExtTrigger;
  bool StrobeShutterOutput;
  int ExpRepeatCount;
  bool ProbeForImplemented;
} QSI_ExposureSettings;

//////////////////////////////////////////////////////////////////////////////////////////
// ...
typedef struct QSI_AdvEnabledOptions_t
{
  bool LEDIndicatorOn;
  bool SoundOn;
  bool FanMode;
  bool CameraGain;
  bool ShutterPriority;
  bool AntiBlooming;
  bool PreExposureFlush;
  bool ShowDLProgress;
  bool Optimizations;
} QSI_AdvEnabledOptions;
//////////////////////////////////////////////////////////////////////////////////////////
// ...
typedef struct QSI_FilterDesc_t
{
	char	Name[32];
	long	FocusOffset;
} QSI_FilterDesc;
//////////////////////////////////////////////////////////////////////////////////////////
// ...
typedef struct QSI_AdvSettings_t
{
  bool LEDIndicatorOn;
  bool SoundOn;
  bool ShowDLProgress;
  bool OptimizeReadoutSpeed;
  int FanModeIndex;
  int CameraGainIndex;
  int ShutterPriorityIndex;
  int AntiBloomingIndex;
  int PreExposureFlushIndex;
  bool FilterTrimEnabled;
  FilterWheel fwWheel;
} QSI_AdvSettings;

typedef struct QSI_AutoZeroData_t
{
	bool zeroEnable;
	USHORT zeroLevel;
	USHORT pixelCount;
} QSI_AutoZeroData;

typedef struct QSI_USBTimeouts_t
{
	int ShortRead;
	int ShortWrite;
	int StandardRead;
	int StandardWrite;
	int ExtendedRead;
	int ExtendedWrite;
} QSI_USBTimeouts;

typedef enum QSICameraState_t
{							// Highest priority at top of list
	CCD_ERROR				= 0,	// Camera is not available
	CCD_FILTERWHEELMOVING	= 1,	// Waiting for filter wheel to finish moving
	CCD_FLUSHING			= 2,	// Flushing CCD chip or camera otherwise busy
	CCD_WAITTRIGGER			= 3,	// Waiting for an external trigger event
	CCD_DOWNLOADING			= 4,	// Downloading the image from camera hardware
	CCD_READING				= 5,	// Reading the CCD chip into camera hardware
	CCD_EXPOSING			= 6,	// Exposing dark or light frame
	CCD_IDLE				= 7,	// Camera idle
}QSICameraState;

typedef enum QSICoolerState_t
{
	COOL_OFF				= 0,	// Cooler is off
	COOL_ON					= 1,	// Cooler is on
	COOL_ATAMBIENT			= 2,	// Cooler is on and regulating at ambient temperature (optional)
	COOL_GOTOAMBIENT		= 3,	// Cooler is on and ramping to ambient
	COOL_NOCONTROL			= 4,	// Cooler cannot be controlled on this camera open loop
	COOL_INITIALIZING		= 5,	// Cooler control is initializing (optional -- displays "Please Wait")
	COOL_INCREASING			= 6,	// Cooler temperature is going up    NEW 2000/02/04
	COOL_DECREASING			= 7,	// Cooler temperature is going down  NEW 2000/02/04
	COOL_BROWNOUT			= 8,	// Cooler brownout condition  NEW 2001/09/06
}QSICoolerState;

//****************************************************************************************
// Error constants from FTDI, repeated here for reference.
//****************************************************************************************
/*
FT_STATUS (DWORD)
FT_OK = 0
FT_INVALID_HANDLE = 1
FT_DEVICE_NOT_FOUND = 2
FT_DEVICE_NOT_OPENED = 3
FT_IO_ERROR = 4
FT_INSUFFICIENT_RESOURCES = 5
FT_INVALID_PARAMETER = 6
FT_INVALID_BAUD_RATE = 7
FT_DEVICE_NOT_OPENED_FOR_ERASE = 8
FT_DEVICE_NOT_OPENED_FOR_WRITE = 9
FT_FAILED_TO_WRITE_DEVICE = 10
FT_EEPROM_READ_FAILED = 11
FT_EEPROM_WRITE_FAILED = 12
FT_EEPROM_ERASE_FAILED = 13
FT_EEPROM_NOT_PRESENT = 14
FT_EEPROM_NOT_PROGRAMMED = 15
FT_INVALID_ARGS = 16
FT_NOT_SUPPORTED = 17
FT_OTHER_ERROR = 18
*/
//////////////////////////////////////////////////////////////////////////////////////////
//
enum QSI_ReturnStates
{
	ALL_OK = 0,

	ERR_CAM_OverTemp		= 1,
	ERR_CAM_UnderTemp		= 2,
	ERR_CAM_UnderVolt		= 3,
	ERR_CAM_OverVolt		= 4,
	ERR_CAM_Filter			= 5,
	ERR_CAM_Shutter			= 6,

	ERR_USB_Load = 50,
	ERR_USB_LoadFunction = 51,

	// Packet Interface
	ERR_PKT_OpenFailed        =  200,    // Open device failed
	ERR_PKT_SetTimeOutFailed  =  300,    // Set timeouts (Rx & Tx) failed
	ERR_PKT_CloseFailed       =  400,    // Close device failed
	ERR_PKT_CheckQueuesFailed =  500,    // Check of Tx and Rx queues failed
	ERR_PKT_BothQueuesDirty   =  600,    // Both Rx and Tx queues dirty
	ERR_PKT_RxQueueDirty      =  700,    // Rx queue dirty
	ERR_PKT_TxQueueDirty      =  800,    // Tx queue dirty
	ERR_PKT_SendInitFailed    =  900,    // 
	ERR_PKT_TxPacketTooLong   = 1000,   // Length of Tx packet is greater than MAX_PKT_LENGTH
	ERR_PKT_TxFailed          = 1100,   // Write of Tx packet failed (header+data)
	ERR_PKT_TxNone            = 1200,   // None of Tx packet was sent
	ERR_PKT_TxTooLittle       = 1300,   // Not all of Tx packet data was sent
	ERR_PKT_RxHeaderFailed    = 1400,   // Read of Rx packet header failed
	ERR_PKT_RxBadHeader       = 1500,   // Tx command and Rx command did not match
	ERR_PKT_RxPacketTooLong   = 1600,   // Length of Rx packet is greater than MAX_PKT_LENGTH
	ERR_PKT_RxFailed          = 1700,   // Read of Rx packet data failed
	ERR_PKT_RxNone            = 1800,   // None of Rx packet was read
	ERR_PKT_RxTooLittle       = 1900,   // Not all of Rx packet data was received
	ERR_PKT_BlockInitFailed   = 2100,   // 
	ERR_PKT_BlockRxFailed     = 2200,   // 
	ERR_PKT_BlockRxTooLittle  = 2300,   // 
	ERR_PKT_SetLatencyFailed  = 2400,
	ERR_PKT_ResetDeviceFailed = 2500,
	ERR_PKT_SetUSBParmsFailed = 2600,

	// Device Interface
	ERR_IFC_InitCamera        =  10000,  // 
	ERR_IFC_GetDeviceDetails  =  20000,  // 
	ERR_IFC_StartExposure     =  30000,  // 
	ERR_IFC_AbortExposure     =  40000,  //
	ERR_IFC_TransferImage     =  50000,  // 
	ERR_IFC_ReadBlock         =  60000,  // 
	ERR_IFC_GetDeviceState    =  70000,  // 
	ERR_IFC_SetTemperature    =  80000,  // 
	ERR_IFC_GetTemperature    =  90000,  // 
	ERR_IFC_ActivateRelay     = 100000, // 
	ERR_IFC_IsRelayDone       = 110000, // 
	ERR_IFC_SetFilterWheel    = 120000, // 
	ERR_IFC_CameraNotOpen     = 130000, // 
	ERR_IFC_FilterNotOpen     = 140000, // 
	ERR_IFC_CameraError       = 150000, // 
	ERR_IFC_CameraHasNoFilter = 160000, // 
	ERR_IFC_FilterAlreadyOpen = 170000, // 
	ERR_IFC_Initialize        = 180000, // 
	ERR_IFC_CountDevices      = 190000, //
	ERR_IFC_ListSerial        = 200000, // 
	ERR_IFC_ListDescription   = 210000, // 
	ERR_IFC_ListMismatch      = 220000, // 
	ERR_IFC_GetDeviceInfo     = 230000, //
	ERR_IFC_AbortRelays	  	  = 240000, //
	ERR_IFC_GetLastExposure   = 250000, //
	ERR_IFC_CanAbortExposure  = 260000, //
	ERR_IFC_CanStopExposure   = 270000, //
	ERR_IFC_GetFilterPosition = 280000, //
	ERR_IFC_GetCCDSpecs	      = 290000, //
	ERR_IFC_GetAdvDetails	  = 300000,	//
	ERR_IFC_NegAutoZero	  	  = 310000,	//
	ERR_IFC_SendAdvSettings   = 320000, //
	ERR_IFC_TriggerCCDError   = 330000, //
	ERR_IFC_NotSupported	  = 340000
};



