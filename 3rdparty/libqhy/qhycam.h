
#include <math.h>
#include "qhyccdstruct.h"
#include "config.h"
#include "qhyccdcamdef.h"
#if defined (_WIN32)
#include "CyAPI.h"
#include <process.h>
#include <windows.h>
#include <mmsystem.h>
#else
#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <libusb-1.0/libusb.h>

#endif

#include "stdint.h"

#ifndef __QHYCAMDEF_H__
#define __QHYCAMDEF_H__


#define IDLE 0
#define EXPOSING 1
#define DOWNLOADING 2

#define QHYCCD_IMAGEMODE_NONE    		0x00
#define QHYCCD_IMAGEMODE_SINGLE   	0x01
#define QHYCCD_IMAGEMODE_LIVE	  	0x02

#define QHYCCD_USBTYPE_NONE    	0x00
#define QHYCCD_USBTYPE_CYUSB   	0x01
#define QHYCCD_USBTYPE_WINUSB  	0x02
#define QHYCCD_USBTYPE_LIBUSB  	0x03

#define USB_ENDPOINT  				0x81

/**
 * typedef the libusb_deivce qhyccd_device
 */
#if (defined(__linux__ )&&!defined (__ANDROID__)) ||(defined (__APPLE__)&&defined( __MACH__)) ||(defined(__linux__ )&&defined (__ANDROID__))
typedef struct libusb_device qhyccd_device;
#endif
#if defined (_WIN32)
typedef void* qhyccd_device;
#endif

/**
 * typedef the libusb_deivce_handle qhyccd_handle
 */
#if (defined(__linux__ )&&!defined (__ANDROID__)) ||(defined (__APPLE__)&&defined( __MACH__)) ||(defined(__linux__ )&&defined (__ANDROID__))
typedef struct libusb_device_handle qhyccd_handle;
#endif
#if defined (_WIN32)
typedef CCyUSBDevice qhyccd_handle;
#endif

#if 1
//QinXiaoXu  START  20181019
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
//QinXiaoXu  END 20181019
#endif

/**
 * @brief QHYCAM class define
 *
 * include all functions for qhycam
 */
class QHYCAM
{
public:

  QHYCAM()
  {
    camstatus = IDLE;
    ep1num = 0x04;
    usbep = 0x82;
    usbintwep = 0x01;
    usbintrep = 0x81;
    intepflag = 0;
    usbtype = QHYCCD_USBTYPE_CYUSB;
//    CameraType = DEVICETYPE_UNKNOW;
	

#if defined (_WIN32)

    InitializeCriticalSection(&csep);
#else

    pthread_mutex_init(&mutex, NULL);
#endif

  }

  virtual ~QHYCAM()
  {
#if defined (_WIN32)
    DeleteCriticalSection(&csep);
#else

    pthread_mutex_destroy(&mutex);
#endif

  }
  
static int32_t QGetTimerMS()
  {
#if defined (_WIN32)
    return timeGetTime() ;
#else
    struct timeval tv;
    gettimeofday(&tv,NULL);
    return tv.tv_sec * 1000 + tv.tv_usec / 1000;
#endif
  }


static void QSleep(uint32_t mstime)
  {
#if defined (_WIN32)
    Sleep(mstime);
#else
    usleep(mstime * 1000);
#endif
  }

static void QBeep(uint32_t volume,uint32_t mstime)
{
#if defined (_WIN32)
  Beep(volume,mstime);
#else
#if 0

  int   fd   =   open("/dev/tty10",   O_RDONLY);
  if   (fd   ==   -1   ||   argc   !=   3)
  {
    return   -1;
  }
  ioctl(fd,   KDMKTONE,   20000);
  close(fd);
#endif
#endif
}


