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
 * @file qhyccd.h
 * @brief QHYCCD SDK interface for programer
 */
#include "qhyccderr.h"
#include "qhyccdcamdef.h"
#include "qhyccdstruct.h"

#ifndef __QHYCCD_H__
#define __QHYCCD_H__


/** \fn int InitQHYCCDResource()
      \brief initialize QHYCCD SDK resource
      \return 
	  on success,return QHYCCD_SUCCESS \n
	  QHYCCD_ERROR_INITRESOURCE if the initialize failed \n
	  another QHYCCD_ERROR code on other failures
  */
EXPORTC int STDCALL InitQHYCCDResource(void);

/** \fn int ReleaseQHYCCDResource()
      \brief release QHYCCD SDK resource
      \return 
	  on success,return QHYCCD_SUCCESS \n
	  QHYCCD_ERROR_RELEASERESOURCE if the release failed \n
	  another QHYCCD_ERROR code on other failures
  */
EXPORTC int STDCALL ReleaseQHYCCDResource(void);

/** \fn int ScanQHYCCD()
      \brief scan the connected cameras
	  \return
	  on success,return the number of connected cameras \n
	  QHYCCD_ERROR_NO_DEVICE,if no camera connect to computer
	  another QHYCCD_ERROR code on other failures
  */
EXPORTC int STDCALL ScanQHYCCD(void);

/** \fn int GetQHYCCDId(int index,char *id)
      \brief get the id from camera
	  \param index sequence number of the connected cameras
	  \param id the id for camera,each camera has only id
	  \return
	  on success,return QHYCCD_SUCCESS \n
	  another QHYCCD_ERROR code on other failures
  */
EXPORTC int STDCALL GetQHYCCDId(int index,char *id);

/** \fn qhyccd_handle *OpenQHYCCD(char *id)
      \brief open camera by camera id
	  \param id the id for camera,each camera has only id
      \return
	  on success,return QHYCCD_SUCCESS \n
	  another QHYCCD_ERROR code on other failures
  */
EXPORTC qhyccd_handle * STDCALL OpenQHYCCD(char *id);

/** \fn int CloseQHYCCD(qhyccd_handle *handle)
      \brief close camera by handle
	  \param handle camera handle
	  \return
	  on success,return QHYCCD_SUCCESS \n
	  another QHYCCD_ERROR code on other failures
  */
EXPORTC int STDCALL CloseQHYCCD(qhyccd_handle *handle);

/** \fn int InitQHYCCD(qhyccd_handle *handle)
      \brief initialization specified camera by camera handle
	  \param handle camera control handle
      \return
	  on success,return QHYCCD_SUCCESS \n
	  on failed,return QHYCCD_ERROR_INITCAMERA \n
	  another QHYCCD_ERROR code on other failures
  */
EXPORTC int STDCALL InitQHYCCD(qhyccd_handle *handle);

/** @fn int IsQHYCCDControlAvailable(qhyccd_handle *handle,CONTROL_ID controlId)
    @brief check the camera has the queried function or not
    @param handle camera control handle
    @param controlId function type
    @return
	  on have,return QHYCCD_SUCCESS \n
	  on do not have,return QHYCCD_ERROR_NOTSUPPORT \n
	  another QHYCCD_ERROR code on other failures
  */
EXPORTC int STDCALL IsQHYCCDControlAvailable(qhyccd_handle *handle,CONTROL_ID controlId);

/** \fn int SetQHYCCDParam(qhyccd_handle *handle,CONTROL_ID controlId,double value)
      \brief set params to camera
      \param handle camera control handle
      \param controlId function type
	  \param value value to camera
	  \return
	  on success,return QHYCCD_SUCCESS \n
	  QHYCCD_ERROR_NOTSUPPORT,if the camera do not have the function \n
	  QHYCCD_ERROR_SETPARAMS,if set params to camera failed \n
	  another QHYCCD_ERROR code on other failures
  */
EXPORTC int STDCALL SetQHYCCDParam(qhyccd_handle *handle,CONTROL_ID controlId, double value);

/** \fn double GetQHYCCDParam(qhyccd_handle *handle,CONTROL_ID controlId)
      \brief get the params value from camera
      \param handle camera control handle
      \param controlId function type
	  \return
	  on success,return the value\n
	  QHYCCD_ERROR_NOTSUPPORT,if the camera do not have the function \n
	  QHYCCD_ERROR_GETPARAMS,if get camera params'value failed \n
	  another QHYCCD_ERROR code on other failures
  */
