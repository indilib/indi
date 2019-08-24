#ifndef __nncam_h__
#define __nncam_h__

/* Version: 34.14088.2019.0307 */
/*
   Platform & Architecture:
       (1) Win32:
              (a) x86: XP SP3 or above; CPU supports SSE2 instruction set or above
              (b) x64: Win7 or above
              (c) arm: Win10 or above
              (d) arm64: Win10 or above
       (2) WinRT: x86, x64, arm, arm64; Win10 or above
       (3) macOS: x86, x64; macOS 10.10 or above
       (4) Linux: kernel 2.6.27 or above
              (a) x86: CPU supports SSE3 instruction set or above; GLIBC 2.8 or above
              (b) x64: GLIBC 2.14 or above
              (c) armel: GLIBC 2.17 or above; built by toolchain arm-linux-gnueabi (version 4.9.2)
              (d) armhf: GLIBC 2.17 or above; built by toolchain arm-linux-gnueabihf (version 4.9.2)
              (e) arm64: GLIBC 2.17 or above; built by toolchain aarch64-linux-gnu (version 4.9.2)
*/
/*
    doc:
       (1) en.html, English
       (2) hans.html, Simplified Chinese
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
#define nncam_deprecated  [[deprecated]]
#elif defined(_MSC_VER)
#define nncam_deprecated  __declspec(deprecated)
#elif defined(__GNUC__) || defined(__clang__)
#define nncam_deprecated  __attribute__((deprecated))
#else
#define nncam_deprecated
#endif

#ifdef _WIN32 /* Windows */

#pragma pack(push, 8)
#ifdef NNCAM_EXPORTS
#define nncam_ports(x)    __declspec(dllexport)   x   __stdcall  /* in Windows, we use __stdcall calling convention, see https://docs.microsoft.com/en-us/cpp/cpp/stdcall */
#elif !defined(NNCAM_NOIMPORTS)
#define nncam_ports(x)    __declspec(dllimport)   x   __stdcall
#else
#define nncam_ports(x)    x   __stdcall
#endif

#else   /* Linux or macOS */

#define nncam_ports(x)    x
#if (!defined(HRESULT)) && (!defined(__COREFOUNDATION_CFPLUGINCOM__)) /* CFPlugInCOM.h */
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
typedef struct NncamT { int unused; } *HNnCam;

#define NNCAM_MAX                     16

#define NNCAM_FLAG_CMOS               0x00000001  /* cmos sensor */
#define NNCAM_FLAG_CCD_PROGRESSIVE    0x00000002  /* progressive ccd sensor */
#define NNCAM_FLAG_CCD_INTERLACED     0x00000004  /* interlaced ccd sensor */
#define NNCAM_FLAG_ROI_HARDWARE       0x00000008  /* support hardware ROI */
#define NNCAM_FLAG_MONO               0x00000010  /* monochromatic */
#define NNCAM_FLAG_BINSKIP_SUPPORTED  0x00000020  /* support bin/skip mode, see Nncam_put_Mode and Nncam_get_Mode */
#define NNCAM_FLAG_USB30              0x00000040  /* usb3.0 */
#define NNCAM_FLAG_TEC                0x00000080  /* Thermoelectric Cooler */
#define NNCAM_FLAG_USB30_OVER_USB20   0x00000100  /* usb3.0 camera connected to usb2.0 port */
#define NNCAM_FLAG_ST4                0x00000200  /* ST4 port */
#define NNCAM_FLAG_GETTEMPERATURE     0x00000400  /* support to get the temperature of the sensor */
#define NNCAM_FLAG_PUTTEMPERATURE     0x00000800  /* support to put the target temperature of the sensor */
#define NNCAM_FLAG_RAW10              0x00001000  /* pixel format, RAW 10bits */
#define NNCAM_FLAG_RAW12              0x00002000  /* pixel format, RAW 12bits */
#define NNCAM_FLAG_RAW14              0x00004000  /* pixel format, RAW 14bits */
#define NNCAM_FLAG_RAW16              0x00008000  /* pixel format, RAW 16bits */
#define NNCAM_FLAG_FAN                0x00010000  /* cooling fan */
#define NNCAM_FLAG_TEC_ONOFF          0x00020000  /* Thermoelectric Cooler can be turn on or off, support to set the target temperature of TEC */
#define NNCAM_FLAG_ISP                0x00040000  /* ISP (Image Signal Processing) chip */
#define NNCAM_FLAG_TRIGGER_SOFTWARE   0x00080000  /* support software trigger */
#define NNCAM_FLAG_TRIGGER_EXTERNAL   0x00100000  /* support external trigger */
#define NNCAM_FLAG_TRIGGER_SINGLE     0x00200000  /* only support trigger single: one trigger, one image */
#define NNCAM_FLAG_BLACKLEVEL         0x00400000  /* support set and get the black level */
#define NNCAM_FLAG_AUTO_FOCUS         0x00800000  /* support auto focus */
#define NNCAM_FLAG_BUFFER             0x01000000  /* frame buffer */
#define NNCAM_FLAG_DDR                0x02000000  /* use very large capacity DDR (Double Data Rate SDRAM) for frame buffer */
#define NNCAM_FLAG_CG                 0x04000000  /* Conversion Gain: HCG, LCG */
#define NNCAM_FLAG_YUV411             0x08000000  /* pixel format, yuv411 */
#define NNCAM_FLAG_VUYY               0x10000000  /* pixel format, yuv422, VUYY */
#define NNCAM_FLAG_YUV444             0x20000000  /* pixel format, yuv444 */
#define NNCAM_FLAG_RGB888             0x40000000  /* pixel format, RGB888 */
#define NNCAM_FLAG_RAW8               0x80000000  /* pixel format, RAW 8 bits */
#define NNCAM_FLAG_GMCY8              0x0000000100000000  /* pixel format, GMCY, 8bits */
#define NNCAM_FLAG_GMCY12             0x0000000200000000  /* pixel format, GMCY, 12bits */
#define NNCAM_FLAG_UYVY               0x0000000400000000  /* pixel format, yuv422, UYVY */
#define NNCAM_FLAG_CGHDR              0x0000000800000000  /* Conversion Gain: HCG, LCG, HDR */
#define NNCAM_FLAG_GLOBALSHUTTER      0x0000001000000000  /* global shutter */
#define NNCAM_FLAG_FOCUSMOTOR         0x0000002000000000  /* support focus motor */

