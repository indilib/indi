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

#include "qhybase.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <opencv/cv.h>
#include <opencv/highgui.h>

void QHYBASE::ControlCamTemp(qhyccd_handle *h,double MAXPWM)
{
	flag_timer = !flag_timer;

	if(flag_timer == true)
	{
		nowVoltage = 1.024 * (float)getDC201FromInterrupt(h); // mV
		currentTEMP = mVToDegree(nowVoltage);
	}
	else 
	{
		flag_timer_2 = !flag_timer_2;

		if(flag_timer_2 == false)
		{
			/////PID方法控温
            NowError = nowVoltage - DegreeTomV(targetTEMP);
            if(NowError>10||NowError< -10)
			{
				currentPWM += Proportion*(1+4/Integral+Derivative/4) * NowError-Proportion*(1+2*Derivative/4) * LastError+Proportion*Derivative/4* PrevError; //功率调控量
				PrevError = LastError;
				LastError = NowError;
			}
			else
			{
				currentPWM += Proportion*(1+4/Integral+Derivative/4) * NowError-Proportion/(1+2*Derivative/4) * LastError+Proportion*(Derivative/4)* PrevError; //功率调控量
				PrevError = LastError;
				LastError = NowError;
			}

			if(currentPWM > MAXPWM)
			{
				currentPWM = MAXPWM;
			}
			if(currentPWM < 0)
			{
				currentPWM = 0;
			}
			setDC201FromInterrupt(h, currentPWM, 255);
		}

	}
}

void QHYBASE::Bit16To8_Stretch(unsigned char *InputData16,unsigned char *OutputData8,int imageX,int imageY,unsigned short B,unsigned short W)
{

    int ratio;
    int i,j;
    unsigned long s,k;
    int pixel;
    s = 0;
    k = 0;
    ratio = (W-B) / 256;

    if(ratio == 0)
    {
        ratio = 1;
    }

    for(i=0; i < imageY; i++) 
    {
        for(j=0;j<imageX;j++)
	{
	    pixel=InputData16[s]+InputData16[s+1]*256;
	    if (pixel>B)
	    {
	        pixel=(pixel-B)/ratio;
		if (pixel>255) pixel=255;
	    }
            else    pixel=0;
		
	    if (pixel>255) pixel=255;
		OutputData8[k]=pixel;
		s=s+2;
		k=k+1;
	}
    }

}

void QHYBASE::HistInfo(int x,int y,unsigned char *InBuf,unsigned char *outBuf)
{
	long s,k;
	int i;
        unsigned long Histogram[256];
	unsigned int pixel;
	unsigned long maxHist;

	IplImage *histImg,*histResizeImg;
	IplImage *histColorImg, *histColorBigImg;

	histImg          = cvCreateImage(cvSize(256,100), IPL_DEPTH_8U, 3 );
	histResizeImg    = cvCreateImage(cvSize(192,130), IPL_DEPTH_8U, 3 );
	histColorImg     = cvCreateImage(cvSize(256,10), IPL_DEPTH_8U, 3 );
	histColorBigImg  = cvCreateImage(cvSize(1000,10), IPL_DEPTH_8U, 3 );

	cvSet(histImg,CV_RGB(0,0,0),NULL);
	s=x * y ;
	for (i=0;i<256;i++) {Histogram[i]=0;}
	k=1;

	while(s)
	{
		pixel=InBuf[k];
		Histogram[pixel]++;
		k=k+2;
		s--;
    }


	maxHist=Histogram[0];
	for (int i=1; i < 255; i++)
	{
		if (Histogram[i]>maxHist)
		{
			maxHist=Histogram[i];
		}
	}

	if (maxHist==0)   maxHist=1;

	for (i=0;i<256;i++)
	{
		cvLine( histImg, cvPoint(i,100), cvPoint(i,100-Histogram[i]*256/maxHist),CV_RGB(255,0,0),1,8, 0 );
    }

	for (i = 0; i < 256; i++)
	{
		cvLine(histColorImg,cvPoint(i,0),cvPoint(i,10),CV_RGB(Histogram[i]*256/maxHist,Histogram[i]*256/maxHist,Histogram[i]*256/maxHist),1,8,0);
	}

	cvResize(histColorImg,histColorBigImg,2);          //œ«ÍŒÏñ·ÅŽó,ÒÔ±ãÏÔÊŸÆœ»¬
	cvResize(histImg,histResizeImg,2);

	memcpy(outBuf,histResizeImg->imageData,histResizeImg->imageSize);

	cvReleaseImage(&histImg);
	cvReleaseImage(&histResizeImg);
	cvReleaseImage(&histColorImg);
        cvReleaseImage(&histColorBigImg);
}
