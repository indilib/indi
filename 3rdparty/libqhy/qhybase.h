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
 * @file qhybase.h
 * @brief QHYCCD QHYBASE class define
 */

#include "qhycam.h"
#include "qhyccdcamdef.h"
#include "qhyccderr.h"
#include "cmosdll.h"
#include "stdint.h"
#include "debugview.h"
#include "config.h"

#if (defined(__linux__ )&&!defined (__ANDROID__)) ||(defined (__APPLE__)&&defined( __MACH__)) ||(defined(__linux__ )&&defined (__ANDROID__))
#include <pthread.h>
#include <string.h>
#endif

#if defined (_WIN32)
#include "opencv2/opencv.hpp"
#endif


#ifndef __QHYBASEDEF_H__
#define __QHYBASEDEF_H__


#define Min2(a, b) ((a) < (b) ? (a) : (b))
#define Max2(a, b) ((a) > (b) ? (a) : (b))
#define LimitByte(v) ((uint8_t)Min2(Max2(v, 0), 0xFF))
#define LimitShort(v) ((uint16_t)Min2(Max2(v, 0), 0xFFFF))

/**
 * the QHYBASE class description
 */
class QHYBASE : public QHYCAM
{
public:

  QHYBASE()
  {
    streammode = 0;
    currentPWM = 0;
    flag_timer_2 = true;
    flag_timer = true;
    camx = 0;
    camy = 0;
    camxbin = 1;
    camybin = 1;
    cambits = 16;
    camchannels = 1;
    usbtraffic = 30;
    usbspeed = 0;
    camtime = 0;
    camgain = 0;
    camoffset = 130;
    camred2green = 0;
    camblue2green = 0;
    camgreen = 0;
    rawarray = NULL;
    roiarray = NULL;
    roixstart = 0;
    roiystart = 0;
    roixsize = 0;
    roiysize = 0;
    overScanStartX = 0;
    overScanStartY = 0;
    overScanSizeX = 0;
    overScanSizeY = 0;
    onlyStartX = 0;
    onlyStartY = 0;
    onlySizeX = 0;
    onlySizeY = 0;
    ccdchipw = 0;
    ccdchiph = 0;
    ccdimagew = 0;
    ccdimageh = 0;
    ccdpixelw = 0;
    ccdpixelh = 0;

    targetTEMP = 0;
    currentTEMP = 0;
    currentPWM = 0;
    nowVoltage = 0;
    flagtempauto = false;

    NowError = 0;
    PrevError = 0;
    LastError = 0;
    Proportion = 0;
    Integral = 0;
    Derivative = 0;
    flagquit = false;
    readprogress = 0;
    streammode = 0;
    isReadoutData = 0;
    isReadoutTemp = 0;
    isOverscanRemoved = false;

    frameflag = 0;

    debayeronoff = false;
    debayerformat = 0;

    chipoutputbits = 8;

    initdone = 0;
    connected = 0;
    resolutionmode = 0;

    isbadframe = 0;
    badframenum = 0;
    isexposureupdate = 0;
    isgainupdate = 0;
    iscolorgainupdate = 0;
    isoffsetupdate = 0;
    isdepthupdate = 0;
    isspeedupdate = 0;
    isresolutionupdate = 0;
    isusbtrafficupdate = 0;
    is_superspeed = 0;

    gpson = 0;

    is_3a_autoexposure_on = 0;
    is_3a_autowhitebalance_on = 0;
    is_3a_autofocus_on = 0;
    autowhitebalanceloops = 0;

    imgbrightness = 0.0;
    imgcontrast = 0.0;
    imggamma = 1.0;

    camampv = 0.0;

    camddr = 0;

    camLPMode = 0;

    camviewmode = 0.0;

    vcamonoff = false;

    filterpos = '0';

    isFX3 = false;

    islive = 0;

    screenstretchb = 0;
    screenstretchw = 65535;
    darkgen_on=0;
    qhy5iiGuidePortOnOff = 0;
    delRowRoise = false;

    memset(&ccdreg, 0, sizeof (ccdreg));

    //testparam = 0;
    //campartnum = DEVICETYPE_UNKNOW;
  }

  virtual ~QHYBASE()
  {}
  ;

  virtual void InitCmos(qhyccd_handle *h)
  {
    OutputDebugPrintf(QHYCCD_MSGL_INFO,"QHYCCD|QHYBASE.H|InitCmos|Not implemented");
  };
  /**
   @fn virtual uint32_t ConnectCamera(qhyccd_device *d,qhyccd_handle **h)
   @brief connect camera,get the control handle
   @param d deivce
   @param h camera control handle pointer
   @return
   success return QHYCCD_SUCCESS \n
   another QHYCCD_ERROR code on other failures
   */
  virtual uint32_t ConnectCamera(qhyccd_device *d, qhyccd_handle **h);

  /**
   @fn virtual uint32_t DisConnectCamera(qhyccd_handle *h)
   @brief disconnect camera
   @param h camera control handle
   @return
   success return QHYCCD_SUCCESS \n
   another QHYCCD_ERROR code on other failures
   */
  virtual uint32_t DisConnectCamera(qhyccd_handle *h);

  /**
   @fn virtual uint32_t InitChipRegs(qhyccd_handle *h)
   @brief Init the registers and some other things
   @param h camera control handle
   @return
   success return QHYCCD_SUCCESS \n
   another QHYCCD_ERROR code on other failures
   */
  virtual uint32_t InitChipRegs(qhyccd_handle *h);

  /**
   @fn virtual uint32_t ReSetParams2cam(qhyccd_handle *h)
   @brief re set the params to camera,because some behavior will cause camera reset
   @param h camera control handle
   @return
   success return QHYCCD_SUCCESS \n
   another QHYCCD_ERROR code on other failures
   */
  virtual uint32_t ReSetParams2cam(qhyccd_handle *h);

  /**
   @fn virtual uint32_t SetChipOffset(qhyccd_handle *h,double offset)
   @brief set the camera offset
   @param h camera control handle
   @param offset offset value
   @return
   success return QHYCCD_SUCCESS \n
   another QHYCCD_ERROR code on other failures
   */
  virtual uint32_t SetChipOffset(qhyccd_handle *h, double offset)
  {
    OutputDebugPrintf(QHYCCD_MSGL_INFO,"QHYCCD|QHYBASE.H|SetChipOffset|Not implemented");
    return QHYCCD_ERROR;
  }

  /**
   @fn virtual uint32_t SetChipExposeTime(qhyccd_handle *h,double i)
   @brief set the expose time to camera
   @param h camera control handle
   @param i expose time value
   @return
   success return QHYCCD_SUCCESS \n
   another QHYCCD_ERROR code on other failures
   */
  virtual uint32_t SetChipExposeTime(qhyccd_handle *h, double i)
  {
    OutputDebugPrintf(QHYCCD_MSGL_INFO,"QHYCCD|QHYBASE.H|SetChipExposeTime|Not implemented");
    return QHYCCD_ERROR;
  }

