/////////////////////////////////////////////////////////////
//
// ApnCamData_ASCENT0402ME3.cpp:  Implementation file for the CApnCamData_ASCENT0402ME3 class.
//
// Copyright (c) 2003-2007 Apogee Instruments, Inc.
//
/////////////////////////////////////////////////////////////

#include "ApnCamData_ASCENT0402ME3.h"

#include <stdlib.h>
#include <malloc.h>
#include <string.h>


/////////////////////////////////////////////////////////////
// Construction/Destruction
/////////////////////////////////////////////////////////////


CApnCamData_ASCENT0402ME3::CApnCamData_ASCENT0402ME3()
{
}


CApnCamData_ASCENT0402ME3::~CApnCamData_ASCENT0402ME3()
{
}


void CApnCamData_ASCENT0402ME3::Initialize()
{
	strcpy( m_Sensor, "ASCENT0402ME3" );
	strcpy( m_CameraModel, "3" );
	m_CameraId = 258;
	m_InterlineCCD = false;
	m_SupportsSerialA = false;
	m_SupportsSerialB = false;
	m_SensorTypeCCD = true;
	m_TotalColumns = 28;
	m_ImagingColumns = 20;
	m_ClampColumns = 4;
	m_PreRoiSkipColumns = 0;
	m_PostRoiSkipColumns = 0;
	m_OverscanColumns = 4;
	m_TotalRows = 28;
	m_ImagingRows = 20;
	m_UnderscanRows = 4;
	m_OverscanRows = 4;
	m_VFlushBinning = 1;
	m_EnableSingleRowOffset = false;
	m_RowOffsetBinning = 1;
	m_HFlushDisable = false;
	m_ShutterCloseDelay = 10;
	m_PixelSizeX = 9;
	m_PixelSizeY = 9;
	m_Color = false;
	m_ReportedGainSixteenBit = 1.5;
	m_MinSuggestedExpTime = 50.0;
	m_CoolingSupported = true;
	m_RegulatedCoolingSupported = true;
	m_TempSetPoint = -20.0;
	m_TempRampRateOne = 1200;
	m_TempRampRateTwo = 4000;
	m_TempBackoffPoint = 2.0;
	m_PrimaryADType = ApnAdType_Ascent_Sixteen;
	m_AlternativeADType = ApnAdType_None;
	m_DefaultGainLeft = 0;
	m_DefaultOffsetLeft = 100;
	m_DefaultGainRight = 0;
	m_DefaultOffsetRight = 100;
	m_DefaultRVoltage = 1000;
	m_DefaultSpeed = 0x1;
	m_DefaultDataReduction = true;

	set_vpattern();

	set_hpattern_clamp_sixteen();
	set_hpattern_skip_sixteen();
	set_hpattern_roi_sixteen();

	set_hpattern_clamp_twelve();
	set_hpattern_skip_twelve();
	set_hpattern_roi_twelve();
}


void CApnCamData_ASCENT0402ME3::set_vpattern()
{
	const unsigned short Mask = 0x6;
	const unsigned short NumElements = 71;
	unsigned short Pattern[NumElements] = 
	{
		0x0000, 0x0002, 0x0002, 0x0002, 0x0002, 0x0002, 0x0002, 0x0002, 0x0002, 0x0002, 
		0x0002, 0x0002, 0x0002, 0x0002, 0x0002, 0x0002, 0x0002, 0x0002, 0x0002, 0x0002, 
		0x0002, 0x0004, 0x0004, 0x0004, 0x0004, 0x0004, 0x0004, 0x0004, 0x0004, 0x0004, 
		0x0004, 0x0004, 0x0004, 0x0004, 0x0004, 0x0004, 0x0004, 0x0004, 0x0004, 0x0004, 
		0x0004, 0x0002, 0x0002, 0x0002, 0x0002, 0x0002, 0x0002, 0x0002, 0x0002, 0x0002, 
		0x0002, 0x0002, 0x0002, 0x0002, 0x0002, 0x0002, 0x0002, 0x0002, 0x0002, 0x0002, 
		0x0002, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0001, 
		0x0000
	};

	m_VerticalPattern.Mask = Mask;
	m_VerticalPattern.NumElements = NumElements;
	m_VerticalPattern.PatternData = 
		(unsigned short *)malloc(NumElements * sizeof(unsigned short));

	for ( int i=0; i<NumElements; i++ )
	{
		m_VerticalPattern.PatternData[i] = Pattern[i];
	}
}