#define NNCAM_TEMP_DEF                 6503    /* temp */
#define NNCAM_TEMP_MIN                 2000    /* temp */
#define NNCAM_TEMP_MAX                 15000   /* temp */
#define NNCAM_TINT_DEF                 1000    /* tint */
#define NNCAM_TINT_MIN                 200     /* tint */
#define NNCAM_TINT_MAX                 2500    /* tint */
#define NNCAM_HUE_DEF                  0       /* hue */
#define NNCAM_HUE_MIN                  (-180)  /* hue */
#define NNCAM_HUE_MAX                  180     /* hue */
#define NNCAM_SATURATION_DEF           128     /* saturation */
#define NNCAM_SATURATION_MIN           0       /* saturation */
#define NNCAM_SATURATION_MAX           255     /* saturation */
#define NNCAM_BRIGHTNESS_DEF           0       /* brightness */
#define NNCAM_BRIGHTNESS_MIN           (-64)   /* brightness */
#define NNCAM_BRIGHTNESS_MAX           64      /* brightness */
#define NNCAM_CONTRAST_DEF             0       /* contrast */
#define NNCAM_CONTRAST_MIN             (-100)  /* contrast */
#define NNCAM_CONTRAST_MAX             100     /* contrast */
#define NNCAM_GAMMA_DEF                100     /* gamma */
#define NNCAM_GAMMA_MIN                20      /* gamma */
#define NNCAM_GAMMA_MAX                180     /* gamma */
#define NNCAM_AETARGET_DEF             120     /* target of auto exposure */
#define NNCAM_AETARGET_MIN             16      /* target of auto exposure */
#define NNCAM_AETARGET_MAX             220     /* target of auto exposure */
#define NNCAM_WBGAIN_DEF               0       /* white balance gain */
#define NNCAM_WBGAIN_MIN               (-127)  /* white balance gain */
#define NNCAM_WBGAIN_MAX               127     /* white balance gain */
#define NNCAM_BLACKLEVEL_MIN           0       /* minimum black level */
#define NNCAM_BLACKLEVEL8_MAX          31              /* maximum black level for bit depth = 8 */
#define NNCAM_BLACKLEVEL10_MAX         (31 * 4)        /* maximum black level for bit depth = 10 */
#define NNCAM_BLACKLEVEL12_MAX         (31 * 16)       /* maximum black level for bit depth = 12 */
#define NNCAM_BLACKLEVEL14_MAX         (31 * 64)       /* maximum black level for bit depth = 14 */
#define NNCAM_BLACKLEVEL16_MAX         (31 * 256)      /* maximum black level for bit depth = 16 */
#define NNCAM_SHARPENING_STRENGTH_DEF  0       /* sharpening strength */
#define NNCAM_SHARPENING_STRENGTH_MIN  0       /* sharpening strength */
#define NNCAM_SHARPENING_STRENGTH_MAX  500     /* sharpening strength */
#define NNCAM_SHARPENING_RADIUS_DEF    2       /* sharpening radius */
#define NNCAM_SHARPENING_RADIUS_MIN    1       /* sharpening radius */
#define NNCAM_SHARPENING_RADIUS_MAX    10      /* sharpening radius */
#define NNCAM_SHARPENING_THRESHOLD_DEF 0       /* sharpening threshold */
#define NNCAM_SHARPENING_THRESHOLD_MIN 0       /* sharpening threshold */
#define NNCAM_SHARPENING_THRESHOLD_MAX 255     /* sharpening threshold */

typedef struct{
    unsigned    width;
    unsigned    height;
}NncamResolution;

/* In Windows platform, we always use UNICODE wchar_t */
/* In Linux or macOS, we use char */

typedef struct {
#ifdef _WIN32
    const wchar_t*      name;        /* model name, in Windows, we use unicode */
#else
    const char*         name;        /* model name */
#endif
    unsigned long long  flag;        /* NNCAM_FLAG_xxx, 64 bits */
    unsigned            maxspeed;    /* number of speed level, same as Nncam_get_MaxSpeed(), the speed range = [0, maxspeed], closed interval */
    unsigned            preview;     /* number of preview resolution, same as Nncam_get_ResolutionNumber() */
    unsigned            still;       /* number of still resolution, same as Nncam_get_StillResolutionNumber() */
    unsigned            maxfanspeed; /* maximum fan speed */
    unsigned            ioctrol;     /* number of input/output control */
    float               xpixsz;      /* physical pixel size */
    float               ypixsz;      /* physical pixel size */
    NncamResolution   res[NNCAM_MAX];
}NncamModelV2; /* camera model v2 */

typedef struct {
#ifdef _WIN32
    wchar_t               displayname[64];    /* display name */
    wchar_t               id[64];             /* unique and opaque id of a connected camera, for Nncam_Open */
#else
    char                  displayname[64];    /* display name */
    char                  id[64];             /* unique and opaque id of a connected camera, for Nncam_Open */
#endif
    const NncamModelV2* model;
}NncamInstV2; /* camera instance for enumerating */

/*
    get the version of this dll/so/dylib, which is: 34.14088.2019.0307
*/
#ifdef _WIN32
nncam_ports(const wchar_t*)   Nncam_Version();
#else
nncam_ports(const char*)      Nncam_Version();
#endif

/*
    enumerate the cameras connected to the computer, return the number of enumerated.

    NncamInstV2 arr[NNCAM_MAX];
    unsigned cnt = Nncam_EnumV2(arr);
    for (unsigned i = 0; i < cnt; ++i)
        ...

    if pti == NULL, then, only the number is returned.
    Nncam_Enum is obsolete.
*/
nncam_ports(unsigned) Nncam_EnumV2(NncamInstV2 pti[NNCAM_MAX]);

/* use the id of NncamInstV2, which is enumerated by Nncam_EnumV2.
    if id is NULL, Nncam_Open will open the first camera.
*/
#ifdef _WIN32
nncam_ports(HNnCam) Nncam_Open(const wchar_t* id);
#else
nncam_ports(HNnCam) Nncam_Open(const char* id);
#endif

/*
    the same with Nncam_Open, but use the index as the parameter. such as:
    index == 0, open the first camera,
    index == 1, open the second camera,
    etc
*/
nncam_ports(HNnCam) Nncam_OpenByIndex(unsigned index);

nncam_ports(void)     Nncam_Close(HNnCam h); /* close the handle */

#define NNCAM_EVENT_EXPOSURE      0x0001    /* exposure time changed */
#define NNCAM_EVENT_TEMPTINT      0x0002    /* white balance changed, Temp/Tint mode */
#define NNCAM_EVENT_IMAGE         0x0004    /* live image arrived, use Nncam_PullImage to get this image */
#define NNCAM_EVENT_STILLIMAGE    0x0005    /* snap (still) frame arrived, use Nncam_PullStillImage to get this frame */
#define NNCAM_EVENT_WBGAIN        0x0006    /* white balance changed, RGB Gain mode */
#define NNCAM_EVENT_TRIGGERFAIL   0x0007    /* trigger failed */
#define NNCAM_EVENT_BLACK         0x0008    /* black balance changed */
#define NNCAM_EVENT_FFC           0x0009    /* flat field correction status changed */
#define NNCAM_EVENT_DFC           0x000a    /* dark field correction status changed */
#define NNCAM_EVENT_ERROR         0x0080    /* generic error */
#define NNCAM_EVENT_DISCONNECTED  0x0081    /* camera disconnected */
#define NNCAM_EVENT_TIMEOUT       0x0082    /* timeout error */
#define NNCAM_EVENT_AFFEEDBACK    0x0083    /* auto focus sensor board positon */
#define NNCAM_EVENT_AFPOSITION    0x0084    /* auto focus information feedback */
#define NNCAM_EVENT_FACTORY       0x8001    /* restore factory settings */

#ifdef _WIN32
nncam_ports(HRESULT)  Nncam_StartPullModeWithWndMsg(HNnCam h, HWND hWnd, UINT nMsg);
#endif

/* Do NOT call Nncam_Close, Nncam_Stop in this callback context, it deadlocks. */
typedef void (__stdcall* PNNCAM_EVENT_CALLBACK)(unsigned nEvent, void* pCallbackCtx);
nncam_ports(HRESULT)  Nncam_StartPullModeWithCallback(HNnCam h, PNNCAM_EVENT_CALLBACK pEventCallback, void* pCallbackContext);

#define NNCAM_FRAMEINFO_FLAG_SEQ          0x01 /* sequence number */
#define NNCAM_FRAMEINFO_FLAG_TIMESTAMP    0x02 /* timestamp */

