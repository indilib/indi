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

#include "qhy5lii_m.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <opencv/cv.h>
#include <opencv/highgui.h>


QHY5LII_M::QHY5LII_M()
{
    /* init the tmp buffer for usb transfer */
    rawarray = new unsigned char[1280*960*2];
    
    /* data endpoint */
    usbep = 0x82;
    /* the cmos current bits mode */
    cambits = 8;   
    /* flag for long time and short time exp mode */
    expmode = 0;

    /* init camera width */
    camx = 1280;
    
    /* init camera height */
    camy = 960;

    /* init camera channels */
    camchannels = 1;

    /* init usb traffic */
    usbtraffic = 200;

    /* init usb speed */
    usbspeed = 0;

    /* init exposetime */
    camtime = 20000;

    /* init cam gain */
    camgain = 10;
}

QHY5LII_M::~QHY5LII_M()
{
    if(rawarray)
    {
        delete(rawarray);
        rawarray = NULL;
    }
}

int QHY5LII_M::ConnectCamera(libusb_device *d,qhyccd_handle **h)
{
    int ret;

    ret = openCamera(d,h);

    if(ret)
    {
        return QHYCCD_ERROR_OPENCAM;
    }

    return QHYCCD_SUCCESS;
}

int QHY5LII_M::DisConnectCamera(qhyccd_handle *h)
{
    InitCmos(h);
    closeCamera(h);
    return QHYCCD_SUCCESS;
}

int QHY5LII_M::ReSetParams2cam(qhyccd_handle *h)
{
    int ret = QHYCCD_SUCCESS;

    ret = SetChipUSBTraffic(h,usbtraffic);
    if(ret != QHYCCD_SUCCESS)
    {
        return ret;
    }

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
 
    return ret;
}

int QHY5LII_M::InitChipRegs(qhyccd_handle *h)
{
    int ret = QHYCCD_ERROR;
    
    ret = SetChipResolution(h,camx,camy);
    if(ret != QHYCCD_SUCCESS)
    {
        return ret;
    }

    ret = SetChipUSBTraffic(h,usbtraffic);
    if(ret != QHYCCD_SUCCESS)
    {
        return ret;
    }

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
 
    return QHYCCD_SUCCESS;
}

