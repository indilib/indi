/*******************************************************************************
 Copyright(c) 2010, 2017 Ilia Platone, Jasem Mutlaq. All rights reserved.

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

#include "defaultdevice.h"
#include "dsp.h"
#include "dsp/manager.h"
#include "stream/streammanager.h"
#include <fitsio.h>

#include <fitsio.h>

#include <memory>
#include <cstring>
#include <chrono>
#include <stdint.h>
#include <mutex>
#include <thread>
#include <stream/streammanager.h>
#include <connectionplugins/connectionserial.h>
#include <connectionplugins/connectiontcp.h>

//JM 2019-01-17: Disabled until further notice
//#define WITH_EXPOSURE_LOOPING

/**
 * \class INDI::Sensor
 * \brief Class to provide general functionality of Monodimensional Sensor.
 *
 * The Sensor capabilities must be set to select which features are exposed to the clients.
 * SetSensorCapability() is typically set in the constructor or initProperties(), but can also be
 * called after connection is established with the Sensor, but must be called /em before returning
 * true in Connect().
 *
 * Developers need to subclass INDI::Sensor to implement any driver for Sensors within INDI.
 *
 * \example Sensor Simulator
 * \author Jasem Mutlaq, Ilia Platone
 *
 */
namespace DSP
{
class Manager;
}
namespace INDI
{
class StreamManager;

/**
 * @brief The SensorDevice class provides functionality of a Sensor Device within a Sensor.
 */
class SensorInterface : public DefaultDevice
{

    public:
        enum
        {
            SENSOR_CAN_ABORT                  = 1 << 0, /*!< Can the Sensor Integration be aborted?  */
            SENSOR_HAS_STREAMING              = 1 << 1, /*!< Does the Sensor supports streaming?  */
            SENSOR_HAS_SHUTTER                = 1 << 2, /*!< Does the Sensor have a mechanical shutter?  */
            SENSOR_HAS_COOLER                 = 1 << 3, /*!< Does the Sensor have a cooler and temperature control?  */
            SENSOR_HAS_DSP                    = 1 << 4,
            SENSOR_MAX_CAPABILITY             = 1 << 5, /*!< Does the Sensor have a cooler and temperature control?  */
        } SensorCapability;

        SensorInterface();
        ~SensorInterface();


        /**
         * \enum SensorConnection
         * \brief Holds the connection mode of the Sensor.
         */
        enum
        {
            CONNECTION_NONE   = 1 << 0, /** Do not use any connection plugin */
            CONNECTION_SERIAL = 1 << 1, /** For regular serial and bluetooth connections */
            CONNECTION_TCP    = 1 << 2  /** For Wired and WiFI connections */
        } SensorConnection;

        bool initProperties();
        bool updateProperties();
        bool processNumber(const char *dev, const char *name, double values[], char *names[], int n);
        void processProperties(const char *dev);
        bool processText(const char *dev, const char *name, char *texts[], char *names[], int n);
        bool processSwitch(const char *dev, const char *name, ISState states[], char *names[], int n);
        bool processBLOB(const char *dev, const char *name, int sizes[], int blobsizes[], char *blobs[],
                         char *formats[], char *names[], int n);
        bool processSnoopDevice(XMLEle *root);

        /**
         * @brief getContinuumBufferSize Get allocated continuum buffer size to hold the Sensor integrationd stream.
         * @return allocated continuum buffer size to hold the Sensor integration stream.
         */
        inline int getBufferSize() const
        {
            return BufferSize;
        }

        /**
         * @brief getIntegrationLeft Get Integration time left in seconds.
         * @return Integration time left in seconds.
         */
        inline double getIntegrationLeft() const
        {
            return FramedIntegrationN[0].value;
        }

        /**
         * @brief getIntegrationTime Get requested Integration duration for the Sensor device in seconds.
         * @return requested Integration duration for the Sensor device in seconds.
         */
        inline double getIntegrationTime() const
        {
            return integrationTime;
        }

        /**
         * @brief getIntegrationStartTime
         * @return Integration start time in ISO 8601 format.
         */
        const char *getIntegrationStartTime();

        /**
         * @brief getBuffer Get raw buffer of the stream of the Sensor device.
         * @return raw buffer of the Sensor device.
         */
        inline uint8_t *getBuffer()
        {
            return Buffer;
        }

        /**
         * @brief setBuffer Set raw frame buffer pointer.
         * @param buffer pointer to continuum buffer
         * /note Sensor Device allocates the frame buffer internally once SetBufferSize is called
         * with allocMem set to true which is the default behavior. If you allocated the memory
         * yourself (i.e. allocMem is false), then you must call this function to set the pointer
         * to the raw frame buffer.
         */
        inline void setBuffer(uint8_t *buffer)
        {
            Buffer = buffer;
        }

        /**
         * @brief getBPS Get Sensor depth (bits per sample).
         * @return bits per sample.
         */
        inline int getBPS() const
        {
            return BPS;
        }

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
        virtual void setMinMaxStep(const char *property, const char *element, double min, double max, double step,
                                   bool sendToClient = true);

