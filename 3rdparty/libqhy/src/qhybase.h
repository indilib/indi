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
#include <pthread.h>
#include <opencv/cv.h>


#ifndef __QHYBASEDEF_H__
#define __QHYBASEDEF_H__

/**
 * the QHYBASE class description
 */
class QHYBASE:public QHYCAM
{
public:
    QHYBASE()
    {

    }
    ~QHYBASE(){};
    
    /**
     @fn virtual int ConnectCamera(qhyccd_device *d,qhyccd_handle **h)
     @brief connect camera,get the control handle
     @param d deivce
     @param h camera control handle pointer
     @return
     success return QHYCCD_SUCCESS \n
     another QHYCCD_ERROR code on other failures
     */
    virtual int ConnectCamera(qhyccd_device *d,qhyccd_handle **h)
    {
        return QHYCCD_ERROR;
    }
    
    /**
     @fn virtual int DisConnectCamera(qhyccd_handle *h)
     @brief disconnect camera
     @param h camera control handle
     @return
     success return QHYCCD_SUCCESS \n
     another QHYCCD_ERROR code on other failures
     */
    virtual int DisConnectCamera(qhyccd_handle *h)
    {
        return QHYCCD_ERROR;
    }
    
    /**
     @fn virtual int InitChipRegs(qhyccd_handle *h)
     @brief Init the registers and some other things
     @param h camera control handle
     @return
     success return QHYCCD_SUCCESS \n
     another QHYCCD_ERROR code on other failures
     */
    virtual int InitChipRegs(qhyccd_handle *h)
    {
        return QHYCCD_ERROR;
    }
    
    /**
     @fn virtual int SetChipOffset(qhyccd_handle *h,double offset)
     @brief set the camera offset
     @param h camera control handle
     @param offset offset value
     @return
     success return QHYCCD_SUCCESS \n
     another QHYCCD_ERROR code on other failures
     */
    virtual int SetChipOffset(qhyccd_handle *h,double offset)
    {
        return QHYCCD_ERROR;
    }
    
    /**
     @fn virtual int SetChipExposeTime(qhyccd_handle *h,double i)
     @brief set the expose time to camera
     @param h camera control handle
     @param i expose time value
     @return
     success return QHYCCD_SUCCESS \n
     another QHYCCD_ERROR code on other failures
     */
    virtual int SetChipExposeTime(qhyccd_handle *h,double i)
    {
        return QHYCCD_ERROR;
    }
    
    /**
     @fn virtual int SetChipGain(qhyccd_handle *h,double gain)
     @brief set the gain to camera
     @param h camera control handle
     @param gain gain value
     @return
     success return QHYCCD_SUCCESS \n
     another QHYCCD_ERROR code on other failures
     */
    virtual int SetChipGain(qhyccd_handle *h,double gain)
    {
        return QHYCCD_ERROR;
    }
    
    /**
     @fn virtual int SetChipGainColor(qhyccd_handle *h,double gain,double RG,double BG)
     @brief set color gain to camera
     @param h camera control handle
     @param gain gain value
     @param RG the red gain value compared to green gain
     @param BG the blue gain value compared to green gain
     @return
     success return QHYCCD_SUCCESS \n
     another QHYCCD_ERROR code on other failures
     */
    virtual int SetChipGainColor(qhyccd_handle *h,double gain, double RG, double BG)
    {
        return QHYCCD_ERROR;
    }
    
    /**
     @fn virtual int SetChipWBRed(qhyccd_handle *h,double red)
     @brief set the red gain value of white balance
     @param h camera control handle
     @param red red gain value
     @return
     success return QHYCCD_SUCCESS \n
     another QHYCCD_ERROR code on other failures
     */
    virtual int SetChipWBRed(qhyccd_handle *h,double red)
    {
        return QHYCCD_ERROR;
    }

    /**
     @fn virtual int SetChipWBGreen(qhyccd_handle *h,double green)
     @brief set the red gain value of white balance
     @param h camera control handle
     @param green green gain value
     @return
     success return QHYCCD_SUCCESS \n
     another QHYCCD_ERROR code on other failures
     */
    virtual int SetChipWBGreen(qhyccd_handle *h,double green)
    {
        return QHYCCD_ERROR;
    }
    