void CApnCamData_ASCENT0402ME3::set_hpattern_skip_sixteen()
{
	const unsigned short Mask = 0x0;
	const unsigned short BinningLimit = 1;
	const unsigned short RefNumElements = 0;
	const unsigned short SigNumElements = 0;

	unsigned short *RefPatternData = NULL;

	unsigned short *SigPatternData = NULL;

	unsigned short BinNumElements[APN_MAX_HBINNING] = 
	{
		0x000E
	};

	unsigned short BinPatternData[1][256] = {
	{
		0x0004, 0x000A, 0x0008, 0x0008, 0x0008, 0x0008, 0x0008, 0x0004, 0x0004, 0x0004, 
		0x0004, 0x0004, 0x0005, 0x0004
	} };

	set_hpattern(	&m_SkipPatternSixteen,
					Mask,
					BinningLimit,
					RefNumElements,
					SigNumElements,
					BinNumElements,
					RefPatternData,
					SigPatternData,
					BinPatternData );
}


void CApnCamData_ASCENT0402ME3::set_hpattern_clamp_sixteen()
{
	const unsigned short Mask = 0x0;
	const unsigned short BinningLimit = 1;
	const unsigned short RefNumElements = 0;
	const unsigned short SigNumElements = 0;

	unsigned short *RefPatternData = NULL;

	unsigned short *SigPatternData = NULL;

	unsigned short BinNumElements[APN_MAX_HBINNING] = 
	{
		0x000E
	};

	unsigned short BinPatternData[1][256] = {
	{
		0x0004, 0x000A, 0x0008, 0x0008, 0x0008, 0x0008, 0x0008, 0x0004, 0x0004, 0x0004, 
		0x0004, 0x0004, 0x0005, 0x0004
	} };

	set_hpattern(	&m_ClampPatternSixteen,
					Mask,
					BinningLimit,
					RefNumElements,
					SigNumElements,
					BinNumElements,
					RefPatternData,
					SigPatternData,
					BinPatternData );
}


void CApnCamData_ASCENT0402ME3::set_hpattern_roi_sixteen()
{
	const unsigned short Mask = 0x0;
	const unsigned short BinningLimit = 1;
	const unsigned short RefNumElements = 0;
	const unsigned short SigNumElements = 0;

	unsigned short *RefPatternData = NULL;

	unsigned short *SigPatternData = NULL;

	unsigned short BinNumElements[APN_MAX_HBINNING] = 
	{
		0x0014
	};

	unsigned short BinPatternData[1][256] = {
	{
		0x0004, 0x012A, 0x0128, 0x0128, 0x0928, 0x0928, 0x0128, 0x0208, 0x0008, 0x0048, 
		0x4004, 0xE004, 0xE004, 0x4004, 0x0004, 0x0404, 0x0004, 0x0004, 0x8005, 0x8084, 
	} };

	set_hpattern(	&m_RoiPatternSixteen,
					Mask,
					BinningLimit,
					RefNumElements,
					SigNumElements,
					BinNumElements,
					RefPatternData,
					SigPatternData,
					BinPatternData );
}


void CApnCamData_ASCENT0402ME3::set_hpattern_skip_twelve()
{
	const unsigned short Mask = 0x0;
	const unsigned short BinningLimit = 1;
	const unsigned short RefNumElements = 0;
	const unsigned short SigNumElements = 0;

	unsigned short *RefPatternData = NULL;

	unsigned short *SigPatternData = NULL;

	unsigned short BinNumElements[APN_MAX_HBINNING] = 
	{
		0x000E
	};

	unsigned short BinPatternData[1][256] = {
	{
		0x0004, 0x000A, 0x0008, 0x0008, 0x0008, 0x0008, 0x0008, 0x0004, 0x0004, 0x0004, 
		0x0004, 0x0004, 0x0005, 0x0004
	} };

	set_hpattern(	&m_SkipPatternTwelve,
					Mask,
					BinningLimit,
					RefNumElements,
					SigNumElements,
					BinNumElements,
					RefPatternData,
					SigPatternData,
					BinPatternData );
}


