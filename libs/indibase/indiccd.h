/*******************************************************************************
 Copyright(c) 2010-2018 Jasem Mutlaq. All rights reserved.

 Copyright(c) 2010, 2011 Gerry Rozema. All rights reserved.

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

#pragma once

#include "indiccdchip.h"
#include "defaultdevice.h"
#include "indiguiderinterface.h"

#ifdef HAVE_WEBSOCKET
#include "indiwsserver.h"
#endif

#include <fitsio.h>

#include <memory>
#include <cstring>
#include <chrono>
#include <stdint.h>
#include <mutex>
#include <thread>

//JM 2019-01-17: Disabled until further notice
//#define WITH_EXPOSURE_LOOPING

extern const char * IMAGE_SETTINGS_TAB;
extern const char * IMAGE_INFO_TAB;
extern const char * GUIDE_HEAD_TAB;
//extern const char * RAPIDGUIDE_TAB;

namespace INDI
{

class StreamManager;

/**
 * \class CCD
 * \brief Class to provide general functionality of CCD cameras with a single CCD sensor, or a
 * primary CCD sensor in addition to a secondary CCD guide head.
 *
 * The CCD capabilities must be set to select which features are exposed to the clients.
 * SetCCDCapability() is typically set in the constructor or initProperties(), but can also be
 * called after connection is established with the CCD, but must be called /em before returning
 * true in Connect().
 *
 * It also implements the interface to perform guiding. The class enable the ability to \e snoop
 * on telescope equatorial coordinates and record them in the FITS file before upload. It also
 * snoops Sky-Quality-Meter devices to record sky quality in units of Magnitudes-Per-Arcsecond-Squared
 * (MPASS) in the FITS header.
 *
 * Support for streaming and recording is available and is handled by the StreamManager class.
 *
 * Developers need to subclass INDI::CCD to implement any driver for CCD cameras within INDI.
 *
 * Data binary transfers are supported using two methods:
 * # INDI BLOBs: This is the and recommended default configuration.
 * # Websockets: This requires INDI to be built with websocket support. There is marginal
 * improvement in throughput with Websockets when compared with INDI base64 BLOB encoding.
 * It requires the client to explicitly support websockets. It is not recommended to use this
 * approach unless for the most demanding and FPS sensitive tasks.
 *
 * INDI::CCD and INDI::StreamManager both upload frames asynchrounously in a worker thread.
 * The CCD Buffer data is protected by the ccdBufferLock mutex. When reading the camera data
 * and writing to the buffer, it must be first locked by the mutex. After the write is complete
 * release the lock. For example:
 *
 * \code{.cpp}
 * std::unique_lock<std::mutex> guard(ccdBufferLock);
 * get_ccd_frame(PrimaryCCD.getFrameBuffer);
 * guard.unlock();
 * ExposureComplete();
 * \endcode
 *
 * Similiary, before calling Streamer->newFrame, the buffer needs to be protected in a similiar fashion using
 * the same ccdBufferLock mutex.
 *
 * \example CCD Simulator
 * \version 1.1
 * \author Jasem Mutlaq
 * \author Gerry Rozema
 *
 */
class CCD : public DefaultDevice, GuiderInterface
{
    public:
        CCD();
        virtual ~CCD();

        enum
        {
            CCD_CAN_BIN        = 1 << 0, /*!< Does the CCD support binning?  */
            CCD_CAN_SUBFRAME   = 1 << 1, /*!< Does the CCD support setting ROI?  */
            CCD_CAN_ABORT      = 1 << 2, /*!< Can the CCD exposure be aborted?  */
            CCD_HAS_GUIDE_HEAD = 1 << 3, /*!< Does the CCD have a guide head?  */
            CCD_HAS_ST4_PORT   = 1 << 4, /*!< Does the CCD have an ST4 port?  */
            CCD_HAS_SHUTTER    = 1 << 5, /*!< Does the CCD have a mechanical shutter?  */
            CCD_HAS_COOLER     = 1 << 6, /*!< Does the CCD have a cooler and temperature control?  */
            CCD_HAS_BAYER      = 1 << 7, /*!< Does the CCD send color data in bayer format?  */
            CCD_HAS_STREAMING  = 1 << 8, /*!< Does the CCD support live video streaming?  */
            CCD_HAS_WEB_SOCKET = 1 << 9  /*!< Does the CCD support web socket transfers?  */
        } CCDCapability;

        typedef enum { UPLOAD_CLIENT, UPLOAD_LOCAL, UPLOAD_BOTH } CCD_UPLOAD_MODE;