    /**
     @fn virtual int SetChipWBBlue(qhyccd_handle *h,double blue)
     @brief set the red gain value of white balance
     @param h camera control handle
     @param blue blue gain value
     @return
     success return QHYCCD_SUCCESS \n
     another QHYCCD_ERROR code on other failures
     */
    virtual int SetChipWBBlue(qhyccd_handle *h,double blue)
    {
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
        return QHYCCD_ERROR;
    }
    
    /**
     @fn virtual double GetChipWBBlue()
     @brief get the blue gain value of white balance
     @return
     success return blue gain value \n
     another QHYCCD_ERROR code on other failures
     */
    virtual double GetChipWBBlue()
    {
        return QHYCCD_ERROR;
    }
    
    /**
     @fn virtual double GetChipWBGreen()
     @brief get the green gain value of white balance
     @return
     success return green gain value \n
     another QHYCCD_ERROR code on other failures
     */
    virtual double GetChipWBGreen()
    {
        return QHYCCD_ERROR;
    }
    
    /**
     @fn virtual double GetChipExposeTime()
     @brief get the current exposetime
     @return
     success return the current expose time (unit:us) \n
     another QHYCCD_ERROR code on other failures
     */
    virtual double GetChipExposeTime()
    {
        return QHYCCD_ERROR;
    }
    
    /**
     @fn virtual double GetChipGain()
     @brief get the current gain
     @return
     success return the current expose gain\n
     another QHYCCD_ERROR code on other failures
     */
    virtual double GetChipGain()
    {
        return QHYCCD_ERROR;
    }
    
    /**
     @fn virtual double GetChipOffset()
     @brief get the current offset
     @return
     success return the current camera offset \n
     another QHYCCD_ERROR code on other failures
     */
    virtual double GetChipOffset()
    {
        return QHYCCD_ERROR;
    }
    
    /**
     @fn virtual double GetChipSpeed()
     @brief get the current transfer speed
     @return
     success return the current speed level \n
     another QHYCCD_ERROR code on other failures
     */
    virtual double GetChipSpeed()
    {
        return QHYCCD_ERROR;
    }
  
    /**
     @fn virtual double GetChipUSBTraffic()
     @brief get the hblank value
     @return
     success return the hblank value \n
     another QHYCCD_ERROR code on other failures
     */
    virtual double GetChipUSBTraffic()
    {
        return QHYCCD_ERROR;
    }
    
    /**
     @fn virtual double GetChipBitsMode()
     @brief get the current camera depth bits
     @return
     success return the current camera depth bits \n
     another QHYCCD_ERROR code on other failures
     */
    virtual double GetChipBitsMode()
    {
        return QHYCCD_ERROR;
    }
 