void CApnCamData_ASCENT0402ME3::set_hpattern_clamp_twelve()
{
	const unsigned short Mask = 0x0;
	const unsigned short BinningLimit = 1;
	const unsigned short RefNumElements = 0;
	const unsigned short SigNumElements = 0;

	unsigned short *RefPatternData = NULL;

	unsigned short *SigPatternData = NULL;

	unsigned short BinNumElements[APN_MAX_HBINNING] = 
	{
		0x000E
	};

	unsigned short BinPatternData[1][256] = {
	{
		0x0004, 0x000A, 0x0008, 0x0008, 0x0008, 0x0008, 0x0008, 0x0004, 0x0004, 0x0004, 
		0x0004, 0x0004, 0x0005, 0x0004
	} };

	set_hpattern(	&m_ClampPatternTwelve,
					Mask,
					BinningLimit,
					RefNumElements,
					SigNumElements,
					BinNumElements,
					RefPatternData,
					SigPatternData,
					BinPatternData );
}


void CApnCamData_ASCENT0402ME3::set_hpattern_roi_twelve()
{
	const unsigned short Mask = 0x2;
	const unsigned short BinningLimit = 1;
	const unsigned short RefNumElements = 0;
	const unsigned short SigNumElements = 0;

	unsigned short *RefPatternData = NULL;

	unsigned short *SigPatternData = NULL;

	unsigned short BinNumElements[APN_MAX_HBINNING] = 
	{
		0x0014
	};

	unsigned short BinPatternData[1][256] = {
	{
		0x0004, 0x012A, 0x0128, 0x0128, 0x0928, 0x0928, 0x0128, 0x0208, 0x0008, 0x0048, 
		0x4004, 0xE004, 0xE004, 0x4004, 0x0004, 0x0404, 0x0004, 0x0004, 0x8005, 0x8084, 
	} };

	set_hpattern(	&m_RoiPatternTwelve,
					Mask,
					BinningLimit,
					RefNumElements,
					SigNumElements,
					BinNumElements,
					RefPatternData,
					SigPatternData,
					BinPatternData );
}


void CApnCamData_ASCENT0402ME3::set_hpattern(	APN_HPATTERN_FILE	*Pattern,
											unsigned short	Mask,
											unsigned short	BinningLimit,
											unsigned short	RefNumElements,
											unsigned short	SigNumElements,
											unsigned short	BinNumElements[],
											unsigned short	RefPatternData[],
											unsigned short	SigPatternData[],
											unsigned short	BinPatternData[][APN_MAX_PATTERN_ENTRIES] )
{
	int i, j;

	Pattern->Mask = Mask;
	Pattern->BinningLimit = BinningLimit;
	Pattern->RefNumElements = RefNumElements;
	Pattern->SigNumElements = SigNumElements;

	if ( RefNumElements > 0 )
	{
		Pattern->RefPatternData = 
			(unsigned short *)malloc(RefNumElements * sizeof(unsigned short));

		for ( i=0; i<RefNumElements; i++ )
		{
			Pattern->RefPatternData[i] = RefPatternData[i];
		}
	}

	if ( SigNumElements > 0 )
	{
		Pattern->SigPatternData = 
			(unsigned short *)malloc(SigNumElements * sizeof(unsigned short));

		for ( i=0; i<SigNumElements; i++ )
		{
			Pattern->SigPatternData[i] = SigPatternData[i];
		}
	}

	if ( BinningLimit > 0 )
	{
		for ( i=0; i<BinningLimit; i++ )
		{
			Pattern->BinNumElements[i] = BinNumElements[i];

			Pattern->BinPatternData[i] = 
				(unsigned short *)malloc(BinNumElements[i] * sizeof(unsigned short));

			for ( j=0; j<BinNumElements[i]; j++ )
			{
				Pattern->BinPatternData[i][j] = BinPatternData[i][j];
			}
		}
	}
}
