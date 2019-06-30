
#include "config.h"






#ifndef __QHYCCDSTRUCTDEF_H__
#define __QHYCCDSTRUCTDEF_H__

#if defined (_WIN32)
#ifndef EXPORTFUNC
#define EXPORTFUNC extern "C" __declspec(dllexport)
#endif
#ifndef STDCALL
#define STDCALL __stdcall
#endif
#ifndef EXPORTC
#define EXPORTC extern "C"
#endif
#else
#define EXPORTFUNC extern "C"
#define STDCALL
#define EXPORTC extern "C"
#endif

#include "stdint.h"


#if defined (_WIN32)

#ifdef _M_IX86
typedef uint32_t QHYDWORD;
#else
typedef uint64_t QHYDWORD;
#endif

#else

#ifdef __i386__
typedef uint32_t QHYDWORD;
#else
typedef uint64_t QHYDWORD;
#endif

#endif


/**
 * usb vendor request command
 */
#define QHYCCD_REQUEST_READ  0xC0

/**
 * usb vendor request command
 */
#define QHYCCD_REQUEST_WRITE 0x40

#define MACHANICALSHUTTER_OPEN  0
#define MACHANICALSHUTTER_CLOSE 1
#define MACHANICALSHUTTER_FREE  2

/**
 * @brief CCDREG struct define
 *
 * List the ccd registers param
 */
typedef struct ccdreg
{
  uint8_t Gain;                //!< ccd gain
  uint8_t Offset;              //!< ccd offset
  uint32_t Exptime;             //!< expose time
  uint8_t HBIN;                //!< width bin
  uint8_t VBIN;                //!< height bin
  uint16_t LineSize;           //!< almost match image width
  uint16_t VerticalSize;       //!< almost match image height
  uint16_t SKIP_TOP;           //!< Reserved
  uint16_t SKIP_BOTTOM;        //!< Reserved
  uint16_t LiveVideo_BeginLine;//!< Reserved
  uint16_t AnitInterlace;      //!< Reserved
  uint8_t MultiFieldBIN;       //!< Reserved
  uint8_t AMPVOLTAGE;          //!< Reserved
  uint8_t DownloadSpeed;       //!< transfer speed
  uint8_t TgateMode;           //!< Reserved
  uint8_t ShortExposure;       //!< Reserved
  uint8_t VSUB;                //!< Reserved
  uint8_t CLAMP;               //!< Reserved
  uint8_t TransferBIT;         //!< Reserved
  uint8_t TopSkipNull;         //!< Reserved
  uint16_t TopSkipPix;         //!< Reserved
  uint8_t MechanicalShutterMode;//!< Reserved
  uint8_t DownloadCloseTEC;    //!< Reserved
  uint8_t SDRAM_MAXSIZE;       //!< Reserved
  uint16_t ClockADJ;           //!< Reserved
  uint8_t Trig;                //!< Reserved
  uint8_t MotorHeating;        //!< Reserved
  uint8_t WindowHeater;        //!< Reserved
  uint8_t ADCSEL;              //!< Reserved
}
CCDREG;

struct BIOREG
{
  uint16_t LineSize;
  uint16_t PatchNumber;
  uint8_t  AMPVOLTAGE;
  uint8_t  ShortExposure;
  uint8_t  SDRAM_MAXSIZE;
  uint8_t  DownloadSpeed;
  uint8_t  TransferBIT;
  uint8_t  BIOCCD_Mode;
  uint8_t  BIOCCD_Video;
  uint8_t  SDRAM_Bypass;
};




#if 0
//lowlevelstatus is used for QHYCCD LowLevelProtocol to readout the camera status from 0XD2 command.
//see QHYCCD Low Level Protocol White Paper.
typedef struct lowlevelstatus
{
  uint8_t speed;               //!< ccd gain
  uint32_t restExpTime;              //!< ccd offset
  uint32_t ExpTime;             //!< expose time
  uint8_t FwVersionYear;                //!< width bin
  uint8_t FwVersionMonth;                //!< height bin
  uint8_t FwVersionDay;           //!< almost match image width
  uint8_t TempType;       //!< almost match image height
  uint16_t CurrentTempADU;           //!< Reserved
  uint16_t TargetTempADU;        //!< Reserved
  uint8_t CurrentPWM;//!< Reserved
  uint8_t TempControlMode;      //!< Reserved
  uint32_t DataInDDR;       //!< Reserved
  double CurrentTempC;          //!< Reserved
  double TargetTempC;       //!< transfer speed
  uint16_t ImageX;           //!< Reserved
  uint16_t ImageY;       //!< Reserved
  uint8_t ImageBitDepth;                //!< Reserved
  uint8_t USBSpeed;               //!< Reserved
  uint8_t cfwbuffer[8];         //!< Reserved
  uint8_t cameraSubModel;         //!< Reserved
  uint8_t cameraColorType;         //!< Reserved
  uint8_t cameraSeriesNumber[16];//!< Reserved
}
LowLevelStatus;
#endif