    /**
     @fn virtual double GetChipChannels()
     @brief get the current camera image channels
     @return
     success return the current camera image channels \n
     another QHYCCD_ERROR code on other failures
     */
    virtual double GetChipChannels()
    {
        return QHYCCD_ERROR;
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
    virtual int GetControlMinMaxStepValue(CONTROL_ID controlId,double *min,double *max,double *step)
    {
        return QHYCCD_ERROR;
    }
    
    /**
     @fn virtual int CorrectWH(int *w,int *h)
     @brief correct width and height if the setting width or height is not correct
     @param w set width
     @param h set height
     @return
     success return QHYCCD_SUCCESS \n
     another QHYCCD_ERROR code on other failures
     */
    virtual int CorrectWH(int *w,int *h)
    {
        return QHYCCD_ERROR;
    }
    
    /**
     @fn virtual int SetChipResolution(qhyccd_handle *h,int x,int y)
     @brief set the camera resolution
     @param h camera control handle
     @param x width
     @param y height
     @return
     success return QHYCCD_SUCCESS \n
     another QHYCCD_ERROR code on other failures
     */
    virtual int SetChipResolution(qhyccd_handle *h,int x,int y)
    {
        return QHYCCD_ERROR;
    }
    
    /**
     @fn virtual int BeginSingleExposure(qhyccd_handle *h)
     @brief begin single exposure
     @param h camera control handle
     @return
     success return QHYCCD_SUCCESS \n
     another QHYCCD_ERROR code on other failures
     */
    virtual int BeginSingleExposure(qhyccd_handle *h)
    {
        return QHYCCD_ERROR;
    }
    
    /**
     @fn virtual int StopSingleExposure(qhyccd_handle *h)
     @brief stop single exposure
     @param h camera control handle
     @return
     success return QHYCCD_SUCCESS \n
     another QHYCCD_ERROR code on other failures
     */
    virtual int StopSingleExposure(qhyccd_handle *h)
    {
        return QHYCCD_ERROR;
    }
    
    /**
     @fn virtual int GetSingleFrame(qhyccd_handle *h,int *pW,int *pH,int * pBpp,int *pChannels,unsigned char *ImgData)
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
    virtual int GetSingleFrame(qhyccd_handle *h,int *pW,int *pH,int * pBpp,int *pChannels,unsigned char *ImgData)
    {
        return QHYCCD_ERROR;
    }

    /**
     @fn virtual int BeginLiveExposure(qhyccd_handle *h)
     @brief begin live exposure
     @param h camera control handle
     @return
     success return QHYCCD_SUCCESS \n
     another QHYCCD_ERROR code on other failures
     */
    virtual int BeginLiveExposure(qhyccd_handle *h)
    {
        return QHYCCD_ERROR;
    }
    
    /**
     @fn virtual int StopLiveExposure(qhyccd_handle *h)
     @brief stop live exposure
     @param h camera control handle
     @return
     success return QHYCCD_SUCCESS \n
     another QHYCCD_ERROR code on other failures
     */
    virtual int StopLiveExposure(qhyccd_handle *h)
    {
        return QHYCCD_ERROR;
    }
    
    /**
     @fn virtual int GetLiveFrame(qhyccd_handle *h,int *pW,int *pH,int * pBpp,int *pChannels,unsigned char *ImgData)
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
    virtual int GetLiveFrame(qhyccd_handle *h,int *pW,int *pH,int * pBpp,int *pChannels,unsigned char *ImgData)
    {
        return QHYCCD_ERROR;
    }

    /**
     @fn virtual int SetChipUSBTraffic(qhyccd_handle *h,int i)
     @brief set hblank
     @param h camera control handle
     @param i hblank value
     @return
     success return QHYCCD_SUCCESS \n
     another QHYCCD_ERROR code on other failures
     */
    virtual int SetChipUSBTraffic(qhyccd_handle *h,int i)
    {
        return QHYCCD_ERROR;
    }
   
    /**
     @fn virtual int DeChipRowNoise(qhyccd_handle *h,bool value)
     @brief enable the function to reduce the row noise
     @param h camera control handle
     @param value enable or disable
     @return
     success return QHYCCD_SUCCESS \n
     another QHYCCD_ERROR code on other failures
     */
    virtual int DeChipRowNoise(qhyccd_handle *h,bool value)
    {
        return QHYCCD_ERROR;
    }
    
    /**
     @fn virtual int GetChipMemoryLength()
     @brief get the image cost memory length
     @return
     success return memory length \n
     another QHYCCD_ERROR code on other failures
     */
    virtual int GetChipMemoryLength()
    {
        return QHYCCD_ERROR;
    }
 
    /**
     @fn virtual bool IsSupportHighSpeed()
     @brief check the camera support high speed transfer or not
     @return
     support return true \n
     not support return false
     */
    virtual bool IsSupportHighSpeed()
    {
        return false;
    }
    
    /**
     @fn virtual int IsChipHasFunction(CONTROL_ID id)
     @brief check the camera has the function or not
     @param id function id
     @return
     HAVE return QHYCCD_SUCCESS \n
     NOT HAVE return QHYCCD_ERROR_NOTSUPPORT
     */
    virtual int IsChipHasFunction(CONTROL_ID id)
    {
        return QHYCCD_ERROR_NOTSUPPORT;
    }
    
    /**
     @fn virtual int IsColorCam()
     @brief check the camera is color or mono one
     @return
     mono chip return QHYCCD_MONO \n
     color chip return QHYCCD_COLOR
     */
    virtual int IsColorCam()
    {
        return QHYCCD_MONO;
    }
    
    /**
     @fn virtual int IsCoolCam()
     @brief check the camera has cool function or not
     @return
     HAVE return QHYCCD_COOL \n
     NOT HAVE return QHYCCD_NOTCOOL
     */
    virtual int IsCoolCam()
    {
        return QHYCCD_NOTCOOL;
    }
    
    /**
     @fn virtual int SetChipCoolPWM(qhyccd_handle *h,double PWM)
     @brief set cool power
     @param h camera control handle
     @param PWM power(0-255)
     @return
     success return QHYCCD_SUCCESS \n
     another QHYCCD_ERROR code on other failures
     */
    virtual int SetChipCoolPWM(qhyccd_handle *h,double PWM)
    {
        return QHYCCD_ERROR;
    }
    
    /**
     @fn virtual int AutoTempControl(qhyccd_handle *h,double ttemp)
     @brief auto temprature control
     @param h camera control handle
     @param ttemp target temprature(degree Celsius)
     @return
     success return QHYCCD_SUCCESS \n
     another QHYCCD_ERROR code on other failures
     */
    virtual int AutoTempControl(qhyccd_handle *h,double ttemp)
    {
        return QHYCCD_ERROR;
    }
    
    
    /**
     @fn virtual int SetChipSpeed(qhyccd_handle *h,int i)
     @brief set the transfer speed to camera
     @param h camera control handle
     @param i speed level
     @return
     success return QHYCCD_SUCCESS \n
     another QHYCCD_ERROR code on other failures
     */
    virtual int SetChipSpeed(qhyccd_handle *h,int i)
    {
        return QHYCCD_ERROR;
    }
    
    /**
     @fn virtual int SetChipBitsMode(qhyccd_handle *h,int bits)
     @brief set the camera depth bits
     @param h camera control handle
     @param bits depth bits
     @return
     success return QHYCCD_SUCCESS \n
     another QHYCCD_ERROR code on other failures
     */
    virtual int SetChipBitsMode(qhyccd_handle *h,int bits)
    {
        return QHYCCD_ERROR;
    }
    
    /**
     @fn virtual int SetChipChannels(qhyccd_handle *h,int channels)
     @brief set the image channels,it means the image is color one
     @param h camera control handle
     @param channels image channels
     @return
     success return QHYCCD_SUCCESS \n
     another QHYCCD_ERROR code on other failures
     */
    virtual int SetChipChannels(qhyccd_handle *h,int channels)
    {
        return QHYCCD_SUCCESS;
    }
    
    /**
     @fn virtual int SetChipBinMode(qhyccd_handle *h,int wbin,int hbin)
     @brief set the camera image bin mode
     @param h camera control handle
     @param wbin width bin
     @param hbin height bin
     @return
     success return QHYCCD_SUCCESS \n
     another QHYCCD_ERROR code on other failures
     */
    virtual int SetChipBinMode(qhyccd_handle *h,int wbin,int hbin)
    {
        return QHYCCD_ERROR;
    }
 
    /**
     @fn virtual void ConvertDataBIN11(unsigned char * Data,int x, int y, unsigned short PixShift)
     @brief move the pixel raw data to correction position,and bin if need
     @param Data raw image data
     @param x image width
     @param y image height
     @param PixShift this is a way to fix the bad pixel data by the usb transfer
     */
    virtual void ConvertDataBIN11(unsigned char *Data,int x, int y, unsigned short PixShift)
    {
        
    }
    
    /**
     @fn virtual void ConvertDataBIN22(unsigned char * Data,int x, int y, unsigned short PixShift)
     @brief move the pixel raw data to correction position,and bin if need
     @param Data raw image data
     @param x image width
     @param y image height
     @param PixShift this is a way to fix the bad pixel data by the usb transfer
     */
    virtual void ConvertDataBIN22(unsigned char *Data,int x, int y, unsigned short TopSkipPix)
    {
        
    }
    
    /**
     @fn void ConvertDataBIN33(unsigned char * Data,int x, int y, unsigned short PixShift)
     @brief move the pixel raw data to correction position,and bin if need
     @param Data raw image data
     @param x image width
     @param y image height
     @param PixShift this is a way to fix the bad pixel data by the usb transfer
     */
    virtual void ConvertDataBIN33(unsigned char *Data,int x, int y, unsigned short TopSkipPix)
    {
        
    }
    
    /**
     @fn virtual void ConvertDataBIN44(unsigned char * Data,int x, int y, unsigned short PixShift)
     @brief move the pixel raw data to correction position,and bin if need
     @param Data raw image data
     @param x image width
     @param y image height
     @param PixShift this is a way to fix the bad pixel data by the usb transfer
     */
    virtual void ConvertDataBIN44(unsigned char *Data,int x, int y, unsigned short TopSkipPix)
    {
        
    }
    
    /**
     @fn virtual int Send2GuiderPort(qhyccd_handle *h,unsigned char Direction,unsigned short PulseTime)
     @brief send the command to camera's guide port
     @param h camera control handle
     @param Direction RA DEC
     @param PulseTime the time last for command
     @return
     success return QHYCCD_SUCCESS \n
     another QHYCCD_ERROR code on other failures
     */
    virtual int Send2GuiderPort(qhyccd_handle *h,unsigned char Direction,unsigned short PulseTime)
    {
        return QHYCCD_ERROR;
    }
    
    /**
     @fn virtual int Send2CFWPort(qhyccd_handle *h,int pos)
     @brief send the command to camera's color filter wheel port
     @param h camera control handle
     @param pos the color filter position
     @return
     success return QHYCCD_SUCCESS \n
     another QHYCCD_ERROR code on other failures
     */
    virtual int Send2CFWPort(qhyccd_handle *h,int pos)
    {
        return QHYCCD_ERROR;
    }

    /** @fn virtual int GetChipInfo(double *chipw,double *chiph,int *imagew,int *imageh,double *pixelw,double *pixelh)
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
    virtual int GetChipInfo(double *chipw,double *chiph,int *imagew,int *imageh,double *pixelw,double *pixelh,int *bpp)
    {
        return QHYCCD_ERROR;
    }
    

public:
    
    /**
     @fn void ControlCamTemp(qhyccd_handle *h,double MAXPWM)
     @brief control the ccd/cmos temprature
     @param h camera control handle
     @param MAXPWM the max power of cool
     */
    void ControlCamTemp(qhyccd_handle *h,double MAXPWM);

    /** 
     @fn void Bit16To8_Stretch(unsigned char *InputData16,unsigned char \ *OutputData8,int imageX,int imageY,unsigned short B,unsigned short W)
     @brief turn 16bits data into 8bits
     @param InputData16 for 16bits data memory
     @param OutputData8 for 8bits data memory
     @param imageX image width
     @param imageY image height
     @param B for stretch balck
     @param W for stretch white
     */
    void Bit16To8_Stretch(unsigned char *InputData16,unsigned char *OutputData8,int imageX,int imageY,unsigned short B,unsigned short W);

    /**
     @fn void HistInfo(int x,int y,unsigned char *InBuf,unsigned char *OutBuf)
     @brief make the hist info
     @param x image width
     @param y image height
     @param InBuf for the raw image data
     @param OutBuf for 192x130 8bits 3 channels image
     */
    void HistInfo(int x,int y,unsigned char *InBuf,unsigned char *OutBuf);


    
    int camx;        //!< current camera width
    int camy;        //!< current camera height
    int camxbin;     //!< current camera width bin
    int camybin;     //!< current camera height bin
    int cambits;     //!< current camera bits mode
    int camchannels; //!< current camera channels
    int usbtraffic;  //!< current usbtraffic
    int usbspeed;    //!< current usb speed mode
    double camtime;  //!< current cam expose time
    double camgain;  //!< current cam gain
    int camoffset;   //!< current cam offset
    double camred;   //!< current white blance red value
    double camblue;  //!< current white blance blue value
    double camgreen; //!< current white blance green value
    unsigned char *rawarray; //!< raw buffer pointer for usb transfer
    unsigned char *roiarray; //!< roi image buffer
    
    int roixstart;   //!< roi image xstart position
    int roiystart;   //!< roi image ystart position
    int roixsize;    //!< roi image xsize
    int roiysize;    //!< roi image ysize
    IplImage *monoimg; //!< mono image pointer
    IplImage *roiimg;  //!< roi image pointer
    IplImage *colorimg; //!< color image pointer

    double targetTEMP; //!< target ccd/cmos temprature
    double currentTEMP; //!< current ccd/cmos tempratue
    double currentPWM; //!< current cool power
    int nowVoltage;    //!< reserved
    bool flag_timer;   //!< timer flag
    bool flag_timer_2; //!< timer flag 2

    double NowError; //!< the step k difference
    double PrevError; //!< the step k - 2 difference
    double LastError; //!< the step k- 1 difference
    double Proportion; //!< scale factor value
    double Integral; //!< integralcoefficients
    double Derivative; //!< differential coefficient

    bool flagquit; //!< the global var for quit signal

#if 0
    void Bin2x2(unsigned char *ImgData,int w,int h);
    void CorrectGamma(unsigned char *Src,long totalP);
    void BuildGammaTable();
	void SetGamma(double gamma);
	void SetBrightness(int b);
	void SetContrast(double c);
#endif
        
};


#endif