  /**
   @fn uint32_t openCamera(qhyccd_deivce *d,qhyccd_handle **h)
   @brief open the camera,open the device handle
   @param d deivce deivce
   @param h device control handle
   @return
   success return QHYCCD_SUCCESS \n
   another QHYCCD_ERROR code on other failures
   */
  uint32_t openCamera(qhyccd_device *d, qhyccd_handle **h);

  /**
   @fn void closeCamera(qhyccd_device *h)
   @brief close the camera,close the deivce handle
   @param h deivce control handle
   */
  void closeCamera(qhyccd_handle *h);

  /**
   @fn uint32_t sendForceStop(qhyccd_handle *h)
   @brief force stop exposure
   @param h device control handle
       @return
       success return QHYCCD_SUCCESS \n
   another QHYCCD_ERROR code on other failures
   */
  uint32_t sendForceStop(qhyccd_handle *h);

  /**
   @fn uint32_t sendInterrupt(qhyccd_handle *handle,uint8_t length,uint8_t *data)
   @brief send a package to deivce using the interrupt endpoint
   @param handle device control handle
   @param length the package size(unit:byte)
   @param data package buffer
   @return
   success return QHYCCD_SUCCESS \n
   another QHYCCD_ERROR code on other failures
   */
  uint32_t sendInterrupt(qhyccd_handle *handle, uint8_t length, uint8_t *data);

  /**
   @fn uint32_t vendTXD(qhyccd_handle *dev_handle, uint8_t req, uint8_t *data, uint16_t length)
   @brief send a package to deivce using the vendor request
   @param dev_handle device control handle
   @param req request command
   @param data package buffer
   @param length the package size(unit:byte)
   @return
   success return QHYCCD_SUCCESS \n
   another QHYCCD_ERROR code on other failures
   */
  uint32_t vendTXD(qhyccd_handle *dev_handle, uint8_t req, uint8_t *data, uint16_t length);

  /**
   @fn uint32_t vendRXD(qhyccd_handle *dev_handle, uint8_t req, uint8_t *data, uint16_t length)
   @brief get a package from deivce using the vendor request
   @param dev_handle device control handle
   @param req request command
   @param data package buffer
   @param length the package size(unit:byte)
   @return
   success return QHYCCD_SUCCESS \n
   another QHYCCD_ERROR code on other failures
   */
  uint32_t vendRXD(qhyccd_handle *dev_handle, uint8_t req, uint8_t *data, uint16_t length);

  /**
   @fn iTXD(qhyccd_handle *dev_handle,uint8_t *data, int32_t length)
   @brief send a package to deivce using the bulk endpoint
   @param dev_handle device control handle
   @param data package buffer
   @param length the package size(unit:byte)
   @return
   success return QHYCCD_SUCCESS \n
   another QHYCCD_ERROR code on other failures
   */
  uint32_t iTXD(qhyccd_handle *dev_handle, uint8_t *data, int32_t length);

  /**
   @fn iTXD_Ex(qhyccd_handle *dev_handle,uint8_t *data, int32_t length,uint8_t ep)
   @brief send a package to deivce using the bulk endpoint
   @param dev_handle device control handle
   @param data package buffer
   @param length the package size(unit:byte)
       @param ep the endpoint number
   @return
   success return QHYCCD_SUCCESS \n
   another QHYCCD_ERROR code on other failures
   */
  uint32_t iTXD_Ex(qhyccd_handle *dev_handle, uint8_t *data, int32_t length, uint8_t ep);

  /**
   @fn iRXD(qhyccd_handle *dev_handle,uint8_t *data, int32_t length)
   @brief get a package from deivce using the bulk endpoint
   @param dev_handle device control handle
   @param data package buffer
   @param length the package size(unit:byte)
   @return
   success return QHYCCD_SUCCESS \n
   another QHYCCD_ERROR code on other failures
   */
  uint32_t iRXD(qhyccd_handle *dev_handle, uint8_t *data, int32_t length);

