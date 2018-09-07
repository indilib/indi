/*
 QHYCCD SDK
 
 Copyright (c) 2014 QHYCCD.
 All Rights Reserved.
 
 This program is free software; you can redistribute it and/or modify it
 under the terms of the GNU General Public License as published by the Free
 Software Foundation; either version 2 of the License, or (at your option)
 any later version.
 
 This program is distributed in the hope that it will be useful, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 more details.
 
 You should have received a copy of the GNU General Public License along with
 this program; if not, write to the Free Software Foundation, Inc., 59
 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 
 The full GNU General Public License is included in this distribution in the
 file called LICENSE.
 */

/*!
 * @file qhyccdstruct.h
 * @brief QHYCCD SDK struct define
 */

#ifndef WIN32
	#define LINUX
#else
	#define QHYCCD_OPENCV_SUPPORT
#endif

#ifdef WIN32
	#include <windows.h>
#else // Linux & Mac
	#include <pthread.h>
#endif

#ifndef __QHYCCDSTRUCTDEF_H__
#define __QHYCCDSTRUCTDEF_H__

#ifdef WIN32
 
#ifndef EXPORTFUNC
#define EXPORTFUNC extern "C" __declspec(dllexport)
#endif
 
#ifndef STDCALL
#define STDCALL __stdcall
#endif

#ifndef EXPORTC
#define EXPORTC extern "C"
#endif

#else

#define EXPORTFUNC extern "C"
#define STDCALL
#define EXPORTC extern "C"

#endif

#include "stdint.h"

/**
 * usb vendor request command
 */
#define QHYCCD_REQUEST_READ  (0xC0)

/**
 * usb vendor request command
 */
#define QHYCCD_REQUEST_WRITE (0x40)

#define MACHANICALSHUTTER_OPEN  0
#define MACHANICALSHUTTER_CLOSE 1
#define MACHANICALSHUTTER_FREE  2

/**
 * @brief CCDREG struct define
 *
 * List the ccd registers param
 */
typedef struct ccdreg
{
    uint8_t Gain;                //!< ccd gain
    uint8_t Offset;              //!< ccd offset
    uint32_t Exptime;             //!< expose time
    uint8_t HBIN;                //!< width bin
    uint8_t VBIN;                //!< height bin
    uint16_t LineSize;           //!< almost match image width
    uint16_t VerticalSize;       //!< almost match image height
    uint16_t SKIP_TOP;           //!< Reserved
    uint16_t SKIP_BOTTOM;        //!< Reserved
    uint16_t LiveVideo_BeginLine;//!< Reserved
    uint16_t AnitInterlace;      //!< Reserved
    uint8_t MultiFieldBIN;       //!< Reserved
    uint8_t AMPVOLTAGE;          //!< Reserved
    uint8_t DownloadSpeed;       //!< transfer speed
    uint8_t TgateMode;           //!< Reserved
    uint8_t ShortExposure;       //!< Reserved
    uint8_t VSUB;                //!< Reserved
    uint8_t CLAMP;               //!< Reserved
    uint8_t TransferBIT;         //!< Reserved
    uint8_t TopSkipNull;         //!< Reserved
    uint16_t TopSkipPix;         //!< Reserved
    uint8_t MechanicalShutterMode;//!< Reserved
    uint8_t DownloadCloseTEC;    //!< Reserved
    uint8_t SDRAM_MAXSIZE;       //!< Reserved
    uint16_t ClockADJ;           //!< Reserved
    uint8_t Trig;                //!< Reserved
    uint8_t MotorHeating;        //!< Reserved
    uint8_t WindowHeater;        //!< Reserved
    uint8_t ADCSEL;              //!< Reserved
}CCDREG;

struct BIOREG
{
    uint16_t LineSize;
    uint16_t PatchNumber;
    uint8_t  AMPVOLTAGE;
    uint8_t  ShortExposure;
    uint8_t  SDRAM_MAXSIZE;
    uint8_t  DownloadSpeed;
    uint8_t  TransferBIT;
    uint8_t  BIOCCD_Mode;
    uint8_t  BIOCCD_Video;
    uint8_t  SDRAM_Bypass;
};

