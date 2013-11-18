/*******************************************************************************
 Copyright(c) 2010, 2011 Gerry Rozema, Jasem Mutlaq. All rights reserved.

 Rapid Guide support added by CloudMakers, s. r. o.
 Copyright(c) 2013 CloudMakers, s. r. o. All rights reserved.
  
 Star detection algorithm is based on PHD Guiding by Craig Stark
 Copyright (c) 2006-2010 Craig Stark. All rights reserved.

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
extern const char *RAPIDGUIDE_TAB;

/**
 * @brief The CCDChip class provides functionality of a CCD Chip within a CCD.
 *
 */
class CCDChip
{

public:
    CCDChip();
    ~CCDChip();

    typedef enum { LIGHT_FRAME=0, BIAS_FRAME, DARK_FRAME, FLAT_FRAME } CCD_FRAME;
    typedef enum { FRAME_X, FRAME_Y, FRAME_W, FRAME_H} CCD_FRAME_INDEX;
    typedef enum { BIN_W, BIN_H} CCD_BIN_INDEX;
    typedef enum { CCD_MAX_X, CCD_MAX_Y, CCD_PIXEL_SIZE, CCD_PIXEL_SIZE_X, CCD_PIXEL_SIZE_Y, CCD_BITSPERPIXEL} CCD_INFO_INDEX;

    /**
     * @brief getXRes Get the horizontal resolution in pixels of the CCD Chip.
     * @return the horizontal resolution of the CCD Chip.
     */
    inline int getXRes() { return XRes; }

    /**
     * @brief Get the vertical resolution in pixels of the CCD Chip.
     * @return the horizontal resolution of the CCD Chip.
     */
    inline int getYRes() { return YRes; }

    /**
     * @brief getSubX Get the starting left coordinates (X) of the frame.
     * @return the starting left coordinates (X) of the image.
     */
    inline int getSubX() { return SubX; }

    /**
     * @brief getSubY Get the starting top coordinates (Y) of the frame.
     * @return the starting top coordinates (Y) of the image.
     */
    inline int getSubY() { return SubY; }

    /**
     * @brief getSubW Get the width of the frame
     * @return unbinned width of the frame
     */
    inline int getSubW() { return SubW; }

    /**
     * @brief getSubH Get the height of the frame
     * @return unbinned height of the frame
     */
    inline int getSubH() { return SubH; }

    /**
     * @brief getBinX Get horizontal binning of the CCD chip.
     * @return horizontal binning of the CCD chip.
     */
    inline int getBinX() { return BinX; }

    /**
     * @brief getBinY Get vertical binning of the CCD chip.
     * @return vertical binning of the CCD chip.
     */
    inline int getBinY() { return BinY; }

    /**
     * @brief getPixelSizeX Get horizontal pixel size in microns.
     * @return horizontal pixel size in microns.
     */
    inline float getPixelSizeX() { return PixelSizex; }

    /**
     * @brief getPixelSizeY Get vertical pixel size in microns.
     * @return vertical pixel size in microns.
     */
    inline float getPixelSizeY() { return PixelSizey; }

    /**
     * @brief getBPP Get CCD Chip depth (bits per pixel).
     * @return bits per pixel.
     */
    inline int getBPP() { return BPP; }

    /**
     * @brief getFrameBufferSize Get allocated frame buffer size to hold the CCD image frame.
     * @return allocated frame buffer size to hold the CCD image frame.
     */
    inline int getFrameBufferSize() { return RawFrameSize; }

    /**
     * @brief getExposureLeft Get exposure time left in seconds.
     * @return exposure time left in seconds.
     */
    inline double getExposureLeft() { return ImageExposureN[0].value; }

    /**
     * @brief getExposureDuration Get requested exposure duration for the CCD chip in seconds.
     * @return requested exposure duration for the CCD chip in seconds.
     */
    inline double getExposureDuration() { return exposureDuration; }

    /**
     * @brief getExposureStartTime
     * @return exposure start time in ISO 8601 format.
     */
    const char *getExposureStartTime();

    /**
     * @brief getFrameBuffer Get raw frame buffer of the CCD chip.
     * @return raw frame buffer of the CCD chip.
     */
    inline char *getFrameBuffer() { return RawFrame; }

    /**
     * @brief isCompressed
     * @return True if frame is compressed, false otherwise.
     */
    inline bool isCompressed() { return SendCompressed; }

    /**
     * @brief isInterlaced
     * @return True if CCD chip is Interlaced, false otherwise.
     */
    inline bool isInterlaced() { return Interlaced; }

    /**
     * @brief getFrameType
     * @return CCD Frame type
     */
    inline CCD_FRAME getFrameType() { return FrameType; }

    /**
     * @brief getFrameTypeName returns CCD Frame type name
     * @param fType type of frame
     * @return CCD Frame type name
     */
    const char *getFrameTypeName(CCD_FRAME fType);