typedef struct {
    unsigned            width;
    unsigned            height;
    unsigned            flag;       /* NNCAM_FRAMEINFO_FLAG_xxxx */
    unsigned            seq;        /* sequence number */
    unsigned long long  timestamp;  /* microsecond */
}NncamFrameInfoV2;

/*
    bits: 24 (RGB24), 32 (RGB32), 8 (Gray) or 16 (Gray). In RAW mode, this parameter is ignored.
    pnWidth, pnHeight: OUT parameter
    rowPitch: The distance from one row to the next row. rowPitch = 0 means using the default row pitch.
*/
nncam_ports(HRESULT)  Nncam_PullImageV2(HNnCam h, void* pImageData, int bits, NncamFrameInfoV2* pInfo);
nncam_ports(HRESULT)  Nncam_PullStillImageV2(HNnCam h, void* pImageData, int bits, NncamFrameInfoV2* pInfo);
nncam_ports(HRESULT)  Nncam_PullImageWithRowPitchV2(HNnCam h, void* pImageData, int bits, int rowPitch, NncamFrameInfoV2* pInfo);
nncam_ports(HRESULT)  Nncam_PullStillImageWithRowPitchV2(HNnCam h, void* pImageData, int bits, int rowPitch, NncamFrameInfoV2* pInfo);

nncam_ports(HRESULT)  Nncam_PullImage(HNnCam h, void* pImageData, int bits, unsigned* pnWidth, unsigned* pnHeight);
nncam_ports(HRESULT)  Nncam_PullStillImage(HNnCam h, void* pImageData, int bits, unsigned* pnWidth, unsigned* pnHeight);
nncam_ports(HRESULT)  Nncam_PullImageWithRowPitch(HNnCam h, void* pImageData, int bits, int rowPitch, unsigned* pnWidth, unsigned* pnHeight);
nncam_ports(HRESULT)  Nncam_PullStillImageWithRowPitch(HNnCam h, void* pImageData, int bits, int rowPitch, unsigned* pnWidth, unsigned* pnHeight);

/*
    (NULL == pData) means that something is error
    pCallbackCtx is the callback context which is passed by Nncam_Start
    bSnap: TRUE if Nncam_Snap

    pDataCallback is callbacked by an internal thread of nncam.dll, so please pay attention to multithread problem.
    Do NOT call Nncam_Close, Nncam_Stop in this callback context, it deadlocks.
*/
typedef void (__stdcall* PNNCAM_DATA_CALLBACK_V2)(const void* pData, const NncamFrameInfoV2* pInfo, int bSnap, void* pCallbackCtx);
nncam_ports(HRESULT)  Nncam_StartPushModeV2(HNnCam h, PNNCAM_DATA_CALLBACK_V2 pDataCallback, void* pCallbackCtx);

typedef void (__stdcall* PNNCAM_DATA_CALLBACK)(const void* pData, const BITMAPINFOHEADER* pHeader, int bSnap, void* pCallbackCtx);
nncam_ports(HRESULT)  Nncam_StartPushMode(HNnCam h, PNNCAM_DATA_CALLBACK pDataCallback, void* pCallbackCtx);

nncam_ports(HRESULT)  Nncam_Stop(HNnCam h);
nncam_ports(HRESULT)  Nncam_Pause(HNnCam h, int bPause);

/*  for pull mode: NNCAM_EVENT_STILLIMAGE, and then Nncam_PullStillImage
    for push mode: the snapped image will be return by PNNCAM_DATA_CALLBACK, with the parameter 'bSnap' set to 'TRUE'
*/
nncam_ports(HRESULT)  Nncam_Snap(HNnCam h, unsigned nResolutionIndex);  /* still image snap */
nncam_ports(HRESULT)  Nncam_SnapN(HNnCam h, unsigned nResolutionIndex, unsigned nNumber);  /* multiple still image snap */
/*
    soft trigger:
    nNumber:    0xffff:     trigger continuously
                0:          cancel trigger
                others:     number of images to be triggered
*/
nncam_ports(HRESULT)  Nncam_Trigger(HNnCam h, unsigned short nNumber);

/*
    put_Size, put_eSize, can be used to set the video output resolution BEFORE NnCam_Start.
    put_Size use width and height parameters, put_eSize use the index parameter.
    for example, UCMOS03100KPA support the following resolutions:
            index 0:    2048,   1536
            index 1:    1024,   768
            index 2:    680,    510
    so, we can use put_Size(h, 1024, 768) or put_eSize(h, 1). Both have the same effect.
*/
nncam_ports(HRESULT)  Nncam_put_Size(HNnCam h, int nWidth, int nHeight);
nncam_ports(HRESULT)  Nncam_get_Size(HNnCam h, int* pWidth, int* pHeight);
nncam_ports(HRESULT)  Nncam_put_eSize(HNnCam h, unsigned nResolutionIndex);
nncam_ports(HRESULT)  Nncam_get_eSize(HNnCam h, unsigned* pnResolutionIndex);

nncam_ports(HRESULT)  Nncam_get_ResolutionNumber(HNnCam h);
nncam_ports(HRESULT)  Nncam_get_Resolution(HNnCam h, unsigned nResolutionIndex, int* pWidth, int* pHeight);
nncam_ports(HRESULT)  Nncam_get_ResolutionRatio(HNnCam h, unsigned nResolutionIndex, int* pNumerator, int* pDenominator);
nncam_ports(HRESULT)  Nncam_get_Field(HNnCam h);

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
nncam_ports(HRESULT)  Nncam_get_RawFormat(HNnCam h, unsigned* nFourCC, unsigned* bitsperpixel);

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

#ifndef __NNCAM_CALLBACK_DEFINED__
#define __NNCAM_CALLBACK_DEFINED__
typedef void (__stdcall* PINNCAM_EXPOSURE_CALLBACK)(void* pCtx);                                     /* auto exposure */
typedef void (__stdcall* PINNCAM_WHITEBALANCE_CALLBACK)(const int aGain[3], void* pCtx);             /* one push white balance, RGB Gain mode */
typedef void (__stdcall* PINNCAM_BLACKBALANCE_CALLBACK)(const unsigned short aSub[3], void* pCtx);   /* one push black balance */
typedef void (__stdcall* PINNCAM_TEMPTINT_CALLBACK)(const int nTemp, const int nTint, void* pCtx);   /* one push white balance, Temp/Tint Mode */
typedef void (__stdcall* PINNCAM_HISTOGRAM_CALLBACK)(const float aHistY[256], const float aHistR[256], const float aHistG[256], const float aHistB[256], void* pCtx);
typedef void (__stdcall* PINNCAM_CHROME_CALLBACK)(void* pCtx);
#endif

nncam_ports(HRESULT)  Nncam_get_AutoExpoEnable(HNnCam h, int* bAutoExposure);
nncam_ports(HRESULT)  Nncam_put_AutoExpoEnable(HNnCam h, int bAutoExposure);
nncam_ports(HRESULT)  Nncam_get_AutoExpoTarget(HNnCam h, unsigned short* Target);
nncam_ports(HRESULT)  Nncam_put_AutoExpoTarget(HNnCam h, unsigned short Target);

