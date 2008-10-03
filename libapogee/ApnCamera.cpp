//////////////////////////////////////////////////////////////////////
//
// ApnCamera.cpp: implementation of the CApnCamera class.
//
// Copyright (c) 2003-2006 Apogee Instruments, Inc.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"

#include "ApnCamera.h"
#include "ApnCamTable.h"


typedef struct _ColorPixel {
	unsigned short Red;
	unsigned short Green;
	unsigned short Blue;
} ColorPixel;


//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CApnCamera::CApnCamera()
{
	AltaDebugOutputString( "APOGEE.DLL - CApnCamera::CApnCamera()" );

	m_pvtPlatformType	= Apn_Platform_Unknown;
	m_ApnSensorInfo		= NULL;
}

CApnCamera::~CApnCamera()
{
	AltaDebugOutputString( "APOGEE.DLL - CApnCamera::~CApnCamera()" );

	if ( m_ApnSensorInfo != NULL )
	{
		delete m_ApnSensorInfo;
		m_ApnSensorInfo = NULL;
                CloseDriver();
	}
}

bool CApnCamera::ConvertBayerImageToRGB( unsigned short *pBayer,
										 unsigned short *pRgb,
										 unsigned short ImageWidth,
										 unsigned short ImageHeight )
{
	unsigned long	i, j;
	unsigned long	Index;
	unsigned long	NextCol, PrevCol;
	unsigned long	NextRow, PrevRow;
	unsigned long	LastRow, LastCol;
	bool			DoRedPixel, DoBluePixel;
	bool			DoGreenRedRow;
	ColorPixel*		Pixel;

	bool			RowShift, ColumnShift;


	if ( (pBayer == NULL) || (pRgb == NULL) )
		return false;

	if ( (ImageWidth == 0) || (ImageHeight == 0) )
		return false;


	RowShift	= false;
	ColumnShift = false;

	Index	= 0;
	Pixel	= (ColorPixel*)pRgb;

	switch ( m_pvtBayerShift )
	{
	case Apn_BayerShift_Automatic:
		if ( (m_pvtRoiStartX % 2) == 1 )
			ColumnShift = true;

		if ( (m_pvtRoiStartY % 2) == 1 )
			RowShift = true;

		break;

	case Apn_BayerShift_None:
		// RowShift and ColumnShift were initialized to "false"
		// We don't need to do anything here
		break;

	case Apn_BayerShift_Column:
		ColumnShift = true;
		break;

	case Apn_BayerShift_Row:
		RowShift = true;
		break;

	case Apn_BayerShift_Both:
		ColumnShift = true;
		RowShift	= true;
		break;
	}

	if ( RowShift == false )
		DoGreenRedRow = true;
	else
		DoGreenRedRow = false;

	LastRow = ImageHeight - 1;
	LastCol	= ImageWidth - 1;

	for ( j=0; j<ImageHeight; j++ )
	{
		if ( DoGreenRedRow )
		{
			///////////////////////
			// Green/Red Row
			///////////////////////

			// DoRedPixel = false;

			if ( ColumnShift )
				DoRedPixel = true;
			else
				DoRedPixel = false;

			for ( i=0; i<ImageWidth; i++ )
			{
				if ( j==0 )
				{
					// First row
					NextRow = Index + ImageWidth;
					PrevRow = NextRow;
					NextCol	= Index + 1;
					PrevCol	= Index - 1;
				}
				else if ( j==LastRow )
				{
					// Last row
					PrevRow = Index - ImageWidth;
					NextRow = PrevRow;
					NextCol	= Index + 1;
					PrevCol	= Index - 1;
				}
				else
				{
					// Middle rows
					NextRow = Index + ImageWidth;
					PrevRow = Index - ImageWidth;
					NextCol	= Index + 1;
					PrevCol	= Index - 1;
				}

				if ( DoRedPixel )
				{
					if ( i==0 )
					{
						Pixel->Red		= pBayer[Index];
						Pixel->Green	= (unsigned long)(pBayer[NextCol] + pBayer[PrevRow] + pBayer[NextRow]) / 3;
						Pixel->Blue		= (unsigned long)(pBayer[PrevRow+1] + pBayer[NextRow+1]) >> 1;
					}
					if ( i==LastCol )
					{
						Pixel->Red		= pBayer[Index];
						Pixel->Green	= (unsigned long)(pBayer[PrevCol] + pBayer[PrevRow] + pBayer[NextRow]) / 3;
						Pixel->Blue		= (unsigned long)(pBayer[PrevRow-1] + pBayer[NextRow-1]) >> 1;
					}
					else
					{
						Pixel->Red		= pBayer[Index];
						Pixel->Green	= (unsigned long)(pBayer[PrevCol] + pBayer[NextCol] + pBayer[PrevRow] + pBayer[NextRow]) >> 2;
						Pixel->Blue		= (unsigned long)(pBayer[PrevRow-1] + pBayer[PrevRow+1] + pBayer[NextRow-1] + pBayer[NextRow+1]) >> 2;
					}
				}
				else
				{
					if ( i==0 )
					{
						Pixel->Red		= pBayer[Index+1];
						Pixel->Green	= pBayer[Index];
						Pixel->Blue		= (unsigned long)(pBayer[NextRow] + pBayer[PrevRow]) >> 1;
					}
					if ( i==LastCol )
					{
						Pixel->Red		= pBayer[PrevCol];
						Pixel->Green	= pBayer[Index];
						Pixel->Blue		= (unsigned long)(pBayer[PrevRow] + pBayer[NextRow]) >> 1;
					}
					else
					{
						Pixel->Red		= (unsigned long)(pBayer[PrevCol] + pBayer[NextCol]) >> 1;
						Pixel->Green	= pBayer[Index];
						Pixel->Blue		= (unsigned long)(pBayer[PrevRow] + pBayer[NextRow]) >> 1;
					}
				}

				// Increment to next pixel
				Pixel++;
				Index++;

				// Alternate
				DoRedPixel = !DoRedPixel;
			}
		}
		else
		{
			/////////////////////
			// Blue/Green Row
			/////////////////////

			// DoBluePixel = true;

			if ( ColumnShift )
				DoBluePixel = false;
			else
				DoBluePixel = true;

			for ( i=0; i<ImageWidth; i++ )
			{
				if ( j==0 )
				{
					// First row
					NextRow = Index + ImageWidth;
					PrevRow = NextRow;
					NextCol	= Index + 1;
					PrevCol	= Index - 1;
				}
				else if ( j==LastRow )
				{
					// Last row
					PrevRow = Index - ImageWidth;
					NextRow = PrevRow;
					NextCol	= Index + 1;
					PrevCol	= Index - 1;
				}
				else
				{
					// Middle rows
					NextRow = Index + ImageWidth;
					PrevRow = Index - ImageWidth;
					NextCol	= Index + 1;
					PrevCol	= Index - 1;
				}

				if ( DoBluePixel )
				{
					if ( i==0 )
					{
						Pixel->Red		= (unsigned long)(pBayer[PrevRow+1] + pBayer[NextRow+1]) >> 1;
						Pixel->Green	= (unsigned long)(pBayer[NextCol] + pBayer[PrevRow] + pBayer[NextRow]) / 3;
						Pixel->Blue		= pBayer[Index]; 
					}
					else if ( i==LastCol )
					{
						Pixel->Red		= (unsigned long)(pBayer[PrevRow-1] + pBayer[NextRow-1]) >> 1;
						Pixel->Green	= (unsigned long)(pBayer[PrevCol] + pBayer[PrevRow] + pBayer[NextRow]) / 3;
						Pixel->Blue		= pBayer[Index]; 
					}
					else
					{
						Pixel->Red		= (unsigned long)(pBayer[PrevRow-1] + pBayer[PrevRow+1] + pBayer[NextRow-1] + pBayer[NextRow+1]) >> 2;
						Pixel->Green	= (unsigned long)(pBayer[PrevCol] + pBayer[NextCol] + pBayer[PrevRow] + pBayer[NextRow]) >> 2;
						Pixel->Blue		= pBayer[Index]; 
					}
				}
				else
				{
					if ( i==0 )
					{
						Pixel->Red		= (unsigned long)(pBayer[PrevRow] + pBayer[NextRow]) >> 1;
						Pixel->Green	= pBayer[Index];
						Pixel->Blue		= pBayer[NextCol];
					}
					else if ( i==LastCol )
					{
						Pixel->Red		= (unsigned long)(pBayer[PrevRow] + pBayer[NextRow]) >> 1;
						Pixel->Green	= pBayer[Index];
						Pixel->Blue		= pBayer[PrevCol];
					}
					else
					{
						Pixel->Red		= (unsigned long)(pBayer[PrevRow] + pBayer[NextRow]) >> 1;
						Pixel->Green	= pBayer[Index];
						Pixel->Blue		= (unsigned long)(pBayer[PrevCol] + pBayer[NextCol]) >> 1;
					}
				}

				// Increment to next pixel
				Pixel++;
				Index++;

				// Alternate
				DoBluePixel = !DoBluePixel;
			}

		}

		// Increment to next pixel
		// Pixel++;
		// Index++;

		DoGreenRedRow = !DoGreenRedRow;
	}

	return true;
}


bool CApnCamera::Expose( double Duration, bool Light )
{
	unsigned long	ExpTime;
	unsigned short	BitsPerPixel;
	unsigned short	UnbinnedRoiX;
	unsigned short	UnbinnedRoiY;		
	unsigned short	PreRoiSkip, PostRoiSkip;
	unsigned short	PreRoiRows, PostRoiRows;
	unsigned short	PreRoiVBinning, PostRoiVBinning;

	unsigned short	RegStartCmd;
	unsigned short	RoiRegBuffer[15];
	unsigned short	RoiRegData[15];

	unsigned short	TotalHPixels;
	unsigned short	TotalVPixels;

	unsigned long	TestImageSize;

	unsigned long	WaitCounter;

	char			szOutputText[128];


	sprintf( szOutputText, "APOGEE.DLL - CApnCamera::Expose( Duration = %f, Light = %d ) -> BEGIN", Duration, Light );
	AltaDebugOutputString( szOutputText );


	WaitCounter = 0;
	while ( read_ImagingStatus() != Apn_Status_Flushing )
	{
		Sleep( 20 );

		WaitCounter++;
		
		if ( WaitCounter > 150 )
		{
			// we've waited longer than 3s to start flushing in the camera head
			// something is amiss...abort the expose command to avoid an infinite loop
			AltaDebugOutputString( "APOGEE.DLL - CApnCamera::Expose() -> ERROR: Timed out waiting for flushing!!" );
			
			return false;
		}
	}

	// Validate the "Duration" parameter
	if ( Duration < m_PlatformExposureTimeMin )
		Duration = m_PlatformExposureTimeMin;

	// Validate the ROI params
	UnbinnedRoiX	= m_pvtRoiPixelsH * m_pvtRoiBinningH;

	PreRoiSkip		= m_pvtRoiStartX;

	PostRoiSkip		= m_ApnSensorInfo->m_TotalColumns -
					  m_ApnSensorInfo->m_ClampColumns -
					  PreRoiSkip - 
					  UnbinnedRoiX;

	TotalHPixels = UnbinnedRoiX + PreRoiSkip + PostRoiSkip + m_ApnSensorInfo->m_ClampColumns;

	if ( TotalHPixels != m_ApnSensorInfo->m_TotalColumns )
	{
		AltaDebugOutputString( "APOGEE.DLL - CApnCamera::Expose() -> ERROR: Horizontal geometry incorrect" );		
		
		return false;
	}

	UnbinnedRoiY	= m_pvtRoiPixelsV * m_pvtRoiBinningV;

	PreRoiRows		= m_ApnSensorInfo->m_UnderscanRows + 
					  m_pvtRoiStartY;
	
	PostRoiRows		= m_ApnSensorInfo->m_TotalRows -
					  PreRoiRows -
					  UnbinnedRoiY;

	TotalVPixels = UnbinnedRoiY + PreRoiRows + PostRoiRows;

	if ( TotalVPixels != m_ApnSensorInfo->m_TotalRows )
	{
		AltaDebugOutputString( "APOGEE.DLL - CApnCamera::Expose() -> ERROR: Vertical geometry incorrect" );		
		
		return false;
	}

	m_pvtExposurePixelsV = m_pvtRoiPixelsV;
	m_pvtExposurePixelsH = m_pvtRoiPixelsH;

	if ( read_CameraMode() == Apn_CameraMode_Test )
	{
		TestImageSize = m_pvtExposurePixelsV * m_pvtExposurePixelsH;
		Write( FPGA_REG_TEST_COUNT_UPPER, (USHORT)(TestImageSize>>16) );
		Write( FPGA_REG_TEST_COUNT_LOWER, (USHORT)(TestImageSize & 0xFFFF) );
	}

	if ( m_pvtDataBits == Apn_Resolution_SixteenBit )
	{
		BitsPerPixel = 16;
	}
	else if ( m_pvtDataBits == Apn_Resolution_TwelveBit )
	{
		BitsPerPixel = 12;
	}

	if ( PreStartExpose( BitsPerPixel ) != 0 )
	{
		AltaDebugOutputString( "APOGEE.DLL - CApnCamera::Expose() -> ERROR: Failed PreStartExpose()!!" );		
		
		return false;
	}

	// Calculate the vertical parameters
	PreRoiVBinning	= m_ApnSensorInfo->m_RowOffsetBinning;
	PostRoiVBinning	= 1;

	// For interline CCDs, set "Fast Dump" mode if the particular array is NOT digitized
	/*
	if ( m_ApnSensorInfo->m_InterlineCCD )
	{
		// use the fast dump feature to get rid of the data quickly.
		// one row, binning to the original row count
		// note that we only are not digitized in arrays 1 and 3
		PreRoiVBinning	= PreRoiRows;
		PostRoiVBinning	= PostRoiRows;

		PreRoiVBinning	|= FPGA_BIT_ARRAY_FASTDUMP;
		PostRoiVBinning	|= FPGA_BIT_ARRAY_FASTDUMP;

		PreRoiRows	= 1;
		PostRoiRows	= 1;
	}
	*/

	// Set up the geometry for a full frame device
	if ( m_ApnSensorInfo->m_EnableSingleRowOffset )
	{
		// PreRoiVBinning	+= PreRoiRows;
		PreRoiVBinning	= PreRoiRows;
		PostRoiVBinning	= PostRoiRows;

		PreRoiVBinning	|= FPGA_BIT_ARRAY_FASTDUMP;
		PostRoiVBinning	|= FPGA_BIT_ARRAY_FASTDUMP;

		PreRoiRows	= 1;
		PostRoiRows	= 1;
	}

	// Calculate the exposure time to program to the camera
	ExpTime = ((unsigned long)(Duration / m_PlatformTimerResolution)) + m_PlatformTimerOffsetCount;

	Write( FPGA_REG_TIMER_LOWER, (unsigned short)(ExpTime & 0xFFFF));
	ExpTime = ExpTime >> 16;
	Write( FPGA_REG_TIMER_UPPER, (unsigned short)(ExpTime & 0xFFFF));

	// Set up the registers for the exposure
	ResetSystemNoFlush();

	// Issue the reset
	// RoiRegBuffer[0] = FPGA_REG_COMMAND_B;
	RoiRegBuffer[0] = FPGA_REG_SCRATCH;
	RoiRegData[0]	= FPGA_BIT_CMD_RESET;

	// Program the horizontal settings
	RoiRegBuffer[1]	= FPGA_REG_PREROI_SKIP_COUNT;
	RoiRegData[1]	= PreRoiSkip;

	RoiRegBuffer[2]	= FPGA_REG_ROI_COUNT;
	// Number of ROI pixels.  Adjust the 12bit operation here to account for an extra 
	// 10 pixel shift as a result of the A/D conversion.
	if ( m_pvtDataBits == Apn_Resolution_SixteenBit )
	{
		RoiRegData[2] = m_pvtExposurePixelsH + 1;
	}
	else if ( m_pvtDataBits == Apn_Resolution_TwelveBit )
	{
		RoiRegData[2] = m_pvtExposurePixelsH + 10;
	}

	RoiRegBuffer[3]	= FPGA_REG_POSTROI_SKIP_COUNT;
	RoiRegData[3]	= PostRoiSkip;

	// Program the vertical settings
	if ( m_pvtFirmwareVersion < 11 )
	{
		RoiRegBuffer[4] = FPGA_REG_A1_ROW_COUNT;
		RoiRegData[4]	= PreRoiRows;
		RoiRegBuffer[5] = FPGA_REG_A1_VBINNING;
		RoiRegData[5]	= PreRoiVBinning;

		RoiRegBuffer[6] = FPGA_REG_A2_ROW_COUNT;
		RoiRegData[6]	= m_pvtRoiPixelsV;
		RoiRegBuffer[7] = FPGA_REG_A2_VBINNING;
		RoiRegData[7]	= (m_pvtRoiBinningV | FPGA_BIT_ARRAY_DIGITIZE);

		RoiRegBuffer[8] = FPGA_REG_A3_ROW_COUNT;
		RoiRegData[8]	= PostRoiRows;
		RoiRegBuffer[9] = FPGA_REG_A3_VBINNING;
		RoiRegData[9]	= PostRoiVBinning;

		RoiRegBuffer[10]	= FPGA_REG_SCRATCH;
		RoiRegData[10]		= 0;
		RoiRegBuffer[11]	= FPGA_REG_SCRATCH;
		RoiRegData[11]		= 0;
		RoiRegBuffer[12]	= FPGA_REG_SCRATCH;
		RoiRegData[12]		= 0;
		RoiRegBuffer[13]	= FPGA_REG_SCRATCH;
		RoiRegData[13]		= 0;
	}
	else
	{
		if ( m_ApnSensorInfo->m_EnableSingleRowOffset )
		{
			RoiRegBuffer[4] = FPGA_REG_A1_ROW_COUNT;
			RoiRegData[4]	= 0;
			RoiRegBuffer[5] = FPGA_REG_A1_VBINNING;
			RoiRegData[5]	= 0;

			RoiRegBuffer[6] = FPGA_REG_A2_ROW_COUNT;
			RoiRegData[6]	= PreRoiRows;
			RoiRegBuffer[7] = FPGA_REG_A2_VBINNING;
			RoiRegData[7]	= PreRoiVBinning;

			RoiRegBuffer[8] = FPGA_REG_A3_ROW_COUNT;
			RoiRegData[8]	= m_pvtRoiPixelsV;
			RoiRegBuffer[9] = FPGA_REG_A3_VBINNING;
			RoiRegData[9]	= (m_pvtRoiBinningV | FPGA_BIT_ARRAY_DIGITIZE);
			
			RoiRegBuffer[10] = FPGA_REG_A4_ROW_COUNT;
			RoiRegData[10]	= 0;
			RoiRegBuffer[11] = FPGA_REG_A4_VBINNING;
			RoiRegData[11]	= 0;

			RoiRegBuffer[12] = FPGA_REG_A5_ROW_COUNT;
			RoiRegData[12]	= PostRoiRows;
			RoiRegBuffer[13] = FPGA_REG_A5_VBINNING;
			RoiRegData[13]	= PostRoiVBinning;
		}
		else
		{
			if ( PreRoiRows > 70 )
			{
				RoiRegBuffer[4] = FPGA_REG_A1_ROW_COUNT;
				RoiRegData[4]	= 1;
				RoiRegBuffer[5] = FPGA_REG_A1_VBINNING;
				RoiRegData[5]	= (PreRoiRows-70);

				RoiRegBuffer[6] = FPGA_REG_A2_ROW_COUNT;
				RoiRegData[6]	= 70;
				RoiRegBuffer[7] = FPGA_REG_A2_VBINNING;
				RoiRegData[7]	= 1;
			}
			else
			{
				RoiRegBuffer[4] = FPGA_REG_A1_ROW_COUNT;
				RoiRegData[4]	= 0;
				RoiRegBuffer[5] = FPGA_REG_A1_VBINNING;
				RoiRegData[5]	= 0;

				RoiRegBuffer[6] = FPGA_REG_A2_ROW_COUNT;
				RoiRegData[6]	= PreRoiRows;
				RoiRegBuffer[7] = FPGA_REG_A2_VBINNING;
				RoiRegData[7]	= PreRoiVBinning;
			}
			
			RoiRegBuffer[8] = FPGA_REG_A3_ROW_COUNT;
			RoiRegData[8]	= m_pvtRoiPixelsV;
			RoiRegBuffer[9] = FPGA_REG_A3_VBINNING;
			RoiRegData[9]	= (m_pvtRoiBinningV | FPGA_BIT_ARRAY_DIGITIZE);
			
			if ( PostRoiRows > 70 )
			{
				RoiRegBuffer[10] = FPGA_REG_A4_ROW_COUNT;
				RoiRegData[10]	= 1;
				RoiRegBuffer[11] = FPGA_REG_A4_VBINNING;
				RoiRegData[11]	= (PostRoiRows-70);

				RoiRegBuffer[12] = FPGA_REG_A5_ROW_COUNT;
				RoiRegData[12]	= 70;
				RoiRegBuffer[13] = FPGA_REG_A5_VBINNING;
				RoiRegData[13]	= 1;
			}
			else
			{
				RoiRegBuffer[10] = FPGA_REG_A4_ROW_COUNT;
				RoiRegData[10]	= 0;
				RoiRegBuffer[11] = FPGA_REG_A4_VBINNING;
				RoiRegData[11]	= 0;

				RoiRegBuffer[12] = FPGA_REG_A5_ROW_COUNT;
				RoiRegData[12]	= PostRoiRows;
				RoiRegBuffer[13] = FPGA_REG_A5_VBINNING;
				RoiRegData[13]	= PostRoiVBinning;
			}
		}
	}

	// Issue the reset
	RoiRegBuffer[14]	= FPGA_REG_COMMAND_B;
	RoiRegData[14]		= FPGA_BIT_CMD_RESET;

	// Send the instruction sequence to the camera
	AltaDebugOutputString( "APOGEE.DLL - CApnCamera::Expose() -> Issue WriteMultiMRMD() for Exposure setup" );
	WriteMultiMRMD( RoiRegBuffer, RoiRegData, 15 );

	// Issue the flush for interlines, or if using the external shutter
	if ( (m_ApnSensorInfo->m_InterlineCCD && m_pvtFastSequence) || (m_pvtExternalShutter) )
	{
		// Make absolutely certain that flushing starts first
		// in order to use Progressive Scan/Ratio Mode
		Write( FPGA_REG_COMMAND_A, FPGA_BIT_CMD_FLUSH );

		for ( int i=0; i<2; i++ )
		{
			Write( FPGA_REG_SCRATCH, 0x8086 );
			Write( FPGA_REG_SCRATCH, 0x8088 );
		}
	}

	m_pvtExposureExternalShutter = m_pvtExternalShutter;

	switch ( m_pvtCameraMode )
	{
	case Apn_CameraMode_Normal:
		if ( Light )
			RegStartCmd		= FPGA_BIT_CMD_EXPOSE;
		else
			RegStartCmd		= FPGA_BIT_CMD_DARK;
		
		if ( m_pvtTriggerNormalGroup )
			m_pvtExposureTriggerGroup = true;
		else
			m_pvtExposureTriggerGroup = false;

		if ( m_pvtTriggerNormalEach )
			m_pvtExposureTriggerEach = true;
		else
			m_pvtExposureTriggerEach = false;

		break;
	case Apn_CameraMode_TDI:
		RegStartCmd			= FPGA_BIT_CMD_TDI;

		if ( m_pvtTriggerTdiKineticsGroup )
			m_pvtExposureTriggerGroup = false;
		else
			m_pvtExposureTriggerGroup = false;

		if ( m_pvtTriggerTdiKineticsEach )
			m_pvtExposureTriggerEach = false;
		else
			m_pvtExposureTriggerEach = false;

		break;
	case Apn_CameraMode_Test:
		if ( Light )
			RegStartCmd		= FPGA_BIT_CMD_TEST;
		else
			RegStartCmd		= FPGA_BIT_CMD_TEST;
		break;
	case Apn_CameraMode_ExternalTrigger:
		RegStartCmd			= FPGA_BIT_CMD_TRIGGER_EXPOSE;
		break;
	case Apn_CameraMode_Kinetics:
		RegStartCmd			= FPGA_BIT_CMD_KINETICS;

		if ( m_pvtTriggerTdiKineticsGroup )
			m_pvtExposureTriggerGroup = true;
		else
			m_pvtExposureTriggerGroup = false;

		if ( m_pvtTriggerTdiKineticsEach )
			m_pvtExposureTriggerEach = true;
		else
			m_pvtExposureTriggerEach = false;

		break;
	}

	// Send the instruction sequence to the camera
	AltaDebugOutputString( "APOGEE.DLL - CApnCamera::Expose() -> Issue start command to FPGA_REG_COMMAND_A" );
	Write( FPGA_REG_COMMAND_A, RegStartCmd );

	// Set our flags to mark the start of a new exposure
	m_pvtImageInProgress = true;
	m_pvtImageReady		 = false;

	// Debug output for leaving this function
	sprintf( szOutputText, "APOGEE.DLL - CApnCamera::Expose( Duration = %f, Light = %d ) -> END", Duration, Light );
	AltaDebugOutputString( szOutputText );

	return true;
}