  /**
   @fn virtual uint32_t SetChipGain(qhyccd_handle *h,double gain)
   @brief set the gain to camera
   @param h camera control handle
   @param gain gain value
   @return
   success return QHYCCD_SUCCESS \n
   another QHYCCD_ERROR code on other failures
   */
  virtual uint32_t SetChipGain(qhyccd_handle *h, double gain)
  {
    OutputDebugPrintf(QHYCCD_MSGL_INFO,"QHYCCD|QHYBASE.H|SetChipGain|Not implemented");
    return QHYCCD_ERROR;
  }

  /**
   @fn virtual uint32_t SetChipWBRed(qhyccd_handle *h,double red)
   @brief set the red gain value of white balance
   @param h camera control handle
   @param red red gain value
   @return
   success return QHYCCD_SUCCESS \n
   another QHYCCD_ERROR code on other failures
   */
  virtual uint32_t SetChipWBRed(qhyccd_handle *h, double red)
  {
    OutputDebugPrintf(QHYCCD_MSGL_INFO,"QHYCCD|QHYBASE.H|SetChipWBRed|Not implemented");
    return QHYCCD_ERROR;
  }

  /**
   @fn virtual uint32_t SetChipWBGreen(qhyccd_handle *h,double green)
   @brief set the red gain value of white balance
   @param h camera control handle
   @param green green gain value
   @return
   success return QHYCCD_SUCCESS \n
   another QHYCCD_ERROR code on other failures
   */
  virtual uint32_t SetChipWBGreen(qhyccd_handle *h, double green)
  {
    OutputDebugPrintf(QHYCCD_MSGL_INFO,"QHYCCD|QHYBASE.H|SetChipWBGreen|Not implemented");
    return QHYCCD_ERROR;
  }

  /**
   @fn virtual uint32_t SetChipWBBlue(qhyccd_handle *h,double blue)
   @brief set the red gain value of white balance
   @param h camera control handle
   @param blue blue gain value
   @return
   success return QHYCCD_SUCCESS \n
   another QHYCCD_ERROR code on other failures
   */
  virtual uint32_t SetChipWBBlue(qhyccd_handle *h, double blue)
  {
    OutputDebugPrintf(QHYCCD_MSGL_INFO,"QHYCCD|QHYBASE.H|SetChipWBBlue|Not implemented");
    return QHYCCD_ERROR;
  }

  /**
   @fn virtual double GetChipWBRed()
   @brief get the red gain value of white balance
   @return
   success return red gain value \n
   another QHYCCD_ERROR code on other failures
   */
  virtual double GetChipWBRed()
  {
    return camred2green;
  }

  /**
   @fn double GetChipWBBlue()
   @brief get the blue gain value of white balance
   @return
   success return blue gain value \n
   another QHYCCD_ERROR code on other failures
   */
  virtual double GetChipWBBlue()
  {
    return camblue2green;
  }

  /**
   @fn double GetChipWBGreen()
   @brief get the green gain value of white balance
   @return
   success return green gain value \n
   another QHYCCD_ERROR code on other failures
   */
  virtual double GetChipWBGreen()
  {
    return camgreen;
  }

  /**
   @fn virtual double GetChipExposeTime()
   @brief get the current exposetime
   @return
   success return the current expose time (unit:us) \n
   another QHYCCD_ERROR code on other failures
   */
  virtual double GetChipExposeTime();

  /**
   @fn virtual double GetChipGain()
   @brief get the current gain
   @return
   success return the current expose gain\n
   another QHYCCD_ERROR code on other failures
   */
  virtual double GetChipGain();

  /**
   @fn virtual double GetChipOffset()
   @brief get the current offset
   @return
   success return the current camera offset \n
   another QHYCCD_ERROR code on other failures
   */
  virtual double GetChipOffset();

  /**
   @fn virtual double GetChipSpeed()
   @brief get the current transfer speed
   @return
   success return the current speed level \n
   another QHYCCD_ERROR code on other failures
   */
  virtual double GetChipSpeed();

  /**
   @fn virtual double GetChipUSBTraffic()
   @brief get the hblank value
   @return
   success return the hblank value \n
   another QHYCCD_ERROR code on other failures
   */
  virtual double GetChipUSBTraffic();

  /**
   @fn virtual double GetChipBitsMode()
   @brief get the current camera depth bits
   @return
   success return the current camera depth bits \n
   another QHYCCD_ERROR code on other failures
   */
  virtual double GetChipBitsMode();

  /**
   @fn virtual double GetChipChannels()
   @brief get the current camera image channels
   @return
   success return the current camera image channels \n
   another QHYCCD_ERROR code on other failures
   */
  virtual double GetChipChannels()
  {
    return 1;
  }

  /**
   @fn virtual double GetChipCoolTemp()
   @brief get the current ccd/cmos chip temprature
   @return
   success return the current chip temprature \n
   another QHYCCD_ERROR code on other failures
   */
  virtual double GetChipCoolTemp(qhyccd_handle *h)
  {
    return currentTEMP;
  }

  /**
   @fn virtual double GetChipCoolPWM()
   @brief get the current ccd/cmos temprature
   @return
   success return the current cool temprature \n
   another QHYCCD_ERROR code on other failures
   */
  virtual double GetChipCoolPWM()
  {
    return currentPWM;
  }

  /**
   @fn virtual double GetChipCoolTarpgetTemp()
   @brief get the current ccd/cmos target temperature
   @return
   success return the current cool target temprature \n
   another QHYCCD_ERROR code on other failures
   */
  virtual double GetChipCoolTarpgetTemp()
  {
    return targetTEMP;
  }

  /**
   @fn uint32_t GetControlMinMaxStepValue(CONTROL_ID controlId,double *min,double *max,double *step)
   @brief get the min,max and step value for function
   @param controlId the control id
   @param min the min value for function
   @param max the max value for function
   @param step single step value for function
   @return
   success return QHYCCD_SUCCESS \n
   another QHYCCD_ERROR code on other failures
   */
  virtual uint32_t GetControlMinMaxStepValue(CONTROL_ID controlId, double *min, double *max, double *step)
  {
    OutputDebugPrintf(QHYCCD_MSGL_INFO,"QHYCCD|QHYBASE.H|GetControlMinMaxStepValue|Not implemented");
    return QHYCCD_ERROR;
  }

  /**
   @fn virtual uint32_t CorrectWH(uint32_t *w,uint32_t *h)
   @brief correct width and height if the setting width or height is not correct
   @param w set width
   @param h set height
   @return
   success return QHYCCD_SUCCESS \n
   another QHYCCD_ERROR code on other failures
   */
  virtual uint32_t CorrectWH(uint32_t *w, uint32_t *h)
  {
    OutputDebugPrintf(QHYCCD_MSGL_INFO,"QHYCCD|QHYBASE.H|CorrectWH|Not implemented");
    return QHYCCD_ERROR;
  }

