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

#include "qhycam.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "qhyccderr.h"


int QHYCAM::openCamera(qhyccd_device *d,qhyccd_handle **h)
{
    int ret;

    if(d)
    {
        ret = libusb_open(d,h);
        libusb_kernel_driver_active(*h,0);
        libusb_claim_interface(*h,0);
    }
    return ret;
}

void QHYCAM::closeCamera(qhyccd_handle *h)
{
    if(h)
    {
        libusb_close(h);
    }
}

int QHYCAM::sendRegisterQHYCCDOld(qhyccd_handle *handle, 
                  CCDREG reg, int P_Size, int *Total_P, int *PatchNumber)
{
        int ret = QHYCCD_ERROR;
        unsigned long T;  //total actual transfer data  (byte)
        unsigned char REG[64];
        unsigned char time_H,time_M,time_L;

        if(P_Size <= 0)
        {
            P_Size = 1024;
        }

        T = reg.LineSize * reg.VerticalSize * 2 + reg.TopSkipPix * 2;
 
        if (T % P_Size) {
                *Total_P = T / P_Size+1;
                *PatchNumber = *Total_P * P_Size - T;
        } else {
                *Total_P = T / P_Size;
                *PatchNumber = 0;
        }

        time_L=(reg.Exptime % 256);
        time_M=(reg.Exptime-time_L)/256;
        time_H=(reg.Exptime-time_L-time_M*256)/65536;

        REG[0]=reg.Gain ;
        
        REG[1]=reg.Offset ;
        
        REG[2]=time_H;
        REG[3]=time_M;
        REG[4]=time_L;
        
        REG[5]=reg.HBIN ;
        REG[6]=reg.VBIN ;
        
        REG[7]=MSB(reg.LineSize );
        REG[8]=LSB(reg.LineSize );
        
        REG[9]= MSB(reg.VerticalSize );
        REG[10]=LSB(reg.VerticalSize );
        
        REG[11]=MSB(reg.SKIP_TOP );
        REG[12]=LSB(reg.SKIP_TOP );
        
        REG[13]=MSB(reg.SKIP_BOTTOM );
        REG[14]=LSB(reg.SKIP_BOTTOM );
        
        REG[15]=MSB(reg.LiveVideo_BeginLine );
        REG[16]=LSB(reg.LiveVideo_BeginLine );
        
        REG[19]=MSB(reg.AnitInterlace );
        REG[20]=LSB(reg.AnitInterlace );
        
        REG[22]=reg.MultiFieldBIN ;
        
        REG[29]=MSB(reg.ClockADJ );
        REG[30]=LSB(reg.ClockADJ );
        
        REG[32]=reg.AMPVOLTAGE ;
        
        REG[33]=reg.DownloadSpeed ;
        
        REG[35]=reg.TgateMode ;
        REG[36]=reg.ShortExposure ;
        REG[37]=reg.VSUB ;
        REG[38]=reg.CLAMP;
        
        REG[42]=reg.TransferBIT ;
        
        REG[46]=reg.TopSkipNull ;
        
        REG[47]=MSB(reg.TopSkipPix );
        REG[48]=LSB(reg.TopSkipPix );
        
        REG[51]=reg.MechanicalShutterMode ;
        REG[52]=reg.DownloadCloseTEC ;
       
        REG[58]=reg.SDRAM_MAXSIZE ;
        
        REG[63]=reg.Trig ;
        
        REG[17]=MSB(*PatchNumber);
        REG[18]=LSB(*PatchNumber);
        
        REG[53]=(reg.WindowHeater&~0xf0)*16+(reg.MotorHeating&~0xf0);
        
        REG[57]=reg.ADCSEL ;
        
        ret = vendTXD(handle, 0xb5, REG, 64);
        if(ret == 64)
        {
            ret = QHYCCD_SUCCESS;
        }
  
        ret = vendTXD(handle, 0xb5, REG, 64);
        if(ret == 64)
        {
            ret = QHYCCD_SUCCESS;
        }

        return ret;
}

