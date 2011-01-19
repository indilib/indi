/*******************************************************************************
  Copyright(c) 2010 Gerry Rozema. All rights reserved.

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
*******************************************************************************/

#ifndef INDI_CCD_H
#define INDI_CCD_H

#include "defaultdriver.h"

#define FRAME_TYPE_LIGHT 0
#define FRAME_TYPE_BIAS 1
#define FRAME_TYPE_DARK 2
#define FRAME_TYPE_FLAT 3

class INDI::CCD : public INDI::DefaultDriver
{
    protected:

        char *RawFrame;
        int RawFrameSize;

        //  Altho these numbers are indeed stored in the indi properties
        //  It makes for much cleaner code if we have 'plain old number' copies
        //  So, when we process messages, just update both

        int XRes;   //  native resolution of the ccd
        int YRes;   //  ditto
        int SubX;   //  left side of the subframe we are requesting
        int SubY;   //  top of the subframe requested
        int SubW;   //  width of the subframe
        int SubH;   //  height of the subframe
        int BinX;   //  Binning requested in the x direction
        int BinY;   //  Binning requested in the Y direction
        float PixelSizex;   //  pixel size in microns, x direction
        float PixelSizey;   //  pixel size in microns, y direction
        bool SendCompressed;

        bool HasSt4Port;

        //  If the camera has a second ccd, or integrated guide head
        //  we need information on that one too
        bool HasGuideHead;
        char *RawGuiderFrame;
        int RawGuideSize;
        int GXRes;  //  native resolution of the guide head
        int GYRes;  //  native resolution
        int GSubX;  //  left side of the guide image subframe
        int GSubY;  //  top of the guide image subframe
        int GSubW;  //  Width of the guide image
        int GSubH;  //  Height of the guide image
        float GPixelSizex;  //  phyiscal size of the guider pixels
        float GPixelSizey;
        bool GuiderCompressed;

        int FrameType;


    private:
    public:
        CCD();
        virtual ~CCD();

        //  A ccd needs to desribe the frame
        //INumberVectorProperty CcdFrameNV;
        //INumberVectorProperty CcdExposureNV;
        //INumberVectorProperty CcdBinNV;
        //INumberVectorProperty CcdPixelSizeNV;


        INumberVectorProperty ImageFrameNV;
        INumber ImageFrameN[4];

        INumberVectorProperty ImageBinNV;
        INumber ImageBinN[2];

        INumberVectorProperty ImagePixelSizeNV;
        INumber ImagePixelSizeN[6];

        INumberVectorProperty ImageExposureNV;
        INumber ImageExposureN[1];

        //INumberVectorProperty ImageExposureReqNV;
        //INumber ImageExposureReqN[1];

        INumberVectorProperty GuiderFrameNV;
        INumber GuiderFrameN[4];
        INumberVectorProperty GuiderPixelSizeNV;
        INumber GuiderPixelSizeN[6];
        INumberVectorProperty GuiderExposureNV;
        INumber GuiderExposureN[1];

        ISwitch FrameTypeS[4];
        ISwitchVectorProperty FrameTypeSV;


        ISwitch CompressS[2];
        ISwitchVectorProperty CompressSV;

        ISwitch GuiderCompressS[2];
        ISwitchVectorProperty GuiderCompressSV;


        ISwitch GuiderVideoS[2];
        ISwitchVectorProperty GuiderVideoSV;

        INumber GuideNS[2];
        INumberVectorProperty GuideNSV;
        INumber GuideEW[2];
        INumberVectorProperty GuideEWV;

        IBLOB FitsB;
        IBLOBVectorProperty FitsBV;

        IBLOB GuiderB;
        IBLOBVectorProperty GuiderBV;

        virtual int  initProperties();
        virtual bool updateProperties();
        virtual void ISGetProperties (const char *dev);

        virtual bool ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n);
        virtual bool ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n);

        virtual int StartExposure(float);
        virtual bool ExposureComplete();
        virtual int StartGuideExposure(float);
        virtual bool AbortGuideExposure();
        virtual bool GuideExposureComplete();
        virtual int uploadfile(void *,int);
        virtual int sendPreview();

        //  Handy functions for child classes
        virtual int SetCCDParams(int x,int y,int bpp,float xf,float yf);
        virtual int SetGuidHeadParams(int x,int y,int bpp,float xf,float yf);

        virtual int GuideNorth(float);
        virtual int GuideSouth(float);
        virtual int GuideEast(float);
        virtual int GuideWest(float);

        virtual int SetFrameType(int);

};

#endif // INDI:CCD_H
