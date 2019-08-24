/*
 QHY INDI Driver

 Copyright (C) 2014 Jasem Mutlaq (mutlaqja@ikarustech.com)
 Copyright (C) 2014 Zhirong Li (lzr@qhyccd.com)

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 This library is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 Lesser General Public License for more details.

 You should have received a copy of the GNU Lesser General Public
 License along with this library; if not, write to the Free Software
 Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#pragma once

#include <qhyccd.h>
#include <indiccd.h>
#include <indifilterinterface.h>
#include <unistd.h>
#include <functional>
#include <pthread.h>

#define DEVICE struct usb_device *

class QHYCCD : public INDI::CCD, public INDI::FilterInterface
{
    public:
        QHYCCD(const char *name);
        virtual ~QHYCCD() override = default;

        virtual const char *getDefaultName() override;

        virtual bool initProperties() override;
        virtual void ISGetProperties(const char *dev) override;
        virtual bool updateProperties() override;

        virtual bool Connect() override;
        virtual bool Disconnect() override;

        virtual int SetTemperature(double temperature) override;
        virtual bool StartExposure(float duration) override;
        virtual bool AbortExposure() override;

        virtual void debugTriggered(bool enable) override;

        virtual bool ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n) override;
        virtual bool ISNewText(const char *dev, const char *name, char *texts[], char *names[], int num) override;
        virtual bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n) override;

    protected:
        // Misc.
        virtual void TimerHit() override;
        virtual bool saveConfigItems(FILE *fp) override;

        // CCD
        virtual bool UpdateCCDFrame(int x, int y, int w, int h) override;
        virtual bool UpdateCCDBin(int binx, int biny) override;

        // Guide Port
        virtual IPState GuideNorth(uint32_t ms) override;
        virtual IPState GuideSouth(uint32_t ms) override;
        virtual IPState GuideEast(uint32_t ms) override;
        virtual IPState GuideWest(uint32_t ms) override;

        // Filter Wheel CFW
        virtual int QueryFilter() override;
        virtual bool SelectFilter(int position) override;

        // Streaming
        virtual bool StartStreaming() override;
        virtual bool StopStreaming() override;

        void addFITSKeywords(fitsfile *fptr, INDI::CCDChip *targetChip) override;

        ISwitch CoolerS[2];
        ISwitchVectorProperty CoolerSP;
        enum
        {
            COOLER_ON,
            COOLER_OFF,
        };

        INumber CoolerN[1];
        INumberVectorProperty CoolerNP;

        INumber GainN[1];
        INumberVectorProperty GainNP;

        INumber OffsetN[1];
        INumberVectorProperty OffsetNP;

        INumber SpeedN[1];
        INumberVectorProperty SpeedNP;

        INumber ReadModeN[1];
        INumberVectorProperty ReadModeNP;

        INumber USBTrafficN[1];
        INumberVectorProperty USBTrafficNP;

        ISwitchVectorProperty CoolerModeSP;
        ISwitch CoolerModeS[2];
        enum
        {
            COOLER_AUTOMATIC,
            COOLER_MANUAL,
        };

    private:
        typedef enum ImageState
        {
            StateNone = 0,
            StateIdle,
            StateStream,
            StateExposure,
            StateRestartExposure,
            StateAbort,
            StateTerminate,
            StateTerminated
        } ImageState;

        /* Imaging functions */
        static void *imagingHelper(void *context);
        void *imagingThreadEntry();
        void streamVideo();
        void getExposure();
        void exposureSetRequest(ImageState request);

        // Get time until next image is due
        double calcTimeLeft();
        // Get image buffer from camera
        int grabImage();
        // Setup basic CCD parameters on connection
        bool setupParams();
        // Enable/disable cooler
        void setCoolerEnabled(bool enable);
        // Set Cooler Mode
        void setCoolerMode(uint8_t mode);
        // Check if the camera is QHY5PII-C model
        bool isQHY5PIIC();
        // Call when max filter count is known
        bool updateFilterProperties();

        // Temperature update
        void updateTemperature();
        static void updateTemperatureHelper(void *);

        char name[MAXINDIDEVICE];
        char camid[MAXINDIDEVICE];

        // CCD extra capabilities
        bool HasUSBTraffic { false };
        bool HasUSBSpeed { false };
        bool HasGain { false };
        bool HasOffset { false };
        bool HasFilters { false };
        bool HasTransferBit { false };
        bool HasCoolerAutoMode { false };
        bool HasCoolerManualMode { false };
        bool HasReadMode { false };

        qhyccd_handle *m_CameraHandle {nullptr};
        INDI::CCDChip::CCD_FRAME m_ImageFrameType;

        // Requested target temperature
        double m_TemperatureRequest {0};
        // Requested target PWM
        double m_PWMRequest { -1 };
        // Max filter count.
        int m_MaxFilterCount { -1 };
        // Temperature Timer
        int m_TemperatureTimerID;

        // Exposure progress
        double m_ExposureRequest;
        // Last exposure request in microseconds
        uint32_t m_LastExposureRequestuS;
        struct timeval ExpStart;

        // Gain
        double GainRequest = 1e6;
        double LastGainRequest = 1e6;

        // Filter Wheel Timeout
        uint16_t m_FilterCheckCounter = 0;

        // Threading
        // Imaging thread
        ImageState threadRequest;
        ImageState threadState;
        pthread_t imagingThread;
        pthread_cond_t cv         = PTHREAD_COND_INITIALIZER;
        pthread_mutex_t condMutex = PTHREAD_MUTEX_INITIALIZER;

        void logQHYMessages(const std::string &message);
        std::function<void(const std::string &)> m_QHYLogCallback;

        friend void ::ISGetProperties(const char *dev);
        friend void ::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int num);
        friend void ::ISNewText(const char *dev, const char *name, char *texts[], char *names[], int num);
        friend void ::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int num);
        friend void ::ISNewBLOB(const char *dev, const char *name, int sizes[], int blobsizes[], char *blobs[],
                                char *formats[], char *names[], int n);
        friend void ::ISSnoopDevice(XMLEle *root);
};