int QHYCAM::sendRegisterQHYCCDNew(qhyccd_handle *handle, 
                  CCDREG reg, int P_Size, int *Total_P, int *PatchNumber)
{
        int ret = QHYCCD_ERROR;
        unsigned long T;  //total actual transfer data  (byte)
        unsigned char REG[64];
        unsigned char time_H,time_M,time_L;

        T = reg.LineSize * reg.VerticalSize * 2 + reg.TopSkipPix * 2;

        if (T % P_Size) {
                *Total_P = T / P_Size+1;
                *PatchNumber = (*Total_P * P_Size - T) / 2 + 16;
        } else {
                *Total_P = T / P_Size;
                *PatchNumber = 16;
        }

        time_L=(reg.Exptime % 256);
        time_M=(reg.Exptime-time_L)/256;
        time_H=(reg.Exptime-time_L-time_M*256)/65536;    
        
        REG[0]=reg.Gain ;
        
        REG[1]=reg.Offset ;
        
        REG[2]=time_H;
        REG[3]=time_M;
        REG[4]=time_L;
        
        REG[5]=reg.HBIN ;
        REG[6]=reg.VBIN ;
        
        REG[7]=MSB(reg.LineSize );
        REG[8]=LSB(reg.LineSize );
        
        REG[9]= MSB(reg.VerticalSize );
        REG[10]=LSB(reg.VerticalSize );
        
        REG[11]=MSB(reg.SKIP_TOP );
        REG[12]=LSB(reg.SKIP_TOP );
        
        REG[13]=MSB(reg.SKIP_BOTTOM );
        REG[14]=LSB(reg.SKIP_BOTTOM );
        
        REG[15]=MSB(reg.LiveVideo_BeginLine );
        REG[16]=LSB(reg.LiveVideo_BeginLine );
        
        REG[19]=MSB(reg.AnitInterlace );
        REG[20]=LSB(reg.AnitInterlace );
        
        REG[22]=reg.MultiFieldBIN ;
        
        REG[29]=MSB(reg.ClockADJ );
        REG[30]=LSB(reg.ClockADJ );
        
        REG[32]=reg.AMPVOLTAGE ;
        
        REG[33]=reg.DownloadSpeed ;
        
        REG[35]=reg.TgateMode ;
        REG[36]=reg.ShortExposure ;
        REG[37]=reg.VSUB ;
        REG[38]=reg.CLAMP;
        
        REG[42]=reg.TransferBIT ;
        
        REG[46]=reg.TopSkipNull ;
        
        REG[47]=MSB(reg.TopSkipPix );
        REG[48]=LSB(reg.TopSkipPix );
        
        REG[51]=reg.MechanicalShutterMode ;
        REG[52]=reg.DownloadCloseTEC ;
       
        REG[58]=reg.SDRAM_MAXSIZE ;
        
        REG[63]=reg.Trig ;
        
        REG[17]=MSB(*PatchNumber);
        REG[18]=LSB(*PatchNumber);
        
        REG[53]=(reg.WindowHeater&~0xf0)*16+(reg.MotorHeating&~0xf0);
        
        REG[57]=reg.ADCSEL ;
        
        ret = vendTXD(handle, 0xb5, REG, 64);
        if(ret == 64)
        {
            ret = QHYCCD_SUCCESS;
        }
  
        ret = vendTXD(handle, 0xb5, REG, 64);
        if(ret == 64)
        {
            ret = QHYCCD_SUCCESS;
        }

        return ret;
}


int QHYCAM::vendTXD(qhyccd_handle *dev_handle, uint8_t req,
                unsigned char* data, uint16_t length)
{
        int ret;
        ret = libusb_control_transfer(dev_handle, QHYCCD_REQUEST_WRITE, req,
                                      0, 0, data, length, 3000);
        return ret;
}


int QHYCAM::vendRXD(qhyccd_handle *dev_handle, uint8_t req,
                unsigned char* data, uint16_t length)
{
        int ret;
        ret = libusb_control_transfer(dev_handle, QHYCCD_REQUEST_READ, req,
                                      0, 0, data, length, 3000);
        return ret;
}

int QHYCAM::vendTXD_Ex(qhyccd_handle *dev_handle, uint8_t req,uint16_t value,uint16_t index,unsigned char* data, uint16_t length)
{
        int ret;
        ret = libusb_control_transfer(dev_handle, QHYCCD_REQUEST_WRITE, req,
                                      value, index, data, length, 3000);
        return ret;
}

int QHYCAM::vendRXD_Ex(qhyccd_handle *dev_handle, uint8_t req,uint16_t value,uint16_t index,unsigned char* data, uint16_t length)
{
        int ret;
        ret = libusb_control_transfer(dev_handle, QHYCCD_REQUEST_READ, req,
                                      value, index, data, length, 3000);
        return ret;
}


