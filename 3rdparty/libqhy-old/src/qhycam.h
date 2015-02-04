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
 * @file qhycam.h
 * @brief QHYCAM class define
 */

#include <unistd.h>
#include <stdbool.h>
#include <math.h>
#include "qhyccdstruct.h"

#ifndef __QHYCAMDEF_H__
#define __QHYCAMDEF_H__


/**
 * @brief QHYCAM class define
 *
 * include all functions for qhycam
 */
class QHYCAM
{
public:
	QHYCAM()
    {
        usbep = 0x82;
        usbintwep = 0x01;
        usbintrep = 0x81;
    }
	~QHYCAM()
    {
        
    }
    
    /**
     @fn int openCamera(qhyccd_deivce *d,qhyccd_handle **h)
     @brief open the camera,open the device handle
     @param d deivce deivce
     @param h device control handle
     @return
     success return QHYCCD_SUCCESS \n
     another QHYCCD_ERROR code on other failures
     */
    int openCamera(qhyccd_device *d,qhyccd_handle **h);

    /**
     @fn void closeCamera(qhyccd_device *h)
     @brief close the camera,close the deivce handle
     @param h deivce control handle
     */
    void closeCamera(qhyccd_handle *h);
    
    /**
     @fn void sendForceStop(qhyccd_handle *h)
     @brief force stop exposure
     @param h device control handle
     */
    void sendForceStop(qhyccd_handle *handle){};

    /**
     @fn int sendInterrupt(qhyccd_handle *handle,unsigned char length,unsigned char *data)
     @brief send a package to deivce using the interrupt endpoint
     @param handle device control handle
     @param length the package size(unit:byte)
     @param data package buffer
     @return
     success return QHYCCD_SUCCESS \n
     another QHYCCD_ERROR code on other failures
     */
    int sendInterrupt(qhyccd_handle *handle,unsigned char length,unsigned char *data);

    /**
     @fn int vendTXD(qhyccd_handle *dev_handle, uint8_t req, unsigned char *data, uint16_t length)
     @brief send a package to deivce using the vendor request
     @param dev_handle device control handle
     @param req request command
     @param data package buffer
     @param length the package size(unit:byte)
     @return
     success return QHYCCD_SUCCESS \n
     another QHYCCD_ERROR code on other failures
     */
	int vendTXD(qhyccd_handle *dev_handle, uint8_t req, unsigned char *data, uint16_t length);

    /**
     @fn int vendRXD(qhyccd_handle *dev_handle, uint8_t req, unsigned char *data, uint16_t length)
     @brief get a package from deivce using the vendor request
     @param dev_handle device control handle
     @param req request command
     @param data package buffer
     @param length the package size(unit:byte)
     @return
     success return QHYCCD_SUCCESS \n
     another QHYCCD_ERROR code on other failures
     */
	int vendRXD(qhyccd_handle *dev_handle, uint8_t req, unsigned char *data, uint16_t length);
    
    /**
     @fn iTXD(qhyccd_handle *dev_handle,unsigned char *data, int length)
     @brief send a package to deivce using the bulk endpoint
     @param dev_handle device control handle
     @param data package buffer
     @param length the package size(unit:byte)
     @return
     success return QHYCCD_SUCCESS \n
     another QHYCCD_ERROR code on other failures
     */
    int iTXD(qhyccd_handle *dev_handle,unsigned char *data, int length);
    
    /**
     @fn iRXD(qhyccd_handle *dev_handle,unsigned char *data, int length)
     @brief get a package from deivce using the bulk endpoint
     @param dev_handle device control handle
     @param data package buffer
     @param length the package size(unit:byte)
     @return
     success return QHYCCD_SUCCESS \n
     another QHYCCD_ERROR code on other failures
     */
    int iRXD(qhyccd_handle *dev_handle,unsigned char *data, int length);
    
    /**
     @fn int vendTXD_Ex(qhyccd_handle *dev_handle, uint8_t req,uint16_t value,uint16_t index,unsigned char* data, uint16_t length)
     @brief send a package to deivce using the vendor request,extend the index and value interface
     @param dev_handle device control handle
     @param req request command
     @param value value interface
     @param index index interface
     @param data package buffer
     @param length the package size(unit:byte)
     @return
     success return QHYCCD_SUCCESS \n
     another QHYCCD_ERROR code on other failures
     */
    int vendTXD_Ex(qhyccd_handle *dev_handle, uint8_t req,uint16_t value,uint16_t index,unsigned char* data, uint16_t length);
    
