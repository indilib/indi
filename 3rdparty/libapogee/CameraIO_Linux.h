// CameraIO.h: interface for the CCameraIO class.
//
// Copyright (c) 2000 Apogee Instruments Inc.
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_CAMERAIO_H__A2882C82_7CFB_11D4_9155_0060676644C1__INCLUDED_)
#define AFX_CAMERAIO_H__A2882C82_7CFB_11D4_9155_0060676644C1__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "Apogee.h"

enum Camera_Interface{
	Camera_Interface_ISA = 0,
	Camera_Interface_PPI,
	Camera_Interface_PCI
};

enum Camera_SensorType{
	Camera_SensorType_CCD = 0,
	Camera_SensorType_CMOS
};

const long MAXCOLUMNS = 16383;
const long MAXROWS = 16383;
const long MAXHBIN = 8;
const long MAXVBIN = 255;

// Number of write only registers
const long NumWriteRegisters = 8;

const long RegISA_Command = 0x000;			// Register 1 in ISA firmware
const long Reg_Command = 0;					// Register 1 shadow
const unsigned short RegBit_TDIMode = 0x1;			// Bit 0
const unsigned short RegBit_StartTimer = 0x2;		// Bit 1
const unsigned short RegBit_ShutterOverride = 0x4;	// Bit 2
const unsigned short RegBit_ResetSystem = 0x8;		// Bit 3
const unsigned short RegBit_FIFOCache = 0x10;		// Bit 4
const unsigned short RegBit_TriggerEnable = 0x20;	// Bit 5
const unsigned short RegBit_StopFlushing = 0x40;	// Bit 6
const unsigned short RegBit_ShutterEnable = 0x80;	// Bit 7
const unsigned short RegBit_CoolerShutdown = 0x100;	// Bit 8
const unsigned short RegBit_DoneReading = 0x200;	// Bit 9
const unsigned short RegBit_TimerLoad = 0x400;		// Bit 10
const unsigned short RegBit_StartNextLine = 0x800;	// Bit 11
const unsigned short RegBit_StartFlushing = 0x1000;	// Bit 12
const unsigned short RegBit_Focus = 0x2000;			// Bit 13
const unsigned short RegBit_CableLength = 0x4000;	// Bit 14
const unsigned short RegBit_CoolerEnable = 0x8000;	// Bit 15

const long RegISA_Timer = 0x002;			// Register 2 in ISA firmware
const long Reg_Timer = 1;					// Register 2 shadow
const unsigned short RegBitShift_Timer = 0;			// Bit 0
const unsigned short RegBitMask_Timer = 0xFFFF;		// 16 bits

const long RegISA_VBinning = 0x004;			// Register 3 in ISA firmware
const long Reg_VBinning = 2;				// Register 3 shadow
const unsigned short RegBitShift_Timer2 = 0;		// Bit 0
const unsigned short RegBitMask_Timer2 = 0xF;		// 4 bits
const unsigned short RegBitShift_VBinning = 0x8;	// Bit 8
const unsigned short RegBitMask_VBinning = 0xFF;	// 8 bits

const long RegISA_AICCounter = 0x006;		// Register 4 in ISA firmware
const long Reg_AICCounter = 3;				// Register 4 shadow
const unsigned short RegBitShift_AICCounter = 0;	// Bit 0
const unsigned short RegBitMask_AICCounter = 0xFFF;	// 12 bits
const unsigned short RegBitShift_Test2 = 0xC;		// Bit 12
const unsigned short RegBitMask_Test2 = 0xF;		// 4 bits

const long RegISA_TempSetPoint = 0x008;		// Register 5 in ISA firmware
const long Reg_TempSetPoint = 4;			// Register 5 shadow
const unsigned short RegBitShift_TempSetPoint = 0;	// Bit 0
const unsigned short RegBitMask_TempSetPoint = 0xFF;// 8 bits
const unsigned short RegBitShift_PortControl = 0x8;	// Bit 8
const unsigned short RegBitMask_PortControl = 0xFF;	// 8 bits