        virtual bool initProperties();
        virtual bool updateProperties();
        virtual void ISGetProperties(const char * dev);
        virtual bool ISNewNumber(const char * dev, const char * name, double values[], char * names[], int n);
        virtual bool ISNewSwitch(const char * dev, const char * name, ISState * states, char * names[], int n);
        virtual bool ISNewText(const char * dev, const char * name, char * texts[], char * names[], int n);
        virtual bool ISSnoopDevice(XMLEle * root);

        static void wsThreadHelper(void * context);

        /////////////////////////////////////////////////////////////////////////////
        /// Group Names
        /////////////////////////////////////////////////////////////////////////////
        static constexpr const char *GUIDE_CONTROL_TAB  = "Guider Control";
        static constexpr const char * WCS_TAB = "WCS";


    protected:
        /**
         * @brief GetCCDCapability returns the CCD capabilities.
         */
        uint32_t GetCCDCapability() const
        {
            return capability;
        }

        /**
         * @brief SetCCDCapability Set the CCD capabilities. Al fields must be initilized.
         * @param cap pointer to CCDCapability struct.
         */
        void SetCCDCapability(uint32_t cap);

        /**
         * @return True if CCD can abort exposure. False otherwise.
         */
        bool CanAbort()
        {
            return capability & CCD_CAN_ABORT;
        }

        /**
         * @return True if CCD supports binning. False otherwise.
         */
        bool CanBin()
        {
            return capability & CCD_CAN_BIN;
        }

        /**
         * @return True if CCD supports subframing. False otherwise.
         */
        bool CanSubFrame()
        {
            return capability & CCD_CAN_SUBFRAME;
        }

        /**
         * @return True if CCD has guide head. False otherwise.
         */
        bool HasGuideHead()
        {
            return capability & CCD_HAS_GUIDE_HEAD;
        }

        /**
         * @return True if CCD has mechanical or electronic shutter. False otherwise.
         */
        bool HasShutter()
        {
            return capability & CCD_HAS_SHUTTER;
        }

        /**
         * @return True if CCD has ST4 port for guiding. False otherwise.
         */
        bool HasST4Port()
        {
            return capability & CCD_HAS_ST4_PORT;
        }

        /**
         * @return True if CCD has cooler and temperature can be controlled. False otherwise.
         */
        bool HasCooler()
        {
            return capability & CCD_HAS_COOLER;
        }

        /**
         * @return True if CCD sends image data in bayer format. False otherwise.
         */
        bool HasBayer()
        {
            return capability & CCD_HAS_BAYER;
        }

        /**
         * @return  True if the CCD supports live video streaming. False otherwise.
         */
        bool HasStreaming()
        {
            return capability & CCD_HAS_STREAMING;
        }

        /**
         * @return  True if the CCD supports native Web Socket transfers. False otherwise.
         */
        bool HasWebSocket()
        {
            return capability & CCD_HAS_WEB_SOCKET;
        }

        /**
         * @brief Set CCD temperature
         * @param temperature CCD temperature in degrees celcius.
         * @return 0 or 1 if setting the temperature call to the hardware is successful. -1 if an
         * error is encountered.
         * Return 0 if setting the temperature to the requested value takes time.
         * Return 1 if setting the temperature to the requested value is complete.
         * \note Upon returning 0, the property becomes BUSY. Once the temperature reaches the requested
         * value, change the state to OK.
         * \note This function is not implemented in CCD, it must be implemented in the child class
         */
        virtual int SetTemperature(double temperature);

        /**
         * \brief Start exposing primary CCD chip
         * \param duration Duration in seconds
         * \return true if OK and exposure will take some time to complete, false on error.
         * \note This function is not implemented in CCD, it must be implemented in the child class
         */
        virtual bool StartExposure(float duration);

        /**
         * \brief Uploads target Chip exposed buffer as FITS to the client. Dervied classes should class
         * this function when an exposure is complete.
         * @param targetChip chip that contains upload image data
         * \note This function is not implemented in CCD, it must be implemented in the child class
         */
        virtual bool ExposureComplete(CCDChip * targetChip);

        /**
         * \brief Abort ongoing exposure
         * \return true is abort is successful, false otherwise.
         * \note This function is not implemented in CCD, it must be implemented in the child class
         */
        virtual bool AbortExposure();

