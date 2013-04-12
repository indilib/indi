//=============================================================================
#if 0
	File name  : sbigcam.h
	Driver type: SBIG CCD Camera INDI Driver

	Copyright (C) 2005-2006 Jan Soldan (jsoldan AT asu DOT cas DOT cz)
	251 65 Ondrejov-236, Czech Republic
	
	Acknowledgement:
  Jasem Mutlaq 	(mutlaqja AT ikarustech DOT com)
	Matt Longmire 	(matto AT sbig DOT com)

	This library is free software; you can redistribute it and/or modify 
	it under the terms of the GNU Lesser General Public License as published 
	by the Free Software Foundation; either version 2.1 of the License, or 
	(at your option) any later version.

	This library is distributed in the hope that it will be useful, but 
	WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY 
	or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public 
	License for more details.

	You should have received a copy of the GNU Lesser General Public License 
	along with this library; if not, write to the Free Software Foundation, 
	Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA
#endif
//=============================================================================
#ifndef _SBIG_CAM_
#define _SBIG_CAM_

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <string>
#include "sbigudrv.h"

// Define INDI if INDI driver should be generated, otherwise only SbigCam class without
// INDI support will be generated.
#define INDI

#define USE_CCD_FRAME_STANDARD_PROPERTY
//#define USE_CCD_BINNING_STANDARD_PROPERTY

#define USE_BLOB_COMPRESS

#ifdef INDI
	#include <ctype.h>
	#include <sys/stat.h>
	#include <sys/time.h>
	#include <sys/types.h>
	#include <sys/socket.h>
	#include <netinet/in.h>
	#include <netdb.h>
	#include <zlib.h>
	#include <memory>
	
	#include <fitsio.h>
	
	#include <indidevapi.h>
	#include <eventloop.h>
	#include <indicom.h>
	#include <lilxml.h>
	#include <base64.h>
#endif

using std::string;

/*
	How to use SBIG CFW-10 SA under Linux:

 1) If you have a RS-232 port on you computer, localize its serial device. 
    It may look like this one: /dev/ttyS0
		In the case you use an USB to RS-232 adapter, the device may be 
    located at: /dev/ttyUSB0

 2) Make a symbolic link to this device with the following name:
		ln -s /dev/ttyUSB0 ~/kdeedu/kstars/kstars/indi/sbigcfw
		Please note that the SBIG CFW-10 SA _always_ uses device named sbigcfw.

 3) Change mode of this device:
    chmod a+rw /dev/ttyUSB0

 4) Inside KStars:
		a) Open CFW panel
    b) Set CFW Type to CFW-10 SA
    c) Press Connect button
    d) The CFW's detection may take a few seconds due to its internal initialization.	
*/
//=============================================================================
const int		INVALID_HANDLE_VALUE		= -1;	// for file operations
//=============================================================================
// SBIG temperature constants:
const double			T0 								= 25.000;
const double    	MAX_AD 						= 4096.000;
const double    	R_RATIO_CCD 				= 2.570;
const double    	R_BRIDGE_CCD				= 10.000;
const double    	DT_CCD 						= 25.000;
const double    	R0 								= 3.000;
const double    	R_RATIO_AMBIENT			= 7.791;
const double    	R_BRIDGE_AMBIENT		= 3.000;
const double    	DT_AMBIENT 				= 45.000;
//=============================================================================
// SBIG CCD camera port definitions:
#define			SBIG_USB0 					"sbigusb0"
#define			SBIG_USB1 					"sbigusb1"
#define			SBIG_USB2 					"sbigusb2"
#define			SBIG_USB3 					"sbigusb3"
#define			SBIG_LPT0 					"sbiglpt0"
#define			SBIG_LPT1 					"sbiglpt1"
#define			SBIG_LPT2 					"sbiglpt2"
//=============================================================================
# ifdef INDI

// DEVICE:
#define 			DEVICE_NAME						"SBIG"

// GROUP:
#define 			CAMERA_GROUP						"Camera"
#define 			CFW_GROUP							"CFW"
#define 			TEMPERATURE_GROUP				"Temperature"
#define 			FRAME_GROUP						"Frame"
#define 			EXPOSURE_GROUP					"Exposure"