EXPORTC double STDCALL GetQHYCCDParam(qhyccd_handle *handle,CONTROL_ID controlId);

/** \fn int GetQHYCCDParamMinMaxStep(qhyccd_handle *handle,CONTROL_ID controlId,double *min,double *max,double *step)
      \brief get the params value from camera
      \param handle camera control handle
      \param controlId function type
	  \param *min the pointer to the function's min value
	  \param *max the pointer to the function's max value
	  \param *step the pointer to the function's single step value
	  \return
	  on success,return QHYCCD_SUCCESS\n
	  QHYCCD_ERROR_NOTSUPPORT,if the camera do not have the function \n
	  another QHYCCD_ERROR code on other failures
  */
EXPORTC int STDCALL GetQHYCCDParamMinMaxStep(qhyccd_handle *handle,CONTROL_ID controlId,double *min,double *max,double *step);

/** @fn int SetQHYCCDResolution(qhyccd_handle *handle,int x,int y,int xsize,int ysize)
    @brief set camera ouput resolution
    @param handle camera control handle
    @param x the top left position x
    @param y the top left position y
    @param xsize the image width
    @param ysize the image height
    @return
        on success,return QHYCCD_SUCCESS\n
        another QHYCCD_ERROR code on other failures
  */
EXPORTC int STDCALL SetQHYCCDResolution(qhyccd_handle *handle,int x,int y,int xsize,int ysize);

/** \fn int GetQHYCCDMemLength(qhyccd_handle *handle)
      \brief get the minimum memory space for image data to save(byte)
      \param handle camera control handle
      \return 
	  on success,return the total memory space for image data(byte) \n
	  another QHYCCD_ERROR code on other failures
  */
EXPORTC int STDCALL GetQHYCCDMemLength(qhyccd_handle *handle);

/** \fn int ExpQHYCCDSingleFrame(qhyccd_handle *handle)
      \brief start to expose one frame
      \param handle camera control handle
      \return
	  on success,return QHYCCD_SUCCESS \n
	  QHYCCD_ERROR_EXPOSING,if the camera is exposing \n
	  QHYCCD_ERROR_EXPFAILED,if start failed \n
	  another QHYCCD_ERROR code on other failures
  */
EXPORTC int STDCALL ExpQHYCCDSingleFrame(qhyccd_handle *handle);

/**
   @fn int GetQHYCCDSingleFrame(qhyccd_handle *handle,int *w,int *h,int *bpp,int *channels,unsigned char *imgdata)
   @brief get live frame data from camera
   @param handle camera control handle
   @param *w pointer to width of ouput image
   @param *h pointer to height of ouput image
   @param *bpp pointer to depth of ouput image
   @param *channels pointer to channels of ouput image
   @param *imgdata image data buffer
   @return
   on success,return QHYCCD_SUCCESS \n
   QHYCCD_ERROR_GETTINGFAILED,if get data failed \n
   another QHYCCD_ERROR code on other failures
 */
EXPORTC int STDCALL GetQHYCCDSingleFrame(qhyccd_handle *handle,int *w,int *h,int *bpp,int *channels,unsigned char *imgdata);

/**
  @fn int CancelQHYCCDExposing(qhyccd_handle *handle)
  @param handle camera control handle
  @return
  on success,return QHYCCD_SUCCESS \n
  another QHYCCD_ERROR code on other failures
  */
EXPORTC int STDCALL CancelQHYCCDExposing(qhyccd_handle *handle);

/** 
  @fn int CancelQHYCCDExposingAndReadout(qhyccd_handle *handle)
  @brief stop the camera exposing and readout
  @param handle camera control handle
  @return
  on success,return QHYCCD_SUCCESS \n
  another QHYCCD_ERROR code on other failures
  */
EXPORTC int STDCALL CancelQHYCCDExposingAndReadout(qhyccd_handle *handle);

/** \fn int BeginQHYCCDLive(qhyccd_handle *handle)
      \brief start continue exposing
	  \param handle camera control handle
	  \return
	  on success,return QHYCCD_SUCCESS \n
	  another QHYCCD_ERROR code on other failures
  */
EXPORTC int STDCALL BeginQHYCCDLive(qhyccd_handle *handle);

