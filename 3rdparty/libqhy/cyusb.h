#ifndef __CYUSB_H
#define __CYUSB_H
#include "config.h"



#if defined (_WIN32)
#include "Context.h"
#include "QUsb.h"
#define QUSB_SINGLEFRAMEUSBPACKETSIZE 2048 * 20 *2
#endif


#if (defined(__linux__ )&&!defined (__ANDROID__)) ||(defined (__APPLE__)&&defined( __MACH__)) ||(defined(__linux__ )&&defined (__ANDROID__))
#include <libusb-1.0/libusb.h>
#include "qhyccd.h"
#define QUSB_SINGLEFRAMEUSBPACKETSIZE 16384
#endif

#include "unlockimagequeue.h"
#include "qhybase.h"


/* This is the maximum number of VID/PID pairs that this library will consider. This limits
   the number of valid VID/PID entries in the configuration file.
 */
#define MAX_ID_PAIRS    100

/*
 This is the maximum number of qhyccd cams.
 */
#define MAXDEVICES (18)

#define MAX_DEVICES_ID (100)


/*
 This is the length for camera ID
 */
#define ID_STR_LEN (0x20)


#if (defined(__linux__ )&&!defined (__ANDROID__)) ||(defined (__APPLE__)&&defined( __MACH__)) ||(defined(__linux__ )&&defined (__ANDROID__))
#define OVERLAPS   32
#define TRANSSIZE  (76800)
#else
#define Overlaps 16
#define BufferSize (1280*960*2*2)
#define BufferEntries 3
#define USB_PACKET_SIZE  1
#define N_USB_PACKET_SIZE 524288
struct IVCamSDK;

#endif


struct COUNTEXPTIME
{
  bool *flagquit;
  double *camtime;
};




/**
 * @brief sturct cydef define
 *
 * Global struct for camera's class and some information
 */
struct cydev
{
  qhyccd_device *dev; //!< Camera deivce
#if defined (_WIN32)

  void *handle; //!< Camera control handle
#else

  qhyccd_handle *handle;
#endif

  uint8_t usbtype;
  uint8_t ImageMode; 
  bool IsChecked; 
  int8_t deviceNO; 
  uint16_t vid; //!< Vendor ID
  uint16_t pid; //!< Product ID
  uint8_t is_open; //!< When device is opened, val = 1
  char id[64]; //!< The camera's id
  QHYBASE *qcam; //!< Camera class pointer
#if defined (_WIN32)
  /* mark VirutalCam status */
  bool is_virtualwdm_working;
  CRITICAL_SECTION cs;
  HANDLE hSem;
  HANDLE ThreadHandle;

  uint32_t CAMExposing;
  uint32_t CAMCAPTUREONE;
  uint32_t THREAD;
  int32_t GoodFrames ;
  uint32_t BadFrames ;

  uint32_t BufferStart;
  uint32_t BufferEnd;


  uint8_t *CurrentFrameNext;
  uint32_t CurrentFrameSize;
  uint32_t CurrentFrame ;

  uint8_t *Buffer;
  uint8_t *Imgbuffer;
  uint32_t CAMBYTE;
  uint32_t CAMWIDTH;
  uint32_t FrameSize;

  volatile uint8_t ContinueCapture;
  OVERLAPPED io[Overlaps];
  uint32_t   sz[Overlaps];
  uint8_t    *inContext[99];
  HANDLE     multio[Overlaps + 1];
  uint8_t isinitcontext;
  int  transSize;
  CamContext g_CamContextList;

  double agc_pos ;                                                       //#AECAGC
  double aec_pos ;                                                     //#AECAGC
  IplImage *AECAGC_maskImgA;
  IplImage *AECAGC_maskImgB;
  double qhyccd3a_gain;
  double qhyccd3a_exposure;
  UsbContext g_usbContextList;
  IVCamSDK* m_pVCam;
#endif

#if (defined(__linux__ )&&!defined (__ANDROID__)) ||(defined (__APPLE__)&&defined( __MACH__)) ||(defined(__linux__ )&&defined (__ANDROID__))

  uint32_t CAMExposing;
  int32_t GoodFrames ;
  uint32_t BadFrames ;
  uint32_t BufferStart;
  uint32_t BufferEnd;



  struct libusb_transfer *img_transfer[OVERLAPS];
  uint8_t img_buff[OVERLAPS * TRANSSIZE];
  pthread_t rawhandle;

  volatile bool raw_exit;
  volatile uint8_t evtnumflag;

  uint8_t sig[16];
  uint8_t sigcrc[16];
  int32_t headerLen;
  int32_t frameLen;
  int32_t endingLen;
  int32_t sigLen;
  int32_t headertype ;
  int32_t rawFrameWidth;
  int32_t rawFrameHeight;
  int32_t rawFrameBpp;
  uint8_t *rawDataCache;
#endif