// PRODUCT:
#define 			PRODUCT_NAME_T					"NAME"
#define 			PRODUCT_LABEL_T				"Name"
#define 			PRODUCT_ID_NAME_T			"ID"
#define 			PRODUCT_ID_LABEL_T			"ID"	
#define			CCD_PRODUCT_NAME_TP		"CCD_PRODUCT"
#define			CCD_PRODUCT_LABEL_TP		"Product"
#define			CFW_PRODUCT_NAME_TP		"CFW_PRODUCT"
#define			CFW_PRODUCT_LABEL_TP		"Product"

// CONNECT & DISCONNECT:
#define			CONNECT_NAME_S							"CONNECT"
#define			CONNECT_LABEL_S						"Connect"
#define			DISCONNECT_NAME_S					"DISCONNECT"
#define			DISCONNECT_LABEL_S					"Disconnect"
#define			CCD_CONNECTION_NAME_SP			"CONNECTION"
#define			CCD_CONNECTION_LABEL_SP		"Connection"
#define			CFW_CONNECTION_NAME_SP			"CFW_CONNECTION"
#define			CFW_CONNECTION_LABEL_SP		"Connection"

// DEVICE PORT:
#define			PORT_NAME_T								"PORT"
#define			PORT_LABEL_T								"Port"
#define			CCD_DEVICE_PORT_NAME_TP			"DEVICE_PORT"
#define			CCD_DEVICE_PORT_LABEL_TP		"Device"

// CCD FAN:
#define			CCD_FAN_ON_NAME_S					"ON"
#define			CCD_FAN_ON_LABEL_S					"On"
#define			CCD_FAN_OFF_NAME_S					"OFF"
#define			CCD_FAN_OFF_LABEL_S				"Off"
#define			CCD_FAN_NAME_SP						"CCD_FAN"
#define			CCD_FAN_LABEL_SP						"Fan"

// CCD REQUESTS:
#define			CCD_IMAGING_NAME_S					"IMAGING"
#define			CCD_IMAGING_LABEL_S				"Imaging"
#define			CCD_TRACKING_NAME_S				"TRACKING"
#define			CCD_TRACKING_LABEL_S				"Tracking"
#define			CCD_EXT_TRACKING_NAME_S			"EXT_TRACKING"
#define			CCD_EXT_TRACKING_LABEL_S		"Ext.Tracking"
#define			CCD_REQUEST_NAME_SP				"CCD_REQUEST"
#define			CCD_REQUEST_LABEL_SP				"CCD"

// CCD COOLER:
#define			CCD_COOLER_NAME_N					"COOLER"
#define			CCD_COOLER_LABEL_N					"[%]"
#define			CCD_COOLER_NAME_NP					"CCD_COOLER"
#define			CCD_COOLER_LABEL_NP				"Cooler"
const double		CCD_COOLER_THRESHOLD = 95.0;	

// CCD TEMPERATURE:
#define			CCD_TEMPERATURE_NAME_N			"CCD_TEMPERATURE_VALUE"
#define			CCD_TEMPERATURE_LABEL_N			"[C]"
#define			CCD_TEMPERATURE_NAME_NP			"CCD_TEMPERATURE"
#define			CCD_TEMPERATURE_LABEL_NP		"Temperature"
const double		MIN_CCD_TEMP		=  -70.0;
const double		MAX_CCD_TEMP		= 	40.0;
const double		CCD_TEMP_STEP 	= 	0.1;
const double		DEF_CCD_TEMP		= 	0.0;
const double		TEMP_DIFF			= 	0.5;

// CCD TEMPERATURE POOLING:
#define			CCD_TEMPERATURE_POLLING_NAME_N 		"TEMPERATURE_POLLING"
#define			CCD_TEMPERATURE_POLLING_LABEL_N 	"[sec]"
#define			CCD_TEMPERATURE_POLLING_NAME_NP		"CCD_TEMPERATURE_POLLING"
#define			CCD_TEMPERATURE_POLLING_LABEL_NP	"Polling Time"
const double		MIN_POLLING_TIME		= 1.0;
const double		MAX_POLLING_TIME		= 3600.0;
const double		STEP_POLLING_TIME	= 1.0;
const double		CUR_POLLING_TIME		= 10.0;

