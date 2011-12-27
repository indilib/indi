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
	int CMD_SetFilterTrim(int Position);
	bool CMD_HasFilterWheelTrim(void);
	int CMD_GetFeatures(BYTE* pMem, int iFeatureArraySize, int & iCountReturned);

	int GetVersionInfo(char tszHWVersion[], char tszFWVersion[]);

	bool GetBoolean(UCHAR);
	USHORT Get2Bytes(PVOID);
	UINT Get3Bytes(PVOID);
	void GetString(PVOID, PVOID,int);
	void PutBool(PVOID, bool);
	void Put2Bytes(PVOID, USHORT);
	void Put3Bytes(PVOID, uint32_t);
	void LogWrite(int iLevel, const char * msg, ...);
	void GetAutoZeroAdjustment(QSI_AutoZeroData autoZeroData, USHORT * zeroPixels, USHORT * usLastMean, int * usAdjust, double * dAdjust);
	int AdjustZero(BYTE* pMem, BYTE* pDst, int iRowLen, int iRowPad, int iRowsLeft, int usAdjust);
	int AdjustZero(BYTE* pMem, double * pDst, int iRowLen, int iRowsLeft, double dAdjust);
	int CMD_StartExposureEx( QSI_ExposureSettings ExposureSettings );
	int CMD_HasFastExposure( bool & bFast );
//
	int QSIRead( unsigned char * Buffer, int BytesToRead, int * BytesReturned);
	int QSIWrite( unsigned char * Buffer, int BytesToWrite, int * BytesWritten);
	int QSIReadDataAvailable(int * count);
	int QSIWriteDataPending(int * count);
	int QSIReadTimeout(int timeout);
	int QSIWriteTimeout(int timeout);
	//
	void HotPixelRemap(	BYTE * Image, int RowPad, QSI_ExposureSettings Exposure,
							QSI_DeviceDetails Details, USHORT ZeroPixel);
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
	bool m_bHasCMD_GetTemperatureEx;
	// Log USB transactions
	static const int IMAXFEATURES = 254;
	BYTE m_Features[IMAXFEATURES];
	int m_iFeaturesReturned;

};

int compareUSHORT( const void *val1, const void *val2);