/**
 * @brief CONTROL_ID enum define
 *
 * List of the function could be control
 */
enum CONTROL_ID
{
    CONTROL_BRIGHTNESS = 0, //!< image brightness
    CONTROL_CONTRAST,       //1 image contrast 
    CONTROL_WBR,            //2 red of white balance 
    CONTROL_WBB,            //3 blue of white balance
    CONTROL_WBG,            //4 the green of white balance 
    CONTROL_GAMMA,          //5 screen gamma 
    CONTROL_GAIN,           //6 camera gain 
    CONTROL_OFFSET,         //7 camera offset 
    CONTROL_EXPOSURE,       //8 expose time (us)
    CONTROL_SPEED,          //9 transfer speed 
    CONTROL_TRANSFERBIT,    //10 image depth bits 
    CONTROL_CHANNELS,       //11 image channels 
    CONTROL_USBTRAFFIC,     //12 hblank 
    CONTROL_ROWNOISERE,     //13 row denoise 
    CONTROL_CURTEMP,        //14 current cmos or ccd temprature 
    CONTROL_CURPWM,         //15 current cool pwm 
    CONTROL_MANULPWM,       //16 set the cool pwm 
    CONTROL_CFWPORT,        //17 control camera color filter wheel port 
    CONTROL_COOLER,         //18 check if camera has cooler
    CONTROL_ST4PORT,        //19 check if camera has st4port
    CAM_COLOR,              //20   
    CAM_BIN1X1MODE,         //21 check if camera has bin1x1 mode 
    CAM_BIN2X2MODE,         //22 check if camera has bin2x2 mode 
    CAM_BIN3X3MODE,         //23 check if camera has bin3x3 mode 
    CAM_BIN4X4MODE,         //24 check if camera has bin4x4 mode 
    CAM_MECHANICALSHUTTER,                  //25 mechanical shutter  
    CAM_TRIGER_INTERFACE,                   //26 triger  
    CAM_TECOVERPROTECT_INTERFACE,           //27 tec overprotect
    CAM_SINGNALCLAMP_INTERFACE,             //28 singnal clamp 
    CAM_FINETONE_INTERFACE,                 //29 fine tone 
    CAM_SHUTTERMOTORHEATING_INTERFACE,      //30 shutter motor heating 
    CAM_CALIBRATEFPN_INTERFACE,             //31 calibrated frame 
    CAM_CHIPTEMPERATURESENSOR_INTERFACE,    //32 chip temperaure sensor
    CAM_USBREADOUTSLOWEST_INTERFACE,        //33 usb readout slowest 

    CAM_8BITS,                              //34 8bit depth 
    CAM_16BITS,                             //35 16bit depth
    CAM_GPS,                                //36 check if camera has gps 

    CAM_IGNOREOVERSCAN_INTERFACE,           //37 ignore overscan area 

    QHYCCD_3A_AUTOBALANCE,					//38
    QHYCCD_3A_AUTOEXPOSURE,					//39
    QHYCCD_3A_AUTOFOCUS,					//40
    CONTROL_AMPV,                           //41 ccd or cmos ampv
    CONTROL_VCAM,                           //42 Virtual Camera on off 
	CAM_VIEW_MODE,							//43

	CONTROL_CFWSLOTSNUM,         			//44 check CFW slots number
	IS_EXPOSING_DONE,						//45
	ScreenStretchB,							//46
	ScreenStretchW,							//47
	CONTROL_DDR,							//47
	CAM_LIGHT_PERFORMANCE_MODE,				//49

	CAM_QHY5II_GUIDE_MODE,					//50
	DDR_BUFFER_CAPACITY,					//51
	DDR_BUFFER_READ_THRESHOLD,				//52

	DefaultOffset,							//53
	OutputDataActualBits,					//54
	OutputDataAlignment						//55
};

/**
 * debayer mode for mono to color */
enum BAYER_ID
{
    BAYER_GB = 1,
    BAYER_GR,
    BAYER_BG,
    BAYER_RG
};

enum CodecID
{
    NONE_CODEC,
    H261_CODEC
};

#endif