bool CApnCamera::ResetSystem()
{
	AltaDebugOutputString( "APOGEE.DLL - CApnCamera::ResetSystem()" );

	// Reset the camera engine
	Write( FPGA_REG_COMMAND_B, FPGA_BIT_CMD_RESET );

	// A little delay before we start flushing
	Write( FPGA_REG_SCRATCH, 0x8086 );
	Write( FPGA_REG_SCRATCH, 0x8088 );
	Write( FPGA_REG_SCRATCH, 0x8086 );
	Write( FPGA_REG_SCRATCH, 0x8088 );
	Write( FPGA_REG_SCRATCH, 0x8086 );
	Write( FPGA_REG_SCRATCH, 0x8088 );

	// Start flushing
	Write( FPGA_REG_COMMAND_A, FPGA_BIT_CMD_FLUSH );

	// A little delay once we've started flushing
	Write( FPGA_REG_SCRATCH, 0x8086 );
	Write( FPGA_REG_SCRATCH, 0x8088 );

	return true;
}

bool CApnCamera::ResetSystemNoFlush()
{
	AltaDebugOutputString( "APOGEE.DLL - CApnCamera::ResetSystemNoFlush()" );


	// Reset the camera engine
	Write( FPGA_REG_COMMAND_B, FPGA_BIT_CMD_RESET );

	// A little delay before we start flushing
	Write( FPGA_REG_SCRATCH, 0x8086 );
	Write( FPGA_REG_SCRATCH, 0x8088 );
	Write( FPGA_REG_SCRATCH, 0x8086 );
	Write( FPGA_REG_SCRATCH, 0x8088 );
	Write( FPGA_REG_SCRATCH, 0x8086 );
	Write( FPGA_REG_SCRATCH, 0x8088 );

	return true;
}

bool CApnCamera::PauseTimer( bool PauseState )
{
	unsigned short RegVal;
	bool		   CurrentState;

	char szOutputText[128];
	sprintf( szOutputText, "APOGEE.DLL - CApnCamera::PauseTimer( PauseState = %d)", PauseState );
	AltaDebugOutputString( szOutputText );


	Read( FPGA_REG_OP_A, RegVal );

	CurrentState = ( RegVal & FPGA_BIT_PAUSE_TIMER ) == FPGA_BIT_PAUSE_TIMER;

	if ( CurrentState != PauseState )
	{
		if ( PauseState )
		{
			RegVal |= FPGA_BIT_PAUSE_TIMER;
		}
		else
		{
			RegVal &= ~FPGA_BIT_PAUSE_TIMER;
		}
		Write ( FPGA_REG_OP_A, RegVal );
	}

	return true;
}

bool CApnCamera::GetImage( unsigned short *pBuffer )
{
	AltaDebugOutputString( "APOGEE.DLL - CApnCamera::GetImage() -> BEGIN" );


	unsigned short	Width, Height;
	unsigned long	Count;
	
	if ( m_pvtImageInProgress )
	{
		if ( GetImageData( pBuffer, Width, Height, Count ) != CAPNCAMERA_SUCCESS )
		{
			return false;
		}
	}

	AltaDebugOutputString( "APOGEE.DLL - CApnCamera::GetImage() -> END" );

	return true;
}

bool CApnCamera::StopExposure( bool DigitizeData )
{
	char szOutputText[128];
	sprintf( szOutputText, "APOGEE.DLL - CApnCamera::StopExposure( DigitizeData = %d) -> BEGIN", DigitizeData );
	AltaDebugOutputString( szOutputText );


	if ( m_pvtImageInProgress )
	{
		Write( FPGA_REG_COMMAND_B, FPGA_BIT_CMD_END_EXPOSURE );

		if ( PostStopExposure( DigitizeData ) != 0 )
		{
			return false;
		}
	}

	sprintf( szOutputText, "APOGEE.DLL - CApnCamera::StopExposure( DigitizeData = %d) -> END", DigitizeData );
	AltaDebugOutputString( szOutputText );

	return true;
}

bool CApnCamera::GuideAbort()
{
	AltaDebugOutputString( "APOGEE.DLL - CApnCamera::GuideAbort()" );

	if ( m_pvtPlatformType == Apn_Platform_Ascent )
		Write( FPGA_REG_COMMAND_A, FPGA_BIT_CMD_GUIDE_ABORT );

	return true;
}

bool CApnCamera::GuideRAPlus()
{
	AltaDebugOutputString( "APOGEE.DLL - CApnCamera::GuideRAPlus()" );

	if ( m_pvtPlatformType == Apn_Platform_Ascent )
		Write( FPGA_REG_COMMAND_A, FPGA_BIT_CMD_GUIDE_RA_PLUS );

	return true;
}

bool CApnCamera::GuideRAMinus()
{
	AltaDebugOutputString( "APOGEE.DLL - CApnCamera::GuideRAMinus()" );

	if ( m_pvtPlatformType == Apn_Platform_Ascent )
		Write( FPGA_REG_COMMAND_A, FPGA_BIT_CMD_GUIDE_RA_MINUS );

	return true;
}

bool CApnCamera::GuideDecPlus()
{
	AltaDebugOutputString( "APOGEE.DLL - CApnCamera::GuideDecPlus()" );

	if ( m_pvtPlatformType == Apn_Platform_Ascent )
		Write( FPGA_REG_COMMAND_A, FPGA_BIT_CMD_GUIDE_DEC_PLUS );

	return true;
}

bool CApnCamera::GuideDecMinus()
{
	AltaDebugOutputString( "APOGEE.DLL - CApnCamera::GuideDecMinus()" );

	if ( m_pvtPlatformType == Apn_Platform_Ascent )
		Write( FPGA_REG_COMMAND_A, FPGA_BIT_CMD_GUIDE_DEC_MINUS );

	return true;
}

#ifndef LINUX
void CApnCamera::SetNetworkTransferMode( Apn_NetworkMode TransferMode )
{
	char szOutputText[128];
	sprintf( szOutputText, "APOGEE.DLL - CApnCamera::SetNetworkTransferMode( TransferMode = %d)", TransferMode );
	AltaDebugOutputString( szOutputText );


	if ( GetCameraInterface() == Apn_Interface_USB )
		return;
}
#endif

unsigned short CApnCamera::GetExposurePixelsH()
{
	return m_pvtExposurePixelsH;
}

unsigned short CApnCamera::GetExposurePixelsV()
{
	return m_pvtExposurePixelsV;
}

double CApnCamera::read_InputVoltage()
{
	UpdateGeneralStatus();

	return m_pvtInputVoltage;
}

long CApnCamera::read_AvailableMemory()
{
	AltaDebugOutputString( "APOGEE.DLL - CApnCamera::read_AvailableMemory()" );

	long AvailableMemory;

	switch( GetCameraInterface() )
	{
	case Apn_Interface_NET:
		AvailableMemory = 28 * 1024;
		break;
	case Apn_Interface_USB:
		AvailableMemory = 32 * 1024;
		break;
	default:
		break;
	}

	return AvailableMemory;
}

unsigned short CApnCamera::read_FirmwareVersion()
{
	AltaDebugOutputString( "APOGEE.DLL - CApnCamera::read_FirmwareVersion()" );

	return m_pvtFirmwareVersion;
}

void CApnCamera::read_CameraModel( )
{
	char szModel[256]; 
	ApnCamModelLookup( m_pvtCameraID, m_pvtFirmwareVersion, GetCameraInterface(), szModel );

//	*BufferLength = strlen( szModel );
//	strncpy( CameraModel, szModel, *BufferLength + 1);
}

bool CApnCamera::read_ShutterState()
{
	AltaDebugOutputString( "APOGEE.DLL - CApnCamera::read_ShutterState()" );

	UpdateGeneralStatus();

	return m_pvtShutterState;
}

bool CApnCamera::read_DisableShutter()
{
	AltaDebugOutputString( "APOGEE.DLL - CApnCamera::read_DisableShutter()" );

	unsigned short RegVal;
	Read( FPGA_REG_OP_A, RegVal );
	return ( (RegVal & FPGA_BIT_DISABLE_SHUTTER) != 0 );
}

void CApnCamera::write_DisableShutter( bool DisableShutter )
{
	char szOutputText[128];
	sprintf( szOutputText, "APOGEE.DLL - CApnCamera::write_DisableShutter( DisableShutter = %d)", DisableShutter );
	AltaDebugOutputString( szOutputText );

	unsigned short RegVal;
	Read( FPGA_REG_OP_A, RegVal );

	if ( DisableShutter )
		RegVal |= FPGA_BIT_DISABLE_SHUTTER;
	else
		RegVal &= ~FPGA_BIT_DISABLE_SHUTTER;

	Write( FPGA_REG_OP_A, RegVal );
}

bool CApnCamera::read_ForceShutterOpen()
{
	AltaDebugOutputString( "APOGEE.DLL - CApnCamera::read_ForceShutterOpen()" );

	unsigned short RegVal;
	Read( FPGA_REG_OP_A, RegVal );
	return ( (RegVal & FPGA_BIT_FORCE_SHUTTER) != 0 );
}

void CApnCamera::write_ForceShutterOpen( bool ForceShutterOpen )
{
	char szOutputText[128];
	sprintf( szOutputText, "APOGEE.DLL - CApnCamera::write_ForceShutterOpen( ForceShutterOpen = %d)", ForceShutterOpen );
	AltaDebugOutputString( szOutputText );

	unsigned short RegVal;
	Read( FPGA_REG_OP_A, RegVal );
	
	if ( ForceShutterOpen )
		RegVal |= FPGA_BIT_FORCE_SHUTTER;
	else 
		RegVal &= ~FPGA_BIT_FORCE_SHUTTER;

	Write( FPGA_REG_OP_A, RegVal );
}

bool CApnCamera::read_ShutterAmpControl()
{
	AltaDebugOutputString( "APOGEE.DLL - CApnCamera::read_ShutterAmpControl()" );

	unsigned short RegVal;
	Read( FPGA_REG_OP_A, RegVal );
	return ( (RegVal & FPGA_BIT_SHUTTER_AMP_CONTROL ) != 0 );
}

void CApnCamera::write_ShutterAmpControl( bool ShutterAmpControl )
{
	char szOutputText[128];
	sprintf( szOutputText, "APOGEE.DLL - CApnCamera::write_ShutterAmpControl( ShutterAmpControl = %d)", ShutterAmpControl );
	AltaDebugOutputString( szOutputText );

	unsigned short RegVal;
	Read( FPGA_REG_OP_A, RegVal );

	if ( ShutterAmpControl )
		RegVal |= FPGA_BIT_SHUTTER_AMP_CONTROL;
	else
		RegVal &= ~FPGA_BIT_SHUTTER_AMP_CONTROL;

	Write( FPGA_REG_OP_A, RegVal );
}

bool CApnCamera::read_DisableFlushCommands()
{
	AltaDebugOutputString( "APOGEE.DLL - CApnCamera::read_DisableFlushCommands()" );

	unsigned short RegVal;
	Read( FPGA_REG_OP_B, RegVal );
	return ( (RegVal & FPGA_BIT_DISABLE_FLUSH_COMMANDS ) != 0 );
}

void CApnCamera::write_DisableFlushCommands( bool DisableFlushCommands )
{
	char szOutputText[128];
	sprintf( szOutputText, "APOGEE.DLL - CApnCamera::write_DisableFlushCommands( DisableFlushCommands = %d)", DisableFlushCommands );
	AltaDebugOutputString( szOutputText );

	unsigned short RegVal;
	Read( FPGA_REG_OP_B, RegVal );

	if ( DisableFlushCommands )
		RegVal |= FPGA_BIT_DISABLE_FLUSH_COMMANDS;
	else
		RegVal &= ~FPGA_BIT_DISABLE_FLUSH_COMMANDS;

	Write( FPGA_REG_OP_B, RegVal );
}

bool CApnCamera::read_DisablePostExposeFlushing()
{
	AltaDebugOutputString( "APOGEE.DLL - CApnCamera::read_DisablePostExposeFlushing()" );

	unsigned short RegVal;
	Read( FPGA_REG_OP_B, RegVal );
	return ( (RegVal & FPGA_BIT_DISABLE_POST_EXP_FLUSH ) != 0 );
}

void CApnCamera::write_DisablePostExposeFlushing( bool DisablePostExposeFlushing )
{
	char szOutputText[128];
	sprintf( szOutputText, "APOGEE.DLL - CApnCamera::write_DisablePostExposeFlushing( DisablePostExposeFlushing = %d)", DisablePostExposeFlushing );
	AltaDebugOutputString( szOutputText );

	unsigned short RegVal;
	Read( FPGA_REG_OP_B, RegVal );

	if ( DisablePostExposeFlushing )
		RegVal |= FPGA_BIT_DISABLE_POST_EXP_FLUSH;
	else
		RegVal &= ~FPGA_BIT_DISABLE_POST_EXP_FLUSH;

	Write( FPGA_REG_OP_B, RegVal );
}

bool CApnCamera::read_ExternalIoReadout()
{
	AltaDebugOutputString( "APOGEE.DLL - CApnCamera::read_ExternalIoReadout()" );

	unsigned short	RegVal;
	bool			RetVal;

	RetVal = false;

	if ( m_pvtPlatformType == Apn_Platform_Alta )
	{
		Read( FPGA_REG_OP_A, RegVal );
		RetVal = (RegVal & FPGA_BIT_SHUTTER_MODE) != 0;
	}
	else if ( m_pvtPlatformType == Apn_Platform_Ascent )
	{
		RetVal = false;
	}

	return RetVal;
}

void CApnCamera::write_ExternalIoReadout( bool ExternalIoReadout )
{
	char szOutputText[128];
	sprintf( szOutputText, "APOGEE.DLL - CApnCamera::write_ExternalIoReadout( ExternalIoReadout = %d)", ExternalIoReadout );
	AltaDebugOutputString( szOutputText );

	unsigned short	RegVal;

	if ( m_pvtPlatformType == Apn_Platform_Alta )
	{
		Read( FPGA_REG_OP_A, RegVal );

		if ( ExternalIoReadout )
			RegVal |= FPGA_BIT_SHUTTER_MODE;
		else
			RegVal &= ~FPGA_BIT_SHUTTER_MODE;

		Write( FPGA_REG_OP_A, RegVal );
	}
}

bool CApnCamera::read_ExternalShutter()
{
	AltaDebugOutputString( "APOGEE.DLL - CApnCamera::read_ExternalShutter()" );

	unsigned short	RegVal;
	Read( FPGA_REG_OP_A, RegVal );
	return ( (RegVal & FPGA_BIT_SHUTTER_SOURCE) != 0 );
}

void CApnCamera::write_ExternalShutter( bool ExternalShutter )
{
	char szOutputText[128];
	sprintf( szOutputText, "APOGEE.DLL - CApnCamera::write_ExternalShutter( ExternalShutter = %d)", ExternalShutter );
	AltaDebugOutputString( szOutputText );

	unsigned short	RegVal;
	Read( FPGA_REG_OP_A, RegVal );

	if ( ExternalShutter )
		RegVal |= FPGA_BIT_SHUTTER_SOURCE;
	else
		RegVal &= ~FPGA_BIT_SHUTTER_SOURCE;

	Write( FPGA_REG_OP_A, RegVal );

	m_pvtExternalShutter = ExternalShutter;
}