/**   
      @fn int GetQHYCCDLiveFrame(qhyccd_handle *handle,int *w,int *h,int *bpp,int *channels,unsigned char *imgdata)
      @brief get live frame data from camera
	  @param handle camera control handle
	  @param *w pointer to width of ouput image
	  @param *h pointer to height of ouput image
      @param *bpp pointer to depth of ouput image
      @param *channels pointer to channels of ouput image
      @param *imgdata image data buffer
	  @return
	  on success,return QHYCCD_SUCCESS \n
	  QHYCCD_ERROR_GETTINGFAILED,if get data failed \n
	  another QHYCCD_ERROR code on other failures
  */
EXPORTC int STDCALL GetQHYCCDLiveFrame(qhyccd_handle *handle,int *w,int *h,int *bpp,int *channels,unsigned char *imgdata);

/** \fn int StopQHYCCDLive(qhyccd_handle *handle)
      \brief stop the camera continue exposing
	  \param handle camera control handle
	  \return
	  on success,return QHYCCD_SUCCESS \n
	  another QHYCCD_ERROR code on other failures
  */
EXPORTC int STDCALL StopQHYCCDLive(qhyccd_handle *handle);

/** \
  @fn int SetQHYCCDBinMode(qhyccd_handle *handle,int wbin,int hbin)
  @brief set camera's bin mode for ouput image data
  @param handle camera control handle
  @param wbin width bin mode
  @param hbin height bin mode
  @return
  on success,return QHYCCD_SUCCESS \n
  another QHYCCD_ERROR code on other failures
*/
EXPORTC int STDCALL SetQHYCCDBinMode(qhyccd_handle *handle,int wbin,int hbin);

/**
   @fn int SetQHYCCDBitsMode(qhyccd_handle *handle,int bits)
   @brief set camera's depth bits for ouput image data
   @param handle camera control handle
   @param bits image depth
   @return
   on success,return QHYCCD_SUCCESS \n
   another QHYCCD_ERROR code on other failures
  */
EXPORTC int STDCALL SetQHYCCDBitsMode(qhyccd_handle *handle,int bits);

/** \fn int ControlQHYCCDTemp(qhyccd_handle *handle,double targettemp)
      \brief This is a auto temprature control for QHYCCD cameras. \n
             To control this,you need call this api every single second
          \param handle camera control handle
          \param targettemp the target control temprature
          \return
          on success,return QHYCCD_SUCCESS \n
          another QHYCCD_ERROR code on other failures
  */
EXPORTC int STDCALL ControlQHYCCDTemp(qhyccd_handle *handle,double targettemp);

/** \fn int ControlQHYCCDGuide(qhyccd_handle *handle,int direction,unsigned short duration)
      \brief control the camera' guide port
	  \param handle camera control handle
	  \param direction direction \n
           0: EAST RA+   \n
           3: WEST RA-   \n
           1: NORTH DEC+ \n 
           2: SOUTH DEC- \n
	  \param duration duration of the direction
	  \return 
	  on success,return QHYCCD_SUCCESS \n
	  another QHYCCD_ERROR code on other failures
  */
EXPORTC int STDCALL ControlQHYCCDGuide(qhyccd_handle *handle,int direction,unsigned short duration);

	 /** 
	  @fn int SendOrder2QHYCCDCFW(qhyccd_handle *handle,char *order,int length)
      @brief control color filter wheel port 
      @param handle camera control handle
	  @param order order send to color filter wheel
	  @param the order string length
	  @return
	  on success,return QHYCCD_SUCCESS \n
	  another QHYCCD_ERROR code on other failures
    */
EXPORTC int STDCALL SendOrder2QHYCCDCFW(qhyccd_handle *handle,char *order,int length);
	 /** 
	  @fn 	int GetQHYCCDCFWStatus(qhyccd_handle *handle,char *status)
      @brief control color filter wheel port 
      @param handle camera control handle
	  @param status the color filter wheel position status
	  @return
	  on success,return QHYCCD_SUCCESS \n
	  another QHYCCD_ERROR code on other failures
    */
EXPORTC	int STDCALL GetQHYCCDCFWStatus(qhyccd_handle *handle,char *status);

/** \fn int SetQHYCCDTrigerMode(qhyccd_handle *handle,int trigerMode)
      \brief set camera triger mode
      \param handle camera control handle
      \param trigerMode triger mode
      \return
	  on success,return QHYCCD_SUCCESS \n
	  another QHYCCD_ERROR code on other failures
  */
EXPORTC int STDCALL SetQHYCCDTrigerMode(qhyccd_handle *handle,int trigerMode);