  /**
   @fn iRXD_Ex(qhyccd_handle *dev_handle,uint8_t *data, int32_t length,uint8_t ep)
   @brief get a package from deivce using the bulk endpoint
   @param dev_handle device control handle
   @param data package buffer
   @param length the package size(unit:byte)
       @param ep the endpoint number
   @return
   success return QHYCCD_SUCCESS \n
   another QHYCCD_ERROR code on other failures
   */
  uint32_t iRXD_Ex(qhyccd_handle *dev_handle, uint8_t *data, uint32_t length, uint8_t ep);

  /**
   @fn uint32_t vendTXD_Ex(qhyccd_handle *dev_handle, uint8_t req,uint16_t value,uint16_t index,uint8_t* data, uint16_t length)
   @brief send a package to deivce using the vendor request,extend the index and value interface
   @param dev_handle device control handle
   @param req request command
   @param value value interface
   @param index index interface
   @param data package buffer
   @param length the package size(unit:byte)
   @return
   success return QHYCCD_SUCCESS \n
   another QHYCCD_ERROR code on other failures
   */


  // uint32_t ReadUSB_SYNC(qhyccd_handle *pDevHandle, uint8_t endpoint, uint32_t length, uint8_t *data, uint32_t timeout);
  uint32_t vendTXD_Ex(qhyccd_handle *dev_handle, uint8_t req, uint16_t value, uint16_t index, uint8_t *data, uint16_t length);

  /**
   @fn uint32_t vendRXD_Ex(qhyccd_handle *dev_handle, uint8_t req,uint16_t value,uint16_t index,uint8_t* data, uint16_t length)
   @brief get a package from deivce using the vendor request,extend the index and value interface
   @param dev_handle device control handle
   @param req request command
   @param value value interface
   @param index index interface
   @param data package buffer
   @param length the package size(unit:byte)
   @return
   success return QHYCCD_SUCCESS \n
   another QHYCCD_ERROR code on other failures
   */
  uint32_t vendRXD_Ex(qhyccd_handle *dev_handle, uint8_t req, uint16_t value, uint16_t index, uint8_t* data, uint16_t length);

  uint32_t vendErroeRecovery(qhyccd_handle *dev_handle);


  uint32_t QHY5IIIreadUSB2B(qhyccd_handle *dev_handle, uint8_t *data, uint32_t p_num, uint32_t timeout);
  /**
   @fn uint32_t readUSB2B(qhyccd_handle *dev_handle, uint8_t *data,uint32_t p_size, uint32_t p_num, uint32_t* pos)
   @brief get image packages from deivce using bulk endpoint
   @param dev_handle device control handle
   @param data package buffer
   @param p_size package size
   @param p_num package num
   @param pos reserved
       @param duration the exposure time
   @return
   success return QHYCCD_SUCCESS \n
   another QHYCCD_ERROR code on other failures
   */
  uint32_t readUSB2B(qhyccd_handle *dev_handle, uint8_t *data, uint32_t p_size, uint32_t p_num, uint32_t* pos, uint32_t timeout);

  /**
   @fn uint32_t beginVideo(qhyccd_handle *handle)
   @brief send begin exposure signal to deivce
   @param handle device control handle
   @return
   success return QHYCCD_SUCCESS \n
   another QHYCCD_ERROR code on other failures
   */
  uint32_t beginVideo(qhyccd_handle *handle);

  /**
   @fn uint32_t sendRegisterQHYCCDOld(qhyccd_handle *handle,
   CCDREG reg, uint32_t P_Size, uint32_t *Total_P, uint32_t *PatchNumber)
   @brief send register params to deivce and some other info
   @param handle deivce control handle
   @param reg register struct
   @param P_Size the package size
   @param Total_P the total packages
   @param PatchNumber the patch for usb transfer,the total size must multiple 512 bytes.
   @return
   success return QHYCCD_SUCCESS \n
   another QHYCCD_ERROR code on other failures
   */
  uint32_t sendRegisterQHYCCDOld(qhyccd_handle *handle,
                                 CCDREG reg, uint32_t P_Size, uint32_t *Total_P, uint32_t *PatchNumber);