  /**
   @fn virtual uint32_t SetChipResolution(qhyccd_handle *handle,uint32_t x,uint32_t y,uint32_t xsize,uint32_t ysize)
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
  virtual uint32_t SetChipResolution(qhyccd_handle *handle, uint32_t x, uint32_t y, uint32_t xsize, uint32_t ysize)
  {
    OutputDebugPrintf(QHYCCD_MSGL_INFO,"QHYCCD|QHYBASE.H|SetChipResolution|Not implemented");
    return QHYCCD_ERROR;
  }

  /**
   @fn virtual uint32_t BeginSingleExposure(qhyccd_handle *h)
   @brief begin single exposure
   @param h camera control handle
   @return
   success return QHYCCD_SUCCESS \n
   another QHYCCD_ERROR code on other failures
   */
  virtual uint32_t BeginSingleExposure(qhyccd_handle *h);

  /**
   @fn virtual uint32_t CancelExposing(qhyccd_handle *handle)
   @param handle camera control handle
   @return
   on success,return QHYCCD_SUCCESS \n
   another QHYCCD_ERROR code on other failures
   */
  virtual uint32_t CancelExposing(qhyccd_handle *handle);

  /**
   @fn virtual uint32_t CancelExposingAndReadout(qhyccd_handle *h)
   @brief stop single exposure
   @param h camera control handle
   @return
   success return QHYCCD_SUCCESS \n
   another QHYCCD_ERROR code on other failures
   */
  virtual uint32_t CancelExposingAndReadout(qhyccd_handle *h);

#if 0

  /**
   @fn virtual uint32_t GetSingleFrame(qhyccd_handle *h,uint32_t *pW,uint32_t *pH,uint32_t * pBpp,uint32_t *pChannels,uint8_t *ImgData)
   @brief get single frame image data
   @param h camera control handle
   @param pW real width
   @param pH real height
   @param pBpp real depth bits
   @param pChannels real channels
   @param ImgData image data buffer
   @return
   success return QHYCCD_SUCCESS \n
   another QHYCCD_ERROR code on other failures
   */
  virtual uint32_t GetSingleFrame(qhyccd_handle *h, uint32_t *pW, uint32_t *pH, uint32_t * pBpp, uint32_t *pChannels, uint8_t *ImgData)
  {
    OutputDebugPrintf(QHYCCD_MSGL_INFO,"QHYCCD|QHYBASE.H|GetSingleFrame|Not implemented");
    return QHYCCD_ERROR;
  }
#endif

  /**
   @fn virtual uint32_t BeginLiveExposure(qhyccd_handle *h)
   @brief begin live exposure
   @param h camera control handle
   @return
   success return QHYCCD_SUCCESS \n
   another QHYCCD_ERROR code on other failures
   */
  virtual uint32_t BeginLiveExposure(qhyccd_handle *h);

  /**
   @fn virtual uint32_t StopLiveExposure(qhyccd_handle *h)
   @brief stop live exposure
   @param h camera control handle
   @return
   success return QHYCCD_SUCCESS \n
   another QHYCCD_ERROR code on other failures
   */
  virtual uint32_t StopLiveExposure(qhyccd_handle *h);

  /**
   @fn virtual uint32_t SetChipUSBTraffic(qhyccd_handle *h,uint32_t i)
   @brief set hblank
   @param h camera control handle
   @param i hblank value
   @return
   success return QHYCCD_SUCCESS \n
   another QHYCCD_ERROR code on other failures
   */
  virtual uint32_t SetChipUSBTraffic(qhyccd_handle *h, uint32_t i)
  {
    OutputDebugPrintf(QHYCCD_MSGL_INFO,"QHYCCD|QHYBASE.H|SetChipUSBTraffic|Not implemented");
    return QHYCCD_ERROR;
  }

  /**
   @fn virtual uint32_t DeChipRowNoise(qhyccd_handle *h,uint32_t value)
   @brief enable the function to reduce the row noise
   @param h camera control handle
   @param value enable or disable
   @return
   success return QHYCCD_SUCCESS \n
   another QHYCCD_ERROR code on other failures
   */
  virtual uint32_t DeChipRowNoise(qhyccd_handle *h, uint32_t value)
  {
    OutputDebugPrintf(QHYCCD_MSGL_INFO,"QHYCCD|QHYBASE.H|DeChipRowNoise|Not implemented");
    return QHYCCD_ERROR;
  }

  /**
   @fn virtual uint32_t GetChipMemoryLength()
   @brief get the image cost memory length
   @return
   success return memory length \n
   another QHYCCD_ERROR code on other failures
   */
  virtual uint32_t GetChipMemoryLength();

  /**
   @fn virtual bool IsSupportHighSpeed()
   @brief check the camera support high speed transfer or not
   @return
   support return true \n
   not support return false
   */
  virtual bool IsSupportHighSpeed()
  {
    OutputDebugPrintf(QHYCCD_MSGL_INFO,"QHYCCD|QHYBASE.H|IsSupportHighSpeed|Not implemented");
    return false;
  }

  /**
   @fn virtual uint32_t IsChipHasFunction(CONTROL_ID id)
   @brief check the camera has the function or not
   @param id function id
   @return
   HAVE return QHYCCD_SUCCESS \n
   NOT HAVE return QHYCCD_ERROR_NOTSUPPORT
   */
  virtual uint32_t IsChipHasFunction(CONTROL_ID id)
  {
    OutputDebugPrintf(QHYCCD_MSGL_INFO,"QHYCCD|QHYBASE.H|IsChipHasFunction|Not implemented");
    return QHYCCD_ERROR;
  }

  /**
   @fn virtual uint32_t SetChipCoolPWM(qhyccd_handle *h,double PWM)
   @brief set cool power
   @param h camera control handle
   @param PWM power(0-255)
   @return
   success return QHYCCD_SUCCESS \n
   another QHYCCD_ERROR code on other failures
   */
  virtual uint32_t SetChipCoolPWM(qhyccd_handle *h, double PWM)
  {
    OutputDebugPrintf(QHYCCD_MSGL_INFO,"QHYCCD|QHYBASE.H|SetChipCoolPWM|Not implemented");
    return QHYCCD_ERROR;
  }

  /**
   @fn virtual uint32_t AutoTempControl(qhyccd_handle *h,double ttemp)
   @brief auto temprature control
   @param h camera control handle
   @param ttemp target temprature(degree Celsius)
   @return
   success return QHYCCD_SUCCESS \n
   another QHYCCD_ERROR code on other failures
   */
  virtual uint32_t AutoTempControl(qhyccd_handle *h, double ttemp)
  {
    OutputDebugPrintf(QHYCCD_MSGL_INFO,"QHYCCD|QHYBASE.H|AutoTempControl|Not implemented");
    return QHYCCD_ERROR;
  }

