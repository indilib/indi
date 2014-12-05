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

#include "qhy10.h"
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <opencv/cv.h>
#include <opencv/highgui.h>


QHY10::QHY10()
{
    /* init the tmp buffer for usb transfer */
    rawarray = new unsigned char[2816*3940*3];
    
    /* data endpoint */
    usbep = 0x82;
    /* the cmos current bits mode */
    cambits = 16;   

    /* init camera width */
    camx = 2816;
    
    /* init camera height */
    camy = 3940;

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
    ccdreg.TopSkipNull = 100;
    ccdreg.TopSkipPix = 0;
    ccdreg.MechanicalShutterMode = 0;
    ccdreg.DownloadCloseTEC = 0;
    ccdreg.SDRAM_MAXSIZE = 100;
    ccdreg.ClockADJ = 0x0000;
    ccdreg.ShortExposure = 0;

    /* pid for temprature control */
    Proportion = 0.9;
    Integral = 12;
    Derivative = 0.2;
   
    /* to init  step k-1 and step k-2 value */
    LastError = 0;
    PrevError = 0;
}

QHY10::~QHY10()
{
    delete(rawarray);
}

int QHY10::ConnectCamera(libusb_device *d,qhyccd_handle **h)
{
    int ret;

    ret = openCamera(d,h);

    if(ret)
    {
        return QHYCCD_ERROR_OPENCAM;
    }
    return QHYCCD_SUCCESS;
}

int QHY10::DisConnectCamera(qhyccd_handle *h)
{
    closeCamera(h);
    return QHYCCD_SUCCESS;
}

int QHY10::ReSetParams2cam(qhyccd_handle *h)
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

int QHY10::InitChipRegs(qhyccd_handle *h)
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

int QHY10::IsChipHasFunction(CONTROL_ID controlId)
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

int QHY10::IsColorCam()
{
    return QHYCCD_COLOR;
}

int QHY10::IsCoolCam()
{
    return QHYCCD_COOL;
}

int QHY10::GetControlMinMaxStepValue(CONTROL_ID controlId,double *min,double *max,double *step)
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

int QHY10::GetChipMemoryLength()
{
    return 2816 * 3940 * 3;
}

double QHY10::GetChipExposeTime()
{
    return camtime;
}

double QHY10::GetChipGain()
{
    return camgain;
}

double QHY10::GetChipOffset()
{
    return camoffset;
}

double QHY10::GetChipSpeed()
{
    return usbspeed;
}

double QHY10::GetChipBitsMode()
{
    return cambits;
}

double QHY10::GetChipCoolTemp(qhyccd_handle *h)
{
    nowVoltage = 1.024 * (float)getDC201FromInterrupt(h); // mV
    currentTEMP = mVToDegree(nowVoltage);
    
    return currentTEMP;
}

double QHY10::GetChipCoolPWM()
{
    return currentPWM;
}

int QHY10::SetChipGain(qhyccd_handle *h,double gain)
{
    int ret = QHYCCD_ERROR;
    camgain = gain;

    ccdreg.Gain = gain;
    ret = sendRegisterQHYCCDOld(h,ccdreg,psize,&totalp,&patchnumber);
    return ret;
}

int QHY10::SetChipOffset(qhyccd_handle *h,double offset)
{
    int ret = QHYCCD_ERROR;
    camoffset = offset;

    ccdreg.Offset = offset;
    ret = sendRegisterQHYCCDOld(h,ccdreg,psize,&totalp,&patchnumber);
    return ret;
}

int QHY10::SetChipExposeTime(qhyccd_handle *h,double time)
{
    int ret = QHYCCD_ERROR;
    camtime = time / 1000;

    ccdreg.Exptime = time / 1000;    
    ret = sendRegisterQHYCCDOld(h,ccdreg,psize,&totalp,&patchnumber);
    return ret; 
}

int QHY10::CorrectWH(int *w,int *h)
{
    return QHYCCD_SUCCESS;
}

int QHY10::InitBIN11Mode()
{
    ccdreg.HBIN = 1;
    ccdreg.VBIN = 1;
    ccdreg.LineSize = 2816;
    ccdreg.VerticalSize = 3964;
    ccdreg.TopSkipPix = 1190;
    psize= 28160;
    camxbin = 1;
    camybin = 1;
    camx = 2816;
    camy = 3940;
  
    return QHYCCD_SUCCESS;
}

