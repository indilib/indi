/*
 ATIK CCD & Filter Wheel Driver

 Copyright (C) 2018 Jasem Mutlaq (mutlaqja@ikarustech.com)

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

#include <AtikCameras.h>

#include <indifilterinterface.h>
#include <indiccd.h>

class ATIKCCD : public INDI::CCD, public INDI::FilterInterface
{
    public:
        explicit ATIKCCD(std::string cameraName, int id);
        ~ATIKCCD() override = default;

        virtual const char *getDefaultName() override;

        virtual bool initProperties() override;
        virtual bool updateProperties() override;

        virtual bool Connect() override;
        virtual bool Disconnect() override;

        virtual int SetTemperature(double temperature) override;
        virtual bool StartExposure(float duration) override;
        virtual bool AbortExposure() override;

        static void debugCallbackHelper(void *context, const char *message);

    protected:
        virtual bool ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n) override;
        virtual bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n) override;
        virtual bool ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n) override;

        virtual void TimerHit() override;
        virtual bool UpdateCCDFrame(int x, int y, int w, int h) override;
        virtual bool UpdateCCDBin(int binx, int biny) override;

        // Guide Port
        virtual IPState GuideNorth(uint32_t ms) override;
        virtual IPState GuideSouth(uint32_t ms) override;
        virtual IPState GuideEast(uint32_t ms) override;
        virtual IPState GuideWest(uint32_t ms) override;

        // Atik specific keywords
        virtual void addFITSKeywords(fitsfile *fptr, INDI::CCDChip *targetChip) override;

        // Save config
        virtual bool saveConfigItems(FILE *fp) override;

        // Filter wheel
        virtual bool SelectFilter(int) override;
        virtual int QueryFilter() override;

        // Debug
        virtual void debugTriggered(bool enable) override;

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

        // Atik Horizon IDs
        enum
        {
            ID_AtikHorizonGOPresetMode = 1,
            ID_AtikHorizonGOPresetLow,
            ID_AtikHorizonGOPresetMed,
            ID_AtikHorizonGOPresetHigh,
            ID_AtikHorizonGOCustomGain,
            ID_AtikHorizonGOCustomOffset,
        };

        typedef enum AtikGuideDirection
        {
            NORTH,
            SOUTH,
            EAST,
            WEST
        } AtikGuideDirection;

        // Image Threading
        static void *imagingHelper(void *context);
        void *imagingThreadEntry();

        // Debug
        void debugCallback(const char *message);

        // Exposure Progress
        void checkExposureProgress();
        void exposureSetRequest(ImageState request);

        // Guiding
        static void TimerHelperNS(void *context);
        static void TimerHelperWE(void *context);
        void stopTimerNS();
        void stopTimerWE();
        IPState guidePulseNS(uint32_t ms, AtikGuideDirection dir, const char *dirName);
        IPState guidePulseWE(uint32_t ms, AtikGuideDirection dir, const char *dirName);

        // Retrieve image from SDK
        bool grabImage();

        /**
         * @brief setupParams get initial camera parameters
         */
        bool setupParams();

        /**
         * @brief activateCooler Turn on/off cooler
         * @param enable True to turn on (not supported). Off to warm up
         * @return True if successful.
         */
        bool activateCooler(bool enable);

        // Cooler power
        INumber CoolerN[1];
        INumberVectorProperty CoolerNP;

        // Cooler switch control
        ISwitch CoolerS[2];
        ISwitchVectorProperty CoolerSP;
        enum
        {
            COOLER_ON,
            COOLER_OFF,
        };

        // Gain & Offset Custom Properties
        INumber ControlN[2];
        INumberVectorProperty ControlNP;
        enum
        {
            CONTROL_GAIN,
            CONTROL_OFFSET,
        };

        // Gain & Offset Presets
        ISwitch ControlPresetsS[4];
        ISwitchVectorProperty ControlPresetsSP;
        enum
        {
            PRESET_CUSTOM,
            PRESET_LOW,
            PRESET_MEDIUM,
            PRESET_HIGH,
        };


        // API & Firmware Version
        IText VersionInfoS[2] = {};
        ITextVectorProperty VersionInfoSP;
        enum
        {
            VERSION_API,
            VERSION_FIRMWARE,
        };


        struct timeval ExpStart;
        double ExposureRequest { 0 };
        double TemperatureRequest { 1e6 };
        int genTimerID {-1};

        // Imaging thread
        ImageState threadRequest;
        ImageState threadState;
        pthread_t imagingThread;
        pthread_cond_t cv         = PTHREAD_COND_INITIALIZER;
        pthread_mutex_t condMutex = PTHREAD_MUTEX_INITIALIZER;
        pthread_mutex_t accessMutex = PTHREAD_MUTEX_INITIALIZER;

        // Pulse Guiding
        int WEtimerID;
        int NStimerID;
        AtikGuideDirection WEDir;
        AtikGuideDirection NSDir;
        const char *WEDirName;
        const char *NSDirName;

        // Camera info
        ArtemisHandle hCam { nullptr };
        int m_iDevice {-1};

        // Gain/Offset & Preview
        bool m_isHorizon { false };
        int m_CameraFlags {0};

        // Offsets
        int normalOffsetX {0}, normalOffsetY {0};
        int previewOffsetX {0}, previewOffsetY {0};

        // Temperature Sensors
        int m_TemperatureSensorsCount {0};

        char name[MAXINDIDEVICE];

        friend void ::ISGetProperties(const char *dev);
        friend void ::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int num);
        friend void ::ISNewText(const char *dev, const char *name, char *texts[], char *names[], int num);
        friend void ::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int num);
        friend void ::ISNewBLOB(const char *dev, const char *name, int sizes[], int blobsizes[], char *blobs[],
                                char *formats[], char *names[], int n);

        static constexpr const char *CONTROLS_TAB = "Controls";
};