        /**
         * \brief Start exposing guide CCD chip
         * \param duration Duration in seconds
         * \return true if OK and exposure will take some time to complete, false on error.
         * \note This function is not implemented in CCD, it must be implemented in the child class
         */
        virtual bool StartGuideExposure(float duration);

        /**
         * \brief Abort ongoing exposure
         * \return true is abort is successful, false otherwise.
         * \note This function is not implemented in CCD, it must be implemented in the child class
         */
        virtual bool AbortGuideExposure();

        /**
         * \brief CCD calls this function when CCD Frame dimension needs to be updated in the
         * hardware. Derived classes should implement this function
         * \param x Subframe X coordinate in pixels.
         * \param y Subframe Y coordinate in pixels.
         * \param w Subframe width in pixels.
         * \param h Subframe height in pixels.
         * \note (0,0) is defined as most left, top pixel in the subframe.
         * \return true is CCD chip update is successful, false otherwise.
         * \note This function is not implemented in CCD, it must be implemented in the child class
         */
        virtual bool UpdateCCDFrame(int x, int y, int w, int h);

        /**
         * \brief CCD calls this function when Guide head frame dimension is updated by the
         * client. Derived classes should implement this function
         * \param x Subframe X coordinate in pixels.
         * \param y Subframe Y coordinate in pixels.
         * \param w Subframe width in pixels.
         * \param h Subframe height in pixels.
         * \note (0,0) is defined as most left, top pixel in the subframe.
         * \return true is CCD chip update is successful, false otherwise.
         * \note This function is not implemented in CCD, it must be implemented in the child class
         */
        virtual bool UpdateGuiderFrame(int x, int y, int w, int h);

        /**
         * \brief CCD calls this function when CCD Binning needs to be updated in the hardware.
         * Derived classes should implement this function
         * \param hor Horizontal binning.
         * \param ver Vertical binning.
         * \return true is CCD chip update is successful, false otherwise.
         * \note This function is not implemented in CCD, it must be implemented in the child class
         */
        virtual bool UpdateCCDBin(int hor, int ver);

        /**
         * \brief CCD calls this function when Guide head binning is updated by the client.
         * Derived classes should implement this function
         * \param hor Horizontal binning.
         * \param ver Vertical binning.
         * \return true is CCD chip update is successful, false otherwise.
         * \note This function is not implemented in CCD, it must be implemented in the child class
         */
        virtual bool UpdateGuiderBin(int hor, int ver);

        /**
         * \brief CCD calls this function when CCD frame type needs to be updated in the hardware.
         * \param fType Frame type
         * \return true is CCD chip update is successful, false otherwise.
         * \note It is \e not mandatory to implement this function in the child class. The CCD hardware
         * layer may either set the frame type when this function is called, or (optionally) before an
         * exposure is started.
         */
        virtual bool UpdateCCDFrameType(CCDChip::CCD_FRAME fType);

        /**
         * \brief CCD calls this function when client upload mode switch is updated.
         * \param mode upload mode. UPLOAD_CLIENT only sends the upload the client application. UPLOAD_BOTH saves the frame and uploads it to the client. UPLOAD_LOCAL only saves
         * the frame locally.
         * \return true if mode is changed successfully, false otherwise.
         * \note By default this function is implemented in the base class and returns true. Override if necessary.
         */
        virtual bool UpdateCCDUploadMode(CCD_UPLOAD_MODE mode)
        {
            INDI_UNUSED(mode);
            return true;
        }

        /**
         * \brief CCD calls this function when Guide frame type is updated by the client.
         * \param fType Frame type
         * \return true is CCD chip update is successful, false otherwise.
         * \note It is \e not mandatory to implement this function in the child class. The CCD hardware
         * layer may either set the frame type when this function is called, or (optionally) before an
         * exposure is started.
         */
        virtual bool UpdateGuiderFrameType(CCDChip::CCD_FRAME fType);

        /**
         * \brief Setup CCD paramters for primary CCD. Child classes call this function to update
         * CCD parameters
         * \param x Frame X coordinates in pixels.
         * \param y Frame Y coordinates in pixels.
         * \param bpp Bits Per Pixels.
         * \param xf X pixel size in microns.
         * \param yf Y pixel size in microns.
         */
        virtual void SetCCDParams(int x, int y, int bpp, float xf, float yf);

        /**
         * \brief Setup CCD paramters for guide head CCD. Child classes call this function to update
         * CCD parameters
         * \param x Frame X coordinates in pixels.
         * \param y Frame Y coordinates in pixels.
         * \param bpp Bits Per Pixels.
         * \param xf X pixel size in microns.
         * \param yf Y pixel size in microns.
         */
        virtual void SetGuiderParams(int x, int y, int bpp, float xf, float yf);