  /**
   @fn virtual uint32_t SetChipSpeed(qhyccd_handle *h,uint32_t i)
   @brief set the transfer speed to camera
   @param h camera control handle
   @param i speed level
   @return
   success return QHYCCD_SUCCESS \n
   another QHYCCD_ERROR code on other failures
   */
  virtual uint32_t SetChipSpeed(qhyccd_handle *h, uint32_t i)
  {
    OutputDebugPrintf(QHYCCD_MSGL_INFO,"QHYCCD|QHYBASE.H|SetChipSpeed|Not implemented");
    return QHYCCD_ERROR;
  }

  /**
   @fn virtual uint32_t SetChipBitsMode(qhyccd_handle *h,uint32_t bits)
   @brief set the camera depth bits
   @param h camera control handle
   @param bits depth bits
   @return
   success return QHYCCD_SUCCESS \n
   another QHYCCD_ERROR code on other failures
   */
  virtual uint32_t SetChipBitsMode(qhyccd_handle *h, uint32_t bits)
  {
    OutputDebugPrintf(QHYCCD_MSGL_INFO,"QHYCCD|QHYBASE.H|SetChipBitsMode|Not implemented");
    return QHYCCD_ERROR;
  }

  /**
   @fn virtual uint32_t SetChipChannels(qhyccd_handle *h,uint32_t channels)
   @brief set the image channels,it means the image is color one
   @param h camera control handle
   @param channels image channels
   @return
   success return QHYCCD_SUCCESS \n
   another QHYCCD_ERROR code on other failures
   */
  virtual uint32_t SetChipChannels(qhyccd_handle *h, uint32_t channels)
  {
    OutputDebugPrintf(QHYCCD_MSGL_INFO,"QHYCCD|QHYBASE.H|SetChipChannels|Not implemented");
    return QHYCCD_SUCCESS;
  }

  /**
   @fn virtual uint32_t SetChipBinMode(qhyccd_handle *h,uint32_t wbin,uint32_t hbin)
   @brief set the camera image bin mode
   @param h camera control handle
   @param wbin width bin
   @param hbin height bin
   @return
   success return QHYCCD_SUCCESS \n
   another QHYCCD_ERROR code on other failures
   */
  virtual uint32_t SetChipBinMode(qhyccd_handle *h, uint32_t wbin, uint32_t hbin);

  /**
   @fn virtual void ConvertDataBIN11(uint8_t * Data,uint32_t x, uint32_t y, uint16_t PixShift)
   @brief move the pixel raw data to correction position,and bin if need
   @param Data raw image data
   @param x image width
   @param y image height
   @param PixShift this is a way to fix the bad pixel data by the usb transfer
   */
  virtual void ConvertDataBIN11(uint8_t *Data, uint32_t x, uint32_t y, uint16_t PixShift)
  {
    OutputDebugPrintf(QHYCCD_MSGL_INFO,"QHYCCD|QHYBASE.H|ConvertDataBIN11|Not implemented");
  }

  /**
   @fn virtual void ConvertDataBIN22(uint8_t * Data,uint32_t x, uint32_t y, uint16_t PixShift)
   @brief move the pixel raw data to correction position,and bin if need
   @param Data raw image data
   @param x image width
   @param y image height
   @param PixShift this is a way to fix the bad pixel data by the usb transfer
   */
  virtual void ConvertDataBIN22(uint8_t *Data, uint32_t x, uint32_t y, uint16_t TopSkipPix)
  {
    OutputDebugPrintf(QHYCCD_MSGL_INFO,"QHYCCD|QHYBASE.H|ConvertDataBIN22|Not implemented");
  }

  /**
   @fn void ConvertDataBIN33(uint8_t * Data,uint32_t x, uint32_t y, uint16_t PixShift)
   @brief move the pixel raw data to correction position,and bin if need
   @param Data raw image data
   @param x image width
   @param y image height
   @param PixShift this is a way to fix the bad pixel data by the usb transfer
   */
  virtual void ConvertDataBIN33(uint8_t *Data, uint32_t x, uint32_t y, uint16_t TopSkipPix)
  {
    OutputDebugPrintf(QHYCCD_MSGL_INFO,"QHYCCD|QHYBASE.H|ConvertDataBIN33|Not implemented");
  }

  /**
   @fn virtual void ConvertDataBIN44(uint8_t * Data,uint32_t x, uint32_t y, uint16_t PixShift)
   @brief move the pixel raw data to correction position,and bin if need
   @param Data raw image data
   @param x image width
   @param y image height
   @param PixShift this is a way to fix the bad pixel data by the usb transfer
   */
  virtual void ConvertDataBIN44(uint8_t *Data, uint32_t x, uint32_t y, uint16_t TopSkipPix)
  {
    OutputDebugPrintf(QHYCCD_MSGL_INFO,"QHYCCD|QHYBASE.H|ConvertDataBIN44|Not implemented");
  }

  /**
   @fn virtual uint32_t Send2GuiderPort(qhyccd_handle *h,uint32_t Direction,uint16_t PulseTime)
   @brief send the command to camera's guide port
   @param h camera control handle
   @param Direction RA DEC
   @param PulseTime the time last for command
   @return
   success return QHYCCD_SUCCESS \n
   another QHYCCD_ERROR code on other failures
   */
  virtual uint32_t Send2GuiderPort(qhyccd_handle *h, uint32_t Direction, uint16_t PulseTime)
  {
    OutputDebugPrintf(QHYCCD_MSGL_INFO,"QHYCCD|QHYBASE.H|Send2GuiderPort|Not implemented");
    return QHYCCD_ERROR;
  }

  /** @fn virtual uint32_t SetFocusSetting(qhyccd_handle *h,uint32_t focusCenterX, uint32_t focusCenterY)
    @brief Set the camera on focus mode
    @param h camera control handle
    @param focusCenterX
    @param focusCenterY
    @return
    on success,return QHYCCD_SUCCESS \n
    another QHYCCD_ERROR code on other failures
   */
  virtual uint32_t SetFocusSetting(qhyccd_handle *h, uint32_t focusCenterX, uint32_t focusCenterY)
  {
    OutputDebugPrintf(QHYCCD_MSGL_INFO,"QHYCCD|QHYBASE.H|SetFocusSetting|Not implemented");
    return QHYCCD_ERROR;
  }

  /** @fn virtual uint32_t ExposureRemaining(qhyccd_handle *h)
    @brief Get remaining ccd/cmos expose time
    @param h camera control handle
    @return
    100 or less 100,it means exposoure is over \n
    another is remaining time
   */
  virtual uint32_t ExposureRemaining(qhyccd_handle *h)
  {
    OutputDebugPrintf(QHYCCD_MSGL_INFO,"QHYCCD|QHYBASE.H|ExposureRemaining|Not implemented");
    return 100;
  }

  /**
   @fn uint32_t SetStreamMode(qhyccd_handle *handle,uint8_t mode)
   @brief Set the camera's mode to chose the way reading data from camera
   @param handle camera control handle
   @param mode the stream mode \n
   0x00:default mode,single frame mode \n
   0x01:live mode \n
   @return
   on success,return QHYCCD_SUCCESS \n
   another QHYCCD_ERROR code on other failures
   */
  virtual uint32_t SetStreamMode(qhyccd_handle *handle, uint8_t mode);

