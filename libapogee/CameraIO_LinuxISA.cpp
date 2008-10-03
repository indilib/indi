// CameraIO.cpp: implementation of the CCameraIO class.
//
// Copyright (c) 2000 Apogee Instruments Inc.
//////////////////////////////////////////////////////////////////////

#ifndef LINUX
#include "stdafx.h"
#else                                                 
#include <assert.h>
#include <sys/io.h>                                                           
#include <sys/time.h>                                                           
#include <sys/resource.h>                                                           
#include <sys/ioctl.h>
#include <string.h>                                                           
#include <sched.h>                                                           
#include <unistd.h>                                                           
#include <fcntl.h>                                                           
#define HANDLE int
#define FALSE 0
#define DWORD long
#define _ASSERT assert
#define REALTIME_PRIORITY_CLASS 1  
#define GetCurrentProcess getpid
#define LOBYTE(x) ((x) & 0xff)
#define HIBYTE(x) ((x >> 8) & 0xff)
#endif

#define MIRQ1	0x21
#define MIRQ2	0xA1

#include "time.h"
#include "tcl.h"
#include "ccd.h"
#include "CameraIO_Linux.h"
#include "ApogeeLinux.h"

const int NUM_POSITIONS = 6;
const int NUM_STEPS_PER_FILTER = 48;
const int STEP_DELAY = 10;

const unsigned char Steps[] = { 0x10, 0x30, 0x20, 0x60, 0x40, 0xc0, 0x80, 0x90 };
const int NUM_STEPS = sizeof ( Steps );

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

void CCameraIO::InitDefaults()
{
	////////////////////////////////////////////////////////////
	// Camera Settings
		
		m_HighPriority = true;
		m_PPRepeat = 1;	
		m_DataBits = 16;	
		m_FastShutter = false;
		m_MaxBinX = 8;
		m_MaxBinY = 63;
		m_MaxExposure = 10485.75;
		m_MinExposure = 0.01;
		m_GuiderRelays = false;
		m_Timeout = 2.0;

	////////////////////////////////////////////////////////////
	// Cooler Settings

		m_TempControl = true;
		m_TempCalibration = 160;
		m_TempScale = 2.1;	
		
	////////////////////////////////////////////////////////////
	// Exposure Settings

		m_BinX = 1;
		m_BinY = 1;		
		m_StartX = 0;
		m_StartY = 0;	
		m_NumX = 1;
		m_NumY = 1;		
	
	////////////////////////////////////////////////////////////
	// Geometry Settings
	
		m_Columns = 0;
		m_Rows = 0;	
		m_SkipC = 0;
		m_SkipR = 0;		
		m_HFlush = 1;
		m_VFlush = 1;	
		m_BIC = 4;
		m_BIR = 4;			
		m_ImgColumns = 0;
		m_ImgRows = 0;

	////////////////////////////////////////////////////////////
	// CCD Settings
		
		memset( m_Sensor, 0, 256 );
		m_Color = false;
		m_Noise = 0.0;				
		m_Gain = 0.0;				
		m_PixelXSize = 0.0;		
		m_PixelYSize = 0.0;		

	////////////////////////////////////////////////////////////
	// Internal variables

                fileHandle = 0;
		m_RegisterOffset = 0;
		m_Interface = Camera_Interface_ISA;	
		m_SensorType = Camera_SensorType_CCD;
}


 
bool CCameraIO::InitDriver(unsigned short camnum) 
{ 
    char deviceName[64];

        sprintf(deviceName,"%s%d",APOGEE_ISA_DEVICE,camnum);
        fileHandle = ::open(deviceName,O_RDONLY);
        if (fileHandle == -1) return false;
	return true; 
} 
 
long CCameraIO::Write( unsigned short reg, unsigned short val ) 
{ 
        int status;

        struct apIOparam request;
	unsigned short realreg = ( reg << 1 ) & 0xE;	// limit input to our address range  
        request.reg = realreg;
        request.param1=(int)val;
        status=ioctl(fileHandle,APISA_WRITE_USHORT,(unsigned long)&request);
        return 0;
} 
 
long CCameraIO::Read( unsigned short reg, unsigned short& val ) 
{ 
	unsigned short realreg; 
        int retval, status;
        struct apIOparam request;

	switch ( reg ) 
	{ 
	case Reg_ImageData: 
		realreg = RegISA_ImageData; 
		break; 
	case Reg_TempData: 
		realreg = RegISA_TempData; 
		break; 
	case Reg_Status: 
		realreg = RegISA_Status; 
		break; 
	case Reg_CommandReadback: 
		realreg = RegISA_CommandReadback; 
		break; 
	default: 
		assert( 1 );	// application program bug 
		val = 0; 
		return 0; 
	} 

        request.reg = realreg;
        request.param1=(unsigned long)&retval;
        status=ioctl(fileHandle,APISA_READ_USHORT,(unsigned long)&request);
        val = (unsigned short)retval;
	return 0; 
} 
 
