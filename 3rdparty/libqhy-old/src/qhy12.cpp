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

#include "qhy12.h"
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <opencv/cv.h>
#include <opencv/highgui.h>


QHY12::QHY12()
{
    /* init the tmp buffer for usb transfer */
    rawarray = new unsigned char[3328*4640*3];
    
    /* data endpoint */
    usbep = 0x82;
    /* the cmos current bits mode */
    cambits = 16;   

    /* init camera width */
    camx = 3328;
    
    /* init camera height */
    camy = 4640;

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

QHY12::~QHY12()
{
    delete(rawarray);
}

int QHY12::ConnectCamera(libusb_device *d,qhyccd_handle **h)
{
    int ret;

    ret = openCamera(d,h);

    if(ret)
    {
        return QHYCCD_ERROR_OPENCAM;
    }
    return QHYCCD_SUCCESS;
}

int QHY12::DisConnectCamera(qhyccd_handle *h)
{
    closeCamera(h);
    return QHYCCD_SUCCESS;
}

int QHY12::ReSetParams2cam(qhyccd_handle *h)
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

int QHY12::InitChipRegs(qhyccd_handle *h)
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

int QHY12::IsChipHasFunction(CONTROL_ID controlId)
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

int QHY12::IsColorCam()
{
    return QHYCCD_COLOR;
}

int QHY12::IsCoolCam()
{
    return QHYCCD_COOL;
}

int QHY12::GetControlMinMaxStepValue(CONTROL_ID controlId,double *min,double *max,double *step)
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

int QHY12::GetChipMemoryLength()
{
    return 3328 * 4640 * 3;
}

double QHY12::GetChipExposeTime()
{
    return camtime;
}

double QHY12::GetChipGain()
{
    return camgain;
}

double QHY12::GetChipOffset()
{
    return camoffset;
}

double QHY12::GetChipSpeed()
{
    return usbspeed;
}

double QHY12::GetChipBitsMode()
{
    return cambits;
}

double QHY12::GetChipCoolTemp(qhyccd_handle *h)
{
    nowVoltage = 1.024 * (float)getDC201FromInterrupt(h); // mV
    currentTEMP = mVToDegree(nowVoltage);
    
    return currentTEMP;
}

double QHY12::GetChipCoolPWM()
{
    return currentPWM;
}

int QHY12::SetChipGain(qhyccd_handle *h,double gain)
{
    int ret = QHYCCD_ERROR;
    camgain = gain;

    ccdreg.Gain = gain;
    ret = sendRegisterQHYCCDOld(h,ccdreg,psize,&totalp,&patchnumber);
    return ret;
}

int QHY12::SetChipOffset(qhyccd_handle *h,double offset)
{
    int ret = QHYCCD_ERROR;
    camoffset = offset;

    ccdreg.Offset = offset;
    ret = sendRegisterQHYCCDOld(h,ccdreg,psize,&totalp,&patchnumber);
    return ret;
}

int QHY12::SetChipExposeTime(qhyccd_handle *h,double time)
{
    int ret = QHYCCD_ERROR;
    camtime = time / 1000;

    ccdreg.Exptime = time / 1000;    
    ret = sendRegisterQHYCCDOld(h,ccdreg,psize,&totalp,&patchnumber);
    return ret; 
}

int QHY12::CorrectWH(int *w,int *h)
{
    return QHYCCD_SUCCESS;
}

int QHY12::InitBIN11Mode()
{
    ccdreg.HBIN = 1;
    ccdreg.VBIN = 1;
    ccdreg.LineSize = 3328;
    ccdreg.VerticalSize = 1170 * 4;
    ccdreg.TopSkipPix = 1190;
    psize= 33280;
    camxbin = 1;
    camybin = 1;
    camx = 3328;
    camy = 4640;
  
    return QHYCCD_SUCCESS;
}

int QHY12::InitBIN22Mode()
{
    ccdreg.HBIN = 1;
    ccdreg.VBIN = 2;
    ccdreg.LineSize = 3328;
    ccdreg.VerticalSize = 1170 * 2;
    ccdreg.TopSkipPix = 1190;
    psize= 33280;
    camxbin = 2;
    camybin = 2;
    camx = 1664;
    camy = 2320;
    
    return QHYCCD_SUCCESS;
}

int QHY12::InitBIN44Mode()
{
    ccdreg.HBIN = 1;
    ccdreg.VBIN = 4;
    ccdreg.LineSize = 3328;
    ccdreg.VerticalSize = 1170;
    ccdreg.TopSkipPix = 1190;
    psize = 33280;
    camxbin = 4;
    camybin = 4;
    camx = 832;
    camy = 1160;

    return QHYCCD_SUCCESS;
}

int QHY12::SetChipResolution(qhyccd_handle *h,int x,int y)
{
    int ret = QHYCCD_ERROR_NOTSUPPORT;

    roixstart = 0;
    roiystart = 0;
    roixsize = camx;
    roiysize = camy;
    
    return ret;
}

int QHY12::BeginSingleExposure(qhyccd_handle *h)
{
    int ret = QHYCCD_ERROR;
    flagquit = false;

    ret = beginVideo(h);

    return ret;
}

int QHY12::StopSingleExposure(qhyccd_handle *h)
{
    flagquit = true;
    sendForceStop(h);
    return QHYCCD_SUCCESS;  
}

int QHY12::GetSingleFrame(qhyccd_handle *h,int *pW,int *pH,int * pBpp,int *pChannels,unsigned char *ImgData)
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

int QHY12::BeginLiveExposure(qhyccd_handle *h)
{
    int ret = QHYCCD_ERROR;

    return ret; 
}

int QHY12::StopLiveExposure(qhyccd_handle *h)
{
    int ret = QHYCCD_ERROR;

    return ret; 
}

int QHY12::GetLiveFrame(qhyccd_handle *h,int *pW,int *pH,int * pBpp,int *pChannels,unsigned char *ImgData)
{
    int ret = QHYCCD_ERROR;
    return ret;  
}

int QHY12::SetChipSpeed(qhyccd_handle *h,int i)
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

int QHY12::SetChipBinMode(qhyccd_handle *h,int wbin,int hbin)
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

int QHY12::Send2CFWPort(qhyccd_handle *h,int pos)
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

int QHY12::AutoTempControl(qhyccd_handle *h,double ttemp)
{
    /* target temprature */
    targetTEMP = ttemp;

    ControlCamTemp(h,255.0);

    return QHYCCD_SUCCESS;
}

int QHY12::SetChipCoolPWM(qhyccd_handle *h,double PWM)
{
    int ret = QHYCCD_ERROR;

    currentPWM = PWM;
    ret = setDC201FromInterrupt(h, (unsigned char)PWM, 255);

    return ret;
}

static void convertQHY12_BIN11_4Frame(unsigned char *Data,unsigned int PixShift){
    //3160*4680
    
	long s,p,m,n;
	long i,j;
    
	int H=3328;
    
	unsigned char * Buf = NULL;
    
	Buf=(unsigned char *) malloc(1170*4*H*2);
    
	s=PixShift*2;
	p=0;
    
	for (j=0; j < 1170*2; j++) {
        
		for (i = 0; i < H; i++) {
			Buf[p]         =Data[s+1];     //◊¢“‚£¨Õ¨ ±Ω¯––¡À∏ﬂµÕŒª◊™ªª
			Buf[p+1]       =Data[s+0];
            
			Buf[p+H*2]    =Data[s+3];
			Buf[p+1+H*2]  =Data[s+2];
            
			s=s+4;
			p=p+2;
            
		}
		p=p+H*2;
		
	}
    
	memcpy(Data,Buf,1170*4*H*2);
	free(Buf);
    
}

//*****************************************************************************
//***************************  2*2 binning  ***********************************
//*****************************************************************************

//3160*4680
static void convertQHY12_BIN11_2Frame_BIN2(unsigned char *Data,unsigned int PixShift){
    
	long s,p,m,n;
	long i,j;
    
	static int H=3328;
    
	unsigned char * Buf = NULL;
    
	Buf=(unsigned char *) malloc(1170*2*H*2);
    
	s=PixShift*2;
	p=0;
    
	for (j=0; j < 1170; j++) {
        
		for (i = 0; i < H; i++) {
			Buf[p]         =Data[s+1];    //◊¢“‚£¨Õ¨ ±Ω¯––¡À∏ﬂµÕŒª◊™ªª
			Buf[p+1]       =Data[s+0];
            
			Buf[p+H*2]       =Data[s+3];
			Buf[p+1+H*2]     =Data[s+2];
            
			s=s+4;
			p=p+2;
            
		}
		p=p+H*2;
        
	}
    
	memcpy(Data,Buf,1170*2*H*2);
	free(Buf);
    
}

//*****************************************************************************
//***************************  4*4 binning  ***********************************
//*****************************************************************************

static void convertQHY12_BIN11_2Frame_BIN4(unsigned char *Data,unsigned int PixShift){
    
	static int H=3328;
    
	long s,p,m,n;
	long i,j;
    
	unsigned char * Buf = NULL;
    
	Buf=(unsigned char *) malloc(1170*H*2);
    
	s=PixShift*2;
	p=0;
    
	for (j=0; j < 1170/2; j++) {
        
		for (i = 0; i < H; i++) {
            
			Buf[p]         =Data[s+1];        //◊¢“‚£¨Õ¨ ±Ω¯––¡À∏ﬂµÕŒª◊™ªª
			Buf[p+1]       =Data[s+0];
            
			Buf[p+H*2]    =Data[s+3];
			Buf[p+1+H*2]  =Data[s+2];
            
			s=s+4;
			p=p+2;
            
		}
        
		p=p+H*2;
        
	}
    
	memcpy(Data,Buf,1170*H*2);
	free(Buf);
    
}
//-----------------------------------------------------------------------------

void QHY12::ConvertDataBIN11(unsigned char * Data,int x, int y, unsigned short PixShift)
{
	long s,p,m,n;
	long i,j;
    
	int H=3328;
    
	//cvNamedWindow("R",3);
	//cvNamedWindow("Gr",3);
	//cvNamedWindow("Gb",3);
	//cvNamedWindow("B",3);
	//cvNamedWindow("I",3);
    
	convertQHY12_BIN11_4Frame(Data,PixShift);
    
    IplImage *QHY10Img;
    QHY10Img = cvCreateImage(cvSize(H,4680), IPL_DEPTH_16U, 1 );
    QHY10Img->imageData = (char*)Data;
    
    //cvShowImage("I",QHY10Img);
    
    IplImage *RImg,*GrImg,*GbImg,*BImg;
    RImg          = cvCreateImage(cvSize(H/2,2320), IPL_DEPTH_16U, 1 );
    GrImg         = cvCreateImage(cvSize(H/2,2320), IPL_DEPTH_16U, 1 );
    GbImg         = cvCreateImage(cvSize(H/2,2320), IPL_DEPTH_16U, 1 );
    BImg          = cvCreateImage(cvSize(H/2,2320), IPL_DEPTH_16U, 1 );
    
    cvSetImageROI(QHY10Img,cvRect(1,2340+8,H/2,2320));
    cvCopy(QHY10Img,GbImg,NULL);
    cvResetImageROI(QHY10Img);
    
    cvSetImageROI(QHY10Img,cvRect(0,8,H/2,2320));
    cvCopy(QHY10Img,GrImg,NULL);
    cvResetImageROI(QHY10Img);
    
    cvSetImageROI(QHY10Img,cvRect(H/2,14,H/2,2320));
    cvCopy(QHY10Img,RImg,NULL);
    cvResetImageROI(QHY10Img);
    
    cvSetImageROI(QHY10Img,cvRect(H/2,2340+14,H/2,2320));
    cvCopy(QHY10Img,BImg,NULL);
    cvResetImageROI(QHY10Img);
    
    cvFlip(GrImg,NULL,0);
    cvFlip(GbImg,NULL,0);
    
    //cvShowImage("R",RImg);
    //cvShowImage("Gr",GrImg);
    //cvShowImage("Gb",GbImg);
    //cvShowImage("B",BImg);
    
	for (long k = 0; k < 2320*2*H*2; k++) {
		Data[k] = 0;
	}
	m=0;
	n=0;
    
	for (j = 0; j < 2320; j++) {
        
		m=H/2*4*(j*2);
        
		for (i = 0; i < H/2; i++) {
            Data[m]=RImg->imageData[n];
            Data[m+1]=RImg->imageData[n+1];
            m=m+4;
            n=n+2;
		}
	}
    
	m=0;
	n=0;
	for (j = 0; j < 2320; j++) {
		m=H/2*4*(j*2)+2;
        
		for (i = 0; i < H/2; i++) {
            Data[m]=GbImg->imageData[n];
            Data[m+1]=GbImg->imageData[n+1];
            m=m+4;
            n=n+2;
        }
	}
    
	m=0;
	n=0;
	for (j = 0; j <2320; j++) {
        
		m=H/2*4*(j*2+1);
        
		for (i = 0; i < H/2; i++) {
            Data[m]=GrImg->imageData[n];
            Data[m+1]=GrImg->imageData[n+1];
            m=m+4;
            n=n+2;
		}
	}
    
	m=0;
	n=0;
	for (j = 0; j < 2320; j++) {
        
		m=H/2*4*(j*2+1)+2;
        
		for (i = 0; i < H/2; i++) {
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


void QHY12::ConvertDataBIN22(unsigned char * Data,int x, int y, unsigned short PixShift)
{
	static int H=3328;
    
	convertQHY12_BIN11_2Frame_BIN2(Data,PixShift);
    
	IplImage *Img;
	IplImage *ImgL,*ImgR;
    
	//cvNamedWindow("L",3);
	//cvNamedWindow("R",3);
	//cvNamedWindow("I",3);
    
	Img           = cvCreateImage(cvSize(H,1170*2), IPL_DEPTH_16U, 1 );
	ImgR          = cvCreateImage(cvSize(H/2,2320), IPL_DEPTH_16U, 1 );
	ImgL          = cvCreateImage(cvSize(H/2,2320), IPL_DEPTH_16U, 1 );
    
	Img->imageData=(char*)Data;
    
	//cvShowImage("I",Img);
    
	cvSetImageROI(Img,cvRect(0,14,H/2,2320));
	cvCopy(Img,ImgL,NULL);
	cvResetImageROI(Img);
    
	cvSetImageROI(Img,cvRect(H/2,8,H/2,2320));
	cvCopy(Img,ImgR,NULL);
	cvResetImageROI(Img);
	cvReleaseImage(&Img);
    
	cvFlip(ImgL,NULL,0);
	//cvShowImage("L",ImgL);
	//cvShowImage("R",ImgR);
    
	Img           = cvCreateImage(cvSize(H/2,2320), IPL_DEPTH_16U, 1 );
	Img->imageData=(char*)Data;
    
	cvAdd(ImgL,ImgR,Img,NULL);
    
	cvReleaseImage(&ImgL);
	cvReleaseImage(&ImgR);
	cvReleaseImage(&Img);
    
}

void QHY12::ConvertDataBIN44(unsigned char * Data,int x, int y, unsigned short PixShift)
{
	static int H=3328;
    
	convertQHY12_BIN11_2Frame_BIN4(Data,PixShift);
    
	IplImage *Img;
	IplImage *ImgL,*ImgR;
    
	//cvNamedWindow("L",3);
	//cvNamedWindow("R",3);
	//cvNamedWindow("I",3);
    
	Img           = cvCreateImage(cvSize(H,1170), IPL_DEPTH_16U, 1 );
	ImgR          = cvCreateImage(cvSize(H/2,1160), IPL_DEPTH_16U, 1 );
	ImgL          = cvCreateImage(cvSize(H/2,1160), IPL_DEPTH_16U, 1 );
    
	Img->imageData=(char*)Data;
    
	//cvShowImage("I",Img);
    
	cvSetImageROI(Img,cvRect(0,9,H/2,1160));
	cvCopy(Img,ImgL,NULL);
	cvResetImageROI(Img);
    
	cvSetImageROI(Img,cvRect(H/2,4,H/2,1160));
	cvCopy(Img,ImgR,NULL);
	cvResetImageROI(Img);
    
	cvFlip(ImgL,NULL,0);
	//cvShowImage("R",ImgR);
    
	cvAdd(ImgL,ImgR,ImgL,NULL);
	//cvShowImage("L",ImgL);
    
	long m,n;
	long i,j;
	unsigned long pixel;
	unsigned char p1,p2,p3,p4;
    
	m=0;
	n=0;
    
	for (j = 0; j < 1160; j++) {
        
        for (i = 0; i < H/4; i++) {
            
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

