// The following ifdef block is the standard way of creating macros which make exporting 
// from a DLL simpler. All files within this DLL are compiled with the CMOSDLL_EXPORTS
// symbol defined on the command line. this symbol should not be defined on any project
// that uses this DLL. This way any other project whose source files include this file see 
// CMOSDLL_API functions as being imported from a DLL, whereas this DLL sees symbols
// defined with this macro as being exported.


#include "qhyccdstruct.h"

#ifdef WIN32

#include "stdint.h"
#include "CyAPI.h"
#include <process.h>

// live interface 
int32_t InitAsyQCamLive(CCyUSBDevice *Camera, uint32_t x, uint32_t y, uint32_t depth, uint32_t frameSize);
void StopAsyQCamLive(CCyUSBDevice *Camera);
void BeginAsyQCamLive(CCyUSBDevice *Camera);
uint32_t ReadAsyQCamLiveFrame(CCyUSBDevice *Camera, uint8_t *pBuffer, int32_t *pFrameFlag);

// other functions
void SetTransferSize(int length);
void InitAsyTransfer(CCyUSBDevice *Camera, int framesize);
void StartSingleExposure(CCyUSBDevice *Camera);
void StartLiveExposure(CCyUSBDevice *Camera);
void StopCapturing(CCyUSBDevice *Camera);
DWORD IsExposing();
DWORD ReadAsySingleFrame(PUCHAR pBuffer , DWORD size, int *pFrameFlag);

#else // Linux & Mac

#include <stdint.h>

// live interface 
int32_t InitAsyQCamLive(qhyccd_handle *pDevHandle, uint32_t x, uint32_t y, uint32_t depth, uint32_t frameSize);
bool BeginAsyQCamLive(qhyccd_handle *pDevHandle);
void StopAsyQCamLive(qhyccd_handle *pDevHandle);
uint32_t ReadAsyQCamLiveFrame(qhyccd_handle *pDevHandle, uint8_t *pBuffer, int32_t *pFrameFlag);
uint32_t ReadAsyQCamLiveFrame(qhyccd_handle *pDevHandle, uint8_t *pBuffer, int32_t *pFrameFlag, UnlockImageQueue *pImageQueue);
uint32_t ClearEndpoint(qhyccd_handle *pDevHandle);
uint32_t ProcessAllPendingTransfers(qhyccd_handle *pDevHandle);
uint32_t CancelAllPendingTransfers(qhyccd_handle *pDevHandle);
void asyImageDataCallBack(struct libusb_transfer *transfer);

// other functions
void SetTransferSize(int length);
void InitAsyTransfer(qhyccd_handle *pDevHandle, uint32_t frameSize);
void ReleaseAsyTransfer(qhyccd_handle *pDevHandle);
void StartSingleExposure(qhyccd_handle *pDevHandle);
void StartLiveExposure(qhyccd_handle *pDevHandle);
void StopCapturing(qhyccd_handle *pDevHandle);
uint32_t IsExposing();
uint32_t ReadAsySingleFrame(uint8_t *pBuffer, uint32_t size, int *pFrameFlag);

void SetThreadExitFlag(int idx, bool val);
bool IsThreadExitFlag(int idx);

void IncrementEventCount(int idx);
void DecrementEventCount(int idx);
void ClearEventCount(int idx);
int GetEventCount(int idx);

void SetFirstExposureFlag(int idx, bool value);
bool IsFirstExpousureFlag(int idx);

#endif // WIN32

