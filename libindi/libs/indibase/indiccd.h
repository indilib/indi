/*******************************************************************************
  Copyright(c) 2010, 2011 Gerry Rozema, Jasem Mutlaq. All rights reserved.

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Library General Public
 License version 2 as published by the Free Software Foundation.

 This library is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 Library General Public License for more details.

 You should have received a copy of the GNU Library General Public License
 along with this library; see the file COPYING.LIB.  If not, write to
 the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 Boston, MA 02110-1301, USA.
*******************************************************************************/


#ifndef INDI_CCD_H
#define INDI_CCD_H

#include <fitsio.h>

#include "defaultdriver.h"
#include "indiguiderinterface.h"

extern const char *IMAGE_SETTINGS_TAB;
extern const char *IMAGE_INFO_TAB;
extern const char *GUIDE_HEAD_TAB;
extern const char *GUIDE_CONTROL_TAB;

class CCDChip
{

public:
    CCDChip();
    ~CCDChip();

    typedef enum { LIGHT_FRAME=0, BIAS_FRAME, DARK_FRAME, FLAT_FRAME } CCD_FRAME;

    int getXRes() { return XRes; }
    int getYRes() { return YRes; }
    int getSubX() { return SubX; }
    int getSubY() { return SubY; }
    int getSubW() { return SubW; }
    int getSubH() { return SubH; }
    int getBinX() { return BinX; }
    int getBinY() { return BinY; }
    int getPixelSizeX() { return PixelSizex; }
    int getPixelSizeY() { return PixelSizey; }
    int getBPP() { return BPP; }
    int getFrameBufferSize() { return RawFrameSize; }
    double getExposure() { return ImageExposureN[0].value; }
    char *getFrameBuffer() { return RawFrame; }
    bool isCompressed() { return SendCompressed; }
    bool isInterlaced() { return Interlaced; }
    CCD_FRAME getFrameType() { return FrameType; }

    void setResolutoin(int x, int y);
    void setFrame(int subx, int suby, int subw, int subh);
    void setBin(int hor, int ver);
    void setPixelSize(int x, int y);
    void setCompressed (bool cmp);
    void setInterlaced(bool intr);
    void setFrameBufferSize(int nbuf);
    void setBPP(int bpp);
    int setFrameType(CCD_FRAME);
    void setExposure(double duration);
    void setExposureFailed();

private:

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
    int BPP;            //  Bytes per Pixel
    bool Interlaced;
    char *RawFrame;
    int RawFrameSize;
    bool SendCompressed;
    CCD_FRAME FrameType;

    INumberVectorProperty *ImageExposureNP;
    INumber ImageExposureN[1];

    INumberVectorProperty *ImageFrameNP;
    INumber ImageFrameN[4];

    INumberVectorProperty *ImageBinNP;
    INumber ImageBinN[2];

    INumberVectorProperty *ImagePixelSizeNP;
    INumber ImagePixelSizeN[6];

    ISwitch FrameTypeS[4];
    ISwitchVectorProperty *FrameTypeSP;

    ISwitch CompressS[2];
    ISwitchVectorProperty *CompressSP;

    IBLOB FitsB;
    IBLOBVectorProperty *FitsBP;

    friend class INDI::CCD;
};

class INDI::CCD : public INDI::DefaultDriver, INDI::GuiderInterface
{
      public:
        CCD();
        virtual ~CCD();

        virtual bool initProperties();
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

        virtual bool updateCCDFrame(int x, int y, int w, int h);
        virtual bool updateCCDBin(int hor, int ver);

        //  Handy functions for child classes
        virtual int SetCCDParams(int x,int y,int bpp,float xf,float yf);
        virtual int SetGuidHeadParams(int x,int y,int bpp,float xf,float yf);     

        virtual void addFITSKeywords(fitsfile *fptr);

        virtual int GuideNorth(float);
        virtual int GuideSouth(float);
        virtual int GuideEast(float);
        virtual int GuideWest(float);

protected:

    float RA;
    float Dec;
    bool HasGuideHead;
    bool HasSt4Port;
    bool InExposure;

    CCDChip PrimaryCCD;
    CCDChip GuideCCD;

 private:
    //  We are going to snoop these from a telescope
    INumberVectorProperty EqNP;
    INumber EqN[2];


/*
    INumberVectorProperty *GuiderFrameNP;
    INumber GuiderFrameN[4];

    INumberVectorProperty *GuiderPixelSizeNP;
    INumber GuiderPixelSizeN[6];

    INumberVectorProperty *GuiderExposureNP;
    INumber GuiderExposureN[1];

    ISwitch GuiderCompressS[2];
    ISwitchVectorProperty *GuiderCompressSP;

    ISwitch GuiderVideoS[2];
    ISwitchVectorProperty *GuiderVideoSP;

    IBLOB GuiderB;
    IBLOBVectorProperty *GuiderBP;
    */

    ITextVectorProperty *TelescopeTP;
    IText TelescopeT[1];

};

#endif // INDI:CCD_H