  /** @fn virtual uint32_t SetInterCamSerialParam(qhyccd_handle *h,uint32_t opt)
    @brief Set InterCam serial2 params
    @param h camera control handle
    @param opt the param
    @return
    on success,return QHYCCD_SUCCESS \n

    another QHYCCD_ERROR code on other failures
   */
  virtual uint32_t SetInterCamSerialParam(qhyccd_handle *h, uint32_t opt)
  {
    OutputDebugPrintf(QHYCCD_MSGL_INFO,"QHYCCD|QHYBASE.H|SetInterCamSerialParam|Not implemented");
    return QHYCCD_ERROR;
  }

  /** @fn virtual uint32_t InterCamSerialTX(qhyccd_handle *h,char *buf,uint32_t length)
      @brief Send data to InterCam serial2
      @param h camera control handle
      @param buf buffer for data
      @param length the length to send
      @return
      on success,return QHYCCD_SUCCESS \n

      another QHYCCD_ERROR code on other failures
   */
  virtual uint32_t InterCamSerialTX(qhyccd_handle *h, char *buf, uint32_t length)
  {
    OutputDebugPrintf(QHYCCD_MSGL_INFO,"QHYCCD|QHYBASE.H|InterCamSerialTX|Not implemented");
    return QHYCCD_ERROR;
  }

  /** @fn virtual uint32_t InterCamSerialRX(qhyccd_handle *h,char *buf)
      @brief Get data from InterCam serial2
      @param h camera control handle
      @param buf buffer for data
      @return
      on success,return the data number \n

      another QHYCCD_ERROR code on other failures
   */
  virtual uint32_t InterCamSerialRX(qhyccd_handle *h, char *buf)
  {
    OutputDebugPrintf(QHYCCD_MSGL_INFO,"QHYCCD|QHYBASE.H|InterCamSerialRX|Not implemented");
    return QHYCCD_ERROR;
  }

  /** @fn virtual uint32_t Send2OledFast(qhyccd_handle *h,char *buffer)
     @brief send data to show on InterCam's OLED
     @param h camera control handle
     @param buffer buffer for data
     @return
     on success,return QHYCCD_SUCCESS \n
     another QHYCCD_ERROR code on other failures
   */
  virtual uint32_t Send2OledFast(qhyccd_handle *h, uint8_t *buffer)
  {
    OutputDebugPrintf(QHYCCD_MSGL_INFO,"QHYCCD|QHYBASE.H|Send2OledFast|Not implemented");
    return QHYCCD_ERROR;
  }

  /** @fn virtual uint32_t InterCamOledOnOff(qhyccd_handle *handle,uint8_t onoff)
    @brief turn off or turn on the InterCam's Oled
    @param h camera control handle
    @param onoff on or off the oled \n
    1:on \n
    0:off \n
    @return
    on success,return QHYCCD_SUCCESS \n
    another QHYCCD_ERROR code on other failures
   */
  virtual uint32_t InterCamOledOnOff(qhyccd_handle *handle, uint8_t onoff)
  {
    OutputDebugPrintf(QHYCCD_MSGL_INFO,"QHYCCD|QHYBASE.H|InterCamOledOnOff|Not implemented");
    return QHYCCD_ERROR;
  }

  /** @fn virtual uint32_t SetInterCamOledBrightness(qhyccd_handle *handle,uint8_t brightness)
    @brief send data to show on InterCam's OLED
    @param h camera control handle
    @param brightness the oled's brightness
    @return
    on success,return QHYCCD_SUCCESS \n
    another QHYCCD_ERROR code on other failures
   */
  virtual uint32_t SetInterCamOledBrightness(qhyccd_handle *handle, uint8_t brightness)
  {
    OutputDebugPrintf(QHYCCD_MSGL_INFO,"QHYCCD|QHYBASE.H|SetInterCamOledBrightness|Not implemented");
    return QHYCCD_ERROR;
  }

  virtual uint32_t SendFourLine2InterCamOled(qhyccd_handle *handle, char *messagetemp, char *messageinfo, char *messagetime, char *messagemode)
  {
    OutputDebugPrintf(QHYCCD_MSGL_INFO,"QHYCCD|QHYBASE.H|SendFourLine2InterCamOled|Not implemented");
    return QHYCCD_ERROR;
  }

  /** @fn virtual uint32_t SendTwoLine2InterCamOled(qhyccd_handle *handle,char *messageTop,char *messageBottom)
    @brief spilit the message to two line,send to camera
    @param h camera control handle
    @param messageTop message for the oled's 1st line
    @param messageBottom message for the oled's 2nd line
    @return
    on success,return QHYCCD_SUCCESS \n
    another QHYCCD_ERROR code on other failures
   */
  virtual uint32_t SendTwoLine2InterCamOled(qhyccd_handle *handle, char *messageTop, char *messageBottom)
  {
    OutputDebugPrintf(QHYCCD_MSGL_INFO,"QHYCCD|QHYBASE.H|SendTwoLine2InterCamOled|Not implemented");
    return QHYCCD_ERROR;
  }

  /** @fn virtual uint32_t SendOneLine2InterCamOled(qhyccd_handle *handle,char *messageTop)
    @brief spilit the message to two line,send to camera
    @param h camera control handle
    @param messageTop message for all the oled
    @return
    on success,return QHYCCD_SUCCESS \n
    another QHYCCD_ERROR code on other failures
   */
  virtual uint32_t SendOneLine2InterCamOled(qhyccd_handle *handle, char *messageTop)
  {
    OutputDebugPrintf(QHYCCD_MSGL_INFO,"QHYCCD|QHYBASE.H|SendOneLine2InterCamOled|Not implemented");
    return QHYCCD_ERROR;
  }

  /** @fn virtual uint32_t GetCameraStatus(qhyccd_handle *h,uint8_t *buf)
      @brief Get camera status
      @param h camera control handle
      @param buf camera's status save space
      @return
      on success,return the camera statu \n
      another QHYCCD_ERROR code on other failures
   */
  virtual uint32_t GetCameraStatus(qhyccd_handle *h, uint8_t *buf)
  {
    OutputDebugPrintf(QHYCCD_MSGL_INFO,"QHYCCD|QHYBASE.H|GetCameraStatus|Not implemented");
    return QHYCCD_ERROR;
  }

  /**
   @fn virtual uint32_t SendOrder2CFW(qhyccd_handle *handle,char *order,uint32_t length)
   @brief control color filter wheel
   @param handle camera control handle
   @param order order send to color filter wheel
   @param length the order string length
   @return
   on success,return QHYCCD_SUCCESS \n
   another QHYCCD_ERROR code on other failures
   */
  virtual uint32_t SendOrder2CFW(qhyccd_handle *handle, char *order, uint32_t length)
  {
    OutputDebugPrintf(QHYCCD_MSGL_INFO,"QHYCCD|QHYBASE.H|SendOrder2CFW|Not implemented");
    return QHYCCD_ERROR;
  }