bool CApnCamera::read_FastSequence()
{
	AltaDebugOutputString( "APOGEE.DLL - CApnCamera::read_FastSequence()" );

	if ( m_ApnSensorInfo->m_InterlineCCD == false )
		return false;

	unsigned short	RegVal;
	Read( FPGA_REG_OP_A, RegVal );
	return ( (RegVal & FPGA_BIT_RATIO) != 0 );
}

void CApnCamera::write_FastSequence( bool FastSequence )
{
	char szOutputText[128];
	sprintf( szOutputText, "APOGEE.DLL - CApnCamera::write_FastSequence( FastSequence = %d)", FastSequence );
	AltaDebugOutputString( szOutputText );

	// fast sequence/progressive scan is for interline only
	if ( m_ApnSensorInfo->m_InterlineCCD == false )
		return;

	// do not allow triggers on each progressive scanned image
	if ( m_pvtTriggerNormalEach )
		return;

	unsigned short	RegVal;
	Read( FPGA_REG_OP_A, RegVal );

	if ( FastSequence )
	{
		RegVal |= FPGA_BIT_RATIO;
		Write( FPGA_REG_SHUTTER_CLOSE_DELAY, 0x0 );
	}
	else
	{
		RegVal &= ~FPGA_BIT_RATIO;
	}

	Write( FPGA_REG_OP_A, RegVal );

	m_pvtFastSequence = FastSequence;
}

Apn_NetworkMode CApnCamera::read_NetworkTransferMode()
{
	AltaDebugOutputString( "APOGEE.DLL - CApnCamera::read_NetworkTransferMode()" );

	return m_pvtNetworkTransferMode;
}

void CApnCamera::write_NetworkTransferMode( Apn_NetworkMode TransferMode )
{
	char szOutputText[128];
	sprintf( szOutputText, "APOGEE.DLL - CApnCamera::write_NetworkTransferMode( TransferMode = %d)", TransferMode );
	AltaDebugOutputString( szOutputText );

	SetNetworkTransferMode( TransferMode );

	m_pvtNetworkTransferMode = TransferMode;
}

Apn_CameraMode CApnCamera::read_CameraMode()
{
	AltaDebugOutputString( "APOGEE.DLL - CApnCamera::read_CameraMode()" );

	return m_pvtCameraMode;
}

void CApnCamera::write_CameraMode( Apn_CameraMode CameraMode )
{
	char szOutputText[128];
	sprintf( szOutputText, "APOGEE.DLL - CApnCamera::write_CameraMode( CameraMode = %d)", CameraMode );
	AltaDebugOutputString( szOutputText );

	unsigned short RegVal;

	// The Apn_CameraMode_ExternalShutter mode was deprecated as of 
	// version 3.0.15.  If an application sends this mode, it is now 
	// converted to Apn_CameraMode_Normal.  Applications should use the
	// ExternalShutter property to enable an external shutter
	if ( CameraMode == Apn_CameraMode_ExternalShutter )
		CameraMode = Apn_CameraMode_Normal;

	// Only allow Apn_CameraMode_Kinetics if our firmware is v17 or higher.
	// If the firmware doesn't support the mode, switch to Apn_CameraMode_Normal
	if ( (m_pvtFirmwareVersion < 17 ) && (CameraMode == Apn_CameraMode_Kinetics) )
		CameraMode = Apn_CameraMode_Normal;

	// If we are an interline CCD, do not allow the mode to be set to 
	// Apn_CameraMode_TDI or Apn_CameraMode_Kinetics
	if ( m_ApnSensorInfo->m_InterlineCCD == true )
	{
		if ( (CameraMode == Apn_CameraMode_TDI) || (CameraMode == Apn_CameraMode_Kinetics) )
			CameraMode = Apn_CameraMode_Normal;
	}

	// If our state isn't going to change, do nothing
	if ( m_pvtCameraMode == CameraMode )
		return;

	// If we didn't return already, then if we know our state is going to
	// change.  If we're in test mode, clear the appropriate FPGA bit(s).
	switch( m_pvtCameraMode )
	{
	case Apn_CameraMode_Normal:
		break;
	case Apn_CameraMode_TDI:
		break;
	case Apn_CameraMode_Test:
		Read( FPGA_REG_OP_B, RegVal );
		RegVal &= ~FPGA_BIT_AD_SIMULATION;
		Write( FPGA_REG_OP_B, RegVal );
		break;
	case Apn_CameraMode_ExternalTrigger:
		RegVal = read_IoPortAssignment();
		if ( m_pvtPlatformType == Apn_Platform_Alta )
		{
			RegVal &= 0x3E;						// External trigger is pin one (bit zero) 
			write_IoPortAssignment( RegVal );
		}
		break;
	case Apn_CameraMode_Kinetics:
		break;
	}
	
	switch ( CameraMode )
	{
	case Apn_CameraMode_Normal:
		break;
	case Apn_CameraMode_TDI:
		break;
	case Apn_CameraMode_Test:
		Read( FPGA_REG_OP_B, RegVal );
		RegVal |= FPGA_BIT_AD_SIMULATION;
		Write( FPGA_REG_OP_B, RegVal );
		break;
	case Apn_CameraMode_ExternalTrigger:
		RegVal = read_IoPortAssignment();
		if ( m_pvtPlatformType == Apn_Platform_Alta )
		{
			RegVal |= 0x01;						// External trigger is pin one (bit zero)
			write_IoPortAssignment( RegVal );
		}
		else if ( m_pvtPlatformType == Apn_Platform_Ascent )
		{
			RegVal |= 0x02;						// External trigger is pin one (bit zero)
			write_IoPortAssignment( RegVal );
		}
		break;
	case Apn_CameraMode_Kinetics:
		break;
	}

	m_pvtCameraMode = CameraMode;
}

Apn_Resolution CApnCamera::read_DataBits()
{
	AltaDebugOutputString( "APOGEE.DLL - CApnCamera::read_DataBits()" );

	return m_pvtDataBits;
}

void CApnCamera::write_DataBits( Apn_Resolution BitResolution )
{
	char szOutputText[128];
	sprintf( szOutputText, "APOGEE.DLL - CApnCamera::write_DataBits( BitResolution = %d)", BitResolution );
	AltaDebugOutputString( szOutputText );

	unsigned short	RegVal;
	bool			UseOppositePatterns;

	if ( GetCameraInterface() == Apn_Interface_NET )
	{
		// The network interface is 16bpp only.  Changing the resolution 
		// for network cameras has no effect.
		return;
	}

	if ( m_ApnSensorInfo->m_AlternativeADType == ApnAdType_None )
	{
		// No 12bit A/D converter is supported.  Changing the resolution
		// for these systems has no effect
		return;
	}


	if ( m_pvtDataBits != BitResolution )
	{
		// Reset the camera
		Write( FPGA_REG_COMMAND_B, FPGA_BIT_CMD_RESET );

		// Change bit setting after the reset
		Read( FPGA_REG_OP_A, RegVal );
		
		if ( BitResolution == Apn_Resolution_TwelveBit )
			RegVal |= FPGA_BIT_DIGITIZATION_RES;

		if ( BitResolution == Apn_Resolution_SixteenBit )
			RegVal &= ~FPGA_BIT_DIGITIZATION_RES;

		Write( FPGA_REG_OP_A, RegVal );

		m_pvtDataBits = BitResolution;
	
		if ( BitResolution == Apn_Resolution_TwelveBit )
			Write( FPGA_REG_HRAM_INV_MASK, m_ApnSensorInfo->m_RoiPatternTwelve.Mask );

		if ( BitResolution == Apn_Resolution_SixteenBit )
			Write( FPGA_REG_HRAM_INV_MASK, m_ApnSensorInfo->m_RoiPatternSixteen.Mask );

		UseOppositePatterns = false;

		if ( (m_ApnSensorInfo->m_CameraId >= 256) && (m_ApnSensorInfo->m_DefaultSpeed == 0x0) )
		{
			UseOppositePatterns = true;
		}

		LoadClampPattern( UseOppositePatterns );
		LoadSkipPattern( UseOppositePatterns );
		LoadRoiPattern( UseOppositePatterns, m_pvtRoiBinningH );

		// Reset the camera and start flushing
		ResetSystem();
	}
}

Apn_Status CApnCamera::read_ImagingStatus()
{
	bool Exposing, Active, Done, Flushing, WaitOnTrigger;
	bool DataHalted, RamError;


	AltaDebugOutputString( "APOGEE.DLL - CApnCamera::read_ImagingStatus()" );

	UpdateGeneralStatus();

	// Update the ImagingStatus
	Exposing		= false;
	Active			= false;
	Done			= false;
	Flushing		= false;
	WaitOnTrigger	= false;
	DataHalted		= false;
	RamError		= false;

	if ( (GetCameraInterface() == Apn_Interface_USB) && 
		 (m_pvtQueryStatusRetVal == CAPNCAMERA_ERR_CONNECT) )
	{
		m_pvtImagingStatus = Apn_Status_ConnectionError;
		return m_pvtImagingStatus;
	}

	if ( (m_pvtStatusReg & FPGA_BIT_STATUS_IMAGING_ACTIVE) != 0 )
		Active = true;
	
	if ( (m_pvtStatusReg & FPGA_BIT_STATUS_IMAGE_EXPOSING) != 0 )
		Exposing = true;
	
	if ( (m_pvtStatusReg & FPGA_BIT_STATUS_IMAGE_DONE) != 0 )
		Done = true;

	if ( (m_pvtStatusReg & FPGA_BIT_STATUS_FLUSHING) != 0 )
		Flushing = true;
	
	if ( (m_pvtStatusReg & FPGA_BIT_STATUS_WAITING_TRIGGER) != 0 )
		WaitOnTrigger = true;

	if ( (m_pvtStatusReg & FPGA_BIT_STATUS_DATA_HALTED) != 0 )
		DataHalted = true;

	if ( (m_pvtStatusReg & FPGA_BIT_STATUS_PATTERN_ERROR) != 0 )
		RamError = true;

	// set the previous imaging status to whatever the current status is
	// this previous status will only be used for stopping a triggered
	// exposure, in the case where the hw trigger was not yet received.
	m_pvtPrevImagingStatus = m_pvtImagingStatus;
	
	if ( RamError )
	{
		m_pvtImagingStatus = Apn_Status_PatternError;
	}
	else
	{
		if ( DataHalted )
		{
			m_pvtImagingStatus = Apn_Status_DataError;
		}
		else
		{
			if ( WaitOnTrigger )
			{
				m_pvtImagingStatus = Apn_Status_WaitingOnTrigger;

				if ( m_pvtExposureExternalShutter && Active && Exposing )
				{
					m_pvtImagingStatus = Apn_Status_Exposing;
				}
			}
			else
			{
				if ( Done && m_pvtImageInProgress && ( (m_pvtCameraMode != Apn_CameraMode_TDI) || 
					((m_pvtCameraMode == Apn_CameraMode_TDI) && (m_pvtSequenceBulkDownload)) ) )
				{
					m_pvtImageReady			= true;
					m_pvtImagingStatus		= Apn_Status_ImageReady;
				}
				else
				{
					if ( Active )
					{
						if ( Exposing )
							m_pvtImagingStatus = Apn_Status_Exposing;
						else
							m_pvtImagingStatus = Apn_Status_ImagingActive;
					}
					else
					{
						if ( Flushing )
							m_pvtImagingStatus = Apn_Status_Flushing;
						else
						{
							if ( m_pvtImageInProgress && (m_pvtCameraMode == Apn_CameraMode_TDI) )
							{
								// Driver defined status...not all rows have been returned to the application
								m_pvtImagingStatus = Apn_Status_ImagingActive;
							}
							else
							{
								m_pvtImagingStatus = Apn_Status_Idle;
							}

							if ( m_pvtPrevImagingStatus == Apn_Status_WaitingOnTrigger )
							{
								// we've transitioned from waiting on the trigger
								// to an idle state.  this means the trigger was
								// never received by the hardware.  reset the hardware
								// and start flushing the sensor.
							}
						}
					}
				}
			}
		}
	}

#ifdef APOGEE_DLL_IMAGING_STATUS_OUTPUT
	char szMsg[512];
	sprintf( szMsg, "APOGEE.DLL - CApnCamera::read_ImagingStatus() - Flags: Active=%d; Exposing=%d; Done=%d; Flushing=%d; WaitOnTrigger=%d", Active, Exposing, Done, Flushing, WaitOnTrigger );
	AltaDebugOutputString( szMsg );

	switch( m_pvtImagingStatus )
	{
	case Apn_Status_DataError:
		AltaDebugOutputString( "APOGEE.DLL - CApnCamera::read_ImagingStatus() returning Apn_Status_DataError" );
		break;
	case Apn_Status_PatternError:
		AltaDebugOutputString( "APOGEE.DLL - CApnCamera::read_ImagingStatus() returning Apn_Status_PatternError" );
		break;
	case Apn_Status_Idle:
		AltaDebugOutputString( "APOGEE.DLL - CApnCamera::read_ImagingStatus() returning Apn_Status_Idle" );
		break;
	case Apn_Status_Exposing:
		AltaDebugOutputString( "APOGEE.DLL - CApnCamera::read_ImagingStatus() returning Apn_Status_Exposing" );
		break;
	case Apn_Status_ImagingActive:
		AltaDebugOutputString( "APOGEE.DLL - CApnCamera::read_ImagingStatus() returning Apn_Status_ImagingActive" );
		break;
	case Apn_Status_ImageReady:
		AltaDebugOutputString( "APOGEE.DLL - CApnCamera::read_ImagingStatus() returning Apn_Status_ImageReady" );
		break;
	case Apn_Status_Flushing:
		AltaDebugOutputString( "APOGEE.DLL - CApnCamera::read_ImagingStatus() returning Apn_Status_Flushing" );
		break;
	case Apn_Status_WaitingOnTrigger:
		AltaDebugOutputString( "APOGEE.DLL - CApnCamera::read_ImagingStatus() returning Apn_Status_WaitingOnTrigger" );
		break;
	default:
		AltaDebugOutputString( "APOGEE.DLL - CApnCamera::read_ImagingStatus() returning UNDEFINED!!" );
		break;
	}
#endif

	return m_pvtImagingStatus;
}

Apn_LedMode CApnCamera::read_LedMode()
{
	AltaDebugOutputString( "APOGEE.DLL - CApnCamera::read_LedMode()" );

	return m_pvtLedMode;
}

void CApnCamera::write_LedMode( Apn_LedMode LedMode )
{
	unsigned short	RegVal;

	char szOutputText[128];
	sprintf( szOutputText, "APOGEE.DLL - CApnCamera::write_LedMode( LedMode = %d)", LedMode );
	AltaDebugOutputString( szOutputText );

	Read( FPGA_REG_OP_A, RegVal );

	switch ( LedMode )
	{
	case Apn_LedMode_DisableAll:
		RegVal |= FPGA_BIT_LED_DISABLE;
		break;
	case Apn_LedMode_DisableWhileExpose:
		RegVal &= ~FPGA_BIT_LED_DISABLE;
		RegVal |= FPGA_BIT_LED_EXPOSE_DISABLE;
		break;
	case Apn_LedMode_EnableAll:
		RegVal &= ~FPGA_BIT_LED_DISABLE;
		RegVal &= ~FPGA_BIT_LED_EXPOSE_DISABLE;
		break;
	}

	m_pvtLedMode = LedMode;

	Write( FPGA_REG_OP_A, RegVal );
}

Apn_LedState CApnCamera::read_LedState( unsigned short LedId )
{
	AltaDebugOutputString( "APOGEE.DLL - CApnCamera::read_LedState()" );

	Apn_LedState RetVal;

	if ( LedId == 0 )				// LED A
		RetVal = m_pvtLedStateA;
	
	if ( LedId == 1 )				// LED B
		RetVal = m_pvtLedStateB;

	return RetVal;
}

void CApnCamera::write_LedState( unsigned short LedId, Apn_LedState LedState )
{
	unsigned short	RegVal;

	char szOutputText[128];
	sprintf( szOutputText, "APOGEE.DLL - CApnCamera::write_LedState( LedId = %d, LedState = %d)", LedId, LedState );
	AltaDebugOutputString( szOutputText );

	RegVal = 0x0;

	if ( LedId == 0 )			// LED A
	{
		RegVal = (m_pvtLedStateB << 4);	// keep the current settings for LED B
		RegVal |= LedState;				// program new settings
		m_pvtLedStateA = LedState;
	}
	else if ( LedId == 1 )		// LED B
	{
		RegVal = m_pvtLedStateA;		// keep the current settings for LED A
		RegVal |= (LedState << 4);		// program new settings
		m_pvtLedStateB = LedState;
	}

	Write( FPGA_REG_LED_SELECT, RegVal );
}

bool CApnCamera::read_CoolerEnable()
{
	AltaDebugOutputString( "APOGEE.DLL - CApnCamera::read_CoolerEnable()" );

	return m_pvtCoolerEnable;
}

void CApnCamera::write_CoolerEnable( bool CoolerEnable )
{
	char szOutputText[128];
	sprintf( szOutputText, "APOGEE.DLL - CApnCamera::write_CoolerEnable( CoolerEnable = %d)", CoolerEnable );
	AltaDebugOutputString( szOutputText );

	if ( CoolerEnable )
	{
		Write( FPGA_REG_COMMAND_B, FPGA_BIT_CMD_RAMP_TO_SETPOINT );
	}
	else
	{
		Write( FPGA_REG_COMMAND_B, FPGA_BIT_CMD_RAMP_TO_AMBIENT );
	}

	m_pvtCoolerEnable = CoolerEnable;
}

Apn_CoolerStatus CApnCamera::read_CoolerStatus()
{
	bool CoolerAtTemp;
	bool CoolerActive;
	bool CoolerTempRevised;

	AltaDebugOutputString( "APOGEE.DLL - CApnCamera::read_CoolerStatus()" );

	UpdateGeneralStatus();

	// Update CoolerStatus
	CoolerActive		= false;
	CoolerAtTemp		= false;
	CoolerTempRevised	= false;

	if ( (m_pvtStatusReg & FPGA_BIT_STATUS_TEMP_AT_TEMP) != 0 )
		CoolerAtTemp = true;
	
	if ( (m_pvtStatusReg & FPGA_BIT_STATUS_TEMP_ACTIVE) != 0 )
		CoolerActive = true;

	if ( (m_pvtStatusReg & FPGA_BIT_STATUS_TEMP_REVISION) != 0 )
		CoolerTempRevised = true;

	// Now derive our cooler state
	if ( !CoolerActive )
	{
		m_pvtCoolerStatus = Apn_CoolerStatus_Off;
	}
	else
	{
		if ( CoolerTempRevised )
		{
			m_pvtCoolerStatus = Apn_CoolerStatus_Revision;
		}
		else
		{
			if ( CoolerAtTemp )
				m_pvtCoolerStatus = Apn_CoolerStatus_AtSetPoint;
			else
				m_pvtCoolerStatus = Apn_CoolerStatus_RampingToSetPoint;

		}
	}

	return m_pvtCoolerStatus;
}

double CApnCamera::read_CoolerSetPoint()
{
	unsigned short	RegVal;
	double			TempVal;

	AltaDebugOutputString( "APOGEE.DLL - CApnCamera::read_CoolerSetPoint()" );

	Read( FPGA_REG_TEMP_DESIRED, RegVal );
	
	RegVal &= 0x0FFF;

	TempVal = ( RegVal - m_PlatformTempSetpointZeroPoint ) * m_PlatformTempDegreesPerBit;

	return TempVal;
}

void CApnCamera::write_CoolerSetPoint( double SetPoint )
{
	char szOutputText[128];
	sprintf( szOutputText, "APOGEE.DLL - CApnCamera::write_CoolerSetPoint( SetPoint = %f)", SetPoint );
	AltaDebugOutputString( szOutputText );


	unsigned short	RegVal;
	double			TempVal;
	
	TempVal = SetPoint;

	if ( SetPoint < (m_PlatformTempSetpointMin - m_PlatformTempKelvinScaleOffset) )
		TempVal = m_PlatformTempSetpointMin;

	if ( SetPoint > (m_PlatformTempSetpointMax - m_PlatformTempKelvinScaleOffset) )
		TempVal = m_PlatformTempSetpointMax;

	RegVal = (unsigned short)( (TempVal / m_PlatformTempDegreesPerBit) + m_PlatformTempSetpointZeroPoint );
	
	Write( FPGA_REG_TEMP_DESIRED, RegVal );
}

