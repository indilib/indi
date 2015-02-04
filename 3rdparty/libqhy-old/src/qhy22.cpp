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

#include "qhy22.h"
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <opencv/cv.h>
#include <opencv/highgui.h>


QHY22::QHY22()
{
    /* init the tmp buffer for usb transfer */
    rawarray = new unsigned char[3072*2240*3];
    
    /* data endpoint */
    usbep = 0x82;
    /* the cmos current bits mode */
    cambits = 16;   

    /* init camera width */
    camx = 3072;
    
    /* init camera height */
    camy = 2240;

    /* init camera channels */
    camchannels = 1;

    /* init usb speed */
    usbspeed = 1;

    /* init exposetime */
    camtime = 1000;

    /* init gain */
    camgain = 0;

    /* init offset */
    camoffset = 140;

    ccdreg.SKIP_TOP = 0;
    ccdreg.SKIP_BOTTOM = 0;
    ccdreg.AMPVOLTAGE = 1;
    ccdreg.LiveVideo_BeginLine =0;
    ccdreg.AnitInterlace = 1;
    ccdreg.MultiFieldBIN = 0;
    ccdreg.TgateMode = 0;
    ccdreg.ShortExposure = 0;
    ccdreg.VSUB = 0;
    ccdreg.TransferBIT = 0;
    ccdreg.TopSkipNull = 30;
    ccdreg.TopSkipPix = 0;
    ccdreg.MechanicalShutterMode = 0;
    ccdreg.DownloadCloseTEC = 0;
    ccdreg.SDRAM_MAXSIZE = 100;
    ccdreg.ClockADJ = 0x0000;
    ccdreg.ShortExposure = 0;

    /* pid for temprature control */
    Proportion = 0.4;
    Integral = 5;
    Derivative = 0.4;
   
    /* to init  step k-1 and step k-2 value */
    LastError = 0;
    PrevError = 0;
}

QHY22::~QHY22()
{
    delete(rawarray);
}

int QHY22::ConnectCamera(libusb_device *d,qhyccd_handle **h)
{
    int ret;

    ret = openCamera(d,h);

    if(ret)
    {
        return QHYCCD_ERROR_OPENCAM;
    }
    return QHYCCD_SUCCESS;
}

int QHY22::DisConnectCamera(qhyccd_handle *h)
{
    closeCamera(h);
    return QHYCCD_SUCCESS;
}

int QHY22::ReSetParams2cam(qhyccd_handle *h)
{
    int ret = QHYCCD_SUCCESS;

    ret = SetChipSpeed(h,usbspeed);
    if(ret != QHYCCD_SUCCESS)
    {
        return ret;
    }

    ret = SetChipExposeTime(h,camtime);
    if(ret != QHYCCD_SUCCESS)
    {
        return ret;  
    }

    ret = SetChipGain(h,camgain);
    if(ret != QHYCCD_SUCCESS)
    {
        return ret;
    }  

    ret = SetChipOffset(h,camoffset);
    if(ret != QHYCCD_SUCCESS)
    {
        return ret;
    } 

    return ret;
}

int QHY22::InitChipRegs(qhyccd_handle *h)
{

    int ret = QHYCCD_ERROR;
    
    ret = SetChipSpeed(h,usbspeed);
    if(ret != QHYCCD_SUCCESS)
    {
        return ret;
    }

    ret = SetChipExposeTime(h,camtime);
    if(ret != QHYCCD_SUCCESS)
    {
        return ret;  
    }

    ret = SetChipGain(h,camgain);
    if(ret != QHYCCD_SUCCESS)
    {
        return ret;
    }  

    ret = SetChipOffset(h,camoffset);
    if(ret != QHYCCD_SUCCESS)
    {
        return ret;
    } 

    ret = SetChipResolution(h,camx,camy);
    if(ret != QHYCCD_SUCCESS)
    {
        return ret;
    }
    
    ret = SetChipBinMode(h,camxbin,camybin);
    if(ret != QHYCCD_SUCCESS)
    {
        return ret;
    }
    
    return ret;
}

