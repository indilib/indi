/////////////////////////////////////////////////////////////
//
// ApnCamera.h:  Interface file for the CApnCamera class.
//
// Copyright (c) 2003-2006 Apogee Instruments, Inc.
//
/////////////////////////////////////////////////////////////

#if !defined(AFX_APNCAMERA_H__CF513996_359F_4103_BBA6_2C730AE2C301__INCLUDED_)
#define AFX_APNCAMERA_H__CF513996_359F_4103_BBA6_2C730AE2C301__INCLUDED_
 
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "Apogee.h"

#include "Apn.h"
#include "FpgaRegs/FpgaRegs.h"


#include "ApnCamData.h"
#include "ApnCamData_KAF0261E.h"
#include "ApnCamData_KAF0401E.h"
#include "ApnCamData_KAF1001E.h"
#include "ApnCamData_KAF1001ENS.h"
#include "ApnCamData_KAF10011105.h"
#include "ApnCamData_KAF1301E.h"
#include "ApnCamData_KAF1602E.h"
#include "ApnCamData_KAF16801E.h"
#include "ApnCamData_KAF16803.h"
#include "ApnCamData_KAF3200E.h"
#include "ApnCamData_KAF6303E.h"
#include "ApnCamData_KAF09000.h"
#include "ApnCamData_KAF09000X.h"

#include "ApnCamData_KAF0261EB.h"
#include "ApnCamData_KAF0401EB.h"
#include "ApnCamData_KAF1001EB.h"
#include "ApnCamData_KAF1301EB.h"
#include "ApnCamData_KAF1602EB.h"
#include "ApnCamData_KAF3200EB.h"
#include "ApnCamData_KAF6303EB.h"

#include "ApnCamData_TH7899.h"
#include "ApnCamData_S101401107.h"
#include "ApnCamData_S101401109.h"

#include "ApnCamData_CCD3011.h"
#include "ApnCamData_CCD4240.h"
#include "ApnCamData_CCD4710.h"
#include "ApnCamData_CCD4710ALT.h"
#include "ApnCamData_CCD4720.h"
#include "ApnCamData_CCD5520.h"
#include "ApnCamData_CCD5710.h"
#include "ApnCamData_CCD7700.h"

#include "ApnCamData_CCD3011B.h"
#include "ApnCamData_CCD4240B.h"
#include "ApnCamData_CCD4710B.h"
#include "ApnCamData_CCD4720B.h"
#include "ApnCamData_CCD5520B.h"
#include "ApnCamData_CCD5710B.h"
#include "ApnCamData_CCD7700B.h"

#include "ApnCamData_KAI2001ML.h"
#include "ApnCamData_KAI2020ML.h"
#include "ApnCamData_KAI4020ML.h"
#include "ApnCamData_KAI11000ML.h"
#include "ApnCamData_KAI2001CL.h"
#include "ApnCamData_KAI2020CL.h"
#include "ApnCamData_KAI4020CL.h"
#include "ApnCamData_KAI11000CL.h"

#include "ApnCamData_KAI2020MLB.h"
#include "ApnCamData_KAI4020MLB.h"
#include "ApnCamData_KAI2020CLB.h"
#include "ApnCamData_KAI4020CLB.h"

#include "ApnCamData_ASCENT0402ME.h"

#include "ApnCamData_ASCENT0402ME2.h"
#include "ApnCamData_ASCENT0402ME3.h"
#include "ApnCamData_ASCENT0402ME4.h"

#include "ApnCamData_ASCENT340.h"
#include "ApnCamData_ASCENT2000.h"
#include "ApnCamData_ASCENT4000.h"
#include "ApnCamData_ASCENT16000.h"