double CApnCamera::read_CoolerBackoffPoint()
{
	AltaDebugOutputString( "APOGEE.DLL - CApnCamera::read_CoolerBackoffPoint()" );

	return ( m_pvtCoolerBackoffPoint );
}

void CApnCamera::write_CoolerBackoffPoint( double BackoffPoint )
{
	char szOutputText[128];
	sprintf( szOutputText, "APOGEE.DLL - CApnCamera::write_CoolerBackoffPoint( BackoffPoint = %f)", BackoffPoint );
	AltaDebugOutputString( szOutputText );


	unsigned short	RegVal;
	double			TempVal;
	
	TempVal = BackoffPoint;

	// BackoffPoint must be a positive number!
	if ( BackoffPoint < 0.0 )
		TempVal = 0.0;

	if ( BackoffPoint < (m_PlatformTempSetpointMin - m_PlatformTempKelvinScaleOffset) )
		TempVal = m_PlatformTempSetpointMin;

	if ( BackoffPoint > (m_PlatformTempSetpointMax - m_PlatformTempKelvinScaleOffset) )
		TempVal = m_PlatformTempSetpointMax;

	m_pvtCoolerBackoffPoint = TempVal;

	RegVal = (unsigned short)( TempVal / m_PlatformTempDegreesPerBit );
	
	Write( FPGA_REG_TEMP_BACKOFF, RegVal );
}

double CApnCamera::read_CoolerDrive()
{
	AltaDebugOutputString( "APOGEE.DLL - CApnCamera::read_CoolerDrive()" );

	UpdateGeneralStatus();

	return m_pvtCoolerDrive;
}

double CApnCamera::read_TempCCD()
{
	double	TempAvg;
	double	TempTotal;

	AltaDebugOutputString( "APOGEE.DLL - CApnCamera::read_TempCCD()" );

	TempTotal = 0;

	for ( int i=0; i<8; i++ )
	{
		UpdateGeneralStatus();
		TempTotal += m_pvtCurrentCcdTemp;
	}

	TempAvg = TempTotal / 8;

	return TempAvg;
}

double CApnCamera::read_TempHeatsink()
{
	double	TempAvg;
	double	TempTotal;


	if ( m_pvtPlatformType == Apn_Platform_Alta )
	{
		TempTotal = 0;

		for ( int i=0; i<8; i++ )
		{
			UpdateGeneralStatus();
			TempTotal += m_pvtCurrentHeatsinkTemp;
		}

		TempAvg = TempTotal / 8;
	}
	else if ( m_pvtPlatformType == Apn_Platform_Ascent )
	{
		// Hinksink temperature recording not supported
		// Return an obviously incorrect value
		TempAvg = -255;
	}

	return TempAvg;
}

Apn_FanMode	CApnCamera::read_FanMode()
{
	Apn_FanMode RetVal;

	if ( m_pvtPlatformType == Apn_Platform_Alta )
		RetVal = m_pvtFanMode;
	else if ( m_pvtPlatformType == Apn_Platform_Ascent )
		RetVal = Apn_FanMode_Off;

	return RetVal;
}

void CApnCamera::write_FanMode( Apn_FanMode FanMode )
{
	char szOutputText[128];
	sprintf( szOutputText, "APOGEE.DLL - CApnCamera::write_FanMode( FanMode = %d)", FanMode );
	AltaDebugOutputString( szOutputText );


	unsigned short	RegVal;
	unsigned short	OpRegA;

	if ( m_pvtPlatformType == Apn_Platform_Alta )
	{
		if ( m_pvtFanMode == FanMode )
			return;

		if ( m_pvtCoolerEnable )
		{
			Read( FPGA_REG_OP_A, OpRegA );
			OpRegA |= FPGA_BIT_TEMP_SUSPEND;
			Write( FPGA_REG_OP_A, OpRegA );

			do 
			{ 
				Read( FPGA_REG_GENERAL_STATUS, RegVal );
			} while ( (RegVal & FPGA_BIT_STATUS_TEMP_SUSPEND_ACK) == 0 );
			
		}

		switch ( FanMode )
		{
		case Apn_FanMode_Off:
			RegVal = m_PlatformFanSpeedOff;
			break;
		case Apn_FanMode_Low:
			RegVal = m_PlatformFanSpeedLow;
			break;
		case Apn_FanMode_Medium:
			RegVal = m_PlatformFanSpeedMedium;
			break;
		case Apn_FanMode_High:
			RegVal = m_PlatformFanSpeedHigh;
			break;
		}

		Write( FPGA_REG_FAN_SPEED_CONTROL, RegVal );

		Read( FPGA_REG_OP_B, RegVal );
		RegVal |= FPGA_BIT_DAC_SELECT_ZERO;
		RegVal &= ~FPGA_BIT_DAC_SELECT_ONE;
		Write( FPGA_REG_OP_B, RegVal );

		Write( FPGA_REG_COMMAND_B, FPGA_BIT_CMD_DAC_LOAD );

		m_pvtFanMode = FanMode;

		if ( m_pvtCoolerEnable )
		{
			OpRegA &= ~FPGA_BIT_TEMP_SUSPEND;
			Write( FPGA_REG_OP_A, OpRegA );
		}
	}
}

double CApnCamera::read_ShutterStrobePosition()
{
	AltaDebugOutputString( "APOGEE.DLL - CApnCamera::read_ShutterStrobePosition" );

	return m_pvtShutterStrobePosition;
}

void CApnCamera::write_ShutterStrobePosition( double Position )
{
	char szOutputText[128];
	sprintf( szOutputText, "APOGEE.DLL - CApnCamera::write_ShutterStrobePosition( Position = %f)", Position );
	AltaDebugOutputString( szOutputText );


	unsigned short RegVal;
	
	if ( Position < m_PlatformStrobePositionMin )
		Position = m_PlatformStrobePositionMin;

	RegVal = (unsigned short)((Position - m_PlatformStrobePositionMin) / m_PlatformTimerResolution);

	Write( FPGA_REG_SHUTTER_STROBE_POSITION, RegVal );

	m_pvtShutterStrobePosition = Position;
}

double CApnCamera::read_ShutterStrobePeriod()
{
	AltaDebugOutputString( "APOGEE.DLL - CApnCamera::read_ShutterStrobePosition" );

	return m_pvtShutterStrobePeriod;
}

void CApnCamera::write_ShutterStrobePeriod( double Period )
{
	char szOutputText[128];
	sprintf( szOutputText, "APOGEE.DLL - CApnCamera::write_ShutterStrobePeriod( Period = %f)", Period );
	AltaDebugOutputString( szOutputText );


	unsigned short RegVal;

	if ( Period < m_PlatformStrobePeriodMin )
		Period = m_PlatformStrobePeriodMin;

	RegVal = (unsigned short)((Period - m_PlatformStrobePeriodMin) / m_PlatformPeriodTimerResolution);

	Write( FPGA_REG_SHUTTER_STROBE_PERIOD, RegVal );
	
	m_pvtShutterStrobePeriod = Period;
}

double CApnCamera::read_ShutterCloseDelay()
{
	AltaDebugOutputString( "APOGEE.DLL - CApnCamera::read_ShutterCloseDelay" );

	return m_pvtShutterCloseDelay;
}

void CApnCamera::write_ShutterCloseDelay( double ShutterCloseDelay )
{
	char szOutputText[128];
	sprintf( szOutputText, "APOGEE.DLL - CApnCamera::write_ShutterCloseDelay( ShutterCloseDelay = %f)", ShutterCloseDelay );
	AltaDebugOutputString( szOutputText );


	unsigned short RegVal;

	if ( ShutterCloseDelay < m_PlatformShutterCloseDiff )
		ShutterCloseDelay = m_PlatformShutterCloseDiff;

	RegVal = (unsigned short)( (ShutterCloseDelay - m_PlatformShutterCloseDiff) / m_PlatformTimerResolution );

	Write( FPGA_REG_SHUTTER_CLOSE_DELAY, RegVal );

	m_pvtShutterCloseDelay = ShutterCloseDelay;
}

bool CApnCamera::read_SequenceBulkDownload()
{
	AltaDebugOutputString( "APOGEE.DLL - CApnCamera::read_SequenceBulkDownload" );

	return m_pvtSequenceBulkDownload;
}

void CApnCamera::write_SequenceBulkDownload( bool SequenceBulkDownload )
{
	char szOutputText[128];
	AltaDebugPrint( szOutputText, "APOGEE.DLL - CApnCamera::write_SequenceBulkDownload( SequenceBulkDownload = %d)", SequenceBulkDownload );
	AltaDebugOutputString( szOutputText );

	if ( GetCameraInterface() == Apn_Interface_NET )
	{
		m_pvtSequenceBulkDownload = true;
		return;
	}

	m_pvtSequenceBulkDownload = SequenceBulkDownload;
}

double CApnCamera::read_SequenceDelay()
{
	AltaDebugOutputString( "APOGEE.DLL - CApnCamera::read_SequenceDelay" );

	return m_pvtSequenceDelay;
}

void CApnCamera::write_SequenceDelay( double Delay )
{
	char szOutputText[128];
	sprintf( szOutputText, "APOGEE.DLL - CApnCamera::write_SequenceDelay( Delay = %f)", Delay );
	AltaDebugOutputString( szOutputText );


	unsigned short RegVal;

	if ( Delay > m_PlatformSequenceDelayMaximum )
		Delay = m_PlatformSequenceDelayMaximum;

	if ( Delay < m_PlatformSequenceDelayMinimum )
		Delay = m_PlatformSequenceDelayMinimum;

	m_pvtSequenceDelay = Delay;

	RegVal = (unsigned short)(Delay / m_PlatformSequenceDelayResolution);

	Write( FPGA_REG_SEQUENCE_DELAY, RegVal );
}

bool CApnCamera::read_VariableSequenceDelay()
{
	AltaDebugOutputString( "APOGEE.DLL - CApnCamera::read_VariableSequenceDelay" );


	unsigned short RegVal;
	Read( FPGA_REG_OP_A, RegVal );
	// variable delay occurs when the bit is 0
	return ( (RegVal & FPGA_BIT_DELAY_MODE) == 0 );	
}

void CApnCamera::write_VariableSequenceDelay( bool VariableSequenceDelay )
{
	char szOutputText[128];
	sprintf( szOutputText, "APOGEE.DLL - CApnCamera::write_VariableSequenceDelay( VariableSequenceDelay = %d)", VariableSequenceDelay );
	AltaDebugOutputString( szOutputText );


	unsigned short RegVal;

	Read( FPGA_REG_OP_A, RegVal );

	if ( VariableSequenceDelay )
		RegVal &= ~FPGA_BIT_DELAY_MODE;		// variable when zero
	else
		RegVal |= FPGA_BIT_DELAY_MODE;		// constant when one

	Write( FPGA_REG_OP_A, RegVal );
}

unsigned short CApnCamera::read_ImageCount()
{
	AltaDebugOutputString( "APOGEE.DLL - CApnCamera::read_ImageCount" );


	return m_pvtImageCount;
}

void CApnCamera::write_ImageCount( unsigned short Count )
{
	char szOutputText[128];
	sprintf( szOutputText, "APOGEE.DLL - CApnCamera::write_ImageCount( Count = %d)", Count );
	AltaDebugOutputString( szOutputText );


	if ( Count == 0 )
		Count = 1;

	Write( FPGA_REG_IMAGE_COUNT, Count );
	
	m_pvtImageCount = Count;
}

unsigned short CApnCamera::read_FlushBinningV()
{
	return m_pvtFlushBinningV;
}

void CApnCamera::write_FlushBinningV( unsigned short FlushBinningV )
{
	unsigned short	NewFlushBinningV;


	// Do some bounds checking on our input parameter
	if ( FlushBinningV == 0 ) 
		NewFlushBinningV = 1;
	else if ( FlushBinningV > read_MaxBinningV() ) 
		NewFlushBinningV = read_MaxBinningV();
	else
		NewFlushBinningV = FlushBinningV;

	// First check to see if we actually need to update the binning
	if ( NewFlushBinningV != m_pvtFlushBinningV )
	{
		ResetSystemNoFlush();

		Write( FPGA_REG_VFLUSH_BINNING, NewFlushBinningV );
		m_pvtFlushBinningV = NewFlushBinningV;

		ResetSystem();
	}
}

unsigned short CApnCamera::read_RoiBinningH()
{
	return m_pvtRoiBinningH;
}

void CApnCamera::write_RoiBinningH( unsigned short RoiBinningH )
{
	unsigned short	NewRoiBinningH;
	bool			UseOppositePatterns;


	// Do some bounds checking on our input parameter
	if ( RoiBinningH == 0 )
		NewRoiBinningH = 1;
	else if ( RoiBinningH > read_MaxBinningH() )
		NewRoiBinningH = read_MaxBinningH();
	else
		NewRoiBinningH = RoiBinningH;

	// Check to see if we actually need to update the binning
	if ( NewRoiBinningH != m_pvtRoiBinningH )
	{
		// Reset the camera
		ResetSystemNoFlush();

		UseOppositePatterns = false;
		if ( (m_ApnSensorInfo->m_CameraId >= 256) && (m_ApnSensorInfo->m_DefaultSpeed == 0x0) )
		{
			UseOppositePatterns = true;
		}

		LoadRoiPattern( UseOppositePatterns, NewRoiBinningH );
		m_pvtRoiBinningH = NewRoiBinningH;

		// Reset the camera and start flushing
		ResetSystem();
	}
}

unsigned short CApnCamera::read_RoiBinningV()
{
	return m_pvtRoiBinningV;
}

void CApnCamera::write_RoiBinningV( unsigned short RoiBinningV )
{
	unsigned short NewRoiBinningV;


	// Do some bounds checking on our input parameter
	if ( RoiBinningV == 0 )
		NewRoiBinningV = 1;
	if ( RoiBinningV > read_MaxBinningV() )
		NewRoiBinningV = read_MaxBinningV();
	else
		NewRoiBinningV = RoiBinningV;

	// Check to see if we actually need to update the binning
	if ( NewRoiBinningV != m_pvtRoiBinningV )
	{
		m_pvtRoiBinningV = NewRoiBinningV;
	}
}

unsigned short CApnCamera::read_RoiPixelsH()
{
	return m_pvtRoiPixelsH;
}

void CApnCamera::write_RoiPixelsH( unsigned short RoiPixelsH )
{
	unsigned short NewRoiPixelsH;


	if ( RoiPixelsH == 0 )
		NewRoiPixelsH = 1;
	else
		NewRoiPixelsH = RoiPixelsH;

	m_pvtRoiPixelsH = NewRoiPixelsH;
}

unsigned short CApnCamera::read_RoiPixelsV()
{
	return m_pvtRoiPixelsV;
}

void CApnCamera::write_RoiPixelsV( unsigned short RoiPixelsV )
{
	unsigned short NewRoiPixelsV;


	if ( RoiPixelsV == 0 )
		NewRoiPixelsV = 1;
	else
		NewRoiPixelsV = RoiPixelsV;

	m_pvtRoiPixelsV = NewRoiPixelsV;
}

unsigned short CApnCamera::read_RoiStartX()
{
	return m_pvtRoiStartX;
}

void CApnCamera::write_RoiStartX( unsigned short RoiStartX )
{
	m_pvtRoiStartX = RoiStartX;
}

unsigned short CApnCamera::read_RoiStartY()
{
	return m_pvtRoiStartY;
}

void CApnCamera::write_RoiStartY( unsigned short RoiStartY )
{
	m_pvtRoiStartY = RoiStartY;
}

bool CApnCamera::read_DigitizeOverscan()
{
	return m_pvtDigitizeOverscan;
}

void CApnCamera::write_DigitizeOverscan( bool DigitizeOverscan )
{
	m_pvtDigitizeOverscan = DigitizeOverscan;
}

unsigned short CApnCamera::read_OverscanColumns()
{
	return m_ApnSensorInfo->m_OverscanColumns;
}

unsigned short CApnCamera::read_MaxBinningH()
{
	return m_PlatformHBinningMax;
}

unsigned short CApnCamera::read_MaxBinningV()
{
	if ( m_ApnSensorInfo->m_ImagingRows < m_PlatformVBinningMax )
		return m_ApnSensorInfo->m_ImagingRows;
	else
		return m_PlatformVBinningMax;
}

unsigned short CApnCamera::read_SequenceCounter()
{
	UpdateGeneralStatus();

	if ( m_pvtSequenceBulkDownload )
		return m_pvtSequenceCounter;
	else
		return m_pvtReadyFrame;
}

bool CApnCamera::read_ContinuousImaging()
{
	// CI requires v17 or higher firmware support
	if ( m_pvtFirmwareVersion < 17 )
		return false;


	unsigned short RegVal;
	Read( FPGA_REG_OP_B, RegVal );
	return ( (RegVal & FPGA_BIT_CONT_IMAGE_ENABLE) == 1 );	
}

void CApnCamera::write_ContinuousImaging( bool ContinuousImaging )
{
	// CI requires v17 or higher firmware support
	if ( m_pvtFirmwareVersion < 17 )
		return;


	unsigned short RegVal;

	Read( FPGA_REG_OP_B, RegVal );

	if ( ContinuousImaging )
		RegVal |= FPGA_BIT_CONT_IMAGE_ENABLE;		// enable
	else
		RegVal &= ~FPGA_BIT_CONT_IMAGE_ENABLE;		// disable

	Write( FPGA_REG_OP_B, RegVal );
}

unsigned short CApnCamera::read_TDICounter()
{
	unsigned short Counter;

	char szOutputText[128];

	AltaDebugOutputString( "APOGEE.DLL - CApnCamera::read_TDICounter()" );


	UpdateGeneralStatus();

	if ( m_pvtSequenceBulkDownload )
		Counter = m_pvtTDICounter;
	else
		Counter = m_pvtReadyFrame;

	AltaDebugPrint( szOutputText, "APOGEE.DLL - CApnCamera::read_TDICounter() returning %d", Counter );
	AltaDebugOutputString( szOutputText );

	return Counter;
}

unsigned short CApnCamera::read_TDIRows()
{
	char szOutputText[128];

	AltaDebugOutputString( "APOGEE.DLL - CApnCamera::read_TDIRows()" );

	return m_pvtTDIRows;

	AltaDebugPrint( szOutputText, "APOGEE.DLL - CApnCamera::read_TDIRows() returning %d", m_pvtTDIRows );
	AltaDebugOutputString( szOutputText );
}

void CApnCamera::write_TDIRows( unsigned short TdiRows )
{
	if ( TdiRows == 0 )		// Make sure the TDI row count is at least 1
		TdiRows = 1;

	Write( FPGA_REG_TDI_COUNT, TdiRows );
	m_pvtTDIRows = TdiRows;
}

double CApnCamera::read_TDIRate()
{
	return m_pvtTDIRate;
}

void CApnCamera::write_TDIRate( double TdiRate )
{
	unsigned short RegVal;

	if ( TdiRate < m_PlatformTdiRateMin )
		TdiRate = m_PlatformTdiRateMin;
	
	if ( TdiRate > m_PlatformTdiRateMax )
		TdiRate = m_PlatformTdiRateMax;

	RegVal = (unsigned short)( TdiRate / m_PlatformTdiRateResolution );

	Write( FPGA_REG_TDI_RATE, RegVal );

	m_pvtTDIRate = TdiRate;
}

unsigned short CApnCamera::read_TDIBinningV()
{
	return m_pvtTDIBinningV;
}

void CApnCamera::write_TDIBinningV( unsigned short TdiBinningV )
{
	if ( TdiBinningV == 0 )		// Make sure the TDI binning is at least 1
		TdiBinningV = 1;

	Write( FPGA_REG_TDI_BINNING, TdiBinningV );
	m_pvtTDIBinningV = TdiBinningV;
}

unsigned short CApnCamera::read_KineticsSections()
{
	return read_TDIRows();
}

void CApnCamera::write_KineticsSections( unsigned short KineticsSections )
{
	write_TDIRows( KineticsSections );
}

double CApnCamera::read_KineticsShiftInterval()
{
	return read_TDIRate();
}

void CApnCamera::write_KineticsShiftInterval( double KineticsShiftInterval )
{
	write_TDIRate( KineticsShiftInterval );
}

unsigned short CApnCamera::read_KineticsSectionHeight()
{
	return read_TDIBinningV();
}