  /**
   @fn virtual uint32_t GetCFWStatus(qhyccd_handle *handle,char *status)
   @brief get the color filter wheel status
   @param handle camera control handle
   @param status the color filter wheel position status
   @return
   on success,return QHYCCD_SUCCESS \n
   another QHYCCD_ERROR code on other failures
   */
  virtual uint32_t GetCFWStatus(qhyccd_handle *handle, char *status)
  {
    OutputDebugPrintf(QHYCCD_MSGL_INFO,"QHYCCD|QHYBASE.H|GetCFWStatus|Not implemented");
    return QHYCCD_ERROR;
  }

  /**
  @fn virtual uint32_t GetCFWSlotsNum()
  @brief get the hole number of color filter wheel
  @return
  on success,return QHYCCD_SUCCESS \n
  another QHYCCD_ERROR code on other failures
   */
  virtual uint32_t GetCFWSlotsNum(qhyccd_handle *handle)
  {
    //OutputDebugPrintf(QHYCCD_MSGL_INFO,"QHYCCD|QHYBASE.H|GetCFWSlotsNum|Not implemented");
    //return QHYCCD_ERROR;
    return 9;
  }

  /**
  @fn virtual uint32_t IsCFWPlugged(qhyccd_handle *handle)
  @brief if CFW plugged in to the port or not.
  @param handle camera control handle
  @return
  on success,return QHYCCD_SUCCESS \n
  another QHYCCD_ERROR code on other failures
   */
  virtual uint32_t IsCFWPlugged(qhyccd_handle *handle)
  {
    OutputDebugPrintf(QHYCCD_MSGL_INFO,"QHYCCD|QHYBASE.H|IsCFWPlugged|Not implemented");
    return QHYCCD_ERROR;
  }

  /**
   @fn virtual uint32_t ControlShutter(qhyccd_handle *handle,uint8_t status)
   @brief control camera's shutter
   @param handle camera control handle
   @param status the shutter status
   @return
   on success,return QHYCCD_SUCCESS \n
   another QHYCCD_ERROR code on other failures
   */
  virtual uint32_t ControlShutter(qhyccd_handle *handle, uint8_t status)
  {
    OutputDebugPrintf(QHYCCD_MSGL_INFO,"QHYCCD|QHYBASE.H|ControlShutter|Not implemented");
    return QHYCCD_ERROR;
  }

  /**
   @fn virtual uint32_t GetShutterStatus(qhyccd_handle *handle)
   @brief get the camera's shutter status
   @param handle camera control handle
   @return
   on success,return status \n
   another QHYCCD_ERROR code on other failures
   */
  virtual uint32_t GetShutterStatus(qhyccd_handle *handle)
  {
    OutputDebugPrintf(QHYCCD_MSGL_INFO,"QHYCCD|QHYBASE.H|GetShutterStatus|Not implemented");
    return QHYCCD_ERROR;
  }

  virtual uint32_t GetHumidity(qhyccd_handle *handle, double *hd)
  {
    OutputDebugPrintf(QHYCCD_MSGL_INFO,"QHYCCD|QHYBASE.H|GetHumidity|Not implemented");
    return QHYCCD_ERROR;
  }

  virtual uint32_t SetTrigerFunction(qhyccd_handle *handle, bool value)
  {
    OutputDebugPrintf(QHYCCD_MSGL_INFO,"QHYCCD|QHYBASE.H|SetTrigerFunction|Not implemented");
    return QHYCCD_ERROR;
  }

  //---------just for debugging-------------

  virtual uint32_t I2C_Write(qhyccd_handle *handle, uint8_t req, uint16_t value, uint16_t index, uint8_t* data, uint16_t length)
  {
    OutputDebugPrintf(QHYCCD_MSGL_INFO,"QHYCCD|QHYBASE.H|I2C_Write|Not implemented");
    return QHYCCD_ERROR;
  }

  virtual uint32_t I2C_Read(qhyccd_handle *handle, uint8_t req, uint16_t value, uint16_t index, uint8_t* data, uint16_t length)
  {
    OutputDebugPrintf(QHYCCD_MSGL_INFO,"QHYCCD|QHYBASE.H|I2C_Read|Not implemented");
    return QHYCCD_ERROR;
  }
  //----------------------------------------

  virtual uint32_t SetFineTone(qhyccd_handle *h, uint8_t setshporshd, uint8_t shdloc, uint8_t shploc, uint8_t shwidth)
  {
    OutputDebugPrintf(QHYCCD_MSGL_INFO,"QHYCCD|QHYBASE.H|SetFineTone|Not implemented");
    return QHYCCD_ERROR;
  }

  virtual uint32_t ReservedFunction(uint32_t width, uint32_t height, uint32_t bpp, uint32_t channels, uint8_t *ImgData)
  {
    return QHYCCD_SUCCESS;
  }

  virtual uint32_t IsExposing(qhyccd_handle *h)
  {
    return QHYCCD_SUCCESS;
  }

  virtual void UpdateParameters(qhyccd_handle *h)
  {

  }
  /**
    @fn virtual uint32_t GetFWVersion(qhyccd_handle *h,uint8_t *buf)
    @brief Get the QHYCCD's firmware version
    @param h camera control handle
    @param buf buffer for version info
    @return
    on success,return QHYCCD_SUCCESS \n
    another QHYCCD_ERROR code on other failures
   */
  virtual uint32_t GetFWVersion(qhyccd_handle *h, uint8_t *buf);

  virtual uint32_t SetGuideModeOnOff(qhyccd_handle *h, double mode)
  {
    OutputDebugPrintf(QHYCCD_MSGL_INFO,"QHYCCD|QHYBASE.H|SetGuideModeOnOff|Not implemented");
    return QHYCCD_ERROR;
  }

  virtual uint32_t GeDDRBufferThreshold()
  {
    return 0;
  }
  virtual uint32_t SetDarkGenerateOnOff(qhyccd_handle *h, double mode);
public:

  /**
   @fn void ControlCamTemp(qhyccd_handle *h,double MAXPWM)
   @brief control the ccd/cmos temprature
   @param h camera control handle
   @param MAXPWM the max power of cool
   */
  void ControlCamTemp(qhyccd_handle *h, double MAXPWM);

  /**
   @fn void Bit16To8_Stretch(uint8_t *InputData16,uint8_t \ *OutputData8,uint32_t imageX,uint32_t imageY,uint16_t B,uint16_t W)
   @brief turn 16bits data into 8bits
   @param InputData16 for 16bits data memory
   @param OutputData8 for 8bits data memory
   @param imageX image width
   @param imageY image height
   @param B for stretch balck
   @param W for stretch white
   */
  void Bit16To8_Stretch(uint8_t *InputData16, uint8_t *OutputData8, uint32_t imageX, uint32_t imageY, uint16_t B, uint16_t W);

