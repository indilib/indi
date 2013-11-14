// ApnCamera.cpp: extras from the CCameraIO class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"

#include "ApnCamera.h"
#include "ApnCamTable.h"
#include "ApnCamData.h"
#ifndef ALTA_STANDALONE
#include "tcl.h"
#include "ccd.h"
#endif

// Determine if camera is present
// True if camera is present, false otherwise.
bool CApnCamera::read_Present()
{
	// OutputDebugString( "read_Present()" );

	USHORT ApStatus;
	USHORT DatumA;
	USHORT DatumB;
	char	szMsg[80];

	DatumA		= 0x0;
	DatumB		= 0x0;
	ApStatus	= 0;

	Write( FPGA_REG_SCRATCH, 0x8086 );
	Read( FPGA_REG_SCRATCH, DatumA );

	Write( FPGA_REG_SCRATCH, 0x1F2F );
	Read( FPGA_REG_SCRATCH, DatumB );

	if ( (DatumA != 0x8086) || (DatumB != 0x1F2F) )
	{
		// OutputDebugString( "read_Present FAILED." );
		sprintf( szMsg, "read_Present FAILED.  DatumA:  0x%X   DatumB:  0x%X", DatumA, DatumB );
		// OutputDebugString( szMsg );
		return false;
	}

	// OutputDebugString( "read_Present SUCCESS" );

	return true;
}

bool CApnCamera::sensorInfo()
{
	strcpy(m_Sensor,m_ApnSensorInfo->m_Sensor);
	strcpy(m_CameraModel,m_ApnSensorInfo->m_CameraModel);
	m_CameraId	= 	m_ApnSensorInfo->m_CameraId;
	m_InterlineCCD 	= 	m_ApnSensorInfo->m_InterlineCCD;
	m_SupportsSerialA =	m_ApnSensorInfo->m_SupportsSerialA;
	m_SupportsSerialB	=	m_ApnSensorInfo->m_SupportsSerialB;
	m_SensorTypeCCD		=m_ApnSensorInfo->m_SensorTypeCCD;
	m_TotalColumns	=m_ApnSensorInfo->m_TotalColumns;
	m_ImagingColumns=	m_ApnSensorInfo->m_ImagingColumns;
	m_ClampColumns=	m_ApnSensorInfo->m_ClampColumns;
	m_PreRoiSkipColumns	=m_ApnSensorInfo->m_PreRoiSkipColumns;
	m_PostRoiSkipColumns=	m_ApnSensorInfo->m_PostRoiSkipColumns;
	m_OverscanColumns	=m_ApnSensorInfo->m_OverscanColumns;
	m_TotalRows	=m_ApnSensorInfo->m_TotalRows;
	m_ImagingRows	=m_ApnSensorInfo->m_ImagingRows;
	m_UnderscanRows=	m_ApnSensorInfo->m_UnderscanRows;
	m_OverscanRows	=m_ApnSensorInfo->m_OverscanRows;
	m_VFlushBinning	=m_ApnSensorInfo->m_VFlushBinning;
	m_HFlushDisable		=m_ApnSensorInfo->m_HFlushDisable;
	m_ShutterCloseDelay=	m_ApnSensorInfo->m_ShutterCloseDelay;
	m_PixelSizeX	=	m_ApnSensorInfo->m_PixelSizeX;
	m_PixelSizeY	=	m_ApnSensorInfo->m_PixelSizeY;
	m_Color	=	m_ApnSensorInfo->m_Color;
//	m_ReportedGainTwelveBit	=	m_ApnSensorInfo->m_ReportedGainTwelveBit;
	m_ReportedGainSixteenBit=		m_ApnSensorInfo->m_ReportedGainSixteenBit;
	m_MinSuggestedExpTime	=	m_ApnSensorInfo->m_MinSuggestedExpTime;
//	m_TempRegRate	=m_ApnSensorInfo->m_TempRegRate;
	m_TempRampRateOne	=m_ApnSensorInfo->m_TempRampRateOne;
	m_TempRampRateTwo	=m_ApnSensorInfo->m_TempRampRateTwo;
//	m_DefaultGainTwelveBit	=m_ApnSensorInfo->m_DefaultGainTwelveBit;
//	m_DefaultOffsetTwelveBit=	m_ApnSensorInfo->m_DefaultOffsetTwelveBit;
//	m_DefaultRVoltage	=m_ApnSensorInfo->m_DefaultRVoltage;
	return true;

}

#ifndef ALTA_STANDALONE

bool CApnCamera::BufferImage(char *bufferName )
{
    unsigned short *pImageData;
    bool status; 
    short cols,rows,hbin,vbin;
    unsigned short xSize, ySize;
    unsigned long count;

    cols = m_pvtExposurePixelsH;
    rows = m_pvtExposurePixelsV;

    /* ALTA code has already applied binning calculations*/
    hbin = 1;
    vbin = 1;

    pImageData = (unsigned short *)CCD_locate_buffer(bufferName, 2 , cols, rows, hbin, vbin );
    if (pImageData == NULL) {
       return 0;
    }
    
    status = GetImageData(pImageData, xSize, ySize, count);
    return status;
}



bool CApnCamera::BufferDriftScan(char *bufferName, int delay, int rowCount, int nblock , int npipe)
{
    unsigned short *pImageData, *ptr;
    bool status; 
    int irow;
    short cols,rows,hbin,vbin;

    cols = m_pvtExposurePixelsH;
    rows = rowCount;
    hbin = m_pvtRoiBinningH;
    vbin = 1;
    return 1;
}

#endif



