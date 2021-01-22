/*******************************************************************************
 Copyright(c) 2019 Jasem Mutlaq. All rights reserved.

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

#pragma once

#include "indiapi.h"

#include <sys/time.h>
#include <stdint.h>

namespace INDI
{

/**
 * @brief The CCDChip class provides functionality of a CCD Chip within a CCD.
 */
class CCDChip
{
    public:
        CCDChip();
        ~CCDChip();

        typedef enum { LIGHT_FRAME = 0, BIAS_FRAME, DARK_FRAME, FLAT_FRAME } CCD_FRAME;
        typedef enum { FRAME_X, FRAME_Y, FRAME_W, FRAME_H } CCD_FRAME_INDEX;
        typedef enum { BIN_W, BIN_H } CCD_BIN_INDEX;
        typedef enum
        {
            CCD_MAX_X,
            CCD_MAX_Y,
            CCD_PIXEL_SIZE,
            CCD_PIXEL_SIZE_X,
            CCD_PIXEL_SIZE_Y,
            CCD_BITSPERPIXEL
        } CCD_INFO_INDEX;

        /**
         * @brief getXRes Get the horizontal resolution in pixels of the CCD Chip.
         * @return the horizontal resolution of the CCD Chip.
         */
        inline int getXRes()
        {
            return XRes;
        }

        /**
         * @brief Get the vertical resolution in pixels of the CCD Chip.
         * @return the horizontal resolution of the CCD Chip.
         */
        inline int getYRes()
        {
            return YRes;
        }

        /**
         * @brief getSubX Get the starting left coordinates (X) of the frame.
         * @return the starting left coordinates (X) of the image.
         */
        inline int getSubX()
        {
            return SubX;
        }

        /**
         * @brief getSubY Get the starting top coordinates (Y) of the frame.
         * @return the starting top coordinates (Y) of the image.
         */
        inline int getSubY()
        {
            return SubY;
        }

        /**
         * @brief getSubW Get the width of the frame
         * @return unbinned width of the frame
         */
        inline int getSubW()
        {
            return SubW;
        }

        /**
         * @brief getSubH Get the height of the frame
         * @return unbinned height of the frame
         */
        inline int getSubH()
        {
            return SubH;
        }

        /**
         * @brief getBinX Get horizontal binning of the CCD chip.
         * @return horizontal binning of the CCD chip.
         */
        inline int getBinX()
        {
            return BinX;
        }

        /**
         * @brief getBinY Get vertical binning of the CCD chip.
         * @return vertical binning of the CCD chip.
         */
        inline int getBinY()
        {
            return BinY;
        }

        /**
         * @brief getPixelSizeX Get horizontal pixel size in microns.
         * @return horizontal pixel size in microns.
         */
        inline float getPixelSizeX()
        {
            return PixelSizeX;
        }

        /**
         * @brief getPixelSizeY Get vertical pixel size in microns.
         * @return vertical pixel size in microns.
         */
        inline float getPixelSizeY()
        {
            return PixelSizeY;
        }

        /**
         * @brief getBPP Get CCD Chip depth (bits per pixel).
         * @return bits per pixel.
         */
        inline int getBPP()
        {
            return BitsPerPixel;
        }

        /**
         * @brief getFrameBufferSize Get allocated frame buffer size to hold the CCD image frame.
         * @return allocated frame buffer size to hold the CCD image frame.
         */
        inline int getFrameBufferSize()
        {
            return RawFrameSize;
        }

        /**
         * @brief getExposureLeft Get exposure time left in seconds.
         * @return exposure time left in seconds.
         */
        inline double getExposureLeft()
        {
            return ImageExposureN[0].value;
        }

        /**
         * @brief getExposureDuration Get requested exposure duration for the CCD chip in seconds.
         * @return requested exposure duration for the CCD chip in seconds.
         */
        inline double getExposureDuration()
        {
            return ExposureDuration;
        }

        /**
         * @brief getExposureStartTime
         * @return exposure start time in ISO 8601 format.
         */
        const char *getExposureStartTime();

        /**
         * @brief getFrameBuffer Get raw frame buffer of the CCD chip.
         * @return raw frame buffer of the CCD chip.
         */
        inline uint8_t *getFrameBuffer()
        {
            return RawFrame;
        }

        /**
         * @brief setFrameBuffer Set raw frame buffer pointer.
         * @param buffer pointer to frame buffer
         * /note CCD Chip allocates the frame buffer internally once SetFrameBufferSize is called
         * with allocMem set to true which is the default behavior. If you allocated the memory
         * yourself (i.e. allocMem is false), then you must call this function to set the pointer
         * to the raw frame buffer.
         */
        void setFrameBuffer(uint8_t *buffer)
        {
            RawFrame = buffer;
        }