  /**
   @fn uint32_t sendRegisterQHYCCDNew(qhyccd_handle *handle,
   CCDREG reg, uint32_t P_Size, uint32_t *Total_P, uint32_t *PatchNumber)
   @brief New interface!!! send register params to deivce and some other info
   @param handle deivce control handle
   @param reg register struct
   @param P_Size the package size
   @param Total_P the total packages
   @param PatchNumber the patch for usb transfer,the total size must multiple 512 bytes.
   @return
   success return QHYCCD_SUCCESS \n
   another QHYCCD_ERROR code on other failures
   */
  uint32_t sendRegisterQHYCCDNew(qhyccd_handle *handle,
                                 CCDREG reg, uint32_t P_Size, uint32_t *Total_P, uint32_t *PatchNumber);

  /**
  @fn uint32_t sendRegisterBioCCD(qhyccd_handle *handle,BIOREG reg)
  @brief api like sendRegisterQHYCCDOld,just a different version for some kinds of cameras
  @param handle deivce control handle
  @param reg register struct
  @return
  success return QHYCCD_SUCCESS \n
  another QHYCCD_ERROR code on other failures
   */
  uint32_t sendRegisterBioCCD(qhyccd_handle *handle, BIOREG reg);

  /**
  @fn uint32_t setDisableGuider_INT(qhyccd_handle *handle)
  @brief turn off the ST4 port(guide port)
  @param handle deivce control handle
  @return
  success return QHYCCD_SUCCESS \n
  another QHYCCD_ERROR code on other failures
   */
  uint32_t setDisableGuider_INT(qhyccd_handle *handle);

  /**
  @fn uint32_t setBioCCDDigitalGain_INT(qhyccd_handle *handle,uint8_t gain)
  @brief set bioccd digitalgain
  @param handle deivce control handle
  @param gain gain value
  @return
  success return QHYCCD_SUCCESS \n
  another QHYCCD_ERROR code on other failures
   */
  uint32_t setBioCCDDigitalGain_INT(qhyccd_handle *handle, uint8_t gain);

  /**
  @fn uint32_t setBioCCDGain_INT(qhyccd_handle *handle,uint8_t gain)
  @brief set bioccd analog gain
  @param handle deivce control handle
  @param gain gain value
  @return
  success return QHYCCD_SUCCESS \n
  another QHYCCD_ERROR code on other failures
   */
  uint32_t setBioCCDGain_INT(qhyccd_handle *handle, uint16_t gain);

  /**
  @fn uint32_t getExpSetting(double T, uint32_t *ExpTime, int32_t *LiveExpTime, double A,double B, double V, double LinePeriod)
  @brief calc the exposure time
  @param T
  @param ExpTime
  @param LiveExpTime
  @param A
  @param B
  @param V
  @param LinePeriod
  @return
  success return QHYCCD_SUCCESS \n
  another QHYCCD_ERROR code on other failures
   */
  uint32_t getExpSetting(double T, uint32_t *ExpTime, int32_t *LiveExpTime, double A, double B, double V, double LinePeriod);

  /**
  @fn uint32_t setBioCCDExp_INT(qhyccd_handle *handle,uint32_t ExpTime)
  @brief set bioccd exposure time
  @param handle deivce control handle
  @param ExpTime exposure time value
  @return
  success return QHYCCD_SUCCESS \n
  another QHYCCD_ERROR code on other failures
   */
  uint32_t setBioCCDExp_INT(qhyccd_handle *handle, uint32_t ExpTime);

  /**
  @fn uint32_t setBioCCDLiveExp_INT(qhyccd_handle *handle,uint16_t VideoExpTime)
  @brief set bioccd live exposure time
  @param handle deivce control handle
  @param VideoExpTime live exposure time value
  @return
  success return QHYCCD_SUCCESS \n
  another QHYCCD_ERROR code on other failures
   */
  uint32_t setBioCCDLiveExp_INT(qhyccd_handle *handle, uint16_t VideoExpTime);

