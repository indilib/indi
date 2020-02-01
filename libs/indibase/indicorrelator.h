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
 * \class INDI::Correlator
 * \brief Class to provide general functionality of Monodimensional Correlator.
 *
 * The Correlator capabilities must be set to select which features are exposed to the clients.
 * SetCorrelatorCapability() is typically set in the constructor or initProperties(), but can also be
 * called after connection is established with the Correlator, but must be called /em before returning
 * true in Connect().
 *
 * Developers need to subclass INDI::Correlator to implement any driver for Correlators within INDI.
 *
 * \example Correlator Simulator
 * \author Jasem Mutlaq, Ilia Platone
 *
 */

namespace INDI
{
class StreamManager;

class Correlator : public SensorInterface
{
    public:
        /**
         * \struct Baseline
         * \brief the baseline (position of the telescopes) of this correlator.
         */
        typedef struct {
            double x;
            double y;
            double z;
        } Baseline;

        /**
         * \enum UVCoordinate
         * \brief The coordinates of the current projection into the UV plane.
         */
        typedef struct {
            double u;
            double v;
        } UVCoordinate;

        /**
         * \enum Correlation
         * \brief Struct containing the correlation, coordinate and baseline informations.
         */
        typedef struct {
            Baseline baseline;
            UVCoordinate coordinate;
            double coefficient;
        } Correlation;

        /**
         * \enum CorrelatorConnection
         * \brief Holds the connection mode of the Correlator.
         */
        enum
        {
            CONNECTION_NONE   = 1 << 0, /** Do not use any connection plugin */
            CONNECTION_SERIAL = 1 << 1, /** For regular serial and bluetooth connections */
            CONNECTION_TCP    = 1 << 2  /** For Wired and WiFI connections */
        } CorrelatorConnection;

        enum
        {
            CORRELATOR_MAX_CAPABILITY                  = SENSOR_MAX_CAPABILITY<<0,  /*!< Can the Sensor Integration be aborted?  */
        } CorrelatorCapability;

        Correlator();
        virtual ~Correlator();

        bool initProperties();
        bool updateProperties();
        void ISGetProperties(const char *dev);
        bool ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n);
        bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n);
        bool ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n);
        bool ISNewBLOB(const char *dev, const char *name, int sizes[], int blobsizes[], char *blobs[], char *formats[], char *names[], int n);
        bool ISSnoopDevice(XMLEle *root);

        virtual bool StartIntegration(double duration);

        /**
         * @brief getCorrelationDegree Get current correlation degree.
         * @return the correlation coefficient.
         */
        virtual inline double getCorrelationDegree() { return 0.0; }

        /**
         * @brief getCorrelation Get current correlation degree plus UV and baseline information.
         * @return the correlation and UV coordinates.
         */
        inline Correlation getCorrelation() { Correlation ret; ret.coordinate = getUVCoordinates(); ret.baseline = baseline; ret.coefficient = getCorrelationDegree(); return ret; }

        /** \brief perform handshake with device to check communication */
        virtual bool Handshake();

        /**
         * @brief setCorrelatorConnection Set Correlator connection mode. Child class should call this in the constructor before Correlator registers
         * any connection interfaces
         * @param value ORed combination of CorrelatorConnection values.
         */
        void setCorrelatorConnection(const uint8_t &value);

        /**
         * @return Get current Correlator connection mode
         */
        uint8_t getCorrelatorConnection() const;

        /**
         * @brief setBaseline Set the baseline size in meters.
         * @param baseline the baseline size.
         */
        void setBaseline(Baseline bl);

        /**
         * @brief setWavelength Set the observed wavelength.
         * @param wavelength the wavelength in meters.
         */
        void setWavelength(double wl);

        /**
         * @brief setBandwidth Set the bandwidth of the correlator.
         * @param bandwidth the instrumentation bandwidth in Hz.
         */
        void setBandwidth(double bw);

        /**
         * @brief setBaseline Get the baseline size in meters.
         * @return the baseline size.
         */
        inline Baseline getBaseline()
        {
            return baseline;
        }

        /**
         * @brief setWavelength Get the observed wavelength.
         * @return the wavelength in meters.
         */
        inline double getWavelength()
        {
            return wavelength;
        }

        /**
         * @brief getUVCoordinates Get current UV projected coordinates.
         * @return the UV coordinates.
         */
        UVCoordinate getUVCoordinates();

        /**
         * @brief setBandwidth Get the bandwidth of the correlator.
         * @return the instrumentation bandwidth in Hz.
         */
        inline double setBandwidth()
        {
            return bandwidth;
        }

        /**
         * @brief Return Vector Info Property
         */
        inline INumberVectorProperty *getCorrelatorSettings()
        {
            return &CorrelatorSettingsNP;
        }

        /**
         * @brief GetCorrelatorCapability returns the Sensor capabilities.
         */
        uint32_t GetCorrelatorCapability() const
        {
            return capability;
        }

        /**
         * @brief SetCorrelatorCapability Set the Correlator capabilities. Al fields must be initilized.
         * @param cap pointer to CorrelatorCapability struct.
         */
        void SetCorrelatorCapability(uint32_t cap);

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

        typedef enum
        {
            CORRELATOR_BASELINE_X = 0,
            CORRELATOR_BASELINE_Y,
            CORRELATOR_BASELINE_Z,
            CORRELATOR_WAVELENGTH,
            CORRELATOR_BANDWIDTH,
        } CORRELATOR_INFO_INDEX;
        INumberVectorProperty CorrelatorSettingsNP;


        Connection::Serial *serialConnection = NULL;
        Connection::TCP *tcpConnection       = NULL;

        /// For Serial & TCP connections
        int PortFD = -1;

      private:
        bool callHandshake();
        uint8_t correlatorConnection = CONNECTION_NONE;
        Baseline baseline;
        double wavelength;
        double bandwidth;
        INumber CorrelatorSettingsN[5];
};
}
