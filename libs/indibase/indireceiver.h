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
 * \class INDI::Receiver
 * \brief Class to provide general functionality of Monodimensional Receiver.
 *
 * The Receiver capabilities must be set to select which features are exposed to the clients.
 * SetReceiverCapability() is typically set in the constructor or initProperties(), but can also be
 * called after connection is established with the Receiver, but must be called /em before returning
 * true in Connect().
 *
 * Developers need to subclass INDI::Receiver to implement any driver for Receivers within INDI.
 *
 * \example Receiver Simulator
 * \author Jasem Mutlaq, Ilia Platone
 *
 */

namespace INDI
{
class StreamManager;

class Receiver : public virtual SensorInterface
{
    public:
        enum
        {
            SPECTROGRAPH_MAX_CAPABILITY                  = SENSOR_MAX_CAPABILITY << 0, /*!< Can the Sensor Integration be aborted?  */
        } ReceiverCapability;

        Receiver();
        virtual ~Receiver();

        virtual bool initProperties() override;
        virtual bool updateProperties() override;
        virtual void ISGetProperties(const char *dev) override;
        virtual bool ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n) override;
        virtual bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n) override;
        virtual bool ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n) override;
        virtual bool ISNewBLOB(const char *dev, const char *name, int sizes[], int blobsizes[], char *blobs[], char *formats[],
                               char *names[], int n) override;
        virtual bool ISSnoopDevice(XMLEle *root) override;

        virtual bool StartIntegration(double duration) override;
        virtual void addFITSKeywords(fitsfile *fptr, uint8_t* buf, int len) override;

        /**
         * @brief setSampleRate Set depth of Receiver device.
         * @param bpp bits per pixel
         */
        void setSampleRate(double sr);

        /**
         * @brief setBandwidth Set bandwidth of Receiver device.
         * @param bandwidth The detector bandwidth
         */
        void setBandwidth(double bandwidth);

        /**
         * @brief setGain Set gain of Receiver device.
         * @param gain The requested gain
         */
        void setGain(double gain);

        /**
         * @brief setFrequency Set the frequency observed.
         * @param freq capture frequency
         */
        void setFrequency(double freq);

        /**
         * @brief setBPS Set the bits per sample.
         * @param BPS bits per sample
         */
        void setBPS(int BPS);

        /**
         * @brief getBandwidth Get requested integration bandwidth for the sensor in Hz.
         * @return requested integration bandwidth for the sensor in Hz.
         */
        inline double getBandwidth()
        {
            return Bandwidth;
        }

        /**
         * @brief getGain Get requested integration gain for the sensor.
         * @return requested integration gain for the sensor.
         */
        inline double getGain()
        {
            return Gain;
        }

        /**
         * @brief getFrequency Get requested integration frequency for the sensor in Hz.
         * @return requested Integration frequency for the sensor in Hz.
         */
        inline double getFrequency()
        {
            return Frequency;
        }

        /**
         * @brief getSampleRate Get requested sample rate for the sensor in Hz.
         * @return requested sample rate for the sensor in Hz.
         */
        inline double getSampleRate()
        {
            return SampleRate;
        }

        /**
         * @brief Return Vector Info Property
         */
        inline INumberVectorProperty *getReceiverSettings()
        {
            return &ReceiverSettingsNP;
        }

        /**
         * @brief GetReceiverCapability returns the Sensor capabilities.
         */
        uint32_t GetReceiverCapability() const
        {
            return capability;
        }

        /**
         * @brief SetReceiverCapability Set the Receiver capabilities. Al fields must be initialized.
         * @param cap pointer to ReceiverCapability struct.
         */
        void SetReceiverCapability(uint32_t cap);

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
                                   bool sendToClient = true) override;

        typedef enum
        {
            RECEIVER_GAIN = 0,
            RECEIVER_FREQUENCY,
            RECEIVER_BANDWIDTH,
            RECEIVER_BITSPERSAMPLE,
            RECEIVER_SAMPLERATE,
            RECEIVER_ANTENNA,
        } RECEIVER_INFO_INDEX;
        INumberVectorProperty ReceiverSettingsNP;
        INumber ReceiverSettingsN[7];

    private:
        int BitsPerSample;
        double Frequency;
        double SampleRate;
        double Bandwidth;
        double Gain;

};
}