int QHY22::IsChipHasFunction(CONTROL_ID controlId)
{
    int ret = QHYCCD_ERROR_NOTSUPPORT;

    switch(controlId)
    {
        case CONTROL_OFFSET:
        {
            ret = QHYCCD_SUCCESS;
            break;
        }
        case CONTROL_EXPOSURE:
        {
            ret = QHYCCD_SUCCESS;
            break;
        }
        case CONTROL_GAIN:
        {
            ret = QHYCCD_SUCCESS;
            break;
        }
        case CONTROL_SPEED:
        {
            ret = QHYCCD_SUCCESS;
            break;
        }
        case CONTROL_CFWPORT:
        {
            ret = QHYCCD_SUCCESS;
            break;
        }
        case CAM_BIN1X1MODE:
        {
            ret = QHYCCD_SUCCESS;
            break;
        }
        case CAM_BIN2X2MODE:
        {
            ret = QHYCCD_SUCCESS;
            break;
        }
        case CAM_BIN4X4MODE:
        {
            ret = QHYCCD_SUCCESS;
            break;
        }
        default:
        {
           ret = QHYCCD_ERROR_NOTSUPPORT;
        }
    }
    return ret;
}

int QHY22::IsColorCam()
{
    return QHYCCD_MONO;
}

int QHY22::IsCoolCam()
{
    return QHYCCD_COOL;
}

int QHY22::GetControlMinMaxStepValue(CONTROL_ID controlId,double *min,double *max,double *step)
{
    int ret = QHYCCD_ERROR_NOTSUPPORT;

    switch(controlId)
    {
        case CONTROL_OFFSET:
        {
            *min = 0;
            *max = 255;
            *step = 1;
            ret = QHYCCD_SUCCESS;
            break;
        }
        case CONTROL_EXPOSURE:
        {
            *min = 1000;
            *max = 1000 * 60 * 60 * 24;
            *step = 1000;
            ret = QHYCCD_SUCCESS;
            break;
        }
        case CONTROL_GAIN:
        {
            *min = 0;
            *max = 63;
            *step = 1;
            ret = QHYCCD_SUCCESS;
            break;
        }
        case CONTROL_SPEED:
        {
            *min = 0;
            *max = 1;
            *step = 1;
            ret = QHYCCD_SUCCESS;
            break;
       }
       case CONTROL_MANULPWM:
       {
            *min = 0;
            *max = 255;
            *step = 1;
            ret = QHYCCD_SUCCESS;
            break;
       }
       default:
       {
           ret = QHYCCD_ERROR_NOTSUPPORT;
       }
    }
    return ret;
}

int QHY22::GetChipMemoryLength()
{
    return 3072 * 2240 * 3;
}

double QHY22::GetChipExposeTime()
{
    return camtime;
}

double QHY22::GetChipGain()
{
    return camgain;
}

double QHY22::GetChipOffset()
{
    return camoffset;
}

double QHY22::GetChipSpeed()
{
    return usbspeed;
}

double QHY22::GetChipBitsMode()
{
    return cambits;
}

double QHY22::GetChipCoolTemp(qhyccd_handle *h)
{
    nowVoltage = 1.024 * (float)getDC201FromInterrupt(h); // mV
    currentTEMP = mVToDegree(nowVoltage);
    
    return currentTEMP;
}

double QHY22::GetChipCoolPWM()
{
    return currentPWM;
}

int QHY22::SetChipGain(qhyccd_handle *h,double gain)
{
    int ret = QHYCCD_ERROR;
    camgain = gain;

    ccdreg.Gain = gain;
    ret = sendRegisterQHYCCDOld(h,ccdreg,psize,&totalp,&patchnumber);
    return ret;
}

int QHY22::SetChipOffset(qhyccd_handle *h,double offset)
{
    int ret = QHYCCD_ERROR;
    camoffset = offset;

    ccdreg.Offset = offset;
    ret = sendRegisterQHYCCDOld(h,ccdreg,psize,&totalp,&patchnumber);
    return ret;
}

