/*****************************************************************************************
NAME
 ...

DESCRIPTION
 ...

COPYRIGHT (C)
 QSI (Quantum Scientific Imaging) 2005-2006

REVISION HISTORY
 MWB 04.28.06 Original Version
 *****************************************************************************************/

#pragma once

#include <cstdarg>
#include <algorithm>
#include <iterator>
#include <vector>
#include <string>
#include "QSI_Global.h"
#include "QSI_PacketWrapper.h"
#include "QSI_Registry.h"
#include "QSILog.h"
#include "HotPixelMap.h"
#include "FilterWheel.h"
#include "QSIFeatures.h"

/*****************************************************************************************
NAME
 Interface

DESCRIPTION
 Camera Interface

COPYRIGHT (C)
 QSI (Quantum Scientific Imaging) 2005-2006

REVISION HISTORY
 MWB 04.28.06 Original Version
 *****************************************************************************************/

#include <unistd.h>

#define INTERFACERETRYMS 2500
// AltMode1 bits
#define EXPOSUREPULSEOUTBIT 0x01
#define MANUALSHUTTERMODE 0x02
#define HOSTTIMEDEXPOSURE 0x04
#define MANUALSHUTTEROPEN 0x10
#define MANUALSHUTTERCLOSE 0x20

//****************************************************************************************
// MF CAMERA INTERFACE CLASS
//****************************************************************************************

class QSI_Interface
{

public:

	////////////////////////////////////////////////////////////////////////////////////////
	// Constructor / Destructor
	QSI_Interface( void );
	~QSI_Interface( void );

	////////////////////////////////////////////////////////////////////////////////////////
	// ...
	void Initialize( void );
	int CountDevices( void );
	int GetDeviceInfo( int iIndex, CameraID & cID );
	int ListDevices( std::vector <CameraID> & vQSIID, int & iNumFound );
	int OpenCamera( std::string acSerialNumber );
	int CloseCamera( void );
	int ReadImage( PVOID pvRxBuffer, int iBytesToRead );
	int CMD_InitCamera( void );
	int CMD_GetDeviceDetails( QSI_DeviceDetails & DeviceDetails );
	int CMD_StartExposure( QSI_ExposureSettings ExposureSettings );
	int CMD_AbortExposure( void );
	int CMD_TransferImage( void );
	int CMD_GetAutoZero( QSI_AutoZeroData & AutoZeroData );
	int CMD_GetDeviceState( int & iCameraState, bool & bShutterOpen, bool & bFilterState );
	int CMD_SetTemperature( bool bCoolerOn, bool bGoToAmbient, double dSetPoint );
	int CMD_GetTemperature( int & iCoolerState, double & dCoolerTemp, double & dTempAmbient, USHORT & usCoolerPower );
	int CMD_GetTemperatureEx( int & iCoolerState, double & dCoolerTemp, double & dHotsideTemp, USHORT & usCoolerPower, double & PCBTemp, bool bProbe );
	int CMD_ActivateRelay( int iXRelay, int iYRelay );
	int CMD_IsRelayDone( bool & bGuiderRelayState );
	int CMD_SetFilterWheel( int iFilterPosition );
	int CMD_GetCamDefaultAdvDetails( QSI_AdvSettings & AdvDefaultSettings, QSI_AdvEnabledOptions & AdvEnabledOptions, QSI_DeviceDetails DeviceDetails  );
	int CMD_GetAdvDefaultSettings( QSI_AdvSettings & AdvSettingsDefaults, QSI_DeviceDetails DeviceDetails  );
	int CMD_SendAdvSettings( QSI_AdvSettings AdvSettings );
	int CMD_GetSetPoint( double & dSetPoint);
	int CMD_SetShutter( bool bOpen );
	int CMD_AbortRelays( void );
	int CMD_GetLastExposure( double & iExposure );
	int CMD_CanAbortExposure( bool & bCanAbort );
	int CMD_CanStopExposure( bool & bCanStop );
	int CMD_GetFilterPosition( int & iPosition );
	int CMD_GetCCDSpecs( int & iMaxADU, double & dEPerADUHigh, double & dEPerADULow, double & dEFullWell, double & dMinExp, double & dMaxExp);
	int CMD_GetAltMode1(unsigned char & mode);
	int CMD_SetAltMode1(unsigned char mode);
	int CMD_GetEEPROM(USHORT usAddress, BYTE & data);
	int CMD_SetFilterTrim(int Position, bool probe = false);
	int CMD_BurstBlock(int Count, BYTE * Buffer, int * Status);
	int CMD_GetFeatures(BYTE* pMem, int iFeatureArraySize, int & iCountReturned);
	int CMD_HSRExposure( QSI_ExposureSettings ExposureSettings, QSI_AutoZeroData& AutoZeroData);
	int CMD_SetHSRMode( bool enable);
	int CMD_StartExposureEx( QSI_ExposureSettings ExposureSettings );
	bool HasFilterWheelTrim(void);
	int GetVersionInfo(char tszHWVersion[], char tszFWVersion[]);
	void LogWrite(int iLevel, const char * msg, ...);
	// Old Autozero
	// ...Deleted...
	// End old autozero
	// New autozero
	void GetAutoZeroAdjustment(QSI_AutoZeroData autoZeroData, USHORT * zeroPixels, USHORT * usLastMean, int * usAdjust, double *  dAdjust);
	int AdjustZero(USHORT* pSrc, USHORT * pDst, int iRowLen, int iRowsLeft, int    usAdjust, bool bAdjust);
	int AdjustZero(USHORT* pSrc, double * pDst, int iRowLen, int iRowsLeft, double dAdjust,  bool bAdjust);
	int AdjustZero(USHORT* pSrc, long   * pDst, int iRowLen, int iRowsLeft, int    usAdjust, bool bAdjust);
	// End new Autozero
	int HasFastExposure( bool & bFast );
	int QSIRead( unsigned char * Buffer, int BytesToRead, int * BytesReturned);
	int QSIWrite( unsigned char * Buffer, int BytesToWrite, int * BytesWritten);
	int QSIReadDataAvailable(int * count);
	int QSIWriteDataPending(int * count);
	int QSIReadTimeout(int timeout);
	int QSIWriteTimeout(int timeout);
	//
	void HotPixelRemap(	BYTE * Image, int RowPad, QSI_ExposureSettings Exposure,
							QSI_DeviceDetails Details, USHORT ZeroPixel);