#define CAPNCAMERA_SUCCESS			0
#define CAPNCAMERA_ERR_CONNECT		1
#define CAPNCAMERA_ERR_READ			2
#define CAPNCAMERA_ERR_WRITE		3
#define CAPNCAMERA_ERR_IMAGE		4
#define CAPNCAMERA_ERR_LINE			5
#define CAPNCAMERA_ERR_START_EXP	6
#define CAPNCAMERA_ERR_STOP_EXP		7
#define CAPNCAMERA_ERR_QUERY		8
#define CAPNCAMERA_ERR_SN			9


typedef struct _FEATURE_SUPPORT_LIST {
	bool bSequenceBulkDownload;
} FEATURE_SUPPORT_LIST;


class CApnCamera
{
public:
	CApnCamera();
	~CApnCamera();

	bool		GetDeviceHandle( void *hCamera, char *CameraInfo );

	bool		InitDriver( unsigned long	CamIdA, 
									unsigned short	CamIdB, 
									unsigned long	Option );

	bool		SimpleInitDriver( unsigned long		CamIdA, 
										  unsigned short	CamIdB, 
										  unsigned long		Option );

	Apn_Interface	GetCameraInterface(); 

	long		GetCameraSerialNumber( char *CameraSerialNumber, long *BufferLength );

	long		GetSystemDriverVersion( char *SystemDriverVersion, long *BufferLength );

	long		GetUsb8051FirmwareRev( char *FirmwareRev, long *BufferLength );

	long		GetUsbProductId( unsigned short *pProductId );
	long		GetUsbDeviceId( unsigned short *pDeviceId );

	bool		CloseDriver();
	long		PreStartExpose( unsigned short BitsPerPixel );
	long		PostStopExposure( bool DigitizeData );

	long		GetImageData( unsigned short *pImageData, 
									  unsigned short &Width,
									  unsigned short &Height,
									  unsigned long  &Count );

	long		GetLineData( unsigned short *pLineBuffer,
									 unsigned short &Size );		

	long		Read( unsigned short reg, unsigned short& val );
	long		Write( unsigned short reg, unsigned short val );

	long		WriteMultiSRMD( unsigned short reg, 
										unsigned short val[], 
										unsigned short count );
	
	long		WriteMultiMRMD( unsigned short reg[], 
										unsigned short val[], 
										unsigned short count );
	
	long		QueryStatusRegs( unsigned short&	StatusReg,
										 unsigned short&	HeatsinkTempReg,
										 unsigned short&	CcdTempReg,
										 unsigned short&	CoolerDriveReg,
										 unsigned short&	VoltageReg,
										 unsigned short&	TdiCounter,
										 unsigned short&	SequenceCounter,
										 unsigned short&	MostRecentFrame,
										 unsigned short&	ReadyFrame,
										 unsigned short&	CurrentFrame );

	void		SetNetworkTransferMode( Apn_NetworkMode TransferMode );

	///////////////////////////////////////////////////////////////////////
	// Generic Public calls for handling key camera activities
	///////////////////////////////////////////////////////////////////////
	long				InitDefaults();

	bool				Expose( double Duration, bool Light );
        bool                            GetImage( unsigned short *pBuffer );
#ifndef ALTA_STANDALONE
        bool BufferImage(char *bufferName );
        bool BufferDriftScan(char *bufferName, int delay, int rowCount, int nblock , int npipe);
#endif 

	bool				StopExposure( bool DigitizeData );
	
	bool				ResetSystem();
	bool				ResetSystemNoFlush();
	bool				PauseTimer( bool PauseState );
	
	bool				GuideAbort();
	bool				GuideRAPlus();
	bool				GuideRAMinus();
	bool				GuideDecPlus();
	bool				GuideDecMinus();
	bool				ConvertBayerImageToRGB( unsigned short *pBayerBuffer,
												unsigned short *pRgbBuffer,
												unsigned short ImageWidth,
												unsigned short ImageHeight );

	///////////////////////////////////////////////////////////////////////
	// Public helper functions
	///////////////////////////////////////////////////////////////////////
	bool				ImageReady();
	bool				ImageInProgress();
	void				SignalImagingDone();