/**
 * @brief CONTROL_ID enum define
 *
 * List of the function could be control
 */
enum CONTROL_ID
{
  CONTROL_BRIGHTNESS = 0, //!< image brightness
  CONTROL_CONTRAST,       //!< image contrast
  CONTROL_WBR,            //!< red of white balance
  CONTROL_WBB,            //!< blue of white balance
  CONTROL_WBG,            //!< the green of white balance
  CONTROL_GAMMA,          //!< screen gamma
  CONTROL_GAIN,           //!< camera gain
  CONTROL_OFFSET,         //!< camera offset
  CONTROL_EXPOSURE,       //!< expose time (us)
  CONTROL_SPEED,          //!< transfer speed
  CONTROL_TRANSFERBIT,    //!< image depth bits
  CONTROL_CHANNELS,       //!< image channels
  CONTROL_USBTRAFFIC,     //!< hblank
  CONTROL_ROWNOISERE,     //!< row denoise
  CONTROL_CURTEMP,        //!< current cmos or ccd temprature
  CONTROL_CURPWM,         //!< current cool pwm
  CONTROL_MANULPWM,       //!< set the cool pwm
  CONTROL_CFWPORT,        //!< control camera color filter wheel port
  CONTROL_COOLER,         //!< check if camera has cooler
  CONTROL_ST4PORT,        //!< check if camera has st4port
  CAM_COLOR,
  CAM_BIN1X1MODE,         //!< check if camera has bin1x1 mode
  CAM_BIN2X2MODE,         //!< check if camera has bin2x2 mode
  CAM_BIN3X3MODE,         //!< check if camera has bin3x3 mode
  CAM_BIN4X4MODE,         //!< check if camera has bin4x4 mode
  CAM_MECHANICALSHUTTER,                   //!< mechanical shutter
  CAM_TRIGER_INTERFACE,                    //!< triger
  CAM_TECOVERPROTECT_INTERFACE,            //!< tec overprotect
  CAM_SINGNALCLAMP_INTERFACE,              //!< singnal clamp
  CAM_FINETONE_INTERFACE,                  //!< fine tone
  CAM_SHUTTERMOTORHEATING_INTERFACE,       //!< shutter motor heating
  CAM_CALIBRATEFPN_INTERFACE,              //!< calibrated frame
  CAM_CHIPTEMPERATURESENSOR_INTERFACE,     //!< chip temperaure sensor
  CAM_USBREADOUTSLOWEST_INTERFACE,         //!< usb readout slowest

  CAM_8BITS,                               //!< 8bit depth
  CAM_16BITS,                              //!< 16bit depth
  CAM_GPS,                                 //!< check if camera has gps

  CAM_IGNOREOVERSCAN_INTERFACE,            //!< ignore overscan area

  QHYCCD_3A_AUTOBALANCE,
  QHYCCD_3A_AUTOEXPOSURE,
  QHYCCD_3A_AUTOFOCUS,
  CONTROL_AMPV,                            //!< ccd or cmos ampv
  CONTROL_VCAM,                            //!< Virtual Camera on off
  CAM_VIEW_MODE,

  CONTROL_CFWSLOTSNUM,         //!< check CFW slots number
  IS_EXPOSING_DONE,
  ScreenStretchB,
  ScreenStretchW,
  CONTROL_DDR,
  CAM_LIGHT_PERFORMANCE_MODE,

  CAM_QHY5II_GUIDE_MODE,
  DDR_BUFFER_CAPACITY,
  DDR_BUFFER_READ_THRESHOLD,
  DefaultGain,
  DefaultOffset,
  OutputDataActualBits,
  OutputDataAlignment,

  CAM_SINGLEFRAMEMODE,
  CAM_LIVEVIDEOMODE,
  CAM_IS_COLOR,
  hasHardwareFrameCounter,
  CONTROL_MAX_ID
};

/**
 * debayer mode for mono to color */
enum BAYER_ID
{
  BAYER_GB = 1,
  BAYER_GR,
  BAYER_BG,
  BAYER_RG
};

enum CodecID
{
  NONE_CODEC,
  H261_CODEC
};

typedef struct _QHYCamMinMaxStepValue
{
  const char *name;
  double min;
  double max;
  double step;
}
QHYCamMinMaxStepValue;

typedef struct _QHYGetImageParam
{
  void *handle;
  uint8_t *imgdata;
  uint32_t w;
  uint32_t h;
  uint32_t bpp;
  uint32_t channels;
}
QHYGetImageParam;


#if CALLBACK_MODE_SUPPORT
typedef uint32_t  (*QHYCCDProcCallBack) (void *handle,
    QHYDWORD message,
    QHYDWORD wParam,
    QHYDWORD lParam);
#endif


#endif