int QHYCAM::iTXD(qhyccd_handle *dev_handle,
                unsigned char *data, int length)
{
        int ret;

        int length_transfered = -1;

        ret = libusb_bulk_transfer(dev_handle, usbintwep,
                                        data, length, &length_transfered, 3000);

        return ret;
}


int QHYCAM::iRXD(qhyccd_handle *dev_handle,
                unsigned char *data, int length)
{
        int ret;

        int length_transfered;

        ret = libusb_bulk_transfer(dev_handle, usbintrep,
                                        data, length, &length_transfered, 3000);
        return ret;
}



int QHYCAM::readUSB2(qhyccd_handle *dev_handle, unsigned char *data,
                    int p_size, int p_num)
{
        int ret;
        int length_transfered;

        ret = libusb_bulk_transfer(dev_handle, usbep,
                                   data, p_size * p_num,
                                   &length_transfered, 0);
        
        return ret;
}


int QHYCAM::readUSB2_OnePackage3(qhyccd_handle *dev_handle,
                                unsigned char *data, int length)
{
        int ret;
        int length_transfered;

        ret = libusb_bulk_transfer(dev_handle, usbep,
                                   data, length, &length_transfered, 0);
        return ret;
}

int QHYCAM::beginVideo(qhyccd_handle *handle)
{
    int ret = QHYCCD_ERROR;
    unsigned char buf[1];
    buf[0] = 100;

    ret = vendTXD(handle, 0xb3, buf, 1);
    if(ret == 1)
    {
        ret = QHYCCD_SUCCESS;
    }

    return ret;
}

int QHYCAM::readUSB2B(qhyccd_handle *dev_handle, unsigned char *data,
                     int p_size, int p_num, int* pos)
{
        int ret;
        int length_transfered,length_total = 0;
        int i;
        unsigned char *buf;
        buf = (unsigned char*)malloc(p_size);
        
        for (i = 0; i < p_num; ++i) 
        {
                ret = libusb_bulk_transfer(dev_handle,
                                           usbep,
                                           buf, p_size,
                                           &length_transfered,0);
                if (ret < 0) {
                        free(buf);
                        return ret;
                }
                length_total += length_transfered;
                memcpy(data + i * p_size, buf, p_size);
                *pos = i;
        }
       
        free(buf);
        return ret;
}

int QHYCAM::readUSB2BForQHY5IISeries(qhyccd_handle *dev_handle, unsigned char *data,int sizetoread,int exptime)
{
    int ret = -1;
    unsigned char buf[4];
    int transfered = 0;
    int try_cnt = 0;
    int try_cmos = 0;
    int pos = 0;
    int to_read = sizetoread + 5;


    while( to_read )
    {
        int ret = libusb_bulk_transfer(dev_handle, usbep, data + pos,to_read, &transfered, (int)exptime + 3000);

        if(ret != LIBUSB_SUCCESS && ret != LIBUSB_ERROR_TIMEOUT)
        {
            if( try_cnt > 3 )
            {
                return QHYCCD_ERROR_EVTUSB;
            }
            try_cnt++;

            continue;
        }
        else if((ret == LIBUSB_ERROR_TIMEOUT) && (transfered == 0))
        {
            if(try_cmos > 2)
            {
                return QHYCCD_ERROR_EVTCMOS;
            }
            try_cmos++;
        }           

        pos += transfered;
        to_read -= transfered;

       /* Here we are using the pattern as a frame delimiter. If we still have bytes
          to read and the pattern is found then the frames are missalined and we are at
          the end of the previous framefram We have to start agin.
       */
       unsigned char pat[4] = {0xaa, 0x11, 0xcc, 0xee};
       void *ppat = memmem(data+pos-5, 4, pat, 4);

       if ((to_read) && (ppat))
       {
           pos = 0;
           to_read = psize + 5;
           continue;
       }
       /* If by accident to_read is 0 and we are not at the end of the frame
          we have missed the alignment pattern, so look for the next one.
       */
       if ((to_read <= 0) && (ppat == NULL))
       {
           if( try_cnt > 3 )
           {
               return QHYCCD_ERROR_EVTUSB;
           }
           pos = 0;
           to_read = psize + 5;
           try_cnt++;
           continue;
       }
    }
    return QHYCCD_SUCCESS;
}