    /**
     @fn int vendRXD_Ex(qhyccd_handle *dev_handle, uint8_t req,uint16_t value,uint16_t index,unsigned char* data, uint16_t length)
     @brief get a package from deivce using the vendor request,extend the index and value interface
     @param dev_handle device control handle
     @param req request command
     @param value value interface
     @param index index interface
     @param data package buffer
     @param length the package size(unit:byte)
     @return
     success return QHYCCD_SUCCESS \n
     another QHYCCD_ERROR code on other failures
     */
    int vendRXD_Ex(qhyccd_handle *dev_handle, uint8_t req,uint16_t value,uint16_t index,unsigned char* data, uint16_t length);
    
    /**
     @fn int readUSB2B(qhyccd_handle *dev_handle, unsigned char *data,int p_size, int p_num, int* pos)
     @brief get image packages from deivce using bulk endpoint
     @param dev_handle device control handle
     @param data package buffer
     @param p_size package size
     @param p_num package num
     @param pos reserved
     @return
     success return QHYCCD_SUCCESS \n
     another QHYCCD_ERROR code on other failures
     */
    int readUSB2B(qhyccd_handle *dev_handle, unsigned char *data,int p_size, int p_num, int* pos);
    
    /**
     @fn int readUSB2BForQHY5IISeries(qhyccd_handle *dev_handle, unsigned char *data,int sizetoread,int exptime)
     @brief get image packages from deivce using bulk endpoint,specify for QHY5II series
     @param dev_handle device control handle
     @param data package buffer
     @param sizetoread total size to get
     @param exptime the expose time
     @return
     success return QHYCCD_SUCCESS \n
     another QHYCCD_ERROR code on other failures
     */
    int readUSB2BForQHY5IISeries(qhyccd_handle *dev_handle, unsigned char *data,int sizetoread,int exptime);

    /**
     @fn int readUSB2(qhyccd_handle *dev_handle, unsigned char *data,int p_size, int p_num)
     @brief get image packages from deivce using bulk endpoint
     @param dev_handle device control handle
     @param data package buffer
     @param p_size package size
     @param p_num package num
     @return
     success return QHYCCD_SUCCESS \n
     another QHYCCD_ERROR code on other failures
     */
	int readUSB2(qhyccd_handle *dev_handle, unsigned char *data, int p_size, int p_num);

    /**
     @fn int readUSB2_OnePackage3(qhyccd_handle *dev_handle, unsigned char *data, int length)
     @brief get one package from deivce using bulk endpoint
     @param dev_handle device control handle
     @param data package buffer
     @param length package size
     @return
     success return QHYCCD_SUCCESS \n
     another QHYCCD_ERROR code on other failures
     */
	int readUSB2_OnePackage3(qhyccd_handle *dev_handle, unsigned char *data, int length);

    /**
     @fn int beginVideo(qhyccd_handle *handle)
     @brief send begin exposure signal to deivce
     @param handle device control handle
     @return
     success return QHYCCD_SUCCESS \n
     another QHYCCD_ERROR code on other failures
     */
	int beginVideo(qhyccd_handle *handle);
    
    /**
     @fn int sendRegisterQHYCCDOld(qhyccd_handle *handle,
     CCDREG reg, int P_Size, int *Total_P, int *PatchNumber)
     @brief send register params to deivce and some other info
     @param handle deivce control handle
     @param reg register struct
     @param P_Size the package size
     @param Total_P the total packages
     @param PatchNumber the patch for usb transfer,the total size must multiple 512 bytes.
     @return
     success return QHYCCD_SUCCESS \n
     another QHYCCD_ERROR code on other failures
     */
    int sendRegisterQHYCCDOld(qhyccd_handle *handle,
                  CCDREG reg, int P_Size, int *Total_P, int *PatchNumber);
    
    /**
     @fn int sendRegisterQHYCCDNew(qhyccd_handle *handle,
     CCDREG reg, int P_Size, int *Total_P, int *PatchNumber)
     @brief New interface!!! send register params to deivce and some other info
     @param handle deivce control handle
     @param reg register struct
     @param P_Size the package size
     @param Total_P the total packages
     @param PatchNumber the patch for usb transfer,the total size must multiple 512 bytes.
     @return
     success return QHYCCD_SUCCESS \n
     another QHYCCD_ERROR code on other failures
     */
    int sendRegisterQHYCCDNew(qhyccd_handle *handle,
                  CCDREG reg, int P_Size, int *Total_P, int *PatchNumber);
    