        /**
         * @brief setBufferSize Set desired buffer size. The function will allocate memory
         * accordingly. The frame size depends on the desired integration time, sampling frequency, and
         * sample depth of the Sensor device (bps). You must set the frame size any time any of
         * the prior parameters gets updated.
         * @param nbuf size of buffer in bytes.
         * @param allocMem if True, it will allocate memory of nbut size bytes.
         */
        void setBufferSize(int nbuf, bool allocMem = true);

        /**
         * @brief setBPP Set depth of Sensor device.
         * @param bpp bits per pixel
         */
        void setBPS(int bps);

        /**
         * @brief setIntegrationTime Set desired Sensor frame Integration duration for next Integration. You must
         * call this function immediately before starting the actual Integration as it is used to calculate
         * the timestamp used for the FITS header.
         * @param duration Integration duration in seconds.
         */
        void setIntegrationTime(double duration);

        /**
         * @brief setIntegrationLeft Update Integration time left. Inform the client of the new Integration time
         * left value.
         * @param duration Integration duration left in seconds.
         */
        void setIntegrationLeft(double duration);

        /**
         * @brief setIntegrationFailed Alert the client that the Integration failed.
         */
        void setIntegrationFailed();

        /**
         * @return Get number of FITS axis in integration. By default 2
         */
        int getNAxis() const;

        /**
         * @brief setNAxis Set FITS number of axis
         * @param value number of axis
         */
        void setNAxis(int value);

        /**
         * @brief setIntegrationExtension Set integration extension
         * @param ext extension (fits, jpeg, raw..etc)
         */
        void setIntegrationFileExtension(const char *ext);

        /**
         * @return Return integration extension (fits, jpeg, raw..etc)
         */
        inline char *getIntegrationFileExtension()
        {
            return integrationExtention;
        }

        /**
         * @return True if Sensor is currently exposing, false otherwise.
         */
        inline bool isCapturing() const
        {
            return (FramedIntegrationNP.s == IPS_BUSY);
        }

        /**
         * @brief Set Sensor temperature
         * @param temperature Sensor temperature in degrees celsius.
         * @return 0 or 1 if setting the temperature call to the hardware is successful. -1 if an
         * error is encountered.
         * Return 0 if setting the temperature to the requested value takes time.
         * Return 1 if setting the temperature to the requested value is complete.
         * \note Upon returning 0, the property becomes BUSY. Once the temperature reaches the requested
         * value, change the state to OK.
         * \note This function is not implemented in Sensor, it must be implemented in the child class
         */
        virtual int SetTemperature(double temperature);

        /////////////////////////////////////////////////////////////////////////////
        /// Misc.
        /////////////////////////////////////////////////////////////////////////////
        friend class StreamManager;
        friend class StreamManagerPrivate;
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
         * \brief Start integration from the Sensor device
         * \param duration Duration in seconds
         * \return true if OK and Integration will take some time to complete, false on error.
         * \note This function is not implemented in Sensor, it must be implemented in the child class
         */
        virtual bool StartIntegration(double duration);

        /**
         * \brief Uploads target Device exposed buffer as FITS to the client. Derived classes should class
         * this function when an Integration is complete.
         * @param targetDevice device that contains upload integration data
         * \note This function is not implemented in Sensor, it must be implemented in the child class
         */
        virtual bool IntegrationComplete();

        /** \brief perform handshake with device to check communication */
        virtual bool Handshake();

        /**
         * @brief setSensorConnection Set Sensor connection mode. Child class should call this in the constructor before Sensor registers
         * any connection interfaces
         * @param value ORed combination of SensorConnection values.
         */
        void setSensorConnection(const uint8_t &value);

        /**
         * @return Get current Sensor connection mode
         */
        inline uint8_t getSensorConnection()
        {
            return sensorConnection;
        }

        /**
         * \brief Add FITS keywords to a fits file
         * \param fptr pointer to a valid FITS file.
         * \param buf The buffer of the fits contents.
         * \param len The length of the buffer.
         * \note In additional to the standard FITS keywords, this function write the following
         * keywords the FITS file:
         * <ul>
         * <li>EXPTIME: Total Integration Time (s)</li>
         * <li>DATAMIN: Minimum value</li>
         * <li>DATAMAX: Maximum value</li>
         * <li>INSTRUME: Sensor Name</li>
         * <li>DATE-OBS: UTC start date of observation</li>
         * </ul>
         *
         * To add additional information, override this function in the child class and ensure to call
         * SensorInterface::addFITSKeywords.
         */
        virtual void addFITSKeywords(fitsfile *fptr, uint8_t* buf, int len);

        /** A function to just remove GCC warnings about deprecated conversion */
        void fits_update_key_s(fitsfile *fptr, int type, std::string name, void *p, std::string explanation, int *status);

        /** Return the connection file descriptor */
        int getPortFD()
        {
            return PortFD;
        }

