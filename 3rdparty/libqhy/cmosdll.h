// The following ifdef block is the standard way of creating macros which make exporting
// from a DLL simpler. All files within this DLL are compiled with the CMOSDLL_EXPORTS
// symbol defined on the command line. this symbol should not be defined on any project
// that uses this DLL. This way any other project whose source files include this file see
// CMOSDLL_API functions as being imported from a DLL, whereas this DLL sees symbols
// defined with this macro as being exported.
#ifndef __CMOSDLL_H__
#define __CMOSDLL_H__

#include "qhyccdstruct.h"
#include "config.h"
#if defined (_WIN32)
#include "stdint.h"
#include "CyAPI.h"
#include <process.h>
#else
#include <stdint.h>
#endif


#if defined (_WIN32)
void SetTransferSize(qhyccd_handle *h,int length);
void InitAsyTransfer(CCyUSBDevice *Camera,int framesize);
void StartSingleExposure(CCyUSBDevice *Camera);
void StartLiveExposure(CCyUSBDevice *Camera);
void StopCapturing(CCyUSBDevice *Camera);
DWORD IsExposing(CCyUSBDevice *Camera);
//DWORD ReadAsySingleFrame(PUCHAR _buffer, DWORD size);
DWORD ReadAsySingleFrame(qhyccd_handle *h,PUCHAR _buffer , DWORD size,int *frameflag);

/* live interface */
int32_t InitAsyQCamLive(CCyUSBDevice *Camera,int32_t x,int32_t y,int32_t depth,int32_t framesize);
void StopAsyQCamLive(CCyUSBDevice *Camera);
void BeginAsyQCamLive(CCyUSBDevice *Camera);
uint32_t ReadAsyQCamLiveFrame(CCyUSBDevice *Camera,uint8_t *_buffer,int32_t *frameflag);
uint32_t ReadUSB_SYNC(qhyccd_handle *pDevHandle, uint8_t endpoint, uint32_t length, uint8_t *data, uint32_t timeout);
#else
void SetTransferSize(qhyccd_handle *h,int length);
void InitAsyTransfer(qhyccd_handle *Camera,uint32_t framesize);
void ReleaseAsyTransfer(qhyccd_handle *Camera);
void StartSingleExposure(qhyccd_handle *Camera);
void StartLiveExposure(qhyccd_handle *Camera);
void StopCapturing(qhyccd_handle *Camera);
uint32_t IsExposing(qhyccd_handle *Camera);
uint32_t ReadAsySingleFrame(qhyccd_handle *h,uint8_t *_buffer, uint32_t size,int *frameflag);

/* live interface */
int32_t InitAsyQCamLive(qhyccd_handle *Camera,int32_t x,int32_t y,int32_t depth,int32_t framesize);
void StopAsyQCamLive(qhyccd_handle *Camera);
bool BeginAsyQCamLive(qhyccd_handle *Camera);
uint32_t ReadAsyQCamLiveFrame(qhyccd_handle *Camera,uint8_t *_buffer,int32_t *frameflag);
#endif


#endif