// CCD TEMPERATURE MSG:
#define			CCD_TEMPERATURE_MSG_YES_NAME_S		"TEMPERATURE_MSG_YES"
#define			CCD_TEMPERATURE_MSG_YES_LABEL_S		"Yes"
#define			CCD_TEMPERATURE_MSG_NO_NAME_S			"TEMPERATURE_MSG_NO"
#define			CCD_TEMPERATURE_MSG_NO_LABEL_S		"No"
#define			CCD_TEMPERATURE_MSG_NAME_SP				"CCD_TEMPERATURE_MSG"
#define			CCD_TEMPERATURE_MSG_LABEL_SP			"Send MSG"

// CCD FRAME TYPES:
#define			CCD_FRAME_LIGHT_NAME_N 			"FRAME_LIGHT"
#define			CCD_FRAME_DARK_NAME_N			"FRAME_DARK"
#define			CCD_FRAME_FLAT_NAME_N 			"FRAME_FLAT"
#define			CCD_FRAME_BIAS_NAME_N			"FRAME_BIAS"
#define			CCD_FRAME_LIGHT_LABEL_N 		"Light"
#define			CCD_FRAME_DARK_LABEL_N			"Dark"
#define			CCD_FRAME_FLAT_LABEL_N 		"Flat"
#define			CCD_FRAME_BIAS_LABEL_N			"Bias"
#define			CCD_FRAME_TYPE_NAME_NP			"CCD_FRAME_TYPE"
#define			CCD_FRAME_TYPE_LABEL_NP			"Type"

// CCD BINNING:
const int		CCD_BIN_1x1_I = 0;
const int		CCD_BIN_2x2_I = 1;
const int		CCD_BIN_3x3_I = 2;
const int		CCD_BIN_9x9_I = 9;	
const int		CCD_BIN_2x2_E = 7;
const int		CCD_BIN_3x3_E = 8;

#ifdef USE_CCD_BINNING_STANDARD_PROPERTY
	#define			CCD_HOR_BIN_NAME_N					"HOR_BIN"
	#define			CCD_HOR_BIN_LABEL_N				"Horizontal"
	#define			CCD_VER_BIN_NAME_N					"VER_BIN"
	#define			CCD_VER_BIN_LABEL_N				"Vertical"
	#define 			CCD_BINNING_NAME_NP				"CCD_BINNING"
	#define 			CCD_BINNING_LABEL_NP				"Binning"
	#define			CCD_MIN_BIN								1
	#define 			CCD_MAX_BIN								3
#else
	#define			CCD_BIN_1x1_I_NAME_S				"CCD_BIN_1x1_I"
	#define			CCD_BIN_1x1_I_LABEL_S			"1x1 On Chip"
	#define			CCD_BIN_2x2_I_NAME_S				"CCD_BIN_2x2_I"
	#define			CCD_BIN_2x2_I_LABEL_S			"2x2 On Chip"
	#define			CCD_BIN_3x3_I_NAME_S				"CCD_BIN_3x3_I"
	#define			CCD_BIN_3x3_I_LABEL_S			"3x3 On Chip"
	#define			CCD_BIN_9x9_I_NAME_S				"CCD_BIN_9x9_I"
	#define			CCD_BIN_9x9_I_LABEL_S			"9x9 On Chip"
	#define			CCD_BIN_2x2_E_NAME_S				"CCD_BIN_2x2_E"
	#define			CCD_BIN_2x2_E_LABEL_S			"2x2 Off Chip"
	#define			CCD_BIN_3x3_E_NAME_S				"CCD_BIN_3x3_E"
	#define			CCD_BIN_3x3_E_LABEL_S			"3x3 Off Chip"
	#define 			CCD_BINNING_MODE_NAME_SP		"CCD_BINNING_MODE"
	#define 			CCD_BINNING_MODE_LABEL_SP	"Binning"
#endif

// CCD PIXEL INFO:
#define			CCD_PIXEL_WIDTH_NAME_N				"PIXEL_WIDTH"
#define			CCD_PIXEL_WIDTH_LABEL_N				"Width"
#define			CCD_PIXEL_HEIGHT_NAME_N				"PIXEL_HEIGHT"
#define			CCD_PIXEL_HEIGHT_LABEL_N			"Height"
#define			CCD_PIXEL_INFO_NAME_NP				"CCD_PIXEL_INFO"
#define			CCD_PIXEL_INFO_LABEL_NP			"Pixel Size [um]"