	unsigned short		GetExposurePixelsH();
	unsigned short		GetExposurePixelsV();

	///////////////////////////////////////////////////////////////////////
	// Public calls for internal state control
	///////////////////////////////////////////////////////////////////////
	bool				read_Present();
	unsigned short		read_FirmwareVersion();

	void				read_CameraModel( );
	bool				read_ShutterState();
	bool				read_DisableShutter();
	void				write_DisableShutter( bool DisableShutter );
	bool				read_ForceShutterOpen();
	void				write_ForceShutterOpen( bool ForceShutterOpen );
	bool				read_ShutterAmpControl();
	void				write_ShutterAmpControl( bool ShutterAmpControl );
	
	bool				read_DisableFlushCommands();
	void				write_DisableFlushCommands( bool DisableFlushCommands );
	bool				read_DisablePostExposeFlushing();
	void				write_DisablePostExposeFlushing( bool DisablePostExposeFlushing );

	bool				read_ExternalIoReadout();
	void				write_ExternalIoReadout( bool ExternalIoReadout );
	bool				read_ExternalShutter();
	void				write_ExternalShutter( bool ExternalShutter );
	bool				read_FastSequence();
	void				write_FastSequence( bool FastSequence );

	Apn_CameraMode		read_CameraMode();
	void				write_CameraMode( Apn_CameraMode CameraMode );

	Apn_Resolution		read_DataBits();
	void				write_DataBits( Apn_Resolution BitResolution );

	Apn_Status			read_ImagingStatus();

	Apn_LedMode			read_LedMode();
	void				write_LedMode( Apn_LedMode LedMode );
	Apn_LedState		read_LedState( unsigned short LedId );
	void				write_LedState( unsigned short LedId, Apn_LedState LedState );

	bool				read_CoolerEnable();
	void				write_CoolerEnable( bool CoolerEnable );
	Apn_CoolerStatus	read_CoolerStatus();
	double				read_CoolerSetPoint();
	void				write_CoolerSetPoint( double SetPoint );
	double				read_CoolerBackoffPoint();
	void				write_CoolerBackoffPoint( double BackoffPoint );
	double				read_CoolerDrive();
	double				read_TempCCD();
	double				read_TempHeatsink();
	Apn_FanMode			read_FanMode();
	void				write_FanMode( Apn_FanMode FanMode );

	void				write_FlushBinningV( unsigned short FlushBinningV );
	unsigned short		read_FlushBinningV();

	unsigned short		read_MaxBinningH();
	unsigned short		read_MaxBinningV();

	void				write_RoiBinningH( unsigned short RoiBinningH );
	unsigned short		read_RoiBinningH();
	void				write_RoiBinningV( unsigned short RoiBinningV );
	unsigned short		read_RoiBinningV();

	unsigned short		read_RoiPixelsH();
	void				write_RoiPixelsH( unsigned short RoiPixelsH );
	unsigned short		read_RoiPixelsV();
	void				write_RoiPixelsV( unsigned short RoiPixelsV );

	unsigned short		read_RoiStartX();
	void				write_RoiStartX( unsigned short RoiStartX );
	unsigned short		read_RoiStartY();
	void				write_RoiStartY( unsigned short RoiStartY );

	bool				read_DigitizeOverscan();
	void				write_DigitizeOverscan( bool DigitizeOverscan );
	unsigned short		read_OverscanColumns();

	double				read_ShutterStrobePosition();
	void				write_ShutterStrobePosition( double Position );
	double				read_ShutterStrobePeriod();
	void				write_ShutterStrobePeriod( double Period );
	double				read_ShutterCloseDelay();
	void				write_ShutterCloseDelay( double ShutterCloseDelay );