/*set the maximum/minimal auto exposure time and analog agin. The default maximum auto exposure time is 350ms */
nncam_ports(HRESULT)  Nncam_put_MaxAutoExpoTimeAGain(HNnCam h, unsigned maxTime, unsigned short maxAGain);
nncam_ports(HRESULT)  Nncam_get_MaxAutoExpoTimeAGain(HNnCam h, unsigned* maxTime, unsigned short* maxAGain);
nncam_ports(HRESULT)  Nncam_put_MinAutoExpoTimeAGain(HNnCam h, unsigned minTime, unsigned short minAGain);
nncam_ports(HRESULT)  Nncam_get_MinAutoExpoTimeAGain(HNnCam h, unsigned* minTime, unsigned short* minAGain);

nncam_ports(HRESULT)  Nncam_get_ExpoTime(HNnCam h, unsigned* Time); /* in microseconds */
nncam_ports(HRESULT)  Nncam_put_ExpoTime(HNnCam h, unsigned Time); /* in microseconds */
nncam_ports(HRESULT)  Nncam_get_RealExpoTime(HNnCam h, unsigned* Time); /* in microseconds, based on 50HZ/60HZ/DC */
nncam_ports(HRESULT)  Nncam_get_ExpTimeRange(HNnCam h, unsigned* nMin, unsigned* nMax, unsigned* nDef);

nncam_ports(HRESULT)  Nncam_get_ExpoAGain(HNnCam h, unsigned short* AGain); /* percent, such as 300 */
nncam_ports(HRESULT)  Nncam_put_ExpoAGain(HNnCam h, unsigned short AGain); /* percent */
nncam_ports(HRESULT)  Nncam_get_ExpoAGainRange(HNnCam h, unsigned short* nMin, unsigned short* nMax, unsigned short* nDef);

/* Auto White Balance, Temp/Tint Mode */
nncam_ports(HRESULT)  Nncam_AwbOnePush(HNnCam h, PINNCAM_TEMPTINT_CALLBACK fnTTProc, void* pTTCtx); /* auto white balance "one push". This function must be called AFTER Nncam_StartXXXX */

/* Auto White Balance, RGB Gain Mode */
nncam_ports(HRESULT)  Nncam_AwbInit(HNnCam h, PINNCAM_WHITEBALANCE_CALLBACK fnWBProc, void* pWBCtx);

/* White Balance, Temp/Tint mode */
nncam_ports(HRESULT)  Nncam_put_TempTint(HNnCam h, int nTemp, int nTint);
nncam_ports(HRESULT)  Nncam_get_TempTint(HNnCam h, int* nTemp, int* nTint);

/* White Balance, RGB Gain mode */
nncam_ports(HRESULT)  Nncam_put_WhiteBalanceGain(HNnCam h, int aGain[3]);
nncam_ports(HRESULT)  Nncam_get_WhiteBalanceGain(HNnCam h, int aGain[3]);

/* Black Balance */
nncam_ports(HRESULT)  Nncam_AbbOnePush(HNnCam h, PINNCAM_BLACKBALANCE_CALLBACK fnBBProc, void* pBBCtx); /* auto black balance "one push". This function must be called AFTER Nncam_StartXXXX */
nncam_ports(HRESULT)  Nncam_put_BlackBalance(HNnCam h, unsigned short aSub[3]);
nncam_ports(HRESULT)  Nncam_get_BlackBalance(HNnCam h, unsigned short aSub[3]);

/* Flat Field Correction */
nncam_ports(HRESULT)  Nncam_FfcOnePush(HNnCam h);
#ifdef _WIN32
nncam_ports(HRESULT)  Nncam_FfcExport(HNnCam h, const wchar_t* filepath);
nncam_ports(HRESULT)  Nncam_FfcImport(HNnCam h, const wchar_t* filepath);
#else
nncam_ports(HRESULT)  Nncam_FfcExport(HNnCam h, const char* filepath);
nncam_ports(HRESULT)  Nncam_FfcImport(HNnCam h, const char* filepath);
#endif

/* Dark Field Correction */
nncam_ports(HRESULT)  Nncam_DfcOnePush(HNnCam h);

#ifdef _WIN32
nncam_ports(HRESULT)  Nncam_DfcExport(HNnCam h, const wchar_t* filepath);
nncam_ports(HRESULT)  Nncam_DfcImport(HNnCam h, const wchar_t* filepath);
#else
nncam_ports(HRESULT)  Nncam_DfcExport(HNnCam h, const char* filepath);
nncam_ports(HRESULT)  Nncam_DfcImport(HNnCam h, const char* filepath);
#endif

nncam_ports(HRESULT)  Nncam_put_Hue(HNnCam h, int Hue);
nncam_ports(HRESULT)  Nncam_get_Hue(HNnCam h, int* Hue);
nncam_ports(HRESULT)  Nncam_put_Saturation(HNnCam h, int Saturation);
nncam_ports(HRESULT)  Nncam_get_Saturation(HNnCam h, int* Saturation);
nncam_ports(HRESULT)  Nncam_put_Brightness(HNnCam h, int Brightness);
nncam_ports(HRESULT)  Nncam_get_Brightness(HNnCam h, int* Brightness);
nncam_ports(HRESULT)  Nncam_get_Contrast(HNnCam h, int* Contrast);
nncam_ports(HRESULT)  Nncam_put_Contrast(HNnCam h, int Contrast);
nncam_ports(HRESULT)  Nncam_get_Gamma(HNnCam h, int* Gamma); /* percent */
nncam_ports(HRESULT)  Nncam_put_Gamma(HNnCam h, int Gamma);  /* percent */

nncam_ports(HRESULT)  Nncam_get_Chrome(HNnCam h, int* bChrome);  /* monochromatic mode */
nncam_ports(HRESULT)  Nncam_put_Chrome(HNnCam h, int bChrome);

nncam_ports(HRESULT)  Nncam_get_VFlip(HNnCam h, int* bVFlip);  /* vertical flip */
nncam_ports(HRESULT)  Nncam_put_VFlip(HNnCam h, int bVFlip);
nncam_ports(HRESULT)  Nncam_get_HFlip(HNnCam h, int* bHFlip);
nncam_ports(HRESULT)  Nncam_put_HFlip(HNnCam h, int bHFlip); /* horizontal flip */

nncam_ports(HRESULT)  Nncam_get_Negative(HNnCam h, int* bNegative);  /* negative film */
nncam_ports(HRESULT)  Nncam_put_Negative(HNnCam h, int bNegative);

nncam_ports(HRESULT)  Nncam_put_Speed(HNnCam h, unsigned short nSpeed);
nncam_ports(HRESULT)  Nncam_get_Speed(HNnCam h, unsigned short* pSpeed);
nncam_ports(HRESULT)  Nncam_get_MaxSpeed(HNnCam h); /* get the maximum speed, see "Frame Speed Level", the speed range = [0, max], closed interval */

nncam_ports(HRESULT)  Nncam_get_FanMaxSpeed(HNnCam h); /* get the maximum fan speed, the fan speed range = [0, max], closed interval */

nncam_ports(HRESULT)  Nncam_get_MaxBitDepth(HNnCam h); /* get the max bit depth of this camera, such as 8, 10, 12, 14, 16 */

/* power supply of lighting:
        0 -> 60HZ AC
        1 -> 50Hz AC
        2 -> DC
*/
nncam_ports(HRESULT)  Nncam_put_HZ(HNnCam h, int nHZ);
nncam_ports(HRESULT)  Nncam_get_HZ(HNnCam h, int* nHZ);

nncam_ports(HRESULT)  Nncam_put_Mode(HNnCam h, int bSkip); /* skip or bin */
nncam_ports(HRESULT)  Nncam_get_Mode(HNnCam h, int* bSkip); /* If the model don't support bin/skip mode, return E_NOTIMPL */