void CApnCamera::write_KineticsSectionHeight( unsigned short KineticsSectionHeight )
{
	write_TDIBinningV( KineticsSectionHeight );
}

bool CApnCamera::read_TriggerNormalEach()
{
	// This trigger option requires v17 or higher firmware on Alta
	if ( (m_pvtPlatformType == Apn_Platform_Alta) && (m_pvtFirmwareVersion < 17) )
		return false;


	unsigned short RegVal;
	Read( FPGA_REG_OP_C, RegVal );
	return ( (RegVal & FPGA_BIT_IMAGE_TRIGGER_EACH) == 1 );	
}

void CApnCamera::write_TriggerNormalEach( bool TriggerNormalEach )
{
	// This trigger option requires v17 or higher firmware on Alta
	if ( (m_pvtPlatformType == Apn_Platform_Alta) && (m_pvtFirmwareVersion < 17) )
		return;

	// do not allow triggers on each progressive scanned image
	if ( m_pvtFastSequence )
		return;

	// If our current state var is equal to the new state, no need to do anything
	if ( m_pvtTriggerNormalEach == TriggerNormalEach )
		return;


	unsigned short RegVal, IoRegVal;

	Read( FPGA_REG_OP_C, RegVal );

	if ( TriggerNormalEach )
	{
		RegVal |= FPGA_BIT_IMAGE_TRIGGER_EACH;	// Enable

		// We only need to set the I/O port bit if it hasn't been set already
		if ( (!m_pvtTriggerNormalGroup) && (!m_pvtTriggerTdiKineticsEach) && (!m_pvtTriggerTdiKineticsGroup) )
		{
			IoRegVal = read_IoPortAssignment();
			
			if ( m_pvtPlatformType == Apn_Platform_Alta )
				IoRegVal |= 0x01;						// External trigger is pin one (bit zero)
			else if ( m_pvtPlatformType == Apn_Platform_Ascent )
				IoRegVal &= 0x02;

			write_IoPortAssignment( IoRegVal );
		}
	}
	else
	{
		RegVal &= ~FPGA_BIT_IMAGE_TRIGGER_EACH;	// Disable

		// We only disable the I/O port bit if it is unused
		if ( (!m_pvtTriggerNormalGroup) && (!m_pvtTriggerTdiKineticsEach) && (!m_pvtTriggerTdiKineticsGroup) )
		{
			IoRegVal = read_IoPortAssignment();
			IoRegVal &= 0x3E;						// External trigger is pin one (bit zero)
			write_IoPortAssignment( IoRegVal );
		}
	}

	Write( FPGA_REG_OP_C, RegVal );

	m_pvtTriggerNormalEach = TriggerNormalEach;
}

bool CApnCamera::read_TriggerNormalGroup()
{
	// This trigger option requires v17 or higher firmware on Alta
	if ( (m_pvtPlatformType == Apn_Platform_Alta) && (m_pvtFirmwareVersion < 17) )
		return false;


	unsigned short RegVal;
	Read( FPGA_REG_OP_C, RegVal );
	return ( (RegVal & FPGA_BIT_IMAGE_TRIGGER_GROUP) == 1 );	
}

void CApnCamera::write_TriggerNormalGroup( bool TriggerNormalGroup )
{
	// This trigger option requires v17 or higher firmware on Alta
	if ( (m_pvtPlatformType == Apn_Platform_Alta) && (m_pvtFirmwareVersion < 17) )
		return;

	// If our current state var is equal to the new state, no need to do anything
	if ( m_pvtTriggerNormalGroup == TriggerNormalGroup )
		return;


	unsigned short RegVal, IoRegVal;

	Read( FPGA_REG_OP_C, RegVal );

	if ( TriggerNormalGroup )
	{
		RegVal |= FPGA_BIT_IMAGE_TRIGGER_GROUP;		// enable

		// We only need to set the I/O port bit if it hasn't been set already
		if ( (!m_pvtTriggerNormalEach) && (!m_pvtTriggerTdiKineticsEach) && (!m_pvtTriggerTdiKineticsGroup) )
		{
			IoRegVal = read_IoPortAssignment();
			
			if ( m_pvtPlatformType == Apn_Platform_Alta )
				IoRegVal |= 0x01;						// External trigger is pin one (bit zero)
			else if ( m_pvtPlatformType == Apn_Platform_Ascent )
				IoRegVal &= 0x02;

			write_IoPortAssignment( IoRegVal );
		}
	}
	else
	{
		RegVal &= ~FPGA_BIT_IMAGE_TRIGGER_GROUP;		// disable

		// We only disable the I/O port bit if it is unused
		if ( (!m_pvtTriggerNormalEach) && (!m_pvtTriggerTdiKineticsEach) && (!m_pvtTriggerTdiKineticsGroup) )
		{
			IoRegVal = read_IoPortAssignment();
			IoRegVal &= 0x3E;						// External trigger is pin one (bit zero)
			write_IoPortAssignment( IoRegVal );
		}
	}

	Write( FPGA_REG_OP_C, RegVal );

	m_pvtTriggerNormalGroup = TriggerNormalGroup;
}

bool CApnCamera::read_TriggerTdiKineticsEach()
{
	// This trigger option requires v17 or higher firmware on Alta
	if ( (m_pvtPlatformType == Apn_Platform_Alta) && (m_pvtFirmwareVersion < 17) )
		return false;


	unsigned short RegVal;
	Read( FPGA_REG_OP_C, RegVal );
	return ( (RegVal & FPGA_BIT_TDI_TRIGGER_EACH) == 1 );	
}

void CApnCamera::write_TriggerTdiKineticsEach( bool TriggerTdiKineticsEach )
{
	// This trigger option requires v17 or higher firmware on Alta
	if ( (m_pvtPlatformType == Apn_Platform_Alta) && (m_pvtFirmwareVersion < 17) )
		return;

	// If our current state var is equal to the new state, no need to do anything
	if ( m_pvtTriggerTdiKineticsEach == TriggerTdiKineticsEach )
		return;


	unsigned short RegVal, IoRegVal;

	Read( FPGA_REG_OP_C, RegVal );

	if ( TriggerTdiKineticsEach )
	{
		RegVal |= FPGA_BIT_TDI_TRIGGER_EACH;		// enable

		// We only need to set the I/O port bit if it hasn't been set already
		if ( (!m_pvtTriggerNormalEach) && (!m_pvtTriggerNormalGroup) && (!m_pvtTriggerTdiKineticsGroup) )
		{
			IoRegVal = read_IoPortAssignment();
			
			if ( m_pvtPlatformType == Apn_Platform_Alta )
				IoRegVal |= 0x01;						// External trigger is pin one (bit zero)
			else if ( m_pvtPlatformType == Apn_Platform_Ascent )
				IoRegVal &= 0x02;

			write_IoPortAssignment( IoRegVal );
		}
	}
	else
	{
		RegVal &= ~FPGA_BIT_TDI_TRIGGER_EACH;		// disable

		// We only disable the I/O port bit if it is unused
		if ( (!m_pvtTriggerNormalEach) && (!m_pvtTriggerNormalGroup) && (!m_pvtTriggerTdiKineticsGroup) )
		{
			IoRegVal = read_IoPortAssignment();
			IoRegVal &= 0x3E;						// External trigger is pin one (bit zero)
			write_IoPortAssignment( IoRegVal );
		}
	}

	Write( FPGA_REG_OP_C, RegVal );

	m_pvtTriggerTdiKineticsEach = TriggerTdiKineticsEach;
}

bool CApnCamera::read_TriggerTdiKineticsGroup()
{
	// This trigger option requires v17 or higher firmware on Alta
	if ( (m_pvtPlatformType == Apn_Platform_Alta) && (m_pvtFirmwareVersion < 17) )
		return false;


	unsigned short RegVal;
	Read( FPGA_REG_OP_C, RegVal );
	return ( (RegVal & FPGA_BIT_TDI_TRIGGER_GROUP) == 1 );
}

void CApnCamera::write_TriggerTdiKineticsGroup( bool TriggerTdiKineticsGroup )
{
	// This trigger option requires v17 or higher firmware on Alta
	if ( (m_pvtPlatformType == Apn_Platform_Alta) && (m_pvtFirmwareVersion < 17) )
		return;

	// If our current state var is equal to the new state, no need to do anything
	if ( m_pvtTriggerTdiKineticsGroup == TriggerTdiKineticsGroup )
		return;


	unsigned short RegVal, IoRegVal;

	Read( FPGA_REG_OP_C, RegVal );

	if ( TriggerTdiKineticsGroup )
	{
		RegVal |= FPGA_BIT_TDI_TRIGGER_GROUP;		// enable

		// We only need to set the I/O port bit if it hasn't been set already
		if ( (!m_pvtTriggerNormalEach) && (!m_pvtTriggerNormalGroup) && (!m_pvtTriggerTdiKineticsEach) )
		{
			IoRegVal = read_IoPortAssignment();
			
			if ( m_pvtPlatformType == Apn_Platform_Alta )
				IoRegVal |= 0x01;						// External trigger is pin one (bit zero)
			else if ( m_pvtPlatformType == Apn_Platform_Ascent )
				IoRegVal &= 0x02;

			write_IoPortAssignment( IoRegVal );
		}
	}
	else
	{
		RegVal &= ~FPGA_BIT_TDI_TRIGGER_GROUP;		// disable

		// We only disable the I/O port bit if it is unused
		if ( (!m_pvtTriggerNormalEach) && (!m_pvtTriggerNormalGroup) && (!m_pvtTriggerTdiKineticsEach) )
		{
			IoRegVal = read_IoPortAssignment();
			IoRegVal &= 0x3E;						// External trigger is pin one (bit zero)
			write_IoPortAssignment( IoRegVal );
		}
	}

	Write( FPGA_REG_OP_C, RegVal );

	m_pvtTriggerTdiKineticsGroup = TriggerTdiKineticsGroup;
}

bool CApnCamera::read_ExposureTriggerEach()
{
	return m_pvtExposureTriggerEach;
}

bool CApnCamera::read_ExposureTriggerGroup()
{
	return m_pvtExposureTriggerGroup;
}

bool CApnCamera::read_ExposureExternalShutter()
{
	return m_pvtExposureExternalShutter;
}

unsigned short CApnCamera::read_IoPortAssignment()
{
	return m_pvtIoPortAssignment;
}

void CApnCamera::write_IoPortAssignment( unsigned short IoPortAssignment )
{
	if ( m_pvtPlatformType == Apn_Platform_Alta )
	{
		IoPortAssignment &= FPGA_MASK_IO_PORT_ASSIGNMENT_ALTA;
		Write( FPGA_REG_IO_PORT_ASSIGNMENT_ALTA, IoPortAssignment );
	}
	else if ( m_pvtPlatformType == Apn_Platform_Ascent )
	{
		IoPortAssignment &= FPGA_MASK_IO_PORT_ASSIGNMENT_ASCENT;
		Write( FPGA_REG_IO_PORT_ASSIGNMENT_ASCENT, IoPortAssignment );
	}

	m_pvtIoPortAssignment = IoPortAssignment;
}

unsigned short CApnCamera::read_IoPortDirection()
{
	unsigned short RetVal;

	RetVal = 0x0;

	if ( m_pvtPlatformType == Apn_Platform_Alta )
		RetVal = m_pvtIoPortDirection;
	else if ( m_pvtPlatformType == Apn_Platform_Ascent )
		RetVal = 0x0;

	return RetVal;
}

void CApnCamera::write_IoPortDirection( unsigned short IoPortDirection )
{
	if ( m_pvtPlatformType == Apn_Platform_Alta )
	{
		IoPortDirection &= FPGA_MASK_IO_PORT_DIRECTION;
		Write( FPGA_REG_IO_PORT_DIRECTION, IoPortDirection );
		m_pvtIoPortDirection = IoPortDirection;
	}
}

unsigned short CApnCamera::read_IoPortData()
{
	unsigned short RegVal;
	
	if ( m_pvtPlatformType == Apn_Platform_Alta )
	{
		Read( FPGA_REG_IO_PORT_READ, RegVal );
		RegVal &= FPGA_MASK_IO_PORT_DATA;
	}
	else if ( m_pvtPlatformType == Apn_Platform_Ascent )
	{
		RegVal = 0x0;
	}

	return RegVal;
}

void CApnCamera::write_IoPortData( unsigned short IoPortData )
{
	if ( m_pvtPlatformType == Apn_Platform_Alta )
	{
		IoPortData &= FPGA_MASK_IO_PORT_DATA;
		Write( FPGA_REG_IO_PORT_WRITE, IoPortData );
	}
}

unsigned short CApnCamera::read_TwelveBitGain()
{
	unsigned short RetVal;

	if ( m_pvtPlatformType == Apn_Platform_Alta )
		RetVal = m_pvtTwelveBitGain;
	else if ( m_pvtPlatformType == Apn_Platform_Ascent )
		RetVal = 0x0;

	return RetVal;
}

void CApnCamera::write_TwelveBitGain( unsigned short TwelveBitGain )
{
	unsigned short NewVal;
	unsigned short StartVal;
	unsigned short FirstBit;


	if ( m_pvtPlatformType == Apn_Platform_Alta )
	{
		NewVal		= 0x0;
		StartVal	= TwelveBitGain & 0x3FF;

		for ( int i=0; i<10; i++ )
		{
			FirstBit = ( StartVal & 0x0001 );
			NewVal |= ( FirstBit << (10-i) );
			StartVal = StartVal >> 1;
		}

		NewVal |= 0x4000;

		Write( FPGA_REG_AD_CONFIG_DATA, NewVal );
		Write( FPGA_REG_COMMAND_B,		0x8000 );

		m_pvtTwelveBitGain = TwelveBitGain & 0x3FF;
	}
}

unsigned short CApnCamera::read_TwelveBitOffset()
{
	unsigned short RetVal;

	if ( m_pvtPlatformType == Apn_Platform_Alta )
		RetVal = m_pvtTwelveBitOffset;
	else if ( m_pvtPlatformType == Apn_Platform_Ascent )
		RetVal = 0x0;

	return RetVal;
}

void CApnCamera::write_TwelveBitOffset( unsigned short TwelveBitOffset )
{
	unsigned short NewVal;
	unsigned short StartVal;
	unsigned short FirstBit;


	if ( m_pvtPlatformType == Apn_Platform_Alta )
	{
		NewVal		= 0x0;
		StartVal	= TwelveBitOffset & 0xFF;

		for ( int i=0; i<8; i++ )
		{
			FirstBit = ( StartVal & 0x0001 );
			NewVal |= ( FirstBit << (10-i) );
			StartVal = StartVal >> 1;
		}

		NewVal |= 0x2000;

		Write( FPGA_REG_AD_CONFIG_DATA, NewVal );
		Write( FPGA_REG_COMMAND_B,		FPGA_BIT_CMD_AD_CONFIG );

		m_pvtTwelveBitOffset = TwelveBitOffset;
	}
}

double CApnCamera::read_MaxExposureTime()
{
	return m_PlatformExposureTimeMax;
}

double CApnCamera::read_TestLedBrightness()
{
	return m_pvtTestLedBrightness;
}

void CApnCamera::write_TestLedBrightness( double TestLedBrightness )
{
	unsigned short	RegVal;
	unsigned short	OpRegA;


	if ( TestLedBrightness == m_pvtTestLedBrightness )
		return;

	if ( m_pvtCoolerEnable )
	{
		Read( FPGA_REG_OP_A, OpRegA );
		OpRegA |= FPGA_BIT_TEMP_SUSPEND;
		Write( FPGA_REG_OP_A, OpRegA );

		do 
		{ 
			Read( FPGA_REG_GENERAL_STATUS, RegVal );
		} while ( (RegVal & FPGA_BIT_STATUS_TEMP_SUSPEND_ACK) == 0 );
		
	}

	if ( m_pvtPlatformType == Apn_Platform_Alta )
	{
		RegVal = (unsigned short)( (double)FPGA_MASK_LED_ILLUMINATION_ALTA * (TestLedBrightness/100.0) );
	}
	else if ( m_pvtPlatformType == Apn_Platform_Ascent )
	{
		RegVal = (unsigned short)( (double)FPGA_MASK_LED_ILLUMINATION_ASCENT * (TestLedBrightness/100.0) );
	}

	Write( FPGA_REG_LED_DRIVE, RegVal );

	Read( FPGA_REG_OP_B, RegVal );
	RegVal &= ~FPGA_BIT_DAC_SELECT_ZERO;
	RegVal |= FPGA_BIT_DAC_SELECT_ONE;
	Write( FPGA_REG_OP_B, RegVal );

	Write( FPGA_REG_COMMAND_B, FPGA_BIT_CMD_DAC_LOAD );

	m_pvtTestLedBrightness = TestLedBrightness;

	if ( m_pvtCoolerEnable )
	{
		OpRegA &= ~FPGA_BIT_TEMP_SUSPEND;
		Write( FPGA_REG_OP_A, OpRegA );
	}
}

Apn_Platform CApnCamera::read_PlatformType()
{
	return m_pvtPlatformType;
}

unsigned short CApnCamera::read_AscentADGainSixteenLeft()
{
	if ( m_pvtPlatformType == Apn_Platform_Alta )
		return 0;


	return m_pvtAscentSixteenBitGainLeft;
}

unsigned short CApnCamera::read_AscentADGainSixteenRight()
{
	if ( m_pvtPlatformType == Apn_Platform_Alta )
		return 0;


	return m_pvtAscentSixteenBitGainRight;
}

void CApnCamera::write_AscentADGainSixteen( unsigned short GainValue )
{
	if ( m_pvtPlatformType == Apn_Platform_Alta )
		return;

	
	unsigned short NewVal;

	NewVal = GainValue & 0x003F;

	NewVal |= 0x2000;

	Write( FPGA_REG_AD_CONFIG_DATA, NewVal );
	Write( FPGA_REG_COMMAND_B,		FPGA_BIT_CMD_AD_CONFIG );
}

unsigned short CApnCamera::read_AscentADOffsetSixteenLeft()
{
	if ( m_pvtPlatformType == Apn_Platform_Alta )
		return 0;

	
	return m_pvtAscentSixteenBitOffsetLeft;
}

unsigned short CApnCamera::read_AscentADOffsetSixteenRight()
{
	if ( m_pvtPlatformType == Apn_Platform_Alta )
		return 0;


	return m_pvtAscentSixteenBitOffsetRight;
}

void CApnCamera::write_AscentADOffsetSixteen( unsigned short OffsetValue )
{
	if ( m_pvtPlatformType == Apn_Platform_Alta )
		return;


	unsigned short NewVal;

	NewVal = OffsetValue & 0x01FF;

	NewVal |= 0x5000;

	Write( FPGA_REG_AD_CONFIG_DATA, NewVal );
	Write( FPGA_REG_COMMAND_B,		FPGA_BIT_CMD_AD_CONFIG );
}

unsigned short CApnCamera::read_DigitizationSpeed()
{
	if ( m_pvtPlatformType == Apn_Platform_Alta )
		return 0;


	unsigned short RegVal;
	Read( FPGA_REG_OP_C, RegVal );
	return ( (RegVal & FPGA_MASK_HCLK) >> 4 );	
}

void CApnCamera::write_DigitizationSpeed( unsigned short DigitizationSpeed )
{
	if ( m_pvtPlatformType == Apn_Platform_Alta )
		return;

	
	unsigned short RegVal;
	unsigned short TempHclkValue;

	TempHclkValue = DigitizationSpeed & 0x7;
	TempHclkValue = TempHclkValue << 4;

	Read( FPGA_REG_OP_C, RegVal );
	RegVal &= ~FPGA_MASK_HCLK;
	RegVal |= TempHclkValue;
	Write( FPGA_REG_OP_C, RegVal );
}