  /**
   @fn void HistInfo(uint32_t x,uint32_t y,uint8_t *InBuf,uint8_t *OutBuf)
   @brief make the hist info
   @param x image width
   @param y image height
   @param InBuf for the raw image data
   @param OutBuf for 192x130 8bits 3 channels image
   */
  void HistInfo(uint32_t x, uint32_t y, uint8_t *InBuf, uint8_t *OutBuf);

  void calibration_difference(uint8_t *inbuf, uint8_t *outbuf, uint32_t width, uint32_t height, uint32_t depth, uint32_t areax1, uint32_t areay1, uint32_t areasizex1, uint32_t areasizey1, uint32_t areax2, uint32_t areay2, uint32_t areasizex2, uint32_t areasizey2);
  /**
   @fn void CalibrateOverScan(uint8_t inbuf, uint8_t outbuf, uint32_t ImgW, uint32_t ImgH, uint32_t OSStartX, uint32_t OSStartY, uint32_t OSSizeX, uint32_t OSSizeY);
   @brief calibrate the image with its overscan arean
   @param x image width
   @param y image height
   @param OSStartX the start X for overscan arean
   @param OSStartY the start Y for overscan arean
   @param OSSizeX the X size for overscan arean
   @param OSSizeY the Y size for overscan arean
   @param InBuf for the raw image data
   @param OutBuf for the image data after calibrated
   */
  void CalibrateOverScan(uint8_t *inbuf, uint8_t *outbuf, uint32_t ImgW, uint32_t ImgH, uint32_t OSStartX, uint32_t OSStartY, uint32_t OSSizeX, uint32_t OSSizeY);


  /**
   @fn void IgnoreOverscanArea(qhyccd_handle *h, bool value);
   @brief set camera to ignore overscan area
   @param h camera control handle
   @param value ignore overscan area or not
   */
  uint32_t IgnoreOverscanArea(qhyccd_handle *h, bool value);

  /** @fn uint32_t GetOverScanArea(uint32_t *startX, uint32_t *startY, uint32_t *sizeX, uint32_t *sizeY)

    @brief get the ccd overscan area
    @param startX the OverScan area x position
    @param startY the OverScan area y position
    @param sizeX the OverScan area x size

    @param sizeY the OverScan area y size
    @return
    on success,return QHYCCD_SUCCESS \n
    another QHYCCD_ERROR code on other failures
   */
  uint32_t GetOverScanArea(uint32_t *startX, uint32_t *startY, uint32_t *sizeX, uint32_t *sizeY);

  /** @fn uint32_t GetEffectiveArea(qint *startX, uint32_t *startY, uint32_t *sizeX, uint32_t *sizeY)
    @brief get the ccd effective area
    @param startX the Effective area x position
    @param startY the Effective area y position

    @param sizeX the Effective area x size
    @param sizeY the Effective area y size
    @return
    on success,return QHYCCD_SUCCESS \n
    another QHYCCD_ERROR code on other failures

   */
  uint32_t GetEffectiveArea(uint32_t *startX, uint32_t *startY, uint32_t *sizeX, uint32_t *sizeY);

  /** @fn uint32_t GetChipInfo(double *chipw,double *chiph,uint32_t *imagew,uint32_t *imageh,double *pixelw,double *pixelh)
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
  uint32_t GetChipInfo(double *chipw, double *chiph, uint32_t *imagew, uint32_t *imageh, double *pixelw, double *pixelh, uint32_t *bpp);

  /**
    @fn double GetReadingProgress(qhyccd_handle *handle)
    @brief get reading data from camera progress
    @param handle camera control handle
    @return current progress
   */
  virtual double GetReadingProgress(qhyccd_handle *handle);

  uint32_t SetGPSOn(qhyccd_handle *handle, uint8_t mode);

  uint32_t GetGPSSOnOff()
  {
    return gpson;
  }

  uint32_t QSleep(uint32_t mstime)
  {
#if defined (_WIN32)
    Sleep(mstime);
#else

    usleep(mstime * 1000);
#endif

    return QHYCCD_SUCCESS;
  }

  /**
   */
  uint32_t SetPIDParas(qhyccd_handle *handle, double p, double i, double d);

  uint32_t PixelsDataSoftBin(uint8_t *srcdata, uint8_t *bindata, uint32_t width, uint32_t height, uint32_t depth, uint32_t camxbin, uint32_t camybin);

  uint32_t SetDebayerOnOff(bool onoff);

  uint32_t RoiTwoChannels2OneImage(uint32_t imgX, uint32_t imgY, uint32_t depth, uint32_t uselessStartX, uint32_t uselessStartY, uint32_t uselessSizeX, uint32_t uselessSizeY, uint8_t *data);

  virtual uint32_t GetLiveFrame(qhyccd_handle *h, uint32_t *pW, uint32_t *pH, uint32_t * pBpp, uint32_t *pChannels, uint8_t *ImgData);

  virtual uint32_t GetSingleFrame(qhyccd_handle *h, uint32_t *pW, uint32_t *pH, uint32_t * pBpp, uint32_t *pChannels, uint8_t *ImgData);

  uint32_t VendRequestWrite(qhyccd_handle *h, uint8_t req, uint16_t value, uint16_t index, uint32_t length, uint8_t *data);

  uint32_t VendRequestRead(qhyccd_handle *h, uint8_t req, uint16_t value, uint16_t index, uint32_t length, uint8_t *data);

  void QHYCCDDemosaic(void* dataIn, uint32_t w, uint32_t h, uint32_t bpp, void* dataOut, uint8_t mode);

  uint32_t QHYCCDImageROI(void* src, uint32_t chipoutputsizex, uint32_t chipoutputsizey, uint32_t cambits, void* dist, uint32_t roixstart, uint32_t roiystart, uint32_t roixsize, uint32_t roiysize);

  uint32_t QHYCCDFlip(void* src, uint32_t xsize, uint32_t ysize, uint32_t cambits, int channels, void* dist, int flip_mode);

  uint32_t QHYCCDFlip(void* src, uint32_t xsize, uint32_t ysize, uint32_t cambits, int channels, int flip_mode);

  uint32_t SetAutoWhiteBalance(qhyccd_handle *h, double value);

  uint32_t SetAutoExposure(qhyccd_handle *h, double value);

  uint32_t SetAutoFocus(qhyccd_handle *h, double value);

  uint32_t SetBrightness(qhyccd_handle *h, double value);

  uint32_t SetContrast(qhyccd_handle *h, double value);

  uint32_t SetGamma(qhyccd_handle *h, double value);

  uint32_t SetAMPV(qhyccd_handle *h, double value);

  virtual uint32_t SetDDR(qhyccd_handle *h, double value);

  uint32_t SetLPMode(qhyccd_handle *h, double value);

  uint32_t SetCamViewMode(qhyccd_handle *h, double value);

  uint32_t SetVcamOnoff(qhyccd_handle *h, double mode);

  uint32_t SetScreenStretchB(qhyccd_handle *h, double value);

