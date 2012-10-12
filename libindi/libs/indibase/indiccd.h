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
#include <string.h>

#include "defaultdevice.h"
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
    typedef enum { FRAME_X, FRAME_Y, FRAME_W, FRAME_H} CCD_FRAME_INDEX;
    typedef enum { BIN_W, BIN_H} CCD_BIN_INDEX;
    typedef enum { CCD_MAX_X, CCD_MAX_Y, CCD_PIXEL_SIZE, CCD_PIXEL_SIZE_X, CCD_PIXEL_SIZE_Y, CCD_BITSPERPIXEL} CCD_INFO_INDEX;

    inline int getXRes() { return XRes; }
    inline int getYRes() { return YRes; }
    inline int getSubX() { return SubX; }
    inline int getSubY() { return SubY; }
    inline int getSubW() { return SubW; }
    inline int getSubH() { return SubH; }
    inline int getBinX() { return BinX; }
    inline int getBinY() { return BinY; }
    inline int getPixelSizeX() { return PixelSizex; }
    inline int getPixelSizeY() { return PixelSizey; }
    inline int getBPP() { return BPP; }
    inline int getFrameBufferSize() { return RawFrameSize; }
    inline double getExposureLeft() { return ImageExposureN[0].value; }
    inline double getExposureDuration() { return exposureDuration; }
    inline char *getFrameBuffer() { return RawFrame; }
    inline bool isCompressed() { return SendCompressed; }
    inline bool isInterlaced() { return Interlaced; }
    inline CCD_FRAME getFrameType() { return FrameType; }
    const char *getExposureStartTime();

    void setResolutoin(int x, int y);
    void setFrame(int subx, int suby, int subw, int subh);
    void setBin(int hor, int ver);
    void setPixelSize(int x, int y);
    void setCompressed (bool cmp);
    void setInterlaced(bool intr);
    void setFrameBufferSize(int nbuf);
    void setBPP(int bpp);
    int setFrameType(CCD_FRAME);
    void setExposureDuration(double duration);
    void setExposureLeft(double duration);
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
    double exposureDuration;
    timeval startExposureTime;

    INumberVectorProperty *ImageExposureNP;
    INumber ImageExposureN[1];

    ISwitchVectorProperty *AbortExposureSP;
    ISwitch AbortExposureS[1];

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

