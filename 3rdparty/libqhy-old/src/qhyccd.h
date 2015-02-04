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
#include <libusb-1.0/libusb.h>
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
int InitQHYCCDResource(void);

/** \fn int ReleaseQHYCCDResource()
      \brief release QHYCCD SDK resource
      \return 
	  on success,return QHYCCD_SUCCESS \n
	  QHYCCD_ERROR_RELEASERESOURCE if the release failed \n
	  another QHYCCD_ERROR code on other failures
  */
int ReleaseQHYCCDResource(void);

/** \fn int ScanQHYCCD()
      \brief scan the connected cameras
	  \return
	  on success,return the number of connected cameras \n
	  QHYCCD_ERROR_NO_DEVICE,if no camera connect to computer
	  another QHYCCD_ERROR code on other failures
  */
int ScanQHYCCD(void);

/** \fn int GetQHYCCDId(int index,char *id)
      \brief get the id from camera
	  \param index sequence number of the connected cameras
	  \param id the id for camera,each camera has only id
	  \return
	  on success,return QHYCCD_SUCCESS \n
	  another QHYCCD_ERROR code on other failures
  */
int GetQHYCCDId(int index,char *id);

/** \fn qhyccd_handle *OpenQHYCCD(char *id)
      \brief open camera by camera id
	  \param id the id for camera,each camera has only id
      \return
	  on success,return QHYCCD_SUCCESS \n
	  another QHYCCD_ERROR code on other failures
  */
qhyccd_handle *OpenQHYCCD(char *id);

/** \fn int CloseQHYCCD(qhyccd_handle *handle)
      \brief close camera by handle
	  \param handle camera handle
	  \return
	  on success,return QHYCCD_SUCCESS \n
	  another QHYCCD_ERROR code on other failures
  */
int CloseQHYCCD(qhyccd_handle *handle);

/** \fn int InitQHYCCD(qhyccd_handle *handle)
      \brief initialization specified camera by camera handle
	  \param handle camera control handle
      \return
	  on success,return QHYCCD_SUCCESS \n
	  on failed,return QHYCCD_ERROR_INITCAMERA \n
	  another QHYCCD_ERROR code on other failures
  */
int InitQHYCCD(qhyccd_handle *handle);

/** @fn int IsQHYCCDControlAvailable(qhyccd_handle *handle,CONTROL_ID controlId)
    @brief check the camera has the queried function or not
    @param handle camera control handle
    @param controlId function type
    @return
	  on have,return QHYCCD_SUCCESS \n
	  on do not have,return QHYCCD_ERROR_NOTSUPPORT \n
	  another QHYCCD_ERROR code on other failures
  */
int IsQHYCCDControlAvailable(qhyccd_handle *handle,CONTROL_ID controlId);

/** \fn unsigned char IsQHYCCDColor(qhyccd_handle *handle)
      \brief check the camera is color or not
      \param handle camera control handle
      \return
	  color return QHYCCD_COLOR \n
	  mono return QHYCCD_MONO \n
	  another QHYCCD_ERROR code on other failures
  */
int IsQHYCCDColor(qhyccd_handle *handle);

/** \fn unsigned char IsQHYCCDCool(qhyccd_handle *handle)
      \brief check the camera has cool function or not
      \param handle camera control handle
      \return
	  cool return QHYCCD_COOL \n
	  not cool return QHYCCD_NOTCOOL \n
	  another QHYCCD_ERROR code on other failures
  */
int IsQHYCCDCool(qhyccd_handle *handle);

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
int SetQHYCCDParam(qhyccd_handle *handle,CONTROL_ID controlId, double value);

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
double GetQHYCCDParam(qhyccd_handle *handle,CONTROL_ID controlId);

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
int GetQHYCCDParamMinMaxStep(qhyccd_handle *handle,CONTROL_ID controlId,double *min,double *max,double *step);

/** \fn int SetQHYCCDResolution(qhyccd_handle *handle,int width,int height)
      \brief set camera ouput resolution
      \param handle camera control handle
	  \param width resolution width
	  \param height resolution height
      \return
	  on success,return QHYCCD_SUCCESS\n
	  QHYCCD_ERROR_NOTSUPPORT,if the camera do not support this resolution \n
	  another QHYCCD_ERROR code on other failures
  */
int SetQHYCCDResolution(qhyccd_handle *handle,int width,int height);

/** \fn int GetQHYCCDMemLength(qhyccd_handle *handle)
      \brief get the minimum memory space for image data to save(byte)
      \param handle camera control handle
      \return 
	  on success,return the total memory space for image data(byte) \n
	  another QHYCCD_ERROR code on other failures
  */
int GetQHYCCDMemLength(qhyccd_handle *handle);

/** \fn int ExpQHYCCDSingleFrame(qhyccd_handle *handle)
      \brief start to expose one frame
      \param handle camera control handle
      \return
	  on success,return QHYCCD_SUCCESS \n
	  QHYCCD_ERROR_EXPOSING,if the camera is exposing \n
	  QHYCCD_ERROR_EXPFAILED,if start failed \n
	  another QHYCCD_ERROR code on other failures
  */
