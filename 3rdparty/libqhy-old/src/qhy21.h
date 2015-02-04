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
 * @file qhy21.h
 * @brief QHY21 class define
 */

#include "qhybase.h"
#include <opencv/cv.h>

#ifndef __QHY21DEF_H__
#define __QHY21DEF_H__

/**
 * @brief QHY21 class define
 *
 * include all functions for QHY21
 */
class QHY21:public QHYBASE
{
public:
    QHY21();
    ~QHY21();
    /**
     @fn int ConnectCamera(libusb_device *d,qhyccd_handle **h)
     @brief connect to the connected camera
     @param d camera deivce
     @param h camera control handle
     @return
     success return QHYCCD_SUCCESS \n
     another QHYCCD_ERROR code on other failures
     */
    int ConnectCamera(libusb_device *d,qhyccd_handle **h);
    
    /**
     @fn int DisConnectCamera(qhyccd_handle *h)
     @brief disconnect to the connected camera
     @param h camera control handle
     @return
     success return QHYCCD_SUCCESS \n
     another QHYCCD_ERROR code on other failures
     */
    int DisConnectCamera(qhyccd_handle *h);
    
    /**
     @fn int InitChipRegs(qhyccd_handle *h)
     @brief Init the registers and some other things
     @param h camera control handle
     @return
     success return QHYCCD_SUCCESS \n
     another QHYCCD_ERROR code on other failures
     */
    int InitChipRegs(qhyccd_handle *h);
    
    /**
     @fn int IsChipHasFunction(CONTROL_ID id)
     @brief check the camera has the function or not
     @param id function id
     @return
     HAVE return QHYCCD_HAVE \n
     NOT HAVE return QHYCCD_NOTHAVE
     */
    int IsChipHasFunction(CONTROL_ID id);
    
    /**
     @fn int IsColorCam()
     @brief check the camera is color or mono one
     @return
     mono chip return QHYCCD_MONO \n
     color chip return QHYCCD_COLOR
     */
    int IsColorCam();
    
    /**
     @fn int IsCoolCam()
     @brief check the camera has cool function or not
     @return
     HAVE return QHYCCD_COOL \n
     NOT HAVE return QHYCCD_NOTCOOL
     */
    int IsCoolCam();
    
    /**
     @fn int ReSetParams2cam(qhyccd_handle *h)
     @brief re set the params to camera,because some behavior will cause camera reset
     @param h camera control handle
     @return
     success return QHYCCD_SUCCESS \n
     another QHYCCD_ERROR code on other failures
     */
    int ReSetParams2cam(qhyccd_handle *h);
    
    /**
     @fn int SetChipGain(qhyccd_handle *h,double gain)
     @brief set the gain to camera
     @param h camera control handle
     @param gain gain value
     @return
     success return QHYCCD_SUCCESS \n
     another QHYCCD_ERROR code on other failures
     */
    int SetChipGain(qhyccd_handle *h,double gain);
    
    /**
     @fn int SetChipExposeTime(qhyccd_handle *h,double i)
     @brief set the expose time to camera
     @param h camera control handle
     @param i expose time value
     @return
     success return QHYCCD_SUCCESS \n
     another QHYCCD_ERROR code on other failures
     */
    int SetChipExposeTime(qhyccd_handle *h,double i);
    
    /**
     @fn int SetChipSpeed(qhyccd_handle *h,int i)
     @brief set the transfer speed to camera
     @param h camera control handle
     @param i speed level
     @return
     success return QHYCCD_SUCCESS \n
     another QHYCCD_ERROR code on other failures
     */
    int SetChipSpeed(qhyccd_handle *h,int i);
    
    /**
     @fn int SetChipOffset(qhyccd_handle *h,double offset)
     @brief set the camera offset
     @param h camera control handle
     @param offset offset value
     @return
     success return QHYCCD_SUCCESS \n
     another QHYCCD_ERROR code on other failures
     */
    int SetChipOffset(qhyccd_handle *h,double offset);
    