  uint32_t SetScreenStretchW(qhyccd_handle *h, double value);


  double GetBrightness();

  double GetContrast();

  double GetGamma();

  double GetAMPV();

  double GetDDR();

  double GetLPMode();

  double GetCamViewModeStatus();

  double GetVcamOnoff();

  double GetScreenStretchB();

  double GetScreenStretchW();

  double GetDefaultGain();

  double GetDefaultOffset();

  double GetOutputDataActualBits();

  double GetOutputDataAlignment();
  void BuildlLut_Contrast_Brightness_Gamma(uint32_t bpp, double brightness_percent, double contrast_percent, double fPrecompensation);

  void ImgProcess_Contrast_Brightness_Gamma(uint8_t *array, uint32_t width, uint32_t height, uint32_t bpp);

  uint32_t GetTempAndPwm(qhyccd_handle *h, double &temp, double &pwm);

  uint32_t QHYImgResize(void *src, uint32_t bpp, uint32_t ch, uint32_t src_width, uint32_t src_height, void *dst, uint32_t dst_width, uint32_t dst_height);

  uint32_t QHYBadLineProc(void *src, uint32_t imgw, uint32_t imgh, uint32_t bpp, uint32_t startx, uint32_t starty, uint32_t linew, uint32_t endy, bool method);

  double Getqhy5iiGuidePortOnoff();

  uint32_t GeDDRBufferCap(qhyccd_handle *h);

  uint32_t QHYConvertToSoftBIN22(void *src, uint32_t bpp, uint32_t src_width, uint32_t src_height, void *dst);
  uint32_t QHYConvertToSoftBIN33(void *src, uint32_t bpp, uint32_t src_width, uint32_t src_height, void *dst);
  uint32_t QHYConvertToSoftBIN44(void *src, uint32_t bpp, uint32_t src_width, uint32_t src_height, void *dst);




  uint8_t camtype;

  uint32_t camx; //!< current camera width
  uint32_t camy; //!< current camera height
  uint32_t camxbin; //!< current camera width bin
  uint32_t camybin; //!< current camera height bin
  uint32_t cambits; //!< current camera bits mode
  uint32_t camchannels; //!< current camera channels
  uint32_t usbtraffic; //!< current usbtraffic
  uint32_t usbspeed; //!< current usb speed mode
  double camtime; //!< current cam expose time
  double camgain; //!< current cam gain
  double camoffset; //!< current cam offset
  double camred2green; //!< current white blance red value
  double camblue2green; //!< current white blance blue value
  double camgreen; //!< current white blance green value
  uint8_t *rawarray; //!< raw buffer pointer for usb transfer
  uint8_t *roiarray; //!< roi image buffer

  uint32_t roixstart; //!< roi image xstart position
  uint32_t roiystart; //!< roi image ystart position
  uint32_t roixsize; //!< roi image xsize
  uint32_t roiysize; //!< roi image ysize

  uint32_t unbinningx;
  uint32_t unbinningy;
  uint32_t unbinningxsize;
  uint32_t unbinningysize;

  uint32_t overScanStartX; //!< overscan area x position
  uint32_t overScanStartY; //!< overscan area y position
  uint32_t overScanSizeX; //!< overscan area x size
  uint32_t overScanSizeY; //!< overscan area y size
  uint32_t onlyStartX; //!< effective area x position
  uint32_t onlyStartY; //!< effective area y position
  uint32_t onlySizeX; //!< effective area x size
  uint32_t onlySizeY; //!< effective area y size

  double ccdchipw; //!< ccd chip width
  double ccdchiph; //!< ccd chip height
  uint32_t ccdimagew; //!< ccd image width
  uint32_t ccdimageh; //!< ccd image height
  double ccdpixelw; //!< ccd pixel width
  double ccdpixelh; //!< ccd pixel height

  uint32_t lastx, lasty, lastxsize, lastysize, lastcambits;
  uint32_t lastcamxbin, lastcamybin;
  uint32_t chipoutputx, chipoutputy, chipoutputsizex, chipoutputsizey, chipoutputbits;

  double targetTEMP; //!< target ccd/cmos temprature
  double currentTEMP; //!< current ccd/cmos tempratue
  double currentPWM; //!< current cool power
  double nowVoltage; //!< reserved
  bool flag_timer; //!< timer flag
  bool flag_timer_2; //!< timer flag 2
  bool flagtempauto; //!< temperature to read flag

  double NowError; //!< the step k difference
  double PrevError; //!< the step k - 2 difference
  double LastError; //!< the step k - 1 difference
  double Proportion; //!< scale factor value
  double Integral; //!< integralcoefficients
  double Derivative; //!< differential coefficient

  bool flagquit; //!< the global var for quit signal

  double readprogress; //!< get the reading data from camera progress status

  uint8_t isbadframe;
  uint8_t badframenum;

  uint8_t isexposureupdate;
  uint8_t isgainupdate;
  uint8_t iscolorgainupdate;
  uint8_t isoffsetupdate;
  uint8_t isdepthupdate;
  uint8_t isspeedupdate;
  uint8_t isresolutionupdate;
  uint8_t isusbtrafficupdate;

  uint8_t streammode; //!< the way to reading data from camera

  int8_t filterpos;

  bool isReadoutData; //!< sign whether it is reading out the image data

  int32_t frameflag;

  double humidityvalue;

  bool debayeronoff;
  uint32_t debayerformat;

  uint32_t initdone;
  uint32_t connected;
  uint32_t resolutionmode;
  uint32_t darkgen_on;
  uint32_t uselessstartx;
  uint32_t uselessstarty;
  uint32_t uselesssizex;
  uint32_t uselesssizey;

  uint8_t is_superspeed;
  uint8_t gpson;
  uint8_t gpsarray[5000 * 11 * 2];

  uint8_t is_3a_autoexposure_on;
  uint8_t is_3a_autowhitebalance_on;
  uint8_t is_3a_autofocus_on;
  uint8_t autoexposure_messuremethod;
  uint8_t autoexposure_controlmode;
  uint8_t autowhitebalanceloops;

  double imgbrightness;
  double imgcontrast;
  double imggamma;
  double camampv;
  double defaultgain;
  double defaultoffset;
  double outputdataactualbits;
  double outputdataalignment;
  uint16_t camddr;
  uint8_t camLPMode; // 1: high light performance or 0: low light performance
  double camviewmode;
  bool vcamonoff;
  int32_t imgprocesslut[65536];

  bool isOverscanRemoved; //!< sign whether removed the overscan area for image.
  bool isFocusmode; //!< hold the camera work mode, false: capture mode, true: focus mdoe

  uint8_t isReadoutTemp;
  uint8_t islive;
  uint8_t singlestatus;
  uint16_t screenstretchb, screenstretchw;

  bool isFX3;

  uint8_t qhy5iiGuidePortOnOff;
  bool delRowRoise;
  uint32_t ddrnum;

#if defined (_WIN32)

  HANDLE reseth;
#else

#endif

};

#endif
