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

#include "qhy5ii.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <opencv/cv.h>
#include <opencv/highgui.h>

int GainTable[] =
{
    0x004, 0x005, 0x006, 0x007, 0x008, 0x009, 0x00A, 0x00B, 0x00C, 0x00D, 0x00E,
    0x00F, 0x010, 0x011, 0x012, 0x013, 0x014, 0x015, 0x016, 0x017, 0x018, 0x019,
    0x01A, 0x01B, 0x01C, 0x01D, 0x01E, 0x01F, 0x051, 0x052, 0x053, 0x054, 0x055,
    0x056, 0x057, 0x058, 0x059, 0x05A, 0x05B, 0x05C, 0x05D, 0x05E, 0x05F, 0x6CE,
    0x6CF, 0x6D0, 0x6D1, 0x6D2, 0x6D3, 0x6D4, 0x6D5, 0x6D6, 0x6D7, 0x6D8, 0x6D9,
    0x6DA, 0x6DB, 0x6DC, 0x6DD, 0x6DE, 0x6DF, 0x6E0, 0x6E1, 0x6E2, 0x6E3, 0x6E4,
    0x6E5, 0x6E6, 0x6E7, 0x6FC, 0x6FD, 0x6FE, 0x6FF
};


QHY5II::QHY5II()
{
    /* init the tmp buffer for usb transfer */
    rawarray = new unsigned char[1280 * 1024 * 2];
    
    /* data endpoint */
    usbep = 0x82;
    /* the cmos current bits mode */
    cambits = 8;   
    /* flag for long time and short time exp mode */
    expmode = 0;

    /* init camera width */
    camx = 1280;
    
    /* init camera height */
    camy = 1024;

    /* init camera channels */
    camchannels = 1;

    /* init usb traffic */
    usbtraffic = 100;

    /* init usb speed */
    usbspeed = 0;

    /* init exposetime */
    camtime = 20000;

    /* init white blance */
    camgain = 0.1;
    camblue = 0.01;
    camgreen = 0.01;
}

QHY5II::~QHY5II()
{
    if(rawarray)
    {
        delete(rawarray);
        rawarray = NULL;
    }
}

int QHY5II::ConnectCamera(libusb_device *d,qhyccd_handle **h)
{
    int ret;

    ret = openCamera(d,h);

    if(ret)
    {
        return QHYCCD_ERROR_OPENCAM;
    }

    return QHYCCD_SUCCESS;
}

int QHY5II::DisConnectCamera(qhyccd_handle *h)
{
    if(monoimg)
    {
        cvReleaseImage(&monoimg);
        monoimg = NULL;
    }
    closeCamera(h);
    return QHYCCD_SUCCESS;
}

int QHY5II::ReSetParams2cam(qhyccd_handle *h)
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

int QHY5II::InitChipRegs(qhyccd_handle *h)
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

int QHY5II::IsChipHasFunction(CONTROL_ID controlId)
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
       default:
       {
           ret = QHYCCD_ERROR_NOTSUPPORT;
       }
    }
    return ret;
}

int QHY5II::IsColorCam()
{
    return QHYCCD_MONO;
}

int QHY5II::IsCoolCam()
{
    return QHYCCD_NOTCOOL;
}