    /**
     @fn int SetChipBinMode(qhyccd_handle *h,int wbin,int hbin)
     @brief set the camera offset
     @param h camera control handle
     @param wbin width bin
     @param hbin height bin
     @return
     success return QHYCCD_SUCCESS \n
     another QHYCCD_ERROR code on other failures
     */
    int SetChipBinMode(qhyccd_handle *h,int wbin,int hbin);
    
    /**
     @fn int Send2CFWPort(qhyccd_handle *h,int pos)
     @brief send the command to camera's color filter wheel port
     @param h camera control handle
     @param pos the color filter position
     @return
     success return QHYCCD_SUCCESS \n
     another QHYCCD_ERROR code on other failures
     */
    int Send2CFWPort(qhyccd_handle *h,int pos);
    
    /**
     @fn int GetControlMinMaxStepValue(CONTROL_ID controlId,double *min,double *max,double *step)
     @brief get the min,max and step value for function
     @param controlId the control id
     @param min the min value for function
     @param max the max value for function
     @param step single step value for function
     @return
     success return QHYCCD_SUCCESS \n
     another QHYCCD_ERROR code on other failures
     */
    int GetControlMinMaxStepValue(CONTROL_ID controlId,double *min,double *max,double *step);
    
    /**
     @fn int GetChipMemoryLength()
     @brief get the min cost memory for the image
     @return
     success return the total memory space(unit:byte) \n
     another QHYCCD_ERROR code on other failures
     */
    int GetChipMemoryLength();
    
    /**
     @fn double GetChipExposeTime()
     @brief get the current exposetime
     @return
     success return the current expose time (unit:us) \n
     another QHYCCD_ERROR code on other failures
     */
    double GetChipExposeTime();
    
    /**
     @fn double GetChipGain()
     @brief get the current gain
     @return
     success return the current expose gain\n
     another QHYCCD_ERROR code on other failures
     */
    double GetChipGain();
    
    /**
     @fn double GetChipSpeed()
     @brief get the current transfer speed
     @return
     success return the current speed level \n
     another QHYCCD_ERROR code on other failures
     */
    double GetChipSpeed();
    
    /**
     @fn double GetChipOffset()
     @brief get the current offset
     @return
     success return the current camera offset \n
     another QHYCCD_ERROR code on other failures
     */
    double GetChipOffset();
    
    /**
     @fn double GetChipBitsMode()
     @brief get the current camera depth bits
     @return
     success return the current camera depth bits \n
     another QHYCCD_ERROR code on other failures
     */
    double GetChipBitsMode();
    
    /**
     @fn double GetChipCoolTemp(qhyccd_handle *h)
     @brief get the current ccd/cmos temprature
     @param h camera control handle
     @return
     success return the current cool temprature \n
     another QHYCCD_ERROR code on other failures
     */
    double GetChipCoolTemp(qhyccd_handle *h);
    
    /**
     @fn double GetChipCoolPWM()
     @brief get the current ccd/cmos temprature
     @return
     success return the current cool temprature \n
     another QHYCCD_ERROR code on other failures
     */
    double GetChipCoolPWM();
    
    /**
     @fn int CorrectWH(int *w,int *h)
     @brief correct width and height if the setting width or height is not correct
     @param w set width
     @param h set height
     @return
     success return QHYCCD_SUCCESS \n
     another QHYCCD_ERROR code on other failures
     */
    int CorrectWH(int *w,int *h);
    
    /**
     @fn double InitBIN11Mode()
     @brief init the bin11 mode setting
     @return
     success return QHYCCD_SUCCESS \n
     another QHYCCD_ERROR code on other failures
     */
    int InitBIN11Mode();
    
    /**
     @fn double InitBIN22Mode()
     @brief init the bin22 mode setting
     @return
     success return QHYCCD_SUCCESS \n
     another QHYCCD_ERROR code on other failures
     */
    int InitBIN22Mode();
    
    /**
     @fn double InitBIN44Mode()
     @brief init the bin44 mode setting
     @return
     success return QHYCCD_SUCCESS \n
     another QHYCCD_ERROR code on other failures
     */
    int InitBIN44Mode();
    