/*
bool CApnCamera::FilterInit( Apn_Filter FilterType )
{
	char szOutputText[128];
	sprintf( szOutputText, "APOGEE.DLL - CApnCamera::FilterInit( FilterType = %d)", FilterType );
	AltaDebugOutputString( szOutputText );

	if ( m_pvtPlatformType == Apn_Platform_Alta )
		return true;

	if ( (FilterType != Apn_Filter_AFW30_7R) && (FilterType != Apn_Filter_AFW50_5R) &&
		 (FilterType != Apn_Filter_AFW25_4R) )
	{
		return false;
	}

	switch ( FilterType )
	{
		case Apn_Filter_AFW30_7R:
			m_pvtFilterWheelType	= Apn_Filter_AFW30_7R;
			m_pvtFilterMaxPositions	= APN_FILTER_AFW30_7R_MAX_POSITIONS;
			break;
		case Apn_Filter_AFW50_5R:
			m_pvtFilterWheelType	= Apn_Filter_AFW50_5R;
			m_pvtFilterMaxPositions	= APN_FILTER_AFW50_5R_MAX_POSITIONS;
			break;
		case Apn_Filter_AFW25_4R:
			m_pvtFilterWheelType	= Apn_Filter_AFW25_4R;
			m_pvtFilterMaxPositions	= APN_FILTER_AFW25_4R_MAX_POSITIONS;
			break;
		default:
			m_pvtFilterWheelType	= Apn_Filter_Unknown;
			m_pvtFilterMaxPositions	= APN_FILTER_UNKNOWN_MAX_POSITIONS;
			return false;
			break;
	}

	return true;
}

bool CApnCamera::FilterClose()
{
	AltaDebugOutputString( "APOGEE.DLL - CApnCamera::FilterClose()" );

	if ( m_pvtPlatformType == Apn_Platform_Alta )
		return true;

	m_pvtFilterWheelType	= Apn_Filter_Unknown;
	m_pvtFilterMaxPositions	= APN_FILTER_UNKNOWN_MAX_POSITIONS;

	return true;
}

unsigned short CApnCamera::read_FilterPosition()
{
	if ( m_pvtPlatformType == Apn_Platform_Alta )
		return 0;

	if ( m_pvtFilterWheelType == Apn_Filter_Unknown )
		return 0;


	unsigned short RegVal;
	Read( FPGA_REG_OP_C, RegVal );
	return ( (RegVal & FPGA_MASK_FILTER_POSITION) >> 8 );	
}

void CApnCamera::write_FilterPosition( unsigned short FilterPosition )
{
	if ( m_pvtPlatformType == Apn_Platform_Alta )
		return;

	if ( m_pvtFilterWheelType == Apn_Filter_Unknown )
		return;

	if ( FilterPosition >= m_pvtFilterMaxPositions )
		return;

	unsigned short RegVal;
	unsigned short TempFilterPosition;

	TempFilterPosition = FilterPosition & 0x7;
	TempFilterPosition = FilterPosition << 8;

	Read( FPGA_REG_OP_C, RegVal );
	RegVal &= ~FPGA_MASK_FILTER_POSITION;
	RegVal |= TempFilterPosition;
	Write( FPGA_REG_OP_C, RegVal );
}

bool CApnCamera::read_FilterReady()
{
	if ( m_pvtPlatformType == Apn_Platform_Alta )
		return false;

	if ( m_pvtFilterWheelType == Apn_Filter_Unknown )
		return false;


	UpdateGeneralStatus();

	return ( (m_pvtStatusReg & FPGA_BIT_STATUS_FILTER_SENSE) != 0 );
}

unsigned short CApnCamera::read_FilterMaxPositions()
{
	if ( m_pvtPlatformType == Apn_Platform_Alta )
		return 0;

	return m_pvtFilterMaxPositions;
}

Apn_Filter CApnCamera::read_FilterWheelType()
{
	return m_pvtFilterWheelType;
}

void CApnCamera::read_FilterModel( char *FilterModel, long *BufferLength )
{
	// we don't check platform type or anything for this property because
	// we want to return something, no matter what.  we key off the private
	// variable m_pvtFilterWheelType, and we'll just return "Unknown"
	// for anything other than what we support

	char szModel[256];

	switch ( m_pvtFilterWheelType )
	{
		case Apn_Filter_AFW30_7R:
			strcpy( szModel, APN_FILTER_AFW30_7R_DESCR );
			break;
		case Apn_Filter_AFW50_5R:
			strcpy( szModel, APN_FILTER_AFW50_5R_DESCR );
			break;
		case Apn_Filter_AFW25_4R:
			strcpy( szModel, APN_FILTER_AFW25_4R_DESCR );
			break;
		default:
			strcpy( szModel, "Unknown" );
			break;
	}

	*BufferLength = strlen( szModel );

	strncpy( FilterModel, szModel, *BufferLength + 1);
}

*/

bool CApnCamera::read_DataAveraging()
{
	if ( m_pvtPlatformType == Apn_Platform_Alta )
		return false;


	unsigned short RegVal;
	Read( FPGA_REG_OP_B, RegVal );
	return ( (RegVal & FPGA_BIT_AD_AVERAGING) != 0 );	
}

void CApnCamera::write_DataAveraging( bool DataAveraging )
{
	if ( m_pvtPlatformType == Apn_Platform_Alta )

		return;

	unsigned short RegVal;

	Read( FPGA_REG_OP_B, RegVal );

	if ( DataAveraging )
		RegVal |= FPGA_BIT_AD_AVERAGING;		// enable
	else
		RegVal &= ~FPGA_BIT_AD_AVERAGING;		// disable

	Write( FPGA_REG_OP_B, RegVal );
}

bool CApnCamera::read_DualReadout()
{
	if ( m_pvtPlatformType == Apn_Platform_Alta )
		return false;


	return m_pvtDualReadout;
}

void CApnCamera::write_DualReadout( bool DualReadout )
{
	if ( m_pvtPlatformType == Apn_Platform_Alta )
	{
		m_pvtDualReadout = false;
		return;
	}

	unsigned short RegVal;

	Read( FPGA_REG_OP_A, RegVal );

	if ( DualReadout )
		RegVal |= FPGA_BIT_DUAL_AD_READOUT;		// enable
	else
		RegVal &= ~FPGA_BIT_DUAL_AD_READOUT;	// disable

	Write( FPGA_REG_OP_A, RegVal );

	m_pvtDualReadout = DualReadout;
}

bool CApnCamera::read_ConnectionTest()
{
	unsigned short	RegData;
	unsigned short	NewRegData;

	RegData = 0x5AA5;
	Write( FPGA_REG_SCRATCH, RegData );
	Read( FPGA_REG_SCRATCH, NewRegData );
	if ( RegData != NewRegData ) 
		return false;

	RegData = 0xA55A;
	Write( FPGA_REG_SCRATCH, RegData );
	Read( FPGA_REG_SCRATCH, NewRegData );
	if ( RegData != NewRegData ) 
		return false;

	return true;
}

bool CApnCamera::read_GuideActive()
{
	if ( m_pvtPlatformType == Apn_Platform_Alta )
		return false;


	UpdateGeneralStatus();

	return ( (m_pvtStatusReg & FPGA_BIT_STATUS_GUIDE_ACTIVE) != 0 );
}

double CApnCamera::read_GuideRAPlusDuration()
{
	double RetVal;

	RetVal = 0;

	if ( m_pvtPlatformType == Apn_Platform_Alta )
		RetVal = 0;
	else if ( m_pvtPlatformType == Apn_Platform_Ascent )
		RetVal = m_pvtGuideRAPlusDuration;

	return RetVal;
}

void CApnCamera::write_GuideRAPlusDuration( double GuideRAPlusDuration )
{
	unsigned short	RegVal;
	double			RelayDuration;


	if ( m_pvtPlatformType == Apn_Platform_Alta )
	{
		return;
	}
	else if ( m_pvtPlatformType == Apn_Platform_Ascent )
	{
		RelayDuration	= CheckGuiderRelayDuration( GuideRAPlusDuration );

		RegVal			= CalculateGuiderRelayTimeCounts( RelayDuration );

		Write( FPGA_REG_GUIDE_RA_PLUS, RegVal );

		m_pvtGuideRAPlusDuration = RelayDuration;
	}
}

double CApnCamera::read_GuideRAMinusDuration()
{
	double RetVal;

	RetVal = 0;

	if ( m_pvtPlatformType == Apn_Platform_Alta )
		RetVal = 0;
	else if ( m_pvtPlatformType == Apn_Platform_Ascent )
		RetVal = m_pvtGuideRAMinusDuration;

	return RetVal;
}

void CApnCamera::write_GuideRAMinusDuration( double GuideRAMinusDuration )
{
	unsigned short	RegVal;
	double			RelayDuration;


	if ( m_pvtPlatformType == Apn_Platform_Alta )
	{
		return;
	}
	else if ( m_pvtPlatformType == Apn_Platform_Ascent )
	{
		RelayDuration	= CheckGuiderRelayDuration( GuideRAMinusDuration );

		RegVal			= CalculateGuiderRelayTimeCounts( RelayDuration );

		Write( FPGA_REG_GUIDE_RA_MINUS, RegVal );

		m_pvtGuideRAMinusDuration = RelayDuration;
	}
}

double CApnCamera::read_GuideDecPlusDuration()
{
	double RetVal;

	RetVal = 0;

	if ( m_pvtPlatformType == Apn_Platform_Alta )
		RetVal = 0;
	else if ( m_pvtPlatformType == Apn_Platform_Ascent )
		RetVal = m_pvtGuideDecPlusDuration;

	return RetVal;
}

void CApnCamera::write_GuideDecPlusDuration( double GuideDecPlusDuration )
{
	unsigned short	RegVal;
	double			RelayDuration;


	if ( m_pvtPlatformType == Apn_Platform_Alta )
	{
		return;
	}
	else if ( m_pvtPlatformType == Apn_Platform_Ascent )
	{
		RelayDuration	= CheckGuiderRelayDuration( GuideDecPlusDuration );

		RegVal			= CalculateGuiderRelayTimeCounts( RelayDuration );

		Write( FPGA_REG_GUIDE_DEC_PLUS, RegVal );

		m_pvtGuideDecPlusDuration = RelayDuration;
	}
}

double CApnCamera::read_GuideDecMinusDuration()
{
	double RetVal;

	RetVal = 0;

	if ( m_pvtPlatformType == Apn_Platform_Alta )
		RetVal = 0;
	else if ( m_pvtPlatformType == Apn_Platform_Ascent )
		RetVal = m_pvtGuideDecMinusDuration;

	return RetVal;
}

void CApnCamera::write_GuideDecMinusDuration( double GuideDecMinusDuration )
{
	unsigned short	RegVal;
	double			RelayDuration;


	if ( m_pvtPlatformType == Apn_Platform_Alta )
	{
		return;
	}
	else if ( m_pvtPlatformType == Apn_Platform_Ascent )
	{
		RelayDuration	= CheckGuiderRelayDuration( GuideDecMinusDuration );

		RegVal			= CalculateGuiderRelayTimeCounts( RelayDuration );

		Write( FPGA_REG_GUIDE_DEC_MINUS, RegVal );

		m_pvtGuideDecMinusDuration = RelayDuration;
	}
}

Apn_BayerShift CApnCamera::read_BayerStartPosition()
{
	return m_pvtBayerShift;
}

void CApnCamera::write_BayerStartPosition( Apn_BayerShift BayerStartPosition )
{
	m_pvtBayerShift = BayerStartPosition;
}






long CApnCamera::LoadVerticalPattern()
{
	unsigned short RegData;

	// Prime the RAM (Enable)
	Read( FPGA_REG_OP_B, RegData );
	RegData |= FPGA_BIT_VRAM_ENABLE;
	Write( FPGA_REG_OP_B, RegData );

	WriteMultiSRMD( FPGA_REG_VRAM_INPUT, 
					m_ApnSensorInfo->m_VerticalPattern.PatternData,
					m_ApnSensorInfo->m_VerticalPattern.NumElements );

	// RAM is now loaded (Disable)
	Read( FPGA_REG_OP_B, RegData );
	RegData &= ~FPGA_BIT_VRAM_ENABLE;
	Write( FPGA_REG_OP_B, RegData );

	return 0;
}

long CApnCamera::LoadClampPattern( bool UseOppositePatterns )
{
	unsigned short RegData;

	// Prime the RAM (Enable)
	Read( FPGA_REG_OP_B, RegData );
	RegData |= FPGA_BIT_HCLAMP_ENABLE;
	Write( FPGA_REG_OP_B, RegData );

	if ( m_pvtDataBits == Apn_Resolution_SixteenBit )
	{
		if ( UseOppositePatterns )
		{
			WriteHorizontalPattern( &m_ApnSensorInfo->m_ClampPatternTwelve, 
									FPGA_REG_HCLAMP_INPUT, 
									1 );
		}
		else
		{
			WriteHorizontalPattern( &m_ApnSensorInfo->m_ClampPatternSixteen, 
									FPGA_REG_HCLAMP_INPUT, 
									1 );
		}
	}
	else if ( m_pvtDataBits == Apn_Resolution_TwelveBit )
	{
		WriteHorizontalPattern( &m_ApnSensorInfo->m_ClampPatternTwelve, 
								FPGA_REG_HCLAMP_INPUT, 
								1 );
	}

	// RAM is now loaded (Disable)
	Read( FPGA_REG_OP_B, RegData );
	RegData &= ~FPGA_BIT_HCLAMP_ENABLE;
	Write( FPGA_REG_OP_B, RegData );

	return 0;
}

long CApnCamera::LoadSkipPattern( bool UseOppositePatterns )
{
	unsigned short	RegData;

	// Prime the RAM (Enable)
	Read( FPGA_REG_OP_B, RegData );
	RegData |= FPGA_BIT_HSKIP_ENABLE;
	Write( FPGA_REG_OP_B, RegData );

	if ( m_pvtDataBits == Apn_Resolution_SixteenBit )
	{
		if ( UseOppositePatterns )
		{
			WriteHorizontalPattern( &m_ApnSensorInfo->m_SkipPatternTwelve, 
									FPGA_REG_HSKIP_INPUT, 
									1 );
		}
		else
		{
			WriteHorizontalPattern( &m_ApnSensorInfo->m_SkipPatternSixteen, 
									FPGA_REG_HSKIP_INPUT, 
									1 );
		}
	}
	else if ( m_pvtDataBits == Apn_Resolution_TwelveBit )
	{
		WriteHorizontalPattern( &m_ApnSensorInfo->m_SkipPatternTwelve, 
								FPGA_REG_HSKIP_INPUT, 
								1 );
	}

	// RAM is now loaded (Disable)
	Read( FPGA_REG_OP_B, RegData );
	RegData &= ~FPGA_BIT_HSKIP_ENABLE;
	Write( FPGA_REG_OP_B, RegData );

	return 0;
}

long CApnCamera::LoadRoiPattern( bool UseOppositePatterns, unsigned short binning )
{
	unsigned short	RegData;

	// Prime the RAM (Enable)
	Read( FPGA_REG_OP_B, RegData );
	RegData |= FPGA_BIT_HRAM_ENABLE;
	Write( FPGA_REG_OP_B, RegData );

	if ( m_pvtDataBits == Apn_Resolution_SixteenBit )
	{
		if ( UseOppositePatterns )
		{
			WriteHorizontalPattern( &m_ApnSensorInfo->m_RoiPatternTwelve, 
									FPGA_REG_HRAM_INPUT, 
									binning );
		}
		else
		{
			WriteHorizontalPattern( &m_ApnSensorInfo->m_RoiPatternSixteen, 
									FPGA_REG_HRAM_INPUT, 
									binning );
		}
	}
	else if ( m_pvtDataBits == Apn_Resolution_TwelveBit )
	{
		WriteHorizontalPattern( &m_ApnSensorInfo->m_RoiPatternTwelve, 
								FPGA_REG_HRAM_INPUT, 
								binning );
	}

	// RAM is now loaded (Disable)
	Read( FPGA_REG_OP_B, RegData );
	RegData &= ~FPGA_BIT_HRAM_ENABLE;
	Write( FPGA_REG_OP_B, RegData );

	return 0;
}

long CApnCamera::WriteHorizontalPattern( APN_HPATTERN_FILE *Pattern, 
										 unsigned short RamReg, 
										 unsigned short Binning )
{
	unsigned short	i;
	unsigned short	DataCount;
	unsigned short	*DataArray;
	unsigned short	Index;
	unsigned short	BinNumber;


	Index	  = 0;
	BinNumber = Binning - 1;				// arrays are zero-based

	DataCount = Pattern->RefNumElements +
				Pattern->BinNumElements[BinNumber] +
				Pattern->SigNumElements;

	DataArray = (unsigned short *)malloc(DataCount * sizeof(unsigned short));

	for ( i=0; i<Pattern->RefNumElements; i++ )
	{
		DataArray[Index] = Pattern->RefPatternData[i];
		Index++;
	}
	
	for ( i=0; i<Pattern->BinNumElements[BinNumber]; i++ )
	{
		DataArray[Index] = Pattern->BinPatternData[BinNumber][i];
		Index++;
	}

	for ( i=0; i<Pattern->SigNumElements; i++ )
	{
		DataArray[Index] = Pattern->SigPatternData[i];
		Index++;
	}

	WriteMultiSRMD( RamReg, DataArray, DataCount );

	// cleanup
	free( DataArray );

	return 0;
}

double CApnCamera::CheckGuiderRelayDuration( double GuideDuration )
{
	double RetVal;


	RetVal = GuideDuration;

	if ( RetVal < m_PlatformGuiderRelayMin )
		RetVal = m_PlatformGuiderRelayMin;
	else if ( RetVal > m_PlatformGuiderRelayMax )
		RetVal = m_PlatformGuiderRelayMax;

	return RetVal;
}

unsigned short CApnCamera::CalculateGuiderRelayTimeCounts( double GuideDuration )
{
	unsigned short RetVal;

	RetVal = (unsigned short)( (GuideDuration + m_PlatformGuiderRelayOpenTime + m_PlatformGuiderRelayCloseTime) /
							   m_PlatformGuiderRelayResolution );
	
	return RetVal;
}

