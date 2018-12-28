#ifndef __toupcam_h__
#define __toupcam_h__

/* Version: 30.12802.2018.0829 */
/*
   Platform & Architecture:
       (1) Win32:
              (a) x86: XP SP3 or above; CPU supports SSE2 instruction set or above
              (b) x64: Win7 or above
       (2) WinRT: x86 and x64; Win10 or above
       (3) macOS: x86 and x64 bundle; macOS 10.10 or above
       (4) Linux: kernel 2.6.27 or above
              (a) x86: CPU supports SSE3 instruction set or above; GLIBC 2.8 or above
              (b) x64: GLIBC 2.14 or above
              (c) armel: GLIBC 2.17 or above; built by toolchain arm-linux-gnueabi (version 4.9.2)
              (d) armhf: GLIBC 2.17 or above; built by toolchain arm-linux-gnueabihf (version 4.9.2)
              (e) arm64: GLIBC 2.17 or above; built by toolchain aarch64-linux-gnu (version 4.9.2)
*/

#ifdef _WIN32
#ifndef _INC_WINDOWS
#include <windows.h>
#endif
#endif

#ifdef __cplusplus
extern "C" {
#endif

#if defined(__cplusplus) && (__cplusplus >= 201402L)
#define toupcam_deprecated  [[deprecated]]
#elif defined(_MSC_VER)
#define toupcam_deprecated  __declspec(deprecated)
#elif defined(__GNUC__) || defined(__clang__)
#define toupcam_deprecated  __attribute__((deprecated))
#else
#define toupcam_deprecated
#endif

#ifdef _WIN32 /* Windows */

#pragma pack(push, 8)
#ifdef TOUPCAM_EXPORTS
#define toupcam_ports(x)    __declspec(dllexport)   x   __stdcall  /* in Windows, we use __stdcall calling convention, see https://docs.microsoft.com/en-us/cpp/cpp/stdcall */
#elif !defined(TOUPCAM_NOIMPORTS)
#define toupcam_ports(x)    __declspec(dllimport)   x   __stdcall
#else
#define toupcam_ports(x)    x   __stdcall
#endif

#else   /* Linux or macOS */

#define toupcam_ports(x)    x
#ifndef HRESULT
#define HRESULT int
#endif
#ifndef SUCCEEDED
#define SUCCEEDED(hr)   (((HRESULT)(hr)) >= 0)
#endif
#ifndef FAILED
#define FAILED(hr)      (((HRESULT)(hr)) < 0)
#endif

#ifndef __stdcall
#define __stdcall
#endif

#ifndef __BITMAPINFOHEADER_DEFINED__
#define __BITMAPINFOHEADER_DEFINED__
typedef struct {
    unsigned        biSize;
    int             biWidth;
    int             biHeight;
    unsigned short  biPlanes;
    unsigned short  biBitCount;
    unsigned        biCompression;
    unsigned        biSizeImage;
    int             biXPelsPerMeter;
    int             biYPelsPerMeter;
    unsigned        biClrUsed;
    unsigned        biClrImportant;
} BITMAPINFOHEADER;
#endif

#ifndef __RECT_DEFINED__
#define __RECT_DEFINED__
typedef struct {
    int left;
    int top;
    int right;
    int bottom;
} RECT, *PRECT;
#endif

#endif

#ifndef TDIBWIDTHBYTES
#define TDIBWIDTHBYTES(bits)  ((unsigned)(((bits) + 31) & (~31)) / 8)
#endif

/********************************************************************************/
/* HRESULT                                                                      */
/*    |----------------|---------------------------------------|------------|   */
/*    | S_OK           |   Operation successful                | 0x00000000 |   */
/*    | S_FALSE        |   Operation successful                | 0x00000001 |   */
/*    | E_FAIL         |   Unspecified failure                 | 0x80004005 |   */
/*    | E_INVALIDARG   |   One or more arguments are not valid | 0x80070057 |   */
/*    | E_NOTIMPL      |   Not supported or not implemented    | 0x80004001 |   */
/*    | E_NOINTERFACE  |   Interface not supported             | 0x80004002 |   */
/*    | E_POINTER      |   Pointer that is not valid           | 0x80004003 |   */
/*    | E_UNEXPECTED   |   Unexpected failure                  | 0x8000FFFF |   */
/*    | E_OUTOFMEMORY  |   Out of memory                       | 0x8007000E |   */
/*    | E_WRONG_THREAD |   call function in the wrong thread   | 0x8001010E |   */
/*    |----------------|---------------------------------------|------------|   */
/********************************************************************************/

/* handle */
typedef struct ToupcamT { int unused; } *HToupCam;

#define TOUPCAM_MAX                     16

#define TOUPCAM_FLAG_CMOS               0x00000001  /* cmos sensor */
#define TOUPCAM_FLAG_CCD_PROGRESSIVE    0x00000002  /* progressive ccd sensor */
#define TOUPCAM_FLAG_CCD_INTERLACED     0x00000004  /* interlaced ccd sensor */
#define TOUPCAM_FLAG_ROI_HARDWARE       0x00000008  /* support hardware ROI */
#define TOUPCAM_FLAG_MONO               0x00000010  /* monochromatic */
#define TOUPCAM_FLAG_BINSKIP_SUPPORTED  0x00000020  /* support bin/skip mode, see Toupcam_put_Mode and Toupcam_get_Mode */
#define TOUPCAM_FLAG_USB30              0x00000040  /* usb3.0 */
#define TOUPCAM_FLAG_TEC                0x00000080  /* Thermoelectric Cooler */
#define TOUPCAM_FLAG_USB30_OVER_USB20   0x00000100  /* usb3.0 camera connected to usb2.0 port */
#define TOUPCAM_FLAG_ST4                0x00000200  /* ST4 port */
#define TOUPCAM_FLAG_GETTEMPERATURE     0x00000400  /* support to get the temperature of the sensor */
#define TOUPCAM_FLAG_PUTTEMPERATURE     0x00000800  /* support to put the target temperature of the sensor */
#define TOUPCAM_FLAG_RAW10              0x00001000  /* pixel format, RAW 10bits */
#define TOUPCAM_FLAG_RAW12              0x00002000  /* pixel format, RAW 12bits */
#define TOUPCAM_FLAG_RAW14              0x00004000  /* pixel format, RAW 14bits */
#define TOUPCAM_FLAG_RAW16              0x00008000  /* pixel format, RAW 16bits */
#define TOUPCAM_FLAG_FAN                0x00010000  /* cooling fan */
#define TOUPCAM_FLAG_TEC_ONOFF          0x00020000  /* Thermoelectric Cooler can be turn on or off, support to set the target temperature of TEC */
#define TOUPCAM_FLAG_ISP                0x00040000  /* ISP (Image Signal Processing) chip */
#define TOUPCAM_FLAG_TRIGGER_SOFTWARE   0x00080000  /* support software trigger */
#define TOUPCAM_FLAG_TRIGGER_EXTERNAL   0x00100000  /* support external trigger */
#define TOUPCAM_FLAG_TRIGGER_SINGLE     0x00200000  /* only support trigger single: one trigger, one image */
#define TOUPCAM_FLAG_BLACKLEVEL         0x00400000  /* support set and get the black level */
#define TOUPCAM_FLAG_AUTO_FOCUS         0x00800000  /* support auto focus */
#define TOUPCAM_FLAG_BUFFER             0x01000000  /* frame buffer */
#define TOUPCAM_FLAG_DDR                0x02000000  /* use very large capacity DDR (Double Data Rate SDRAM) for frame buffer */
#define TOUPCAM_FLAG_CG                 0x04000000  /* Conversion Gain: HCG, LCG */
#define TOUPCAM_FLAG_YUV411             0x08000000  /* pixel format, yuv411 */
#define TOUPCAM_FLAG_VUYY               0x10000000  /* pixel format, yuv422, VUYY */
#define TOUPCAM_FLAG_YUV444             0x20000000  /* pixel format, yuv444 */
#define TOUPCAM_FLAG_RGB888             0x40000000  /* pixel format, RGB888 */
#define TOUPCAM_FLAG_RAW8               0x80000000  /* pixel format, RAW 8 bits */
#define TOUPCAM_FLAG_GMCY8      0x0000000100000000  /* pixel format, GMCY, 8bits */
#define TOUPCAM_FLAG_GMCY12     0x0000000200000000  /* pixel format, GMCY, 12bits */
#define TOUPCAM_FLAG_UYVY       0x0000000400000000  /* pixel format, yuv422, UYVY */
#define TOUPCAM_FLAG_CGHDR      0x0000000800000000  /* Conversion Gain: HCG, LCG, HDR */

#define TOUPCAM_TEMP_DEF                 6503    /* temp */
#define TOUPCAM_TEMP_MIN                 2000    /* temp */
#define TOUPCAM_TEMP_MAX                 15000   /* temp */
#define TOUPCAM_TINT_DEF                 1000    /* tint */
#define TOUPCAM_TINT_MIN                 200     /* tint */
#define TOUPCAM_TINT_MAX                 2500    /* tint */
#define TOUPCAM_HUE_DEF                  0       /* hue */
#define TOUPCAM_HUE_MIN                  (-180)  /* hue */
#define TOUPCAM_HUE_MAX                  180     /* hue */
#define TOUPCAM_SATURATION_DEF           128     /* saturation */
#define TOUPCAM_SATURATION_MIN           0       /* saturation */
#define TOUPCAM_SATURATION_MAX           255     /* saturation */
#define TOUPCAM_BRIGHTNESS_DEF           0       /* brightness */
#define TOUPCAM_BRIGHTNESS_MIN           (-64)   /* brightness */
#define TOUPCAM_BRIGHTNESS_MAX           64      /* brightness */
#define TOUPCAM_CONTRAST_DEF             0       /* contrast */
#define TOUPCAM_CONTRAST_MIN             (-100)  /* contrast */
#define TOUPCAM_CONTRAST_MAX             100     /* contrast */
#define TOUPCAM_GAMMA_DEF                100     /* gamma */
#define TOUPCAM_GAMMA_MIN                20      /* gamma */
#define TOUPCAM_GAMMA_MAX                180     /* gamma */
#define TOUPCAM_AETARGET_DEF             120     /* target of auto exposure */
#define TOUPCAM_AETARGET_MIN             16      /* target of auto exposure */
#define TOUPCAM_AETARGET_MAX             220     /* target of auto exposure */
#define TOUPCAM_WBGAIN_DEF               0       /* white balance gain */
#define TOUPCAM_WBGAIN_MIN               (-127)  /* white balance gain */
#define TOUPCAM_WBGAIN_MAX               127     /* white balance gain */
#define TOUPCAM_BLACKLEVEL_MIN           0       /* minimum black level */
#define TOUPCAM_BLACKLEVEL8_MAX          31              /* maximum black level for bit depth = 8 */
#define TOUPCAM_BLACKLEVEL10_MAX         (31 * 4)        /* maximum black level for bit depth = 10 */
#define TOUPCAM_BLACKLEVEL12_MAX         (31 * 16)       /* maximum black level for bit depth = 12 */
#define TOUPCAM_BLACKLEVEL14_MAX         (31 * 64)       /* maximum black level for bit depth = 14 */
#define TOUPCAM_BLACKLEVEL16_MAX         (31 * 256)      /* maximum black level for bit depth = 16 */
#define TOUPCAM_SHARPENING_STRENGTH_DEF  0       /* sharpening strength */
#define TOUPCAM_SHARPENING_STRENGTH_MIN  0       /* sharpening strength */
#define TOUPCAM_SHARPENING_STRENGTH_MAX  500     /* sharpening strength */
#define TOUPCAM_SHARPENING_RADIUS_DEF    2       /* sharpening radius */
#define TOUPCAM_SHARPENING_RADIUS_MIN    1       /* sharpening radius */
#define TOUPCAM_SHARPENING_RADIUS_MAX    10      /* sharpening radius */
#define TOUPCAM_SHARPENING_THRESHOLD_DEF 0       /* sharpening threshold */
#define TOUPCAM_SHARPENING_THRESHOLD_MIN 0       /* sharpening threshold */
#define TOUPCAM_SHARPENING_THRESHOLD_MAX 255     /* sharpening threshold */

typedef struct{
    unsigned    width;
    unsigned    height;
}ToupcamResolution;

/* In Windows platform, we always use UNICODE wchar_t */
/* In Linux or macOS, we use char */

typedef struct {
#ifdef _WIN32
    const wchar_t*      name;        /* model name, in Windows, we use unicode */
#else
    const char*         name;        /* model name */
#endif
    unsigned long long  flag;        /* TOUPCAM_FLAG_xxx, 64 bits */
    unsigned            maxspeed;    /* number of speed level, same as Toupcam_get_MaxSpeed(), the speed range = [0, maxspeed], closed interval */
    unsigned            preview;     /* number of preview resolution, same as Toupcam_get_ResolutionNumber() */
    unsigned            still;       /* number of still resolution, same as Toupcam_get_StillResolutionNumber() */
    unsigned            maxfanspeed; /* maximum fan speed */
    unsigned            ioctrol;     /* number of input/output control */
    float               xpixsz;      /* physical pixel size */
    float               ypixsz;      /* physical pixel size */
    ToupcamResolution   res[TOUPCAM_MAX];
}ToupcamModelV2; /* camera model v2 */

typedef struct {
#ifdef _WIN32
    wchar_t               displayname[64];    /* display name */
    wchar_t               id[64];             /* unique and opaque id of a connected camera, for Toupcam_Open */
#else
    char                  displayname[64];    /* display name */
    char                  id[64];             /* unique and opaque id of a connected camera, for Toupcam_Open */
#endif
    const ToupcamModelV2* model;
}ToupcamInstV2; /* camera instance for enumerating */

/*
    get the version of this dll/so/dylib, which is: 30.12802.2018.0829
*/
#ifdef _WIN32
toupcam_ports(const wchar_t*)   Toupcam_Version();
#else
toupcam_ports(const char*)      Toupcam_Version();
#endif

/*
    enumerate the cameras connected to the computer, return the number of enumerated.

    ToupcamInstV2 arr[TOUPCAM_MAX];
    unsigned cnt = Toupcam_EnumV2(arr);
    for (unsigned i = 0; i < cnt; ++i)
        ...

    if pti == NULL, then, only the number is returned.
    Toupcam_Enum is obsolete.
*/
toupcam_ports(unsigned) Toupcam_EnumV2(ToupcamInstV2 pti[TOUPCAM_MAX]);

/* use the id of ToupcamInstV2, which is enumerated by Toupcam_EnumV2.
    if id is NULL, Toupcam_Open will open the first camera.
*/
#ifdef _WIN32
toupcam_ports(HToupCam) Toupcam_Open(const wchar_t* id);
#else
toupcam_ports(HToupCam) Toupcam_Open(const char* id);
#endif

/*
    the same with Toupcam_Open, but use the index as the parameter. such as:
    index == 0, open the first camera,
    index == 1, open the second camera,
    etc
*/
toupcam_ports(HToupCam) Toupcam_OpenByIndex(unsigned index);

toupcam_ports(void)     Toupcam_Close(HToupCam h); /* close the handle */

#define TOUPCAM_EVENT_EXPOSURE      0x0001    /* exposure time changed */
#define TOUPCAM_EVENT_TEMPTINT      0x0002    /* white balance changed, Temp/Tint mode */
#define TOUPCAM_EVENT_IMAGE         0x0004    /* live image arrived, use Toupcam_PullImage to get this image */
#define TOUPCAM_EVENT_STILLIMAGE    0x0005    /* snap (still) frame arrived, use Toupcam_PullStillImage to get this frame */
#define TOUPCAM_EVENT_WBGAIN        0x0006    /* white balance changed, RGB Gain mode */
#define TOUPCAM_EVENT_TRIGGERFAIL   0x0007    /* trigger failed */
#define TOUPCAM_EVENT_BLACK         0x0008    /* black balance changed */
#define TOUPCAM_EVENT_FFC           0x0009    /* flat field correction status changed */
#define TOUPCAM_EVENT_DFC           0x000a    /* dark field correction status changed */
#define TOUPCAM_EVENT_ERROR         0x0080    /* generic error */
#define TOUPCAM_EVENT_DISCONNECTED  0x0081    /* camera disconnected */
#define TOUPCAM_EVENT_TIMEOUT       0x0082    /* timeout error */
#define TOUPCAM_EVENT_FACTORY       0x8001    /* restore factory settings */

#ifdef _WIN32
toupcam_ports(HRESULT)  Toupcam_StartPullModeWithWndMsg(HToupCam h, HWND hWnd, UINT nMsg);
#endif

/* Do NOT call Toupcam_Close, Toupcam_Stop in this callback context, it deadlocks. */
typedef void (__stdcall* PTOUPCAM_EVENT_CALLBACK)(unsigned nEvent, void* pCallbackCtx);
toupcam_ports(HRESULT)  Toupcam_StartPullModeWithCallback(HToupCam h, PTOUPCAM_EVENT_CALLBACK pEventCallback, void* pCallbackContext);

#define TOUPCAM_FRAMEINFO_FLAG_SEQ          0x01 /* sequence number */
#define TOUPCAM_FRAMEINFO_FLAG_TIMESTAMP    0x02

typedef struct {
    unsigned            width;
    unsigned            height;
    unsigned            flag;       /* TOUPCAM_FRAMEINFO_FLAG_xxxx */
    unsigned            seq;        /* sequence number */
    unsigned long long  timestamp;  /* microsecond */
}ToupcamFrameInfoV2;

/*
    bits: 24 (RGB24), 32 (RGB32), 8 (Gray) or 16 (Gray). In RAW mode, this parameter is ignored.
    pnWidth, pnHeight: OUT parameter
    rowPitch: The distance from one row of to the next row. rowPitch = 0 means using the default row pitch.
*/
toupcam_ports(HRESULT)  Toupcam_PullImageV2(HToupCam h, void* pImageData, int bits, ToupcamFrameInfoV2* pInfo);
toupcam_ports(HRESULT)  Toupcam_PullStillImageV2(HToupCam h, void* pImageData, int bits, ToupcamFrameInfoV2* pInfo);
toupcam_ports(HRESULT)  Toupcam_PullImageWithRowPitchV2(HToupCam h, void* pImageData, int bits, int rowPitch, ToupcamFrameInfoV2* pInfo);
toupcam_ports(HRESULT)  Toupcam_PullStillImageWithRowPitchV2(HToupCam h, void* pImageData, int bits, int rowPitch, ToupcamFrameInfoV2* pInfo);

toupcam_ports(HRESULT)  Toupcam_PullImage(HToupCam h, void* pImageData, int bits, unsigned* pnWidth, unsigned* pnHeight);
toupcam_ports(HRESULT)  Toupcam_PullStillImage(HToupCam h, void* pImageData, int bits, unsigned* pnWidth, unsigned* pnHeight);
toupcam_ports(HRESULT)  Toupcam_PullImageWithRowPitch(HToupCam h, void* pImageData, int bits, int rowPitch, unsigned* pnWidth, unsigned* pnHeight);
toupcam_ports(HRESULT)  Toupcam_PullStillImageWithRowPitch(HToupCam h, void* pImageData, int bits, int rowPitch, unsigned* pnWidth, unsigned* pnHeight);

/*
    (NULL == pData) means that something is error
    pCallbackCtx is the callback context which is passed by Toupcam_Start
    bSnap: TRUE if Toupcam_Snap

    pDataCallback is callbacked by an internal thread of toupcam.dll, so please pay attention to multithread problem.
    Do NOT call Toupcam_Close, Toupcam_Stop in this callback context, it deadlocks.
*/
typedef void (__stdcall* PTOUPCAM_DATA_CALLBACK_V2)(const void* pData, const ToupcamFrameInfoV2* pInfo, int bSnap, void* pCallbackCtx);
toupcam_ports(HRESULT)  Toupcam_StartPushModeV2(HToupCam h, PTOUPCAM_DATA_CALLBACK_V2 pDataCallback, void* pCallbackCtx);

typedef void (__stdcall* PTOUPCAM_DATA_CALLBACK)(const void* pData, const BITMAPINFOHEADER* pHeader, int bSnap, void* pCallbackCtx);
toupcam_ports(HRESULT)  Toupcam_StartPushMode(HToupCam h, PTOUPCAM_DATA_CALLBACK pDataCallback, void* pCallbackCtx);

toupcam_ports(HRESULT)  Toupcam_Stop(HToupCam h);
toupcam_ports(HRESULT)  Toupcam_Pause(HToupCam h, int bPause);

/*  for pull mode: TOUPCAM_EVENT_STILLIMAGE, and then Toupcam_PullStillImage
    for push mode: the snapped image will be return by PTOUPCAM_DATA_CALLBACK, with the parameter 'bSnap' set to 'TRUE'
*/
toupcam_ports(HRESULT)  Toupcam_Snap(HToupCam h, unsigned nResolutionIndex);  /* still image snap */
toupcam_ports(HRESULT)  Toupcam_SnapN(HToupCam h, unsigned nResolutionIndex, unsigned nNumber);  /* multiple still image snap */
/*
    soft trigger:
    nNumber:    0xffff:     trigger continuously
                0:          cancel trigger
                others:     number of images to be triggered
*/
toupcam_ports(HRESULT)  Toupcam_Trigger(HToupCam h, unsigned short nNumber);

/*
    put_Size, put_eSize, can be used to set the video output resolution BEFORE ToupCam_Start.
    put_Size use width and height parameters, put_eSize use the index parameter.
    for example, UCMOS03100KPA support the following resolutions:
            index 0:    2048,   1536
            index 1:    1024,   768
            index 2:    680,    510
    so, we can use put_Size(h, 1024, 768) or put_eSize(h, 1). Both have the same effect.
*/
toupcam_ports(HRESULT)  Toupcam_put_Size(HToupCam h, int nWidth, int nHeight);
toupcam_ports(HRESULT)  Toupcam_get_Size(HToupCam h, int* pWidth, int* pHeight);
toupcam_ports(HRESULT)  Toupcam_put_eSize(HToupCam h, unsigned nResolutionIndex);
toupcam_ports(HRESULT)  Toupcam_get_eSize(HToupCam h, unsigned* pnResolutionIndex);

toupcam_ports(HRESULT)  Toupcam_get_ResolutionNumber(HToupCam h);
toupcam_ports(HRESULT)  Toupcam_get_Resolution(HToupCam h, unsigned nResolutionIndex, int* pWidth, int* pHeight);
toupcam_ports(HRESULT)  Toupcam_get_ResolutionRatio(HToupCam h, unsigned nResolutionIndex, int* pNumerator, int* pDenominator);
toupcam_ports(HRESULT)  Toupcam_get_Field(HToupCam h);

/*
see: http://www.fourcc.org
FourCC:
    MAKEFOURCC('G', 'B', 'R', 'G'), see http://www.siliconimaging.com/RGB%20Bayer.htm
    MAKEFOURCC('R', 'G', 'G', 'B')
    MAKEFOURCC('B', 'G', 'G', 'R')
    MAKEFOURCC('G', 'R', 'B', 'G')
    MAKEFOURCC('Y', 'Y', 'Y', 'Y'), monochromatic sensor
    MAKEFOURCC('Y', '4', '1', '1'), yuv411
    MAKEFOURCC('V', 'U', 'Y', 'Y'), yuv422
    MAKEFOURCC('U', 'Y', 'V', 'Y'), yuv422
    MAKEFOURCC('Y', '4', '4', '4'), yuv444
    MAKEFOURCC('R', 'G', 'B', '8'), RGB888

#ifndef MAKEFOURCC
#define MAKEFOURCC(a, b, c, d) ((unsigned)(unsigned char)(a) | ((unsigned)(unsigned char)(b) << 8) | ((unsigned)(unsigned char)(c) << 16) | ((unsigned)(unsigned char)(d) << 24))
#endif
*/
toupcam_ports(HRESULT)  Toupcam_get_RawFormat(HToupCam h, unsigned* nFourCC, unsigned* bitsperpixel);

/*
    ------------------------------------------------------------------|
    | Parameter               |   Range       |   Default             |
    |-----------------------------------------------------------------|
    | Auto Exposure Target    |   10~230      |   120                 |
    | Temp                    |   2000~15000  |   6503                |
    | Tint                    |   200~2500    |   1000                |
    | LevelRange              |   0~255       |   Low = 0, High = 255 |
    | Contrast                |   -100~100    |   0                   |
    | Hue                     |   -180~180    |   0                   |
    | Saturation              |   0~255       |   128                 |
    | Brightness              |   -64~64      |   0                   |
    | Gamma                   |   20~180      |   100                 |
    | WBGain                  |   -127~127    |   0                   |
    ------------------------------------------------------------------|
*/

#ifndef __TOUPCAM_CALLBACK_DEFINED__
#define __TOUPCAM_CALLBACK_DEFINED__
typedef void (__stdcall* PITOUPCAM_EXPOSURE_CALLBACK)(void* pCtx);                                     /* auto exposure */
typedef void (__stdcall* PITOUPCAM_WHITEBALANCE_CALLBACK)(const int aGain[3], void* pCtx);             /* one push white balance, RGB Gain mode */
typedef void (__stdcall* PITOUPCAM_BLACKBALANCE_CALLBACK)(const unsigned short aSub[3], void* pCtx);   /* one push black balance */
typedef void (__stdcall* PITOUPCAM_TEMPTINT_CALLBACK)(const int nTemp, const int nTint, void* pCtx);   /* one push white balance, Temp/Tint Mode */
typedef void (__stdcall* PITOUPCAM_HISTOGRAM_CALLBACK)(const float aHistY[256], const float aHistR[256], const float aHistG[256], const float aHistB[256], void* pCtx);
typedef void (__stdcall* PITOUPCAM_CHROME_CALLBACK)(void* pCtx);
#endif

toupcam_ports(HRESULT)  Toupcam_get_AutoExpoEnable(HToupCam h, int* bAutoExposure);
toupcam_ports(HRESULT)  Toupcam_put_AutoExpoEnable(HToupCam h, int bAutoExposure);
toupcam_ports(HRESULT)  Toupcam_get_AutoExpoTarget(HToupCam h, unsigned short* Target);
toupcam_ports(HRESULT)  Toupcam_put_AutoExpoTarget(HToupCam h, unsigned short Target);

#define TOUPCAM_MAX_AE_EXPTIME  350000  /* default: 350 ms */
#define TOUPCAM_MAX_AE_AGAIN    500

/*set the maximum auto exposure time and analog agin. The default maximum auto exposure time is 350ms */
toupcam_ports(HRESULT)  Toupcam_put_MaxAutoExpoTimeAGain(HToupCam h, unsigned maxTime, unsigned short maxAGain);

toupcam_ports(HRESULT)  Toupcam_get_ExpoTime(HToupCam h, unsigned* Time); /* in microseconds */
toupcam_ports(HRESULT)  Toupcam_put_ExpoTime(HToupCam h, unsigned Time); /* in microseconds */
toupcam_ports(HRESULT)  Toupcam_get_ExpTimeRange(HToupCam h, unsigned* nMin, unsigned* nMax, unsigned* nDef);

toupcam_ports(HRESULT)  Toupcam_get_ExpoAGain(HToupCam h, unsigned short* AGain); /* percent, such as 300 */
toupcam_ports(HRESULT)  Toupcam_put_ExpoAGain(HToupCam h, unsigned short AGain); /* percent */
toupcam_ports(HRESULT)  Toupcam_get_ExpoAGainRange(HToupCam h, unsigned short* nMin, unsigned short* nMax, unsigned short* nDef);

/* Auto White Balance, Temp/Tint Mode */
toupcam_ports(HRESULT)  Toupcam_AwbOnePush(HToupCam h, PITOUPCAM_TEMPTINT_CALLBACK fnTTProc, void* pTTCtx); /* auto white balance "one push". This function must be called AFTER Toupcam_StartXXXX */

/* Auto White Balance, RGB Gain Mode */
toupcam_ports(HRESULT)  Toupcam_AwbInit(HToupCam h, PITOUPCAM_WHITEBALANCE_CALLBACK fnWBProc, void* pWBCtx);

/* White Balance, Temp/Tint mode */
toupcam_ports(HRESULT)  Toupcam_put_TempTint(HToupCam h, int nTemp, int nTint);
toupcam_ports(HRESULT)  Toupcam_get_TempTint(HToupCam h, int* nTemp, int* nTint);

/* White Balance, RGB Gain mode */
toupcam_ports(HRESULT)  Toupcam_put_WhiteBalanceGain(HToupCam h, int aGain[3]);
toupcam_ports(HRESULT)  Toupcam_get_WhiteBalanceGain(HToupCam h, int aGain[3]);

/* Black Balance */
toupcam_ports(HRESULT)  Toupcam_AbbOnePush(HToupCam h, PITOUPCAM_BLACKBALANCE_CALLBACK fnBBProc, void* pBBCtx); /* auto black balance "one push". This function must be called AFTER Toupcam_StartXXXX */
toupcam_ports(HRESULT)  Toupcam_put_BlackBalance(HToupCam h, unsigned short aSub[3]);
toupcam_ports(HRESULT)  Toupcam_get_BlackBalance(HToupCam h, unsigned short aSub[3]);

/* Flat Field Correction */
toupcam_ports(HRESULT)  Toupcam_FfcOnePush(HToupCam h);
#ifdef _WIN32
toupcam_ports(HRESULT)  Toupcam_FfcExport(HToupCam h, const wchar_t* filepath);
toupcam_ports(HRESULT)  Toupcam_FfcImport(HToupCam h, const wchar_t* filepath);
#else
toupcam_ports(HRESULT)  Toupcam_FfcExport(HToupCam h, const char* filepath);
toupcam_ports(HRESULT)  Toupcam_FfcImport(HToupCam h, const char* filepath);
#endif

/* Dark Field Correction */
toupcam_ports(HRESULT)  Toupcam_DfcOnePush(HToupCam h);

#ifdef _WIN32
toupcam_ports(HRESULT)  Toupcam_DfcExport(HToupCam h, const wchar_t* filepath);
toupcam_ports(HRESULT)  Toupcam_DfcImport(HToupCam h, const wchar_t* filepath);
#else
toupcam_ports(HRESULT)  Toupcam_DfcExport(HToupCam h, const char* filepath);
toupcam_ports(HRESULT)  Toupcam_DfcImport(HToupCam h, const char* filepath);
#endif

toupcam_ports(HRESULT)  Toupcam_put_Hue(HToupCam h, int Hue);
toupcam_ports(HRESULT)  Toupcam_get_Hue(HToupCam h, int* Hue);
toupcam_ports(HRESULT)  Toupcam_put_Saturation(HToupCam h, int Saturation);
toupcam_ports(HRESULT)  Toupcam_get_Saturation(HToupCam h, int* Saturation);
toupcam_ports(HRESULT)  Toupcam_put_Brightness(HToupCam h, int Brightness);
toupcam_ports(HRESULT)  Toupcam_get_Brightness(HToupCam h, int* Brightness);
toupcam_ports(HRESULT)  Toupcam_get_Contrast(HToupCam h, int* Contrast);
toupcam_ports(HRESULT)  Toupcam_put_Contrast(HToupCam h, int Contrast);
toupcam_ports(HRESULT)  Toupcam_get_Gamma(HToupCam h, int* Gamma); /* percent */
toupcam_ports(HRESULT)  Toupcam_put_Gamma(HToupCam h, int Gamma);  /* percent */

toupcam_ports(HRESULT)  Toupcam_get_Chrome(HToupCam h, int* bChrome);  /* monochromatic mode */
toupcam_ports(HRESULT)  Toupcam_put_Chrome(HToupCam h, int bChrome);

toupcam_ports(HRESULT)  Toupcam_get_VFlip(HToupCam h, int* bVFlip);  /* vertical flip */
toupcam_ports(HRESULT)  Toupcam_put_VFlip(HToupCam h, int bVFlip);
toupcam_ports(HRESULT)  Toupcam_get_HFlip(HToupCam h, int* bHFlip);
toupcam_ports(HRESULT)  Toupcam_put_HFlip(HToupCam h, int bHFlip); /* horizontal flip */

toupcam_ports(HRESULT)  Toupcam_get_Negative(HToupCam h, int* bNegative);  /* negative film */
toupcam_ports(HRESULT)  Toupcam_put_Negative(HToupCam h, int bNegative);

toupcam_ports(HRESULT)  Toupcam_put_Speed(HToupCam h, unsigned short nSpeed);
toupcam_ports(HRESULT)  Toupcam_get_Speed(HToupCam h, unsigned short* pSpeed);
toupcam_ports(HRESULT)  Toupcam_get_MaxSpeed(HToupCam h); /* get the maximum speed, see "Frame Speed Level", the speed range = [0, max], closed interval */

toupcam_ports(HRESULT)  Toupcam_get_FanMaxSpeed(HToupCam h); /* get the maximum fan speed, the fan speed range = [0, max], closed interval */

toupcam_ports(HRESULT)  Toupcam_get_MaxBitDepth(HToupCam h); /* get the max bit depth of this camera, such as 8, 10, 12, 14, 16 */

/* power supply of lighting:
        0 -> 60HZ AC
        1 -> 50Hz AC
        2 -> DC
*/
toupcam_ports(HRESULT)  Toupcam_put_HZ(HToupCam h, int nHZ);
toupcam_ports(HRESULT)  Toupcam_get_HZ(HToupCam h, int* nHZ);

toupcam_ports(HRESULT)  Toupcam_put_Mode(HToupCam h, int bSkip); /* skip or bin */
toupcam_ports(HRESULT)  Toupcam_get_Mode(HToupCam h, int* bSkip); /* If the model don't support bin/skip mode, return E_NOTIMPL */

toupcam_ports(HRESULT)  Toupcam_put_AWBAuxRect(HToupCam h, const RECT* pAuxRect); /* auto white balance ROI */
toupcam_ports(HRESULT)  Toupcam_get_AWBAuxRect(HToupCam h, RECT* pAuxRect);
toupcam_ports(HRESULT)  Toupcam_put_AEAuxRect(HToupCam h, const RECT* pAuxRect);  /* auto exposure ROI */
toupcam_ports(HRESULT)  Toupcam_get_AEAuxRect(HToupCam h, RECT* pAuxRect);

toupcam_ports(HRESULT)  Toupcam_put_ABBAuxRect(HToupCam h, const RECT* pAuxRect); /* auto black balance ROI */
toupcam_ports(HRESULT)  Toupcam_get_ABBAuxRect(HToupCam h, RECT* pAuxRect);

/*
    S_FALSE:    color mode
    S_OK:       mono mode, such as EXCCD00300KMA and UHCCD01400KMA
*/
toupcam_ports(HRESULT)  Toupcam_get_MonoMode(HToupCam h);

toupcam_ports(HRESULT)  Toupcam_get_StillResolutionNumber(HToupCam h);
toupcam_ports(HRESULT)  Toupcam_get_StillResolution(HToupCam h, unsigned nResolutionIndex, int* pWidth, int* pHeight);

/* use minimum frame buffer.
    If DDR present, also limit the DDR frame buffer to only one frame.
    default: FALSE
*/
toupcam_ports(HRESULT)  Toupcam_put_RealTime(HToupCam h, int bEnable);
toupcam_ports(HRESULT)  Toupcam_get_RealTime(HToupCam h, int* bEnable);

/* discard the current internal frame cache.
    If DDR present, also discard the frames in the DDR.
*/
toupcam_ports(HRESULT)  Toupcam_Flush(HToupCam h);

/* get the temperature of the sensor, in 0.1 degrees Celsius (32 means 3.2 degrees Celsius, -35 means -3.5 degree Celsius)
    return E_NOTIMPL if not supported
*/
toupcam_ports(HRESULT)  Toupcam_get_Temperature(HToupCam h, short* pTemperature);

/* set the target temperature of the sensor, in 0.1 degrees Celsius (32 means 3.2 degrees Celsius, -35 means -3.5 degree Celsius)
    return E_NOTIMPL if not supported
*/
toupcam_ports(HRESULT)  Toupcam_put_Temperature(HToupCam h, short nTemperature);

/*
    get the revision
*/
toupcam_ports(HRESULT)  Toupcam_get_Revision(HToupCam h, unsigned short* pRevision);

/*
    get the serial number which is always 32 chars which is zero-terminated such as "TP110826145730ABCD1234FEDC56787"
*/
toupcam_ports(HRESULT)  Toupcam_get_SerialNumber(HToupCam h, char sn[32]);

/*
    get the camera firmware version, such as: 3.2.1.20140922
*/
toupcam_ports(HRESULT)  Toupcam_get_FwVersion(HToupCam h, char fwver[16]);

/*
    get the camera hardware version, such as: 3.12
*/
toupcam_ports(HRESULT)  Toupcam_get_HwVersion(HToupCam h, char hwver[16]);

/*
    get the production date, such as: 20150327, YYYYMMDD, (YYYY: year, MM: month, DD: day)
*/
toupcam_ports(HRESULT)  Toupcam_get_ProductionDate(HToupCam h, char pdate[10]);

/*
    get the FPGA version, such as: 1.13
*/
toupcam_ports(HRESULT)  Toupcam_get_FpgaVersion(HToupCam h, char fpgaver[16]);

/*
    get the sensor pixel size, such as: 2.4um
*/
toupcam_ports(HRESULT)  Toupcam_get_PixelSize(HToupCam h, unsigned nResolutionIndex, float* x, float* y);

toupcam_ports(HRESULT)  Toupcam_put_LevelRange(HToupCam h, unsigned short aLow[4], unsigned short aHigh[4]);
toupcam_ports(HRESULT)  Toupcam_get_LevelRange(HToupCam h, unsigned short aLow[4], unsigned short aHigh[4]);

toupcam_ports(HRESULT)  Toupcam_put_ExpoCallback(HToupCam h, PITOUPCAM_EXPOSURE_CALLBACK fnExpoProc, void* pExpoCtx);
toupcam_ports(HRESULT)  Toupcam_put_ChromeCallback(HToupCam h, PITOUPCAM_CHROME_CALLBACK fnChromeProc, void* pChromeCtx);

/*
    The following functions must be called AFTER Toupcam_StartPushMode or Toupcam_StartPullModeWithWndMsg or Toupcam_StartPullModeWithCallback
*/
toupcam_ports(HRESULT)  Toupcam_LevelRangeAuto(HToupCam h);
toupcam_ports(HRESULT)  Toupcam_GetHistogram(HToupCam h, PITOUPCAM_HISTOGRAM_CALLBACK fnHistogramProc, void* pHistogramCtx);

/* led state:
    iLed: Led index, (0, 1, 2, ...)
    iState: 1 -> Ever bright; 2 -> Flashing; other -> Off
    iPeriod: Flashing Period (>= 500ms)
*/
toupcam_ports(HRESULT)  Toupcam_put_LEDState(HToupCam h, unsigned short iLed, unsigned short iState, unsigned short iPeriod);

toupcam_ports(HRESULT)  Toupcam_write_EEPROM(HToupCam h, unsigned addr, const unsigned char* pBuffer, unsigned nBufferLen);
toupcam_ports(HRESULT)  Toupcam_read_EEPROM(HToupCam h, unsigned addr, unsigned char* pBuffer, unsigned nBufferLen);

toupcam_ports(HRESULT)  Toupcam_read_Pipe(HToupCam h, unsigned pipeNum, void* pBuffer, unsigned nBufferLen);
toupcam_ports(HRESULT)  Toupcam_write_Pipe(HToupCam h, unsigned pipeNum, const void* pBuffer, unsigned nBufferLen);
toupcam_ports(HRESULT)  Toupcam_feed_Pipe(HToupCam h, unsigned pipeNum);

#define TOUPCAM_TEC_TARGET_MIN      (-300) /* -30.0 degrees Celsius */
#define TOUPCAM_TEC_TARGET_DEF      0      /* 0.0 degrees Celsius */
#define TOUPCAM_TEC_TARGET_MAX      300    /* 30.0 degrees Celsius */

#define TOUPCAM_OPTION_NOFRAME_TIMEOUT      0x01    /* 1 = enable; 0 = disable. default: disable */
#define TOUPCAM_OPTION_THREAD_PRIORITY      0x02    /* set the priority of the internal thread which grab data from the usb device. iValue: 0 = THREAD_PRIORITY_NORMAL; 1 = THREAD_PRIORITY_ABOVE_NORMAL; 2 = THREAD_PRIORITY_HIGHEST; default: 0; see: msdn SetThreadPriority */
#define TOUPCAM_OPTION_PROCESSMODE          0x03    /* 0 = better image quality, more cpu usage. this is the default value
                                                       1 = lower image quality, less cpu usage */
#define TOUPCAM_OPTION_RAW                  0x04    /* raw data mode, read the sensor "raw" data. This can be set only BEFORE Toupcam_StartXXX(). 0 = rgb, 1 = raw, default value: 0 */
#define TOUPCAM_OPTION_HISTOGRAM            0x05    /* 0 = only one, 1 = continue mode */
#define TOUPCAM_OPTION_BITDEPTH             0x06    /* 0 = 8 bits mode, 1 = 16 bits mode, subset of TOUPCAM_OPTION_PIXEL_FORMAT */
#define TOUPCAM_OPTION_FAN                  0x07    /* 0 = turn off the cooling fan, [1, max] = fan speed */
#define TOUPCAM_OPTION_TEC                  0x08    /* 0 = turn off the thermoelectric cooler, 1 = turn on the thermoelectric cooler */
#define TOUPCAM_OPTION_LINEAR               0x09    /* 0 = turn off the builtin linear tone mapping, 1 = turn on the builtin linear tone mapping, default value: 1 */
#define TOUPCAM_OPTION_CURVE                0x0a    /* 0 = turn off the builtin curve tone mapping, 1 = turn on the builtin polynomial curve tone mapping, 2 = logarithmic curve tone mapping, default value: 2 */
#define TOUPCAM_OPTION_TRIGGER              0x0b    /* 0 = video mode, 1 = software or simulated trigger mode, 2 = external trigger mode, default value = 0 */
#define TOUPCAM_OPTION_RGB                  0x0c    /* 0 => RGB24; 1 => enable RGB48 format when bitdepth > 8; 2 => RGB32; 3 => 8 Bits Gray (only for mono camera); 4 => 16 Bits Gray (only for mono camera when bitdepth > 8) */
#define TOUPCAM_OPTION_COLORMATIX           0x0d    /* enable or disable the builtin color matrix, default value: 1 */
#define TOUPCAM_OPTION_WBGAIN               0x0e    /* enable or disable the builtin white balance gain, default value: 1 */
#define TOUPCAM_OPTION_TECTARGET            0x0f    /* get or set the target temperature of the thermoelectric cooler, in 0.1 degree Celsius. For example, 125 means 12.5 degree Celsius, -35 means -3.5 degree Celsius */
#define TOUPCAM_OPTION_AGAIN                0x10    /* enable or disable adjusting the analog gain when auto exposure is enabled. default value: enable */
#define TOUPCAM_OPTION_FRAMERATE            0x11    /* limit the frame rate, range=[0, 63], the default value 0 means no limit */
#define TOUPCAM_OPTION_DEMOSAIC             0x12    /* demosaic method for both video and still image: BILINEAR = 0, VNG(Variable Number of Gradients interpolation) = 1, PPG(Patterned Pixel Grouping interpolation) = 2, AHD(Adaptive Homogeneity-Directed interpolation) = 3, see https://en.wikipedia.org/wiki/Demosaicing, default value: 0 */
#define TOUPCAM_OPTION_DEMOSAIC_VIDEO       0x13    /* demosaic method for video */
#define TOUPCAM_OPTION_DEMOSAIC_STILL       0x14    /* demosaic method for still image */
#define TOUPCAM_OPTION_BLACKLEVEL           0x15    /* black level */
#define TOUPCAM_OPTION_MULTITHREAD          0x16    /* multithread image processing */
#define TOUPCAM_OPTION_BINNING              0x17    /* binning, 0x01 (no binning), 0x02 (add, 2*2), 0x03 (add, 3*3), 0x04 (add, 4*4), 0x82 (average, 2*2), 0x83 (average, 3*3), 0x84 (average, 4*4) */
#define TOUPCAM_OPTION_ROTATE               0x18    /* rotate clockwise: 0, 90, 180, 270 */
#define TOUPCAM_OPTION_CG                   0x19    /* Conversion Gain: 0 = LCG, 1 = HCG, 2 = HDR */
#define TOUPCAM_OPTION_PIXEL_FORMAT         0x1a    /* pixel format, TOUPCAM_PIXELFORMAT_xxxx */
#define TOUPCAM_OPTION_FFC                  0x1b    /* flat field correction
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
#define TOUPCAM_OPTION_DDR_DEPTH            0x1c    /* the number of the frames that DDR can cache
                                                            1: DDR cache only one frame
                                                            0: Auto:
                                                                    ->one for video mode when auto exposure is enabled
                                                                    ->full capacity for others
                                                           -1: DDR can cache frames to full capacity
                                                    */
#define TOUPCAM_OPTION_DFC                  0x1d    /* dark field correction
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
#define TOUPCAM_OPTION_SHARPENING           0x1e    /* Sharpening: (threshold << 24) | (radius << 16) | strength)
                                                        strength: [0, 500], default: 0 (disable)
                                                        radius: [1, 10]
                                                        threshold: [0, 255]
                                                    */
#define TOUPCAM_OPTION_FACTORY              0x1f    /* restore the factory settings */
#define TOUPCAM_OPTION_TEC_VOLTAGE          0x20    /* get the current TEC voltage in 0.1V, 59 mean 5.9V; readonly */
#define TOUPCAM_OPTION_TEC_VOLTAGE_MAX      0x21    /* get the TEC maximum voltage in 0.1V; readonly */
#define TOUPCAM_OPTION_DEVICE_RESET         0x22    /* reset usb device, simulate a replug */

#define TOUPCAM_PIXELFORMAT_RAW8            0x00
#define TOUPCAM_PIXELFORMAT_RAW10           0x01
#define TOUPCAM_PIXELFORMAT_RAW12           0x02
#define TOUPCAM_PIXELFORMAT_RAW14           0x03
#define TOUPCAM_PIXELFORMAT_RAW16           0x04
#define TOUPCAM_PIXELFORMAT_YUV411          0x05
#define TOUPCAM_PIXELFORMAT_VUYY            0x06
#define TOUPCAM_PIXELFORMAT_YUV444          0x07
#define TOUPCAM_PIXELFORMAT_RGB888          0x08
#define TOUPCAM_PIXELFORMAT_GMCY8           0x09   /* map to RGGB 8 bits */
#define TOUPCAM_PIXELFORMAT_GMCY12          0x0a   /* map to RGGB 12 bits */
#define TOUPCAM_PIXELFORMAT_UYVY            0x0b

toupcam_ports(HRESULT)  Toupcam_put_Option(HToupCam h, unsigned iOption, int iValue);
toupcam_ports(HRESULT)  Toupcam_get_Option(HToupCam h, unsigned iOption, int* piValue);

toupcam_ports(HRESULT)  Toupcam_put_Roi(HToupCam h, unsigned xOffset, unsigned yOffset, unsigned xWidth, unsigned yHeight);
toupcam_ports(HRESULT)  Toupcam_get_Roi(HToupCam h, unsigned* pxOffset, unsigned* pyOffset, unsigned* pxWidth, unsigned* pyHeight);

#define TOUPCAM_IOCONTROLTYPE_GET_SUPPORTEDMODE           0x01 /* 0x01->Input, 0x02->Output, (0x01 | 0x02)->support both Input and Output */
#define TOUPCAM_IOCONTROLTYPE_GET_ALLSTATUS               0x02 /* a single bit field indicating the current logical state of all available line signals at time of polling */
#define TOUPCAM_IOCONTROLTYPE_GET_MODE                    0x03 /* 0x01->Input, 0x02->Output */
#define TOUPCAM_IOCONTROLTYPE_SET_MODE                    0x04
#define TOUPCAM_IOCONTROLTYPE_GET_FORMAT                  0x05 /*
                                                                   0x00-> not connected
                                                                   0x01-> Tri-state: Tri-state mode (Not driven)
                                                                   0x02-> TTL: TTL level signals
                                                                   0x03-> LVDS: LVDS level signals
                                                                   0x04-> RS422: RS422 level signals
                                                                   0x05-> Opto-coupled
                                                               */
#define TOUPCAM_IOCONTROLTYPE_SET_FORMAT                  0x06
#define TOUPCAM_IOCONTROLTYPE_GET_INVERTER                0x07 /* boolean */
#define TOUPCAM_IOCONTROLTYPE_SET_INVERTER                0x08
#define TOUPCAM_IOCONTROLTYPE_GET_LOGIC                   0x09 /* 0x01->Positive, 0x02->Negative */
#define TOUPCAM_IOCONTROLTYPE_SET_LOGIC                   0x0a
#define TOUPCAM_IOCONTROLTYPE_GET_MINIMUMOUTPUTPULSEWIDTH 0x0b /* minimum signal width of an output signal (in microseconds) */
#define TOUPCAM_IOCONTROLTYPE_SET_MINIMUMOUTPUTPULSEWIDTH 0x0c
#define TOUPCAM_IOCONTROLTYPE_GET_OVERLOADSTATUS          0x0d /* boolean */
#define TOUPCAM_IOCONTROLTYPE_SET_OVERLOADSTATUS          0x0e
#define TOUPCAM_IOCONTROLTYPE_GET_PITCH                   0x0f /* Number of bytes separating the starting pixels of two consecutive lines */
#define TOUPCAM_IOCONTROLTYPE_SET_PITCH                   0x10
#define TOUPCAM_IOCONTROLTYPE_GET_PITCHENABLE             0x11 /* boolean */
#define TOUPCAM_IOCONTROLTYPE_SET_PITCHENABLE             0x12
#define TOUPCAM_IOCONTROLTYPE_GET_SOURCE                  0x13 /*
                                                                   0->ExposureActive
                                                                   1->TimerActive
                                                                   2->UserOutput
                                                                   3->TriggerReady
                                                                   4->SerialTx
                                                                   5->AcquisitionTriggerReady
                                                               */
#define TOUPCAM_IOCONTROLTYPE_SET_SOURCE                  0x14
#define TOUPCAM_IOCONTROLTYPE_GET_STATUS                  0x15 /* boolean */
#define TOUPCAM_IOCONTROLTYPE_SET_STATUS                  0x16
#define TOUPCAM_IOCONTROLTYPE_GET_DEBOUNCERTIME           0x17 /* debouncer time in microseconds */
#define TOUPCAM_IOCONTROLTYPE_SET_DEBOUNCERTIME           0x18
#define TOUPCAM_IOCONTROLTYPE_GET_PWM_FREQ                0x19
#define TOUPCAM_IOCONTROLTYPE_SET_PWM_FREQ                0x1a
#define TOUPCAM_IOCONTROLTYPE_GET_PWM_DUTYRATIO           0x1b
#define TOUPCAM_IOCONTROLTYPE_SET_PWM_DUTYRATIO           0x1c

toupcam_ports(HRESULT)  Toupcam_IoControl(HToupCam h, unsigned index, unsigned nType, int outVal, int* inVal);

toupcam_ports(HRESULT)  Toupcam_write_UART(HToupCam h, const unsigned char* pData, unsigned nDataLen);
toupcam_ports(HRESULT)  Toupcam_read_UART(HToupCam h, unsigned char* pBuffer, unsigned nBufferLen);

toupcam_ports(HRESULT)  Toupcam_put_Linear(HToupCam h, const unsigned char* v8, const unsigned short* v16);
toupcam_ports(HRESULT)  Toupcam_put_Curve(HToupCam h, const unsigned char* v8, const unsigned short* v16);
toupcam_ports(HRESULT)  Toupcam_put_ColorMatrix(HToupCam h, const double v[9]);
toupcam_ports(HRESULT)  Toupcam_put_InitWBGain(HToupCam h, const unsigned short v[3]);

/*
    get the frame rate: framerate (fps) = Frame * 1000.0 / nTime
*/
toupcam_ports(HRESULT)  Toupcam_get_FrameRate(HToupCam h, unsigned* nFrame, unsigned* nTime, unsigned* nTotalFrame);

/* astronomy: for ST4 guide, please see: ASCOM Platform Help ICameraV2.
    nDirect: 0 = North, 1 = South, 2 = East, 3 = West, 4 = Stop
    nDuration: in milliseconds
*/
toupcam_ports(HRESULT)  Toupcam_ST4PlusGuide(HToupCam h, unsigned nDirect, unsigned nDuration);

/* S_OK: ST4 pulse guiding
   S_FALSE: ST4 not pulse guiding
*/
toupcam_ports(HRESULT)  Toupcam_ST4PlusGuideState(HToupCam h);

toupcam_ports(HRESULT)  Toupcam_InitOcl();

/* https://software.intel.com/en-us/articles/sharing-surfaces-between-opencl-and-directx-11-on-intel-processor-graphics */
/* https://software.intel.com/en-us/articles/opencl-and-opengl-interoperability-tutorial */
typedef struct {
#ifdef _WIN32
    void*       d3d11_device;      /* ID3D11Device */
    void*       d3d11_texture;     /* ID3D11Texture2D shared by opencl and d3d11, DXGI_FORMAT_R8G8B8A8_UINT */
#else
#ifdef __APPLE__
    void*       cgl_sharegroup;    /* CGLShareGroupObj */
#else
    void*       gl_context;        /* opengl context */
    void*       gl_display;        /* opengl display */
#endif
    unsigned    gl_type_texture_or_renderbuffer;    /* opengl texture (0) or renderbuffer (1) */
    unsigned    gl_texture_or_renderbuffer;         /* opengl texture or renderbuffer to be shared */
#endif
}ToupcamOclWithSharedTexture;
toupcam_ports(HRESULT)  Toupcam_StartOclWithSharedTexture(HToupCam h, const ToupcamOclWithSharedTexture* pocl, PTOUPCAM_EVENT_CALLBACK pEventCallback, void* pCallbackContext);

/*
    calculate the clarity factor:
    pImageData: pointer to the image data
    bits: 8(Grey), 24 (RGB24), 32(RGB32)
    nImgWidth, nImgHeight: the image width and height
*/
toupcam_ports(double)   Toupcam_calc_ClarityFactor(const void* pImageData, int bits, unsigned nImgWidth, unsigned nImgHeight);

toupcam_ports(void)     Toupcam_deBayer(unsigned nBayer, int nW, int nH, const void* input, void* output, unsigned char nBitDepth);

typedef void (__stdcall* PTOUPCAM_DEMOSAIC_CALLBACK)(unsigned nBayer, int nW, int nH, const void* input, void* output, unsigned char nBitDepth, void* pCallbackCtx);
toupcam_ports(HRESULT)  Toupcam_put_Demosaic(HToupCam h, PTOUPCAM_DEMOSAIC_CALLBACK pCallback, void* pCallbackCtx);

/*
    obsolete, please use ToupcamModelV2
*/
typedef struct {
#ifdef _WIN32
    const wchar_t*      name;       /* model name, in Windows, we use unicode */
#else
    const char*         name;       /* model name */
#endif
    unsigned            flag;       /* TOUPCAM_FLAG_xxx */
    unsigned            maxspeed;   /* number of speed level, same as Toupcam_get_MaxSpeed(), the speed range = [0, maxspeed], closed interval */
    unsigned            preview;    /* number of preview resolution, same as Toupcam_get_ResolutionNumber() */
    unsigned            still;      /* number of still resolution, same as Toupcam_get_StillResolutionNumber() */
    ToupcamResolution   res[TOUPCAM_MAX];
}ToupcamModel; /* camera model */

/*
    obsolete, please use ToupcamInstV2
*/
typedef struct {
#ifdef _WIN32
    wchar_t             displayname[64];    /* display name */
    wchar_t             id[64];             /* unique and opaque id of a connected camera, for Toupcam_Open */
#else
    char                displayname[64];    /* display name */
    char                id[64];             /* unique and opaque id of a connected camera, for Toupcam_Open */
#endif
    const ToupcamModel* model;
}ToupcamInst; /* camera instance for enumerating */

/*
    obsolete, please use Toupcam_EnumV2
*/
toupcam_deprecated
toupcam_ports(unsigned) Toupcam_Enum(ToupcamInst pti[TOUPCAM_MAX]);

#ifndef _WIN32

/*
This function is only available on macOS and Linux, it's unnecessary on Windows.
  (1) To process the device plug in / pull out in Windows, please refer to the MSDN
       (a) Device Management, http://msdn.microsoft.com/en-us/library/windows/desktop/aa363224(v=vs.85).aspx
       (b) Detecting Media Insertion or Removal, http://msdn.microsoft.com/en-us/library/windows/desktop/aa363215(v=vs.85).aspx
  (2) To process the device plug in / pull out in Linux / macOS, please call this function to register the callback function.
      When the device is inserted or pulled out, you will be notified by the callback funcion, and then call Toupcam_EnumV2(...) again to enum the cameras.
Recommendation: for better rubustness, when notify of device insertion arrives, don't open handle of this device immediately, but open it after delaying a short time (e.g., 200 milliseconds).
*/
typedef void (*PTOUPCAM_HOTPLUG)(void* pCallbackCtx);
toupcam_ports(void)   Toupcam_HotPlug(PTOUPCAM_HOTPLUG pHotPlugCallback, void* pCallbackCtx);

#else

/* Toupcam_Start is obsolete, it's a synonyms for Toupcam_StartPushMode. */
toupcam_deprecated
toupcam_ports(HRESULT)  Toupcam_Start(HToupCam h, PTOUPCAM_DATA_CALLBACK pDataCallback, void* pCallbackCtx);

/* Toupcam_put_TempTintInit is obsolete, it's a synonyms for Toupcam_AwbOnePush. */
toupcam_deprecated
toupcam_ports(HRESULT)  Toupcam_put_TempTintInit(HToupCam h, PITOUPCAM_TEMPTINT_CALLBACK fnTTProc, void* pTTCtx);

/*
    obsolete, please use Toupcam_put_Option or Toupcam_get_Option to set or get the process mode: TOUPCAM_PROCESSMODE_FULL or TOUPCAM_PROCESSMODE_FAST.
    default is TOUPCAM_PROCESSMODE_FULL.
*/
#ifndef __TOUPCAM_PROCESSMODE_DEFINED__
#define __TOUPCAM_PROCESSMODE_DEFINED__
#define TOUPCAM_PROCESSMODE_FULL        0x00    /* better image quality, more cpu usage. this is the default value */
#define TOUPCAM_PROCESSMODE_FAST        0x01    /* lower image quality, less cpu usage */
#endif

toupcam_deprecated
toupcam_ports(HRESULT)  Toupcam_put_ProcessMode(HToupCam h, unsigned nProcessMode);
toupcam_deprecated
toupcam_ports(HRESULT)  Toupcam_get_ProcessMode(HToupCam h, unsigned* pnProcessMode);

#endif

/* obsolete, please use Toupcam_put_Roi and Toupcam_get_Roi */
toupcam_deprecated
toupcam_ports(HRESULT)  Toupcam_put_RoiMode(HToupCam h, int bRoiMode, int xOffset, int yOffset);
toupcam_deprecated
toupcam_ports(HRESULT)  Toupcam_get_RoiMode(HToupCam h, int* pbRoiMode, int* pxOffset, int* pyOffset);

/* obsolete:
     ------------------------------------------------------------|
     | Parameter         |   Range       |   Default             |
     |-----------------------------------------------------------|
     | VidgetAmount      |   -100~100    |   0                   |
     | VignetMidPoint    |   0~100       |   50                  |
     -------------------------------------------------------------
*/
toupcam_ports(HRESULT)  Toupcam_put_VignetEnable(HToupCam h, int bEnable);
toupcam_ports(HRESULT)  Toupcam_get_VignetEnable(HToupCam h, int* bEnable);
toupcam_ports(HRESULT)  Toupcam_put_VignetAmountInt(HToupCam h, int nAmount);
toupcam_ports(HRESULT)  Toupcam_get_VignetAmountInt(HToupCam h, int* nAmount);
toupcam_ports(HRESULT)  Toupcam_put_VignetMidPointInt(HToupCam h, int nMidPoint);
toupcam_ports(HRESULT)  Toupcam_get_VignetMidPointInt(HToupCam h, int* nMidPoint);

/* obsolete flags */
#define TOUPCAM_FLAG_BITDEPTH10    TOUPCAM_FLAG_RAW10  /* pixel format, RAW 10bits */
#define TOUPCAM_FLAG_BITDEPTH12    TOUPCAM_FLAG_RAW12  /* pixel format, RAW 12bits */
#define TOUPCAM_FLAG_BITDEPTH14    TOUPCAM_FLAG_RAW14  /* pixel format, RAW 14bits */
#define TOUPCAM_FLAG_BITDEPTH16    TOUPCAM_FLAG_RAW16  /* pixel format, RAW 16bits */

#ifdef _WIN32
#pragma pack(pop)
#endif

#ifdef __cplusplus
}
#endif

#endif