int ExpQHYCCDSingleFrame(qhyccd_handle *handle);

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
int GetQHYCCDSingleFrame(qhyccd_handle *handle,int *w,int *h,int *bpp,int *channels,unsigned char *imgdata);

/** \fn int StopQHYCCDExpSingle(qhyccd_handle *handle)
      \brief stop the camera exposing
	  \param handle camera control handle
	  \return
	  on success,return QHYCCD_SUCCESS \n
	  another QHYCCD_ERROR code on other failures
  */
int StopQHYCCDExpSingle(qhyccd_handle *handle);

/** \fn int BeginQHYCCDLive(qhyccd_handle *handle)
      \brief start continue exposing
	  \param handle camera control handle
	  \return
	  on success,return QHYCCD_SUCCESS \n
	  another QHYCCD_ERROR code on other failures
  */
int BeginQHYCCDLive(qhyccd_handle *handle);

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
int GetQHYCCDLiveFrame(qhyccd_handle *handle,int *w,int *h,int *bpp,int *channels,unsigned char *imgdata);

/** \fn int StopQHYCCDLive(qhyccd_handle *handle)
      \brief stop the camera continue exposing
	  \param handle camera control handle
	  \return
	  on success,return QHYCCD_SUCCESS \n
	  another QHYCCD_ERROR code on other failures
  */
int StopQHYCCDLive(qhyccd_handle *handle);

/** \fn int SetQHYCCDBinMode(qhyccd_handle *handle,int wbin,int hbin)
      \brief set camera's bin mode for ouput image data
      \param handle camera control handle
      \param wbin width bin mode
	  \param hbin height bin mode
      \return
	  on success,return QHYCCD_SUCCESS \n
	  another QHYCCD_ERROR code on other failures
  */
int SetQHYCCDBinMode(qhyccd_handle *handle,int wbin,int hbin);

/**
   @fn int SetQHYCCDBitsMode(qhyccd_handle *handle,int bits)
   @brief set camera's depth bits for ouput image data
   @param handle camera control handle
   @param bits image depth
   @return
   on success,return QHYCCD_SUCCESS \n
   another QHYCCD_ERROR code on other failures
  */
int SetQHYCCDBitsMode(qhyccd_handle *handle,int bits);

/** \fn int ControlQHYCCDTemp(qhyccd_handle *handle,double targettemp)
      \brief This is a auto temprature control for QHYCCD cameras. \n
             To control this,you need call this api every single second
          \param handle camera control handle
          \param targettemp the target control temprature
          \return
          on success,return QHYCCD_SUCCESS \n
          another QHYCCD_ERROR code on other failures
  */
int ControlQHYCCDTemp(qhyccd_handle *handle,double targettemp);

/** \fn int ControlQHYCCDGuide(qhyccd_handle *handle,int direction,int duration)
      \brief control the camera' guide port
	  \param handle camera control handle
	  \param direction direction
	  \param duration duration of the direction
	  \return 
	  on success,return QHYCCD_SUCCESS \n
	  another QHYCCD_ERROR code on other failures
  */
int ControlQHYCCDGuide(qhyccd_handle *handle,int direction,int duration);

/** \fn int ControlQHYCCDCFW(qhyccd_handle *handle,int pos)
      \brief control color filter wheel port 
      \param handle camera control handle
      \param pos the position of filter disk 
	  \return
	  on success,return QHYCCD_SUCCESS \n
	  another QHYCCD_ERROR code on other failures
  */
int ControlQHYCCDCFW(qhyccd_handle *handle,int pos);

/** \fn int SetQHYCCDTrigerMode(qhyccd_handle *handle,int trigerMode)
      \brief set camera triger mode
      \param handle camera control handle
      \param trigerMode triger mode
      \return
	  on success,return QHYCCD_SUCCESS \n
	  another QHYCCD_ERROR code on other failures
  */
int SetQHYCCDTrigerMode(qhyccd_handle *handle,int trigerMode);

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
void Bits16ToBits8(qhyccd_handle *h,unsigned char *InputData16,unsigned char *OutputData8,int imageX,int imageY,unsigned short B,unsigned short W);

/**
   @fn void HistInfo192x130(qhyccd_handle *h,int x,int y,unsigned char *InBuf,unsigned char *OutBuf)
   @brief make the hist info
   @param h camera control handle
   @param x image width
   @param y image height
   @param InBuf for the raw image data
   @param OutBuf for 192x130 8bits 3 channels image
  */
void HistInfo192x130(qhyccd_handle *h,int x,int y,unsigned char *InBuf,unsigned char *OutBuf);

/** 
    @fn int OSXInitQHYCCDFirmware()
    @brief download the firmware to camera.(this api just need call in OSX system)
 */
int OSXInitQHYCCDFirmware();


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
int GetQHYCCDChipInfo(qhyccd_handle *h,double *chipw,double *chiph,int *imagew,int *imageh,double *pixelw,double *pixelh,int *bpp);




#endif