    /**
     @fn int SetChipResolution(qhyccd_handle *h,int x,int y)
     @brief set the camera resolution
     @param h camera control handle
     @param x width
     @param y height
     @return
     success return QHYCCD_SUCCESS \n
     another QHYCCD_ERROR code on other failures
     */
    int SetChipResolution(qhyccd_handle *h,int x,int y);
    
    /**
     @fn int BeginSingleExposure(qhyccd_handle *h)
     @brief begin single exposure
     @param h camera control handle
     @return
     success return QHYCCD_SUCCESS \n
     another QHYCCD_ERROR code on other failures
     */
    int BeginSingleExposure(qhyccd_handle *h);
    
    /**
     @fn int StopSingleExposure(qhyccd_handle *h)
     @brief stop single exposure
     @param h camera control handle
     @return
     success return QHYCCD_SUCCESS \n
     another QHYCCD_ERROR code on other failures
     */
    int StopSingleExposure(qhyccd_handle *h);
    
    /**
     @fn int GetSingleFrame(qhyccd_handle *h,int *pW,int *pH,int * pBpp,int *pChannels,unsigned char *ImgData)
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
    int GetSingleFrame(qhyccd_handle *h,int *pW,int *pH,int * pBpp,int *pChannels,unsigned char *ImgData);
    
    /**
     @fn int BeginLiveExposure(qhyccd_handle *h)
     @brief begin live exposure
     @param h camera control handle
     @return
     success return QHYCCD_SUCCESS \n
     another QHYCCD_ERROR code on other failures
     */
    int BeginLiveExposure(qhyccd_handle *h);
    
    /**
     @fn int StopLiveExposure(qhyccd_handle *h)
     @brief stop live exposure
     @param h camera control handle
     @return
     success return QHYCCD_SUCCESS \n
     another QHYCCD_ERROR code on other failures
     */
    int StopLiveExposure(qhyccd_handle *h);
    
    /**
     @fn int GetLiveFrame(qhyccd_handle *h,int *pW,int *pH,int * pBpp,int *pChannels,unsigned char *ImgData)
     @brief get live frame image data
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
    int GetLiveFrame(qhyccd_handle *h,int *pW,int *pH,int * pBpp,int *pChannels,unsigned char *ImgData);
    
    /**
     @fn int AutoTempControl(qhyccd_handle *h,double ttemp)
     @brief auto temprature control
     @param h camera control handle
     @param ttemp target temprature(degree Celsius)
     @return
     success return QHYCCD_SUCCESS \n
     another QHYCCD_ERROR code on other failures
     */
    int AutoTempControl(qhyccd_handle *h,double ttemp);
    
    /**
     @fn int SetChipCoolPWM(qhyccd_handle *h,double PWM)
     @brief set cool power
     @param h camera control handle
     @param PWM power(0-255)
     @return
     success return QHYCCD_SUCCESS \n
     another QHYCCD_ERROR code on other failures
     */
    int SetChipCoolPWM(qhyccd_handle *h,double PWM);
    
    /**
     @fn void ConvertDataBIN11(unsigned char * Data,int x, int y, unsigned short PixShift)
     @brief move the pixel raw data to correction position,and bin if need
     @param Data raw image data
     @param x image width
     @param y image height
     @param PixShift this is a way to fix the bad pixel data by the usb transfer
     */
    void ConvertDataBIN11(unsigned char * Data,int x, int y, unsigned short PixShift);
    
    /**
     @fn void ConvertDataBIN22(unsigned char * Data,int x, int y, unsigned short PixShift)
     @brief move the pixel raw data to correction position,and bin if need
     @param Data raw image data
     @param x image width
     @param y image height
     @param PixShift this is a way to fix the bad pixel data by the usb transfer
     */
    void ConvertDataBIN22(unsigned char * Data,int x, int y, unsigned short PixShift);
    
    /**
     @fn void ConvertDataBIN44(unsigned char * Data,int x, int y, unsigned short PixShift)
     @brief move the pixel raw data to correction position,and bin if need
     @param Data raw image data
     @param x image width
     @param y image height
     @param PixShift this is a way to fix the bad pixel data by the usb transfer
     */
    void ConvertDataBIN44(unsigned char * Data,int x, int y, unsigned short PixShift);
};
#endif