const long RegISA_PixelCounter = 0x00a;		// Register 6 in ISA firmware
const long Reg_PixelCounter = 5;			// Register 6 shadow
const unsigned short RegBitShift_PixelCounter = 0;		// Bit 0
const unsigned short RegBitMask_PixelCounter = 0xFFF;	// 12 bits
const unsigned short RegBitShift_HBinning = 0xC;		// Bit 12
const unsigned short RegBitMask_HBinning = 0x7;			// 3 bits
const unsigned short RegBit_LoopLock = 0x8000;			// Bit 15

const long RegISA_LineCounter = 0x00c;		// Register 7 in ISA firmware
const long Reg_LineCounter = 6;				// Register 7 shadow
const unsigned short RegBitShift_LineCounter = 0;		// Bit 0
const unsigned short RegBitMask_LineCounter = 0xFFF;	// 12 bits
const unsigned short RegBitShift_Mode = 0xC;			// Bit 12
const unsigned short RegBitMask_Mode = 0xF;				// 4 bits

const long RegISA_BICCounter = 0x00e;		// Register 8 in ISA firmware
const long Reg_BICCounter = 7;				// Register 8 shadow
const unsigned short RegBitShift_BICCounter = 0;	// Bit 0
const unsigned short RegBitMask_BICCounter = 0xFFF;	// 12 bits
const unsigned short RegBitShift_Test = 0xC;		// Bit 12
const unsigned short RegBitMask_Test = 0xF;			// 4 bits

const long RegISA_ImageData = 0x000;		// Register 9 in ISA firmware
const long Reg_ImageData = 8;				// Register 9
const unsigned short RegBitShift_ImageData = 0;		// Bit 0
const unsigned short RegBitMask_ImageData = 0xFFFF;	// 16 bits

const long RegISA_TempData = 0x002;		// Register 10 in ISA firmware
const long Reg_TempData = 9;				// Register 10
const unsigned short RegBitShift_TempData = 0;	// Bit 0
const unsigned short RegBitMask_TempData = 0xFF;	// 8 bits

const long RegISA_Status = 0x006;			// Register 11 in firmware
const long Reg_Status = 10;					// Register 11
const unsigned short RegBit_Exposing = 0x1;			// Bit 0
const unsigned short RegBit_LineDone = 0x2;			// Bit 1
const unsigned short RegBit_CacheReadOK = 0x4;		// Bit 2
const unsigned short RegBit_TempAtMin = 0x10;		// Bit 4
const unsigned short RegBit_TempAtMax = 0x20;		// Bit 5
const unsigned short RegBit_ShutdownComplete = 0x40;// Bit 6
const unsigned short RegBit_TempAtSetPoint = 0x80;	// Bit 7
const unsigned short RegBit_GotTrigger = 0x400;		// Bit 10
const unsigned short RegBit_FrameDone = 0x800;		// Bit 11
const unsigned short RegBit_LoopbackTest = 0x8000;	// Bit 15

const long RegISA_CommandReadback = 0x008;	// Register 12 in ISA firmware
const long Reg_CommandReadback = 11;		// Register 12
// Use RegBit offsets from Reg_Command

const long RegPCI_Command               = 0x000;        // Register 1 in PCI firmware
const long RegPCI_CommandRead   = 0x020;
const long RegPCI_Timer                 = 0x004;        // Register 2 in PCI firmware
const long RegPCI_TimerRead             = 0x024;
const long RegPCI_VBinning              = 0x008;        // Register 3 in PCI firmware
const long RegPCI_VBinningRead  = 0x028;
const long RegPCI_AICCounter            = 0x00C;        // Register 4 in PCI firmware
const long RegPCI_AICCounterRead        = 0x02C;
const long RegPCI_TempSetPoint          = 0x010;        // Register 5 in PCI firmware
const long RegPCI_TempSetPointRead      = 0x030;
const long RegPCI_PixelCounter          = 0x014;        // Register 6 in PCI firmware
const long RegPCI_PixelCounterRead      = 0x034;
const long RegPCI_LineCounter           = 0x018;        // Register 7 in PCI firmware
const long RegPCI_LineCounterRead       = 0x038;
const long RegPCI_BICCounter            = 0x01C;        // Register 8 in PCI firmware
const long RegPCI_BICCounterRead        = 0x03C;
const long RegPCI_ImageData = 0x000;            // Register 9 in PCI firmware
const long RegPCI_TempData = 0x004;                     // Register 10 in PCI firmware
const long RegPCI_Status = 0x00C;                       // Register 11 in firmware
const long RegPCI_CommandReadback = 0x010;      // Register 12 in PCI firmware