	bool				read_SequenceBulkDownload();
	void				write_SequenceBulkDownload( bool SequenceBulkDownload );
	double				read_SequenceDelay();
	void				write_SequenceDelay( double Delay );
	bool				read_VariableSequenceDelay();
	void				write_VariableSequenceDelay( bool VariableSequenceDelay );
	unsigned short		read_ImageCount();
	void				write_ImageCount( unsigned short Count );

	unsigned short		read_SequenceCounter();

	bool				read_ContinuousImaging();
	void				write_ContinuousImaging( bool ContinuousImaging );

	unsigned short		read_TDICounter();
	unsigned short		read_TDIRows();
	void				write_TDIRows( unsigned short TdiRows );
	double				read_TDIRate();
	void				write_TDIRate( double TdiRate );
	unsigned short		read_TDIBinningV();
	void				write_TDIBinningV( unsigned short TdiBinningV );

	unsigned short		read_KineticsSections();
	void				write_KineticsSections( unsigned short KineticsSections );
	double				read_KineticsShiftInterval();
	void				write_KineticsShiftInterval( double KineticsShiftInterval );
	unsigned short		read_KineticsSectionHeight();
	void				write_KineticsSectionHeight( unsigned short KineticsSectionHeight );

	bool				read_TriggerNormalEach();
	void				write_TriggerNormalEach( bool TriggerNormalEach );
	bool				read_TriggerNormalGroup();
	void				write_TriggerNormalGroup( bool TriggerNormalGroup );
	bool				read_TriggerTdiKineticsEach();
	void				write_TriggerTdiKineticsEach( bool TriggerTdiKineticsEach );
	bool				read_TriggerTdiKineticsGroup();
	void				write_TriggerTdiKineticsGroup( bool TriggerTdiKineticsGroup );

        bool                            read_ExposureTriggerEach();
        bool                            read_ExposureTriggerGroup();
 
        bool                            read_ExposureExternalShutter();
 

	unsigned short		read_IoPortAssignment();
	void				write_IoPortAssignment( unsigned short IoPortAssignment );
	unsigned short		read_IoPortDirection();
	void				write_IoPortDirection( unsigned short IoPortDirection );
	unsigned short		read_IoPortData();
	void				write_IoPortData( unsigned short IoPortData );

	unsigned short		read_TwelveBitGain();
	void				write_TwelveBitGain( unsigned short TwelveBitGain );

	unsigned short		read_TwelveBitOffset();
	void				write_TwelveBitOffset( unsigned short TwelveBitOffset );


	double				read_InputVoltage();
	long				read_AvailableMemory();

	double				read_MaxExposureTime();

	Apn_NetworkMode		read_NetworkTransferMode();
	void				write_NetworkTransferMode( Apn_NetworkMode TransferMode );

	double				read_TestLedBrightness();
	void				write_TestLedBrightness( double TestLedBrightness );


	Apn_Platform		read_PlatformType();

	unsigned short		read_AscentADGainSixteenLeft();
	unsigned short		read_AscentADGainSixteenRight();
	void				write_AscentADGainSixteen( unsigned short GainValue );

	unsigned short		read_AscentADOffsetSixteenLeft();
	unsigned short		read_AscentADOffsetSixteenRight();
	void				write_AscentADOffsetSixteen( unsigned short OffsetValue );

	unsigned short		read_DigitizationSpeed();
	void				write_DigitizationSpeed( unsigned short DigitizationSpeed );

	/*
	bool				FilterInit( Apn_Filter FilterType );
	bool				FilterClose();

	unsigned short		read_FilterPosition();
	void				write_FilterPosition( unsigned short FilterPosition );

	bool				read_FilterReady();
	unsigned short		read_FilterMaxPositions();
	Apn_Filter			read_FilterWheelType();
	void				read_FilterModel( char *FilterModel, long *BufferLength );
	*/

	bool				read_DataAveraging();
	void				write_DataAveraging( bool DataAveraging );

