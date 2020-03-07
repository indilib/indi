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

#include "indisensorinterface.h"
#include "dsp.h"
#include <fitsio.h>

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


/**
 * \class INDI::Detector
 * \brief Class to provide general functionality of Monodimensional Detector.
 *
 * The Detector capabilities must be set to select which features are exposed to the clients.
 * SetDetectorCapability() is typically set in the constructor or initProperties(), but can also be
 * called after connection is established with the Detector, but must be called /em before returning
 * true in Connect().
 *
 * Developers need to subclass INDI::Detector to implement any driver for Detectors within INDI.
 *
 * \example Detector Simulator
 * \author Jasem Mutlaq, Ilia Platone
 *
 */

namespace INDI
{
class StreamManager;

struct pulse_t {
    timespec start;
    timespec duration;
};

class Detector : public SensorInterface
{
    public:
        /**
         * \struct DetectorConnection
         * \brief Holds the connection mode of the Detector.
         */
        enum
        {
            CONNECTION_NONE   = 1 << 0, /** Do not use any connection plugin */
            CONNECTION_SERIAL = 1 << 1, /** For regular serial and bluetooth connections */
            CONNECTION_TCP    = 1 << 2  /** For Wired and WiFI connections */
        } DetectorConnection;

        enum
        {
            DETECTOR_MAX_CAPABILITY                  = SENSOR_MAX_CAPABILITY<<0,  /*!< Can the Sensor Integration be aborted?  */
        } DetectorCapability;

        Detector();
        virtual ~Detector();

        bool initProperties();
        bool updateProperties();
        void ISGetProperties(const char *dev);
        bool ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n);
        bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n);
        bool ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n);
        bool ISNewBLOB(const char *dev, const char *name, int sizes[], int blobsizes[], char *blobs[], char *formats[], char *names[], int n);
        bool ISSnoopDevice(XMLEle *root);

        bool StartIntegration(double duration);

        /**
         * @brief setDetectorConnection Set Detector connection mode. Child class should call this in the constructor before Detector registers
         * any connection interfaces
         * @param value ORed combination of DetectorConnection values.
         */
        void setDetectorConnection(const uint8_t &value);

        /**
         * @return Get current Detector connection mode
         */
        uint8_t getDetectorConnection() const;

        /**
         * @brief setTriggerLevel Set Trigger voltage level used for pulse detection.
         * @param level trigger voltage level
         */
        void setTriggerLevel(double level);

        /**
         * @brief getTriggerLevel Get Trigger voltage level used for pulse detection.
         * @return requested trigger voltage level.
         */
        inline double getTriggerLevel()
        {
            return TriggerLevel;
        }

        /**
         * @brief Return Vector Info Property
         */
        inline INumberVectorProperty *getDetectorSettings()
        {
            return &DetectorSettingsNP;
        }

        /**
         * @brief GetDetectorCapability returns the Sensor capabilities.
         */
        uint32_t GetDetectorCapability() const
        {
            return capability;
        }

        /**
         * @brief SetDetectorCapability Set the Detector capabilities. Al fields must be initilized.
         * @param cap pointer to DetectorCapability struct.
         */
        void SetDetectorCapability(uint32_t cap);

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
        void setMinMaxStep(const char *property, const char *element, double min, double max, double step, bool sendToClient);

        /** \brief perform handshake with device to check communication */
        virtual bool Handshake();

        typedef enum
        {
            DETECTOR_RESOLUTION = 0,
            DETECTOR_TRIGGER,
        } DETECTOR_INFO_INDEX;
        INumberVectorProperty DetectorSettingsNP;


        Connection::Serial *serialConnection = NULL;
        Connection::TCP *tcpConnection       = NULL;

        /// For Serial & TCP connections
        int PortFD = -1;

      private:
        bool callHandshake();
        uint8_t detectorConnection = CONNECTION_NONE;
        double TriggerLevel;
        INumber DetectorSettingsN[2];
};
}