	int CMD_ExtTrigMode( BYTE action, BYTE polarity);
	//////////////////////////////////////////////////////////////////////////////////////////
	// Member Variables
	//
	bool m_bColorProfiling;
	bool m_bTestBayerImage;
	bool m_bCameraStateCacheInvalid;
	//
	bool m_bAutoZeroEnable;
	DWORD m_dwAutoZeroSatThreshold;
	DWORD m_dwAutoZeroMaxADU;
	//
	DWORD m_dwAutoZeroSkipStartPixels;
	DWORD m_dwAutoZeroSkipEndPixels;
	bool m_bAutoZeroMedianNotMean;
	//
	HotPixelMap m_hpmMap;
	QSILog *m_log;				// Log interface transactions
	
private:
	////////////////////////////////////////////////////////////////////////////////////////
	// Private variables
	bool GetBoolean(UCHAR);
	USHORT Get2Bytes(PVOID);
	UINT Get3Bytes(PVOID);
	void GetString(PVOID, PVOID,int);
	void PutBool(PVOID, bool);
	void Put2Bytes(PVOID, USHORT);
	void Put3Bytes(PVOID, uint32_t);
	
	int m_iError; // Holds errors; declared here so it won't have to be declared every function
	QSI_PacketWrapper PacketWrapper;
	QSI_USBTimeouts 	USBTimeouts;
	UCHAR   Cmd_Pkt[MAX_PKT_LENGTH];
	UCHAR   Rsp_Pkt[MAX_PKT_LENGTH];
	QSI_DeviceDetails m_DeviceDetails;
	
	FilterWheel	m_fwWheel;

	bool m_bHighGainOverride;
	bool m_bLowGainOverride;
	double m_dHighGainOverride;
	double m_dLowGainOverride;