class CCameraIO
{
public:

	CCameraIO();
	virtual ~CCameraIO();

	////////////////////////////////////////////////////////////
	// Low level read write methods - Overridables
        bool InitDriver(unsigned short camnum);
        long ReadLine( long SkipPixels, long Pixels, unsigned short* pLineBuffer );
        long Write( unsigned short reg, unsigned short val );
        long Read( unsigned short reg, unsigned short& val );
 
	////////////////////////////////////////////////////////////
	// Camera Settings

	Camera_Status read_Status();		// Current camera state
										// <0: error codes
										// 0: idle
										// 1: flushing
										// 2: waiting for trigger
										// 3: exposing
										// 4: reading
										// 5: downloading
										// 6: line ready
										// 7: image ready

	bool read_Present();				// True if camera is present, false otherwise.
	
	bool read_Shutter();				// Current shutter state, true = open, false = closed.
	void write_Shutter( bool val );
	
	bool read_ForceShutterOpen();				// True: Forces shutter permanently open. False: allows
	void write_ForceShutterOpen( bool val );	// normal shutter operation.

	bool read_LongCable();				// Long cable mode.
	void write_LongCable( bool val );			

	short read_Mode();					// First four bits map to Mode bits used for
	void write_Mode( short val );		// special functions or camera configurations.

	short read_TestBits();				// First four bits to Test bits used for
	void write_TestBits( short val );		// troubleshooting.
	
	short read_Test2Bits();				// First four bits map to Test2 bits used for
	void write_Test2Bits( short val );	// special functions or camera configurations.

	bool read_FastReadout();				// Fast readout mode (used for focusing).
	void write_FastReadout( bool val );	// True means fast focus is on			

	bool read_UseTrigger();				// Triggered exposure mode.
	void write_UseTrigger( bool val );	// True means triggered exposure is on.

	bool m_HighPriority;		// Bost thread priority level during download

	short m_PPRepeat;			// Delay used on parallel port systems.
	
	short m_DataBits;			// Digitization resolution, 8 - 18.

	bool m_FastShutter;			// Capable of 0.001 sec exposure resolution

	bool m_GuiderRelays;		// Capable of outputing autoguider signals

	short m_MaxBinX, m_MaxBinY;	// Maximum binning factors	
	
	double m_MaxExposure;	// Maximum exposure length
	double m_MinExposure;	// Minimum exposure length

	double m_Timeout;		// camera polling timeout value

	////////////////////////////////////////////////////////////
	// Cooler Settings
	// N.B. DAC units = ( m_TempScale * CoolerSetPoint (deg. C ) ) + m_TempCalibration;
	// N.B. Temperature (deg. C) = (DAC units - m_TempCalibration) / m_TempScale
	
	double read_CoolerSetPoint();				// Returns/sets setpoint temperature in degrees
	void write_CoolerSetPoint( double val );	// Celcius.

	Camera_CoolerStatus read_CoolerStatus();	// Returns current cooler status

	Camera_CoolerMode read_CoolerMode();		// Returns/sets current cooler operation mode.
	void write_CoolerMode( Camera_CoolerMode val );

	double read_Temperature();	// Current temperature in degrees Celcius.
	