  /**
   @fn uint32_t setDC201FromInterrupt(qhyccd_handle *handle,uint8_t PWM,uint8_t FAN)
   @brief control the DC201 using the interrupt endpoint
   @param handle deivce control handle
   @param PWM cool power
   @param FAN switch for camera's fan (NOT SUPPORT NOW)
   @return
   success return QHYCCD_SUCCESS \n
   another QHYCCD_ERROR code on other failures
   */
  uint32_t setDC201FromInterrupt(qhyccd_handle *handle, uint8_t PWM, uint8_t FAN);

  /**
   @fn signed short getDC201FromInterrupt(qhyccd_handle *handle)
   @brief get the temprature value from DC201 using the interrupt endpoint
   @param handle deivce control handle
   @return
   success return  temprature value \n
   another QHYCCD_ERROR code on other failures
   */
  int16_t getDC201FromInterrupt(qhyccd_handle *handle);

  /**
   @fn uint8_t getFromInterrupt(qhyccd_handle *handle,uint8_t length,uint8_t *data)
   @brief get some special information from deivce,using interrupt endpoint
   @param handle deivce control handle
   @param length package size
   @param data package buffer
   @return
   success return QHYCCD_SUCCESS \n
   another QHYCCD_ERROR code on other failures
   */
  uint8_t getFromInterrupt(qhyccd_handle *handle, uint8_t length, uint8_t *data);

  /**
   @fn double GetCCDTemp(qhyccd_handle *handle)
   @brief get ccd/cmos chip temperature
   @param handle deivce control handle
   @return
   success return temperature \n
   */
  double GetCCDTemp(qhyccd_handle *handle);

  /**
   @fn double RToDegree(double R)
   @brief calc,transfer R to degree
   @param R R
   @return
   success return degree
   */
  double RToDegree(double R);

  /**
   @fn double mVToDegree(double V)
   @brief calc,transfer mV to degree
   @param V mv
   @return
   success return degree
   */
  double mVToDegree(double V);

  /**
   @fn double DegreeTomV(double degree)
   @brief calc,transfer degree to mv
   @param degree degree
   @return
   success return mv
   */
  double DegreeTomV(double degree);

  /**
   @fn double DegreeToR(double degree)
   @brief calc,transfer degree to R
   @param degree degree
   @return
   success return R
   */
  double DegreeToR(double degree);

  /**
   @fn uint32_t I2CTwoWrite(qhyccd_handle *handle,uint16_t addr,uint16_t value)
   @brief set param by I2C
   @param handle device control handle
   @param addr inter reg addr
   @param value param value
   @return
   success return QHYCCD_SUCCESS \n
   another QHYCCD_ERROR code on other failures
   */
  uint32_t I2CTwoWrite(qhyccd_handle *handle, uint16_t addr, uint16_t value);

  /**
   @fn uint16_t I2CTwoRead(qhyccd_handle *handle,uint16_t addr)
   @brief get param by I2C
   @param handle device control handle
   @param addr inter reg addr
   @return
   success return param value \n
   another QHYCCD_ERROR code on other failures
   */
  uint16_t I2CTwoRead(qhyccd_handle *handle, uint16_t addr);

  /**
   @fn uint8_t MSB(uint16_t i)
   @brief apart the 16 bits value to 8bits(high 8bits)
   @param i 16bits value
   @return
   success return 8bits value(high 8bits) \n
   another QHYCCD_ERROR code on other failures
   */
  uint8_t MSB(uint16_t i);

