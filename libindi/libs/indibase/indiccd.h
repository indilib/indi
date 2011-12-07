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

#include <fitsio.h>

#include "defaultdriver.h"


extern const char *IMAGE_SETTINGS_TAB;
extern const char *IMAGE_INFO_TAB;
extern const char *GUIDE_HEAD_TAB;
extern const char *GUIDE_CONTROL_TAB;

class INDI::CCD : public INDI::DefaultDriver
{


      public:
        CCD();
        virtual ~CCD();

        typedef enum { LIGHT_FRAME=0, BIAS_FRAME, DARK_FRAME, FLAT_FRAME } CCD_FRAME;

        //  We are going to snoop these from a telescope
        INumberVectorProperty EqNP;
        INumber EqN[2];

        INumberVectorProperty *ImageFrameNP;
        INumber ImageFrameN[4];

        INumberVectorProperty *ImageBinNP;
        INumber ImageBinN[2];

        INumberVectorProperty *ImagePixelSizeNP;
        INumber ImagePixelSizeN[6];

        INumberVectorProperty *ImageExposureNP;
        INumber ImageExposureN[1];

        //INumberVectorProperty ImageExposureReqNP;
        //INumber ImageExposureReqN[1];

        INumberVectorProperty *GuiderFrameNP;
        INumber GuiderFrameN[4];
        INumberVectorProperty *GuiderPixelSizeNP;
        INumber GuiderPixelSizeN[6];
        INumberVectorProperty *GuiderExposureNP;
        INumber GuiderExposureN[1];

        ISwitch FrameTypeS[4];
        ISwitchVectorProperty *FrameTypeSP;


        ISwitch CompressS[2];
        ISwitchVectorProperty *CompressSP;

        ISwitch GuiderCompressS[2];
        ISwitchVectorProperty *GuiderCompressSP;

        ISwitch GuiderVideoS[2];
        ISwitchVectorProperty *GuiderVideoSP;

        INumber GuideNS[2];
        INumberVectorProperty *GuideNSP;
        INumber GuideEW[2];
        INumberVectorProperty *GuideEWP;

        IBLOB FitsB;
        IBLOBVectorProperty *FitsBP;

        IBLOB GuiderB;
        IBLOBVectorProperty *GuiderBP;

        ITextVectorProperty *TelescopeTP; //  A text vector that stores the telescope we want to snoop
        IText TelescopeT[1];

        virtual bool  initProperties();
        virtual bool updateProperties();
        virtual void ISGetProperties (const char *dev);

        virtual bool ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n);
        virtual bool ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n);

        virtual bool ISNewText (const char *dev, const char *name, char *texts[], char *names[], int n);
        virtual void ISSnoopDevice (XMLEle *root);


        virtual int StartExposure(float duration);
        virtual bool ExposureComplete();
        virtual bool AbortExposure();
        virtual int StartGuideExposure(float duration);
        virtual bool AbortGuideExposure();
        virtual bool GuideExposureComplete();
        virtual int uploadfile(void *fitsdata,int total);

        int sendPreview();

        virtual void updateCCDFrame();
        virtual void updateCCDBin();

        //  Handy functions for child classes
        virtual int SetCCDParams(int x,int y,int bpp,float xf,float yf);
        virtual int SetGuidHeadParams(int x,int y,int bpp,float xf,float yf);

        virtual int GuideNorth(float);
        virtual int GuideSouth(float);
        virtual int GuideEast(float);
        virtual int GuideWest(float);

        virtual int SetFrameType(CCD_FRAME);

        virtual void addFITSKeywords(fitsfile *fptr);


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

    CCD_FRAME FrameType;

    float RA;
    float Dec;

};

#endif // INDI:CCD_H