// CCD FRAME
#ifdef USE_CCD_FRAME_STANDARD_PROPERTY
	#define			CCD_FRAME_X_NAME_N					"X"
	#define			CCD_FRAME_X_LABEL_N				"Left"
	#define			CCD_FRAME_Y_NAME_N					"Y"
	#define			CCD_FRAME_Y_LABEL_N				"Top"
	#define			CCD_FRAME_W_NAME_N					"WIDTH"
	#define			CCD_FRAME_W_LABEL_N				"Width"
	#define			CCD_FRAME_H_NAME_N					"HEIGHT"
	#define			CCD_FRAME_H_LABEL_N				"Height"
	#define			CCD_FRAME_NAME_NP					"CCD_FRAME"
	#define			CCD_FRAME_LABEL_NP					"Position"
#else
	// CCD FRAME X:
	#define			CCD_FRAME_X_NAME_N					"FRAME_X"
	#define			CCD_FRAME_X_LABEL_N				"left"
	#define			CCD_FRAME_X_NAME_NP				"CCD_FRAME_X"
	#define			CCD_FRAME_X_LABEL_NP				"Position"
	// CCD FRAME Y:
	#define			CCD_FRAME_Y_NAME_N					"FRAME_Y"
	#define			CCD_FRAME_Y_LABEL_N				"Top"
	#define			CCD_FRAME_Y_NAME_NP				"CCD_FRAME_Y"
	#define			CCD_FRAME_Y_LABEL_NP				"Position"
	// CCD FRAME W:
	#define			CCD_FRAME_W_NAME_N					"FRAME_W"
	#define			CCD_FRAME_W_LABEL_N				"Width"
	#define			CCD_FRAME_W_NAME_NP				"CCD_FRAME_W"
	#define			CCD_FRAME_W_LABEL_NP				"Size"
	// CCD FRAME H:
	#define			CCD_FRAME_H_NAME_N					"FRAME_H"
	#define			CCD_FRAME_H_LABEL_N				"Height"
	#define			CCD_FRAME_H_NAME_NP				"CCD_FRAME_H"
	#define			CCD_FRAME_H_LABEL_NP				"Size"
#endif

// CCD EXPOSE DURATION [s]:
#define 			CCD_EXPOSE_DURATION_NAME_N		"CCD_EXPOSURE_VALUE"
#define 			CCD_EXPOSE_DURATION_LABEL_N		"[sec]"
#define 			CCD_EXPOSE_DURATION_NAME_NP		"CCD_EXPOSURE"
#define 			CCD_EXPOSE_DURATION_LABEL_NP	"Time"
const double		MIN_EXP_TIME		= 0.0;
const double		MAX_EXP_TIME		= 3600.0;
const double		EXP_TIME_STEP	= 0.01;
const double		DEF_EXP_TIME		= 1.0;

// BLOB:
#define			BLOB_NAME_B 						"FITS_BLOB"
#define			BLOB_LABEL_B						"FITS"
#define			BLOB_NAME_BP 					"CCD_FITS_BLOB"
#define			BLOB_LABEL_BP					"BLOB"

#ifdef USE_BLOB_COMPRESS 
	#define		BLOB_FORMAT_B					".fits.z"		
#else
	#define		BLOB_FORMAT_B					".fits"	
#endif

// FITS file name:
#define			FITS_NAME_T						"NAME"
#define			FITS_LABEL_T						"Name"
#define			FITS_NAME_TP						"FITS_NAME"
#define			FITS_LABEL_TP					"FITS"

// CFW TYPES:
#define			CFW1_NAME_S						"CFW1"
#define			CFW1_LABEL_S						"CFW-2"
#define			CFW2_NAME_S						"CFW2"
#define			CFW2_LABEL_S						"CFW-5"
#define			CFW3_NAME_S						"CFW3"
#define			CFW3_LABEL_S						"CFW-6A"
#define			CFW4_NAME_S						"CFW4"
#define			CFW4_LABEL_S						"CFW-8"
#define			CFW5_NAME_S						"CFW5"
#define			CFW5_LABEL_S						"CFW-402"
#define			CFW6_NAME_S						"CFW6"
#define			CFW6_LABEL_S						"CFW-10"
#define			CFW7_NAME_S						"CFW7"
#define			CFW7_LABEL_S						"CFW-10 SA"
#define			CFW8_NAME_S						"CFW8"
#define			CFW8_LABEL_S						"CFW-L"
#define			CFW9_NAME_S						"CFW9"
#define			CFW9_LABEL_S						"CFW-9"
#ifdef	 USE_CFW_AUTO
	#define			CFW10_NAME_S						"CFW10"
	#define			CFW10_LABEL_S						"CFW-Auto"
	const int		MAX_CFW_TYPES = 10;		