  uint32_t imagequeuelength;
  uint32_t GlobalFrameCounter;
  UnlockImageQueue imagequeue;
  int receivedRawDataLen;
  char chiptemp;
  uint8_t cmossleeprun;
  uint32_t total_length2nd;
  uint32_t ddrstable;
  uint32_t retrynum;
  struct COUNTEXPTIME CountExposureTime;
};



/* Global struct for camera's vendor id */
static uint16_t camvid[MAX_DEVICES_ID] =
  {
    0x1618, 0x1618, 0x1618, 0x1618, 0x1618, 0x1618, 0x1618, 0x1618, 0x1618,
    0x1618, 0x1618, 0x1618, 0x1618, 0X1618, 0x1618, 0x1618, 0x16c0, 0x1618,
    0x16c0, 0x1618, 0x1618, 0x1618, 0x1618, 0x1618, 0x1618, 0x1618, 0x1618,
    0x1618, 0x04b4, 0x1618, 0x1618, 0x1618, 0x1618, 0x1618, 0x1618, 0x1618,
    0x1618, 0x1618, 0x04b4, 0x1618, 0x1618, 0x1618, 0x1618, 0x1618, 0x1618,
    0x1618, 0x1618, 0x1618, 0x1618, 0x1618, 0x1618, 0x1618, 0x04b4, 0x1618,
    0x1618, 0x1618, 0x1618, 0x1618, 0x1618, 0x1618, 0x1618, 0x1618, 0x1618,
    0x1618, 0x1618, 0x1618
  };

/* Global struct for camera'io product id */
static uint16_t campid[MAX_DEVICES_ID] =
  {
    0x0921, 0x8311, 0x6741, 0x6941, 0x6005, 0x1001, 0x1201, 0x8301, 0x6003,
    0x1111, 0x8141, 0x2851, 0x025a, 0x6001, 0x0931, 0x1611, 0x296d, 0x4023,
    0x2971, 0xa618, 0x1501, 0x1651, 0x8321, 0x1621, 0x1671, 0x8303, 0x1631,
    0x2951, 0x00f1, 0x296d, 0x0941, 0x0175, 0x8323, 0x0179, 0x1623, 0x0237,
    0x0186, 0x6953, 0x8614, 0x1601, 0x1633, 0x4201, 0x0225, 0xC175, 0x0291,
    0xC179, 0xC225, 0xC291, 0xC164, 0xC166, 0xC368, 0xC184, 0x8614, 0xF368,
    0xA815, 0x5301, 0x1633, 0xC248, 0xC168, 0xC129, 0x9001 ,0x4041, 0xC295,
    0x2021, 0xC551, 0x4203
  };

/* Global struct for camera'firmware product id */
static uint16_t fpid[MAX_DEVICES_ID] =
  {
    0x0920, 0x8310, 0x6740, 0x6940, 0x6004, 0x1000, 0x1200, 0x8300, 0x6002,
    0x1110, 0x8140, 0x2850, 0x0259, 0x6000, 0x0930, 0x1610, 0x0901, 0x4022,
    0x2970, 0xb618, 0x1500, 0x1650, 0x8320, 0x1620, 0x1670, 0x8302, 0x1630,
    0x2950, 0x00f1, 0x0901, 0x0940, 0x0174, 0x8322, 0x0178, 0x1622, 0x0236,
    0x0185, 0x6952, 0x8613, 0x1600, 0x1632, 0xC400, 0x0224, 0xC174, 0x0290,
    0xC178, 0xC224, 0xC290, 0xC163, 0xC165, 0xC367, 0xC183, 0x8613, 0xF367,
    0xA814, 0x5300, 0x1632, 0xC247, 0xC167, 0xC128, 0x9000, 0x4040, 0xC294,
    0x2020, 0xC550, 0X4202
  };

/****************************************************************************************
  Prototype    : void cyusb_download_fx2(cyusb_handle *h, char *filename,
                     unsigned char vendor_command);
  Description  : Performs firmware download on FX2.
  Parameters   :
                 cyusb_handle *h              : Device handle
                 char * filename              : Path where the firmware file is stored
                 unsigned char vendor_command : Vendor command that needs to be passed during download
  Return Value : 0 on success, or an appropriate LIBUSB_ERROR.
 ****************************************************************************************/
int fx2_ram_download(qhyccd_handle *h, char *filename, unsigned char vendor_command);


/****************************************************************************************
  Prototype    : void cyusb_download_fx3(cyusb_handle *h, char *filename);
  Description  : Performs firmware download on FX3.
  Parameters   :
                 cyusb_handle *h : Device handle
                 char *filename  : Path where the firmware file is stored
  Return Value : 0 on success, or an appropriate LIBUSB_ERROR.
 ***************************************************************************************/
int fx3_usbboot_download(qhyccd_handle *h, char *filename);

#endif