int QHYCAM::setDC201FromInterrupt(qhyccd_handle *handle,unsigned char PWM,unsigned char FAN)
{
        int ret;
	unsigned char Buffer[3];
	Buffer[0]=0x01;
	
	if (PWM == 0)
	{
		Buffer[2] = (Buffer[2] &~ 0x80);
		Buffer[1] = 0;
	}
	else
	{
		Buffer[1] = PWM;
		Buffer[2] = (Buffer[2] | 0x80);
	}
	
	if (FAN==0) 	
	{
		Buffer[2] = (Buffer[2] &~ 0x01);
	}
        else            
        {
        	Buffer[2] = (Buffer[2] | 0x01);
        }
        ret = sendInterrupt(handle,3, Buffer);
        
        return ret;
}

signed short QHYCAM::getDC201FromInterrupt(qhyccd_handle *handle)
{
	unsigned char Buffer[64];
	signed short x;
	
	getFromInterrupt(handle,4,Buffer);
	x = Buffer[1]*256+Buffer[2];
	return x;
}
int  QHYCAM::sendInterrupt(qhyccd_handle *handle,unsigned char length,unsigned char *data)
{
    int ret = QHYCCD_ERROR;

    ret = iTXD(handle,data,length);

    if(ret != LIBUSB_SUCCESS)
    {
        return QHYCCD_ERROR; 
    }

    return QHYCCD_SUCCESS;
}

unsigned char QHYCAM::getFromInterrupt(qhyccd_handle *handle,unsigned char length,unsigned char *data)
{
	unsigned char i;
	
	i = iRXD(handle,data,length);

	return i;
}

double QHYCAM::GetCCDTemp(qhyccd_handle *handle)
{
	signed short v;

	v = (signed short)(1.024*(float)getDC201FromInterrupt(handle));
           
	return mVToDegree((double)v);
}

double QHYCAM::RToDegree(double R)
{
	double 	T;
	double LNR;
	
	if(R > 400) R=400;
	if(R < 1) R=1;
	
	LNR=log(R);
	T=1 / (0.002679+0.000291*LNR + LNR*LNR*LNR*4.28e-7);
	T=T-273.15;
	return T;
}

double QHYCAM::DegreeTomV(double degree)
{
	double V;
	double R;

	R=DegreeToR(degree);
	V=33000/(R+10)-1625;

	return V;
}

double QHYCAM::mVToDegree(double V)
{
	double R;
	double T;

	R = 33 / (V / 1000+1.625) - 10;
        
	T=RToDegree(R);

	return T;
}

double QHYCAM::DegreeToR(double degree)
{

#define SQR3(x) ((x)*(x)*(x))
#define SQRT3(x) (exp(log(x)/3))

	if (degree<-50) degree=-50;
	if (degree>50)  degree=50;

	double x,y;
	double R;
	double T;

	double A=0.002679;
	double B=0.000291;
	double C=4.28e-7;

	T=degree+273.15;


	y=(A-1/T)/C;
	x=sqrt( SQR3(B/(3*C))+(y*y)/4);
	R=exp(  SQRT3(x-y/2)-SQRT3(x+y/2));

	return R;
}

unsigned char QHYCAM::MSB(unsigned short i)
{
    unsigned int j;
    j=(i&~0x00ff)>>8;
    return j;
}

unsigned char QHYCAM::LSB(unsigned short i)
{
    unsigned int j;
    j=i&~0xff00;
    return j;
}

int QHYCAM::I2CTwoWrite(qhyccd_handle *handle,uint16_t addr,unsigned short value)
{
    unsigned char data[2];
    data[0] = MSB(value);
    data[1] = LSB(value);

    return libusb_control_transfer(handle,QHYCCD_REQUEST_WRITE,0xbb,0,addr,data,2,2000);
}

unsigned short QHYCAM::I2CTwoRead(qhyccd_handle *handle,uint16_t addr)
{
    unsigned char data[2];

    libusb_control_transfer(handle,QHYCCD_REQUEST_READ,0xb7,0,addr,data,2,2000);

    return data[0] * 256 + data[1];
}

void QHYCAM::SWIFT_MSBLSB(unsigned char *Data, int x, int y)
{
    int i,j;
    unsigned char tempData;
    unsigned long s;

    s=0;

    for (j = 0; j < y; j++) 
    {
	for (i = 0; i < x; i++) 
	{
	    tempData=Data[s];
	    Data[s]=Data[s+1];
	    Data[s+1]=tempData;
	    s=s+2;
	}
    }
}