        /**
         * \brief Guide northward for ms milliseconds
         * \param ms Duration in milliseconds.
         * \note This function is not implemented in CCD, it must be implemented in the child class
         * \return True if successful, false otherwise.
         */
        virtual IPState GuideNorth(uint32_t ms);

        /**
         * \brief Guide southward for ms milliseconds
         * \param ms Duration in milliseconds.
         * \note This function is not implemented in CCD, it must be implemented in the child class
         * \return 0 if successful, -1 otherwise.
         */
        virtual IPState GuideSouth(uint32_t ms);

        /**
         * \brief Guide easward for ms milliseconds
         * \param ms Duration in milliseconds.
         * \note This function is not implemented in CCD, it must be implemented in the child class
         * \return 0 if successful, -1 otherwise.
         */
        virtual IPState GuideEast(uint32_t ms);

        /**
         * \brief Guide westward for ms milliseconds
         * \param ms Duration in milliseconds.
         * \note This function is not implemented in CCD, it must be implemented in the child class
         * \return 0 if successful, -1 otherwise.
         */
        virtual IPState GuideWest(uint32_t ms);

        /**
         * @brief StartStreaming Start live video streaming
         * @return True if successful, false otherwise.
         */
        virtual bool StartStreaming();

        /**
         * @brief StopStreaming Stop live video streaming
         * @return True if successful, false otherwise.
         */
        virtual bool StopStreaming();

        /**
         * \brief Add FITS keywords to a fits file
         * \param fptr pointer to a valid FITS file.
         * \param targetChip The target chip to extract the keywords from.
         * \note In additional to the standard FITS keywords, this function write the following
         * keywords the FITS file:
         * <ul>
         * <li>EXPTIME: Total Exposure Time (s)</li>
         * <li>DARKTIME (if applicable): Total Exposure Time (s)</li>
         * <li>PIXSIZE1: Pixel Size 1 (microns)</li>
         * <li>PIXSIZE2: Pixel Size 2 (microns)</li>
         * <li>BINNING: Binning HOR x VER</li>
         * <li>FRAME: Frame Type</li>
         * <li>DATAMIN: Minimum value</li>
         * <li>DATAMAX: Maximum value</li>
         * <li>INSTRUME: CCD Name</li>
         * <li>DATE-OBS: UTC start date of observation</li>
         * </ul>
         *
         * To add additional information, override this function in the child class and ensure to call
         * CCD::addFITSKeywords.
         */
        virtual void addFITSKeywords(fitsfile * fptr, CCDChip * targetChip);

        /** A function to just remove GCC warnings about deprecated conversion */
        void fits_update_key_s(fitsfile * fptr, int type, std::string name, void * p, std::string explanation, int * status);

        /**
         * @brief activeDevicesUpdated Inform children that ActiveDevices property was updated so they can
         * snoop on the updated devices if desired.
         */
        virtual void activeDevicesUpdated() {}

        /**
         * @brief saveConfigItems Save configuration items in XML file.
         * @param fp pointer to file to write to
         * @return True if successful, false otherwise
         */
        virtual bool saveConfigItems(FILE * fp);

        void GuideComplete(INDI_EQ_AXIS axis);

        // Epoch Position
        double RA, Dec;

        // J2000 Position
        double J2000RA;
        double J2000DE;

        double primaryFocalLength, primaryAperture, guiderFocalLength, guiderAperture;
        bool InExposure;
        bool InGuideExposure;
        //bool RapidGuideEnabled;
        //bool GuiderRapidGuideEnabled;

        bool AutoLoop;
        bool GuiderAutoLoop;
        bool SendImage;
        bool GuiderSendImage;
        bool ShowMarker;
        bool GuiderShowMarker;

        float ExposureTime;
        float GuiderExposureTime;

        // Sky Quality
        double MPSAS;

        // Rotator Angle
        double RotatorAngle;

        // Airmas
        double Airmass;
        double Latitude;
        double Longitude;

        // Threading
        std::mutex ccdBufferLock;

        std::vector<std::string> FilterNames;
        int CurrentFilterSlot;

        std::unique_ptr<StreamManager> Streamer;

        CCDChip PrimaryCCD;
        CCDChip GuideCCD;

        ///////////////////////////////////////////////////////////////////////////////
        /// Properties
        ///////////////////////////////////////////////////////////////////////////////