long CApnCamera::LookupAltaCameraId( unsigned short CameraId )
{
	switch ( CameraId & FPGA_MASK_CAMERA_ID_ALTA )
	{
	case APN_ALTA_KAF0401E_CAM_ID:
		m_ApnSensorInfo = new CApnCamData_KAF0401E;
		break;
	case APN_ALTA_KAF1602E_CAM_ID:
		m_ApnSensorInfo = new CApnCamData_KAF1602E;
		break;
	case APN_ALTA_KAF0261E_CAM_ID:
		m_ApnSensorInfo = new CApnCamData_KAF0261E;
		break;
	case APN_ALTA_KAF1301E_CAM_ID:
		m_ApnSensorInfo = new CApnCamData_KAF1301E;
		break;
	case APN_ALTA_KAF1001E_CAM_ID:
		m_ApnSensorInfo = new CApnCamData_KAF1001E;
		break;
	case APN_ALTA_KAF1001ENS_CAM_ID:
		m_ApnSensorInfo = new CApnCamData_KAF1001ENS;
		break;
	case APN_ALTA_KAF10011105_CAM_ID:
		m_ApnSensorInfo = new CApnCamData_KAF10011105;
		break;
	case APN_ALTA_KAF3200E_CAM_ID:
		m_ApnSensorInfo = new CApnCamData_KAF3200E;
		break;
	case APN_ALTA_KAF6303E_CAM_ID:
		m_ApnSensorInfo = new CApnCamData_KAF6303E;
		break;
	case APN_ALTA_KAF16801E_CAM_ID:
		m_ApnSensorInfo = new CApnCamData_KAF16801E;
		break;
	case APN_ALTA_KAF16803_CAM_ID:
		m_ApnSensorInfo = new CApnCamData_KAF16803;
		break;
	case APN_ALTA_KAF09000_CAM_ID:
		m_ApnSensorInfo = new CApnCamData_KAF09000;
		break;
	case APN_ALTA_KAF09000X_CAM_ID:
		m_ApnSensorInfo = new CApnCamData_KAF09000X;
		break;
	case APN_ALTA_KAF0401EB_CAM_ID:
		m_ApnSensorInfo = new CApnCamData_KAF0401EB;
		break;
	case APN_ALTA_KAF1602EB_CAM_ID:
		m_ApnSensorInfo = new CApnCamData_KAF1602EB;
		break;
	case APN_ALTA_KAF0261EB_CAM_ID:
		m_ApnSensorInfo = new CApnCamData_KAF0261EB;
		break;
	case APN_ALTA_KAF1301EB_CAM_ID:
		m_ApnSensorInfo = new CApnCamData_KAF1301EB;
		break;
	case APN_ALTA_KAF1001EB_CAM_ID:
		m_ApnSensorInfo = new CApnCamData_KAF1001EB;
		break;
	case APN_ALTA_KAF6303EB_CAM_ID:
		m_ApnSensorInfo = new CApnCamData_KAF6303EB;
		break;
	case APN_ALTA_KAF3200EB_CAM_ID:
		m_ApnSensorInfo = new CApnCamData_KAF3200EB;
		break;

	case APN_ALTA_TH7899_CAM_ID:
		m_ApnSensorInfo = new CApnCamData_TH7899;
		break;
	case APN_ALTA_S101401107_CAM_ID:
		m_ApnSensorInfo = new CApnCamData_S101401107;
		break;
	case APN_ALTA_S101401109_CAM_ID:
		m_ApnSensorInfo = new CApnCamData_S101401109;
		break;

	case APN_ALTA_CCD4710_CAM_ID:
		m_ApnSensorInfo = new CApnCamData_CCD4710;
		break;
	case APN_ALTA_CCD4710ALT_CAM_ID:
		m_ApnSensorInfo = new CApnCamData_CCD4710ALT;
		break;
	case APN_ALTA_CCD4240_CAM_ID:
		m_ApnSensorInfo = new CApnCamData_CCD4240;
		break;
	case APN_ALTA_CCD5710_CAM_ID:
		m_ApnSensorInfo = new CApnCamData_CCD5710;
		break;
	case APN_ALTA_CCD3011_CAM_ID:
		m_ApnSensorInfo = new CApnCamData_CCD3011;
		break;
	case APN_ALTA_CCD5520_CAM_ID:
		m_ApnSensorInfo = new CApnCamData_CCD5520;
		break;
	case APN_ALTA_CCD4720_CAM_ID:
		m_ApnSensorInfo = new CApnCamData_CCD4720;
		break;
	case APN_ALTA_CCD7700_CAM_ID:
		m_ApnSensorInfo = new CApnCamData_CCD7700;
		break;

	case APN_ALTA_CCD4710B_CAM_ID:
		m_ApnSensorInfo = new CApnCamData_CCD4710B;
		break;
	case APN_ALTA_CCD4240B_CAM_ID:
		m_ApnSensorInfo = new CApnCamData_CCD4240B;
		break;
	case APN_ALTA_CCD5710B_CAM_ID:
		m_ApnSensorInfo = new CApnCamData_CCD5710B;
		break;
	case APN_ALTA_CCD3011B_CAM_ID:
		m_ApnSensorInfo = new CApnCamData_CCD3011B;
		break;
	case APN_ALTA_CCD5520B_CAM_ID:
		m_ApnSensorInfo = new CApnCamData_CCD5520B;
		break;
	case APN_ALTA_CCD4720B_CAM_ID:
		m_ApnSensorInfo = new CApnCamData_CCD4720B;
		break;
	case APN_ALTA_CCD7700B_CAM_ID:
		m_ApnSensorInfo = new CApnCamData_CCD7700B;
		break;

	case APN_ALTA_KAI2001ML_CAM_ID:
		m_ApnSensorInfo = new CApnCamData_KAI2001ML;
		break;
	case APN_ALTA_KAI2020ML_CAM_ID:
		m_ApnSensorInfo = new CApnCamData_KAI2020ML;
		break;
	case APN_ALTA_KAI4020ML_CAM_ID:
		m_ApnSensorInfo = new CApnCamData_KAI4020ML;
		break;
	case APN_ALTA_KAI11000ML_CAM_ID:
		m_ApnSensorInfo = new CApnCamData_KAI11000ML;
		break;
	case APN_ALTA_KAI2001CL_CAM_ID:
		m_ApnSensorInfo = new CApnCamData_KAI2001CL;
		break;
	case APN_ALTA_KAI2020CL_CAM_ID:
		m_ApnSensorInfo = new CApnCamData_KAI2020CL;
		break;
	case APN_ALTA_KAI4020CL_CAM_ID:
		m_ApnSensorInfo = new CApnCamData_KAI4020CL;
		break;
	case APN_ALTA_KAI11000CL_CAM_ID:
		m_ApnSensorInfo = new CApnCamData_KAI11000CL;
		break;

	case APN_ALTA_KAI2020MLB_CAM_ID:
		m_ApnSensorInfo = new CApnCamData_KAI2020MLB;
		break;
	case APN_ALTA_KAI4020MLB_CAM_ID:
		m_ApnSensorInfo = new CApnCamData_KAI4020MLB;
		break;
	case APN_ALTA_KAI2020CLB_CAM_ID:
		m_ApnSensorInfo = new CApnCamData_KAI2020CLB;
		break;
	case APN_ALTA_KAI4020CLB_CAM_ID:
		m_ApnSensorInfo = new CApnCamData_KAI4020CLB;
		break;
	
	default:
		return 1;
		break;
	}

	return 0;
}

long CApnCamera::LookupAscentCameraId( unsigned short CameraId )
{
	switch ( CameraId & FPGA_MASK_CAMERA_ID_ASCENT )
	{
		case APN_ASCENT_KAF0402E_CAM_ID:
			m_ApnSensorInfo = new CApnCamData_ASCENT0402ME;
			break;
		case APN_ASCENT_KAF0402E2_CAM_ID:
			m_ApnSensorInfo = new CApnCamData_ASCENT0402ME2;
			break;
		case APN_ASCENT_KAF0402E3_CAM_ID:
			m_ApnSensorInfo = new CApnCamData_ASCENT0402ME3;
			break;
		case APN_ASCENT_KAF0402E4_CAM_ID:
			m_ApnSensorInfo = new CApnCamData_ASCENT0402ME4;
			break;

		case APN_ASCENT_KAI340_CAM_ID:
			m_ApnSensorInfo = new CApnCamData_ASCENT340;
			break;
		case APN_ASCENT_KAI2000_CAM_ID:
			m_ApnSensorInfo = new CApnCamData_ASCENT2000;
			break;
		case APN_ASCENT_KAI4000_CAM_ID:
			m_ApnSensorInfo = new CApnCamData_ASCENT4000;
			break;
		case APN_ASCENT_KAI16000_CAM_ID:
			m_ApnSensorInfo = new CApnCamData_ASCENT16000;
			break;

		default:
			return 1;	// No known camera located, return
			break;
	}

	return 0;
}

long CApnCamera::InitDefaults()
{
	unsigned short	RegVal;
	unsigned short	ShutterDelay;
	unsigned short	PreRoiRows, PostRoiRows;
	unsigned short	PreRoiVBinning, PostRoiVBinning;
	unsigned short	UnbinnedRoiY;						// Vertical ROI pixels
	unsigned short	CameraId;
	double			CloseDelay;
	bool			UseOppositePatterns;
	bool			AltaPlatform;
	bool			AscentPlatform;



	AltaPlatform	= false;
	AscentPlatform	= false;

	// Init the camera data structure
	if ( m_ApnSensorInfo )
		delete m_ApnSensorInfo;

	m_ApnSensorInfo = NULL;

	// Read and store the firmware version for reference
	Read( FPGA_REG_FIRMWARE_REV, m_pvtFirmwareVersion );

	// Read the Camera ID register
	Read( FPGA_REG_CAMERA_ID, CameraId );

	// Deterministically check if we are an Ascent camera
	AscentPlatform = ApnCamModelIsAscent( CameraId, m_pvtFirmwareVersion );

	// Deterministically check if we are an Alta camera
	AltaPlatform = ApnCamModelIsAlta( CameraId, m_pvtFirmwareVersion );

	// We cannot be both and Alta and an Ascent, and we must be one or the other
	if ( (AscentPlatform && AltaPlatform) || (!AscentPlatform && !AltaPlatform) )
	{
		m_pvtPlatformType = Apn_Platform_Unknown;

		return 1;		// failure to determine camera line
	}

	// Look up the ID and dynamically create the m_ApnSensorInfo object
	if ( AltaPlatform )
	{
		if ( LookupAltaCameraId( CameraId ) != 0 )
			return 1;

		m_pvtPlatformType	= Apn_Platform_Alta;
		m_pvtCameraID		= CameraId & FPGA_MASK_CAMERA_ID_ALTA;
	}
	
	if ( AscentPlatform )
	{
		if ( LookupAscentCameraId( CameraId ) != 0 )
			return 1;

		m_pvtPlatformType	= Apn_Platform_Ascent;
		m_pvtCameraID		= CameraId & FPGA_MASK_CAMERA_ID_ASCENT;
	}

	// First set all of our constants
	SetPlatformConstants();

	// New Reset command
	ResetSystemNoFlush();

	// we created the object, now set everything
	m_ApnSensorInfo->Initialize();

	// Initialize private variables
	write_CameraMode( Apn_CameraMode_Normal );

	write_DigitizeOverscan( false );

	write_DisableFlushCommands( false );
	write_DisablePostExposeFlushing( false );

	m_pvtDataBits				= Apn_Resolution_SixteenBit;

	m_pvtExternalShutter		= false;
	m_pvtNetworkTransferMode	= Apn_NetworkMode_Tcp;

	// Initialize variables used for imaging
	m_pvtRoiStartX		= 0;
	m_pvtRoiStartY		= 0;
	m_pvtRoiPixelsH		= m_ApnSensorInfo->m_ImagingColumns;
	m_pvtRoiPixelsV		= m_ApnSensorInfo->m_ImagingRows;

	m_pvtRoiBinningH	= 1;
	m_pvtRoiBinningV	= 1;

        printf ("Camera ID is %u\n",m_pvtCameraID);
        printf("sensor = %s\n",                 m_ApnSensorInfo->m_Sensor);        printf("model = %s\n",m_ApnSensorInfo->m_CameraModel);
        printf("interline = %u\n",m_ApnSensorInfo->m_InterlineCCD);
        printf("serialA = %u\n",m_ApnSensorInfo->m_SupportsSerialA);
        printf("serialB = %u\n",m_ApnSensorInfo->m_SupportsSerialB);
        printf("ccdtype = %u\n",m_ApnSensorInfo->m_SensorTypeCCD);
        printf("Tcolumns = %u\n",m_ApnSensorInfo->m_TotalColumns);
        printf("ImgColumns = %u\n",m_ApnSensorInfo->m_ImagingColumns);
        printf("ClampColumns = %u\n",m_ApnSensorInfo->m_ClampColumns);
        printf("PreRoiSColumns = %u\n",m_ApnSensorInfo->m_PreRoiSkipColumns);
        printf("PostRoiSColumns = %u\n",m_ApnSensorInfo->m_PostRoiSkipColumns);
        printf("OverscanColumns = %u\n",m_ApnSensorInfo->m_OverscanColumns);
        printf("TRows = %u\n",m_ApnSensorInfo->m_TotalRows);
        printf("ImgRows = %u\n",m_ApnSensorInfo->m_ImagingRows);
        printf("UnderscanRows = %u\n",m_ApnSensorInfo->m_UnderscanRows);
        printf("OverscanRows = %u\n",m_ApnSensorInfo->m_OverscanRows);
        printf("VFlushBinning = %u\n",m_ApnSensorInfo->m_VFlushBinning);
        printf("HFlushDisable = %u\n",m_ApnSensorInfo->m_HFlushDisable);
        printf("ShutterCloseDelay = %u\n",m_ApnSensorInfo->m_ShutterCloseDelay);
        printf("PixelSizeX = %lf\n",m_ApnSensorInfo->m_PixelSizeX);
        printf("PixelSizeY = %lf\n",m_ApnSensorInfo->m_PixelSizeY);
        printf("Color = %u\n",m_ApnSensorInfo->m_Color);
//      printf("ReportedGainTwelveBit = %lf\n",m_ApnSensorInfo->m_ReportedGainTwelveBit);
        printf("ReportedGainSixteenBit = %lf\n",m_ApnSensorInfo->m_ReportedGainSixteenBit);
        printf("MinSuggestedExpTime = %lf\n",m_ApnSensorInfo->m_MinSuggestedExpTime);
        printf("CoolingSupported = %u\n",m_ApnSensorInfo->m_CoolingSupported);
        printf("RegulatedCoolingSupported = %u\n",m_ApnSensorInfo->m_RegulatedCoolingSupported);
        printf("TempSetPoint = %lf\n",m_ApnSensorInfo->m_TempSetPoint);
//      printf("TempRegRate = %u\n",m_ApnSensorInfo->m_TempRegRate);
        printf("TempRampRateOne = %u\n",m_ApnSensorInfo->m_TempRampRateOne);
        printf("TempRampRateTwo = %u\n",m_ApnSensorInfo->m_TempRampRateTwo);
        printf("TempBackoffPoint = %lf\n",m_ApnSensorInfo->m_TempBackoffPoint);
//        printf("DefaultGainTwelveBit = %u\n",m_ApnSensorInfo->m_DefaultGainTwelveBit);
//        printf("DefaultOffsetTwelveBit = %u\n",m_ApnSensorInfo->m_DefaultOffsetTwelveBit);
        printf("DefaultRVoltage = %u\n",m_ApnSensorInfo->m_DefaultRVoltage);
                                                                           
                                                                           
                                                                           
        printf ("RoiPixelsH is %u\n",m_pvtRoiPixelsH);
        printf ("RoiPixelsV is %u\n",m_pvtRoiPixelsV);
                                                                           

	// Issue a clear command, so the registers are zeroed out
	// This will put the camera in a known state for us.
	Write( FPGA_REG_COMMAND_B, FPGA_BIT_CMD_CLEAR_ALL );

	// Reset the camera
	ResetSystemNoFlush();

	// Load Inversion Masks
	Write( FPGA_REG_VRAM_INV_MASK, m_ApnSensorInfo->m_VerticalPattern.Mask );
	Write( FPGA_REG_HRAM_INV_MASK, m_ApnSensorInfo->m_RoiPatternSixteen.Mask );

	// Load Pattern Files
	LoadVerticalPattern();

	UseOppositePatterns = false;
	if ( (m_ApnSensorInfo->m_CameraId >= 256) && (m_ApnSensorInfo->m_DefaultSpeed == 0x0) )
	{
		UseOppositePatterns = true;
	}

	LoadClampPattern( UseOppositePatterns );
	LoadSkipPattern( UseOppositePatterns );
	LoadRoiPattern( UseOppositePatterns, m_pvtRoiBinningH );

	// Set the HCLK speed for Ascent
	if ( (m_pvtPlatformType == Apn_Platform_Ascent) && (m_ApnSensorInfo->m_DefaultSpeed != 0xFFFF) )
	{
		write_DigitizationSpeed( m_ApnSensorInfo->m_DefaultSpeed );
	}

	// Program default camera settings
	Write( FPGA_REG_CLAMP_COUNT,		m_ApnSensorInfo->m_ClampColumns );	
	Write( FPGA_REG_PREROI_SKIP_COUNT,	m_ApnSensorInfo->m_PreRoiSkipColumns );	
	Write( FPGA_REG_ROI_COUNT,			m_ApnSensorInfo->m_ImagingColumns );	
	Write( FPGA_REG_POSTROI_SKIP_COUNT,	m_ApnSensorInfo->m_PostRoiSkipColumns +
										m_ApnSensorInfo->m_OverscanColumns );		
	
	// Since the default state of m_DigitizeOverscan is false, set the count to zero.
	Write( FPGA_REG_OVERSCAN_COUNT,		0x0 );	

	// Now calculate the vertical settings
	UnbinnedRoiY	= m_pvtRoiPixelsV * m_pvtRoiBinningV;

	PreRoiRows		= m_ApnSensorInfo->m_UnderscanRows + 
					  m_pvtRoiStartY;
	
	PostRoiRows		= m_ApnSensorInfo->m_TotalRows -
					  PreRoiRows -
					  UnbinnedRoiY;

	PreRoiVBinning	= 1;
	PostRoiVBinning	= 1;

	// For interline CCDs, set "Fast Dump" mode if the particular array is NOT digitized
	if ( m_ApnSensorInfo->m_InterlineCCD )
	{
		// use the fast dump feature to get rid of the data quickly.
		// one row, binning to the original row count
		// note that we only are not digitized in arrays 1 and 3
		PreRoiVBinning	= PreRoiRows;
		PostRoiVBinning	= PostRoiRows;

		PreRoiVBinning	|= FPGA_BIT_ARRAY_FASTDUMP;
		PostRoiVBinning	|= FPGA_BIT_ARRAY_FASTDUMP;

		PreRoiRows	= 1;
		PostRoiRows	= 1;
	}

	// Program the vertical settings
	if ( m_pvtFirmwareVersion < 11 )
	{
		Write( FPGA_REG_A1_ROW_COUNT, PreRoiRows );	
		Write( FPGA_REG_A1_VBINNING,  PreRoiVBinning );
		
		Write( FPGA_REG_A2_ROW_COUNT, m_pvtRoiPixelsV );	
		Write( FPGA_REG_A2_VBINNING,  (m_pvtRoiBinningV | FPGA_BIT_ARRAY_DIGITIZE) );	
		
		Write( FPGA_REG_A3_ROW_COUNT, PostRoiRows );	
		Write( FPGA_REG_A3_VBINNING,  PostRoiVBinning );	
	}
	else
	{
		Write( FPGA_REG_A1_ROW_COUNT, 0 );	
		Write( FPGA_REG_A1_VBINNING,  0 );
		
		Write( FPGA_REG_A2_ROW_COUNT, PreRoiRows );	
		Write( FPGA_REG_A2_VBINNING,  PreRoiVBinning );
		
		Write( FPGA_REG_A3_ROW_COUNT, m_pvtRoiPixelsV );	
		Write( FPGA_REG_A3_VBINNING,  (m_pvtRoiBinningV | FPGA_BIT_ARRAY_DIGITIZE) );	
		
		Write( FPGA_REG_A4_ROW_COUNT, 0 );	
		Write( FPGA_REG_A4_VBINNING,  0 );	
		
		Write( FPGA_REG_A5_ROW_COUNT, PostRoiRows );	
		Write( FPGA_REG_A5_VBINNING,  PostRoiVBinning );	
	}

	// we don't use write_FlushBinningV() here because that would include additional RESETs
	m_pvtFlushBinningV = m_ApnSensorInfo->m_VFlushBinning;
	Write( FPGA_REG_VFLUSH_BINNING, m_pvtFlushBinningV );

	if ( m_ApnSensorInfo->m_ShutterCloseDelay == 0 )
	{
		// This is the case for interline cameras
		ShutterDelay = 0;

		m_pvtShutterCloseDelay = ShutterDelay;
	}
	else
	{
		CloseDelay	 = (double)m_ApnSensorInfo->m_ShutterCloseDelay / 1000;

		m_pvtShutterCloseDelay = CloseDelay;

		ShutterDelay = (unsigned short)
			( (CloseDelay - m_PlatformShutterCloseDiff) / m_PlatformTimerResolution );
	}

	if ( m_ApnSensorInfo->m_HFlushDisable )
	{
		Read( FPGA_REG_OP_A, RegVal );

		RegVal |= FPGA_BIT_DISABLE_H_CLK; 
		
		Write( FPGA_REG_OP_A, RegVal );
	}

	// If we are a USB2 camera, set all the 12bit variables for the 12bit
	// A/D processor
	if ( GetCameraInterface() == Apn_Interface_USB )
	{
		if ( m_ApnSensorInfo->m_PrimaryADType == ApnAdType_Ascent_Sixteen )
		{
			// left side
			Read( FPGA_REG_OP_B, RegVal );
			RegVal &= ~FPGA_BIT_AD_LOAD_SELECT;
			Write( FPGA_REG_OP_B, RegVal );
			InitAscentSixteenBitAD();
			write_AscentADGainSixteen( m_ApnSensorInfo->m_DefaultGainLeft );
			write_AscentADOffsetSixteen( m_ApnSensorInfo->m_DefaultOffsetLeft );

			// right side
			Read( FPGA_REG_OP_B, RegVal );
			RegVal |= FPGA_BIT_AD_LOAD_SELECT;
			Write( FPGA_REG_OP_B, RegVal );
			InitAscentSixteenBitAD();
			write_AscentADGainSixteen( m_ApnSensorInfo->m_DefaultGainRight );
			write_AscentADOffsetSixteen( m_ApnSensorInfo->m_DefaultOffsetRight );

			// when the right side is done, we need to set the 
			// FPGA_BIT_AD_LOAD_SELECT bit back to zero
			Read( FPGA_REG_OP_B, RegVal );
			RegVal &= ~FPGA_BIT_AD_LOAD_SELECT;
			Write( FPGA_REG_OP_B, RegVal );

			// assign private vars
			m_pvtAscentSixteenBitGainLeft		= m_ApnSensorInfo->m_DefaultGainLeft;
			m_pvtAscentSixteenBitGainRight		= m_ApnSensorInfo->m_DefaultGainRight;
			m_pvtAscentSixteenBitOffsetLeft		= m_ApnSensorInfo->m_DefaultOffsetLeft;
			m_pvtAscentSixteenBitOffsetRight	= m_ApnSensorInfo->m_DefaultOffsetRight;
		}

		if ( m_ApnSensorInfo->m_AlternativeADType == ApnAdType_Alta_Twelve )
		{
			InitTwelveBitAD();
			write_TwelveBitGain( m_ApnSensorInfo->m_DefaultGainLeft );
			write_TwelveBitOffset( m_ApnSensorInfo->m_DefaultOffsetLeft );
		}
	}

	// Reset the camera and start flushing
	ResetSystem();

	write_SequenceBulkDownload( true );

	write_ImageCount( 1 );
	write_SequenceDelay( 0.000327 );
	write_VariableSequenceDelay( true );

	Write( FPGA_REG_SHUTTER_CLOSE_DELAY, ShutterDelay );

	// Set the Fan State.  Setting the private var first to make sure the write_FanMode
	// call thinks we're doing a state transition.
	// On write_FanMode return, our state will be Apn_FanMode_Medium
	m_pvtFanMode = Apn_FanMode_Off;		// we're going to set this
	write_FanMode( Apn_FanMode_Low );

	// Initialize the LED states and the LED mode.  There is nothing to output
	// to the device since we issued our CLEAR early in the init() process, and 
	// we are now in a known state.
	m_pvtLedStateA	= Apn_LedState_Expose;
	m_pvtLedStateB	= Apn_LedState_Expose;
	m_pvtLedMode	= Apn_LedMode_EnableAll;

	// The CLEAR puts many vars into their default state
	m_pvtTriggerNormalEach			= false;
	m_pvtTriggerNormalGroup			= false;
	m_pvtTriggerTdiKineticsEach		= false;
	m_pvtTriggerTdiKineticsGroup	= false;

	m_pvtFastSequence				= false;

	// Default value for test LED is 0%
	m_pvtTestLedBrightness = 0.0;

	// Default values for I/O Port - the CLEAR op doesn't clear these values
	// This will also init our private vars to 0x0
	write_IoPortAssignment( 0x0 );
	write_IoPortDirection( 0x0 );

	// Set the default TDI variables.  These also will automatically initialize
	// the "virtual" kinetics mode variables.
	write_TDIRate( m_PlatformTdiRateDefault );
	write_TDIRows( 1 );
	write_TDIBinningV( 1 );

	// Set the shutter strobe values to their defaults
	write_ShutterStrobePeriod( m_PlatformStrobePeriodDefault );
	write_ShutterStrobePosition( m_PlatformStrobePositionDefault );

	// Set default averaging state
	if ( m_ApnSensorInfo->m_DefaultDataReduction )
	{
		Read( FPGA_REG_OP_B, RegVal );
		RegVal |= FPGA_BIT_AD_AVERAGING;
		Write( FPGA_REG_OP_B, RegVal );
	}

	// Program our initial cooler values.  The only cooler value that we reset
	// at init time is the backoff point.  Everything else is left untouched, and
	// state information is determined from the camera controller.
	m_pvtCoolerBackoffPoint = m_ApnSensorInfo->m_TempBackoffPoint;
	write_CoolerBackoffPoint( m_pvtCoolerBackoffPoint );
	Write( FPGA_REG_TEMP_RAMP_DOWN_A,	m_ApnSensorInfo->m_TempRampRateOne );
	Write( FPGA_REG_TEMP_RAMP_DOWN_B,	m_ApnSensorInfo->m_TempRampRateTwo );
	// the cooler code not only determines the m_pvtCoolerEnable state, but
	// also implicitly calls UpdateGeneralStatus() as part of read_CoolerStatus()
	if ( read_CoolerStatus() == Apn_CoolerStatus_Off )
		m_pvtCoolerEnable = false;
	else
		m_pvtCoolerEnable = true;

	// Perform any platform-specific initialization
	if ( m_pvtPlatformType == Apn_Platform_Alta )
	{
	}
	else if ( m_pvtPlatformType == Apn_Platform_Ascent )
	{
		write_DataAveraging( false );
		write_DualReadout( false );

		write_GuideRAPlusDuration( 0.005 );
		write_GuideRAMinusDuration( 0.005 );
		write_GuideDecPlusDuration( 0.005 );
		write_GuideDecMinusDuration( 0.005 );
	}

	m_pvtImageInProgress	= false;
	m_pvtImageReady			= false;

	m_pvtMostRecentFrame	= 0;
	m_pvtReadyFrame			= 0;
	m_pvtCurrentFrame		= 0;
	
	m_pvtBayerShift			= Apn_BayerShift_Automatic;

	m_pvtFilterWheelType	= Apn_Filter_Unknown;
	m_pvtFilterMaxPositions	= APN_FILTER_UNKNOWN_MAX_POSITIONS;

	return 0;
}

