/////////////////////////////////////////////////////////////
//
// ApnCamData.h:  Interface file for the CApnCamData class.
//
// Copyright (c) 2003-2007 Apogee Instruments, Inc.
//
/////////////////////////////////////////////////////////////

#if !defined(AFX_APNCAMDATA_H__32231556_A1FD_421B_94F8_295D4148E195__INCLUDED_)
#define AFX_APNCAMDATA_H__32231556_A1FD_421B_94F8_295D4148E195__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000


#define APN_MAX_HBINNING		10
#define APN_MAX_PATTERN_ENTRIES 256


typedef struct _APN_VPATTERN_FILE {
	unsigned short	Mask;
	unsigned short	NumElements;
	unsigned short	*PatternData;
} APN_VPATTERN_FILE;

typedef struct _APN_HPATTERN_FILE {
	unsigned short	Mask;
	unsigned short	BinningLimit;
	unsigned short	RefNumElements;
	unsigned short	BinNumElements[APN_MAX_HBINNING];
	unsigned short	SigNumElements;
	unsigned short	*RefPatternData;
	unsigned short	*BinPatternData[APN_MAX_HBINNING];
	unsigned short	*SigPatternData;
} APN_HPATTERN_FILE;

typedef enum ApnAdType {
	ApnAdType_None,
	ApnAdType_Alta_Sixteen,
	ApnAdType_Alta_Twelve,
	ApnAdType_Ascent_Sixteen
};


class CApnCamData  
{
public:
	CApnCamData();
	virtual ~CApnCamData();

	virtual void Initialize() = 0;


	char			m_Sensor[20];
	char			m_CameraModel[20];

	unsigned short	m_CameraId;

	bool			m_InterlineCCD;
	bool			m_SupportsSerialA;
	bool			m_SupportsSerialB;
	bool			m_SensorTypeCCD;

	unsigned short	m_TotalColumns;
	unsigned short	m_ImagingColumns;

	unsigned short	m_ClampColumns;
	unsigned short	m_PreRoiSkipColumns;
	unsigned short	m_PostRoiSkipColumns;
	unsigned short	m_OverscanColumns;

	unsigned short	m_TotalRows;
	unsigned short	m_ImagingRows;

	unsigned short	m_UnderscanRows;
	unsigned short	m_OverscanRows;

	unsigned short	m_VFlushBinning;

	bool			m_EnableSingleRowOffset;
	unsigned short	m_RowOffsetBinning;

	bool			m_HFlushDisable;

	unsigned short	m_ShutterCloseDelay;

	double			m_PixelSizeX;
	double			m_PixelSizeY;

	bool			m_Color;
	
	double			m_ReportedGainSixteenBit;

	double			m_MinSuggestedExpTime;

	bool			m_CoolingSupported;
	bool			m_RegulatedCoolingSupported;

	double			m_TempSetPoint;
	unsigned short	m_TempRampRateOne;
	unsigned short	m_TempRampRateTwo;
	double			m_TempBackoffPoint;

	ApnAdType		m_PrimaryADType;
	ApnAdType		m_AlternativeADType;

	int				m_DefaultGainLeft;
	int				m_DefaultOffsetLeft;
	int				m_DefaultGainRight;
	int				m_DefaultOffsetRight;

	unsigned short	m_DefaultRVoltage;

	unsigned short	m_DefaultSpeed;
	bool			m_DefaultDataReduction;


	// Pattern Files
	APN_VPATTERN_FILE m_VerticalPattern;
	
	APN_HPATTERN_FILE m_ClampPatternSixteen;
	APN_HPATTERN_FILE m_SkipPatternSixteen;
	APN_HPATTERN_FILE m_RoiPatternSixteen;

	APN_HPATTERN_FILE m_ClampPatternTwelve;
	APN_HPATTERN_FILE m_SkipPatternTwelve;
	APN_HPATTERN_FILE m_RoiPatternTwelve;


private:

	void init_vpattern( );
	void clear_vpattern( );

	void init_hpattern( APN_HPATTERN_FILE *Pattern );
	void clear_hpattern( APN_HPATTERN_FILE *Pattern );

};

#endif // !defined(AFX_APNCAMDATA_H__32231556_A1FD_421B_94F8_295D4148E195__INCLUDED_)
