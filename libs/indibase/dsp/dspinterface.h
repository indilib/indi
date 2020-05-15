/*******************************************************************************
  Copyright(c) 2017 Ilia Platone, Jasem Mutlaq. All rights reserved.

 DSP plugin Interface

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

#include "indidevapi.h"
#include "dsp.h"

#include <fitsio.h>
#include <functional>
#include <string>

namespace INDI
{
class DefaultDevice;
}

namespace DSP
{
/**
 * @namespace DSP
 * @brief The DSP Namespace adds signal processing to INDI drivers. Primarily written for sensors and detectors, it can be used also
 * for CCDs. This namespace includes buffer transformations, convolution and signal filters, like bandpass and wavelets.
 */
extern const char *DSP_TAB;
class Interface
{
    /**
     * @brief The Interface class is the base class for the DSP plugins of INDI.
     *
     * The plugins must implement the Callback, which will be called by the DSP::Manager class. All the DSP
     * plugins work multidimensionally, so even for single dimensional streams the meaning is to add a single dimension.
     * Initially classes calls set dimensions and sizes the same as the original picture or frame, later or during processing
     * into the Callback functions the plugins can alter or set dimensions arbitrarily.
     * The plugins return an array which can be of various depths and a BLOB will be generated and sent to the client with the
     * result.
     * The DSP, when enabled by the property xxx_HAS_DSP, will generate properties for activation of the single plugins, and
     * after activation or deactivation, Activate() and Deactivate() methods will be called permitting further properties etc.
     * getSizes()/setSizes() will help within Callback to alter dimensions and sizes, getBPS/setBPS will change color depth or
     * sample size.
     * Classes that use the Interface class children should call processBLOB to propagate until children's Callback methods and
     * generate BLOBs.
     *
     * @see DSP::Convolution
     * @see DSP::Transforms
     * @see DSP::Manager
     * @see INDI::CCD and INDI::SensorInterface utilize the DSP Namespace.
     */
    public:
        /**
         * \struct Type
         * \brief Holds the process type
         */
        typedef enum  {
            DSP_NONE = 0,
            DSP_TRANSFORMATIONS,
            DSP_CONVOLUTION,
            DSP_SPECTRUM,
        } Type;

        virtual void ISGetProperties(const char *dev);
        virtual bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n);
        virtual bool ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n);
        virtual bool ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n);
        virtual bool ISNewBLOB(const char *dev, const char *name, int sizes[], int blobsizes[], char *blobs[], char *formats[], char *names[], int n);
        virtual bool saveConfigItems(FILE *fp);
        virtual bool updateProperties();

        /**
         * @brief processBLOB Propagate to Callback and generate BLOBs for parent device.
         * @param buf The input buffer
         * @param ndims Number of the dimensions of the input buffer
         * @param dims Sizes of the dimensions of the input buffer
         * @param bits_per_sample original bit depth of the input buffer
         * @return True if successful, false otherwise.
         */
        bool processBLOB(uint8_t* buf, uint32_t ndims, int* dims, int bits_per_sample);

        /**
         * @brief setSizes Set the returned file dimensions and corresponding sizes.
         * @param num Number of dimensions.
         * @param sizes Sizes of the dimensions.
         */
        void setSizes(uint32_t num, int* sizes) { BufferSizes = sizes; BufferSizesQty = num; }

        /**
         * @brief getSizes Get the returned file dimensions and corresponding sizes.
         * @param num Number of dimensions.
         * @param sizes Sizes of the dimensions.
         */
        void getSizes(uint32_t *num, int** sizes) { *sizes = BufferSizes; *num = BufferSizesQty; }

        /**
         * @brief setBPS Set the returned file bit depth/sample size.
         * @param bps Bit depth / sample size.
         */
        void setBPS(int bps) { BPS = bps; }

        /**
         * @brief getBPS Get the returned file bit depth/sample size.
         * @return Bit depth / sample size.
         */
        int getBPS() { return BPS; }

    protected:

        /**
         * @brief Activated Called after activation from client application.
         */
        virtual void Activated();

        /**
         * @brief Deactivated Called after deactivation from client application.
         */
        virtual void Deactivated();

        /**
         * @brief Callback Called by processBLOB.
         * @param buf The input buffer
         * @param ndims Number of the dimensions of the input buffer
         * @param dims Sizes of the dimensions of the input buffer
         * @param bits_per_sample original bit depth of the input buffer
         * @return The processed buffer
         */
        virtual uint8_t* Callback(uint8_t* buf, uint32_t ndims, int* dims, int bits_per_sample);

        /**
         * @brief loadFITS Converts FITS data into a dsp_stream structure pointer.
         * @param buf The input buffer
         * @param len Size of the FIT in bytes
         * @return A dsp_stream_p structure containing the data of the loaded FITS
         */
        dsp_stream_p loadFITS(char* buf, int len);

        bool PluginActive;

        IBLOBVectorProperty FitsBP;
        IBLOB FitsB;

        ISwitchVectorProperty ActivateSP;
        ISwitch ActivateS[2];

        //  We are going to snoop these from a telescope
        INumberVectorProperty EqNP;
        INumber EqN[2];

        ITextVectorProperty ActiveDeviceTP;
        IText ActiveDeviceT[4] {};

        Interface(INDI::DefaultDevice *dev, Type type = DSP_NONE, const char *name = "DSP_PLUGIN", const char *label = "DSP Plugin");
        virtual ~Interface();

        const char *getDeviceName();
        INDI::DefaultDevice *m_Device {  nullptr };
        const char *m_Name {  nullptr };
        const char *m_Label {  nullptr };
        Type m_Type {  DSP_NONE };
        void setStream(void *buf, uint32_t dims, int *sizes, int bits_per_sample);
        uint8_t *getStream();
        dsp_stream_p stream;

    private:
        uint32_t BufferSizesQty;
        int *BufferSizes;
        int BPS;

        char processedFileName[MAXINDINAME];
        void fits_update_key_s(fitsfile *fptr, int type, std::string name, void *p, std::string explanation, int *status);
        void addFITSKeywords(fitsfile *fptr);
        bool sendFITS(uint8_t *buf, bool sendCapture, bool saveCapture);
        bool uploadFile(const void *fitsData, size_t totalBytes, bool sendIntegration, bool saveIntegration, const char* format);
        int getFileIndex(const char *dir, const char *prefix, const char *ext);
};
}
