/*
 INDI Altair Driver

 Copyright (C) 2018-2019 Jasem Mutlaq (mutlaqja@ikarustech.com)

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

#include <map>
#include <indiccd.h>

#ifdef BUILD_TOUPCAM
#include <toupcam.h>
#define FP(x) Toupcam_##x
#define CP(x) TOUPCAM_##x
#define XP(x) Toupcam##x
#define THAND HToupCam
#define DNAME "Toupcam"
#elif BUILD_ALTAIRCAM
#include <altaircam.h>
#define FP(x) Altaircam_##x
#define CP(x) ALTAIRCAM_##x
#define XP(x) Altaircam##x
#define THAND HAltairCam
#define DNAME "Altair"
#elif BUILD_STARSHOOTG
#include <starshootg.h>
#define FP(x) Starshootg_##x
#define CP(x) STARSHOOTG_##x
#define XP(x) Starshootg##x
#define THAND HStarshootG
#define DNAME "StarshootG"
#elif BUILD_NNCAM
#include <nncam.h>
#define FP(x) Nncam_##x
#define CP(x) NNCAM_##x
#define XP(x) Nncam##x
#define THAND HNnCam
#define DNAME "Levenhuk"
#endif

#define RAW_SUPPORTED   (CP(FLAG_RAW10) | CP(FLAG_RAW12) | CP(FLAG_RAW14) | CP(FLAG_RAW16))

typedef unsigned long   ulong;            /* Short for unsigned long */

class ToupBase : public INDI::CCD
{
    public:
        explicit ToupBase(const XP(InstV2) *instance);
        ~ToupBase() override = default;

        virtual const char *getDefaultName() override;

        virtual bool initProperties() override;
        virtual bool updateProperties() override;

        virtual bool Connect() override;
        virtual bool Disconnect() override;

        virtual int SetTemperature(double temperature) override;
        virtual bool StartExposure(float duration) override;
        virtual bool AbortExposure() override;

