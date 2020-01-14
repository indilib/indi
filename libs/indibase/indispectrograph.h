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

extern const char *CAPTURE_SETTINGS_TAB;
extern const char *CAPTURE_INFO_TAB;
extern const char *GUIDE_HEAD_TAB;


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

class Spectrograph : public SensorInterface
{
    public:
        enum
        {
            SPECTROGRAPH_MAX_CAPABILITY                  = 1<<SENSOR_MAX_CAPABILITY,  /*!< Can the Sensor Integration be aborted?  */
        } SpectrographCapability;

        Spectrograph();
        virtual ~Spectrograph();

        bool initProperties();
        bool updateProperties();
        void ISGetProperties(const char *dev);
        bool ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n);
        bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n);
        bool ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n);
        bool ISSnoopDevice(XMLEle *root);

        /**
         * @brief setSampleRate Set depth of Spectrograph device.
         * @param bpp bits per pixel
         */
        void setSampleRate(double sr);

        /**
         * @brief setBandwidth Set bandwidth of Spectrograph device.
         * @param bandwidth The detector bandwidth
         */
        void setBandwidth(double bandwidth);

        /**
         * @brief setGain Set gain of Spectrograph device.
         * @param gain The requested gain
         */
        void setGain(double gain);

        /**
         * @brief setFrequency Set the frequency observed.
         * @param freq capture frequency
         */
        void setFrequency(double freq);

        /**
         * @brief getBandwidth Get requested integration bandwidth for the Vector device in Hz.
         * @return requested integration bandwidth for the Vector device in Hz.
         */
        inline double getBandwidth()
        {
            return Bandwidth;
        }

        /**
         * @brief getGain Get requested integration gain for the Vector device.
         * @return requested integration gain for the Vector device.
         */
        inline double getGain()
        {
            return Gain;
        }

        /**
         * @brief getFrequency Get requested integration frequency for the Vector device in Hz.
         * @return requested Integration frequency for the Vector device in Hz.
         */
        inline double getFrequency()
        {
            return Frequency;
        }

        /**
         * @brief getSampleRate Get requested sample rate for the Vector device in Hz.
         * @return requested sample rate for the Vector device in Hz.
         */
        inline double getSampleRate()
        {
            return Samplerate;
        }

        /**
         * @brief Return Vector Info Property
         */
        inline INumberVectorProperty *getSpectrographSettings()
        {
            return &SpectrographSettingsNP;
        }

        /**
         * \brief Set common integration params
         * \param sr Spectrograph samplerate (in Hz)
         * \param cfreq Integration frequency of the detector (Hz, observed frequency).
         * \param sfreq Sampling frequency of the detector (Hz, electronic speed of the detector).
         * \param bps Bit resolution of a single sample.
         * \param bw Bandwidth (Hz).
         * \param gain Gain of the detector.
         * \return true if OK and Integration will take some time to complete, false on error.
         * \note This function is not implemented in Spectrograph, it must be implemented in the child class
         */
        virtual bool paramsUpdated(double sr, double freq, double bps, double bw, double gain);

        /**
         * \brief Setup parameters for the Spectrograph. Child classes call this function to update
         * Vector parameters
         * \param samplerate Vector samplerate (in Hz)
         * \param freq Center frequency of the detector (Hz, observed frequency).
         * \param bps Bit resolution of a single sample.
         * \param bw Vector bandwidth (in Hz).
         * \param gain Vector gain.
         */
         virtual void setParams(double samplerate, double freq, double bps, double bw, double gain);

        virtual bool processNumber(const char *dev, const char *name, double values[], char *names[], int n);

        /**
         * @brief GetSensorCapability returns the Sensor capabilities.
         */
        uint32_t GetSpectrographCapability() const
        {
            return capability;
        }

        /**
         * @brief SetSensorCapability Set the Sensor capabilities. Al fields must be initilized.
         * @param cap pointer to SensorCapability struct.
         */
        void SetSpectrographCapability(uint32_t cap);

        typedef enum
        {
            SPECTROGRAPH_GAIN,
            SPECTROGRAPH_FREQUENCY,
            SPECTROGRAPH_BANDWIDTH,
            SPECTROGRAPH_BITSPERSAMPLE,
            SPECTROGRAPH_SAMPLERATE,
            SPECTROGRAPH_CHANNEL,
            SPECTROGRAPH_ANTENNA,
            SPECTROGRAPH_FWHM,
        } SPECTROGRAPH_INFO_INDEX;

        void Spectrum(void *buf, void *out, int n_elements, int size, int bits_per_sample);
        void Histogram(void *buf, void *out, int n_elements, int histogram_size, int bits_per_sample);
        void FourierTransform(void *buf, void *out, int dims, int *sizes, int bits_per_sample);
        void Convolution(void *buf, void *matrix, void *out, int dims, int *sizes, int matrix_dims, int *matrix_sizes, int bits_per_sample);
        void WhiteNoise(void *buf, int n_elements, int bits_per_sample);
private:
        double Samplerate;
        double Frequency;
        double Bandwidth;
        double Gain;
        INumberVectorProperty SpectrographSettingsNP;
        INumber SpectrographSettingsN[5];

};
}