/** \fn void Bits16ToBits8(qhyccd_handle *h,unsigned char *InputData16,unsigned char *OutputData8,int imageX,int imageY,unsigned short B,unsigned short W)
      \brief turn 16bits data into 8bits
      \param h camera control handle
      \param InputData16 for 16bits data memory
      \param OutputData8 for 8bits data memory
      \param imageX image width
      \param imageY image height
      \param B for stretch balck
      \param W for stretch white
  */
EXPORTC void STDCALL Bits16ToBits8(qhyccd_handle *h,unsigned char *InputData16,unsigned char *OutputData8,int imageX,int imageY,unsigned short B,unsigned short W);

/**
   @fn void HistInfo192x130(qhyccd_handle *h,int x,int y,unsigned char *InBuf,unsigned char *OutBuf)
   @brief make the hist info
   @param h camera control handle
   @param x image width
   @param y image height
   @param InBuf for the raw image data
   @param OutBuf for 192x130 8bits 3 channels image
  */
EXPORTC void  STDCALL HistInfo192x130(qhyccd_handle *h,int x,int y,unsigned char *InBuf,unsigned char *OutBuf);


/** 
    @fn int OSXInitQHYCCDFirmware(char *path)
    @brief download the firmware to camera.(this api just need call in OSX system)
    @param path path to HEX file
  */
int OSXInitQHYCCDFirmware(char *path);



/** @fn int GetQHYCCDChipInfo(qhyccd_handle *h,double *chipw,double *chiph,int *imagew,int *imageh,double *pixelw,double *pixelh,int *bpp)
      @brief get the camera's ccd/cmos chip info
      @param h camera control handle
      @param chipw chip size width
      @param chiph chip size height
      @param imagew chip output image width
      @param imageh chip output image height
      @param pixelw chip pixel size width
      @param pixelh chip pixel size height
      @param bpp chip pixel depth
  */
EXPORTC int STDCALL GetQHYCCDChipInfo(qhyccd_handle *h,double *chipw,double *chiph,int *imagew,int *imageh,double *pixelw,double *pixelh,int *bpp);

/** @fn int GetQHYCCDOverScanArea(qhyccd_handle *h,int *startX, int *startY, int *sizeX, int *sizeY)
      @brief get the camera's ccd/cmos chip info
      @param h camera control handle
      @param startX the OverScan area x position
      @param startY the OverScan area y position
      @param sizeX the OverScan area x size
      @param sizeY the OverScan area y size
	  @return
	  on success,return QHYCCD_SUCCESS \n
	  another QHYCCD_ERROR code on other failures
  */
EXPORTC int STDCALL GetQHYCCDOverScanArea(qhyccd_handle *h,int *startX, int *startY, int *sizeX, int *sizeY);

/** @fn int GetQHYCCDEffectiveArea(qhyccd_handle *h,int *startX, int *startY, int *sizeX, int *sizeY)
      @brief get the camera's ccd/cmos chip info
      @param h camera control handle
      @param startX the Effective area x position
      @param startY the Effective area y position
      @param sizeX the Effective area x size
      @param sizeY the Effective area y size
	  @return
	  on success,return QHYCCD_SUCCESS \n
	  another QHYCCD_ERROR code on other failures
  */
EXPORTC int STDCALL GetQHYCCDEffectiveArea(qhyccd_handle *h,int *startX, int *startY, int *sizeX, int *sizeY);

/** @fn int SetQHYCCDFocusSetting(qhyccd_handle *h,int focusCenterX, int focusCenterY)
      @brief Set the camera on focus mode
      @param h camera control handle
      @param focusCenterX
      @param focusCenterY
      @return
	  on success,return QHYCCD_SUCCESS \n

	  another QHYCCD_ERROR code on other failures
  */
EXPORTFUNC int STDCALL SetQHYCCDFocusSetting(qhyccd_handle *h,int focusCenterX, int focusCenterY);

/** @fn unsigned long GetQHYCCDExposureRemaining(qhyccd_handle *h)
      @brief Get remaining ccd/cmos expose time
      @param h camera control handle
      @return
      100 or less 100,it means exposoure is over \n
      another is remaining time
 */
EXPORTFUNC unsigned long STDCALL GetQHYCCDExposureRemaining(qhyccd_handle *h);

/** @fn int GetQHYCCDFWVersion(qhyccd_handle *h,unsigned char *buf)
      @brief Get the QHYCCD's firmware version
      @param h camera control handle
	  @param buf buffer for version info
      @return
	  on success,return QHYCCD_SUCCESS \n

	  another QHYCCD_ERROR code on other failures
 */