int QHY5II::GetControlMinMaxStepValue(CONTROL_ID controlId,double *min,double *max,double *step)
{
    int ret = QHYCCD_ERROR_NOTSUPPORT;

    switch(controlId)
    {
        case CONTROL_EXPOSURE:
        {
            *min = 1;
            *max = 1000 * 60 * 60;
            ret = QHYCCD_SUCCESS;
            break;
        }
        case CONTROL_GAIN:
        {
            *min = 0;
            *max = 72;
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
       default:
       {
           ret = QHYCCD_ERROR_NOTSUPPORT;
       }
    }
    return ret;
}

int QHY5II::GetChipMemoryLength()
{
    return 1280 * 1024 * 2;
}

double QHY5II::GetChipExposeTime()
{
    return camtime;
}

double QHY5II::GetChipGain()
{
    return camgain;
}

double QHY5II::GetChipSpeed()
{
    return usbspeed;
}

double QHY5II::GetChipUSBTraffic()
{
    return usbtraffic;
}

int QHY5II::SetChipGain(qhyccd_handle *h,double gain)
{
    int ret = QHYCCD_ERROR;
    
    int i = (int)gain;
    
    I2CTwoWrite(h, 0x35, GainTable[i]);
    
    return QHYCCD_SUCCESS;
}

int QHY5II::SetChipExposeTime(qhyccd_handle *h,double time)
{
    double CMOSCLK;
    unsigned char buf[4];
    double pixelPeriod;
    double A, Q;
    double P1, P2;
    double RowTime; // unit: us
    unsigned long ExpTime; // unit: us
    unsigned short REG04, REG05, REG0C, REG09;
    double MaxShortExpTime;
    
    
    buf[0] = 0;
    buf[1] = 0;
    buf[2] = 0;
    buf[3] = 0;
    vendTXD_Ex(h, 0xc1, 0x00, 0x00, buf, 4);
    usleep(10000);
    I2CTwoWrite(h, 0x09, 0);
    usleep(100000);
    
    if (usbspeed == 0)
        CMOSCLK = 12;
    else if(usbspeed == 1)
        CMOSCLK = 24;
    else
        CMOSCLK = 48;
    
    pixelPeriod = 1 / CMOSCLK; // unit: us
    
    REG04 = I2CTwoRead(h, 0x04);
    REG05 = I2CTwoRead(h, 0x05);
    REG09 = I2CTwoRead(h, 0x09);
    REG0C = I2CTwoRead(h, 0x0C);
    ExpTime = time;
    
    A = REG04 + 1;
    P1 = 242;
    P2 = 2 + REG05 - 19;
    Q = P1 + P2;
    RowTime = (A + Q) * pixelPeriod;
    
    MaxShortExpTime = 15000 * RowTime - 180 * pixelPeriod - 4 * REG0C * pixelPeriod;
    
    if (ExpTime > MaxShortExpTime)
    {
        I2CTwoWrite(h,0x09,15000);
        
        ExpTime = (unsigned long)(ExpTime - MaxShortExpTime);
        
        buf[0] = 0;
        buf[1] = (unsigned char)(((ExpTime / 1000)&~0xff00ffff) >> 16);
        buf[2] = ((ExpTime / 1000)&~0xffff00ff) >> 8;
        buf[3] = ((ExpTime / 1000)&~0xffffff00);
        
        vendTXD_Ex(h, 0xc1, 0x00, 0x00, buf, 4);
        ExpTime = (unsigned long)(ExpTime + MaxShortExpTime);
    }
    else
    {
        buf[0] = 0;
        buf[1] = 0;
        buf[2] = 0;
        buf[3] = 0;
        vendTXD_Ex(h, 0xc1, 0x00, 0x00, buf, 4);
        usleep(1000);
        REG09 = (unsigned short)((ExpTime + 180 * pixelPeriod + 4 * REG0C * pixelPeriod)/ RowTime);
        if (REG09 < 1)
            REG09 = 1;
        I2CTwoWrite(h, 0x09, REG09);
        ExpTime = (unsigned long)(REG09 * RowTime - 180 * pixelPeriod - 4 * REG0C * pixelPeriod);
    }
    return QHYCCD_SUCCESS;
}

int QHY5II::CorrectWH(int *w,int *h)
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

int QHY5II::Init1280x1024(qhyccd_handle *h)
{
    int ret;
    int xstart;
    int ystart;
    
    camx = 1280;
    camy = 960;
    
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

int QHY5II::Init1024x768(qhyccd_handle *h)
{ 
    int xstart;
    int ystart;
    int ret;

    camx = 1024;
    camy = 768;

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

int QHY5II::Init800x600(qhyccd_handle *h)
{
    int xstart;
    int ystart;
    int ret;

    camx = 800;
    camy = 600;

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

int QHY5II::Init640x480(qhyccd_handle *h)
{
    int xstart;
    int ystart;
    int ret;

    camx = 640;
    camy = 480;   

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

int QHY5II::Init320x240(qhyccd_handle *h)
{
    int xstart;
    int ystart;
    int ret;

    camx = 320;
    camy = 240;

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

int QHY5II::SetChipResolution(qhyccd_handle *h,int x,int y)
{
    int ret;
    int xstart;
    int ystart;
    
    if(monoimg)
    {
        cvReleaseImage(&monoimg);
        monoimg = NULL;
    }

    if(colorimg)
    {
        cvReleaseImage(&colorimg);
    }

    if(x == 1280 && y == 960)
    {
        ret = Init1280x1024(h);

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
    
    if(monoimg == NULL)
    {
        monoimg = cvCreateImage(cvSize(roixsize,roiysize),cambits,1);
        monoimg->imageData = (char *)rawarray;
        if(monoimg == NULL)
        {
            monoimg = NULL;
            ret =  QHYCCD_ERROR_RESOLUTION;
            return ret;
        }
    }

    if(colorimg == NULL)
    {
        colorimg = cvCreateImage(cvSize(roixsize,roiysize),cambits,3);
        if(colorimg == NULL)
        {
            colorimg = NULL;
            ret =  QHYCCD_ERROR_RESOLUTION;
            return ret;
        }
    }

    ret = ReSetParams2cam(h);
    
    return ret;
}

int QHY5II::SetChipUSBTraffic(qhyccd_handle *h,int i)
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


int QHY5II::BeginSingleExposure(qhyccd_handle *h)
{
    flagquit = false;
    beginVideo(h);
    return QHYCCD_SUCCESS;
}

int QHY5II::StopSingleExposure(qhyccd_handle *h)
{
    flagquit = true;
    sendForceStop(h);
    return QHYCCD_SUCCESS;  
}

int QHY5II::GetSingleFrame(qhyccd_handle *h,int *pW,int *pH,int * pBpp,int *pChannels,unsigned char *ImgData)
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
        if(*pChannels == 3)
        {
            cvCvtColor(monoimg,colorimg,CV_BayerGR2RGB);
        }

        memcpy(ImgData,colorimg->imageData,colorimg->imageSize);
    }
    return ret;  
}

int QHY5II::BeginLiveExposure(qhyccd_handle *h)
{
    flagquit = false;
    beginVideo(h);
    return QHYCCD_SUCCESS;
}

int QHY5II::StopLiveExposure(qhyccd_handle *h)
{
    flagquit = true;
    sendForceStop(h);
    return QHYCCD_SUCCESS;  
}

int QHY5II::GetLiveFrame(qhyccd_handle *h,int *pW,int *pH,int * pBpp,int *pChannels,unsigned char *ImgData)
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
            if(*pChannels == 3)
            {
                cvCvtColor(monoimg,colorimg,CV_BayerGR2RGB);
                memcpy(ImgData,colorimg->imageData,colorimg->imageSize);
            }
            memcpy(ImgData,monoimg->imageData,monoimg->imageSize);
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

int QHY5II::SetChipSpeed(qhyccd_handle *h,int i)
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

int QHY5II::Send2GuiderPort(qhyccd_handle *h,unsigned char Direction,unsigned short PulseTime)
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

int QHY5II::SetPll(qhyccd_handle *h,unsigned char clk)
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