#else
	const int		MAX_CFW_TYPES = 9;			
#endif

#define			CFW_TYPE_NAME_SP				"CFW_TYPE"
#define			CFW_TYPE_LABEL_SP			"Type"

// CFW SLOTS:
#define			CFW_SLOT_NAME_N				"SLOT"
#define			CFW_SLOT_LABEL_N				"Slot"
#define			CFW_SLOT_NAME_NP				"FILTER_SLOT"
#define			CFW_SLOT_LABEL_NP			"Goto"
const int		MIN_FILTER_SLOT 	= 1;
const int		MAX_FILTER_SLOT 	= 10;
const int		FILTER_SLOT_STEP 	= 1;
const int		DEF_FILTER_SLOT	= 1;

// Auxiliary:
#define 			UNKNOWN_LABEL					"Unknown"

// INDI timeout:
const double	INDI_TIMEOUT				= 5.0;
const int		POLL_TEMPERATURE_MS	= 10000;		// Temperature polling time (ms)
const int		POLL_EXPOSURE_MS		= 1000;			// Exposure polling time (ms)

// Define byte order:
#define GET_BIG_ENDIAN(p) ( ((p & 0xff) << 8) | (p  >> 8))
//(((p << 8) & 0xff00) | ((p >> 8) & 0xff))
#endif // INDI
//=============================================================================
typedef enum
{
	CCD_THERMISTOR,
	AMBIENT_THERMISTOR
}THERMISTOR_TYPE;
//=============================================================================
class SbigCam
{
	protected:	 
 	int						m_fd;
 	CAMERA_TYPE		m_camera_type;
 	int						m_drv_handle; 
 	bool					m_link_status;
 	char						m_dev_name[PATH_MAX];
	string					m_start_exposure_timestamp;

	#ifdef INDI

	// CAMERA GROUP:
	IText									m_icam_product_t[2];
	ITextVectorProperty		m_icam_product_tp;	

	IText									m_icam_device_port_t[1];
	ITextVectorProperty		m_icam_device_port_tp;

	ISwitch 								m_icam_connection_s[2];
	ISwitchVectorProperty	m_icam_connection_sp;
	 
	// TEMPERATURE GROUP:
	ISwitch								m_icam_fan_state_s[2];
	ISwitchVectorProperty	m_icam_fan_state_sp;

	INumber 								m_icam_temperature_n[1];   
 	INumberVectorProperty	m_icam_temperature_np;
	double									m_icam_temperature;

	INumber								m_icam_cooler_n[1]; 
	INumberVectorProperty	m_icam_cooler_np;

	INumber								m_icam_temperature_polling_n[1]; 
	INumberVectorProperty	m_icam_temperature_polling_np;	

	ISwitch								m_icam_temperature_msg_s[2];
	ISwitchVectorProperty	m_icam_temperature_msg_sp;

	// FRAME GROUP:
  ISwitch 								m_icam_frame_type_s[4];
	ISwitchVectorProperty	m_icam_frame_type_sp; 

	ISwitch								m_icam_ccd_request_s[3];
	ISwitchVectorProperty	m_icam_ccd_request_sp;

	#ifdef USE_CCD_BINNING_STANDARD_PROPERTY
	INumber								m_icam_ccd_binning_n[2]; 
	INumberVectorProperty	m_icam_ccd_binning_np;	
	#else
	ISwitch								m_icam_binning_mode_s[6];
	ISwitchVectorProperty	m_icam_binning_mode_sp; 
	#endif

	INumber 								m_icam_ccd_info_n[1];
	INumberVectorProperty	m_icam_ccd_info_np;

	INumber 								m_icam_pixel_size_n[2];
	INumberVectorProperty	m_icam_pixel_size_np;
	 
	#ifdef USE_CCD_FRAME_STANDARD_PROPERTY
	INumber 								m_icam_ccd_frame_n[4];
	INumberVectorProperty	m_icam_ccd_frame_np;
	#else
	INumber 								m_icam_frame_x_n[1];
 	INumberVectorProperty	m_icam_frame_x_np;
	INumber 								m_icam_frame_y_n[1];
 	INumberVectorProperty	m_icam_frame_y_np;
	INumber 								m_icam_frame_w_n[1];
 	INumberVectorProperty	m_icam_frame_w_np;
	INumber 								m_icam_frame_h_n[1];
 	INumberVectorProperty	m_icam_frame_h_np;	
	#endif
 