EXPORTFUNC int STDCALL GetQHYCCDFWVersion(qhyccd_handle *h,unsigned char *buf);

/** @fn int SetQHYCCDInterCamSerialParam(qhyccd_handle *h,int opt)
      @brief Set InterCam serial2 params
      @param h camera control handle
	  @param opt the param \n
	  opt: \n
	   0x00 baud rate 9600bps  8N1 \n
       0x01 baud rate 4800bps  8N1 \n
       0x02 baud rate 19200bps 8N1 \n
       0x03 baud rate 28800bps 8N1 \n
       0x04 baud rate 57600bps 8N1
      @return
	  on success,return QHYCCD_SUCCESS \n

	  another QHYCCD_ERROR code on other failures
 */
EXPORTFUNC int STDCALL SetQHYCCDInterCamSerialParam(qhyccd_handle *h,int opt);

/** @fn int QHYCCDInterCamSerialTX(qhyccd_handle *h,char *buf,int length)
      @brief Send data to InterCam serial2
      @param h camera control handle
	  @param buf buffer for data
	  @param length to send
      @return
	  on success,return QHYCCD_SUCCESS \n

	  another QHYCCD_ERROR code on other failures
 */
EXPORTFUNC int STDCALL QHYCCDInterCamSerialTX(qhyccd_handle *h,char *buf,int length);

/** @fn int QHYCCDInterCamSerialRX(qhyccd_handle *h,char *buf)
      @brief Get data from InterCam serial2
      @param h camera control handle
	  @param buf buffer for data
      @return
	  on success,return the data number \n

	  another QHYCCD_ERROR code on other failures
 */
EXPORTFUNC int STDCALL QHYCCDInterCamSerialRX(qhyccd_handle *h,char *buf);

	/** @fn int QHYCCDInterCamOledOnOff(qhyccd_handle *handle,unsigned char onoff)
      @brief turn off or turn on the InterCam's Oled
      @param h camera control handle
	  @param onoff on or off the oled \n
	  1:on \n
	  0:off \n
      @return
	  on success,return QHYCCD_SUCCESS \n
	  another QHYCCD_ERROR code on other failures
    */
EXPORTFUNC int STDCALL QHYCCDInterCamOledOnOff(qhyccd_handle *handle,unsigned char onoff);

/** 
  @fn int SetQHYCCDInterCamOledBrightness(qhyccd_handle *handle,unsigned char brightness)
  @brief send data to show on InterCam's OLED
  @param h camera control handle
  @param brightness the oled's brightness
  @return
  on success,return QHYCCD_SUCCESS \n
  another QHYCCD_ERROR code on other failures
*/
EXPORTFUNC int STDCALL SetQHYCCDInterCamOledBrightness(qhyccd_handle *handle,unsigned char brightness);

/** 
  @fn int SendTwoLine2QHYCCDInterCamOled(qhyccd_handle *handle,char *messageTop,char *messageBottom)
  @brief spilit the message to two line,send to camera
  @param h camera control handle
  @param messageTop message for the oled's 1st line
  @param messageBottom message for the oled's 2nd line
  @return
  on success,return QHYCCD_SUCCESS \n
  another QHYCCD_ERROR code on other failures
*/
EXPORTFUNC int STDCALL SendTwoLine2QHYCCDInterCamOled(qhyccd_handle *handle,char *messageTop,char *messageBottom);

/** 
  @fn int SendOneLine2QHYCCDInterCamOled(qhyccd_handle *handle,char *messageTop)
  @brief spilit the message to two line,send to camera
  @param h camera control handle
  @param messageTop message for all the oled
  @return
  on success,return QHYCCD_SUCCESS \n
  another QHYCCD_ERROR code on other failures
*/  
EXPORTFUNC int STDCALL SendOneLine2QHYCCDInterCamOled(qhyccd_handle *handle,char *messageTop);

/** 
  @fn int GetQHYCCDCameraStatus(qhyccd_handle *h,unsigned char *buf)
  @brief Get the camera statu
  @param h camera control handle
  @param buf camera's status save space
  @return
  on success,return QHYCCD_SUCCESS \n
  another QHYCCD_ERROR code on other failures
 */
EXPORTFUNC int STDCALL GetQHYCCDCameraStatus(qhyccd_handle *h,unsigned char *buf);

 /** 
  @fn int GetQHYCCDShutterStatus(qhyccd_handle *handle)
  @brief get the camera's shutter status 
  @param handle camera control handle
  @return
  on success,return status \n
  0x00:shutter turn to right \n
  0x01:shutter from right turn to middle \n
  0x02:shutter from left turn to middle \n
  0x03:shutter turn to left \n
  0xff:IDLE \n
  another QHYCCD_ERROR code on other failures
*/
EXPORTFUNC int STDCALL GetQHYCCDShutterStatus(qhyccd_handle *handle);

