#pragma once

#ifdef __cplusplus
extern "C" {
#else
#include <stdbool.h>
#endif

#ifdef WIN32
	#include <Windows.h>	
#else
    #include <libusb-1.0/libusb.h>
#endif

    enum ARTEMISERROR
    {
        ARTEMIS_OK = 0,
        ARTEMIS_INVALID_PARAMETER,
        ARTEMIS_NOT_CONNECTED,
        ARTEMIS_NOT_IMPLEMENTED,
        ARTEMIS_NO_RESPONSE,
        ARTEMIS_INVALID_FUNCTION,
        ARTEMIS_NOT_INITIALIZED,
        ARTEMIS_OPERATION_FAILED,
        ARTEMIS_INVALID_PASSWORD
    };
    
    // Colour properties
    enum ARTEMISCOLOURTYPE
    {
        ARTEMIS_COLOUR_UNKNOWN = 0,
        ARTEMIS_COLOUR_NONE,
        ARTEMIS_COLOUR_RGGB
    };

    //Other enumeration types
    enum ARTEMISPRECHARGEMODE
    {
        PRECHARGE_NONE = 0,		// Precharge ignored
        PRECHARGE_ICPS,			// In-camera precharge subtraction
        PRECHARGE_FULL,			// Precharge sent with image data
    };

    // Camera State
    enum ARTEMISCAMERASTATE
    {
        CAMERA_ERROR = -1,
        CAMERA_IDLE = 0,
        CAMERA_WAITING,
        CAMERA_EXPOSING,
        CAMERA_READING,
        CAMERA_DOWNLOADING,
        CAMERA_FLUSHING,
    };

    // Parameters for ArtemisSendMessage
    enum ARTEMISSENDMSG
    {
        ARTEMIS_LE_LOW				=0,
        ARTEMIS_LE_HIGH				=1,
        ARTEMIS_GUIDE_NORTH			=10,
        ARTEMIS_GUIDE_SOUTH			=11,
        ARTEMIS_GUIDE_EAST			=12,
        ARTEMIS_GUIDE_WEST			=13,
        ARTEMIS_GUIDE_STOP			=14,
    };

    // Parameters for ArtemisGet/SetProcessing
    // These must be powers of 2.
    enum ARTEMISPROCESSING
    {
        ARTEMIS_PROCESS_LINEARISE	=1,	// compensate for JFET nonlinearity
        ARTEMIS_PROCESS_VBE			=2, // adjust for 'Venetian Blind effect'
    };

    // Parameters for ArtemisSetUpADC
    enum ARTEMISSETUPADC
    {
        ARTEMIS_SETUPADC_MODE		=0,
        ARTEMIS_SETUPADC_OFFSETR	=(1<<10),
        ARTEMIS_SETUPADC_OFFSETG	=(2<<10),
        ARTEMIS_SETUPADC_OFFSETB	=(3<<10),
        ARTEMIS_SETUPADC_GAINR		=(4<<10),
        ARTEMIS_SETUPADC_GAING		=(5<<10),
        ARTEMIS_SETUPADC_GAINB		=(6<<10),
    };

    enum ARTEMISPROPERTIESCCDFLAGS
    {
        ARTEMIS_PROPERTIES_CCDFLAGS_INTERLACED =1, // CCD is interlaced type
        ARTEMIS_PROPERTIES_CCDFLAGS_DUMMY=0x7FFFFFFF // force size to 4 bytes
    };

    enum ARTEMISPROPERTIESCAMERAFLAGS
    {
        ARTEMIS_PROPERTIES_CAMERAFLAGS_FIFO					= 1, // Camera has readout FIFO fitted
        ARTEMIS_PROPERTIES_CAMERAFLAGS_EXT_TRIGGER			= 2, // Camera has external trigger capabilities
        ARTEMIS_PROPERTIES_CAMERAFLAGS_PREVIEW				= 4, // Camera can return preview data
        ARTEMIS_PROPERTIES_CAMERAFLAGS_SUBSAMPLE			= 8, // Camera can return subsampled data
        ARTEMIS_PROPERTIES_CAMERAFLAGS_HAS_SHUTTER			= 16, // Camera has a mechanical shutter
        ARTEMIS_PROPERTIES_CAMERAFLAGS_HAS_GUIDE_PORT		= 32, // Camera has a guide port
        ARTEMIS_PROPERTIES_CAMERAFLAGS_HAS_GPIO				= 64, // Camera has GPIO capability
        ARTEMIS_PROPERTIES_CAMERAFLAGS_HAS_WINDOW_HEATER	= 128, // Camera has a window heater
        ARTEMIS_PROPERTIES_CAMERAFLAGS_HAS_EIGHT_BIT_MODE	= 256, // Camera can download 8-bit images
        ARTEMIS_PROPERTIES_CAMERAFLAGS_HAS_OVERLAP_MODE		= 512, // Camera can overlap
        ARTEMIS_PROPERTIES_CAMERAFLAGS_HAS_FILTERWHEEL		= 1024, // Camera has internal filterwheel
        ARTEMIS_PROPERTIES_CAMERAFLAGS_DUMMY				= 0x7FFFFFFF // force size to 4 bytes
    };

    //enum ARTEMISCOOLINGINFO
    //{
    //	/*Info flags
    //	b0-4 capabilities
    //	b0  0 = no cooling 1=cooling
    //	b1   0= always on 1= controllable
    //	b2   0 = on/off control not available  1= on off cooling control
    //	b3    0= no selectable power levels 1= selectable power levels
    //	b4   0 = no temperature set point cooling 1= set point cooling
    //	b5-7 report what�s actually happening
    //	b5  0=normal control 1=warming up
    //	b6   0=cooling off   1=cooling on
    //	b7   0= no set point control 1=set point control*/
    //	ARTEMIS_COOLING_INFO_HASCOOLING          = 1,
    //	ARTEMIS_COOLING_INFO_CONTROLLABLE        = 2,
    //	ARTEMIS_COOLING_INFO_ONOFFCOOLINGCONTROL = 4,
    //	ARTEMIS_COOLING_INFO_POWERLEVELCONTROL   = 8,
    //	ARTEMIS_COOLING_INFO_SETPOINTCONTROL     = 16,
    //	ARTEMIS_COOLING_INFO_WARMINGUP           = 32,
    //	ARTEMIS_COOLING_INFO_COOLINGON           = 64,
    //	ARTEMIS_COOLING_INFO_SETPOINTCONTROL     = 128
    //};

    enum ARTEMISEFWTYPE
    {
        ARTEMIS_EFW1 = 1,
        ARTEMIS_EFW2 = 2
    };

    //Structures

    // camera/CCD properties
    struct ARTEMISPROPERTIES
    {
        int Protocol;
        int nPixelsX;
        int nPixelsY;
        float PixelMicronsX;
        float PixelMicronsY;
        int ccdflags;
        int cameraflags;
        char Description[40];
        char Manufacturer[40];
    };

    typedef void * ArtemisHandle;
#ifndef WIN32
	//typedef bool   BOOL;
#endif // !WIN32

    // -------------------  DLL --------------------------
	int  ArtemisAPIVersion();
	int  ArtemisDLLVersion();
	bool ArtemisIsLocalConnection();
	void ArtemisAllowDebugToConsole(bool value);
    void ArtemisSetDebugCallback(void(*callback)(const char *message));
	void ArtemisSetFirmwareDir(const char * firmwareDir);
    void ArtemisShutdown(); 

	// -------------------  Device --------------------------
	bool			ArtemisDeviceIsPresent(int iDevice);
	bool			ArtemisDevicePresent(  int iDevice);
	bool			ArtemisDeviceInUse(    int iDevice);
	bool			ArtemisDeviceName(     int iDevice, char *pName);
	bool			ArtemisDeviceSerial(   int iDevice, char *pSerial);
	bool			ArtemisDeviceIsCamera( int iDevice);
    bool			ArtemisDeviceHasFilterWheel(int iDevice);
	bool			ArtemisDeviceHasGuidePort(int iDevice);
    int  			ArtemisDeviceGetLibUSBDevice(int iDevice, libusb_device ** device);
	ArtemisHandle	ArtemisConnect(        int iDevice);
	bool			ArtemisIsConnected(ArtemisHandle hCam);
	bool			ArtemisDisconnect( ArtemisHandle handle);
	int				ArtemisRefreshDevicesCount();
	int				ArtemisDeviceCount();

	// ------------------- Camera Info -----------------------------------
	int  ArtemisCameraSerial(	 ArtemisHandle hCam, int* flags, int* serial);
	int  ArtemisColourProperties(ArtemisHandle hCam, enum ARTEMISCOLOURTYPE *colourType, int *normalOffsetX, int *normalOffsetY, int *previewOffsetX, int *previewOffsetY);
	int  ArtemisProperties(		 ArtemisHandle hCam, struct ARTEMISPROPERTIES *pProp);

	// ------------------- Exposure Settings -----------------------------------
	int  ArtemisBin(                             ArtemisHandle hCam, int  x, int  y); // set the x,y binning factors
	int  ArtemisGetBin(                          ArtemisHandle hCam, int *x, int *y); // get the x,y binning factors
	int  ArtemisGetMaxBin(                       ArtemisHandle hCam, int *x, int *y); // get the maximum x,y binning factors
	int  ArtemisGetSubframe(                     ArtemisHandle hCam, int *x, int *y, int *w, int *h); // get the pos and size of imaging subframe
	int  ArtemisSubframe(                        ArtemisHandle hCam, int  x, int  y, int  w, int  h); // set the pos and size of imaging subframe
	int  ArtemisSubframePos(                     ArtemisHandle hCam, int x, int y); // set the start x,y coords for imaging subframe
	int  ArtemisSubframeSize(                    ArtemisHandle hCam, int w, int h); // set the width and height of imaging subframe
	int  ArtemisSetSubSample(                    ArtemisHandle hCam, bool bSub); // Set subsample mode
	bool ArtemisContinuousExposingModeSupported( ArtemisHandle hCam);
	bool ArtemisGetContinuousExposingMode(	     ArtemisHandle hCam);
	int  ArtemisSetContinuousExposingMode(	     ArtemisHandle hCam, bool bEnable);
	bool ArtemisGetDarkMode(                     ArtemisHandle hCam);
	int  ArtemisSetDarkMode(                     ArtemisHandle hCam, bool bEnable);
	int  ArtemisSetPreview(		                 ArtemisHandle hCam, bool bPrev); // Set preview mode
	int  ArtemisAutoAdjustBlackLevel(            ArtemisHandle hCam, bool bEnable);
	int  ArtemisPrechargeMode(		             ArtemisHandle hCam, int mode); // set the Precharge mode
	int  ArtemisEightBitMode(		             ArtemisHandle hCam, bool  eightbit); // set the 8-bit imaging mode
    int  ArtemisGetEightBitMode(				 ArtemisHandle hCam, bool *eightbit);
	int  ArtemisStartOverlappedExposure(         ArtemisHandle hCam);
	bool ArtemisOverlappedExposureValid(         ArtemisHandle hCam);
	int  ArtemisSetOverlappedExposureTime(       ArtemisHandle hCam, float fSeconds); // Set overlapped exposure time
	int  ArtemisTriggeredExposure(               ArtemisHandle hCam, bool bAwaitTrigger); // Set external exposure trigger mode
	int  ArtemisGetProcessing(                   ArtemisHandle hCam); // Get current image processing options // Returns 0 on error.
	int  ArtemisSetProcessing(                   ArtemisHandle hCam, int options); // Set current image processing options

	// ------------------- Exposures -----------------------------------
	int   ArtemisStartExposure(             ArtemisHandle hCam, float seconds);
	int   ArtemisStartExposureMS(           ArtemisHandle hCam, int ms);
	int   ArtemisAbortExposure(             ArtemisHandle hCam);
	int   ArtemisStopExposure(              ArtemisHandle hCam);
	bool  ArtemisImageReady(                ArtemisHandle hCam);
	int   ArtemisCameraState(               ArtemisHandle hCam);
	float ArtemisExposureTimeRemaining(     ArtemisHandle hCam);
	int   ArtemisDownloadPercent(           ArtemisHandle hCam);
	int   ArtemisGetImageData(              ArtemisHandle hCam, int *x, int *y, int *w, int *h, int *binx, int *biny);
	void* ArtemisImageBuffer(               ArtemisHandle hCam);
	float ArtemisLastExposureDuration(      ArtemisHandle hCam);
	char* ArtemisLastStartTime(             ArtemisHandle hCam);
	int   ArtemisLastStartTimeMilliseconds( ArtemisHandle hCam);

#ifdef WIN32
	int ArtemisExposureReadyCallback(       ArtemisHandle hCam, HWND hWnd, int msg, int wParam, int lParam);
#endif

	// ------------------- Amplifier -----------------------------------
	int  ArtemisAmplifier(		     ArtemisHandle hCam, bool bOn); // set the CCD amplifier on or off
	bool ArtemisGetAmplifierSwitched(ArtemisHandle hCam); // return true if amp switched off during exposures
	int  ArtemisSetAmplifierSwitched(ArtemisHandle hCam, bool bSwitched); // set whether amp switched off during exposures

	// ------------ Camera Specific Options -------------
	bool ArtemisHasCameraSpecificOption(    ArtemisHandle handle, unsigned short int id);
	int  ArtemisCameraSpecificOptionGetData(ArtemisHandle handle, unsigned short int id, unsigned char * data, int dataLength, int *actualLength);
	int  ArtemisCameraSpecificOptionSetData(ArtemisHandle handle, unsigned short int id, unsigned char * data, int dataLength);

	// ------------------- Column Repair ----------------------------------	
	int ArtemisSetColumnRepairColumns(		ArtemisHandle handle, int   nColumn, unsigned short int * columns);
	int ArtemisGetColumnRepairColumns(		ArtemisHandle handle, int * nColumn, unsigned short int * columns);
	int ArtemisClearColumnRepairColumns(	ArtemisHandle handle);
	int ArtemisSetColumnRepairFixColumns(	ArtemisHandle handle, bool value);
	int ArtemisGetColumnRepairFixColumns(	ArtemisHandle handle, bool * value);
	int ArtemisGetColumnRepairCanFixColumns(ArtemisHandle handle, bool * value);

	// ---------------- EEPROM -------------------------
	int ArtemisCanInteractWithEEPROM(ArtemisHandle handle, bool * canInteract);
	int ArtemisWriteToEEPROM(	     ArtemisHandle handle, char * password, int address, int length, const unsigned char * data);
	int ArtemisReadFromEEPROM(	     ArtemisHandle handle, char * password, int address, int length,       unsigned char * data);
  
	// ------------------- Filter Wheel -----------------------------------
	int ArtemisFilterWheelInfo(ArtemisHandle hCam, int *numFilters, int *moving, int *currentPos, int *targetPos);
	int ArtemisFilterWheelMove(ArtemisHandle hCam, int targetPos);
	bool ArtemisEFWIsPresent(int i);
	int	 ArtemisEFWGetDeviceDetails(int i, enum ARTEMISEFWTYPE * type, char * serialNumber);
	ArtemisHandle ArtemisEFWConnect(int i);
	bool ArtemisEFWIsConnected(ArtemisHandle handle);
	int ArtemisEFWDisconnect( ArtemisHandle handle);
	int ArtemisEFWGetDetails( ArtemisHandle handle, enum ARTEMISEFWTYPE * type, char * serialNumber);
	int ArtemisEFWNmrPosition(ArtemisHandle handle, int * nPosition);
	int ArtemisEFWSetPosition(ArtemisHandle handle, int   iPosition);
	int ArtemisEFWGetPosition(ArtemisHandle handle, int * iPosition, bool * isMoving);

	// ------------------- Firmware ----------------------------------------	
	bool ArtemisCanUploadFirmware(ArtemisHandle handle);
	int  ArtemisUploadFirmware(   ArtemisHandle handle, char * fileName, char * password);

	// ------------------- Gain -----------------------------------
	int ArtemisGetGain(ArtemisHandle hCam, bool isPreview, int *gain, int *offset);
	int ArtemisSetGain(ArtemisHandle hCam, bool isPreview, int  gain, int  offset);

	// ------------------- GPIO -----------------------------------
	int ArtemisGetGpioInformation(ArtemisHandle hCam, int* lineCount, int* lineValues);
	int ArtemisSetGpioDirection(  ArtemisHandle hCam, int directionMask);
	int ArtemisSetGpioValues(     ArtemisHandle hCam, int lineValues);

	// ------------------- Guiding -----------------------------------
	int ArtemisGuide(					 ArtemisHandle hCam, int axis); // activate a guide relay
	int ArtemisGuidePort(				 ArtemisHandle hCam, int nibble);
	int ArtemisPulseGuide(				 ArtemisHandle hCam, int axis, int milli);
	int ArtemisStopGuiding(				 ArtemisHandle hCam);
	int ArtemisStopGuidingBeforeDownload(ArtemisHandle hCam, bool bEnable);

	// ------------------- Lens -----------------------------------
	int ArtemisGetLensAperture(ArtemisHandle hCam, int* aperture);
	int ArtemisGetLensFocus(   ArtemisHandle hCam, int* focus);
	int ArtemisGetLensLimits(  ArtemisHandle hCam, int* apertureMin, int* apertureMax, int* focusMin, int* focusMax);
	int ArtemisInitializeLens( ArtemisHandle hCam);
	int ArtemisSetLensAperture(ArtemisHandle hCam, int aperture);
	int ArtemisSetLensFocus(   ArtemisHandle hCam, int focus);

	// ------------------- Shutter ----------------------------------	
	int ArtemisCanControlShutter( ArtemisHandle handle, bool * canControl);
	int ArtemisOpenShutter(		  ArtemisHandle handle);
	int ArtemisCloseShutter(	  ArtemisHandle handle);
	int ArtemisCanSetShutterSpeed(ArtemisHandle handle, bool *canSetShutterSpeed);
	int ArtemisGetShutterSpeed(	  ArtemisHandle handle, int *speed);
	int ArtemisSetShutterSpeed(	  ArtemisHandle handle, int  speed);

	// ------------------- Temperature -----------------------------------
	int ArtemisTemperatureSensorInfo(ArtemisHandle hCam, int sensor, int* temperature);
	int ArtemisSetCooling(			 ArtemisHandle hCam, int setpoint);
	int ArtemisCoolingInfo(			 ArtemisHandle hCam, int* flags, int* level, int* minlvl, int* maxlvl, int* setpoint);
	int ArtemisCoolerWarmUp(		 ArtemisHandle hCam);
	int ArtemisGetWindowHeaterPower( ArtemisHandle hCam, int* windowHeaterPower);
	int ArtemisSetWindowHeaterPower( ArtemisHandle hCam, int windowHeaterPower);

#ifdef __cplusplus
}
#endif