int QHY22::SetChipExposeTime(qhyccd_handle *h,double time)
{
    int ret = QHYCCD_ERROR;
    camtime = time / 1000;

    ccdreg.Exptime = time / 1000;    
    ret = sendRegisterQHYCCDOld(h,ccdreg,psize,&totalp,&patchnumber);
    return ret; 
}

int QHY22::CorrectWH(int *w,int *h)
{
    return QHYCCD_SUCCESS;
}

int QHY22::InitBIN11Mode()
{
    ccdreg.HBIN = 1;
    ccdreg.VBIN = 1;
    ccdreg.LineSize = 3072;
    ccdreg.VerticalSize = 2240;
    ccdreg.TopSkipPix = 1035;
    psize= 3072 * 20;
    camxbin = 1;
    camybin = 1;
    camx = 3072;
    camy = 2240;
  
    return QHYCCD_SUCCESS;
}

int QHY22::InitBIN22Mode()
{
    ccdreg.HBIN = 2;
    ccdreg.VBIN = 2;
    ccdreg.LineSize = 1536;
    ccdreg.VerticalSize = 1120;
    ccdreg.TopSkipPix = 1035;
    psize= 1536 * 10;
    camxbin = 2;
    camybin = 2;
    camx = 1536;
    camy = 1120;
    
    return QHYCCD_SUCCESS;
}

int QHY22::InitBIN44Mode()
{
    ccdreg.HBIN = 2;
    ccdreg.VBIN = 4;
    ccdreg.LineSize = 1536;
    ccdreg.VerticalSize = 560;
    ccdreg.TopSkipPix = 0;
    psize = 1536*10;
    camxbin = 4;
    camybin = 4;
    camx = 768;
    camy = 560;

    return QHYCCD_SUCCESS;
}

int QHY22::SetChipResolution(qhyccd_handle *h,int x,int y)
{
    int ret = QHYCCD_ERROR_NOTSUPPORT;

    roixstart = 0;
    roiystart = 0;
    roixsize = camx;
    roiysize = camy;
    
    return ret;
}

int QHY22::BeginSingleExposure(qhyccd_handle *h)
{
    int ret = QHYCCD_ERROR;
    flagquit = false;

    ret = beginVideo(h);

    return ret;
}

int QHY22::StopSingleExposure(qhyccd_handle *h)
{
    flagquit = true;
    sendForceStop(h);
    return QHYCCD_SUCCESS;  
}

int QHY22::GetSingleFrame(qhyccd_handle *h,int *pW,int *pH,int * pBpp,int *pChannels,unsigned char *ImgData)
{
    int ret = QHYCCD_ERROR;

    *pW = camx;
    *pH = camy;
    *pBpp = cambits;
    *pChannels = camchannels;

    ret = readUSB2B(h,rawarray,psize,totalp,&patchnumber); 

    if(camxbin == 1 && camybin == 1)
    {
	ConvertDataBIN11(rawarray,camx,camy,ccdreg.TopSkipPix);
    }
    else if(camxbin == 2 && camybin == 2)
    {
	ConvertDataBIN22(rawarray,camx,camy,ccdreg.TopSkipPix);
    }
    else if(camxbin == 4 && camybin == 4)
    {
	ConvertDataBIN44(rawarray,camx,camy,ccdreg.TopSkipPix);
    }
    
    memcpy(ImgData,rawarray,camx*camy*cambits*camchannels/8);

    return ret;  
}

int QHY22::BeginLiveExposure(qhyccd_handle *h)
{
    int ret = QHYCCD_ERROR;

    return ret; 
}

int QHY22::StopLiveExposure(qhyccd_handle *h)
{
    int ret = QHYCCD_ERROR;

    return ret; 
}

int QHY22::GetLiveFrame(qhyccd_handle *h,int *pW,int *pH,int * pBpp,int *pChannels,unsigned char *ImgData)
{
    int ret = QHYCCD_ERROR;
    return ret;  
}