nncam_ports(HRESULT)  Nncam_put_AWBAuxRect(HNnCam h, const RECT* pAuxRect); /* auto white balance ROI */
nncam_ports(HRESULT)  Nncam_get_AWBAuxRect(HNnCam h, RECT* pAuxRect);
nncam_ports(HRESULT)  Nncam_put_AEAuxRect(HNnCam h, const RECT* pAuxRect);  /* auto exposure ROI */
nncam_ports(HRESULT)  Nncam_get_AEAuxRect(HNnCam h, RECT* pAuxRect);

nncam_ports(HRESULT)  Nncam_put_ABBAuxRect(HNnCam h, const RECT* pAuxRect); /* auto black balance ROI */
nncam_ports(HRESULT)  Nncam_get_ABBAuxRect(HNnCam h, RECT* pAuxRect);

/*
    S_FALSE:    color mode
    S_OK:       mono mode, such as EXCCD00300KMA and UHCCD01400KMA
*/
nncam_ports(HRESULT)  Nncam_get_MonoMode(HNnCam h);

nncam_ports(HRESULT)  Nncam_get_StillResolutionNumber(HNnCam h);
nncam_ports(HRESULT)  Nncam_get_StillResolution(HNnCam h, unsigned nResolutionIndex, int* pWidth, int* pHeight);

/* use minimum frame buffer.
    If DDR present, also limit the DDR frame buffer to only one frame.
    default: FALSE
*/
nncam_ports(HRESULT)  Nncam_put_RealTime(HNnCam h, int bEnable);
nncam_ports(HRESULT)  Nncam_get_RealTime(HNnCam h, int* bEnable);

/* discard the current internal frame cache.
    If DDR present, also discard the frames in the DDR.
*/
nncam_ports(HRESULT)  Nncam_Flush(HNnCam h);

/* get the temperature of the sensor, in 0.1 degrees Celsius (32 means 3.2 degrees Celsius, -35 means -3.5 degree Celsius)
    return E_NOTIMPL if not supported
*/
nncam_ports(HRESULT)  Nncam_get_Temperature(HNnCam h, short* pTemperature);

/* set the target temperature of the sensor or TEC, in 0.1 degrees Celsius (32 means 3.2 degrees Celsius, -35 means -3.5 degree Celsius)
    return E_NOTIMPL if not supported
*/
nncam_ports(HRESULT)  Nncam_put_Temperature(HNnCam h, short nTemperature);

/*
    get the revision
*/
nncam_ports(HRESULT)  Nncam_get_Revision(HNnCam h, unsigned short* pRevision);

/*
    get the serial number which is always 32 chars which is zero-terminated such as "TP110826145730ABCD1234FEDC56787"
*/
nncam_ports(HRESULT)  Nncam_get_SerialNumber(HNnCam h, char sn[32]);

/*
    get the camera firmware version, such as: 3.2.1.20140922
*/
nncam_ports(HRESULT)  Nncam_get_FwVersion(HNnCam h, char fwver[16]);

/*
    get the camera hardware version, such as: 3.12
*/
nncam_ports(HRESULT)  Nncam_get_HwVersion(HNnCam h, char hwver[16]);

/*
    get the production date, such as: 20150327, YYYYMMDD, (YYYY: year, MM: month, DD: day)
*/
nncam_ports(HRESULT)  Nncam_get_ProductionDate(HNnCam h, char pdate[10]);

/*
    get the FPGA version, such as: 1.13
*/
nncam_ports(HRESULT)  Nncam_get_FpgaVersion(HNnCam h, char fpgaver[16]);

/*
    get the sensor pixel size, such as: 2.4um
*/
nncam_ports(HRESULT)  Nncam_get_PixelSize(HNnCam h, unsigned nResolutionIndex, float* x, float* y);

nncam_ports(HRESULT)  Nncam_put_LevelRange(HNnCam h, unsigned short aLow[4], unsigned short aHigh[4]);
nncam_ports(HRESULT)  Nncam_get_LevelRange(HNnCam h, unsigned short aLow[4], unsigned short aHigh[4]);

nncam_ports(HRESULT)  Nncam_put_ExpoCallback(HNnCam h, PINNCAM_EXPOSURE_CALLBACK fnExpoProc, void* pExpoCtx);
nncam_ports(HRESULT)  Nncam_put_ChromeCallback(HNnCam h, PINNCAM_CHROME_CALLBACK fnChromeProc, void* pChromeCtx);

/*
    The following functions must be called AFTER Nncam_StartPushMode or Nncam_StartPullModeWithWndMsg or Nncam_StartPullModeWithCallback
*/
nncam_ports(HRESULT)  Nncam_LevelRangeAuto(HNnCam h);
nncam_ports(HRESULT)  Nncam_GetHistogram(HNnCam h, PINNCAM_HISTOGRAM_CALLBACK fnHistogramProc, void* pHistogramCtx);

/* led state:
    iLed: Led index, (0, 1, 2, ...)
    iState: 1 -> Ever bright; 2 -> Flashing; other -> Off
    iPeriod: Flashing Period (>= 500ms)
*/
nncam_ports(HRESULT)  Nncam_put_LEDState(HNnCam h, unsigned short iLed, unsigned short iState, unsigned short iPeriod);

nncam_ports(HRESULT)  Nncam_write_EEPROM(HNnCam h, unsigned addr, const unsigned char* pBuffer, unsigned nBufferLen);
nncam_ports(HRESULT)  Nncam_read_EEPROM(HNnCam h, unsigned addr, unsigned char* pBuffer, unsigned nBufferLen);

nncam_ports(HRESULT)  Nncam_read_Pipe(HNnCam h, unsigned pipeNum, void* pBuffer, unsigned nBufferLen);
nncam_ports(HRESULT)  Nncam_write_Pipe(HNnCam h, unsigned pipeNum, const void* pBuffer, unsigned nBufferLen);
nncam_ports(HRESULT)  Nncam_feed_Pipe(HNnCam h, unsigned pipeNum);

#define NNCAM_TEC_TARGET_MIN          (-300)  /* -30.0 degrees Celsius */
#define NNCAM_TEC_TARGET_DEF          0       /* 0.0 degrees Celsius */
#define NNCAM_TEC_TARGET_MAX          300     /* 30.0 degrees Celsius */

#define NNCAM_OPTION_NOFRAME_TIMEOUT  0x01    /* 1 = enable; 0 = disable. default: disable */
#define NNCAM_OPTION_THREAD_PRIORITY  0x02    /* set the priority of the internal thread which grab data from the usb device. iValue: 0 = THREAD_PRIORITY_NORMAL; 1 = THREAD_PRIORITY_ABOVE_NORMAL; 2 = THREAD_PRIORITY_HIGHEST; default: 0; see: msdn SetThreadPriority */
#define NNCAM_OPTION_PROCESSMODE      0x03    /* 0 = better image quality, more cpu usage. this is the default value
                                                   1 = lower image quality, less cpu usage */