        /**
         * @brief isCompressed
         * @return True if frame is compressed, false otherwise.
         */
        inline bool isCompressed()
        {
            return SendCompressed;
        }

        /**
         * @brief isInterlaced
         * @return True if CCD chip is Interlaced, false otherwise.
         */
        //        inline bool isInterlaced()
        //        {
        //            return Interlaced;
        //        }

        /**
         * @brief getFrameType
         * @return CCD Frame type
         */
        inline CCD_FRAME getFrameType()
        {
            return FrameType;
        }

        /**
         * @brief getFrameTypeName returns CCD Frame type name
         * @param fType type of frame
         * @return CCD Frame type name
         */
        const char *getFrameTypeName(CCD_FRAME fType);

        /**
         * @brief Return CCD Info Property
         */
        INumberVectorProperty *getCCDInfo()
        {
            return &ImagePixelSizeNP;
        }

        /**
         * @brief setResolution set CCD Chip resolution
         * @param x width
         * @param y height
         */
        void setResolution(uint32_t x, uint32_t y);

        /**
         * @brief setFrame Set desired frame resolutoin for an exposure.
         * @param subx Left position.
         * @param suby Top position.
         * @param subw unbinned width of the frame.
         * @param subh unbinned height of the frame.
         */
        void setFrame(uint32_t subx, uint32_t suby, uint32_t subw, uint32_t subh);

        /**
         * @brief setBin Set CCD Chip binnig
         * @param hor Horizontal binning.
         * @param ver Vertical binning.
         */
        void setBin(uint8_t hor, uint8_t ver);

        /**
         * @brief setMinMaxStep for a number property element
         * @param property Property name
         * @param element Element name
         * @param min Minimum element value
         * @param max Maximum element value
         * @param step Element step value
         * @param sendToClient If true (default), the element limits are updated and is sent to the
         * client. If false, the element limits are updated without getting sent to the client.
         */
        void setMinMaxStep(const char *property, const char *element, double min, double max, double step,
                           bool sendToClient = true);

        /**
         * @brief setPixelSize Set CCD Chip pixel size
         * @param x Horziontal pixel size in microns.
         * @param y Vertical pixel size in microns.
         */
        void setPixelSize(double x, double y);

        /**
         * @brief setCompressed Set whether a frame is compressed after exposure?
         * @param cmp If true, compress frame.
         */
        void setCompressed(bool cmp);

        //        /**
        //         * @brief setInterlaced Set whether the CCD chip is interlaced or not?
        //         * @param intr If true, the CCD chip is interlaced.
        //         */
        //        void setInterlaced(bool intr);

        /**
         * @brief setFrameBufferSize Set desired frame buffer size. The function will allocate memory
         * accordingly. The frame size depends on the desired frame resolution (Left, Top, Width, Height),
         * depth of the CCD chip (bpp), and binning settings. You must set the frame size any time any of
         * the prior parameters gets updated.
         * @param nbuf size of buffer in bytes.
         * @param allocMem if True, it will allocate memory of nbut size bytes.
         */
        void setFrameBufferSize(uint32_t nbuf, bool allocMem = true);

        /**
         * @brief setBPP Set depth of CCD chip.
         * @param bpp bits per pixel
         */
        void setBPP(uint8_t bpp);

        /**
         * @brief setFrameType Set desired frame type for next exposure.
         * @param type desired CCD frame type.
         */
        void setFrameType(CCD_FRAME type);

        /**
         * @brief setExposureDuration Set desired CCD frame exposure duration for next exposure. You must
         * call this function immediately before starting the actual exposure as it is used to calculate
         * the timestamp used for the FITS header.
         * @param duration exposure duration in seconds.
         */
        void setExposureDuration(double duration);

        /**
         * @brief setExposureLeft Update exposure time left. Inform the client of the new exposure time
         * left value.
         * @param duration exposure duration left in seconds.
         */
        void setExposureLeft(double duration);

        /**
         * @brief setExposureFailed Alert the client that the exposure failed.
         */
        void setExposureFailed();

        /**
         * @return Get number of FITS axis in image. By default 2
         */
        int getNAxis() const;

        /**
         * @brief setNAxis Set FITS number of axis
         * @param value number of axis
         */
        void setNAxis(int value);

        /**
         * @brief setImageExtension Set image exntension
         * @param ext extension (fits, jpeg, raw..etc)
         */
        void setImageExtension(const char *ext);

        /**
         * @return Return image extension (fits, jpeg, raw..etc)
         */
        char *getImageExtension()
        {
            return ImageExtention;
        }

        /**
         * @return True if CCD is currently exposing, false otherwise.
         */
        bool isExposing()
        {
            return (ImageExposureNP.s == IPS_BUSY);
        }