    protected:
        virtual bool ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n) override;
        virtual bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n) override;

        // Streaming
        virtual bool StartStreaming() override;
        virtual bool StopStreaming() override;

        virtual void TimerHit() override;
        virtual bool UpdateCCDFrame(int x, int y, int w, int h) override;
        virtual bool UpdateCCDBin(int binx, int biny) override;

        // Guide Port
        virtual IPState GuideNorth(uint32_t ms) override;
        virtual IPState GuideSouth(uint32_t ms) override;
        virtual IPState GuideEast(uint32_t ms) override;
        virtual IPState GuideWest(uint32_t ms) override;

        // ASI specific keywords
        virtual void addFITSKeywords(fitsfile *fptr, INDI::CCDChip *targetChip) override;

        // Save config
        virtual bool saveConfigItems(FILE *fp) override;

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

        enum
        {
            S_OK            = 0x00000000,
            S_FALSE         = 0x00000001,
            E_FAIL          = 0x80004005,
            E_INVALIDARG    = 0x80070057,
            E_NOTIMPL       = 0x80004001,
            E_NOINTERFACE   = 0x80004002,
            E_POINTER       = 0x80004003,
            E_UNEXPECTED    = 0x8000FFFF,
            E_OUTOFMEMORY   = 0x8007000E,
            E_WRONG_THREAD  = 0x8001010E,
        };
        static std::map<int, std::string> errorCodes;


        enum eFLAG
        {
            FLAG_CMOS                = 0x00000001,   /* cmos sensor */
            FLAG_CCD_PROGRESSIVE     = 0x00000002,   /* progressive ccd sensor */
            FLAG_CCD_INTERLACED      = 0x00000004,   /* interlaced ccd sensor */
            FLAG_ROI_HARDWARE        = 0x00000008,   /* support hardware ROI */
            FLAG_MONO                = 0x00000010,   /* monochromatic */
            FLAG_BINSKIP_SUPPORTED   = 0x00000020,   /* support bin/skip mode */
            FLAG_USB30               = 0x00000040,   /* usb3.0 */
            FLAG_TEC                 = 0x00000080,   /* Thermoelectric Cooler */
            FLAG_USB30_OVER_USB20    = 0x00000100,   /* usb3.0 camera connected to usb2.0 port */
            FLAG_ST4                 = 0x00000200,   /* ST4 */
            FLAG_GETTEMPERATURE      = 0x00000400,   /* support to get the temperature of the sensor */
            FLAG_PUTTEMPERATURE      = 0x00000800,   /* support to put the target temperature of the sensor */
            FLAG_RAW10               = 0x00001000,   /* pixel format, RAW 10bits */
            FLAG_RAW12               = 0x00002000,   /* pixel format, RAW 12bits */
            FLAG_RAW14               = 0x00004000,   /* pixel format, RAW 14bits*/
            FLAG_RAW16               = 0x00008000,   /* pixel format, RAW 16bits */
            FLAG_FAN                 = 0x00010000,   /* cooling fan */
            FLAG_TEC_ONOFF           = 0x00020000,   /* Thermoelectric Cooler can be turn on or off, support to set the target temperature of TEC */
            FLAG_ISP                 = 0x00040000,   /* ISP (Image Signal Processing) chip */
            FLAG_TRIGGER_SOFTWARE    = 0x00080000,   /* support software trigger */
            FLAG_TRIGGER_EXTERNAL    = 0x00100000,   /* support external trigger */
            FLAG_TRIGGER_SINGLE      = 0x00200000,   /* only support trigger single: one trigger, one image */
            FLAG_BLACKLEVEL          = 0x00400000,   /* support set and get the black level */
            FLAG_AUTO_FOCUS          = 0x00800000,   /* support auto focus */
            FLAG_BUFFER              = 0x01000000,   /* frame buffer */
            FLAG_DDR                 = 0x02000000,   /* use very large capacity DDR (Double Data Rate SDRAM) for frame buffer */
            FLAG_CG                  = 0x04000000,   /* support Conversion Gain mode: HCG, LCG */
            FLAG_YUV411              = 0x08000000,   /* pixel format, yuv411 */
            FLAG_VUYY                = 0x10000000,   /* pixel format, yuv422, VUYY */
            FLAG_YUV444              = 0x20000000,   /* pixel format, yuv444 */
            FLAG_RGB888              = 0x40000000,   /* pixel format, RGB888 */
            FLAG_RAW8                = 0x80000000,   /* pixel format, RAW 8 bits */
            FLAG_GMCY8               = 0x0000000100000000,  /* pixel format, GMCY, 8 bits */
            FLAG_GMCY12              = 0x0000000200000000,  /* pixel format, GMCY, 12 bits */
            FLAG_UYVY                = 0x0000000400000000,  /* pixel format, yuv422, UYVY */
            FLAG_CGHDR               = 0x0000000800000000   /* Conversion Gain: HCG, LCG, HDR */
        };

        enum eEVENT
        {
            EVENT_EXPOSURE             = 0x0001, /* exposure time changed */
            EVENT_TEMPTINT             = 0x0002, /* white balance changed, Temp/Tint mode */
            EVENT_CHROME               = 0x0003, /* reversed, do not use it */
            EVENT_IMAGE                = 0x0004, /* live image arrived, use Toupcam_PullImage to get this image */
            EVENT_STILLIMAGE           = 0x0005, /* snap (still) frame arrived, use Toupcam_PullStillImage to get this frame */
            EVENT_WBGAIN               = 0x0006, /* white balance changed, RGB Gain mode */
            EVENT_TRIGGERFAIL          = 0x0007, /* trigger failed */
            EVENT_BLACK                = 0x0008, /* black balance changed */
            EVENT_FFC                  = 0x0009, /* flat field correction status changed */
            EVENT_DFC                  = 0x000a, /* dark field correction status changed */
            EVENT_ERROR                = 0x0080, /* generic error */
            EVENT_DISCONNECTED         = 0x0081, /* camera disconnected */
            EVENT_TIMEOUT              = 0x0082, /* timeout error */
            EVENT_FACTORY              = 0x8001  /* restore factory settings */
        };

        enum ePROCESSMODE
        {
            PROCESSMODE_FULL = 0x00, /* better image quality, more cpu usage. this is the default value */
            PROCESSMODE_FAST = 0x01 /* lower image quality, less cpu usage */
        };

        enum eOPTION
        {
            OPTION_NOFRAME_TIMEOUT = 0x01,    /* 1 = enable; 0 = disable. default: disable */
            OPTION_THREAD_PRIORITY = 0x02,    /* set the priority of the internal thread which grab data from the usb device. iValue: 0 = THREAD_PRIORITY_NORMAL; 1 = THREAD_PRIORITY_ABOVE_NORMAL; 2 = THREAD_PRIORITY_HIGHEST; default: 0; see: msdn SetThreadPriority */
            OPTION_PROCESSMODE     = 0x03,    /* 0 = better image quality, more cpu usage. this is the default value
                                                     1 = lower image quality, less cpu usage */
            OPTION_RAW             = 0x04,    /* raw data mode, read the sensor "raw" data. This can be set only BEFORE Toupcam_StartXXX(). 0 = rgb, 1 = raw, default value: 0 */
            OPTION_HISTOGRAM       = 0x05,    /* 0 = only one, 1 = continue mode */
            OPTION_BITDEPTH        = 0x06,    /* 0 = 8 bits mode, 1 = 16 bits mode */
            OPTION_FAN             = 0x07,    /* 0 = turn off the cooling fan, [1, max] = fan speed */
            OPTION_TEC             = 0x08,    /* 0 = turn off the thermoelectric cooler, 1 = turn on the thermoelectric cooler */
            OPTION_LINEAR          = 0x09,    /* 0 = turn off the builtin linear tone mapping, 1 = turn on the builtin linear tone mapping, default value: 1 */
            OPTION_CURVE           = 0x0a,    /* 0 = turn off the builtin curve tone mapping, 1 = turn on the builtin polynomial curve tone mapping, 2 = logarithmic curve tone mapping, default value: 2 */
            OPTION_TRIGGER         = 0x0b,    /* 0 = video mode, 1 = software or simulated trigger mode, 2 = external trigger mode, default value = 0 */
            OPTION_RGB             = 0x0c,    /* 0 => RGB24; 1 => enable RGB48 format when bitdepth > 8; 2 => RGB32; 3 => 8 Bits Gray (only for mono camera); 4 => 16 Bits Gray (only for mono camera when bitdepth > 8) */
            OPTION_COLORMATIX      = 0x0d,    /* enable or disable the builtin color matrix, default value: 1 */
            OPTION_WBGAIN          = 0x0e,    /* enable or disable the builtin white balance gain, default value: 1 */
            OPTION_TECTARGET       = 0x0f,    /* get or set the target temperature of the thermoelectric cooler, in 0.1 degree Celsius. For example, 125 means 12.5 degree Celsius, -35 means -3.5 degree Celsius */
            OPTION_AGAIN           = 0x10,    /* enable or disable adjusting the analog gain when auto exposure is enabled. default value: enable */
            OPTION_FRAMERATE       = 0x11,    /* limit the frame rate, range=[0, 63], the default value 0 means no limit */
            OPTION_DEMOSAIC        = 0x12,    /* demosaic method for both video and still image: BILINEAR = 0, VNG(Variable Number of Gradients interpolation) = 1, PPG(Patterned Pixel Grouping interpolation) = 2, AHD(Adaptive Homogeneity-Directed interpolation) = 3, see https://en.wikipedia.org/wiki/Demosaicing, default value: 0 */
            OPTION_DEMOSAIC_VIDEO  = 0x13,    /* demosaic method for video */
            OPTION_DEMOSAIC_STILL  = 0x14,    /* demosaic method for still image */
            OPTION_BLACKLEVEL      = 0x15,    /* black level */
            OPTION_MULTITHREAD     = 0x16,    /* multithread image processing */
            OPTION_BINNING         = 0x17,    /* binning, 0x01 (no binning), 0x02 (add, 2*2), 0x03 (add, 3*3), 0x04 (add, 4*4), 0x82 (average, 2*2), 0x83 (average, 3*3), 0x84 (average, 4*4) */
            OPTION_ROTATE          = 0x18,    /* rotate clockwise: 0, 90, 180, 270 */
            OPTION_CG              = 0x19,    /* Conversion Gain mode: 0 = LCG, 1 = HCG, 2 = HDR */
            OPTION_PIXEL_FORMAT    = 0x1a,    /* pixel format */
            OPTION_FFC             = 0x1b,    /* flat field correction
                                                        set:
                                                             0: disable
                                                             1: enable
                                                            -1: reset
                                                            (0xff000000 | n): set the average number to n, [1~255]
                                                        get:
                                                             (val & 0xff): 0 -> disable, 1 -> enable, 2 -> inited
                                                             ((val & 0xff00) >> 8): sequence
                                                             ((val & 0xff0000) >> 8): average number
                                                  */
            OPTION_DDR_DEPTH       = 0x1c,    /* the number of the frames that DDR can cache
                                                         1: DDR cache only one frame
                                                         0: Auto:
                                                                 ->one for video mode when auto exposure is enabled
                                                                 ->full capacity for others
                                                         1: DDR can cache frames to full capacity
                                                  */
            OPTION_DFC             = 0x1d,    /* dark field correction
                                                        set:
                                                             0: disable
                                                             1: enable
                                                            -1: reset
                                                            (0xff000000 | n): set the average number to n, [1~255]
                                                        get:
                                                             (val & 0xff): 0 -> disable, 1 -> enable, 2 -> inited
                                                             ((val & 0xff00) >> 8): sequence
                                                             ((val & 0xff0000) >> 8): average number
                                                  */
            OPTION_SHARPENING      = 0x1e,    /* Sharpening: (threshold << 24) | (radius << 16) | strength)
                                                            strength: [0, 500], default: 0 (disable)
                                                            radius: [1, 10]
                                                            threshold: [0, 255]
                                                  */
            OPTION_FACTORY         = 0x1f,    /* restore the factory settings */
            OPTION_TEC_VOLTAGE     = 0x20,    /* get the current TEC voltage in 0.1V, 59 mean 5.9V; readonly */
            OPTION_TEC_VOLTAGE_MAX = 0x21,    /* get the TEC maximum voltage in 0.1V; readonly */
            OPTION_DEVICE_RESET    = 0x22     /* reset usb device, simulate a replug */
        };

        enum eGUIDEDIRECTION
        {
            TOUPBASE_NORTH,
            TOUPBASE_SOUTH,
            TOUPBASE_EAST,
            TOUPBASE_WEST,
            TOUPBASE_STOP,
        };

        enum ePIXELFORMAT
        {
            PIXELFORMAT_RAW8       = 0x00,
            PIXELFORMAT_RAW10      = 0x01,
            PIXELFORMAT_RAW12      = 0x02,
            PIXELFORMAT_RAW14      = 0x03,
            PIXELFORMAT_RAW16      = 0x04,
            PIXELFORMAT_YUV411     = 0x05,
            PIXELFORMAT_VUYY       = 0x06,
            PIXELFORMAT_YUV444     = 0x07,
            PIXELFORMAT_RGB888     = 0x08,
            PIXELFORMAT_GMCY8      = 0x09,
            PIXELFORMAT_GMCY12     = 0x0a,
            PIXELFORMAT_UYVY       = 0x0b
        };

        enum eTriggerMode
        {
            TRIGGER_VIDEO,
            TRIGGER_SOFTWARE,
            TRIGGER_EXTERNAL,
        };

        struct Resolution
        {
            uint width;
            uint height;
        };

        struct ModelV2
        {
            std::string name;
            ulong flag;
            uint maxspeed;
            uint preview;
            uint still;
            uint maxfanspeed;
            uint ioctrol;
            float xpixsz;
            float ypixsz;
            XP(Resolution) res[CP(MAX)];
        };

        struct InstanceV2
        {
            std::string displayname; /* display name */
            std::string id; /* unique and opaque id of a connected camera */
            ModelV2 model;
        };

        struct FrameInfoV2
        {
            uint  width;
            uint  height;
            uint  flag;      /* FRAMEINFO_FLAG_xxxx */
            uint  seq;       /* sequence number */
            ulong timestamp; /* microsecond */
        };

        //#############################################################################
        // Capture
        //#############################################################################
        void allocateFrameBuffer();
        struct timeval ExposureEnd;
        double ExposureRequest;

        //#############################################################################
        // Video Format & Streaming
        //#############################################################################
        void getVideoImage();

        //#############################################################################
        // Guiding
        //#############################################################################
        // N/S Guiding
        static void TimerHelperNS(void *context);
        void TimerNS();
        void stopTimerNS();
        IPState guidePulseNS(uint32_t ms, eGUIDEDIRECTION dir, const char *dirName);
        struct timeval NSPulseEnd;
        int NStimerID;
        eGUIDEDIRECTION NSDir;
        const char *NSDirName;

        // W/E Guiding
        static void TimerHelperWE(void *context);
        void TimerWE();
        void stopTimerWE();
        IPState guidePulseWE(uint32_t ms, eGUIDEDIRECTION dir, const char *dirName);
        struct timeval WEPulseEnd;
        int WEtimerID;
        eGUIDEDIRECTION WEDir;
        const char *WEDirName;

        //#############################################################################
        // Temperature Control & Cooling
        //#############################################################################
        bool activateCooler(bool enable);
        double TemperatureRequest;

        //#############################################################################
        // Setup & Controls
        //#############################################################################
        // Get initial parameters from camera
        void setupParams();
        // Create number and switch controls for camera by querying the API
        void createControls(int piNumberOfControls);
        // Update control values from camera
        void refreshControls();

        //#############################################################################
        // Resolution
        //#############################################################################
        ISwitch ResolutionS[CP(MAX)];
        ISwitchVectorProperty ResolutionSP;

        //#############################################################################
        // Misc.
        //#############################################################################
        // Get the current Bayer string used
        const char *getBayerString();

        //#############################################################################
        // Callbacks
        //#############################################################################
        static void pushCB(const void* pData, const XP(FrameInfoV2)* pInfo, int bSnap, void* pCallbackCtx);
        void pushCallback(const void* pData, const XP(FrameInfoV2)* pInfo, int bSnap);

        static void eventCB(unsigned event, void* pCtx);
        void eventPullCallBack(unsigned event);

        static void TempTintCB(const int nTemp, const int nTint, void* pCtx);
        void TempTintChanged(const int nTemp, const int nTint);

        static void WhiteBalanceCB(const int aGain[3], void* pCtx);
        void WhiteBalanceChanged(const int aGain[3]);

        static void BlackBalanceCB(const unsigned short aSub[3], void* pCtx);
        void BlackBalanceChanged(const unsigned short aSub[3]);

        static void AutoExposureCB(void* pCtx);
        void AutoExposureChanged();

        //#############################################################################
        // Camera Handle & Instance
        //#############################################################################
        THAND m_CameraHandle { nullptr };
        const XP(InstV2) *m_Instance;
        // Camera Display Name
        char name[MAXINDIDEVICE];

        //#############################################################################
        // Properties
        //#############################################################################
        ISwitch CoolerS[2];
        ISwitchVectorProperty CoolerSP;
        enum
        {
            TC_COOLER_ON,
            TC_COOLER_OFF,
        };

        INumber ControlN[8];
        INumberVectorProperty ControlNP;
        enum
        {
            TC_GAIN,
            TC_CONTRAST,
            TC_HUE,
            TC_SATURATION,
            TC_BRIGHTNESS,
            TC_GAMMA,
            TC_SPEED,
            TC_FRAMERATE_LIMIT
        };

        ISwitch AutoControlS[3];
        ISwitchVectorProperty AutoControlSP;
        enum
        {
            TC_AUTO_TINT,
            TC_AUTO_WB,
            TC_AUTO_BB,
        };

        ISwitch AutoExposureS[2];
        ISwitchVectorProperty AutoExposureSP;
        enum
        {
            TC_AUTO_EXPOSURE_ON,
            TC_AUTO_EXPOSURE_OFF,
        };

        INumber BlackBalanceN[3];
        INumberVectorProperty BlackBalanceNP;
        enum
        {
            TC_BLACK_R,
            TC_BLACK_G,
            TC_BLACK_B,
        };

        // R/G/B/Gray low/high levels
        INumber LevelRangeN[8];
        INumberVectorProperty LevelRangeNP;
        enum
        {
            TC_LO_R,
            TC_HI_R,
            TC_LO_G,
            TC_HI_G,
            TC_LO_B,
            TC_HI_B,
            TC_LO_Y,
            TC_HI_Y,
        };

        // Temp/Tint White Balance
        INumber WBTempTintN[2];
        INumberVectorProperty WBTempTintNP;
        enum
        {
            TC_WB_TEMP,
            TC_WB_TINT,
        };

        // RGB White Balance
        INumber WBRGBN[3];
        INumberVectorProperty WBRGBNP;
        enum
        {
            TC_WB_R,
            TC_WB_G,
            TC_WB_B,
        };

        // Auto Balance Mode
        ISwitch WBAutoS[2];
        ISwitchVectorProperty WBAutoSP;
        enum
        {
            TC_AUTO_WB_TT,
            TC_AUTO_WB_RGB
        };

        // Fan control
        ISwitch FanControlS[2];
        ISwitchVectorProperty FanControlSP;
        enum
        {
            TC_FAN_ON,
            TC_FAN_OFF,
        };

        // Fan Speed
        ISwitch *FanSpeedS = nullptr;
        ISwitchVectorProperty FanSpeedSP;

        // Video Format
        ISwitch VideoFormatS[2];
        ISwitchVectorProperty VideoFormatSP;
        enum
        {
            TC_VIDEO_COLOR_RGB,
            TC_VIDEO_COLOR_RAW,
        };
        enum
        {

            TC_VIDEO_MONO_8,
            TC_VIDEO_MONO_16,
        };

        // Firmware Info
        IText FirmwareT[5] = {};
        ITextVectorProperty FirmwareTP;
        enum
        {
            TC_FIRMWARE_SERIAL,
            TC_FIRMWARE_SW_VERSION,
            TC_FIRMWARE_HW_VERSION,
            TC_FIRMWARE_DATE,
            TC_FIRMWARE_REV
        };

        uint8_t m_CurrentVideoFormat = TC_VIDEO_COLOR_RGB;
        INDI_PIXEL_FORMAT m_CameraPixelFormat = INDI_RGB;
        eTriggerMode m_CurrentTriggerMode = TRIGGER_VIDEO;

        bool m_CanSnap { false };
        bool m_RAWFormatSupport { false };
        bool m_RAWHighDepthSupport { false };
        bool m_MonoCamera { false };

        uint8_t m_BitsPerPixel { 8 };
        uint8_t m_RawBitsPerPixel { 8 };
        uint8_t m_MaxBitDepth { 8 };
        uint8_t m_Channels { 1 };
        uint8_t m_TimeoutRetries { 0 };

        friend void ::ISGetProperties(const char *dev);
        friend void ::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int num);
        friend void ::ISNewText(const char *dev, const char *name, char *texts[], char *names[], int num);
        friend void ::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int num);
        friend void ::ISNewBLOB(const char *dev, const char *name, int sizes[], int blobsizes[], char *blobs[],
                                char *formats[], char *names[], int n);

        static const uint8_t MAX_RETRIES { 5 };
};
