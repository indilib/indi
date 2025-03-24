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
 * \class INDI::Spectrograph
 * \brief Class to provide general functionality of Monodimensional Spectrograph.
 *
 * The Spectrograph capabilities must be set to select which features are exposed to the clients.
 * SetSpectrographCapability() is typically set in the constructor or initProperties(), but can also be
 * called after connection is established with the Spectrograph, but must be called /em before returning
 * true in Connect().
 *
 * Developers need to subclass INDI::Spectrograph to implement any driver for Spectrographs within INDI.
 *
 * \example Spectrograph Simulator
 * \author Jasem Mutlaq, Ilia Platone
 *
 */

namespace INDI
{
class StreamManager;

class Spectrograph : public virtual SensorInterface
{
    public:
        enum
        {
            SPECTROGRAPH_MAX_CAPABILITY                  = SENSOR_MAX_CAPABILITY << 0, /*!< Can the Sensor Integration be aborted?  */
        } SpectrographCapability;

        Spectrograph();
        virtual ~Spectrograph();

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
         * @brief setLowCutFrequency Set low cut frequency of Spectrograph device.
         * @param freq The low frequency cutoff
         */
        void setLowCutFrequency(double freq);

        /**
         * @brief setHighCutFrequency Set high cut frequency of Spectrograph device.
         * @param freq The high frequency cutoff
         */
        void setHighCutFrequency(double freq);

        /**
         * @brief setGain Set gain of Spectrograph device.
         * @param gain The requested gain
         */
        void setGain(double gain);

        /**
         * @brief getLowCutFrequency Get requested low cut frequency in Hz.
         * @return requested low cut frequency in Hz.
         */
        inline double getLowCutFrequency()
        {
            return LowCutFrequency;
        }

        /**
         * @brief getHighCutFrequency Get requested high cut frequency in Hz.
         * @return requested high cut frequency in Hz.
         */
        inline double getHighCutFrequency()
        {
            return HighCutFrequency;
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
         * @brief Return Vector Info Property
         */
        inline INumberVectorProperty *getSpectrographSettings()
        {
            return &SpectrographSettingsNP;
        }

        /**
         * @brief GetSpectrographCapability returns the Sensor capabilities.
         */
        uint32_t GetSpectrographCapability() const
        {
            return capability;
        }

        /**
         * @brief SetSpectrographCapability Set the Spectrograph capabilities. Al fields must be initialized.
         * @param cap pointer to SpectrographCapability struct.
         */
        void SetSpectrographCapability(uint32_t cap);

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
            SPECTROGRAPH_GAIN = 0,
            SPECTROGRAPH_LOWFREQ,
            SPECTROGRAPH_HIGHFREQ,
            SPECTROGRAPH_BITSPERSAMPLE,
            SPECTROGRAPH_ANTENNA,
        } SPECTROGRAPH_INFO_INDEX;
        INumberVectorProperty SpectrographSettingsNP;
        INumber SpectrographSettingsN[8];

    private:
        double LowCutFrequency;
        double HighCutFrequency;
        double Gain;

};
}