#define NNCAM_OPTION_RAW              0x04    /* raw data mode, read the sensor "raw" data. This can be set only BEFORE Nncam_StartXXX(). 0 = rgb, 1 = raw, default value: 0 */
#define NNCAM_OPTION_HISTOGRAM        0x05    /* 0 = only one, 1 = continue mode */
#define NNCAM_OPTION_BITDEPTH         0x06    /* 0 = 8 bits mode, 1 = 16 bits mode, subset of NNCAM_OPTION_PIXEL_FORMAT */
#define NNCAM_OPTION_FAN              0x07    /* 0 = turn off the cooling fan, [1, max] = fan speed */
#define NNCAM_OPTION_TEC              0x08    /* 0 = turn off the thermoelectric cooler, 1 = turn on the thermoelectric cooler */
#define NNCAM_OPTION_LINEAR           0x09    /* 0 = turn off the builtin linear tone mapping, 1 = turn on the builtin linear tone mapping, default value: 1 */
#define NNCAM_OPTION_CURVE            0x0a    /* 0 = turn off the builtin curve tone mapping, 1 = turn on the builtin polynomial curve tone mapping, 2 = logarithmic curve tone mapping, default value: 2 */
#define NNCAM_OPTION_TRIGGER          0x0b    /* 0 = video mode, 1 = software or simulated trigger mode, 2 = external trigger mode, default value = 0 */
#define NNCAM_OPTION_RGB              0x0c    /* 0 => RGB24; 1 => enable RGB48 format when bitdepth > 8; 2 => RGB32; 3 => 8 Bits Gray (only for mono camera); 4 => 16 Bits Gray (only for mono camera when bitdepth > 8) */
#define NNCAM_OPTION_COLORMATIX       0x0d    /* enable or disable the builtin color matrix, default value: 1 */
#define NNCAM_OPTION_WBGAIN           0x0e    /* enable or disable the builtin white balance gain, default value: 1 */
#define NNCAM_OPTION_TECTARGET        0x0f    /* get or set the target temperature of the thermoelectric cooler, in 0.1 degree Celsius. For example, 125 means 12.5 degree Celsius, -35 means -3.5 degree Celsius */
#define NNCAM_OPTION_AUTOEXP_POLICY   0x10    /* auto exposure policy:
                                                    0: Exposure Only
                                                    1: Exposure Preferred
                                                    2: Analog Gain Only
                                                    3: Analog Gain Preferred
                                                    default value: 1
                                                */
#define NNCAM_OPTION_FRAMERATE        0x11    /* limit the frame rate, range=[0, 63], the default value 0 means no limit */
#define NNCAM_OPTION_DEMOSAIC         0x12    /* demosaic method for both video and still image: BILINEAR = 0, VNG(Variable Number of Gradients interpolation) = 1, PPG(Patterned Pixel Grouping interpolation) = 2, AHD(Adaptive Homogeneity-Directed interpolation) = 3, see https://en.wikipedia.org/wiki/Demosaicing, default value: 0 */
#define NNCAM_OPTION_DEMOSAIC_VIDEO   0x13    /* demosaic method for video */
#define NNCAM_OPTION_DEMOSAIC_STILL   0x14    /* demosaic method for still image */
#define NNCAM_OPTION_BLACKLEVEL       0x15    /* black level */
#define NNCAM_OPTION_MULTITHREAD      0x16    /* multithread image processing */
#define NNCAM_OPTION_BINNING          0x17    /* binning, 0x01 (no binning), 0x02 (add, 2*2), 0x03 (add, 3*3), 0x04 (add, 4*4), 0x82 (average, 2*2), 0x83 (average, 3*3), 0x84 (average, 4*4) */
#define NNCAM_OPTION_ROTATE           0x18    /* rotate clockwise: 0, 90, 180, 270 */
#define NNCAM_OPTION_CG               0x19    /* Conversion Gain: 0 = LCG, 1 = HCG, 2 = HDR */
#define NNCAM_OPTION_PIXEL_FORMAT     0x1a    /* pixel format, NNCAM_PIXELFORMAT_xxxx */
#define NNCAM_OPTION_FFC              0x1b    /* flat field correction
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
#define NNCAM_OPTION_DDR_DEPTH        0x1c    /* the number of the frames that DDR can cache
                                                        1: DDR cache only one frame
                                                        0: Auto:
                                                                ->one for video mode when auto exposure is enabled
                                                                ->full capacity for others
                                                       -1: DDR can cache frames to full capacity
                                                */
#define NNCAM_OPTION_DFC              0x1d    /* dark field correction
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
#define NNCAM_OPTION_SHARPENING       0x1e    /* Sharpening: (threshold << 24) | (radius << 16) | strength)
                                                    strength: [0, 500], default: 0 (disable)
                                                    radius: [1, 10]
                                                    threshold: [0, 255]
                                                */
#define NNCAM_OPTION_FACTORY          0x1f    /* restore the factory settings */
#define NNCAM_OPTION_TEC_VOLTAGE      0x20    /* get the current TEC voltage in 0.1V, 59 mean 5.9V; readonly */
#define NNCAM_OPTION_TEC_VOLTAGE_MAX  0x21    /* get the TEC maximum voltage in 0.1V; readonly */
#define NNCAM_OPTION_DEVICE_RESET     0x22    /* reset usb device, simulate a replug */
#define NNCAM_OPTION_UPSIDE_DOWN      0x23    /* upsize down:
                                                    1: yes
                                                    0: no
                                                    default: 1 (win), 0 (linux/macos)
                                                */
#define NNCAM_OPTION_AFPOSITION       0x24    /* auto focus sensor board positon */
#define NNCAM_OPTION_AFMODE           0x25    /* auto focus mode (0:manul focus; 1:auto focus; 2:onepush focus; 3:conjugate calibration) */
#define NNCAM_OPTION_AFZONE           0x26    /* auto focus zone */
#define NNCAM_OPTION_AFFEEDBACK       0x27    /* auto focus information feedback; 0:unknown; 1:focused; 2:focusing; 3:defocus; 4:up; 5:down */

#define NNCAM_PIXELFORMAT_RAW8        0x00
#define NNCAM_PIXELFORMAT_RAW10       0x01
#define NNCAM_PIXELFORMAT_RAW12       0x02
#define NNCAM_PIXELFORMAT_RAW14       0x03
#define NNCAM_PIXELFORMAT_RAW16       0x04
#define NNCAM_PIXELFORMAT_YUV411      0x05
#define NNCAM_PIXELFORMAT_VUYY        0x06
#define NNCAM_PIXELFORMAT_YUV444      0x07
#define NNCAM_PIXELFORMAT_RGB888      0x08
#define NNCAM_PIXELFORMAT_GMCY8       0x09   /* map to RGGB 8 bits */
#define NNCAM_PIXELFORMAT_GMCY12      0x0a   /* map to RGGB 12 bits */
#define NNCAM_PIXELFORMAT_UYVY        0x0b

nncam_ports(HRESULT)  Nncam_put_Option(HNnCam h, unsigned iOption, int iValue);
nncam_ports(HRESULT)  Nncam_get_Option(HNnCam h, unsigned iOption, int* piValue);

nncam_ports(HRESULT)  Nncam_put_Roi(HNnCam h, unsigned xOffset, unsigned yOffset, unsigned xWidth, unsigned yHeight);
nncam_ports(HRESULT)  Nncam_get_Roi(HNnCam h, unsigned* pxOffset, unsigned* pyOffset, unsigned* pxWidth, unsigned* pyHeight);

#ifndef __NNCAMAFPARAM_DEFINED__
#define __NNCAMAFPARAM_DEFINED__
typedef struct {
    int imax;    /* maximum auto focus sensor board positon */
    int imin;    /* minimum auto focus sensor board positon */
    int idef;    /* conjugate calibration positon */
    int imaxabs; /* maximum absolute auto focus sensor board positon, micrometer */
    int iminabs; /* maximum absolute auto focus sensor board positon, micrometer */
    int zoneh;   /* zone horizontal */
    int zonev;   /* zone vertical */
}NncamAfParam;
#endif