// Returns 0 if successfull, 1 if Line_Done poll timed out. 
long CCameraIO::ReadLine( long SkipPixels, long Pixels,unsigned short* pLineBuffer )
{ 
    int j;
    int retval, status;
    struct apIOparam request;

	if ( !m_TDI ) 
	{ 
		///////////////////////////////////// 
		// Clock out the line 
		m_RegShadow[ Reg_Command ] |= RegBit_StartNextLine;		// set bit to 1 
		Write( Reg_Command, m_RegShadow[ Reg_Command ] ); 
		 
		m_RegShadow[ Reg_Command ] &= ~RegBit_StartNextLine;	// set bit to 0 
		Write( Reg_Command, m_RegShadow[ Reg_Command ] ); 
		///////////////////////////////////// 
	} 
 
        request.reg = RegISA_ImageData;
        request.param1=(unsigned long)&retval;

	for (j = 0; j < SkipPixels; j++) 
	{ 
             status=ioctl(fileHandle,APISA_READ_USHORT,(unsigned long)&request);
	} 
	for (j = 0; j < Pixels; j++) 
	{ 
             status=ioctl(fileHandle,APISA_READ_USHORT,(unsigned long)&request);
	     *pLineBuffer++ = (unsigned short)retval; 
	} 
	///////////////////////////////////// 
	// Assert done reading line 
	m_RegShadow[ Reg_Command ] |= RegBit_DoneReading;	// set bit to 1 
	Write( Reg_Command, m_RegShadow[ Reg_Command ] ); 
	 
	m_RegShadow[ Reg_Command ] &= ~RegBit_DoneReading;	// set bit to 0 
	Write( Reg_Command, m_RegShadow[ Reg_Command ] ); 
	///////////////////////////////////// 
 
	if ( !m_TDI ) 
	{ 
		///////////////////////////////////// 
		// Wait until camera is done 
		clock_t StopTime = clock() + CLOCKS_PER_SEC;	// wait at most one second 
		while ( true ) 
		{ 
			unsigned short val = 0; 
			Read( Reg_Status, val ); 
			if ( ( val & RegBit_LineDone ) != 0 ) break;// Line done 
			 
			if ( clock() > StopTime ) return 1;		// Timed out 
		} 
	} 
 
	return 0; 
} 
 

 
long CCameraIO::ReadImage( unsigned short* pImageBuffer )
{
        m_RegShadow[ Reg_Command ] |= RegBit_FIFOCache;         // set bit to 1
        Write( Reg_Command, m_RegShadow[ Reg_Command ] );

        long XEnd = long( m_ExposureNumX );
        long SkipC = long( m_ExposureSkipC );
        for (long i = 0; i < m_ExposureSkipR; i++)
        {
                if( InternalReadLine( false, SkipC, XEnd, NULL ) ) return 1;
        }

        long YEnd = long( m_ExposureNumY );
        unsigned short* pLineBuffer = pImageBuffer;
        for (long i = 0; i < YEnd; i++)
        {
                if ( InternalReadLine( true, SkipC, XEnd, pLineBuffer ) ) return 1;
                pLineBuffer += XEnd;
        }

        m_RegShadow[ Reg_Command ] &= !RegBit_FIFOCache;        // set bit to 0
        Write( Reg_Command, m_RegShadow[ Reg_Command ] );

        return 0;
}



// Returns 0 if successfull, 1 if Line_Done poll timed out.
long CCameraIO::InternalReadLine( bool KeepData, long SkipC, long XEnd, unsigned short* pLineBuffer )
{
      struct apIOparam request;
      int retval, status;

        /////////////////////////////////////
        // Clock out the line
        m_RegShadow[ Reg_Command ] |= RegBit_StartNextLine;             // set bit to 1
        Write( Reg_Command, m_RegShadow[ Reg_Command ] );

        m_RegShadow[ Reg_Command ] &= !RegBit_StartNextLine;    // set bit to 0
        Write( Reg_Command, m_RegShadow[ Reg_Command ] );
        /////////////////////////////////////

        request.reg = RegISA_ImageData;
        request.param1=(unsigned long)&retval;

        for (long j = 0; j < SkipC; j++)
             status=ioctl(fileHandle,APISA_READ_USHORT,(unsigned long)&request);

        if ( KeepData )
        {
                for (long j = 0; j < XEnd; j++) {
                  status=ioctl(fileHandle,APISA_READ_USHORT,(unsigned long)&request);
                  *pLineBuffer++ = (unsigned short)retval;
                }
        }
        else
        {
                for (long j = 0; j < XEnd; j++)
                  status=ioctl(fileHandle,APISA_READ_USHORT,(unsigned long)&request);
        }

        /////////////////////////////////////
        // Assert done reading line
        m_RegShadow[ Reg_Command ] |= RegBit_DoneReading;       // set bit to 1
        Write( Reg_Command, m_RegShadow[ Reg_Command ] );

        m_RegShadow[ Reg_Command ] &= !RegBit_DoneReading;      // set bit to 0
        Write( Reg_Command, m_RegShadow[ Reg_Command ] );
        /////////////////////////////////////

        /////////////////////////////////////
        // Wait until camera is done clocking
        clock_t StopTime = clock() + CLOCKS_PER_SEC;    // wait at most one second
        while ( true )
        {
                unsigned short val = 0;
                Read( Reg_Status, val );
                if ( ( val & RegBit_LineDone ) != 0 ) break;// Line done

                clock_t CurrentTime = clock();
                if ( CurrentTime > StopTime ) return 1;         // Timed out
        }

        return 0;
}