	bool m_TempControl;			// Temperature can be externally controlled
	short m_TempCalibration;	// Temperature calibration factor.
	double m_TempScale;			// Temperature scaling factor.

	////////////////////////////////////////////////////////////
	// Exposure Settings
	// The following variables are latched in Expose method, until next Reset or GetImage

	short m_BinX, m_BinY;		// Horizontal and vertical binning.
	short m_StartX, m_StartY;	// Zero based subframe start position in unbinned pixels.
	short m_NumX, m_NumY;		// Subframe size in binned pixels.

	////////////////////////////////////////////////////////////
	// Geometry Settings
	// The following variables are latched in Expose method, until next Reset or GetImage
	
	short m_Columns, m_Rows;		// Total columns/rows on CCD (physical).
	short m_ImgColumns, m_ImgRows;	// Unbinned columns/rows in imaging area
	short m_SkipC, m_SkipR;			// Deleted data columns/rows not to be displayed or saved
	short m_HFlush, m_VFlush;		// Horizontal/Vertical flush binning.
	short m_BIC, m_BIR;				// Before Image Column/Row count (dark non-imaging pixels).
	
	////////////////////////////////////////////////////////////
	// CCD Settings

	char m_Sensor[ 256 ];	// Sensor model installed in camera (i.e. Sensor = SITe 502).
	bool m_Color;			// Sensor has color dyes
	double m_Noise;			// Read out noise in e-.
	double m_Gain;			// Gain in e-/ADU units.
	double m_PixelXSize;	// Size of pixel in X direction in micrometers.
	double m_PixelYSize;	// Size of pixel in Y direction in micrometers.

	////////////////////////////////////////////////////////////
	// System methods

	// Resets camera to idle state, will terminate current exposure.
	void Reset();

	// Mask user requested set of IRQS
//	void MaskIrqs();

	// Restore default IRQ mask
//	void UnmaskIrqs();

	// Starts flushing the camera (which should be the normal idle state)
	// If Rows is non-negative, only the specified number of rows are flushed,
	// in which case the method will return only when flushing is completed.
	void Flush( short Rows = -1 );

	// Output byte to auxillary output port (e.g. for driving guider relays).
	void AuxOutput( unsigned char val );

	// Write a 16 bit value to register 1 to 8.
	void RegWrite( short reg, unsigned short val );

	// Read a 16 bit value from register 9 to 12.
	void RegRead( short reg, unsigned short& val );

	// Move the filterwheel to the home position - failure indicates no filterwheel
	//attached or broken filterwheel
	bool FilterHome();
	
	// Move filterwheel to the given slot
	void FilterSet( short Slot );

	////////////////////////////////////////////////////////////
	// Normal exposure methods

	// The Duration parameter is the exposure time in seconds. The Light parameter controls
	// the status of the shutter during the exposure, Light = True opens the shutter, Light
	// = False closes the shutter. Returns immediately after invocation, poll the CameraStatus
	// property to determine the start time of a triggered exposure and the end of an exposure.
	bool Expose( double Duration, bool Light );

	// Returns the pImageData parameter which is a pointer to unsigned short data with NumX*NumY
	// elements.  Can be overridden if necessary
	virtual bool GetImage( unsigned short* pImageData, short& xSize, short& ySize );

	virtual bool BufferImage(char *bufferName );

	////////////////////////////////////////////////////////////
	// Drift scan methods

	// Begins clocking and digitization of a single line of data begining with a vertical clock
	// sequence and ending with a buffer full of line data. Poll the CameraStatus property to
	// determine when the data is ready for download.
	bool DigitizeLine();

	// Returns the pLineData parameter which is a pointer to unsigned short data with NumX elements.
	bool GetLine( unsigned short* pLineData, short& xSize );

        bool BufferDriftScan(char *bufferName, int delay, int rowCount, int nblock , int npipe);

	////////////////////////////////////////////////////////////
	// Easy to use methods