int QHY10::InitBIN22Mode()
{
    ccdreg.HBIN = 1;
    ccdreg.VBIN = 2;
    ccdreg.LineSize = 2816;
    ccdreg.VerticalSize = 1982;
    ccdreg.TopSkipPix = 1190;
    psize= 28160;
    camxbin = 2;
    camybin = 2;
    camx = 1408;
    camy = 1970;
    
    return QHYCCD_SUCCESS;
}

int QHY10::InitBIN44Mode()
{
    ccdreg.HBIN = 1;
    ccdreg.VBIN = 4;
    ccdreg.LineSize = 2816;
    ccdreg.VerticalSize = 992;
    ccdreg.TopSkipPix = 1190;
    psize = 28160;
    camxbin = 4;
    camybin = 4;
    camx = 704;
    camy = 985;

    return QHYCCD_SUCCESS;
}

int QHY10::SetChipResolution(qhyccd_handle *h,int x,int y)
{
    int ret = QHYCCD_ERROR_NOTSUPPORT;

    roixstart = 0;
    roiystart = 0;
    roixsize = camx;
    roiysize = camy;
    
    return ret;
}

int QHY10::BeginSingleExposure(qhyccd_handle *h)
{
    int ret = QHYCCD_ERROR;
    flagquit = false;

    ret = beginVideo(h);

    return ret;
}

int QHY10::StopSingleExposure(qhyccd_handle *h)
{
    flagquit = true;
    sendForceStop(h);
    return QHYCCD_SUCCESS;  
}

int QHY10::GetSingleFrame(qhyccd_handle *h,int *pW,int *pH,int * pBpp,int *pChannels,unsigned char *ImgData)
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

int QHY10::BeginLiveExposure(qhyccd_handle *h)
{
    int ret = QHYCCD_ERROR;

    return ret; 
}

int QHY10::StopLiveExposure(qhyccd_handle *h)
{
    int ret = QHYCCD_ERROR;

    return ret; 
}

int QHY10::GetLiveFrame(qhyccd_handle *h,int *pW,int *pH,int * pBpp,int *pChannels,unsigned char *ImgData)
{
    int ret = QHYCCD_ERROR;
    return ret;  
}

int QHY10::SetChipSpeed(qhyccd_handle *h,int i)
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

int QHY10::SetChipBinMode(qhyccd_handle *h,int wbin,int hbin)
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

int QHY10::Send2CFWPort(qhyccd_handle *h,int pos)
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

int QHY10::AutoTempControl(qhyccd_handle *h,double ttemp)
{
    /* target temprature */
    targetTEMP = ttemp;

    ControlCamTemp(h,255.0);

    return QHYCCD_SUCCESS;
}

int QHY10::SetChipCoolPWM(qhyccd_handle *h,double PWM)
{
    int ret = QHYCCD_ERROR;

    currentPWM = PWM;
    ret = setDC201FromInterrupt(h, (unsigned char)PWM, 255);

    return ret;
}

static void convertQHY10_BIN11_4Frame(unsigned char *Data,unsigned int PixShift){
    
    
    long s,p;
    long i,j;
    
    int H=2816;
    
    unsigned char * Buf = NULL;
    
    Buf=(unsigned char *) malloc(991*4*H*2);
    
    s=PixShift*2;
    p=0;
    
    
    for (j=0; j < 991*2; j++) {
        
        for (i = 0; i < H; i++) {
            
            
            
            Buf[p+2]         =Data[s+3];     //◊¢“‚£¨Õ¨ ±Ω¯––¡À∏ﬂµÕŒª◊™ªª
            Buf[p+1+2]       =Data[s+2];
            
            Buf[p+H*2]    =Data[s+1];
            Buf[p+1+H*2]  =Data[s+0];
            
            s=s+4;
            p=p+2;
            
        }
        
        p=p+H*2;
        
        
    }
    
    memcpy(Data,Buf,991*4*H*2);
    free(Buf);
    
}

