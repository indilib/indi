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

#include "ic8300.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <opencv/cv.h>
#include <opencv/highgui.h>


IC8300::IC8300()
{
    /* init the tmp buffer for usb transfer */
    rawarray = new unsigned char[3584*2574*3];
    
    /* data endpoint */
    usbep = 0x82;
    /* the cmos current bits mode */
    cambits = 16;   

    /* init camera width */
    camx = 3584;
    
    /* init camera height */
    camy = 2574;

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

IC8300::~IC8300()
{
    delete(rawarray);
}

int IC8300::ConnectCamera(libusb_device *d,qhyccd_handle **h)
{
    int ret;

    ret = openCamera(d,h);

    if(ret)
    {
        return QHYCCD_ERROR_OPENCAM;
    }
    return QHYCCD_SUCCESS;
}

int IC8300::DisConnectCamera(qhyccd_handle *h)
{
    closeCamera(h);
    return QHYCCD_SUCCESS;
}

int IC8300::ReSetParams2cam(qhyccd_handle *h)
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

int IC8300::InitChipRegs(qhyccd_handle *h)
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
 
    ret = SetChipBinMode(h,camxbin,camybin);
    if(ret != QHYCCD_SUCCESS)
    {
        return ret;
    }
    
    return ret;
}

int IC8300::IsChipHasFunction(CONTROL_ID controlId)
{
    int ret = QHYCCD_ERROR_NOTSUPPORT;

    switch(controlId)
    {
        case CONTROL_OFFSET:
        case CONTROL_EXPOSURE:
        case CONTROL_GAIN:
        case CONTROL_SPEED:
        case CONTROL_CFWPORT:
        case CONTROL_COOLER:
        case CAM_BIN1X1MODE:
        case CAM_BIN2X2MODE:
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

int IC8300::IsColorCam()
{
    return QHYCCD_MONO;
}

int IC8300::IsCoolCam()
{
    return QHYCCD_COOL;
}

int IC8300::GetControlMinMaxStepValue(CONTROL_ID controlId,double *min,double *max,double *step)
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

int IC8300::GetChipMemoryLength()
{
    return 3584 * 2574 * 3;
}

double IC8300::GetChipExposeTime()
{
    return camtime;
}

double IC8300::GetChipGain()
{
    return camgain;
}

double IC8300::GetChipOffset()
{
    return camoffset;
}

double IC8300::GetChipSpeed()
{
    return usbspeed;
}

double IC8300::GetChipBitsMode()
{
    return cambits;
}

double IC8300::GetChipCoolTemp(qhyccd_handle *h)
{
    nowVoltage = 1.024 * (float)getDC201FromInterrupt(h); // mV
    currentTEMP = mVToDegree(nowVoltage);
    
    return currentTEMP;
}

double IC8300::GetChipCoolPWM()
{
    return currentPWM;
}

int IC8300::SetChipGain(qhyccd_handle *h,double gain)
{
    int ret = QHYCCD_ERROR;
    camgain = gain;

    ccdreg.Gain = gain;
    ret = sendRegisterQHYCCDOld(h,ccdreg,psize,&totalp,&patchnumber);
    return ret;
}

int IC8300::SetChipOffset(qhyccd_handle *h,double offset)
{
    int ret = QHYCCD_ERROR;
    camoffset = offset;

    ccdreg.Offset = offset;
    ret = sendRegisterQHYCCDOld(h,ccdreg,psize,&totalp,&patchnumber);
    return ret;
}

int IC8300::SetChipExposeTime(qhyccd_handle *h,double time)
{
    int ret = QHYCCD_ERROR;
    camtime = time / 1000;

    ccdreg.Exptime = time / 1000;    
    ret = sendRegisterQHYCCDOld(h,ccdreg,psize,&totalp,&patchnumber);
    return ret; 
}

int IC8300::CorrectWH(int *w,int *h)
{
    return QHYCCD_SUCCESS;
}

int IC8300::InitBIN11Mode()
{
    ccdreg.HBIN = 1;
    ccdreg.VBIN = 1;
    ccdreg.LineSize = 3584;
    ccdreg.VerticalSize = 2574;
    ccdreg.TopSkipPix = 1150;
    psize= 3584 * 14;
    camxbin = 1;
    camybin = 1;
    camx = 3584;
    camy = 2574;
  
    return QHYCCD_SUCCESS;
}

int IC8300::InitBIN22Mode()
{
    ccdreg.HBIN = 2;
    ccdreg.VBIN = 2;
    ccdreg.LineSize = 1792;
    ccdreg.VerticalSize = 1287;
    ccdreg.TopSkipPix = 1100;
    psize= 3584 * 2;
    camxbin = 2;
    camybin = 2;
    camx = 1792;
    camy = 1287;   
    
    return QHYCCD_SUCCESS;
}

int IC8300::InitBIN44Mode()
{
    ccdreg.HBIN = 2;
    ccdreg.VBIN = 4;
    ccdreg.LineSize = 1792;
    ccdreg.VerticalSize = 644;
    ccdreg.TopSkipPix = 0;
    psize = 1792 * 644;
    camxbin = 4;
    camybin = 4;
    camx = 896;
    camy = 644; 

    return QHYCCD_SUCCESS;
}

int IC8300::SetChipResolution(qhyccd_handle *h,int x,int y)
{
    int ret = QHYCCD_ERROR_NOTSUPPORT;
    
    roixstart = 0;
    roiystart = 0;
    roixsize = camx;
    roiysize = camy;
    
    return ret;
}

int IC8300::BeginSingleExposure(qhyccd_handle *h)
{
    int ret = QHYCCD_ERROR;
    flagquit = false;

    ret = beginVideo(h);

    return ret;
}

int IC8300::StopSingleExposure(qhyccd_handle *h)
{
    flagquit = true;
    sendForceStop(h);
    return QHYCCD_SUCCESS;  
}

int IC8300::GetSingleFrame(qhyccd_handle *h,int *pW,int *pH,int * pBpp,int *pChannels,unsigned char *ImgData)
{
    int ret = QHYCCD_ERROR;

    *pW = camx;
    *pH = camy;
    *pBpp = cambits;
    *pChannels = camchannels;

    ret = readUSB2B(h,rawarray,psize,totalp,&patchnumber); 
    if(ret == LIBUSB_SUCCESS)
    {
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
        ret = QHYCCD_SUCCESS;
    }
    return ret;  
}

int IC8300::BeginLiveExposure(qhyccd_handle *h)
{
    int ret = QHYCCD_ERROR;

    return ret; 
}

int IC8300::StopLiveExposure(qhyccd_handle *h)
{
    int ret = QHYCCD_ERROR;

    return ret; 
}

int IC8300::GetLiveFrame(qhyccd_handle *h,int *pW,int *pH,int * pBpp,int *pChannels,unsigned char *ImgData)
{
    int ret = QHYCCD_ERROR;
    return ret;  
}

int IC8300::SetChipSpeed(qhyccd_handle *h,int i)
{
    usbspeed = i;

    ccdreg.DownloadSpeed = i;

    return QHYCCD_SUCCESS;
}

int IC8300::SetChipBinMode(qhyccd_handle *h,int wbin,int hbin)
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

int IC8300::Send2CFWPort(qhyccd_handle *h,int pos)
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

int IC8300::AutoTempControl(qhyccd_handle *h,double ttemp)
{
    /* target temprature */
    targetTEMP = ttemp;

    ControlCamTemp(h,255.0);

    return QHYCCD_SUCCESS;
}

int IC8300::SetChipCoolPWM(qhyccd_handle *h,double PWM)
{
    int ret = QHYCCD_ERROR;

    currentPWM = PWM;
    ret = setDC201FromInterrupt(h, (unsigned char)PWM, 255);

    return ret;
}

void IC8300::ConvertDataBIN11(unsigned char * Data,int x, int y, unsigned short PixShift)
{
     unsigned char *Buf = NULL;

     SWIFT_MSBLSB(Data,x,y);

     Buf=(unsigned char *)malloc(x*y*2);
     
     memcpy(Buf,Data+PixShift*2,x*y*2);
     memcpy(Data,Buf,x*y*2);
     free(Buf);
}


void IC8300::ConvertDataBIN22(unsigned char * Data,int x, int y, unsigned short PixShift)
{
    unsigned char *Buf = NULL;

    SWIFT_MSBLSB(Data,x,y );

    Buf=(unsigned char *) malloc(x*y*2);
    memcpy(Buf,Data+PixShift*2,x*y*2);
    memcpy(Data,Buf,x*y*2);
    free(Buf);
}


void IC8300::ConvertDataBIN44(unsigned char * Data,int x, int y, unsigned short PixShift)
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

int IC8300::GetChipInfo(double *chipw,double *chiph,int *imagew,int *imageh,double *pixelw,double *pixelh,int *bpp)
{
    *chipw = 17.96;
    *chiph = 13.52;
    *imagew = 3584;
    *imageh = 2574;
    *pixelw = 5.4;
    *pixelh = 5.4;
    *bpp = 16;

    return QHYCCD_SUCCESS;
}