	// Combination of the Expose and GetImage methods. Blocks calling thread for duration
	// of exposure and readout.
	bool Snap( double Duration, bool Light, unsigned short* pImageData, short& xSize, short& ySize );

// Internal variables to keep track of things that can not be read from the firmware
// directly, or are a combination of firmware setting

	bool m_TDI;					// Time drift integration mode

	bool m_WaitingforTrigger;	// camera is waiting for external trigger
	bool m_WaitingforImage;		// camera is exposing and wiating for an to available
	bool m_WaitingforLine;		// camera is clocking and digitizing a row of data

	short m_RegisterOffset;		// Offset from base address used in parallel port systems.
	
	short m_FilterPosition;		// Current filter position
	short m_FilterStepPos;		// Current filter position in our internal array

	bool m_Shutter;				// Last known shutter state
	Camera_Status m_Status;		// Last known camera status

	Camera_Interface m_Interface;		// String acronyms may be used in INI file.
										// 0 or ISA: Industry Standard Architecture bus
										// 1 or PPI: Parallel Port Interface 
										// 2 or PCI: Peripheral Component Interface
	
	Camera_SensorType m_SensorType;		// 0 or CCD: Charge Coupled Device
										// 1 or CMOS: Complementary Metal-Oxide-Silicon

	Camera_CoolerStatus m_CoolerStatus;		// Last known cooler status.
	unsigned int m_IRQMask;     	// Set of IRQs masked on user request										// 0: Off
											// 1: Ramping to set point
											// 2: Correcting
											// 3: Ramping to ambient
											// 4: At ambient
											// 5: Max cooling limit
											// 6: Min cooling limit
											// 7: At set point

	// Latched public variables used during Exposure..GetImage sequence
	short m_ExposureBinX, m_ExposureBinY;		// Horizontal and vertical binning.
	short m_ExposureStartX, m_ExposureStartY;	// Subframe start position in unbinned pixels.
	short m_ExposureNumX, m_ExposureNumY;		// Subframe size in binned pixels.
	short m_ExposureColumns, m_ExposureRows;	// Total columns/rows on CCD (physical).
	short m_ExposureSkipC, m_ExposureSkipR;		// Deleted data columns/rows not to be displayed or saved to disk.
	short m_ExposureHFlush, m_ExposureVFlush;	// Horizontal/Vertical flush binning.
	short m_ExposureBIC, m_ExposureBIR;			// Before Image Column/Row count (dark non-imaging pixels).
	unsigned short m_ExposureAIC;				// Calculated After Image Column count (dark non-imaging pixels).
	unsigned short m_ExposureRemainingLines;	// Number of lines to be clocked out by GetImage
	unsigned short m_ExposureAIR;				// Number of lines to be flushed after GetImage

	////////////////////////////////////////////////////////////
	// Write register shadow variables
	unsigned short m_RegShadow[ NumWriteRegisters ];
	
	unsigned short m_FastShutterBits_Mode;		// Mask to enable fast shutter mode
	unsigned short m_FastShutterBits_Test;		// Mask to enable fast shutter mode
	
	////////////////////////////////////////////////////////////
	// Internal helper routines

	void LoadLineCounter( unsigned short rows );
	void LoadColumnLayout( unsigned short aic, unsigned short bic, unsigned short pixels );
	void LoadTimerAndBinning( double Duration, unsigned short HBin, unsigned short VBin );
	
	void StartFlushing();
	void StopFlushing();

	void InitDefaults();
#ifndef WITHPPI
long ReadImage(short unsigned int *);
long InternalReadLine(bool, long int, long int, unsigned short *);
#endif

private:
        unsigned short m_BaseAddressp2;
        unsigned int   saveIRQS;
        int fileHandle;
#ifdef WITHPPI
        inline void RegisterSelect( unsigned short reg );
        inline unsigned short INPW();
        inline void OUTPW( unsigned short val );                               
#endif

};

#endif // !defined(AFX_CAMERAIO_H__A2882C82_7CFB_11D4_9155_0060676644C1__INCLUDED_)
