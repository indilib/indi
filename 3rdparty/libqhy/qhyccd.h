/*
 QHYCCD SDK
 
 Copyright (c) 2014 QHYCCD.
 All Rights Reserved.
 
 This program is free software; you can redistribute it and/or modify it
 under the terms of the GNU General Public License as published by the Free
 Software Foundation; either version 2 of the License, or (at your option)
 any later version.
 
 This program is distributed in the hope that it will be useful, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 more details.
 
 You should have received a copy of the GNU General Public License along with
 this program; if not, write to the Free Software Foundation, Inc., 59
 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 
 The full GNU General Public License is included in this distribution in the
 file called LICENSE.
 */
#ifndef _QHYCCD_H_
#define _QHYCCD_H_

#include "qhycam.h"
#include "qhyccderr.h"
#include "qhyccdcamdef.h"
#include "qhyccdstruct.h"
#include "stdint.h"
#include "qhydevice.h"

#ifdef WIN32
#include "cyapi.h"
typedef CCyUSBDevice qhyccd_handle;
#else // Linux & Mac
typedef struct libusb_device_handle qhyccd_handle;
uint32_t DeviceIsQHYCCD(uint32_t index, qhyccd_device *pDevice);
EXPORTC void STDCALL MutexInit();
EXPORTC void STDCALL MutexDestroy();
EXPORTC void STDCALL MutexLock();
EXPORTC void STDCALL MutexUnlock();
EXPORTC int STDCALL MutexTrylock();
#endif // WIN32

class QHYBASE;
class QhyDevice;

uint32_t DeviceIsQHYCCD(uint32_t index, uint32_t vid, uint32_t pid);
uint32_t QHYCCDSeriesMatch(uint32_t index, qhyccd_handle *pHandle);
uint32_t GetIdFromCam(qhyccd_handle *pHandle, char *id);