	bool				read_DualReadout();
	void				write_DualReadout( bool DualReadout );

	bool				read_ConnectionTest();

	bool				read_GuideActive();
//	void				write_GuideActive( bool GuideActive );

	double				read_GuideRAPlusDuration();
	void				write_GuideRAPlusDuration( double GuideRAPlusDuration );

	double				read_GuideRAMinusDuration();
	void				write_GuideRAMinusDuration( double GuideRAMinusDuration );

	double				read_GuideDecPlusDuration();
	void				write_GuideDecPlusDuration( double GuideDecPlusDuration );

	double				read_GuideDecMinusDuration();
	void				write_GuideDecMinusDuration( double GuideDecMinusDuration );

	Apn_BayerShift		read_BayerStartPosition();
	void				write_BayerStartPosition( Apn_BayerShift BayerStartPosition );

	///////////////////////////////////////////////////////////////////////
	// Public Variable
	///////////////////////////////////////////////////////////////////////
	CApnCamData			*m_ApnSensorInfo;


/* was private: */

	// General helper functions
	long LoadVerticalPattern();
	long LoadClampPattern( bool UseOppositePatterns );
        long LoadSkipPattern( bool UseOppositePatterns );
        long LoadRoiPattern( bool UseOppositePatterns, unsigned short Binning );

	long WriteHorizontalPattern( APN_HPATTERN_FILE *Pattern, 
								 unsigned short reg, 
								 unsigned short binning );
	
	long InitTwelveBitAD();

	long InitAscentSixteenBitAD();

	void UpdateGeneralStatus();

	long LookupAltaCameraId( unsigned short CameraId );
	long LookupAscentCameraId( unsigned short CameraId );

	double				CheckGuiderRelayDuration( double GuideDuration );
	unsigned short		CalculateGuiderRelayTimeCounts( double GuideDuration );

	// Internal private variables
	Apn_Platform			m_pvtPlatformType;
	bool					m_pvtResetVerticalArrays;
	FEATURE_SUPPORT_LIST	m_pvtNewFeatureSupport;

	// Camera imaging params
	unsigned short		m_pvtRoiBinningH;
	unsigned short		m_pvtRoiBinningV;
	unsigned short		m_pvtRoiPixelsH;
	unsigned short		m_pvtRoiPixelsV;
	unsigned short		m_pvtRoiStartX;
	unsigned short		m_pvtRoiStartY;

	// Camera state variables
	Apn_CameraMode		m_pvtCameraMode;
	bool				m_pvtExternalShutter;

	Apn_Resolution		m_pvtDataBits;

	Apn_NetworkMode		m_pvtNetworkTransferMode;

	Apn_BayerShift		m_pvtBayerShift;	
	
	bool				m_pvtUseAdvancedStatus;

	unsigned short		m_pvtCameraID;
	unsigned short		m_pvtFirmwareVersion;

	unsigned short		m_pvtImageCount;
	unsigned short		m_pvtTDIRows;
	unsigned short		m_pvtTDIBinningV;
	double				m_pvtTDIRate;
	unsigned short		m_pvtTDICounter;

	unsigned short		m_pvtFlushBinningV;

	bool				m_pvtDigitizeOverscan;

	bool				m_pvtSequenceBulkDownload;
	double				m_pvtSequenceDelay;
	unsigned short		m_pvtSequenceCounter;

	bool				m_pvtFastSequence;

	double				m_pvtShutterStrobePosition;
	double				m_pvtShutterStrobePeriod;
	double				m_pvtShutterCloseDelay;

	unsigned short		m_pvtExposurePixelsH;
	unsigned short		m_pvtExposurePixelsV;

        bool                            m_pvtExposureTriggerGroup;
        bool                            m_pvtExposureTriggerEach;
 
        bool                            m_pvtExposureExternalShutter;
 

	unsigned short		m_pvtTwelveBitGain;
	unsigned short		m_pvtTwelveBitOffset;

