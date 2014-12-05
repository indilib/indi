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

/*! @file qhy5ii.h
 *  @brief QHY5II class define
 */

#include "qhybase.h"
#include <opencv/cv.h>

#ifndef QHY5II_DEF
#define QHY5II_DEF

/**
 * @brief QHY5II class define
 *
 * include all functions for qhy5ii
 */
class QHY5II:public QHYBASE
{
public:
    QHY5II();
    ~QHY5II();
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
     @fn virtual double GetChipUSBTraffic()
     @brief get the hblank value
     @return
     success return the hblank value \n
     another QHYCCD_ERROR code on other failures
     */
    double GetChipUSBTraffic();
    
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
     @fn int Init1280x1024(qhyccd_handle *h)
     @brief init the 1280x1024 resolution
     @param h camera control handle
     @return
     success return QHYCCD_SUCCESS \n
     another QHYCCD_ERROR code on other failures
     */
    int Init1280x1024(qhyccd_handle *h);
    
    /**
     @fn int Init1024x768(qhyccd_handle *h)
     @brief init the 1024x768 resolution
     @param h camera control handle
     @return
     success return QHYCCD_SUCCESS \n
     another QHYCCD_ERROR code on other failures
     */
    int Init1024x768(qhyccd_handle *h);
    
    /**
     @fn int Init800x600(qhyccd_handle *h)
     @brief init the 800x600 resolution
     @param h camera control handle
     @return
     success return QHYCCD_SUCCESS \n
     another QHYCCD_ERROR code on other failures
     */
    int Init800x600(qhyccd_handle *h);
    
    /**
     @fn int Init640x480(qhyccd_handle *h)
     @brief init the 640x480 resolution
     @param h camera control handle
     @return
     success return QHYCCD_SUCCESS \n
     another QHYCCD_ERROR code on other failures
     */
    int Init640x480(qhyccd_handle *h);
    
    /**
     @fn int Init320x240(qhyccd_handle *h)
     @brief init the 320x240 resolution
     @param h camera control handle
     @return
     success return QHYCCD_SUCCESS \n
     another QHYCCD_ERROR code on other failures
     */
    int Init320x240(qhyccd_handle *h);
    
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
     @fn int SetChipUSBTraffic(qhyccd_handle *h,int i)
     @brief set hblank
     @param h camera control handle
     @param i hblank value
     @return
     success return QHYCCD_SUCCESS \n
     another QHYCCD_ERROR code on other failures
     */
    int SetChipUSBTraffic(qhyccd_handle *h,int i);
    
    /**
     @fn int Send2GuiderPort(qhyccd_handle *h,unsigned char Direction,unsigned short PulseTime)
     @brief send the command to camera's guide port
     @param h camera control handle
     @param Direction RA DEC
     @param PulseTime the time last for command
     @return
     success return QHYCCD_SUCCESS \n
     another QHYCCD_ERROR code on other failures
     */
    int Send2GuiderPort(qhyccd_handle *h,unsigned char Direction,unsigned short PulseTime);
    
    /**
     @fn int SetPll(qhyccd_handle *h,unsigned char clk)
     @brief set the cmos inter pll
     @param h camera control handle
     @param clk clock
     @return
     success return QHYCCD_SUCCESS \n
     another QHYCCD_ERROR code on other failures
     */
    int SetPll(qhyccd_handle *h,unsigned char clk);
private:
    int expmode;  //!< expose time mode
    int pllratio; //!< inter pll ratio
    
};
#endif