int QHY22::SetChipSpeed(qhyccd_handle *h,int i)
{
    int ret = QHYCCD_ERROR;
    
    if(i >= 0 && i <= 1)
    {
        usbspeed = i;

        ccdreg.DownloadSpeed = i;
        
        ret = QHYCCD_SUCCESS;
    }
    return ret;
}

int QHY22::SetChipBinMode(qhyccd_handle *h,int wbin,int hbin)
{
    int ret = QHYCCD_ERROR;
    
    if(wbin == 1 && hbin == 1)
    {
        ret = InitBIN11Mode();
        
        if(ret < 0)
        {
            ret = QHYCCD_ERROR_BINMODE;
            return ret;
        }
        ret = QHYCCD_SUCCESS;
    }
    else if(wbin == 2 && hbin == 2)
    {
        ret = InitBIN22Mode();
        if(ret < 0)
        {
            ret = QHYCCD_ERROR_BINMODE;
            return ret;
        }
        
        ret = QHYCCD_SUCCESS;
    }
    else
    {
        ret = InitBIN44Mode();
        if(ret < 0)
        {
            ret = QHYCCD_ERROR_BINMODE;
            return ret;
        }
        
        ret = QHYCCD_SUCCESS;
    }
    
    fprintf(stdout,"Current bin mode is xbin:%d ybin:%d\n",camxbin,camybin);
    
    
    ret = sendRegisterQHYCCDOld(h,ccdreg,psize,&totalp,&patchnumber);
    
    return ret;
}

int QHY22::Send2CFWPort(qhyccd_handle *h,int pos)
{
    int ret;
    unsigned char buf[2];

    buf[0] = pos;
    ret = vendTXD(h,0xc1,buf,1);
    if(ret == 1)
    {
        return QHYCCD_SUCCESS;
    }
    return QHYCCD_ERROR;
}

int QHY22::AutoTempControl(qhyccd_handle *h,double ttemp)
{
    /* target temprature */
    targetTEMP = ttemp;

    ControlCamTemp(h,255.0);

    return QHYCCD_SUCCESS;
}

int QHY22::SetChipCoolPWM(qhyccd_handle *h,double PWM)
{
    int ret = QHYCCD_ERROR;

    currentPWM = PWM;
    ret = setDC201FromInterrupt(h, (unsigned char)PWM, 255);

    return ret;
}

void QHY22::ConvertDataBIN11(unsigned char * Data,int x, int y, unsigned short PixShift)
{
     unsigned char *Buf = NULL;

     SWIFT_MSBLSB(Data,x,y);

     Buf=(unsigned char *)malloc(x*y*2);
     
     memcpy(Buf,Data+PixShift*2,x*y*2);
     memcpy(Data,Buf,x*y*2);
     free(Buf);
}


void QHY22::ConvertDataBIN22(unsigned char * Data,int x, int y, unsigned short PixShift)
{
    unsigned char *Buf = NULL;

    SWIFT_MSBLSB(Data,x,y );

    Buf=(unsigned char *) malloc(x*y*2);
    memcpy(Buf,Data+PixShift*2,x*y*2);
    memcpy(Data,Buf,x*y*2);
    free(Buf);
}


void QHY22::ConvertDataBIN44(unsigned char * Data,int x, int y, unsigned short PixShift)
{
     unsigned char * Buf = NULL;
     unsigned int pix;

     SWIFT_MSBLSB(Data,x*2 ,y);

     Buf=(unsigned char *) malloc(x*y*2);

     unsigned long k=0;
     unsigned long s=PixShift*2;

     while((int)k < x*y*2)
     {
         pix=(Data[s]+Data[s+1]*256+Data[s+2]+Data[s+3]*256)/2;
         if (pix>65535) pix=65535;

         Buf[k] = LSB(pix);
         Buf[k+1] = MSB(pix);
         s=s+4;
         k=k+2;
     }

    memcpy(Data,Buf,x*y*2);
    free(Buf);
}