nncam_ports(HRESULT)  Nncam_get_AfParam(HNnCam h, NncamAfParam* pAfParam);

#define NNCAM_IOCONTROLTYPE_GET_SUPPORTEDMODE           0x01 /* 0x01->Input, 0x02->Output, (0x01 | 0x02)->support both Input and Output */
#define NNCAM_IOCONTROLTYPE_GET_GPIODIR                 0x03 /* 0x01->Input, 0x02->Output */
#define NNCAM_IOCONTROLTYPE_SET_GPIODIR                 0x04
#define NNCAM_IOCONTROLTYPE_GET_FORMAT                  0x05 /*
                                                                   0x00-> not connected
                                                                   0x01-> Tri-state: Tri-state mode (Not driven)
                                                                   0x02-> TTL: TTL level signals
                                                                   0x03-> LVDS: LVDS level signals
                                                                   0x04-> RS422: RS422 level signals
                                                                   0x05-> Opto-coupled
                                                               */
#define NNCAM_IOCONTROLTYPE_SET_FORMAT                  0x06
#define NNCAM_IOCONTROLTYPE_GET_OUTPUTINVERTER          0x07 /* boolean, only support output signal */
#define NNCAM_IOCONTROLTYPE_SET_OUTPUTINVERTER          0x08
#define NNCAM_IOCONTROLTYPE_GET_INPUTACTIVATION         0x09 /* 0x01->Positive, 0x02->Negative */
#define NNCAM_IOCONTROLTYPE_SET_INPUTACTIVATION         0x0a
#define NNCAM_IOCONTROLTYPE_GET_DEBOUNCERTIME           0x0b /* debouncer time in microseconds */
#define NNCAM_IOCONTROLTYPE_SET_DEBOUNCERTIME           0x0c
#define NNCAM_IOCONTROLTYPE_GET_TRIGGERSOURCE           0x0d /*
                                                                  0x00-> Opto-isolated input
                                                                  0x01-> GPIO0
                                                                  0x02-> GPIO1
                                                                  0x03-> Counter
                                                                  0x04-> PWM
                                                                  0x05-> Software
                                                               */
#define NNCAM_IOCONTROLTYPE_SET_TRIGGERSOURCE           0x0e
#define NNCAM_IOCONTROLTYPE_GET_TRIGGERDELAY            0x0f /* Trigger delay time in microseconds */
#define NNCAM_IOCONTROLTYPE_SET_TRIGGERDELAY            0x10
#define NNCAM_IOCONTROLTYPE_GET_BURSTCOUNTER            0x11 /* Burst Counter: 1, 2, 3 ... 1023 */
#define NNCAM_IOCONTROLTYPE_SET_BURSTCOUNTER            0x12
#define NNCAM_IOCONTROLTYPE_GET_COUNTERSOURCE           0x13 /* 0x00-> Opto-isolated input, 0x01-> GPIO0, 0x02-> GPIO1 */
#define NNCAM_IOCONTROLTYPE_SET_COUNTERSOURCE           0x14
#define NNCAM_IOCONTROLTYPE_GET_COUNTERVALUE            0x15 /* Counter Value: 1, 2, 3 ... 1023 */
#define NNCAM_IOCONTROLTYPE_SET_COUNTERVALUE            0x16
#define NNCAM_IOCONTROLTYPE_SET_RESETCOUNTER            0x18
#define NNCAM_IOCONTROLTYPE_GET_PWM_FREQ                0x19
#define NNCAM_IOCONTROLTYPE_SET_PWM_FREQ                0x1a
#define NNCAM_IOCONTROLTYPE_GET_PWM_DUTYRATIO           0x1b
#define NNCAM_IOCONTROLTYPE_SET_PWM_DUTYRATIO           0x1c
#define NNCAM_IOCONTROLTYPE_GET_PWMSOURCE               0x1d /* 0x00-> Opto-isolated input, 0x01-> GPIO0, 0x02-> GPIO1 */
#define NNCAM_IOCONTROLTYPE_SET_PWMSOURCE               0x1e
#define NNCAM_IOCONTROLTYPE_GET_OUTPUTMODE              0x1f /*
                                                                  0x00-> Frame Trigger Wait
                                                                  0x01-> Exposure Active
                                                                  0x02-> Strobe
                                                                  0x03-> User output
                                                               */
#define NNCAM_IOCONTROLTYPE_SET_OUTPUTMODE              0x20
#define NNCAM_IOCONTROLTYPE_GET_STROBEDELAYMODE         0x21 /* boolean, 1 -> delay, 0 -> pre-delay; compared to exposure active signal */
#define NNCAM_IOCONTROLTYPE_SET_STROBEDELAYMODE         0x22
#define NNCAM_IOCONTROLTYPE_GET_STROBEDELAYTIME         0x23 /* Strobe delay or pre-delay time in microseconds */
#define NNCAM_IOCONTROLTYPE_SET_STROBEDELAYTIME         0x24
#define NNCAM_IOCONTROLTYPE_GET_STROBEDURATION          0x25 /* Strobe duration time in microseconds */
#define NNCAM_IOCONTROLTYPE_SET_STROBEDURATION          0x26
#define NNCAM_IOCONTROLTYPE_GET_USERVALUE               0x27 /* 
                                                                  bit0-> Opto-isolated output
                                                                  bit1-> GPIO0 output
                                                                  bit2-> GPIO1 output
                                                               */
#define NNCAM_IOCONTROLTYPE_SET_USERVALUE               0x28

nncam_ports(HRESULT)  Nncam_IoControl(HNnCam h, unsigned index, unsigned nType, int outVal, int* inVal);

nncam_ports(HRESULT)  Nncam_write_UART(HNnCam h, const unsigned char* pData, unsigned nDataLen);
nncam_ports(HRESULT)  Nncam_read_UART(HNnCam h, unsigned char* pBuffer, unsigned nBufferLen);

nncam_ports(HRESULT)  Nncam_put_Linear(HNnCam h, const unsigned char* v8, const unsigned short* v16);
nncam_ports(HRESULT)  Nncam_put_Curve(HNnCam h, const unsigned char* v8, const unsigned short* v16);
nncam_ports(HRESULT)  Nncam_put_ColorMatrix(HNnCam h, const double v[9]);
nncam_ports(HRESULT)  Nncam_put_InitWBGain(HNnCam h, const unsigned short v[3]);

/*
    get the frame rate: framerate (fps) = Frame * 1000.0 / nTime
*/
nncam_ports(HRESULT)  Nncam_get_FrameRate(HNnCam h, unsigned* nFrame, unsigned* nTime, unsigned* nTotalFrame);

/* astronomy: for ST4 guide, please see: ASCOM Platform Help ICameraV2.
    nDirect: 0 = North, 1 = South, 2 = East, 3 = West, 4 = Stop
    nDuration: in milliseconds
*/
nncam_ports(HRESULT)  Nncam_ST4PlusGuide(HNnCam h, unsigned nDirect, unsigned nDuration);

/* S_OK: ST4 pulse guiding
   S_FALSE: ST4 not pulse guiding
*/
nncam_ports(HRESULT)  Nncam_ST4PlusGuideState(HNnCam h);

nncam_ports(HRESULT)  Nncam_InitOcl();

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
}NncamOclWithSharedTexture;
nncam_ports(HRESULT)  Nncam_StartOclWithSharedTexture(HNnCam h, const NncamOclWithSharedTexture* pocl, PNNCAM_EVENT_CALLBACK pEventCallback, void* pCallbackContext);