    /**
     * @brief setResolutoin set CCD Chip resolution
     * @param x width
     * @param y height
     */
    void setResolutoin(int x, int y);

    /**
     * @brief setFrame Set desired frame resolutoin for an exposure.
     * @param subx Left position.
     * @param suby Top position.
     * @param subw unbinned width of the frame.
     * @param subh unbinned height of the frame.
     */
    void setFrame(int subx, int suby, int subw, int subh);

    /**
     * @brief setBin Set CCD Chip binnig
     * @param hor Horizontal binning.
     * @param ver Vertical binning.
     */
    void setBin(int hor, int ver);

    /**
     * @brief setPixelSize Set CCD Chip pixel size
     * @param x Horziontal pixel size in microns.
     * @param y Vertical pixel size in microns.
     */
    void setPixelSize(float x, float y);

    /**
     * @brief setCompressed Set whether a frame is compressed after exposure?
     * @param cmp If true, compress frame.
     */
    void setCompressed (bool cmp);

    /**
     * @brief setInterlaced Set whether the CCD chip is interlaced or not?
     * @param intr If true, the CCD chip is interlaced.
     */
    void setInterlaced(bool intr);

    /**
     * @brief setFrameBufferSize Set desired frame buffer size. The function will allocate memory accordingly. The frame size depends on the
     * desired frame resolution (Left, Top, Width, Height), depth of the CCD chip (bpp), and binning settings. You must set the frame size any time
     * any of the prior parameters gets updated.
     * @param nbuf size of buffer in bytes.
     */
    void setFrameBufferSize(int nbuf);

    /**
     * @brief setBPP Set depth of CCD chip.
     * @param bpp bits per pixel
     */
    void setBPP(int bpp);

    /**
     * @brief setFrameType Set desired frame type for next exposure.
     * @param type desired CCD frame type.
     */
    void setFrameType(CCD_FRAME type);

    /**
     * @brief setExposureDuration Set desired CCD frame exposure duration for next exposure. You must call this function immediately before
     * starting the actual exposure as it is used to calculate the timestamp used for the FITS header.
     * @param duration exposure duration in seconds.
     */
    void setExposureDuration(double duration);

    /**
     * @brief setExposureLeft Update exposure time left. Inform the client of the new exposure time left value.
     * @param duration exposure duration left in seconds.
     */
    void setExposureLeft(double duration);

    /**
     * @brief setExposureFailed Alert the client that the exposure failed.
     */
    void setExposureFailed();


private:

    int XRes;   //  native resolution of the ccd
    int YRes;   //  ditto
    int SubX;   //  left side of the subframe we are requesting
    int SubY;   //  top of the subframe requested
    int SubW;   //  UNBINNED width of the subframe
    int SubH;   //  UNBINNED height of the subframe
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

    ISwitch FrameTypeS[5];
    ISwitchVectorProperty *FrameTypeSP;

    ISwitch CompressS[2];
    ISwitchVectorProperty *CompressSP;

    IBLOB FitsB;
    IBLOBVectorProperty *FitsBP;

    ISwitch RapidGuideS[2];
    ISwitchVectorProperty *RapidGuideSP;

    ISwitch RapidGuideSetupS[3];
    ISwitchVectorProperty *RapidGuideSetupSP;

    INumber RapidGuideDataN[3];
    INumberVectorProperty *RapidGuideDataNP;

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
        /**
         * @brief Sets whether the CCD has a guide head (2nd chip) designated for guiding.
         * @param enable Set to true if CCD has guide head.
         */
        void SetGuideHead(bool enable) { hasGuideHead = enable; }

        /**
         * @brief Set whether the CCD has an ST4 port for guiding.
         * @param enable Set to true if CCD has ST4 port.
         */
        void SetST4Port(bool enable)   { hasST4Port = enable; }

        /**
         * @brief Sets whether the CCD has a cooler and the temperature can be controlled.
         * @param enable Set to true if CCD temperature can be queried and controlled.
         */
        void SetCooler(bool enable)    { hasCooler = enable; }

        /**
         * @brief Sets whether the CCD has an electronic or mechanical shutter.
         * @param enable Set to true if the CCD has either an electronic or mechanical shutter.
         */
        void SetShutter(bool enable)   { hasShutter = enable; }

        /**
         * @return True if CCD has a guide head, false otherwise.
         */
        bool HasGuideHead() { return hasGuideHead; }

        /**
         * @return True if CCD has an ST4 port, false otherwise.
         */
        bool HasST4Port()   { return hasST4Port; }

        /**
         * @return True if CCD has a cooler, false otherwise.
         */
        bool HasCooler()    { return hasCooler; }

        /**
         * @return True if CCD has either an electronic or mechanical shutter, false otherwise.
         */
        bool HasShutter()   { return hasShutter; }


