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

#include "qhy8.h"
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <opencv/cv.h>
#include <opencv/highgui.h>


QHY8::QHY8()
{
    /* init the tmp buffer for usb transfer */
    rawarray = new unsigned char[3328 * 2030 * 3];
    
    /* data endpoint */
    usbep = 0x82;
    /* the cmos current bits mode */
    cambits = 16;   

    /* init camera width */
    camx = 3328;
    
    /* init camera height */
    camy = 2030;

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

QHY8::~QHY8()
{
    delete(rawarray);
}

int QHY8::ConnectCamera(libusb_device *d,qhyccd_handle **h)
{
    int ret;

    ret = openCamera(d,h);

    if(ret)
    {
        return QHYCCD_ERROR_OPENCAM;
    }
    return QHYCCD_SUCCESS;
}

int QHY8::DisConnectCamera(qhyccd_handle *h)
{
    closeCamera(h);
    return QHYCCD_SUCCESS;
}

int QHY8::ReSetParams2cam(qhyccd_handle *h)
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

int QHY8::InitChipRegs(qhyccd_handle *h)
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

int QHY8::IsChipHasFunction(CONTROL_ID controlId)
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

int QHY8::IsColorCam()
{
    return QHYCCD_MONO;
}

int QHY8::IsCoolCam()
{
    return QHYCCD_COOL;
}

int QHY8::GetControlMinMaxStepValue(CONTROL_ID controlId,double *min,double *max,double *step)
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

int QHY8::GetChipMemoryLength()
{
    return 2048 * 1500 * 3;
}

double QHY8::GetChipExposeTime()
{
    return camtime;
}

double QHY8::GetChipGain()
{
    return camgain;
}

double QHY8::GetChipOffset()
{
    return camoffset;
}

double QHY8::GetChipSpeed()
{
    return usbspeed;
}

double QHY8::GetChipBitsMode()
{
    return cambits;
}

double QHY8::GetChipCoolTemp(qhyccd_handle *h)
{
    nowVoltage = 1.024 * (float)getDC201FromInterrupt(h); // mV
    currentTEMP = mVToDegree(nowVoltage);
    
    return currentTEMP;
}

double QHY8::GetChipCoolPWM()
{
    return currentPWM;
}

int QHY8::SetChipGain(qhyccd_handle *h,double gain)
{
    int ret = QHYCCD_ERROR;
    camgain = gain;

    ccdreg.Gain = gain;
    ret = sendRegisterQHYCCDOld(h,ccdreg,psize,&totalp,&patchnumber);
    return ret;
}

int QHY8::SetChipOffset(qhyccd_handle *h,double offset)
{
    int ret = QHYCCD_ERROR;
    camoffset = offset;

    ccdreg.Offset = offset;
    ret = sendRegisterQHYCCDOld(h,ccdreg,psize,&totalp,&patchnumber);
    return ret;
}

int QHY8::SetChipExposeTime(qhyccd_handle *h,double time)
{
    int ret = QHYCCD_ERROR;
    camtime = time / 1000;

    ccdreg.Exptime = time / 1000;    
    ret = sendRegisterQHYCCDOld(h,ccdreg,psize,&totalp,&patchnumber);
    return ret; 
}

int QHY8::CorrectWH(int *w,int *h)
{
    return QHYCCD_SUCCESS;
}

int QHY8::InitBIN11Mode()
{
    ccdreg.HBIN = 1;
    ccdreg.VBIN = 1;
    ccdreg.LineSize = 6656;
    ccdreg.VerticalSize = 1015;
    ccdreg.TopSkipPix = 1500;
    psize= 26624;
    camxbin = 1;
    camybin = 1;
    camx = 3328;
    camy = 2030;
  
    return QHYCCD_SUCCESS;
}

int QHY8::InitBIN22Mode()
{
    ccdreg.HBIN = 2;
    ccdreg.VBIN = 1;
    ccdreg.LineSize = 3328;
    ccdreg.VerticalSize = 1015;
    ccdreg.TopSkipPix = 1250;
    psize= 26624;
    camxbin = 2;
    camybin = 2;
    camx = 1664;
    camy = 1015;
    
    return QHYCCD_SUCCESS;
}

int QHY8::InitBIN44Mode()
{
    ccdreg.HBIN = 2;
    ccdreg.VBIN = 2;
    ccdreg.LineSize = 3328;
    ccdreg.VerticalSize = 507;
    ccdreg.TopSkipPix = 0;
    psize = 3296 * 1024;
    camxbin = 4;
    camybin = 4;
    camx = 832;
    camy = 508;

    return QHYCCD_SUCCESS;
}

int QHY8::SetChipResolution(qhyccd_handle *h,int x,int y)
{
    int ret = QHYCCD_ERROR_NOTSUPPORT;

    roixstart = 0;
    roiystart = 0;
    roixsize = camx;
    roiysize = camy;
    
    return ret;
}

int QHY8::BeginSingleExposure(qhyccd_handle *h)
{
    int ret = QHYCCD_ERROR;
    flagquit = false;

    ret = beginVideo(h);

    return ret;
}

int QHY8::StopSingleExposure(qhyccd_handle *h)
{
    flagquit = true;
    sendForceStop(h);
    return QHYCCD_SUCCESS;  
}

int QHY8::GetSingleFrame(qhyccd_handle *h,int *pW,int *pH,int * pBpp,int *pChannels,unsigned char *ImgData)
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

int QHY8::BeginLiveExposure(qhyccd_handle *h)
{
    int ret = QHYCCD_ERROR;

    return ret; 
}

int QHY8::StopLiveExposure(qhyccd_handle *h)
{
    int ret = QHYCCD_ERROR;

    return ret; 
}

int QHY8::GetLiveFrame(qhyccd_handle *h,int *pW,int *pH,int * pBpp,int *pChannels,unsigned char *ImgData)
{
    int ret = QHYCCD_ERROR;
    return ret;  
}

int QHY8::SetChipSpeed(qhyccd_handle *h,int i)
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

int QHY8::SetChipBinMode(qhyccd_handle *h,int wbin,int hbin)
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

int QHY8::Send2CFWPort(qhyccd_handle *h,int pos)
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

int QHY8::AutoTempControl(qhyccd_handle *h,double ttemp)
{
    /* target temprature */
    targetTEMP = ttemp;

    ControlCamTemp(h,255.0);

    return QHYCCD_SUCCESS;
}

int QHY8::SetChipCoolPWM(qhyccd_handle *h,double PWM)
{
    int ret = QHYCCD_ERROR;

    currentPWM = PWM;
    ret = setDC201FromInterrupt(h, (unsigned char)PWM, 255);

    return ret;
}

void QHY8::ConvertDataBIN11(unsigned char * Data,int x, int y, unsigned short PixShift)
{
	unsigned char * Buf = NULL;
	Buf=(unsigned char *) malloc(x*y*2);
	
	int j=0;
    int k=0;
    
    while(j<y)
	{
	    memcpy(Buf+j*x*2, Data+k*x*2+PixShift*2,x*2);
	    j++;
	    memcpy(Buf+j*x*2, Data+k*x*2+(y/2)*x*2+PixShift*2,x*2);
	    j++;
	    k++;
    }
	memcpy(Data,Buf,x*y*2);
	free(Buf);
	
	SWIFT_MSBLSB(Data,x,y);
}


void QHY8::ConvertDataBIN22(unsigned char * Data,int x, int y, unsigned short PixShift)
{
	unsigned char * Buf = NULL;
    
    Buf=(unsigned char *) malloc(x*y*2);
    
    memcpy(Buf,Data+PixShift*2,x*y*2) ;
    SWIFT_MSBLSB(Buf,x,y);
    memcpy(Data,Buf,x*y*2);
    
    free(Buf);
}


void QHY8::ConvertDataBIN44(unsigned char * Data,int x, int y, unsigned short PixShift)
{
	unsigned char * Buf = NULL;
	unsigned int pixel;
	Buf=(unsigned char *) malloc(x*y*2);
    long s,k;
    
	s=PixShift*2;
    k=0;
    
    while(k < x*y*2)
	{
		pixel=Data[s]*256+Data[s+1]+Data[s+2]*256+Data[s+3];
        if (pixel>65535) pixel=65535;
        Buf[k]=LSB(pixel);
        Buf[k+1]=MSB(pixel);
        s=s+4;
        k=k+2;
	}
    
    memcpy(Data,Buf,x*y*2);
    free(Buf);
}