	// Commands sensed when Open is called.
	bool m_bHasCMD_GetTemperatureEx;
	bool m_bHasCMD_StartExposureEx;
	bool m_bHasCMD_SetFilterTrim;
	bool m_bHasCMD_GetFeatures;

	QSIFeatures m_Features;

	// Define Camera byte commands
	static const BYTE CMD_STARTBOOTLOADER		= 0x21;
	static const BYTE CMD_FORCEBOOTLOAFER		= 0x22;
	static const BYTE CMD_GETDEVICEDETAILS		= 0x41;
	static const BYTE CMD_GETDEVICESTATE		= 0x42;
	static const BYTE CMD_STARTEXPOSURE			= 0x43;
	static const BYTE CMD_ABORTEXPOSURE			= 0x44;
	static const BYTE CMD_TRANSFERIMAGE			= 0x45;
	static const BYTE CMD_SETTEMPERATURE		= 0x46;
	static const BYTE CMD_GETTEMPERATURE		= 0x47;
	static const BYTE CMD_ACTIVATERELAY			= 0x48;
	static const BYTE CMD_ISRELAYDONE			= 0x49;
	static const BYTE CMD_SETFILTERWHEEL		= 0x4A;
	static const BYTE CMD_INIT					= 0x4B;
	static const BYTE CMD_GETDEFAULTADVDETAILS	= 0x4C;
	static const BYTE CMD_SETADVSETTINGS		= 0x4D;
	static const BYTE CMD_GETAUTOZERO			= 0x4E;
	static const BYTE CMD_SETALTMODE1			= 0x4F;
	static const BYTE CMD_GETALTMODE1			= 0x50;
	static const BYTE CMD_GETSETPOINT			= 0x51;
	static const BYTE CMD_SETSHUTTER			= 0x52;
	static const BYTE CMD_ABORTRELAYS			= 0x53;
	static const BYTE CMD_GETLASTEXPOSURETIME	= 0x54;
	static const BYTE CMD_CANABORTEXPOSURE		= 0x55;
	static const BYTE CMD_CANSTOPEXPOSURE		= 0x56;
	static const BYTE CMD_GETFILTERPOSITION		= 0x57;
	static const BYTE CMD_GETCCDSPECS			= 0x58;
	static const BYTE CMD_STARTEXPOSUREEX		= 0x59;
	static const BYTE CMD_SETFILTERTRIM			= 0x5A;
	static const BYTE CMD_GETTEMPERATUREEX		= 0x5B;
	static const BYTE CMD_GETFEATURES			= 0x5C;
	static const BYTE CMD_SETHSRMODE			= 0x5E;
	static const BYTE CMD_HSREXPOSURE			= 0x5F;
	static const BYTE CMD_GETEEPROM				= 0x60;
	static const BYTE CMD_SETEEPROM				= 0x61;
	static const BYTE CMD_CAMERARESET			= 0x64;
	static const BYTE CMD_BURSTBLOCK			= 0x65;
	static const BYTE CMD_SHUTTERUNLOCK			= 0x70;
	static const BYTE CMD_BASICHWTRIGGER		= 0x71;

	// CMD_SHUTTERLOCK MODES
	static const int LOCKMODE_LOCKREQ 			= 0;
	static const int LOCKMODE_LOCKACK 			= 1;
	static const int LOCKMODE_UNLOCKREQ			= 2;
	static const int LOCKMODE_UNLOCKACK			= 3;

	// CMD_BASICHWTRIGGER MODES
	static const BYTE TRIG_DISABLE				= 0x00;
	static const BYTE TRIG_CLEARERROR			= 0x01;
	static const BYTE TRIG_TERMINATE			= 0x02;
	static const BYTE TRIG_SHORTWAIT			= 0x04;
	static const BYTE TRIG_LONGWAIT				= 0x06;
	static const BYTE TRIG_HIGHTOLOW			= 0x00;
	static const BYTE TRIG_LOWTOHIGH			= 0x01;

	BYTE m_TriggerMode;
};

int compareUSHORT( const void *val1, const void *val2);