	unsigned short		m_pvtAscentSixteenBitGainLeft;
	unsigned short		m_pvtAscentSixteenBitOffsetLeft;
	unsigned short		m_pvtAscentSixteenBitGainRight;
	unsigned short		m_pvtAscentSixteenBitOffsetRight;

	bool				m_pvtDualReadout;

	Apn_LedMode			m_pvtLedMode;
	Apn_LedState		m_pvtLedStateA;
	Apn_LedState		m_pvtLedStateB;
	
	double				m_pvtTestLedBrightness;

	bool				m_pvtCoolerEnable;
	Apn_FanMode			m_pvtFanMode;

	double				m_pvtCoolerBackoffPoint;

	Apn_CoolerStatus	m_pvtCoolerStatus;
	Apn_Status			m_pvtImagingStatus;
	Apn_Status			m_pvtPrevImagingStatus;

	bool				m_pvtShutterState;
	bool				m_pvtImageInProgress;
	bool				m_pvtImageReady;

	bool				m_pvtTriggerNormalEach;
	bool				m_pvtTriggerNormalGroup;
	bool				m_pvtTriggerTdiKineticsEach;
	bool				m_pvtTriggerTdiKineticsGroup;

	unsigned short		m_pvtStatusReg;

	double				m_pvtCoolerDrive;
	double				m_pvtCurrentHeatsinkTemp;
	double				m_pvtCurrentCcdTemp;
	
	double				m_pvtInputVoltage;

	unsigned short		m_pvtIoPortDirection;
	unsigned short		m_pvtIoPortAssignment;

	long				m_pvtQueryStatusRetVal;

	unsigned short		m_pvtMostRecentFrame;
	unsigned short		m_pvtReadyFrame;
	unsigned short		m_pvtCurrentFrame;

	double				m_pvtGuideRAPlusDuration;
	double				m_pvtGuideRAMinusDuration;
	double				m_pvtGuideDecPlusDuration;
	double				m_pvtGuideDecMinusDuration;

	Apn_Filter			m_pvtFilterWheelType;
	unsigned short		m_pvtFilterMaxPositions;

	//////////////////////////////////////////////////
	// Platform Constants
	//////////////////////////////////////////////////

	void				SetPlatformConstants();

	unsigned short		m_PlatformHBinningMax;
	unsigned short		m_PlatformVBinningMax;

	double				m_PlatformTimerResolution;
	double				m_PlatformPeriodTimerResolution;

	long				m_PlatformTimerOffsetCount;

	double				m_PlatformSequenceDelayResolution;
	double				m_PlatformSequenceDelayMaximum;
	double				m_PlatformSequenceDelayMinimum;

	double				m_PlatformExposureTimeMin;
	double				m_PlatformExposureTimeMax;

	double				m_PlatformTdiRateResolution;
	double				m_PlatformTdiRateMin;
	double				m_PlatformTdiRateMax;
	double				m_PlatformTdiRateDefault;

	double				m_PlatformVoltageResolution;

	double				m_PlatformShutterCloseDiff;

	double				m_PlatformStrobePositionMin;
	double				m_PlatformStrobePositionMax;
	double				m_PlatformStrobePositionDefault;

	double				m_PlatformStrobePeriodMin;
	double				m_PlatformStrobePeriodMax;
	double				m_PlatformStrobePeriodDefault;

	long				m_PlatformTempCounts;
	double				m_PlatformTempKelvinScaleOffset;

	double				m_PlatformTempSetpointMin;
	double				m_PlatformTempSetpointMax;

	double				m_PlatformTempHeatsinkMin;
	double				m_PlatformTempHeatsinkMax;

	long				m_PlatformTempSetpointZeroPoint;
	long				m_PlatformTempHeatsinkZeroPoint;

	double				m_PlatformTempDegreesPerBit;

