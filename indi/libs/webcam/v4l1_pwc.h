/*
    Phlips webcam driver for V4L 1
    Copyright (C) 2005 by Jasem Mutlaq <mutlaqja@ikarustech.com>

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA

*/

#ifndef V4L1_PWC_H
#define V4L1_PWC_H

#include <stdio.h>
#include <stdlib.h>
#include "videodev.h"
#include "v4l1_base.h"

class V4L1_PWC : public V4L1_Base
{
  public:
   V4L1_PWC();
   ~V4L1_PWC();
 
        int connectCam(const char * devpath, char *errmsg);

	/* Philips related, from QAstrocam */
	int    saveSettings(char *errmsg);
	void   restoreSettings();
	void   restoreFactorySettings();
	int    setGain(int value, char *errmsg);
	int    getGain();
	int    setExposure(int val, char *errmsg);
	void   setCompression(int value);
	int    getCompression();
	int    setNoiseRemoval(int value, char *errmsg);
	int    getNoiseRemoval();
	int    setSharpness(int value, char *errmsg);
	int    getSharpness();
	int    setBackLight(bool val, char *errmsg);
	bool   getBackLight();
	int    setFlicker(bool val, char *errmsg);
	bool   getFlicker();
	void   setGama(int value);
	int    getGama();
	int    setFrameRate(int value, char *errmsg);
	int    getFrameRate();
	int    setWhiteBalance(char *errmsg);
	int    getWhiteBalance();
	int    setWhiteBalanceMode(int val, char *errmsg);
	int    setWhiteBalanceRed(int val, char *errmsg);
	int    setWhiteBalanceBlue(int val, char *errmsg);
	
	/* TODO consider the SC modded cam after this
	void setLongExposureTime(const QString& str);
	void setFrameRateMultiplicateur(int value);*/


	/* Updates */
	//void updateFrame(int d, void *p);

	/* Image Size */
	void checkSize(int & x, int & y);
	bool setSize(int x, int y);


        private:
	int whiteBalanceMode_;
	int whiteBalanceRed_;
	int whiteBalanceBlue_;
	int lastGain_;
	int multiplicateur_;
	int skippedFrame_;
	int type_;


};

#endif