long CApnCamera::InitTwelveBitAD()
{
	// Write( FPGA_REG_AD_CONFIG_DATA, 0x0028 );
	Write( FPGA_REG_AD_CONFIG_DATA, 0x0008 );
	Write( FPGA_REG_COMMAND_B,		FPGA_BIT_CMD_AD_CONFIG );

	return 0;
}

long CApnCamera::InitAscentSixteenBitAD()
{
	Write( FPGA_REG_AD_CONFIG_DATA, 0x0058 );
	Write( FPGA_REG_COMMAND_B,		FPGA_BIT_CMD_AD_CONFIG );
	
	Write( FPGA_REG_AD_CONFIG_DATA, 0x10C0 );
	Write( FPGA_REG_COMMAND_B,		FPGA_BIT_CMD_AD_CONFIG );

	return 0;
}

void CApnCamera::UpdateGeneralStatus()
{
	unsigned short	StatusReg;
	unsigned short	HeatsinkTempReg;
	unsigned short	CcdTempReg;
	unsigned short	CoolerDriveReg;
	unsigned short	VoltageReg;
	unsigned short	TdiCounterReg;
	unsigned short	SequenceCounterReg;
	unsigned short	MostRecentFrame;
	unsigned short	ReadyFrame;
	unsigned short	CurrentFrame;


	char szOutputText[256];

//	AltaDebugOutputString( "APOGEE.DLL - CApnCamera::UpdateGeneralStatus()" );

	
	// Read the general status register of the device
	m_pvtQueryStatusRetVal = QueryStatusRegs( StatusReg, 
											  HeatsinkTempReg, 
											  CcdTempReg, 
											  CoolerDriveReg,
											  VoltageReg,
											  TdiCounterReg,
											  SequenceCounterReg,
											  MostRecentFrame,
											  ReadyFrame,
											  CurrentFrame );

	/*
	AltaDebugPrint( szOutputText, "APOGEE.DLL - CApnCamera::UpdateGeneralStatus() -> StatusReg = 0x%04X", StatusReg );
	AltaDebugOutputString( szOutputText );
	AltaDebugPrint( szOutputText, "APOGEE.DLL - CApnCamera::UpdateGeneralStatus() -> HeatsinkTempReg = 0x%04X", HeatsinkTempReg );
	AltaDebugOutputString( szOutputText );
	AltaDebugPrint( szOutputText, "APOGEE.DLL - CApnCamera::UpdateGeneralStatus() -> CcdTempReg = 0x%04X", CcdTempReg );
	AltaDebugOutputString( szOutputText );
	AltaDebugPrint( szOutputText, "APOGEE.DLL - CApnCamera::UpdateGeneralStatus() -> CoolerDriveReg = 0x%04X", CoolerDriveReg );
	AltaDebugOutputString( szOutputText );
	AltaDebugPrint( szOutputText, "APOGEE.DLL - CApnCamera::UpdateGeneralStatus() -> VoltageReg = 0x%04X", VoltageReg );
	AltaDebugOutputString( szOutputText );
	AltaDebugPrint( szOutputText, "APOGEE.DLL - CApnCamera::UpdateGeneralStatus() -> TdiCounterReg = 0x%04X", TdiCounterReg );
	AltaDebugOutputString( szOutputText );
	AltaDebugPrint( szOutputText, "APOGEE.DLL - CApnCamera::UpdateGeneralStatus() -> SequenceCounterReg = 0x%04X", SequenceCounterReg );
	AltaDebugOutputString( szOutputText );
	AltaDebugPrint( szOutputText, "APOGEE.DLL - CApnCamera::UpdateGeneralStatus() -> MostRecentFrame = %d", MostRecentFrame );
	AltaDebugOutputString( szOutputText );
	AltaDebugPrint( szOutputText, "APOGEE.DLL - CApnCamera::UpdateGeneralStatus() -> ReadyFrame = %d", ReadyFrame );
	AltaDebugOutputString( szOutputText );
	AltaDebugPrint( szOutputText, "APOGEE.DLL - CApnCamera::UpdateGeneralStatus() -> CurrentFrame = %d", CurrentFrame );
	AltaDebugOutputString( szOutputText );
	*/

	m_pvtStatusReg	= StatusReg;

	HeatsinkTempReg &= FPGA_MASK_TEMP_PARAMS;
	CcdTempReg		&= FPGA_MASK_TEMP_PARAMS;
	VoltageReg		&= FPGA_MASK_INPUT_VOLTAGE;

	if ( m_pvtPlatformType == Apn_Platform_Alta )
	{
		CoolerDriveReg	&= FPGA_MASK_TEMP_PARAMS;
	
		if ( CoolerDriveReg > 3200 )
			m_pvtCoolerDrive = 100.0;
		else
			m_pvtCoolerDrive = ( (double)(CoolerDriveReg - 600) / 2600.0 ) * 100.0;
	}
	else if ( m_pvtPlatformType == Apn_Platform_Ascent )
	{
		if ( CoolerDriveReg > 60000 )
			m_pvtCoolerDrive = 100.0;
		else
			m_pvtCoolerDrive = ( (double)(CoolerDriveReg - 15000) / 45000.0 ) * 100.0;
	}
	
	// Don't return a negative value
	if ( m_pvtCoolerDrive < 0.0 )
		m_pvtCoolerDrive = 0.0;

	m_pvtCurrentCcdTemp			= ( (CcdTempReg - m_PlatformTempSetpointZeroPoint) 
									* m_PlatformTempDegreesPerBit );

	m_pvtCurrentHeatsinkTemp	= ( (HeatsinkTempReg - m_PlatformTempHeatsinkZeroPoint) 
									* m_PlatformTempDegreesPerBit );
	
	m_pvtInputVoltage			= VoltageReg * m_PlatformVoltageResolution;

	// Update ShutterState
	m_pvtShutterState = ( (m_pvtStatusReg & FPGA_BIT_STATUS_SHUTTER_OPEN) != 0 );

	// Update counters
	m_pvtSequenceCounter	= SequenceCounterReg;
	m_pvtTDICounter			= TdiCounterReg;

	// Update USB frame info (for images in a sequence)
	// Network systems will just set these vars to zero (they are not used)
	m_pvtMostRecentFrame	= MostRecentFrame;
	m_pvtReadyFrame			= ReadyFrame;
	m_pvtCurrentFrame		= CurrentFrame;
}


bool CApnCamera::ImageReady()
{
	return m_pvtImageReady;
}


bool CApnCamera::ImageInProgress()
{
	return m_pvtImageInProgress;
}


void CApnCamera::SignalImagingDone()
{
	m_pvtImageInProgress = false;
}

void CApnCamera::SetPlatformConstants()
{
	if ( read_PlatformType() == Apn_Platform_Alta )
	{
		m_PlatformHBinningMax				= APN_HBINNING_MAX_ALTA;
		m_PlatformVBinningMax				= APN_HBINNING_MAX_ALTA;

		m_PlatformTimerResolution			= APN_TIMER_RESOLUTION_ALTA;
		m_PlatformPeriodTimerResolution		= APN_PERIOD_TIMER_RESOLUTION_ALTA;

		m_PlatformTimerOffsetCount			= APN_TIMER_OFFSET_COUNT_ALTA;

		m_PlatformSequenceDelayResolution	= APN_SEQUENCE_DELAY_RESOLUTION_ALTA;
		m_PlatformSequenceDelayMaximum		= APN_SEQUENCE_DELAY_MAXIMUM_ALTA;
		m_PlatformSequenceDelayMinimum		= APN_SEQUENCE_DELAY_MINIMUM_ALTA;

		m_PlatformExposureTimeMin			= APN_EXPOSURE_TIME_MIN_ALTA;
		m_PlatformExposureTimeMax			= APN_EXPOSURE_TIME_MAX_ALTA;

		m_PlatformTdiRateResolution			= APN_TDI_RATE_RESOLUTION_ALTA;
		m_PlatformTdiRateMin				= APN_TDI_RATE_MIN_ALTA;
		m_PlatformTdiRateMax				= APN_TDI_RATE_MAX_ALTA;
		m_PlatformTdiRateDefault			= APN_TDI_RATE_DEFAULT_ALTA;

		m_PlatformVoltageResolution			= APN_VOLTAGE_RESOLUTION_ALTA;

		m_PlatformShutterCloseDiff			= APN_SHUTTER_CLOSE_DIFF_ALTA;

		m_PlatformStrobePositionMin			= APN_STROBE_POSITION_MIN_ALTA;
		m_PlatformStrobePositionMax			= APN_STROBE_POSITION_MAX_ALTA;
		m_PlatformStrobePositionDefault		= APN_STROBE_POSITION_DEFAULT_ALTA;

		m_PlatformStrobePeriodMin			= APN_STROBE_PERIOD_MIN_ALTA;
		m_PlatformStrobePeriodMax			= APN_STROBE_PERIOD_MAX_ALTA;
		m_PlatformStrobePeriodDefault		= APN_STROBE_PERIOD_DEFAULT_ALTA;

		m_PlatformTempCounts				= APN_TEMP_COUNTS_ALTA;
		m_PlatformTempKelvinScaleOffset		= APN_TEMP_KELVIN_SCALE_OFFSET_ALTA;

		m_PlatformTempSetpointMin			= APN_TEMP_SETPOINT_MIN_ALTA;
		m_PlatformTempSetpointMax			= APN_TEMP_SETPOINT_MAX_ALTA;

		m_PlatformTempHeatsinkMin			= APN_TEMP_HEATSINK_MIN_ALTA;
		m_PlatformTempHeatsinkMax			= APN_TEMP_HEATSINK_MAX_ALTA;

		m_PlatformTempSetpointZeroPoint		= APN_TEMP_SETPOINT_ZERO_POINT_ALTA;
		m_PlatformTempHeatsinkZeroPoint		= APN_TEMP_HEATSINK_ZERO_POINT_ALTA;

		m_PlatformTempDegreesPerBit			= APN_TEMP_DEGREES_PER_BIT_ALTA;

		m_PlatformFanSpeedOff				= APN_FAN_SPEED_OFF_ALTA;
		m_PlatformFanSpeedLow				= APN_FAN_SPEED_LOW_ALTA;
		m_PlatformFanSpeedMedium			= APN_FAN_SPEED_MEDIUM_ALTA;
		m_PlatformFanSpeedHigh				= APN_FAN_SPEED_HIGH_ALTA;

		m_PlatformGuiderRelayResolution		= APN_GUIDER_RELAY_RESOLUTION_ALTA;
		m_PlatformGuiderRelayMin			= APN_GUIDER_RELAY_MIN_ALTA;
		m_PlatformGuiderRelayMax			= APN_GUIDER_RELAY_MAX_ALTA;
		m_PlatformGuiderRelayOpenTime		= APN_GUIDER_RELAY_OPEN_TIME_ALTA;
		m_PlatformGuiderRelayCloseTime		= APN_GUIDER_RELAY_CLOSE_TIME_ALTA;
	}
	else if ( read_PlatformType() == Apn_Platform_Ascent )
	{
		m_PlatformHBinningMax				= APN_HBINNING_MAX_ASCENT;
		m_PlatformVBinningMax				= APN_HBINNING_MAX_ASCENT;

		m_PlatformTimerResolution			= APN_TIMER_RESOLUTION_ASCENT;
		m_PlatformPeriodTimerResolution		= APN_PERIOD_TIMER_RESOLUTION_ASCENT;

		m_PlatformTimerOffsetCount			= APN_TIMER_OFFSET_COUNT_ASCENT;

		m_PlatformSequenceDelayResolution	= APN_SEQUENCE_DELAY_RESOLUTION_ASCENT;
		m_PlatformSequenceDelayMaximum		= APN_SEQUENCE_DELAY_MAXIMUM_ASCENT;
		m_PlatformSequenceDelayMinimum		= APN_SEQUENCE_DELAY_MINIMUM_ASCENT;

		m_PlatformExposureTimeMin			= APN_EXPOSURE_TIME_MIN_ASCENT;
		m_PlatformExposureTimeMax			= APN_EXPOSURE_TIME_MAX_ASCENT;

		m_PlatformTdiRateResolution			= APN_TDI_RATE_RESOLUTION_ASCENT;
		m_PlatformTdiRateMin				= APN_TDI_RATE_MIN_ASCENT;
		m_PlatformTdiRateMax				= APN_TDI_RATE_MAX_ASCENT;
		m_PlatformTdiRateDefault			= APN_TDI_RATE_DEFAULT_ASCENT;

		m_PlatformVoltageResolution			= APN_VOLTAGE_RESOLUTION_ASCENT;

		m_PlatformShutterCloseDiff			= APN_SHUTTER_CLOSE_DIFF_ASCENT;

		m_PlatformStrobePositionMin			= APN_STROBE_POSITION_MIN_ASCENT;
		m_PlatformStrobePositionMax			= APN_STROBE_POSITION_MAX_ASCENT;
		m_PlatformStrobePositionDefault		= APN_STROBE_POSITION_DEFAULT_ASCENT;

		m_PlatformStrobePeriodMin			= APN_STROBE_PERIOD_MIN_ASCENT;
		m_PlatformStrobePeriodMax			= APN_STROBE_PERIOD_MAX_ASCENT;
		m_PlatformStrobePeriodDefault		= APN_STROBE_PERIOD_DEFAULT_ASCENT;

		m_PlatformTempCounts				= APN_TEMP_COUNTS_ASCENT;
		m_PlatformTempKelvinScaleOffset		= APN_TEMP_KELVIN_SCALE_OFFSET_ASCENT;

		m_PlatformTempSetpointMin			= APN_TEMP_SETPOINT_MIN_ASCENT;
		m_PlatformTempSetpointMax			= APN_TEMP_SETPOINT_MAX_ASCENT;

		m_PlatformTempHeatsinkMin			= APN_TEMP_HEATSINK_MIN_ASCENT;
		m_PlatformTempHeatsinkMax			= APN_TEMP_HEATSINK_MAX_ASCENT;

		m_PlatformTempSetpointZeroPoint		= APN_TEMP_SETPOINT_ZERO_POINT_ASCENT;
		m_PlatformTempHeatsinkZeroPoint		= APN_TEMP_HEATSINK_ZERO_POINT_ASCENT;

		m_PlatformTempDegreesPerBit			= APN_TEMP_DEGREES_PER_BIT_ASCENT;

		m_PlatformFanSpeedOff				= APN_FAN_SPEED_OFF_ASCENT;
		m_PlatformFanSpeedLow				= APN_FAN_SPEED_LOW_ASCENT;
		m_PlatformFanSpeedMedium			= APN_FAN_SPEED_MEDIUM_ASCENT;
		m_PlatformFanSpeedHigh				= APN_FAN_SPEED_HIGH_ASCENT;

		m_PlatformGuiderRelayResolution		= APN_GUIDER_RELAY_RESOLUTION_ASCENT;
		m_PlatformGuiderRelayMin			= APN_GUIDER_RELAY_MIN_ASCENT;
		m_PlatformGuiderRelayMax			= APN_GUIDER_RELAY_MAX_ASCENT;
		m_PlatformGuiderRelayOpenTime		= APN_GUIDER_RELAY_OPEN_TIME_ASCENT;
		m_PlatformGuiderRelayCloseTime		= APN_GUIDER_RELAY_CLOSE_TIME_ASCENT;

	}
}