	unsigned short		m_PlatformFanSpeedOff;
	unsigned short		m_PlatformFanSpeedLow;
	unsigned short		m_PlatformFanSpeedMedium;
	unsigned short		m_PlatformFanSpeedHigh;

	double				m_PlatformGuiderRelayResolution;
	double				m_PlatformGuiderRelayMin;
	double				m_PlatformGuiderRelayMax;
	double				m_PlatformGuiderRelayOpenTime;
	double				m_PlatformGuiderRelayCloseTime;

////////////////////////////////////////////////////////////////////////
// Cached variables for the Expose() method
//        double                          m_pvtCacheDuration;
//        unsigned short          m_pvtCacheDataBits;
//        unsigned short          m_pvtCacheRoiPixelsH, m_pvtCacheRoiPixelsV;
//        unsigned short          m_pvtCacheRoiStartX, m_pvtCacheRoiStartY;
//        unsigned short          m_pvtCacheRoiBinningH, m_pvtCacheRoiBinningV;

/* added USB/NET specifics */
        int             m_pvtConnectionOpen;
        char            m_SysDeviceName[80];
        int             m_CamIdA;
        int             m_CamIdB;
        int             m_Option;
        int             m_SysImgSizeBytes;
        unsigned short  m_pvtVendorId;
        unsigned short  m_pvtProductId;
        unsigned short  m_pvtDeviceId;
        double          m_SysDriverVersion;
	unsigned short  m_pvtSequenceImagesDownloaded ;
	unsigned short  m_pvtNumImages;
	unsigned short  m_pvtTdiLinesDownloaded;
	char		m_HostAddr[80];
	unsigned int	m_ImageSizeBytes;
	bool		m_ImageInProgress;
	bool		m_FastDownload;
        unsigned short  m_pvtExposeWidth;
        unsigned short  m_pvtExposeHeight;
        bool            m_pvtExposeExternalShutter;
        unsigned short  m_pvtExposeCameraMode;
        bool            m_pvtExposeSequenceBulkDownload;
        bool            m_pvtExposeCI;
        int             m_pvtExposeHBinning;
        unsigned short  m_pvtExposeBitsPerPixel;
        unsigned short  m_pvtBitsPerPixel;
        bool            m_pvtExposeDualReadout;


 

/* added sensor data mirrors */
        bool            sensorInfo();
        char            m_Sensor[20];
        char            m_CameraModel[20];
        unsigned short  m_CameraId;
        bool            m_InterlineCCD;
        bool            m_SupportsSerialA;
        bool            m_SupportsSerialB;
        bool            m_SensorTypeCCD;
        unsigned short  m_TotalColumns;
        unsigned short  m_ImagingColumns;
        unsigned short  m_ClampColumns;
        unsigned short  m_PreRoiSkipColumns;
        unsigned short  m_PostRoiSkipColumns;
        unsigned short  m_OverscanColumns;
        unsigned short  m_TotalRows;
        unsigned short  m_ImagingRows;
        unsigned short  m_UnderscanRows;
        unsigned short  m_OverscanRows;
        unsigned short  m_VFlushBinning;
        bool            m_HFlushDisable;
        unsigned short  m_ShutterCloseDelay;
        double          m_PixelSizeX;
        double          m_PixelSizeY;
        bool            m_Color;
//      double          m_ReportedGainTwelveBit;
        double          m_ReportedGainSixteenBit;
        double          m_MinSuggestedExpTime;
//      unsigned short  m_TempRegRate;
        unsigned short  m_TempRampRateOne;
        unsigned short  m_TempRampRateTwo;
//        unsigned short  m_DefaultGainTwelveBit;
//        unsigned short  m_DefaultOffsetTwelveBit;
//        unsigned short  m_DefaultRVoltage;
 
};

#endif // !defined(AFX_APNCAMERA_H__CF513996_359F_4103_BBA6_2C730AE2C301__INCLUDED_)