        /**
         * @brief Set primary CCD features.
         * @param hasGuideHead Does it have a guide head?
         * @param hasST4Port Does it have an ST4 port?
         * @param hasCooler Does it have a cooler?
         * @param hasShutter Does it have an electronic or mechanical shutter?
         */
        void SetCCDFeatures(bool hasGuideHead, bool hasST4Port, bool hasCooler, bool hasShutter);

        /**
         * @brief Set CCD temperature
         * @param temperature CCD temperature in degrees celcius.
         * @return 0 or 1 if setting the temperature call to the hardware is successful. -1 if an error is encountered.
         *         Return 0 if setting the temperature to the requested value takes time. Return 1 if setting the temperature to the
         *         requested value is complete.
         * \note Upon returning 0, the property becomes BUSY. Once the temperature reaches the requested value, change the state to OK.
         * \note This function is not implemented in INDI::CCD, it must be implemented in the child class
         */
        virtual int SetTemperature(double temperature);

        /** \brief Start exposing primary CCD chip
            \param duration Duration in seconds
            \return true if OK and exposure will take some time to complete, false on error.
            \note This function is not implemented in INDI::CCD, it must be implemented in the child class
        */
        virtual bool StartExposure(float duration);

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
            \return true if OK and exposure will take some time to complete, false on error.
            \note This function is not implemented in INDI::CCD, it must be implemented in the child class
        */
        virtual bool StartGuideExposure(float duration);

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
        virtual bool UpdateCCDFrame(int x, int y, int w, int h);


        /** \brief INDI::CCD calls this function when Guide head frame dimension is updated by the client. Derived classes should implement this function
            \param x Subframe X coordinate in pixels.
            \param y Subframe Y coordinate in pixels.
            \param w Subframe width in pixels.
            \param h Subframe height in pixels.
            \note (0,0) is defined as most left, top pixel in the subframe.
            \return true is CCD chip update is successful, false otherwise.
            \note This function is not implemented in INDI::CCD, it must be implemented in the child class
        */
        virtual bool UpdateGuideFrame(int x, int y, int w, int h);


        /** \brief INDI::CCD calls this function when CCD Binning needs to be updated in the hardware. Derived classes should implement this function
            \param hor Horizontal binning.
            \param ver Vertical binning.
            \return true is CCD chip update is successful, false otherwise.
            \note This function is not implemented in INDI::CCD, it must be implemented in the child class
        */
        virtual bool UpdateCCDBin(int hor, int ver);


        /** \brief INDI::CCD calls this function when Guide head binning is updated by the client. Derived classes should implement this function
            \param hor Horizontal binning.
            \param ver Vertical binning.
            \return true is CCD chip update is successful, false otherwise.
            \note This function is not implemented in INDI::CCD, it must be implemented in the child class
        */
        virtual bool UpdateGuideBin(int hor, int ver);

        /** \brief INDI::CCD calls this function when CCD frame type needs to be updated in the hardware.
            \param fType Frame type
            \return true is CCD chip update is successful, false otherwise.
            \note It is \e not mandotary to implement this function in the child class. The CCD hardware layer may either set the frame type when this function
             is called, or (optionally) before an exposure is started.
        */
        virtual bool UpdateCCDFrameType(CCDChip::CCD_FRAME fType);

        /** \brief INDI::CCD calls this function when Guide frame type is updated by the client.
            \param fType Frame type
            \return true is CCD chip update is successful, false otherwise.
            \note It is \e not mandotary to implement this function in the child class. The CCD hardware layer may either set the frame type when this function
             is called, or (optionally) before an exposure is started.
        */
        virtual bool UpdateGuideFrameType(CCDChip::CCD_FRAME fType);

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
        virtual void SetGuideHeadParams(int x,int y,int bpp,float xf,float yf);


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

        /**
         * @brief activeDevicesUpdated Inform children that ActiveDevices property was updated so they can snoop on the updated devices if desired.
         */
        virtual void activeDevicesUpdated() {}

        virtual bool saveConfigItems(FILE *fp);


        float RA;
        float Dec;
        bool InExposure;
        bool InGuideExposure;
        bool RapidGuideEnabled;
        bool GuiderRapidGuideEnabled;

        bool AutoLoop;
        bool GuiderAutoLoop;
        bool SendImage;
        bool GuiderSendImage;
        bool ShowMarker;
        bool GuiderShowMarker;
        
        float ExposureTime;
        float GuiderExposureTime;

        CCDChip PrimaryCCD;
        CCDChip GuideCCD;

        //  We are going to snoop these from a telescope
        INumberVectorProperty EqNP;
        INumber EqN[2];

        ITextVectorProperty *ActiveDeviceTP;
        IText ActiveDeviceT[2];

        INumber                 TemperatureN[1];
        INumberVectorProperty   TemperatureNP;

     private:
        bool hasGuideHead;
        bool hasST4Port;
        bool hasShutter;
        bool hasCooler;
        void getMinMax(double *min, double *max, CCDChip *targetChip);


};

#endif // INDI:CCD_H