/** 
  @fn int ControlQHYCCDShutter(qhyccd_handle *handle,unsigned char status)
  @brief control camera's shutter
  @param handle camera control handle
  @param status the shutter status \n
  0x00:shutter turn to right \n
  0x01:shutter from right turn to middle \n
  0x02:shutter from left turn to middle \n
  0x03:shutter turn to left \n
  0xff:IDLE
  @return
  on success,return QHYCCD_SUCCESS \n
  another QHYCCD_ERROR code on other failures
*/
EXPORTFUNC int STDCALL ControlQHYCCDShutter(qhyccd_handle *handle,unsigned char status);

/** 
  @fn int SetQHYCCDStreamMode(qhyccd_handle *handle,unsigned char mode)
  @brief Set the camera's mode to chose the way reading data from camera
  @param handle camera control handle
  @param mode the stream mode \n
  0x00:default mode,single frame mode \n
  0x01:live mode \n
  @return
  on success,return QHYCCD_SUCCESS \n
  another QHYCCD_ERROR code on other failures
*/
EXPORTFUNC int STDCALL SetQHYCCDStreamMode(qhyccd_handle *handle,unsigned char mode);

/** 
  @fn bool FFmpegInitAVI(char* fileName, int width, int height, int bpp, int fps, CodecID codeId)
  @brief init avi file writer
  @param fileName the filename to save,include path and .avi
  @param width the image width save to avi
  @param height the image height save to avi
  @param bpp the image bpps save to avi
  @param fps reserved
  @param codeId reserved
  @return
  on success,return QHYCCD_SUCCESS \n
  another QHYCCD_ERROR code on other failures
*/
EXPORTFUNC bool STDCALL FFmpegInitAVI(wchar_t* fileName, int width, int height, int bpp, int fps, CodecID codeId);

/** 
  @fn void STDCALL FFmpegFreeAVI(void)
  @brief avi file write done
*/
EXPORTFUNC void STDCALL FFmpegFreeAVI(void);

/** 
  @fn int SetQHYCCDStreamMode(qhyccd_handle *handle,unsigned char mode)
  @brief write a frame to avi file
  @param data the raw image buffer
  @param frameCount the image size(byte) save to avi
  @param frameIndex reserved
*/
EXPORTFUNC void STDCALL FFmpegWriteToFrame(unsigned char* data, int frameCount, int frameIndex);

/** 
  @fn int GetQHYCCDHumidity(qhyccd_handle *handle,double *hd)
  @brief query cavity's humidity 
  @param handle control handle
  @param hd the humidity value
  @return
  on success,return QHYCCD_SUCCESS \n
  another QHYCCD_ERROR code on other failures 
*/
EXPORTFUNC int STDCALL GetQHYCCDHumidity(qhyccd_handle *handle,double *hd);

/** 
  @fn int QHYCCDI2CTwoWrite(qhyccd_handle *handle,unsigned short addr,unsigned short value)
  @brief Set the value of the addr register in the camera.
  @param handle camera control handle
  @param addr the address of register
  @param value the value of the address
  @return
  on success,return QHYCCD_SUCCESS \n
  another QHYCCD_ERROR code on other failures
*/
EXPORTFUNC int STDCALL QHYCCDI2CTwoWrite(qhyccd_handle *handle,unsigned short addr,unsigned short value);
	
/** 
  @fn int QHYCCDI2CTwoRead(qhyccd_handle *handle,unsigned short addr)
  @brief Get the value of the addr register in the camera.
  @param handle camera control handle
  @param addr the address of register
  @return value of the addr register
*/
EXPORTFUNC int STDCALL QHYCCDI2CTwoRead(qhyccd_handle *handle,unsigned short addr);

/** 
  @fn double GetQHYCCDReadingProgress(qhyccd_handle *handle)
  @brief get reading data from camera progress
  @param handle camera control handle
  @return current progress
*/
EXPORTFUNC double STDCALL GetQHYCCDReadingProgress(qhyccd_handle *handle);

/**
  test pid parameters
*/
EXPORTFUNC int STDCALL TestQHYCCDPIDParas(qhyccd_handle *h, double p, double i, double d);

#endif