        /**
         * @brief binFrame Perform softwre binning on the CCD frame. Only use this function if hardware
         * binning is not supported.
         */
        void binFrame();

    private:
        /////////////////////////////////////////////////////////////////////////////////////////
        /// Chip Variables
        /////////////////////////////////////////////////////////////////////////////////////////

        /// Native Horizontal resolution of the camera chip
        uint32_t XRes {0};
        /// Native Vertical resolution of the camera chip
        uint32_t YRes {0};
        /// Left side of the subframe we are requesting
        uint32_t SubX {0};
        /// Top of the subframe requested
        uint32_t SubY {0};
        /// UNBINNED width of the subframe
        uint32_t SubW {0};
        /// UNBINNED height of the subframe
        uint32_t SubH {0};
        /// Binning requested in the x direction
        uint8_t BinX {1};
        /// Binning requested in the y direction
        uint32_t BinY {1};
        /// # of Axis
        uint8_t NAxis {2};
        /// Pixel size in microns, x direction
        double PixelSizeX {0};
        /// Pixel size in microns, y direction
        double PixelSizeY {0};
        /// Bit per Pixel
        uint8_t BitsPerPixel {8};
        //bool Interlaced {false};
        // RAW Frame for image data stored as bytes.
        uint8_t *RawFrame {nullptr};
        // RAW Frame size in bytes.
        uint32_t RawFrameSize {0};
        // BINNED Frame when software binning is used.
        uint8_t *BinFrame {nullptr};
        // Should we compress frame before transmission?
        bool SendCompressed {false};
        // Frame Type
        CCD_FRAME FrameType {LIGHT_FRAME};
        // Exposure duration in seconds.
        double ExposureDuration {0};
        // Exposure startup time
        timeval StartExposureTime;
        // Image extension type (e.g. jpg)
        char ImageExtention[MAXINDIBLOBFMT];

        /////////////////////////////////////////////////////////////////////////////////////////
        /// Chip Properties
        /////////////////////////////////////////////////////////////////////////////////////////

        /////////////////////////////////////////////////////////////////////////////////////////
        /// Image Exposure Duration
        /////////////////////////////////////////////////////////////////////////////////////////
        INumberVectorProperty ImageExposureNP;
        INumber ImageExposureN[1];

        /////////////////////////////////////////////////////////////////////////////////////////
        /// Abort Exposure
        /////////////////////////////////////////////////////////////////////////////////////////
        ISwitchVectorProperty AbortExposureSP;
        ISwitch AbortExposureS[1];

        /////////////////////////////////////////////////////////////////////////////////////////
        /// Image Frame ROI
        /////////////////////////////////////////////////////////////////////////////////////////
        INumberVectorProperty ImageFrameNP;
        INumber ImageFrameN[4];

        /////////////////////////////////////////////////////////////////////////////////////////
        /// Image Binning
        /////////////////////////////////////////////////////////////////////////////////////////
        INumberVectorProperty ImageBinNP;
        INumber ImageBinN[2];

        /////////////////////////////////////////////////////////////////////////////////////////
        /// Image Resolution & Pixel Size data
        /////////////////////////////////////////////////////////////////////////////////////////
        INumberVectorProperty ImagePixelSizeNP;
        INumber ImagePixelSizeN[6];

        /////////////////////////////////////////////////////////////////////////////////////////
        /// Frame Type (Light, Bias..etc)
        /////////////////////////////////////////////////////////////////////////////////////////
        ISwitchVectorProperty FrameTypeSP;
        ISwitch FrameTypeS[4];

        /////////////////////////////////////////////////////////////////////////////////////////
        /// Compression Toggle
        /////////////////////////////////////////////////////////////////////////////////////////
        ISwitchVectorProperty CompressSP;
        ISwitch CompressS[2];

        /////////////////////////////////////////////////////////////////////////////////////////
        /// FITS Binary Data
        /////////////////////////////////////////////////////////////////////////////////////////
        IBLOBVectorProperty FitsBP;
        IBLOB FitsB;

        /////////////////////////////////////////////////////////////////////////////////////////
        /// Reset ROI Frame to Full Resolution
        /////////////////////////////////////////////////////////////////////////////////////////
        ISwitchVectorProperty ResetSP;
        ISwitch ResetS[1];

        friend class CCD;
        friend class StreamRecoder;

#if 0
        ISwitch RapidGuideS[2];
        ISwitchVectorProperty RapidGuideSP;

        ISwitch RapidGuideSetupS[3];
        ISwitchVectorProperty RapidGuideSetupSP;

        INumber RapidGuideDataN[3];
        INumberVectorProperty RapidGuideDataNP;
#endif

};

}