        /** Export the serial connection object */
        Connection::Serial *getSerialConnection()
        {
            return serialConnection;
        }

        /** Export the TCP connection object */
        Connection::TCP *getTcpConnection()
        {
            return tcpConnection;
        }


    protected:

        /**
         * @return True if Sensor has mechanical or electronic shutter. False otherwise.
         */
        bool HasShutter() const
        {
            return capability & SENSOR_HAS_SHUTTER;
        }

        /**
         * @return True if Sensor has cooler and temperature can be controlled. False otherwise.
         */
        bool HasCooler() const
        {
            return capability & SENSOR_HAS_COOLER;
        }

        /**
         * @return True if Sensor can abort Integration. False otherwise.
         */
        bool CanAbort() const
        {
            return capability & SENSOR_CAN_ABORT;
        }

        /**
         * @return  True if the Sensor wants DSP processing. False otherwise.
         */
        bool HasDSP()
        {
            if (capability & SENSOR_HAS_DSP)
            {
                if(DSP.get() == nullptr)
                {
                    DSP.reset(new DSP::Manager(this));
                }
                return true;
            }
            return false;
        }

        /**
         * @return  True if the Sensor supports live video streaming. False otherwise.
         */
        bool HasStreaming()
        {
            if (capability & SENSOR_HAS_STREAMING)
            {
                if(Streamer.get() == nullptr)
                {
                    Streamer.reset(new StreamManager(this));
                    Streamer->initProperties();
                }
                return true;
            }
            return false;
        }

        /**
         * @brief GetCapability returns the Sensor capabilities.
         */
        uint32_t GetCapability() const
        {
            return capability;
        }

        /**
         * @brief SetCapability Set the Sensor capabilities. Al fields must be initialized.
         * @param cap pointer to SensorCapability struct.
         */
        void SetCapability(uint32_t cap);

        /**
         * \brief Abort ongoing Integration
         * \return true is abort is successful, false otherwise.
         * \note This function is not implemented in Sensor, it must be implemented in the child class
         */
        virtual bool AbortIntegration();

        uint32_t capability;

        INumberVectorProperty FramedIntegrationNP;
        INumber FramedIntegrationN[1];

        ISwitchVectorProperty AbortIntegrationSP;
        ISwitch AbortIntegrationS[1];

        IBLOB FitsB;
        IBLOBVectorProperty FitsBP;

        ITextVectorProperty ActiveDeviceTP;
        IText ActiveDeviceT[4] {};

        IText FileNameT[1] {};
        ITextVectorProperty FileNameTP;
        enum
        {
            TELESCOPE_PRIMARY
        };

        ISwitch DatasetS[1];
        ISwitchVectorProperty DatasetSP;


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

        // FITS Header
        IText FITSHeaderT[2] {};
        ITextVectorProperty FITSHeaderTP;
        enum
        {
            FITS_OBSERVER,
            FITS_OBJECT
        };

        /**
         * @brief saveConfigItems Save configuration items in XML file.
         * @param fp pointer to file to write to
         * @return True if successful, false otherwise
         */
        virtual bool saveConfigItems(FILE *fp);

        bool InIntegration;

        INumberVectorProperty EqNP;
        INumber EqN[2];
        double RA, Dec;

        INumberVectorProperty LocationNP;
        INumber LocationN[3];
        double Latitude, Longitude, Elevation;

        INumberVectorProperty ScopeParametersNP;
        INumber ScopeParametersN[4];
        double primaryAperture, primaryFocalLength;

        bool AutoLoop;
        bool SendIntegration;
        bool ShowMarker;

        double IntegrationTime;

        // Sky Quality
        double MPSAS;

        /**
         * @brief activeDevicesUpdated Inform children that ActiveDevices property was updated so they can
         * snoop on the updated devices if desired.
         */
        virtual void activeDevicesUpdated() {}

        INumber TemperatureN[1];
        INumberVectorProperty TemperatureNP;

        // Threading
        std::mutex detectorBufferLock;

        std::unique_ptr<StreamManager> Streamer;
        std::unique_ptr<DSP::Manager> DSP;
        Connection::Serial *serialConnection = NULL;
        Connection::TCP *tcpConnection       = NULL;

        /// For Serial & TCP connections
        int PortFD = -1;

    private:
        bool callHandshake();
        uint8_t sensorConnection = CONNECTION_NONE;

        int BPS;
        /// # of Axis
        int NAxis;
        /// Bytes per Sample
        uint8_t *Buffer;
        int BufferSize;
        double integrationTime;
        double startIntegrationTime;
        char integrationExtention[MAXINDIBLOBFMT];

        bool uploadFile(const void *fitsData, size_t totalBytes, bool sendIntegration, bool saveIntegration);
        void getMinMax(double *min, double *max, uint8_t *buf, int len, int bpp);
        int getFileIndex(const char *dir, const char *prefix, const char *ext);

        bool IntegrationCompletePrivate();
        void* sendFITS(uint8_t* buf, int len);
};
}