EXPORTC QhyDevice* STDCALL GetCyDevBasedOnInstance(QHYBASE *pCam);
EXPORTC QhyDevice* STDCALL GetCyDevBasedOnHandle(qhyccd_handle *pHandle);
EXPORTC int GetCyDevIdxBasedOnInstance(QHYBASE *pCam);
EXPORTC int GetCyDevIdxBasedOnHandle(qhyccd_handle *pHandle);
EXPORTC uint32_t STDCALL GetReceivedRawDataLen(QHYBASE *pCam);
EXPORTC bool STDCALL SetReceivedRawDataLen(QHYBASE *pCam, uint32_t value);
EXPORTC bool STDCALL CleanUnlockImageQueue(QHYBASE *pCam);
//EXPORTC bool STDCALL SetUnlockImageQueue(QHYBASE *pCam);
EXPORTC uint32_t STDCALL InitQHYCCDClass(uint32_t camtype, uint32_t index);
EXPORTC uint32_t STDCALL InitQHYCCDResource(void);
EXPORTC uint32_t STDCALL ReleaseQHYCCDResource(void);
EXPORTC uint32_t STDCALL ScanQHYCCD(void);
EXPORTC uint32_t STDCALL GetQHYCCDId(int index, char *id);
EXPORTC uint32_t STDCALL GetQHYCCDModel(char *id, char *model);
EXPORTC qhyccd_handle * STDCALL OpenQHYCCD(char *id);
EXPORTC uint32_t STDCALL CloseQHYCCD(qhyccd_handle *handle);
EXPORTC uint32_t STDCALL SetQHYCCDStreamMode(qhyccd_handle *handle,uint8_t mode);
EXPORTC uint32_t STDCALL InitQHYCCD(qhyccd_handle *handle);
EXPORTC uint32_t STDCALL IsQHYCCDControlAvailable(qhyccd_handle *handle,CONTROL_ID controlId);
EXPORTC uint32_t STDCALL SetQHYCCDParam(qhyccd_handle *handle,CONTROL_ID controlId, double value);
EXPORTC double STDCALL GetQHYCCDParam(qhyccd_handle *handle,CONTROL_ID controlId);
EXPORTC uint32_t STDCALL GetQHYCCDParamMinMaxStep(qhyccd_handle *handle,CONTROL_ID controlId,double *min,double *max,double *step);
EXPORTC uint32_t STDCALL SetQHYCCDResolution(qhyccd_handle *handle,uint32_t x,uint32_t y,uint32_t xsize,uint32_t ysize);
EXPORTC uint32_t STDCALL GetQHYCCDMemLength(qhyccd_handle *handle);
EXPORTC uint32_t STDCALL ExpQHYCCDSingleFrame(qhyccd_handle *handle);
EXPORTC uint32_t STDCALL GetQHYCCDSingleFrame(qhyccd_handle *handle,uint32_t *w,uint32_t *h,uint32_t *bpp,uint32_t *channels,uint8_t *imgdata);
EXPORTC uint32_t STDCALL CancelQHYCCDExposing(qhyccd_handle *handle);
EXPORTC uint32_t STDCALL CancelQHYCCDExposingAndReadout(qhyccd_handle *handle);
EXPORTC uint32_t STDCALL BeginQHYCCDLive(qhyccd_handle *handle);
EXPORTC uint32_t STDCALL GetQHYCCDLiveFrame(qhyccd_handle *handle,uint32_t *w,uint32_t *h,uint32_t *bpp,uint32_t *channels,uint8_t *imgdata);
EXPORTC uint32_t STDCALL StopQHYCCDLive(qhyccd_handle *handle);
EXPORTC uint32_t STDCALL SetQHYCCDBinMode(qhyccd_handle *handle,uint32_t wbin,uint32_t hbin);
EXPORTC uint32_t STDCALL SetQHYCCDBitsMode(qhyccd_handle *handle,uint32_t bits);
EXPORTC uint32_t STDCALL ControlQHYCCDTemp(qhyccd_handle *handle,double targettemp);
EXPORTC uint32_t STDCALL ControlQHYCCDGuide(qhyccd_handle *handle,uint32_t direction,uint16_t duration);
EXPORTC uint32_t STDCALL SendOrder2QHYCCDCFW(qhyccd_handle *handle,char *order,uint32_t length);
EXPORTC	uint32_t STDCALL GetQHYCCDCFWStatus(qhyccd_handle *handle,char *status);
EXPORTC	uint32_t STDCALL IsQHYCCDCFWPlugged(qhyccd_handle *handle);
EXPORTC uint32_t STDCALL SetQHYCCDTrigerMode(qhyccd_handle *handle,uint32_t trigerMode);
EXPORTC void STDCALL Bits16ToBits8(qhyccd_handle *h,uint8_t *InputData16,uint8_t *OutputData8,uint32_t imageX,uint32_t imageY,uint16_t B,uint16_t W);
EXPORTC void  STDCALL HistInfo192x130(qhyccd_handle *h,uint32_t x,uint32_t y,uint8_t *InBuf,uint8_t *OutBuf);
EXPORTC uint32_t STDCALL OSXInitQHYCCDFirmware(char *path);
EXPORTC uint32_t STDCALL GetQHYCCDChipInfo(qhyccd_handle *h,double *chipw,double *chiph,uint32_t *imagew,uint32_t *imageh,double *pixelw,double *pixelh,uint32_t *bpp);
EXPORTC uint32_t STDCALL GetQHYCCDEffectiveArea(qhyccd_handle *h,uint32_t *startX, uint32_t *startY, uint32_t *sizeX, uint32_t *sizeY);
EXPORTC uint32_t STDCALL GetQHYCCDOverScanArea(qhyccd_handle *h,uint32_t *startX, uint32_t *startY, uint32_t *sizeX, uint32_t *sizeY);
EXPORTC uint32_t STDCALL SetQHYCCDFocusSetting(qhyccd_handle *h,uint32_t focusCenterX, uint32_t focusCenterY);
EXPORTC uint32_t STDCALL GetQHYCCDExposureRemaining(qhyccd_handle *h);
EXPORTC uint32_t STDCALL GetQHYCCDFWVersion(qhyccd_handle *h,uint8_t *buf);
EXPORTC uint32_t STDCALL SetQHYCCDInterCamSerialParam(qhyccd_handle *h,uint32_t opt);
EXPORTC uint32_t STDCALL QHYCCDInterCamSerialTX(qhyccd_handle *h,char *buf,uint32_t length);
EXPORTC uint32_t STDCALL QHYCCDInterCamSerialRX(qhyccd_handle *h,char *buf);
EXPORTC uint32_t STDCALL QHYCCDInterCamOledOnOff(qhyccd_handle *handle,uint8_t onoff);
EXPORTC uint32_t STDCALL SetQHYCCDInterCamOledBrightness(qhyccd_handle *handle,uint8_t brightness);
EXPORTC uint32_t STDCALL SendFourLine2QHYCCDInterCamOled(qhyccd_handle *handle,char *messagetemp,char *messageinfo,char *messagetime,char *messagemode);
EXPORTC uint32_t STDCALL SendTwoLine2QHYCCDInterCamOled(qhyccd_handle *handle,char *messageTop,char *messageBottom);
EXPORTC uint32_t STDCALL SendOneLine2QHYCCDInterCamOled(qhyccd_handle *handle,char *messageTop);
EXPORTC uint32_t STDCALL GetQHYCCDCameraStatus(qhyccd_handle *h,uint8_t *buf);
EXPORTC uint32_t STDCALL GetQHYCCDShutterStatus(qhyccd_handle *handle);
EXPORTC uint32_t STDCALL ControlQHYCCDShutter(qhyccd_handle *handle,uint8_t status);
EXPORTC uint32_t STDCALL GetQHYCCDHumidity(qhyccd_handle *handle,double *hd);
EXPORTC uint32_t STDCALL QHYCCDI2CTwoWrite(qhyccd_handle *handle,uint16_t addr,uint16_t value);
EXPORTC uint32_t STDCALL QHYCCDI2CTwoRead(qhyccd_handle *handle,uint16_t addr);
EXPORTC double STDCALL GetQHYCCDReadingProgress(qhyccd_handle *handle);
EXPORTC void STDCALL SetQHYCCDLogLevel(uint8_t logLevel);
EXPORTC uint32_t STDCALL TestQHYCCDPIDParas(qhyccd_handle *h, double p, double i, double d);
EXPORTC uint32_t STDCALL SetQHYCCDTrigerFunction(qhyccd_handle *h,bool value);
EXPORTC uint32_t STDCALL DownloadFX3FirmWare(uint16_t vid,uint16_t pid,char *imgpath);
EXPORTC uint32_t STDCALL GetQHYCCDType(qhyccd_handle *h);
EXPORTC uint32_t STDCALL SetQHYCCDDebayerOnOff(qhyccd_handle *h,bool onoff);
EXPORTC uint32_t STDCALL SetQHYCCDFineTone(qhyccd_handle *h,uint8_t setshporshd,uint8_t shdloc,uint8_t shploc,uint8_t shwidth);
EXPORTC uint32_t STDCALL SetQHYCCDGPSVCOXFreq(qhyccd_handle *handle,uint16_t i);
EXPORTC uint32_t STDCALL SetQHYCCDGPSLedCalMode(qhyccd_handle *handle,uint8_t i);
EXPORTC void STDCALL SetQHYCCDGPSLedCal(qhyccd_handle *handle,uint32_t pos,uint8_t width);
EXPORTC void STDCALL SetQHYCCDGPSPOSA(qhyccd_handle *handle,uint8_t is_slave,uint32_t pos,uint8_t width);
EXPORTC void STDCALL SetQHYCCDGPSPOSB(qhyccd_handle *handle,uint8_t is_slave,uint32_t pos,uint8_t width);
EXPORTC uint32_t STDCALL SetQHYCCDGPSMasterSlave(qhyccd_handle *handle,uint8_t i);
EXPORTC void STDCALL SetQHYCCDGPSSlaveModeParameter(qhyccd_handle *handle,uint32_t target_sec,uint32_t target_us,uint32_t deltaT_sec,uint32_t deltaT_us,uint32_t expTime);
EXPORTC uint32_t STDCALL QHYCCDVendRequestWrite(qhyccd_handle *h,uint8_t req,uint16_t value,uint16_t index1,uint32_t length,uint8_t *data);
EXPORTC uint32_t STDCALL QHYCCDReadUSB_SYNC(qhyccd_handle *pDevHandle, uint8_t endpoint, uint32_t length, uint8_t *data, uint32_t timeout);
EXPORTC uint32_t STDCALL QHYCCDLibusbBulkTransfer(qhyccd_handle *pDevHandle, uint8_t endpoint, uint8_t *data, uint32_t length, int32_t *transferred, uint32_t timeout);
EXPORTC uint32_t STDCALL GetQHYCCDSDKVersion(uint32_t *year,uint32_t *month,uint32_t *day,uint32_t *subday);
EXPORTC void STDCALL print_cydev(const char *pTitle);
EXPORTC const char* STDCALL GetTimeStamp();
EXPORTC void STDCALL GetQHYCCDControlIdString(CONTROL_ID controlId, char *pStr);

#endif // __QHYCCD_H__