	// CFW GROUP:
	IText									m_icfw_product_t[2];
	ITextVectorProperty		m_icfw_product_tp;	

	ISwitch								m_icfw_type_s[MAX_CFW_TYPES];
	ISwitchVectorProperty	m_icfw_type_sp;

	ISwitch 								m_icfw_connection_s[2];
	ISwitchVectorProperty	m_icfw_connection_sp;

	INumber								m_icfw_slot_n[1];
 	INumberVectorProperty	m_icfw_slot_np;

	// EXPOSURE GROUP:
  INumber 								m_icam_expose_time_n[1];
	INumberVectorProperty	m_icam_expose_time_np;
	double									m_icam_expose_time;

    // INDI BLOBs: 
  IBLOB 									m_icam_fits_b;
  IBLOBVectorProperty		m_icam_fits_bp;

	// FITS file name:	
	IText									m_icam_fits_name_t[1];
	ITextVectorProperty		m_icam_fits_name_tp;

	#endif	// INDI

	public:
								SbigCam();
								SbigCam(const char* device_name);
 	virtual				~SbigCam();

 	inline int			GetFileDescriptor(){return(m_fd);}
 	inline void		SetFileDescriptor(int val = -1){m_fd = val;}
 	inline bool		IsDeviceOpen(){return((m_fd == -1) ? false : true);}

 	inline CAMERA_TYPE GetCameraType(){return(m_camera_type);}
 	inline void		SetCameraType(CAMERA_TYPE val = NO_CAMERA){m_camera_type = val;}

 	inline int 		GetDriverHandle(){return(m_drv_handle);}
 	inline void		SetDriverHandle(int val = INVALID_HANDLE_VALUE){m_drv_handle = val;}

 	inline bool		GetLinkStatus(){return(m_link_status);}
 	inline void		SetLinkStatus(bool val = false){m_link_status = val;}

	inline char		*GetDeviceName(){return(m_dev_name);}
	int						SetDeviceName(const char*);

	inline string	GetStartExposureTimestamp(){return(m_start_exposure_timestamp);}
	inline void		SetStartExposureTimestamp(const char *p)
								{
									m_start_exposure_timestamp = p;
								}

 	// Driver Related Commands:
	int				GetCfwSelType();
 	int 				OpenDevice(const char *name);
 	int				CloseDevice();
 	int				GetDriverInfo(GetDriverInfoParams *, void *);
 	int				SetDriverHandle(SetDriverHandleParams *);
 	int				GetDriverHandle(GetDriverHandleResults *);

 	// Exposure Related Commands:
 	int				StartExposure(StartExposureParams2 *);
 	int				EndExposure(EndExposureParams *);
 	int				StartReadout(StartReadoutParams *);
 	int				ReadoutLine(ReadoutLineParams *, unsigned short *results, 
												bool subtract);
 	int				DumpLines(DumpLinesParams *);
 	int				EndReadout(EndReadoutParams *);

 	// Temperature Related Commands:
 	int				SetTemperatureRegulation(SetTemperatureRegulationParams *); 
 	int				SetTemperatureRegulation(double temp, bool enable = true);
 	int				QueryTemperatureStatus(QueryTemperatureStatusResults *);
 	int				QueryTemperatureStatus(bool &enabled, double &ccdTemp,
                              		double &setpointT, double &power);

 	// External Control Commands:
 	int				ActivateRelay(ActivateRelayParams *);
 	int				PulseOut(PulseOutParams *);
 	int				TxSerialBytes(TXSerialBytesParams *, TXSerialBytesResults *);
 	int				GetSerialStatus(GetSerialStatusResults *);
 	int				AoTipTilt(AOTipTiltParams *);
 	int				AoSetFocus(AOSetFocusParams *);
 	int				AoDelay(AODelayParams *);
 	int 				Cfw(CFWParams *, CFWResults *);
	