  /**
   @fn uint8_t LSB(uint16_t i)
   @brief apart the 16 bits value to 8bits(low 8bits)
   @param i 16bits value
   @return
   success return 8bits value(low 8bits) \n
   another QHYCCD_ERROR code on other failures
   */
  uint8_t LSB(uint16_t i);
  uint8_t MSB3(uint32_t i);
  uint8_t MSB2(uint32_t i);
  uint8_t MSB1(uint32_t i);
  uint8_t MSB0(uint32_t i);

  void SWIFT_MSBLSB12BITS(uint8_t * Data, uint32_t x, uint32_t y);
  void SWIFT_MSBLSB16BITS(uint8_t * Data, uint32_t x, uint32_t y);
  void SWIFT_MSBLSB14BITS(uint8_t * Data, uint32_t x, uint32_t y);


  void QHY5II_SWIFT_MSBLSB12BITS(uint8_t * Data, uint32_t x, uint32_t y);
  void QHY5II_SWIFT_MSBLSB14BITS(uint8_t * Data, uint32_t x, uint32_t y);
  void QHY5II_SWIFT_8BitsTo16Bits(uint8_t * Dst, uint8_t * Src, uint32_t x, uint32_t y);

  void QHY5II_DeNoise(uint8_t *data, uint32_t x, uint32_t y, double curgain);



  static void *pollHandleEvents(void *arg);
  static void findCompleteFrame(uint8_t *rawarray, uint32_t length);
  static void asyImageDataCallBack(struct libusb_transfer *transfer);


  //QinXiaoXu  START  20181019
  uint32_t LowLevelA0(qhyccd_handle *h,uint8_t mode, uint16_t xbin, uint16_t ybin );
  uint32_t LowLevelA1( qhyccd_handle *h,uint8_t speed );
  uint32_t LowLevelA2( qhyccd_handle *h,uint8_t resmode, uint16_t roixsize, uint16_t  roixstart, uint16_t roiysize,uint16_t roiystart );
  uint32_t LowLevelA3( qhyccd_handle *h,uint32_t exptime );
  uint32_t LowLevelA4( qhyccd_handle *h,uint16_t againR,uint16_t dgainR, uint16_t againG,uint16_t dgainG,uint16_t againB,uint16_t dgainB );
  uint32_t LowLevelA5(qhyccd_handle *h,uint8_t usbtraffic);
  uint32_t LowLevelA6( qhyccd_handle *h,uint8_t command );
  uint32_t LowLevelA7( qhyccd_handle *h,uint8_t data);
  uint32_t LowLevelA8( qhyccd_handle *h,uint16_t offset1R,uint16_t offset1G,uint16_t offset1B,uint16_t offset2R,uint16_t offset2G,uint16_t offset2B );
  uint32_t LowLevelA9(qhyccd_handle *h, uint8_t command,uint32_t value);
  uint32_t LowLevelAD(qhyccd_handle *h);
  uint32_t LowLevelGetStatus(qhyccd_handle *h,LowLevelStatus s);
  uint32_t LowLevelGetDebugData(qhyccd_handle *h,LowLevelStatus s);
  //QinXiaoXu  END  20181019

  uint32_t camstatus; //!< the camera current status

  CCDREG ccdreg; //!< ccd registers params
  BIOREG imgreg; //!< bioccd registers params

  uint8_t usbep; //!< usb transfer endpoint

  uint32_t ep1num; //!< ep1in transfer data length

  uint8_t usbintwep; //!< usb interrupt write endpoint

  uint32_t usbintrep; //!< usb interrupt read endpoint

  uint32_t psize; //!< usb transfer package size at onece

  uint32_t totalp; //!< the total usb transfer packages

  uint32_t patchnumber; //!< patch for image transfer packages

  uint32_t readp; //!< the number of alreay read usb transfer packages

  uint8_t intepflag;

  uint8_t usbtype;

  uint8_t vrreadstatus;

#if defined (_WIN32)

  CRITICAL_SECTION csep;
#else

  pthread_mutex_t mutex;
#endif

};

#endif