void QHY10::ConvertDataBIN11(unsigned char * Data,int x, int y, unsigned short PixShift)
{
	long m,n;
	long i,j;
    
	int H=2816;
    
	convertQHY10_BIN11_4Frame(Data,PixShift);
    
    
	IplImage *QHY10Img;
	QHY10Img          = cvCreateImage(cvSize(H,3964), IPL_DEPTH_16U, 1 );
	QHY10Img->imageData = (char *)Data;
    
    
	IplImage *RImg,*GrImg,*GbImg,*BImg;
	RImg          = cvCreateImage(cvSize(H/2,1970), IPL_DEPTH_16U, 1 );
	GrImg         = cvCreateImage(cvSize(H/2,1970), IPL_DEPTH_16U, 1 );
	GbImg         = cvCreateImage(cvSize(H/2,1970), IPL_DEPTH_16U, 1 );
	BImg          = cvCreateImage(cvSize(H/2,1970), IPL_DEPTH_16U, 1 );
    
    
	cvSetImageROI(QHY10Img,cvRect(0,12,H/2,1970));
	cvCopy(QHY10Img,GrImg,NULL);
	cvResetImageROI(QHY10Img);
    
	cvSetImageROI(QHY10Img,cvRect(H/2,4,H/2,1970));
	cvCopy(QHY10Img,RImg,NULL);
	cvResetImageROI(QHY10Img);
    
	cvSetImageROI(QHY10Img,cvRect(1,1994,H/2,1970));
	cvCopy(QHY10Img,GbImg,NULL);
	cvResetImageROI(QHY10Img);
    
	cvSetImageROI(QHY10Img,cvRect(H/2,1986,H/2,1970));
	cvCopy(QHY10Img,BImg,NULL);
	cvResetImageROI(QHY10Img);
    
	cvFlip(GrImg,NULL,0);
	cvFlip(GbImg,NULL,0);
    
    
	for (long k = 0; k < 3964*H*2; k++) 	Data[k]=0;
    
	m=0;
	n=0;
    
	for (j = 0; j < 1970; j++)
	{
		m=H/2*4*(j*2);
        
		for (i = 0; i < H/2; i++)
		{
			Data[m]=RImg->imageData[n];
			Data[m+1]=RImg->imageData[n+1];
			m=m+4;
			n=n+2;
		}
	}
    
    
	m=0;
	n=0;
    
    
	for (j = 0; j < 1970; j++)
	{
		m=H/2*4*(j*2)+2;
        
		for (i = 0; i < H/2; i++)
		{
			Data[m]=GbImg->imageData[n];
			Data[m+1]=GbImg->imageData[n+1];
			m=m+4;
			n=n+2;
		}
	}
    
    
    
	m=0;
	n=0;
    
	for (j = 0; j <1970; j++)
	{
		m=H/2*4*(j*2+1);
        
		for (i = 0; i < H/2; i++)
		{
			Data[m]=GrImg->imageData[n];
			Data[m+1]=GrImg->imageData[n+1];
			m=m+4;
			n=n+2;
		}
	}
    
    
    
	m=0;
	n=0;
    
	for (j = 0; j < 1970; j++)
	{
		m=H/2*4*(j*2+1)+2;
        
		for (i = 0; i < H/2; i++)
		{
			Data[m]=BImg->imageData[n];
			Data[m+1]=BImg->imageData[n+1];
			m=m+4;
			n=n+2;
		}
	}
    
	cvReleaseImage(&RImg);
	cvReleaseImage(&GrImg);
	cvReleaseImage(&GbImg);
	cvReleaseImage(&BImg);
    
	cvReleaseImage(&QHY10Img);
}


static void convertQHY10_BIN11_2Frame_BIN2(unsigned char *Data,unsigned int PixShift)
{
    
	long s,p;//,m,n;
	long i,j;
    
	static int H=2816;
    
	unsigned char * Buf = NULL;
    
	Buf=(unsigned char *) malloc(991*2*H*2);
    
	s=PixShift*2;
	p=0;
    
    
    
	for (j=0; j < 991; j++) {
        
        for (i = 0; i < H; i++) {
            
            
            
            Buf[p+2]         =Data[s+3];    //◊¢“‚£¨Õ¨ ±Ω¯––¡À∏ﬂµÕŒª◊™ªª
            Buf[p+1+2]       =Data[s+2];
            
            Buf[p+H*2]       =Data[s+1];
            Buf[p+1+H*2]     =Data[s+0];
            
            s=s+4;
            p=p+2;
            
        }
        
        p=p+H*2;
        
        
	}
    
    
	memcpy(Data,Buf,991*2*H*2);
	free(Buf);
    
}

