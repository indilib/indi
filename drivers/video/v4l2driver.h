#if 0
V4L2 INDI Driver
INDI Interface for V4L2 devices
Copyright (C) 2003 - 2005 Jasem Mutlaq (mutlaqja@ikarustech.com)
    Copyright (C) 2013 Geehalel (geehalel@gmail.com)

    This library is free software;
you can redistribute it and / or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation;
either
version 2.1 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
     but WITHOUT ANY WARRANTY;
without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library;
if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110 - 1301  USA

#endif

#pragma once

#include "indiccd.h"
#include "webcam/v4l2_base.h"

#define IMAGE_CONTROL  "Image Control"
#define IMAGE_GROUP    "V4L2 Control"
#define IMAGE_BOOLEAN  "V4L2 Options"
#define CAPTURE_FORMAT "Capture Options"

#define MAX_PIXELS 4096 /* Max number of pixels in one dimension */
#define ERRMSGSIZ  1024

#define TEMPFILE_LEN 16

#ifdef __FreeBSD__
typedef unsigned long ulong;
#endif

class Lx;

// Pixel size info for different cameras
typedef struct PixelSizeInfo
{
    const char * deviceLabel; // Device label used by INDI
    const char * deviceName; // device name reported by V4L
    const char * commonName; // if null, use device name
    float pixelSizeX;
    float pixelSizeY; // if negative, use pixelSizeX also for Y
    uint32_t width;     // Default width, if 0 then don't set anything
    uint32_t height;    // Default height, if 0 then don't set anything
    bool tested; //if False print please report message
} PixelSizeInfo;

class V4L2_Driver : public INDI::CCD
{
public:
    V4L2_Driver();
        explicit V4L2_Driver(std::string label, std::string path);
        virtual ~V4L2_Driver() override;

        /* INDI Functions that must be called from indidrivermain */
        virtual void ISGetProperties(const char *dev) override;
        virtual bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n) override;
        virtual bool ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n) override;
        virtual bool ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n) override;

        virtual bool initProperties() override;
        virtual bool updateProperties() override;
        virtual void initCamBase();

        static void newFrame(void *p);
        void stackFrame();
        void newFrame();

    protected:
        virtual bool Connect() override;
        virtual bool Disconnect() override;

        virtual const char *getDefaultName() override;
        virtual bool StartExposure(float duration) override;
        virtual bool AbortExposure() override;
        virtual bool UpdateCCDFrame(int x, int y, int w, int h) override;
        virtual bool UpdateCCDBin(int hor, int ver) override;
        virtual bool SetCaptureFormat(uint8_t index) override;
        virtual bool saveConfigItems(FILE *fp) override;

        virtual bool StartStreaming() override;
        virtual bool StopStreaming() override;

        bool updateCaptureSize(uint32_t width, uint32_t height);

        /* Structs */
        typedef struct
        {
            int width;
            int height;
            int bpp;
            //int  expose;
            double expose;
            unsigned char *Y;
            unsigned char *U;
            unsigned char *V;
            unsigned char *RGB24Buffer;
            unsigned char *compressedFrame;
            float *stackedFrame;
            float *darkFrame;
        } img_t;

        enum
        {
            IMAGE_MONO = 0,
            IMAGE_RGB
        };

        enum stackmodes
        {
            STACK_NONE       = 0,
            STACK_MEAN       = 1,
            STACK_ADDITIVE   = 2,
            STACK_TAKE_DARK  = 3,
            STACK_RESET_DARK = 4
        };

        /* Switches */

        ISwitch ImageDepthS[2];
        ISwitch ColorProcessingS[3];

        /* Texts */
        IText PortT[1] {};
        IText camNameT[1] {};
        IText CaptureColorSpaceT[3] {};

        /* Numbers */
        //INumber *ExposeTimeN;
        INumber *FrameN;
        INumber FrameRateN[1];

        /* Switch vectors */
        ISwitchVectorProperty ImageDepthSP;     /* 8 bits or 16 bits switch */
        INDI::PropertySwitch  StackModeSP {5};  /* StackMode switch */
        ISwitchVectorProperty InputsSP;         /* Select input switch */
        ISwitchVectorProperty CaptureFormatsSP; /* Select Capture format switch */
        ISwitchVectorProperty CaptureSizesSP;   /* Select Capture size switch (Discrete)*/
        ISwitchVectorProperty FrameRatesSP;     /* Select Frame rate (Discrete) */
        ISwitchVectorProperty *Options;
        ISwitchVectorProperty ColorProcessingSP;

        unsigned int v4loptions;
        unsigned int v4ladjustments;
        bool useExtCtrl;

        /* Number vectors */
        //INumberVectorProperty *ExposeTimeNP;			/* Exposure */
        INumberVectorProperty CaptureSizesNP; /* Select Capture size switch (Step/Continuous)*/
        INumberVectorProperty FrameRateNP;    /* Frame rate (Step/Continuous) */
        INumberVectorProperty ImageAdjustNP;  /* Image controls */

        /* Text vectors */
        ITextVectorProperty PortTP;
        ITextVectorProperty camNameTP;
        ITextVectorProperty CaptureColorSpaceTP;

        /* Pointers to optional properties */
        INumber *AbsExposureN;
        ISwitchVectorProperty *ManualExposureSP;

        /* Initialization functions */
        //virtual void connectCamera();
        virtual void getBasicData();
        bool getPixelFormat(uint32_t v4l2format, INDI_PIXEL_FORMAT &pixelFormat, uint8_t &pixelDepth);
        void allocateBuffers();
        void releaseBuffers();
        void updateFrameSize();

        /* Shutter control */
        bool setShutter(double duration);
        bool setManualExposure(double duration);
        bool startlongexposure(double timeinsec);
        static void lxtimerCallback(void *userpointer);
        static void stdtimerCallback(void *userpointer);

        /* start/stop functions */
        bool start_capturing(bool do_stream);
        bool stop_capturing();

        virtual void updateV4L2Controls();

        /* Variables */
        INDI::V4L2_Base *v4l_base;

        char device_name[MAXINDIDEVICE];

        int subframeCount; /* For stacking */
        int frameCount;
        double divider;  /* For limits */
        img_t *V4LFrame; /* Video frame */

        struct timeval capture_start; /* To calculate how long a frame take */

        //struct timeval capture_end;

        struct timeval frame_duration;
        struct timeval frame_received;

        struct timeval exposure_duration;
        struct timeval elapsed_exposure;

        struct timeval getElapsedExposure() const;
        float getRemainingExposure() const;

        unsigned int m_StackMode;
        ulong frameBytes;
        unsigned int non_capture_frames;
        bool v4l_capture_started;
        bool is_capturing;
        bool is_exposing;

        //Long Exposure
        Lx *lx;
        int lxtimer;
        int stdtimer;

        short lxstate;
        PixelSizeInfo * m_Info {nullptr};

        char defaultVideoPort[256] = {"/dev/video0"};
        char configPort[256] = {0};
        bool waiting_for_iguider_to_start;
};