/*
    calculate the clarity factor:
    pImageData: pointer to the image data
    bits: 8(Grey), 24 (RGB24), 32(RGB32)
    nImgWidth, nImgHeight: the image width and height
*/
nncam_ports(double)   Nncam_calc_ClarityFactor(const void* pImageData, int bits, unsigned nImgWidth, unsigned nImgHeight);

/*
    nBitCount: output bitmap bit count
    when nBitDepth == 8:
        nBitCount must be 24 or 32
    when nBitDepth > 8
        nBitCount:  24 -> RGB24
                    32 -> RGB32
                    48 -> RGB48
                    64 -> RGB64
*/
nncam_ports(void)     Nncam_deBayerV2(unsigned nBayer, int nW, int nH, const void* input, void* output, unsigned char nBitDepth, unsigned char nBitCount);

/*
    obsolete, please use Nncam_deBayerV2
*/
nncam_deprecated
nncam_ports(void)     Nncam_deBayer(unsigned nBayer, int nW, int nH, const void* input, void* output, unsigned char nBitDepth);

typedef void (__stdcall* PNNCAM_DEMOSAIC_CALLBACK)(unsigned nBayer, int nW, int nH, const void* input, void* output, unsigned char nBitDepth, void* pCallbackCtx);
nncam_ports(HRESULT)  Nncam_put_Demosaic(HNnCam h, PNNCAM_DEMOSAIC_CALLBACK pCallback, void* pCallbackCtx);

/*
    obsolete, please use NncamModelV2
*/
typedef struct {
#ifdef _WIN32
    const wchar_t*      name;       /* model name, in Windows, we use unicode */
#else
    const char*         name;       /* model name */
#endif
    unsigned            flag;       /* NNCAM_FLAG_xxx */
    unsigned            maxspeed;   /* number of speed level, same as Nncam_get_MaxSpeed(), the speed range = [0, maxspeed], closed interval */
    unsigned            preview;    /* number of preview resolution, same as Nncam_get_ResolutionNumber() */
    unsigned            still;      /* number of still resolution, same as Nncam_get_StillResolutionNumber() */
    NncamResolution   res[NNCAM_MAX];
}NncamModel; /* camera model */

/*
    obsolete, please use NncamInstV2
*/
typedef struct {
#ifdef _WIN32
    wchar_t             displayname[64];    /* display name */
    wchar_t             id[64];             /* unique and opaque id of a connected camera, for Nncam_Open */
#else
    char                displayname[64];    /* display name */
    char                id[64];             /* unique and opaque id of a connected camera, for Nncam_Open */
#endif
    const NncamModel* model;
}NncamInst; /* camera instance for enumerating */

/*
    obsolete, please use Nncam_EnumV2
*/
nncam_deprecated
nncam_ports(unsigned) Nncam_Enum(NncamInst pti[NNCAM_MAX]);

#ifndef _WIN32

/*
This function is only available on macOS and Linux, it's unnecessary on Windows.
  (1) To process the device plug in / pull out in Windows, please refer to the MSDN
       (a) Device Management, http://msdn.microsoft.com/en-us/library/windows/desktop/aa363224(v=vs.85).aspx
       (b) Detecting Media Insertion or Removal, http://msdn.microsoft.com/en-us/library/windows/desktop/aa363215(v=vs.85).aspx
  (2) To process the device plug in / pull out in Linux / macOS, please call this function to register the callback function.
      When the device is inserted or pulled out, you will be notified by the callback funcion, and then call Nncam_EnumV2(...) again to enum the cameras.
Recommendation: for better rubustness, when notify of device insertion arrives, don't open handle of this device immediately, but open it after delaying a short time (e.g., 200 milliseconds).
*/
typedef void (*PNNCAM_HOTPLUG)(void* pCallbackCtx);
nncam_ports(void)   Nncam_HotPlug(PNNCAM_HOTPLUG pHotPlugCallback, void* pCallbackCtx);

#else

/* Nncam_Start is obsolete, it's a synonyms for Nncam_StartPushMode. */
nncam_deprecated
nncam_ports(HRESULT)  Nncam_Start(HNnCam h, PNNCAM_DATA_CALLBACK pDataCallback, void* pCallbackCtx);

/* Nncam_put_TempTintInit is obsolete, it's a synonyms for Nncam_AwbOnePush. */
nncam_deprecated
nncam_ports(HRESULT)  Nncam_put_TempTintInit(HNnCam h, PINNCAM_TEMPTINT_CALLBACK fnTTProc, void* pTTCtx);

/*
    obsolete, please use Nncam_put_Option or Nncam_get_Option to set or get the process mode: NNCAM_PROCESSMODE_FULL or NNCAM_PROCESSMODE_FAST.
    default is NNCAM_PROCESSMODE_FULL.
*/
#ifndef __NNCAM_PROCESSMODE_DEFINED__
#define __NNCAM_PROCESSMODE_DEFINED__
#define NNCAM_PROCESSMODE_FULL        0x00    /* better image quality, more cpu usage. this is the default value */
#define NNCAM_PROCESSMODE_FAST        0x01    /* lower image quality, less cpu usage */
#endif

nncam_deprecated
nncam_ports(HRESULT)  Nncam_put_ProcessMode(HNnCam h, unsigned nProcessMode);
nncam_deprecated
nncam_ports(HRESULT)  Nncam_get_ProcessMode(HNnCam h, unsigned* pnProcessMode);

#endif

/* obsolete, please use Nncam_put_Roi and Nncam_get_Roi */
nncam_deprecated
nncam_ports(HRESULT)  Nncam_put_RoiMode(HNnCam h, int bRoiMode, int xOffset, int yOffset);
nncam_deprecated
nncam_ports(HRESULT)  Nncam_get_RoiMode(HNnCam h, int* pbRoiMode, int* pxOffset, int* pyOffset);

/* obsolete:
     ------------------------------------------------------------|
     | Parameter         |   Range       |   Default             |
     |-----------------------------------------------------------|
     | VidgetAmount      |   -100~100    |   0                   |
     | VignetMidPoint    |   0~100       |   50                  |
     -------------------------------------------------------------
*/
nncam_ports(HRESULT)  Nncam_put_VignetEnable(HNnCam h, int bEnable);
nncam_ports(HRESULT)  Nncam_get_VignetEnable(HNnCam h, int* bEnable);
nncam_ports(HRESULT)  Nncam_put_VignetAmountInt(HNnCam h, int nAmount);
nncam_ports(HRESULT)  Nncam_get_VignetAmountInt(HNnCam h, int* nAmount);
nncam_ports(HRESULT)  Nncam_put_VignetMidPointInt(HNnCam h, int nMidPoint);
nncam_ports(HRESULT)  Nncam_get_VignetMidPointInt(HNnCam h, int* nMidPoint);

/* obsolete flags */
#define NNCAM_FLAG_BITDEPTH10    NNCAM_FLAG_RAW10  /* pixel format, RAW 10bits */
#define NNCAM_FLAG_BITDEPTH12    NNCAM_FLAG_RAW12  /* pixel format, RAW 12bits */
#define NNCAM_FLAG_BITDEPTH14    NNCAM_FLAG_RAW14  /* pixel format, RAW 14bits */
#define NNCAM_FLAG_BITDEPTH16    NNCAM_FLAG_RAW16  /* pixel format, RAW 16bits */

#ifdef _WIN32
#pragma pack(pop)
#endif

#ifdef __cplusplus
}
#endif

#endif