 	// General Purpose Commands:
 	int				EstablishLink();
 	int				GetCcdInfo(GetCCDInfoParams *, void *);
 	int				QueryCommandStatus(	QueryCommandStatusParams *, 
																QueryCommandStatusResults *);
 	int				MiscellaneousControl(MiscellaneousControlParams *);
 	int				ReadOffset(ReadOffsetParams *, ReadOffsetResults *);
 	int				GetLinkStatus(GetLinkStatusResults *);
 	string			GetErrorString(int err);
 	int				SetDriverControl(SetDriverControlParams *);
 	int				GetDriverControl(GetDriverControlParams *, 
														GetDriverControlResults *);
 	int				UsbAdControl(USBADControlParams *);
 	int				QueryUsb(QueryUSBResults *);
 	int				RwUsbI2c(RWUSBI2CParams *);
 	int				BitIo(BitIOParams *, BitIOResults *);

	// SBIG's software interface to the Universal Driver Library function:
 	int				SBIGUnivDrvCommand(PAR_COMMAND, void *, void *);
	
 	// High level functions:
 	bool			CheckLink();
 	string			GetCameraName();
	string			GetCameraID();
	int				GetCcdSizeInfo(	int ccd, int rm, int &frmW, int &frmH, 
						  							double &pixW, double &pixH);
	int				GetNumOfCcdChips();
	bool			IsFanControlAvailable();

	#ifdef INDI
  void			ISGetProperties();
  void			ISNewSwitch(	const char *name, ISState *states, char *names[], 
												int num);
  void 			ISNewText(	const char *name, char *texts[], char *names[], 
											int num);
  void 			ISNewNumber(const char *name, double values[], char *names[], 
												int num);
	int  			UpdateCcdFrameProperties(bool bUpdateClient = false);
	int	 			GetCcdTemperaturePoolingTime();
	int  			UpdateProperties();
	int 	 			UpdateCfwProperties();
	int  			StartExposure();
	int  			StopExposure();
	bool 			CheckConnection(ISwitchVectorProperty *sp);
	bool 			CheckConnection(INumberVectorProperty *np);
	bool 			CheckConnection(ITextVectorProperty *tp);
	int	 			GetSelectedCcdChip(int &ccd_request);
	int	 			GetSelectedCcdBinningMode(int &binning);
	int				GetSelectedCcdFrameType(string &frame_type);
	int	 			GetCcdShutterMode(int &shutter, int ccd_request);
	int				CfwConnect();
	int				CfwDisconnect();
	int 				CfwOpenDevice(CFWResults *);
	int		 		CfwCloseDevice(CFWResults *);
	int		 		CfwInit(CFWResults *);
	int		 		CfwGetInfo(CFWResults *);
	int		 		CfwQuery(CFWResults*);
	int				CfwGoto(CFWResults *);
	int				CfwGotoMonitor(CFWResults *);
	void 			CfwShowResults(string name, CFWResults);
	void			CfwUpdateProperties(CFWResults);
	void 			SetBlobState(IPState state);
	unsigned short 	*AllocateBuffer(unsigned short width, unsigned short height);
	int				ReleaseBuffer(unsigned short height, unsigned short *buffer);
	int 				ReadoutCcd(	unsigned short left, 	 unsigned short top, 
												unsigned short width, unsigned short height,
												unsigned short *buffer);
	string			CreateFitsName();
	int 				WriteFits(	string fits_name, unsigned short width, 
											unsigned short height, unsigned short *buffer);
	/* FIXME use CFITSIO
	FITS_HDU_LIST 	*CreateFitsHeader(	FITS_FILE *fp, 
																	unsigned int width, 
																	unsigned int height);*/
	void		CreateFitsHeader(fitsfile *fptr, unsigned int width, unsigned int height);
	int		UploadFits(string file_name);
	void 		UpdateTemperature();
	void 		UpdateExposure();
	static void 	UpdateTemperature(void *);
	static void 	UpdateExposure(void *);	
	inline void	SaveExposeTime(double val){m_icam_expose_time = val;}
	inline double	GetExposeTime(){return(m_icam_expose_time);}
	inline double 	GetLastExposeTime(){return(m_icam_expose_time);}
	inline void	SaveTemperature(double val){m_icam_temperature = val;}
	inline double	GetLastTemperature(){return(m_icam_temperature);}	

	#endif

	protected:
 	void			InitVars();
 	int				OpenDriver();
 	int				CloseDriver();
	unsigned short 	CalcSetpoint(double temperature);
 	double			CalcTemperature(short thermistorType, short ccdSetpoint);
 	double  		BcdPixel2double(ulong bcd);
};
//=============================================================================
#endif //_SBIG_CAM_