    /**
     @fn int setDC201FromInterrupt(qhyccd_handle *handle,unsigned char PWM,unsigned char FAN)
     @brief control the DC201 using the interrupt endpoint
     @param handle deivce control handle
     @param PWM cool power
     @param FAN switch for camera's fan (NOT SUPPORT NOW)
     @return
     success return QHYCCD_SUCCESS \n
     another QHYCCD_ERROR code on other failures
     */
    int setDC201FromInterrupt(qhyccd_handle *handle,unsigned char PWM,unsigned char FAN);
    
    /**
     @fn signed short getDC201FromInterrupt(qhyccd_handle *handle)
     @brief get the temprature value from DC201 using the interrupt endpoint
     @param handle deivce control handle
     @return
     success return  temprature value \n
     another QHYCCD_ERROR code on other failures
     */
    signed short getDC201FromInterrupt(qhyccd_handle *handle);
    
    /**
     @fn unsigned char getFromInterrupt(qhyccd_handle *handle,unsigned char length,unsigned char *data)
     @brief get some special information from deivce,using interrupt endpoint
     @param handle deivce control handle
     @param length package size
     @param data package buffer
     @return
     success return QHYCCD_SUCCESS \n
     another QHYCCD_ERROR code on other failures
     */
    unsigned char getFromInterrupt(qhyccd_handle *handle,unsigned char length,unsigned char *data);
    
    /**
     @fn double GetCCDTemp(qhyccd_handle *handle)
     @brief get ccd/cmos chip temperature
     @param handle deivce control handle
     @return
     success return temperature \n
     */
    double GetCCDTemp(qhyccd_handle *handle);
    
    /**
     @fn double RToDegree(double R)
     @brief calc,transfer R to degree
     @param R R
     @return
     success return degree
     */
    double RToDegree(double R);
    
    /**
     @fn double mVToDegree(double V)
     @brief calc,transfer mV to degree
     @param V mv
     @return
     success return degree
     */
    double mVToDegree(double V);
    
    /**
     @fn double DegreeTomV(double degree)
     @brief calc,transfer degree to mv
     @param degree degree
     @return
     success return mv
     */
    double DegreeTomV(double degree);

    /**
     @fn double DegreeToR(double degree)
     @brief calc,transfer degree to R
     @param degree degree
     @return
     success return R
     */
    double DegreeToR(double degree);

    /**
     @fn int I2CTwoWrite(qhyccd_handle *handle,uint16_t addr,unsigned short value)
     @brief set param by I2C
     @param handle device control handle
     @param addr inter reg addr
     @param value param value
     @return
     success return QHYCCD_SUCCESS \n
     another QHYCCD_ERROR code on other failures
     */
	int I2CTwoWrite(qhyccd_handle *handle,uint16_t addr,unsigned short value);

    /**
     @fn unsigned short I2CTwoRead(qhyccd_handle *handle,uint16_t addr)
     @brief get param by I2C
     @param handle device control handle
     @param addr inter reg addr
     @return
     success return param value \n
     another QHYCCD_ERROR code on other failures
     */
	unsigned short I2CTwoRead(qhyccd_handle *handle,uint16_t addr);

    /**
     @fn unsigned char MSB(unsigned short i)
     @brief apart the 16 bits value to 8bits(high 8bits)
     @param i 16bits value
     @return
     success return 8bits value(high 8bits) \n
     another QHYCCD_ERROR code on other failures
     */
	unsigned char MSB(unsigned short i);

    /**
     @fn unsigned char LSB(unsigned short i)
     @brief apart the 16 bits value to 8bits(low 8bits)
     @param i 16bits value
     @return
     success return 8bits value(low 8bits) \n
     another QHYCCD_ERROR code on other failures
     */
	unsigned char LSB(unsigned short i);

    /**
     @fn void SWIFT_MSBLSB(unsigned char * Data, int x, int y)
     @brief to switch the every single 16bits pixel data,hign 8bits and low 8bits
     @param Data 16bits image data buffer
     @param x image width
     @param y image height
     @return
     success return QHYCCD_SUCCESS \n
     another QHYCCD_ERROR code on other failures
     */
	void SWIFT_MSBLSB(unsigned char * Data, int x, int y);

    CCDREG ccdreg; //!< ccd registers params

    int usbep;     //!< usb transfer endpoint

    int usbintwep; //!< usb interrupt write endpoint
    
    int usbintrep; //!< usb interrupt read endpoint
    
    int psize;     //!< usb transfer package size at onece

    int totalp;    //!< the total usb transfer packages

    int patchnumber;//!< patch for image transfer packages 

};

#endif