/**
 * \class INDI::CCD
   \brief Class to provide general functionality of CCD cameras with a single CCD sensor, or a primary CCD sensor in addition to a secondary CCD guide head.

   It also implements the interface to perform guiding. The class enable the ability to \e snoop on telescope equatorial coordinates and record them in the
   FITS file before upload. Developers need to subclass INDI::CCD to implement any driver for CCD cameras within INDI.

\author Gerry Rozema, Jasem Mutlaq
*/
class INDI::CCD : public INDI::DefaultDevice, INDI::GuiderInterface
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
        virtual bool ISSnoopDevice (XMLEle *root);

     protected:
        /** \brief Start exposing primary CCD chip
            \param duration Duration in seconds
            \return 0 if OK and exposure will take some time to complete, 1 if exposure is short and complete already (e.g. bias), -1 on error.
            \note This function is not implemented in INDI::CCD, it must be implemented in the child class
        */
        virtual int StartExposure(float duration);

        /** \brief Uploads target Chip exposed buffer as FITS to the client. Dervied classes should class this function when an exposure is complete.
             \note This function is not implemented in INDI::CCD, it must be implemented in the child class
        */
        virtual bool ExposureComplete(CCDChip *targetChip);

        /** \brief Abort ongoing exposure
            \return true is abort is successful, false otherwise.
            \note This function is not implemented in INDI::CCD, it must be implemented in the child class
        */
        virtual bool AbortExposure();

        /** \brief Start exposing guide CCD chip
            \param duration Duration in seconds
            \return 0 if OK and exposure will take some time to complete, 1 if exposure is short and complete already (e.g. bias), -1 on error.
            \note This function is not implemented in INDI::CCD, it must be implemented in the child class
        */
        virtual int StartGuideExposure(float duration);

        /** \brief Abort ongoing exposure
            \return true is abort is successful, false otherwise.
            \note This function is not implemented in INDI::CCD, it must be implemented in the child class
        */
        virtual bool AbortGuideExposure();

        /** \brief INDI::CCD calls this function when CCD Frame dimension needs to be updated in the hardware. Derived classes should implement this function
            \param x Subframe X coordinate in pixels.
            \param y Subframe Y coordinate in pixels.
            \param w Subframe width in pixels.
            \param h Subframe height in pixels.
            \note (0,0) is defined as most left, top pixel in the subframe.
            \return true is CCD chip update is successful, false otherwise.
            \note This function is not implemented in INDI::CCD, it must be implemented in the child class
        */
        virtual bool updateCCDFrame(int x, int y, int w, int h);

        /** \brief INDI::CCD calls this function when CCD Binning needs to be updated in the hardware. Derived classes should implement this function
            \param hor Horizontal binning.
            \param ver Vertical binning.
            \return true is CCD chip update is successful, false otherwise.
            \note This function is not implemented in INDI::CCD, it must be implemented in the child class
        */
        virtual bool updateCCDBin(int hor, int ver);


        /** \brief INDI::CCD calls this function when CCD frame type needs to be updated in the hardware.
            \param fType Frame type
            \return true is CCD chip update is successful, false otherwise.
            \note It is \e not mandotary to implement this function in the child class. The CCD hardware layer may either set the frame type when this function
             is called, or (optionally) before an exposure is started.
        */
        virtual bool updateCCDFrameType(CCDChip::CCD_FRAME fType);

        /** \brief Setup CCD paramters for primary CCD. Child classes call this function to update CCD paramaters
            \param x Frame X coordinates in pixels.
            \param y Frame Y coordinates in pixels.
            \param bpp Bits Per Pixels.
            \param xf X pixel size in microns.
            \param yf Y pixel size in microns.
        */
        virtual void SetCCDParams(int x,int y,int bpp,float xf,float yf);

        /** \brief Setup CCD paramters for guide head CCD. Child classes call this function to update CCD paramaters
            \param x Frame X coordinates in pixels.
            \param y Frame Y coordinates in pixels.
            \param bpp Bits Per Pixels.
            \param xf X pixel size in microns.
            \param yf Y pixel size in microns.
        */
        virtual void SetGuidHeadParams(int x,int y,int bpp,float xf,float yf);


        /** \brief Guide northward for ms milliseconds
            \param ms Duration in milliseconds.
            \note This function is not implemented in INDI::CCD, it must be implemented in the child class
            \return True if successful, false otherwise.
        */
        virtual bool GuideNorth(float ms);

        /** \brief Guide southward for ms milliseconds
            \param ms Duration in milliseconds.
            \note This function is not implemented in INDI::CCD, it must be implemented in the child class
            \return 0 if successful, -1 otherwise.
        */
        virtual bool GuideSouth(float ms);

        /** \brief Guide easward for ms milliseconds
            \param ms Duration in milliseconds.
            \note This function is not implemented in INDI::CCD, it must be implemented in the child class
            \return 0 if successful, -1 otherwise.
        */
        virtual bool GuideEast(float ms);

        /** \brief Guide westward for ms milliseconds
            \param ms Duration in milliseconds.
            \note This function is not implemented in INDI::CCD, it must be implemented in the child class
            \return 0 if successful, -1 otherwise.
        */
        virtual bool GuideWest(float ms);

        /** \brief Add FITS keywords to a fits file
            \param fptr pointer to a valid FITS file.
            \param targetChip The target chip to extract the keywords from.
            \note In additional to the standard FITS keywords, this function write the following keywords the FITS file:
            <ul>
            <li>EXPTIME: Total Exposure Time (s)</li>
            <li>DARKTIME (if applicable): Total Exposure Time (s)</li>
            <li>PIXSIZE1: Pixel Size 1 (microns)</li>
            <li>PIXSIZE2: Pixel Size 2 (microns)</li>
            <li>BINNING: Binning HOR x VER</li>
            <li>FRAME: Frame Type</li>
            <li>DATAMIN: Minimum value</li>
            <li>DATAMAX: Maximum value</li>
            <li>INSTRUME: CCD Name</li>
            <li>DATE-OBS: UTC start date of observation</li>
            </ul>

            To add additional information, override this function in the child class and ensure to call INDI::CCD::addFITSKeywords.
        */
        virtual void addFITSKeywords(fitsfile *fptr, CCDChip *targetChip);

        /* A function to just remove GCC warnings about deprecated conversion */
        void fits_update_key_s(fitsfile* fptr, int type, std::string name, void* p, std::string explanation, int* status);

        virtual bool saveConfigItems(FILE *fp);

        float RA;
        float Dec;
        bool HasGuideHead;
        bool HasSt4Port;
        bool InExposure;

        CCDChip PrimaryCCD;
        CCDChip GuideCCD;

        //  We are going to snoop these from a telescope
        INumberVectorProperty EqNP;
        INumber EqN[2];

        ITextVectorProperty *ActiveDeviceTP;
        IText ActiveDeviceT[2];

     private:
        void getMinMax(double *min, double *max, CCDChip *targetChip);


};

#endif // INDI:CCD_H