void QHY10::ConvertDataBIN22(unsigned char * Data,int x, int y, unsigned short PixShift)
{
	static int H=2816;
    
	convertQHY10_BIN11_2Frame_BIN2(Data,PixShift);
    
	IplImage *Img;
	IplImage *ImgL,*ImgR;
    
	Img           = cvCreateImage(cvSize(H,991*2), IPL_DEPTH_16U, 1 );
	ImgR          = cvCreateImage(cvSize(H/2,1970), IPL_DEPTH_16U, 1 );
	ImgL          = cvCreateImage(cvSize(H/2,1970), IPL_DEPTH_16U, 1 );
    
	Img->imageData=(char *)Data;
    
	cvSetImageROI(Img,cvRect(0,12,H/2,1970));
	cvCopy(Img,ImgL,NULL);
	cvResetImageROI(Img);
    
	cvSetImageROI(Img,cvRect(H/2,4,H/2,1970));
	cvCopy(Img,ImgR,NULL);
	cvResetImageROI(Img);
    
	cvReleaseImage(&Img);
    
    
	cvFlip(ImgL,NULL,0);
    
	Img           = cvCreateImage(cvSize(H/2,1970), IPL_DEPTH_16U, 1 );
	Img->imageData=(char *)Data;
    
	cvAdd(ImgL,ImgR,Img,NULL);
    
	cvReleaseImage(&ImgL);
	cvReleaseImage(&ImgR);
	cvReleaseImage(&Img);
}

static void convertQHY10_BIN11_2Frame_BIN4(unsigned char *Data,unsigned int PixShift)
{
    
    
    static int H=2816;
    
    long s,p;//,m,n;
    long i,j;
    
    unsigned char * Buf = NULL;
    
    Buf=(unsigned char *) malloc(991*H*2);
    
    s=PixShift*2;
    p=0;
    
    
    
    for (j=0; j < 991/2; j++) {
        
        for (i = 0; i < H; i++) {
            
            
            
            Buf[p+2]         =Data[s+3];        //◊¢“‚£¨Õ¨ ±Ω¯––¡À∏ﬂµÕŒª◊™ªª
            Buf[p+1+2]       =Data[s+2];
            
            Buf[p+H*2]    =Data[s+1];
            Buf[p+1+H*2]  =Data[s+0];
            
            s=s+4;
            p=p+2;
            
        }
        
        p=p+H*2;
        
        
    }
    
    memcpy(Data,Buf,991*H*2);
    free(Buf);
    
    
    
}

void QHY10::ConvertDataBIN44(unsigned char * Data,int x, int y, unsigned short PixShift)
{
	static int H=2816;
    
	convertQHY10_BIN11_2Frame_BIN4(Data,PixShift);
    
	IplImage *Img;
	IplImage *ImgL,*ImgR;
    
	Img           = cvCreateImage(cvSize(H,992), IPL_DEPTH_16U, 1 );
	ImgR          = cvCreateImage(cvSize(H/2,985), IPL_DEPTH_16U, 1 );
	ImgL          = cvCreateImage(cvSize(H/2,985), IPL_DEPTH_16U, 1 );
    
	Img->imageData=(char *)Data;
    
	cvSetImageROI(Img,cvRect(0,6,H/2,985));
	cvCopy(Img,ImgL,NULL);
	cvResetImageROI(Img);
    
	cvSetImageROI(Img,cvRect(H/2,4,H/2,985));
	cvCopy(Img,ImgR,NULL);
	cvResetImageROI(Img);
    
	cvFlip(ImgL,NULL,0);
    
	cvAdd(ImgL,ImgR,ImgL,NULL);
    
	long m,n;
	long i,j;
	unsigned long pixel;
	unsigned char p1,p2,p3,p4;
    
	m=0;
	n=0;
    
    
	for (j = 0; j < 985; j++) 
	{
		for (i = 0; i < H/4; i++) 
		{
			p1=ImgL->imageData[n];
			p2=ImgL->imageData[n+1];
			p3=ImgL->imageData[n+2];
			p4=ImgL->imageData[n+3];
            
			pixel=p1+(long)p2*256+p3+(long)p4*256;//p3+p4;
            
			if (pixel>65535)  pixel=65535;
            
            
			Data[m]=LSB(pixel);
			Data[m+1]=MSB(pixel);
            
			m=m+2;
			n=n+4;
		}
	}
    
	cvReleaseImage(&ImgL);
	cvReleaseImage(&ImgR);
	cvReleaseImage(&Img);
}