int QHY5LII_M::IsChipHasFunction(CONTROL_ID controlId)
{
    int ret = QHYCCD_ERROR_NOTSUPPORT;

    switch(controlId)
    {
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
       case CONTROL_USBTRAFFIC:
       {
            ret = QHYCCD_SUCCESS;
            break;
       }  
       case CONTROL_TRANSFERBIT:
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

int QHY5LII_M::IsColorCam()
{
    return QHYCCD_MONO;
}

int QHY5LII_M::IsCoolCam()
{
    return QHYCCD_NOTCOOL;
}

int QHY5LII_M::GetControlMinMaxStepValue(CONTROL_ID controlId,double *min,double *max,double *step)
{
    int ret = QHYCCD_ERROR_NOTSUPPORT;

    switch(controlId)
    {
        case CONTROL_EXPOSURE:
        {
            *min = 1;
            *max = 10 * 60 * 1000 * 1000;               
            ret = QHYCCD_SUCCESS;
            break;
        }
        case CONTROL_GAIN:
        {
            *min = 0;
            *max = 100;
            *step = 1;
            ret = QHYCCD_SUCCESS;
            break;
        }
        case CONTROL_SPEED:
        {
            *min = 0;
            *max = 2;
            *step = 1;
            ret = QHYCCD_SUCCESS;
            break;
       }
       case CONTROL_USBTRAFFIC:
       {
            *min = 0;
            *max = 255;
            *step = 1;
            ret = QHYCCD_SUCCESS;
            break;
       }  
       case CONTROL_TRANSFERBIT:
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

int QHY5LII_M::GetChipMemoryLength()
{
    return 1280 * 960 * 2;
}

double QHY5LII_M::GetChipExposeTime()
{
    return camtime;
}

double QHY5LII_M::GetChipGain()
{
    return camgain;
}

double QHY5LII_M::GetChipSpeed()
{
    return usbspeed;
}

double QHY5LII_M::GetChipUSBTraffic()
{
    return usbtraffic;
}

double QHY5LII_M::GetChipBitsMode()
{
    return cambits;
}

int QHY5LII_M::SetChipGain(qhyccd_handle *h,double gain)
{
    int ret = QHYCCD_ERROR;

    camgain = gain;
    
    SetGainMonoQHY5LII(h,gain);

    return QHYCCD_SUCCESS;
}

int QHY5LII_M::SetChipExposeTime(qhyccd_handle *h,double time)
{
    // 需要输入的参数: cmosclk
    double cmosclk;
    double pixelPeriod;
    double RowTime;
    unsigned long ExpTime;
    unsigned short REG300C, REG3012;
    double MaxShortExpTime;
    unsigned char buf[4];
    int ret;

    camtime = time;

    if(usbspeed == 0)
    {
        cmosclk = 12;
    }
    else if(usbspeed == 1)
    {
        cmosclk = 24;
    }
    else
    {
        cmosclk = 48;
    }

    pixelPeriod = 1 / (cmosclk * pllratio); // unit: us

    REG300C = I2CTwoRead(h,0x300C);

    RowTime = REG300C * pixelPeriod;

    MaxShortExpTime = 65000 * RowTime;

    ExpTime = time;

    if (ExpTime > MaxShortExpTime) 
    {
        ret = I2CTwoWrite(h,0x3012,65000);
        if(ret <= 0)
        {
             return QHYCCD_ERROR_SETEXPOSE;
        }

	ExpTime = ExpTime - MaxShortExpTime;

	buf[0] = 0;
	buf[1] = (unsigned char)(((ExpTime / 1000) & ~0xff00ffff) >> 16);
	buf[2] = (unsigned char)(((ExpTime / 1000) & ~0xffff00ff) >> 8);
	buf[3] = (unsigned char)((ExpTime / 1000) & ~0xffffff00);
	ret = vendTXD(h,0xc1,buf, 4);
        if(ret <= 0)
        {
             return QHYCCD_ERROR_SETEXPOSE;
        }

	ExpTime = ExpTime + MaxShortExpTime;
	REG3012 = 65000;
    }
    else 
    {
        buf[0] = 0;
        buf[1] = 0;
        buf[2] = 0;
        buf[3] = 0;
        ret = vendTXD(h,0xc1,buf, 4);
        if(ret <= 0)
        {
             return QHYCCD_ERROR_SETEXPOSE;
        }

        usleep(100);
        REG3012 = (unsigned short)(ExpTime / RowTime);
        if(REG3012 < 1)
            REG3012 = 1;
        ret = I2CTwoWrite(h,0x3012, REG3012);
        if(ret <= 0)
        {
             return QHYCCD_ERROR_SETEXPOSE;
        }
        ExpTime = (unsigned long)(REG3012 * RowTime);
    }
    return QHYCCD_SUCCESS;  
}

int QHY5LII_M::CorrectWH(int *w,int *h)
{
    if(*w <= 320 && *h <= 240)
    {
	*w = 320;
	*h = 240;
    }
    else if(*w <= 640 && *h <= 480)
    {
	*w = 640;
	*h = 480;
    }
    else if(*w <= 800 && *h <= 600)
    {
	*w = 800;
	*h = 600;
    }
    else if(*w <= 1024 && *h <= 768)
    {
	*w = 1024;
	*h = 768;
    }
    else
    {
	*w = 1280;
	*h = 960;
    }
    roixsize = *w;
    roiysize = *h;
    return QHYCCD_SUCCESS;
}

int QHY5LII_M::Init1280x960(qhyccd_handle *h)
{
    int ret;
    int xstart;
    int ystart;
    
    camx = 1280;
    camy = 960;
    InitCmos(h);
    pllratio = SetPll(h,0);
 
    xstart = 4;
    ystart = 4;

    ret = I2CTwoWrite(h,0x3002, ystart); 
    if(ret < 0)
    {
        return ret;    
    }
       
    ret = I2CTwoWrite(h,0x3004, xstart);
    if(ret < 0)
    {
        return ret;    
    }

    ret = I2CTwoWrite(h,0x3006, ystart + camy - 1); // y end
    if(ret < 0)
    {
        return ret;    
    }   
    
    ret = I2CTwoWrite(h,0x3008, xstart + camx - 1); // x end
    if(ret < 0)
    {
        return ret;    
    }

    ret = I2CTwoWrite(h,0x300a, 990); 
    if(ret < 0)
    {
        return ret;    
    }

    ret = I2CTwoWrite(h,0x300c, 1650);
    if(ret < 0)
    {
        return ret;    
    }

    ret = I2CTwoWrite(h,0x301A, 0x10DC); 
    if(ret < 0)
    {
        return ret;    
    }
    return ret;
}

int QHY5LII_M::Init1024x768(qhyccd_handle *h)
{ 
    int xstart;
    int ystart;
    int ret;

    camx = 1024;
    camy = 768;

    InitCmos(h);
    pllratio = SetPll(h,0);

    xstart = 4 + (1280 - 1024) / 2;
    ystart = 4 + (960 - 768) / 2;

    ret = I2CTwoWrite(h,0x3002, ystart); // y start
    if(ret < 0)
    {
        return ret;    
    }

    ret = I2CTwoWrite(h,0x3004, xstart); // x start
    if(ret < 0)
    {
        return ret;    
    }

    ret = I2CTwoWrite(h,0x3006, ystart + camy - 1); // y end
    if(ret < 0)
    {
        return ret;    
    }

    ret = I2CTwoWrite(h,0x3008, xstart + camx - 1); // x end
    if(ret < 0)
    {
        return ret;    
    }

    ret = I2CTwoWrite(h,0x300a, 795); // frame length
    if(ret < 0)
    {
        return ret;    
    }

    ret = I2CTwoWrite(h,0x300c, 1388); // line  length
    if(ret < 0)
    {
        return ret;    
    }

    ret = I2CTwoWrite(h,0x301A, 0x10DC); // RESET_REGISTER
    if(ret < 0)
    {
        return ret;    
    }

    return QHYCCD_SUCCESS;
}

int QHY5LII_M::Init800x600(qhyccd_handle *h)
{
    int xstart;
    int ystart;
    int ret;

    camx = 800;
    camy = 600;

    InitCmos(h);
    pllratio = SetPll(h,2);

    xstart = 4 + (1280 - 800) / 2;
    ystart = 4 + (960 - 600) / 2;

    ret = I2CTwoWrite(h,0x3002, ystart); // y start
    if(ret < 0)
    {
        return ret;    
    }

    ret = I2CTwoWrite(h,0x3004, xstart); // x start
    if(ret < 0)
    {
        return ret;    
    }

    ret = I2CTwoWrite(h,0x3006, ystart + camy - 1); // y end
    if(ret < 0)
    {
        return ret;    
    }

    ret = I2CTwoWrite(h,0x3008, xstart + camx - 1); // x end
    if(ret < 0)
    {
        return ret;    
    }

    ret = I2CTwoWrite(h,0x300a, 626); // frame length
    if(ret < 0)
    {
        return ret;    
    }

    ret = I2CTwoWrite(h,0x300c, 1388); // line  length
    if(ret < 0)
    {
        return ret;    
    }

    ret = I2CTwoWrite(h,0x301A, 0x10DC); // RESET_REGISTER
    if(ret < 0)
    {
        return ret;    
    }

    return QHYCCD_SUCCESS;
}

int QHY5LII_M::Init640x480(qhyccd_handle *h)
{
    int xstart;
    int ystart;
    int ret;

    camx = 640;
    camy = 480;   

    InitCmos(h);
    pllratio = SetPll(h,1);

    xstart = 4 + (1280 - 640) / 2;
    ystart = 4 + (960 - 480) / 2;

    ret = I2CTwoWrite(h,0x3002, ystart); // y start
    if(ret < 0)
    {
        return ret;    
    }

    ret = I2CTwoWrite(h,0x3004, xstart); // x start

    if(ret < 0)
    {
        return ret;    
    }

    ret = I2CTwoWrite(h,0x3006, ystart + camy - 1); // y end
    if(ret < 0)
    {
        return ret;    
    }

    ret = I2CTwoWrite(h,0x3008, xstart + camx - 1); // x end
    if(ret < 0)
    {
        return ret;    
    }

    ret = I2CTwoWrite(h,0x300a, 506); // frame length
    if(ret < 0)
    {
        return ret;    
    }

    ret = I2CTwoWrite(h,0x300c, 1388); // line  length
    if(ret < 0)
    {
        return ret;    
    }

    ret = I2CTwoWrite(h,0x301A, 0x10DC); // RESET_REGISTER 
    if(ret < 0)
    {
        return ret;    
    }
    
    return QHYCCD_SUCCESS;
}

int QHY5LII_M::Init320x240(qhyccd_handle *h)
{
    int xstart;
    int ystart;
    int ret;

    camx = 320;
    camy = 240;

    InitCmos(h);
    pllratio = SetPll(h,1);

    xstart = 4 + (1280 - 320) / 2;
    ystart = 4 + (960 - 320) / 2;

    ret = I2CTwoWrite(h,0x3002, ystart); // y start
    if(ret < 0)
    {
        return ret;    
    }

    ret = I2CTwoWrite(h,0x3004, xstart); // x start
    if(ret < 0)
    {
        return ret;    
    }

    ret = I2CTwoWrite(h,0x3006, ystart + camy - 1); // y end
    if(ret < 0)
    {
        return ret;    
    }

    ret = I2CTwoWrite(h,0x3008, xstart + camx - 1); // x end
    if(ret < 0)
    {
        return ret;    
    }

    ret = I2CTwoWrite(h,0x300a, 266); // frame length
    if(ret < 0)
    {
        return ret;    
    }

    ret = I2CTwoWrite(h,0x300c, 1388); // line  length
    if(ret < 0)
    {
        return ret;    
    }

    ret = I2CTwoWrite(h,0x301A, 0x10DC); // RESET_REGISTER
    if(ret < 0)
    {
        return ret;    
    }

    return QHYCCD_SUCCESS;
}

int QHY5LII_M::SetChipResolution(qhyccd_handle *h,int x,int y)
{
    int ret;
    int xstart;
    int ystart;

    if(x == 1280 && y == 960)
    {
        ret = Init1280x960(h);

        if(ret < 0)
        {
            ret =  QHYCCD_ERROR_RESOLUTION;
            return ret;
        }
    }
    else if(x == 1024 && y == 768)
    {
        ret = Init1024x768(h);
        if(ret < 0)
        {
            ret =  QHYCCD_ERROR_RESOLUTION;
            return ret;
        }
    }
    else if(x == 800 && y == 600)
    {
        ret = Init800x600(h);
        if(ret < 0)
        {
            ret =  QHYCCD_ERROR_RESOLUTION;
            return ret;
        }
    }
    else if(x == 640 && y == 480)
    {
        ret = Init640x480(h);
        if(ret < 0)
        {
            ret =  QHYCCD_ERROR_RESOLUTION;
            return ret;
        }
    }
    else
    {
        ret = Init320x240(h);
        if(ret < 0)
        {
            ret =  QHYCCD_ERROR_RESOLUTION;
            return ret;
        }
    }
        
    camx = x;
    camy = y;
    roixstart = 0;
    roiystart = 0;
    roixsize = camx;
    roiysize = camy;  
    psize= camx * camy;
    totalp = 1;
    
    ret = ReSetParams2cam(h);
    
    return ret;
}

int QHY5LII_M::SetChipUSBTraffic(qhyccd_handle *h,int i)
{
    int ret;

    usbtraffic = i;

    if (camx == 1280)
    {
        ret = I2CTwoWrite(h,0x300c, 1650 + i*50);
    }
    else
    {
	ret = I2CTwoWrite(h, 0x300c, 1388 + i*50);
    }

    if(ret < 0)
    {
        return QHYCCD_ERROR_USBTRAFFIC;
    }
    return QHYCCD_SUCCESS;
}


int QHY5LII_M::BeginSingleExposure(qhyccd_handle *h)
{
    flagquit = false;
    beginVideo(h);
    return QHYCCD_SUCCESS;
}

int QHY5LII_M::StopSingleExposure(qhyccd_handle *h)
{
    flagquit = true;
    sendForceStop(h);
    return QHYCCD_SUCCESS;  
}

int QHY5LII_M::GetSingleFrame(qhyccd_handle *h,int *pW,int *pH,int * pBpp,int *pChannels,unsigned char *ImgData)
{
    int ret = QHYCCD_ERROR;
    int exptime;

    *pW = camx;
    *pH = camy;
    *pBpp = cambits;
    *pChannels = camchannels;

    exptime = camtime / 1000;
    ret = readUSB2BForQHY5IISeries(h,rawarray,psize * totalp,exptime);
    if(ret == QHYCCD_SUCCESS)
    {
        memcpy(ImgData,rawarray,psize * totalp);
    }
    return ret;  
}

int QHY5LII_M::BeginLiveExposure(qhyccd_handle *h)
{
    flagquit = false;
    beginVideo(h);
    return QHYCCD_SUCCESS;
}

int QHY5LII_M::StopLiveExposure(qhyccd_handle *h)
{
    flagquit = true;
    sendForceStop(h);
    return QHYCCD_SUCCESS;  
}

int QHY5LII_M::GetLiveFrame(qhyccd_handle *h,int *pW,int *pH,int * pBpp,int *pChannels,unsigned char *ImgData)
{
    int ret = QHYCCD_ERROR;
    int exptime;
    int retevt;

    try
    {
        *pW = camx;
        *pH = camy;
        *pBpp = cambits;
        *pChannels = camchannels;

        exptime = camtime / 1000;
        ret = readUSB2BForQHY5IISeries(h,rawarray,psize * totalp,exptime);
        if(ret == QHYCCD_SUCCESS)
        {
            memcpy(ImgData,rawarray,psize * totalp);
        }
        else if(ret == QHYCCD_ERROR_EVTUSB)
        {
            /* This is almost the slowest mode */
            retevt = SetChipSpeed(h,0);
            if(retevt != QHYCCD_SUCCESS)
            {
                printf("set speed failure\n");
            }

            retevt = SetChipUSBTraffic(h,125);
            if(retevt != QHYCCD_SUCCESS)
            {
                printf("set traffic failure\n");
            }
 
        }
    }
    catch(...)
    {
        printf("try catch\n");
    }
#if 0
    else if(ret == QHYCCD_ERROR_EVTCMOS)
    {
        /* This because the cmos chip not work,so reInit */
        retevt = InitChipRegs(h);
    }
#endif 
    return ret;  
}

int QHY5LII_M::SetChipSpeed(qhyccd_handle *h,int i)
{
    int ret;
    unsigned char buf[2];
    buf[0] = i;

    ret = vendTXD(h,0xc8,buf,1);  
    if(ret == 1)
    {
        usbspeed = i;
        return QHYCCD_SUCCESS;
    }

    return QHYCCD_ERROR_SETSPEED;
}

int QHY5LII_M::SetChipBitsMode(qhyccd_handle *h,int bits)
{
    unsigned char buf[2];

    if(bits == 8)
    { 
        buf[0] = 0;
        cambits = 8;
    }
    else if(bits == 16)
    {
        cambits = 16;
        buf[0] = 1;
    }
    else
    {
        cambits = 8;
        buf[0] = 0;
    }

    vendTXD_Ex(h, 0xcd, 0, 0, buf, 1);

    return QHYCCD_SUCCESS;  
}

int QHY5LII_M::SetChipBinMode(qhyccd_handle *h,int wbin,int hbin)
{
    return QHYCCD_SUCCESS;
}

int QHY5LII_M::Send2GuiderPort(qhyccd_handle *h,unsigned char Direction,unsigned short PulseTime)
{
    unsigned char Buffer[4];
    unsigned short value = 0;
    unsigned short index = 0;

    switch(Direction)
    {
        case 0:
	{
	    index = 0x80;
            value = 0x01;
	    break;
        }
	case 1:
	{
	    index = 0x40;
	    value = 0x02;
	    break;
        }
	case 2:
	{
	    index = 0x20;
	    value = 0x02;
	    break;
	}
	case 3:
	{
	    index = 0x10;
	    value = 0x01;
	    break;
        }
    }
    vendTXD_Ex(h,0xc0,value,index,Buffer,2);
    usleep(PulseTime*1000);
    vendTXD_Ex(h,0xc0,value,0x0,Buffer,2);
    return QHYCCD_SUCCESS;  
}

int QHY5LII_M::SetPll(qhyccd_handle *h,unsigned char clk)
{
    double i = 0;
    if (clk == 0) 
    {
        I2CTwoWrite(h,0x302A, 14); // DIV           14
	I2CTwoWrite(h,0x302C, 1); // DIV
	I2CTwoWrite(h,0x302E, 3); // DIV
	I2CTwoWrite(h,0x3030, 42); // MULTI          44

	I2CTwoWrite(h,0x3082, 0x0029);
	// OPERATION_MODE_CTRL
        //if (LoneExpMode) 
        //{
	    I2CTwoWrite(h,0x30B0, 0x5330);
	    // DIGITAL_TEST    5370: PLL BYPASS   1370  USE PLL
	    i = 1.0;
	//}
	//else 
        //{
	   // I2CTwoWrite(h,0x30B0, 0x1330);
	   // i = 1.0;
	//}
	// 5330
	I2CTwoWrite(h,0x305e, 0x00ff); // gain
	I2CTwoWrite(h,0x3012, 0x0020);
	// coarse integration time

	I2CTwoWrite(h,0x3064, 0x1802);
	// EMBEDDED_DATA_CTRL

	return i;
    }
    else if (clk == 1) 
    {
        I2CTwoWrite(h,0x302A, 14); // DIV           14
	I2CTwoWrite(h,0x302C, 1); // DIV
	I2CTwoWrite(h,0x302E, 3); // DIV
	I2CTwoWrite(h,0x3030, 65); // MULTI          44

	I2CTwoWrite(h,0x3082, 0x0029);
	// OPERATION_MODE_CTRL

	//if (LoneExpMode) 
        //{
	    I2CTwoWrite(h,0x30B0, 0x5330);
	    // DIGITAL_TEST    5370: PLL BYPASS   1370  USE PLL
	    i = 1.0;
	//}
	//else 
        //{
	//    I2CTwoWrite(h,0x30B0, 0x1330);
	//    i = ((double)65) / 14 / 3;
	//}
	I2CTwoWrite(h,0x305e, 0x00ff); // gain
	I2CTwoWrite(h,0x3012, 0x0020);
	// coarse integration time

	I2CTwoWrite(h,0x3064, 0x1802);
	// EMBEDDED_DATA_CTRL

	return i;
    }
    else if (clk == 2) 
    {
        I2CTwoWrite(h,0x302A, 14); // DIV           14
	I2CTwoWrite(h,0x302C, 1); // DIV
	I2CTwoWrite(h,0x302E, 3); // DIV
	I2CTwoWrite(h,0x3030, 57); // MULTI          44

	I2CTwoWrite(h,0x3082, 0x0029);
	// OPERATION_MODE_CTRL

	//if (LoneExpMode) 
        //{
	    I2CTwoWrite(h,0x30B0, 0x5330);
	    // DIGITAL_TEST    5370: PLL BYPASS   1370  USE PLL
	    i = 1.0;
	//}
	//else 
        //{
	//    I2CTwoWrite(h,0x30B0, 0x1330);
	//    i = ((double)57) / 14 / 3;
	//}
	I2CTwoWrite(h,0x305e, 0x00ff); // gain
        I2CTwoWrite(h,0x3012, 0x0020);
	// coarse integration time

	I2CTwoWrite(h,0x3064, 0x1802);
	// EMBEDDED_DATA_CTRL

        return i;
    }
    return i;
}


void QHY5LII_M::InitCmos(qhyccd_handle *h)
{
	// [720p, 25fps input27Mhz,output50Mhz, ]
	I2CTwoWrite(h,0x301A, 0x0001); // RESET_REGISTER
	I2CTwoWrite(h,0x301A, 0x10D8); // RESET_REGISTER
	usleep(200000);
	/////Linear sequencer
	I2CTwoWrite(h,0x3088, 0x8000); // SEQ_CTRL_PORT
	I2CTwoWrite(h,0x3086, 0x0025); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x5050); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x2D26); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x0828); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x0D17); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x0926); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x0028); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x0526); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0xA728); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x0725); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x8080); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x2925); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x0040); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x2702); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x1616); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x2706); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x1F17); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x3626); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0xA617); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x0326); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0xA417); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x1F28); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x0526); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x2028); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x0425); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x2020); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x2700); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x171D); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x2500); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x2017); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x1028); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x0519); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x1703); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x2706); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x1703); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x1741); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x2660); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x175A); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x2317); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x1122); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x1741); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x2500); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x9027); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x0026); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x1828); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x002E); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x2A28); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x081C); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x1470); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x7003); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x1470); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x7004); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x1470); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x7005); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x1470); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x7009); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x170C); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x0014); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x0020); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x0014); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x0050); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x0314); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x0020); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x0314); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x0050); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x0414); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x0020); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x0414); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x0050); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x0514); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x0020); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x2405); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x1400); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x5001); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x2550); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x502D); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x2608); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x280D); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x1709); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x2600); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x2805); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x26A7); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x2807); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x2580); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x8029); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x2500); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x4027); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x0216); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x1627); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x0620); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x1736); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x26A6); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x1703); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x26A4); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x171F); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x2805); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x2620); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x2804); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x2520); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x2027); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x0017); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x1D25); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x0020); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x1710); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x2805); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x1A17); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x0327); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x0617); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x0317); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x4126); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x6017); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0xAE25); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x0090); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x2700); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x2618); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x2800); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x2E2A); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x2808); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x1D05); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x1470); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x7009); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x1720); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x1400); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x2024); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x1400); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x5002); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x2550); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x502D); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x2608); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x280D); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x1709); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x2600); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x2805); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x26A7); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x2807); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x2580); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x8029); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x2500); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x4027); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x0216); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x1627); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x0617); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x3626); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0xA617); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x0326); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0xA417); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x1F28); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x0526); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x2028); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x0425); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x2020); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x2700); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x171D); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x2500); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x2021); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x1710); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x2805); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x1B17); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x0327); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x0617); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x0317); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x4126); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x6017); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0xAE25); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x0090); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x2700); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x2618); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x2800); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x2E2A); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x2808); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x1E17); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x0A05); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x1470); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x7009); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x1616); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x1616); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x1616); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x1616); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x1616); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x1616); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x1616); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x1616); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x1616); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x1616); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x1616); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x1616); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x1616); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x1616); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x1616); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x1616); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x1400); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x2024); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x1400); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x502B); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x302C); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x2C2C); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x2C00); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x0225); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x5050); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x2D26); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x0828); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x0D17); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x0926); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x0028); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x0526); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0xA728); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x0725); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x8080); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x2917); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x0525); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x0040); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x2702); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x1616); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x2706); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x1736); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x26A6); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x1703); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x26A4); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x171F); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x2805); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x2620); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x2804); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x2520); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x2027); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x0017); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x1E25); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x0020); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x2117); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x1028); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x051B); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x1703); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x2706); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x1703); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x1747); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x2660); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x17AE); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x2500); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x9027); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x0026); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x1828); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x002E); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x2A28); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x081E); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x0831); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x1440); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x4014); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x2020); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x1410); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x1034); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x1400); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x1014); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x0020); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x1400); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x4013); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x1802); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x1470); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x7004); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x1470); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x7003); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x1470); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x7017); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x2002); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x1400); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x2002); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x1400); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x5004); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x1400); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x2004); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x1400); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x5022); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x0314); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x0020); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x0314); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x0050); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x2C2C); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x3086, 0x2C2C); // SEQ_DATA_PORT
	I2CTwoWrite(h,0x309E, 0x018A); // RESERVED_MFR_309E
	I2CTwoWrite(h,0x301A, 0x10D8); // RESET_REGISTER
	I2CTwoWrite(h,0x3082, 0x0029); // OPERATION_MODE_CTRL
	I2CTwoWrite(h,0x301E, 0x00C8); // DATA_PEDESTAL
	I2CTwoWrite(h,0x3EDA, 0x0F03); // RESERVED_MFR_3EDA
	I2CTwoWrite(h,0x3EDE, 0xC007); // RESERVED_MFR_3EDE
	I2CTwoWrite(h,0x3ED8, 0x01EF); // RESERVED_MFR_3ED8
	I2CTwoWrite(h,0x3EE2, 0xA46B); // RESERVED_MFR_3EE2
	I2CTwoWrite(h,0x3EE0, 0x067D); // RESERVED_MFR_3EE0
	I2CTwoWrite(h,0x3EDC, 0x0070); // RESERVED_MFR_3EDC
	I2CTwoWrite(h,0x3044, 0x0404); // DARK_CONTROL
	I2CTwoWrite(h,0x3EE6, 0x4303); // RESERVED_MFR_3EE6
	I2CTwoWrite(h,0x3EE4, 0xD208); // DAC_LD_24_25
	I2CTwoWrite(h,0x3ED6, 0x00BD); // RESERVED_MFR_3ED6
	I2CTwoWrite(h,0x3EE6, 0x8303); // RESERVED_MFR_3EE6
	I2CTwoWrite(h,0x30E4, 0x6372); // RESERVED_MFR_30E4
	I2CTwoWrite(h,0x30E2, 0x7253); // RESERVED_MFR_30E2
	I2CTwoWrite(h,0x30E0, 0x5470); // RESERVED_MFR_30E0
	I2CTwoWrite(h,0x30E6, 0xC4CC); // RESERVED_MFR_30E6
	I2CTwoWrite(h,0x30E8, 0x8050); // RESERVED_MFR_30E8
	usleep(100000);
	I2CTwoWrite(h,0x302A, 14); // DIV           14
	I2CTwoWrite(h,0x302C, 1); // DIV
	I2CTwoWrite(h,0x302E, 3); // DIV
	I2CTwoWrite(h,0x3030, 65); // MULTI          44
	I2CTwoWrite(h,0x3082, 0x0029);
	// OPERATION_MODE_CTRL
	I2CTwoWrite(h,0x30B0, 0x1330);
	I2CTwoWrite(h,0x305e, 0x00ff); // gain
	I2CTwoWrite(h,0x3012, 0x0020);
	// coarse integration time
	I2CTwoWrite(h,0x3064, 0x1802);
}