        /**
         * @brief EqNP Snoop property to read the equatorial coordinates of the mount.
         * ActiveDeviceTP defines snoop devices and the driver listens to this property emitted
         * by the mount driver if specified. It is important to generate a proper FITS header.
         */
        INumberVectorProperty EqNP;
        INumber EqN[2];

        /**
         * @brief ActiveDeviceTP defines 4 devices the camera driver can listen to (snoop) for
         * properties of interest so that it can generate a proper FITS header.
         * + **Mount**: Listens for equatorial coordinates in JNow epoch.
         * + **Rotator**: Listens for Rotator Absolute Rotation Angle (E of N) in degrees.
         * + **Filter Wheel**: Listens for FILTER_SLOT and FILTER_NAME properties.
         * + **SQM**: Listens for sky quality meter magnitude.
         */
        ITextVectorProperty ActiveDeviceTP;
        IText ActiveDeviceT[4] {};
        enum
        {
            SNOOP_MOUNT,
            SNOOP_ROTATOR,
            SNOOP_FILTER_WHEEL,
            SNOOP_SQM
        };

        /**
         * @brief TemperatureNP Read-Only Temperature in Celcius.
         */
        INumberVectorProperty TemperatureNP;
        INumber TemperatureN[1];

        /**
         *@brief BayerTP Bayer pattern offset and type
         */
        ITextVectorProperty BayerTP;
        IText BayerT[3] {};

        /**
         *@brief FileNameTP File name of locally-saved images. By default, images are uploaded to the client
         * but when upload option is set to either @a Both or @a Local, then they are saved on the local disk with
         * this name.
         */
        ITextVectorProperty FileNameTP;
        IText FileNameT[1] {};

        ISwitch UploadS[3];
        ISwitchVectorProperty UploadSP;

        IText UploadSettingsT[2] {};
        ITextVectorProperty UploadSettingsTP;
        enum
        {
            UPLOAD_DIR,
            UPLOAD_PREFIX
        };

        ISwitch TelescopeTypeS[2];
        ISwitchVectorProperty TelescopeTypeSP;
        enum
        {
            TELESCOPE_PRIMARY,
            TELESCOPE_GUIDE
        };

        // Websocket Support
        ISwitch WebSocketS[2];
        ISwitchVectorProperty WebSocketSP;
        enum
        {
            WEBSOCKET_ENABLED,
            WEBSOCKET_DISABLED,
        };


        // Websocket Settings
        INumber WebSocketSettingsN[1];
        INumberVectorProperty WebSocketSettingsNP;
        enum
        {
            WS_SETTINGS_PORT,
        };

        // WCS
        ISwitch WorldCoordS[2];
        ISwitchVectorProperty WorldCoordSP;

        // WCS CCD Rotation
        INumber CCDRotationN[1];
        INumberVectorProperty CCDRotationNP;

#ifdef WITH_EXPOSURE_LOOPING
        // Exposure Looping
        ISwitch ExposureLoopS[2];
        ISwitchVectorProperty ExposureLoopSP;
        enum
        {
            EXPOSURE_LOOP_ON,
            EXPOSURE_LOOP_OFF
        };

        // Exposure Looping Count
        INumber ExposureLoopCountN[1];
        INumberVectorProperty ExposureLoopCountNP;
        double uploadTime = { 0 };
        std::chrono::system_clock::time_point exposureLoopStartup;
#endif

        // FITS Header
        IText FITSHeaderT[2] {};
        ITextVectorProperty FITSHeaderTP;
        enum
        {
            FITS_OBSERVER,
            FITS_OBJECT
        };

    private:
        uint32_t capability;

        bool m_ValidCCDRotation;

        ///////////////////////////////////////////////////////////////////////////////
        /// Utility Functions
        ///////////////////////////////////////////////////////////////////////////////
        bool uploadFile(CCDChip * targetChip, const void * fitsData, size_t totalBytes, bool sendImage, bool saveImage);
        void getMinMax(double * min, double * max, CCDChip * targetChip);
        int getFileIndex(const char * dir, const char * prefix, const char * ext);
        bool ExposureCompletePrivate(CCDChip * targetChip);

        // Threading for Websocket
#ifdef HAVE_WEBSOCKET
        std::thread wsThread;
        void wsThreadEntry();
        INDIWSServer wsServer;
#endif

        /////////////////////////////////////////////////////////////////////////////
        /// Misc.
        /////////////////////////////////////////////////////////////////////////////
        friend class StreamManager;
};
}