void QHY5LII_M::SWIFT_MSBLSB(unsigned char *ImgData)
{
    int i = 0;
    unsigned char temp;

    while(i < roixsize * roiysize * 2)
    { 
        temp = ImgData[i + 1];
        ImgData[i + 1] = ImgData[i];
	ImgData[i] = temp << 4;
	i += 2;
    }
}

void QHY5LII_M::SetGainMonoQHY5LII(qhyccd_handle *h,double gain)
{
	// gain input range 0-100  输入范围已经归一化到0-1000

	int Gain_Min, Gain_Max;

	Gain_Min = 0;
	Gain_Max = 398;

	gain = (Gain_Max - Gain_Min) * gain / 100;

	unsigned short REG30B0;

	//if (QCam5LII.CheckBoxQHY5LIILoneExpMode)
	//	REG30B0 = 0X5330;
	//else
		REG30B0 = 0X1330;

	unsigned short baseDGain;

	double C[8] = {10, 8, 5, 4, 2.5, 2, 1.25, 1};
	double S[8] = {0, 0, 0, 0, 0, 0, 0, 0};
	int A[8] = {0, 0, 0, 0, 0, 0, 0, 0};
	int B[8] = {0, 0, 0, 0, 0, 0, 0, 0};

	double Error[8];

        int i;
	for (i = 0; i < 8; i++) 
        {
		S[i] = gain / C[i];
		A[i] = (int)(S[i]);
		B[i] = (int)((S[i] - A[i]) / 0.03125);
		if (A[i] > 7)
			A[i] = 10000; // 限制A的范围在1-3
		if (A[i] == 0)
			A[i] = 10000; // 限制A的范围在1-3
		Error[i] = fabs(((double)(A[i])+(double)(B[i]) * 0.03125) * C[i] - gain);
	}

	double minValue;
	int minValuePosition;

	minValue = Error[0];
	minValuePosition = 0;

	for (i = 0; i < 8; i++) {

		if (minValue > Error[i]) {
			minValue = Error[i];
			minValuePosition = i;
		}
	}
	// Form1->Edit6->Text=Form1->Edit6->Text+"minPosition="+AnsiString(minValuePosition)+"minValue="+minValue;
	int AA, BB, CC;
	double DD;
	double EE;

	AA = A[minValuePosition];
	BB = B[minValuePosition];
	if (minValuePosition == 0) {
		CC = 8;
		DD = 1.25;
		I2CTwoWrite(h,0x30B0, (REG30B0 &~0x0030) + 0x30);
		I2CTwoWrite(h,0x3EE4, 0XD308);
	}
	if (minValuePosition == 1) {
		CC = 8;
		DD = 1;
		I2CTwoWrite(h,0x30B0, (REG30B0 &~0x0030) + 0x30);
		I2CTwoWrite(h,0x3EE4, 0XD208);
	}
	if (minValuePosition == 2) {
		CC = 4;
		DD = 1.25;
		I2CTwoWrite(h,0x30B0, (REG30B0 &~0x0030) + 0x20);
		I2CTwoWrite(h,0x3EE4, 0XD308);
	}
	if (minValuePosition == 3) {
		CC = 4;
		DD = 1;
		I2CTwoWrite(h,0x30B0, (REG30B0 &~0x0030) + 0x20);
		I2CTwoWrite(h,0x3EE4, 0XD208);
	}
	if (minValuePosition == 4) {
		CC = 2;
		DD = 1.25;
		I2CTwoWrite(h,0x30B0, (REG30B0 &~0x0030) + 0x10);
		I2CTwoWrite(h,0x3EE4, 0XD308);
	}
	if (minValuePosition == 5) {
		CC = 2;
		DD = 1;
		I2CTwoWrite(h,0x30B0, (REG30B0 &~0x0030) + 0x10);
		I2CTwoWrite(h,0x3EE4, 0XD208);
	}
	if (minValuePosition == 6) {
		CC = 1;
		DD = 1.25;
		I2CTwoWrite(h,0x30B0, (REG30B0 &~0x0030) + 0x00);
		I2CTwoWrite(h,0x3EE4, 0XD308);
	}
	if (minValuePosition == 7) {
		CC = 1;
		DD = 1;
		I2CTwoWrite(h,0x30B0, (REG30B0 &~0x0030) + 0x00);
		I2CTwoWrite(h,0x3EE4, 0XD208);
	}

	EE = fabs(((double)(AA)+(double)(BB) * 0.03125) * CC * DD - gain);

	baseDGain = BB + AA * 32;
	I2CTwoWrite(h,0x305E, baseDGain);

}

int QHY5LII_M::GetChipInfo(double *chipw,double *chiph,int *imagew,int *imageh,double *pixelw,double *pixelh,int *bpp)
{
    *chipw = 4.8;
    *chiph = 3.6;
    *imagew = 1280;
    *imageh = 960;
    *pixelw = 3.75;
    *pixelh = 3.75;
    *bpp = 16;

    return QHYCCD_SUCCESS;
}


